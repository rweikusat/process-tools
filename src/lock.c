/*
  execute a command with certain locks held
*/

/*  includes */
#include <stdlib.h>
#include <unistd.h>

#include "diag.h"

/*  routines */
static void usage(void)
{
    msg("Usage: lock <-r|-w file>* <cmd> <args>*");
    msg("    Execute a command with a certain set of POSIX record locks held.");
    msg("    The -r option denotes a read lock on a file that's to be acquired,");
    msg("    -w a write lock. Both can appear any number of times.");

    exit(1);
}

/*  main */
int main(int argc, char **argv)
{
    int c;

    init_diag("lock");
    while (c = getopt(argc, argv, "r:w:"), c != -1)
        switch (c) {
        case 'r':
        case 'w':
            lock(optarg, c);
            break;

        default:
            usage();
        }

    argv += optind;
    if (!*argv) usage();

    execvp(*argv, argv);

    sys_die("execvp");
    return 0;
}
