# PropMakerEscapeRoom Dependencies

This document lists all the dependencies required to use the **PropMakerEscapeRoom** library.

## Automatic Installation (Arduino Library Manager)

When installing this library via the Arduino Library Manager, the following dependencies are
installed automatically:

| Library | Name in Library Manager |
|------------|------------------------|
| ArduinoJson | `ArduinoJson` |
| IoAbstraction | `IoAbstraction` |
| TaskManagerIO | `TaskManagerIO` |
| Adafruit NeoPixel | `Adafruit NeoPixel` |
| Adafruit LED Backpack Library | `Adafruit LED Backpack Library` |
| Adafruit LIS3DH | `Adafruit LIS3DH` |
| Adafruit BusIO | `Adafruit BusIO` |
| Adafruit PN532 | `Adafruit PN532` |
| ADS1115_WE | `ADS1115_WE` |
| PCA9685 | `PCA9685` |
| Servo | `Servo` |

No version pins are declared for the RP2040 target: this list mirrors the `depends=` line of
`library.properties`, and the RP2040 build is not covered by CI yet.

### Port expanders

The expander accessors (`hw.portExpander()`, `hw.io()`, `hw.inputOutput()`) are typed
`PCF8574IoAbstraction*`: the supported expanders are the **PCF8574 / PCF8575** driven through
IoAbstraction, addressed by I²C. `hw.addPcf8574(0x20)` / `hw.addPcf8575(0x21)` register them
explicitly, and the boot scan reports what it found.

## Mandatory Manual Installation

Three libraries are **NOT** installable from the Arduino Library Manager and must be installed
manually.

### arduino-audio-tools (audio pipeline)

**Why:** `AudioPlayer` includes `AudioTools.h`, `AudioTools/AudioCodecs/CodecMP3Helix.h` and
`AudioTools/CoreAudio/AudioEffects/SoundGenerator.h`. The library is published on GitHub only;
`arduino-cli lib install arduino-audio-tools` fails with `Library 'arduino-audio-tools@latest'
not found`.

**Installation:**
1. Download the ZIP: https://github.com/pschatzmann/arduino-audio-tools/archive/refs/heads/main.zip
2. Arduino IDE: Sketch > Include Library > Add .ZIP Library
3. Select the downloaded file

Or via command:
```bash
cd <arduino>/libraries
git clone https://github.com/pschatzmann/arduino-audio-tools.git
```

### arduino-libhelix (MP3 Decoder)

**Why:** Helix MP3 decoder required for audio playback.

**Installation:**
1. Download the ZIP: https://github.com/pschatzmann/arduino-libhelix/archive/refs/heads/main.zip
2. Arduino IDE: Sketch > Include Library > Add .ZIP Library
3. Select the downloaded file

Or via command:
```bash
cd <arduino>/libraries
git clone https://github.com/pschatzmann/arduino-libhelix.git
```

### RotaryDialUnified (pulse dial) — NOT in the Library Manager

**Why:** `RotaryDialReader` needs the pulse-dial library with this API:

```cpp
RotaryDialUnified::begin(pin, ROTARYDIAL_UNIFIED_MECHANICAL | ROTARYDIAL_UNIFIED_OPTICAL);
RotaryDialUnified::setDebounce(ms); setMaxGap(ms); setDigitEnd(ms); reset();
```

It is not published in the Arduino Library Manager, so `arduino-cli lib install RotaryDialUnified`
finds nothing. The Manager does have a `RotaryDial` 0.0.1 by Tomas Mudrunka — the original,
mechanical-only project this library derives from — and it is a different project: no
optical-encoder support and not this API. Installing it does not provide our header, and the build
stops on the first include:

```
sketch.ino:1:10: fatal error: RotaryDialUnified.h: No such file or directory
```

The library this code needs is the mechanical/optical one. Clone it:

```bash
cd <arduino>/libraries
git clone https://github.com/djairjr/RotaryDialUnified.git
```

## Installation Verification

After installing all the dependencies, compile the `LibrarySmokeTest` example:

```
Arduino IDE: File > Examples > PropMakerEscapeRoom > Hardware_Tests > LibrarySmokeTest
```

If it compiles without errors, all dependencies are correctly installed.

## Important Notes

- **TaskManagerIO** is installed automatically as a dependency of **IoAbstraction**, but it is essential for the keypad and switches to work
- **arduino-libhelix** uses ~20KB of RAM during MP3 decoding - do not remove the `hw.sound().loop()` calls from the main `loop()`
