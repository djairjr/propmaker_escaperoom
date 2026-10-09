/**
 * UNIVERSAL CONTROLLER – Multiple puzzles on one board
 *
 * Example with 2 instances of UniversalController, each with:
 * - Seu proprio PCF8575 (endereco I2C unico)
 * Your JSON configuration (mode, sequence, audio, color)
 * Your NeoPixels strip’s range
 * Your outlet pin (switch/button)
 *
 * Hardware: Adafruit PropMaker Feather RP2040
 * - PCF8575 #1 em 0x20 (Puzzle A)
 * - PCF8575 #2 em 0x21 (Puzzle B)
 * - NeoPixel strip compartilhado
 */

#include <PropMakerEscapeRoom.h>
#include "ConfigManager.h"
#include "PuzzleEngine.h"

using namespace propmaker_escaperoom;
using namespace universal_controller;

// ============================================================================
// CLASSE PUZZLE (reutiliza ConfigManager + PuzzleEngine)
// ============================================================================

class MultiControllerPuzzle : public PuzzleBase {
public:
    MultiControllerPuzzle(uint8_t id, const char* configPath,
                          uint8_t pcfAddress, uint8_t pixelOffset, uint8_t pixelCount)
        : PuzzleBase(id), configManager_(configPath),
          pcfAddress_(pcfAddress), pixelOffset_(pixelOffset), pixelCount_(pixelCount) {}

    void setupHardware() override;
    void update() override;

    PuzzleEngine* engine() { return engine_; }

private:
    ConfigManager configManager_;
    PuzzleEngine* engine_ = nullptr;

    uint8_t pcfAddress_;
    uint8_t pixelOffset_;
    uint8_t pixelCount_;

    bool configLoaded_ = false;
    unsigned long lastStatusPrint_ = 0;

    void printStatus();
};

void MultiControllerPuzzle::setupHardware() {
    Serial.printf("[Puzzle %d] Starting up (PCF=0x%02X, pixels=%d-%d)\n",
                  getId(), pcfAddress_, pixelOffset_, pixelOffset_ + pixelCount_ - 1);

    // Monta LittleFS (so na primeira chamada)
    static bool fsMounted = false;
    if (!fsMounted) {
        fsMounted = LittleFS.begin();
    }

    configLoaded_ = configManager_.loadConfig();
    if (!configLoaded_) {
        configManager_.loadDefaults();
    }

    const auto& inputs = configManager_.getInputConfig();

    // Configura PCF especifico deste puzzle
    hw.addPcf8575(pcfAddress_);
    if (hw.hasPortExpander()) {
        auto* io = hw.io();
        for (uint8_t i = 0; i < inputs.count; i++) {
            io->pinMode(inputs.pinMapping[i], INPUT_PULLUP);
        }
    }

    // Initializes the engine
    engine_ = new PuzzleEngine(configManager_);
    engine_->begin();

    // Configure the output.
    const auto& output = configManager_.getOutputConfig();
    hw.pinMode(output.lockPin, OUTPUT);
    hw.digitalWrite(output.lockPin, output.activeHigh ? LOW : HIGH);

    // Define the range of NeoPixel components in this puzzle.
    setPixelRange(pixelOffset_, pixelCount_);

    setState(PuzzleState::Idle);
}

void MultiControllerPuzzle::update() {
    hw.sound().loop();

    if (getState() == PuzzleState::Idle) {
        // Neste exemplo, iniciamos automaticamente no setup()
    }

    if (engine_ && getState() == PuzzleState::Running) {
        engine_->update();
        if (engine_->isSolved()) {
            setState(PuzzleState::Solved);
        }
    }

    if (getState() == PuzzleState::Running && millis() - lastStatusPrint_ > 5000) {
        printStatus();
        lastStatusPrint_ = millis();
    }
}

void MultiControllerPuzzle::printStatus() {
    if (!engine_) return;
    Serial.printf("[Puzzle %d] Progresso: %d%% (%d/%d)\n",
                  getId(), engine_->getProgress(),
                  engine_->getCurrentStep(), engine_->getTotalSteps());
}

// ============================================================================
// INSTANCIAS
// ============================================================================

// Puzzle A: In the LINEAR_SEQUENCE mode, PCF 0x20, LEDs 0-7
MultiControllerPuzzle puzzleA(1, "/config_puzzleA.json", 0x20, 0, 8);

// Puzzle B: In the “ALL_CONNECTED” mode, PCF 0x21, LEDs 8-15
MultiControllerPuzzle puzzleB(2, "/config_puzzleB.json", 0x21, 8, 8);

LogicManager master(0, false);

void setup() {
    Serial.begin(115200);

    uint32_t start = millis();
    while (!Serial && (millis() - start < 3000)) delay(10);

    hw.begin();

    master.addPuzzle(&puzzleA);
    master.addPuzzle(&puzzleB);
    master.begin();

    puzzleA.run();
    puzzleB.run();

    Serial.println(F("[MAIN] Puzzle A e B em execucao"));
}

void loop() {
    hw.sound().loop();
    puzzleA.update();
    puzzleB.update();
    delay(1);
}
