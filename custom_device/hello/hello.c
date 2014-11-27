// char device test program
// use cdev_add()

#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <asm/current.h>
#include <asm/uaccess.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("tm");
MODULE_DESCRIPTION("test hello device driver");

#define DRIVER_NAME "hello"

// max device count
static int hello_devs_max = 2;

// if hello_major = 0 then use dynamic allocation
static unsigned int hello_major;
// get parameter from console if needed for hello_major
module_param(hello_major, uint, 0);

static struct cdev hello_cdev;

static int hello_open(struct inode *inode, struct file *file)
{
	printk("%s: major %d minor %d (pid %d)\n", 
			__func__, imajor(inode), iminor(inode), current->pid);

	inode->i_private = inode;
	file->private_data = file;

	printk(" i_private=%p private_data=%p\n",
			inode->i_private, file->private_data);

	return 0; // success
}

static int hello_close(struct inode *inode, struct file *file)
{
	printk("%s: major %d minor %d (pid %d)\n", 
			__func__, imajor(inode), iminor(inode), current->pid);

	printk(" i_private=%p private_data=%p\n",
			inode->i_private, file->private_data);

	return 0; // success
}

struct file_operations hello_fops = 
{
	.open = hello_open,
	.release = hello_close,
};

static int hello_init(void)
{
	dev_t dev = MKDEV(hello_major, 0);
	int alloc_ret = 0;
	int major;
	int cdev_err = 0;

	alloc_ret = alloc_chrdev_region(&dev, 0, hello_devs_max, DRIVER_NAME);
	if(alloc_ret) goto error;

	hello_major = major = MAJOR(dev);

	cdev_init(&hello_cdev, &hello_fops);
	hello_cdev.owner = THIS_MODULE;

	cdev_err = cdev_add(&hello_cdev, MKDEV(hello_major, 0), hello_devs_max);
	if(cdev_err) goto error;

	printk(KERN_ALERT "%s driver(major %d) installed.\n", DRIVER_NAME, major);

	return 0;

error:
	if(cdev_err == 0) cdev_del(&hello_cdev);

	if(alloc_ret == 0) unregister_chrdev_region(dev, hello_devs_max);

	return -1;
}

static void hello_exit(void)
{
	dev_t dev = MKDEV(hello_major, 0);

	cdev_del(&hello_cdev);
	unregister_chrdev_region(dev, hello_devs_max);

	printk("%s driver (major %d) unloaded\n", DRIVER_NAME, hello_major);
}

module_init(hello_init);
module_exit(hello_exit);
