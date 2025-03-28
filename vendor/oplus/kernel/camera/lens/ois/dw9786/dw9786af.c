/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 OPLUS Inc.
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm_runtime.h>
#include <linux/mutex.h>
#include <linux/atomic.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include "adaptor-i2c.h"
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include "hf_manager.h"

#define DRIVER_NAME                  "dw9786"
#define DW9786_I2C_SLAVE_ADDR        0x32

#define LOG_INF(format, args...)                                               \
	pr_info(DRIVER_NAME " [%s] " format, __func__, ##args)

#define DW9786_NAME				 "dw9786"
#define DW9786_MAX_FOCUS_POS		  4095
#define DW9786_ORIGIN_FOCUS_POS	  512
/*
 * This sets the minimum granularity for the focus positions.
 * A value of 1 gives maximum accuracy for a desired focus position
 */
#define DW9786_FOCUS_STEPS			  1
#define DW9786_SET_POSITION_ADDR	  0xB300
// #define DW9786_STATUS_ADDR			  0x05

#define DW9786_CMD_DELAY			  0xff
#define DW9786_CTRL_DELAY_US		  1000
#define DW9786_POS_CTRL_DELAY_US    1000
/*
 * This acts as the minimum granularity of lens movement.
 * Keep this value power of 2, so the control steps can be
 * uniformly adjusted for gradual lens movement, with desired
 * number of control steps.
 */
#define DW9786_MOVE_STEPS			  400
// base on 0x06 and 0x07 setting
// tVIB = (6.3 + (SACT[5:0]) *0.1)*DIV[2:0] ms
// 0x06 = 0x40 ==> SAC3
// 0x07 = 0x60 ==> tVIB = 9.4ms
// op_time = 9.4 * 0.72 = 6.77ms
// tolerance -+ 19%
#define DW9786_MOVE_DELAY_US		  8100

atomic_t g_is_need_standby = ATOMIC_INIT(0);
static int g_last_pos = DW9786_ORIGIN_FOCUS_POS;
extern struct mutex dw9786_mutex;
static DEFINE_MUTEX(dw9786_suspend_mutex);
static struct workqueue_struct *dw9786_suspend_wq;

/* dw9786 device structure */
struct dw9786_device {
	struct v4l2_ctrl_handler ctrls;
	struct v4l2_subdev sd;
	struct v4l2_ctrl *focus;
	struct regulator *afvdd;
	struct regulator *iovdd;
	struct regulator *oisvdd;
	/* active or standby mode */
	bool active;
	bool is_suspending;
	bool stop_wait_suspend_flag;
	wait_queue_head_t wait_suspend_queue_head;
	struct work_struct suspend_work;
	struct hf_client *client;
};

#define VCM_IOC_POWER_ON         _IO('V', BASE_VIDIOC_PRIVATE + 4)
#define VCM_IOC_POWER_OFF        _IO('V', BASE_VIDIOC_PRIVATE + 5)

static inline struct dw9786_device *to_dw9786_vcm(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct dw9786_device, ctrls);
}

static inline struct dw9786_device *sd_to_dw9786_vcm(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct dw9786_device, sd);
}

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};


static int dw9786_set_position(struct dw9786_device *dw9786, u16 val)
{
	int ret;
	struct i2c_client *client = v4l2_get_subdevdata(&dw9786->sd);
	uint16_t set_pos, last_actual_pos, status;

	adaptor_i2c_rd_u16(client, client->addr, 0xB300, &set_pos);
	adaptor_i2c_rd_u16(client, client->addr, 0xB302, &last_actual_pos);
	adaptor_i2c_rd_u16(client, client->addr, 0xB020, &status);
	LOG_INF("set pos: %d, reg pos: %d, last actual pos: %d, AF status: 0x%x", val, set_pos, last_actual_pos, status);

	ret = adaptor_i2c_wr_u16(client, client->addr, DW9786_SET_POSITION_ADDR, val);

	return ret;
}

