// (c) Copyright 2022 Aaron Kimball

#include "like-the-art.h"

// Helper macro to insert repetitive enum cases in debugPrintEffect().
#define PRINT_EFFECT_NAME(e) case Effect::e: DBGPRINT(#e); break;

void debugPrintEffect(const Effect e) {
  switch (e) {
    PRINT_EFFECT_NAME(EF_APPEAR);
    PRINT_EFFECT_NAME(EF_GLOW);
    PRINT_EFFECT_NAME(EF_BLINK);
    PRINT_EFFECT_NAME(EF_BLINK_FAST);
    PRINT_EFFECT_NAME(EF_ONE_AT_A_TIME);
    PRINT_EFFECT_NAME(EF_BUILD);
    PRINT_EFFECT_NAME(EF_BUILD_RANDOM);
    PRINT_EFFECT_NAME(EF_SNAKE);
    PRINT_EFFECT_NAME(EF_SLIDE_TO_END);
    PRINT_EFFECT_NAME(EF_MELT);

    PRINT_EFFECT_NAME(EF_ALL_BRIGHT);
    PRINT_EFFECT_NAME(EF_ALL_DARK);
    PRINT_EFFECT_NAME(EF_FADE_LOVE_HATE);
    PRINT_EFFECT_NAME(EF_NO_EFFECT);
  default:
    DBGPRINTU("Unknown effect id:", (unsigned int)e);
    break;
  }
}

// Return true if the specified effect ends with all words in the ON position.
// (Technically, blinks could end in all-off state, but we can easily snap it back on
// again without breaking the flow of the animation.)
static bool effectEndsAllWordsOn(Effect e) {
  return e == Effect::EF_APPEAR || e == Effect::EF_BLINK || e == Effect::EF_BLINK_FAST
      || e == Effect::EF_BUILD || e == Effect::EF_BUILD_RANDOM;
}


/**
 * Generate a random assortment of animation flags that can be applied to the specified
 * effect and sentence to change the visual impact.
 */
uint32_t newAnimationFlags(Effect e, const Sentence &s) {
  uint32_t flags = 0;

  // Roll the dice to see how many signs should flicker.
  unsigned int flickerProbability = random(FLICKER_LIKELIHOOD_MAX);
  if (flickerProbability < FLICKER_LIKELIHOOD_1) {
    flags |= ANIM_FLAG_FLICKER_COUNT_1; // 1 flickering sign.
  } else if (flickerProbability < FLICKER_LIKELIHOOD_2) {
    flags |= ANIM_FLAG_FLICKER_COUNT_2; // 2 flickering signs.
  } else if (flickerProbability < FLICKER_LIKELIHOOD_3) {
    flags |= ANIM_FLAG_FLICKER_COUNT_3; // 3 flickering signs.
  } else if (flickerProbability < FLICKER_LIKELIHOOD_ALL) {
    flags |= ANIM_FLAG_FULL_SIGN_GLITCH_BRIGHT; // Entire board flickers.
  }

  // Roll for ANIM_FLAG_FADE_LOVE_HATE, if eligible.
  if ((s.getSignBits() & S_LOVE || s.getSignBits() & S_HATE)
      && effectEndsAllWordsOn(e)
      && random(LOVE_HATE_LIKELIHOOD_MAX) > LOVE_HATE_FADE_LIKELIHOOD ) {
    // This sentence does include the word LOVE or HATE; the effect can be extended
    // to include the fade, and we passed the random roll test. Fade from LOVE to HATE
    // (or vice versa).
    flags |= ANIM_FLAG_FADE_LOVE_HATE;
  }

  return flags;
}

Animation::Animation():
    _sentence(0, 0), _effect(Effect::EF_APPEAR), _flags(0), _remainingTime(0), _isRunning(false),
    _phaseDuration(0), _phaseRemainingMillis(0), _phaseCountRemaining(0)
    {
}

// Helper method to setup an intro-hold-outro style animation pattern.
void Animation::_setupIntroHoldOutro(unsigned int introTime, unsigned int holdTime, unsigned int outroTime) {
  _isIntroHoldOutro = true; // set up intro-hold-outro mode.

  // Record the timing specified in the parameters.
  _ihoIntroDuration = introTime;
  _ihoHoldDuration = holdTime;
  _ihoOutroDuration = outroTime;

  _phaseCountRemaining = 3; // definitionally a 3-phase animation.
}

