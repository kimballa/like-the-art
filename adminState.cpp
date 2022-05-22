// (c) Copyright 2022 Aaron Kimball

#include "like-the-art.h"

// State machine describing where we are within the admin options.
AdminState adminState = AS_INITIALIZING;

static void attachAdminButtonHandlers() {
}

void loopStateAdmin() {
  // Nothing to actually do while in the admin state.
  // We just wait for button presses and take action there specifically.
  // TODO: Actually, stuff should blink.
}

/** Switch to the MS_ADMIN MacroState. */
void setMacroStateAdmin() {
  DBGPRINT(">>>> Entering ADMIN MacroState <<<<");
  macroState = MS_ADMIN;
  adminState = AS_INITIALIZING;
  attachAdminButtonHandlers();
}
