// pci character device driver test program

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

#include "hello_pci_device.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("tm");
MODULE_DESCRIPTION("test hello pci device driver");

#define DRIVER_HELLO_NAME "hello_pci"
#define DEVICE_HELLO_NAME "hello_pci_dev"


// max device count (minor number)
static int hello_pci_devs_max = 2;

// if hello_pci_major = 0 then use dynamic allocation
static unsigned int hello_pci_major = 0;
// get parameter from console if needed for hello_pci_major when insmod
module_param(hello_pci_major, uint, 0);

static unsigned int hello_pci_minor = 0; // static allocation

static dev_t hello_pci_devt; // MKDEV(hello_pci_major, hello_pci_minor)

static struct cdev *hello_pci_cdev = NULL;
/* struct hello_data { */
/* 	unsigned char val; */
/* 	rwlock_t lock; */
/* }; */

struct pci_cdev {
	struct pci_dev *pdev;
	struct cdev *cdev;

	// for coherent/streaming dma
	dma_addr_t dma_addr, sdma_addr;
	int dma_len, sdma_len;
	void *dma_buffer, *sdma_buffer;
};

static struct pci_cdev *pci_cdev;

//-----------------------------------------------------------------
// file_operations function

ssize_t hello_pci_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_ops)
{
	/* struct hello_data *p = filp->private_data; */
	/* unsigned char val; */
	int retval = 0;
	unsigned long pio_base;
	struct pci_dev *pdev = (struct pci_dev *)filp->private_data;

	printk("%s, count %d pos %lld\n", __func__, count, *f_ops);

	/* if(count >= 1){ */
	/* 	if(copy_from_user(&val, &buf[0], 1)) { */
	/* 		retval = -EFAULT; */
	/* 		goto out; */
	/* 	} */
	/* 	printk("%02x ", val); */
	/* 	 */
	/* 	write_lock(&p->lock); */
	/* 	p->val = val; */
	/* 	write_unlock(&p->lock); */
	/* 	retval = count; */
	/* } */

/* out: */
	return retval;
}

ssize_t hello_pci_read(struct file *filp, char __user *buf, size_t count, loff_t *f_ops)
{
	unsigned long pio_base;
	struct pci_dev *pdev = (struct pci_dev *)filp->private_data;

	unsigned int read_num = 0;
	long val = 0;
	int *buffer = buf;

	printk("\t%s: count %d pos %lld\n", __func__, count, *f_ops);

	pio_base = pci_resource_start(pdev, BAR_PIO);
	printk("\tinb port:%lx\n", pio_base);

	// read from ioport
	while (read_num*4 < count) {
		// val = inb(pio_base++);
		// put_user(val, &buf[read_num++]);
		printk("\tpio_base:%lx\n", pio_base);
		val = inl(pio_base);
		put_user(val, &buffer[read_num++]);
		pio_base += 4;

	}

/* out: */
	return read_num;
}

