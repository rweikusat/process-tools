/*
  monitor program
*/

/*  includes */
#include <signal.h>
#include <string.h>

#include "diag.h"

/*  constants */
enum {
    DEF_GRACE = 20`             /* seconds */
};

/*  macros */
#define DEF_CTRL_PATH		"/run/__monitors__"
#define CTRL_PATH_ENV		"MONITOR_CTRL_PATH"

/*  variables */
static char *name, **cmdv;
static unsigned grace = DEF_GRACE;
static int term_sig = SIGTERM;

/*  routines */
static void usage(void)
{
    msg("Usage: monitor [-g <ctrl socket group>] "
        "[-n <name>] "
        "[-p <term grace period>] "
        "[-t <termsig>]");
    exit(1);
}

static void create_ctrl(char *name, char *grp)
{
    struct sockaddr_un sun;
    char *sysc;
    int cwd, rc, sk;
    gid_t gid;

    sk = socket(AF_UNIX, SOCK_STREM, 0);
    if (sk == -1) die("socket");

    cwd = open(".", O_PATH, 0);
    if (cwd == -1) die("open .");

    move_to_ctrl_dir();

    sun.sun_family = AF_UNIX;
    sprintf(sun.sun_path, ".tmp.%ld", getpid());
    rc = bind(sk, (struct sockaddr *)&sun,
              offsetof(struct sockaddr_un, sun_path) + strlen(sun_path) + 1);
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

    rc = rename(sun.sun_path, name);
    if (rc == 0) return;
    sysc = "rename";

err:
    unlink(sun.sun_path);
    die(sysc);
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

    if (!name) name = *cmdv;
    create_ctrl(name, ctrl_grp);
}

/*  main */
int main(int argc, char **argv)
{
    init_diag("monitor");

    init(argc, argv);

    return 0;
}
