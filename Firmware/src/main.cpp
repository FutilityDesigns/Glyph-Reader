// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

/*
================================================================================
  IR Wand Tracker - Main Application Code
================================================================================
  
  Project: Glyph Reader
  Description: ESP32-S3 based gesture recognition system using a Pixart IR 
               camera (similar to Wiimote sensor) to track wand movements in 
               3D space and recognize predefined gesture patterns (spells).
  
  Hardware:
    - ESP32-S3 microcontroller
    - Pixart IR camera (I2C interface, 1024x768 tracking resolution)
    - GC9A01A round LCD display (240x240, SPI)
    - NeoPixel RGB LEDs (WS2812B)
    - SD card reader (SPI)
  
  Features:
    - Real-time IR wand tracking at ~100Hz
    - Pattern matching for gesture recognition
    - Visual feedback via LCD display (trail visualization, spell images)
    - LED feedback with multiple modes (solid, rainbow, sparkle, nightlight)
    - WiFi configuration portal (WiFiManager)
    - MQTT publishing of detected spells
    - SD card support for custom spell configurations
    - Configurable nightlight mode triggered by specific spells
  
  Created: 2025-2026 Futility Designs
  License: PolyForm Noncommercial License 1.0.0
================================================================================
*/

// ESP32 core library for mDNS (network service discovery)
#include <ESPmDNS.h>

// Hardware interface modules
#include "led_control.h"          // NeoPixel LED control and effects
#include "screenFunctions.h"      // GC9A01A display operations
#include "cameraFunctions.h"      // Pixart IR camera I2C communication
#include "buttonFunctions.h"    // Button handling

// Spell recognition system
#include "spell_patterns.h"       // Predefined gesture patterns
#include "spell_matching.h"       // Pattern matching algorithms

// Configuration and network
#include "preferenceFunctions.h"  // NVS preference storage
#include "webFunctions.h"         // WiFiManager web portal
#include "wifiFunctions.h"        // MQTT client management
#include "sdFunctions.h"          // SD card operations

// Global definitions and hardware pins
#include "glyphReader.h"

//=====================================
// Global State Variables
//=====================================

// Screen timing control
unsigned long screenSpellOnTime = 0;            // Timestamp when spell name was displayed
const unsigned long screenSpellDuration = 3000; // Display spell name for 3 seconds
unsigned long screenOnTime = 0;                 // Timestamp of last screen activity
const unsigned long screenTimeout = 60000;      // Turn off screen after 60 seconds of inactivity

// LED timing control
unsigned long ledOnTime = 0;                    // Timestamp when LED effect started
const unsigned long ledeffectTimeout = 5000;    // Turn off LEDs after 5 seconds of inactivity
unsigned long nightlightOnTime = 0;             // Timestamp when nightlight mode started
//const unsigned long nightlightTimeout = 28800000; // Nightlight auto-off after 8 hours
const unsigned long nightlightTimeout = 60000; // Nightlight auto-off after 10 seconds (for testing)

// System state flags
bool backlightStateOn = false;    // Current backlight on/off state
bool nightlightActive = false;    // Track if nightlight mode is currently on
bool cameraInitialized = false;   // Track if camera successfully initialized
uint32_t lastReadTime = 0;        // Timestamp of last camera read (for 100Hz timing)

// Settings menu state
bool inSettingsMode = false;      // Are we in settings menu?
bool editingSettingValue = false; // Are we currently editing a value?
int currentSettingIndex = 0;      // Which setting are we viewing/editing?
int settingValueIndex = 0;        // Current value option for multi-choice settings


//=====================================
// I2C Device Scanner
//=====================================
/**
 * Scans the I2C bus for connected devices
 * Attempts to communicate with all possible I2C addresses (1-126) and reports
 * which addresses respond. Used during setup to verify camera connection.
 * 
 * Typical devices:
 *   - 0x0C: Pixart IR camera (default address)
 */