long hello_pci_uioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	unsigned long pio_base, mmio_base;
	char *mmio_addr;
	struct pci_dev *pdev = (struct pci_dev *)filp->private_data;

	int val = 0;
	int data[DATANUM];

	hello_ioctl_data *d = arg;

	int n;

	pio_base = pci_resource_start(pdev, BAR_PIO);
	mmio_base = pci_resource_start(pdev, BAR_MMIO);
	mmio_addr = ioremap(mmio_base, HELLO_PCI_MEMSIZE);

	printk("_cmd:%d\n", cmd);
	switch (cmd) {
		case HELLO_CMD_READ :
			val = inl(pio_base + HELLO_PIOOFFSET);
			put_user(val, &(d->buf[0]));
			break;
		case HELLO_CMD_WRITE :
			get_user(val, &(d->val));
			printk("_val:%x\n", val);
			outl(val, pio_base + HELLO_PIOOFFSET);
			break;

		case HELLO_CMD_MEMREAD :
			/* val = ioread32(mmio_addr+HELLO_MEMOFFSET); */
			/* put_user(val, &(d->buf[0])); */

			// ioread32_rep(mmio_addr+HELLO_MEMOFFSET, data, DATANUM);
			memcpy_fromio(data, mmio_addr+HELLO_MEMOFFSET, DATANUM*sizeof(int));
			copy_to_user(d->buf, data, DATANUM*sizeof(int));
			break;
		case HELLO_CMD_MEMWRITE :
			get_user(val, &(d->val));
			printk("_val:%x\n", val);
			iowrite32(val, mmio_addr+HELLO_MEMOFFSET);
			break;

		case HELLO_CMD_DMA_START :
			copy_from_user(pci_cdev->sdma_buffer, d->buf, HELLO_DMA_BUFFER_SIZE);
			/* memcpy(pci_cdev->dma_buffer, data, HELLO_DMA_BUFFER_SIZE); */
			wmb();
			outl(DMA_START, pio_base + HELLO_DMA_START);
			break;
		case HELLO_CMD_GET_DMA_DATA :
			n = copy_to_user(d->buf, pci_cdev->dma_buffer, HELLO_DMA_BUFFER_SIZE);
			if(n != 0){
				printk(KERN_ALERT "  cannot copy all data (%d bytes in %d bytes)\n", n, HELLO_DMA_BUFFER_SIZE);
			}
			break;

		case HELLO_CMD_SDMA_START :
			copy_from_user(pci_cdev->sdma_buffer, d->buf, HELLO_DMA_BUFFER_SIZE);
			pci_cdev->sdma_addr = pci_map_single(pdev, pci_cdev->sdma_buffer,
					HELLO_DMA_BUFFER_SIZE, DMA_BIDIRECTIONAL);
			pci_cdev->sdma_len = HELLO_DMA_BUFFER_SIZE;
			wmb();
			outl(pci_cdev->sdma_addr, pio_base + HELLO_SET_SDMA_ADDR);
			outl(pci_cdev->sdma_len, pio_base + HELLO_SET_SDMA_LEN);
			wmb();
			outl(DMA_START, pio_base + HELLO_SDMA_START);
			break;
		case HELLO_CMD_GET_SDMA_DATA :
			pci_unmap_single(pdev, pci_cdev->sdma_addr,
		  			HELLO_DMA_BUFFER_SIZE, DMA_BIDIRECTIONAL);
			n = copy_to_user(d->buf, pci_cdev->sdma_buffer, HELLO_DMA_BUFFER_SIZE);
			if(n != 0) {
				printk(KERN_ALERT "  cannot copy all data (%d bytes in %d bytes)\n", n, HELLO_DMA_BUFFER_SIZE);
			}
			break;

		case HELLO_CMD_DOSOMETHING :
			outl(val, pio_base + HELLO_DOOFFSET);
			break;
		default:
			iounmap(mmio_addr);
			return -EINVAL; // invalid argument(cmd)
	}

	iounmap(mmio_addr);

	return 0;
}

static int hello_pci_open(struct inode *inode, struct file *file)
{
	/* struct pci_cdev *p; */
	/* p = kmalloc(sizeof(struct pci_cdev), GFP_KERNEL); */
	/* if(p == NULL){ */
	/* 	printk("%s: cannot allocate memory\n", __func__); */
	/* 	return -ENOMEM; */
	/* } */

	file->private_data = pci_cdev->pdev;

	return 0; // success
}

static int hello_pci_close(struct inode *inode, struct file *file)
{

	if(file->private_data) {
		kfree(file->private_data);
		file->private_data = NULL;
	}

	return 0; // success
}


struct file_operations hello_pci_fops = 
{
	.open = hello_pci_open,
	.release = hello_pci_close,
	.read = hello_pci_read,
	.write = hello_pci_write,
	.unlocked_ioctl = hello_pci_uioctl,
};

