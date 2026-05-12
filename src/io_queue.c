/*
  relay I/O queue
*/

/*  includes */
#include <errno.h>
#include <sys/poll.h>

#include "diag.h"
#include "io_queue.h"
#include "loggers.h"

/*  constants */
enum {
    MAX_IOS =	4,
    Q_QUOTA =	8
};

/*  variables */
static struct {
    struct io *first, **chain;
} io_q = {
    .chain = &io_q.first
};

/*  routines */
void queue_io(struct io *io)
{
    io->p = NULL;
    *io_q.chain = io;
    io_q.chain = &io->p;
}

static void do_poll(struct pollfd *p_fds, struct io **p_ios, unsigned *n_p,
                    int tmout)
{
    unsigned pos, n;
    int rc;

    n = *n_p;
    do
        rc = poll(p_fds, n, tmout);
    while (rc == -1 && errno == EINTR);
    switch (rc) {
    case -1:
        die("poll");

    case 0:
        return;
    }

    pos = 0;
    do {
        if (p_fds[pos].revents) {
            info("%s: %02x for %d",
                 __func__, p_fds[pos].revents, p_fds[pos].fd);

            queue_io(p_ios[pos]);

            --rc;
            --n;
            p_fds[pos] = p_fds[n];
            p_ios[pos] = p_ios[n];
        } else
            ++pos;
    } while (rc);

    *n_p = n;
}

void run_io_loop(void)
{
    struct pollfd p_fds[MAX_IOS];
    struct io *p_ios[MAX_IOS], *cur, *next;
    unsigned n_p, quota;
    int rc;

    n_p = 0;
    quota = Q_QUOTA;
    while (io_q.first || n_p) {
        cur = io_q.first;
        if (cur) {
            io_q.first = NULL;
            io_q.chain = &io_q.first;
        }

        if (n_p && !(cur && quota)) {
            do_poll(p_fds, p_ios, &n_p, cur ? 0 : -1);
            quota = Q_QUOTA;
        }

        while (cur) {
            next = cur->p;

            rc = cur->handler(cur->fd, cur);
            if (rc) {
                p_fds[n_p].fd = cur->fd;
                p_fds[n_p].events = rc;
                p_ios[n_p] = cur;

                ++n_p;
            }

            cur = next;
        }
        --quota;
    }
}
