// (c) Copyright 2022 Aaron Kimball
//
// samd51pwm -- A PWM library for ATSAMD51 devices.
// Tested/designed for the Adafruit Feather M4 -- ATSAMD51 @ 120 MHz.
// Output a series of PWM waveforms via TCCn on the specified pin.

#include "samd51pwm.h"

/** A PWM timer returned by makePwmTimer() if it cannot actually validate. */
static PwmTimer INVALID_PWM_TIMER(0, 0, 0, NULL, 0, 100, 100);

PwmTimer makePwmTimer(uint32_t arduinoPin, uint32_t pwmFreq, int &retval) {
  retval = ERR_SUCCESS;

  const PinDescription &pinDesc = g_APinDescription[arduinoPin];
  uint32_t portGroup = (uint32_t)pinDesc.ulPort;
  uint32_t portPin = pinDesc.ulPin;

  // WARNING: This table in the Feather_m4 variant.cpp seems incorrect for some pins;
  // it seems based on ATSAMD21 components which have fewer channels per TCC and different
  // wrapping semantics between TCC channels and W0..W6 gpio pin driver mappings.
  // e.g. pin D6 is definitely mis-mapped.
  uint32_t tccNum = (pinDesc.ulPWMChannel >> 8) & 0xFF;
  uint32_t tccChan = pinDesc.ulPWMChannel & 0xFF;
  Tcc *tcc = NULL;

  if (tccNum > 2) {
    // Even though TCC3 and TCC4 exist in ATSAMD51, Arduino does not make use of them.
    // You must specify the constructor manually if you want to activate one of these
    // alternate TCCs.
    // This refers to a 'TC', not a TCC.
    retval = ERR_MAKE_PWM_NOT_A_TCC;
    return INVALID_PWM_TIMER;
  } else if (tccNum == 0) {
    tcc = TCC0;
  } else if (tccNum == 1) {
    tcc = TCC1;
  } else if (tccNum == 2) {
    tcc = TCC2;
  }

  uint32_t altSelFn = 0;
  if (pinDesc.ulPinAttribute & PIN_ATTR_PWM_E) {
    altSelFn = 0x4; // function 'E'
  } else if (pinDesc.ulPinAttribute & PIN_ATTR_PWM_F) {
    altSelFn = 0x5; // function 'F'
  } else if  (pinDesc.ulPinAttribute & PIN_ATTR_PWM_G) {
    altSelFn = 0x6; // function 'G'
  }

  return std::move(PwmTimer(portGroup, portPin, altSelFn, tcc, tccChan, pwmFreq, DEFAULT_PWM_CLOCK_HZ));
}

PwmTimer::PwmTimer(uint32_t portGroup, uint32_t portPin, uint32_t portFn, Tcc* const tcc,
    uint32_t pwmChannel, uint32_t pwmFreq, uint32_t pwmClockHz):
    _portGroup(portGroup), _portPin(portPin), _portPinBit((uint32_t)(1 << portPin)),
    _portFn(portFn), _TCC(tcc), _pwmChannel(pwmChannel),
    _pwmFreq(pwmFreq), _pwmClockHz(pwmClockHz),
    _pwmWaveCount((pwmClockHz / pwmFreq) - 1), _dutyCycle(pwmFreq / 2)
    {
}

PwmTimer::~PwmTimer() {
  disable();
}

void PwmTimer::enable() {
  if (!isValid()) {
    return;
  }
  _TCC->CTRLA.bit.ENABLE = 1;
  while (_TCC->SYNCBUSY.bit.ENABLE);       // Wait for synchronization
}

void PwmTimer::disable() {
  if (!isValid()) {
    return;
  }
  _TCC->CTRLA.bit.ENABLE = 0;
  while (_TCC->SYNCBUSY.bit.ENABLE);       // Wait for synchronization
}

int PwmTimer::setDutyCycle(unsigned int dutyCycle) {
  if (!isValid()) {
    return ERR_INVALID_PWM;
  }

  // dutyCycle must vary between [0, pwmFreq].
  // Convert that to dutyCount -- the actual counter value at which we switch over.
  unsigned int dutyCount;
  if (dutyCycle > _pwmFreq) {
    return ERR_DUTY_CYCLE_TOO_LONG;
  } else if (dutyCycle == _pwmFreq) {
    dutyCount = _pwmWaveCount;
  } else {
    dutyCount = (((_pwmWaveCount + 1) * dutyCycle) / _pwmFreq) - 1;
  }

  // Set up the CC (counter compare), channel N register for the selected duty cycle.
  _TCC->CC[_pwmChannel].reg = dutyCount;
  while (_TCC->SYNCBUSY.reg & (1 << (_pwmChannel + 8)));          // Wait for synchronization

  _dutyCycle = dutyCycle;

  return ERR_SUCCESS;
}

