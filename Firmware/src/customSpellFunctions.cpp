/*
================================================================================
  Custom Spell Functions - User-Created Spell Pattern Management Implementation
================================================================================
  
  This module handles the creation, storage, and management of user-created
  spell patterns, allowing users to extend the built-in spell library with
  their own custom gestures.
  
  Workflow:
    1. User enters recording mode from settings menu
    2. System checks for SD card presence (required for storage)
    3. User draws gesture with wand (tracked by normal camera state machine)
    4. Gesture captured and stored in recordedSpellPattern vector
    5. User confirms save, pattern written to spells.json on SD card
    6. Auto-generated name assigned ("Custom 1", "Custom 2", etc.)
    7. Spell patterns reloaded to make new spell immediately available
  
  Storage Format (spells.json):
    {
      "custom": [
        {
          "name": "Custom 1",
          "pattern": [
            {"x": 100, "y": 200},
            {"x": 150, "y": 250},
            ...
          ]
        }
      ]
    }
  
  Features:
    - Unlimited custom spells (limited only by SD card space)
    - Auto-naming with sequential numbers
    - Manual rename support via spells.json editing
    - Patterns normalized/resampled like built-in spells
    - Immediate availability after save (no reboot required)
  
  State Machine:
    SPELL_RECORD_IDLE:      Not recording
    SPELL_RECORD_TRACKING:  Actively recording gesture via camera
    SPELL_RECORD_COMPLETE:  Recording finished, returning to menu
  
================================================================================
*/

#include "customSpellFunctions.h"
#include "glyphReader.h"
#include "sdFunctions.h"
#include "screenFunctions.h"
#include "led_control.h"
#include "spell_matching.h"
#include "spell_patterns.h"
#include "cameraFunctions.h"
#include "preferenceFunctions.h"
#include <ArduinoJson.h>

// Global state tracking for custom spell recording
SpellRecordingState spellRecordingState = SPELL_RECORD_IDLE;
std::vector<Point> recordedSpellPattern;  // Stores captured gesture points

/**
 * Enter spell recording mode
 * 
 * Initiates the custom spell recording workflow. Checks for SD card availability
 * (required for storage), then prepares the system to capture a new gesture.
 * The actual gesture tracking is handled by the normal camera state machine
 * (readCameraData in cameraFunctions.cpp) which detects the global flag
 * isRecordingCustomSpell and stores points in recordedSpellPattern.
 * 
 * Error Handling:
 *   - If no SD card detected: Displays error message and exits to COMPLETE state
 *   - Display will show "SD Card Required" for 2 seconds before returning
 * 
 * Display State:
 *   - Clears screen to provide clean canvas for gesture visualization
 *   - IR trail cleared to start fresh tracking display
 * 
 * Side Effects:
 *   - Sets global isRecordingCustomSpell = true (camera uses this flag)
 *   - Clears recordedSpellPattern vector for new capture
 *   - Changes spellRecordingState to SPELL_RECORD_TRACKING
 */
void enterSpellRecordingMode() {
  LOG_DEBUG("Entering spell recording mode");
  
  // Verify SD card is present - required for saving custom spells
  if (!isCardPresent()) {
    // Display error on screen for user feedback
    displayError("SD Card Required");
    delay(2000);  // Give user time to read error message
    spellRecordingState = SPELL_RECORD_COMPLETE;
    return;
  }
  
  // Prepare display for clean gesture capture
  clearDisplay();
  clearIRTrail();  // Remove any previous tracking visualization
  recordedSpellPattern.clear();  // Start with empty pattern
  
  // Set global flag that camera state machine checks
  isRecordingCustomSpell = true;
  spellRecordingState = SPELL_RECORD_TRACKING;
  
  LOG_DEBUG("Spell recording: Ready to track (use existing tracking)");
}

/**
 * Exit spell recording mode
 * 
 * Terminates the custom spell recording workflow and cleans up state.
 * Called when user cancels recording or after successfully saving a pattern.
 * Resets all recording-related flags and clears captured data.
 * 
 * Side Effects:
 *   - Sets isRecordingCustomSpell = false (disables camera capture)
 *   - Returns spellRecordingState to IDLE
 *   - Clears recordedSpellPattern to free memory
 *   - Clears display (returns to normal menu/operation)
 */
void exitSpellRecordingMode() {
  LOG_DEBUG("Exiting spell recording mode");
  
  // Disable custom spell capture in camera state machine
  isRecordingCustomSpell = false;
  spellRecordingState = SPELL_RECORD_IDLE;
  
  // Free memory from captured pattern
  recordedSpellPattern.clear();
  
  // Clean up display
  clearDisplay();
}

