// (c) Copyright 2022 Aaron Kimball
//
// Operates on the Adafruit Feather M4 -- ATSAMD51 @ 120 MHz.
// Output a series of PWM waveforms on pin D6 (PA18) via TCC0.

#include "like-the-art.h"

// DARK sensor is on A4 / D18.
constexpr uint8_t DARK_SENSOR_PIN = 18;
constexpr uint8_t DARK_SENSOR_ANALOG = A4;

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

// State machine to determine whether it's dark or daylight. This system only operates
// when it's dark; we go into a no-op sleep mode for daylight.

// We want the DARK sensor to be stable for a few seconds before changing state.
static constexpr unsigned int DARK_SENSOR_DEBOUNCE_MILLIS = 5000;
static unsigned int lastDarkSensorChangeTime = 0;

// After the sensor triggers a state change, we commit to that state for 60s.
static constexpr unsigned int DARK_SENSOR_STATE_DELAY_MILLIS = 60000;
static unsigned int lastDarkStateChangeTime = 0;

static constexpr uint8_t DARK = 1;
static constexpr uint8_t LIGHT = 0;

static uint8_t prevDark = DARK; // Prior polled value of the DARK gpio pin within debouncing period.
static uint8_t debouncedDarkState = DARK; // Fully-debounced determination of DARK gpio state.

// For analog reading of DARK sensor.
static uint16_t darkAnalogValues[AVG_NUM_DARK_SAMPLES];
static uint8_t lastAnalogDarkIdx = 0;

// Threshold in 0..1023 for "how dark does it need to be for us to say it's DARK".
// 0 is very bright. 1023 is quite dark indeed.
static constexpr uint16_t ANALOG_DARK_SENSOR_IS_DARK_THRESHOLD = 640;
// Threshold for opposite direction: when dark, at what brightness would we
// subsequently say it is light out?
static constexpr uint16_t ANALOG_DARK_SENSOR_IS_LIGHT_THRESHOLD = 580;

/**
 * Poll the DARK sensor pin. If DARK=1 then it's dark out and we can display the magic.
 * If DARK=0 then it's light out and we should be in idle mode.
 * n.b. that if we're in admin mode, the dark sensor should not cause a state transition;
 * we stay in admin mode day or night.
 *
 * Return true if we got a valid reading, false if we're just collecting sample data.
 */
