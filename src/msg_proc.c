#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "msg_proc.h"

#define MSG_STR_SIG_SETUP "SETUP"
#define MSG_STR_SIG_OK    "OK"
#define MSG_STR_SIG_ERROR "ERROR"

#define MSG_PRINT_BUF_LEN 512

#define IP2STR_MAX 4


static const char *msg_faxModeStr(fax_mode_e mode)
{
    switch (mode)
    {
        case FAX_MODE_GW_GW:   return MSG_STR_MODE_GG;
        case FAX_MODE_GW_TERM: return MSG_STR_MODE_GT;
        default:               return MSG_STR_MODE_UN;
    }
}

const char *msg_msgTypeStr(sig_msg_type_e type)
{
    switch (type)
    {
        case FAX_MSG_SETUP:    return MSG_STR_SIG_SETUP;
        case FAX_MSG_OK:       return MSG_STR_SIG_OK;
        case FAX_MSG_ERROR:    return MSG_STR_SIG_ERROR;
        default:               return "UNKNOWN";
    }
}


static const char *msg_errStr(sig_msg_error_e err)
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


int  msg_createSetup(const char *call_id,
                     uint32_t src_ip, uint16_t src_port,
                     uint32_t dst_ip, uint16_t dst_port,
                     fax_mode_e mode, sig_message_setup_t **msg_setup)
{
    int ret_val = 0;
    sig_message_setup_t *setup = NULL;

    if(!call_id || !msg_setup)
    {
        ret_val = -1; goto _exit;
    }

    setup = calloc(1, sizeof(*setup));
    if(!setup)
    {
        ret_val = -2; goto _exit;
    }

    setup->src_ip = src_ip;
    setup->src_port = src_port;
    setup->dst_ip = dst_ip;
    setup->dst_port = dst_port;
    setup->mode = mode;
    strcpy(setup->msg.call_id, call_id);
    setup->msg.type = FAX_MSG_SETUP;

    *msg_setup = setup;

_exit:
    return ret_val;
}


/*============================================================================*/


int  msg_createOk(const char *call_id, uint32_t ip, uint16_t port,
                  sig_message_ok_t **msg_ok)
{
    int ret_val = 0;
    sig_message_ok_t *ok = NULL;

    if(!call_id || !msg_ok)
    {
        ret_val = -1; goto _exit;
    }

    ok = calloc(1, sizeof(*ok));
    if(!ok)
    {
        ret_val = -2; goto _exit;
    }

    ok->ip = ip;
    ok->port = port;
    strcpy(ok->msg.call_id, call_id);
    ok->msg.type = FAX_MSG_OK;

    *msg_ok = ok;

_exit:
    return ret_val;
}


/*============================================================================*/


int  msg_createError(const char *call_id, sig_msg_error_e err,
                  sig_message_error_t **msg_error)
{
    int ret_val = 0;
    sig_message_error_t *error = NULL;

    if(!call_id || !msg_error)
    {
        ret_val = -1; goto _exit;
    }

    error = calloc(1, sizeof(*error));
    if(!error)
    {
        ret_val = -2; goto _exit;
    }

    error->err = err;
    strcpy(error->msg.call_id, call_id);
    error->msg.type = FAX_MSG_ERROR;

    *msg_error = error;

_exit:
    return ret_val;
}


/*============================================================================*/


static int msg_bufCreateSetup(const sig_message_setup_t *message,
                             uint8_t *msg_buf)
{
    int len = 0;

    switch(message->mode)
    {
        case FAX_MODE_GW_GW:
            len = snprintf((char *)msg_buf,
                           MSG_BUF_LEN_MAX, "%s %s %s %s:%u %s:%u\r\n",
                           msg_msgTypeStr(FAX_MSG_SETUP), message->msg.call_id,
                           msg_faxModeStr(message->mode),
                           ip2str(message->src_ip, 0), message->src_port,
                           ip2str(message->dst_ip, 1), message->dst_port);
            break;

        case FAX_MODE_GW_TERM:
            len = snprintf((char *)msg_buf,
                           MSG_BUF_LEN_MAX, "%s %s %s %s:%u\r\n",
                           msg_msgTypeStr(FAX_MSG_SETUP), message->msg.call_id,
                           msg_faxModeStr(message->mode),
                           ip2str(message->src_ip, 0), message->src_port);
            break;

        default:
            break;

    }

    return len;
}


static int msg_bufCreateError(const sig_message_error_t *message,
                             uint8_t *msg_buf)
{
    int len = 0;

    len = snprintf((char *)msg_buf, MSG_BUF_LEN_MAX, "%s %s %s\r\n",
                   msg_msgTypeStr(FAX_MSG_ERROR), message->msg.call_id,
                   msg_errStr(message->err));

    return len;
}


