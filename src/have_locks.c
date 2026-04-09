/*
  check if a certain set of locks is held in the current "process
  scope"
*/

/*  includes */
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "diag.h"

/*  types */
struct ppid {
    struct ppid *p;
    pid_t pid;
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

static char *read_proc_file(char *path)
{
    char *s, *p, *e, *tmp;
    size_t have, want;
    ssize_t nr;
    int fd;

    fd = open(path, O_RDONLY, 0);
    if (fd == -1) {
        err("%s: open %s: %m(%d)", __func__, path, errno);
        exit(1);
    }

    p = s = malloc(128);
    e = s + 128;
    while (nr = read(fd, p, e - p), nr > 0) {
        p += nr;

        if (p == e) {
            have = e - s;
            want = have * 2;
            tmp = realloc(s, want);
            if (!tmp) die("realloc");

            e = tmp + want;
            p = tmp + have + 1;
            s = tmp;
        }
    }
    if (nr == -1) die("read");

    *p = 0;
    close(fd);
    return s;
}

static pid_t ppid_for(pid_t pid)
{
    char status_name[128];
    char *status;

    sprintf(status_name, "/proc/%ld/status", (long)pid);
    status = read_proc_file(status_name);
    return 0;
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