void scanI2C() {
  LOG_DEBUG("\n=== Scanning I2C Bus ===");
  int devicesFound = 0;
  
  // Scan all valid I2C addresses (0x01 to 0x7E)
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t error = Wire.endTransmission();
    
    if (error == 0) {
      LOG_DEBUG("Device found at address 0x%02X\n", addr);
      devicesFound++;
    }
  }
  
  if (devicesFound == 0) {
    LOG_ALWAYS("No I2C devices found!");
  } else {
    LOG_DEBUG("Found %d device(s)\n", devicesFound);
  }
  LOG_DEBUG("");
}

//=====================================
// Setup - Hardware Initialization
//=====================================
void setup() {
  //-----------------------------------
  // Step 1: Serial Console
  //-----------------------------------
  // Initialize Serial for debugging
  Serial.begin(115200);
  delay(1000); // Give serial time to initialize
  
  LOG_DEBUG("\n\n=================================");
  LOG_DEBUG("ESP32-S3 Interactive Wand");
  LOG_DEBUG("=================================\n");
  
  //-----------------------------------
  // Step 2: Display Initialization
  //-----------------------------------
  // Variable to track setup step for the setup display
  int step = 1;
  
  // Initialize screen (GC9A01A round LCD, 240x240)
  screenInit();
  
  // Turn on backlight for user feedback
  backlightOn();
  
  // Update setup display with display status
  updateSetupDisplay(step, "Display", "pass");
  
  // Increment step counter
  step++;

  //-----------------------------------
  // Step 3: LED Initialization
  //-----------------------------------
  updateSetupDisplay(step, "LEDs", "init");
  
  // Initialize NeoPixel LED strip
  initLEDs();
  
  // Turn off LEDs initially
  ledOff();
  
  // Update display for LED status
  updateSetupDisplay(step, "LEDs", "pass");
  step++;

  //-----------------------------------
  // Step 4: Load Preferences from NVS
  //-----------------------------------
  updateSetupDisplay(step, "Preferences", "init");
  
  // Load all persistent preferences from non-volatile storage
  // Includes: MQTT config, tuning parameters, nightlight settings
  loadPreferences();
  
  // Log loaded tuning parameters for debugging
  LOG_DEBUG("Loaded tuning parameters:");
  LOG_DEBUG("  MOVEMENT_THRESHOLD: %d", MOVEMENT_THRESHOLD);
  LOG_DEBUG("  STILLNESS_THRESHOLD: %d", STILLNESS_THRESHOLD);
  LOG_DEBUG("  READY_STILLNESS_TIME: %d", READY_STILLNESS_TIME);
  LOG_DEBUG("  END_STILLNESS_TIME: %d", END_STILLNESS_TIME);
  LOG_DEBUG("  GESTURE_TIMEOUT: %d", GESTURE_TIMEOUT);
  LOG_DEBUG("  IR_LOSS_TIMEOUT: %d", IR_LOSS_TIMEOUT);

  // Update display for preferences status
  updateSetupDisplay(step, "Preferences", "pass");
  step++;
  
  //-----------------------------------
  // Step 5: Initialize Spell Patterns
  //-----------------------------------
  // Initialize spell patterns 
  // This normalizes and resamples all patterns to 40 points for consistent matching
  initSpellPatterns();
  
  //-----------------------------------
  // Step 6: WiFi Configuration
  //-----------------------------------
  // Setup WiFi with WiFiManager
  updateSetupDisplay(step, "WiFi Manager", "init");
  
  // Attempt WiFi connection or start AP mode
  // Returns true if connected to WiFi, false if in AP mode
  bool wifiConnected = initWM();
  
  if (wifiConnected) {
    updateSetupDisplay(step, "WiFi Manager", "pass");
    
    //-----------------------------------
    // Step 6a: mDNS Responder
    //-----------------------------------
    // Configure mDNS for easy access via http://glyphreader.local
    if (!MDNS.begin("glyphreader")) {
      LOG_ALWAYS("Error setting up MDNS responder!");
    } else {
      LOG_DEBUG("mDNS responder started: http://glyphreader.local");
      MDNS.addService("http", "tcp", 80);
    }
  } else {
    // WiFi not connected - offline mode (AP mode active)
    updateSetupDisplay(step, "WiFi Manager", "offline");
    LOG_ALWAYS("Continuing in offline mode - web portal available at 192.168.4.1");
  }
  step++;
  
  //-----------------------------------
  // Step 7: MQTT Client ID Generation
  //-----------------------------------
  // Generate unique client ID from MAC address
  // Format: GlyphReader-XXXXXX (last 3 bytes of MAC)
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(mqttClientId, sizeof(mqttClientId), "GlyphReader-%02X%02X%02X", mac[3], mac[4], mac[5]);
  LOG_DEBUG("Device ID: %s", mqttClientId);
  
  //-----------------------------------
  // Step 8: MQTT Configuration
  //-----------------------------------
  // Setup MQTT (only if WiFi connected AND MQTT host is configured)
  updateSetupDisplay(step, "MQTT", "init");
  
  if (wifiConnected && MQTT_HOST.length() > 0) {
    // Configure MQTT client with broker address and port
    mqttClient.setServer(MQTT_HOST.c_str(), MQTT_PORT);
    LOG_DEBUG("MQTT configured for %s:%d\n", MQTT_HOST.c_str(), MQTT_PORT);
    updateSetupDisplay(step, "MQTT", "ready");
    // Note: MQTT will auto-connect in loop() via reconnectMQTT()
  } else if (!wifiConnected) {
    updateSetupDisplay(step, "MQTT", "skip");
    LOG_DEBUG("MQTT skipped - no WiFi connection");
  } else {
    updateSetupDisplay(step, "MQTT", "skip");
    LOG_DEBUG("MQTT skipped - no broker configured (set via web portal)");
  }
  step++;
  
  //-----------------------------------
  // Step 9: SD Card Initialization
  //-----------------------------------
  // Initialize SD card
  updateSetupDisplay(step, "SD Card", "init");
  bool sdCardReady = false;
  
  if (initSD()) {
    updateSetupDisplay(step, "SD Card", "pass");
    listDirectory("/", 0);  // List root directory contents
    sdCardReady = true;
  } else {
    updateSetupDisplay(step, "SD Card", "fail");
  }
  step++;

  //-----------------------------------
  // Step 10: I2C Bus Initialization
  //-----------------------------------
  // Initialize I2C
  updateSetupDisplay(step, "Camera", "init");
  LOG_DEBUG("Initializing I2C (SDA=%d, SCL=%d)...\n", I2C_SDA, I2C_SCL);
  
  // Configure I2C with custom pins
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000); // 400kHz I2C speed for faster updates
  Wire.setTimeOut(500);  // 500ms timeout for I2C transactions
  
  // Scan for I2C devices (debug/diagnostic)
  scanI2C();
  
  //-----------------------------------
  // Step 11: Load Custom Spells
  //-----------------------------------
  // Apply custom spell configurations from SD card (if present)
  // This allows users to modify/add/replace spells via spells.json
  if (sdCardReady) {
    applyCustomSpells();
  }

