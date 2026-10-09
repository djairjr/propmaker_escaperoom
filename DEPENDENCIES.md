# PropMakerEscapeRoom Dependencies

This document lists all the dependencies required to use the **PropMakerEscapeRoom** library.

## Automatic Installation (Arduino Library Manager)

When installing this library via the Arduino Library Manager, the following dependencies are installed automatically:

| Library | Name in Library Manager | Version |
|------------|------------------------|--------|
| ArduinoJson | `ArduinoJson` | ^7.0.0 |
| IoAbstraction | `IoAbstraction` | ^3.0.0 |
| TaskManagerIO | `TaskManagerIO` | ^3.0.0 |
| arduino-audio-tools | `arduino-audio-tools` | ^1.0.0 |
| Adafruit NeoPixel | `Adafruit NeoPixel` | ^1.0.0 |
| Adafruit LED Backpack Library | `Adafruit LED Backpack Library` | ^1.0.0 |
| Adafruit LIS3DH | `Adafruit LIS3DH` | ^1.0.0 |
| Adafruit BusIO | `Adafruit BusIO` | ^1.0.0 |
| Adafruit AW9523 | `Adafruit AW9523` | ^1.0.0 |
| Adafruit MPR121 | `Adafruit MPR121` | ^1.0.0 |
| ADS1X15 | `ADS1X15` | ^2.0.0 |
| PCA9685 | `PCA9685` | ^0.0.1 |
| Servo | `Servo` | ^1.0.0 |
| RotaryDial | `RotaryDial` | ^1.0.0 |

### Notes on IoAbstraction Devices

- **MCP23017**: Native support via IoAbstraction (already included automatically)
- **PGA2310**: Support via IoAbstraction (may need additional configuration)

## Mandatory Manual Installation

The following library is **NOT available** in the Arduino Library Manager and must be installed manually:

### arduino-libhelix (MP3 Decoder)

**Why:** MP3 decoder required for audio playback.

**Installation:**
1. Download the ZIP: https://github.com/pschatzmann/arduino-libhelix/archive/refs/heads/main.zip
2. Arduino IDE: Sketch > Include Library > Add .ZIP Library
3. Select the downloaded file

Or via command:
```bash
cd ~/Documents/Arduino/libraries
git clone https://github.com/pschatzmann/arduino-libhelix.git
```

## Installation Verification

After installing all the dependencies, compile the `LibrarySmokeTest` example:

```
Arduino IDE: File > Examples > PropMakerEscapeRoom > LibrarySmokeTest
```

If it compiles without errors, all dependencies are correctly installed.

## Important Notes

- **TaskManagerIO** is installed automatically as a dependency of **IoAbstraction**, but it is essential for the keypad and switches to work
- **arduino-libhelix** uses ~20KB of RAM during MP3 decoding - do not remove the `hw.sound().loop()` calls from the main `loop()`
