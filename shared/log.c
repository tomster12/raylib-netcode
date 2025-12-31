#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

void log_printf(const char *fmt, ...)
{
    struct timespec ts;
    struct tm tm;

    clock_gettime(CLOCK_REALTIME, &ts);
    localtime_r(&ts.tv_sec, &tm);

    printf("[%02d:%02d:%02d.%06ld] ",
               tm.tm_hour,
               tm.tm_min,
               tm.tm_sec,
               ts.tv_nsec / 1000);

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}
