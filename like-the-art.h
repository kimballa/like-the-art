// (c) Copyright 2022 Aaron Kimball

#ifndef _LIKE_THE_ART_H
#define _LIKE_THE_ART_H

#define DEBUG
#define DBG_STD_STRING
//#define DBG_WAIT_FOR_CONNECT
//#define DBG_START_PAUSED

#include <Arduino.h>
#include <vector>

#include <I2CParallel.h>
#include <Adafruit_NeoPixel.h>
#include <dbg.h>

using namespace std;

#include "lib/samd51pwm.h"
#include "lib/smarteeprom.h"
#include "sign.h"
#include "sentence.h"
#include "buttons.h"
#include "adminState.h"
#include "saveconfig.h"
#include "animation.h"


// Define this to be 1 for production, 0 for breadboard.
#define IS_TARGET_PRODUCTION 1

// Where is the Arduino installed? Valid values are 'Breadboard' or 'Production'
#if IS_TARGET_PRODUCTION == 1
  #define TARGET_INSTALL Production
#else
  #define TARGET_INSTALL Breadboard
#endif // if IS_TARGET_PRODUCTION

// Define REPORT_ANALOG_DARK_SENSOR if you want the analog read value
// of the DARK sensor (avg of AVG_NUM_DARK_SAMPLES readings) reported on
// the debug console.
#define REPORT_ANALOG_DARK_SENSOR

// Number of DARK readings to average together to get a useful reading.
constexpr uint8_t AVG_NUM_DARK_SAMPLES = 32;

/** Every loop iteration lasts for 10ms. */
constexpr unsigned int LOOP_MICROS = 10 * 1000;
constexpr unsigned int LOOP_MILLIS = LOOP_MICROS / 1000;

// The top-level state machine of the system: it's either running, waiting for nightfall, or in
// admin mode. Other state machines controlling LED signs, etc. are only valid in certain macro
// states.
enum MacroState {
  MS_RUNNING,       // Default "go" state
  MS_ADMIN,         // "Admin" mode entered for manual operator control.
  MS_WAITING,       // Waiting for nightfall; idle system.
};

/** The top-level state. Do not set this directly; call setMacroState{Running|Waiting|Admin}(). */
extern MacroState macroState;

/** Switch to MS_RUNNING MacroState. */
extern void setMacroStateRunning();
/** Switch to MS_WAITING MacroState. */
extern void setMacroStateWaiting();

/** The global PWM timer. */
extern PwmTimer pwmTimer;

/**
 * Pack r/g/b channels for a neopixel into a 32-bit word.
 */
inline static constexpr uint32_t neoPixelColor(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

#endif /* _LIKE_THE_ART_H */
