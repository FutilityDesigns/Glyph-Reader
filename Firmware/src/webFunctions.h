/*
================================================================================
  Web Functions - WiFiManager Configuration Portal Header
================================================================================
  
  This module manages the WiFiManager web portal for device configuration.
  Provides a captive portal for WiFi setup and custom parameter configuration.
  
  WiFiManager Features:
    - Auto-connect to saved WiFi credentials
    - Fallback AP mode if connection fails (SSID: "GlyphReader-Setup")
    - Captive portal for easy configuration (http://192.168.4.1)
    - Custom parameter fields for device-specific settings
  
  Configuration Portal:
    - WiFi SSID/password selection
    - MQTT broker settings (host, port, topic)
    - Gesture tuning parameters (movement thresholds, timeouts)
    - Nightlight spell selection (dropdowns populated from spell patterns)
    - Auto-reboot after save for immediate effect
  
  Custom HTML Parameters:
    - MQTT configuration fields (text inputs)
    - Tuning parameter adjusters (+/- buttons for easy tuning)
    - Nightlight spell dropdowns (generated from spellPatterns vector)
  
  Portal Access:
    - Station mode: http://glyphreader.local (via mDNS)
    - AP mode: http://192.168.4.1 (captive portal)
  
  Implementation Notes:
    - Custom parameters must be initialized before wm.process()
    - Parameters are populated in generateAdjusters() function
    - Spell dropdowns require initSpellPatterns() to be called first
    - Save callback triggers ESP.restart() for immediate effect
  
================================================================================
*/

#ifndef WEBFUNCTIONS_H
#define WEBFUNCTIONS_H

#include <Arduino.h>
#include <WiFiManager.h>

//=====================================
// Global WiFiManager Object
//=====================================

/// WiFiManager instance for configuration portal (defined in webFunctions.cpp)
extern WiFiManager wm;

//=====================================
// WiFiManager Functions
//=====================================

/**
 * Initialize WiFiManager and configure web portal
 * Portal URLs:
 *   - Station mode: http://glyphreader.local
 *   - AP mode: http://192.168.4.1
 * return true if connected to WiFi, false if in AP mode
 * note: Must be called AFTER initSpellPatterns() (spell dropdowns require patterns)
 */
bool initWM();

#endif // WEBFUNCTIONS_H