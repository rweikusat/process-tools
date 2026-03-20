/*
  sanitize environment
*/

/*  includes */
#include <alloca.h>
#include <pwd.h>
#include <string.h>
#include <unistd.h>

#include "alloc.h"
#include "diag.h"

/*  macros */
#define DEF_PATH	"PATH=/usr/local/sbin:/sbin:/usr/sbin:/usr/local/bin:/bin:/usr/bin"
#define HOME		"HOME"
#define LOGNAME		"LOGNAME"
#define PATH		"PATH"
#define USER		"USER"
#define S_LEN(s)	(sizeof(s) - 1)

/*  types */
struct want_var {
    char *name;
    unsigned flag;
};

struct var {
    struct var *p;
    char *v;
};

/*  constants */
enum {
    F_HOME =		1,
    F_LOGNAME =		2,
    F_PATH =		4,
    F_USER =		8,

    F_UVARS =		F_HOME | F_LOGNAME | F_USER
};

/*  variables */
static struct want_var wanted[] = {
    {
        .name =		HOME,
        .flag =		F_HOME },
    {
        .name =		LOGNAME,
        .flag =		F_LOGNAME },
    {
        .name =		PATH,
        .flag =		F_PATH },
    {
        .name =		USER,
        .flag =		F_USER }
};

static unsigned n_wanted = sizeof(wanted)/sizeof(*wanted);
static unsigned have;
static struct var *vars;
static size_t n_vars;

extern char **environ;

/*  routines */
static void usage(void)
{
    syslog(LOG_NOTICE, "Usage: sane-env [-k <keep>] [-s <set>] <cmd> <arg>*");
    syslog(LOG_NOTICE, "    Both options can appear any number of times. Keep arguments name vars to keep.");
    syslog(LOG_NOTICE, "    Set arguments must be of the form <name>=<value>. Will be added.");
    exit(1);
}

static void do_wanted(char *name)
{
    unsigned ndx;

    ndx = 0;
    while (ndx < n_wanted) {
        if (strcmp(name, wanted[ndx].name) == 0) {
            have |= wanted[ndx].flag;

            --n_wanted;
            wanted[ndx] = wanted[n_wanted];
            break;
        }

        ++ndx;
    }
}

static void set_v(char *v)
{
    struct var *var;

    var = alloc(sizeof(*var));
    var->v = v;

    var->p = vars;
    vars = var;

    ++n_vars;
}

static void set_nv(char *name, size_t n_len, char *val)
{
    char *v;

    v = alloc(n_len + 1 + strlen(val) + 1);
    memcpy(v, name, n_len);
    v[n_len] = '=';
    strcpy(v + n_len + 1, val);

    set_v(v);
}

static char *search_env(char *n, size_t n_len)
{
    /* avoid 'fancy' getenvs possibly allocating memory */
    char **env, *e;

    env = environ;
    while (e = *env, e) {
        if (strncmp(n, e, n_len) == 0) return e;
        ++env;
    }

    return NULL;
}

static void keep_var(char *name)
{
    size_t n_len;
    char *n, *v;

    n_len = strlen(name);
    if (!n_len) return;

    n = alloca(n_len + 2);
    memcpy(n, name, n_len);
    n[n_len] = '=';
    n[++n_len] = 0;

    v = search_env(n, n_len);
    if (!v) return;

    do_wanted(name);
    set_v(v);
}

static void set_var(char *v)
{
    char *name, *n_end;
    size_t n_len;

    n_end = strchr(v, '=');
    if (!n_end) {
        syslog(LOG_ERR, "invalid set: must be name=value, not %s",
               v);
        exit(1);
    }
    if (n_end == v) {
        syslog(LOG_ERR, "empty name in -s");
        exit(1);
    }

    n_len = n_end - v;
    name = alloca(n_len + 1);
    memcpy(name, v, n_len);
    name[n_len] = 0;
    do_wanted(name);

    set_v(v);
}

static void do_uvars(void)
{
    struct passwd *pwd;
    uid_t uid;

    uid = getuid();
    pwd = getpwuid(uid);
    if (!pwd) {
        syslog(LOG_WARNING, "found no pwd for uid %u", (unsigned)uid);
        return;
    }

    if (!(have & F_USER)) set_nv(USER, S_LEN(USER), pwd->pw_name);
    if (!(have & F_LOGNAME)) set_nv(LOGNAME, S_LEN(LOGNAME), pwd->pw_name);
    if (!(have & F_HOME)) set_nv(HOME, S_LEN(HOME), pwd->pw_dir);
}

/*  main */
int main(int argc, char **argv)
{
    char **e;
    int c;

    init_diag("sane-env");

    while (c = getopt(argc, argv, "+k:s:"), c != -1)
        switch (c) {
        case 'k':
            keep_var(optarg);
            break;

        case 's':
            set_var(optarg);
            break;

        default:
            usage();
        }

    argv += optind;
    if (!*argv) usage();

    if (!(have & F_PATH)) set_v(DEF_PATH);
    if (!((have & F_UVARS) == F_UVARS)) do_uvars();

    e = environ = alloca(sizeof(*e) * n_vars + 1);
    while (vars) {
        *e++ = vars->v;
        vars = vars->p;
    }
    *e = NULL;

    execvp(*argv, argv);
    die("execvp");

    return 0;
}
