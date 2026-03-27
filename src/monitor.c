/*
  monitor program
*/

#define _GNU_SOURCE

/*  includes */
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
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

enum {
    CMD_STATUS,
    CMD_TERM,
    CMD_RESTART,
    CMD_SIG,
    CMD_REX,

    N_CMDS
};

/*  macros */
#define DEF_CTRL_PATH		"/run/__monitors__"
#define CTRL_PATH_ENV		"MONITOR_CTRL_PATH"
#define REX_ENV			"\x01\x02\x03herbei"

/*  types */
struct ctrl_msg {
    unsigned cmd, data;
};

typedef void handler(void);

struct handlers {
    handler *alrm, *chld, *term;
};

/*  prototypes */
static void child_running(void);
static void respawn_child(void);
static void term_child(void);
static void start_starting(void);
static void exit_monitor(void);
static void kill_child(void);
static void nop(void);

/*  variables */
static struct handlers state_handlers[] = {
    [CHILD_START] = {
        .alrm = child_running,
        .chld = respawn_child,
        .term = term_child
    },

    [CHILD_RUN] = {
        .chld = start_starting,
        .term = term_child
    },

    [CHILD_WAIT] = {
        .alrm = start_starting,
        .term = exit_monitor
    },

    [CHILD_TERM] = {
        .alrm = kill_child,
        .chld = exit_monitor,
        .term = nop
    }
};

static struct handlers handlers;

static struct {
    int listen, active;
} ctrl = {
    .active = -1
};

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

static sigjmp_buf rexec_jmp;

#define n_(x) [x] = #x

static char *cmds[] = {
    n_(CMD_STATUS),
    n_(CMD_TERM),
    n_(CMD_RESTART),
    n_(CMD_SIG),
    n_(CMD_REX)
};

static char *states[] = {
    n_(CHILD_START),
    n_(CHILD_RUN),
    n_(CHILD_TERM),
    n_(CHILD_WAIT),
    n_(CHILD_STOPPED)
};

#undef n_

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

/**  handlers & handler helpers */
/***  helpers */
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

static void switch_state_to(int state)
{
    child.state = state;
    handlers = state_handlers[state];

    msg("%s: %s(%d)", __func__, states[state], state);
}

static void handle_stopped(void)
{
    msg("%s stopped", child.name);

    child.old_state = child.state;
    child.state = CHILD_STOPPED;

    alarm(0);
    signal(SIGALRM, SIG_IGN); /* discard pending alarm */
    signal(SIGALRM, SIG_DFL);

    sigdelset(&sigs.handled, SIGTERM);
    sigdelset(&sigs.handled, SIGINT);
    sigdelset(&sigs.handled, SIGIO);
}

static void handle_continued(void)
{
    msg("%s continued", child.name);

    child.state = child.old_state;
    switch (child.state) {
    case CHILD_START:
        alarm(START_WAIT);
        break;

    case CHILD_TERM:
        alarm(term.grace);
    }

    sigaddset(&sigs.handled, SIGTERM);
    sigaddset(&sigs.handled, SIGINT);
    sigaddset(&sigs.handled, SIGIO);
}

/***  handlers */
static void start_starting(void)
{
    msg("starting %s", child.name);

    child.restarts = 0;
    alarm(START_WAIT);
    start_cmd();

    switch_state_to(CHILD_START);
}

static void child_running(void)
{
    msg("%s running", child.name);
    switch_state_to(CHILD_RUN);
}

static void respawn_child(void)
{
    if (child.restarts < MAX_RESTARTS) {
        ++child.restarts;
        start_cmd();
        return;
    }

    msg("%s respawning too fast, waiting %us", child.name,
        RESTART_WAIT);

    alarm(RESTART_WAIT);
    switch_state_to(CHILD_WAIT);
}

static void term_child(void)
{
    msg("terminating %s", child.name);

    kill(-child.pid, term.sig);
    alarm(term.grace);
    switch_state_to(CHILD_TERM);
}

static void exit_monitor(void)
{
    exit(0);
}

static void kill_child(void)
{
    msg("%s didn't terminate after %us, killing", child.name, term.grace);
    kill(-child.pid, SIGKILL);
}

static void nop(void)
{}

static void handle_chld(void)
{
    int status;

    waitpid(child.pid, &status, WUNTRACED | WCONTINUED);
    msg("%s status %d", child.name, status);

    if (WIFSTOPPED(status)) {
        handle_stopped();
        return;
    }

    if (WIFCONTINUED(status)) {
        handle_continued();
        return;
    }

    handlers.chld();
}

