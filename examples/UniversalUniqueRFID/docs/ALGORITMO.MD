# ALGORITHM - UniversalUniqueRFID

## General Information

- **Algorithm Name:** UniversalUniqueRFID
- **Version:** 1.0
- **Creation Date:** April 2026
- **Author:** Escape Room Puzzle System
- **Platform:** Arduino (RP2040)
- **Base Library:** PropMakerEscapeRoom

---

## Objective

Implement a universal RFID reader configurable via JSON for escape room puzzles, supporting UNIQUE (any programmed tag solves) and SEQUENCE (tags in a specific order) modes, with integrated audiovisual feedback and optional RS485 communication.

---

## Data Structure

### Enumerated Types

```cpp
enum class RfidMode {
    UNIQUE,    // Any programmed tag solves
    SEQUENCE   // Tags in a specific sequence
};

enum class EngineState {
    IDLE,            // Waiting for start
    RUNNING,         // Processing tag readings
    TAG_CORRECT,     // Correct tag feedback
    TAG_WRONG,       // Wrong tag feedback
    SOLVED,          // Puzzle complete
    ERROR,           // Error
    TIMEOUT          // Time expired
};
```

### Main Structures

```cpp
struct RfidTag {
    char uid[16];      // UID in hex (e.g. "A1B2C3D4")
    char name[24];     // Friendly name
};

struct ValidTag {
    char uid[16];
    char name[24];
};

struct RfidConfig {
    RfidMode mode;
    RfidTag tags[16];         // Tags used in the puzzle
    uint8_t tagCount;
    ValidTag validTags[32];   // Known valid tags
    uint8_t validTagCount;
    uint8_t sequence[16];     // Sequence (indices)
    uint16_t timeoutSeconds;
    bool resetOnError;
};
```

---

## Main Flowchart

```
                                BOOT
                                  |
                                  v
                        INITIALIZATION (setup)
     ----------------------------------------------------
     | 1. Initialize Hardware (hw.begin())              |
     | 2. Mount LittleFS                                |
     | 3. Load config.json (or defaults)               |
     | 4. Initialize PN532 (NfcReader)                 |
     | 5. Initialize NeoPixel                          |
     | 6. Configure lock output                        |
     | 7. Initialize PuzzleEngine                      |
     ----------------------------------------------------
                                  |
                                  v
                            MAIN LOOP
                                  |
            ----------------------+---------------------
            |                     |                     |
            v                     v                     v
    hw.sound().loop()     puzzle.update()      processSerial()
                                  |
                                  v
                          STATE MACHINE
                                  |
            ----------------------+---------------------
            |                     |                     |
            v                     v                     v
          IDLE                 RUNNING               SOLVED
                                  |
            ----------------------+---------------------
            |                     |
            v                     v
      TAG_CORRECT             TAG_WRONG
                                  |
                                  v
                              TIMEOUT
```

---

## Processing Algorithm per Mode

### UNIQUE Mode

```
ALGORITHM: ProcessUniqueMode
─────────────────────────────────
INPUT: None (uses global state)
OUTPUT: Updated state

1. IF !readTag() THEN RETURN
2. IF tagCount == 0 THEN RETURN

3. FOR each tag i FROM 0 TO tagCount DO:
     3.1. convert tags[i].uid to bytes
     3.2. IF bytes == lastUid THEN:
            3.2.1. onTagCorrect(i)
            3.2.2. state <- SOLVED
            3.2.3. RETURN

4. onTagWrong()
5. END
```

### SEQUENCE Mode

```
ALGORITHM: ProcessSequenceMode
─────────────────────────────────
INPUT: None
OUTPUT: Updated state

1. IF !readTag() THEN RETURN
2. IF tagCount == 0 OR sequenceLength == 0 THEN RETURN

3. tagIndex <- findTagIndex(lastUid, lastUidLen)
4. IF tagIndex < 0 THEN:
     4.1. onTagWrong()
     4.2. RETURN

5. expectedIndex <- sequence[currentStep]
6. IF tagIndex == expectedIndex THEN:
     6.1. onTagCorrect(tagIndex)
     6.2. currentStep <- currentStep + 1
     6.3. IF currentStep >= sequenceLength THEN:
            6.3.1. onSequenceComplete()
   ELSE:
     6.4. onTagWrong()

7. END
```

---

## RFID Tag Reading Algorithm

```
ALGORITHM: readTag
────────────────────────────
INPUT: None
OUTPUT: boolean (true if new tag detected)

1. card <- reader.readCard(100ms)  // 100ms timeout
2. IF !card.present THEN:
     2.1. lastUidLen <- 0
     2.2. RETURN false

3. IF card.uidLength == lastUidLen AND
      memcmp(card.uid, lastUid) == 0 THEN:
     3.1. RETURN false  // Same tag

4. lastUidLen <- card.uidLength
5. memcpy(lastUid, card.uid, lastUidLen)

6. uidHex <- bytesToHex(card.uid)
7. playTagSound()

8. RETURN true

COMPLEXITY: O(1)
```

---

## UID Conversion Algorithm

