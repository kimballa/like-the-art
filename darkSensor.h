// (c) Copyright 2022 Aaron Kimball

#ifndef _DARK_SENSOR_H
#define _DARK_SENSOR_H

// DARK sensor is on A4 / D18.
constexpr uint8_t DARK_SENSOR_PIN = 18;
constexpr uint8_t DARK_SENSOR_ANALOG = A4;

// Adjust the sensor thresholds up or down in units of 20/1024.
void adjustDarkSensorCalibration(int8_t offset);

// Perform initial setup. Requires that calibration config be loaded from EEPROM.
void setupDarkSensor();

/**
 * Take one reading from the DARK sensor pin. If we are still collecting data for the next
 * averaged sample, smoothedValueOut remains unchanged and this returns false. If we have
 * enough data for a confirmed sample, set the output in smoothedValueOut and return true.
 *
 * The output value will be between 0 (very bright) and 1024 (pitch black).
 */
bool readDarkSensorOnce(uint16_t &smoothedValueOut);

/**
 * Poll the DARK sensor pin. If DARK=1 then it's dark out and we can display the magic.
 * If DARK=0 then it's light out and we should be in idle mode.
 *
 * Adjusts MacroState if appropriate to do so.
 *
 * n.b. that if we're in admin mode, the dark sensor does not cause a state transition;
 * we stay in admin mode day or night.
 *
 * Return true if we got a valid reading, false if we're just collecting sample data.
 */
bool pollDarkSensor();

/** Return the most recent averaged DARK sensor reading. */
uint16_t getLastDarkSensorValue();

uint16_t getDarkThreshold(); // Return calibrated rising-edge threshold.
uint16_t getLightThreshold(); // Return calibrated falling-edge threshold.

/**
 * Take the initial readings to establish whether it's light or dark outside on boot-up.
 * Set the macroState accordingly.
 */
void initialDarkSensorRead();

/** Print DARK sensor calibration / threshold. */
void printDarkThreshold();

#endif // _DARK_SENSOR_H
