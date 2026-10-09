/**
 * Universal Unique RFID – A configurable universal RFID reader
 *
 * A configurable RFID puzzle that can be configured using JSON. It supports:
 * - UNIQUE: Qualquer tag programada resolve o puzzle
 * SEQUENCE: Tags must be read in a specific sequence.
 *
 * Hardware: Adafruit PropMaker Feather RP2040
 * - PN532 NFC/RFID via I2C
 * -NeoPixel for visual feedback
 * - Audio MP3 via I2S
 * RS485 optional for network integration
 *
 * Configuration file: /data/config.json
 */

#include <PropMakerEscapeRoom.h>
#include "ConfigManager.h"
#include "PuzzleEngine.h"

using namespace propmaker_escaperoom;
using namespace universal_unique_rfid;

// ============================================================================
// CONFIGURACOES
// ============================================================================

#define BUTTON_START_PIN 19  // PIN_BTN na PropMaker

// ============================================================================
// CLASSE PRINCIPAL DO PUZZLE
// ============================================================================

// Forward decl of serial command handlers
static bool cmdStart(const char*);
static bool cmdReset(const char*);
static bool cmdStatus(const char*);
static bool cmdSolve(const char*);
static bool cmdConfig(const char*);
static bool cmdTags(const char*);

class UniversalUniqueRFIDPuzzle : public PuzzleBase {
public:
    UniversalUniqueRFIDPuzzle();

    void setupHardware() override;
    void update() override;

public:
    ConfigManager configManager_;
    PuzzleEngine* engine_ = nullptr;
    SerialDebug dbg_;

private:
    bool configLoaded_ = false;
    unsigned long lastStatusPrint_ = 0;

public:
    void printStatus();
};

UniversalUniqueRFIDPuzzle::UniversalUniqueRFIDPuzzle()
    : PuzzleBase(2)  // ID 2
{
}

void UniversalUniqueRFIDPuzzle::setupHardware() {
    Serial.println(F("\n========================================"));
    Serial.println(F("   UNIVERSAL UNIQUE RFID v1.0"));
    Serial.println(F("   Escape Room RFID Configurable Puzzle"));
    Serial.println(F("========================================\n"));

    // Initializes LittleFS for configuration.
    if (!configManager_.begin()) {
        Serial.println("[Setup] ERRO: Falha ao inicializar LittleFS");
    }

    // Load configuration
    configLoaded_ = configManager_.loadConfig();
    if (!configLoaded_) {
        Serial.println("[Setup] Using the default settings");
        configManager_.loadDefaults();
    }

    // Log of the loaded configuration
    const auto& controller = configManager_.getControllerConfig();
    Serial.printf("[Setup] Nome: %s | ID: %d | Habilitado: %s\n",
                  controller.name, controller.id, controller.enabled ? "Sim" : "Nao");

    const auto& rfid = configManager_.getRfidConfig();
    Serial.printf("[Setup] Mode: %s | Tags: %d | Timeout: %ds | ResetOnError: %s\n",
                  ConfigManager::modeToString(rfid.mode),
                  rfid.tagCount,
                  rfid.timeoutSeconds,
                  rfid.resetOnError ? "Sim" : "Nao");

    // Initializes the puzzle engine
    engine_ = new PuzzleEngine(configManager_);
    engine_->begin();

    // Configura botao de start
    hw.pinMode(BUTTON_START_PIN, INPUT_PULLUP);

    // Configure the output lockout
    const auto& output = configManager_.getOutputConfig();
    hw.pinMode(output.lockPin, OUTPUT);
    hw.digitalWrite(output.lockPin, output.activeHigh ? LOW : HIGH);  // Activity activated

    dbg_.begin();
    dbg_.addCommand("start",  "         Inicia o puzzle",                   cmdStart);
    dbg_.addCommand("reset",  "         Reseta o puzzle",                   cmdReset);
    dbg_.addCommand("status", "Show current status",               cmdStatus);
    dbg_.addCommand("solve",  "The puzzle is now considered solved.",       cmdSolve);
    dbg_.addCommand("config", "It shows the loaded configuration.",     cmdConfig);
    dbg_.addCommand("tags",   "         Lista tags programadas",            cmdTags);
    dbg_.printHelp();

    setState(PuzzleState::Idle);
}

