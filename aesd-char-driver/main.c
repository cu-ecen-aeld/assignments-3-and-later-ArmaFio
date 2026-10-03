/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include "aesd-circular-buffer.h"
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Armando Fiorini"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    struct aesd_dev *dev;
    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev;
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    size_t entryOffset;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    mutex_lock(&aesd_device.lock);
    struct aesd_buffer_entry *offset = aesd_circular_buffer_find_entry_offset_for_fpos(&aesd_device.buffer, (size_t)*f_pos, &entryOffset );
    if(offset == NULL){
        goto out;
    }
    size_t copy_size = min_t(size_t, ((offset->size) - entryOffset), count);
    long uncopied = copy_to_user(buf, (offset->buffptr)+entryOffset, copy_size);
    retval = copy_size-uncopied;
    *f_pos += retval; 
    out:
        mutex_unlock(&aesd_device.lock);
        return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    const char *rest = NULL;
    mutex_lock(&aesd_device.lock);
    if((aesd_device.workingBuffer.buffptr = krealloc(aesd_device.workingBuffer.buffptr, aesd_device.workingBuffer.size + count, GFP_KERNEL)) == NULL){
        mutex_unlock(&aesd_device.lock);
        return retval;
    }
    unsigned long ret = copy_from_user((void*) aesd_device.workingBuffer.buffptr+aesd_device.workingBuffer.size, buf, count);
    if (ret != 0){
        mutex_unlock(&aesd_device.lock);
        return -EFAULT;
    }
    aesd_device.workingBuffer.size += count;
    if (memchr(aesd_device.workingBuffer.buffptr, '\n', aesd_device.workingBuffer.size) != NULL){
        rest = aesd_circular_buffer_add_entry(&aesd_device.buffer, &aesd_device.workingBuffer);
        aesd_device.workingBuffer.buffptr = NULL;
        aesd_device.workingBuffer.size = 0;
        kfree(rest);
    }
    retval = count;
    mutex_unlock(&aesd_device.lock);
    return retval;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    aesd_circular_buffer_init(&aesd_device.buffer);
    mutex_init(&aesd_device.lock);

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    for(int i = 0; i<10 ; i++){
        kfree(aesd_device.buffer.entry[i].buffptr);
    }

    kfree(aesd_device.workingBuffer.buffptr);

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
