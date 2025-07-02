// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright 2025, University of Ljubljana
 *
 * This file is part of Cora-Z7-DAQ-OS.
 * Cora-Z7-DAQ-OS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or any later version.
 * Cora-Z7-DAQ-OS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along with Cora-Z7-DAQ-OS.
 * If not, see <https://www.gnu.org/licenses/>.
 */

#include <linux/atomic.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/wait.h>
#include <linux/poll.h>

#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>

#include "kfifo-iomod.h"

/* Standard module information, edit as appropriate */
MODULE_LICENSE("GPL");
MODULE_AUTHOR
    ("Stas Bucik");
MODULE_DESCRIPTION
    ("daqdrv - loadable module for Data acquisition system implemented on FPGA.");

#define DRIVER_NAME "daqdrv"

#define FPGA_BUF_LEN 4096
#define FIFO_BUF_LEN FPGA_BUF_LEN * 8

#define ADC_RUN_BIT 0
#define DAC_RUN_BIT 1
#define CLEAR_BIT_C 2

#define OVERWRITE_BIT 0

#define CLK_SOFT_RST_REG         0x0
#define CLK_STAT_REG             0x4
#define CLK_MONITOR_ERR_STAT_REG 0x8
#define CLK_INTERRUPT_STAT_REG   0xC
#define CLK_INTERRUPT_EN_REG     0x10
#define CLK_CLK_CONF_REG_0       0x200
#define CLK_CLK_CONF_REG_1       0x204
#define CLK_CLK_CONF_REG_2       0x208
#define CLK_CLK_CONF_REG_3       0x20C
#define CLK_CLK_CONF_REG_4       0x210
#define CLK_CLK_CONF_REG_23      0x25C

#define SAMPLE_RATE_200KSPS 0x0
#define SAMPLE_RATE_500KSPS 0x1
#define SAMPLE_RATE_1MSPS   0x2
#define SAMPLE_RATE_2MSPS   0x3

#define SR_200KSPS_R0 0x00000801
#define SR_200KSPS_R2 0x0000007d
#define SR_500KSPS_R0 0x00000a01
#define SR_500KSPS_R2 0x0001f43e
#define SR_1MSPS_R0   0x00000a01
#define SR_1MSPS_R2   0x0000fa1f
#define SR_2MSPS_R0   0x00000a01
#define SR_2MSPS_R2   0x0002710f

#define REG_SET_BIT(reg, bit) reg = reg | (1u << bit)
#define REG_UNSET_BIT(reg, bit) reg = reg & ~(1u << bit)
#define REG_GET_BIT(reg, bit) ((reg & (1u << bit)) >> bit)

static void sampleRate_release(struct kobject *);
static ssize_t sampleRate_store(struct kobject *, struct kobj_attribute *, const char *, size_t);
static irqreturn_t daqdrv_irq(int, void *);
static int daqdrv_open(struct inode *, struct file *);
static int daqdrv_release(struct inode *, struct file *);
static ssize_t daqdrv_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t daqdrv_write(struct file *, const char __user *, size_t, loff_t *);
static unsigned int daqdrv_poll(struct file *, struct poll_table_struct *);

static int major; /* major number assigned to our device driver */
static int num_of_dev = 1;
enum {
	CDEV_NOT_USED,
	CDEV_EXCLUSIVE_OPEN
};
static atomic_t already_open = ATOMIC_INIT(CDEV_NOT_USED);

static struct class *cls;

static struct file_operations chardev_fops = {
	.read = daqdrv_read,
	.write = daqdrv_write,
	.open = daqdrv_open,
	.release = daqdrv_release,
	.poll = daqdrv_poll,
};

struct daqdrv_local {
	int irq;
	struct cdev chardev;
	unsigned long buffer_mem_start;
	unsigned long buffer_mem_end;
	unsigned long ctrl_mem_start;
	unsigned long ctrl_mem_end;
	unsigned long stat_mem_start;
	unsigned long stat_mem_end;
	unsigned long clk_mem_start;
	unsigned long clk_mem_end;
	void __iomem *buffer_base_addr;
	void __iomem *ctrl_base_addr;
	void __iomem *stat_base_addr;
	void __iomem *clk_base_addr;
	struct kfifo_iomod fifo;
	struct kobject sampleRate_module_object;
	struct wait_queue_head wait_queue_head;
	bool overflowing;
	bool prev_overflowing;
	bool allowed_to_read;
};

