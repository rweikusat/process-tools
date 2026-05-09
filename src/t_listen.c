/*
  t-listen --- TCP communication/ passive
*/

/*  includes */
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "diag.h"
#include "fill_sun.h"

/*  routines */
static void usage(void)
{
    msg("Usage: t-listen [-l] [-n <name>] <port>[@<addr>] [<cmd> <arg>*]");
    msg("    Create a TCP socket listening on <port>. If the optional");
    msg("    @<addr> is present, the socket will be bound to the address");
    msg("    specified by <addr>. Otherwise, the wildcard address will be");
    msg("    used. The accept program will be invoked to handle actual");
    msg("    client connections and any arguments after the first will");
    msg("    be passed to it.");
    msg("    The -l option can be used to request that information about");
    msg("    new connection is logged.");
    msg("    The -n option can be used to specify a non-default name for");
    msg("    that (default <cmd> or 'relay' if <cmd> wasn't provided).");

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
    hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV | AI_ADDRCONFIG;

    rc = getaddrinfo(addr, port, &hints, &ainfo);
    if (rc) {
        err("%s: getaddrinfo: %s(%d)",
            __func__, gai_strerror(rc), rc);
        exit(1);
    }

    if (sep) *sep = '@';
    return ainfo;
}

static int listen_on(char *addr)
{
    struct addrinfo *ainfo;
    int rc, sk;

    ainfo = xlate_addr(addr);

    sk = socket(ainfo->ai_family, ainfo->ai_socktype, ainfo->ai_protocol);
    if (sk == -1) die("socket");

    rc = 1;
    rc = setsockopt(sk, SOL_SOCKET, SO_REUSEADDR, &rc, sizeof(rc));
    if (rc == -1) die("setsockopt");

    rc = bind(sk, ainfo->ai_addr, ainfo->ai_addrlen);
    if (rc == -1) die("bind");

    freeaddrinfo(ainfo);
    return sk;
}

/*  main */
int main(int argc, char **argv)
{
    char **accv, *name, **p, sks[128];
    int c, sk, log;

    init_diag("t-listen");

    log = 0;
    name = NULL;
    while (c = getopt(argc, argv, "+ln:"), c != -1)
        switch (c) {
        case 'l':
            log = 1;
            break;

        case 'n':
            name = optarg;
            break;

        default:
            usage();
        }

    argv += optind;
    if (!*argv) usage();
    sk = listen_on(*argv);

    ++argv;
    p = accv = alloca((7 + argc - 2) * sizeof(*accv));
    *p++ = "accept";

    if (log) {
        *p++ = "-l";

        if (name) {
            *p++ = "-n";
            *p++ = name;
        }
    }

    sprintf(sks, "%d", sk);
    *p++ = sks;
    while (*argv) *p++ = *argv++;
    *p = NULL;

    execvp(*accv, accv);
    die("execvp");

    return 0;
}
