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
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include "ak7316t.h"
#define DRIVER_NAME "ak7316t"
#define LOG_INF(format, args...)                                               \
	pr_info(DRIVER_NAME " [%s] " format, __func__, ##args)

#define AK7316T_NAME				"ak7316t"
#define AK7316T_MAX_FOCUS_POS			1023
#define AK7316T_ORIGIN_FOCUS_POS		0
#define AK7316T_I2C_SLAVE_ADDR			0x5A
#define AK7316T_PID_VER_ADDR			0x16
#define AK7316T_DATA_CHECK_ADDR			0x4B
#define AK7316T_DATA_CHECK_BIT			0x04
#define AK7316T_STORE_ADDR			0x03
#define AK7316T_WRITE_CONTROL_ADDR		0xAE
#define PID_VERSION				0x10
/*
 * This sets the minimum granularity for the focus positions.
 * A value of 1 gives maximum accuracy for a desired focus position
 */
#define AK7316T_FOCUS_STEPS			1
#define AK7316T_SET_POSITION_ADDR		0x00

#define AK7316T_CMD_DELAY			0xff
#define AK7316T_CTRL_DELAY_US		5000
/*
 * This acts as the minimum granularity of lens movement.
 * Keep this value power of 2, so the control steps can be
 * uniformly adjusted for gradual lens movement, with desired
 * number of control steps.
 */
#define AK7316T_MOVE_STEPS			100
#define AK7316T_MOVE_DELAY_US		1000

#define AK7316T_VIN_MIN			(3100000)
#define AK7316T_VIN_MAX			(3100000)
#define AK7316T_CMD_DELAY_US	(5000)

static u16 g_origin_pos = AK7316T_MAX_FOCUS_POS / 2;
static u16 g_last_pos = AK7316T_MAX_FOCUS_POS / 2;

/* ak7316t device structure */
struct ak7316t_device {
	struct v4l2_ctrl_handler ctrls;
	struct v4l2_subdev sd;
	struct v4l2_ctrl *focus;
	struct regulator *vin;
	struct regulator *vdd;
	struct pinctrl *vcamaf_pinctrl;
	struct pinctrl_state *vcamaf_on;
	struct pinctrl_state *vcamaf_off;
	/* active or standby mode */
	bool active;
};

#define VCM_IOC_POWER_ON         _IO('V', BASE_VIDIOC_PRIVATE + 4)
#define VCM_IOC_POWER_OFF        _IO('V', BASE_VIDIOC_PRIVATE + 5)

static inline struct ak7316t_device *to_ak7316t_vcm(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct ak7316t_device, ctrls);
}

static inline struct ak7316t_device *sd_to_ak7316t_vcm(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct ak7316t_device, sd);
}

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};


static int ak7316t_set_position(struct ak7316t_device *ak7316t, u16 val)
{
	int ret;
	struct i2c_client *client = v4l2_get_subdevdata(&ak7316t->sd);
	ret = i2c_smbus_write_word_data(client, AK7316T_SET_POSITION_ADDR,
					 swab16(val << 6));
	return ret;
}

static int ak7316t_goto_last_pos(struct ak7316t_device *ak7316t)
{
	int ret, val = g_origin_pos, diff_dac, nStep_count, i;
	diff_dac = g_last_pos - val;
	LOG_INF("+\n");
	if (diff_dac == 0) {
		return 0;
	}
	nStep_count = (diff_dac < 0 ? (diff_dac*(-1)) : diff_dac) /
			AK7316T_MOVE_STEPS;
	for (i = 0; i < nStep_count; ++i) {
		val += (diff_dac < 0 ? (AK7316T_MOVE_STEPS*(-1)) : AK7316T_MOVE_STEPS);
		ret = ak7316t_set_position(ak7316t, val);
		if (ret) {
			LOG_INF("%s I2C failure: %d", __func__, ret);
		}
		usleep_range(AK7316T_MOVE_DELAY_US, AK7316T_MOVE_DELAY_US + 10);
	}
	LOG_INF("-\n");
	return 0;
}

