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

// WiFiManager instance
WiFiManager wm;

// Buffers for stored values
char mqttServerBuffer[20] = "";
char mqttPortBuffer[6] = "";
char mqttTopicBuffer[50] = "";

//Custom Text Parameters to explain each variable
WiFiManagerParameter custom_Header_Text("<h1>Wand Commander Settings</h1>");
WiFiManagerParameter custom_MQTT_Text("<p>Enter your MQTT Broker settings below:</p>");
WiFiManagerParameter custom_Topic_Text("<p>MQTT Topic to publish recognized spells</p>");
WiFiManagerParameter custom_Nightlight_Header_Text("<h2>Nightlight Configuration</h2>");
WiFiManagerParameter custom_Nightlight_Text("<p>Select spells to turn nightlight on/off. When active, LEDs return to nightlight instead of turning off.</p>");
WiFiManagerParameter custom_Tuning_Header_Text("<h2>Tuning Parameters for Spell Detection</h2>");
WiFiManagerParameter custom_Start_Movement_text("<p>Minimum pixels to consider motion to start tracking</p>");
WiFiManagerParameter custom_stillness_text("<p>Maximum Pixels to consider the wand stationary and initiate or end the spell tracking</p>");
WiFiManagerParameter custom_Start_Stillness_Time_text("<p>How long the wand needs to be still to initiate the device</p>");
WiFiManagerParameter custom_End_STillness_Time_text("<p>How long the wand needs to remain still to end tracking</p>");
WiFiManagerParameter custom_Max_Gesture_Time_text("<p>Maximum time to track a spell before timing out</p>");
WiFiManagerParameter custom_IR_Loss_Timeout_text("<p>Max time tracking can be lost before tracking is ended</p>");

// Custom Parameter Fields for MQTT Settings
WiFiManagerParameter custom_mqtt_server("mqtt_server", "MQTT Broker Address", mqttServerBuffer, 20);
WiFiManagerParameter custom_mqtt_port("mqtt_port", "MQTT Broker Port", mqttPortBuffer, 6, "1883");
WiFiManagerParameter custom_mqtt_topic("mqtt_topic", "MQTT Topic", mqttTopicBuffer, 50);

/**
 * Generate HTML for numeric parameter adjuster with +/- buttons
 * settingName: Display name and field ID for the parameter
 * startValue: Current value to display
 * stepValue: Amount to increment/decrement per button click
 * return String containing complete HTML div with buttons, input, and JavaScript
 * Creates an interactive numeric control with:
 * - Decrement button (-) in red
 * - Numeric input field (center, user can also type)
 * - Increment button (+) in green
 * - JavaScript adjust() function (added once)
 * JAVASCRIPT:
 * - adjust(id, step) function modifies input value
 * - Static flag prevents duplicate script blocks
 * - Client-side interactivity (no page reload)
 * Called from generateAdjusters() for each tuning parameter.
 */
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

// Declare Strings but don't initialize yet (values aren't loaded from prefs yet)
String movementThresholdHTML;
String stillnessThresholdHTML;
String readyStillnessTimeHTML;
String endStillnessTimeHTML;
String gestureTimeoutHTML;
String irLossTimeoutHTML;
String nightlightOnHTML;
String nightlightOffHTML;

// Declare pointers to WiFiManagerParameters (will be created later)
WiFiManagerParameter* custom_Movement_adjust = nullptr;
WiFiManagerParameter* custom_Stillness_adjust = nullptr;
WiFiManagerParameter* custom_Ready_Stillness_adjust = nullptr;
WiFiManagerParameter* custom_End_Stillness_adjust = nullptr;
WiFiManagerParameter* custom_Gesture_Timeout_adjust = nullptr;
WiFiManagerParameter* custom_IR_Loss_Timeout_adjust = nullptr;
WiFiManagerParameter* custom_Nightlight_On_Dropdown = nullptr;
WiFiManagerParameter* custom_Nightlight_Off_Dropdown = nullptr;

