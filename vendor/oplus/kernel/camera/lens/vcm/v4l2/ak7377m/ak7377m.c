// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm_runtime.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include "ak7377m.h"
#define DRIVER_NAME "ak7377m"
#define AK7377M_I2C_SLAVE_ADDR 0x18

#define LOG_INF(format, args...)                                               \
	pr_info(DRIVER_NAME " [%s] " format, __func__, ##args)

#define AK7377M_NAME				"ak7377m"
#define AK7377M_MAX_FOCUS_POS			4095
#define AK7377M_ORIGIN_FOCUS_POS		0
#define AK7377M_I2C_SLAVE_ADDR			0x18
#define AK7377M_PID_VER_ADDR			0x16
#define AK7377M_DATA_CHECK_ADDR			0x4B
#define AK7377M_DATA_CHECK_BIT			0x04
#define AK7377M_STORE_ADDR			0x03
#define AK7377M_WRITE_CONTROL_ADDR		0xAE
#define AK7377M_PID_VERSION				0x64

/*
 * This sets the minimum granularity for the focus positions.
 * A value of 1 gives maximum accuracy for a desired focus position
 */
#define AK7377M_FOCUS_STEPS			1
#define AK7377M_SET_POSITION_ADDR		0x00

#define AK7377M_CMD_DELAY			0xff
#define AK7377M_CTRL_DELAY_US			10000
/*
 * This acts as the minimum granularity of lens movement.
 * Keep this value power of 2, so the control steps can be
 * uniformly adjusted for gradual lens movement, with desired
 * number of control steps.
 */
#define AK7377M_MOVE_STEPS			400
#define AK7377M_MOVE_DELAY_US			5000

#define AK7377M_VIN_MIN			(2800000)
#define AK7377M_VIN_MAX			(2800000)
/* ak7377m device structure */
struct ak7377m_device {
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

extern unsigned int get_project(void);
extern unsigned int get_PCB_Version(void);

enum PCB_VERSION {
    UNKNOWN_VERSION = 0,
    PRE_EVB1,
    PRE_EVB2,
    EVB1 = 8,
    EVB2,
    EVB3,
    EVB4,
    EVB5,
    EVB6,
    T0 = 16,
    T1,
    T2,
    T3,
    T4,
    T5,
    T6,
    EVT1 = 24,
    EVT2,
    EVT3,
    EVT4,
    EVT5,
    EVT6,
    DVT1 = 32,
    DVT2,
    DVT3,
    DVT4,
    DVT5,
    DVT6,
    PVT1 = 40,
    PVT2,
    PVT3,
    PVT4,
    PVT5,
    PVT6,
    MP1 = 48,
    MP2,
    MP3,
    MP4,
    MP5,
    MP6,
    PCB_MAX,
};

static inline struct ak7377m_device *to_ak7377m_vcm(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct ak7377m_device, ctrls);
}

static inline struct ak7377m_device *sd_to_ak7377m_vcm(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct ak7377m_device, sd);
}

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};

static int ak7377m_set_position(struct ak7377m_device *ak7377m, u16 val)
{
	int retry = 3;
	int ret;
	struct i2c_client *client = v4l2_get_subdevdata(&ak7377m->sd);

	while (--retry > 0) {
		ret = i2c_smbus_write_word_data(client, AK7377M_SET_POSITION_ADDR,
					 swab16(val << 4));
		if (ret < 0) {
			usleep_range(AK7377M_MOVE_DELAY_US,
				     AK7377M_MOVE_DELAY_US + 1000);
		} else {
			break;
		}
	}

	return ret;
}

static int ak7377m_release(struct ak7377m_device *ak7377m)
{
	int ret, val;
	int diff_dac = 0;
	int nStep_count = 0;
	int i = 0;
	struct i2c_client *client = v4l2_get_subdevdata(&ak7377m->sd);

	diff_dac = AK7377M_ORIGIN_FOCUS_POS - ak7377m->focus->val;

	nStep_count = (diff_dac < 0 ? (diff_dac*(-1)) : diff_dac) /
		AK7377M_MOVE_STEPS;

	val = ak7377m->focus->val;

	for (i = 0; i < nStep_count; ++i) {
		val += (diff_dac < 0 ? (AK7377M_MOVE_STEPS*(-1)) : AK7377M_MOVE_STEPS);

		ret = ak7377m_set_position(ak7377m, val);
		if (ret) {
			LOG_INF("%s I2C failure: %d",
				__func__, ret);
			return ret;
		}
		usleep_range(AK7377M_MOVE_DELAY_US,
			     AK7377M_MOVE_DELAY_US + 1000);
	}

	// last step to origin
	ret = ak7377m_set_position(ak7377m, AK7377M_ORIGIN_FOCUS_POS);
	if (ret) {
		LOG_INF("%s I2C failure: %d",
			__func__, ret);
		return ret;
	}

	i2c_smbus_write_byte_data(client, 0x02, 0x20);
	ak7377m->active = false;
	LOG_INF("-\n");

	return 0;
}

