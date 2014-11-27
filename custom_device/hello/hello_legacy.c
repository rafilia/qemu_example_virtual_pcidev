// char device test program
// use register_chrdev()

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

// if hello_major = 0 then use dynamic allocation
static unsigned int hello_major;
// get parameter from console if needed for hello_major
module_param(hello_major, uint, 0);

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
	int major; 
	int ret = 0;

	printk(KERN_ALERT "driver hello load start\n");

	major = register_chrdev(hello_major, DRIVER_NAME, &hello_fops);
	if((hello_major > 0 && major != 0) ||  // static allocation
			(hello_major == 0 && major < 0) || // dynamic allocation
			major < 0 ) {                      // else
		printk("%s driver registration error\n", DRIVER_NAME);
		ret = major;
		goto error;
	}

	if(hello_major == 0) { // dynamic allocation
		hello_major = major;
	}
	printk("%s driver (major %d) installed\n", DRIVER_NAME, hello_major);

error:
	return ret;
}

static void hello_exit(void)
{
	unregister_chrdev(hello_major, DRIVER_NAME);

	printk("%s driver (major %d) unloaded\n", DRIVER_NAME, hello_major);
}

module_init(hello_init);
module_exit(hello_exit);
