// pci character device driver for test_pci

#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <asm/dma.h>
#include <asm/current.h>
#include <asm/uaccess.h>

#include "test_pci.h"

MODULE_LICENSE("GPL");
// MODULE_AUTHOR("tm");
MODULE_DESCRIPTION("device driver for test pci");

#define DRIVER_TEST_NAME "test_pci"
#define DEVICE_TEST_NAME "test_pci_dev"

#define TEST_PCI_DRIVER_DEBUG
#ifdef  TEST_PCI_DRIVER_DEBUG
#define tprintk(fmt, ...) printk(KERN_ALERT "** (%3d) %-20s: " fmt, __LINE__,  __func__,  ## __VA_ARGS__)
#else
#define tprintk(fmt, ...) 
#endif

// max device count (minor number)
static int test_pci_devs_max = 1;

// if test_pci_major = 0 then use dynamic allocation
static unsigned int test_pci_major = 0;
// get parameter from console if needed for test_pci_major when insmod
module_param(test_pci_major, uint, 0);

static unsigned int test_pci_minor = 0; // static allocation

static dev_t test_pci_devt; // MKDEV(test_pci_major, test_pci_minor)

struct test_device_data {
	struct pci_dev *pdev;
	struct cdev *cdev;

	// for PCI pio/mmio
	unsigned long pio_base, pio_flags, pio_length;
	unsigned long mmio_base, mmio_flags, mmio_length;

	unsigned int pio_memsize;

	// for consistent/streaming dma
	dma_addr_t cdma_addr, sdma_addr;
	int cdma_len, sdma_len;
	void *cdma_buffer, *sdma_buffer;
};

static struct test_device_data *dev_data;

//-----------------------------------------------------------------
// file_operations function

ssize_t test_pci_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_ops)
{
	int retval;
	char val;
	unsigned int write_num = 0;
	/* unsigned long pio_base; */

	tprintk("count %d pos %lld\n", count, *f_ops);

	while(write_num < count) {
		tprintk("port io number:%lx\n", dev_data->pio_base+*f_ops);

		retval = get_user(val, &buf[write_num++]);
		if(retval) break; // 0 on success

		outb(val, dev_data->pio_base+*f_ops);
		(*f_ops)++;
	}

/* out: */
	return write_num;
}

// read from port io per 1 byte
ssize_t test_pci_read(struct file *filp, char __user *buf, size_t count, loff_t *f_ops)
{
	int retval;
	char val = 0;
	unsigned int read_num = 0;

	tprintk("count %d pos %lld\n", count, *f_ops);

	// read from ioport
	while (read_num < count) {
		tprintk("port io number:%lx\n", dev_data->pio_base+*f_ops);

		val = inb(dev_data->pio_base + *f_ops); // read 1 byte
		// printk("val %d\n", val);
		retval = put_user(val, &buf[read_num++]);
		if(retval) break; // 0 on success

		(*f_ops)++;
	}

/* out: */
	return read_num;
}

loff_t test_pci_llseek(struct file *filp, loff_t off, int whence)
{
	loff_t newpos =-1;

	tprintk("lseek whence:%d\n", whence);
	switch(whence) {
    case SEEK_SET:
      newpos = off;
      break;

		case SEEK_CUR:
      newpos = filp->f_pos + off;
      break;

		case SEEK_END:
      newpos = dev_data->pio_memsize + off;
      break;

    default: /* can't happen */
			return -EINVAL;
	}

	if (newpos < 0) return -EINVAL;

  filp->f_pos = newpos;
  return newpos;
}

