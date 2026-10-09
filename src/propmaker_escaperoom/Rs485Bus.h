#pragma once

#include <Arduino.h>

#include "PlatformPins.h"
#include "PuzzleState.h"

namespace propmaker_escaperoom {

class Rs485Bus {
 public:
  void begin(uint8_t nodeId, const char *puzzleType) {
    nodeId_ = nodeId;
    puzzleType_ = puzzleType;

#if PUZZLE_ENABLE_RS485
    Serial1.setTX(PIN_RS485_TX);
    Serial1.setRX(PIN_RS485_RX);
    Serial1.begin(PUZZLE_RS485_BAUD);
    enabled_ = true;
    sendBootFrame();
#else
    enabled_ = false;
#endif
  }

  void update() {
#if PUZZLE_ENABLE_RS485
    if (enabled_ && millis() - lastStatusAt_ >= PUZZLE_RS485_STATUS_INTERVAL_MS) {
      sendStatusFrame(lastState_, solved_);
    }
#endif
  }

  void setState(PuzzleState state, bool solved) {
    lastState_ = state;
    solved_ = solved;
#if PUZZLE_ENABLE_RS485
    if (enabled_) {
      sendStatusFrame(lastState_, solved_);
    }
#endif
  }

  void sendEvent(const char *eventName) {
#if PUZZLE_ENABLE_RS485
    if (enabled_) {
      writeFrame(F("EVENT"), eventName);
    }
#else
    (void)eventName;
#endif
  }

 private:
  void sendBootFrame() {
    writeFrame(F("BOOT"), puzzleType_);
  }

  void sendStatusFrame(PuzzleState state, bool solved) {
    char payload[96];
    snprintf(payload, sizeof(payload), "%s,%u,%lu", solved ? "SOLVED" : "UNSOLVED", static_cast<unsigned>(state),
             millis());
    writeFrame(F("STATUS"), payload);
    lastStatusAt_ = millis();
  }

  void writeFrame(const __FlashStringHelper *kind, const char *payload) {
    Serial1.print(F("$PZ,"));
    Serial1.print(nodeId_);
    Serial1.print(F(","));
    Serial1.print(puzzleType_);
    Serial1.print(F(","));
    Serial1.print(kind);
    Serial1.print(F(","));
    Serial1.println(payload);
    Serial1.flush();
  }

  uint8_t nodeId_ = 0;
  const char *puzzleType_ = "PUZZLE";
  PuzzleState lastState_ = PuzzleState::Initialising;
  bool solved_ = false;
  bool enabled_ = false;
  unsigned long lastStatusAt_ = 0;
};

}  // namespace propmaker_escaperoom
