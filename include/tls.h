/*
  TLS support
*/
#ifndef proc_tools_tls_h
#define proc_tools_tls_h

/*  includes */
#include <openssl/bio.h>
#include <openssl/ssl.h>

/*  macros */
#define DEF_CERT_FILE	"tls-cert"
#define DEF_KEY_FILE	"tls-key"

/*  types */
struct tls_state {
    SSL_CTX *ctx;
    SSL *ssl;
    BIO *rbio, wbio;
    struct pipe *me, *other;
    void (*start)(struct tls_state *);
};

/*  routines */
void tls_client_init(struct pipe *me, struct pipe *other,
                     char *ca_path, char *ca_file,
                     struct tls_state *tls_state);

void tls_server_init(struct pipe *me, struct pipe *other,
                     char *cert_file, char *key_file,
                     struct tls_state *tls_state);

#endif
