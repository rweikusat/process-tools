/*
  relay data between two pairs of
  file descriptors
*/

/*  includes */
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "diag.h"

/*  constants */
enum {
    DEF_BUF_SZ =	4096,
    DEF_MAX_BUFS =	16,
    IO_Q_QUOTA =	8
};

enum {
    IN_WAIT   ,
    IN_RDY,
    IN_WANT_BUF,
    IN_EOF,
    IN_MUTED
};

enum {
    D_QUIET,
    D_INFO,
    D_DEBUG
};

/*  macros */
#define info loggers[D_INFO - 1]
#define debug loggers[D_DEBUG - 1]

/*  types */
struct buf {
    struct buf *p;
    char *s, *e;
};

struct io {
    struct io *p;
    int (*handler)(int, void *, struct io **);
    int fd;
};

struct io_output {
    struct io io;
    struct buf *q, **q_chain;
};

struct io_input {
    struct io io;
    struct io_output to;
    int state;
};

struct params {
    size_t buf_sz, max_bufs;
    int verbosity;
};

/*  variables */
static struct {
    char *p, *e;
    size_t sz, data_sz;
    struct buf *free;
} bufs;

static void nop(char *, ...);

static void (*loggers[2])(char *, ...) = {
    nop,
    nop
};

/*  routines */
/**  misc */
static void usage(void)
{
    msg("Usage: relay [-b <bufsize>] [-m <maxbuf>] [-v <verbosity>] <fd0>[,<fdw0>] <fd1>[,<fdw1>]");
    msg("   Relay data between two file descriptors or pairs of file descriptors.");
    msg("   Data read from <fd0> will be written to <fd1> and vice versa. If an");
    msg("   optional for-write file descriptor (<fdwN>) was specified as well,");
    msg("   the first file descriptor of the pair will only be used for reading");
    msg("   data.");
    msg("   The -b option can be used to specifiy an non-default input buffer");
    msg("   size. Default is 4096 bytes.");
    msg("   The -m option enables changing the maximum number of buffers the");
    msg("   program wil allocate. Default is 16.");
    msg("   The -v option can be used to request printing of informational messages.");
    msg("   At level 1, messages about bytes read and written will be prinred. On");
    msg("   level 2, an additional message will be printed before each I/O task");
    msg("   queue run.");

    exit(0);
}

static void nop(char *unused, ...)
{
    (void)unused;
}

/**  buffer management */
static inline void reset_buf(struct buf *buf)
{
    buf->s = buf->e = (char *)(buf + 1);
}

static struct buf *get_buf(void)
{
    struct buf *buf;

    buf = bufs.free;
    if (buf) bufs.free = buf->p;
    else {
        if (bufs.p == bufs.e) return NULL;

        buf = (void *)bufs.p;
        bufs.p += bufs.sz;
    }

    buf->p = NULL;
    reset_buf(buf);

    return buf;
}

static inline void return_buf(struct buf *buf)
{
    buf->p = bufs.free;
    bufs.free = buf;
}

