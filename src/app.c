#include "app.h"
#include "session.h"
#include "msg_proc.h"

#define IP_MAX_LEN 15
#define NET_IFACE "eth0"
#define POLL_TIMEOUT 40 /* msec */
#define PORT_START 37000
#define PORT_COUNT 200

static cfg_t app_config;

static uint16_t port_table[PORT_COUNT];
static uint16_t port_idx_static = 0;

uint8_t app_run = 1;

/*============================================================================*/

void app_trace(int level, char *format, ...)
{
	va_list	ap;
	int length, size;
	char str[4096] = {0}, timestamp[100] = {0}, *p;
	struct timeval tp;
	struct tm *pt;
	time_t t;

	gettimeofday(&tp, NULL);
	t = (time_t)tp.tv_sec;
	pt = localtime(&t);
	sprintf(timestamp, " %02d:%02d:%02d.%06lu", pt->tm_hour, pt->tm_min,
			pt->tm_sec, tp.tv_usec);

	switch(level){
		case TRACE_INFO: p="[INFO]";  break;
		case TRACE_ERR:  p="[ERR ]";  break;
		case TRACE_WARN: p="[WARN]";  break;
		case TRACE_DEBUG:p="[DEBG]";  break;
		case -1:         p="[    ]";  break;

		default:	 p="[????]";  break;
	}

	sprintf(str, " %s  %s  ", timestamp, p);

	length = strlen(str);
	size = sizeof(str) - length - 10;

	va_start(ap, format);
	length = vsnprintf(&str[length], size, format, ap);
	va_end(ap);

	size = strlen(str);
	while(size && (str[size-1] == '\n' || str[size-1] == '\r')) size--;

	str[size++] = '\r';
	str[size++] = '\n';
	str[size++] = 0;

	printf("%s", str);
}

/*============================================================================*/

cfg_t *app_getCfg()
{
	return &app_config;
}

/*============================================================================*/

static uint32_t app_getLocalIP(const char *iface)
{
    struct ifreq ifr;

    strcpy(ifr.ifr_name, iface);

    int s = socket(AF_INET, SOCK_DGRAM, 0);
    ioctl(s, SIOCGIFADDR, &ifr);
    close(s);

    struct sockaddr_in *sa = (struct sockaddr_in *)&ifr.ifr_addr;

    return ntohl(sa->sin_addr.s_addr);
}

/*============================================================================*/

void sigint_handler(int sig) {
    if(sig == SIGINT ||
       sig == SIGHUP ||
       sig == SIGTERM)
    {
        app_trace(TRACE_INFO, "App. Signal %d received", sig);
        app_run = 0;
    }
}

/*============================================================================*/

static int app_InitControlFD()
{
    int ret_val = 0;
    int res;
    session_t *session = NULL;
    cfg_t *cfg = app_getCfg();

    app_trace(TRACE_INFO, "App. Create control session: %s:%u",
              ip2str(cfg->local_ip, 0), cfg->local_port);

    session = session_create(FAX_SESSION_MODE_CTRL, FAX_CTRL_FD_IDX,
                             FAX_SESSION_DIR_IN);
    if(!session)
    {
        app_trace(TRACE_ERR, "App. Control session creating failed");
        ret_val = -1; goto _exit;
    }

    res = session_initCtrl(session);
    if(res)
    {
        app_trace(TRACE_ERR, "App. Control session init failed (%d)",
                  res);
        session_destroy(session);
        ret_val = -2; goto _exit;
    }

    cfg->pfds[FAX_CTRL_FD_IDX].fd = session->fds;
    cfg->pfds[FAX_CTRL_FD_IDX].events = POLLIN;
    cfg->session[FAX_CTRL_FD_IDX] = session;

    cfg->session_cnt = 1;

    app_trace(TRACE_INFO, "App. Control session created: Session %04x fd = %d",
              session->ses_id, session->fds);

_exit:
    return ret_val;
}

/*============================================================================*/

