# VintageShortwaveMorseRadio - Implemented Algorithm (PropMaker EscapeRoom)

## Overview

***VintageShortwaveMorseRadio** is a puzzle game that simulates a vintage shortwave radio. The player must connect a transformer and four valves to solve the puzzle. The third valve activates a virtual FM tuning system, allowing the player to explore 9 different radio stations. The fourth valve (FINAL) interrupts the FM signal and transmits the secret message in Morse code.

**Original Author:** Nicolau dos Brinquedos (2018)
**Original Client:** Escape Junior (Henrique Sitchin)  
**Port:** PropMaker EscapeRoom Library

**Architecture:**
- Based on the Adafruit PropMaker Feather RP2040
- Digital inputs via the PCF8575 I2C Expander
- I2S audio with real-time Morse tone generation
- External NeoPixel for visual feedback
- FM tuning system with a logarithmic volume array
- SD Card > LittleFS priority for station*.mp3 files

---

## Index

1. [Hardware](#hardware)
2. [Constants and Configuration](#constants-and-configuration)
3. [Main Flow](#main-flow)
4. [Game States](#game-states)
5. [FM System (Virtual Dial)](#fm-system-virtual-dial)
6. [Morse Transmission](#morse-transmission)
7. [Text Normalization](#text-normalization)
8. [Main Functions](#main-functions)

---

## Hardware

### PCF8575 - Pin Mapping

| PCF Pin | Function | Type | Description |
|----------|--------|------|-----------|
| P0 | Transformer | Input (INPUT_PULLUP) | Active LOW - connects to GND |
| P1 | Valve 1 | Input (INPUT_PULLUP) | Active LOW |
| P2 | Valve 2 | Input (INPUT_PULLUP) | Active LOW |
| P3 | Valve 3 | Input (INPUT_PULLUP) | Active LOW - activates FM |
| P4 | Encoder A | Input (INPUT_PULLUP) | Rotary Encoder channel A |
| P5 | Encoder B | Input (INPUT_PULLUP) | Rotary Encoder channel B |
| P6 | Encoder Btn | Input (INPUT_PULLUP) | Encoder button (reserved) |
| P7 | Valve 4 | Input (INPUT_PULLUP) | Active LOW - FINAL valve |

### External NeoPixel - Mapping

| Index | Function | Color |
|--------|--------|-----|
| 0-11 | Dial (unused) | - |
| 12 | Power LED | White |
| 13 | Morse LED | White (blinks) |
| 14 | Valve 1 LED | Blue |
| 15 | Valve 2 LED | Blue |
| 16 | Valve 3 LED | Blue |
| 17 | Trafo Hum LED | Pulsing |
| 18 | Valve 4 LED | Blue (FINAL) |

---

## Constants and Configuration

### Morse Parameters

```cpp
#define MORSE_DOT_MS        500     // Dot duration
#define MORSE_DASH_MS       1500    // Dash duration (3x dot)
#define MORSE_END_LETTER_MS 2000    // Pause between letters
#define MORSE_SPACE_MS      3500    // Pause between words
#define MORSE_FREQ_HZ       432     // Tone frequency
#define MORSE_VOLUME        128     // Volume (0-255)
#define MORSE_REPEAT        5       // Message repetitions
```

### FM System (Virtual Dialing)

```cpp
#define FM_NUM_STATIONS     9       // Total stations
#define FM_VOLUME_STEPS     80      // Steps in the volume array
#define FM_VOLUME_LOW       25      // ~10% of 255
#define FM_VOLUME_HIGH      204     // ~80% of 255

// Volume array - logarithmic scale (10% → 80% → 10%)
const uint8_t fmVolumeCurve[FM_VOLUME_STEPS] = {
    // Rising: indices 0-39 (25 → 147)
    25, 26, 27, 28, 30, 31, 33, 34, 36, 37,
    39, 41, 43, 45, 47, 49, 51, 53, 56, 58,
    61, 64, 67, 70, 73, 76, 80, 84, 88, 92,
    96, 101, 106, 111, 116, 122, 128, 134, 140, 147,
    // Falling: indices 40-79 (147 → 25)
    147, 140, 134, 128, 122, 116, 111, 106, 101, 96,
    92, 88, 84, 80, 76, 73, 70, 67, 64, 61,
    58, 56, 53, 51, 49, 47, 45, 43, 41, 39,
    37, 36, 34, 33, 31, 30, 28, 27, 26, 25
};
```

### Morse Table

```cpp
const char* morseTable[] = {
    // Numbers 0-9
    "-----", ".----", "..---", "...--", "....-",  // 0-4
    ".....", "-....", "--...", "---..", "----.",  // 5-9
    // Symbols : ; < = > ? @
    "---...", "-.-.-.", "*", "-...-", "*", "..--..", ".--.-.",
    // Letters A-Z
    ".-", "-...", "-.-.", "-..", ".", "..-.",      // A-F
    "--.", "....", "..", ".---", "-.-", ".-..",     // G-L
    "--", "-.", "---", ".--.", "--.-", ".-.",     // M-R
    "...", "-", "..-", "...-", ".--", "-..-",      // S-X
    "-.--", "--.."                                   // Y-Z
};
```

**Index:** `character - '0'` (e.g.: 'A' = 65, '0' = 48, index = 17)

---

## Main Flow

### State Diagram

```
┌─────────┐
│  RESET  │
└────┬────┘
     │
     ▼
┌─────────────┐
│  Waiting    │◄─────────────────────────┐
│   Trafo     │                          │
└──────┬──────┘                          │
       │ trafo connected                 │
       ▼                                   │
┌─────────────┐     ┌───────────┐        │
│  POWER ON   │────▶│ TRAFO REM │────────┘
│  (hum led)  │     │ HARD RESET│
└──────┬──────┘     └───────────┘
       │
       ├─── valve1 ───▶ LED fade + DOT DOT DOT
       │
       ├─── valve2 ───▶ LED fade + DASH DASH DASH
       │
       ├─── valve3 ───▶ LED fade + FM ACTIVE
       │                   ↓
       │              ┌────────────┐
       │              │ FM Dial    │◄──── Encoder turns
       │              │ (9 stations│      → changes station
       │              │ with fade)  │      → volume varies
       │              └─────┬──────┘
       │                    │ valve4
       ▼ (valve4 connected) ▼
┌─────────────┐
│ Transmitting│
│   Morse     │─── Repeats 5x ───▶ SOLVED
└─────────────┘       (SD message)
```

### Activation Sequence

```
[RESET]
    ↓
[POWER ON] ← trafo inserted (P0 LOW)
    - Power LED turns on (NeoPixel 12)
    - Trafo Hum LED starts fade
    ↓
[VALVE 1] ← v1 inserted (P1 LOW)
    - Valve1 LED fade-in (NeoPixel 14 - blue)
    - Morse LED: DOT DOT DOT (3x 500ms)
    - Sound: 432Hz during the blinks
    ↓
[VALVE 2] ← v2 inserted (P2 LOW)
    - Valve2 LED fade-in (NeoPixel 15 - blue)
    - Morse LED: DASH DASH DASH (3x 1500ms)
    - Sound: 432Hz during the blinks
    ↓
[VALVE 3] ← v3 inserted (P3 LOW)
    - Valve3 LED fade-in (NeoPixel 16 - blue)
    - FM Dial activated (encoder controls)
    - Volume array: 10% → 80% → 10%
    - Switches station*.mp3 according to the index
    ↓
[VALVE 4 - FINAL] ← v4 inserted (P7 LOW)
    - Stops FM audio immediately
    - Valve4 LED fade-in (NeoPixel 18 - blue)
    ↓
[TRANSMITTING]
    - Message read from the SD Card (normalized)
    - Transmits in Morse 5x
    - Morse LED blinks synchronized
    - 432Hz tone via I2S
    ↓
[SOLVED]
    - Waits for trafo removal
    - HARD RESET on removal
```

---

## Game States

### State Variables

```cpp
bool valve1flag = false;    // Valve 1 connected
bool valve2flag = false;    // Valve 2 connected
bool valve3flag = false;    // Valve 3 connected (FM)
bool valve4flag = false;    // Valve 4 connected (FINAL)
bool trafoflag = false;     // Transformer connected
bool gameover = false;      // Game finished
```

### FM System Variables

```cpp
int volumeIndex = 0;              // Index in the volume array (0-79)
int lastStationFromIndex = -1;    // Last station played
int currentPlayingStation = -1;   // Station currently playing
bool fmDialActive = false;        // Is FM active?
int lastEncoderState = 0;         // Previous encoder state
```

### Detection Logic

```cpp
// Input reading (active LOW)
bool trafoConnected = !pcf->digitalRead(PIN_TRAFO);
bool valve1Connected = !pcf->digitalRead(PIN_VALVE1);
bool valve2Connected = !pcf->digitalRead(PIN_VALVE2);
bool valve3Connected = !pcf->digitalRead(PIN_VALVE3);
bool valve4Connected = !pcf->digitalRead(PIN_VALVE4);
```

### Hard Reset

Removing the transformer at any time causes a **HARD RESET**:

```cpp
if (!checkTrafo() && trafoflag) {
    Serial.println("[VintageRadio] HARD RESET - Trafo removed");
    resetGame();
    return;
}
```

---

## FM System (Virtual Dialing)

### Volume Array

The system uses an 80-position array with a logarithmic curve:
- Indices 0-39: The volume increases from 10% (25) to 80% (147)
- Indices 40-79: The volume drops from 80% (147) to 10% (25)

```
Volume
  ↑
147│                    ╭─╮
   │                 ╭──╯ ╰──╮
 80│              ╭──╯       ╰──╮
   │           ╭──╯             ╰──╮
 25│──────────╯                   ╰──────────
   └────┬────┬────┬────┬────┬────┬────┬────┬────→ Index
        0    10   20   30   40   50   60   70   79
        │    │    │    │    │    │    │    │
      St1  St2  St3  St4  St5  St6  St7  St8  St9
```

### Station → Index Mapping

| Station | Indices | Threshold |
|---------|---------|-----------|
| Station 1 | 0-9, 70-79 | ≤ 9 |
| Station 2 | 10-18, 61-69 | ≤ 18 |
| Station 3 | 19-27, 52-60 | ≤ 27 |
| Station 4 | 28-36, 43-51 | ≤ 36 |
| Station 5 | 37-45 | ≤ 45 (center/max volume) |
| Station 6 | 46-54 | ≤ 54 |
| Station 7 | 55-63 | ≤ 63 |
| Station 8 | 64-72 | ≤ 72 |
| Station 9 | 73-79 | > 72 |

### Encoder Control

```cpp
// Encoder right → increments index
// Encoder left → decrements index
// The array is circular (wrap around)

void processFmDial() {
    // Reads encoder state (quadrature)
    // Detects a valid transition
    // delta = +1 (right) or -1 (left)
    
    volumeIndex += delta;
    if (volumeIndex >= FM_VOLUME_STEPS) volumeIndex = 0;
    if (volumeIndex < 0) volumeIndex = FM_VOLUME_STEPS - 1;
    
    updateFmAudio();
}
```

### Station Switching

The station changes automatically when `volumeIndex` crosses the thresholds:

```cpp
int stationFromIndex = indexToStation(volumeIndex);

if (stationFromIndex != lastStationFromIndex) {
    // Plays new file (priority SD > LittleFS)
    playStationFile(stationFromIndex);
    lastStationFromIndex = stationFromIndex;
}
```

### File Priority (SD > LittleFS)

```cpp
// Availability flags
bool stationAvailableSD[9] = {false};
bool stationAvailableLFS[9] = {false};

void scanStationFiles() {
    for (int i = 0; i < FM_NUM_STATIONS; i++) {
        // Checks SD first (priority)
        if (SD.exists(fmStationFiles[i]))
            stationAvailableSD[i] = true;
        // Checks LittleFS
        if (LittleFS.exists(fmStationFiles[i]))
            stationAvailableLFS[i] = true;
    }
}

bool playStationFile(int stationIndex) {
    // Priority 1: SD Card
    if (stationAvailableSD[stationIndex])
        return hw.sound().playLoop(filename, true);   // true = SD
    
    // Priority 2: LittleFS
    if (stationAvailableLFS[stationIndex])
        return hw.sound().playLoop(filename, false);  // false = LittleFS
    
    return false; // Not available
}
```

**This allows:** Placing "special" versions (with clues) on the SD Card that override the default versions on LittleFS.

---

## Morse Transmission

### Algorithm

```cpp
void morseCode(const char* asciimsg) {
    // 1. Message already normalized (without accents)
    // 2. For each character:
    //    - If space: delay(MORSE_SPACE_MS)
    //    - If 0-9 or A-Z: converts via morseTable[]
    // 3. For each Morse symbol (. or -):
    //    - Turns on the Morse LED
    //    - Plays tone: hw.sound().tone(freq, volume)
    //    - Delay: dot (500ms) or dash (1500ms)
    //    - Turns off the LED
    //    - Stops tone: hw.sound().noTone()
    //    - Delay: dot (pause between symbols)
    // 4. After each letter: delay(MORSE_END_LETTER_MS)
}
```

### Timing

```
Dot (.) :  LED ON + tone  ─┬──500ms──┬─ LED OFF + silence ─┬──500ms──┬─►
                            │         │                        │         │
Dash (-) :  LED ON + tone  ─┴──1500ms─┴─ LED OFF + silence ─┴──500ms──┴─►

Between letters: ────2000ms────►
Between words: ────3500ms───►
```

### Verification During Transmission

```cpp
while (indMorse < tamMorse) {
    // Checks hard reset on each symbol
    if (!checkTrafo()) {
        gameover = true;
        hw.sound().noTone();
        return;
    }
    // ... transmits symbol
}
```

---

## Text Normalization

The message is loaded from the file `/mensagem.txt` and normalized before transmission.

### Process

```cpp
void normalizeText(char* buffer) {
    // 1. Converts accents to plain ASCII
    //    'á' → 'A', 'ç' → 'C', 'ñ' → 'N', etc.
    // 2. Converts lowercase to uppercase
    // 3. Removes punctuation and invalid symbols
    // 4. Keeps only: A-Z, 0-9, space
    
    // Example:
    // "Parabéns! Você resolveu." → "PARABENS VOCE RESOLVEU"
}
```

### Mapping Table

| Original Character | Result |
|-------------------|-----------|
| à, á, â, ã, ä, å | A |
| é, é, ê, ë | E |
| ì, í, î, ï | I |
| ó, ó, ô, õ, ö | O |
| ù, ú, û, ü | U |
| ç | C |
| ñ | N |
| °, º | O |
| ª | A |
| Others | Removed |

**This allows:** The client to use any text file with accents and punctuation. The system automatically converts it to valid Morse code.

---

## Main Functions

### setupHardware()

```cpp
void setupHardware() override {
    // 1. Configures external NeoPixel
    // 2. Loads message from the SD Card
    // 3. Normalizes message (removes accents)
    // 4. Calls resetGame()
    // 5. Sets state to Running
}
```

### update()

```cpp
void update() override {
    // 1. hw.sound().loop() - keeps audio active
    // 2. If not Running, returns
    // 3. If !gameover:
    //    a. Checks hard reset (trafo removed)
    //    b. Checks trafo connection
    //    c. Checks valves 1-4
    //    d. If valve3 active: processes FM dial
    //    e. If valve4 active: stops FM + transmits Morse
    // 4. If gameover: waits for reset via trafo removal
}
```

### resetGame()

```cpp
void resetGame() {
    // 1. Log "INITIAL CONDITION"
    // 2. Turns off all LEDs (1-4, power, morse, trafo)
    // 3. Clears all flags (valve1-4, trafo, gameover)
    // 4. Resets FM variables (volumeIndex, stations)
    // 5. Clears station file availability
    // 6. Stops sound: hw.sound().noTone() + stop()
}
```

### checkValves()

```cpp
void checkValves() {
    // For each valve (1-4):
    //   - Reads PCF input
    //   - If connected (LOW) and was not flagged:
    //       * Turns on LED with fade
    //       * If valve1: DOT DOT DOT + sound
    //       * If valve2: DASH DASH DASH + sound
    //       * If valve3: activates FM + scanStationFiles()
    //       * If valve4: nothing (waits for transmit)
    //   - If disconnected: turns off LED, clears flag
    //       * If valve3 disconnected: stops FM
}
```

### processFmDial()

```cpp
void processFmDial() {
    // 1. Reads encoder A and B
    // 2. Detects transition (quadrature)
    // 3. Determines direction (delta = +1 or -1)
    // 4. Updates volumeIndex (with wrap around)
    // 5. Calls updateFmAudio()
}
```

### updateFmAudio()

```cpp
void updateFmAudio() {
    // 1. Gets volume: fmVolumeCurve[volumeIndex]
    // 2. Determines station: indexToStation(volumeIndex)
    // 3. If the station changed:
    //    - playStationFile(station) [priority SD > LittleFS]
    // 4. Adjusts volume: hw.sound().setVolume(vol)
}
```

### scanStationFiles()

```cpp
void scanStationFiles() {
    // For each station1-9.mp3:
    //   - Checks SD.exists() → stationAvailableSD[i]
    //   - Checks LittleFS.exists() → stationAvailableLFS[i]
    // Log: "Found /stationX.mp3 (SD Card)" or "(LittleFS)"
}
```

### blinkMorsePattern()

```cpp
void blinkMorsePattern(bool isDot) {
    int duration = isDot ? MORSE_DOT_MS : MORSE_DASH_MS;
    for (int i = 0; i < 3; i++) {
        // LED ON + tone
        setLight(NEO_MORSE, COLOR_MORSE);
        hw.sound().tone(MORSE_FREQ_HZ, MORSE_VOLUME);
        delay(duration);

        // LED OFF + noTone
        setLight(NEO_MORSE, COLOR_OFF);
        hw.sound().noTone();
        delay(duration);

        // Hard reset check
        if (!checkTrafo()) return;
    }
}
```

### fadeLed()

```cpp
void fadeLed(uint8_t pixelIndex, uint32_t targetColor) {
    // Extracts R, G, B components
    // For i from 0 to 255 (step 5):
    //   - Calculates intermediate color
    //   - setLight(pixelIndex, color)
    //   - delay(5)
    // Final setLight with target color
}
```

### loadMessage()

```cpp
void loadMessage() {
    // 1. Checks whether SD is mounted
    // 2. Opens /mensagem.txt
    // 3. Reads up to newline or MAX_MESSAGE_LEN characters
    // 4. Closes the file
    // 5. Normalizes text (removes accents)
    // 6. Fallback: "SOS" if it fails or is empty
}
```

### normalizeText()

```cpp
void normalizeText(char* buffer) {
    // For each character in the buffer:
    //   - Converts lowercase → uppercase
    //   - Maps accents (á, é, ç, etc.) → without accent
    //   - Keeps only A-Z, 0-9, space
    //   - Removes other characters
}
```

---

## Required Files

### SD Card / LittleFS

| File | Required | Location | Description |
|---------|-------------|-------------|-----------|
| /mensagem.txt | Yes | SD or LittleFS | Message to transmit (max 256 chars) |
| /station1.mp3 | Recommended | SD (priority) or LittleFS | Station 1 audio |
| /station2.mp3 | Recommended | SD (priority) or LittleFS | Station 2 audio |
| ... | ... | ... | ... |
| /station9.mp3 | Recommended | SD (priority) or LittleFS | Station 9 audio |

**Priority:** If the same file exists on the SD Card and on LittleFS, the SD Card takes priority.

**This allows:** Placing custom "station*.mp3" files on the SD Card (with clues) that override the LittleFS defaults.

### mensagem.txt Format

Plain text, one or more lines. Accented characters are accepted.

**Example:**
```
Parabéns! Você resolveu o puzzle.
A senha é: 1-2-3-4
```

It will be automatically converted to:
```
PARABENS VOCE RESOLVEU O PUZZLE A SENHA E 1234
```

---

## Serial Debug

Always enabled (there is no #define DEBUG):

```
[VintageRadio] Configuring hardware...
[VintageRadio] External NeoPixel configured
[VintageRadio] Message loaded: Parabens voce resolveu
[VintageRadio] Normalized message: PARABENS VOCE RESOLVEU
[VintageRadio] INITIAL CONDITION
[VintageRadio] Setup complete
[Main] System ready
[Main] Waiting for transformer connection...

[VintageRadio] TRAFO Connected
[VintageRadio] POWER ON
[VintageRadio] Valve1 Connected
[VintageRadio] Morse Dot
[VintageRadio] Valve2 Connected
[VintageRadio] Morse Dash
[VintageRadio] Valve3 Connected
[VintageRadio] FM dial activated - volume array
[FM] Scanning station*.mp3 files...
[FM] Found /station1.mp3 (SD Card)
[FM] Found /station2.mp3 (LittleFS)
...
[FM Dial] Starting at index 0 (minimum volume)
[FM Dial] Use the encoder to scroll through the volume array
[FM Dial] Priority: SD Card > LittleFS

[FM Dial] Encoder -> right
[FM] Index=1, Station=1, Volume=26
[FM Dial] Encoder -> left
[FM] Index=0, Station=1, Volume=25
...
[FM] Switching station: 1 -> 2 (index=10, vol=41)
[FM] Playing /station2.mp3 (LittleFS)

[VintageRadio] Valve4 Connected - FINAL VALVE
[VintageRadio] Interrupting FM and starting Morse transmission
[VintageRadio] Valve 4 (FINAL) connected - transmitting Morse message
P A R A B E N S   V O C E   R E S O L V E U   O   P U Z Z L E
.--. .- .-. .- -... . -. ...   ...-- --- ...-
[VintageRadio] Transmitting (repetition 1/5)
...
[VintageRadio] Transmission complete
```

---

## Compilation

### arduino-cli Command

```bash
cd D:\Users\djair\Documents\Arduino\libraries\propmaker_escaperoom
arduino-cli compile --fqbn rp2040:rp2040:adafruit_feather_prop_maker examples/VintageRadio/VintageRadio.ino
```

### Required Libraries

- PropMakerEscapeRoom (this library)
- IoAbstractionWire
- TaskManager
- SwitchInput
- arduino-audio-tools
- arduino-libhelix
- SD (included in the RP2040 core)
- LittleFS (included in the RP2040 core)

---

*Documentation of the actual implementation - PropMaker EscapeRoom Library*  
*Based on the original code by Nicolau dos Brinquedos (2018)*
*Updated version: FM system with volume array + SD > LittleFS priority*