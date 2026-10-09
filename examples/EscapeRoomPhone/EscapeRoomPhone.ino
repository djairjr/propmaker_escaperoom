/**
 * MASTER EXAMPLE: ESCAPE ROOM PHONE (RP2040)
 * ------------------------------------------------------
 * Example using PropMakerEscapeRoom - just one include needed!
 *
 * INPUT MODE (uncomment ONLY ONE of the defines below):
 * - No define:            Keypad via I2C with PCF8574/8575 (default - recommended)
 * - #define KEYPAD_GPIO:         Keypad via direct GPIO (legacy)
 * - #define USE_ROTARY_DIAL:     MECHANICAL rotary dial (metal contacts)
 * - #define USE_ROTARY_OPTICAL:  OPTICAL rotary dial (encoder + coded disc)
 *
 * FOR PRODUCTION WITH FREERTOS:
 * 1. Enable FreeRTOS: Tools > RP2040 Boards > "FreeRTOS SMP"
 * 2. Use xTaskCreate() tasks in setup() (see comments at the end)
 *
 * FOR TESTING: Works without FreeRTOS - audio runs via I2S DMA in loop()
 */

// ============================================================================
// INPUT MODE SELECTION (uncomment only ONE)
// ============================================================================
// #define KEYPAD_GPIO         // Keypad via direct GPIO (legacy)
// #define USE_ROTARY_DIAL      // MECHANICAL rotary dial (metal contacts)
// #define USE_ROTARY_OPTICAL   // OPTICAL rotary dial (encoder + coded disc)

// ============================================================================
// OPTICAL MODE CONFIGURATION (only when USE_ROTARY_OPTICAL is active)
// ============================================================================
// These values should be obtained with the DialCalibrator (examples/DialCalibrator).
// Run the calibrator, dial the digits 1-0, and copy the suggested values here.
//
// WHAT EACH VALUE MEANS:
//   ROTARY_DEBOUNCE : floor (ms) — ignores pulses shorter than this value.
//                     Filters noise, bouncing and pulses from the disc's return.
//                     Typical values: mechanical=15, optical Djair=50.
//                     ⚠ Increase it if it captures false pulses, decrease it if it misses pulses.
//
//   ROTARY_MAXGAP   : ceiling (ms) — ignores pulses longer than this value.
//                     0 = no ceiling (recommended for Djair's optical disc,
//                     where gap and slot have overlapping ranges).
//                     ⚠ Only enable if gap and slot have clearly distinct durations.
//
//   ROTARY_DIGITEND : timeout (ms) — time without pulses to close the digit.
//                     Must be greater than the largest gap between digits on your disc.
//                     Typical values: mechanical=600, optical Djair=800.
//                     ⚠ Too low: breaks the digit in the middle. Too high: slowness.

#define USE_ROTARY_OPTICAL // Matheus Leite Version

#ifdef USE_ROTARY_OPTICAL
#define ROTARY_DEBOUNCE  60      // floor (ms)
#define ROTARY_MAXGAP    0       // ceiling (0 = no ceiling)
#define ROTARY_DIGITEND  800     // timeout (ms)
#endif

#include <PropMakerEscapeRoom.h>
using namespace propmaker_escaperoom;

// ============================================================================
// INPUT CONFIGURATION ACCORDING TO THE SELECTED MODE
// ============================================================================

#if defined(USE_ROTARY_DIAL) || defined(USE_ROTARY_OPTICAL)
// Rotary dial (mechanical or optical) via Hardware.h
RotaryDialReader* rotaryDial = nullptr;

#elif defined(KEYPAD_GPIO)
// ============================================================================
// KEYPAD VIA DIRECT GPIO (LEGACY - without the Keypad library)
// ============================================================================
// Native matrix keypad implementation using direct GPIO.
// Avoids conflicts with IoAbstraction (both libraries define PRESSED).

const byte KEYPAD_ROWS = 4;
const byte KEYPAD_COLS = 3;
const char KEYPAD_MAP[KEYPAD_ROWS][KEYPAD_COLS] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};

// PropMaker RP2040 GPIO pins for the keypad
const byte KEYPAD_ROW_PINS[KEYPAD_ROWS] = {6, 7, 8, 9};    // Rows (OUTPUT)
const byte KEYPAD_COL_PINS[KEYPAD_COLS] = {10, 11, 12};     // Columns (INPUT_PULLUP)

