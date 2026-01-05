#ifndef MQTT_FUNCTIONS_H
#define MQTT_FUNCTIONS_H

#include <PubSubClient.h>
#include <WiFi.h>

extern WiFiClient espClient;
extern PubSubClient mqttClient;
extern char mqttClientId[32];

void reconnectMQTT();
void publishSpell(const char* spellName);

#endif