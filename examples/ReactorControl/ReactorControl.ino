/**
 * EXAMPLE: REACTOR CONTROL – NUCLEAR CONTROL PANEL
 * ----------------------------------------------------------
 * Analog adjustment puzzles with potentiometers, PWM gauges, and buttons.
 *
 * NARRATIVA:
 * The nuclear reactor is in a critical state. Three technicians are operating the control panel.
 * It has 3 faces. On each face, there is a potentiometer and a voltmeter.
 * The valve on one side NEVER controls the gauge on that same side – the mapping is…
 * Randomness (disorder). Additionally, each gauge can respond in a random manner.
 * normal ou reverso.
 *
 * If the players fail to confirm it (by pressing the buttons simultaneously), the panel
 * Reconfigure the mapping and orientations, forcing them to rediscover everything.
 *
 * HARDWARE: Adafruit PropMaker Feather RP2040
 * ADS1115 (I2C interface) – 3 channels for potentiometers
 * PCA9685 (I2C interface) – 3 PWM channels for voltmeters
 * - PCF8575 (I2C interface) – 3 confirmation buttons
 * External NeoPixel – 3 LEDs for button feedback + status bar
 * - Magnetic Trava — digital pin using hw.digitalWrite()
 *
 * NECESSARY AUDIO FILES (LittleFS or SD):
 * - ready.mp3       : Puzzle started
 * - alarm.mp3       : Gauge outside the green area
 * -stabilizing.mp3: Gauge has entered the green zone.
 * warning.mp3     : All gauges in the area (voltage)
 * -button_press.mp3: Button pressed
 * - fail.mp3        : Confirmation failed (time ran out)
 * -reshuffle.mp3: Reconfiguration after failure
 * - victory.mp3     : Puzzle solved
 */

#include <PropMakerEscapeRoom.h>
#include <IoAbstractionWire.h>
#include <TaskManager.h>
#include <SwitchInput.h>

using namespace propmaker_escaperoom;

// ============================================================================
// CONSTANTES
// ============================================================================
#define PCF_ADDRESS         0x20

// Confirmation buttons (P0-P2 in PCF8575)
#define BUTTON_1_PIN        0
#define BUTTON_2_PIN        1
#define BUTTON_3_PIN        2

// NeoPixels for each button (indices on the external strip)
#define BUTTON_1_PIXEL      0
#define BUTTON_2_PIXEL      1
#define BUTTON_3_PIXEL      2

// Magnetic lock pin
#define LOCK_PIN            20

// Game Configuration
static constexpr uint8_t NUM_VALVES = 3;
static constexpr uint8_t ADS_CHANNELS[NUM_VALVES] = {0, 1, 2};
static constexpr uint8_t PCA_CHANNELS[NUM_VALVES] = {0, 1, 2};
static constexpr uint8_t BUTTON_PINS[NUM_VALVES] = {BUTTON_1_PIN, BUTTON_2_PIN, BUTTON_3_PIN};
static constexpr uint8_t BUTTON_PIXELS[NUM_VALVES] = {BUTTON_1_PIXEL, BUTTON_2_PIXEL, BUTTON_3_PIXEL};

// Green Zone: Targets on a scale of 0-10, with tolerance
static constexpr float TARGETS[NUM_VALVES] = {7.0f, 5.0f, 3.0f};
static constexpr float TOLERANCE = 0.5f;

// Tempos (ms)
static constexpr unsigned long CONFIRM_WINDOW_MS = 2000UL;
static constexpr unsigned long RESHUFFLE_DURATION_MS = 1500UL;
static constexpr unsigned long EFFECT_INTERVAL_MS = 200UL;

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================
extern volatile bool btnPressed[NUM_VALVES];

// Forward Declarations
void onButtonPressed(pinid_t pin, bool held);

// ============================================================================
// CLASSE PUZZLE
// ============================================================================
class ReactorControlPuzzle : public PuzzleBase {
public:
    ReactorControlPuzzle() : PuzzleBase(31) {}  // ID 31 na rede RS485