static int msg_bufCreateOk(const sig_message_ok_t *message, uint8_t *msg_buf)
{
    int len = 0;

    len = snprintf((char *)msg_buf, MSG_BUF_LEN_MAX, "%s %s %s:%u\r\n",
                   msg_msgTypeStr(FAX_MSG_OK), message->msg.call_id,
                   ip2str(message->ip, 0), message->port);

    return len;
}


int  msg_bufCreate(const sig_message_t *message, uint8_t **msg_buf)
{
    int ret_val = 0;
    uint8_t *buf = NULL;

    if(!message || !msg_buf)
    {
        ret_val = -1; goto _exit;
    }

    buf = calloc(MSG_BUF_LEN_MAX, sizeof(**msg_buf));
    if(!buf)
    {
        ret_val = -2; goto _exit;
    }

    switch(message->type)
    {
        case FAX_MSG_SETUP:
            ret_val = msg_bufCreateSetup((sig_message_setup_t *)message, buf);
            break;

        case FAX_MSG_OK:
            ret_val = msg_bufCreateOk((sig_message_ok_t *)message, buf);
            break;

        case FAX_MSG_ERROR:
            ret_val = msg_bufCreateError((sig_message_error_t *)message, buf);
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


void msg_bufDestroy(uint8_t *msg_buf)
{
    if(msg_buf) free(msg_buf);
}


/*============================================================================*/


static int msg_parseSetup(const char *msg_payload, sig_message_setup_t **message)
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


static int msg_parseOk(const char *msg_payload, sig_message_ok_t **message)
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


static int msg_parseError(const char *msg_payload, sig_message_error_t **message)
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


int msg_parse(const uint8_t *msg_buf, sig_message_t **message)
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

    if(!strcmp(msg_type_str, msg_msgTypeStr(FAX_MSG_SETUP)))
    {
        res = msg_parseSetup(msg_payload, (sig_message_setup_t **)message);
        msg_type = FAX_MSG_SETUP;
    } else if(!strcmp(msg_type_str, msg_msgTypeStr(FAX_MSG_OK))) {
        res = msg_parseOk(msg_payload, (sig_message_ok_t **)message);
        msg_type = FAX_MSG_OK;
    } else if(!strcmp(msg_type_str, msg_msgTypeStr(FAX_MSG_ERROR))) {
        res = msg_parseError(msg_payload, (sig_message_error_t **)message);
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


void msg_destroy(sig_message_t *message)
{
    if(message) free(message);
}


/*============================================================================*/

static int msg_printSetup(const sig_message_setup_t *message, char *buf)
{
    sprintf(buf,
            "\t src:     %s:%u\n"
            "\t dst:     %s:%u\n"
            "\t mode:    %s\n",
            ip2str(message->src_ip, 0), message->src_port,
            ip2str(message->dst_ip, 1), message->dst_port,
            msg_faxModeStr(message->mode));

    return 0;
}


static int msg_printOk(const sig_message_ok_t *message, char *buf)
{
    sprintf(buf,
            "\t addr:    %s:%u\n",
            ip2str(message->ip, 0), message->port);

    return 0;
}


static int msg_printError(const sig_message_error_t *message, char *buf)
{
    sprintf(buf,
            "\t error:   %s\n",
            msg_errStr(message->err));

    return 0;
}


int msg_print(const sig_message_t *message, char *buf, int len)
{
    int ret_val = 0;
    char tmp_buf[MSG_PRINT_BUF_LEN];
    char *p;


    if(!message || !buf)
    {
        sprintf(tmp_buf, "PRINT_ERROR\n");
        ret_val = -1; goto _exit;
    }

    sprintf(tmp_buf,
            "%s:\n"
            "\t call_id: '%s'\n",
            msg_msgTypeStr(message->type),
            message->call_id);

    p = tmp_buf + strlen(tmp_buf);

    switch(message->type)
    {
        case FAX_MSG_SETUP:
            msg_printSetup((sig_message_setup_t *)message, p);
            break;

        case FAX_MSG_OK:
            msg_printOk((sig_message_ok_t *)message, p);
            break;

        case FAX_MSG_ERROR:
            msg_printError((sig_message_error_t *)message, p);
            break;

        default:
            ret_val = -2;
            sprintf(p, "\t PRINT_ERROR\n");
            goto _exit;
    }

    strncpy(buf, tmp_buf, len);

_exit:
    return ret_val;
}


/*============================================================================*/

























