// command for ioctl
#define HELLO_PCI_MAGIC 222  // 8bit
#define HELLO_CMD_READ  _IOR(HELLO_PCI_MAGIC, 0, int)
#define HELLO_CMD_WRITE _IOW(HELLO_PCI_MAGIC, 1, int)

#define HELLO_OFFSET 40
