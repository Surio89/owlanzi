/*
 * owlanzi.h - shared types, pins and state
 *
 * Custom firmware for the Ulanzi TC001 that talks to the Owlet cloud
 * directly. No Home Assistant, no MQTT, no broker.
 *
 * It COMPLEMENTS the Owlet base station, it does not REPLACE it.
 */
#pragma once
#include <Arduino.h>

// --- Ulanzi TC001 hardware -------------------------------------------------
// Source: https://github.com/rroels/ulanzi_tc001_hardware
// Pin assignments are facts about the board and free to use. Not one line
// here comes from the AWTRIX source.
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

// --- Colour palette --------------------------------------------------------
// Fully editable from the web interface, stored as one block in the NVS.
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

// --- Configuration ---------------------------------------------------------
struct Config {
  char  wifiSsid[33]   = "";
  char  wifiPass[65]   = "";
  char  owletMail[65]  = "";
  char  owletPass[65]  = "";
  char  webPass[33]    = "";    // empty = web interface without login
  bool  europe         = true;

  // Own alarms: OFF by default. An oxygen threshold is a medical judgement
  // and nobody should be handed one they did not choose.
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
  int   briTest        = 40;    // so previews stay visible at night too
  int   ldrThreshold   = 150;
  int   ldrHysteresis  = 25;    // against flicker around the threshold

  int   volAlarm       = 10;
  bool  soundEnabled   = true;
  int   alarmRepeatSec = 25;    // repeat the tone, 0 = once only
  bool  serpentine     = true;
  char  lang[3]        = "en";  // "en" or "de"
  int   pollSeconds    = 5;

  Palette pal;
};

extern Config gCfg;
bool cfgLoad();
void cfgSave();
void cfgFactoryReset();
bool cfgDevNoSeed();   // after a factory reset, do not seed again

// --- Vitals from REAL_TIME_VITALS -----------------------------------------
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

// Which screen is currently up - exposed to the web interface as well, so
// the intended state can be read off there instead of guessed.
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
  // Freshness gate. When the sock comes off the charger, the Owlet cloud
  // immediately serves the last reading of the PREVIOUS session - which
  // looks brand new to us. So what counts is not when we fetched, but when
  // the value last changed, and whether that was after charging ended.
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

// Cached alarm text: rebuilt only when the state changes, not 40 times a
// second in the draw loop. That keeps the heap quiet.
extern String gAlarmText;
extern bool   gAlarmCritical;    // true = sound and full brightness
void alarmRecompute();
bool anyAlarm();                 // critical and not acknowledged
bool vitalsFresh();              // may the values be shown?
const char *L(const char *en, const char *de);   // language pick

// Vitals are touched from two cores and need a mutex.
void stateLock();
void stateUnlock();