```
ALGORITHM: hexToBytes
────────────────────────────
INPUT: hex (hexadecimal string), bytes (output array)
OUTPUT: len (size in bytes)

1. len <- 0
2. FOR i FROM 0 TO strlen(hex) STEP 2 DO:
     2.1. IF len >= 7 THEN BREAK
     2.2. byteStr <- hex[i..i+1]
     2.3. bytes[len] <- strtoul(byteStr, NULL, 16)
     2.4. len <- len + 1

3. END


ALGORITHM: bytesToHex
────────────────────────────
INPUT: bytes (byte array), len (size)
OUTPUT: hex (hexadecimal string)

1. FOR i FROM 0 TO len DO:
     1.1. sprintf(hex + i*2, "%02X", bytes[i])

2. hex[len*2] <- '\0'
3. END


ALGORITHM: findTagIndex
────────────────────────────
INPUT: uid (bytes), uidLen
OUTPUT: tag index or -1

1. FOR i FROM 0 TO tagCount DO:
     1.1. convert tags[i].uid to bytes (tagBytes, tagLen)
     1.2. IF tagLen == uidLen AND memcmp(uid, tagBytes, tagLen) == 0 THEN:
            1.2.1. RETURN i

2. RETURN -1

COMPLEXITY: O(n * m) where n = tagCount, m = uidLen
```

---

## State Machine

### Transition Diagram

```
Current State     Event                Next State        Action
─────────────────────────────────────────────────────────────────
IDLE              start()              RUNNING           engine.start()
RUNNING           correctTag           TAG_CORRECT       playCorrect()
RUNNING           wrongTag             TAG_WRONG         playWrong()
RUNNING           timeout              TIMEOUT           onTimeout()
TAG_CORRECT       timer500ms           RUNNING           -
TAG_WRONG         resetOnError=true    IDLE              engine.reset()
TAG_WRONG         resetOnError=false   RUNNING           -
RUNNING           puzzleComplete       SOLVED            onSequenceComplete()
SOLVED            reset()              IDLE              engine.reset()
TIMEOUT           reset()              IDLE              engine.reset()
```

### State Table

| State | Event | Maximum Time | Action on Exit |
|--------|--------|--------------|--------------|
| IDLE | start | infinite | - |
| RUNNING | tag read | timeout | check tag |
| TAG_CORRECT | timer | 500ms | return to RUNNING |
| TAG_WRONG | timer/reset | 1000ms | reset or RUNNING |
| SOLVED | reset | infinite | - |
| ERROR | reset | infinite | - |
| TIMEOUT | reset | infinite | - |

---

## Algorithm Complexity

| Operation | Time Complexity | Space Complexity |
|----------|---------------------|---------------------|
| setup() | O(n) n=tagCount | O(1) |
| update() | O(1) (polling 250ms) | O(1) |
| readTag() | O(1) (NFC read) | O(1) |
| findTagIndex() | O(n) n=tagCount | O(1) |
| hexToBytes() | O(m) m=uidLen | O(1) |
| bytesToHex() | O(m) m=uidLen | O(1) |
| onSequenceComplete() | O(n) n=ledCount | O(1) |

---

## Restrictions and Limitations

### Hardware Limitations

| Resource | Limit |
|---------|--------|
| Programmed tags | 16 |
| Valid tags | 32 |
| UID length | 7 bytes max |
| NeoPixel LEDs | 8 (recommended) / 48 (maximum) |
| Audio files | 6 |
| JSON size | 4KB (LittleFS) |

### PN532 Limitations

| Parameter | Specification |
|-----------|---------------|
| Frequency | 13.56 MHz |
| Distance | ~5cm (depends on the antenna) |
| Protocols | ISO14443A (MIFARE, NTAG) |
| Interface | I2C (default), SPI, UART |
| Read time | ~20-100ms per tag |

---

## Execution Examples

### Example 1: UNIQUE Mode - Correct Tag

```
INPUT: tags = [TagA(UID=A1B2C3D4)], tag read = A1B2C3D4

1. readTag() -> true (UID = A1B2C3D4)
2. findTagIndex(A1B2C3D4) -> 0
3. tags[0] found
4. onTagCorrect(0)
5. currentStep = 1, progress = 100%
6. playCorrectSound()
7. onSequenceComplete() -> state = SOLVED

RESULT: Puzzle solved, lock released
```

### Example 2: SEQUENCE Mode - Tag in Order

```
INPUT: sequence = [0, 2, 1], tag read = tags[0]

1. currentStep = 0
2. expected = sequence[0] = 0
3. tagIndex = findTagIndex(uid) -> 0
4. tagIndex == expected? YES
5. currentStep = 1, progress = 33%
6. playCorrectSound()

RESULT: Progress 33%, waiting for tags[2]
```

### Example 3: SEQUENCE Mode - Wrong Tag

```
INPUT: sequence = [0, 2, 1], tag read = tags[1]

1. currentStep = 0
2. expected = sequence[0] = 0
3. tagIndex = findTagIndex(uid) -> 1
4. tagIndex == expected? NO
5. playWrongSound()
6. state = TAG_WRONG
7. IF resetOnError: reset()

RESULT: Sequence restarted (if configured)
```

---

## References

- PropMakerEscapeRoom Library
- PN532 Datasheet (NXP)
- RP2040 Datasheet (Raspberry Pi)
- I2C Protocol (Philips/NXP)
- ISO/IEC 14443 (Proximity Cards)

---

## Implementation Notes

1. **Polling:** The NFC is read every 250ms to avoid blocking the main loop. The PN532 read timeout is 100ms.

2. **Deduplication:** Repeated UIDs are ignored until the tag is removed from the field. This prevents multiple detection of the same tag.

3. **UID Comparison:** UIDs in hex in the JSON are converted to bytes and compared byte-by-byte with the UID read by the PN532.

4. **Valid tags:** The `validTags` field stores all known tags of the system, even those not in the current puzzle. This makes it easier to swap tags between puzzles without reprogramming.

5. **LittleFS:** Configuration must be loaded before initializing peripherals to avoid inconsistent states.

---

*The document was generated according to the ALGORITMO.MD specification*.
*Last update: April 2026*
