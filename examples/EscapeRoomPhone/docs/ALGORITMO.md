# Algorithm - Escape Room Phone

## Technical Overview

The Escape Room Phone is an interactive telephone system based on the **Adafruit PropMaker Feather RP2040** board. The system emulates the behavior of a real telephone, providing sound feedback (DTMF or pulse) and custom audio messages.

### Architecture

The system operates as a **state machine** with two main blocking loops based on the telephone's hook state. The processing is divided into:

1. **Core 0 (optional with FreeRTOS):** Game logic and input processing
2. **Core 1 (optional with FreeRTOS):** Audio system (MP3 decoder via I2S)

Without FreeRTOS, everything runs in the main `loop()` with non-blocking updates via `hw.sound().loop()`.

---

## Pinout (PropMaker RP2040)

| Function | Pin | Description | Configuration |
|--------|------|-----------|--------------|
| `PHONE_PIN_HOOK` | 19 | Hook sensor | INPUT_PULLUP |
| `PHONE_PIN_DOORLOCK` | 10 | Output relay | OUTPUT |
| `PHONE_PIN_EXTERNAL_POWER` | 23 | External power activation | OUTPUT (HIGH) |
| `PHONE_PIN_ROTARY_PULSE` | 20 | Rotary dial pulse reading | INPUT |

### Hook Logic Levels

| State | Value | Description |
|--------|-------|-----------|
| `HOOK_ON_HOOK_LEVEL` | HIGH | Handset on hook (phone "hung up") |
| `HOOK_OFF_HOOK_LEVEL` | LOW | Handset off hook (phone "off the hook") |

### Relay Levels

| State | Value | Description |
|--------|-------|-----------|
| `LOCK_ACTIVE_LEVEL` | LOW | Relay activated |
| `LOCK_INACTIVE_LEVEL` | HIGH | Relay deactivated |

---

## Configuration Constants

```cpp
#define DIAL_TIMEOUT_MS      3000    // Timeout between digits (3 seconds)
#define MAX_PHONE_DIGITS     8       // Maximum digits per number
```

---

## File Structure

### LittleFS (data/) - Configurable by the Manufacturer

System files written to internal memory (4MB):

| File | Function |
|---------|--------|
| `/dialtone.mp3` | Dial tone (played when lifting the handset) |
| `/busy.mp3` | Busy tone (after a message) |
| `/naoexiste.mp3` | Number not found message |
| `/dtmf_0.mp3` to `/dtmf_9.mp3` | Key tones 0-9 |
| `/dtmf_star.mp3` | * key tone |
| `/dtmf_hash.mp3` | # key tone |
| `/pulse_0.mp3` to `/pulse_9.mp3` | Rotary dial tones (if enabled) |

### SD Card (sdcard/) - Configurable by the User

Custom messages that the client (room owner) can create:

| File | Function |
|---------|--------|
| `[number].mp3` | Custom audio message |
| `[number].txt` | Marker to activate the relay at the end of the message |

**Note:** The system looks for the files in the SD Card root (`/`). The internal mapping manages the `sdcard/` folder.

---

## State Machine

### Hook States

```
┌─────────────────┐     ┌──────────────────┐
│   ON HOOK       │────▶│  OFF HOOK        │
│   (On Hook)     │◄────│  (Off Hook)      │
└─────────────────┘     └──────────────────┘
        │                        │
        ▼                        ▼
┌─────────────────┐     ┌──────────────────┐
│ System Reset    │     │ Dialtone Playing │
│ No activity     │     │ Waiting         │
└─────────────────┘     │ for digits...   │
                        └──────────────────┘
                                 │
                    ┌────────────┼────────────┐
                    ▼            ▼            ▼
            ┌──────────┐  ┌──────────┐  ┌──────────┐
            │ Dialing  │  │ Timeout  │  │ Max Dig. │
            │ Digits   │  │ Reached  │  │ Reached  │
            └────┬─────┘  └────┬─────┘  └────┬─────┘
                 │            │            │
                 └────────────┴────────────┘
                              │
                              ▼
                    ┌──────────────────┐
                    │ Plays Message    │
                    │ (SD or Invalid)  │
                    └────────┬─────────┘
                             │
                    ┌────────┴────────┐
                    ▼                 ▼
           ┌──────────────┐   ┌──────────────┐
           │ .txt exists? │   │ .txt does    │
           │ Triggers Relay│  │ not exist    │
           └──────┬───────┘   └──────┬───────┘
                  │                  │
                  ▼                  ▼
         ┌──────────────┐   ┌──────────────┐
         │ Busy Tone    │   │ Busy Tone    │
         │ (Loop)       │   │ (Loop)       │
         └──────────────┘   └──────────────┘
```

