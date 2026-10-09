/**
 * EXAMPLE: KEYPAD CODE ENTRY – Password entry via JSON
 * ------------------------------------------------------
 * Numerical password input via 4x3 I2C keyboard (PCF8574/8575).
 * Configuration via /keypad.json in LittleFS.
 *
 * If the file does not exist, use default values.
 */

#include <PropMakerEscapeRoom.h>
#include <IoAbstractionWire.h>
#include <KeyboardManager.h>
#include <TaskManager.h>

using namespace propmaker_escaperoom;

// ============================================================================
// JSON Configuration
// ============================================================================
struct KeypadConfig {
    char code[5] = "8671";         // 4-digit code + null
    uint8_t lockPin = 20;
    char sfxKeypress[24] = "/sfx/beep.mp3";
    char sfxUnlock[24] = "/sfx/magic.mp3";
    char sfxDenied[24] = "/sfx/error.mp3";
    char sfxCancel[24] = "/sfx/cancel.mp3";
    uint8_t neoCount = 8;
    uint32_t colorIdle = 0x050514;     // Soft blue
    uint32_t colorUnlock = 0x003200;   // Green
    uint32_t colorDenied = 0x320000;   // Red
} config;

static void loadConfig() {
    if (!LittleFS.exists("/keypad.json")) {
        Serial.println(F("[CONFIG] /keypad.json It doesn’t exist; it uses a default pattern."));
        return;
    }
    File f = LittleFS.open("/keypad.json", "r");
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        Serial.printf("[CONFIG] Erro parsing JSON: %s\n", err.c_str());
        return;
    }

    // Code
    if (doc["code"].is<const char*>()) {
        strncpy(config.code, doc["code"], sizeof(config.code) - 1);
        config.code[sizeof(config.code) - 1] = '\0';
    }
    config.lockPin = doc["lockPin"] | config.lockPin;

    // Audio
    strncpy(config.sfxKeypress, doc["audio"]["keypress"] | config.sfxKeypress, sizeof(config.sfxKeypress));
    strncpy(config.sfxUnlock,   doc["audio"]["unlock"]   | config.sfxUnlock,   sizeof(config.sfxUnlock));
    strncpy(config.sfxDenied,   doc["audio"]["denied"]   | config.sfxDenied,   sizeof(config.sfxDenied));
    strncpy(config.sfxCancel,   doc["audio"]["cancel"]   | config.sfxCancel,   sizeof(config.sfxCancel));

    // NeoPixel
    config.neoCount = doc["neopixel"]["count"] | config.neoCount;
    if (doc["neopixel"]["colorIdle"].is<JsonArray>()) {
        auto arr = doc["neopixel"]["colorIdle"].as<JsonArray>();
        config.colorIdle = hw.pixelsExternal().Color(arr[0] | 5, arr[1] | 5, arr[2] | 20);
    }
    if (doc["neopixel"]["colorUnlock"].is<JsonArray>()) {
        auto arr = doc["neopixel"]["colorUnlock"].as<JsonArray>();
        config.colorUnlock = hw.pixelsExternal().Color(arr[0] | 0, arr[1] | 50, arr[2] | 0);
    }
    if (doc["neopixel"]["colorDenied"].is<JsonArray>()) {
        auto arr = doc["neopixel"]["colorDenied"].as<JsonArray>();
        config.colorDenied = hw.pixelsExternal().Color(arr[0] | 50, arr[1] | 0, arr[2] | 0);
    }

    Serial.printf("[CONFIG] Code=%s, lockPin=%d\n", config.code, config.lockPin);
}

