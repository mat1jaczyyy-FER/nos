// Dominik Matijaca 0036524568
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

void errExit(const char* msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

char path[] = "/dev/shofer0";

int main(int argc, char** argv) {
    int nfds, num_open_fds;
    struct pollfd* pfds;

    num_open_fds = nfds = 6;
    pfds = calloc(nfds, sizeof(struct pollfd));
    if (pfds == NULL) errExit("malloc");

    for (int j = 0; j < nfds; j++) {
        path[11] = 0x30 + j;

        pfds[j].fd = open(path, O_RDONLY);
        if (pfds[j].fd == -1) errExit("open");

        printf("Opened \"%s\" on fd %d\n", path, pfds[j].fd);

        pfds[j].events = POLLIN;
    }

    while (num_open_fds > 0) {
        int ready;

        printf("About to poll()\n");
        ready = poll(pfds, nfds, -1);
        if (ready == -1) errExit("poll");

        printf("Ready: %d\n", ready);

        for (int j = 0; j < nfds; j++) {
            char buf[10];

            if (pfds[j].revents != 0) {
                printf(
                    "  fd=%d; events: %s%s%s\n",
                    pfds[j].fd,
                    (pfds[j].revents & POLLIN)  ? "POLLIN "  : "",
                    (pfds[j].revents & POLLHUP) ? "POLLHUP " : "",
                    (pfds[j].revents & POLLERR) ? "POLLERR " : ""
                );

                if (pfds[j].revents & POLLIN) {
                    printf("    reading from fd %d\n", pfds[j].fd);

                    ssize_t s = read(pfds[j].fd, buf, sizeof(buf));
                    if (s == -1) errExit("read");

                    printf("    read %zd bytes: %.*s\n", s, (int) s, buf);

                } else { // POLLERR | POLLHUP
                    printf("    closing fd %d\n", pfds[j].fd);
                    if (close(pfds[j].fd) == -1) errExit("close");

                    num_open_fds--;
                }
            }
        }
    }

    printf("All file descriptors closed; bye\n");
    exit(EXIT_SUCCESS);
}
