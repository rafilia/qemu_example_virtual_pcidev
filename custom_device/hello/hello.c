// char device test program
// use cdev_add()

#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <linux/slab.h>
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

struct hello_data {
	unsigned char val;
	rwlock_t lock;
};

ssize_t hello_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_ops)
{
	struct hello_data *p = filp->private_data;
	unsigned char val;
	int retval = 0;

	printk("%s, count %d pos %lld\n", __func__, count, *f_ops);

	if(count >= 1){
		if(copy_from_user(&val, &buf[0], 1)) {
			retval = -EFAULT;
			goto out;
		}
		printk("%02x ", val);
		
		write_lock(&p->lock);
		p->val = val;
		write_unlock(&p->lock);
		retval = count;
	}

out:
	return retval;
}

ssize_t hello_read(struct file *filp, char __user *buf, size_t count, loff_t *f_ops)
{
	struct hello_data *p = filp->private_data;
	int i;
	unsigned char val;
	int retval;

	read_lock(&p->lock);
	val = p->val;
	read_unlock(&p->lock);

	printk("%s: count %d pos %lld\n", __func__, count, *f_ops);

	for(i = 0; i < count; i++) {
		if(copy_to_user(&buf[i], &val, 1)) {
			retval = -EFAULT;
			goto out;
			}
	}

	retval = count;

out:
	return retval;
}

static int hello_open(struct inode *inode, struct file *file)
{
	struct hello_data *p;

	
	p = kmalloc(sizeof(struct hello_data), GFP_KERNEL);
	if(p == NULL){
		printk("%s: cannot allocate memory\n", __func__);
		return -ENOMEM;
	}

	p->val = 0xff;
	rwlock_init(&p->lock);

	file->private_data = p;

	// printk("%s: major %d minor %d (pid %d)\n", 
	//		__func__, imajor(inode), iminor(inode), current->pid);
	// inode->i_private = inode;
	// file->private_data = file;
	//
	// printk(" i_private=%p private_data=%p\n",
	// 		inode->i_private, file->private_data);

	return 0; // success
}

static int hello_close(struct inode *inode, struct file *file)
{

	if(file->private_data) {
		kfree(file->private_data);
		file->private_data = NULL;
	}

	// printk("%s: major %d minor %d (pid %d)\n", 
	//		__func__, imajor(inode), iminor(inode), current->pid);
	// printk(" i_private=%p private_data=%p\n",
	//		inode->i_private, file->private_data);

	return 0; // success
}

struct file_operations hello_fops = 
{
	.open = hello_open,
	.release = hello_close,
	.read = hello_read,
	.write = hello_write,
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
