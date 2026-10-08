#include <iostream>
#include <unistd.h>

int main() {
    std::cout << "Normal program started.\n";

    sleep(3);

    std::cout << "Normal program finished successfully.\n";

    return 0;
}