/**
 * Generate HTML dropdown menu populated with spell names
 * settingName: Label and field name for the dropdown
 * currentValue: Currently selected spell name (will be marked selected)
 * return String containing complete HTML div with label and <select> element
 * Creates dropdown menu with:
 * - "-- None --" option (empty value for disabling feature)
 * - One <option> for each spell in spellPatterns vector
 * - Currently selected spell pre-selected in UI
 * USAGE:
 * - Nightlight control: User selects which spell activates/deactivates nightlight
 * - Future features: Could be used for other spell-triggered actions
 * STYLING:
 * - Full width with padding for touch-friendly interface
 * - Border and rounded corners for consistency
 * - Label positioned above dropdown
 * Called from generateAdjusters() to create nightlight spell selectors.
 */
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

/**
 * Generate all adjuster parameter HTML from current preferences
 * Creates HTML strings for all tuning parameters and nightlight dropdowns
 * using current values from global preference variables. Cleans up old
 * WiFiManagerParameter objects and creates new ones with fresh HTML.
 * PARAMETERS GENERATED:
 * - Movement Threshold (step: 1 pixel)
 * - Stillness Threshold (step: 1 pixel)
 * - Ready Stillness Time (step: 50ms)
 * - End Stillness Time (step: 50ms)
 * - Gesture Timeout (step: 500ms)
 * - IR Loss Timeout (step: 50ms)
 * - Nightlight On Spell (dropdown)
 * - Nightlight Off Spell (dropdown)
 * Called from loadCustomParameters() before adding parameters to WiFiManager.
 */
void generateAdjusters(){
    // Generate HTML with current preference values
    movementThresholdHTML = generateAdjusterHTML("Movement Threshold", MOVEMENT_THRESHOLD, 1);
    stillnessThresholdHTML = generateAdjusterHTML("Stillness Threshold", STILLNESS_THRESHOLD, 1);
    readyStillnessTimeHTML = generateAdjusterHTML("Ready Stillness Time", READY_STILLNESS_TIME, 50);
    endStillnessTimeHTML = generateAdjusterHTML("End Stillness Time", END_STILLNESS_TIME, 50);
    gestureTimeoutHTML = generateAdjusterHTML("Gesture Timeout", GESTURE_TIMEOUT, 500);
    irLossTimeoutHTML = generateAdjusterHTML("IR Loss Timeout", IR_LOSS_TIMEOUT, 50);
    
    // Generate nightlight spell dropdowns (use global strings so they persist)
    nightlightOnHTML = generateSpellDropdown("Nightlight On Spell", NIGHTLIGHT_ON_SPELL);
    nightlightOffHTML = generateSpellDropdown("Nightlight Off Spell", NIGHTLIGHT_OFF_SPELL);
    
    // Clean up old parameters if they exist
    if (custom_Movement_adjust) delete custom_Movement_adjust;
    if (custom_Stillness_adjust) delete custom_Stillness_adjust;
    if (custom_Ready_Stillness_adjust) delete custom_Ready_Stillness_adjust;
    if (custom_End_Stillness_adjust) delete custom_End_Stillness_adjust;
    if (custom_Gesture_Timeout_adjust) delete custom_Gesture_Timeout_adjust;
    if (custom_IR_Loss_Timeout_adjust) delete custom_IR_Loss_Timeout_adjust;
    if (custom_Nightlight_On_Dropdown) delete custom_Nightlight_On_Dropdown;
    if (custom_Nightlight_Off_Dropdown) delete custom_Nightlight_Off_Dropdown;
    
    // Create new parameters with current values
    custom_Movement_adjust = new WiFiManagerParameter(movementThresholdHTML.c_str());
    custom_Stillness_adjust = new WiFiManagerParameter(stillnessThresholdHTML.c_str());
    custom_Ready_Stillness_adjust = new WiFiManagerParameter(readyStillnessTimeHTML.c_str());
    custom_End_Stillness_adjust = new WiFiManagerParameter(endStillnessTimeHTML.c_str());
    custom_Gesture_Timeout_adjust = new WiFiManagerParameter(gestureTimeoutHTML.c_str());
    custom_IR_Loss_Timeout_adjust = new WiFiManagerParameter(irLossTimeoutHTML.c_str());
    custom_Nightlight_On_Dropdown = new WiFiManagerParameter(nightlightOnHTML.c_str());
    custom_Nightlight_Off_Dropdown = new WiFiManagerParameter(nightlightOffHTML.c_str());
}

