/**
 * EXAMPLE: MASTERMIND – A game of guessing colors
 * ----------------------------------------------------
 * The player must guess a sequence of 4 colors (1-6).
 * With each attempt, the display shows how many colors are present.
 * The correct position (P), and how many colors exist in that position.
 * Wrong (E). NeoPixel displays the colors of the current attempt.
 *
 * Hardware:
 *   4x3 keyboard via PCF8575 (keys 1-6 for colors)
 *   External NeoPixel for color feedback
 *   Display HT16K33 for hints (P=0, E=2…)
 *   - Audio MP3 via I2S (LittleFS)
 *   Relay/Lock no pino lockPin
 *
 * Configuration via /mastermind.json in LittleFS.
 */

#include <PropMakerEscapeRoom.h>
#include <IoAbstractionWire.h>
#include <KeyboardManager.h>
#include <TaskManager.h>
#include <SwitchInput.h>

using namespace propmaker_escaperoom;

// ============================================================================
// JSON Configuration
// ============================================================================
struct MastermindConfig {
  uint8_t codeLength = 4;       // quantas posicoes
  uint8_t maxColors = 6;        // Available cores (1-6)
  uint8_t maxAttempts = 10;     // Maximum attempts
  uint8_t lockPin = 20;
  uint8_t neoCount = 8;
  uint32_t colorIdle   = 0x050514;
  uint32_t colorUnlock = 0x003200;
  uint32_t colorDenied = 0x320000;
  char sfxKeypress[24] = "/sfx/beep.mp3";
  char sfxUnlock[24]   = "/sfx/magic.mp3";
  char sfxDenied[24]   = "/sfx/error.mp3";
  char sfxCancel[24]   = "/sfx/cancel.mp3";
} config;

static const uint32_t colorValues[6] = {
  0xFF0000, // 1 = Red
  0x0000FF, // 2 = Blue
  0x00FF00, // 3 = Green
  0xFFFF00, // 4 = Yellow
  0xFF00FF, // 5 = Magenta
  0x00FFFF  // 6 = Ciano
};
static const char colorNames[6][12] = {
  "Red", "Azul", "Green", "Yellow", "Magenta", "Ciano"
};

static void loadConfig() {
  if (!LittleFS.exists("/mastermind.json")) return;
  File f = LittleFS.open("/mastermind.json", "r");
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return;

  config.codeLength  = doc["codeLength"]  | config.codeLength;
  config.maxColors   = doc["maxColors"]   | config.maxColors;
  config.maxAttempts = doc["maxAttempts"] | config.maxAttempts;
  config.lockPin     = doc["lockPin"]     | config.lockPin;
  config.neoCount    = doc["neoCount"]    | config.neoCount;

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

  strncpy(config.sfxKeypress, doc["audio"]["keypress"] | config.sfxKeypress, sizeof(config.sfxKeypress));
  strncpy(config.sfxUnlock,   doc["audio"]["unlock"]   | config.sfxUnlock,   sizeof(config.sfxUnlock));
  strncpy(config.sfxDenied,   doc["audio"]["denied"]   | config.sfxDenied,   sizeof(config.sfxDenied));
  strncpy(config.sfxCancel,   doc["audio"]["cancel"]   | config.sfxCancel,   sizeof(config.sfxCancel));
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
  bool check() { return hw.hasDisplay(); }
  void print(const char* s) { if (check()) { hw.display().clear(); hw.display().printString(s); } }
  void num(int v) { if (check()) hw.display().printNum(v); }
  void digit(int pos, char c) { if (check()) hw.display().writeDigitAscii(pos, c); }
  void bright(uint8_t b) { if (check()) hw.display().setBrightness(b); }
};
DisplayWrapper disp;

// ============================================================================
// PUZZLE
// ============================================================================
class MastermindPuzzle : public PuzzleBase {
public:
  MastermindPuzzle() : PuzzleBase(17) {}

