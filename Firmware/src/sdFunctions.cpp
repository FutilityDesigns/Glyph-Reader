/**
================================================================================
 SD Functions - SD Card Interface and Custom Spell Management
================================================================================
 * This module manages SD card operations for loading custom spell patterns
 * and associated BMP image files. Provides abstraction layer for file I/O
 * and implements JSON-based spell customization system.
 * 
 * HARDWARE:
 * - Interface: SPI (HSPI bus, separate from display SPI)
 * - Card Detect: Optional GPIO switch 
 * 
 * FILE SYSTEM STRUCTURE:
 * - /spells.json: Custom spell configurations (optional)
 * - /<spellname>.bmp: Spell image files (240x240, 24-bit BMP)
 * - Custom image filenames supported via JSON configuration
 * 
 * CUSTOM SPELL SYSTEM (/spells.json):
 * The JSON configuration file supports two operations:
 * 
 * 1. MODIFY BUILT-IN SPELLS:
 *    - Change display name (e.g., "Lumos" → "Light")
 *    - Specify custom image file (e.g., "my_image.bmp")
 *    - Redefine gesture pattern (array of x,y coordinates)
 * 
 * 2. ADD NEW SPELLS:
 *    - Define completely new spell patterns
 *    - Associate custom image files
 *    - Pattern format: array of {x, y} objects (0-1000 range)
 * 
 * BMP IMAGE SUPPORT:
 * - Format: 24-bit uncompressed BMP (BI_RGB)
 * - Size: 240x240 pixels recommended for full-screen display
 * - Color: RGB888 (converted to RGB565 for display)
 * - Naming: Default is /<spellname>.bmp (lowercase)
 * - Custom naming via JSON "imageFile" field
 */

#include "sdFunctions.h"
#include "glyphReader.h"
#include "spell_patterns.h"
#include <map>
#include <ArduinoJson.h>

// Configure whether the card-detect switch is active-low (pulls to GND when card present)
#ifndef SD_DETECT_ACTIVE_LOW
#define SD_DETECT_ACTIVE_LOW 1
#endif

// Create separate SPI instance for SD card
SPIClass sdSPI(HSPI);  // Use HSPI bus for SD card

// Map to track which spells have image files
std::map<String, bool> spellImageAvailable;

// Variable to track number of custom spells loaded
int numCustomSpells = 0;

// Initialize SD card
bool initSD() {

    LOG_DEBUG("Initializing SD card...");
  
#ifndef NO_SD_SWITCH // Only if card detect switch is used
  // Configure card detect pin only if switch is available
  pinMode(SD_DETECT, INPUT_PULLUP);

  // Check if card is physically present
    if (!isCardPresent()) {
        LOG_DEBUG("No SD card detected (switch open)");
        return false;
    }
#endif
  
  // Initialize SPI bus for SD card
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  
  // Initialize SD card
  if (!SD.begin(SD_CS, sdSPI, 80000000)) {  // 80MHz SPI speed
    LOG_ALWAYS("SD card initialization failed!");
    return false;
  }
  
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  LOG_DEBUG("SD card initialized successfully! Size: %llu MB", cardSize);
  LOG_DEBUG("Card type: ");
  
  uint8_t cardType = SD.cardType();
  switch(cardType) {
    case CARD_MMC:
      LOG_DEBUG("MMC");
      break;
    case CARD_SD:
      LOG_DEBUG("SDSC");
      break;
    case CARD_SDHC:
      LOG_DEBUG("SDHC");
      break;
    default:
      LOG_DEBUG("UNKNOWN");
  }
  
  return true;
}

// Check if SD card is present
bool isCardPresent() {
#ifdef NO_SD_SWITCH
  // No physical switch - try to read card directly
  // Caller should ensure SD.begin() has been called before relying on cardType
  uint8_t cardType = SD.cardType();
  return cardType != CARD_NONE;
#else
  // Check physical switch first
  // Read detect pin (with debounce) and log state for debugging
  int state1 = digitalRead(SD_DETECT);
  delay(5);
  int state2 = digitalRead(SD_DETECT);
  int state = (state1 == state2) ? state1 : digitalRead(SD_DETECT);
  LOG_DEBUG("SD detect pin %d read values: %d,%d => %d (SD_DETECT_ACTIVE_LOW=%d)", SD_DETECT, state1, state2, state, SD_DETECT_ACTIVE_LOW);
  bool present = SD_DETECT_ACTIVE_LOW ? (state == LOW) : (state == HIGH);
  // Do NOT call SD.cardType() here because SD.begin() may not have been called yet.
  // initSD() will perform SD initialization and then check card type after SD.begin().
  return present;
#endif
}

