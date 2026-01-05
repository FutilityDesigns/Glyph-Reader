#ifndef WEBFUNCTIONS_H
#define WEBFUNCTIONS_H

#include <Arduino.h>
#include <WiFiManager.h>

extern WiFiManager wm;

bool initWM();

// Generate HTML for a numeric adjuster with +/- buttons
//String generateAdjusterHTML(const char* settingName, int startValue, int stepValue);

#endif