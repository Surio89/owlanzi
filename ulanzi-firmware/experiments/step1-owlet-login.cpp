/*
 * ============================================================================
 *  Owlet -> ESP32, Schritt 1: nur Anmeldung und Datenabruf
 * ============================================================================
 *
 *  Zweck: Beweisen, dass ein ESP32 die Owlet-Cloud allein erreichen kann -
 *  ohne Home Assistant, ohne Bruecke, ohne Display. Wenn das hier laeuft,
 *  ist der Rest des Projekts Fleissarbeit. Wenn es nicht laeuft, hast du
 *  einen Abend verloren statt eines Projekts.
 *
 *  KEIN Display, KEINE Matrix, KEIN Ton. Laeuft auf jedem beliebigen ESP32.
 *  Nimm ausdruecklich NICHT die Uhr, die nachts am Kinderbett steht.
 *
 *  Die Anmeldung ist eine dreistufige Kette (aus quelloffenen Python-
 *  Implementierungen uebernommen, siehe Quellen unten):
 *
 *    1. Firebase        E-Mail + Passwort            -> idToken (JWT, ~1 kB)
 *    2. Owlet-SSO       idToken                      -> mini_token
 *    3. Ayla            mini_token + App-Secret      -> access_token
 *    4. Ayla            access_token                 -> Geraeteliste (dsn)
 *    5. Ayla            APP_ACTIVE=1 setzen          (weckt die Cloud auf)
 *    6. Ayla            properties.json              -> REAL_TIME_VITALS
 *
 *  Schritt 5 ist nicht optional. Ohne APP_ACTIVE liefert die Cloud
 *  eingefrorene Werte - sie aktualisiert nur, solange eine App zuhoert.
 *
 *  WAS DIESER SKETCH MISST, und darum geht es eigentlich:
 *  Er gibt vor und nach jedem TLS-Aufruf den freien Heap und den groessten
 *  zusammenhaengenden Block aus. Drei verkettete HTTPS-Verbindungen sind auf
 *  einem ESP32 ohne PSRAM die eigentliche Frage. Behalte die Zahlen im Auge:
 *  faellt "groesster Block" unter ~45 kB, wird der naechste Handshake knapp.
 *
 *  Quellen der Endpunkte und Feldnamen:
 *    https://github.com/ryanbdclark/pyowletapi  (src/pyowletapi/const.py)
 *    https://github.com/mbevand/owlet_monitor
 *
 * ============================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <Preferences.h>
#include <time.h>

// ---------------------------------------------------------------------------
//  KONFIGURATION - hier eintragen
// ---------------------------------------------------------------------------

static const char *WIFI_SSID = "";
static const char *WIFI_PASS = "";

static const char *DEFAULT_OWLET_EMAIL    = "";
static const char *DEFAULT_OWLET_PASSWORD = "#";

// Abstand zwischen zwei Abrufen. Die Socke misst etwa alle 5 s;
// zum Messen reichen 10 s voellig und schonen die Cloud.
static const uint32_t POLL_INTERVAL_MS = 10000;

/*  TLS-Zertifikatspruefung.
 *
 *  1 = richtig (Mozilla-Wurzelzertifikate aus dem ESP32-Bundle).
 *  0 = ohne Pruefung. NUR fuer die allererste Inbetriebnahme, wenn du
 *      wissen willst ob ueberhaupt etwas ankommt.
 *
 *  Hier geht dein Owlet-Passwort ueber die Leitung. Auf 0 laesst sich die
 *  Verbindung von jedem im selben Netz mitlesen und umbiegen. Stell es
 *  zurueck auf 1, sobald die Kette einmal durchgelaufen ist, und lass es
 *  dort - erst recht wenn das Geraet spaeter bei anderen Leuten steht.
 */
#define TLS_VERIFY 1

// ---------------------------------------------------------------------------
//  Regionsdaten
// ---------------------------------------------------------------------------

static const char *R_API_KEY;
static const char *R_APP_ID;
static const char *R_APP_SEC;
static const char *R_MINI;
static const char *R_SIGNIN;
static const char *R_REFRESH;
static const char *R_BASE;

