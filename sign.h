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

/**
 * A Sign defines a single light-up sign. Signs are enabled or disabled through SignChannels.
 */
class Sign {
public:
  Sign(unsigned int id, const char *const word, SignChannel *sc);
  Sign(Sign &&other) noexcept;
  ~Sign();

  void enable();
  void disable();
  bool isActive() const { return _active; };
  const char *word() const { return _word; };

private:
  const unsigned int _id;
  const char *const _word;
  SignChannel *_channel;
  bool _active;
};

#define I2CSC I2CSignChannel
#define GPIOSC GpioSignChannel
#define NSC NullSignChannel

extern vector<Sign> signs;

extern "C" {
  extern void setupSigns(I2CParallel &bank0, I2CParallel &bank1);
  extern void allSignsOff(); // Turn all signs off
  extern void configMaxPwm(); // Set current PWM level to the configured max brightness.
  extern uint32_t getMaxPwmDutyCycle();
  extern void logSignStatus(); // Log the current sign status.
}

constexpr unsigned int NUM_SIGNS = 16;

// Bitfield-based one shot ids for each sign.
constexpr unsigned int S_WHY = 1 << 0;
constexpr unsigned int S_DO = 1 << 1;
constexpr unsigned int S_YOU = 1 << 2;
constexpr unsigned int S_DONT = 1 << 3;
constexpr unsigned int S_HAVE = 1 << 4;
constexpr unsigned int S_TO = 1 << 5;
constexpr unsigned int S_I = 1 << 6;
constexpr unsigned int S_LIKE = 1 << 7;
constexpr unsigned int S_LOVE = 1 << 8;
constexpr unsigned int S_HATE = 1 << 9;
constexpr unsigned int S_BM = 1 << 10;
constexpr unsigned int S_ALL = 1 << 11;
constexpr unsigned int S_THE = 1 << 12;
constexpr unsigned int S_ART = 1 << 13;
constexpr unsigned int S_BANG = 1 << 14;
constexpr unsigned int S_QUESTION = 1 << 15;

#endif /* _SIGN_H */
