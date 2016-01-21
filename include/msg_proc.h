#ifndef MSG_PROC_H
#define MSG_PROC_H

#include <stdint.h>
#include "fax_bu.h"

#define MSG_STR_ERROR_INTERNAL "INTERNAL_ERR"
#define MSG_STR_ERROR_UNKNOWN  "UNKNOWN_ERR"

#define MSG_STR_MODE_GG "GG"
#define MSG_STR_MODE_GT "GT"
#define MSG_STR_MODE_UN "UN"

#define MSG_BUF_LEN_MAX 256

typedef enum {
    FAX_MSG_SETUP,
    FAX_MSG_OK,
    FAX_MSG_ERROR
} sig_msg_type_e;

typedef enum {
    FAX_ERROR_UNKNOWN,
    FAX_ERROR_INTERNAL
} sig_msg_error_e;

typedef struct sig_message_t {
    sig_msg_type_e type;
    char           call_id[32];
} sig_message_t;

typedef struct sig_message_setup_t {
    sig_message_t  msg;
    uint32_t       src_ip;
    uint16_t       src_port;
    uint32_t       dst_ip;
    uint16_t       dst_port;
    fax_mode_e     mode;
} sig_message_setup_t;

typedef struct sig_message_ok_t {
    sig_message_t  msg;
    uint32_t       ip;
    uint16_t       port;
} sig_message_ok_t;

typedef struct sig_message_error_t {
    sig_message_t  msg;
    sig_msg_error_e err;
} sig_message_error_t;

int  msg_createSetup(const char *call_id,
                     uint32_t src_ip, uint16_t src_port,
                     uint32_t dst_ip, uint16_t dst_port,
                     fax_mode_e mode, sig_message_setup_t **msg_setup);
int  msg_createOk(const char *call_id, uint32_t ip, uint16_t port,
                  sig_message_ok_t **msg_ok);
int  msg_createError(const char *call_id, sig_msg_error_e err,
                  sig_message_error_t **msg_error);

int  msg_bufCreate(const sig_message_t *message, uint8_t **msg_buf);
void msg_bufDestroy(uint8_t *msg_buf);

int  msg_parse(const uint8_t *msg_buf, sig_message_t **message);
void msg_destroy(sig_message_t *message);

int  msg_print(const sig_message_t *message, char *buf, int len);

char *ip2str(uint32_t ip, int id);
const char *msg_msgTypeStr(sig_msg_type_e type);

#endif // MSG_PROC_H