class GpioKeypad {
public:
    void begin() {
        // Configures rows as OUTPUT (initially HIGH)
        for (int i = 0; i < KEYPAD_ROWS; i++) {
            pinMode(KEYPAD_ROW_PINS[i], OUTPUT);
            digitalWrite(KEYPAD_ROW_PINS[i], HIGH);
        }
        // Configures columns as INPUT_PULLUP
        for (int i = 0; i < KEYPAD_COLS; i++) {
            pinMode(KEYPAD_COL_PINS[i], INPUT_PULLUP);
        }
    }

    char getKey() {
        char key = 0;
        // Scans each row
        for (int row = 0; row < KEYPAD_ROWS; row++) {
            // Activates the current row (LOW)
            digitalWrite(KEYPAD_ROW_PINS[row], LOW);
            delayMicroseconds(10);  // Small delay for stabilization

            // Checks each column
            for (int col = 0; col < KEYPAD_COLS; col++) {
                if (digitalRead(KEYPAD_COL_PINS[col]) == LOW) {
                    // Key pressed (LOW due to the pull-up and connection to the LOW row)
                    key = KEYPAD_MAP[row][col];
                    // Waits for release (simple debounce)
                    while (digitalRead(KEYPAD_COL_PINS[col]) == LOW) {
                        delayMicroseconds(100);
                    }
                    delay(50);  // Additional debounce
                }
            }

            // Deactivates the row (HIGH)
            digitalWrite(KEYPAD_ROW_PINS[row], HIGH);
        }
        return key;
    }

    void setDebounceTime(unsigned int time) { debounceMs = time; }

private:
    unsigned int debounceMs = 50;
};

GpioKeypad keypadGpio;

#else
// ============================================================================
// KEYPAD VIA I2C WITH IOABSTRACTION (DEFAULT - RECOMMENDED)
// ============================================================================
// Uses PCF8574/8575 + IoAbstraction (automatically included in PropMakerEscapeRoom)

static const char keymap[] PROGMEM = "123456789*0#";
KeyboardLayout keypadLayout(4, 3, keymap);

// PCF8574/8575 pins connected to the keypad (address configured in setup)
byte rowPins[4] = {3, 2, 1, 0};   // P3-P0 (Rows)
byte colPins[3] = {6, 5, 4};      // P6-P4 (Columns)

class KeypadListener : public KeyboardListener {
public:
    void keyPressed(char key, bool held) override {
        if (!held) {
            lastKey = key;
            lastKeyTime = millis();
        }
    }
    void keyReleased(char key) override {}

    char getLastKey() {
        char k = lastKey;
        lastKey = 0;
        return k;
    }
    unsigned long getLastKeyTime() { return lastKeyTime; }

private:
    volatile char lastKey = 0;
    volatile unsigned long lastKeyTime = 0;
};

KeypadListener keypadListener;
MatrixKeyboardManager keypadManager;
#endif

// ============================================================================
// NOTE: Three input modes supported
// ============================================================================
// 1. KEYPAD I2C (default): PCF8574/8575 + IoAbstraction - more robust, frees GPIOs
// 2. KEYPAD GPIO (legacy): Keypad via direct GPIO - for existing hardware
// 3. ROTARY DIAL: Rotary dial - for old phones (MECHANICAL - default and OPTICAL restored)
//
// To migrate from GPIO to I2C: replace KEYPAD_GPIO with no define (default)
// and connect the keypad to a PCF8574/8575 on the I2C bus.

using namespace propmaker_escaperoom;

// ============================================================================
// CONFIGURATIONS AND MAPPING (According to the Real Hardware)
// ============================================================================
#define PHONE_PIN_HOOK           19
#define PHONE_PIN_DOORLOCK       10 // PropMaker Relay
#define PHONE_PIN_EXTERNAL_POWER 23
#define PHONE_PIN_ROTARY_PULSE   20 // Servo pin on the PropMaker

#define HOOK_ON_HOOK_LEVEL   HIGH
#define HOOK_OFF_HOOK_LEVEL  LOW

#define LOCK_ACTIVE_LEVEL    LOW
#define LOCK_INACTIVE_LEVEL  HIGH

#define DIAL_TIMEOUT_MS      4000
#define MAX_PHONE_DIGITS     8

const char* FILE_DIALTONE = "/dialtone.mp3";
const char* FILE_BUSY     = "/busy.mp3";
const char* FILE_INVALID  = "/naoexiste.mp3";