/** Return the optimal duration (in millis) for an Animation of the specified sentence and effect. */
uint32_t Animation::getOptimalDuration(const Sentence &s, const Effect e, const uint32_t flags) {
  uint32_t i;
  uint32_t sentenceBits;
  uint32_t positionSum;

  switch(e) {
  case Effect::EF_APPEAR:
    return 5000;  // Show the sentence for 5 seconds.
  case Effect::EF_GLOW:
    return 5000;  // 1250 ms glow-up, 2500ms hold, 1250 ms glow-down
  case Effect::EF_BLINK:
    // approx 6 seconds total (1s on / 1s off x 3 blinks)
    return durationForBlinkCount(3);
  case Effect::EF_BLINK_FAST:
    // approx 4 seconds total (250ms on / 250 ms off x 8 blinks)
    return durationForFastBlinkCount(8);
  case Effect::EF_ONE_AT_A_TIME:
    // Show each word in sentence by itself for 1 second, followed by 'N' seconds of blank.
    return (s.getNumWords() + ONE_AT_A_TIME_BLANK_PHASES) * ONE_AT_A_TIME_WORD_DELAY;
  case Effect::EF_BUILD:
    // words in sentence light up 1/2 sec apart, and it has a full-sentence hold phase at the end.
    return s.getNumWords() * BUILD_WORD_DELAY + BUILD_HOLD_DURATION;
  case Effect::EF_BUILD_RANDOM:
    return s.getNumWords() * BUILD_RANDOM_WORD_DELAY + BUILD_RANDOM_HOLD_DURATION;
  case Effect::EF_SNAKE:
    // words in sentence light up and tear down 3/4 sec apart.
    return 2 * s.getNumWords() * SNAKE_WORD_DELAY;
  case Effect::EF_SLIDE_TO_END:
    // Each word lights up by zipping through all preceeding words. (O(n^2) behavior.)
    // So the ids/positions of the words in the sentence give the proportion of time required
    // for each word -- plus a 'hold time' once we arrive at the word.
    positionSum = 0;
    sentenceBits = s.getSignBits();
    for (i = 0; i < NUM_SIGNS; i++) {
      if (sentenceBits & (1 << i)) {
        positionSum += i;
      }
    }

    // (zip-in times + per-word holds) + (2s whole-sentence hold) + (zip-out times + per-word holds)
    return (positionSum * SLIDE_TO_END_PER_WORD_ZIP + s.getNumWords() * SLIDE_TO_END_PER_WORD_HOLD)
        + SLIDE_TO_END_DEFAULT_SENTENCE_HOLD
        + (positionSum * SLIDE_TO_END_PER_WORD_ZIP + s.getNumWords() * SLIDE_TO_END_PER_WORD_HOLD);
  case Effect::EF_MELT:
    // time for all words to melt plus full-sign hold time plus blank outro hold time.
    return MELT_ONE_WORD_MILLIS * NUM_SIGNS + MELT_OPTIMAL_HOLD_TIME + MELT_BLANK_TIME;
  case Effect::EF_ALL_BRIGHT:
    return ALL_BRIGHT_MILLIS;
  case Effect::EF_ALL_DARK:
    return ALL_DARK_MILLIS;
  case Effect::EF_FADE_LOVE_HATE:
    return FADE_LOVE_HATE_MILLIS;
  case Effect::EF_NO_EFFECT:
    return 0; // No-effect animation should not occupy any duration.
  default:
    DBGPRINTU("Unknown effect in getOptimalDuration():", (uint32_t)e);
    DBGPRINT("Returning default duration of 2000ms.");
    return 2000;
  }
}

void Animation::_setParamsAppear(const Sentence &s, const Effect e, uint32_t flags,
    uint32_t milliseconds) {
  // Single phase which lasts the entire duration of the effect.
  _phaseDuration = milliseconds;
  _phaseCountRemaining = 1;
  DBGPRINTU("New animation: EF_APPEAR", milliseconds);
}

void Animation::_setParamsGlow(const Sentence &s, const Effect e, uint32_t flags,
    uint32_t milliseconds) {

  uint32_t framesPerPhase;

  // 1/4 the time in phase 0: increasing brightness (fade in)
  // 1/2 the time in phase 1: hold at max brightness
  // 1/4 the time in phase 2: decreasing brightness (fade out)
  _phaseDuration = milliseconds / 4;
  _setupIntroHoldOutro(_phaseDuration, 2 * _phaseDuration, _phaseDuration);

  framesPerPhase = _phaseDuration / LOOP_MILLIS;
  _glowStepSize = getMaxPwmDutyCycle() / framesPerPhase;
  _glowCurrentBrightness = 0;
  DBGPRINTU("New animation: EF_GLOW", milliseconds);
}

void Animation::_setParamsBlink(const Sentence &s, const Effect e, uint32_t flags,
    uint32_t milliseconds) {
  // Simple blinking effect; an even number of phases alternating on & off, of fixed duration.
  _phaseDuration = BLINK_PHASE_MILLIS;
  _phaseCountRemaining = (milliseconds + BLINK_PHASE_MILLIS - 1) / BLINK_PHASE_MILLIS;
  DBGPRINTU("New animation: EF_BLINK", milliseconds);
}
void Animation::_setParamsBlinkFast(const Sentence &s, const Effect e, uint32_t flags,
    uint32_t milliseconds) {
  // Like EF_BLINK but with faster phases.
  _phaseDuration = FAST_BLINK_PHASE_MILLIS;
  _phaseCountRemaining = (milliseconds + FAST_BLINK_PHASE_MILLIS - 1) / FAST_BLINK_PHASE_MILLIS;
  DBGPRINTU("New animation: EF_BLINK_FAST", milliseconds);
}

void Animation::_setParamsOneAtATime(const Sentence &s, const Effect e, uint32_t flags,
    uint32_t milliseconds) {
  // Have N phases where N = number of words in sentence. One word at a time is lit.
  _phaseCountRemaining = s.getNumWords() + ONE_AT_A_TIME_BLANK_PHASES;
  _phaseDuration = milliseconds / _phaseCountRemaining;
  DBGPRINTU("New animation: EF_ONE_AT_A_TIME", milliseconds);
}

void Animation::_setParamsBuild(const Sentence &s, const Effect e, uint32_t flags,
    uint32_t milliseconds) {
  // Have N + M phases where N = number of words in sentence; in phase k the first k words of the
  // sentence are lit. M is the number of 'hold' phases for which the whole sentence remains lit.
  _phaseCountRemaining = s.getNumWords() + BUILD_HOLD_PHASES;
  _phaseDuration = milliseconds / _phaseCountRemaining;
  DBGPRINTU("New animation: EF_BUILD", milliseconds);
}

void Animation::_setParamsBuildRandom(const Sentence &s, const Effect e, uint32_t flags,
    uint32_t milliseconds) {
  // Have N + M phases where N = number of words in sentence; in phase k, a random k words of the
  // sentence are lit. M is the number of 'hold' phases for which the whole sentence remains lit.
  _phaseCountRemaining = s.getNumWords() + BUILD_RANDOM_HOLD_PHASES;
  _phaseDuration = milliseconds / _phaseCountRemaining;

  // Randomize the order we light up the sentence words in this animation session:

  // Step 1: Reset ordering array.
  memset(_buildRandomOrder, 0, sizeof(uint8_t) * NUM_SIGNS);

  // Step 2: Put sign ids into the ordering array sequentially.
  uint32_t signBits = s.getSignBits();
  uint32_t idx = 0;
  for (uint8_t i = 0; i < NUM_SIGNS; i++) {
    if (signBits & (1 << i)) {
      _buildRandomOrder[idx++] = i;
    }
  }

  // Step 3: Shuffle the elements of the array.
  // We have populated the first `numWords` elements of the array.
  uint32_t numWords = s.getNumWords();
  constexpr unsigned int rounds = NUM_SIGNS * 20;
  for (uint32_t i = 0; i < rounds; i++) {
    unsigned int idxA = random(numWords);
    unsigned int idxB = random(numWords);
    uint8_t tmp = _buildRandomOrder[idxA];
    _buildRandomOrder[idxA] = _buildRandomOrder[idxB];
    _buildRandomOrder[idxB] = tmp;
  }

  // ... now the first `numWords` elements of the array contain the sign ids to light up,
  // in a scrambled order. Rely on this during the animation frames.

  DBGPRINTU("New animation: EF_BUILD_RANDOM", milliseconds);
}

