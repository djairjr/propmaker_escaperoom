/**
 * UNIVERSAL CONTROLLER - Universal Controller for Escape Room
 *
 * A JSON-configurable puzzle that supports multiple operating modes:
 * - CUSTOM_SEQUENCE: Custom sequence defined in JSON
 * - LINEAR_SEQUENCE: Linear order (0,1,2,3...)
 * - ALL_CONNECTED: All inputs activated, order does not matter
 * - RANDOM: Random sequence generated at boot
 *
 * Hardware: Adafruit PropMaker Feather RP2040
 * - PCF8575 for up to 16 digital inputs
 * - NeoPixel for visual feedback
 * - Gradual PWM output
 * - MP3 audio via I2S
 * - Optional RS485 for network integration
 *
 * Configuration file: /data/config.json
 */

#include <PropMakerEscapeRoom.h>
#include "ConfigManager.h"
#include "PuzzleEngine.h"

using namespace propmaker_escaperoom;
using namespace universal_controller;

// ============================================================================
// SETTINGS
// ============================================================================

// Reset/start button pin (using a board pin)
#define BUTTON_START_PIN 19  // PIN_BTN on the PropMaker

// ============================================================================
// MAIN PUZZLE CLASS
// ============================================================================

class UniversalControllerPuzzle : public PuzzleBase {
public:
    UniversalControllerPuzzle(uint8_t id = 1, const char* configPath = "/config.json");

    void setupHardware() override;
    void update() override;

private:
    ConfigManager configManager_;
    PuzzleEngine* engine_ = nullptr;

    // States
    bool configLoaded_ = false;
    unsigned long lastStatusPrint_ = 0;

    // Methods
    void printStatus();
    void handleSerialCommands();
    void printHelp();
};

UniversalControllerPuzzle::UniversalControllerPuzzle(uint8_t id, const char* configPath)
    : PuzzleBase(id), configManager_(configPath)
{
}

void UniversalControllerPuzzle::setupHardware() {
    Serial.println(F("\n========================================"));
    Serial.println(F("   UNIVERSAL CONTROLLER v1.0"));
    Serial.println(F("   Escape Room Configurable Puzzle"));
    Serial.println(F("========================================\n"));

    // Initializes LittleFS for configuration
    if (!configManager_.begin()) {
        Serial.println("[Setup] ERROR: Failed to initialize LittleFS");
    }

    // Loads configuration
    configLoaded_ = configManager_.loadConfig();
    if (!configLoaded_) {
        Serial.println("[Setup] Using default configuration");
        configManager_.loadDefaults();
    }

    // Log of the loaded configuration
    const auto& controller = configManager_.getControllerConfig();
    Serial.printf("[Setup] Name: %s | ID: %d | Enabled: %s\n",
                  controller.name, controller.id, controller.enabled ? "Yes" : "No");

    const auto& inputs = configManager_.getInputConfig();
    Serial.printf("[Setup] Inputs: %d | PCF Address: 0x%02X | Debounce: %dms\n",
                  inputs.count, inputs.pcfAddress, inputs.debounceMs);

    const auto& logic = configManager_.getLogicConfig();
    Serial.printf("[Setup] Mode: %s | Timeout: %ds | ResetOnError: %s\n",
                  ConfigManager::modeToString(logic.mode),
                  logic.timeoutSeconds,
                  logic.resetOnError ? "Yes" : "No");

    // Configures PCF8575
    if (inputs.pcfAddress >= 0x20 && inputs.pcfAddress <= 0x27) {
        hw.addPcf8575(inputs.pcfAddress);
        Serial.printf("[Setup] PCF8575 added at address 0x%02X\n", inputs.pcfAddress);

        // Configures pins as INPUT_PULLUP
        if (hw.hasPortExpander()) {
            auto* io = hw.io();
            for (uint8_t i = 0; i < inputs.count; i++) {
                io->pinMode(inputs.pinMapping[i], INPUT_PULLUP);
            }
            Serial.println("[Setup] Pins configured as INPUT_PULLUP");
        }
    }

    // Initializes puzzle engine
    engine_ = new PuzzleEngine(configManager_);
    engine_->begin();

    // Configures start button
    hw.pinMode(BUTTON_START_PIN, INPUT_PULLUP);

    // Configures lock output
    const auto& output = configManager_.getOutputConfig();
    hw.pinMode(output.lockPin, OUTPUT);
    hw.digitalWrite(output.lockPin, output.activeHigh ? LOW : HIGH);  // Lock engaged

    Serial.println("[Setup] Hardware configured. Waiting for start...");
    printHelp();

    setState(PuzzleState::Idle);
}

