/*
  u-listen --- AF_UNIX communication/ passive
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
    msg("Usage: u-listen [-g <group>] [-p] <socket> [<cmd> <arg>*]");
    msg("    Create a listening AF_UNIX socket bound to the address <socket>. ");
    msg("    If <socket> starts with '//', an address in the Linux abstract");
    msg("    namespace whose name is the remainder of the string will be used.");
    msg("    The accept program will be invoked to handle actual client");
    msg("    connections.");
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

/*  main */
int main(int argc, char **argv)
{
    char **accv, **p, sks[128];
    int sk;

    init_diag("u-listen");

    sk = init(argc, argv);
    ++optind;
    argv += optind;

    p = accv = alloca((3 + argc - optind) * sizeof(*accv));
    *p++ = "accept";
    sprintf(sks, "%d", sk);
    *p++ = sks;
    while (*argv) *p++ = *argv++;
    *p = NULL;

    execvp(*accv, accv);
    die("execvp");

    return 0;
}
