/*
 * display.cpp - 8x32 matrix, own 3x5 font, the screens
 *
 * The TC001 wires its pixels in a serpentine: the panel is one long strip
 * folded row by row, so even rows run left to right and odd rows right to
 * left, starting top left.
 * Source: github.com/rroels/ulanzi_tc001_hardware
 *
 * This used to be a setting. It is not one any more: this firmware is for
 * the TC001, where the answer is always yes, and a setting with one correct
 * value is only something to get wrong.
 */
#include <FastLED.h>
#include "display.h"
#include "online_update.h"

static CRGB leds[NUM_LEDS];
static uint8_t gBri = 1;

// --- 3x5 font, ASCII 32..90 -----------------------------------------------
// One byte per column, bit 0 = top row. Three columns plus one pixel of
// spacing make an advance of 4 - eight characters fit side by side.
static const uint8_t FONT[59][3] PROGMEM = {
  {0x00,0x00,0x00}, {0x00,0x17,0x00}, {0x03,0x00,0x03}, {0x0A,0x1F,0x0A},
  {0x16,0x1F,0x0D}, {0x19,0x04,0x13}, {0x1F,0x15,0x1A}, {0x00,0x03,0x00},
  {0x00,0x0E,0x11}, {0x11,0x0E,0x00}, {0x0A,0x04,0x0A}, {0x04,0x0E,0x04},
  {0x00,0x18,0x00}, {0x04,0x04,0x04}, {0x00,0x10,0x00}, {0x18,0x04,0x03},
  {0x1F,0x11,0x1F}, {0x12,0x1F,0x10}, {0x1D,0x15,0x17}, {0x11,0x15,0x1F},
  {0x07,0x04,0x1F}, {0x17,0x15,0x1D}, {0x1F,0x15,0x1D}, {0x01,0x01,0x1F},
  {0x1F,0x15,0x1F}, {0x17,0x15,0x1F}, {0x00,0x0A,0x00}, {0x00,0x1A,0x00},
  {0x04,0x0A,0x11}, {0x0A,0x0A,0x0A}, {0x11,0x0A,0x04}, {0x01,0x15,0x07},
  {0x1F,0x11,0x17}, {0x1F,0x05,0x1F}, {0x1F,0x15,0x1B}, {0x1F,0x11,0x11},
  {0x1F,0x11,0x0E}, {0x1F,0x15,0x15}, {0x1F,0x05,0x05}, {0x1F,0x11,0x1D},
  {0x1F,0x04,0x1F}, {0x11,0x1F,0x11}, {0x18,0x10,0x1F}, {0x1F,0x04,0x1B},
  {0x1F,0x10,0x10}, {0x1F,0x02,0x1F}, {0x1F,0x0C,0x1F}, {0x1F,0x11,0x1F},
  {0x1F,0x05,0x07}, {0x0F,0x09,0x1F}, {0x1F,0x05,0x1B}, {0x17,0x15,0x1D},
  {0x01,0x1F,0x01}, {0x1F,0x10,0x1F}, {0x0F,0x10,0x0F}, {0x1F,0x08,0x1F},
  {0x1B,0x04,0x1B}, {0x03,0x1C,0x03}, {0x19,0x15,0x13}
};

// --- Basics ----------------------------------------------------------------
static inline CRGB C(uint32_t rgb) { return CRGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF); }

static inline uint16_t xy(int x, int y) {
  if (x < 0 || x >= MATRIX_W || y < 0 || y >= MATRIX_H) return NUM_LEDS;
  if (y & 1) return y * MATRIX_W + (MATRIX_W - 1 - x);
  return y * MATRIX_W + x;
}
static inline void px(int x, int y, const CRGB &c) {
  uint16_t i = xy(x, y);
  if (i < NUM_LEDS) leds[i] = c;
}
static void fillRect(int x, int y, int w, int h, const CRGB &c) {
  for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) px(x + i, y + j, c);
}
static void clear() { fill_solid(leds, NUM_LEDS, CRGB::Black); }

