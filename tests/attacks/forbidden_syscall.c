#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char** argv) {
    const char* w = argc > 1 ? argv[1] : "mount";
    long r = -1;
    struct timespec ts = {0, 0};
    if      (!strcmp(w, "mount"))   r = syscall(SYS_mount, "none", "/tmp", "tmpfs", 0, 0);
    else if (!strcmp(w, "ptrace"))  r = syscall(SYS_ptrace, 0, 0, 0, 0);
    else if (!strcmp(w, "unshare")) r = syscall(SYS_unshare, CLONE_NEWNS);
    else if (!strcmp(w, "reboot"))  r = syscall(SYS_reboot, 0, 0, 0, 0);
    else if (!strcmp(w, "settime")) r = syscall(SYS_clock_settime, 0, &ts);
    printf("%s was NOT killed (returned %ld)\n", w, r);   
    return 42;
}