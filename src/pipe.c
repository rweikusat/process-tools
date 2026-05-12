/*
  relay pipe abstraction
*/

/*  includes */
#include <errno.h>
#include <fcntl.h>
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
    IN_ACTIVE,
    IN_PASSIVE,
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

    info("%s: read %zd from %d", __func__, nr, fd);
    pipe->input.state = IN_PASSIVE;

    if (nr) buf->e = buf->s + nr;
    else {
        close(fd);
        pipe->input.state = IN_EOF;

        info("%s: closed %d", __func__, fd);

        return_buf(buf);
        buf = NULL;
    }

    pipe->input.cb(buf, pipe->input.p);
    return 0;
}

static void close_wr_io(struct io *io)
{
    shutdown(io->fd, SHUT_WR);
    close(io->fd);

    info("%s: closed %d", __func__, io->fd);
}

static int handle_output(int fd, struct io *io)
{
    struct pipe *pipe;
    struct buf *buf;
    ssize_t nw;
    int unmuted;

    pipe = (void *)((char *)io - offsetof(struct pipe, wr));

    unmuted = 0;
    if (pipe->feeder->input.state == IN_MUTED) {
        pipe->feeder->input.state = IN_ACTIVE;
        queue_io(&pipe->feeder->rd);

        unmuted = 1;
    }

    buf = pipe->output.q;
    nw = write(fd, buf->s, buf->e - buf->s);
    if (nw == -1) {
        if (errno == EAGAIN) return POLLOUT;
        die("write");
    }

    info("%s: wrote %zd to %d", __func__, nw, fd);

    buf->s += nw;
    if (buf->s == buf->e) {
        pipe->output.q = buf->p;
        if (!buf->p) pipe->output.q_chain = &pipe->output.q;

        return_buf(buf);
    }

    if (pipe->output.q) {
        if (!unmuted && pipe->feeder->input.state == IN_ACTIVE)
            pipe->feeder->input.state = IN_MUTED;

        queue_io(&pipe->wr);
    } else
        if (pipe->feeder->input.state == IN_EOF)
            close_wr_io(io);

    return 0;
}

static inline void set_nonblocking(int fd)
{
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
}

void init_pipe(int r_fd, int w_fd, struct pipe *feeder, struct pipe *pipe)
{
    set_nonblocking(r_fd);
    pipe->rd.fd = r_fd;
    pipe->rd.handler = handle_input;

    set_nonblocking(w_fd);
    pipe->wr.fd = w_fd;
    pipe->wr.handler = handle_output;

    pipe->feeder = feeder;

    pipe->input.state = IN_PASSIVE;

    pipe->output.q = NULL;
    pipe->output.q_chain = &pipe->output.q;
}

void want_data(struct pipe *pipe,
               input_cb *cb, struct buf *buf, void *p)
{
    pipe->input.buf = buf;
    pipe->input.p = p;
    pipe->input.cb = cb;
    pipe->input.state = IN_ACTIVE;

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
