/**
================================================================================
  Screen Functions - GC9A01A Round LCD Display Implementation
================================================================================
 * 
 * This module manages the 240x240 pixel round LCD display for real-time gesture
 * visualization, spell name display, and visual feedback during wand tracking.
 * 
 * HARDWARE:
 * - Display: GC9A01A 240x240 round LCD (1.28" diameter)
 * - Interface: SPI (separate bus from SD card)
 * - Backlight: GPIO controlled PWM (manual on/off, no dimming currently)
 * 
 * COORDINATE SYSTEMS:
 * - Camera: 0-1023 x 0-1023 (Pixart IR sensor raw coordinates)
 * - Display: 0-239 x 0-239 (LCD pixel coordinates)
 * - Pattern: 0-1000 x 0-1000 (normalized spell pattern space)
 * All coordinate mappings use Arduino map() function for scaling.
 * 
 * POWER MANAGEMENT:
 * - Backlight timeout controlled by global screenOnTime variable
 * 
 * IMAGE SUPPORT:
 * - Loads 24-bit BMP files from SD card for custom spell images
 * - Converts BMP (BGR) to RGB565 display format on-the-fly
 * - Handles bottom-to-top BMP row order reversal
 * - Falls back to text display if image load fails
 */

#include "screenFunctions.h"
#include "glyphReader.h"
#include "sdFunctions.h"
#include "spell_patterns.h"

// Create display instance
Adafruit_GC9A01A tft(TFT_CS, TFT_DC, TFT_RST);

// Track last IR point position for drawing connected trail lines
static int lastIRX = -1;  // -1 indicates no previous point
static int lastIRY = -1;

/**
 * Initialize GC9A01A round display and backlight
 * Sets up SPI bus, initializes display controller, configures backlight GPIO,
 * and displays a startup animation (random color flash + reference circle).
 * 
 * INITIALIZATION SEQUENCE:
 * 1. Configure SPI bus with correct pins (separate bus from SD card)
 * 2. Call tft.begin() to initialize GC9A01A controller
 * 3. Set up backlight GPIO as output, turn on
 * 4. Set rotation to 0 (portrait, can be 0-3 for different orientations)
 * 5. Flash random color briefly (visual confirmation of successful init)
 * 6. Draw dark gray reference circle at display edge
 * BACKLIGHT CONTROL:
 * - TFT_BL GPIO set HIGH to enable backlight
 * - Manual on/off control (no PWM dimming implemented)
 * - Power management handled by backlightOn/Off() functions
 * Called once during setup() in main.cpp.
 */
void screenInit() {
  LOG_DEBUG("Initializing GC9A01 Display...");
  
  // Initialize SPI bus (clock, MISO=-1 for write-only, MOSI, CS)
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  
  // Initialize the display controller
  LOG_DEBUG("Calling tft.begin()...");
  tft.begin();  // Sends initialization commands to GC9A01A
  LOG_DEBUG("Display initialized!");
  
  delay(100);  // Allow controller to stabilize
  
  // Configure Backlight pin for manual on/off control
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);  // Turn on backlight
  LOG_DEBUG("Backlight pin %d set to HIGH", TFT_BL);
  delay(100);
  
  tft.setRotation(0);  // 0-3 for different orientations (0=portrait)
  
  // Fill screen with random color as visual confirmation (using ESP32 hardware RNG)
  uint16_t randomColor = esp_random() & 0xFFFF;  // Random RGB565 color
  tft.fillScreen(randomColor);
  delay(500);  // Show random color briefly for visual feedback
  
  // Draw outer circle as reference frame for round 240x240 display
  tft.drawCircle(120, 120, 119, 0x4208);  // Center at (120,120), radius 119, dark gray
  
  LOG_DEBUG("Display initialized successfully!");
}

/**
 * Display setup progress messages during boot
 * line: Line number (0-based) for vertical positioning
 * function: Name of subsystem being initialized
 * status: Status string: "init" for in-progress, or result (e.g., "OK", "FAIL")
 * Allows user to see progress and identify failures during boot.
 * USAGE EXAMPLE:
 *   updateSetupDisplay(0, "WiFi", "init");     // Shows "WiFi..."
 *   updateSetupDisplay(0, "WiFi", "OK");       // Shows "WiFi... OK"
 * Called from main.cpp setup() for each initialization step.
 */
