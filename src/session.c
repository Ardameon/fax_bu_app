#include "session.h"
#include "msg_proc.h"
#include "fax.h"

#define ERROR_CALL_ID "FAIL"

#define FAX_PROC_THREAD_TIMEOUT 2000 /* usec */

static int session_id_array[SESSION_ID_CNT];

static int ses_id_static_in = 0;
static int ses_id_static_out = SESSION_ID_OUT;

void show_data(uint8_t *data, int len)
{
	char str[2048];
	int i, size;

	sprintf(str, "buffer(%d): ", len);
	size = strlen(str);

	size += strlen(&str[size]);

	for(i = 0; i<len; i++)
	{
		sprintf(&str[size], "%02X.", (uint8_t)data[i]);
		size += strlen(&str[size]);
	}

	app_trace(TRACE_INFO, "%s", str);
}

/*============================================================================*/

static int session_sendMsg(const session_t *session, uint8_t *msgbuf,
                           int msglen);
static int session_recvMsg(session_t *session, uint8_t *msgbuf,
                           int msglen);

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

/*============================================================================*/

static const char *msg2str(const uint8_t *buf)
{
    static char str[2][MSG_BUF_LEN];
    static int i = 0;
    int j;
    char *c = (char *)buf;

    i %= 2;

    for(j = 0; *c != '\r' && *c != '\n' && *c != '\0'; j++, c++)
    {
        if(*c >= ' ' && *c < '~')
            str[i][j] = *c;
        else
            str[i][j] = '.';
    }

    str[i][j] = '\0';

    return str[i++];
}

/*============================================================================*/

static int session_idGetNext(int in)
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
                ses_id_static_out = i + 1;
                ret_val = i;
                goto _exit;
            }
        }

        for(i = SESSION_ID_OUT; i < ses_id_static_out; i++)
        {
            if(session_id_array[i] == 0)
            {
                session_id_array[i] = 1;
                ses_id_static_out = i + 1;
                ret_val = i;
                goto _exit;
            }
        }
    }

_exit:
    return ret_val;
}

/*============================================================================*/

static int session_idRelease(int id)
{
    int ret_val = 0;

    if(id < SESSION_ID_IN || id >= SESSION_ID_IN + SESSION_ID_CNT)
    {
        ret_val = -1; goto _exit;
    }

    if(!session_id_array[id])
    {
        ret_val = -2; goto _exit;
    }

    session_id_array[id] = 0;

_exit:
    return ret_val;
}

/*============================================================================*/