  void setupHardware() override {
    loadConfig();

    hw.pinMode(config.lockPin, OUTPUT);
    hw.digitalWrite(config.lockPin, HIGH);

    if (config.neoCount > 0) setPixelRange(0, config.neoCount);

    disp.bright(15);
    disp.print("CodE");

    randomSeed(analogRead(28));
    newGame();

    dbg_.begin();
    dbg_.addCommand("reset",  "        Reseta o jogo",           cmdReset);
    dbg_.addCommand("status", "Show configuration",     cmdStatus);
    dbg_.printHelp();

    Serial.printf("[MM] Colors: 1=Vermelho 2=Azul 3=Verde 4=Amarelo 5=Magenta 6=Ciano\n");
    Serial.printf("[MM] Use keys 1-6; =send *=clean\n");
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
  uint8_t secret_[8] = {0};
  uint8_t guess_[8] = {0};
  int guessLen_ = 0;
  int attempt_ = 0;
  bool gameOver_ = false;

  void newGame() {
    attempt_ = 0;
    guessLen_ = 0;
    gameOver_ = false;
    memset(guess_, 0, sizeof(guess_));
    for (uint8_t i = 0; i < config.codeLength; i++)
      secret_[i] = random(1, config.maxColors + 1);

    Serial.print(F("[MM] Secret code:"));
    for (uint8_t i = 0; i < config.codeLength; i++)
      Serial.printf("%d ", secret_[i]);
    Serial.println();

    showGuess();
  }

  void handleKey(char key) {
    if (gameOver_) return;

    if (key == '*') {
      guessLen_ = 0;
      memset(guess_, 0, sizeof(guess_));
      hw.sound().playOnce(config.sfxCancel);
      showGuess();
    }
    else if (key == '#') {
      submitGuess();
    }
    else if (key >= '1' && key <= '6') {
      if (guessLen_ < config.codeLength) {
        guess_[guessLen_++] = key - '0';
        hw.sound().playOnce(config.sfxKeypress);
        showGuess();
      }
    }
  }

  void submitGuess() {
    if (guessLen_ < config.codeLength) {
      Serial.println(F("[MM] Fill in all the spaces!"));
      return;
    }

    attempt_++;
    Serial.printf("[MM] Tentativa %d/%d: ", attempt_, config.maxAttempts);

    // Calcula feedback
    uint8_t exact = 0, color = 0;
    uint8_t secretCopy[8], guessCopy[8];
    memcpy(secretCopy, secret_, config.codeLength);
    memcpy(guessCopy, guess_, config.codeLength);

    // Primeiro: posicoes exatas
    for (uint8_t i = 0; i < config.codeLength; i++) {
      if (guessCopy[i] == secretCopy[i]) {
        exact++;
        secretCopy[i] = guessCopy[i] = 0;
      }
    }
    // Afterwards: The right colors in the wrong positions
    for (uint8_t i = 0; i < config.codeLength; i++) {
      if (guessCopy[i] == 0) continue;
      for (uint8_t j = 0; j < config.codeLength; j++) {
        if (secretCopy[j] == 0 || i == j) continue;
        if (guessCopy[i] == secretCopy[j]) {
          color++;
          secretCopy[j] = 0;
          break;
        }
      }
    }

    Serial.printf("P=%d E=%d\n", exact, color);

    // Feedback visual no NeoPixel
    showFeedback(guess_, exact, color);

    // Feedback no display
    char buf[5];
    snprintf(buf, sizeof(buf), "P%dE%d", exact, color);
    disp.print(buf);

    // Venceu?
    if (exact == config.codeLength) {
      handleSolve();
      return;
    }

    // Perdeu?
    if (attempt_ >= config.maxAttempts) {
      Serial.printf("[MM] FIM! The code was:");
      for (uint8_t i = 0; i < config.codeLength; i++)
        Serial.printf("%d ", secret_[i]);
      Serial.println();
      gameOver_ = true;
      hw.sound().playOnce(config.sfxDenied);
      hw.setLightAll(config.colorDenied);
      disp.print("noPE");
      return;
    }

    guessLen_ = 0;
    memset(guess_, 0, sizeof(guess_));
  }

  void handleSolve() {
    Serial.println(F("[MM] SOLVED!"));
    hw.digitalWrite(config.lockPin, LOW);
    hw.sound().playOnce(config.sfxUnlock);
    hw.setLightAll(config.colorUnlock);
    disp.print("donE");
    gameOver_ = true;
    setState(PuzzleState::Solved);
  }

  void showGuess() {
    if (!pixels()) return;
    auto* px = pixels();
    px->clear();

    for (int i = 0; i < guessLen_; i++) {
      uint8_t c = guess_[i] - 1;
      if (c < 6) px->setPixel(i, colorValues[c]);
    }

    // Empty positions in blue dimness
    for (int i = guessLen_; i < config.codeLength; i++) {
      px->setPixel(i, px->Color(0, 0, 15));
    }

    px->show();
  }

  void showFeedback(uint8_t* g, uint8_t exact, uint8_t color) {
    if (!pixels()) return;
    auto* px = pixels();
    px->clear();

    // It shows the attempt (first LEDs).
    for (int i = 0; i < config.codeLength; i++) {
      uint8_t c = g[i] - 1;
      if (c < 6) px->setPixel(i, colorValues[c]);
    }

    // Feedback: P (green LEDs) and E (yellow LEDs)
    int offset = config.codeLength;
    for (int i = 0; i < exact && offset < config.neoCount; i++)
      px->setPixel(offset++, px->Color(0, 50, 0));
    for (int i = 0; i < color && offset < config.neoCount; i++)
      px->setPixel(offset++, px->Color(50, 50, 0));

    px->show();
  }

  void handleReset() {
    hw.digitalWrite(config.lockPin, HIGH);
    hw.setLightAll(0);
    newGame();
    disp.print("CodE");
    setState(PuzzleState::Running);
  }

  SerialDebug dbg_;
};

// ============================================================================
// MAIN
// ============================================================================
LogicManager master(17);
MastermindPuzzle puzzle;

// ============================================================================
// Serial Commands
// ============================================================================
static bool cmdReset(const char*) {
  puzzle.setState(PuzzleState::Running);
  Serial.println(F("[CMD] Puzzle resetado"));
  return true;
}

static bool cmdStatus(const char*) {
  Serial.printf("State=%s lockPin=%d\n",
                PuzzleBase::stateName(puzzle.getState()), config.lockPin);
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
