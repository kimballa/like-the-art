// (c) Copyright 2022 Aaron Kimball

#include "like-the-art.h"

// Delays (milliseconds) for certain button inputs.
static constexpr unsigned int EXIT_ADMIN_DEBOUNCE = 1000;
static constexpr unsigned int REBOOT_DEBOUNCE = 3000;

// State machine describing where we are within the admin options.
static AdminState adminState = AdminState::AS_MAIN_MENU;
static AdminState lastAdminState = AdminState::AS_MAIN_MENU;

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
static void btnCtrlAltDelete(uint8_t btnId, uint8_t btnState);
static void btnModeDarkCal(uint8_t btnId, uint8_t btnState);
static void btnDarkCalIncrease(uint8_t btnId, uint8_t btnState);
static void btnDarkCalDecrease(uint8_t btnId, uint8_t btnState);

////////////// State tracked by various AdminStates ////////////////

// What effect to use for current illumination?
// (mode: TEST_ONE_SIGN, TEST_EACH_EFFECT, TEST_SENTENCE)
static int currentEffect = 0;

// What sign is lit? (mode: TEST_ONE_SIGN, IN_ORDER_TEST)
static int currentSign = 0;

// What sentence is lit? (mode: TEST_SENTENCE)
static int currentSentence = 0;

// Have we changed persistent state that we need to commit?
static bool isConfigDirty = false;

////////////// Button functions for main menu ////////////////

/** "return to main menu" function. */
static void initMainMenu() {
  adminState = AdminState::AS_MAIN_MENU;
  attachAdminButtonHandlers();
  activeAnimation.stop();
  allSignsOff();
  configMaxPwm();
}

/**
 * Set up the full sign in-order test. Auto Light up each sign in series.
 * Repeats indefinitely until a new operation is chosen.
 * Main menu remains active.
 *
 * This can be selected by button 1 in admin mode, or by the out-of-band
 * self-test button.
 */
void performInOrderTest() {
  DBGPRINT("Performing in-order sign test");
  adminState = AdminState::AS_IN_ORDER_TEST;
  activeAnimation.stop();
  allSignsOff();
  configMaxPwm();
  currentSign = 0;
  activeAnimation.setParameters(Sentence(0, 1 << currentSign), Effect::EF_APPEAR, 0, 1000);
  activeAnimation.start();
}

/**
 * Button 1: Full sign in-order test. Auto Light up each sign in series.
 * Repeats indefinitely until a new operation is chosen.
 * Main menu remains active.
 */
static void btnInOrderTest(uint8_t btnId, uint8_t btnState) {
  if (btnState == BTN_OPEN) { return; }

  performInOrderTest();
}

/**
 * Button 2: Switch to mode to light up signs one-by-one; don't auto-progress.
 * Use buttons 4 and 6 to go back/forward. Use button 9 to return to the top level menu.
 */
static void btnModeTestOneSign(uint8_t btnId, uint8_t btnState) {
  if (btnState == BTN_OPEN) { return; }
  adminState = AdminState::AS_TEST_ONE_SIGN;

  attachEmptyButtonHandlers();
  buttons[3].setHandler(btnPrevSign);
  buttons[5].setHandler(btnNextSign);
  buttons[8].setHandler(btnGoToMainMenu);
  buttons[8].setPushDebounceInterval(BTN_DEBOUNCE_MILLIS);

  activeAnimation.stop();
  allSignsOff();
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
  adminState = AdminState::AS_TEST_EACH_EFFECT;

  attachEmptyButtonHandlers();
  buttons[3].setHandler(btnPrevEffect);
  buttons[5].setHandler(btnNextEffect);
  buttons[8].setHandler(btnGoToMainMenu);
  buttons[8].setPushDebounceInterval(BTN_DEBOUNCE_MILLIS);

  activeAnimation.stop();
  allSignsOff();

  DBGPRINT("Testing one effect at a time");
  DBGPRINTU("Active effect:", currentEffect);
  DBGPRINTU("Active sentence:", currentSentence);
  sentences[currentSentence].toDbgPrint();
}