void updateSetupDisplay(int line, const String &function, const String &status) {
    // Calculate cursor position based on line number
    tft.setTextSize(1);  // Small text for compact status display
    tft.setTextColor(0xFFFF);  // White color for good contrast
    tft.setCursor(50, line * 10 + 40);  // Vertical spacing, offset from top
    
    if (status == "init") {
      tft.print(function + "...");  // In-progress indicator
    } else {
      tft.print(function + "... " + status);  // Show result (OK/FAIL/etc)
    }
}

/**
 * Draw IR tracking point on display with connected trail
 * x: Camera X coordinate (0-1023 from Pixart IR sensor)
 * y: Camera Y coordinate (0-1023 from Pixart IR sensor)
 * isActive: True if IR point is currently detected, false otherwise
 * Visualizes real-time wand position during gesture recording.
 * Draws green connecting lines between points to show trajectory.
 * COORDINATE MAPPING:
 * - Input: 0-1023 (camera coordinates)
 * - Output: 0-239 (display coordinates)
 * - Uses Arduino map() function for scaling
 * CLEARING BEHAVIOR:
 * - When isActive=false, erases last point (fills with black)
 * - Resets tracking state to allow fresh trail on next gesture
 * Called from main loop during RECORDING state
 */
void drawIRPoint(int x, int y, bool isActive) {
  if (isActive && x >= 0 && y >= 0) {
    // Scale from camera coordinates (0-1023) to display (0-239)
    int displayX = map(x, 0, 1023, 0, 239);
    int displayY = map(y, 0, 1023, 0, 239);
    
    // Draw trail line from previous point to current point
    if (lastIRX >= 0 && lastIRY >= 0) {
      tft.drawLine(lastIRX, lastIRY, displayX, displayY, 0x07E0);  // Green trail
    }
    
    // Draw current point with two-color style for visibility
    tft.fillCircle(displayX, displayY, 5, 0xFFE0);  // Yellow center (bright)
    tft.drawCircle(displayX, displayY, 6, 0xF800);  // Red outline (contrast)
    
    // Update last position for next frame's trail line
    lastIRX = displayX;
    lastIRY = displayY;
  } else {
    // No active IR point - clear last point if it exists
    if (lastIRX >= 0 && lastIRY >= 0) {
      tft.fillCircle(lastIRX, lastIRY, 6, 0x0000);  // Black (erase)
    }
    lastIRX = -1;  // Reset state
    lastIRY = -1;
  }
}

/**
 * Display recognized spell name (image or text)
 * spellName: Null-terminated string containing spell name (e.g., "Illuminate")
 * Shows spell on display when match is found. Tries to load BMP image from SD
 * card first, falls back to centered text if no image or load fails.
 * IMAGE MODE:
 * - Checks hasSpellImage() to see if spell has associated BMP file
 * - Gets filename from getSpellImageFilename() (custom or default naming)
 * - Displays 240x240 BMP centered at (0,0) if load successful
 * - Falls back to text mode if image load fails
 * TEXT MODE:
 * - Purple decorative double circle (radius 110 and 105)
 * - Cyan text (0x07FF), size 3 for readability
 * - Multi-word spells split into two centered lines (10px gap)
 * - Single-word spells centered vertically and horizontally
 * - Uses getTextBounds() for precise centering
 * TIMEOUT MANAGEMENT:
 * - Sets screenSpellOnTime = millis() to start display timeout
 * - Sets screenOnTime = millis() to keep backlight on
 * - Display cleared after timeout (handled in main loop)
 */
