/*
  sanitize environment
*/

/*  includes */
#include "diag.h"
#include <unistd.h>

/*  routines */
static void usage(void)
{
    syslog(LOG_NOTICE, "Usage: sane-env [-k <keep>] [-s <set>] <cmd> <arg>*");
    syslog(LOG_NOTICE, "    Both options can appear any number of times. Keep arguments name vars to keep.");
    syslog(LOG_NOTICE, "    Set arguments must be of the form <name>=<value>. Will be added.");
    exit(1);
}

/*  main */
int main(int argc, char **argv)
{
    int c;

    init_diag("sane-env");
    while (c = getopt(argc, argv, "k:s:"), c != -1)
        switch (c) {
        case 'k':
            syslog(LOG_NOTICE, "keep %s", optarg);
            break;

        case 's':
            syslog(LOG_NOTICE, "set %s", optarg);
            break;

        default:
            usage();
        }

    return 0;
}
