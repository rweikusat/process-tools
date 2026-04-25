/*
  relay data between two pairs of
  file descriptors
*/

/*  includes */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "diag.h"

/*  constants */
enum {
    DEF_BUF_SZ =	4096
};

/*  types */
struct buf {
    struct buf *p;
    char *e;
};

struct io {
    int from, to;
    struct buf *q, **q_chain;
};

/*  variables */
static size_t buf_sz = DEF_BUF_SZ, buf_data_sz;
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

static inline char *buf_data(struct buf *buf)
{
    return (char *)(buf + 1);
}

static struct buf *get_buf(void)
{
    struct buf *buf;

    buf = buffers;
    if (buf) buffers = buf->p;
    else {
        buf = malloc(buf_sz);
        if (!buf) die("malloc");
    }

    buf->p = NULL;
    buf->e = buf_data(buf);

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

    ios[0].q = NULL;
    ios[0].q_chain = &ios[0].q;

    ios[1].q = NULL;
    ios[1].q_chain = &ios[1].q;
}


/*  main */
int main(int argc, char **argv)
{
    struct io ios[2];

    init_diag("relay");
    process_args(argc, argv, ios);
    return 0;
}
