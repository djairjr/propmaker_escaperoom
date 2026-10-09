/**
 * EXAMPLE: KEYPAD RFID LOCK - LOCK WITH PASSWORD + RFID
 * --------------------------------------------------------
 * Combines a 4x3 keypad (I2C/PCF8575) + PN532 reader (I2C) to
 * release a lock. Configuration via /lock.json in LittleFS.
 *
 * Hardware:
 *   - 4x3 keypad via PCF8575 (IOAbstraction KeyboardManager)
 *   - PN532 NFC Reader via I2C (hw.createNfcReader)
 *   - NeoPixel for visual feedback
 *   - MP3 audio via I2S
 *   - Relay/Lock on the configured pin (lockPin)
 *
 * Authentication modes (defined in the JSON):
 *   "mode": "KEYPAD_ONLY"   - password only
 *   "mode": "RFID_ONLY"     - RFID tag only
 *   "mode": "KEYPAD_OR_RFID" - either one (default)
 *   "mode": "KEYPAD_AND_RFID" - both required
 */

#include <PropMakerEscapeRoom.h>
#include <IoAbstractionWire.h>
#include <KeyboardManager.h>
#include <TaskManager.h>

using namespace propmaker_escaperoom;

// ============================================================================
// AUTHENTICATION MODES
// ============================================================================
enum class AuthMode : uint8_t {
    KEYPAD_ONLY = 0,
    RFID_ONLY = 1,
    KEYPAD_OR_RFID = 2,
    KEYPAD_AND_RFID = 3
};

// ============================================================================
// JSON CONFIGURATION
// ============================================================================
// RFID tag stored as binary bytes
struct RfidTagBin {
    uint8_t bytes[7];
    uint8_t len;
    bool valid;
};

struct LockConfig {
    AuthMode mode = AuthMode::KEYPAD_OR_RFID;
    uint8_t lockPin = 10;
    uint16_t unlockDurationMs = 3000;

    // Access codes (up to 4)
    char codes[4][5] = {"1234", "", "", ""};
    uint8_t codeCount = 1;

    // RFID tags in binary (up to 4, up to 7 bytes each)
    RfidTagBin rfidTags[4] = {{{0}, 0, false}, {{0}, 0, false}, {{0}, 0, false}, {{0}, 0, false}};
    uint8_t rfidTagCount = 0;

    // Audio
    char sfxKeypress[24] = "/sfx/beep.mp3";
    char sfxUnlock[24] = "/sfx/magic.mp3";
    char sfxDenied[24] = "/sfx/error.mp3";
    char sfxReset[24] = "/sfx/cancel.mp3";

    // NeoPixel
    uint8_t neoCount = 8;
    uint32_t colorIdle = 0x050514;
    uint32_t colorUnlock = 0x003200;
    uint32_t colorDenied = 0x320000;
    uint32_t colorRfidScan = 0x000032;
} config;

// Converts a hex string ("04:2A:6B:01") to binary bytes
static uint8_t hexStringToBytes(const char* hexStr, uint8_t* bytes, uint8_t maxBytes) {
    uint8_t count = 0;
    const char* p = hexStr;
    while (*p && count < maxBytes) {
        // Skips separators (:, space, -)
        while (*p && !isxdigit(*p)) p++;
        if (!*p) break;
        // Reads 2 hex digits
        unsigned int b;
        if (sscanf(p, "%2x", &b) != 1) break;
        bytes[count++] = (uint8_t)b;
        p += 2;
    }
    return count;
}

static AuthMode parseAuthMode(const char* s) {
    if (!s) return AuthMode::KEYPAD_OR_RFID;
    if (!strcmp(s, "KEYPAD_ONLY"))     return AuthMode::KEYPAD_ONLY;
    if (!strcmp(s, "RFID_ONLY"))       return AuthMode::RFID_ONLY;
    if (!strcmp(s, "KEYPAD_AND_RFID")) return AuthMode::KEYPAD_AND_RFID;
    return AuthMode::KEYPAD_OR_RFID;  // default
}

