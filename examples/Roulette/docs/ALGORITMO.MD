# ALGORITHM - Roulette

## General Information

- **Algorithm Name:** Roulette
- **Version:** 1.0
- **Creation Date:** April 2026
- **Platform:** Adafruit Feather RP2040 Prop-Maker
- **Base Library:** PropMakerEscapeRoom
- **Hardware:** SELECT button (GPIO 4)

---

## Objective

Implement a luminous roulette where the player presses a button to spin a ring of LEDs. The light decelerates gradually (physical model with friction) and stops at a position. If it stops on the winning sector, the player wins the round. Three rounds won solve the puzzle.

---

## Data Structure

```
struct RouletteConfig {
    lockPin: uint8          // Lock pin
    neoCount: uint8         // LEDs in the ring
    winSectorCount: uint8   // How many sectors are winners
    spinSpeed: float        // Initial speed (LEDs/tick)
    friction: float         // Deceleration factor (0-1)
    minSpeed: float         // Minimum speed (stops below this)
    volume: uint8           // Audio volume
}

enum RouletteState {
    IDLE     = Waiting for the player to spin
    SPINNING = Roulette spinning (decelerating)
    SOLVED   = Puzzle solved (3 correct spins)
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
                      - chooseWinSector()
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
                    | IDLE           |---- SELECT --------> SPINNING
                    | SPINNING       |---- speed < min ----> check
                    |                |      |               result
                    |                |      +--- hit ------> check
                    |                |      |              spins
                    |                |      +--- missed ---> IDLE
                    | SOLVED         |---- 8s timeout ---> IDLE
                    +----------------+

BETWEEN ROUNDS:
                    SPINNING -> hit (but < 3 spins) -> IDLE (shows green)
                    SPINNING -> hit (3 spins) -> SOLVED
```

---

## Spin with Deceleration Algorithm

```
ALGORITHM: updateSpinning
-------------------------
INPUT: speed (float), pos (float), friction (float)
OUTPUT: updated pos, rendering

1. // Apply friction
2. speed = speed * friction

3. // Advance position
4. pos = pos + speed

5. // Wrap-around (keep inside the ring)
6. WHILE pos >= neoCount:
    6.1. pos = pos - neoCount
    6.2. spins = spins + 1

7. // Render main light + trail
8. ipos = (int)pos
9. FOR each LED i in neoCount:
    9.1. IF i == ipos THEN:
            setPixel(i, BRIGHT_WHITE)
    9.2. ELSE IF i == (ipos - 1) % neoCount THEN:
            setPixel(i, GRAY_60)
    9.3. ELSE IF i == (ipos - 2) % neoCount THEN:
            setPixel(i, GRAY_40)
    9.4. ELSE IF i == (ipos - 3) % neoCount THEN:
            setPixel(i, GRAY_20)
    9.5. ELSE:
            setPixel(i, OFF)

10. // Highlight the winning sector (dim green)
11. IF winSectorCount <= 1:
    11.1. setPixel(winSector, GREEN_DIM)
12. ELSE:
    12.1. FOR i from -offset to +offset:
            12.1.1. setPixel((winSector + i) % neoCount, GREEN_DIM)

13. pixels.show()

14. // Check whether it stopped
15. IF speed < minSpeed THEN:
    15.1. process result

COMPLEXITY: O(n) per tick, where n = neoCount
```

---

## Spin Result Algorithm

