#include <linux/module.h>
#include <linux/version.h>    //for LINUX_VERSION_CODE
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/mm.h>         //for remap_pfn_range() or remap_page_range()
#include <linux/miscdevice.h>
#include <linux/wait.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#include <linux/workqueue.h>
#else
#include <linux/tqueue.h>
#endif
#include <asm/io.h>
#include <asm/uaccess.h>

#ifdef CONFIG_SMP
#define __SMP__
#endif

#define	CDATA_MAJOR 121 
#define BUFSIZE     1024

#define LCD_WIDTH   (640)
#define LCD_HEIGHT  (480)
#define LCD_BPP     (1)
#define LCD_SIZE    (LCD_WIDTH*LCD_HEIGHT*LCD_BPP)

struct cdata_t {
    char        data[BUFSIZE];
    int         index;
    char        *iomem;
    unsigned int    offset;
#if 0
    struct timer_list   timer;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    struct work_struct    work;     //2.6
#else
    struct tq_struct   tq;        //2.4
#endif

    wait_queue_head_t   wait;
};

static DECLARE_MUTEX(cdata_sem);

// Supporting APIs
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
void flush_lcd(struct work_struct *work)
{
    struct cdata_t *cdata = container_of(work, struct cdata_t, work);
#else
void flush_lcd(unsigned long priv)
{
    struct cdata_t *cdata = (struct cdata_t *)priv;
#endif
    char *fb = cdata->iomem;
    int offset = cdata->offset;
    int index = cdata->index;
    int i;

    for (i = 0; i < index; i++) {
        writeb(cdata->data[i], fb + offset);
        offset++;

        if (offset >= LCD_SIZE)
            offset = 0;
    }

    cdata->index = 0;
    cdata->offset = offset;

    // Wake up process
    //current->state = TASK_RUNNING;
    wake_up(&cdata->wait);
}

static int cdata_open(struct inode *inode, struct file *filp)
{
    struct cdata_t *cdata;

    cdata = (struct cdata_t *)kmalloc(sizeof(struct cdata_t), GFP_KERNEL);
    cdata->index = 0;

    init_waitqueue_head(&cdata->wait);
    cdata->iomem = ioremap(0xe0000000, LCD_SIZE);   // we use Notebook's VGA as frame buffer
#if 0
    init_timer(&cdata->timer);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    INIT_WORK(&cdata->work, flush_lcd);
#else
    INIT_TQUEUE(&cdata->tq, flush_lcd, (void *)cdata);
#endif

    cdata->offset = 0;

    filp->private_data = (void *)cdata;

	return 0;
}

static int cdata_ioctl(struct inode *inode, struct file *filp, 
			unsigned int cmd, unsigned long arg)
{
	printk(KERN_ALERT "cdata: in cdata_ioctl()\n");

    return 0;
}

static ssize_t cdata_read(struct file *filp, char *buf, 
				size_t size, loff_t *off)
{
	printk(KERN_ALERT "cdata: in cdata_read()\n");
    return 0;
}

static ssize_t cdata_write(struct file *filp, const char *buf, 
				size_t count, loff_t *off)
{
    struct cdata_t *cdata = (struct cdata_t *)filp->private_data;
    DECLARE_WAITQUEUE(wait, current);
    int i;

    down(&cdata_sem);
    for (i = 0; i < count; i++) {
        if (cdata->index >= BUFSIZE) {
            add_wait_queue(&cdata->wait, &wait);
            current->state = TASK_INTERRUPTIBLE;

            /*
             */
#if 0
            cdata->timer.expires = jiffies + 500;
            cdata->timer.function = flush_lcd;
            cdata->timer.data = (unsigned long)cdata;
            add_timer(&cdata->timer);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
            //schedule_work_on(1, &cdata->work);
            schedule_work(&cdata->work);
#else
            schedule_task(&cdata->tq);
#endif

            schedule();

            current->state = TASK_RUNNING;
            remove_wait_queue(&cdata->wait, &wait);
        }

        if (copy_from_user(&cdata->data[cdata->index++], &buf[i], 1))
            return -EFAULT;
    }
    up(&cdata_sem);

	return 0;
}

static int cdata_release(struct inode *inode, struct file *filp)
{
	printk(KERN_ALERT "cdata: in cdata_release()\n");
	return 0;
}

static int cdata_mmap(struct file *filp, struct vm_area_struct *vma)
{
    unsigned long start = vma->vm_start;
    unsigned long to;
    unsigned long size = vma->vm_end - vma->vm_start; 

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    to = 0xe0000000;
    remap_pfn_range(vma, start, to >> PAGE_SHIFT, size, vma->vm_page_prot);
#else
    to = 0x33f00000;
    remap_page_range(start, to, size, PAGE_SHARED);
#endif

}

struct file_operations cdata_fops = {	
	open:		cdata_open,
	release:	cdata_release,
	ioctl:		cdata_ioctl,
	read:		cdata_read,
	write:		cdata_write,
    mmap:       cdata_mmap,
};

int my_init_module(void)
{
    int i;

	if (register_chrdev(CDATA_MAJOR, "cdata", &cdata_fops)) {
	    printk(KERN_ALERT "cdata module: can't registered.\n");
    }

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
