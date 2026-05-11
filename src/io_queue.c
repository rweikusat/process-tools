/*
  relay I/O queue
*/

/*  includes */
#include "io_queue.h"

/*  constants */
enum {
    MAX_IOS =	4
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
