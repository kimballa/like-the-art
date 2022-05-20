// (c) Copyright 2022 Aaron Kimball

#ifndef _LTA_DEBUG_STATE_H
#define _LTA_DEBUG_STATE_H

enum DebugState {
  DS_INITIALIZING,   // Debug state just started.

};

extern DebugState debugState;

extern void loopStateDebug();


#endif /* _LTA_DEBUG_STATE_H */
