/**
================================================================================
  Web Functions - WiFiManager Configuration Portal 
================================================================================
 * 
 * This module implements a captive portal configuration interface using WiFiManager
 * library. Provides web-based GUI for configuring WiFi credentials, MQTT settings,
 * detection tuning parameters, and nightlight control spells.
 * 
 * ARCHITECTURE:
 * - WiFiManager library creates AP mode captive portal on first boot or failed WiFi
 * - Custom HTML form fields added for all user-configurable parameters
 * - JavaScript +/- buttons for numeric adjuster controls (no manual typing needed)
 * - Dropdown menus populated dynamically from spellPatterns vector
 * - Settings saved to NVS flash via preferenceFunctions.cpp
 * - Device reboots after save to apply new settings
 * 
 * USER INTERFACE SECTIONS:
 * 1. WiFi Configuration (standard WiFiManager)
 *    - SSID selection from scan
 *    - Password entry
 * 
 * 2. MQTT Broker Settings
 *    - Server address (IP or hostname)
 *    - Port number (default 1883)
 *    - Topic for spell publishing
 * 
 * 3. Nightlight Configuration
 *    - Spell to turn nightlight ON (dropdown)
 *    - Spell to turn nightlight OFF (dropdown)
 *    - When nightlight active, LEDs return to warm white instead of turning off
 * 
 * 4. Tuning Parameters (numeric adjusters with +/- buttons)
 *    - Movement Threshold: Min pixels to start tracking (default 15)
 *    - Stillness Threshold: Max pixels considered \"still\" (default 20)
 *    - Ready Stillness Time: How long still to enter READY state (default 600ms)
 *    - End Stillness Time: How long still to end gesture (default 500ms)
 *    - Gesture Timeout: Max tracking duration (default 5000ms)
 *    - IR Loss Timeout: Max time IR can be lost (default 300ms)
 * 
 * PORTAL ACCESS:
 * - First boot: Creates \"GlyphReader-Setup\" WiFi AP
 * - Connect to AP, navigate to 192.168.4.1
 * - After WiFi configured: Access via http://<device-ip>/
 * - Can also access via mDNS: http://glyphreader.local/ (if mDNS configured)
 * 
 * TIMEOUT BEHAVIOR:
 * - Config portal timeout: 30 seconds (then continues boot without WiFi)
 * - Connect timeout: 20 seconds (for connecting to saved WiFi)
 * - Non-blocking: Device continues operation even if WiFi fails
 * - Portal remains available after boot for configuration changes
 * 
 */

#include "webFunctions.h"
#include <WiFiManager.h>
#include "preferenceFunctions.h"
#include "glyphReader.h"
#include "spell_patterns.h"
#include "customSpellFunctions.h"
#include "customSpellFunctions.h"
#include "sdFunctions.h"
#include <cctype>

// WiFiManager instance
WiFiManager wm;

// Flags for background saving
bool pendingSaveToPreferences = false;
bool pendingSaveCustomSpellsToSD = false;

// Storage for custom spell renames (pending SD save)
struct CustomSpellRename {
    String oldName;
    String newName;
};
std::vector<CustomSpellRename> pendingSpellRenames;

// Buffers for stored values
char mqttServerBuffer[20] = "";
char mqttPortBuffer[6] = "";
char mqttTopicBuffer[50] = "";

// Buffers for location override
char latitudeBuffer[20] = "";
char longitudeBuffer[20] = "";
char timezoneBuffer[10] = "";

//Custom Text Parameters to explain each variable
WiFiManagerParameter custom_Header_Text("<h1>Glyph Reader Settings</h1>");
WiFiManagerParameter custom_MQTT_Text("<p>Enter your MQTT Broker settings below:</p>");
WiFiManagerParameter custom_Topic_Text("<p>MQTT Topic to publish recognized spells</p>");
WiFiManagerParameter custom_Nightlight_Header_Text("<h2>Nightlight Configuration</h2>");
WiFiManagerParameter custom_Nightlight_Text("<p>Select spells to turn nightlight on/off. When active, LEDs return to nightlight instead of turning off.</p>");
WiFiManagerParameter custom_Location_Header_Text("<h2>Location Override</h2>");
WiFiManagerParameter custom_Location_Text("<p>Override auto-detected location for sunrise/sunset calculations.</p>");
WiFiManagerParameter custom_Sound_Header_Text("<h2>Sound Settings</h2>");
WiFiManagerParameter custom_Sound_Text("<p>Enable or disable sound effects for spell detection and system events.</p>");
WiFiManagerParameter custom_Tuning_Header_Text("<h2>Tuning Parameters for Spell Detection</h2>");
WiFiManagerParameter custom_Start_Movement_text("<p>Minimum pixels to consider motion to start tracking</p>");
WiFiManagerParameter custom_stillness_text("<p>Maximum Pixels to consider the wand stationary and initiate the spell tracking</p>");
WiFiManagerParameter custom_Start_Stillness_Time_text("<p>How long the wand needs to be still to initiate the device</p>");
WiFiManagerParameter custom_End_STillness_Time_text("<p>How long the wand needs to remain still to end tracking</p>");
WiFiManagerParameter custom_Max_Gesture_Time_text("<p>Maximum time to track a spell before timing out</p>");
WiFiManagerParameter custom_IR_Loss_Timeout_text("<p>Max time tracking can be lost before tracking is ended</p>");
WiFiManagerParameter custom_User_Spell_Names_Header_Text("<h2>Custom Spell Names</h2>");
WiFiManagerParameter custom_User_Spell_Names_Text("<p>Rename custom spells recorded via the device.</p>");



