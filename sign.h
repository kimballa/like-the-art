// (c) Copyright 2022 Aaron Kimball
//
// Definition of a single light-up sign

#ifndef _SIGN_H
#define _SIGN_H

/**
 * A SignChannel turns a single sign on or off.
 */
class SignChannel {
public:
  SignChannel() { };
  virtual ~SignChannel() {};
  virtual void setup() = 0;
  virtual void enable() = 0;
  virtual void disable() = 0;
};

/**
 * I2CSignChannel is a SignChannel that communicates over I2C to a specific
 * pin on an I2C parallel bus expander (PCF8574).
 */
class I2CSignChannel: public SignChannel {
public:
  I2CSignChannel(I2CParallel &bus, unsigned int pin): _bus(bus), _pin(pin), _bit(1 << pin) { };

  virtual void setup();
  virtual void enable();
  virtual void disable();
private:
  I2CParallel &_bus;
  const unsigned int _pin;
  const unsigned int _bit;
};


/**
 * GpioSignChannel is a SignChannel that directly drives an Arduino digital output pin
 */
class GpioSignChannel: public SignChannel {
public:
  GpioSignChannel(unsigned int pin): _pin(pin) { };

  virtual void setup();
  virtual void enable();
  virtual void disable();
private:
  const unsigned int _pin;
};

/**
 * A sign channel that doesn't actually do anything. For debugging, of course.
 */
class NullSignChannel: public SignChannel {
  virtual void setup() {};
  virtual void enable() {};
  virtual void disable() {};
};

constexpr unsigned int FLICKER_RANGE_MAX = 1000;
constexpr unsigned int FLICKER_ALWAYS_ON = 0;
constexpr unsigned int FLICKER_ALWAYS_OFF = FLICKER_RANGE_MAX;

// Sub-range within (ALWAYS_ON, RANGE_MAX] used when assigning flicker potential
// to a sign determined to be flickering for the subsequent animation.
constexpr unsigned int FLICKER_ASSIGN_MIN = 50;
constexpr unsigned int FLICKER_ASSIGN_MAX = 300;

/**
 * A Sign defines a single light-up sign. Signs are enabled or disabled through SignChannels.
 */
class Sign {
public:
  Sign(unsigned int id, const char *const word, SignChannel *sc);
  Sign(Sign &&other) noexcept;
  ~Sign();

  void enable(); // Turn the sign on (unless flickering switches it momentarily off)
  void disable(); // Turn the sign off.

  // Flickering generates a random number 'r' between 0 and FLICKER_RANGE_MAX.
  // If r > threshold, the light is on (assuming its already enabled). Setting the
  // threshold to 0 (FLICKER_ALWAYS_ON) ensures it's always enabled.
  void setFlickerThreshold(unsigned int threshold) { _flickerThreshold = threshold; };
  unsigned int getFlickerThreshold() const { return _flickerThreshold; };
  void flickerFrame();

  // If the sign was commanded to be on via enable.
  bool isEnabled() const { return _enabled; };
  // If the sign is actually supposed to be on (enabled and not flickered off).
  bool isActive() const { return _active; };

  const char *word() const { return _word; };
  unsigned int id() const { return _id; };

private:
  void _activate();
  void _deactivate();

  const unsigned int _id;
  const char *const _word;
  SignChannel *_channel;
  bool _enabled; // Whether this sign is nominally on
  bool _active;  // Whether this sign is truly on (nominally on AND flicker is in 'on' state).
  unsigned int _flickerThreshold;
};

#define I2CSC I2CSignChannel
#define GPIOSC GpioSignChannel
#define NSC NullSignChannel

extern vector<Sign> signs;

extern "C" {
  extern void setupSigns(I2CParallel &bank0, I2CParallel &bank1);
  extern void allSignsOff(); // Turn all signs off
  extern void allSignsOn(); // Turn all signs on
  extern void configMaxPwm(); // Set current PWM level to the configured max brightness.
  extern uint32_t getMaxPwmDutyCycle();
  extern void logSentence(uint32_t sentenceBits);
  extern void logSignStatus(); // Log the current sign status.
}

constexpr unsigned int NUM_SIGNS = 16;
constexpr unsigned int INVALID_SIGN_ID = NUM_SIGNS + 1;

// Bitfield-based one shot ids for each sign.
constexpr unsigned int S_WHY = 1 << 0;
constexpr unsigned int S_DO = 1 << 1;
constexpr unsigned int S_YOU = 1 << 2;
constexpr unsigned int S_I = 1 << 3;
constexpr unsigned int S_DONT = 1 << 4;
constexpr unsigned int S_HAVE = 1 << 5;
constexpr unsigned int S_TO = 1 << 6;
constexpr unsigned int S_LOVE = 1 << 7;
constexpr unsigned int S_LIKE = 1 << 8;
constexpr unsigned int S_HATE = 1 << 9;
constexpr unsigned int S_BM = 1 << 10;
constexpr unsigned int S_ALL = 1 << 11;
constexpr unsigned int S_THE = 1 << 12;
constexpr unsigned int S_ART = 1 << 13;
constexpr unsigned int S_BANG = 1 << 14;
constexpr unsigned int S_QUESTION = 1 << 15;

// Indexes of each word in the `signs` vector.
constexpr unsigned int IDX_WHY      = 0;
constexpr unsigned int IDX_DO       = 1;
constexpr unsigned int IDX_YOU      = 2;
constexpr unsigned int IDX_I        = 3;
constexpr unsigned int IDX_DONT     = 4;
constexpr unsigned int IDX_HAVE     = 5;
constexpr unsigned int IDX_TO       = 6;
constexpr unsigned int IDX_LOVE     = 7;
constexpr unsigned int IDX_LIKE     = 8;
constexpr unsigned int IDX_HATE     = 9;
constexpr unsigned int IDX_BM       = 10;
constexpr unsigned int IDX_ALL      = 11;
constexpr unsigned int IDX_THE      = 12;
constexpr unsigned int IDX_ART      = 13;
constexpr unsigned int IDX_BANG     = 14;
constexpr unsigned int IDX_QUESTION = 15;

#endif /* _SIGN_H */