static int ak7377m_init(struct ak7377m_device *ak7377m)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ak7377m->sd);
	int ret = 0, val = 0;

	LOG_INF("+\n");

	client->addr = AK7377M_I2C_SLAVE_ADDR >> 1;
	//ret = i2c_smbus_read_byte_data(client, 0x02);

	val = i2c_smbus_read_byte_data(client, AK7377M_PID_VER_ADDR);
	if (val != AK7377M_PID_VERSION) {
		LOG_INF("please update pid");
	}
	LOG_INF("Check HW version: %x\n", ret);

	/* 00:active mode , 10:Standby mode , x1:Sleep mode */
	ret = i2c_smbus_write_byte_data(client, 0x02, 0x00);
	ak7377m->active = true;

	LOG_INF("-\n");

	return 0;
}

/* Power handling */
static int ak7377m_power_off(struct ak7377m_device *ak7377m)
{
	int ret;

	LOG_INF("+\n");

	ret = ak7377m_release(ak7377m);
	if (ret)
		LOG_INF("ak7377m release failed!\n");

	ret = regulator_disable(ak7377m->vin);
	if (ret)
		return ret;

	ret = regulator_disable(ak7377m->vdd);
	if (ret)
		return ret;

	if (ak7377m->vcamaf_pinctrl && ak7377m->vcamaf_off)
		ret = pinctrl_select_state(ak7377m->vcamaf_pinctrl,
					ak7377m->vcamaf_off);

	LOG_INF("-\n");

	return ret;
}

static int ak7377m_power_on(struct ak7377m_device *ak7377m)
{
	int ret;

	LOG_INF("+\n");

	ret = regulator_enable(ak7377m->vin);
	if (ret < 0)
		return ret;

	ret = regulator_enable(ak7377m->vdd);
	if (ret < 0)
		return ret;

	if (ak7377m->vcamaf_pinctrl && ak7377m->vcamaf_on)
		ret = pinctrl_select_state(ak7377m->vcamaf_pinctrl,
					ak7377m->vcamaf_on);

	if (ret < 0)
		return ret;

	/*
	 * TODO(b/139784289): Confirm hardware requirements and adjust/remove
	 * the delay.
	 */
	usleep_range(AK7377M_CTRL_DELAY_US, AK7377M_CTRL_DELAY_US + 100);

	ret = ak7377m_init(ak7377m);
	if (ret < 0)
		goto fail;

	LOG_INF("-\n");

	return 0;

fail:
	regulator_disable(ak7377m->vin);
	regulator_disable(ak7377m->vdd);
	if (ak7377m->vcamaf_pinctrl && ak7377m->vcamaf_off) {
		pinctrl_select_state(ak7377m->vcamaf_pinctrl,
				ak7377m->vcamaf_off);
	}

	return ret;
}

static int ak7377m_set_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct ak7377m_device *ak7377m = to_ak7377m_vcm(ctrl);

	if (ctrl->id == V4L2_CID_FOCUS_ABSOLUTE) {
		LOG_INF("pos(%d)\n", ctrl->val);
		ret = ak7377m_set_position(ak7377m, ctrl->val);
		if (ret) {
			LOG_INF("%s I2C failure: %d",
				__func__, ret);
			return ret;
		}
	}
	return 0;
}

static const struct v4l2_ctrl_ops ak7377m_vcm_ctrl_ops = {
	.s_ctrl = ak7377m_set_ctrl,
};

static int ak7377m_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	int ret;
	struct ak7377m_device *ak7377m = sd_to_ak7377m_vcm(sd);

	ret = ak7377m_power_on(ak7377m);
	if (ret < 0) {
		LOG_INF("power on fail, ret = %d\n", ret);
		return ret;
	}

	return 0;
}

static int ak7377m_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct ak7377m_device *ak7377m = sd_to_ak7377m_vcm(sd);

	ak7377m_power_off(ak7377m);
	return 0;
}