static bool pollDarkSensor() {
  // Perform analog read of DARK sensor to show value between 0...1024
  // as to how dark it is outside according to the sensor. (This takes ~27us / reading.)
  // We collect a series of (noisy) readings in a row and average them together to get
  // a useful value. While still collecting data, this fn returns false and does not
  // yet make a state-change judgement.
  uint16_t darkAnalogVal = analogRead(DARK_SENSOR_ANALOG);
  darkAnalogValues[lastAnalogDarkIdx++] = darkAnalogVal;
  if (lastAnalogDarkIdx < AVG_NUM_DARK_SAMPLES) {
    // Didn't get a final averaged sample. Still collecting data.
    return false;
  }

  uint16_t averagedDarkReading = 0;
  // We have read enough values, average them.
  uint32_t sum = 0;
  for (uint8_t i = 0; i < AVG_NUM_DARK_SAMPLES; i++) {
    sum += darkAnalogValues[i];
  }
  averagedDarkReading = sum / AVG_NUM_DARK_SAMPLES;
  lastAnalogDarkIdx = 0; // Reset array for next series of readings.

  unsigned int now = millis();
  // New value of isDark is Schmitt-triggered; depending on prior state (debouncedDarkState),
  // we use a different threshold to determine the current isDark state.
  uint8_t isDark;
  if (debouncedDarkState == LIGHT) {
    // We currently believe it is daylight. Use higher darkness threshold to determine if it's dark yet.
    isDark = (averagedDarkReading > ANALOG_DARK_SENSOR_IS_DARK_THRESHOLD) ? DARK : LIGHT;
  } else {
    // debouncedDarkState == DARK; we have previously affirmed it's dark out.
    // Use lower darkness threshold to determine if it's now daylight.
    // (We track this independently of macroState because MS_ADMIN state confuses matters.
    // What matters here is this fn's internal dark/light Schmitt trigger state machine.)
    isDark = (averagedDarkReading < ANALOG_DARK_SENSOR_IS_LIGHT_THRESHOLD) ? LIGHT : DARK;
  }

  #ifdef REPORT_ANALOG_DARK_SENSOR
    DBGPRINTU("DARK sensor avg:", averagedDarkReading);
    // DBGPRINTU("Current DARK Bool:", isDark);
  #endif // REPORT_ANALOG_DARK_SENSOR

  if (isDark != prevDark) {
    lastDarkSensorChangeTime = now;
    prevDark = isDark;
  }

  // Check that the sensor value has been stable (debounced) for long enough.
  unsigned int sensorStabilityTime = now - lastDarkSensorChangeTime;
  bool sensorIsStable = sensorStabilityTime >= DARK_SENSOR_DEBOUNCE_MILLIS;

  // Lock in our state changes: even on a stable sensor, don't change state back and
  // forth particularly quickly; commit to a new state for a reasonable dwell time.
  unsigned int stateDuration = now - lastDarkStateChangeTime;
  bool stateDwellLongEnough = stateDuration > DARK_SENSOR_STATE_DELAY_MILLIS;

  // In order to change state, we need a stable sensor AND enough time in the prior state.
  bool changeAllowed = sensorIsStable && stateDwellLongEnough;
  if (changeAllowed) {
    // We have a fully-debounced state to record. Save it so we know which way to aim
    // the Schmitt trigger next.
    debouncedDarkState = isDark;
  }

  if (changeAllowed && isDark && macroState == MS_WAITING) {
    // Time to start the show.
    lastDarkStateChangeTime = now;
    setMacroStateRunning();
  } else if (changeAllowed && !isDark && macroState == MS_RUNNING) {
    // The sun has found us; pack up for the day.
    lastDarkStateChangeTime = now;
    setMacroStateWaiting();
  }

  return true;
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

  pinMode(DARK_SENSOR_PIN, INPUT);

  // Define signs and map them to I/O channels.
  setupSigns(parallelBank0, parallelBank1);
  setupSentences(); // Define collections of signs for each sentence.

  // Initialize random seed for random choices of button assignment
  // and sentence/animation combos to show. Analog read from A3 (disconnected/floating).
  randomSeed(analogRead(3));

  // Connects button-input I2C and configures Button dispatch handler methods.
  setupButtons();

  // For dark sensor; set up analog reference and discard a few reads to get accurate ones.
  analogReference(AR_DEFAULT);
  for (uint8_t i = 0; i < 10; i++) {
    delay(5);
    analogRead(DARK_SENSOR_ANALOG);
  }

  // Decide whether to begin in RUNNING (i.e. "DARK") mode or WAITING (DARK==0; daylight).
  // Repeatedly call pollDarkSensor() until we get enough readings to have a valid average.
  // Put the current DARK sensor boolean in prevDark, and immediately use that to set the initial
  // state without waiting for a full multi-second debounce cycle.
  while (!pollDarkSensor()) {
    delay(1);
  }

  if (prevDark == LIGHT) {
    setMacroStateWaiting();
  } else {
    setMacroStateRunning();
  }

  debouncedDarkState = prevDark;  // Make our debouncer state consistent with macro state.
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
  // TODO(aaron): If we do go to sleep, we need to enable an interrupt to watch for the
  // BTN_INT gpio signal. (And an INT on the 9th button.) Otherwise we'll miss any
  // keypresses while in sleep (meaning you won't be able to enter admin mode during the
  // day).
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
  Effect e = (Effect)random(5); // appear, glow, blink, fast blink, one-at-a-time.
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
    setMacroStateRunning();
  }

  // At the end of each loop iteration, sleep until this iteration is LOOP_MICROS long.
  sleep_loop_increment(loop_start_micros);
}
