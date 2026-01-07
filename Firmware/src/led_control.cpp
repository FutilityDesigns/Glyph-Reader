/*
================================================================================
  LED Control - NeoPixel LED Effects Implementation
================================================================================
  
  This module controls the NeoPixel RGB LED strip for visual feedback during
  wand gesture tracking and spell detection.
  
  Hardware:
    - WS2812B NeoPixel LED strip (10 LEDs)
    - Data pin: GPIO 45
  
  LED Modes:
    - LED_OFF: All LEDs off (default state)
    - LED_SOLID: Single solid color (feedback during tracking states)
    - LED_RAINBOW: Animated rainbow cycle (not currently used)
    - LED_SPARKLE: Random sparkle effect (spell detection celebration)
    - LED_NIGHTLIGHT: Soft warm white (ambient nightlight mode)
  
  State-Based Colors:
    - Yellow: IR detected, waiting for stillness
    - Green: Ready to track, wand is stable
    - Blue: Recording gesture trajectory
    - Red: Error or no spell match
    - Sparkle: Spell successfully detected
    - Warm white: Nightlight active  
================================================================================
*/

#include "led_control.h"

//=====================================
// Global NeoPixel Object
//=====================================

/**
 * NeoPixel strip object
 * Configuration:
 *   - NUM_LEDS: 10 LEDs in strip
 *   - LED_PIN: GPIO 45 (data line)
 *   - NEO_GRB: Color order (Green-Red-Blue)
 *   - NEO_KHZ800: 800kHz signal timing
 */
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

//=====================================
// Effect State Variables
//=====================================

/// Current LED mode (determines which effect is active)
LEDMode currentMode = LED_OFF;

/// Rainbow animation offset (0-65535, wraps around for continuous cycle)
static uint16_t rainbowOffset = 0;

/// Timestamp of last effect update (for animation timing)
static unsigned long lastEffectUpdate = 0;

/// Effect update interval in milliseconds (50ms = 20fps animation)
const unsigned long EFFECT_UPDATE_INTERVAL = 50;

//=====================================
// LED Initialization
//=====================================

/**
 * Initialize NeoPixel LED strip
 * Configures hardware and sets initial state:
 *   - Initialize NeoPixel library
 *   - Set brightness to 50/255 (20%)
 *   - Turn off all LEDs
 *   - Set mode to LED_OFF
 * Must be called once during setup() before using LEDs.
 */
void initLEDs() {
  strip.begin();           // Initialize NeoPixel hardware
  strip.setBrightness(50); // Set brightness (0-255, 50 = 20% for comfort)
  strip.show();            // Update strip (turns all pixels off initially)
  currentMode = LED_OFF;   // Set initial mode
}

//=====================================
// Core LED Control Functions
//=====================================

/**
 * Set all LEDs to a specific RGB color
 * Sets all LEDs in the strip to the same solid color and switches
 * to LED_SOLID mode to prevent animation overwriting.
 * r: Red component (0-255)
 * g: Green component (0-255)
 * b: Blue component (0-255)
 */
void setLED(uint8_t r, uint8_t g, uint8_t b) {
  currentMode = LED_SOLID;  // Switch to solid mode (no animation)
  for(int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();  // Update physical LEDs
}

/**
 *  Set LED mode
 * Changes the current LED mode which determines what effect is displayed.
 * If switching to LED_OFF, immediately turns off all LEDs.
 * For other modes, the effect is handled by updateLEDs() in main loop.
 * mode: New LED mode (LED_OFF, LED_SOLID, LED_RAINBOW, LED_SPARKLE, LED_NIGHTLIGHT)
 */
void setLEDMode(LEDMode mode) {
  currentMode = mode;
  
  // If turning off, immediately clear all LEDs
  if (mode == LED_OFF) {
    for(int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, 0);  // Black (off)
    }
    strip.show();  // Update physical LEDs
  }
}

//=====================================
// Animation Update Function
//=====================================

/**
 * Update animated LED effects
 * Updates animated effects (rainbow, sparkle) at regular intervals while
 * leaving static modes (solid, off, nightlight) unchanged.
 * Animation Details:
 *   - Updates every 50ms (EFFECT_UPDATE_INTERVAL)
 *   - Rainbow: Smooth cycling through full color spectrum
 *   - Sparkle: Random LEDs flash in random colors
 */
