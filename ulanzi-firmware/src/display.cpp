/*
 * display.cpp - 8x32-Matrix, eigener 3x5-Font, die Schirme
 *
 * Die Pixelanordnung der TC001 ist eine Serpentine: gerade Zeilen laufen von
 * links nach rechts, ungerade von rechts nach links, erster Pixel oben links.
 * Quelle: github.com/rroels/ulanzi_tc001_hardware
 * Im Webinterface umschaltbar, falls eine Platine anders verdrahtet ist.
 */
#include <FastLED.h>
#include "display.h"

static CRGB leds[NUM_LEDS];
static uint8_t gBri = 1;

// --- 3x5-Font, ASCII 32..90 -----------------------------------------------
// Ein Byte je Spalte, Bit 0 = oberste Zeile. Drei Spalten plus ein Pixel
// Abstand ergeben 4 Pixel Vorschub - acht Zeichen passen nebeneinander.
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

// --- Grundfunktionen -------------------------------------------------------
static inline CRGB C(uint32_t rgb) { return CRGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF); }

static inline uint16_t xy(int x, int y) {
  if (x < 0 || x >= MATRIX_W || y < 0 || y >= MATRIX_H) return NUM_LEDS;
  if (gCfg.serpentine && (y & 1)) return y * MATRIX_W + (MATRIX_W - 1 - x);
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
  if (x > MATRIX_W || x < -4) return x + 4;      // ausserhalb, nichts zeichnen
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

// --- Die einzelnen Schirme -------------------------------------------------
static void screenVitals(const Vitals &v) {
  const Palette &p = gCfg.pal;
  drawHeart(C(p.heart));
  drawNumRight(17, (int)lroundf(v.heart), C(p.numbers));
  px(18, 3, C(p.sep));
  drawNumRight(31, (int)lroundf(v.oxygen), C(p.numbers));
  // Schlafbalken unten: je tiefer der Schlaf, desto kuerzer und kuehler.
  // ACHTUNG: die Zahlenwerte von "ss" sind nicht dokumentiert. Bis sie
  // gegen die Owlet-App gegengeprueft sind, faellt alles Unbekannte
  // bewusst auf den grauen Balken - lieber ehrlich unbekannt als falsch.
  int x = 15, w = 2; CRGB c = C(p.sleepUnk);
  switch (v.sleepSt) {
    case 1: case 8:  x = 7;  w = 18; c = C(p.awake);      break;
    case 2: case 9:  x = 11; w = 10; c = C(p.lightSleep); break;
    case 3: case 10: x = 14; w = 4;  c = C(p.deepSleep);  break;
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
  drawNumRight(28, pct, C(p.numbers));
  drawText(29, "%", C(p.batFrame));
}

static void screenWaiting() {
  const Palette &p = gCfg.pal;
  drawHeart(C(p.heartWait));
  drawText(10, "--", C(p.dashes));
  px(18, 3, C(p.sep));
  drawText(25, "--", C(p.dashes));
  fillRect(15, 7, 2, 1, C(p.sleepUnk));
}

static void screenOffline() { drawText(2, "OFFLINE", C(gCfg.pal.offline)); }
static void screenSetup()   { drawText(1, "SETUP", C(gCfg.pal.info)); }

static int gScrollX = MATRIX_W;
static uint32_t gScrollAt = 0;
static void screenAlarm(const String &txt, bool critical) {
  CRGB c = C(critical ? gCfg.pal.alarm : gCfg.pal.info);
  // Der Vorschub steht bewusst HIER und nicht in dispTick: sonst laeuft der
  // Text nur im Normalbetrieb, und die Vorschau-Knoepfe Alarm und Hinweis
  // zeigen eine leere Matrix, weil der Text bei x=32 draussen stehen bleibt.
  if (millis() - gScrollAt > 60) {
    gScrollAt = millis();
    if (--gScrollX < -textWidth(txt)) gScrollX = MATRIX_W;
  }
  drawText(gScrollX, txt, c);
  // Die drei Eckpixel blinken - Dringlichkeit, ohne den Text flackern zu
  // lassen. Nur bei kritischen Alarmen; ein Hinweis blinkt nicht.
  if (critical && ((millis() / 500) & 1)) {
    px(0, 0, c); px(15, 0, c); px(31, 0, c);
  }
}

// --- Testbilder ------------------------------------------------------------
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
      d.sleepSt = ((t / 3000) % 3 == 0) ? 8 : ((t / 3000) % 3 == 1) ? 9 : 10;
      screenVitals(d); break;
    }
    case TEST_BATTERY: screenBattery(64, ((t / 3000) & 1) != 0); break;
    case TEST_WAITING: screenWaiting(); break;
    case TEST_OFFLINE: screenOffline(); break;
    case TEST_ALARM:   screenAlarm(L("TEST ALARM OXYGEN 84 SUSTAINED BELOW 86","TESTALARM SAUERSTOFF 84 DAUERHAFT UNTER 86"), true); break;
    case TEST_INFO:    screenAlarm(L("SOCK BATTERY LOW","SOCKE AKKU LEER"), false); break;
  }
}

