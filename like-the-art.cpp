// (c) Copyright 2022 Aaron Kimball
//
// Operates on the Adafruit Feather M4 -- ATSAMD51 @ 120 MHz.
// Output a series of PWM waveforms on pin D6 (PA18) via TCC0.

#include "like-the-art.h"

// DARK sensor is on A4 / D18.
constexpr uint8_t DARK_SENSOR_PIN = 18;

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

PwmTimer pwmTimer(PORT_GROUP, PORT_PIN, PORT_FN, TCC, PWM_CHANNEL, PWM_FREQ, DEFAULT_PWM_PRESCALER);

// Integrated neopixel on D8.
Adafruit_NeoPixel neoPixel(1, 8, NEO_GRB | NEO_KHZ800);

// I2C is connected to 3 PCF8574N's, on channel 0x20 (LED0), 0x21 (LED1), and 0x23 (buttons).
// Two declared here for interacting with LEDs. The buttonBank is declared in button.cpp.
I2CParallel parallelBank0;
I2CParallel parallelBank1;

MacroState macroState = MS_RUNNING;

// Neopixel intensity is increasing each tick if true.
static bool isNeoPixelIncreasing = true;
static constexpr float NEO_PIXEL_INCREMENT = 1.0f / 256.0f;
static constexpr uint32_t NEO_PIXEL_MAX_INTENSITY = 20; // out of 255.
static float neoPixelIntensity = 0;

// NeoPixel color reflects current MacroState.
static inline void updateNeoPixel() {
  if (isNeoPixelIncreasing) {
    neoPixelIntensity += NEO_PIXEL_INCREMENT;
    if (neoPixelIntensity >= 1 - NEO_PIXEL_INCREMENT) {
      isNeoPixelIncreasing = false;
      neoPixelIntensity = 1.0f;
    }
  } else {
    neoPixelIntensity -= NEO_PIXEL_INCREMENT;
    if (neoPixelIntensity <= NEO_PIXEL_INCREMENT) {
      isNeoPixelIncreasing = true;
      neoPixelIntensity = 0.0f;
    }
  }

  uint8_t colorIntensity = NEO_PIXEL_MAX_INTENSITY * neoPixelIntensity;

  neoPixel.clear();
  switch(macroState) {
  case MS_RUNNING:
    neoPixel.setPixelColor(0, neoPixelColor(0, colorIntensity, 0)); // Green
    break;
  case MS_ADMIN:
    neoPixel.setPixelColor(0, neoPixelColor(colorIntensity, 0, 0)); // Red
    break;
  case MS_WAITING:
    neoPixel.setPixelColor(0, neoPixelColor(0, 0, colorIntensity)); // Blue
    break;
  };
  neoPixel.show();
}

static void printWhyLastReset() {
  switch (RSTC->RCAUSE.reg) {
  case 1<<0:
    DBGPRINT("Last reset: power-on");
    break;
  case 1<<1:
    DBGPRINT("Last reset: 1V2 brown-out detected");
    break;
  case 1<<2:
    DBGPRINT("Last reset: 3V3 brown-out detected");
    break;
  case 1<<3:
    DBGPRINT("Last reset: <reason reserved 3>"); // ???
    break;
  case 1<<4:
    DBGPRINT("Last reset: external reset");
    break;
  case 1<<5:
    DBGPRINT("Last reset: WDT timeout");
    break;
  case 1<<6:
    DBGPRINT("Last reset: system reset request");
    break;
  case 1<<7:
    DBGPRINT("Last reset: <reason reserved 7>"); // ???
    break;
  }
}

void setup() {
  DBGSETUP();

  // Connect to I2C parallel bus expanders for signs.
  Wire.begin();
  parallelBank0.init(0 + I2C_PCF8574_MIN_ADDR, I2C_SPEED_STANDARD);
  parallelBank0.write(0); // Turn off signs asap so we don't spend too much time in all-on state.
#if IS_TARGET_PRODUCTION == 1
  parallelBank1.init(1 + I2C_PCF8574_MIN_ADDR, I2C_SPEED_STANDARD);
  parallelBank1.write(0);
#endif // I2C bank 1 only in prod, not in breadboard.

  printWhyLastReset();

  // If we don't already have SmartEEPROM space configured, reconfigure
  // the NVM controller to allow that. (Will trigger instant reset.)
  // If the fuses are already correct, this will do nothing and continue.
  programEEPROMFuses(1, 0); // sblk=1, psz=0 => 512 byte EEPROM.
  setEEPROMCommitMode(true); // Require explicit commit for EEPROM data changes.

  // Set up neopixel
  neoPixel.begin();
  neoPixel.clear(); // start with pixel turned off
  updateNeoPixel();
  neoPixel.show();

  // Load field configuration, which specifies the max brightness pwm level to use.
  if (loadFieldConfig(&fieldConfig) == FIELD_CONF_EMPTY) {
    // No field configuration initialized. Use defaults.
    int initConfigRet = initDefaultFieldConfig();
    if (initConfigRet != EEPROM_SUCCESS) {
      DBGPRINTI("Warning: got error code when initializing field config:", initConfigRet);
    }
  }

  // Print current config'd brightness to dbg console.
  printCurrentBrightness();

  // Set up PWM on PORT_GROUP:PORT_PIN via TCC0.
  pwmTimer.setupTcc();

  // Connects button-input I2C and configures Button dispatch handler methods.
  setupButtons();

  pinMode(DARK_SENSOR_PIN, INPUT);

  // Define signs and map them to I/O channels.
  setupSigns(parallelBank0, parallelBank1);
  setupSentences(); // Define collections of signs for each sentence.
}