    void setupHardware() override {
        Serial.println(F("[REACTOR] Setup has begun."));

        // Initializes buttons using SwitchInput on PCF8575
        if (hw.hasExpander()) {
            Serial.println(F("[REACTOR] Inicializando SwitchInput no PCF8575..."));
            switches.init(hw.io(), SWITCHES_POLL_KEYS_ONLY, true);

            for (uint8_t i = 0; i < NUM_VALVES; i++) {
                switches.addSwitch(BUTTON_PINS[i], onButtonPressed, NO_REPEAT, false);
            }
            Serial.println(F("[REACTOR] SwitchInput OK: 3 buttons"));
        } else {
            Serial.println(F("[REACTOR] ERROR: PCF8575 not detected!"));
        }

        // Initializes the lock
        hw.pinMode(LOCK_PIN, OUTPUT);
        hw.digitalWrite(LOCK_PIN, HIGH);  // Active in HIGH mode

        // Zera gauges PCA9685
        for (uint8_t i = 0; i < NUM_VALVES; i++) {
            hw.setAnalogOutput(PCA_CHANNELS[i], 0.0f);
        }

        // Gera mapeamento inicial
        randomizeMapping();

        // Initial State
        setState(PuzzleState::Idle);

        Serial.println(F("[REACTOR] Ready-to-use nuclear control panel."));
    }

    void update() override {
        // Process switch inputs (buttons)
        switches.runLoop();
        taskManager.runLoop();

        // Update visual/sound effects (non-blocking)
        updateEffects();

        // State Machine
        switch (getState()) {
            case PuzzleState::Idle:
                if (buttonPressed(BUTTON_1_PIN)) {
                    startPuzzle();
                }
                break;

            case PuzzleState::Running:
                switch (subState_) {
                    case SubState::Adjusting:
                        readValvesAndWriteGauges();
                        checkZones();
                        processButtonsAdjusting();
                        updateGaugeColors();
                        break;

                    case SubState::Confirming:
                        readValvesAndWriteGauges();
                        checkZones();
                        processButtonsConfirming();
                        updateConfirmationEffect();
                        break;

                    case SubState::Reshuffling:
                        updateReshuffleEffect();
                        break;
                }
                break;

            case PuzzleState::Solved:
                // Aguarda reset manual
                if (buttonPressed(BUTTON_1_PIN)) {
                    resetPuzzle();
                }
                break;

            default:
                break;
        }
    }

    void onNetworkPacket(const Rs485Payload& p) override {
        switch (p.pCommand) {
            case NetCommand::ForceSolve:
                if (getState() == PuzzleState::Running) {
                    Serial.println(F("[REACTOR] Comando remoto: ForceSolve"));
                    handleSolve();
                }
                break;

            case NetCommand::GlobalEvent:
                Serial.println(F("[REACTOR] Comando remoto: GlobalEvent (reset)"));
                resetPuzzle();
                break;

            case NetCommand::StatusReport:
                Serial.printf("[REACTOR] Status: %s, SubState: %d, Zones: %d/%d\n",
                              stateName(getState()), (int)subState_,
                              zonesActive_, NUM_VALVES);
                break;

            default:
                break;
        }
    }

private:
    enum class SubState { Adjusting, Confirming, Reshuffling };
    SubState subState_ = SubState::Adjusting;

    // Random mapping
    uint8_t valveToGauge_[NUM_VALVES];
    bool gaugeReversed_[NUM_VALVES];

    // State of the gauges
    bool inZone_[NUM_VALVES] = {false};
    uint8_t zonesActive_ = 0;

    // Confirmation
    unsigned long confirmWindowStart_ = 0;
    bool buttonPressedInWindow_[NUM_VALVES] = {false};
    uint8_t buttonsConfirmed_ = 0;

    // Efeitos
    enum class EffectPhase { None, Alarm, Stabilizing, AllGreen, Victory, Reshuffle };
    EffectPhase effectPhase_ = EffectPhase::None;
    unsigned long effectTimer_ = 0;
    uint8_t effectStep_ = 0;

    // =======================================================================
    // MAPEAMENTO
    // =======================================================================
    void randomizeMapping() {
        // Disorder of 3 elements: only 2 possibilities
        // [1,2,0] ou [2,0,1]
        uint8_t derangements[2][NUM_VALVES] = {
            {1, 2, 0},
            {2, 0, 1}
        };
        uint8_t choice = random(0, 2);
        for (uint8_t i = 0; i < NUM_VALVES; i++) {
            valveToGauge_[i] = derangements[choice][i];
        }

        // Normal/Inverted orientation for each gauge
        for (uint8_t i = 0; i < NUM_VALVES; i++) {
            gaugeReversed_[i] = random(0, 2) == 1;
        }

        Serial.println(F("[REACTOR] Mapeamento reconfigurado:"));
        for (uint8_t i = 0; i < NUM_VALVES; i++) {
            Serial.printf("Valvula %d -> Gauge %d (%s)\n",
                          i, valveToGauge_[i],
                          gaugeReversed_[valveToGauge_[i]] ? "REVERSO" : "NORMAL");
        }
    }

