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
typedef struct _Sign {
  unsigned int id;
  string word;
  SignChannel *channel;

  void setup(unsigned int id, const string &word, SignChannel *channel);
  void enable();
  void disable();

  _Sign(SignChannel *sc=NULL): id(0), channel(sc) { word = "???"; };
  ~_Sign();
} Sign;

#define I2CSC I2CSignChannel
#define GPIOSC GpioSignChannel
#define NSC NullSignChannel

extern vector<Sign*> signs;
extern "C" void setupSigns(I2CParallel &bank0, I2CParallel &bank1);

extern Sign S_WHY;
extern Sign S_DO;
extern Sign S_YOU;
extern Sign S_DONT;
extern Sign S_HAVE;
extern Sign S_TO;
extern Sign S_I;
extern Sign S_LIKE;
extern Sign S_LOVE;
extern Sign S_HATE;
extern Sign S_BM;
extern Sign S_ALL;
extern Sign S_THE;
extern Sign S_ART;
extern Sign S_BANG;
extern Sign S_QUESTION;

#endif /* _SIGN_H */
