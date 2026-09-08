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

void stateBegin() { gMux = xSemaphoreCreateRecursiveMutex(); configASSERT(gMux); }
void stateLock() { xSemaphoreTakeRecursive(gMux, portMAX_DELAY); }
void stateUnlock() { xSemaphoreGiveRecursive(gMux); }

#define G(k, f)   gCfg.f = prefs.getInt(k, gCfg.f)
#define GB(k, f)  gCfg.f = prefs.getBool(k, gCfg.f)
#define GS(k, f)  prefs.getString(k, gCfg.f, sizeof(gCfg.f))
#define P(k, f)   prefs.putInt(k, gCfg.f)
#define PB(k, f)  prefs.putBool(k, gCfg.f)
#define PUTS(k, f) prefs.putString(k, gCfg.f)

bool cfgLoad() {
  StateGuard lock;
  prefs.begin("owlanzi", true);
  GS("ssid", wifiSsid);  GS("wpass", wifiPass);
  GS("mail", owletMail); GS("opass", owletPass);
  GS("webp", webPass);
  GS("dsn", owletDsn);
  GB("eu", europe);      GB("own", ownAlarms);
  G("spo2L", spo2Limit); G("spo2S", spo2Seconds);
  G("hrLL", hrLowLimit); G("hrLS", hrLowSeconds);
  G("hrHL", hrHighLimit);G("hrHS", hrHighSeconds);
  G("briMin", briMin);   G("briDay", briDay);
  G("briAl", briAlarm);  G("briTst", briTest);
  G("ldrT", ldrThreshold); G("ldrH", ldrHysteresis);
  G("vol", volAlarm);    GB("snd", soundEnabled);
  G("arep", alarmRepeatSec);
  G("poll", pollSeconds);
  GB("autoUpd", autoUpdateCheck);
  GS("lang", lang);
  // The palette is stored as one block. If the size does not match (older
  // firmware, a new colour added since), the defaults simply stay.
  if (prefs.getBytesLength("pal") == sizeof(Palette))
    prefs.getBytes("pal", &gCfg.pal, sizeof(Palette));
  prefs.end();
  // Preserve credentials on migration; unsafe legacy numeric values revert
  // to defaults and leave optional own alarms disabled.
  if (!cfgValid(gCfg)) {
    Config safe;
    strlcpy(safe.wifiSsid,gCfg.wifiSsid,sizeof(safe.wifiSsid));
    strlcpy(safe.wifiPass,gCfg.wifiPass,sizeof(safe.wifiPass));
    strlcpy(safe.owletMail,gCfg.owletMail,sizeof(safe.owletMail));
    strlcpy(safe.owletPass,gCfg.owletPass,sizeof(safe.owletPass));
    strlcpy(safe.webPass,gCfg.webPass,sizeof(safe.webPass));
    safe.europe=gCfg.europe;
    gCfg=safe;
  }
  return strlen(gCfg.wifiSsid) > 0;
}

void cfgSave() {
  StateGuard lock;
  prefs.begin("owlanzi", false);
  PUTS("ssid", wifiSsid);  PUTS("wpass", wifiPass);
  PUTS("mail", owletMail); PUTS("opass", owletPass);
  PUTS("webp", webPass);  PUTS("dsn", owletDsn);
  PB("eu", europe);      PB("own", ownAlarms);
  P("spo2L", spo2Limit); P("spo2S", spo2Seconds);
  P("hrLL", hrLowLimit); P("hrLS", hrLowSeconds);
  P("hrHL", hrHighLimit);P("hrHS", hrHighSeconds);
  P("briMin", briMin);   P("briDay", briDay);
  P("briAl", briAlarm);  P("briTst", briTest);
  P("ldrT", ldrThreshold); P("ldrH", ldrHysteresis);
  P("vol", volAlarm);    PB("snd", soundEnabled);
  P("arep", alarmRepeatSec);
  P("poll", pollSeconds);
  PB("autoUpd", autoUpdateCheck);
  PUTS("lang", lang);
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
  StateGuard lock;
  prefs.begin("owlanzi", false);
  prefs.clear();
  prefs.end();
  prefs.begin("owlanzi_dev", false);
  prefs.putBool("noseed", true);
  prefs.end();
}

bool cfgDevNoSeed() {
  StateGuard lock;
  prefs.begin("owlanzi_dev", true);
  bool v = prefs.getBool("noseed", false);
  prefs.end();
  return v;
}

// --- Language --------------------------------------------------------------
// English is the default so the device is understandable to a stranger
// without changing anything. Switchable in the web interface.
const char *L(const char *en, const char *de) {
  StateGuard lock;
  return gCfg.lang[0] == 'd' ? de : en;
}

// --- Freshness gate --------------------------------------------------------
/*
 * Require both a recent successful fetch and a recent cloud measurement.
 * Identical new measurements remain valid; fetching cached data does not
 * renew its age. After boot/charging/removal, wait for a measurement taken
 * at least five seconds after detecting the active session.
 */
