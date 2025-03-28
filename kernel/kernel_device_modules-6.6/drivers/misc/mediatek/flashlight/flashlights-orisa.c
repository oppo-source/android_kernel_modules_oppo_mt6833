// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2021 Oplus

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/regmap.h>
#include <linux/videodev2.h>
#include <linux/pinctrl/consumer.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <linux/pm_runtime.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>

#if IS_ENABLED(CONFIG_MTK_FLASHLIGHT)
#include "flashlight-core.h"

#include <linux/power_supply.h>
#endif

#define sc6607_24700_NAME	"sc6607_24700"
#define sc6607_24700_I2C_ADDR	(0x64)

/* define channel, level */
#define ORIS_CHANNEL_NUM          2
#define ORIS_CHANNEL_CH1          0
#define ORIS_CHANNEL_CH2          1

/* define mutex and work queue */
static DEFINE_MUTEX(oris_mutex);
static struct work_struct oris_work_ch1;
static struct work_struct oris_work_ch2;


/* registers definitions */
#define REG_ENABLE		0x80
#define REG_LED0_FLASH_BR	0x81
#define REG_LED1_FLASH_BR	0x04
#define REG_LED0_TORCH_BR	0x84
#define REG_LED1_TORCH_BR	0x06
#define REG_FLASH_TOUT		0x08
#define REG_FLAG1		0x0A
#define REG_FLAG2		0x0B

/* define registers */
#define SC6607_REG_DEVICE_ID            (0x00)
#define SC6607_REG_LED_CTRL1            (0x80)
#define SC6607_REG_TLED1_FLASH_BR_CTR   (0x81)
#define SC6607_REG_TLED2_FLASH_BR_CTR   (0x82)
#define SC6607_REG_FLED_TIMER           (0x83)
#define SC6607_REG_TLED1_TORCH_BR_CTR   (0x84)
#define SC6607_REG_TLED2_TORCH_BR_CTR   (0x85)
#define SC6607_REG_LED_PRO              (0x86)
#define SC6607_REG_LED_STAT1            (0x87)
#define SC6607_REG_LED_STAT2            (0x88)
#define SC6607_REG_LED_FLG              (0x89)
#define SC6607_REG_LED_MASK             (0x8A)
#define SC6607_REG_FL_TX_REPORT         (0x8B)

#define SC6607_DISABLE              (0x01)
#define SC6607_ENABLE_LED1_TORCH    (0x21)
#define SC6607_ENABLE_LED1_FLASH    (0x81)
#define SC6607_ENABLE_LED2_TORCH    (0x11)
#define SC6607_ENABLE_LED2_FLASH    (0x41)
#define SC6607_TORCH_RAMP_TIME      (0x00)
#define SC6607_FLASH_TIMEOUT        (0x0A)

/* fault mask */
#define FAULT_TIMEOUT	(1<<0)
#define FAULT_THERMAL_SHUTDOWN	(1<<2)
#define FAULT_LED0_SHORT_CIRCUIT	(1<<5)
#define FAULT_LED1_SHORT_CIRCUIT	(1<<4)
#define ORIS_HW_TIMEOUT  400

/*  FLASH Brightness
 *	min 3.91 mA, step 7.83 mA, max 2000 mA
 */
#define sc6607_24700_FLASH_BRT_MIN 3910
#define sc6607_24700_FLASH_BRT_STEP 7830
#define sc6607_24700_FLASH_BRT_MAX 2000000
#define sc6607_24700_FLASH_BRT_uA_TO_REG(a)	\
	((a) < sc6607_24700_FLASH_BRT_MIN ? 0 :	\
	 (((a) - sc6607_24700_FLASH_BRT_MIN) / sc6607_24700_FLASH_BRT_STEP))
#define sc6607_24700_FLASH_BRT_REG_TO_uA(a)		\
	((a) * sc6607_24700_FLASH_BRT_STEP + sc6607_24700_FLASH_BRT_MIN)

/*  FLASH TIMEOUT DURATION
 *	min 32ms, step 32ms, max 1024ms
 */
#define sc6607_24700_FLASH_TOUT_MIN 200
#define sc6607_24700_FLASH_TOUT_STEP 200
#define sc6607_24700_FLASH_TOUT_MAX 1600

/*  TORCH BRT
 *	min 0.98 mA, step 1.96 mA, max 500 mA
 */
#define sc6607_24700_TORCH_BRT_MIN 25000
#define sc6607_24700_TORCH_BRT_STEP 12500
#define sc6607_24700_TORCH_BRT_MAX 500000
#define sc6607_24700_TORCH_BRT_uA_TO_REG(a)	\
	((a) < sc6607_24700_TORCH_BRT_MIN ? 0 :	\
	 (((a) - sc6607_24700_TORCH_BRT_MIN) / sc6607_24700_TORCH_BRT_STEP))
#define sc6607_24700_TORCH_BRT_REG_TO_uA(a)		\
	((a) * sc6607_24700_TORCH_BRT_STEP + sc6607_24700_TORCH_BRT_MIN)

#define sc6607_24700_LEVEL_NUM   28
#define sc6607_24700_LEVEL_TORCH 6

extern void oplus_chg_set_camera_on(bool val);

static const int *oris_current;
static const unsigned char *oris_torch_level;
static const unsigned char *oris_flash_level;
static volatile int oris_level_ch1 = -1;
static volatile int oris_level_ch2 = -1;

static const int sc6607_24700_current[sc6607_24700_LEVEL_NUM] = {
    25,   50,   75,   110,  125,  150,  175,  200,  250,  300,
    350,  400,  450,  500,  550,  600,  650,  700,  750,  800,
    850,  900,  950,  1000, 1050, 1100, 1150, 1200
};

//static const int sc6607_current[ARK_LEVEL_NUM] = {
//	25,   50,   75,   100,  125,  150,  175,  200,  250,  300,
//	350,  400,  450,  500,  550,  600,  650,  700,  750,  800,
//	850,  900,  950,  1000, 1050, 1100, 1150, 1200
//};

