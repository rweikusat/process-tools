/*
  relay loggers
*/
#ifndef proc_tools_loggers_h
#define proc_tools_loggers_h

/*  constants */
enum {
    D_QUIET,
    D_INFO,
    D_DEBUG
};

/*  macros */
#define info loggers[D_INFO - 1]
#define debug loggers[D_DEBUG - 1]

/*  variables */
extern void (*loggers[])(char *, ...);

#endif
