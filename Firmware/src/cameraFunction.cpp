/*
================================================================================
  Camera Functions - Pixart IR Camera Interface Implementation
================================================================================
  
  This module implements the gesture tracking state machine using a Pixart IR
  camera (similar to Wiimote sensor). The camera tracks up tp 4 IR points
  in 1024x768 space at ~100Hz. We use the first point to track the wand tip.
  
  State Machine Overview:
    WAITING_FOR_IR: No IR detected, LEDs off
      → READY when IR appears
    
    READY: IR detected, waiting for stillness (yellow LED)
      → Turns green when stable for READY_STILLNESS_TIME
      → RECORDING when movement exceeds MOVEMENT_THRESHOLD
      → WAITING_FOR_IR on timeout or IR loss
    
    RECORDING: Actively recording gesture (blue LED)
      → Collecting trajectory points until IR lost or timeout
      → Triggers spell matching when IR lost
      → WAITING_FOR_IR after processing
  
  Tunable Parameters (from preferences):
    - MOVEMENT_THRESHOLD: Pixels of movement to trigger recording
    - STILLNESS_THRESHOLD: Maximum drift while "still"
    - READY_STILLNESS_TIME: Milliseconds of stillness required before green
    - END_STILLNESS_TIME: Milliseconds of stillness to end gesture
    - GESTURE_TIMEOUT: Maximum milliseconds for a gesture
    - IR_LOSS_TIMEOUT: Milliseconds before IR loss is confirmed
  
  Validation Checks:
    - Minimum trajectory points (50)
    - Minimum bounding box size (100 pixels)
    - Minimum total movement distance (50 pixels)
  
  Special Spell Handling:
    - Nightlight on/off spells don't trigger sparkle effect or timeout
    - Regular spells trigger sparkle effect with 5-second timeout
  
================================================================================
*/

#include "cameraFunctions.h"
#include "glyphReader.h"
#include "led_control.h"
#include "preferenceFunctions.h"
#include "spell_matching.h"
#include "spell_patterns.h"
#include "wifiFunctions.h"
#include "screenFunctions.h"
#include "audioFunctions.h"
#include "customSpellFunctions.h"

#include <vector>
#include <cmath>

//=====================================
// Configuration Constants
//=====================================

/**
 * Maximum number of points to store in trajectory 
 * Prevents memory overflow during long gestures.
 * Oldest points are discarded when limit is reached.
 */
#define MAX_TRAJECTORY_POINTS 1000

/**
 * No movement timeout (milliseconds)
 * Time without movement before ending gesture recording
 */
#define NO_MOVEMENT_TIMEOUT 500

/**
 * Minimum bounding box size for valid spell (pixels)
 * Gestures with smaller bounding boxes are rejected as "too small".
 * Prevents accidental triggers from tiny movements or jitter.
 */
#define MIN_BOUNDING_BOX_SIZE 200

/**
 * Tracking Point Jump Threshold (pixels)
 * If the tracked IR point jumps more than this distance between
 * frames, it is considered an invalid reading and ignored.
 * Helps filter out spurious readings from camera noise.
 */
#define POINT_JUMP_THRESHOLD 40

//=====================================
// Gesture State Machine
//=====================================

/**
 * Gesture detection states
 * State transitions are driven by IR presence, movement, and timing.
 */
enum GestureState {
  WAITING_FOR_IR,      ///< No IR point detected, waiting for wand
  READY,               ///< IR point detected and stable (yellow → green LED)
  RECORDING            ///< Movement detected, recording trajectory (blue LED)
};

/// Current state of the gesture detection state machine
GestureState currentState = WAITING_FOR_IR;

/**
 * Check if camera is actively tracking (IR detected)
 * Used by main loop to determine camera polling rate.
 * Fast polling (100Hz) when tracking, slow polling (20Hz) when idle.
 */
bool isTrackingActive() {
  return (currentState == READY || currentState == RECORDING);
}

//=====================================
// Tracking Variables
//=====================================

/// Recorded trajectory points for current gesture
std::vector<Point> currentTrajectory;

/// Timestamp of last significant movement (for end-of-gesture detection)
uint32_t lastMovementTime = 0;

/// Timestamp when stillness period started (for READY state timing)
uint32_t stillnessStartTime = 0;

