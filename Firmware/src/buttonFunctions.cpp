// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

/*
================================================================================
  Button Functions - Physical Button Interface
================================================================================
  
  This module handles all physical button interactions for the Glyph Reader.
  Uses the Button2 library for debouncing and multi-click detection.
  
  Hardware Configuration:
    - Button 1 (GPIO 11): Primary action button
        - Single click: Toggle nightlight ON/OFF (normal mode)
        - Single click: Select/confirm (settings mode)
        - Single click: Save spell (recording preview mode)
    
    - Button 2 (GPIO 33): Navigation/secondary button
        - Single click: Reserved (normal mode)
        - Single click: Navigate settings / cycle values (settings mode)
        - Single click: Discard spell (recording preview mode)
        - Double click: Enter settings mode
        - Long press: Exit settings mode
  
  Settings Menu Indices:
    0 - Nightlight ON Spell
    1 - Nightlight OFF Spell
    2 - Nightlight RAISE Spell
    3 - Nightlight LOWER Spell
    4 - Add Custom Spell
    5 - Spell Color
  
  Created: 2025-2026 Futility Designs
================================================================================
*/

#include "glyphReader.h"
#include "buttonFunctions.h"
#include "led_control.h"
#include "preferenceFunctions.h"
#include "screenFunctions.h"
#include "spell_patterns.h"
#include "customSpellFunctions.h"

//=====================================
// Forward Declarations
//=====================================
int getCurrentColorIndex();

//=====================================
// Global Button Objects
//=====================================
Button2 button1;  // Primary action button (GPIO 11)
Button2 button2;  // Navigation button (GPIO 33)

//=====================================
// Button Initialization
//=====================================

/**
 * Initialize button hardware and event handlers
 * 
 * Configures both buttons with appropriate timing and registers
 * callbacks for all click types: single, double, triple, and long-press.
 */
void buttonInit() {
    LOG_DEBUG("Initializing buttons...");

    //--- Button 1: Primary Action Button ---
    button1.begin(BUTTON_1_PIN);
    button1.setLongClickTime(1000);    // 1 second threshold for long-press
    button1.setDoubleClickTime(500);   // 500ms window for double-click detection

    // Register event handlers
    button1.setClickHandler(click);
    button1.setDoubleClickHandler(doubleClick);
    button1.setTripleClickHandler(tripleClick);
    button1.setLongClickDetectedHandler(longClickDetected);
    button1.setLongClickHandler(longClick);
    button1.setLongClickDetectedRetriggerable(false);  // Fire once per long-press

    //--- Button 2: Navigation Button ---
    button2.begin(BUTTON_2_PIN);
    button2.setLongClickTime(1000);    // 1 second threshold for long-press
    button2.setDoubleClickTime(500);   // 500ms window for double-click detection

    // Register event handlers (same functions, differentiated by pin)
    button2.setClickHandler(click);
    button2.setDoubleClickHandler(doubleClick);
    button2.setTripleClickHandler(tripleClick);
    button2.setLongClickDetectedHandler(longClickDetected);
    button2.setLongClickHandler(longClick);
    button2.setLongClickDetectedRetriggerable(false);  // Fire once per long-press

    LOG_DEBUG("Buttons initialized.");
}

