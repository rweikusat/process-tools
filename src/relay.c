/*
  relay data between two pairs of
  file descriptors
*/

/*  includes */
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "diag.h"

/*  constants */
enum {
    DEF_BUF_SZ =	4096,
    DEF_MAX_BUFS =	16
};

enum {
    WANT_BUF,
    WANT_INPUT,
    IN_EOF,
    CLOSED
};

/*  types */
struct buf {
    struct buf *p;
    char *s, *e;
};

struct io {
    int from, to, state;
    struct buf *in_buf;
    struct buf *q, **q_chain;
};

/*  variables */
static size_t buf_sz = DEF_BUF_SZ, buf_data_sz, bufs_remain = DEF_MAX_BUFS;
static struct buf *buffers;
static void (*noise)(char *, ...) = msg;

/*  routines */
/**  misc */
static void usage(void)
{
    msg("Usage: relay [-q] [-b <bufsize>] <fd0>[,<fdw0>] <fd1>[,<fdw1>]");
    msg("   Relay data between two file descriptors or pairs of file descriptors.");
    msg("   Data read from <fd0> will be written to <fd1> and vice versa. If an");
    msg("   optional for-write file descriptor (<fdwN>) was specified as well,");
    msg("   the first file descriptor of the pair will only be used for reading");
    msg("   data.");
    msg("   The -b option can be used to specifiy an non-default input buffer");
    msg("   size. Default is 4096 bytes.");
    msg("   The -q option can be used to disabled printing of informative");
    msg("   messages.");

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

    buf = buffers;
    if (buf) buffers = buf->p;
    else {
        if (!bufs_remain) return NULL;

        buf = malloc(buf_sz);
        if (!buf) die("malloc");
        --bufs_remain;
    }

    buf->p = NULL;
    reset_buf(buf);

    return buf;
}

static void return_buf(struct buf *buf)
{
    buf->p = buffers;
    buffers = buf;
}

/**  relaying */
static void ctl_mod(int ep_fd, int fd, void *p, unsigned ev)
{
    struct epoll_event epev;
    int rc;

    epev.events = ev;
    epev.data.ptr = p;
    rc = epoll_ctl(ep_fd, EPOLL_CTL_MOD, fd, &epev);
    if (rc == -1) die("epoll_ctl");

    noise("%s: changed %d to %u", __func__, fd, ev);
}

static void ctl_del(int ep_fd, int fd)
{
    struct epoll_event dummy;
    int rc;

    rc = epoll_ctl(ep_fd, EPOLL_CTL_DEL, fd, &dummy);
    if (rc == -1) die("epoll_ctl");

    noise("%s: deleted %d", __func__, fd);
}

static void handle_from(int ep_fd, struct io *io)
{
    struct buf *buf;

    switch (io->state) {
    case WANT_BUF:
        buf = get_buf();
        if (buf) {
            io->in_buf = buf;
            io->state = WANT_INPUT;
            ctl_mod(ep_fd, io->from, io, EPOLLIN);
        }

        break;

    case WANT_INPUT:
        if (io->in_buf) break;

        buf = get_buf();
        if (buf) io->in_buf = buf;
        else {
            io->state = WANT_BUF;
            ctl_mod(ep_fd, io->from, io, 0);
        }
    }
}

static void close_to(int ep_fd, struct io *io)
{
    ctl_del(ep_fd, io->to);
    shutdown(io->to, SHUT_WR);
    close(io->to);

    io->state = CLOSED;
    noise("%s: closed %d", __func__, io->to);
}

static void handle_out(int ep_fd, struct io *io)
{
    struct buf *buf;
    ssize_t nw;

    while (buf = io->q, buf) {
        do {
            nw = write(io->to, buf->s, buf->e - buf->s);
            if (nw > -1) {
                buf->s += nw;
                noise("%s: wrote %zd to %d", __func__, nw, io->to);
            }
        } while (nw != -1 && buf->s < buf->e);
        if (nw == -1) {
            if (errno == EAGAIN) return;
            die("write");
        }

        io->q = buf->p;
        return_buf(buf);
    }

    io->q = NULL;
    io->q_chain = &io->q;

    if (io->state == IN_EOF) {
        close_to(ep_fd, io);
        return;
    }

    ctl_mod(ep_fd, io->to, io, 0);
}

