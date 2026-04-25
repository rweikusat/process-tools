/*
  relay data between two pairs of
  file descriptors
*/

/*  includes */
#include <stdlib.h>
#include <unistd.h>

#include "diag.h"

/*  constants */
enum {
    DEF_BUF_SZ =	4096
};

/*  variables */
static size_t buf_sz = DEF_BUF_SZ;

/*  routines */
static void usage(void)
{
    msg("Usage: relay [-b <bufsize>] <fdr0>,<fdw0> <fdr1>,<fdw1>");
    msg("   Relay data between two pairs of file descriptors. Data read from");
    msg("   <fdr0> will be written to <fdw1>, data read from <fdr1> to <fdw0>");
    msg("   The -b option can be used to specifiy an non-default input buffer");
    msg("   size. Default is 4096 bytes.");

    exit(0);
}

/*  main */
int main(int argc, char **argv)
{
    int c;

    init_diag("relay");
    while (c = getopt(argc, argv, "+b:"), c != -1)
        switch (c) {
        case 'b':
            buf_sz = atoi(optarg);
            break;

        default:
            usage();
        }

    argv += optind;
    if (!*argv || !argv[1] || argv[2])
        usage();

    return 0;
}