static int drawChar(int x, char ch, const CRGB &c) {
  if (x > MATRIX_W || x < -4) return x + 4;      // off screen, draw nothing
  if (ch >= 'a' && ch <= 'z') ch -= 32;
  int idx = (ch < 32 || ch > 90) ? 0 : ch - 32;
  for (int col = 0; col < 3; col++) {
    uint8_t bits = pgm_read_byte(&FONT[idx][col]);
    for (int row = 0; row < 5; row++)
      if (bits & (1 << row)) px(x + col, 1 + row, c);
  }
  return x + 4;
}
static int textWidth(const String &s) { return s.length() * 4 - 1; }
static void drawText(int x, const String &s, const CRGB &c) {
  for (size_t i = 0; i < s.length(); i++) x = drawChar(x, s[i], c);
}
static void drawNumRight(int rightX, int value, const CRGB &c) {
  String s = String(value);
  drawText(rightX - textWidth(s) + 1, s, c);
}
static void drawHeart(const CRGB &c) {
  px(1, 1, c); px(3, 1, c);
  for (int x = 0; x <= 4; x++) { px(x, 2, c); px(x, 3, c); }
  for (int x = 1; x <= 3; x++) px(x, 4, c);
  px(2, 5, c);
}

// --- The individual screens ------------------------------------------------
static void screenVitals(const Vitals &v) {
  const Palette &p = gCfg.pal;
  drawHeart(C(p.heart));
  drawNumRight(17, (int)lroundf(v.heart), C(p.numbers));
  // x=19, one to the right of centre: the pulse ends at 17, the oxygen block
  // starts at 25, and sitting a pixel clear of the number reads better than
  // hanging off its last column.
  px(19, 3, C(p.sep));
  drawNumRight(31, (int)lroundf(v.oxygen), C(p.numbers));
  // Sleep bar along the bottom: the deeper the sleep, the shorter and cooler.
  // CAREFUL: the numeric values of "ss" are undocumented. Until they have
  // been checked against the Owlet app, anything unknown deliberately falls
  // back to the grey bar - honestly unknown beats confidently wrong.
  int x = 15, w = 2; CRGB c = C(p.sleepUnk);
  switch (sleepState(v.sleepSt)) {
    case SLEEP_AWAKE: x = 7;  w = 18; c = C(p.awake);      break;
    case SLEEP_LIGHT: x = 11; w = 10; c = C(p.lightSleep); break;
    case SLEEP_DEEP:  x = 14; w = 4;  c = C(p.deepSleep);  break;
    default: break;
  }
  fillRect(x, 7, w, 1, c);
}

static void screenBattery(int pct, bool charging) {
  const Palette &p = gCfg.pal;
  CRGB fr = C(p.batFrame);
  fillRect(0, 1, 10, 1, fr); fillRect(0, 5, 10, 1, fr);
  fillRect(0, 2, 1, 3, fr);  fillRect(9, 2, 1, 3, fr);
  fillRect(10, 2, 1, 3, fr);
  int w = constrain((pct * 8) / 100, 1, 8);
  CRGB c = charging ? C(p.batCharge)
         : (pct < 20 ? C(p.batLow) : (pct < 40 ? C(p.batMid) : C(p.batOk)));
  fillRect(1, 2, w, 3, c);
  drawNumRight(28, pct, C(p.batText));
  drawText(29, "%", C(p.batFrame));
}

static void screenWaiting() {
  const Palette &p = gCfg.pal;
  drawHeart(C(p.heartWait));
  drawText(10, "--", C(p.dashes));
  px(19, 3, C(p.sep));
  drawText(25, "--", C(p.dashes));
  fillRect(15, 7, 2, 1, C(p.sleepUnk));
}

static void screenOffline() { drawText(2, "OFFLINE", C(gCfg.pal.offline)); }
static void screenSetup()   { drawText(1, "SETUP", C(gCfg.pal.info)); }

