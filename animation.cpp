// (c) Copyright 2022 Aaron Kimball

#include "like-the-art.h"

Animation::Animation():
    _sentence(0, 0), _effect(EF_APPEAR), _flags(0), _remainingTime(0), _isRunning(false),
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
  case EF_APPEAR:
    return 5000;  // Show the sentence for 5 seconds.
  case EF_GLOW:
    return 5000;  // 1250 ms glow-up, 2500ms hold, 1250 ms glow-down
  case EF_BLINK:
    // approx 6 seconds total (1s on / 1s off x 3 blinks)
    return durationForBlinkCount(3);
  case EF_BLINK_FAST:
    // approx 4 seconds total (250ms on / 250 ms off x 8 blinks)
    return durationForFastBlinkCount(8);
  case EF_ONE_AT_A_TIME:
    return s.getNumWords() * 500; // 1/2 sec of each word in sentence.
  case EF_BUILD:
    return s.getNumWords() * 500; // words in sentence light up 1/2 sec apart.
  case EF_SNAKE:
    return 2 * s.getNumWords() * 500; // words in sentence light up and tear down 1/2 sec apart.
  case EF_SLIDE_TO_END:
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

    // (zip-in times + per-word holds) + (2s whole-sentence hold) + (zip-out times)
    return (positionSum * SLIDE_TO_END_PER_WORD_ZIP + s.getNumWords() * SLIDE_TO_END_PER_WORD_HOLD)
        + SLIDE_TO_END_DEFAULT_SENTENCE_HOLD
        + (positionSum * SLIDE_TO_END_PER_WORD_ZIP);
  case EF_MELT:
    // time for all words to melt plus 3s hold time
    return MELT_ONE_WORD_MILLIS * NUM_SIGNS + 3000;
  default:
    DBGPRINTU("Unknown effect in getOptimalDuration():", (uint32_t)e);
    DBGPRINT("Returning default duration of 2000ms.");
    return 2000;
  }
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
    DBGPRINT("Warning: Resetting animation parameters without finishing last animation");
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

  uint32_t framesPerPhase;
  uint32_t numWordsToMelt;
  uint32_t holdPhaseTime;
  uint32_t setupTime;
  uint32_t teardownTime;
  uint32_t positionSum, sentenceBits;
  unsigned int i;

  switch(_effect) {
  case EF_APPEAR:
    // Single phase which lasts the entire duration of the effect.
    _phaseDuration = milliseconds;
    _phaseCountRemaining = 1;
    DBGPRINTU("New animation: EF_APPEAR", milliseconds);
    break;
  case EF_GLOW:
    // 1/4 the time in phase 0: increasing brightness (fade in)
    // 1/2 the time in phase 1: hold at max brightness
    // 1/4 the time in phase 2: decreasing brightness (fade out)
    _phaseDuration = milliseconds / 4;
    _setupIntroHoldOutro(_phaseDuration, 2 * _phaseDuration, _phaseDuration);

    framesPerPhase = _phaseDuration / LOOP_MILLIS;
    _glowStepSize = getMaxPwmDutyCycle() / framesPerPhase;
    _glowCurrentBrightness = 0;
    DBGPRINTU("New animation: EF_GLOW", milliseconds);
    break;
  case EF_BLINK:
    // Simple blinking effect; an even number of phases alternating on & off, of fixed duration.
    _phaseDuration = BLINK_PHASE_MILLIS;
    _phaseCountRemaining = (milliseconds + BLINK_PHASE_MILLIS - 1) / BLINK_PHASE_MILLIS;
    DBGPRINTU("New animation: EF_BLINK", milliseconds);
    break;
  case EF_BLINK_FAST:
    // Like EF_BLINK but with faster phases.
    _phaseDuration = FAST_BLINK_PHASE_MILLIS;
    _phaseCountRemaining = (milliseconds + FAST_BLINK_PHASE_MILLIS - 1) / FAST_BLINK_PHASE_MILLIS;
    DBGPRINTU("New animation: EF_BLINK_FAST", milliseconds);
    break;
  case EF_ONE_AT_A_TIME:
    // Have N phases where N = number of words in sentence. One word at a time is lit.
    _phaseCountRemaining = s.getNumWords();
    _phaseDuration = milliseconds / _phaseCountRemaining;
    DBGPRINTU("New animation: EF_ONE_AT_A_TIME", milliseconds);
    break;
  case EF_BUILD:
    // Have N phases where N = number of words in sentence; in phase k the first k words of the
    // sentence are lit.
    _phaseCountRemaining = s.getNumWords();
    _phaseDuration = milliseconds / _phaseCountRemaining;
    DBGPRINTU("New animation: EF_BUILD", milliseconds);
    break;
  case EF_SNAKE:
    // Like BUILD, but also "unbuild" by then turning off the 1st word, then the
    // 2nd... until all is dark.
    _phaseCountRemaining = s.getNumWords() * 2;
    _phaseDuration = milliseconds / _phaseCountRemaining;
    DBGPRINTU("New animation: EF_SNAKE", milliseconds);
    break;
  case EF_SLIDE_TO_END:
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

    // (zip-in times + per-word holds) + (2s whole-sentence hold) + (zip-out times)
    setupTime = (positionSum * SLIDE_TO_END_PER_WORD_ZIP + s.getNumWords() * SLIDE_TO_END_PER_WORD_HOLD);
    teardownTime = (positionSum * SLIDE_TO_END_PER_WORD_ZIP);
    if (setupTime + teardownTime + SLIDE_TO_END_MINIMUM_SENTENCE_HOLD < milliseconds) {
      // Enforce a minimum 1s hold phase.
      holdPhaseTime = SLIDE_TO_END_MINIMUM_SENTENCE_HOLD;
    } else {
      // If milliseconds is bigger than the minimum, allocate all the remaining time to hold phase.
      holdPhaseTime = milliseconds - setupTime - teardownTime;
    }
    _setupIntroHoldOutro(setupTime, holdPhaseTime, teardownTime);

    DBGPRINTU("New animation: EF_SLIDE_TO_END", setupTime + holdPhaseTime + teardownTime);
    break;
  case EF_MELT:
    // Start with all words on and "melt away" words one-by-one to reveal the
    // real sentence. The sentence holds, and then individual words turn off
    // to fade to black for outro.
    numWordsToMelt = NUM_SIGNS - s.getNumWords();
    if (NUM_SIGNS * MELT_ONE_WORD_MILLIS > milliseconds) {
      // This is going to be an over-length animation. Just do a quick hold.
      holdPhaseTime = 1000;
    } else {
      // All time not spent melting is just in hold.
      holdPhaseTime = milliseconds - (NUM_SIGNS * MELT_ONE_WORD_MILLIS);
    }

    _setupIntroHoldOutro(numWordsToMelt * MELT_ONE_WORD_MILLIS,
                         holdPhaseTime,
                         s.getNumWords() * MELT_ONE_WORD_MILLIS);

    DBGPRINTU("New animation: EF_MELT", milliseconds);
    break;
  default:
    DBGPRINTU("Unknown effect id", (uint32_t)_effect);
    // Act like this was EF_APPEAR
    _phaseDuration = milliseconds;
    _phaseCountRemaining = 1;
    _effect = EF_APPEAR;
    break;
  }

  if (_phaseCountRemaining == 0) {
    DBGPRINT("Warning: animation planner set up 0 phase count.");
  }

}

