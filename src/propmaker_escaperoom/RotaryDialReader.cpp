#include "RotaryDialReader.h"
#include <RotaryDial.h>

namespace propmaker_escaperoom {

bool RotaryDialReader::begin(uint8_t pin) {
    // Standard mode: Mechanic
    RotaryDial::begin(pin, ROTARYDIAL_MECHANICAL);
    lastPin_ = pin;
    initialized_ = true;
    Serial.printf("[ROTARY] Initialized at pin %d (MECHANICAL)\n", pin);
    Serial.printf("[ROTARY]   Debounce=15ms  MaxGap=0  DigitEnd=600ms\n");
    return true;
}

bool RotaryDialReader::beginOptical(uint8_t pin, unsigned long debounce, unsigned long maxgap, unsigned long digitend) {
    // It starts in optical mode with the corrected library.
    RotaryDial::begin(pin, ROTARYDIAL_OPTICAL);
    // Apply the calibrated values (if they differ from the default value of 50/0/800).
    if (debounce != 50)  RotaryDial::setDebounce(debounce);
    if (maxgap != 0)     RotaryDial::setMaxGap(maxgap);
    if (digitend != 800) RotaryDial::setDigitEnd(digitend);

    lastPin_ = pin;
    initialized_ = true;
    Serial.printf("[ROTARY] Initialized at pin %d (OPTICAL)\n", pin);
    Serial.printf("[ROTARY]   Debounce=%lums  MaxGap=%lu  DigitEnd=%lums\n", debounce, maxgap, digitend);
    return true;
}

bool RotaryDialReader::available() {
    if (!initialized_) return false;
    return RotaryDial::available();
}

int RotaryDialReader::read() {
    if (!initialized_) return -1;
    // A biblioteca ja decodifica: 10 pulsos → 0, 1-9 → 1-9
    return RotaryDial::read();
}

void RotaryDialReader::reset() {
    if (!initialized_) return;
    RotaryDial::reset();
}

}  // namespace propmaker_escaperoom