static const char *EU_API_KEY  = "AIzaSyDm6EhV70wudN3iOSq3vTjtsdGjdFLuuM";
static const char *EU_APP_ID   = "OwletCare-Android-EU-fw-id";
static const char *EU_APP_SEC  = "OwletCare-Android-EU-JKupMPBoj_Npce_9a95Pc8Qo0Mw";
static const char *EU_MINI     = "https://ayla-sso.eu.owletdata.com/mini/";
static const char *EU_SIGNIN   = "https://user-field-eu-1a2039d9.aylanetworks.com/api/v1/token_sign_in";
static const char *EU_REFRESH  = "https://user-field-eu-1a2039d9.aylanetworks.com/users/refresh_token.json";
static const char *EU_BASE     = "https://ads-field-eu-1a2039d9.aylanetworks.com/apiv1";

static const char *WORLD_API_KEY  = "AIzaSyCsDZ8kWxQuLJAMVnmEhEkayH1TSxKXfGA";
static const char *WORLD_APP_ID   = "sso-prod-3g-id";
static const char *WORLD_APP_SEC  = "sso-prod-UEjtnPCtFfjdwIwxqnC0OipxRFU";
static const char *WORLD_MINI     = "https://ayla-sso.owletdata.com/mini/";
static const char *WORLD_SIGNIN   = "https://user-field-1a2039d9.aylanetworks.com/api/v1/token_sign_in";
static const char *WORLD_REFRESH  = "https://user-field-1a2039d9.aylanetworks.com/users/refresh_token.json";
static const char *WORLD_BASE     = "https://ads-field-1a2039d9.aylanetworks.com/apiv1";

// Die Owlet-Android-App gibt sich bei Firebase so zu erkennen. Ohne diese
// beiden Header lehnt Google die Anfrage ab.
static const char *ANDROID_PKG  = "com.owletcare.owletcare";
static const char *ANDROID_CERT = "2A3BC26DB0B8B0792DBE28E6FFDC2598F9B12B74";

// Wurzelzertifikat-Bundle, das der ESP32-Arduino-Core mitbringt.
#if TLS_VERIFY
extern const uint8_t rootca_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t rootca_crt_bundle_end[]   asm("_binary_x509_crt_bundle_end");
#endif

// ---------------------------------------------------------------------------
//  Zustand
// ---------------------------------------------------------------------------

static String gAccessToken;
static String gRefreshToken;
static String gDsn;
static String gOwletEmail;
static String gOwletPassword;
static bool gEurope = false;
static uint32_t gTokenExpiresAt = 0;   // Sekunden seit Boot
static uint32_t gFailCount = 0;
static Preferences gPreferences;
static WebServer gWebServer(80);

static void selectRegion() {
  if (gEurope) {
    R_API_KEY = EU_API_KEY; R_APP_ID = EU_APP_ID; R_APP_SEC = EU_APP_SEC;
    R_MINI = EU_MINI; R_SIGNIN = EU_SIGNIN; R_REFRESH = EU_REFRESH; R_BASE = EU_BASE;
  } else {
    R_API_KEY = WORLD_API_KEY; R_APP_ID = WORLD_APP_ID; R_APP_SEC = WORLD_APP_SEC;
    R_MINI = WORLD_MINI; R_SIGNIN = WORLD_SIGNIN; R_REFRESH = WORLD_REFRESH; R_BASE = WORLD_BASE;
  }
}

static void loadSettings() {
  gPreferences.begin("owlet", false);
  gOwletEmail = gPreferences.getString("email", DEFAULT_OWLET_EMAIL);
  gOwletPassword = gPreferences.getString("password", DEFAULT_OWLET_PASSWORD);
  gEurope = gPreferences.getBool("europe", false);
  selectRegion();
}

static String htmlEscape(const String &value) {
  String escaped = value;
  escaped.replace("&", "&amp;"); escaped.replace("<", "&lt;");
  escaped.replace(">", "&gt;"); escaped.replace("\"", "&quot;");
  return escaped;
}

