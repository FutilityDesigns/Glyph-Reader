/*
================================================================================
  Preference Functions - NVS Persistent Storage Header
================================================================================
  
  This module manages persistent configuration storage using ESP32's Non-Volatile
  Storage (NVS) system. All user-configurable settings are stored here and
  survive power cycles and firmware updates.
  
  Design Pattern - X-Macro:
    - PREF_LIST macro defines all preferences in one place
    - Automatically generates: enum, accessor functions, default values
    - Single source of truth prevents inconsistencies
    - Easy to add new preferences
  
  Global Variables:
    - All preferences are exposed as global variables for easy access
    - Initialized by loadPreferences() during setup()
    - Modified via web portal and saved back to NVS
  
================================================================================
*/

#ifndef PREFERNCE_FUNCTIONS_H
#define PREFERNCE_FUNCTIONS_H
#include <Preferences.h>

//=====================================
// Global Preferences Object
//=====================================

/// ESP32 NVS preferences object (defined in preferenceFunctions.cpp)
extern Preferences preferences;

//=====================================
// Preference Type Enumeration
//=====================================

/**
 * Data types for NVS preferences
 * Used internally to determine which NVS read/write function to use.
 */
enum class PrefType { 
  BOOL,    ///< Boolean value (true/false)
  INT,     ///< Integer value (32-bit signed)
  STRING   ///< String value (null-terminated)
};

//=====================================
// Preference List (X-Macro Pattern)
//=====================================

/**
 * Master list of all preferences
 * X-Macro pattern: Define once, use multiple times
 * Format: PREF_X(EnumName, Type, "nvsKey")
 * To add a new preference:
 *   1. Add PREF_X line here
 *   2. Add extern declaration at bottom of this file
 *   3. Add default value in loadPreferences() (preferenceFunctions.cpp)
 */
#define PREF_LIST \
    PREF_X(MQTT_HOST,            STRING, "mqttHost")    \
    PREF_X(MQTT_PORT,            INT,    "mqttPort")    \
    PREF_X(MQTT_TOPIC,           STRING, "mqttTopic")    \
    PREF_X(MOVEMENT_THRESHOLD,   INT,    "movThreshold")    \
    PREF_X(STILLNESS_THRESHOLD,  INT,    "stillThreshold")    \
    PREF_X(READY_STILLNESS_TIME, INT,    "readyStillTime")    \
    PREF_X(GESTURE_TIMEOUT,      INT,    "gestureTimeout")    \
    PREF_X(IR_LOSS_TIMEOUT,      INT,    "irLossTimeout")    \
    PREF_X(NIGHTLIGHT_ON_SPELL,  STRING, "NLon")    \
    PREF_X(NIGHTLIGHT_OFF_SPELL, STRING, "NLoff")   \
    PREF_X(NIGHTLIGHT_BRIGHTNESS, INT,   "NLbright") \
    PREF_X(LATITUDE,             STRING, "latitude")    \
    PREF_X(LONGITUDE,            STRING, "longitude")    \
    PREF_X(TIMEZONE_OFFSET,      INT,    "tzOffset")    \

//=====================================
// Preference Key Enumeration
//=====================================

/**
 * Preference key enumeration
 * Generated from PREF_LIST macro.
 * Used as type-safe keys for get/set functions.
 */
enum class PrefKey {
#define PREF_X(name, type, nvsKey) name,
    PREF_LIST
#undef PREF_X
    COUNT  ///< Total number of preferences
};

//=====================================
// Preference Access Functions
//=====================================

/**
 * Get boolean preference value
 * key: Preference key (from PrefKey enum)
 * defaultValue: Value to return if preference not set
 * return Stored value or defaultValue if not found
 */
bool getPrefBool(PrefKey key, bool defaultValue);

/**
 * Get integer preference value
 * key: Preference key (from PrefKey enum)
 * defaultValue: Value to return if preference not set
 * return Stored value or defaultValue if not found
 */
int getPrefInt(PrefKey key, int defaultValue);

/**
 * Get string preference value
 * key: Preference key (from PrefKey enum)
 * defaultValue: Value to return if preference not set
 * return Stored value or defaultValue if not found
 */
String getPrefString(PrefKey key, const String& defaultValue);

/**
 * Set boolean preference value
 * Writes value to NVS immediately.
 * key: Preference key (from PrefKey enum)
 * value: Boolean value to store
 */
void setPref(PrefKey key, bool value);

/**
 * Set integer preference value
 * Writes value to NVS immediately.
 * key: Preference key (from PrefKey enum)
 * value: Integer value to store
 */
void setPref(PrefKey key, int value);

/**
  Set string preference value
 * Writes value to NVS immediately.
 * key: Preference key (from PrefKey enum)
 * value: String value to store
 */
void setPref(PrefKey key, const String& value);

/**
 * Load all preferences into global variables
 * Reads all NVS preferences and initializes global variables.
 * Uses default values if preference not previously set.
 * Must be called during setup() before using any preference values.
 */
void loadPreferences();

//=====================================
// Global Preference Variables
//=====================================

// MQTT Configuration
extern String MQTT_HOST;          ///< MQTT broker hostname/IP
extern int MQTT_PORT;             ///< MQTT broker port (default 1883)
extern String MQTT_TOPIC;         ///< MQTT topic for spell publications

// Gesture Tuning Parameters
extern int MOVEMENT_THRESHOLD;    ///< Pixels of movement to trigger recording (default 30)
extern int STILLNESS_THRESHOLD;   ///< Maximum drift while "still" (default 10)
extern int READY_STILLNESS_TIME;  ///< Milliseconds of stillness for ready state (default 1000)
extern int END_STILLNESS_TIME;    ///< Milliseconds of stillness to end gesture (default 500)
extern int GESTURE_TIMEOUT;       ///< Maximum milliseconds for gesture (default 5000)
extern int IR_LOSS_TIMEOUT;       ///< Milliseconds before IR loss confirmed (default 200)

// Nightlight Configuration
extern String NIGHTLIGHT_ON_SPELL;   ///< Spell name to activate nightlight (e.g., "Illuminate")
extern String NIGHTLIGHT_OFF_SPELL;  ///< Spell name to deactivate nightlight (e.g., "Dark")
extern int NIGHTLIGHT_BRIGHTNESS;    ///< Nightlight brightness (0-255, default 150)

// Location Configuration (for sunrise/sunset calculations)
extern String LATITUDE;           ///< Device latitude in decimal degrees (e.g., "37.7749")
extern String LONGITUDE;          ///< Device longitude in decimal degrees (e.g., "-122.4194")
extern int TIMEZONE_OFFSET;       ///< Timezone offset from UTC in seconds (e.g., -28800 for PST)

#endif // PREFERNCE_FUNCTIONS_H
