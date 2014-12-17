// command for ioctl
#define HELLO_PCI_MAGIC 222  // 8bit

// for portio
#define HELLO_CMD_READ  _IOR(HELLO_PCI_MAGIC, 0, int)
#define HELLO_CMD_WRITE _IOW(HELLO_PCI_MAGIC, 1, int)

// for mmio
#define HELLO_CMD_MEMREAD  _IOR(HELLO_PCI_MAGIC, 2, int)
#define HELLO_CMD_MEMWRITE _IOW(HELLO_PCI_MAGIC, 3, int)

#define HELLO_PIOOFFSET 40

#define HELLO_MEMOFFSET 80

// pci bar(base address register) number
#define BAR_MMIO 0
#define BAR_PIO  1

#define PCI_VENDOR_ID_HELLO 0x7000
#define PCI_DEVICE_ID_HELLO 0x0001

#define HELLO_PCI_IOSIZE 128
#define HELLO_PCI_MEMSIZE 2048

#define DATANUM 10

// use for ioctl
typedef struct hello_ioctl_data{
	int val;
	int buf[HELLO_PCI_MEMSIZE/sizeof(int)];
} hello_ioctl_data;
