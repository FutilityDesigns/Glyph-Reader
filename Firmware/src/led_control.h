#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

// NeoPixel configuration
#define LED_PIN 45
#define NUM_LEDS 10

extern Adafruit_NeoPixel strip;

// LED mode enum
enum LEDMode {
  LED_OFF,
  LED_SOLID,
  LED_RAINBOW,
  LED_SPARKLE,
  LED_NIGHTLIGHT
};

// LED control functions
void initLEDs();
void setLED(uint8_t r, uint8_t g, uint8_t b);
void ledOff();
void ledSolid(const char* color);

// Effect functions
void setLEDMode(LEDMode mode);
void updateLEDs();  // Call this from loop() for animated effects
void ledRainbow();  // Start rainbow cycle effect
void ledSparkle();  // Start sparkle effect

extern LEDMode currentMode;

#endif
