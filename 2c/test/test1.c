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

void load(const char* str) {
    strncpy(buf, str, MESSAGE_SIZE - 1);
    buf[MESSAGE_SIZE - 1] = '\0';
}

void send() {
    printf("[w]  Sending: %s\n", buf);
    int result = write(fd, buf, MESSAGE_SIZE);
    if (result < 0) {
        perror("write");
        exit(1);
    }
    printf("[w]     Sent: %s\n", buf);
}

void recv() {
    printf("[r] >Waiting<\n");
    int result = read(fd, buf, MESSAGE_SIZE);
    if (result < 0) {
        perror("read");
        exit(1);
    }
    printf("[r] Received: %s\n", buf);
}

const char* messages[] = {
    "Hello, world!",
    "This is a test message.",
    "This is a very long test message. It should be longer than the buffer size, so that it will be cut off.",
    "NOS laboratorijska vježba 2c",
    "More padding messages to make the next one not fit.",
    "This message can't fit in the FIFO ;)",
    "And neither can this one!"
};

void cleanup(int sig) {
    if (fd >= 0)
        close(fd);
    
    exit(0);
}

int main() {
    memset(buf, 0, MESSAGE_SIZE);

    if (fork() == 0) {
        fd = open("/dev/shofer", O_WRONLY);
        if (fd < 0) {
            perror("open /dev/shofer O_WRONLY");
            return 1;
        }

        if (read(fd, buf, MESSAGE_SIZE) < 0)
            perror("[w] trying to read (this is expected)!");

        sleep(1);
        
        load(messages[0]);
        send();
        
        sleep(1);
        
        for (int i = 1; i < 7; i++) {
            load(messages[i]);
            send();
        }

    } else {
        fd = open("/dev/shofer", O_RDONLY);
        if (fd < 0) {
            perror("open /dev/shofer O_RDONLY");
            return 1;
        }

        sigset(SIGINT, cleanup);

        if (write(fd, buf, MESSAGE_SIZE) < 0)
            perror("[r] trying to write (this is expected)!");
        
        recv();

        sleep(2);

        for (int i = 0; i < 6; i++) {
            recv();
        }
    }

    cleanup(SIGINT);

    return 0;
}
