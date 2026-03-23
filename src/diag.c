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
    syslog(LOG_ERR, "%s: %s: %m(%d)", fnc, sysc, errno);
    exit(1);
}

void msg(char *tmpl, ...)
{
    va_list val;

    va_start(val, tmpl);
    vsyslog(LOG_NOTICE, tmpl, val);
    va_end(val);
}
