/**
 * EXAMPLE: ROULETTE – LOTTERY TICKET
 * ------------------------------------
 * A lighted panel with NeoPixel chips. The player presses SELECT to turn it around.
 * The light travels along the ring and slows down until it stops at a certain position.
 * If you stop in the winning sector, the puzzle is solved. If not, try again.
 *
 * Hardware:
 *   External NeoPixel for the roulette wheel
 *   Press the SELECT button (GPIO 4) to rotate.
 *   - Audio MP3 via I2S (LittleFS)
 *   Relay/Lock no pino lockPin
 *
 * Configuration via /roulette.json in LittleFS.
 */

#include <PropMakerEscapeRoom.h>
#include <TaskManager.h>
#include <SwitchInput.h>

using namespace propmaker_escaperoom;

// ============================================================================
// JSON Configuration
// ============================================================================
struct RouletteConfig {
  uint8_t lockPin = 10;
  uint8_t neoCount = 24;
  uint8_t winSectorCount = 1;   // quantos setores vencedores
  float spinSpeed = 8.0f;       // velocidade inicial (LEDs/tick)
  float friction = 0.97f;       // desaceleracao por tick
  float minSpeed = 0.1f;        // It’s completely empty when it’s below this level.
  uint8_t volume = 128;
  char sfxSpin[24]   = "/sfx/spin.mp3";
  char sfxWin[24]    = "/sfx/win.mp3";
  char sfxLose[24]   = "/sfx/lose.mp3";
  char sfxSolved[24] = "/sfx/solved.mp3";
} config;

static void loadConfig() {
  if (!LittleFS.exists("/roulette.json")) return;
  File f = LittleFS.open("/roulette.json", "r");
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return;

  config.lockPin    = doc["lockPin"]    | config.lockPin;
  config.neoCount   = doc["neoCount"]   | config.neoCount;
  config.winSectorCount = doc["winSectorCount"] | config.winSectorCount;
  config.spinSpeed  = doc["spinSpeed"]  | config.spinSpeed;
  config.friction   = doc["friction"]   | config.friction;
  config.minSpeed   = doc["minSpeed"]   | config.minSpeed;
  config.volume     = doc["volume"]     | config.volume;

  strncpy(config.sfxSpin,   doc["audio"]["spin"]   | config.sfxSpin,   sizeof(config.sfxSpin));
  strncpy(config.sfxWin,    doc["audio"]["win"]    | config.sfxWin,    sizeof(config.sfxWin));
  strncpy(config.sfxLose,   doc["audio"]["lose"]   | config.sfxLose,   sizeof(config.sfxLose));
  strncpy(config.sfxSolved, doc["audio"]["solved"] | config.sfxSolved, sizeof(config.sfxSolved));
}

// ============================================================================
// PINOS
// ============================================================================
#define BTN_SELECT 4

// ============================================================================
// FORWARD
// ============================================================================
class RoulettePuzzle;
static RoulettePuzzle* rouletteInstance = nullptr;
static void onButtonPress(uint8_t pin, bool held);

// ============================================================================
// PUZZLE
// ============================================================================
class RoulettePuzzle : public PuzzleBase {
public:
  RoulettePuzzle() : PuzzleBase(16) { rouletteInstance = this; }

  void setupHardware() override {
    loadConfig();

    hw.pinMode(config.lockPin, OUTPUT);
    hw.digitalWrite(config.lockPin, HIGH);

    if (config.neoCount > 0) setPixelRange(0, config.neoCount);

    switches.addSwitch(BTN_SELECT, onButtonPress, NO_REPEAT, false);

    randomSeed(analogRead(28));
    resetSpin();
    chooseWinSector();
    setState(PuzzleState::Idle);
    Serial.printf("[ROULETTE] Select to spin! Winner’s area: %d\n", winSector_);
  }

  void update() override {
    hw.sound().loop();
    taskManager.runLoop();
    switches.runLoop();

    switch (getState()) {
      case PuzzleState::Idle:    renderIdle(); break;
      case PuzzleState::Running: updateSpinning(); break;
      case PuzzleState::Solved:  renderSolved(); break;
      default: break;
    }
  }

  void handleSelect() {
    if (getState() == PuzzleState::Idle) {
      startSpin();
    }
  }

private:
  float pos_ = 0;
  float speed_ = 0;
  int winSector_ = 0;
  int spins_ = 0;
  unsigned long solvedAt_ = 0;
  static constexpr int SPINS_TO_WIN = 3;

  void chooseWinSector() {
    winSector_ = random(0, config.neoCount);
  }

