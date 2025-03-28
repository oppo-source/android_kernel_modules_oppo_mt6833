// SPDX-License-Identifier: GPL-2.0-only
/******************************************************************************
 * Copyright (C) 2015, The Linux Foundation. All rights reserved.
 * Copyright (C) 2019-2021 NXP
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 ******************************************************************************/
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>

#include "common_ese.h"

int nfc_parse_dt(struct device *dev, struct platform_configs *nfc_configs,
		 uint8_t interface)
{
	struct device_node *np = dev->of_node;
	struct platform_gpio *nfc_gpio = &nfc_configs->gpio;

	if (!np) {
		pr_err("%s: nfc of_node NULL\n", __func__);
		return -EINVAL;
	}

	nfc_gpio->irq = -EINVAL;
	nfc_gpio->dwl_req = -EINVAL;
	nfc_gpio->ven = -EINVAL;
	nfc_gpio->clkreq = -EINVAL;

	/* irq required for i2c based chips only */
	if (interface == PLATFORM_IF_I2C) {
		nfc_gpio->irq = of_get_named_gpio(np, DTS_IRQ_GPIO_STR, 0);
		if ((!gpio_is_valid(nfc_gpio->irq))) {
			pr_err("%s: irq gpio invalid %d\n", __func__,
			       nfc_gpio->irq);
			return -EINVAL;
		}
		pr_info("%s: irq %d\n", __func__, nfc_gpio->irq);
	}
	nfc_gpio->ven = of_get_named_gpio(np, DTS_VEN_GPIO_STR, 0);
	if ((!gpio_is_valid(nfc_gpio->ven))) {
		pr_err("%s: ven gpio invalid %d\n", __func__, nfc_gpio->ven);
		return -EINVAL;
	}
	/* some products like sn220 does not required fw dwl pin */
	nfc_gpio->dwl_req = of_get_named_gpio(np, DTS_FWDN_GPIO_STR, 0);
	if ((!gpio_is_valid(nfc_gpio->dwl_req)))
		pr_warn("%s: dwl_req gpio invalid %d\n", __func__,
			nfc_gpio->dwl_req);

	nfc_gpio->clkreq = of_get_named_gpio(np, DTS_CLKREQ_GPIO_STR, 0);
	if ((!gpio_is_valid(nfc_gpio->clkreq)))
		pr_warn("%s: clkreq gpio invalid %d\n", __func__,
			nfc_gpio->clkreq);

	pr_info("%s: %d, %d, %d, %d\n", __func__, nfc_gpio->irq, nfc_gpio->ven,
		nfc_gpio->dwl_req, nfc_gpio->clkreq);
	return 0;
}

void set_valid_gpio(int gpio, int value)
{
	if (gpio_is_valid(gpio)) {
		pr_debug("%s: gpio %d value %d\n", __func__, gpio, value);
		gpio_set_value(gpio, value);
		/* hardware dependent delay */
		usleep_range(NFC_GPIO_SET_WAIT_TIME_US,
			     NFC_GPIO_SET_WAIT_TIME_US + 100);
	}
}

int get_valid_gpio(int gpio)
{
	int value = -EINVAL;

	if (gpio_is_valid(gpio)) {
		value = gpio_get_value(gpio);
		pr_debug("%s: gpio %d value %d\n", __func__, gpio, value);
	}
	return value;
}

void gpio_set_ven(struct nfc_dev *nfc_dev, int value)
{
	struct platform_gpio *nfc_gpio = &nfc_dev->configs.gpio;

	if (gpio_get_value(nfc_gpio->ven) != value) {
		pr_debug("%s: value %d\n", __func__, value);
		/* reset on change in level from high to low */
		if (value)
			ese_cold_reset_release(nfc_dev);

		gpio_set_value(nfc_gpio->ven, value);
		/* hardware dependent delay */
		usleep_range(NFC_GPIO_SET_WAIT_TIME_US,
			     NFC_GPIO_SET_WAIT_TIME_US + 100);
	}
}

