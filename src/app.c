#include "app.h"
#include "session.h"
#include "msg_proc.h"

#define IP_MAX_LEN 15
#define NET_IFACE "eth0"

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
    int sock = -1;
    struct sockaddr_in local_addr;
    int reuse = 1;
    cfg_t *cfg = app_getCfg();

    app_trace(TRACE_INFO, "%s: Create control listener: %s:%u", __func__,
              ip2str(cfg->local_ip, 0), cfg->local_port);

    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl(cfg->local_ip);
    local_addr.sin_port = htons(cfg->local_port);

    if((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        app_trace(TRACE_ERR, "%s: socket() failed: %s",
                  __func__, strerror(errno));
        ret_val = -1; goto _exit;
    }

    fcntl(sock, F_SETFL, O_NONBLOCK);

    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        app_trace(TRACE_ERR, "%s: setsockopt() failed: %s",
                  __func__, strerror(errno));
        ret_val = -2; goto _exit;
    }

    if(bind(sock, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0)
    {
        app_trace(TRACE_ERR, "%s: bind() failed: %s",
                  __func__, strerror(errno));
        ret_val = -3; goto _exit;
    }

    cfg->pfds[0].fd = sock;
    cfg->pfds[0].events = POLLIN;

    cfg->session[0] = calloc(1, sizeof(**(cfg->session)));
    cfg->session[0]->mode = FAX_SESSION_MODE_CTRL;
    cfg->session_cnt = 1;

    app_trace(TRACE_INFO, "%s: Control descriptor created: fd = %d",
              __func__, sock);

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
		if(cfg->pfds[i].fd != -1) close(cfg->pfds[i].fd);
		if(cfg->session[i] != NULL) free(cfg->session[i]);
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











































