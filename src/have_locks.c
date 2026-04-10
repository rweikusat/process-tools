/*
  check if a certain set of locks is held in the current "process
  scope"
*/

/*  includes */
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "diag.h"

/*  macros */
#define LOCKS	"/proc/locks"
#define PPID	"\nPPid:\t"

/*  types */
struct ppid {
    struct ppid *p;
    pid_t pid;
};

struct file_id {
    dev_t dev;
    ino_t ino;
    char *path;
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

static char *read_file(char *path)
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

    *p = 0;
    close(fd);
    return s;
}

static pid_d atopid(char *p)
{
    char *e;
    pid_t pid;

    errno = 0;
    ppid = strtol(p, &e, 10);
    if (ppid == LONG_MAX && errno) die("strtol");
    if (*e) {
        err("%s: garbage in %s-line: %s",
            __func__, PPID, e);
        exit(1);
    }

    return pid;
}

static pid_t ppid_for(pid_t pid)
{
    char status_name[128];
    char *status, *p, *e;
    long ppid;

    sprintf(status_name, "/proc/%ld/status", (long)pid);
    status = read_file(status_name);

    p = strstr(status, PPID);
    if (!p) {
        err("%s: failed to find %s in %s",
            __func__, PPID, status_name);
        exit(1);
    }

    p += sizeof(PPID) - 1;
    e = strchr(p, '\n');
    if (!e) {
        err("%s: missing \\n in %s-line",
            __func__, PPID);
        exit(1);
    }
    *e = 0;
    ppid = atopid(p);

    free(status);
    return ppid;
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

static void get_f_ids(char **paths, unsigned n, struct file_id *f_ids)
{
    struct stat st;
    unsigned pos;
    int rc;

    pos = 0;
    while (pos < n) {
        rc = stat(paths[pos], &st);
        if (rc == -1) {
            err("%s: stat %s: %m(%d)",
                __func__, paths[pos], errno);
            exit(1);
        }
        f_ids[pos].dev = st.st_dev;
        f_ids[pos].ino = st.st_ino;
        f_ids[pos].path = path[pos];

        ++pos;
    }
}

static char *field_end(char *p)
{
    char *pp;

    pp = strchr(p, ' ');
    if (!pp) {
        err("%s: missing field sep in %s", __func__, LOCKS);
        exit(1);
    }

    return pp;
}

static char *skip_field(char *p)
{
    p = field_end(p);
    while (*p == ' ') ++p;
    return p;
}

static char *skip_fields(char *p, unsigned n)
{
    while (n) {
        p = skip_field(p);
        --n;
    }
}

static void parse_f_id(char *p, struct file_id *f_id)
{
    unsigned maj, min;
    uintmax_t ino;
    int rc;

    rc = sscanf(p, "%02x:%02x:%uj", &maj, &min, &ino);
    if (rc != 3) {
        err("%s: failed to parse %s", __func__, p);
        exit(1);
    }

    f_id->dev = makedev(maj, min);
    f_id->ino = ino;
}

static void scan_locks(struct file_id *f_ids, unsigned n_fids,
                       struct ppid *ppids)
{
    char *locks, *p, *pp;
    struct file_id lock_id;
    pid_t pid;
    int pos;

    p = locks = read_file(LOCKS);

    while (n_fids) {
        p = skip_fields(p, 4);

        pp = field_end(p);
        *pp = 0;
        pid = atopid(p);

        p = pp + 1;
        pp = field_end(p);
        *pp = 0;
        parse_f_id(p, &lock_id);
        pos = search_for_f_id(f_ids, n_fids, &lock_id);
        if (pos != -1) {
            pos = search_for_pid(ppids, pid);
            if (pos == -1) {
                if (verbose) msg("%s: %s not locked", __func__, f_ids[pos].path);
                exit(2);
            }

            if (verbose) msg("%s: %s locked", __func__, f_ids[pos].path);
            f_ids[pos] = f_ids[--n_fids];
        }

        p = strchr(pp + 1, '\n');
        if (!p) {
            err("%s: missing \\n in %s", __func__, LOCKS);
            exit(1);
        }
    }

    free(locks);
}

/*  main */
int main(int argc, char **argv)
{
    struct ppid *ppids;
    struct file_id *f_ids;
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

    argc -= optind;
    f_ids = alloca(sizeof(*f_ids) * argc);
    get_f_ids(argv, argc, f_ids);
    scan_locks(f_ids, argc, ppids);

    return 0;
}