const char* FILE_DTMF[] = {
    "/dtmf_0.mp3", "/dtmf_1.mp3", "/dtmf_2.mp3", "/dtmf_3.mp3", "/dtmf_4.mp3",
    "/dtmf_5.mp3", "/dtmf_6.mp3", "/dtmf_7.mp3", "/dtmf_8.mp3", "/dtmf_9.mp3"
};
const char* FILE_DTMF_STAR = "/dtmf_star.mp3";
const char* FILE_DTMF_HASH = "/dtmf_hash.mp3";

const char* FILE_ROTARY[] = {
    "/pulse_0.mp3", "/pulse_1.mp3", "/pulse_2.mp3", "/pulse_3.mp3", "/pulse_4.mp3",
    "/pulse_5.mp3", "/pulse_6.mp3", "/pulse_7.mp3", "/pulse_8.mp3", "/pulse_9.mp3"
};

// ============================================================================
// HOOK ISR
// ============================================================================
volatile bool isHookedGlobal = true;

void IRAM_ATTR handleHookISR() {
    static unsigned long lastInterruptTime = 0;
    unsigned long interruptTime = millis();
    if (interruptTime - lastInterruptTime > 50) {
        isHookedGlobal = (digitalRead(PHONE_PIN_HOOK) == HOOK_ON_HOOK_LEVEL);
    }
    lastInterruptTime = interruptTime;
}

// ============================================================================
// PHONE PUZZLE CLASS
// ============================================================================
class PhonePuzzle : public PuzzleBase {
public:
    PhonePuzzle() : PuzzleBase(100) {}

    void setupHardware() override {
        // Hardware::begin() was already called in setup(), but here we ensure the puzzle-specific pins
        // Note: When KEYPAD_GPIO is defined, we use native pinMode due to the Keypad library macro
        #ifdef KEYPAD_GPIO
        pinMode(PHONE_PIN_HOOK, INPUT_PULLUP);
        pinMode(PHONE_PIN_DOORLOCK, OUTPUT);
        pinMode(PHONE_PIN_EXTERNAL_POWER, OUTPUT);

        digitalWrite(PHONE_PIN_EXTERNAL_POWER, HIGH);
        digitalWrite(PHONE_PIN_DOORLOCK, LOCK_INACTIVE_LEVEL);
        #else
        hw.pinMode(PHONE_PIN_HOOK, INPUT_PULLUP);
        hw.pinMode(PHONE_PIN_DOORLOCK, OUTPUT);
        hw.pinMode(PHONE_PIN_EXTERNAL_POWER, OUTPUT);

        hw.digitalWrite(PHONE_PIN_EXTERNAL_POWER, HIGH);
        hw.digitalWrite(PHONE_PIN_DOORLOCK, LOCK_INACTIVE_LEVEL);
        #endif
        
        isHookedGlobal = (hw.digitalRead(PHONE_PIN_HOOK) == HOOK_ON_HOOK_LEVEL);
        attachInterrupt(digitalPinToInterrupt(PHONE_PIN_HOOK), handleHookISR, CHANGE);
        
        Serial.printf("[PHONE] Hardware Ready. Initial Hook State: %s\n", 
            isHookedGlobal ? "ON HOOK" : "OFF HOOK");
    }

