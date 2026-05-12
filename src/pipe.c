/*
  relay pipe abstraction
*/

/*  includes */
#include <errno.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "bufs.h"
#include "diag.h"
#include "io_queue.h"
#include "loggers.h"
#include "pipe.h"

/*  constants */
enum {
    IN_POLL,
    IN_RDY,
    IN_MUTED,
    IN_EOF
};

/*  routines */
static int handle_input(int fd, struct io *io)
{
    struct pipe *pipe;
    struct buf *buf;
    ssize_t nr;

    pipe = (void *)((char *)io - offsetof(struct pipe, rd));
    if (pipe->input.state == IN_MUTED) return 0;
    buf = pipe->input.buf;

    nr = read(fd, buf->s, buf_data_sz);
    if (nr == -1) {
        if (errno == EAGAIN) {
            pipe->input.state = IN_POLL;
            return POLLIN;
        }

        die("read");
    }

    info("%s: read %zd from $d", __func__, fd, nr);

    if (nr) {
        buf->e = buf->s + nr;
        pipe->input.cb(buf, pipe->input.p);
    } else {
        close(pipe->rd.fd);
        pipe->input.state = IN_EOF;

        return_buf(buf);
        pipe->input.cb(NULL, pipe->input.p);
    }

    return 0;
}

static void close_wr_io(struct io *io)
{
    shutdown(io->fd, SHUT_WR);
    close(io->fd);
}

static int handle_output(int fd, struct io *io)
{
    struct pipe *pipe;
    struct buf *buf;
    ssize_t nw;

    pipe = (void *)((char *)io - offsetof(struct pipe, wr));

    if (pipe->feeder->input.state == IN_MUTED) {
        pipe->feeder->input.state = IN_RDY;
        queue_io(&pipe->feeder->rd);
    }

    buf = pipe->output.q;
    nw = write(fd, buf->s, buf->e - buf->s);
    if (nw == -1) {
        if (errno == EAGAIN) return POLLOUT;
        die("write");
    }

    info("%s: wrote %zd to %d", __func__, fd, nw);

    buf->s += nw;
    if (buf->s == buf->e) {
        pipe->output.q = buf->p;
        if (!buf->p) pipe->output.q_chain = &pipe->output.q;

        return_buf(buf);
    }

    if (pipe->output.q) {
        if (pipe->feeder->input.state == IN_RDY)
            pipe->feeder->input.state = IN_MUTED;

        queue_io(&pipe->wr);
    } else
        if (pipe->feeder->input.state == IN_EOF)
            close_wr_io(io);

    return 0;
}

void init_pipe(int r_fd, int w_fd, struct pipe *feeder, struct pipe *pipe)
{
    pipe->rd.fd = r_fd;
    pipe->rd.handler = handle_input;

    pipe->wr.fd = w_fd;
    pipe->wr.handler = handle_output;

    pipe->feeder = feeder;

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

void all_sent(struct pipe *pipe)
{
    if (pipe->output.q) return;
    close_wr_io(&pipe->wr);
}
