#pragma once

/**
 * PropMakerEscapeRoom - Complete engine for Escape Room puzzles
 * Single header needed to use all features.
 *
 * Automatically includes:
 * - ArduinoJson (JSON parsing)
 * - IoAbstraction (PCF8574/PCF8575 I/O expanders)
 * - TaskManagerIO (Keypad scanning)
 * - arduino-audio-tools + arduino-libhelix (MP3 playback)
 * - Adafruit NeoPixel (LED strips)
 * - Adafruit LED Backpack (HT16K33 displays)
 * - Adafruit LIS3DH (Accelerometer)
 * - ADS1X15 (ADC converter)
 * - PCA9685 (PWM driver)
 * - MFRC522 (RFID readers - optional)
 */

// External libraries (wrappers below)
#include <ArduinoJson.h>
#include <IoAbstraction.h>
#include <TaskManagerIO.h>
#include <SwitchInput.h>
#include <KeyboardManager.h>

// Platform and utilities
#include "propmaker_escaperoom/PlatformPins.h"
#include "propmaker_escaperoom/PuzzleState.h"
#include "propmaker_escaperoom/Rs485Payload.h"

// Hardware drivers (use helper libraries)
#include "propmaker_escaperoom/AudioPlayer.h"              // arduino-audio-tools
#include "propmaker_escaperoom/Display14Seg.h"             // Adafruit LED Backpack
#include "propmaker_escaperoom/Pcf857x.h"                  // IoAbstraction PCF857x wrapper
#include "propmaker_escaperoom/RotaryEncoder.h"            // GPIO native
#include "propmaker_escaperoom/RotaryDialUnifiedReader.h"         // RotaryDial library wrapper
#include "propmaker_escaperoom/Rs485Bus.h"                   // Serial native

// Core hardware and puzzle framework
#include "propmaker_escaperoom/Hardware.h"
#include "propmaker_escaperoom/NfcReader.h"
#include "propmaker_escaperoom/PuzzleBase.h"
#include "propmaker_escaperoom/GameLevelConfig.h"
#include "propmaker_escaperoom/LogicManager.h"
#include "propmaker_escaperoom/SerialDebug.h"

// RFID - optional (only includes if MFRC522.h is available)
#if __has_include(<MFRC522.h>)
#include "propmaker_escaperoom/RfidBus.h"
#endif
