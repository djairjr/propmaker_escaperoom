# ALGORITHM - BallMaze

## General Information

- **Algorithm Name:** BallMaze
- **Version:** 1.0
- **Creation Date:** April 2026
- **Platform:** Adafruit Feather RP2040 Prop-Maker
- **Base Library:** PropMakerEscapeRoom
- **Main Sensor:** LIS3DH Accelerometer (I2C 0x18)

---

## Objective

Simulate a virtual physical ball within a 2D maze using the LIS3DH accelerometer built into the PropMaker board. The player must tilt the “box” to move the ball and must reach all of the target areas in order to solve the puzzle.

---

## Data Structure

```
struct BallPhysics {
    posX: float      // Ball X position in the maze (0..mazeW)
    posY: float      // Ball Y position in the maze (0..mazeH)
    velX: float      // X velocity (pixels per tick)
    velY: float      // Y velocity (pixels per tick)
}

struct TargetZone {
    centerX: uint8   // Zone center X
    centerY: uint8   // Zone center Y
    radius: uint8    // Zone radius (circular detection)
    hit: bool        // true if already visited
}

enum MazeState {
    IDLE     = Waiting for the player to start (shake)
    RUNNING  = Physics active, ball in motion
    SOLVED   = All zones visited, victory animation
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
                      - loadConfig()
                      - setupHardware()
                      - setState(IDLE)
                              |
                              v
                      MAIN LOOP
                      - hw.sound().loop()
                      - taskManager.runLoop()
                      - switches.runLoop()
                      - puzzle.update()
                              |
                              v
                    STATE MACHINE
                    +----------------+
                    | IDLE           |---- shake > 15 m/s^2 --> RUNNING
                    | RUNNING        |---- all zones hit -----> SOLVED
                    | SOLVED         |---- 10s timeout ---------> IDLE
                    +----------------+
```

---

## Ball Physics Algorithm

```
ALGORITHM: updatePhysics
------------------------
INPUT: Accelerometer reading (ax, ay, az) in m/s^2
OUTPUT: posX, posY updated

1. Read the accelerometer via hw.accelerometer().getEvent()
2. Get accelX = event.acceleration.x (m/s^2)
3. Get accelY = event.acceleration.y (m/s^2)
4. Get accelZ = event.acceleration.z (m/s^2)

5. // Deadzone: ignore very small tilts
6. IF abs(accelX) < deadZone THEN accelX = 0
7. IF abs(accelY) < deadZone THEN accelY = 0

8. // Map tilt to acceleration in the maze plane
9. // Inverted: tilting right moves the ball to the right
10. accelX = accelX * accelFactor
11. accelY = accelY * accelFactor

12. // Euler integration: acceleration -> velocity
13. velX = velX + accelX * dt
14. velY = velY + accelY * dt

15. // Damping (friction)
16. velX = velX * damping
17. velY = velY * damping

18. // Terminal velocity
19. IF velX > maxSpeed THEN velX = maxSpeed
20. IF velX < -maxSpeed THEN velX = -maxSpeed
21. IF velY > maxSpeed THEN velY = maxSpeed
22. IF velY < -maxSpeed THEN velY = -maxSpeed

23. // Integration: velocity -> position
24. posX = posX + velX * dt
25. posY = posY + velY * dt

26. // Wall collision (bounce)
27. IF posX < 0 THEN posX = 0, velX = -velX * bounce
28. IF posX > mazeW THEN posX = mazeW, velX = -velX * bounce
29. IF posY < 0 THEN posY = 0, velY = -velY * bounce
30. IF posY > mazeH THEN posY = mazeH, velY = -velY * bounce

31. // Anti-NaN: if the velocity becomes too small, zero it
32. IF abs(velX) < 0.01 THEN velX = 0
33. IF abs(velY) < 0.01 THEN velY = 0

COMPLEXITY: O(1)
```

---

## Target Zone Detection Algorithm

```
ALGORITHM: checkTargets
-----------------------
INPUT: posX, posY of the ball, array of TargetZone
OUTPUT: targetsHit updated, allHit flag

1. allHit = true
2. FOR each target T in targets[]:
    2.1. dx = posX - T.centerX
    2.2. dy = posY - T.centerY
    2.3. dist = sqrt(dx*dx + dy*dy)
    2.4. IF dist < T.radius THEN:
            2.4.1. IF !T.hit THEN:
                    T.hit = true
                    play target_hit.mp3 sound
                    update targetsHit counter
            2.5. ELSE IF T.hit == false THEN:
                    allHit = false
    2.6. ELSE:
            2.6.1. IF T.hit == false THEN:
                    allHit = false

3. IF allHit == true AND targetsHit > 0 THEN:
    3.1. Transition to SOLVED

4. RETURN allHit

COMPLEXITY: O(n) where n = number of target zones
```

---

## NeoPixel Rendering Algorithm

