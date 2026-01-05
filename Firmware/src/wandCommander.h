#ifndef WAND_COMMANDER_H
#define WAND_COMMANDER_H

//=====================================
// Hardware Definitions
//=====================================
#define BUTTON_1_PIN  11    // GPIO2 for Button 1
#define BUTTON_2_PIN  33    // GPIO3 for Button 2
#define I2C_SDA 6
#define I2C_SCL 5
//=====================================


// Universal includes
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>

// Logging definitions
#define LOG_ALWAYS(format, ...) Serial.printf(format "\n", ##__VA_ARGS__)

#ifdef ENV_DEV  // Dev logging
#define LOG_DEBUG(format, ...) Serial.printf(format "\n", ##__VA_ARGS__)
#endif

#ifdef ENV_PROD // Production logging, if debug on only
//#define LOG_DEBUG(format, ...)do { if (debug) Serial.printf("[DEBUG] " format "\n", ##__VA_ARGS__); } while(0)
#endif

extern unsigned long screenSpellOnTime;
extern unsigned long screenOnTime;
extern bool backlightStateOn;
extern unsigned long ledOnTime;


#endif