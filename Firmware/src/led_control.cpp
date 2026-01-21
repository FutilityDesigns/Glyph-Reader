/*
================================================================================
  LED Control - NeoPixel LED Effects Implementation
================================================================================
  
  This module controls the NeoPixel RGBW LED strip for visual feedback during
  wand gesture tracking and spell detection.
  
  Hardware:
    - NeoPixel RGBW LED strip (12 LEDs)
    - Data pin: GPIO 48
  
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
#include "glyphReader.h"
#include "preferenceFunctions.h"
#include "wifiFunctions.h"

//=====================================
// Global NeoPixel Object
//=====================================

/**
 * NeoPixel strip object
 * Configuration:
 *   - NUM_LEDS: 12 LEDs in strip
 *   - LED_PIN: GPIO 48 (data line)
 *   - NEO_GRBW: Color order (Green-Red-Blue-White)
 *   - NEO_KHZ800: 800kHz signal timing
 */
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRBW + NEO_KHZ800);

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

// Effect-specific state variables
static uint8_t pulseDirection = 1;          // 1 = brightening, 0 = dimming
static uint8_t pulseBrightness = 0;         // Current pulse brightness (0-255)
static uint32_t pulseColor = 0;             // Color for pulse effect
static int wavePosition = 0;                // Position of wave (0-NUM_LEDS)
static uint32_t waveColor = 0;              // Color for wave effect
static int cometPosition = 0;               // Position of comet head
static uint32_t cometColor = 0;             // Color for comet effect

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
 * Set all LEDs to a specific RGBW color
 * Sets all LEDs in the strip to the same solid color and switches
 * to LED_SOLID mode to prevent animation overwriting.
 * r: Red component (0-255)
 * g: Green component (0-255)
 * b: Blue component (0-255)
 * w: White component (0-255)
 */
void setLED(uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
  currentMode = LED_SOLID;  // Switch to solid mode (no animation)
  for(int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(r, g, b, w));
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
 * Generate random HSV hue excluding red range
 * Red (error color) occupies roughly 0° (65535) and 0-10° (0-1820)
 * This function returns a hue in the range that avoids pure red.
 * return Random hue value (1820-60000) - excludes red, includes orange through violet
 */
uint16_t getRandomNonRedHue() {
  // Red is approximately 0-1820 and 60000-65535 in HSV
  // Safe range: 1820-60000 (orange, yellow, green, cyan, blue, magenta)
  return random(1820, 60000);
}

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
    case LED_RAINBOW: {
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
    }
      
    case LED_SPARKLE: {
      //-----------------------------------
      // Sparkle Effect
      //-----------------------------------
      // Random LEDs flash in random colors, creating a twinkling effect
      for(int i = 0; i < NUM_LEDS; i++) {
        if (random(100) < 20) {  // 20% chance for each LED to change
          // Randomly choose color or off
          if (random(100) < 70) {  // 70% chance for color, 30% for off
            uint32_t color = strip.ColorHSV(getRandomNonRedHue(), 255, 255);  // Random hue (no red), full saturation and brightness
            strip.setPixelColor(i, strip.gamma32(color));  // Apply gamma correction for better color
          } else {
            strip.setPixelColor(i, 0);  // Turn off (creates twinkling effect)
          }
        }
        // If not changing (80% of time), LED stays at current color
      }
      strip.show();
      break;
    }
      
    case LED_PULSE: {
      //-----------------------------------
      // Pulse/Breathing Effect
      //-----------------------------------
      // Smoothly fade all LEDs in and out
      if (pulseDirection) {
        pulseBrightness += 5;
        if (pulseBrightness >= 250) pulseDirection = 0;
      } else {
        pulseBrightness -= 5;
        if (pulseBrightness <= 5) pulseDirection = 1;
      }
      
      // Apply brightness to the chosen color
      uint8_t r = (pulseColor >> 16) & 0xFF;
      uint8_t g = (pulseColor >> 8) & 0xFF;
      uint8_t b = pulseColor & 0xFF;
      for(int i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, (r * pulseBrightness) / 255, 
                                (g * pulseBrightness) / 255, 
                                (b * pulseBrightness) / 255);
      }
      strip.show();
      break;
    }
      
    case LED_COLOR_WAVE: {
      //-----------------------------------
      // Color Wave Effect
      //-----------------------------------
      // Wave of color moves through the strip
      wavePosition = (wavePosition + 1) % (NUM_LEDS * 2);
      
      for(int i = 0; i < NUM_LEDS; i++) {
        // Calculate distance from wave center
        int distance = abs(wavePosition - i);
        if (distance > NUM_LEDS) distance = NUM_LEDS * 2 - distance;
        
        // Brightness falls off with distance
        uint8_t brightness = max(0, 255 - (distance * 40));
        
        uint8_t r = ((waveColor >> 16) & 0xFF) * brightness / 255;
        uint8_t g = ((waveColor >> 8) & 0xFF) * brightness / 255;
        uint8_t b = (waveColor & 0xFF) * brightness / 255;
        
        strip.setPixelColor(i, r, g, b);
      }
      strip.show();
      break;
    }
      
    case LED_COMET: {
      //-----------------------------------
      // Comet/Meteor Effect
      //-----------------------------------
      // Comet with trailing tail moves through strip
      cometPosition = (cometPosition + 1) % (NUM_LEDS + 8);  // Extra space for tail to clear
      
      strip.clear();
      for(int i = 0; i < NUM_LEDS; i++) {
        // Calculate tail brightness (fades behind comet head)
        int tailDistance = cometPosition - i;
        if (tailDistance >= 0 && tailDistance < 8) {
          uint8_t brightness = 255 - (tailDistance * 32);  // Fade over 8 LEDs
          
          uint8_t r = ((cometColor >> 16) & 0xFF) * brightness / 255;
          uint8_t g = ((cometColor >> 8) & 0xFF) * brightness / 255;
          uint8_t b = (cometColor & 0xFF) * brightness / 255;
          
          strip.setPixelColor(i, r, g, b);
        }
      }
      strip.show();
      break;
    }
      
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
 *   - "green":  Ready to track (RGBW: 0, 50, 0, 0)
 *   - "blue":   Recording gesture (RGBW: 0, 0, 50, 0)
 *   - "red":    Error or no match (RGBW: 50, 0, 0, 0)
 *   - "yellow": IR detected, waiting for stillness (RGBW: 50, 50, 0, 0)
 *   - "purple": Custom state (RGBW: 25, 0, 25, 0)
 *   - "orange": Custom state (RGBW: 50, 25, 0, 0)
 * color: Color name (case-sensitive string)
 */
