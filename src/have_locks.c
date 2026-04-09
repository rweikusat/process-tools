/*
  check if a certain set of locks is held in the current "process
  scope"
*/

/*  includes */
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "diag.h"

/*  macros */
#define PPID "PPid:\t"

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

static char *read_proc_file(char *path, char **end)
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
            p = tmp + have;
            s = tmp;
        }
    }
    if (nr == -1) die("read");

    *end = p;
    close(fd);
    return s;
}

static pid_t ppid_for(pid_t pid)
{
    char status_name[128];
    char *status, *e, *p, *pp, *want;
    unsigned long ppid;
    int c;

    sprintf(status_name, "/proc/%ld/status", (long)pid);
    p = status = read_proc_file(status_name, &e);
    while (p < e) {
        want = PPID;
        while (p < e && (c = *p, c != '\n' && *want == c)) {
            ++want;
            ++p;
        }

        if (p < e){
            if (!*want) {
                pp = p;
                do
                    c = *pp;
                while (c != '\n' && ++pp < e);
                if (c != '\n') {
                err_no_nl:
                    err("%s: missing \\n in %s", __func__, status_name);
                    exit(1);
                }

                *pp = 0;
                errno = 0;
                ppid = strtoul(p, &e, 10);
                if (ppid == ULONG_MAX && errno) die("strtoul");
                if (*e) {
                    err("%s: garbage in ppid", __func__);
                    exit(1);
                }

                free(status);
                return ppid;
            }

            do c = *p; while (c != '\n' && ++p < e);
            if (p == e) goto err_no_nl;
            ++p;
        }
    }

    err("%s: didn't find %s", __func__, PPID);
    exit(1);
    return 0;
}

static struct ppid *get_ppids(void)
{
    struct ppid *first, **chain, *ppid;
    pid_t cur, pid;

    first = NULL;
    chain = &first;
    cur = getpid();

    while (pid = ppid_for(cur), pid) {
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
