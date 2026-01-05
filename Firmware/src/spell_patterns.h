#ifndef SPELL_PATTERNS_H
#define SPELL_PATTERNS_H

#include <Arduino.h>
#include <vector>

// Point structure
struct Point {
  int x;
  int y;
  uint32_t timestamp;
};

// Spell pattern structure
struct SpellPattern {
  const char* name;
  std::vector<Point> pattern;
  String customImageFilename;  // Optional custom image filename (empty = use default naming)
};

// Global spell patterns vector
extern std::vector<SpellPattern> spellPatterns;

// Initialize all spell patterns
void initSpellPatterns();

// Apply custom spell configurations from SD card
void applyCustomSpells();

#endif
