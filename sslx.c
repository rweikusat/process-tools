/*
  openssl test program
*/

#include <stdio.h>
#include <stdlib.h>

#include <openssl/ssl.h>

int main(void)
{
    SSL_CTX *ctx_c, *ctx_s;
    SSL *ssl_c, *ssl_s;
    BIO *rbio_c, *wbio_c, *rbio_s, *wbio_s;
    int rc, rc2;

    ctx_c = SSL_CTX_new(TLS_client_method());
    ssl_c = SSL_new(ctx_c);
    rbio_c = BIO_new(BIO_s_mem());
    wbio_c = BIO_new(BIO_s_mem());
    SSL_set_bio(ssl_c, rbio_c, wbio_c);

    ctx_s = SSL_CTX_new(TLS_server_method());
    ssl_s = SSL_new(ctx_s);
    rbio_s = BIO_new(BIO_s_mem());
    wbio_s = BIO_new(BIO_s_mem());
    SSL_set_bio(ssl_s, rbio_s, wbio_s);
    rc = SSL_use_certificate_file(ssl_s, "cert", SSL_FILETYPE_PEM);
    if (rc == 1) rc = SSL_use_PrivateKey_file(ssl_s, "cert", SSL_FILETYPE_PEM);
    if (rc != 1) {
        fputs("failed to load cert or key\n", stderr);
        exit(1);
    }

    return 0;
}
