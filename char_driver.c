#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/string.h>
#include <asm/uaccess.h>
#include <linux/uaccess.h>	
#include <linux/slab.h>
#include <linux/fs.h>


#define MYDEV_NAME "mycdev"

#define ramdisk_size (size_t) (16 * PAGE_SIZE) // ramdisk size 

#define CDEV_IOC_MAGIC 'k'
#define asp_CLEAR_BUFF _IO(CDEV_IOC_MAGIC, 1)
//by default create 3 devices
static int Num_DEVICES = 3;

//my structure
struct asp_mycdev {
	struct cdev cdev;
	char *ramDisk;
  	struct semaphore sem;
	int devNo;
	struct list_head list;
	unsigned long buffer_size;
};

//take parameter from insmod otherwise consider default module param
module_param(Num_DEVICES, int, S_IRUGO);
static unsigned int mycdev_major = 0;
static struct class *class_device = NULL;
LIST_HEAD(DeviceListHead);

int myasp_open(struct inode *inode, struct file *filp)// open function for device driver 
{
    unsigned int major_num, minor_num; 
	struct list_head *position = NULL;
    struct asp_mycdev *device = NULL;	
	major_num = imajor(inode);
    minor_num = iminor(inode);
		
	if (major_num != mycdev_major || minor_num < 0 || minor_num >= Num_DEVICES) {
                printk(" device Not found...\n");
                return -ENODEV;
        }

	list_for_each(position, &DeviceListHead) {
		device = list_entry(position, struct asp_mycdev, list);
		if(device->devNo == minor_num) {
			break;
		}
	}

        filp->private_data = device; 

        return 0;
}

int myasp_release(struct inode *inode, struct file *filp)// relese function 
{
        return 0;
}

ssize_t myasp_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)//function to read data from char device driver
{
	struct asp_mycdev *device = (struct asp_mycdev *)filp->private_data;
	ssize_t return_value = 0;
	loff_t i;
	unsigned char *temporary_buff = NULL;//temp buffer to store data

	if (down_interruptible(&(device->sem))!=0) {
		pr_err("%s: Not able to able while reading\n", MYDEV_NAME);
	}

	if (*f_pos >= device->buffer_size) { /* End of File*/
		up(&(device->sem));
	        return return_value;
        }

	temporary_buff = (unsigned char*) kzalloc(count, GFP_KERNEL);	//assigning space to buffer

	for (i = 0; i < count; i++) {
		temporary_buff[i] = device->ramDisk[*f_pos + i];
	}
	*f_pos += count;

	if (copy_to_user(buf, temporary_buff, count) != 0) //copying to user space
	{
		return_value = -EFAULT;
		kfree(temporary_buff);
                up(&(device->sem));
                return return_value;
	}

	return_value = count;

        kfree(temporary_buff);
	up(&(device->sem));
	return return_value;
}

loff_t myasp_llseek(struct file *filp, loff_t off, int position) //lseek
{
	struct asp_mycdev *dev = (struct asp_mycdev *)filp->private_data;
	loff_t updatedposition = 0;       

	if (down_interruptible(&(dev->sem))!=0) {
		pr_err("%s: not able to lock the device\n", MYDEV_NAME);
	}

	switch(position) {
		case 0: /* SEEK_SET */
			updatedposition = off;
			break;

		case 1: /* SEEK_CUR */
			updatedposition = filp->f_pos + off;
			break;

		case 2: /* SEEK_END */
			updatedposition = dev->buffer_size + off;
			break;

		default: /* can't happen */
			updatedposition = -EINVAL;
	
	}

        //increase buffer size by requested amount
        unsigned char* tmp_buffer;
	if (updatedposition > dev->buffer_size) {
                tmp_buffer = (unsigned char*)kzalloc(updatedposition, GFP_KERNEL);
                memcpy(tmp_buffer, dev->ramDisk, dev->buffer_size);
                kfree(dev->ramDisk);
                dev->ramDisk = tmp_buffer;
                dev->buffer_size = updatedposition;
                
	}
//update file pointer
	filp->f_pos = updatedposition;

	up(&(dev->sem));
	return updatedposition;
}

//ioctl function for clearing the data
long myasp_ioctl (struct file *filp, unsigned int cmd, unsigned long arg)
{
        struct asp_mycdev *dev = (struct asp_mycdev *)filp->private_data;

	if (cmd != asp_CLEAR_BUFF) {
		pr_err("Wrong command\n");
		return -1;
	}

	if (down_interruptible(&(dev->sem))!=0) {
		pr_err("%s: Not able to lock while clearing\n", MYDEV_NAME);
	}

        memset((volatile void *)dev->ramDisk, 0, dev->buffer_size);  	// Clears the contents of the buffer
        filp->f_pos = 0;						// Resets the file pointer position

	up(&(dev->sem));
	return 1;
}

