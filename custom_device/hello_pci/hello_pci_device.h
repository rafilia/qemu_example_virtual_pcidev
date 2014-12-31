// command for ioctl
#define HELLO_PCI_MAGIC 222  // 8bit

// for portio
#define HELLO_CMD_READ  _IOR(HELLO_PCI_MAGIC, 0, int)
#define HELLO_CMD_WRITE _IOW(HELLO_PCI_MAGIC, 1, int)

// for mmio
#define HELLO_CMD_MEMREAD  _IOR(HELLO_PCI_MAGIC, 2, int)
#define HELLO_CMD_MEMWRITE _IOW(HELLO_PCI_MAGIC, 3, int)

// for irq test
#define HELLO_CMD_DOSOMETHING _IOW(HELLO_PCI_MAGIC, 4, int)
#define HELLO_CMD_DONESOMETHING _IOW(HELLO_PCI_MAGIC, 5, int)

// for dma test
#define HELLO_CMD_DMA_START _IOW(HELLO_PCI_MAGIC, 6, int)
#define HELLO_CMD_GET_DMA_DATA _IOR(HELLO_PCI_MAGIC, 7, int)

#define HELLO_CMD_SDMA_START _IOW(HELLO_PCI_MAGIC, 8, int)
#define HELLO_CMD_GET_SDMA_DATA _IOR(HELLO_PCI_MAGIC, 9, int)

#define HELLO_PIOOFFSET 40
#define HELLO_MEMOFFSET 80

#define HELLO_DOOFFSET 100
#define HELLO_DOWN_IRQ_OFFSET 104

#define HELLO_GET_DMA_DATA 108
#define HELLO_DMA_START 112
#define HELLO_SET_DMA_ADDR 116
#define HELLO_SET_DMA_LEN 120

#define HELLO_GET_SDMA_DATA 48
#define HELLO_SDMA_START 52
#define HELLO_SET_SDMA_ADDR 56
#define HELLO_SET_SDMA_LEN 60

// pci bar(base address register) number
#define BAR_MMIO 0
#define BAR_PIO  1

#define PCI_VENDOR_ID_HELLO 0x7000
#define PCI_DEVICE_ID_HELLO 0x0001

#define HELLO_PCI_IOSIZE 128
#define HELLO_PCI_MEMSIZE 2048

#define DATANUM 10

#define HELLO_DMA_BUFFER_SIZE 1024
// use for ioctl
typedef struct hello_ioctl_data{
	int val;
	int buf[HELLO_DMA_BUFFER_SIZE/sizeof(int)];
} hello_ioctl_data;

#define DMA_START 1
