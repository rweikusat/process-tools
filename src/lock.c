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
static char *tps[] = {
    [F_RDLCK] =	"READ",
    [F_WRLCK] = "WRITE"
};

/*  routines */
static void usage(void)
{
    msg("Usage: lock ([-t] -r|w <file>)* <cmd> <args>*");
    msg("    Execute a command with a certain set of POSIX record locks held.");
    msg("    The -r option denotes a read lock on a file that's to be acquired,");
    msg("    -w a write lock. Both can appear any number of times. Using -t");
    msg("    requests that the next attempted lock operation will be a trylock,");
    msg("    that is, the program will exit with an error status code if the ");
    msg("    lock cannot be acquired without waiting.");

    exit(1);
}

static void lock(char *path, int kind, int op)
{
    struct flock lk;
    int fd, o_acc, rc;

    o_acc = 0;                /* silence pointless warning */
    switch (kind) {
    case 'r':
        o_acc = O_RDONLY;
        lk.l_type = F_RDLCK;
        break;

    case 'w':
        o_acc = O_RDWR;
        lk.l_type = F_WRLCK;
    }

    fd = open(path, o_acc | O_CREAT, 0666);
    if (fd == -1) {
        err("%s: open %s: %m(%d)", __func__, path, errno);
        exit(1);
    }

    lk.l_whence = lk.l_start = lk.l_len = 0;
    rc = fcntl(fd, op, &lk);
    if (rc == -1) {
        if (op == F_SETLK &&
            (errno == EACCES || errno == EAGAIN)) {
            err("%s: failed to acquire %s lock on %s",
                __func__, tps[lk.l_type], path);
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

    while (c = getopt(argc, argv, "+r:w:t"), c != -1) {
        switch (c) {
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