void displaySpellName(const char* spellName) {
  // Clear screen to black
  tft.fillScreen(0x0000);
  
  // Check if there's a BMP image for this spell on SD card
  if (hasSpellImage(spellName)) {
    // Get the appropriate filename (custom or default naming convention)
    String filename = getSpellImageFilename(spellName);
    
    Serial.printf("Displaying image for spell: %s\n", filename.c_str());
    
    // Try to display the image centered on screen (0, 0 for 240x240 image)
    if (displayImageFromSD(filename.c_str(), 0, 0)) {
      // Image displayed successfully - set timeouts and return
      screenSpellOnTime = millis();
      screenOnTime = millis();
      return;
    } else {
      // Image failed to load, fall through to text display
      Serial.println("Failed to load image, falling back to text");
      tft.fillScreen(0x0000);  // Clear any partial image artifacts
    }
  }
  
  // TEXT MODE: Draw decorative circle frame
  tft.drawCircle(120, 120, 110, 0x780F);  // Purple outer circle
  tft.drawCircle(120, 120, 105, 0x780F);  // Purple inner circle (double border)
  
  tft.setTextSize(3);  // Large text for readability
  tft.setTextColor(0x07FF);  // Cyan color for good contrast
  
  // Check if spell name has a space (indicates multi-word spell)
  String spell = String(spellName);
  int spaceIndex = spell.indexOf(' ');
  
  if (spaceIndex > 0) {
    // Multi-word spell - split into two lines for better fit
    String firstWord = spell.substring(0, spaceIndex);
    String secondWord = spell.substring(spaceIndex + 1);
    
    // Get text bounds for each word to calculate centering
    int16_t x1, y1;
    uint16_t w1, h1, w2, h2;
    tft.getTextBounds(firstWord.c_str(), 0, 0, &x1, &y1, &w1, &h1);
    tft.getTextBounds(secondWord.c_str(), 0, 0, &x1, &y1, &w2, &h2);
    
    // Calculate vertical spacing (total height of both lines + 10px gap)
    int totalHeight = h1 + h2 + 10;  // 10 pixel gap between lines
    int startY = (240 - totalHeight) / 2;  // Center vertically
    
    // Display first word centered horizontally
    int centerX1 = (240 - w1) / 2;
    tft.setCursor(centerX1, startY);
    tft.println(firstWord);
    
    // Display second word centered below first
    int centerX2 = (240 - w2) / 2;
    tft.setCursor(centerX2, startY + h1 + 10);
    tft.println(secondWord);
    
  } else {
    // Single word - center it both horizontally and vertically
    int16_t x1, y1;
    uint16_t w, h;
    tft.getTextBounds(spellName, 0, 0, &x1, &y1, &w, &h);
    
    int centerX = (240 - w) / 2;  // Horizontal center
    int centerY = (240 - h) / 2;  // Vertical center
    
    tft.setCursor(centerX, centerY);
    tft.println(spellName);
  }
  
  // Set timeout timers to keep display on for spell viewing
  screenSpellOnTime = millis();  // Spell-specific timeout
  screenOnTime = millis();  // Reset overall screen timeout as well
}

/**
 * Clear display and redraw reference circle
 * Resets display to idle state: black background with dark gray border circle.
 * Also resets IR tracking state to start fresh trail on next gesture.
 * Called when:
 * - Spell display timeout expires
 * - Transitioning from RECORDING back to WAITING_FOR_IR state
 * - User manually clears display
 */
void clearDisplay() {
  tft.fillScreen(0x0000);  // Fill entire display with black
  tft.drawCircle(120, 120, 119, 0x4208);  // Redraw dark gray border circle
  lastIRX = -1;  // Reset IR trail tracking
  lastIRY = -1;
}

/**
 * Turn off display backlight to save power
 * Sets backlight GPIO LOW to disable LED backlight.
 */
void backlightOff() {
    digitalWrite(TFT_BL, HIGH);  // Turn off backlight LED
    LOG_DEBUG("Backlight turned OFF (pin %d set to LOW)", TFT_BL);
    backlightStateOn = false;  // Update global state flag
}

/**
 * Turn on display backlight
 * Sets backlight GPIO HIGH to enable LED backlight.
 */
void backlightOn() {
    digitalWrite(TFT_BL, LOW);  // Turn on backlight LED
    LOG_DEBUG("Backlight turned ON (pin %d set to HIGH)", TFT_BL);
    backlightStateOn = true;  // Update global state flag
}