static void handle_in(int ep_fd, struct io *io)
{
    struct buf *buf;
    ssize_t n;

    buf = io->in_buf;
    n = read(io->from, buf->s, buf_data_sz);
    switch (n) {
    case -1:
        if (errno == EAGAIN) return;
        die("read");

    case 0:
        ctl_del(ep_fd, io->from);
        close(io->from);

        if (io->q) io->state = IN_EOF;
        else close_to(ep_fd, io);
        return;
    }

    buf->e = buf->s + n;
    noise("%s: read %zd from %d", __func__, n, io->from);

    if (io->q) {
        buf->p = NULL;

        *io->q_chain = buf;
        io->q_chain = &buf->p;
        io->in_buf = NULL;
        return;
    }

    do {
        n = write(io->to, buf->s, buf->e - buf->s);
        if (n > -1) {
            buf->s += n;
            noise("%s: wrote %zd to %d", __func__, n, io->to);
        }
    } while (n != -1 && buf->s < buf->e);
    if (n == -1) {
        if (errno != EAGAIN) die("write");

        buf->p = NULL;

        io->q = buf;
        io->q_chain = &buf->p;
        io->in_buf = NULL;
        ctl_mod(ep_fd, io->to, io, EPOLLOUT);
        return;
    }

    reset_buf(buf);
}

static void relay_data(int ep_fd, struct io *ios)
{
    struct epoll_event epevs[4];
    int rc;

    do {
        handle_from(ep_fd, ios);
        handle_from(ep_fd, ios + 1);

        rc = epoll_wait(ep_fd, epevs, 4, -1);
        if (rc == -1) die("epoll_wait");

        while (rc--)
            if (epevs[rc].events & EPOLLOUT)
                handle_out(ep_fd, epevs[rc].data.ptr);
            else
                handle_in(ep_fd, epevs[rc].data.ptr);
    } while (!(ios[0].state == CLOSED && ios[1].state == CLOSED));
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

static void process_args(int argc, char **argv, struct io *ios)
{
    int c;

    while (c = getopt(argc, argv, "+b:q"), c != -1)
        switch (c) {
        case 'b':
            buf_sz = atoi(optarg);
            if (buf_sz <= sizeof(struct buf)) {
                err("%s: buffer size must be larger than %zu",
                    __func__, sizeof(struct buf));
                exit(1);
            }

            break;

        case 'q':
            noise = nop;
            break;

        default:
            usage();
        }

    argv += optind;
    if (!*argv || !argv[1] || argv[2])
        usage();

    parse_fd_arg(*argv, &ios[0].from, &ios[1].to);
    parse_fd_arg(argv[1], &ios[1].from, &ios[0].to);
    buf_data_sz = buf_sz - sizeof(struct buf);
}

static void init_io(struct io *io)
{
    io->state = WANT_BUF;
    io->in_buf = NULL;
    io->q = NULL;
    io->q_chain = &io->q;

    fcntl(io->from, F_SETFL, fcntl(io->from, F_GETFL) | O_NONBLOCK);
    fcntl(io->to, F_SETFL, fcntl(io->to, F_GETFL) | O_NONBLOCK);
}

static void init(int argc, char **argv, struct io *ios)
{
    process_args(argc, argv, ios);

    init_io(ios);
    init_io(ios + 1);
}

static int setup_epoll(struct io *ios)
{
    struct epoll_event epev;
    int ep_fd, rc;

    ep_fd = epoll_create(4);
    if (ep_fd ==- -1) die("epoll_create");

    epev.events = 0;

    epev.data.ptr = ios;
    rc = epoll_ctl(ep_fd, EPOLL_CTL_ADD, ios->to, &epev);
    if (rc != -1) rc = epoll_ctl(ep_fd, EPOLL_CTL_ADD, ios->from, &epev);
    if (rc == -1) die("epoll_ctl/ 0");

    epev.data.ptr = ++ios;
    rc = epoll_ctl(ep_fd, EPOLL_CTL_ADD, ios->to, &epev);
    if (rc != -1) rc = epoll_ctl(ep_fd, EPOLL_CTL_ADD, ios->from, &epev);
    if (rc == -1) die("epoll_ctl/ 1");

    return ep_fd;
}

/*  main */
int main(int argc, char **argv)
{
    struct io ios[2];
    int ep_fd;

    init_diag("relay");
    init(argc, argv, ios);
    ep_fd = setup_epoll(ios);
    relay_data(ep_fd, ios);

    return 0;
}
