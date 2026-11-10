#include <stdlib.h>
#include <string.h>
int main(void) {
    for (;;) {                            
        char* p = malloc(1 << 20);
        if (!p) return 1;
        memset(p, 1, 1 << 20);
    }
}