/*
================================================================================
  Camera Functions - Pixart IR Camera Interface Header
================================================================================
  
  This module provides the interface for the Pixart IR camera, which tracks
  IR point sources (wand tip) in 2D space similar to a Wiimote sensor.
  
  Functions:
    - initCamera():     Initialize camera via I2C, configure registers
    - readCameraData(): Main state machine for gesture tracking and recognition
  
  State Machine (implemented in cameraFunction.cpp):
    WAITING_FOR_IR: No IR detected, waiting for wand
    READY:          IR detected and stable, ready to record (yellow LED)
    RECORDING:      Actively recording gesture trajectory (blue LED)
  
  Camera Details:
    - I2C address: 0x0C (default Pixart address)
    - Tracking resolution: 1024x768 pixels
    - Update rate: ~100Hz when polled
    - Tracks up to 4 IR points simultaneously (we use 1)
  
================================================================================
*/

#ifndef CAMERA_FUNCTIONS_H
#define CAMERA_FUNCTIONS_H

/**
 * Initialize Pixart IR camera via I2C
 * Configures camera registers for IR tracking mode and verifies communication.
 * Must be called after Wire.begin() in setup().
 * return true if camera initialized successfully, false on I2C error
 */
bool initCamera();

/**
 * Read camera data and process gesture state machine
 * Main state machine that:
 *   1. Reads IR point position from camera
 *   2. Tracks wand movement and stability
 *   3. Records trajectory points during gesture
 *   4. Triggers spell matching when gesture completes
 *   5. Handles nightlight on/off spells specially
 */
void readCameraData();

/**
 * Check if camera is actively tracking (IR detected)
 * Returns true when in READY or RECORDING state, meaning IR is detected
 * and the system needs fast polling for responsive tracking.
 * return true if actively tracking, false if waiting for IR
 */
bool isTrackingActive();

/**
 * Get current IR position for spell recording
 * Helper function that reads camera data and returns the current IR position.
 * Used by custom spell recording state machine.
 * param x Output: X coordinate in display space (0-239)
 * param y Output: Y coordinate in display space (0-239)
 * return true if valid IR point detected, false otherwise
 */
bool getIRPosition(int& x, int& y);

// Constants exported for spell recording
#define MAX_TRAJECTORY_POINTS 1000
#define NO_MOVEMENT_TIMEOUT 500

#endif // CAMERA_FUNCTIONS_H