/*
  relay buffer management
*/
#ifndef proc_tools_bufs_h
#define proc_tools_bufs_h

/*  includes */
#include <stddef.h>

/*  types */
struct buf {
    struct buf *p;
    char *s, *e;
};

typedef void want_buf_cb(struct buf *, void *);

/*  variables */
extern size_t buf_data_sz;

/*  routines */
static inline void reset_buf(struct buf *buf)
{
    buf->s = buf->e = (char *)(buf + 1);
}

struct buf *get_buf(void);
void return_buf(struct buf *buf);
void init_buffers(size_t buf_sz, size_t max_bufs);
void want_buf(want_buf_cb *cb, void *p);

#endif
