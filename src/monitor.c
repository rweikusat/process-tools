/*
  monitor program
*/

/*  includes */
#include "diag.h"

/*  main */
int main(void)
{
    init_diag("monitor");
    syslog(LOG_NOTICE, "Hurz!");
    return 0;
}
