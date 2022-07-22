// (c) Copyright 2022 Aaron Kimball
//
// Analog DARK sensor control.
//
// State machine to determine whether it's dark or daylight. This system only operates
// when it's dark; we go into a no-op sleep mode for daylight.

#include "like-the-art.h"

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
static uint16_t darkAnalogValues[AVG_NUM_DARK_SAMPLES]; // Raw data to compute next avg reading.
static uint8_t lastAnalogDarkIdx = 0; // next idx into darkAnalogValues.
static uint16_t lastAveragedDarkVal; // most recent averaged reading.

uint16_t getLastDarkSensorValue() {
  return lastAveragedDarkVal;
}

// Threshold in 0..1023 for "how dark does it need to be for us to say it's DARK".
// 0 is very bright. 1023 is quite dark indeed.
static constexpr int16_t ANALOG_DARK_SENSOR_IS_DARK_THRESHOLD = 640;
// Threshold for opposite direction: when dark, at what brightness would we
// subsequently say it is light out?
static constexpr int16_t ANALOG_DARK_SENSOR_IS_LIGHT_THRESHOLD = 580;

// The active threshold levels to use after calibration is applied.
static uint16_t calibratedDarkThreshold = ANALOG_DARK_SENSOR_IS_DARK_THRESHOLD;
static uint16_t calibratedLightThreshold = ANALOG_DARK_SENSOR_IS_LIGHT_THRESHOLD;

// Offset moves thresholds up and down in units of 20/1024.
static constexpr int16_t ANALOG_SENSOR_SHIFT_PER_OFFSET = 20;

static constexpr int8_t MAX_CAL_OFFSET = 5;
static constexpr int8_t MIN_CAL_OFFSET = -5;

// Shift the calibrated thresholds we use. Offset is in units of 20 and should be
// between +5 and -5. e.g, 0 sets DARK at 680/1024; +2 sets it at 720/1024; -3 sets it at
// 620/1024...
void adjustDarkSensorCalibration(int8_t offset) {
  if (offset > MAX_CAL_OFFSET) {
    DBGPRINTI("*** WARNING: DARK calibration offset exceeds max:", offset);
    offset = MAX_CAL_OFFSET;
  } else if (offset < MIN_CAL_OFFSET) {
    DBGPRINTI("*** WARNING: DARK calibration offset exceeds min:", offset);
    offset = MIN_CAL_OFFSET;
  }

  // Save the calibrated value of `offset` into the main config structure
  // that we can persist to EEPROM.
  fieldConfig.darkSensorCalibration = offset;

  calibratedDarkThreshold = ANALOG_DARK_SENSOR_IS_DARK_THRESHOLD +
      offset * ANALOG_SENSOR_SHIFT_PER_OFFSET;

  calibratedLightThreshold = ANALOG_DARK_SENSOR_IS_LIGHT_THRESHOLD +
      offset * ANALOG_SENSOR_SHIFT_PER_OFFSET;

  printDarkThreshold();
}

uint16_t getDarkThreshold() {
  return calibratedDarkThreshold;
}

uint16_t getLightThreshold() {
  return calibratedLightThreshold;
}

void setupDarkSensor() {
  pinMode(DARK_SENSOR_PIN, INPUT);

  // Load in EEPROM-saved calibration.
  adjustDarkSensorCalibration(fieldConfig.darkSensorCalibration);

  // For dark sensor; set up analog reference and discard a few reads to get accurate ones.
  analogReference(AR_DEFAULT);
  for (uint8_t i = 0; i < 10; i++) {
    delay(5);
    analogRead(DARK_SENSOR_ANALOG);
  }
}

/**
 * Take a reading from the DARK sensor pin. If we are still collecting data for the next
 * averaged sample, smoothedValueOut remains unchanged and this returns false. If we have
 * enough data for a confirmed sample, set the output in smoothedValueOut and return true.
 *
 * The output value will be between 0 (very bright) and 1024 (pitch black).
 */
