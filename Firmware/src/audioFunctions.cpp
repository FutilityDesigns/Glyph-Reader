/*
================================================================================
  Audio Functions - I2S Audio Playback Implementation
================================================================================
  
  Implements non-blocking WAV file playback through MAX98357A I2S amplifier.
  Uses FreeRTOS tasks to play audio in parallel with other operations.
  Reads WAV files from SD card, parses headers, and streams audio data
  through ESP32's I2S peripheral.
  
  WAV File Format Support:
    - RIFF/WAVE format
    - PCM audio (format code 1)
    - 16-bit samples
    - Mono (1 channel) or Stereo (2 channels)
    - Common sample rates: 16000, 22050, 44100, 48000 Hz
  
  I2S Configuration:
    - Mode: I2S_MODE_MASTER | I2S_MODE_TX
    - Format: I2S standard format (Philips)
    - DMA buffers for smooth playback
  
  Non-Blocking Implementation:
    - Uses FreeRTOS task for audio playback
    - playSound() queues filename and returns immediately
    - Audio task handles file reading and I2S streaming
  
================================================================================
*/

#include "audioFunctions.h"
#include "glyphReader.h"
#include "sdFunctions.h"
#include "preferenceFunctions.h"
#include <driver/i2s.h>
#include <SD.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

//=====================================
// I2S Configuration
//=====================================

#define I2S_NUM         I2S_NUM_0  // Use I2S peripheral 0

//=====================================
// Audio State
//=====================================

static bool audioInitialized = false;
static uint8_t currentVolume = 100;  // Default 100% volume
static bool isPlaying = false;

//=====================================
// FreeRTOS Task Management
//=====================================

static TaskHandle_t audioTaskHandle = NULL;
static QueueHandle_t audioQueue = NULL;

//=====================================
// Forward Declarations
//=====================================

static bool playSound_internal(const char* filename);
static void audioPlaybackTask(void* parameter);

//=====================================
// WAV File Header Structures
//=====================================

/**
 * WAV file RIFF chunk header
 * First 12 bytes of any WAV file
 */
struct WAVHeader {
  char riff[4];           // "RIFF"
  uint32_t fileSize;      // File size - 8
  char wave[4];           // "WAVE"
};

/**
 * WAV format chunk
 * Describes audio format parameters
 */
struct WAVFormat {
  char fmt[4];            // "fmt "
  uint32_t chunkSize;     // Size of format chunk (usually 16)
  uint16_t audioFormat;   // Audio format (1 = PCM)
  uint16_t numChannels;   // Number of channels (1 = mono, 2 = stereo)
  uint32_t sampleRate;    // Sample rate (Hz)
  uint32_t byteRate;      // Bytes per second
  uint16_t blockAlign;    // Bytes per sample frame
  uint16_t bitsPerSample; // Bits per sample (8, 16, etc.)
};

/**
 * WAV data chunk header
 * Precedes the actual audio sample data
 */
struct WAVData {
  char data[4];           // "data"
  uint32_t dataSize;      // Size of audio data in bytes
};

//=====================================
// Helper Functions
//=====================================

/**
 * Apply volume scaling to audio samples
 * Scales 16-bit sample values based on current volume setting
 */
static int16_t applyVolume(int16_t sample) {
  if (currentVolume == 100) {
    return sample;
  } else if (currentVolume == 0) {
    return 0;
  }
  return (int16_t)((int32_t)sample * currentVolume / 100);
}

//=====================================
// Audio Playback Implementation
//=====================================

/**
 * Internal blocking playback function
 * Called by audio task - does the actual file reading and I2S streaming
 */
