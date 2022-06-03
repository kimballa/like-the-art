// (c) Copyright 2022 Aaron Kimball

#include "like-the-art.h"

Animation::Animation():
    _sentence(0, 0), _effect(EF_APPEAR), _flags(0), _remainingTime(0), _isRunning(false),
    _phaseDuration(0), _phaseRemainingMillis(0), _phaseCountRemaining(0)
    {
}

void Animation::setParameters(const Sentence &s, const Effect e, uint32_t flags, uint32_t milliseconds) {

  if (_isRunning) {
    // We continue to do what was asked of us, but let operator know last animation was incomplete.
    DBGPRINT("Warning: Resetting animation parameters without finishing last animation");
  }

  _isRunning = false;
  _remainingTime = milliseconds;

  _effect = e;
  _flags = flags;
  _sentence = s;
  _phaseCountRemaining = 0;

  uint32_t framesPerPhase;

  switch(_effect) {
  case EF_APPEAR:
    // Single phase which lasts the entire duration of the effect.
    _phaseDuration = milliseconds;
    _phaseCountRemaining = 1;
    DBGPRINTU("New animation: EF_APPEAR", milliseconds);
    break;
  case EF_GLOW:
    // 1/3 the time in phase 0: increasing brightness (fade in)
    // 1/3 the time in phase 1: hold at max brightness
    // 1/3 the time in phase 2: decreasing brightness (fade out)
    _phaseDuration = milliseconds / 3;
    _phaseCountRemaining = 3;
    framesPerPhase = _phaseDuration / LOOP_MILLIS;
    _glowStepSize = getMaxPwmDutyCycle() / framesPerPhase;
    _glowCurrentBrightness = 0;
    DBGPRINTU("New animation: EF_GLOW", milliseconds);
    break;
  case EF_BLINK:
    _phaseDuration = BLINK_PHASE_MILLIS;
    _phaseCountRemaining = (milliseconds + BLINK_PHASE_MILLIS - 1) / BLINK_PHASE_MILLIS;
    DBGPRINTU("New animation: EF_BLINK", milliseconds);
    break;
  case EF_ONE_AT_A_TIME:
    DBGPRINT("TODO: EF_ONE_AT_A_TIME");
    break;
  case EF_BUILD:
    DBGPRINT("TODO: EF_BUILD");
    break;
  case EF_SNAKE:
    DBGPRINT("TODO: EF_SNAKE");
    break;
  case EF_SLIDE_TO_END:
    DBGPRINT("TODO: EF_SLIDE_TO_END");
    break;
  default:
    DBGPRINTU("Unknown effect id", (uint32_t)_effect);
    // Act like this was EF_APPEAR
    _phaseDuration = milliseconds;
    _phaseCountRemaining = 1;
    _effect = EF_APPEAR;
    break;
  }

}

// Start the animation sequence.
void Animation::start() {
  _isRunning = true;
  _curPhaseNum = 0;
  _isFirstPhaseTic = true;
  _phaseRemainingMillis = _phaseDuration; // set up 1st phase duration.

  allSignsOff(); // All animations start with a clean slate.
  next(); // Do first frame of first phase.
}

// Perform the next step of animation.
void Animation::next() {
  if (!_isRunning || _phaseCountRemaining == 0) {
    DBGPRINT("Warning: Animation is not running; no work to do in next()");
    return;
  }

  if (_phaseRemainingMillis <= 0) {
    // We have finished a phase of the animation. Advance to next phase.
    _phaseCountRemaining--;
    _phaseRemainingMillis = _phaseDuration;
    _curPhaseNum++;
    _isFirstPhaseTic = true;
  }

  if (_phaseCountRemaining == 0) {
    // We have finished the animation.
    _isRunning = false;
    return;
  }

  // Actually perform the appropriate frame advance action for the specified effect.

  switch(_effect) {
  case EF_APPEAR:
    // Single phase which lasts the entire duration of the effect.
    // On first frame, turn on the signs; and we're done.
    if (_isFirstPhaseTic) {
      configMaxPwm();
      _sentence.enable();
    }
    break;

  case EF_GLOW:
    // 1/3 the time in phase 0: increasing brightness (fade in)
    // 1/3 the time in phase 1: hold at max brightness
    // 1/3 the time in phase 3: decreasing brightness (fade out)
    if (_isFirstPhaseTic) {
      _sentence.enable();

      if (_curPhaseNum == 0) {
        pwmTimer.setDutyCycle(0); // Start fully off.
      } else if (_curPhaseNum == 1) {
        // Second phase is fully glow'd up and holding steady here.
        configMaxPwm();
      } else if (_curPhaseNum == 2) {
        // Third phase starts at fully glowing and fade out.
        configMaxPwm();
        _glowCurrentBrightness = getMaxPwmDutyCycle();
      }
    } else if (_curPhaseNum == 0) {
      // Not first frame of phase, and we are in the 1st phase (increasing glow)
      _glowCurrentBrightness += _glowStepSize;
      pwmTimer.setDutyCycle(_glowCurrentBrightness);
    } else if (_curPhaseNum == 2) {
      // Not first frame of phase and we are in last phase (fade out)
      _glowCurrentBrightness -= _glowStepSize;
      pwmTimer.setDutyCycle(_glowCurrentBrightness);
    }
    break;

  case EF_BLINK:
    if (_isFirstPhaseTic) {
      if (_curPhaseNum % 2 == 0) {
        // Even phase: show
        _sentence.enable();
        configMaxPwm();
      } else {
        // Odd phase: hide
        _sentence.disable();
      }
    }
    break;

  case EF_ONE_AT_A_TIME:
    DBGPRINT("TODO: EF_ONE_AT_A_TIME");
    break;

  case EF_BUILD:
    DBGPRINT("TODO: EF_BUILD");
    break;

  case EF_SNAKE:
    DBGPRINT("TODO: EF_SNAKE");
    break;

  case EF_SLIDE_TO_END:
    DBGPRINT("TODO: EF_SLIDE_TO_END");
    break;

  default:
    // Shouldn't get here; setParameters() should have validated _effect.
    if (_isFirstPhaseTic) {
      DBGPRINTU("Unknown effect id in next()?! _effect = ", (uint32_t)_effect);
    }
    _sentence.enable();
    configMaxPwm();
    break;
  }


  if (LOOP_MILLIS > _phaseRemainingMillis) {
    _phaseRemainingMillis = 0;
  } else {
    _phaseRemainingMillis -= LOOP_MILLIS;
  }

  _isFirstPhaseTic = false;
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