int PwmTimer::setupTcc() {
  if (!isValid()) {
    return ERR_INVALID_PWM; // Nothing to do.
  }

  // Set as output.
  PORT->Group[_portGroup].DIRSET.reg |= _portPinBit;

  // Enable the peripheral multiplexer on output pin
  PORT->Group[_portGroup].PINCFG[_portPin].bit.PMUXEN = 1;

  // Set the peripheral multiplexer to the indicated alt-sel function
  if (_portPin % 2) {
    // Odd pin, use PORT_PMUX_PMUXO
    PORT->Group[_portGroup].PMUX[_portPin >> 1].reg |= PORT_PMUX_PMUXO(_portFn);
  } else {
    // Even pin, use PORT_PMUX_PMUXE
    PORT->Group[_portGroup].PMUX[_portPin >> 1].reg |= PORT_PMUX_PMUXE(_portFn);
  }

  disable();

  // enable TCC Bus Clock.
  if (_TCC == TCC0) {
    MCLK->APBBMASK.reg |= MCLK_APBBMASK_TCC0;
  } else if (_TCC == TCC1) {
    MCLK->APBBMASK.reg |= MCLK_APBBMASK_TCC1;
  } else if (_TCC == TCC2) {
    MCLK->APBCMASK.reg |= MCLK_APBCMASK_TCC2;
  } else if (_TCC == TCC3) {
    MCLK->APBCMASK.reg |= MCLK_APBCMASK_TCC3;
  } else if (_TCC == TCC4) {
    MCLK->APBDMASK.reg |= MCLK_APBDMASK_TCC4;
  }

  // Set up the generic clock (GCLK7) to be the clock for the selected TCC.
  GCLK->GENCTRL[7].reg = GCLK_GENCTRL_DIV(1) |       // Divide the 48MHz clock source by divisor 1:
                                                     // 48MHz/1 = 48MHz
                         GCLK_GENCTRL_IDC |          // Set the duty cycle to 50/50 HIGH/LOW
                         GCLK_GENCTRL_GENEN |        // Enable GCLK7
                         GCLK_GENCTRL_SRC(GCLK_GENCTRL_SRC_DFLL);  // Select 48MHz DFLL clock source
                         //GCLK_GENCTRL_SRC_DPLL1;     // Select 100MHz DPLL clock source
                         //GCLK_GENCTRL_SRC_DPLL0;     // Select 120MHz DPLL clock source
  while (GCLK->SYNCBUSY.bit.GENCTRL7);               // Wait for synchronization

  unsigned int periphId = TCC0_GCLK_ID;
  if (_TCC == TCC0) {
    periphId = TCC0_GCLK_ID;
  } else if (_TCC == TCC1) {
    periphId = TCC1_GCLK_ID;
  } else if (_TCC == TCC2) {
    periphId = TCC2_GCLK_ID;
  } else if (_TCC == TCC3) {
    periphId = TCC3_GCLK_ID;
  } else if (_TCC == TCC4) {
    periphId = TCC4_GCLK_ID;
  }
  GCLK->PCHCTRL[periphId].reg =
                          GCLK_PCHCTRL_CHEN |        // Enable the TCCn peripheral channel
                          GCLK_PCHCTRL_GEN_GCLK7;    // Connect generic clock 7 to TCCn

  _TCC->CTRLA.reg = TC_CTRLA_PRESCALER_DIV8 |        // Set prescaler to 8, 48MHz/8 = 6MHz
                    TC_CTRLA_PRESCSYNC_PRESC;        // Set the reset/reload to trigger on prescaler clock

  _TCC->WAVE.reg = TC_WAVE_WAVEGEN_NPWM;   // Set-up TCCn timer for Normal (single slope) PWM mode (NPWM)
  while (_TCC->SYNCBUSY.bit.WAVE);         // Wait for synchronization

  // Set-up the PER (period) register for specified PWM freq and reset the counter.
  _TCC->PER.reg = TCC_PER_PER(_pwmWaveCount);
  _TCC->COUNT.reg = 0;
  while (_TCC->SYNCBUSY.bit.PER);          // Wait for synchronization

  setDutyCycle(_dutyCycle);
  enable();

  return ERR_SUCCESS;
}