/// Timestamp when IR was lost (for debouncing IR loss)
uint32_t irLostTime = 0;

/// Flag: has significant movement occurred during RECORDING state
bool hasMovedDuringRecording = false;

/// Flag: has stillness been achieved and ready to start tracking on next movement
bool readyToTrack = false;

/// Last valid IR position (for movement detection)
int lastX = -1;
int lastY = -1;

/// Stable "ready" position (center of stillness region)
Point stablePosition = {-1, -1, 0};

//=====================================
// Validation Functions
//=====================================

/**
 * Check if trajectory has minimum bounding box size
 * Calculates the bounding box of the trajectory and ensures it meets
 * the minimum size requirement in at least one dimension.
 * return true if bounding box is large enough, false otherwise
 */
bool hasMinimumMovement(const std::vector<Point>& trajectory) {
  if (trajectory.size() < 2) return false;
  
  // Find bounding box
  int minX = trajectory[0].x;
  int maxX = trajectory[0].x;
  int minY = trajectory[0].y;
  int maxY = trajectory[0].y;
  
  for (const auto& p : trajectory) {
    if (p.x < minX) minX = p.x;
    if (p.x > maxX) maxX = p.x;
    if (p.y < minY) minY = p.y;
    if (p.y > maxY) maxY = p.y;
  }
  
  int width = maxX - minX;
  int height = maxY - minY;
  
  Serial.printf("Trajectory bounding box: %dx%d pixels\n", width, height);
  
  // Require minimum size in at least one dimension
  return (width >= MIN_BOUNDING_BOX_SIZE || height >= MIN_BOUNDING_BOX_SIZE);
}

//=====================================
// Pixart Camera I2C Communication
//=====================================

/**
 * Pixart IR camera I2C address
 * Standard address for Wiimote-style Pixart cameras.
 */
#define PIXART_ADDR 0x58

/**
 * Camera register addresses
 */
#define REG_PART_ID 0x00       ///< Part ID register (device identification)
#define REG_FRAME_START 0x36   ///< Frame data start register (IR blob data)

/**
 * Write a single byte to camera register
 * return true if successful, false on I2C error
 */
bool writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(PIXART_ADDR);
  Wire.write(reg);
  Wire.write(value);
  uint8_t error = Wire.endTransmission();
  
  if (error != 0) {
    LOG_ALWAYS("I2C Write Error: %d", error);
    return false;
  }
  return true;
}

/**
 * Read a single byte from camera register
 * return Register value, or 0xFF on error
 */
uint8_t readRegister(uint8_t reg) {
  Wire.beginTransmission(PIXART_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false); // Repeated start
  
  Wire.requestFrom(PIXART_ADDR, 1);
  if (Wire.available()) {
    return Wire.read();
  }
  return 0xFF; // Error value
}

/**
 * Read multiple bytes starting from a register
 * 
 * reg: Starting register address
 * buffer: Buffer to store read data
 * length: Number of bytes to read
 */
void readRegisters(uint8_t reg, uint8_t* buffer, uint8_t length) {
  // Initialize buffer to 0
  memset(buffer, 0, length);
  
  Wire.beginTransmission(PIXART_ADDR);
  Wire.write(reg);
  uint8_t error = Wire.endTransmission(false); // Repeated start
  
  if (error != 0) {
    LOG_ALWAYS("[Read] I2C Write Error: %d\n", error);
    return;
  }
  
  delay(1); // Small delay for camera to prepare data
  
  uint8_t bytesRead = Wire.requestFrom((uint8_t)PIXART_ADDR, (uint8_t)length);
  for (uint8_t i = 0; i < bytesRead && Wire.available(); i++) {
    buffer[i] = Wire.read();
  }
}

//=====================================
// Camera Initialization
//=====================================

/**
 * Initialize the Pixart IR camera
 * Sends initialization sequence to configure camera for IR tracking mode.
 * Initialization Steps:
 *   1. Check I2C communication with camera
 *   2. Send 6-step configuration sequence
 *   3. Each step writes specific values to control registers
 * return true if camera initialized successfully, false on I2C error
 */