static const struct kobj_type dynamic_kobj_ktype = {
	.release	= sampleRate_release,
	.sysfs_ops	= &kobj_sysfs_ops,
};

static void sampleRate_release(struct kobject *kobj)
{
	pr_debug("(%p): %s\n", kobj, __func__);
}

static u8 sampleRate;

static ssize_t sampleRate_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	if (count < 1) {
		return -EINVAL;
	}

	struct daqdrv_local *lp = container_of(kobj, struct daqdrv_local, sampleRate_module_object);
	if (lp == NULL) {
		printk("drv data is null\n");
		return -ENOTRECOVERABLE;
	}

	if (lp->allowed_to_read == true) {
		return -EBUSY;
	}

	unsigned long number = 0;
	int ret_conversion = kstrtoul(buf, 10, &number);
	if (ret_conversion) {
		return ret_conversion;
	}

	if (number == SAMPLE_RATE_2MSPS) {
		iowrite32(SR_2MSPS_R0, lp->clk_base_addr + CLK_CLK_CONF_REG_0);
		iowrite32(SR_2MSPS_R2, lp->clk_base_addr + CLK_CLK_CONF_REG_2);
	} else if(number == SAMPLE_RATE_1MSPS) {
		iowrite32(SR_1MSPS_R0, lp->clk_base_addr + CLK_CLK_CONF_REG_0);
		iowrite32(SR_1MSPS_R2, lp->clk_base_addr + CLK_CLK_CONF_REG_2);
	} else if(number == SAMPLE_RATE_500KSPS) {
		iowrite32(SR_500KSPS_R0, lp->clk_base_addr + CLK_CLK_CONF_REG_0);
		iowrite32(SR_500KSPS_R2, lp->clk_base_addr + CLK_CLK_CONF_REG_2);
	} else if(number == SAMPLE_RATE_200KSPS) {
		iowrite32(SR_200KSPS_R0, lp->clk_base_addr + CLK_CLK_CONF_REG_0);
		iowrite32(SR_200KSPS_R2, lp->clk_base_addr + CLK_CLK_CONF_REG_2);
	} else {
		return -EINVAL;
	}

	iowrite32(2, lp->clk_base_addr + CLK_CLK_CONF_REG_23); // use registers we just set instead of vivado generated settings
	iowrite32(3, lp->clk_base_addr + CLK_CLK_CONF_REG_23); // apply

	u32 clk_monitor_err_stat_reg = ioread32(lp->clk_base_addr + CLK_MONITOR_ERR_STAT_REG);

	if (clk_monitor_err_stat_reg) {
		printk("error configuring clock!");
		printk("clk_monitor_err_stat_reg is %#010x\n", clk_monitor_err_stat_reg);
		iowrite32(0, lp->clk_base_addr + CLK_CLK_CONF_REG_23); // fallback to vivado generated settings
		iowrite32(1, lp->clk_base_addr + CLK_CLK_CONF_REG_23); // apply
	}

	return count;
}

static struct kobj_attribute sampleRate_attribute = __ATTR_WO(sampleRate);

static irqreturn_t daqdrv_irq(int irq, void *lp)
{
	struct daqdrv_local *lpp = (struct daqdrv_local *)lp;
	u32 availible = kfifo_iomod_avail(&(lpp->fifo));

	if (lpp->allowed_to_read == false) {
		return IRQ_HANDLED;
	}

	lpp->prev_overflowing = lpp->overflowing;

	if (availible < 4*FPGA_BUF_LEN) {
		lpp->overflowing = true;
	} else {
		lpp->overflowing = false;
	}

	if (lpp->overflowing == true && lpp->prev_overflowing == false) {
		printk("Started overflowing! Dropping data, no space in fifo.\n");
	}

	if (lpp->overflowing == false && lpp->prev_overflowing == true) {
		printk("Stopped overflowing.\n");
	}

	if (lpp->overflowing == false) {
		kfifo_iomod_in(&(lpp->fifo), lpp->buffer_base_addr, 4*FPGA_BUF_LEN);
		u32 stat_reg = ioread32(lpp->stat_base_addr);

		if (REG_GET_BIT(stat_reg, OVERWRITE_BIT)) {
			printk("FPGA buffer might be overwritten, IRQ was too slow!");
		}
	}

	wake_up(&(lpp->wait_queue_head));
	return IRQ_HANDLED;
}

