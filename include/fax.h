#ifndef FAX_H_
#define FAX_H_

#include <sys/socket.h>
#include <stdint.h>

#include "app.h"
#include "spandsp.h"
#include "spandsp/expose.h"
#include "udptl.h"
#include "session.h"

typedef enum {
    FAX_TRANSPORT_T38_MOD,
    FAX_TRANSPORT_AUDIO_MOD
} fax_transport_mod_e;

int fax_sessionInit(session_t *session, t38_send_callback *send_cb);
int fax_sessionDestroy(session_t *session);

int fax_rxUDPTL(const session_t *session, const uint8_t *buf, int len);

int fax_rxAUDIO(const session_t *session, const uint8_t *buf, int len);
int fax_txAUDIO(const session_t *session, const uint8_t *buf, int *len);

#endif /* FAX_H_ */
