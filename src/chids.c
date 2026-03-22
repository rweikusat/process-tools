/*
  change uid/gid and run a command
*/

/*  includes */
#include <alloca.h>
#include <grp.h>
#include <pwd.h>
#include <unistd.h>

#include "diag.h"

/*  types */
struct substr {
    struct substr *p;
    char *s, *e;
};

/*  routines */
static void usage(void)
{
    syslog(LOG_NOTICE, "Usage: chids [-g <group>] [-s <group>[:<group>*] [-u <user>] <cmd> <arg>*");
    exit(1);
}

static void change_gid(char *group)
{
    struct group *grp;
    gid_t gid;
    int rc;

    grp = getgrnam(group);
    if (grp) gid = grp->gr_gid;
    else gid = atoi(group);

    rc = setgid(gid);
    if (rc == -1) die("setgid");

    setgroups(0, NULL);
}

static gid_t gid_for(struct substr *ss)
{
    struct group *grp;

    if (ss->s == ss->e) return -1;

    *ss->e = 0;

    grp = getgrnam(ss->s);
    if (grp) gid = grp->gr_gid;
    else gid = atoi(ss->s);

    *ss->e = ':';
    return gid;
}

static void change_suppl(char *suppl)
{
    struct substr *subs, *next;
    unsigned n_suppl;
    gid_t *gids, *g, gid;
    int c, rc;

    subs = alloca(sizeof(*subs));
    subs->p = NULL;
    subs->s = suppl;

    next = NULL;
    n_suppl = 1;

    while (c = *suppl, c) {
        if (next) {
            next->p = subs;
            next->s = suppl;
            subs = next;

            next = NULL;
            ++n_suppl;
        }

        if (c == ':') {
            subs->e = suppl;
            next = alloca(sizeof(*next));
        }

        ++suppl;
    }

    g = gids = alloca(sizeof(*gids) * n_suppl);
    while (subs) {
        gid = gid_for(subs);
        if (gid != -1) *g++ = gid;

        subs = subs->p;
    }

    if (g > gids) rc = setgroups(g - gids, gids);
    else rc = setgroups(0, NULL);
    if (rc == -1) die("setgroups");
}

static void change_uid(char *user, char *group, char *suppl)
{
    struct passwd *pwd;
    uid_t uid;
    int rc;

    pwd = getpwnam(user);
    if (pwd) {
        uid = pwd->pw_uid;

        if (!group) {
            rc = setgid(pwd->pw_gid);
            if (rc == -1) die("setgid");
        }

        if (!suppl) {
            rc = initgroups(user, pwd->pw_gid);
            if (rc == -1) die("initgroups");
        }
    } else
        uid = atoi(user);

    rc = setuid(uid);
    if (rc == -1) die("setuid");
}

/*  main */
int main(int argc, char **argv)
{
    char *group, *suppl, *user;
    int c;

    init_diag("chids");

    user = suppl = group = NULL;
    while (c = getopt(argc, argv, "+g:u:s:"), c != -1)
        switch (c) {
        case 'g':
            group = optarg;
            break;

        case 'u':
            user = optarg;
            break;

        case 's':
            suppl = optarg;
            break;

        default:
            usage();
        }

    argv += optind;
    if (!*argv) usage();

    if (group) change_gid(group);
    if (suppl) change_suppl(suppl);
    if (user) change_uid(user, group, suppl);
    execvp(*argv, argv);

    die("execvp");
    return 0;
}