String generateAdjusterHTML(const char* settingName, int startValue, int stepValue) {
  String html = "<div style='margin: 10px 0; padding: 10px; border: 1px solid #ddd; border-radius: 5px;'>";
  html += "<label style='display: block; margin-bottom: 5px; font-weight: bold;'>" + String(settingName) + "</label>";
  html += "<div style='display: flex; align-items: center; gap: 10px;'>";
  
  // Decrement button
  html += "<button type='button' onclick='adjust(\"" + String(settingName) + "\", -" + String(stepValue) + ")' ";
  html += "style='width: 40px; height: 40px; font-size: 20px; cursor: pointer; background: #f44336; color: white; border: none; border-radius: 5px;'>-</button>";
  
  // Value display/input
  html += "<input type='number' name='" + String(settingName) + "' id='" + String(settingName) + "' ";
  html += "value='" + String(startValue) + "' ";
  html += "style='width: 100px; height: 40px; text-align: center; font-size: 18px; border: 2px solid #ccc; border-radius: 5px;' ";
  html += "step='" + String(stepValue) + "' />";
  
  // Increment button
  html += "<button type='button' onclick='adjust(\"" + String(settingName) + "\", " + String(stepValue) + ")' ";
  html += "style='width: 40px; height: 40px; font-size: 20px; cursor: pointer; background: #4CAF50; color: white; border: none; border-radius: 5px;'>+</button>";
  
  html += "</div></div>";
  
  // Add JavaScript for the adjust function (only once)
  static bool scriptAdded = false;
  if (!scriptAdded) {
    html += "<script>function adjust(id, step) { ";
    html += "var input = document.getElementById(id); ";
    html += "var val = parseInt(input.value) || 0; ";
    html += "input.value = val + step; ";
    html += "}</script>";
    scriptAdded = true;
  }
  
  return html;
}

String generateSpellDropdown(const char* settingName, const String& currentValue) {
  LOG_DEBUG("Generating dropdown for %s, spellPatterns.size()=%d", settingName, spellPatterns.size());
  
  String html = "<div style='margin: 10px 0; padding: 10px; border: 1px solid #ddd; border-radius: 5px;'>";
  html += "<label style='display: block; margin-bottom: 5px; font-weight: bold;'>" + String(settingName) + "</label>";
  html += "<select name='" + String(settingName) + "' style='width: 100%; padding: 8px; font-size: 16px; border: 2px solid #ccc; border-radius: 5px;'>";
  html += "<option value=''>-- None --</option>";
  
  // Add an option for each spell
  for (size_t i = 0; i < spellPatterns.size(); i++) {
    const char* spellName = spellPatterns[i].name;
    LOG_DEBUG("  Adding spell: %s", spellName);
    html += "<option value='" + String(spellName) + "'";
    if (currentValue == spellName) {
      html += " selected";
    }
    html += ">" + String(spellName) + "</option>";
  }
  
  html += "</select></div>";
  return html;
}

// Custom Parameter Fields for MQTT Settings
WiFiManagerParameter custom_mqtt_server("mqtt_server", "MQTT Broker Address", mqttServerBuffer, 20);
WiFiManagerParameter custom_mqtt_port("mqtt_port", "MQTT Broker Port", mqttPortBuffer, 6, "1883");
WiFiManagerParameter custom_mqtt_topic("mqtt_topic", "MQTT Topic", mqttTopicBuffer, 50);

// Custom Parameter Fields for Location Override
WiFiManagerParameter custom_latitude("latitude", "Latitude (decimal degrees)", latitudeBuffer, 20);
WiFiManagerParameter custom_longitude("longitude", "Longitude (decimal degrees)", longitudeBuffer, 20);
WiFiManagerParameter custom_timezone("timezone", "Timezone Offset (hours from UTC)", timezoneBuffer, 10);