void Animation::_setParamsSnake(const Sentence &s, const Effect e, uint32_t flags,
    uint32_t milliseconds) {
  // Like BUILD, but also "unbuild" by then turning off the 1st word, then the
  // 2nd... until all is dark; then hold that for ONE_AT_A_TIME_BLANK_PHASES.
  _phaseCountRemaining = s.getNumWords() * 2;
  _phaseDuration = milliseconds / _phaseCountRemaining;
  DBGPRINTU("New animation: EF_SNAKE", milliseconds);
}

void Animation::_setParamsSlide(const Sentence &s, const Effect e, uint32_t flags,
    uint32_t milliseconds) {

  uint32_t holdPhaseTime;
  uint32_t setupTime;
  uint32_t teardownTime;
  uint32_t positionSum, sentenceBits;
  unsigned int i;

  // Light pulse 'zips' through all words on the board to the last word in the sentence and sticks
  // there. Then another light pulse zips through all words starting @ first to the 2nd to last
  // word in the sentence...
  //
  // There is a brief hold when each zip arrives at its destination word.
  //
  // There is a full hold with the sentence on after they're all illuminated.
  //
  // Then all the word lights "zip back" to left.
  //
  // This effect has fixed timing for light zips and per-word holds; any additional time
  // is used for the full-sentence hold. A full sentence hold of at least 1s is enforced.
  // If `milliseconds` is too short for minimum timing, it will be disregarded.
  positionSum = 0;
  sentenceBits = s.getSignBits();
  for (i = 0; i < NUM_SIGNS; i++) {
    if (sentenceBits & (1 << i)) {
      positionSum += i;
    }
  }

  // (zip-in times + per-word holds) + (2s whole-sentence hold) + (zip-out times + per-word holds)
  setupTime = (positionSum * SLIDE_TO_END_PER_WORD_ZIP + s.getNumWords() * SLIDE_TO_END_PER_WORD_HOLD);
  teardownTime = setupTime;
  if (setupTime + teardownTime + SLIDE_TO_END_MINIMUM_SENTENCE_HOLD > milliseconds) {
    // Enforce a minimum 1s hold phase.
    holdPhaseTime = SLIDE_TO_END_MINIMUM_SENTENCE_HOLD;
  } else {
    // If milliseconds is bigger than the minimum, allocate all the remaining time to hold phase.
    holdPhaseTime = milliseconds - setupTime - teardownTime;
  }
  _setupIntroHoldOutro(setupTime, holdPhaseTime, teardownTime);
  DBGPRINTU("New animation: EF_SLIDE_TO_END", setupTime + holdPhaseTime + teardownTime);
}

void Animation::_setParamsMelt(const Sentence &s, const Effect e, uint32_t flags,
    uint32_t milliseconds) {

  uint32_t numWordsToMelt;
  uint32_t holdPhaseTime;

  // Start with all words on and "melt away" words one-by-one to reveal the
  // real sentence. The sentence holds, and then individual words turn off
  // to fade to black for outro.
  numWordsToMelt = NUM_SIGNS - s.getNumWords();
  uint32_t introTime = numWordsToMelt * MELT_ONE_WORD_MILLIS;
  uint32_t outroTime = s.getNumWords() * MELT_ONE_WORD_MILLIS + MELT_BLANK_TIME;
  if (introTime + outroTime + MELT_MINIMUM_HOLD_TIME >= milliseconds) {
    // This is going to be an over-length animation. Just do a quick hold.
    holdPhaseTime = MELT_MINIMUM_HOLD_TIME;
  } else {
    // All time not spent melting is just in hold.
    holdPhaseTime = milliseconds - introTime - outroTime;
  }

  _setupIntroHoldOutro(introTime, holdPhaseTime, outroTime);

  DBGPRINTU("New animation: EF_MELT", introTime + holdPhaseTime + outroTime);
}

void Animation::_setParamsAllBright(const Sentence &s, const Effect e, uint32_t flags,
    uint32_t milliseconds) {
  _phaseDuration = milliseconds;
  _phaseCountRemaining = 1;
  DBGPRINTU("New animation: EF_ALL_BRIGHT", milliseconds);
}

void Animation::_setParamsAllDark(const Sentence &s, const Effect e, uint32_t flags,
    uint32_t milliseconds) {
  _phaseDuration = milliseconds;
  _phaseCountRemaining = 1;
  DBGPRINTU("New animation: EF_ALL_DARK", milliseconds);
}

