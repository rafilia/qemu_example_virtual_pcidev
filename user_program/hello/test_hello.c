// test program for hello device
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define DEVFILE "/dev/hello"

int open_device()
{
	int fd;

	fd = open(DEVFILE, O_RDWR);
	if(fd == -1){
		perror("open device hello");
		exit(1);
	}

	return fd;
}

void close_device(int fd)
{
	if(close(fd) != 0){
		perror("close");
		exit(1);
	}
}

int main(int argc, char const* argv[])
{
	int fd;
	fd = open_device();
	printf("success: open device hello \n");

	close_device(fd);
	printf("success: close device hello \n");
	return 0;
}