int configure_gpio(unsigned int gpio, int flag)
{
	int ret;

	pr_debug("%s: nfc gpio [%d] flag [%01x]\n", __func__, gpio, flag);
	if (gpio_is_valid(gpio)) {
		ret = gpio_request(gpio, "nfc_gpio");
		if (ret) {
			pr_err("%s: unable to request nfc gpio [%d]\n",
			       __func__, gpio);
			return ret;
		}
		/* set direction and value for output pin */
		if (flag & GPIO_OUTPUT) {
			ret = gpio_direction_output(gpio, (GPIO_HIGH & flag));
			pr_debug("%s: nfc o/p gpio %d level %d\n", __func__,
				 gpio, gpio_get_value(gpio));
		} else {
			ret = gpio_direction_input(gpio);
			pr_debug("%s: nfc i/p gpio %d\n", __func__, gpio);
		}

		if (ret) {
			pr_err("%s: unable to set direction for nfc gpio [%d]\n",
			       __func__, gpio);
			gpio_free(gpio);
			return ret;
		}
		/* Consider value as control for input IRQ pin */
		if (flag & GPIO_IRQ) {
			ret = gpio_to_irq(gpio);
			if (ret < 0) {
				pr_err("%s: unable to set irq [%d]\n", __func__,
				       gpio);
				gpio_free(gpio);
				return ret;
			}
			pr_debug("%s: gpio_to_irq successful [%d]\n", __func__,
				 gpio);
			return ret;
		}
	} else {
		pr_err("%s: invalid gpio\n", __func__);
		ret = -EINVAL;
	}
	return ret;
}

void gpio_free_all(struct nfc_dev *nfc_dev)
{
	struct platform_gpio *nfc_gpio = &nfc_dev->configs.gpio;

	if (gpio_is_valid(nfc_gpio->dwl_req))
		gpio_free(nfc_gpio->dwl_req);

	if (gpio_is_valid(nfc_gpio->irq))
		gpio_free(nfc_gpio->irq);

	if (gpio_is_valid(nfc_gpio->ven))
		gpio_free(nfc_gpio->ven);
}

void nfc_misc_unregister(struct nfc_dev *nfc_dev, int count)
{
	pr_debug("%s: entry\n", __func__);
	device_destroy(nfc_dev->nfc_class, nfc_dev->devno);
	cdev_del(&nfc_dev->c_dev);
	class_destroy(nfc_dev->nfc_class);
	unregister_chrdev_region(nfc_dev->devno, count);
}

int nfc_misc_register(struct nfc_dev *nfc_dev,
		      const struct file_operations *nfc_fops, int count,
		      char *devname, char *classname)
{
	int ret = 0;

	ret = alloc_chrdev_region(&nfc_dev->devno, 0, count, devname);
	if (ret < 0) {
		pr_err("%s: failed to alloc chrdev region ret %d\n", __func__,
		       ret);
		return ret;
	}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,4,0))
	nfc_dev->nfc_class = class_create(classname);
#else
	nfc_dev->nfc_class = class_create(THIS_MODULE, classname);
