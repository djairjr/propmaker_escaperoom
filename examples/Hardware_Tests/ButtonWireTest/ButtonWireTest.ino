/**
 * EXAMPLE: BUTTON WIRE TEST - BUTTON AND WIRE TEST
 * ------------------------------------------------------
 * Isolated test of 4 buttons and 4 wires via PCF8575 (I2C) using SwitchInput.
 *
 * HARDWARE: Adafruit PropMaker Feather RP2040 + PCF8575
 * - PCF8575 I2C address: 0x20
 * - Buttons: P10-P13 (active LOW, INPUT_PULLUP)
 * - Wires: P14-P17 (active LOW when cut, INPUT_PULLUP)
 *
 * SERIAL MESSAGES:
 * - [BTN] LEFT/SEL/RIGHT/DET PRESSED
 * - [WIRE] Wire X CUT
 * - [STATUS] Current state of all pins
 */

#include <PropMakerEscapeRoom.h>
#include <IoAbstractionWire.h>
#include <SwitchInput.h>
#include <TaskManager.h>

using namespace propmaker_escaperoom;

// ============================================================================
// CONFIGURATION (same pins as BombTimer)
// ============================================================================
#define PCF_ADDRESS     0x20   // PCF8575 I2C address

// Buttons (physical P10-P13 = 8-11 in IoAbstraction) - active LOW
#define BUTTON_LEFT_PIN     8   // physical P10
#define BUTTON_SELECT_PIN   9   // physical P11
#define BUTTON_RIGHT_PIN    10  // physical P12
#define BUTTON_DET_PIN      11  // physical P13

// Wires (physical P14-P17 = 12-15 in IoAbstraction) - active LOW when cut
#define WIRE1_PIN           12  // physical P14
#define WIRE2_PIN           13  // physical P15
#define WIRE3_PIN           14  // physical P16
#define WIRE4_PIN           15  // physical P17

// Names for debug
const char* BTN_NAMES[] = {"LEFT", "SELECT", "RIGHT", "DET"};
const char* WIRE_NAMES[] = {"WIRE1", "WIRE2", "WIRE3", "WIRE4"};

// State variables (set by the callbacks)
volatile bool btnPressed[4] = {false, false, false, false};
volatile bool wireCutFlag[4] = {false, false, false, false};

// Counters for statistics
unsigned long btnPressCount[4] = {0, 0, 0, 0};
unsigned long wireCutCount[4] = {0, 0, 0, 0};

// Forward declarations
void onButtonPressed(pinid_t pin, bool held);
void onWireChanged(pinid_t pin, bool held);
void printStatus();

// ============================================================================
// WIRE LISTENER (global for access in setup)
// ============================================================================
class WireListener : public SwitchListener {
    int wireIdx;
public:
    WireListener(int idx) : wireIdx(idx) {}
    void onPressed(pinid_t pin, bool held) override {
        onWireChanged(pin, held);
    }
    void onReleased(pinid_t pin, bool held) override {
        onWireChanged(pin, false);
    }
};

// Global listener instances
WireListener wireListeners[4] = {WireListener(0), WireListener(1), WireListener(2), WireListener(3)};

// ============================================================================
// SwitchInput CALLBACKS
// ============================================================================

