#include "../include/sandbox.h"
#include "../include/cgroup.h"
#include "../include/filesystem.h"
#include <sched.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>

static const size_t STACK_SIZE = 1024 * 1024;

struct ChildArgs { const Config* cfg; int sync_r; int sync_w; };   // NEW

static int child_main(void* arg) {
    auto* a = static_cast<ChildArgs*>(arg);
    const Config* cfg = a->cfg;

    // NEW: wait until the parent has put us in the cgroup.
    close(a->sync_w);
    char go = 0;
    if (read(a->sync_r, &go, 1) != 1 || go != 'G') return 1;
    close(a->sync_r);

    if (sethostname("sandbox", 7) != 0) { perror("sethostname"); return 1; }
    if (!setup_filesystem(cfg->rootfs)) return 1;

    // NEW: CPU-time limit (soft = SIGXCPU warning, hard = SIGKILL).
    struct rlimit rl = { (rlim_t)cfg->cpu_seconds, (rlim_t)cfg->cpu_seconds + 1 };
    if (setrlimit(RLIMIT_CPU, &rl) != 0) { perror("setrlimit"); return 1; }

    clearenv();
    setenv("PATH", "/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin", 1);
    setenv("HOME", "/", 1);

    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(cfg->program.c_str()));
    for (const auto& s : cfg->args) argv.push_back(const_cast<char*>(s.c_str()));
    argv.push_back(nullptr);

    execvp(argv[0], argv.data());
    perror("execvp");
    return 127;
}

int run_sandbox(const Config& cfg) {
    Cgroup cg;                                                    // NEW
    if (!cg.create(cfg)) return 1;

    int fds[2];                                                   // NEW
    if (pipe(fds) != 0) { perror("pipe"); cg.destroy(); return 1; }

    std::vector<char> stack(STACK_SIZE);
    ChildArgs args{&cfg, fds[0], fds[1]};
    int flags = CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWIPC | CLONE_NEWNS | SIGCHLD;
    if (!cfg.allow_net) flags |= CLONE_NEWNET;

    pid_t pid = clone(child_main, stack.data() + STACK_SIZE, flags, &args);
    if (pid < 0) { perror("clone"); cg.destroy(); return 1; }
    close(fds[0]);

    // NEW: put the child in the cgroup, THEN tell it to go.
    if (!cg.add_process(pid)) {
        kill(pid, SIGKILL); waitpid(pid, nullptr, 0); cg.destroy(); return 1;
    }
    write(fds[1], "G", 1);
    close(fds[1]);

    auto start = std::chrono::steady_clock::now();
    int status = 0;
    bool timed_out = false;
    while (true) {
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid) break;
        if (r < 0) { perror("waitpid"); break; }
        auto secs = std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::steady_clock::now() - start).count();
        if (secs >= cfg.wall_seconds) {
            timed_out = true;
            cg.kill_all();                       // NEW: kills every process inside
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            break;
        }
        usleep(50000);
    }

    int code = 0;
    if (timed_out)                std::cout << "[result] wall-clock limit exceeded\n";
    else if (WIFEXITED(status)) { code = WEXITSTATUS(status);
                                  std::cout << "[result] exited with code " << code << "\n"; }
    else if (WIFSIGNALED(status)) { code = 128 + WTERMSIG(status);
                                  std::cout << "[result] killed by signal " << WTERMSIG(status) << "\n"; }

    // NEW: read stats BEFORE destroying the cgroup.
    std::cout << "[stats] peak memory: " << cg.peak_memory() / 1024 << " KB, "
              << "CPU: " << cg.cpu_usec() / 1000 << " ms, "
              << "OOM kills: " << cg.oom_kills() << "\n";
    cg.destroy();
    return code;
}