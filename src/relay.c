/*
  relay data between two pairs of
  file descriptors
*/

/*  includes */
#include "diag.h"

/*  main */
int main(void)
{
    init_diag("relay");
    msg("Ha!");
    return 0;
}