void Animation::_setParamsFadeLoveHate(const Sentence &s, const Effect e, uint32_t flags,
    uint32_t milliseconds) {

  uint32_t introTime = FADE_LOVE_HATE_INTRO_MILLIS;
  uint32_t outroTime = FADE_LOVE_HATE_OUTRO_MILLIS;
  uint32_t mainTime = milliseconds - FADE_LOVE_HATE_INTRO_MILLIS - FADE_LOVE_HATE_OUTRO_MILLIS;

  _setupIntroHoldOutro(introTime, mainTime, outroTime);

  // Which direction are we going? If LOVE is part of the sentence, we go from lots of LOVE to lots
  // of HATE; if HATE is part of the sentence -- do the opposite.
  int loveDeltaDirection = (s.getSignBits() & S_LOVE) ? -1 : 1;
  if (loveDeltaDirection < 0) {
    // Decreasing % of LOVE over time b/c LOVE is part of the sign => start with LOVE at max.
    _loveHateFadeLoveOnThreshold = LOVE_HATE_FADE_THRESHOLD_MAX;
  } else {
    _loveHateFadeLoveOnThreshold = 0;
  }

  // In the intro phase, LOVE (or HATE) is lit 100% of the time. In the outro phase, the reverse
  // is true.
  //
  // During the main phase, the percentage of time LOVE is lit changes linearly over the animation
  // from 100% to 0% (or vice versa). We start at THRESHOLD_MAX and go down to 0 linearly over the
  // course of the 'milliseconds' animation length; the per-frame change is LoveOnDeltaPerTic.
  _loveHateFadeLoveOnDeltaPerTic = loveDeltaDirection * LOVE_HATE_FADE_THRESHOLD_MAX *
      (int)LOOP_MILLIS / (int)mainTime;

  // The percentage of time HATE is lit is the exact opposite; exactly one of these two
  // signs will be lit on every frame; so we don't need to track that separately.
  DBGPRINTU("New animation: EF_FADE_LOVE_HATE", milliseconds);
}

void Animation::_setParamsNoEffect(const Sentence &s, const Effect e, uint32_t flags,
    uint32_t milliseconds) {

  // Disregard 'milliseconds'; this effect is definitionally over before it begins.
  _phaseCountRemaining = 0;
  _phaseDuration = 0;

  DBGPRINT("New animation: EF_NO_EFFECT (0 length)");
}


/** Pick a word within the sentence and configure it to flicker for this animation. */
static void configureRandomFlickeringWord(const Sentence &s) {
  signs[s.getNthWord(random(s.getNumWords()) + 1)].setFlickerThreshold(
      random(FLICKER_ASSIGN_MIN, FLICKER_ASSIGN_MAX));
}

/**
 * Set the parameters for the next animation cycle. Based on the specified sentence, effect, and
 * duration, tees up the internal plan to execute the animation frames.
 *
 * Effects can be applied over a flexible range of durations. For a given effect and sentence, there
 * may be an "optimal" duration for an aesthetically pleasing effect. Pass milliseconds==0 to use
 * the result of getOptimalDuration().
 */
void Animation::setParameters(const Sentence &s, const Effect e, uint32_t flags, uint32_t milliseconds) {
  if (_isRunning) {
    // We continue to do what was asked of us, but let operator know last animation was incomplete.
    DBGPRINT("*** WARNING: Resetting animation parameters without finishing last animation");
  }

  if (milliseconds == 0) {
    milliseconds = getOptimalDuration(s, e, flags);
  }

  _isRunning = false;
  _remainingTime = milliseconds;

  _effect = e;
  _flags = flags;
  _sentence = s;
  _phaseCountRemaining = 0;
  _isIntroHoldOutro = false;

  // Reset all flickering state to off.
  for (unsigned int i = 0; i < NUM_SIGNS; i++) {
    signs[i].setFlickerThreshold(FLICKER_ALWAYS_ON);
  }

  if (flags & ANIM_FLAG_FLICKER_COUNT_1) {
    // Choose 1 word in the sentence to flicker.
    configureRandomFlickeringWord(s);
  } else if (flags & ANIM_FLAG_FLICKER_COUNT_2) {
    // Choose two.
    configureRandomFlickeringWord(s);
    configureRandomFlickeringWord(s);
  } else if (flags & ANIM_FLAG_FLICKER_COUNT_3) {
    // Or tres.
    configureRandomFlickeringWord(s);
    configureRandomFlickeringWord(s);
    configureRandomFlickeringWord(s);
  }

  if (flags & ANIM_FLAG_FULL_SIGN_GLITCH_DARK) {
    // All signs should be set to flicker with a low duty cycle.
    for (unsigned int i = 0; i < NUM_SIGNS; i++) {
      signs[i].setFlickerThreshold(FULL_SIGN_GLITCH_FLICKER_DARK_THRESHOLD);
    }
  } else if (flags & ANIM_FLAG_FULL_SIGN_GLITCH_BRIGHT) {
    // All signs should be set to flicker with a high duty cycle.
    for (unsigned int i = 0; i < NUM_SIGNS; i++) {
      signs[i].setFlickerThreshold(FULL_SIGN_GLITCH_FLICKER_BRIGHT_THRESHOLD);
    }
  }

  switch(_effect) {
  case Effect::EF_APPEAR:
    _setParamsAppear(s, e, flags, milliseconds);
    break;
  case Effect::EF_GLOW:
    _setParamsGlow(s, e, flags, milliseconds);
    break;
  case Effect::EF_BLINK:
    _setParamsBlink(s, e, flags, milliseconds);
    break;
  case Effect::EF_BLINK_FAST:
    _setParamsBlinkFast(s, e, flags, milliseconds);
    break;
  case Effect::EF_ONE_AT_A_TIME:
    _setParamsOneAtATime(s, e, flags, milliseconds);
    break;
  case Effect::EF_BUILD:
    _setParamsBuild(s, e, flags, milliseconds);
    break;
  case Effect::EF_BUILD_RANDOM:
    _setParamsBuildRandom(s, e, flags, milliseconds);
    break;
  case Effect::EF_SNAKE:
    _setParamsSnake(s, e, flags, milliseconds);
    break;
  case Effect::EF_SLIDE_TO_END:
    _setParamsSlide(s, e, flags, milliseconds);
    break;
  case Effect::EF_MELT:
    _setParamsMelt(s, e, flags, milliseconds);
    break;
  case Effect::EF_ALL_BRIGHT:
    _setParamsAllBright(s, e, flags, milliseconds);
    break;
  case Effect::EF_ALL_DARK:
    _setParamsAllDark(s, e, flags, milliseconds);
    break;
  case Effect::EF_FADE_LOVE_HATE:
    _setParamsFadeLoveHate(s, e, flags, milliseconds);
    break;
  case Effect::EF_NO_EFFECT:
    _setParamsNoEffect(s, e, flags, milliseconds);
    break;
  default:
    DBGPRINTU("Unknown effect id", (uint32_t)_effect);
    // Act like this was EF_APPEAR
    _effect = Effect::EF_APPEAR;
    _setParamsAppear(s, _effect, flags, milliseconds);
    break;
  }

  if (_flags & ANIM_FLAG_FADE_LOVE_HATE) {
    // We should run an EF_FADE_LOVE_HATE animation on the same sentence
    // after this animation ends. Place that animation on deck.
    setOnDeckAnimationParams(s.id(), Effect::EF_FADE_LOVE_HATE, 0);
  }

  if (_phaseCountRemaining == 0) {
    DBGPRINT("*** WARNING: Animation planner set up 0 phase count.");
  }
}

