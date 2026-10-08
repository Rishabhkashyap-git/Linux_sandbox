#include <iostream>
#include <unistd.h>

int main() {

    std::cout << "Target started. PID: " << getpid() << "\n";

    for (int i = 1; i <= 10; i++) {
        std::cout << "Target running... " << i << " seconds\n";
        sleep(1);
    }

    std::cout << "Target finished normally.\n";

    return 0;
}