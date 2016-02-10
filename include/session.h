#ifndef SESSION_H
#define SESSION_H

#include "app.h"
#include "spandsp.h"
#include "udptl.h"

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

typedef enum {
    FAX_SESSION_DIR_OUT,
    FAX_SESSION_DIR_IN,
} session_dir_e;

typedef struct session_t session_t;
typedef struct fax_params_t fax_params_t;

typedef int (t38_send_callback)(const session_t *session, uint8_t *msgbuf,
                           int msglen);

struct fax_params_t {
    struct {
        t38_gateway_state_t  *t38_gw_state;
        t38_core_state_t     *t38_core;
        udptl_state_t        *udptl_state;

        char *ident;
        char *header;

        uint8_t use_ecm:     1,
                disable_v17: 1,
                verbose:     1,
                done:        1,
                reserve:     3;
    } pvt;

    struct {
        uint16_t T38FaxVersion;
        uint32_t T38MaxBitRate;
        uint32_t T38FaxMaxBuffer;
        uint32_t T38FaxMaxDatagram;
        char    *T38FaxRateManagement;
        char    *T38FaxUdpEC;
        char    *T38VendorInfo;
        uint8_t T38FaxFillBitRemoval:  1,
                T38FaxTranscodingMMR:  1,
                T38FaxTranscodingJBIG: 1,
                reserve:               5;
    } t38_options;

    uint8_t fax_success;

    char log_tag[64];

    t38_send_callback *send_cb;

    session_t *session;
};

struct session_t {
    int  ses_id;
    char call_id[32];
    int  sidx;

    int  fds;
    session_t *peer_ses;

    uint32_t    rem_ip;
    uint16_t    rem_port;

    uint32_t    loc_ip;
    uint16_t    loc_port;

    struct sockaddr_in remaddr;

    session_state_e state;
    session_mode_e  mode;

    uint8_t     FLAG_IN:1,
                FLAG_RESERV:7;

    pthread_t   fax_proc_thread;

    fax_params_t fax_params;
};




session_t *session_create(session_mode_e mode, int sidx, session_dir_e dir);
void session_destroy(session_t *session);

int session_initCtrl(session_t *session);
int session_init(session_t *session, const char *call_id, uint32_t remote_ip,
                 uint16_t remote_port);

int session_proc(session_t *session);
int session_procFax(session_t *session);
int session_procCMD(session_t *session);

#endif // SESSION_H