    // =======================================================================
    // Analogical Reading and Writing
    // =======================================================================
    void readValvesAndWriteGauges() {
        for (uint8_t valveIdx = 0; valveIdx < NUM_VALVES; valveIdx++) {
            uint8_t gaugeIdx = valveToGauge_[valveIdx];

            // Potentiometer control using ADS1115 (0-65535 raw)
            uint16_t raw = hw.readAnalogRaw(ADS_CHANNELS[valveIdx]);

            // Convert it to a percentage between 0.0 and 100.0.
            float percent = map((long)raw, 0L, 65535L, 0L, 1000L) / 10.0f;
            percent = constrain(percent, 0.0f, 100.0f);

            // Apply inversion if necessary
            if (gaugeReversed_[gaugeIdx]) {
                percent = 100.0f - percent;
            }

            // Write using the PCA9685 interface.
            hw.setAnalogOutput(PCA_CHANNELS[gaugeIdx], percent);
        }
    }

    // =======================================================================
    // ZONE VERIFICATION
    // =======================================================================
    void checkZones() {
        zonesActive_ = 0;
        for (uint8_t i = 0; i < NUM_VALVES; i++) {
            float percent = getCurrentGaugePercent(i);
            float value = percent / 10.0f;  // Escala 0-10
            bool wasInZone = inZone_[i];
            inZone_[i] = (fabs(value - TARGETS[i]) <= TOLERANCE);

            if (inZone_[i]) {
                zonesActive_++;
            }

            // During the transition phase (only when entering the zone)
            if (!wasInZone && inZone_[i] && effectPhase_ != EffectPhase::Victory) {
                hw.sound().playOnce("/stabilizing.mp3");
            }
        }
    }

    float getCurrentGaugePercent(uint8_t gaugeIdx) {
        // Reconstruct the percentage of the gauge based on the potentiometer that controls it.
        uint8_t valveIdx = 255;
        for (uint8_t v = 0; v < NUM_VALVES; v++) {
            if (valveToGauge_[v] == gaugeIdx) {
                valveIdx = v;
                break;
            }
        }
        if (valveIdx == 255) return 0.0f;

        uint16_t raw = hw.readAnalogRaw(ADS_CHANNELS[valveIdx]);
        float percent = map((long)raw, 0L, 65535L, 0L, 1000L) / 10.0f;
        percent = constrain(percent, 0.0f, 100.0f);
        if (gaugeReversed_[gaugeIdx]) {
            percent = 100.0f - percent;
        }
        return percent;
    }

    bool isInZone(uint8_t gaugeIdx) {
        float percent = getCurrentGaugePercent(gaugeIdx);
        float value = percent / 10.0f;
        return fabs(value - TARGETS[gaugeIdx]) <= TOLERANCE;
    }

    // =======================================================================
    // BOTS
    // =======================================================================
    void processButtonsAdjusting() {
        // When all gauges are in the correct range, a confirmation window will open.
        if (zonesActive_ == NUM_VALVES && subState_ == SubState::Adjusting) {
            startConfirmationWindow();
        }
    }

    void processButtonsConfirming() {
        // Check if the confirmation window has expired.
        unsigned long elapsed = millis() - confirmWindowStart_;
        if (elapsed >= CONFIRM_WINDOW_MS) {
            // Time ran out.
            if (buttonsConfirmed_ < NUM_VALVES) {
                handleConfirmationFail();
            }
            return;
        }

        // Process the buttons that are pressed in the window
        for (uint8_t i = 0; i < NUM_VALVES; i++) {
            if (!buttonPressedInWindow_[i] && buttonPressed(BUTTON_PINS[i])) {
                buttonPressedInWindow_[i] = true;
                buttonsConfirmed_++;
                hw.sound().playOnce("/button_press.mp3");

                // Turn on the NeoPixel of the button
                hw.setLight(BUTTON_PIXELS[i], Adafruit_NeoPixel::Color(0, 255, 0));

                Serial.printf("[REACTOR] Botao %d pressionado (%d/%d)\n",
                              i + 1, buttonsConfirmed_, NUM_VALVES);
            }
        }

        // Verify victory
        if (buttonsConfirmed_ >= NUM_VALVES) {
            handleSolve();
        }
    }