void UniversalControllerPuzzle::update() {
    // Keeps audio active
    hw.sound().loop();

    // Processes Serial commands
    handleSerialCommands();

    // Checks start button (if in Idle)
    if (getState() == PuzzleState::Idle) {
        if (hw.digitalRead(BUTTON_START_PIN) == LOW) {
            delay(50);  // Simple debounce
            if (hw.digitalRead(BUTTON_START_PIN) == LOW) {
                Serial.println("[Puzzle] START button pressed");
                run();  // Changes to Running
                if (engine_) {
                    engine_->start();
                }
            }
        }
    }

    // Updates puzzle engine
    if (engine_ && getState() == PuzzleState::Running) {
        engine_->update();

        // Checks whether the puzzle was solved
        if (engine_->isSolved()) {
            setState(PuzzleState::Solved);
        }
    }

    // Print status every 5 seconds (if running)
    if (getState() == PuzzleState::Running) {
        if (millis() - lastStatusPrint_ > 5000) {
            printStatus();
            lastStatusPrint_ = millis();
        }
    }
}

void UniversalControllerPuzzle::printStatus() {
    if (!engine_) return;

    Serial.println("\n----- STATUS -----");
    Serial.printf("State: %s\n", PuzzleBase::stateName(getState()));
    Serial.printf("Progress: %d%% (%d/%d)\n",
                  engine_->getProgress(),
                  engine_->getCurrentStep(),
                  engine_->getTotalSteps());

    const auto& logic = configManager_.getLogicConfig();
    if (logic.timeoutSeconds > 0) {
        Serial.printf("Remaining time: %ds\n", engine_->getRemainingTime());
    }
    Serial.println("------------------\n");
}

void UniversalControllerPuzzle::handleSerialCommands() {
    if (!Serial.available()) return;

    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toLowerCase();

    if (cmd == "start" || cmd == "run") {
        if (getState() == PuzzleState::Idle) {
            run();
            if (engine_) engine_->start();
            Serial.println("[CMD] Puzzle started");
        } else {
            Serial.println("[CMD] Puzzle is already running or was solved");
        }

    } else if (cmd == "reset") {
        if (engine_) {
            engine_->reset();
            Serial.println("[CMD] Puzzle reset");
        }
        setState(PuzzleState::Idle);

    } else if (cmd == "status") {
        printStatus();

    } else if (cmd == "solve" || cmd == "force") {
        if (engine_) {
            for (uint8_t i = engine_->getCurrentStep(); i < engine_->getTotalSteps(); i++) {
                engine_->update();  // Forces update
            }
        }
        setState(PuzzleState::Solved);
        Serial.println("[CMD] Puzzle forced as solved");

    } else if (cmd == "config") {
        const auto& controller = configManager_.getControllerConfig();
        const auto& inputs = configManager_.getInputConfig();
        const auto& logic = configManager_.getLogicConfig();

        Serial.println("\n----- CONFIGURATION -----");
        Serial.printf("Name: %s\n", controller.name);
        Serial.printf("ID: %d\n", controller.id);
        Serial.printf("Inputs: %d\n", inputs.count);
        Serial.printf("Mode: %s\n", ConfigManager::modeToString(logic.mode));
        Serial.printf("Timeout: %ds\n", logic.timeoutSeconds);
        Serial.println("------------------------\n");

    } else if (cmd == "help" || cmd == "?") {
        printHelp();

    } else if (cmd.length() > 0) {
        Serial.printf("[CMD] Unknown command: '%s'\n", cmd.c_str());
        Serial.println("Type 'help' to see available commands");
    }
}

void UniversalControllerPuzzle::printHelp() {
    Serial.println("\n----- SERIAL COMMANDS -----");
    Serial.println("start    - Starts the puzzle");
    Serial.println("reset    - Resets the puzzle");
    Serial.println("status   - Shows current status");
    Serial.println("solve    - Forces puzzle as solved");
    Serial.println("config   - Shows loaded configuration");
    Serial.println("help     - Shows this help");
    Serial.println("--------------------------\n");
}

// ============================================================================
// MAIN
// ============================================================================

UniversalControllerPuzzle puzzle;

void setup() {
    Serial.begin(115200);

    // Waits for Serial for up to 3 seconds
    uint32_t start = millis();
    while (!Serial && (millis() - start < 3000)) delay(10);

    // Initializes hardware
    hw.begin();

    // Initializes puzzle
    puzzle.begin();
}

void loop() {
    hw.sound().loop();
    puzzle.update();
    delay(1);
}
