// (c) Copyright 2022 Aaron Kimball

#ifndef _LTA_ADMIN_STATE_H
#define _LTA_ADMIN_STATE_H

enum AdminState {
  AS_INITIALIZING,   // Admin state just started.

};

extern AdminState adminState;

/** Main loop while in MS_ADMIN MacroState. */
extern void loopStateAdmin();

/** Switch to the MS_ADMIN MacroState. */
extern void setMacroStateAdmin();


#endif /* _LTA_ADMIN_STATE_H */