#endif
	if (IS_ERR(nfc_dev->nfc_class)) {
		ret = PTR_ERR(nfc_dev->nfc_class);
		pr_err("%s: failed to register device class ret %d\n", __func__,
		       ret);
		unregister_chrdev_region(nfc_dev->devno, count);
		return ret;
	}
	cdev_init(&nfc_dev->c_dev, nfc_fops);
	ret = cdev_add(&nfc_dev->c_dev, nfc_dev->devno, count);
	if (ret < 0) {
		pr_err("%s: failed to add cdev ret %d\n", __func__, ret);
		class_destroy(nfc_dev->nfc_class);
		unregister_chrdev_region(nfc_dev->devno, count);
		return ret;
	}
	nfc_dev->nfc_device = device_create(nfc_dev->nfc_class, NULL,
					    nfc_dev->devno, nfc_dev, devname);
	if (IS_ERR(nfc_dev->nfc_device)) {
		ret = PTR_ERR(nfc_dev->nfc_device);
		pr_err("%s: failed to create the device ret %d\n", __func__,
		       ret);
		cdev_del(&nfc_dev->c_dev);
		class_destroy(nfc_dev->nfc_class);
		unregister_chrdev_region(nfc_dev->devno, count);
		return ret;
	}
	return 0;
}
/**
 * nfc_gpio_info() - gets the status of nfc gpio pins and encodes into a byte.
 * @nfc_dev:	nfc device data structure
 * @arg:		userspace buffer
 *
 * Encoding can be done in following manner
 * 1) map the gpio value into INVALID(-2), SET(1), RESET(0).
 * 2) mask the first 2 bits of gpio.
 * 3) left shift the 2 bits as multiple of 2.
 * 4) multiply factor can be defined as position of gpio pin in struct platform_gpio
 *
 * Return: -EFAULT, if unable to copy the data from kernel space to userspace, 0
 * if Success(or no issue)
 */

static int nfc_gpio_info(struct nfc_dev *nfc_dev, unsigned long arg)
{
	unsigned int gpios_status = 0;
	int value = 0;
	int gpio_no = 0;
	int i;
	int ret = 0;
	struct platform_gpio *nfc_gpio = &nfc_dev->configs.gpio;

	for (i = 0; i < sizeof(struct platform_gpio) / sizeof(unsigned int);
	     i++) {
		gpio_no = *((unsigned int *)nfc_gpio + i);
		value = get_valid_gpio(gpio_no);
		if (value < 0)
			value = -2;
		gpios_status |= (value & GPIO_STATUS_MASK_BITS)<<(GPIO_POS_SHIFT_VAL*i);
	}
	ret = copy_to_user((uint32_t *) arg, &gpios_status, sizeof(value));
	if (ret < 0) {
		pr_err("%s : Unable to copy data from kernel space to user space", __func__);
		return -EFAULT;
	}
	return 0;
}

/**
 * nfc_ioctl_power_states() - power control
 * @nfc_dev:    nfc device data structure
 * @arg:    mode that we want to move to
 *
 * Device power control. Depending on the arg value, device moves to
 * different states, refer common.h for args
 *
 * Return: -ENOIOCTLCMD if arg is not supported, 0 if Success(or no issue)
 * and error ret code otherwise
 */
static int nfc_ioctl_power_states(struct nfc_dev *nfc_dev, unsigned long arg)
{
	int ret = 0;
	struct platform_gpio *nfc_gpio = &nfc_dev->configs.gpio;

	if (arg == NFC_POWER_OFF) {
		/*
		 * We are attempting a hardware reset so let us disable
		 * interrupts to avoid spurious notifications to upper
		 * layers.
		 */
		nfc_dev->nfc_disable_intr(nfc_dev);
		set_valid_gpio(nfc_gpio->dwl_req, 0);
		gpio_set_ven(nfc_dev, 0);
		nfc_dev->nfc_ven_enabled = false;
		nfc_dev->nfc_state = NFC_STATE_NCI;
	} else if (arg == NFC_POWER_ON) {
		nfc_dev->nfc_enable_intr(nfc_dev);
		set_valid_gpio(nfc_gpio->dwl_req, 0);

		gpio_set_ven(nfc_dev, 1);
		nfc_dev->nfc_ven_enabled = true;
		nfc_dev->nfc_state = NFC_STATE_NCI;
	} else if (arg == NFC_FW_DWL_VEN_TOGGLE) {
		/*
		 * We are switching to download Mode, toggle the enable pin
		 * in order to set the NFCC in the new mode
		 */
		nfc_dev->nfc_disable_intr(nfc_dev);
		set_valid_gpio(nfc_gpio->dwl_req, 1);
		nfc_dev->nfc_state = NFC_STATE_FW_DWL;
		gpio_set_ven(nfc_dev, 0);
		gpio_set_ven(nfc_dev, 1);
		nfc_dev->nfc_enable_intr(nfc_dev);
	} else if (arg == NFC_FW_DWL_HIGH) {
		/*
		 * Setting firmware download gpio to HIGH
		 * before FW download start
		 */
		set_valid_gpio(nfc_gpio->dwl_req, 1);
		nfc_dev->nfc_state = NFC_STATE_FW_DWL;

	} else if (arg == NFC_VEN_FORCED_HARD_RESET) {
		nfc_dev->nfc_disable_intr(nfc_dev);
		gpio_set_ven(nfc_dev, 0);
		gpio_set_ven(nfc_dev, 1);
		nfc_dev->nfc_enable_intr(nfc_dev);
	} else if (arg == NFC_FW_DWL_LOW) {
		/*
		 * Setting firmware download gpio to LOW
		 * FW download finished
		 */
		set_valid_gpio(nfc_gpio->dwl_req, 0);
		nfc_dev->nfc_state = NFC_STATE_NCI;
	} else {
		pr_err("%s: bad arg %lu\n", __func__, arg);
		ret = -ENOIOCTLCMD;
	}
	return ret;
}