// Start the animation sequence.
void Animation::start() {
  if (_phaseCountRemaining == 0) {
    DBGPRINT("Warning: _phaseCountRemaining is 0 in start(); no animation to start.");
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

// Perform the next step of animation.
void Animation::next() {
  if (!_isRunning || _phaseCountRemaining == 0) {
    DBGPRINT("Warning: Animation is not running; no work to do in next()");
    // Somehow these variables got out-of-sync; ensure isRunning() returns false.
    _phaseCountRemaining = 0;
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
      }
    } else {
      // An ordinary animation of 1+ homogenous-timing phases.
      _phaseRemainingMillis = _phaseDuration;
    }
  }

  if (_phaseCountRemaining == 0) {
    // We have finished the animation.
    _isRunning = false;
    return;
  }

  // Actually perform the appropriate frame advance action for the specified effect.
  unsigned int signBits;
  unsigned int numWordsSeen;
  unsigned int highlightWord;
  unsigned int i;
  unsigned int targetWordIdx;
  unsigned int numWordsInSentence;

  switch(_effect) {
  case EF_APPEAR:
    // Single phase which lasts the entire duration of the effect.
    // On first frame, turn on the signs; and we're done.
    if (_isFirstPhaseTic) {
      _sentence.enable();
    }
    break;

  case EF_GLOW:
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
    break;

  case EF_BLINK:      // Both blink and fast blink have same logic; difference is all
  case EF_BLINK_FAST: // in the timing setup during setParameters().
    if (_isFirstPhaseTic) {
      if (_curPhaseNum % 2 == 0) {
        // Even phase: show
        _sentence.enable();
      } else {
        // Odd phase: hide
        _sentence.disable();
      }
    }
    break;

  case EF_ONE_AT_A_TIME:
    // In phase 'N', light up only the N+1'th word in the sentence.
    if (_isFirstPhaseTic) {
      allSignsOff();

      // Show the N'th word in the sentence.
      signBits = _sentence.getSignBits();
      // Find the n'th word.
      numWordsSeen = 0;
      highlightWord = 0;
      // In phase 0 we want to choose the 1st word, and so on...
      targetWordIdx = _curPhaseNum + 1;
      for (i = 0; i < NUM_SIGNS; i++) {
        if (signBits & (1 << i)) {
          numWordsSeen++;
        }

        if (numWordsSeen == targetWordIdx) {
          // This sign bit is the word to highlight.
          highlightWord = i;
        }
      }

      signs[highlightWord].enable();
    }
    break;

  case EF_BUILD:
    // Logic very similar to ONE_AT_A_TIME but previously-shown words remain lit.
    if (_isFirstPhaseTic) {
      // Turn on the N'th word in the sentence.
      signBits = _sentence.getSignBits();
      // Find the n'th word.
      numWordsSeen = 0;
      highlightWord = 0;
      // In phase 0 we want to choose the 1st word, and so on...
      targetWordIdx = _curPhaseNum + 1;
      for (i = 0; i < NUM_SIGNS; i++) {
        if (signBits & (1 << i)) {
          numWordsSeen++;
        }

        if (numWordsSeen == targetWordIdx) {
          // This sign bit is the word to highlight.
          highlightWord = i;
        }
      }

      signs[highlightWord].enable();
    }
    break;

  case EF_SNAKE:
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

      for (i = 0; i < NUM_SIGNS; i++) {
        if (signBits & (1 << i)) {
          numWordsSeen++;
        }

        if (numWordsSeen == targetWordIdx) {
          // This sign bit is the word to highlight or turn off.
          highlightWord = i;
        }
      }

      if (_curPhaseNum < numWordsInSentence) {
        signs[highlightWord].enable();
      } else {
        signs[highlightWord].disable();
      }
    }

    break;

  case EF_SLIDE_TO_END:
    if (_curPhaseNum == PHASE_INTRO) {
      if (_isFirstPhaseTic) {
        // ****** todo make slide-to-end work. *****
        DBGPRINT("Error: TODO - SLIDE_TO_END Intro first phase tic not implemented");
      }

      if (_phaseRemainingMillis <= _nextZipTime) {
        // todo - zip.....
        DBGPRINT("Error: TODO - SLIDE_TO_END Intro phase zip not implemented");
      }
    } else if (_curPhaseNum == PHASE_HOLD) {
      if (_isFirstPhaseTic) {
        // Make sure we're setup correctly in case something got missed in intro phase.
        allSignsOff();
        configMaxPwm();
        _sentence.enable();
      }
    } else if (_curPhaseNum == PHASE_OUTRO) {
      if (_isFirstPhaseTic) {
      }
    }
    DBGPRINT("TODO: EF_SLIDE_TO_END");
    break;

  case EF_MELT:
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
        allSignsOff();
        configMaxPwm();
        _sentence.enable();
      }
    } else if (_curPhaseNum == PHASE_OUTRO) {
      if (_isFirstPhaseTic) {
        // Queue up a melt to begin immediately; we now start removing words from the real sentence.
        _nextMeltTime = _phaseRemainingMillis;
        _availableMeltSet = _sentence.getSignBits(); // We want to melt the sentence itself.
        _numWordsLeftToMelt = _sentence.getNumWords();
      }

      if (_phaseRemainingMillis <= _nextMeltTime) {
        _meltWord();
      }
    }
    break;

  default:
    // Shouldn't get here; setParameters() should have validated _effect.
    if (_isFirstPhaseTic) {
      DBGPRINTU("Unknown effect id in next()?! _effect = ", (uint32_t)_effect);
    }
    // Just reset to EF_APPEAR state.
    allSignsOff();
    _sentence.enable();
    configMaxPwm();
    // And kill this animation.
    _phaseRemainingMillis = 0;
    _phaseCountRemaining = 0;
    _isRunning = false;
    break;
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
  _isRunning = false;
  _phaseCountRemaining = 0;
  _phaseRemainingMillis = 0;
  _phaseDuration = 0;
  _remainingTime = 0;
}

// The central Animation instance that is used in the main loop.
Animation activeAnimation;

