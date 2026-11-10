#pragma once
#include "config.h"
#include <cstdint>
#include <string>

enum class Verdict {
    OK,
    RUNTIME_ERROR,
    TIME_LIMIT_EXCEEDED,
    MEMORY_LIMIT_EXCEEDED,
    PROCESS_LIMIT_EXCEEDED,
    SECURITY_VIOLATION,
};


struct RunResult {
    bool timed_out = false;      
    bool exited = false;    
    int exit_code = 0;
    int term_signal = 0;       
    uint64_t wall_ms = 0, cpu_ms = 0, peak_kb = 0;
    uint64_t oom_kills = 0, pids_hits = 0;
    Verdict verdict = Verdict::OK;
    std::string reason;
};

void classify(RunResult& r, const Config& cfg);        
void print_report(const RunResult& r, const Config& cfg);
int  exit_code_for(const RunResult& r);   
const char* verdict_name(Verdict v);