// Helper function for EF_SLIDE_TO_END - pick the next destination word
// and update the state of the zipper.
bool Animation::_slidePickNextZipTarget() {
  uint32_t signBits = _sentence.getSignBits();
  if (signBits == 0) {
    return false; // Impossible to find another zip target; it's an empty sentence.
  }

  bool found = false;

  if (_curPhaseNum == PHASE_INTRO) {
    // Search right-to-left for a target as we zip in the words.
    for (int i = _slideCurTargetSignId - 1; i >= 0; i--) {
      if (signBits & (1 << i)) {
        _slideCurTargetSignId = i;
        found = true;
        break;
      }
    }

    _slideCurZipPosition = 0; // If we're looking for next zip target, we're starting a new zip.

    if (_slideCurTargetSignId != _slideCurZipPosition) {
      // We'll need to zip through at least one sign. Set zip timer appropriately.
      _nextZipTime = _phaseRemainingMillis - SLIDE_TO_END_PER_WORD_ZIP;
    } else {
      // We're starting on the destination word. Just hold it.
      _nextZipTime = _phaseRemainingMillis - SLIDE_TO_END_PER_WORD_HOLD;
    }
  } else {
    // We are in PHASE_OUTRO. (This method is not valid in PHASE_HOLD.)
    // Search left-to-right for a target as we zip out the words.
    for (unsigned int i = _slideCurTargetSignId; i < NUM_SIGNS; i++) {
      if (signBits & (1 << i)) {
        _slideCurTargetSignId = i;
        found = true;
        break;
      }
    }

    // We start zipping @ the target sign id. We are in this method because we just turned
    // off sign 0 at the end of zipping out a previous word in the outro phase; so we start
    // with a 'hold' step that holds the 'blank' before zipping the next word out.
    _slideCurZipPosition = _slideCurTargetSignId;
    _nextZipTime = _phaseRemainingMillis - SLIDE_TO_END_PER_WORD_HOLD;
  }

  return found;
}

// Start the animation sequence.
void Animation::start() {
  if (_phaseCountRemaining == 0) {
    DBGPRINT("*** WARNING: _phaseCountRemaining is 0 in start(); no animation to start.");
    _isRunning = false;
    _phaseRemainingMillis = 0;
    _curPhaseNum = 0;
    return;
  }

  _isRunning = true;
  _curPhaseNum = 0;
  _isFirstPhaseTic = true;
  if (_isIntroHoldOutro) {
    // In intro-hold-outro mode, the first phase is the intro phase, with its own timing.
    _phaseRemainingMillis = _ihoIntroDuration;
  } else {
    // Homogenous phase timing.
    _phaseRemainingMillis = _phaseDuration; // set up 1st phase duration.
  }

  allSignsOff(); // All animations start with a clean slate.
  configMaxPwm();
  next(); // Do first frame of first phase.
}

void Animation::_nextAppear() {
  // Single phase which lasts the entire duration of the effect.
  // On first frame, turn on the signs; and we're done.
  if (_isFirstPhaseTic) {
    _sentence.enable();
  }
}

void Animation::_nextGlow() {
  // 1/3 the time in phase 0: increasing brightness (fade in)
  // 1/3 the time in phase 1: hold at max brightness
  // 1/3 the time in phase 3: decreasing brightness (fade out)
  if (_isFirstPhaseTic) {
    _sentence.enable();

    if (_curPhaseNum == PHASE_INTRO) {
      pwmTimer.setDutyCycle(0); // Start fully off.
    } else if (_curPhaseNum == PHASE_HOLD) {
      // Second phase is fully glow'd up and holding steady here.
      configMaxPwm();
    } else if (_curPhaseNum == PHASE_OUTRO) {
      // Third phase starts at fully glowing and fade out.
      configMaxPwm();
      _glowCurrentBrightness = getMaxPwmDutyCycle();
    }
  } else if (_curPhaseNum == PHASE_INTRO) {
    // Not first frame of phase, and we are in the 1st phase (increasing glow)
    _glowCurrentBrightness += _glowStepSize;
    pwmTimer.setDutyCycle(_glowCurrentBrightness);
  } else if (_curPhaseNum == PHASE_OUTRO) {
    // Not first frame of phase and we are in last phase (fade out)
    _glowCurrentBrightness -= _glowStepSize;
    pwmTimer.setDutyCycle(_glowCurrentBrightness);
  }
}

void Animation::_nextBlink() {
  if (_isFirstPhaseTic) {
    if (_curPhaseNum % 2 == 0) {
      // Even phase: show
      _sentence.enable();
    } else {
      // Odd phase: hide
      _sentence.disable();
    }
  }
}

void Animation::_nextBlinkFast() {
  // Both blink and fast blink have same logic; difference is all
  // in the timing setup during setParameters().
  _nextBlink();
}