/**
 * Load and display BMP image from SD card
 * filename: Path to BMP file on SD card (e.g., "/lumos.bmp")
 * x: X coordinate for top-left corner of image on display
 * y: Y coordinate for top-left corner of image on display
 * return true if image displayed successfully, false on error
 * Loads 24-bit uncompressed BMP files from SD card and displays on screen.
 * Handles BMP format quirks: BGR color order, bottom-to-top row storage,
 * and 4-byte row padding.
 * SUPPORTED BMP FORMAT:
 * - Bit depth: 24-bit RGB (8 bits per channel)
 * - Compression: Uncompressed (BI_RGB)
 * - Color order: BGR (Blue-Green-Red)
 * - Row order: Bottom-to-top (BMP standard)
 * - Row padding: Aligned to 4-byte boundaries
 * MEMORY MANAGEMENT:
 * - Allocates heap memory for all rows to avoid SD card seeking
 * - Row buffers: width * sizeof(uint16_t) per row
 * - Total allocation: width * height * 2 bytes (RGB565 format)
 * - Frees all memory before function returns
 * CONVERSION PROCESS:
 * 1. Read BMP header to get width, height, bit depth
 * 2. Validate format (24-bit, uncompressed only)
 * 3. Allocate memory for all rows in RGB565 format
 * 4. Read rows sequentially from file (bottom-to-top in BMP)
 * 5. Convert each pixel from BGR888 to RGB565
 * 6. Write rows to display in reverse order (top-to-bottom)
 * 7. Free all allocated memory 
 * RGB565 CONVERSION:
 * - Red: Top 5 bits of R channel (R & 0xF8) << 8
 * - Green: Top 6 bits of G channel (G & 0xFC) << 3
 * - Blue: Top 5 bits of B channel (B >> 3)
 * USAGE:
 * - Spell images: 240x240 BMP files for full-screen spell display
 * - Custom graphics: Any size BMP can be positioned anywhere on screen
 * 
 * Called from displaySpellName() when spell has associated image file.
 */
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
  
  // Read all rows sequentially from file (BMP stores bottom-to-top)
  for (int row = 0; row < height; row++) {
    file.read(rowBuffer, rowSize);  // Read one row (with padding)
    
    // Convert each pixel from BGR888 to RGB565 and store in row buffer
    for (int col = 0; col < width; col++) {
      uint8_t b = rowBuffer[col * 3];      // Blue channel
      uint8_t g = rowBuffer[col * 3 + 1];  // Green channel
      uint8_t r = rowBuffer[col * 3 + 2];  // Red channel
      // Pack into RGB565: RRRRR GGGGGG BBBBB
      imageRows[row][col] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    }
  }
  
  // Write to display in reverse order (convert BMP bottom-to-top to display top-to-bottom)
  tft.startWrite();  // Begin SPI transaction for bulk write
  tft.setAddrWindow(x, y, width, height);  // Set drawing window
  
  for (int row = height - 1; row >= 0; row--) {  // Reverse row order
    tft.writePixels(imageRows[row], width);  // Write entire row at once
  }
  
  tft.endWrite();  // End SPI transaction
  
  // Free all allocated memory (row arrays, pointer array, temp buffers)
  for (int i = 0; i < height; i++) {
    free(imageRows[i]);  // Free each row buffer
  }
  free(imageRows);  // Free row pointer array
  free(rgb565Buffer);  // Free temporary row buffer
  
  free(rowBuffer);  // Free BMP row buffer
  file.close();  // Close SD file
  
  LOG_DEBUG("Successfully displayed image: %s", filename);
  return true;
}

/**
 * Visualize spell pattern on display for debugging
 * name: Spell name to display at top
 * pattern: Vector of normalized Point coordinates (0-1000 range)
 * Debug visualization tool to verify spell pattern definitions are correct.
 * Useful for validating custom spell patterns from spells.json.
 * Called from spell_patterns.cpp initialization to preview patterns.
 */