static int gScrollX = MATRIX_W;
static uint32_t gScrollAt = 0;
static void screenAlarm(const String &txt, bool critical) {
  CRGB c = C(critical ? gCfg.pal.alarm : gCfg.pal.info);
  // The advance sits HERE on purpose and not in dispTick: otherwise the text
  // only scrolls during normal operation, and the Alarm and Notice preview
  // buttons show an empty matrix because the text stays parked off at x=32.
  if (millis() - gScrollAt > 60) {
    gScrollAt = millis();
    if (--gScrollX < -textWidth(txt)) gScrollX = MATRIX_W;
  }
  drawText(gScrollX, txt, c);
  // The three corner pixels blink - urgency without making the text flicker.
  // Critical alarms only; a notice does not blink.
  if (critical && ((millis() / 500) & 1)) {
    px(0, 0, c); px(15, 0, c); px(31, 0, c);
  }
}

// --- Test patterns ---------------------------------------------------------
static void screenTest(uint8_t mode) {
  uint32_t t = millis();
  const Palette &p = gCfg.pal;
  switch (mode) {
    case TEST_CORNERS:
      px(0, 0, CRGB::Red);
      px(MATRIX_W - 1, 0, CRGB::Green);
      px(0, MATRIX_H - 1, CRGB::Blue);
      px(MATRIX_W - 1, MATRIX_H - 1, CRGB::White);
      drawText(5, L("EDGES","ECKEN"), C(p.numbers));
      break;
    case TEST_SWEEP: { int i = (t / 25) % NUM_LEDS; leds[i] = CRGB::White; break; }
    case TEST_COLORS: {
      int q = (t / 1000) % 4;
      fill_solid(leds, NUM_LEDS, q == 0 ? CRGB(255,0,0) : q == 1 ? CRGB(0,255,0)
                                : q == 2 ? CRGB(0,0,255) : CRGB(255,255,255));
      break;
    }
    case TEST_VITALS: {
      Vitals d; d.heart = 132; d.oxygen = 97;
      d.sleepSt = gSt.testSleep>=0 ? gSt.testSleep :
        ((t / 3000) % 3 == 0) ? SLEEP_AWAKE : ((t / 3000) % 3 == 1) ? SLEEP_LIGHT : SLEEP_DEEP;
      screenVitals(d); break;
    }
    case TEST_BATTERY: screenBattery(64, ((t / 3000) & 1) != 0); break;
    case TEST_WAITING: screenWaiting(); break;
    case TEST_OFFLINE: screenOffline(); break;
    case TEST_ALARM:   screenAlarm(L("TEST ALARM OXYGEN 84 SUSTAINED BELOW 86","TESTALARM SAUERSTOFF 84 DAUERHAFT UNTER 86"), true); break;
    case TEST_INFO:    screenAlarm(L("SOCK BATTERY LOW","SOCKE AKKU LEER"), false); break;
  }
}

// --- Brightness ------------------------------------------------------------
/*
 * Order of precedence, unchanged from the Home Assistant version:
 *   alarm                            -> full, beats everything
 *   sock active AND room is bright   -> daylight value
 *   otherwise                        -> minimum
 *
 * The light sensor is averaged and read with hysteresis. Without that, the
 * brightness jumps between minimum and daylight several times a second
 * around the threshold - visible flicker next to a cot.
 */
static uint16_t gLdrAvg = 0;
static bool     gBrightAmbient = false;

static void ldrTick() {
  static uint32_t last = 0;
  if (millis() - last < 200) return;             // 5 readings per second
  last = millis();
  uint16_t raw = analogRead(PIN_LDR) >> 2;       // 12 bit -> 0..1023
  gLdrAvg = gLdrAvg ? (uint16_t)((gLdrAvg * 7 + raw) / 8) : raw;
  int hi = gCfg.ldrThreshold + gCfg.ldrHysteresis;
  int lo = gCfg.ldrThreshold - gCfg.ldrHysteresis;
  if (!gBrightAmbient && gLdrAvg > hi) gBrightAmbient = true;
  else if (gBrightAmbient && gLdrAvg < lo) gBrightAmbient = false;
  gSt.ldrRaw = gLdrAvg;
  gSt.ambientBright = gBrightAmbient;
}

// True while a scrolling message is on screen. Defined further down, next to
// the message state it reads.
static bool msgShowing();

/*
 * Setup and the boot messages are exempt from all of the above. Both are
 * transient, both are meant to be read from across the room, and both are
 * useless when dim: an IP address you cannot make out is no better than no
 * IP address, and the hotspot screen exists precisely to be noticed. They
 * ignore the light sensor and the configured levels and go to full.
 */
