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

// Version information
#include "version.h"

// Hardware interface modules
#include "led_control.h"          // NeoPixel LED control and effects
#include "screenFunctions.h"      // GC9A01A display operations
#include "cameraFunctions.h"      // Pixart IR camera I2C communication
#include "buttonFunctions.h"      // Button handling
#include "customSpellFunctions.h" // Custom spell recording
#include "audioFunctions.h"       // I2S audio playback

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

// FreeRTOS for dual-core operation
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

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
unsigned long nightlightCalculatedTimeout = 0;  // Calculated timeout (sunrise-based or fixed)
//const unsigned long nightlightTimeout = 28800000; // Nightlight auto-off after 8 hours
#ifdef ENV_DEV
const unsigned long nightlightTimeout = 60000; // Nightlight auto-off after 60 seconds (for testing)
#elif defined ENV_PROD
const unsigned long nightlightTimeout = 28800000; // Nightlight auto-off after 8 hours
#endif

// System state flags
bool backlightStateOn = false;    // Current backlight on/off state
bool nightlightActive = false;    // Track if nightlight mode is currently on
bool cameraInitialized = false;   // Track if camera successfully initialized
uint32_t lastReadTime = 0;        // Timestamp of last camera read (for adaptive polling)

// Camera/Sensor retry with exponential backoff
uint32_t lastCameraRetry = 0;               // Timestamp of last camera retry attempt
uint32_t cameraBackoffInterval = 5000;      // Start at 5 seconds, max 1 hour
const uint32_t CAMERA_BACKOFF_MAX = 3600000; // 1 hour maximum backoff
bool cameraErrorDisplayed = false;          // Track if error is shown on screen

// Settings menu state
bool inSettingsMode = false;      // Are we in settings menu?
bool editingSettingValue = false; // Are we currently editing a value?
int currentSettingIndex = 0;      // Which setting are we viewing/editing?
int settingValueIndex = 0;        // Current value option for multi-choice settings
bool inSpellRecordingMode = false; // Are we in spell recording mode?
bool isRecordingCustomSpell = false; // Is custom spell recording active (uses normal tracking)?

// WiFi task handle for dual-core operation
TaskHandle_t wifiTaskHandle = NULL;
volatile bool wifiTaskRunning = false;


//=====================================
// WiFi Task - Runs on Core 0
//=====================================
/**
 * Dedicated WiFi processing task running on Core 0
 * 
 * Handles all WiFi-related operations separately from main application:
 * - WiFiManager portal processing (wm.process())
 * - MQTT connection maintenance and message processing
 * - Background NVS/SD saves triggered by web portal
 * 
 * Running WiFi on a dedicated core prevents the main application's
 * I2C operations, display updates, and LED animations from starving
 * the WiFi stack of CPU time.
 * 
 * Stack size: 8KB (WiFiManager uses significant stack for HTML generation)
 * Priority: 1 (same as main loop, but on different core)
 */
