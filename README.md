# GlyphReader

A magical wand system powered by ESP32-S3 with gesture recognition and spell casting!

## Features

- **Gesture Recognition**: Draw spell patterns in the air with IR tracking
- **Custom Spells**: Modify existing spells or create your own via SD card configuration
- **Visual Feedback**: Display spell names and images on circular TFT screen
- **Home Automation**: Device can join your home network and report detected spells over MQTT
- **Night Light**: Spells can be set to turn on and off a nightlight mode

#### Open Items to be completed
1. All boards are still being verified, designs should not be ordered until this line is removed
2. Complete Bill of Materials for All boards still needs to be generated
3. Enclosures and wand designs still need to be finalized and included
4. ~~Functionality for the built in buttons needs to be programmed~~
5. DIY version schematics need to be drawn for those wishing to build their own without the Custom Glyph Reader PCBs
6. ~~Some improvement still is required on the matching functions, false matches still happen more commonly than desired~~ - Matching greatly improved
7. User Docs still need to be finalized


## Getting Started

1. Flash the firmware to your ESP32-S3 device
2. Insert an SD card (optional - for custom spells and images)
3. Power on and start casting spells!

## Spell Casting
The Glyph Reader uses a specialized camera that looks for strong IR illumination points. The wands are designed with a powerful IR LED that can be turned on and off. The gesture tracking works as follows:

1. Point the wand at the Glyph Reader and turn on the LED
2. When the wand is detected a point will appear on the display to show you where in the field of view it is. The LEDS on the Glyph Reader will also turn yellow to indicate a point is detected. 
3. Hold the wand still until the LEDs turn green, this indicates the device is ready to track. 
4. Move the wand and draw the spell you wish to cast. The LEDs will turn blue to indicate tracking
5. When you have completed the spell turn the Wand LED off
6. The Glyph Reader will now process the tracked pattern, and if a match is found display the spell and a short LED animation. If no match is found it will show "No Match". If the tracked pattern is not large enough it will show "Too Small"

### Connecting to Wifi
Using wifi is not required, the device will run completely offline, you just won't be able to link it into home automation systems. The device will pause for 30 seconds at boot looking for a known wifi network, if none is found boot will continue after this pause. 

1. When the device first boots it will create a wifi Access point named "GlyphReader-Setup"
2. Connect to this access point with a phone or laptop. You may have to allow the connection since no internet access will be availible.
3. In a web browser, navigate to 192.168.4.1 in order to access the setup page
4. Click on "Configure Wifi" and allow the device to scan for networks
5. Select the desired network and enter the password, then press save
6. If the connection is successful the GlyphReader-Setup access point will go away and the device will be availible on your network at http://glyphreader.local

### Configuring MQTT
If you wish to connect the device to home automation, you must configure MQTT on the device. If you do not already have an MQTT broker running on your network it's recommended to setup Mostquitto MQTT Broker on some other device (https://mosquitto.org/)

1. Once the device is connected to your wifi network, navigate to http://glyphreader.local
2. Go to the setup page
3. Enter your mqtt broker's ip address and port
4. Enter the desired topic you want to publish on
5. When a spell is identified a message will be published with the built in spell name as the payload

### Configuring Night Light
There are two drop downs that allow setting specific spells to directly trigger the night light to turn on or off. 

### Tuning the gesture Tracking
There are some variables that can be adjusted to tune the gesture detection and tracking. These are also in the setup page of the webportal at http://glyphreader.local

- **STILLNESS THRESHOLD**: The maximum number of pixels the wand IR point can move when detecting the initial still position
- **READY STILLNESS TIME**: How long the IR point needs to stay still for the device to register ready to track
- **MOVEMENT THRESHOLD**: The minimum number of pixels the IR point then needs to move in order for the device to start tracking the motion path
- **GESTURE TIMEOUT**: Maximum time for path tracking
- **IR LOSS TIMEOUT**: How long the IR point needs to be missing before tracking is considered complete. This allows the point to briefly leave the camera's field of view and return without tracking ending. Longer time out time will result in a longer wait to begin processing the recorded gesture. Shorter times are less tolerant to tracking losses.

## Hardware Requirements (incomplete)
### Glyph Reader 
- Glyph Reader Custom PCBA and enclosure

Or to assemble your own:

- ESP32-S3 DevKit
- GC9A01A Circular TFT Display (240x240)
- DFRobot SEN0158 IR Positioning Camera
- SD Card Reader (optional but recommended)
- NeoPixel LEDs (optional)

### Wands
- A high power IR LED and some way to command it on and off


## Custom Spell Configuration

You can customize spells by placing a `spells.json` file on the SD card. This allows you to:

- Rename existing spells
- Change spell gesture patterns
- Use custom image files
- Add completely new spells

**📖 See [CUSTOM_SPELLS.md](Firmware/CUSTOM_SPELLS.md) for complete documentation and examples**

Quick example to rename a spell:
```json
{
  "modify": [
    {
      "builtInName": "Ignite",
      "customName": "Fire Spell",
      "imageFile": "my_fire.bmp"
    }
  ]
}
```


## Built-in Spells
The built in spells may share some familiarity to a certain story franchise, however the names have been generalized to avoid any potential copyright issues. If you wish to use story accurate names you can use the spell customization to rename any spell you wish. 

1. Unlock - Unlocking charm
2. Terminate - Counterspell
3. Ignite - Fire charm
4. Gust - Wind charm
5. Lower - Descending charm
6. Raise - Ascending charm
7. Move - Movement charm
8. Levitate - Levitation charm
9. Silence - Silencing charm
10. Halt - Stopping charm
11. Resume - Reanimation Charm
12. Illuminate - Illuminating Spell
13. Dark - Darkness Spell




## Licensing

This project is source-available and non-commercial.

- Firmware and tools are licensed under the **PolyForm Noncommercial License 1.0.0**.
- Hardware designs (PCB, CAD, enclosure files) are licensed under **CC BY-NC-SA 4.0**.

Commercial use — including manufacturing, selling hardware, selling kits,
or bundling the firmware in a product — is not permitted without a separate
commercial license from the author.