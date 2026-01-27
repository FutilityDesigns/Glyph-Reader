/*
================================================================================
  Spell Patterns - Gesture Pattern Definitions Implementation
================================================================================
  
  This module defines the library of built-in spell gesture patterns.
  Each spell is represented as a sequence of (x, y) points that trace the
  ideal path for that gesture.
  
  Pattern Design Philosophy:
    - Patterns defined in arbitrary coordinate space (normalized during init)
    - Clock-face metaphor for circular patterns (12=top, 3=right, 6=bottom, 9=left)
    - Shapes chosen to be distinctive and easy to draw

  Built-in Spells (14 total):
    1. Unlock:     Clockwise circle + line down (key in lock motion)
    2. Terminate:  Z-shape (aggressive cutting motion)
    3. Ignite:     Triangle (fire symbol)
    4. Gust:       V-shape (wind direction)
    5. Lower:      Arc 12→7 clockwise + line down (lowering motion)
    6. Raise:      Arc 6→10 counter-clockwise + line up (raising motion)
    7. Levitate:   Figure-8 / infinity symbol (continuous flow)
    8. Silence:    Horizontal line (silencing gesture)
    9. Arresto:    Vertical line up (stop/arrest motion)
    10. Halt:      Vertical line down (halt command)
    11. Move:      Horizontal line (move/push)
    12. Resume:    W-shape (resume/continue pattern)
    13. Illuminate: Star shape (light/illumination)
    14. Dark:      Diagonal slash top-right to bottom-left (darkness/shadow)
  
  Initialization Process:
    1. Define each spell pattern with raw coordinates
    2. Normalize all patterns to 0-1000 space (scale/translation invariant)
    3. Resample all patterns to exactly 40 evenly-spaced points
    4. Store in global spellPatterns vector
  
  Customization:
    - Patterns can be modified/added/replaced via spells.json on SD card
    - See applyCustomSpells() function (implemented in sdFunctions.cpp)
  
================================================================================
*/

#include "spell_patterns.h"
#include "spell_matching.h"
#include "sdFunctions.h"
#include <Arduino.h>
#include <cmath>

//=====================================
// Forward Declarations
//=====================================
// From spell_matching.cpp - avoids circular dependency
std::vector<Point> normalizeTrajectory(const std::vector<Point>& traj);
std::vector<Point> resampleTrajectory(const std::vector<Point>& traj, int numPoints);

//=====================================
// Global Pattern Storage
//=====================================
/// Global vector of all available spell patterns (built-in + custom from SD)
std::vector<SpellPattern> spellPatterns;

//=====================================
// Pattern Initialization
//=====================================

/**
 * Initialize all built-in spell patterns
 * Creates the library of recognizable gestures by defining each spell's
 * point sequence, then normalizing and resampling for consistent matching.
 * Process for each pattern:
 *   1. Define raw coordinate points (arbitrary scale/position)
 *   2. Add to spellPatterns vector
 *   3. Normalize to 0-1000 space (scale/translation invariant)
 *   4. Resample to exactly 40 points (length invariant)
 * Pattern coordinates use a 1024x768 reference space (camera resolution)
 * but exact values don't matter since normalization rescales everything.
 * Note: Must be called during setup() before WiFiManager (patterns used in web portal)
 */
