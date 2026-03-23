/*
  diagnostics
*/

/*  includes */
#include "diag.h"

/*  routines */
void init_diag(char *name)
{
    openlog(name, LOG_PID | LOG_PERROR, LOG_USER);
}

void die_(char const *fnc, char const *sysc)
{
    err("%s: %s: %m(%d)", fnc, sysc, errno);
    exit(1);
}

static void log_msg(int prio, char *tmpl, va_list val)
{
    vsyslog(prio, tmpl, val);
    va_end(val);
}

void msg(char *tmpl, ...)
{
    va_list val;

    va_start(val, tmpl);
    log_msg(LOG_NOTICE, tmpl, val);
}

void err(char *tmpl, ...)
{
    va_list val;

    va_start(val, tmpl);
    log_msg(LOG_ERR, tmpl, val);
}

void warn(char *tmpl, ...)
{
    va_list val;

    va_start(val, tmpl);
    log_msg(LOG_WARNING, tmpl, val);
}
