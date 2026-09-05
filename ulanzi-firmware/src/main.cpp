/*
 * ============================================================================
 *  Owlanzi - eigene Firmware fuer die Ulanzi TC001
 * ============================================================================
 *
 *  Zeigt Puls, Sauerstoffsaettigung und Schlafzustand einer Owlet Smart Sock
 *  auf der Pixeluhr. Holt die Daten selbst aus der Owlet-Cloud - kein Home
 *  Assistant, kein MQTT, kein Broker, kein zweites Geraet.
 *
 *  DIESES GERAET ERGAENZT DIE OWLET-BASISSTATION UND ERSETZT SIE NICHT.
 *  Faellt das WLAN aus, ist die Cloud gestoert oder haengt diese Firmware,
 *  zeigt die Uhr das an - aber sie kann es nicht in jedem Fall. Die Station
 *  bleibt der Alarmgeber, auf den man sich verlaesst.
 *
 *  Einrichtung: Beim ersten Start oeffnet das Geraet den WLAN-Hotspot
 *  "Owlanzi-Setup". Damit verbinden, http://192.168.4.1 aufrufen.
 *
 *  ZWEI KERNE: Der Netzwerkteil laeuft in einer eigenen Task auf Kern 0,
 *  Anzeige und Webserver auf Kern 1. Vorher standen beide in loop(), und die
 *  Anzeige fror bei jedem Abruf ein - bei 5 Sekunden Takt und ein bis zwei
 *  Sekunden Netzwerkzeit ruckelte der Alarm-Lauftext sichtbar. Genau dann
 *  muss er lesbar sein.
 * ============================================================================
 */
#include <WiFi.h>
#include <DNSServer.h>
#include <time.h>
#include "owlanzi.h"
#include "display.h"
#include "owlet.h"
#include "webui.h"

// secrets_local.h ist eine reine Entwicklerbequemlichkeit: sie befuellt den
// NVS einmalig, damit man beim Testen nicht jedes Mal durch den Hotspot muss.
// Ihre Zeichenketten landen dabei ABER IM ABBILD - wer die so gebaute .bin
// weitergibt, verteilt sein WLAN- und Owlet-Passwort mit. Darum baut alles,
// was veroeffentlicht wird, mit -DNO_LOCAL_SECRETS (Umgebung "release"), und
// make-installer.ps1 durchsucht das fertige Abbild zur Sicherheit nochmal.
#if !defined(NO_LOCAL_SECRETS) && __has_include("secrets_local.h")
  #include "secrets_local.h"
  #define HAVE_SEED 1
#endif

static const char *AP_SSID = "owlanzi";   // offen, ohne Passwort
static DNSServer gDns;
static uint32_t gApSince = 0;

// --- WLAN ------------------------------------------------------------------
static void startAp() {
  gSt.apMode = true;
  gApSince = millis();
  // AP_STA statt AP: nur so kann waehrend der Einrichtung nach Netzen
  // gesucht werden.
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID);                 // offen: neue Nutzer sollen kein
                                        // Passwort raten muessen
  // Captive Portal: jede Namensauflösung zeigt auf uns. Handy und Laptop
  // oeffnen die Einrichtungsseite dann von selbst, statt dass jemand eine
  // IP-Adresse eintippen muss.
  gDns.setErrorReplyCode(DNSReplyCode::NoError);
  gDns.start(53, "*", WiFi.softAPIP());
  Serial.printf("Hotspot '%s', http://%s/\n", AP_SSID, WiFi.softAPIP().toString().c_str());
}

static bool startSta() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(gCfg.wifiSsid, gCfg.wifiPass);
  Serial.printf("WLAN '%s'", gCfg.wifiSsid);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) { delay(250); Serial.print("."); }
  Serial.println();
  gSt.wifiOk = WiFi.status() == WL_CONNECTED;
  if (gSt.wifiOk) Serial.printf("verbunden, %s\n", WiFi.localIP().toString().c_str());
  return gSt.wifiOk;
}

