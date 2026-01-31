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
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <time.h>

// WiFi and MQTT
WiFiClient espClient;
PubSubClient mqttClient(espClient);
char mqttClientId[32];
uint32_t lastMqttReconnect = 0;
uint32_t mqttBackoffInterval = 5000;       // Start at 5 seconds, max 1 hour
const uint32_t MQTT_BACKOFF_MAX = 3600000; // 1 hour maximum backoff
bool mqttWasConnected = false;             // Track if we've ever connected this session

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
  
  // If connected, reset backoff and track connection state
  if (mqttClient.connected()) {
    if (!mqttWasConnected) {
      mqttWasConnected = true;
      mqttBackoffInterval = 5000;  // Reset backoff on successful connection
      LOG_DEBUG("MQTT connected - backoff reset to 5 seconds");
    }
    return;
  }
  
  // Not connected - check if we should attempt reconnection
  if (WiFi.status() == WL_CONNECTED) {
    uint32_t now = millis();
    if (now - lastMqttReconnect >= mqttBackoffInterval) {
      lastMqttReconnect = now;
      LOG_DEBUG("Attempting MQTT connection (backoff: %lu sec)...", mqttBackoffInterval / 1000);
      
      if (mqttClient.connect(mqttClientId)) {
        LOG_DEBUG("MQTT connected");
        mqttWasConnected = true;
        mqttBackoffInterval = 5000;  // Reset backoff on success
      } else {
        LOG_DEBUG("MQTT connection failed, rc=%d", mqttClient.state());
        
        // Exponential backoff: double the interval, cap at 1 hour
        mqttBackoffInterval = min(mqttBackoffInterval * 2, MQTT_BACKOFF_MAX);
        LOG_DEBUG("Next MQTT attempt in %lu seconds", mqttBackoffInterval / 1000);
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


JsonDocument* fetchJsonFromApi(const String& url, const JsonDocument& filter) {
    if (WiFi.status() != WL_CONNECTED) {
        LOG_ALWAYS("WiFi not connected!");
        return nullptr;
    }

    HTTPClient http;
    WiFiClientSecure client;
    
    LOG_DEBUG("Fetching URL: %s", url.c_str());
    
    // For HTTPS URLs, use insecure mode (skip certificate validation)
    if (url.startsWith("https://")) {
        client.setInsecure();  // Skip SSL certificate validation
        http.begin(client, url);
    } else {
        http.begin(url);
    }
    
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    int httpCode = http.GET();
    LOG_DEBUG("HTTP code: %d", httpCode);

    if (httpCode != HTTP_CODE_OK) {
        LOG_ALWAYS("HTTP request failed with code: %d", httpCode);
        http.end();
        return nullptr;
    }

    JsonDocument* doc = new JsonDocument;
    
    DeserializationError err = deserializeJson(*doc, http.getStream(), DeserializationOption::Filter(filter));
    http.end();

    if (err) {
        LOG_ALWAYS("Failed to parse JSON: %s", err.c_str());
        delete doc;
        return nullptr;
    }
    return doc;
}
  
// IPAPI get location data
ApiData fetchIpApiData() {
    ApiData result;
    JsonDocument filter;
    filter["latitude"] = true;
    filter["longitude"] = true;
    filter["utc_offset"] = true;

    // Use HTTP instead of HTTPS - avoids SSL/TLS time sync issues on boot
    String url = "https://ipapi.co/json/";
    JsonDocument* doc = fetchJsonFromApi(url, filter);

    if (!doc) {
        result.error = true;
        return result;
    }

    // Parse UTC offset from ipapi.co format: "+HHMM" or "-HHMM" (e.g., "+0530" = UTC+5:30)
    // Convert to total seconds for timezone calculations
    String offsetStr = (*doc)["utc_offset"].as<String>();
    int offset = 0;
    if (offsetStr.length() >= 5) {
        int sign = (offsetStr.charAt(0) == '-') ? -1 : 1;
        int hours = offsetStr.substring(1, 3).toInt();    // Extract HH
        int minutes = offsetStr.substring(3, 5).toInt();  // Extract MM
        offset = sign * (hours * 3600 + minutes * 60);    // Convert to seconds
        LOG_DEBUG("Parsed UTC offset: %s -> %d seconds", offsetStr.c_str(), offset);
    } else {
        LOG_DEBUG("Warning: Could not parse UTC offset: %s", offsetStr.c_str());
    }

    result.strings[0] = (*doc)["latitude"].as<String>();
    result.strings[1] = (*doc)["longitude"].as<String>();
    result.ints[0] = offset;
    result.error = false;
    
    delete doc;
    return result;
}

/**
 * Calculate milliseconds until next sunrise
 * Uses simplified sunrise algorithm based on solar noon calculation.
 * This is not perfectly accurate but good enough for nightlight automation.
 * 
 * Algorithm:
 * 1. Get current time and date
 * 2. Calculate solar noon for given longitude
 * 3. Calculate sunrise offset based on latitude and day of year
 * 4. Determine if sunrise is today or tomorrow
 * 
 * lat: Latitude in decimal degrees (positive = North)
 * lon: Longitude in decimal degrees (positive = East)
 * tzOffset: Timezone offset from UTC in seconds
 * return Milliseconds until next sunrise, or 0 on error
 */
unsigned long calculateMillisToNextSunrise(String lat, String lon, int tzOffset) {
    // Validate inputs
    if (lat.length() == 0 || lon.length() == 0) {
        LOG_DEBUG("Missing lat/lon for sunrise calculation");
        return 0;
    }
    
    float latitude = lat.toFloat();
    float longitude = lon.toFloat();
    
    // Validate reasonable lat/lon ranges
    if (latitude < -90 || latitude > 90 || longitude < -180 || longitude > 180) {
        LOG_DEBUG("Invalid lat/lon values");
        return 0;
    }
    
    // Get current UTC time (Unix timestamp in seconds)
    time_t nowUTC;
    time(&nowUTC);
    
    // Convert to local time
    time_t nowLocal = nowUTC + tzOffset;
    struct tm *timeinfo = gmtime(&nowLocal);
    
    int dayOfYear = timeinfo->tm_yday + 1;  // Day of year (1-365)
    int hour = timeinfo->tm_hour;
    int minute = timeinfo->tm_min;
    int second = timeinfo->tm_sec;
    
    LOG_DEBUG("Current local time: %02d:%02d:%02d, day %d", hour, minute, second, dayOfYear);
    
    // Calculate solar declination (simplified)
    float declinationAngle = 23.45 * sin(2.0 * PI * (284.0 + dayOfYear) / 365.0);
    float declinationRad = declinationAngle * PI / 180.0;
    float latitudeRad = latitude * PI / 180.0;
    
    // Calculate hour angle for sunrise
    float cosHourAngle = -tan(latitudeRad) * tan(declinationRad);
    
    // Check if sun rises today (polar regions may have no sunrise/sunset)
    if (cosHourAngle < -1.0 || cosHourAngle > 1.0) {
        LOG_DEBUG("No sunrise today (polar region?)");
        return 0;  // Sun doesn't rise or set today
    }
    
    float hourAngle = acos(cosHourAngle) * 180.0 / PI;
    
    // Calculate sunrise time in UTC hours
    // Solar noon occurs at 12:00 UTC + longitude offset
    float utcNoon = 12.0 - (longitude / 15.0);
    float sunriseUTC = utcNoon - (hourAngle / 15.0);
    
    // Convert to local time by adding timezone offset in hours
    float sunriseLocal = sunriseUTC + (tzOffset / 3600.0);
    
    // Normalize to 0-24 range
    while (sunriseLocal < 0) sunriseLocal += 24.0;
    while (sunriseLocal >= 24) sunriseLocal -= 24.0;
    
    // Convert to hours and minutes
    int sunriseHourInt = (int)sunriseLocal;
    int sunriseMinInt = (int)((sunriseLocal - sunriseHourInt) * 60.0);
    
    // Calculate seconds until sunrise today
    int currentSeconds = hour * 3600 + minute * 60 + second;
    int sunriseSeconds = sunriseHourInt * 3600 + sunriseMinInt * 60;
    int secondsUntil = sunriseSeconds - currentSeconds;
    
    // If sunrise already passed today, calculate for tomorrow
    if (secondsUntil < 0) {
        secondsUntil += 86400;  // Add 24 hours
    }
    
    LOG_DEBUG("Next sunrise in %d hours, %d minutes (at %02d:%02d local time)",
              secondsUntil / 3600, (secondsUntil % 3600) / 60, 
              sunriseHourInt, sunriseMinInt);
    
    // Convert to milliseconds
    return (unsigned long)secondsUntil * 1000UL;
}
