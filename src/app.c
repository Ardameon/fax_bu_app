#include "app.h"


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
	sprintf(timestamp, " %02d:%02d:%02d.%06lu", pt->tm_hour, pt->tm_min, pt->tm_sec, tp.tv_usec);

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