/**
 * Load current preference values into web form fields
 * Populates all WiFiManager form fields with current values from NVS:
 * - MQTT settings copied into char buffers
 * - Buffers assigned to WiFiManagerParameter objects
 * - Adjuster HTML generated with current tuning parameter values
 * WORKFLOW:
 * 1. loadPreferences() reads all NVS values into global variables
 * 2. MQTT values copied to char buffers (required by WiFiManager)
 * 3. setValue() updates WiFiManagerParameter display values
 * 4. generateAdjusters() creates HTML for numeric controls
 * Called from initWM() before starting the configuration portal.
 */
void loadCustomParameters() {
    loadPreferences();
    strcpy(mqttServerBuffer, MQTT_HOST.c_str());
    snprintf(mqttPortBuffer, sizeof(mqttPortBuffer), "%d", MQTT_PORT);
    strcpy(mqttTopicBuffer, MQTT_TOPIC.c_str());
    custom_mqtt_server.setValue(mqttServerBuffer, 20);
    custom_mqtt_port.setValue(mqttPortBuffer, 6);
    custom_mqtt_topic.setValue(mqttTopicBuffer, 50);
    generateAdjusters();

}

/**
 * Save web form values to NVS preferences and reboot device
 * Callback function triggered when user submits WiFiManager config form.
 * Extracts all form field values, compares to current settings, saves
 * changes to NVS flash, displays confirmation page, and reboots device.
 * SAVE LOGIC:
 * - Only saves values that have changed (avoids unnecessary NVS writes)
 * - Empty strings allowed for nightlight spells (disables feature)
 * - Non-empty, non-zero values validated before saving
 * PARAMETERS SAVED:
 * - MQTT: Host, port, topic
 * - Nightlight: On spell, off spell
 * - Tuning: All 6 detection parameters
 * REBOOT SEQUENCE:
 * 1. Extract form values from wm.server->arg()
 * 2. Compare to current global variables
 * 3. Save changes via setPref() (writes to NVS)
 * 4. Update char buffers for MQTT parameters
 * 5. Send HTML confirmation page to browser
 * 6. delay(1000) to ensure page loads
 * 7. ESP.restart() to reboot and apply changes
 * HTML CONFIRMATION PAGE:
 * - Simple centered message
 * - Instructs user to reconnect to portal
 * - Delivered before reboot so user sees feedback
 * Registered as callback via wm.setSaveParamsCallback().
 */