/**
 * Button 4: Light up sentences one-by-one; don't auto-progress. 4/6/9 as before.
 */
static void btnModeTestEachSentence(uint8_t btnId, uint8_t btnState) {
  if (btnState == BTN_OPEN) { return; }
  adminState = AdminState::AS_TEST_SENTENCE;

  attachEmptyButtonHandlers();
  buttons[3].setHandler(btnPrevSentence);
  buttons[5].setHandler(btnNextSentence);
  buttons[8].setHandler(btnGoToMainMenu);
  buttons[8].setPushDebounceInterval(BTN_DEBOUNCE_MILLIS);

  activeAnimation.stop();
  allSignsOff();

  DBGPRINT("Testing one sentence at a time");
  DBGPRINTU("Active effect:", currentEffect);
  DBGPRINTU("Active sentence:", currentSentence);
  sentences[currentSentence].toDbgPrint();
}

// Set up the animation that expresses the current brightness level
// by lighting up 1 to 4 signs.
static void brightnessSelectAnimation() {
  activeAnimation.stop();
  allSignsOff();
  configMaxPwm();
  // Turn on 1--4 signs at this pwm.
  // The BRIGHTNESS_xyz enums are actually coded as 1 to 4 full bits, so we can
  // use that value directly as the "sentence" bitmask to display.
  activeAnimation.setParameters(Sentence(0, fieldConfig.maxBrightness), Effect::EF_BLINK, 0,
      durationForBlinkCount(1));
  activeAnimation.start();
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
  adminState = AdminState::AS_CONFIG_BRIGHTNESS;

  attachEmptyButtonHandlers();
  buttons[0].setHandler(btnBrightness0);
  buttons[1].setHandler(btnBrightness1);
  buttons[2].setHandler(btnBrightness2);
  buttons[3].setHandler(btnBrightness3);
  buttons[8].setHandler(btnGoToMainMenu);
  buttons[8].setPushDebounceInterval(BTN_DEBOUNCE_MILLIS);

  brightnessSelectAnimation();

  DBGPRINT("Configuring brightness level");
  printCurrentBrightness();
}

/**
 * Button 6: Light up all 16 signs at once at the chosen brightness level.
 * Buttons remain at main menu.
 */
static void btnLightEntireBoard(uint8_t btnId, uint8_t btnState) {
  if (btnState == BTN_OPEN) { return; }
  adminState = AdminState::AS_ALL_SIGNS_ON;
  activeAnimation.stop();
  configMaxPwm();
  allSignsOn();
  DBGPRINT("Turning on all signs");
}

/**
 * Button 7: Hold 1 second to exit admin mode.
 * The first 3 signs flash 3 times and then admin mode ends.
 * Trigger on button release.
 */
static void btnExitAdminMode(uint8_t btnId, uint8_t btnState) {
  if (btnState == BTN_PRESSED) { return; }
  adminState = AdminState::AS_EXITING;

  activeAnimation.stop();
  allSignsOff();
  attachEmptyButtonHandlers();
  if (isConfigDirty) {
    saveFieldConfig(&fieldConfig);
  }
  activeAnimation.setParameters(Sentence(0, 0x7), Effect::EF_BLINK_FAST, 0,
      durationForFastBlinkCount(3));
  activeAnimation.start();
  DBGPRINT("Exiting admin menu...");
}

// When in DARK sensor calibration mode, LED 15 is lit when the sensor is over the
// calibrated 'DARK' threshold.
static constexpr unsigned int DARK_SENSOR_HIGH_SIGN_IDX = MAX_SIGN_ID;

/**
 * Turn on the sign LED when DARK is high, off otherwise.
 */
static void showDarkSensorIndicator() {
  if (getLastDarkSensorValue() >= getDarkThreshold()) {
    signs[DARK_SENSOR_HIGH_SIGN_IDX].enable();
  } else {
    signs[DARK_SENSOR_HIGH_SIGN_IDX].disable();
  }
}

