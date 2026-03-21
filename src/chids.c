/*
  change uid/gid and run a command
*/

/*  includes */
#include "diag.h"

/*  main */
int main(void)
{
    init_diag("chids");
    syslog(LOG_NOTICE, "Ha!");
    return 0;
}
