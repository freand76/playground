/**                                                     
 * Author: Fredrik Andersson 760617-8932
 **/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/workqueue.h>
#include <linux/uaccess.h>

/* Kernel Module Macros to define Author andf License type */
MODULE_AUTHOR("Fredrik Andersson <freand@gmail.com>");
MODULE_LICENSE("Dual BSD/GPL");

/* I2C Client Variable */
static struct i2c_client *client;

/* Timer Variable */
static struct timer_list eeprom_periodic_timer;
#define POLL_PERIOD HZ /* Poll Once per second */

/* Workqueue Setup */
static void eeprom_periodic_work_func(struct work_struct *work);
static DECLARE_WORK(eeprom_work, eeprom_periodic_work_func);
static struct workqueue_struct *eeprom_workqueue;

/* Data Buffer */
#define DATA_MESSAGE_LENGTH 16
static char data_string[DATA_MESSAGE_LENGTH + 1]; /* One extra to null terminate the string */
static struct semaphore data_string_sem;

/* Wait Queue Setup */
DECLARE_WAIT_QUEUE_HEAD(eeprom_data_queue); /* waked when data_string contains data */

/**
 * i2c probe function, called when module has been inserted and i2c_new_device has been called
 **/ 
static int eeprom_probe(struct i2c_client *client, const struct i2c_device_id *id) {
    printk(KERN_INFO "EEPROM Probe\n");
    /* No initalization is needed, nothing done here! */
    return 0;
}

/**
 * i2c remove function, called when module has been removed and i2c_unregister_device has been called
 **/ 
static int eeprom_remove(struct i2c_client *client) {
	printk(KERN_INFO "EEPROM Remove\n");
    /* No teardown is needed, nothing done here! */
    return 0;
}

/**
 * i2c device table, not used since I do not have different data sets for different HW
 **/ 
static const struct i2c_device_id eeprom_id[] = {
    { "eeprom", 0},
    {},
};

/* Register Device Table */
MODULE_DEVICE_TABLE(i2c, eeprom_id);

/**
 * i2c driver struct, name and function pointers to probe/remove 
 **/ 
static struct i2c_driver eeprom_driver = {
    .driver = {
        .owner = THIS_MODULE,
        .name  = "eeprom",  
    },
    .probe    = eeprom_probe,
    .remove   = eeprom_remove,    
    .id_table = eeprom_id
};

/**
 * Char Driver File Operation: Open
 **/
static int eeprom_drv_open( struct inode *inode, struct file *file ) {
    printk(KERN_INFO "EEPROM Device FOPS Open\n");
    /* Nothing special is done in open */
    return 0;
}

/**
 * Char Driver File Operation: Close
 **/
static int eeprom_drv_close( struct inode *inode, struct file *file ) {
    printk(KERN_INFO "EEPROM Device FOPS Close\n");
    /* Nothing special is done in close */
    return 0;
}

/**
 * Char Driver File Operation: Read
 **/
static ssize_t eeprom_drv_read( struct file* F, char *buf, size_t count, loff_t *f_pos ) {
    int len;

    printk(KERN_INFO "EEPROM Device FOPS Read\n");

    /* Wait for valid string from polling process */
    wait_event_interruptible(eeprom_data_queue, strlen(data_string) > 0);

    /* Take semaphore */
	if (down_interruptible(&data_string_sem)) {
		return -ERESTARTSYS;
    }

    /* Get String Length */
    len = strlen(data_string);

    /* Copy String to User */
    if (copy_to_user(buf, data_string, len) != 0) {
        printk(KERN_ERR "EEPROM Device FOPS Read: copy_to_user error\n");
        /* Release semaphore */
        up(&data_string_sem);
        /* Return ERROR */
        return -EFAULT;
    };

    /* Kill string when we have used it */
    data_string[0] = '\0';
    
    /* Release semaphore */
    up(&data_string_sem);

    /* Return String Length */
    return len;
}

/**
 * Char Driver File Operation: Write
 **/
static ssize_t eeprom_drv_write( struct file* F, const char *buf, size_t count, loff_t *f_pos ) {
    printk(KERN_INFO "EEPROM Device FOPS Write: %d\n", (int)count);
    /* No Write implementation done yet */
    return count;
}

/**
 * Periodic Work Function
 **/

#define NOF_MESSAGES_IN_POLL 2