long test_pci_uioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	char *mmio_addr;
	int data[TEST_MMIO_DATANUM];
	test_ioctl_data *d = arg;
	int n;

	mmio_addr = ioremap(dev_data->mmio_base, TEST_PCI_MEMSIZE);

	tprintk("_cmd:%d\n", cmd);
	switch (cmd) {
		case TEST_CMD_MEMREAD :
			/* val = ioread32(mmio_addr); */
			// ioread32_rep(mmio_addr, data, TEST_MMIO_DATANUM);
			
			memcpy_fromio(data, mmio_addr, TEST_MMIO_DATASIZE);
			copy_to_user(d->mmiodata, data, TEST_MMIO_DATASIZE);
			break;
		case TEST_CMD_MEMWRITE :
			/* iowrite32(val, mmio_addr+TEST_MEM); */

			copy_from_user(data, d->mmiodata, TEST_MMIO_DATASIZE);
			memcpy_toio(mmio_addr, data, TEST_MMIO_DATASIZE);
			break;

		case TEST_CMD_CDMA_START :
			copy_from_user(dev_data->cdma_buffer, d->cdmabuf, TEST_CDMA_BUFFER_SIZE);
			/* memcpy(dev_data->cdma_buffer, data, TEST_CDMA_BUFFER_SIZE); */
			wmb();
			outl(CDMA_START, dev_data->pio_base + TEST_CDMA_START);
			break;
		case TEST_CMD_GET_CDMA_DATA :
			n = copy_to_user(d->cdmabuf, dev_data->cdma_buffer, TEST_CDMA_BUFFER_SIZE);
			if(n != 0){
				tprintk("cannot copy all data (%d bytes in %d bytes)\n", n, TEST_CDMA_BUFFER_SIZE);
			}
			break;

		case TEST_CMD_SDMA_START :
			copy_from_user(dev_data->sdma_buffer, d->sdmabuf, TEST_SDMA_BUFFER_SIZE);
			dev_data->sdma_addr = pci_map_single(dev_data->pdev, dev_data->sdma_buffer,
					TEST_SDMA_BUFFER_SIZE, DMA_BIDIRECTIONAL);
			dev_data->sdma_len = TEST_SDMA_BUFFER_SIZE;
			wmb();
			outl(dev_data->sdma_addr, dev_data->pio_base + TEST_SET_SDMA_ADDR);
			outl(dev_data->sdma_len, dev_data->pio_base + TEST_SET_SDMA_LEN);
			wmb();
			outl(SDMA_START, dev_data->pio_base + TEST_SDMA_START);
			break;
		case TEST_CMD_GET_SDMA_DATA :
			pci_unmap_single(dev_data->pdev, dev_data->sdma_addr,
		  			TEST_SDMA_BUFFER_SIZE, DMA_BIDIRECTIONAL);
			n = copy_to_user(d->sdmabuf, dev_data->sdma_buffer, TEST_SDMA_BUFFER_SIZE);
			if(n != 0) {
				tprintk("cannot copy all data (%d bytes in %d bytes)\n", n, TEST_SDMA_BUFFER_SIZE);
			}
			break;

		case TEST_CMD_DOSOMETHING :
			outl(0, dev_data->pio_base + TEST_DO);
			break;

		/* case TEST_CMD_GET_INTMASK : */
		/* 	val = inb(dev_data->pio_base + TEST_INTMASK); */
		/* 	break; */
		default:
			iounmap(mmio_addr);
			return -EINVAL; // invalid argument(cmd)
	}

	iounmap(mmio_addr);

	return 0;
}

static int test_pci_open(struct inode *inode, struct file *file)
{

	file->private_data = NULL;

	return 0; // success
}

// todo check free region
static int test_pci_close(struct inode *inode, struct file *file)
{

	if(file->private_data) {
		kfree(file->private_data);
		file->private_data = NULL;
	}

	return 0; // success
}


struct file_operations test_pci_fops = 
{
	.open = test_pci_open,
	.release = test_pci_close,
	.read = test_pci_read,
	.write = test_pci_write,
	.llseek = test_pci_llseek,
	.unlocked_ioctl = test_pci_uioctl,
};

//-----------------------------------------------------------------
// supported pci id type
static struct pci_device_id test_pci_ids[] =
{
	{ PCI_DEVICE(PCI_VENDOR_ID_TEST, PCI_DEVICE_ID_TEST) },
	{ 0, },
};

// export pci_device_id
MODULE_DEVICE_TABLE(pci, test_pci_ids);

