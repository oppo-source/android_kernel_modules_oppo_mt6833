/**************************************************************
* Copyright (C), 2008-2024, OPPO Mobile Comm Corp., Ltd.
* VENDOR_EDIT
* File: ois_dw9786.c
* Description: OIS Function Implement
* Version: 1.0
* Date : 2024/02/28
* Author: Wang Jianwei
* ----------------------Revision History-----------------------
*   <author>       <data>      <version>     <desc>
* Wang Jianwei   2024/02/28       1.0     OIS Function Implement
****************************************************************/

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <mtk_boot_common.h>

#include "hf_manager.h"
#include "hf_sensor_io.h"
#include "../ois_def.h"
#include "dw9786_if.h"
#include <linux/timekeeping.h>
#include <linux/spinlock_types.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <semaphore.h>
#include <linux/atomic.h>

#define DW9786_NAME "ois_dw9786"

#define GYRO_REG_MONITOR
#define DW9786_EIS_DATA_GROUP_SIZE (1)
#define DW9786_EIS_DATA_PACKET_SIZE (44)

#define INPUT_MIN (4000)
#define INPUT_MAX (28000)
#define OUTPUT_MIN (-8192)
#define OUTPUT_MAX (8191)
#define GYRO_GAIN_X_GOLDEN 0x1BFE
#define GYRO_GAIN_Y_GOLDEN 0x2212

struct dw9786_device *dw9786;
extern struct mutex dw9786_idle_mutex;

static struct sensor_info support_sensors[] = {
	{
		.sensor_type = SENSOR_TYPE_OIS2,
		.gain = 1,
		.name = "ultra_tele_ois",
		.vendor = "oplus"
	},
};

static int g_gyro_offset[] = {0xdc, 0x18};
static unsigned short g_gyro_gain_x = 0;
static unsigned short g_gyro_gain_y = 0;
static int g_still_en = 0;
static int dbg = 0;
static int g_gyro_cal = 0;
static struct hf_client *gyro_client = NULL;
static int buffer_dump = 0;
static int64_t g_last_time = 0;
static uint16_t gyro_gain_x = 0;
static uint16_t gyro_gain_y = 0;
static bool sleep_mode_state = false;
extern struct mutex dw9786_mutex;
static struct semaphore dw9786_power_on_sem;
static atomic_t dw9786_power_on_sem_once = ATOMIC_INIT(0);
static atomic_t dw9786_shake_state = ATOMIC_INIT(0);
bool is_do_cali = false;

#define OIS_DATA_NUMBER 32
struct ois_info {
	int32_t is_supported;
	int32_t mode; /* ON/OFF */
	int32_t samples;
	int32_t x_shifts[OIS_DATA_NUMBER];
	int32_t y_shifts[OIS_DATA_NUMBER];
	int64_t timestamps[OIS_DATA_NUMBER];
};

struct mtk_ois_pos_info {
	struct ois_info *p_ois_info;
};

/* OIS control interface of MTK, not actually used */
#define VIDIOC_MTK_S_OIS_MODE _IOW('V', BASE_VIDIOC_PRIVATE + 2, int32_t)
#define VIDIOC_MTK_G_OIS_POS_INFO _IOWR('V', BASE_VIDIOC_PRIVATE + 3, struct mtk_ois_pos_info)

static inline struct dw9786_device *to_dw9786_ois(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct dw9786_device, ctrls);
}

static inline struct dw9786_device *subdev_to_dw9786_ois(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct dw9786_device, subdev);
}

static int dw9786_release(struct dw9786_device *dw9786)
{
	return 0;
}

static struct i2c_ops_info ops_inf = {"dw9786", 0, 0};

#define SHOW(buf, len, fmt, ...) { \
	len += snprintf(buf + len, PAGE_SIZE - len, fmt, ##__VA_ARGS__); \
}

static ssize_t dw9786_i2c_ops_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;
	SHOW(buf, len, "%s i2c read 0x%08x = 0x%08x\n", ops_inf.name, ops_inf.RegAddr, ops_inf.RegData);

	return len;
}


static ssize_t dw9786_i2c_ops_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	char delim[] = " ";
	char *token = NULL;
	char *sbuf = kzalloc(sizeof(char) * (count + 1), GFP_KERNEL);
	char *s = sbuf;
	int ret;
	unsigned int num_para = 0;
	char *arg[DBG_ARG_IDX_MAX_NUM];
	u32 val;
	u32 reg;
	unsigned short tmp_val = 0;
	struct i2c_client *client = i2c_verify_client(dev);
	if (!client) {
		LOG_ERR("client is null!");
	}

	ops_inf.RegAddr = 0;
	ops_inf.RegData = 0;

	if (!sbuf) {
		goto ERR_DEBUG_OPS_STORE;
	}

	memcpy(sbuf, buf, count);

	token = strsep(&s, delim);
	while (token != NULL && num_para < DBG_ARG_IDX_MAX_NUM) {
		if (strlen(token)) {
			arg[num_para] = token;
			num_para++;
		}

		token = strsep(&s, delim);
	}

	if (num_para > DBG_ARG_IDX_MAX_NUM) {
		LOG_ERR("Wrong command parameter number %u\n", num_para);
		goto ERR_DEBUG_OPS_STORE;
	}
	ret = kstrtouint(arg[DBG_ARG_IDX_I2C_ADDR], 0, &reg);
	if (ret) {
		goto ERR_DEBUG_OPS_STORE;
	}
	ops_inf.RegAddr = reg;

	if (num_para == DBG_ARG_IDX_MAX_NUM) {
		ret = kstrtouint(arg[DBG_ARG_IDX_I2C_DATA], 0, &val);
		if (ret) {
			goto ERR_DEBUG_OPS_STORE;
		}
		ops_inf.RegData = val;

		ret = I2C_WRITE_16BIT_OIS(ops_inf.RegAddr, (unsigned short)ops_inf.RegData);
		LOG_INF("i2c write 0x%08x = 0x%08x ret = %d\n", ops_inf.RegAddr, ops_inf.RegData, ret);
	}

	ret = I2C_READ_16BIT_OIS(ops_inf.RegAddr, &tmp_val);
	ops_inf.RegData = (unsigned int)tmp_val;
	LOG_INF("i2c read 0x%08x = 0x%08x  ret = %d\n", ops_inf.RegAddr, ops_inf.RegData, ret);

ERR_DEBUG_OPS_STORE:
	kfree(sbuf);
	LOG_ERR("exit\n");

	return count;
}
static DEVICE_ATTR_RW(dw9786_i2c_ops);

static ssize_t dw9786_i2c_ops32_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	int len = 0;
	SHOW(buf, len, "%s i2c read 0x%08x = 0x%08x\n", ops_inf.name, ops_inf.RegAddr, ops_inf.RegData);

	return len;
}