```
ALGORITHM: renderMaze
---------------------
INPUT: posX, posY, targets[], state
OUTPUT: NeoPixel LEDs updated

1. Clear all LEDs (clear)

2. // Map the 2D position (0..mazeW, 0..mazeH) to a linear LED
3. // Uses serpentine mapping: even rows L->R, odd rows R->L
4. row = floor(posY / (mazeH / numRows))
5. col = floor(posX / (mazeW / numCols))
6. ledIndex = (row % 2 == 0) ? (row * numCols + col) : (row * numCols + (numCols - 1 - col))
7. ledIndex = constrain(ledIndex, 0, neoCount - 1)

8. // Draw the ball (bright white)
9. setPixel(ledIndex, WHITE)

10. // Draw the target zones
11. FOR each target T in targets[]:
    12. ledTarget = map T.centerX, T.centerY to LED
    13. IF T.hit THEN:
            setPixel(ledTarget, GREEN_DIM)
    14. ELSE:
            // Calculate distance for proximity glow
            dist = sqrt((posX-T.centerX)^2 + (posY-T.centerY)^2)
            IF dist < T.radius * 2 THEN:
                glow = map(dist, 0, T.radius*2, 255, 50)
                setPixel(ledTarget, YELLOW with glow brightness)
            ELSE:
                setPixel(ledTarget, YELLOW_DIM)

15. pixels.show()

COMPLEXITY: O(m + n) where m = targets, n = LEDs
```

---

## State Machine

### Transition Diagram

```
Current State      Event                     Next State        Action
--------------------------------------------------------------------------
BOOT              setup complete             IDLE              renderIdle()
IDLE              shake > 15 m/s^2           RUNNING           startGame()
RUNNING           all zones visited          SOLVED            solve()
SOLVED            10s timeout                IDLE              reset()
```

### State Table

| State | Description | Processed Inputs | Active Outputs |
|--------|-----------|---------------------|---------------|
| IDLE | Waiting for shake to start | Accelerometer (shake) | NeoPixel: pulsing blue; Audio: idle_chime (optional) |
| RUNNING | Physics active, player moving | Accelerometer (tilt) | NeoPixel: ball + targets; Audio: ball_roll (loop) |
| SOLVED | Puzzle solved | None (auto-reset timer) | NeoPixel: rainbow chase; Audio: solved.mp3 |
| ERROR | Accelerometer not detected | None | NeoPixel: blinking red |

---

## Hardware Mapping

### Inputs

| ID | Description | Type | Range | Read |
|----|-----------|------|-------|---------|
| ACCEL_X | Accelerometer X axis | I2C (0x18) | +/- 78.4 m/s^2 | 50Hz via getEvent() |
| ACCEL_Y | Accelerometer Y axis | I2C (0x18) | +/- 78.4 m/s^2 | 50Hz via getEvent() |
| ACCEL_Z | Accelerometer Z axis | I2C (0x18) | +/- 78.4 m/s^2 | 50Hz via getEvent() |

### Outputs

| ID | Description | Type | Details |
|----|-----------|------|----------|
| NEO_BALL | Maze LEDs | NeoPixel GRB | 30 LEDs, 2D serpentine mapping |
| AUDIO_MAIN | Audio I2S | MP3 via LittleFS | See files below |
| LOCK_RELAY | Lock | OUTPUT | lockPin, HIGH = locked |

### Audio Files

| File | Event | Loop? |
|---------|--------|-------|
| /sfx/ball_roll.mp3 | Ball in motion | Yes |
| /sfx/target_hit.mp3 | Target visited | No |
| /sfx/solved.mp3 | Puzzle solved | No |
| /sfx/idle_chime.mp3 | Every 15s while idle | No (optional) |

---

## Configurable Parameters (ballmaze.json)

| Parameter | Description | Range | Default |
|-----------|-----------|-------|--------|
| phys.accel | Acceleration factor | 100-1000 | 600 |
| phys.damp | Damping (friction) | 1.0-5.0 | 3.5 |
| phys.bounce | Bounce coefficient | 0.0-1.0 | 0.3 |
| phys.maxSpd | Maximum speed | 50-500 | 250 |
| phys.deadZ | Accelerometer deadzone | 0.1-1.0 | 0.25 |
| maze.w | Width of the virtual maze | 100-500 | 200 |
| maze.h | Height of the virtual maze | 100-500 | 200 |
| startX | Initial ball X position | 0-mazeW | 100 |
| startY | Initial ball Y position | 0-mazeH | 100 |
| targetCount | Number of target zones | 1-8 | 3 |
| targetRadius | Radius of the target zones | 5-30 | 12 |
| requireOrder | Mandatory sequential order | true/false | false |
| lockPin | Lock pin | GPIO | 10 |
| neoCount | Number of NeoPixels | 1-60 | 30 |
| volume | Audio volume | 0-255 | 128 |

---

## Restrictions and Limitations

| Resource | Limit |
|---------|--------|
| Target zones | Maximum 8 |
| NeoPixels | Maximum 60 |
| Physics update rate | ~60Hz (16ms) |
| NeoPixel update rate | ~30Hz (33ms) |
| Accelerometer accuracy | +/- 2% (typical LIS3DH error) |

### Notes

1. The LIS3DH must be in low-power mode to avoid excessive consumption.
2. Deadzone calibration depends on the surface where the box is placed.
3. The serpentine mapping assumes the NeoPixels are arranged in 2 parallel rows.
4. In the ERROR state, the puzzle cannot be solved - a reset is required.

---

*The document was generated according to the ALGORITMO.MD specification*.
*Last update: April 2026*