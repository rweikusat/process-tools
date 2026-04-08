/*
  execute a command with certain locks held
*/

/*  includes */
#include "diag.h"

/*  main */
int main(void)
{
    init_diag("lock");
    msg("Ho ho ho!");
    return 0;
}