/**
 * Save recorded pattern to spells.json
 * 
 * Writes the captured gesture pattern to the SD card in JSON format.
 * Handles file I/O, JSON parsing/creation, auto-naming, and spell reload.
 * 
 * Algorithm:
 *   1. Verify SD card is still present
 *   2. Read existing spells.json (if it exists)
 *   3. Parse JSON to get "custom" array
 *   4. Scan existing custom spells to find highest number (for auto-naming)
 *   5. Create new spell entry with "Custom N+1" name
 *   6. Add all points from recordedSpellPattern as x,y coordinates
 *   7. Write updated JSON back to spells.json
 *   8. Reload spell patterns so new spell is immediately available
 * 
 * Auto-Naming:
 *   - Scans existing custom spells for names like "Custom 1", "Custom 2"
 *   - Finds the highest number (e.g., if Custom 1, 2, 5 exist, highest = 5)
 *   - New spell gets next number (e.g., "Custom 6")
 *   - This ensures unique names even if middle entries are deleted
 * 
 * Error Handling:
 *   - Returns false if SD card removed
 *   - Returns false if JSON parse fails
 *   - Returns false if file write fails
 *   - Logs all errors for debugging
 * 
 * return true on successful save, false on any error
 */
bool saveRecordedSpell() {
  LOG_DEBUG("Saving custom spell to SD card");
  
  // Verify SD card still present (could have been removed during recording)
  if (!isCardPresent()) {
    LOG_ALWAYS("Cannot save spell - no SD card");
    return false;
  }
  
  const char* configFile = "/spells.json";
  JsonDocument doc;
  
  // Read existing file if it exists (to preserve other custom spells)
  if (SD.exists(configFile)) {
    File file = SD.open(configFile, FILE_READ);
    if (file) {
      DeserializationError error = deserializeJson(doc, file);
      file.close();
      
      if (error) {
        LOG_ALWAYS("Failed to parse existing spells.json");
        return false;
      }
    }
  }
  
  // Get or create "custom" array in JSON structure
  // If spells.json doesn't have a "custom" section yet, create it
  JsonArray customArray;
  if (doc["custom"].is<JsonArray>()) {
    customArray = doc["custom"].as<JsonArray>();
  } else {
    customArray = doc["custom"].to<JsonArray>();
  }
  
  // Scan existing custom spells to find highest number for auto-naming
  // Example: if "Custom 1", "Custom 3", "Custom 7" exist, maxCustomNum = 7
  int maxCustomNum = 0;
  for (JsonObject custom : customArray) {
    const char* name = custom["name"];
    // Check if name starts with "Custom "
    if (name && strncmp(name, "Custom ", 7) == 0) {
      // Extract number after "Custom " (e.g., "Custom 5" -> 5)
      int num = atoi(name + 7);
      if (num > maxCustomNum) {
        maxCustomNum = num;
      }
    }
  }
  
  // Generate new spell name: "Custom N+1"
  char newName[32];
  snprintf(newName, sizeof(newName), "Custom %d", maxCustomNum + 1);
  
  // Create new spell object in JSON array
  JsonObject newSpell = customArray.add<JsonObject>();
  newSpell["name"] = newName;
  
  // Add pattern array with all captured points
  // Format: [{"x": 100, "y": 200}, {"x": 110, "y": 210}, ...]
  JsonArray patternArray = newSpell["pattern"].to<JsonArray>();
  for (const auto& p : recordedSpellPattern) {
    JsonObject pointObj = patternArray.add<JsonObject>();
    pointObj["x"] = p.x;
    pointObj["y"] = p.y;
    // Note: timestamp not saved, will be regenerated during normalization
  }
  
  // Write updated JSON back to file (overwrites existing)
  File file = SD.open(configFile, FILE_WRITE);
  if (!file) {
    LOG_ALWAYS("Failed to open spells.json for writing");
    return false;
  }
  
  if (serializeJson(doc, file) == 0) {
    LOG_ALWAYS("Failed to write JSON to file");
    file.close();
    return false;
  }
  
  file.close();
  LOG_DEBUG("Saved custom spell '%s' with %d points", newName, recordedSpellPattern.size());
  
  // Reload spell patterns so new spell is immediately available for matching
  // This rebuilds the global spellPatterns vector with built-in + custom spells
  initSpellPatterns();  // Load built-in spells
  loadCustomSpells();   // Apply customizations from spells.json
  
  return true;
}

/**
 * Get list of custom spell names for web portal
 * 
 * Scans the global spellPatterns vector and extracts all spell names that
 * start with "Custom " (indicating user-created spells). Used by the web
 * configuration portal to display available custom spells.
 * 
 * Implementation:
 *   - Iterates through all loaded spells in spellPatterns
 *   - Checks if name starts with "Custom " prefix
 *   - Adds matching names to return vector
 * 
 * Note: This function searches the in-memory spellPatterns vector, not the
 * SD card directly. Call after initSpellPatterns() + loadCustomSpells() to
 * ensure custom spells are loaded.
 * 
 * return Vector of custom spell name strings (e.g., ["Custom 1", "Custom 2"])
 */
