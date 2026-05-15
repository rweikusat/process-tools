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

static char the_message[] = "This is the message!";

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
    char recvd_msg[64];
    int rc_c, rc_s;

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
    rc_s = SSL_use_certificate_file(ssl_s, "cert", SSL_FILETYPE_PEM);
    if (rc_s == 1) rc_s = SSL_use_PrivateKey_file(ssl_s, "cert", SSL_FILETYPE_PEM);
    if (rc_s != 1) {
        fputs("failed to load cert or key\n", stderr);
        exit(1);
    }

    while (rc_c = SSL_connect(ssl_c), rc_c == -1) {
        assert_want_read(ssl_c, rc_c);
        get_data(wbio_c, &data);

        put_data(&data, rbio_s);
        rc_s = SSL_accept(ssl_s);
        if (rc_s != 1) assert_want_read(ssl_s, rc_s);
        get_data(wbio_s, &data);

        put_data(&data, rbio_c);
    }

    if (rc_c != 1) {
        fputs("client handshake failed\n", stderr);
        exit(1);
    }

    if (rc_s == -1) {
        get_data(wbio_c, &data);
        put_data(&data, rbio_s);
        rc_s = SSL_accept(ssl_s);
    }
    if (rc_s != 1) {
        fputs("server handshake failed\n", stderr);
        exit(1);
    }

    rc_c = SSL_write(ssl_c, the_message, sizeof(the_message) - 1);
    if (rc_c != sizeof(the_message) - 1) {
        fprintf(stderr, "unexpected write ret %d\n", rc_c);
        exit(1);
    }

    get_data(wbio_c, &data);
    put_data(&data, rbio_s);

    rc_s = SSL_read(ssl_s, recvd_msg, sizeof(recvd_msg));
    if (rc_s != sizeof(the_message) - 1) {
        fprintf(stderr, "unexpected read ret %d\n", rc_s);
        exit(1);
    }

    recvd_msg[rc_s] = 0;
    if (strcmp(the_message, recvd_msg) != 0) {
        fputs("incorrect data recvd\n", stderr);
        exit(1);
    }

    return 0;
}