void wifiTask(void* parameter) {
  LOG_DEBUG("WiFi task started on Core %d", xPortGetCoreID());
  wifiTaskRunning = true;
  
  while (true) {
    // Process WiFiManager web portal
    wm.process();
    
    // Process background saves (NVS/SD writes from web portal)
    processBackgroundSaves();
    
    // Maintain MQTT connection (handles backoff internally)
    reconnectMQTT();
    mqttClient.loop();
    
    // Give other tasks and WiFi stack time to run
    // 10ms delay provides ~100Hz processing rate which is plenty for web portal
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}


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
  LOG_DEBUG("Glyph Reader Startup");
  LOG_DEBUG("Version: %s", getVersionStringComplete());
  LOG_DEBUG("Built: %s", getBuildTimestamp());
  LOG_DEBUG("=================================\n");
  
  //-----------------------------------
  // Step 2: Display Initialization
  //-----------------------------------
  // Variable to track setup step for the setup display
  int step = 1;
  
  // Initialize screen (GC9A01A round LCD, 240x240)
  screenInit();
  
  
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
  
  // Apply persisted spell color preference (index into predefined palette)
  setSpellPrimaryColorByIndex(SPELL_PRIMARY_COLOR_INDEX);
  LOG_DEBUG("Applied persisted spell color index: %d", SPELL_PRIMARY_COLOR_INDEX);

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
  // Step 6: SD Card Initialization
  //-----------------------------------
  // Initialize SD card BEFORE WiFi Manager so custom spell names are loaded
  // and available for the web portal dropdowns
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
  // Step 7: Load Custom Spells
  //-----------------------------------
  // Apply custom spell configurations from SD card (if present)
  // This allows users to modify/add/replace spells via spells.json
  // MUST be done before WiFi Manager so web portal dropdowns show renamed spells
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
  // Step 8: WiFi Configuration
  //-----------------------------------
  // Setup WiFi with WiFiManager
  updateSetupDisplay(step, "WiFi Manager", "init");
  
  // Attempt WiFi connection or start AP mode
  // Returns true if connected to WiFi, false if in AP mode
  bool wifiConnected = initWM(120);
  
  if (wifiConnected) {
    updateSetupDisplay(step, "WiFi Manager", "pass");
    
    //-----------------------------------
    // Step 8a: Configure NTP Time Sync
    //-----------------------------------
    // Configure NTP time synchronization for accurate date/time
    // This is critical for sunrise/sunset calculations
    LOG_DEBUG("Configuring NTP time sync...");
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");  // GMT+0, will use timezone offset from preferences
    
    // Wait briefly for time to sync
    int retries = 0;
    while (time(nullptr) < 100000 && retries < 20) {  // Valid time is > 100000 (past Jan 1970)
      delay(100);
      retries++;
    }
    
    if (time(nullptr) > 100000) {
      LOG_DEBUG("Time synchronized successfully");
    } else {
      LOG_ALWAYS("Warning: NTP time sync may have failed");
    }
    
    //-----------------------------------
    // Step 8b: Fetch Location Data (if not configured)
    //-----------------------------------
    // If latitude/longitude not set, fetch from ipapi.co
    if (LATITUDE.length() == 0 || LONGITUDE.length() == 0) {
      LOG_DEBUG("Location not configured - fetching from ipapi.co...");
      ApiData locationData = fetchIpApiData();
      
      if (!locationData.error) {
        // Save fetched location to preferences
        LATITUDE = locationData.strings[0];
        LONGITUDE = locationData.strings[1];
        TIMEZONE_OFFSET = locationData.ints[0];
        
        setPref(PrefKey::LATITUDE, LATITUDE);
        setPref(PrefKey::LONGITUDE, LONGITUDE);
        setPref(PrefKey::TIMEZONE_OFFSET, TIMEZONE_OFFSET);
        
        LOG_DEBUG("Location configured: %s, %s (UTC%+d)", 
                  LATITUDE.c_str(), LONGITUDE.c_str(), TIMEZONE_OFFSET / 3600);
      } else {
        LOG_ALWAYS("Failed to fetch location from ipapi.co");
      }
    } else {
      LOG_DEBUG("Location already configured: %s, %s", LATITUDE.c_str(), LONGITUDE.c_str());
    }
    
    //-----------------------------------
    // Step 8c: mDNS Responder
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
  // Step 9: MQTT Client ID Generation
  //-----------------------------------
  // Generate unique client ID from MAC address
  // Format: GlyphReader-XXXXXX (last 3 bytes of MAC)
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(mqttClientId, sizeof(mqttClientId), "GlyphReader-%02X%02X%02X", mac[3], mac[4], mac[5]);
  LOG_DEBUG("Device ID: %s", mqttClientId);
  
  //-----------------------------------
  // Step 10: MQTT Configuration
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
  // Step 11: I2C Bus Initialization
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
  // Step 14: Audio Initialization
  //-----------------------------------
  // Initialize I2S audio (MAX98357A amplifier)
  updateSetupDisplay(step, "Audio", "init");
  
  if (initAudio()) {
    updateSetupDisplay(step, "Audio", "pass");
    LOG_DEBUG("Audio system ready");
    playSound("/sounds/startup.wav");
  } else {
    updateSetupDisplay(step, "Audio", "fail");
    LOG_ALWAYS("Audio initialization failed - sound effects disabled");
  }
  step++;
  
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
  
  //-----------------------------------
  // Step 15: Start WiFi Task on Core 0
  //-----------------------------------
  // Create dedicated WiFi task on Core 0 for reliable portal/MQTT operation
  // Main loop runs on Core 1 (default Arduino core)
  // This separation prevents I2C/display operations from starving WiFi stack
  xTaskCreatePinnedToCore(
    wifiTask,           // Task function
    "WiFiTask",         // Task name
    8192,               // Stack size (8KB - WiFiManager needs significant stack)
    NULL,               // Parameters
    1,                  // Priority (same as main loop)
    &wifiTaskHandle,    // Task handle
    0                   // Core 0 (WiFi core)
  );
  
  LOG_DEBUG("WiFi task created on Core 0, main loop on Core %d", xPortGetCoreID());
}


//=====================================
// Main Loop 
//=====================================
void loop() {
  //-----------------------------------
  // Timing
  //-----------------------------------
  uint32_t currentTime = millis();
  
  // Periodic heap monitoring (every 10 seconds in dev mode)
  #ifdef CHECK_HEAP
  static uint32_t lastHeapCheck = 0;
  if (currentTime - lastHeapCheck > 10000) {
    lastHeapCheck = currentTime;
    LOG_DEBUG("Heap: free=%u, min=%u, maxAlloc=%u", 
              ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
  }
  #endif
  
  //-----------------------------------
  // LED Animation Updates
  //-----------------------------------
  // Update LED effects 
  updateLEDs();
  
  // NOTE: WiFi portal (wm.process), MQTT, and background saves are now
  // handled by wifiTask() running on Core 0 for reliable operation

  //-----------------------------------
  // Button Processing
  //-----------------------------------
  // Check for button presses and process them
  button1.loop();
  button2.loop();

  //-----------------------------------
  // Sensor Reinitialization (with exponential backoff)
  //-----------------------------------
  // If sensor not initialized, attempt reinitialization with backoff
  if (!cameraInitialized) {
    // Show error on screen if not already displayed
    if (!cameraErrorDisplayed) {
      displayError("Sensor Not Responding");
      backlightOn();
      cameraErrorDisplayed = true;
      LOG_ALWAYS("Sensor error displayed - will retry with backoff");
    }
    
    // Non-blocking retry with exponential backoff
    if (currentTime - lastCameraRetry >= cameraBackoffInterval) {
      lastCameraRetry = currentTime;
      LOG_DEBUG("Attempting to reinitialize sensor (backoff: %lu sec)...", cameraBackoffInterval / 1000);
      
      cameraInitialized = initCamera();
      
      if (cameraInitialized) {
        // Success - reset backoff and clear error display
        cameraBackoffInterval = 5000;
        cameraErrorDisplayed = false;
        clearDisplay();
        LOG_ALWAYS("Sensor reinitialized successfully");
      } else {
        // Failed - increase backoff interval (cap at 1 hour)
        cameraBackoffInterval = min(cameraBackoffInterval * 2, CAMERA_BACKOFF_MAX);
        LOG_DEBUG("Sensor reinit failed, next attempt in %lu seconds", cameraBackoffInterval / 1000);
      }
    }
    // Don't return - allow rest of loop to process (buttons, etc.)
  }
  
  // NOTE: MQTT is now handled by wifiTask() on Core 0
  
  //-----------------------------------
  // Camera Data Acquisition
  //-----------------------------------
  // Adaptive polling rate based on tracking state:
  // - When idle (WAITING_FOR_IR): 20Hz (50ms)
  // - When tracking (READY/RECORDING): 100Hz (10ms) - responsive gesture capture
  // Skip camera processing when in settings mode (still process during spell recording)
  // Skip entirely if camera/sensor is not initialized
  uint32_t cameraInterval = isTrackingActive() ? 10 : 50;
  
  if (cameraInitialized && !inSettingsMode && currentTime - lastReadTime >= cameraInterval) {
    readCameraData();  // Process IR tracking and gesture recognition
    lastReadTime = currentTime;
  }

  //-----------------------------------
  // Screen Timeout Handling
  //-----------------------------------
  // Screen and LED timeout handling
  
  // Check if the spell name has been displayed long enough
  if (screenSpellOnTime > 0 && (millis() - screenSpellOnTime >= screenSpellDuration)) {
    // Clear spell name after 3 seconds
    screenSpellOnTime = 0;
    // Restore idle background after spell display timeout
    clearDisplay();
  }

  // Check if the screen has been on for too long without activity
  // Check if the screen has been on for too long without activity
  // Skip timeout while user is interacting with settings menu so device
  // doesn't appear unresponsive during configuration.
  if (!inSettingsMode && backlightStateOn && (millis() - screenOnTime >= screenTimeout)) {
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
  if (nightlightActive && nightlightCalculatedTimeout > 0 && 
      (millis() - nightlightOnTime >= nightlightCalculatedTimeout)) {
    // Turn off nightlight after calculated duration (sunrise or fixed timeout)
    nightlightActive = false;
    setLEDMode(LED_OFF);
    LOG_DEBUG("Nightlight mode timed out - LEDs turned off");
  }
}