static int ak7316t_release(struct ak7316t_device *ak7316t)
{
	int ret, val;
	int diff_dac = 0;
	int nStep_count = 0;
	int i = 0;
	struct i2c_client *client = v4l2_get_subdevdata(&ak7316t->sd);
	LOG_INF("+\n");
	diff_dac = AK7316T_ORIGIN_FOCUS_POS - ak7316t->focus->val;
	nStep_count = (diff_dac < 0 ? (diff_dac*(-1)) : diff_dac) /
		AK7316T_MOVE_STEPS;
	val = ak7316t->focus->val;
	for (i = 0; i < nStep_count; ++i) {
		val += (diff_dac < 0 ? (AK7316T_MOVE_STEPS*(-1)) : AK7316T_MOVE_STEPS);
		ret = ak7316t_set_position(ak7316t, val);
		if (ret) {
			LOG_INF("%s I2C failure: %d", __func__, ret);
			return ret;
		}
		usleep_range(AK7316T_MOVE_DELAY_US, AK7316T_MOVE_DELAY_US + 10);
	}
	// last step to origin
	ret = ak7316t_set_position(ak7316t, AK7316T_ORIGIN_FOCUS_POS);
	if (ret) {
		LOG_INF("%s I2C failure: %d", __func__, ret);
		return ret;
	}

	i2c_smbus_write_byte_data(client, 0x02, 0x20);
	ak7316t->active = false;
	LOG_INF("-\n");
	return 0;
}

// static int ak7316t_dac_control(struct ak7316t_device *ak7316t)
// {
// 	struct i2c_client *client = v4l2_get_subdevdata(&ak7316t->sd);
// 	int ret;
// 	LOG_INF("+\n");
// 	ret = i2c_smbus_write_byte_data(client, 0xAE, 0x3B); // Unlock
// 	ret = i2c_smbus_write_byte_data(client, 0xA6, 0x7B); // Enter to DAC mode
// 	ret = i2c_smbus_write_byte_data(client, 0x00, 0x28); // set DAC
// 	i2c_smbus_write_byte_data(client, 0x02, 0x00);
// 	msleep(30);
// 	ret = i2c_smbus_write_byte_data(client, 0x02, 0x40); // Enter to Standy mode
// 	usleep_range(100, 150); // wait over 0.1ms
// 	ret = i2c_smbus_write_byte_data(client, 0xA6, 0x00); // leave from DAC mode
// 	usleep_range(100, 150); // wait over 0.1ms
// 	ret = i2c_smbus_write_byte_data(client, 0xAE, 0x00); // lock
// 	usleep_range(100, 150); // wait over 0.1ms
// 	ret = i2c_smbus_write_byte_data(client, 0x02, 0x00); // Enter to active mode
// 	usleep_range(100, 150); // wait over 0.1ms
// 	ret = i2c_smbus_write_byte_data(client, 0x02, 0x40); // Enter to Standy mode
// 	usleep_range(2800, 3000); // wait over 2.71ms
// 	LOG_INF("-\n");
// 	return 0;
// }


static int ak7316t_init(struct ak7316t_device *ak7316t)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ak7316t->sd);
	int ret = 0, val = 0;
	LOG_INF("+\n");
	client->addr = AK7316T_I2C_SLAVE_ADDR >> 1;
	ret = i2c_smbus_write_byte_data(client, 0x02, 0x40); // standy
	usleep_range(2800, 3000); // wait over 2.71ms
	// ak7316t_dac_control(ak7316t);
	val = i2c_smbus_read_byte_data(client, AK7316T_PID_VER_ADDR);
	if (val != PID_VERSION) {
		LOG_INF("please update pid");
	}
	ret = i2c_smbus_read_word_data(client, 0x84);
	if (ret < 0) {
		LOG_INF("%s I2C failure: %d", __func__, ret);
		g_origin_pos = AK7316T_MAX_FOCUS_POS / 2;
	} else {
		g_origin_pos = swab16(ret & 0xFFFF) >> 6;
	}
	if (g_origin_pos > AK7316T_MAX_FOCUS_POS) {
		g_origin_pos = AK7316T_MAX_FOCUS_POS;
	}
	ak7316t_set_position(ak7316t, g_origin_pos);

	/* 00:active mode , 10:Standby mode , x1:Sleep mode */
	ret = i2c_smbus_write_byte_data(client, 0x02, 0x00);
	ak7316t->active = true;
	ak7316t_goto_last_pos(ak7316t);

	LOG_INF("-\n");

	return 0;
}

