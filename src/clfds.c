/*
  close all open file descriptors except 0, 1 and 2 plus any
  additional fds asked for via command-line and exec a program
*/

/*  includes */
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>

/*  types */
struct keep  {
    struct keep *p;
    int fd;
};

/*  macros */
#define die(sysc) die_(__func__, sysc)
#define FDS	"/proc/self/fd"

/*  variables */
static struct keep std_keeps[3] = {
    {
        .fd = 0,
        .p = std_keeps + 1},
    {
        .fd = 1,
        .p = std_keeps + 2 },
    {
        .fd = 2 }
};

/*   routines */
static void usage(void)
{
    syslog(LOG_NOTICE, "Usage: clfds [-k <fd>[,<fd>]* <cmd> <args>*");
    exit(1);
}

static void die_(char const *fnc, char *sysc)
{
    syslog(LOG_ERR, "%s: %s: %m(%d)", fnc, sysc, errno);
    exit(1);
}

static inline int c2dg(unsigned c)
{
    c -= '0';
    if (c < 10) return c;
    return -1;
}

static void add_keeps(char *s, struct keep **keeps)
{
    struct keep *next, *k;
    int fd, dg, c;

    next = *keeps;
    fd = -1;
    while (c = *s, c){
        if (c == ',') {
            if (fd >= 0) {
                k = sbrk(sizeof(*k));
                k->fd = fd;
                k->p = next;
                next = k;

                fd = -1;
            }
        } else {
            dg = c2dg(c);
            if (dg == -1) {
                syslog(LOG_NOTICE, "garbage in fd: %c", c);
                exit(1);
            }

            if (fd == -1) fd = 0;
            fd = fd * 10 + dg;
        }

        ++s;
    }

    if (fd != -1) {
        k = sbrk(sizeof(*k));
        k->fd = fd;
        k->p = next;
        next = k;
    }

    *keeps = next;
}

static int in_keeps(int fd, struct keep **keeps)
{
    struct keep **pk, *k;

    pk = keeps;
    while (k = *pk, k) {
        if (k->fd == fd) {
            *pk = k->p;
            return 1;
        }

        pk = &k->p;
    }

    return 0;
}

static void close_fds(struct keep *keeps)
{
    DIR *d;
    struct dirent *d_ent;
    struct keep k;
    int fd;

    d = opendir(FDS);
    if (!d) die("opendir");
    k.fd = dirfd(d);
    k.p = keeps;
    keeps = &k;

    while (d_ent = readdir(d), d_ent) {
        if (*d_ent->d_name == '.') continue;

        fd = atoi(d_ent->d_name);
        if (in_keeps(fd, &keeps)) continue;

        close(fd);
    }

    closedir(d);
}

/*  main */
int main(int argc, char **argv)
{
    struct keep *keeps;
    int c;

    openlog("clfds", LOG_PID | LOG_PERROR, LOG_USER);

    keeps = std_keeps;
    while (c = getopt(argc, argv, "+k:"), c != -1)
        switch (c) {
        case 'k':
            add_keeps(optarg, &keeps);
            break;

        default:
            usage();
        }

    argv += optind;
    if (!*argv) usage();

    close_fds(keeps);
    execvp(*argv, argv);

    die("execvp");
    return 0;
}