static int dw9786_release(struct dw9786_device *dw9786)
{
	int ret, val;
	int diff_dac = 0;
	int nStep_count = 0;
	int i = 0;
	struct i2c_client *client = v4l2_get_subdevdata(&dw9786->sd);

	diff_dac = DW9786_ORIGIN_FOCUS_POS - dw9786->focus->val;

	nStep_count = (diff_dac < 0 ? (diff_dac*(-1)) : diff_dac) /
		DW9786_MOVE_STEPS;

	val = dw9786->focus->val;

	for (i = 0; i < nStep_count; ++i) {
		val += (diff_dac < 0 ? (DW9786_MOVE_STEPS*(-1)) : DW9786_MOVE_STEPS);

		ret = dw9786_set_position(dw9786, val);
		if (ret < 0) {
			LOG_INF("%s I2C failure: %d",
				__func__, ret);
			return ret;
		}
		usleep_range(DW9786_MOVE_DELAY_US,
			     DW9786_MOVE_DELAY_US + 1000);
	}

	// last step to origin
	ret = dw9786_set_position(dw9786, DW9786_ORIGIN_FOCUS_POS);
	if (ret < 0) {
		LOG_INF("%s I2C failure: %d",
			__func__, ret);
		return ret;
	}

	adaptor_i2c_wr_u16(client, client->addr, 0xE000, 0x0000);
	dw9786->active  = false;

	LOG_INF("-\n");

	return 0;
}

static int dw9786_init(void* data)
{
	int retry = 6;
	struct dw9786_device* dw9786 = (struct dw9786_device*)data;
	struct i2c_client *client = v4l2_get_subdevdata(&dw9786->sd);
	int ret = 0;
	int puSendCmdArray[6][3] = {
	{0xB026, 0x0001, 1},    // OP mode
	{0xB022, 0x0002, 1},    // OIS Servo On
	{0xB024, 0x0002, 1},    // AF Servo On
	{0xB0B2, 0x8000, 1},    // still mode
	{0xB96E, 0x1902, 1},    // EIS Param
	{0xB022, 0x0001, 1},    // OIS On
	};
	unsigned short stdby[17] = {0x0100, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
								0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
								0x0000, 0x0000, 0x0000};
	unsigned char cmd_number;
	unsigned char checksum_byte[2];
	uint16_t chip_en;
	uint16_t mcu_active;
	uint16_t af_status;
	LOG_INF("+\n");
	LOG_INF("dw9786_mutex: %p", &dw9786_mutex);
	mutex_lock(&dw9786_mutex);

	client->addr = DW9786_I2C_SLAVE_ADDR >> 1;

	LOG_INF("Check HW version: %x\n", ret);

	adaptor_i2c_rd_u16(client, client->addr, 0xB020, &af_status);
	LOG_INF("af status: 0x%04X", af_status);
	if (af_status == 0x1022 || af_status == 0x1021) {
		dw9786->active  = true;
		LOG_INF("af has been enabled, return 0");
		mutex_unlock(&dw9786_mutex);
		return 0;
	}

	do{
		adaptor_i2c_rd_u16(client, client->addr, 0xE000, &chip_en);
		LOG_INF("chip_en: 0x%04X", chip_en);
		if (chip_en != 0x0001) {
			ret = adaptor_i2c_wr_u16(client, client->addr, 0xE000, 0x0000);
			mdelay(2);
			adaptor_i2c_wr_p8(client, client->addr, 0xE000, (unsigned char *)stdby, 34);
			mdelay(5);
		}

		adaptor_i2c_rd_u16(client, client->addr, 0xE004, &mcu_active);
		LOG_INF("mcu_active: 0x%04X", mcu_active);
		if(mcu_active != 0x0001) {
			ret = adaptor_i2c_wr_u16(client, client->addr, 0xE004, 0x0001);
			mdelay(20);
		}

		for (cmd_number = 0; cmd_number < sizeof(puSendCmdArray) / sizeof(puSendCmdArray[0]); cmd_number++) {
			ret = adaptor_i2c_wr_u16(client, client->addr,
					puSendCmdArray[cmd_number][0],
					puSendCmdArray[cmd_number][1]);
			mdelay(puSendCmdArray[cmd_number][2]);
			LOG_INF("dw9786_init: %x\n", cmd_number);
		}

		adaptor_i2c_rd_u16(client, client->addr, 0xB020, &af_status);
		LOG_INF("af status: 0x%04X", af_status);

		retry--;
	} while (af_status != 0x1021 && retry > 0);

	dw9786->active  = true;

	adaptor_i2c_rd_p8(client, client->addr, 0xB022, checksum_byte, 2);
	LOG_INF("checksum: 0x%x%x\n", checksum_byte[0], checksum_byte[1]);

	mutex_unlock(&dw9786_mutex);

	LOG_INF("-\n");
	return ret;
}

