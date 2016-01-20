#include "app.h"
#include "session.h"
#include "msg_proc.h"

#define IP_MAX_LEN 15
#define NET_IFACE "eth0"
#define CTRL_FD_IDX 0

static cfg_t app_config;


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

	length = strlen(str);
	size = sizeof(str) - length - 10;

	sprintf(str, " %s  %s  ", timestamp, p);

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


static int app_InitControlFD()
{
    int ret_val = 0;
    int res;
    session_t *session = NULL;
    cfg_t *cfg = app_getCfg();

    app_trace(TRACE_INFO, "%s: Create control session: %s:%u", __func__,
              ip2str(cfg->local_ip, 0), cfg->local_port);

    session = session_create(FAX_SESSION_MODE_CTRL, CTRL_FD_IDX,
                             FAX_SESSION_DIR_IN);
    if(!session)
    {
        app_trace(TRACE_ERR, "%s: control session creating failed", __func__);
        ret_val = -1; goto _exit;
    }

    res = session_initCtrl(session);
    if(res)
    {
        app_trace(TRACE_ERR, "%s: control session init failed (%d)",
                  __func__, res);
        session_destroy(session);
        ret_val = -2; goto _exit;
    }

    cfg->pfds[CTRL_FD_IDX].fd = session->fds;
    cfg->pfds[CTRL_FD_IDX].events = POLLIN;

    cfg->session_cnt = 1;

    app_trace(TRACE_INFO, "%s: control session created: id:%04x fd = %d",
              __func__, session->ses_id, session->fds);

_exit:
    return ret_val;
}


static int app_cfgInit()
{
	int ret_val = 0, res;
	cfg_t *cfg = app_getCfg();

	app_trace(TRACE_INFO, "%s: init cfg", __func__);

	cfg->local_ip = app_getLocalIP(NET_IFACE);
	cfg->local_port = FAX_CONTROL_PORT;

	cfg->pfds = calloc(FAX_MAX_SESSIONS, sizeof(*(cfg->pfds)));
	if(!cfg->pfds)
	{
		app_trace(TRACE_ERR, "%s: memory allocation for file"
				  " descriptors failed", __func__);
		ret_val = -1; goto _exit;
	}

	cfg->session = calloc(FAX_MAX_SESSIONS, sizeof(*(cfg->session)));
	if(!cfg->session)
	{
		app_trace(TRACE_ERR, "%s: memory allocation for sessions failed",
				  __func__);
		free(cfg->pfds);
		ret_val = -2; goto _exit;
	}

	res = app_InitControlFD();
	if(res)
	{
		app_trace(TRACE_ERR, "%s: creating of control fd failed (%d)",
				  __func__, res);
		free(cfg->pfds);
		free(cfg->session);
		ret_val = -3; goto _exit;
	}

_exit:
	return ret_val;
}


int app_init()
{
	int res = 0;
	int ret_val = 0;

	app_trace(TRACE_INFO, "%s: initializing application", __func__);

	res = app_cfgInit();
	if(res)
	{
		app_trace(TRACE_INFO, "%s: failed to init config (%d)", __func__, res);
		ret_val = -1; goto _exit;
	}

_exit:
	return ret_val;
}


/*============================================================================*/


int app_start()
{

	app_trace(TRACE_INFO, "%s: starting application", __func__);

	return 0;
}


/*============================================================================*/

void app_cfgDestroy()
{
	cfg_t *cfg = app_getCfg();
	int i;

	app_trace(TRACE_INFO, "%s: destroy cfg", __func__);

	for(i = 0; i < cfg->session_cnt; i++)
	{
		session_destroy(cfg->session[i]);
		cfg->session[i] = NULL;
		cfg->pfds->fd = -1;
	}

	free(cfg->pfds);
	free(cfg->session);
}


int app_destroy()
{
	app_trace(TRACE_INFO, "%s: destroing application", __func__);

	app_cfgDestroy();

	return 0;
}


/*============================================================================*/











































