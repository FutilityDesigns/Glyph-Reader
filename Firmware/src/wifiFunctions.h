/*
================================================================================
  WiFi Functions - MQTT Client Management Header
================================================================================
  
  This module manages the MQTT client for publishing spell detection events
  to a network broker. Uses the PubSubClient library for MQTT communication.
  
  MQTT (Message Queuing Telemetry Transport):
    - Lightweight publish/subscribe messaging protocol
    - Ideal for IoT devices and home automation
    - Low bandwidth, reliable delivery
  
  Integration:
    - Broker address/port configured via web portal (stored in NVS)
    - Topic configurable (default: "glyphreader/spell")
    - Publishes spell name when gesture is detected
    - Auto-reconnect on connection loss
  
  Message Format:
    - Topic: [configured MQTT_TOPIC]
    - Payload: Spell name (e.g., "Ignite")
    - QoS: 0 (at most once delivery)
  
  Example Home Assistant Integration:
    - Subscribe to topic in HA automation
    - Trigger actions based on spell name
    - Example: "Ignite" -> Turn on lights  
================================================================================
*/

#ifndef MQTT_FUNCTIONS_H
#define MQTT_FUNCTIONS_H

#include <PubSubClient.h>
#include <WiFi.h>

//=====================================
// Global MQTT Objects
//=====================================

/// WiFi client for MQTT connection (defined in wifiFunctions.cpp)
extern WiFiClient espClient;

/// PubSubClient MQTT client object (defined in wifiFunctions.cpp)
extern PubSubClient mqttClient;

/// Unique MQTT client ID (generated from MAC address)
/// Format: "GlyphReader-XXXXXX" where X = last 3 bytes of MAC
extern char mqttClientId[32];

//=====================================
// MQTT Functions
//=====================================

/**
 * Maintain MQTT connection with auto-reconnect
 * Attempts to connect/reconnect to MQTT broker if connection is lost.
 * Uses credentials from NVS preferences (MQTT_HOST, MQTT_PORT).
 * Should be called regularly from main loop() to maintain connection.
 */
void reconnectMQTT();

/**
 * Publish spell detection event to MQTT broker
 * Sends spell name to configured MQTT topic.
 * Only publishes if MQTT client is connected.
 * spellName: Name of detected spell (e.g., "Ignite")
 * Topic: Configured via MQTT_TOPIC preference
 * Payload: Spell name as plain text
 * QoS: 0 (at most once delivery)
 */
void publishSpell(const char* spellName);

#endif // MQTT_FUNCTIONS_H