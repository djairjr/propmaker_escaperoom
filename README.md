# PropMaker Escape Room Engine (RP2040)

Engine library for Escape Room puzzles based on the **Adafruit PropMaker Feather RP2040**(https://learn.adafruit.com/adafruit-rp2040-prop-maker-feather/overview) platform.

[![Version](https://img.shields.io/badge/version-2.0.0-blue)]()
[![Platform](https://img.shields.io/badge/platform-RP2040-orange)]()

---

## Index

- [Installation](#installation)
- [Architecture](#architecture)
- [Design notes](#design-notes)
- [Supported Hardware](#supported-hardware)
- [Full API Reference](#full-api-reference)
- [Usage Examples](#usage-examples)
- [FreeRTOS](#freertos)
- [Audio Format](#audio-format)
- [Troubleshooting](#troubleshooting)
- [Support](#support)

---

## Installation

### Required Dependencies

Install via **Arduino Library Manager** (Sketch > Include Library > Manage Libraries):

| Library | Name in Manager |
|------------|---------------------|
| ArduinoJson | `ArduinoJson` |
| IoAbstraction | `IoAbstraction` |
| TaskManagerIO | `TaskManagerIO` |
| Adafruit LIS3DH | `Adafruit LIS3DH` |
| Adafruit NeoPixel | `Adafruit NeoPixel` |
| Adafruit LED Backpack Library | `Adafruit LED Backpack Library` |
| Adafruit BusIO | `Adafruit BusIO` |
| Adafruit PN532 | `Adafruit PN532` |
| ADS1115_WE | `ADS1115_WE` |
| PCA9685 | `PCA9685` |
| Servo | `Servo` |

Three dependencies have to be installed by hand, all of them **not** installable from the
Arduino Library Manager:

- **`arduino-audio-tools`** — the audio pipeline (`AudioTools.h`, MP3 over I2S).
  Clone it from <https://github.com/pschatzmann/arduino-audio-tools>:

  ```
  cd <arduino>/libraries
  git clone https://github.com/pschatzmann/arduino-audio-tools.git
  ```

- **`arduino-libhelix`** — the MP3 decoder used by the audio pipeline, from
  <https://github.com/pschatzmann/arduino-libhelix>, with the same procedure.

- **`RotaryDialUnified`** — the pulse-dial library behind `RotaryDialUnifiedReader`
  (`begin(pin, ROTARYDIAL_UNIFIED_MECHANICAL | ROTARYDIAL_UNIFIED_OPTICAL)`, `setDebounce`, `setMaxGap`,
  `setDigitEnd`, `reset`), which reads both the mechanical dial and the optical encoder disc.
  It is **not in the Arduino Library Manager**. The Manager does have a `RotaryDial` (0.0.1) — the
  original mechanical-only project this one derives from — but it has neither the optical-encoder
  support nor this API, and installing it does not provide our header: the build stops with
  `fatal error: RotaryDialUnified.h: No such file or directory`. Clone the right one:

  ```
  cd <arduino>/libraries
  git clone https://github.com/djairjr/RotaryDialUnified.git
  ```

### Library Installation

1. Clone or download this repository
2. Move it to `Documents/Arduino/libraries/propmaker_escaperoom`
3. Restart the Arduino IDE

### Installation Verification

Open the **LibrarySmokeTest** example and compile it:

```
Arduino IDE: File > Examples > PropMaker EscapeRoom > Hardware_Tests > LibrarySmokeTest
```

---

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    YOUR PUZZLE / SKETCH                      │
├─────────────────────────────────────────────────────────────┤
│                    LogicManager (Optional)                   │
│         Manages multiple puzzles + RS485 + Timer             │
├─────────────────────────────────────────────────────────────┤
│              Hardware Abstraction Layer (hw)                 │
│  ┌──────────┬──────────┬──────────┬──────────┬──────────┐   │
│  │  Audio   │ Display  │ NeoPixel │   ADC    │   PWM    │   │
│  │ I2S+MP3  │ HT16K33  │  WS2812  │ ADS1115  │ PCA9685  │   │
│  └──────────┴──────────┴──────────┴──────────┴──────────┘   │
│  ┌──────────┬──────────┬──────────┬──────────┬──────────┐   │
│  │  I/O     │  Accel.  │   Servo  │   RFID   │  RS485   │   │
│  │ PCF857x  │  LIS3DH  │  PWM HW  │  PN532   │  UART    │   │
│  └──────────┴──────────┴──────────┴──────────┴──────────┘   │
├─────────────────────────────────────────────────────────────┤
│                    Arduino RP2040 Core                       │
│              FreeRTOS SMP (Optional)                         │
└─────────────────────────────────────────────────────────────┘
```

### Minimal Usage

```cpp
#include <PropMakerEscapeRoom.h>

using namespace propmaker_escaperoom;

void setup() {
    hw.begin();  // Initializes all detected hardware
    hw.sound().setVolume(200);
}

void loop() {
    hw.sound().loop();  // Processes audio (mandatory)
    // Your logic here
}
```

---

## Design notes

### An I2C-first build

A puzzle needs far more inputs and outputs than the board has GPIO pins: buttons, sensors,
locks, relays, LEDs, keypads, a display. This library is therefore designed for a build wired
around **I2C** rather than around the few free pins — two pins (SDA/SCL) can carry the whole
installation: the I/O expanders (PCF8574/PCF8575 through IoAbstraction), the HT16K33 display,
the PCA9685 PWM driver behind servos and dimmers, the ADS1115 analog inputs, the PN532 NFC
reader and the LIS3DH accelerometer are all I2C devices.

`Hardware::begin()` runs `Wire.begin()` and then a full I2C scan, and every `hw.hasX()` flag
comes from that scan, so a sketch adapts to the hardware that is physically there; the full
address map is under [Supported Hardware](#supported-hardware). Adding a peripheral costs an
I2C address, not a GPIO pin — which is what keeps a puzzle box buildable once the board has
run out of free pins.

### Home Assistant

This library has no MQTT/Home Assistant stack of its own: on the RP2040 the puzzles talk to
each other over the RS485 bus (`hw.network()`, `Rs485Bus`, `Rs485Payload`). The Home
Assistant integration lives in the ESP32 sibling library,
[`esp32_escaperoom`](https://github.com/djairjr/esp32_escaperoom), which replaces the RS485
bus with MQTT and HA auto-discovery for the same purpose: so that the **game automation**
(puzzle state, locks, timers, sequences) and the **room automation** (cameras, lights, sound,
doors) can be managed from a single place, the puzzle being just another device of the room.

The room-level view of this — what the integration is for, what the Home Assistant Green is,
how the integration works in practice, and how the rooms are organized in HA — is in
[`docs/HOME_ASSISTANT_IN_ESCAPE_ROOMS.md`](docs/HOME_ASSISTANT_IN_ESCAPE_ROOMS.md).

---

## Supported Hardware

### Automatic I2C Detection

The library runs a **verbose boot** that automatically detects devices:

| Device | I2C Address | Verification Method |
|-------------|--------------|----------------------|
| LIS3DH Accelerometer | 0x18, 0x19 | `hw.hasAccelerometer()` |
| PCF8574 I/O Expander | 0x20-0x27 | `hw.hasPortExpander()` |
| PCF8574A I/O Expander | 0x38-0x3F | `hw.hasPortExpander()` |
| PCF8575 I/O Expander | 0x20-0x27 | `hw.hasPortExpander()` |
| PCA9685 PWM Driver | 0x40-0x44 | `hw.hasPwmDriver()` |
| ADS1115 ADC | 0x48-0x4B | `hw.hasAdc()` |
| HT16K33 Display | 0x70 | `hw.hasDisplay()` |
| PCF8563 RTC | 0x51 | `hw.hasRtc()` |
| SSD1306 OLED | 0x3C, 0x3D | `hw.hasOled()` |

### SPI

| Device | CS Pin | Method |
|-------------|--------|--------|
| SD Card | 9 | `hw.isSDMounted()` |
| LittleFS | Internal | `hw.isLittleFSMounted()` |

### PropMaker Feather RP2040 Pinout

```
I2S Audio:
  BCLK  → GPIO 16
  WS    → GPIO 17
  DOUT  → GPIO 18

NeoPixel:
  Internal → GPIO 21
  External → GPIO 19

I2C:
  SDA → GPIO 2
  SCL → GPIO 3

SPI (SD Card):
  CS    → GPIO 9
  SCK   → GPIO 10
  MOSI  → GPIO 11
  MISO  → GPIO 8

Servo PWM:
  PWM   → GPIO 20 (PIN_SERVO_PWM)

RS485:
  DE/RE → GPIO 6
  TX    → GPIO 8
  RX    → GPIO 9

Power Enable:
  EXTERNAL_POWER → GPIO 23
```

---

## Full API Reference

### Hardware (Global Singleton)

Access via the global instance `hw`:

#### Initialization and Diagnostics

```cpp
hw.begin();                    // Initializes all detected hardware
hw.test();                     // Runs full diagnostic tests
bool ok = hw.isSystemHealthy(); // true if critical components are OK
```

#### Checking Detected Hardware

```cpp
bool hasAccel = hw.hasAccelerometer();   // LIS3DH present?
bool hasDisplay = hw.hasDisplay();       // HT16K33 present?
bool hasPortExp = hw.hasPortExpander();  // PCF857x present?
bool hasPwm = hw.hasPwmDriver();         // PCA9685 present?
bool hasAdc = hw.hasAdc();               // ADS1115 present?
bool hasRtc = hw.hasRtc();               // PCF8563 present?
bool hasOled = hw.hasOled();             // SSD1306 present?
bool sdMounted = hw.isSDMounted();       // SD Card mounted?
bool lfsMounted = hw.isLittleFSMounted();// LittleFS mounted?
```

---

### AudioPlayer

MP3 audio playback control via hardware I2S.

#### Main Methods

| Method | Parameters | Return | Description |
|--------|------------|---------|-----------|
| `setVolume` | `uint8_t volume` (0-255) | `void` | Set the audio volume |
| `setSynchronous` | `bool sync` | `void` | Sets synchronous/asynchronous mode |
| `playOnce` | `const char* path`, `bool useSD=false` | `bool` | Plays once (asynchronous) |
| `playOnceBlocking` | `const char* path`, `bool useSD=false` | `bool` | Plays once (synchronous/blocking) |
| `playLoop` | `const char* path`, `bool useSD=false` | `bool` | Plays in a continuous loop |
| `isPlaying` | none | `bool` | true if audio is playing |
| `stop` | none | `void` | Stops playback |
| `loop` | none | `void` | Processes the audio buffer (call in loop()) |
| `mounted` | none | `bool` | true if storage is available |

#### Example: Volume Configuration

```cpp
void setup() {
    hw.begin();
    hw.sound().setVolume(200);  // 0-255 (200 = ~78%)
}
```

#### Example: Keypad Sound Effect (Synchronous)

```cpp
void onKeyPressed() {
    // Blocks until the sound finishes for perfect synchronization
    hw.sound().playOnceBlocking("/sfx/beep.mp3");
}
```

#### Example: Background Music (Asynchronous)

```cpp
void setup() {
    hw.begin();
    hw.sound().playLoop("/music/ambient.mp3");  // Does not block
}

void loop() {
    hw.sound().loop();  // Processes the audio buffer
    // Your logic keeps running
}
```

#### Example: Audio Sequence with Timing

```cpp
void dramaticSequence() {
    hw.sound().playOnceBlocking("/sfx/arming.mp3");
    delay(500);  // Dramatic pause
    hw.sound().playOnceBlocking("/sfx/tick.mp3");
}
```

#### Example: Status Check

```cpp
void update() {
    if (hw.sound().isPlaying()) {
        Serial.println("Audio playing...");
    }
    
    if (!hw.sound().mounted()) {
        Serial.println("Error: No storage for audio!");
    }
}
```

---

### Display14Seg (HT16K33)

Driver for a 4-digit 14-segment display.

#### Main Methods

| Method | Parameters | Return | Description |
|--------|------------|---------|-----------|
| `printTime` | `int min`, `int sec` | `void` | Displays MM:SS with a colon |
| `printString` | `const char* str` | `void` | Displays text (4 characters max) |
| `printNum` | `int num` | `void` | Displays an integer number |
| `setBrightness` | `uint8_t level` (0-15) | `void` | Sets the display brightness |
| `setColon` | `bool on` | `void` | Turns the colon on/off |

#### Example: Bomb Timer

```cpp
void updateTimer(int totalSeconds) {
    int min = totalSeconds / 60;
    int sec = totalSeconds % 60;
    
    hw.display().printTime(min, sec);
    
    // Blinks in the last 10 seconds
    if (totalSeconds <= 10) {
        if (millis() % 500 < 250) {
            hw.display().setBrightness(15);  // Maximum
        } else {
            hw.display().setBrightness(2);   // Minimum
        }
    }
}
```

#### Example: Status Messages

```cpp
void showStatus(const char* msg) {
    hw.display().printString(msg);  // E.g.: "HELP", "OPEn", "Err"
    hw.display().setBrightness(8);  // Medium brightness
}
```

#### Example: Counter

```cpp
void showScore(int points) {
    hw.display().printNum(points);  // E.g.: 42 → "  42"
}
```

---

### NeoPixel (WS2812 LEDs)

Control of addressable RGB LED strips.

#### Main Methods

| Method | Parameters | Return | Description |
|--------|------------|---------|-----------|
| `setLight` | `uint8_t index`, `Color c` | `void` | Sets the external LED color |
| `setLightAll` | `Color c` | `void` | Sets the color of all external LEDs |
| `setLightBrightness` | `uint8_t b` (0-255) | `void` | Sets the external brightness |
| `setLightInternal` | `uint8_t index`, `Color c` | `void` | Sets the internal LED color |
| `setLightInternalAll` | `Color c` | `void` | Sets the color of all internal LEDs |
| `pixelsExternal` | none | `Adafruit_NeoPixel&` | Direct access to the instance |
| `pixelsInternal` | none | `Adafruit_NeoPixel&` | Direct access to the instance |
| `applyGamma` | `uint8_t val` | `uint8_t` | Applies gamma correction |

#### Example: Basic Colors

```cpp
// Single internal LED
hw.setLightInternal(0, hw.pixelsInternal().Color(255, 0, 0));  // Red
hw.setLightInternal(0, hw.pixelsInternal().Color(0, 255, 0)); // Green
hw.setLightInternal(0, hw.pixelsInternal().Color(0, 0, 255)); // Blue

// External LEDs (8 LEDs)
hw.setLight(0, hw.pixelsExternal().Color(0, 255, 0));  // Green on LED 0
hw.setLightAll(hw.pixelsExternal().Color(0, 0, 255));  // Blue on all
hw.setLightAll(0);  // Turns all off
```

#### Example: Pulse Effect (Sine Wave)

```cpp
float intensity = getSine(2.5, 10, 100);  // freq, min, max
hw.setLightAll(hw.pixelsExternal().Color(0, intensity/4, intensity));
```

#### Example: Progress Bar

```cpp
void updateProgress(int count, int total) {
    int ledsOn = map(count, 0, total, 0, 8);
    
    for (int i = 0; i < 8; i++) {
        if (i < ledsOn) {
            hw.setLight(i, hw.pixelsExternal().Color(0, 50, 0));
        } else {
            hw.setLight(i, 0);
        }
    }
}
```

#### Example: Gamma Correction

```cpp
// Adjusts perceived brightness for more natural colors
uint8_t corrected = hw.applyGamma(100);  // Input 0-255
hw.setLightAll(hw.pixelsExternal().Color(corrected, 0, 0));
```

---

### Input/Output Expander (PCF8574/8575 via IoAbstraction)

Reading and writing of digital pins via I2C.

#### ⚠️ PCF8575 Pin Mapping (IMPORTANT)

The physical PCF8575 module has pins labeled **P0-P7** and **P10-P17**, but IoAbstraction treats them as a sequential array of 16 positions (0-15):

| IoAbstraction | Physical Pin | Typical Connection |
|---------------|-------------|----------------|
| 0-7 | P0-P7 | Port 0 (available) |
| **8** | **P10** | **LEFT Button** |
| **9** | **P11** | **SELECT Button** |
| **10** | **P12** | **RIGHT Button** |
| **11** | **P13** | **DET Button** |
| **12** | **P14** | **Wire 1** |
| **13** | **P15** | **Wire 2** |
| **14** | **P16** | **Wire 3** |
| **15** | **P17** | **Wire 4** |

**Common mistake:** Using pins 16 and 17, which do not exist on the PCF8575! The module shows P16-P17 physically, but in IoAbstraction they are 14-15.

```cpp
// Correct:
#define BUTTON_LEFT_PIN     8   // Physical P10
#define BUTTON_SELECT_PIN   9   // Physical P11
#define WIRE1_PIN           12  // Physical P14 (not 16!)

// WRONG (causes unexpected behavior):
#define WIRE1_PIN           16  // Does not exist on the PCF8575!
```

#### Main Methods (via `hw.inputOutput()` or `hw.portExpander()`)

| Method | Parameters | Return | Description |
|--------|------------|---------|-----------|
| `digitalRead` | `uint8_t pin` | `uint8_t` | Reads the pin state (HIGH/LOW) |
| `digitalWrite` | `uint8_t pin`, `uint8_t val` | `void` | Writes the pin state |
| `pinMode` | `uint8_t pin`, `uint8_t mode` | `void` | Configures the pin direction |
| `readValue` | `uint8_t pin` | `uint8_t` | Reads the value directly from the pin |

**Note:** Use `->` to access methods (pointer):
```cpp
hw.inputOutput()->digitalRead(0);  // CORRECT
hw.inputOutput().digitalRead(0);   // WRONG
```

#### Example: Detecting an Active Sensor

```cpp
// LOW = active sensor (common on PCF expanders)
bool isSensorActive(uint8_t pin) {
    return hw.inputOutput()->readValue(pin) == LOW;
}

void checkAllSensors() {
    bool s1 = isSensorActive(0);
    bool s2 = isSensorActive(1);
    bool s3 = isSensorActive(2);
    
    if (s1 && s2 && s3) {
        // All sensors activated - solve puzzle
    }
}
```

#### Example: Connection Test (Connect Wires Puzzle)

```cpp
bool checkPair(uint8_t outPin, uint8_t inPin) {
    // Pulls outPin to LOW and checks whether inPin also goes low
    hw.inputOutput()->digitalWrite(outPin, LOW);
    delayMicroseconds(100);  // Waits to stabilize
    
    bool connected = (hw.inputOutput()->digitalRead(inPin) == LOW);
    
    hw.inputOutput()->digitalWrite(outPin, HIGH);  // Restores
    return connected;
}
```

#### Example: Output Control

```cpp
// Turns on the relay on pin 0 of the expander
hw.inputOutput()->digitalWrite(0, LOW);   // Activates (LOW = active)
hw.inputOutput()->digitalWrite(0, HIGH);  // Deactivates
```

---

### ADS1115 ADC

Precision analog voltage reading (0-5V, 16-bit).

#### Main Methods

| Method | Parameters | Return | Description |
|--------|------------|---------|-----------|
| `readVoltage` | `uint8_t channel` (0-3) | `float` | Voltage in millivolts |
| `readAnalogRaw` | `uint8_t channel` (0-3) | `uint16_t` | Raw value (0-65535) |

#### Example: Voltmeter

```cpp
void readSensors() {
    float mv = hw.readVoltage(0);      // Channel 0 in mV
    float volts = mv / 1000.0f;        // Converts to volts
    uint16_t raw = hw.readAnalogRaw(0); // Raw value
    
    Serial.print("Channel 0: ");
    Serial.print(volts);
    Serial.print("V (raw: ");
    Serial.print(raw);
    Serial.println(")");
}
```

#### Example: Threshold Detection

```cpp
void checkThreshold() {
    if (hw.readVoltage(0) > 2500) {  // 2.5V
        // Signal detected - activates output
        hw.digitalWrite(LOCK_PIN, HIGH);
    }
}
```

---

### PCA9685 PWM Driver

Control of multiple PWM channels (0-100% duty cycle).

#### Main Methods

| Method | Parameters | Return | Description |
|--------|------------|---------|-----------|
| `setChannelDutyCycle` | `uint8_t ch`, `float duty` | `void` | Sets the duty cycle (0-100%) |
| `setAnalogOutput` | `uint8_t ch`, `float percent` | `void` | Alias for setChannelDutyCycle |

#### Example: Servo Control via PCA9685

```cpp
// Channel 0 at 50% (approximately 90 degrees for a standard servo)
hw.setAnalogOutput(0, 50.0);

// Maps angle 0-180 to 0-100%
void setServoAngle(uint8_t channel, int angle) {
    float percent = map(angle, 0, 180, 0, 100) / 100.0f;
    hw.setAnalogOutput(channel, percent);
}
```

#### Example: LED Dimmer

```cpp
void setLedBrightness(uint8_t channel, uint8_t brightness) {
    float percent = brightness / 255.0f * 100.0f;
    hw.setAnalogOutput(channel, percent);
}
```

---

### Servo (Hardware PWM)

Servo motor control via a dedicated PWM pin (GPIO 20).

#### Method

```cpp
hw.setActuator(int angle);  // 0-180 degrees
```

#### Example: Test Sweep

```cpp
void servoSweep() {
    for (int angle = 0; angle <= 180; angle++) {
        hw.setActuator(angle);
        delay(15);  // Waits for the servo to move
    }
    delay(500);
    
    for (int angle = 180; angle >= 0; angle--) {
        hw.setActuator(angle);
        delay(15);
    }
}
```

#### Example: Electromechanical Lock

```cpp
void unlock() {
    hw.setActuator(90);  // Unlocked position
    delay(3000);         // Holds for 3 seconds
    hw.setActuator(0);   // Returns (locks)
}
```

---

### Accelerometer (LIS3DH)

3-axis acceleration reading.

#### Main Method

```cpp
hw.accelerometer().getEvent(&event);
```

Returns `sensors_event_t` with:
- `event.acceleration.x` - X Acceleration (m/s²)
- `event.acceleration.y` - Y Acceleration (m/s²)
- `event.acceleration.z` - Z Acceleration (m/s²)

#### Example: Full Reading

```cpp
sensors_event_t event;
hw.accelerometer().getEvent(&event);

float ax = event.acceleration.x;
float ay = event.acceleration.y;
float az = event.acceleration.z;

Serial.print("X: "); Serial.print(ax);
Serial.print(" Y: "); Serial.print(ay);
Serial.print(" Z: "); Serial.println(az);
```

#### Example: Motion Detection

```cpp
bool isMoving() {
    sensors_event_t event;
    hw.accelerometer().getEvent(&event);
    
    float mag = sqrt(
        event.acceleration.x * event.acceleration.x +
        event.acceleration.y * event.acceleration.y +
        event.acceleration.z * event.acceleration.z
    );
    
    // Outside the rest range (~1G = 9.8 m/s²)
    return mag > 12.0 || mag < 7.0;
}
```

---

### PuzzleBase

The base class for creating custom puzzles.

#### Lifecycle

```cpp
class MyPuzzle : public PuzzleBase {
public:
    MyPuzzle() : PuzzleBase(1) {}  // Unique puzzle ID
    
    void setupHardware() override {
        // Configures pins, actuators - called once
    }
    
    void update() override {
        // Main logic - called repeatedly
    }
};
```

#### States (PuzzleState)

```cpp
enum class PuzzleState {
    Inactive,      // Disabled
    Initialising,  // Initializing
    Idle,          // Waiting for start
    Running,       // In progress
    Solved,        // Solved
    Error,         // Error
    Disabled       // Disabled
};
```

#### Control Methods

| Method | Parameters | Return | Description |
|--------|------------|---------|-----------|
| `setState` | `PuzzleState state` | `void` | Changes the puzzle state |
| `getState` | none | `PuzzleState` | Returns the current state |
| `run` | none | `void` | Starts the puzzle (setup + Running) |

#### Effect Helpers

| Method | Parameters | Return | Description |
|--------|------------|---------|-----------|
| `getSine` | `float freq`, `float min`, `float max` | `float` | Sine wave for pulse |
| `getFlicker` | `float base`, `float intensity` | `float` | Random variation (flicker) |
| `delayAsync` | `unsigned long ms`, `void(*cb)()` | `void` | Non-blocking delay with callback |

#### Example: Complete Puzzle with All Features

```cpp
class AdvancedPuzzle : public PuzzleBase {
public:
    AdvancedPuzzle() : PuzzleBase(5) {}
    
    void setupHardware() override {
        hw.pinMode(LOCK_PIN, OUTPUT);
        hw.digitalWrite(LOCK_PIN, LOW);
        Serial.println("Puzzle 5 initialized");
    }
    
    void update() override {
        if (getState() == PuzzleState::Running) {
            // Smooth pulse effect
            float brightness = getSine(2.0, 20, 100);
            hw.setLightAll(hw.pixelsExternal().Color(0, brightness/4, brightness));
            
            // Checks sensor
            if (hw.inputOutput()->readValue(0) == LOW) {
                handleWin();
            }
            
            // 30-second timeout
            if (millis() - startTime_ > 30000) {
                setState(PuzzleState::Error);
            }
        }
    }
    
private:
    unsigned long startTime_ = 0;
    
    void handleWin() {
        hw.sound().playOnceBlocking("/sfx/unlock.mp3");
        hw.digitalWrite(LOCK_PIN, HIGH);
        setState(PuzzleState::Solved);
        
        // Callback after delay
        delayAsync(3000, [](){
            hw.setLightAll(hw.pixelsExternal().Color(0, 255, 0));
        });
    }
};
```

---

### LogicManager

Orchestrator for multiple puzzles with a global timer and RS485.

#### Main Methods

| Method | Parameters | Return | Description |
|--------|------------|---------|-----------|
| `begin` | none | `void` | Initializes hardware and puzzles |
| `update` | none | `void` | Updates all puzzles + timer |
| `addPuzzle` | `PuzzleBase* puzzle` | `void` | Adds a puzzle to the system |
| `startTimer` | `unsigned long seconds` | `void` | Starts the global timer |
| `stopTimer` | none | `void` | Pauses the timer |
| `getRemainingTime` | none | `unsigned long` | Remaining time (seconds) |
| `onTimerTick` | `function callback` | `void` | Callback every second |
| `onTimerEnd` | `function callback` | `void` | Callback when the timer reaches zero |
| `onPuzzleSolved` | `function callback` | `void` | Callback when a puzzle is solved |

#### Example: Basic Configuration

```cpp
LogicManager master(1);  // RS485 ID = 1
MyPuzzle puzzle1;
MyPuzzle puzzle2;

void setup() {
    master.addPuzzle(&puzzle1);
    master.addPuzzle(&puzzle2);
    master.begin();
    master.startTimer(300);  // 5 minutes
}

void loop() {
    master.update();
    delay(1);
}
```

#### Example: Event Callbacks

```cpp
void setup() {
    master.addPuzzle(&puzzle);
    master.begin();
    
    // Updates the display every second
    master.onTimerTick([](int remaining){
        int min = remaining / 60;
        int sec = remaining % 60;
        hw.display().printTime(min, sec);
    });
    
    // Plays an explosion sound when the timer reaches zero
    master.onTimerEnd([](){
        hw.sound().playOnceBlocking("/sfx/boom.mp3");
        hw.setLightAll(hw.pixelsExternal().Color(255, 0, 0));
    });
    
    // Logs when a puzzle is solved
    master.onPuzzleSolved([](int puzzleId){
        Serial.print("Puzzle solved: ");
        Serial.println(puzzleId);
    });
    
    master.startTimer(300);
}
```

---

### Rs485Bus

Serial communication between puzzles via the RS485 bus.

#### Event Types

```cpp
enum Rs485EventType {
    RS485_EVENT_NONE,
    RS485_EVENT_TIMER_START,
    RS485_EVENT_TIMER_STOP,
    RS485_EVENT_TIMER_TICK,
    RS485_EVENT_PUZZLE_SOLVED,
    RS485_EVENT_PUZZLE_RESET,
    RS485_EVENT_SOUND_PLAY,
    RS485_EVENT_ALL_SOLVED
};
```

#### Main Methods

| Method | Parameters | Return | Description |
|--------|------------|---------|-----------|
| `begin` | `uint8_t id` | `void` | Initializes the bus with an ID |
| `update` | none | `void` | Processes received messages |
| `sendEvent` | `Rs485EventType type`, `uint8_t data` | `void` | Sends an event |
| `available` | none | `bool` | true if a message is pending |
| `readEvent` | none | `Rs485Event` | Reads an event from the queue |

#### Example: Sending an Event

```cpp
Rs485Bus bus;

void setup() {
    bus.begin(1);  // This puzzle is ID 1
}

void onPuzzleSolved() {
    // Notifies the master that this puzzle was solved
    bus.sendEvent(RS485_EVENT_PUZZLE_SOLVED, 1);
}
```

#### Example: Receiving an Event

```cpp
void loop() {
    bus.update();
    
    if (bus.available()) {
        Rs485Event evt = bus.readEvent();
        
        switch (evt.eventType) {
            case RS485_EVENT_TIMER_START:
                master.startTimer(evt.data * 60);  // data = minutes
                break;
            case RS485_EVENT_ALL_SOLVED:
                hw.setLightAll(hw.pixelsExternal().Color(0, 255, 0));
                break;
        }
    }
}
```

---

### RotaryEncoder

Incremental rotary encoder reading.

#### Methods

```cpp
RotaryEncoder encoder;

encoder.begin(pinA, pinB);     // Initializes with DT, CLK pins
encoder.update();              // Call in loop to read the encoder
int pos = encoder.getPosition(); // Current position
int dir = encoder.getDirection(); // 1 = clockwise, -1 = counter-clockwise
```

#### Example: Value Adjustment

```cpp
RotaryEncoder encoder;
int value = 0;

void setup() {
    encoder.begin(20, 21);  // DT=GPIO20, CLK=GPIO21
}

void loop() {
    encoder.update();
    
    if (encoder.getDirection() == 1) {
        value++;  // Turned clockwise
    } else if (encoder.getDirection() == -1) {
        value--;  // Turned counter-clockwise
    }
    
    hw.display().printNum(value);
}
```

---

## FreeRTOS

### Configuration

1. **Enable it in the IDE menu:** Tools > RP2040 Boards > "FreeRTOS SMP"
2. **Or via arduino-cli:** Add the flag `-D__FREERTOS`

### Recommended Task Structure

```cpp
// Audio task - Core 1, high priority
void audioTask(void *param) {
    while (1) {
        hw.sound().loop();
        vTaskDelay(pdMS_TO_TICKS(1));  // 1ms
    }
}

// Logic task - Core 0, normal priority
void logicTask(void *param) {
    while (1) {
        master.update();
        rs485.update();
        vTaskDelay(pdMS_TO_TICKS(1));  // 1ms
    }
}

void setup() {
    hw.begin();
    
    // Audio: 16KB stack, priority 2 (higher)
    xTaskCreate(audioTask, "Audio", 16384, NULL, 2, NULL);
    
    // Logic: 8KB stack, priority 1
    xTaskCreate(logicTask, "Logic", 8192, NULL, 1, NULL);
}

void loop() {
    // Empty - tasks run independently on both cores
}
```

### Task Settings

| Task   | Stack    | Priority | Core | Description |
|--------|----------|------------|------|-----------|
| Audio  | 16384    | 2          | 1    | MP3 Decoder + I2S DMA |
| Logic  | 8192     | 1          | 0    | Puzzle update + RS485 |
| Display| 4096     | 1          | 0    | Display update (optional) |

### Operation without FreeRTOS

For testing and development, the library works without explicit FreeRTOS:

```cpp
void setup() {
    hw.begin();
    puzzle.setupHardware();
    puzzle.setState(PuzzleState::Running);
}

void loop() {
    hw.sound().loop();  // Processes audio via I2S DMA
    puzzle.update();
    delay(1);
}
```

---

## Audio Format

### Supported MP3 Specifications

| Parameter | Required Value |
|-----------|-----------------|
| Codec | MP3 (MPEG Layer 3) |
| Sample Rate | 44100 Hz |
| Channels | 2 (Stereo) or 1 (Mono) |
| Bitrate | 128 kbps (recommended) |
| ID3 Tags | **NOT** supported |

### Conversion with FFmpeg

```bash
# Removes ID3 tags and re-encodes to a compatible format
ffmpeg -y -i input.mp3 \
  -acodec libmp3lame \
  -ar 44100 \
  -ac 2 \
  -b:a 128k \
  -map_metadata -1 \
  output.mp3
```

### Verification with ffprobe

```bash
ffprobe -v error \
  -show_entries stream=codec_name,sample_rate,bit_rate \
  file.mp3
```

Expected output:
```
codec_name=mp3
sample_rate=44100
bit_rate=128000
```

---

## Included Examples

| Example | Description | Difficulty |
|---------|-----------|-------------|
| LibrarySmokeTest | Full hardware diagnostics | Beginner |
| BombTimer | Bomb timer with display and keypad | Intermediate |
| KeypadCodeEntry | Code puzzle with keypad | Beginner |
| TripleSensorLock | 3 ball sensors + knob | Intermediate |
| ConnectWires | Wire connection with voltmeter | Intermediate |
| EscapeRoomPhone | Rotary dial phone | Advanced |
| CircuitActivation | Sequential circuit activation | Beginner |

---

## Troubleshooting

### Audio with Noise or Static

**Problem:** Audio plays but with background noise or distorted.

**Solution:**
1. Check the MP3 format:
   ```bash
   ffprobe file.mp3
   ```
2. Re-encode without ID3 tags:
   ```bash
   ffmpeg -i input.mp3 -acodec libmp3lame -ar 44100 -b:a 128k -map_metadata -1 output.mp3
   ```
3. Check the I2S connections (GPIO 16, 17, 18)

### Hardware Not Detected at Boot

**Problem:** `hw.has*()` returns false for a connected device.

**Solution:**
1. Check the I2C power (3.3V, not 5V)
2. Check the addresses with an I2C scanner
3. Run `hw.test()` for detailed diagnostics
4. Check the pull-up resistors on the I2C bus (4.7kΩ recommended)

### Compilation Fails

**Problem:** Compilation errors related to external libraries.

**Solution:**
```bash
# Compile with arduino-cli for diagnostics
arduino-cli compile \
  --fqbn rp2040:rp2040:adafruit_feather_prop_maker \
  examples/Hardware_Tests/LibrarySmokeTest/LibrarySmokeTest.ino
```

Check that all dependencies are installed (see the [Installation](#installation) section).

### External NeoPixel Does Not Work

**Problem:** External LEDs do not light up.

**Solution:**
- Check that `PIN_EXTERNAL_POWER` (GPIO 23) is HIGH
- `hw.begin()` turns it on automatically, but verify with a multimeter
- Confirm adequate 5V power for the number of LEDs

---

## Support

This library is maintained by **Nicolau dos Brinquedos** (Djair Guilherme da Silva Junior).
If it saves you time in a build, you can buy me a coffee:
<https://buymeacoffee.com/nicolaudosbrinquedos>

---

## License

MIT License - See the LICENSE file at the root of the project.

---

## Useful Links

- [Adafruit PropMaker Documentation](https://learn.adafruit.com/adafruit-rp2040-prop-maker-feather)
- [arduino-audio-tools Documentation](https://github.com/pschatzmann/arduino-audio-tools)
- [arduino-libhelix (MP3 Decoder)](https://github.com/pschatzmann/arduino-libhelix)
- [IoAbstraction Library](https://github.com/TcMenu/IoAbstraction)

---

## Version History

### 1.0.0
- Initial implementation of the PropMaker EscapeRoom engine
- Full support for PropMaker Feather RP2040 hardware
- AudioPlayer with MP3 playback via I2S
- LogicManager with global timer and RS485
- PuzzleBase with organic effect helpers
- Functional examples for various scenarios