/*Offset: 25mA(b0000000)
Step:12.5mA
Range: 25mA(b0000000)~500mA(b0100110~b1111111)*/
static const unsigned char sc6607_24700_torch_level[sc6607_24700_LEVEL_NUM] = {
	0x00, 0x02, 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0E, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/*Offset:25mA(b0000000)
Step:12.5mA
Range: 25mA(b0000000)~1.5A(b1110110~b1111111)*/
static const unsigned char sc6607_24700_flash_level[sc6607_24700_LEVEL_NUM] = {
	0x00, 0x02, 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0E, 0x12, 0x16,
	0x1A, 0x1E, 0x22, 0x26, 0x2A, 0x2E, 0x32, 0x36, 0x3A, 0x3E,
	0x42, 0x46, 0x4A, 0x4E, 0x52, 0x56, 0x5A, 0x5E
};

/******************************************************************************
 * Timer and work queue
 *****************************************************************************/
static struct hrtimer oris_timer_ch1;
static struct hrtimer oris_timer_ch2;
static unsigned int oris_timeout_ms[ORIS_CHANNEL_NUM];
static volatile unsigned char oris_reg_enable;
static volatile int oris_flash_cts_enable = 0;
static volatile int oris_charge_enable = 0;
static volatile int oris_charge_mode = 0;
static volatile int oris_flash_mode = -1;


enum sc6607_24700_led_id {
	sc6607_24700_LED0 = 0,
	sc6607_24700_LED1,
	sc6607_24700_LED_MAX
};

enum v4l2_flash_led_nums {
	sc6607_CONTROL_LED0 = 2,
	sc6607_CONTROL_LED1,
};

/* struct sc6607_24700_platform_data
 *
 * @max_flash_timeout: flash timeout
 * @max_flash_brt: flash mode led brightness
 * @max_torch_brt: torch mode led brightness
 */
struct sc6607_24700_platform_data {
	u32 max_flash_timeout;
	u32 max_flash_brt[sc6607_24700_LED_MAX];
	u32 max_torch_brt[sc6607_24700_LED_MAX];
};


enum led_enable {
	MODE_SHDN = 0x0,
	MODE_TORCH = 0x08,
	MODE_FLASH = 0x0C,
};

/**
 * struct sc6607_24700_flash
 *
 * @dev: pointer to &struct device
 * @pdata: platform data
 * @regmap: reg. map for i2c
 * @lock: muxtex for serial access.
 * @led_mode: V4L2 LED mode
 * @ctrls_led: V4L2 controls
 * @subdev_led: V4L2 subdev
 */
struct sc6607_24700_flash {
	struct device *dev;
	struct sc6607_24700_platform_data *pdata;
	struct regmap *regmap;
	struct mutex lock;

	enum v4l2_flash_led_mode led_mode;
	struct v4l2_ctrl_handler ctrls_led[sc6607_24700_LED_MAX];
	struct v4l2_subdev subdev_led[sc6607_24700_LED_MAX];
	struct device_node *dnode[sc6607_24700_LED_MAX];
	struct pinctrl *sc6607_24700_hwen_pinctrl;
	struct pinctrl_state *sc6607_24700_hwen_high;
	struct pinctrl_state *sc6607_24700_hwen_low;
#if IS_ENABLED(CONFIG_MTK_FLASHLIGHT)
	struct flashlight_device_id flash_dev_id[sc6607_24700_LED_MAX];
#endif
    int level;
};

/* define usage count */
static int use_count;

static struct sc6607_24700_flash *sc6607_24700_flash_data;

#define to_sc6607_24700_flash(_ctrl, _no)	\
	container_of(_ctrl->handler, struct sc6607_24700_flash, ctrls_led[_no])

/* define pinctrl */
#define sc6607_24700_PINCTRL_PIN_HWEN 0
#define sc6607_24700_PINCTRL_PINSTATE_LOW 0
#define sc6607_24700_PINCTRL_PINSTATE_HIGH 1
#define sc6607_24700_PINCTRL_STATE_HWEN_HIGH "hwen_high"
#define sc6607_24700_PINCTRL_STATE_HWEN_LOW  "hwen_low"
#define ORIS_LEVEL_FLASH_CTS   4
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// i2c write and read
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int sc6607_write_reg(struct sc6607_24700_flash *flash, u8 reg, u8 val)
{
	int ret;
	struct i2c_client *client = to_i2c_client(flash->dev);
	mutex_lock(&flash->lock);
	ret = i2c_smbus_write_byte_data(client, reg, val);
	mutex_unlock(&flash->lock);

	if (ret < 0)
		pr_err("failed writing at 0x%02x\n", reg);

	return ret;
}

static int sc6607_read_reg(struct sc6607_24700_flash *flash, u8 reg)
{
	int ret;
	struct i2c_client *client = to_i2c_client(flash->dev);
	mutex_lock(&flash->lock);
	ret = i2c_smbus_read_byte_data(client, reg);
	mutex_unlock(&flash->lock);

	if (ret < 0) {
		pr_err("failed read at 0x%02x\n", reg);
	} else {
		pr_err("yyy-successed read, data = 0x%x",ret);
	}
	return ret;
}

static int oris_is_torch(int level)
{
	if (level >= sc6607_24700_LEVEL_TORCH)
		return -1;

	return 0;
}

/* flashlight disable function */
static int oris_disable_ch1(struct sc6607_24700_flash *flash)
{
	unsigned char reg, val;

	reg = SC6607_REG_LED_CTRL1;
	val = SC6607_DISABLE;
	return sc6607_write_reg(flash, reg, val);
}

static int oris_disable_ch2(struct sc6607_24700_flash *flash)
{
	unsigned char reg, val;

	reg = SC6607_REG_LED_CTRL1;
	val = SC6607_DISABLE;

	return sc6607_write_reg(flash, reg, val);
}


static void oris_work_disable_ch1(struct work_struct *data)
{
	pr_info("ht work queue callback\n");
	oris_disable_ch1(sc6607_24700_flash_data);
}

static void oris_work_disable_ch2(struct work_struct *data)
{
	pr_info("lt work queue callback\n");
	oris_disable_ch2(sc6607_24700_flash_data);
}

static enum hrtimer_restart oris_timer_func_ch1(struct hrtimer *timer)
{
	schedule_work(&oris_work_ch1);
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart oris_timer_func_ch2(struct hrtimer *timer)
{
	schedule_work(&oris_work_ch2);
	return HRTIMER_NORESTART;
}

/* enable mode control */
static int sc6607_24700_mode_ctrl(struct sc6607_24700_flash *flash)
{
	int rval = -EINVAL;
	pr_info("%s:%d", __func__, __LINE__);

	pr_info("mode_ctrl %s mode:%d", __func__, flash->led_mode);

	switch (flash->led_mode) {
	case V4L2_FLASH_LED_MODE_NONE:
/* 		rval = regmap_update_bits(flash->regmap,
					  REG_ENABLE, 0x20, 0);
		rval2 = regmap_update_bits(flash->regmap,
					  REG_ENABLE, 0x80, 0); */
		rval = sc6607_read_reg(flash, SC6607_REG_LED_CTRL1);
		rval = sc6607_write_reg(flash, SC6607_REG_LED_CTRL1, SC6607_DISABLE);
		pr_info("yuan disable write reg rval:%d", rval);
		break;
	case V4L2_FLASH_LED_MODE_TORCH:
/* 		rval = regmap_update_bits(flash->regmap,
					  REG_ENABLE, 0x20, 1); */
		rval = sc6607_write_reg(flash, SC6607_REG_LED_CTRL1, SC6607_ENABLE_LED1_TORCH);
		break;
	case V4L2_FLASH_LED_MODE_FLASH:
/* 		rval = regmap_update_bits(flash->regmap,
					  REG_ENABLE, 0x80, 1); */
		//rval = sc6607_write_reg(flash, SC6607_REG_LED_CTRL1, SC6607_ENABLE_LED1_FLASH);
		rval = sc6607_write_reg(flash, SC6607_REG_LED_CTRL1, SC6607_ENABLE_LED2_FLASH);
		break;
	}
	return rval;
}

/* led1/2 enable/disable */
static int sc6607_24700_enable_ctrl(struct sc6607_24700_flash *flash,
			      enum sc6607_24700_led_id led_no, bool on)
{
	int rval = 0;
	pr_info("%s:%d", __func__, __LINE__);
	pr_info("%s %d enable:%d", __func__, led_no, on);
	if (led_no == sc6607_24700_LED0) {
		if (on) {
/* 			rval = regmap_update_bits(flash->regmap,
							REG_ENABLE, 0x01, 0x01); */
		} else {
/* 			rval = regmap_update_bits(flash->regmap,
							REG_ENABLE, 0x01, 0x00); */
		}
	} else {
		if (on) {
/* 			rval = regmap_update_bits(flash->regmap,
							REG_ENABLE, 0x02, 0x02); */
		} else {
/* 			rval = regmap_update_bits(flash->regmap,
							REG_ENABLE, 0x02, 0x00); */
		}
	}
	pr_info("%s: return val:%d", __func__,  rval);
	return rval;
}

static int sc6607_24700_select_led(struct sc6607_24700_flash *flash,
										int led_num)
{
	int rval = 0;
	pr_info("engineer cam select led->%d", led_num);
	pr_info("%s:%d", __func__, __LINE__);
	if (led_num == sc6607_CONTROL_LED0) {
/* 		rval = regmap_update_bits(flash->regmap, REG_ENABLE, 0x01, 0x01);
		rval = regmap_update_bits(flash->regmap, REG_ENABLE, 0x02, 0x00); */
	} else if (led_num == sc6607_CONTROL_LED1) {
/* 		rval = regmap_update_bits(flash->regmap, REG_ENABLE, 0x01, 0x00);
		rval = regmap_update_bits(flash->regmap, REG_ENABLE, 0x02, 0x02); */
	} else {
/* 		rval = regmap_update_bits(flash->regmap, REG_ENABLE, 0x01, 0x00);
		rval = regmap_update_bits(flash->regmap, REG_ENABLE, 0x02, 0x00); */
	}
	 return rval;
}

/* torch1/2 brightness control */
static int sc6607_24700_torch_brt_ctrl(struct sc6607_24700_flash *flash,
				 enum sc6607_24700_led_id led_no, unsigned int brt)
{
	int rval = 0;
	u8 br_bits;
	pr_info("%s:%d", __func__, __LINE__);
	pr_info("%s set brt:%u for both torch leds", __func__, brt);
/* 	if (brt < sc6607_24700_TORCH_BRT_MIN)
		return sc6607_24700_enable_ctrl(flash, led_no, false); */

	br_bits = sc6607_24700_TORCH_BRT_uA_TO_REG(brt);


	//rval = i2c_smbus_write_byte_data(client, 0x80, 0x21);
	rval = sc6607_write_reg(flash, SC6607_REG_TLED1_TORCH_BR_CTR, br_bits);

/* 	rval = regmap_update_bits(flash->regmap,
				REG_LED0_TORCH_BR, 0xff, 0xa8); */
/* 	rval = regmap_update_bits(flash->regmap,
				REG_LED1_TORCH_BR, 0x7f, br_bits); */

	pr_info("%s: return val:%d", __func__,  rval);
	return rval;
}

/* flash1/2 brightness control */
static int sc6607_24700_flash_brt_ctrl(struct sc6607_24700_flash *flash,
				 enum sc6607_24700_led_id led_no, unsigned int brt)
{
	int rval = 0;
	u8 br_bits;
	pr_info("%s:%d", __func__, __LINE__);
	pr_info("%s %d brt:%u", __func__, led_no, brt);
	if (brt < sc6607_24700_FLASH_BRT_MIN)
		return sc6607_24700_enable_ctrl(flash, led_no, false);

	br_bits = sc6607_24700_FLASH_BRT_uA_TO_REG(brt);
	if (led_no == sc6607_24700_LED0) {
		rval = regmap_update_bits(flash->regmap,
				  REG_LED0_FLASH_BR, 0xff, br_bits);
	} else {
		rval = regmap_update_bits(flash->regmap,
				  REG_LED1_FLASH_BR, 0xff, br_bits);
	}
	pr_info("%s: return val:%d", __func__,  rval);
	return rval;
}

/* flash1/2 timeout control */
static int sc6607_24700_flash_tout_ctrl(struct sc6607_24700_flash *flash,
				unsigned int tout)
{
	int rval = 0;
	u8 tout_bits;
	pr_info("%s:%d", __func__, __LINE__);
	if (tout == 200)
		tout_bits = 0x04;
	else
		tout_bits = 0x07 + (tout / sc6607_24700_FLASH_TOUT_STEP);

/* 	rval = regmap_update_bits(flash->regmap,
				  REG_FLASH_TOUT, 0x1f, tout_bits); */

	return rval;
}

/* v4l2 controls  */
static int sc6607_24700_get_ctrl(struct v4l2_ctrl *ctrl, enum sc6607_24700_led_id led_no)
{
	struct sc6607_24700_flash *flash = to_sc6607_24700_flash(ctrl, led_no);
	int rval = -EINVAL;
	pr_info("%s:%d", __func__, __LINE__);
	mutex_lock(&flash->lock);
	if (ctrl->id == V4L2_CID_FLASH_FAULT) {
		s32 fault = 0;
		unsigned int reg_val = 0;

		//rval = regmap_read(flash->regmap, REG_FLAG1, &reg_val);
		if (rval < 0)
			goto out;
		if (reg_val & FAULT_LED0_SHORT_CIRCUIT)
			fault |= V4L2_FLASH_FAULT_SHORT_CIRCUIT;
		if (reg_val & FAULT_LED1_SHORT_CIRCUIT)
			fault |= V4L2_FLASH_FAULT_SHORT_CIRCUIT;
		if (reg_val & FAULT_THERMAL_SHUTDOWN)
			fault |= V4L2_FLASH_FAULT_OVER_TEMPERATURE;
		if (reg_val & FAULT_TIMEOUT)
			fault |= V4L2_FLASH_FAULT_TIMEOUT;
		ctrl->cur.val = fault;
	}

out:
	mutex_unlock(&flash->lock);
	return rval;
}

static int sc6607_24700_set_ctrl(struct v4l2_ctrl *ctrl, enum sc6607_24700_led_id led_no)
{
	struct sc6607_24700_flash *flash = to_sc6607_24700_flash(ctrl, led_no);
	int rval = -EINVAL;
	//unsigned int boost_reg_val;
	pr_info("%s:%d", __func__, __LINE__);
	//rval = regmap_read(flash->regmap, REG_ENABLE, &boost_reg_val);
	pr_info("%s: led_no:%d ctrID:0x%x, ctrlVal:0x%x", __func__, led_no, ctrl->id, ctrl->val);
	mutex_lock(&flash->lock);

	switch (ctrl->id) {
	case V4L2_CID_FLASH_LED_MODE:
		pr_info("yuan V4L2_CID_FLASH_LED_MODE");
		flash->led_mode = ctrl->val;
		if (flash->led_mode != V4L2_FLASH_LED_MODE_FLASH)
			rval = sc6607_24700_mode_ctrl(flash);
		else
			rval = 0;
		if (flash->led_mode == V4L2_FLASH_LED_MODE_NONE)
			sc6607_24700_enable_ctrl(flash, led_no, false);
		else if (flash->led_mode == V4L2_FLASH_LED_MODE_TORCH)
			rval = sc6607_24700_enable_ctrl(flash, led_no, true);
		break;

	case V4L2_CID_FLASH_STROBE_SOURCE:
		pr_info("yuan V4L2_CID_FLASH_STROBE_SOURCE");
/* 		rval = regmap_update_bits(flash->regmap,
					  REG_ENABLE, 0x20, (ctrl->val) << 5); */
		if (rval < 0)
			goto err_out;
		break;

	case V4L2_CID_FLASH_STROBE:
		pr_info("yuan V4L2_CID_FLASH_STROBE");
		if (flash->led_mode != V4L2_FLASH_LED_MODE_FLASH) {
			rval = -EBUSY;
			goto err_out;
		}
		flash->led_mode = V4L2_FLASH_LED_MODE_FLASH;
		rval = sc6607_24700_mode_ctrl(flash);
		rval = sc6607_24700_enable_ctrl(flash, led_no, true);
		break;

	case V4L2_CID_FLASH_STROBE_STOP:
		pr_info("yuan V4L2_CID_FLASH_STROBE_STOP");
		if (flash->led_mode != V4L2_FLASH_LED_MODE_FLASH) {
			rval = -EBUSY;
			goto err_out;
		}
		sc6607_24700_enable_ctrl(flash, led_no, false);
		flash->led_mode = V4L2_FLASH_LED_MODE_NONE;
		rval = sc6607_24700_mode_ctrl(flash);
		break;

	case V4L2_CID_FLASH_TIMEOUT:
		pr_info("yuan V4L2_CID_FLASH_TIMEOUT");
		rval = sc6607_24700_flash_tout_ctrl(flash, ctrl->val);
		break;

	case V4L2_CID_FLASH_INTENSITY:
		pr_info("yuan V4L2_CID_FLASH_INTENSITY");
		rval = sc6607_24700_flash_brt_ctrl(flash, led_no, ctrl->val);
		break;

	case V4L2_CID_FLASH_TORCH_INTENSITY:
		pr_info("yuan V4L2_CID_FLASH_TORCH_INTENSITY");
		rval = sc6607_24700_torch_brt_ctrl(flash, led_no, ctrl->val);
		break;
	}

err_out:
	mutex_unlock(&flash->lock);
	return rval;
}

static int sc6607_24700_led1_get_ctrl(struct v4l2_ctrl *ctrl)
{
	return sc6607_24700_get_ctrl(ctrl, sc6607_24700_LED1);
}

static int sc6607_24700_led1_set_ctrl(struct v4l2_ctrl *ctrl)
{
	return sc6607_24700_set_ctrl(ctrl, sc6607_24700_LED1);
}

static int sc6607_24700_led0_get_ctrl(struct v4l2_ctrl *ctrl)
{
	return sc6607_24700_get_ctrl(ctrl, sc6607_24700_LED0);
}

static int sc6607_24700_led0_set_ctrl(struct v4l2_ctrl *ctrl)
{
	return sc6607_24700_set_ctrl(ctrl, sc6607_24700_LED0);
}

static const struct v4l2_ctrl_ops sc6607_24700_led_ctrl_ops[sc6607_24700_LED_MAX] = {
	[sc6607_24700_LED0] = {
			.g_volatile_ctrl = sc6607_24700_led0_get_ctrl,
			.s_ctrl = sc6607_24700_led0_set_ctrl,
			},
	[sc6607_24700_LED1] = {
			.g_volatile_ctrl = sc6607_24700_led1_get_ctrl,
			.s_ctrl = sc6607_24700_led1_set_ctrl,
			}
};

static int sc6607_24700_init_controls(struct sc6607_24700_flash *flash,
				enum sc6607_24700_led_id led_no)
{
	struct v4l2_ctrl *fault;
	u32 max_flash_brt = flash->pdata->max_flash_brt[led_no];
	u32 max_torch_brt = flash->pdata->max_torch_brt[led_no];
	struct v4l2_ctrl_handler *hdl = &flash->ctrls_led[led_no];
	const struct v4l2_ctrl_ops *ops = &sc6607_24700_led_ctrl_ops[led_no];
	pr_info("%s:%d", __func__, __LINE__);
	v4l2_ctrl_handler_init(hdl, 8);

	/* flash mode */
	v4l2_ctrl_new_std_menu(hdl, ops, V4L2_CID_FLASH_LED_MODE,
			       V4L2_FLASH_LED_MODE_TORCH, ~0x7,
			       V4L2_FLASH_LED_MODE_NONE);
	flash->led_mode = V4L2_FLASH_LED_MODE_NONE;

	/* flash source */
	v4l2_ctrl_new_std_menu(hdl, ops, V4L2_CID_FLASH_STROBE_SOURCE,
			       0x1, ~0x3, V4L2_FLASH_STROBE_SOURCE_SOFTWARE);

	/* flash strobe */
	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FLASH_STROBE, 0, 0, 0, 0);

	/* flash strobe stop */
	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FLASH_STROBE_STOP, 0, 0, 0, 0);

	/* flash strobe timeout */
	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FLASH_TIMEOUT,
			  sc6607_24700_FLASH_TOUT_MIN,
			  flash->pdata->max_flash_timeout,
			  sc6607_24700_FLASH_TOUT_STEP,
			  flash->pdata->max_flash_timeout);

	/* flash brt */
	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FLASH_INTENSITY,
			  sc6607_24700_FLASH_BRT_MIN, max_flash_brt,
			  sc6607_24700_FLASH_BRT_STEP, max_flash_brt);

	/* torch brt */
	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FLASH_TORCH_INTENSITY,
			  sc6607_24700_TORCH_BRT_MIN, max_torch_brt,
			  sc6607_24700_TORCH_BRT_STEP, max_torch_brt);

	/* fault */
	fault = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FLASH_FAULT, 0,
				  V4L2_FLASH_FAULT_OVER_VOLTAGE
				  | V4L2_FLASH_FAULT_OVER_TEMPERATURE
				  | V4L2_FLASH_FAULT_SHORT_CIRCUIT
				  | V4L2_FLASH_FAULT_TIMEOUT, 0, 0);
	if (fault != NULL)
		fault->flags |= V4L2_CTRL_FLAG_VOLATILE;

	if (hdl->error)
		return hdl->error;

	flash->subdev_led[led_no].ctrl_handler = hdl;
	return 0;
}

