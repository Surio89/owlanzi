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
 *  web server in separate tasks on core 1. Both used to sit in loop(), and the display froze
 *  on every poll - with a 5 second interval and one to two seconds of
 *  network time the scrolling alarm text stuttered visibly. That is exactly
 *  when it has to stay readable.
 * ============================================================================
 */
#include <WiFi.h>
#include <DNSServer.h>
#include <time.h>
#include <esp_system.h>
#include "owlanzi.h"
#include "display.h"
#include "owlet.h"
#include "webui.h"
#include "online_update.h"

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
// ESP-IDF specifies task stacks in bytes. HTTPS certificate verification needs
// headroom beyond the application frames; 1.0.4 used only 10 KB here.
static constexpr uint32_t NET_TASK_STACK_BYTES=16384;

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

// --- Network task, core 0 --------------------------------------------------
// Every HTTPS operation takes an immutable configuration snapshot. Only
// short state transitions hold the mutex; HTTP and display never share a wait.
static void netTask(void *) {
  uint32_t lastPoll=0,lastRetry=0,lastFind=0,generation=0;
  bool retryWaiting=false,findWaiting=false;
  for(;;) {
    // TLS downloads share this worker with Owlet so memory use stays bounded.
    if(onlineUpdateTick())continue;
    if(updateBusy()){vTaskDelay(pdMS_TO_TICKS(100));continue;}
    uint32_t now=millis();
    bool connected,ap,logged,haveDsn; int pollSeconds; uint32_t currentGeneration;
    { StateGuard lock;
      connected=gSt.wifiOk; ap=gSt.apMode; logged=gSt.loggedIn;
      haveDsn=strlen(gSt.dsn)>0; pollSeconds=gCfg.pollSeconds;
      currentGeneration=gSt.authGeneration;
    }
    if(currentGeneration!=generation) {
      generation=currentGeneration;retryWaiting=false;findWaiting=false;
      lastPoll=now-(uint32_t)pollSeconds*1000;
    }
    if(ap || !connected) {vTaskDelay(pdMS_TO_TICKS(200));continue;}
    if(!logged) {
      if(retryWaiting && now-lastRetry<20000){vTaskDelay(pdMS_TO_TICKS(100));continue;}
      {StateGuard lock;alarmsResetOwn();alarmRecompute();}
      if(owletLogin()) {owletFindDevice();findWaiting=true;lastFind=millis();}
      lastRetry=millis();retryWaiting=true;
      continue;
    }
    if(owletTokenSecondsLeft()==0 && !owletRefresh()) {
      StateGuard lock;
      if(generation==gSt.authGeneration) {gSt.loggedIn=false;alarmsResetOwn();alarmRecompute();}
      continue;
    }
    if(!haveDsn) {
      if(!findWaiting || now-lastFind>=5000) {owletFindDevice();lastFind=millis();findWaiting=true;}
      vTaskDelay(pdMS_TO_TICKS(100));continue;
    }
    if(now-lastPoll<(uint32_t)pollSeconds*1000) {vTaskDelay(pdMS_TO_TICKS(100));continue;}
    lastPoll=now;
    {StateGuard lock;gSt.lastTryAt=now;}
    bool ok=owletPoll();
    {StateGuard lock;
      if(generation!=gSt.authGeneration)continue;
      if(ok)++gSt.pollCount;
      else {
        gSt.cloudOk=false;
        if(++gSt.failCount>=5) {gSt.loggedIn=false;gSt.failCount=0;}
        alarmsResetOwn();alarmRecompute();
      }
    }
  }
}

static void webTask(void *) {
  for(;;) { webTick(); vTaskDelay(pdMS_TO_TICKS(5)); }
}

// --- setup / loop ----------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n\n=== owlanzi ===");
  Serial.printf("Firmware %s, reset reason %d\n",OWLANZI_VERSION,(int)esp_reset_reason());
  Serial.printf("Chip %s, flash %u MB, heap %u\n", ESP.getChipModel(),
                (unsigned)(ESP.getFlashChipSize() / 1048576), (unsigned)ESP.getFreeHeap());

  stateBegin();
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

  // TLS certificate/key calculations can run for >5 s without yielding on the
  // original ESP32. Share priority with IDLE0 so RTOS time slicing can run its
  // watchdog hook throughout HTTPS. Wi-Fi/TCP tasks still have higher priority.
  // Do not disable or feed the watchdog on behalf of a starved idle task.
  if(xTaskCreatePinnedToCore(netTask,"net",NET_TASK_STACK_BYTES,nullptr,tskIDLE_PRIORITY,nullptr,0)!=pdPASS)
    strlcpy(gSt.lastError,"Cannot start network task",sizeof(gSt.lastError));
  if(xTaskCreatePinnedToCore(webTask,"web",10240,nullptr,1,nullptr,1)!=pdPASS) {
    StateGuard lock;
    strlcpy(gSt.lastError,"Cannot start web task",sizeof(gSt.lastError));
  }
}

void loop() {
  if (gSt.apMode) gDns.processNextRequest();
  { StateGuard lock;
    bool wifi=WiFi.status()==WL_CONNECTED;
    if(gSt.wifiOk && !wifi) {gSt.cloudOk=false;alarmsResetOwn();alarmRecompute();}
    gSt.wifiOk=wifi;
  }
  stateTick();
  dispTick();
  soundTick();

  // The middle button acknowledges the alarm tone. Without it you would have
  // to reach for your phone at three in the morning to stop the repeat.
  static uint32_t btnAt = 0;
  static bool wasDown=false;
  bool down=digitalRead(PIN_BTN_MID)==LOW;
  if (down && !wasDown && millis() - btnAt > 60) {
    btnAt = millis();
    if (anyAlarm()) { alarmAcknowledge(); soundStop(); Serial.println("alarm acknowledged"); }
    else dispTest(TEST_VITALS, 8);
  }

  wasDown=down;

  /*
   * The hotspot is not a permanent state. If a Wi-Fi is stored and the
   * device is still sitting in setup mode - the router is rebooting after a
   * power cut, which is exactly when the base station comes back up too -
   * retry after three minutes. Before, it stayed there until somebody pulled
   * the plug.
   */
  bool retryWifi;
  {StateGuard lock;retryWifi=gSt.apMode && strlen(gCfg.wifiSsid) && millis()-gApSince>180000UL;}
  if (retryWifi) {
    Serial.println("hotspot up for 3 min - retrying Wi-Fi");
    ESP.restart();
  }
  delay(1);
}
