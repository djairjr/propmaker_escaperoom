#pragma once

#include <Arduino.h>

namespace propmaker_escaperoom {

constexpr int PIN_SPI_MISO = 8;
constexpr int PIN_SD_CS = 9;
constexpr int PIN_RELAY = 10;

constexpr int PIN_RS485_TX = 0;
constexpr int PIN_RS485_RX = 1;
constexpr int PIN_IO2 = 2;
constexpr int PIN_IO3 = 3;
constexpr int PIN_IO4 = 4;
constexpr int PIN_IO5 = 5;
constexpr int PIN_IO6 = 6;
constexpr int PIN_IO7 = 7;
constexpr int PIN_IO11 = 11;
constexpr int PIN_IO13 = 13;
constexpr int PIN_IO24 = 24;
constexpr int PIN_IO25 = 25;

constexpr int PIN_SPI_SCK = 14;
constexpr int PIN_SPI_MOSI = 15;

#ifdef PIN_I2S_DATA
#undef PIN_I2S_DATA
#endif
#ifdef PIN_I2S_BCLK
#undef PIN_I2S_BCLK
#endif
#ifdef PIN_I2S_WS
#undef PIN_I2S_WS
#endif
#ifdef PIN_NEOPIXEL
#undef PIN_NEOPIXEL
#endif
#ifdef PIN_NEOPIXEL_INTERNAL
#undef PIN_NEOPIXEL_INTERNAL
#endif
#ifdef PIN_EXTERNAL_NEOPIXEL
#undef PIN_EXTERNAL_NEOPIXEL
#endif
#ifdef PIN_EXTERNAL_POWER
#undef PIN_EXTERNAL_POWER
#endif
#ifdef PIN_SPI_MISO
#undef PIN_SPI_MISO
#endif
#ifdef PIN_SD_CS
#undef PIN_SD_CS
#endif
#ifdef PIN_RELAY
#undef PIN_RELAY
#endif
#ifdef PIN_SPI_SCK
#undef PIN_SPI_SCK
#endif
#ifdef PIN_SPI_MOSI
#undef PIN_SPI_MOSI
#endif
#ifdef PIN_BTN
#undef PIN_BTN
#endif
#ifdef PIN_SERVO_PWM
#undef PIN_SERVO_PWM
#endif
#ifdef PIN_RS485_TX
#undef PIN_RS485_TX
#endif
#ifdef PIN_RS485_RX
#undef PIN_RS485_RX
#endif
constexpr int PIN_I2S_DATA = 16;
constexpr int PIN_I2S_BCLK = 17;
constexpr int PIN_I2S_WS = 18;
constexpr int PIN_BTN = 19;
constexpr int PIN_SERVO_PWM = 20;
constexpr int PIN_NEOPIXEL_INTERNAL = 4;
constexpr int PIN_EXTERNAL_NEOPIXEL = 21;
constexpr int PIN_NEOPIXEL = PIN_EXTERNAL_NEOPIXEL;  // Aliases for compatibility
constexpr int PIN_EXTERNAL_POWER = 23;
constexpr int PIN_BATTERY_SENSE = 26;  // A0 / GPIO26

constexpr uint8_t I2C_DEFAULT_ADDRESS = 0x20;
constexpr unsigned long DOOR_OPEN_MS = 5000;

}  // namespace propmaker_escaperoom

#ifndef PUZZLE_ENABLE_RS485
#define PUZZLE_ENABLE_RS485 0
#endif

#ifndef PUZZLE_RS485_BAUD
#define PUZZLE_RS485_BAUD 115200
#endif

#ifndef PUZZLE_RS485_STATUS_INTERVAL_MS
#define PUZZLE_RS485_STATUS_INTERVAL_MS 1000UL
#endif

#ifndef PUZZLE_RS485_NODE_ID
#define PUZZLE_RS485_NODE_ID 1
#endif

// Temporary aliases to facilitate the migration of the inherited sketches.
constexpr int PUZZLE_PIN_SPI_MISO = propmaker_escaperoom::PIN_SPI_MISO;
constexpr int PUZZLE_PIN_SD_CS = propmaker_escaperoom::PIN_SD_CS;
constexpr int PUZZLE_PIN_RELAY = propmaker_escaperoom::PIN_RELAY;
constexpr int PUZZLE_PIN_RS485_TX = propmaker_escaperoom::PIN_RS485_TX;
constexpr int PUZZLE_PIN_RS485_RX = propmaker_escaperoom::PIN_RS485_RX;
constexpr int PUZZLE_PIN_IO2 = propmaker_escaperoom::PIN_IO2;
constexpr int PUZZLE_PIN_IO3 = propmaker_escaperoom::PIN_IO3;
constexpr int PUZZLE_PIN_IO4 = propmaker_escaperoom::PIN_IO4;
constexpr int PUZZLE_PIN_IO5 = propmaker_escaperoom::PIN_IO5;
constexpr int PUZZLE_PIN_IO6 = propmaker_escaperoom::PIN_IO6;
constexpr int PUZZLE_PIN_IO7 = propmaker_escaperoom::PIN_IO7;
constexpr int PUZZLE_PIN_IO11 = propmaker_escaperoom::PIN_IO11;
constexpr int PUZZLE_PIN_IO13 = propmaker_escaperoom::PIN_IO13;
constexpr int PUZZLE_PIN_IO24 = propmaker_escaperoom::PIN_IO24;
constexpr int PUZZLE_PIN_IO25 = propmaker_escaperoom::PIN_IO25;
constexpr int PUZZLE_PIN_SPI_SCK = propmaker_escaperoom::PIN_SPI_SCK;
constexpr int PUZZLE_PIN_SPI_MOSI = propmaker_escaperoom::PIN_SPI_MOSI;
constexpr int PUZZLE_PIN_I2S_DATA = propmaker_escaperoom::PIN_I2S_DATA;
constexpr int PUZZLE_PIN_I2S_BCLK = propmaker_escaperoom::PIN_I2S_BCLK;
constexpr int PUZZLE_PIN_I2S_WS = propmaker_escaperoom::PIN_I2S_WS;
constexpr int PUZZLE_PIN_BTN = propmaker_escaperoom::PIN_BTN;
constexpr int PUZZLE_PIN_SERVO_PWM = propmaker_escaperoom::PIN_SERVO_PWM;
constexpr int PUZZLE_PIN_NEOPIXEL_INTERNAL = propmaker_escaperoom::PIN_NEOPIXEL_INTERNAL;
constexpr int PUZZLE_PIN_EXTERNAL_NEOPIXEL = propmaker_escaperoom::PIN_EXTERNAL_NEOPIXEL;
constexpr int PUZZLE_PIN_NEOPIXEL = propmaker_escaperoom::PIN_NEOPIXEL;  // Alias externo
constexpr int PUZZLE_PIN_EXTERNAL_POWER = propmaker_escaperoom::PIN_EXTERNAL_POWER;
