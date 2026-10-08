#include <iostream>

int main() {
    std::cout << "CPU-intensive program started.\n";

    volatile unsigned long long x = 0;

    while (true) {
        x++;
    }

    return 0;
}