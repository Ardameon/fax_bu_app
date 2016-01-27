#ifndef APP_H
#define APP_H

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <net/if.h>
#include <netdb.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <signal.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#define TRACE_ERR		1
#define TRACE_WARN		2
#define TRACE_INFO		3
#define TRACE_DEBUG		4
#define TRACE_OFF		0

#define FAX_MAX_SESSIONS 80

#define FAX_CONTROL_PORT 23232

struct session_t;

typedef struct cfg_t {
    uint32_t local_ip;
    uint16_t local_port;     /* port for control fd */

    struct pollfd *pfds;     /* poll descriptions array (for sockets) */
    struct session_t **session;

    uint8_t session_cnt;
} cfg_t;

void app_trace(int level, char *format, ...) __attribute__ ((format(printf, 2, 3)));

int app_init();
int app_start();
int app_destroy();

int app_portGetFree();
int app_portRelease(uint16_t port);

cfg_t *app_getCfg();

#endif
