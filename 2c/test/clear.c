// Dominik Matijaca 0036524568
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../defs.h"

int fd = -1;
typedef char msg_t[MESSAGE_SIZE];
msg_t buf;

void recv() {
    printf(">Waiting<\n");
    int result = read(fd, buf, MESSAGE_SIZE);
    if (result < 0) {
        perror("read");
        exit(1);
    }
    printf("Received: %s\n", buf);
}

int main() {
    fd = open("/dev/shofer", O_RDONLY);

    if (fd < 0) {
        perror("open /dev/shofer O_RDONLY");
        return 1;
    }

    for (int i = 0; i < MAX_MESSAGES; i++) {
        recv();
    }

    close(fd);

    return 0;
}
