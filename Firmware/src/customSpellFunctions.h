/*
================================================================================
  Custom Spell Functions - User-Created Spell Pattern Management
================================================================================
  
  This module handles the creation, storage, and management of user-created
  spell patterns. Users can record new gestures via the settings menu and
  save them to the SD card.
  
  Features:
    - Record new spell patterns via wand tracking
    - Save patterns to spells.json on SD card
    - Auto-generate names (Custom 1, Custom 2, etc.)
    - Preview patterns before saving
    - Rename custom spells via web portal
================================================================================
*/

#ifndef CUSTOM_SPELL_FUNCTIONS_H
#define CUSTOM_SPELL_FUNCTIONS_H

#include "spell_patterns.h"
#include <vector>

// State machine for custom spell recording
enum SpellRecordingState {
  SPELL_RECORD_IDLE,           // Not recording
  SPELL_RECORD_WAITING,        // Waiting for IR detection
  SPELL_RECORD_READY,          // IR detected, waiting for stillness
  SPELL_RECORD_TRACKING,       // Actively recording gesture
  SPELL_RECORD_PREVIEW,        // Showing preview, waiting for save/discard
  SPELL_RECORD_COMPLETE        // Recording complete, returning to settings
};

// Global state
extern SpellRecordingState spellRecordingState;
extern std::vector<Point> recordedSpellPattern;

/**
 * Enter spell recording mode
 * Checks for SD card, displays error if missing, or starts recording workflow
 */
void enterSpellRecordingMode();

/**
 * Exit spell recording mode
 * Returns to settings menu
 */
void exitSpellRecordingMode();

/**
 * Update spell recording state machine
 * Called from main loop to handle the recording process
 * Returns true if still recording, false if complete
 */
bool updateSpellRecording();

/**
 * Save recorded pattern to spells.json
 * Adds pattern to "custom" section with auto-generated name
 * Returns true on success, false on failure
 */
bool saveRecordedSpell();

/**
 * Get list of custom spell names for web portal
 * Returns vector of spell names that start with "Custom"
 */
std::vector<String> getCustomSpellNames();

/**
 * Rename a custom spell
 * Updates spells.json with new name
 * oldName: Current spell name (e.g., "Custom 1")
 * newName: New spell name
 * Returns true on success, false on failure
 */
bool renameCustomSpell(const char* oldName, const char* newName);

// Batch rename helper used to apply multiple renames in a single JSON write
struct SpellRenamePair {
  String oldName;
  String newName;
};
bool renameCustomSpellsBatch(const std::vector<SpellRenamePair>& renames);

#endif