// Generate Adjusters
String movementThresholdHTML = generateAdjusterHTML("Movement Threshold", MOVEMENT_THRESHOLD, 1);
String stillnessThresholdHTML = generateAdjusterHTML("Stillness Threshold", STILLNESS_THRESHOLD, 1);
String readyStillnessTimeHTML = generateAdjusterHTML("Ready Stillness Time", READY_STILLNESS_TIME, 50);
String gestureTimeoutHTML = generateAdjusterHTML("Gesture Timeout", GESTURE_TIMEOUT, 500);
String irLossTimeoutHTML = generateAdjusterHTML("IR Loss Timeout", IR_LOSS_TIMEOUT, 50);

// Declare dropdown strings - will be populated by generateDropdowns() before WiFiManager uses them
String nightlightOnHTML;
String nightlightOffHTML;
String nightlightRaiseHTML;
String nightlightLowerHTML;

WiFiManagerParameter custom_Movement_thresh_adjust(movementThresholdHTML.c_str());
WiFiManagerParameter custom_Stillness_thresh_adjust(stillnessThresholdHTML.c_str());
WiFiManagerParameter custom_Stillness_time_adjust(readyStillnessTimeHTML.c_str());
WiFiManagerParameter custom_Gesture_timout_adjust(gestureTimeoutHTML.c_str());
WiFiManagerParameter custom_IR_Loss_timeout_adjust(irLossTimeoutHTML.c_str());
WiFiManagerParameter custom_Nightlight_On_Dropdown(nightlightOnHTML.c_str());
WiFiManagerParameter custom_Nightlight_Off_Dropdown(nightlightOffHTML.c_str());
WiFiManagerParameter custom_Nightlight_Raise_Dropdown(nightlightRaiseHTML.c_str());
WiFiManagerParameter custom_Nightlight_Lower_Dropdown(nightlightLowerHTML.c_str());

// Sound settings checkbox (custom HTML will be set in loadCustomParameters)
WiFiManagerParameter custom_sound_enabled("sound_enabled", "Enable Sound Effects", "T", 2, "type=\"checkbox\"", WFM_LABEL_AFTER);

WiFiManagerParameter* showAdvOptsBtn = new WiFiManagerParameter("<button id=\"showadvopts\">Show Advanced Options</button>");


// Custom spell name fields
static std::vector<WiFiManagerParameter*> customSpellNameParams;
// Persistent labels for custom spell parameters (avoid temporary c_str() usage)
static std::vector<String> customSpellParamLabels;
// Store original spell names corresponding to each generated field
static std::vector<String> customSpellOriginalNames;

void generateAdjusters(){
    // Generate HTML with current preference values - populate strings FIRST
    movementThresholdHTML = generateAdjusterHTML("Movement Threshold (pixels)", MOVEMENT_THRESHOLD, 1);
    stillnessThresholdHTML = generateAdjusterHTML("Stillness Threshold (pixels)", STILLNESS_THRESHOLD, 1);
    readyStillnessTimeHTML = generateAdjusterHTML("Ready Stillness Time (milliseconds)", READY_STILLNESS_TIME, 50);
    gestureTimeoutHTML = generateAdjusterHTML("Gesture Timeout (milliseconds)", GESTURE_TIMEOUT, 500);
    irLossTimeoutHTML = generateAdjusterHTML("IR Loss Timeout (milliseconds)", IR_LOSS_TIMEOUT, 50);
}

void generateDropdowns(){
    nightlightOnHTML = generateSpellDropdown("Nightlight On Spell", NIGHTLIGHT_ON_SPELL);
    nightlightOffHTML = generateSpellDropdown("Nightlight Off Spell", NIGHTLIGHT_OFF_SPELL);
    nightlightRaiseHTML = generateSpellDropdown("Nightlight Raise Spell", NIGHTLIGHT_RAISE_SPELL);
    nightlightLowerHTML = generateSpellDropdown("Nightlight Lower Spell", NIGHTLIGHT_LOWER_SPELL);
}


