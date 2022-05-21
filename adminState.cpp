// (c) Copyright 2022 Aaron Kimball

#include "like-the-art.h"

// State machine describing where we are within the admin options.
AdminState adminState = AS_INITIALIZING;

void loopStateAdmin() {
  // Nothing to actually do while in the admin state.
  // We just wait for button presses and take action there specifically.
  // TODO: Actually, stuff should blink.
}