/**
 * Show an animation that informs the user about the current state of the
 * DARK sensor calibration level.
 *
 * We want to display a range over [-5, +5]. LED sign 5 is the 'zero' position; we set
 * an animation where that sign always blinks, along with up to 5 signs to the left
 * (negative) or right (positive).
 *
 * The last sign (LED 15) is lit if the DARK sensor is currently reading as DARK=true
 * based on the indicated calibration level.
 */
static void darkCalibrationAnimation() {
  activeAnimation.stop();
  allSignsOff();
  configMaxPwm();

  // Turn on signs 0--4 for cal=[-5...-1], always 5 blinking (0), and signs 6..10 for cal=[1...5].
  static constexpr int CENTER_SIGN = 5; // this is the sign to indicate "zero"; always lit.
  // The upper or lower limit (inclusive) of sign idx's to light up:
  int signLimit = CENTER_SIGN + fieldConfig.darkSensorCalibration;
  int lowerSignLimit = min(CENTER_SIGN, signLimit);
  int upperSignLimit = max(CENTER_SIGN, signLimit);

  // Put a string of 1 bits between those two ends in a "sentence" to indicate the signs to light up:
  unsigned int signBitsToLight = 0;
  for (int i = 0; i < NUM_SIGNS; i++) {
    if (i >= lowerSignLimit && i <= upperSignLimit) {
      signBitsToLight |= (1 << i);
    }
  }

  // And set those signs to blinking.
  activeAnimation.setParameters(Sentence(0, signBitsToLight), Effect::EF_BLINK, 0,
      durationForBlinkCount(1));
  activeAnimation.start();

  showDarkSensorIndicator();

  DBGPRINTU("DARK threshold set to:", getDarkThreshold());
  DBGPRINTU("Current DARK reading:", getLastDarkSensorValue());
}

/**
 * Button 8 - Enter mode to calibrate DARK sensor sensitivity.
 * Use buttons 4 and 6 to decrease threshold or increase threshold in range from -5 to +5.
 * This value is persistent across reboots.
 * Press 9 to return to top menu.
 */
static void btnModeDarkCal(uint8_t btnId, uint8_t btnState) {
  if (btnState == BTN_OPEN) { return; }
  adminState = AdminState::AS_DARK_CALIBRATION;

  attachEmptyButtonHandlers();
  buttons[3].setHandler(btnDarkCalDecrease);
  buttons[5].setHandler(btnDarkCalIncrease);
  buttons[8].setHandler(btnGoToMainMenu);
  buttons[8].setPushDebounceInterval(BTN_DEBOUNCE_MILLIS);

  darkCalibrationAnimation();

  DBGPRINT("Calibrating DARK sensor threshold level");
  printDarkThreshold();
}


/**
 * Button 9 - Hold 3 seconds to completely reset system.
 * The first 3 signs flash 5 times and then the system is rebooted.
 * Trigger on button release.
 */
static void btnCtrlAltDelete(uint8_t btnId, uint8_t btnState) {
  if (btnState == BTN_PRESSED) { return; }
  adminState = AdminState::AS_REBOOTING;

  activeAnimation.stop();
  allSignsOff();
  attachEmptyButtonHandlers();
  if (isConfigDirty) {
    saveFieldConfig(&fieldConfig);
  }

  // Set up blinking animation before we reboot.
  activeAnimation.setParameters(Sentence(0, 0x7), Effect::EF_BLINK_FAST, 0,
      durationForFastBlinkCount(5));
  activeAnimation.start();
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
  buttons[7].setHandler(btnModeDarkCal);
  buttons[8].setHandler(btnCtrlAltDelete);

  buttons[6].setPushDebounceInterval(EXIT_ADMIN_DEBOUNCE); // 1 second press required.
  buttons[8].setPushDebounceInterval(REBOOT_DEBOUNCE); // 3 second press required.
}

