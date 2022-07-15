// (c) Copyright 2022 Aaron Kimball

#ifndef _ANIMATION_H
#define _ANIMATION_H

enum Effect {
  EF_APPEAR,         // Just turn on the words and hold them there.
  EF_GLOW,           // Fade up from nothing, hold high, fade back to zero.
  EF_BLINK,          // Behold the cursed <blink> tag!
  EF_BLINK_FAST,
  EF_ONE_AT_A_TIME,  // Highlight each word in series, turning off word n before showing n+1.
  EF_BUILD,          // Light up incrementally more words one at a time left-to-right
  EF_SNAKE,          // Like BUILD, but also "unbuild" by then turning off the 1st word, then the
                     // 2nd... until all is dark.
  EF_SLIDE_TO_END,   // Make a light pulse "zip" through all the words until reaching the last
                     // word in the phrase, then that word stays on. Then zip the 2nd-to-last
                     // word...
  EF_MELT,           // Start with all words on and "melt away" words one-by-one to reveal the
                     // real sentence. The sentence holds, and then individual words turn off
                     // to fade to black for outro.

// TODO(aaron): These are effectively layered effects that should be possible to 'or' on top of
// the preceeding effects.
  EF_FRITZING_ART,   // Like GLOW, but the word "ART" zaps in and out randomly, like neon on the fritz.
                     // Only valid for sentences with 'ART' in them.
  EF_FRITZING_DONT,  // Like FRITZING_ART, but the word "DON'T" is what's zapping in and out.
  EF_ALT_LOVE_HATE,  // Alternate lighting the "LOVE" and "HATE" words.
};


constexpr unsigned int NUM_EFFECTS = (unsigned int)(Effect::EF_ALT_LOVE_HATE) + 1;

constexpr unsigned int BLINK_PHASE_MILLIS = 1000;
constexpr unsigned int FAST_BLINK_PHASE_MILLIS = 250;

inline constexpr uint32_t durationForBlinkCount(const uint32_t blinkCount) {
  return blinkCount * BLINK_PHASE_MILLIS * 2; // one on + one off phase per blink = 2 * phase_millis.
}

inline constexpr uint32_t durationForFastBlinkCount(const uint32_t blinkCount) {
  return blinkCount * FAST_BLINK_PHASE_MILLIS * 2; // one on + one off phase per blink = 2 * phase_millis.
}

// The EF_MELT animation will melt away one word every 'n' milliseconds configured here:
constexpr unsigned int MELT_ONE_WORD_MILLIS = 250;

// The SLIDE_TO_END animation uses the following timing:
constexpr unsigned int SLIDE_TO_END_PER_WORD_ZIP = 100; // a 'light zip' moves thru words at 1x / 100ms
constexpr unsigned int SLIDE_TO_END_PER_WORD_HOLD = 350; // light zip 'holds' on a its destination word
constexpr unsigned int SLIDE_TO_END_MINIMUM_SENTENCE_HOLD = 1500;
constexpr unsigned int SLIDE_TO_END_DEFAULT_SENTENCE_HOLD = 2500;

// In intro-hold-outro mode animations, the 3 phases have specific names:
constexpr unsigned int PHASE_INTRO = 0;
constexpr unsigned int PHASE_HOLD = 1;
constexpr unsigned int PHASE_OUTRO = 2;

/**
 * An animation makes a sentence appear with a specified effect.
 *
 * The animation has two phases: planning and execution.
 *
 * The planning phase occurs in the setParameters() method. All the information needed
 * to direct the animation is provided at once: the sentence to show, the effect to apply,
 * and the desired duration of effect. Within this method, the planner calculates how many
 * distinct phases must occur and how long each phase lasts. The duration, count, and nature
 * of phases of animation are particular to the chosen effect and may vary with respect to
 * the number of words to display or other aspects of the chosen sentence.
 *
 * At this point both isRunning() and isComplete() will return false.
 *
 * The execution phase begin with the 'start()' method. Thi sets the start time and
 * performs the first 'key frame' of the animation. It is after this method returns
 * that isRunning() will return true.
 *
 * Each frame of animation lasts for LOOP_MICROS microseconds.
 *
 * Execution continues with successive calls to next(). This shows the next frame of animation.
 * This continues until isComplete() returns true. (At which point isRunning() returns false.)
 * At which point next() will do nothing until a new animation is planned with setParameters().
 *
 * If at any point during execution the `stop()` method is called, the animation is
 * short-circuited and isComplete() will return true.
 */
