#pragma once

#include <Arduino.h>

namespace propmaker_escaperoom {

/**
 * Wrapper for the RotaryDial unified library (rotating dial of telephones).
 * Prove simple interfaces for reading entered digits.
 *
 * Supported modes (defined in the library):
 *   ROTARYDIAL_UNIFIED_MECHANICAL (default) — contatos metalicos
 *   ROTARYDIAL_UNIFIED_OPTICAL            — encoder otico
 *
 * External dependence:
 *   Biblioteca RotaryDial v1.0.0+ em Documents/Arduino/libraries/RotaryDial
 *
 * Hardware wiring:
 *   Connect the NC (Normal Closed) terminal of the disk to the GPIO pin.
 *   - Conecte o outro terminal ao GND
 *   - Use INPUT_PULLUP ou INPUT (dependendo do modelo de telefone)
 */
class RotaryDialReader {
 public:
  RotaryDialReader() : initialized_(false), lastPin_(0) {}

  /**
   * Initializes the rotary disk reader in the standard mode (MECHANICAL).
   * @param pin The pin number of the GPIO pin connected to the NC terminal of the disk.
   * @return true has been successfully initialized.
   */
  bool begin(uint8_t pin);

  /**
   * The reader is initialized in optical mode, with properly calibrated timings.
   * @param pin       The pin on GPIO that is connected to the optical sensor
   * @param debounce  Piso (ms) — filtra ruido e pulsos da volta (ex: 50)
   * @param maxgap    Maximum gap in milliseconds — 0 means no limit (recommended for Djair disks)
   * @param digitend The timeout period (in milliseconds) for closing the digit (e.g., 800)
   * @return true has been successfully initialized.
   */
  bool beginOptical(uint8_t pin, unsigned long debounce, unsigned long maxgap, unsigned long digitend);

  /**
   * Check whether there is a digit available for reading.
   * @return true if the digit has been dialed and is ready to use
   */
  bool available();

  /**
   * The digits are marked with a dash.
   * @return Printed digits (0-9), or -1 if not available
   */
  int read();

  /** Zera buffer interno do RotaryDial (pulseCount, firstPulse, etc). */
  void reset();

  /**
   * Check whether it has been initialized.
   */
  bool isInitialized() const { return initialized_; }

  /**
   * Returns the configured pin.
   */
  uint8_t getPin() const { return lastPin_; }

 private:
  bool initialized_;
  uint8_t lastPin_;
};

}  // namespace propmaker_escaperoom
