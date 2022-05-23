// (c) Copyright 2022 Aaron Kimball

#include "like-the-art.h"

// Delays (milliseconds) for certain button inputs.
constexpr unsigned int EXIT_ADMIN_DEBOUNCE = 1000;
constexpr unsigned int REBOOT_DEBOUNCE = 3000;

// State machine describing where we are within the admin options.
AdminState adminState = AS_MAIN_MENU;

// Forward declarations for remainder of compilation unit...
static void attachAdminButtonHandlers();
static void btnPrevSign(uint8_t btnId, uint8_t btnState);
static void btnNextSign(uint8_t btnId, uint8_t btnState);
static void btnPrevEffect(uint8_t btnId, uint8_t btnState);
static void btnNextEffect(uint8_t btnId, uint8_t btnState);
static void btnPrevSentence(uint8_t btnId, uint8_t btnState);
static void btnNextSentence(uint8_t btnId, uint8_t btnState);
static void btnBrightness0(uint8_t btnId, uint8_t btnState);
static void btnBrightness1(uint8_t btnId, uint8_t btnState);
static void btnBrightness2(uint8_t btnId, uint8_t btnState);
static void btnBrightness3(uint8_t btnId, uint8_t btnState);
static void btnGoToMainMenu(uint8_t btnId, uint8_t btnState);

////////////// State tracked by various AdminStates ////////////////

// if blinking something N times, how many remain?
// (modes: exit, reboot)
int countDownBlinks = 0;

// What effect to use for current illumination?
// (mode: TEST_ONE_SIGN, TEST_EACH_EFFECT, TEST_SENTENCE)
int currentEffect = 0;

// What sign is lit? (mode: TEST_ONE_SIGN)
int currentSign = 0;

// What sentence is lit? (mode: TEST_SENTENCE)
int currentSentence = 0;

// Have we changed persistent state that we need to commit?
bool isConfigDirty = false;

////////////// Button functions for main menu ////////////////

/** "return to main menu" function. */
static void initMainMenu() {
  adminState = AS_MAIN_MENU;
  attachAdminButtonHandlers();
  allSignsOff();
  configMaxPwm();
}

/**
 * Button 1: Full sign in-order test. Auto Light up each sign in series.
 * Repeats indefinitely until a new operation is chosen.
 * Main menu remains active.
 */
static void btnInOrderTest(uint8_t btnId, uint8_t btnState) {
  if (btnState == BTN_OPEN) { return; }
  adminState = AS_IN_ORDER_TEST;
  allSignsOff();
  configMaxPwm();
  DBGPRINT("Performing in-order sign test");
}

/**
 * Button 2: Switch to mode to light up signs one-by-one; don't auto-progress.
 * Use buttons 4 and 6 to go back/forward. Use button 9 to return to the top level menu.
 */
static void btnModeTestOneSign(uint8_t btnId, uint8_t btnState) {
  if (btnState == BTN_OPEN) { return; }
  adminState = AS_TEST_ONE_SIGN;

  allSignsOff();
  attachEmptyButtonHandlers();
  buttons[3].setHandler(btnPrevSign);
  buttons[5].setHandler(btnNextSign);
  buttons[8].setHandler(btnGoToMainMenu);
  configMaxPwm();
  signs[currentSign].enable();
  DBGPRINT("Testing one sign at a time");
  DBGPRINTU("Active sign:", currentSign);
}

/**
 * Button 3 - Change active effect.
 * A sample message is lit while you choose the effect. Use 4 and 6 to
 * scroll back/forward. 9 returns to the top menu.
 */
static void btnModeChangeEffect(uint8_t btnId, uint8_t btnState) {
  if (btnState == BTN_OPEN) { return; }
  adminState = AS_TEST_EACH_EFFECT;

  allSignsOff();
  attachEmptyButtonHandlers();
  buttons[3].setHandler(btnPrevEffect);
  buttons[5].setHandler(btnNextEffect);
  buttons[8].setHandler(btnGoToMainMenu);
  DBGPRINT("Testing one effect at a time");
  DBGPRINTU("Active effect:", currentEffect);
  DBGPRINTU("Active sentence:", currentSentence);
}

/**
 * Button 4: Light up sentences one-by-one; don't auto-progress. 4/6/9 as before.
 */
static void btnModeTestEachSentence(uint8_t btnId, uint8_t btnState) {
  if (btnState == BTN_OPEN) { return; }
  adminState = AS_TEST_SENTENCE;

  allSignsOff();
  attachEmptyButtonHandlers();
  buttons[3].setHandler(btnPrevSentence);
  buttons[5].setHandler(btnNextSentence);
  buttons[8].setHandler(btnGoToMainMenu);
  DBGPRINT("Testing one sentence at a time");
  DBGPRINTU("Active effect:", currentEffect);
  DBGPRINTU("Active sentence:", currentSentence);
}

