// Dominik Matijaca 0036524568
#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/seq_file.h>
#include <linux/cdev.h>
#include <linux/kfifo.h>
#include <linux/log2.h>

#include "config.h"

MODULE_AUTHOR(AUTHOR);
MODULE_LICENSE(LICENSE);

static int max_messages = MAX_MESSAGES;
module_param(max_messages, int, S_IRUGO);
MODULE_PARM_DESC(max_messages, "Maximum number of messages in queue, must be a power of 2");

static int message_size = MESSAGE_SIZE;
module_param(message_size, int, S_IRUGO);
MODULE_PARM_DESC(message_size, "Message size in bytes, must be a power of 2");

static int max_threads = MAX_THREADS;
module_param(max_threads, int, S_IRUGO);
MODULE_PARM_DESC(max_threads, "Maximum number of threads that can read/write simultaneously");

#define buffer_size (max_messages * message_size)

struct shofer_dev *Shofer = NULL;
struct buffer *Buffer = NULL;
char* Fifo_print_data = NULL;
static dev_t Dev_no = 0;

/* prototypes */
static void *buffer_create(size_t, int *);
static struct buffer *fifo_create(size_t, int *);
static void buffer_delete(void *);
static struct shofer_dev *shofer_create(dev_t, struct file_operations *,
	struct buffer *, int *);
static void shofer_delete(struct shofer_dev *);
static void cleanup(void);
static void dump_buffer(char *, struct buffer *);

static int shofer_open(struct inode *, struct file *);
static int shofer_release(struct inode *, struct file *);
static ssize_t shofer_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t shofer_write(struct file *, const char __user *, size_t, loff_t *);

static struct file_operations shofer_fops = {
	.owner =    THIS_MODULE,
	.open =     shofer_open,
	.release =  shofer_release,
	.read =     shofer_read,
	.write =    shofer_write
};

/* init module */
static int __init shofer_module_init(void)
{
	int retval;
	struct buffer *buffer;
	char *fifo_print_data;
	struct shofer_dev *shofer;
	dev_t dev_no = 0;

	printk(KERN_NOTICE "Module 'shofer' started initialization\n");

	/* get device number(s) */
	retval = alloc_chrdev_region(&dev_no, 0, 1, DRIVER_NAME);
	if (retval < 0) {
		printk(KERN_WARNING "%s: can't get major device number %d\n",
			DRIVER_NAME, MAJOR(dev_no));
		return retval;
	}

	/* check given parameters */
	if (
		!is_power_of_2(max_messages) ||
		!is_power_of_2(message_size) ||
		max_threads < 1
	) {
		printk(KERN_WARNING "%s: invalid module parameters\n", DRIVER_NAME);
		retval = -EINVAL;
		goto no_driver;
	}

	/* create a buffer */
	// buffer_size is now guaranteed to be a power of two
	buffer = fifo_create(buffer_size, &retval);
	if (!buffer)
		goto no_driver;
	Buffer = buffer;

	/* create a buffer for printing */
	fifo_print_data = (char*)buffer_create(buffer_size, &retval);
	if (!fifo_print_data)
		goto no_driver;
	Fifo_print_data = fifo_print_data;

	/* create a device */
	shofer = shofer_create(dev_no, &shofer_fops, buffer, &retval);
	if (!shofer)
		goto no_driver;

	printk(KERN_NOTICE "Module 'shofer' initialized with major=%d, minor=%d\n",
		MAJOR(dev_no), MINOR(dev_no));

	Shofer = shofer;
	Dev_no = dev_no;

	return 0;

no_driver:
	cleanup();

	return retval;
}

static void cleanup(void) {
	if (Shofer)
		shofer_delete(Shofer);
	if (Buffer)
		buffer_delete(Buffer);
	if (Fifo_print_data)
		buffer_delete(Fifo_print_data);
	if (Dev_no)
		unregister_chrdev_region(Dev_no, 1);
}

/* called when module exit */
static void __exit shofer_module_exit(void)
{
	printk(KERN_NOTICE "Module 'shofer' started exit operation\n");
	cleanup();
	printk(KERN_NOTICE "Module 'shofer' finished exit operation\n");
}

module_init(shofer_module_init);
module_exit(shofer_module_exit);

/* Create and initialize a single buffer */
static void *buffer_create(size_t size, int *retval)
{
	void *buffer = kmalloc(size, GFP_KERNEL);
	if (!buffer) {
		*retval = -ENOMEM;
		printk(KERN_NOTICE "shofer:kmalloc failed\n");
		return NULL;
	}
	*retval = 0;

	return buffer;
}

static struct buffer *fifo_create(size_t size, int *retval)
{
	struct buffer *buffer = buffer_create(sizeof(struct buffer) + size, retval);
	if (*retval != 0) {
		return NULL;
	}
	*retval = kfifo_init(&buffer->fifo, buffer + 1, size);
	if (*retval) {
		kfree(buffer);
		printk(KERN_NOTICE "shofer:kfifo_init failed\n");
		return NULL;
	}
	spin_lock_init(&buffer->key);
	*retval = 0;

	return buffer;
}

static void buffer_delete(void *buffer)
{
	kfree(buffer);
}