// Load Settings from Preferences
void LoadCustomParameters() {
    strcpy(mqttServerBuffer, MQTT_HOST.c_str());
    snprintf(mqttPortBuffer, sizeof(mqttPortBuffer), "%d", MQTT_PORT);
    strcpy(mqttTopicBuffer, MQTT_TOPIC.c_str());
    custom_mqtt_server.setValue(mqttServerBuffer, 20);
    custom_mqtt_port.setValue(mqttPortBuffer, 6);
    custom_mqtt_topic.setValue(mqttTopicBuffer, 50);
    
    // Populate location buffers with current values
    strcpy(latitudeBuffer, LATITUDE.c_str());
    strcpy(longitudeBuffer, LONGITUDE.c_str());
    snprintf(timezoneBuffer, sizeof(timezoneBuffer), "%d", TIMEZONE_OFFSET / 3600);  // Convert seconds to hours for display
    custom_latitude.setValue(latitudeBuffer, 20);
    custom_longitude.setValue(longitudeBuffer, 20);
    custom_timezone.setValue(timezoneBuffer, 10);
    
    // Recreate checkbox with correct checked state
    // WiFiManager requires "checked" in the custom HTML string
    if (SOUND_ENABLED) {
        new (&custom_sound_enabled) WiFiManagerParameter("sound_enabled", "Enable Sound Effects", "T", 2, "type=\"checkbox\" checked", WFM_LABEL_AFTER);
    } else {
        new (&custom_sound_enabled) WiFiManagerParameter("sound_enabled", "Enable Sound Effects", "T", 2, "type=\"checkbox\"", WFM_LABEL_AFTER);
    }
    
    generateAdjusters();
    generateDropdowns();
    
    // Reconstruct adjuster parameters with updated HTML using placement new
    new (&custom_Movement_thresh_adjust) WiFiManagerParameter(movementThresholdHTML.c_str());
    new (&custom_Stillness_thresh_adjust) WiFiManagerParameter(stillnessThresholdHTML.c_str());
    new (&custom_Stillness_time_adjust) WiFiManagerParameter(readyStillnessTimeHTML.c_str());
    new (&custom_Gesture_timout_adjust) WiFiManagerParameter(gestureTimeoutHTML.c_str());
    new (&custom_IR_Loss_timeout_adjust) WiFiManagerParameter(irLossTimeoutHTML.c_str());
    
    // Reconstruct dropdown parameters with updated HTML using placement new
    new (&custom_Nightlight_On_Dropdown) WiFiManagerParameter(nightlightOnHTML.c_str());
    new (&custom_Nightlight_Off_Dropdown) WiFiManagerParameter(nightlightOffHTML.c_str());
    new (&custom_Nightlight_Raise_Dropdown) WiFiManagerParameter(nightlightRaiseHTML.c_str());
    new (&custom_Nightlight_Lower_Dropdown) WiFiManagerParameter(nightlightLowerHTML.c_str());
}