    boolean buttonPressed(int button) {
        int idx = button - BUTTON_1_PIN;
        if (idx < 0 || idx >= (int)NUM_VALVES) return false;

        if (btnPressed[idx]) {
            btnPressed[idx] = false;  // Limpa flag
            return true;
        }
        return false;
    }

    // =======================================================================
    // EFEITOS VISUAIS
    // =======================================================================
    void updateGaugeColors() {
        // Colors by gauge: Red = outside, Yellow = in zone, Green = all in zone
        if (subState_ == SubState::Confirming || subState_ == SubState::Reshuffling) {
            return;
        }

        uint32_t colors[NUM_VALVES];
        for (uint8_t i = 0; i < NUM_VALVES; i++) {
            if (inZone_[i]) {
                colors[i] = Adafruit_NeoPixel::Color(0, 255, 0);  // Green
            } else {
                colors[i] = Adafruit_NeoPixel::Color(255, 0, 0);  // Red
            }
        }

        // If everyone in the area is present, flash green.
        if (zonesActive_ == NUM_VALVES) {
            unsigned long t = millis() % 1000;
            if (t < 500) {
                for (uint8_t i = 0; i < NUM_VALVES; i++) {
                    hw.setLight(BUTTON_PIXELS[i], colors[i]);
                }
            } else {
                hw.setLightAll(0);
            }
            return;
        }

        // Alarme se nenhum na zona
        if (zonesActive_ == 0) {
            unsigned long t = millis() % 600;
            if (t < 300) {
                for (uint8_t i = 0; i < NUM_VALVES; i++) {
                    hw.setLight(BUTTON_PIXELS[i], colors[i]);
                }
            } else {
                hw.setLightAll(0);
            }
            return;
        }

        // Partial state: Each button lights up in the color of your gauge
        for (uint8_t i = 0; i < NUM_VALVES; i++) {
            hw.setLight(BUTTON_PIXELS[i], colors[i]);
        }
    }

    void updateConfirmationEffect() {
        // A white “Pisca” icon appears on buttons that aren’t pressed during confirmation.
        unsigned long t = millis() % 400;
        uint32_t color = (t < 200) ? Adafruit_NeoPixel::Color(255, 255, 255) : 0;

        for (uint8_t i = 0; i < NUM_VALVES; i++) {
            if (!buttonPressedInWindow_[i]) {
                hw.setLight(BUTTON_PIXELS[i], color);
            }
        }
    }

    void updateReshuffleEffect() {
        unsigned long elapsed = millis() - effectTimer_;
        if (elapsed >= RESHUFFLE_DURATION_MS) {
            // Fim do re-embaralhamento
            subState_ = SubState::Adjusting;
            effectPhase_ = EffectPhase::None;
            hw.setLightAll(0);
            Serial.println(F("[REACTOR] Reconfiguracao completa. Continue ajustando."));
            return;
        }

        // A white blink signal appears on all LEDs during the reconfiguration process.
        unsigned long t = millis() % 150;
        if (t < 75) {
            hw.setLightAll(Adafruit_NeoPixel::Color(255, 255, 255));
        } else {
            hw.setLightAll(0);
        }
    }

    void updateEffects() {
        switch (effectPhase_) {
            case EffectPhase::Alarm:
                if (millis() - effectTimer_ > EFFECT_INTERVAL_MS) {
                    effectPhase_ = EffectPhase::None;
                }
                break;

            case EffectPhase::Victory:
                updateVictoryEffect();
                break;

            case EffectPhase::Reshuffle:
                // Controlado por updateReshuffleEffect()
                break;

            default:
                break;
        }
    }

    void updateVictoryEffect() {
        unsigned long elapsed = millis() - effectTimer_;

        switch (effectStep_) {
            case 0:
                hw.setLightAll(Adafruit_NeoPixel::Color(0, 255, 0));
                if (elapsed >= 200) {
                    effectStep_ = 1;
                }
                break;
            case 1:
                hw.setLightAll(0);
                if (elapsed >= 400) {
                    hw.setLightAll(Adafruit_NeoPixel::Color(0, 255, 0));
                    effectStep_ = 2;
                }
                break;
            case 2:
                if (elapsed >= 800) {
                    hw.setLightAll(Adafruit_NeoPixel::Color(0, 255, 0));
                    effectPhase_ = EffectPhase::None;
                }
                break;
        }
    }

