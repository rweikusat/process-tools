/*
  relay data between two pairs of
  file descriptors
*/

/*  includes */
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "diag.h"

/*  constants */
enum {
    DEF_BUF_SZ =	4096,
    DEF_MAX_BUFS =	16
};

enum {
    IN_OK,
    IN_WANT_BUF,
    IN_EOF
};

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
static void close_to(int fd)
{
    shutdown(fd, SHUT_WR);
    close(fd);

    noise("%s: closed %d", __func__, fd);
}

static int handle_input(int fd, void *arg, struct io **io_q)
{
    struct io_input *input;
    struct buf *buf;
    ssize_t nr;

    input = arg;

    buf = get_buf();
    if (!buf) {
        input->state = IN_WANT_BUF;
        return -1;
    }
    input->state = IN_OK;

    nr = read(fd, buf->s, buf_data_sz);
    switch (nr) {
    case -1:
        if (errno == EAGAIN) {
            return_buf(buf);
            return POLLIN;
        }

        die("read");

    case 0:
        return_buf(buf);
        close(fd);
        noise("%s: closed %d", __func__, fd);

        if (input->to.q) input->state = IN_EOF;
        else close_to(input->to.io.fd);
        return -1;
    }

    if (!input->to.q) {
        input->to.io.p = *io_q;
        *io_q = &input->to.io;
    }

    buf->e = buf->s + nr;
    *input->to.q_chain = buf;
    input->to.q_chain = &buf->p;

    noise("%s: read %zd from %d", __func__, nr, fd);
    return 0;
}

static int handle_output(int fd, void *arg, struct io **io_q)
{
    struct io_input *input;
    struct buf *buf;
    ssize_t nw;

    input = (void *)((char *)arg - offsetof(struct io_input, to));

    buf = input->to.q;
    nw = write(fd, buf->s, buf->e - buf->s);
    if (nw == -1) {
        if (errno == EAGAIN) return POLLOUT;
        die("write");
    }

    noise("%s: wrote %zd to %d", __func__, nw, fd);

    buf->s += nw;
    if (buf->s < buf->e) return 0;

    input->to.q = buf->p;
    return_buf(buf);
    if (input->state == IN_WANT_BUF) {
        input->io.p = *io_q;
        *io_q = &input->io;
    }

    if (!input->to.q) {
        if (input->state == IN_EOF)
            close_to(fd);
        else
            input->to.q_chain = &input->to.q;

        return -1;
    }

    return 0;
}

static void relay_data(struct io_input *input)
{
    struct pollfd pfds[4];
    struct io *ios[4], *io_q, *cur, *next;
    unsigned n_pfds;
    int rc;

    *ios = &input->io;
    pfds->fd = input->io.fd;
    pfds->events = POLLIN;

    ios[1] = &input[1].io;
    pfds[1].fd = input[1].io.fd;
    pfds[1].events = POLLIN;

    n_pfds = 2;
    io_q = NULL;

    do {
        if (n_pfds) {
            rc = poll(pfds, n_pfds, io_q ? 0 : -1);
            if (rc == -1) die("poll");

            while (rc) {
                --rc;

                if (pfds[rc].revents) {
                    ios[rc]->p = io_q;
                    io_q = ios[rc];

                    noise("%s: 0x%02x for %d",
                          __func__, pfds[rc].revents, pfds[rc].fd);
                }
            }

            n_pfds = 0;
        }

        next = NULL;
        while (io_q) {
            cur = io_q;
            io_q = io_q->p;

            rc = cur->handler(cur->fd, cur, &io_q);
            switch (rc) {
            case -1:
                break;

            case 0:
                cur->p = next;
                next = cur;
                break;

            default:
                ios[n_pfds] = cur;
                pfds[n_pfds].fd = cur->fd;
                pfds[n_pfds].events = rc;

                ++n_pfds;
            }
        }
        io_q = next;
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

static void process_args(int argc, char **argv, struct io_input *input)
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

    parse_fd_arg(*argv, &input[0].io.fd, &input[1].to.io.fd);
    parse_fd_arg(argv[1], &input[1].io.fd, &input[0].to.io.fd);

    buf_data_sz = buf_sz - sizeof(struct buf);
}

static void init_input(struct io_input *input)
{
    input->io.handler = handle_input;

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
