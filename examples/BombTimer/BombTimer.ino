/**
 * BombTimerPro v. 2.0 - Version ported to RP2040
 * By Djair Guilherme – (Nicolau dos Brinquedos)
 * do original Game Timer Pro, da nootropic design
 * https://nootropicdesign.com/game-timer-pro/
 * https://github.com/nootropicdesign/game-timer-pro
 *
 * Hardware: Adafruit PropMaker Feather RP2040
 * Uses the PropMakerEscapeRoom Engine, developed by Nicolau dos Brinquedos
 * - Configuration via LittleFS (JSON) - bomb.json file
 * - Inputs via PCF8575 (I2C) using IoAbstraction (Wires, Buttons and Keypad)
 * - HT16K33 display, 14-segment, 4 digits
 * - MP3 audio via Arduino-Audio-Tools (I2S) with Helix MP3 codec
 * - NeoPixel for visual effects (BOM 48 NeoPixel LEDs)
 * - LIS3DH accelerometer (native to the board)
 * - RS485 communication via MAX485 module with automatic flow control
 * - PN532 NFC/RFID reader (I2C) for defusing or detonating via tag
 *
 * NEW APPROACH: Only PropMakerEscapeRoom.h is needed - all
 * helper libraries (IoAbstraction, ArduinoJson, etc) are included
 * automatically by the engine.
 */

#include <PropMakerEscapeRoom.h>

using namespace propmaker_escaperoom;

// ============================================================================
// CONSTANTS (compatible with the original GameTimerPro)
// ============================================================================

// Defuse modes
#define DEFUSE_WIRE_CODE    0
#define DEFUSE_WIRE_ORDER   1
#define DEFUSE_WIRE_RAND    6
#define DET_WIRE_NONE       0
#define DET_WIRE_RAND       1
#define WIRE1               2
#define WIRE2               3
#define WIRE3               4
#define WIRE4               5

// RFID ON/OFF
#define RFID_OFF            0
#define RFID_ON             1

// Accelerometer modes
#define ACCEL_OFF           0
#define ACCEL_START         1   // Starts countdown when movement is detected
#define ACCEL_SPEED2X       2
#define ACCEL_PAUSE         3
#define ACCEL_DETONATE      4

// NeoPixel effects
#define NEOFX_OFF           0
#define NEOFX_NORMAL        1
#define NEOFX_COUNTING      2
#define NEOFX_DOUBLE_SPEED  3
#define NEOFX_DEFUSED       4
#define NEOFX_EXPLOSION     5
#define NEOFX_RADIATION     6

// Pins on the PCF8575 (IoAbstraction: 0-7=P0-P7, 8-15=P10-P17)
#define BUTTON_LEFT_PIN     8   // physical P10
#define BUTTON_SELECT_PIN   9   // physical P11
#define BUTTON_RIGHT_PIN    10  // physical P12
#define BUTTON_DET_PIN      11  // physical P13
#define WIRE1_PIN           12  // physical P14
#define WIRE2_PIN           13  // physical P15
#define WIRE3_PIN           14  // physical P16
#define WIRE4_PIN           15  // physical P17

// JSON configuration
#define CONFIG_FILE         "/bomb.json"

// ============================================================================
// NOTE ON ADDITIONAL HARDWARE VIA PropMakerEscapeRoom
// GLOBAL VARIABLES (original GameTimerPro names)
// ============================================================================

// ============================================================================
// ROBUST DISPLAY WRAPPER
// If display not detected, redirects to Serial.print
// ============================================================================
class DisplayWrapper {
public:
  bool checkDisplay() {
    return hw.hasDisplay();
  }

  void setDisplayColon(bool on) {
    if (checkDisplay()) {
      hw.display().setColon(on);
    }
  }

  void printNumber(int num) {
    if (checkDisplay()) {
      hw.display().printNum(num);
    } else {
      Serial.printf("[DISPLAY] Number: %d\n", num);
    }
  }

  void printTime(int min, int sec) {
    if (checkDisplay()) {
      hw.display().printTime(min, sec);
    } else {
      Serial.printf("[DISPLAY] Time: %02d:%02d\n", min, sec);
    }
  }

  void printString(const char* str) {
    if (checkDisplay()) {
      hw.display().printString(str);
    } else {
      Serial.printf("[DISPLAY] Text: %s\n", str);
    }
  }

  void writeDigitAscii(int digit, char c) {
    if (checkDisplay()) {
      hw.display().writeDigitAscii(digit, c);
    } else {
      Serial.printf("[DISPLAY] Digit %d: %c\n", digit, c);
    }
  }

  void clear() {
    if (checkDisplay()) {
      hw.display().clear();
    } else {
      Serial.println(F("[DISPLAY] Clear"));
    }
  }

  void setBrightness(uint8_t b) {
    if (checkDisplay()) {
      hw.display().setBrightness(b);
    } else {
      Serial.printf("[DISPLAY] Brightness: %d (display not detected)\n", b);
    }
  }
};
DisplayWrapper display;

// RFID tag in binary format (UID stored as bytes)
struct RfidTagBin {
  uint8_t bytes[7];
  uint8_t len;
  bool valid;
};

// Configuration (structure identical to the original)
struct Config {
  int countdownSeconds = 600;
  int defuseMode = DEFUSE_WIRE_CODE;
  int defuseCode = 1234;
  int defuseOrder = 1234;
  int detWire = DET_WIRE_RAND;
  int defuseWire = DEFUSE_WIRE_RAND;
  int speedupWire = 0;
  int pauseWire = 0;
  int brightness = 15;
  int volume = 128;
  int detTriggerSec = 5;
  int defuseTriggerSec = 5;
  int rs485Enabled = 0;
  int netPuzzlesCount = 4;
  int accelMode = ACCEL_OFF;
  int tickEnabled = 1;

  // RFID
  int rfidEnabled = RFID_OFF;
  RfidTagBin defuseTags[4] = {};
  RfidTagBin detonateTags[4] = {};
  int defuseTagCount = 0;
  int detonateTagCount = 0;
} config;

// Timer state
bool countdownRunning = false;
bool doubleSpeed = false;
bool updateDisplayFlag = false;
int countdownSeconds = 600;
bool isDefused = false;

// Accelerometer pause control
bool accelPauseActive = false;
unsigned long accelPauseEndTime = 0;
bool accelPauseSoundPlayed = false;

// Wire configuration (random or fixed)
int activeDetWire = 0;
int activeDefuseWire = 0;
int activeSpeedupWire = 0;
int activePauseWire = 0;

// Wire state
bool wireCutState[4] = {false, false, false, false};
bool pauseDone = false;

// ORDER mode
int wireOrderPos = 0;
int nextWire = 0;
int sequence[4];

// Keypad state
int digits[4];
int currentDigit = 0;
bool enteringCode = false;
int currentCode = 1; // Current attempt
int maxCode = 3; // Maximum attempts

// Network
bool netPuzzlesSolved[32] = {false};

// Timing
unsigned long lastTickAt = 0;
unsigned long lastSecondAt = 0;
unsigned long relayReleaseAt = 0;

// NeoPixel - Check Adafruit Led Animation
int neoEffect = NEOFX_NORMAL;
unsigned long neoLastUpdate = 0;
int neoStep = 0;

// Simultaneous effect control (explosion/defuse)
enum class EffectState { IDLE, RUNNING, FINISHED };
EffectState explosionEffect = EffectState::IDLE;
EffectState defuseEffect = EffectState::IDLE;
unsigned long effectStartTime = 0;
#define EFFECT_SERVO_ANGLE_DETONATE  180
#define EFFECT_SERVO_ANGLE_DEFUSE    0
#define EFFECT_SERVO_RETURN_DELAY_MS 500

// Main menu
int menuItem = 0;
bool inMenu = false;
bool inAdjustMode = false;  // true = adjusting value, false = navigating between items
unsigned long lastBtnCheck = 0;
unsigned long selectPressStart = 0;
bool selectWasPressed = false;
#define SELECT_LONG_PRESS_MS 800
#define MENU_ITEMS_COUNT 9

// WIRE submenu (5 items: 0=ORDR, 1=DET, 2=DEF, 3=SPD, 4=PAUS)
bool inWireSubmenu = false;
int wireSubItem = 0;
#define WIRE_SUBITEMS_COUNT 5

// RELE submenu (2 items: 0=TDET, 1=TDEF)
bool inReleSubmenu = false;
int releSubItem = 0;
#define RELE_SUBITEMS_COUNT 2

// RFID submenu (2 items: 0=DEFU, 1=DETO)
bool inRfidSubmenu = false;
int rfidSubItem = 0;
#define RFID_SUBITEMS_COUNT 2
unsigned long rfidFeedbackUntil = 0;
unsigned long lastRfidDisplayUpdate = 0;
char rfidFeedbackMsg[5] = "";  // "GOOD", "dup ", "FULL", etc

// Keypad via IoAbstraction KeyboardManager
// Configuration: 4x3 matrix using the PCF8575 (address configured in setup)
static const char keymap[] PROGMEM = "123456789*0#";
static KeyboardLayout keypadLayout(4, 3, keymap);
// PCF pins connected to the keypad (L1-L4, C1-C3)
// Use the KeypadTester.ino example to identify the correct pinout for your hardware
static byte rowPins[4] = {3, 2, 1, 0};   // P3, P2, P1, P0 on the PCF8575 (Rows)
static byte colPins[3] = {6, 5, 4};      // P6, P5, P4 on the PCF8575 (Columns)

class KeypadListener : public KeyboardListener {
public:
  char lastKey = 0;
  void keyPressed(char key, bool held) override { if (!held) lastKey = key; }
  void keyReleased(char key) override { lastKey = 0; }
};
static KeypadListener keypadListener;
static MatrixKeyboardManager keypadManager;

// Button and wire state variables (defined before the class)
extern volatile bool btnPressed[4];
extern volatile bool wireCutFlag[4];

// Forward declarations
void onButtonPressed(pinid_t pin, bool held);
void onWireCut(pinid_t pin, bool held);

// Puzzle wrapper
class GameTimerPuzzle : public PuzzleBase {
public:
  GameTimerPuzzle() : PuzzleBase(1) {}

