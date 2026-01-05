#ifndef SD_FUNCTIONS_H
#define SD_FUNCTIONS_H

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>

// SD Card pin definitions (separate SPI bus from display)
#define SD_MOSI 17
#define SD_MISO 16
#define SD_SCK  18
#define SD_CS   19
#define SD_DETECT 35  // Card detect switch

// SD card functions
bool initSD();
bool isCardPresent();
bool fileExists(const char* path);
File openFile(const char* path, const char* mode = FILE_READ);
void listDirectory(const char* dirname, uint8_t levels = 1);
bool loadImageData(const char* filename, uint8_t** buffer, uint16_t* width, uint16_t* height);

// Helper function to read BMP file header info
bool readBMPHeader(File& file, uint16_t* width, uint16_t* height, uint16_t* bitDepth);

// Check which spells have image files available
void checkSpellImages();
bool hasSpellImage(const char* spellName);
String getSpellImageFilename(const char* spellName);  // Get the image filename for a spell

// Load custom spell configurations from SD card
bool loadCustomSpells();

#endif