// --- Eigene Alarme ---------------------------------------------------------
/*
 * Dauer statt Anzahl der Messungen - ein stabil schlechter Wert ist der
 * gefaehrlichste Fall und darf nicht durch eine Zaehlung fallen.
 *
 * Vier Sperren gegen Fehlalarme: eigene Alarme eingeschaltet, Socke laedt
 * nicht und ist nicht abgelegt, Messwert ist nicht 0 (das ist das
 * Abschaltsignal der Station), und der Abruf ist frisch.
 */
static void evalOne(bool cond, uint32_t &since, bool &flag, int seconds) {
  if (!cond) { since = 0; flag = false; return; }
  uint32_t now = millis();
  if (!since) since = now;
  if (!flag && now - since >= (uint32_t)seconds * 1000UL) flag = true;
}

static void evalAlarms() {
  const Vitals &v = gSt.v;
  bool usable = gCfg.ownAlarms && gSt.cloudOk &&
                (millis() - gSt.lastOkAt < 30000UL) &&
                v.charging == 0 && !v.sockOff;
  bool oxOk = usable && v.oxygen > 0;
  bool hrOk = usable && v.heart  > 0;

  evalOne(oxOk && v.oxygen < gCfg.spo2Limit,   gSt.spo2Since,   gSt.alSpo2,   gCfg.spo2Seconds);
  evalOne(hrOk && v.heart  < gCfg.hrLowLimit,  gSt.hrLowSince,  gSt.alHrLow,  gCfg.hrLowSeconds);
  evalOne(hrOk && v.heart  > gCfg.hrHighLimit, gSt.hrHighSince, gSt.alHrHigh, gCfg.hrHighSeconds);

  bool was = gAlarmCritical && !gSt.silenced;
  alarmRecompute();
  bool now = gAlarmCritical && !gSt.silenced;

  // Ton bei JEDEM kritischen Alarm - auch bei denen der Socke selbst.
  // Vorher klang nur der eigene Alarm; ein Sauerstoffalarm von Owlet, also
  // die wichtigste Meldung ueberhaupt, erschien stumm.
  if (now && !was) {
    Serial.printf("ALARM: %s\n", gAlarmText.c_str());
    soundAlarm();
  }
}

// --- Netzwerk-Task, Kern 0 -------------------------------------------------
static void netTask(void *) {
  uint32_t lastPoll = 0, retryAt = 0;
  for (;;) {
    if (gSt.apMode || !gSt.wifiOk) { vTaskDelay(pdMS_TO_TICKS(500)); continue; }
    uint32_t now = millis();

    if (!gSt.loggedIn) {
      if (now < retryAt) { vTaskDelay(pdMS_TO_TICKS(300)); continue; }
      retryAt = now + 20000;
      if (owletLogin()) owletFindDevice();
      continue;
    }
    if (owletTokenSecondsLeft() == 0 && !owletRefresh()) { gSt.loggedIn = false; continue; }
    if (!strlen(gSt.dsn)) { owletFindDevice(); vTaskDelay(pdMS_TO_TICKS(1000)); continue; }

    if (now - lastPoll < (uint32_t)gCfg.pollSeconds * 1000UL) {
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }
    lastPoll = now;
    gSt.lastTryAt = now;

    if (owletPoll()) {
      gSt.pollCount++;
    } else {
      gSt.failCount++;
      if (gSt.failCount >= 5) { gSt.loggedIn = false; gSt.failCount = 0; }
      if (millis() - gSt.lastOkAt > 90000UL) gSt.cloudOk = false;
    }
    evalAlarms();
  }
}

// --- setup / loop ----------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n\n=== Owlanzi ===");
  Serial.printf("Chip %s, Flash %u MB, Heap %u\n", ESP.getChipModel(),
                (unsigned)(ESP.getFlashChipSize() / 1048576), (unsigned)ESP.getFreeHeap());

  bool haveCfg = cfgLoad();
