// test program for test_pci device
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

#include "../../custom_device/test_pci/test_pci.h"
#define DEVFILE "/dev/test_pci"

int open_device(const char* filename)
{
	int fd;

	fd = open(filename, O_RDWR);
	if(fd == -1){
		printf("can't open device: %s", filename);
		exit(1);
	}

	printf("success: open device %s\n", filename);
	return fd;
}

void close_device(int fd)
{
	if(close(fd) != 0){
		printf("can't close device");
		exit(1);
	}

	printf("success: close device\n");
}

void read_device(int fd)
{
	char buf[TEST_PIO_DATASIZE], *p;
	int ret; // ssize_t

	ret = read(fd, &buf, sizeof(buf)); 
	if(ret > 0) {
		p = buf;
		printf("read device : %d bytes read\n", ret);
		while(ret--) printf("%2d ", *p++);
	} else {
		printf("read error");
	}

	printf("\n");
}

void write_device(int fd, void *buf, unsigned int size)
{
	int ret;

	ret = write(fd, buf, size);
	if(ret < 0) {
		printf("write error");
	}
	printf("write device : %d bytes write\n", ret);
}

void portio_test(int fd)
{
	char buf[TEST_PIO_DATASIZE];
	int i;

	printf("\n---- start portio test ----\n");
	read_device(fd);
  lseek(fd,	0, SEEK_SET);

	for (i = 0; i < TEST_PIO_DATASIZE; i++) {
		buf[i] = i+10;
	}
	write_device(fd, buf, TEST_PIO_DATASIZE);
  lseek(fd,	0, SEEK_SET);

	read_device(fd);

	printf("\n---- end portio test ----\n");
}

void mmio_test(int fd)
{
	int i, retval;

	test_ioctl_data *d;
	d = malloc(sizeof(test_ioctl_data));
	if(!d) exit(1);

	printf("\n---- start mmio test ----\n");

	retval = ioctl(fd, TEST_CMD_MEMREAD, d);
	for(i = 0; i < TEST_MMIO_DATANUM; i++) {
		printf("%2d ", d->mmiodata[i]);
		d->mmiodata[i] += 10;
	}
	printf("\n");

	retval = ioctl(fd, TEST_CMD_MEMWRITE, d);
	for(i = 0; i < TEST_MMIO_DATANUM; i++) {
		d->mmiodata[i] = 0;
	}

	retval = ioctl(fd, TEST_CMD_MEMREAD, d);
	for(i = 0; i < TEST_MMIO_DATANUM; i++) {
		printf("%2d ", d->mmiodata[i]);
	}
	printf("\n");
	printf("\n---- end mmio test ----\n");

	free(d);
}

void interrupt_test(int fd)
{
	int i, retval;

	test_ioctl_data *d;
	d = malloc(sizeof(test_ioctl_data));

	if(!d) exit(1);
	printf("\n---- start interrupt test ----\n");

	ioctl(fd, TEST_CMD_DOSOMETHING, d);

	printf("\n---- end interrupt test ----\n");

	free(d);
}
void dma_test(int fd)
{
	int i, retval;

	test_ioctl_data *d;
	d = malloc(sizeof(test_ioctl_data));

	if(!d) exit(1);
	printf("\n---- start dma test ----\n");
	printf("\n---- start consistent dma test ----\n");

	for(i = 0; i < TEST_CDMA_BUFFER_NUM; i++){
		d->cdmabuf[i] = i;
	}
	ioctl(fd, TEST_CMD_CDMA_START, d);

	sleep(1);
	ioctl(fd, TEST_CMD_GET_CDMA_DATA, d);
	for(i = 0; i < TEST_CDMA_BUFFER_NUM; i++){
		printf("%3d ", d->cdmabuf[i]);
	}
	printf("\n");

	printf("\n---- end consistent dma test ----\n");

	sleep(1);
	printf("\n---- start streaming dma test ----\n");

	for(i = 0; i < TEST_SDMA_BUFFER_NUM; i++){
		d->sdmabuf[i] = i;
	}
	ioctl(fd, TEST_CMD_SDMA_START, d);

	ioctl(fd, TEST_CMD_GET_SDMA_DATA, d);
	for(i = 0; i < TEST_SDMA_BUFFER_NUM; i++){
		printf("%3d ", d->sdmabuf[i]);
	}
	printf("\n");

	printf("\n---- end streaming dma test ----\n");

	printf("\n---- end dma test ----\n");

	free(d);
}

int main(int argc, char const* argv[])
{
	int fd, i;

	fd = open_device(DEVFILE);

	portio_test(fd);
	mmio_test(fd);
	interrupt_test(fd);
	dma_test(fd);

	close_device(fd);
	return 0;
}
