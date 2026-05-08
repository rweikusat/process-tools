/*
  accept -- handle connections on a listening socket
*/

/*  includes */
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/ip.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include "diag.h"

/*  types */
struct log_info {
    int enab;
    char *name;
};

/*  macros */
#define DEF_NAME	"relay"

/*  routines */
static void usage(void)
{
    msg("Usage: accept [-l] [-n <name>] <fd> [<cmd> <arg>*]");
    msg("    Accept connections on the socket whose file descriptor number is <fd>.");
    msg("    If the optional <cmd> argument is passed, an instance of it will be");
    msg("    executed in a forked process with stdin, stdout and stderr referring");
    msg("    to accepted client connection for each client which connects. Otherwise");
    msg("    only one client connection can exist at any given time and data will");
    msg("    be relayed between stdin and stdout of the process and the");
    msg("    client connection.");
    msg("    The -l option can be used to request that messages about new connections");
    msg("    are logged.");
    msg("    The -n can be used to specify another name for these than <cmd> (or");
    msg("    'relay' if no <cmd> was given).");

    exit(1);
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

static void configure_socket(int sk)
{
    int rc;

    fcntl(sk, F_SETOWN, getpid());
    fcntl(sk, F_SETFL, fcntl(sk, F_GETFL) | O_ASYNC | O_NONBLOCK);

    fcntl(sk, F_SETFD, fcntl(sk, F_GETFD) | FD_CLOEXEC);

    rc = listen(sk, 10);
    if (rc == -1) die("listen");
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

static void log_conn(char *name, struct sockaddr_storage *ss, socklen_t sa_len)
{
    struct sockaddr_un *sun;
    unsigned port, ofs;
    char *sas;

    if (ss)
        switch (ss->ss_family) {
        case AF_UNIX:
            sun = (void *)ss;

            if (*sun->sun_path) sas = sun->sun_path;
            else {
                sa_len -= offsetof(struct sockaddr_un, sun_path);
                sas = alloca(sa_len + 1);
                *sas = '@';
                memcpy(sas + 1, sun->sun_path + 1, sa_len - 1);
                sas[sa_len] = 0;
            }

            msg("%s: connect from %s", name, sas);
            return;

        case AF_INET:
        case AF_INET6:
            sas = alloca(INET6_ADDRSTRLEN);

            if (ss->ss_family == AF_INET) {
                ofs = offsetof(struct sockaddr_in, sin_addr);
                port = ((struct sockaddr_in *)ss)->sin_port;
            } else {
                ofs = offsetof(struct sockaddr_in6, sin6_addr);
                port = ((struct sockaddr_in6 *)ss)->sin6_port;
            }

            inet_ntop(ss->ss_family, (char *)ss + ofs,
                      sas, INET6_ADDRSTRLEN);
            msg("%s: connect from %u@%s", name, ntohs(port), sas);
            return;

        default:
            warn("%s: cannot handle addresses of family %d", ss->ss_family);
        }

    msg("%s: connect", name);
}

static void multi_accept(int sk, char **argv, sigset_t *omask,
                         struct log_info *li)
{
    struct sockaddr_storage ss;
    socklen_t sa_len;
    int client_sk;

    while (sa_len = sizeof(ss),
           client_sk = accept(sk, (struct sockaddr *)&ss, &sa_len),
           client_sk != -1) {
        if (li->enab) log_conn(li->name, sa_len ? &ss : NULL, sa_len);

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

static void wait_for_children(void)
{
    pid_t pid;

    do
        pid = waitpid(-1, NULL, WNOHANG);
    while (pid > 0);
    if (pid == -1 && errno != ECHILD) die("waitpid");
}

static void exec_relay(int sk)
{
    char sks[128];

    sprintf(sks, "%d", sk);
    execlp("relay", "relay", "0,1", sks, (void *)0);
    die("execlp");
}

static void single_accept(int sk, char **unused, sigset_t *omask,
                          struct log_info *li)
{
    struct sockaddr_storage ss;
    socklen_t sa_len;
    int client_sk;

    (void)unused;

    sa_len = sizeof(&ss);
    client_sk = accept(sk, (struct sockaddr *)&ss, &sa_len);
    if (client_sk == -1) {
        if (errno == EAGAIN) return;
        die("accept");
    }

    if (li->enab) log_conn(li->name, sa_len ? &ss : NULL, sa_len);

    switch (fork()) {
    case -1:
        die("fork");

    case 0:
        sigprocmask(SIG_SETMASK, omask, NULL);
        exec_relay(client_sk);
    }

    close(client_sk);
    wait(NULL);
    raise(SIGIO);
}

/*  main */
int main(int argc, char **argv)
{
    sigset_t my_sigs, omask;
    struct log_info li;
    void (*do_accept)(int, char **, sigset_t *, struct log_info *);
    int c, sk, sig;

    init_diag("accept");

    li.enab = 0;
    li.name = NULL;
    while (c = getopt(argc, argv, "+ln:"), c != -1)
        switch (c) {
        case 'l':
            li.enab = 1;
            break;

        case 'n':
            li.name = optarg;
            break;

        default:
            usage();
        }

    argv += optind;
    if (!*argv) usage();
    sk = atoi(*argv);
    ++argv;

    if (!li.name) li.name = *argv ? *argv : DEF_NAME;
    setup_sigs(&my_sigs, &omask);
    configure_socket(sk);
    do_accept = *argv ? multi_accept : single_accept;

    while (1) {
        sigwait(&my_sigs, &sig);

        switch (sig) {
        case SIGIO:
            do_accept(sk, argv, &omask, &li);
            break;

        case SIGCHLD:
            wait_for_children();
        }
    }

    return 0;
}
