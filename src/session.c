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
       dir != FAX_SESSION_DIR_IN ||
       dir != FAX_SESSION_DIR_OUT)
    {
        goto _exit;
    }

    new_session = calloc(1, sizeof(*new_session));
    if(!new_session)
    {
        app_trace(TRACE_ERR, "%s: memory allocation for new session failed",
                  __func__);
        goto _exit;
    }

    new_session->ses_id = session_NextID(dir);
    new_session->sidx = sidx;
    new_session->mode = mode;
    new_session->state = FAX_SESSION_STATE_NULL;
    new_session->FLAG_IN = dir;
    new_session->fds = -1;

    app_trace(TRACE_INFO, "%s: S %04x. Created. Idx:%02d Mode:'%s' Dir:'%s' ",
              __func__, new_session->ses_id,
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
    cfg_t *cfg = app_getCfg();

    app_trace(TRACE_INFO, "%s: Create listener: %s:%u", __func__,
              ip2str(ip, 0), port);

    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl(cfg->local_ip);
    local_addr.sin_port = htons(cfg->local_port);

    if((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        app_trace(TRACE_ERR, "%s: socket() failed: %s",
                  __func__, strerror(errno));
        ret_val = -2; goto _exit;
    }

    fcntl(sock, F_SETFL, O_NONBLOCK);

    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        app_trace(TRACE_ERR, "%s: setsockopt() failed: %s",
                  __func__, strerror(errno));
        ret_val = -3; goto _exit;
    }

    if(bind(sock, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0)
    {
        app_trace(TRACE_ERR, "%s: bind() failed: %s",
                  __func__, strerror(errno));
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
        app_trace(TRACE_INFO, "%s: S %04x. Listener creation failed (%d)",
                  __func__, session->ses_id, fd);
        ret_val = -2; goto _exit;
    }

    session->fds = fd;

_exit:
    return ret_val;
}




