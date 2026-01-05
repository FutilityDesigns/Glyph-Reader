/*
Spell Pattern Definitions
Defines known spell gesture patterns for matching.
*/

#include "spell_patterns.h"
#include "sdFunctions.h"
#include <Arduino.h>
#include <cmath>

// Global spell patterns vector
std::vector<SpellPattern> spellPatterns;

// Initialize spell patterns
void initSpellPatterns() {
  // Pattern 1: Horizontal swish (left to right)
  // SpellPattern swish;
  // swish.name = "Swish";
  // swish.pattern = {
  //   {0, 500, 0},
  //   {256, 500, 100},
  //   {512, 500, 200},
  //   {768, 500, 300},
  //   {1024, 500, 400}
  // };
  // spellPatterns.push_back(swish);
  
  // Pattern 2: Unlock - Clockwise circle starting at top, then line down through center
  SpellPattern unlock;
  unlock.name = "Unlock";
  // Create circular pattern starting at top (12 o'clock), going clockwise
  float centerX = 512, centerY = 384;
  float radius = 200;
  // Circle: start at top and go clockwise (9 points to complete circle back to top)
  for (int i = 0; i <= 8; i++) {
    float angle = (i * M_PI * 2) / 8 - M_PI / 2;  // Start at top (-90 degrees), go clockwise
    Point p;
    p.x = centerX + radius * cos(angle);
    p.y = centerY + radius * sin(angle);
    p.timestamp = i * 100;
    unlock.pattern.push_back(p);
  }
  // Add line down through center and out bottom, 4 more points
  Point lineTop = {(int)(centerX), (int)(centerY - radius), 900};
  unlock.pattern.push_back(lineTop);
  Point lineCenter = {(int)centerX, (int)centerY, 1000};
  unlock.pattern.push_back(lineCenter);
  Point lineBottom = {(int)centerX, (int)(centerY + radius), 1100};
  unlock.pattern.push_back(lineBottom);
  Point lineEnd = {(int)centerX, (int)(centerY + radius + 100), 1200};
  unlock.pattern.push_back(lineEnd);
  spellPatterns.push_back(unlock);
  
  // Pattern 3: Terminate - Z shape with dropped tail
  SpellPattern terminate;
  terminate.name = "Terminate";
  terminate.pattern = {
    {200, 200},
    {512, 200},    // Mid-point of top horizontal
    {824, 200},
    {612, 342},    // Mid-point of diagonal
    {400, 484},    // Mid-point of diagonal
    {200, 584},
    {512, 602},    // Mid-point of bottom horizontal
    {824, 620}
  };
  spellPatterns.push_back(terminate);
  
  // Pattern 4: Ignite - Triangle (lower left, up to middle, down to right, back to start)
  SpellPattern ignite;
  ignite.name = "Ignite";
  ignite.pattern = {
    {200, 600},      // Lower left
    {356, 400},      // Mid-point going up left side
    {512, 200},      // Top
    {668, 400},      // Mid-point going down right side
    {824, 600},      // Lower right
    {512, 600},      // Mid-point of base
    {200, 600}       // Back to start (lower left)
  };
  spellPatterns.push_back(ignite);
  
  // Pattern 5: Gust - V shape (upper left, down to middle, up to right)
  SpellPattern gust;
  gust.name = "Gust";
  gust.pattern = {
    {200, 200},      // Upper left
    {356, 350},      // Mid-point going down left
    {512, 500},      // Bottom middle
    {668, 350},      // Mid-point going up right
    {824, 200}       // Upper right
  };
  spellPatterns.push_back(gust);
  
  // Pattern 6: Lower - "p" shape (partial circle from 6 o'clock to 7 o'clock + line down)
  SpellPattern lower;
  lower.name = "Lower";
  centerX = 512; centerY = 384; radius = 200;
  // Start at 6 o'clock (90 degrees), go counter-clockwise to 7 o'clock (210 degrees)
  // That's a 120 degree arc (1/3 circle)
  for (int i = 0; i <= 6; i++) {
    float angle = (90 + i * 20) * M_PI / 180;  // 6 o'clock to 7 o'clock, 120° arc
    Point p;
    p.x = centerX + radius * cos(angle);
    p.y = centerY + radius * sin(angle);
    p.timestamp = i * 100;
    lower.pattern.push_back(p);
  }
  // Add line down from end of circle (7 o'clock position)
  Point lowerLineStart = {(int)(centerX - radius * 0.866), (int)(centerY + radius * 0.5), 700};
  lower.pattern.push_back(lowerLineStart);
  Point lowerLineEnd = {(int)(centerX - radius * 0.866), (int)(centerY + radius + 150), 800};
  lower.pattern.push_back(lowerLineEnd);
  spellPatterns.push_back(lower);
  
  // Pattern 7: Raise - "b" shape (partial circle from 12 o'clock to 10 o'clock + line up)
  SpellPattern raise;
  raise.name = "Raise";
  centerX = 512; centerY = 384; radius = 200;
  // Start at 12 o'clock (-90 degrees), go clockwise to 10 o'clock (-60 degrees)
  // That's a 30 degree arc (1/12 circle)
  for (int i = 0; i <= 6; i++) {
    float angle = (-90 - i * 5) * M_PI / 180;  // 12 o'clock to 10 o'clock, 30° arc clockwise
    Point p;
    p.x = centerX + radius * cos(angle);
    p.y = centerY + radius * sin(angle);
    p.timestamp = i * 100;
    raise.pattern.push_back(p);
  }
  // Add line up from end of circle (10 o'clock position)
  Point raiseLineStart = {(int)(centerX - radius * 0.5), (int)(centerY - radius * 0.866), 700};
  raise.pattern.push_back(raiseLineStart);
  Point raiseLineEnd = {(int)(centerX - radius * 0.5), (int)(centerY - radius - 150), 800};
  raise.pattern.push_back(raiseLineEnd);
  spellPatterns.push_back(raise);
  
  // Pattern 8: Move - "4" shape (vertical up, diagonal down-left, horizontal right)
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
  
  // Pattern 9: Levitate - Half circle 9→3 counter-clockwise + line down
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
  
  // Pattern 10: Silence - Half circle 3→9 clockwise + line down
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
  
  // Pattern 11: Halt - Capital letter M
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
  
  Serial.printf("Loaded %d spell patterns\n", spellPatterns.size());
}

// Apply custom spell configurations from SD card
// This should be called after initSpellPatterns() and after SD card is initialized
void applyCustomSpells() {
  loadCustomSpells();
}
