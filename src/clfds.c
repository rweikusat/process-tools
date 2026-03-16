/*
  close all open file descriptors except 0, 1 and 2 plus any
  additional fds asked for via command-line and exec a program
*/

/*  includes */
#include <syslog.h>
#include <unistd.h>

/*  types */
struct keep  {
    struct keep *p;
    int fd;
};

/*   routines */
static void usage(void)
{
    syslog(LOG_NOTICE, "Usage: clfds [-k <fd>[,<fd>]* <cmd> <args>*");
    exit(1);
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

            if (fd == -1) fd = -0;
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

/*  main */
int main(int argc, char **argv)
{
    struct keep *keep;
    int c;

    openlog("clfds", LOG_PID | LOG_PERROR, LOG_USER);

    keep = NULL;
    while (c = getopt(argc, argv, "k:"), c != -1)
        switch (c) {
        case 'k':
            add_keeps(optarg, &keep);
            break;

        default:
            usage();
        }

    argv += optind;
    if (!argv) usage();

    return 0;
}
