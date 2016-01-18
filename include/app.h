#ifndef APP_H
#define APP_H

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

#include <sys/time.h>
#include <sys/types.h>

#define TRACE_ERR		1
#define TRACE_WARN		2
#define TRACE_INFO		3
#define TRACE_DEBUG		4
#define TRACE_OFF		0

void app_trace(int level, char *format, ...) __attribute__ ((format(printf, 2, 3)));

#endif