static ssize_t dw9786_i2c_ops32_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	char delim[] = " ";
	char *token = NULL;
	char *sbuf = kzalloc(sizeof(char) * (count + 1), GFP_KERNEL);
	char *s = sbuf;
	int ret;
	unsigned int num_para = 0;
	char *arg[DBG_ARG_IDX_MAX_NUM];
	u32 val;
	u32 reg;
	struct i2c_client *client = i2c_verify_client(dev);
	if (!client) {
		LOG_ERR("client is null!");
	}

	ops_inf.RegAddr = 0;
	ops_inf.RegData = 0;

	if (!sbuf) {
		goto ERR_DEBUG_OPS_STORE;
	}

	memcpy(sbuf, buf, count);

	token = strsep(&s, delim);
	while (token != NULL && num_para < DBG_ARG_IDX_MAX_NUM) {
		if (strlen(token)) {
			arg[num_para] = token;
			num_para++;
		}

		token = strsep(&s, delim);
	}

	if (num_para > DBG_ARG_IDX_MAX_NUM) {
		LOG_ERR("Wrong command parameter number %u\n", num_para);
		goto ERR_DEBUG_OPS_STORE;
	}
	ret = kstrtouint(arg[DBG_ARG_IDX_I2C_ADDR], 0, &reg);
	if (ret) {
		goto ERR_DEBUG_OPS_STORE;
	}
	ops_inf.RegAddr = reg;

	if (num_para == DBG_ARG_IDX_MAX_NUM) {
		ret = kstrtouint(arg[DBG_ARG_IDX_I2C_DATA], 0, &val);
		if (ret) {
			goto ERR_DEBUG_OPS_STORE;
		}
		ops_inf.RegData = val;

		ret = I2C_WRITE_32BIT_OIS(ops_inf.RegAddr, ops_inf.RegData);
		LOG_INF("i2c write 0x%08x = 0x%08x ret = %d\n", ops_inf.RegAddr, ops_inf.RegData, ret);
	}

	ret = I2C_READ_32BIT_OIS(ops_inf.RegAddr, &(ops_inf.RegData));
	LOG_INF("i2c read 0x%08x = 0x%08x  ret = %d\n", ops_inf.RegAddr, ops_inf.RegData, ret);


ERR_DEBUG_OPS_STORE:
	kfree(sbuf);
	LOG_ERR("exit\n");

	return count;
}
static DEVICE_ATTR_RW(dw9786_i2c_ops32);

static ssize_t dw9786_dbg_show(struct device *dev, struct device_attribute *attr, char *buf) {
	return scnprintf(buf, PAGE_SIZE, "%d\n", dbg);
}

static ssize_t dw9786_dbg_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	unsigned long data;
	int ret;
	ret = kstrtoul(buf, 10, &data);
	if (ret) {
		LOG_ERR("kstrtoul failed %d", ret);
		return count;
	}
	dbg = data & 0x1;
	buffer_dump = data & 0x2;
	return count;
}
static DEVICE_ATTR_RW(dw9786_dbg);

static int dw9786_set_ctrl(struct v4l2_ctrl *ctrl)
{
	/* struct dw9786_device *dw9786 = to_dw9786_ois(ctrl); */
	return 0;
}

static const struct v4l2_ctrl_ops dw9786_ois_ctrl_ops = {
	.s_ctrl = dw9786_set_ctrl,
};

static int dw9786_open(struct v4l2_subdev *subdev, struct v4l2_subdev_fh *fh)
{
	int ret;

	LOG_DBG(dbg, "%s\n", __func__);

	ret = pm_runtime_get_sync(subdev->dev);
	if (ret < 0) {
		pm_runtime_put_noidle(subdev->dev);
		return ret;
	}

	return 0;
}

static int dw9786_close(struct v4l2_subdev *subdev, struct v4l2_subdev_fh *fh)
{
	LOG_DBG(dbg, "%s\n", __func__);

	pm_runtime_put(subdev->dev);

	return 0;
}

