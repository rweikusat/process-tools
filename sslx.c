/*
  openssl test program
*/

#include <openssl/ssl.h>

int main(void)
{
    SSL_CTX *ctx;
    SSL *ssl;
    BIO *bio;

    ctx = SSL_CTX_new(TLS_client_method());
    ssl = SSL_new(ctx);
    bio = BIO_new(BIO_s_mem());

    return 0;
}