static void stop_background_works(struct dw9786_device *dw9786)
{
	LOG_INF("+\n");

	atomic_set(&g_is_need_standby, 0);
	dw9786->stop_wait_suspend_flag = true;

	wake_up_interruptible(&dw9786->wait_suspend_queue_head);

	if (dw9786_suspend_wq) {
		LOG_INF("flush dw9786 suspend work queue\n");

		/* flush work queue */
		flush_work(&dw9786->suspend_work);

		flush_workqueue(dw9786_suspend_wq);
		destroy_workqueue(dw9786_suspend_wq);
		dw9786_suspend_wq = NULL;
	}

	dw9786->stop_wait_suspend_flag = false;

	LOG_INF("-\n");
}

static int enable_disable_imu(struct dw9786_device *dw9786, bool action)
{
	int ret = -1;
	uint8_t sensor_type = SENSOR_TYPE_OIS_IMU;
	struct hf_manager_cmd cmd;
	struct hf_manager_batch *batch = NULL;

	if (NULL == dw9786->client) {
		dw9786->client = hf_client_create();
	}
	if (NULL == dw9786->client) {
		LOG_INF("hf_client_create fail\n");
		return -1;
	}

	ret = hf_client_find_sensor(dw9786->client, sensor_type);
	if (ret < 0) {
		LOG_INF("hf_client_find_sensor %u fail, ret: %d\n", sensor_type, ret);
		return -2;
	}
	LOG_INF("hf_client_find_sensor %u success\n", sensor_type);

	memset(&cmd, 0, sizeof(cmd));
	cmd.sensor_type = sensor_type;

	if (action) {
		cmd.action = HF_MANAGER_SENSOR_ENABLE;
		batch = (struct hf_manager_batch *)cmd.data;
		batch->delay = 0;
		batch->latency = 0;
	} else {
		cmd.action = HF_MANAGER_SENSOR_DISABLE;
	}

	ret = hf_client_control_sensor(dw9786->client, &cmd);
	if (ret < 0) {
		LOG_INF("hf_client_control_sensor %u fail, ret: %d\n", sensor_type, ret);
		return -3;
	}
	LOG_INF("hf_client_control_sensor %u success\n", sensor_type);

	return 0;
}

/* Power handling */
static int dw9786_power_off(struct dw9786_device *dw9786)
{
	int ret;

	LOG_INF("+\n");

	ret = dw9786_release(dw9786);
	if (ret)
		LOG_INF("dw9786 release failed!\n");

	ret = regulator_disable(dw9786->oisvdd);
	if (ret) {
		LOG_INF("disable dw9786->oisvdd failed!\n");
		return ret;
	}
	LOG_INF("disable dw9786->iovdd success ret[%d]", ret);

	ret = regulator_disable(dw9786->afvdd);
	if (ret) {
		LOG_INF("disable dw9786->iovdd failed!\n");
		return ret;
	}
	LOG_INF("disable dw9786->iovdd success ret[%d]", ret);

	ret = regulator_disable(dw9786->iovdd);
	if (ret) {
		LOG_INF("disable dw9786->iovdd failed!\n");
		return ret;
	}
	LOG_INF("disable dw9786->iovdd success ret[%d]", ret);

	dw9786->active = false;
	enable_disable_imu(dw9786, false);
	LOG_INF("-\n");

	return ret;
}