class Animation {
public:
  Animation();

  void setParameters(const Sentence &s, const Effect e, uint32_t flags, uint32_t milliseconds);
  uint32_t getOptimalDuration(const Sentence &s, const Effect e, const uint32_t flags);

  bool isRunning() const { return _isRunning; }
  bool isComplete() const { return !_isRunning && _phaseCountRemaining == 0; };
  const Sentence &getSentence() const { return _sentence; };

  void start(); // Start the animation sequence.
  void next(); // Perform the next step of animation.
  void stop(); // Halt the animation sequence even if there's part remaining.

private:
  //// Core parameters for the animation ////
  Sentence _sentence;
  Effect _effect;
  uint32_t _flags;

  void _setParamsAppear(const Sentence &s, const Effect e, uint32_t flags, uint32_t milliseconds);
  void _setParamsGlow(const Sentence &s, const Effect e, uint32_t flags, uint32_t milliseconds);
  void _setParamsBlink(const Sentence &s, const Effect e, uint32_t flags, uint32_t milliseconds);
  void _setParamsBlinkFast(const Sentence &s, const Effect e, uint32_t flags, uint32_t milliseconds);
  void _setParamsOneAtATime(const Sentence &s, const Effect e, uint32_t flags, uint32_t milliseconds);
  void _setParamsBuild(const Sentence &s, const Effect e, uint32_t flags, uint32_t milliseconds);
  void _setParamsSnake(const Sentence &s, const Effect e, uint32_t flags, uint32_t milliseconds);
  void _setParamsSlide(const Sentence &s, const Effect e, uint32_t flags, uint32_t milliseconds);
  void _setParamsMelt(const Sentence &s, const Effect e, uint32_t flags, uint32_t milliseconds);

  void _nextAppear();
  void _nextGlow();
  void _nextBlink();
  void _nextBlinkFast();
  void _nextOneAtATime();
  void _nextBuild();
  void _nextSnake();
  void _nextSlide();
  void _nextMelt();

  //// State to manage advancing frames & phases of the animation ////
  unsigned int _remainingTime; // millis.
  bool _isRunning;
  bool _isFirstPhaseTic; // True during the first frame of a phase.

  unsigned int _phaseDuration; // millis per phase
  unsigned int _phaseRemainingMillis; // millis remaining in current phase
  unsigned int _phaseCountRemaining; // number of phases to go.
  unsigned int _curPhaseNum; // sequentially incrementing counter thru phases.

  bool _isIntroHoldOutro; // intro-hold-outro mode is 3 phases which may not
                          // have the same length. Each phase duration is initialized
                          // from the variables below.

  unsigned int _ihoIntroDuration;
  unsigned int _ihoHoldDuration;
  unsigned int _ihoOutroDuration;

  // Helper method to setup an intro-hold-outro style animation pattern.
  void _setupIntroHoldOutro(unsigned int introTime, unsigned int holdTime, unsigned int outroTime);

  //// Effect-specific state ////

  // EF_GLOW
  uint32_t _glowStepSize;
  unsigned int _glowCurrentBrightness;

  // EF_SLIDE_TO_END
  uint32_t _slideCurZipPosition;  // The sign id where the 'zipping light' currently is.
  uint32_t _slideCurTargetSignId; // The sign id where the 'zipping light' will rest.
  unsigned int _nextZipTime;  // Next phaseRemainingMillis when the zip should advance.
  bool _slidePickNextZipTarget();

  // EF_MELT
  unsigned int _nextMeltTime; // the phaseRemainingMillis value when we should next melt a word.
  unsigned int _availableMeltSet;  // the bitmask of words we are allowed to melt in this phase.
  unsigned int _numWordsLeftToMelt; // number of words in available melt set
  void _meltWord();
};

extern Animation activeAnimation;

#endif // _ANIMATION_H

