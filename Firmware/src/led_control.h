/*
================================================================================
  LED Control - NeoPixel RGBW LED Control Header
================================================================================
  
  This module manages NeoPixel RGBW LED effects for visual feedback.
  LEDs indicate system state and spell detection with colors and patterns.
  
  Hardware:
    - NeoPixel RGBW LED strip (12 LEDs)
    - Connected to GPIO 48
  
  LED Modes:
    - LED_OFF: All LEDs off
    - LED_SOLID: Single solid color (yellow/green/blue/red for state machine)
    - LED_RAINBOW: Animated rainbow cycle (continuous rotating colors)
    - LED_SPARKLE: Random sparkle effect (spell detection feedback)
    - LED_NIGHTLIGHT: Soft warm white glow (nightlight mode)
  
  State Machine Colors:
    - Yellow: IR detected, waiting for stillness
    - Green: Ready to track, stillness achieved
    - Blue: Recording gesture
    - Red: Error or no match  
================================================================================
*/

#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

//=====================================
// Hardware Configuration
//=====================================

/// GPIO pin connected to NeoPixel data line
#define LED_PIN 48

/// Number of LEDs in the strip
#define NUM_LEDS 12

//=====================================
// Global Objects
//=====================================

/// Adafruit NeoPixel strip object (defined in led_control.cpp)
extern Adafruit_NeoPixel strip;

//=====================================
// LED Mode Enumeration
//=====================================

/**
 * LED operating modes
 * Modes determine the visual behavior of the LED strip.
 * Some modes are static (SOLID, OFF), others are animated (RAINBOW, SPARKLE).
 */
enum LEDMode {
  LED_OFF,         ///< All LEDs off
  LED_SOLID,       ///< Single solid color (set via ledSolid())
  LED_RAINBOW,     ///< Animated rainbow cycle
  LED_SPARKLE,     ///< Random sparkle effect
  LED_PULSE,       ///< Breathing pulse effect
  LED_COLOR_WAVE,  ///< Wave of color moving through strip
  LED_COMET,       ///< Comet/meteor trail effect
  LED_NIGHTLIGHT   ///< Soft warm white nightlight
};

/// Current active LED mode (defined in led_control.cpp)
extern LEDMode currentMode;

//=====================================
// LED Control Functions
//=====================================

/**
 * Initialize NeoPixel LED strip
 * Configures Adafruit_NeoPixel library and sets initial state to off.
 * Must be called during setup() before using LEDs.
 */
void initLEDs();

/**
 * Set all LEDs to specific RGB color
 * r: Red component (0-255)
 * g: Green component (0-255)
 * b: Blue component (0-255)
 */
void setLED(uint8_t r, uint8_t g, uint8_t b);

/**
 * Turn off all LEDs
 * Sets mode to LED_OFF and immediately clears the strip.
 */
void ledOff();

/**
 * Set solid color by name
 * Convenience function for common state machine colors.
 * color: Color name: "red", "green", "blue", "yellow"
 */
void ledSolid(const char* color);

//=====================================
// Effect Functions
//=====================================

/**
 * Switch to specific LED mode
 * Changes the active mode and initializes the effect.
 * For animated effects, updateLEDs() must be called regularly from loop().
 * mode: Desired LED mode
 */
void setLEDMode(LEDMode mode);

/**
 * Update animated LED effects
 * Non-blocking function that advances animated effects (rainbow, sparkle).
 * Should be called every iteration of main loop() for smooth animations.
 * Has no effect for static modes (SOLID, OFF).
 */
void updateLEDs();

/**
 * Start rainbow cycle effect
 * Continuously rotating rainbow colors across the LED strip.
 * Switches mode to LED_RAINBOW.
 */
void ledRainbow();

/**
 * Start sparkle effect
 * Random LEDs flash white with varying brightness.
 * Used for spell detection feedback.
 * Switches mode to LED_SPARKLE.
 */
void ledSparkle();

/**
 * Start pulse/breathing effect
 * LEDs smoothly fade in and out in a random color.
 * Switches mode to LED_PULSE.
 */
void ledPulse();

/**
 * Start color wave effect
 * Wave of color moves through the LED strip.
 * Switches mode to LED_COLOR_WAVE.
 */
void ledColorWave();

/**
 * Start comet effect
 * Comet/meteor with trailing tail moves through strip.
 * Switches mode to LED_COMET.
 */
void ledComet();

/**
 * Pick random spell effect and activate it
 * Randomly selects one of: Sparkle, Rainbow, Pulse, ColorWave, Comet
 * Used for spell detection feedback to add variety.
 */
void ledRandomEffect();

/**
 * Start nightlight effect
 * Soft warm white glow at configurable brightness.
 * Switches mode to LED_NIGHTLIGHT.
 * brightness: White channel brightness (0-255, default 150)
 */
void ledNightlight(int brightness = 150);

#endif // LED_CONTROL_H
