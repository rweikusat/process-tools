/*
  relay I/O queue
*/

/*  includes */
#include "io_queue.h"

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

void run_io_loop(void)
{
    struct pollfd p_fds[MAX_IOS];
    struct io *p_ios[MAX_IOS], *cur;
    unsigned n_p, quota;
    int rc;

    n_p = 0;
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
            rc = cur->handler(cur->fd, cur);
            if (rc) {
                p_fds[n_p].fd = cur->fd;
                p_fds[n_p].events = rc;
                p_ios[n_p] = cur;

                ++n_p;
            }

            cur = cur->p;
        }
        --quota;
    }
}
