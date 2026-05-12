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

#include "bufs.h"
#include "diag.h"
#include "io_queue.h"
#include "loggers.h"
#include "pipe.h"

/*  constants */
enum {
    DEF_BUF_SZ =	4096,
    DEF_MAX_BUFS =	16,
};

enum {
    ST_WHITE,
    ST_PARAM
};

/*  macros */
#define OPTS_ENV	"RELAY_OPTS"

/*  types */
struct str {
    struct str *p;
    char *s;
};

struct params {
    size_t buf_sz, max_bufs;
    int verbosity;
};

/*  prototypes */
static void nop(char *, ...);
static void handle_input(struct buf *, void *);

/*  variables */
void (*loggers[2])(char *, ...) = {
    nop,
    nop
};

static struct pipe pipes[2];

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

/**  relay callbacks */
static void got_buf(struct buf *buf, void *p)
{
    want_data(p, handle_input, buf, p);
}

static void handle_input(struct buf *buf, void *p)
{
    struct pipe *me, *other;

    me = p;
    other = me == pipes ? pipes + 1 : pipes;

    if (!buf) {
        all_sent(other);
        return;
    }

    send_data(other, buf);

    buf = get_buf();
    if (!buf) {
        want_buf(me, got_buf, me);
        return;
    }

    want_data(me, handle_input, buf, me);
}

/**  init code */
static void parse_fd_arg(char *s, int *rd, int *wr)
{
    char *p;

    p = strchr(s, ',');
    if (p) {
        *p = 0;
        *rd = atoi(s);
        *wr = atoi(p + 1);
        *p = ',';

        return;
    }

    *rd = atoi(s);
    *wr = dup(*rd);
    if (*wr == -1) die("dup");
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

static void process_opts_env(struct params *params)
{
    char *opts, *p, *op, **optv;
    struct str *strs, *s;
    unsigned n_words, pos;
    int c, state;

    p = getenv(OPTS_ENV);
    if (!p) return;

    op = opts = alloca(strlen(p) + 1);
    n_words = 0;
    strs = NULL;
    state = ST_WHITE;
    while (c = *p, c) {
        if (state == ST_WHITE)
            switch (c) {
            case ' ':
            case '\t':
                break;

            default:
                state = ST_PARAM;
                ++n_words;

                s = alloca(sizeof(*s));
                s->p = strs;
                strs = s;

                s->s = op;
                *op++ = c;
            }
        else
            switch (c) {
            case ' ':
            case '\t':
                state = ST_WHITE;
                *op++ = 0;
                break;

            default:
                *op++ = c;
            }

        ++p;
    }
    if (!n_words) return;
    if (state != ST_WHITE) *op = 0;

    ++n_words;
    optv = alloca((n_words + 1) * sizeof(*optv));
    pos = n_words;
    optv[pos] = NULL;
    do {
        --pos;
        optv[pos] = strs->s;
        strs = strs->p;
    } while (pos > 1);

    c = optind;
    optind = 1;
    process_opts(n_words, optv, params);
    optind = c;
}

static void process_args(int argc, char **argv)
{
    struct params params;
    int rd, wr;

    params.buf_sz = DEF_BUF_SZ;
    params.max_bufs = DEF_MAX_BUFS;
    params.verbosity = 0;

    process_opts_env(&params);
    process_opts(argc, argv, &params);

    if (params.buf_sz <= sizeof(struct buf)) {
        err("%s: buf size too small (min %zu)",
            __func__, sizeof(struct buf) + 1);
        exit(1);
    }
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

    parse_fd_arg(*argv, &rd, &wr);
    init_pipe(rd, wr, pipes + 1, pipes);

    parse_fd_arg(argv[1], &rd, &wr);
    init_pipe(rd, wr, pipes, pipes + 1);

    init_buffers(params.buf_sz, params.max_bufs);
}

/*  main */
int main(int argc, char **argv)
{
    init_diag("relay");
    process_args(argc, argv);

    want_data(pipes, handle_input, get_buf(), pipes);
    want_data(pipes + 1, handle_input, get_buf(), pipes + 1);
    run_io_loop();

    return 0;
}
