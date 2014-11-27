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

#define DEVFILE "/dev/hello0"

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

void read_device(int fd)
{
	unsigned char buf[8], *p;
	ssize_t ret;

	ret = read(fd, buf, sizeof(buf));
	if(ret > 0) {
		p = buf;
		while(ret--) printf("%02x ", *p++);
	} else {
		perror("read error");
	}

	printf("\n");
}

void write_device(int fd, unsigned char val)
{
	ssize_t ret;

	ret = write(fd, &val, 1);
	if(ret <= 0) {
		perror("write error");
	}
}

int main(int argc, char const* argv[])
{
	int fd, i;
	// fd = open_device();
	// printf("success: open device hello \n");

	for(i = 0; i < 2; i++) {
		printf("No. %d\n", i+1);

		fd = open_device(DEVFILE);
		 
		read_device(fd);

		write_device(fd, 0x00);
		read_device(fd);

		write_device(fd, 0xab);
		read_device(fd);
	}


	// close_device(fd);
	// printf("success: close device hello \n");
	return 0;
}

