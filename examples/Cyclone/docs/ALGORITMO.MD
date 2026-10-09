# ALGORITHM - Cyclone

## General Information

- **Algorithm Name:** Cyclone
- **Version:** 1.0
- **Creation Date:** April 2026
- **Platform:** Adafruit Feather RP2040 Prop-Maker
- **Base Library:** PropMakerEscapeRoom
- **Hardware:** SELECT button (GPIO 4), DET button (GPIO 11)

---

## Objective

Implement a reflex game where a light travels around a ring of NeoPixels at increasing speed and the player must press a button at the exact moment when the light is in the target zone. Three hits solve the puzzle.

---

## Data Structure

```
struct CycloneConfig {
    lockPin: uint8         // Lock pin
    neoCount: uint8        // LEDs in the ring
    targetSize: uint8      // How many LEDs form the target zone
    startSpeed: float      // Initial speed (LEDs/tick)
    speedIncrement: float  // Speed increase per round
    maxSpeed: float        // Maximum speed
    roundsToWin: uint8     // How many hits to win
    volume: uint8          // Audio volume
}

enum CycloneState {
    IDLE     = Waiting for start
    RUNNING  = Light traveling around the ring
    HIT      = Player hit (green feedback)
    MISS     = Player missed (red feedback)
    SOLVED   = Puzzle solved (3 hits)
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
                    | IDLE           |---- SELECT ---------> RUNNING
                    | RUNNING        |---- SELECT (in target)-> HIT
                    | RUNNING        |---- SELECT (outside) ---> MISS
                    | HIT            |---- 1.5s timer ------> RUNNING (next round)
                    | MISS           |---- 1.5s timer ------> RUNNING (same round)
                    | SOLVED         |---- 8s timeout ------> IDLE
                    +----------------+
```

---

## Light Movement Algorithm

```
ALGORITHM: updateRunning
------------------------
INPUT: current speed, current position
OUTPUT: updated position, rendering

1. pos = pos + speed

2. IF pos >= neoCount THEN:
    2.1. pos = pos - neoCount (wrap around)

3. // Render the light
4. ipos = (int)pos
5. FOR each LED i in neoCount:
    5.1. IF i == ipos THEN:
            setPixel(i, BRIGHT_WHITE)  // Main light
    5.2. ELSE IF i == (ipos - 1 + neoCount) % neoCount THEN:
            setPixel(i, MEDIUM_GRAY)        // Trail 1
    5.3. ELSE IF i == (ipos - 2 + neoCount) % neoCount THEN:
            setPixel(i, DIM_GRAY)          // Trail 2
    5.4. ELSE:
            setPixel(i, OFF)

6. // Highlight the target zone
7. FOR i from targetStart to targetStart + targetSize:
    7.1. IF i != ipos THEN:
            setPixel(i, GREEN_DIM)          // Target zone

8. pixels.show()

COMPLEXITY: O(n) where n = neoCount
```

---

## Hit Verification Algorithm

```
ALGORITHM: checkHit
-------------------
INPUT: pos (float), targetCenter (int), targetSize (int)
OUTPUT: bool (true = hit)

1. ipos = (int)pos
2. targetStart = targetCenter - targetSize / 2
3. targetEnd = targetCenter + targetSize / 2

4. // Consider the ring wrap-around
5. IF targetStart < 0:
    5.1. IF ipos >= (targetStart + neoCount) OR ipos <= targetEnd:
            RETURN true
6. IF targetEnd >= neoCount:
    6.1. IF ipos >= targetStart OR ipos <= (targetEnd - neoCount):
            RETURN true
7. ELSE:
    7.1. IF ipos >= targetStart AND ipos <= targetEnd:
            RETURN true

8. RETURN false

COMPLEXITY: O(1)
```

---

## Round Progression Algorithm