/*
 * Inside nfc_ioctl_nfcc_info
 *
 * @brief   nfc_ioctl_nfcc_info
 *
 * Check the NFC Chipset and firmware version details
 */
unsigned int nfc_ioctl_nfcc_info(struct file *filp, unsigned long arg)
{
        unsigned int r = 0;
        struct nfc_dev *nfc_dev = filp->private_data;
        pr_debug("%s:%d NFCC_GET_INFO enter \n", __func__, __LINE__);
        r = nfc_dev->nqx_info.i;
        pr_debug("%s:%d r = 0x%x\n", __func__, __LINE__, r);

        return r;
}

#ifdef CONFIG_COMPAT
/**
 * nfc_dev_compat_ioctl - used to set or get data from upper layer.
 * @pfile   file node for opened device.
 * @cmd     ioctl type from upper layer.
 * @arg     ioctl arg from upper layer.
 *
 * NFC and ESE Device power control, based on the argument value
 *
 * Return: -ENOIOCTLCMD if arg is not supported
 * 0 if Success(or no issue)
 * 0 or 1 in case of arg is ESE_GET_PWR/ESE_POWER_STATE
 * and error ret code otherwise
 */
long nfc_dev_compat_ioctl(struct file *pfile, unsigned int cmd,
		      unsigned long arg)
{
	int ret = 0;
	arg = (compat_u64)arg;
	pr_debug("%s: cmd = %x arg = %zx\n", __func__, cmd, arg);
	ret = nfc_dev_ioctl(pfile, cmd, arg);
	return ret;
}
#endif

/**
 * nfc_dev_ioctl - used to set or get data from upper layer.
 * @pfile   file node for opened device.
 * @cmd     ioctl type from upper layer.
 * @arg     ioctl arg from upper layer.
 *
 * NFC and ESE Device power control, based on the argument value
 *
 * Return: -ENOIOCTLCMD if arg is not supported
 * 0 if Success(or no issue)
 * 0 or 1 in case of arg is ESE_GET_PWR/ESE_POWER_STATE
 * and error ret code otherwise
 */
