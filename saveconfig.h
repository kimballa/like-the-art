// (c) Copyright 2022 Aaron Kimball

#ifndef __LTA_SAVE_CONFIG_H
#define __LTA_SAVE_CONFIG_H

constexpr uint32_t PROGRAMMING_SIGNATURE = 0xA1B29C8D;

// Bits can transition 1->0 but not back 0->1 w/o a full pg rewrite.
// Make this a set of flags that transition 1111 -> 0111 -> 0011 .. so we can
// cycle thru w/o unnecessary page wear leveling.
constexpr uint8_t BRIGHTNESS_FULL = 0xF;         // 100%
constexpr uint8_t BRIGHTNESS_NORMAL = 0x7;       //  75%
constexpr uint8_t BRIGHTNESS_POWER_SAVE_1 = 0x3; //  60%
constexpr uint8_t BRIGHTNESS_POWER_SAVE_2 = 0x1; //  50%

constexpr uint8_t DEFAULT_MAX_BRIGHTNESS = BRIGHTNESS_NORMAL;

// Data structure holding the field-programmable configuration.
struct __attribute__((packed, aligned(4))) field_config_t {
  uint32_t validitySignature;
  uint8_t maxBrightness;  // PWM setting
  uint8_t padding[3];
};
typedef struct field_config_t DeviceFieldConfig;


constexpr int FIELD_CONF_EMPTY = 2;

extern int loadFieldConfig(DeviceFieldConfig *configOut);
extern int saveFieldConfig(DeviceFieldConfig *config);
extern int initDefaultFieldConfig();

extern void printCurrentBrightness();

extern DeviceFieldConfig fieldConfig;

#endif
