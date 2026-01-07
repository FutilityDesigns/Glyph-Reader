/**
================================================================================
  WiFi Functions - MQTT Client Management 
================================================================================
 * 
 * This module manages MQTT connectivity for publishing recognized spell events
 * to a home automation system or other MQTT-capable platforms.
 * 
 * MQTT CONFIGURATION:
 * - Broker address/port loaded from NVS preferences (set via WiFiManager portal)
 * - Client ID: Generated from ESP32 MAC address for uniqueness
 * - Topic: Configurable via preferences (e.g., "home/wand/spell")
 * - QoS: Fire-and-forget (QoS 0) - spells published best-effort
 * 
 * SPELL PUBLISHING:
 * - When spell is recognized, spell name is published to configured MQTT topic
 * - Example: "Lumos" published to "home/wand/spell" topic
 * - Only publishes if MQTT client is connected
 * - Silent failure if not connected (non-critical feature)
 * 
 * USAGE FLOW:
 * 1. WiFiManager connects to WiFi and configures MQTT settings
 * 2. reconnectMQTT() called in main loop to maintain connection
 * 3. When spell detected, publishSpell(name) sends to broker
 * 
 * INTEGRATION:
 * - Home Assistant: Create MQTT sensor listening to spell topic
 * - Node-RED: Subscribe to spell events for automation flows
 * - OpenHAB: MQTT binding to trigger rules based on spells
 * 
 */

#include "wifiFunctions.h"
#include "glyphReader.h"
#include "preferenceFunctions.h"

// WiFi and MQTT
WiFiClient espClient;
PubSubClient mqttClient(espClient);
char mqttClientId[32];
uint32_t lastMqttReconnect = 0;

/**
 * Maintain MQTT broker connection with auto-reconnect
 * Called repeatedly from main loop to ensure MQTT stays connected.
 * Uses non-blocking retry logic to avoid freezing main loop.
 * BEHAVIOR:
 * - Does nothing if MQTT_HOST is empty (MQTT disabled)
 * - Only attempts reconnect if WiFi is connected
 * - Retry interval: 5 seconds between connection attempts
 * - Logs connection status to serial console
 * Called from main loop every iteration.
 */
void reconnectMQTT() {
  // Don't try to connect if no MQTT host is configured
  if (MQTT_HOST.length() == 0) {
    return;  // MQTT disabled - skip
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
        Serial.println(mqttClient.state());  // Print error code
      }
    }
  }
}

/**
 * Publish recognized spell name to MQTT broker
 * spellName: Null-terminated string containing spell name (e.g., "Illuminate")
 * Sends spell name as MQTT message payload to configured topic.
 * Only publishes if MQTT client is currently connected to broker.
 * MQTT MESSAGE FORMAT:
 * - Topic: From MQTT_TOPIC preference (e.g., "home/wand/spell")
 * - Payload: Raw spell name string (no JSON wrapping)
 * - QoS: 0 (fire-and-forget, no delivery guarantee)
 * - Retain: false (not a persistent state)
 * Called from spell_matching.cpp when spell confidence exceeds threshold.
 */
void publishSpell(const char* spellName) {
  if (mqttClient.connected()) {
    Serial.printf("Publishing spell to MQTT: %s\n", spellName);
    mqttClient.publish(MQTT_TOPIC.c_str(), spellName);  // Send spell name to topic
  } else {
    Serial.println("MQTT not connected, cannot publish spell");
  }
}