void Animation::_nextOneAtATime() {
  unsigned int signBits;
  unsigned int numWordsSeen;
  unsigned int highlightWord;
  unsigned int targetWordIdx;
  // In phase 'N', light up only the N+1'th word in the sentence.
  if (_isFirstPhaseTic) {
    allSignsOff();

    if (_phaseCountRemaining <= ONE_AT_A_TIME_BLANK_PHASES) {
      // Final phase(s) we just keep the sign blank to add some breathing room
      // before the next sentence animation begins.
      return;
    }

    // Show the N'th word in the sentence.
    signBits = _sentence.getSignBits();
    // Find the n'th word.
    numWordsSeen = 0;
    highlightWord = 0;
    // In phase 0 we want to choose the 1st word, and so on...
    targetWordIdx = _curPhaseNum + 1;
    for (unsigned int i = 0; i < NUM_SIGNS; i++) {
      if (signBits & (1 << i)) {
        numWordsSeen++;
      }

      if (numWordsSeen == targetWordIdx) {
        // This sign bit is the word to highlight.
        highlightWord = i;
        break;
      }
    }

    signs[highlightWord].enable();
  }
}

void Animation::_nextBuild() {
  unsigned int signBits;
  unsigned int numWordsSeen;
  unsigned int highlightWord;
  unsigned int targetWordIdx;

  // Logic very similar to ONE_AT_A_TIME but previously-shown words remain lit.
  if (_isFirstPhaseTic) {
    if (_phaseCountRemaining <= BUILD_HOLD_PHASES) {
      // We're in the last few phases, which just keep the whole sentence lit.
      // Nothing further to do.
      _sentence.enableExclusively();
      return;
    }

    // Turn on the N'th word in the sentence.
    signBits = _sentence.getSignBits();
    // Find the n'th word.
    numWordsSeen = 0;
    highlightWord = 0;
    // In phase 0 we want to choose the 1st word, and so on...
    targetWordIdx = _curPhaseNum + 1;
    for (unsigned int i = 0; i < NUM_SIGNS; i++) {
      if (signBits & (1 << i)) {
        numWordsSeen++;
      }

      if (numWordsSeen == targetWordIdx) {
        // This sign bit is the word to highlight.
        highlightWord = i;
        break;
      }
    }

    signs[highlightWord].enable();
  }
}

void Animation::_nextBuildRandom() {
  // Logic as in EF_BUILD, but instead of lighting up the n'th sign of the sentence in phase n,
  // we use the sign id from _buildRandomOrder[n]. There's no inner loop here because it got
  // pulled into the planning step method (_setParamsBuildRandom()).
  if (_isFirstPhaseTic) {
    if (_phaseCountRemaining <= BUILD_RANDOM_HOLD_PHASES) {
      // We're in the last few phases, which just keep the whole sentence lit.
      // Nothing further to do.
      _sentence.enableExclusively();
      return;
    }

    // Turn on the N'th word in the shuffled sentence light-up order.
    signs[_buildRandomOrder[_curPhaseNum]].enable();
  }
}

void Animation::_nextSnake() {
  unsigned int signBits;
  unsigned int numWordsSeen;
  unsigned int highlightWord;
  unsigned int targetWordIdx;
  unsigned int numWordsInSentence;

  // Logic for the first half of the phases is identical to EF_BUILD; we then repeat
  // the loop, turning words off one at a time.
  if (_isFirstPhaseTic) {
    // Turn on [or off] the N'th word in the sentence.
    signBits = _sentence.getSignBits();
    // Find the n'th word.
    numWordsSeen = 0;
    highlightWord = 0;
    numWordsInSentence = _sentence.getNumWords();
    // In phase 0 we want to choose the 1st word, and so on...
    if (_curPhaseNum < numWordsInSentence) {
      // We are turning words on.
      targetWordIdx = _curPhaseNum + 1;
    } else {
      // We are in the second meta-phase, turning words off, starting at the beginning
      // of the sign and working our way to the end of the sentence.
      targetWordIdx = _curPhaseNum + 1 - numWordsInSentence;
    }

    for (unsigned int i = 0; i < NUM_SIGNS; i++) {
      if (signBits & (1 << i)) {
        numWordsSeen++;
      }

      if (numWordsSeen == targetWordIdx) {
        // This sign bit is the word to highlight or turn off.
        highlightWord = i;
        break;
      }
    }

    if (_curPhaseNum < numWordsInSentence) {
      signs[highlightWord].enable();
    } else {
      signs[highlightWord].disable();
    }
  }
}

