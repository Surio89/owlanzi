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
#include <time.h>

#define OWLANZI_VERSION "1.0.10"
constexpr uint32_t FETCH_MAX_MS = 20000;
constexpr uint32_t OFFLINE_AFTER_MS = 30000;
constexpr uint32_t MEASUREMENT_MAX_SECONDS = 60;
inline bool timeReached(uint32_t now, uint32_t deadline) {
  return (int32_t)(now - deadline) >= 0;
}
enum SleepState : uint8_t { SLEEP_UNKNOWN=0, SLEEP_AWAKE=1, SLEEP_LIGHT=8, SLEEP_DEEP=15 };
SleepState sleepState(int raw);
const char *sleepName(int raw, bool german);

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
/*
 * Fully editable from the web interface, stored as one block in the NVS.
 *
 * These are the values from the Home Assistant original (script
 * awtrix_owlet_update), taken across unchanged. They are deliberately muted
 * so the picture still reads as dark below the lowest brightness step: the
 * brightness scales the colour channels, so a darker colour lowers the light
 * output on top of it. The one exception is the alarm text in full red - a
 * warning must not be dimmed along with everything else.
 *
 * Measured on 22 Aug at BRI 1, and the reason the greys look oddly bright
 * next to the reds: a grey pixel lights all three sub-LEDs, a red one only
 * one, so a saturated colour needs roughly twice the channel value of a grey
 * to appear equally bright. Heart invisible at red 60, visible at 110; SpO2
 * invisible at 59, visible at 96; the grey pulse still readable at 76 and
 * gone at 69. Below that, darker has to come from lighting fewer pixels, not
 * from a darker colour.
 */
struct Palette {
  uint32_t heart      = 0x5E1220;
  uint32_t numbers    = 0x3E434C;   // pulse and SpO2 share one grey; position
  uint32_t sep        = 0x1C1C1C;   // tells them apart, heart left, SpO2 right
  uint32_t heartWait  = 0x3E4650;
  uint32_t dashes     = 0x4A5260;
  uint32_t awake      = 0x7E5408;   // amber
  uint32_t lightSleep = 0x234E80;   // blue
  uint32_t deepSleep  = 0x5E2A8C;   // purple
  uint32_t sleepUnk   = 0x242424;
  uint32_t batFrame   = 0x3A424C;
  uint32_t batOk      = 0x48505C;
  uint32_t batCharge  = 0x1F6E3C;
  uint32_t batMid     = 0x70500E;
  uint32_t batLow     = 0x7E1E1E;
  uint32_t batText    = 0x5E6672;   // the percentage, a touch brighter than
                                    // the vitals numbers - as it was in HA
  uint32_t alarm      = 0xFF3030;
  uint32_t info       = 0xE3B341;   // no counterpart in HA, where every alarm
                                    // was red; owlanzi's own notice colour
  uint32_t offline    = 0x8E2A2A;
};

// --- Configuration ---------------------------------------------------------
struct Config {
  char  wifiSsid[33]   = "";
  char  wifiPass[65]   = "";
  char  owletMail[65]  = "";
  char  owletPass[65]  = "";
  char  webPass[33]    = "";    // empty = web interface without login
  char  owletDsn[24]   = "";    // required when more than one device is paired
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

  // 5, not 1: below step 5 the matrix can no longer show a colour honestly.
  // A grey pixel lights all three of its LEDs, a saturated one only a single
  // LED, so reds and blues fall away first while greys are still there, and
  // some pixels stop lighting at all. Lower is still allowed - a night that
  // has to be truly dark is a fair reason - but it is no longer the default,
  // and the interface says so.
  int   briMin         = 5;
  int   briDay         = 60;
  int   briAlarm       = 255;
  int   briTest        = 40;    // so previews stay visible at night too
  int   ldrThreshold   = 150;
  int   ldrHysteresis  = 25;    // against flicker around the threshold

  int   volAlarm       = 10;
  bool  soundEnabled   = true;
  int   alarmRepeatSec = 25;    // repeat the tone, 0 = once only
  char  lang[3]        = "en";  // "en" or "de"
  int   pollSeconds    = 5;
  bool  autoUpdateCheck = true; // daily metadata only; installation stays manual

  Palette pal;
};

extern Config gCfg;
bool cfgLoad();
void cfgSave();
void cfgFactoryReset();
bool cfgDevNoSeed();   // after a factory reset, do not seed again
bool cfgValid(const Config &cfg);
void cloudInvalidate(); // caller holds StateGuard; rejects in-flight old-account results

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
  bool  criticalOx=false, criticalBatt=false;
  uint32_t fetchedAt = 0;
  uint32_t measuredAt = 0; // UTC seconds from REAL_TIME_VITALS.data_updated_at
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
  uint32_t authGeneration = 0;
  bool     appActiveOk = false;
  char     devices[256] = ""; // paired serials for explicit selection
  Vitals   v;
  uint32_t sessionAfter = 0; // earliest UTC measurement allowed in this session
  bool sessionPending = true;
  uint32_t lastAlarmMeasurement = 0;

  bool     alSpo2 = false, alHrLow = false, alHrHigh = false;
  uint32_t spo2Since = 0, hrLowSince = 0, hrHighSince = 0;
  bool     silenced = false;
  uint32_t alarmMask = 0, acknowledgedMask = 0;
  bool soundPending = false;
  uint32_t lastAlarmSound = 0;

  uint8_t  testMode  = 0;
  uint32_t testUntil = 0;
  int testSleep = -1;

  ScreenId screen     = SCR_OFFLINE;
  bool     updateNotice = false; // update text currently replacing Battery only
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
bool cloudFresh();
bool cloudOffline();            // display grace only; never makes readings fresh
void alarmsEvaluate(bool newSample);
void alarmsResetOwn();
void alarmAcknowledge();
void stateTick();
const char *L(const char *en, const char *de);   // language pick

// Vitals are touched from two cores and need a mutex.
void stateLock();
void stateUnlock();
void stateBegin(); // before starting the second task
struct StateGuard {
  StateGuard() { stateLock(); }
  ~StateGuard() { stateUnlock(); }
  StateGuard(const StateGuard&) = delete;
  StateGuard& operator=(const StateGuard&) = delete;
};
