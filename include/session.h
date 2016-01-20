#ifndef SESSION_H
#define SESSION_H

#include "app.h"

#define SESSION_ID_OUT 1024
#define SESSION_ID_IN  0

#define SESSION_ID_CNT (SESSION_ID_OUT * 2)

#define MAX_SESSION

typedef enum {
    FAX_SESSION_STATE_NULL,
    FAX_SESSION_STATE_SETUP,
    FAX_SESSION_STATE_PROGRESS,
    FAX_SESSION_STATE_COMPLETE
} session_state_e;

typedef enum {
    FAX_SESSION_MODE_UNKNOWN,
    FAX_SESSION_MODE_CTRL,
    FAX_SESSION_MODE_GATEWAY,
    FAX_SESSION_MODE_TERMINAL
} session_mode_e;

typedef struct session_t {
    int  ses_id;
    int  peer_ses_id;
    char call_id[32];
    int  sidx;

    session_state_e state;

    session_mode_e  mode;

    uint16_t     FLAG_IN:1,
                 FLAG_RESERV:15;
} session_t;

#endif // SESSION_H