/**
 * Button 5: Choose the brightness level.
 * Use buttons 1--4 where 1 is super-low, 2 is low, 3 is standard, and 4 is high-intensity
 * brightness (50, 60, 75, 100% full power respectively).
 *
 * One to four signs will illuminate and blink quickly, indicating the chosen brightness level.
 * This value is persistent across reboots.
 * Press 9 to return to top menu.
 */
static void btnModeChooseBrightnessLevel(uint8_t btnId, uint8_t btnState) {
  if (btnState == BTN_OPEN) { return; }
  adminState = AS_CONFIG_BRIGHTNESS;

  allSignsOff();
  attachEmptyButtonHandlers();
  buttons[0].setHandler(btnBrightness0);
  buttons[1].setHandler(btnBrightness1);
  buttons[2].setHandler(btnBrightness2);
  buttons[3].setHandler(btnBrightness3);

  // TODO(aaron): Turn on 1--4 signs at this pwm.
  configMaxPwm();
  DBGPRINT("Configuring brightness level");
  printCurrentBrightness();
}

/**
 * Button 6: Light up all 16 signs at once at the chosen brightness level.
 * Buttons remain at main menu.
 */
static void btnLightEntireBoard(uint8_t btnId, uint8_t btnState) {
  if (btnState == BTN_OPEN) { return; }
  adminState = AS_ALL_SIGNS_ON;
  configMaxPwm();
  for (auto &sign: signs) {
    sign.enable();
  }
  DBGPRINT("Turning on all signs");
}

/**
 * Button 7: Hold 1 second to exit admin mode.
 * The first 3 signs flash 3 times and then admin mode ends.
 * Trigger on button release.
 */
static void btnExitAdminMode(uint8_t btnId, uint8_t btnState) {
  if (btnState == BTN_PRESSED) { return; }
  adminState = AS_EXITING;

  allSignsOff();
  attachEmptyButtonHandlers();
  if (isConfigDirty) {
    saveFieldConfig(&fieldConfig);
  }
  configMaxPwm();
  countDownBlinks = 3;
  DBGPRINT("Exiting admin menu...");
}

/**
 * Button 9 - Hold 3 seconds to completely reset system.
 * The first 3 signs flash 5 times and then the system is rebooted.
 * Trigger on button release.
 */
static void btnCtrlAltDelete(uint8_t btnId, uint8_t btnState) {
  if (btnState == BTN_PRESSED) { return; }
  adminState = AS_REBOOTING;

  allSignsOff();
  attachEmptyButtonHandlers();
  if (isConfigDirty) {
    saveFieldConfig(&fieldConfig);
  }
  configMaxPwm();
  countDownBlinks = 5;
  DBGPRINT("User requested reboot");
}

static void attachAdminButtonHandlers() {
  buttons[0].setHandler(btnInOrderTest);
  buttons[1].setHandler(btnModeTestOneSign);
  buttons[2].setHandler(btnModeChangeEffect);
  buttons[3].setHandler(btnModeTestEachSentence);
  buttons[4].setHandler(btnModeChooseBrightnessLevel);
  buttons[5].setHandler(btnLightEntireBoard);
  buttons[6].setHandler(btnExitAdminMode);
  buttons[7].setHandler(emptyBtnHandler);
  buttons[8].setHandler(btnCtrlAltDelete);

  buttons[6].setDebounceInterval(EXIT_ADMIN_DEBOUNCE); // 1 second press required.
  buttons[8].setDebounceInterval(REBOOT_DEBOUNCE); // 3 second press required.
}

/** Button 9 in various sub menus is "return to main menu" */
static void btnGoToMainMenu(uint8_t btnId, uint8_t btnState) {
  if (btnState == BTN_OPEN) { return; }
  initMainMenu();
}

////////////// Button functions for TEST_ONE_SIGN ////////////////

static void btnPrevSign(uint8_t btnId, uint8_t btnState) {
  if (btnState == BTN_OPEN) { return; }
  signs[currentSign].disable();
  if (currentSign == 0) {
    currentSign = signs.size() - 1;
  } else {
    currentSign--;
  }
  signs[currentSign].enable();
  DBGPRINTU("Testing sign:", currentSign);
}

static void btnNextSign(uint8_t btnId, uint8_t btnState) {
  if (btnState == BTN_OPEN) { return; }
  signs[currentSign].disable();
  currentSign = (currentSign + 1) % signs.size();
  signs[currentSign].enable();
  DBGPRINTU("Testing sign:", currentSign);
}

////////////// Button functions for TEST_EACH_EFFECT ////////////////