  void setupHardware() override {
    Serial.println(F("[GameTimer] Setup started"));

    // Loads configuration
    loadConfig();
    Serial.printf("[GameTimer] Config: %ds, mode=%d, code=%04d\n",
                  config.countdownSeconds, config.defuseMode, config.defuseCode);

    // Applies settings
    display.setBrightness(config.brightness);
    hw.sound().setVolume(config.volume);

    // Initializes SwitchInput for buttons and wires via PCF8575
    // Note: IoAbstraction (via hw.io()) manages the PCF8574/8575 automatically
    if (hw.hasExpander()) {
      Serial.println(F("[GameTimer] Initializing SwitchInput via IoAbstraction..."));
      switches.init(hw.io(), SWITCHES_POLL_KEYS_ONLY, true);  // true = pull up

      // Adds buttons
      switches.addSwitch(BUTTON_LEFT_PIN, onButtonPressed, NO_REPEAT, false);
      switches.addSwitch(BUTTON_SELECT_PIN, onButtonPressed, NO_REPEAT, false);
      switches.addSwitch(BUTTON_RIGHT_PIN, onButtonPressed, NO_REPEAT, false);
      switches.addSwitch(BUTTON_DET_PIN, onButtonPressed, NO_REPEAT, false);

      // Adds wires (with cut callback)
      // IMPORTANT: invertLogic=true because we want to detect HIGH as "cut"
      // Connected = LOW (tied to GND), Cut = HIGH (pullup)
      switches.addSwitch(WIRE1_PIN, onWireCut, NO_REPEAT, true);  // true = inverted
      switches.addSwitch(WIRE2_PIN, onWireCut, NO_REPEAT, true);
      switches.addSwitch(WIRE3_PIN, onWireCut, NO_REPEAT, true);
      switches.addSwitch(WIRE4_PIN, onWireCut, NO_REPEAT, true);

      Serial.println(F("[GameTimer] SwitchInput OK: 4 buttons + 4 wires"));
    } else {
      Serial.println(F("[GameTimer] ERROR: PCF8575 not detected!"));
    }

    // Initializes keypad
    for (int i = 0; i < 4; i++) keypadLayout.setRowPin(i, rowPins[i]);
    for (int i = 0; i < 3; i++) keypadLayout.setColPin(i, colPins[i]);
    keypadManager.initialise(hw.io(), &keypadLayout, &keypadListener, false);

    // Initializes NFC Reader if hardware is detected
    if (hw.hasNfc()) {
      nfc_ = hw.createNfcReader();
      if (nfc_) {
        Serial.println(F("[RFID] PN532 reader ready"));
      } else {
        Serial.println(F("[RFID] PN532 init failed"));
      }
    }
    if (!nfc_) {
      Serial.println(F("[RFID] No NFC hardware"));
    }

    // Configures outputs
    pinMode(PUZZLE_PIN_RELAY, OUTPUT);
    digitalWrite(PUZZLE_PIN_RELAY, HIGH);
    pinMode(PIN_EXTERNAL_POWER, OUTPUT);
    digitalWrite(PIN_EXTERNAL_POWER, HIGH);

    // Initializes servo via PropMaker library (neutral position)
    hw.setActuator(90);

    // Initial State
    countdownSeconds = config.countdownSeconds;
    if (hw.hasDisplay()) {
      display.printTime(countdownSeconds / 60, countdownSeconds % 60);
    }
    setNeoEffect(NEOFX_NORMAL);

    // Startup sound
    hw.sound().playOnceBlocking("/armed.mp3");
    Serial.println(F("[GameTimer] Self Destruction Initiated"));

    setState(PuzzleState::Idle);
  }

  void update() override {
    // Keeps audio active
    hw.sound().loop();

    // Processes keypad and switch input (buttons/wires)
    taskManager.runLoop();
    switches.runLoop();

    // Updates NeoPixel
    updateNeoEffect();

    // State machine
    switch (getState()) {
      case PuzzleState::Idle:
        if (!countdownRunning) getTimerInput();
        else startCountdown();
        break;
      case PuzzleState::Running:
        countdown();
        break;
      case PuzzleState::Solved:
        // Waits for restart
        if (buttonPressed(BUTTON_DET_PIN)) {
          while (buttonPressed(BUTTON_DET_PIN)) delay(10);
          resetSystem();
        }
        break;
      case PuzzleState::Inactive:
        // Waits for restart
        if (buttonPressed(BUTTON_DET_PIN)) {
          while (buttonPressed(BUTTON_DET_PIN)) delay(10);
          resetSystem();
        }
        break;
      default:
        break;
    }
  }

  void onNetworkPacket(const Rs485Payload& p) override {
    if (!config.rs485Enabled) return;

    switch (p.pCommand) {
      case NetCommand::SyncTimer:
        if (getState() == PuzzleState::Idle) {
          countdownSeconds = p.data;
          checkDetButton(0x10);  // CMD_START
        }
        break;
      case NetCommand::ForceSolve:
        if (getState() == PuzzleState::Running) {
          isDefused = true;
        }
        break;
      case NetCommand::StatusReport:
        if (config.defuseMode == 3 && p.pState == PuzzleState::Solved) {  // BY_NET
          if (p.sourceId > 0 && p.sourceId < 32) {
            netPuzzlesSolved[p.sourceId] = true;
            int count = 0;
            for (int i = 0; i < 32; i++) if (netPuzzlesSolved[i]) count++;
            if (count >= config.netPuzzlesCount) isDefused = true;
          }
        }
        break;
      default:
        break;
    }
  }

private:
  // =======================================================================
  // GAME TIMER PRO FUNCTIONS (original names)
  // =======================================================================

  // RFID state
  NfcReader* nfc_ = nullptr;
  unsigned long lastRfidPoll_ = 0;
  uint8_t lastRfidUid_[7] = {0};
  uint8_t lastRfidUidLen_ = 0;

  void getTimerInput() {
    static unsigned long lastAdjCheck = 0;
    static unsigned long selectEnterStart = 0;
    static bool selectEnterPressed = false;

    // Menu navigation control
    if (inMenu) {
      configure();
      return;
    }

    // Detects SELECT start for menu entry (long press)
    if (buttonPressed(BUTTON_SELECT_PIN) && !selectEnterPressed) {
      selectEnterPressed = true;
      selectEnterStart = millis();
      return;
    }

    // Checks if SELECT was released (outside the menu)
    if (!switches.isSwitchPressed(BUTTON_SELECT_PIN) && selectEnterPressed) {
      selectEnterPressed = false;
      unsigned long pressDuration = millis() - selectEnterStart;

      // SHORT SELECT (<800ms): enters the menu
      if (pressDuration < SELECT_LONG_PRESS_MS) {
        Serial.println(F("[INPUT] SHORT SELECT -> entering menu"));
        inMenu = true;
        menuItem = 0;
        inAdjustMode = false;
        showMenuItem();
      } else {
        // LONG SELECT (>=800ms): saves config (including adjusted time)
        Serial.println(F("[INPUT] LONG SELECT -> saving configuration"));
        saveConfig();
        display.printString("-OK-");
        hw.sound().playOnce("/keypress.mp3");
        Serial.printf("[INPUT] Time saved: %d seconds\n", config.countdownSeconds);
        delay(800);  // Time to visualize -OK-
      }
      return;
    }

    // Debounce for other buttons
    if (millis() - lastBtnCheck < 150) return;

    // DET button starts the countdown
    if (buttonPressed(BUTTON_DET_PIN)) {
      lastBtnCheck = millis();
      Serial.println(F("[INPUT] DET -> starting countdown"));
      checkDetButton(0);
      return;
    }

    // Time adjustment with short/long detection
    handleTimeAdjustment();

    // Checks accelerometer START mode (starts countdown when movement is detected)
    if (config.accelMode == ACCEL_START && hw.hasAccelerometer()) {
      if (hw.isShaking(12.0f)) {
        Serial.println(F("[ACCEL] SHAKE DETECTED in Idle -> starting countdown"));
        checkDetButton(0);
      }
    }
  }

  // Variables for time adjustment control
  unsigned long leftPressStart = 0;
  unsigned long rightPressStart = 0;
  bool leftWasPressed = false;
  bool rightWasPressed = false;
  unsigned long lastAdjTime = 0;
  bool timeChanged = false;
  #define TIME_ADJUST_INTERVAL_MS 150
  #define TIME_LONG_PRESS_MS 600
  #define TIME_AUTO_SAVE_MS 2000

  void handleTimeAdjustment() {
    unsigned long now = millis();

    // Checks LEFT
    if (buttonPressed(BUTTON_LEFT_PIN) && !leftWasPressed) {
      leftWasPressed = true;
      leftPressStart = now;
      // First action: decrement 1
      if (countdownSeconds > 10) {
        countdownSeconds--;
        config.countdownSeconds = countdownSeconds;
        updateDisplay();
        Serial.printf("[TIME] LEFT: -1s = %d\n", countdownSeconds);
        timeChanged = true;
      }
      lastAdjTime = now;
    }

    // Checks RIGHT
    if (buttonPressed(BUTTON_RIGHT_PIN) && !rightWasPressed) {
      rightWasPressed = true;
      rightPressStart = now;
      // First action: increment 1
      if (countdownSeconds < 5999) {
        countdownSeconds++;
        config.countdownSeconds = countdownSeconds;
        updateDisplay();
        Serial.printf("[TIME] RIGHT: +1s = %d\n", countdownSeconds);
        timeChanged = true;
      }
      lastAdjTime = now;
    }

    // Checks if LEFT is being held (long press)
    if (leftWasPressed && switches.isSwitchPressed(BUTTON_LEFT_PIN)) {
      unsigned long heldTime = now - leftPressStart;
      if (heldTime >= TIME_LONG_PRESS_MS && now - lastAdjTime >= TIME_ADJUST_INTERVAL_MS) {
        // Long press active: decrement 5
        if (countdownSeconds > 15) {
          countdownSeconds -= 5;
          config.countdownSeconds = countdownSeconds;
          updateDisplay();
          Serial.printf("[TIME] LEFT (hold): -5s = %d\n", countdownSeconds);
          timeChanged = true;
        } else if (countdownSeconds > 10) {
          countdownSeconds = 10;
          config.countdownSeconds = countdownSeconds;
          updateDisplay();
          Serial.printf("[TIME] LEFT (hold): min = %d\n", countdownSeconds);
          timeChanged = true;
        }
        lastAdjTime = now;
      }
    }

    // Checks if RIGHT is being held (long press)
    if (rightWasPressed && switches.isSwitchPressed(BUTTON_RIGHT_PIN)) {
      unsigned long heldTime = now - rightPressStart;
      if (heldTime >= TIME_LONG_PRESS_MS && now - lastAdjTime >= TIME_ADJUST_INTERVAL_MS) {
        // Long press active: increment 5
        if (countdownSeconds < 5994) {
          countdownSeconds += 5;
          config.countdownSeconds = countdownSeconds;
          updateDisplay();
          Serial.printf("[TIME] RIGHT (hold): +5s = %d\n", countdownSeconds);
          timeChanged = true;
        } else if (countdownSeconds < 5999) {
          countdownSeconds = 5999;
          config.countdownSeconds = countdownSeconds;
          updateDisplay();
          Serial.printf("[TIME] RIGHT (hold): max = %d\n", countdownSeconds);
          timeChanged = true;
        }
        lastAdjTime = now;
      }
    }

    // Resets flags when buttons are released
    if (!switches.isSwitchPressed(BUTTON_LEFT_PIN) && leftWasPressed) {
      leftWasPressed = false;
    }
    if (!switches.isSwitchPressed(BUTTON_RIGHT_PIN) && rightWasPressed) {
      rightWasPressed = false;
    }

    // Auto-save: saves after 2 seconds without adjustments
    if (timeChanged && (now - lastAdjTime >= TIME_AUTO_SAVE_MS)) {
      saveConfig();
      timeChanged = false;
      Serial.printf("[TIME] Auto-saved after %dms: %d seconds\n", TIME_AUTO_SAVE_MS, config.countdownSeconds);
    }
  }

