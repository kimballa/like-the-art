// (c) Copyright 2022 Aaron Kimball

#include "like-the-art.h"

// State machine describing where we are within the debug options.
DebugState debugState = DS_INITIALIZING;

void loopStateDebug() {
  // Nothing to actually do while in the debug state.
  // We just wait for button presses and take action there specifically.
  // TODO: Actually, stuff should blink.
}
