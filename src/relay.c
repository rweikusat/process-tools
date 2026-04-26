/*
  relay data between two pairs of
  file descriptors
*/

/*  includes */
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
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

/*  routines */
static void usage(void)
{
    msg("Usage: relay [-b <bufsize>] <fdr0>,<fdw0> <fdr1>,<fdw1>");
    msg("   Relay data between two pairs of file descriptors. Data read from");
    msg("   <fdr0> will be written to <fdw1>, data read from <fdr1> to <fdw0>");
    msg("   The -b option can be used to specifiy an non-default input buffer");
    msg("   size. Default is 4096 bytes.");

    exit(0);
}

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

static void parse_fd_pair(char *s, int *from, int *to)
{
    char *p;

    p = strchr(s, ',');
    if (!p) {
        err("%s: invalid fd pair: %s",
            __func__, s);
        exit(1);
    }

    *p = 0;
    *from = atoi(s);
    *to = atoi(p + 1);
    *p = ',';
}

static void process_args(int argc, char **argv, struct io *ios)
{
    int c;

    while (c = getopt(argc, argv, "+b:"), c != -1)
        switch (c) {
        case 'b':
            buf_sz = atoi(optarg);
            if (buf_sz <= sizeof(struct buf)) {
                err("%s: buffer size must be larger than %zu",
                    __func__, sizeof(struct buf));
                exit(1);
            }

            buf_data_sz = buf_sz - sizeof(struct buf);
            break;

        default:
            usage();
        }

    argv += optind;
    if (!*argv || !argv[1] || argv[2])
        usage();

    parse_fd_pair(*argv, &ios[0].from, &ios[1].to);
    parse_fd_pair(argv[1], &ios[1].from, &ios[0].to);
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
}

/*  main */
int main(int argc, char **argv)
{
    struct io ios[2];
    int ep_fd;

    init_diag("relay");
    init(argc, argv, ios);
    ep_fd = setup_epoll(ios);

    return 0;
}
