# ALGORITHM - KeypadCodeMultiple

## General Information

- **Algorithm Name:** KeypadCodeMultiple
- **Version:** 1.0
- **Creation Date:** April 2026
- **Platform:** Adafruit Feather RP2040 Prop-Maker
- **Base Library:** PropMakerEscapeRoom
- **Hardware:** 4x3 matrix keypad via PCF8575 (I2C 0x20)

---

## Objective

Implement a numeric password puzzle where any of the multiple codes configured in the JSON file can be used to solve the puzzle. Ideal for rooms where different clues lead to different valid codes.

---

## Data Structure

```
struct KeypadMultiConfig {
    codes: string[8][4]    // Array of up to 8 four-digit codes
    codeCount: uint8       // Number of registered codes
    lockPin: uint8         // Lock pin
    neoCount: uint8        // Number of NeoPixels
}

enum PuzzleState {
    IDLE     = Waiting for start
    RUNNING  = Accepting keypad input
    SOLVED   = Correct code entered
}
```

---

## Main Flowchart

```
                            BOOT
                              |
                              v
                      INITIALIZATION
                      - hw.begin()
                      - addPcf8575(0x20)
                      - loadConfig()
                      - setupHardware()
                              |
                              v
                      MAIN LOOP
                      - master.update()
                              |
                              v
                   CHECK KEYPAD INPUT
                              |
               +--------------+---------------+
               |                              |
               v                              v
         DIGIT ENTERED                  CODE COMPLETE
               |                              |
               v                              v
         ACCUMULATE IN BUFFER          COMPARE WITH CODES
               |                              |
               v                       +------+-------+
               END                     |              |
                                       v              v
                                 VALID CODE      INVALID
                                       |              |
                                       v              v
                                  TRANSITION     CLEAR BUFFER
                                  TO SOLVED      PLAY ERROR
                                                  |
                                                  v
                                                 END

COMPLEXITY: O(n) per loop, where n = codeCount
```

---

## Code Validation Algorithm

```
ALGORITHM: checkCode
--------------------
INPUT: buffer[4] with accumulated digits, codes[] with valid codes
OUTPUT: boolean (true if code is valid)

1. IF bufferLen < 4 THEN RETURN false

2. FOR each code C in codes[] (up to codeCount):
    2.1. match = true
    2.2. FOR i from 0 to 3:
            2.2.1. IF buffer[i] != C[i] THEN:
                    match = false
                    break loop
    2.3. IF match == true THEN:
            2.3.1. RETURN true

3. RETURN false

COMPLEXITY: O(n * m) where n = codeCount, m = codeLength
```

---

## Key Handling Algorithm

```
ALGORITHM: handleKey
--------------------
INPUT: key (char of the pressed key)
OUTPUT: buffer updated, state updated

1. IF state != RUNNING THEN RETURN (ignore keys)

2. IF key == '*' (CANCEL):
    2.1. bufferLen = 0
    2.2. clear buffer[]
    2.3. play cancel.mp3 sound
    2.4. update NeoPixel to idle
    2.5. RETURN

3. IF key == '#' (CONFIRM):
    3.1. IF bufferLen < 4 THEN RETURN (incomplete code)
    3.2. valid = checkCode()
    3.3. IF valid THEN:
            3.3.1. play unlock.mp3 sound
            3.3.2. activate lockPin (LOW = unlocked)
            3.3.3. NeoPixel green
            3.3.4. state = SOLVED
    3.4. ELSE:
            3.4.1. play denied.mp3 sound
            3.4.2. NeoPixel red (temporary)
            3.4.3. bufferLen = 0
            3.4.4. clear buffer[]
    3.5. RETURN

4. IF key >= '0' AND key <= '9' (DIGIT):
    4.1. IF bufferLen >= 4 THEN RETURN (buffer full)
    4.2. buffer[bufferLen] = key
    4.3. bufferLen++
    4.4. play keypress.mp3 sound
    4.5. update NeoPixel (visual feedback)
    4.6. IF bufferLen == 4 THEN:
            4.6.1. auto-submit (same as pressing #)

COMPLEXITY: O(1)
```

---

## State Machine

### Transition Diagram

```
Current State      Event                Next State        Action
----------------------------------------------------------------------
BOOT              setup complete         IDLE              renderIdle()
IDLE              start/input command    RUNNING           start game
RUNNING           valid code             SOLVED            unlock
RUNNING           * key (cancel)         RUNNING           clear buffer
SOLVED            reset command          IDLE              reset
```

### State Table

| State | Description | Processed Inputs | Active Outputs |
|--------|-----------|---------------------|---------------|
| IDLE | Waiting for start | None (puzzle always ready) | NeoPixel: dim blue (optional) |
| RUNNING | Accepting keys | Keypad 0-9, *, # | NeoPixel: digit feedback; Audio: keypress |
| SOLVED | Correct code | None (waits for reset) | NeoPixel: green; Audio: unlock; lockPin: LOW |

---

## Hardware Mapping

### Inputs

| ID | Description | Type | Address | Read |
|----|-----------|------|----------|---------|
| KEYPAD_ROW | Keypad rows | I2C (PCF8575) | 0x20 | 4x3 scan |
| KEYPAD_COL | Keypad columns | I2C (PCF8575) | 0x20 | Column reads |

Key mapping:
```
Row 0: [1] [2] [3]
Row 1: [4] [5] [6]
Row 2: [7] [8] [9]
Row 3: [*] [0] [#]
```

### Outputs

| ID | Description | Type | Details |
|----|-----------|------|----------|
| NEO_STATUS | Visual feedback | NeoPixel GRB | 8 LEDs |
| LOCK_RELAY | Lock | OUTPUT | lockPin, HIGH = locked |
| AUDIO_MAIN | Audio I2S | MP3 via LittleFS | See files below |

### Audio Files

| File | Event | Loop? |
|---------|--------|-------|
| /sfx/beep.mp3 | Key pressed | No |
| /sfx/magic.mp3 | Correct code | No |
| /sfx/error.mp3 | Wrong code | No |
| /sfx/cancel.mp3 | Cancel entry | No |

---

## Configurable Parameters (keypadmulti.json)

| Parameter | Description | Range | Default |
|-----------|-----------|-------|--------|
| codes[] | Array of valid codes | up to 8 codes | ["1234","5678","0000"] |
| lockPin | Lock pin | GPIO | 20 |
| neopixel.count | Number of NeoPixels | 1-60 | 8 |
| neopixel.colorIdle | Idle color | [R,G,B] | [5,5,20] |
| neopixel.colorUnlock | Unlock color | [R,G,B] | [0,50,0] |
| neopixel.colorDenied | Error color | [R,G,B] | [50,0,0] |

---

## Restrictions and Limitations

| Resource | Limit |
|---------|--------|
| Valid codes | Maximum 8 |
| Code length | Fixed 4 digits |
| Attempts | Unlimited (no lockout) |
| Keypad | 4x3, only 0-9, *, # |

---

## Serial Commands

| Command | Description |
|---------|-----------|
| `codes list` | Lists all registered codes |
| `codes add XXXX` | Adds a new code (4 digits) |
| `codes clear` | Removes all codes |
| `start` | Starts the puzzle |
| `reset` | Resets the puzzle |

---

*The document was generated according to the ALGORITMO.MD specification*.
*Last update: April 2026*