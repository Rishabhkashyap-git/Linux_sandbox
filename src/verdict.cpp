#include "verdict.h"
#include <csignal>
#include <cstdio>
#include <cstring>

const char* verdict_name(Verdict v) {
    switch (v) {
        case Verdict::OK:                     return "OK";
        case Verdict::RUNTIME_ERROR:          return "RUNTIME ERROR";
        case Verdict::TIME_LIMIT_EXCEEDED:    return "TIME LIMIT EXCEEDED";
        case Verdict::MEMORY_LIMIT_EXCEEDED:  return "MEMORY LIMIT EXCEEDED";
        case Verdict::PROCESS_LIMIT_EXCEEDED: return "PROCESS LIMIT EXCEEDED";
        case Verdict::SECURITY_VIOLATION:     return "SECURITY VIOLATION";
    }
    return "UNKNOWN";
}

void classify(RunResult& r, const Config& cfg) {
    const bool clean_exit = r.exited && r.exit_code == 0;

    if (r.term_signal == SIGSYS) {                    
        r.verdict = Verdict::SECURITY_VIOLATION;
        r.reason  = "used a forbidden system call (killed by seccomp)";
    } else if (r.oom_kills > 0) {                   
        r.verdict = Verdict::MEMORY_LIMIT_EXCEEDED;
        r.reason  = "exceeded the memory limit";
    } else if (r.pids_hits > 0 && !clean_exit) {        
        r.verdict = Verdict::PROCESS_LIMIT_EXCEEDED;
        r.reason  = "tried to create more than " + std::to_string(cfg.max_pids) + " processes";
    } else if (r.timed_out) {                        
        r.verdict = Verdict::TIME_LIMIT_EXCEEDED;
        r.reason  = "ran past the wall-clock limit of " + std::to_string(cfg.wall_seconds) + " s";
    } else if (r.term_signal == SIGXCPU ||
               (r.term_signal == SIGKILL && r.cpu_ms >= (uint64_t)cfg.cpu_seconds * 1000)) {
        r.verdict = Verdict::TIME_LIMIT_EXCEEDED;
        r.reason  = "used more than " + std::to_string(cfg.cpu_seconds) + " s of CPU time";
    } else if (r.term_signal != 0) {
        r.verdict = Verdict::RUNTIME_ERROR;
        r.reason  = std::string("crashed: ") + strsignal(r.term_signal);
    } else if (r.exit_code == 127) {
        r.verdict = Verdict::RUNTIME_ERROR;
        r.reason  = "could not run the program (is the path valid inside the rootfs?)";
    } else if (r.exit_code != 0) {
        r.verdict = Verdict::RUNTIME_ERROR;
        r.reason  = "exited with non-zero code " + std::to_string(r.exit_code);
    } else {
        r.verdict = Verdict::OK;
        r.reason  = "exited normally";
    }
}

void print_report(const RunResult& r, const Config& cfg) {
    printf("\n========================================\n");
    printf(" VERDICT   : %s\n", verdict_name(r.verdict));
    printf(" Reason    : %s\n", r.reason.c_str());
    printf("----------------------------------------\n");
    printf(" Wall time : %llu ms (limit %d s)\n", (unsigned long long)r.wall_ms, cfg.wall_seconds);
    printf(" CPU time  : %llu ms (limit %d s)\n", (unsigned long long)r.cpu_ms, cfg.cpu_seconds);
    printf(" Peak mem  : %llu KB (limit %llu KB)\n",
           (unsigned long long)r.peak_kb, (unsigned long long)(cfg.mem_bytes / 1024));
    if (r.term_signal) printf(" Signal    : %d (%s)\n", r.term_signal, strsignal(r.term_signal));
    else               printf(" Exit code : %d\n", r.exit_code);
    printf("========================================\n");
}


int exit_code_for(const RunResult& r) {
    switch (r.verdict) {
        case Verdict::OK:                     return 0;
        case Verdict::RUNTIME_ERROR:          return 10;
        case Verdict::TIME_LIMIT_EXCEEDED:    return 11;
        case Verdict::MEMORY_LIMIT_EXCEEDED:  return 12;
        case Verdict::PROCESS_LIMIT_EXCEEDED: return 13;
        case Verdict::SECURITY_VIOLATION:     return 14;
    }
    return 1;
}