void saveCustomParameters() {
    // temporarily load new mqtt values to compare to old values
    String newMqttHost = wm.server->arg("mqtt_server");
    int newMqttPort = wm.server->arg("mqtt_port").toInt();
    String newMqttTopic = wm.server->arg("mqtt_topic");
    
    // Get nightlight spell settings
    String newNightlightOn = wm.server->arg("Nightlight On Spell");
    String newNightlightOff = wm.server->arg("Nightlight Off Spell");

    // If the new values are different and non-zero length, save them
    if (newMqttHost.length() > 0 && newMqttHost != MQTT_HOST) {
        MQTT_HOST = newMqttHost;
        setPref(PrefKey::MQTT_HOST, MQTT_HOST);
    }
    if (newMqttPort > 0 && newMqttPort != MQTT_PORT) {
        MQTT_PORT = newMqttPort;
        setPref(PrefKey::MQTT_PORT, MQTT_PORT);
    }
    if (newMqttTopic.length() > 0 && newMqttTopic != MQTT_TOPIC) {
        MQTT_TOPIC = newMqttTopic;
        setPref(PrefKey::MQTT_TOPIC, MQTT_TOPIC);
    }
    
    // Save nightlight spell settings (can be empty to disable)
    if (newNightlightOn != NIGHTLIGHT_ON_SPELL) {
        NIGHTLIGHT_ON_SPELL = newNightlightOn;
        setPref(PrefKey::NIGHTLIGHT_ON_SPELL, NIGHTLIGHT_ON_SPELL);
    }
    if (newNightlightOff != NIGHTLIGHT_OFF_SPELL) {
        NIGHTLIGHT_OFF_SPELL = newNightlightOff;
        setPref(PrefKey::NIGHTLIGHT_OFF_SPELL, NIGHTLIGHT_OFF_SPELL);
    }

    // Get tuning parameters from form
    int newMovementThreshold = wm.server->arg("Movement Threshold").toInt();
    int newStillnessThreshold = wm.server->arg("Stillness Threshold").toInt();
    int newReadyStillnessTime = wm.server->arg("Ready Stillness Time").toInt();
    int newGestureTimeout = wm.server->arg("Gesture Timeout").toInt();
    int newIrLossTimeout = wm.server->arg("IR Loss Timeout").toInt();
    
    // Save tuning parameters if changed
    if (newMovementThreshold > 0 && newMovementThreshold != MOVEMENT_THRESHOLD) {
        MOVEMENT_THRESHOLD = newMovementThreshold;
        setPref(PrefKey::MOVEMENT_THRESHOLD, MOVEMENT_THRESHOLD);
    }
    if (newStillnessThreshold > 0 && newStillnessThreshold != STILLNESS_THRESHOLD) {
        STILLNESS_THRESHOLD = newStillnessThreshold;
        setPref(PrefKey::STILLNESS_THRESHOLD, STILLNESS_THRESHOLD);
    }
    if (newReadyStillnessTime > 0 && newReadyStillnessTime != READY_STILLNESS_TIME) {
        READY_STILLNESS_TIME = newReadyStillnessTime;
        setPref(PrefKey::READY_STILLNESS_TIME, READY_STILLNESS_TIME);
    }
    if (newGestureTimeout > 0 && newGestureTimeout != GESTURE_TIMEOUT) {
        GESTURE_TIMEOUT = newGestureTimeout;
        setPref(PrefKey::GESTURE_TIMEOUT, GESTURE_TIMEOUT);
    }
    if (newIrLossTimeout > 0 && newIrLossTimeout != IR_LOSS_TIMEOUT) {
        IR_LOSS_TIMEOUT = newIrLossTimeout;
        setPref(PrefKey::IR_LOSS_TIMEOUT, IR_LOSS_TIMEOUT);
    }

    // Update MQTT buffers to reflect saved values
    strcpy(mqttServerBuffer, MQTT_HOST.c_str());
    snprintf(mqttPortBuffer, sizeof(mqttPortBuffer), "%d", MQTT_PORT);
    strcpy(mqttTopicBuffer, MQTT_TOPIC.c_str());
    custom_mqtt_server.setValue(mqttServerBuffer, 20);
    custom_mqtt_port.setValue(mqttPortBuffer, 6);
    custom_mqtt_topic.setValue(mqttTopicBuffer, 50);

    // **Send reboot confirmation page**
    String responseHTML = "<html><head>";
    responseHTML += "<style>body{font-family:sans-serif;text-align:center;padding:20px;}</style>";
    responseHTML += "</head><body>";
    responseHTML += "<h2>Settings Saved!</h2>";
    responseHTML += "<p>Device is rebooting to apply changes...</p>";
    responseHTML += "<p>Reconnect to the web portal in a few seconds.</p>";
    responseHTML += "</body></html>";

    wm.server->send(200, "text/html", responseHTML);
    
    // Give the browser time to receive the response before rebooting
    delay(1000);
    
    // Reboot to apply all settings
    ESP.restart();
}

