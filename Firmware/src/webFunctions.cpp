/*
All functions and items realted to web server operations
This file adds the customization options to the wifi manager portal
*/
#include "webFunctions.h"
#include <WiFiManager.h>
#include "preferenceFunctions.h"
#include "wandCommander.h"

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

// Generate HTML for a numeric adjuster with +/- buttons
// This functions can be called multiple times to generate different adjusters
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

// Declare pointers to WiFiManagerParameters (will be created later)
WiFiManagerParameter* custom_Movement_adjust = nullptr;
WiFiManagerParameter* custom_Stillness_adjust = nullptr;
WiFiManagerParameter* custom_Ready_Stillness_adjust = nullptr;
WiFiManagerParameter* custom_End_Stillness_adjust = nullptr;
WiFiManagerParameter* custom_Gesture_Timeout_adjust = nullptr;
WiFiManagerParameter* custom_IR_Loss_Timeout_adjust = nullptr;

// function to generate adjuster parameters based on current preference values
void generateAdjusters(){
    // Generate HTML with current preference values
    movementThresholdHTML = generateAdjusterHTML("Movement Threshold", MOVEMENT_THRESHOLD, 1);
    stillnessThresholdHTML = generateAdjusterHTML("Stillness Threshold", STILLNESS_THRESHOLD, 1);
    readyStillnessTimeHTML = generateAdjusterHTML("Ready Stillness Time", READY_STILLNESS_TIME, 50);
    endStillnessTimeHTML = generateAdjusterHTML("End Stillness Time", END_STILLNESS_TIME, 50);
    gestureTimeoutHTML = generateAdjusterHTML("Gesture Timeout", GESTURE_TIMEOUT, 500);
    irLossTimeoutHTML = generateAdjusterHTML("IR Loss Timeout", IR_LOSS_TIMEOUT, 50);
    
    // Clean up old parameters if they exist
    if (custom_Movement_adjust) delete custom_Movement_adjust;
    if (custom_Stillness_adjust) delete custom_Stillness_adjust;
    if (custom_Ready_Stillness_adjust) delete custom_Ready_Stillness_adjust;
    if (custom_End_Stillness_adjust) delete custom_End_Stillness_adjust;
    if (custom_Gesture_Timeout_adjust) delete custom_Gesture_Timeout_adjust;
    if (custom_IR_Loss_Timeout_adjust) delete custom_IR_Loss_Timeout_adjust;
    
    // Create new parameters with current values
    custom_Movement_adjust = new WiFiManagerParameter(movementThresholdHTML.c_str());
    custom_Stillness_adjust = new WiFiManagerParameter(stillnessThresholdHTML.c_str());
    custom_Ready_Stillness_adjust = new WiFiManagerParameter(readyStillnessTimeHTML.c_str());
    custom_End_Stillness_adjust = new WiFiManagerParameter(endStillnessTimeHTML.c_str());
    custom_Gesture_Timeout_adjust = new WiFiManagerParameter(gestureTimeoutHTML.c_str());
    custom_IR_Loss_Timeout_adjust = new WiFiManagerParameter(irLossTimeoutHTML.c_str());
}

// Load current preference values into the custom parameter fields
void loadCustomParameters() {
    loadPreferences();
    strcpy(mqttServerBuffer, MQTT_HOST.c_str());
    snprintf(mqttPortBuffer, sizeof(mqttPortBuffer), "%d", MQTT_PORT);
    custom_mqtt_server.setValue(mqttServerBuffer, 20);
    custom_mqtt_port.setValue(mqttPortBuffer, 6);
    generateAdjusters();

}

// Save custom parameters from the web form back to preferences
void saveCustomParameters() {
    // temporarily load new mqtt values to compare to old values
    String newMqttHost = wm.server->arg("mqtt_server");
    int newMqttPort = wm.server->arg("mqtt_port").toInt();
    String newMqttTopic = wm.server->arg("mqtt_topic");

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

    // update the stored values
    loadCustomParameters();

    // **Send an auto-redirect page instead of "Saved!"**
    String responseHTML = "<html><head>";
    responseHTML += "<meta http-equiv='refresh' content='2;url=/param'>";
    responseHTML += "<style>body{font-family:sans-serif;text-align:center;padding:20px;}</style>";
    responseHTML += "</head><body>";
    responseHTML += "<h2>Settings Saved!</h2>";
    responseHTML += "<p>Returning to settings page...</p>";
    responseHTML += "</body></html>";

    wm.server->send(200, "text/html", responseHTML);
}

// Initialize and start the WiFiManager portal
bool initWM() {
    LOG_DEBUG("Initializing WiFiManager");
    WiFi.mode(WIFI_STA);

    // load stored parameters
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
    wm.addParameter(&custom_Tuning_Header_Text);
    wm.addParameter(&custom_Start_Movement_text);
    wm.addParameter(custom_Movement_adjust);
    wm.addParameter(&custom_stillness_text);
    wm.addParameter(custom_Stillness_adjust);
    wm.addParameter(&custom_Start_Stillness_Time_text);
    wm.addParameter(custom_Ready_Stillness_adjust);
    wm.addParameter(&custom_End_STillness_Time_text);
    wm.addParameter(custom_End_Stillness_adjust);
    wm.addParameter(&custom_Max_Gesture_Time_text);
    wm.addParameter(custom_Gesture_Timeout_adjust);
    wm.addParameter(&custom_IR_Loss_Timeout_text);
    wm.addParameter(custom_IR_Loss_Timeout_adjust);

    // ensure settings are saved when changed
    wm.setSaveParamsCallback(saveCustomParameters);

    // Custom Menu
    std::vector<const char*> menu = {"wifi", "param", "info", "sep", "restart"};
    wm.setMenu(menu);

    // Set timeout for autoConnect (30 seconds)
    wm.setConfigPortalTimeout(30);
    
    // Don't block forever - set connect timeout
    wm.setConnectTimeout(20);  // 20 seconds to connect to saved WiFi
    
    // Try to connect to saved WiFi or start portal
    bool connected = wm.autoConnect("GlyphReader-Setup");
    
    if (connected) {
        LOG_ALWAYS("WiFi connected: %s", WiFi.SSID().c_str());
    } else {
        LOG_ALWAYS("WiFi failed to connect - continuing without network");
    }
    
    // Always start web portal for configuration (non-blocking)
    wm.startWebPortal();
    
    return connected;  // Return status but don't block boot
}