void onButtonPressed(pinid_t pin, bool held) {
    int idx = pin - BUTTON_LEFT_PIN;
    if (idx >= 0 && idx < 4) {
        btnPressed[idx] = true;
        btnPressCount[idx]++;
        Serial.printf("[BTN] %s PRESSED%s (count=%lu)\n",
                      BTN_NAMES[idx], held ? " (HELD)" : "", btnPressCount[idx]);

        // Visual feedback on the NeoPixel
        hw.setLightAll(0);
        hw.setLight(idx, Adafruit_NeoPixel::Color(0, 50, 0));  // Green at the index
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

void onWireChanged(pinid_t pin, bool held) {
    int idx = wirePinToIndex(pin);
    if (idx >= 0 && idx < 4) {
        // held=true when wire is cut (open circuit = HIGH on PCF8575)
        // held=false when wire is connected (closed circuit = LOW)

        if (held && !wireCutFlag[idx]) {
            // Wire was just cut
            wireCutFlag[idx] = true;
            wireCutCount[idx]++;
            Serial.printf("[WIRE] %s CUT (count=%lu)\n", WIRE_NAMES[idx], wireCutCount[idx]);

            // Visual feedback: red
            hw.setLightAll(Adafruit_NeoPixel::Color(50, 0, 0));
        }
        else if (!held && wireCutFlag[idx]) {
            // Wire was reconnected
            wireCutFlag[idx] = false;
            Serial.printf("[WIRE] %s RECONNECTED\n", WIRE_NAMES[idx]);

            // Visual feedback: blue
            hw.setLightAll(Adafruit_NeoPixel::Color(0, 0, 50));
        }
    }
}

// ============================================================================
// PUZZLE CLASS
// ============================================================================
class ButtonWireTest : public PuzzleBase {
public:
    ButtonWireTest() : PuzzleBase(99) {}  // ID 99 (test)

    void setupHardware() override {
        Serial.println(F("\n========================================"));
        Serial.println(F("  ButtonWireTest - Input Test"));
        Serial.println(F("========================================"));

        // Initializes SwitchInput
        if (hw.hasPortExpander()) {
            Serial.println(F("[TEST] Initializing SwitchInput on PCF8575..."));
            switches.init(hw.io(), SWITCHES_POLL_KEYS_ONLY, true);  // pull up

            // Adds 4 buttons
            switches.addSwitch(BUTTON_LEFT_PIN, onButtonPressed, NO_REPEAT, false);
            switches.addSwitch(BUTTON_SELECT_PIN, onButtonPressed, NO_REPEAT, false);
            switches.addSwitch(BUTTON_RIGHT_PIN, onButtonPressed, NO_REPEAT, false);
            switches.addSwitch(BUTTON_DET_PIN, onButtonPressed, NO_REPEAT, false);

            // Adds 4 wires (using global listeners)
            // IMPORTANT: invertLogic=true because we want to detect HIGH as "cut"
            // Connected = LOW (GND), Cut = HIGH (pullup)
            switches.addSwitchListener(WIRE1_PIN, &wireListeners[0], NO_REPEAT, true);  // true = inverted
            switches.addSwitchListener(WIRE2_PIN, &wireListeners[1], NO_REPEAT, true);
            switches.addSwitchListener(WIRE3_PIN, &wireListeners[2], NO_REPEAT, true);
            switches.addSwitchListener(WIRE4_PIN, &wireListeners[3], NO_REPEAT, true);

            Serial.println(F("[TEST] 4 buttons + 4 wires configured"));

            // Shows initial state
            delay(100);  // Waits for stabilization
            printStatus();
        } else {
            Serial.println(F("[TEST] ERROR: PCF8575 not detected!"));
        }

        Serial.println(F("\n[TEST] Ready for test."));
        Serial.println(F("[TEST] Press buttons or cut wires to see events."));
        Serial.println(F("[TEST] Full status every 5 seconds.\n"));

        setState(PuzzleState::Running);
    }

    void update() override {
        // Processes SwitchInput (automatic debouncing)
        switches.runLoop();

        // Clears button flags (edge detection)
        for (int i = 0; i < 4; i++) {
            btnPressed[i] = false;
        }

        // Scans all pins 0-15 to identify activity
        scanAllPins();

        // Periodic status
        static unsigned long lastStatus = 0;
        if (millis() - lastStatus >= 5000) {
            lastStatus = millis();
            printStatus();
        }
    }

private:
    void scanAllPins() {
        static unsigned long lastScan = 0;
        static uint16_t lastState = 0xFFFF;  // All HIGH (pullup)

        if (millis() - lastScan < 100) return;  // Scan every 100ms
        lastScan = millis();

        uint16_t currentState = 0;
        for (int i = 0; i < 16; i++) {
            int val = hw.io()->readValue(i);
            if (val == LOW) {
                currentState |= (1 << i);  // Bit i = 1 if LOW
            }
        }

        // Detects changes
        if (currentState != lastState) {
            Serial.println(F("\n--- PIN SCAN 0-15 ---"));
            Serial.print(F("LOW State: "));
            for (int i = 0; i < 16; i++) {
                bool isLow = (currentState >> i) & 1;
                if (isLow) {
                    Serial.printf("P%d ", i);
                }
            }
            Serial.println();

            // Detects which one changed
            uint16_t changed = currentState ^ lastState;
            for (int i = 0; i < 16; i++) {
                if (changed & (1 << i)) {
                    bool nowLow = (currentState >> i) & 1;
                    Serial.printf("CHANGE: P%d went to %s\n", i, nowLow ? "LOW" : "HIGH");
                }
            }
            Serial.println(F("------------------------"));
            lastState = currentState;
        }
    }

    void printStatus() {
        Serial.println(F("\n--- Current Status ---"));

        // Buttons
        Serial.print(F("Buttons: "));
        for (int i = 0; i < 4; i++) {
            int pin = BUTTON_LEFT_PIN + i;
            bool pressed = switches.isSwitchPressed(pin);
            Serial.printf("%s=%s ", BTN_NAMES[i], pressed ? "PRESSED" : "open");
        }
        Serial.println();

        // Wires
        Serial.print(F("Wires: "));
        int wirePins[] = {WIRE1_PIN, WIRE2_PIN, WIRE3_PIN, WIRE4_PIN};
        for (int i = 0; i < 4; i++) {
            bool cut = switches.isSwitchPressed(wirePins[i]);  // HIGH = cut
            Serial.printf("%s=%s ", WIRE_NAMES[i], cut ? "CUT" : "conn");
        }
        Serial.println();

        // Statistics
        Serial.print(F("Stats BTN: "));
        for (int i = 0; i < 4; i++) {
            Serial.printf("%s=%lu ", BTN_NAMES[i], btnPressCount[i]);
        }
        Serial.println();

        Serial.print(F("Stats WIRE: "));
        for (int i = 0; i < 4; i++) {
            Serial.printf("%s=%lu ", WIRE_NAMES[i], wireCutCount[i]);
        }
        Serial.println();
        Serial.println(F("--------------------"));
    }
};

// ============================================================================
// MAIN
// ============================================================================
LogicManager master(99);
ButtonWireTest puzzle;

void setup() {
    Serial.begin(115200);
    uint32_t start = millis();
    while (!Serial && (millis() - start < 3000)) delay(10);

    hw.begin();
    hw.addPcf8575(PCF_ADDRESS);  // Same address as the BombTimer

    master.addPuzzle(&puzzle);
    master.begin();
    puzzle.run();
}

void loop() {
    master.update();
}