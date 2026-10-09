# ALGORITHM - UniversalController

## General Information

- **Algorithm Name:** UniversalController
- **Version:** 1.0
- **Creation Date:** April 2026
- **Author:** Escape Room Puzzle System
- **Platform:** Arduino (RP2040)
- **Base Library:** PropMakerEscapeRoom

---

## Objective

Implement a universal controller configurable via JSON for escape room puzzles, supporting multiple sequential operating modes, integrated audiovisual feedback and optional RS485 communication.

---

## Data Structure

### Enumerated Types

```cpp
enum class PuzzleMode {
    CUSTOM_SEQUENCE,   // Sequence defined in JSON
    LINEAR_SEQUENCE,   // Linear order 0,1,2,3...
    ALL_CONNECTED,     // All inputs active (AND)
    RANDOM             // Random sequence at boot
};

enum class EngineState {
    IDLE,            // Waiting for start
    RUNNING,         // Processing inputs
    INPUT_CORRECT,   // Correct feedback
    INPUT_WRONG,     // Error feedback
    SOLVED,          // Puzzle complete
    ERROR,           // Sequence error
    TIMEOUT          // Time expired
};
```

### Main Structures

```cpp
struct ConfigManager {
    ControllerConfig controller;
    InputConfig inputs;
    LogicConfig logic;
    PwmConfig pwm;
    AudioConfig audio;
    NeoPixelConfig neopixel;
    Rs485Config rs485;
    OutputConfig output;
};

struct PuzzleEngine {
    uint8_t sequence[16];        // Expected sequence
    uint8_t sequenceLength;      // Sequence length
    uint8_t currentStep;         // Current step
    uint8_t progress;            // Progress 0-100%
    EngineState state;           // Current state
    unsigned long startTime;     // Start time
    unsigned long lastInputTime; // Last input
};
```

---

## Main Flowchart

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                                   BOOT                                      │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                         INITIALIZATION (setup)                              │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │ 1. Initialize Hardware (hw.begin())                                 │   │
│  │ 2. Mount LittleFS                                                   │   │
│  │ 3. Load config.json (or defaults)                                   │   │
│  │ 4. Configure PCF8575                                                │   │
│  │ 5. Configure pins as INPUT_PULLUP                                   │   │
│  │ 6. Initialize NeoPixel                                              │   │
│  │ 7. Configure lock output                                            │   │
│  │ 8. Initialize PuzzleEngine                                          │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                              MAIN LOOP                                      │
│                              (loop/while)                                   │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
              ┌─────────────────────┼─────────────────────┐
              │                     │                     │
              ▼                     ▼                     ▼
    ┌─────────────────┐   ┌─────────────────┐   ┌─────────────────┐
    │   hw.sound()    │   │ puzzle.update() │   │ processSerial() │
    │    .loop()      │   │                 │   │                 │
    └─────────────────┘   └────────┬────────┘   └─────────────────┘
                                   │
                                   ▼
                    ┌──────────────────────────────┐
                    │        STATE MACHINE         │
                    └──────────────────────────────┘
                                   │
              ┌─────────────────────┼─────────────────────┐
              │                     │                     │
              ▼                     ▼                     ▼
    ┌─────────────────┐   ┌─────────────────┐   ┌─────────────────┐
    │      IDLE       │   │    RUNNING      │   │     SOLVED      │
    └─────────────────┘   └────────┬────────┘   └─────────────────┘
                                   │
              ┌─────────────────────┼─────────────────────┐
              │                     │                     │
              ▼                     ▼                     ▼
    ┌─────────────────┐   ┌─────────────────┐   ┌─────────────────┐
    │ INPUT_CORRECT   │   │  INPUT_WRONG    │   │    ERROR        │
    └─────────────────┘   └─────────────────┘   └─────────────────┘
                                   │
                                   ▼
                          ┌─────────────────┐
                          │    TIMEOUT      │
                          └─────────────────┘
```

---

## Processing Algorithm per Mode

### CUSTOM_SEQUENCE Mode

```
ALGORITHM: ProcessCustomSequence
─────────────────────────────────
INPUT: None (uses global state)
OUTPUT: Updated state

1. FOR each input i FROM 0 TO inputCount DO:
     1.1. IF detectRisingEdge(i) THEN:
            1.1.1. expected ← sequence[currentStep]
            1.1.2. IF i == expected THEN:
                     1.1.2.1. currentStep ← currentStep + 1
                     1.1.2.2. updateProgress()
                     1.1.2.3. playInputSound(currentStep)
                     1.1.2.4. state ← INPUT_CORRECT
                     1.1.2.5. IF currentStep >= sequenceLength THEN:
                                1.1.2.5.1. onSequenceComplete()
                     1.1.2.6. RETURN
            1.1.3. ELSE:
                     1.1.3.1. playErrorSound()
                     1.1.3.2. state ← INPUT_WRONG
                     1.1.3.3. RETURN

