// (c) Copyright 2022 Aaron Kimball
//
// Operates on the Adafruit Feather M4 -- ATSAMD51 @ 120 MHz.
// Output a series of PWM waveforms on pin D10 (PA20) via TCC0.

#include "like-the-art.h"

// PWM is on D6 -- PA18, altsel G (TCC0/WO[6]; channel 0)
constexpr unsigned int PORT_GROUP = 0; // 0 = PORTA
constexpr unsigned int PORT_PIN = 18;
constexpr unsigned int PORT_FN = 0x6; // 0=A, 1=B, ... 0x5=F, 0x6=G, ...

constexpr unsigned int PWM_CHANNEL = 0;
constexpr unsigned int PWM_FREQ = 6000; // 6 KHz

constexpr unsigned int COARSE_STEP_DELAY = 1000; // milliseconds per step.
constexpr unsigned int FINE_STEP_DELAY = 25; // milliseconds.
constexpr unsigned int HOLD_TOP_DELAY = 2000;
constexpr unsigned int HOLD_BOTTOM_DELAY = 500;

constexpr unsigned int NUM_STEPS_COARSE = 10;
constexpr unsigned int NUM_STEPS_FINE = 200;

constexpr unsigned int COARSE_STEP_SIZE = PWM_FREQ / NUM_STEPS_COARSE;
constexpr unsigned int FINE_STEP_SIZE = PWM_FREQ / NUM_STEPS_FINE;

PwmTimer pwmTimer(PORT_GROUP, PORT_PIN, PORT_FN, TCC0, PWM_CHANNEL, PWM_FREQ, DEFAULT_PWM_PRESCALER);


// State machine that drives this skech is based on cycling through the following modes,
// where we then take a number of PWM-varying actions that cycle 'step' through different ranges
// and at different speeds.
constexpr int MODE_COARSE = 0;
constexpr int MODE_FINE_UP = 1;
constexpr int MODE_HOLD_TOP = 2;
constexpr int MODE_FINE_DOWN = 3;
constexpr int MODE_HOLD_BOTTOM = 4;

constexpr int MAX_MODE_STATE = MODE_HOLD_BOTTOM;

static int mode = MODE_COARSE;

static int step = 0;

void setup() {
  pwmTimer.setupTcc();
}

void loop() {
  DBGPRINT("mode:");
  DBGPRINT(mode);
  DBGPRINT("step:");
  DBGPRINT(step);

  switch (mode) {
  case MODE_COARSE:
    {
      step++;
      if (step >= NUM_STEPS_COARSE) {
        step = 0;
        // We wrapped. Next mode is fine.
        mode++;
      }
      pwmTimer.setDutyCycle(step * COARSE_STEP_SIZE);
      delay(COARSE_STEP_DELAY);
    };
    break;
  case MODE_FINE_UP:
    {
      // Fine step mode, increasing brightness.
      step++;
      if (step >= NUM_STEPS_FINE) {
        mode++; // next mode: hold at top brightness.
      }
      pwmTimer.setDutyCycle(step * FINE_STEP_SIZE);
      delay(FINE_STEP_DELAY);
    };
    break;
  case MODE_HOLD_TOP:
    {
      pwmTimer.setDutyCycle(PWM_FREQ);
      delay(HOLD_TOP_DELAY);
      step = NUM_STEPS_FINE;
      mode++;
    };
    break;
  case MODE_FINE_DOWN:
    {
      step--;
      if (step == 0) {
        mode++;
      }
      pwmTimer.setDutyCycle(step * FINE_STEP_SIZE);
      delay(FINE_STEP_DELAY);
    };
    break;
  case MODE_HOLD_BOTTOM:
    {
      // Blank.
      pwmTimer.setDutyCycle(0);
      delay(HOLD_BOTTOM_DELAY);
      step = 0;
      mode = MODE_COARSE; // Back to beginning of cycle.
    };
    break;
  default:
    DBGPRINT("ERROR *** Invalid mode state:");
    DBGPRINT(mode);
    mode = MODE_COARSE;
    step = 0;
    break;
  }

  if (mode > MAX_MODE_STATE) {
    // Reset state machine.
    mode = 0;
    step = 0;
  }
}