// --- Helligkeit ------------------------------------------------------------
/*
 * Rangfolge, unveraendert aus der Home-Assistant-Fassung:
 *   Alarm                          -> voll, schlaegt alles
 *   Socke aktiv UND Umgebung hell  -> Tageswert
 *   sonst                          -> Minimum
 *
 * Der Lichtsensor wird gemittelt und mit Hysterese ausgewertet. Ohne das
 * springt die Helligkeit rund um die Schwelle mehrmals je Sekunde zwischen
 * Minimum und Tageswert - am Kinderbett ein sichtbares Flackern.
 */
static uint16_t gLdrAvg = 0;
static bool     gBrightAmbient = false;

static void ldrTick() {
  static uint32_t last = 0;
  if (millis() - last < 200) return;             // 5 Messungen je Sekunde
  last = millis();
  uint16_t raw = analogRead(PIN_LDR) >> 2;       // 12 Bit -> 0..1023
  gLdrAvg = gLdrAvg ? (uint16_t)((gLdrAvg * 7 + raw) / 8) : raw;
  int hi = gCfg.ldrThreshold + gCfg.ldrHysteresis;
  int lo = gCfg.ldrThreshold - gCfg.ldrHysteresis;
  if (!gBrightAmbient && gLdrAvg > hi) gBrightAmbient = true;
  else if (gBrightAmbient && gLdrAvg < lo) gBrightAmbient = false;
  gSt.ldrRaw = gLdrAvg;
  gSt.ambientBright = gBrightAmbient;
}

