/*
  monitor program
*/

#define _GNU_SOURCE

/*  includes */
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <stddef.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "diag.h"

/*  constants */
enum {
    DEF_GRACE = 20              /* seconds */
};

/*  macros */
#define DEF_CTRL_PATH		"/run/__monitors__"
#define CTRL_PATH_ENV		"MONITOR_CTRL_PATH"

/*  variables */
static char *name, **cmdv;
static unsigned grace = DEF_GRACE;
static int term_sig = SIGTERM;
static int ctrl_sk;
static sigset_t handled_sigs, omask;

static int my_sigs[] = {
    SIGALRM,
    SIGCHLD,
    SIGINT,
    SIGIO,
    SIGTERM,

    -1
};

/*  routines */
static void usage(void)
{
    msg("Usage: monitor [-g <ctrl socket group>] "
        "[-n <name>] "
        "[-p <term grace period>] "
        "[-t <termsig>]");
    exit(1);
}

static void move_to_ctrl_dir(void)
{
    char *path;
    mode_t omask;
    int rc;

    path = getenv(CTRL_PATH_ENV);
    if (!path) path = DEF_CTRL_PATH;

    omask = umask(0);
    rc = mkdir(path, 0711);
    if (rc == -1 && errno != EEXIST) die("mkdir");
    umask(omask);

    rc = chdir(path);
    if (rc == -1) die("chdir");
}

static gid_t gid_for(char *group)
{
    struct group *grp;

    grp = getgrnam(group);
    return grp ? grp->gr_gid : atoi(group);
}

static int create_ctrl(char *name, char *grp)
{
    struct sockaddr_un sun;
    char *sysc;
    int cwd, rc, sk;
    gid_t gid;

    sk = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sk == -1) die("socket");

    cwd = open(".", O_PATH, 0);
    if (cwd == -1) die("open .");

    move_to_ctrl_dir();

    sun.sun_family = AF_UNIX;
    sprintf(sun.sun_path, ".tmp.%ld", (long)getpid());
    rc = bind(sk, (struct sockaddr *)&sun,
              offsetof(struct sockaddr_un, sun_path) + strlen(sun.sun_path) + 1);
    if (rc == -1) die("bind");

    if (grp) {
        gid = gid_for(grp);

        rc = chown(sun.sun_path, -1, gid);
        if (rc == -1) {
            sysc = "chown";
            goto err;
        }

        rc = 0660;
    } else
        rc = 0600;
    rc = chmod(sun.sun_path, rc);
    if (rc == -1) {
        sysc = "chmod";
        goto err;
    }

    rc = listen(sk, 10);
    if (rc == -1) {
        sysc = "listen";
        goto err;
    }

    rc = rename(sun.sun_path, name);
    if (rc == -1) {
        sysc = "rename";
        goto err;
    }

    rc = fchdir(cwd);
    if (rc == -1) die("fchdir");

    return sk;

err:
    unlink(sun.sun_path);
    die(sysc);
    return 0;
}

static void dummy_handler(int)
{}

static void enable_chld(void)
{
    struct sigaction sa;

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = dummy_handler;

    sigaction(SIGCHLD, &sa, NULL);
}

static void setup_sigs(void)
{
    int *psig, sig;

    sigemptyset(&handled_sigs);
    psig = my_sigs;
    while (sig = *psig, sig != -1) {
        sigaddset(&handled_sigs, sig);
        ++psig;
    }

    sigprocmask(SIG_BLOCK, &handled_sigs, &omask);
    enable_chld();
}

static void init(int argc, char **argv)
{
    char *ctrl_grp;
    int c;

    ctrl_grp = NULL;
    while (c = getopt(argc, argv, "g:n:p:t:"), c != -1)
        switch (c) {
        case 'g':
            ctrl_grp = optarg;
            break;

        case 'n':
            name = optarg;
            break;

        case 'p':
            grace = atoi(optarg);
            break;

        case 't':
            term_sig = atoi(optarg);
            break;

        default:
            usage();
        }

    argv += optind;
    if (!*argv) usage();
    cmdv = argv;

    setup_sigs();

    if (!name) name = *cmdv;
    ctrl_sk = create_ctrl(name, ctrl_grp);
}

/*  main */
int main(int argc, char **argv)
{
    init_diag("monitor");

    init(argc, argv);

    return 0;
}
