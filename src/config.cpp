/*
 * config.cpp - settings in the NVS, alarm evaluation
 *
 * Credentials do NOT belong in the source. They are entered in the web
 * interface and live in the device's NVS. That way the same firmware can be
 * passed on without anyone having to type their Owlet details into a .cpp
 * and recompile.
 */
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "owlanzi.h"

Config gCfg;
State  gSt;
String gAlarmText;
bool   gAlarmCritical = false;

static Preferences prefs;
static SemaphoreHandle_t gMux = nullptr;

void stateLock() {
  if (!gMux) gMux = xSemaphoreCreateMutex();
  xSemaphoreTake(gMux, portMAX_DELAY);
}
void stateUnlock() { if (gMux) xSemaphoreGive(gMux); }

#define G(k, f)   gCfg.f = prefs.getInt(k, gCfg.f)
#define GB(k, f)  gCfg.f = prefs.getBool(k, gCfg.f)
#define GS(k, f)  prefs.getString(k, gCfg.f, sizeof(gCfg.f))
#define P(k, f)   prefs.putInt(k, gCfg.f)
#define PB(k, f)  prefs.putBool(k, gCfg.f)
#define PS(k, f)  prefs.putString(k, gCfg.f)

bool cfgLoad() {
  prefs.begin("owlanzi", true);
  GS("ssid", wifiSsid);  GS("wpass", wifiPass);
  GS("mail", owletMail); GS("opass", owletPass);
  GS("webp", webPass);
  GB("eu", europe);      GB("own", ownAlarms);
  G("spo2L", spo2Limit); G("spo2S", spo2Seconds);
  G("hrLL", hrLowLimit); G("hrLS", hrLowSeconds);
  G("hrHL", hrHighLimit);G("hrHS", hrHighSeconds);
  G("briMin", briMin);   G("briDay", briDay);
  G("briAl", briAlarm);  G("briTst", briTest);
  G("ldrT", ldrThreshold); G("ldrH", ldrHysteresis);
  G("vol", volAlarm);    GB("snd", soundEnabled);
  G("arep", alarmRepeatSec);
  GB("serp", serpentine);G("poll", pollSeconds);
  GS("lang", lang);
  // The palette is stored as one block. If the size does not match (older
  // firmware, a new colour added since), the defaults simply stay.
  if (prefs.getBytesLength("pal") == sizeof(Palette))
    prefs.getBytes("pal", &gCfg.pal, sizeof(Palette));
  prefs.end();
  return strlen(gCfg.wifiSsid) > 0;
}

void cfgSave() {
  prefs.begin("owlanzi", false);
  PS("ssid", wifiSsid);  PS("wpass", wifiPass);
  PS("mail", owletMail); PS("opass", owletPass);
  PS("webp", webPass);
  PB("eu", europe);      PB("own", ownAlarms);
  P("spo2L", spo2Limit); P("spo2S", spo2Seconds);
  P("hrLL", hrLowLimit); P("hrLS", hrLowSeconds);
  P("hrHL", hrHighLimit);P("hrHS", hrHighSeconds);
  P("briMin", briMin);   P("briDay", briDay);
  P("briAl", briAlarm);  P("briTst", briTest);
  P("ldrT", ldrThreshold); P("ldrH", ldrHysteresis);
  P("vol", volAlarm);    PB("snd", soundEnabled);
  P("arep", alarmRepeatSec);
  PB("serp", serpentine);P("poll", pollSeconds);
  PS("lang", lang);
  prefs.putBytes("pal", &gCfg.pal, sizeof(Palette));
  prefs.end();
}

/*
 * Factory reset. The important part: the developer seeding from
 * secrets_local.h must NOT kick in again afterwards - otherwise the device
 * reboots, immediately refills itself with the same credentials, and to the
 * user it looks as if nothing happened at all. That is exactly what went
 * wrong once. The marker lives in its own namespace, which the reset
 * deliberately leaves alone.
 */
void cfgFactoryReset() {
  prefs.begin("owlanzi", false);
  prefs.clear();
  prefs.end();
  prefs.begin("owlanzi_dev", false);
  prefs.putBool("noseed", true);
  prefs.end();
}

