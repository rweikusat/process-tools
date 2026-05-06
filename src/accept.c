/*
  accept -- handle connections on a listening socket
*/

/*  includes */
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include "diag.h"

/*  routines */
static void usage(void)
{
    msg("Usage: accept <fd> [<cmd> <arg>*]");
    msg("    Accept connections on the socket whose file descriptor number is <fd>.");
    msg("    If the optional <cmd> argument is passed, an instance of it will be");
    msg("    executed in a forked process with stdin, stdout and stderr referring");
    msg("    to accepted client connection for each client which connects. Otherwise");
    msg("    only one client connection can exist at any given time and data will");
    msg("    be relayed between stdin and stdout of the process and the");
    msg("    client connection.");

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
    fcntl(sk, F_SETOWN, getpid());
    fcntl(sk, F_SETFL, fcntl(sk, F_GETFL) | O_ASYNC | O_NONBLOCK);

    fcntl(sk, F_SETFD, fcntl(sk, F_GETFD) | FD_CLOEXEC);
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

static void multi_accept(int sk, char **argv, sigset_t *omask)
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

static void wait_for_children(void)
{
    pid_t pid;

    do
        pid = waitpid(-1, NULL, WNOHANG);
    while (pid > 0);
    if (pid == -1) die("waitpid");
}

static void exec_relay(int sk)
{
    char sks[128];

    sprintf(sks, "%d", sk);
    execlp("relay", "relay", "0,1", sks, (void *)0);
    die("execlp");
}

static void single_accept(int sk, char **unused, sigset_t *omask)
{
    int client_sk;

    (void)unused;

    client_sk = accept(sk, NULL, NULL);
    if (client_sk == -1) {
        if (errno == EAGAIN) return;
        die("accept");
    }

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
    void (*do_accept)(int, char **, sigset_t *);
    int sk, sig;

    init_diag("accept");

    if (argc < 2) usage();
    sk = atoi(*++argv);
    ++argv;
    setup_sigs(&my_sigs, &omask);
    configure_socket(sk);
    do_accept = *argv ? multi_accept : single_accept;

    while (1) {
        sigwait(&my_sigs, &sig);

        switch (sig) {
        case SIGIO:
            do_accept(sk, argv, &omask);
            break;

        case SIGCHLD:
            wait_for_children();
        }
    }

    return 0;
}
