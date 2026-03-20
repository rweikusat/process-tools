/*
  change directory and run a command
*/

/*  includes */
#include <stdlib.h>
#include <unistd.h>

#include "diag.h"

/*  main */
int main(int argc, char **argv)
{
    int rc;

    init_diag("ch-dir");

    if (argc < 3) {
        syslog(LOG_NOTICE, "Usage: ch-dir <directory> <cmd> <arg>*");
        exit(1);
    }

    ++argv;
    rc = chdir(*argv);
    if (rc == -1) die("chdir");

    ++argv;
    execvp(*argv, argv);

    die("execvp");
    return 0;
}