  void startSpin() {
    pos_ = 0;
    speed_ = config.spinSpeed;
    spins_ = 0;
    hw.sound().playOnce(config.sfxSpin);
    setState(PuzzleState::Running);
    Serial.println(F("[ROULETTE] Girando..."));
  }

  void updateSpinning() {
    // Desacelera
    speed_ *= config.friction;

    // Avanca position
    pos_ += speed_;
    while (pos_ >= config.neoCount) {
      pos_ -= config.neoCount;
      spins_++;
    }

    renderSpin();

    // Parou?
    if (speed_ < config.minSpeed) {
      stopSpin();
    }
  }

  void stopSpin() {
    int ipos = (int)pos_;
    bool won = false;

    // Check whether this is in the winning sector.
    if (ipos == winSector_) {
      won = true;
    }

    // Adjacent sectors also win if config.winSectorCount > 1
    if (!won && config.winSectorCount > 1) {
      for (int i = 1; i < config.winSectorCount; i++) {
        int left = (winSector_ - i + config.neoCount) % config.neoCount;
        int right = (winSector_ + i) % config.neoCount;
        if (ipos == left || ipos == right) { won = true; break; }
      }
    }

    if (won) {
      Serial.printf("[ROULETTE] GANHOU! Parou em %d (vencedor=%d)\n", ipos, winSector_);
      if (spins_ >= SPINS_TO_WIN - 1) {
        solve();
      } else {
        hw.sound().playOnce(config.sfxWin);
        showResult(true);
        setState(PuzzleState::Idle);
      }
    } else {
      Serial.printf("[ROULETTE] Perdeu! Parou em %d (vencedor=%d)\n", ipos, winSector_);
      hw.sound().playOnce(config.sfxLose);
      showResult(false);
      setState(PuzzleState::Idle);
    }
  }

  void solve() {
    hw.digitalWrite(config.lockPin, LOW);
    hw.sound().playOnce(config.sfxSolved);
    solvedAt_ = millis();
    showResult(true);
    setState(PuzzleState::Solved);
    Serial.println(F("[ROULETTE] SOLVED!"));
  }

  void showResult(bool win) {
    if (!pixels()) return;
    auto* px = pixels();
    uint32_t color = win ? px->Color(0, 50, 0) : px->Color(50, 0, 0);
    px->setAll(color);
    px->show();
  }

  void renderSpin() {
    if (!pixels()) return;
    auto* px = pixels();
    px->clear();
    int ipos = (int)pos_;

    // Destaque o setor vencedor
    px->setPixel(winSector_, px->Color(0, 20, 0));

    // Corredor branco
    px->setPixel(ipos, px->Color(255, 255, 255));

    // Esteira (trail)
    for (int i = 1; i <= 3; i++) {
      int idx = ((int)pos_ - i + config.neoCount * 2) % config.neoCount;
      uint8_t b = 80 - i * 20;
      px->setPixel(idx, px->Color(b, b, b));
    }

    px->show();
  }

  void renderIdle() {
    if (!pixels()) return;
    auto* px = pixels();
    uint8_t b = (sin(millis() / 600.0f) * 0.5f + 0.5f) * 20;
    px->clear();
    px->setPixel(winSector_, px->Color(0, b, 0));
    px->show();
  }

  void renderSolved() {
    if (!pixels()) return;
    auto* px = pixels();
    unsigned long t = millis() - solvedAt_;
    uint8_t hue = (t / 40) % 256;
    for (uint8_t i = 0; i < px->count(); i++)
      px->setPixel(i, px->strip()->ColorHSV(((uint16_t)(hue + i * 12) % 256) << 8, 255, 70));
    px->show();

    if (t > 8000) {
      hw.digitalWrite(config.lockPin, HIGH);
      chooseWinSector();
      setState(PuzzleState::Idle);
    }
  }

  void resetSpin() {
    pos_ = 0; speed_ = 0;
  }
};

// ============================================================================
// CALLBACK
// ============================================================================
static void onButtonPress(uint8_t pin, bool held) {
  if (held || !rouletteInstance) return;
  if (pin == BTN_SELECT) rouletteInstance->handleSelect();
}

// ============================================================================
// MAIN
// ============================================================================
LogicManager master(16);
RoulettePuzzle puzzle;

void setup() {
  uint32_t start = millis();
  while (!Serial && (millis() - start < 3000)) delay(10);
  hw.begin();
  master.addPuzzle(&puzzle);
  master.begin();
}

void loop() {
  master.update();
}
