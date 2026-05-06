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

    init_diag("accept");

    if (argc < 2) usage();
    sk = atoi(*++argv);
    ++argv;
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
