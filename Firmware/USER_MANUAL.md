# Glyph Reader User Manual

**Version 1.0**  
**Futility Designs**

==================================================
Status - DRAFT
Some instructions describe functionality that is not
yet implemented, and some instructions are flat out inaccurate
===================================================

---

## Table of Contents

1. [Introduction](#introduction)
2. [Initial Setup](#initial-setup)
3. [WiFi Configuration](#wifi-configuration)
4. [Web Portal Settings](#web-portal-settings)
5. [Using the Device](#using-the-device)
6. [Nightlight Mode](#nightlight-mode)
7. [On-Board Settings Menu](#on-board-settings-menu)
8. [Home Automation Integration](#home-automation-integration)
9. [Customizing Spells (SD Card)](#customizing-spells-sd-card)
10. [Adding New Spells (SD Card)](#adding-new-spells-sd-card)
11. [Troubleshooting](#troubleshooting)
12. [Appendix: Built-in Spells](#appendix-built-in-spells)

---

## Introduction

The Glyph Reader is an interactive gesture recognition device that uses an infrared camera to track wand movements and recognize predefined patterns (spells). Draw magical patterns in the air, and the device responds with visual feedback, LED effects, and can trigger home automation actions via MQTT.


## Initial Setup

### First Power-On

1. **Power the device** using a USB-C cable (5V)
2. The **setup screen** will display initialization progress:
   - Display initialization
   - LED initialization
   - Preferences loading
   - WiFi configuration
   - Camera initialization
   - SD card detection

3. **Watch for the camera status**:
   - ✓ **Pass**: Camera initialized successfully
   - ✗ **Fail**: Camera not detected (check wiring)


### Display Backlight
- The backlight turns on during setup
- It will automatically turn off after **60 seconds** of inactivity
- Any spell detection or button press will wake it up

---

## WiFi Configuration

### First-Time WiFi Setup

When the device powers on for the first time, it will create a WiFi access point:

1. **Connect to the access point**:
   - Network Name: `GlyphReader-Setup`
   - No password required

2. **Open a web browser** and navigate to:
   - `http://192.168.4.1`
   - Or the captive portal should open automatically

3. **Configure WiFi**:
   - Click "Configure WiFi"
   - Select your home WiFi network from the list
   - Enter your WiFi password
   - Click "Save"

4. **Device connects**:
   - The device will restart and connect to your WiFi
   - The display will show "WiFi Manager: pass"

### Accessing the Web Portal After Setup

Once connected to WiFi, access the device at:
- `http://glyphreader.local` (recommended)
- Or by IP address (check your router)

### Offline Mode

If WiFi is not configured or unavailable:
- Device operates in offline mode
- MQTT features disabled
- Web portal accessible at `http://192.168.4.1` (AP mode)
- Local spell detection still works

---

## Web Portal Settings

Access the web portal to configure advanced settings.

### MQTT Configuration

**Purpose**: Send spell detection events to home automation systems

- **MQTT Host**: IP address or hostname of your MQTT broker
  - Example: `192.168.1.100` or `homeassistant.local`
- **MQTT Port**: Default is `1883`
- **MQTT Topic**: Topic where spell events are published
  - Example: `home/wand/spell`
  - Published message will have the spell name as the payload

### Gesture Tuning Parameters

Fine-tune how the device detects and recognizes gestures:

#### **Movement Threshold** (default: 15 pixels)
- Minimum movement required to start recording a gesture
- **Lower**: More sensitive, may trigger from small movements
- **Higher**: Requires more deliberate movement to start

#### **Stillness Threshold** (default: 20 pixels)
- Maximum movement considered "still"
- Used to detect ready state at the beginning of a spell
- **Lower**: Stricter stillness requirement
- **Higher**: Allows more drift while "still"

#### **Ready Stillness Time** (default: 600ms)
- How long the wand must be still to enter READY state
- READY state = green LED indicator
- **Shorter**: Faster to start recording
- **Longer**: More deliberate ready position required

#### **Gesture Timeout** (default: 5000ms / 5 seconds)
- Maximum time allowed to complete a gesture
- Gesture ends automatically if exceeded
- **Shorter**: Forces faster spell casting
- **Longer**: Allows slower, more careful gestures

#### **IR Loss Timeout** (default: 300ms)
- How long IR can be lost before ending gesture
- Allows brief tracking loss without ending spell
- **Shorter**: Less forgiving of lost tracking
- **Longer**: More tolerant of interruptions

### Nightlight Settings

Configure spell-based nightlight control (also available via on-board settings):

- **Nightlight ON Spell**: Choose which spell activates nightlight
  - Example: "Illuminate"
- **Nightlight OFF Spell**: Choose which spell deactivates nightlight
  - Example: "Dark"
- **Nightlight Brightness**: LED brightness (10-255)
  - Default: 150

**Toggle Mode**: If you set the same spell for both ON and OFF, the device will toggle the nightlight (one cast turns it on, next cast turns it off).

### Saving Settings

- Click "Save Settings" button
- Settings are stored in non-volatile memory and will survive reboots

---

## Using the Device

### Basic Gesture Flow

1. **Point your wand** at the device (IR LED tip facing camera)
    - LED ring turns **yellow**
    - Disply will show the wand tip location in its fielf of view
    - Move the wand to a position that is appropriate for the starting point of the spell you want to cast
2. **Hold still** for ~600ms
   - LED ring turns **green** (READY state)
3. **Draw your spell pattern** in the air
   - Display shows a trace of your spell
   - LED ring turns **blue** (RECORDING)
4. **End Spell** by turning off the wand
   - Device processes your gesture
   - Matches against known spell patterns

### Successful Match

When your gesture matches a spell (Highest score ≥70% similarity):

1. **Display**: Shows spell name and image (if available)
   - Spell name displays for 3 seconds
2. **LEDs**: Random color effect
   - Solid color, rainbow, or sparkle
   - Effect lasts 5 seconds
3. **MQTT**: Publishes spell name if MQTT is configured
4. **Special Spells**: Nightlight control (if configured)

### No Match

When gesture doesn't match any spell:

1. **Display**: Shows "No Match"
2. **LEDs**: Solid red

### Other Feedback Messages

- **"Too Small"**: Gesture didn't move enough (< 50 pixels total)
- **"Too Short"**: Gesture had too few points (< 10 samples)

### Screen Behavior

- **Backlight timeout**: 60 seconds of inactivity
- **Spell name timeout**: 3 seconds after detection
- **Trail clearing**: Automatically clears after spell or timeout
- **Wake up**: Any spell detection or button press wakes the screen

---

## Nightlight Mode

### What is Nightlight Mode?

A continuous, ambient LED effect that stays on until turned off. Perfect for:
- Room accent lighting
- Night light for hallways/bedrooms

### Activating Nightlight

**Method 1: Spell-based (Default)**
- Configure nightlight spells in web portal or on-board settings
- Cast the configured "ON" spell
- Example: Cast "Illuminate" to turn on

**Method 2: Button Control**
- Single-click **Button 1**
- Toggles nightlight on/off

### Nightlight Brightness

**Adjust while active:**
- Cast "Raise" spell: Increase brightness (+50)
- Cast "Lower" spell: Decrease brightness (-50)
- Range: 10-255
- New brightness is saved automatically

**Default brightness**: 150 (medium)

### Nightlight Timeout

- If device is connected to wifi the nightlight will turn off at sunrise
- If the device is not connected to wifi the nightlight turns off after 8 hours

### Toggle Mode

If the same spell is assigned to both ON and OFF:
- First cast: Turns nightlight ON
- Second cast: Turns nightlight OFF
- Third cast: Turns nightlight ON (continues toggling)

---

## On-Board Settings Menu

Configure nightlight spells without needing the web portal.

### Entering Settings Mode

1. **Double-click Button 2** 
2. Display shows settings menu
3. Camera tracking is **disabled** while in settings

### Settings Menu Display

- **Top**: Current setting name
  - "NL ON Spell" or "NL OFF Spell"
- **Center**: Current value (spell name or "Disabled")
- **Bottom**: Navigation instructions
- **Mode indicator**:
  - `< BROWSE >` (yellow): Navigating between settings
  - `[ EDITING ]` (green): Changing a value

### Navigating Settings

**Browse Mode** (default):
- **Button 2 (single-click)**: Move to next setting
  - Cycles between NL ON and NL OFF
- **Button 1 (single-click)**: Enter edit mode

**Edit Mode**:
- **Button 2 (single-click)**: Cycle through spell options
  - Options: Disabled, Unlock, Terminate, Ignite, Gust, Lower, Raise, Levitate, Silence, Arresto, Halt, Move, Resume, Illuminate, Dark
- **Button 1 (single-click)**: Save and exit edit mode

### Exiting Settings

- **Long-press Button 2** (hold 1 second)
- Returns to normal operation
- Camera tracking re-enabled

### Settings Persistence

- All changes saved to non-volatile memory immediately
- Settings survive reboots and power cycles
- Same settings available in web portal

---

## Home Automation Integration

### MQTT Publishing

When a spell is detected, the device publishes a JSON message:

**Topic**: Configured in web portal (e.g., `home/wand/spell`)

**Payload**:
```json
{
  "payload": "Ignite",
}
```

### Home Assistant Example

**MQTT Sensor Configuration** (`configuration.yaml`):
```yaml
mqtt:
  sensor:
    - name: "Wand Last Spell"
      state_topic: "home/wand/spell"
      value_template: "{{ value_json.spell }}"
      icon: mdi:magic-staff
```

**Automation Example** (Turn on lights with "Ignite"):
```yaml
automation:
  - alias: "Wand - Ignite Lights"
    trigger:
      - platform: mqtt
        topic: "home/wand/spell"
    condition:
      - condition: template
        value_template: "{{ trigger.payload_json.spell == 'Ignite' }}"
    action:
      - service: light.turn_on
        target:
          entity_id: light.living_room
        data:
          brightness: 255
          color_temp: 370
```

### NodeRED Example

**MQTT In Node**:
- Server: Your MQTT broker
- Topic: `home/wand/spell`

**Function Node** (Parse and route):
```javascript
var spell = msg.payload.spell;

switch(spell) {
    case "Ignite":
        msg.payload = {on: true};
        return [msg, null];
    case "Dark":
        msg.payload = {on: false};
        return [null, msg];
}
```

**Output Nodes**: Connect to your devices

### OpenHAB Example

**Items** (`wand.items`):
```
String WandSpell "Last Spell [%s]" { mqtt="<[broker:home/wand/spell:state:JSONPATH($.spell)]" }
```

**Rules** (`wand.rules`):
```
rule "Wand Ignite"
when
    Item WandSpell changed to "Ignite"
then
    sendCommand(LivingRoomLight, ON)
end
```

---

## Customizing Spells (SD Card)

You can modify existing spells by placing a `spells.json` file on the SD card.

### SD Card Setup

1. **Format SD card**: FAT32 file system
2. **Insert into device**: SD card slot on board
3. **Create `spells.json`**: In the root directory

### Modify Existing Spells

Change the gesture pattern or display image for built-in spells.

**File**: `spells.json`
```json
{
  "mode": "modify",
  "spells": [
    {
      "name": "Ignite",
      "pattern": [
        {"x": 100, "y": 100},
        {"x": 200, "y": 200},
        {"x": 300, "y": 100}
      ],
      "image": "my_ignite.bmp"
    }
  ]
}
```

**Fields**:
- `name`: Exact name of existing spell to modify
- `pattern`: Array of {x, y} coordinates (optional)
  - Coordinates in any scale (normalized automatically)
  - Resampled to 40 points
- `image`: Custom .bmp filename (optional)
  - 24-bit BMP format
  - 240x240 pixels recommended

### Custom Images

1. Create 24-bit BMP file (240x240)
2. Place in SD card root directory
3. Reference filename in `spells.json`
4. Filename is case-sensitive

**Default naming**: If no custom image specified, device looks for `<spellname>.bmp` (e.g., `ignite.bmp`)

### Reloading Changes

1. Power off device
2. Update `spells.json` on SD card
3. Power on device
4. Changes loaded during startup

---

## Adding New Spells (SD Card)

Create entirely new spells with custom patterns.

### Add Mode

**File**: `spells.json`
```json
{
  "mode": "add",
  "spells": [
    {
      "name": "Fireball",
      "pattern": [
        {"x": 512, "y": 100},
        {"x": 600, "y": 200},
        {"x": 700, "y": 400},
        {"x": 600, "y": 600},
        {"x": 512, "y": 700},
        {"x": 400, "y": 600},
        {"x": 300, "y": 400},
        {"x": 400, "y": 200},
        {"x": 512, "y": 100}
      ],
      "image": "fireball.bmp"
    }
  ]
}
```

**Notes**:
- New spells added to existing library
- Must have unique names
- Pattern normalized and resampled automatically

### Replace Mode (Advanced)

**Warning**: Replaces ALL built-in spells with your custom set.

**File**: `spells.json`
```json
{
  "mode": "replace",
  "spells": [
    {
      "name": "MySpell1",
      "pattern": [ ... ]
    },
    {
      "name": "MySpell2",
      "pattern": [ ... ]
    }
  ]
}
```

**Use cases**:
- Completely custom spell library
- Different language/theme
- Simplified set for specific use

### Pattern Design Tips

1. **Coordinate scale**: Use any scale (100-1000 recommended)
2. **Point count**: 10-50 points works well
   - Too few: Loses detail
   - Too many: Redundant (resampled to 40)
3. **Distinctive shapes**: Avoid similar patterns
4. **Test and iterate**: Adjust based on recognition accuracy

### Spell Pattern Examples

See `CUSTOM_SPELLS.md` for detailed examples and advanced configuration options.

---

## Troubleshooting

### Camera Not Detected

**Symptoms**: "Camera: fail" during setup

**Solutions**:
1. Check I2C wiring (SDA=GPIO6, SCL=GPIO5)
2. Verify camera power (3.3V)
3. Check camera address (should be 0x0C)
4. Device will retry initialization every 5 seconds

### WiFi Won't Connect

**Symptoms**: Stuck in AP mode

**Solutions**:
1. Verify WiFi password is correct
2. Check WiFi is 2.4GHz (ESP32 doesn't support 5GHz)
3. Move device closer to router
4. Try accessing web portal at `192.168.4.1`
5. Reset WiFi settings in web portal

### Spells Not Recognized

**Symptoms**: Always shows "No Match"

**Solutions**:
1. Point wand IR LED directly at camera
2. Hold still longer in ready position
3. Draw pattern more slowly
4. Adjust tuning parameters in web portal:
   - Increase Movement Threshold
   - Increase Stillness Threshold
   - Increase Ready Stillness Time
5. Check camera can see IR LED (use phone camera)

### Display Backlight Won't Turn On

**Solutions**:
1. Cast any spell to wake display
2. Press any button to wake display
3. Check TFT_BL pin connection (GPIO 13)

### SD Card Not Detected

**Symptoms**: "SD Card: fail" during setup

**Solutions**:
1. Verify SD card is formatted FAT32
2. Check SD card is inserted fully
3. Try different SD card (max 32GB recommended)
4. Check SPI wiring for SD card

### LEDs Not Working

**Solutions**:
1. Check LED power and data connections
2. Verify LED type in code (WS2812B)
3. Check LED_PIN configuration
4. Try different LED effect or nightlight mode

### Settings Not Saving

**Symptoms**: Settings revert after reboot

**Solutions**:
1. Wait for confirmation message in web portal
2. Check NVS partition in flash memory
3. May need to erase flash and re-upload firmware

---

## Appendix: Built-in Spells

### Complete Spell List

1. **Unlock**
   - Pattern: Clockwise circle + line down
   - Motion: Key turning in lock

2. **Terminate**
   - Pattern: Z-shape
   - Motion: Aggressive cutting motion

3. **Ignite**
   - Pattern: Triangle
   - Motion: Fire symbol

4. **Gust**
   - Pattern: V-shape
   - Motion: Wind direction

5. **Lower**
   - Pattern: Arc 12→7 clockwise + line down
   - Motion: Lowering gesture

6. **Raise**
   - Pattern: Arc 6→10 counter-clockwise + line up
   - Motion: Raising gesture

7. **Levitate**
   - Pattern: Figure-8 / infinity symbol
   - Motion: Continuous flow

8. **Silence**
   - Pattern: Horizontal line
   - Motion: Silencing gesture

9. **Arresto** (also "Halt")
   - Pattern: Vertical line up
   - Motion: Stop/arrest command

10. **Halt**
    - Pattern: Vertical line down
    - Motion: Halt command

11. **Move**
    - Pattern: Horizontal line
    - Motion: Move/push gesture

12. **Resume**
    - Pattern: W-shape
    - Motion: Resume/continue pattern

13. **Illuminate**
    - Pattern: Star shape
    - Motion: Light/illumination symbol
    - Common use: Nightlight ON

14. **Dark**
    - Pattern: Diagonal slash
    - Motion: Darkness/shadow
    - Common use: Nightlight OFF

### Special Spell Behaviors

**Nightlight Control** (configurable):
- Illuminate → Turn ON nightlight
- Dark → Turn OFF nightlight

**Nightlight Brightness** (when active):
- Raise → Increase brightness
- Lower → Decrease brightness

### Matching Threshold

- Minimum similarity: **70%**
- Scale invariant: Size doesn't matter
- Translation invariant: Position doesn't matter
- Length invariant: Speed doesn't matter

---

## Quick Reference Card

### Button Controls

| Action | Result |
|--------|--------|
| Button 1 - Single Click | Toggle nightlight (normal mode) |
| Button 2 - Double Click | Enter settings menu |
| Button 2 - Long Press | Exit settings menu |
| Settings: Button 1 | Select/Confirm setting |
| Settings: Button 2 | Navigate/Change value |

### LED Indicators

| Color | Meaning |
|-------|---------|
| Yellow | READY - Hold still, ready to record |
| Blue | RECORDING - Drawing gesture |
| Random Effect | Spell matched successfully |
| Red Blink | No match / Error |
| Soft White | Nightlight mode active |

### Web Portal Quick Links

- Portal: `http://glyphreader.local`
- AP Mode: `http://192.168.4.1`
- mDNS: `glyphreader.local`

---

**For more information**, see:
- `CUSTOM_SPELLS.md` - Advanced spell customization
- Source code: `github.com/futilitydesigns/glyphReader`

**Support**: [Your support contact information]

**License**: PolyForm Noncommercial License 1.0.0

---

*End of User Manual*
