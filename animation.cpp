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
  _isIntroHoldOutro = false;

  uint32_t framesPerPhase;

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
    _phaseDuration = BLINK_PHASE_MILLIS;
    _phaseCountRemaining = (milliseconds + BLINK_PHASE_MILLIS - 1) / BLINK_PHASE_MILLIS;
    DBGPRINTU("New animation: EF_BLINK", milliseconds);
    break;
  case EF_BLINK_FAST:
    _phaseDuration = FAST_BLINK_PHASE_MILLIS;
    _phaseCountRemaining = (milliseconds + FAST_BLINK_PHASE_MILLIS - 1) / FAST_BLINK_PHASE_MILLIS;
    DBGPRINTU("New animation: EF_BLINK_FAST", milliseconds);
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
  case EF_MELT:
    DBGPRINT("TODO: EF_MELT");
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
  if (_isIntroHoldOutro) {
    // In intro-hold-outro mode, the first phase is the intro phase, with its own timing.
    _phaseRemainingMillis = _ihoIntroDuration;
  } else {
    // Homogenous phase timing.
    _phaseRemainingMillis = _phaseDuration; // set up 1st phase duration.
  }

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

  case EF_MELT:
    DBGPRINT("TODO: EF_MELT");
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

