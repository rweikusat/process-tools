/*
  openssl test program
*/

#include <stdio.h>
#include <stdlib.h>

#include <openssl/ssl.h>

struct data {
    char *d;
    int len;
};

static void assert_want_read(SSL *ssl, int ret)
{
    int ret2;

    ret2 = SSL_get_error(ssl, ret);
    if (ret2 == SSL_ERROR_WANT_READ) return;

    fprintf(stderr, "unexpected SSL error %d\n", ret2);
    exit(1);
}

static void get_data(BIO *bio, struct data *data)
{
    int nr;

    data->len = BIO_pending(bio);
    if (!data->len) {
        fputs("no data in bio\n", stderr);
        exit(1);
    }

    data->d = malloc(data->len);
    nr = BIO_read(bio, data->d, data->len);
    if (nr != data->len) {
        fprintf(stderr, "unexpected BIO_read ret %d\n", nr);
        exit(1);
    }
}

static void put_data(struct data *data, BIO *bio)
{
    int nw;

    nw = BIO_write(bio, data->d, data->len);
    if (nw != data->len) {
        fprintf(stderr, "unexpected BIO_write ret %d\n", nw);
        exit(1);
    }

    free(data->d);
}

int main(void)
{
    SSL_CTX *ctx_c, *ctx_s;
    SSL *ssl_c, *ssl_s;
    BIO *rbio_c, *wbio_c, *rbio_s, *wbio_s;
    struct data data;
    int rc;

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

    while (rc = SSL_connect(ssl_c), rc == -1) {
        assert_want_read(ssl_c, rc);
        get_data(wbio_c, &data);
        put_data(&data, rbio_s);
    }

    return 0;
}