static long dw9786_ops_core_ioctl(struct v4l2_subdev *subdev, unsigned int cmd, void *arg)
{
	int ret = 0;

	switch (cmd) {
	case VIDIOC_MTK_S_OIS_MODE: {
		int *ois_mode = arg;

		if (*ois_mode)
			LOG_DBG(dbg, "VIDIOC_MTK_S_OIS_MODE Enable\n");
		else
			LOG_DBG(dbg, "VIDIOC_MTK_S_OIS_MODE Disable\n");
	} break;

	case VIDIOC_MTK_G_OIS_POS_INFO: {
		struct mtk_ois_pos_info *info = arg;
		struct ois_info pos_info;
		int i = 0;

		memset(&pos_info, 0, sizeof(struct ois_info));

		/* To Do */
		pos_info.mode = 1;

		pos_info.samples = OIS_DATA_NUMBER;
		pos_info.is_supported = 1;
		for (i = 0; i < OIS_DATA_NUMBER; i++) {
			pos_info.x_shifts[i] = 0xab + i;
			pos_info.y_shifts[i] = 0xab + i;
			pos_info.timestamps[i] = 123 + i;
		}

		if (copy_to_user((void *)info->p_ois_info, &pos_info,
						 sizeof(pos_info)))
			ret = -EFAULT;
	} break;

	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

#ifdef GYRO_REG_MONITOR

static struct task_struct *reg_monitor_task;

static int reg_monitor_kthread(void *arg) {
	struct i2c_client *client = (struct i2c_client *)arg;
	int16_t gyro_data[2];
	int16_t acc_data[2];
	int16_t gyro_offset[2];
	uint16_t gyro_gain[2];
	int16_t hall[2];
	int16_t target[2];
	uint16_t stable_gain[2];
	int16_t gyro_filter_out[2];
	uint16_t tripod_mode;
	struct v4l2_subdev *subdev;
	struct dw9786_device *dw9786;

	subdev = i2c_get_clientdata(client);
	if (subdev == NULL) {
		LOG_ERR("subdev is NULL");
		return -1;
	}
	dw9786 = subdev_to_dw9786_ois(subdev);
	if (dw9786 == NULL) {
		LOG_ERR("dw9786 is NULL");
		return -1;
	}

	while(!kthread_should_stop()) {
		if (dbg) {
			I2C_READ_16BIT_OIS(DW9786_GYRO_RAW_X_ADDR, &gyro_data[0]);
			I2C_READ_16BIT_OIS(DW9786_GYRO_RAW_Y_ADDR, &gyro_data[1]);
			I2C_READ_16BIT_OIS(0xB1E0, &acc_data[0]);
			I2C_READ_16BIT_OIS(0xB2E0, &acc_data[1]);

			if (g_still_en) {
				I2C_READ_16BIT_OIS(0xB80C, &gyro_offset[0]);
				I2C_READ_16BIT_OIS(0xB80E, &gyro_offset[1]);

				I2C_READ_16BIT_OIS(0xB806, &gyro_gain[0]);
				I2C_READ_16BIT_OIS(0xB808, &gyro_gain[1]);

				I2C_READ_16BIT_OIS(0xB102, &hall[0]);
				I2C_READ_16BIT_OIS(0xB202, &hall[1]);

				I2C_READ_16BIT_OIS(0xB1B8, &target[0]);
				I2C_READ_16BIT_OIS(0xB2B8, &target[1]);

				I2C_READ_16BIT_OIS(0xB082, &tripod_mode);

				I2C_READ_16BIT_OIS(0xB1C8, &stable_gain[0]);
				I2C_READ_16BIT_OIS(0xB2C8, &stable_gain[1]);

				I2C_READ_16BIT_OIS(0xB1B0, &gyro_filter_out[0]);
				I2C_READ_16BIT_OIS(0xB2B0, &gyro_filter_out[1]);
				LOG_DBG(dbg, "gyro data: %d %d acc data: %d %d gyro offset: %d %d gyro gain: %d %d "
						"hall: %d %d target: %d %d tripod mode: %d stable gain: %d %d gyro filter out: %d %d",
						gyro_data[0], gyro_data[1], acc_data[0], acc_data[1], gyro_offset[0], gyro_offset[1],
						gyro_gain[0], gyro_gain[1], hall[0], hall[1], target[0], target[1], tripod_mode,
						stable_gain[0], stable_gain[1], gyro_filter_out[0], gyro_filter_out[1]);
			} else {
				LOG_DBG(dbg, "gyro data: %d %d acc data: %d %d", gyro_data[0], gyro_data[1], acc_data[0], acc_data[1]);
			}
		}
		msleep(30);
	}
	LOG_ERR("reg_monitor_kthread return...");
	return 0;
}

static int reg_monitor_thread_init(struct i2c_client *client)
{
	int err;
	LOG_ERR("Kernel thread initalizing...\n");
	reg_monitor_task = kthread_create(reg_monitor_kthread, client, "reg_monitor_kthread");
	if (IS_ERR(reg_monitor_task)) {
		LOG_ERR("Unable to start kernel thread./n");
		err = PTR_ERR(reg_monitor_task);
		reg_monitor_task = NULL;
		return err;
	}
	wake_up_process(reg_monitor_task);
	return 0;
}
static void __maybe_unused reg_monitor_thread_exit(void)
{
	if (reg_monitor_task) {
		LOG_INF("exit reg_monitor_thread\n");
		kthread_stop(reg_monitor_task);
		LOG_INF("exit\n");
	}
}

void big_to_little_endian(uint16_t *data)
{
	unsigned char table[2] = {0};
	table[0] = (*data >> 8) & 0xff;
	table[1] = *data & 0xff;
	*data = table[1] << 8 | table[0];
}

static int dw9786_init(void *data)
{
	struct dw9786_device* dw9786 = (struct dw9786_device*)data;
	struct i2c_client *client = v4l2_get_subdevdata(&dw9786->subdev);
	int ret = 0;
	uint16_t status;

	LOG_ERR("E");
	LOG_INF("dw9786_mutex: %p", &dw9786_mutex);
	mutex_lock(&dw9786_mutex);
	client->addr = DW9786_OIS_SLAVE_ADDR;
	LOG_DBG(dbg, "%s i2c_client %p addr 0x%x", __func__, client, client->addr);

	dw9786_device_reset();

	if (dw9786->need_ois_on) {
		dw9786_ois_on();
	}
	I2C_READ_16BIT_OIS(DW9786_STATUS_ADDR, &status);
	LOG_INF("0xB020: 0x%04X\n", status);

	ret =  I2C_READ_16BIT_OIS(DW9786_GYRO_GAIN_X, &gyro_gain_x);
	if (ret < 0) {
		LOG_ERR("read gyro gain X failed ret[%d]", ret);
		gyro_gain_x = GYRO_GAIN_X_GOLDEN;
	}
	ret =  I2C_READ_16BIT_OIS(DW9786_GYRO_GAIN_Y, &gyro_gain_y);
	if (ret < 0) {
		LOG_ERR("read gyro gain Y failed ret[%d]", ret);
		gyro_gain_x = GYRO_GAIN_Y_GOLDEN;
	}
	LOG_ERR("gyro gain [0x%04X, 0x%04X]", gyro_gain_x, gyro_gain_y);
	sleep_mode_state = false;
	g_still_en = 0;
	LOG_ERR("X");
	mutex_unlock(&dw9786_mutex);
	if (atomic_inc_return(&dw9786_power_on_sem_once) == 1) {
		up(&dw9786_power_on_sem);
	}

	return 0;
}

static int dw9786_power_on(struct dw9786_device *dw9786, bool need_ois_on)
{
	int ret = 0;
	int ret1 = 0;
	int err;
	struct task_struct *dw9786_init_task;

	LOG_ERR("E\n");

	if (dw9786->vin == NULL || dw9786->iovdd == NULL || dw9786->afvdd == NULL) {
		LOG_ERR("regulator is NULL");
		return -1;
	}

	ret = regulator_enable(dw9786->iovdd);
	ret1 = regulator_set_voltage(dw9786->iovdd, 1800000, 1800000);
	if (ret < 0 || ret1 < 0) {
		LOG_ERR("%s dw9786->iovdd set failed ret[%d] ret1[%d]", __func__,
			   ret, ret1);
		return -1;
	}
	LOG_INF("dw9786->iovdd set success ret[%d], ret1[%d]", ret, ret1);

	ret = regulator_enable(dw9786->vin);
	if (ret < 0 || ret1 < 0) {
		LOG_ERR("%s dw9786->vin set failed ret[%d] ret1[%d]", __func__,
			   ret, ret1);
		return -1;
	}
	LOG_INF("dw9786->vin set success ret[%d], ret1[%d]", ret, ret1);

	ret = regulator_enable(dw9786->oisvdd);
	ret1 = regulator_set_voltage(dw9786->oisvdd, 3100000, 3100000);
	if (ret < 0 || ret1 < 0) {
		LOG_ERR("%s dw9786->oisvdd set failed ret[%d] ret1[%d]", __func__,
			   ret, ret1);
		return -1;
	}
	LOG_INF("dw9786->oisvdd set success ret[%d], ret1[%d]", ret, ret1);
	mdelay(2);

	ret = regulator_enable(dw9786->afvdd);
	ret1 = regulator_set_voltage(dw9786->afvdd, 3100000, 3100000);
	if (ret < 0 || ret1 < 0) {
		LOG_ERR("%s dw9786->afvdd set failed ret[%d] ret1[%d]", __func__,
			   ret, ret1);
		return -1;
	}
	LOG_INF("dw9786->afvdd set success ret[%d], ret1[%d]", ret, ret1);

	udelay(5000);

	dw9786->need_ois_on = need_ois_on;
	dw9786_init_task = kthread_create(dw9786_init, dw9786, "dw9786_init_kthread");
	if (IS_ERR(dw9786_init_task)) {
		LOG_INF("unable to start dw9786_init thread/n");
		err = PTR_ERR(dw9786_init_task);
		dw9786_init_task = NULL;
		dw9786_init(dw9786);
		return 0;
	}
	wake_up_process(dw9786_init_task);

	LOG_ERR("X\n");
	return 0;
}

static int dw9786_power_off(struct dw9786_device *dw9786)
{
	int ret;
	uint16_t status;

	LOG_ERR("%s E\n", __func__);
	g_gyro_cal = 0;

	if (dw9786->vin == NULL || dw9786->iovdd == NULL || dw9786->afvdd == NULL) {
		LOG_ERR("regulator is NULL");
		return -1;
	}

	ret = I2C_READ_16BIT_OIS(DW9786_CHIP_EN_ADDR, &status);
	if (!(ret < 0) && (status != 0)) {
		LOG_INF("OIS step power off");
		I2C_WRITE_16BIT_OIS(DW9786_MODE_CONTROL_ADDR, 0x0001);    /* active mode */
		mdelay(1);
		I2C_WRITE_16BIT_OIS(DW9786_ACTIVE_CONTROL_ADDR, 0x0002);    /* ois servo on */
		mdelay(1);
		I2C_WRITE_16BIT_OIS(DW9786_AF_SERVO_ON_CONTROL, 0x0002);    /* af servo on */
		mdelay(5);
		I2C_WRITE_16BIT_OIS(DW9786_TARGET_HALL_X_ADDR, -5000);
		I2C_WRITE_16BIT_OIS(DW9786_TARGET_HALL_Y_ADDR, 5000);
		mdelay(15);
		I2C_WRITE_16BIT_OIS(DW9786_TARGET_HALL_X_ADDR, -6000);
		I2C_WRITE_16BIT_OIS(DW9786_TARGET_HALL_Y_ADDR, 6000);
		mdelay(15);
	}

	ret = dw9786_release(dw9786);
	if (ret) {
		LOG_ERR("dw9786 release failed!\n");
	}

	ret = regulator_disable(dw9786->afvdd);
	if (ret) {
		LOG_ERR("disable dw9786->afvdd failed!\n");
		return ret;
	}
	LOG_INF("disable dw9786->afvdd success ret[%d]", ret);

	ret = regulator_disable(dw9786->vin);
	if (ret) {
		LOG_ERR("disable dw9786->vin failed!\n");
		return ret;
	}
	LOG_INF("disable dw9786->vin success ret[%d]", ret);

	ret = regulator_disable(dw9786->oisvdd);
	if (ret) {
		LOG_ERR("disable dw9786->oisvdd failed!\n");
		return ret;
	}
	LOG_INF("disable dw9786->oisvdd success ret[%d]", ret);

	ret = regulator_disable(dw9786->iovdd);
	if (ret) {
		LOG_ERR("disable dw9786->iovdd failed!\n");
		return ret;
	}
	LOG_INF("disable dw9786->iovdd success ret[%d]", ret);

	LOG_ERR("%s X\n", __func__);
	return ret;
}
#endif    /* GYRO_REG_MONITOR */

static int dw9786_batch(struct hf_device *hfdev, int sensor_type,
						 int64_t delay, int64_t latency)
{
	pr_debug("%s id:%d delay:%lld latency:%lld\n", __func__, sensor_type, delay, latency);
	return 0;
}

static short convert(unsigned short value)
{
	short l_value = 0;
	l_value = (short)(value + 65536L);
	return l_value;
}

int map_value(int value) {
	int mapped_value = 0;
	int64_t format_value = 0;
	if (value == 0) {
		LOG_ERR("map_value input error %d!\n", value);
		return mapped_value;
	}
	format_value = (value - 16000) << 12;
	format_value = format_value *125LL /3LL;
	/* Map the ratio to the output range -2048*1000000~2048*1000000 */
	mapped_value = (int)(format_value);
	LOG_DBG(dbg, "%s value =%d ,format_value = %lld ,mapped_value =%d \n", __func__, value, format_value, mapped_value);

	return mapped_value;
}

int dw9786_hall_convert(uint16_t value) {
	int mapped_value = 0;
	if (value > 8191) {
		mapped_value = value - 65536;
	} else {
		mapped_value = value;
	}

	return mapped_value;
}

static int dw9786_enable(struct hf_device *hfdev, int sensor_type, int en)
{
	struct i2c_client *client;
	struct v4l2_subdev *subdev;
	struct dw9786_device *dw9786;
	int ret = 0;

	if (0 == hfdev) {
		LOG_ERR("%s hfdev is null!! return error", __func__);
		return -1;
	}
	LOG_DBG(dbg, "%s dw9786 hfdev[%p]", __func__, hfdev);
	client = hf_device_get_private_data(hfdev);
	subdev = i2c_get_clientdata(client);
	dw9786 = subdev_to_dw9786_ois(subdev);

	LOG_ERR("%s sensor_type[%d] en[%d]", __func__, sensor_type, en);
	if (en) {
		ret = dw9786_power_on(dw9786, true);
		if (ret < 0) {
			return -1;
		}
		g_last_time = 0;
#ifdef GYRO_REG_MONITOR
		reg_monitor_thread_init(client);
#endif
	} else {
#ifdef GYRO_REG_MONITOR
		reg_monitor_thread_exit();
#endif
		g_last_time = 0;
		dw9786_power_off(dw9786);
	}

	return 0;
}

int dw9786_config_cali(struct hf_device *hfdev, int sensor_type, void *data, uint8_t length)
{
	struct i2c_client *client;
	int ret = 0;
	unsigned short offset_x, offset_y;
	mois_config_data *config;
	uint16_t status = 0;

	client = hf_device_get_private_data(hfdev);
	if (!client) {
		LOG_ERR("i2c client is null");
		return -1;
	}
	LOG_DBG(dbg, "sensor_type: %d, length: %d, mois_config_data size: %zd", sensor_type, length, sizeof(mois_config_data));
	if (data) {
		config = (mois_config_data *)data;
	} else {
		LOG_ERR("config data is null");
		return -1;
	}
	LOG_INF("config->mode: %d", config->mode);

	switch (config->mode) {
	case AK_Centering: {
		LOG_INF("into AK_Centering mode");
		ret = dw9786_servo_on();
		if (ret < 0) {
			LOG_ERR("enter to servo on mode failed, ret: %d", ret);
			return ret;
		}
		int ret = dw9786_set_hall(0, 0);
		if (ret < 0) {
			LOG_ERR("enter to AK_Centering failed, ret: %d", ret);
			return -1;
		}
		LOG_INF("AK_Centering on success");
	} break;
	case AK_WorkingMode: {
		LOG_INF("into AK_Working mode");
		LOG_INF("this mode to do");
	} break;
	case AK_StandbyMode: {
		LOG_INF("into AK_Standby mode");
		LOG_INF("this mode to do");
	} break;
	case AK_EnableMOIS: {
		if (sleep_mode_state) {
			ret = dw9786_idle_mode();
			if (ret < 0) {
				LOG_ERR("enter idle mode failed, ret: %d", ret);
				return -1;
			}
			sleep_mode_state = false;
			LOG_INF("enter idle mode success");
		}
	}
	fallthrough;
	case AK_Still: {
		LOG_INF("into ois on mode");
		ret = dw9786_ois_on();
		if (ret < 0) {
			LOG_ERR("ois on failed, ret: %d", ret);
			return -1;
		}
		g_still_en = 1;
		LOG_INF("ois on success");
	} break;
	case AK_Movie: {
		g_still_en = 1;
		return 0;
		if (sleep_mode_state) {
			ret = dw9786_idle_mode();
			if (ret < 0) {
				LOG_ERR("enter idle mode failed, ret: %d", ret);
				return 0;
			}
			sleep_mode_state = false;
			LOG_INF("enter idle mode success");
		}
		LOG_INF("into movie on mode");
		ret = dw9786_movie_mode_on();
		if (ret < 0) {
			LOG_ERR("movie on failed, ret: %d", ret);
			return 0;
		}
		g_still_en = 1;
		LOG_INF("movie on success");
	} break;
	case AK_DisableMOIS:
	fallthrough;
	case AK_CenteringOn: {
		LOG_INF("into center on mode");
		ret = dw9786_servo_on();
		if (ret < 0) {
			LOG_ERR("center_on failed, ret: %d", ret);
		}
		ret = dw9786_set_hall(0, 0);
		if (ret < 0) {
			LOG_ERR("center_on failed, ret: %d", ret);
			return 0;
		}
		g_still_en = 0;
		LOG_INF("center on success");
	} break;
	case AK_SleepMode: {
		LOG_INF("into sleep mode");
		ret = I2C_READ_16BIT_OIS(DW9786_CHIP_EN_ADDR, &status);
		if ((ret < 0) || (status == 0)) {
			LOG_INF("have enter sleep mode success");
			sleep_mode_state = true;
			return 0;
		}
		if (!sleep_mode_state) {
			ret = dw9786_sleep_mode();
			if (ret < 0) {
				LOG_ERR("enter sleep mode failed, ret: %d", ret);
				return 0;
			}
			sleep_mode_state = true;
			LOG_INF("enter sleep mode success");
		}
	} break;
	case AK_ManualMovieLens: {
		LOG_INF("into manual mode");
		ret = dw9786_servo_on();
		if (ret < 0) {
			LOG_ERR("enter to servo on mode failed, ret: %d", ret);
			return ret;
		}
		int ret = dw9786_set_hall(0, 0);
		if (ret < 0) {
			LOG_ERR("fixed failed, ret: %d", ret);
			return -1;
		}
		LOG_ERR("fixed on success");
	} break;
	case AK_Pantilt: {
		LOG_ERR("into pantilt mode");
		ret = dw9786_ois_on();
		if (ret < 0) {
			LOG_ERR("pantilt failed, ret: %d", ret);
			return -1;
		}
		g_still_en = 1;
		LOG_ERR("pantilt success");
	} break;
	case AK_TestMode: {
		if (1) {
			g_gyro_cal = 1;
			is_do_cali = true;
			LOG_ERR("do gyro offset...");
			dw9786_gyro_offset_calibration(&offset_x, &offset_y);
			g_gyro_offset[0] = convert(offset_x);
			g_gyro_offset[1] = convert(offset_y);
			LOG_ERR("%s sensor_type: %d, cali: [%d, %d], g_gyro_offset: [%d, %d]", __func__,
					sensor_type, offset_x, offset_y, g_gyro_offset[0], g_gyro_offset[1]);
			is_do_cali = false;
		} else {
			LOG_ERR("%s skip offset cali, sensor_type[%d] cali[%d,%d]",
					__func__, sensor_type, g_gyro_offset[0], g_gyro_offset[1]);
		}
	} break;
	case MOIS_Gyro_Gain_Cal: {
		LOG_ERR("into MOIS_Gyro_Gain_Cal");
		LOG_ERR("traverse gyro gain: [0x%04X, 0x%04X]", config->mois_gain_x, config->mois_gain_y);
		if (!g_still_en) {
			/* ois on */
			ret = dw9786_ois_on();
			if (ret < 0) {
				LOG_ERR("Gyro Gain ois on failed, ret[%d] return", ret);
				return -1;
			}
			LOG_ERR("Gyro Gain ois on success");
			g_still_en = 1;
		}
		g_gyro_gain_x = (uint16_t)config->mois_gain_x;
		g_gyro_gain_y = (uint16_t)config->mois_gain_y;
		dw9786_set_gyro_gain(g_gyro_gain_x, g_gyro_gain_y);
	} break;
	default:
		LOG_DBG(dbg, "into default mode, just break");
		break;
	}

	return 0;
}

uint64_t reverse_bytes(uint64_t num) {
	uint64_t reversed_num = 0;

	for (int i = 0; i < 8; ++i) {
		uint64_t byte_value = (num >> (i * 8)) & 0xFF;
		reversed_num |= byte_value << ((7 - i) * 8);
	}

	return reversed_num;
}

static int dw9786_sample(struct hf_device *hfdev)
{
	struct i2c_client *client;
	struct v4l2_subdev *subdev;
	struct dw9786_device *dw9786;
	struct hf_manager *manager;
	struct hf_manager_event event;
	int i2c_ret;
	int16_t position_x = 0;
	int16_t position_y = 0;
	uint8_t data[110];
	uint64_t reverse_timestamp = 0;
	uint16_t fifo_count = 0;
	uint32_t qtime_h = 0;
	uint32_t qtime_l = 0;
	uint64_t qtime = 0;
	uint64_t after_i2c_write_timestamp = 0;

	if (sleep_mode_state) {
		goto err;
	}

	if (is_do_cali) {
		goto err;
	}

	if (hfdev) {
		client = hf_device_get_private_data(hfdev);
	} else {
		LOG_ERR("NULL hfdev");
		goto err;
	}
	if (client) {
		subdev = i2c_get_clientdata(client);
	} else {
		LOG_ERR("NULL client");
		goto err;
	}
	if (subdev) {
		dw9786 = subdev_to_dw9786_ois(subdev);
	} else {
		LOG_ERR("NULL subdev");
		goto err;
	}
	if (dw9786) {
		manager = dw9786->hf_dev.manager;
	} else {
		LOG_ERR("NULL dw9786");
		goto err;
	}
	if (!manager) {
		LOG_ERR("NULL manager");
		goto err;
	}

	memset(&event, 0, sizeof(struct hf_manager_event));
	event.sensor_type = SENSOR_TYPE_OIS2;
	event.accurancy = SENSOR_ACCURANCY_HIGH;
	event.action = DATA_ACTION;
	event.timestamp = ktime_get_boottime_ns();

	reverse_timestamp = reverse_bytes(event.timestamp);
	I2C_BLOCK_WRITE_OIS(0xB970, &reverse_timestamp, sizeof(uint64_t));
	after_i2c_write_timestamp = ktime_get_boottime_ns();
	dw9786_udelay(500);
	i2c_ret = I2C_BLOCK_READ_OIS(0xB900, data, 10);
	if (i2c_ret < 0) {
		LOG_ERR("read EIS data failed, ret: %d", i2c_ret);
		goto err;
	}
	qtime_h = ((uint32_t)data[2] << 24) + ((uint32_t)data[3] << 16) + ((uint32_t)data[4] << 8) + data[5];
	qtime_l = ((uint32_t)data[6] << 24) + ((uint32_t)data[7] << 16) + ((uint32_t)data[8] << 8) + data[9];
	qtime = qtime_l + ((uint64_t)qtime_h << 32);
	LOG_DBG(dbg, "write timestamp: 0x%016llX, reverse timestamp: 0x%016llX, read timestamp: 0x%016llX", event.timestamp, reverse_timestamp, qtime);
	LOG_DBG(dbg, "write timestamp: %lld, read timestamp: %lld, Delta T:%lld", event.timestamp, qtime, event.timestamp - qtime);
	qtime += after_i2c_write_timestamp - event.timestamp;
	LOG_DBG(dbg, "i2c_write_time: %lld, timestamp after i2c_write_delay cali: %lld", after_i2c_write_timestamp - event.timestamp, qtime);

	if (abs(event.timestamp - (int64_t)qtime) > 11 * 1000 * 1000) {
		LOG_ERR("timestamps not legal, write timestamp: %lld, read timestamp: %lld, Delta T:%lld", event.timestamp, qtime, event.timestamp - qtime);
		goto err;
	}

	fifo_count = data[1] & 0xFF;
	fifo_count = fifo_count > 25 ? 25 : fifo_count;
	LOG_DBG(dbg, "fifo_count: %d", fifo_count);
	i2c_ret = I2C_BLOCK_READ_OIS(0xB90A, data, fifo_count * 4);
	if (i2c_ret < 0) {
		LOG_ERR("read EIS data failed, ret: %d", i2c_ret);
		goto err;
	}
	for (int i = 0; i < fifo_count && i < 25; i += 1) {
		event.timestamp = qtime - 2000000 * (fifo_count - i + 1);
		position_x = dw9786_hall_convert(((uint16_t)data[i*4] << 8) + data[i*4 + 1]);
		position_y = dw9786_hall_convert(((uint16_t)data[i*4 + 2] << 8) + data[i*4 + 3]);
		LOG_DBG(dbg, "ts[%lld] data[%d]: hall_x: %d, hall_y: %d", event.timestamp, i, position_x, position_y);
		if (abs(position_x) > 10000 || abs(position_y) > 10000) {
			LOG_ERR("position not legal position_x: %d, position_y: %d", position_x, position_y);
			continue;
		}

		/*OIS data format
		data[0 1]: gyro_x, gyro_y
		data[2 3]: target_x, target_y
		data[4 5]: hall_x, hall_y*/
		event.word[0] = 0;
		event.word[1] = 0;
		event.word[2] = position_x;
		event.word[3] = position_y;
		event.word[4] = position_x;
		event.word[5] = position_y;
		manager->report(manager, &event);
	}
	manager->complete(manager);

	return 0;

err:
	g_last_time = 0;
	return -1;
}

static int dw9786_set_lock_state(int state)
{
	int ret = 0;
	int ret1 = 0;
	uint16_t chip_en;
	uint16_t mcu_active;
	uint16_t af_status;

	int puSendCmdArray[6][3] = {
		{0xB026, 0x0001, 1},    /* OP mode */
		{0xB022, 0x0002, 1},    /* OIS Servo On */
		{0xB024, 0x0002, 1},    /* AF Servo On */
		{0xB0B2, 0x8000, 1},    /* still mode */
		{0xB96E, 0x1901, 1},    /* EIS Param */
		{0xB022, 0x0001, 1},    /* OIS On */
	};
	unsigned short stdby[17] = {0x0100, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
								0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
								0x0000, 0x0000, 0x0000};

	LOG_INF("E\n");

	if (dw9786->vin == NULL || dw9786->iovdd == NULL || dw9786->afvdd == NULL) {
		LOG_ERR("regulator is NULL");
		return -1;
	}

	if (state && (atomic_read(&dw9786_shake_state) == 0)) {
		ret = regulator_enable(dw9786->iovdd);
		ret1 = regulator_set_voltage(dw9786->iovdd, 1800000, 1800000);
		if (ret < 0 || ret1 < 0) {
			LOG_ERR("%s dw9786->iovdd set failed ret[%d] ret1[%d]", __func__,
				ret, ret1);
			return -1;
		}
		LOG_INF("dw9786->iovdd set success ret[%d], ret1[%d]", ret, ret1);

		ret = regulator_enable(dw9786->vin);
		if (ret < 0 || ret1 < 0) {
			LOG_ERR("%s dw9786->vin set failed ret[%d] ret1[%d]", __func__,
				ret, ret1);
			return -1;
		}
		LOG_INF("dw9786->vin set success ret[%d], ret1[%d]", ret, ret1);

		ret = regulator_enable(dw9786->oisvdd);
		ret1 = regulator_set_voltage(dw9786->oisvdd, 3100000, 3100000);
		if (ret < 0 || ret1 < 0) {
			LOG_ERR("%s dw9786->oisvdd set failed ret[%d] ret1[%d]", __func__,
				ret, ret1);
			return -1;
		}
		LOG_INF("dw9786->oisvdd set success ret[%d], ret1[%d]", ret, ret1);
		mdelay(2);

		ret = regulator_enable(dw9786->afvdd);
		ret1 = regulator_set_voltage(dw9786->afvdd, 3100000, 3100000);
		if (ret < 0 || ret1 < 0) {
			LOG_ERR("%s dw9786->afvdd set failed ret[%d] ret1[%d]", __func__,
				ret, ret1);
			return -1;
		}
		LOG_INF("dw9786->afvdd set success ret[%d], ret1[%d]", ret, ret1);

		udelay(5000);

		I2C_READ_16BIT_OIS(0xE000, &chip_en);
		LOG_INF("chip_en: 0x%04X", chip_en);
		if (chip_en != 0x0001) {
			ret = I2C_WRITE_16BIT_OIS(0xE000, 0x0000);
			mdelay(2);
			I2C_BLOCK_WRITE_OIS(0xE000, (unsigned char *)stdby, 34);
			mdelay(5);
		}

		I2C_READ_16BIT_OIS(0xE004, &mcu_active);
		LOG_INF("mcu_active: 0x%04X", mcu_active);
		if(mcu_active != 0x0001) {
			ret = I2C_WRITE_16BIT_OIS(0xE004, 0x0001);
			mdelay(20);
		}

		for (int cmd_number = 0; cmd_number < sizeof(puSendCmdArray) / sizeof(puSendCmdArray[0]); cmd_number++) {
			ret = I2C_WRITE_16BIT_OIS(puSendCmdArray[cmd_number][0], puSendCmdArray[cmd_number][1]);
			mdelay(puSendCmdArray[cmd_number][2]);
			LOG_INF("dw9786_init: %x\n", cmd_number);
		}

		I2C_READ_16BIT_OIS(0xB020, &af_status);
		LOG_INF("af status: 0x%04X", af_status);
		atomic_set(&dw9786_shake_state, 1);
	} else if (state == 0 && (atomic_read(&dw9786_shake_state) == 1)) {
		ret = regulator_disable(dw9786->afvdd);
		if (ret) {
			LOG_ERR("disable dw9786->afvdd failed!\n");
			return ret;
		}
		LOG_INF("disable dw9786->afvdd success ret[%d]", ret);

		ret = regulator_disable(dw9786->vin);
		if (ret) {
			LOG_ERR("disable dw9786->vin failed!\n");
			return ret;
		}
		LOG_INF("disable dw9786->vin success ret[%d]", ret);

		ret = regulator_disable(dw9786->oisvdd);
		if (ret) {
			LOG_ERR("disable dw9786->oisvdd failed!\n");
			return ret;
		}
		LOG_INF("disable dw9786->oisvdd success ret[%d]", ret);

		ret = regulator_disable(dw9786->iovdd);
		if (ret) {
			LOG_ERR("disable dw9786->iovdd failed!\n");
			return ret;
		}
		LOG_INF("disable dw9786->iovdd success ret[%d]", ret);
		atomic_set(&dw9786_shake_state, 0);
	}
	LOG_INF("X\n");
	return 0;
}

static int dw9786_custom_cmd(struct hf_device *hfdev, int sensor_type, struct custom_cmd *cust_cmd)
{
	LOG_DBG(dbg, "cammand: 0x%x, type: %d, rxlen: %d, txlen: %d",
			cust_cmd->command, sensor_type, cust_cmd->rx_len, cust_cmd->tx_len);
	/* print cmd data for debug */
	/*for (int i = 0; i < cust_cmd->tx_len; i++) {
		printk("%s cust_cmd->data %x ", __func__, cust_cmd->data[i]);
	}*/
	switch (cust_cmd->command) {
	case 0xDC: {
		cust_cmd->rx_len = 2;
		cust_cmd->data[0] = g_gyro_offset[0];
		cust_cmd->data[1] = g_gyro_offset[1];
	}
		break;
	case 0x85: {    /* save gain to flash */
		LOG_ERR("save gain[%d,%d] len[%d]", cust_cmd->data[0], cust_cmd->data[1], cust_cmd->tx_len);
		g_gyro_gain_x = (unsigned short)cust_cmd->data[0];
		g_gyro_gain_y = (unsigned short)cust_cmd->data[1];
		dw9786_set_gyro_gain(g_gyro_gain_x, g_gyro_gain_y);
		dw9786_set_store();
	}
		break;
	case 0x10: {    /* ois manual control */
		uint16_t ois_status = 0;
		I2C_READ_16BIT_OIS(DW9786_STATUS_ADDR, &ois_status);
		if (ois_status != 0x1022) {
			dw9786_servo_on();
		}
		LOG_ERR("save hall[%d,%d] len[%d]", cust_cmd->data[0], cust_cmd->data[1], cust_cmd->tx_len);
		dw9786_set_hall(cust_cmd->data[0], cust_cmd->data[1]);
	}
		break;
	case 0x8D: {    /* lock OIS by shake detect */
		LOG_INF("into lock af and ois: %d", cust_cmd->data[0]);
		dw9786_set_lock_state(cust_cmd->data[0]);
	}
		break;
	default:
		break;
	}

	return 0;
}

static const struct v4l2_subdev_internal_ops dw9786_int_ops = {
	.open = dw9786_open,
	.close = dw9786_close,
};

static struct v4l2_subdev_core_ops dw9786_ops_core = {
	.ioctl = dw9786_ops_core_ioctl,
};

static const struct v4l2_subdev_ops dw9786_ops = {
	.core = &dw9786_ops_core,
};

static void dw9786_subdev_cleanup(struct dw9786_device *dw9786)
{
	v4l2_async_unregister_subdev(&dw9786->subdev);
	v4l2_ctrl_handler_free(&dw9786->ctrls);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&dw9786->subdev.entity);
#endif
}

static int dw9786_init_controls(struct dw9786_device *dw9786)
{
	struct v4l2_ctrl_handler *hdl = &dw9786->ctrls;

	v4l2_ctrl_handler_init(hdl, 1);

	if (hdl->error) {
		LOG_ERR("v4l2_ctrl_handler_init failed, hdl->error: %d", hdl->error);
		return hdl->error;
	}

	dw9786->subdev.ctrl_handler = hdl;

	return 0;
}
static int dw9786_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	int ret;
	int boot_mode;
	bool reg_rewrite_flag = false;

	uint16_t id;

	LOG_INF("E");

	mutex_init(&dw9786_idle_mutex);
	sema_init(&dw9786_power_on_sem, 0);

	dw9786 = devm_kzalloc(dev, sizeof(*dw9786), GFP_KERNEL);
	if (!dw9786)
		return -ENOMEM;

	dw9786->need_ois_on = true;

	/* get regulator for OIS */
	dw9786->vin = devm_regulator_get(dev, "vin");
	if (IS_ERR(dw9786->vin)) {
		ret = PTR_ERR(dw9786->vin);
		if (ret != -EPROBE_DEFER)
			LOG_ERR("cannot get vin regulator\n");
		return ret;
	}
	dw9786->oisvdd = devm_regulator_get(dev, "oisvdd");
	if (IS_ERR(dw9786->oisvdd)) {
		ret = PTR_ERR(dw9786->oisvdd);
		if (ret != -EPROBE_DEFER)
			LOG_ERR("cannot get oisvdd regulator\n");
		return ret;
	}
	/* get regulator for sensor, since the i2c is to be used */
	dw9786->iovdd = devm_regulator_get(dev, "iovdd");
	if (IS_ERR(dw9786->iovdd)) {
		ret = PTR_ERR(dw9786->iovdd);
		if (ret != -EPROBE_DEFER)
			LOG_ERR("cannot get iovdd regulator\n");
		return ret;
	}
	dw9786->afvdd = devm_regulator_get(dev, "afvdd");
	if (IS_ERR(dw9786->afvdd)) {
		ret = PTR_ERR(dw9786->afvdd);
		if (ret != -EPROBE_DEFER)
			LOG_ERR("cannot get afvdd regulator\n");
		return ret;
	}

	v4l2_i2c_subdev_init(&dw9786->subdev, client, &dw9786_ops);
	dw9786->subdev.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	dw9786->subdev.internal_ops = &dw9786_int_ops;

	ret = dw9786_init_controls(dw9786);
	if (ret != 0) {
		LOG_ERR("dw9786_init_controls failed");
		goto err_cleanup;
	}

	dw9786->hf_dev.dev_name = "dw9786_ois";
	dw9786->hf_dev.device_poll = HF_DEVICE_IO_POLLING;
	dw9786->hf_dev.device_bus = HF_DEVICE_IO_SYNC;
	dw9786->hf_dev.support_list = support_sensors;
	dw9786->hf_dev.support_size = ARRAY_SIZE(support_sensors);
	dw9786->hf_dev.enable = dw9786_enable;
	dw9786->hf_dev.batch = dw9786_batch;
	dw9786->hf_dev.sample = dw9786_sample;
	dw9786->hf_dev.custom_cmd = dw9786_custom_cmd;
	dw9786->hf_dev.config_cali = dw9786_config_cali;
	hf_device_set_private_data(&dw9786->hf_dev, client);
	ret = hf_device_register_manager_create(&dw9786->hf_dev);
	if (ret < 0) {
		LOG_ERR("hf_manager_create fail");
		goto err_cleanup;
	}

	ret = device_create_file(dev, &dev_attr_dw9786_dbg);
	if (ret) {
		LOG_ERR("failed to create sysfs dw9786_dbg\n");
	}
	ret = device_create_file(dev, &dev_attr_dw9786_i2c_ops);
	if (ret) {
		LOG_ERR("failed to create sysfs dw9786_i2c_ops\n");
	}
	ret = device_create_file(dev, &dev_attr_dw9786_i2c_ops32);
	if (ret) {
		LOG_ERR("failed to create sysfs dw9786_i2c_ops32\n");
	}

#if defined(CONFIG_MEDIA_CONTROLLER)
	ret = media_entity_pads_init(&dw9786->subdev.entity, 0, NULL);
	if (ret < 0) {
		goto err_cleanup;
	}

	dw9786->subdev.entity.function = MEDIA_ENT_F_LENS;
#endif

	ret = v4l2_async_register_subdev(&dw9786->subdev);
	if (ret < 0) {
		LOG_ERR("v4l2_async_register_subdev failed.");
		goto err_cleanup;
	}

	gyro_client = hf_client_create();

	ret = dw9786_power_on(dw9786, false);
	if (ret < 0) {
		LOG_ERR("dw9786 init failed caused I2C failed.");
		goto err_cleanup;
	}
	down(&dw9786_power_on_sem);
	ret = I2C_READ_16BIT_OIS(DW9786_CHIP_ID_ADDR, &id);
	LOG_INF("id: 0x%0x", id);
	if (id == DW9786_CHIP_ID_VAL) {
		LOG_INF("probed");
	} else {
		LOG_INF("probed failed");
		goto err_probe;
	}

	/* set gyro type */
	dw9786_check_and_write(DW9786_IMU_TYPE_ADDR, IMU_TYPE_ICM45621, &reg_rewrite_flag);

	/* Set gyro direction */
	/* GYRO_ROT_MAT_X */
	dw9786_check_and_write(0xB812, 0x7FFF, &reg_rewrite_flag);
	dw9786_check_and_write(0xB814, 0x7FFF, &reg_rewrite_flag);
	/* GYRO_ROT_MAT_Y */
	dw9786_check_and_write(0xB818, 0x0000, &reg_rewrite_flag);
	dw9786_check_and_write(0xB81A, 0x0000, &reg_rewrite_flag);
	/* GYRO_GAIN_POL_X */
	dw9786_check_and_write(0xB800, 0x0001, &reg_rewrite_flag);
	/* GYRO_GAIN_POL_Y */
	dw9786_check_and_write(0xB802, 0xFFFF, &reg_rewrite_flag);

	/* read gyro gain, if gain is 0 write golden */
	ret = I2C_READ_16BIT_OIS(DW9786_GYRO_GAIN_X, &gyro_gain_x);
	if (ret < 0) {
		LOG_ERR("read gyro gain X failed ret[%d]", ret);
		gyro_gain_x = GYRO_GAIN_X_GOLDEN;
	}
	if (gyro_gain_x == 0 || gyro_gain_x > DW9786_GYRO_GAIN_X_MAX) {
		ret = I2C_WRITE_16BIT_OIS(DW9786_GYRO_GAIN_X, GYRO_GAIN_X_GOLDEN);
		LOG_INF("gyro gain x is 0 or too large, write golden");
		if (ret < 0) {
			LOG_ERR("write golden gain failed: ret: %d", ret);
		}
		gyro_gain_x = GYRO_GAIN_X_GOLDEN;
	}
	ret =  I2C_READ_16BIT_OIS(DW9786_GYRO_GAIN_Y, &gyro_gain_y);
	if (ret < 0) {
		LOG_ERR("read gyro gain Y failed ret[%d]", ret);
		gyro_gain_y = GYRO_GAIN_Y_GOLDEN;
	}
	if (gyro_gain_y == 0 || gyro_gain_y > DW9786_GYRO_GAIN_Y_MAX) {
		ret = I2C_WRITE_16BIT_OIS(DW9786_GYRO_GAIN_Y, GYRO_GAIN_Y_GOLDEN);
		LOG_INF("gyro gain y is 0 or too large, write golden");
		if (ret < 0) {
			LOG_ERR("write golden gain failed: ret: %d", ret);
		}
		gyro_gain_y = GYRO_GAIN_Y_GOLDEN;
	}
	if (reg_rewrite_flag) {
		LOG_INF("reg value has been changed, exec set store");
		dw9786_set_store();
		dw9786_device_reset();
	}

	dw9786_chip_info();
	/* only update firmeware at normal boot */
	boot_mode = get_boot_mode();
	if (FACTORY_BOOT != boot_mode) {
		ret = dw9786_update_fw();
		if (ret < 0) {
			LOG_ERR("updata firmware failed!");
		}
	} else {
		LOG_ERR("boot_mode %d not update firmware", boot_mode);
	}

	dw9786_power_off(dw9786);

	pm_runtime_enable(dev);

	LOG_INF("X");

	return 0;

err_probe:
	dw9786_power_off(dw9786);
	return 0;

err_cleanup:
	dw9786_power_off(dw9786);
	if (dw9786->hf_dev.manager) {
		hf_device_unregister_manager_destroy(&dw9786->hf_dev);
	}
	dw9786_subdev_cleanup(dw9786);
	LOG_ERR("dw9786 err_cleanup");
	return ret;
}

