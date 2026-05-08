/*
  t-listen --- TCP communication/ passive
*/

/*  includes */
#include <grp.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "diag.h"
#include "fill_sun.h"

/*  routines */
static void usage(void)
{
    msg("Usage: t-listen <port>[@<addr>] [<cmd> <arg>*]");
    msg("    Create a TCP socket listening on <port>. If the optional");
    msg("    @<addr> is present, the socket will be bound to the address");
    msg("    specified by <addr>. Otherwise, the wildcard address will be");
    msg("    used. The accept program will be invoked to handle actual");
    msg("    client connections and any arguments after the first will");
    msg("    be passed to it.");

    exit(1);
}

static int listen_on(char *addr)
{
}

/*  main */
int main(int argc, char **argv)
{
    char **accv, **p, sks[128];
    int sk;

    init_diag("u-listen");

    if (argc < 2) usage();

    ++argv;
    sk = listen_on(*argv);

    ++argv;
    p = accv = alloca((3 + argc - 2) * sizeof(*accv));
    *p++ = "accept";
    sprintf(sks, "%d", sk);
    *p++ = sks;
    while (*argv) *p++ = *argv++;
    *p = NULL;

    execvp(*accv, accv);
    die("execvp");

    return 0;
}
