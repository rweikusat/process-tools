/*
  execute a command with certain locks held
*/

/*  includes */
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "diag.h"

/*  variables */
static int quiet;

static char *tps[] = {
    [F_RDLCK] =	"READ",
    [F_WRLCK] = "WRITE"
};

/*  routines */
static void usage(void)
{
    msg("Usage: lock [-q] ([-t] -r|w <file>)* <cmd> <args>*");
    msg("    Execute a command with a certain set of POSIX record locks held.");
    msg("    The -r option denotes a read lock on a file that's to be acquired,");
    msg("    -w a write lock. Both can appear any number of times. Using -t");
    msg("    requests that the next attempted lock operation will be a trylock,");
    msg("    that is, the program will exit with an error status code if the ");
    msg("    lock cannot be acquired without waiting.");
    msg("    The -q option can be used to disable information messages.");

    exit(1);
}

static void lock(char *path, int kind, int op)
{
    struct flock lk;
    int fd, o_flags, rc;

    o_flags = 0;                /* silence pointless warning */
    switch (kind) {
    case 'r':
        o_flags = O_RDONLY;
        lk.l_type = F_RDLCK;
        break;

    case 'w':
        o_flags = O_RDWR;
        lk.l_type = F_WRLCK;
    }

    if (!quiet)
        msg("trying to acquire %s lock on %s",
            tps[lk.l_type], path);

    fd = open(path, o_flags, 0);
    if (fd == -1) die("open");

    lk.l_whence = lk.l_start = lk.l_len = 0;
    rc = fcntl(fd, op, &lk);
    if (rc == -1) {
        if (op == F_SETLK &&
            (errno == EACCES || errno == EAGAIN)) {
            msg("failed");
            exit(1);
        }

        die("fcntl");
    }
}

/*  main */
int main(int argc, char **argv)
{
    int c, op;

    init_diag("lock");
    op = F_SETLKW;

    while (c = getopt(argc, argv, "qr:w:t"), c != -1) {
        switch (c) {
        case 'q':
            quiet = 1;
            break;

        case 'r':
        case 'w':
            lock(optarg, c, op);
            op = F_SETLKW;
            break;

        case 't':
            op = F_SETLK;
            break;

        default:
            usage();
        }
    }

    argv += optind;
    if (!*argv) usage();

    execvp(*argv, argv);

    die("execvp");
    return 0;
}
