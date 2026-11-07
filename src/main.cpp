#include "../include/config.h"
#include "../include/sandbox.h"
#include <unistd.h>
#include <iostream>

int main(int argc, char* argv[]) {
    Config cfg;
    if (!parse_args(argc, argv, cfg)) { print_usage(argv[0]); return 1; }
    if (geteuid() != 0) {
        std::cerr << "Run as root (sudo): namespaces need CAP_SYS_ADMIN.\n";
        return 1;
    }
    return run_sandbox(cfg);
}