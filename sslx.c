/*
  openssl test program
*/

#include <openssl/ssl.h>

int main(void)
{
    SSL_CTX *ctx;
    SSL *ssl;
    BIO *rbio, *wbio;
    int rc, rc2;

    ctx = SSL_CTX_new(TLS_client_method());
    ssl = SSL_new(ctx);
    rbio = BIO_new(BIO_s_mem());
    wbio = BIO_new(BIO_s_mem());
    SSL_set_bio(ssl, rbio, wbio);

    rc = SSL_connect(ssl);
    rc2 = SSL_get_error(ssl, rc);

    return 0;
}