// interrupt handler
void test_do_tasklet(unsigned long unused_data)
{
	tprintk("tasklet called\n");
}

DECLARE_TASKLET(test_tasklet, test_do_tasklet, 0); // no data

irqreturn_t test_pci_handler(int irq, void *dev_id)
{
	struct pci_dev *pdev = dev_id;
	char intmask;

	tprintk("irq handler called\n");

	intmask = inb(dev_data->pio_base + TEST_GET_INTMASK);
	if(intmask & INT_DO) {
		// register tasklet
		tasklet_schedule(&test_tasklet);
	}
	if(intmask & INT_CDMA) {
		tasklet_schedule(&test_tasklet);
	}
	if(intmask & INT_SDMA) {
		tasklet_schedule(&test_tasklet);
	}

	outb(0, dev_data->pio_base + TEST_SET_INTMASK);

	// down irq line
	outl(0, dev_data->pio_base + TEST_DOWN_IRQ);

	return IRQ_HANDLED; 
}

//-----------------------------------------------------------------
// pci initialization function
// enable pci & register character device
static int test_pci_probe (struct pci_dev *pdev,
		const struct pci_device_id *id)
{
	int err;
	char irq;

	int alloc_ret = 0;
	int cdev_err = 0;

	short vendor_id, device_id;


	//-----------------------------------------------------------------
	// config PCI
	// enable pci device
	err = pci_enable_device(pdev);
	if(err) {
		printk(KERN_ERR "can't enable pci device\n");
		goto error; 
	}
	tprintk("PCI enabled for %s\n", DRIVER_TEST_NAME);

	// request PCI region
	// bar 0 ... MMIO
	dev_data->mmio_base = pci_resource_start(pdev, BAR_MMIO);
	dev_data->mmio_length = pci_resource_len(pdev, BAR_MMIO);
	dev_data->mmio_flags = pci_resource_flags(pdev, BAR_MMIO);
	tprintk( "mmio_base: %lx, mmio_length: %lx, mmio_flags: %lx\n",
			dev_data->mmio_base, dev_data->mmio_length, dev_data->mmio_flags);

	if(!(dev_data->mmio_flags & IORESOURCE_MEM)){
		printk(KERN_ERR "BAR%d is not for mmio\n", BAR_MMIO);
		goto error;
	}

	err = pci_request_region(pdev, BAR_MMIO, DRIVER_TEST_NAME);
	if(err) {
		printk(KERN_ERR "%s :error pci_request_region MMIO\n", __func__);
		goto error; 
	}

	
	// bar 1 ... IO port
	dev_data->pio_base = pci_resource_start(pdev, BAR_PIO);
	dev_data->pio_length = pci_resource_len(pdev, BAR_PIO);
	dev_data->pio_flags = pci_resource_flags(pdev, BAR_PIO);
	tprintk("pio_base: %lx, pio_length: %lx, pio_flags: %lx\n",
			dev_data->pio_base, dev_data->pio_length, dev_data->pio_flags);

	if(!(dev_data->pio_flags & IORESOURCE_IO)){
		printk(KERN_ERR "BAR%d is not for pio\n", BAR_PIO);
		goto error;
	}

	err = pci_request_region(pdev, BAR_PIO, DRIVER_TEST_NAME);
	if(err) {
		printk(KERN_ERR "%s :error pci_request_region PIO\n", __func__);
		goto error; 
	}

	// show PCI configuration data
	// define at include/uapi/linux/pci_regs.h
	pci_read_config_word(pdev, PCI_VENDOR_ID, &vendor_id);
	pci_read_config_word(pdev, PCI_DEVICE_ID, &device_id);
	tprintk("PCI Vendor ID:%x, Device ID:%x\n", vendor_id, device_id);


	dev_data->pdev = pdev;
	dev_data->pio_memsize = TEST_PIO_DATASIZE;

	tprintk("sucess allocate i/o region\n");

	//-----------------------------------------------------------------
	// config irq 
	// get irq number
	irq = pdev->irq; // same as pci_read_config_byte(pdev, PCI_INTERRUPT_LINE, &irq);
	tprintk("device irq: %d\n", irq);

	err = request_irq(irq, test_pci_handler, 0, DRIVER_TEST_NAME, pdev);
	if(err){
		printk(KERN_ERR "%s :error request irq %d\n", __func__, irq);
		goto error;
	}


	//-----------------------------------------------------------------
	// register character device
	// allocate major number
	alloc_ret = alloc_chrdev_region(&test_pci_devt, test_pci_minor, test_pci_devs_max, DRIVER_TEST_NAME);
	if(alloc_ret) goto error;

	test_pci_major = MAJOR(test_pci_devt);

	dev_data->cdev = (struct cdev*)kmalloc(sizeof(struct cdev), GFP_KERNEL);
	if(!dev_data->cdev) goto error;

	cdev_init(dev_data->cdev, &test_pci_fops);
	dev_data->cdev->owner = THIS_MODULE;

	cdev_err = cdev_add(dev_data->cdev, test_pci_devt, test_pci_devs_max);
	if(cdev_err) goto error;

	tprintk("%s driver(major %d) installed.\n", DRIVER_TEST_NAME, test_pci_major);

	//-----------------------------------------------------------------
	// config DMA
	err = pci_set_dma_mask(pdev, DMA_BIT_MASK(32));
	if(err) {
		printk(KERN_ERR "Cannot set DMA mask\n");
		goto error;
	}
	pci_set_master(pdev);

	// allocate consistent DMA
	dev_data->cdma_buffer = pci_alloc_consistent(pdev, TEST_CDMA_BUFFER_SIZE, &dev_data->cdma_addr);
	if(dev_data->cdma_buffer == NULL) {
		printk(KERN_ERR "Cannot allocate consistent DMA buffer\n");
		goto error;
	}
	dev_data->cdma_len = TEST_CDMA_BUFFER_SIZE;

	// send consistent DMA info to device
	outl(dev_data->cdma_addr, dev_data->pio_base + TEST_SET_CDMA_ADDR);
	outl(dev_data->cdma_len,  dev_data->pio_base + TEST_SET_CDMA_LEN);

	tprintk("cdma_addr : %x\n",  dev_data->cdma_addr);

	// streaming DMA
	dev_data->sdma_buffer = kmalloc(TEST_SDMA_BUFFER_SIZE, GFP_KERNEL);
	if(dev_data->sdma_buffer == NULL) {
		printk(KERN_ERR "Cannot allocate streaming DMA buffer\n");
		goto error;
	}

	return 0;

error:
	tprintk("PCI load error\n");
	if(cdev_err == 0) cdev_del(dev_data->cdev);

	if(alloc_ret == 0) unregister_chrdev_region(test_pci_devt, test_pci_devs_max);

	return -1;
}

static void test_pci_remove(struct pci_dev *pdev)
{
	cdev_del(dev_data->cdev);
	unregister_chrdev_region(test_pci_devt, test_pci_devs_max);

	pci_release_region(pdev, BAR_MMIO);
	pci_release_region(pdev, BAR_PIO);
	pci_disable_device(pdev);

	tprintk("%s driver (major %d) unloaded\n", DRIVER_TEST_NAME, test_pci_major);
}


static struct pci_driver test_pci_driver =
{
	.name = DRIVER_TEST_NAME,
	.id_table = test_pci_ids,
	.probe = test_pci_probe,
	.remove = test_pci_remove,
};


//-----------------------------------------------------------------
// module init/exit
static int __init test_pci_init(void)
{
	dev_data = kmalloc(sizeof(struct test_device_data), GFP_KERNEL);
	if(!dev_data) {
		printk(KERN_ERR "cannot allocate device data memory\n");
		return -1; // should return other value?
	}

	return pci_register_driver(&test_pci_driver);
}

static void __exit test_pci_exit(void)
{
	pci_unregister_driver(&test_pci_driver);
}

module_init(test_pci_init);
module_exit(test_pci_exit);
