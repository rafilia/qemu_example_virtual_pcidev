#define TEST_PCI_MAGIC 123  // 8bit

/* command for ioctl */
// for mmio
#define TEST_CMD_MEMREAD  _IOR(TEST_PCI_MAGIC, 0, int)
#define TEST_CMD_MEMWRITE _IOW(TEST_PCI_MAGIC, 1, int)

// for irq test
#define TEST_CMD_DOSOMETHING _IOW(TEST_PCI_MAGIC, 2, int)
#define TEST_CMD_DONESOMETHING _IOW(TEST_PCI_MAGIC, 3, int)

// for dma test
#define TEST_CMD_CDMA_START _IOW(TEST_PCI_MAGIC, 4, int)
#define TEST_CMD_GET_CDMA_DATA _IOR(TEST_PCI_MAGIC, 5, int)

#define TEST_CMD_SDMA_START _IOW(TEST_PCI_MAGIC, 6, int)
#define TEST_CMD_GET_SDMA_DATA _IOR(TEST_PCI_MAGIC, 7, int)

/* address offset */
#define TEST_PIO 0
#define TEST_MEM 0

#define TEST_SET_INTMASK 20
#define TEST_GET_INTMASK 21

#define TEST_DO 100
#define TEST_DOWN_IRQ 104

#define TEST_GET_CDMA_DATA 108
#define TEST_CDMA_START 112
#define TEST_SET_CDMA_ADDR 116
#define TEST_SET_CDMA_LEN 120

#define TEST_GET_SDMA_DATA 48
#define TEST_SDMA_START 52
#define TEST_SET_SDMA_ADDR 56
#define TEST_SET_SDMA_LEN 60

// PCI bar(base address register) number
#define BAR_MMIO 0
#define BAR_PIO  1

#define PCI_VENDOR_ID_TEST 0x1234
#define PCI_DEVICE_ID_TEST 0x0001

#define TEST_PCI_IOSIZE 128
#define TEST_PCI_MEMSIZE 2048

#define TEST_PIO_DATASIZE 6 
#define TEST_MMIO_DATASIZE 100
#define TEST_MMIO_DATANUM TEST_MMIO_DATASIZE/4

#define TEST_CDMA_BUFFER_SIZE 1024 // bytes , for consistent DMA 
#define TEST_CDMA_BUFFER_NUM  TEST_CDMA_BUFFER_SIZE/4 

#define TEST_SDMA_BUFFER_SIZE 1024 // bytes , for streaming DMA
#define TEST_SDMA_BUFFER_NUM  TEST_SDMA_BUFFER_SIZE/4 

// intterupt mask
#define INT_DO 1
#define INT_CDMA 2
#define INT_SDMA 4

#define CDMA_START 1
#define SDMA_START 1

// use for ioctl
typedef struct hello_ioctl_data{
	int mmiodata[TEST_MMIO_DATANUM];

	int cdmabuf[TEST_CDMA_BUFFER_NUM];
	int sdmabuf[TEST_SDMA_BUFFER_NUM];
} test_ioctl_data;