/**  ctrl message handling */
static int read_ctrl_msg(int sk, struct ctrl_msg *c_msg)
{
    uint8_t raw[4];
    int rc;

    rc = read(sk, raw, sizeof(raw));
    if (rc != 4) {
        msg("failed to read message");
        return -1;
    }

    if (*raw != 1) {
        msg("version mismatch, got %d wanted 1", *raw);
        return -1;
    }

    if (raw[1] >= N_CMDS) {
        msg("unknown command %d", raw[1]);
        return -1;
    }

    c_msg->cmd = raw[1];
    c_msg->data = raw[2] | raw[3] << 8;

    msg("recvd %s(%d), data %u", cmds[c_msg->cmd], c_msg->cmd, c_msg->data);
    return 0;
}

static void send_fail(int sk)
{
    uint8_t r_msg[4] = { 0 };
    write(sk, r_msg, sizeof(r_msg));

    msg("ctrl fail");
}

static void send_success(int sk)
{
    uint8_t r_msg[4] = { 1, 0 };
    write(sk, r_msg, sizeof(r_msg));

    msg("ctrl success");
}

static void finish_term_cmd(void)
{
    send_success(ctrl.active);
    exit(0);
}

static void finish_restart_cmd(void)
{
    start_starting();

    send_success(ctrl.active);
    close(ctrl.active);
    ctrl.active = -1;

    signal(SIGIO, SIG_DFL);
    raise(SIGIO);
}

static void kill_restart(void)
{
    msg("killing restart");

    send_fail(ctrl.active);
    close(ctrl.active);
    ctrl.active = -1;

    handlers.chld = exit_monitor;
}

static void handle_ctrl(void)
{
    struct ctrl_msg c_msg;
    int sk, rc;

    sk = accept4(ctrl.listen, NULL, NULL, SOCK_CLOEXEC);
    if (sk == -1) {
        if (errno == EAGAIN) return;
        die("accept");
    }

    msg("ctrl connect");

    rc = read_ctrl_msg(sk, &c_msg);
    if (rc == -1) {
        close(sk);
        return;
    }

    switch (c_msg.cmd) {
    case CMD_STATUS:
        switch (child.state) {
        case CHILD_START:
        case CHILD_RUN:
        case CHILD_WAIT:
        case CHILD_STOPPED:
            send_success(sk);
            break;

        default:
            send_fail(sk);
        }

        close(sk);
        sk = -1;
        break;

    case CMD_TERM:
        if (child.state == CHILD_WAIT) {
            send_success(sk);
            exit(0);
        }

        term_child();
        handlers.chld = finish_term_cmd;
        break;

    case CMD_RESTART:
        switch (child.state) {
        case CHILD_START:
        case CHILD_RUN:
            term_child();
            handlers.chld = finish_restart_cmd;
            handlers.term = kill_restart;
            break;

        case CHILD_TERM:
            send_fail(sk);
            close(sk);
            sk = -1;
            break;

        case CHILD_WAIT:
            handlers.alrm = finish_restart_cmd;
            break;
        }
        break;

    case CMD_SIG:
        switch (child.state) {
        case CHILD_START:
        case CHILD_RUN:
            kill(child.pid, c_msg.data);
            send_success(sk);
            break;

        default:
            send_fail(sk);
        }

        close(sk);
        sk = -1;
        break;

    case CMD_REX:
        siglongjmp(rexec_jmp, 1);
    }

    if (sk != -1) {
        ctrl.active = sk;
        signal(SIGIO, SIG_IGN);
        return;
    }

    raise(SIGIO);               /* must keep accepting until EAGAIN */
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
static void restore_state(char *rex)
{
    int state;

    sscanf(rex, "%d:%d", &child.pid, &state);
    switch_state_to(state);
    if (state == CHILD_START) alarm(START_WAIT);

    msg("restored state, child pid %d, child state %s(%d)",
        child.pid, states[state], state);
}

static void init(int argc, char **argv)
{
    char *ctrl_grp, *rex;
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
    ctrl.listen = create_ctrl(child.name, ctrl_grp);

    rex = getenv(REX_ENV);
    if (rex) {
        restore_state(rex);
        unsetenv(REX_ENV);
    } else
        start_starting();
}

/*  main */
int main(int argc, char **argv)
{
    int sig;

    if (sigsetjmp(rexec_jmp, 1) != 0) {
        char buf[128];

        sprintf(buf, "%d:%d", child.pid, child.state);
        setenv(REX_ENV, buf, 1);
        execvp(*argv, argv);

        die("execvp");
    }

    init_diag("monitor");
    init(argc, argv);

    while (1) {
        sigwait(&sigs.handled, &sig);
        msg("got signal %d", sig);

        switch (sig) {
        case SIGALRM:
            handlers.alrm();
            break;

        case SIGCHLD:
            handle_chld();
            break;

        case SIGINT:
        case SIGTERM:
            handlers.term();
            break;

        case SIGIO:
            handle_ctrl();
        }
    }

    return 0;
}
