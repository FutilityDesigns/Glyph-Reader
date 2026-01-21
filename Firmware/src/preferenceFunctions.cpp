/**
 ================================================================================
  Preference Functions - ESP32 NVS (Non-Volatile Storage) Preferences Management
================================================================================
 * 
 * This module provides a type-safe interface to the ESP32's NVS flash storage system
 * for persisting configuration settings across power cycles and reboots.
 * 
 * ARCHITECTURE:
 * - Uses ESP32 Preferences library to store key-value pairs in NVS flash partition
 * - X-Macro pattern (PREF_LIST) centralizes preference definitions in preferenceFunctions.h
 * - Type-safe getters/setters enforce correct data types for each preference key
 * - Global variables cache preference values for fast runtime access
 * 
 * X-MACRO PATTERN:
 * - PREF_LIST in header defines all preferences once: PREF_X(VARIABLE, TYPE, NVS_KEY)
 * - Generates PrefKey enum, PrefSpec array, and global variable declarations
 * - Modifications only need to update PREF_LIST, not multiple locations
 * 
 * USAGE EXAMPLE:
 *   loadPreferences();  // Read all NVS values into global variables
 *   setPref(PrefKey::MQTT_HOST, "192.168.1.100");  // Save new value to NVS
 *   String host = MQTT_HOST;  // Access cached global variable
 */

#include "glyphReader.h"
#include "preferenceFunctions.h"

// Preferences instance
Preferences preferences;

// Preference specifications
struct PrefSpec { const char* name; PrefType type; };

// Global preference variables
String MQTT_HOST;
int MQTT_PORT;
String MQTT_TOPIC;
int MOVEMENT_THRESHOLD;
int STILLNESS_THRESHOLD;
int READY_STILLNESS_TIME;
int END_STILLNESS_TIME;
int GESTURE_TIMEOUT;
int IR_LOSS_TIMEOUT;
String NIGHTLIGHT_ON_SPELL;
String NIGHTLIGHT_OFF_SPELL;
int NIGHTLIGHT_BRIGHTNESS;
String LATITUDE;
String LONGITUDE;
int TIMEZONE_OFFSET;

// Define preference specifications
static const PrefSpec PREF_SPECS[] = {
#define PREF_X(name, type, nvs) { nvs, PrefType::type },
    PREF_LIST
#undef PREF_X
};

/**
 * Get NVS key name for a preference enum value
 * k: PrefKey enum value
 * return Const char* to NVS key string, or "" if invalid
 * Maps PrefKey enum to the actual string key used in NVS storage.
 * Includes bounds checking to prevent array overruns.
 */
static const char* prefName(PrefKey k) {
    int idx = static_cast<int>(k);
    if (idx < 0 || idx >= static_cast<int>(PrefKey::COUNT)) return "";  // Out of bounds
    return PREF_SPECS[idx].name;
}

/**
 * Get data type for a preference enum value
 * k: PrefKey enum value
 * return PrefType enum (BOOL, INT, or STRING)
 * Used by getter/setter functions to verify type-safe access.
 * Returns STRING as safe default if key is invalid.
 */
static PrefType prefType(PrefKey k) {
    int idx = static_cast<int>(k);
    if (idx < 0 || idx >= static_cast<int>(PrefKey::COUNT)) return PrefType::STRING; 
    return PREF_SPECS[idx].type;
}

/**
 * Read boolean preference from NVS
 * key: PrefKey enum identifying which preference to read
 * def: Default value to return if not found in NVS or type mismatch
 * return Boolean value from NVS, or default if not set
 * Opens NVS in read-only mode, reads value, then closes.
 * Validates key type matches BOOL before reading.
 */
bool getPrefBool(PrefKey key, bool def) {
    if (prefType(key) != PrefType::BOOL) {
        LOG_DEBUG("getPrefBool wrong key type: %s", prefName(key));
        return def;  // Type mismatch - return default
    }
    preferences.begin("settings", true);  // Open read-only
    bool v = preferences.getBool(prefName(key), def);
    preferences.end();
    return v;
}

/**
 * Read integer preference from NVS
 * key: PrefKey enum identifying which preference to read
 * def: Default value to return if not found in NVS or type mismatch
 * return Integer value from NVS, or default if not set
 * Validates key type matches INT before reading.
 */
int getPrefInt(PrefKey key, int def) {
    if (prefType(key) != PrefType::INT) {
        LOG_DEBUG("getPrefInt wrong key type: %s", prefName(key));
        return def;  // Type mismatch - return default
    }
    preferences.begin("settings", true);  // Open read-only
    int v = preferences.getInt(prefName(key), def);
    preferences.end();
    return v;
}

/**
 * Read String preference from NVS
 * key: PrefKey enum identifying which preference to read
 * def: Default value to return if not found in NVS or type mismatch
 * return String value from NVS, or default if not set
 * Used for hostnames, topics, and spell names.
 * Validates key type matches STRING before reading.
 */
String getPrefString(PrefKey key, const String &def) {
    if (prefType(key) != PrefType::STRING) {
        LOG_DEBUG("getPrefString wrong key type: %s", prefName(key));
        return def;  // Type mismatch - return default
    }
    preferences.begin("settings", true);  // Open read-only
    String v = preferences.getString(prefName(key), def);
    preferences.end();
    return v;
}