  void checkDetButton(int trigger) {
    if (trigger == 0x10 || countdownRunning) return;  // CMD_START or already running

    Serial.println(F("[GameTimer] Starting countdown"));
    countdownRunning = true;

    // Resets accelerometer pause state
    accelPauseActive = false;
    accelPauseEndTime = 0;
    accelPauseSoundPlayed = false;

    // Flash on NeoPixel
    for (int i = 0; i < 3; i++) {
      hw.setLightAll(neoColor(255, 0, 0));
      delay(100);
      hw.setLightAll(0);
      delay(100);
    }

    setState(PuzzleState::Running);
  }

  void countdown() {
    if (!countdownRunning) return;  // Silent - not an error, just not started yet

    // Checks if accelerometer pause ended
    if (accelPauseActive && millis() >= accelPauseEndTime) {
      accelPauseActive = false;
      accelPauseSoundPlayed = false;
      Serial.println(F("[ACCEL] Pause finished"));
      // Adjusts lastSecondAt so the 2s of pause are "lost"
      // Ex: paused at 00:58, after 2s continues at 00:56
      lastSecondAt = millis();
    }

    // Timing
    unsigned long interval = doubleSpeed ? 500 : 1000;
    if (!accelPauseActive && millis() - lastSecondAt >= interval) {
      lastSecondAt = millis();
      if (countdownSeconds > 0) {
        countdownSeconds--;
        if (config.tickEnabled) tick();
        updateDisplayFlag = true;

        // Updates NeoPixel
        if (doubleSpeed) setNeoEffect(NEOFX_DOUBLE_SPEED);
        else setNeoEffect(NEOFX_COUNTING);
      }
    }

    // Updates display (wrapper checks automatically)
    if (updateDisplayFlag && !enteringCode) {
      updateDisplay();
      updateDisplayFlag = false;
    }

    // Checks accelerometer
    checkAccelerometer();

    // Checks wires
    if (config.defuseMode != DEFUSE_WIRE_ORDER) {
      checkWiresNormal();
    } else {
      checkWiresOrder();
    }

    // Checks keypad (CODE mode)
    if (config.defuseMode == DEFUSE_WIRE_CODE) {
      checkKeypadCode();
    }

    // Checks RFID (parallel mechanism, independent of defuseMode)
    checkRfidDuringCountdown();

    // Checks end
    if (countdownSeconds == 0) {
      countdownRunning = false;
      detonate();
    } else if (isDefused) {
      countdownRunning = false;
      defused();
    }
  }

  void checkAccelerometer() {
    if (config.accelMode == ACCEL_OFF) return;
    if (!hw.hasAccelerometer()) {
      static bool accelWarned = false;
      if (!accelWarned) {
        Serial.println(F("[ACCEL] Warning: Accelerometer not detected"));
        accelWarned = true;
      }
      return;
    }

    // Diagnostics: shows reading every 2 seconds when in debug mode
    static unsigned long lastAccelDiag = 0;
    bool showDiag = (millis() - lastAccelDiag > 2000);

    switch (config.accelMode) {
      case ACCEL_START:
        // START mode only works in Idle to start the countdown
        // In Running it has no effect
        break;
      case ACCEL_SPEED2X:
        if (hw.isShaking(12.0f)) {
          Serial.println(F("[ACCEL] SHAKE DETECTED -> doubleSpeed=true"));
          doubleSpeed = true;
        }
        break;
      case ACCEL_PAUSE:
        if (hw.isShaking(10.0f) && !accelPauseActive) {
          Serial.println(F("[ACCEL] SHAKE DETECTED -> pause 2 seconds"));
          accelPauseActive = true;
          accelPauseEndTime = millis() + 2000;
          accelPauseSoundPlayed = false;
        }
        // Plays error sound at the start of the pause (only once)
        if (accelPauseActive && !accelPauseSoundPlayed) {
          if (LittleFS.exists("/error.mp3")) {
            hw.sound().playOnce("/error.mp3");
          }
          accelPauseSoundPlayed = true;
        }
        break;
      case ACCEL_DETONATE:
        if (hw.isShaking(18.0f)) {
          Serial.println(F("[ACCEL] SHAKE DETECTED -> DETONATE!"));
          countdownSeconds = 0;
        }
        break;
    }
  }

  void checkWiresNormal() {
    static bool lastDetWireState = false;

    // Detonator wire
    if (activeDetWire >= WIRE1) {
      bool cut = wireCut(activeDetWire);
      if (cut && !lastDetWireState) {
        Serial.printf("[checkWiresNormal] DETONATOR wire cut! Exploding...\n");
        countdownSeconds = 0;
      }
      lastDetWireState = cut;
      if (cut) return;
    }

    // Defuse wire (specific wire mode)
    if (config.defuseMode == 1 && activeDefuseWire >= WIRE1) {  // BY_WIRE
      if (wireCut(activeDefuseWire)) {
        isDefused = true;
        return;
      }
    }

    // Pause wire
    if (activePauseWire >= WIRE1 && !pauseDone) {
      if (wireCut(activePauseWire)) {
        pauseCountdown();
      }
    }

    // Speedup wire
    if (activeSpeedupWire >= WIRE1) {
      if (wireCut(activeSpeedupWire)) {
        doubleSpeed = true;
      }
    }

    // All wires cut = detonates
    bool allCut = true;
    for (int i = 0; i < 4; i++) {
      if (!wireCut(WIRE1 + i)) {
        allCut = false;
        break;
      }
    }
    if (allCut) countdownSeconds = 0;
  }

  void pauseCountdown() {
    pauseDone = true;
    hw.sound().playOnce("/keypress.mp3");
    setNeoEffect(NEOFX_OFF);
    delay(2000);
    setNeoEffect(NEOFX_COUNTING);
  }

  void checkWiresOrder() {
    for (int w = 0; w < 4; w++) {
      if (countdownSeconds == 0) break;
      if (!wireCutState[w] && wireCut(WIRE1 + w)) {
        wireCutState[w] = true;

        if ((w + 1) != nextWire) {
          countdownSeconds = 0;
          break;
        } else {
          wireOrderPos++;
          if (wireOrderPos >= 4) {
            isDefused = true;
            break;
          }
          nextWire = sequence[wireOrderPos];
        }
      }
    }
  }

  void checkKeypadCode() {
    char key = keypadListener.lastKey;
    if (key == 0) return;

    // Clears the key immediately so the next read is not missed
    keypadListener.lastKey = 0;

    Serial.printf("[checkKeypadCode] Key pressed: %c\n", key);

    // Starts code entry
    if (!enteringCode && key >= '0' && key <= '9') {
      Serial.println(F("[checkKeypadCode] Starting code entry"));
      enteringCode = true;
      currentDigit = 0;
      memset(digits, 0, sizeof(digits));
      display.clear();
      display.setDisplayColon(false);
      display.printString("____");
      return;  // Sound already played above
    }

    // Digit entry
    if (enteringCode && key >= '0' && key <= '9' && currentDigit < 4) {
      digits[currentDigit] = key - '0';
      display.writeDigitAscii(currentDigit, key);
	      // Plays key sound (non-blocking) - processed before any logic
    hw.sound().playOnce("/keypress.mp3");
      currentDigit++;

      // Checks if code is complete
      if (currentDigit >= 4) {
        int entered = digits[0] * 1000 + digits[1] * 100 + digits[2] * 10 + digits[3];
        enteringCode = false;

        if (entered == config.defuseCode) {
          Serial.printf("[checkKeypadCode] Correct code! Defusing...\n");
          isDefused = true;
        } else {
          currentCode++;
          // Gives the user more attempts before exploding
          if (currentCode <= maxCode) {
            display.clear();
            display.printString("_Err");
            enteringCode = true;
            currentDigit = 0; // First digit from left to right
            digits[0] = digits[0];  // Keeps first digit for visual reference
          } else {
            Serial.printf("[checkKeypadCode] Wrong code! Exploding...\n");
            countdownSeconds = 0; // wrong code bomb explodes
          }
        }
      }
    }

    // Cancels entry
    if ((key == '*' || key == '#') && enteringCode) {
      enteringCode = false;
      currentDigit = 0;
      updateDisplayFlag = true;
	      // Plays key sound (non-blocking) - processed before any logic
    hw.sound().playOnce("/keypress.mp3");
    }
  }

  // Maps the WIRE1-4 constant to a physical pin
  int wireToPin(int wire) {
    switch(wire) {
      case WIRE1: return WIRE1_PIN;
      case WIRE2: return WIRE2_PIN;
      case WIRE3: return WIRE3_PIN;
      case WIRE4: return WIRE4_PIN;
      default: return -1;
    }
  }

  bool wireCut(int wire) {
    if (wire < WIRE1 || wire > WIRE4) return false;
    int pin = wireToPin(wire);
    // Uses SwitchInput to check state (HIGH = wire cut)
    return switches.isSwitchPressed(pin);
  }

