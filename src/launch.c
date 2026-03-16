/*
  launch a background process
*/

/*  includes */
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>

/*  macros */
#define die(sysc) die_(__func__, sysc)

/*  routines */
static void die_(char const *fnc, char *sysc)
{
    syslog(LOG_ERR, "%s: %s: %m(%d)", fnc, sysc, errno);
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
    openlog("launch", LOG_PID | LOG_PERROR, LOG_USER);

    if (argc < 2) {
        syslog(LOG_WARNING, "Usage: launch <command> <arg>*");
        exit(1);
    }

    background();
    setup_std_fds();

    ++argv;
    syslog(LOG_NOTICE, "exec '%s'", *argv);
    execvp(*argv, argv);

    die("execvp");
    return 1;
}