2. IF timeoutEnabled AND timeExpired THEN:
     2.1. state ← TIMEOUT
     2.2. onTimeout()

3. END
```

### LINEAR_SEQUENCE Mode

```
ALGORITHM: ProcessLinearSequence
─────────────────────────────────
INPUT: None
OUTPUT: Updated state

1. sequence[i] ← i  (for every i from 0 to inputCount)
2. sequenceLength ← inputCount
3. EXECUTE ProcessCustomSequence()

END
```

### ALL_CONNECTED Mode

```
ALGORITHM: ProcessAllConnected
───────────────────────────────
INPUT: None
OUTPUT: Updated state

1. activeCount ← 0
2. FOR each input i FROM 0 TO inputCount DO:
     2.1. IF isInputActive(i) THEN:
            2.1.1. activeCount ← activeCount + 1

3. currentStep ← activeCount
4. updateProgress()
5. updateNeoPixel()
6. updatePwmOutput()

7. IF activeCount >= inputCount THEN:
     7.1. onSequenceComplete()

8. END
```

### RANDOM Mode

```
ALGORITHM: ProcessRandomSequence
─────────────────────────────────
INPUT: None
OUTPUT: Updated state

1. IF firstBoot THEN:
     1.1. tempArray ← [0, 1, 2, ..., inputCount-1]
     1.2. FisherYatesShuffle(tempArray)
     1.3. sequence ← tempArray
     1.4. sequenceLength ← inputCount

2. EXECUTE ProcessCustomSequence()

END

PROCEDURE: FisherYatesShuffle(array)
───────────────────────────────────────
INPUT: array (size n)
OUTPUT: shuffled array

1. FOR i FROM n-1 TO 1 (descending) DO:
     1.1. j ← random(0, i+1)
     1.2. SWAP array[i] WITH array[j]

2. RETURN array
```

---

## Input Detection Algorithm

```
ALGORITHM: detectRisingEdge
────────────────────────────
INPUT: pin (pin number)
OUTPUT: boolean (true if edge detected)

1. current ← readPin(pin)  // LOW = active (PULLUP)
2. current ← (current == LOW) ? 1 : 0
3. previous ← lastInputState[pin]
4. lastInputState[pin] ← current

5. IF previous == 0 AND current == 1 THEN:
     5.1. RETURN true  // Rising edge

6. RETURN false

COMPLEXITY: O(1)
```

---

## Progress Update Algorithm

```
ALGORITHM: updateProgress
─────────────────────────
INPUT: None (uses global state)
OUTPUT: updated progress (0-100)

1. IF totalSteps == 0 THEN:
     1.1. progress ← 0
     1.2. RETURN

2. IF currentStep >= totalSteps THEN:
     2.1. progress ← 100
     2.2. RETURN

3. progress ← (currentStep * 100) / totalSteps

4. pwmValue ← map(progress, 0, 100, pwmMin, pwmMax)
5. setAnalogOutput(pwmChannel, pwmValue)

6. updateNeoPixel()

COMPLEXITY: O(1)
```

---

## NeoPixel Update Algorithm

```
ALGORITHM: updateNeoPixel
─────────────────────────
INPUT: None
OUTPUT: updated LEDs

1. IF !neopixelEnabled THEN RETURN

2. ledsToLight ← (progress * neopixelCount) / 100

3. FOR i FROM 0 TO neopixelCount DO:
     3.1. IF i < ledsToLight THEN:
            3.1.1. setPixelColor(i, colorProgress)
     3.2. ELSE IF i == ledsToLight AND state == INPUT_CORRECT THEN:
            3.2.1. brightness ← 128 + 127 * sin(millis/200)
            3.2.2. setPixelColor(i, colorProgress with brightness)
     3.3. ELSE:
            3.3.1. setPixelColor(i, colorIdle)

4. pixels.show()

COMPLEXITY: O(n) where n = neopixelCount
```

---

## Serial Command Processing Algorithm

```
ALGORITHM: processSerialCommands
────────────────────────────────
INPUT: Data available on Serial
OUTPUT: Action executed or error message

1. IF !Serial.available() THEN RETURN

2. cmd ← readSerialString()
3. cmd ← toLowerCase(cmd)

4. SWITCH cmd:
     CASE "start" OR "run":
          4.1. IF state == IDLE THEN:
                 4.1.1. state ← RUNNING
                 4.1.2. engine.start()
                 4.1.3. print("Puzzle started")

     CASE "reset":
          4.2. engine.reset()
          4.3. state ← IDLE
          4.4. print("Puzzle reset")

     CASE "status":
          4.5. printStatus()

     CASE "solve" OR "force":
          4.6. state ← SOLVED
          4.7. onSequenceComplete()

     CASE "config":
          4.8. printConfiguration()

     CASE "help":
          4.9. printHelp()

     DEFAULT CASE:
          4.10. print("Unknown command")

5. END
```

---

## onSequenceComplete Callback Algorithm

```
ALGORITHM: onSequenceComplete
─────────────────────────────
INPUT: None
OUTPUT: Puzzle solved, outputs activated

