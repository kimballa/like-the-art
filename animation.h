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
  EF_BUILD_RANDOM,   // Like EF_BUILD, but light 'em up in randomly-chosen order.
  EF_SNAKE,          // Like BUILD, but also "unbuild" by then turning off the 1st word, then the
                     // 2nd... until all is dark.
  EF_SLIDE_TO_END,   // Make a light pulse "zip" through all the words until reaching the last
                     // word in the phrase, then that word stays on. Then zip the 2nd-to-last
                     // word...
  EF_MELT,           // Start with all words on and "melt away" words one-by-one to reveal the
                     // real sentence. The sentence holds, and then individual words turn off
                     // to fade to black for outro.

  //// Special-purpose effects ////
  // These cannot be returned by randomEffect(), they are triggered under specific conditions:
  // user button presses; as a secondary chained animation registered "on deck"; etc.

  EF_ALL_BRIGHT,     // Disregard Sentence; entire sign illuminated a la EF_APPEAR.
  EF_ALL_DARK,       // Disregard Sentence; entire sign just remains off.

  EF_FADE_LOVE_HATE, // Fade across between lighting the "LOVE" and "HATE" words.

  EF_NO_EFFECT,      // A do-nothing placeholder / "null" effect, to mark the end of the
                     // enumeration. Not intended for direct use. Do not place new enumerations
                     // below this one, as its used for counting purposes.
};

// The enum value representing the highest-numbered Effect that `randomEffect()` can return.
constexpr Effect MAX_RANDOM_EFFECT_ID = EF_MELT;

// The enum value representing the highest-numbered valid Effect.
constexpr Effect MAX_EFFECT_ID = EF_NO_EFFECT;
constexpr unsigned int NUM_EFFECTS = (unsigned int)(MAX_EFFECT_ID) + 1;

/** Return a random Effect. */
inline Effect randomEffect() {
  return (Effect)random((uint32_t)MAX_RANDOM_EFFECT_ID + 1);
};

/** Return random flags appropriate for this effect/sentence. */
extern uint32_t newAnimationFlags(Effect e, const Sentence &s);

/** Print the name of the specified Effect enum to the debug console. */
extern void debugPrintEffect(const Effect e);


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
constexpr unsigned int MELT_OPTIMAL_HOLD_TIME = 3000; // Optimal hold phase timing for EF_MELT.
constexpr unsigned int MELT_MINIMUM_HOLD_TIME = 1000; // Enforce a min hold time this long.
constexpr unsigned int MELT_BLANK_TIME = 1500; // Millis @ end of outro when sign is held blank.

// The SLIDE_TO_END animation uses the following timing:
constexpr unsigned int SLIDE_TO_END_PER_WORD_ZIP = 100; // a 'light zip' moves thru words at 1x / 100ms
constexpr unsigned int SLIDE_TO_END_PER_WORD_HOLD = 350; // light zip 'holds' on a its destination word
constexpr unsigned int SLIDE_TO_END_MINIMUM_SENTENCE_HOLD = 1500;
constexpr unsigned int SLIDE_TO_END_DEFAULT_SENTENCE_HOLD = 2500;

// In intro-hold-outro mode animations, the 3 phases have specific names:
constexpr unsigned int PHASE_INTRO = 0;
constexpr unsigned int PHASE_HOLD = 1;
constexpr unsigned int PHASE_OUTRO = 2;

// Duration (millis) for which EF_ONE_AT_A_TIME shows each word:
constexpr unsigned int ONE_AT_A_TIME_WORD_DELAY = 1000;
// Number of phases (of len OAAT_WORD_DELAY) at the end when we hold a blank screen.
constexpr unsigned int ONE_AT_A_TIME_BLANK_PHASES = 2;

// Duration (millis) between lighting up words in EF_BUILD.
constexpr unsigned int BUILD_WORD_DELAY = 500;
// Number of phases (of len BUILD_WORD_DELAY) for which we hold the whole sentence
// after it's all lit in EF_BUILD.
constexpr unsigned int BUILD_HOLD_PHASES = 4;
constexpr unsigned int BUILD_HOLD_DURATION = BUILD_HOLD_PHASES * BUILD_WORD_DELAY;

// Duration (millis) between lighting up words in EF_BUILD_RANDOM.
constexpr unsigned int BUILD_RANDOM_WORD_DELAY = 1000;
constexpr unsigned int BUILD_RANDOM_HOLD_PHASES = 2;
constexpr unsigned int BUILD_RANDOM_HOLD_DURATION = BUILD_RANDOM_HOLD_PHASES * BUILD_RANDOM_WORD_DELAY;

// Duration (millis) between lighting up or turning off words in EF_SNAKE.
constexpr unsigned int SNAKE_WORD_DELAY = 750;

constexpr unsigned int ALL_BRIGHT_MILLIS = 10000; // Turn the whole sign on for 10 seconds by default
constexpr unsigned int ALL_DARK_MILLIS = 20000; // If the user presses an 'all dark' button, make them
                                                // really think they turned the whole thing off.

