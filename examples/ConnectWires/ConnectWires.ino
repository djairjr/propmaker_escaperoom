/**
 * EXAMPLE: CONNECT WIRES - WIRE CONNECTION
 * ------------------------------------------------------
 * Checks whether the wires are connected in the correct pairs.
 * Uses a PCF expander for the sockets and PCA9685 for the voltmeter.
 *
 * Based on the structure of BombTimer.ino - with RS485 network support
 */

#include <PropMakerEscapeRoom.h>
#include <IoAbstractionWire.h>
#include <TaskManager.h>
#include <SwitchInput.h>

using namespace propmaker_escaperoom;

// ============================================================================
// CONSTANTS
// ============================================================================
// Pins on the PCF8575 (IoAbstraction: 0-7=P0-P7, 8-15=P10-P17)
#define BUTTON_RESET_PIN    8   // physical P10 - to reset the puzzle
#define BUTTON_UNLOCK_PIN   9   // physical P11 - manual lock release

// Wire socket configuration (9 sockets = 4 connections + 1 extra)
#define NUM_SOCKETS         9
#define NUM_CONNECTIONS     4

// Socket pins on the PCF (P0-P8)
const uint8_t SOCKET_PINS[NUM_SOCKETS] = {0, 1, 2, 3, 4, 5, 6, 7, 8};

// PCA9685 channel for the analog voltmeter
#define VOLTMETER_CHANNEL   0

// Lock pin (via PCF or native)
#define LOCK_PIN            20

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================
extern volatile bool btnPressed[2];

// Forward declarations
void onButtonPressed(pinid_t pin, bool held);

// ============================================================================
// PUZZLE CLASS
// ============================================================================
class ConnectWiresPuzzle : public PuzzleBase {
public:
    ConnectWiresPuzzle() : PuzzleBase(11) {} // ID 11 on the RS485 network

    void setupHardware() override {
        Serial.println(F("[WIRES] Setup started"));

        // Initializes SwitchInput for the buttons via PCF8575
        if (hw.hasExpander()) {
            Serial.println(F("[WIRES] Initializing SwitchInput on PCF8575..."));
            switches.init(hw.io(), SWITCHES_POLL_KEYS_ONLY, true);  // true = pull up

            // Adds buttons
            switches.addSwitch(BUTTON_RESET_PIN, onButtonPressed, NO_REPEAT, false);
            switches.addSwitch(BUTTON_UNLOCK_PIN, onButtonPressed, NO_REPEAT, false);

            Serial.println(F("[WIRES] SwitchInput OK: 2 buttons"));
        } else {
            Serial.println(F("[WIRES] ERROR: PCF8575 not detected!"));
        }

        // Initializes sockets as INPUT_PULLUP
        for (int i = 0; i < NUM_SOCKETS; i++) {
            hw.inputOutput()->pinMode(SOCKET_PINS[i], INPUT_PULLUP);
        }

        // Initializes lock
        hw.pinMode(LOCK_PIN, OUTPUT);
        hw.digitalWrite(LOCK_PIN, HIGH);  // Lock active at HIGH

        // Zeroes the voltmeter PWM channel
        hw.setAnalogOutput(VOLTMETER_CHANNEL, 0);

        // Initializes connections (fixed or random)
        randomizeConnections();

        // Initial state
        setState(PuzzleState::Idle);

        Serial.println(F("[WIRES] Wire System Ready."));
    }

    void update() override {
        // Processes switch input (buttons)
        switches.runLoop();
        taskManager.runLoop();

        // Processes the success visual effect (non-blocking)
        if (solveEffectActive_) {
            updateSolveEffect();
        }

        // State machine
        switch (getState()) {
            case PuzzleState::Idle:
                // Waits for a start command (manual or via the network)
                if (buttonPressed(BUTTON_RESET_PIN)) {
                    resetPuzzle();
                }
                break;

            case PuzzleState::Running:
                processConnections();
                if (buttonPressed(BUTTON_RESET_PIN)) {
                    resetPuzzle();
                }
                break;

            case PuzzleState::Solved:
                // Manual release with the button (Solved mode)
                handleManualRelease();
                if (buttonPressed(BUTTON_RESET_PIN)) {
                    resetPuzzle();
                }
                break;

            default:
                break;
        }
    }

    void onNetworkPacket(const Rs485Payload& p) override {
        switch (p.pCommand) {
            case NetCommand::ForceSolve:
                if (getState() == PuzzleState::Running) {
                    Serial.println(F("[WIRES] Remote command: ForceSolve"));
                    handleSolve();
                }
                break;

            case NetCommand::GlobalEvent:
                // Global event - can be used for reset
                Serial.println(F("[WIRES] Remote command: GlobalEvent (reset)"));
                resetPuzzle();
                break;

            case NetCommand::StatusReport:
                // Responds with the current state
                Serial.printf("[WIRES] Status: %s, Connections: %d/%d\n",
                             stateName(getState()), activeConnections_, NUM_CONNECTIONS);
                break;

            default:
                break;
        }
    }

private:
    // Connections (can be fixed or random)
    uint8_t connections_[NUM_CONNECTIONS][2];
    bool lastState_[NUM_CONNECTIONS] = {false};
    int activeConnections_ = 0;

