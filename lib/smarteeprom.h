// (c) Copyright 2022 Aaron Kimball
// SmartEEPROM mgmt library for SAMD51

#ifndef _SMART_EEPROM_H
#define _SMART_EEPROM_H

#ifndef __SAMD51__
#  error This library only works on the ATSAMD51 architecture.
#endif

#include <Arduino.h>
#include <samd.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

extern void programEEPROMFuses(uint8_t sblk, uint8_t psz);

constexpr unsigned int EEPROM_SUCCESS = 0;
constexpr unsigned int EEPROM_INVALID_ARG = 1;
constexpr unsigned int EEPROM_EMPTY = 2;
constexpr unsigned int EEPROM_OVERFLOW = 3;
constexpr unsigned int EEPROM_WRITE_FAILED = 4;

extern int readEEPROM(unsigned int offset, void *dataOut, size_t size);
extern int writeEEPROM(unsigned int offset, const void *data, size_t size);

void setEEPROMCommitMode(bool useExplicitCommit);
extern int commitEEPROM();

#ifdef __cplusplus
}
#endif // __cplusplus

// returns true if SmartEEPROM has left uncommitted data in the page buffer.
inline bool isEEPROMDirty() {
  return NVMCTRL->SEESTAT.bit.LOAD;
};

// Returns 0 for auto-commit, 1 for buffered mode (explicit commit() required).
inline bool getEEPROMCommitMode() {
  return NVMCTRL->SEECFG.bit.WMODE;
}

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