session_t *session_create(session_mode_e mode, int sidx, session_dir_e dir)
{
    session_t *new_session = NULL;

    if(sidx < 0 ||
       mode == FAX_SESSION_MODE_UNKNOWN ||
       mode > FAX_SESSION_MODE_TERMINAL ||
       dir > FAX_SESSION_DIR_IN)
    {
        goto _exit;
    }

    new_session = calloc(1, sizeof(*new_session));
    if(!new_session)
    {
        app_trace(TRACE_ERR, "Session. Memory allocation "
                  "for new session failed");
        goto _exit;
    }

    new_session->ses_id = session_idGetNext(dir);
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

/*============================================================================*/

void session_destroy(session_t *session)
{
    if(!session) return;

    if(session->mode != FAX_SESSION_MODE_CTRL) fax_sessionDestroy(session);

    if(session->FLAG_THREAD_ACTIVE) pthread_cancel(session->fax_proc_thread);

    session_idRelease(session->ses_id);

    app_portRelease(session->loc_port);

    if(session->fds > 0) close(session->fds);

    app_trace(TRACE_INFO, "Session %04x. Destroyed", session->ses_id);

    free(session);
}

/*============================================================================*/

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

/*============================================================================*/

static int get_hostAddr(char *ip, char *port, struct sockaddr_in *sa)
{
    int n;
    struct addrinfo hints, *res;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_socktype = SOCK_DGRAM;

    n = getaddrinfo(ip, port, &hints, &res);
    if (n == 0) {
        memcpy(sa, res->ai_addr, res->ai_addrlen);
        freeaddrinfo(res);
    }

    return n;
}

/*============================================================================*/

int session_init(session_t *session, const char *call_id, uint32_t remote_ip,
                 uint16_t remote_port)
{
    int ret_val = 0;
    int fd, res;
    cfg_t *cfg = app_getCfg();
    struct in_addr addr;
    char port_str[16];

    if(!session)
    {
        ret_val = -1; goto _exit;
    }

    session->loc_ip   = cfg->local_ip;
    session->loc_port = app_portGetFree();

    addr.s_addr = htonl(remote_ip);
    sprintf(port_str, "%u", remote_port);

    session->rem_ip   = remote_ip;
    session->rem_port = remote_port;

    strcpy(session->call_id, call_id);

    res = get_hostAddr(inet_ntoa(addr), port_str, &session->remaddr);
    if(res)
    {
        app_trace(TRACE_ERR, "Session %04x. Getting remote address failed",
                  session->ses_id);
        ret_val = res + 100; goto _exit;
    }

    fd = session_createListener(session->loc_ip, session->loc_port);
    if(fd <= 0)
    {
        app_trace(TRACE_ERR, "Session %04x. Listener creation failed (%d)",
                  session->ses_id, fd);
        ret_val = -2; goto _exit;
    }

    res = fax_sessionInit(session, &session_sendMsg);
    if(res)
    {
        app_trace(TRACE_ERR, "Session %04x. FAX session init failed (%d)",
                  session->ses_id, res);
        ret_val = -3; goto _exit;
    }

    app_trace(TRACE_INFO, "Session %04x. Inited successfully: Call '%s' "
              "dir: '%3s' mode: '%s' [%s:%u] <==> [%s:%u]",
              session->ses_id, session->call_id,
              session->FLAG_IN == FAX_SESSION_DIR_IN ? "IN" : "OUT",
              session_modeStr(session->mode),
              ip2str(session->loc_ip, 0), session->loc_port,
              ip2str(session->rem_ip, 1), session->rem_port);

    session->fds = fd;

_exit:
    return ret_val;
}

/*============================================================================*/

int session_initCtrl(session_t *session)
{
    int ret_val = 0;
    int fd;
    cfg_t *cfg = app_getCfg();

    if(!session)
    {
        ret_val = -1; goto _exit;
    }

    session->loc_ip = cfg->local_ip;
    session->loc_port = cfg->local_port;

    fd = session_createListener(session->loc_ip, session->loc_port);
    if(fd <= 0)
    {
        app_trace(TRACE_ERR, "Session %04x. Listener creation failed (%d)",
                  session->ses_id, fd);
        ret_val = -2; goto _exit;
    }

    session->fds = fd;

_exit:
    return ret_val;
}

/*============================================================================*/

static int session_recvMsg(session_t *session, uint8_t *msgbuf,
                           int msglen)
{
    int ret_val = 0;
    struct sockaddr_in sa;
    socklen_t sa_len = sizeof(sa);

    if(!session || !msgbuf)
    {
        ret_val = -2; goto _exit;
    }

    ret_val = recvfrom(session->fds, msgbuf, msglen, 0,
                       (struct sockaddr *)(&sa), &sa_len);

//    app_trace(TRACE_INFO, "Session %04x. RX buffer len: %d",
//              session->ses_id, ret_val);

    if(session->mode == FAX_SESSION_MODE_CTRL)
    {
        memcpy((void *)(&session->remaddr), &sa, sizeof(session->remaddr));
        session->rem_ip = ntohl(sa.sin_addr.s_addr);
        session->rem_port = ntohs(sa.sin_port);
    }

_exit:
    return ret_val;
}

/*============================================================================*/

static int session_sendMsg(const session_t *session, uint8_t *msgbuf,
                           int msglen)
{
    int ret_val = 0;

    if(!session || !msgbuf)
    {
        ret_val = -2; goto _exit;
    }

    ret_val = sendto(session->fds, msgbuf, msglen, 0,
                     (struct sockaddr *)(&session->remaddr),
                     sizeof(session->remaddr));

//    app_trace(TRACE_INFO, "Session %04x. TX buffer len: %d",
//              session->ses_id, ret_val);

_exit:
    return ret_val;
}

/*============================================================================*/

int session_proc(session_t *session)
{
    uint8_t udptl_buf[MSG_BUF_LEN];
    int res, len;
    int ret_val = 0;

    res = session_recvMsg(session, udptl_buf, MSG_BUF_LEN);
    if(res < 0)
    {
        app_trace(TRACE_ERR, "Session %04x. recvMsg() error (%d) %s",
                  session->ses_id, res,
                  res == -1 ? strerror(errno) : "");
        ret_val = -1; goto _exit;
    }

    len = res;

    res = fax_rxUDPTL(session, udptl_buf, len);
    if(res < 0)
    {
        ret_val = -2; goto _exit;
    }

_exit:
    return ret_val;
}

/*============================================================================*/

int session_procFax(session_t *session)
{
    int len;
    uint8_t audio_buf[MSG_BUF_LEN * 2] = {0};

    fax_txAUDIO(session, audio_buf, &len);
    fax_rxAUDIO(session->peer_ses, audio_buf, len);

    return 0;
}

/*============================================================================*/

static void *fax_ses_proc_thread_routine(void *arg)
{
	session_t *session = (session_t *)arg;
	int timeout = FAX_PROC_THREAD_TIMEOUT;

	app_trace(TRACE_INFO, "Fax processing thread for call '%s' started (%lu)",
			  session->call_id, pthread_self());

	pthread_detach(pthread_self());

	/* Don't remove this sleep() */
	sleep(2);

	while(1)
	{
		session_procFax(session);
		session_procFax(session->peer_ses);

		usleep(timeout);
	}

	pthread_exit(NULL);

	return NULL;
}

/*============================================================================*/

static session_t *proc_setup(const sig_message_setup_t *message)
{
    cfg_t *cfg = app_getCfg();
    session_t *in_session = NULL;
    session_t *out_session = NULL;
    session_mode_e out_mode;
    int res = 0;

    app_trace(TRACE_INFO, "Processing %s message call '%s'",
              sig_msgTypeStr(message->msg.type), message->msg.call_id);

    if(cfg->session_cnt >= FAX_MAX_SESSIONS)
    {
        app_trace(TRACE_INFO, "Maximum session count is reached. Reject setup");
        goto _exit;
    }

    switch(message->mode)
    {
        case FAX_MODE_GW_GW:   out_mode = FAX_SESSION_MODE_GATEWAY;  break;
        case FAX_MODE_GW_TERM: out_mode = FAX_SESSION_MODE_TERMINAL; break;
        default:
            app_trace(TRACE_ERR, "Unknown fax mode in %s message (%d)"
                      " for call '%s'",
                      sig_msgTypeStr(message->msg.type), message->mode,
                      message->msg.call_id);
            goto _exit;
    }

    /* Create input session (input leg) */
    in_session = session_create(FAX_SESSION_MODE_GATEWAY, cfg->session_cnt,
                                FAX_SESSION_DIR_IN);
    if(!in_session)
    {
        app_trace(TRACE_ERR, "Creation of IN session for call '%s' failed",
                  message->msg.call_id);
        goto _exit;
    }

    /* Init input session */
    res = session_init(in_session, message->msg.call_id,
                       message->src_ip, message->src_port);
    if(res)
    {
        app_trace(TRACE_ERR, "Initing IN session for call '%s' failed (%d)",
                  message->msg.call_id, res);
        session_destroy(in_session);
        goto _exit;
    }

    /* Create output session (output leg) */
    out_session = session_create(out_mode, cfg->session_cnt + 1,
                                 FAX_SESSION_DIR_OUT);
    if(!out_session)
    {
        app_trace(TRACE_ERR, "Creation of OUT session for call '%s' failed",
                  message->msg.call_id);
        session_destroy(in_session);
        in_session = NULL;
        goto _exit;
    }

    /* Init output session */
    res = session_init(out_session, message->msg.call_id,
                       message->dst_ip, message->dst_port);
    if(res)
    {
        app_trace(TRACE_ERR, "Initing OUT session for call '%s' failed (%d)",
                  message->msg.call_id, res);
        session_destroy(out_session);
        session_destroy(in_session);
        in_session = NULL;
        goto _exit;
    }

    /* Start fax processing thread  */
    res = pthread_create(&in_session->fax_proc_thread, NULL,
                         fax_ses_proc_thread_routine, in_session);

    if(res)
    {
        app_trace(TRACE_ERR, "Creating fax processing thread "
                  "for call '%s' failed (%d)",
                  message->msg.call_id, res);
        session_destroy(out_session);
        session_destroy(in_session);
        in_session = NULL;
        goto _exit;
    }

    in_session->FLAG_THREAD_ACTIVE = 1;

    in_session->peer_ses = out_session;
    out_session->peer_ses = in_session;

    /* Save IN session info */
    cfg->session[cfg->session_cnt] = in_session;
    cfg->pfds[cfg->session_cnt].fd = in_session->fds;
    cfg->pfds[cfg->session_cnt++].events = POLLIN;

    /* Save OUT session info */
    cfg->session[cfg->session_cnt] = out_session;
    cfg->pfds[cfg->session_cnt].fd = out_session->fds;
    cfg->pfds[cfg->session_cnt++].events = POLLIN;

_exit:
    return in_session;
}

/*============================================================================*/

static sig_message_t *create_setup_answer_msg(const sig_message_t *setup_msg,
                                              session_t *in_session)
{
    sig_message_t *answer_msg = NULL;
    int res = 0;

    if(in_session == NULL)
    {
        /* Processing failed */
        res = sig_msgCreateError(setup_msg->call_id, FAX_ERROR_INTERNAL,
                               (sig_message_error_t **)(&answer_msg));
        if(res < 0)
        {
            app_trace(TRACE_ERR, "Setup answer. ERROR msg creating failed (%d)",
                      res);
            goto _exit;
        }
    } else {
        /* Processed successfully */
        res = sig_msgCreateOk(setup_msg->call_id, in_session->loc_ip,
                               in_session->loc_port,
                               (sig_message_ok_t **)(&answer_msg));
        if(res < 0)
        {
            app_trace(TRACE_ERR, "Setup answer. OK msg creating failed (%d)",
                      res);
            goto _exit;
        }
    }

_exit:
    return answer_msg;
}

/*============================================================================*/

static int proc_release(const sig_message_rel_t *message)
{
    int ret_val = 0;
    cfg_t *cfg = app_getCfg();
    session_t *cs;
    int i;

    app_trace(TRACE_INFO, "Processing %s message call '%s'",
              sig_msgTypeStr(message->msg.type), message->msg.call_id);

    for(i = FAX_CTRL_FD_IDX + 1; i < cfg->session_cnt; i++)
    {
        if((cs = cfg->session[i]) == NULL) continue;

        if(!strcmp(message->msg.call_id, cs->call_id))
        {
            cfg->session[i] = NULL;
            cfg->pfds[i].fd = -1;

            cfg->session[cs->peer_ses->sidx] = NULL;
            cfg->pfds[cs->peer_ses->sidx].fd = -1;

            session_destroy(cs->peer_ses);
            session_destroy(cs);

            break;
        }
    }

    if(i >= cfg->session_cnt)
    {
        app_trace(TRACE_INFO, "Processing %s message: call '%s' not found!",
                  sig_msgTypeStr(message->msg.type), message->msg.call_id);
    }

    return ret_val;
}

/*============================================================================*/

static int process_sig_message(const sig_message_t *received_msg,
                                      sig_message_t **answer_msg)
{
    session_t *in_session = NULL;
    int ret_val = 0;

    switch(received_msg->type)
    {
        case FAX_MSG_SETUP:
            in_session = proc_setup((sig_message_setup_t *)received_msg);
            *answer_msg = create_setup_answer_msg(received_msg, in_session);
            if(!in_session) ret_val = -1;
            break;

        case FAX_MSG_RELEASE:
            proc_release((sig_message_rel_t *)received_msg);
            break;

        default:
            app_trace(TRACE_INFO, "Message '%s' can't be handeled."
                      " Only %s allowed",
                      sig_msgTypeStr(received_msg->type),
                      sig_msgTypeStr(FAX_MSG_SETUP));
            break;
    }

    return ret_val;
}

/*============================================================================*/

int session_procCMD(session_t *ctrl_session)
{
    uint8_t buf_recv[MSG_BUF_LEN];
    uint8_t buf_send[MSG_BUF_LEN];
    int buf_len;
    char msg_str[512];
    int res, ret_val = 0;
    sig_message_t *message_recv = NULL;
    sig_message_t *message_send = NULL;

    /* Receive signaling message */
    res = session_recvMsg(ctrl_session, buf_recv, MSG_BUF_LEN);
    if(res < 0)
    {
        app_trace(TRACE_ERR, "Session %04x. Message receiving error (%d) %s",
                  ctrl_session->ses_id, res,
                  (res == -1) ? strerror(errno) : "");
        ret_val = -1; goto _exit;
    }

    app_trace(TRACE_INFO, "SIG MSG RX .......... FROM %s:%u '%s' (%d)",
              ip2str(ctrl_session->rem_ip, 0), ctrl_session->rem_port,
              msg2str(buf_recv), res);

    /* Parse it */
    res = sig_msgParse((char *)buf_recv, &message_recv);
    if(res < 0)
    {
        /* Parsing error - send ERROR message */
        app_trace(TRACE_ERR, "Session %04x. Message parsing error (%d)",
                  ctrl_session->ses_id, res);
        ret_val = -2;

        sig_msgCreateError(ERROR_CALL_ID, FAX_ERROR_INVALID_MESSAGE,
                           (sig_message_error_t **)(&message_send));
        goto _send_answer_msg;
    }

    sig_msgPrint(message_recv, msg_str, sizeof(msg_str));
    app_trace(TRACE_INFO, "Session %04x. Message received: %s",
              ctrl_session->ses_id, msg_str);

    /* Process received message */
    res = process_sig_message(message_recv, &message_send);

    if(res)
    {
        /* Processing failed */
        app_trace(TRACE_ERR, "Session %04x. Processing CMD %s "
                  "(call_id: '%s') failed (%d)",
                  ctrl_session->ses_id, sig_msgTypeStr(message_recv->type),
                  message_recv->call_id, res);
        ret_val = -3;
    }

_send_answer_msg:
    /* If answer message not created - no need to send it */
    if(message_send == NULL) goto _exit_1;

    sig_msgPrint(message_send, msg_str, sizeof(msg_str));
    app_trace(TRACE_INFO, "Session %04x. Message to send: %s",
              ctrl_session->ses_id, msg_str);

    buf_len = sig_msgCompose(message_send, (char *)buf_send, sizeof(buf_send));

    /* Send answer */
    res = session_sendMsg(ctrl_session, buf_send, buf_len);
    if(res < 0)
    {
        app_trace(TRACE_ERR, "Session %04x. Message sending error (%d) %s",
                  ctrl_session->ses_id, res,
                  (res == -1) ? strerror(errno) : "");
        ret_val = -4;
    }

    app_trace(TRACE_INFO, "SIG MSG TX .......... TO %s:%u '%s' (%d)",
              ip2str(ctrl_session->rem_ip, 0), ctrl_session->rem_port,
              msg2str(buf_send), res);

    if(message_send) sig_msgDestroy(message_send);

_exit_1:
    if(message_recv) sig_msgDestroy(message_recv);

_exit:
    return ret_val;
}

/*============================================================================*/




