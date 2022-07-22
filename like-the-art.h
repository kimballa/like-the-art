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
#include <Adafruit_SleepyDog.h>
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
#include "darkSensor.h"


// Where is the Arduino installed?
// Set this to be true for production, false for breadboard.
constexpr bool IS_TARGET_PRODUCTION = true;

// Is the WDT enabled to enforce reboots on a jam?
constexpr bool WATCHDOG_ENABLED = true;

// Set REPORT_ANALOG_DARK_SENSOR to true if you want the analog read value
// of the DARK sensor (avg of AVG_NUM_DARK_SAMPLES readings) reported on
// the debug console.
constexpr bool REPORT_ANALOG_DARK_SENSOR = false;

// Number of DARK readings to average together to get a useful reading.
constexpr uint8_t AVG_NUM_DARK_SAMPLES = 32;

/** Every loop iteration lasts for 10ms. */
constexpr unsigned int LOOP_MICROS = 10 * 1000;
constexpr unsigned int LOOP_MILLIS = LOOP_MICROS / 1000;

/** The Watchdog timer resets the MCU if not pinged once per 2 seconds. */
constexpr unsigned int WATCHDOG_TIMEOUT_MILLIS = 2000;

// The main loop selects either the main YDHTLATA! sentence or a different random sentence.
// It chooses a random number X in [0, MAX_TEMP) and if X < main_sentence_temperature, the
// main sentence is chosen. Otherwise, the temperature rises by TEMPERATURE_INCREMENT for
// the next sentence choice.
constexpr unsigned int MAIN_SENTENCE_BASE_TEMPERATURE = 200;
constexpr unsigned int TEMPERATURE_INCREMENT = 50;
constexpr unsigned int MAIN_SENTENCE_MAX_TEMPERATURE = 1000;


/** "Lock in" the specified effect for the next few seconds. */
void lockEffect(const Effect e);
/** "Lock in" the specified sentence for the next few seconds. */
void lockSentence(const unsigned int sentenceId);

/** Set animation parameters to use after current animation finishes. */
void setOnDeckAnimationParams(unsigned int sentenceId, Effect ef, uint32_t flags);
void clearOnDeckAnimationParams();

// The top-level state machine of the system: it's either running, waiting for nightfall, or in
// admin mode. Other state machines controlling LED signs, etc. are only valid in certain macro
// states.
enum class MacroState: unsigned int {
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
