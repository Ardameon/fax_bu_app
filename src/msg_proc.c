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
        case FAX_MODE_GW_GW:   return MSG_STR_MODE_GG;
        case FAX_MODE_GW_TERM: return MSG_STR_MODE_GT;
        default:               return MSG_STR_MODE_UN;
    }
}


static const char *msgErrStr(sig_msg_error_e err)
{
    switch (err)
    {
        case FAX_ERROR_INTERNAL: return MSG_STR_ERROR_INTERNAL;
        default:                 return MSG_STR_ERROR_UNKNOWN;
    }
}


char *ip2str(uint32_t ip, int id)
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


/*============================================================================*/


static int msgBufCreateSetup(const sig_message_setup_t *message,
                             uint8_t *msg_buf)
{
    int len = 0;

    switch(message->mode)
    {
        case FAX_MODE_GW_GW:
            len = snprintf((char *)msg_buf,
                           MESSAGE_BUF_LEN, "%s %s %s %s:%u %s:%u\r\n",
                           MSG_STR_SIG_SETUP, message->msg.call_id,
                           msgFaxModeStr(message->mode),
                           ip2str(message->src_ip, 0), message->src_port,
                           ip2str(message->dst_ip, 1), message->dst_port);
            break;

        case FAX_MODE_GW_TERM:
            len = snprintf((char *)msg_buf,
                           MESSAGE_BUF_LEN, "%s %s %s %s:%u\r\n",
                           MSG_STR_SIG_SETUP, message->msg.call_id,
                           msgFaxModeStr(message->mode),
                           ip2str(message->src_ip, 0), message->src_port);
            break;

        default:
            break;

    }

    return len;
}


static int msgBufCreateError(const sig_message_error_t *message,
                             uint8_t *msg_buf)
{
    int len = 0;

    len = snprintf((char *)msg_buf, MESSAGE_BUF_LEN, "%s %s %s\r\n",
                   MSG_STR_SIG_ERROR, message->msg.call_id,
                   msgErrStr(message->err));

    return len;
}


static int msgBufCreateOk(const sig_message_ok_t *message, uint8_t *msg_buf)
{
    int len = 0;

    len = snprintf((char *)msg_buf, MESSAGE_BUF_LEN, "%s %s %s:%u\r\n",
                   MSG_STR_SIG_OK, message->msg.call_id,
                   ip2str(message->ip, 0), message->port);

    return len;
}


int  msgBufCreate(const sig_message_t *message, uint8_t **msg_buf)
{
    int ret_val = 0;
    uint8_t *buf = NULL;

    if(!message || !msg_buf)
    {
        ret_val = -1; goto _exit;
    }

    buf = calloc(MESSAGE_BUF_LEN, sizeof(**msg_buf));
    if(!buf)
    {
        ret_val = -2; goto _exit;
    }

    switch(message->type)
    {
        case FAX_MSG_SETUP:
            ret_val = msgBufCreateSetup((sig_message_setup_t *)message, buf);
            break;

        case FAX_MSG_OK:
            ret_val = msgBufCreateOk((sig_message_ok_t *)message, buf);
            break;

        case FAX_MSG_ERROR:
            ret_val = msgBufCreateError((sig_message_error_t *)message, buf);
            break;

        default:
            ret_val = -3;
            break;
    }

    *msg_buf = buf;

_exit:
    return ret_val;
}


/*============================================================================*/


void msgBufDestroy(uint8_t *msg_buf)
{
    if(msg_buf) free(msg_buf);
}


/*============================================================================*/


