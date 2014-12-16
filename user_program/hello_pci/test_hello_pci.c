// test program for hello_pci device
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "../../custom_device/hello_pci/hello_pci_device.h"
#define DEVFILE "/dev/hello_pci"

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
	/* unsigned char buf[8], *p; */
	unsigned int buf[8], *p;
	ssize_t ret;

	ret = read(fd, buf, sizeof(buf)); 
	if(ret > 0) {
		p = buf;
		while(ret--) printf("%x ", *p++);
	} else {
		perror("read error");
	}

	printf("\n");
}

void write_device(int fd, unsigned char val)
{
	ssize_t ret;

	ret = write(fd, &val, 1);
	if(ret < 0) {
		perror("write error");
	}
}

typedef struct hello_ioctl_data {
	int val;
	int buf;
} hello_ioctl_data;

void ioctl_device(int fd, int cmd, int val)
{
	int ret;
	hello_ioctl_data *d;

	d = malloc(sizeof(hello_ioctl_data));
	if(!d) exit(1);

	d->val = val;
	ret = ioctl(fd, cmd, d);

	if(ret < 0) {
		perror("invalid ioctl command");
	}

	if(cmd == HELLO_CMD_READ){
		printf("%x\n", d->buf);
	}
}

int main(int argc, char const* argv[])
{
	int fd, i;
	fd = open_device();
	printf("success: open device hello \n");

	read_device(fd);

	write_device(fd, 0);

	printf("\n ioctl check \n");
	ioctl_device(fd, HELLO_CMD_READ, 0);
	ioctl_device(fd, HELLO_CMD_WRITE, 0x34567812);
	ioctl_device(fd, HELLO_CMD_READ, 0);

	ioctl_device(fd, 999, 0);

	close_device(fd);
	printf("success: close device hello \n");
	return 0;
}