/* Power handling */
static int ak7316t_power_off(struct ak7316t_device *ak7316t)
{
	int ret;

	LOG_INF("+\n");

	ret = ak7316t_release(ak7316t);
	if (ret)
		LOG_INF("ak7316t release failed!\n");

	ret = regulator_disable(ak7316t->vin);
	if (ret)
		return ret;

	if (ak7316t->vcamaf_pinctrl && ak7316t->vcamaf_off)
		ret = pinctrl_select_state(ak7316t->vcamaf_pinctrl,
					ak7316t->vcamaf_off);

	LOG_INF("-\n");

	return ret;
}

static int ak7316t_power_on(struct ak7316t_device *ak7316t)
{
	int ret, min, max;

	LOG_INF("+\n");

	min = AK7316T_VIN_MIN;
	max = AK7316T_VIN_MAX;
	ret = regulator_set_voltage(ak7316t->vin, min, max);
	if (ret) {
		LOG_INF("regulator_set_voltage(vin), min(%d - %d), ret(%d)(fail)\n",
			min, max, ret);
	}
	ret = regulator_enable(ak7316t->vin);
	if (ret < 0) {
		LOG_INF("regulator_enable(vin), ret(%d)(fail)\n", ret);
		return ret;
	}

	if (ak7316t->vcamaf_pinctrl && ak7316t->vcamaf_on)
		ret = pinctrl_select_state(ak7316t->vcamaf_pinctrl,
					ak7316t->vcamaf_on);

	if (ret < 0)
		return ret;

	/*
	 * TODO(b/139784289): Confirm hardware requirements and adjust/remove
	 * the delay.
	 */
	usleep_range(AK7316T_CTRL_DELAY_US + 100, AK7316T_CTRL_DELAY_US + 200);

	ret = ak7316t_init(ak7316t);
	if (ret < 0)
		goto fail;

	LOG_INF("-\n");

	return 0;

fail:
	regulator_disable(ak7316t->vin);
	if (ak7316t->vcamaf_pinctrl && ak7316t->vcamaf_off) {
		pinctrl_select_state(ak7316t->vcamaf_pinctrl,
				ak7316t->vcamaf_off);
	}

	return ret;
}

static int ak7316t_set_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct ak7316t_device *ak7316t = to_ak7316t_vcm(ctrl);

	if (ctrl->id == V4L2_CID_FOCUS_ABSOLUTE) {
		LOG_INF("pos(%d)\n", ctrl->val);
		ret = ak7316t_set_position(ak7316t, ctrl->val);
		if (ret) {
			LOG_INF("%s I2C failure: %d",
				__func__, ret);
			return ret;
		}
		g_last_pos = ctrl->val;
	}

	return 0;
}

static const struct v4l2_ctrl_ops ak7316t_vcm_ctrl_ops = {
	.s_ctrl = ak7316t_set_ctrl,
};

static int ak7316t_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	int ret;
	struct ak7316t_device *ak7316t = sd_to_ak7316t_vcm(sd);

	ret = ak7316t_power_on(ak7316t);
	if (ret < 0) {
		LOG_INF("power on fail, ret = %d\n", ret);
		return ret;
	}

	return 0;
}

static int ak7316t_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct ak7316t_device *ak7316t = sd_to_ak7316t_vcm(sd);

	ak7316t_power_off(ak7316t);
	return 0;
}

