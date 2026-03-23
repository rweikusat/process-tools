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
void init_diag(char *name);
void die_(char const *fnc, char const *sysc);
void msg(char *tmpl, ...);

#endif