static int ak7377m_vcm_suspend(struct ak7377m_device *ak7377m)
{
	int ret, val;
	int diff_dac = 0;
	int nStep_count = 0;
	int i = 0;
	struct i2c_client *client = v4l2_get_subdevdata(&ak7377m->sd);

	if (!ak7377m->active)
		return 0;

	diff_dac = AK7377M_ORIGIN_FOCUS_POS - ak7377m->focus->val;
	nStep_count = (diff_dac < 0 ? (diff_dac*(-1)) : diff_dac) /
		AK7377M_MOVE_STEPS;
	val = ak7377m->focus->val;
	for (i = 0; i < nStep_count; ++i) {
		val += (diff_dac < 0 ? (AK7377M_MOVE_STEPS*(-1)) : AK7377M_MOVE_STEPS);
		ret = ak7377m_set_position(ak7377m, val);
		if (ret) {
			LOG_INF("%s I2C failure: %d", __func__, ret);
			return ret;
		}
		usleep_range(AK7377M_MOVE_DELAY_US, AK7377M_MOVE_DELAY_US + 10);
	}
	ret = ak7377m_set_position(ak7377m, AK7377M_ORIGIN_FOCUS_POS);
	if (ret) {
		LOG_INF("%s I2C failure: %d", __func__, ret);
		return ret;
	}
	ret = i2c_smbus_write_byte_data(client, 0x02, 0x40);
	if (ret) {
		LOG_INF("I2C failure!!!\n");
	} else {
		ak7377m->active = false;
		LOG_INF("enter stand by mode\n");
	}
	return ret;
}

static int ak7377m_vcm_resume(struct ak7377m_device *ak7377m)
{
	int ret, val;
	struct i2c_client *client = v4l2_get_subdevdata(&ak7377m->sd);
	if (ak7377m->active)
		return 0;
	ret = i2c_smbus_write_byte_data(client, 0x02, 0x00);
	if (ret) {
		LOG_INF("I2C failure!!!\n");
		return ret;
	}

	for (val = ak7377m->focus->val % AK7377M_MOVE_STEPS; val <= ak7377m->focus->val;
		val += AK7377M_MOVE_STEPS) {
		ret = ak7377m_set_position(ak7377m, val);
		if (ret)
			LOG_INF("%s I2C failure: %d", __func__, ret);
		usleep_range(AK7377M_MOVE_DELAY_US, AK7377M_MOVE_DELAY_US + 10);
	}
	ak7377m->active = true;
	LOG_INF("exit stand by mode\n");
	return ret;
}

static long ak7377m_ops_core_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct ak7377m_device *ak7377m = sd_to_ak7377m_vcm(sd);
	struct i2c_client *client = v4l2_get_subdevdata(&ak7377m->sd);
	LOG_INF("+\n");
	client->addr = AK7377M_I2C_SLAVE_ADDR >> 1;

	switch (cmd) {
	case VCM_IOC_POWER_ON:
		ret = ak7377m_vcm_resume(ak7377m);
		LOG_INF("VCM_IOC_POWER_ON, cmd:%d\n", cmd);
		break;
	case VCM_IOC_POWER_OFF:
		ret = ak7377m_vcm_suspend(ak7377m);
		LOG_INF("VCM_IOC_POWER_OFF, cmd:%d\n", cmd);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}
	return ret;
}

static const struct v4l2_subdev_internal_ops ak7377m_int_ops = {
	.open = ak7377m_open,
	.close = ak7377m_close,
};

static struct v4l2_subdev_core_ops ak7377m_ops_core = {
	.ioctl = ak7377m_ops_core_ioctl,
};

static const struct v4l2_subdev_ops ak7377m_ops = {
	.core = &ak7377m_ops_core,
 };

static void ak7377m_subdev_cleanup(struct ak7377m_device *ak7377m)
{
	v4l2_async_unregister_subdev(&ak7377m->sd);
	v4l2_ctrl_handler_free(&ak7377m->ctrls);
#if IS_ENABLED(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&ak7377m->sd.entity);
#endif
}

static int ak7377m_init_controls(struct ak7377m_device *ak7377m)
{
	struct v4l2_ctrl_handler *hdl = &ak7377m->ctrls;
	const struct v4l2_ctrl_ops *ops = &ak7377m_vcm_ctrl_ops;

	v4l2_ctrl_handler_init(hdl, 1);

	ak7377m->focus = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FOCUS_ABSOLUTE,
			  0, AK7377M_MAX_FOCUS_POS, AK7377M_FOCUS_STEPS, 0);

	if (hdl->error)
		return hdl->error;

	ak7377m->sd.ctrl_handler = hdl;

	return 0;
}