/* initialize device */
static const struct v4l2_subdev_ops sc6607_24700_ops = {
	.core = NULL,
};

static const struct regmap_config sc6607_24700_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xF,
};

static void sc6607_24700_v4l2_i2c_subdev_init(struct v4l2_subdev *sd,
		struct i2c_client *client,
		const struct v4l2_subdev_ops *ops)
{
	v4l2_subdev_init(sd, ops);
	sd->flags |= V4L2_SUBDEV_FL_IS_I2C;
	/* the owner is the same as the i2c_client's driver owner */
	//pr_info("yuan client->addr:0x%x", client->addr);
	//client->addr = 0x64 ;
	sd->owner = client->dev.driver->owner;
	sd->dev = &client->dev;
	/* i2c_client and v4l2_subdev point to one another */
	v4l2_set_subdevdata(sd, client);
	i2c_set_clientdata(client, sd);
	/* initialize name */
	snprintf(sd->name, sizeof(sd->name), "%s %d-%04x",
		client->dev.driver->name, i2c_adapter_id(client->adapter),
		client->addr);
	pr_info("yuan sd->name:%s\n", sd->name);
}

static int sc6607_24700_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	int ret;

	pr_info("%s\n", __func__);

	ret = pm_runtime_get_sync(sd->dev);
	if (ret < 0) {
		pm_runtime_put_noidle(sd->dev);
		return ret;
	}

	return 0;
}

