// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

/*
IR Wand Tracker - Main Application Code
Uses ESP32-S3 with Pixart IR Camera to track wand movements and recognize gestures.
*/



#include <ESPmDNS.h>

#include "led_control.h"
#include "spell_patterns.h"
#include "spell_matching.h"
#include "webFunctions.h"
#include "preferenceFunctions.h"
#include "wandCommander.h"
#include "cameraFunctions.h"
#include "wifiFunctions.h"
#include "screenFunctions.h"
#include "sdFunctions.h"




// Spell detection parameters (loaded from preferences in setup())
int MOVEMENT_THRESHOLD;
int STILLNESS_THRESHOLD;
int READY_STILLNESS_TIME;
int END_STILLNESS_TIME;
int GESTURE_TIMEOUT;
int IR_LOSS_TIMEOUT;



unsigned long screenSpellOnTime = 0;
const unsigned long screenSpellDuration = 3000; // Display spell name for 3 seconds
unsigned long screenOnTime = 0;
const unsigned long screenTimeout = 60000; // Turn off screen after 60 seconds of inactivity

unsigned long ledOnTime = 0;
const unsigned long ledeffectTimeout = 5000; // Turn off LEDs after 5 seconds of inactivity

bool backlightStateOn = false;
bool cameraInitialized = false;
uint32_t lastReadTime = 0;



// Scan I2C bus for devices
void scanI2C() {
  LOG_DEBUG("\n=== Scanning I2C Bus ===");
  int devicesFound = 0;
  
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

void setup() {
  // Initialize Serial for debugging
  Serial.begin(115200);
  delay(1000); // Give serial time to initialize
  
  LOG_DEBUG("\n\n=================================");
  LOG_DEBUG("ESP32-S3 Interactive Wand");
  LOG_DEBUG("=================================\n");
  
  // Variable to track setup step for the setup display
  int step = 1;
  // Initialize screen
  screenInit();
  // Turn on backlight
  backlightOn();
  // Update setup display with disaply status
  updateSetupDisplay(step, "Display", "pass");
  // Increment step
  step++;

  // Initialize LEDs
  updateSetupDisplay(step, "LEDs", "init");
  initLEDs();
  // turn off LEDS
  ledOff();
  // Update display for LED status
  updateSetupDisplay(step, "LEDs", "pass");
  step++;

  // Load preferences
  updateSetupDisplay(step, "Preferences", "init");
  loadPreferences();
  
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
  
  // Setup WiFi with WiFiManager
  updateSetupDisplay(step, "WiFi Manager", "init");
  bool wifiConnected = initWM();
  
  if (wifiConnected) {
    updateSetupDisplay(step, "WiFi Manager", "pass");
    // Configure mDNS
    if (!MDNS.begin("glyphreader")) {
      LOG_ALWAYS("Error setting up MDNS responder!");
    } else {
      LOG_DEBUG("mDNS responder started: http://glyphreader.local");
      MDNS.addService("http", "tcp", 80);
    }
  } else {
    // WiFi not connected - offline mode
    updateSetupDisplay(step, "WiFi Manager", "offline");
    LOG_ALWAYS("Continuing in offline mode - web portal available at 192.168.4.1");
  }
  step++;
  
  // Generate unique client ID from MAC address
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(mqttClientId, sizeof(mqttClientId), "GlyphReader-%02X%02X%02X", mac[3], mac[4], mac[5]);
  LOG_DEBUG("Device ID: %s", mqttClientId);
  
  // Setup MQTT (only if WiFi connected AND MQTT host is configured)
  updateSetupDisplay(step, "MQTT", "init");
  
  if (wifiConnected && MQTT_HOST.length() > 0) {
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

  // Initialize I2C
  updateSetupDisplay(step, "Camera", "init");
  LOG_DEBUG("Initializing I2C (SDA=%d, SCL=%d)...\n", I2C_SDA, I2C_SCL);
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000); // 400kHz I2C speed for faster updates
  Wire.setTimeOut(500); // 500ms timeout
  
  // Scan for I2C devices
  scanI2C();
  
  // Initialize spell patterns
  initSpellPatterns();
  
  // Apply custom spell configurations from SD card (if present)
  if (sdCardReady) {
    applyCustomSpells();
  }
  
  // Check for spell image files (must be after initSpellPatterns and applyCustomSpells)
  if (sdCardReady) {
    checkSpellImages();
  }
  
  // Initialize camera
  cameraInitialized = initCamera();
  
  if (!cameraInitialized) {
    LOG_ALWAYS("ERROR: Failed to initialize camera!");
    LOG_ALWAYS("Check your wiring:");
    LOG_ALWAYS("  SDA -> GPIO %d", I2C_SDA);
    LOG_ALWAYS("  SCL -> GPIO %d", I2C_SCL);
    LOG_ALWAYS("  VCC -> 3.3V");
    LOG_ALWAYS("  GND -> GND");
  }

  else {
    updateSetupDisplay(step, "Camera", "pass");
    step++;
  }
  // artificial delay to let user see final status
  delay(1000);
  // Clear setup display
  clearDisplay();
  // Set screen on time
  screenOnTime = millis();  
}

void loop() {
  // Update LED effects (non-blocking)
  updateLEDs();
  
  // Handle web portal
  wm.process();

  // If camera not initialized, attempt reinitialization
  if (!cameraInitialized) {
    // Try to reinitialize every 5 seconds
    delay(5000);
    cameraInitialized = initCamera();
    return;
  }
  
  // Maintain MQTT connection
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();
  
  // Read camera data at high speed (aiming for ~100Hz)
  uint32_t currentTime = millis();
  if (currentTime - lastReadTime >= 10) { // Read every 10ms (100Hz)
    readCameraData();
    lastReadTime = currentTime;
  }
  
  // Minimal delay to prevent watchdog issues
  delayMicroseconds(100);

  // Screen and LED timeout handling
  // check if the spell name has been displayed long enough
  if (screenSpellOnTime > 0 && (millis() - screenSpellOnTime >= screenSpellDuration)) {
    // Clear spell name
    screenSpellOnTime = 0;
    clearDisplay();
  }

  // check if the screen has been on for too long without activity
  if (backlightStateOn && (millis() - screenOnTime >= screenTimeout)) {
    // Turn off backlight
    backlightOff();
  }

  // check if the LEDs have been on for too long without activity
  if (ledOnTime > 0 && (millis() - ledOnTime >= ledeffectTimeout) && currentMode != LED_NIGHTLIGHT) {
    // Turn off LEDs
    setLEDMode(LED_OFF);
    ledOnTime = 0;
  }
}