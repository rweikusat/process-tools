/*
  relay task queue
*/

/*  include */
#include "tasks.h"

/*  types */
struct task {
    struct task *p;
    task_fn *fn;
    void *p;
};
