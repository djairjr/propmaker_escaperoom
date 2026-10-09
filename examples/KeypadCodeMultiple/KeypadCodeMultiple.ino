/**
 * EXEMPLO: KEYPAD CODE MULTIPLE - MULTIPLAS SENHAS VIA JSON
 * ----------------------------------------------------------
 * Numerical password input via 4x3 I2C keyboard (PCF8574/8575).
 * Multiple codes can be accepted, as configured through /keypadmulti.json.
 * Any valid code will solve the puzzle.
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
#define MAX_CODES 8

struct KeypadMultiConfig {
  char codes[MAX_CODES][5] = {"1234", "5678", "", "", "", "", "", ""};
  uint8_t codeCount = 2;
  uint8_t lockPin = 20;
  char sfxKeypress[24] = "/sfx/beep.mp3";
  char sfxUnlock[24]  = "/sfx/magic.mp3";
  char sfxDenied[24]  = "/sfx/error.mp3";
  char sfxCancel[24]  = "/sfx/cancel.mp3";
  uint8_t neoCount = 8;
  uint32_t colorIdle   = 0x050514;
  uint32_t colorUnlock = 0x003200;
  uint32_t colorDenied = 0x320000;
} config;

static void loadConfig() {
  if (!LittleFS.exists("/keypadmulti.json")) {
    Serial.println(F("[CONFIG] /keypadmulti.json It doesn’t exist; it uses a default pattern."));
    return;
  }
  File f = LittleFS.open("/keypadmulti.json", "r");
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    Serial.printf("[CONFIG] Erro JSON: %s\n", err.c_str());
    return;
  }

  // Codes array
  if (doc["codes"].is<JsonArray>()) {
    auto arr = doc["codes"].as<JsonArray>();
    config.codeCount = min((uint8_t)arr.size(), (uint8_t)MAX_CODES);
    for (uint8_t i = 0; i < config.codeCount; i++) {
      strncpy(config.codes[i], arr[i] | "", sizeof(config.codes[i]) - 1);
      config.codes[i][sizeof(config.codes[i]) - 1] = '\0';
    }
  } else {
    config.codeCount = doc["codeCount"] | config.codeCount;
    if (config.codeCount > MAX_CODES) config.codeCount = MAX_CODES;
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

  Serial.printf("[CONFIG] Codes=%d, lockPin=%d\n", config.codeCount, config.lockPin);
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
  }
};
DisplayWrapper display;

// ============================================================================
// PUZZLE
// ============================================================================
String enteredCode = "";

class KeypadCodeMultiple : public PuzzleBase {
public:
  KeypadCodeMultiple() : PuzzleBase(14) {}

  void setupHardware() override {
    loadConfig();

    hw.pinMode(config.lockPin, OUTPUT);
    hw.digitalWrite(config.lockPin, HIGH);

    display.setBrightness(15);
    display.printString("____");

    dbg_.begin();
    dbg_.addCommand("codes", "list|add|clear <s> Code Management", cmdCodes);
    dbg_.addCommand("reset", "        Reseta o puzzle",              cmdReset);
    dbg_.addCommand("status","Show configuration",          cmdStatus);
    dbg_.printHelp();

    setVisualIdle();
    Serial.printf("[KEYPAD] Waiting for password (%d valid codes)...\n", config.codeCount);
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
    for (uint8_t i = 0; i < config.codeCount; i++) {
      if (enteredCode == config.codes[i]) {
        handleSolve();
        return;
      }
    }
    handleWrong();
  }

  void handleSolve() {
    Serial.println(F("[KEYPAD] ACERTOU!"));
    hw.digitalWrite(config.lockPin, LOW);
    hw.sound().playOnce(config.sfxUnlock);
    display.printString(enteredCode.c_str());
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
LogicManager master(14);
KeypadCodeMultiple puzzle;

// ============================================================================
// Serial Commands
// ============================================================================
static bool cmdCodes(const char* arg) {
  if (!arg) return false;
  if (strcmp(arg, "list") == 0) {
    Serial.printf("Codes (%d/%d):\n", config.codeCount, MAX_CODES);
    for (uint8_t i = 0; i < config.codeCount; i++) {
      Serial.printf("  [%d] %s\n", i, config.codes[i]);
    }
    return true;
  }
  if (strcmp(arg, "clear") == 0) {
    config.codeCount = 0;
    memset(config.codes, 0, sizeof(config.codes));
    Serial.println(F("[CMD] All codes have been removed."));
    return true;
  }
  if (strncmp(arg, "add ", 4) == 0) {
    const char* newCode = arg + 4;
    if (strlen(newCode) != 4) return false;
    if (config.codeCount >= MAX_CODES) {
      Serial.printf("[CMD] The limit of %d codes reached is \n", MAX_CODES);
      return true;
    }
    strncpy(config.codes[config.codeCount], newCode, 4);
    config.codes[config.codeCount][4] = '\0';
    config.codeCount++;
    Serial.printf("[CMD] Added code: %s\n", newCode);
    return true;
  }
  return false;
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
  Serial.printf("Codes=%d lockPin=%d state=%s\n",
                config.codeCount, config.lockPin, PuzzleBase::stateName(puzzle.getState()));
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
