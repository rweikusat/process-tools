/*
  sanitize environment
*/

/*  includes */
#include "diag.h"
#include <string.h>
#include <unistd.h>

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
    F_USER =		8
};

/*  variables */
static struct want_var wanted[] = {
    {
        .name =		"HOME",
        .flag =		F_HOME },
    {
        .name =		"LOGNAME",
        .flag =		F_LOGNAME },
    {
        .name =		"PATH",
        .flag =		F_PATH },
    {
        .name =		"USER",
        .flag =		F_USER }
};

static unsigned n_wanted = sizeof(wanted)/sizeof(*wanted);
static unsigned have;
static struct var *vars;

/*  routines */
static void usage(void)
{
    syslog(LOG_NOTICE, "Usage: sane-env [-k <keep>] [-s <set>] <cmd> <arg>*");
    syslog(LOG_NOTICE, "    Both options can appear any number of times. Keep arguments name vars to keep.");
    syslog(LOG_NOTICE, "    Set arguments must be of the form <name>=<value>. Will be added.");
    exit(1);
}

static void keep_var(char *name)
{
    struct var *var;
    size_t n_len;
    char *val;

    n_len = strlen(name);
    if (!n_len) return;

    val = getenv(name);
    if (!val) return;

    do_wanted(name);

    var = sbrk(sizeof(*var));
    var->p = vars;
    vars = var;

    var->v = sbrk(n_len + 1 + strlen(val) + 1);
    memcpy(var->v, name, n_len);
    var->v[n_len] = '=';
    strcpy(v->v + n_len + 1, val);
}

/*  main */
int main(int argc, char **argv)
{
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

    return 0;
}
