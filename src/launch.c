/*
  launch a background process
*/

/*  includes */
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "diag.h"

/*  routines */
static void usage(void)
{
    msg("Usage: launch [-n <name>] <cmd> <arg>*");
    exit(1);
}

static void background(void)
{
    switch (fork()) {
    case -1:
        die("fork");

    case 0:
        break;

    default:
        exit(0);
    }

    setsid();
}

static void setup_std_fds(void)
{
    int fd, rc;

    close(0);
    close(1);
    close(2);

    fd = open("/dev/null", O_RDWR, 0);
    if (fd == -1) die("open /dev/null");

    rc = dup(fd);
    if (rc != -1) rc = dup(fd);
    if (rc == -1) die("dup");
}

/*  main */
int main(int argc, char **argv)
{
    char *name;
    int c;

    init_diag("launch");

    name = NULL;
    while (c = getopt(argc, argv, "+n:"), c != -1)
        switch (c) {
        case 'n':
            name = optarg;
            break;

        default:
            usage();
        }

    argv += optind;
    if (!*argv) usage();
    if (!name) name = *argv;

    background();
    setup_std_fds();

    msg("exec '%s'", name);
    execvp(*argv, argv);

    die("execvp");
    return 1;
}
