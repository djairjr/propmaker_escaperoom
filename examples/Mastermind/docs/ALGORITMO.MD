# ALGORITHM - Mastermind

## General Information

- **Algorithm Name:** Mastermind
- **Version:** 1.0
- **Creation Date:** April 2026
- **Platform:** Adafruit Feather RP2040 Prop-Maker
- **Base Library:** PropMakerEscapeRoom
- **Hardware:** 4x3 matrix keypad via PCF8575 (I2C 0x20), HT16K33 Display (I2C 0x70)

---

## Objective

Implement the classic Mastermind color-guessing game. The system generates a secret sequence of 4 colors (numbers 1-6). The player makes guesses using the keypad and receives feedback on how many colors are in the correct position (P) and how many colors exist but in the wrong position (E). The player has 10 attempts to get it right.

---

## Data Structure

```
struct MastermindConfig {
    codeLength: uint8      // How many positions (default: 4)
    maxColors: uint8       // Available colors 1-N (default: 6)
    maxAttempts: uint8     // Maximum attempts (default: 10)
    lockPin: uint8         // Lock pin
    neoCount: uint8        // Available NeoPixels
}

enum MMState {
    IDLE     = Waiting for start
    RUNNING  = Player making guesses
    SOLVED   = Code discovered
    FAILED   = Attempts exhausted
}
```

Number-to-color mapping:
```
1 = Red        (RGB: FF0000)
2 = Blue       (RGB: 0000FF)
3 = Green      (RGB: 00FF00)
4 = Yellow     (RGB: FFFF00)
5 = Magenta    (RGB: FF00FF)
6 = Cyan       (RGB: 00FFFF)
```

---

## Main Flowchart

```
                            BOOT
                              |
                              v
                      INITIALIZATION
                      - loadConfig()
                      - generate secret code
                              |
                              v
                      MAIN LOOP
                      - hw.sound().loop()
                      - taskManager.runLoop()
                      - puzzle.update()
                              |
                              v
                   CHECK KEYPAD INPUT
                              |
               +--------------+---------------+
               |              |               |
               v              v               v
           DIGIT 1-6    * (CLEAR)      # (SUBMIT)
               |              |               |
               v              v               v
         ACCUMULATE IN   CLEAR BUFFER    SUBMIT
           GUESS                          GUESS
               |                              |
               v                              v
         UPDATE LEDs                    CHECK GUESS
               |                        +-------+-------+
               |                        |               |
               v                        v               v
              END                 CORRECT CODE    WRONG CODE
                                        |               |
                                        v               v
                                  TRANSITION     CALCULATE P/E
                                  TO SOLVED      +--Attempts--+
                                        |         | exhausted? |
                                        |         +------+------+
                                        |                |
                                        |           +----+----+
                                        |           |         |
                                        |           v         v
                                        |         FAILED   CONTINUE
                                        |                    |
                                        |                    v
                                        |              SHOW P/E
                                        |              ON DISPLAY
                                        |              AND NEO PIXEL
                                        |                    |
                                        +--------------------+
```

---

## Secret Code Generation Algorithm

```
ALGORITHM: newGame
------------------
INPUT: codeLength, maxColors
OUTPUT: secret[] filled

1. FOR i from 0 to codeLength - 1:
    1.1. secret[i] = random(1, maxColors + 1)

2. attempt = 0
3. guessLen = 0
4. clear guess[]
5. gameOver = false

6. // Debug: show the secret code on Serial

COMPLEXITY: O(n) where n = codeLength
```

---

## Feedback Calculation Algorithm (P and E)

```
ALGORITHM: calculateFeedback
----------------------------
INPUT: secret[codeLength], guess[codeLength]
OUTPUT: exact (P), color (E)

1. // Copy the arrays so as not to modify the originals
2. secretCopy = copy(secret, codeLength)
3. guessCopy = copy(guess, codeLength)

4. exact = 0
5. color = 0

6. // STEP 1: Find exact positions (P)
7. FOR i from 0 to codeLength - 1:
    7.1. IF guessCopy[i] == secretCopy[i] THEN:
            7.1.1. exact = exact + 1
            7.1.2. secretCopy[i] = 0  // mark as used
            7.1.3. guessCopy[i] = 0   // mark as used

8. // STEP 2: Find correct colors in the wrong position (E)
9. FOR i from 0 to codeLength - 1:
    9.1. IF guessCopy[i] == 0 THEN: continue (skip)
    9.2. FOR j from 0 to codeLength - 1:
            9.2.1. IF secretCopy[j] == 0 OR i == j THEN: continue
            9.2.2. IF guessCopy[i] == secretCopy[j] THEN:
                    color = color + 1
                    secretCopy[j] = 0  // mark as used
                    break inner loop

10. RETURN (exact, color)

COMPLEXITY: O(n^2) where n = codeLength, but n <= 8 (small constant)

EXAMPLE:
    Secret: [1, 2, 3, 4]
    Guess:  [1, 3, 2, 5]

    Step 1 (exact):
        i=0: 1==1 -> exact=1, secretCopy=[0,2,3,4], guessCopy=[0,3,2,5]
        i=1: 3!=2
        i=2: 2!=3
        i=3: 5!=4
        Result: exact=1

    Step 2 (colors):
        i=1: guessCopy[1]=3 -> secretCopy[2]=3 -> color=1, secretCopy[2]=0
        i=2: guessCopy[2]=2 -> secretCopy[1]=2 -> color=2, secretCopy[1]=0
        Result: color=2

    Feedback: P=1, E=2
```

