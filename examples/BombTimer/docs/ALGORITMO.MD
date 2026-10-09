# GameTimerPro - Algorithm and Complete Structure (PropMaker EscapeRoom)

## Overview

GameTimerPro is a "bomb"-type timer system for escape rooms, based on the **Adafruit PropMaker Feather RP2040** board and using the **PropMaker EscapeRoom** library.

**Dual-Core Architecture (FreeRTOS):**
- Core 0: Game logic (puzzle, RS485, inputs, menu)
- Core 1: Audio system (MP3 decoder via I2S)

---

## Function Index

### 1. Structures and Classes
- [DisplayWrapper](#displaywrapper)
- [GameTimerPuzzle](#gametimerpuzzle)
- [KeypadHandler](#keypadhandler)

### 2. Setup and Initialization
- [setup()](#setup)
- [setupHardware()](#setuphardware)
- [loadConfig()](#loadconfig)
- [startCountdown()](#startcountdown)
- [resetSystem()](#resetsystem)

### 3. Main Loop and States
- [loop()](#loop)
- [update()](#update)
- [countdown()](#countdown)

### 4. Input and Controls
- [getTimerInput()](#gettimerinput)
- [handleTimeAdjustment()](#handletimeadjustment)
- [checkDetButton()](#checkdetbutton)
- [checkAccelerometer()](#checkaccelerometer)
- [inputCode()](#inputcode)
- [inputWireOrder()](#inputwireorder)

### 5. Wire Verification
- [checkWiresNormal()](#checkwiresnormal)
- [checkWiresOrder()](#checkwiresorder)
- [wireCut()](#wirecut)
- [wireToPin()](#wiretopin)
- [wirePinToIndex()](#wirepintoindex)
- [wireConflict()](#wireconflict)
- [pauseCountdown()](#pausecountdown)

### 6. Keypad Verification
- [checkKeypadCode()](#checkkeypadcode)
- [keyPressed()](#keypressed)
- [keyReleased()](#keyreleased)

### 7. Menu and Configuration
- [configure()](#configure)
- [handleSelectShort()](#handleselectshort)
- [handleSelectLong()](#handleselectlong)
- [handleMainMenuNavigation()](#handlemainmenunavigation)
- [handleWireSubmenuNavigation()](#handlewiresubmenunavigation)
- [handleReleSubmenuNavigation()](#handlerelesubmenunavigation)
- [handleRfidSubmenuNavigation()](#handlerfidsubmenunavigation)
- [handleAdjustMode()](#handleadjustmode)
- [exitAllMenus()](#exitallmenus)

### 8. Display and UI
- [showMenuItem()](#showmenuitem)
- [showWireSubmenuItem()](#showwiresubmenuitem)
- [showReleSubmenuItem()](#showrelesubmenuitem)
- [showRfidSubmenuItem()](#showrfidsubmenuitem)
- [adjustMenuItem()](#adjustmenuitem)
- [adjustWireSubmenuItem()](#adjustwiresubmenuitem)
- [adjustReleSubmenuItem()](#adjustrelesubmenuitem)
- [showCurrentValue()](#showcurrentvalue)
- [showCurrentWireValue()](#showcurrentwirevalue)
- [showCurrentReleValue()](#showcurrentrelevalue)
- [showShakeMode()](#showshakemode)
- [showWireValue()](#showwirevalue)
- [showWire()](#showwire)
- [showNum()](#shownum)
- [showBool()](#showbool)
- [updateDisplay()](#updatedisplay)

### 9. RFID (New)
- [showRfidSubmenuItem()](#showrfidsubmenuitem)
- [inputRfidTag()](#inputrfidtag)
- [checkRfidDuringCountdown()](#checkrfidduringcountdown)
- [hexStringToBytes()](#hexstringtobytes)
- [bytesToHexString()](#bytestohexstring)

### 9. Effects and Feedback
- [setNeoEffect()](#setneoeffect)
- [updateNeoEffect()](#updateneoeffect)
- [doExplosionEffect()](#doexplosioneffect)
- [tick()](#tick)
- [beep()](#beep)

### 10. Final Actions
- [detonate()](#detonate)
- [defused()](#defused)

### 11. Persistence
- [saveConfig()](#saveconfig)
- [getRandomWire()](#getrandomwire)
- [wireConflict()](#wireconflict)

### 12. Utilities
- [constrainValue()](#constrainvalue)
- [onNetworkPacket()](#onnetworkpacket)

### 13. IoAbstraction Callbacks
- [onButtonPressed()](#onbuttonpressed)
- [onWireCut()](#onwirecut)

---

## 1. Structures and Classes

### DisplayWrapper
Wrapper class for the HT16K33 display that redirects to Serial if the display is not detected.

**Methods:**
- `checkDisplay()` → bool: Checks whether the display is available via `hw.hasDisplay()`
- `setDisplayColon(bool on)` → void: Controls the display colon
- `printNumber(int num)` → void: Displays a number (0-9999)
- `printTime(int min, int sec)` → void: Displays time in MM:SS format
- `printString(const char* str)` → void: Displays a string of up to 4 characters
- `writeDigitAscii(int digit, char c)` → void: Writes a character at a specific position
- `clear()` → void: Clears the display
- `setBrightness(uint8_t b)` → void: Adjusts brightness (0-15)

### GameTimerPuzzle
Main class that inherits from `Puzzle`.

**Public members:**
- `setupHardware()` → void: Initializes hardware and callbacks
- `update()` → void: Main state loop
- `onNetworkPacket(const Rs485Payload& p)` → void: RS485 command handler

**Attempt control variables (CODE mode):**
- `currentCode` (int): Current code entry attempt (1-3)
- `maxCode` (int): Maximum allowed attempts (default: 3)

### KeypadHandler
Class for handling keypad events.

**Methods:**
- `keyPressed(char key, bool held)` → void: Callback when a key is pressed
- `keyReleased(char key)` → void: Callback when a key is released

---

## 2. Setup and Initialization

### setup()
**Location:** line 1832
**Description:** Standard Arduino setup() function.

**Flow:**
1. Initializes Serial at 115200 baud
2. Calls `hw.begin()` to initialize hardware
3. Calls `puzzle.setupHardware()`
4. Calls `puzzle.loadConfig()`
5. Displays the loaded values on Serial
6. If RS485 is enabled, sends `hello` on the network

### setupHardware()
**Location:** line 258
**Description:** Initializes all hardware components.

**Flow:**
1. Configures analog inputs (PA0-PA1 as OUTPUT LOW)
2. Configures button callbacks via IoAbstraction
3. Configures wire callbacks (INPUT_PULLUP)
4. Configures a 4x3 matrix keypad
5. Loads configuration via `loadConfig()`
6. Sets initial state to Idle
7. Updates the display with the initial time
8. Configures NeoPixel with the NORMAL effect

### loadConfig()
**Location:** line 1698
**Description:** Loads configuration from the JSON file in LittleFS.

**JSON parameters:**
- `ctdn`: Countdown time (default: 600)
- `mode`: Defuse mode (default: 0/CODE)
- `code`: Defuse code (default: 1234)
- `ordr`: Wire order (default: 1234)
- `detW`, `dffW`, `spdW`, `psW`: Wire configuration
- `vol`: Volume (default: 128)
- `bri`: Display brightness (default: 15)
- `tdEt`, `tdEF`: Trigger times
- `rs485`: Enables RS485 (default: false)
- `netC`: Number of network puzzles (default: 4)
- `accl`: Accelerometer mode (default: 0/OFF)
- `tic`: Tick sound enabled (default: true)

**If the file does not exist:** Creates it with default values

### startCountdown()
**Location:** line 951
**Description:** Prepares and starts the countdown.

**Flow:**
1. Checks whether the countdown is already running
2. Draws wires configured as RAND via `getRandomWire()`
3. Loads the time from the configuration
4. Resets flags: defused, paused, double speed
5. Clears the history of cut wires
6. Loads the sequence if in ORDER mode
7. **`currentCode = 1` (restarts the attempt counter for CODE mode)**
8. Plays the armed sound (`/armed.mp3`)
9. Activates NeoPixel with the COUNTING effect
10. Activates the display colon
11. Transitions to the Running state

### resetSystem()
**Location:** line 920
**Description:** Resets the system to the initial state.

**Resets:**
- `countdownRunning = false`
- `doubleSpeed = false`
- `isDefused = false`
- `enteringCode = false`
- **`currentCode = 1` (restarts the attempt counter)**
- Menu flags (inMenu, inAdjustMode, etc)
- Cut wire states
- Resolved network puzzles
- Time to the configuration value
- NeoPixel to the NORMAL effect
- State to Idle

---

## 3. Main Loop and States

### loop()
**Location:** line 1852
**Description:** Empty main loop (logic runs in FreeRTOS tasks).

**Note:* In production with FreeRTOS, `loop()` remains empty because tasks are executed on the cores.

### update()
**Location:** line 321
**Description:** Main state machine called by the task.

**States:**
- **Idle:** Calls `getTimerInput()` if not running, otherwise `startCountdown()`
- **Running:** Calls `countdown()`
- **Solved:** Waits for the DET button to restart
- **Inactive:** Waits for the DET button to restart

**RS485 commands processed:**
- `SyncTimer`: Synchronizes time and starts
- `ForceSolve`: Marks as defused
- `ForceDetonate`: Forces detonation
- `StatusReport`: Updates the state of network puzzles

**Attempt System (CODE mode):**
- The player has 3 attempts to get the code right
- On each error: shows "_Err" and allows a new entry
- 4th error: immediate detonation
- The counter restarts on each new game (`resetSystem` / `startCountdown`)

### countdown()
**Location:** line 584
**Description:** Active countdown loop.

**Per-cycle flow:**
1. Checks whether the accelerometer pause has ended
2. Updates the display with the MM:SS time
3. Blinks the colon every second
4. Processes RS485 commands
5. Checks the accelerometer via `checkAccelerometer()`
6. Checks wires via `checkWiresNormal()` or `checkWiresOrder()`
7. Checks the keypad if in CODE mode via `checkKeypadCode()`
8. Decrements the time every 1s (or 0.5s in double speed)
9. Plays the tick if enabled
10. If time reaches 0: calls `detonate()`
11. If defused: calls `defused()`

---

## 4. Input and Controls

### getTimerInput()
**Location:** line 395
**Description:** Processes input when in the Idle state.

**Flow:**
1. If in the menu: calls `configure()`
2. Detects SELECT pressed to enter the menu
3. If SELECT released short (<800ms): enters the menu
4. If SELECT released long (>=800ms): saves config
5. DET button: starts countdown via `checkDetButton()`
6. Time adjustment via `handleTimeAdjustment()`
7. **NEW:** Checks accelerometer in START mode while Idle

### handleTimeAdjustment()
**Location:** line 469
**Description:** Adjusts the countdown time while Idle.

**Controls:**
- **LEFT short:** -1 second (minimum: 10s)
- **LEFT held (>600ms):** -5s every 150ms
- **RIGHT short:** +1 second (maximum: 5999s)
- **RIGHT held (>600ms):** +5s every 150ms
- **Auto-save:** Saves after 2 seconds without adjustments

**Static variables:**
- `leftPressStart`, `rightPressStart`: Press start timestamp
- `leftWasPressed`, `rightWasPressed`: State flags
- `lastAdjTime`: Last adjustment timestamp
- `timeChanged`: Auto-save flag

### checkDetButton()
**Location:** line 562
**Description:** Starts the countdown when the DET button is pressed.

**Parameters:**
- `trigger`: Trigger type (0=button, 0x10=CMD_START)

**Flow:**
1. Ignores if CMD_START or already running
2. Logs to Serial
3. Activates `countdownRunning`
4. Resets the accelerometer pause state
5. Red flash on the NeoPixel (3x)
6. Transitions to Running

### checkAccelerometer()
**Location:** line 643
**Description:** Checks the accelerometer shake.

**Modes:**
- **ACCEL_START (1):** Starts countdown while Idle (threshold: 12.0 m/s²)
- **ACCEL_SPEED2X (2):** Activates double speed (threshold: 12.0 m/s²)
- **ACCEL_PAUSE (3):** Pauses for 2 seconds (threshold: 10.0 m/s²)
  - Plays error.mp3
  - Freezes the display
  - Loses 2 seconds when resuming
- **ACCEL_DETONATE (4):** Detonates immediately (threshold: 18.0 m/s²)

**NEW - PAUSE Mode:**
- When shake is detected: `accelPauseActive = true`
- error.mp3 sound played via `playOnce()`
- Display frozen for 2s
- At the end: `lastSecondAt = millis()` (loses time)

### inputCode()
**Location:** line 1475
**Description:** Defuse code entry via keypad.

**Flow:**
1. Accumulates 4 digits in a buffer
2. Compares with `config.defuseCode`
3. If equal: sets `isDefused = true`
4. If different: zeroes the time (explodes)
5. keypress.mp3 sound on each digit

### inputWireOrder()
**Location:** line 1517
**Description:** Wire order entry via keypad.

**Format:** 4 unique digits (1-4 without repetition)
**Stores in:** `config.wireOrder[4]`

---

## 5. Wire Verification

### checkWiresNormal()
**Location:** line 693
**Description:** Checks wires in normal mode (not ORDER).

**Checks:**
1. **DET wire:** If cut → zeroes the time (explodes)
2. **DEF wire (WIRE mode):** If cut → `isDefused = true`
3. **PAUS wire:** If cut and not used → pauses 2s via `pauseCountdown()`
4. **SPD wire:** If cut → `doubleSpeed = true`
5. **All cut:** Zeroes the time (explodes)

### checkWiresOrder()
**Location:** line 748
**Description:** Checks wires in ORDER mode (specific order).

**Flow:**
- For each wire cut in the correct order
- If the order is wrong → zeroes the time (explodes)
- If it is the last of the sequence → `isDefused = true`

### wireCut()
**Location:** line 830
**Description:** Checks whether a specific wire is cut.

**Parameters:**
- `wire`: Wire number (WIRE1-WIRE4)

**Return:** bool - true if the circuit is open (cut)

**Implementation:**
- Uses `switches.isSwitchPressed()` via IoAbstraction
- Wires are INPUT_PULLUP (cut = HIGH)

### wireToPin()
**Location:** line 820
**Description:** Converts a wire number to a physical pin.

**Mapping:**
- WIRE1 → WIRE1_PIN (12/P14)
- WIRE2 → WIRE2_PIN (13/P15)
- WIRE3 → WIRE3_PIN (14/P16)
- WIRE4 → WIRE4_PIN (15/P17)

### wirePinToIndex()
**Location:** line 1808
**Description:** Converts a pin to a wire index (0-3).

**Return:** int (0-3) or -1 if invalid

### pauseCountdown()
**Location:** line 740
**Description:** Pauses the countdown for 2 seconds.

**Actions:**
- Sets `pauseStartTime = millis()`
- Sets `isPaused = true`
- Plays the pause sound
- After 2s, `isPaused = false`

---

## 6. Keypad Verification

### checkKeypadCode()
**Location:** line 772
**Description:** Checks code entry in CODE mode with a 3-attempt system.

**Flow:**
1. Processes accumulated digits
2. When 4 digits are complete
3. Compares with the configured code
4. **Success:** → `isDefused = true`
5. **Failure (wrong code):**
   - Increments `currentCode` (current attempt)
   - If `currentCode <= maxCode` (1-3):
     - Shows "_Err" on the display
     - Allows a new entry (`enteringCode = true`)
     - `currentDigit = 1` (keeps the first digit entered)
   - If `currentCode > maxCode` (4th attempt):
     - Zeroes the time (explodes)
     - Serial message: "Wrong code! Exploding..."

**Variables:**
- `currentCode`: Starts at 1, increments on each error
- `maxCode`: 3 (maximum attempts)

**Resets in:**
- `resetSystem()` → `currentCode = 1`
- `startCountdown()` → `currentCode = 1`

### keyPressed()
**Location:** line 237
**Description:** IoAbstraction callback when a key is pressed.

**Stores:** `lastKey = key`

### keyReleased()
**Location:** line 238
**Description:** IoAbstraction callback when a key is released.

**Clears:** `lastKey = 0`

---

## 7. Menu and Configuration

### configure()
**Location:** line 1019
**Description:** Processes menu navigation and adjustments.

**Modes:**
- Main menu: `handleMainMenuNavigation()`
- WIRE submenu: `handleWireSubmenuNavigation()`
- RELE submenu: `handleReleSubmenuNavigation()`
- Adjust mode: `handleAdjustMode()`

### handleSelectShort()
**Location:** line 1067
**Description:** Processes a short SELECT (<800ms).

**Actions per context:**
- Outside the menu: enters the menu
- Main menu: enters adjustment or submenu
- Submenu: enters adjustment
- Adjust mode: exits adjustment

### handleSelectLong()
**Location:** line 1116
**Description:** Processes a long SELECT (>=800ms) - saves and goes back a level.

**Actions per context:**
- Adjust mode: saves, exits adjustment, shows item
- Submenu: saves, returns to the main menu
- Main menu: saves, exits the menu completely

### handleMainMenuNavigation()
**Location:** line 1155
**Description:** LEFT/RIGHT navigation in the main menu.

**Items:* BRIGHT → VOLUME → TICK → SHAK → NET → WIRE → CODE → RELAY

### handleWireSubmenuNavigation()
**Location:** line 1170
**Description:** LEFT/RIGHT navigation in the WIRE submenu.

**Items:** ORDR → DET → DEF → SPD → PAUS

### handleReleSubmenuNavigation()
**Location:** line 1185
**Description:* Left/Right navigation in the RELE submenu.

**Items:** TDET → TDEF

### handleAdjustMode()
**Location:** line 1200
**Description:** Adjusts values in adjust mode.

**Controls:**
- LEFT: decreases the value
- RIGHT: increases the value

### exitAllMenus()
**Location:** line 1059
**Description:** Exits all menu levels.

**Resets:**
- `inMenu = false`
- `inWireSubmenu = false`
- `inReleSubmenu = false`
- `inAdjustMode = false`

---

## 8. Display and UI

### showMenuItem()
**Location:** line 1236
**Description:** Displays the name of the current main menu item.

**Names:** “bri”, “vol”, “tck”, “accl”, “r485”, “WIRE”, “code”, “RELE”

### showWireSubmenuItem()
**Location:** line 1254
**Description:** Displays the name of the WIRE submenu item.

**Names:** "ORDR", "DET ", "DEF ", "SPD ", "PAUS"

### showReleSubmenuItem()
**Location:** line 1269
**Description:* Displays the name of the RELE submenu item.

**Names:** "TDET", "TDEF"

### adjustMenuItem()
**Location:** line 1281
**Description:** Adjusts the value of a main menu item.

**Items:**
- 0 (BRIGHT): 0-15
- 1 (VOLUME): 0-15
- 2 (TICK): boolean
- 3 (SHAK): 0-4 (OFF/STRT/FAST/PAUS/BOOM)
- 4 (NET): boolean
- 5 (WIRE): enters submenu
- 6 (CODE): entry via keypad
- 7 (RELE): enters submenu

### adjustWireSubmenuItem()
**Location:** line 1334
**Description:** Adjusts the value of a WIRE submenu item.

**Conflict Validation:**
- Before assigning a new value, calls `wireConflict()`
- If the wire is already assigned to another function: shows "err " and plays error.mp3
- The previous value is kept

**Items:**
- ORDR: entry via keypad
- DET/DEF/SPD/PAUS: NONE/1/2/3/4 (with validation via wireConflict)

### adjustReleSubmenuItem()
**Location:** line 1364
**Description:* Adjusts the value of a RELE submenu item.

**Items:** TDET/TDEF (1-10 seconds)

### showCurrentValue()
**Location:** line 1386
**Description:** Displays the current value of the item being adjusted.

### showCurrentWireValue()
**Location:** line 1400
**Description:** Displays the current value of the WIRE item being adjusted.

### showCurrentReleValue()
**Location:** line 1410
**Description:* Displays the current value of the RELE item that is being adjusted.

### showShakeMode()
**Location:** line 1417
**Description:** Displays the name of the accelerometer mode.

**Names:** "OFF ", "STRT", "FAST", "PAUS", "BOOM"

### showWireValue()
**Location:** line 1437
**Description:** Displays the wire value (0-9).

### showWire()
**Location:** line 1448
**Description:** Displays the wire name (NONE/1/2/3/4).

### showNum()
**Location:** line 1457
**Description:** Displays a number on the display.

### showBool()
**Location:** line 1464
**Description:** Displays ON/OFF on the display.

### updateDisplay()
**Location:** line 1568
**Description:** Updates the display with the current MM:SS time.

---

## 9. Effects and Feedback

### setNeoEffect()
**Location:** line 1621
**Description:** Sets the current NeoPixel effect.

**Effects:**
- NEOFX_OFF (0): Off
- NEOFX_NORMAL (1): Soft white
- NEOFX_COUNTING (2): Pulsing red
- NEOFX_DOUBLE_SPEED (3): Fast blinking red
- NEOFX_DEFUSED (4): Bright green
- NEOFX_EXPLOSION (5): Yellow→orange→red sequence
- NEOFX_RADIATION (6): Blinking green

### updateNeoEffect()
**Location:** line 1626
**Description:** Updates the NeoPixel state based on the current effect.

### doExplosionEffect()
**Location:** line 1671
**Description:** Runs the explosion sequence on the NeoPixel.

**Sequence:**
1. Intense white flash
2. Bright yellow
3. Intense orange
4. Vivid red
5. Fade to dark red
6. Random sparks

### tick()
**Location:** line 1572
**Description:** Plays the tick sound (/tick.mp3).

### beep()
**Location:** line 1578
**Description:** Plays the beep sound (/keypress.mp3).

---

## 10. Final Actions

### detonate()
**Location:** line 837
**Description:** Runs the detonation sequence.

**Flow:**
1. Activates the EXPLOSION effect on the NeoPixel
2. Plays the /boom.mp3 sound
3. Shows "boom" on the display
4. Activates the detonation trigger for `config.detTriggerSec`
5. Sets the state to Inactive

### defused()
**Location:** line 871
**Description:** Runs the defuse sequence.

**Flow:**
1. Activates the DEFUSED effect (green)
2. Plays the /sucess.mp3 sound
3. Shows the defuse code on the display
4. Activates the defuse trigger for `config.defuseTriggerSec`
5. Sets the state to Solved

---

## 11. Persistence

### saveConfig()
**Location:** line 1750
**Description:** Saves the current configuration to the JSON file.

**Saves:** All configuration parameters to `/bomb.json`

### getRandomWire()
**Location:** line 977
**Description:** Draws wires configured as RAND.

**Guarantees:**
- Each wire has at most one function
- DET, DEF, SPD, PAUS are mutually exclusive

### wireConflict()
**Location:** line 1003
**Description:** Checks whether a new wire value conflicts with others.

**Parameters:**
- `key`: Which wire is being adjusted
- `value`: New value

**Return:** bool - true if there is a conflict

---

## 12. Utilities

### constrainValue()
**Location:** line 1380
**Description:** Constrains a value between a minimum and a maximum.

### onNetworkPacket()
**Location:** line 360
**Description:** Processes received RS485 packets.

**Commands:**
- `SyncTimer`: Synchronizes time
- `ForceSolve`: Forces defuse
- `ForceDetonate`: Forces detonation
- `StatusReport`: Updates the state of network puzzles

---

## 13. IoAbstraction Callbacks

### onButtonPressed()
**Location:** line 1784
**Description:** Callback when a button is pressed.

### onWireCut()
**Location:** line 1818
**Description:** Callback when a wire is cut (open circuit).

---

## Menu Constants

```cpp
// Main menu (8 items)
#define MENU_ITEM_BRIGHTNESS    0   // "bri "
#define MENU_ITEM_VOLUME        1   // "vol "
#define MENU_ITEM_TICK          2   // "tck "
#define MENU_ITEM_ACCEL         3   // "accl"
#define MENU_ITEM_RS485         4   // "r485"
#define MENU_ITEM_WIRE          5   // "WIRE"
#define MENU_ITEM_CODE          6   // "code"
#define MENU_ITEM_RELE          7   // "RELE"

// WIRE submenu (5 items)
#define WIRE_SUBITEM_ORDR       0   // "ORDR"
#define WIRE_SUBITEM_DET        1   // "DET "
#define WIRE_SUBITEM_DEF        2   // "DEF "
#define WIRE_SUBITEM_SPD        3   // "SPD "
#define WIRE_SUBITEM_PAUS       4   // "PAUS"

// RELE submenu (2 items)
#define RELE_SUBITEM_TDET       0   // "TDET"
#define RELE_SUBITEM_TDEF       1   // "TDEF"

// Accelerometer modes
#define ACCEL_OFF           0   // Off
#define ACCEL_START         1   // Starts countdown when moved
#define ACCEL_SPEED2X       2   // 2x speed
#define ACCEL_PAUSE         3   // Pauses 2 seconds
#define ACCEL_DETONATE      4   // Detonates immediately

// Controls
#define SELECT_LONG_PRESS_MS    800     // Time for long press
```

---

## Required Audio Files

| File | Description | When Played |
|---------|-----------|---------------|
| `/armed.mp3` | Arming sound | When starting the countdown |
| `/tick.mp3` | Second tick | Every second of the countdown |
| `/boom.mp3` | Explosion | When detonating |
| `/sucess.mp3` | Success | When defusing |
| `/error.mp3` | Error | On wrong code or accelerometer pause |
| `/keypress.mp3` | Key beep | When pressing a button/keypad |
| `/pause.mp3` | Pause | When the pause wire is cut |

**Format:** MP3, 128kbps, 44.1kHz, stereo, no ID3 tags

---

## System State Flow

```
[INITIALIZATION]
      |
      v
[SETUP] --load error--> [RESET JSON]
      |
      v
[IDLE] <--restart--+--[SOLVED]  [INACTIVE]
      |             |      ^            ^
      |             |      |            |
      +--SELECT LP-->+      |            |
      |   (enters           |            |
      |    the menu)        |            |
      |                     |            |
      +--DET/SHAKE------>[RUNNING]-------+
      (START mode)        |              |
                    [checks wires]        |
                    [keypad/network]      |
                    [accelerometer]       |
                          |              |
                    [defuse]-----+        |
                    [timeout]    |        |
                    [detonate]---+--------+

[KEYPAD - CODE MODE]
      |
      +--Correct Code-->[defuse]
      |
      +--Wrong Code (1st-3rd attempt)-->[_Err]-->Retry
      |
      +--Wrong Code (4th attempt)-->[detonate]
```

---

## Best Practices

1. **Always call `hw.begin()` in setup** before using any hardware
2. **Call `hw.sound().loop()` in `loop()`** or run it in a separate task (Core 1)
3. **Use `taskManager.runLoop()`** for the keypad to work correctly
4. **Check `hw.hasExpander()`** before accessing inputs via the PCF8575
5. **Save the configuration via `saveConfig()`** after changes in the menu
6. **Format MP3s correctly:** 128kbps, 44.1kHz, no ID3 tags
7. **Check that the files exist** in LittleFS before trying to play
8. **Never assume a mount error** when the code stops after a change

---

*Document updated for the PropMaker EscapeRoom library - Complete version with all functions*