static void loadConfig() {
    if (!LittleFS.exists("/lock.json")) {
        Serial.println(F("[CONFIG] /lock.json does not exist, using default"));
        return;
    }
    File f = LittleFS.open("/lock.json", "r");
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        Serial.printf("[CONFIG] JSON Error: %s\n", err.c_str());
        return;
    }

    config.mode = parseAuthMode(doc["mode"] | "");
    config.lockPin = doc["lockPin"] | config.lockPin;
    config.unlockDurationMs = doc["unlockDurationMs"] | config.unlockDurationMs;

    // Codes
    if (doc["codes"].is<JsonArray>()) {
        auto arr = doc["codes"].as<JsonArray>();
        config.codeCount = min((uint8_t)arr.size(), (uint8_t)4);
        for (uint8_t i = 0; i < config.codeCount; i++) {
            strncpy(config.codes[i], arr[i] | "", sizeof(config.codes[i]) - 1);
            config.codes[i][sizeof(config.codes[i]) - 1] = '\0';
        }
    }

    // RFID tags (hex string, e.g.: "04:2A:6B:01") → binary
    if (doc["rfidTags"].is<JsonArray>()) {
        auto arr = doc["rfidTags"].as<JsonArray>();
        config.rfidTagCount = min((uint8_t)arr.size(), (uint8_t)4);
        for (uint8_t i = 0; i < config.rfidTagCount; i++) {
            const char* hexStr = arr[i] | "";
            RfidTagBin& tag = config.rfidTags[i];
            tag.len = hexStringToBytes(hexStr, tag.bytes, 7);
            tag.valid = (tag.len > 0);
            if (tag.valid) {
                Serial.printf("[CONFIG] Tag %d: %d bytes\n", i, tag.len);
            }
        }
    }

    // Audio
    strncpy(config.sfxKeypress, doc["audio"]["keypress"] | config.sfxKeypress, sizeof(config.sfxKeypress));
    strncpy(config.sfxUnlock,   doc["audio"]["unlock"]   | config.sfxUnlock,   sizeof(config.sfxUnlock));
    strncpy(config.sfxDenied,   doc["audio"]["denied"]   | config.sfxDenied,   sizeof(config.sfxDenied));
    strncpy(config.sfxReset,    doc["audio"]["reset"]    | config.sfxReset,    sizeof(config.sfxReset));

    // NeoPixel
    config.neoCount = doc["neopixel"]["count"] | config.neoCount;
    if (doc["neopixel"]["colorIdle"].is<JsonArray>()) {
        auto a = doc["neopixel"]["colorIdle"].as<JsonArray>();
        config.colorIdle = hw.pixelsExternal().Color(a[0] | 5, a[1] | 5, a[2] | 20);
    }
    if (doc["neopixel"]["colorUnlock"].is<JsonArray>()) {
        auto a = doc["neopixel"]["colorUnlock"].as<JsonArray>();
        config.colorUnlock = hw.pixelsExternal().Color(a[0] | 0, a[1] | 50, a[2] | 0);
    }
    if (doc["neopixel"]["colorDenied"].is<JsonArray>()) {
        auto a = doc["neopixel"]["colorDenied"].as<JsonArray>();
        config.colorDenied = hw.pixelsExternal().Color(a[0] | 50, a[1] | 0, a[2] | 0);
    }
    if (doc["neopixel"]["colorRfidScan"].is<JsonArray>()) {
        auto a = doc["neopixel"]["colorRfidScan"].as<JsonArray>();
        config.colorRfidScan = hw.pixelsExternal().Color(a[0] | 0, a[1] | 0, a[2] | 50);
    }

    Serial.printf("[CONFIG] Mode=%d, codes=%d, rfidTags=%d, lockPin=%d\n",
                  (int)config.mode, config.codeCount, config.rfidTagCount, config.lockPin);
}

// ============================================================================
// HARDWARE - KEYPAD
// ============================================================================
#define PCF_ADDRESS 0x20
#define PCF_IS_8575 true

static const char keymap[] PROGMEM = "123456789*0#";
byte rowPins[4] = {3, 2, 1, 0};
byte colPins[3] = {6, 5, 4};

KeyboardLayout keypadLayout(4, 3, keymap);

