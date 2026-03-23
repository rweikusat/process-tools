/*
  diagnostics
*/
#ifndef proc_tools_diag_h
#define proc_tools_diag_h

/*  includes */
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <syslog.h>

/*  macros */
#define die(sysc) die_(__func__, sysc)

/*  routines */
static void init_diag(char *name)
{
    openlog(name, LOG_PID | LOG_PERROR, LOG_USER);
}

static void die_(char const *fnc, char const *sysc)
{
    syslog(LOG_ERR, "%s: %s: %m(%d)", fnc, sysc, errno);
    exit(1);
}

static void msg(char *tmpl, ...)
{
    va_list val;

    va_start(val, tmpl);
    vsyslog(LOG_NOTICE, tmpl, val);
    va_end(val);
}

#endif