static const uint8_t BRI_FULL = 255;

static uint8_t targetBrightness() {
  // Same order as the branches in dispTick(), so the brightness always
  // belongs to the picture that is actually being drawn.
  if (gAlarmCritical) return constrain(gCfg.briAlarm, 1, 255);
  if (gSt.testMode) return constrain(gCfg.briTest, 1, 255);
  if (msgShowing()) return BRI_FULL;
  if (gSt.apMode)   return BRI_FULL;
  bool sockActive = vitalsFresh();
  if (sockActive && gBrightAmbient) return constrain(gCfg.briDay, 1, 255);
  return constrain(gCfg.briMin, 1, 255);
}

uint8_t dispBrightness() { return gBri; }

void dispBegin() {
  FastLED.addLeds<WS2812B, PIN_MATRIX, GRB>(leds, NUM_LEDS);
  FastLED.setCorrection(TypicalLEDStrip);
  gBri = constrain(gCfg.briMin, 1, 255);
  FastLED.setBrightness(gBri);
  clear(); FastLED.show();
}

static bool    gPrevOn      = false;
static Palette gPrevPal;
static int     gPrevBriTest = 0;

void previewBegin() {
  StateGuard lock;
  if (gPrevOn) return;
  gPrevPal = gCfg.pal; gPrevBriTest = gCfg.briTest;
  gPrevOn = true;
}
void previewEnd(bool restore) {
  StateGuard lock;
  if (!gPrevOn) return;
  gPrevOn = false;
  if (restore) { gCfg.pal = gPrevPal; gCfg.briTest = gPrevBriTest; }
}
void previewSavedConfig(Config &config) {
  StateGuard lock;
  if(gPrevOn){config.pal=gPrevPal;config.briTest=gPrevBriTest;}
}

// A message that scrolls for a while and then disappears by itself. At boot
// the device shows its IP address this way - otherwise you would have to
// hunt for it in the router just to reach the interface at all.
static String   gMsg;
static uint32_t gMsgUntil = 0;
static uint32_t gMsgCol   = 0xFFFFFF;
static bool gMsgActive = false;
static bool msgShowing() { return gMsgActive && !timeReached(millis(),gMsgUntil); }
void dispMessage(const String &txt, uint32_t seconds, uint32_t rgb) {
  StateGuard lock;
  gMsg = txt; gMsgCol = rgb;
  gMsgUntil = millis() + constrain(seconds,0u,600u) * 1000UL;
  gMsgActive=seconds>0;
  gScrollX = MATRIX_W;
}

void dispTest(uint8_t mode, uint32_t seconds, int sleep) {
  StateGuard lock;
  if(gAlarmCritical || mode>TEST_INFO)mode=TEST_OFF;
  gSt.testMode = mode;
  gSt.testSleep=sleep<0?-1:(int)sleepState(sleep);
  gSt.testUntil = mode ? millis() + constrain(seconds,1u,300u) * 1000UL : 0;
  if (mode) gScrollX = MATRIX_W;
}

const char *screenName(ScreenId s) {
  switch (s) {
    case SCR_ALARM:   return "Alarm";
    case SCR_VITALS:  return "Vitals";
    case SCR_BATTERY: return "Battery";
    case SCR_WAITING: return "Waiting for values";
    case SCR_OFFLINE: return "Offline";
    case SCR_SETUP:   return "Setup";
    default:          return "Test";
  }
}