// We want the sensor to be stable for a full second before changing state.
static constexpr unsigned int DARK_SENSOR_DEBOUNCE_MILLIS = 1000;
static unsigned int lastDarkSensorChangeTime = 0;

// After the sensor triggers a state change, we commit to that state for 60s.
static constexpr unsigned int DARK_SENSOR_STATE_DELAY_MILLIS = 60000;
static unsigned int lastDarkStateChangeTime = 0;

static uint8_t prevDark = 0; // prior polled value of the gpio pin.

/**
 * Poll the DARK sensor pin. If DARK=1 then it's dark out and we can display the magic.
 * If DARK=0 then it's light out and we should be in idle mode.
 * n.b. that if we're in admin mode, the dark sensor should not cause a state transition;
 * we stay in admin mode day or night.
 */
static void pollDarkSensor() {
  uint8_t isDark = digitalRead(DARK_SENSOR_PIN);
  unsigned int now = millis();

  if (isDark != prevDark) {
    lastDarkSensorChangeTime = now;
    prevDark = isDark;
  }

  // Check that the sensor value has been stable (debounced) for long enough.
  unsigned int sensorStabilityTime = now - lastDarkSensorChangeTime;
  bool sensorIsStable = sensorStabilityTime >= DARK_SENSOR_DEBOUNCE_MILLIS;

  // Schmitt-trigger for state changes: even on a stable sensor, don't change state
  // back and forth particularly quickly; commit to a new state for a reasonable dwell time.
  unsigned int stateDuration = now - lastDarkStateChangeTime;
  bool stateDwellLongEnough = stateDuration > DARK_SENSOR_STATE_DELAY_MILLIS;

  // In order to change state, we need a stable sensor AND enough time in the prior state.
  bool changeAllowed = sensorIsStable && stateDwellLongEnough;

  if (changeAllowed && isDark && macroState == MS_WAITING) {
    // Time to start the show.
    lastDarkStateChangeTime = now;
    setMacroStateRunning();
  } else if (changeAllowed && !isDark && macroState == MS_RUNNING) {
    // The sun has found us; pack up for the day.
    lastDarkStateChangeTime = now;
    setMacroStateWaiting();
  }
}

void setMacroStateRunning() {
  DBGPRINT(">>>> Entering RUNNING MacroState <<<<");
  macroState = MS_RUNNING;
  attachStandardButtonHandlers();
}

void setMacroStateWaiting() {
  DBGPRINT(">>>> Entering WAITING MacroState <<<<");
  macroState = MS_WAITING;
  attachStandardButtonHandlers();
  allSignsOff();
  // TODO(aaron): Put the ATSAMD51 to sleep for a while?
  // TODO(aaron): If we do go to sleep, we need to enable an interrupt to watch for
  // the BTN_INT gpio signal. Otherwise we'll miss any keypresses while in sleep (meaning
  // you won't be able to enter admin mode during the day).
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

  if (delay_time > 0) {
    delayMicroseconds(delay_time);
  }
}

/** Main loop body when we're in the MS_RUNNING macro state. */
static void loopStateRunning() {

  if (activeAnimation.isRunning()) {
    // We're currently in an animation; just advance the next frame.
    activeAnimation.next();
    return;
  }

  // Need to choose a new animation
  Effect e = (Effect)random(3); // appear, glow, or blink.
  Sentence s(0, random(16)); // light up some combo of the 4 LEDs we have.

  activeAnimation.setParameters(s, e, 0, 6000); // start a new animation for 6 seconds
  activeAnimation.start();
}

void loop() {
  unsigned int loop_start_micros = micros();

  // Poll buttons and dark sensor every loop.
  pollButtons();
  pollDarkSensor();

  updateNeoPixel(); // Display current macroState on NeoPixel.
  logSignStatus();

  // Run the macro-state-specific loop body.
  switch(macroState) {
  case MS_RUNNING:
    loopStateRunning();
    break;
  case MS_ADMIN:
    loopStateAdmin();
    break;
  case MS_WAITING:
    // Definitionally nothing to do in the waiting state...
    // TODO - instead of loops of short-sleep & polling the dark sensor every 10ms,
    // we should put the system into a lower-power mode and query every minute or so
    // or wait for an interrupt to wake us.
    break;
  default:
    DBGPRINTI("ERROR -- unknown MacroState:", macroState);
    DBGPRINT("Reverting to running state");
    macroState = MS_RUNNING;
  }

  // At the end of each loop iteration, sleep until this iteration is LOOP_MICROS long.
  sleep_loop_increment(loop_start_micros);
}