void click(Button2& btn) {
    //LOG_DEBUG("Button %d: Click detected", btn.getPin());
    // switch case depending on which button is pressed
    switch (btn.getPin()) {
        case BUTTON_1_PIN:
            // Handle button 1 click
            if (spellRecordingState == SPELL_RECORD_PREVIEW) {
                // In spell recording preview - save the spell
                if (saveRecordedSpell()) {
                    // Show success message
                    displayMessage("Spell Saved!", 0x07E0);  // Green
                    delay(1500);
                }
                spellRecordingState = SPELL_RECORD_COMPLETE;
                exitSpellRecordingMode();
                enterSettingsMode();  // Return to settings
                return;
            }
            
            if (inSettingsMode) {
                // In settings mode: Button 1 selects/confirms
                if (!editingSettingValue) {
                    // Check if this is the "Add Spell" setting (index 4)
                    if (currentSettingIndex == 4) {
                        // Enter spell recording mode
                        inSettingsMode = false;
                        enterSpellRecordingMode();
                        return;
                    }
                    // Not editing - enter edit mode for this setting
                    editingSettingValue = true;
                    if (currentSettingIndex == 5) {
                        // Color setting - get current color index
                        settingValueIndex = getCurrentColorIndex();
                        displayColorPicker(settingValueIndex);
                    } else {
                        settingValueIndex = getCurrentValueIndex(currentSettingIndex);
                        displaySettingsMenu(currentSettingIndex, settingValueIndex, true);
                    }
                    LOG_DEBUG("Editing setting %d, current value index: %d", currentSettingIndex, settingValueIndex);
                } else {
                    // Editing - confirm and save the value
                    saveSettingValue(currentSettingIndex, settingValueIndex);
                    editingSettingValue = false;
                    // After saving, show settings menu (non-editing)
                    displaySettingsMenu(currentSettingIndex, settingValueIndex, false);
                    LOG_DEBUG("Saved setting %d with value index %d", currentSettingIndex, settingValueIndex);
                }
            } else {
                // Normal mode: Toggle the nightlight state
                LOG_DEBUG("Button 1 clicked");
                if (nightlightActive) {
                    nightlightActive = false;
                    setLEDMode(LED_OFF);
                } else {
                    ledNightlight(NIGHTLIGHT_BRIGHTNESS);
                }
            }
            break;
        
        //---------------------------------------
        // Button 2: Navigation Button
        //---------------------------------------
        case BUTTON_2_PIN:
            // Check if in spell recording preview mode
            if (spellRecordingState == SPELL_RECORD_PREVIEW) {
                // In spell recording preview - discard and return to settings
                spellRecordingState = SPELL_RECORD_COMPLETE;
                exitSpellRecordingMode();
                enterSettingsMode();
                return;
            }
            
            if (inSettingsMode) {
                // In settings mode: Button 2 navigates
                if (editingSettingValue) {
                    // Editing - cycle to next value
                    settingValueIndex++;
                    if (currentSettingIndex == 5) {
                        int count = getPredefinedColorCount();
                        if (settingValueIndex >= count) settingValueIndex = 0;
                        // Show picker with new selection
                        displayColorPicker(settingValueIndex);
                    } else {
                        if (settingValueIndex >= getSettingValueCount(currentSettingIndex)) {
                            settingValueIndex = 0;  // Wrap around
                        }
                        displaySettingsMenu(currentSettingIndex, settingValueIndex, true);
                    }
                    LOG_DEBUG("Changed to value index %d", settingValueIndex);
                } else {
                    // Not editing - move to next setting
                    currentSettingIndex++;
                    if (currentSettingIndex >= SETTINGS_MENU_COUNT) {
                        currentSettingIndex = 0;  // Wrap around
                    }
                    settingValueIndex = getCurrentValueIndex(currentSettingIndex);
                    displaySettingsMenu(currentSettingIndex, settingValueIndex, false);
                    LOG_DEBUG("Moved to setting %d", currentSettingIndex);
                }
            } else {
                // Normal mode: reserved for future use
                LOG_DEBUG("Button 2 clicked");
            }
            break;
        default:
            break;
    }
}

/**
 * Handle double-click events
 * 
 * Button 1: Currently unused (reserved)
 * Button 2: Enter settings menu mode (only from normal mode)
 * 
 * param btn: Reference to the Button2 object that was double-clicked
 */
void doubleClick(Button2& btn) {
    switch (btn.getPin()) {
        case BUTTON_1_PIN:
            LOG_DEBUG("Button 1 double clicked");
            // Reserved for future use
            break;
        case BUTTON_2_PIN:
            // Only enter settings mode if not already in it
            // This prevents accidentally resetting the menu when navigating quickly
            if (!inSettingsMode) {
                LOG_DEBUG("Button 2 double clicked - entering settings mode");
                enterSettingsMode();
            } else {
                LOG_DEBUG("Button 2 double clicked - already in settings, ignoring");
            }
            break;
        default:
            break;
    }
}

/**
 * Handle triple-click (or more) events
 * 
 * Currently logs the event but takes no action.
 * Reserved for future advanced features.
 * 
 * param btn Reference to the Button2 object that was multi-clicked
 */
void tripleClick(Button2& btn) {
    switch (btn.getPin()) {
        case BUTTON_1_PIN:
            LOG_DEBUG("Button 1 multiple click: ", btn.getNumberOfClicks());
            // Reserved for future use
            break;
        case BUTTON_2_PIN:
            LOG_DEBUG("Button 2 multiple click: ", btn.getNumberOfClicks());
            // Reserved for future use
            break;
        default:
            break;
    }
}