void Animation::_nextSlide() {
  bool foundZipTarget;

  if (_curPhaseNum == PHASE_INTRO) {
    if (_isFirstPhaseTic) {
      // We start by zipping from sign 0 to the last sign in the sentence.
      // Targets are selected from right to left.
      _slideCurTargetSignId = NUM_SIGNS;
      foundZipTarget = _slidePickNextZipTarget(); // Find the last sign in the sentence.

      if (!foundZipTarget) {
        // Shouldn't get here; it implies we lit an empty sentence?
        DBGPRINT("*** WARNING: no valid slide target sign id at start of intro phase");
        _phaseRemainingMillis = 0; // Force progression to next phase.
        _slideCurTargetSignId = 0; // Set this to a valid value for paranoia's sake.
        return; // short-circuit and don't handle zip movement.
      } else {
        // Ok... Get the first zip going!
        signs[_slideCurZipPosition].enable();
      }
    }

    // Most times we have nothing to do to update the animation...
    // Unless we've hit the next zip movement time.
    if (_phaseRemainingMillis <= _nextZipTime) {
      // Update the position of the zipping word.
      if (_slideCurZipPosition == _slideCurTargetSignId) {
        // We already landed on the target word. This is the end of the hold phase.
        // Reset to begin a new zip. Or if pickNext returns false, this intro phase is over.
        foundZipTarget = _slidePickNextZipTarget();
        if (foundZipTarget) {
          signs[_slideCurZipPosition].enable(); // zip pos reset to 0 by pickNext(); begin zip there.
          // n.b. we don't disable the currently-lit sign; we leave the destination sign on.
        } else {
          // Forcibly end the intro phase; no zip target left.
          _phaseRemainingMillis = 0;
          _slideCurZipPosition = 0;
          _slideCurTargetSignId = 0;
        }
      } else {
        signs[_slideCurZipPosition].disable(); // Turn off our current position...
        _slideCurZipPosition++; // move one to the right...
        signs[_slideCurZipPosition].enable(); // And wink on there.

        // And reset the timer.
        if (_slideCurZipPosition == _slideCurTargetSignId) {
          // We just arrived at the destination word. Hold here.
          _nextZipTime = _phaseRemainingMillis - SLIDE_TO_END_PER_WORD_HOLD;
        } else {
          // More zipping to do.
          _nextZipTime = _phaseRemainingMillis - SLIDE_TO_END_PER_WORD_ZIP;
        }
      }
    }
  } else if (_curPhaseNum == PHASE_HOLD) {
    if (_isFirstPhaseTic) {
      // Make sure we're setup correctly in case something got missed in intro phase.
      configMaxPwm();
      _sentence.enableExclusively();
    }
  } else if (_curPhaseNum == PHASE_OUTRO) {
    if (_isFirstPhaseTic) {
      _slideCurTargetSignId = 0; // Reset our zip target for left-to-right search.
      foundZipTarget = _slidePickNextZipTarget();
      if (!foundZipTarget) {
        // Nothing to do?
        DBGPRINT("*** WARNING: no zip target @ beginning of EF_SLIDE outro phase; empty sentence?");
        _phaseRemainingMillis = 0; // Instant end to phase.
      }
    }

    if (_phaseRemainingMillis <= _nextZipTime) {
      // Move the zipper along.
      signs[_slideCurZipPosition].disable();
      if (_slideCurZipPosition != 0) {
        _slideCurZipPosition--;
        signs[_slideCurZipPosition].enable();
        _nextZipTime = _phaseRemainingMillis - SLIDE_TO_END_PER_WORD_ZIP;
      } else {
        // Find another word to start zipping out.
        _slideCurTargetSignId++; // Advance zip target search starting point.
        foundZipTarget = _slidePickNextZipTarget();
      }
    }
  }
}

void Animation::_nextMelt() {
  if (_curPhaseNum == PHASE_INTRO) {
    if (_isFirstPhaseTic) {
      // When the intro phase starts, the entire board will be lit.
      allSignsOn();
      // Queue up the first melt sub-phase to start one melt interval after that.
      _nextMeltTime = _ihoIntroDuration - MELT_ONE_WORD_MILLIS;
      _numWordsLeftToMelt = NUM_SIGNS - _sentence.getNumWords();
      // We need to melt away all the signs...
      _availableMeltSet = (1 << NUM_SIGNS) - 1;
      // ... except those in the current sentence.
      _availableMeltSet &= ~(_sentence.getSignBits());
    }

    if (_phaseRemainingMillis <= _nextMeltTime) {
      _meltWord();
    }
  } else if (_curPhaseNum == PHASE_HOLD) {
    if (_isFirstPhaseTic) {
      // Make sure we're setup correctly in case something got missed in intro phase.
      configMaxPwm();
      _sentence.enableExclusively();
    }
  } else if (_curPhaseNum == PHASE_OUTRO) {
    if (_isFirstPhaseTic) {
      // Queue up a melt to begin immediately; we now start removing words from the real sentence.
      _nextMeltTime = _phaseRemainingMillis;
      _availableMeltSet = _sentence.getSignBits(); // We want to melt the sentence itself.
      _numWordsLeftToMelt = _sentence.getNumWords();
    } else if (_phaseRemainingMillis < MELT_BLANK_TIME) {
      // The last `MELT_BLANK_TIME` millis of the outro phase we just idle on a blank screen.
      return;
    }

    if (_phaseRemainingMillis <= _nextMeltTime) {
      _meltWord();
    }
  }
}

void Animation::_nextAllBright() {
  // Let there be light!
  if (_isFirstPhaseTic) {
    allSignsOn();
    configMaxPwm();
  }
}

void Animation::_nextAllDark() {
  // Last one out, please turn out the lights.
  if (_isFirstPhaseTic) {
    allSignsOff();
  }
}

void Animation::_nextFadeLoveHate() {
  if (_curPhaseNum == PHASE_INTRO) {
    if (_isFirstPhaseTic) {
      // Start the animation by showing the sentence as-is "APPEAR" style.
      _sentence.enableExclusively();
      configMaxPwm();

      _loveHateFrozenFramesRemaining = 0; // reset freeze counter @ start of animation.
    }

    return;
  } else if (_curPhaseNum == PHASE_OUTRO) {
    if (_isFirstPhaseTic) {
      // Make sure the faded-to word is lit.
      _sentence.enableExclusively();
      if (_loveHateFadeLoveOnDeltaPerTic > 0) {
        // Fading toward LOVE.
        signs[IDX_LOVE].enable();
        signs[IDX_HATE].disable();
      } else {
        // Fading toward HATE.
        signs[IDX_HATE].enable();
        signs[IDX_LOVE].disable();
      }

    }

    return;
  }

  // If we get here, we are in the main/hold phase of the intro/hold/outro phases.

  if (_loveHateFrozenFramesRemaining > 0) {
    // We don't update every frame; we freeze for N frames after making an update.
    // We're currently within a freeze. Update the probability change per tic but that's it.
    _loveHateFrozenFramesRemaining--;
    _loveHateFadeLoveOnThreshold += _loveHateFadeLoveOnDeltaPerTic;
    return;
  }

  // On all other frames, we use a weighted probability to decide which of the "LOVE" and
  // "HATE" signs to display.
  //
  // If the random number - in [0, THRESHOLD_MAX) - is less than LoveOnThreshold, turn on LOVE
  // and turn off HATE. Otherwise, do the opposite.

  int rnd = random(0, LOVE_HATE_FADE_THRESHOLD_MAX);
  if (rnd < _loveHateFadeLoveOnThreshold) {
    signs[IDX_LOVE].enable();
    signs[IDX_HATE].disable();
  } else {
    signs[IDX_HATE].enable();
    signs[IDX_LOVE].disable();
  }

  _loveHateFrozenFramesRemaining = 2;

  // The probability changes by DeltaPerTic each frame.
  _loveHateFadeLoveOnThreshold += _loveHateFadeLoveOnDeltaPerTic;
}

