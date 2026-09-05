/*
 * display.h - Matrix, Schrift und die Schirme
 */
#pragma once
#include "owlanzi.h"

void dispBegin();
void dispTick();                 // aus loop() aufrufen
uint8_t dispBrightness();

// Testbilder fuers Webinterface
enum TestMode : uint8_t {
  TEST_OFF = 0,
  TEST_CORNERS,    // Ecken einzeln - zeigt Orientierung und Reihenfolge
  TEST_SWEEP,      // Lauflicht in Kettenreihenfolge
  TEST_COLORS,     // rot/gruen/blau/weiss nacheinander
  TEST_VITALS,
  TEST_BATTERY,
  TEST_WAITING,
  TEST_OFFLINE,
  TEST_ALARM,
  TEST_INFO
};
void dispTest(uint8_t mode, uint32_t seconds);
// Laufschrift fuer eine feste Dauer, z.B. die IP nach dem Start
void dispMessage(const String &txt, uint32_t seconds, uint32_t rgb);

// Aktuelles Bild als Hexkette fuer die Live-Vorschau im Browser:
// 256 Pixel * 3 Bytes. Schreibt in einen festen Puffer, kein Heap.
const char *dispFrameHex();

// Der 3x5-Font als Hexkette: 59 Zeichen ab ASCII 32, je drei Spaltenbytes.
// Die Farbvorschau im Browser zeichnet die Schirme selbst nach und holt sich
// die Glyphen hierher - so kann die Schrift dort nicht auseinanderlaufen.
const char *dispFontHex();

const char *screenName(ScreenId s);

// Ton
void soundBegin();
void soundBeep(uint16_t freq, uint16_t ms);
void soundAlarm();
void soundStop();
void soundTick();                // wiederholt den Alarmton