long nfc_dev_ioctl(struct file *pfile, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct nfc_dev *nfc_dev = pfile->private_data;

	if (!nfc_dev)
		return -ENODEV;

	pr_debug("%s: cmd = %x(dec: %d) arg = %zx\n", __func__, cmd, cmd, arg);
	switch (cmd) {
	case NFC_SET_PWR:
		ret = nfc_ioctl_power_states(nfc_dev, arg);
		break;
	case NFC_SET_RESET_READ_PENDING:
		if (arg == NFC_SET_READ_PENDING) {
			nfc_dev->cold_reset.is_nfc_read_pending = true;
			/* Set default NFC state as NCI for Nfc read pending request */
			nfc_dev->nfc_state = NFC_STATE_NCI;
		} else if (arg == NFC_RESET_READ_PENDING) {
			nfc_dev->cold_reset.is_nfc_read_pending = false;
		} else {
			ret = -EINVAL;
		}
		break;
	case ESE_SET_PWR:
		ret = nfc_ese_pwr(nfc_dev, arg);
		break;
	case ESE_GET_PWR:
		ret = nfc_ese_pwr(nfc_dev, ESE_POWER_STATE);
		break;
        case NFCC_GET_INFO:
                ret = nfc_ioctl_nfcc_info(pfile, arg);
                break;
	case NFC_GET_GPIO_STATUS:
		ret = nfc_gpio_info(nfc_dev, arg);
		break;
	default:
		pr_err("%s: bad cmd %lu\n", __func__, arg);
		ret = -ENOIOCTLCMD;
	};
	return ret;
}

int nfc_dev_open(struct inode *inode, struct file *filp)
{
	struct nfc_dev *nfc_dev = NULL;
	nfc_dev = container_of(inode->i_cdev, struct nfc_dev, c_dev);

	if (!nfc_dev)
		return -ENODEV;

	pr_debug("%s: %d, %d\n", __func__, imajor(inode), iminor(inode));

	mutex_lock(&nfc_dev->dev_ref_mutex);

	filp->private_data = nfc_dev;

	if (nfc_dev->dev_ref_count == 0) {
		set_valid_gpio(nfc_dev->configs.gpio.dwl_req, 0);

		nfc_dev->nfc_enable_intr(nfc_dev);
	}
	nfc_dev->dev_ref_count = nfc_dev->dev_ref_count + 1;
	mutex_unlock(&nfc_dev->dev_ref_mutex);
	return 0;
}

int nfc_dev_flush(struct file *pfile, fl_owner_t id)
{
	struct nfc_dev *nfc_dev = pfile->private_data;

	if (!nfc_dev)
		return -ENODEV;
	/*
	 * release blocked user thread waiting for pending read during close
	 */
	if (!mutex_trylock(&nfc_dev->read_mutex)) {
		nfc_dev->release_read = true;
		nfc_dev->nfc_disable_intr(nfc_dev);
		wake_up(&nfc_dev->read_wq);
		pr_debug("%s: waiting for release of blocked read\n", __func__);
		mutex_lock(&nfc_dev->read_mutex);
		nfc_dev->release_read = false;
	} else {
		pr_debug("%s: read thread already released\n", __func__);
	}
	mutex_unlock(&nfc_dev->read_mutex);
	return 0;
}

int nfc_dev_close(struct inode *inode, struct file *filp)
{
	struct nfc_dev *nfc_dev = NULL;
	nfc_dev = container_of(inode->i_cdev, struct nfc_dev, c_dev);

	if (!nfc_dev)
		return -ENODEV;

	pr_debug("%s: %d, %d\n", __func__, imajor(inode), iminor(inode));
	mutex_lock(&nfc_dev->dev_ref_mutex);
	if (nfc_dev->dev_ref_count == 1) {
		nfc_dev->nfc_disable_intr(nfc_dev);
		set_valid_gpio(nfc_dev->configs.gpio.dwl_req, 0);
		/*
		 * Use "ESE_RST_PROT_DIS" as argument
		 * if eSE calls flow is via NFC driver
		 * i.e. direct calls from SPI HAL to NFC driver
		 */
		mutex_unlock(&nfc_dev->dev_ref_mutex);
		nfc_ese_pwr(nfc_dev, ESE_RST_PROT_DIS_NFC);
		mutex_lock(&nfc_dev->dev_ref_mutex);
	}
	if (nfc_dev->dev_ref_count > 0)
		nfc_dev->dev_ref_count = nfc_dev->dev_ref_count - 1;

	filp->private_data = NULL;

	mutex_unlock(&nfc_dev->dev_ref_mutex);
	return 0;
}

