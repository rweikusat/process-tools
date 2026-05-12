/*
  relay buffer management
*/

/*  includes */
#include <sys/mman.h>

#include "bufs.h"
#include "diag.h"

/*  variables */
size_t buf_data_sz;

static char *buf_mem, *buf_mem_e;
static size_t sz;
static struct buf *bufs;

static struct {
    want_buf_cb *cb;
    void *p;
} wanters[2];

static unsigned n_wanters;

/*  routines */
struct buf *get_buf(void)
{
    struct buf *buf;

    buf = bufs;
    if (buf) bufs = buf->p;
    else {
        if (buf_mem == buf_mem_e) return NULL;

        buf = (void *)buf_mem;
        buf_mem += sz;
    }

    buf->p = NULL;
    reset_buf(buf);

    return buf;
}

void return_buf(struct buf *buf)
{
    want_buf_cb *cb;
    void *p;

    buf->s = (void *)(buf + 1);

    if (n_wanters) {
        cb = wanters[0].cb;
        p = wanters[0].p;

        --n_wanters;
        if (n_wanters) wanters[0] = wanters[1];

        cb(buf, p);
        return;
    }

    buf->p = bufs;
    bufs = buf;
}

void init_buffers(size_t buf_sz, size_t max_bufs)
{
    size_t need;

    sz = buf_sz;
    buf_data_sz = sz - sizeof(struct buf);

    need = buf_sz * max_bufs;
    buf_mem = mmap(NULL, need, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    if (buf_mem == MAP_FAILED) die("mmap");

    buf_mem_e = buf_mem + need;
}

void want_buf(want_buf_cb *cb, void *p)
{
    wanters[n_wanters].cb = cb;
    wanters[n_wanters].p = p;

    ++n_wanters;
}
