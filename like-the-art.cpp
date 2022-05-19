// (c) Copyright 2022 Aaron Kimball
//
// Operates on the Adafruit Feather M4 -- ATSAMD51 @ 120 MHz.
// Output a series of PWM waveforms on pin D6 (PA18) via TCC0.

#include "like-the-art.h"

// Value between 0--255 controlling how bright the onboard NeoPixel is. (255=max)
constexpr unsigned int NEOPIXEL_BRIGHTNESS = 64;
constexpr uint32_t neopx_color_standard = neoPixelColor(0, 255, 0); // green in std mode.
constexpr uint32_t neopx_color_debug = neoPixelColor(255, 0, 0); // red in debug mode.
constexpr uint32_t neopx_color_idle = neoPixelColor(0, 0, 255); // blue while waiting for dark


// PWM is on D6 -- PA18, altsel G (TCC0/WO[6]; channel 0)
constexpr unsigned int PORT_GROUP = 0; // 0 = PORTA
constexpr unsigned int PORT_PIN = 18;
constexpr unsigned int PORT_FN = 0x6; // 0=A, 1=B, ... 0x5=F, 0x6=G, ...

static Tcc* const TCC = TCC0;
constexpr unsigned int PWM_CHANNEL = 0;
constexpr unsigned int PWM_FREQ = 6000; // 6 KHz

constexpr unsigned long COARSE_STEP_DELAY = 1000 * 1000; // microseconds per step.
constexpr unsigned long FINE_STEP_DELAY = 25 * 1000; // microseconds.
constexpr unsigned long HOLD_TOP_DELAY = 2000 * 1000;
constexpr unsigned long HOLD_BOTTOM_DELAY = 500 * 1000;

constexpr unsigned int NUM_STEPS_COARSE = 10;
constexpr unsigned int NUM_STEPS_FINE = 200;

constexpr unsigned int COARSE_STEP_SIZE = PWM_FREQ / NUM_STEPS_COARSE;
constexpr unsigned int FINE_STEP_SIZE = PWM_FREQ / NUM_STEPS_FINE;

/** Every loop iteration lasts for 10ms. */
constexpr unsigned int LOOP_MICROS = 10 * 1000;

/**
 * When we want to hold at a current display state, we sleep in LOOP_MICROS increments
 * until remaining_sleep_micros is zero.
 */
unsigned int remaining_sleep_micros = 0;

PwmTimer pwmTimer(PORT_GROUP, PORT_PIN, PORT_FN, TCC, PWM_CHANNEL, PWM_FREQ, DEFAULT_PWM_PRESCALER);

// Integrated neopixel on D8.
Adafruit_NeoPixel neoPixel(1, 8, NEO_GRB | NEO_KHZ800);

// I2C is connected to 3 PCF8574N's, on channel 0x20, 0x21, and 0x22.
I2CParallel parallelBank0;
I2CParallel parallelBank1;
I2CParallel buttonBank;

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

// TODO(aaron): Move button logic out to a different cpp module.
static uint8_t lastButtonState = 0xFF;
void buttonBankISR() {
  uint8_t newButtonState = buttonBank.read();
  if (newButtonState != lastButtonState) {
    DBGPRINT("Button state change");
    DBGPRINT(newButtonState);
    lastButtonState = newButtonState;
  }
}

void setup() {
  DBGSETUP();

  // Set up neopixel
  neoPixel.begin();
  neoPixel.clear(); // start with pixel turned off
  neoPixel.setPixelColor(0, neopx_color_idle);
  neoPixel.setBrightness(NEOPIXEL_BRIGHTNESS);
  neoPixel.show();

  // Set up PWM on PORT_GROUP:PORT_PIN via TCC0.
  pwmTimer.setupTcc();

  // Connect to I2C parallel bus expanders.
  Wire.begin();
  parallelBank0.init(0 + I2C_PCF8574_MIN_ADDR, I2C_SPEED_STANDARD);
  parallelBank1.init(1 + I2C_PCF8574_MIN_ADDR, I2C_SPEED_STANDARD);

  buttonBank.init(   2 + I2C_PCF8574_MIN_ADDR, I2C_SPEED_STANDARD);
  buttonBank.enableInputs(0xFF); // all 8 channels of button bank are inputs.
  buttonBank.initInterrupt(9, buttonBankISR);

  // Define signs and map them to I/O channels.
  setupSigns(parallelBank0, parallelBank1);
}

