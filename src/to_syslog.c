/*
  relay output on a set of file descriptors (default: 1 and 2)
  to syslog
*/

/*  includes */
#include <ctype.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "diag.h"

/*  types */
struct relay_fd {
    struct relay_fd *p;
    int pipe[2], to;
};

struct l_buf {
    char *s, *p, *e;
};

/*  variables */
static struct relay_fd def_relay[] = {
    {
        .p = def_relay + 1,
        .to = 1 },
    {
        .to = 2 }
};

static unsigned relayers;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

/*  routines */
static void usage(void)
{
    msg("Usage: to-syslog [-f <fd>[,<fd>*] <cmd> <arg>*");
    msg("    Execute a command and relay output on certain file descriptors");
    msg("    (default: 1 and 2) to syslog.");
    msg("    The -f option can be used to specify a different set of file");
    msg("    descriptors.");
    msg("    Output lines matching the Perl pattern '^\\w\\[0-9+\\]: ' won't be");
    msg("    won't be relayed as it's assumed that they were already sent to");
    msg("    syslog.");

    exit(1);
}

static inline int c2dg(unsigned c)
{
    c -= '0';
    if (c < 10) return c;
    return -1;
}

static struct relay_fd *parse_fd_list(char *fdl)
{
    struct relay_fd *first, **chain, *r_fd;
    int fd, c;

    chain = &first;
    fd = -1;
    while (c = *fdl, c) {
        if (c == ',') {
            if (fd != -1) {
                r_fd = malloc(sizeof(*r_fd));
                if (!r_fd) die("malloc");
                r_fd->to = fd;
                r_fd->p = NULL;

                *chain = r_fd;
                chain = &r_fd->p;

                fd = -1;
            }
        } else {
            c = c2dg(c);
            if (c == -1) {
                err("%s: garbage in fd spec: %s", __func__, fdl);
                exit(1);
            }

            fd = fd * 10 + c;
        }

        ++fdl;
    }

    if (fd != -1) {
        r_fd = malloc(sizeof(*r_fd));
        if (!r_fd) die("malloc");
        r_fd->to = fd;
        r_fd->p = NULL;

        *chain = r_fd;
        chain = &r_fd->p;
    }

    return first;
}

static void create_pipes(struct relay_fd *r_fds)
{
    int rc;
    do {
        rc = pipe(r_fds->pipe);
        if (rc == -1) die("pipe");
    } while (r_fds = r_fds->p, r_fds);
}

static void Write(int fd, char *p, char *e)
{
    ssize_t nw;

    while (p < e) {
        nw = write(fd, p, e - p);
        if (nw == -1) die("write");

        p += nw;
    }
}

static int has_syslog_hdr(char *l)
{
    unsigned char c;
    char *ll;

    ll = l;
    while (c = *ll, c != '[' && isprint(c) && !isspace(c))
        ++ll;
    if (c != '[' || ll == l) return 0;

    l = ++ll;
    while (c = *ll, isdigit(c)) ++ll;
    if (c != ']' || ll == l) return 0;

    if (ll[1] != ':') return 0;
    if (ll[2]!= ' ') return 0;

    return 1;
}

static void log_line_if(char *l)
{
    if (has_syslog_hdr(l)) return;
    msg(l);
}

static void l_buf_append(char *s, char *e, struct l_buf *l_buf)
{
    char *tmp;
    size_t need, have, new_sz;

    need = e - s;
    have = l_buf->e - l_buf->p;
    if (have < need) {
        new_sz = need + (l_buf->e - l_buf->s);
        tmp = realloc(l_buf->s, new_sz);
        if (!tmp) die("realloc");

        l_buf->s = tmp;
        l_buf->e = tmp + new_sz;
        l_buf->p = tmp + have;
    }

    memcpy(l_buf->p, s, need);
    l_buf->p += need;
}

static void log_lines(char *s, size_t len, struct l_buf *l_buf)
{
    char *p, *e;
    int c;

    p = s;
    e = s + len;

    if (l_buf->p > l_buf->s) {
        do
            c = *p;
        while (c != '\n' && ++p < e);
        l_buf_append(s, p, l_buf);
        if (c != '\n') return;

        l_buf->p[-1] = 0;
        log_line_if(l_buf->s);

        l_buf->p = l_buf->s;
        s = ++p;
    }

    while (s < e) {
        do
            c = *p;
        while (c != '\n' && ++p < e);
        if (c != '\n') {
            l_buf_append(s, p, l_buf);
            return;
        }

        *p = 0;
        log_line_if(s);

        s = ++p;
    }
}

static void do_relay(struct relay_fd *r_fd)
{
    char buf[4096];
    struct l_buf l_buf;
    ssize_t nr;
    int from, to;

    from = *r_fd->pipe;
    close(r_fd->pipe[1]);
    to = r_fd->to;

    l_buf.p = l_buf.s = malloc(128);
    if (!l_buf.s) die("malloc");
    l_buf.e = l_buf.s + 128;

    do {
        nr = read(from, buf, sizeof(buf));

        if (nr > 0) {
            Write(to, buf, buf + nr);
            log_lines(buf, nr, &l_buf);
        }
    } while (nr > 0);

    if (l_buf.p > l_buf.s){
        *l_buf.p = 0;
        log_line_if(l_buf.s);
    }
}

static void *relay_thread(void *arg)
{
    unsigned running;

    do_relay(arg);

    pthread_mutex_lock(&lock);
    running = --relayers;
    pthread_mutex_unlock(&lock);

    if (!running) pthread_cond_signal(&cond);
    return NULL;
}

static void start_relayer(struct relay_fd *r_fd)
{
    pthread_t tid;
    int rc;

    rc = pthread_create(&tid, NULL, relay_thread, r_fd);
    if (rc) {
        errno = rc;
        die("pthread_create");
    }

    rc = pthread_detach(tid);
    if (rc) {
        errno = rc;
        die("pthread_detach");
    }
}

static unsigned count_relays(struct relay_fd *relays)
{
    unsigned cnt;

    cnt = 0;

    while (relays) {
        ++cnt;
        relays = relays->p;
    }

    return cnt;
}

static void run_relayers(struct relay_fd *relays)
{
    struct relay_fd *mine;

    relayers = count_relays(relays) - 1;
    mine = relays;

    while (relays = relays->p, relays)
        start_relayer(relays);

    do_relay(mine);

    pthread_mutex_lock(&lock);
    while (relayers)
        pthread_cond_wait(&cond, &lock);
}

static void run_cmd(struct relay_fd *relays, char **argv)
{
    int rc;

    while (relays) {
        rc = dup2(relays->pipe[1], relays->to);
        if (rc == -1) die("dup2");
        close(*relays->pipe);
        close(relays->pipe[1]);

        relays = relays->p;
    }

    execvp(*argv, argv);
    die("execvp");
}

/*  main */
int main(int argc, char **argv)
{
    struct relay_fd *relays;
    int c;

    init_diag("to-syslog");
    relays = def_relay;
    while (c = getopt(argc, argv, "+f"), c != -1)
        switch (c) {
        case 'f':
            relays = parse_fd_list(optarg);
            if (!relays) {
                err("%s: -f witout actual fd list: %s",
                    __func__, optarg);
                exit(1);
            }

            break;

        default:
            usage();
        }

    argv += optind;
    if (!*argv) usage();

    create_pipes(relays);
    switch (fork()) {
    case -1:
        die("fork");

    case 0:
        run_relayers(relays);
        break;

    default:
        run_cmd(relays, argv);
    }

    return 0;
}
