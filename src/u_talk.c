/*
  u-talk --- AF_UNIX communication/ active
*/

/*  includes */
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "diag.h"
#include "fill_sun.h"

/*  routines */
static void usage(void)
{
    msg("Usage: u-talk [-p] <socket> [<cmd> <arg>*]");
    msg("    Connect to the AF_UNIX socket speciffied by <socket>. If");
    msg("    <socket> starts with '//', connect to a socket in the Linux ");
    msg("    abstract namespace whose name is the remainder of the string.");
    msg("    When the optional <cmd> argument is passed, the specified");
    msg("    command will be executed with stdin and stdout referring to the");
    msg("    connected socket. Otherwise, data will be relayed between stdin");
    msg("    and stdout of the process and the connected socket.");
    msg("    The -p option can be used to request using a SOCK_SEQPACKET");
    msg("    instead of a SOCK_STREAM socket.");

    exit(1);
}

static int connect_to(char *addr, int type)
{
    struct sockaddr_un sun;
    unsigned sun_len;
    int rc, sk;

    sk = socket(AF_UNIX, type, 0);
    if (sk == -1) die("socket");

    fill_sun(addr, &sun, &sun_len);
    rc = connect(sk, (struct sockaddr *)&sun, sun_len);
    if (rc == -1) die("connect");

    return sk;
}

static int init(int argc, char **argv)
{
    int c, type;

    type = SOCK_STREAM;
    while (c = getopt(argc, argv, "+p"), c != -1)
        switch (c) {
        case 'p':
            type = SOCK_SEQPACKET;
            break;

        default:
            usage();
        }

    argv += optind;
    if (!*argv) usage();

    return connect_to(*argv, type);
}

static void exec_cmd(int sk, char **argv)
{
    int rc;

    rc = dup2(sk, 0);
    if (rc == -1) die("dup2/0");
    rc = dup2(sk, 1);
    if (rc == -1) die("dup2/1");
    close(sk);

    execvp(*argv, argv);
    die("execvp");
}

static void exec_relay(int sk)
{
    char sks[128];

    sprintf(sks, "%d", sk);
    execlp("relay", "relay", "0,1", sks, (void *)0);
    die("execlp");
}

/*  main */
int main(int argc, char **argv)
{
    int sk;

    init_diag("u-talk");
    sk = init(argc, argv);

    argv += optind;
    ++argv;
    if (*argv) exec_cmd(sk, argv);
    exec_relay(sk);

    return 0;
}