//-----------------------------------------------------------------
// supported pci id type
static struct pci_device_id hello_pci_ids[] =
{
	{ PCI_DEVICE(PCI_VENDOR_ID_HELLO, PCI_DEVICE_ID_HELLO) },
	{ 0, },
};

// export pci_device_id
MODULE_DEVICE_TABLE(pci, hello_pci_ids);

// interrupt handler

void hello_do_tasklet(unsigned long unused_data)
{
	printk(KERN_ALERT "  tasklet called\n");
}

DECLARE_TASKLET(hello_tasklet, hello_do_tasklet, 0); // no data

irqreturn_t hello_handler(int irq, void *dev_id)
{
	unsigned long pio_base;
	struct pci_dev *pdev = dev_id;

	printk(KERN_ALERT "  irq handler called\n");

	pio_base = pci_resource_start(pdev, BAR_PIO);
	// down irq line
	outl(0, pio_base + HELLO_DOWN_IRQ_OFFSET);

	// register tasklet
	tasklet_schedule(&hello_tasklet);

	return IRQ_HANDLED; 
}

//-----------------------------------------------------------------
// pci initialization function
// enable pci & register character device
static int hello_pci_probe (struct pci_dev *pdev,
		const struct pci_device_id *id)
{
	int err;
	unsigned long pio_base, mmio_base, pio_flags, mmio_flags, pio_length, mmio_length;
	char irq;

	int alloc_ret = 0;
	int cdev_err = 0;

	short vendor_id, device_id;


	//-----------------------------------------------------------------
	// enable pci device
	err = pci_enable_device(pdev);
	if(err) {
		printk(KERN_ERR "can't enable pci device\n");
		return err; 
	}
	printk(KERN_ALERT "PCI enabled for %s\n", DRIVER_HELLO_NAME);

	/* // request pci regions (bar 0-5?) */
	/* err = pic_request_regions(pdev, DRIVER_HELLO_NAME); */
	/* if(err) { */
	/* 	dev_err(pdev, "hello_pci_probe: can't request regions\n"); */
	/* 	return err;  */
	/* } */

	// bar 0 ... MMIO
	mmio_base = pci_resource_start(pdev, BAR_MMIO);
	mmio_length = pci_resource_len(pdev, BAR_MMIO);
	mmio_flags = pci_resource_flags(pdev, BAR_MMIO);
	err = pci_request_region(pdev, BAR_MMIO, DRIVER_HELLO_NAME);
	if(err) {
		printk(KERN_ERR "%s :error pci_request_region MMIO\n", __func__);
		return err; 
	}

	printk(KERN_ALERT "mmio_base: %lx, mmio_length: %lx, mmio_flags: %lx\n", mmio_base, mmio_length, mmio_flags);
	
	// bar 1 ... IO port
	pio_base = pci_resource_start(pdev, BAR_PIO);
	pio_length = pci_resource_len(pdev, BAR_PIO);
	pio_flags = pci_resource_flags(pdev, BAR_PIO);
	err = pci_request_region(pdev, BAR_PIO, DRIVER_HELLO_NAME);
	if(err) {
		printk(KERN_ERR "%s :error pci_request_region PIO\n", __func__);
		return err; 
	}

	printk(KERN_ALERT "pio_base: %lx, pio_length: %lx, pio_flags: %lx\n", pio_base, pio_length, pio_flags);

	// PCI configuration space
	// define at include/uapi/linux/pci_regs.h
	pci_read_config_word(pdev, PCI_VENDOR_ID, &vendor_id);
	pci_read_config_word(pdev, PCI_DEVICE_ID, &device_id);
	printk(KERN_ALERT "PCI Vendor ID:%x, Device ID:%x\n", vendor_id, device_id);

	pci_cdev = (struct pci_cdev*)kmalloc(sizeof(struct pci_cdev), GFP_KERNEL);
	if(!pci_cdev) goto error;
	pci_cdev->pdev = pdev;

	printk(KERN_ALERT "sucess allocate i/o region\n");

	//-----------------------------------------------------------------
	// set irq hundler 
	// get irq number
	irq = pdev->irq; // same as pci_read_config_byte(pdev, PCI_INTERRUPT_LINE, &irq);
	printk(KERN_ALERT "device irq: %d\n", irq);

	err = request_irq(irq, hello_handler, 0, DRIVER_HELLO_NAME, pdev);
	if(err) goto error;

	//-----------------------------------------------------------------
	// register character device
	// allocate major number
	
	alloc_ret = alloc_chrdev_region(&hello_pci_devt, 0, hello_pci_devs_max, DRIVER_HELLO_NAME);
	if(alloc_ret) goto error;

	hello_pci_major = MAJOR(hello_pci_devt);

	hello_pci_cdev = (struct cdev*)kmalloc(sizeof(struct cdev), GFP_KERNEL);
	if(!hello_pci_cdev) goto error;
	cdev_init(hello_pci_cdev, &hello_pci_fops);
	hello_pci_cdev->owner = THIS_MODULE;

	cdev_err = cdev_add(hello_pci_cdev, hello_pci_devt, hello_pci_devs_max);
	if(cdev_err) goto error;

	printk(KERN_ALERT "%s driver(major %d) installed.\n", DRIVER_HELLO_NAME, hello_pci_major);

	pci_cdev->cdev = hello_pci_cdev;

	//-----------------------------------------------------------------
	// config DMA
	err = pci_set_dma_mask(pdev, DMA_BIT_MASK(32));
	if(err) {
		printk(KERN_ERR "Cannot set DMA mask\n");
		goto error;
	}
	pci_set_master(pdev);

	// allocate coherent DMA
	pci_cdev->dma_buffer = pci_alloc_consistent(pdev, HELLO_DMA_BUFFER_SIZE, &pci_cdev->dma_addr);
	if(pci_cdev->dma_buffer == NULL) {
		printk(KERN_ERR "Cannot allocate coherent DMA\n");
		goto error;
	}


	outl(pci_cdev->dma_addr, pio_base + HELLO_SET_DMA_ADDR);
	pci_cdev->dma_len = HELLO_DMA_BUFFER_SIZE;
	outl(pci_cdev->dma_len, pio_base + HELLO_SET_DMA_LEN);

	printk(KERN_ALERT "dma_addr : %x\n",  pci_cdev->dma_addr);

	// streaming DMA
	pci_cdev->sdma_buffer = kmalloc(HELLO_DMA_BUFFER_SIZE, GFP_KERNEL);
	return 0;

error:
	printk(KERN_ALERT "cdev load error\n");
	if(cdev_err == 0) cdev_del(hello_pci_cdev);

	if(alloc_ret == 0) unregister_chrdev_region(hello_pci_devt, hello_pci_devs_max);

	return -1;
}

static void hello_pci_remove(struct pci_dev *pdev)
{
	cdev_del(hello_pci_cdev);
	unregister_chrdev_region(hello_pci_devt, hello_pci_devs_max);

	pci_release_region(pdev, BAR_MMIO);
	pci_release_region(pdev, BAR_PIO);
	pci_disable_device(pdev);

	printk("%s driver (major %d) unloaded\n", DRIVER_HELLO_NAME, hello_pci_major);
}


static struct pci_driver pci_driver =
{
	.name = DRIVER_HELLO_NAME,
	.id_table = hello_pci_ids,
	.probe = hello_pci_probe,
	.remove = hello_pci_remove,
};


//-----------------------------------------------------------------
// module init/exit
static int __init hello_pci_init(void)
{
	return pci_register_driver(&pci_driver);
}

static void __exit hello_pci_exit(void)
{
	pci_unregister_driver(&pci_driver);
}

module_init(hello_pci_init);
module_exit(hello_pci_exit);