static uint8_t targetBrightness() {
  if (gSt.testMode) return constrain(gCfg.briTest, 1, 255);
  if (anyAlarm())   return constrain(gCfg.briAlarm, 1, 255);
  bool sockActive = gSt.cloudOk && gSt.v.charging == 0 && gSt.v.heart > 0;
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

// Eine Meldung, die eine Weile durchlaeuft und dann von selbst verschwindet.
// Beim Start zeigt das Geraet damit seine IP-Adresse - sonst muesste man sie
// im Router suchen, um ueberhaupt an die Oberflaeche zu kommen.
static String   gMsg;
static uint32_t gMsgUntil = 0;
static uint32_t gMsgCol   = 0xFFFFFF;
void dispMessage(const String &txt, uint32_t seconds, uint32_t rgb) {
  gMsg = txt; gMsgCol = rgb;
  gMsgUntil = millis() + seconds * 1000UL;
  gScrollX = MATRIX_W;
}

void dispTest(uint8_t mode, uint32_t seconds) {
  gSt.testMode = mode;
  gSt.testUntil = mode ? millis() + seconds * 1000UL : 0;
  if (mode) gScrollX = MATRIX_W;
}

const char *screenName(ScreenId s) {
  switch (s) {
    case SCR_ALARM:   return "Alarm";
    case SCR_VITALS:  return "Vitalwerte";
    case SCR_BATTERY: return "Akku";
    case SCR_WAITING: return "Warten auf Werte";
    case SCR_OFFLINE: return "Offline";
    case SCR_SETUP:   return "Einrichtung";
    default:          return "Test";
  }
}

// --- Hauptschleife der Anzeige --------------------------------------------
void dispTick() {
  static uint32_t last = 0;
  // 30 Bilder je Sekunde reichen fuer den Lauftext und lassen dem WLAN Luft.
  if (millis() - last < 33) return;
  last = millis();
  ldrTick();

  if (gSt.testMode && millis() > gSt.testUntil) gSt.testMode = TEST_OFF;
  clear();

  if (gSt.testMode) {
    gSt.screen = SCR_TEST;
    screenTest(gSt.testMode);
  } else if (gMsgUntil && millis() < gMsgUntil) {
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
    stateLock();
    String  al   = gAlarmText;
    bool    crit = gAlarmCritical;
    Vitals  v    = gSt.v;
    bool    ok   = gSt.cloudOk;
    uint32_t age = millis() - gSt.lastOkAt;
    stateUnlock();

    if (al.length() && !(crit && gSt.silenced)) {
      gSt.screen = SCR_ALARM;
      screenAlarm(al, crit);
    } else {
      gScrollX = MATRIX_W;
      // 20 s statt 90: bei 5 s Abruftakt sind vier verpasste Runden genug,
      // um nicht mehr von einer lebenden Verbindung auszugehen.
      bool connected = ok && age < 20000UL;
      if (!connected) {
        gSt.screen = SCR_OFFLINE; screenOffline();
      } else if (v.charging != 0 || v.sockOff) {
        gSt.screen = SCR_BATTERY; screenBattery((int)lroundf(v.battery), v.charging != 0);
      } else if (vitalsFresh() && v.heart > 0 && v.oxygen > 0) {
        // Puls und Sauerstoff beide 0 heisst: die Station ist deaktiviert,
        // nicht dass das Kind keinen Puls hat. Dann lieber Striche zeigen.
        gSt.screen = SCR_VITALS; screenVitals(v);
      } else {
        gSt.screen = SCR_WAITING; screenWaiting();
      }
    }
  }

  uint8_t t = targetBrightness();
  if (t != gBri) { gBri = t; FastLED.setBrightness(gBri); }
  gSt.brightness = gBri;
  FastLED.show();
}

// --- Live-Vorschau fuer den Browser ---------------------------------------
// Fester Puffer, kein Heap: das wird mehrmals je Sekunde abgerufen.
static char gHex[NUM_LEDS * 6 + 8];
const char *dispFrameHex() {
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
  // Helligkeit gleich mitschicken: die Vorschau im Browser holte sie
  // vorher aus dem Statusbericht, der nur alle 1,5 s kommt - deshalb
  // leuchtete das Bild kurz hell auf und dimmte dann nach.
  *o++ = H[gBri >> 4]; *o++ = H[gBri & 15];
  *o = 0;
  return gHex;
}

// Der Font fuer die Farbvorschau im Browser. Die Vorschau zeichnet die
// Schirme in JavaScript nach; die Glyphen kommen von hier, damit die Schrift
// dort nicht von der Matrix abweichen kann. Die ANORDNUNG der Schirme steht
// dagegen zweimal da - einmal oben in screenVitals & Co. und einmal als
// Zwilling in webui.cpp. Wer hier etwas verschiebt, muss dort nachziehen.
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

// --- Ton -------------------------------------------------------------------
// Passiver Piezo an GPIO15. Die Lautstaerke regelt die Pulsbreite des
// LEDC-Kanals: kleineres Tastverhaeltnis klingt leiser.
void soundBegin() {
  pinMode(PIN_BUZZER, OUTPUT);
  ledcSetup(0, 2000, 10);
  ledcAttachPin(PIN_BUZZER, 0);
  ledcWrite(0, 0);
}
void soundStop() { ledcWrite(0, 0); }

void soundBeep(uint16_t freq, uint16_t ms) {
  if (!gCfg.soundEnabled) return;
  int duty = map(constrain(gCfg.volAlarm, 0, 30), 0, 30, 0, 512);
  ledcWriteTone(0, freq);
  ledcWrite(0, duty);
  delay(ms);
  soundStop();
}

void soundAlarm() {
  if (!gCfg.soundEnabled) return;
  // Zweitonig, damit man am Klang hoert, dass der Alarm von diesem Geraet
  // kommt und nicht von der Owlet-Basisstation.
  for (int i = 0; i < 3; i++) { soundBeep(1568, 90); soundBeep(2093, 90); delay(60); }
  gSt.lastAlarmSound = millis();
}

/*
 * Wiederholt den Alarmton, solange der Alarm anliegt und nicht quittiert
 * wurde. Vorher gab es genau einen Ton bei Alarmbeginn - wer die erste
 * Sekunde verpasst hat, bekam nichts mehr mit.
 */
void soundTick() {
  if (!gCfg.alarmRepeatSec || !anyAlarm()) return;
  if (millis() - gSt.lastAlarmSound < (uint32_t)gCfg.alarmRepeatSec * 1000UL) return;
  soundAlarm();
}