bool cloudFresh() {
  StateGuard lock;
  return gSt.cloudOk && gSt.wifiOk && millis()-gSt.lastOkAt <= FETCH_MAX_MS;
}
bool cloudOffline() {
  StateGuard lock;
  // A single failed poll need not announce an outage. Allow reconnection
  // until 30 s after the last accepted fetch, showing dashes in the meantime.
  // No grace before the first result or after an account/device change.
  return !cloudFresh() && (!gSt.v.valid || millis()-gSt.lastOkAt >= OFFLINE_AFTER_MS);
}
bool vitalsFresh() {
  StateGuard lock;
  const Vitals &v=gSt.v;
  time_t now=time(nullptr);
  return cloudFresh() && v.valid && v.baseOn && v.sockConn != 0 &&
    !v.charging && !v.sockOff && !gSt.sessionPending &&
    now >= 1700000000 && v.measuredAt >= gSt.sessionAfter &&
    v.measuredAt <= (uint32_t)now && (uint32_t)now-v.measuredAt <= MEASUREMENT_MAX_SECONDS &&
    v.heart > 0 && v.oxygen > 0;
}

// --- Alarm evaluation ------------------------------------------------------
/*
 * Called only when something changed - after each poll and whenever the own
 * alarms are toggled. This used to run in the draw loop, 40 times a second,
 * building strings every time. On a device that runs for months that
 * fragments the heap.
 */
void alarmRecompute() {
  StateGuard lock;
  const Vitals &v = gSt.v;
  String l; bool crit = false;
  uint32_t mask = (v.lowHr?1u:0) | (v.highHr?2u:0) | (v.lowOx?4u:0) |
    (v.highOx?8u:0) | (v.sockDiscon?16u:0) | (v.lostPower?32u:0) |
    (v.criticalOx?64u:0) | (gSt.alSpo2?128u:0) |
    (gSt.alHrLow?256u:0) | (gSt.alHrHigh?512u:0);
  auto add = [&](const String &s, bool critical) {
    if (l.length()) l += "   +   ";
    l += s;
    if (critical) crit = true;
  };
  String hr = vitalsFresh() ? String((int)lroundf(v.heart)) : String("?");
  String ox = vitalsFresh() ? String((int)lroundf(v.oxygen)) : String("?");

  if (v.lowHr)      add(String(L("HEART RATE ","PULS ")) + hr + L(" TOO LOW"," ZU NIEDRIG"), true);
  if (v.highHr)     add(String(L("HEART RATE ","PULS ")) + hr + L(" TOO HIGH"," ZU HOCH"), true);
  if (v.lowOx)      add(String(L("OXYGEN ","SAUERSTOFF ")) + ox + L(" TOO LOW"," ZU NIEDRIG"), true);
  if (v.highOx)     add(String(L("OXYGEN ","SAUERSTOFF ")) + ox + L(" TOO HIGH"," ZU HOCH"), true);
  if (v.criticalOx) add(L("OWLET CRITICAL OXYGEN ALERT","OWLET KRITISCHER SAUERSTOFFALARM"), true);
  // The own alarms carry the word SUSTAINED / DAUERHAFT - that is how you
  // tell at a glance they come from your own rule and not from Owlet.
  if (gSt.alSpo2)   add(String(L("OXYGEN ","SAUERSTOFF ")) + ox + L(" SUSTAINED BELOW "," DAUERHAFT UNTER ") + gCfg.spo2Limit, true);
  if (gSt.alHrLow)  add(String(L("HEART RATE ","PULS ")) + hr + L(" SUSTAINED BELOW "," DAUERHAFT UNTER ") + gCfg.hrLowLimit, true);
  if (gSt.alHrHigh) add(String(L("HEART RATE ","PULS ")) + hr + L(" SUSTAINED ABOVE "," DAUERHAFT UEBER ") + gCfg.hrHighLimit, true);
  if (v.sockDiscon) add(L("SOCK DISCONNECTED","SOCKE GETRENNT"), true);
  if (v.lostPower)  add(L("BASE STATION LOST POWER","BASISSTATION OHNE STROM"), true);
  // A flat sock battery is a notice, not an emergency: amber, no sound, no
  // pulling the brightness up. At night this must not light up the room.
  if (cloudFresh()) {
    if (v.criticalBatt) add(L("SOCK BATTERY CRITICAL","SOCKE AKKU KRITISCH"), false);
    else if (v.lowBatt) add(L("SOCK BATTERY LOW","SOCKE AKKU LEER"), false);
  }
  if (crit && !cloudFresh()) add(L("OFFLINE - LAST KNOWN ALARM","OFFLINE - LETZTER ALARM"), false);

  gAlarmText = l;
  gAlarmCritical = crit;
  gSt.acknowledgedMask &= mask;
  if (mask & ~gSt.alarmMask & ~gSt.acknowledgedMask) gSt.soundPending=true;
  gSt.alarmMask=mask;
  gSt.silenced=mask && !(mask & ~gSt.acknowledgedMask);
  if (!mask) gSt.soundPending=false;
}