1. state ← SOLVED
2. progress ← 100
3. updatePwmOutput()  // PWM at maximum
4. updateNeoPixel()   // All LEDs green

5. playUnlockSound()  // Plays unlock sound

6. // Activates lock output
7. IF activeHigh THEN:
     7.1. digitalWrite(lockPin, HIGH)
   ELSE:
     7.2. digitalWrite(lockPin, LOW)

8. // Celebration visual effect
9. showSolvedEffect()

10. log("PUZZLE SOLVED")

COMPLEXITY: O(n) where n = neopixelCount
```

---

## State Machine

### Transition Diagram

```
Current State      Event               Next State        Action
────────────────────────────────────────────────────────────────────
IDLE              start()             RUNNING           engine.start()
RUNNING           correctInput        INPUT_CORRECT     playSound()
RUNNING           wrongInput          INPUT_WRONG       playError()
RUNNING           timeout             TIMEOUT           onTimeout()
INPUT_CORRECT     timer500ms          RUNNING           -
INPUT_WRONG       resetOnError=true   IDLE              engine.reset()
INPUT_WRONG       resetOnError=false  RUNNING           -
RUNNING           sequenceComplete    SOLVED            onSequenceComplete()
SOLVED            reset()             IDLE              engine.reset()
TIMEOUT           reset()             IDLE              engine.reset()
```

### State Table

| State | Allowed Input | Maximum Time | Action on Exit |
|--------|-------------------|--------------|--------------|
| IDLE | start | ∞ | - |
| RUNNING | input | timeout | check input |
| INPUT_CORRECT | timer | 500ms | return to RUNNING |
| INPUT_WRONG | timer/reset | 1000ms | reset or RUNNING |
| SOLVED | reset | ∞ | - |
| ERROR | reset | ∞ | - |
| TIMEOUT | reset | ∞ | - |

---

## Algorithm Complexity

| Operation | Time Complexity | Space Complexity |
|----------|---------------------|---------------------|
| setup() | O(n) n=inputCount | O(1) |
| update() | O(n) n=inputCount | O(1) |
| detectRisingEdge() | O(1) | O(1) |
| updateProgress() | O(1) | O(1) |
| updateNeoPixel() | O(n) n=ledCount | O(1) |
| FisherYatesShuffle() | O(n) | O(n) |
| generateRandomSequence() | O(n) | O(n) |
| checkSequence() | O(m) m=currentLen | O(1) |

---

## Restrictions and Limitations

### Hardware Limitations

| Resource | Limit |
|---------|--------|
| Digital inputs | 16 (PCF8575) |
| PWM outputs (PCA9685) | 16 channels |
| NeoPixel LEDs | 16 (recommended) / 48 (maximum) |
| Audio files | 16 input sounds |
| JSON size | 4KB (LittleFS) |

### Time Limitations

| Operation | Maximum Time |
|----------|--------------|
| Debounce | Configurable (default: 50ms) |
| Input response | < 10ms |
| LED update | < 1ms (16 LEDs) |
| Play MP3 sound | ~100ms (buffering) |

---

## Execution Examples

### Example 1: Correct Sequence

```
INPUT: sequence = [1, 3, 5], input = 1

1. currentStep = 0
2. expected = sequence[0] = 1
3. input == expected? YES
4. currentStep = 1
5. progress = 33%
6. playInputSound(0)
7. state = INPUT_CORRECT

RESULT: Progress 33%, waiting for next input
```

### Example 2: Wrong Sequence

```
INPUT: sequence = [1, 3, 5], input = 2

1. currentStep = 0
2. expected = sequence[0] = 1
3. input == expected? NO
4. playErrorSound()
5. state = INPUT_WRONG
6. IF resetOnError: reset()

RESULT: Sequence restarted (if configured)
```

### Example 3: ALL_CONNECTED Mode

```
INPUT: count = 4, active inputs = [0, 2]

1. activeCount = 2
2. currentStep = 2
3. progress = 50%
4. activeCount < count? YES
5. Continues waiting

NEXT: Wait for inputs 1 and 3
```

---

## References

- PropMakerEscapeRoom Library
- PCF8575 Datasheet (Texas Instruments)
- RP2040 Datasheet (Raspberry Pi)
- I2C Protocol (Philips/NXP)

---

## Implementation Notes

1. **Thread Safety:** The algorithm assumes single-threaded execution. In multi-threaded environments, use mutexes to protect shared state.

2. **Memory:** State arrays (lastInputState, currentInputState) must be zeroed at boot to avoid undefined values.

3. **Randomization:** In RANDOM mode, use a seed based on an unconnected analog input or timestamp for true randomness.

4. **Debouncing:** The current implementation uses software debounce. Hardware with capacitors may allow reducing debounceMs.

5. **LittleFS:** Configuration must be loaded before initializing peripherals to avoid inconsistent states.

---

*The document was generated according to the ALGORITMO.MD specification*.
*Last update: April 2026*