static int ak7377m_power_on2(struct ak7377m_device *ak7377m)
{
	int ret, min, max;

	LOG_INF("+");

	min = AK7377M_VIN_MIN;
	max = AK7377M_VIN_MAX;
	ret = regulator_set_voltage(ak7377m->vin, min, max);
	if (ret) {
		LOG_INF("regulator_set_voltage(vin), min(%d - %d), ret(%d)(fail)\n",
			min, max, ret);
	}
	ret = regulator_enable(ak7377m->vin);
	if (ret < 0) {
		LOG_INF("regulator_enable(vin), ret(%d)(fail)\n", ret);
		goto fail2;
	}
	LOG_INF("-\n");
	return ret;

fail2:
	regulator_disable(ak7377m->vin);
	return ret;
}

static int ak7377m_power_off2(struct ak7377m_device *ak7377m)
{
	int ret;
	LOG_INF("+\n");
	ret = regulator_disable(ak7377m->vin);
	LOG_INF("-\n");
	return ret;
}

static int ak7377m_set_setting(struct ak7377m_device *ak7377m)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ak7377m->sd);
	int ret = 0, val =0;
	int i = 0;
	bool is_update_pid = false;
	u8 pid_setting_len =  ARRAY_SIZE(ak7377m_pid_setting);
	client->addr = AK7377M_I2C_SLAVE_ADDR >> 1;

	//power on
	ret = ak7377m_power_on2(ak7377m);
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
	val = i2c_smbus_read_byte_data(client, AK7377M_PID_VER_ADDR);
	if (val != AK7377M_PID_VERSION) {
		is_update_pid = true;
	}
	//update pid
	if (is_update_pid) {
		//change to setting mode
		ret = i2c_smbus_write_byte_data(client, AK7377M_WRITE_CONTROL_ADDR, 0x3B);

		//write pid setting
		for (i = 0; i < pid_setting_len; i = i + 2) {
			ret = i2c_smbus_write_byte_data(client, ak7377m_pid_setting[i], ak7377m_pid_setting[i + 1]);
			if (ret < 0) {
				LOG_INF("%s:%d I2C failure: %d", __func__, __LINE__, ret);
				goto out;
			}
		}
		ret = i2c_smbus_write_byte_data(client, AK7377M_STORE_ADDR, 0x01);
		if (ret < 0) {
			LOG_INF("%s:%d I2C failure: %d", __func__, __LINE__, ret);
			goto out;
		}
		msleep(110);
		ret = i2c_smbus_write_byte_data(client, AK7377M_STORE_ADDR, 0x02);
		if (ret < 0) {
			LOG_INF("%s:%d I2C failure: %d", __func__, __LINE__, ret);
			goto out;
		}
		msleep(190);
		ret = i2c_smbus_write_byte_data(client, AK7377M_STORE_ADDR, 0x04);
		if (ret < 0) {
			LOG_INF("%s:%d I2C failure: %d", __func__, __LINE__, ret);
			goto out;
		}
		msleep(160);
		ret = i2c_smbus_write_byte_data(client, AK7377M_STORE_ADDR, 0x08);
		if (ret < 0) {
			LOG_INF("%s:%d I2C failure: %d", __func__, __LINE__, ret);
			goto out;
		}
		msleep(170);

		//check store
		ret = i2c_smbus_read_byte_data(client, AK7377M_DATA_CHECK_ADDR);
		if (ret < 0 || (ret & AK7377M_DATA_CHECK_BIT) ) {
			LOG_INF("%s:%d I2C failure: %d", __func__, __LINE__, ret);
			goto out;
		}

		//release setting mode
		ret = i2c_smbus_write_byte_data(client, AK7377M_WRITE_CONTROL_ADDR, 0x00);
		if (ret < 0) {
			LOG_INF("%s:%d I2C failure: %d", __func__, __LINE__, ret);
			goto out;
		}

		// check pid
		val = i2c_smbus_read_byte_data(client, AK7377M_PID_VER_ADDR);
		if (val != AK7377M_PID_VERSION) {
			LOG_INF("failed to update pid");
		} else {
			LOG_INF("update pid successfully");
		}
	}
	LOG_INF("-");
