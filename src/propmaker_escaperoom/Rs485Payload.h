#pragma once

#include <Arduino.h>
#include "PuzzleState.h"

namespace propmaker_escaperoom {

enum class NetCommand : uint8_t { 
  None = 0, 
  ForceSolve = 1, 
  SetProperty = 2, 
  SyncTimer = 3,       // Master broadcasts remaining seconds
  GlobalEvent = 4,     // Trigger universal event
  StatusReport = 5     // Node reporting its current state
};

struct __attribute__((packed)) Rs485Payload {
  uint8_t header;         // Fixed: 0xAA
  uint8_t targetId;       // Destination (0 = Broadcast)
  uint8_t sourceId;       // Origin
  uint8_t puzzleId;       // Logic ID
  PuzzleState pState;     // Current FSM state
  NetCommand pCommand;    // Network Command
  uint16_t data;          // Payload (16-bit for timer/values)
  uint8_t checksum;       // Checksum (Header to Data)
};

}  // namespace propmaker_escaperoom