---

## Guess Submission Algorithm

```
ALGORITHM: submitGuess
----------------------
INPUT: guess[codeLength], secret[codeLength], attempt
OUTPUT: Updated state

1. IF guessLen < codeLength THEN:
    1.1. // Incomplete guess, wait
    1.2. RETURN

2. attempt = attempt + 1

3. (exact, color) = calculateFeedback(secret, guess)

4. // Visual feedback on the NeoPixel
5. SHOW: LEDs 0..codeLength-1 = guess colors
6. SHOW: LEDs codeLength.. = P green LEDs + E yellow LEDs

7. // Feedback on the display
8. display.print("P" + exact + "E" + color)

9. // Check for victory
10. IF exact == codeLength THEN:
    10.1. handleSolve()  // Puzzle solved!
    10.2. RETURN

11. // Check for defeat
12. IF attempt >= maxAttempts THEN:
    12.1. gameOver = true
    12.2. play denied.mp3
    12.3. NeoPixel red
    12.4. display.print("noPE")
    12.5. RETURN

13. // Keep playing
14. guessLen = 0
15. clear guess[]

COMPLEXITY: O(1)
```

---

## State Machine

### Transition Diagram

```
Current State      Event                       Next State        Action
-------------------------------------------------------------------------
BOOT              setup complete               RUNNING           generate code
RUNNING           exact == codeLength           SOLVED            unlock
RUNNING           attempt >= maxAttempts        FAILED            error feedback
FAILED            reset()                       RUNNING           new game
SOLVED            reset()                       RUNNING           new game
```

### State Table

| State | Description | Inputs | Outputs |
|--------|-----------|----------|--------|
| RUNNING | Player making guesses | Keypad 1-6, *, # | NeoPixel: guess + feedback; Display: P/E; Audio: keypress |
| SOLVED | Code discovered | None | NeoPixel: green; Audio: unlock; lockPin: LOW; Display: "donE" |
| FAILED | Attempts exhausted | Reset via Serial | NeoPixel: red; Audio: denied; Display: "noPE" |

---

## Hardware Mapping

### Inputs

| ID | Description | Type | Address | Read |
|----|-----------|------|----------|---------|
| KEYPAD | 4x3 matrix keypad | I2C (PCF8575) | 0x20 | 1-6 = colors, * = clear, # = submit |

Key mapping:
```
Row 0: [1] [2] [3]
Row 1: [4] [5] [6]
Row 2: [7] [8] [9]
Row 3: [*] [0] [#]
```
Note: Only keys 1-6, * and # are used. 7-9 and 0 have no function.

### Outputs

| ID | Description | Type | Details |
|----|-----------|------|----------|
| NEO_FEEDBACK | Guess + feedback | NeoPixel GRB | 8 LEDs (4 guess + 4 P/E) |
| DISPLAY | 4-digit Display | HT16K33 I2C 0x70 | Shows "CodE", "PxEy", "donE", "noPE" |
| LOCK_RELAY | Lock | OUTPUT | lockPin, HIGH = locked |
| AUDIO_MAIN | Audio I2S | MP3 via LittleFS | See below |

### Audio Files

| File | Event | Loop? |
|---------|--------|-------|
| /sfx/beep.mp3 | Key pressed | No |
| /sfx/magic.mp3 | Code discovered | No |
| /sfx/error.mp3 | Attempts exhausted | No |
| /sfx/cancel.mp3 | Clear guess | No |

---

## Configurable Parameters (mastermind.json)

| Parameter | Description | Range | Default |
|-----------|-----------|-------|--------|
| codeLength | Positions in the code | 3-8 | 4 |
| maxColors | Available colors | 4-8 | 6 |
| maxAttempts | Maximum attempts | 3-20 | 10 |
| lockPin | Lock pin | GPIO | 20 |
| neoCount | NeoPixels (>= codeLength*2) | 6-60 | 8 |

### Difficulty

| Difficulty | codeLength | maxColors | maxAttempts |
|-------------|------------|-----------|-------------|
| Easy | 3 | 4 | 12 |
| Medium | 4 | 6 | 10 |
| Hard | 5 | 8 | 8 |
| Expert | 6 | 8 | 6 |

---

## Restrictions and Limitations

| Resource | Limit |
|---------|--------|
| codeLength | Minimum 3, maximum 8 |
| maxColors | Minimum 4, maximum 8 |
| maxAttempts | Minimum 3 |
| NeoPixels | Minimum codeLength * 2 |

---

## Serial Commands

| Command | Description |
|---------|-----------|
| `reset` | Resets the game (new secret code) |
| `status` | Shows the current configuration |

---

*The document was generated according to the ALGORITMO.MD specification*.
*Last update: April 2026*