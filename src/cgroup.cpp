#include "../include/cgroup.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>

static const std::string ROOT = "/sys/fs/cgroup";

static bool write_file(const std::string& path, const std::string& val,
                       bool quiet = false) {
    int fd = open(path.c_str(), O_WRONLY);
    if (fd < 0 || write(fd, val.c_str(), val.size()) < 0) {
        if (!quiet) perror(path.c_str());
        if (fd >= 0) close(fd);
        return false;
    }
    close(fd);
    return true;
}


static uint64_t read_key(const std::string& path, const std::string& key) {
    std::ifstream f(path);
    std::string k; uint64_t v;
    while (f >> k >> v) if (k == key) return v;
    return 0;
}

bool Cgroup::create(const Config& cfg) {
    struct stat st;
    if (stat((ROOT + "/cgroup.controllers").c_str(), &st) != 0) {
        std::cerr << "cgroups v2 not found at " << ROOT << "\n";
        return false;
    }
    write_file(ROOT + "/cgroup.subtree_control", "+cpu +memory +pids", true);

    path_ = ROOT + "/sandbox-" + std::to_string(getpid());
    if (mkdir(path_.c_str(), 0755) != 0) { perror("mkdir cgroup"); path_.clear(); return false; }

    bool ok = write_file(path_ + "/memory.max", std::to_string(cfg.mem_bytes))
           && write_file(path_ + "/pids.max",   std::to_string(cfg.max_pids))
           && write_file(path_ + "/cpu.max",    "100000 100000");  
    write_file(path_ + "/memory.swap.max", "0", true);              
    if (!ok) { destroy(); return false; }
    return true;
}

bool Cgroup::add_process(pid_t pid) {
    return write_file(path_ + "/cgroup.procs", std::to_string(pid));
}

void Cgroup::kill_all() {
    if (!path_.empty()) write_file(path_ + "/cgroup.kill", "1", true);
}

uint64_t Cgroup::peak_memory() const {
    std::ifstream f(path_ + "/memory.peak");
    uint64_t v = 0; f >> v; return v;
}
uint64_t Cgroup::oom_kills() const   { return read_key(path_ + "/memory.events", "oom_kill"); }
uint64_t Cgroup::cpu_usec() const    { return read_key(path_ + "/cpu.stat", "usage_usec"); }
uint64_t Cgroup::pids_hits() const { return read_key(path_ + "/pids.events", "max"); }
void Cgroup::destroy() {
    if (path_.empty()) return;
    kill_all();
    for (int i = 0; i < 50; i++) {            // up to ~0.5 s
        if (rmdir(path_.c_str()) == 0) { path_.clear(); return; }
        usleep(10000);
    }
    perror("rmdir cgroup");
}