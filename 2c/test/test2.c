// Dominik Matijaca 0036524568
#include <fcntl.h>
#include <signal.h>
extern int sigset(int, void (*)(int));
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../defs.h"

int fd = -1;
typedef char msg_t[MESSAGE_SIZE];
msg_t buf;
char name;

void send() {
    printf("[%c]  Sending: %s\n", name, buf);
    int result = write(fd, buf, MESSAGE_SIZE);
    if (result < 0) {
        perror("write");
        exit(1);
    }
    printf("[%c]     Sent: %s\n", name, buf);
}

void recv() {
    printf("[%c] >Waiting<\n", name);
    int result = read(fd, buf, MESSAGE_SIZE);
    if (result < 0) {
        perror("read");
        exit(1);
    }
    printf("[%c] Received: %s\n", name, buf);
}

void cleanup(int sig) {
    if (fd >= 0)
        close(fd);
    
    exit(0);
}

int main() {
    memset(buf, 0, MESSAGE_SIZE);

    if (fork() == 0) {
        name = 'w';

        fd = open("/dev/shofer", O_WRONLY);
        if (fd < 0) {
            perror("open /dev/shofer O_WRONLY");
            return 1;
        }

        for (char c = 'A'; c <= 'Z'; c++) {
            buf[0] = c;
            send();
        }

    } else {
        name = '1';
        if (fork()) name += 2;
        if (fork()) name += 1;

        sleep(1);

        fd = open("/dev/shofer", O_RDONLY);
        if (fd < 0) {
            printf("[%c] couldn't open for reading\n", name);
            perror("open /dev/shofer O_RDONLY");
            return 1;
        }

        sigset(SIGINT, cleanup);

        while (1) {
            recv();
            sleep(1);
        }
    }

    cleanup(SIGINT);

    return 0;
}
