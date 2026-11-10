#pragma once
#include "config.h"
#include <cstdint>
#include <string>
#include <sys/types.h>

class Cgroup {
public:
    bool create(const Config& cfg);  
    bool add_process(pid_t pid);   
    void kill_all();                 
    uint64_t peak_memory() const;    
    uint64_t oom_kills() const;      
    uint64_t cpu_usec() const;        
    void destroy();                  
private:
    std::string path_;
};