bool cfgDevNoSeed() {
  prefs.begin("owlanzi_dev", true);
  bool v = prefs.getBool("noseed", false);
  prefs.end();
  return v;
}

// --- Language --------------------------------------------------------------
// English is the default so the device is understandable to a stranger
// without changing anything. Switchable in the web interface.
const char *L(const char *en, const char *de) {
  return gCfg.lang[0] == 'd' ? de : en;
}

// --- Freshness gate --------------------------------------------------------
/*
 * A stale value must NEVER sit on the display. Two conditions, the same as
 * in the Home Assistant version and for the same reason:
 *   a) the value actually changed within the last 60 s, and
 *   b) that change came at least 5 s after charging ended.
 * Without (b) the device shows the last reading of the PREVIOUS night right
 * after the sock comes off the charger - the Owlet cloud serves it
 * immediately, and to us it looks brand new.
 */
bool vitalsFresh() {
  if (!gSt.cloudOk) return false;
  uint32_t now = millis();
  if (now - gSt.lastOkAt > 20000UL) return false;          // fetch is stuck
  if (!gSt.vitalsChangedAt) return false;                  // nothing yet
  if (now - gSt.vitalsChangedAt > 60000UL) return false;   // standing still
  if (gSt.chargeEndedAt &&
      (int32_t)(gSt.vitalsChangedAt - gSt.chargeEndedAt) < 5000) return false;
  return true;
}

// --- Alarm evaluation ------------------------------------------------------
/*
 * Called only when something changed - after each poll and whenever the own
 * alarms are toggled. This used to run in the draw loop, 40 times a second,
 * building strings every time. On a device that runs for months that
 * fragments the heap.
 */
void alarmRecompute() {
  const Vitals &v = gSt.v;
  String l; bool crit = false;
  auto add = [&](const String &s, bool critical) {
    if (l.length()) l += "   +   ";
    l += s;
    if (critical) crit = true;
  };
  int hr = (int)lroundf(v.heart), ox = (int)lroundf(v.oxygen);

  if (v.lowHr)      add(String(L("HEART RATE ","PULS ")) + hr + L(" TOO LOW"," ZU NIEDRIG"), true);
  if (v.highHr)     add(String(L("HEART RATE ","PULS ")) + hr + L(" TOO HIGH"," ZU HOCH"), true);
  if (v.lowOx)      add(String(L("OXYGEN ","SAUERSTOFF ")) + ox + L(" TOO LOW"," ZU NIEDRIG"), true);
  if (v.highOx)     add(String(L("OXYGEN ","SAUERSTOFF ")) + ox + L(" TOO HIGH"," ZU HOCH"), true);
  // The own alarms carry the word SUSTAINED / DAUERHAFT - that is how you
  // tell at a glance they come from your own rule and not from Owlet.
  if (gSt.alSpo2)   add(String(L("OXYGEN ","SAUERSTOFF ")) + ox + L(" SUSTAINED BELOW "," DAUERHAFT UNTER ") + gCfg.spo2Limit, true);
  if (gSt.alHrLow)  add(String(L("HEART RATE ","PULS ")) + hr + L(" SUSTAINED BELOW "," DAUERHAFT UNTER ") + gCfg.hrLowLimit, true);
  if (gSt.alHrHigh) add(String(L("HEART RATE ","PULS ")) + hr + L(" SUSTAINED ABOVE "," DAUERHAFT UEBER ") + gCfg.hrHighLimit, true);
  if (v.sockDiscon) add(L("SOCK DISCONNECTED","SOCKE GETRENNT"), true);
  if (v.lostPower)  add(L("BASE STATION LOST POWER","BASISSTATION OHNE STROM"), true);
  // A flat sock battery is a notice, not an emergency: amber, no sound, no
  // pulling the brightness up. At night this must not light up the room.
  if (v.lowBatt)    add(L("SOCK BATTERY LOW","SOCKE AKKU LEER"), false);

  gAlarmText = l;
  gAlarmCritical = crit;
  if (!crit) gSt.silenced = false;   // an acknowledgement covers the alarm only
}

bool anyAlarm() { return gAlarmCritical && !gSt.silenced; }
