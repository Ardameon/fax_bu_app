#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "msg_proc.h"

#define MESSAGE_BUF_LEN 256

#define IP2STR_MAX 4


static const char *msgFaxModeStr(fax_mode_e mode)
{
    switch (mode)
    {
        case FAX_MODE_GW_GW:   return "GG";
        case FAX_MODE_GW_TERM: return "GT";
        default:               return "UN";
    }
}


static const char *msgErrStr(sig_msg_error_e err)
{
    switch (err)
    {
        case FAX_ERROR_INTERNAL: return "INTERNAL_ERR";
        default:                 return "UNKNOWN_ERR";
    }
}


static char *ip2str(uint32_t ip, int id)
{
	static char ip_str[2][64];
	char *p;
	struct in_addr ipaddr;

	if(id < 0 || id >= IP2STR_MAX) return "";

	p = ip_str[id];

	ipaddr.s_addr = htonl(ip);

	strcpy(p, inet_ntoa(ipaddr));

	return p;
}


static int msgCreateSetup(sig_message_setup_t *message, uint8_t *msg_buf)
{
    int len = 0;

    len = snprintf((char *)msg_buf, MESSAGE_BUF_LEN, "SETUP %u %s:%u %s:%u %s",
                  message->call_id,
                  ip2str(message->src_ip, 0), message->src_port,
                  ip2str(message->dst_ip, 1), message->dst_port,
                  msgFaxModeStr(message->mode));

    return len + 1;
}


static int msgCreateError(sig_message_error_t *message, uint8_t *msg_buf)
{
    int len = 0;

    len = snprintf((char *)msg_buf, MESSAGE_BUF_LEN, "ERROR %u %s",
                   message->call_id, msgErrStr(message->err));

    return len + 1;
}


static int msgCreateOk(sig_message_ok_t *message, uint8_t *msg_buf)
{
    int len = 0;

    len = snprintf((char *)msg_buf, MESSAGE_BUF_LEN, "SETUP %u %s:%u",
                  message->call_id,
                  ip2str(message->ip, 0), message->port);

    return len + 1;
}


int  msgCreate(sig_message_t *message, uint8_t **msg_buf)
{
    int ret_val = -1;
    uint8_t *buf = NULL;

    if(!message || !msg_buf) goto _exit;

    buf = calloc(MESSAGE_BUF_LEN, sizeof(**msg_buf));
    if(!buf)
    {
        ret_val = -2;
        goto _exit;
    }

    switch(message->type)
    {
        case FAX_MSG_SETUP:
            ret_val = msgCreateSetup((sig_message_setup_t *)message, buf);
            break;

        case FAX_MSG_OK:
            ret_val = msgCreateOk((sig_message_ok_t *)message, buf);
            break;

        case FAX_MSG_ERROR:
            ret_val = msgCreateError((sig_message_error_t *)message, buf);
            break;

        default:
            ret_val = -3;
            break;
    }

_exit:
    return ret_val;
}


void msgDestroy(uint8_t *msg_buf)
{
    if(msg_buf) free(msg_buf);
}