void updateLEDs() {
  unsigned long currentTime = millis();
  
  // Only update at specified interval for smooth, consistent animation
  if (currentTime - lastEffectUpdate < EFFECT_UPDATE_INTERVAL) {
    return;  // Not time to update yet
  }
  lastEffectUpdate = currentTime;
  
  switch(currentMode) {
    case LED_RAINBOW:
      //-----------------------------------
      // Rainbow Effect
      //-----------------------------------
      // Smoothly cycle through rainbow colors
      // Each LED is offset so they form a rainbow pattern along the strip
      rainbowOffset = (rainbowOffset + 1) % 65536;  // Increment hue (0-65535)
      
      for(int i = 0; i < NUM_LEDS; i++) {
        // Calculate hue for this LED (evenly spaced around color wheel)
        uint16_t pixelHue = rainbowOffset + (i * 65536L / NUM_LEDS);
        strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
      }
      strip.show();
      break;
      
    case LED_SPARKLE:
      //-----------------------------------
      // Sparkle Effect
      //-----------------------------------
      // Random LEDs flash in random colors, creating a twinkling effect
      for(int i = 0; i < NUM_LEDS; i++) {
        if (random(100) < 20) {  // 20% chance for each LED to change
          // Randomly choose color or off
          if (random(100) < 70) {  // 70% chance for color, 30% for off
            uint32_t color = strip.ColorHSV(random(65536), 255, 255);  // Random hue, full saturation and brightness
            strip.setPixelColor(i, strip.gamma32(color));  // Apply gamma correction for better color
          } else {
            strip.setPixelColor(i, 0);  // Turn off (creates twinkling effect)
          }
        }
        // If not changing (80% of time), LED stays at current color
      }
      strip.show();
      break;
      
    case LED_SOLID:
    case LED_OFF:
    case LED_NIGHTLIGHT:
    default:
      // No animation needed - these modes are static
      break;
  }
}

//=====================================
// Convenience Functions
//=====================================

/**
 * Turn off all LEDs
 * Immediately turns off all LEDs by switching to LED_OFF mode.
 * Used when gesture tracking is inactive or after timeout.
 */
void ledOff() {
  setLEDMode(LED_OFF);
}

/**
 * Set LEDs to a solid color by name
 * Convenience function for setting common colors used during gesture tracking.
 * Supported colors:
 *   - "green":  Ready to track (RGB: 0, 50, 0)
 *   - "blue":   Recording gesture (RGB: 0, 0, 50)
 *   - "red":    Error or no match (RGB: 50, 0, 0)
 *   - "yellow": IR detected, waiting for stillness (RGB: 50, 50, 0)
 *   - "purple": Custom state (RGB: 25, 0, 25)
 *   - "orange": Custom state (RGB: 50, 25, 0)
 * color: Color name (case-sensitive string)
 */
void ledSolid(const char* color){
    if (strcmp(color, "green") == 0) {
        setLED(0, 50, 0);         // Green: Ready to track
        setLEDMode(LED_SOLID);
    } else if (strcmp(color, "blue") == 0) {
        setLED(0, 0, 50);         // Blue: Recording gesture
        setLEDMode(LED_SOLID);
    } else if (strcmp(color, "red") == 0) {
        setLED(50, 0, 0);         // Red: Error/no match
        setLEDMode(LED_SOLID);
    } else if (strcmp(color, "yellow") == 0) {
        setLED(50, 50, 0);        // Yellow: IR detected, waiting
        setLEDMode(LED_SOLID);
    } else if (strcmp(color, "purple") == 0) {
        setLED(25, 0, 25);        // Purple: Custom state
        setLEDMode(LED_SOLID);
    } else if (strcmp(color, "orange") == 0) {
        setLED(50, 25, 0);        // Orange: Custom state
        setLEDMode(LED_SOLID);
    } else {
        // Unknown color - turn off
        ledOff();
    }
}

/**
 * Start rainbow cycle animation
 * Switches to LED_RAINBOW mode. Animation is updated by updateLEDs()
 * in main loop at 20fps (50ms intervals).
 * Currently not used in main application, but available for future features
 */
void ledRainbow() {
  setLEDMode(LED_RAINBOW);
}

/**
 * Start sparkle effect animation
 * Switches to LED_SPARKLE mode. Random LEDs flash in random colors,
 * creating a celebration effect. Used when spell is successfully detected.
 */
void ledSparkle() {
  setLEDMode(LED_SPARKLE);
}

/**
 * Activate nightlight mode
 * Sets LEDs to soft warm white color and switches to LED_NIGHTLIGHT mode.
 * Used for ambient lighting when nightlight feature is activated via spell.
 */
void ledNightlight() {
  // Soft warm white (red-heavy for warm tone)
  setLED(50, 30, 10);
  setLEDMode(LED_NIGHTLIGHT);
}