---

## Detailed Algorithm

### PhonePuzzle Class

#### State Variables

```cpp
String dialed = "";           // Dialed digits buffer
bool audioPlayed = false;     // Message already played?
bool dialtoneSent = false;    // Dialtone already started?
bool isBusyState = false;     // Is it in the busy state?
bool waitingForStart = false; // Waiting for audio to start?
bool newline = true;          // Debug formatting control
unsigned long lastDigitAt = 0;// Timestamp of the last digit
```

#### Main Flow (update())

1. **Hook Synchronization**
   ```cpp
   currentHook = digitalRead(PHONE_PIN_HOOK);
   if (currentHook != isHookedGlobal) {
       isHookedGlobal = currentHook;
       // State log
   }
   ```

2. **"On Hook" Loop (Blocking)**
   ```cpp
   while (isHookedGlobal) {
       // If there is activity in progress, immediate reset
       if (audioPlayed || dialtoneSent || isBusyState || !newline) {
           stopAudio();
           resetPhone();
       }
       // Waits for a state change
       delay(10);
   }
   ```

3. **"Off Hook" Loop (Blocking)**
   ```cpp
   while (!isHooked) {
       // State 1: Initial dialtone
       if (bufferEmpty && !audioPlayed && !dialtoneSent && !isBusyState) {
           playLoop(FILE_DIALTONE);
           dialtoneSent = true;
       }
       
       // State 2: Processing input
       if (!audioPlayed && !isBusyState) {
           processKeypad();  // or processRotary()
       }
       
       // State 3: Post-message → Busy
       if (messageFinished && !isBusyState) {
           playLoop(FILE_BUSY);
           isBusyState = true;
       }
       
       delay(1);
   }
   ```

#### Call Processing (processCall())

```cpp
void processCall() {
    mp3Path = "/" + dialed + ".mp3";
    txtPath = "/" + dialed + ".txt";
    
    stopAudio();
    audioPlayed = true;
    waitingForStart = true;
    
    if (SD.exists(mp3Path)) {
        playOnceBlocking(mp3Path, SOURCE_SD);
        
        // Checks the trigger file
        if (SD.exists(txtPath)) {
            digitalWrite(PHONE_PIN_DOORLOCK, LOCK_ACTIVE_LEVEL);
        }
    } else {
        playOnceBlocking(FILE_INVALID);  // LittleFS
    }
    
    dialed = "";  // Clears the buffer
}
```

#### Input Processing

**Keypad (I2C/default):**
```cpp
void processKeypad() {
    key = keypadListener.getLastKey();
    if (key != 0) {
        stopAudio();  // Stops the dialtone
        dialtoneSent = true;
        
        // DTMF sound feedback
        playOnceBlocking(FILE_DTMF[key]);
        
        dialed += key;
        lastDigitAt = millis();
    } else {
        checkDialTimeout();
    }
}
```

**Rotary Dial (optional):**
```cpp
void processRotary() {
    if (rotaryDial->available()) {
        digit = rotaryDial->read();
        
        stopAudio();
        dialtoneSent = true;
        
        // Pulse sound feedback
        playOnceBlocking(FILE_ROTARY[digit]);
        
        dialed += String(digit);
        lastDigitAt = millis();
    } else {
        checkDialTimeout();
    }
}
```

#### Dialing Timeout

```cpp
void checkDialTimeout() {
    isMaxDigits = (dialed.length() == MAX_PHONE_DIGITS);
    isTimeout = (!newline && (millis() - lastDigitAt > DIAL_TIMEOUT_MS));
    
    if ((isMaxDigits || isTimeout) && dialed.length() > 0) {
        processCall();
    }
}
```

---

## Input Modes (Compile-time)

The system supports three input modes selected by `#define`:

