/*
  u-listen --- AF_UNIX communication/ passive
*/

/*  includes */
#include <fcntl.h>
#include <grp.h>
#include <signal.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include "diag.h"
#include "fill_sun.h"

/*  routines */
static void usage(void)
{
    msg("Usage: u-listen [-g <group>] [-p] <socket> [<cmd> <arg>*]");
    msg("    Create an AF_UNIX socket bound to the address <socket> and listen ");
    msg("    on it. If <socket> starts with '//', an address in the Linux abstract");
    msg("    namespace whose name is the remainder of the string will be used.");
    msg("    If the optional <cmd> argument is passed, an instance of it will be");
    msg("    executed in a forked process with stdin, stdout and stderr refering to");
    msg("    to accepted client connection for each client which connects. Otherwise");
    msg("    only one client connection can exist at any given time and data will");
    msg("    be relayed between stdin and stdout of the u-listen process and the");
    msg("    client connection.");
    msg("    The -g option can be used to specify a group for the listening socket.");
    msg("    If provided, it will be made writeable by this group.");
    msg("    The -p option can be used to request using a SOCK_SEQPACKET");
    msg("    instead of a SOCK_STREAM socket.");

    exit(1);
}

static gid_t gid_for(char *group)
{
    struct group *grp;

    grp = getgrnam(group);
    return grp ? grp->gr_gid : atoi(group);
}

static int listen_on(char *addr, int type, char *group)
{
    struct sockaddr_un sun;
    unsigned sun_len;
    mode_t mode;
    gid_t gid;
    int rc, sk;

    sk = socket(AF_UNIX, type | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (sk == -1) die("socket");

    fill_sun(addr, &sun, &sun_len);
    if (*sun.sun_path) unlink(sun.sun_path);
    rc = bind(sk, (struct sockaddr *)&sun, sun_len);
    if (rc == -1) die("bind");

    if (group) {
        gid = gid_for(group);
        rc = chown(sun.sun_path, -1, gid);
        if (rc == -1) die("chown");

        mode = 0660;
    } else
        mode = 0600;
    rc = chmod(sun.sun_path, mode);
    if (rc == -1) die("chmod");

    rc = listen(sk, 10);
    if (rc == -1) die("listen");

    return sk;
}

static int init(int argc, char **argv)
{
    char *group;
    int c, type;

    group = NULL;
    type = SOCK_STREAM;
    while (c = getopt(argc, argv, "+g:p"), c != -1)
        switch (c) {
        case 'g':
            group = optarg;
            break;

        case 'p':
            type = SOCK_SEQPACKET;
            break;

        default:
            usage();
        }

    argv += optind;
    if (!*argv) usage();

    return listen_on(*argv, type, group);
}

static void dummy(int unused)
{
    (void)unused;
}

static void enable_chld(void)
{
    struct sigaction sa;

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = dummy;

    sigaction(SIGCHLD, &sa, NULL);
}

static void setup_sigs(sigset_t *my_sigs, sigset_t *omask)
{
    sigaddset(my_sigs, SIGIO);
    sigaddset(my_sigs, SIGCHLD);
    sigprocmask(SIG_BLOCK, my_sigs, omask);

    enable_chld();
}

static void enable_async(int sk)
{
    fcntl(sk, F_SETOWN, getpid());
    fcntl(sk, F_SETFL, fcntl(sk, F_GETFL) | O_ASYNC);
}

static void exec_cmd(int sk, char **argv)
{
    int rc;

    rc = dup2(sk, 0);
    if (rc == -1) die("dup2/0");
    rc = dup2(sk, 1);
    if (rc == -1) die("dup2/1");
    rc = dup2(sk, 2);
    if (rc == -1) die("dup2/2");
    close(sk);

    execvp(*argv, argv);
    die("execvp");
}

static void do_accepts(int sk, char **argv, sigset_t *omask)
{
    int client_sk;

    while (client_sk = accept(sk, NULL, NULL), client_sk != -1) {
        switch (fork()) {
        case -1:
            die("fork");

        case 0:
            sigprocmask(SIG_SETMASK, omask, NULL);
            exec_cmd(client_sk, argv);
        }

        close(client_sk);
    }

    if (errno != EAGAIN) die("accept");
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
    sigset_t my_sigs, omask;
    int client_sk, sk, sig;

    init_diag("u-listen");

    sk = init(argc, argv);
    argv += optind + 1;
    setup_sigs(&my_sigs, &omask);
    enable_async(sk);

    while (1) {
        sigwait(&my_sigs, &sig);

        switch (sig) {
        case SIGIO:
            if (*argv) do_accepts(sk, argv, &omask);
            else {
                client_sk = accept(sk, NULL, NULL);
                if (client_sk == -1) {
                    if (errno == EAGAIN) break;
                    die("accept");
                }

                switch (fork()) {
                case -1:
                    die("fork");

                case 0:
                    sigprocmask(SIG_SETMASK, &omask, NULL);
                    exec_relay(client_sk);
                }

                close(client_sk);
                sigdelset(&my_sigs, SIGIO);
            }

            break;

        case SIGCHLD:
            do
                sig = waitpid(-1, NULL, WNOHANG);
            while (sig > 0);

            if (!*argv) {
                sigaddset(&my_sigs, SIGIO);
                raise(SIGIO);
            }
        }
    }

    return 0;
}
