/*
Functions related to the IR Camera
*/

#include "cameraFunctions.h"
#include "wandCommander.h"
#include "led_control.h"
#include "preferenceFunctions.h"
#include "spell_matching.h"
#include "wifiFunctions.h"
#include "screenFunctions.h"

#include <vector>
#include <cmath>

#define MAX_TRAJECTORY_POINTS 1000

// Gesture detection states
enum GestureState {
  WAITING_FOR_IR,      // No IR point detected
  READY,               // IR point detected and still (yellow LED)
  RECORDING            // Movement detected, recording trajectory (blue LED)
};

GestureState currentState = WAITING_FOR_IR;


// Trajectory tracking
std::vector<Point> currentTrajectory;
uint32_t lastMovementTime = 0;
uint32_t stillnessStartTime = 0;
uint32_t irLostTime = 0;  // Track when IR was lost
bool hasMovedDuringRecording = false;  // Track if movement detected in RECORDING state
bool readyToTrack = false;  // Track if stillness achieved and ready to start on movement
int lastX = -1;
int lastY = -1;
Point stablePosition = {-1, -1, 0};

// Minimum bounding box size for valid spell (avoids matching tiny movements)
#define MIN_BOUNDING_BOX_SIZE 100  // Minimum width or height in pixels

// Calculate bounding box of trajectory and check if it meets minimum size
bool hasMinimumMovement(const std::vector<Point>& trajectory) {
  if (trajectory.size() < 2) return false;
  
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

// Pixart IR Camera I2C Address (typical for Wiimote camera)
#define PIXART_ADDR 0x58

// Camera registers (common for Pixart IR cameras in Wiimotes)
#define REG_PART_ID 0x00
#define REG_FRAME_START 0x36

// Write a single byte to a register
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

// Read a single byte from a register
uint8_t readRegister(uint8_t reg) {
  Wire.beginTransmission(PIXART_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false); // repeated start
  
  Wire.requestFrom(PIXART_ADDR, 1);
  if (Wire.available()) {
    return Wire.read();
  }
  return 0xFF; // error value
}

// Read multiple bytes starting from a register
void readRegisters(uint8_t reg, uint8_t* buffer, uint8_t length) {
  // Initialize buffer to 0
  memset(buffer, 0, length);
  
  Wire.beginTransmission(PIXART_ADDR);
  Wire.write(reg);
  uint8_t error = Wire.endTransmission(false); // repeated start
  
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

// Initialize the Pixart camera
bool initCamera() {
  LOG_DEBUG("\n=== Initializing Pixart IR Camera ===");
  
  // Check if device is present
  Wire.beginTransmission(PIXART_ADDR);
  uint8_t error = Wire.endTransmission();
  
  if (error != 0) {
    LOG_ALWAYS("Camera not found at address 0x%02X (error: %d)\n", PIXART_ADDR, error);
    return false;
  }
  
  LOG_DEBUG("Camera detected at address 0x%02X\n", PIXART_ADDR);
  
  // Initialization sequence based on Japanese documentation
  LOG_DEBUG("Sending initialization sequence...");
  
  // Step 1: Write 0x01 to register 0x30
  LOG_DEBUG("Step 1: Write 0x01 to 0x30");
  writeRegister(0x30, 0x01);
  delay(10);
  
  // Step 2: Write 0x08 to register 0x30
  LOG_DEBUG("Step 2: Write 0x08 to 0x30");
  writeRegister(0x30, 0x08);
  delay(10);
  
  // Step 3: Write 0x90 to register 0x06
  LOG_DEBUG("Step 3: Write 0x90 to 0x06");
  writeRegister(0x06, 0x90);
  delay(10);
  
  // Step 4: Write 0xC0 to register 0x08
  LOG_DEBUG("Step 4: Write 0xC0 to 0x08");
  writeRegister(0x08, 0xC0);
  delay(10);
  
  // Step 5: Write 0x40 to register 0x1A
  LOG_DEBUG("Step 5: Write 0x40 to 0x1A");
  writeRegister(0x1A, 0x40);
  delay(10);
  
  // Step 6: Write 0x33 to register 0x33
  LOG_DEBUG("Step 6: Write 0x33 to 0x33");
  writeRegister(0x33, 0x33);
  delay(10);
  
  LOG_DEBUG("Camera initialized successfully!\n");
  return true;
}

// Read IR blob data from camera
void readCameraData() {
  uint8_t data[16]; // IR camera returns 16 bytes: 1 header + 4 coordinates x 3 bytes each
  
  // First, send command to read from register 0x36
  Wire.beginTransmission(PIXART_ADDR);
  Wire.write(0x36);
  uint8_t error = Wire.endTransmission();
  
  if (error != 0) {
    return; // Silently skip on error for high-speed operation
  }
  
  // Small delay for camera to prepare data
  delayMicroseconds(200);
  
  // Now read 16 bytes
  uint8_t bytesRead = Wire.requestFrom((uint8_t)PIXART_ADDR, (uint8_t)16);
  if (bytesRead != 16) {
    return; // Skip incomplete reads
  }
  
  for (int i = 0; i < 16 && Wire.available(); i++) {
    data[i] = Wire.read();
  }
  
  // Parse IR blob data
  // Format: [header] [coord1: 3 bytes] [coord2: 3 bytes] [coord3: 3 bytes] [coord4: 3 bytes]
  // Each coordinate: XX YY SS
  // X = (SS & 0x30) << 4 | XX
  // Y = (SS & 0xC0) << 2 | YY
  
  // Parse and track trajectory for spell detection
  int currentX = -1;
  int currentY = -1;
  uint32_t currentTime = millis();
  
  // Find the first valid IR point (assuming single wand)
  for (int i = 0; i < 4; i++) {
    int offset = 1 + (i * 3);
    if (offset + 2 < 16) {
      uint8_t xx = data[offset];
      uint8_t yy = data[offset + 1];
      uint8_t ss = data[offset + 2];
      
      int x = ((ss & 0x30) << 4) | xx;
      int y = ((ss & 0xC0) << 2) | yy;
      
      if (x != 0x3FF && y != 0x3FF) {
        currentX = x;
        currentY = y;
        break; // Use first detected point
      }
    }
  }
  
  // State machine for gesture detection
  if (currentX >= 0 && currentY >= 0) {
    // IR point detected
    float movement = 0;
    if (lastX >= 0 && lastY >= 0) {
      float dx = currentX - lastX;
      float dy = currentY - lastY;
      movement = sqrt(dx*dx + dy*dy);
      drawIRPoint(currentX, currentY, true);  // Show active point
    }
    else {
      drawIRPoint(-1, -1, false);  // Clear point
    }
    
    switch (currentState) {
      case WAITING_FOR_IR: {
        // IR detected - show yellow and wait for stillness
        stablePosition = {currentX, currentY, currentTime};
        stillnessStartTime = currentTime;
        currentState = READY;
        ledSolid("yellow");
        backlightOn();
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
            Point p = {currentX, currentY, currentTime};
            currentTrajectory.push_back(p);
            lastMovementTime = currentTime;
            hasMovedDuringRecording = false;
            currentState = RECORDING;
            ledSolid("blue");
            LOG_DEBUG("STATE: Tracking started");
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
          ledOff();
        }
        break;
      }
        
      case RECORDING: {
        // Always add point to trajectory
        Point p = {currentX, currentY, currentTime};
        currentTrajectory.push_back(p);
        
        // Limit trajectory size
        if (currentTrajectory.size() > MAX_TRAJECTORY_POINTS) {
          currentTrajectory.erase(currentTrajectory.begin());
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
          delay(500);
          currentTrajectory.clear();
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
        matchSpell(currentTrajectory);
        
        // Check if we found a match
        std::vector<Point> normalized = normalizeTrajectory(currentTrajectory);
        std::vector<Point> resampled = resampleTrajectory(normalized, 20);
        float bestMatch = 0;
        const char* bestSpell = "Unknown";
        
        for (const auto& spell : spellPatterns) {
          std::vector<Point> spellNorm = normalizeTrajectory(spell.pattern);
          std::vector<Point> spellResampled = resampleTrajectory(spellNorm, 20);
          float similarity = calculateSimilarity(resampled, spellResampled);
          if (similarity > bestMatch) {
            bestMatch = similarity;
            bestSpell = spell.name;
          }
        }
        
        if (bestMatch >= MATCH_THRESHOLD) {
          // Spell detected - publish to MQTT and show effects
          publishSpell(bestSpell);
          displaySpellName(bestSpell);
          setLEDMode(LED_SPARKLE);
          ledOnTime = millis();  // Start LED effect timer
        } else {
          // No match - blink red
          ledSolid("red");
          ledOnTime = millis();  // Start LED effect timer
          displaySpellName("No Match");
        }
      } else {
        // Not enough movement - blink red
        LOG_DEBUG("Insufficient movement (%.1f px)\n", totalDistance);
        ledSolid("red");
        ledOnTime = millis();  // Start LED effect timer
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
      ledOff();
      drawIRPoint(-1, -1, false);  // Clear IR point from display
      lastX = -1;
      lastY = -1;
      irLostTime = 0;
    }
  }
  
  // Output in compact CSV format for visualization
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