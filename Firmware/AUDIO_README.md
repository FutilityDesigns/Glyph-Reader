# Audio System - MAX98357A I2S Amplifier

## Hardware Configuration

- **Amplifier**: MAX98357A I2S Class D Amplifier
- **BCLK** (Bit Clock): GPIO37
- **LRCLK** (Left/Right Clock): GPIO38  
- **DIN** (Data Input): GPIO39

## Audio File Requirements

### Required Format
WAV files must meet these specifications:

- **Format**: WAV (RIFF/WAVE)
- **Encoding**: PCM (uncompressed)
- **Bit Depth**: 16-bit
- **Channels**: Mono (1) or Stereo (2)
- **Sample Rates**: 16000, 22050, 44100, or 48000 Hz

### Recommended Settings for Spell Effects
For best balance of quality and file size:
- **Sample Rate**: 22050 Hz or 44100 Hz
- **Channels**: Mono (files are smaller, auto-converted to stereo)
- **Bit Depth**: 16-bit

### File Size Examples
- 1 second @ 22050 Hz mono 16-bit: ~44 KB
- 1 second @ 44100 Hz mono 16-bit: ~88 KB
- 1 second @ 44100 Hz stereo 16-bit: ~176 KB

## Converting Audio Files

### Using Audacity (Free)
1. Open your audio file in Audacity
2. Go to **File → Export → Export as WAV**
3. Set these options:
   - **Format**: WAV (Microsoft)
   - **Encoding**: Signed 16-bit PCM
4. In **Edit → Preferences → Quality**:
   - Set **Default Sample Rate** to 22050 or 44100 Hz
5. If stereo, convert to mono: **Tracks → Mix → Mix Stereo Down to Mono**
6. Export the file

### Using FFmpeg (Command Line)
```bash
# Convert any audio to WAV 16-bit 22050Hz mono
ffmpeg -i input.mp3 -ar 22050 -ac 1 -sample_fmt s16 output.wav

# Convert to 44100Hz stereo
ffmpeg -i input.mp3 -ar 44100 -ac 2 -sample_fmt s16 output.wav
```

## SD Card File Structure

Create a `/sounds/` folder on your SD card:

```
/
├── sounds/
│   ├── spell1.wav
│   ├── spell2.wav
│   ├── ignite.wav
│   ├── levitate.wav
│   └── illuminate.wav
├── images/
│   └── (spell images)
└── spells.json
```

## Usage Examples

### Basic Playback
```cpp
#include "audioFunctions.h"

void setup() {
    // Initialize audio system
    initAudio();
    
    // Play a sound effect
    playSound("/sounds/ignite.wav");
}
```

### With Volume Control
```cpp
// Set volume (0-100)
setVolume(80);  // 80% volume

// Play sound
playSound("/sounds/spell.wav");
```

### Stop Playback
```cpp
// Stop currently playing sound
stopSound();
```

### Adding Sound to Spell Detection

In `cameraFunction.cpp`, add sound playback when a spell is detected:

```cpp
#include "audioFunctions.h"

// After spell is matched:
if (bestScore >= MATCH_THRESHOLD) {
    Serial.printf("Best spell match: %s (%.2f%%)\n", bestSpell, bestScore * 100);
    
    // Play spell-specific sound effect
    String soundFile = "/sounds/" + String(bestSpell) + ".wav";
    playSound(soundFile.c_str());
    
    // Display spell name
    displaySpell(bestSpell);
}
```

### Conditional Sound Effects
```cpp
// Only play sound if file exists
String soundFile = "/sounds/" + String(bestSpell) + ".wav";
if (SD.exists(soundFile.c_str())) {
    playSound(soundFile.c_str());
}
```

## Important Notes

1. **Blocking Playback**: `playSound()` blocks until the entire file plays. For long sounds, this may affect gesture detection responsiveness.

2. **File Size**: Keep sound effects short (0.5-2 seconds) for responsive feedback.

3. **SD Card Speed**: Use a fast SD card (Class 10 or UHS-I) for smooth playback without stuttering.

4. **Volume Control**: The MAX98357A has fixed hardware gain. Volume is adjusted digitally by scaling sample values.

5. **Mono vs Stereo**: Mono files are automatically converted to stereo output for the amplifier.

## Testing

Simple test code to verify audio system:

```cpp
void testAudio() {
    Serial.println("Testing audio system...");
    
    // Test with a simple beep or tone
    if (SD.exists("/sounds/test.wav")) {
        Serial.println("Playing test sound...");
        bool result = playSound("/sounds/test.wav");
        
        if (result) {
            Serial.println("Audio test passed!");
        } else {
            Serial.println("Audio test failed - check file format");
        }
    } else {
        Serial.println("Test file not found: /sounds/test.wav");
    }
}
```

## Troubleshooting

**No sound output:**
- Check wiring (BCLK=37, LRCLK=38, DIN=39)
- Verify MAX98357A has power (VIN and GND)
- Check SD card has valid WAV files
- Ensure audio initialization succeeded (check serial output)

**Distorted/garbled audio:**
- Verify file is 16-bit PCM WAV format
- Try lower sample rate (22050 Hz instead of 48000 Hz)
- Check volume isn't set too high (try 50%)

**Audio stuttering:**
- Use faster SD card
- Reduce sample rate
- Shorten audio files
- Check for other intensive tasks blocking playback

**File won't play:**
- Verify file format (use MediaInfo or similar tool)
- Check file size isn't corrupted
- Ensure file path is correct (case-sensitive)
- Verify SD card has enough free space
