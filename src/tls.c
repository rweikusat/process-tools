/*
  TLS support
*/

/*  includes */
#include <stdlib.h>

#include <openssl/err.h>

#include "diag.h"
#include "tls.h"

/*  macros */
#define NO_VERIFY	"SSL_NO_VERIFY"

/*  routines */
static void log_ssl_error(char const *s, size_t len, void *p)
{
    (void)len;
    (void)p;

    msg("%s", s);
}

static void ssl_error(void)
{
    ERR_print_errors_cb(log_ssl_error, NULL);
}

static void shared_tls_init(struct pipe *me, struct pipe *other,
                            struct tls_state *tls_state)
{
    BIO_METHOD *meth;

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

    ctx = tls_state->ctx = SSL_CTX_NEW(TLS_client_method());
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