void ledSolid(const char* color){
    if (strcmp(color, "green") == 0) {
        setLED(0, 150, 0, 0);      // Green: Ready to track
        setLEDMode(LED_SOLID);
    } else if (strcmp(color, "blue") == 0) {
        setLED(0, 0, 150, 0);      // Blue: Recording gesture
        setLEDMode(LED_SOLID);
    } else if (strcmp(color, "red") == 0) {
        setLED(150, 0, 0, 0);      // Red: Error/no match
        setLEDMode(LED_SOLID);
    } else if (strcmp(color, "yellow") == 0) {
        setLED(150, 150, 0, 0);     // Yellow: IR detected, waiting
        setLEDMode(LED_SOLID);
    } else if (strcmp(color, "purple") == 0) {
        setLED(150, 0, 150, 0);     // Purple: Custom state
        setLEDMode(LED_SOLID);
    } else if (strcmp(color, "orange") == 0) {
        setLED(150, 75, 0, 0);     // Orange: Custom state
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
 * Start pulse/breathing effect
 * LEDs smoothly fade in and out in a random color (excluding red).
 * Creates a calming breathing effect.
 */
void ledPulse() {
  pulseColor = strip.ColorHSV(getRandomNonRedHue(), 255, 255);  // Random color (no red)
  pulseBrightness = 0;
  pulseDirection = 1;
  setLEDMode(LED_PULSE);
}

/**
 * Start color wave effect
 * Wave of color (excluding red) moves through the LED strip.
 * Creates a flowing, dynamic effect.
 */
void ledColorWave() {
  waveColor = strip.ColorHSV(getRandomNonRedHue(), 255, 255);  // Random color (no red)
  wavePosition = 0;
  setLEDMode(LED_COLOR_WAVE);
}

/**
 * Start comet effect
 * Comet/meteor (excluding red) with trailing tail moves through strip.
 * Creates a shooting star effect.
 */
void ledComet() {
  cometColor = strip.ColorHSV(getRandomNonRedHue(), 255, 255);  // Random color (no red)
  cometPosition = 0;
  setLEDMode(LED_COMET);
}

/**
 * Pick random spell effect and activate it
 * Randomly selects one of the available spell effects:
 * - Sparkle (twinkling stars)
 * - Rainbow (color cycle)
 * - Pulse (breathing)
 * - ColorWave (flowing wave)
 * - Comet (shooting star)
 * Used for spell detection feedback to add variety and excitement.
 */
void ledRandomEffect() {
  int effect = random(5);  // Pick from 5 effects
  
  switch(effect) {
    case 0:
      ledSparkle();
      break;
    case 1:
      ledRainbow();
      break;
    case 2:
      ledPulse();
      break;
    case 3:
      ledColorWave();
      break;
    case 4:
      ledComet();
      break;
    default:
      ledSparkle();
      break;
  }
}

/**
 * Activate nightlight mode
 * Sets LEDs to soft warm white color and switches to LED_NIGHTLIGHT mode.
 * If location is configured, calculates time until next sunrise for auto-off.
 * If location is not configured or calculation fails, uses fixed timeout.
 * Used for ambient lighting when nightlight feature is activated via spell.
 * brightness: White channel brightness (0-255, clamped to safe range 10-255)
 */
void ledNightlight(int brightness) {
  // Clamp brightness to safe range (10 minimum for visibility, 255 maximum)
  int safeBrightness = constrain(brightness, 10, 255);
  
  // Soft warm white using dedicated white channel
  setLED(0, 0, 0, safeBrightness);
  nightlightActive = true;
  nightlightOnTime = millis();
  ledOnTime = 0;  // Don't timeout nightlight
  setLEDMode(LED_NIGHTLIGHT);
  
  // Calculate timeout based on sunrise or use fixed timeout
  unsigned long sunriseTimeout = calculateMillisToNextSunrise(LATITUDE, LONGITUDE, TIMEZONE_OFFSET);
  
  if (sunriseTimeout > 0) {
    // Successfully calculated sunrise time
    nightlightCalculatedTimeout = sunriseTimeout;
    LOG_DEBUG("Nightlight will turn off at sunrise (in %lu hours)", sunriseTimeout / 3600000);
  } else {
    // Calculation failed or location not configured - use fixed timeout
    #ifdef ENV_DEV
    nightlightCalculatedTimeout = 60000;  // 1 minute for testing
    LOG_DEBUG("Using fixed nightlight timeout: 60 seconds (testing mode)");
    #else
    nightlightCalculatedTimeout = 28800000;  // 8 hours
    LOG_DEBUG("Using fixed nightlight timeout: 8 hours");
    #endif
  }
}