static int msgParseSetup(const char *msg_payload, sig_message_setup_t **message)
{
    int ret_val = 0, res = 0;
    sig_message_setup_t *msg;
    char src_ip_port_str[32];
    char dst_ip_port_str[32];
    char mode_str[8];
    char *p, *ip, *port;

    msg = calloc(sizeof(sig_message_setup_t), 1);
    if(!msg)
    {
        ret_val = -1; goto _exit;
    }

    res = sscanf(msg_payload, "%s %s %s", mode_str,
                 src_ip_port_str, dst_ip_port_str);
    if(res < 2)
    {
        ret_val = -2; goto _exit;
    }

    p = strchr(src_ip_port_str, ':');
    if(!p)
    {
        ret_val = -3; goto _exit;
    }

    ip = src_ip_port_str;
    port = p + 1;
    *p = '\0';

    msg->src_ip = ntohl(inet_addr(ip));
    msg->src_port = strtoul(port, NULL, 10);

    if(!strcmp(mode_str, MSG_STR_MODE_GG))
    {
        msg->mode = FAX_MODE_GW_GW;

        p = strchr(dst_ip_port_str, ':');
        if(!p)
        {
            ret_val = -4; goto _exit;
        }

        ip = dst_ip_port_str;
        port = p + 1;
        *p = '\0';

        msg->dst_ip = ntohl(inet_addr(ip));
        msg->dst_port = strtoul(port, NULL, 10);

    } else if(!strcmp(mode_str, MSG_STR_MODE_GT)) {
        msg->mode = FAX_MODE_GW_TERM;
    } else {
        msg->mode = FAX_MODE_UNKNOWN;
    }

    *message = msg;

_exit:
    return ret_val;
}


static int msgParseOk(const char *msg_payload, sig_message_ok_t **message)
{
    int ret_val = 0, res = 0;
    sig_message_ok_t *msg;
    char ip_port_str[32];
    char *p, *ip, *port;

    msg = calloc(sizeof(sig_message_ok_t), 1);
    if(!msg)
    {
        ret_val = -1; goto _exit;
    }

    res = sscanf(msg_payload, "%s", ip_port_str);
    if(res < 1)
    {
        ret_val = -2; goto _exit;
    }

    p = strchr(ip_port_str, ':');
    if(!p) goto _exit;

    ip = ip_port_str;
    port = p + 1;
    *p = '\0';

    msg->ip = ntohl(inet_addr(ip));
    msg->port = strtoul(port, NULL, 10);

    *message = msg;

_exit:
    return ret_val;
}


static int msgParseError(const char *msg_payload, sig_message_error_t **message)
{
    int ret_val = 0, res = 0;
    sig_message_error_t *msg;
    char error_str[64];

    msg = calloc(sizeof(sig_message_error_t), 1);
    if(!msg)
    {
        ret_val = -1; goto _exit;
    }

    res = sscanf(msg_payload, "%s", error_str);
    if(res < 1)
    {
        ret_val = -2; goto _exit;
    }

    if(!strcmp(error_str, MSG_STR_ERROR_INTERNAL))
    {
        msg->err = FAX_ERROR_INTERNAL;
    } else {
        msg->err = FAX_ERROR_UNKNOWN;
    }

    *message = msg;

_exit:
    return ret_val;
}


int msgParse(const uint8_t *msg_buf, sig_message_t **message)
{
    int ret_val = 0;
    char msg_type_str[32];
    char *msg_payload;
    char call_id[32];
    sig_msg_type_e msg_type;

    int res;

    if(!msg_buf || !message)
    {
        ret_val = -1; goto _exit;
    }

    res = sscanf((char *)msg_buf, "%s %s", msg_type_str, call_id);
    if(res < 2)
    {
        ret_val = -2; goto _exit;
    }

    msg_payload = (char *)
            (msg_buf + strlen(msg_type_str) + strlen(call_id) + 2);

    if(!strcmp(msg_type_str, MSG_STR_SIG_SETUP))
    {
        res = msgParseSetup(msg_payload, (sig_message_setup_t **)message);
        msg_type = FAX_MSG_SETUP;
    } else if(!strcmp(msg_type_str, MSG_STR_SIG_OK)) {
        res = msgParseOk(msg_payload, (sig_message_ok_t **)message);
        msg_type = FAX_MSG_OK;
    } else if(!strcmp(msg_type_str, MSG_STR_SIG_ERROR)) {
        res = msgParseError(msg_payload, (sig_message_error_t **)message);
        msg_type = FAX_MSG_ERROR;
    } else{
        ret_val = -3; goto _exit;
    }

    if(res < 0)
    {
        ret_val = -4; goto _exit;
    }

    (*message)->type = msg_type;
    strcpy((*message)->call_id, call_id);

_exit:
    return ret_val;
}


/*============================================================================*/


void msgDestroy(sig_message_t *message)
{
    if(message) free(message);
}


/*============================================================================*/



























