#ifndef SCREEN_FUNCTIONS_H
#define SCREEN_FUNCTIONS_H

#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <SPI.h>

// Pin definitions - safe ESP32-S3 pins
#define TFT_CS    10
#define TFT_DC    9
#define TFT_RST   8
#define TFT_MOSI  11
#define TFT_SCLK  12
#define TFT_BL    13

extern Adafruit_GC9A01A tft;

void screenInit();
void drawIRPoint(int x, int y, bool isActive = true);
void displaySpellName(const char* spellName);
void clearDisplay();
void backlightOff();
void backlightOn();
void updateSetupDisplay(int line, const String &function, const String &status);

// Image display functions
bool displayImageFromSD(const char* filename, int16_t x = 0, int16_t y = 0);


#endif