/*
  check if a certain set of locks is held in the current "process
  scope"
*/

/*  includes */
#include <stdlib.h>
#include <unistd.h>

#include "diag.h"

/*  types */
struct ppid {
    struct ppid *p;
    pid_t *pid;
};

/*  variables */
static int verbose;

/*  routines */
static void usage(void)
{
    msg("Usage: have-locks [-v] </path/to/file>+");
    msg("    Determines if locks on all files are held somewhere within");
    msg("    the current process scope, that is, the set of all processes");
    msg("    part of the ppid chain starting with the ppid of the current");
    msg("    process and ending with pid 1.");
    msg("    The -v option can be used to request printing of informational");
    msg("    messages.");

    exit(1);
}

static struct ppid *get_ppids(void)
{
    struct ppid *first, **chain, *ppid;
    pid_t cur, pid;

    first = NULL;
    chain = &first;
    cur = getpid();

    while (pid = ppid_for(cur), cur) {
        ppid = *chain = malloc(sizeof(*ppid));
        if (!ppid) die("malloc");
        ppid->p = NULL;
        chain = &ppid->p;

        ppid->pid = pid;
        cur = pid;
    }

    return first;
}

/*  main */
int main(int argc, char **argv)
{
    struct ppid *ppids;
    int c;

    init_diag("have-locks");

    while (c = getopt(argc, argv, "+v"), c != -1)
        switch (c) {
        case 'v':
            verbose = 1;
            break;

        default:
            usage();
        }

    argv += optind;
    if (!*argv) usage();

    ppids = get_ppids();

    return 0;
}
