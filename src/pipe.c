/*
  relay pipe abstraction
*/

/*  includes */
#include "bufs.h"
#include "io_queue.h"
#include "pipe.h"

/*  constants */
enum {
    IN_POLL,
    IN_RDY,
    IN_MUTED
};

/*  routines */
static int handle_input(int fd, struct io *io)
{
    return 0;
}

static int handle_output(int fd, struct io *io)
{
    return 0;
}

void init_pipe(int r_fd, int w_fd, struct pipe *pipe)
{
    pipe->rd.fd = r_fd;
    pipe->rd.handler = handle_input;

    pipe->wr.fd = w_fd;
    pipe->wr.handler = handle_ouput;

    pipe->input.state = IN_RDY;

    pipe->output.q = NULL;
    pipe->output.q_chain = &pipe->output.q;
}

void want_data(struct pipe *pipe,
               input_cb *cb, struct buf *buf, void *p)
{
    pipe->input.buf = buf;
    pipe->input.p = p;
    pipe->input.cb = cb;

    queue_io(&pipe->rd);
}

void send_data(struct pipe *pipe, struct buf *buf)
{
    if (!pipe->output.q) queue_io(&pipe->wr);

    buf->p = NULL;
    *pipe->output.q_chain = buf;
    pipe->output.q_chain = &buf->p;
}
