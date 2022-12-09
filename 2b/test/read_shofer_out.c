// Dominik Matijaca 0036524568
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
    int fd = open("/dev/shofer_out", O_RDONLY);
    if (fd < 0) {
        perror("open /dev/shofer_out");
        return 1;
    }

    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;

    while (1) {
        int ready = poll(&pfd, 1, -1);

        if (ready < 0) {
            perror("poll");
            break;
        }

        if (ready > 0 && (pfd.revents & POLLIN)) {
            char ch;
            int avail = read(fd, &ch, 1);

            if (avail < 0) {
                perror("read");
                break;
            }

            if (avail > 0) {
                putchar(ch);
                fflush(stdout);
            }
        }
    }

    close(fd);

    return 0;
}
