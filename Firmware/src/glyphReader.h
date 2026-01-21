/*
================================================================================
  Glyph Reader - Global Header File
================================================================================
  
  This header contains:
    - Hardware pin definitions (I2C, buttons)
    - Universal library includes (Arduino, Wire, SPI)
    - Logging macros (development vs production builds)
    - Global state variable declarations (extern)
  
  This file is included by all major modules to ensure consistent hardware
  configuration and access to shared global state.
  
  Hardware Configuration:
    - ESP32-S3 GPIO assignments for peripherals
    - I2C pins for Pixart IR camera
    - Button pins (currently defined but not fully implemented)
  
  Build Configurations:
    - ENV_DEV: Full debug logging enabled
    - ENV_PROD: Minimal logging (or conditional debug)
  
================================================================================
*/

#ifndef WAND_COMMANDER_H
#define WAND_COMMANDER_H

//=====================================
// Hardware Pin Definitions
//=====================================

// Button GPIO assignments (reserved for future features)
#define BUTTON_1_PIN  41    // GPIO41 for Button 1
#define BUTTON_2_PIN  42    // GPIO42 for Button 2

// I2C Bus Configuration
#define I2C_SDA 6           // GPIO6 - I2C Data Line (Pixart IR camera)
#define I2C_SCL 5           // GPIO5 - I2C Clock Line (Pixart IR camera)

//=====================================
// Universal Library Includes
//=====================================

#include <Arduino.h>        // Core Arduino framework for ESP32
#include <Wire.h>           // I2C communication library
#include <SPI.h>            // SPI communication library (for display and SD card)

//=====================================
// Logging Macros
//=====================================

/**
 * Always-on logging macro
 * Logs to serial console regardless of build configuration.
 * Use for critical errors and important status messages.
 */
#define LOG_ALWAYS(format, ...) Serial.printf(format "\n", ##__VA_ARGS__)

#ifdef ENV_DEV
/**
 * Development build logging macro
 * Logs detailed debug information to serial console.
 * Only active when ENV_DEV is defined during compilation.
 */
#define LOG_DEBUG(format, ...) Serial.printf(format "\n", ##__VA_ARGS__)
#endif

#ifdef ENV_PROD
/**
 * Production build logging macro
 * Currently disabled in production builds to reduce serial overhead.
 * Can be enabled conditionally based on runtime debug flag if needed.
 */
//#define LOG_DEBUG(format, ...) do { if (debug) Serial.printf("[DEBUG] " format "\n", ##__VA_ARGS__); } while(0)
#endif

//=====================================
// Global State Variables (Extern)
//=====================================

/**
 * Timestamp when spell name was displayed on screen
 * Used to track how long a spell name has been shown.
 * Set to millis() when spell is detected, checked for timeout in main loop.
 * Value of 0 indicates no spell currently displayed.
 */
extern unsigned long screenSpellOnTime;

/**
 * Timestamp of last screen activity
 * Used to track screen backlight timeout (60 seconds of inactivity).
 * Updated whenever screen is used for visual feedback.
 */
extern unsigned long screenOnTime;

/**
 * Current backlight on/off state
 * True when LCD backlight is on, false when off.
 * Used to prevent redundant backlight operations.
 */
extern bool backlightStateOn;

/**
 * Timestamp when LED effect started
 * Used to track LED effect timeout (5 seconds after spell detection).
 * Value of 0 indicates no active LED effect timer.
 */
extern unsigned long ledOnTime;

/**
 * Nightlight mode active flag
 * True when nightlight mode is enabled (via spell trigger).
 * When true, LEDs return to nightlight mode after timeout instead of turning off.
 * When false, LEDs turn off completely after timeout.
 */
extern bool nightlightActive;

/**
 * Timestamp when nightlight mode was activated
 * Used to track nightlight timeout duration.
 */
extern unsigned long nightlightOnTime;

/**
 * Calculated nightlight timeout in milliseconds
 * When nightlight is activated, this is set to either:
 * - Milliseconds until next sunrise (if location configured)
 * - Fixed timeout value (fallback if sunrise calculation fails)
 */
extern unsigned long nightlightCalculatedTimeout;

/**
 * Settings Menu Variables
 * Used to manage the settings menu
 * and allow direct user settings changes
 */
extern bool inSettingsMode;         // True if currently in settings menu
extern bool editingSettingValue;    // True if currently editing a setting value
extern int currentSettingIndex;     // Index of currently selected setting
extern int settingValueIndex;       // Index of currently selected value within setting

#endif // WAND_COMMANDER_H