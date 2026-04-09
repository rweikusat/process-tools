/*
  check if a certain set of locks is held in the current "process
  scope"
*/

/*  includes */
#include "diag.h"

/*  main */
int main(void)
{
    init_diag("have_locks");
    msg("Yo!");
    return 0;
}
