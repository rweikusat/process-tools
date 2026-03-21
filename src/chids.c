/*
  change uid/gid and run a command
*/

/*  includes */
#include <grp.h>
#include <pwd.h>
#include <unistd.h>

#include "diag.h"

/*  routines */
static void usage(void)
{
    syslog(LOG_NOTICE, "Usage: chids [-g <group>] [-u <user>] <cmd> <arg>*");
    exit(1);
}

static void change_gid(char *group)
{
    struct group *grp;
    gid_t gid;
    int rc;

    grp = getgrnam(group);
    if (grp) gid = grp->gr_gid;
    else grp = atoi(group);

    rc = setgid(gid);
    if (rc == -1) die("setgid");
}

static void change_uid(char *user)
{
    struct passwd *pwd;
    uid_t uid;
    int rc;

    pwd = getpwnam(user);
    if (pwd) uid = pwd->pw_uid;
    else uid = atoi(user);

    rc = setuid(uid);
    if (rc == -1) die("setuid")
}

/*  main */
int main(int argc, char **argv)
{
    char *group, *user;
    int c;

    init_diag("chids");

    user = group = NULL;
    while (c = getopt(argc, argv, "u:g:"), c != -1)
        switch (c) {
        case 'g':
            group = optarg;
            break;

        case 'u':
            user = optarg;
            break;

        default:
            usage();
        }

    argv += optind;
    if (!*argv) usage();

    if (group) change_gid(group);
    if (user) change_uid(user);
    execvp(*argv, argv);

    die("execvp");
    return 0;
}
