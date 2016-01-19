#ifndef MSG_PROC_H
#define MSG_PROC_H

#include <stdint.h>
#include "fax_bu.h"

typedef enum {
    FAX_MSG_SETUP,
    FAX_MSG_OK,
    FAX_MSG_ERROR
} sig_msg_type_e;

typedef enum {
    FAX_ERROR_INTERNAL
} sig_msg_error_e;

typedef struct sig_message_t {
    sig_msg_type_e type;
    uint32_t       call_id;
} sig_message_t;

typedef struct sig_message_setup_t {
    sig_msg_type_e type;
    uint32_t       call_id;
    uint32_t       src_ip;
    uint16_t       src_port;
    uint32_t       dst_ip;
    uint16_t       dst_port;
    fax_mode_e     mode;
} sig_message_setup_t;

typedef struct sig_message_ok_t {
    sig_msg_type_e type;
    uint32_t       call_id;
    uint32_t       ip;
    uint16_t       port;
} sig_message_ok_t;

typedef struct sig_message_error_t {
    sig_msg_type_e  type;
    uint32_t        call_id;
    sig_msg_error_e err;

} sig_message_error_t;

int  msgCreate(sig_message_t *message, uint8_t **msg_buf);
void msgDestroy(uint8_t *msg_buf);

#endif // MSG_PROC_H
