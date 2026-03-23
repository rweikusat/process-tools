/*
  monitor program
*/

/*  includes */
#include "diag.h"

/*  routines */
static void usage(void)
{
    msg("Usage: monitor [-g <ctrl socket group>] "
        "[-n <name>] "
        "[-p <term grace period>] "
        "[-t <termsig>]");
    exit(1);
}

/*  main */
int main(int argc, char **argv)
{
    init_diag("monitor");
    if (argc < 2) usage();

    return 0;
}
