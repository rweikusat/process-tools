/*
  relay task queue
*/
#ifndef proc_tools_tasks_h
#define proc_tools_tasks_h

/*  types */
typedef void task_fn(void *);

/*  routines */
void queue_task(task_fn *p, void *p);
void run_tasks();

#endif