int validate_nfc_state_nci(struct nfc_dev *nfc_dev)
{
	struct platform_gpio *nfc_gpio = &nfc_dev->configs.gpio;

	if (!gpio_get_value(nfc_gpio->ven)) {
		pr_err("%s: ven low - nfcc powered off\n", __func__);
		return -ENODEV;
	}
	if (get_valid_gpio(nfc_gpio->dwl_req) == 1) {
		pr_err("%s: fw download in-progress\n", __func__);
		return -EBUSY;
	}
	if (nfc_dev->nfc_state != NFC_STATE_NCI) {
		pr_err("%s: fw download state\n", __func__);
		return -EBUSY;
	}
	return 0;
}

/**
 * get_nfcc_chip_type_dl() - get chip type in fw download command;
 * @nfc_dev:    nfc device data structure
 *
 * Perform get version command and determine chip
 * type from response.
 *
 * @Return:  enum chip_types value
 *
 */
static enum chip_types get_nfcc_chip_type_dl(struct nfc_dev *nfc_dev)
{
	int ret = 0;
	uint8_t *cmd = nfc_dev->write_kbuf;
	uint8_t *rsp = nfc_dev->read_kbuf;
	enum chip_types chip_type = CHIP_UNKNOWN;

	*cmd++ = DL_CMD;
	*cmd++ = DL_GET_VERSION_CMD_PAYLOAD_LEN;
	*cmd++ = DL_GET_VERSION_CMD_ID;
	*cmd++ = DL_PAYLOAD_BYTE_ZERO;
	*cmd++ = DL_PAYLOAD_BYTE_ZERO;
	*cmd++ = DL_PAYLOAD_BYTE_ZERO;
	*cmd++ = DL_GET_VERSION_CMD_CRC_1;
	*cmd++ = DL_GET_VERSION_CMD_CRC_2;

	pr_err("NxpDrv: %s:Sending GET_VERSION cmd of size = %d\n", __func__, DL_GET_VERSION_CMD_LEN);
	ret = nfc_dev->nfc_write(nfc_dev, nfc_dev->write_kbuf, DL_GET_VERSION_CMD_LEN,
									MAX_RETRY_COUNT);
	if (ret <= 0) {
		pr_err("NxpDrv: %s: - nfc get version cmd error ret %d\n", __func__, ret);
		goto err;
	}
	memset(rsp, 0x00, DL_GET_VERSION_RSP_LEN_2);
	pr_err("NxpDrv: %s:Reading response of GET_VERSION cmd\n", __func__);
	ret = nfc_dev->nfc_read(nfc_dev, rsp, DL_GET_VERSION_RSP_LEN_2, NCI_CMD_RSP_TIMEOUT_MS);
	if (ret <= 0) {
		pr_err("NxpDrv: %s: - nfc get version rsp error ret %d\n", __func__, ret);
		goto err;
	}
	if (rsp[0] == FW_MSG_CMD_RSP && ret >= DL_GET_VERSION_RSP_LEN_2) {

		nfc_dev->fw_major_version = rsp[FW_MAJOR_VER_OFFSET];

		if (rsp[FW_ROM_CODE_VER_OFFSET] == SN1XX_ROM_VER &&
			rsp[FW_MAJOR_VER_OFFSET] == SN1xx_MAJOR_VER)
			chip_type = CHIP_SN1XX;
		else if (rsp[FW_ROM_CODE_VER_OFFSET] == SN220_ROM_VER &&
			rsp[FW_MAJOR_VER_OFFSET] == SN220_MAJOR_VER)
			chip_type = CHIP_SN220;

		pr_err("NxpDrv: %s:NFC Chip Type 0x%02x Rom Version 0x%02x FW Minor 0x%02x Major 0x%02x\n",
			__func__, rsp[GET_VERSION_RSP_CHIP_TYPE_OFFSET],
					rsp[FW_ROM_CODE_VER_OFFSET],
					rsp[GET_VERSION_RSP_MINOR_VERSION_OFFSET],
					rsp[FW_MAJOR_VER_OFFSET]);

		nfc_dev->nqx_info.info.chip_type = rsp[GET_VERSION_RSP_CHIP_TYPE_OFFSET];
		nfc_dev->nqx_info.info.rom_version = rsp[FW_ROM_CODE_VER_OFFSET];
		nfc_dev->nqx_info.info.fw_minor = rsp[GET_VERSION_RSP_MINOR_VERSION_OFFSET];
		nfc_dev->nqx_info.info.fw_major = rsp[FW_MAJOR_VER_OFFSET];
	}
err:
	return chip_type;
}

