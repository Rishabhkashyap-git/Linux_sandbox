#include <unistd.h>
int main(void) {
    for (;;) {
        pid_t p = fork();
        if (p == 0) { pause(); _exit(0); }  
        if (p < 0) usleep(1000);          
    }
}