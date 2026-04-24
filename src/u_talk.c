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
    msg("Usage: u-talk [-g <group>] [-p] <socket>");
    msg("    Communicate using AF_UNIX stream sockets.");
    msg("    Connect to the socket specified by <socket> and relay data");
    msg("    back and forth. If socket starts with '//', it refers to a ");
    msg("    socket in the Linux abstract namespace for AF_UNIX sockets.");
    msg("    The -p option can be used to demand a passive open, that is");
    msg("    creation of a server socket instead. Any number of clients");
    msg("    are supported. Unless -g is also provided, the socket will be");
    msg("    created with 0600 access permissions.");
    msg("    When creating a server socket in the filesystem namespace, ");
    msg("    the -g option can be used to specify a particular group for ");
    msg("    it. In this case, the permissions will be 0660.");

    exit(1);
}

static void init(int argc, char **argv)
{
    int c;

    while (c = getopt(argc, argv, "+g:p"), c != -1)
        switch (c) {
        case 'g':
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
