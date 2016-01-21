#include "session.h"
#include "msg_proc.h"


static int session_id_array[SESSION_ID_CNT];

static int ses_id_static_in = 0;
static int ses_id_static_out = SESSION_ID_OUT;


/*============================================================================*/
static const char *session_modeStr(session_mode_e mode)
{
    switch(mode)
    {
        case FAX_SESSION_MODE_CTRL:     return "MODE_CTRL";
        case FAX_SESSION_MODE_GATEWAY:  return "MODE_GATEWAY";
        case FAX_SESSION_MODE_TERMINAL: return "MODE_TERMINAL";
        default:                        return "MODE_UNKNOWN";
    }
}

static int session_NextID(int in)
{
    int i = 0;
    int ret_val = -1;

    if(in)
    {
        for(i = ses_id_static_in; i < SESSION_ID_OUT; i++)
        {
            if(session_id_array[i] == 0)
            {
                session_id_array[i] = 1;
                ses_id_static_in = i + 1;
                ret_val = i;
                goto _exit;
            }
        }

        for(i = SESSION_ID_IN; i < ses_id_static_in; i++)
        {
            if(session_id_array[i] == 0)
            {
                session_id_array[i] = 1;
                ses_id_static_in = i + 1;
                ret_val = i;
                goto _exit;
            }
        }
    } else {
        for(i = ses_id_static_out; i < SESSION_ID_CNT; i++)
        {
            if(session_id_array[i] == 0)
            {
                session_id_array[i] = 1;
                ses_id_static_in = i + 1;
                ret_val = i;
                goto _exit;
            }
        }

        for(i = SESSION_ID_OUT; i < ses_id_static_out; i++)
        {
            if(session_id_array[i] == 0)
            {
                session_id_array[i] = 1;
                ses_id_static_in = i + 1;
                ret_val = i;
                goto _exit;
            }
        }
    }

_exit:
    return ret_val;
}


session_t *session_create(session_mode_e mode, int sidx, session_dir_e dir)
{
    session_t *new_session = NULL;

    if(sidx < 0 ||
       mode == FAX_SESSION_MODE_UNKNOWN ||
       mode > FAX_SESSION_MODE_TERMINAL ||
       dir > FAX_SESSION_DIR_IN ||
       dir < FAX_SESSION_DIR_OUT)
    {
        goto _exit;
    }

    new_session = calloc(1, sizeof(*new_session));
    if(!new_session)
    {
        app_trace(TRACE_ERR, "Session. Memory allocation for new session failed");
        goto _exit;
    }

    new_session->ses_id = session_NextID(dir);
    new_session->sidx = sidx;
    new_session->mode = mode;
    new_session->state = FAX_SESSION_STATE_NULL;
    new_session->FLAG_IN = dir;
    new_session->fds = -1;

    app_trace(TRACE_INFO, "Session %04x. Created:\n"
              "\tIdx:%02d Mode:'%s' Dir:'%s'",
              new_session->ses_id,
              new_session->sidx,
              session_modeStr(new_session->mode),
              new_session->FLAG_IN ? "IN" : "OUT");

_exit:
    return new_session;
}

void session_destroy(session_t *session)
{
    if(!session) return;

    if(session->fds > 0) close(session->fds);
    free(session);
}


