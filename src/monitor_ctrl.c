/*
  send commands to a running monitor
*/

/*  includes */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
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
    msg("Usage: monitor-ctrl [-q] <instance> status|stop|restart|signal|rexec [<arg>]");
    msg("    Manage a monitored or a monitor process named <instance>.");
    msg("    The status, stop, restart and signal commands perform the corresponding");
    msg("    actions on the monitored process. The rexec command cause the monitor");
    msg("    process to re-exec itself to enable in-place updates.");
    msg("    The -q option can be used to suppress success/failure output.");
    msg("    The exit status of the program will reflect success or failure of");
    msg("    the requested operation.");

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

static int connect_to(char *instance)
{
    struct sockaddr_un sun;
    size_t path_len, total;
    char *path;
    int sk, rc;

    path = getenv(CTRL_PATH_ENV);
    if (!path) path = DEF_CTRL_PATH;
    path_len = strlen(path);
    total = path_len + strlen(instance) + 2;
    if (total > sizeof(sun.sun_path)) {
        err("instance path too long (max %zu)", sizeof(sun.sun_path));
        exit(1);
    }
    sun.sun_family = AF_UNIX;
    memcpy(sun.sun_path, path, path_len);
    sun.sun_path[path_len] = '/';
    strcpy(sun.sun_path + path_len + 1, instance);

    sk = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sk == -1) die("socket");

    rc = connect(sk, (struct sockaddr *)&sun,
                 offsetof(struct sockaddr_un, sun_path) + total);
    if (rc == -1) die("connect");

    return sk;
}

static void send_cmd(int sk, unsigned cmd, unsigned arg)
{
    uint8_t cmd_msg[4];
    ssize_t rc;

    *cmd_msg = 1;
    cmd_msg[1] = cmd;
    cmd_msg[2] = arg;
    cmd_msg[3] = arg >> 8;

    rc = write(sk, cmd_msg, sizeof(cmd_msg));
    if (rc == -1) die("write");
}

static unsigned read_reply(int sk)
{
    uint8_t rp[4];
    ssize_t rc;

    rc = read(sk, rp, sizeof(rp));
    if (rc == -1) die("read");

    if (!quiet) msg(*rp ? "OK" : "FAILED");
    return *rp;
}

/*  main */
int main(int argc, char **argv)
{
    struct the_cmd the_cmd;
    int sk, rp;

    init_diag("monitor-ctrl");
    parse_args(argc, argv, &the_cmd);

    sk = connect_to(the_cmd.instance);
    send_cmd(sk, the_cmd.cmd, the_cmd.arg);
    rp = read_reply(sk);

    return rp ? 0 : 1;
}