/**
 * Handle long-press detection (fires when threshold reached)
 * 
 * This fires immediately when the button has been held for the
 * long-press threshold (1 second), BEFORE the button is released.
 * Used for immediate feedback on long-press actions.
 * 
 * Button 1: Currently unused (reserved)
 * Button 2: Exit settings mode immediately (for responsive UX)
 * 
 * param btn: Reference to the Button2 object being long-pressed
 */
void longClickDetected(Button2& btn) {
    switch (btn.getPin()) {
        case BUTTON_1_PIN:
            LOG_DEBUG("Button 1 long click detected");
            // Reserved for future use
            break;
        case BUTTON_2_PIN:
            LOG_DEBUG("Button 2 long click detected");
            // Exit settings mode immediately (don't wait for release)
            if (inSettingsMode) {
                LOG_DEBUG("Exiting settings mode immediately");
                exitSettingsMode();
            }
            break;
        default:
            break;
    }
}

/**
 * Handle long-press release (fires when button released after long-press)
 * 
 * This fires when the button is released after being held for the
 * long-press threshold. Most long-press actions happen in longClickDetected()
 * for immediate feedback; this handler is for post-release cleanup if needed.
 * 
 * param btn: Reference to the Button2 object that was long-pressed
 */
void longClick(Button2& btn) {
    switch (btn.getPin()) {
        case BUTTON_1_PIN:
            LOG_DEBUG("Button 1 long click executed");
            // Reserved for future use
            break;
        case BUTTON_2_PIN:
            LOG_DEBUG("Button 2 long click executed");
            // Note: Settings mode exit happens in longClickDetected() 
            break;
        default:
            break;
    }
}

//=====================================
// Settings Management Functions
//=====================================

/**
 * Enter settings menu mode
 * Initializes the settings menu and displays it on screen.
 */
void enterSettingsMode() {
    inSettingsMode = true;
    editingSettingValue = false;
    currentSettingIndex = 0;
    settingValueIndex = getCurrentValueIndex(currentSettingIndex);
    
    // Turn on backlight if off
    if (!backlightStateOn) {
        backlightOn();
    }
    
    // Display the menu
    displaySettingsMenu(currentSettingIndex, settingValueIndex, false);
    
    LOG_DEBUG("Entered settings mode");
}

/**
 * Exit settings menu mode
 * Returns to normal operation and clears the settings display.
 */
void exitSettingsMode() {
    inSettingsMode = false;
    editingSettingValue = false;
    
    // Clear the display
    clearDisplay();
    
    LOG_DEBUG("Exited settings mode");
}


/**
 * Get number of value options for a setting
 * 
 * Returns the count of selectable values for each setting type:
 *   - Indices 0-3 (spell assignments): Number of spells + 1 ("Disabled")
 *   - Index 4 (Add Spell): 1 (it's an action, not a selection)
 *   - Index 5 (Spell Color): Number of predefined colors
 * 
 * param settingIndex: Index of the setting (0-5)
 * return Number of selectable options
 */
int getSettingValueCount(int settingIndex) {
    if (settingIndex == 4) {
        return 1;  // "Add Spell" is an action trigger, not a selection
    }
    
    if (settingIndex == 5) {
        // Color picker - uses predefined color palette
        return getPredefinedColorCount();
    }
    
    // Spell assignment settings (indices 0-3): all spells + "Disabled" option
    return spellPatterns.size() + 1;
}

/**
 * Get current value index for a setting
 * 
 * Looks up the currently saved value for a setting and returns its
 * index in the options list. For spell assignment settings (0-3),
 * this finds the spell name in spellPatterns and returns index+1
 * (because index 0 is reserved for "Disabled").
 * 
 * param settingIndex: Index of the setting (0-5)
 * return Current value index (0 = disabled/not found for spell settings)
 */
int getCurrentValueIndex(int settingIndex) {
    if (settingIndex == 4) {
        return 0;  // "Add Spell" is an action, has no stored value
    }
    
    String currentSpell;
    
    switch (settingIndex) {
        case 0:  // Nightlight ON spell
            currentSpell = NIGHTLIGHT_ON_SPELL;
            break;
        case 1:  // Nightlight OFF spell
            currentSpell = NIGHTLIGHT_OFF_SPELL;
            break;
        case 2:  // Nightlight RAISE spell
            currentSpell = NIGHTLIGHT_RAISE_SPELL;
            break;
        case 3:  // Nightlight LOWER spell
            currentSpell = NIGHTLIGHT_LOWER_SPELL;
            break;
        default:
            return 0;  // Invalid setting
    }
    
    // If empty or disabled, return 0 (Disabled option)
    if (currentSpell.length() == 0 || currentSpell.equalsIgnoreCase("Disabled")) {
        return 0;
    }
    
    // Find the spell in the spell patterns list
    for (size_t i = 0; i < spellPatterns.size(); i++) {
        if (currentSpell.equalsIgnoreCase(spellPatterns[i].name)) {
            return i + 1;  // +1 because index 0 is "Disabled"
        }
    }
    
    // Not found - return disabled
    return 0;
}