class KeypadListener : public KeyboardListener {
public:
    char lastKey = 0;
    void keyPressed(char key, bool held) override { if (!held) lastKey = key; }
    void keyReleased(char key) override { lastKey = 0; }
};
KeypadListener keypadListener;
MatrixKeyboardManager keypadManager;

// ============================================================================
// DISPLAY WRAPPER
// ============================================================================
class DisplayWrapper {
public:
  bool checkDisplay() { return hw.hasDisplay(); }

  void printNumber(int num) {
    if (checkDisplay()) hw.display().printNum(num);
    else Serial.printf("[DISPLAY] Number: %d\n", num);
  }

  void printString(const char* str) {
    if (checkDisplay()) {
      hw.display().clear();
      hw.display().printString(str);
    } else Serial.printf("[DISPLAY] Text: %s\n", str);
  }

  void writeDigitAscii(int digit, char c) {
    if (checkDisplay()) hw.display().writeDigitAscii(digit, c);
    else Serial.printf("[DISPLAY] Digit %d: %c\n", digit, c);
  }

  void setBrightness(uint8_t b) {
    if (checkDisplay()) hw.display().setBrightness(b);
    else Serial.printf("[DISPLAY] Brightness: %d\n", b);
  }
};
DisplayWrapper display;

// ============================================================================
// PUZZLE
// ============================================================================
String enteredCode = "";
bool rfidAuthenticated = false;
unsigned long unlockUntil = 0;
bool lockOpen = false;

class KeypadRfidLock : public PuzzleBase {
public:
    KeypadRfidLock() : PuzzleBase(13), nfc_(nullptr) {}

    void setupHardware() override {
        loadConfig();

        hw.pinMode(config.lockPin, OUTPUT);
        hw.digitalWrite(config.lockPin, HIGH);

        display.setBrightness(15);
        if (config.mode == AuthMode::RFID_ONLY) {
            display.printString("rFId");
        } else {
            display.printString("____");
        }

        dbg_.begin();
        dbg_.addCommand("mode",   "<mode>    KEYPAD_ONLY|RFID_ONLY|OR|AND", cmdMode);
        dbg_.addCommand("code",   "list|add|clear <s>  Manages codes",   cmdCode);
        dbg_.addCommand("tag",    "list|add|clear <h>  Manages tags",      cmdTag);
        dbg_.addCommand("status", "          Shows configuration",           cmdStatus);
        dbg_.addCommand("reset",  "          Resets the puzzle",               cmdReset);
        dbg_.printHelp();

        // Initializes NFC Reader
        nfc_ = hw.createNfcReader();
        if (nfc_) {
            Serial.println(F("[RFID] PN532 ready"));
        } else {
            Serial.println(F("[RFID] PN532 not detected"));
        }

        setVisualIdle();
        Serial.printf("[LOCK] Mode=%d, waiting for authentication\n", (int)config.mode);
    }

    void update() override {
        if (state_ != PuzzleState::Running) return;

        dbg_.update();
        taskManager.runLoop();

        // Manages the opening time
        if (lockOpen && millis() >= unlockUntil) {
            lockOpen = false;
            hw.digitalWrite(config.lockPin, HIGH);
            Serial.println(F("[LOCK] Lock locked"));
            if (config.mode == AuthMode::RFID_ONLY) {
                display.printString("rFId");
            } else {
                display.printString("____");
            }
            setVisualIdle();
        }

        // RFID reading (non-blocking)
        if (nfc_ && config.mode != AuthMode::KEYPAD_ONLY) {
            pollRfid();
        }

        // Keypad reading
        char key = keypadListener.lastKey;
        if (key != 0) {
            keypadListener.lastKey = 0;
            handleKey(key);
        }
    }

private:
    NfcReader* nfc_;
    unsigned long lastRfidPoll_ = 0;
    uint8_t lastUid_[7] = {0};
    uint8_t lastUidLen_ = 0;

    void pollRfid() {
        if (millis() - lastRfidPoll_ < 300) return;
        lastRfidPoll_ = millis();

        NfcReader::CardData card = nfc_->readCard(200);
        if (card.present) {
            // Only processes if the UID differs from the last one
            if (card.uidLength != lastUidLen_ || memcmp(card.uid, lastUid_, lastUidLen_) != 0) {
                memcpy(lastUid_, card.uid, card.uidLength);
                lastUidLen_ = card.uidLength;
                handleRfid(card.uid, card.uidLength);
            }
        } else {
            lastUidLen_ = 0;
        }
    }