static int sc6607_24700_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	pr_info("%s\n", __func__);

	pm_runtime_put(sd->dev);

	return 0;
}

static const struct v4l2_subdev_internal_ops sc6607_24700_int_ops = {
	.open = sc6607_24700_open,
	.close = sc6607_24700_close,
};

static int sc6607_24700_subdev_init(struct sc6607_24700_flash *flash,
			      enum sc6607_24700_led_id led_no, char *led_name)
{
	struct i2c_client *client = to_i2c_client(flash->dev);
	struct device_node *np = flash->dev->of_node, *child;
	const char *fled_name = "flash";
	int rval;
	pr_info("%s:%d", __func__, __LINE__);
	// pr_info("%s %d", __func__, led_no);

	sc6607_24700_v4l2_i2c_subdev_init(&flash->subdev_led[led_no],
				client, &sc6607_24700_ops);
	flash->subdev_led[led_no].flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	flash->subdev_led[led_no].internal_ops = &sc6607_24700_int_ops;
	strscpy(flash->subdev_led[led_no].name, led_name,
		sizeof(flash->subdev_led[led_no].name));

	for (child = of_get_child_by_name(np, fled_name); child;
			child = of_find_node_by_name(child, fled_name)) {
		int rv;
		u32 reg = 0;

		rv = of_property_read_u32(child, "reg", &reg);
		if (rv)
			continue;

		if (reg == led_no) {
			flash->dnode[led_no] = child;
			flash->subdev_led[led_no].fwnode =
				of_fwnode_handle(flash->dnode[led_no]);
		}
	}