  void detonate() {
    Serial.println(F("\n[detonate] ========== BOOM! =========="));
    Serial.printf("[detonate] Time expired or forced detonation\n");
    setState(PuzzleState::Inactive);

    // Starts simultaneous effects immediately
    setNeoEffect(NEOFX_EXPLOSION);
    display.printString("boom");

    // Explosion sound (non-blocking) - starts along with the other effects
    hw.sound().playOnce("/boom.mp3");

    // Immediate physical trigger - relay and servo activated along with the sound
    digitalWrite(PUZZLE_PIN_RELAY, LOW);
    hw.setActuator(EFFECT_SERVO_ANGLE_DETONATE);  // Servo via library

    // Records effect start for relay timing control
    effectStartTime = millis();
    relayReleaseAt = effectStartTime + (config.detTriggerSec * 1000UL);
    explosionEffect = EffectState::RUNNING;

    // Simultaneous animation loop: keeps audio, NeoPixel and checks button
    while (millis() < relayReleaseAt) {
      hw.sound().loop();        // Keeps audio active (fundamental)
      switches.runLoop();       // Processes inputs
      updateNeoEffect();        // Updates NeoPixel

      // Checks restart button
      if (buttonPressed(BUTTON_DET_PIN)) {
        while (buttonPressed(BUTTON_DET_PIN)) { taskManager.runLoop(); switches.runLoop(); delay(10); }
        break;
      }
      delay(10);
    }

    // Deactivates physical trigger
    digitalWrite(PUZZLE_PIN_RELAY, HIGH);

    // Smoothly returns servo after delay
    delay(EFFECT_SERVO_RETURN_DELAY_MS);
    hw.setActuator(90);  // Neutral position

    explosionEffect = EffectState::FINISHED;
  }

  void defused() {
    Serial.println(F("\n[defused] ========== BOMB DEFUSED =========="));
    Serial.printf("[defused] Correct code entered\n");
    setState(PuzzleState::Solved);

    // Starts simultaneous effects immediately
    setNeoEffect(NEOFX_DEFUSED);

    // Display shows code
    display.printString("SAFE");

    // Success sound (non-blocking) - starts along with the other effects
    hw.sound().playOnce("/sucess.mp3");

    // Immediate physical trigger - relay activated along with the sound
    // Servo stays in neutral position (0) for defuse
    digitalWrite(PUZZLE_PIN_RELAY, LOW);
    hw.setActuator(EFFECT_SERVO_ANGLE_DEFUSE);  // Servo via library

    // Records effect start for relay timing control
    effectStartTime = millis();
    relayReleaseAt = effectStartTime + (config.defuseTriggerSec * 1000UL);
    defuseEffect = EffectState::RUNNING;

    // Simultaneous animation loop: keeps audio, NeoPixel and checks button
    while (millis() < relayReleaseAt) {
      hw.sound().loop();        // Keeps audio active (fundamental)
      switches.runLoop();       // Processes inputs
      updateNeoEffect();        // Updates NeoPixel

      // Checks restart button
      if (buttonPressed(BUTTON_DET_PIN)) {
        while (buttonPressed(BUTTON_DET_PIN)) { taskManager.runLoop(); switches.runLoop(); delay(10); }
        break;
      }
      delay(10);
    }

    // Deactivates physical trigger
    digitalWrite(PUZZLE_PIN_RELAY, HIGH);

    // Smoothly returns servo after delay
    delay(EFFECT_SERVO_RETURN_DELAY_MS);
    hw.setActuator(90);  // Neutral position

    defuseEffect = EffectState::FINISHED;
  }

  void resetSystem() {
    Serial.println(F("[GameTimer] Reset"));

    countdownRunning = false;
    doubleSpeed = false;
    isDefused = false;
    enteringCode = false;
	currentCode = 1; // resets attempts
    inMenu = false;
    inAdjustMode = false;
    inWireSubmenu = false;
    inReleSubmenu = false;
    inRfidSubmenu = false;
    wireSubItem = 0;
    releSubItem = 0;
    rfidSubItem = 0;
    selectWasPressed = false;
    selectPressStart = 0;
    accelPauseActive = false;
    accelPauseEndTime = 0;
    accelPauseSoundPlayed = false;
    memset(wireCutState, 0, sizeof(wireCutState));
    memset(netPuzzlesSolved, 0, sizeof(netPuzzlesSolved));
    lastRfidUidLen_ = 0;

    countdownSeconds = config.countdownSeconds;

    setNeoEffect(NEOFX_NORMAL);
    display.setDisplayColon(false);
    updateDisplay();

    setState(PuzzleState::Idle);
  }

  void startCountdown() {
    Serial.println(F("\n[startCountdown] ========== STARTING COUNTDOWN =========="));
    Serial.printf("[startCountdown] Configured time: %d seconds\n", config.countdownSeconds);
    Serial.printf("[startCountdown] Defuse mode: %d (0=CODE, 1=WIRE, 2=ORDER, 3=NET)\n", config.defuseMode);

    // Draws wires
    Serial.println(F("[startCountdown] Drawing wires..."));
    getRandomWire();

    // Resets state
    lastRfidPoll_ = 0;
    lastRfidUidLen_ = 0;
    countdownSeconds = config.countdownSeconds;
    isDefused = false;
    doubleSpeed = false;
    pauseDone = false;
    enteringCode = false;
    currentDigit = 0;
	currentCode = 1; // resets attempts
    memset(wireCutState, 0, sizeof(wireCutState));
    memset(netPuzzlesSolved, 0, sizeof(netPuzzlesSolved));

    // Configures ORDER mode
    if (config.defuseMode == DEFUSE_WIRE_ORDER) {
      sequence[0] = config.defuseOrder / 1000;
      sequence[1] = (config.defuseOrder % 1000) / 100;
      sequence[2] = (config.defuseOrder % 100) / 10;
      sequence[3] = config.defuseOrder % 10;
      wireOrderPos = 0;
      nextWire = sequence[0];
    }

    // Arming sound
    hw.sound().playOnceBlocking("/armed.mp3");

    // Visual effect
    setNeoEffect(NEOFX_COUNTING);
    display.setDisplayColon(true);

    lastSecondAt = millis();
    updateDisplayFlag = true;
  }

  void getRandomWire() {
    activeDetWire = (config.detWire == DET_WIRE_RAND) ? random(WIRE1, WIRE4 + 1) : config.detWire;

    if (config.defuseMode == 1) {  // BY_WIRE
      do {
        activeDefuseWire = (config.defuseWire == DEFUSE_WIRE_RAND) ? random(WIRE1, WIRE4 + 1) : config.defuseWire;
      } while (activeDefuseWire == activeDetWire);
    }

    if (config.speedupWire > 0) {
      do {
        activeSpeedupWire = (config.speedupWire == 1) ? random(WIRE1, WIRE4 + 1) : config.speedupWire;
      } while (activeSpeedupWire == activeDetWire || activeSpeedupWire == activeDefuseWire);
    }

    if (config.pauseWire > 0) {
      do {
        activePauseWire = (config.pauseWire == 1) ? random(WIRE1, WIRE4 + 1) : config.pauseWire;
      } while (activePauseWire == activeDetWire || activePauseWire == activeDefuseWire ||
               activePauseWire == activeSpeedupWire);
    }

    Serial.printf("[WIRES] Det=%d Def=%d Spd=%d Paus=%d\n",
                activeDetWire, activeDefuseWire, activeSpeedupWire, activePauseWire);
  }

  bool wireConflict(int key, int value) {
    // Checks if the value is already in use by another configuration
    if (value < WIRE1 || value > WIRE4) return false;

    if (key != 2 && activeDetWire == value) return true;      // DET_WIRE
    if (key != 3 && activeDefuseWire == value) return true;   // DEFUSE_WIRE
    if (key != 4 && activeSpeedupWire == value) return true;  // SPEEDUP_WIRE
    if (key != 5 && activePauseWire == value) return true;    // PAUSE_WIRE

    return false;
  }

  // =======================================================================
  // CONFIGURATION MENU
  // =======================================================================

  void configure() {
    // Detects SELECT press start for the long press timer
    if (buttonPressed(BUTTON_SELECT_PIN) && !selectWasPressed) {
      selectWasPressed = true;
      selectPressStart = millis();
      return;
    }

    // Verifica se SELECT foi solto
    if (!switches.isSwitchPressed(BUTTON_SELECT_PIN) && selectWasPressed) {
      selectWasPressed = false;
      unsigned long pressDuration = millis() - selectPressStart;

      // LONG SELECT (>=800ms): saves and goes back one level
      if (pressDuration >= SELECT_LONG_PRESS_MS) {
        Serial.println(F("[CONFIG] LONG SELECT -> saving and going back a level"));
        handleSelectLong();
      } else {
        // SHORT SELECT: navigates/enters/exits according to context
        handleSelectShort();
      }
      lastBtnCheck = millis();
      return;
    }

    // Waits for debounce for other buttons
    if (millis() - lastBtnCheck < 150) return;

    // DET: exits the menu and starts the countdown
    if (buttonPressed(BUTTON_DET_PIN)) {
      lastBtnCheck = millis();
      Serial.println(F("[CONFIG] DET -> exiting menu and starting countdown"));
      exitAllMenus();
      checkDetButton(0);
      return;
    }

    // ===== NAVIGATION AND ADJUSTMENT =====
    if (inAdjustMode) {
      handleAdjustMode();
    } else if (inWireSubmenu) {
      handleWireSubmenuNavigation();
    } else if (inReleSubmenu) {
      handleReleSubmenuNavigation();
    } else if (inRfidSubmenu) {
      handleRfidSubmenuNavigation();
      // Non-blocking RFID poll if NFC is present
      if (nfc_) {
        pollRfidTag();
      }
      // Updates display: feedback or mode name (with throttle)
      if (millis() - lastRfidDisplayUpdate >= 200) {
        if (rfidFeedbackUntil != 0 && millis() < rfidFeedbackUntil && rfidFeedbackMsg[0]) {
          display.clear();
          display.setDisplayColon(false);
          display.printString(rfidFeedbackMsg);
        } else {
          showRfidSubmenuItem(rfidSubItem);
          rfidFeedbackUntil = 0;
          rfidFeedbackMsg[0] = '\0';
        }
        lastRfidDisplayUpdate = millis();
      }
    } else {
      handleMainMenuNavigation();
    }
  }

