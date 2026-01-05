/*
All functions realted to screen operations
*/

#include "screenFunctions.h"
#include "glyphReader.h"
#include "sdFunctions.h"

// Create display instance
Adafruit_GC9A01A tft(TFT_CS, TFT_DC, TFT_RST);

// Track last IR point position for clearing
static int lastIRX = -1;
static int lastIRY = -1;

void screenInit() {
  LOG_DEBUG("Initializing GC9A01 Display...");
  
  // Initialize SPI manually first
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  
  // Initialize the display
  LOG_DEBUG("Calling tft.begin()...");
  tft.begin();
  LOG_DEBUG("Display initialized!");
  
  delay(100);
  
  // Configure Backlight pin for manual control
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  LOG_DEBUG("Backlight pin %d set to HIGH", TFT_BL);
  delay(100);
  
  tft.setRotation(0);  // 0-3 for different orientations
  
  // Fill screen with random color (using ESP32 hardware RNG)
  uint16_t randomColor = esp_random() & 0xFFFF;
  tft.fillScreen(randomColor);
  delay(500);  // Show random color briefly
  
  // Draw outer circle as reference frame (240x240 round display)
  tft.drawCircle(120, 120, 119, 0x4208);  // Dark gray circle
  
  LOG_DEBUG("Display initialized successfully!");
}

// Function for displaying setup process
void updateSetupDisplay(int line, const String &function, const String &status) {

    // set cursor to position based on line * 10
    tft.setTextSize(1);
    tft.setTextColor(0xFFFF); // White color
    tft.setCursor(50, line * 10 + 40);
    if (status == "init") {
      tft.print(function + "...");
    } else {
      tft.print(function + "... " + status);
    }
}

// Draw IR point on display (scales 1024x1024 camera coords to 240x240 display)
void drawIRPoint(int x, int y, bool isActive) {
  // Clear previous point if it exists
  if (lastIRX >= 0 && lastIRY >= 0) {
    tft.fillCircle(lastIRX, lastIRY, 6, 0x0000);  // Black (clear)
  }
  
  if (isActive && x >= 0 && y >= 0) {
    // Scale from camera coordinates (0-1023) to display (0-239)
    int displayX = map(x, 0, 1023, 0, 239);
    int displayY = map(y, 0, 1023, 0, 239);
    
    // Draw new point
    tft.fillCircle(displayX, displayY, 5, 0xFFE0);  // Yellow center
    tft.drawCircle(displayX, displayY, 6, 0xF800);  // Red outline
    
    // Update last position
    lastIRX = displayX;
    lastIRY = displayY;
  } else {
    // No active point
    lastIRX = -1;
    lastIRY = -1;
  }
}

// Display spell name on screen
void displaySpellName(const char* spellName) {
  // Clear screen
  tft.fillScreen(0x0000);
  
  // Check if there's an image for this spell
  if (hasSpellImage(spellName)) {
    // Get the appropriate filename (custom or default)
    String filename = getSpellImageFilename(spellName);
    
    Serial.printf("Displaying image for spell: %s\n", filename.c_str());
    
    // Display the image centered on screen (0, 0 for 240x240 image)
    if (displayImageFromSD(filename.c_str(), 0, 0)) {
      // Image displayed successfully
      screenSpellOnTime = millis();
      screenOnTime = millis();
      return;
    } else {
      // Image failed to load, fall through to text display
      Serial.println("Failed to load image, falling back to text");
      tft.fillScreen(0x0000);  // Clear any partial image
    }
  }
  
  // Draw decorative circle
  tft.drawCircle(120, 120, 110, 0x780F);  // Purple
  tft.drawCircle(120, 120, 105, 0x780F);
  
  tft.setTextSize(3);
  tft.setTextColor(0x07FF);  // Cyan
  
  // Check if spell name has a space (multi-word)
  String spell = String(spellName);
  int spaceIndex = spell.indexOf(' ');
  
  if (spaceIndex > 0) {
    // Multi-word spell - split into two lines
    String firstWord = spell.substring(0, spaceIndex);
    String secondWord = spell.substring(spaceIndex + 1);
    
    // Get bounds for each word
    int16_t x1, y1;
    uint16_t w1, h1, w2, h2;
    tft.getTextBounds(firstWord.c_str(), 0, 0, &x1, &y1, &w1, &h1);
    tft.getTextBounds(secondWord.c_str(), 0, 0, &x1, &y1, &w2, &h2);
    
    // Calculate vertical spacing (total height + gap)
    int totalHeight = h1 + h2 + 10;  // 10 pixel gap between lines
    int startY = (240 - totalHeight) / 2;
    
    // Display first word centered
    int centerX1 = (240 - w1) / 2;
    tft.setCursor(centerX1, startY);
    tft.println(firstWord);
    
    // Display second word centered below
    int centerX2 = (240 - w2) / 2;
    tft.setCursor(centerX2, startY + h1 + 10);
    tft.println(secondWord);
    
  } else {
    // Single word - center it
    int16_t x1, y1;
    uint16_t w, h;
    tft.getTextBounds(spellName, 0, 0, &x1, &y1, &w, &h);
    
    int centerX = (240 - w) / 2;
    int centerY = (240 - h) / 2;
    
    tft.setCursor(centerX, centerY);
    tft.println(spellName);
  }
  
  screenSpellOnTime = millis();
  screenOnTime = millis();  // Reset overall screen timeout as well
}

