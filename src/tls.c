/*
  TLS support
*/

/*  includes */
#include <stdlib.h>
#include <string.h>

#include <openssl/err.h>

#include "bufs.h"
#include "diag.h"
#include "tls.h"

/*  macros */
#define NO_VERIFY	"SSL_NO_VERIFY"

/*  types */
typedef int ssl_op_fn(struct tls_state *, void *);
typedef void cont_fn(struct tls_state *, void *);

struct ssl_op_state {
    struct tls_state *tls_state;

    struct {
        ssl_op_fn *fn;
        void *p;
    } ssl_op;

    struct {
        cont_fn *fn;
        void *p;
    } cont;
};

/*  variables */
struct {
    struct ssl_op_state sts[2];
    unsigned nxt;
} ssl_op_states;

/*  routines */
static int log_ssl_error(char const *s, size_t len, void *p)
{
    (void)len;
    (void)p;

    msg("%s", s);
    return 1;
}

static void ssl_error(void)
{
    ERR_print_errors_cb(log_ssl_error, NULL);
}

static int my_bio_write_ex(BIO *b, char const *d, size_t len, size_t *nw)
{
    struct buf *buf;
    size_t avail;

    BIO_clear_retry_flags(b);
    buf = BIO_get_data(b);
    if (!buf) {
        BIO_set_retry_write(b);
        return 0;
    }

    avail = buf_data_sz - (buf->e - buf->s);
    if (len > avail) len = avail;
    memcpy(buf->e, d, len);
    buf->e += len;
    *nw = len;

    return 1;
}

static int my_bio_read_ex(BIO *b, char *d, size_t want, size_t *nr)
{
    struct buf *buf;
    size_t have;

    BIO_clear_retry_flags(b);
    buf = BIO_get_data(b);
    if (!buf) {
        BIO_set_retry_read(b);
        return 0;
    }

    have = buf->e - buf->s;
    if (want > have) want = have;
    memcpy(d, buf->s, want);
    *nr = want;

    buf->s += want;
    if (buf->s == buf->e) {
        BIO_set_data(b ,NULL);
        return_buf(buf);
    }

    return 1;
}

static long my_bio_ctrl(BIO *b, int cmd, long larg, void *parg)
{
    struct buf *buf;

    if (cmd != BIO_CTRL_PENDING) {
        msg("%s: BIO %p, cmd %d, larg %ld, parg %p",
        __func__, b, cmd, larg, parg);
        return 0;
    }

    buf = BIO_get_data(b);
    return buf->e - buf->s;
}

static BIO_METHOD *my_bio_method(void)
{
    BIO_METHOD *meth;

    meth = BIO_meth_new(BIO_get_new_index() | BIO_TYPE_SOURCE_SINK,
                        "relay_bio");
    BIO_meth_set_write_ex(meth, my_bio_write_ex);
    BIO_meth_set_read_ex(meth, my_bio_read_ex);
    BIO_meth_set_ctrl(meth, my_bio_ctrl);

    return meth;
}

static void start_ssl_op_rd(struct buf *buf, void *p)
{
    struct ssl_op_state *st;

    st = p;
    if (!buf) {
        buf = get_buf();
        if (!buf) {
            want_buf(st->tls_state->me, start_ssl_op_rd, p);
            return;
        }
    }

    want_data(st->tls_state->me,
              run_ssl_op_rd, buf, p);
}

static void run_ssl_op_wr(struct buf *buf, void *p)
{
    struct ssl_op_state *st;
    struct tls_state *tls_state;
    BIO *wbio;
    SSL *ssl;
    int rc;

    st = p;
    tls_state = st->tls_state;
    ssl = tls_state->ssl;
    wbio = tls_state->wbio;

    if (buf) BIO_set_data(wbio, buf);
    rc = st->ssl_op.fn(tls_state, st->ssl_op.p);
    while (rc == -1
           && ssl_get_error(ssl, rc) == SSL_ERROR_WANT_WRITE) {
        if (buf) send_data(tls_state->me, buf);

        buf = get_buf();
        if (!buf) {
            want_buf(tls_state->me, run_ssl_op_wr, st);
            return;
        }

        BIO_set_data(wbio, buf);
        rc = st->ssl_op.fn(tls_state, st->ssl_op.p);
    }

    if (rc == 0) ssl_error();

    if (buf) {
        BIO_set_data(wbio, NULL);

        if (buf->e > buf->s) {
            send_data(tls_state->me, buf);
            buf = NULL;
        }
    }

    if (rc == -1)
        switch (ssl_get_error(ssl, rc)) {
        case SSL_ERROR_WANT_READ:
            start_ssl_op_rd(buf, st);
            return;

        default:
            ssl_error();
        }

    st->cont.fn(tls_state, st->cont.p);
}