static void handleConfigPage() {
  String html = R"HTML(<!doctype html><html lang="de"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Owlet ESP32</title><style>body{font:16px system-ui;max-width:520px;margin:40px auto;padding:0 20px;color:#17202a}label{display:block;margin:18px 0 6px}input,select,button{box-sizing:border-box;width:100%;padding:12px;font:inherit}button{margin-top:24px;background:#1769aa;color:white;border:0;cursor:pointer}small{color:#59636e}</style>
<h1>Owlet ESP32</h1><p>Cloud-Zugang konfigurieren</p><form method="post" action="/save">
<label for="email">E-Mail</label><input id="email" name="email" type="email" required value=")HTML";
  html += htmlEscape(gOwletEmail);
  html += R"HTML("><label for="password">Passwort</label><input id="password" name="password" type="password" placeholder="unverändert lassen"><small>Leer lassen, um das gespeicherte Passwort zu behalten.</small>
<label for="region">Region</label><select id="region" name="region"><option value="world" )HTML";
  if (!gEurope) html += "selected";
  html += R"HTML(>Non-EU / World</option><option value="eu" )HTML";
  if (gEurope) html += "selected";
  html += R"HTML(>EU / Europe</option></select><button type="submit">Speichern und neu anmelden</button></form></html>)HTML";
  gWebServer.send(200, "text/html; charset=utf-8", html);
}

static void handleConfigSave() {
  String email = gWebServer.arg("email");
  String password = gWebServer.arg("password");
  if (email.isEmpty()) { gWebServer.send(400, "text/plain", "E-Mail fehlt"); return; }
  if (!password.isEmpty()) {
    gOwletPassword = password;
    gPreferences.putString("password", gOwletPassword);
  }
  gOwletEmail = email;
  gEurope = gWebServer.arg("region") == "eu";
  gPreferences.putString("email", gOwletEmail);
  gPreferences.putBool("europe", gEurope);
  selectRegion();
  gAccessToken = ""; gRefreshToken = ""; gDsn = ""; gFailCount = 0;
  gWebServer.sendHeader("Location", "/");
  gWebServer.send(303, "text/plain", "Gespeichert");
}

static void startWebInterface() {
  gWebServer.on("/", HTTP_GET, handleConfigPage);
  gWebServer.on("/save", HTTP_POST, handleConfigSave);
  gWebServer.begin();
  Serial.printf("Webinterface: http://%s/\n", WiFi.localIP().toString().c_str());
}

static void serviceWebFor(uint32_t durationMs) {
  uint32_t started = millis();
  while (millis() - started < durationMs) {
    gWebServer.handleClient();
    delay(2);
  }
}

struct Vitals {
  bool  valid          = false;
  float oxygen         = 0;   // ox   - Sauerstoffsaettigung %
  float heartRate      = 0;   // hr   - Puls bpm
  float battery        = 0;   // bat  - Akku %
  float batteryMinutes = 0;   // btt  - Restlaufzeit min
  float signal         = 0;   // rsi  - BLE-Feldstaerke
  float oxygen10av     = 0;   // oxta - 10-Minuten-Mittel
  bool  baseStationOn  = false; // bso
  int   sockConnection = 0;   // sc
  int   skinTemp       = 0;   // st
  int   sleepState     = 0;   // ss
  int   movement       = 0;   // mv
  int   alertPaused    = 0;   // aps
  int   charging       = 0;   // chg
  int   alertsMask     = 0;   // alrt
  int   readingsFlag   = 0;   // srf
  int   brickStatus    = 0;   // sb
  // Eigenstaendige Properties neben dem Vitals-Blob
  bool  lowOxAlert = false, highOxAlert = false;
  bool  lowHrAlert = false, highHrAlert = false;
  bool  lostPowerAlert = false, sockDisconnected = false;
  bool  sockOff = false, lowBattAlert = false;
  bool  critBattAlert = false, critOxAlert = false;
};

// ---------------------------------------------------------------------------
//  Hilfsmittel
// ---------------------------------------------------------------------------

