# Custom Spell Configuration Guide

## Overview

The Glyph Reader wand system  supports customizing spells through a JSON configuration file on the SD card. You can:
- **Rename** existing built-in spells
- **Redefine** gesture patterns for existing spells
- **Change** image files for existing spells
- **Add** completely new custom spells

## Quick Start

1. Copy `spells.json.example` to your SD card and rename it to `spells.json`
2. Edit the file to customize your spells
3. Insert the SD card into your wand
4. Power on - your customizations will be loaded automatically!

## Built-in Spells

The following spells are built into the system by default:

1. **Unlock** - Clockwise circle with line down
2. **Terminate** - Z shape with sloping final line
3. **Ignite** - Triangle
4. **Gust** - V shape
5. **Lower** - "p" shape (partial circle + line down)
6. **Raise** - "b" shape (partial circle + line up)
7. **Move** - "4" shape
8. **Levitate** - Half circle (9→3) + line down
9. **Silence** - Half circle (3→9) + line down
10. **Halt** - Capital letter M

## JSON File Format

The `spells.json` file should be placed in the root directory of your SD card and follows this structure:

```json
{
  "modify": [
    // Modifications to existing built-in spells
  ],
  "custom": [
    // Completely new custom spells
  ]
}
```

### Coordinate System

- Screen dimensions: **1024 x 768** pixels
- Origin (0,0) is at the **top-left** corner
- X increases to the right (0 → 1024)
- Y increases downward (0 → 768)
- Center of screen: approximately (512, 384)

## Modifying Built-in Spells

### Rename a Spell

Change the display name while keeping the pattern:

```json
{
  "modify": [
    {
      "builtInName": "Ignite",
      "customName": "Fire Spell"
    }
  ]
}
```

### Change Image File

Use a custom image file instead of the default:

```json
{
  "modify": [
    {
      "builtInName": "Ignite",
      "imageFile": "my_fire.bmp"
    }
  ]
}
```

**Note:** Image files should be:
- 24-bit uncompressed BMP format
- Placed in the root directory of the SD card
- Referenced with or without leading slash (both `/fire.bmp` and `fire.bmp` work)
- Reference Affinity Design files are included for all default images to be used to make alterations or new images

### Redefine Pattern

Change the gesture pattern:

```json
{
  "modify": [
    {
      "builtInName": "Unlock",
      "pattern": [
        {"x": 512, "y": 184},
        {"x": 712, "y": 384},
        {"x": 512, "y": 584},
        {"x": 312, "y": 384},
        {"x": 512, "y": 184}
      ]
    }
  ]
}
```

### Complete Modification

Rename, change image, AND redefine pattern:

```json
{
  "modify": [
    {
      "builtInName": "Gust",
      "customName": "Wind Blast",
      "imageFile": "wind.bmp",
      "pattern": [
        {"x": 100, "y": 384},
        {"x": 300, "y": 200},
        {"x": 500, "y": 384},
        {"x": 700, "y": 200},
        {"x": 900, "y": 384}
      ]
    }
  ]
}
```

## Adding New Custom Spells

Add completely new spells that aren't built into the system:

```json
{
  "custom": [
    {
      "name": "Disarm",
      "imageFile": "disarm.bmp",
      "pattern": [
        {"x": 200, "y": 200},
        {"x": 824, "y": 584}
      ]
    },
    {
      "name": "Stun",
      "pattern": [
        {"x": 512, "y": 600},
        {"x": 512, "y": 200},
        {"x": 300, "y": 400},
        {"x": 724, "y": 400}
      ]
    }
  ]
}
```

**Notes:**
- `name` is required for custom spells
- `imageFile` is optional (if omitted, spell name will be displayed as text)
- `pattern` must have at least 2 points

## Pattern Design Tips

### Simple Shapes

**Line:**
```json
[
  {"x": 200, "y": 400},
  {"x": 824, "y": 400}
]
```

**Triangle:**
```json
[
  {"x": 200, "y": 600},
  {"x": 512, "y": 200},
  {"x": 824, "y": 600},
  {"x": 200, "y": 600}
]
```

**Square:**
```json
[
  {"x": 300, "y": 300},
  {"x": 724, "y": 300},
  {"x": 724, "y": 468},
  {"x": 300, "y": 468},
  {"x": 300, "y": 300}
]
```

### Circular Shapes