    // Non-blocking visual effect for success
    uint8_t solveEffectStep_ = 0;
    unsigned long solveEffectStartTime_ = 0;
    bool solveEffectActive_ = false;

    // ========================================================================
    // MAIN LOGIC
    // ========================================================================
    void processConnections() {
        activeConnections_ = 0;
        bool allWiresCorrect = true;
        bool stateChanged = false;

        // Checks each connection
        for (int i = 0; i < NUM_CONNECTIONS; i++) {
            uint8_t pin1 = connections_[i][0];
            uint8_t pin2 = connections_[i][1];

            bool currentState = checkPair(pin1, pin2);

            if (currentState != lastState_[i]) {
                stateChanged = true;
                lastState_[i] = currentState;

                if (currentState) {
                    Serial.printf("[WIRES] Connection %d established: Pin %d <-> Pin %d\n",
                                  i + 1, pin1, pin2);
                    hw.sound().playOnce("/connected.mp3");
                } else {
                    Serial.printf("[WIRES] Connection %d removed: Pin %d <-> Pin %d\n",
                                  i + 1, pin1, pin2);
                }
            }

            if (currentState) {
                activeConnections_++;
            } else {
                allWiresCorrect = false;
            }
        }

        // Updates the VU Meter via PCA9685 (0-NUM_CONNECTIONS -> 0-100%)
        float percent = map(activeConnections_, 0, NUM_CONNECTIONS, 0, 100) / 100.0f;
        hw.setAnalogOutput(VOLTMETER_CHANNEL, percent);

        // Updates the LED bar
        updateGauge(activeConnections_);

        // Debug when the state changes
        if (stateChanged) {
            Serial.printf("[WIRES] Connections state: %d/%d\n", activeConnections_, NUM_CONNECTIONS);
        }

        // Checks the solution
        if (allWiresCorrect && getState() == PuzzleState::Running) {
            handleSolve();
        } else if (!allWiresCorrect && getState() == PuzzleState::Solved) {
            handleUnsolve();
        }
    }

    // ========================================================================
    // HARDWARE FUNCTIONS
    // ========================================================================
    bool checkPair(uint8_t outPin, uint8_t inPin) {
        auto* io = hw.inputOutput();

        // Configures the output pin
        io->pinMode(outPin, OUTPUT);
        io->digitalWrite(outPin, LOW);

        delayMicroseconds(100);  // Time to stabilize

        // Reads the input pin
        io->pinMode(inPin, INPUT_PULLUP);
        bool connected = (io->digitalRead(inPin) == LOW);

        // Restores the state
        io->digitalWrite(outPin, HIGH);
        io->pinMode(outPin, INPUT_PULLUP);

        return connected;
    }

    void randomizeConnections() {
        // Temporary array with all the pins
        uint8_t pins[NUM_SOCKETS];
        for (int i = 0; i < NUM_SOCKETS; i++) {
            pins[i] = SOCKET_PINS[i];
        }

        // Fisher-Yates shuffle
        for (int i = NUM_SOCKETS - 1; i > 0; i--) {
            int j = random(i + 1);
            uint8_t temp = pins[i];
            pins[i] = pins[j];
            pins[j] = temp;
        }

        // Forms pairs with the first 8 shuffled pins
        for (int i = 0; i < NUM_CONNECTIONS; i++) {
            connections_[i][0] = pins[i * 2];
            connections_[i][1] = pins[i * 2 + 1];
        }

        Serial.println(F("[WIRES] Random connections generated:"));
        for (int i = 0; i < NUM_CONNECTIONS; i++) {
            Serial.printf("  %d: Pin %d <-> Pin %d\n",
                          i + 1, connections_[i][0], connections_[i][1]);
        }
    }

    // ========================================================================
    // VISUAL EFFECTS
    // ========================================================================
    void updateGauge(int count) {
        // Gradient colors
        const uint32_t colors[] = {
            Adafruit_NeoPixel::Color(0, 0, 0),       // Black
            Adafruit_NeoPixel::Color(255, 0, 0),     // Red
            Adafruit_NeoPixel::Color(255, 69, 0),    // Orange
            Adafruit_NeoPixel::Color(255, 165, 0),   // Light orange
            Adafruit_NeoPixel::Color(255, 255, 0),   // Yellow
            Adafruit_NeoPixel::Color(0, 255, 0)      // Green
        };

        // Clears the LEDs
        hw.setLightAll(0);

        // Fills the proportional bar
        int ledsPerConnection = 8 / NUM_CONNECTIONS;  // 2 LEDs per connection
        for (int i = 0; i < count * ledsPerConnection; i++) {
            uint8_t colorIndex = map(i, 0, 8, 0, 5);
            hw.setLight(i, colors[colorIndex]);
        }
    }