#ifdef SHOW_PATTERNS_ON_STARTUP  
  // Visualize spell patterns on screen for debugging
  // Shows how each spell is defined after normalization/resampling
  showSpellPatterns();
#endif
  
  // Check for spell image files 
  // Validates that .bmp image files exist on SD card for each spell
  if (sdCardReady) {
    checkSpellImages();
  }
  
  //-----------------------------------
  // Step 12: Camera Initialization
  //-----------------------------------
  // Initialize Pixart IR camera
  cameraInitialized = initCamera();
  
  if (!cameraInitialized) {
    // Camera failed to initialize - log error and wiring info
    LOG_ALWAYS("ERROR: Failed to initialize camera!");
    LOG_ALWAYS("Check your wiring:");
    LOG_ALWAYS("  SDA -> GPIO %d", I2C_SDA);
    LOG_ALWAYS("  SCL -> GPIO %d", I2C_SCL);
    LOG_ALWAYS("  VCC -> 3.3V");
    LOG_ALWAYS("  GND -> GND");
  }

  else {
    // Camera successfully initialized
    updateSetupDisplay(step, "Camera", "pass");
    step++;
  }

  //-----------------------------------
  // Step 13: Button Initialization
  //-----------------------------------
  // Initialize the buttons
  updateSetupDisplay(step, "Buttons", "init");
  delay(100); // Short delay to stabilize
  buttonInit();
  updateSetupDisplay(step, "Buttons", "pass");
  step++;

  LOG_DEBUG("buttons complete");
  
  //-----------------------------------
  // Setup Complete
  //-----------------------------------
  // Artificial delay to let user see final status
  delay(1000);
  
  // Clear setup display
  LOG_DEBUG("Setup complete - clearing display");
  clearDisplay();
  
  // Set screen on time for timeout tracking
  screenOnTime = millis();  
}


