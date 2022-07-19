// (C) Copyright 2022 Aaron Kimball
//
// Sign class and SignChannel class implementations.
// Sign channels pull a pin high to enable a sign, low to turn it off.

#include "like-the-art.h"

static uint32_t activeSignBits = 0; // bit array tracking active signs.
static uint32_t loggedActiveSignBits = 0; // bit array tracking active signs @ last time logged.

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
    _id(id), _word(std::move(word)), _channel(channel), _active(false) {

  channel->setup();
}

// Move constructor.
Sign::Sign(Sign &&other):
    _id(other._id), _word(other._word), _channel(other._channel) {
  other._channel = NULL; // So its d'tor doesn't deallocate it.
}

void Sign::enable() {
  //DBGPRINTU("Enable sign:", _id);
  //DBGPRINT(_word);
  this->_channel->enable();
  this->_active = true;
  activeSignBits |= 1 << _id;
}

void Sign::disable() {
  //DBGPRINTU("Disable sign:", _id);
  //DBGPRINT(_word);
  if (NULL != this->_channel) {
    this->_channel->disable();
  }
  this->_active = false;
  activeSignBits &= ~(1 << _id);
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
const char *const W_I = "I";
const char *const W_DONT = "DON'T";
const char *const W_HAVE = "HAVE";
const char *const W_TO = "TO";
const char *const W_LOVE = "LOVE";
const char *const W_LIKE = "LIKE";
const char *const W_HATE = "HATE";
const char *const W_BM = ")'(";
const char *const W_ALL = "ALL";
const char *const W_THE = "THE";
const char *const W_ART = "ART";
const char *const W_BANG = "!";
const char *const W_QUESTION = "?";

static constexpr unsigned int SENTENCE_LEN = 64; // At most 62 chars + \0 in the sentence.
static char activeSentence[SENTENCE_LEN];

void setupSigns(I2CParallel &bank0, I2CParallel &bank1) {
  // Define Signs with bindings to I/O channels.
  signs.clear();
  signs.reserve(NUM_SIGNS);

  if constexpr (IS_TARGET_PRODUCTION) {
    // Setup the signs array to have channels reflecting the actual
    // production connections through the PCBs to all 16 LED neon signs.
    DBGPRINT("Initializing PRODUCTION sign channel bindings (x16).");

    // Note that the production channels are NOT wired in order in I2C; see schematic.
    signs.emplace_back(0, W_WHY,  new I2CSC(bank0, 2));
    signs.emplace_back(1, W_DO,   new I2CSC(bank0, 0));
    signs.emplace_back(2, W_YOU,  new I2CSC(bank0, 1));
    signs.emplace_back(3, W_I,    new I2CSC(bank0, 3));
    signs.emplace_back(4, W_DONT, new I2CSC(bank0, 4));
    signs.emplace_back(5, W_HAVE, new I2CSC(bank0, 6));
    signs.emplace_back(6, W_TO,   new I2CSC(bank0, 5));
    signs.emplace_back(7, W_LOVE, new I2CSC(bank0, 7));

    signs.emplace_back(8, W_LIKE,       new I2CSC(bank1, 2));
    signs.emplace_back(9, W_HATE,       new I2CSC(bank1, 0));
    signs.emplace_back(10, W_BM,        new I2CSC(bank1, 1));
    signs.emplace_back(11, W_ALL,       new I2CSC(bank1, 3));
    signs.emplace_back(12, W_THE,       new I2CSC(bank1, 4));
    signs.emplace_back(13, W_ART,       new I2CSC(bank1, 6));
    signs.emplace_back(14, W_BANG,      new I2CSC(bank1, 5));
    signs.emplace_back(15, W_QUESTION,  new I2CSC(bank1, 7));
  } else {
    // !(IS_TARGET_PRODUCTION) case; breadboard-mode.
    // Setup the signs array to have channels reflecting the
    // connections available on the breadboard model (4 standard THT LEDs).
    DBGPRINT("Initializing BREADBOARD sign channel bindings (x4).");

    signs.emplace_back(0, W_WHY,  new I2CSC(bank0, 0));
    signs.emplace_back(1, W_DO,   new I2CSC(bank0, 1));
    signs.emplace_back(2, W_YOU,  new I2CSC(bank0, 2));
    signs.emplace_back(3, W_I,    new I2CSC(bank0, 3));

    signs.emplace_back(4, W_DONT,       new NSC());
    signs.emplace_back(5, W_HAVE,       new NSC());
    signs.emplace_back(6, W_TO,         new NSC());
    signs.emplace_back(7, W_LOVE,       new NSC());
    signs.emplace_back(8, W_LIKE,       new NSC());
    signs.emplace_back(9, W_HATE,       new NSC());
    signs.emplace_back(10, W_BM,        new NSC());
    signs.emplace_back(11, W_ALL,       new NSC());
    signs.emplace_back(12, W_THE,       new NSC());
    signs.emplace_back(13, W_ART,       new NSC());
    signs.emplace_back(14, W_BANG,      new NSC());
    signs.emplace_back(15, W_QUESTION,  new NSC());
  }
}

void allSignsOff() {
  for (auto &sign : signs) {
    sign.disable();
  }
}

void allSignsOn() {
  for (auto &sign : signs) {
    sign.enable();
  }
}

/** Print a log msg w/ the signs that would be active in the specified sentence. */
void logSentence(uint32_t sentenceBits) {
  loggedActiveSignBits = sentenceBits;
  memset(activeSentence, 0, SENTENCE_LEN);
  for (const auto &sign : signs) {
    if (sentenceBits & (1 << sign.id())) {
      strcat(activeSentence, sign.word());
    } else {
      uint8_t len = strlen(sign.word());
      for (uint8_t i = 0; i < len; i++) {
        strcat(activeSentence, "-");
      }
    }
    strcat(activeSentence, " ");
  }

  DBGPRINT(activeSentence);
}

/** Print a log msg w/ the current active signs. */
void logSignStatus() {
  if (activeSignBits == loggedActiveSignBits) {
    // State hasn't changed since last loop. Don't log.
    return;
  }

  // There's been a change in sign lighting. Log the current sentence.
  logSentence(activeSignBits);
}

// Set the PWM level to the current configured maximum brightness
void configMaxPwm() {
  pwmTimer.setDutyCycle(getMaxPwmDutyCycle());
}

// Return the configured max-brightness PWM duty cycle
uint32_t getMaxPwmDutyCycle() {
  uint32_t freq = pwmTimer.getPwmFreq();

  switch (fieldConfig.maxBrightness) {
  case BRIGHTNESS_FULL: // 100%
    return freq;
  case BRIGHTNESS_NORMAL: // 70%
    return (freq * 70) / 100;
  case BRIGHTNESS_POWER_SAVE_1: // 60%
    return (freq * 60) / 100;
  case BRIGHTNESS_POWER_SAVE_2: // 50%
    return freq / 2;
  default: // Unknown PWM frequency required... Just use 'normal' (70%)
    DBGPRINT("Warning: invalid fieldConfig.maxBrightness; using normal/70%");
    return (freq * 70) / 100;
  }

}

