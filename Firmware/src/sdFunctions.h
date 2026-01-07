/*
================================================================================
  SD Functions - SD Card Interface Header
================================================================================
  
  This module provides SD card operations for storing and loading data.
  
  Hardware:
    - SD card reader (SPI interface, separate bus from display)
    - Card detect switch (optional)
  
  Pin Configuration:
    - SD_MOSI: GPIO 17 (SPI Data Out)
    - SD_MISO: GPIO 16 (SPI Data In)
    - SD_SCK:  GPIO 18 (SPI Clock)
    - SD_CS:   GPIO 19 (Chip Select)
    - SD_DETECT: GPIO 35 (Card detect switch, optional)
  
  File Operations:
    - Initialize SD card and file system
    - Check file existence
    - Open/read files
    - List directory contents
  
  Custom Spell System:
    - spells.json: JSON configuration file for customizing patterns
    - Supports three operations: "modify", "add", "replace"
    - Allows users to change spell patterns without recompiling firmware  
================================================================================
*/

#ifndef SD_FUNCTIONS_H
#define SD_FUNCTIONS_H

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>

//=====================================
// Hardware Pin Definitions
//=====================================

/// SPI MOSI (Master Out Slave In) - data to SD card
#define SD_MOSI 17

/// SPI MISO (Master In Slave Out) - data from SD card
#define SD_MISO 16

/// SPI SCK (Serial Clock)
#define SD_SCK  18

/// SPI CS (Chip Select) - active low
#define SD_CS   19

/// Card detect switch pin (optional, reads low when card inserted)
#define SD_DETECT 35

//=====================================
// SD Card Core Functions
//=====================================

/**
 * Initialize SD card and SPI bus
 * Configures SPI pins and attempts to mount SD card filesystem.
 * Separate SPI bus from display to avoid conflicts.
 * return true if SD card initialized successfully, false on error
 */
bool initSD();

/**
 * Check if SD card is physically present
 * Reads SD_DETECT pin (if hardware supports card detect switch).
 * return true if card detected, false if not present
 */
bool isCardPresent();

/**
 * Check if file exists on SD card
 * path: File path (relative to SD root, e.g., "/spells.json")
 * return true if file exists and is accessible, false otherwise
 */
bool fileExists(const char* path);

/**
 * Open file on SD card
 * path: File path (relative to SD root)
 * mode: File mode: FILE_READ (default), FILE_WRITE, FILE_APPEND
 * return File object (check file.available() to verify success)
 */
File openFile(const char* path, const char* mode = FILE_READ);

/**
 * List directory contents recursively
 * Prints directory tree to serial console for debugging.
 * dirname: Directory path
 * levels: Recursion depth (0 = current directory only, 1 = +1 level, etc.)
 */
void listDirectory(const char* dirname, uint8_t levels = 1);

/**
 * Load image file into memory buffer
 * Allocates memory and reads image data from SD card.
 * Caller is responsible for freeing buffer with free().
 * filename: Path to image file
 * buffer: Pointer to buffer pointer (allocated by function)
 * width: Pointer to store image width
 * height: Pointer to store image height
 * return true if loaded successfully, false on error
 */
bool loadImageData(const char* filename, uint8_t** buffer, uint16_t* width, uint16_t* height);

//=====================================
// Image File Functions
//=====================================

/**
 * Read BMP file header information
 * Parses BMP header to extract image dimensions and bit depth.
 * File position is left at start of pixel data.
 * file: Opened BMP file object
 * width: Pointer to store image width
 * height: Pointer to store image height
 * bitDepth: Pointer to store bits per pixel
 * return true if header parsed successfully, false on error
 */
bool readBMPHeader(File& file, uint16_t* width, uint16_t* height, uint16_t* bitDepth);

/**
 * Check which spells have image files available
 * Scans SD card for .bmp image files matching spell names.
 * Logs results to serial console.
 * Called during setup() after spell patterns are initialized.
 */
void checkSpellImages();

/**
 * Check if specific spell has an image file
 * spellName: Spell name to check
 * return true if image file exists, false otherwise
 */
bool hasSpellImage(const char* spellName);

/**
 * Get image filename for a spell
 * Returns the filename that should be used for spell image.
 * Checks for custom image filename in pattern definition first,
 * then falls back to default "<spellname>.bmp" format.
 * spellName: Spell name
 * return Image filename (e.g., "ignite.bmp")
 */
String getSpellImageFilename(const char* spellName);

//=====================================
// Custom Spell Configuration
//=====================================

/**
 * Load custom spell configurations from spells.json
 * Parses JSON file and applies custom spell modifications:
 * JSON file location: /spells.json on SD card root
 * See CUSTOM_SPELLS.md and spells.json.example for format details.
 * return true if file loaded and parsed successfully, false on error
 */
bool loadCustomSpells();

#endif // SD_FUNCTIONS_H