static int dw9786_power_on(struct dw9786_device *dw9786)
{
	int ret, ret1;
	int err;
	struct task_struct *dw9786_init_task;

	LOG_INF("+\n");

	ret = regulator_enable(dw9786->iovdd);
	ret1 = regulator_set_voltage(dw9786->iovdd, 1800000, 1800000);
	if (ret < 0 || ret1 < 0) {
		LOG_INF("%s dw9786->iovdd set failed ret[%d] ret1[%d]", __func__,
			   ret, ret1);
		return -1;
	}
	LOG_INF("dw9786->iovdd set success ret[%d], ret1[%d]", ret, ret1);

	ret = regulator_enable(dw9786->oisvdd);
	ret1 = regulator_set_voltage(dw9786->oisvdd, 3100000, 3100000);
	if (ret < 0 || ret1 < 0) {
		LOG_INF("%s dw9786->oisvdd set failed ret[%d] ret1[%d]", __func__,
			   ret, ret1);
		return -1;
	}
	LOG_INF("dw9786->oisvdd set success ret[%d], ret1[%d]", ret, ret1);
	mdelay(2);

	ret = regulator_enable(dw9786->afvdd);
	ret1 = regulator_set_voltage(dw9786->afvdd, 3100000, 3100000);
	if (ret < 0 || ret1 < 0) {
		LOG_INF("%s dw9786->afvdd set failed ret[%d] ret1[%d]", __func__,
			   ret, ret1);
		return -1;
	}
	LOG_INF("dw9786->afvdd set success ret[%d], ret1[%d]", ret, ret1);

	if (ret < 0)
		return ret;

	enable_disable_imu(dw9786, true);

	/*
	 * TODO(b/139784289): Confirm hardware requirements and adjust/remove
	 * the delay.
	 */
	mdelay(3);

	dw9786_init_task = kthread_create(dw9786_init, dw9786, "dw9786_init_kthread");
	if (IS_ERR(dw9786_init_task)) {
		LOG_INF("unable to start dw9786_init thread/n");
		err = PTR_ERR(dw9786_init_task);
		dw9786_init_task = NULL;
		dw9786_init(dw9786);
		return 0;
	}
	wake_up_process(dw9786_init_task);

	LOG_INF("-\n");

	return 0;
}

static int dw9786_set_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct dw9786_device *dw9786 = to_dw9786_vcm(ctrl);

	if (ctrl->id == V4L2_CID_FOCUS_ABSOLUTE) {
		LOG_INF("pos(%d)\n", ctrl->val);
		ret = dw9786_set_position(dw9786, ctrl->val);
		if (ret < 0) {
			LOG_INF("%s I2C failure: %d",
				__func__, ret);
			return ret;
		}
		g_last_pos = ctrl->val;
	}
	return 0;
}

static const struct v4l2_ctrl_ops dw9786_vcm_ctrl_ops = {
	.s_ctrl = dw9786_set_ctrl,
};

static int dw9786_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	int ret;
	struct dw9786_device *dw9786 = sd_to_dw9786_vcm(sd);

	ret = dw9786_power_on(dw9786);
	if (ret < 0) {
		LOG_INF("power on fail, ret = %d\n", ret);
		return ret;
	}

	return 0;
}

static int dw9786_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct dw9786_device *dw9786 = sd_to_dw9786_vcm(sd);

	dw9786_power_off(dw9786);

	return 0;
}

static int dw9786_vcm_suspend(struct dw9786_device *dw9786)
{
	LOG_INF("+\n");
	if (dw9786->is_suspending) {
		return 0;
	}

	mutex_lock(&dw9786_suspend_mutex);
	LOG_INF("dw9786_suspend_mutex: %p", &dw9786_suspend_mutex);
	dw9786->is_suspending = true;

	if (!dw9786->active) {
		dw9786->is_suspending = false;
		mutex_unlock(&dw9786_suspend_mutex);
		return 0;
	}

	/* DW9786 suspend workqueue */
	if (dw9786_suspend_wq == NULL) {
		LOG_INF("create_singlethread_workqueue\n");
		dw9786_suspend_wq = create_singlethread_workqueue("dw9786_suspend_work");
		if (!dw9786_suspend_wq) {
			LOG_INF("create_singlethread_workqueue fail\n");
			dw9786->is_suspending = false;
			mutex_unlock(&dw9786_suspend_mutex);
			return -ENOMEM;
		}
	} else {
		LOG_INF("flush work queue\n");
		flush_work(&dw9786->suspend_work);    /* flush work queue */
	}

	atomic_set(&g_is_need_standby, 1);
	queue_work(dw9786_suspend_wq, &dw9786->suspend_work);
	mutex_unlock(&dw9786_suspend_mutex);
	LOG_INF("-\n");

	return 0;
}