  void exitAllMenus() {
    inMenu = false;
    inAdjustMode = false;
    inWireSubmenu = false;
    inReleSubmenu = false;
    inRfidSubmenu = false;
    Serial.println(F("[CONFIG] Menu closed"));
  }

  void handleSelectShort() {
    if (inAdjustMode) {
      // RFID ON: opens submenu instead of exiting adjustment
      if (menuItem == 8 && config.rfidEnabled == RFID_ON) {
        inAdjustMode = false;
        Serial.println(F("[CONFIG] SHORT SELECT -> entering RFID submenu"));
        inRfidSubmenu = true;
        rfidSubItem = 0;
        showRfidSubmenuItem(rfidSubItem);
      } else {
        // In adjust mode: exits adjustment, returns to current level navigation
        inAdjustMode = false;
        Serial.println(F("[CONFIG] SHORT SELECT -> exiting adjustment"));
        if (inWireSubmenu) {
          showWireSubmenuItem(wireSubItem);
        } else if (inReleSubmenu) {
          showReleSubmenuItem(releSubItem);
        } else if (inRfidSubmenu) {
          showRfidSubmenuItem(rfidSubItem);
        } else {
          showMenuItem();
        }
      }
    } else if (inWireSubmenu) {
      // In WIRE submenu: enters adjustment of the selected item
      if (wireSubItem == 0) {  // ORDR - uses special input
        Serial.println(F("[CONFIG] SHORT SELECT -> entering ORDR"));
        inputWireOrder();
        showWireSubmenuItem(wireSubItem);
      } else {
        Serial.printf("[CONFIG] SHORT SELECT -> entering WIRE adjustment [%d]\n", wireSubItem);
        inAdjustMode = true;
        showCurrentWireValue(wireSubItem);
      }
    } else if (inReleSubmenu) {
      // In the RELE submenu: enter time adjustment
      Serial.printf("[CONFIG] SHORT SELECT -> entering RELE adjustment [%d]\n", releSubItem);
      inAdjustMode = true;
      showCurrentReleValue(releSubItem);
    } else if (inRfidSubmenu) {
      // Short SELECT: exits RFID submenu, returns to main menu
      inRfidSubmenu = false;
      Serial.println(F("[RFID] Exiting RFID submenu"));
      hw.sound().playOnce("/keypress.mp3");
      showMenuItem();
    } else {
      // Main menu: WIRE (5) and RELE (7) open submenus
      if (menuItem == 5) {  // WIRE
        Serial.println(F("[CONFIG] SHORT SELECT -> entering WIRE submenu"));
        inWireSubmenu = true;
        wireSubItem = 0;
        showWireSubmenuItem(wireSubItem);
      } else if (menuItem == 7) {  // RELE
        Serial.println(F("[CONFIG] SHORT SELECT -> entering the RELE submenu"));
        inReleSubmenu = true;
        releSubItem = 0;
        showReleSubmenuItem(releSubItem);
      } else {
        // Other items: enter adjust mode
        Serial.printf("[CONFIG] SHORT SELECT -> entering adjustment item %d\n", menuItem);
        inAdjustMode = true;
        showCurrentValue();
      }
    }
  }

  void handleSelectLong() {
    // LONG SELECT: saves and goes back one level
    if (inAdjustMode) {
      // Exits adjustment, shows current level navigation
      inAdjustMode = false;
      saveConfig();
      hw.sound().playOnce("/keypress.mp3");
      Serial.println(F("[CONFIG] LONG SELECT -> saved, returning to menu"));
      if (inWireSubmenu) {
        showWireSubmenuItem(wireSubItem);
      } else if (inReleSubmenu) {
        showReleSubmenuItem(releSubItem);
      } else if (inRfidSubmenu) {
        showRfidSubmenuItem(rfidSubItem);
      } else {
        showMenuItem();
      }
    } else if (inWireSubmenu) {
      // Exits WIRE submenu, returns to main menu
      inWireSubmenu = false;
      saveConfig();
      hw.sound().playOnce("/keypress.mp3");
      Serial.println(F("[CONFIG] LONG SELECT -> saved, returning to main menu"));
      showMenuItem();
    } else if (inReleSubmenu) {
      // Exit from the RELE submenu, return to the main menu
      inReleSubmenu = false;
      saveConfig();
      hw.sound().playOnce("/keypress.mp3");
      Serial.println(F("[CONFIG] LONG SELECT -> saved, returning to main menu"));
      showMenuItem();
    } else if (inRfidSubmenu) {
      // Long SELECT: clears current mode tags + saves + exits
      {
        int& tagCount = (rfidSubItem == 0) ? config.defuseTagCount : config.detonateTagCount;
        RfidTagBin* tags = (rfidSubItem == 0) ? config.defuseTags : config.detonateTags;
        memset(tags, 0, sizeof(RfidTagBin) * 4);
        tagCount = 0;
      }
      Serial.printf("[RFID] Tags cleared for %s\n", rfidSubItem == 0 ? "DEFUSE" : "DETONATE");
      display.clear();
      display.setDisplayColon(false);
      display.printString("CLR ");
      hw.sound().playOnce("/keypress.mp3");
      delay(500);
      inRfidSubmenu = false;
      saveConfig();
      showMenuItem();
    } else {
      // Main menu: exits completely
      exitAllMenus();
      saveConfig();
      updateDisplay();
      hw.sound().playOnce("/keypress.mp3");
      Serial.println(F("[CONFIG] LONG SELECT -> saved, exiting menu"));
    }
  }

  void handleMainMenuNavigation() {
    if (buttonPressed(BUTTON_LEFT_PIN)) {
      lastBtnCheck = millis();
      menuItem = (menuItem == 0) ? (MENU_ITEMS_COUNT - 1) : menuItem - 1;
      Serial.printf("[CONFIG] LEFT -> menuItem=%d\n", menuItem);
      showMenuItem();
    }
    if (buttonPressed(BUTTON_RIGHT_PIN)) {
      lastBtnCheck = millis();
      menuItem = (menuItem >= MENU_ITEMS_COUNT - 1) ? 0 : menuItem + 1;
      Serial.printf("[CONFIG] RIGHT -> menuItem=%d\n", menuItem);
      showMenuItem();
    }
  }

  void handleWireSubmenuNavigation() {
    if (buttonPressed(BUTTON_LEFT_PIN)) {
      lastBtnCheck = millis();
      wireSubItem = (wireSubItem == 0) ? (WIRE_SUBITEMS_COUNT - 1) : wireSubItem - 1;
      Serial.printf("[CONFIG] LEFT -> wireSubItem=%d\n", wireSubItem);
      showWireSubmenuItem(wireSubItem);
    }
    if (buttonPressed(BUTTON_RIGHT_PIN)) {
      lastBtnCheck = millis();
      wireSubItem = (wireSubItem >= WIRE_SUBITEMS_COUNT - 1) ? 0 : wireSubItem + 1;
      Serial.printf("[CONFIG] RIGHT -> wireSubItem=%d\n", wireSubItem);
      showWireSubmenuItem(wireSubItem);
    }
  }

  void handleReleSubmenuNavigation() {
    if (buttonPressed(BUTTON_LEFT_PIN)) {
      lastBtnCheck = millis();
      releSubItem = (releSubItem == 0) ? (RELE_SUBITEMS_COUNT - 1) : releSubItem - 1;
      Serial.printf("[CONFIG] LEFT -> releSubItem=%d\n", releSubItem);
      showReleSubmenuItem(releSubItem);
    }
    if (buttonPressed(BUTTON_RIGHT_PIN)) {
      lastBtnCheck = millis();
      releSubItem = (releSubItem >= RELE_SUBITEMS_COUNT - 1) ? 0 : releSubItem + 1;
      Serial.printf("[CONFIG] RIGHT -> releSubItem=%d\n", releSubItem);
      showReleSubmenuItem(releSubItem);
    }
  }

  void handleRfidSubmenuNavigation() {
    if (buttonPressed(BUTTON_LEFT_PIN)) {
      lastBtnCheck = millis();
      rfidSubItem = (rfidSubItem == 0) ? (RFID_SUBITEMS_COUNT - 1) : rfidSubItem - 1;
      rfidFeedbackUntil = 0;
      rfidFeedbackMsg[0] = '\0';
      Serial.printf("[CONFIG] LEFT -> rfidSubItem=%d\n", rfidSubItem);
      showRfidSubmenuItem(rfidSubItem);
    }
    if (buttonPressed(BUTTON_RIGHT_PIN)) {
      lastBtnCheck = millis();
      rfidSubItem = (rfidSubItem >= RFID_SUBITEMS_COUNT - 1) ? 0 : rfidSubItem + 1;
      rfidFeedbackUntil = 0;
      rfidFeedbackMsg[0] = '\0';
      Serial.printf("[CONFIG] RIGHT -> rfidSubItem=%d\n", rfidSubItem);
      showRfidSubmenuItem(rfidSubItem);
    }
  }

  void handleAdjustMode() {
    if (inWireSubmenu) {
      // Adjustment inside the WIRE submenu
      if (buttonPressed(BUTTON_LEFT_PIN)) {
        lastBtnCheck = millis();
        adjustWireSubmenuItem(-1);
      }
      if (buttonPressed(BUTTON_RIGHT_PIN)) {
        lastBtnCheck = millis();
        adjustWireSubmenuItem(1);
      }
    } else if (inReleSubmenu) {
      // Adjustments within the RELE submenu
      if (buttonPressed(BUTTON_LEFT_PIN)) {
        lastBtnCheck = millis();
        adjustReleSubmenuItem(-1);
      }
      if (buttonPressed(BUTTON_RIGHT_PIN)) {
        lastBtnCheck = millis();
        adjustReleSubmenuItem(1);
      }
    } else {
      // Adjustment in the main menu
      if (buttonPressed(BUTTON_LEFT_PIN)) {
        lastBtnCheck = millis();
        adjustMenuItem(-1);
      }
      if (buttonPressed(BUTTON_RIGHT_PIN)) {
        lastBtnCheck = millis();
        adjustMenuItem(1);
      }
    }
  }