bool anyAlarm() { StateGuard lock; return (gSt.alarmMask & ~gSt.acknowledgedMask) != 0; }
void alarmAcknowledge() {
  StateGuard lock;
  gSt.acknowledgedMask=gSt.alarmMask;
  gSt.silenced=gSt.alarmMask != 0;
  gSt.soundPending=false;
}
void alarmsResetOwn() {
  StateGuard lock;
  gSt.spo2Since=gSt.hrLowSince=gSt.hrHighSince=0;
  gSt.alSpo2=gSt.alHrLow=gSt.alHrHigh=false;
  gSt.lastAlarmMeasurement=0;
}
static void evalOne(bool cond, uint32_t &since, bool &flag, int seconds, uint32_t measured) {
  if (!cond) { since=0; flag=false; return; }
  if (!since) since=measured;
  flag=measured-since >= (uint32_t)seconds;
}
void alarmsEvaluate(bool newSample) {
  StateGuard lock;
  if (!gCfg.ownAlarms || !vitalsFresh()) alarmsResetOwn();
  else if (newSample) {
    uint32_t stamp=gSt.v.measuredAt;
    if (gSt.lastAlarmMeasurement && (stamp < gSt.lastAlarmMeasurement ||
        stamp-gSt.lastAlarmMeasurement > (uint32_t)(gCfg.pollSeconds*2+5))) alarmsResetOwn();
    const Vitals &v=gSt.v;
    evalOne(v.oxygen<gCfg.spo2Limit,gSt.spo2Since,gSt.alSpo2,gCfg.spo2Seconds,stamp);
    evalOne(v.heart<gCfg.hrLowLimit,gSt.hrLowSince,gSt.alHrLow,gCfg.hrLowSeconds,stamp);
    evalOne(v.heart>gCfg.hrHighLimit,gSt.hrHighSince,gSt.alHrHigh,gCfg.hrHighSeconds,stamp);
    gSt.lastAlarmMeasurement=stamp;
  }
  alarmRecompute();
}
void stateTick() {
  StateGuard lock;
  // Only rebuild strings on a freshness transition, rather than every frame.
  static bool wasCloud=false, wasFresh=false;
  bool cloud=cloudFresh(), fresh=vitalsFresh();
  if (cloud!=wasCloud || fresh!=wasFresh) {
    wasCloud=cloud; wasFresh=fresh;
    alarmsEvaluate(false);
  }
}
void cloudInvalidate() {
  StateGuard lock;
  ++gSt.authGeneration;
  gSt.loggedIn=gSt.cloudOk=gSt.appActiveOk=false;
  gSt.dsn[0]=0; gSt.devices[0]=0; gSt.lastError[0]=0;
  gSt.v=Vitals{};
  gSt.sessionPending=true; gSt.sessionAfter=0;
  gSt.failCount=0;
  alarmsResetOwn(); alarmRecompute();
}
SleepState sleepState(int raw) {
  switch(raw) {case 1:return SLEEP_AWAKE; case 8:return SLEEP_LIGHT;
    case 15:return SLEEP_DEEP; default:return SLEEP_UNKNOWN;}
}
const char *sleepName(int raw, bool de) {
  switch(sleepState(raw)) {case SLEEP_AWAKE:return de?"wach":"awake";
    case SLEEP_LIGHT:return de?"leichter Schlaf":"light sleep";
    case SLEEP_DEEP:return de?"Tiefschlaf":"deep sleep";
    default:return de?"unbekannt":"unknown";}
}
bool cfgValid(const Config &c) {
  auto between=[](int v,int a,int b){return v>=a && v<=b;};
  if (strcmp(c.lang,"en") && strcmp(c.lang,"de")) return false;
  for(const char *p=c.owletDsn;*p;++p) if(!isalnum((unsigned char)*p) && *p!='-' && *p!='_') return false;
  return between(c.spo2Limit,50,99) && between(c.spo2Seconds,5,300) &&
    between(c.hrLowLimit,30,150) && between(c.hrHighLimit,100,260) && c.hrLowLimit<c.hrHighLimit &&
    between(c.hrLowSeconds,5,300) && between(c.hrHighSeconds,5,300) &&
    between(c.briMin,1,255) && between(c.briDay,1,255) && between(c.briAlarm,1,255) && between(c.briTest,1,255) &&
    between(c.ldrThreshold,0,1023) && between(c.ldrHysteresis,0,200) &&
    between(c.volAlarm,0,30) && between(c.alarmRepeatSec,0,120) &&
    (c.pollSeconds==5 || c.pollSeconds==10 || c.pollSeconds==15);
}