void saveCustomParameters() {
    LOG_DEBUG("Processing web form parameters...");
    // Dump all posted form args for debugging
    int postedArgs = wm.server->args();
    LOG_DEBUG("POST: arg count=%d", postedArgs);
    for (int ai = 0; ai < postedArgs; ++ai) {
        String an = wm.server->argName(ai);
        String av = wm.server->arg(ai);
        LOG_DEBUG("POST arg[%d]=%s='%s'", ai, an.c_str(), av.c_str());
    }
    // Collect indices of truly unnamed or garbled POST args.
    // Consider an arg "unnamed" only if its name is empty or contains non-printable characters.
    std::vector<int> unnamedArgIndices;
    for (int ai = 0; ai < postedArgs; ++ai) {
        String an = wm.server->argName(ai);
        bool valid = (an.length() > 0);
        for (size_t ci = 0; ci < an.length() && valid; ++ci) {
            if (!std::isprint((unsigned char)an[ci])) valid = false;
        }
        if (!valid) unnamedArgIndices.push_back(ai);
    }
    if (!unnamedArgIndices.empty()) {
        LOG_DEBUG("Found %d unnamed/garbled POST args; indices start=%d end=%d", (int)unnamedArgIndices.size(), unnamedArgIndices.front(), unnamedArgIndices.back());
    }
    
    // Read all form values and update global variables immediately
    // This allows instant regeneration of HTML with new values
    
    //-----------------------------------
    // Location Settings
    //-----------------------------------
    String newLatitude = wm.server->arg("latitude") ? wm.server->arg("latitude") : "";
    String newLongitude = wm.server->arg("longitude") ? wm.server->arg("longitude") : "";
    String newTimezoneOffset = wm.server->arg("timezone") ? wm.server->arg("timezone") : "";
    int timezoneOffsetSeconds = newTimezoneOffset.toInt() * 3600;

    if (newLatitude.length() > 0 && newLatitude != LATITUDE) {
        LATITUDE = newLatitude;
        pendingSaveToPreferences = true;
    }
    if (newLongitude.length() > 0 && newLongitude != LONGITUDE) {
        LONGITUDE = newLongitude;
        pendingSaveToPreferences = true;
    }
    if (newTimezoneOffset.length() > 0 && timezoneOffsetSeconds != TIMEZONE_OFFSET) {
        TIMEZONE_OFFSET = timezoneOffsetSeconds;
        pendingSaveToPreferences = true;
    }

    //-----------------------------------
    // Sound Settings
    //-----------------------------------
    bool newSoundEnabled = wm.server->hasArg("sound_enabled");
    if (newSoundEnabled != SOUND_ENABLED) {
        SOUND_ENABLED = newSoundEnabled;
        pendingSaveToPreferences = true;
        LOG_DEBUG("Sound setting changed to: %s", newSoundEnabled ? "enabled" : "disabled");
    }

    //-----------------------------------
    // MQTT Settings
    //-----------------------------------
    String newMqttHost = wm.server->arg("mqtt_server") ? wm.server->arg("mqtt_server") : "";
    int newMqttPort = wm.server->arg("mqtt_port") ? wm.server->arg("mqtt_port").toInt() : 0;
    String newMqttTopic = wm.server->arg("mqtt_topic") ? wm.server->arg("mqtt_topic") : "";

    if (newMqttHost.length() > 0 && newMqttHost != MQTT_HOST) {
        MQTT_HOST = newMqttHost;
        pendingSaveToPreferences = true;
    }
    if (newMqttPort > 0 && newMqttPort != MQTT_PORT) {
        MQTT_PORT = newMqttPort;
        pendingSaveToPreferences = true;
    }  
    if (newMqttTopic.length() > 0 && newMqttTopic != MQTT_TOPIC) {
        MQTT_TOPIC = newMqttTopic;
        pendingSaveToPreferences = true;
    }

    //-----------------------------------
    // Nightlight Spell Settings
    //-----------------------------------
    String newNightlightOn = wm.server->arg("Nightlight On Spell");
    String newNightlightOff = wm.server->arg("Nightlight Off Spell");
    String newNightlightRaise = wm.server->arg("Nightlight Raise Spell");
    String newNightlightLower = wm.server->arg("Nightlight Lower Spell");

    if (newNightlightOn != NIGHTLIGHT_ON_SPELL) {
        NIGHTLIGHT_ON_SPELL = newNightlightOn;
        pendingSaveToPreferences = true;
    }
    if (newNightlightOff != NIGHTLIGHT_OFF_SPELL) {
        NIGHTLIGHT_OFF_SPELL = newNightlightOff;
        pendingSaveToPreferences = true;
    }
    if (newNightlightRaise != NIGHTLIGHT_RAISE_SPELL) {
        NIGHTLIGHT_RAISE_SPELL = newNightlightRaise;
        pendingSaveToPreferences = true;
    }
    if (newNightlightLower != NIGHTLIGHT_LOWER_SPELL) {
        NIGHTLIGHT_LOWER_SPELL = newNightlightLower;
        pendingSaveToPreferences = true;
    }

    //-----------------------------------
    // Tuning Parameters
    //-----------------------------------
    int newMovementThreshold = wm.server->arg("Movement Threshold (pixels)").toInt();
    int newStillnessThreshold = wm.server->arg("Stillness Threshold (pixels)").toInt();
    int newReadyStillnessTime = wm.server->arg("Ready Stillness Time (milliseconds)").toInt();
    int newGestureTimeout = wm.server->arg("Gesture Timeout (milliseconds)").toInt();
    int newIRLossTimeout = wm.server->arg("IR Loss Timeout (milliseconds)").toInt();

    if (newMovementThreshold > 0 && newMovementThreshold != MOVEMENT_THRESHOLD) {
        MOVEMENT_THRESHOLD = newMovementThreshold;
        pendingSaveToPreferences = true;
    }
    if (newStillnessThreshold > 0 && newStillnessThreshold != STILLNESS_THRESHOLD) {
        STILLNESS_THRESHOLD = newStillnessThreshold;
        pendingSaveToPreferences = true;
    }
    if (newReadyStillnessTime > 0 && newReadyStillnessTime != READY_STILLNESS_TIME) {
        READY_STILLNESS_TIME = newReadyStillnessTime;
        pendingSaveToPreferences = true;
    }
    if (newGestureTimeout > 0 && newGestureTimeout != GESTURE_TIMEOUT) {
        GESTURE_TIMEOUT = newGestureTimeout;
        pendingSaveToPreferences = true;
    }
    if (newIRLossTimeout > 0 && newIRLossTimeout != IR_LOSS_TIMEOUT) {
        IR_LOSS_TIMEOUT = newIRLossTimeout;
        pendingSaveToPreferences = true;
    }

    //-----------------------------------
    // Custom Spell Rename Handling (read generated fields)
    //-----------------------------------
    if (numCustomSpells > 0) {
        // Build list of current custom spell names from tail of spellPatterns
        std::vector<String> currentCustomNames;
        if (spellPatterns.size() >= (size_t)numCustomSpells) {
            size_t start = spellPatterns.size() - (size_t)numCustomSpells;
            for (size_t i = start; i < spellPatterns.size(); ++i) {
                currentCustomNames.push_back(String(spellPatterns[i].name));
            }
        } else {
            for (size_t i = 0; i < spellPatterns.size(); ++i) {
                if (strncmp(spellPatterns[i].name, "Custom ", 7) == 0) currentCustomNames.push_back(String(spellPatterns[i].name));
            }
        }

        for (int i = 0; i < numCustomSpells; ++i) {
            String newName = "";
            // Parameter IDs are alphanumeric: customspell1, customspell2, ...
            String fieldId = "customspell" + String(i + 1);
            if (wm.server->hasArg(fieldId)) {
                newName = wm.server->arg(fieldId);
            } else if (i < (int)customSpellNameParams.size() && customSpellNameParams[i]) {
                const char* val = customSpellNameParams[i]->getValue();
                if (val) newName = String(val);
            }

            // Fallback: some WiFiManager builds send unnamed arg names for custom parameters.
            // If we didn't find the value by name, try mapping to the sequence of unnamed args.
            if (newName.length() == 0 && unnamedArgIndices.size() > (size_t)i) {
                int argIdx = unnamedArgIndices[i];
                String posVal = wm.server->arg(argIdx);
                if (posVal.length() > 0) {
                    newName = posVal;
                    LOG_DEBUG("Using unnamed POST arg for custom field %d -> '%s' (argIdx=%d)", i, newName.c_str(), argIdx);
                }
            }

            // Write back into the in-memory WiFiManagerParameter so redirect shows updated values
            if (i < (int)customSpellNameParams.size() && customSpellNameParams[i]) {
                if (newName.length() > 0) {
                    int maxlen = 40; // buffer size used when creating the parameter
                    int copylen = min((int)newName.length(), maxlen - 1);
                    customSpellNameParams[i]->setValue(newName.c_str(), copylen + 1);
                    LOG_DEBUG("Updated in-memory param[%d] value='%s'", i, newName.c_str());
                } else {
                    LOG_DEBUG("Skipped updating in-memory param[%d] because newName is empty", i);
                }
            }

            String oldName = "";
            if (i < (int)customSpellOriginalNames.size()) {
                oldName = customSpellOriginalNames[i];
            } else if (i < (int)currentCustomNames.size()) {
                oldName = currentCustomNames[i];
            }
            if (newName.length() > 0 && oldName.length() > 0 && newName != oldName) {
                pendingSpellRenames.push_back({oldName, newName});
                pendingSaveCustomSpellsToSD = true;
                LOG_DEBUG("Queued rename: '%s' -> '%s'", oldName.c_str(), newName.c_str());
            }
        }
    }

    //-----------------------------------
    // Regenerate HTML with new values
    //-----------------------------------
    // This updates all the WiFiManager parameters with current values
    // so the redirect shows the updated settings immediately
    LoadCustomParameters();
    
    LOG_DEBUG("Settings updated in memory, flagged for background save");

    //-----------------------------------
    // Send immediate redirect response
    //-----------------------------------
    String responseHTML = "<html><head>";
    responseHTML += "<meta http-equiv='refresh' content='2;url=/param'>";
    responseHTML += "<style>body{font-family:sans-serif;text-align:center;padding:20px;}</style>";
    responseHTML += "</head><body>";
    responseHTML += "<h2>Settings Saved!</h2>";
    responseHTML += "<p>Returning to settings page...</p>";
    responseHTML += "</body></html>";

    wm.server->send(200, "text/html", responseHTML);

    // Force portal to rebuild fields after save
    wm.stopConfigPortal();
    delay(100); // Give time for cleanup
    wm.startWebPortal();
}

