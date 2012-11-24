#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>    // 0-include header file
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/wait.h>     // 0-include header file
#include <linux/smp_lock.h> // 08-include header file for semaphore
#include <asm/io.h>
#include <asm/uaccess.h>

#include "myioctl.h"

#ifdef CONFIG_SMP
#define __SMP__
#endif

#define	CDATA_MAJOR 121 
#define BUFSIZE 1024

struct cdata_t {
   char         data[BUFSIZE];
   int          index;
   int          count;

   wait_queue_head_t	wait;   // 1-must declare a wait queue by myself
   spinlock_t		lock;

};	//semicolon must be added otherwise unexpected error may happen

static DECLARE_MUTEX(cdata_sem);  // 18-declare semaphore and initial

static int cdata_open(struct inode *inode, struct file *filp)
{
	struct cdata_t *cdata;

	printk(KERN_ALERT "cdata: in cdata_open()\n");

	cdata = (struct cdata_t *)kmalloc(sizeof(struct cdata_t), GFP_KERNEL);
	cdata->index = 0;	// kmalloc won't init the memory to 0
	cdata->count = 0;
	filp->private_data = (void *)cdata;

        init_waitqueue_head(&cdata->wait);   // 2-init wait queue head

	return 0;
}

static int cdata_ioctl(struct inode *inode, struct file *filp, 
			unsigned int cmd, unsigned long arg)
{
	struct cdata_t *cdata;
	int i;
	cdata = (struct cdata_t *)filp->private_data;

	printk(KERN_ALERT "cdata: in cdata_ioctl()\n");
        switch (cmd)
	{
	   case CDATA_EMPTY:
	      //printk(KERN_ALERT "cdata_ioctl: IOCTL_EMPTY\n");
	      printk(KERN_ALERT "cdata_ioctl: %p\n", filp);

	      for (i=0; i<cdata->count; i++)
	         cdata->data[i] = 0;

	      break;
	   case CDATA_SYNC:
	      printk(KERN_ALERT "cdata_ioctl: IOCTL_SYNC\n");

	      for (i=0; i <cdata->count; i++)
	         printk(KERN_ALERT "data[%i]=%c\n", i, cdata->data[i]);

	      break;
	   default:
	      return -ENOTTY;
	}
}

static ssize_t cdata_read(struct file *filp, char *buf, 
				size_t size, loff_t *off)
{
        struct cdata_t *cdata;        
        cdata = (struct cdata_t *)filp->private_data;

        printk(KERN_ALERT "cdata: in cdata_read()\n");
}

static ssize_t cdata_write(struct file *filp, const char *buf, 
				size_t count, loff_t *off)
{
	struct cdata_t *cdata = (struct cdata_t *)filp->private_data;
	int i;

	printk(KERN_ALERT "cdata_write: %s\n", buf);

	cdata->count = count;

        DECLARE_WAITQUEUE(wait, current);   // 3-create a node with current process
	
	//mutex_lock
        down(&cdata_sem);   // 28- lock semaphore

	for(i=0; i<count; i++) {
	   if(cdata->index > BUFSIZE)
              add_wait_queue(&cdata->wait, &wait);   // 4-add the node into wait queue
              current->state = TASK_UNINTERRUPTIBLE;   // 5-state change to waiting
              schedule();   // 6-call scheduler to take over

              current->state = TASK_RUNNING;   // 7-at this moment, current process is back and re-gain the cpu control
              remove_wait_queue(&cdata->wait, &wait);  // 8-remove our node from wait queue
	      return -EFAULT;
	   if(copy_from_user(&cdata->data[cdata->index++], &buf[i], 1))
	      return -EFAULT;
	}

	//mutex_unlock
        up(&cdata_sem);   // 38- unlock semaphore

	return 0;
}

static int cdata_release(struct inode *inode, struct file *filp)
{
	printk(KERN_ALERT "cdata: in cdata_release()\n");
	return 0;
}

struct file_operations cdata_fops = {	
	open:		cdata_open,
	release:	cdata_release,
	ioctl:		cdata_ioctl,
	read:		cdata_read,
	write:		cdata_write,
};

int my_init_module(void)
{
	register_chrdev(CDATA_MAJOR, "cdata", &cdata_fops);
	printk(KERN_ALERT "cdata module: registered.\n");

	return 0;
}

void my_cleanup_module(void)
{
	unregister_chrdev(CDATA_MAJOR, "cdata");
	printk(KERN_ALERT "cdata module: unregisterd.\n");
}

module_init(my_init_module);
module_exit(my_cleanup_module);

MODULE_LICENSE("GPL");
