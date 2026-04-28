/*
  u-talk --- AF_UNIX communication
*/

/*  includes */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <unistd.h>

#include "diag.h"

/*  macros */
#define DEF_BUFSZ	"4096"

/*  types */
struct my_sk {
    int sk, passive;
};

/*  routines */
static char *buf_sz = DEF_BUFSZ;

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

static void fill_sun(char *addr,
                     struct sockaddr_un *sun, unsigned *sun_len)
{
    size_t a_len;
    char *a_dst;

    sun->sun_family = AF_UNIX;
    a_len = strlen(addr);
    a_dst = sun->sun_path;

    if (*addr == '/' && addr[1] == '/') {
        a_len -= 2;
        addr += 2;

        *a_dst++ = 0;
    } else
        ++a_len;

    if (a_len > (sun->sun_path + sizeof(sun->sun_path)) - a_dst) {
        err("%s: addr too large", __func__);
        exit(1);
    }

    memcpy(a_dst, addr, a_len);
    *sun_len = offsetof(struct sockaddr_un, sun_path) + a_len;
}

static void create_socket(char *addr, struct my_sk *my_sk)
{
    struct sockaddr_un sun;
    unsigned sun_len;
    int rc, omask;

    rc = socket(AF_UNIX, SOCK_STREAM, 0);
    if (rc == -1) die("socket");
    my_sk->sk = rc;

    fill_sun(addr, &sun, &sun_len);

    if (my_sk->passive) {
        if (*sun.sun_path) {
            unlink(sun.sun_path);
            omask = umask(~0600);
        }

        rc = bind(my_sk->sk, (struct sockaddr *)&sun, sun_len);
        if (rc == -1) die("bind");

        if (*sun.sun_path) umask(omask);

        rc = listen(my_sk->sk, 10);
        if (rc == -1) die("listen");

        return;
    }

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
            buf_sz = optarg;
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

static void exec_relay(int sk)
{
    char sks[128];

    sprintf(sks, "%d", sk);
    execlp("relay", "relay", "0,1", sks, (void *)0);
    die("execlp");
}

/*  main */
int main(int argc, char **argv)
{
    struct my_sk my_sk;
    int acc_sk;

    init_diag("u-talk");
    init(argc, argv, &my_sk);

    if (my_sk.passive)
        while (1) {
            acc_sk = accept(my_sk.sk, NULL, NULL);
            if (acc_sk == -1) die("accept");

            switch (fork()) {
            case -1:
                die("fork");

            case 0:
                exec_relay(acc_sk);
            }

            close(acc_sk);
            wait(NULL);
        }

    exec_relay(my_sk.sk);
    return 0;
}