static int ak7316t_vcm_suspend(struct ak7316t_device *ak7316t)
{
	int ret, val;
	int diff_dac = 0;
	int nStep_count = 0;
	int i = 0;
	struct i2c_client *client = v4l2_get_subdevdata(&ak7316t->sd);
	LOG_INF("+\n");
	if (!ak7316t->active)
		return 0;

	diff_dac = AK7316T_ORIGIN_FOCUS_POS - ak7316t->focus->val;
	nStep_count = (diff_dac < 0 ? (diff_dac*(-1)) : diff_dac) /
		AK7316T_MOVE_STEPS;
	val = ak7316t->focus->val;
	for (i = 0; i < nStep_count; ++i) {
		val += (diff_dac < 0 ? (AK7316T_MOVE_STEPS*(-1)) : AK7316T_MOVE_STEPS);
		ret = ak7316t_set_position(ak7316t, val);
		if (ret) {
			LOG_INF("%s I2C failure: %d", __func__, ret);
			return ret;
		}
		usleep_range(AK7316T_MOVE_DELAY_US, AK7316T_MOVE_DELAY_US + 10);
	}
	ret = ak7316t_set_position(ak7316t, AK7316T_ORIGIN_FOCUS_POS);
	if (ret) {
		LOG_INF("%s I2C failure: %d", __func__, ret);
		return ret;
	}
	ret = i2c_smbus_write_byte_data(client, 0x02, 0x40);
	if (ret) {
		LOG_INF("I2C failure!!!\n");
	} else {
		ak7316t->active = false;
		LOG_INF("enter stand by mode\n");
	}
	LOG_INF("-\n");
	return ret;
}

static int ak7316t_vcm_resume(struct ak7316t_device *ak7316t)
{
	int ret, val;
	struct i2c_client *client = v4l2_get_subdevdata(&ak7316t->sd);
	if (ak7316t->active)
		return 0;
	LOG_INF("+\n");
	ret = i2c_smbus_write_byte_data(client, 0x02, 0x00);
	if (ret) {
		LOG_INF("I2C failure!!!\n");
		return ret;
	}

	for (val = ak7316t->focus->val % AK7316T_MOVE_STEPS; val <= ak7316t->focus->val;
		val += AK7316T_MOVE_STEPS) {
		ret = ak7316t_set_position(ak7316t, val);
		if (ret)
			LOG_INF("%s I2C failure: %d", __func__, ret);
		usleep_range(AK7316T_MOVE_DELAY_US, AK7316T_MOVE_DELAY_US + 10);
	}
	ak7316t->active = true;
	LOG_INF("exit stand by mode\n");
	return ret;
}

static long ak7316t_ops_core_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct ak7316t_device *ak7316t = sd_to_ak7316t_vcm(sd);
	struct i2c_client *client = v4l2_get_subdevdata(&ak7316t->sd);
	LOG_INF("+\n");
	client->addr = AK7316T_I2C_SLAVE_ADDR >> 1;

	switch (cmd) {
	case VCM_IOC_POWER_ON:
		ret = ak7316t_vcm_resume(ak7316t);
		break;
	case VCM_IOC_POWER_OFF:
		ret = ak7316t_vcm_suspend(ak7316t);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}
	return ret;
}

static const struct v4l2_subdev_internal_ops ak7316t_int_ops = {
	.open = ak7316t_open,
	.close = ak7316t_close,
};

static struct v4l2_subdev_core_ops ak7316t_ops_core = {
	.ioctl = ak7316t_ops_core_ioctl,
};

static const struct v4l2_subdev_ops ak7316t_ops = {
	.core = &ak7316t_ops_core,
 };

static void ak7316t_subdev_cleanup(struct ak7316t_device *ak7316t)
{
	v4l2_async_unregister_subdev(&ak7316t->sd);
	v4l2_ctrl_handler_free(&ak7316t->ctrls);
#if IS_ENABLED(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&ak7316t->sd.entity);
#endif
}

static int ak7316t_init_controls(struct ak7316t_device *ak7316t)
{
	struct v4l2_ctrl_handler *hdl = &ak7316t->ctrls;
	const struct v4l2_ctrl_ops *ops = &ak7316t_vcm_ctrl_ops;

	v4l2_ctrl_handler_init(hdl, 1);

	ak7316t->focus = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FOCUS_ABSOLUTE,
			  0, AK7316T_MAX_FOCUS_POS, AK7316T_FOCUS_STEPS, 0);

	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FOCUS_AUTO, 0, 0, 0, 0);

	if (hdl->error)
		return hdl->error;

	ak7316t->sd.ctrl_handler = hdl;

	return 0;
}

