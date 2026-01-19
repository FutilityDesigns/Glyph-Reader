/*
================================================================================
  Button Functions - Physical Button Interface Header
================================================================================
  
  This module provides interface for physical button inputs.
  Currently defined but not fully implemented in main application.
  
  Hardware:
    - Button 1: GPIO 11 (BUTTON_1_PIN in glyphReader.h)
    - Button 2: GPIO 33 (BUTTON_2_PIN in glyphReader.h)
  
  Button2 Library Features:
    - Debouncing
    - Click detection
    - Double-click detection
    - Triple-click detection
    - Long-press detection
  
  Functions:
    - buttonInit(): Initialize Button2 library and attach handlers
    - click(): Single click event handler
    - doubleClick(): Double-click event handler
    - tripleClick(): Triple-click event handler
    - longClickDetected(): Long-press start event handler
    - longClick(): Long-press release event handler
  
  Note: Button functionality is reserved for future features.
        Pin definitions exist in glyphReader.h but buttons are not
        currently polled or used in main application loop.
  
================================================================================
*/

#ifndef BUTTON_FUNCTIONS_H
#define BUTTON_FUNCTIONS_H

#include "Button2.h"

//=====================================
// Global Button Objects
//=====================================

/// Button 1 object (GPIO 11, defined in glyphReader.h)
extern Button2 button1;

/// Button 2 object (GPIO 33, defined in glyphReader.h)
extern Button2 button2;

//=====================================
// Button Control Functions
//=====================================

/**
 * @brief Initialize button objects and attach event handlers
 * 
 * Configures Button2 library for both buttons and registers callbacks
 * for click, double-click, triple-click, and long-press events.
 * 
 * Note: Currently not called from main application.
 */
void buttonInit();

//=====================================
// Event Handlers
//=====================================

/**
 * @brief Single click event handler
 * 
 * Called when button is pressed and released once.
 * 
 * @param btn Reference to Button2 object that triggered event
 */
void click(Button2& btn);

/**
 * @brief Double-click event handler
 * 
 * Called when button is clicked twice in rapid succession.
 * 
 * @param btn Reference to Button2 object that triggered event
 */
void doubleClick(Button2& btn);

/**
 * @brief Triple-click event handler
 * 
 * Called when button is clicked three times in rapid succession.
 * 
 * @param btn Reference to Button2 object that triggered event
 */
void tripleClick(Button2& btn);

//=====================================
// Settings Management Functions
//=====================================

/**
 * @brief Enter settings menu mode
 * 
 * Initializes settings menu, loads current values, and displays menu.
 */
void enterSettingsMode();

/**
 * @brief Exit settings menu mode
 * 
 * Returns to normal operation mode and clears settings display.
 */
void exitSettingsMode();

/**
 * @brief Get total number of settings
 * 
 * @return Number of configurable settings
 */
int getSettingsCount();

/**
 * @brief Get number of value options for a setting
 * 
 * @param settingIndex Index of the setting
 * @return Number of options (including "Disabled")
 */
int getSettingValueCount(int settingIndex);

/**
 * @brief Get current value index for a setting
 * 
 * @param settingIndex Index of the setting
 * @return Current value index
 */
int getCurrentValueIndex(int settingIndex);

/**
 * @brief Save setting value to preferences
 * 
 * @param settingIndex Index of the setting
 * @param valueIndex Index of the selected value
 */
void saveSettingValue(int settingIndex, int valueIndex);

//=====================================
// Event Handlers
//=====================================

/**
 * @brief Long-press start event handler
 * 
 * Called when button is held down for long-press threshold duration.
 * Fires once at the beginning of long-press.
 * 
 * @param btn Reference to Button2 object that triggered event
 */
void longClickDetected(Button2& btn);

/**
 * @brief Long-press release event handler
 * 
 * Called when button is released after being held for long-press.
 * 
 * @param btn Reference to Button2 object that triggered event
 */
void longClick(Button2& btn);

#endif // BUTTON_FUNCTIONS_H