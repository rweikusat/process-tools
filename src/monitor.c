/*
  monitor program
*/

/*  includes */
#include <signal.h>

#include "diag.h"

/*  constants */
enum {
    DEF_GRACE = 20`             /* seconds */
};

/*  variables */
static char *name, **cmdv;
static unsigned grace = DEF_GRACE;
static int term_sig = SIGTERM;

/*  routines */
static void usage(void)
{
    msg("Usage: monitor [-g <ctrl socket group>] "
        "[-n <name>] "
        "[-p <term grace period>] "
        "[-t <termsig>]");
    exit(1);
}

static void init(int argc, char **argv)
{
    char *ctrl_grp;
    int c;

    ctrl_grp = NULL;
    while (c = getopt(argc, argv, "g:n:p:t:"), c != -1)
        switch (c) {
        case 'g':
            ctrl_grp = optarg;
            break;

        case 'n':
            name = optarg;
            break;

        case 'p':
            grace = atoi(optarg);
            break;

        case 't':
            term_sig = atoi(optarg);
            break;

        default:
            usage();
        }

    argv += optind;
    if (!*argv) usage();
    cmdv = argv;

    if (!name) name = *cmdv;
    create_ctrl(name, ctrl_grp);
}

/*  main */
int main(int argc, char **argv)
{
    init_diag("monitor");

    init(argc, argv);

    return 0;
}