bool readDarkSensorOnce(uint16_t &smoothedValueOut) {
  // Perform analog read of DARK sensor to show value between 0...1024
  // as to how dark it is outside according to the sensor. (This takes ~27us / reading.)
  // We collect a series of (noisy) readings in a row and average them together to get
  // a useful value. While still collecting data, this fn returns false and does not
  // produce a numerical value out.
  uint16_t darkAnalogVal = analogRead(DARK_SENSOR_ANALOG);
  darkAnalogValues[lastAnalogDarkIdx++] = darkAnalogVal;
  if (lastAnalogDarkIdx < AVG_NUM_DARK_SAMPLES) {
    // Didn't get a final averaged sample. Still collecting data.
    return false;
  }

  // We have read enough values, average them.
  uint32_t sum = 0;
  for (uint8_t i = 0; i < AVG_NUM_DARK_SAMPLES; i++) {
    sum += darkAnalogValues[i];
  }
  smoothedValueOut = sum / AVG_NUM_DARK_SAMPLES;
  lastAnalogDarkIdx = 0; // Reset array for next series of readings.
  lastAveragedDarkVal = smoothedValueOut; // Save the averaged value for later recall.

  if constexpr (REPORT_ANALOG_DARK_SENSOR) {
    DBGPRINTU("DARK sensor avg:", lastAveragedDarkVal);
  }

  return true; // Processed an averaged reading to return in smoothedValueOut.
}

/**
 * Poll the DARK sensor pin. If DARK=1 then it's dark out and we can display the magic.
 * If DARK=0 then it's light out and we should be in idle mode.
 * n.b. that if we're in admin mode, the dark sensor should not cause a state transition;
 * we stay in admin mode day or night.
 *
 * Return true if we got a valid reading, false if we're just collecting sample data.
 */
bool pollDarkSensor() {
  // Perform analog read of DARK sensor to show value between 0...1024
  // as to how dark it is outside according to the sensor. (This takes ~27us / reading.)
  // While still collecting data, this fn returns false and does not change the MacroState.
  uint16_t averagedDarkReading = 0;
  bool receivedDarkData = readDarkSensorOnce(averagedDarkReading);
  if (!receivedDarkData) {
    // Didn't get a final averaged sample. Still collecting data.
    return false;
  }

  unsigned int now = millis();
  // New value of isDark is Schmitt-triggered; depending on prior state (debouncedDarkState),
  // we use a different threshold to determine the current isDark state.
  uint8_t isDark;
  if (debouncedDarkState == LIGHT) {
    // We currently believe it is daylight. Use higher darkness threshold to determine if it's dark yet.
    isDark = (averagedDarkReading > calibratedDarkThreshold) ? DARK : LIGHT;
  } else {
    // debouncedDarkState == DARK; we have previously affirmed it's dark out.
    // Use lower darkness threshold to determine if it's now daylight.
    // (We track this independently of macroState because MS_ADMIN state confuses matters.
    // What matters here is this fn's internal dark/light Schmitt trigger state machine.)
    isDark = (averagedDarkReading < calibratedLightThreshold) ? LIGHT : DARK;
  }

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

  if (changeAllowed && isDark && macroState == MacroState::MS_WAITING) {
    // Time to start the show.
    lastDarkStateChangeTime = now;
    setMacroStateRunning();
  } else if (changeAllowed && !isDark && macroState == MacroState::MS_RUNNING) {
    // The sun has found us; pack up for the day.
    lastDarkStateChangeTime = now;
    setMacroStateWaiting();
  }

  return true;
}

void initialDarkSensorRead() {
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

void printDarkThreshold() {
  DBGPRINTI("Dark sensor calibration setting:", fieldConfig.darkSensorCalibration);
  DBGPRINTU("  Going-dark threshold: ", calibratedDarkThreshold);
  DBGPRINTU("  Going-light threshold:", calibratedLightThreshold);
}