    // =======================================================================
    // ESTADOS DO PUZZLE
    // =======================================================================
    void startPuzzle() {
        Serial.println(F("[REACTOR] Starting sequence…"));
        hw.sound().playOnce("/ready.mp3");
        subState_ = SubState::Adjusting;
        setState(PuzzleState::Running);
    }

    void startConfirmationWindow() {
        subState_ = SubState::Confirming;
        confirmWindowStart_ = millis();
        buttonsConfirmed_ = 0;
        for (uint8_t i = 0; i < NUM_VALVES; i++) {
            buttonPressedInWindow_[i] = false;
        }

        hw.sound().playOnce("/warning.mp3");
        Serial.println(F("[REACTOR] Todos os gauges na zona! Janela de confirmacao aberta (2s)"));
    }

    void handleSolve() {
        Serial.println(F("[REACTOR] The puzzle is solved! The reactor is now stable."));

        hw.sound().playOnce("/victory.mp3");

        // Open and work
        hw.digitalWrite(LOCK_PIN, LOW);

        // Visual effect of victory
        effectPhase_ = EffectPhase::Victory;
        effectTimer_ = millis();
        effectStep_ = 0;

        setState(PuzzleState::Solved);
    }

    void handleConfirmationFail() {
        Serial.println(F("[REACTOR] Confirmation failed! Reconfiguring the panel…"));

        hw.sound().playOnce("/fail.mp3");

        // It recombines the mapping and orientation.
        randomizeMapping();

        // Status of confirmation
        subState_ = SubState::Reshuffling;
        effectPhase_ = EffectPhase::Reshuffle;
        effectTimer_ = millis();
        buttonsConfirmed_ = 0;
        for (uint8_t i = 0; i < NUM_VALVES; i++) {
            buttonPressedInWindow_[i] = false;
        }

        // One of the reconfigurations occurs after a short non-blocking delay.
        // It will be updated in the next iteration of updateEffects.
        hw.sound().playOnce("/reshuffle.mp3");
    }

    void resetPuzzle() {
        Serial.println(F("[REACTOR] Resetando puzzle..."));

        // Travar
        hw.digitalWrite(LOCK_PIN, HIGH);

        // Zerar gauges
        for (uint8_t i = 0; i < NUM_VALVES; i++) {
            hw.setAnalogOutput(PCA_CHANNELS[i], 0.0f);
        }

        // Limpar LEDs
        hw.setLightAll(0);

        // Reset the state
        subState_ = SubState::Adjusting;
        zonesActive_ = 0;
        buttonsConfirmed_ = 0;
        effectPhase_ = EffectPhase::None;
        for (uint8_t i = 0; i < NUM_VALVES; i++) {
            inZone_[i] = false;
            buttonPressedInWindow_[i] = false;
        }

        // To shuffle again for a new round
        randomizeMapping();

        setState(PuzzleState::Idle);
    }
};

// ============================================================================
// CALLBACKS SwitchInput (fora da classe)
// ============================================================================
volatile bool btnPressed[NUM_VALVES] = {false, false, false};

void onButtonPressed(pinid_t pin, bool held) {
    if (held) return;

    int idx = pin - BUTTON_1_PIN;
    if (idx >= 0 && idx < NUM_VALVES) {
        btnPressed[idx] = true;
        Serial.printf("[BTN] Botao %d (P%d) PRESSIONADO\n", idx + 1, pin);
    }
}

// ============================================================================
// MAIN
// ============================================================================
LogicManager master(31, true);  // ID 31, com rede RS485 habilitada
ReactorControlPuzzle puzzle;

void setup() {
    Serial.begin(115200);
    uint32_t start = millis();
    while (!Serial && (millis() - start < 3000)) delay(10);

    Serial.println(F("============================================"));
    Serial.println(F("  ReactorControl - PropMaker EscapeRoom"));
    Serial.println(F("Nuclear Control Panel"));
    Serial.println(F("April 2026 – Nicolau dos Brinquedos"));
    Serial.println(F("============================================"));

    hw.begin();
    hw.addPcf8575(PCF_ADDRESS);  // Adiciona expander PCF

    master.addPuzzle(&puzzle);
    master.begin();
    master.setNetworkEnabled(true);  // Enable RS485 communication

    Serial.println(F("[Main] Ready-to-use system"));
    Serial.println(F("[Main] Aguardando comando de inicio (rede ou botao 1)"));
}

void loop() {
    master.update();
    delay(1);
}
