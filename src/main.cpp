#include "../include/config.h"
#include <iostream>

int main(int argc, char* argv[]) {
    Config cfg;
    if (!parse_args(argc, argv, cfg)) { print_usage(argv[0]); return 1; }
    std::cout << "rootfs=" << cfg.rootfs << " prog=" << cfg.program
              << " mem=" << cfg.mem_bytes << " cpu=" << cfg.cpu_seconds
              << " time=" << cfg.wall_seconds << " pids=" << cfg.max_pids << "\n";
}