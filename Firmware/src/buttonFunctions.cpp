/*
This file holds all functions related to the buttons
*/

#include "glyphReader.h"
#include "buttonFunctions.h"
#include "led_control.h"
#include "preferenceFunctions.h"
#include "screenFunctions.h"
#include "spell_patterns.h"

Button2 button1;
Button2 button2;

// Initialize buttons and set handlers
void buttonInit() {

    LOG_DEBUG("Initializing buttons...");

    button1.begin(BUTTON_1_PIN);
    button1.setLongClickTime(1000); // 1 second for long click
    button1.setDoubleClickTime(500); // 0.5 second for double click

    button1.setClickHandler(click);
    button1.setDoubleClickHandler(doubleClick);
    button1.setTripleClickHandler(tripleClick);
    button1.setLongClickDetectedHandler(longClickDetected);
    button1.setLongClickHandler(longClick);
    button1.setLongClickDetectedRetriggerable(false);

    button2.begin(BUTTON_2_PIN);
    button2.setLongClickTime(1000); // 1 second for long click
    button2.setDoubleClickTime(500); // 0.5 second for double click

    button2.setClickHandler(click);
    button2.setDoubleClickHandler(doubleClick);
    button2.setTripleClickHandler(tripleClick);
    button2.setLongClickDetectedHandler(longClickDetected);
    button2.setLongClickHandler(longClick);
    button2.setLongClickDetectedRetriggerable(false);

    LOG_DEBUG("Buttons initialized.");

}

void click(Button2& btn) {
    //LOG_DEBUG("Button %d: Click detected", btn.getPin());
    // switch case depending on which button is pressed
    switch (btn.getPin()) {
        case BUTTON_1_PIN:
            // Handle button 1 click
            if (inSettingsMode) {
                // In settings mode: Button 1 selects/confirms
                if (!editingSettingValue) {
                    // Not editing - enter edit mode for this setting
                    editingSettingValue = true;
                    settingValueIndex = getCurrentValueIndex(currentSettingIndex);
                    displaySettingsMenu(currentSettingIndex, settingValueIndex, true);
                    LOG_DEBUG("Editing setting %d, current value index: %d", currentSettingIndex, settingValueIndex);
                } else {
                    // Editing - confirm and save the value
                    saveSettingValue(currentSettingIndex, settingValueIndex);
                    editingSettingValue = false;
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
        case BUTTON_2_PIN:
            // Handle button 2 click
            if (inSettingsMode) {
                // In settings mode: Button 2 navigates
                if (editingSettingValue) {
                    // Editing - cycle to next value
                    settingValueIndex++;
                    if (settingValueIndex >= getSettingValueCount(currentSettingIndex)) {
                        settingValueIndex = 0;  // Wrap around
                    }
                    displaySettingsMenu(currentSettingIndex, settingValueIndex, true);
                    LOG_DEBUG("Changed to value index %d", settingValueIndex);
                } else {
                    // Not editing - move to next setting
                    currentSettingIndex++;
                    if (currentSettingIndex >= getSettingsCount()) {
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

void doubleClick(Button2& btn) {
    switch (btn.getPin()) {
        case BUTTON_1_PIN:
            LOG_DEBUG("Button 1 double clicked");
            break;
        case BUTTON_2_PIN:
            LOG_DEBUG("Button 2 double clicked - entering settings mode");
            enterSettingsMode();
            break;
        default:
            break;
    }
}

void tripleClick(Button2& btn) {
    switch (btn.getPin()) {
        case BUTTON_1_PIN:
            LOG_DEBUG("Button 1 multiple click: ", btn.getNumberOfClicks());
            break;
        case BUTTON_2_PIN:
            LOG_DEBUG("Button 2 multiple click: ", btn.getNumberOfClicks());
            break;
        default:
            break;
    }
}

void longClickDetected(Button2& btn) {
    switch (btn.getPin()) {
        case BUTTON_1_PIN:
            LOG_DEBUG("Button 1 long click detected");
            break;
        case BUTTON_2_PIN:
            LOG_DEBUG("Button 2 long click detected");
            break;
        default:
            break;
    }
}

void longClick(Button2& btn) {
    switch (btn.getPin()) {
        case BUTTON_1_PIN:
            LOG_DEBUG("Button 1 long click executed");
            break;
        case BUTTON_2_PIN:
            if (inSettingsMode) {
                LOG_DEBUG("Button 2 long click - exiting settings mode");
                exitSettingsMode();
            } else {
                LOG_DEBUG("Button 2 long click executed");
            }
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
 * Get total number of configurable settings
 * Currently: 2 (Nightlight ON spell, Nightlight OFF spell)
 */
int getSettingsCount() {
    return 2;  // NL ON, NL OFF
}

/**
 * Get number of value options for a setting
 * Returns the number of spells + 1 (for "Disabled" option)
 */
int getSettingValueCount(int settingIndex) {
    // All settings use spell list + "Disabled"
    return spellPatterns.size() + 1;
}

/**
 * Get current value index for a setting
 * Looks up the saved spell name and finds its index in the spell list.
 * Returns 0 if disabled or spell not found.
 */
int getCurrentValueIndex(int settingIndex) {
    String currentSpell;
    
    switch (settingIndex) {
        case 0:  // Nightlight ON spell
            currentSpell = NIGHTLIGHT_ON_SPELL;
            break;
        case 1:  // Nightlight OFF spell
            currentSpell = NIGHTLIGHT_OFF_SPELL;
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
 * Save setting value to preferences
 * Converts value index to spell name and saves to NVS.
 */
void saveSettingValue(int settingIndex, int valueIndex) {
    String spellName;
    
    // Convert value index to spell name
    if (valueIndex == 0) {
        spellName = "";  // Disabled
    } else if (valueIndex > 0 && valueIndex <= (int)spellPatterns.size()) {
        spellName = spellPatterns[valueIndex - 1].name;
    } else {
        LOG_DEBUG("Invalid value index: %d", valueIndex);
        return;
    }
    
    // Save to appropriate preference
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
        default:
            LOG_DEBUG("Invalid setting index: %d", settingIndex);
            break;
    }
}

