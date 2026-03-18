/*
  sanitize environment
*/

/*  includes */
#include "diag.h"

/*  main */
int main(void)
{
    init_diag("sane-env");
    syslog(LOG_NOTICE, "Ha!");

    return 0;
}