static bool playSound_internal(const char* filename) {
  if (!audioInitialized) {
    LOG_ALWAYS("Audio not initialized - call initAudio() first");
    return false;
  }
  
  if (!SD.exists(filename)) {
    LOG_ALWAYS("Audio file not found: %s", filename);
    return false;
  }
  
  LOG_DEBUG("Playing sound: %s", filename);
  
  // Open WAV file
  File audioFile = SD.open(filename, FILE_READ);
  if (!audioFile) {
    LOG_ALWAYS("Failed to open audio file: %s", filename);
    return false;
  }
  
  //-----------------------------------
  // Parse WAV Header
  //-----------------------------------
  
  WAVHeader wavHeader;
  audioFile.read((uint8_t*)&wavHeader, sizeof(WAVHeader));
  
  // Validate RIFF header
  if (strncmp(wavHeader.riff, "RIFF", 4) != 0 || strncmp(wavHeader.wave, "WAVE", 4) != 0) {
    LOG_ALWAYS("Invalid WAV file - missing RIFF/WAVE header");
    audioFile.close();
    return false;
  }
  
  // Parse format chunk
  WAVFormat wavFormat;
  audioFile.read((uint8_t*)&wavFormat, sizeof(WAVFormat));
  
  // Validate format chunk
  if (strncmp(wavFormat.fmt, "fmt ", 4) != 0) {
    LOG_ALWAYS("Invalid WAV file - missing fmt chunk");
    audioFile.close();
    return false;
  }
  
  // Check audio format
  if (wavFormat.audioFormat != 1) {
    LOG_ALWAYS("Unsupported audio format (must be PCM, got %d)", wavFormat.audioFormat);
    audioFile.close();
    return false;
  }
  
  // Check bit depth
  if (wavFormat.bitsPerSample != 16) {
    LOG_ALWAYS("Unsupported bit depth (must be 16-bit, got %d-bit)", wavFormat.bitsPerSample);
    audioFile.close();
    return false;
  }
  
  // Check channels
  if (wavFormat.numChannels != 1 && wavFormat.numChannels != 2) {
    LOG_ALWAYS("Unsupported channel count (must be 1 or 2, got %d)", wavFormat.numChannels);
    audioFile.close();
    return false;
  }
  
  LOG_DEBUG("WAV Format:");
  LOG_DEBUG("  Sample Rate: %d Hz", wavFormat.sampleRate);
  LOG_DEBUG("  Channels: %d", wavFormat.numChannels);
  LOG_DEBUG("  Bits/Sample: %d", wavFormat.bitsPerSample);
  
  // Skip any extra format bytes
  if (wavFormat.chunkSize > 16) {
    audioFile.seek(audioFile.position() + (wavFormat.chunkSize - 16));
  }
  
  // Find data chunk (skip any other chunks like LIST, INFO, etc.)
  WAVData wavData;
  bool foundData = false;
  
  while (audioFile.available()) {
    audioFile.read((uint8_t*)&wavData, sizeof(WAVData));
    
    if (strncmp(wavData.data, "data", 4) == 0) {
      foundData = true;
      break;
    }
    
    // Skip this unknown chunk
    audioFile.seek(audioFile.position() + wavData.dataSize);
  }
  
  if (!foundData) {
    LOG_ALWAYS("Invalid WAV file - no data chunk found");
    audioFile.close();
    return false;
  }
  
  LOG_DEBUG("  Data Size: %d bytes", wavData.dataSize);
  
  //-----------------------------------
  // Configure I2S for this audio file
  //-----------------------------------
  
  i2s_channel_t channelFormat = (wavFormat.numChannels == 2) ? I2S_CHANNEL_STEREO : I2S_CHANNEL_MONO;
  i2s_set_clk(I2S_NUM, wavFormat.sampleRate, I2S_BITS_PER_SAMPLE_16BIT, channelFormat);
  
  //-----------------------------------
  // Stream Audio Data
  //-----------------------------------
  
  isPlaying = true;
  uint32_t bytesRemaining = wavData.dataSize;
  uint8_t buffer[I2S_BUFFER_SIZE];
  size_t bytesWritten;
  
  while (bytesRemaining > 0 && isPlaying) {
    // Read chunk from file
    size_t bytesToRead = min(bytesRemaining, (uint32_t)I2S_BUFFER_SIZE);
    size_t bytesRead = audioFile.read(buffer, bytesToRead);
    
    if (bytesRead == 0) {
      break;  // End of file or read error
    }
    
    // Apply volume scaling
    if (currentVolume != 100) {
      int16_t* samples = (int16_t*)buffer;
      size_t sampleCount = bytesRead / 2;
      for (size_t i = 0; i < sampleCount; i++) {
        samples[i] = applyVolume(samples[i]);
      }
    }
    
    // If mono, duplicate samples for stereo output
    if (wavFormat.numChannels == 1 && channelFormat == I2S_CHANNEL_STEREO) {
      // Convert mono to stereo by duplicating each sample
      int16_t monoBuffer[I2S_BUFFER_SIZE / 2];
      memcpy(monoBuffer, buffer, bytesRead);
      
      int16_t* stereoSamples = (int16_t*)buffer;
      int16_t* monoSamples = monoBuffer;
      size_t sampleCount = bytesRead / 2;
      
      for (size_t i = 0; i < sampleCount; i++) {
        stereoSamples[i * 2] = monoSamples[i];      // Left channel
        stereoSamples[i * 2 + 1] = monoSamples[i];  // Right channel
      }
      
      bytesRead *= 2;  // Double the data size
    }
    
    // Write to I2S
    i2s_write(I2S_NUM, buffer, bytesRead, &bytesWritten, portMAX_DELAY);
    
    bytesRemaining -= bytesToRead;
    
    // Allow other tasks to run
    yield();
  }
  
  // Close file
  audioFile.close();
  isPlaying = false;
  
  LOG_DEBUG("Sound playback complete");
  return true;
}

