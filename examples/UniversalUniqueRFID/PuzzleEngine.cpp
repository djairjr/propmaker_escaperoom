#include "PuzzleEngine.h"

namespace universal_unique_rfid {

PuzzleEngine::PuzzleEngine(ConfigManager& config) : config_(config) {
}

void PuzzleEngine::begin() {
    const auto& rfid = config_.getRfidConfig();
    const auto& np = config_.getNeoPixelConfig();

    // Creates NFC reader
    reader_ = hw.createNfcReader();
    if (reader_ && reader_->isInitialized()) {
        Serial.println("[PuzzleEngine] NFC Reader initialized");
    } else {
        Serial.println("[PuzzleEngine] NFC Reader NOT initialized");
    }

    // Configures sequence
    if (rfid.mode == RfidMode::SEQUENCE && rfid.sequenceLength > 0) {
        totalSteps_ = rfid.sequenceLength;
        Serial.printf("[PuzzleEngine] Mode: SEQUENCE | Steps: %d\n", totalSteps_);
        Serial.print("[PuzzleEngine] Sequence: ");
        for (uint8_t i = 0; i < rfid.sequenceLength; i++) {
            Serial.printf("%d ", rfid.sequence[i]);
        }
        Serial.println();
    } else {
        totalSteps_ = 1; // UNIQUE: 1 step (any tag)
        Serial.printf("[PuzzleEngine] Mode: UNIQUE | Programmed tags: %d\n", rfid.tagCount);
    }

    // Log of the programmed tags
    Serial.println("[PuzzleEngine] Programmed tags:");
    for (uint8_t i = 0; i < rfid.tagCount; i++) {
        Serial.printf("  [%d] UID: %s | Name: %s\n", i, rfid.tags[i].uid, rfid.tags[i].name);
    }

    currentStep_ = 0;
    progress_ = 0;

    // Configures NeoPixel
    if (np.enabled && np.count > 0) {
        hw.pixelsExternal().updateLength(np.count);
        hw.pixelsExternal().begin();
        hw.pixelsExternal().setBrightness(128);
        hw.pixelsExternal().clear();
        hw.pixelsExternal().show();
    }

    state_ = EngineState::IDLE;
    showIdleEffect();
}

void PuzzleEngine::update() {
    const auto& rfid = config_.getRfidConfig();

    // Check timeout
    if (rfid.timeoutSeconds > 0 && state_ == EngineState::RUNNING) {
        if (millis() - puzzleStartTime_ > rfid.timeoutSeconds * 1000UL) {
            onTimeout();
            return;
        }
    }

    // NFC polling every 250ms
    if (millis() - lastPollTime_ < 250) return;
    lastPollTime_ = millis();

    switch (state_) {
        case EngineState::IDLE:
            break;

        case EngineState::RUNNING:
            if (!reader_ || !reader_->isInitialized()) break;

            if (rfid.mode == RfidMode::UNIQUE) {
                processUniqueMode();
            } else {
                processSequenceMode();
            }
            break;

        case EngineState::TAG_CORRECT:
            if (millis() - stateStartTime_ > 500) {
                state_ = EngineState::RUNNING;
                showRunningEffect();
            }
            break;

        case EngineState::TAG_WRONG:
            if (millis() - stateStartTime_ > 1000) {
                if (rfid.resetOnError) {
                    reset();
                    start();
                } else {
                    state_ = EngineState::RUNNING;
                    showRunningEffect();
                }
            }
            break;

        case EngineState::ERROR:
        case EngineState::TIMEOUT:
            break;

        case EngineState::SOLVED:
            showSolvedEffect();
            break;
    }
}

void PuzzleEngine::reset() {
    currentStep_ = 0;
    progress_ = 0;
    memset(lastUid_, 0, sizeof(lastUid_));
    lastUidLen_ = 0;
    state_ = EngineState::IDLE;
    showIdleEffect();
}

void PuzzleEngine::start() {
    puzzleStartTime_ = millis();
    state_ = EngineState::RUNNING;
    showRunningEffect();
    Serial.println("[PuzzleEngine] Puzzle started");
}

uint16_t PuzzleEngine::getRemainingTime() const {
    const auto& rfid = config_.getRfidConfig();
    if (rfid.timeoutSeconds == 0) return 0;

    unsigned long elapsed = (millis() - puzzleStartTime_) / 1000;
    if (elapsed >= rfid.timeoutSeconds) return 0;
    return rfid.timeoutSeconds - elapsed;
}

// ===== PER-MODE PROCESSING =====

void PuzzleEngine::processUniqueMode() {
    if (!readTag()) return;

    const auto& rfid = config_.getRfidConfig();
    if (rfid.tagCount == 0) return;

    // Checks whether the read UID matches any programmed tag
    for (uint8_t i = 0; i < rfid.tagCount; i++) {
        uint8_t tagBytes[7];
        uint8_t tagLen = 0;
        hexToBytes(rfid.tags[i].uid, tagBytes, tagLen);

        if (tagLen == lastUidLen_ && memcmp(lastUid_, tagBytes, tagLen) == 0) {
            onTagCorrect(i);
            return;
        }
    }

    // No tag matched
    onTagWrong();
}

void PuzzleEngine::processSequenceMode() {
    if (!readTag()) return;

    const auto& rfid = config_.getRfidConfig();
    if (rfid.tagCount == 0 || rfid.sequenceLength == 0) return;

    // Finds the index of the read tag
    int tagIndex = findTagIndex(lastUid_, lastUidLen_);
    if (tagIndex < 0) {
        onTagWrong();
        return;
    }

    // Checks whether the tag matches the expected step
    uint8_t expectedIndex = rfid.sequence[currentStep_];
    if ((uint8_t)tagIndex == expectedIndex) {
        onTagCorrect(tagIndex);
    } else {
        onTagWrong();
    }
}

// ===== HELPERS =====

bool PuzzleEngine::readTag() {
    if (!reader_) return false;

    // Reads the current tag (100ms timeout to avoid blocking)
    auto card = reader_->readCard(100);

    if (!card.present) {
        lastUidLen_ = 0;
        return false;
    }

    // Checks whether it is the same tag as the previous cycle
    if (card.uidLength == lastUidLen_ && memcmp(card.uid, lastUid_, lastUidLen_) == 0) {
        return false; // Same tag still present
    }

    // New tag detected
    lastUidLen_ = card.uidLength;
    memcpy(lastUid_, card.uid, lastUidLen_);

    char uidHex[15];
    bytesToHex(card.uid, card.uidLength, uidHex);
    Serial.printf("[PuzzleEngine] Tag detected: UID=%s (%d bytes)\n", uidHex, card.uidLength);

    playTagSound();
    return true;
}

void PuzzleEngine::hexToBytes(const char* hex, uint8_t* bytes, uint8_t& len) {
    len = 0;
    size_t hexLen = strlen(hex);
    for (size_t i = 0; i < hexLen && len < 7; i += 2) {
        char byteStr[3] = {hex[i], hex[i+1], '\0'};
        bytes[len++] = strtoul(byteStr, NULL, 16);
    }
}

void PuzzleEngine::bytesToHex(const uint8_t* bytes, uint8_t len, char* hex) {
    for (uint8_t i = 0; i < len; i++) {
        sprintf(hex + (i * 2), "%02X", bytes[i]);
    }
    hex[len * 2] = '\0';
}

int PuzzleEngine::findTagIndex(const uint8_t* uid, uint8_t uidLen) {
    const auto& rfid = config_.getRfidConfig();

    for (uint8_t i = 0; i < rfid.tagCount; i++) {
        uint8_t tagBytes[7];
        uint8_t tagLen = 0;
        hexToBytes(rfid.tags[i].uid, tagBytes, tagLen);

        if (tagLen == uidLen && memcmp(uid, tagBytes, tagLen) == 0) {
            return i;
        }
    }

    return -1;
}

void PuzzleEngine::onTagCorrect(uint8_t tagIndex) {
    stateStartTime_ = millis();
    state_ = EngineState::TAG_CORRECT;

    const auto& rfid = config_.getRfidConfig();
    const auto& tags = config_.getRfidConfig().tags;

    if (rfid.mode == RfidMode::SEQUENCE) {
        currentStep_++;
        updateProgress();
    } else {
        // UNIQUE: any correct tag = solved
        currentStep_ = 1;
        progress_ = 100;
    }

    playCorrectSound();
    showCorrectEffect();

    Serial.printf("[PuzzleEngine] CORRECT Tag: %s (%s)\n",
                  tags[tagIndex].name, tags[tagIndex].uid);

    if (currentStep_ >= totalSteps_) {
        onSequenceComplete();
    }
}

void PuzzleEngine::onTagWrong() {
    stateStartTime_ = millis();
    state_ = EngineState::TAG_WRONG;

    playWrongSound();
    showWrongEffect();

    Serial.println("[PuzzleEngine] WRONG Tag (not programmed)");
}

void PuzzleEngine::onSequenceComplete() {
    state_ = EngineState::SOLVED;
    progress_ = 100;

    playUnlockSound();
    showSolvedEffect();

    // Activates output (lock/relay)
    const auto& output = config_.getOutputConfig();
    hw.pinMode(output.lockPin, OUTPUT);
    hw.digitalWrite(output.lockPin, output.activeHigh ? HIGH : LOW);

    Serial.println("[PuzzleEngine] PUZZLE SOLVED!");
}

void PuzzleEngine::onTimeout() {
    state_ = EngineState::TIMEOUT;
    Serial.println("[PuzzleEngine] TIMEOUT!");
}

void PuzzleEngine::updateProgress() {
    if (totalSteps_ == 0) {
        progress_ = 0;
    } else {
        progress_ = (currentStep_ * 100) / totalSteps_;
    }
}

// ===== AUDIO =====

void PuzzleEngine::playTagSound() {
    const auto& audio = config_.getAudioConfig();
    if (!audio.enabled) return;
    hw.sound().playOnce(audio.tagSound);
}

void PuzzleEngine::playCorrectSound() {
    const auto& audio = config_.getAudioConfig();
    if (!audio.enabled) return;
    hw.sound().playOnce(audio.correctSound);
}

void PuzzleEngine::playWrongSound() {
    const auto& audio = config_.getAudioConfig();
    if (!audio.enabled) return;
    hw.sound().playOnce(audio.wrongSound);
}

void PuzzleEngine::playUnlockSound() {
    const auto& audio = config_.getAudioConfig();
    if (!audio.enabled) return;
    hw.sound().playOnce(audio.unlockSound);
}

void PuzzleEngine::playErrorSound() {
    const auto& audio = config_.getAudioConfig();
    if (!audio.enabled) return;
    hw.sound().playOnce(audio.errorSound);
}

void PuzzleEngine::playResetSound() {
    const auto& audio = config_.getAudioConfig();
    if (!audio.enabled) return;
    hw.sound().playOnce(audio.resetSound);
}

// ===== VISUAL EFFECTS =====

void PuzzleEngine::showIdleEffect() {
    const auto& np = config_.getNeoPixelConfig();
    if (!np.enabled || np.count == 0) return;

    for (uint8_t i = 0; i < np.count; i++) {
        hw.pixelsExternal().setPixelColor(i, np.colorIdle);
    }
    hw.pixelsExternal().show();
}

void PuzzleEngine::showRunningEffect() {
    const auto& np = config_.getNeoPixelConfig();
    if (!np.enabled || np.count == 0) return;

    for (uint8_t i = 0; i < np.count; i++) {
        hw.pixelsExternal().setPixelColor(i, np.colorIdle);
    }
    hw.pixelsExternal().show();
}

void PuzzleEngine::showCorrectEffect() {
    const auto& np = config_.getNeoPixelConfig();
    if (!np.enabled || np.count == 0) return;

    // Green flash
    for (uint8_t i = 0; i < np.count; i++) {
        hw.pixelsExternal().setPixelColor(i, np.colorCorrect);
    }
    hw.pixelsExternal().show();
}

void PuzzleEngine::showWrongEffect() {
    const auto& np = config_.getNeoPixelConfig();
    if (!np.enabled || np.count == 0) return;

    // Red flash
    for (uint8_t i = 0; i < np.count; i++) {
        hw.pixelsExternal().setPixelColor(i, np.colorWrong);
    }
    hw.pixelsExternal().show();
}

void PuzzleEngine::showSolvedEffect() {
    const auto& np = config_.getNeoPixelConfig();
    if (!np.enabled || np.count == 0) return;

    // Green fade in/out
    static unsigned long lastUpdate = 0;
    static uint8_t brightness = 0;
    static bool increasing = true;

    if (millis() - lastUpdate > 20) {
        lastUpdate = millis();

        if (increasing) {
            brightness += 5;
            if (brightness >= 255) { brightness = 255; increasing = false; }
        } else {
            brightness -= 5;
            if (brightness <= 100) { brightness = 100; increasing = true; }
        }

        uint32_t color = Adafruit_NeoPixel::Color(
            ((np.colorSolved >> 16) & 0xFF) * brightness / 255,
            ((np.colorSolved >> 8) & 0xFF) * brightness / 255,
            (np.colorSolved & 0xFF) * brightness / 255);

        for (uint8_t i = 0; i < np.count; i++) {
            hw.pixelsExternal().setPixelColor(i, color);
        }
        hw.pixelsExternal().show();
    }
}

} // namespace universal_unique_rfid