// Clear display and redraw reference circle
void clearDisplay() {
  tft.fillScreen(0x0000);
  tft.drawCircle(120, 120, 119, 0x4208);  // Dark gray circle
  lastIRX = -1;
  lastIRY = -1;
}

// Backlight control functions
void backlightOff() {
    digitalWrite(TFT_BL, LOW);
    LOG_DEBUG("Backlight turned OFF (pin %d set to LOW)", TFT_BL);
    backlightStateOn = false;
}

void backlightOn() {
    digitalWrite(TFT_BL, HIGH);
    LOG_DEBUG("Backlight turned ON (pin %d set to HIGH)", TFT_BL);
    backlightStateOn = true;
}

// Display BMP image from SD card
bool displayImageFromSD(const char* filename, int16_t x, int16_t y) {
  LOG_DEBUG("Loading image from SD: %s", filename);
  
  // Check if card is present
  if (!isCardPresent()) {
    LOG_DEBUG("No SD card present");
    return false;
  }
  
  // Open file
  File file = openFile(filename, FILE_READ);
  if (!file) {
    LOG_DEBUG("Failed to open file: %s", filename);
    return false;
  }
  
  // Read BMP header
  uint16_t width, height, bitDepth;
  if (!readBMPHeader(file, &width, &height, &bitDepth)) {
    file.close();
    return false;
  }
  
  // Get the data offset (where pixel data starts)
  uint32_t dataOffset = file.position();
  
  // BMP rows are padded to 4-byte boundaries
  uint16_t rowSize = ((width * 3) + 3) & ~3;
  uint8_t* rowBuffer = (uint8_t*)malloc(rowSize);
  
  if (rowBuffer == NULL) {
    LOG_DEBUG("Failed to allocate row buffer");
    file.close();
    return false;
  }
  
  // Allocate RGB565 buffer for one row
  uint16_t* rgb565Buffer = (uint16_t*)malloc(width * sizeof(uint16_t));
  if (rgb565Buffer == NULL) {
    LOG_DEBUG("Failed to allocate RGB565 buffer");
    free(rowBuffer);
    file.close();
    return false;
  }
  
  // Allocate buffer to hold all rows in memory (so we can reorder without seeking)
  uint16_t** imageRows = (uint16_t**)malloc(height * sizeof(uint16_t*));
  if (imageRows == NULL) {
    LOG_DEBUG("Failed to allocate image rows array");
    free(rgb565Buffer);
    free(rowBuffer);
    file.close();
    return false;
  }
  
  // Allocate memory for each row
  for (int i = 0; i < height; i++) {
    imageRows[i] = (uint16_t*)malloc(width * sizeof(uint16_t));
    if (imageRows[i] == NULL) {
      LOG_DEBUG("Failed to allocate row buffer");
      // Free previously allocated rows
      for (int j = 0; j < i; j++) {
        free(imageRows[j]);
      }
      free(imageRows);
      free(rgb565Buffer);
      free(rowBuffer);
      file.close();
      return false;
    }
  }
  
  // Read all rows sequentially from file (BMP is bottom-to-top)
  for (int row = 0; row < height; row++) {
    file.read(rowBuffer, rowSize);
    
    // Convert to RGB565 and store in the row buffer
    for (int col = 0; col < width; col++) {
      uint8_t b = rowBuffer[col * 3];
      uint8_t g = rowBuffer[col * 3 + 1];
      uint8_t r = rowBuffer[col * 3 + 2];
      imageRows[row][col] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    }
  }
  
  // Now write to display in reverse order (top-to-bottom)
  tft.startWrite();
  tft.setAddrWindow(x, y, width, height);
  
  for (int row = height - 1; row >= 0; row--) {
    tft.writePixels(imageRows[row], width);
  }
  
  tft.endWrite();
  
  // Free all allocated memory
  for (int i = 0; i < height; i++) {
    free(imageRows[i]);
  }
  free(imageRows);
  free(rgb565Buffer);
  
  free(rowBuffer);
  file.close();
  
  LOG_DEBUG("Successfully displayed image: %s", filename);
  return true;
}