  // Main menu: 9 items
  // 0=BRIGHT, 1=VOLUME, 2=TICK, 3=SHAK, 4=NET, 5=WIRE(submenu), 6=CODE, 7=RELAY(submenu), 8=RFID(submenu)
  void showMenuItem() {
    display.clear();
    display.setDisplayColon(false);

    switch (menuItem) {
      case 0: display.printString("BRIG"); break;  // Brightness
      case 1: display.printString("VOL "); break;  // Volume
      case 2: display.printString("TICK"); break;  // Tick sound
      case 3: display.printString("SHAK"); break;  // Shake/Accel mode
      case 4: display.printString("NET "); break;  // RS485 Network
      case 5: display.printString("WIRE"); break;  // Wires submenu
      case 6: display.printString("CODE"); break;  // Defuse code
      case 7: display.printString("RELE"); break;  // Relay times submenu
      case 8: display.printString("RFID"); break;  // RFID submenu
      default: menuItem = 0; showMenuItem(); return;
    }
  }

  // WIRE submenu: ORDR, DET, DEF, SPD, PAUS (5 items: 0-4)
  void showWireSubmenuItem(int subItem) {
    display.clear();
    display.setDisplayColon(false);

    switch (subItem) {
      case 0: display.printString("ORDR"); break;  // Wire order
      case 1: display.printString("DET "); break;  // Detonation wire
      case 2: display.printString("DEF "); break;  // Defuse wire
      case 3: display.printString("SPD "); break;  // Speedup wire
      case 4: display.printString("PAUS"); break;  // Pause wire
      default: subItem = 0; showWireSubmenuItem(subItem); return;
    }
  }

  // RELE submenu: TDET, TDEF (2 items: 0-1)
  void showReleSubmenuItem(int subItem) {
    display.clear();
    display.setDisplayColon(false);

    switch (subItem) {
      case 0: display.printString("TDET"); break;  // Trigger Detonation time
      case 1: display.printString("TDEF"); break;  // Trigger Defuse time
      default: subItem = 0; showReleSubmenuItem(subItem); return;
    }
  }

  // Adjustments in the main menu (8 items: BRIGHT, VOLUME, TICK, SHAK, RS485, WIRE, CODE, RELAY)
  void adjustMenuItem(int direction) {
    switch (menuItem) {
      case 0:  // BRIGHT (0-15)
        config.brightness = constrainValue(config.brightness + direction, 0, 15);
        Serial.printf("[ADJ] brightness=%d\n", config.brightness);
        display.setBrightness(config.brightness);
        showNum(config.brightness);
        break;

      case 1:  // VOLUME (0-255, step 16)
        config.volume = constrainValue(config.volume + (direction * 16), 0, 255);
        Serial.printf("[ADJ] volume=%d\n", config.volume);
        hw.sound().setVolume(config.volume);
        showNum(config.volume / 16);
        break;

      case 2:  // TICK (bool)
        config.tickEnabled = !config.tickEnabled;
        Serial.printf("[ADJ] tickEnabled=%d\n", config.tickEnabled);
        showBool(config.tickEnabled);
        break;

      case 3:  // SHAK (0-4) - shows option name
        config.accelMode = constrainValue(config.accelMode + direction, 0, 4);
        Serial.printf("[ADJ] accelMode=%d\n", config.accelMode);
        showShakeMode(config.accelMode);
        break;

      case 4:  // RS485 (bool)
        config.rs485Enabled = !config.rs485Enabled;
        Serial.printf("[ADJ] rs485Enabled=%d\n", config.rs485Enabled);
        showBool(config.rs485Enabled);
        break;

      case 5:  // WIRE - submenu, does not adjust here
        break;

      case 6:  // CODE - special input mode
        if (direction > 0) {
          inputCode();
          inAdjustMode = false;
          showMenuItem();
        }
        break;

      case 7:  // RELE – Submenu; it doesn’t adjust here.
        break;

      case 8:  // RFID toggle ON/OFF
        config.rfidEnabled = !config.rfidEnabled;
        Serial.printf("[ADJ] rfidEnabled=%d\n", config.rfidEnabled);
        showBool(config.rfidEnabled);
        break;
    }
  }

  // Adjustment in the WIRE submenu (0=ORDR, 1=DET, 2=DEF, 3=SPD, 4=PAUS)
  void adjustWireSubmenuItem(int direction) {
    switch (wireSubItem) {
      case 0:  // ORDR - does not adjust with +/-, uses special input
        break;

      case 1:  // DET (0-4, where 0=NONE)
        {
          int newVal = constrainValue(config.detWire + direction, 0, 4);
          if (newVal > 0 && wireConflict(1, newVal)) {
            display.printString("err ");
            hw.sound().playOnce("/error.mp3");
            delay(500);
            showWireValue(config.detWire);
          } else {
            config.detWire = newVal;
            Serial.printf("[ADJ] detWire=%d\n", config.detWire);
            showWireValue(config.detWire);
          }
        }
        break;

      case 2:  // DEF (0-4, where 0=NONE)
        {
          int newVal = constrainValue(config.defuseWire + direction, 0, 4);
          if (newVal > 0 && wireConflict(2, newVal)) {
            display.printString("err ");
            hw.sound().playOnce("/error.mp3");
            delay(500);
            showWireValue(config.defuseWire);
          } else {
            config.defuseWire = newVal;
            Serial.printf("[ADJ] defuseWire=%d\n", config.defuseWire);
            showWireValue(config.defuseWire);
          }
        }
        break;

      case 3:  // SPD (0-4, where 0=NONE)
        {
          int newVal = constrainValue(config.speedupWire + direction, 0, 4);
          if (newVal > 0 && wireConflict(3, newVal)) {
            display.printString("err ");
            hw.sound().playOnce("/error.mp3");
            delay(500);
            showWireValue(config.speedupWire);
          } else {
            config.speedupWire = newVal;
            Serial.printf("[ADJ] speedupWire=%d\n", config.speedupWire);
            showWireValue(config.speedupWire);
          }
        }
        break;

      case 4:  // PAUS (0-4, where 0=NONE)
        {
          int newVal = constrainValue(config.pauseWire + direction, 0, 4);
          if (newVal > 0 && wireConflict(4, newVal)) {
            display.printString("err ");
            hw.sound().playOnce("/error.mp3");
            delay(500);
            showWireValue(config.pauseWire);
          } else {
            config.pauseWire = newVal;
            Serial.printf("[ADJ] pauseWire=%d\n", config.pauseWire);
            showWireValue(config.pauseWire);
          }
        }
        break;
    }
  }

  // Adjustment in the RELE submenu (0=TDET, 1=TDEF)
  void adjustReleSubmenuItem(int direction) {
    switch (releSubItem) {
      case 0:  // TDET (1-10)
        config.detTriggerSec = constrainValue(config.detTriggerSec + direction, 1, 10);
        Serial.printf("[ADJ] detTriggerSec=%d\n", config.detTriggerSec);
        showNum(config.detTriggerSec);
        break;

      case 1:  // TDEF (1-10)
        config.defuseTriggerSec = constrainValue(config.defuseTriggerSec + direction, 1, 10);
        Serial.printf("[ADJ] defuseTriggerSec=%d\n", config.defuseTriggerSec);
        showNum(config.defuseTriggerSec);
        break;
    }
  }

  int constrainValue(int value, int min, int max) {
    if (value < min) return max;
    if (value > max) return min;
    return value;
  }

  void showCurrentValue() {
    // Shows the current value when entering main menu adjust mode
    switch (menuItem) {
      case 0: showNum(config.brightness); break;           // BRIGHT
      case 1: showNum(config.volume / 16); break;          // VOLUME
      case 2: showBool(config.tickEnabled); break;         // TICK
      case 3: showShakeMode(config.accelMode); break;      // SHAK
      case 4: showBool(config.rs485Enabled); break;        // RS485
      case 5: showWireSubmenuItem(0); break;               // WIRE submenu
      case 6: display.printString("CODE"); break;          // CODE
      case 7: showReleSubmenuItem(0); break;               // RELE Submenu
      case 8: showBool(config.rfidEnabled); break;         // RFID ON/OFF
    }
  }

  void showCurrentWireValue(int subItem) {
    switch (subItem) {
      case 0: display.printString("ORDR"); break;  // ORDR
      case 1: showWireValue(config.detWire); break;       // DET
      case 2: showWireValue(config.defuseWire); break;    // DEF
      case 3: showWireValue(config.speedupWire); break;   // SPD
      case 4: showWireValue(config.pauseWire); break;    // PAUS
    }
  }

  void showCurrentReleValue(int subItem) {
    switch (subItem) {
      case 0: showNum(config.detTriggerSec); break;   // TDET
      case 1: showNum(config.defuseTriggerSec); break; // TDEF
    }
  }

  void showShakeMode(int mode) {
    // Shows the name of the SHAK mode: OFF, STRT, FAST, PAUS, BOOM
    switch (mode) {
      case 0: display.printString("OFF "); break;   // ACCEL_OFF
      case 1: display.printString("STRT"); break;   // ACCEL_START
      case 2: display.printString("FAST"); break;   // ACCEL_SPEED2X
      case 3: display.printString("PAUS"); break;   // ACCEL_PAUSE
      case 4: display.printString("BOOM"); break;   // ACCEL_DETONATE
    }
    // Waits 500ms for reading before returning to the menu
    delay(500);
    if (inWireSubmenu) {
      showWireSubmenuItem(wireSubItem);
    } else if (inReleSubmenu) {
      showReleSubmenuItem(releSubItem);
    } else {
      showMenuItem();
    }
  }

  void showWireValue(int val) {
    // Shows wire value: none, 1-4
    if (val == 0) display.printString("NONE");
    else {
      char buf[2] = {'0' + val, 0};
      display.printString(buf);
    }
    delay(500);
    showWireSubmenuItem(wireSubItem);
  }

  void showWire(int val) {
    if (val == 0) display.printString("none");
    else if (val == 1) display.printString("rand");
    else { char buf[2] = {'0' + (val - 1), 0}; display.printString(buf); }
    // Waits 500ms before returning to the menu
    delay(500);
    showMenuItem();
  }

  void showNum(int val) {
    display.printNumber(val);
    // Waits 500ms before returning to the menu
    delay(500);
    showMenuItem();
  }

  void showBool(bool val) {
    display.printString(val ? " on " : " off");
    // Waits 500ms before returning to the menu
    delay(500);
    showMenuItem();
  }

  // =======================================================================
  // CODE AND ORDER ENTRY
  // =======================================================================

