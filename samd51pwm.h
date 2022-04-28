// (c) Copyright 2022 Aaron Kimball
//
// samd51pwm -- A PWM library for ATSAMD51 devices.

#ifndef _SAMD51_PWM_H
#define _SAMD51_PWM_H

#include<utility>

#include<Arduino.h>
#include<samd.h>

constexpr unsigned int TCC_PLL_FREQ = 48000000; // Use 48MHz base clock.
constexpr unsigned int DEFAULT_PWM_PRESCALER = 8;
constexpr unsigned int DEFAULT_PWM_CLOCK_HZ = 6000000; // 48MHz clock div 8 = 6 MHz.

/**
 * Creates a PWM timer
 */
class PwmTimer {
public:
  PwmTimer(uint32_t portGroup, uint32_t portPin, uint32_t portFn, Tcc* tcc,
      uint32_t pwmChannel, uint32_t pwmFreq, uint32_t pwmPrescaler);
  ~PwmTimer();

  void enable();
  void disable();

  int setDutyCycle(unsigned int dutyCycle);
  uint32_t getDutyCycle() const { return _dutyCycle; };

  uint32_t getPwmFreq() const { return _pwmFreq; };
  uint32_t getPwmClockFreq() const { return _pwmClockHz; };
  bool isEnabled() const { return isValid() && _TCC->CTRLA.bit.ENABLE != 0; };
  bool isValid() const { return _TCC != NULL; };

  int setupTcc();

private:

  const uint32_t _portGroup;
  const uint32_t _portPin;
  const uint32_t _portPinBit;
  const uint32_t _portFn;
  const uint32_t _pwmChannel;
  const uint32_t _pwmFreq;
  const uint32_t _pwmPrescaler;
  const uint32_t _pwmClockHz;
  const uint32_t _pwmWaveCount;
  uint32_t _dutyCycle;
  Tcc *const _TCC;
};

PwmTimer makePwmTimer(uint32_t arduinoPin, uint32_t pwmFreq);

constexpr int ERR_SUCCESS = 0; // Return code '0' is success, not an error.
constexpr int ERR_MAKE_PWM_NOT_A_TCC = 1; // This only works with TCC's, not TC's.
constexpr int ERR_DUTY_CYCLE_TOO_LONG = 2; // setDutyCycle() max val of pwmFreq was exceeded.
constexpr int ERR_INVALID_PWM = 3; // Invalid TCC timer object.

#endif /* _SAMD51_PWM_H */

