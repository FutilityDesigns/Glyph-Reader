#ifndef PREFERNCE_FUNCTIONS_H
#define PREFERNCE_FUNCTIONS_H
#include <Preferences.h>

extern Preferences preferences;

enum class PrefType { BOOL, INT, STRING };

// Single source list: PREF_X(EnumName, Type, "nvsKey", defaultValue)
#define PREF_LIST \
    PREF_X(MQTT_HOST,            STRING, "mqttHost")    \
    PREF_X(MQTT_PORT,            INT,    "mqttPort")    \
    PREF_X(MQTT_TOPIC,           STRING, "mqttTopic")    \
    PREF_X(MOVEMENT_THRESHOLD,   INT,    "movementThreshold")    \
    PREF_X(STILLNESS_THRESHOLD,  INT,    "stillnessThreshold")    \
    PREF_X(READY_STILLNESS_TIME, INT,    "readyStillnessTime")    \
    PREF_X(END_STILLNESS_TIME,   INT,    "endStillnessTime")    \
    PREF_X(GESTURE_TIMEOUT,      INT,    "gestureTimeout")    \
    PREF_X(IR_LOSS_TIMEOUT,      INT,    "irLossTimeout")    \

enum class PrefKey {
#define PREF_X(name, type, nvsKey) name,
    PREF_LIST
#undef PREF_X
    COUNT
};

// Functions for getting and setting Preferences
bool getPrefBool(PrefKey key, bool defaultValue);
int getPrefInt(PrefKey key, int defaultValue);
String getPrefString(PrefKey key, const String& defaultValue);

void setPref(PrefKey key, bool value);
void setPref(PrefKey key, int value);
void setPref(PrefKey key, const String& value);

void loadPreferences();

extern String MQTT_HOST;
extern int MQTT_PORT;
extern String MQTT_TOPIC;
extern int MOVEMENT_THRESHOLD;
extern int STILLNESS_THRESHOLD;
extern int READY_STILLNESS_TIME;
extern int END_STILLNESS_TIME;
extern int GESTURE_TIMEOUT;
extern int IR_LOSS_TIMEOUT;

#endif