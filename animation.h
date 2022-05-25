// (c) Copyright 2022 Aaron Kimball

#ifndef _ANIMATION_H
#define _ANIMATION_H

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
  Animation(): _sentence(0, 0), _effect(EF_APPEAR) {};

  void setParameters(const Sentence &s, const Effect e, unsigned int milliseconds);

  bool isRunning() const;
  bool isComplete() const;
  const Sentence &getSentence() const { return _sentence; };

  void start(); // Start the animation sequence.
  void next(); // Perform the next step of animation.
  void stop(); // Halt the animation sequence even if there's part remaining.

private:
  Sentence _sentence;
  Effect _effect;
  unsigned int _remainingTime; // millis.
  bool _isRunning;

  unsigned int _phaseDuration; // millis per phase
  unsigned int _phaseRemainingMillis; // millis remaining in current phase
  unsigned int _phaseCountRemaining; // number of phases to go.

};

extern Animation activeAnimation;

#endif // _ANIMATION_H

