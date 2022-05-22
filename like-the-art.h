// (c) Copyright 2022 Aaron Kimball

#ifndef _LIKE_THE_ART_H
#define _LIKE_THE_ART_H

#define DEBUG
#define DBG_STD_STRING
#define DBG_WAIT_FOR_CONNECT
#define DBG_START_PAUSED

#include <Arduino.h>
#include <string>
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

enum Effect {
  EF_APPEAR,         // Just turn on the words and hold them there.
  EF_GLOW,           // Fade up from nothing, hold high, fade back to zero.
  EF_BLINK,          // Behold the cursed <blink> tag!
  EF_ONE_AT_A_TIME,  // Highlight each word in series, turning off word n before showing n+1.
  EF_BUILD,          // Light up incrementally more words one at a time left-to-right
  EF_SNAKE,          // Like BUILD, but also "unbuild" by then turning off the 1st word, then the
                     // 2nd... until all is dark.
  EF_SLIDE_TO_END,   // Make a light pulse "zip" through all the words until reaching the last
                     // word in the phrase, then that word stays on. Then zip the 2nd-to-last
                     // word...

// TODO(aaron): These are effectively layered effects that should be possible to 'or' on top of
// the preceeding effects.
  EF_FRITZING_ART,   // Like GLOW, but the word "ART" zaps in and out randomly, like neon on the fritz.
                     // Only valid for sentences with 'ART' in them.
  EF_FRITZING_DONT,  // Like FRITZING_ART, but the word "DON'T" is what's zapping in and out.
  EF_ALT_LOVE_HATE,  // Alternate lighting the "LOVE" and "HATE" words.
};

constexpr unsigned int NUM_EFFECTS = (unsigned int)(Effect::EF_ALT_LOVE_HATE) + 1;

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