#ifdef HAVE_SEED
  // Nur beim Entwickeln: secrets_local.h befuellt den NVS einmalig, damit man
  // nicht jedes Mal durch den Hotspot muss. Die Datei ist gitignored und
  // fehlt in der veroeffentlichten Firmware.
  if (!haveCfg && !cfgDevNoSeed()) {
    strlcpy(gCfg.wifiSsid,  SEED_WIFI_SSID,  sizeof(gCfg.wifiSsid));
    strlcpy(gCfg.wifiPass,  SEED_WIFI_PASS,  sizeof(gCfg.wifiPass));
    strlcpy(gCfg.owletMail, SEED_OWLET_MAIL, sizeof(gCfg.owletMail));
    strlcpy(gCfg.owletPass, SEED_OWLET_PASS, sizeof(gCfg.owletPass));
    cfgSave(); haveCfg = true;
    Serial.println("secrets_local.h gefunden - NVS einmalig vorbefuellt");
  }
#endif

  dispBegin();
  soundBegin();
  pinMode(PIN_BTN_LEFT, INPUT_PULLUP);
  pinMode(PIN_BTN_MID, INPUT_PULLUP);
  pinMode(PIN_BTN_RIGHT, INPUT_PULLUP);

  if (!haveCfg) {
    startAp();
    webBegin();
  } else if (!startSta()) {
    // WLAN hinterlegt, aber nicht erreichbar: kurz melden und zurueck in die
    // Einrichtung. Danach kann der Nutzer ein anderes Netz eintragen, ohne
    // erst herausfinden zu muessen, warum nichts passiert.
    dispMessage(L("WIFI ERROR", "WLAN FEHLER"), 6, gCfg.pal.alarm);
    uint32_t t0 = millis();
    while (millis() - t0 < 6500) { dispTick(); delay(5); }
    startAp();
    webBegin();
  } else {
    // Ohne richtige Uhrzeit scheitert jede Zertifikatspruefung.
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    time_t t = 0; uint32_t t0 = millis();
    while (t < 1700000000 && millis() - t0 < 15000) { delay(250); time(&t); }
    webBegin();
    // Bei jedem Start die IP zeigen - sonst muesste man sie im Router suchen.
    dispMessage(String(L("IP ","IP ")) + WiFi.localIP().toString(), 15, gCfg.pal.numbers);
  }

  xTaskCreatePinnedToCore(netTask, "net", 8192, nullptr, 1, nullptr, 0);
}

void loop() {
  if (gSt.apMode) gDns.processNextRequest();
  webTick();
  dispTick();
  soundTick();

  // Mittlere Taste quittiert den Alarmton. Ohne das muesste man nachts um
  // drei zum Handy greifen, um die Wiederholung abzustellen.
  static uint32_t btnAt = 0;
  if (digitalRead(PIN_BTN_MID) == LOW && millis() - btnAt > 600) {
    btnAt = millis();
    if (anyAlarm()) { gSt.silenced = true; soundStop(); Serial.println("Alarm quittiert"); }
    else dispTest(TEST_VITALS, 8);
  }

  /*
   * Der Hotspot ist kein Dauerzustand. Ist ein WLAN hinterlegt und das
   * Geraet haengt trotzdem im Einrichtungsmodus - Router startet nach einem
   * Stromausfall gerade neu, also genau dann, wenn auch die Basisstation
   * wieder hochfaehrt - dann nach drei Minuten neu versuchen. Vorher blieb
   * es dort haengen, bis jemand den Stecker zog.
   */
  if (gSt.apMode && strlen(gCfg.wifiSsid) && millis() - gApSince > 180000UL) {
    Serial.println("Hotspot laeuft seit 3 min - WLAN erneut versuchen");
    ESP.restart();
  }
}