static void dw9786_vcm_suspend_work_fun(struct work_struct *work)
{
	struct dw9786_device *dw9786 = container_of(work, struct dw9786_device, suspend_work);
	int ret, val;
	int diff_dac = 0;
	int nStep_count = 0;
	int i = 0;
	uint16_t status;
	struct i2c_client *client = v4l2_get_subdevdata(&dw9786->sd);
	LOG_INF("+\n");
	uint16_t ois_status;

	if (!dw9786) {
		LOG_INF("dw9786 device is NULL, return");
		return;
	}

	if (!dw9786->active) {
		dw9786->is_suspending = false;
		return;
	}

	adaptor_i2c_rd_u16(client, client->addr, 0xB0B2, &ois_status);
	if (ois_status == 0x8001) {
		dw9786->active = false;
		dw9786->is_suspending = false;
		LOG_INF("ois has been used movie, return 0");
		return;
	}

	// delay power off
	dw9786->stop_wait_suspend_flag = false;
	if (atomic_read(&g_is_need_standby) == 0) {
		LOG_INF("delay power off stop\n");
		dw9786->is_suspending = false;
		return;
	}
	LOG_INF("delay power off start\n");
	ret = wait_event_interruptible_timeout(dw9786->wait_suspend_queue_head,
			dw9786->stop_wait_suspend_flag, msecs_to_jiffies(2000));
	if (ret > 0) {
		LOG_INF("interrupting, ret = %d\n", ret);
		dw9786->is_suspending = false;
		return;
	} else {
		// no interruptions, continue to power off
		LOG_INF("delay power off end\n");
	}

	if (!dw9786->active) {
		dw9786->is_suspending = false;
		return;
	}

	diff_dac = DW9786_ORIGIN_FOCUS_POS - dw9786->focus->val;
	nStep_count = (diff_dac < 0 ? (diff_dac*(-1)) : diff_dac) /
		DW9786_MOVE_STEPS;
	val = dw9786->focus->val;
	for (i = 0; i < nStep_count; ++i) {
		val += (diff_dac < 0 ? (DW9786_MOVE_STEPS*(-1)) : DW9786_MOVE_STEPS);

		ret = dw9786_set_position(dw9786, val);
		if (ret < 0) {
			LOG_INF("%s I2C failure: %d", __func__, ret);
			dw9786->is_suspending = false;
			return;
		}
		usleep_range(DW9786_CTRL_DELAY_US, DW9786_CTRL_DELAY_US + 10);
	}

	// last step to origin
	ret = dw9786_set_position(dw9786, DW9786_ORIGIN_FOCUS_POS);
	if (ret < 0) {
		LOG_INF("%s I2C failure: %d", __func__, ret);
		dw9786->is_suspending = false;
		return;
	}
	ret = adaptor_i2c_rd_u16(client, client->addr, 0xE000, &status);
	if (!(ret < 0) && (status != 0)) {
		LOG_INF("OIS step power off");
		adaptor_i2c_wr_u16(client, client->addr, 0xB026, 0x0001);    /* active mode */
		mdelay(1);
		adaptor_i2c_wr_u16(client, client->addr, 0xB022, 0x0002);    /* ois servo on */
		mdelay(1);
		adaptor_i2c_wr_u16(client, client->addr, 0xB024, 0x0002);    /* af servo on */
		mdelay(5);
		adaptor_i2c_wr_u16(client, client->addr, 0xB100, -5000);
		adaptor_i2c_wr_u16(client, client->addr, 0xB200, 5000);
		mdelay(15);
		adaptor_i2c_wr_u16(client, client->addr, 0xB100, -6000);
		adaptor_i2c_wr_u16(client, client->addr, 0xB200, 6000);
		mdelay(15);
	}
	ret = adaptor_i2c_wr_u16(client, client->addr, 0xE000, 0x0000);
	if (ret < 0) {
		LOG_INF("I2C failure!!!\n");
	} else {
		dw9786->active = false;
		LOG_INF("enter stand by mode\n");
	}
	dw9786->is_suspending = false;
	LOG_INF("-\n");
	return;
}