static enum chip_types get_nfcc_chip_type(struct nfc_dev *nfc_dev)
{
	int ret = 0;
	uint8_t major_version = 0;
	uint8_t rom_version = 0;
	uint8_t *cmd = nfc_dev->write_kbuf;
	uint8_t *rsp = nfc_dev->read_kbuf;
	enum chip_types chip_type = CHIP_UNKNOWN;

	*cmd++ = NCI_CMD;
	*cmd++ = NCI_CORE_RESET_CMD_OID;
	*cmd++ = NCI_CORE_RESET_CMD_PAYLOAD_LEN;
	*cmd++ = NCI_CORE_RESET_KEEP_CONFIG;

	pr_err("NxpDrv: %s:Sending NCI Core Reset cmd of size = %d\n", __func__, NCI_RESET_CMD_LEN);
	ret = nfc_dev->nfc_write(nfc_dev, nfc_dev->write_kbuf, NCI_RESET_CMD_LEN, NO_RETRY);
	if (ret <= 0) {
		pr_err("NxpDrv: %s: - nfc nci core reset cmd error ret %d\n", __func__, ret);
		goto err;
	}

	/* to flush out debug NTF this delay is required */
	usleep_range(NCI_RESET_RESP_READ_DELAY, NCI_RESET_RESP_READ_DELAY + 100);
	nfc_dev->nfc_enable_intr(nfc_dev);

	memset(rsp, 0x00, NCI_RESET_RSP_LEN);
	pr_err("NxpDrv: %s:Reading NCI Core Reset rsp\n", __func__);
	ret = nfc_dev->nfc_read(nfc_dev, rsp, NCI_RESET_RSP_LEN, NCI_CMD_RSP_TIMEOUT_MS);
	if (ret <= 0) {
		pr_err("NxpDrv: %s: - nfc nci core reset rsp error ret %d\n", __func__, ret);
		goto err_disable_intr;
	}

	pr_err("NxpDrv: %s: nci core reset response 0x%02x%02x%02x%02x\n",
		__func__, rsp[0], rsp[1], rsp[2], rsp[3]);
	if (rsp[0] != NCI_RSP) {
		/* reset response failed response*/
		pr_err("NxpDrv: %s invalid nci core reset response\n", __func__);
		goto err_disable_intr;
	}

	memset(rsp, 0x00, NCI_RESET_NTF_LEN);
	/* read nci rest response ntf */
	ret = nfc_dev->nfc_read(nfc_dev, rsp, NCI_RESET_NTF_LEN, NCI_CMD_RSP_TIMEOUT_MS);
	if (ret <= 0) {
		pr_err("NxpDrv: %s - nfc nci rest rsp ntf error status %d\n", __func__, ret);
		goto err_disable_intr;
	}

