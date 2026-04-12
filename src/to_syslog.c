/*
  relay output on a set of file descriptors (default: 1 and 2)
  to syslog
*/

/*  includes */
#include <alloc.h>
#include <stdlib.h>

#include "diag.h"

/*  types */
struct relay_fd {
    struct relay_fd *p;
    int pipe[2], to;
};

/*  variables */
static struct relay_fd def_relay[] = {
    {
        .p = def_relay + 1,
        .to = 1 },
    {
        .to = 2 }
};

/*  routines */
static void usage(void)
{
    msg("Usage: to-syslog [-f <fd>[,<fd>*] <cmd> <arg>*");
    msg("    Execute a command and relay output on certain file descriptors");
    msg("    (default: 1 and 2) to syslog.");
    msg("    The -f option can be used to specify a different set of file");
    msg("    descriptors.");
    msg("    Output lines matching the Perl pattern '^\\w\\[0-9+\\]: ' won't be");
    msg("    won't be relayed as it's assumed that they were already sent to");
    msg("    syslog.");

    exit(1);
}

static inline int c2dg(unsigned c)
{
    c -= '0';
    if (c < 10) return c;
    return -1;
}

static struct relay_fd *parse_fd_list(char *fdl)
{
    struct relay_fd *first, **chain, *r_fd;
    int fd, c;

    chain = &first;
    fd = -1;
    while (c = *fdl, c) {
        if (c == ',') {
            if (fd != -1) {
                r_fd = alloc(sizeof(*r_fd));
                r_fd->to = fd;
                r_fd->p = NULL;

                *chain = r_fd;
                chain = &r_fd->p;

                fd = -1;
            }
        } else {
            c = c2dg(c);
            if (c == -1) {
                err("%s: garbage in fd spec: %s", __func__, fdl);
                exit(1);
            }

            fd = fd * 10 + c;
        }

        ++fdl;
    }

    if (fd != -1) {
        r_fd = alloc(sizeof(*r_fd));
        r_fd->to = fd;
        r_fd->p = NULL;

        *chain = r_fd;
        chain = &r_fd->p;
    }

    return first;
}

static void create_pipes(struct relay_fd *r_fds)
{
    int rc;
    do {
        rc = pipe(r_fds->pipe);
        if (rc == -1) die("pipe");
    } while (r_fds = r_fds->p, r_fds);
}

/*  main */
int main(int argc, char **argv)
{
    struct relay_fd *relays;
    int c;

    init_diag("to-syslog");
    relays = def_relay;
    while (c = getopt(argc, argv, "+f"), c != -1)
        switch (c) {
        case 'f':
            relays = parse_fd_list(optarg);
            if (!relays) {
                err("%s: -f witout actual fd list: %s",
                    __func__, optarg);
                exit(1);
            }

            break;

        default:
            usage();
        }

    argv += optind;
    if (!*argv) usage();

    create_pipes(relays);
    switch (fork()) {
    case -1:
        die("fork");

    case 0:
        run_relayers(relays);
        break;

    default:
        run_cmd(relays, argv);
    }

    return 0;
}
