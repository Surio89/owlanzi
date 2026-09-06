/*
 * ============================================================================
 *  owlanzi - custom firmware for the Ulanzi TC001
 * ============================================================================
 *
 *  Shows heart rate, oxygen saturation and sleep state of an Owlet Smart Sock
 *  on the pixel clock. It fetches the data from the Owlet cloud itself - no
 *  Home Assistant, no MQTT, no broker, no second device.
 *
 *  THIS DEVICE COMPLEMENTS THE OWLET BASE STATION, IT DOES NOT REPLACE IT.
 *  If Wi-Fi drops, the cloud has trouble or this firmware hangs, the clock
 *  says so - but it cannot say so in every case. The base station stays the
 *  alarm you rely on.
 *
 *  Setup: on first boot the device opens the Wi-Fi hotspot "owlanzi". Join
 *  it and browse to http://192.168.4.1.
 *
 *  TWO CORES: the network part runs in its own task on core 0, display and
 *  web server on core 1. Both used to sit in loop(), and the display froze
 *  on every poll - with a 5 second interval and one to two seconds of
 *  network time the scrolling alarm text stuttered visibly. That is exactly
 *  when it has to stay readable.
 * ============================================================================
 */
#include <WiFi.h>
#include <DNSServer.h>
#include <time.h>
#include "owlanzi.h"
#include "display.h"
#include "owlet.h"
#include "webui.h"

// secrets_local.h is a pure developer convenience: it seeds the NVS once so
// you don't have to walk through the hotspot on every test. Its string
// literals END UP IN THE IMAGE though - whoever hands out a .bin built that
// way hands out their Wi-Fi and Owlet password with it. So anything that
// gets published builds with -DNO_LOCAL_SECRETS (environment "release"), and
// make-installer.ps1 scans the finished image once more to be sure.
#if !defined(NO_LOCAL_SECRETS) && __has_include("secrets_local.h")
  #include "secrets_local.h"
  #define HAVE_SEED 1
#endif

static const char *AP_SSID = "owlanzi";   // open, no password
static DNSServer gDns;
static uint32_t gApSince = 0;

// --- Wi-Fi -----------------------------------------------------------------
static void startAp() {
  gSt.apMode = true;
  gApSince = millis();
  // AP_STA rather than AP: only this way can we scan for networks while the
  // setup page is open.
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID);                 // open: a new user should not have
                                        // to guess a password
  // Captive portal: every name resolution points at us. Phones and laptops
  // then open the setup page by themselves instead of somebody having to
  // type an IP address.
  gDns.setErrorReplyCode(DNSReplyCode::NoError);
  gDns.start(53, "*", WiFi.softAPIP());
  Serial.printf("Hotspot '%s', http://%s/\n", AP_SSID, WiFi.softAPIP().toString().c_str());
}

static bool startSta() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(gCfg.wifiSsid, gCfg.wifiPass);
  Serial.printf("Wi-Fi '%s'", gCfg.wifiSsid);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) { delay(250); Serial.print("."); }
  Serial.println();
  gSt.wifiOk = WiFi.status() == WL_CONNECTED;
  if (gSt.wifiOk) Serial.printf("connected, %s\n", WiFi.localIP().toString().c_str());
  return gSt.wifiOk;
}

// --- Own alarms ------------------------------------------------------------
/*
 * Duration rather than a count of readings - a steadily bad value is the
 * most dangerous case and must not slip through a counter.
 *
 * Four guards against false alarms: own alarms switched on, sock neither
 * charging nor taken off, reading is not 0 (that is the base station being
 * switched off), and the poll is fresh.
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

  // Sound on EVERY critical alarm - including the sock's own. Before, only
  // our own rules made a noise; an oxygen alarm from Owlet, the single most
  // important message there is, arrived silently.
  if (now && !was) {
    Serial.printf("ALARM: %s\n", gAlarmText.c_str());
    soundAlarm();
  }
}

// --- Network task, core 0 --------------------------------------------------
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
  Serial.println("\n\n=== owlanzi ===");
  Serial.printf("Chip %s, flash %u MB, heap %u\n", ESP.getChipModel(),
                (unsigned)(ESP.getFlashChipSize() / 1048576), (unsigned)ESP.getFreeHeap());

  bool haveCfg = cfgLoad();
#ifdef HAVE_SEED
  // Development only: secrets_local.h seeds the NVS once so you don't have
  // to walk through the hotspot every time. The file is gitignored and
  // absent from the published firmware.
  if (!haveCfg && !cfgDevNoSeed()) {
    strlcpy(gCfg.wifiSsid,  SEED_WIFI_SSID,  sizeof(gCfg.wifiSsid));
    strlcpy(gCfg.wifiPass,  SEED_WIFI_PASS,  sizeof(gCfg.wifiPass));
    strlcpy(gCfg.owletMail, SEED_OWLET_MAIL, sizeof(gCfg.owletMail));
    strlcpy(gCfg.owletPass, SEED_OWLET_PASS, sizeof(gCfg.owletPass));
    cfgSave(); haveCfg = true;
    Serial.println("secrets_local.h found - NVS seeded once");
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
    // Wi-Fi is stored but unreachable: say so briefly and go back to setup.
    // The user can then enter a different network without first having to
    // work out why nothing is happening.
    dispMessage(L("WIFI ERROR", "WLAN FEHLER"), 6, gCfg.pal.alarm);
    uint32_t t0 = millis();
    while (millis() - t0 < 6500) { dispTick(); delay(5); }
    startAp();
    webBegin();
  } else {
    // Without a correct clock every certificate check fails.
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    time_t t = 0; uint32_t t0 = millis();
    while (t < 1700000000 && millis() - t0 < 15000) { delay(250); time(&t); }
    webBegin();
    // Show the IP on every boot - otherwise you would have to hunt for it in
    // the router.
    dispMessage(String(L("IP ","IP ")) + WiFi.localIP().toString(), 15, gCfg.pal.numbers);
  }

  xTaskCreatePinnedToCore(netTask, "net", 8192, nullptr, 1, nullptr, 0);
}

void loop() {
  if (gSt.apMode) gDns.processNextRequest();
  webTick();
  dispTick();
  soundTick();

  // The middle button acknowledges the alarm tone. Without it you would have
  // to reach for your phone at three in the morning to stop the repeat.
  static uint32_t btnAt = 0;
  if (digitalRead(PIN_BTN_MID) == LOW && millis() - btnAt > 600) {
    btnAt = millis();
    if (anyAlarm()) { gSt.silenced = true; soundStop(); Serial.println("alarm acknowledged"); }
    else dispTest(TEST_VITALS, 8);
  }

  /*
   * The hotspot is not a permanent state. If a Wi-Fi is stored and the
   * device is still sitting in setup mode - the router is rebooting after a
   * power cut, which is exactly when the base station comes back up too -
   * retry after three minutes. Before, it stayed there until somebody pulled
   * the plug.
   */
  if (gSt.apMode && strlen(gCfg.wifiSsid) && millis() - gApSince > 180000UL) {
    Serial.println("hotspot up for 3 min - retrying Wi-Fi");
    ESP.restart();
  }
}
