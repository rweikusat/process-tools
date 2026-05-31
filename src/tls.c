/*
  TLS support
*/

/*  includes */
#include <stdlib.h>
#include <string.h>

#include <openssl/err.h>

#include "bufs.h"
#include "diag.h"
#include "pipe.h"
#include "tasks.h"
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

/*  prototypes */
static void start_ssl_op_rd(struct buf *, void *);
static void start_ssl_op_wr(struct buf *, void *);
static void run_ssl_op_wr(struct buf *, void *);

static void start_read(struct tls_state *, void *);
static void other_read_task(void *);

/*  variables */
struct {
    struct ssl_op_state sts[2];
    unsigned nxt;
} ssl_op_states;

/*  routines */
/**  SSL error handling */
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

/**  BIO methods */
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

/**  SSL op execution engine */
static void run_ssl_op_rd(struct buf *buf, void *p)
{
    struct ssl_op_state *st;
    int rc;

    st = p;
    BIO_set_data(st->tls_state->rbio, buf);
    rc = st->ssl_op.fn(st->tls_state, st->ssl_op.p);

    if (rc <= 0) {
        switch (SSL_get_error(st->tls_state->ssl, rc)) {
        case SSL_ERROR_WANT_READ:
            start_ssl_op_rd(NULL, p);
            return;

        case SSL_ERROR_WANT_WRITE:
            start_ssl_op_wr(NULL, p);
            return;
        }
    }

    if (rc <= 0) ssl_error();
    st->cont.fn(st->tls_state, st->cont.p);
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
    while (rc <= 0
           && SSL_get_error(ssl, rc) == SSL_ERROR_WANT_WRITE) {
        if (buf) send_data(tls_state->me, buf);

        buf = get_buf();
        if (!buf) {
            want_buf(tls_state->me, run_ssl_op_wr, st);
            return;
        }

        BIO_set_data(wbio, buf);
        rc = st->ssl_op.fn(tls_state, st->ssl_op.p);
    }

    if (rc <= 0) ssl_error();

    if (buf) {
        BIO_set_data(wbio, NULL);

        if (buf->e > buf->s) {
            send_data(tls_state->me, buf);
            buf = NULL;
        }
    }

    if (rc == -1)
        switch (SSL_get_error(ssl, rc)) {
        case SSL_ERROR_WANT_READ:
            start_ssl_op_rd(buf, st);
            return;

        default:
            ssl_error();
        }

    st->cont.fn(tls_state, st->cont.p);
}

static void start_ssl_op_wr(struct buf *buf, void *p)
{
    struct ssl_op_state *st;

    if (!buf) {
        buf = get_buf();
        if (!buf) {
            st = p;
            want_buf(st->tls_state->me, start_ssl_op_wr, p);
            return;
        }
    }

    run_ssl_op_wr(buf, p);
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
    st->cont.p = cont_p;

    run_ssl_op_wr(NULL, st);
}

/**  SSL incoming path */
static int do_read(struct tls_state *tls_state, void *p)
{
    struct buf *buf;
    int rc;

    buf = p;
    rc = SSL_read(tls_state->ssl, buf->s, buf_data_sz);
    if (rc <= 0) return rc;

    buf->e = buf->s + rc;
    return 1;
}

static void my_read_task(void *tls_state)
{
    start_read(tls_state, NULL);
}

static void do_my_send(struct tls_state *tls_state, void *p)
{
    send_data(tls_state->other, p);
    queue_task(my_read_task, tls_state);
}

static void do_start_read(struct buf *buf, void *tls_state)
{
    start_ssl_op(tls_state,
                 do_read, buf, do_my_send, buf);
}

static void start_read(struct tls_state *tls_state, void *unused)
{
    struct buf *buf;

    (void)unused;

    buf = get_buf();
    if (!buf) {
        want_buf(tls_state->me, do_start_read, tls_state);
        return;
    }

    do_start_read(buf, tls_state);
}

/**  SSL outgoing path */
static void ssl_write_done(struct tls_state *tls_state, void *unused)
{
    (void)unused;
    queue_task(other_read_task, tls_state);
}

static int do_ssl_write(struct tls_state *tls_state, void *p)
{
    struct buf *buf;
    int rc;

    buf = p;
    do {
        rc = SSL_write(tls_state->ssl, buf->s, buf->e - buf->s);
        if (rc < 1) return rc;

        buf->s += rc;
    } while (buf->s < buf->e);

    return_buf(buf);
    return 1;
}

static void handle_other_input(struct buf *buf, void *p)
{
    start_ssl_op(p,
                 do_ssl_write, buf,
                 ssl_write_done, NULL);
}

static void other_got_buf(struct buf *buf, void *p)
{
    struct tls_state *tls_state;

    tls_state = p;
    want_data(tls_state->other, handle_other_input, buf, p);
}

static void other_read_task(void *p)
{
    struct tls_state *tls_state;
    struct buf *buf;

    tls_state = p;

    buf = get_buf();
    if (!buf) {
        want_buf(tls_state->other, other_got_buf, p);
        return;
    }

    other_got_buf(buf, tls_state);
}

/**  SSL 'both' */
static void start_first_reads(struct tls_state *tls_state, void *unused)
{
    (void)unused;

    queue_task(my_read_task, tls_state);
    queue_task(other_read_task, tls_state);
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

/**  SSL client */
static int do_connect(struct tls_state *tls_state, void *unused)
{
    (void)unused;
    return SSL_connect(tls_state->ssl);
}

static void tls_client_start(struct tls_state *tls_state)
{
    start_ssl_op(tls_state,
                 do_connect, NULL,
                 start_first_reads, NULL);
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

    tls_state->ssl = SSL_new(ctx);
    if (!tls_state->ssl) ssl_error();
    if (!getenv(NO_VERIFY)) SSL_set_verify(tls_state->ssl, SSL_VERIFY_PEER, NULL);

    tls_state->start = tls_client_start;
    shared_tls_init(me, other, tls_state);
}

/**  SSL server */
static int do_accept(struct tls_state *tls_state, void *unused)
{
    (void)unused;
    return SSL_accept(tls_state->ssl);
}

static void tls_server_start(struct tls_state *tls_state)
{
    start_ssl_op(tls_state,
                 do_accept, NULL,
                 start_first_reads, NULL);
}

void tls_server_init(struct pipe *me, struct pipe *other,
                     char *cert_file, char *key_file,
                     struct tls_state *tls_state)
{
    SSL_CTX *ctx;
    SSL *ssl;
    int rc;

    ctx = tls_state->ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx) ssl_error();

    rc = SSL_CTX_use_certificate_chain_file(ctx, cert_file);
    if (rc != 1) ssl_error();
    rc = SSL_CTX_use_PrivateKey_file(ctx, key_file, SSL_FILETYPE_PEM);
    if (rc != 1) ssl_error();
    rc = SSL_CTX_check_PrivateKey(ctx);
    if (rc != 1) ssl_error();

    ssl = tls_state->ssl = SSL_new(ctx);
    if (ssl) ssl_error();
    SSL_set_verify(ssl, SSL_VERIFY_NONE, NULL);

    tls_state=>start = tls_server_start;
    shared_tls_init(me, other, tls_state);
}