static int session_createListener(uint32_t ip, uint16_t port)
{
    int ret_val = -1;
    int sock = -1;
    struct sockaddr_in local_addr;
    int reuse = 1;

    app_trace(TRACE_INFO, "Session. Create listener: %s:%u",
              ip2str(ip, 0), port);

    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl(ip);
    local_addr.sin_port = htons(port);

    if((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        app_trace(TRACE_ERR, "Session. socket() failed: %s",
                  strerror(errno));
        ret_val = -2; goto _exit;
    }

    fcntl(sock, F_SETFL, O_NONBLOCK);

    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        app_trace(TRACE_ERR, "Session. setsockopt() failed: %s",
                  strerror(errno));
        ret_val = -3; goto _exit;
    }

    if(bind(sock, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0)
    {
        app_trace(TRACE_ERR, "Session. bind() failed: %s",
                  strerror(errno));
        ret_val = -4; goto _exit;
    }

    ret_val = sock;

_exit:
    return ret_val;
}


int session_initCtrl(session_t *session)
{
    int ret_val = 0;
    int fd;
    cfg_t *cfg = app_getCfg();

    if(!session)
    {
        ret_val = -1; goto _exit;
    }

    fd = session_createListener(cfg->local_ip, cfg->local_port);
    if(fd <= 0)
    {
        app_trace(TRACE_INFO, "Session %04x. Listener creation failed (%d)",
                  session->ses_id, fd);
        ret_val = -2; goto _exit;
    }

    session->fds = fd;

_exit:
    return ret_val;
}

static int session_recvMsg(const session_t *session, uint8_t *msgbuf,
                           int msglen)
{
    int ret_val = 0;
    struct sockaddr_storage sa;
    socklen_t sa_len;

    if(!session)
    {
        ret_val = -2; goto _exit;
    }

    ret_val = recvfrom(session->fds, msgbuf, msglen, 0,
                       (struct sockaddr *)(&sa), &sa_len);

    if(session->mode == FAX_SESSION_MODE_CTRL)
    {
        memcpy((void *)(&session->remaddr), &sa, sa_len);
    }

_exit:
    return ret_val;
}


static int session_sendMsg(const session_t *session, uint8_t *msgbuf,
                           int msglen)
{
    int ret_val = 0;

    if(!session)
    {
        ret_val = -1; goto _exit;
    }

    ret_val = sendto(session->fds, msgbuf, msglen, 0,
                     (struct sockaddr *)(&session->remaddr),
                     sizeof(session->remaddr));

_exit:
    return ret_val;
}


int session_proc(const session_t *session)
{
    (void)session;
    return 0;
}


static int session_procMsg(sig_message_t *message)
{
    (void)message;
    return 0;
}


int session_procCMD(const session_t *ctrl_session)
{
    uint8_t buf_recv[MSG_BUF_LEN_MAX];
    uint8_t *buf_send = NULL;
    char msg_str[512];
    int res, ress, ret_val = 0;
    sig_message_t *message_recv = NULL;
    sig_message_t *message_send = NULL;
    cfg_t *cfg = app_getCfg();

    res = session_recvMsg(ctrl_session, buf_recv, MSG_BUF_LEN_MAX);
    if(res < 0)
    {
        app_trace(TRACE_ERR, "Session %04x. Message receiving error (%d) %s",
                  ctrl_session->ses_id, res,
                  (res == -1) ? strerror(errno) : "");
        ret_val = -1; goto _exit;
    }

    res = msg_parse(buf_recv, &message_recv);
    if(res < 0)
    {
        app_trace(TRACE_ERR, "Session %04x. Message parsing error (%d)",
                  ctrl_session->ses_id, res);
        ret_val = -2; goto _exit;
    }

    msg_print(message_recv, msg_str, sizeof(msg_str));
    app_trace(TRACE_INFO, "Session %04x. Message received: %s",
              ctrl_session->ses_id, msg_str);

    res = session_procMsg(message_recv);

    if(res < 0)
    {
        app_trace(TRACE_ERR, "Session %04x. Processing CMD %s "
                  "(call_id: '%s') failed(%d)",
                  ctrl_session->ses_id, msg_msgTypeStr(message_recv->type),
                  message_recv->call_id, res);

        ress = msg_createError(message_recv->call_id, FAX_ERROR_INTERNAL,
                               (sig_message_error_t **)(&message_send));
        if(ress < 0)
        {
            app_trace(TRACE_ERR, "Session %04x. ERROR msg creating failed (%d)",
                      ctrl_session->ses_id, ress);
            ret_val = -3; goto _exit_1;
        }


        ress = msg_bufCreate(message_send, &buf_send);
        if(ress < 0)
        {
            app_trace(TRACE_ERR, "Session %04x. ERROR msg buffer "
                      "creating failed (%d)",
                      ctrl_session->ses_id, ress);
            ret_val = -4; goto _exit_2;
        }
    } else {
        ress = msg_createOk(message_recv->call_id, cfg->local_ip, 33333,
                               (sig_message_ok_t **)(&message_send));
        if(ress < 0)
        {
            app_trace(TRACE_ERR, "Session %04x. OK msg creating failed (%d)",
                      ctrl_session->ses_id, ress);
            ret_val = -5; goto _exit_1;
        }

        ress = msg_bufCreate(message_send, &buf_send);
        if(ress < 0)
        {
            app_trace(TRACE_ERR, "Session %04x. OK msg buffer "
                      "creating failed (%d)",
                      ctrl_session->ses_id, ress);
            ret_val = -6; goto _exit_2;
        }
    }

    msg_print(message_send, msg_str, sizeof(msg_str));
    app_trace(TRACE_INFO, "Session %04x. Message to send: %s",
              ctrl_session->ses_id, msg_str);

    res = session_sendMsg(ctrl_session, buf_send, res);
    if(ress < 0)
    {
        app_trace(TRACE_ERR, "Session %04x. Message sending error (%d) %s",
                  ctrl_session->ses_id, res,
                  (res == -1) ? strerror(errno) : "");
        ret_val = -7;
    }

    msg_bufDestroy(buf_send);

_exit_2:
    msg_destroy(message_send);

_exit_1:
    msg_destroy(message_recv);

_exit:
    return ret_val;
}




