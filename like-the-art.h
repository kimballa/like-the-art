// (c) Copyright 2022 Aaron Kimball

#ifndef _LIKE_THE_ART_H
#define _LIKE_THE_ART_H

#define DEBUG
#define DBG_STD_STRING
//#define DBG_WAIT_FOR_CONNECT
//#define DBG_START_PAUSED

#include <Arduino.h>
#include <string>
#include <vector>

#include <I2CParallel.h>
#include <dbg.h>

using namespace std;

#include "samd51pwm.h"
#include "sign.h"
#include "sentence.h"

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
  EF_FRITZING_ART,   // Like GLOW, but the word "ART" zaps in and out randomly, like neon on the fritz.
                     // Only valid for sentences with 'ART' in them.

};

#endif /* _LIKE_THE_ART_H */
