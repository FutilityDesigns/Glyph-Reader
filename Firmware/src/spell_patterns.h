/*
================================================================================
  Spell Patterns - Gesture Pattern Definitions Header
================================================================================
  
  This module defines the library of recognizable gesture patterns (spells).
  Each spell is represented as a sequence of (x, y) points that define the
  ideal trajectory for that gesture.
  
  Pattern Definition:
    - Patterns are defined as arrays of {x, y, timestamp} points
    - Coordinates are in arbitrary space (normalized during initialization)
    - Patterns are resampled to exactly 40 points during initialization
  
  Data Structures:
    - Point: Single (x, y, timestamp) coordinate
    - SpellPattern: Named pattern with point sequence and optional custom image
    - spellPatterns: Global vector of all available spells
  
  Customization:
    - Patterns can be modified/added/replaced via spells.json on SD card
    - Custom image files (.bmp format) can be specified per spell
  
================================================================================
*/

#ifndef SPELL_PATTERNS_H
#define SPELL_PATTERNS_H

#include <Arduino.h>
#include <vector>

//=====================================
// Data Structures
//=====================================

/**
 * Single point in a trajectory or pattern
 * Represents one (x, y) coordinate with timestamp.
 * Used for both pattern definitions and recorded gestures.
 */
struct Point {
  int x;           ///< X coordinate (camera: 0-1023, normalized: 0-1000)
  int y;           ///< Y coordinate (camera: 0-767, normalized: 0-1000)
  uint32_t timestamp;  ///< Timestamp in milliseconds (unused in patterns)
};

/**
 * Spell pattern definition
 * Contains all information needed to recognize and display a spell.
 */
struct SpellPattern {
  const char* name;                   ///< Spell name (e.g., "Ignite")
  std::vector<Point> pattern;         ///< Sequence of points defining gesture
  String customImageFilename;         ///< Optional custom image filename (empty = use default naming)
};

//=====================================
// Global Pattern Storage
//=====================================

/**
 * Global vector of all available spell patterns
 * Populated by initSpellPatterns() with built-in spells, then optionally
 * modified by applyCustomSpells() from SD card configuration.
 * Patterns are normalized and resampled to 40 points during initialization.
 */
extern std::vector<SpellPattern> spellPatterns;

//=====================================
// Initialization Functions
//=====================================

/**
 * Initialize all spell patterns
 * Loads built-in spell patterns and processes them:
 *   1. Defines all 14 built-in spell patterns
 *   2. Normalizes each pattern to 0-1000 coordinate space
 *   3. Resamples each pattern to exactly 40 points
 * Must be called during setup() before WiFiManager initialization
 * (patterns are needed for web portal dropdown generation).
 */
void initSpellPatterns();

/**
 * Display all spell patterns on screen for debugging
 * Animates each spell pattern on the display with color-coded points
 * Useful for verifying pattern definitions and visualizing how the system
 * "sees" each gesture after normalization and resampling.
 * Only compiled if SHOW_PATTERNS_ON_STARTUP is defined.
 */
void showSpellPatterns();

/**
 * Apply custom spell configurations from SD card
 * Loads spells.json from SD card and applies modifications:
 *   - "modify": Update existing spell patterns
 *   - "add": Add new custom spells
 *   - "replace": Replace entire spell library
 * Called during setup() if SD card is available.
 * See sdFunctions.cpp for JSON parsing implementation.
 */
void applyCustomSpells();

#endif // SPELL_PATTERNS_H
