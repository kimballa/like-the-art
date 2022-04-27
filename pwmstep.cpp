// (c) Copyright 2022 Aaron Kimball
//
// Operates on the Adafruit Feather M4 -- ATSAMD51 @ 120 MHz.
// Output a series of PWM waveforms on pin D10 (PA20) via TCC0.

#include<Arduino.h>

#define DEBUG
//#define DBG_WAIT_FOR_CONNECT
//#define DBG_START_PAUSED
#include <dbg.h>

// D6 -- PA 18
// altsel G (TCC0/WO[6])
constexpr unsigned int PORT_GROUP = 0;
constexpr unsigned int PORT_PIN = 18;
constexpr unsigned int PORT_PIN_BIT = (uint32_t)(1 << PORT_PIN);
constexpr unsigned int PORT_FN = 0x6; // 0=A, 1=B, ... 0x5=F, 0x6=G, ...

constexpr unsigned int PWM_CHANNEL = 0;

constexpr unsigned int COARSE_STEP_DELAY = 1000; // milliseconds per step.
constexpr unsigned int FINE_STEP_DELAY = 25; // milliseconds.
constexpr unsigned int HOLD_TOP_DELAY = 2000;
constexpr unsigned int HOLD_BOTTOM_DELAY = 500;

constexpr unsigned int PWM_FREQ = 2000; // 2 KHz
constexpr unsigned int PWM_CLOCK_HZ = 6000000; // We configure TCC0 to 48MHz prescaled by 8 = 6 MHz.
// What is the value where the PWM clock rolls over based on our target PWM frequency?
constexpr unsigned int PWM_WAVE_COUNT = (PWM_CLOCK_HZ / PWM_FREQ) - 1;

constexpr unsigned int NUM_STEPS_COARSE = 10;
constexpr unsigned int NUM_STEPS_FINE = 200;

constexpr unsigned int COARSE_STEP_SIZE = PWM_FREQ / NUM_STEPS_COARSE;
constexpr unsigned int FINE_STEP_SIZE = PWM_FREQ / NUM_STEPS_FINE;

constexpr int MODE_COARSE = 0;
constexpr int MODE_FINE_UP = 1;
constexpr int MODE_HOLD_TOP = 2;
constexpr int MODE_FINE_DOWN = 3;
constexpr int MODE_HOLD_BOTTOM = 4;

constexpr int MAX_MODE_STATE = MODE_HOLD_BOTTOM;

static int mode = MODE_COARSE;

static int step = 0;

/** Given int between 0 and PWM_FREQ, how many counter ticks does that take? */
constexpr unsigned int pwmDutyCount(const unsigned int duty) {
  unsigned int useDuty = duty;
  if (duty > PWM_FREQ) {
    useDuty = PWM_FREQ;
  }

  if (duty == 0) {
    return 0;
  } else if (duty == PWM_FREQ) {
    return PWM_WAVE_COUNT;
  } else {
    return (uint32_t)( (((float)duty / PWM_FREQ) * (PWM_WAVE_COUNT + 1)) - 1 );
  }
}

/** Set the CC comparator value to the associated duty cycle. */
void setPwmDuty(const unsigned int duty) {
  DBGPRINT("Updating PWM Duty cycle:");
  DBGPRINT(duty);
  TCC0->CC[PWM_CHANNEL].reg = pwmDutyCount(duty);
  while (TCC0->SYNCBUSY.reg & (1 << (PWM_CHANNEL + 8)));          // Wait for synchronization
}

