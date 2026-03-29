/*
  send commands to a running monitor
*/

/*  includes */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "diag.h"
#include "monitor_ctrl.h"

/*  types */
struct cmd {
    char *name;
    unsigned val;
};

struct the_cmd {
    char *instance;
    unsigned cmd, arg;
};

/*  variables */
static struct cmd cmds[] = {
    {
        .name = "status",
        .val = CMD_STATUS },

    {
        .name = "stop",
        .val = CMD_TERM },

    {
        .name = "restart",
        .val = CMD_RESTART },

    {
        .name = "signal",
        .val = CMD_SIG },

    {
        .name = "rexec",
        .val = CMD_REX },

    {
        .name = NULL }
};

static int quiet;

/*  routines */
static void usage(void)
{
    msg("Usage: monitor-ctrl [-q] <instance> status|terminate|restart|signal|rexec [<arg>]");
    exit(1);
}

static int find_cmd(char *name)
{
    struct cmd *pcmd;
    size_t n_len;
    int cmd;

    n_len = strlen(name);
    pcmd = cmds;
    cmd = -1;
    while (pcmd->name) {
        if (strncmp(name, pcmd->name, n_len) == 0) {
            if (cmd != -1) {
                msg("%s: ambiguous abbreviation", name);
                return -1;
            }

            cmd = pcmd->val;
        }

        ++pcmd;
    }

    if (cmd == -1) msg("%s: unrecognized command", name);
    return cmd;
}

static void parse_args(int argc, char **argv, struct the_cmd *the_cmd)
{
    int c;

    while (c = getopt(argc, argv, "q"), c != -1)
        if (c == 'q')
            quiet = 1;
        else
            usage();

    argv += optind;
    if (!*argv) usage();
    the_cmd->instance = *argv++;

    if (!*argv) usage();
    c = find_cmd(*argv++);
    if (c == -1) usage();
    the_cmd->cmd = c;

    the_cmd->arg = *argv ? atoi(*argv) : 0;
}

/*  main */
int main(int argc, char **argv)
{
    struct the_cmd the_cmd;

    init_diag("monitor-ctrl");
    parse_args(argc, argv, &the_cmd);

    return 0;
}