/**
 * Find current color index for the Spell Color setting
 * 
 * Maps the currently active spell color to its index in the predefined
 * color palette. Handles the special "Random" mode which is always the
 * last entry in the color list.
 * 
 * return Index into predefined colors (0 to N-1), or 0 if not found
 */
int getCurrentColorIndex() {
    // Special case: Random mode active - return last slot (Random pseudo-entry)
    if (isRandomColorMode()) {
        int count = getPredefinedColorCount();
        return (count > 0) ? (count - 1) : 0;
    }

    // Find current color in the predefined palette
    uint16_t cur = getSpellPrimaryColor();
    int count = getPredefinedColorCount();
    
    // Exclude the final "Random" pseudo-entry when searching for concrete colors
    int concreteCount = (count > 0) ? (count - 1) : 0;
    for (int i = 0; i < concreteCount; ++i) {
        if (getPredefinedColor(i) == cur) return i;
    }
    
    // Color not in palette - default to first color
    return 0;
}


/**
 * Save setting value to non-volatile storage (NVS)
 * 
 * Persists the selected value for a setting. Handles two types:
 *   - Index 5 (Spell Color): Saves color palette index and applies immediately
 *   - Indices 0-3 (Spell assignments): Maps value index to spell name and saves
 * 
 * Value index mapping for spell assignments:
 *   - 0 = Disabled (empty string)
 *   - 1+ = Spell from spellPatterns[valueIndex - 1]
 * 
 * param settingIndex: Index of the setting (0-5)
 * param valueIndex: Selected value index from the options list
 */
void saveSettingValue(int settingIndex, int valueIndex) {
    //--- Handle Spell Color setting (index 5) ---
    if (settingIndex == 5) {
        setSpellPrimaryColorByIndex(valueIndex);                    // Apply color immediately
        setPref(PrefKey::SPELL_PRIMARY_COLOR_INDEX, valueIndex);    // Persist to NVS
        displaySettingsMenu(settingIndex, valueIndex, false);       // Update display
        LOG_DEBUG("Saved Spell Color index %d", valueIndex);
        return;
    }

    //--- Handle Spell Assignment settings (indices 0-3) ---
    // Map value index to spell name
    String spellName;
    if (valueIndex == 0) {
        spellName = "";  // Index 0 = Disabled
    } else if (valueIndex > 0 && valueIndex <= (int)spellPatterns.size()) {
        spellName = spellPatterns[valueIndex - 1].name;  // Index 1+ maps to spell list
    } else {
        LOG_DEBUG("Invalid value index: %d", valueIndex);
        return;
    }

    // Save to appropriate preference based on setting index
    switch (settingIndex) {
        case 0:  // Nightlight ON spell
            NIGHTLIGHT_ON_SPELL = spellName;
            setPref(PrefKey::NIGHTLIGHT_ON_SPELL, spellName);
            LOG_DEBUG("Saved Nightlight ON spell: %s", spellName.c_str());
            break;
        case 1:  // Nightlight OFF spell
            NIGHTLIGHT_OFF_SPELL = spellName;
            setPref(PrefKey::NIGHTLIGHT_OFF_SPELL, spellName);
            LOG_DEBUG("Saved Nightlight OFF spell: %s", spellName.c_str());
            break;
        case 2:  // Nightlight RAISE spell
            NIGHTLIGHT_RAISE_SPELL = spellName;
            setPref(PrefKey::NIGHTLIGHT_RAISE_SPELL, spellName);
            LOG_DEBUG("Saved Nightlight RAISE spell: %s", spellName.c_str());
            break;
        case 3:  // Nightlight LOWER spell
            NIGHTLIGHT_LOWER_SPELL = spellName;
            setPref(PrefKey::NIGHTLIGHT_LOWER_SPELL, spellName);
            LOG_DEBUG("Saved Nightlight LOWER spell: %s", spellName.c_str());
            break;
        default:
            LOG_DEBUG("Invalid setting index: %d", settingIndex);
            break;
    }
}