/** Button 9 in various sub menus is "return to main menu" */
static void btnGoToMainMenu(uint8_t btnId, uint8_t btnState) {
  if (btnState == BTN_OPEN) { return; }
  // Don't directly return to main menu functionality; wait for
  // the user to release all buttons first. In the meantime, null
  // out what the buttons do.
  attachEmptyButtonHandlers();
  adminState = AdminState::AS_WAIT_FOR_CLEAR_BTNS;
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
  // Cancel current effect; new effect will be initialized immediately in loop().
  activeAnimation.stop();
  DBGPRINTU("Testing effect:", currentEffect);
}

static void btnNextEffect(uint8_t btnId, uint8_t btnState) {
  if (btnState == BTN_OPEN) { return; }
  currentEffect = (currentEffect + 1) % NUM_EFFECTS;
  // Cancel current effect; new effect will be initialized immediately in loop().
  activeAnimation.stop();
  DBGPRINTU("Testing effect:", currentEffect);
  debugPrintEffect((Effect)currentEffect);
}

////////////// Button functions for TEST_SENTENCE ////////////////

static void btnPrevSentence(uint8_t btnId, uint8_t btnState) {
  if (btnState == BTN_OPEN) { return; }
  if (currentSentence == 0) {
    currentSentence = sentences.size() - 1;
  } else {
    currentSentence--;
  }
  // Cancel current sentence/effect; new sentence will be initialized immediately in loop().
  activeAnimation.stop();
  DBGPRINTU("Testing sentence:", currentSentence);
  sentences[currentSentence].toDbgPrint();
}

static void btnNextSentence(uint8_t btnId, uint8_t btnState) {
  if (btnState == BTN_OPEN) { return; }
  currentSentence = (currentSentence + 1) % sentences.size();
  // Cancel current sentence/effect; new sentence will be initialized immediately in loop().
  activeAnimation.stop();
  DBGPRINTU("Testing sentence:", currentSentence);
  sentences[currentSentence].toDbgPrint();
}

////////////// Button functions for CONFIG_BRIGHTNESS ////////////////

