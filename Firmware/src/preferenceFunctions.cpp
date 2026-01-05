/*
This file holds all the functions related to the preferences library
*/

#include <wandCommander.h>
#include "preferenceFunctions.h"

// Preferences instance
Preferences preferences;

// Preference specifications
struct PrefSpec { const char* name; PrefType type; };

// Define preference specifications
static const PrefSpec PREF_SPECS[] = {
#define PREF_X(name, type, nvs) { nvs, PrefType::type },
    PREF_LIST
#undef PREF_X
};

// Helper functions to get preference name and type
static const char* prefName(PrefKey k) {
    int idx = static_cast<int>(k);
    if (idx < 0 || idx >= static_cast<int>(PrefKey::COUNT)) return "";
    return PREF_SPECS[idx].name;
}
static PrefType prefType(PrefKey k) {
    int idx = static_cast<int>(k);
    if (idx < 0 || idx >= static_cast<int>(PrefKey::COUNT)) return PrefType::STRING;
    return PREF_SPECS[idx].type;
}

// Functions for fetching preference values
bool getPrefBool(PrefKey key, bool def) {
    if (prefType(key) != PrefType::BOOL) {
        LOG_DEBUG("getPrefBool wrong key type: %s", prefName(key));
        return def;
    }
    preferences.begin("settings", true);
    bool v = preferences.getBool(prefName(key), def);
    preferences.end();
    return v;
}

int getPrefInt(PrefKey key, int def) {
    if (prefType(key) != PrefType::INT) {
        LOG_DEBUG("getPrefInt wrong key type: %s", prefName(key));
        return def;
    }
    preferences.begin("settings", true);
    int v = preferences.getInt(prefName(key), def);
    preferences.end();
    return v;
}

String getPrefString(PrefKey key, const String &def) {
    if (prefType(key) != PrefType::STRING) {
        LOG_DEBUG("getPrefString wrong key type: %s", prefName(key));
        return def;
    }
    preferences.begin("settings", true);
    String v = preferences.getString(prefName(key), def);
    preferences.end();
    return v;
}

// Functions for setting preference values
void setPref(PrefKey key, bool val) {
    if (prefType(key) != PrefType::BOOL) {
        LOG_DEBUG("setPref(bool) wrong key type: %s", prefName(key));
        return;
    }
    preferences.begin("settings", false);
    preferences.putBool(prefName(key), val);
    preferences.end();
}

void setPref(PrefKey key, int val) {
    if (prefType(key) != PrefType::INT) {
        LOG_DEBUG("setPref(int) wrong key type: %s", prefName(key));
        return;
    }
    preferences.begin("settings", false);
    preferences.putInt(prefName(key), val);
    preferences.end();
}

void setPref(PrefKey key, const String &val) {
    if (prefType(key) != PrefType::STRING) {
        LOG_DEBUG("setPref(String) wrong key type: %s", prefName(key));
        return;
    }
    preferences.begin("settings", false);
    preferences.putString(prefName(key), val);
    preferences.end();
}

// Load all preferences into global variables
// Default values are provided if no stored overrides exist
void loadPreferences() {
    MQTT_HOST = getPrefString(PrefKey::MQTT_HOST, "");  // Empty by default - user must configure
    MQTT_PORT = getPrefInt(PrefKey::MQTT_PORT, 1883);
    MQTT_TOPIC = getPrefString(PrefKey::MQTT_TOPIC, ""); // Empty by default - user must configure
    MOVEMENT_THRESHOLD = getPrefInt(PrefKey::MOVEMENT_THRESHOLD, 15);
    STILLNESS_THRESHOLD = getPrefInt(PrefKey::STILLNESS_THRESHOLD, 20);
    READY_STILLNESS_TIME = getPrefInt(PrefKey::READY_STILLNESS_TIME, 600);
    END_STILLNESS_TIME = getPrefInt(PrefKey::END_STILLNESS_TIME, 500);
    GESTURE_TIMEOUT = getPrefInt(PrefKey::GESTURE_TIMEOUT, 5000);
    IR_LOSS_TIMEOUT = getPrefInt(PrefKey::IR_LOSS_TIMEOUT, 300);
    
}
    
