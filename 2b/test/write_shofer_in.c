// Dominik Matijaca 0036524568
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main() {
    int ch;

    int fd = open("/dev/shofer_in", O_WRONLY);
    if (fd < 0) {
        perror("open /dev/shofer_in");
        return 1;
    }

    while ((ch = getchar()) != EOF) {
        if (write(fd, &ch, 1) < 1) {
            perror("write");
            break;
        }
    }

    close(fd);

    return 0;
}
