#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

namespace universal_unique_rfid {

// RFID operating modes
enum class RfidMode {
    UNIQUE,    // Any programmed tag solves the puzzle
    SEQUENCE   // Tags must be read in a specific sequence
};

// Controller configuration
struct ControllerConfig {
    char name[32];
    uint8_t id;
    bool enabled;
};

// Programmed RFID tag
struct RfidTag {
    char uid[16];       // UID in hex (e.g. "A1B2C3D4")
    char name[24];      // Friendly name (e.g. "Red Tag")
};

// Valid RFID tag (detected but not necessarily in the puzzle)
struct ValidTag {
    char uid[16];
    char name[24];
};

// RFID configuration
struct RfidConfig {
    RfidMode mode;
    RfidTag tags[16];           // Programmed tags (used in the puzzle)
    uint8_t tagCount;           // How many tags were programmed
    ValidTag validTags[32];     // Known valid tags (detected)
    uint8_t validTagCount;      // How many known valid tags
    uint8_t sequence[16];       // Expected sequence (tag indices)
    uint8_t sequenceLength;     // Sequence length
    uint16_t timeoutSeconds;
    bool resetOnError;
};

// Audio configuration
struct AudioConfig {
    bool enabled;
    char tagSound[32];       // Sound when reading any tag
    char correctSound[32];   // Sound when tag is correct
    char wrongSound[32];     // Sound when tag is wrong
    char unlockSound[32];    // Sound when puzzle is solved
    char errorSound[32];
    char resetSound[32];
};

// NeoPixel colors configuration
struct NeoPixelConfig {
    bool enabled;
    uint8_t count;
    uint32_t colorIdle;
    uint32_t colorTagRead;
    uint32_t colorCorrect;
    uint32_t colorWrong;
    uint32_t colorSolved;
};

// RS485 configuration
struct Rs485Config {
    bool enabled;
    uint8_t address;
    uint32_t baudRate;
};

// Output configuration (lock/relay)
struct OutputConfig {
    uint8_t lockPin;
    bool activeHigh;
};

class ConfigManager {
public:
    ConfigManager();

    bool begin();
    bool loadConfig();
    bool saveConfig();
    bool loadDefaults();

    // Getters
    const ControllerConfig& getControllerConfig() const { return controller_; }
    const RfidConfig& getRfidConfig() const { return rfid_; }
    const AudioConfig& getAudioConfig() const { return audio_; }
    const NeoPixelConfig& getNeoPixelConfig() const { return neopixel_; }
    const Rs485Config& getRs485Config() const { return rs485_; }
    const OutputConfig& getOutputConfig() const { return output_; }

    // Helpers
    static RfidMode parseMode(const char* modeStr);
    static const char* modeToString(RfidMode mode);
    static uint32_t parseColor(const JsonArray& arr);

private:
    static constexpr const char* CONFIG_FILE = "/config.json";

    ControllerConfig controller_;
    RfidConfig rfid_;
    AudioConfig audio_;
    NeoPixelConfig neopixel_;
    Rs485Config rs485_;
    OutputConfig output_;

    bool parseControllerConfig(const JsonObject& obj);
    bool parseRfidConfig(const JsonObject& obj);
    bool parseAudioConfig(const JsonObject& obj);
    bool parseNeoPixelConfig(const JsonObject& obj);
    bool parseRs485Config(const JsonObject& obj);
    bool parseOutputConfig(const JsonObject& obj);
};

} // namespace universal_unique_rfid
