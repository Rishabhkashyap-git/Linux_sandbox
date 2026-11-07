#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct Config {
    std::string rootfs;                
    std::string program;                
    std::vector<std::string> args;   
    uint64_t mem_bytes   = 64ULL << 20; 
    int cpu_seconds      = 2;           
    int wall_seconds     = 5;         
    int max_pids         = 20;        
    bool allow_net       = false;
};

bool parse_args(int argc, char* argv[], Config& cfg);
void print_usage(const char* prog);