```
ALGORITHM: stopSpin
-------------------
INPUT: pos (float), winSector (int), spins (int)
OUTPUT: Updated state

1. ipos = (int)pos
2. won = false

3. // Check the winning sector
4. IF ipos == winSector THEN:
    4.1. won = true

5. // If winSectorCount > 1, check the adjacent ones
6. IF !won AND winSectorCount > 1 THEN:
    6.1. FOR i from 1 to winSectorCount - 1:
            6.1.1. left = (winSector - i + neoCount) % neoCount
            6.1.2. right = (winSector + i) % neoCount
            6.1.3. IF ipos == left OR ipos == right THEN:
                    won = true
                    break

7. IF won THEN:
    7.1. IF spins >= ROUNDS_TO_WIN - 1 THEN:
            7.1.1. // Last spin - solved
            7.1.2. state = SOLVED
            7.1.3. play solved.mp3
            7.1.4. lockPin = LOW
    7.2. ELSE:
            7.2.1. play win.mp3
            7.2.2. show green (2s)
            7.2.3. state = IDLE
    7.3. RETURN

8. // Lost
9. play lose.mp3
10. show red (1.5s)
11. state = IDLE
12. resetSpin()

COMPLEXITY: O(k) where k = winSectorCount
```

---

## State Machine

### Transition Diagram

```
Current State      Event                     Next State        Action
----------------------------------------------------------------------
IDLE              SELECT                    SPINNING          start spin
SPINNING          speed < minSpeed + hit    IDLE (next round) green feedback
SPINNING          speed < minSpeed + miss   IDLE              red feedback
SPINNING          speed < minSpeed + won    SOLVED            unlock
SOLVED            8s timeout                IDLE              full reset
```

### State Table

| State | Description | Inputs | Outputs |
|--------|-----------|----------|--------|
| IDLE | Waiting for a spin | SELECT | NeoPixel: pulsing green on the winning sector |
| SPINNING | Roulette spinning | None | NeoPixel: light + trail + target sector |
| SOLVED | Puzzle solved | 8s Timer | NeoPixel: rainbow; Audio: solved |

---

## Hardware Mapping

### Inputs

| ID | Description | Type | Pin | Logic |
|----|-----------|------|------|--------|
| BTN_SELECT | Button to spin | INPUT_PULLUP | GPIO 4 | LOW = pressed |

### Outputs

| ID | Description | Type | Details |
|----|-----------|------|----------|
| NEO_RING | Roulette ring | NeoPixel GRB | 24 LEDs |
| LOCK_RELAY | Lock | OUTPUT | lockPin, HIGH = locked |
| AUDIO_MAIN | Audio I2S | MP3 via LittleFS | See below |

### Audio Files

| File | Event | Loop? |
|---------|--------|-------|
| /sfx/spin.mp3 | When spinning | No |
| /sfx/win.mp3 | Hit the sector | No |
| /sfx/lose.mp3 | Missed the sector | No |
| /sfx/solved.mp3 | 3 hits, solved | No |

---

## Configurable Parameters (roulette.json)

| Parameter | Description | Range | Default |
|-----------|-----------|-------|--------|
| lockPin | Lock pin | GPIO | 10 |
| neoCount | LEDs in the ring | 8-60 | 24 |
| winSectorCount | Winning sector size | 1-5 | 1 |
| spinSpeed | Initial spin speed | 1.0-20.0 | 8.0 |
| friction | Deceleration factor | 0.90-0.99 | 0.97 |
| minSpeed | Speed to stop | 0.05-0.5 | 0.1 |
| volume | Audio volume | 0-255 | 128 |

---

## Implementation Notes

1. The winning sector is chosen randomly at boot.
2. After solving, a new winning sector is chosen.
3. The roulette never stops instantly - it always decelerates gradually.
4. The number of complete spins (spins) is counted to determine victory.
5. The winning sector is revealed in the IDLE state (pulsing green) - this is intentional.

### Deceleration Formula

Each tick:
- speed = speed * friction
- pos = pos + speed

Example with spinSpeed=8.0, friction=0.97:
- Tick 1: speed=8.00 (advances 8 LEDs)
- Tick 10: speed=6.03
- Tick 50: speed=1.69
- Tick 100: speed=0.36
- Tick 150: speed=0.08 (< minSpeed, stops)

Total: ~150 ticks to stop, traveling ~300 LEDs on a 24-LED ring = ~12.5 laps

---

*The document was generated according to the ALGORITMO.MD specification*.
*Last update: April 2026*