constexpr unsigned int FADE_LOVE_HATE_MILLIS = 12000;
constexpr unsigned int FADE_LOVE_HATE_INTRO_MILLIS = 1000;
constexpr unsigned int FADE_LOVE_HATE_OUTRO_MILLIS = 2000;

constexpr uint32_t ANIM_FLAG_FLICKER_COUNT_1 = 0x1; // One word should be flickering.
constexpr uint32_t ANIM_FLAG_FLICKER_COUNT_2 = 0x2; // Two words should be flickering.
constexpr uint32_t ANIM_FLAG_FLICKER_COUNT_3 = 0x4; // Three words should be flickering.

// Flag indicates a post-animation fade-over from "LOVE" to "HATE" or vice versa.
// Requires that LOVE or HATE be part of the sentence AND the Effect ends with the
// whole sentence visible.
constexpr uint32_t ANIM_FLAG_FADE_LOVE_HATE = 0x8;

// The whole sign should "glitch out" with all words flickering.
constexpr uint32_t ANIM_FLAG_FULL_SIGN_GLITCH_DARK = 0x10;
constexpr uint32_t ANIM_FLAG_FULL_SIGN_GLITCH_BRIGHT = 0x20;
// The buttons are remapped at the end of this animation.
constexpr uint32_t ANIM_FLAG_RESET_BUTTONS_ON_END = 0x40;

// In a random roll out of 1000, what's the likelihood of various numbers of signs flickering?
constexpr unsigned int FLICKER_LIKELIHOOD_MAX = 1000;
constexpr unsigned int FLICKER_LIKELIHOOD_1 = 120; // 1 sign: 12%
constexpr unsigned int FLICKER_LIKELIHOOD_2 = 170; // 2 signs: 5%
constexpr unsigned int FLICKER_LIKELIHOOD_3 = 190; // 3 signs: 2%
constexpr unsigned int FLICKER_LIKELIHOOD_ALL = 200; // Entire msg board: 1%

// In a random roll out of 1000, define the likelihood of the word "LOVE" in a sentence fading
// over to "HATE" (or vice versa).
constexpr unsigned int LOVE_HATE_LIKELIHOOD_MAX = 1000;
constexpr int LOVE_HATE_FADE_LIKELIHOOD = 650;

// When in ANIM_FLAG_FULL_SIGN_GLITCH_DARK state, use a very high flicker threshold
// so the signs are mostly off except when they randomly flick on briefly.
constexpr unsigned int FULL_SIGN_GLITCH_FLICKER_DARK_THRESHOLD = 925;
// ... the same, for the GLITCH_LIGHT flag
constexpr unsigned int FULL_SIGN_GLITCH_FLICKER_BRIGHT_THRESHOLD = 250;

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
  Effect getEffect() const { return _effect; };

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
  void _setParamsBuildRandom(const Sentence &s, const Effect e, uint32_t flags, uint32_t milliseconds);
  void _setParamsSnake(const Sentence &s, const Effect e, uint32_t flags, uint32_t milliseconds);
  void _setParamsSlide(const Sentence &s, const Effect e, uint32_t flags, uint32_t milliseconds);
  void _setParamsMelt(const Sentence &s, const Effect e, uint32_t flags, uint32_t milliseconds);
  void _setParamsAllBright(const Sentence &s, const Effect e, uint32_t flags, uint32_t milliseconds);
  void _setParamsAllDark(const Sentence &s, const Effect e, uint32_t flags, uint32_t milliseconds);
  void _setParamsFadeLoveHate(const Sentence &s, const Effect e, uint32_t flags, uint32_t milliseconds);
  void _setParamsNoEffect(const Sentence &s, const Effect e, uint32_t flags, uint32_t milliseconds);

  void _nextAppear();
  void _nextGlow();
  void _nextBlink();
  void _nextBlinkFast();
  void _nextOneAtATime();
  void _nextBuild();
  void _nextBuildRandom();
  void _nextSnake();
  void _nextSlide();
  void _nextMelt();
  void _nextAllBright();
  void _nextAllDark();
  void _nextFadeLoveHate();
  void _nextNoEffect();

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

  // EF_BUILD_RANDOM
  uint8_t _buildRandomOrder[NUM_SIGNS]; // Order we light up signs in this animation.

  // EF_FADE_LOVE_HATE
  int _loveHateFadeLoveOnThreshold; // Prob. of rand chance (out of THRESHOLD_MAX) that 'LOVE' is lit.
  int _loveHateFadeLoveOnDeltaPerTic; // Amount that threshold changes each frame.
  int _loveHateFrozenFramesRemaining;
  static constexpr int LOVE_HATE_FADE_THRESHOLD_MAX = 1000000;
};

extern Animation activeAnimation;

#endif // _ANIMATION_H