out:
	ak7377m_power_off2(ak7377m);
	return ret;
}
static int ak7377m_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct ak7377m_device *ak7377m;
	int ret;
	int prj_id = 0;
	unsigned int version = UNKNOWN_VERSION;

	LOG_INF("+\n");

	ak7377m = devm_kzalloc(dev, sizeof(*ak7377m), GFP_KERNEL);
	if (!ak7377m)
		return -ENOMEM;

	ak7377m->vin = devm_regulator_get(dev, "vin");
	if (IS_ERR(ak7377m->vin)) {
		ret = PTR_ERR(ak7377m->vin);
		if (ret != -EPROBE_DEFER)
			LOG_INF("cannot get vin regulator\n");
		return ret;
	}

	ak7377m->vdd = devm_regulator_get(dev, "vdd");
	if (IS_ERR(ak7377m->vdd)) {
		ret = PTR_ERR(ak7377m->vdd);
		if (ret != -EPROBE_DEFER)
			LOG_INF("cannot get vdd regulator\n");
		return ret;
	}

	ak7377m->vcamaf_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(ak7377m->vcamaf_pinctrl)) {
		ret = PTR_ERR(ak7377m->vcamaf_pinctrl);
		ak7377m->vcamaf_pinctrl = NULL;
		LOG_INF("cannot get pinctrl\n");
	} else {
		ak7377m->vcamaf_on = pinctrl_lookup_state(
			ak7377m->vcamaf_pinctrl, "vcamaf_on");

		if (IS_ERR(ak7377m->vcamaf_on)) {
			ret = PTR_ERR(ak7377m->vcamaf_on);
			ak7377m->vcamaf_on = NULL;
			LOG_INF("cannot get vcamaf_on pinctrl\n");
		}

		ak7377m->vcamaf_off = pinctrl_lookup_state(
			ak7377m->vcamaf_pinctrl, "vcamaf_off");

		if (IS_ERR(ak7377m->vcamaf_off)) {
			ret = PTR_ERR(ak7377m->vcamaf_off);
			ak7377m->vcamaf_off = NULL;
			LOG_INF("cannot get vcamaf_off pinctrl\n");
		}
	}

	v4l2_i2c_subdev_init(&ak7377m->sd, client, &ak7377m_ops);
	ak7377m->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	ak7377m->sd.internal_ops = &ak7377m_int_ops;

	ret = ak7377m_init_controls(ak7377m);
	if (ret)
		goto err_cleanup;

#if IS_ENABLED(CONFIG_MEDIA_CONTROLLER)
	ret = media_entity_pads_init(&ak7377m->sd.entity, 0, NULL);
	if (ret < 0)
		goto err_cleanup;

	ak7377m->sd.entity.function = MEDIA_ENT_F_LENS;
#endif

	ret = v4l2_async_register_subdev(&ak7377m->sd);
	if (ret < 0)
		goto err_cleanup;

	version = get_PCB_Version();
	prj_id = get_project();
	LOG_INF("project: %d, PCB VERSION[%u]", prj_id, version);
	if (((23101 == prj_id) || (23205 == prj_id)) && version < DVT1) {
		if (ak7377m_set_setting(ak7377m)) {
			LOG_INF("ak7377m_set_setting failed");
		}
	}
	LOG_INF("-\n");

	return 0;

err_cleanup:
	ak7377m_subdev_cleanup(ak7377m);
	return ret;
}

static void ak7377m_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ak7377m_device *ak7377m = sd_to_ak7377m_vcm(sd);

	LOG_INF("+\n");

	ak7377m_subdev_cleanup(ak7377m);

	LOG_INF("-\n");
}

static const struct i2c_device_id ak7377m_id_table[] = {
	{ AK7377M_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, ak7377m_id_table);

static const struct of_device_id ak7377m_of_table[] = {
	{ .compatible = "mediatek,ak7377m" },
	{ },
};
MODULE_DEVICE_TABLE(of, ak7377m_of_table);

static struct i2c_driver ak7377m_i2c_driver = {
	.driver = {
		.name = AK7377M_NAME,
		.of_match_table = ak7377m_of_table,
	},
	.probe  = ak7377m_probe,
	.remove = ak7377m_remove,
	.id_table = ak7377m_id_table,
};

module_i2c_driver(ak7377m_i2c_driver);

MODULE_AUTHOR("Po-Hao Huang <Po-Hao.Huang@mediatek.com>");
MODULE_DESCRIPTION("AK7377M VCM driver");
MODULE_LICENSE("GPL v2");
