/*
  launch a background process
*/

/*  includes */
#include <stdlib.h>
#include <syslog.h>

/*  main */
int main(int argc, char **argv)
{
    openlog("launch", LOG_PID | LOG_PERROR, LOG_USER);

    if (argc < 2) {
        syslog(LOG_WARNING, "Usage: launch <command< <arg>*");
        exit(1);
    }

    return 0;
}
