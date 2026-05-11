/*
  relay I/O queue
*/
#ifndef proc_tools_io_queue_h
#define proc_tools_io_queue_h

/*  types */
struct io {
    struct io *p;
    int (*handler)(int, struct io *);
    int fd;
};

/*  routines */
void queue_io(struct io *io);
void run_io_loop(void);

#endif
