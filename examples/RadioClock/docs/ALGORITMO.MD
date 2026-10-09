# ALGORITHM - RadioClock

## General Information

- **Algorithm Name:** RadioClock
- **Version:** 1.0
- **Creation Date:** April 2026
- **Platform:** Adafruit Feather RP2040 Prop-Maker
- **Base Library:** PropMakerEscapeRoom
- **Hardware:** SELECT button (GPIO 4), HT16K33 Display (I2C 0x70)

---

## Objective

Implement a radio tuner puzzle. The player presses SELECT to step through N frequencies on a dial represented by a ring of LEDs. On wrong frequencies, the audio plays static. On the correct frequency, a clean signal is played. Three hits on different frequencies solve the puzzle.

---

## Data Structure

```
struct RadioClockConfig {
    lockPin: uint8                  // Lock pin
    neoCount: uint8                 // LEDs in the ring
    frequencyCount: uint8           // How many frequencies on the dial
    targetFrequencies[3]: uint8     // Target frequencies (one per round)
    roundsToWin: uint8              // Rounds to win (default: 3)
    volume: uint8                   // Audio volume
}

enum RadioState {
    IDLE     = Waiting for start
    RUNNING  = Player stepping through frequencies
    SOLVED   = Puzzle solved
}
```

---

## Main Flowchart

```
                            BOOT
                              |
                              v
                      INITIALIZATION
                      - loadConfig()
                      - setupHardware()
                      - setState(IDLE)
                              |
                              v
                      MAIN LOOP
                      - hw.sound().loop()
                      - switches.runLoop()
                      - puzzle.update()
                              |
                              v
                    STATE MACHINE
                    +----------------+
                    | IDLE           |---- SELECT --------> RUNNING
                    | RUNNING        |---- SELECT ---------> next freq
                    |                |---- freq hit -------> increment round
                    |                |---- last round -----> SOLVED
                    | SOLVED         |---- 8s timeout ------> IDLE
                    +----------------+
```

---

## Frequency Navigation Algorithm

```
ALGORITHM: nextFrequency
------------------------
INPUT: currentFreq, frequencyCount
OUTPUT: updatedFreq, audio and LEDs updated

1. freq = (freq + 1) % frequencyCount

2. play select.mp3 sound

3. // Render the current frequency on the display and LEDs
4. showFrequency(freq)

5. // Check whether it is this round's target frequency
6. IF freq == targetFrequencies[round] THEN:
    6.1. onTargetHit()
7. ELSE:
    7.1. play static.mp3 (static)

COMPLEXITY: O(1)
```

---

## Frequency Hit Algorithm

```
ALGORITHM: onTargetHit
----------------------
INPUT: currentRound, roundsToWin
OUTPUT: Updated state

1. round = round + 1

2. play signal.mp3 (clean signal)

3. show "GOOd" on the display
4. render pulsing green on the LEDs

5. IF round >= roundsToWin THEN:
    5.1. // Puzzle solved
    5.2. state = SOLVED
    5.3. play solved.mp3
    5.4. lockPin = LOW (unlocked)
    5.5. display.print("donE")

6. ELSE:
    6.1. // Wait 2s showing "GOOd"
    6.2. after 2s: show the new target frequency
    6.3. Serial.printf showing the next round's target

COMPLEXITY: O(1)
```

---

## Rendering Algorithm

