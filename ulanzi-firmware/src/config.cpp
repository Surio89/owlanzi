/*
 * config.cpp - Einstellungen im NVS, Alarmauswertung
 *
 * Zugangsdaten gehoeren NICHT in den Quelltext. Sie werden im Webinterface
 * eingegeben und liegen im NVS des Geraets. Damit laesst sich dieselbe
 * Firmware weitergeben, ohne dass jemand seine Owlet-Daten in eine .cpp
 * tippen und neu kompilieren muss.
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
  // Die Palette liegt als ein Block. Passt die Groesse nicht (aeltere
  // Firmware, neue Farbe dazugekommen), bleiben die Vorgaben stehen.
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
 * Werksreset. Wichtig dabei: die Entwickler-Vorbefuellung aus
 * secrets_local.h darf danach NICHT wieder greifen - sonst startet das
 * Geraet neu, fuellt sich sofort selbst wieder mit denselben Zugangsdaten
 * und fuer den Benutzer sieht es aus, als waere gar nichts passiert.
 * Genau das ist am 4.9. passiert. Der Merker liegt in einem eigenen
 * Namensraum, den der Reset bewusst stehen laesst.
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

// --- Sprache ---------------------------------------------------------------
// Vorgabe ist Englisch, damit das Geraet fuer Fremde ohne Umstellung
// verstaendlich ist. Umschaltbar im Webinterface.
const char *L(const char *en, const char *de) {
  return gCfg.lang[0] == 'd' ? de : en;
}

// --- Frischepruefung -------------------------------------------------------
/*
 * Es darf NIE ein alter Wert auf dem Schirm stehen. Zwei Bedingungen, genau
 * wie in der Home-Assistant-Fassung, und aus demselben Grund:
 *   a) der Wert hat sich in den letzten 60 s ueberhaupt geaendert, und
 *   b) diese Aenderung kam mindestens 5 s nach dem Ende des Ladevorgangs.
 * Ohne (b) zeigt das Geraet direkt nach dem Abnehmen vom Ladegeraet den
 * letzten Messwert der VORIGEN Nacht an - die Owlet-Cloud liefert den
 * naemlich sofort aus, und fuer uns sieht er taufrisch aus.
 */
bool vitalsFresh() {
  if (!gSt.cloudOk) return false;
  uint32_t now = millis();
  if (now - gSt.lastOkAt > 20000UL) return false;          // Abruf haengt
  if (!gSt.vitalsChangedAt) return false;                  // noch nie etwas
  if (now - gSt.vitalsChangedAt > 60000UL) return false;   // steht still
  if (gSt.chargeEndedAt &&
      (int32_t)(gSt.vitalsChangedAt - gSt.chargeEndedAt) < 5000) return false;
  return true;
}

// --- Alarmauswertung -------------------------------------------------------
/*
 * Wird nur aufgerufen, wenn sich etwas geaendert hat - nach jedem Abruf und
 * bei jedem Wechsel der eigenen Alarme. Frueher lief das im Zeichentakt,
 * also 40 Mal je Sekunde, und hat dabei staendig Strings gebaut. Auf einem
 * Geraet, das monatelang durchlaeuft, fragmentiert das den Heap.
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
  // Die eigenen Alarme tragen das Wort DAUERHAFT - daran erkennt man auf
  // einen Blick, dass sie aus der eigenen Regel kommen und nicht von Owlet.
  if (gSt.alSpo2)   add(String(L("OXYGEN ","SAUERSTOFF ")) + ox + L(" SUSTAINED BELOW "," DAUERHAFT UNTER ") + gCfg.spo2Limit, true);
  if (gSt.alHrLow)  add(String(L("HEART RATE ","PULS ")) + hr + L(" SUSTAINED BELOW "," DAUERHAFT UNTER ") + gCfg.hrLowLimit, true);
  if (gSt.alHrHigh) add(String(L("HEART RATE ","PULS ")) + hr + L(" SUSTAINED ABOVE "," DAUERHAFT UEBER ") + gCfg.hrHighLimit, true);
  if (v.sockDiscon) add(L("SOCK DISCONNECTED","SOCKE GETRENNT"), true);
  if (v.lostPower)  add(L("BASE STATION LOST POWER","BASISSTATION OHNE STROM"), true);
  // Akku leer ist ein Hinweis, kein Notfall: erscheint in Bernstein, ohne
  // Ton und ohne die Helligkeit hochzureissen. Nachts soll das nicht das
  // Zimmer erhellen.
  if (v.lowBatt)    add(L("SOCK BATTERY LOW","SOCKE AKKU LEER"), false);

  gAlarmText = l;
  gAlarmCritical = crit;
  if (!crit) gSt.silenced = false;   // Quittierung gilt nur fuer den Alarm
}

bool anyAlarm() { return gAlarmCritical && !gSt.silenced; }