  void inputCode() {
    display.printString("____");
    int pos = 0;
    int tempCode = 0;

    while (true) {
      taskManager.runLoop();
      switches.runLoop();
      hw.sound().loop();

      char key = keypadListener.lastKey;
      if (key != 0) {
        keypadListener.lastKey = 0;

        if (key >= '0' && key <= '9' && pos < 4) {
          tempCode = tempCode * 10 + (key - '0');
          display.writeDigitAscii(pos, key);
          pos++;
          hw.sound().playOnce("/keypress.mp3");
        }

        if (pos >= 4) {
          config.defuseCode = tempCode;
          hw.sound().playOnce("/keypress.mp3");
          // Clears DET flag to avoid double trigger
          btnPressed[3] = false;
          break;
        }
      }

      if (buttonPressed(BUTTON_DET_PIN)) {
        // Cancels entry
        btnPressed[3] = false;
        break;
      }

      delay(10);
    }

    showMenuItem();
  }

  void inputWireOrder() {
    display.printString("____");
    int pos = 0;
    int used[4] = {0, 0, 0, 0};
    int tempOrder = 0;

    while (true) {
      taskManager.runLoop();
      switches.runLoop();
      hw.sound().loop();

      char key = keypadListener.lastKey;
      if (key != 0) {
        keypadListener.lastKey = 0;

        if (key >= '1' && key <= '4' && pos < 4) {
          int wire = key - '0';
          if (!used[wire - 1]) {
            used[wire - 1] = 1;
            tempOrder = tempOrder * 10 + wire;
            display.writeDigitAscii(pos, key);
            pos++;
            hw.sound().playOnce("/keypress.mp3");
          }
        }

        if (pos >= 4) {
          config.defuseOrder = tempOrder;
          hw.sound().playOnce("/keypress.mp3");
          // Clears DET flag to avoid double trigger
          btnPressed[3] = false;
          break;
        }
      }

      if (buttonPressed(BUTTON_DET_PIN)) {
        // Cancels entry
        btnPressed[3] = false;
        break;
      }

      delay(10);
    }

    showMenuItem();
  }

  // =======================================================================
  // HELPERS RFID
  // =======================================================================

  static uint8_t hexStringToBytes(const char* hexStr, uint8_t* bytes, uint8_t maxBytes) {
    uint8_t count = 0;
    const char* p = hexStr;
    while (*p && count < maxBytes) {
      while (*p && !isxdigit(*p)) p++;
      if (!*p) break;
      unsigned int b;
      if (sscanf(p, "%2x", &b) != 1) break;
      bytes[count++] = (uint8_t)b;
      p += 2;
    }
    return count;
  }

  static void bytesToHexString(const uint8_t* bytes, uint8_t len, char* out) {
    char* p = out;
    for (uint8_t i = 0; i < len; i++) {
      if (i > 0) *p++ = ':';
      p += sprintf(p, "%02X", bytes[i]);
    }
    *p = '\0';
  }

  // =======================================================================
  // SUBMENU RFID
  // =======================================================================

  void showRfidSubmenuItem(int subItem) {
    display.clear();
    display.setDisplayColon(false);

    switch (subItem) {
      case 0: display.printString("DEFU"); break;
      case 1: display.printString("DETO"); break;
      default: subItem = 0; showRfidSubmenuItem(subItem); return;
    }
  }

  // =======================================================================
  // NON-BLOCKING RFID POLL (called from configure while in the submenu)
  // =======================================================================

  void pollRfidTag() {
    if (!nfc_) return;
    if (millis() - lastRfidPoll_ < 250) return;
    lastRfidPoll_ = millis();

    bool isDefuse = (rfidSubItem == 0);  // 0=DEFU, 1=DETO
    int& tagCount = isDefuse ? config.defuseTagCount : config.detonateTagCount;
    RfidTagBin* tags = isDefuse ? config.defuseTags : config.detonateTags;
    RfidTagBin* otherTags = isDefuse ? config.detonateTags : config.defuseTags;
    int otherTagCount = isDefuse ? config.detonateTagCount : config.defuseTagCount;

    NfcReader::CardData card = nfc_->readCard(100);
    if (!card.present || card.uidLength == 0) return;

    // Ignores the same UID as the last read
    if (card.uidLength == lastRfidUidLen_ &&
        memcmp(card.uid, lastRfidUid_, lastRfidUidLen_) == 0) return;
    memcpy(lastRfidUid_, card.uid, card.uidLength);
    lastRfidUidLen_ = card.uidLength;

    // Checks for duplicate in the current mode
    for (int i = 0; i < tagCount; i++) {
      if (tags[i].valid && tags[i].len == card.uidLength &&
          memcmp(tags[i].bytes, card.uid, card.uidLength) == 0) {
        Serial.println(F("[RFID] dup in current mode"));
        strcpy(rfidFeedbackMsg, "dup ");
        rfidFeedbackUntil = millis() + 400;
        return;
      }
    }

    // Checks for duplicate in the other mode (cross-mode)
    for (int i = 0; i < otherTagCount; i++) {
      if (otherTags[i].valid && otherTags[i].len == card.uidLength &&
          memcmp(otherTags[i].bytes, card.uid, card.uidLength) == 0) {
        Serial.println(F("[RFID] dup in other mode"));
        strcpy(rfidFeedbackMsg, "dup ");
        rfidFeedbackUntil = millis() + 400;
        return;
      }
    }

    if (tagCount >= 4) {
      Serial.println(F("[RFID] tag array FULL"));
      hw.sound().playOnce("/error.mp3");
      strcpy(rfidFeedbackMsg, "FULL");
      rfidFeedbackUntil = millis() + 500;
      return;
    }

    // Adds tag
    tags[tagCount].len = card.uidLength;
    memcpy(tags[tagCount].bytes, card.uid, card.uidLength);
    tags[tagCount].valid = true;
    tagCount++;

    Serial.printf("[RFID] Tag %d registered: ", tagCount);
    for (uint8_t i = 0; i < card.uidLength; i++)
      Serial.printf("%02X%s", card.uid[i], i < card.uidLength - 1 ? ":" : "");
    Serial.println();

    hw.sound().playOnce("/keypress.mp3");
    strcpy(rfidFeedbackMsg, "GOOD");
    rfidFeedbackUntil = millis() + 500;
  }

  // =======================================================================
  // CHECAGEM RFID DURANTE COUNTDOWN
  // =======================================================================

  void checkRfidDuringCountdown() {
    if (!config.rfidEnabled || !nfc_) return;

    if (millis() - lastRfidPoll_ < 300) return;
    lastRfidPoll_ = millis();

    NfcReader::CardData card = nfc_->readCard(100);
    if (!card.present) {
      lastRfidUidLen_ = 0;
      return;
    }

    if (card.uidLength == lastRfidUidLen_ &&
        memcmp(card.uid, lastRfidUid_, lastRfidUidLen_) == 0) {
      return;
    }

    memcpy(lastRfidUid_, card.uid, card.uidLength);
    lastRfidUidLen_ = card.uidLength;

    // Checks DEFUSE if there are registered tags
    if (config.defuseTagCount > 0) {
      for (int i = 0; i < config.defuseTagCount; i++) {
        if (config.defuseTags[i].valid &&
            config.defuseTags[i].len == card.uidLength &&
            memcmp(config.defuseTags[i].bytes, card.uid, card.uidLength) == 0) {
          Serial.println(F("[RFID] DEFUSE tag matched!"));
          isDefused = true;
          return;
        }
      }
    }

    // Checks DETONATE if there are registered tags
    if (config.detonateTagCount > 0) {
      for (int i = 0; i < config.detonateTagCount; i++) {
        if (config.detonateTags[i].valid &&
            config.detonateTags[i].len == card.uidLength &&
            memcmp(config.detonateTags[i].bytes, card.uid, card.uidLength) == 0) {
          Serial.println(F("[RFID] DETONATE tag matched!"));
          countdownSeconds = 0;
          return;
        }
      }
    }
  }

  // =======================================================================
  // =======================================================================

  void updateDisplay() {
    display.printTime(countdownSeconds / 60, countdownSeconds % 60);
  }

  void tick() {
    if (config.tickEnabled) {
      hw.sound().playOnce("/tick.mp3");
    }
  }

  // =======================================================================
  // BUTTONS (reads flags set by SwitchInput)
  // =======================================================================

  boolean buttonPressed(int button) {
    int idx = button - BUTTON_LEFT_PIN;
    if (idx < 0 || idx > 3) return false;

    if (btnPressed[idx]) {
      btnPressed[idx] = false;  // Clears flag
      return true;
    }
    return false;
  }

  // =======================================================================
  // NEOPIXEL - ADD ADAFRUIT LED ANIMATION SUPPORT TO THE LIBRARY
  // =======================================================================

  // Applies configured brightness (0-15) to the NeoPixel colors
  uint32_t neoColor(uint8_t r, uint8_t g, uint8_t b) {
    uint16_t scale = constrain(config.brightness, 0, 15) * 17;
    return Adafruit_NeoPixel::Color(
      (uint16_t)r * scale / 255,
      (uint16_t)g * scale / 255,
      (uint16_t)b * scale / 255
    );
  }

  void setNeoEffect(int effect) {
    neoEffect = effect;
    neoStep = 0;
  }

  void updateNeoEffect() {
    if (millis() - neoLastUpdate < 50) return;
    neoLastUpdate = millis();

    switch (neoEffect) {
      case NEOFX_OFF:
        hw.setLightAll(0);
        break;

      case NEOFX_NORMAL: {
        uint8_t b = 50 + (uint8_t)(50 * sin(millis() / 1000.0));
        hw.setLightAll(neoColor(b, b, b));
        break;
      }

      case NEOFX_COUNTING: {
        uint8_t b = 100 + (uint8_t)(100 * sin(millis() / 500.0));
        hw.setLightAll(neoColor(b, 0, 0));
        break;
      }

      case NEOFX_DOUBLE_SPEED: {
        uint8_t b = (millis() % 250 < 125) ? 255 : 50;
        hw.setLightAll(neoColor(b, 0, 0));
        break;
      }

      case NEOFX_DEFUSED: {
        uint8_t b = 100 + (uint8_t)(155 * sin(millis() / 800.0));
        hw.setLightAll(neoColor(0, b, 0));
        break;
      }

      case NEOFX_EXPLOSION:
        doExplosionEffect();
        break;

      case NEOFX_RADIATION: {
        uint8_t flicker = random(50, 255);
        hw.setLightAll(neoColor(random(20), flicker, random(50)));
        break;
      }
    }
  }

