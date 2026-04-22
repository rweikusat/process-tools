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

/*  constants */
enum {
    ST_COPY,
    ST_ESC,
    ST_3ESC,
    ST_CSI
};

enum {
    ESC =		27,
    CSIS =		'[',    /* CSI start */
    CSI_FIN_LO =	0x40,
    CSI_FIN_HI =	0x7e
};

/*  types */
struct relay_fd {
    struct relay_fd *p;
    int pipe[2], to;
};

struct l_buf {
    char *s, *p, *e;
};

/*  variables */
static struct relay_fd def_relays[] = {
    {
        .p = def_relays + 1,
        .to = 1 },
    {
        .to = 2 }
};

static int three_esc[255] = {
    ['%'] = 1,
    ['('] = 1,
    [')'] = 1,
    [']'] = 1,
    ['#'] = 1
};

static unsigned relayers;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static void (*log_it)(char *, size_t);

/*  routines */
static void usage(void)
{
    msg("Usage: syslogging [-e] [-f <fd>[,<fd>*] [-n <name>] <cmd> <arg>*");
    msg("    Execute a command and relay output on certain file descriptors");
    msg("    (default: 1 and 2) to syslog. Captured output lines starting");
    msg("    '<word>[<pid>]: ' won't be logged as it's assumed they were");
    msg("    logged already.");
    msg("    The -e option can be used to request stripping of terminal");
    msg("    control escape sequences from lines of text before logging.");
    msg("    The -f option can be used to specify a different set of file");
    msg("    descriptors.");
    msg("    The -n option can be used to specify an alternate identifier for");
    msg("    log messages (default <cmd>).");

    exit(1);
}

static inline int c2dg(unsigned c)
{
    c -= '0';
    if (c < 10) return c;
    return -1;
}

static void add_fd_to(int fd, struct relay_fd **r_fds)
{
    struct relay_fd *r_fd;

    r_fd = malloc(sizeof(*r_fd));
    if (!r_fd) die("malloc");
    r_fd->to = fd;

    r_fd->p = *r_fds;
    *r_fds = r_fd;
}

static struct relay_fd *parse_fd_specs(char *fd_specs)
{
    struct relay_fd *r_fds;
    int fd, c;

    r_fds = NULL;
    fd = -1;
    while (c = *fd_specs, c) {
        if (c == ',') {
            if (fd != -1) {
                add_fd_to(fd, &r_fds);
                fd = -1;
            }
        } else {
            c = c2dg(c);
            if (c == -1) {
                err("%s: garbage in fd spec: %s", __func__, fd_specs);
                exit(1);
            }

            fd = fd == -1 ? c : fd * 10 + c;
        }

        ++fd_specs;
    }

    if (fd != -1) add_fd_to(fd, &r_fds);
    return r_fds;
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
    /*
      A syslog-header is considered to be

      - a sequence of printable, non-space characters
      - followed by '['
      - followed by a sequence of digits
      - followed by ']: '
    */
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

static void strip_and_log(char *l, size_t len)
{
    char *s, *p;
    int state, c;

    p = s = alloca(len);
    state = ST_COPY;

    while (c = *l, c) {
        switch (state) {
        case ST_COPY:
            if (c != ESC) *p++ = c;
            else state = ST_ESC;
            break;

        case ST_ESC:
            if (three_esc[c]) state = ST_3ESC;
            else if (c == CSIS) state = ST_CSI;
            else state = ST_COPY;
            break;

        case ST_3ESC:
            state = ST_COPY;
            break;

        case ST_CSI:
            if (c >= CSI_FIN_LO && c <= CSI_FIN_HI)
                state = ST_COPY;
        }

        ++l;
    }

    *p = 0;
    msg("%s", s);
}

static void just_log(char *l, size_t len)
{
    (void)len;
    msg("%s", l);
}

static void log_line_if(char *l, size_t len)
{
    if (has_syslog_hdr(l)) return;
    log_it(l, len);
}

static void l_buf_append(char *s, char *e, struct l_buf *l_buf)
{
    char *tmp;
    size_t need, ofs, new_sz;

    need = e - s;
    if (l_buf->e - l_buf->p < need + 1) {
        ofs = l_buf->p - l_buf->s;
        new_sz = 1 + need + (l_buf->e - l_buf->s);
        tmp = realloc(l_buf->s, new_sz);
        if (!tmp) die("realloc");

        l_buf->s = tmp;
        l_buf->e = tmp + new_sz;
        l_buf->p = tmp + ofs;
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
            c = *p++;
        while (c != '\n' && p < e);
        l_buf_append(s, p, l_buf);
        if (c != '\n') return;

        *--l_buf->p = 0;
        log_line_if(l_buf->s, l_buf->p - l_buf->s);

        l_buf->p = l_buf->s;
        s = p;
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
        log_line_if(s, p - s);

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
        log_line_if(l_buf.s, l_buf.p - l_buf.s);
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
    pthread_mutex_unlock(&lock);
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
    char *name;
    int c;

    init_diag("to-syslog");
    relays = def_relays;
    name = NULL;
    log_it = just_log;

    while (c = getopt(argc, argv, "+ef:n:"), c != -1)
        switch (c) {
        case 'e':
            log_it = strip_and_log;
            break;

        case 'f':
            relays = parse_fd_specs(optarg);
            break;

        case 'n':
            name = optarg;
            break;

        default:
            usage();
        }

    if (!relays) {
        err("%s: no file descriptors specified", __func__);
        exit(1);
    }

    argv += optind;
    if (!*argv) usage();

    create_pipes(relays);
    switch (fork()) {
    case -1:
        die("fork");

    case 0:
        closelog();
        if (!name) name = *argv;
        openlog(name, LOG_PID, LOG_USER);

        run_relayers(relays);
        break;

    default:
        run_cmd(relays, argv);
    }

    return 0;
}
