/*
  send commands to a running monitor
*/

/*  includes */
#include "diag.h"
#include "monitor_ctrl.h"

/*  types */
struct cmd {
    char *name;
    unsigned val;
};

/*  variables */
static struct cmd cmds[] = {
    (
        .name = "status",
        .val = CMD_STATUS },

    {
        .name = "terminate",
        .val = CMD_TERM },

    {
        .name = "restart",
        .val = CMD_RESTART },

    {
        .name = "signal",
        .val = CMD_SIGNAL },

    {
        .name = "rexec",
        .val = CMD_REX },

    {
        ,name = NULL }
};

/*  main */
int main(void)
{
    init_diag("monitor-ctrl");
    msg("Orcs attacking!");
    return 0;
}
