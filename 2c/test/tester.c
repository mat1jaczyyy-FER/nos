// Dominik Matijaca 0036524568
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../defs.h"

typedef char msg_t[MESSAGE_SIZE];

void send(int fd, msg_t* msg) {
    write(fd, msg, MESSAGE_SIZE);
}

void recv(int fd, msg_t* msg) {
    read(fd, msg, MESSAGE_SIZE);
}

int main() {
    msg_t msg;

    if (fork() == 0) {
        int fd = open("/dev/shofer", O_RDONLY);
        if (fd < 0) {
            perror("open /dev/shofer");
            return 1;
        }
        
        recv(fd, &msg);

        printf("Received: %s", msg);

    } else {
        int fd = open("/dev/shofer", O_WRONLY);
        if (fd < 0) {
            perror("open /dev/shofer");
            return 1;
        }

        sleep(2);

        strncpy(msg, "Hello, world!", MESSAGE_SIZE - 1);
        msg[MESSAGE_SIZE - 1] = '\0';

        send(fd, &msg);
    }

    return 0;
}