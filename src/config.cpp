#include "../include/config.h"
#include <iostream>

static bool parse_size(const std::string &s,uint64_t& out){
    try{
        size_t pos;
        uint64_t n=std::stoull(s,&pos);
        std::string suffix=s.substr(pos);
        if(suffix.empty() || suffix=="B")out=n;
        else if (suffix == "K") out = n << 10;
        else if (suffix == "M") out = n << 20;
        else if (suffix == "G") out = n << 30;
        else return false;
        return true;
    } catch (...) { return false; }
}

static bool parse_int(const std::string &s,int &out){
    try{
        out=std::stoi(s);return out>0;
    }
    catch(...){return false;}
}
void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " --rootfs <dir> [--mem 64M] [--cpu 2]\n"
              << "       [--time 5] [--pids 20] [--net] -- <program> [args...]\n";
}
bool parse_args(int argc, char* argv[], Config& cfg) {
    int i = 1;
    for (; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--") { i++; break; }      
        if (a == "--net") { cfg.allow_net = true; continue; }
        if (i + 1 >= argc) return false;
        std::string v = argv[++i];
        if      (a == "--rootfs") cfg.rootfs = v;
        else if (a == "--mem")  { if (!parse_size(v, cfg.mem_bytes)) return false; }
        else if (a == "--cpu")  { if (!parse_int(v, cfg.cpu_seconds)) return false; }
        else if (a == "--time") { if (!parse_int(v, cfg.wall_seconds)) return false; }
        else if (a == "--pids") { if (!parse_int(v, cfg.max_pids)) return false; }
        else return false;                     
    }

    if (cfg.rootfs.empty() || i >= argc) return false; 
    cfg.program = argv[i];
    for (int j = i + 1; j < argc; j++) cfg.args.push_back(argv[j]);
    return true;
}