bool initCamera() {
  LOG_DEBUG("\n=== Initializing Pixart IR Camera ===");
  
  //-----------------------------------
  // Step 0: Check Camera Presence
  //-----------------------------------
  // Verify device is present on I2C bus
  Wire.beginTransmission(PIXART_ADDR);
  uint8_t error = Wire.endTransmission();
  
  if (error != 0) {
    LOG_ALWAYS("Camera not found at address 0x%02X (error: %d)\n", PIXART_ADDR, error);
    return false;
  }
  
  LOG_DEBUG("Camera detected at address 0x%02X\n", PIXART_ADDR);
  
  //-----------------------------------
  // Initialization Sequence
  //-----------------------------------
  // Based on Japanese Pixart documentation
  LOG_DEBUG("Sending initialization sequence...");
  
  // Step 1: Write 0x01 to register 0x30 (reset/init)
  LOG_DEBUG("Step 1: Write 0x01 to 0x30");
  writeRegister(0x30, 0x01);
  delay(10);
  
  // Step 2: Write 0x08 to register 0x30 (mode select)
  LOG_DEBUG("Step 2: Write 0x08 to 0x30");
  writeRegister(0x30, 0x08);
  delay(10);
  
  // Step 3: Write 0x90 to register 0x06 (sensitivity)
  LOG_DEBUG("Step 3: Write 0x90 to 0x06");
  writeRegister(0x06, 0x90);
  delay(10);
  
  // Step 4: Write 0xC0 to register 0x08 (gain control)
  LOG_DEBUG("Step 4: Write 0xC0 to 0x08");
  writeRegister(0x08, 0xC0);
  delay(10);
  
  // Step 5: Write 0x40 to register 0x1A (exposure time)
  LOG_DEBUG("Step 5: Write 0x40 to 0x1A");
  writeRegister(0x1A, 0x40);
  delay(10);
  
  // Step 6: Write 0x33 to register 0x33 (blob tracking mode)
  LOG_DEBUG("Step 6: Write 0x33 to 0x33");
  writeRegister(0x33, 0x33);
  delay(10);
  
  LOG_DEBUG("Camera initialized successfully!\n");
  return true;
}

//=====================================
// Main Camera Data Processing
//=====================================

/**
 * Display spell result - either debug visualization or spell name
 * 
 * Helper function to avoid code duplication for SHOW_MATCHING debug mode.
 * When SHOW_MATCHING is defined, shows side-by-side pattern comparison.
 * Otherwise, displays the spell name image.
 * 
 * bestSpell: Name of the matched spell
 * resampled: User's normalized/resampled trajectory
 * bestMatch: Similarity score (0.0 to 1.0)
 */
void displaySpellResult(const char* bestSpell, const std::vector<Point>& resampled, float bestMatch) {
#ifdef SHOW_MATCHING
  // Debug mode: show pattern comparison
  for (const auto& spell : spellPatterns) {
    if (strcasecmp(spell.name, bestSpell) == 0) {
      std::vector<Point> spellNorm = normalizeTrajectory(spell.pattern);
      std::vector<Point> spellResampled = resampleTrajectory(spellNorm, RESAMPLE_POINTS);
      visualizeMatchComparison(bestSpell, spellResampled, resampled, bestMatch);
      break;
    }
  }
#else
  // Normal mode: show spell name/image
  displaySpellName(bestSpell);
#endif
}

/**
 * Read IR blob data from camera and process gesture state machine
 * 
 * This is the main function called at ~100Hz from the main loop.
 * 
 * Workflow:
 *   1. Read 16 bytes of IR data from camera via I2C
 *   2. Parse up to 4 IR blob coordinates (we use first detected blob)
 *   3. Update gesture state machine based on IR presence and movement
 *   4. Record trajectory points during RECORDING state
 *   5. Trigger spell matching when gesture completes
 * 
 * IR Data Format (16 bytes):
 *   [header] [blob1: 3 bytes] [blob2: 3 bytes] [blob3: 3 bytes] [blob4: 3 bytes]
 *   Each blob: XX YY SS
 *     X = (SS & 0x30) << 4 | XX  (10-bit X coordinate)
 *     Y = (SS & 0xC0) << 2 | YY  (10-bit Y coordinate)
 *     Size = SS & 0x0F           (blob size 0-15)
 *   Invalid blobs: X=0x3FF, Y=0x3FF
 */
