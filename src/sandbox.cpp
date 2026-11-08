#include "../include/sandbox.h"
#include <cstdlib>
#include "../include/filesystem.h"
#include <sched.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <vector>

static const size_t STACK_SIZE = 1024 * 1024;  
static int child_main(void* arg) {
    const Config* cfg = static_cast<const Config*>(arg);

    if (sethostname("sandbox", 7) != 0) { perror("sethostname"); return 1; }

    if (!setup_filesystem(cfg->rootfs)) return 1;
    clearenv();
    setenv("PATH", "/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin", 1);
    setenv("HOME", "/", 1);

    std::cout << "[child] my PID inside the sandbox: " << getpid() << "\n"
              << std::flush;
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(cfg->program.c_str()));
    for (const auto& a : cfg->args) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);

    execvp(argv[0], argv.data());
    perror("execvp");    
    return 127;
}

int run_sandbox(const Config& cfg) {
    std::vector<char> stack(STACK_SIZE);

    int flags = CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWIPC | CLONE_NEWNS | SIGCHLD;
    if (!cfg.allow_net) flags |= CLONE_NEWNET;

    pid_t pid = clone(child_main, stack.data() + STACK_SIZE, flags,
                      const_cast<Config*>(&cfg));
    if (pid < 0) { perror("clone"); return 1; }
    std::cout << "[parent] child PID as seen from the host: " << pid << "\n";
    auto start = std::chrono::steady_clock::now();
    int status = 0;
    while (true) {
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid) break;
        if (r < 0) { perror("waitpid"); return 1; }

        auto secs = std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::steady_clock::now() - start).count();
        if (secs >= cfg.wall_seconds) {
            std::cout << "[parent] wall-clock limit hit, killing sandbox\n";
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            break;
        }
        usleep(50000); 
    }

    if (WIFEXITED(status)) {
        std::cout << "[parent] exited with code " << WEXITSTATUS(status) << "\n";
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        std::cout << "[parent] killed by signal " << WTERMSIG(status) << "\n";
        return 128 + WTERMSIG(status);
    }
    return 0;
}