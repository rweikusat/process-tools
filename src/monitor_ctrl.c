/*
  send commands to a running monitor
*/

/*  includes */
#include "diag.h"
#include "monitor_ctrl.h"

/*  main */
int main(void)
{
    init_diag("monitor-ctrl");
    msg("Orcs attacking!");
    return 0;
}