static void dw9786_remove(struct i2c_client *client)
{
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct dw9786_device *dw9786 = subdev_to_dw9786_ois(subdev);
	struct device *dev = &client->dev;
	LOG_ERR("remove dw9786");

	dw9786_subdev_cleanup(dw9786);
	pm_runtime_disable(&client->dev);
	device_remove_file(dev, &dev_attr_dw9786_dbg);
	device_remove_file(dev, &dev_attr_dw9786_i2c_ops);
	device_remove_file(dev, &dev_attr_dw9786_i2c_ops32);

	if (gyro_client) {
		hf_client_destroy(gyro_client);
	}

	if (!pm_runtime_status_suspended(&client->dev))
		dw9786_power_off(dw9786);
	pm_runtime_set_suspended(&client->dev);

	mutex_destroy(&dw9786_idle_mutex);

	return;
}

static int dw9786_ois_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct dw9786_device *dw9786 = subdev_to_dw9786_ois(subdev);

	return dw9786_power_off(dw9786);
}

static int dw9786_ois_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct dw9786_device *dw9786 = subdev_to_dw9786_ois(subdev);

	return dw9786_power_on(dw9786, true);
}

static const struct i2c_device_id dw9786_id_table[] = {
	{DW9786_NAME, 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, dw9786_id_table);

static const struct of_device_id dw9786_of_table[] = {
	{.compatible = "oplus,ois_dw9786"},
	{},
};
MODULE_DEVICE_TABLE(of, dw9786_of_table);

static const struct dev_pm_ops dw9786_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend, pm_runtime_force_resume)
	SET_RUNTIME_PM_OPS(dw9786_ois_suspend, dw9786_ois_resume, NULL)
};

static struct i2c_driver dw9786_i2c_driver = {
	.driver = {
		.name = DW9786_NAME,
		.of_match_table = dw9786_of_table,
	},
	.probe = dw9786_probe,
	.remove = dw9786_remove,
	.id_table = dw9786_id_table,
};
module_i2c_driver(dw9786_i2c_driver);

MODULE_AUTHOR("WangJianwei");
MODULE_DESCRIPTION("DW9786 OIS driver");
MODULE_LICENSE("GPL v2");
