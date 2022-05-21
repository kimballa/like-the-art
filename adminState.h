// (c) Copyright 2022 Aaron Kimball

#ifndef _LTA_ADMIN_STATE_H
#define _LTA_ADMIN_STATE_H

enum AdminState {
  AS_INITIALIZING,   // Admin state just started.

};

extern AdminState adminState;

extern void loopStateAdmin();


#endif /* _LTA_ADMIN_STATE_H */