void readCameraData() {
  uint8_t data[16]; // IR camera returns 16 bytes of blob data
  
  //-----------------------------------
  // Step 1: Request IR Data via I2C
  //-----------------------------------
  // Send register address 0x36 (frame data start)
  Wire.beginTransmission(PIXART_ADDR);
  Wire.write(0x36);
  uint8_t error = Wire.endTransmission();
  
  if (error != 0) {
    return; // Silently skip on error for high-speed operation
  }
  
  // Small delay for camera to prepare data
  delayMicroseconds(200);
  
  // Read 16 bytes of IR blob data
  uint8_t bytesRead = Wire.requestFrom((uint8_t)PIXART_ADDR, (uint8_t)16);
  if (bytesRead != 16) {
    return; // Skip incomplete reads
  }
  
  for (int i = 0; i < 16 && Wire.available(); i++) {
    data[i] = Wire.read();
  }
  
  //-----------------------------------
  // Step 2: Parse IR Blob Data
  //-----------------------------------
  // Camera reports up to 4 IR blobs, we track only the first valid one (single wand)
  int currentX = -1;        // Current IR X coordinate (-1 if not detected)
  int currentY = -1;        // Current IR Y coordinate (-1 if not detected)
  uint32_t currentTime = millis();
  
  // Find the first valid IR point from the 4 possible blobs
  for (int i = 0; i < 4; i++) {
    int offset = 1 + (i * 3);  // Skip header, then 3 bytes per blob
    if (offset + 2 < 16) {
      uint8_t xx = data[offset];      // Low 8 bits of X
      uint8_t yy = data[offset + 1];  // Low 8 bits of Y
      uint8_t ss = data[offset + 2];  // High bits and size
      
      // Reconstruct 10-bit coordinates from packed format
      int x = ((ss & 0x30) << 4) | xx;  // X: 0-1023
      int y = ((ss & 0xC0) << 2) | yy;  // Y: 0-767
      
      // Check if this is a valid IR point (0x3FF = invalid/not detected)
      if (x != 0x3FF && y != 0x3FF) {
        currentX = x;
        currentY = y;
        break; // Use first detected point only
      }
    }
  }
  
  //=====================================
  // Gesture State Machine
  //=====================================
  
  if (currentX >= 0 && currentY >= 0) {
    //-----------------------------------
    // Branch: IR Point Detected
    //-----------------------------------
    
    // Calculate movement distance from last position
    float movement = 0;
    if (lastX >= 0 && lastY >= 0) {
      float dx = currentX - lastX;
      float dy = currentY - lastY;
      movement = sqrt(dx*dx + dy*dy);  // Euclidean distance in pixels
      drawIRPoint(currentX, currentY, true);  // Draw green trail on display
    }
    else {
      drawIRPoint(-1, -1, false);  // Clear any previous point
    }
    
    switch (currentState) {
      //=================================
      // WAITING_FOR_IR State
      //=================================
      case WAITING_FOR_IR: {
        // IR detected for the first time - transition to READY state
        // User needs to hold wand still for READY_STILLNESS_TIME before tracking begins
        stablePosition = {currentX, currentY, currentTime};  // Record initial position
        stillnessStartTime = currentTime;  // Start stillness timer
        currentState = READY;  // Move to READY state
        readyToTrack = false;  // Reset ready flag - user must achieve stillness first
        ledOnTime = 0;  // Cancel any active LED effect timeout (prevents old timers from interrupting tracking)
        ledSolid("yellow");  // Yellow = detected but not ready yet
        backlightOn();  // Turn on screen for visual feedback
        screenOnTime = millis();  // Reset screen timeout
        
        LOG_DEBUG("STATE: IR detected (hold still to begin)");
        break;
      }
        
      case READY: {
        // If already green (ready to track), check for movement outside boundary to start tracking
        if (readyToTrack) {
          // Check distance from stable ready position
          float dx = currentX - stablePosition.x;
          float dy = currentY - stablePosition.y;
          float distanceFromReady = sqrt(dx*dx + dy*dy);
          
          if (distanceFromReady >= MOVEMENT_THRESHOLD) {
            // Moved outside boundary - start tracking!
            currentTrajectory.clear();
            // Save the stable ready position as the first point (the actual start of the gesture)
            Point startPoint = {stablePosition.x, stablePosition.y, stablePosition.timestamp};
            currentTrajectory.push_back(startPoint);
            // Add the current position as the second point
            Point p = {currentX, currentY, currentTime};
            currentTrajectory.push_back(p);
            lastMovementTime = currentTime;
            hasMovedDuringRecording = false;
            currentState = RECORDING;
            ledSolid("blue");
            LOG_DEBUG("STATE: Tracking started from stable position (%d, %d)", stablePosition.x, stablePosition.y);
          }
          // Otherwise stay green and ready
        } else {
          // Not ready yet - waiting for stillness
          float dx = currentX - stablePosition.x;
          float dy = currentY - stablePosition.y;
          float drift = sqrt(dx*dx + dy*dy);
          
          if (drift < STILLNESS_THRESHOLD) {
            // Still stable, update stable position
            stablePosition.x = currentX;
            stablePosition.y = currentY;
            
            // Check if been still long enough to be ready to track
            if (currentTime - stillnessStartTime >= READY_STILLNESS_TIME) {
              // Stillness achieved - show green, ready to start tracking on next movement
              readyToTrack = true;
              ledSolid("green");
              showReadyBackground();
              playSound("/sounds/detected.wav");  // Play detection sound
              LOG_DEBUG("STATE: Ready to track - move wand to begin casting");
            }
          } else if (drift >= MOVEMENT_THRESHOLD) {
            // Movement before stillness achieved - reset stillness timer
            stablePosition = {currentX, currentY, currentTime};
            stillnessStartTime = currentTime;
          }
          // Small movement (between stillness and movement threshold) - stay in READY, keep yellow
        }
        
        // Check for timeout in READY state
        if (currentTime - stillnessStartTime > GESTURE_TIMEOUT) {
          Serial.println("STATE: Ready timeout");
          currentState = WAITING_FOR_IR;
          readyToTrack = false;
          if (nightlightActive) {
            ledNightlight(NIGHTLIGHT_BRIGHTNESS);
          } else {
            ledOff();
          }
          drawIRPoint(-1, -1, false);  // Clear IR point from display
        }
        break;
      }
        
      case RECORDING: {
        // Check if point is an outlier (too far from previous point - likely a reflection)
        bool isOutlier = false;
        if (currentTrajectory.size() > 0) {
          Point& lastPoint = currentTrajectory.back();
          float dx = currentX - lastPoint.x;
          float dy = currentY - lastPoint.y;
          float jumpDistance = sqrt(dx*dx + dy*dy);
          
          // Reject points that jump more than 150 pixels from previous point
          // (normal wand movement should be continuous, reflections cause large jumps)
          if (jumpDistance > POINT_JUMP_THRESHOLD) {
            isOutlier = true;
            LOG_DEBUG("Outlier rejected: jump=%.1f from (%d,%d) to (%d,%d)", 
                     jumpDistance, lastPoint.x, lastPoint.y, currentX, currentY);
          }
        }
        
        // Only add point if it's not an outlier
        if (!isOutlier) {
          Point p = {currentX, currentY, currentTime};
          currentTrajectory.push_back(p);
          
          // Limit trajectory size
          if (currentTrajectory.size() > MAX_TRAJECTORY_POINTS) {
            currentTrajectory.erase(currentTrajectory.begin());
          }
        }
        
        // Check if this is significant movement
        if (movement >= MOVEMENT_THRESHOLD) {
          // Significant movement detected - show blue LED (gesture in progress)
          lastMovementTime = currentTime;
          hasMovedDuringRecording = true;  // Mark that we've moved
          ledSolid("blue");
        }
        // If no significant movement, user might be pausing - keep recording, stay blue
        
        // Check for timeout
        if (currentTrajectory.size() > 0 && currentTime - currentTrajectory[0].timestamp > GESTURE_TIMEOUT) {
          LOG_DEBUG("STATE: Gesture timeout");
          ledSolid("red");
          currentTrajectory.clear();
          drawIRPoint(-1, -1, false);  // Clear IR point from display
          lastX = -1;
          lastY = -1;
          clearDisplay();
          delay(500);
          
          currentState = WAITING_FOR_IR;
          readyToTrack = false;
          ledOff();
          
        }
        break;
      }
    }
    
    lastX = currentX;
    lastY = currentY;
    irLostTime = 0;  // Reset IR lost timer when we have a valid point
    
  } else {
    // No IR point detected
    uint32_t currentTime = millis();
    
    // Start tracking when IR was lost
    if (irLostTime == 0) {
      irLostTime = currentTime;
    }
    
    // Only process IR loss after timeout threshold
    if (currentTime - irLostTime < IR_LOSS_TIMEOUT) {
      return;  // IR briefly lost, keep waiting
    }
    
    if (currentState == RECORDING) {
      // IR lost during recording - end of spell gesture
      LOG_DEBUG("STATE: IR lost, processing gesture...");
      ledOff();  // Turn off LEDs while processing
      
      // Check if we have minimum movement (bounding box size)
      if (!hasMinimumMovement(currentTrajectory)) {
        LOG_DEBUG("Gesture too small - insufficient movement");
        ledSolid("red");
        ledOnTime = millis();
        playSound("/sounds/error.wav");  // Play error sound
        displaySpellName("Too Small");
        currentTrajectory.clear();
        currentState = WAITING_FOR_IR;
        irLostTime = 0;
        lastX = -1;
        lastY = -1;
        return;
      }
      
      // Check if we have enough movement to constitute a gesture
      float totalDistance = 0;
      for (size_t i = 1; i < currentTrajectory.size(); i++) {
        float dx = currentTrajectory[i].x - currentTrajectory[i-1].x;
        float dy = currentTrajectory[i].y - currentTrajectory[i-1].y;
        totalDistance += sqrt(dx*dx + dy*dy);
      }
      
      if (totalDistance > 50) {  // At least 50 pixels of movement
        // Valid gesture - try to match
        LOG_DEBUG("Processing gesture (%.1f px total movement)...\n", totalDistance);
        
        // Check if trajectory has minimum points before matching
        if (currentTrajectory.size() < MIN_TRAJECTORY_POINTS) {
          LOG_DEBUG("Trajectory too short (%d points)\n", currentTrajectory.size());
          ledSolid("red");
          ledOnTime = millis();
          playSound("/sounds/error.wav");  // Play error sound
          displaySpellName("Too Short");
          currentTrajectory.clear();
          currentState = WAITING_FOR_IR;
          readyToTrack = false;
          irLostTime = 0;
          lastX = -1;
          lastY = -1;
          return;
        }
        
        // Check if we're recording a custom spell
        if (isRecordingCustomSpell) {
          // Recording mode - show preview instead of matching
          std::vector<Point> normalized = normalizeTrajectory(currentTrajectory);
          std::vector<Point> resampled = resampleTrajectory(normalized, RESAMPLE_POINTS);
          
          // Store for saving
          extern std::vector<Point> recordedSpellPattern;
          recordedSpellPattern = resampled;
          
          // Show preview
          visualizeSpellPattern("New Spell", resampled);
          
          // Display save/discard prompt
          tft.setTextSize(1);
          tft.setTextColor(0x07E0);  // Green
          tft.setCursor(100, 210);
          tft.print("BTN1:Save");
          tft.setTextColor(0xF800);  // Red
          tft.setCursor(100, 190);
          tft.print("BTN2:Discard");
          
          extern SpellRecordingState spellRecordingState;
          spellRecordingState = SPELL_RECORD_PREVIEW;
          ledOff();
          LOG_DEBUG("Spell record: Preview (%d points)", resampled.size());
          
          currentTrajectory.clear();
          currentState = WAITING_FOR_IR;
          readyToTrack = false;
          irLostTime = 0;
          lastX = -1;
          lastY = -1;
          return;
        }
        
        matchSpell(currentTrajectory);
        
        // Check if we found a match
        std::vector<Point> normalized = normalizeTrajectory(currentTrajectory);
        std::vector<Point> resampled = resampleTrajectory(normalized, RESAMPLE_POINTS);
        float bestMatch = 0;
        const char* bestSpell = "Unknown";
        
        for (const auto& spell : spellPatterns) {
          std::vector<Point> spellNorm = normalizeTrajectory(spell.pattern);
          std::vector<Point> spellResampled = resampleTrajectory(spellNorm, RESAMPLE_POINTS);
          float similarity = calculateSimilarity(resampled, spellResampled);
          if (similarity > bestMatch) {
            bestMatch = similarity;
            bestSpell = spell.name;
          }
        }
        
        if (bestMatch >= MATCH_THRESHOLD) {
          // Spell detected - check if it's a nightlight control spell
          
          
          // Check if this spell is configured for nightlight control
          bool isNightlightOnSpell = (NIGHTLIGHT_ON_SPELL.length() > 0 && strcasecmp(bestSpell, NIGHTLIGHT_ON_SPELL.c_str()) == 0);
          bool isNightlightOffSpell = (NIGHTLIGHT_OFF_SPELL.length() > 0 && strcasecmp(bestSpell, NIGHTLIGHT_OFF_SPELL.c_str()) == 0);
          
          // Check if same spell is assigned to both (toggle mode)
          bool isToggleMode = (NIGHTLIGHT_ON_SPELL.length() > 0 && 
                               NIGHTLIGHT_OFF_SPELL.length() > 0 && 
                               strcasecmp(NIGHTLIGHT_ON_SPELL.c_str(), NIGHTLIGHT_OFF_SPELL.c_str()) == 0);
          
          if (isToggleMode && (isNightlightOnSpell || isNightlightOffSpell)) {
            // Toggle mode - same spell turns on and off
            if (nightlightActive) {
              nightlightActive = false;
              ledOff();
              LOG_DEBUG("Nightlight toggled OFF");
            } else {
              ledNightlight(NIGHTLIGHT_BRIGHTNESS);
              LOG_DEBUG("Nightlight toggled ON");
            }
            // Play random spell sound (1-5)
            char soundFile[32];
            snprintf(soundFile, sizeof(soundFile), "/sounds/spell%d.wav", random(1, 6));
            playSound(soundFile);
            publishSpell(bestSpell);
          } else if (isNightlightOnSpell) {
            // Turn on nightlight mode
            ledNightlight(NIGHTLIGHT_BRIGHTNESS);
            // Play random spell sound (1-5)
            char soundFile[32];
            snprintf(soundFile, sizeof(soundFile), "/sounds/spell%d.wav", random(1, 6));
            playSound(soundFile);
            displaySpellResult(bestSpell, resampled, bestMatch);
            publishSpell(bestSpell);
            LOG_DEBUG("Nightlight turned ON");
          } else if (isNightlightOffSpell) {
            // Turn off nightlight mode
            nightlightActive = false;
            ledOff();
            // Play random spell sound (1-5)
            char soundFile[32];
            snprintf(soundFile, sizeof(soundFile), "/sounds/spell%d.wav", random(1, 6));
            playSound(soundFile);
            displaySpellResult(bestSpell, resampled, bestMatch);
            publishSpell(bestSpell);
            ledOnTime = 0;
            LOG_DEBUG("Nightlight turned OFF");
          } else if (nightlightActive && 
                     ((NIGHTLIGHT_RAISE_SPELL.length() > 0 && strcasecmp(bestSpell, NIGHTLIGHT_RAISE_SPELL.c_str()) == 0) || 
                      (NIGHTLIGHT_LOWER_SPELL.length() > 0 && strcasecmp(bestSpell, NIGHTLIGHT_LOWER_SPELL.c_str()) == 0))) {
            // Nightlight brightness adjustment - Raise/Lower spells
            bool isRaise = (NIGHTLIGHT_RAISE_SPELL.length() > 0 && strcasecmp(bestSpell, NIGHTLIGHT_RAISE_SPELL.c_str()) == 0);
            
            if (isRaise) {
              NIGHTLIGHT_BRIGHTNESS = constrain(NIGHTLIGHT_BRIGHTNESS + 50, 10, 255);
              LOG_DEBUG("Nightlight brightness increased to %d", NIGHTLIGHT_BRIGHTNESS);
            } else {
              NIGHTLIGHT_BRIGHTNESS = constrain(NIGHTLIGHT_BRIGHTNESS - 50, 10, 255);
              LOG_DEBUG("Nightlight brightness decreased to %d", NIGHTLIGHT_BRIGHTNESS);
            }
            
            // Save new brightness to preferences
            setPref(PrefKey::NIGHTLIGHT_BRIGHTNESS, NIGHTLIGHT_BRIGHTNESS);
            
            // Apply new brightness immediately
            ledNightlight(NIGHTLIGHT_BRIGHTNESS);
            
            // Play random spell sound (1-5)
            char soundFile[32];
            snprintf(soundFile, sizeof(soundFile), "/sounds/spell%d.wav", random(1, 6));
            playSound(soundFile);
            
            // Show spell feedback
            displaySpellResult(bestSpell, resampled, bestMatch);
            publishSpell(bestSpell);
          } else {
            // Regular spell - publish to MQTT and show random effect
            // Play random spell sound (1-5)
            char soundFile[32];
            snprintf(soundFile, sizeof(soundFile), "/sounds/spell%d.wav", random(1, 6));
            playSound(soundFile);
            publishSpell(bestSpell);
            displaySpellResult(bestSpell, resampled, bestMatch);
            ledRandomEffect();  // Pick a random LED effect for variety
            ledOnTime = millis();  // Start LED effect timer
          }
        } else {
          // No match - blink red
          displaySpellName("No Match");
          ledSolid("red");
          ledOnTime = millis();  // Start LED effect timer
          playSound("/sounds/error.wav");  // Play error sound
        }
      } else {
        // Not enough movement - blink red
        LOG_DEBUG("Insufficient movement (%.1f px)\n", totalDistance);
        ledSolid("red");
        ledOnTime = millis();  // Start LED effect timer
        playSound("/sounds/error.wav");  // Play error sound
        displaySpellName("No Match");
      }
      
      currentTrajectory.clear();
      currentState = WAITING_FOR_IR;
      readyToTrack = false;
      irLostTime = 0;
      lastX = -1;
      lastY = -1;
      LOG_DEBUG("STATE: Waiting for next gesture");
      
    } else if (currentState != WAITING_FOR_IR) {
      // IR lost in READY state - reset
      LOG_DEBUG("STATE: IR lost before spell started");
      currentState = WAITING_FOR_IR;
      currentTrajectory.clear();
      readyToTrack = false;
      if (nightlightActive) {
        ledNightlight(NIGHTLIGHT_BRIGHTNESS);
      } else {
        ledOff();
      }
      // Clear trail (idle background will be restored after any displayed message times out)
      clearDisplay();
      lastX = -1;
      lastY = -1;
      irLostTime = 0;
    }
  }
  
//=================================
// Debuggung output of IR points
//=================================
/**
 * Outputs the current IR blob data to Serial for debugging.
 * Use the visualise_ir.py script from the Tools folder to read
 * and visualize the output.
 */
#ifdef OUTPUT_POINTS
  Serial.print("IR,");
  Serial.print(currentTime);
  
  for (int i = 0; i < 4; i++) {
    int offset = 1 + (i * 3);
    if (offset + 2 < 16) {
      uint8_t xx = data[offset];
      uint8_t yy = data[offset + 1];
      uint8_t ss = data[offset + 2];
      
      int x = ((ss & 0x30) << 4) | xx;
      int y = ((ss & 0xC0) << 2) | yy;
      int size = ss & 0x0F;
      
      if (x == 0x3FF || y == 0x3FF) {
        Serial.print(",-1,-1,-1");
      } else {
        Serial.printf(",%d,%d,%d", x, y, size);
      }
    }
  }
  Serial.println();
#endif
}

