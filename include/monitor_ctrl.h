/*
  definitions for monitor control connections
*/
#ifndef proc_tools_monitor_ctrl_h
#define proc_tools_monitor_ctrl_h

/*  macros */
#define DEF_CTRL_PATH		"/run/__monitors__"
#define CTRL_PATH_ENV		"MONITOR_CTRL_PATH"

/*  constants */
enum {
    CMD_STATUS,
    CMD_TERM,
    CMD_RESTART,
    CMD_SIG,
    CMD_REX,

    N_CMDS
};

#endif
