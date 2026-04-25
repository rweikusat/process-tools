/*
  relay data between two pairs of
  file descriptors
*/

/*  includes */
#include <stdlib.h>
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

/*  main */
int main(int argc, char **argv)
{
    int c;

    init_diag("relay");
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

    return 0;
}