/**
 * Initialize WiFiManager and start configuration portal
 * return true if WiFi connected successfully, false if connection failed
 * Sets up WiFiManager with custom parameters for MQTT, tuning, and nightlight
 * configuration. Attempts to connect to saved WiFi credentials or starts
 * captive portal for configuration.
 * INITIALIZATION SEQUENCE:
 * 1. Set WiFi to station mode (STA)
 * 2. Load current NVS preferences into form fields
 * 3. Configure WiFiManager title and theme
 * 4. Register all custom HTML parameters (MQTT, tuning, nightlight)
 * 5. Set save callback to handle form submission
 * 6. Configure portal menu and timeouts
 * 7. Attempt autoConnect (try saved WiFi or start portal)
 * 8. Start non-blocking web portal for later access
 * CUSTOM PARAMETERS ADDED:
 * - Header texts (HTML descriptions)
 * - MQTT: Server, port, topic
 * - Nightlight: On/off spell dropdowns
 * - Tuning: 6 numeric adjusters for detection parameters
 * TIMEOUT CONFIGURATION:
 * - Portal timeout: 30 seconds (then boot continues)
 * - Connect timeout: 20 seconds (for saved WiFi)
 * - Non-blocking: Device works without WiFi
 * MENU ITEMS:
 * - wifi: WiFi configuration page
 * - param: Custom parameters page
 * - info: Device information
 * - sep: Visual separator
 * - restart: Reboot device button
 * AP MODE:
 * - SSID: \"GlyphReader-Setup\"
 * - IP: 192.168.4.1 (default captive portal IP)
 * - Started on first boot or if saved WiFi fails
 * WEB PORTAL:
 * - Started in non-blocking mode
 * - Remains accessible after boot for config changes
 * - Can access via device IP if WiFi connected
 * note: Portal remains active even if WiFi connection fails
 * note: Device continues normal operation without WiFi
 * Called from main.cpp setup() during initialization.
 */
bool initWM() {
    LOG_DEBUG("Initializing WiFiManager");
    WiFi.mode(WIFI_STA);  // Station mode (not AP mode yet)

    // Load stored parameters from NVS into form fields
    loadCustomParameters();

    // Set WifiManager Title
    wm.setTitle("Wand Commander Configuration");
    wm.setClass("invert");

    // Add Custom Fields
    wm.addParameter(&custom_Header_Text);
    wm.addParameter(&custom_MQTT_Text);
    wm.addParameter(&custom_mqtt_server);
    wm.addParameter(&custom_mqtt_port);
    wm.addParameter(&custom_Topic_Text);
    wm.addParameter(&custom_mqtt_topic);
    wm.addParameter(&custom_Nightlight_Header_Text);
    wm.addParameter(&custom_Nightlight_Text);
    wm.addParameter(custom_Nightlight_On_Dropdown);
    wm.addParameter(custom_Nightlight_Off_Dropdown);
    wm.addParameter(&custom_Tuning_Header_Text);
    wm.addParameter(&custom_stillness_text);
    wm.addParameter(custom_Stillness_adjust);
    wm.addParameter(&custom_Start_Movement_text);
    wm.addParameter(custom_Movement_adjust);
    wm.addParameter(&custom_Start_Stillness_Time_text);
    wm.addParameter(custom_Ready_Stillness_adjust);
    wm.addParameter(&custom_Max_Gesture_Time_text);
    wm.addParameter(custom_Gesture_Timeout_adjust);
    wm.addParameter(&custom_IR_Loss_Timeout_text);
    wm.addParameter(custom_IR_Loss_Timeout_adjust);

    // ensure settings are saved when changed
    wm.setSaveParamsCallback(saveCustomParameters);

    // Custom Menu
    std::vector<const char*> menu = {"wifi", "param", "info", "sep", "restart"};
    wm.setMenu(menu);

    // Enable WiFiManager to automatically manage AP when disconnected
    // This makes WiFiManager handle AP creation whenever WiFi is not connected
    wm.setConfigPortalBlocking(false);  // Non-blocking mode
    
    // Set connect timeout for saved WiFi credentials
    wm.setConnectTimeout(20);  // 20 seconds to connect to saved WiFi
    
    // Try to connect to saved WiFi or start AP if no connection
    // In non-blocking mode, this returns immediately and wm.process() handles everything
    bool connected = wm.autoConnect("GlyphReader-Setup");
    
    if (connected) {
        LOG_ALWAYS("WiFi connected: %s", WiFi.SSID().c_str());
        LOG_ALWAYS("Web portal available at: http://%s", WiFi.localIP().toString().c_str());
    } else {
        LOG_ALWAYS("WiFi not connected - AP mode active (GlyphReader-Setup)");
    }
    
    // Start web portal for configuration (works both in STA and AP mode)
    // This keeps the web interface accessible even when connected to WiFi
    wm.startWebPortal();
    
    return connected;  // Return initial status
}