| Mode | Define | Hardware | Implementation |
|------|--------|----------|---------------|
| Default | (none) | I2C Keypad (PCF8574/8575) | IoAbstraction |
| GPIO | `KEYPAD_GPIO` | Direct GPIO Keypad (pins 6-12) | Native class |
| Rotary | `USE_ROTARY_DIAL` | Rotary dial (pin 20) | RotaryDialReader |

### Pin Mapping - I2C Keypad

| Keypad | PCF8574/8575 |
|---------|--------------|
| R1-R4 (Rows) | P3, P2, P1, P0 |
| C1-C3 (Columns) | P6, P5, P4 |

**Default I2C address:** `0x20`

---

## Audio System

### Priority Hierarchy

1. **playOnce() / playOnceBlocking()**: Plays once, interrupts the previous one
2. **playLoop()**: Plays in a loop until interrupted
3. **stop()**: Interrupts any playback

### Audio Sources

| Constant | Value | Meaning |
|-----------|-------|-------------|
| `SOURCE_LITTLEFS` | `false` | Loads from internal memory |
| `SOURCE_SD` | `true` | Loads from the SD card |

---

## Hook Interrupt

```cpp
void IRAM_ATTR handleHookISR() {
    static unsigned long lastInterruptTime = 0;
    unsigned long interruptTime = millis();
    
    // 50ms debounce
    if (interruptTime - lastInterruptTime > 50) {
        isHookedGlobal = (digitalRead(PHONE_PIN_HOOK) == HOOK_ON_HOOK_LEVEL);
    }
    lastInterruptTime = interruptTime;
}
```

**Configuration:**
```cpp
attachInterrupt(digitalPinToInterrupt(PHONE_PIN_HOOK), handleHookISR, CHANGE);
```

---

## Initialization (setup())

### Boot Sequence

1. **Waits for Serial** (3s timeout)
2. **Initializes Hardware**
   - `hw.begin()` - Initializes subsystems
   - `hw.addPcf8575(0x20)` - Adds the I2C expander
   - `hw.sound().setVolume(89)` – Sets the volume level.
3. **Initializes Input**
   - I2C, GPIO or Rotary keypad according to the defines
4. **Initializes Puzzle**
   - `master.addPuzzle(&phone)`
   - `master.begin()`
   - `phone.run()`

---

## Production Configurations

### With FreeRTOS (Recommended)

Enable: `Tools > RP2040 Boards > "FreeRTOS SMP"`

```cpp
void audioTask(void *param) {
    while (1) {
        hw.sound().loop();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void logicTask(void *param) {
    while (1) {
        master.update();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void setup() {
    // ... initialization ...
    
    xTaskCreate(audioTask, "Audio", 16384, NULL, 2, NULL);
    xTaskCreate(logicTask, "Logic", 8192, NULL, 1, NULL);
}

void loop() {
    // Empty - tasks run on the cores
}
```

### Without FreeRTOS (Testing)

```cpp
void loop() {
    hw.sound().loop();
    master.update();
    delay(1);
}
```

---

## Global Variables

```cpp
LogicManager master(1);     // ID 1, master mode
PhonePuzzle phone;          // Puzzle instance

// Pointer to the rotary input (if used)
RotaryDialReader* rotaryDial = nullptr;

// Hook state (shared with the ISR)
volatile bool isHookedGlobal = true;
```

---

## Debug via Serial

The system sends debug messages to the Serial port:

```
[PHONE] Hardware Ready. Initial Hook State: ON HOOK
[PHONE] Hook changed: OFF HOOK
[PHONE] Hook Detected: System Reset.
[PHONE] Handset lifted!
[PHONE] Message finished. Starting Busy Tone.
[PHONE] Checking SD for: /1234.mp3
[PHONE] Success: File found on the SD Card.
[PHONE] Lock file detected. Opening door.
[PHONE] Error: Number not mapped on the SD. Playing Invalid.
[PHONE] Digit limit reached.
[PHONE] Dialing timeout.
```

---

## Version

**Algorithm:** Escape Room Phone v1.0  
**Platform:** Adafruit PropMaker Feather RP2040  
**Framework:** Arduino RP2040 Core (Earle Philhower)  
**Libraries:** PropMakerEscapeRoom, IoAbstraction, Arduino Audio Tools