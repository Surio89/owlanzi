/*
 * owlanzi.h - gemeinsame Typen, Pins und Zustand
 *
 * Eigene Firmware fuer die Ulanzi TC001, die die Owlet-Cloud direkt abfragt.
 * Kein Home Assistant, kein MQTT, kein Broker.
 *
 * ERGAENZT die Owlet-Basisstation, ERSETZT sie nicht.
 */
#pragma once
#include <Arduino.h>

// --- Hardware der Ulanzi TC001 --------------------------------------------
// Quelle: https://github.com/rroels/ulanzi_tc001_hardware
// Pinbelegungen sind Tatsachen ueber die Platine und damit frei verwendbar.
// Aus dem AWTRIX-Quellcode stammt hier bewusst keine Zeile.
#define PIN_MATRIX      32
#define PIN_BUZZER      15
#define PIN_BTN_LEFT    26
#define PIN_BTN_MID     27
#define PIN_BTN_RIGHT   14
#define PIN_LDR         35
#define PIN_BATTERY     34
#define MATRIX_W        32
#define MATRIX_H         8
#define NUM_LEDS        (MATRIX_W * MATRIX_H)

// --- Farbpalette -----------------------------------------------------------
// Komplett im Webinterface einstellbar, als ein Block im NVS abgelegt.
struct Palette {
  uint32_t heart      = 0xC81E3C;
  uint32_t numbers    = 0x96A0B2;
  uint32_t sep        = 0x191919;
  uint32_t heartWait  = 0x3C424E;
  uint32_t dashes     = 0x3C424E;
  uint32_t awake      = 0xBE8214;
  uint32_t lightSleep = 0x286EBE;
  uint32_t deepSleep  = 0x823CC8;
  uint32_t sleepUnk   = 0x282828;
  uint32_t batFrame   = 0x5A6473;
  uint32_t batOk      = 0x5A6473;
  uint32_t batCharge  = 0x28AA5A;
  uint32_t batMid     = 0xAA8214;
  uint32_t batLow     = 0xB43232;
  uint32_t alarm      = 0xFF2828;
  uint32_t info       = 0xE3B341;
  uint32_t offline    = 0x962828;
};

// --- Konfiguration ---------------------------------------------------------
struct Config {
  char  wifiSsid[33]   = "";
  char  wifiPass[65]   = "";
  char  owletMail[65]  = "";
  char  owletPass[65]  = "";
  char  webPass[33]    = "";    // leer = Webinterface ohne Anmeldung
  bool  europe         = true;

  // Eigene Alarme: ab Werk AUS. Eine Sauerstoffgrenze ist eine medizinische
  // Einschaetzung, die niemand geschenkt bekommen soll.
  bool  ownAlarms      = false;
  int   spo2Limit      = 86;
  int   spo2Seconds    = 15;
  int   hrLowLimit     = 80;
  int   hrLowSeconds   = 15;
  int   hrHighLimit    = 200;
  int   hrHighSeconds  = 15;

  int   briMin         = 1;
  int   briDay         = 60;
  int   briAlarm       = 255;
  int   briTest        = 40;    // damit Tests auch nachts sichtbar sind
  int   ldrThreshold   = 150;
  int   ldrHysteresis  = 25;    // gegen Flackern an der Schwelle

  int   volAlarm       = 10;
  bool  soundEnabled   = true;
  int   alarmRepeatSec = 25;    // Ton wiederholen, 0 = nur einmal
  bool  serpentine     = true;
  char  lang[3]        = "en";  // "en" oder "de"
  int   pollSeconds    = 5;

  Palette pal;
};

extern Config gCfg;
bool cfgLoad();
void cfgSave();
void cfgFactoryReset();
bool cfgDevNoSeed();   // nach einem Werksreset nicht wieder vorbefuellen

// --- Vitalwerte aus REAL_TIME_VITALS --------------------------------------
struct Vitals {
  bool  valid    = false;
  float oxygen   = 0;
  float heart    = 0;
  float battery  = 0;
  float oxygen10 = 0;
  bool  baseOn   = false;
  int   sleepSt  = 0;
  int   charging = 0;
  int   sockConn = 0;
  int   movement = 0;
  char  hardware[8] = "";
  bool  lowOx=false, highOx=false, lowHr=false, highHr=false;
  bool  lostPower=false, sockDiscon=false, sockOff=false;
  bool  lowBatt=false;
  uint32_t fetchedAt = 0;
};

// Welcher Schirm gerade dran ist - auch fuers Webinterface, damit der
// Sollzustand dort ablesbar ist statt geraten werden zu muessen.
enum ScreenId : uint8_t {
  SCR_ALARM = 0, SCR_VITALS, SCR_BATTERY, SCR_WAITING, SCR_OFFLINE,
  SCR_SETUP, SCR_TEST, SCR_MESSAGE
};

struct State {
  bool     wifiOk       = false;
  bool     apMode       = false;
  bool     loggedIn     = false;
  bool     cloudOk      = false;
  uint32_t lastOkAt     = 0;
  uint32_t lastTryAt    = 0;
  uint32_t failCount    = 0;
  uint32_t pollCount    = 0;
  char     dsn[24]      = "";
  char     lastError[96]= "";
  Vitals   v;
  // Frischepruefung. Die Owlet-Cloud liefert beim Abnehmen vom Ladegeraet
  // sofort den letzten Messwert der VORIGEN Sitzung aus - fuer uns sieht der
  // taufrisch aus. Deshalb zaehlt nicht, wann wir abgerufen haben, sondern
  // wann sich der Wert zuletzt geaendert hat, und ob das nach dem Ende des
  // Ladevorgangs war.
  uint32_t vitalsChangedAt = 0;
  uint32_t chargeEndedAt   = 0;
  float    lastHr = -1, lastOx = -1;

  bool     alSpo2 = false, alHrLow = false, alHrHigh = false;
  uint32_t spo2Since = 0, hrLowSince = 0, hrHighSince = 0;
  bool     silenced = false;
  uint32_t lastAlarmSound = 0;

  uint8_t  testMode  = 0;
  uint32_t testUntil = 0;

  ScreenId screen     = SCR_OFFLINE;
  uint8_t  brightness = 1;
  int      ldrRaw     = 0;
  bool     ambientBright = false;
};

extern State gSt;

// Zwischengespeicherter Alarmtext: wird nur bei Zustandsaenderung neu
// gebaut, nicht 40 Mal je Sekunde im Zeichentakt. Das schont den Heap.
extern String gAlarmText;
extern bool   gAlarmCritical;    // true = Ton und volle Helligkeit
void alarmRecompute();
bool anyAlarm();                 // kritisch und nicht quittiert
bool vitalsFresh();              // duerfen die Werte gezeigt werden?
const char *L(const char *en, const char *de);   // Sprachwahl

// Vitalwerte werden von zwei Kernen angefasst und brauchen Wechselschutz.
void stateLock();
void stateUnlock();
