/*
  u-talk --- AF_UNIX communication
*/

/*  includes */
#include <stdlib.h>
#include <unistd.h>

#include "diag.h"

/*  routines */
static void usage(void)
{
    msg("Usage: u-talk [-b <bufsize>] [-p] <socket>");
    msg("    Communicate using AF_UNIX stream sockets.");
    msg("    Connect to the socket specified by <socket> and relay data");
    msg("    back and forth. If <socket> starts with '//', it refers to a ");
    msg("    socket in the Linux abstract namespace for AF_UNIX sockets.");
    msg("    The -p option can be used to demand a passive open, that is");
    msg("    creation of a server socket instead. This is intended for ");
    msg("    testing and only one client connect is supported. The socket");
    msg("    will be created with 0600 access permissions.");
    msg("    The -b option can be used to use a different I/O buffer size");
    msg("    than the default of 4096.");

    exit(1);
}

static void init(int argc, char **argv)
{
    int c;

    while (c = getopt(argc, argv, "+b:p"), c != -1)
        switch (c) {
        case 'b':
        case 'p':
            break;

        default:
            usage();
        }

    argv += optind;
    if (!*argv) usage();
}

/*  main */
int main(int argc, char **argv)
{
    init_diag("u-talk");
    init(argc, argv);
    return 0;
}
