/*
  relay pipe abstraction
*/
#ifndef proc_tools_pipe_h
#define proc_tools_pipe_h

/*  includes */
#include "io_queue.h"

/*  types */
struct buf;

typedef void input_cb(struct buf *, void *);

struct pipe {
    struct io rd, wr, *feeder;

    struct {
        struct buf *buf;
        void *p;
        input_cb *cb;
        int state;
    } input;

    struct {
        struct buf *q, **q_chain;
    } output;
};

/*  routines */
void init_pipe(int r_fd, int w_fd, struct pipe *pipe);

void want_data(struct pipe *pipe,
               input_cb *cb, struct buf *buf, void *p);
void send_data(struct pipe *pipe, struct buf *buf);

#endif
