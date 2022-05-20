// (c) Copyright 2022 Aaron Kimball
//
// SmartEEPROM storage for field-programmable device configuration.
// Relies on the smarteeprom.cpp/.h mini-lib for actual NVM interaction.
//
// This module stores a single data structure at offset 0 in the smart eeprom.

#include "like-the-art.h"

// Our data structure lives at offset 0 within the EEPROM region.
static constexpr unsigned int DATA_EEPROM_OFFSET = 0;

DeviceFieldConfig fieldConfig;

/**
 * Load the configuration from EEPROM and save it in configOut.
 *
 * Returns 0 if ok, otherwise an error code.
 */
int loadFieldConfig(DeviceFieldConfig *configOut) {
  DBGPRINT("Reading field configuration...");

  int ret = readEEPROM(DATA_EEPROM_OFFSET, configOut);
  if (ret != 0) {
    return ret;
  }

  if (configOut->validitySignature != PROGRAMMING_SIGNATURE) {
    // What we read back doesn't have the magic signature in it; not a real config.
    DBGPRINT("EEPROM data signature mismatch");
    DBGPRINTX("Expected:", PROGRAMMING_SIGNATURE);
    DBGPRINTX("Received:", configOut->validitySignature);
    return FIELD_CONF_EMPTY;
  }

  return EEPROM_SUCCESS;
}

/**
 * Save the argument configuration to EEPROM. Modifies the config struct to
 * ensure validitySignature is set correctly.
 *
 * Returns 0 if ok, otherwise a non-zero error code.
 */
int saveFieldConfig(DeviceFieldConfig *config) {
  DBGPRINT("Writing field configuration...");
  config->validitySignature = PROGRAMMING_SIGNATURE;
  return writeEEPROM(DATA_EEPROM_OFFSET, config);
}

/**
 * Initialize our global fieldConfig object with system defaults and persist it to EEPROM.
 */
int initDefaultFieldConfig() {
  DBGPRINT("Setting up default field configuration...");
  fieldConfig.validitySignature = PROGRAMMING_SIGNATURE;
  fieldConfig.maxBrightness = DEFAULT_MAX_BRIGHTNESS;
  return saveFieldConfig(&fieldConfig);
}

