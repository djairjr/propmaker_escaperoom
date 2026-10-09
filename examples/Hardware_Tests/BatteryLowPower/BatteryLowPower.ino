/*
  BatteryLowPower

  Demonstra o Battery Monitoring e LowPower Mode da biblioteca.

  Hardware necessario:
  - PropMaker RP2040 com display HT16K33 (opcional)
  - External NeoPixel cable (optional)
  2:1 voltage divider (2 x 100Kohms) between BAT and GND.
    with the center connected to pin A0 (GPIO26)

  Comportamento:
  Display battery voltage on the screen and via serial port.
  - Anima NeoPixels em arco-iris
  With LowPower enabled, brightness and volume can be adjusted accordingly.
    Battery discharge (proportional dimming)
  - Abaixo de 3.5V, EXTERNAL_POWER e cortado

  Monte o divisor no A0:
    BAT ---[100K]---[100K]--- GND
                      |
                      A0

  Use a variable font to simulate battery discharge.
*/

#include <PropMakerEscapeRoom.h>

// ============================================
// Battery management + power saving
// ============================================
class BatteryDemo {
public:
  void begin() {
    hw.begin();

    // Attenuators (adjusted according to your hardware)
    if (hw.hasExpander() == false) {
      hw.addPcf8575(0x20);
    }

    // Inicia monitoramento no A0
    hw.beginBatteryMonitor();

    // Configure thresholds (more aggressive for demo purposes)
    hw.setPowerSaveThresholds(3.8, 3.5);

    // Ativa power save
    hw.setLowPowerMode(true);

    Serial.println(F("[DEMO] Battery LowPower demo started"));
    Serial.println(F("[DEMO] Conecte fonte variavel ao A0"));
    Serial.println(F("[DEMO] Adjust between 0-4.2V to simulate a battery\n"));
    report();
  }

  void update() {
    unsigned long now = millis();

    // Power save (dimming proporcional)
    hw.updatePowerSave();

    // Animacao NeoPixel
    if (now - lastAnim_ > animInterval_) {
      lastAnim_ = now;
      hue_ = (hue_ + 5) % 360;
      uint32_t c = strip().ColorHSV(hue_ * 182, 255, currentBrightness());
      hw.setLightAll(c);
      hw.setLightInternal(0, hw.pixelsInternal().ColorHSV((hue_ + 180) * 182, 255, currentBrightness()));
    }

    // Update the display every 2 seconds
    if (now - lastDisplay_ > 2000) {
      lastDisplay_ = now;
      if (hw.hasDisplay()) {
        float v = hw.readBatteryVoltage();
        if (hw.isBatteryConnected()) {
          int dispVal = (int)(v * 100);
          hw.display().printNum(dispVal, true);
        } else {
          hw.display().printString("USB ");
        }
      }
      report();
    }

    hw.sound().loop();
  }

private:
  unsigned long lastAnim_ = 0;
  unsigned long lastDisplay_ = 0;
  uint16_t animInterval_ = 30;
  uint16_t hue_ = 0;

  Adafruit_NeoPixel &strip() { return hw.pixelsExternal(); }

  uint8_t currentBrightness() {
    return strip().getBrightness();
  }

  void report() {
    float v = hw.readBatteryVoltage();
    int pct = (int)hw.readBatteryPercent();
    bool bat = hw.isBatteryConnected();
    bool lp = hw.getLowPowerMode();

    Serial.printf("[DEMO] BAT=%.2fV (%d%%) %s | LowPower=%s | NeoBri=%d\n",
      v, pct,
      bat ? "BATTERY" : "USB    ",
      lp ? "ON " : "OFF",
      currentBrightness());
  }
};

BatteryDemo demo;

void setup() {
  uint32_t start = millis();
  while (!Serial && (millis() - start < 3000)) delay(10);

  demo.begin();
}

void loop() {
  demo.update();
}
