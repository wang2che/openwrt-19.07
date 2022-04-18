#include "QMIThread.h"
#include <sys/time.h>

#if defined(__STDC__)
#include <stdarg.h>
#define __V(x)	x
#else
#include <varargs.h>
#define __V(x)	(va_alist) va_dcl
#define const
#define volatile
#endif
#include <sys/types.h>
#include <syslog.h>

static void setTimespecRelative(struct timespec *p_ts, long long msec)
{
    struct timeval tv;
    const int NS_PER_S = 1000 * 1000 * 1000;
    gettimeofday(&tv, (struct timezone *) NULL);

    /* what's really funny about this is that I know
       pthread_cond_timedwait just turns around and makes this
       a relative time again */
    p_ts->tv_sec = tv.tv_sec + (msec / 1000);
    p_ts->tv_nsec = (tv.tv_usec + (msec % 1000) * 1000L ) * 1000L;
    /* assuming tv.tv_usec < 10^6 */
    if (p_ts->tv_nsec >= NS_PER_S) {
        p_ts->tv_sec++;
        p_ts->tv_nsec -= NS_PER_S;
    }
}

int pthread_cond_timeout_np(pthread_cond_t *cond,
                            pthread_mutex_t * mutex,
                            unsigned msecs) {
    if (msecs != 0) {
        struct timespec ts;
        setTimespecRelative(&ts, msecs);
        return pthread_cond_timedwait(cond, mutex, &ts);
    } else {
        return pthread_cond_wait(cond, mutex);
    }
}

static const char * get_time(void) {
    static char time_buf[50];
    struct timeval  tv;
    time_t time;
    suseconds_t millitm;
    struct tm *ti;

    gettimeofday (&tv, NULL);

    time= tv.tv_sec;
    millitm = (tv.tv_usec + 500) / 1000;

    if (millitm == 1000) {
        ++time;
        millitm = 0;
    }

    ti = localtime(&time);
    sprintf(time_buf, "[%02d-%02d_%02d:%02d:%02d:%03d]", ti->tm_mon+1, ti->tm_mday, ti->tm_hour, ti->tm_min, ti->tm_sec, (int)millitm);
    return time_buf;
}


#if 0
static char line[1024];
FILE *logfilefp = NULL;
static pthread_mutex_t printfMutex = PTHREAD_MUTEX_INITIALIZER;
void dbg_time (const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    pthread_mutex_lock(&printfMutex);
    snprintf(line, sizeof(line), "%s ", get_time());
    vsnprintf(line + strlen(line), sizeof(line) - strlen(line), fmt, args);
    fprintf(stdout, "%s\n", line);
    if (logfilefp) {
        fprintf(logfilefp, "%s\n", line);
    }
    pthread_mutex_unlock(&printfMutex);
}
#else
FILE *logfilefp = NULL;
//static pthread_mutex_t printfMutex = PTHREAD_MUTEX_INITIALIZER;
void dbg_time (const char *fmt, ...) 
{
    va_list ptr;
    char buf[1024] = {'\0'};
    va_start(ptr, fmt);
    vsprintf(buf, fmt, ptr);
    va_end(ptr);
    //pthread_mutex_lock(&printfMutex);
    syslog(LOG_INFO, "%s", buf);
    //pthread_mutex_unlock(&printfMutex);
}
#endif

const int i = 1;
#define is_bigendian() ( (*(char*)&i) == 0 )

USHORT le16_to_cpu(USHORT v16) {
    USHORT tmp = v16;
    if (is_bigendian()) {
        unsigned char *s = (unsigned char *)(&v16);
        unsigned char *d = (unsigned char *)(&tmp);
        d[0] = s[1];
        d[1] = s[0];
    }
    return tmp;
}

UINT  le32_to_cpu (UINT v32) {
    UINT tmp = v32;
    if (is_bigendian()) {
        unsigned char *s = (unsigned char *)(&v32);
        unsigned char *d = (unsigned char *)(&tmp);
        d[0] = s[3];
        d[1] = s[2];
        d[2] = s[1];
        d[3] = s[0];
    }
    return tmp;
}

UINT sc_swap32(UINT v32) {
    UINT tmp = v32;
    {
        unsigned char *s = (unsigned char *)(&v32);
        unsigned char *d = (unsigned char *)(&tmp);
        d[0] = s[3];
        d[1] = s[2];
        d[2] = s[1];
        d[3] = s[0];
    }
    return tmp;
}

USHORT cpu_to_le16(USHORT v16) {
    USHORT tmp = v16;
    if (is_bigendian()) {
        unsigned char *s = (unsigned char *)(&v16);
        unsigned char *d = (unsigned char *)(&tmp);
        d[0] = s[1];
        d[1] = s[0];
    }
    return tmp;
}

UINT cpu_to_le32 (UINT v32) {
    UINT tmp = v32;
    if (is_bigendian()) {
        unsigned char *s = (unsigned char *)(&v32);
        unsigned char *d = (unsigned char *)(&tmp);
        d[0] = s[3];
        d[1] = s[2];
        d[2] = s[1];
        d[3] = s[0];
    }
    return tmp;
}
