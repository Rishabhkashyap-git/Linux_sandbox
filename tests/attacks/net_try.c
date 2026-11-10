#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main(void) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { printf("socket(): %s -> BLOCKED\n", strerror(errno)); return 0; }
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET; a.sin_port = htons(53);
    inet_pton(AF_INET, "1.1.1.1", &a.sin_addr);
    if (connect(s, (struct sockaddr*)&a, sizeof a) == 0) { puts("ESCAPED: connected"); return 42; }
    printf("socket allowed but connect failed: %s\n", strerror(errno));
    return 0;
}