// --- Main display loop -----------------------------------------------------
static char gUpdateNoticeVersion[24]="";
static uint32_t gUpdateNoticeRemaining=0,gUpdateNoticeAt=0;
static int gUpdateNoticeScrollX=MATRIX_W;
static uint32_t gUpdateNoticeScrollAt=0;
void dispUpdateAvailable(const char *version) {
  StateGuard lock;
  if(!version || strlen(version)>=sizeof(gUpdateNoticeVersion))return;
  if(!strcmp(gUpdateNoticeVersion,version))return;
  strlcpy(gUpdateNoticeVersion,version,sizeof(gUpdateNoticeVersion));
  gUpdateNoticeRemaining=*version?20000:0;
  gSt.updateNotice=false;
}
static void updateNoticeTick(uint32_t now) {
  // Charge only time the previous frame actually displayed the notice. This
  // subtraction also works across millis() rollover, with no sleeping/delays.
  if(gSt.updateNotice)gUpdateNoticeRemaining-=std::min(gUpdateNoticeRemaining,now-gUpdateNoticeAt);
  bool wasShowing=gSt.updateNotice;
  gUpdateNoticeAt=now;
  gSt.updateNotice=gSt.screen==SCR_BATTERY && !updateBusy() && gUpdateNoticeRemaining>0;
  if(!gSt.updateNotice)return;
  if(!wasShowing){gUpdateNoticeScrollX=MATRIX_W;gUpdateNoticeScrollAt=now;}
  const String text=L("UPDATE AVAILABLE","UPDATE VERFUEGBAR");
  if(now-gUpdateNoticeScrollAt>=60) {
    gUpdateNoticeScrollAt=now;
    if(--gUpdateNoticeScrollX < -textWidth(text))gUpdateNoticeScrollX=MATRIX_W;
  }
  clear();drawText(gUpdateNoticeScrollX,text,C(gCfg.pal.info));
}
void dispTick() {
  static uint32_t last = 0;
  // 30 frames a second are enough for the scroller and leave Wi-Fi room.
  if (millis() - last < 33) return;
  last = millis();
  {
  StateGuard lock;
  ldrTick();
  if(gAlarmCritical || !gSt.testMode || timeReached(millis(),gSt.testUntil))previewEnd(true);

  if (gSt.testMode && timeReached(millis(),gSt.testUntil)) gSt.testMode = TEST_OFF;
  if(gMsgActive && timeReached(millis(),gMsgUntil))gMsgActive=false;
  clear();

  if(gAlarmCritical) {
    gSt.testMode=TEST_OFF; gMsgActive=false;
    gSt.screen=SCR_ALARM; screenAlarm(gAlarmText,true);
  } else if (gSt.testMode) {
    gSt.screen = SCR_TEST;
    screenTest(gSt.testMode);
  } else if (msgShowing()) {
    gSt.screen = SCR_MESSAGE;
    if (millis() - gScrollAt > 60) {
      gScrollAt = millis();
      if (--gScrollX < -textWidth(gMsg)) gScrollX = MATRIX_W;
    }
    drawText(gScrollX, gMsg, C(gMsgCol));
  } else if (gSt.apMode) {
    gSt.screen = SCR_SETUP;
    screenSetup();
  } else {
    String  al   = gAlarmText;
    bool    crit = gAlarmCritical;
    Vitals  v    = gSt.v;

    if (al.length() && cloudFresh()) {
      gSt.screen = SCR_ALARM;
      screenAlarm(al, crit);
    } else {
      gScrollX = MATRIX_W;
      // Delay the OFFLINE label only. Failed/stale fetches still hide values
      // and battery status immediately while the network worker retries.
      if (cloudOffline()) {
        gSt.screen = SCR_OFFLINE; screenOffline();
      } else if (!cloudFresh()) {
        gSt.screen = SCR_WAITING; screenWaiting();
      } else if (v.charging != 0 || v.sockOff) {
        gSt.screen = SCR_BATTERY; screenBattery((int)lroundf(v.battery), v.charging != 0);
      } else if (vitalsFresh() && v.heart > 0 && v.oxygen > 0) {
        // Heart rate and oxygen both 0 means the base station is switched
        // off, not that the child has no pulse. Show dashes instead.
        gSt.screen = SCR_VITALS; screenVitals(v);
      } else {
        gSt.screen = SCR_WAITING; screenWaiting();
      }
    }
  }

  updateNoticeTick(millis());
  uint8_t t = targetBrightness();
  if (t != gBri) { gBri = t; FastLED.setBrightness(gBri); }
  gSt.brightness = gBri;
  }
  FastLED.show();
}

