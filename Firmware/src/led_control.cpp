/*
Functions related to controlling the LED effects
*/
#include "led_control.h"

// Create NeoPixel strip object
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Effect state variables
LEDMode currentMode = LED_OFF;
static uint16_t rainbowOffset = 0;
static unsigned long lastEffectUpdate = 0;
const unsigned long EFFECT_UPDATE_INTERVAL = 50;  // Update effects every 50ms

// Initialize LEDs
void initLEDs() {
  strip.begin();
  strip.setBrightness(50);  // Set brightness (0-255)
  strip.show();  // Initialize all pixels to 'off'
  currentMode = LED_OFF;
}


// Set all LEDs to a specific color
void setLED(uint8_t r, uint8_t g, uint8_t b) {
  currentMode = LED_SOLID;
  for(int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();
}

// Set LED mode
void setLEDMode(LEDMode mode) {
  currentMode = mode;
  if (mode == LED_OFF) {
    for(int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, 0);
    }
    strip.show();
  }
}

// Update animated effects - call this from loop()
void updateLEDs() {
  unsigned long currentTime = millis();
  
  // Only update at specified interval for smooth animation
  if (currentTime - lastEffectUpdate < EFFECT_UPDATE_INTERVAL) {
    return;
  }
  lastEffectUpdate = currentTime;
  
  switch(currentMode) {
    case LED_RAINBOW:
      // Cycle through rainbow colors
      rainbowOffset = (rainbowOffset + 1) % 65536;
      for(int i = 0; i < NUM_LEDS; i++) {
        // Each LED is offset in the rainbow
        uint16_t pixelHue = rainbowOffset + (i * 65536L / NUM_LEDS);
        strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
      }
      strip.show();
      break;
      
    case LED_SPARKLE:
      // Random sparkles
      for(int i = 0; i < NUM_LEDS; i++) {
        if (random(100) < 20) {  // 20% chance to change
          // Random bright color or off
          if (random(100) < 70) {  // 70% chance for color, 30% for off
            uint32_t color = strip.ColorHSV(random(65536), 255, 255);
            strip.setPixelColor(i, strip.gamma32(color));
          } else {
            strip.setPixelColor(i, 0);
          }
        }
      }
      strip.show();
      break;
      
    case LED_SOLID:
    case LED_OFF:
    default:
      // No animation needed
      break;
  }
}

void ledOff() {
  setLEDMode(LED_OFF);
}

void ledSolid(const char* color){
    if (strcmp(color, "green") == 0) {
        setLED(0, 50, 0);
        setLEDMode(LED_SOLID);
    } else if (strcmp(color, "blue") == 0) {
        setLED(0, 0, 50);
        setLEDMode(LED_SOLID);
    } else if (strcmp(color, "red") == 0) {
        setLED(50, 0, 0);
        setLEDMode(LED_SOLID);
    } else if (strcmp(color, "yellow") == 0) {
        setLED(50, 50, 0);
        setLEDMode(LED_SOLID);
    } else if (strcmp(color, "purple") == 0) {
        setLED(25, 0, 25);
        setLEDMode(LED_SOLID);
    } else if (strcmp(color, "orange") == 0) {
        setLED(50, 25, 0);
        setLEDMode(LED_SOLID);
    } else {
        ledOff();
    }
}
       

void ledRainbow() {
  setLEDMode(LED_RAINBOW);
}

void ledSparkle() {
  setLEDMode(LED_SPARKLE);
}

void ledNightlight() {
  // Soft warm white
  setLED(50, 30, 10);
  setLEDMode(LED_NIGHTLIGHT);
}