```
ALGORITHM: nextRound
--------------------
INPUT: currentRound, currentSpeed
OUTPUT: updatedRound, updatedSpeed

1. currentRound = currentRound + 1

2. IF currentRound >= roundsToWin THEN:
    2.1. // Puzzle solved
    2.2. state = SOLVED
    2.3. play solved.mp3
    2.4. lockPin = LOW (unlocked)
    2.5. RETURN

3. // Increase difficulty
4. currentSpeed = currentSpeed + speedIncrement
5. IF currentSpeed > maxSpeed THEN:
    5.1. currentSpeed = maxSpeed

6. // Choose a new position for the target zone
7. targetCenter = random(targetSize/2, neoCount - targetSize/2)

8. state = RUNNING
9. play hit.mp3

COMPLEXITY: O(1)
```

---

## State Machine

### Transition Diagram

```
Current State      Event                     Next State        Action
----------------------------------------------------------------------
IDLE              SELECT pressed            RUNNING           start game
RUNNING           SELECT (hit)              HIT               increment round
RUNNING           SELECT (missed)           MISS              red feedback
HIT              timer 1.5s (last round)    SOLVED            unlock
HIT              timer 1.5s (more rounds)   RUNNING           next round
MISS             timer 1.5s                 RUNNING           repeat round
SOLVED           8s timeout                 IDLE              reset
```

### State Table

| State | Description | Duration | Inputs | Outputs |
|--------|-----------|---------|----------|--------|
| IDLE | Waiting for SELECT | Indeterminate | SELECT | NeoPixel: pulsing blue |
| RUNNING | Light traveling the ring | Indeterminate | SELECT | NeoPixel: light + target; Audio: tick |
| HIT | Hit feedback | 1.5s | Timer | NeoPixel: green; Audio: hit |
| MISS | Miss feedback | 1.5s | Timer | NeoPixel: red; Audio: miss |
| SOLVED | Puzzle solved | 8s | Timer | NeoPixel: rainbow; Audio: solved |

---

## Hardware Mapping

### Inputs

| ID | Description | Type | Pin | Logic |
|----|-----------|------|------|--------|
| BTN_SELECT | Button to stop the light | Digital INPUT_PULLUP | GPIO 4 | LOW = pressed |
| BTN_DET | Alternative button (optional) | Digital INPUT_PULLUP | GPIO 11 | LOW = pressed |

### Outputs

| ID | Description | Type | Details |
|----|-----------|------|----------|
| NEO_RING | LED ring | NeoPixel GRB | 16 LEDs (configurable) |
| LOCK_RELAY | Lock | OUTPUT | lockPin, HIGH = locked |
| AUDIO_MAIN | Audio I2S | MP3 via LittleFS | See below |

### Audio Files

| File | Event | Loop? |
|---------|--------|-------|
| /sfx/tick.mp3 | Light step (optional) | No |
| /sfx/hit.mp3 | Hit the target | No |
| /sfx/miss.mp3 | Missed the target | No |
| /sfx/solved.mp3 | Puzzle solved | No |

---

## Configurable Parameters (cyclone.json)

| Parameter | Description | Range | Default |
|-----------|-----------|-------|--------|
| lockPin | Lock pin | GPIO | 10 |
| neoCount | LEDs in the ring | 8-60 | 16 |
| targetSize | Target size (LEDs) | 1-10 | 3 |
| startSpeed | Initial speed | 0.01-0.2 | 0.05 |
| speedIncrement | Increase per round | 0.01-0.1 | 0.02 |
| maxSpeed | Maximum speed | 0.1-1.0 | 0.4 |
| roundsToWin | Rounds to win | 1-10 | 3 |
| volume | Audio volume | 0-255 | 128 |

---

## Restrictions and Limitations

| Resource | Limit |
|---------|--------|
| Rounds | Configurable (default 3) |
| NeoPixel ring | Minimum 8 LEDs |
| Maximum speed | Limited by the NeoPixel fps |
| Difficulty | Proportional to targetSize (larger = easier) |

---

## Implementation Notes

1. The target zone is randomly repositioned each round.
2. The player can use SELECT or DET (both work).
3. The speed increases linearly with each successful round.
4. On a miss, the current round is repeated (does not increment).
5. The trail effect helps the player anticipate the movement.

---

*The document was generated according to the ALGORITMO.MD specification*.
*Last update: April 2026*