/**
 * Process background saves to NVS preferences and SD card
 * 
 * Called from main loop to handle deferred saves after web portal updates.
 * This prevents blocking the HTTP response with slow SD card/NVS writes.
 * 
 * Saves all changed settings to persistent storage:
 *   - NVS flash: MQTT, location, sound, nightlight, tuning parameters
 *   - SD card: Custom spell renames (if any pending)
 */
void processBackgroundSaves() {
    // Save to NVS preferences if flagged
    if (pendingSaveToPreferences) {
        LOG_DEBUG("Background save: Writing settings to NVS preferences...");
        
        // MQTT settings
        setPref(PrefKey::MQTT_HOST, MQTT_HOST);
        setPref(PrefKey::MQTT_PORT, MQTT_PORT);
        setPref(PrefKey::MQTT_TOPIC, MQTT_TOPIC);
        
        // Location settings
        setPref(PrefKey::LATITUDE, LATITUDE);
        setPref(PrefKey::LONGITUDE, LONGITUDE);
        setPref(PrefKey::TIMEZONE_OFFSET, TIMEZONE_OFFSET);
        
        // Sound settings
        setPref(PrefKey::SOUND_ENABLED, SOUND_ENABLED);
        
        // Nightlight spell settings
        setPref(PrefKey::NIGHTLIGHT_ON_SPELL, NIGHTLIGHT_ON_SPELL);
        setPref(PrefKey::NIGHTLIGHT_OFF_SPELL, NIGHTLIGHT_OFF_SPELL);
        setPref(PrefKey::NIGHTLIGHT_RAISE_SPELL, NIGHTLIGHT_RAISE_SPELL);
        setPref(PrefKey::NIGHTLIGHT_LOWER_SPELL, NIGHTLIGHT_LOWER_SPELL);
        
        // Tuning parameters
        setPref(PrefKey::MOVEMENT_THRESHOLD, MOVEMENT_THRESHOLD);
        setPref(PrefKey::STILLNESS_THRESHOLD, STILLNESS_THRESHOLD);
        setPref(PrefKey::READY_STILLNESS_TIME, READY_STILLNESS_TIME);
        setPref(PrefKey::GESTURE_TIMEOUT, GESTURE_TIMEOUT);
        setPref(PrefKey::IR_LOSS_TIMEOUT, IR_LOSS_TIMEOUT);
        
        pendingSaveToPreferences = false;
        LOG_DEBUG("Background save: NVS preferences updated successfully");
    }
    
    // Save custom spell renames to SD card if flagged
    if (pendingSaveCustomSpellsToSD && pendingSpellRenames.size() > 0) {
        LOG_DEBUG("Background save: Processing %d spell renames to SD card (batch)", pendingSpellRenames.size());

        // Convert to the local header's pair type
        std::vector<SpellRenamePair> batch;
        for (const auto& r : pendingSpellRenames) batch.push_back({r.oldName, r.newName});

        if (renameCustomSpellsBatch(batch)) {
            LOG_DEBUG("Background save: Batch rename applied successfully");
        } else {
            LOG_ALWAYS("Background save: Batch rename failed");
            // Fallback: try individual renames
            for (const auto& rename : pendingSpellRenames) {
                if (renameCustomSpell(rename.oldName.c_str(), rename.newName.c_str())) {
                    LOG_DEBUG("  Renamed '%s' to '%s'", rename.oldName.c_str(), rename.newName.c_str());
                } else {
                    LOG_ALWAYS("  Failed to rename '%s'", rename.oldName.c_str());
                }
            }
        }

        pendingSpellRenames.clear();
        pendingSaveCustomSpellsToSD = false;
        LOG_DEBUG("Background save: Custom spell renames complete");
    }
}

