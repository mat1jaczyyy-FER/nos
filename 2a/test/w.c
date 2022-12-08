// Dominik Matijaca 0036524568
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

void errExit(const char* msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

char path[] = "/dev/shofer0";
char buf[1] = {'x'};

int main(int argc, char** argv) {
    srand(time(NULL));
    int nfds, num_open_fds;
    struct pollfd* pfds;

    num_open_fds = nfds = 6;
    pfds = calloc(nfds, sizeof(struct pollfd));
    if (pfds == NULL) errExit("malloc");

    for (int j = 0; j < nfds; j++) {
        path[11] = 0x30 + j;

        pfds[j].fd = open(path, O_WRONLY);
        if (pfds[j].fd == -1) errExit("open");

        printf("Opened \"%s\" on fd %d\n", path, pfds[j].fd);

        pfds[j].events = POLLOUT;
    }

    while (num_open_fds > 0) {
        int ready;

        printf("About to poll()\n");
        ready = poll(pfds, nfds, -1);
        if (ready == -1) errExit("poll");

        printf("Ready: %d\n", ready);

        int perm[nfds];
        int ii = 0;

        for (int j = 0; j < nfds; j++) {
            if (pfds[j].revents != 0) {
                printf(
                    "  fd=%d; events: %s%s%s\n",
                    pfds[j].fd,
                    (pfds[j].revents & POLLOUT) ? "POLLOUT " : "",
                    (pfds[j].revents & POLLHUP) ? "POLLHUP " : "",
                    (pfds[j].revents & POLLERR) ? "POLLERR " : ""
                );

                if (pfds[j].revents & POLLOUT) {
                    perm[ii++] = j;

                } else { // POLLERR | POLLHUP
                    printf("    closing fd %d\n", pfds[j].fd);
                    if (close(pfds[j].fd) == -1) errExit("close");

                    num_open_fds--;
                }
            }
        }

        int j = rand() % ii;

        printf("    writing to fd %d\n", pfds[perm[j]].fd);
        
        ssize_t s = write(pfds[perm[j]].fd, buf, sizeof(buf));
        if (s == -1) errExit("write");

        printf("    wrote %zd bytes to %d: %.*s\n", s, pfds[perm[j]].fd, (int) s, buf);
    }

    printf("All file descriptors closed; bye\n");
    exit(EXIT_SUCCESS);
}
