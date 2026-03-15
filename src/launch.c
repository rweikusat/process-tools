/*
  launch a background process
*/

/*  includes */
#include <errno.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>

/*  main */
int main(int argc, char **argv)
{
    openlog("launch", LOG_PID | LOG_PERROR, LOG_USER);

    if (argc < 2) {
        syslog(LOG_WARNING, "Usage: launch <command< <arg>*");
        exit(1);
    }

    switch (fork()) {
    case -1:
        syslog(LOG_ERR, "%s: fork: %m(%d)", __func__, errno);
        exit(1);

    case 0:
        break;

    default:
        exit(0);
    }

    syslog(LOG_NOTICE, "launching not implemented");
    return 0;
}