	if (rsp[0] == NCI_NTF) {
		/* read version info from NCI Reset Notification */
		rom_version = rsp[NCI_HDR_LEN + rsp[NCI_PAYLOAD_LEN_IDX] - 3];
		major_version = rsp[NCI_HDR_LEN + rsp[NCI_PAYLOAD_LEN_IDX] - 2];
		/* determine chip type based on version info */
		if (rom_version == SN1XX_ROM_VER && major_version == SN1xx_MAJOR_VER)
			chip_type = CHIP_SN1XX;
		else if (rom_version == SN220_ROM_VER && major_version == SN220_MAJOR_VER)
			chip_type = CHIP_SN220;
		pr_err("NxpDrv: %s:NCI  Core Reset ntf 0x%02x%02x%02x%02x\n",
			__func__, rsp[0], rsp[1], rsp[2], rsp[3]);

		nfc_dev->nqx_info.info.chip_type = rsp[NCI_HDR_LEN + rsp[NCI_PAYLOAD_LEN_IDX] -
									NFC_CHIP_TYPE_OFF];
		nfc_dev->nqx_info.info.rom_version = rom_version;
		nfc_dev->nqx_info.info.fw_major = major_version;
		nfc_dev->nqx_info.info.fw_minor = rsp[NCI_HDR_LEN + rsp[NCI_PAYLOAD_LEN_IDX] -
									NFC_FW_MINOR_OFF];
	}
err_disable_intr:
	nfc_dev->nfc_disable_intr(nfc_dev);
err:
	return chip_type;
}


/**
 * validate_download_gpio() - validate download gpio.
 * @nfc_dev: nfc_dev device data structure.
 * @chip_type: chip type of the platform.
 *
 * Validates dwnld gpio should configured for supported and
 * should not be configured for unsupported platform.
 *
 * @Return:  true if gpio validation successful ortherwise
 *           false if validation fails.
 */
static bool validate_download_gpio(struct nfc_dev *nfc_dev, enum chip_types chip_type)
{
	bool status = false;
	struct platform_gpio *nfc_gpio;

	if (nfc_dev == NULL) {
		pr_err("NxpDrv: %s nfc devices structure is null\n", __func__);
		return status;
	}
	nfc_gpio = &nfc_dev->configs.gpio;
	if (chip_type == CHIP_SN1XX) {
		/* gpio should be configured for SN1xx */
		status = gpio_is_valid(nfc_gpio->dwl_req);
	} else if (chip_type == CHIP_SN220) {
		/* gpio should not be configured for SN220 */
		set_valid_gpio(nfc_gpio->dwl_req, 0);
		gpio_free(nfc_gpio->dwl_req);
		nfc_gpio->dwl_req = -EINVAL;
		status = true;
	}
	return status;
}


int nfcc_hw_check(struct nfc_dev *nfc_dev)
{
        int ret = 0;
		enum chip_types chip_type = CHIP_UNKNOWN;
		struct platform_gpio *nfc_gpio = &nfc_dev->configs.gpio;

		/*get fw version in nci mode*/
		usleep_range(NFC_GPIO_SET_WAIT_TIME_US, NFC_GPIO_SET_WAIT_TIME_US + 100);//add for Satisfy VEN spec of	15ms delay
		gpio_set_ven(nfc_dev, 1);
		gpio_set_ven(nfc_dev, 0);
		gpio_set_ven(nfc_dev, 1);
		chip_type = get_nfcc_chip_type(nfc_dev);

	   /*get fw version in fw dwl mode*/
	   if (chip_type == CHIP_UNKNOWN) {
		   nfc_dev->nfc_enable_intr(nfc_dev);
		   /*Chip is unknown, initially assume with fw dwl pin enabled*/
		   set_valid_gpio(nfc_gpio->dwl_req, 1);
		   gpio_set_ven(nfc_dev, 0);
		   gpio_set_ven(nfc_dev, 1);
		   chip_type = get_nfcc_chip_type_dl(nfc_dev);
		   /*get the state of nfcc normal/teared in fw dwl mode*/
	   }

	   /*validate gpio config required as per the chip*/
	   if (!validate_download_gpio(nfc_dev, chip_type)) {
		   pr_err("NxpDrv: %s gpio validation fail\n", __func__);
		   ret = -ENXIO;
		   goto err;
	   }

       err:
		nfc_dev->nfc_disable_intr(nfc_dev);
		set_valid_gpio(nfc_gpio->dwl_req, 0);
		gpio_set_ven(nfc_dev, 0);
		gpio_set_ven(nfc_dev, 1);
		nfc_dev->nfc_ven_enabled = true;
        return ret;
}

