// (c) Copyright 2022 Aaron Kimball

#ifndef _LTA_ADMIN_STATE_H
#define _LTA_ADMIN_STATE_H

/** State machine within the MS_ADMIN macro state. */
enum AdminState {
  AS_MAIN_MENU,         // Waiting at main menu.
  AS_IN_ORDER_TEST,     // Testing each sign in series.
  AS_TEST_ONE_SIGN,     // Testing a single sign, user can cursor left/right.
  AS_TEST_EACH_EFFECT,  // Testing effects, user can cursor left/right.
  AS_TEST_SENTENCE,     // Testing each sentence, user can cursor left/right.
  AS_CONFIG_BRIGHTNESS, // Configuring the current brightness level.
  AS_ALL_SIGNS_ON,      // Turn on all signs at configured brightness level.
  AS_EXITING,           // Preparing to exit admin state.
  AS_REBOOTING,         // Preparing to reboot.
  AS_WAIT_FOR_CLEAR_BTNS, // Waiting for user to release buttons before returning
                          // to main menu.
};

extern AdminState adminState;

/** Main loop while in MS_ADMIN MacroState. */
extern void loopStateAdmin();

/** Switch to the MS_ADMIN MacroState. */
extern void setMacroStateAdmin();


#endif /* _LTA_ADMIN_STATE_H */