	rval = sc6607_24700_init_controls(flash, led_no);
	if (rval)
		goto err_out;
	rval = media_entity_pads_init(&flash->subdev_led[led_no].entity, 0, NULL);
	if (rval < 0)
		goto err_out;
	flash->subdev_led[led_no].entity.function = MEDIA_ENT_F_FLASH;

	rval = v4l2_async_register_subdev(&flash->subdev_led[led_no]);
	if (rval < 0)
		goto err_out;

	return rval;

err_out:
	v4l2_ctrl_handler_free(&flash->ctrls_led[led_no]);
	return rval;
}

/* flashlight init */
static int sc6607_24700_init(struct sc6607_24700_flash *flash)
{
	int ret = 0;

	/* clear enable register */
	ret = sc6607_read_reg(flash, SC6607_REG_LED_CTRL1);
	ret = sc6607_write_reg(flash, SC6607_REG_LED_CTRL1, SC6607_DISABLE);

	/* set torch current ramp time and flash timeout */
	ret |= sc6607_write_reg(flash, SC6607_REG_TLED1_FLASH_BR_CTR, 0x56);
	ret |= sc6607_write_reg(flash, SC6607_REG_TLED2_FLASH_BR_CTR, 0x00);
	ret |= sc6607_write_reg(flash, SC6607_REG_FLED_TIMER, 0x9f);
	ret |= sc6607_write_reg(flash, SC6607_REG_TLED1_TORCH_BR_CTR, 0x06);
	ret |= sc6607_write_reg(flash, SC6607_REG_TLED2_TORCH_BR_CTR, 0x00);
	ret |= sc6607_write_reg(flash, SC6607_REG_LED_PRO , 0x02);
	ret |= sc6607_write_reg(flash, SC6607_REG_LED_MASK, 0x48);
	ret |= sc6607_write_reg(flash, SC6607_REG_FL_TX_REPORT, 0x01);

  	if (ret < 0)
  		pr_info("Failed to init.\n");

	return ret;
}

/* flashlight uninit */
static int sc6607_24700_uninit(struct sc6607_24700_flash *flash)
{
	int rval = 0;
	pr_info("%s:%d", __func__, __LINE__);
/* 	sc6607_24700_pinctrl_set(flash,
			sc6607_24700_PINCTRL_PIN_HWEN, sc6607_24700_PINCTRL_PINSTATE_LOW); */
	rval = sc6607_write_reg(flash, SC6607_REG_LED_CTRL1, SC6607_DISABLE);
	
	return 0;
}

static int sc6607_24700_flash_open(void)
{
	return 0;
}

static int sc6607_24700_flash_release(void)
{
	return 0;
}

/* legacy flashlight ops */
//static int sc6607_24700_is_torch(int level)
//{
//	if (level >= sc6607_24700_LEVEL_TORCH)
//		return -1;

//	return 0;
//}

static int sc6607_24700_verify_level(int level)
{
	if (level < 0)
		level = 0;
	else if (level >= sc6607_24700_LEVEL_NUM)
		level = sc6607_24700_LEVEL_NUM - 1;

	return level;
}

