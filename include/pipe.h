/*
  relay pipe abstraction
*/
#ifndef proc_tools_pipe_h
#define proc_tools_pipe_h

/*  includes */
#include "io_queue.h"

/*  types */
struct buf;

struct pipe {
    struct io rd, wr, *feeder;

    struct {
        struct buf *buf;
        void *p;
        void (*cb)(struct buf *, void *);
        int state;
    } input;

    struct {
        struct buf *q, **q_chain;
    } output;
};

#endif