// --- Live mirror for the browser -------------------------------------------
// Fixed buffer, no heap: this is fetched several times a second.
static char gHex[NUM_LEDS * 6 + 8];
const char *dispFrameHex() {
  StateGuard lock;
  static const char *H = "0123456789abcdef";
  char *o = gHex;
  for (int y = 0; y < MATRIX_H; y++)
    for (int x = 0; x < MATRIX_W; x++) {
      uint16_t i = xy(x, y);
      CRGB c = (i < NUM_LEDS) ? leds[i] : CRGB::Black;
      *o++ = H[c.r >> 4]; *o++ = H[c.r & 15];
      *o++ = H[c.g >> 4]; *o++ = H[c.g & 15];
      *o++ = H[c.b >> 4]; *o++ = H[c.b & 15];
    }
  // Send the brightness along: the mirror in the browser used to take it
  // from the status report, which only arrives every 1.5 s - so the picture
  // flashed up bright and then dimmed down afterwards.
  *o++ = H[gBri >> 4]; *o++ = H[gBri & 15];
  *o = 0;
  return gHex;
}

// The font for the colour preview in the browser. The preview redraws the
// screens in JavaScript; the glyphs come from here so the type there cannot
// drift away from the matrix. The LAYOUT of the screens, on the other hand,
// exists twice - once above in screenVitals & co. and once as a twin in
// webui.cpp. Move something here and you have to follow suit there.
static char gFontHex[sizeof(FONT) * 2 + 1];
const char *dispFontHex() {
  static const char *H = "0123456789abcdef";
  char *o = gFontHex;
  for (size_t i = 0; i < sizeof(FONT) / sizeof(FONT[0]); i++)
    for (size_t c = 0; c < 3; c++) {
      uint8_t b = pgm_read_byte(&FONT[i][c]);
      *o++ = H[b >> 4]; *o++ = H[b & 15];
    }
  *o = 0;
  return gFontHex;
}

// --- Sound -----------------------------------------------------------------
// Passive piezo on GPIO15. Volume is the pulse width of the LEDC channel: a
// smaller duty cycle sounds quieter.
void soundBegin() {
  pinMode(PIN_BUZZER, OUTPUT);
  ledcSetup(0, 2000, 10);
  ledcAttachPin(PIN_BUZZER, 0);
  ledcWrite(0, 0);
}
static bool toneActive=false,toneSequence=false,toneManual=false;
static uint32_t toneUntil=0;
static uint8_t toneStep=0;
static void toneStart(uint16_t frequency,uint16_t duration) {
  ledcWriteTone(0,frequency);
  ledcWrite(0,frequency ? map(gCfg.volAlarm,0,30,0,512) : 0);
  toneUntil=millis()+duration; toneActive=true;
}
void soundStop() { StateGuard lock;toneActive=false;toneSequence=false;ledcWrite(0, 0); }

void soundBeep(uint16_t freq, uint16_t ms) {
  StateGuard lock;
  if (!gCfg.soundEnabled || gAlarmCritical) return;
  toneSequence=false;toneManual=true;toneStart(freq,ms);
}

void soundAlarm(bool manual) {
  StateGuard lock;
  if (!gCfg.soundEnabled || (manual && gAlarmCritical)) return;
  toneSequence=true;toneManual=manual;toneStep=0;
  toneStart(1568,90);
  gSt.lastAlarmSound = millis();
}

/*
 * Repeats the alarm tone as long as the alarm stands and has not been
 * acknowledged. There used to be exactly one tone at the start - miss the
 * first second and you heard nothing more.
 */
void soundTick() {
  StateGuard lock;
  if(!gCfg.soundEnabled || (toneActive && !toneManual && !anyAlarm()))soundStop();
  if(anyAlarm() && gCfg.soundEnabled && (gSt.soundPending ||
     (!toneActive && gCfg.alarmRepeatSec && millis()-gSt.lastAlarmSound >= (uint32_t)gCfg.alarmRepeatSec*1000))) {
    gSt.soundPending=false;soundAlarm();
  }
  if(!toneActive || !timeReached(millis(),toneUntil))return;
  if(!toneSequence || ++toneStep>=9){soundStop();return;}
  int phase=toneStep%3;
  toneStart(phase==0?1568:phase==1?2093:0,phase==2?60:90);
}