static int dw9786_vcm_resume_thread_func(void* data)
{
	int ret, diff_dac, nStep_count, i;
	int val = DW9786_ORIGIN_FOCUS_POS;
	struct dw9786_device *dw9786 = (struct dw9786_device*)data;
	struct i2c_client *client = v4l2_get_subdevdata(&dw9786->sd);
	uint16_t af_status = 0;
	LOG_INF("+\n");
	mutex_lock(&dw9786_suspend_mutex);
	stop_background_works(dw9786);
	LOG_INF("dw9786_suspend_mutex: %p", &dw9786_suspend_mutex);

	adaptor_i2c_rd_u16(client, client->addr, 0xB020, &af_status);
	LOG_INF("af status: 0x%04X", af_status);
	if ((af_status == 0x1022 || af_status == 0x1021) && dw9786->active) {
		LOG_INF("af has been enabled, return 0");
		mutex_unlock(&dw9786_suspend_mutex);
		return 0;
	}

	ret = dw9786_init(dw9786);
	if (ret < 0) {
		LOG_INF("I2C failure!!!\n");
		mutex_unlock(&dw9786_suspend_mutex);
		return ret;
	}
	usleep_range(DW9786_CTRL_DELAY_US, DW9786_CTRL_DELAY_US + 10);
	diff_dac = dw9786->focus->val - DW9786_ORIGIN_FOCUS_POS;
	nStep_count = (diff_dac < 0 ? (diff_dac*(-1)) : diff_dac) /
			DW9786_MOVE_STEPS;
	for (i = 0; i < nStep_count; ++i) {
		val += (diff_dac < 0 ? (DW9786_MOVE_STEPS*(-1)) : DW9786_MOVE_STEPS);
		ret = dw9786_set_position(dw9786, val);
		if (ret < 0) {
			LOG_INF("%s I2C failure: %d", __func__, ret);
			mutex_unlock(&dw9786_suspend_mutex);
			return ret;
		}
		usleep_range(DW9786_CTRL_DELAY_US, DW9786_CTRL_DELAY_US + 10);
	}
	ret = dw9786_set_position(dw9786, dw9786->focus->val);
	if (ret < 0) {
		LOG_INF("%s I2C failure: %d", __func__, ret);
		mutex_unlock(&dw9786_suspend_mutex);
		return ret;
	}
	dw9786->active = true;
	mutex_unlock(&dw9786_suspend_mutex);
	LOG_INF("-\n");

	return ret;
}

static int dw9786_vcm_resume(struct dw9786_device *dw9786)
{
	int err;

	struct task_struct *dw9786_vcm_resume_task = kthread_create(dw9786_vcm_resume_thread_func, dw9786, "dw9786_vcm_resume_kthread");
	if (IS_ERR(dw9786_vcm_resume_task)) {
		LOG_INF("unable to start dw9786_dw9786_vcm_resume thread/n");
		err = PTR_ERR(dw9786_vcm_resume_task);
		dw9786_vcm_resume_task = NULL;
		dw9786_vcm_resume_thread_func(dw9786);
		return 0;
	}
	wake_up_process(dw9786_vcm_resume_task);
	return 0;
}

static long dw9786_ops_core_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct dw9786_device *dw9786 = sd_to_dw9786_vcm(sd);
	struct i2c_client *client = v4l2_get_subdevdata(&dw9786->sd);

	client->addr = DW9786_I2C_SLAVE_ADDR >> 1;

	switch (cmd) {
	case VCM_IOC_POWER_ON:
		ret = dw9786_vcm_resume(dw9786);
		LOG_INF("VCM_IOC_POWER_ON, cmd:%d\n", cmd);
		break;
	case VCM_IOC_POWER_OFF:
		ret = dw9786_vcm_suspend(dw9786);
		LOG_INF("VCM_IOC_POWER_OFF, cmd:%d\n", cmd);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}
	return ret;
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
	v4l2_async_unregister_subdev(&dw9786->sd);
	v4l2_ctrl_handler_free(&dw9786->ctrls);
