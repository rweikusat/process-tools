/*
  relay output on a set of file descriptors (default: 1 and 2)
  to syslog
*/

/*  includes */
#include "diag.h"

/*  main */
int main(void)
{
    init_diag("to-syslog");
    msg("Hab Gubmuh!");
    return 0;
}