void Animation::_nextNoEffect() {
  // Nothing to do. (We technically shouldn't even get in here, because setParams should
  // have given us zero phases of animation.)
}


// Perform the next step of animation.
void Animation::next() {
  if (!_isRunning || _phaseCountRemaining == 0) {
    DBGPRINT("*** WARNING: Animation is not running; no work to do in next()");
    // Somehow these variables got out-of-sync; ensure isRunning() returns false.
    _phaseCountRemaining = 0;
    if (_flags & ANIM_FLAG_RESET_BUTTONS_ON_END) {
      attachStandardButtonHandlers();
      _flags &= ~ANIM_FLAG_RESET_BUTTONS_ON_END;
    }
    _isRunning = false;
    return;
  }

  if (_phaseRemainingMillis <= 0) {
    // We have finished a phase of the animation. Advance to next phase.
    _phaseCountRemaining--;
    _curPhaseNum++;
    _isFirstPhaseTic = true;

    if (_isIntroHoldOutro) {
      // Use the intro-hold-outro timing for the next phase.
      if (_curPhaseNum == PHASE_INTRO) {
        _phaseRemainingMillis = _ihoIntroDuration;
      } else if (_curPhaseNum == PHASE_HOLD) {
        _phaseRemainingMillis = _ihoHoldDuration;
      } else if (_curPhaseNum == PHASE_OUTRO) {
        _phaseRemainingMillis = _ihoOutroDuration;
      } else {
        // No further phases. Ensure animation ends.
        _phaseRemainingMillis = 0;
        _phaseCountRemaining = 0;
      }
    } else {
      // An ordinary animation of 1+ homogenous-timing phases.
      _phaseRemainingMillis = _phaseDuration;
    }
  }

  if (_phaseCountRemaining == 0) {
    // We have finished the animation.
    _isRunning = false;
    if (_flags & ANIM_FLAG_RESET_BUTTONS_ON_END) {
      attachStandardButtonHandlers();
      _flags &= ~ANIM_FLAG_RESET_BUTTONS_ON_END;
    }
    return;
  }

  // Actually perform the appropriate frame advance action for the specified effect.
  switch(_effect) {
  case Effect::EF_APPEAR:
    _nextAppear();
    break;
  case Effect::EF_GLOW:
    _nextGlow();
    break;
  case Effect::EF_BLINK:
    _nextBlink();
    break;
  case Effect::EF_BLINK_FAST:
    _nextBlinkFast();
    break;
  case Effect::EF_ONE_AT_A_TIME:
    _nextOneAtATime();
    break;
  case Effect::EF_BUILD:
    _nextBuild();
    break;
  case Effect::EF_BUILD_RANDOM:
    _nextBuildRandom();
    break;
  case Effect::EF_SNAKE:
    _nextSnake();
    break;
  case Effect::EF_SLIDE_TO_END:
    _nextSlide();
    break;
  case Effect::EF_MELT:
    _nextMelt();
    break;
  case Effect::EF_ALL_BRIGHT:
    _nextAllBright();
    break;
  case Effect::EF_ALL_DARK:
    _nextAllDark();
    break;
  case Effect::EF_FADE_LOVE_HATE:
    _nextFadeLoveHate();
    break;
  case Effect::EF_NO_EFFECT:
    _nextNoEffect();
    break;
  default:
    // Shouldn't get here; setParameters() should have validated _effect.
    if (_isFirstPhaseTic) {
      DBGPRINTU("*** ERROR: Unknown effect id in next()?! _effect = ", (uint32_t)_effect);
    }
    // Just reset to EF_APPEAR state.
    _sentence.enableExclusively();
    configMaxPwm();
    // And kill this animation.
    stop();
    break;
  }

  // Update any flickering signs.
  for (unsigned int i = 0; i < NUM_SIGNS; i++) {
    signs[i].flickerFrame();
  }

  if (LOOP_MILLIS > _phaseRemainingMillis) {
    _phaseRemainingMillis = 0;
  } else {
    _phaseRemainingMillis -= LOOP_MILLIS;
  }

  _isFirstPhaseTic = false;
}

/* Helper function for EF_MELT animation. Pick a random word to turn off. */
void Animation::_meltWord() {
  // Melt away a word. Use random(_numWordsLeftToMelt) to get an idx into
  // the i'th lit word we don't want to preserve as part of the sentence (tracked as a bitmask
  // in _availableMeltSet).

  // Melt the idx'th word in the melt set.
  unsigned int validWordIdx = random(_numWordsLeftToMelt) + 1;
  unsigned int numWordsSeen = 0;
  unsigned int meltWordId = 0;
  for (unsigned int i = 0; i < NUM_SIGNS; i++) {
    if (_availableMeltSet & (1 << i)) {
      numWordsSeen++;
      if (numWordsSeen == validWordIdx) {
        meltWordId = i; // we found the word to disable.
        break;
      }
    }
  }

  // We found our target; turn it off.
  signs[meltWordId].disable();

  // This word is no longer available for melting.
  _availableMeltSet &= ~(1 << meltWordId);
  _numWordsLeftToMelt--;

  // Set the timer for the next melt tick.
  if (_numWordsLeftToMelt > 0) {
    _nextMeltTime -= MELT_ONE_WORD_MILLIS;
  }
}

// Halt the animation sequence even if there's part remaining.
void Animation::stop() {
  if (_flags & ANIM_FLAG_RESET_BUTTONS_ON_END) {
    attachStandardButtonHandlers();
    _flags &= ~ANIM_FLAG_RESET_BUTTONS_ON_END;
  }

  _isRunning = false;
  _phaseCountRemaining = 0;
  _phaseRemainingMillis = 0;
  _phaseDuration = 0;
  _remainingTime = 0;
}

// The central Animation instance that is used in the main loop.
Animation activeAnimation;

