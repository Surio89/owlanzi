/*
 * display.h - matrix, font and the screens
 */
#pragma once
#include "owlanzi.h"

void dispBegin();
void dispTick();                 // call from loop()
uint8_t dispBrightness();

// Test patterns for the web interface
enum TestMode : uint8_t {
  TEST_OFF = 0,
  TEST_CORNERS,    // corners one by one - shows orientation and order
  TEST_SWEEP,      // chase light in chain order
  TEST_COLORS,     // red/green/blue/white in turn
  TEST_VITALS,
  TEST_BATTERY,
  TEST_WAITING,
  TEST_OFFLINE,
  TEST_ALARM,
  TEST_INFO
};
void dispTest(uint8_t mode, uint32_t seconds);
// Scrolling text for a fixed duration, e.g. the IP address after boot
void dispMessage(const String &txt, uint32_t seconds, uint32_t rgb);

// Current frame as a hex string for the live mirror in the browser:
// 256 pixels * 3 bytes. Writes into a fixed buffer, no heap.
const char *dispFrameHex();

// The 3x5 font as a hex string: 59 glyphs from ASCII 32, three column bytes
// each. The colour preview in the browser redraws the screens itself and
// takes the glyphs from here, so the type there cannot drift apart.
const char *dispFontHex();

const char *screenName(ScreenId s);

// Sound
void soundBegin();
void soundBeep(uint16_t freq, uint16_t ms);
void soundAlarm();
void soundStop();
void soundTick();                // repeats the alarm tone