// Check if a file exists on SD card
bool fileExists(const char* path) {
  if (!isCardPresent()) return false;
  return SD.exists(path);
}

// Open a file on SD card
File openFile(const char* path, const char* mode) {
  if (!isCardPresent()) {
    LOG_DEBUG("No SD card present");
    return File();
  }
  
  return SD.open(path, mode);
}

// List directory contents
void listDirectory(const char* dirname, uint8_t levels) {
  LOG_DEBUG("Listing directory: %s", dirname);
  
  File root = SD.open(dirname);
  if (!root) {
    LOG_DEBUG("Failed to open directory");
    return;
  }
  
  if (!root.isDirectory()) {
    LOG_DEBUG("Not a directory");
    return;
  }
  
  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDirectory(file.path(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("\tSIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

// Helper function to read BMP file header info
bool readBMPHeader(File& file, uint16_t* width, uint16_t* height, uint16_t* bitDepth) {
  // Read BMP header (54 bytes minimum)
  if (file.size() < 54) {
    LOG_DEBUG("File too small to be a valid BMP");
    return false;
  }
  
  // Read and verify BMP signature
  uint16_t signature = file.read() | (file.read() << 8);
  if (signature != 0x4D42) {  // 'BM' in little endian
    LOG_DEBUG("Not a valid BMP file (wrong signature)");
    return false;
  }
  
  // Skip file size (4 bytes) and reserved (4 bytes)
  file.seek(10);
  
  // Read pixel data offset
  uint32_t dataOffset = file.read() | (file.read() << 8) | (file.read() << 16) | (file.read() << 24);
  
  // Skip header size (4 bytes)
  file.seek(18);
  
  // Read width (4 bytes)
  *width = file.read() | (file.read() << 8) | (file.read() << 16) | (file.read() << 24);
  
  // Read height (4 bytes)
  *height = file.read() | (file.read() << 8) | (file.read() << 16) | (file.read() << 24);
  
  // Skip planes (2 bytes)
  file.seek(28);
  
  // Read bit depth (2 bytes)
  *bitDepth = file.read() | (file.read() << 8);
  
  Serial.printf("BMP Info: %dx%d, %d-bit\n", *width, *height, *bitDepth);
  
  // Only support 24-bit uncompressed BMPs
  if (*bitDepth != 24) {
    LOG_ALWAYS("Only 24-bit BMPs are supported");
    return false;
  }
  
  // Check compression (should be 0 for uncompressed)
  file.seek(30);
  uint32_t compression = file.read() | (file.read() << 8) | (file.read() << 16) | (file.read() << 24);
  if (compression != 0) {
    LOG_ALWAYS("Only uncompressed BMPs are supported");
    return false;
  }
  
  // Seek to start of pixel data
  file.seek(dataOffset);
  
  return true;
}

// Load BMP image data into buffer (RGB565 format)
bool loadImageData(const char* filename, uint8_t** buffer, uint16_t* width, uint16_t* height) {
  if (!isCardPresent()) {
    LOG_DEBUG("No SD card present");
    return false;
  }
  
  File file = SD.open(filename, FILE_READ);
  if (!file) {
    LOG_DEBUG("Failed to open file: %s", filename);
    return false;
  }
  
  uint16_t bitDepth;
  if (!readBMPHeader(file, width, height, &bitDepth)) {
    file.close();
    return false;
  }
  
  // Calculate buffer size (RGB565 format = 2 bytes per pixel)
  uint32_t bufferSize = (*width) * (*height) * 2;
  *buffer = (uint8_t*)malloc(bufferSize);
  
  if (*buffer == NULL) {
    LOG_DEBUG("Failed to allocate memory for image buffer");
    file.close();
    return false;
  }
  
  // BMP rows are padded to 4-byte boundaries
  uint16_t rowSize = ((*width) * 3 + 3) & ~3;
  uint8_t* rowBuffer = (uint8_t*)malloc(rowSize);
  
  if (rowBuffer == NULL) {
    LOG_DEBUG("Failed to allocate row buffer");
    free(*buffer);
    file.close();
    return false;
  }
  
  // BMPs are stored bottom-to-top, so read in reverse
  for (int y = *height - 1; y >= 0; y--) {
    file.read(rowBuffer, rowSize);
    
    for (int x = 0; x < *width; x++) {
      // BMP format is BGR
      uint8_t b = rowBuffer[x * 3];
      uint8_t g = rowBuffer[x * 3 + 1];
      uint8_t r = rowBuffer[x * 3 + 2];
      
      // Convert to RGB565
      uint16_t color565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
      
      // Store in buffer (little endian)
      int bufferIndex = (y * (*width) + x) * 2;
      (*buffer)[bufferIndex] = color565 >> 8;
      (*buffer)[bufferIndex + 1] = color565 & 0xFF;
    }
  }
  
  free(rowBuffer);
  file.close();
  
  LOG_DEBUG("Successfully loaded image: %s (%dx%d)\n", filename, *width, *height);
  return true;
}

// Count how many spell images are available
int countSpellImages() {
  int count = 0;
  for (const auto& pair : spellImageAvailable) {
    if (pair.second) count++;
  }
  return count;
}

// Check for spell image files on SD card
void checkSpellImages() {
  LOG_DEBUG("Checking for spell image files...");
  
  if (!isCardPresent()) {
    LOG_DEBUG("No SD card present - no spell images available");
    return;
  }
  
  // Check each spell pattern for a corresponding BMP file
  for (const auto& spell : spellPatterns) {
    String spellNameLower = String(spell.name);
    spellNameLower.toLowerCase();
    String filename;
    
    // Use custom image filename if defined, otherwise use default naming
    if (spell.customImageFilename.length() > 0) {
      filename = spell.customImageFilename;
      // Ensure it starts with /
      if (!filename.startsWith("/")) {
        filename = "/" + filename;
      }
    } else {
      // Default: spell name + .bmp
      filename = "/" + spellNameLower + ".bmp";
    }
    
    // Check if file exists
    if (SD.exists(filename)) {
      spellImageAvailable[spellNameLower] = true;
      LOG_DEBUG("  ✓ Found image for '%s': %s", spell.name, filename.c_str());
    } else {
      spellImageAvailable[spellNameLower] = false;
      LOG_DEBUG("  ✗ No image for '%s' (will use text)", spell.name);
    }
  }
  
  LOG_DEBUG("Spell image check complete: %d/%d spells have images", 
                countSpellImages(), spellPatterns.size());
}

// Check if a spell has an associated image
bool hasSpellImage(const char* spellName) {
  String name = String(spellName);
  name.toLowerCase();  // Normalize to lowercase to match map keys
  auto it = spellImageAvailable.find(name);
  if (it != spellImageAvailable.end()) {
    return it->second;
  }
  return false;
}

// Get the image filename for a spell (returns empty string if no image)
String getSpellImageFilename(const char* spellName) {
  if (!hasSpellImage(spellName)) {
    return "";
  }
  
  // Find the spell in spellPatterns to check for custom image filename
  for (const auto& spell : spellPatterns) {
    if (strcasecmp(spell.name, spellName) == 0) {
      if (spell.customImageFilename.length() > 0) {
        // Use custom image filename
        String filename = spell.customImageFilename;
        // Ensure it starts with /
        if (!filename.startsWith("/")) {
          filename = "/" + filename;
        }
        return filename;
      }
      break;
    }
  }
  
  // Default: spell name + .bmp
  String filename = "/";
  filename += String(spellName);
  filename.toLowerCase();
  filename += ".bmp";
  return filename;
}

// Load custom spell configurations from SD card
// Expected file: /spells.json
// Returns true if file was successfully parsed (or doesn't exist), false on error
bool loadCustomSpells() {
  const char* configFile = "/spells.json";
  
  if (!isCardPresent()) {
    LOG_DEBUG("No SD card present - skipping custom spells");
    return true;  // Not an error, just no customization
  }
  
  if (!SD.exists(configFile)) {
    LOG_DEBUG("No %s found - using default spells only", configFile);
    return true;  // Not an error, just no customization
  }
  
  File file = SD.open(configFile, FILE_READ);
  if (!file) {
    LOG_ALWAYS("Failed to open %s", configFile);
    return false;
  }
  
  // Read file into string
  size_t fileSize = file.size();
  if (fileSize > 16384) {  // Limit to 16KB
    LOG_ALWAYS("Config file too large (max 16KB)");
    file.close();
    return false;
  }
  
  String jsonString;
  jsonString.reserve(fileSize);
  while (file.available()) {
    jsonString += (char)file.read();
  }
  file.close();
  
  // Parse JSON
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, jsonString);
  
  if (error) {
    LOG_ALWAYS("Failed to parse %s: %s", configFile, error.c_str());
    return false;
  }
  
  LOG_DEBUG("Successfully loaded %s", configFile);
  
  // Process modifications to existing spells
  if (doc["modify"].is<JsonArray>()) {
    JsonArray modifications = doc["modify"].as<JsonArray>();
    for (JsonObject mod : modifications) {
      const char* builtInName = mod["builtInName"];
      if (!builtInName) continue;
      
      // Find the built-in spell
      bool found = false;
      for (auto& spell : spellPatterns) {
        if (strcasecmp(spell.name, builtInName) == 0) {
          found = true;
          
          // Apply custom name if provided
          if (mod["customName"].is<const char*>()) {
            spell.name = strdup(mod["customName"].as<const char*>());
            LOG_DEBUG("  Renamed '%s' to '%s'", builtInName, spell.name);
          }
          
          // Apply custom image filename if provided
          if (mod["imageFile"].is<const char*>()) {
            spell.customImageFilename = mod["imageFile"].as<String>();
            LOG_DEBUG("  Custom image for '%s': %s", spell.name, spell.customImageFilename.c_str());
          }
          
          // Apply custom pattern if provided
          if (mod["pattern"].is<JsonArray>()) {
            JsonArray patternArray = mod["pattern"].as<JsonArray>();
            if (patternArray.size() > 0) {
              spell.pattern.clear();
              int pointIndex = 0;
              for (JsonObject pointObj : patternArray) {
                Point p;
                p.x = pointObj["x"] | 0;
                p.y = pointObj["y"] | 0;
                p.timestamp = pointIndex * 100;  // Auto-generate timestamps
                spell.pattern.push_back(p);
                pointIndex++;
              }
              LOG_DEBUG("  Redefined pattern for '%s' with %d points", spell.name, spell.pattern.size());
            }
          }
          
          break;
        }
      }
      
      if (!found) {
        LOG_DEBUG("  Warning: Built-in spell '%s' not found for modification", builtInName);
      }
    }
  }
  
  // Process new custom spells
  if (doc["custom"].is<JsonArray>()) {
    JsonArray customSpells = doc["custom"].as<JsonArray>();
    for (JsonObject custom : customSpells) {
      const char* name = custom["name"];
      if (!name) {
        LOG_DEBUG("  Skipping custom spell with no name");
        continue;
      }
      
      SpellPattern newSpell;
      newSpell.name = strdup(name);
      
      // Get custom image filename if provided
      if (custom["imageFile"].is<const char*>()) {
        newSpell.customImageFilename = custom["imageFile"].as<String>();
      }
      
      // Get pattern points
      if (custom["pattern"].is<JsonArray>()) {
        JsonArray patternArray = custom["pattern"].as<JsonArray>();
        int pointIndex = 0;
        for (JsonObject pointObj : patternArray) {
          Point p;
          p.x = pointObj["x"] | 0;
          p.y = pointObj["y"] | 0;
          p.timestamp = pointIndex * 100;  // Auto-generate timestamps
          newSpell.pattern.push_back(p);
          pointIndex++;
        }
      }
      
      if (newSpell.pattern.size() > 0) {
        spellPatterns.push_back(newSpell);
        numCustomSpells++;
        LOG_DEBUG("  Added custom spell '%s' with %d points", name, newSpell.pattern.size());
      } else {
        LOG_DEBUG("  Skipping custom spell '%s' - no pattern defined", name);
      }
    }
  }
  
  LOG_DEBUG("Custom spell configuration applied. Total spells: %d", spellPatterns.size());
  return true;
}