static void heap(const char *wo) {
  Serial.printf("   [heap] %-22s frei %6u  groesster Block %6u\n",
                wo, (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
}

// Baut einen TLS-Client. Bewusst als lokale Variable im Aufrufer angelegt und
// nach jedem Aufruf wieder freigegeben - drei gleichzeitig offene TLS-Kontexte
// passen auf einem ESP32 ohne PSRAM nicht nebeneinander.
static void prepareTls(WiFiClientSecure &c) {
#if TLS_VERIFY
  // Der Core 3.x will zusaetzlich die Groesse, 2.x nicht. Beide abgedeckt.
  #if ESP_ARDUINO_VERSION_MAJOR >= 3
    c.setCACertBundle(rootca_crt_bundle_start,
                      (size_t)(rootca_crt_bundle_end - rootca_crt_bundle_start));
  #else
    c.setCACertBundle(rootca_crt_bundle_start);
  #endif
#else
  c.setInsecure();
#endif
  c.setTimeout(15000);
}

/*
 * Ein HTTP-Aufruf, dessen Antwort direkt aus dem Stream heraus gefiltert
 * geparst wird. Das ist der entscheidende Kniff: properties.json ist je nach
 * Geraet mehrere zehn Kilobyte gross und wuerde am Stueck den Heap sprengen.
 * Der Filter wirft alles weg, was uns nicht interessiert, waehrend die Daten
 * noch hereinlaufen.
 *
 * method: "GET" oder "POST". body darf leer sein.
 */
static bool httpJson(const char *method, const String &url,
                     const String &body,
                     const char *hdrName1, const String &hdrVal1,
                     const char *hdrName2, const String &hdrVal2,
                     JsonDocument &out, JsonDocument *filter,
                     const char *label) {
  WiFiClientSecure client;
  prepareTls(client);

  HTTPClient http;
  http.setTimeout(15000);
  http.setReuse(false);

  if (!http.begin(client, url)) {
    Serial.printf("   !! %s: begin() fehlgeschlagen\n", label);
    return false;
  }

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept", "application/json");
  if (hdrName1) http.addHeader(hdrName1, hdrVal1);
  if (hdrName2) http.addHeader(hdrName2, hdrVal2);

  int code = (strcmp(method, "POST") == 0)
               ? http.POST((uint8_t *)body.c_str(), body.length())
               : http.GET();

  if (code <= 0) {
    Serial.printf("   !! %s: Verbindungsfehler %s\n", label,
                  http.errorToString(code).c_str());
    http.end();
    return false;
  }
  if (code < 200 || code >= 300) {
    Serial.printf("   !! %s: HTTP %d\n", label, code);
    // Bei Fehlern ist die Antwort klein genug, um sie zur Diagnose zu zeigen.
    String err = http.getString();
    if (err.length() > 300) err = err.substring(0, 300) + "...";
    Serial.printf("      %s\n", err.c_str());
    http.end();
    return false;
  }

  DeserializationError e = filter
    ? deserializeJson(out, http.getStream(),
                      DeserializationOption::Filter(*filter))
    : deserializeJson(out, http.getStream());

  http.end();

  if (e) {
    Serial.printf("   !! %s: JSON-Fehler %s\n", label, e.c_str());
    return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
//  Schritt 1-3: Anmeldung
// ---------------------------------------------------------------------------

static bool owletLogin() {
  Serial.println("\n-- Anmeldung --");
  heap("vor Firebase");

  // --- 1. Firebase: E-Mail + Passwort -> idToken -------------------------
  String idToken;
  {
    String url = String("https://www.googleapis.com/identitytoolkit/v3/"
                        "relyingparty/verifyPassword?key=") + R_API_KEY;

    JsonDocument req;
    req["email"] = gOwletEmail;
    req["password"] = gOwletPassword;
    req["returnSecureToken"] = true;
    String body;
    serializeJson(req, body);

    // Der JWT ist rund 1 kB. Wir behalten nur ihn, nicht den Rest der Antwort.
    JsonDocument filter;
    filter["idToken"] = true;

    JsonDocument res;
    if (!httpJson("POST", url, body,
                  "X-Android-Package", ANDROID_PKG,
                  "X-Android-Cert", ANDROID_CERT,
                  res, &filter, "Firebase")) return false;

    idToken = res["idToken"].as<String>();
    if (idToken.isEmpty()) {
      Serial.println("   !! Firebase: kein idToken in der Antwort");
      return false;
    }
    Serial.printf("   1/3 Firebase ok, idToken %u Zeichen\n", idToken.length());
  }
  heap("nach Firebase");

  // --- 2. Owlet-SSO: idToken -> mini_token --------------------------------
  String miniToken;
  {
    // Achtung: hier steht der nackte JWT im Authorization-Header,
    // ohne "Bearer" davor. Mit Bearer antwortet der Dienst mit 401.
    JsonDocument filter;
    filter["mini_token"] = true;

    JsonDocument res;
    if (!httpJson("GET", R_MINI, "",
                  "Authorization", idToken,
                  nullptr, "",
                  res, &filter, "Owlet-SSO")) return false;

    miniToken = res["mini_token"].as<String>();
    if (miniToken.isEmpty()) {
      Serial.println("   !! Owlet-SSO: kein mini_token in der Antwort");
      return false;
    }
    Serial.printf("   2/3 SSO ok, mini_token %u Zeichen\n", miniToken.length());
  }
  idToken = String();   // wird nicht mehr gebraucht, Speicher freigeben
  heap("nach SSO");

  // --- 3. Ayla: mini_token -> access_token --------------------------------
  {
    JsonDocument req;
    req["app_id"] = R_APP_ID;
    req["app_secret"] = R_APP_SEC;
    req["provider"] = "owl_id";
    req["token"] = miniToken;
    String body;
    serializeJson(req, body);

    JsonDocument filter;
    filter["access_token"] = true;
    filter["refresh_token"] = true;
    filter["expires_in"] = true;

    JsonDocument res;
    if (!httpJson("POST", R_SIGNIN, body, nullptr, "", nullptr, "",
                  res, &filter, "Ayla-Anmeldung")) return false;

    gAccessToken  = res["access_token"].as<String>();
    gRefreshToken = res["refresh_token"].as<String>();
    uint32_t ttl  = res["expires_in"] | 3600;

    if (gAccessToken.isEmpty()) {
      Serial.println("   !! Ayla: kein access_token in der Antwort");
      return false;
    }
    // 60 s Sicherheitsabstand, damit uns der Token nicht mitten im Abruf
    // unter den Fuessen wegstirbt.
    gTokenExpiresAt = (millis() / 1000) + (ttl > 60 ? ttl - 60 : ttl);
    Serial.printf("   3/3 Ayla ok, Token gilt %u s\n", ttl);
  }
  heap("nach Anmeldung");
  return true;
}

// Verlaengert den Token, ohne die ganze Kette neu zu durchlaufen.
static bool owletRefresh() {
  Serial.println("\n-- Token verlaengern --");
  JsonDocument req;
  req["user"]["refresh_token"] = gRefreshToken;
  String body;
  serializeJson(req, body);

  JsonDocument filter;
  filter["access_token"] = true;
  filter["refresh_token"] = true;
  filter["expires_in"] = true;

  JsonDocument res;
  if (!httpJson("POST", R_REFRESH, body, nullptr, "", nullptr, "",
                res, &filter, "Token-Refresh")) return false;

  String at = res["access_token"].as<String>();
  if (at.isEmpty()) return false;
  gAccessToken = at;
  if (!res["refresh_token"].isNull()) gRefreshToken = res["refresh_token"].as<String>();
  uint32_t ttl = res["expires_in"] | 3600;
  gTokenExpiresAt = (millis() / 1000) + (ttl > 60 ? ttl - 60 : ttl);
  Serial.printf("   ok, gilt wieder %u s\n", ttl);
  return true;
}

// ---------------------------------------------------------------------------
//  Schritt 4: Geraeteseriennummer holen
// ---------------------------------------------------------------------------

static bool owletFindDevice() {
  Serial.println("\n-- Geraet suchen --");
  String url = String(R_BASE) + "/devices.json";

  // Antwort ist ein Array voller Metadaten; uns interessiert nur die dsn.
  JsonDocument filter;
  filter[0]["device"]["dsn"] = true;
  filter[0]["device"]["product_name"] = true;

  JsonDocument res;
  if (!httpJson("GET", url, "",
                "Authorization", String("auth_token ") + gAccessToken,
                nullptr, "", res, &filter, "Geraeteliste")) return false;

  JsonArray arr = res.as<JsonArray>();
  if (arr.isNull() || arr.size() == 0) {
    Serial.println("   !! keine Geraete im Konto gefunden");
    return false;
  }
  for (JsonObject o : arr) {
    const char *dsn = o["device"]["dsn"];
    const char *nm  = o["device"]["product_name"] | "?";
    Serial.printf("   gefunden: %s  (%s)\n", dsn ? dsn : "?", nm);
    if (dsn && gDsn.isEmpty()) gDsn = dsn;
  }
  if (gDsn.isEmpty()) {
    Serial.println("   !! keine dsn lesbar");
    return false;
  }
  Serial.printf("   verwende: %s\n", gDsn.c_str());
  return true;
}

// ---------------------------------------------------------------------------
//  Schritt 5: APP_ACTIVE - ohne das liefert die Cloud alte Werte
// ---------------------------------------------------------------------------

static bool owletKeepAwake() {
  String url = String(R_BASE) + "/dsns/" + gDsn +
               "/properties/APP_ACTIVE/datapoints.json";
  String body = "{\"datapoint\":{\"metadata\":{},\"value\":1}}";

  JsonDocument filter;   // Antwort interessiert uns nicht
  filter.set(false);

  JsonDocument res;
  return httpJson("POST", url, body,
                  "Authorization", String("auth_token ") + gAccessToken,
                  nullptr, "", res, &filter, "APP_ACTIVE");
}

// ---------------------------------------------------------------------------
//  Schritt 6: Vitalwerte
// ---------------------------------------------------------------------------

static bool boolProp(JsonVariant v) {
  if (v.is<bool>()) return v.as<bool>();
  return v.as<int>() != 0;
}

static bool owletFetchVitals(Vitals &out) {
  String url = String(R_BASE) + "/dsns/" + gDsn + "/properties.json";

  // Das ist die speicherkritische Stelle. properties.json listet jede
  // Eigenschaft des Geraets samt Zeitstempeln, Anzeigenamen und Typen -
  // je nach Socke mehrere zehn Kilobyte. Der Filter behaelt pro Eintrag
  // nur name und value, alles andere faellt schon beim Einlesen weg.
  JsonDocument filter;
  filter[0]["property"]["name"]  = true;
  filter[0]["property"]["value"] = true;

  JsonDocument res;
  if (!httpJson("GET", url, "",
                "Authorization", String("auth_token ") + gAccessToken,
                nullptr, "", res, &filter, "properties.json")) return false;

  JsonArray arr = res.as<JsonArray>();
  if (arr.isNull()) return false;

  String vitalsRaw;
  for (JsonObject o : arr) {
    const char *name = o["property"]["name"];
    if (!name) continue;
    JsonVariant val = o["property"]["value"];

    if      (!strcmp(name, "REAL_TIME_VITALS"))  vitalsRaw = val.as<String>();
    else if (!strcmp(name, "LOW_OX_ALRT"))       out.lowOxAlert       = boolProp(val);
    else if (!strcmp(name, "HIGH_OX_ALRT"))      out.highOxAlert      = boolProp(val);
    else if (!strcmp(name, "LOW_HR_ALRT"))       out.lowHrAlert       = boolProp(val);
    else if (!strcmp(name, "HIGH_HR_ALRT"))      out.highHrAlert      = boolProp(val);
    else if (!strcmp(name, "LOST_POWER_ALRT"))   out.lostPowerAlert   = boolProp(val);
    else if (!strcmp(name, "SOCK_DISCON_ALRT"))  out.sockDisconnected = boolProp(val);
    else if (!strcmp(name, "SOCK_OFF"))          out.sockOff          = boolProp(val);
    else if (!strcmp(name, "LOW_BATT_ALRT"))     out.lowBattAlert     = boolProp(val);
    else if (!strcmp(name, "CRIT_BATT_ALRT"))    out.critBattAlert    = boolProp(val);
    else if (!strcmp(name, "CRIT_OX_ALRT"))      out.critOxAlert      = boolProp(val);
  }

  if (vitalsRaw.isEmpty()) {
    Serial.println("   !! REAL_TIME_VITALS nicht gefunden.");
    Serial.println("      Aeltere Socken (v2) liefern stattdessen einzelne");
    Serial.println("      Properties wie HEART_RATE und OXYGEN_LEVEL.");
    return false;
  }

  // Der Wert von REAL_TIME_VITALS ist selbst wieder JSON - als Zeichenkette
  // verpackt. Also ein zweites Mal parsen. Klein genug, ohne Filter.
  JsonDocument v;
  DeserializationError e = deserializeJson(v, vitalsRaw);
  if (e) {
    Serial.printf("   !! Vitals-Blob nicht lesbar: %s\n", e.c_str());
    return false;
  }

  out.oxygen         = v["ox"]   | 0.0f;
  out.heartRate      = v["hr"]   | 0.0f;
  out.battery        = v["bat"]  | 0.0f;
  out.batteryMinutes = v["btt"]  | 0.0f;
  out.signal         = v["rsi"]  | 0.0f;
  out.oxygen10av     = v["oxta"] | 0.0f;
  // bso kommt je nach Firmware als true/false ODER als 0/1 - deshalb nicht
  // mit "| false" abfragen, das liefert bei einer Zahl still den Vorgabewert.
  out.baseStationOn  = boolProp(v["bso"]);
  out.sockConnection = v["sc"]   | 0;
  out.skinTemp       = v["st"]   | 0;
  out.sleepState     = v["ss"]   | 0;
  out.movement       = v["mv"]   | 0;
  out.alertPaused    = v["aps"]  | 0;
  out.charging       = v["chg"]  | 0;
  out.alertsMask     = v["alrt"] | 0;
  out.readingsFlag   = v["srf"]  | 0;
  out.brickStatus    = v["sb"]   | 0;
  out.valid = true;
  return true;
}

// ---------------------------------------------------------------------------
//  Ausgabe
// ---------------------------------------------------------------------------

static void printVitals(const Vitals &x) {
  Serial.println("\n========================================================");
  Serial.printf("  Puls              %6.0f bpm\n", x.heartRate);
  Serial.printf("  Sauerstoff        %6.0f %%   (10-min-Mittel %.0f)\n",
                x.oxygen, x.oxygen10av);
  Serial.printf("  Akku              %6.0f %%   (noch %.0f min)\n",
                x.battery, x.batteryMinutes);
  Serial.printf("  Basisstation an   %6s\n", x.baseStationOn ? "ja" : "nein");
  Serial.println("  --------------------------------------------------");
  Serial.println("  Rohwerte, deren Bedeutung du selbst zuordnen musst:");
  Serial.printf("    ss  Schlafzustand   %d\n", x.sleepState);
  Serial.printf("    chg Laden           %d\n", x.charging);
  Serial.printf("    sc  Sockenkontakt   %d\n", x.sockConnection);
  Serial.printf("    mv  Bewegung        %d\n", x.movement);
  Serial.printf("    st  Hauttemperatur  %d\n", x.skinTemp);
  Serial.printf("    srf Messwert-Flag   %d\n", x.readingsFlag);
  Serial.printf("    aps Alarm pausiert  %d\n", x.alertPaused);
  Serial.printf("    alrt Alarmmaske     %d\n", x.alertsMask);
  Serial.printf("    sb  Brick-Status    %d\n", x.brickStatus);
  Serial.printf("    rsi BLE-Feldstaerke %.0f\n", x.signal);
  Serial.println("  --------------------------------------------------");
  Serial.print("  Alarme:");
  bool any = false;
  if (x.lowOxAlert)       { Serial.print(" SAUERSTOFF-NIEDRIG");  any = true; }
  if (x.highOxAlert)      { Serial.print(" SAUERSTOFF-HOCH");     any = true; }
  if (x.critOxAlert)      { Serial.print(" SAUERSTOFF-KRITISCH"); any = true; }
  if (x.lowHrAlert)       { Serial.print(" PULS-NIEDRIG");        any = true; }
  if (x.highHrAlert)      { Serial.print(" PULS-HOCH");           any = true; }
  if (x.lostPowerAlert)   { Serial.print(" STROM-WEG");           any = true; }
  if (x.sockDisconnected) { Serial.print(" SOCKE-GETRENNT");      any = true; }
  if (x.sockOff)          { Serial.print(" SOCKE-AB");            any = true; }
  if (x.lowBattAlert)     { Serial.print(" AKKU-NIEDRIG");        any = true; }
  if (x.critBattAlert)    { Serial.print(" AKKU-KRITISCH");       any = true; }
  if (!any) Serial.print(" keine");
  Serial.println();
  Serial.println("========================================================");
}

// ---------------------------------------------------------------------------
//  setup / loop
// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(300);
  loadSettings();
  Serial.println("\n\n=== Owlet auf ESP32 - Schritt 1 ===");
  Serial.printf("Chip: %s, Kerne %d, Flash %u MB, PSRAM %u\n",
                ESP.getChipModel(), ESP.getChipCores(),
                (unsigned)(ESP.getFlashChipSize() / (1024 * 1024)),
                (unsigned)ESP.getPsramSize());
  heap("beim Start");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("WLAN");
  while (WiFi.status() != WL_CONNECTED) { delay(400); Serial.print("."); }
  Serial.printf(" verbunden, %s\n", WiFi.localIP().toString().c_str());
  heap("nach WLAN");
  startWebInterface();

  // Ohne korrekte Uhrzeit schlaegt jede Zertifikatspruefung fehl - das
  // Zertifikat waere aus Sicht des ESP32 noch nicht gueltig. Also erst NTP.
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Zeit");
  time_t now = 0;
  while (now < 1700000000) { serviceWebFor(400); Serial.print("."); time(&now); }
  Serial.printf(" gesetzt (%ld)\n", (long)now);

#if !TLS_VERIFY
  Serial.println("\n*** WARNUNG: TLS_VERIFY ist 0. Zertifikate werden NICHT");
  Serial.println("*** geprueft. Dein Owlet-Passwort geht ungeschuetzt gegen");
  Serial.println("*** Mitlesen ueber die Leitung. Nur zur Inbetriebnahme.\n");
#endif

  if (!owletLogin())      { Serial.println("Anmeldung fehlgeschlagen."); return; }
  if (!owletFindDevice()) { Serial.println("Kein Geraet gefunden.");     return; }

  Serial.printf("\nBereit. Abruf alle %u s.\n", POLL_INTERVAL_MS / 1000);
}

void loop() {
  static uint32_t last = 0;
  gWebServer.handleClient();

  // Ist die Anmeldung im setup gescheitert, hier weiter versuchen statt
  // stumm liegenzubleiben - meist ist die Owlet-Cloud nur kurz weg.
  if (gAccessToken.isEmpty()) {
    Serial.println("\nNicht angemeldet - neuer Versuch in 15 s.");
    serviceWebFor(15000);
    if (owletLogin()) owletFindDevice();
    return;
  }
  if (gDsn.isEmpty()) {
    if (!owletFindDevice()) delay(15000);
    return;
  }

  if (millis() - last < POLL_INTERVAL_MS && last != 0) { delay(50); return; }
  last = millis();

  if ((millis() / 1000) >= gTokenExpiresAt) {
    if (!owletRefresh() && !owletLogin()) {
      Serial.println("Anmeldung verloren, neuer Versuch in 30 s.");
      delay(30000);
      return;
    }
  }

  heap("vor Abruf");
  owletKeepAwake();          // Fehler hier ist nicht fatal, nur unschoen

  Vitals x;
  if (owletFetchVitals(x)) {
    printVitals(x);
    gFailCount = 0;
  } else {
    gFailCount++;
    Serial.printf("Abruf fehlgeschlagen (%u in Folge)\n", gFailCount);
    // Die Owlet-Cloud hat regelmaessig kurze Aussetzer. Erst nach mehreren
    // Fehlschlaegen die ganze Anmeldekette neu durchlaufen.
    if (gFailCount >= 5) {
      Serial.println("Zu viele Fehlschlaege - melde neu an.");
      gAccessToken = ""; gFailCount = 0;
      owletLogin();
    }
  }
  heap("nach Abruf");
}