std::vector<String> getCustomSpellNames() {
  std::vector<String> customNames;
  
  // Scan all loaded spells for custom entries
  for (const auto& spell : spellPatterns) {
    // Check if name starts with "Custom " (7 character prefix)
    if (strncmp(spell.name, "Custom ", 7) == 0) {
      customNames.push_back(String(spell.name));
    }
  }
  
  return customNames;
}

/**
 * Rename a custom spell
 * 
 * Updates a custom spell's name in spells.json and reloads patterns.
 * Used for manual spell renaming by editing the SD card file directly
 * or potentially through future web portal functionality.
 * 
 * Algorithm:
 *   1. Verify SD card present and spells.json exists
 *   2. Read and parse entire spells.json file
 *   3. Search "custom" array for spell matching oldName
 *   4. Update the name field to newName
 *   5. Write entire JSON back to file
 *   6. Reload spell patterns to apply change immediately
 * 
 * Validation:
 *   - Checks SD card availability before proceeding
 *   - Verifies spells.json file exists
 *   - Confirms JSON parsing succeeds
 *   - Ensures spell with oldName is found
 * 
 * Side Effects:
 *   - Overwrites spells.json with updated content
 *   - Reloads global spellPatterns vector (expensive operation)
 *   - New name immediately available for matching
 * 
 * Example:
 *   renameCustomSpell("Custom 1", "Fireball") changes spell name from
 *   auto-generated "Custom 1" to user-friendly "Fireball"
 * 
 * oldName: Current spell name to search for (e.g., "Custom 1")
 * newName: New name to assign (e.g., "Fireball")
 * return true on successful rename, false on any error
 */
bool renameCustomSpell(const char* oldName, const char* newName) {
  LOG_DEBUG("Renaming spell '%s' to '%s'", oldName, newName);
  
  // Verify SD card is present
  if (!isCardPresent()) {
    return false;
  }
  
  // Verify spells.json exists
  const char* configFile = "/spells.json";
  if (!SD.exists(configFile)) {
    return false;
  }
  
  // Read existing spells.json
  File file = SD.open(configFile, FILE_READ);
  if (!file) {
    return false;
  }
  
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  
  if (error) {
    return false;
  }
  
  // Find spell with matching name in custom array
  if (doc["custom"].is<JsonArray>()) {
    JsonArray customArray = doc["custom"].as<JsonArray>();
    
    for (JsonObject custom : customArray) {
      const char* name = custom["name"];
      
      // Found matching spell - update its name
      if (name && strcmp(name, oldName) == 0) {
        custom["name"] = newName;
        
        // Write updated JSON back to file
        file = SD.open(configFile, FILE_WRITE);
        if (!file) {
          return false;
        }
        
        serializeJson(doc, file);
        file.close();
        
        // Reload all spell patterns to apply change
        // This rebuilds spellPatterns with new name
        initSpellPatterns();  // Load built-in spells
        loadCustomSpells();   // Apply customizations from spells.json
        
        LOG_DEBUG("Successfully renamed spell");
        return true;
      }
    }
  }
  
  // Spell with oldName not found in custom array
  return false;
}

bool renameCustomSpellsBatch(const std::vector<SpellRenamePair>& renames) {
  LOG_DEBUG("Batch renaming %d spells", (int)renames.size());
  if (!isCardPresent()) return false;
  const char* configFile = "/spells.json";
  if (!SD.exists(configFile)) return false;

  File file = SD.open(configFile, FILE_READ);
  if (!file) return false;

  DynamicJsonDocument doc(16384);
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  if (error) return false;

  bool anyChanged = false;
  if (doc["custom"].is<JsonArray>()) {
    JsonArray customArray = doc["custom"].as<JsonArray>();
    for (JsonObject custom : customArray) {
      const char* name = custom["name"];
      if (!name) continue;
      for (const auto& r : renames) {
        if (strcmp(name, r.oldName.c_str()) == 0) {
          custom["name"] = r.newName.c_str();
          anyChanged = true;
          break;
        }
      }
    }
  }

  if (!anyChanged) return false;

  file = SD.open(configFile, FILE_WRITE);
  if (!file) return false;
  if (serializeJson(doc, file) == 0) {
    file.close();
    return false;
  }
  file.close();

  // Reload once
  initSpellPatterns();
  loadCustomSpells();

  LOG_DEBUG("Batch rename applied and spell patterns reloaded");
  return true;
}