void setup() {

  // Set as output.
  PORT->Group[PORT_GROUP].DIRSET.reg |= PORT_PIN_BIT;

  // Enable the peripheral multiplexer on output pin
  PORT->Group[PORT_GROUP].PINCFG[PORT_PIN].bit.PMUXEN = 1;

  // Set the D6 (PA18) peripheral multiplexer to peripheral (even port number) G(6): TCC0, Channel 0
  PORT->Group[PORT_GROUP].PMUX[PORT_PIN >> 1].reg |= PORT_PMUX_PMUXE(PORT_FN);

  DBGPRINT("ctrla");
  DBGPRINT(TCC0->CTRLA.reg);

  TCC0->CTRLA.bit.ENABLE = 0; // Start with TCC0 off.
  while (TCC0->SYNCBUSY.bit.ENABLE);       // Wait for synchronization

  MCLK->APBBMASK.reg |= MCLK_APBBMASK_TCC0; // enable TCC Bus Clock.

  // Set up the generic clock (GCLK7) to clock timer TCC0
  GCLK->GENCTRL[7].reg = GCLK_GENCTRL_DIV(1) |       // Divide the 48MHz clock source by divisor 1:
                                                     // 48MHz/1 = 48MHz
                         GCLK_GENCTRL_IDC |          // Set the duty cycle to 50/50 HIGH/LOW
                         GCLK_GENCTRL_GENEN |        // Enable GCLK7
                         GCLK_GENCTRL_SRC(GCLK_GENCTRL_SRC_DFLL);  // Select 48MHz DFLL clock source
                         //GCLK_GENCTRL_SRC_DPLL1;     // Select 100MHz DPLL clock source
                         //GCLK_GENCTRL_SRC_DPLL0;     // Select 120MHz DPLL clock source
  while (GCLK->SYNCBUSY.bit.GENCTRL7);               // Wait for synchronization

  GCLK->PCHCTRL[TCC0_GCLK_ID].reg =
                          GCLK_PCHCTRL_CHEN |        // Enable the TCC0 peripheral channel
                          GCLK_PCHCTRL_GEN_GCLK7;    // Connect generic clock 7 to TCC0

  TCC0->CTRLA.reg = TC_CTRLA_PRESCALER_DIV8 |        // Set prescaler to 8, 48MHz/8 = 6MHz
                    TC_CTRLA_PRESCSYNC_PRESC;        // Set the reset/reload to trigger on prescaler clock

  TCC0->WAVE.reg = TC_WAVE_WAVEGEN_NPWM;   // Set-up TCC0 timer for Normal (single slope) PWM mode (NPWM)
  while (TCC0->SYNCBUSY.bit.WAVE);         // Wait for synchronization

  TCC0->PER.reg = TCC_PER_PER(PWM_WAVE_COUNT);  // Set-up the PER (period) register for 2KHz PWM
  TCC0->COUNT.reg = 0;
  while (TCC0->SYNCBUSY.bit.PER);          // Wait for synchronization

  // Set up the CC (counter compare), channel 0 register for the selected duty cycle.
  setPwmDuty(PWM_WAVE_COUNT / 2); // start with 50/50.

  TCC0->CTRLA.bit.ENABLE = 1;              // Enable timer TCC0
  while (TCC0->SYNCBUSY.bit.ENABLE);       // Wait for synchronization

  DBGPRINT("ctrla");
  DBGPRINT(TCC0->CTRLA.reg);
  DBGPRINT("wave");
  DBGPRINT(TCC0->WAVE.reg);
  DBGPRINT("per");
  DBGPRINT(TCC0->PER.reg);
  DBGPRINT("cc for channel");
  DBGPRINT(TCC0->CC[PWM_CHANNEL].reg);
  DBGPRINT("count");
  DBGPRINT(TCC0->COUNT.reg);
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
      setPwmDuty(step * COARSE_STEP_SIZE);
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
      setPwmDuty(step * FINE_STEP_SIZE);
      delay(FINE_STEP_DELAY);
    };
    break;
  case MODE_HOLD_TOP:
    {
      setPwmDuty(PWM_FREQ);
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
      setPwmDuty(step * FINE_STEP_SIZE);
      delay(FINE_STEP_DELAY);
    };
    break;
  case MODE_HOLD_BOTTOM:
    {
      // Blank.
      setPwmDuty(0);
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