static int daqdrv_open(struct inode *inode, struct file *file)
{
	if (atomic_cmpxchg(&already_open, CDEV_NOT_USED, CDEV_EXCLUSIVE_OPEN))
		return -EBUSY;

	try_module_get(THIS_MODULE);

	struct daqdrv_local *lp = container_of(inode->i_cdev, struct daqdrv_local, chardev);
	if (lp == NULL) {
		printk("drv data is null\n");
		return -ENOTRECOVERABLE;
	}

	u32 ctrl_reg = ioread32(lp->ctrl_base_addr);
	REG_SET_BIT(ctrl_reg, CLEAR_BIT_C);
	iowrite32(ctrl_reg, lp->ctrl_base_addr);
	REG_UNSET_BIT(ctrl_reg, CLEAR_BIT_C);
	iowrite32(ctrl_reg, lp->ctrl_base_addr);

	kfifo_iomod_reset_out(&(lp->fifo));
	lp->overflowing = false;
	lp->prev_overflowing = false;

	lp->allowed_to_read = true;
	enable_irq(lp->irq);
	
	REG_SET_BIT(ctrl_reg, ADC_RUN_BIT);
	REG_SET_BIT(ctrl_reg, DAC_RUN_BIT);
	iowrite32(ctrl_reg, lp->ctrl_base_addr);
	return 0;
}

static int daqdrv_release(struct inode *inode, struct file *file)
{
	if (inode->i_cdev == NULL) {
		printk("can't find chardev\n");
		return -ENOTRECOVERABLE;
	}

	struct daqdrv_local *lp = container_of(inode->i_cdev, struct daqdrv_local, chardev);
	if (lp == NULL) {
		printk("drv data is null");
		return -ENOTRECOVERABLE;
	}
	
	lp->allowed_to_read = false;
	disable_irq(lp->irq);
	
	u32 ctrl_reg = ioread32(lp->ctrl_base_addr);
	REG_UNSET_BIT(ctrl_reg, ADC_RUN_BIT);
	REG_UNSET_BIT(ctrl_reg, DAC_RUN_BIT);
	iowrite32(ctrl_reg, lp->ctrl_base_addr);

	atomic_set(&already_open, CDEV_NOT_USED);
	module_put(THIS_MODULE);
	return 0;
}

static ssize_t daqdrv_read(struct file *filp, char __user *buffer, size_t length, loff_t *offset)
{
	if (filp->f_inode == NULL) {
		printk("can't find inode\n");
		return -ENOTRECOVERABLE;
	}

	if (filp->f_inode->i_cdev == NULL) {
		printk("can't find chardev\n");
		return -ENOTRECOVERABLE;
	}

	struct daqdrv_local *lp = container_of(filp->f_inode->i_cdev, struct daqdrv_local, chardev);
	if (lp == NULL) {
		printk("drv data is null\n");
		return -ENOTRECOVERABLE;
	}

	size_t availible_data = (size_t)kfifo_iomod_len(&(lp->fifo));
	size_t min_length = min(length, availible_data);
	size_t aligned_len = min_length - (min_length % 4);

	if (aligned_len == 0) {
		return -EAGAIN;
	}

	size_t actual_len = 0;
	int ret_copy = kfifo_iomod_to_user(&(lp->fifo), buffer, aligned_len, &actual_len);

	if (ret_copy) {
		printk("EFAULT when copying to userspace!");
		return ret_copy;
	}

	return actual_len;
}

unsigned int daqdrv_poll(struct file *filp, struct poll_table_struct *wait)
{
	if (filp->f_inode == NULL) {
		printk("can't find inode\n");
		return -ENOTRECOVERABLE;
	}

	if (filp->f_inode->i_cdev == NULL) {
		printk("can't find chardev\n");
		return -ENOTRECOVERABLE;
	}

	struct daqdrv_local *lp = container_of(filp->f_inode->i_cdev, struct daqdrv_local, chardev);
	if (lp == NULL) {
		printk("drv data is null\n");
		return -ENOTRECOVERABLE;
	}

	poll_wait(filp, &(lp->wait_queue_head), wait);

	unsigned int retval = 0;

	size_t availible_data = (size_t)kfifo_iomod_len(&(lp->fifo));
	size_t aligned_len = availible_data - (availible_data % 4);

	if (aligned_len != 0) {
		retval = POLLIN | POLLRDNORM;
	}

	return retval;
}

