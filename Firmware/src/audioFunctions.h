/*
================================================================================
  Audio Functions - I2S Audio Playback Header
================================================================================
  
  This module manages audio playback through a MAX98357A I2S amplifier.
  Supports playing WAV files from SD card for spell sound effects.
  
  Hardware:
    - MAX98357A I2S Class D Amplifier
    - BCLK:  GPIO37 (Bit Clock)
    - LRCLK: GPIO38 (Left/Right Clock / Word Select)
    - DIN:   GPIO39 (I2S Data Input)
  
  Supported Audio Format:
    - WAV files (PCM format)
    - 16-bit samples
    - Mono or Stereo
    - Sample rates: 16kHz, 22.05kHz, 44.1kHz, 48kHz
  
  Usage:
    initAudio();                          // Initialize I2S
    playSound("/sounds/spell1.wav");      // Play sound effect
    stopSound();                          // Stop playback
  
================================================================================
*/

#ifndef AUDIO_FUNCTIONS_H
#define AUDIO_FUNCTIONS_H

#include <Arduino.h>

//=====================================
// I2S Pin Configuration
//=====================================

#define I2S_BCLK  37    // Bit Clock
#define I2S_LRC   38    // Left/Right Clock (Word Select)
#define I2S_DOUT  39    // Data Out

//=====================================
// Audio Configuration
//=====================================

#define I2S_BUFFER_SIZE 512   // Buffer size for I2S DMA (bytes)

//=====================================
// Audio Functions
//=====================================

/**
 * Initialize I2S audio system
 * Configures I2S peripheral for the MAX98357A amplifier.
 * Must be called during setup() before playing any sounds.
 * return true if successful, false on error
 */
bool initAudio();

/**
 * Play a WAV file from SD card (non-blocking)
 * Queues audio file for playback in background task. File must be in WAV format
 * with PCM encoding, 16-bit samples, mono or stereo.
 * 
 * filename: Path to WAV file on SD card (e.g., "/sounds/spell.wav")
 * return true if playback queued successfully, false on error
 * 
 * note: This is a non-blocking function - returns immediately while audio plays in background
 */
bool playSound(const char* filename);

/**
 * Stop current audio playback
 * Stops I2S playback immediately and releases resources.
 */
void stopSound();

/**
 * Set audio volume (0-100)
 * Note: MAX98357A has fixed gain, so this adjusts digital volume
 * by scaling sample values before sending to I2S.
 * 
 * volume: Volume level 0-100 (0 = mute, 100 = full volume)
 */
void setVolume(uint8_t volume);

#endif // AUDIO_FUNCTIONS_H