// Helper function to get current IR position for spell recording
// Returns true if valid IR point is detected, false otherwise
// Outputs: x and y coordinates in display space (0-239)
bool getIRPosition(int& x, int& y) {
  // Read IR data from camera
  Wire.beginTransmission(PIXART_ADDR);
  Wire.write(0x36);  // Register for IR tracking data
  if (Wire.endTransmission() != 0) {
    return false;  // I2C error
  }
  
  Wire.requestFrom(PIXART_ADDR, 16);
  if (Wire.available() < 16) {
    return false;  // Not enough data
  }
  
  // Read first blob (most prominent IR source)
  byte data[16];
  for (int i = 0; i < 16; i++) {
    data[i] = Wire.read();
  }
  
  // Parse coordinates from first blob
  int xx = data[1];
  int yy = data[2];
  byte ss = data[3];
  
  int rawX = ((ss & 0x30) << 4) | xx;
  int rawY = ((ss & 0xC0) << 2) | yy;
  
  // Check for invalid coordinates
  if (rawX == 0x3FF || rawY == 0x3FF) {
    return false;  // No IR source detected
  }
  
  // Convert camera coordinates (0-1023) to display coordinates (0-239)
  x = map(rawX, 0, 1023, 0, 239);
  y = map(rawY, 0, 1023, 0, 239);
  
  return true;  // Valid position detected
}