static void init_buffers(size_t max_bufs)
{
    size_t need;

    need = bufs.sz * max_bufs;
    bufs.p = mmap(NULL, need, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    if (bufs.p == MAP_FAILED) die("mmap");

    bufs.e = bufs.p + need;
}

/**  relaying */
static void close_to(int fd)
{
    shutdown(fd, SHUT_WR);
    close(fd);

    info("%s: closed %d", __func__, fd);
}

static int handle_input(int fd, void *arg, struct io **also)
{
    struct io_input *input;
    struct buf *buf;
    int state;
    ssize_t nr;

    input = arg;
    state = input->state;
    if (state == IN_MUTED) return 0;

    buf = get_buf();
    if (!buf) {
        input->state = IN_WANT_BUF;
        return -1;
    }

    nr = read(fd, buf->s, bufs.data_sz);
    switch (nr) {
    case -1:
        if (errno == EAGAIN) {
            input->state = IN_WAIT;
            return_buf(buf);
            return POLLIN;
        }

        die("read");

    case 0:
        return_buf(buf);
        close(fd);
        info("%s: closed %d", __func__, fd);

        if (input->to.q) input->state = IN_EOF;
        else close_to(input->to.io.fd);
        return -1;
    }

    if (state == IN_WAIT) input->state = IN_RDY;
    if (!input->to.q) *also = &input->to.io;

    buf->e = buf->s + nr;
    *input->to.q_chain = buf;
    input->to.q_chain = &buf->p;

    info("%s: read %zd from %d", __func__, nr, fd);
    return 0;
}

static int handle_output(int fd, void *arg, struct io **also)
{
    struct io_input *input;
    struct buf *buf;
    int state;
    ssize_t nw;

    input = (void *)((char *)arg - offsetof(struct io_input, to));

    buf = input->to.q;
    nw = write(fd, buf->s, buf->e - buf->s);
    if (nw == -1) {
        if (errno == EAGAIN) return POLLOUT;
        die("write");
    }

    info("%s: wrote %zd to %d", __func__, nw, fd);

    buf->s += nw;
    if (buf->s < buf->e) return 0;

    input->to.q = buf->p;
    return_buf(buf);

    if (input->state == IN_WANT_BUF) {
        *also = &input->io;
        input->state = IN_RDY;
    }
    state = input->state;

    /*
      If more than one buffer has been queued for transmission, the
      sender has moved into the past relative to the receiver. Give it
      a chance to catch up by ensuring that its receiver will only
      read data on every other iteration if it's also on the queue.
    */
    if (input->to.q) {
        switch (state) {
        case IN_RDY:
            input->state = IN_MUTED;
            break;

        case IN_MUTED:
            input->state = IN_RDY;
        }
    } else {
        switch (state) {
        case IN_EOF:
            close_to(fd);
            break;

        case IN_MUTED:
            input->state = IN_RDY;

        default:
            input->to.q_chain = &input->to.q;
        }

        return -1;
    }

    return 0;
}

static void do_poll(struct pollfd *pfds, struct io **ios,
                    unsigned *n_pfds, struct io **io_q)
{
    unsigned pos, n;
    int rc;

    n = *n_pfds;

    rc = poll(pfds, n, *io_q ? 0 : -1);
    if (rc == -1) die("poll");

    pos = 0;
    while (rc) {
        if (pfds[pos].revents) {
            info("%s: 0x%02x for %d",
                 __func__, pfds[pos].revents, pfds[pos].fd);

            ios[pos]->p = *io_q;
            *io_q = ios[pos];

            --n;
            pfds[pos] = pfds[n];
            ios[pos] = ios[n];

            --rc;
        } else
            ++pos;
    }

    *n_pfds = n;
}

static struct io *run_io_q(struct io *io_q,
                           struct pollfd *pfds, struct io **ios, unsigned *n_pfds)
{
    struct io *cur, *next, **next_chain, *also;
    unsigned quota, n;
    int rc;

    quota = IO_Q_QUOTA;
    n = *n_pfds;
    do {
        next = NULL;
        next_chain = &next;

        debug("%s: running q", __func__);
        while (io_q) {
            cur = io_q;
            io_q = io_q->p;

            also = NULL;

            rc = cur->handler(cur->fd, cur, &also);
            switch (rc) {
            case -1:
                break;

            case 0:
                cur->p = NULL;
                *next_chain = cur;
                next_chain = &cur->p;
                break;

            default:
                ios[n] = cur;
                pfds[n].fd = cur->fd;
                pfds[n].events = rc;

                ++n;
            }

            if (also) {
                also->p = next;
                next = also;
            }
        }

        io_q = next;
    } while (io_q && --quota);

    *n_pfds = n;
    return io_q;
}

static void init_pfd(struct io *io, struct io **pio, struct pollfd *pfd)
{
    *pio = io;
    pfd->fd = io->fd;
    pfd->events = POLLIN;
}

static void relay_data(struct io_input *input)
{
    struct pollfd pfds[4];
    struct io *ios[4], *io_q;
    unsigned n_pfds;

    init_pfd(&input->io, ios, pfds);
    init_pfd(&input[1].io, ios + 1, pfds + 1);
    n_pfds = 2;
    io_q = NULL;

    do {
        if (n_pfds) do_poll(pfds, ios, &n_pfds, &io_q);
        io_q = run_io_q(io_q, pfds, ios, &n_pfds);
    } while (n_pfds || io_q);
}

/**  init code */
static void parse_fd_arg(char *s, int *from, int *to)
{
    char *p;

    p = strchr(s, ',');
    if (p) {
        *p = 0;
        *from = atoi(s);
        *to = atoi(p + 1);
        *p = ',';

        return;
    }

    *from = atoi(s);
    *to = dup(*from);
    if (*to == -1) die("dup");
}

static void process_opts(int n_args, char **argv, struct params *params)
{
    int c;

    while (c = getopt(n_args, argv, "+b:m:v:"), c != -1)
        switch (c) {
        case 'b':
            params->buf_sz = atoi(optarg);
            break;

        case 'm':
            params->max_bufs = atoi(optarg);
            break;

        case 'v':
            params->verbosity = atoi(optarg);
            break;

        default:
            usage();
        }
}

static void process_args(int argc, char **argv, struct io_input *input)
{
    struct params params;

    params.buf_sz = DEF_BUF_SZ;
    params.max_bufs = DEF_MAX_BUFS;
    params.verbosity = 0;

    process_opts(argc, argv, &params);

    if (params.buf_sz <= sizeof(struct buf)) {
        err("%s: buf size too small (min %zu)",
            __func__, sizeof(struct buf) + 1);
        exit(1);
    }
    bufs.sz = params.buf_sz;

    if (params.max_bufs < 2) {
        err("%s: max bufs too small (min 2)",
            __func__);
        exit(1);
    }

    if (params.verbosity > D_QUIET) loggers[D_INFO - 1] = msg;
    if (params.verbosity > D_INFO) loggers[D_DEBUG - 1] = msg;

    argv += optind;
    if (!*argv || !argv[1] || argv[2])
        usage();

    parse_fd_arg(*argv, &input[0].io.fd, &input[1].to.io.fd);
    parse_fd_arg(argv[1], &input[1].io.fd, &input[0].to.io.fd);

    bufs.data_sz = bufs.sz - sizeof(struct buf);
    init_buffers(params.max_bufs);
}

static void init_input(struct io_input *input)
{
    input->io.handler = handle_input;
    input->state = IN_WAIT;

    input->to.io.handler = handle_output;
    input->to.q = NULL;
    input->to.q_chain = &input->to.q;

    fcntl(input->io.fd, F_SETFL, fcntl(input->io.fd, F_GETFL) | O_NONBLOCK);
    fcntl(input->to.io.fd, F_SETFL, fcntl(input->to.io.fd, F_GETFL) | O_NONBLOCK);
}

static void init(int argc, char **argv, struct io_input *input)
{
    process_args(argc, argv, input);

    init_input(input);
    init_input(input + 1);
}

/*  main */
int main(int argc, char **argv)
{
    struct io_input input[2];

    init_diag("relay");
    init(argc, argv, input);

    relay_data(input);

    return 0;
}