/**
 * Audio playback task (runs in background)
 * Handles actual WAV file reading and I2S streaming
 */
static void audioPlaybackTask(void* parameter) {
  char filename[64];
  
  while (true) {
    // Wait for filename from queue
    if (xQueueReceive(audioQueue, filename, portMAX_DELAY) == pdTRUE) {
      // Play the sound (blocking within this task only)
      playSound_internal(filename);
    }
  }
}

//=====================================
// I2S Initialization
//=====================================

/**
 * Initialize I2S audio system
 * Configures ESP32 I2S peripheral for audio output to MAX98357A.
 */
bool initAudio() {
  LOG_DEBUG("Initializing I2S audio...");
  
  // I2S configuration
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 44100,  // Default sample rate (will be updated per file)
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,  // Stereo
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = I2S_BUFFER_SIZE,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };
  
  // I2S pin configuration
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCLK,
    .ws_io_num = I2S_LRC,
    .data_out_num = I2S_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };
  
  // Install and start I2S driver
  esp_err_t err = i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    LOG_ALWAYS("Failed to install I2S driver: %d", err);
    return false;
  }
  
  err = i2s_set_pin(I2S_NUM, &pin_config);
  if (err != ESP_OK) {
    LOG_ALWAYS("Failed to set I2S pins: %d", err);
    i2s_driver_uninstall(I2S_NUM);
    return false;
  }
  
  // Set initial clock to avoid issues
  i2s_set_clk(I2S_NUM, 44100, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);
  
  // Create queue for audio filenames (holds up to 3 pending sounds)
  audioQueue = xQueueCreate(3, sizeof(char) * 64);
  if (audioQueue == NULL) {
    LOG_ALWAYS("Failed to create audio queue");
    i2s_driver_uninstall(I2S_NUM);
    return false;
  }
  
  // Create audio playback task
  BaseType_t taskCreated = xTaskCreatePinnedToCore(
    audioPlaybackTask,    // Task function
    "AudioTask",          // Task name
    4096,                 // Stack size (bytes)
    NULL,                 // Parameters
    1,                    // Priority (1 = low, don't interfere with WiFi/camera)
    &audioTaskHandle,     // Task handle
    0                     // Core 0 (Core 1 is used for WiFi/Arduino loop)
  );
  
  if (taskCreated != pdPASS) {
    LOG_ALWAYS("Failed to create audio task");
    vQueueDelete(audioQueue);
    i2s_driver_uninstall(I2S_NUM);
    return false;
  }
  
  audioInitialized = true;
  LOG_DEBUG("I2S audio initialized successfully (non-blocking mode)");
  LOG_DEBUG("  BCLK:  GPIO%d", I2S_BCLK);
  LOG_DEBUG("  LRCLK: GPIO%d", I2S_LRC);
  LOG_DEBUG("  DIN:   GPIO%d", I2S_DOUT);
  
  return true;
}

//=====================================
// Volume Control
//=====================================

/**
 * Set audio volume (0-100)
 * Adjusts digital volume by scaling samples
 */
void setVolume(uint8_t volume) {
  currentVolume = constrain(volume, 0, 100);
  LOG_DEBUG("Audio volume set to %d%%", currentVolume);
}

//=====================================
// Public Audio Functions
//=====================================

/**
 * Queue a sound for playback (non-blocking)
 * Adds filename to queue for background task to play
 */
bool playSound(const char* filename) {
  // Check if sound is enabled in preferences
  if (!SOUND_ENABLED) {
    return false;  // Sound disabled - skip silently
  }
  
  if (!audioInitialized) {
    LOG_ALWAYS("Audio not initialized - call initAudio() first");
    return false;
  }
  
  if (!SD.exists(filename)) {
    LOG_DEBUG("Audio file not found: %s", filename);
    return false;
  }
  
  // Queue the filename for playback
  if (xQueueSend(audioQueue, filename, 0) == pdTRUE) {
    LOG_DEBUG("Queued sound: %s", filename);
    return true;
  } else {
    LOG_DEBUG("Audio queue full - skipping: %s", filename);
    return false;
  }
}

/**
 * Stop current audio playback
 */
void stopSound() {
  isPlaying = false;
  
  // Clear I2S DMA buffers
  if (audioInitialized) {
    i2s_zero_dma_buffer(I2S_NUM);
  }
  
  LOG_DEBUG("Sound playback stopped");
}