```
ALGORITHM: showFrequency
------------------------
INPUT: freq (int), frequencyCount, neoCount
OUTPUT: LEDs updated, display updated

1. // Display: F-X where X is the frequency
2. display.printf("F%2d", freq)

3. // Map the frequency to a position on the NeoPixel ring
4. center = map(freq, 0, frequencyCount - 1, 0, neoCount - 1)

5. // Main light (white)
6. setPixel(center, MEDIUM_WHITE)

7. // Trail (2 LEDs on each side, fade out)
8. setPixel((center-1)%neoCount, GRAY_20)
9. setPixel((center+1)%neoCount, GRAY_20)
10. setPixel((center-2)%neoCount, GRAY_12)
11. setPixel((center+2)%neoCount, GRAY_12)

12. pixels.show()

COMPLEXITY: O(1) (only 5 LEDs lit)

ALGORITHM: showSignal (when hitting)
--------------------------------------
INPUT: None
OUTPUT: Pulsing green LEDs, display "GOOd"

1. // Pulsing green (sine)
2. phase = millis() / 200.0
3. brightness = (sin(phase) * 0.5 + 0.5) * 50 + 30
4. setAll(0, brightness, 0)  // Green

5. display.print("GOOd")

COMPLEXITY: O(n) where n = neoCount

ALGORITHM: renderIdle
---------------------
INPUT: None
OUTPUT: Pulsing blue LEDs

1. phase = millis() / 800.0
2. brightness = (sin(phase) * 0.5 + 0.5) * 10 + 5
3. setAll(0, 0, brightness)  // Blue

COMPLEXITY: O(n)
```

---

## State Machine

### Transition Diagram

```
Current State      Event                     Next State        Action
----------------------------------------------------------------------
IDLE              SELECT                    RUNNING           start game
RUNNING           SELECT (next freq)        RUNNING           change frequency
RUNNING           target freq + round       RUNNING (signal)  green feedback
RUNNING           last target hit           SOLVED            unlock
SOLVED            8s timeout                IDLE              full reset
```

### State Table

| State | Description | Inputs | Outputs |
|--------|-----------|----------|--------|
| IDLE | Waiting for start | SELECT | NeoPixel: pulsing blue; Display: "rAdi" |
| RUNNING | Stepping through frequencies | SELECT | NeoPixel: white + trail; Display: "F-X"; Audio: select/static |
| SIGNAL | Hit feedback | 2s Timer | NeoPixel: pulsing green; Display: "GOOd"; Audio: signal |
| SOLVED | Puzzle solved | 8s Timer | NeoPixel: rainbow; Audio: solved; lockPin: LOW |

---

## Hardware Mapping

### Inputs

| ID | Description | Type | Pin | Logic |
|----|-----------|------|------|--------|
| BTN_SELECT | Advance frequency | INPUT_PULLUP | GPIO 4 | LOW = pressed |

### Outputs

| ID | Description | Type | Details |
|----|-----------|------|----------|
| NEO_DIAL | Radio dial ring | NeoPixel GRB | 24 LEDs |
| DISPLAY | 4-digit Display | HT16K33 I2C 0x70 | Shows the current frequency |
| LOCK_RELAY | Lock | OUTPUT | lockPin, HIGH = locked |
| AUDIO_MAIN | Audio I2S | MP3 via LittleFS | See below |

### Audio Files

| File | Event | Loop? |
|---------|--------|-------|
| /sfx/static.mp3 | Wrong frequency | No |
| /sfx/signal.mp3 | Correct frequency | No |
| /sfx/solved.mp3 | Puzzle solved | No |
| /sfx/select.mp3 | Change frequency | No |

---

## Configurable Parameters (radioClock.json)

| Parameter | Description | Range | Default |
|-----------|-----------|-------|--------|
| lockPin | Lock pin | GPIO | 10 |
| neoCount | LEDs in the ring | 8-60 | 24 |
| frequencyCount | Frequencies on the dial | 3-30 | 12 |
| targetFrequencies | Array of 3 target frequencies | 0..frequencyCount-1 | [3, 8, 5] |
| roundsToWin | Hits required | 1-5 | 3 |
| volume | Audio volume | 0-255 | 128 |

---

## Restrictions and Limitations

| Resource | Limit |
|---------|--------|
| Frequencies on the dial | Configurable (default 12) |
| Rounds to win | Default 3 |
| targetFrequencies | Array of 3 values, each < frequencyCount |

---

## Implementation Notes

1. The target frequencies are defined in the JSON and must differ from each other.
2. The display shows "F-3" for frequency 3, "F-10" for frequency 10, etc.
3. The "static" effect is a short MP3 that plays on wrong frequencies.
4. The player always starts from frequency 0 and advances in increasing order (wrap).
5. After each hit, the display shows "GOOd" for 2s, then returns to showing frequencies.

---

*The document was generated according to the ALGORITMO.MD specification*.
*Last update: April 2026*