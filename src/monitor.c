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
#include <sys/wait.h>
#include <unistd.h>

#include "diag.h"

/*  constants */
enum {
    DEF_GRACE = 20,             /* seconds */
    MAX_RESTARTS = 5,
    START_WAIT = 10,
    RESTART_WAIT = 5
};

enum {
    CHILD_START,
    CHILD_RUN,
    CHILD_TERM,
    CHILD_WAIT,
    CHILD_STOPPED
};

/*  macros */
#define DEF_CTRL_PATH		"/run/__monitors__"
#define CTRL_PATH_ENV		"MONITOR_CTRL_PATH"

/*  variables */
static int ctrl_sk;

static struct {
    char *name, **cmdv;
    pid_t pid;
    int state, old_state;
    unsigned restarts;
} child;

static struct {
    unsigned grace;
    int sig;
} term = {
    .grace = DEF_GRACE,
    .sig = SIGTERM
};

static struct {
    sigset_t handled, omask;
} sigs;

static int my_sigs[] = {
    SIGALRM,
    SIGCHLD,
    SIGINT,
    SIGIO,
    SIGTERM,

    -1
};

/*  routines */
/**  misc */
static void usage(void)
{
    msg("Usage: monitor [-g <ctrl socket group>] "
        "[-n <name>] "
        "[-p <term grace period>] "
        "[-t <termsig>]");
    exit(1);
}

/**  signal handlers & helpers */
static void start_cmd(void)
{
    child.pid = fork();
    switch (child.pid) {
    case -1:
        die("fork");

    case 0:
        setpgid(0, 0);
        sigprocmask(SIG_SETMASK, &sigs.omask, NULL);
        execvp(*child.cmdv, child.cmdv);

        die("execvp");
    }

    setpgid(child.pid, 0);
    msg("started %s, pid %ld", child.name, (long)child.pid);
}

static void start_starting(void)
{
    msg("starting %s", child.name);

    child.state = CHILD_START;
    child.restarts = 0;

    alarm(START_WAIT);
    start_cmd();
}


static void handle_ctrl(void)
{
    int sk;

    sk = accept(ctrl_sk, NULL, NULL);
    if (sk == -1) {
        if (errno == EAGAIN) return;
        die("accept");
    }

    write(sk, "Thoelke!\n", 9);
    close(sk);
}

static void handle_alrm(void)
{
    switch (child.state) {
    case CHILD_START:
        msg("%s running", child.name);

        child.restarts = 0;
        child.state = CHILD_RUN;
        break;

    case CHILD_TERM:
        msg("%s didn't terminate after %us, killing", child.name, term.grace);
        kill(-child.pid, SIGKILL);
        break;

    case CHILD_WAIT:
        start_starting();
    }
}

static void handle_chld(void)
{
    int status;

    waitpid(child.pid, &status, WUNTRACED | WCONTINUED);
    msg("%s terminated, status %d", child.name, status);

    if (WIFSTOPPED(status)) {
        msg("%s stopped", child.name);

        child.old_state = child.state;
        child.state = CHILD_STOPPED;

        alarm(0);
        return;
    }

    if (WIFCONTINUED(status)) {
        msg("%s continued", child.name);

        child.state = child.old_state;
        switch (child.state) {
        case CHILD_START:
            alarm(START_WAIT);
            break;

        case CHILD_TERM:
            alarm(term.grace);
        }

        return;
    }

    switch (child.state) {
    case CHILD_START:
        if (child.restarts < MAX_RESTARTS) {
            ++child.restarts;
            start_cmd();
        } else {
            msg("%s respawning too fast, waiting 5s", child.name);

            child.state = CHILD_WAIT;
            alarm(RESTART_WAIT);
        }
        break;

    case CHILD_RUN:
        start_starting();
        break;

    case CHILD_TERM:
        exit(0);
    }
}

static void handle_term(void)
{
    switch (child.state) {
    case CHILD_START:
    case CHILD_RUN:
        msg("terminating %s", child.name);

        kill(-child.pid, term.sig);
        child.state = CHILD_TERM;
        alarm(term.grace);
        break;

    case CHILD_TERM:
        break;

    case CHILD_WAIT:
        exit(0);
    }
}

/** ctrl socket creation */
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

    sk = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
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

    rc = fcntl(sk, F_SETOWN, getpid());
    if (rc != -1)
        rc = fcntl(sk, F_SETFL, fcntl(sk, F_GETFL) | O_ASYNC);
    if (rc == -1) die("fcntl");

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

/**  signal setup */
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

    sigemptyset(&sigs.handled);
    psig = my_sigs;
    while (sig = *psig, sig != -1) {
        sigaddset(&sigs.handled, sig);
        ++psig;
    }

    sigprocmask(SIG_BLOCK, &sigs.handled, &sigs.omask);
    enable_chld();
}

/**  general init */
static void init(int argc, char **argv)
{
    char *ctrl_grp;
    int c;

    ctrl_grp = NULL;
    while (c = getopt(argc, argv, "+g:n:p:t:"), c != -1)
        switch (c) {
        case 'g':
            ctrl_grp = optarg;
            break;

        case 'n':
            child.name = optarg;
            break;

        case 'p':
            term.grace = atoi(optarg);
            break;

        case 't':
            term.sig = atoi(optarg);
            break;

        default:
            usage();
        }

    argv += optind;
    if (!*argv) usage();
    child.cmdv = argv;

    setup_sigs();

    if (!child.name) child.name = *child.cmdv;
    ctrl_sk = create_ctrl(child.name, ctrl_grp);

    start_starting();
}


/*  main */
int main(int argc, char **argv)
{
    int sig;

    init_diag("monitor");
    init(argc, argv);

    while (1) {
        sigwait(&sigs.handled, &sig);
        msg("got signal %d", sig);

        switch (sig) {
        case SIGALRM:
            handle_alrm();
            break;

        case SIGCHLD:
            handle_chld();
            break;

        case SIGINT:
            handle_term();
            break;

        case SIGIO:
            handle_ctrl();
            break;

        case SIGTERM:
            handle_term();
        }
    }

    return 0;
}