// ============================================================================
// HARDWARE
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
    } else Serial.printf("[DISPLAY] Texto: %s\n", str);
  }

  void writeDigitAscii(int digit, char c) {
    if (checkDisplay()) hw.display().writeDigitAscii(digit, c);
    else Serial.printf("[DISPLAY] Digito %d: %c\n", digit, c);
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

class KeypadCodeEntry : public PuzzleBase {
public:
    KeypadCodeEntry() : PuzzleBase(12) {}

    void setupHardware() override {
        loadConfig();

        hw.pinMode(config.lockPin, OUTPUT);
        hw.digitalWrite(config.lockPin, HIGH);

        display.setBrightness(15);
        display.printString("____");

        dbg_.begin();
        dbg_.addCommand("code",    "<novo> Change the password",          cmdCode);
        dbg_.addCommand("lockpin", "<pino> Changes the lock pin",    cmdLockPin);
        dbg_.addCommand("reset",   "        Reseta o puzzle",         cmdReset);
        dbg_.addCommand("status",  "Show configuration",     cmdStatus);
        dbg_.printHelp();

        setVisualIdle();
        Serial.printf("[KEYPAD] Waiting for password (code=%s)…\n", config.code);
    }

    void update() override {
        if (state_ != PuzzleState::Running) return;

        dbg_.update();
        taskManager.runLoop();

        char key = keypadListener.lastKey;
        if (key != 0) {
            keypadListener.lastKey = 0;
            handleKey(key);
        }
    }

private:
    void handleKey(char key) {
        Serial.printf("[KEYPAD] Tecla: %c\n", key);

        if (key == '*') {
            enteredCode = "";
            hw.sound().playOnce(config.sfxCancel);
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
        if (enteredCode == config.code) {
            handleSolve();
        } else {
            handleWrong();
        }
    }

    void handleSolve() {
        Serial.println(F("[KEYPAD] ACERTOU!"));
        hw.digitalWrite(config.lockPin, LOW);
        hw.sound().playOnce(config.sfxUnlock);
        display.printString(config.code);
        hw.setLightAll(config.colorUnlock);
        setState(PuzzleState::Solved);
    }

    void handleWrong() {
        Serial.println(F("[KEYPAD] Incorrect password."));
        hw.sound().playOnce(config.sfxDenied);
        enteredCode = "";
        display.printString("Err ");
        hw.setLightAll(config.colorDenied);
        delay(800);
        display.printString("____");
        setVisualIdle();
    }

    void handleReset() {
        hw.digitalWrite(config.lockPin, HIGH);
        enteredCode = "";
        display.printString("____");
        setVisualIdle();
        setState(PuzzleState::Running);
    }

    void setVisualIdle() {
        hw.setLightAll(0);
        hw.setLight(0, config.colorIdle);
    }

    void setVisualProgress(uint8_t step) {
        uint32_t c = hw.pixelsExternal().Color(0, 0, 50);
        for (uint8_t i = 0; i < step && i < config.neoCount; i++) {
            hw.setLight(i, c);
        }
    }

    SerialDebug dbg_;
};

// ============================================================================
// MAIN
// ============================================================================
LogicManager master(12);
KeypadCodeEntry puzzle;

// ============================================================================
// Serial Commands
// ============================================================================
static bool cmdCode(const char* arg) {
    if (!arg || strlen(arg) != 4) return false;
    strncpy(config.code, arg, sizeof(config.code) - 1);
    config.code[sizeof(config.code) - 1] = '\0';
    Serial.printf("[CMD] The password has been changed to: %s\n", config.code);
    return true;
}

static bool cmdLockPin(const char* arg) {
    if (!arg) return false;
    uint8_t pin = atoi(arg);
    if (pin < 2 || pin > 28) return false;
    config.lockPin = pin;
    Serial.printf("[CMD] LockPin has been changed to: %d\n", config.lockPin);
    return true;
}

static bool cmdReset(const char*) {
    hw.digitalWrite(config.lockPin, HIGH);
    enteredCode = "";
    puzzle.setState(PuzzleState::Running);
    hw.setLightAll(0);
    hw.setLight(0, config.colorIdle);
    Serial.println(F("[CMD] Puzzle resetado"));
    return true;
}

static bool cmdStatus(const char*) {
    Serial.printf("Code=%s lockPin=%d state=%s\n",
                  config.code, config.lockPin, PuzzleBase::stateName(puzzle.getState()));
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