void initSpellPatterns() {
  //-----------------------------------
  // Pattern 1: Unlock
  //-----------------------------------
  // Clockwise circle starting at top + line down through center
  SpellPattern unlock;
  unlock.name = "Unlock";
  
  // Create circular pattern starting at top (12 o'clock), going clockwise
  float centerX = 512, centerY = 384;  // Center of 1024x768 space
  float radius = 200;
  
  // Circle: 9 points to complete full rotation (8 segments + return to start)
  for (int i = 0; i <= 8; i++) {
    float angle = (i * M_PI * 2) / 8 - M_PI / 2;  // Start at top (-90°), go clockwise
    Point p;
    p.x = centerX + radius * cos(angle);
    p.y = centerY + radius * sin(angle);
    p.timestamp = i * 100;  // Sequential timestamps (not used in matching)
    unlock.pattern.push_back(p);
  }
  
  // Add line down through center and extend below
  Point lineTop = {(int)(centerX), (int)(centerY - radius), 900};
  unlock.pattern.push_back(lineTop);
  Point lineCenter = {(int)centerX, (int)centerY, 1000};
  unlock.pattern.push_back(lineCenter);
  Point lineBottom = {(int)centerX, (int)(centerY + radius), 1100};
  unlock.pattern.push_back(lineBottom);
  Point lineEnd = {(int)centerX, (int)(centerY + radius + 100), 1200};
  unlock.pattern.push_back(lineEnd);
  spellPatterns.push_back(unlock);
  
  //-----------------------------------
  // Pattern 2: Terminate
  //-----------------------------------
  // Z-shape with extended tail 
  SpellPattern terminate;
  terminate.name = "Terminate";
  terminate.pattern = {
    {200, 200},     // Top left
    {512, 200},     // Mid-point of top horizontal
    {824, 200},     // Top right
    {612, 342},     // Mid-point of diagonal
    {400, 484},     // Mid-point of diagonal
    {200, 584},     // Bottom left
    {512, 602},     // Mid-point of bottom horizontal
    {824, 620}      // Bottom right (extended tail)
  };
  spellPatterns.push_back(terminate);
  
  //-----------------------------------
  // Pattern 3: Ignite
  //-----------------------------------
  // Triangle shape 
  SpellPattern ignite;
  ignite.name = "Ignite";
  ignite.pattern = {
    {200, 600},      // Lower left
    {356, 400},      // Mid-point going up left side
    {512, 200},      // Top (apex)
    {668, 400},      // Mid-point going down right side
    {824, 600},      // Lower right
    {512, 600},      // Mid-point of base
    {200, 600}       // Back to start (complete triangle)
  };
  spellPatterns.push_back(ignite);
  
  //-----------------------------------
  // Pattern 4: Gust
  //-----------------------------------
  // V-shape 
  SpellPattern gust;
  gust.name = "Gust";
  gust.pattern = {
    {200, 200},      // Upper left
    {356, 350},      // Mid-point going down left
    {512, 500},      // Bottom middle (V point)
    {668, 350},      // Mid-point going up right
    {824, 200}       // Upper right
  };
  spellPatterns.push_back(gust);
  
  //-----------------------------------
  // Pattern 5: Lower
  //-----------------------------------
  // Large arc from 12 o'clock clockwise to 7 o'clock + line down 
  SpellPattern lower;
  lower.name = "Lower";
  centerX = 400; centerY = 400; radius = 200;
  // Start at 12 o'clock and go clockwise (right and down) around to 7 o'clock
  lower.pattern.push_back({(int)(centerX + radius * cos(90 * M_PI / 180)), (int)(centerY + radius * sin(90 * M_PI / 180)), 0});      // 12 o'clock
  lower.pattern.push_back({(int)(centerX + radius * cos(60 * M_PI / 180)), (int)(centerY + radius * sin(60 * M_PI / 180)), 100});    // 1 o'clock
  lower.pattern.push_back({(int)(centerX + radius * cos(30 * M_PI / 180)), (int)(centerY + radius * sin(30 * M_PI / 180)), 200});    // 2 o'clock
  lower.pattern.push_back({(int)(centerX + radius * cos(0 * M_PI / 180)), (int)(centerY + radius * sin(0 * M_PI / 180)), 300});      // 3 o'clock
  lower.pattern.push_back({(int)(centerX + radius * cos(330 * M_PI / 180)), (int)(centerY + radius * sin(330 * M_PI / 180)), 400});  // 4 o'clock
  lower.pattern.push_back({(int)(centerX + radius * cos(300 * M_PI / 180)), (int)(centerY + radius * sin(300 * M_PI / 180)), 500});  // 5 o'clock
  lower.pattern.push_back({(int)(centerX + radius * cos(270 * M_PI / 180)), (int)(centerY + radius * sin(270 * M_PI / 180)), 600});  // 6 o'clock
  lower.pattern.push_back({(int)(centerX + radius * cos(240 * M_PI / 180)), (int)(centerY + radius * sin(240 * M_PI / 180)), 700});  // 7 o'clock

  // Line straight down from 10 o'clock position
  int endX = (int)(centerX + radius * cos(150 * M_PI / 180));
  int endY = (int)(centerY + radius * sin(150 * M_PI / 180));
  lower.pattern.push_back({endX, endY + 150, 900});
  lower.pattern.push_back({endX, endY + 300, 1000});
  spellPatterns.push_back(lower);

  //-----------------------------------
  // Pattern 7: Raise
  //-----------------------------------
  // Large arc from 12 o'clock clockwise to 7 o'clock, then line up
  SpellPattern raise;
  raise.name = "Raise";
  centerX = 400; centerY = 400; radius = 200;
  // Start at 6 o'clock and go counter-clockwise (right and up) around to 10 o'clock
  // Using standard trig: 12=90°, 3=0°, 6=270°, 9=180°
  raise.pattern.push_back({(int)(centerX + radius * cos(270 * M_PI / 180)), (int)(centerY + radius * sin(270 * M_PI / 180)), 0});     // 6 o'clock
  raise.pattern.push_back({(int)(centerX + radius * cos(300 * M_PI / 180)), (int)(centerY + radius * sin(300 * M_PI / 180)), 100});   // 5 o'clock
  raise.pattern.push_back({(int)(centerX + radius * cos(330 * M_PI / 180)), (int)(centerY + radius * sin(330 * M_PI / 180)), 200});   // 4 o'clock
  raise.pattern.push_back({(int)(centerX + radius * cos(0 * M_PI / 180)), (int)(centerY + radius * sin(0 * M_PI / 180)), 300});       // 3 o'clock
  raise.pattern.push_back({(int)(centerX + radius * cos(30 * M_PI / 180)), (int)(centerY + radius * sin(30 * M_PI / 180)), 400});     // 2 o'clock
  raise.pattern.push_back({(int)(centerX + radius * cos(60 * M_PI / 180)), (int)(centerY + radius * sin(60 * M_PI / 180)), 500});     // 1 o'clock
  raise.pattern.push_back({(int)(centerX + radius * cos(90 * M_PI / 180)), (int)(centerY + radius * sin(90 * M_PI / 180)), 600});     // 12 o'clock
  raise.pattern.push_back({(int)(centerX + radius * cos(120 * M_PI / 180)), (int)(centerY + radius * sin(120 * M_PI / 180)), 700});   // 11 o'clock
  raise.pattern.push_back({(int)(centerX + radius * cos(150 * M_PI / 180)), (int)(centerY + radius * sin(150 * M_PI / 180)), 800});   // 10 o'clock

  // Line straight up from 7 o'clock position
  int raiseEndX = (int)(centerX + radius * cos(240 * M_PI / 180));
  int raiseEndY = (int)(centerY + radius * sin(240 * M_PI / 180));
  raise.pattern.push_back({raiseEndX, raiseEndY - 150, 800});
  raise.pattern.push_back({raiseEndX, raiseEndY - 300, 900});
  spellPatterns.push_back(raise);
  
  //-----------------------------------  
  // Pattern 8: Move 
  //-----------------------------------
  // "4" shape (vertical up, diagonal down-left, horizontal right)
  SpellPattern move;
  move.name = "Move";
  move.pattern = {
    {650, 600},      // Bottom of vertical line
    {650, 400},      // Mid-point going up
    {650, 200},      // Top of vertical line
    {425, 300},      // Mid-point of diagonal
    {200, 400},      // End of diagonal (down and left)
    {512, 400},      // Mid-point of horizontal
    {824, 400}       // End of horizontal (right)
  };
  spellPatterns.push_back(move);
  
  //-----------------------------------
  // Pattern 9: Levitate 
  //-----------------------------------
  // Half circle 9→3 counter-clockwise + line down
  SpellPattern levitate;
  levitate.name = "Levitate";
  centerX = 512; centerY = 300; radius = 200;
  // Half circle from 9 o'clock (180°) to 3 o'clock (0°) counter-clockwise
  for (int i = 0; i <= 6; i++) {
    float angle = (180 - i * 30) * M_PI / 180;  // 9 o'clock counter-clockwise to 3
    Point p;
    p.x = centerX + radius * cos(angle);
    p.y = centerY + radius * sin(angle);
    p.timestamp = i * 100;
    levitate.pattern.push_back(p);
  }
  // Add line down
  Point levitateLineEnd = {(int)(centerX + radius), (int)(centerY + radius + 150), 700};
  levitate.pattern.push_back(levitateLineEnd);
  spellPatterns.push_back(levitate);
  
  //-----------------------------------
  // Pattern 10: Silence 
  //-----------------------------------
  // Half circle 3→9 clockwise + line down
  SpellPattern silence;
  silence.name = "Silence";
  centerX = 512; centerY = 300; radius = 200;
  // Half circle from 3 o'clock (0°) to 9 o'clock (180°) clockwise
  for (int i = 0; i <= 6; i++) {
    float angle = (i * 30) * M_PI / 180;  // 3 o'clock clockwise to 9
    Point p;
    p.x = centerX + radius * cos(angle);
    p.y = centerY + radius * sin(angle);
    p.timestamp = i * 100;
    silence.pattern.push_back(p);
  }
  // Add line down
  Point silenceLineEnd = {(int)(centerX - radius), (int)(centerY + radius + 150), 700};
  silence.pattern.push_back(silenceLineEnd);
  spellPatterns.push_back(silence);
  
  //-----------------------------------
  // Pattern 11: Halt
  //-----------------------------------
  //Capital letter M
  SpellPattern halt;
  halt.name = "Halt";
  halt.pattern = {
    {200, 600},      // Bottom left
    {275, 400},      // Mid-point going up first leg
    {350, 200},      // Up to first peak
    {431, 325},      // Mid-point going down to valley
    {512, 450},      // Down to middle valley
    {593, 325},      // Mid-point going up to second peak
    {674, 200},      // Up to second peak
    {749, 400},      // Mid-point going down second leg
    {824, 600}       // Down to bottom right
  };
  spellPatterns.push_back(halt);

  //-----------------------------------
  // Pattern 12: Resume 
  //-----------------------------------
  // Capital letter W
  SpellPattern resume;
  resume.name = "Resume";
  resume.pattern = {
    {200, 200},      // Top left
    {275, 400},      // Mid-point going down first leg
    {350, 600},      // Down to first valley
    {431, 475},      // Mid-point going up to middle peak
    {512, 350},      // Up to middle peak
    {593, 475},      // Mid-point going down to second valley
    {674, 600},      // Down to second valley
    {749, 400},      // Mid-point going up second leg
    {824, 200}       // Up to top right
  };
  spellPatterns.push_back(resume);

  //-----------------------------------
  // Pattern 13: Illuminate
  //-----------------------------------
  // Star shape
  SpellPattern illuminate;
  illuminate.name = "Illuminate";
  illuminate.pattern = {
    {320, 775},     // Bottom Left
    {512, 186},     // Top
    {703, 775},     // Bottom Right
    {202, 441},     // Mid Left
    {821, 441},     // Mid Right
    {320, 775}      // Back to Bottom Left
  };
  spellPatterns.push_back(illuminate);

  //-----------------------------------
  // Pattern 14: Dark
  //-----------------------------------
  // X Shape with left side connected, from top-right to bottom-left, up to top-left, then down to bottom-right
  SpellPattern dark;
  dark.name = "Dark";
  dark.pattern = {
    {824, 200},     // Top Right
    {488, 484},     // Mid-point
    {152, 768},      // Bottom Left
    {152, 484},     // Mid-point going up
    {152, 200},     // Top Left
    {488, 484},     // Mid-point going down
    {824, 768}      // Bottom Right
  };
  spellPatterns.push_back(dark);
  
  // Normalize and resample all patterns to 50 points for consistent matching
  // This allows patterns to be defined with a small number of key points,
  // then extrapolated to match the resolution used for recorded gestures
  Serial.println("Resampling spell patterns to 50 points...");
  for (auto& spell : spellPatterns) {
    std::vector<Point> normalized = normalizeTrajectory(spell.pattern);
    spell.pattern = resampleTrajectory(normalized, RESAMPLE_POINTS);
  }
  
  Serial.printf("Loaded and resampled %d spell patterns\n", spellPatterns.size());
}

// Visualize spell patterns on screen (forward declaration from screenFunctions.h)
void visualizeSpellPattern(const char* name, const std::vector<Point>& pattern);

// Show all spell patterns on screen for debugging
void showSpellPatterns() {
  Serial.println("Visualizing spell patterns...");
  for (const auto& spell : spellPatterns) {
    visualizeSpellPattern(spell.name, spell.pattern);
  }
  Serial.println("Pattern visualization complete");
}

// Apply custom spell configurations from SD card
// This should be called after initSpellPatterns() and after SD card is initialized
void applyCustomSpells() {
  loadCustomSpells();
}
