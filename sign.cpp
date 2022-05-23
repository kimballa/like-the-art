// (C) Copyright 2022 Aaron Kimball
//
// Sign class and SignChannel class implementations.
// Sign channels pull a pin high to enable a sign, low to turn it off.

#include "like-the-art.h"

void I2CSignChannel::setup() {
  disable();
}

void I2CSignChannel::enable() {
  _bus.setOr(_bit); // Set our pin bit high.
}

void I2CSignChannel::disable() {
  _bus.setAnd(~_bit); // Set our pin bit low.
}


void GpioSignChannel::setup() {
  pinMode(_pin, OUTPUT);
  disable();
}

void GpioSignChannel::enable() {
  digitalWrite(_pin, 1);
}

void GpioSignChannel::disable() {
  digitalWrite(_pin, 0);
}


Sign::Sign(unsigned int id, const char *const word, SignChannel *channel):
    _id(id), _word(std::move(word)), _channel(channel) {

  channel->setup();
}

// Move constructor.
Sign::Sign(Sign &&other):
    _id(other._id), _word(other._word), _channel(other._channel) {
  other._channel = NULL; // So its d'tor doesn't deallocate it.
}

void Sign::enable() {
  DBGPRINTU("Enable sign:", _id);
  DBGPRINT(_word);
  this->_channel->enable();
}

void Sign::disable() {
  DBGPRINTU("Disable sign:", _id);
  DBGPRINT(_word);
  this->_channel->disable();
}

Sign::~Sign() {
  if (NULL != _channel) {
    delete _channel;
  }
}

vector<Sign> signs;

const char *const W_WHY = "WHY";
const char *const W_DO = "DO";
const char *const W_YOU = "YOU";
const char *const W_DONT = "DON'T";
const char *const W_HAVE = "HAVE";
const char *const W_TO = "TO";
const char *const W_I = "I";
const char *const W_LIKE = "LIKE";
const char *const W_LOVE = "LOVE";
const char *const W_HATE = "HATE";
const char *const W_BM = ")'(";
const char *const W_ALL = "ALL";
const char *const W_THE = "THE";
const char *const W_ART = "ART";
const char *const W_BANG = "!";
const char *const W_QUESTION = "?";

void setupSigns(I2CParallel &bank0, I2CParallel &bank1) {
  // Define Signs with bindings to I/O channels.
  signs.clear();
  signs.reserve(NUM_SIGNS);
  // TODO(aaron): Bind signs to actual production channels.
  signs.emplace_back(0, W_WHY, new I2CSC(bank0, 0));
  signs.emplace_back(1, W_DO, new NSC());
  signs.emplace_back(2, W_YOU, new NSC());
  signs.emplace_back(3, W_DONT, new NSC());
  signs.emplace_back(4, W_HAVE, new NSC());
  signs.emplace_back(5, W_TO, new NSC());
  signs.emplace_back(6, W_I, new NSC());
  signs.emplace_back(7, W_LIKE, new NSC());
  signs.emplace_back(8, W_LOVE, new NSC());
  signs.emplace_back(9, W_HATE, new NSC());
  signs.emplace_back(10, W_BM, new NSC());
  signs.emplace_back(11, W_ALL, new NSC());
  signs.emplace_back(12, W_THE, new NSC());
  signs.emplace_back(13, W_ART, new NSC());
  signs.emplace_back(14, W_BANG, new NSC());
  signs.emplace_back(15, W_QUESTION, new NSC());
}

void allSignsOff() {
  for (auto &sign : signs) {
    sign.disable();
  }
}

// Set the PWM level to the current configured maximum brightness
void configMaxPwm() {
  uint32_t freq = pwmTimer.getPwmFreq();

  switch (fieldConfig.maxBrightness) {
  case BRIGHTNESS_FULL: // 100%
    pwmTimer.setDutyCycle(freq);
    break;
  case BRIGHTNESS_NORMAL: // 70%
    pwmTimer.setDutyCycle((freq * 70) / 100);
    break;
  case BRIGHTNESS_POWER_SAVE_1: // 60%
    pwmTimer.setDutyCycle((freq * 60) / 100);
    break;
  case BRIGHTNESS_POWER_SAVE_2: // 50%
    pwmTimer.setDutyCycle(freq / 2);
    break;
  default: // Unknown PWM frequency required... Just use 'normal' (70%)
    pwmTimer.setDutyCycle((freq * 70) / 100);
    break;
  }
}