static int ak7316t_power_on2(struct ak7316t_device *ak7316t)
{
	int ret, min, max;

	LOG_INF("+");

	min = AK7316T_VIN_MIN;
	max = AK7316T_VIN_MAX;
	ret = regulator_set_voltage(ak7316t->vin, min, max);
	if (ret) {
		LOG_INF("regulator_set_voltage(vin), min(%d - %d), ret(%d)(fail)\n",
			min, max, ret);
	}
	ret = regulator_enable(ak7316t->vin);
	if (ret < 0) {
		LOG_INF("regulator_enable(vin), ret(%d)(fail)\n", ret);
		goto fail2;
	}
	LOG_INF("-\n");
	return ret;

fail2:
	regulator_disable(ak7316t->vin);
	return ret;
}

static int ak7316t_power_off2(struct ak7316t_device *ak7316t)
{
	int ret;
	LOG_INF("+\n");
	ret = regulator_disable(ak7316t->vin);
	LOG_INF("-\n");
	return ret;
}

static int ak7316t_set_setting(struct ak7316t_device *ak7316t)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ak7316t->sd);
	int ret = 0, val =0;
	int i = 0;
	bool is_update_pid = false;
	u8 pid_setting_len =  ARRAY_SIZE(ak7316t_pid_setting);
	client->addr = AK7316T_I2C_SLAVE_ADDR >> 1;
	LOG_INF("+\n");
	//power on
	ret = ak7316t_power_on2(ak7316t);
	if (ret) {
		LOG_INF("%s:%d power on failure: %d", __func__, __LINE__, ret);
		return ret;
	}
	usleep_range(5100, 5200); // wait over 2.71ms

	//enter standby
	ret = i2c_smbus_write_byte_data(client, 0x02, 0x40); // standy
	if (ret < 0) {
		LOG_INF("%s:%d I2C failure: %d", __func__, __LINE__, ret);
		goto out;
	}
	usleep_range(2800, 2900); // wait over 2.71ms

	// check pid
	val = i2c_smbus_read_byte_data(client, AK7316T_PID_VER_ADDR);
	if (val != PID_VERSION) {
		is_update_pid = true;
	}
	//update pid
	if (is_update_pid) {
		LOG_INF("ak7316t update pid ing...");
		//change to setting mode
		ret = i2c_smbus_write_byte_data(client, AK7316T_WRITE_CONTROL_ADDR, 0x3B);

		//write pid setting
		for (i = 0; i < pid_setting_len; i = i + 2) {
			ret = i2c_smbus_write_byte_data(client, ak7316t_pid_setting[i], ak7316t_pid_setting[i + 1]);
			if (ret < 0) {
				LOG_INF("%s:%d I2C failure: %d", __func__, __LINE__, ret);
				goto out;
			}
		}
		ret = i2c_smbus_write_byte_data(client, AK7316T_STORE_ADDR, 0x01);
		if (ret < 0) {
			LOG_INF("%s:%d I2C failure: %d", __func__, __LINE__, ret);
			goto out;
		}
		msleep(110);
		ret = i2c_smbus_write_byte_data(client, AK7316T_STORE_ADDR, 0x02);
		if (ret < 0) {
			LOG_INF("%s:%d I2C failure: %d", __func__, __LINE__, ret);
			goto out;
		}
		msleep(190);
		ret = i2c_smbus_write_byte_data(client, AK7316T_STORE_ADDR, 0x04);
		if (ret < 0) {
			LOG_INF("%s:%d I2C failure: %d", __func__, __LINE__, ret);
			goto out;
		}
		msleep(160);
		ret = i2c_smbus_write_byte_data(client, AK7316T_STORE_ADDR, 0x08);
		if (ret < 0) {
			LOG_INF("%s:%d I2C failure: %d", __func__, __LINE__, ret);
			goto out;
		}
		msleep(170);

		//check store
		ret = i2c_smbus_read_byte_data(client, AK7316T_DATA_CHECK_ADDR);
		if (ret < 0 || (ret & AK7316T_DATA_CHECK_BIT) ) {
			LOG_INF("%s:%d I2C failure: %d", __func__, __LINE__, ret);
			goto out;
		}

		//release setting mode
		ret = i2c_smbus_write_byte_data(client, AK7316T_WRITE_CONTROL_ADDR, 0x00);
		if (ret < 0) {
			LOG_INF("%s:%d I2C failure: %d", __func__, __LINE__, ret);
			goto out;
		}

		// check pid
		val = i2c_smbus_read_byte_data(client, AK7316T_PID_VER_ADDR);
		if (val != PID_VERSION) {
			LOG_INF("failed to update pid");
		} else {
			LOG_INF("update pid successfully");
		}
	}
	LOG_INF("-");
