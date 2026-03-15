/*
  launch a background process
*/

/*  includes */
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>

/*  routines */
static void background(void)
{
    switch (fork()) {
    case -1:
        syslog(LOG_ERR, "%s: fork: %m(%d)", __func__, errno);
        exit(1);

    case 0:
        break;

    default:
        exit(0);
    }

    setsid();
}

static void setup_std_fds(void)
{
    int fd;

    close(0);
    close(1);
    close(2);

    fd = open("/dev/null", O_RDWR, 0);
    dup(fd);
    dup(fd);
}

/*  main */
int main(int argc, char **argv)
{
    openlog("launch", LOG_PID | LOG_PERROR, LOG_USER);

    if (argc < 2) {
        syslog(LOG_WARNING, "Usage: launch <command< <arg>*");
        exit(1);
    }

    background();
    setup_std_fds();

    ++argv;
    execvp(*argv, argv);

    syslog(LOG_ERR, "execvp %s: %m(%d)", *argv, errno);
    return 1;
}
