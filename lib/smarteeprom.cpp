// (c) Copyright 2022 Aaron Kimball
//
// Configure NVM controller to allocate a SmartEEPROM region for non-volatile data storage.
// See SAM D5x datasheet section 25.6.8 for SmartEEPROM description.
// Datasheet section 9.4 describes the user row programming.

#include <dbg.h>
#include "smarteeprom.h"

// Pointer to the start of the SmartEEPROM area allocated.
volatile uint32_t *const ptrEEPROM = (volatile uint32_t *)SEEPROM_ADDR;

// 8x32 bits stored in "user page" fuse bits.
constexpr unsigned int nFuseUserPageWords = 8;
// The user page area is actually 512 bytes; the first 32 bytes contain system
// settings that must be read in and written back with any rewrite of the user page.
// The other 480 pages are for our own purposes; since we don't care to store anything
// there, we just ignore it.

// The NVM page buffer holds 4x32-bit words and must be flushed every 4 writes of 32bits each.
constexpr unsigned int pageBufferWidth = 4;

// Reset the MCU; required after changing fuse bits.
static void doReboot() __attribute__((noreturn));
static void doReboot() {
  NVIC_SystemReset();
}

// Wait for NVM controller to be ready for command.
static void waitNVMReady() {
  while (!(NVMCTRL->STATUS.reg & NVMCTRL_STATUS_READY));
}

constexpr unsigned int EEPROM_USER_ROW_WORD_IDX = 1;

constexpr unsigned int SBLK_MASK = 0xF;
constexpr unsigned int SBLK_BIT_POS = 0x0;

constexpr unsigned int PSZ_MASK = 0x70;
constexpr unsigned int PSZ_BIT_POS = 0x4;

/******************************************************************************
 * Set the SBLK and PSZ fuses as required to configure a target EEPROM size.
 *
 * SBLK and PSZ settings for smartEEPROM size per data sheet section 25.6.9
 *
 *      EEPROM Size     SEESBLK     SEEPSZ
 *      512             1           0
 *      1024            1           1
 *      2048            1           2
 *      4096            1           3
 *      8192            2           4
 *      16384           3           5
 *      32968           5           6
 *      65536           10          7
 ******************************************************************************/
void programEEPROMFuses(uint8_t sblk, uint8_t psz) {
  uint32_t fuseWords[nFuseUserPageWords];

  // Read the current fuse values
  DBGPRINT("Reading current user page fuse values...");
  waitNVMReady();
  uint32_t *nvmCursor = (uint32_t*)NVMCTRL_USER;
  for (unsigned int i = 0; i < nFuseUserPageWords; i++) {
    fuseWords[i] = *nvmCursor++;
  }

  // Update our copy to have the fuses we want.
  // Set SBLK and PSZ per the table above to achieve desired SmartEEPROM area size.
  uint32_t updatedFuseWord = fuseWords[EEPROM_USER_ROW_WORD_IDX];
  updatedFuseWord &= ~(SBLK_MASK | PSZ_MASK);
  updatedFuseWord |= (sblk << SBLK_BIT_POS) & SBLK_MASK;
  updatedFuseWord |= (psz << PSZ_BIT_POS) & PSZ_MASK;

  DBGPRINTX("Existing EEPROM config word in user page:", fuseWords[EEPROM_USER_ROW_WORD_IDX]);
  DBGPRINTX("Updated  EEPROM config word in user page:", updatedFuseWord);

  if (fuseWords[EEPROM_USER_ROW_WORD_IDX] == updatedFuseWord) {
    DBGPRINT("NVM does not need update; fuses already configured for SmartEEPROM.");
    return;
  }

  DBGPRINTX("Last reboot cause:", RSTC->RCAUSE.reg);
  if ((RSTC->RCAUSE.reg & RSTC_RCAUSE_SYST) != 0) {
    DBGPRINT("Last reboot was via system reset but fuse bits unprogrammed");
    DBGPRINT("*** EEPROM / NVM CONFIG PROGRAMMING FAILED ***");
    return;
  }

  DBGPRINT("Writing fuse data...");
  fuseWords[EEPROM_USER_ROW_WORD_IDX] = updatedFuseWord; // Put our changes back in the queue.

  // Erase the user page
  NVMCTRL->ADDR.reg = NVMCTRL_USER;
  NVMCTRL->CTRLB.reg = NVMCTRL_CTRLB_CMDEX_KEY | NVMCTRL_CTRLB_CMD_EP; // Erase Page (EP) cmd.
  waitNVMReady();

  // Erase page buffer
  NVMCTRL->ADDR.reg = NVMCTRL_USER;
  NVMCTRL->CTRLB.reg = NVMCTRL_CTRLB_CMDEX_KEY | NVMCTRL_CTRLB_CMD_PBC; // Page Buffer Clear (PBC) cmd.
  waitNVMReady();

  // Write the new data for the page.
  NVMCTRL->ADDR.reg = NVMCTRL_USER;
  nvmCursor = (uint32_t*)NVMCTRL_USER;
  for (unsigned int i = 0; i < nFuseUserPageWords; i++) {
    *nvmCursor++ = fuseWords[i];
    if ((i % pageBufferWidth) == (pageBufferWidth - 1)) {
      // Flush page buffer to fuse area.
      NVMCTRL->CTRLB.reg = NVMCTRL_CTRLB_CMDEX_KEY | NVMCTRL_CTRLB_CMD_WQW; // Write Quad Word (WQW)
      waitNVMReady();
    }
  }

  DBGPRINT("Data write complete. Rebooting...");
  doReboot();
}

