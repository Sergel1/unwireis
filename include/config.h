#pragma once
#include <stdint.h>

namespace cfg {

//  GPIO (original ESP32
constexpr int PIN_I2S_BCK   = 26;  // PCM5102 BCK
constexpr int PIN_I2S_WS    = 25;  // PCM5102 LCK
constexpr int PIN_I2S_DOUT  = 22;  // PCM5102 DIN
constexpr int PIN_DAC_XSMT  = 27;  // high = audio, low = soft mute
constexpr int PIN_MUTE_SW   = 33;  // slide switch to GND, INPUT_PULLUP
constexpr int PIN_BUTTON    = 32;  // tact button to GND, INPUT_PULLUP, deep-sleep wake
constexpr int PIN_LED_BLUE  = 18;
constexpr int PIN_LED_RED   = 19;
constexpr int PIN_BAT_SENSE = 34;  // ADC1, 100k/100k divider from the switched battery rail
constexpr int PIN_MOD_ID    = 35;  // ADC1, output-module ID line
constexpr int PIN_ID_PULLUP = 21;  // drives the 10k pull-up only while sampling

//LED timing
constexpr uint32_t LED_HANDSHAKE_MS      = 5000;  // blue breathing after connect
constexpr uint32_t LED_BREATHE_PERIOD_MS = 2000;
constexpr uint32_t LED_LOWBATT_PERIOD_MS = 4000;

// Button timing
constexpr uint32_t DEBOUNCE_MS     = 30;
constexpr uint32_t CLICK_WINDOW_MS = 350;
constexpr uint32_t LONG_PRESS_MS   = 1000;  // hold, reported on release
constexpr uint32_t PAIR_HOLD_MS    = 5000;  // pairing mode, reported while still held

// Sleep
constexpr uint32_t IDLE_SLEEP_MS = 10UL * 60UL * 1000UL;

// Battery
constexpr float    LOW_BATT_V       = 3.60f;
constexpr float    LOW_BATT_CLEAR_V = 3.70f;
constexpr float    SHUTDOWN_V       = 3.45f;
constexpr uint32_t BATT_FILTER_MS   = 5000;   // filter time constant
constexpr uint32_t BATT_HOLD_MS     = 10000;  // time below a threshold before it counts


constexpr uint8_t VOL_CAP_GENERIC = 60;  // percent, for unknown or generic modules

} 