void UniversalUniqueRFIDPuzzle::update() {
    // Keep the audio active.
    hw.sound().loop();

    dbg_.update();

    // Verifica botao de start (se estiver em Idle)
    if (getState() == PuzzleState::Idle) {
        if (hw.digitalRead(BUTTON_START_PIN) == LOW) {
            delay(50);  // Debounce simples
            if (hw.digitalRead(BUTTON_START_PIN) == LOW) {
                Serial.println("[Puzzle] Botao START pressionado");
                run();  // Muda for Running
                if (engine_) {
                    engine_->start();
                }
            }
        }
    }

    // Atualiza engine do puzzle
    if (engine_ && getState() == PuzzleState::Running) {
        engine_->update();

        // Check whether the puzzle has been solved.
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

void UniversalUniqueRFIDPuzzle::printStatus() {
    if (!engine_) return;

    Serial.println("\n----- STATUS -----");
    Serial.printf("State: %s\n", PuzzleBase::stateName(getState()));
    Serial.printf("Progresso: %d%% (%d/%d)\n",
                  engine_->getProgress(),
                  engine_->getCurrentStep(),
                  engine_->getTotalSteps());

    const auto& rfid = configManager_.getRfidConfig();
    if (rfid.timeoutSeconds > 0) {
        Serial.printf("Remaining time: %ds\n", engine_->getRemainingTime());
    }
    Serial.println("------------------\n");
}

// ============================================================================
// MAIN
// ============================================================================

UniversalUniqueRFIDPuzzle puzzle;

// ============================================================================
// Serial Commands
// ============================================================================
static bool cmdStart(const char*) {
    if (puzzle.getState() == PuzzleState::Idle) {
        puzzle.run();
        if (puzzle.engine_) puzzle.engine_->start();
        Serial.println("[CMD] Puzzle started");
    } else {
        Serial.println("[CMD] Is the puzzle still running or has it been solved?");
    }
    return true;
}

static bool cmdReset(const char*) {
    if (puzzle.engine_) puzzle.engine_->reset();
    puzzle.setState(PuzzleState::Idle);
    Serial.println("[CMD] Puzzle resetado");
    return true;
}

static bool cmdStatus(const char*) {
    puzzle.printStatus();
    return true;
}

static bool cmdSolve(const char*) {
    puzzle.setState(PuzzleState::Solved);
    Serial.println("[CMD] The puzzle has been forced to be solved.");
    return true;
}

static bool cmdConfig(const char*) {
    const auto& controller = puzzle.configManager_.getControllerConfig();
    const auto& rfid = puzzle.configManager_.getRfidConfig();

    Serial.println("\n----- Configuration -----");
    Serial.printf("Nome: %s\n", controller.name);
    Serial.printf("ID: %d\n", controller.id);
    Serial.printf("Mode: %s\n", ConfigManager::modeToString(rfid.mode));
    Serial.printf("Tags programadas: %d\n", rfid.tagCount);
    for (uint8_t i = 0; i < rfid.tagCount; i++) {
        Serial.printf("  [%d] %s -> %s\n", i, rfid.tags[i].name, rfid.tags[i].uid);
    }
    if (rfid.mode == RfidMode::SEQUENCE) {
        Serial.print("Sequence:");
        for (uint8_t i = 0; i < rfid.sequenceLength; i++) {
            Serial.printf("%d ", rfid.sequence[i]);
        }
        Serial.println();
    }
    Serial.printf("Timeout: %ds\n", rfid.timeoutSeconds);
    Serial.printf("ResetOnError: %s\n", rfid.resetOnError ? "Sim" : "Nao");
    Serial.println("------------------------\n");
    return true;
}

static bool cmdTags(const char*) {
    const auto& rfid = puzzle.configManager_.getRfidConfig();
    Serial.printf("\nTags programadas (%d):\n", rfid.tagCount);
    for (uint8_t i = 0; i < rfid.tagCount; i++) {
        Serial.printf("  [%d] %s -> UID: %s\n", i, rfid.tags[i].name, rfid.tags[i].uid);
    }
    Serial.printf("\nTags validas conhecidas (%d):\n", rfid.validTagCount);
    for (uint8_t i = 0; i < rfid.validTagCount; i++) {
        bool isProgrammed = false;
        for (uint8_t j = 0; j < rfid.tagCount; j++) {
            if (strcmp(rfid.validTags[i].uid, rfid.tags[j].uid) == 0) {
                isProgrammed = true; break;
            }
        }
        Serial.printf("  [%d] %s -> UID: %s %s\n", i, rfid.validTags[i].name,
                      rfid.validTags[i].uid, isProgrammed ? "(programada)" : "");
    }
    Serial.println();
    return true;
}

void setup() {
    Serial.begin(115200);

    // Wait for the serial port for up to 3 seconds
    uint32_t start = millis();
    while (!Serial && (millis() - start < 3000)) delay(10);

    // Initializes hardware
    hw.begin();

    // Initializes the puzzle
    puzzle.begin();
}

void loop() {
    hw.sound().loop();
    puzzle.update();
    delay(1);
}