/**
 * Read 'size' bytes of data at 'offset' bytes into the EEPROM region and copy it into dataOut.
 * dataOut must not be null.
 * size must be non-zero and 32-bit aligned.
 * offset must be 32-bit aligned.
 *
 * Returns 0 on success, non-zero error code otherwise.
 */
int readEEPROM(unsigned int offset, void *dataOut, size_t size) {
  if (NULL == dataOut) {
    return EEPROM_INVALID_ARG;
  }

  if (size == 0 || (size % 4) != 0) {
    return EEPROM_INVALID_ARG; // Require 32-bit aligned reads.
  }

  if ((offset % 4) != 0) {
    return EEPROM_INVALID_ARG; // Alignment.
  }

  // Wait for hardware to be ready...
  while (NVMCTRL->SEESTAT.bit.BUSY);

  const volatile uint32_t *readCursor = ptrEEPROM;
  uint32_t *writeCursor = (uint32_t *)dataOut;
  size_t copied = 0;
  while (copied < size) {
    *writeCursor++ = *readCursor++;
    copied += sizeof(uint32_t);
  }

  return EEPROM_SUCCESS;
}

/**
 * Write 'size' bytes of data at 'offset' bytes in the EEPROM region, copied from data.
 * data must not be null.
 * size must be non-zero and 32-bit aligned.
 * offset must be 32-bit aligned.
 *
 * Returns 0 on success, non-zero error code otherwise.
 */
int writeEEPROM(unsigned int offset, const void *data, size_t size) {
  if (NULL == data) {
    return EEPROM_INVALID_ARG;
  }

  if (size == 0 || (size % 4) != 0) {
    return EEPROM_INVALID_ARG; // Require 32-bit aligned writes.
  }

  if ((offset % 4) != 0) {
    return EEPROM_INVALID_ARG; // Alignment.
  }

  // Wait for hardware to be ready...
  while (NVMCTRL->SEESTAT.bit.BUSY);

  // Copy data to EEPROM.
  volatile uint32_t *writeCursor = ptrEEPROM;
  const uint32_t *readCursor = (const uint32_t *)data;
  size_t copied = 0;
  while (copied < size) {
    *writeCursor++ = *readCursor++;
    copied += sizeof(uint32_t);
  }

  return EEPROM_SUCCESS;
}

/**
 * If useExplicitCommit is true, you must explicitly call commitEEPROM() after making writes.
 * Otherwise it auto-commits.
 */
void setEEPROMCommitMode(bool useExplicitCommit) {
  DBGPRINTI("Setting EEPROM commit mode; useExplicitCommit =", useExplicitCommit);
  NVMCTRL->SEECFG.bit.WMODE = useExplicitCommit ? 1 : 0;
}

int commitEEPROM() {
  DBGPRINT("Committing EEPROM...");

  if (!NVMCTRL->SEESTAT.bit.LOAD) {
    DBGPRINT("(Nothing to commit)");
    return EEPROM_SUCCESS;
  }

  // Wait for hardware to be ready...
  while (NVMCTRL->SEESTAT.bit.BUSY);
  waitNVMReady();

  // Issue SEE flush command.
  NVMCTRL->INTFLAG.bit.DONE = 1; // clear flag.
  NVMCTRL->CTRLB.reg = NVMCTRL_CTRLB_CMDEX_KEY | NVMCTRL_CTRLB_CMD_SEEFLUSH;
  while (!NVMCTRL->INTFLAG.bit.DONE);
  while (!NVMCTRL->INTFLAG.bit.SEEWRC);

  if (NVMCTRL->INTFLAG.bit.SEESOVF) {
    // We overflowed the smart eeprom area.
    NVMCTRL->INTFLAG.bit.SEESOVF = 1; // clear the flag.
    return EEPROM_OVERFLOW;
  }

  if (NVMCTRL->SEESTAT.bit.LOAD == 1) {
    return EEPROM_WRITE_FAILED; // Didn't fully commit the data.
  }

  return EEPROM_SUCCESS;
}