#if IS_ENABLED(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&dw9786->sd.entity);
#endif
}

static int dw9786_init_controls(struct dw9786_device *dw9786)
{
	struct v4l2_ctrl_handler *hdl = &dw9786->ctrls;
	const struct v4l2_ctrl_ops *ops = &dw9786_vcm_ctrl_ops;

	v4l2_ctrl_handler_init(hdl, 1);

	dw9786->focus = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FOCUS_ABSOLUTE,
			  0, DW9786_MAX_FOCUS_POS, DW9786_FOCUS_STEPS, 0);

	if (hdl->error)
		return hdl->error;

	dw9786->sd.ctrl_handler = hdl;

	return 0;
}

static int dw9786_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct dw9786_device *dw9786;
	int ret;

	LOG_INF("+\n");

	dw9786 = devm_kzalloc(dev, sizeof(*dw9786), GFP_KERNEL);
	if (!dw9786)
		return -ENOMEM;

	init_waitqueue_head(&dw9786->wait_suspend_queue_head);
	LOG_INF("init_wait_suspend_queue_head done\n");

	dw9786->afvdd = devm_regulator_get(dev, "afvdd");
	if (IS_ERR(dw9786->afvdd)) {
		ret = PTR_ERR(dw9786->afvdd);
		if (ret != -EPROBE_DEFER)
			LOG_INF("cannot get afvdd regulator\n");
		return ret;
	}

	dw9786->oisvdd = devm_regulator_get(dev, "oisvdd");
	if (IS_ERR(dw9786->oisvdd)) {
		ret = PTR_ERR(dw9786->oisvdd);
		if (ret != -EPROBE_DEFER)
			LOG_INF("cannot get oisvdd regulator\n");
		return ret;
	}

	dw9786->iovdd = devm_regulator_get(dev, "iovdd");
	if (IS_ERR(dw9786->iovdd)) {
		ret = PTR_ERR(dw9786->iovdd);
		if (ret != -EPROBE_DEFER)
			LOG_INF("cannot get iovdd regulator\n");
		return ret;
	}

	INIT_WORK(&dw9786->suspend_work, dw9786_vcm_suspend_work_fun);

	v4l2_i2c_subdev_init(&dw9786->sd, client, &dw9786_ops);
	dw9786->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	dw9786->sd.internal_ops = &dw9786_int_ops;

	ret = dw9786_init_controls(dw9786);
	if (ret)
		goto err_cleanup;

#if IS_ENABLED(CONFIG_MEDIA_CONTROLLER)
	ret = media_entity_pads_init(&dw9786->sd.entity, 0, NULL);
	if (ret < 0)
		goto err_cleanup;

	dw9786->sd.entity.function = MEDIA_ENT_F_LENS;
#endif

	ret = v4l2_async_register_subdev(&dw9786->sd);
	if (ret < 0)
		goto err_cleanup;

	dw9786->is_suspending = false;
	dw9786->client = NULL;
	LOG_INF("-\n");

	return 0;

err_cleanup:
	dw9786_subdev_cleanup(dw9786);
	return ret;
}

static void dw9786_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct dw9786_device *dw9786 = sd_to_dw9786_vcm(sd);

	LOG_INF("+\n");

	dw9786_subdev_cleanup(dw9786);

	LOG_INF("-\n");
}

static const struct i2c_device_id dw9786_id_table[] = {
	{ DW9786_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, dw9786_id_table);

static const struct of_device_id dw9786_of_table[] = {
	{ .compatible = "oplus,dw9786" },
	{ },
};
MODULE_DEVICE_TABLE(of, dw9786_of_table);

static struct i2c_driver dw9786_i2c_driver = {
	.driver = {
		.name = DW9786_NAME,
		.of_match_table = dw9786_of_table,
	},
	.probe  = dw9786_probe,
	.remove = dw9786_remove,
	.id_table = dw9786_id_table,
};

module_i2c_driver(dw9786_i2c_driver);

MODULE_AUTHOR("XXX");
MODULE_DESCRIPTION("DW9786 VCM driver");
MODULE_LICENSE("GPL v2");