static ssize_t daqdrv_write(struct file *filp, const char __user *buff, size_t len, loff_t *off)
{
	pr_alert("Sorry, this operation is not supported.\n");
	return -EINVAL;
}

static int daqdrv_probe(struct platform_device *pdev)
{
	//struct resource *r_irq; /* Interrupt resources */
	struct resource *r_mem_buff; /* IO mem resources */
	struct resource *r_mem_ctrl; /* IO mem resources */
	struct resource *r_mem_stat; /* IO mem resources */
	struct resource *r_mem_clk; /* IO mem resources */
	struct device *dev = &pdev->dev;
	struct daqdrv_local *lp = NULL;
	dev_t dvt;

	int rc = 0;
	dev_info(dev, "Device Tree Probing\n");
	/* Get iospace for the device */

	// Get iospace for buffer
	r_mem_buff = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r_mem_buff) {
		dev_err(dev, "invalid address for buffer\n");
		return -ENODEV;
	}

	// Get iospace for ctrl
	r_mem_ctrl = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!r_mem_ctrl) {
		dev_err(dev, "invalid address for ctrl\n");
		return -ENODEV;
	}

	// Get iospace for stat
	r_mem_stat = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (!r_mem_stat) {
		dev_err(dev, "invalid address for stat\n");
		return -ENODEV;
	}

	// Get iospace for clk
	r_mem_clk = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	if (!r_mem_clk) {
		dev_err(dev, "invalid address for clk\n");
		return -ENODEV;
	}

	// init and allocate daqdrv_local structure
	lp = (struct daqdrv_local *) kmalloc(sizeof(struct daqdrv_local), GFP_KERNEL);
	if (!lp) {
		dev_err(dev, "Cound not allocate daqdrv device\n");
		return -ENOMEM;
	}
	dev_set_drvdata(dev, lp);
	lp->buffer_mem_start = r_mem_buff->start;
	lp->buffer_mem_end = r_mem_buff->end;
	lp->ctrl_mem_start = r_mem_ctrl->start;
	lp->ctrl_mem_end = r_mem_ctrl->end;
	lp->stat_mem_start = r_mem_stat->start;
	lp->stat_mem_end = r_mem_stat->end;
	lp->clk_mem_start = r_mem_clk->start;
	lp->clk_mem_end = r_mem_clk->end;
	lp->allowed_to_read = false;
	sampleRate = SAMPLE_RATE_2MSPS;

	// request memory region for buffer
	if (!request_mem_region(lp->buffer_mem_start,
				lp->buffer_mem_end - lp->buffer_mem_start + 1,
				DRIVER_NAME)) {
		dev_err(dev, "Couldn't lock memory region at %p\n",
			(void *)lp->buffer_mem_start);
		rc = -EBUSY;
		goto error1;
	}

	// request memory region for ctrl
	if (!request_mem_region(lp->ctrl_mem_start,
				lp->ctrl_mem_end - lp->ctrl_mem_start + 1,
				DRIVER_NAME)) {
		dev_err(dev, "Couldn't lock memory region at %p\n",
			(void *)lp->ctrl_mem_start);
		rc = -EBUSY;
		goto error2;
	}

	// request memory region for stat
	if (!request_mem_region(lp->stat_mem_start,
				lp->stat_mem_end - lp->stat_mem_start + 1,
				DRIVER_NAME)) {
		dev_err(dev, "Couldn't lock memory region at %p\n",
			(void *)lp->stat_mem_start);
		rc = -EBUSY;
		goto error3;
	}

	// request memory region for clk
	if (!request_mem_region(lp->clk_mem_start,
				lp->clk_mem_end - lp->clk_mem_start + 1,
				DRIVER_NAME)) {
		dev_err(dev, "Couldn't lock memory region at %p\n",
			(void *)lp->clk_mem_start);
		rc = -EBUSY;
		goto error4;
	}

	// remap buffer
	lp->buffer_base_addr = ioremap(lp->buffer_mem_start, lp->buffer_mem_end - lp->buffer_mem_start + 1);
	if (!lp->buffer_base_addr) {
		dev_err(dev, "daqdrv: Could not allocate iomem for buffer\n");
		rc = -EIO;
		goto error5;
	}

	// remap ctrl
	lp->ctrl_base_addr = ioremap(lp->ctrl_mem_start, lp->ctrl_mem_end - lp->ctrl_mem_start + 1);
	if (!lp->ctrl_base_addr) {
		dev_err(dev, "daqdrv: Could not allocate iomem for ctrl\n");
		rc = -EIO;
		goto error6;
	}

	// remap stat
	lp->stat_base_addr = ioremap(lp->stat_mem_start, lp->stat_mem_end - lp->stat_mem_start + 1);
	if (!lp->stat_base_addr) {
		dev_err(dev, "daqdrv: Could not allocate iomem for stat\n");
		rc = -EIO;
		goto error7;
	}

	// remap clk
	lp->clk_base_addr = ioremap(lp->clk_mem_start, lp->clk_mem_end - lp->clk_mem_start + 1);
	if (!lp->clk_base_addr) {
		dev_err(dev, "daqdrv: Could not allocate iomem for clk\n");
		rc = -EIO;
		goto error8;
	}

	init_waitqueue_head(&(lp->wait_queue_head));

	// allocate character device 
	int ret_alloc_chardev = alloc_chrdev_region(&dvt, 0, num_of_dev, DRIVER_NAME);
	if (ret_alloc_chardev) {
		dev_err(dev, "Allocating char device failed with %d\n", ret_alloc_chardev);
		rc = ret_alloc_chardev;
		goto error9;
	}

	// register character device
	major = MAJOR(dvt);
	cdev_init(&(lp->chardev), &chardev_fops);
	int ret_cdev_add = cdev_add(&(lp->chardev), dvt, num_of_dev);
	if (ret_cdev_add) {
		dev_err(dev, "Registering char device failed with %d\n", ret_cdev_add);
		rc = ret_cdev_add;
		goto error10;
	}

	// Create device file
	dev_info(dev, "I was assigned major number %d.\n", major);
	cls = class_create(DRIVER_NAME);
	device_create(cls, NULL, dvt, NULL, DRIVER_NAME);
	dev_info(dev, "Device created on /dev/%s\n", DRIVER_NAME);

	// allocate fifo
	int ret_fifo_alloc = kfifo_iomod_alloc(&(lp->fifo), FIFO_BUF_LEN*4, GFP_KERNEL);
	if (ret_fifo_alloc) {
		dev_err(dev, "Allocating fifo failed with %d\n", ret_fifo_alloc);
		rc = ret_fifo_alloc;
		goto error11;
	}

	// create sysfs files
	kobject_init(&(lp->sampleRate_module_object), &dynamic_kobj_ktype);
	int ret_kobject_add = kobject_add(&(lp->sampleRate_module_object), kernel_kobj, "%s", DRIVER_NAME);
	if (ret_kobject_add) {
		dev_err(dev, "kobject_add error: %d\n", ret_kobject_add);
		rc = ret_kobject_add;
		goto error12;
	}

	int ret_sysfs_create_file = sysfs_create_file(&(lp->sampleRate_module_object), &sampleRate_attribute.attr);
	if (ret_sysfs_create_file) {
		dev_err(dev, "Sysfs file creation failed with %d.\n", ret_sysfs_create_file);
		rc = ret_sysfs_create_file;
		goto error13;
	}

	// get interrupt
	int n_irq = platform_get_irq_optional(pdev, 0);
	if (n_irq < 0) {
		dev_info(dev, "no IRQ found\n");
		dev_info(dev, "daqdrv buffer at 0x%08x mapped to 0x%08x\n",
			(unsigned int __force)lp->buffer_mem_start,
			(unsigned int __force)lp->buffer_base_addr);
		dev_info(dev, "daqdrv ctrl at 0x%08x mapped to 0x%08x\n",
			(unsigned int __force)lp->ctrl_mem_start,
			(unsigned int __force)lp->ctrl_base_addr);
		dev_info(dev, "daqdrv stat at 0x%08x mapped to 0x%08x\n",
			(unsigned int __force)lp->stat_mem_start,
			(unsigned int __force)lp->stat_base_addr);
		dev_info(dev, "daqdrv clk at 0x%08x mapped to 0x%08x\n",
			(unsigned int __force)lp->clk_mem_start,
			(unsigned int __force)lp->clk_base_addr);
		return 0;
	}
	lp->irq = n_irq;

	// register interrupt
	rc = request_irq(lp->irq, &daqdrv_irq, 0, DRIVER_NAME, lp);
	if (rc) {
		dev_err(dev, "daqdrv: Could not allocate interrupt %d.\n",
			lp->irq);
		goto error14;
	}
	disable_irq(lp->irq);

	dev_info(dev,"daqdrv buffer at 0x%08x mapped to 0x%08x, irq=%d\n",
		(unsigned int __force)lp->buffer_mem_start,
		(unsigned int __force)lp->buffer_base_addr,
		lp->irq);
	dev_info(dev,"daqdrv ctrl at 0x%08x mapped to 0x%08x",
		(unsigned int __force)lp->ctrl_mem_start,
		(unsigned int __force)lp->ctrl_base_addr);
	dev_info(dev,"daqdrv stat at 0x%08x mapped to 0x%08x",
		(unsigned int __force)lp->stat_mem_start,
		(unsigned int __force)lp->stat_base_addr);
	dev_info(dev,"daqdrv clk at 0x%08x mapped to 0x%08x",
		(unsigned int __force)lp->clk_mem_start,
		(unsigned int __force)lp->clk_base_addr);
	return 0;