static void btnPrevEffect(uint8_t btnId, uint8_t btnState) {
  if (btnState == BTN_OPEN) { return; }
  if (currentEffect == 0) {
    currentEffect = NUM_EFFECTS - 1;
  } else {
    currentEffect--;
  }
  // TODO(aaron): Cancel current effect; initialize the new effect.
  DBGPRINTU("Testing effect:", currentEffect);
}

static void btnNextEffect(uint8_t btnId, uint8_t btnState) {
  if (btnState == BTN_OPEN) { return; }
  currentEffect = (currentEffect + 1) % NUM_EFFECTS;
  // TODO(aaron): Cancel current effect; initialize the new effect.
  DBGPRINTU("Testing effect:", currentEffect);
}

////////////// Button functions for TEST_SENTENCE ////////////////

static void btnPrevSentence(uint8_t btnId, uint8_t btnState) {
  if (btnState == BTN_OPEN) { return; }
  if (currentSentence == 0) {
    currentSentence = sentences.size() - 1;
  } else {
    currentSentence--;
  }
  // TODO(aaron): Cancel current sentence/effect; show new sentence w/ selected effect.
  DBGPRINTU("Testing sentence:", currentSentence);
}

static void btnNextSentence(uint8_t btnId, uint8_t btnState) {
  if (btnState == BTN_OPEN) { return; }
  currentSentence = (currentSentence + 1) % sentences.size();
  // TODO(aaron): Cancel current sentence/effect; show new sentence w/ selected effect.
  DBGPRINTU("Testing sentence:", currentSentence);
}

////////////// Button functions for CONFIG_BRIGHTNESS ////////////////

static void setBrightness(uint8_t brightness) {
  fieldConfig.maxBrightness = brightness;
  isConfigDirty = true;
  printCurrentBrightness();
  configMaxPwm();
}

static void btnBrightness0(uint8_t btnId, uint8_t btnState) {
  if (btnState == BTN_OPEN) { return; }
  setBrightness(BRIGHTNESS_POWER_SAVE_2);
}

static void btnBrightness1(uint8_t btnId, uint8_t btnState) {
  if (btnState == BTN_OPEN) { return; }
  setBrightness(BRIGHTNESS_POWER_SAVE_1);
}

static void btnBrightness2(uint8_t btnId, uint8_t btnState) {
  if (btnState == BTN_OPEN) { return; }
  setBrightness(BRIGHTNESS_NORMAL);
}

static void btnBrightness3(uint8_t btnId, uint8_t btnState) {
  if (btnState == BTN_OPEN) { return; }
  setBrightness(BRIGHTNESS_FULL);
}


////////////// Main admin state machine loop ////////////////

void loopStateAdmin() {
  switch (adminState) {
  case AS_MAIN_MENU:
    // TODO(aaron): First sign should blink slowly.
    break;
  case AS_IN_ORDER_TEST:
    // TODO(aaron): Scroll through signs one-by-one for a second each.
    break;
  case AS_TEST_ONE_SIGN:
    // One sign turned on @ configured brightness level on state entry.
    // Nothing to monitor in-state.
    break;
  case AS_TEST_EACH_EFFECT:
    // TODO(aaron): Show current selected sentence, apply configured effect.
    break;
  case AS_TEST_SENTENCE:
    // TODO(aaron): Show current selected sentence, apply configured effect.
    break;
  case AS_CONFIG_BRIGHTNESS:
    // TODO(aaron): 1--4 signs slowly blink at current brightness level.
    break;
  case AS_ALL_SIGNS_ON:
    // All signs turned on @ configured brightness level on state entry.
    // Nothing to monitor in-state.
    break;
  case AS_EXITING:
    // TODO(aaron): first 3 signs flash 3 times.
    // TODO(aaron): Decrement countDownBlinks
    if (countDownBlinks == 0) {
      // Done with the sign-off indicator; actually exit.
      setMacroStateRunning();
    }
    break;
  case AS_REBOOTING:
    // TODO(aaron): first 3 signs flash 5 times.
    // TODO(aaron): Decrement countDownBlinks
    if (countDownBlinks == 0) {
      // Done with the sign-off indicator; actually reboot.
      DBGPRINT("*** REBOOTING SYSTEM ***");
      NVIC_SystemReset(); // Adios!
    }
    break;
  default:
    DBGPRINTX("Unknown admin state:", (unsigned int)adminState);
    DBGPRINT("Reverting to main menu state");
    initMainMenu();
    break;
  }
}

/** Switch to the MS_ADMIN MacroState. */
void setMacroStateAdmin() {
  DBGPRINT(">>>> Entering ADMIN MacroState <<<<");
  macroState = MS_ADMIN;

  // Reset all Admin state to entry defaults
  countDownBlinks = 0;
  currentEffect = 0;
  currentSign = 0;
  currentSentence = 0;
  isConfigDirty = false; // No modifications to persistent state made yet.

  initMainMenu(); // Reconfigure button functions for admin mode.
}