    // ========================================================================
    // PUZZLE STATES
    // ========================================================================
    void handleSolve() {
        Serial.println(F("[WIRES] Puzzle solved!"));

        // Starts the success visual effect (non-blocking)
        startSolveEffect();

        // Success sound (plays in parallel with the visual effect)
        hw.sound().playOnce("/success.mp3");

        setState(PuzzleState::Solved);
    }

    void startSolveEffect() {
        solveEffectActive_ = true;
        solveEffectStep_ = 0;
        solveEffectStartTime_ = millis();
        // First frame immediate
        hw.setLightAll(Adafruit_NeoPixel::Color(0, 255, 0));
    }

    void updateSolveEffect() {
        unsigned long elapsed = millis() - solveEffectStartTime_;

        // Sequence: GREEN -> off -> GREEN (with non-blocking timings)
        switch (solveEffectStep_) {
            case 0:
                if (elapsed >= 100) {
                    hw.setLightAll(0);  // Turns off
                    solveEffectStep_ = 1;
                }
                break;
            case 1:
                if (elapsed >= 200) {
                    hw.setLightAll(Adafruit_NeoPixel::Color(0, 255, 0));  // Turns on again
                    solveEffectStep_ = 2;
                }
                break;
            case 2:
                if (elapsed >= 500) {
                    // Keeps green longer, then finishes the effect
                    solveEffectActive_ = false;
                }
                break;
        }
    }

    void handleUnsolve() {
        Serial.println(F("[WIRES] Puzzle undone!"));

        // Locks again
        hw.digitalWrite(LOCK_PIN, HIGH);

        setState(PuzzleState::Running);
    }

    void handleManualRelease() {
        if (buttonPressed(BUTTON_UNLOCK_PIN)) {
            Serial.println(F("[WIRES] Manual lock release"));
            hw.digitalWrite(LOCK_PIN, LOW);  // Releases the lock
            hw.sound().playOnce("/lock_open.mp3");
        }
    }

    void resetPuzzle() {
        Serial.println(F("[WIRES] Resetting puzzle..."));

        // Resets the state
        activeConnections_ = 0;
        for (int i = 0; i < NUM_CONNECTIONS; i++) {
            lastState_[i] = false;
        }

        // Cancels the visual effect if it is running
        solveEffectActive_ = false;

        // Locks
        hw.digitalWrite(LOCK_PIN, HIGH);

        // Zeroes the voltmeter
        hw.setAnalogOutput(VOLTMETER_CHANNEL, 0);

        // Clears the LEDs
        hw.setLightAll(0);

        // Regenerates random connections
        randomizeConnections();

        setState(PuzzleState::Idle);
    }

    // ========================================================================
    // BUTTONS
    // ========================================================================
    boolean buttonPressed(int button) {
        int idx = button - BUTTON_RESET_PIN;
        if (idx < 0 || idx > 1) return false;

        if (btnPressed[idx]) {
            btnPressed[idx] = false;  // Clears the flag
            return true;
        }
        return false;
    }
};

// ============================================================================
// SwitchInput CALLBACKS (outside the class)
// ============================================================================
volatile bool btnPressed[2] = {false, false};

void onButtonPressed(pinid_t pin, bool held) {
    if (held) return;

    int idx = pin - BUTTON_RESET_PIN;
    if (idx >= 0 && idx < 2) {
        btnPressed[idx] = true;
        const char* btnName = (idx == 0) ? "RESET" : "UNLOCK";
        Serial.printf("[BTN] %s (P%d) PRESSED\n", btnName, pin);
    }
}

// ============================================================================
// MAIN
// ============================================================================
LogicManager master(11, true);  // ID 11, with the RS485 network enabled
ConnectWiresPuzzle puzzle;

void setup() {
    Serial.begin(115200);
    uint32_t start = millis();
    while (!Serial && (millis() - start < 3000)) delay(10);

    Serial.println(F("============================================"));
    Serial.println(F("  ConnectWires - PropMaker EscapeRoom"));
    Serial.println(F("April 2026 – Nicolau dos Brinquedos"));
    Serial.println(F("============================================"));

    hw.begin();
    hw.addPcf8575(0x20);  // Adds the PCF expander

    master.addPuzzle(&puzzle);
    master.begin();
    master.setNetworkEnabled(true);  // Enables RS485 communication

    Serial.println(F("[Main] System ready"));
    Serial.println(F("[Main] Waiting for a start command (network or RESET button)"));
}

void loop() {
    master.update();
    delay(1);
}