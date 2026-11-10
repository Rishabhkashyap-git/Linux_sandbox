#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

static int bad = 0;
static void check(const char* what, int ok) {
    printf("  [%s] %s\n", ok ? " ok " : "FAIL", what);
    if (!ok) bad++;
}

static int status_has(const char* needle) {
    char buf[4096] = {0};
    int fd = open("/proc/self/status", O_RDONLY);
    if (fd < 0) return 0;
    read(fd, buf, sizeof buf - 1);
    close(fd);
    return strstr(buf, needle) != NULL;
}

int main(void) {
    char host[64] = {0};
    gethostname(host, sizeof host);

    check("cannot read /etc/shadow",                 open("/etc/shadow", O_RDONLY) < 0);
    check("cannot create a file in /",               open("/pwned", O_CREAT | O_WRONLY, 0644) < 0);
    check("host /sys/fs/cgroup is not visible",      access("/sys/fs/cgroup", F_OK) != 0);
    check("cannot become root again (setuid 0)",     setuid(0) != 0);
    check("cannot create a user namespace",          ({
        long r = syscall(SYS_clone, CLONE_NEWUSER | SIGCHLD, 0, 0, 0, 0);
        if (r == 0) _exit(0);                        // it worked: that's the failure
        r < 0; }));
    check("capabilities are all zero",               status_has("CapEff:\t0000000000000000"));
    check("no_new_privs is set",                     status_has("NoNewPrivs:\t1"));
    check("seccomp filter is active",                status_has("Seccomp:\t2"));
    check("we are PID 1 in our own PID namespace",   getpid() == 1);
    check("hostname is isolated (\"sandbox\")",      !strcmp(host, "sandbox"));

    puts(bad ? "ESCAPED" : "CONTAINED");
    return bad ? 42 : 0;
}