/**
 * Write boolean preference to NVS flash
 * key: PrefKey enum identifying which preference to write
 * val: Boolean value to store
 * Opens NVS in write mode, saves value to flash, then closes.
 * Validates key type matches BOOL before writing.
 */
void setPref(PrefKey key, bool val) {
    if (prefType(key) != PrefType::BOOL) {
        LOG_DEBUG("setPref(bool) wrong key type: %s", prefName(key));
        return;  // Type mismatch - abort write
    }
    preferences.begin("settings", false);  // Open read-write
    preferences.putBool(prefName(key), val);
    preferences.end();  // Commit to flash
}

/**
 * Write integer preference to NVS flash
 * key: PrefKey enum identifying which preference to write
 * val: Integer value to store
 * Used for thresholds, timeouts, and port numbers.
 * Validates key type matches INT before writing.
 */
void setPref(PrefKey key, int val) {
    if (prefType(key) != PrefType::INT) {
        LOG_DEBUG("setPref(int) wrong key type: %s", prefName(key));
        return;  // Type mismatch - abort write
    }
    preferences.begin("settings", false);  // Open read-write
    preferences.putInt(prefName(key), val);
    preferences.end();  // Commit to flash
}

/**
 * Write String preference to NVS flash
 * key: PrefKey enum identifying which preference to write
 * val: String value to store (max length depends on NVS partition)
 * Used for hostnames, topics, and spell names.
 * Validates key type matches STRING before writing.
 */
void setPref(PrefKey key, const String &val) {
    if (prefType(key) != PrefType::STRING) {
        LOG_DEBUG("setPref(String) wrong key type: %s", prefName(key));
        return;  // Type mismatch - abort write
    }
    preferences.begin("settings", false);  // Open read-write
    preferences.putString(prefName(key), val);
    preferences.end();  // Commit to flash
}

/**
 * Load all preferences from NVS into global variables
 * Called once during boot (in main.cpp setup()) to populate global preference
 * variables from NVS flash. If a preference has never been set, the default
 * value is used instead.
 * 
 * MQTT DEFAULTS:
 * - MQTT_HOST: "" (empty - user must configure via web portal)
 * - MQTT_PORT: 1883 (standard MQTT port)
 * - MQTT_TOPIC: "" (empty - user must configure)
 * 
 * MOTION DETECTION DEFAULTS:
 * - MOVEMENT_THRESHOLD: 15 pixels (minimum motion to start tracking)
 * - STILLNESS_THRESHOLD: 20 pixels (maximum motion still considered "still")
 * - READY_STILLNESS_TIME: 600ms (wand must be still this long to enter READY state)
 * - GESTURE_TIMEOUT: 5000ms (max time for gesture before timeout)
 * - IR_LOSS_TIMEOUT: 300ms (max time IR can be lost before ending gesture)
 * 
 * NIGHTLIGHT DEFAULTS:
 * - NIGHTLIGHT_ON_SPELL: "" (no spell assigned)
 * - NIGHTLIGHT_OFF_SPELL: "" (no spell assigned)
 */
void loadPreferences() {
    // MQTT Configuration
    MQTT_HOST = getPrefString(PrefKey::MQTT_HOST, "");  // Empty by default - user must configure
    MQTT_PORT = getPrefInt(PrefKey::MQTT_PORT, 1883);  // Standard MQTT port
    MQTT_TOPIC = getPrefString(PrefKey::MQTT_TOPIC, ""); // Empty by default - user must configure
    
    // Motion Detection Thresholds
    MOVEMENT_THRESHOLD = getPrefInt(PrefKey::MOVEMENT_THRESHOLD, 15);  // Pixels to start tracking
    STILLNESS_THRESHOLD = getPrefInt(PrefKey::STILLNESS_THRESHOLD, 20);  // Max pixels still "still"
    
    // Timing Parameters (milliseconds)
    READY_STILLNESS_TIME = getPrefInt(PrefKey::READY_STILLNESS_TIME, 600);  // Enter READY state
    GESTURE_TIMEOUT = getPrefInt(PrefKey::GESTURE_TIMEOUT, 5000);  // Max gesture duration
    IR_LOSS_TIMEOUT = getPrefInt(PrefKey::IR_LOSS_TIMEOUT, 300);  // Max IR loss time
    
    // Nightlight Control Spells
    NIGHTLIGHT_ON_SPELL = getPrefString(PrefKey::NIGHTLIGHT_ON_SPELL, "");  // No default spell
    NIGHTLIGHT_OFF_SPELL = getPrefString(PrefKey::NIGHTLIGHT_OFF_SPELL, "");  // No default spell    NIGHTLIGHT_BRIGHTNESS = getPrefInt(PrefKey::NIGHTLIGHT_BRIGHTNESS, 150);  // Default medium brightness}
    
    NIGHTLIGHT_BRIGHTNESS = getPrefInt(PrefKey::NIGHTLIGHT_BRIGHTNESS, 150);  // Default medium brightness
    
    // Location settings (empty = not configured, will fetch from ipapi.co on boot)
    LATITUDE = getPrefString(PrefKey::LATITUDE, "");
    LONGITUDE = getPrefString(PrefKey::LONGITUDE, "");
    TIMEZONE_OFFSET = getPrefInt(PrefKey::TIMEZONE_OFFSET, 0);
}