out:
	ak7316t_power_off2(ak7316t);
	return ret;
}

static int ak7316t_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct ak7316t_device *ak7316t;
	int ret;

	LOG_INF("+\n");

	ak7316t = devm_kzalloc(dev, sizeof(*ak7316t), GFP_KERNEL);
	if (!ak7316t)
		return -ENOMEM;

	ak7316t->vin = devm_regulator_get(dev, "vin");
	if (IS_ERR(ak7316t->vin)) {
		ret = PTR_ERR(ak7316t->vin);
		if (ret != -EPROBE_DEFER)
			LOG_INF("cannot get vin regulator\n");
		return ret;
	}

	ak7316t->vcamaf_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(ak7316t->vcamaf_pinctrl)) {
		ret = PTR_ERR(ak7316t->vcamaf_pinctrl);
		ak7316t->vcamaf_pinctrl = NULL;
		LOG_INF("cannot get pinctrl\n");
	} else {
		ak7316t->vcamaf_on = pinctrl_lookup_state(
			ak7316t->vcamaf_pinctrl, "vcamaf_on");

		if (IS_ERR(ak7316t->vcamaf_on)) {
			ret = PTR_ERR(ak7316t->vcamaf_on);
			ak7316t->vcamaf_on = NULL;
			LOG_INF("cannot get vcamaf_on pinctrl\n");
		}

		ak7316t->vcamaf_off = pinctrl_lookup_state(
			ak7316t->vcamaf_pinctrl, "vcamaf_off");

		if (IS_ERR(ak7316t->vcamaf_off)) {
			ret = PTR_ERR(ak7316t->vcamaf_off);
			ak7316t->vcamaf_off = NULL;
			LOG_INF("cannot get vcamaf_off pinctrl\n");
		}
	}

	v4l2_i2c_subdev_init(&ak7316t->sd, client, &ak7316t_ops);
	ak7316t->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	ak7316t->sd.internal_ops = &ak7316t_int_ops;

	ret = ak7316t_init_controls(ak7316t);
	if (ret)
		goto err_cleanup;

#if IS_ENABLED(CONFIG_MEDIA_CONTROLLER)
	ret = media_entity_pads_init(&ak7316t->sd.entity, 0, NULL);
	if (ret < 0)
		goto err_cleanup;

	ak7316t->sd.entity.function = MEDIA_ENT_F_LENS;
#endif

	ret = v4l2_async_register_subdev(&ak7316t->sd);
	if (ret < 0)
		goto err_cleanup;

	if (ak7316t_set_setting(ak7316t)) {
		LOG_INF("ak7316t_set_setting failed");
	}
	LOG_INF("-\n");

	return 0;

err_cleanup:
	ak7316t_subdev_cleanup(ak7316t);
	return ret;
}

static void ak7316t_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ak7316t_device *ak7316t = sd_to_ak7316t_vcm(sd);

	LOG_INF("+\n");

	ak7316t_subdev_cleanup(ak7316t);

	LOG_INF("-\n");
}

static const struct i2c_device_id ak7316t_id_table[] = {
	{ AK7316T_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, ak7316t_id_table);

static const struct of_device_id ak7316t_of_table[] = {
	{ .compatible = "oplus,ak7316t" },
	{ },
};
MODULE_DEVICE_TABLE(of, ak7316t_of_table);

static struct i2c_driver ak7316t_i2c_driver = {
	.driver = {
		.name = AK7316T_NAME,
		.of_match_table = ak7316t_of_table,
	},
	.probe  = ak7316t_probe,
	.remove = ak7316t_remove,
	.id_table = ak7316t_id_table,
};

module_i2c_driver(ak7316t_i2c_driver);

MODULE_AUTHOR("XXX");
MODULE_DESCRIPTION("AK7316T VCM driver");
MODULE_LICENSE("GPL v2");