void visualizeSpellPattern(const char* name, const std::vector<Point>& pattern) {
  if (pattern.empty()) return;  // Nothing to draw
  
  // Clear screen and draw border circle
  tft.fillScreen(0x0000);
  tft.drawCircle(120, 120, 119, 0x4208);  // Dark gray reference circle
  
  // Draw spell name centered at top
  tft.setTextSize(2);
  tft.setTextColor(0xFFFF);  // White text
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(name, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor((240 - w) / 2, 10);  // Center horizontally, 10px from top
  tft.println(name);
  
  // Find bounding box of pattern (patterns should already be 0-1000 normalized)
  // Find bounding box of pattern (patterns should already be 0-1000 normalized)
  int minX = pattern[0].x, maxX = pattern[0].x;
  int minY = pattern[0].y, maxY = pattern[0].y;
  for (const auto& p : pattern) {
    minX = min(minX, p.x);
    maxX = max(maxX, p.x);
    minY = min(minY, p.y);
    maxY = max(maxY, p.y);
  }
  
  // Scale pattern to fit in display with margins
  int displayMargin = 40;  // 40px margin on all sides
  int displaySize = 240 - (displayMargin * 2);  // 160px drawing area
  
  // Draw pattern point by point with connecting lines (animated)
  for (size_t i = 0; i < pattern.size(); i++) {
    // Scale from 0-1000 normalized space to display space (40-200)
    int displayX = map(pattern[i].x, 0, 1000, displayMargin, 240 - displayMargin);
    int displayY = map(pattern[i].y, 0, 1000, displayMargin, 240 - displayMargin);
    
    // Draw connecting line from previous point
    if (i > 0) {
      int prevX = map(pattern[i-1].x, 0, 1000, displayMargin, 240 - displayMargin);
      int prevY = map(pattern[i-1].y, 0, 1000, displayMargin, 240 - displayMargin);
      tft.drawLine(prevX, prevY, displayX, displayY, 0x07E0);  // Green trail line
    }
    
    // Draw point with different colors for start/end/middle
    if (i == 0) {
      // Starting point - larger red circle
      tft.fillCircle(displayX, displayY, 4, 0xF800);  // Red start marker
    } else if (i == pattern.size() - 1) {
      // Ending point - blue circle
      tft.fillCircle(displayX, displayY, 3, 0x001F);  // Blue end marker
    } else {
      // Middle points - small yellow circles
      tft.fillCircle(displayX, displayY, 2, 0xFFE0);  // Yellow trajectory points
    }
    
    delay(30);  // Small delay to animate the drawing (30ms per point)
  }
  
  delay(1200);  // Hold final pattern for 1.2 seconds so user can see it
}

//=====================================
// Settings Menu Display
//=====================================

/**
 * Display settings menu on screen
 * Shows the current setting name and value with visual indicators.
 * Displays:
 * - Setting name at top
 * - Current value in center (large text)
 * - Navigation indicators (arrows/brackets)
 * - Edit mode indicator
 * 
 * settingIndex: Which setting to display (0 = NL ON, 1 = NL OFF)
 * valueIndex: Which value option is selected (0 = Disabled, 1+ = spell index)
 * isEditing: True if currently editing value, false if browsing settings
 */
void displaySettingsMenu(int settingIndex, int valueIndex, bool isEditing) {
    // Clear display
    tft.fillScreen(0x0000);  // Black background
    
    // Define setting names
    const char* settingNames[] = {
        "NL ON Spell",
        "NL OFF Spell"
    };
    
    // Get value name
    String valueName;
    if (valueIndex == 0) {
        valueName = "Disabled";
    } else if (valueIndex > 0 && valueIndex <= (int)spellPatterns.size()) {
        valueName = spellPatterns[valueIndex - 1].name;
    } else {
        valueName = "Error";
    }
    
    // Display setting name at top (centered)
    tft.setTextColor(0xFFFF);  // White text
    tft.setTextSize(2);
    
    if (settingIndex >= 0 && settingIndex < 2) {
        String settingNameStr = settingNames[settingIndex];
        int nameWidth = settingNameStr.length() * 12;  // Size 2: ~12px per char
        int nameCenterX = (240 - nameWidth) / 2;
        tft.setCursor(nameCenterX, 30);
        tft.print(settingNameStr);
    }
    
    // Display navigation/edit indicator (centered)
    tft.setTextSize(1);
    if (isEditing) {
        // Editing mode - show brackets around value
        tft.setTextColor(0x07E0);  // Green for edit mode
        String editText = "[ EDITING ]";
        int editWidth = editText.length() * 6;  // Size 1: ~6px per char
        int editCenterX = (240 - editWidth) / 2;
        tft.setCursor(editCenterX, 60);
        tft.print(editText);
    } else {
        // Browse mode - show navigation hint
        tft.setTextColor(0xFFE0);  // Yellow for browse mode
        String browseText = "< BROWSE >";
        int browseWidth = browseText.length() * 6;  // Size 1: ~6px per char
        int browseCenterX = (240 - browseWidth) / 2;
        tft.setCursor(browseCenterX, 60);
        tft.print(browseText);
    }
    
    // Display current value in center (large text)
    tft.setTextColor(0xFFFF);  // White text
    tft.setTextSize(2);
    
    // Calculate center position for value text
    int textWidth = valueName.length() * 12;  // Size 2: ~12px per char
    int centerX = (240 - textWidth) / 2;
    tft.setCursor(centerX, 120);
    tft.print(valueName);
    
    // Display instructions at bottom (centered)
    tft.setTextSize(1);
    tft.setTextColor(0x7BEF);  // Light gray
    
    String instruction1;
    if (isEditing) {
        instruction1 = "BTN2:Cycle BTN1:Save";
    } else {
        instruction1 = "BTN2:Next BTN1:Edit";
    }
    int inst1Width = instruction1.length() * 6;  // Size 1: ~6px per char
    int inst1CenterX = (240 - inst1Width) / 2;
    tft.setCursor(inst1CenterX, 200);
    tft.print(instruction1);
    
    // Show long-press hint (centered)
    String instruction2 = "Hold BTN2: Exit";
    int inst2Width = instruction2.length() * 6;  // Size 1: ~6px per char
    int inst2CenterX = (240 - inst2Width) / 2;
    tft.setCursor(inst2CenterX, 215);
    tft.print(instruction2);
    
    // Update screen timestamp
    screenOnTime = millis();
}
