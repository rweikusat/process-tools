/*
  change uid/gid and run a command
*/

/*  includes */
#include <unistd.h>

#include "diag.h"

/*  routines */
static void usage(void)
{
    syslog(LOG_NOTICE, "Usage: chids [-g <group>] [-u <user>]");
    exit(1);
}

/*  main */
int main(int argc, char **argv)
{
    char *group, *user;
    int c;

    init_diag("chids");

    user = group = NULL;
    while (c = getopt(argc, argv, "u:g:"), c != -1)
        switch (c) {
        case 'g':
            group = optarg;
            break;

        case 'u':
            user = optarg;
            break;

        default:
            usage();
        }

    if (!(user || group)) {
        syslog(LOG_NOTICE, "either -u or -g must be provided");
        exit(1);
    }

    argv += optind;
    if (!*argv) usage();

    return 0;
}