static void eeprom_periodic_work_func(struct work_struct *work) {
    char tx[2];
    char rx[DATA_MESSAGE_LENGTH];
    struct i2c_msg txrx_msg[NOF_MESSAGES_IN_POLL];

    /* Write 0x0000 to eeprom (use address 0) */
    tx[0] = 0x00;
    tx[1] = 0x00;
    txrx_msg[0].addr = client->addr;
    txrx_msg[0].flags = 0;
    txrx_msg[0].len = 2;
    txrx_msg[0].buf = tx;

    /* Read 16 bytes from eeprom */
    txrx_msg[1].addr = client->addr;
    txrx_msg[1].flags = I2C_M_RD;
    txrx_msg[1].len = DATA_MESSAGE_LENGTH;
    txrx_msg[1].buf = rx;

    /* Execute */
    if (NOF_MESSAGES_IN_POLL == i2c_transfer(client->adapter, txrx_msg, NOF_MESSAGES_IN_POLL)) {
        int idx;

        /* Take semaphore */
        if (down_interruptible(&data_string_sem) == 0) {
            for (idx=0;idx<DATA_MESSAGE_LENGTH;idx++) {
                char c = rx[idx];
                if ((c >= 0x20) && (c < 0x7f)) {
                    data_string[idx] = c;
                } else {
                    data_string[idx] = '.';
                }
            }
            /* Null terminate string */
            data_string[DATA_MESSAGE_LENGTH] = '\0';
            
            /* Release semaphore */
            up(&data_string_sem);

            /* Wake up read function */
            wake_up_interruptible(&eeprom_data_queue);
        } else {
            /* Do we ever end here? maybe.... */
            printk(KERN_ERR "Did not get semaphore\n");
        }
    } else {
        /* Do we ever end here? maybe.... */
        printk(KERN_ERR "EEPROM Communication Error (i2c_transfer)\n");
    }
}

/**
 *  Periodic Timer Function
 **/
static void eeprom_periodic_timer_func(unsigned long unused) {
    /* Setup next timer event */
    eeprom_periodic_timer.expires = jiffies + POLL_PERIOD;
	add_timer(&eeprom_periodic_timer);

    /* Schedule workqueue to run */
	queue_work(eeprom_workqueue, &eeprom_work);
} 

/* File Operations Struct with function pointers to file functions */
static struct file_operations eeprom_drv_fops = {
    .owner        = THIS_MODULE,
    .open         = eeprom_drv_open,
    .read         = eeprom_drv_read,
    .write        = eeprom_drv_write,
    .release      = eeprom_drv_close,
};

/* Device Node Variales */
static dev_t first;
static struct cdev c_dev; 
static struct class *cl;

/* i2c Board Info struct, sets the i2c address of the i2c_device */
static struct i2c_board_info eeprom_info = {
    I2C_BOARD_INFO("eeprom", 0x50),
};

/* Helper Function to cleanup device node */
static void cleanup_device_node(void) {
    device_destroy(cl, first);
    class_destroy(cl);
    unregister_chrdev_region(first, 1);
}

/* Kernel Module Init Function */
static int eeprom_module_init(void) {
    struct i2c_adapter *adapter;
	int retval;

    /* Create Device Node */
    if (alloc_chrdev_region(&first, 0, 1, "char_dev") < 0) {
        return -EINVAL;
    }
    if ((cl = class_create(THIS_MODULE, "chardrv")) == NULL) {
        unregister_chrdev_region(first, 1);
        return -EINVAL;
    }
    if (device_create(cl, NULL, first, NULL, "eeprom") == NULL) {
        class_destroy(cl);
        unregister_chrdev_region(first, 1);
        return -EINVAL;
    }
    cdev_init(&c_dev, &eeprom_drv_fops);
    if (cdev_add(&c_dev, first, 1) == -1) {
        cleanup_device_node();
        return -EINVAL;
    }

    /* Create EEPROM Driver */
    retval = i2c_add_driver(&eeprom_driver);
    if (retval != 0) {
        cdev_del( &c_dev );
        cleanup_device_node();
        return retval;
    }
    adapter = i2c_get_adapter(0);
    if (adapter == NULL) {
        cdev_del( &c_dev );
        cleanup_device_node();
        return -EINVAL;
    }

    /* Add new device to I2C bus */
    client = i2c_new_device(adapter, &eeprom_info);
    if (client == NULL) {
        cdev_del( &c_dev );
        cleanup_device_node();
        return -EINVAL;
    }

	/* Set up workqueue */
	eeprom_workqueue = create_singlethread_workqueue("eeprom");

    /* Set up periodic timer */
	init_timer(&eeprom_periodic_timer);
	eeprom_periodic_timer.function = eeprom_periodic_timer_func;
	eeprom_periodic_timer.data = 0;
    eeprom_periodic_timer.expires = jiffies + POLL_PERIOD;
	add_timer(&eeprom_periodic_timer);

    /* Initialize semaphore */
	sema_init(&data_string_sem, 1);

    /* Nothing polled yet */
    data_string[0] = '\0';

    /* Return OK */
	printk(KERN_INFO "EEPROM Driver Loaded OK\n");
    return 0;
}

/* Kernel Module Exit Function */
static void eeprom_module_exit(void) {
	printk(KERN_INFO "EEPROM Driver Removed\n");
    /* Flush and Kill workqueue */
	flush_workqueue(eeprom_workqueue);
	destroy_workqueue(eeprom_workqueue);

    /* Kill Timer */
    del_timer_sync(&eeprom_periodic_timer);

    /* Kill I2C Device */
    i2c_unregister_device(client);
    i2c_del_driver(&eeprom_driver);

    /* Kill device node */
    cdev_del( &c_dev );
    cleanup_device_node();
}

module_init(eeprom_module_init);
module_exit(eeprom_module_exit);
