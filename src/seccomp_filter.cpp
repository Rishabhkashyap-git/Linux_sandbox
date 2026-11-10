#include "seccomp_filter.h"
#include <seccomp.h>
#include <sched.h>
#include <sys/ioctl.h>
#include <cerrno>
#include <cstdio>

#ifndef CLONE_NEWTIME
#define CLONE_NEWTIME 0x00000080
#endif

static const char* ALLOWED[] = {
    // files and I/O
    "read","write","readv","writev","pread64","pwrite64","preadv","pwritev",
    "open","openat","close","close_range","lseek","dup","dup2","dup3","fcntl",
    "pipe","pipe2","sendfile","flock","fsync","fdatasync","ftruncate","truncate",
    "stat","fstat","lstat","newfstatat","statx","statfs","fstatfs","access",
    "faccessat","faccessat2","readlink","readlinkat","getdents","getdents64",
    "getcwd","chdir","fchdir","mkdir","mkdirat","rmdir","unlink","unlinkat",
    "rename","renameat","renameat2","link","linkat","symlink","symlinkat",
    "chmod","fchmod","fchmodat","umask","utimensat","utimes","futimesat",
    // memory
    "mmap","munmap","mprotect","mremap","brk","madvise",
    // processes and signals
    "execve","exit","exit_group","fork","vfork","wait4","waitid",
    "kill","tgkill","tkill","rt_sigaction","rt_sigprocmask","rt_sigreturn",
    "sigaltstack","rt_sigsuspend","pause","setpgid","getpgid","getpgrp","setsid","getsid",
    // ids and info
    "getpid","getppid","gettid","getuid","geteuid","getgid","getegid","getgroups",
    "uname","sysinfo","getrlimit","prlimit64","getrusage","times","getcpu",
    "sched_yield","sched_getaffinity",
    // time
    "nanosleep","clock_nanosleep","clock_gettime","clock_getres","gettimeofday",
    "time","setitimer","getitimer","alarm",
    // waiting and threading
    "poll","ppoll","select","pselect6","epoll_create","epoll_create1","epoll_ctl",
    "epoll_wait","epoll_pwait","eventfd","eventfd2","futex","set_tid_address",
    "set_robust_list","get_robust_list","rseq","arch_prctl","prctl","getrandom",
    "restart_syscall",
};

static const char* NETWORK[] = {
    "socket","connect","bind","listen","accept","accept4","sendto","recvfrom",
    "sendmsg","recvmsg","sendmmsg","recvmmsg","shutdown","getsockname",
    "getpeername","setsockopt","getsockopt","socketpair",
};

bool install_seccomp(const Config& cfg) {
    scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_KILL_PROCESS);
    if (!ctx) { fprintf(stderr, "seccomp_init failed\n"); return false; }

    auto allow = [&](const char* name) {
        int nr = seccomp_syscall_resolve_name(name);
        if (nr < 0) return true;
        return seccomp_rule_add(ctx, SCMP_ACT_ALLOW, nr, 0) == 0;
    };

    bool ok = true;
    for (const char* name : ALLOWED) ok = allow(name) && ok;
    if (cfg.allow_net) for (const char* name : NETWORK) ok = allow(name) && ok;

    const unsigned long NS_FLAGS = CLONE_NEWNS | CLONE_NEWCGROUP | CLONE_NEWUTS |
        CLONE_NEWIPC | CLONE_NEWUSER | CLONE_NEWPID | CLONE_NEWNET | CLONE_NEWTIME;
    ok = ok && seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(clone), 1,
                                SCMP_A0(SCMP_CMP_MASKED_EQ, NS_FLAGS, 0)) == 0;
    int clone3 = seccomp_syscall_resolve_name("clone3");
    if (clone3 >= 0)
        ok = ok && seccomp_rule_add(ctx, SCMP_ACT_ERRNO(ENOSYS), clone3, 0) == 0;
    const unsigned long IOCTLS[] = { TCGETS, TIOCGWINSZ, TIOCGPGRP, FIONREAD,
                                     FIONBIO, FIOCLEX, FIONCLEX };
    for (unsigned long req : IOCTLS)
        ok = ok && seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(ioctl), 1,
                                    SCMP_A1(SCMP_CMP_EQ, req)) == 0;

    if (!ok) { fprintf(stderr, "seccomp: failed to build filter\n"); seccomp_release(ctx); return false; }

    int rc = seccomp_load(ctx);
    seccomp_release(ctx);
    if (rc != 0) { fprintf(stderr, "seccomp_load failed: %d\n", rc); return false; }
    return true;
}