    void update() override {
        // Syncs the initial state by reading the hardware
        bool currentHook = (hw.digitalRead(PHONE_PIN_HOOK) == HOOK_ON_HOOK_LEVEL);
        if (currentHook != isHookedGlobal) {
            isHookedGlobal = currentHook;
            Serial.printf("[PHONE] Hook changed: %s\n", isHookedGlobal ? "ON HOOK" : "OFF HOOK");
        }

        // --- BLOCKING LOOP: ON HOOK ---
        while (isHookedGlobal) {
            // If the handset is on the hook, ANY activity must be annihilated immediately.
            if (audioPlayed || dialtoneSent || isBusyState || !newline) {
                hw.sound().stop();
                resetPhone();
                Serial.println("[PHONE] Hook Detected: System Reset.");
            }
            
            // Re-checks the state to exit the while
            currentHook = (hw.digitalRead(PHONE_PIN_HOOK) == HOOK_ON_HOOK_LEVEL);
            if (currentHook != isHookedGlobal) {
                isHookedGlobal = currentHook;
                Serial.println("[PHONE] Handset lifted!");
                break;
            }
            delay(10);
        }

        // --- BLOCKING LOOP: OFF HOOK ---
        while (!isHookedGlobal) {
            // 1. Dialtone: Only plays if it is clear and nothing has been played
            if (dialed.length() == 0 && !audioPlayed && !dialtoneSent && !isBusyState) {
                if (!hw.sound().isPlaying()) {
                    hw.sound().playLoop(FILE_DIALTONE);
                    dialtoneSent = true;
                }
            }

            // 2. Processes Dialing (Blocked if a message already finished or it is in Busy)
            if (!audioPlayed && !isBusyState) {
                #if defined(USE_ROTARY_DIAL) || defined(USE_ROTARY_OPTICAL)
                processRotary();
                #else
                processKeypad();  // Works for KEYPAD_GPIO and I2C (default)
                #endif
            } else {
                // 3. Post-dialing: Manages the end of audio and the transition to Busy
                if (waitingForStart) {
                    if (hw.sound().isPlaying()) waitingForStart = false;
                } else if (!hw.sound().isPlaying()) {
                    if (!isBusyState) {
                        Serial.println("[PHONE] Message finished. Starting Busy Tone.");
                        hw.sound().playLoop(FILE_BUSY);
                        isBusyState = true;
                        audioPlayed = false;
                        dialtoneSent = true;
                    }
                }
            }

            // Re-checks the state to exit the while
            isHookedGlobal = (hw.digitalRead(PHONE_PIN_HOOK) == HOOK_ON_HOOK_LEVEL);
            delay(1);
        }
    }

private:
    String dialed = "";
    bool audioPlayed = false;
    bool dialtoneSent = false;
    bool isBusyState = false;
    bool waitingForStart = false;
    bool newline = true;
    unsigned long lastDigitAt = 0;

    void resetPhone() {
        dialed = "";
        audioPlayed = false;
        dialtoneSent = false;
        isBusyState = false;
        waitingForStart = false;
        newline = true;
        // Clears the RotaryDial buffer to avoid a phantom digit from the ISR during playback
        #if defined(USE_ROTARY_DIAL) || defined(USE_ROTARY_OPTICAL)
        if (rotaryDial) rotaryDial->reset();
        #endif
    }

    void processCall() {
        String mp3Path = "/" + dialed + ".mp3";
        String txtPath = "/" + dialed + ".txt";

        hw.sound().stop();
        audioPlayed = true;
        waitingForStart = true;
        newline = true;

        Serial.print("[PHONE] Checking SD for: ");
        Serial.println(mp3Path);

        if (SD.exists(mp3Path.c_str())) {
            Serial.println("[PHONE] Success: File found on the SD Card.");
            hw.sound().playOnceBlocking(mp3Path.c_str(), true); // true = Source SD
            
            // Lock logic
            if (SD.exists(txtPath.c_str())) {
                Serial.println("[PHONE] Lock file detected. Opening door.");
                hw.digitalWrite(PHONE_PIN_DOORLOCK, LOCK_INACTIVE_LEVEL); // "Inactive" in your system means release
            }
        } else {
            Serial.println("[PHONE] Error: Number not mapped on the SD. Playing Invalid.");
            hw.sound().playOnceBlocking(FILE_INVALID);
        }
        
        dialed = "";
    }

    #if defined(USE_ROTARY_DIAL) || defined(USE_ROTARY_OPTICAL)
    void processRotary() {
        if (rotaryDial && rotaryDial->available()) {
            int digit = rotaryDial->read();

            if (digit >= 0 && digit <= 9) {
                // Interrupts the dialtone
                dialtoneSent = true;
                hw.sound().stop();

                // Pulse feedback
                // hw.sound().playOnceBlocking(FILE_ROTARY[digit]);

                dialed += String(digit);
                Serial.print(digit);
                lastDigitAt = millis();
                newline = false;
            }
        } else {
            checkDialTimeout();
        }
    }
    #elif defined(KEYPAD_GPIO)
    void processKeypad() {
        char keyChar = keypadGpio.getKey();
        if (keyChar) {
            // Interrupts the dialtone
            dialtoneSent = true;
            hw.sound().stop();

            // DTMF feedback
            const char* dtmfPath = nullptr;
            if (keyChar >= '0' && keyChar <= '9') dtmfPath = FILE_DTMF[keyChar - '0'];
            else if (keyChar == '*') dtmfPath = FILE_DTMF_STAR;
            else if (keyChar == '#') dtmfPath = FILE_DTMF_HASH;

            if (dtmfPath) hw.sound().playOnceBlocking(dtmfPath);

            dialed += keyChar;
            Serial.print(keyChar);
            lastDigitAt = millis();
            newline = false;
        } else {
            checkDialTimeout();
        }
    }
    #else
    void processKeypad() {
        char keyChar = keypadListener.getLastKey();
        if (keyChar != 0) {
            // Interrupts the dialtone
            dialtoneSent = true;
            hw.sound().stop();

            // DTMF feedback
            const char* dtmfPath = nullptr;
            if (keyChar >= '0' && keyChar <= '9') dtmfPath = FILE_DTMF[keyChar - '0'];
            else if (keyChar == '*') dtmfPath = FILE_DTMF_STAR;
            else if (keyChar == '#') dtmfPath = FILE_DTMF_HASH;

            if (dtmfPath) hw.sound().playOnceBlocking(dtmfPath);

            dialed += keyChar;
            Serial.print(keyChar);
            lastDigitAt = keypadListener.getLastKeyTime();
            newline = false;
        } else {
            checkDialTimeout();
        }
    }
    #endif

