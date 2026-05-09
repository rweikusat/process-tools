/*
  t-talk --- TCP communication/ active
*/

/*  includes */
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "diag.h"

/*  routines */
static void usage(void)
{
    msg("Usage: t-talk <port>@<addr> [<cmd> <arg>*]");
    msg("    Establish a TCP connection to <port> at <addr>. When the");
    msg("    optional <cmd> argument is passed, the specified,  <cmd>");
    msg("    will be executed with stdin, stdout and stderr referring to the");
    msg("    connected socket. Otherwise, data will be relayed between stdin");
    msg("    and stdout of the process and the connected socket.");

    exit(1);
}

static struct addrinfo *xlate_addr(char *addr)
{
    char *sep, *port;
    struct addrinfo hints, *ainfo;
    int rc;

    port = addr;
    sep = strchr(addr, '@');
    if (sep) {
        *sep = 0;
        addr = sep + 1;
    } else
        addr = NULL;

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    hints.ai_flags = AI_NUMERICSERV | AI_ADDRCONFIG;

    rc = getaddrinfo(addr, port, &hints, &ainfo);
    if (rc) {
        err("%s: getaddrinfo: %s(%d)",
            __func__, gai_strerror(rc), rc);
        exit(1);
    }

    if (sep) *sep = '@';
    return ainfo;
}

static int connect_to(char *addr)
{
    struct addrinfo *ainfo;
    int rc, sk;

    ainfo = xlate_addr(addr);

    sk = socket(ainfo->ai_family, ainfo->ai_socktype, ainfo->ai_protocol);
    if (sk == -1) die("socket");

    rc = connect(sk, ainfo->ai_addr, ainfo->ai_addrlen);
    if (rc == -1) die("connect");

    return sk;
}

static void exec_cmd(int sk, char **argv)
{
    int rc;

    rc = dup2(sk, 0);
    if (rc == -1) die("dup2/0");
    rc = dup2(sk, 1);
    if (rc == -1) die("dup2/1");
    rc = dup2(sk, 2);
    if (rc == -1) die("dup2/2");
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

    init_diag("t-talk");
    if (argc < 2) usage();

    ++argv;
    sk = connect_to(*argv);

    ++argv;
    if (*argv) exec_cmd(sk, argv);
    exec_relay(sk);

    return 0;
}