//write function for device driver
ssize_t myasp_write(struct file *filp, const char __user *buf, size_t count, 
		loff_t *f_pos)
{
	struct asp_mycdev *dev = (struct asp_mycdev *)filp->private_data;
	ssize_t return_value = 0;
	unsigned char *temporary_buff = NULL;
	loff_t i;

	if (down_interruptible(&(dev->sem))!=0) {
		pr_err("%s: Not able to write while writting\n", MYDEV_NAME);
	}

	if (*f_pos >= dev->buffer_size) {
		return_value = -EINVAL;
		up(&(dev->sem));
		return return_value;
	} 

	temporary_buff = (unsigned char*) kzalloc(count, GFP_KERNEL);	//allocating space for buffer

	if (copy_from_user(temporary_buff, buf, count) != 0)//copy data from userspace
	{
		return_value = -EFAULT;
		kfree(temporary_buff);
                up(&(dev->sem));
	        return return_value;
	}

	for (i = 0; i < count; i++) {
		dev->ramDisk[*f_pos + i] = temporary_buff[i];	// ramdisk to temp buffer data
	}
	*f_pos += count;

	return_value = count;//returning number of words written

	kfree(temporary_buff);
	up(&(dev->sem));
	return return_value;
}

struct file_operations mycdev_fops = {
        .owner =    THIS_MODULE,
        .read =     myasp_read,
        .write =    myasp_write,
        .open =     myasp_open,
        .release =  myasp_release,
        .llseek =   myasp_llseek,
	.unlocked_ioctl = myasp_ioctl,
};



static int myasp_control_device(struct asp_mycdev *dev, int minor, 
        struct class *class)
{
        int err = 0;
        dev_t devno = MKDEV(mycdev_major, minor);
        struct device *device = NULL;

        dev->buffer_size = ramdisk_size;
	dev->devNo = minor;
	dev->ramDisk = NULL;
	printk("******registering for device %d****",minor);
	sema_init(&(dev->sem),1);
        
        cdev_init(&dev->cdev, &mycdev_fops);
        dev->cdev.owner = THIS_MODULE;

        dev->ramDisk = (unsigned char*)kzalloc(dev->buffer_size, GFP_KERNEL);

        err = cdev_add(&dev->cdev, devno, 1);

        device = device_create(class, NULL, devno, NULL,
				MYDEV_NAME "%d", minor);
        if (IS_ERR(device)) {
                err = PTR_ERR(device);
                printk("Error %d while trying to create %s%d", err,
			MYDEV_NAME, minor);
                cdev_del(&dev->cdev);
                return err;
        }

        return 0;
}





static void asp_cleanup(void)
{
        int i = 0;
        struct asp_mycdev *device = NULL;
        struct list_head *position = NULL;

redo: //
	list_for_each(position, &DeviceListHead) {
		device = list_entry(position, struct asp_mycdev, list);
		device_destroy(class_device, MKDEV(mycdev_major, i));
        	cdev_del(&device->cdev);
        	kfree(device->ramDisk);
		list_del(&(device->list));
                kfree(device);
		i++;
        	goto redo;
        }

	if (class_device)
		class_destroy(class_device);

	unregister_chrdev_region(MKDEV(mycdev_major, 0), Num_DEVICES);
	return;
}

static int __init my_init(void)
{
        int err = 0;
        int i = 0;
        dev_t device = 0;
        struct asp_mycdev *mycdev_device = NULL;

        if ( alloc_chrdev_region(&device, 0, Num_DEVICES, MYDEV_NAME) < 0 ) {
                printk("alloc_chrdev_region() failed\n");
                return err;
        }

        mycdev_major = MAJOR(device);
        class_device = class_create(THIS_MODULE, MYDEV_NAME);
        if (IS_ERR(class_device)) {
                err = PTR_ERR(class_device);
                asp_cleanup();
                return err;
        }
 
	for (i = 0; i <= Num_DEVICES; ++i) 
        {     
		mycdev_device = (struct asp_mycdev *)kzalloc(sizeof(struct asp_mycdev), 
						GFP_KERNEL);
		if (mycdev_device == NULL) {
			err = -ENOMEM;
			asp_cleanup();
                        return err;
		}

		err = myasp_control_device(mycdev_device, i, class_device);

                if (err) {
                        asp_cleanup();
                        return err;
                }

		INIT_LIST_HEAD(&(mycdev_device->list));
		list_add(&(mycdev_device->list), &DeviceListHead);
        }

        return 0;

}

static void __exit my_exit(void)
{
        asp_cleanup();
        return;
}

module_init(my_init);
module_exit(my_exit);

MODULE_AUTHOR("sonali");
MODULE_LICENSE("GPL v2");
