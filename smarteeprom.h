// (c) Copyright 2022 Aaron Kimball
// SmartEEPROM mgmt library for SAMD51

#ifndef _SMART_EEPROM_H
#define _SMART_EEPROM_H

#ifndef __SAMD51__
#  error This library only works on the ATSAMD51 architecture.
#endif

extern void programFuses(uint8_t sblk, uint8_t psz);


#endif // _SMART_EEPROM_H