static int app_cfgInit()
{
	int ret_val = 0, res;
	cfg_t *cfg = app_getCfg();

	app_trace(TRACE_INFO, "App. Init cfg");

	cfg->local_ip = app_getLocalIP(NET_IFACE);
	cfg->local_port = FAX_CONTROL_PORT;

	cfg->pfds = calloc(FAX_MAX_SESSIONS, sizeof(*(cfg->pfds)));
	if(!cfg->pfds)
	{
		app_trace(TRACE_ERR, "App. Memory allocation for file"
				  " descriptors failed");
		ret_val = -1; goto _exit;
	}

	cfg->session = calloc(FAX_MAX_SESSIONS, sizeof(*(cfg->session)));
	if(!cfg->session)
	{
		app_trace(TRACE_ERR, "App. Memory allocation for sessions failed");
		free(cfg->pfds);
		ret_val = -2; goto _exit;
	}

	res = app_InitControlFD();
	if(res)
	{
		app_trace(TRACE_ERR, "App. Creating of control fd failed (%d)",
				  res);
		free(cfg->pfds);
		free(cfg->session);
		ret_val = -3; goto _exit;
	}

_exit:
	return ret_val;
}

/*============================================================================*/

int app_init()
{
	int res = 0;
	int ret_val = 0;

	app_trace(TRACE_INFO, "App. Initializing application");

	signal(SIGINT, &sigint_handler);

	res = app_cfgInit();
	if(res)
	{
		app_trace(TRACE_INFO, "App. Failed to init config (%d)", res);
		ret_val = -1; goto _exit;
	}

_exit:
	return ret_val;
}

/*============================================================================*/

static int app_procSessions()
{
	cfg_t *cfg = app_getCfg();
	int i, skip_sessions;
	struct pollfd *pfds = cfg->pfds;
	int session_cnt = cfg->session_cnt;
	session_t **session = cfg->session;

	for(i = FAX_CTRL_FD_IDX + 1, skip_sessions = 0; i < session_cnt; i++)
	{
		if(pfds[i].fd == -1)
		{
			skip_sessions++;
			continue;
		}

		/* remove destroyed sessions */
		if(skip_sessions > 0)
		{
			session[i - skip_sessions] = session[i];
			pfds[i - skip_sessions] = pfds[i];
			session[i]->sidx = i - skip_sessions;
		}

		if(pfds[i].revents & POLLIN)
		{
			session_proc(session[i]);
		}
	}

	cfg->session_cnt -= skip_sessions;

	return 0;
}

/*============================================================================*/

static int app_procCMD()
{
	cfg_t *cfg = app_getCfg();

	return (cfg->pfds[FAX_CTRL_FD_IDX].revents & POLLIN) ?
				session_procCMD(cfg->session[FAX_CTRL_FD_IDX]) : -1;
}

/*============================================================================*/

int app_start()
{
	cfg_t *cfg = app_getCfg();
	int poll_res;

	app_trace(TRACE_INFO, "App. Starting application");

	while(app_run)
	{
		poll_res = poll(cfg->pfds, cfg->session_cnt, POLL_TIMEOUT);

		app_procSessions();

		if(poll_res > 0)
		{
			app_procCMD();
		}
	}

	return 0;
}

/*============================================================================*/

void app_cfgDestroy()
{
	cfg_t *cfg = app_getCfg();
	int i;

	app_trace(TRACE_INFO, "App. Destroy cfg");

	for(i = 0; i < cfg->session_cnt; i++)
	{
		session_destroy(cfg->session[i]);
		cfg->session[i] = NULL;
		cfg->pfds[i].fd = -1;
	}

	free(cfg->pfds);
	free(cfg->session);
}

/*============================================================================*/

int app_destroy()
{
	app_trace(TRACE_INFO, "App. Destroing application");

	app_cfgDestroy();

	return 0;
}

/*============================================================================*/

int app_portGetFree()
{
	int i, ret_val = -1;

	for(i = port_idx_static; i < PORT_COUNT; i++)
	{
		if(!port_table[i])
		{
			port_idx_static = i + 1;
			port_table[i] = 1;
			ret_val = i + PORT_START;
			goto _exit;
		}
	}

	for(i = 0; i < port_idx_static; i++)
	{
		if(!port_table[i])
		{
			port_idx_static = i + 1;
			port_table[i] = 1;
			ret_val = i + PORT_START;
			goto _exit;
		}
	}

_exit:

	return ret_val;
}

/*============================================================================*/

int app_portRelease(uint16_t port)
{
	int ret_val = 0;

	if(port < PORT_START || port >= PORT_START + PORT_COUNT)
	{
		ret_val = -1;
		goto _exit;
	}

	if(!port_table[port - PORT_START])
	{
		ret_val = -2;
		goto _exit;
	}

	port_table[port - PORT_START] = 0;


_exit:
	return ret_val;
}

/*============================================================================*/









































