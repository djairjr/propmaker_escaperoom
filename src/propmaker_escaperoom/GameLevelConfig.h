#pragma once

#include <Arduino.h>

namespace propmaker_escaperoom {

enum class GameLevelMode : uint8_t {
    PARALLEL = 0,   // All puzzles at this level are solved simultaneously.
    SEQUENCE = 1    // The puzzles are connected one after another.
};

struct GameLevelConfig {
    const uint8_t* puzzleIds;   // Array of IDs for the puzzles at this level
    uint8_t puzzleCount;        // Number of puzzles in the level
    GameLevelMode mode;         // PARALLEL ou SEQUENCE
};

} // namespace propmaker_escaperoom
