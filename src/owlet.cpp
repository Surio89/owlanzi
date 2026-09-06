/*
 * owlet.cpp - the login chain and the data fetch
 *
 *   1. Firebase   mail + password        -> idToken (JWT, ~940 chars)
 *   2. Owlet SSO  idToken                -> mini_token
 *   3. Ayla       mini_token + secret    -> access_token (valid 24 h)
 *   4. Ayla       access_token           -> device list (dsn)
 *   5. Ayla       APP_ACTIVE = 1         (without it the cloud freezes)
 *   6. Ayla       properties.json        -> REAL_TIME_VITALS
 *
 * Endpoints and field names taken from:
 *   github.com/ryanbdclark/pyowletapi (src/pyowletapi/const.py)
 *   github.com/mbevand/owlet_monitor
 */
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "owlet.h"

extern const uint8_t rootca_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t rootca_crt_bundle_end[]   asm("_binary_x509_crt_bundle_end");

static const char *EU_KEY   = "AIzaSyDm6EhV70wudwN3iOSq3vTjtsdGjdFLuuM";
static const char *EU_ID    = "OwletCare-Android-EU-fw-id";
static const char *EU_SEC   = "OwletCare-Android-EU-JKupMPBoj_Npce_9a95Pc8Qo0Mw";
static const char *EU_MINI  = "https://ayla-sso.eu.owletdata.com/mini/";
static const char *EU_SIGN  = "https://user-field-eu-1a2039d9.aylanetworks.com/api/v1/token_sign_in";
static const char *EU_REFR  = "https://user-field-eu-1a2039d9.aylanetworks.com/users/refresh_token.json";
static const char *EU_BASE  = "https://ads-field-eu-1a2039d9.aylanetworks.com/apiv1";

static const char *WO_KEY   = "AIzaSyCsDZ8kWxQuLJAMVnmEhEkayH1TSxKXfGA";
static const char *WO_ID    = "sso-prod-3g-id";
static const char *WO_SEC   = "sso-prod-UEjtnPCtFfjdwIwxqnC0OipxRFU";
static const char *WO_MINI  = "https://ayla-sso.owletdata.com/mini/";
static const char *WO_SIGN  = "https://user-field-1a2039d9.aylanetworks.com/api/v1/token_sign_in";
static const char *WO_REFR  = "https://user-field-1a2039d9.aylanetworks.com/users/refresh_token.json";
static const char *WO_BASE  = "https://ads-field-1a2039d9.aylanetworks.com/apiv1";

#define RK (gCfg.europe ? EU_KEY  : WO_KEY)
#define RI (gCfg.europe ? EU_ID   : WO_ID)
#define RS (gCfg.europe ? EU_SEC  : WO_SEC)
#define RM (gCfg.europe ? EU_MINI : WO_MINI)
#define RG (gCfg.europe ? EU_SIGN : WO_SIGN)
#define RR (gCfg.europe ? EU_REFR : WO_REFR)
#define RB (gCfg.europe ? EU_BASE : WO_BASE)

static const char *ANDROID_PKG  = "com.owletcare.owletcare";
static const char *ANDROID_CERT = "2A3BC26DB0B8B0792DBE28E6FFDC2598F9B12B74";

static String   gAccess, gRefreshTok;
static uint32_t gExpiresAt = 0;

uint32_t owletTokenSecondsLeft() {
  uint32_t now = millis() / 1000;
  return gExpiresAt > now ? gExpiresAt - now : 0;
}
static void fail(const char *fmt, ...) {
  va_list ap; va_start(ap, fmt);
  vsnprintf(gSt.lastError, sizeof(gSt.lastError), fmt, ap);
  va_end(ap);
  Serial.printf("   !! %s\n", gSt.lastError);
}

/*
 * One HTTPS call with filtered JSON parsing.
 *
 * IMPORTANT, this one cost an hour:
 * with "Transfer-Encoding: chunked", HTTPClient::getStream() hands the chunk
 * length markers through as part of the data. ArduinoJson trips over them
 * and returns an empty document WITHOUT reporting an error. That turned the
 * 940 character JWT into the string "null". Every Owlet response is chunked.
 * getString() unpacks the markers correctly.
 */
