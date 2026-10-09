#include "LogicManager.h"

namespace propmaker_escaperoom {

// ===================== BEGIN =====================

void LogicManager::begin() {
  Serial1.setTX(PIN_RS485_TX);
  Serial1.setRX(PIN_RS485_RX);
  Serial1.begin(PUZZLE_RS485_BAUD);

  for (uint8_t i = 0; i < puzzleCount_; i++) {
    puzzles_[i]->begin();
    puzzles_[i]->setStateChangeCallback(onPuzzleStateChanged, this);
    previousStates_[i] = puzzles_[i]->getState();
  }

  buildLevelMap();
}

// ===================== UPDATE =====================

void LogicManager::update() {
  processNetwork();
  updateTimer();

  if (levelCount_ == 0) {
    // LEGACY MODE: todos os puzzles rodam todo ciclo
    for (uint8_t i = 0; i < puzzleCount_; i++) {
      puzzles_[i]->update();
    }
    return;
  }

  // GAME LEVEL MODE: So, the puzzles that are currently at this level can be solved.
  if (!gameStarted_ || gameComplete_ || currentLevel_ >= levelCount_) return;

  const GameLevelConfig& level = levels_[currentLevel_];

  for (uint8_t i = 0; i < puzzleCount_; i++) {
    if (puzzleLevelMap_[i] != currentLevel_) continue;

    if (level.mode == GameLevelMode::SEQUENCE) {
      uint8_t seqId = level.puzzleIds[levelPosition_[currentLevel_]];
      if (puzzles_[i]->getId() == seqId) {
        puzzles_[i]->update();
      }
    } else {
      puzzles_[i]->update();
    }
  }

  // Poll fallback: detects transitions that weren’t captured by the callback
  for (uint8_t i = 0; i < puzzleCount_; i++) {
    PuzzleState cur = puzzles_[i]->getState();
    if (cur != previousStates_[i]) {
      previousStates_[i] = cur;
      if (cur == PuzzleState::Solved) {
        handlePuzzleStateChange(puzzles_[i], cur);
      }
    }
  }
}

// ===================== GAME LEVEL API =====================

void LogicManager::setGameLevel(const GameLevelConfig* levels, uint8_t count) {
  if (count > MAX_LEVELS) count = MAX_LEVELS;
  levelCount_ = count;
  for (uint8_t i = 0; i < count; i++) {
    levels_[i] = levels[i];
    levelPosition_[i] = 0;
  }
  Serial.printf("[GAME] %d levels configured\n", count);
}

void LogicManager::startGame() {
  if (levelCount_ == 0) return;
  gameStarted_ = true;
  gameComplete_ = false;
  currentLevel_ = 0;
  activateLevel(0);
  onGameLevelEnter(0);
  Serial.printf("[GAME] Started at level 0\n");
}

// ===================== LEVEL TRANSITIONS =====================

void LogicManager::buildLevelMap() {
  for (uint8_t i = 0; i < MAX_PUZZLES; i++) {
    puzzleLevelMap_[i] = 0xFF;
  }
  for (uint8_t l = 0; l < levelCount_; l++) {
    for (uint8_t j = 0; j < levels_[l].puzzleCount; j++) {
      uint8_t pid = levels_[l].puzzleIds[j];
      uint8_t idx = findPuzzleIndex(pid);
      if (idx < MAX_PUZZLES) {
        puzzleLevelMap_[idx] = l;
      }
    }
  }
}

bool LogicManager::isLevelComplete(uint8_t levelIdx) {
  if (levelIdx >= levelCount_) return true;

  const GameLevelConfig& level = levels_[levelIdx];

  if (level.mode == GameLevelMode::SEQUENCE) {
    return levelPosition_[levelIdx] >= level.puzzleCount;
  }

  // PARALLEL: All puzzles in this level must be Solved.
  for (uint8_t i = 0; i < puzzleCount_; i++) {
    if (puzzleLevelMap_[i] == levelIdx && puzzles_[i]->getState() != PuzzleState::Solved) {
      return false;
    }
  }
  return true;
}

void LogicManager::activateLevel(uint8_t levelIdx) {
  if (levelIdx >= levelCount_) return;

  const GameLevelConfig& level = levels_[levelIdx];
  levelPosition_[levelIdx] = 0;

  if (level.mode == GameLevelMode::PARALLEL) {
    for (uint8_t i = 0; i < puzzleCount_; i++) {
      if (puzzleLevelMap_[i] == levelIdx) {
        puzzles_[i]->run();
      }
    }
  } else {
    activateSequentialPuzzle(levelIdx, 0);
  }
}

void LogicManager::activateSequentialPuzzle(uint8_t levelIdx, uint8_t seqPos) {
  if (levelIdx >= levelCount_) return;

  const GameLevelConfig& level = levels_[levelIdx];
  if (seqPos >= level.puzzleCount) return;

  uint8_t pid = level.puzzleIds[seqPos];
  for (uint8_t i = 0; i < puzzleCount_; i++) {
    if (puzzles_[i]->getId() == pid) {
      puzzles_[i]->run();
      break;
    }
  }
}

// ===================== STATE CHANGE CALLBACK =====================

void LogicManager::onPuzzleStateChanged(PuzzleBase* puzzle, PuzzleState oldState, PuzzleState newState, void* userData) {
  (void)oldState;
  auto* self = static_cast<LogicManager*>(userData);
  if (newState == PuzzleState::Solved) {
    self->handlePuzzleStateChange(puzzle, newState);
  }
}

void LogicManager::handlePuzzleStateChange(PuzzleBase* puzzle, PuzzleState newState) {
  if (!gameStarted_ || levelCount_ == 0 || gameComplete_) return;
  if (newState != PuzzleState::Solved) return;

  // notified that the puzzle has been solved
  uint8_t pIdx = findPuzzleIndex(puzzle->getId());
  if (pIdx < MAX_PUZZLES) {
    previousStates_[pIdx] = PuzzleState::Solved;
    onPuzzleSolved(pIdx);
  }

  // Check whether the current level has been completed.
  if (!isLevelComplete(currentLevel_)) {
    // SEQUENCE: Move forward to the next puzzle within the same level
    if (levels_[currentLevel_].mode == GameLevelMode::SEQUENCE) {
      levelPosition_[currentLevel_]++;
      if (!isLevelComplete(currentLevel_)) {
        activateSequentialPuzzle(currentLevel_, levelPosition_[currentLevel_]);
      }
    }
  }

  if (isLevelComplete(currentLevel_)) {
    onGameLevelExit(currentLevel_);
    currentLevel_++;
    if (currentLevel_ >= levelCount_) {
      gameComplete_ = true;
      Serial.println("[GAME] All levels complete!");
      onGameComplete();
    } else {
      Serial.printf("[GAME] Advancing to level %d\n", currentLevel_);
      activateLevel(currentLevel_);
      onGameLevelEnter(currentLevel_);
    }
  }
}

// ===================== HELPER =====================

uint8_t LogicManager::findPuzzleIndex(uint8_t puzzleId) const {
  for (uint8_t i = 0; i < puzzleCount_; i++) {
    if (puzzles_[i]->getId() == puzzleId) {
      return i;
    }
  }
  return 0xFF;
}

// ===================== TIMER =====================

void LogicManager::updateTimer() {
  if (!timerRunning_) return;

  if (millis() - lastTickAt_ >= 1000) {
    lastTickAt_ += 1000;
    if (countdownSeconds_ > 0) {
      countdownSeconds_--;

      hw.display().printTime(countdownSeconds_ / 60, countdownSeconds_ % 60);

      if (isMaster_ && networkEnabled_) {
        broadcastTimer();
      }

      if (countdownSeconds_ == 0) {
        Serial.println("TIME UP!");
        timerRunning_ = false;
        if (isMaster_) {
            sendPacket(0, 0, PuzzleState::Solved, NetCommand::SyncTimer, 0);
        }
      }
    }
  }
}

// ===================== NETWORK =====================

void LogicManager::processNetwork() {
  if (!networkEnabled_) return;
  if (Serial1.available() >= sizeof(Rs485Payload)) {
    if (Serial1.peek() != 0xAA) { Serial1.read(); return; }

    Rs485Payload in;
    Serial1.readBytes((uint8_t*)&in, sizeof(Rs485Payload));

    uint8_t check = in.header + in.targetId + in.sourceId + in.puzzleId + (uint8_t)in.pState + (uint8_t)in.pCommand + (in.data & 0xFF) + (in.data >> 8);
    if (check != in.checksum) return;

    if (in.targetId == hwId_ || in.targetId == 0) {
      Serial.printf("[NET] Packet Received (Src:%d, Target:%d, Cmd:%d, Data:%d)\n", in.sourceId, in.targetId, (int)in.pCommand, in.data);

      if (in.pCommand == NetCommand::SyncTimer && !isMaster_) {
        this->countdownSeconds_ = in.data;
        this->timerRunning_ = (in.data > 0);
        hw.display().printTime(countdownSeconds_ / 60, countdownSeconds_ % 60);
      }

      for (uint8_t i = 0; i < puzzleCount_; i++) {
        if (in.puzzleId == puzzles_[i]->getId() || in.puzzleId == 0) {
          if (in.pCommand == NetCommand::SetProperty) puzzles_[i]->setProperty(in.data);
          else if (in.pCommand == NetCommand::ForceSolve) puzzles_[i]->setState(PuzzleState::Solved);
        }

        puzzles_[i]->onNetworkPacket(in);
      }
    }
  }
}

void LogicManager::broadcastTimer() {
    sendPacket(0, 0, (timerRunning_ ? PuzzleState::Running : PuzzleState::Solved), NetCommand::SyncTimer, countdownSeconds_);
}

void LogicManager::broadcastStatus(uint8_t puzId) {
    for (uint8_t i = 0; i < puzzleCount_; i++) {
        if (puzId == 0 || puzzles_[i]->getId() == puzId) {
            sendPacket(0, puzzles_[i]->getId(), puzzles_[i]->getState(), NetCommand::StatusReport, 0);
        }
    }
}

void LogicManager::sendPacket(uint8_t target, uint8_t puzId, PuzzleState state, NetCommand cmd, uint16_t data) {
    if (!networkEnabled_) return;
    Rs485Payload p;
    p.header = 0xAA;
    p.targetId = target;
    p.sourceId = hwId_;
    p.puzzleId = puzId;
    p.pState = state;
    p.pCommand = cmd;
    p.data = data;
    p.checksum = p.header + p.targetId + p.sourceId + p.puzzleId + (uint8_t)p.pState + (uint8_t)p.pCommand + (p.data & 0xFF) + (p.data >> 8);
    Serial.printf("[NET] Sending Packet to Target:%d (Cmd:%d, Data:%d)\n", target, (int)cmd, data);
    Serial1.write((const uint8_t*)&p, sizeof(Rs485Payload));
}

} // namespace propmaker_escaperoom
