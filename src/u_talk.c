/*
  u-talk --- AF_UNIX communication
*/

/*  includes */
#include <stdlib.h>
#include <unistd.h>

#include "diag.h"

/*  constants */
enum {
    BUFSZ =	4096
};

/*  types */
struct my_sk {
    int sk, passive;
};

/*  routines */
static unsigned buf_sz = DEF_BUFSZ;

/*  routines */
static void usage(void)
{
    msg("Usage: u-talk [-b <bufsize>] [-p] <socket>");
    msg("    Communicate using AF_UNIX stream sockets.");
    msg("    Connect to the socket specified by <socket> and relay data");
    msg("    back and forth. If <socket> starts with '//', it refers to a ");
    msg("    socket in the Linux abstract namespace for AF_UNIX sockets.");
    msg("    The -p option can be used to demand a passive open, that is");
    msg("    creation of a server socket instead. This is intended for ");
    msg("    testing and only one client connect is supported. The socket");
    msg("    will be created with 0600 access permissions.");
    msg("    The -b option can be used to use a different I/O buffer size");
    msg("    than the default of 4096.");

    exit(1);
}

static void create_socket(char *addr, struct my_sk *my_sk)
{
    struct sockaddr_un sun;
    unsigned sun_len;
    int rc;

    rc = socket(AF_UNIX, SOCK_STREAM, 0);
    if (rc == -1) die("socket");
    my_sk->sk = rc;

    fill_sun(addr, &sun, &sun_len);

    if (my_sk->passive) {
        listen_on(my_sk->sk, &sun);
        return;
    }

    unlink(sun.sun_path);
    rc = connect(my_sk->sk, (struct sockaddr *)&sun, sun_len);
    if (rc == -1) die("connect");
}

static void init(int argc, char **argv, struct my_sk *my_sk)
{
    int c;

    my_sk->passive = 0;
    while (c = getopt(argc, argv, "+b:p"), c != -1)
        switch (c) {
        case 'b':
            buf_sz = atoi(optarg);
            break;

        case 'p':
            my_sk->passive = 1;
            break;

        default:
            usage();
        }

    argv += optind;
    if (!*argv) usage();

    create_socket(*argv, my_sk);
}

/*  main */
int main(int argc, char **argv)
{
    struct my_sk my_sk;

    init_diag("u-talk");
    init(argc, argv, &my_sk);
    return 0;
}