    void handleRfid(uint8_t* uid, uint8_t len) {
        // Debug: shows UID in hex
        Serial.print(F("[RFID] TAG: "));
        for (uint8_t i = 0; i < len; i++) {
            Serial.printf("%02X%s", uid[i], (i < len - 1) ? ":" : "");
        }
        Serial.println();

        // Checks whether the tag is in the list (binary comparison)
        for (uint8_t i = 0; i < config.rfidTagCount; i++) {
            RfidTagBin& tag = config.rfidTags[i];
            if (tag.valid && tag.len == len && memcmp(tag.bytes, uid, len) == 0) {
                Serial.println(F("[RFID] TAG authorized!"));
                onRfidAuthorized();
                return;
            }
        }
        Serial.println(F("[RFID] TAG unknown"));
    }

    void onRfidAuthorized() {
        if (config.mode == AuthMode::KEYPAD_AND_RFID) {
            rfidAuthenticated = true;
            Serial.println(F("[RFID] Step 1/2 complete, waiting for password"));
            hw.sound().playOnce(config.sfxKeypress);
            display.printString("CodE");
            setVisualRfidScan();
        } else {
            unlockNow();
        }
    }

    void handleKey(char key) {
        Serial.printf("[KEYPAD] Key: %c\n", key);

        if (key == '*') {
            enteredCode = "";
            rfidAuthenticated = false;
            hw.sound().playOnce(config.sfxReset);
            display.printString("____");
            setVisualIdle();
        }
        else if (key == '#') {
            checkCode();
        }
        else {
            enteredCode += key;
            hw.sound().playOnce(config.sfxKeypress);
            display.writeDigitAscii(enteredCode.length() - 1, key);
            setVisualProgress(enteredCode.length());

            if (enteredCode.length() >= 4) {
                checkCode();
            }
        }
    }

    void checkCode() {
        if (config.mode == AuthMode::RFID_ONLY) {
            Serial.println(F("[KEYPAD] RFID-only mode, keypad ignored"));
            enteredCode = "";
            return;
        }

        // Checks whether the code is in the list
        bool valid = false;
        for (uint8_t i = 0; i < config.codeCount; i++) {
            if (enteredCode == config.codes[i]) {
                valid = true;
                break;
            }
        }

        if (valid) {
            if (config.mode == AuthMode::KEYPAD_AND_RFID) {
                if (rfidAuthenticated) {
                    unlockNow();
                } else {
                    Serial.println(F("[KEYPAD] Code ok! Bring the RFID tag close"));
                    hw.sound().playOnce(config.sfxKeypress);
                    display.printString("rFId");
                    setVisualRfidScan();
                }
            } else {
                unlockNow();
            }
        } else {
            handleDenied();
        }
        enteredCode = "";
    }

    void unlockNow() {
        if (lockOpen) return;
        lockOpen = true;
        unlockUntil = millis() + config.unlockDurationMs;
        hw.digitalWrite(config.lockPin, LOW);
        hw.sound().playOnce(config.sfxUnlock);
        display.printString("OPEn");
        hw.setLightAll(config.colorUnlock);
        Serial.printf("[LOCK] OPEN for %d ms\n", config.unlockDurationMs);
    }

    void handleDenied() {
        Serial.println(F("[LOCK] ACCESS DENIED"));
        hw.sound().playOnce(config.sfxDenied);
        display.printString("no  ");
        hw.setLightAll(config.colorDenied);
        delay(800);
        display.printString("____");
        setVisualIdle();
    }

    void setVisualIdle() {
        rfidAuthenticated = false;
        hw.setLightAll(0);
        hw.setLight(0, config.colorIdle);
    }

    void setVisualProgress(uint8_t step) {
        uint32_t c = hw.pixelsExternal().Color(0, 0, 50);
        for (uint8_t i = 0; i < step && i < config.neoCount; i++) {
            hw.setLight(i, c);
        }
    }

