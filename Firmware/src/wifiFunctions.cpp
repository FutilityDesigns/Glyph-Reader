/*
Functions related to WiFi and MQTT
*/
#include "wifiFunctions.h"
#include "wandCommander.h"
#include "preferenceFunctions.h"

// WiFi and MQTT
WiFiClient espClient;
PubSubClient mqttClient(espClient);
char mqttClientId[32];
uint32_t lastMqttReconnect = 0;

// MQTT Configuration (loaded from preferences)
String MQTT_HOST = "";  // Will be loaded from preferences
int MQTT_PORT = 1883;
String MQTT_TOPIC = "";


// MQTT reconnect
void reconnectMQTT() {
  // Don't try to connect if no MQTT host is configured
  extern String MQTT_HOST;
  if (MQTT_HOST.length() == 0) {
    return;
  }
  
  if (!mqttClient.connected() && WiFi.status() == WL_CONNECTED) {
    uint32_t now = millis();
    if (now - lastMqttReconnect > 5000) {  // Try reconnect every 5 seconds
      lastMqttReconnect = now;
      Serial.print("Attempting MQTT connection...");
      if (mqttClient.connect(mqttClientId)) {
        Serial.println("connected");
      } else {
        Serial.print("failed, rc=");
        Serial.println(mqttClient.state());
      }
    }
  }
}

// Publish spell to MQTT
void publishSpell(const char* spellName) {
  if (mqttClient.connected()) {
    Serial.printf("Publishing spell to MQTT: %s\n", spellName);
    mqttClient.publish(MQTT_TOPIC.c_str(), spellName);
  } else {
    Serial.println("MQTT not connected, cannot publish spell");
  }
}