  void doExplosionEffect() {
    uint32_t elapsed = millis() % 3000;

    if (elapsed < 200) {
      hw.setLightAll(neoColor(255, 255, 255));
    } else if (elapsed < 500) {
      uint8_t fade = 255 - ((elapsed - 200) * 255 / 300);
      hw.setLightAll(neoColor(255, 255, fade));
    } else if (elapsed < 1000) {
      uint8_t green = 150 - ((elapsed - 500) * 150 / 500);
      hw.setLightAll(neoColor(255, green, 0));
    } else if (elapsed < 2000) {
      uint8_t brightness = 255 - ((elapsed - 1000) * 200 / 1000);
      hw.setLightAll(neoColor(brightness, 0, 0));
    } else {
      if (random(100) < 30) {
        hw.setLightAll(neoColor(255, random(100), 0));
      } else {
        hw.setLightAll(neoColor(random(50), 0, 0));
      }
    }
  }

  // =======================================================================
  // CONFIGURATION PERSISTENCE - bomb.json in LittleFS
  // =======================================================================

  void loadConfig() {
    if (!LittleFS.exists(CONFIG_FILE)) {
      Serial.println(F("[GameTimer] Config file does not exist, creating default..."));
      saveConfig();
      return;
    }

    File f = LittleFS.open(CONFIG_FILE, "r");
    JsonDocument doc;
    deserializeJson(doc, f);

    config.countdownSeconds = doc["ctdn"] | 600;
    config.defuseMode = doc["mode"] | 0;
    config.defuseCode = doc["code"] | 1234;
    config.defuseOrder = doc["ordr"] | 1234;
    config.detWire = doc["detW"] | 1;
    config.defuseWire = doc["dffW"] | 1;
    config.speedupWire = doc["spdW"] | 0;
    config.pauseWire = doc["psW"] | 0;
    config.volume = doc["vol"] | 128;
    config.brightness = doc["bri"] | 15;
    config.detTriggerSec = doc["tdEt"] | 5;
    config.defuseTriggerSec = doc["tdEF"] | 5;
    config.rs485Enabled = doc["rs485"] | 0;
    config.netPuzzlesCount = doc["netC"] | 4;
    config.accelMode = doc["accl"] | 0;
    config.tickEnabled = doc["tic"] | 1;

    // RFID
    config.rfidEnabled = doc["rFId"] | 0;
    config.defuseTagCount = 0;
    config.detonateTagCount = 0;
    memset(config.defuseTags, 0, sizeof(config.defuseTags));
    memset(config.detonateTags, 0, sizeof(config.detonateTags));
    if (doc["dTAG"].is<JsonArray>()) {
      auto arr = doc["dTAG"].as<JsonArray>();
      int count = min((int)arr.size(), 4);
      for (int i = 0; i < count; i++) {
        config.defuseTags[i].len = hexStringToBytes(arr[i] | "", config.defuseTags[i].bytes, 7);
        config.defuseTags[i].valid = (config.defuseTags[i].len > 0);
        if (config.defuseTags[i].valid) config.defuseTagCount++;
      }
    }
    if (doc["oTAG"].is<JsonArray>()) {
      auto arr = doc["oTAG"].as<JsonArray>();
      int count = min((int)arr.size(), 4);
      for (int i = 0; i < count; i++) {
        config.detonateTags[i].len = hexStringToBytes(arr[i] | "", config.detonateTags[i].bytes, 7);
        config.detonateTags[i].valid = (config.detonateTags[i].len > 0);
        if (config.detonateTags[i].valid) config.detonateTagCount++;
      }
    }

    f.close();

    // Full debug of the loaded configuration
    Serial.println(F("\n[GameTimer] ========================================="));
    Serial.println(F("[GameTimer] CONFIGURATION LOADED:"));
    Serial.println(F("[GameTimer] ========================================="));
    Serial.printf("[GameTimer] Countdown: %d seconds\n", config.countdownSeconds);
    Serial.printf("[GameTimer] Defuse mode: %d (0=CODE, 1=WIRE, 2=ORDER, 3=NET)\n", config.defuseMode);
    Serial.printf("[GameTimer] Code: %04d\n", config.defuseCode);
    Serial.printf("[GameTimer] Wire order: %04d\n", config.defuseOrder);
    Serial.printf("[GameTimer] Detonator wire: %d (0=none, 1=rand, 2-5=wire)\n", config.detWire);
    Serial.printf("[GameTimer] Defuse wire: %d\n", config.defuseWire);
    Serial.printf("[GameTimer] Speedup wire: %d\n", config.speedupWire);
    Serial.printf("[GameTimer] Pause wire: %d\n", config.pauseWire);
    Serial.printf("[GameTimer] Brightness: %d/15\n", config.brightness);
    Serial.printf("[GameTimer] Volume: %d/255\n", config.volume);
    Serial.printf("[GameTimer] Explosion trigger: %ds\n", config.detTriggerSec);
    Serial.printf("[GameTimer] Defuse trigger: %ds\n", config.defuseTriggerSec);
    Serial.printf("[GameTimer] RS485: %s\n", config.rs485Enabled ? "ON" : "OFF");
    Serial.printf("[GameTimer] Accelerometer mode: %d (0=OFF, 1=SPEED2X, 2=PAUSE, 3=DETONATE)\n", config.accelMode);
    Serial.printf("[GameTimer] Tick sound: %s\n", config.tickEnabled ? "ON" : "OFF");
    Serial.printf("[GameTimer] RFID: %s\n", config.rfidEnabled ? "ON" : "OFF");
    Serial.printf("[GameTimer] Defuse tags (%d):\n", config.defuseTagCount);
    for (int i = 0; i < config.defuseTagCount && i < 4; i++) {
      if (config.defuseTags[i].valid) {
        char hexBuf[24];
        bytesToHexString(config.defuseTags[i].bytes, config.defuseTags[i].len, hexBuf);
        Serial.printf("  [%d] %s\n", i, hexBuf);
      }
    }
    Serial.printf("[GameTimer] Detonate tags (%d):\n", config.detonateTagCount);
    for (int i = 0; i < config.detonateTagCount && i < 4; i++) {
      if (config.detonateTags[i].valid) {
        char hexBuf[24];
        bytesToHexString(config.detonateTags[i].bytes, config.detonateTags[i].len, hexBuf);
        Serial.printf("  [%d] %s\n", i, hexBuf);
      }
    }
    Serial.println(F("[GameTimer] =========================================\n"));
  }

  void saveConfig() {
    File f = LittleFS.open(CONFIG_FILE, "w");
    JsonDocument doc;
    doc["ctdn"] = config.countdownSeconds;
    doc["mode"] = config.defuseMode;
    doc["code"] = config.defuseCode;
    doc["ordr"] = config.defuseOrder;
    doc["detW"] = config.detWire;
    doc["dffW"] = config.defuseWire;
    doc["spdW"] = config.speedupWire;
    doc["psW"] = config.pauseWire;
    doc["vol"] = config.volume;
    doc["bri"] = config.brightness;
    doc["tdEt"] = config.detTriggerSec;
    doc["tdEF"] = config.defuseTriggerSec;
    doc["rs485"] = config.rs485Enabled;
    doc["netC"] = config.netPuzzlesCount;
    doc["accl"] = config.accelMode;
    doc["tic"] = config.tickEnabled;
    doc["rFId"] = config.rfidEnabled;

    // Serializes defuse tags
    JsonArray dTagArr = doc["dTAG"].to<JsonArray>();
    for (int i = 0; i < config.defuseTagCount; i++) {
      if (config.defuseTags[i].valid) {
        char hexBuf[22];
        bytesToHexString(config.defuseTags[i].bytes, config.defuseTags[i].len, hexBuf);
        dTagArr.add(hexBuf);
      }
    }

    // Serializes detonation tags
    JsonArray oTagArr = doc["oTAG"].to<JsonArray>();
    for (int i = 0; i < config.detonateTagCount; i++) {
      if (config.detonateTags[i].valid) {
        char hexBuf[22];
        bytesToHexString(config.detonateTags[i].bytes, config.detonateTags[i].len, hexBuf);
        oTagArr.add(hexBuf);
      }
    }

    serializeJson(doc, f);
    f.close();
    Serial.println(F("[GameTimer] Config saved"));
  }
};

// ============================================================================
// SwitchInput CALLBACKS (outside the class)
// ============================================================================

// Button and wire state variables
volatile bool btnPressed[4] = {false, false, false, false};
volatile bool wireCutFlag[4] = {false, false, false, false};

void onButtonPressed(pinid_t pin, bool held) {
  // Only processes if held=false (initial press, not release)
  if (held) return;

  int idx = pin - BUTTON_LEFT_PIN;
  if (idx >= 0 && idx < 4) {
    btnPressed[idx] = true;
    const char* btnName = (idx == 0) ? "LEFT" : (idx == 1) ? "SELECT" : (idx == 2) ? "RIGHT" : "DET";
    Serial.printf("[BTN] %s (P%d) PRESSED\n", btnName, pin);
  }
}

// Maps physical pin to wire index (0-3)
int wirePinToIndex(pinid_t pin) {
  switch(pin) {
    case WIRE1_PIN: return 0;
    case WIRE2_PIN: return 1;
    case WIRE3_PIN: return 2;
    case WIRE4_PIN: return 3;
    default: return -1;
  }
}

void onWireCut(pinid_t pin, bool held) {
  int wireNum = wirePinToIndex(pin);
  if (wireNum >= 0 && wireNum < 4) {
    wireCutFlag[wireNum] = true;
    Serial.printf("[WIRE] P%d (Wire %d) CUT\n", pin, wireNum + 1);
  }
}

// ============================================================================
// MAIN
// ============================================================================
propmaker_escaperoom::LogicManager master(1, true);
GameTimerPuzzle puzzle;

void setup() {
  Serial.begin(115200);
  uint32_t start = millis();
  while (!Serial && (millis() - start < 3000)) delay(10);

  Serial.println(F("============================================"));
  Serial.println(F("  GameTimerPro 2.0 - PropMaker EscapeRoom"));
  Serial.println(F("March/April 2026 – Nicolau dos Brinquedos"));
  Serial.println(F("============================================"));

  hw.begin();
  hw.addPcf8575(0x20);

  master.addPuzzle(&puzzle);
  master.begin();
  // puzzle.run();  // Manual start via BTN_DET or RS485 command
  master.setNetworkEnabled(true);

  Serial.println(F("[Main] System ready"));
}

void loop() {
  master.update(); // Just update the class.
  delay(1);
}