static bool httpJson(const char *method, const String &url, const String &body,
                     const char *h1, const String &v1,
                     const char *h2, const String &v2,
                     JsonDocument &out, JsonDocument *filter, const char *label) {
  WiFiClientSecure client;
  // Core 3.x wants the size as well, 2.x does not.
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  client.setCACertBundle(rootca_crt_bundle_start,
                         (size_t)(rootca_crt_bundle_end - rootca_crt_bundle_start));
#else
  client.setCACertBundle(rootca_crt_bundle_start);
  (void)rootca_crt_bundle_end;
#endif
  client.setTimeout(15000);

  HTTPClient http;
  http.setTimeout(15000);
  http.setReuse(false);
  if (!http.begin(client, url)) { fail("%s: begin() failed", label); return false; }

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept", "application/json");
  if (h1) http.addHeader(h1, v1);
  if (h2) http.addHeader(h2, v2);

  int code = (strcmp(method, "POST") == 0)
               ? http.POST((uint8_t *)body.c_str(), body.length())
               : http.GET();

  if (code <= 0) { fail("%s: %s", label, http.errorToString(code).c_str()); http.end(); return false; }
  if (code < 200 || code >= 300) {
    String e = http.getString();
    if (e.length() > 120) e = e.substring(0, 120);
    fail("%s: HTTP %d %s", label, code, e.c_str());
    http.end(); return false;
  }

  int clen = http.getSize();            // -1 = chunked
  DeserializationError e;
  if (clen >= 0) {
    e = filter ? deserializeJson(out, http.getStream(), DeserializationOption::Filter(*filter))
               : deserializeJson(out, http.getStream());
  } else {
    String payload = http.getString();
    e = filter ? deserializeJson(out, payload, DeserializationOption::Filter(*filter))
               : deserializeJson(out, payload);
  }
  http.end();
  if (e) { fail("%s: JSON %s", label, e.c_str()); return false; }
  return true;
}

bool owletLogin() {
  gSt.loggedIn = false;
  if (!strlen(gCfg.owletMail)) { fail("No Owlet credentials stored"); return false; }
  Serial.println("-- Owlet login --");

  String idToken;
  {
    String url = String("https://www.googleapis.com/identitytoolkit/v3/"
                        "relyingparty/verifyPassword?key=") + RK;
    JsonDocument req;
    req["email"] = gCfg.owletMail;
    req["password"] = gCfg.owletPass;
    req["returnSecureToken"] = true;
    String body; serializeJson(req, body);
    JsonDocument filter; filter["idToken"] = true;
    JsonDocument res;
    if (!httpJson("POST", url, body, "X-Android-Package", ANDROID_PKG,
                  "X-Android-Cert", ANDROID_CERT, res, &filter, "Firebase")) return false;
    idToken = res["idToken"].as<String>();
    if (idToken.length() < 100) { fail("Firebase: idToken only %u chars", idToken.length()); return false; }
    Serial.printf("   1/3 Firebase ok (%u chars)\n", idToken.length());
  }
  String mini;
  {
    // The bare JWT, with no "Bearer" in front - with Bearer the service
    // answers 401.
    JsonDocument filter; filter["mini_token"] = true;
    JsonDocument res;
    if (!httpJson("GET", RM, "", "Authorization", idToken, nullptr, "",
                  res, &filter, "Owlet SSO")) return false;
    mini = res["mini_token"].as<String>();
    if (mini.length() < 10) { fail("SSO: no mini_token"); return false; }
    Serial.printf("   2/3 SSO ok (%u chars)\n", mini.length());
  }
  idToken = String();
  {
    JsonDocument req;
    req["app_id"] = RI; req["app_secret"] = RS;
    req["provider"] = "owl_id"; req["token"] = mini;
    String body; serializeJson(req, body);
    JsonDocument filter;
    filter["access_token"] = true; filter["refresh_token"] = true; filter["expires_in"] = true;
    JsonDocument res;
    if (!httpJson("POST", RG, body, nullptr, "", nullptr, "", res, &filter, "Ayla")) return false;
    gAccess = res["access_token"].as<String>();
    gRefreshTok = res["refresh_token"].as<String>();
    uint32_t ttl = res["expires_in"] | 3600;
    if (!gAccess.length()) { fail("Ayla: no access_token"); return false; }
    gExpiresAt = millis() / 1000 + (ttl > 120 ? ttl - 120 : ttl);
    Serial.printf("   3/3 Ayla ok, token valid for %u s\n", ttl);
  }
  gSt.loggedIn = true;
  gSt.lastError[0] = 0;
  return true;
}

bool owletRefresh() {
  if (!gRefreshTok.length()) return false;
  JsonDocument req; req["user"]["refresh_token"] = gRefreshTok;
  String body; serializeJson(req, body);
  JsonDocument filter;
  filter["access_token"] = true; filter["refresh_token"] = true; filter["expires_in"] = true;
  JsonDocument res;
  if (!httpJson("POST", RR, body, nullptr, "", nullptr, "", res, &filter, "Refresh")) return false;
  String at = res["access_token"].as<String>();
  if (!at.length()) return false;
  gAccess = at;
  if (!res["refresh_token"].isNull()) gRefreshTok = res["refresh_token"].as<String>();
  uint32_t ttl = res["expires_in"] | 3600;
  gExpiresAt = millis() / 1000 + (ttl > 120 ? ttl - 120 : ttl);
  return true;
}

