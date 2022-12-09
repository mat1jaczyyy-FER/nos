#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>

int main(int argc, char** argv)  {
	if (argc < 2) {
		fprintf(stderr, "Usage: %s n\n", argv[0]);
		return 1;
	}

	unsigned long cmd = atol(argv[1]);
	if (cmd < 1 || cmd > 100) {
		fprintf(stderr, "Usage: %s n\n", argv[0]);
		fprintf(stderr, "n => [1, 100]\n");
		return 1;
	}

	int fd = open("/dev/shofer_control", O_RDONLY);
	if (fd < 0) {
		perror("open /dev/shofer_control");
		return 1;
	}

	int count = ioctl(fd, cmd);
	if (count < 0) {
		perror("ioctl error");
		return 1;
	}

	printf("ioctl returned %d\n", count);

	return 0;
}