    void checkDialTimeout() {
        bool isMaxDigits = (dialed.length() == MAX_PHONE_DIGITS);
        bool isTimeout = (!newline && millis() - lastDigitAt > DIAL_TIMEOUT_MS);

        if (isMaxDigits || isTimeout) {
            if (dialed.length() > 0) {
                if (isMaxDigits) {
                    Serial.println("\n[PHONE] Digit limit reached.");
                    delay(300);
                } else {
                    Serial.println("\n[PHONE] Dialing timeout.");
                }
                processCall();
            }
        }
    }
};

// ============================================================================
// SETUP & LOOP
// ============================================================================
LogicManager master(1);
PhonePuzzle phone;

void setup() {
    // 1. Waits for Serial for Debug
    uint32_t start = millis();
    while (!Serial && (millis() - start < 3000)) delay(10);

    // 2. Initializes the Hardware Engine (Activates Serial, SD, Audio, etc)
    hw.begin();
    hw.addPcf8575(0x20);  // Initializes PCF8575 for the I2C keypad
    hw.sound().setVolume(89);

    Serial.println("=========================================");
    Serial.println("    ESCAPEROOM PHONE - LIBRARY VERSION");
    Serial.println("=========================================");

#if defined(USE_ROTARY_OPTICAL)
    rotaryDial = hw.createRotaryDialOptical(PHONE_PIN_ROTARY_PULSE,
                                              ROTARY_DEBOUNCE,
                                              ROTARY_MAXGAP,
                                              ROTARY_DIGITEND);
    if (rotaryDial) {
        Serial.println("[SETUP] OPTICAL rotary dial started (Pin 20)");
        Serial.printf("[SETUP]   Debounce=%lums  MaxGap=%lu  DigitEnd=%lums\n",
                      (unsigned long)ROTARY_DEBOUNCE,
                      (unsigned long)ROTARY_MAXGAP,
                      (unsigned long)ROTARY_DIGITEND);
    } else {
        Serial.println("[SETUP] ERROR: Failed to start Optical RotaryDial");
    }
#elif defined(USE_ROTARY_DIAL)
    rotaryDial = hw.createRotaryDial(PHONE_PIN_ROTARY_PULSE);
    if (rotaryDial) {
        Serial.println("[SETUP] MECHANICAL rotary dial started (Pin 20)");
    } else {
        Serial.println("[SETUP] ERROR: Failed to start RotaryDial");
    }
#elif defined(KEYPAD_GPIO)
    // Keypad via direct GPIO - native implementation
    keypadGpio.begin();
    Serial.println("[SETUP] Keypad GPIO started (Pins 6-12) - NATIVE LEGACY MODE");
#else
    // Keypad via I2C with IoAbstraction (default)
    // Configures the pins in the layout
    for (int i = 0; i < 4; i++) keypadLayout.setRowPin(i, rowPins[i]);
    for (int i = 0; i < 3; i++) keypadLayout.setColPin(i, colPins[i]);

    // Initializes KeyboardManager with the PCF8574/8575 expander
    keypadManager.initialise(hw.portExpander(), &keypadLayout, &keypadListener, false);
    Serial.println("[SETUP] Keypad I2C started with IoAbstraction (PCF8574/8575)");
#endif

    // 3. Initializes the Manager and Puzzles
    master.addPuzzle(&phone);
    master.begin();
    phone.run();
}

void loop() {
    // Without FreeRTOS: audio and logic run in the main loop
    // For production with FreeRTOS, see the instructions in the header
    hw.sound().loop();
    master.update();
    delay(1);
}