error14:
	free_irq(lp->irq, lp);
error13:
	kobject_put(&(lp->sampleRate_module_object));
error12:
	kfifo_iomod_free(&(lp->fifo));
error11:
	device_destroy(cls, dvt);
	class_destroy(cls);
	cdev_del(&(lp->chardev));
error10:
	unregister_chrdev_region(dvt, num_of_dev);
error9:
	iounmap(lp->clk_base_addr);
error8:
	iounmap(lp->stat_base_addr);
error7:
	iounmap(lp->ctrl_base_addr);
error6:
	iounmap(lp->buffer_base_addr);
error5:
	release_mem_region(lp->clk_mem_start, lp->clk_mem_end - lp->clk_mem_start + 1);
error4:
	release_mem_region(lp->stat_mem_start, lp->stat_mem_end - lp->stat_mem_start + 1);
error3:
	release_mem_region(lp->ctrl_mem_start, lp->ctrl_mem_end - lp->ctrl_mem_start + 1);
error2:
	release_mem_region(lp->buffer_mem_start, lp->buffer_mem_end - lp->buffer_mem_start + 1);
error1:
	kfree(lp);
	dev_set_drvdata(dev, NULL);
	return rc;
}

static int daqdrv_remove(struct platform_device *pdev)
{
	dev_t dvt = MKDEV(major, 0);
	struct device *dev = &pdev->dev;
	struct daqdrv_local *lp = dev_get_drvdata(dev);
	free_irq(lp->irq, lp);
	kobject_put(&(lp->sampleRate_module_object));
	kfifo_iomod_free(&(lp->fifo));

	device_destroy(cls, dvt);
	class_destroy(cls);
	cdev_del(&(lp->chardev));
	unregister_chrdev_region(dvt, num_of_dev);

	iounmap(lp->clk_base_addr);
	iounmap(lp->stat_base_addr);
	iounmap(lp->ctrl_base_addr);
	iounmap(lp->buffer_base_addr);
	release_mem_region(lp->clk_mem_start, lp->clk_mem_end - lp->clk_mem_start + 1);
	release_mem_region(lp->stat_mem_start, lp->stat_mem_end - lp->stat_mem_start + 1);
	release_mem_region(lp->ctrl_mem_start, lp->ctrl_mem_end - lp->ctrl_mem_start + 1);
	release_mem_region(lp->buffer_mem_start, lp->buffer_mem_end - lp->buffer_mem_start + 1);
	kfree(lp);
	dev_set_drvdata(dev, NULL);
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id daqdrv_of_match[] = {
	{ .compatible = "stasbucik,daqdrv", },
	{ /* end of list */ },
};
MODULE_DEVICE_TABLE(of, daqdrv_of_match);
#else
# define daqdrv_of_match
#endif


static struct platform_driver daqdrv_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table	= daqdrv_of_match,
	},
	.probe		= daqdrv_probe,
	.remove		= daqdrv_remove,
};

static int __init daqdrv_init(void)
{
	printk("<1>DAQ driver loaded.\n");
	return platform_driver_register(&daqdrv_driver);
}


static void __exit daqdrv_exit(void)
{
	platform_driver_unregister(&daqdrv_driver);
	printk(KERN_ALERT "DAQ driver exited.\n");
}

module_init(daqdrv_init);
module_exit(daqdrv_exit);
