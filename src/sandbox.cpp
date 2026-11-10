#include "../include/sandbox.h"
#include "../include/cgroup.h"
#include "../include/filesystem.h"
#include "../include/caps.h"
#include "../include/seccomp_filter.h"
#include "../include/verdict.h"
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

struct ChildArgs { const Config* cfg; int sync_r; int sync_w; }; 

static int child_main(void* arg) {
    auto* a = static_cast<ChildArgs*>(arg);
    const Config* cfg = a->cfg;

    close(a->sync_w);
    char go = 0;
    if (read(a->sync_r, &go, 1) != 1 || go != 'G') return 1;
    close(a->sync_r);

    if (sethostname("sandbox", 7) != 0) { perror("sethostname"); return 1; }
    if (!setup_filesystem(cfg->rootfs)) return 1;

    struct rlimit rl = { (rlim_t)cfg->cpu_seconds, (rlim_t)cfg->cpu_seconds + 1 };
    if (setrlimit(RLIMIT_CPU, &rl) != 0) { perror("setrlimit"); return 1; }

    clearenv();
    setenv("PATH", "/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin", 1);
    setenv("HOME", "/", 1);

    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(cfg->program.c_str()));
    for (const auto& s : cfg->args) argv.push_back(const_cast<char*>(s.c_str()));
    argv.push_back(nullptr);

    if (!drop_privileges()) return 1;
    if (!install_seccomp(*cfg)) return 1; 

    execvp(argv[0], argv.data());
    perror("execvp");
    return 127;
}

int run_sandbox(const Config& cfg) {
    Cgroup cg;                                                 
    if (!cg.create(cfg)) return 1;

    int fds[2];                                              
    if (pipe(fds) != 0) { perror("pipe"); cg.destroy(); return 1; }

    std::vector<char> stack(STACK_SIZE);
    ChildArgs args{&cfg, fds[0], fds[1]};
    int flags = CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWIPC | CLONE_NEWNS | SIGCHLD;
    if (!cfg.allow_net) flags |= CLONE_NEWNET;

    pid_t pid = clone(child_main, stack.data() + STACK_SIZE, flags, &args);
    if (pid < 0) { perror("clone"); cg.destroy(); return 1; }
    close(fds[0]);


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
            cg.kill_all();                       
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            break;
        }
        usleep(50000);
    }
        auto end = std::chrono::steady_clock::now();

    RunResult r;
    r.timed_out = timed_out;
    r.wall_ms   = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    if (WIFEXITED(status))   { r.exited = true; r.exit_code = WEXITSTATUS(status); }
    if (WIFSIGNALED(status)) { r.term_signal = WTERMSIG(status); }

    r.cpu_ms    = cg.cpu_usec() / 1000;
    r.peak_kb   = cg.peak_memory() / 1024;
    r.oom_kills = cg.oom_kills();
    r.pids_hits = cg.pids_hits();
    cg.destroy();

    classify(r, cfg);
    print_report(r, cfg);
    return exit_code_for(r);
}