#pragma once

#include <Arduino.h>
#include <PropMakerEscapeRoom.h>
#include "ConfigManager.h"

using namespace propmaker_escaperoom;

namespace universal_unique_rfid {

enum class EngineState {
    IDLE,
    RUNNING,
    TAG_CORRECT,
    TAG_WRONG,
    SOLVED,
    ERROR,
    TIMEOUT
};

class PuzzleEngine {
public:
    PuzzleEngine(ConfigManager& config);

    void begin();
    void update();
    void reset();
    void start();

    EngineState getState() const { return state_; }
    uint8_t getProgress() const { return progress_; }
    uint8_t getCurrentStep() const { return currentStep_; }
    uint8_t getTotalSteps() const { return totalSteps_; }
    uint16_t getRemainingTime() const;

    bool isSolved() const { return state_ == EngineState::SOLVED; }
    bool hasError() const { return state_ == EngineState::ERROR; }

    // Access to the reader for external configuration
    NfcReader* getReader() { return reader_; }

private:
    ConfigManager& config_;
    EngineState state_ = EngineState::IDLE;

    // NfcReader
    NfcReader* reader_ = nullptr;

    // State of the sequence
    uint8_t currentStep_ = 0;
    uint8_t totalSteps_ = 0;
    uint8_t progress_ = 0;

    // Timer
    unsigned long puzzleStartTime_ = 0;
    unsigned long stateStartTime_ = 0;
    unsigned long lastPollTime_ = 0;

    // UID da ultima tag lida (evita repeticao)
    uint8_t lastUid_[7] = {0};
    uint8_t lastUidLen_ = 0;

    // Processing methods by mode
    void processUniqueMode();
    void processSequenceMode();

    // Helpers
    bool readTag();
    bool matchTag(const uint8_t* uid, uint8_t uidLen, const char* uidHex);
    void hexToBytes(const char* hex, uint8_t* bytes, uint8_t& len);
    void bytesToHex(const uint8_t* bytes, uint8_t len, char* hex);
    int findTagIndex(const uint8_t* uid, uint8_t uidLen);

    void onTagCorrect(uint8_t tagIndex);
    void onTagWrong();
    void onSequenceComplete();
    void onTimeout();

    void updateProgress();
    void playTagSound();
    void playCorrectSound();
    void playWrongSound();
    void playUnlockSound();
    void playErrorSound();
    void playResetSound();

    // Visualizacao
    void showIdleEffect();
    void showRunningEffect();
    void showCorrectEffect();
    void showWrongEffect();
    void showSolvedEffect();
};

} // namespace universal_unique_rfid