    void setVisualRfidScan() {
        for (uint8_t i = 0; i < config.neoCount; i++) {
            hw.setLight(i, config.colorRfidScan);
        }
    }

    SerialDebug dbg_;
};

// ============================================================================
// MAIN
// ============================================================================
LogicManager master(13);
KeypadRfidLock puzzle;

// ============================================================================
// SERIAL COMMANDS
// ============================================================================
static bool cmdMode(const char* arg) {
    if (!arg) return false;
    config.mode = parseAuthMode(arg);
    Serial.printf("[CMD] Mode changed to: %d\n", (int)config.mode);
    return true;
}

static bool cmdCode(const char* arg) {
    if (!arg) return false;
    if (strcmp(arg, "list") == 0) {
        Serial.printf("Codes (%d):\n", config.codeCount);
        for (uint8_t i = 0; i < config.codeCount; i++) {
            Serial.printf("  [%d] %s\n", i, config.codes[i]);
        }
        return true;
    }
    if (strcmp(arg, "clear") == 0) {
        config.codeCount = 0;
        memset(config.codes, 0, sizeof(config.codes));
        Serial.println(F("[CMD] All codes removed"));
        return true;
    }
    if (strncmp(arg, "add ", 4) == 0) {
        const char* newCode = arg + 4;
        if (strlen(newCode) != 4) return false;
        if (config.codeCount >= 4) {
            Serial.println(F("[CMD] 4-code limit reached"));
            return true;
        }
        strncpy(config.codes[config.codeCount], newCode, 4);
        config.codes[config.codeCount][4] = '\0';
        config.codeCount++;
        Serial.printf("[CMD] Code added: %s\n", newCode);
        return true;
    }
    return false;
}

static bool cmdTag(const char* arg) {
    if (!arg) return false;
    if (strcmp(arg, "list") == 0) {
        Serial.printf("Tags (%d):\n", config.rfidTagCount);
        for (uint8_t i = 0; i < config.rfidTagCount; i++) {
            auto& tag = config.rfidTags[i];
            Serial.printf("  [%d] %d bytes: ", i, tag.len);
            for (uint8_t b = 0; b < tag.len; b++) {
                Serial.printf("%02X%s", tag.bytes[b], b < tag.len - 1 ? ":" : "");
            }
            Serial.println();
        }
        return true;
    }
    if (strcmp(arg, "clear") == 0) {
        config.rfidTagCount = 0;
        memset(config.rfidTags, 0, sizeof(config.rfidTags));
        Serial.println(F("[CMD] All tags removed"));
        return true;
    }
    if (strncmp(arg, "add ", 4) == 0) {
        const char* hexStr = arg + 4;
        if (config.rfidTagCount >= 4) {
            Serial.println(F("[CMD] 4-tag limit reached"));
            return true;
        }
        RfidTagBin& tag = config.rfidTags[config.rfidTagCount];
        tag.len = hexStringToBytes(hexStr, tag.bytes, 7);
        if (tag.len == 0) return false;
        tag.valid = true;
        config.rfidTagCount++;
        Serial.printf("[CMD] Tag added\n");
        return true;
    }
    return false;
}

static bool cmdStatus(const char*) {
    Serial.printf("Mode=%d codes=%d tags=%d lockPin=%d state=%s\n",
                  (int)config.mode, config.codeCount, config.rfidTagCount,
                  config.lockPin, PuzzleBase::stateName(puzzle.getState()));
    return true;
}

static bool cmdReset(const char*) {
    puzzle.setState(PuzzleState::Running);
    Serial.println(F("[CMD] Puzzle reset"));
    return true;
}

void setup() {
    uint32_t start = millis();
    while (!Serial && (millis() - start < 3000)) delay(10);
    hw.begin();

    hw.addPcf8575(PCF_ADDRESS);

    for (int i = 0; i < 4; i++) keypadLayout.setRowPin(i, rowPins[i]);
    for (int i = 0; i < 3; i++) keypadLayout.setColPin(i, colPins[i]);
    keypadManager.initialise(hw.portExpander(), &keypadLayout, &keypadListener, false);

    master.addPuzzle(&puzzle);
    master.begin();
    puzzle.run();
}

void loop() {
    master.update();
    taskManager.runLoop();
}