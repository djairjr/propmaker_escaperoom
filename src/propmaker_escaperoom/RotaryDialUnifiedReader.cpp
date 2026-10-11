#include "RotaryDialUnifiedReader.h"
#include <RotaryDialUnified.h>

namespace propmaker_escaperoom {

bool RotaryDialUnifiedReader::begin(uint8_t pin) {
    // Standard mode: Mechanic
    RotaryDialUnified::begin(pin, ROTARYDIAL_UNIFIED_MECHANICAL);
    lastPin_ = pin;
    initialized_ = true;
    Serial.printf("[ROTARY] Initialized at pin %d (MECHANICAL)\n", pin);
    Serial.printf("[ROTARY]   Debounce=15ms  MaxGap=0  DigitEnd=600ms\n");
    return true;
}

bool RotaryDialUnifiedReader::beginOptical(uint8_t pin, unsigned long debounce, unsigned long maxgap, unsigned long digitend) {
    // It starts in optical mode with the corrected library.
    RotaryDialUnified::begin(pin, ROTARYDIAL_UNIFIED_OPTICAL);
    // Apply the calibrated values (if they differ from the default value of 50/0/800).
    if (debounce != 50)  RotaryDialUnified::setDebounce(debounce);
    if (maxgap != 0)     RotaryDialUnified::setMaxGap(maxgap);
    if (digitend != 800) RotaryDialUnified::setDigitEnd(digitend);

    lastPin_ = pin;
    initialized_ = true;
    Serial.printf("[ROTARY] Initialized at pin %d (OPTICAL)\n", pin);
    Serial.printf("[ROTARY]   Debounce=%lums  MaxGap=%lu  DigitEnd=%lums\n", debounce, maxgap, digitend);
    return true;
}

bool RotaryDialUnifiedReader::available() {
    if (!initialized_) return false;
    return RotaryDialUnified::available();
}

int RotaryDialUnifiedReader::read() {
    if (!initialized_) return -1;
    // A biblioteca ja decodifica: 10 pulsos → 0, 1-9 → 1-9
    return RotaryDialUnified::read();
}

void RotaryDialUnifiedReader::reset() {
    if (!initialized_) return;
    RotaryDialUnified::reset();
}

}  // namespace propmaker_escaperoom