//static int sc6607_24700_set_level(int channel, int level)
//{
//	sc6607_24700_flash_data->level = sc6607_24700_verify_level(level);
//	if (!sc6607_24700_is_torch(sc6607_24700_flash_data->level)) {
//		sc6607_24700_torch_brt_ctrl(sc6607_24700_flash_data, channel,
//				1000 * sc6607_24700_current[sc6607_24700_flash_data->level]);
//	} else {
//		sc6607_24700_flash_brt_ctrl(sc6607_24700_flash_data, channel,
//				1000 * sc6607_24700_current[sc6607_24700_flash_data->level]);
//	}
//	return 0;
//}

/* set flashlight level */
static int oris_set_level_ch1(int level)
{
	int ret;
	unsigned char reg, val;

	level = sc6607_24700_verify_level(level);

	if (!oris_is_torch(level)) {
		/* set torch brightness level */
		reg = SC6607_REG_TLED1_TORCH_BR_CTR;
		val = oris_torch_level[level];
		pr_err("meng torch 0x%x\n",val);
	} else {
		/* set flash brightness level */
		reg = SC6607_REG_TLED1_FLASH_BR_CTR;
		val = oris_flash_level[level];
		pr_err("meng flash 0x%x\n",val);
	}
	oris_level_ch1 = level;
	ret = sc6607_read_reg(sc6607_24700_flash_data, reg);
	ret = sc6607_write_reg(sc6607_24700_flash_data, reg, val);

	return ret;
}

int oris_set_level_ch2(int level)
{
	int ret;
	unsigned char reg, val;

	level = sc6607_24700_verify_level(level);

	if (!oris_is_torch(level)) {
		/* set torch brightness level */
		reg = SC6607_REG_TLED2_TORCH_BR_CTR;
		val = oris_torch_level[level];
	} else {
		/* set flash brightness level */
		reg = SC6607_REG_TLED2_FLASH_BR_CTR;
		val = oris_flash_level[level];
	}
	oris_level_ch2 = level;
	ret = sc6607_write_reg(sc6607_24700_flash_data, reg, val);

	return ret;
}

static int oris_set_level(int channel, int level)
{
	if (channel == ORIS_CHANNEL_CH1)
		oris_set_level_ch1(level);
	else if (channel == ORIS_CHANNEL_CH2)
		oris_set_level_ch2(level);
	else {
		pr_err("Error channel\n");
		return -1;
	}

	return 0;
}

int oris_timer_start(int channel, ktime_t ktime)
{
	if (channel == ORIS_CHANNEL_CH1)
		hrtimer_start(&oris_timer_ch1, ktime, HRTIMER_MODE_REL);
	else if (channel ==  ORIS_CHANNEL_CH2)
		hrtimer_start(&oris_timer_ch2, ktime, HRTIMER_MODE_REL);
	else {
		pr_err("Error channel\n");
		return -1;
	}

	return 0;
}

int oris_timer_cancel(int channel)
{
	if (channel ==  ORIS_CHANNEL_CH1)
		hrtimer_cancel(&oris_timer_ch1);
	else if (channel ==  ORIS_CHANNEL_CH2)
		hrtimer_cancel(&oris_timer_ch2);
	else {
		pr_err("Error channel\n");
		return -1;
	}

	return 0;
}

/* flashlight enable function */
static int oris_enable_ch1(void)
{
	unsigned char reg, val;

	reg = SC6607_REG_LED_CTRL1;
	if (!oris_is_torch(oris_level_ch1)) {
		/* torch mode */
		pr_err("menmg111111\n");
		oris_reg_enable = SC6607_ENABLE_LED1_TORCH;
	} else {
		/* flash mode */
		if (oris_charge_mode == FLASHLIGHT_CHARGER_READY && oris_charge_enable == 1 && oris_flash_cts_enable == 0) {
			oris_flash_mode++;
		}
    	    if (oris_charge_enable == 0){
    		oplus_chg_set_camera_on(1);
	    	oris_charge_enable = 1;
	    	oris_flash_mode++;
	    	pr_info("oplus_chg_set_camera_on set amg_charge_enable 1");
	    }

		oris_reg_enable = SC6607_ENABLE_LED1_FLASH;
	}
	val = oris_reg_enable;

	return sc6607_write_reg(sc6607_24700_flash_data, reg, val);
}

static int oris_enable_ch2(void)
{
	unsigned char reg, val;

	reg = SC6607_REG_LED_CTRL1;
	if (!oris_is_torch(oris_level_ch2)) {
		/* torch mode */
		oris_reg_enable |= SC6607_ENABLE_LED2_TORCH;
	} else {
		/* flash mode */
		if (oris_charge_enable == 0){
			oplus_chg_set_camera_on(1);
			oris_charge_enable = 1;
		}
		oris_reg_enable |= SC6607_ENABLE_LED2_FLASH;
	}
	val = oris_reg_enable;

	return sc6607_write_reg(sc6607_24700_flash_data, reg, val);
}

static int oris_enable(int channel)
{
	if (channel == ORIS_CHANNEL_CH1)
		oris_enable_ch1();
	else if (channel == ORIS_CHANNEL_CH2)
		oris_enable_ch2();
	else {
		pr_err("Error channel\n");
		return -1;
	}
	return 0;
}

/* flashlight disable function */
static int oris_disable(int channel)
{
	if (channel == ORIS_CHANNEL_CH1) {
		oris_disable_ch1(sc6607_24700_flash_data);
		pr_info("ORIS_CHANNEL_CH1\n");
	} else if (channel == ORIS_CHANNEL_CH2) {
		oris_disable_ch2(sc6607_24700_flash_data);
		pr_info("ORIS_CHANNEL_CH2\n");
	} else {
		pr_err("Error channel\n");
		return -1;
	}

	if (oris_flash_mode == 0 && oris_charge_enable == 1) {
		oplus_chg_set_camera_on(0);
		oris_charge_enable = 0;
		oris_flash_mode = -1;
		oris_charge_mode = 0;
	} else if (oris_flash_mode == 3 && oris_charge_mode == 1 && oris_charge_enable == 0) {
		oplus_chg_set_camera_on(1);
		oris_charge_enable = 1;
		oris_charge_mode = 0;
	}

	return 0;
}

void oris_is_flash_cts(void) {
	if (oris_charge_mode == FLASHLIGHT_CHARGER_READY && oris_level_ch1 == ORIS_LEVEL_FLASH_CTS) {
		oris_flash_cts_enable = 1;
	}
}