bool owletFindDevice() {
  JsonDocument filter;
  filter[0]["device"]["dsn"] = true;
  JsonDocument res;
  if (!httpJson("GET", String(RB) + "/devices.json", "",
                "Authorization", String("auth_token ") + gAccess,
                nullptr, "", res, &filter, "Device list")) return false;
  JsonArray arr = res.as<JsonArray>();
  if (arr.isNull() || arr.size() == 0) { fail("No devices in this account"); return false; }
  const char *dsn = arr[0]["device"]["dsn"];
  if (!dsn) { fail("No serial number readable"); return false; }
  strlcpy(gSt.dsn, dsn, sizeof(gSt.dsn));
  Serial.printf("   device: %s\n", gSt.dsn);
  return true;
}

static bool boolProp(JsonVariant v) {
  if (v.isNull()) return false;
  if (v.is<bool>()) return v.as<bool>();
  return v.as<int>() != 0;
}

bool owletPoll() {
  if (!gSt.loggedIn || !strlen(gSt.dsn)) return false;
  String auth = String("auth_token ") + gAccess;

  // APP_ACTIVE wakes the cloud up. Without it, it serves frozen values - it
  // only refreshes them while an app is listening.
  {
    JsonDocument f; f.set(false);
    JsonDocument r;
    httpJson("POST", String(RB) + "/dsns/" + gSt.dsn + "/properties/APP_ACTIVE/datapoints.json",
             "{\"datapoint\":{\"metadata\":{},\"value\":1}}",
             "Authorization", auth, nullptr, "", r, &f, "APP_ACTIVE");
  }

  // properties.json is around 24 kB. The filter keeps only name and value
  // per entry - everything else is dropped while parsing.
  JsonDocument filter;
  filter[0]["property"]["name"]  = true;
  filter[0]["property"]["value"] = true;
  JsonDocument res;
  if (!httpJson("GET", String(RB) + "/dsns/" + gSt.dsn + "/properties.json", "",
                "Authorization", auth, nullptr, "", res, &filter, "properties")) return false;

  JsonArray arr = res.as<JsonArray>();
  if (arr.isNull()) { fail("properties: not an array"); return false; }

  Vitals v;
  String raw;
  for (JsonObject o : arr) {
    const char *n = o["property"]["name"];
    if (!n) continue;
    JsonVariant val = o["property"]["value"];
    if      (!strcmp(n, "REAL_TIME_VITALS")) raw = val.as<String>();
    else if (!strcmp(n, "LOW_OX_ALRT"))      v.lowOx      = boolProp(val);
    else if (!strcmp(n, "HIGH_OX_ALRT"))     v.highOx     = boolProp(val);
    else if (!strcmp(n, "LOW_HR_ALRT"))      v.lowHr      = boolProp(val);
    else if (!strcmp(n, "HIGH_HR_ALRT"))     v.highHr     = boolProp(val);
    else if (!strcmp(n, "LOST_POWER_ALRT"))  v.lostPower  = boolProp(val);
    else if (!strcmp(n, "SOCK_DISCON_ALRT")) v.sockDiscon = boolProp(val);
    else if (!strcmp(n, "SOCK_OFF"))         v.sockOff    = boolProp(val);
    else if (!strcmp(n, "LOW_BATT_ALRT"))    v.lowBatt    = boolProp(val);
  }
  if (!raw.length()) { fail("REAL_TIME_VITALS missing (older sock?)"); return false; }

  // The value is itself JSON again, wrapped in a string.
  JsonDocument j;
  if (deserializeJson(j, raw)) { fail("vitals blob not readable"); return false; }
  v.oxygen   = j["ox"]   | 0.0f;
  v.heart    = j["hr"]   | 0.0f;
  v.battery  = j["bat"]  | 0.0f;
  v.oxygen10 = j["oxta"] | 0.0f;
  v.baseOn   = boolProp(j["bso"]);
  v.sleepSt  = j["ss"]   | 0;
  v.charging = j["chg"]  | 0;
  v.sockConn = j["sc"]   | 0;
  v.movement = j["mv"]   | 0;
  strlcpy(v.hardware, j["hw"] | "", sizeof(v.hardware));
  v.valid = true;
  v.fetchedAt = millis();

  // The network task on core 0 writes the vitals, the display reads them on
  // core 1 - without a lock it could see a half-overwritten struct.
  stateLock();
  // Record the change timestamps BEFORE the new values are taken over -
  // the decision whether anything may be shown hangs on them later.
  if (gSt.v.charging != 0 && v.charging == 0) gSt.chargeEndedAt = millis();
  if (v.heart != gSt.lastHr || v.oxygen != gSt.lastOx) {
    gSt.lastHr = v.heart; gSt.lastOx = v.oxygen;
    gSt.vitalsChangedAt = millis();
  }
  gSt.v = v;
  gSt.cloudOk = true;
  gSt.lastOkAt = millis();
  gSt.failCount = 0;
  gSt.lastError[0] = 0;
  stateUnlock();
  return true;
}