For circles, use trigonometry or an online tool. Example circle pattern (8 points):

```json
[
  {"x": 512, "y": 184},  // Top (12 o'clock)
  {"x": 653, "y": 243},  // 1:30
  {"x": 712, "y": 384},  // Right (3 o'clock)
  {"x": 653, "y": 525},  // 4:30
  {"x": 512, "y": 584},  // Bottom (6 o'clock)
  {"x": 371, "y": 525},  // 7:30
  {"x": 312, "y": 384},  // Left (9 o'clock)
  {"x": 371, "y": 243},  // 10:30
  {"x": 512, "y": 184}   // Back to top
]
```

### Complex Patterns

You can combine multiple shapes by connecting them with points:

```json
{
  "name": "Complex Spell",
  "pattern": [
    // First: Circle
    {"x": 512, "y": 184},
    {"x": 712, "y": 384},
    {"x": 512, "y": 584},
    {"x": 312, "y": 384},
    {"x": 512, "y": 184},
    // Then: Line through center
    {"x": 512, "y": 384},
    {"x": 512, "y": 668}
  ]
}
```

## Troubleshooting

### Spell Not Loading

1. **Check file name**: Must be exactly `spells.json` (lowercase)
2. **Check location**: Must be in root directory of SD card (`/spells.json`)
3. **Check JSON syntax**: Use a JSON validator online
4. **Check serial output**: Connect via USB and check for error messages

### Pattern Not Matching

1. **Too few points**: Patterns need at least 2-3 points
2. **Points too close**: Spread points out for better recognition
3. **Wrong coordinates**: Check that x is 0-1024 and y is 0-768
4. **Pattern too complex**: Start simple and add complexity gradually

### Image Not Showing

1. **Check file format**: Must be 24-bit uncompressed BMP
2. **Check file name**: Should match `imageFile` in JSON
3. **Check location**: Images should be in root directory of SD card
4. **File too large**: Keep images reasonably sized (< 1MB recommended)

## Serial Debug Output

When the wand boots, you'll see messages like:

```
Successfully loaded /spells.json
  Renamed 'Ignite' to 'Fire Spell'
  Custom image for 'Fire Spell': my_fire.bmp
  Redefined pattern for 'Unlock' with 5 points
  Added custom spell 'Disarm' with 2 points
Custom spell configuration applied. Total spells: 12
```

## Example Configurations

### Example 1: Rename All Spells to Latin

```json
{
  "modify": [
    {"builtInName": "Unlock", "customName": "Aperio"},
    {"builtInName": "Ignite", "customName": "Ignis"},
    {"builtInName": "Gust", "customName": "Ventulus"}
  ]
}
```

### Example 2: Add New Spells Only

```json
{
  "custom": [
    {
      "name": "Illuminate",
      "pattern": [
        {"x": 512, "y": 600},
        {"x": 512, "y": 200}
      ]
    },
    {
      "name": "Dark",
      "pattern": [
        {"x": 512, "y": 200},
        {"x": 512, "y": 600}
      ]
    }
  ]
}
```

### Example 3: Complete Custom Spell

```json
{
  "modify": [
    {
      "builtInName": "Ignite",
      "customName": "Fireball",
      "imageFile": "fireball.bmp",
      "pattern": [
        {"x": 200, "y": 600},
        {"x": 512, "y": 200},
        {"x": 824, "y": 600}
      ]
    }
  ],
  "custom": [
    {
      "name": "Lightning",
      "imageFile": "lightning.bmp",
      "pattern": [
        {"x": 512, "y": 100},
        {"x": 400, "y": 300},
        {"x": 600, "y": 300},
        {"x": 500, "y": 500},
        {"x": 650, "y": 500},
        {"x": 512, "y": 700}
      ]
    }
  ]
}
```

## Technical Details

- **File size limit**: 16KB maximum for `spells.json`
- **Max pattern points**: Limited by available memory (typically 50+ points per spell)
- **Case sensitivity**: Spell names are case-insensitive for matching
- **Processing order**: Built-in spells loaded first, then modifications applied, then custom spells added

## SD Card File Structure Example

```
/
├── spells.json             # Your custom configuration
├── ignite.bmp              # Default image for Ignite
├── unlock.bmp              # Default image for Unlock
├── my_fire.bmp             # Custom image referenced in JSON
├── disarm.bmp              # Custom spell image
└── lightning.bmp           # Another custom image
```


