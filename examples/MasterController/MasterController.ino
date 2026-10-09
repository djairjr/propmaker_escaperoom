/**
 * EXAMPLE: MASTER CONTROLLER (ADMIN) - WITH RELAYS AND TIMER
 * ------------------------------------------------------
 * This module manages the global time, the RS485 network and
 * controls a bank of 8 relays via PCF8574 expander.
 */

#include <PropMakerEscapeRoom.h>

using namespace propmaker_escaperoom;

// ============================================================================
// SYSTEM CONFIGURATION
// ============================================================================
#define MASTER_ID 1
#define TOTAL_GAME_TIME 3600 // 60 minutes (Max 7200 for 2h rooms)

// Relay Mapping on the PCF8574 (Pins 0 to 7)
#define RELAY_MAIN_DOOR    0
#define RELAY_LIGHTS       1
#define RELAY_SMOKE_GEN    2
#define RELAY_AUX_1        3

// ============================================================================
// MASTER CLASS (Actuator Management)
// ============================================================================
class MasterLogic : public PuzzleBase {
public:
    MasterLogic() : PuzzleBase(0) {} // ID 0 for the Central Controller

    void setupHardware() override {
        // Configures all expander pins as output for the relays
        // Initializes as HIGH (common for relay modules active LOW)
        for(int i=0; i<8; i++) {
            hw.inputOutput()->pinMode(i, OUTPUT);
            hw.inputOutput()->digitalWrite(i, HIGH);
        }
        
        Serial.println("[MASTER] Relay Bank Initialized.");
    }

    void update() override {
        // Example: If time runs out, turns off the lights and opens the door
        // In the LogicManager the time is managed automatically, 
        // but here we can react to it.
    }
    
    void openAllDoors() {
        hw.inputOutput()->digitalWrite(RELAY_MAIN_DOOR, LOW); // Triggers relay
        Serial.println("[MASTER] Doors Open!");
    }
};

// ============================================================================
// ORCHESTRATION
// ============================================================================
LogicManager master(MASTER_ID, true); // true = Sets as network MASTER
MasterLogic logic;

void logicTask(void *parameter) {
    while (true) {
        master.update();
        
        // Local end-of-game check
        if (master.getRemainingTime() == 0) {
            logic.openAllDoors();
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void setup() {
    // 1. Waits for Serial for Debug
    uint32_t start = millis();
    while (!Serial && (millis() - start < 3000)) delay(10);
    hw.begin();
    hw.addPcf8575(0x20);  // Initializes PCF8575 for relay control

    master.addPuzzle(&logic);
    master.begin();
    logic.run();

    // Starts the global timer (automatic RS485 Sync to the Slaves)
    master.startTimer(TOTAL_GAME_TIME);

    // Creates the logic task on Core 0
    xTaskCreate(logicTask, "MasterLogic", 8192, NULL, 1, NULL);
}

void loop() {
    // Simple Administration Console via Serial
    if (Serial.available()) {
        char cmd = Serial.read();
        if (cmd == 'o') { // 'o' for Open
            logic.openAllDoors();
        }
        if (cmd == 'r') { // 'r' for Reset
            master.startTimer(TOTAL_GAME_TIME);
            Serial.println("[ADMIN] Time Reset.");
        }
    }
    delay(10);
}