/**
 * Sleep for the appropriate amount of time to make each loop iteration
 * take an equal LOOP_MICROS microseconds of time.
 */
static inline void sleep_loop_increment(unsigned long loop_start_micros) {
  unsigned long cur_micros = micros();
  unsigned long delay_time = LOOP_MICROS;
  if (cur_micros < loop_start_micros) {
    // Clock wraparound happened mid-loop. Pretend the loop was zero-duration.
    cur_micros = loop_start_micros;
  }
  unsigned long loop_exec_duration = cur_micros - loop_start_micros;
  if (loop_exec_duration > LOOP_MICROS) {
    delay_time = 0; // The loop actually exceeded the target interval. No need to sleep.
  } else {
    delay_time -= loop_exec_duration; // Subtract loop runtime from total sleep.
  }

  if (delay_time > remaining_sleep_micros) {
    // Don't sleep for longer than necessary to pay off the sleep debt for the current state cycle.
    delay_time = remaining_sleep_micros;
  }

  delayMicroseconds(delay_time);

  if (LOOP_MICROS >= remaining_sleep_micros) {
    // Sleep debt is paid off. Next loop we advance state.
    remaining_sleep_micros = 0;
  } else {
    remaining_sleep_micros -= LOOP_MICROS;
  }
}

void loop() {
  unsigned int loop_start_micros = micros();

  // Poll buttons and dark sensor every loop.
  DBGPRINT(buttonBank.read());

  if (remaining_sleep_micros > 0) {
    // Do not advance the state machine. Our existing sleep debt remains to be paid off.
    sleep_loop_increment(loop_start_micros);
    return;
  }

  // Advance to next state.

/*
  DBGPRINT("mode:");
  DBGPRINT(mode);
  DBGPRINT("step:");
  DBGPRINT(step);
  */

  switch (mode) {
  case MODE_COARSE:
    {
      signs[0]->enable();
      step++;
      if (step >= NUM_STEPS_COARSE) {
        step = 0;
        // We wrapped. Next mode is fine.
        mode++;
      }
      pwmTimer.setDutyCycle(step * COARSE_STEP_SIZE);
      remaining_sleep_micros = COARSE_STEP_DELAY;
    };
    break;
  case MODE_FINE_UP:
    {
      // Fine step mode, increasing brightness.
      signs[0]->enable();
      step++;
      if (step >= NUM_STEPS_FINE) {
        mode++; // next mode: hold at top brightness.
      }
      pwmTimer.setDutyCycle(step * FINE_STEP_SIZE);
      remaining_sleep_micros = FINE_STEP_DELAY;
    };
    break;
  case MODE_HOLD_TOP:
    {
      signs[0]->enable();
      pwmTimer.setDutyCycle(PWM_FREQ);
      remaining_sleep_micros = HOLD_TOP_DELAY;
      step = NUM_STEPS_FINE;
      mode++;
    };
    break;
  case MODE_FINE_DOWN:
    {
      signs[0]->enable();
      step--;
      if (step == 0) {
        mode++;
      }
      pwmTimer.setDutyCycle(step * FINE_STEP_SIZE);
      remaining_sleep_micros = FINE_STEP_DELAY;
    };
    break;
  case MODE_HOLD_BOTTOM:
    {
      signs[0]->disable();
      // Blank.
      pwmTimer.setDutyCycle(0);
      remaining_sleep_micros = HOLD_BOTTOM_DELAY;
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

  // At the end of each loop iteration, sleep until this iteration is LOOP_MICROS long.
  // We then continue to do poll-only iterations of LOOP_MICROS until remaining_sleep_micros
  // is zero, at which point we can advance to the next state.
  sleep_loop_increment(loop_start_micros);
}
