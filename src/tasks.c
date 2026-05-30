/*
  relay task queue
*/

/*  include */
#include "tasks.h"

/*  types */
struct task {
    struct task *p;
    task_fn *fn;
    void *fn_p;
};

/*  variables */
static struct task tsks[2];
static unsigned nxt_tsk;

static struct {
    struct task *first, **chain;
} q = {
    .chain = &q.first
};

/*  routines */
void queue_task(task_fn *fn, void *p)
{
    struct task *tsk;

    tsk = tsks + nxt_tst;
    nxt_tsk ^= 1;

    tsk->fn = fn;
    tsk->fn_p = p;
    tsk->p = NULL;

    *q.chain = tsk;
    q.chain = &tsk->p;
}

void run_tasks(void)
{
    struct task *tsk;

    tsk = q.first;
    if (!tsk) return;

    q.first = NULL;
    q.chain = &q.first;

    do {
        tsk->fn(tsk->fn_p);
        tsk = tsk->p;
    } while (tsk);
}
