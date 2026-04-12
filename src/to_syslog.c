/*
  relay output on a set of file descriptors (default: 1 and 2)
  to syslog
*/

/*  includes */
#include <stdlib.h>

#include "diag.h"

/*  types */
struct relay_fd {
    struct relay_fd *p;
    int from, to;
};

/*  variables */
static struct relay_fd def_relay[] = {
    {
        .p = def_relay + 1,
        .to = 1 },
    {
        .to = 2 }
};

/*  routines */
static void usage(void)
{
    msg("Usage: to-syslog [-f <fd>[,<fd>*] <cmd> <arg>*");
    msg("    Execute a command and relay output on certain file descriptors");
    msg("    (default: 1 and 2) to syslog.");
    msg("    The -f option can be used to specify a different set of file");
    msg("    descriptors.");
    msg("    Output lines matching the Perl pattern '^\\w\\[0-9+\\]: ' won't be");
    msg("    won't be relayed as it's assumed that they were already sent to");
    msg("    syslog.");

    exit(1);
}

/*  main */
int main(int argc, char **argv)
{
    struct relay_fd *relays;
    int c;

    init_diag("to-syslog");
    relays = def_realy;
    while (c = getopt(argc, argv, "+f"), c != -1)
        switch (c) {
        case 'f':
            relays = parse_fd_lst(optarg);
            break;

        default:
            usage();
        }

    argv += optind;
    if (!*argv) usage();

    return 0;
}
