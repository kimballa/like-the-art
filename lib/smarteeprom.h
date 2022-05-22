// (c) Copyright 2022 Aaron Kimball
// SmartEEPROM mgmt library for SAMD51

#ifndef _SMART_EEPROM_H
#define _SMART_EEPROM_H

#ifndef __SAMD51__
#  error This library only works on the ATSAMD51 architecture. (Requires `gcc -D__SAMD51__`)
#endif

#include <Arduino.h>
#include <samd.h>

#ifdef __cplusplus
  #define _SE_EXTERN extern "C"

  constexpr unsigned int EEPROM_SUCCESS = 0;
  constexpr unsigned int EEPROM_INVALID_ARG = 1;
  constexpr unsigned int EEPROM_EMPTY = 2;
  constexpr unsigned int EEPROM_OVERFLOW = 3;
  constexpr unsigned int EEPROM_WRITE_FAILED = 4;
#else
  #define _SE_EXTERN extern

  #define EEPROM_SUCCESS 0
  #define EEPROM_INVALID_ARG 1
  #define EEPROM_EMPTY 2
  #define EEPROM_OVERFLOW 3
  #define EEPROM_WRITE_FAILED 4
#endif // __cplusplus

_SE_EXTERN void programEEPROMFuses(uint8_t sblk, uint8_t psz);
_SE_EXTERN int readEEPROM(unsigned int offset, void *dataOut, size_t size);
_SE_EXTERN int writeEEPROM(unsigned int offset, const void *data, size_t size);

_SE_EXTERN void setEEPROMCommitMode(bool useExplicitCommit);
_SE_EXTERN int commitEEPROM();

// returns true if SmartEEPROM has left uncommitted data in the page buffer.
inline bool isEEPROMDirty() {
  return NVMCTRL->SEESTAT.bit.LOAD;
};

// Returns 0 for auto-commit, 1 for buffered mode (explicit commit() required).
inline bool getEEPROMCommitMode() {
  return NVMCTRL->SEECFG.bit.WMODE;
};

#ifdef __cplusplus
  /** Read an object of type T at 'offset' bytes into the EEPROM region into dataOut. */
  template<class T> int readEEPROM(unsigned int offset, T *dataOut) {
    return readEEPROM(offset, dataOut, sizeof(T));
  }

  /** Write an object of type T at 'offset' bytes in the EEPROM region from data. */
  template<class T> int writeEEPROM(unsigned int offset, const T *data) {
    return writeEEPROM(offset, data, sizeof(T));
  }
#endif // __cplusplus

#endif // _SMART_EEPROM_H