//=====================================
// Main Loop 
//=====================================
void loop() {
  //-----------------------------------
  // LED Animation Updates
  //-----------------------------------
  // Update LED effects 
  updateLEDs();
  
  //-----------------------------------
  // Web Portal Processing
  //-----------------------------------
  // Handle web portal (WiFiManager)
  // Processes HTTP requests, serves configuration pages
  // WiFiManager automatically manages AP mode when WiFi is disconnected
  wm.process();

  //-----------------------------------
  // Button Processing
  //-----------------------------------
  // Check for button presses and process them
  button1.loop();
  button2.loop();

  //-----------------------------------
  // Camera Reinitialization
  //-----------------------------------
  // If camera not initialized, attempt reinitialization
  if (!cameraInitialized) {
    // Try to reinitialize every 5 seconds
    delay(5000);
    cameraInitialized = initCamera();
    return;  // Skip rest of loop until camera is ready
  }
  
  //-----------------------------------
  // MQTT Connection Maintenance
  //-----------------------------------
  // Maintain MQTT connection
  if (!mqttClient.connected()) {
    reconnectMQTT();  // Attempt reconnection
  }
  mqttClient.loop();  // Process MQTT messages
  
  //-----------------------------------
  // Camera Data Acquisition
  //-----------------------------------
  // Read camera data at high speed (aiming for ~100Hz)
  // Skip camera processing when in settings mode
  uint32_t currentTime = millis();
  if (!inSettingsMode && currentTime - lastReadTime >= 10) { // Read every 10ms (100Hz)
    readCameraData();  // Process IR tracking and gesture recognition
    lastReadTime = currentTime;
  }
  
  // Minimal delay to prevent watchdog issues
  delayMicroseconds(100);

  //-----------------------------------
  // Screen Timeout Handling
  //-----------------------------------
  // Screen and LED timeout handling
  
  // Check if the spell name has been displayed long enough
  if (screenSpellOnTime > 0 && (millis() - screenSpellOnTime >= screenSpellDuration)) {
    // Clear spell name after 3 seconds
    screenSpellOnTime = 0;
    clearDisplay();
  }

  // Check if the screen has been on for too long without activity
  if (backlightStateOn && (millis() - screenOnTime >= screenTimeout)) {
    // Turn off backlight after 60 seconds
    backlightOff();
  }

  //-----------------------------------
  // LED Timeout Handling
  //-----------------------------------
  // Check if the LEDs have been on for too long without activity
  if (ledOnTime > 0 && (millis() - ledOnTime >= ledeffectTimeout) && currentMode != LED_NIGHTLIGHT) {
    // After 5 seconds, either return to nightlight or turn off
    if (nightlightActive) {
      ledNightlight(NIGHTLIGHT_BRIGHTNESS);  // Return to nightlight mode with saved brightness
    } else {
      setLEDMode(LED_OFF);  // Turn off completely
    }
    ledOnTime = 0;  // Reset timer
  }

  // Check if the nightlight mode needs to be turned off
  if (nightlightActive && (millis() - nightlightOnTime >= nightlightTimeout)) {
    // Turn off nightlight after configured duration
    nightlightActive = false;
    setLEDMode(LED_OFF);
    LOG_DEBUG("Nightlight mode timed out - LEDs turned off");
  }
}