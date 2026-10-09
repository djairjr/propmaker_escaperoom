// GameConfig.h – Configuration of the 5 Game Levels in Nuclear Reactor
// This file defines the levels of the game. Each level contains a set of elements.
// Puzzles that are played in parallel or sequentially.
//
// Levels:
//   0 - ACESSO:    P1 KeypadRFID, P2 CoolantValves, P3 ElectricalPanel (PARALLEL)
//   1 – POSITIONING: P4 RFID at the base of the platforms (parallel)
//   2 – REATOR: P5 ReactorControl (PARALLEL)
//   3 - TAMPA:     P6 SimonSays na tampa (PARALLEL)
//   4 - INTERIOR:  P7 RadioRepair, P8 RotaryDial, P9 BombTimer (SEQUENCE)

#pragma once

#include <PropMakerEscapeRoom.h>

using namespace propmaker_escaperoom;

// =====IDs OF THE PUZZLES=====
// Each puzzle has a unique ID. This ID is used to reference it in GameLevelConfig.
const uint8_t ID_P1_KEYPAD_RFID    = 1;
const uint8_t ID_P2_COOLANT_VALVES = 2;
const uint8_t ID_P3_ELECTRICAL_PNL = 3;
const uint8_t ID_P4_POSITION_RFID  = 4;
const uint8_t ID_P5_REACTOR_CTRL   = 5;
const uint8_t ID_P6_LID_SIMONSAYS  = 6;
const uint8_t ID_P7_RADIO_REPAIR   = 7;
const uint8_t ID_P8_ROTARY_DIAL    = 8;
const uint8_t ID_P9_BOMB_TIMER     = 9;

// ===== GAME LEVELS =====
// Declaration of ID arrays for each level

// Level 0: Acesso (P1, P2, P3 em PARALLEL)
// The 3 technicians are trapped. The player must solve all 3 puzzles to get access.
const uint8_t level0_acesso[] = {
    ID_P1_KEYPAD_RFID,
    ID_P2_COOLANT_VALVES,
    ID_P3_ELECTRICAL_PNL
};

// Level 1: Positioning (P4 in parallel)
// Extended platforms. The player places each technician in the correct position.
const uint8_t level1_posicionamento[] = {
    ID_P4_POSITION_RFID
};

// Level 2: Reactor (P5 in parallel)
// Adjust the valves to keep the core stable.
const uint8_t level2_reator[] = {
    ID_P5_REACTOR_CTRL
};

// Level 3: Tampa (P6 em PARALLEL)
// SimonSays na tampa fechada. Resolver abre a tampa.
const uint8_t level3_tampa[] = {
    ID_P6_LID_SIMONSAYS
};

// Level 4: Interior (P7, P8, P9 em SEQUENCE)
// Repair the radio → Discard the numbers → Cut the wires in the correct order.
const uint8_t level4_interior[] = {
    ID_P7_RADIO_REPAIR,
    ID_P8_ROTARY_DIAL,
    ID_P9_BOMB_TIMER
};

// =====CONFIGURING THE GAME LEVELS====
// Complete set of 5 levels to pass to LogicManager::setGameLevel().

const GameLevelConfig NUCLEAR_GAME_LEVELS[] = {
    { level0_acesso,          3, GameLevelMode::PARALLEL  },
    { level1_posicionamento,  1, GameLevelMode::PARALLEL  },
    { level2_reator,          1, GameLevelMode::PARALLEL  },
    { level3_tampa,           1, GameLevelMode::PARALLEL  },
    { level4_interior,        3, GameLevelMode::SEQUENCE  }
};

const uint8_t NUCLEAR_LEVEL_COUNT = 5;
