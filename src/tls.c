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

    buf = BIO_get_data(b);
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

    buf = BIO_get_data(b);
    have = buf->e - buf->s;
    if (want > have) want = have;
    memcpy(d, buf->s, want);
    buf->s += want;
    *nr = want;

    return 1;
}

static int my_bio_ctrl(BIO *b, int cmd, long larg, void *parg)
{
    msg("%s: BIO %p, cmd %d, larg %ld, parg %p,
        __func__, bio, cmd, larg, parg);
    return 0;
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

    shared_tls_init(me, other, tls_state);
}