static void start_ssl_op(struct tls_state *tls_state,
                         ssl_op_fn *ssl_op, void *ssl_op_p,
                         cont_fn *cont, void *cont_p)
{
    struct ssl_op_state *st;

    st = ssl_op_states.sts + ssl_op_states.nxt;
    ssl_op_states.nxt ^= 1;

    st->tls_state = tls_state;
    st->ssl_op.fn = ssl_op;
    st->ssl_op.p = ssl_op_p;
    st->cont.fn = cont;
    st->cont.p = cont-p;

    run_ssl_op_wr(NULL, st);
}

static void cont_accept(struct buf *buf, void *p)
{
    struct tls_state *tls_state;
    int rc;

    tls_state = p;

    BIO_set_data(tls_state->rbio, buf);

    buf = get_buf();
    BIO_set_data(tls_state->wbio, buf);

    rc = SSL_accept(tls_state->ssl);
    if (rc == 0 ||
        (rc == -1
         && ssl_get_error(tls_state->ssl, rc) != SSL_ERROR_WANT_READ))
        ssl_error();

    if (buf->e > buf->s) {
        send_data(tls_state->me, buf);
        BIUO_set_data(tls_state->wbio, NULL);

        buf = get_buf();
    }

    want_data(tls_state->me,
              rc == -1 ? cont_accept : ssl_read,
              buf, tls_state);
}

static void tls_client_start(struct tls_state *tls_state)
{
    struct buf *buf;
    int rc;

    buf = get_buf();
    BIO_set_data(tls_state->wbio, buf);

    rc = SSL_accept(tls_state->ssl);
    if (!(rc == -1
          && ssl_get_error(tls_state->ssl, rc) == SSL_ERROR_WANT_READ))
        ssl_error();

    send_data(tls_state->me, buf);

    buf = get_buf();
    want_data(tls_state->me, cont_accept, buf, tls_state);
}

static void shared_tls_init(struct pipe *me, struct pipe *other,
                            struct tls_state *tls_state)
{
    BIO_METHOD *meth;

    tls_state->me = me;
    tls_state->other = other;

    meth = my_bio_method();
    tls_state->rbio = BIO_new(meth);
    tls_state->wbio = BIO_new(meth);

    SSL_set_bio(tls_state->ssl, tls_state->rbio, tls_state->wbio);
}

void tls_client_init(struct pipe *me, struct pipe *other,
                     char *ca_path, char *ca_file,
                     struct tls_state *tls_state)
{
    SSL_CTX *ctx;
    int rc;

    ctx = tls_state->ctx = SSL_CTX_new(TLS_client_method());
    if (ctx) ssl_error();

    if (ca_path) rc = SSL_CTX_load_verify_dir(ctx, ca_path);
    else rc = SSL_CTX_set_default_verify_dir(ctx);
    if (!rc) ssl_error();

    if (ca_file) rc = SSL_CTX_load_verify_file(ctx, ca_file);
    else rc = SSL_CTX_set_default_verify_file(ctx);
    if (!rc) ssl_error();

    tls_state->ssl = SSL_new(tls_state->ctx);
    if (!tls_state->ssl) ssl_error();
    if (!getenv(NO_VERIFY)) SSL_set_verify(tls_state->ssl, SSL_VERIFY_PEER, NULL);

    tls_state->start = tls_client_start;
    shared_tls_init(me, other, tls_state);
}
