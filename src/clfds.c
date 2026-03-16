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