static int sc6607_24700_ioctl(unsigned int cmd, unsigned long arg)
{
	struct flashlight_dev_arg *fl_arg;
	int channel;
	int ret;
	ktime_t ktime;

	pr_info("%s:%d", __func__, __LINE__);
	fl_arg = (struct flashlight_dev_arg *)arg;
	channel = fl_arg->channel;

	/* verify channel */
	if (channel < 0 || channel >= ORIS_CHANNEL_NUM) {
		pr_err("Failed with error channel\n");
		return -EINVAL;
	}

	switch (cmd) {
	case FLASH_IOC_SET_TIME_OUT_TIME_MS:
		pr_info("FLASH_IOC_SET_TIME_OUT_TIME_MS(%d): %d\n",
				channel, (int)fl_arg->arg);
		oris_timeout_ms[channel] = fl_arg->arg;
		break;

	case FLASH_IOC_SET_ONOFF:
		pr_info("FLASH_IOC_SET_ONOFF(%d): %d\n",
				channel, (int)fl_arg->arg);
		if (fl_arg->arg == 1) {
			if (oris_timeout_ms[channel]) {
				ktime = ktime_set(oris_timeout_ms[channel] / 1000,
						(oris_timeout_ms[channel] % 1000) * 1000000);
				oris_timer_start(channel, ktime);
			}
			oris_enable(channel);
		} else {
			oris_disable(channel);
			oris_timer_cancel(channel);
		}
		break;
    case FLASH_IOC_IS_CHARGER_READY:
	    pr_info("FLASH_IOC_IS_CHARGER_READY(%d)\n", (int)fl_arg->arg);
	    oris_charge_mode = 0;
	    if (fl_arg->arg == FLASHLIGHT_CHARGER_READY) {
		    oris_charge_mode = 1;
	    }
	    oris_is_flash_cts();
	    break;

	case OPLUS_FLASH_IOC_SELECT_LED_NUM:
		if (fl_arg->arg == sc6607_CONTROL_LED0 || fl_arg->arg == sc6607_CONTROL_LED1) {
			sc6607_24700_flash_data->led_mode = V4L2_FLASH_LED_MODE_FLASH;
			 ret = sc6607_24700_select_led(sc6607_24700_flash_data, fl_arg->arg);
			if (ret < 0) {
				pr_err("engineer cam set led[%d] fail", fl_arg->arg);
				return ret;
			}
		} else {
			sc6607_24700_flash_data->led_mode = V4L2_FLASH_LED_MODE_NONE;
			sc6607_24700_mode_ctrl(sc6607_24700_flash_data);
			sc6607_24700_enable_ctrl(sc6607_24700_flash_data, channel, false);
		}
		break;

	case FLASH_IOC_SET_DUTY:
		pr_info("FLASH_IOC_SET_DUTY(%d): %d\n",
				channel, (int)fl_arg->arg);
		oris_set_level(channel, fl_arg->arg);
		break;

	case FLASH_IOC_GET_HW_TIMEOUT:
		pr_info("FLASH_IOC_GET_HW_TIMEOUT(%d)\n", channel);
		fl_arg->arg = ORIS_HW_TIMEOUT;
		break;

	case FLASH_IOC_GET_DUTY_NUMBER:
	     pr_info("FLASH_IOC_GET_DUTY_NUMBER(%d): %d\n",
				channel, (int)fl_arg->arg);
		 fl_arg->arg = sc6607_24700_LEVEL_NUM;
		break;

	case FLASH_IOC_GET_MAX_TORCH_DUTY:
		pr_info("FLASH_IOC_GET_MAX_TORCH_DUTY(%d): %d\n",
				channel, (int)fl_arg->arg);
		fl_arg->arg = sc6607_24700_LEVEL_TORCH - 1;
		break;

	case FLASH_IOC_GET_DUTY_CURRENT:
		pr_info("FLASH_IOC_GET_DUTY_CURRENT(%d): %d\n",
				channel, (int)fl_arg->arg);
		fl_arg->arg = oris_current[(int)fl_arg->arg];
		break;

	default:
		pr_info("No such command and arg(%d): (%d, %d)\n",
				channel, _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;
	}

	return 0;
}

static int sc6607_24700_set_driver(int set)
{
	int ret = 0;
	pr_info("%s:%d", __func__, __LINE__);
	/* set chip and usage count */
	mutex_lock(&oris_mutex);
	if (set) {
		if (!use_count)
			ret = sc6607_24700_init(sc6607_24700_flash_data);
		use_count++;
		oris_flash_cts_enable = 0;
		pr_debug("Set driver: %d\n", use_count);
	} else {
		use_count--;
		if (!use_count)
			ret = sc6607_24700_uninit(sc6607_24700_flash_data);
		if (use_count < 0)
			use_count = 0;
		if (oris_charge_enable == 1) {
			oplus_chg_set_camera_on(0);
			oris_charge_enable = 0;
		}
		oris_flash_cts_enable = 0;
		pr_debug("Unset driver: %d\n", use_count);
	}
	mutex_unlock(&oris_mutex);

	return 0;
}

static ssize_t sc6607_24700_strobe_store(struct flashlight_arg arg)
{
	pr_info("%s:%d", __func__, __LINE__);
	if(arg.dur != 0) {
		sc6607_24700_set_driver(1);
		sc6607_24700_torch_brt_ctrl(sc6607_24700_flash_data, arg.channel,
					arg.level * 25000);
		sc6607_24700_flash_data->led_mode = V4L2_FLASH_LED_MODE_TORCH;
		sc6607_24700_mode_ctrl(sc6607_24700_flash_data);
		sc6607_24700_enable_ctrl(sc6607_24700_flash_data, arg.channel, true);
		msleep(arg.dur);
		//sc6607_24700_disable(arg.channel);
		sc6607_24700_flash_data->led_mode = V4L2_FLASH_LED_MODE_NONE;
		sc6607_24700_mode_ctrl(sc6607_24700_flash_data);
		sc6607_24700_enable_ctrl(sc6607_24700_flash_data, arg.channel, false);
		sc6607_24700_set_driver(0);
	} else if (arg.level == 1){
		sc6607_24700_set_driver(1);
		sc6607_24700_torch_brt_ctrl(sc6607_24700_flash_data, arg.channel,
				6 * 25000);
		sc6607_24700_flash_data->led_mode = V4L2_FLASH_LED_MODE_TORCH;
		sc6607_24700_mode_ctrl(sc6607_24700_flash_data);
		sc6607_24700_enable_ctrl(sc6607_24700_flash_data, arg.channel, true);
	} else if (arg.level == 0){
		sc6607_24700_flash_data->led_mode = V4L2_FLASH_LED_MODE_NONE;
		sc6607_24700_mode_ctrl(sc6607_24700_flash_data);
		sc6607_24700_enable_ctrl(sc6607_24700_flash_data, arg.channel, false);
		sc6607_24700_set_driver(0);
	}
	return 0;
}

static struct flashlight_operations sc6607_24700_flash_ops = {
	sc6607_24700_flash_open,
	sc6607_24700_flash_release,
	sc6607_24700_ioctl,
	sc6607_24700_strobe_store,
	sc6607_24700_set_driver
};

static int sc6607_24700_parse_dt(struct sc6607_24700_flash *flash)
{
	struct device_node *np, *cnp;
	struct device *dev = flash->dev;
	u32 decouple = 0;
	int i = 0;
	pr_info("%s:%d", __func__, __LINE__);
	if (!dev || !dev->of_node)
		return -ENODEV;

	np = dev->of_node;
	for_each_child_of_node(np, cnp) {
		if (of_property_read_u32(cnp, "type",
					&flash->flash_dev_id[i].type))
			goto err_node_put;
		if (of_property_read_u32(cnp,
					"ct", &flash->flash_dev_id[i].ct))
			goto err_node_put;
		if (of_property_read_u32(cnp,
					"part", &flash->flash_dev_id[i].part))
			goto err_node_put;
		snprintf(flash->flash_dev_id[i].name, FLASHLIGHT_NAME_SIZE,
				flash->subdev_led[i].name);
		flash->flash_dev_id[i].channel = i;
		flash->flash_dev_id[i].decouple = decouple;

		pr_info("Parse dt (type,ct,part,name,channel,decouple)=(%d,%d,%d,%s,%d,%d).\n",
				flash->flash_dev_id[i].type,
				flash->flash_dev_id[i].ct,
				flash->flash_dev_id[i].part,
				flash->flash_dev_id[i].name,
				flash->flash_dev_id[i].channel,
				flash->flash_dev_id[i].decouple);
		if (flashlight_dev_register_by_device_id(&flash->flash_dev_id[i],
			&sc6607_24700_flash_ops))
			return -EFAULT;
		i++;
	}

	return 0;

err_node_put:
	of_node_put(cnp);
	return -EINVAL;
}

static int sc6607_24700_probe(struct i2c_client *client)
{
	struct sc6607_24700_flash *flash;
	struct sc6607_24700_platform_data *pdata = dev_get_platdata(&client->dev);
	int rval;

	pr_info("%s:%d", __func__, __LINE__);

	client->addr = sc6607_24700_I2C_ADDR;

	flash = devm_kzalloc(&client->dev, sizeof(*flash), GFP_KERNEL);
	if (flash == NULL)
		return -ENOMEM;

	flash->regmap = devm_regmap_init_i2c(client, &sc6607_24700_regmap);
	if (IS_ERR(flash->regmap)) {
		rval = PTR_ERR(flash->regmap);
		return rval;
	}

	/* if there is no platform data, use chip default value */
	if (pdata == NULL) {
		pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
		if (pdata == NULL)
			return -ENODEV;
		pdata->max_flash_timeout = sc6607_24700_FLASH_TOUT_MAX;
		/* led 1 */
		pdata->max_flash_brt[sc6607_24700_LED0] = sc6607_24700_FLASH_BRT_MAX;
		pdata->max_torch_brt[sc6607_24700_LED0] = sc6607_24700_TORCH_BRT_MAX;
		/* led 2 */
		pdata->max_flash_brt[sc6607_24700_LED1] = sc6607_24700_FLASH_BRT_MAX;
		pdata->max_torch_brt[sc6607_24700_LED1] = sc6607_24700_TORCH_BRT_MAX;
	}
	flash->pdata = pdata;
	flash->dev = &client->dev;
	mutex_init(&flash->lock);
	sc6607_24700_flash_data = flash;

/* 	rval = sc6607_24700_pinctrl_init(flash);
	if (rval < 0)
		return rval; */

	rval = sc6607_24700_subdev_init(flash, sc6607_24700_LED0, "sc6607_24700-led0");
	if (rval < 0)
		return rval;

/* 	rval = sc6607_24700_subdev_init(flash, sc6607_24700_LED1, "sc6607_24700-led1");
	if (rval < 0)
		return rval; */

	pm_runtime_enable(flash->dev);

	rval = sc6607_24700_parse_dt(flash);

	i2c_set_clientdata(client, flash);

	/* init work queue */
	INIT_WORK(&oris_work_ch1, oris_work_disable_ch1);
	INIT_WORK(&oris_work_ch2, oris_work_disable_ch2);
	
	/* init timer */
	hrtimer_init(&oris_timer_ch1, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	oris_timer_ch1.function = oris_timer_func_ch1;
    hrtimer_init(&oris_timer_ch2, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	oris_timer_ch2.function = oris_timer_func_ch2;
	oris_timeout_ms[ORIS_CHANNEL_CH1] = 100;
	oris_timeout_ms[ORIS_CHANNEL_CH2] = 100;

	oris_current = sc6607_24700_current;
	oris_torch_level = sc6607_24700_torch_level;
	oris_flash_level = sc6607_24700_flash_level;


	pr_info("%s:%d", __func__, __LINE__);
	return 0;
}

static void sc6607_24700_remove(struct i2c_client *client)
{
	struct sc6607_24700_flash *flash = i2c_get_clientdata(client);
	unsigned int i;
	pr_info("%s:%d", __func__, __LINE__);
	for (i = sc6607_24700_LED0; i < sc6607_24700_LED_MAX; i++) {
		v4l2_device_unregister_subdev(&flash->subdev_led[i]);
		v4l2_ctrl_handler_free(&flash->ctrls_led[i]);
		media_entity_cleanup(&flash->subdev_led[i].entity);
	}

	pm_runtime_disable(&client->dev);

	pm_runtime_set_suspended(&client->dev);
}

static int __maybe_unused sc6607_24700_suspend(struct device *dev)
{
	//struct i2c_client *client = to_i2c_client(dev);
	//struct sc6607_24700_flash *flash = i2c_get_clientdata(client);

	pr_info("%s %d", __func__, __LINE__);

	return 0;
}

static int __maybe_unused sc6607_24700_resume(struct device *dev)
{
	//struct i2c_client *client = to_i2c_client(dev);
	//struct sc6607_24700_flash *flash = i2c_get_clientdata(client);

	pr_info("%s %d", __func__, __LINE__);

	return 0;
}

static const struct i2c_device_id sc6607_24700_id_table[] = {
	{sc6607_24700_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, sc6607_24700_id_table);

static const struct of_device_id sc6607_24700_of_table[] = {
	{ .compatible = "mediatek,sc6607_24700" },
	{ },
};
MODULE_DEVICE_TABLE(of, sc6607_24700_of_table);

static const struct dev_pm_ops sc6607_24700_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
	SET_RUNTIME_PM_OPS(sc6607_24700_suspend, sc6607_24700_resume, NULL)
};

static struct i2c_driver sc6607_24700_i2c_driver = {
	.driver = {
		   .name = sc6607_24700_NAME,
		   .pm = &sc6607_24700_pm_ops,
		   .of_match_table = sc6607_24700_of_table,
		   },
	.probe = sc6607_24700_probe,
	.remove = sc6607_24700_remove,
	.id_table = sc6607_24700_id_table,
};

module_i2c_driver(sc6607_24700_i2c_driver);

MODULE_AUTHOR("XXX");
MODULE_DESCRIPTION("sc6607_24700 LED flash v4l2 driver");
MODULE_LICENSE("GPL");