bool initWM() {
    LOG_DEBUG("Initializing WiFiManager...");
    WiFi.mode(WIFI_STA); // Set WiFi to station mode

    // Load stored parameters into buffers (also regenerates dropdowns)
    LoadCustomParameters();

    wm.setConfigPortalBlocking(false); // Non-blocking - device continues even if WiFi fails
    wm.setConfigPortalTimeout(20); // Config portal timeout: 60 seconds
    wm.setCaptivePortalEnable(true); // Enable Captive Portal

    // Set WiFiManager Title
    wm.setTitle("Glyph Reader Configuration Portal");
    wm.setClass("invert");

    // add custom fields
    wm.addParameter(&custom_Header_Text);
    wm.addParameter(&custom_MQTT_Text);
    wm.addParameter(&custom_mqtt_server);
    wm.addParameter(&custom_mqtt_port);
    wm.addParameter(&custom_Topic_Text);
    wm.addParameter(&custom_mqtt_topic);
    wm.addParameter(&custom_Nightlight_Header_Text);
    wm.addParameter(&custom_Nightlight_Text);
    wm.addParameter(&custom_Nightlight_On_Dropdown);
    wm.addParameter(&custom_Nightlight_Off_Dropdown);
    wm.addParameter(&custom_Nightlight_Raise_Dropdown);
    wm.addParameter(&custom_Nightlight_Lower_Dropdown);
    wm.addParameter(&custom_Sound_Header_Text);
    wm.addParameter(&custom_sound_enabled);
    // Insert custom spell rename UI here (header, description, and fields)
    wm.addParameter(&custom_User_Spell_Names_Header_Text);
    wm.addParameter(&custom_User_Spell_Names_Text);
    // Build and add custom spell fields here (moved from lower in function)
    // Clean up any previous custom spell name fields and labels
    for (auto* param : customSpellNameParams) {
        delete param;
    }
    customSpellNameParams.clear();
    customSpellParamLabels.clear();
    customSpellOriginalNames.clear();
    // Reserve capacity to avoid reallocation which would invalidate c_str() pointers
    if (numCustomSpells > 0) {
        customSpellParamLabels.reserve((size_t)numCustomSpells);
        customSpellNameParams.reserve((size_t)numCustomSpells);
        customSpellOriginalNames.reserve((size_t)numCustomSpells);
    }
    
    // Build list of current custom spell names from the tail of spellPatterns
    std::vector<String> customNames;
    if (numCustomSpells > 0 && spellPatterns.size() >= (size_t)numCustomSpells) {
        size_t start = spellPatterns.size() - (size_t)numCustomSpells;
        for (size_t i = start; i < spellPatterns.size(); ++i) {
            customNames.push_back(String(spellPatterns[i].name));
        }
    } else {
        // Fallback: scan for any names that look like custom entries
        for (size_t i = 0; i < spellPatterns.size(); ++i) {
            const char* spellName = spellPatterns[i].name;
            if (strncmp(spellName, "Custom ", 7) == 0) customNames.push_back(String(spellName));
        }
    }

    LOG_DEBUG("initWM: numCustomSpells=%d, discovered customNames.size()=%d, spellPatterns.size()=%d", numCustomSpells, (int)customNames.size(), (int)spellPatterns.size());

    // Generate custom spell name fields using numCustomSpells (prefill with discovered names)
    for (int i = 0; i < numCustomSpells; ++i) {
        // Use alphanumeric-only parameter IDs to satisfy WiFiManager validation
        String fieldName = "customspell" + String(i + 1);
        String value = (i < customNames.size()) ? customNames[i] : "";
        LOG_DEBUG("initWM: creating custom param idx=%d name=%s value='%s'", i, fieldName.c_str(), value.c_str());
        char* buf = new char[40];
        strncpy(buf, value.c_str(), 39);
        buf[39] = '\0';
        // Create persistent label string and store it so c_str() remains valid
        customSpellParamLabels.push_back(String("Custom Spell ") + String(i + 1));
        // Record original name for this field so we can identify the correct spell to rename later
        customSpellOriginalNames.push_back(value);
        const char* labelCStr = customSpellParamLabels.back().c_str();
        LOG_DEBUG("initWM: label[%d]=%s addr=%p", i, labelCStr, (void*)labelCStr);
        WiFiManagerParameter* param = new WiFiManagerParameter(fieldName.c_str(), labelCStr, buf, 40);
        customSpellNameParams.push_back(param);
        LOG_DEBUG("initWM: pushed param ptr=%p", (void*)param);
    }

    // Add custom spell name fields to portal
    for (auto* param : customSpellNameParams) {
        wm.addParameter(param);
    }
    wm.addParameter(&custom_Location_Header_Text);
    wm.addParameter(&custom_Location_Text);
    wm.addParameter(&custom_latitude);
    wm.addParameter(&custom_longitude);
    wm.addParameter(&custom_timezone);

    wm.addParameter(&custom_Tuning_Header_Text);

    wm.addParameter(&custom_stillness_text);
    wm.addParameter(&custom_Stillness_thresh_adjust);

    wm.addParameter(&custom_Start_Stillness_Time_text);
    wm.addParameter(&custom_Stillness_time_adjust);

    wm.addParameter(&custom_Start_Movement_text);
    wm.addParameter(&custom_Movement_thresh_adjust);

    wm.addParameter(&custom_Max_Gesture_Time_text);
    wm.addParameter(&custom_Gesture_timout_adjust);

    wm.addParameter(&custom_IR_Loss_Timeout_text);
    wm.addParameter(&custom_IR_Loss_timeout_adjust);

    // (custom spell fields will be created/added earlier in the function)

    // ensure settings are saved when changed
    wm.setSaveParamsCallback(saveCustomParameters);

    // Custom Menu
    std::vector<const char*> menu = {"wifi", "param", "info", "sep", "restart"};
    wm.setMenu(menu);

    wm.setConnectTimeout(15); // 15 seconds to connect to WiFi

    // Try to connect to saved WiFi or start config portal
    if (!wm.autoConnect("GlyphReader-Setup")) {
        LOG_ALWAYS("Failed to connect to WiFi - continuing without network");
        // Device continues operating without WiFi
        return false;  // Indicates no WiFi, but device still functional
    } else {
        LOG_DEBUG("Connected to WiFi!");
        wm.startWebPortal();  // Keep web portal available for configuration changes
        return true;  // WiFi connected successfully
    }

}