/* Create and initialize a single shofer_dev */
static struct shofer_dev *shofer_create(dev_t dev_no,
	struct file_operations *fops, struct buffer *buffer, int *retval)
{
	struct shofer_dev *shofer = kmalloc(sizeof(struct shofer_dev), GFP_KERNEL);
	if (!shofer){
		*retval = -ENOMEM;
		printk(KERN_NOTICE "shofer:kmalloc failed\n");
		return NULL;
	}
	memset(shofer, 0, sizeof(struct shofer_dev));
	shofer->buffer = buffer;

	init_waitqueue_head(&shofer->readers_waiting);
	init_waitqueue_head(&shofer->writers_waiting);

	cdev_init(&shofer->cdev, fops);
	shofer->cdev.owner = THIS_MODULE;
	shofer->cdev.ops = fops;
	*retval = cdev_add (&shofer->cdev, dev_no, 1);
	shofer->dev_no = dev_no;
	if (*retval) {
		printk(KERN_NOTICE "Error (%d) when adding device shofer\n",
			*retval);
		kfree(shofer);
		shofer = NULL;
	}

	return shofer;
}
static void shofer_delete(struct shofer_dev *shofer)
{
	cdev_del(&shofer->cdev);
	kfree(shofer);
}

/* Called when a process calls "open" on this device */
static int shofer_open(struct inode *inode, struct file *filp)
{
	struct shofer_dev *shofer; /* device information */
	unsigned int flags = filp->f_flags & O_ACCMODE;

	if (flags != O_RDONLY && flags != O_WRONLY)
		return -EINVAL;

	shofer = container_of(inode->i_cdev, struct shofer_dev, cdev);

	if (shofer->threads_active >= max_threads)
		return -EBUSY;
	
	filp->private_data = shofer; /* for other methods */
	shofer->threads_active++;

	return 0;
}

/* Called when a process performs "close" operation */
static int shofer_release(struct inode *inode, struct file *filp)
{
	struct shofer_dev *shofer = filp->private_data;
	shofer->threads_active--;

	return 0;
}

/* Read count bytes from buffer to user space ubuf */
static ssize_t shofer_read(struct file *filp, char __user *ubuf, size_t count,
	loff_t *f_pos /* ignoring f_pos */)
{
	ssize_t retval = 0;
	struct shofer_dev *shofer = filp->private_data;
	unsigned int flags = filp->f_flags & O_ACCMODE;
	struct buffer *buffer = shofer->buffer;
	struct kfifo *fifo = &buffer->fifo;
	unsigned int copied;

	if (flags != O_RDONLY)
		return -EPERM;
	
	if (count != message_size)
		return -EINVAL;
	
	if (kfifo_is_empty(fifo)) {
		if (wait_event_interruptible(shofer->readers_waiting, !kfifo_is_empty(fifo)))
			return -ERESTARTSYS;
	}

	spin_lock(&buffer->key);

	dump_buffer("read-start", buffer);

	retval = kfifo_to_user(fifo, (char __user *) ubuf, count, &copied);
	if (retval)
		printk(KERN_NOTICE "shofer:kfifo_to_user failed\n");
	else
		retval = copied;

	dump_buffer("read-end", buffer);

	spin_unlock(&buffer->key);

	wake_up_interruptible(&shofer->writers_waiting);

	return retval;
}

/* Write count bytes from user space ubuf to buffer */
static ssize_t shofer_write(struct file *filp, const char __user *ubuf,
	size_t count, loff_t *f_pos /* ignoring f_pos */)
{
	ssize_t retval = 0;
	struct shofer_dev *shofer = filp->private_data;
	unsigned int flags = filp->f_flags & O_ACCMODE;
	struct buffer *buffer = shofer->buffer;
	struct kfifo *fifo = &buffer->fifo;
	unsigned int copied;

	if (flags != O_WRONLY)
		return -EPERM;
	
	if (count != message_size)
		return -EINVAL;

	if (kfifo_is_full(fifo)) {
		if (wait_event_interruptible(shofer->writers_waiting, !kfifo_is_full(fifo)))
			return -ERESTARTSYS;
	}

	spin_lock(&buffer->key);

	dump_buffer("write-start", buffer);

	retval = kfifo_from_user(fifo, (char __user *) ubuf, count, &copied);
	if (retval)
		printk(KERN_NOTICE "shofer:kfifo_from_user failed\n");
	else
		retval = copied;

	dump_buffer("write-end", buffer);

	spin_unlock(&buffer->key);

	wake_up_interruptible(&shofer->readers_waiting);

	return retval;
}

static void dump_buffer(char *prefix, struct buffer *b)
{
	unsigned int i;
	int len = kfifo_len(&b->fifo);
	size_t copied;

	printk(KERN_NOTICE "shofer:%s:fifo:size=%u:contains=%u",
		prefix, kfifo_size(&b->fifo), len);
	
	copied = kfifo_out_peek(&b->fifo, Fifo_print_data, len);

	for (i = 0; i < len; i += message_size)
		printk(KERN_NOTICE "shofer:%s:fifo:[%u:%u]=%s",
			prefix, i, i + message_size - 1, Fifo_print_data + i);
}
