/*
================================================================================
  Screen Functions - GC9A01A Round LCD Display Header
================================================================================
  
  This module manages the GC9A01A round LCD display for visual feedback.
  The display shows IR tracking trails, spell names, images, and setup status.
  
  Hardware:
    - GC9A01A round LCD (240x240 pixels, 1.28" diameter)
    - SPI interface
    - Backlight control via GPIO
  
  Pin Configuration:
    - TFT_CS:   GPIO 10 (SPI Chip Select)
    - TFT_DC:   GPIO 9  (Data/Command select)
    - TFT_RST:  GPIO 8  (Reset)
    - TFT_MOSI: GPIO 11 (SPI Data Out)
    - TFT_SCLK: GPIO 12 (SPI Clock)
    - TFT_BL:   GPIO 13 (Backlight control)  
================================================================================
*/

#ifndef SCREEN_FUNCTIONS_H
#define SCREEN_FUNCTIONS_H

#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <SPI.h>
#include <vector>

// Forward declaration from spell_patterns.h
struct Point;

//=====================================
// Hardware Pin Definitions
//=====================================

/// SPI Chip Select pin
#define TFT_CS    10

/// Data/Command select pin
#define TFT_DC    9

/// Reset pin (active low)
#define TFT_RST   8

/// SPI MOSI (Master Out Slave In) data pin
#define TFT_MOSI  11

/// SPI SCLK (clock) pin
#define TFT_SCLK  12

/// Backlight control pin (PWM capable)
#define TFT_BL    13

//=====================================
// Global Display Object
//=====================================

/// Adafruit GC9A01A display object (defined in screenFunctions.cpp)
extern Adafruit_GC9A01A tft;

//=====================================
// Display Functions
//=====================================

/**
 * Initialize GC9A01A display and backlight
 * Configures SPI, initializes display controller, and sets up backlight PWM.
 * Clears screen to black after initialization.
 * Must be called during setup() before using display.
 */
void screenInit();

/**
 * Initialize GC9A01A display and backlight
 * Displays the wand position as a green trail on the screen.
 * Automatically scales camera coordinates (1024x768) to display (240x240).
 * x: IR X coordinate (0-1023, or -1 to clear)
 * y: IR Y coordinate (0-767, or -1 to clear)
 * isActive: true to draw green trail, false to clear
 */
void drawIRPoint(int x, int y, bool isActive = true);

/**
 * Clear IR trail state
 * Resets the last IR position to prevent trail lines from old positions
 */
void clearIRTrail();

/**
 * Display spell name and load associated image from SD card
 * Shows spell name text and attempts to load matching .bmp image file.
 * Image filename format: "<spellname>.bmp" (e.g., "ignite.bmp")
 * Falls back to text-only if image not found.
 * spellName: Name of detected spell
 */
void displaySpellName(const char* spellName);

/**
 * Clear entire display to black
 * Fills screen with black pixels, removing all trails, text, and images.
 */
void clearDisplay();

/**
 * Turn off display backlight
 * Sets backlight PWM to 0, effectively turning off screen visibility.
 * Display controller remains active.
 */
void backlightOff();

/**
 * Turn on display backlight
 * Sets backlight PWM to full brightness.
 */
void backlightOn();

/**
 * Update setup progress display
 * Shows initialization step with status indicator during boot.
 */
void updateSetupDisplay(int line, const String &function, const String &status);

//=====================================
// Image Display Functions
//=====================================

/**
 * Display .bmp image file from SD card
 * Loads and displays a raw RGB565 image file in .bmp format.
 * filename: Path to .bmp image file on SD card
 * x: X offset for image display (default 0)
 * y: Y offset for image display (default 0)
 * return true if image loaded successfully, false on error
 */
bool displayImageFromSD(const char* filename, int16_t x = 0, int16_t y = 0);

//=====================================
// Pattern Visualization (Debug)
//=====================================

/**
 * Visualize spell pattern on display
 * Used for debugging pattern definitions at startup.
 * Only active when SHOW_PATTERNS_ON_STARTUP is defined.
 * name: Spell name to display
 * pattern: Vector of points defining the spell pattern
 */
void visualizeSpellPattern(const char* name, const std::vector<Point>& pattern);

/**
 * Visualize spell match comparison (debug)
 * Overlays the matched spell pattern and user trajectory for debugging.
 * Only active when SHOW_PATTERNS_ON_STARTUP is defined.
 * name: Spell name
 * spellPattern: The matched spell pattern (resampled)
 * userTrajectory: The user's drawn trajectory (resampled)
 * similarity: The calculated similarity score (0.0-1.0)
 */
void visualizeMatchComparison(const char* name, const std::vector<Point>& spellPattern, const std::vector<Point>& userTrajectory, float similarity);

//=====================================
// Settings Menu Display
//=====================================

/**
 * Display settings menu on screen
 * Shows the current setting name and value, with visual indicators
 * for navigation state (browsing vs editing).
 * settingIndex: Index of the current setting (0 = Nightlight ON spell, 1 = Nightlight OFF spell)
 * valueIndex: Index of the current value option for the setting
 * isEditing: True if currently editing the value, false if browsing settings
 */
void displaySettingsMenu(int settingIndex, int valueIndex, bool isEditing);

/**
 * Display a centered error message on screen
 * Shows the message in red with two-line support (split at space)
 * message: Error message to display
 */
void displayError(const char* message);

/**
 * Display a centered message on screen
 * Shows the message in the specified color
 * message: Message to display
 * color: RGB565 color for the text
 */
void displayMessage(const char* message, uint16_t color);

/**
 * Show ready-state background
 * Fills the screen with a green background and preserves border circle.
 * Used to indicate READY state (user should hold still to begin tracking).
 */
void showReadyBackground();

/**
 * Restore idle background
 * Returns the display to default idle appearance (black with border circle).
 */
void restoreIdleBackground();

#endif // SCREEN_FUNCTIONS_H