static void setBrightness(uint8_t brightness) {
  fieldConfig.maxBrightness = brightness;
  isConfigDirty = true;
  printCurrentBrightness();
  brightnessSelectAnimation();
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

////////////// Button functions for DARK_CALIBRATION ////////////////

/**
 * Update the DARK sensor threshold in a relative way;
 * delta should be +1 or -1 to change the current val.
 */
static void adminAdjustDarkThreshold(int8_t delta) {
  adjustDarkSensorCalibration(fieldConfig.darkSensorCalibration + delta);
  isConfigDirty = true;
  printDarkThreshold();
  darkCalibrationAnimation();
}

static void btnDarkCalIncrease(uint8_t btnId, uint8_t btnState) {
  if (btnState == BTN_OPEN) { return; }
  adminAdjustDarkThreshold(+1); // Increase the threshold
}

static void btnDarkCalDecrease(uint8_t btnId, uint8_t btnState) {
  if (btnState == BTN_OPEN) { return; }
  adminAdjustDarkThreshold(-1); // Decrease the threshold
}

////////////// AdminState utility ////////////////

// Helper macro to insert repetitive enum cases in debugPrintAdminState().
#define PRINT_ADMIN_STATE(as) case AdminState::as: DBGPRINT(#as); break;

void debugPrintAdminState(AdminState state) {
  switch (state) {
    PRINT_ADMIN_STATE(AS_MAIN_MENU);
    PRINT_ADMIN_STATE(AS_IN_ORDER_TEST);
    PRINT_ADMIN_STATE(AS_TEST_ONE_SIGN);
    PRINT_ADMIN_STATE(AS_TEST_EACH_EFFECT);
    PRINT_ADMIN_STATE(AS_TEST_SENTENCE);
    PRINT_ADMIN_STATE(AS_CONFIG_BRIGHTNESS);
    PRINT_ADMIN_STATE(AS_ALL_SIGNS_ON);
    PRINT_ADMIN_STATE(AS_EXITING);
    PRINT_ADMIN_STATE(AS_DARK_CALIBRATION);
    PRINT_ADMIN_STATE(AS_REBOOTING);
    PRINT_ADMIN_STATE(AS_WAIT_FOR_CLEAR_BTNS);
  default:
    DBGPRINTU("Unknown AdminState id:", (unsigned int)state);
    break;
  }
}

////////////// Main admin state machine loop ////////////////

void loopStateAdmin() {
  if (adminState != lastAdminState) {
    // adminState has changed in prior loop; print out state change notification.
    DBGPRINT(">>> AdminState changed to:");
    debugPrintAdminState(adminState);
  }
  lastAdminState = adminState;

  if (adminState == AdminState::AS_WAIT_FOR_CLEAR_BTNS) {
    // After we press a "return to main menu" button, wait for user to stop
    // pressing buttons before reassigning their capabilities.
    bool buttonsAreClear = true;
    for (int i = 0; i < NUM_MAIN_BUTTONS; i++) {
      if (buttons[i].getState() == BTN_PRESSED) {
        buttonsAreClear = false;
        break;
      }
    }

    if (buttonsAreClear) {
      // Buttons are released; go back to the MAIN_MENU state.
      initMainMenu();
    }
  } else if (adminState == AdminState::AS_DARK_CALIBRATION) {
    // Independent of if the main animation is complete, keep the DARK indicator accurate.
    showDarkSensorIndicator();
  }

  if (activeAnimation.isRunning()) {
    // We triggered an animation within admin mode; just run that.
    activeAnimation.next();
    return;
  }

  // Any animation is complete/idle as we process state changes or other tasks here.

  switch (adminState) {
  case AdminState::AS_MAIN_MENU:
    // First sign should blink slowly.
    activeAnimation.setParameters(Sentence(0, 1), Effect::EF_BLINK, 0, durationForBlinkCount(1));
    activeAnimation.start();
    break;
  case AdminState::AS_IN_ORDER_TEST:
    // Scroll through signs one-by-one for a second each.
    currentSign = (currentSign + 1) % signs.size();
    activeAnimation.setParameters(Sentence(0, 1 << currentSign), Effect::EF_APPEAR, 0, 1000);
    activeAnimation.start();
    break;
  case AdminState::AS_TEST_ONE_SIGN:
    // One sign turned on @ configured brightness level on state entry.
    // Nothing to monitor in-state.
    break;
  case AdminState::AS_TEST_EACH_EFFECT:
  case AdminState::AS_TEST_SENTENCE:
    // Show current selected sentence, apply configured effect.
    activeAnimation.setParameters(sentences[currentSentence], (Effect)currentEffect, 0, 0);
    activeAnimation.start();
    break;
  case AdminState::AS_CONFIG_BRIGHTNESS:
    // 1--4 signs slowly blink at current brightness level.
    brightnessSelectAnimation();
    break;
  case AdminState::AS_ALL_SIGNS_ON:
    // All signs turned on @ configured brightness level on state entry.
    // Nothing to monitor in-state.
    break;
  case AdminState::AS_DARK_CALIBRATION:
    // Keep showing the calibration-level indicator animation.
    darkCalibrationAnimation();
    break;
  case AdminState::AS_EXITING:
    // First 3 signs flash 3 times. Once done, then we exit the admin state.
    // As we are done with the sign-off indicator -- actually exit.
    setMacroStateRunning();
    break;
  case AdminState::AS_REBOOTING:
    // first 3 signs flash 5 times. When done, we do the reboot.
    // As we are done with the sign-off indicator -- actually reboot.
    DBGPRINT("*** REBOOTING SYSTEM ***");
    NVIC_SystemReset(); // Adios!
    break;
  case AdminState::AS_WAIT_FOR_CLEAR_BTNS:
    // Nothing further to do.
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
  macroState = MacroState::MS_ADMIN;

  // Reset all Admin state to entry defaults
  currentEffect = 0;
  currentSign = 0;
  currentSentence = 0;
  isConfigDirty = false; // No modifications to persistent state made yet.

  activeAnimation.stop(); // Cancel any in-flight animation.

  initMainMenu(); // Reconfigure button functions for admin mode.
}

