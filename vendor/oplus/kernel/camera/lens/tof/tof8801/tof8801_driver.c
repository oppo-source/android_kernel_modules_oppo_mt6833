// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm_runtime.h>
#include <linux/firmware.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <linux/kfifo.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/sysfs.h>
#include <linux/spinlock.h>
#include <linux/atomic.h>
#include "include/tof8801.h"
#include "tof8801_driver.h"

#define DRIVER_NAME "tof8801"
#define TOF8801_I2C_SLAVE_ADDR			0x18

#define LOG_INF(format, args...) \
		pr_info(DRIVER_NAME " [%s] " format, __func__, ##args)

#define LOG_ERR(format, args...) \
		pr_err(DRIVER_NAME " [%s] " format, __func__, ##args)

#define TOF_LOG_IF(cond, ...)	do { if ( (cond) ) { LOG_INF(__VA_ARGS__); } }while(0)

//#define AMS_MUTEX_DEBUG
#ifdef AMS_MUTEX_DEBUG
#define AMS_MUTEX_LOCK(m) { \
	pr_info("%s: Mutex Lock\n", __func__); \
	mutex_lock_interruptible(m); \
}
#define AMS_MUTEX_UNLOCK(m) { \
	pr_info("%s: Mutex Unlock\n", __func__); \
	mutex_unlock(m); \
	}
#else
#define AMS_MUTEX_LOCK(m) { \
	mutex_lock(m); \
}
#define AMS_MUTEX_UNLOCK(m) { \
	mutex_unlock(m); \
}
#endif
//#define AMS_OEM_MUTEX_DEBUG
#ifdef AMS_OEM_MUTEX_DEBUG
#define AMS_MUTEX_OEM_LOCK(m) { \
	pr_info("%s: OEMMutex Lock\n", __func__); \
	mutex_lock_interruptible(m); \
}
#define AMS_MUTEX_OEM_UNLOCK(m) { \
	pr_info("%s: OEMMutex Unlock\n", __func__); \
	mutex_unlock(m); \
}
#else
#define AMS_MUTEX_OEM_LOCK(m) { \
	mutex_lock(m); \
}
#define AMS_MUTEX_OEM_UNLOCK(m) { \
	mutex_unlock(m); \
}
#endif
/* This is the salt used for decryption on an encrypted sensor */
static char tof_salt_value = TOF8801_BL_DEFAULT_SALT;

static const unsigned long tof_irq_flags[] = {
	IRQ_TYPE_EDGE_RISING,
	IRQ_TYPE_EDGE_FALLING,
	IRQ_TYPE_LEVEL_LOW,
	IRQ_TYPE_LEVEL_HIGH,
};

static struct tof8801_platform_data tof_pdata = {
	.tof_name = "tof8801",
	.fac_calib_data_fname = "tof8801_fac_calib.bin",
	.config_calib_data_fname = "tof8801_config_calib.bin",
	.ram_patch_fname = {
		"tof8801_firmware.bin",
		"tof8801_firmware-1.bin",
		"tof8801_firmware-2.bin",
		NULL,
	},
};

static struct tof_sensor_chip * g_tof_sensor_chip = NULL;
static bool is_alread_probe = 0;
static int g_error_index = 0;
bool g_is_alread_runing = 0;			// tof have start
bool g_is_download_fw = 0;				// fw have download
bool g_dump_tof_registers = FALSE;
bool stop_tof_data = FALSE;
bool g_is_need_recover = FALSE;
atomic_t g_data_valid = ATOMIC_INIT(0);
atomic_t g_recover_process = ATOMIC_INIT(0);

static struct task_struct *async_task_recover;

#define TOF8801_NAME				"oplus-laser-tof8801"

#define TOF8801_CTRL_DELAY_US			2000

#define TOF8801_ENABLECONTROL		0

#define TOF8801_POWERCONTROL	 	0

#define TOF8801_DELAY_COUNT			11

/* tof8801 device structure */
struct tof8801_device {
	int driver_init;
	struct v4l2_ctrl_handler ctrls;
	struct v4l2_subdev sd;
	struct v4l2_ctrl *focus;
	struct regulator *vin;
	struct regulator *vdd;
	struct pinctrl *vcamaf_pinctrl;
	struct pinctrl_state *vcamaf_on;
	struct pinctrl_state *vcamaf_off;
};

#define NUMBER_OF_MAX_TOF_DATA 1
struct Oplus_TofInfo {
	int32_t is_tof_supported;
	int32_t num_of_rows; /* Max : 8 */
	int32_t num_of_cols; /* Max : 8 */
	int32_t ranging_distance[NUMBER_OF_MAX_TOF_DATA];
	int32_t dmax_distance[NUMBER_OF_MAX_TOF_DATA];
	int32_t error_status[NUMBER_OF_MAX_TOF_DATA];
	int32_t maximal_distance; /* Operating Range Distance */
	int64_t timestamp;
	int32_t SignalRate[NUMBER_OF_MAX_TOF_DATA];
	int32_t AmbientRate[NUMBER_OF_MAX_TOF_DATA];
	int32_t tof_id;
	int32_t fov_d;
	int32_t confidence;
	int32_t result_cnt;
	uint16_t histogram[TOF_HISTOGRAM_SUM];
};

static int dbg = 1;

struct mtk_oplus_tof_info {
	struct Oplus_TofInfo *p_oplus_tof_info;
};

struct oplus_tof_get_info {
	struct Oplus_TofGetinfo* p_oplus_tof_get_info;
};

struct Oplus_TofGetinfo {
	char trim_data[AMS_TOF_TRIM_DATA_SIZE];
	char xtalk_data[AMS_TOF_XTALK_DATA_SIZE];
	char calibration_data[AMS_TOF_CALIBRATION_DATA_SIZE];
};


/*
 *
 * Function Declarations
 *
 */
static void tof_ram_patch_callback(const struct firmware *cfg, void *ctx);
static int tof_switch_apps(struct tof_sensor_chip * chip, char req_app_id);
static int tof8801_get_config_calib_data(struct tof_sensor_chip *chip);
static int tof8801_firmware_download(struct tof_sensor_chip *chip, int startup);
static irqreturn_t tof_irq_handler(int irq, void *dev_id);
static int tof8801_enable_interrupts(struct tof_sensor_chip *chip, char int_en_flags);
static int tof8801_app0_poll_irq_thread(void *tof_chip);
static int start_poll_thread(void);
static int tof_oem_start(void);
static int tof_stop(void);
/*
 *
 * Function Definitions
 *
 */

/* Control commnad */
#define VIDIOC_MTK_G_TOF_INIT			_IOWR('V', BASE_VIDIOC_PRIVATE + 3, struct mtk_oplus_tof_info)
#define VIDIOC_MTK_G_TOF_INFO			_IOWR('V', BASE_VIDIOC_PRIVATE + 4, struct mtk_oplus_tof_info)
#define VIDIOC_OPLUS_S_TOF_CAPTURE		_IOW('V', BASE_VIDIOC_PRIVATE + 5, unsigned int)
#define VIDIOC_OPLUS_S_TOF_PERIOD		_IOW('V', BASE_VIDIOC_PRIVATE + 6, char *)
#define VIDIOC_OPLUS_S_TOF_ITERATIONS	_IOW('V', BASE_VIDIOC_PRIVATE + 7, char *)
#define VIDIOC_OPLUS_S_TOF_ALGSETTING	_IOW('V', BASE_VIDIOC_PRIVATE + 8, char *)
#define VIDIOC_OPLUS_S_TOF_CALIB		_IOW('V', BASE_VIDIOC_PRIVATE + 9, char [20])
#define VIDIOC_OPLUS_G_TOF_CALIB		_IOWR('V', BASE_VIDIOC_PRIVATE + 10, struct oplus_tof_get_info)
#define VIDIOC_OPLUS_S_TOF_TRIM			_IOWR('V', BASE_VIDIOC_PRIVATE + 11, char *)
#define VIDIOC_OPLUS_G_TOF_TRIM			_IOWR('V', BASE_VIDIOC_PRIVATE + 12, struct oplus_tof_get_info)
#define VIDIOC_OPLUS_G_TOF_XTALK		_IOWR('V', BASE_VIDIOC_PRIVATE + 13, struct oplus_tof_get_info)
#define VIDIOC_OPLUS_S_TOF_STOP			_IOW('V', BASE_VIDIOC_PRIVATE + 14, unsigned int)
#define VIDIOC_OPLUS_S_TOF_DEBUG		_IOW('V', BASE_VIDIOC_PRIVATE + 15, char *)
#define VIDIOC_OPLUS_S_TOF_CHIP_ENABLE	_IOW('V', BASE_VIDIOC_PRIVATE + 16, char *)
#define VIDIOC_OPLUS_S_TOF_HISTOGRAM_ENABLE	_IOW('V', BASE_VIDIOC_PRIVATE + 17, unsigned int)

static ssize_t chip_enable_store(unsigned int capture)
{
	int rc = -1;
	struct tof_sensor_chip *chip = g_tof_sensor_chip;

	if (capture) {
		AMS_MUTEX_LOCK(&chip->lock)
		if(g_is_alread_runing) {
			AMS_MUTEX_UNLOCK(&chip->lock);
			return 0;
		}
		g_is_alread_runing=1;
		AMS_MUTEX_UNLOCK(&chip->lock);
		AMS_MUTEX_LOCK(&chip->state_lock);
		rc = tof_oem_start();
		AMS_MUTEX_UNLOCK(&chip->state_lock);
		if(rc != 0) {
			LOG_INF("start tof failed");
		}
	} else {
		AMS_MUTEX_LOCK(&chip->lock)
		if(g_is_alread_runing == 0) {
			AMS_MUTEX_UNLOCK(&chip->lock);
			return 0;
		}
		g_is_alread_runing = 0;
		AMS_MUTEX_UNLOCK(&chip->lock);
		AMS_MUTEX_LOCK(&chip->state_lock);
		rc = tof_stop();
		AMS_MUTEX_UNLOCK(&chip->state_lock);
		if(rc != 0) {
			LOG_INF("stop tof failed");
		}
	}
	return 0;
}

static ssize_t capture_store(unsigned int capture)
{
	struct tof_sensor_chip *chip = g_tof_sensor_chip;
	int error = 0;
	LOG_INF("%s: %d", __func__, capture);
	AMS_MUTEX_LOCK(&chip->lock);
	if (chip->info_rec.record.app_id != TOF8801_APP_ID_APP0) {
		LOG_INF("%s: Error ToF chip app_id: %#x",
				__func__, chip->info_rec.record.app_id);
		g_is_need_recover = true;
		AMS_MUTEX_UNLOCK(&chip->lock);
		return -1;
	}
	if (capture) {
		if (chip->app0_app.cap_settings.cmd == 0) {
			error = tof8801_app0_capture((void *)chip, capture);
		} else {
			AMS_MUTEX_UNLOCK(&chip->lock);
			return -EBUSY;
		}
	} else {
		tof8801_app0_capture(chip, 0);
	}
	AMS_MUTEX_UNLOCK(&chip->lock);

	return error ? -1 : 0;
}

static ssize_t stop_data_store(unsigned int debug)
{
	struct tof_sensor_chip *chip = g_tof_sensor_chip;
	LOG_INF("%s\n", __func__);
	if (debug == 0) {
		stop_tof_data = TRUE;
		AMS_MUTEX_OEM_LOCK(&chip->irq_lock);
		if (chip->poll_period != 0 && chip->irq_thread_status == TOF_IRQ_THREAD_START) {
			(void)kthread_stop(chip->app0_poll_irq);
		}
		chip->irq_thread_status = TOF_IRQ_THREAD_STOP;
		AMS_MUTEX_OEM_UNLOCK(&chip->irq_lock);
		LOG_INF("stop irq threadd done");

	} else {
		stop_tof_data = FALSE;
		start_poll_thread();
		LOG_INF("reset stop_tof_data");
	}
	return 0;
}

static ssize_t driver_debug_store(const char * buf, size_t count)
{
	struct tof_sensor_chip *chip = g_tof_sensor_chip;
	int debug;
	LOG_INF("%s\n", __func__);
	sscanf(buf, "%d", &debug);
	if (debug == 0) {
		chip->driver_debug = 0;
		g_dump_tof_registers = 0;
	} else if (debug == 1) {
		chip->driver_debug = 1;
		g_dump_tof_registers = 0;
	} else if (debug == 2) {
		chip->driver_debug = 2;
		g_dump_tof_registers = 1;
	}
	return count;
}

// static ssize_t get_period(char * buf)
// {
//	 unsigned int len = 0;
//	 struct tof_sensor_chip *chip = g_tof_sensor_chip;
//	 LOG_INF("%s\n", __func__);
//	 AMS_MUTEX_LOCK(&chip->lock);
//	 if (chip->info_rec.record.app_id != TOF8801_APP_ID_APP0) {
//		 LOG_ERR("%s: Error ToF chip app_id: %#x",
//						 __func__, chip->info_rec.record.app_id);
//		 AMS_MUTEX_UNLOCK(&chip->lock);
//		 return -1;
//	 }
//	 len += scnprintf(buf, PAGE_SIZE, "%d\n", chip->app0_app.cap_settings.period);
//	 AMS_MUTEX_UNLOCK(&chip->lock);
//	 return len;
// }

static ssize_t period_store(const char * buf)
{
	struct tof_sensor_chip *chip = g_tof_sensor_chip;
	int error = 0;
	unsigned int value = 0;
	LOG_INF("%s: %s", __func__, buf);
	AMS_MUTEX_LOCK(&chip->lock);
	if (chip->info_rec.record.app_id != TOF8801_APP_ID_APP0) {
		LOG_ERR("%s: Error ToF chip app_id: %#x",
						__func__, chip->info_rec.record.app_id);
		AMS_MUTEX_UNLOCK(&chip->lock);
		return -1;
	}
	if (sscanf(buf, "%u", &value) != 1) {
		AMS_MUTEX_UNLOCK(&chip->lock);
		return -1;
	}
	chip->app0_app.cap_settings.period = (value > 0xFF) ? 0xFF : value;
	if (chip->app0_app.cap_settings.cmd != 0) {
		(void)tof8801_app0_capture((void*)chip, 0);
		error = tof8801_app0_capture((void*)chip, 1);
	}
	AMS_MUTEX_UNLOCK(&chip->lock);
	return error ? -1 : 0;
}

// static ssize_t iterations_show(char * buf)
// {
//	 unsigned int len = 0;
//	 unsigned int iterations = 0;
//	 struct tof_sensor_chip *chip = g_tof_sensor_chip;
//	 LOG_INF("%s\n", __func__);
//	 AMS_MUTEX_LOCK(&chip->lock);
//	 if (chip->info_rec.record.app_id != TOF8801_APP_ID_APP0) {
//		 LOG_ERR("%s: Error ToF chip app_id: %#x",
//						 __func__, chip->info_rec.record.app_id);
//		 AMS_MUTEX_UNLOCK(&chip->lock);
//		 return -1;
//	 }
//	 iterations = 1000 * le16_to_cpup((const __le16 *)chip->app0_app.cap_settings.iterations);
//	 len += scnprintf(buf, PAGE_SIZE, "%u\n", iterations);
//	 AMS_MUTEX_UNLOCK(&chip->lock);
//	 return len;
// }

static ssize_t iterations_store(const char * buf, size_t count)
{
	struct tof_sensor_chip *chip = g_tof_sensor_chip;
	int error = 0;
	unsigned int value = 0;
	LOG_INF("%s: %s", __func__, buf);
	AMS_MUTEX_LOCK(&chip->lock);
	if (chip->info_rec.record.app_id != TOF8801_APP_ID_APP0) {
		LOG_ERR("%s: Error ToF chip app_id: %#x",
						__func__, chip->info_rec.record.app_id);
		AMS_MUTEX_UNLOCK(&chip->lock);
		return -1;
	}
	if (sscanf(buf, "%u", &value) != 1) {
		AMS_MUTEX_UNLOCK(&chip->lock);
		return -1;
	}
	// we need to appropriately change the clock iteration counter
	//	when the capture iterations are changed to keep the time acceptable
	tof8801_app0_set_clk_iterations(chip, value);
	// chip takes iterations in 1000s
	value /= 1000;
	*((__le16 *)chip->app0_app.cap_settings.iterations) = cpu_to_le16(value);
	if (chip->app0_app.cap_settings.cmd != 0) {
		(void)tof8801_app0_capture((void*)chip, 0);
		error = tof8801_app0_capture((void*)chip, 1);
	}
	AMS_MUTEX_UNLOCK(&chip->lock);
	return error ? -1 : count;
}

// static ssize_t alg_setting_show(char * buf)
// {
//	 unsigned int len = 0;
//	 struct tof_sensor_chip *chip = g_tof_sensor_chip;
//	 LOG_INF("%s\n", __func__);
//	 AMS_MUTEX_LOCK(&chip->lock);
//	 if (chip->info_rec.record.app_id != TOF8801_APP_ID_APP0) {
//		 LOG_ERR("%s: Error ToF chip app_id: %#x",
//						 __func__, chip->info_rec.record.app_id);
//		 AMS_MUTEX_UNLOCK(&chip->lock);
//		 return -1;
//	 }
//	 if (!tof8801_app0_is_v2(chip)) {
//		 LOG_ERR("%s: Error alg setting not supported in revision: %#x",
//						 __func__, chip->info_rec.record.app_ver);
//		 AMS_MUTEX_UNLOCK(&chip->lock);
//		 return -1;
//	 }
//	 len += scnprintf(buf, PAGE_SIZE, "%x\n", chip->app0_app.cap_settings.v2.alg);
//	 AMS_MUTEX_UNLOCK(&chip->lock);
//	 return len;
// }

static ssize_t alg_setting_store(const char * buf, size_t count)
{
	struct tof_sensor_chip *chip = g_tof_sensor_chip;
	int error = 0;
	LOG_INF("%s: %s", __func__, buf);
	AMS_MUTEX_LOCK(&chip->lock);
	if (chip->info_rec.record.app_id != TOF8801_APP_ID_APP0) {
		LOG_ERR("%s: Error ToF chip app_id: %#x",
						__func__, chip->info_rec.record.app_id);
		AMS_MUTEX_UNLOCK(&chip->lock);
		return -1;
	}
	if (!tof8801_app0_is_v2(chip)) {
		LOG_ERR("%s: Error alg setting not supported in revision: %#x",
						__func__, chip->info_rec.record.app_ver);
		AMS_MUTEX_UNLOCK(&chip->lock);
		return -1;
	}
	if (sscanf(buf, "%hhx", &chip->app0_app.cap_settings.v2.alg) != 1) {
		AMS_MUTEX_UNLOCK(&chip->lock);
		return -1;
	}
	if (chip->app0_app.cap_settings.cmd != 0) {
		(void)tof8801_app0_capture((void*)chip, 0);
		error = tof8801_app0_capture((void*)chip, 1);
	}
	AMS_MUTEX_UNLOCK(&chip->lock);
	return error ? -1 : count;
}

static ssize_t app0_read_peak_crosstalk_show(char *buf)
{
	unsigned int len = 0;
	struct tof_sensor_chip *chip = g_tof_sensor_chip;

	LOG_INF("%s\n", __func__);
	AMS_MUTEX_LOCK(&chip->lock);
	if (chip->info_rec.record.app_id != TOF8801_APP_ID_APP0) {
		LOG_ERR("%s: Error ToF chip app_id: %#x",
				__func__, chip->info_rec.record.app_id);
		AMS_MUTEX_UNLOCK(&chip->lock);
		return -EIO;
	}

	LOG_INF("xtalk=%d \n", chip->xtalk_peak);
	len = scnprintf(buf, PAGE_SIZE, "%u\n", chip->xtalk_peak);

	AMS_MUTEX_UNLOCK(&chip->lock);
	return len;
}

// static ssize_t app0_apply_fac_calib_show(char * buf)
// {
//	 struct tof_sensor_chip *chip = g_tof_sensor_chip;
//	 int i;
//	 int len = 0;
//	 char *tmpbuf = (char *)&chip->ext_calib_data.fac_data;
//	 LOG_INF("%s\n", __func__);
//	 AMS_MUTEX_LOCK(&chip->lock);
//	 if (chip->info_rec.record.app_id != TOF8801_APP_ID_APP0) {
//		 LOG_ERR("%s: Error ToF chip app_id: %#x",
//						 __func__, chip->info_rec.record.app_id);
//		 AMS_MUTEX_UNLOCK(&chip->lock);
//		 return -1;
//	 }
//	 for (i = 0; i < chip->ext_calib_data.size; i++) {
//		 len += scnprintf(buf + len, PAGE_SIZE - len, "fac_calib[%d]:%02x\n",
//											i, tmpbuf[i]);
//	 }
//	 AMS_MUTEX_UNLOCK(&chip->lock);
//	 return len;
// }

static ssize_t app0_apply_fac_calib_store(const char * buf, size_t count)
{
	struct tof_sensor_chip *chip = g_tof_sensor_chip;
	int fw_size = 14;
	//unsigned char fw_data[14];
	int i=0;

	LOG_INF("%s\n", __func__);
	AMS_MUTEX_LOCK(&chip->lock);
	if (chip->info_rec.record.app_id != TOF8801_APP_ID_APP0) {
		LOG_ERR("%s: Error ToF chip app_id: %#x",
				__func__, chip->info_rec.record.app_id);
		AMS_MUTEX_UNLOCK(&chip->lock);
		return -1;
	}

	chip->ext_calib_data.size = 0;
	chip->alg_info.size = 0;
	/*fw_size = sscanf(buf, "0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x",
				 &fw_data[0], &fw_data[1], &fw_data[2], &fw_data[3],
				 &fw_data[4], &fw_data[5], &fw_data[6], &fw_data[7],
				 &fw_data[8], &fw_data[9], &fw_data[10],&fw_data[11],
				 &fw_data[12],&fw_data[13]);*/
	memcpy((void *)&chip->ext_calib_data.fac_data,
			 buf, fw_size);
	chip->ext_calib_data.size = fw_size;

	/*if (!fw_size) {
	CAM_WARN(CAM_TOF, "factory calibration data not available \'%s\\n",
			 chip->pdata->fac_calib_data_fname);
	return 0;
	} else {
	LOG_INF("Read in fac_calib file: \'%s\'. size is %d\n",
			 chip->pdata->fac_calib_data_fname,fw_size);
	}*/
	for(i=0; i< fw_size; i++)
		LOG_INF("factory calib i=%d data=0x%x", i, chip->ext_calib_data.fac_data.data[i]);
	//set flag to update fac calib on next measure
	chip->app0_app.cal_update.dataFactoryConfig = 1;
	AMS_MUTEX_UNLOCK(&chip->lock);
	return count;
}

static ssize_t app0_get_fac_calib_show(char * buf)
{

	struct tof_sensor_chip *chip = g_tof_sensor_chip;
	int error;
	u32 len;
	int i;
	unsigned long start = jiffies;
	int timeout_flag = 0;
	AMS_MUTEX_LOCK(&chip->lock);
	LOG_INF("%s\n", __func__);
	if (chip->info_rec.record.app_id != TOF8801_APP_ID_APP0) {
		LOG_INF("%s: Error ToF chip app_id: %#x",
				 __func__, chip->info_rec.record.app_id);
		AMS_MUTEX_UNLOCK(&chip->lock);
		return -1;
	}
	//Stop any in-progress measurements
	(void) tof8801_app0_capture(chip, 0);
	error = tof8801_app0_perform_factory_calibration(chip);
	if (error) {
		LOG_ERR("Error starting factory calibration routine: %d", error);
		AMS_MUTEX_UNLOCK(&chip->lock);
		return 0;
	}
	do {
		//spin here waiting for factory calibration to complete
		AMS_MUTEX_UNLOCK(&chip->lock);
		msleep(100);
		AMS_MUTEX_LOCK(&chip->lock);
		if (chip->app0_app.cap_settings.period != 0) {
			uint8_t buf = 0;
			tof_i2c_read(chip->client, OL_REGISTER_CONTENTS_OFFSET, (char *)&buf, 1);
			LOG_INF("performing factory calibration, buf %u", buf);
			if (buf == OL_COMMAND_FACTORY_CALIB) {
					int error = tof_i2c_read(chip->client, OL_FACTORY_CALIB_OFFSET,
															(char *)&chip->ext_calib_data.fac_data,
															sizeof(chip->ext_calib_data.fac_data));
					if (!error) {
						chip->app0_app.measure_in_prog = 0;
						chip->ext_calib_data.size = sizeof(chip->ext_calib_data.fac_data);
					} else {
						chip->ext_calib_data.size = 0;
					}
					chip->app0_app.cal_update.dataFactoryConfig = 1;
			}
		}
		timeout_flag = ((jiffies - start) >= msecs_to_jiffies(APP0_FAC_CALIB_MSEC_TIMEOUT));
	} while(!timeout_flag && tof8801_app0_measure_in_progress(chip));
	// read the calibrated data
	if (!tof8801_app0_measure_in_progress(chip) &&
			chip->app0_app.cal_update.dataFactoryConfig) {
		// If calib measure complete and was successful
		if (chip->ext_calib_data.size) {
			memcpy(buf, (void *)&chip->ext_calib_data.fac_data, chip->ext_calib_data.size);
		}
		len = chip->ext_calib_data.size;
		buf[len] = 0; //output is a string so we need to add null-terminating character
		LOG_INF("Done performing factory calibration, size: %u", len);
	} else {
		LOG_ERR("Error timeout waiting on factory calibration");
		AMS_MUTEX_UNLOCK(&chip->lock);
		return 0;
	}
	for (i = 0; i < len; i++) {
		LOG_INF("bin[%d]:0x%x", i, ((char *)&chip->ext_calib_data.fac_data)[i]);
	}
	AMS_MUTEX_UNLOCK(&chip->lock);
	return len;
}

static ssize_t app0_clk_trim_set_show(char * buf)
{
	unsigned int len = 0;
	struct tof_sensor_chip *chip = g_tof_sensor_chip;
	int trim = 0;
	int error = 0;
	LOG_INF("%s\n", __func__);
	AMS_MUTEX_LOCK(&chip->lock);
	if (chip->info_rec.record.app_id != TOF8801_APP_ID_APP0) {
		LOG_ERR("%s: Error ToF chip app_id: %#x",
				__func__, chip->info_rec.record.app_id);
		AMS_MUTEX_UNLOCK(&chip->lock);
		return -1;
	}
	if (!tof8801_app0_is_v2(chip)) {
		LOG_ERR("%s: Error clk trim not supported in revision: %#x",
				__func__, chip->info_rec.record.app_ver);
		AMS_MUTEX_UNLOCK(&chip->lock);
		return -1;
	}
	error = tof8801_app0_rw_osc_trim(chip, &trim, 0);
	if (error) {
		AMS_MUTEX_UNLOCK(&chip->lock);
		return -1;
	}
	len += scnprintf(buf, PAGE_SIZE, "%d\n", trim);
	AMS_MUTEX_UNLOCK(&chip->lock);
	return len;
}

static ssize_t app0_clk_trim_set_store(const char * buf, size_t count)
{
	struct tof_sensor_chip *chip = g_tof_sensor_chip;
	int trim = 0;
	LOG_INF("%s: %s", __func__, buf);
	AMS_MUTEX_LOCK(&chip->lock);
	if (sscanf(buf, "%d", &trim) != 1) {
		AMS_MUTEX_UNLOCK(&chip->lock);
		return -1;
	}
	LOG_INF("%s: trim = 0x%08x\n", __func__, trim);
	if ((trim > 255) || (trim < -256)) {
		LOG_ERR("%s: Error clk trim setting is out of range [%d,%d]\n", __func__, 255, -256);
		AMS_MUTEX_UNLOCK(&chip->lock);
		return -1;
	}
	chip->saved_clk_trim = trim; // cache value even if app0 is not running
	if (chip->info_rec.record.app_id != TOF8801_APP_ID_APP0) {
		LOG_ERR("%s: Caching trim value, ToF chip app_id: %#x",
				__func__, chip->info_rec.record.app_id);
		AMS_MUTEX_UNLOCK(&chip->lock);
		return count;
	}
	if (!tof8801_app0_is_v2(chip)) {
		LOG_ERR("%s: Error clk trim not supported in revision: %#x",
				__func__, chip->info_rec.record.app_ver);
		AMS_MUTEX_UNLOCK(&chip->lock);
		return -1;
	}
	if (tof8801_app0_rw_osc_trim(chip, &trim, 1)) {
		LOG_ERR("%s: Error setting clock trimming\n", __func__);
		AMS_MUTEX_UNLOCK(&chip->lock);
		return -1;
	}
	AMS_MUTEX_UNLOCK(&chip->lock);
	return count;
}

/****************************************************************************
 * Common Sysfs Attributes
 * **************************************************************************/

/****************************************************************************
 * Bootloader Sysfs Attributes
 * **************************************************************************/


static inline struct tof8801_device *to_tof8801_ois(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct tof8801_device, ctrls);
}

static inline struct tof8801_device *sd_to_tof8801_ois(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct tof8801_device, sd);
}

static int tof8801_release(struct tof8801_device *tof8801)
{
	return 0;
}

int tof_queue_frame(struct tof_sensor_chip *chip, void *buf, int size)
{
	unsigned int fifo_len;
	unsigned int frame_size;
	int result = kfifo_in(&chip->tof_output_fifo, buf, size);
	if (result == 0) {
		if (chip->driver_debug == 1)
			LOG_ERR("Error: Frame buffer is full, clearing buffer.\n");
		kfifo_reset(&chip->tof_output_fifo);
		tof8801_app0_report_error(chip, ERR_BUF_OVERFLOW, DEV_OK);
		result = kfifo_in(&chip->tof_output_fifo, buf, size);
		if (result == 0) {
			LOG_ERR("Error: queueing ToF output frame.\n");
		}
	}
	if (chip->driver_debug == 2) {
		fifo_len = kfifo_len(&chip->tof_output_fifo);
		frame_size = ((char *)buf)[DRV_FRAME_SIZE_LSB] |
					 (((char *)buf)[DRV_FRAME_SIZE_MSB] << 8);
		LOG_INF("Add frame_id: 0x%x, data_size: %u\n",
				 ((char *)buf)[DRV_FRAME_ID], frame_size);
		LOG_INF("New fifo len: %u, fifo utilization: %u%%\n",
				 fifo_len, (1000*fifo_len/kfifo_size(&chip->tof_output_fifo))/10);
	}
	return (result == size) ? 0 : -1;
}
/**
 * tof_init_info_record - initialize info record of currently running app
 *
 * @client: the i2c client
 * @record: pointer to info_record struct where data will be placed
 */
int tof_init_info_record(struct tof_sensor_chip *chip)
{
	int error;
	char data[sizeof(struct record)] = {0};

	error = tof_i2c_read(chip->client, TOF8801_APP_ID,
						 data, TOF8801_INFO_RECORD_SIZE);
	if (error) {
		LOG_INF("read record failed: %d\n", error);
		goto err;
	}

	memcpy(&chip->info_rec.record,data,sizeof(struct record));

	LOG_INF("Read info record - Running app_id: %#x.\n", chip->info_rec.record.app_id);
	/* re-initialize apps */
	if (chip->info_rec.record.app_id == TOF8801_APP_ID_BOOTLOADER) {
		tof8801_BL_init_app(&chip->BL_app);
	} else if (chip->info_rec.record.app_id == TOF8801_APP_ID_APP0) {
		tof8801_app0_init_app(&chip->app0_app);
	}
	return 0;
err:
	return error;
}

int tof_start_thread(void *tof_chip)
{
	struct tof_sensor_chip *chip = g_tof_sensor_chip;
	int error;
	int value = 900000;
	AMS_MUTEX_OEM_LOCK(&chip->power_lock);
	if (chip->power_status == TOF_POWER_ON) {
		AMS_MUTEX_OEM_UNLOCK(&chip->power_lock);
		LOG_INF("start tof thread init!");

		error = tof_hard_reset(chip);

		if (error) {
			LOG_INF("TOF Hrad reset failed !");
			return error;
		}
		AMS_MUTEX_LOCK(&chip->lock);
		chip->app0_app.cap_settings.period = 66;
		//chip->app0_app.cap_settings.v2.alg = 0xA7;
		// we need to appropriately change the clock iteration counter
		//	when the capture iterations are changed to keep the time acceptable
		tof8801_app0_set_clk_iterations(chip, value);
		// chip takes iterations in 1000s
		value /= 1000;
		*((__le16 *)chip->app0_app.cap_settings.iterations) = cpu_to_le16(value);
		//error = tof8801_app0_capture((void *)chip, 1);
		AMS_MUTEX_UNLOCK(&chip->lock);

		LOG_INF("end tof thread init done !	error = %d	",error);
	} else {
		LOG_INF("chip->power_status = %d	",chip->power_status);
		AMS_MUTEX_OEM_UNLOCK(&chip->power_lock);
	}

	//AMS_MUTEX_OEM_UNLOCK(&chip->oem_lock);

	return 0;

}

static int tof_oem_start(void)
{
	struct tof_sensor_chip *chip = g_tof_sensor_chip;
	/*** Use Polled I/O instead of interrupt ***/

	if (is_alread_probe == 1) {

		AMS_MUTEX_OEM_LOCK(&chip->oem_lock);
		LOG_INF("start tof download-fw thread.\n");

		memset(&chip->app0_app,0,sizeof(chip->app0_app));
		memset(&chip->BL_app,0,sizeof(chip->BL_app));
		kfifo_reset(&chip->tof_output_fifo);
		tof_start_thread(chip);
		TOF_LOG_IF(dbg,"dataAlgorithmState=%d.dataConfiguration=%d.\n",
				 chip->app0_app.cal_update.dataAlgorithmState,
				 chip->app0_app.cal_update.dataConfiguration);
		LOG_INF("end tof download-fw thread.\n");
		AMS_MUTEX_OEM_UNLOCK(&chip->oem_lock);

	}
	return 0;
}
/**
 * tof_hard_reset - use GPIO Chip Enable to reset the device
 *
 * @tof_chip: tof_sensor_chip pointer
 */
int tof_hard_reset(struct tof_sensor_chip *chip)
{
	int error = 0;

	struct i2c_client *client = tof_pdata.client;
	struct tof_sensor_chip *tof_chip = (struct tof_sensor_chip *)i2c_get_clientdata(client);

	// if (!chip->pdata->gpiod_enable) {
	// 	return -EIO;
	// }
	gpio_direction_output(chip->enable_gpio, 0);
	gpio_direction_output(chip->enable_gpio, 1);

#if 1
	error = tof_wait_for_cpu_startup(chip->client);
	if (error) {
		LOG_INF("I2C communication failure: %d\n", error);
		return error;
	}

	tof_chip->saved_clk_trim = UNINITIALIZED_CLK_TRIM_VAL;
	//read external (manufacturer) configuration data
	error = tof8801_get_config_calib_data(tof_chip);
	if (error) {
		LOG_INF("Error reading config data: %d\n", error);
	}

	tof8801_app0_default_cap_settings(&tof_chip->app0_app);

	error = tof_init_info_record(tof_chip);
	if (error) {
		LOG_INF("Read application info record failed.\n");
		return error;
	}

	/* enable all ToF interrupts on sensor */
	tof8801_enable_interrupts(tof_chip, IRQ_RESULTS | IRQ_DIAG | IRQ_ERROR);


	AMS_MUTEX_LOCK(&tof_chip->lock);
	if(!g_is_download_fw) {
		/***** Make firmware download available to user space *****/
		error = tof8801_firmware_download(tof_chip, 0);
		if (error) {
			AMS_MUTEX_UNLOCK(&tof_chip->lock);
			LOG_INF("fail to tof8801_firmware_download,rc = %d", error);
			// return error;
		}
	} else {
		LOG_INF("ams-tof fw have download");
	}

	tof_switch_apps(chip, (char)TOF8801_APP_ID_APP0);
	tof8801_enable_interrupts(tof_chip, IRQ_RESULTS | IRQ_DIAG | IRQ_ERROR);

	// tof8801_enable_pinctrl(&(client->dev));

	AMS_MUTEX_UNLOCK(&tof_chip->lock);

	error = start_poll_thread();
	if (error) {
		LOG_INF("start_poll_thread failed.\n");
		return error;
	}

#endif

	return error;
}

static int tof_switch_from_bootloader(struct tof_sensor_chip * chip, char req_app_id)
{
	int error = 0;
	char *new_app_id;

	// Try to perform RAM download (if possible)
	LOG_INF("start to tof8801_firmware_download	\n");
	error = tof8801_firmware_download(chip, 0);

	start_poll_thread();

	LOG_INF("end to tof8801_firmware_download \n");
	if (error != 0) {
		LOG_INF("tof_switch_from_bootloader tof_switch_from_bootloader error != 0	\n");

		//This means either there is no firmware, or there was a failure
		error = tof8801_set_register(chip->client, TOF8801_REQ_APP_ID, req_app_id);
		if (error) {
			LOG_INF("Error setting REQ_APP_ID register.\n");
			error = -EIO;
		}
		error = tof_wait_for_cpu_ready_timeout(chip->client, 100000);
		if (error) {
			LOG_INF("Error waiting for CPU ready flag.\n");
		}
		error = tof_init_info_record(chip);
		if (error) {
			LOG_INF("Error reading info record.\n");
		}
	}
	new_app_id = &chip->info_rec.record.app_id;
	LOG_INF("Running app_id: 0x%02x\n", *new_app_id);
	switch (*new_app_id) {
	case TOF8801_APP_ID_BOOTLOADER:
		LOG_INF("Error: application switch failed.\n");
		break;
	case TOF8801_APP_ID_APP0:
		/* enable all ToF interrupts on sensor */
		LOG_INF("tof_switch_from_bootloader tof8801_enable_interrupts start \n");
		tof8801_enable_interrupts(chip, IRQ_RESULTS | IRQ_DIAG | IRQ_ERROR);
		break;
	case TOF8801_APP_ID_APP1:
		break;
	default:
		LOG_INF("Error: Unrecognized application.\n");
		return -1;
	}
	return (*new_app_id == req_app_id) ? 0 : -1;
}
/**
 * tof8801_enable_interrupts - enable specified interrutps
 *
 * @tof_chip: tof_sensor_chip pointer
 * @int_en_flags: OR'd flags of interrupts to enable
 */
static int tof8801_enable_interrupts(struct tof_sensor_chip *chip, char int_en_flags)
{
	char flags;
	int error = tof8801_get_register(chip->client, TOF8801_INT_EN, &flags);
	flags &= TOF8801_INT_MASK;
	flags |= int_en_flags;
	if (error) {
		LOG_INF("Can't set tof8801 interrupts\n");
		return error;
	}
	return tof8801_set_register(chip->client, TOF8801_INT_EN, flags);
}
int tof_switch_apps(struct tof_sensor_chip *chip, char req_app_id)
{
	int error = 0;
	LOG_INF("entery tof_switch_apps ,chip->info_rec.record.app_id is %x",chip->info_rec.record.app_id);
	if (req_app_id == chip->info_rec.record.app_id) {
		LOG_INF("don't switch tof_switch_apps ,return ");
		return 0;
	}
	if ((req_app_id != TOF8801_APP_ID_BOOTLOADER) &&
		(req_app_id != TOF8801_APP_ID_APP0)	 &&
		(req_app_id != TOF8801_APP_ID_APP1)) {
		LOG_INF("without cmd tof_switch_apps ,return ");
		return -1;
	}
	LOG_INF("chip->info_rec.record.app_id is %x",chip->info_rec.record.app_id);
	switch (chip->info_rec.record.app_id) {
	case TOF8801_APP_ID_BOOTLOADER:
		LOG_INF("tof_switch_from_bootloader start -----");
		error = tof_switch_from_bootloader(chip, req_app_id);
		if (error) {
			/* Hard reset back to bootloader if error */
		 	gpio_direction_output(chip->enable_gpio, 0);
			g_is_download_fw = 0;
			gpio_direction_output(chip->enable_gpio, 1);
			LOG_INF("tof_wait_for_cpu_startup start -----");
			error = tof_wait_for_cpu_startup(chip->client);
			if (error) {
				LOG_INF("I2C communication failure: %d\n",
						error);
				return error;
			}
			error = tof_init_info_record(chip);
			if (error) {
				LOG_INF("Read application info record failed.\n");
				return error;
			}
			return -1;
		}
		break;
	case TOF8801_APP_ID_APP0:
		LOG_INF("switch	TOF8801_APP_ID_APP0 start -----");
		error = tof8801_app0_switch_to_bootloader(chip);
		break;
	case TOF8801_APP_ID_APP1:
		LOG_INF("switch	TOF8801_APP_ID_APP1 start -----");
		return -1;
		break;
	default:
		LOG_INF("without any match	cmd");
		error = -1;
		break;
	}
	return error;
}
int start_poll_thread(void)
{
	struct tof_sensor_chip *chip = g_tof_sensor_chip;
	int error = 0;
	AMS_MUTEX_LOCK(&chip->irq_lock);
	LOG_INF("start_poll_thread!");
	if (chip->irq_thread_status == TOF_IRQ_THREAD_STOP) {
		/*** Use Polled I/O instead of interrupt ***/
		chip->app0_poll_irq = kthread_run(tof8801_app0_poll_irq_thread,(void *)chip,"tof-irq_poll");

		if (IS_ERR(chip->app0_poll_irq)) {
			LOG_INF("Error starting IRQ polling thread.\n");
			error = PTR_ERR(chip->app0_poll_irq);
			AMS_MUTEX_UNLOCK(&chip->irq_lock)
			return error;
		}
	} else {
		LOG_INF("Error starting IRQ polling thread.,thread already exit \n");
	}
	chip->irq_thread_status = TOF_IRQ_THREAD_START;
	AMS_MUTEX_UNLOCK(&chip->irq_lock);
	return error ;
}
/**
 * tof_i2c_read - Read number of bytes starting at a specific address over I2C
 *
 * @client: the i2c client
 * @reg: the i2c register address
 * @buf: pointer to a buffer that will contain the received data
 * @len: number of bytes to read
 */
int tof_i2c_read(struct i2c_client *client, char reg, char *buf, int len)
{
	struct i2c_msg msgs[2];
	int ret=-1;

	msgs[0].flags = 0;
	msgs[0].addr	= client->addr;
	msgs[0].len	 = 1;
	msgs[0].buf	 = &reg;

	msgs[1].flags = I2C_M_RD;
	msgs[1].addr	= client->addr;
	msgs[1].len	 = len;
	msgs[1].buf	 = buf;

	ret = i2c_transfer(client->adapter, msgs, 2);
	return ret < 0 ? ret : (ret != ARRAY_SIZE(msgs) ? -EIO : 0);
}
/**
 * tof_irq_handler - The IRQ handler
 *
 * @irq: interrupt number.
 * @dev_id: private data pointer.
 */
static irqreturn_t tof_irq_handler(int irq, void *dev_id)
{
	struct tof_sensor_chip *tof_chip = (struct tof_sensor_chip *)dev_id;
	char int_stat = 0;
	char appid;
	unsigned char cpu_stat, state;
	int error;
	AMS_MUTEX_LOCK(&tof_chip->lock);
	//Go to appropriate IRQ handler depending on the app running
	appid = tof_chip->info_rec.record.app_id;
	switch(appid) {
		case TOF8801_APP_ID_BOOTLOADER:
			goto irq_handled;
		case TOF8801_APP_ID_APP0:
			(void)tof8801_get_register(tof_chip->client, TOF8801_INT_STAT, &int_stat);
			if (tof_chip->driver_debug) {
				dev_info(&tof_chip->client->dev, "IRQ stat: %#x\n", int_stat);
			}
			if (int_stat != 0) {
				//Clear interrupt on ToF chip
				error = tof8801_set_register(tof_chip->client, TOF8801_INT_STAT, int_stat);
				if (error) {
					tof8801_app0_report_error(tof_chip, ERR_COMM, DEV_OK);
				}
				tof8801_app0_process_irq(tof_chip, int_stat);
				// /* Alert user space of changes */
				// sysfs_notify(&tof_chip->client->dev.kobj,
				//							tof_app0_group.name,
				//	 						bin_attr_app0_tof_output.attr.name);
				g_error_index = 0;
				if (atomic_read(&g_data_valid) == 0) {
					dev_info(&tof_chip->client->dev, "data status is true\n");
					atomic_set(&g_data_valid, 1);
				}
			} else {
				g_error_index = g_error_index + 1;
				if (g_error_index == 6) {
					tof8801_get_register(tof_chip->client, TOF8801_STAT, &cpu_stat);
					tof8801_get_register(tof_chip->client, OL_STATE_OFFSET, &state);
					dev_err(&tof_chip->client->dev, "Error App0 start up; cpu_stat: %#x state: %#x\n", cpu_stat, state);
					if ((cpu_stat == OL_STATE_STARTUP) && (state == OL_STATE_STARTUP)) {
						g_is_need_recover = true;
						dev_info(&tof_chip->client->dev, "tof need recover\n");
					}
				} else if (g_error_index == 3) {
					if (atomic_read(&g_data_valid) == 1) {
						atomic_set(&g_data_valid, 0);
						dev_info(&tof_chip->client->dev, "data status is error\n");
					}
				}
			}
			break;
		case TOF8801_APP_ID_APP1:
			goto irq_handled;
	}
irq_handled:
	AMS_MUTEX_UNLOCK(&tof_chip->lock);
	return IRQ_HANDLED;
}

int tof8801_app0_poll_irq_thread(void *tof_chip)
{
	struct tof_sensor_chip *chip = (struct tof_sensor_chip *)tof_chip;
	char meas_cmd = 0;
	char delay_count = 0;
	int us_sleep = 0;
	int rc = 0;
	g_error_index = 0;
//	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };

//	sched_setscheduler(chip->app0_poll_irq, SCHED_FIFO, &param);
	AMS_MUTEX_LOCK(&chip->lock);
	// Poll period is interpreted in units of 100 usec
//	us_sleep = chip->poll_period * 100;
	us_sleep = chip->poll_period * 6600;
	LOG_INF( "Starting ToF irq polling thread, period: %u us\n", us_sleep);
	AMS_MUTEX_UNLOCK(&chip->lock);
	while (!kthread_should_stop()) {
		if (delay_count % TOF8801_DELAY_COUNT == 0) {
			delay_count = 0;
			AMS_MUTEX_LOCK(&chip->lock);
			meas_cmd = chip->app0_app.cap_settings.cmd;
			AMS_MUTEX_UNLOCK(&chip->lock);
			if (meas_cmd && rc != -EINVAL) {
				rc = tof_irq_handler(0, tof_chip);
			}
			if(meas_cmd && rc == -EINVAL){
				LOG_INF( "tof iic error but umd thread not exit \n");
			}
			if(meas_cmd == 0 && stop_tof_data == FALSE){
				LOG_INF( "tof stop but umd thread not exit \n");
			}
		}
		delay_count++;
		usleep_range(us_sleep / TOF8801_DELAY_COUNT, (us_sleep + us_sleep/10) / TOF8801_DELAY_COUNT);
	}
	LOG_INF( "End app0_poll_irq_thread thread \n");
	return 0;
}
/**
 * tof_i2c_write - Write nuber of bytes starting at a specific address over I2C
 *
 * @client: the i2c client
 * @reg: the i2c register address
 * @buf: pointer to a buffer that will contain the data to write
 * @len: number of bytes to write
 */
int tof_i2c_write(struct i2c_client *client, char reg, const char *buf, int len)
{
	u8 *addr_buf;
	struct i2c_msg msg;
	int ret=0;
	addr_buf = kmalloc(len + 1, GFP_KERNEL);
	if (!addr_buf)
		return -ENOMEM;

	addr_buf[0] = reg;
	memcpy(&addr_buf[1], buf, len);
	msg.flags = 0;
	msg.addr = client->addr;
	msg.buf = addr_buf;
	msg.len = len + 1;

	ret = i2c_transfer(client->adapter, &msg, 1);

	kfree(addr_buf);
	return ret < 0 ? ret : (ret != 1 ? -EIO : 0);
}

/**
 * tof8801_set_register - Set a specific register
 *
 * @chip: tof_sensor_chip pointer
 * @value: value to set in register
 */
int tof8801_set_register(struct i2c_client *client,
	char reg, const char value)
{
	return tof_i2c_write(client, reg, &value, sizeof(char));
}
/**
 * tof_standby_operation - Tell the ToF chip to wakeup/standby
 *
 * @client: the i2c client
 */
static int tof_standby_operation(struct i2c_client *client, char oper)
{
	return tof8801_set_register(client, TOF8801_STAT, oper);
}
/**
 * tof8801_get_register - Return a specific register
 *
 * @chip: tof_sensor_chip pointer
 * @value: pointer to value in register
 */
int tof8801_get_register(struct i2c_client *client,
	char reg, char *value)
{
	return tof_i2c_read(client, reg, value, sizeof(char));
}
/**
 * tof_CE_toggle - Hard reset the ToF by toggling the ChipEnable
 *
 * @client: the i2c client
 */
static int tof_CE_toggle(struct i2c_client *client)
{
	struct tof_sensor_chip *chip = i2c_get_clientdata(client);
	int error = 0;
	if (!chip->pdata->gpiod_enable) {
		//not supported in poll mode
		return -EIO;
	}
	error = gpio_direction_output(chip->enable_gpio, 0);
	if (error)
		return error;
	// g_is_download_fw = 0;
	error = gpio_direction_output(chip->enable_gpio, 1);
	/* ToF requires 5ms to get i2c back up */
	usleep_range(TOF8801_I2C_WAIT_USEC, TOF8801_I2C_WAIT_USEC+1);
	return error;
}
/**
 * tof_wait_for_cpu_startup - Check for CPU ready state in the ToF sensor
 *
 * @client: the i2c client
 */
int tof_wait_for_cpu_startup(struct i2c_client *client)
{
	int retry = 0;
	int CE_retry = 0;
	int error=0;
	u8 status;

	while (retry++ < TOF8801_MAX_STARTUP_RETRY) {
		usleep_range(TOF8801_I2C_WAIT_USEC, TOF8801_I2C_WAIT_USEC+1);
		error = tof8801_get_register(client, TOF8801_STAT, &status);
		if (error) {
			LOG_INF("i2c test failed attempt %d: %d\n", retry, error);
			continue;
		} else {
			TOF_LOG_IF(dbg,"CPU status register: %#04x value: %#04x\n",
					TOF8801_STAT, status);
		}
		if (TOF8801_STAT_CPU_READY(status)) {
			LOG_INF("ToF chip CPU is ready");
			return 0;
		} else if (TOF8801_STAT_CPU_SLEEP(status)) {
			LOG_INF("ToF chip in standby state, waking up");
			tof_standby_operation(client, WAKEUP);
			error = -EIO;
			continue;
		} else if (TOF8801_STAT_CPU_BUSY(status) &&
					 (retry >= TOF8801_MAX_STARTUP_RETRY)) {
			if ( (CE_retry < TOF8801_MAX_STARTUP_RETRY) ) {
				LOG_INF("ToF chip still busy, try toggle CE");
				if (tof_CE_toggle(client)) {
					return -EIO;
				}
				retry = 0;
				CE_retry++;
			} else {
				return -EIO;
			}
		}
	}
	return error;
}
/**
 * tof_wait_for_cpu_ready - Check for CPU ready state in the ToF sensor
 *
 * @client: the i2c client
 */
int tof_wait_for_cpu_ready(struct i2c_client *client)
{
	int retry = 0;
	int error;
	u8 status;

	//wait for i2c
	usleep_range(TOF8801_I2C_WAIT_USEC, TOF8801_I2C_WAIT_USEC+1);
	while (retry++ < TOF8801_MAX_WAIT_RETRY) {
		error = tof8801_get_register(client, TOF8801_STAT, &status);
		if (error) {
			LOG_INF("i2c test failed attempt %d: %d\n", retry, error);
			continue;
		}
		if (TOF8801_STAT_CPU_READY(status)) {
			LOG_INF("ToF chip CPU is ready");
			return 0;
		} else if (TOF8801_STAT_CPU_SLEEP(status)) {
			LOG_INF("ToF chip in standby state, waking up");
			tof_standby_operation(client, WAKEUP);
			usleep_range(TOF8801_I2C_WAIT_USEC, TOF8801_I2C_WAIT_USEC+1);
			error = -EIO;
			continue;
		} else if (TOF8801_STAT_CPU_BUSY(status) &&
					 (retry >= TOF8801_MAX_WAIT_RETRY)) {
			return -EIO;
		}
		usleep_range(TOF8801_WAIT_UDELAY, 2*TOF8801_WAIT_UDELAY);
	}

//	EXIT_ON_RELEASE_TOF_FAILURE;

	return error;
}
/**
 * tof_wait_for_cpu_ready_timeout - Check for CPU ready state in the ToF sensor
 *									for a specified period of time
 *
 * @client: the i2c client
 */
int tof_wait_for_cpu_ready_timeout(struct i2c_client *client, unsigned long usec)
{
	int error = 0;
	unsigned long curr = jiffies;
	do {
		error = tof_wait_for_cpu_ready(client);
		if (error == 0) {
			return 0;
		}
	} while ((jiffies - curr) < usecs_to_jiffies(usec));
		LOG_INF("Error timeout (%lu usec) waiting on cpu_ready: %d\n", usec, error);
	return -EIO;
}

static int tof8801_init(struct tof8801_device *tof8801)
{
	struct i2c_client *client = v4l2_get_subdevdata(&tof8801->sd);

	LOG_INF("[%s] %p\n", __func__, client);

	LOG_INF("-\n");

	return 0;
}
/**
 * tof_ram_patch_callback - The firmware download callback
 *
 * @cfg: the firmware cfg structure
 * @ctx: private data pointer to struct tof_sensor_chip
 */
static void tof_ram_patch_callback(const struct firmware *cfg, void *ctx)
{
	struct tof_sensor_chip *chip = ctx;
	const u8 *line;
	const u8 *line_end;
	int verify = 0;
	int result = 0;
	u32 patch_size = 0;
	u64 fwdl_time = 0;
	struct timespec64 start_ts = {0}, end_ts = {0};
	AMS_MUTEX_LOCK(&chip->lock);

	if (!chip) {
		pr_err("AMS-TOF Error: Ram patch callback NULL context pointer.\n");
		AMS_MUTEX_UNLOCK(&chip->lock);
		return;
	}

	if (!cfg) {
		LOG_INF("%s: Warning, firmware not available.\n", __func__);
		goto err_fwdl;
	}
	TOF_LOG_IF(dbg,"%s: Ram patch in progress... 1th\n", __func__);
	/* Assuming you can only perform ram download while in BL application */
	/* switch back to BL app to perform RAM download */
	if ( chip->info_rec.record.app_id != TOF8801_APP_ID_BOOTLOADER ) {
		LOG_INF("Current app_id: %hhx - Switching to bootloader for RAM download",
				 chip->info_rec.record.app_id);
		pr_err("AMS-TOF Error: debug for tof_switch_apps start .\n");
		result = tof_switch_apps(chip, (char)TOF8801_APP_ID_BOOTLOADER);
		if (result) {
			LOG_INF("Error changing to bootloader app: \'%d\'", result);
			goto err_fwdl;
		}
	}
	//Start fwdl timer
	ktime_get_real_ts64(&start_ts);
	/* setup encryption salt */
	result = tof8801_BL_upload_init(chip->client, &chip->BL_app, tof_salt_value);
	TOF_LOG_IF(dbg,"%s: Ram patch in progress... 2th \n", __func__);
	if (result) {
		LOG_INF("Error setting upload salt: \'%d\'", result);
		goto err_fwdl;
	}
	//assume we have mutex already
	intelHexInterpreterInitialise( );
	TOF_LOG_IF(dbg,"%s: Ram patch in progress... 3th \n", __func__);
	line = cfg->data;
	line_end = line;
	while ((line_end - cfg->data) < cfg->size) {
		line_end = strchrnul(line, '\n');
		patch_size += ((line_end - line) > INTEL_HEX_MIN_RECORD_SIZE) ?
						((line_end - line - INTEL_HEX_MIN_RECORD_SIZE) / 2) : 0;
		result = intelHexHandleRecord(chip->client, &chip->BL_app,

										line_end - line, line, verify);
		if (result) {
			LOG_INF("%s: Ram patch failed: %d\n", __func__, result);
			goto err_fwdl;
		}
		line = ++line_end;
	}
	//Stop fwdl timer
	ktime_get_real_ts64(&end_ts);
	fwdl_time = timespec64_sub(end_ts, start_ts).tv_nsec / 1000000; //time in ms
	LOG_INF("%s: Ram patch complete, patch size: %uK, dl time: %llu ms\n",
			 __func__, ((patch_size >> 10) + 1), fwdl_time);
	//wait for i2c
	usleep_range(TOF8801_I2C_WAIT_USEC, TOF8801_I2C_WAIT_USEC+1);
	/* resync our info record since we just switched apps */
	tof_init_info_record(chip);
	TOF_LOG_IF(dbg,"laser download fw success\n");
	// g_is_download_fw = 1;
err_fwdl:
	release_firmware(cfg);
	complete_all(&chip->ram_patch_in_progress);
	AMS_MUTEX_UNLOCK(&chip->lock);
}
static int tof8801_get_config_calib_data(struct tof_sensor_chip *chip)
{
	int error;
	const struct firmware *config_fw = NULL;
	/* Set current configuration calibration data size to 0*/
	chip->config_data.size = 0;
	///***** Check for available fac_calib to read *****/
	error = request_firmware_direct(&config_fw,
									chip->pdata->config_calib_data_fname,
									&chip->client->dev);
	if (error || !config_fw) {
		LOG_INF("configuration calibration data not available \'%s\': %d\n",
				 chip->pdata->config_calib_data_fname, error);
		return 0;
	} else {
		LOG_INF("Read in config_calib file: \'%s\'.\n",
				 chip->pdata->config_calib_data_fname);
	}
	if (config_fw->size > sizeof(chip->config_data.cfg_data)) {
		LOG_INF("Error: config calibration data size too large %ld > %lu (MAX)\n",
				config_fw->size, sizeof(chip->config_data.cfg_data));
		return 1;
	}
	memcpy((void *)&chip->config_data.cfg_data,
			 config_fw->data, config_fw->size);
	chip->config_data.size = config_fw->size;
	release_firmware(config_fw);
	return 0;
}
static int tof8801_firmware_download(struct tof_sensor_chip *chip, int startup)
{
	int error;
	struct timespec64 start_ts = {0}, end_ts = {0};
	int mutex_locked = mutex_is_locked(&chip->lock);
	int file_idx = 0;
	ktime_get_real_ts64(&start_ts);
	/* Iterate through all Firmware(s) to find one that works. 'Works' here is
	 * defined as running APP0 after FWDL
	 */
	for (file_idx=0; chip->pdata->ram_patch_fname[file_idx] != NULL; file_idx++) {
		/*** reset completion event that FWDL is starting ***/
		reinit_completion(&chip->ram_patch_in_progress);
		if (mutex_locked) {
			AMS_MUTEX_UNLOCK(&chip->lock);
		}
		TOF_LOG_IF(dbg,"Trying firmware: \'%s\'...\n",
				 chip->pdata->ram_patch_fname[file_idx]);
		/***** Check for available firmware to load *****/
		error = request_firmware_nowait(THIS_MODULE, true,
										chip->pdata->ram_patch_fname[file_idx],
										&chip->client->dev, GFP_KERNEL, chip,
										tof_ram_patch_callback);
		if (error) {
			LOG_INF("Firmware not available \'%s\': %d\n",
					 chip->pdata->ram_patch_fname[file_idx], error);
		}
		if (!startup &&
			!wait_for_completion_interruptible_timeout(&chip->ram_patch_in_progress,
							msecs_to_jiffies(TOF_FWDL_TIMEOUT_MSEC))) {
			LOG_INF("Timeout waiting for Ram Patch \'%s\' Complete",
					chip->pdata->ram_patch_fname[file_idx]);
		}
		if (mutex_locked) {
			AMS_MUTEX_LOCK(&chip->lock);
		}
		if (chip->info_rec.record.app_id == TOF8801_APP_ID_APP0) {
			LOG_INF("APP0 is running");
			// assume we are done if APP0 is running
			break;
		}
	}
	ktime_get_real_ts64(&end_ts);
	LOG_INF("FWDL callback %lu ms to finish",
			(timespec64_sub(end_ts, start_ts).tv_nsec / 1000000));
	// error if App0 is not running (fwdl failed)
	return (chip->info_rec.record.app_id != TOF8801_APP_ID_APP0) ? -EIO : 0;
}

static int tof_stop(void)
{
	struct tof_sensor_chip *chip;

	if (!g_tof_sensor_chip) {
		LOG_INF("g_tof_sensor_chip is NULL");
		return -EINVAL;
	}

	chip = g_tof_sensor_chip;

	if (is_alread_probe == 1) {
		AMS_MUTEX_OEM_LOCK(&chip->oem_lock);
		tof8801_app0_capture(chip, 0);
		LOG_INF("tof stop capture ");
		AMS_MUTEX_OEM_UNLOCK(&chip->oem_lock);
		g_is_alread_runing = 0;
	}

	return 0;
}

/* Power handling */
static int tof8801_power_off(struct tof8801_device *tof8801)
{
	int ret;
		int ret1 = 0;
	struct tof_sensor_chip *chip = g_tof_sensor_chip;
	LOG_INF("%s\n", __func__);

	ret = tof8801_release(tof8801);
	if (ret)
		LOG_INF("tof8801 release failed!\n");

	AMS_MUTEX_OEM_LOCK(&chip->power_lock);
	if(chip->power_status == TOF_POWER_ON) {
		if (!TOF8801_ENABLECONTROL) {
			// ret = tof_standby_operation(chip->client, STANDBY);
		} else {
			ret = gpio_direction_output(chip->enable_gpio, 0);
		}

		if(TOF8801_POWERCONTROL) {
			ret1 = gpio_direction_output(chip->pow_gpio, 0);
		}

		if (ret1 < 0 || ret < 0) {
			LOG_ERR("%s tof8801 set failed ret[%d] ret1[%d]", __func__, ret, ret1);
		}
		chip->power_status = TOF_POWER_OFF;
		LOG_INF("power off success");
	} else {
		LOG_INF("donot need do power down,power status=%d",chip->power_status);
	}
	chip->driver_debug = 0;
	g_dump_tof_registers = 0;
	AMS_MUTEX_OEM_UNLOCK(&chip->power_lock);
	return 0;
}

static int tof8801_power_on(struct tof8801_device *tof8801)
{
	int ret = 0;
	int ret1 = 0;
	struct tof_sensor_chip *chip = g_tof_sensor_chip;

	LOG_ERR("%s E\n", __func__);

	AMS_MUTEX_OEM_LOCK(&chip->power_lock);
	if (chip->power_status == TOF_POWER_OFF) {
		if (TOF8801_POWERCONTROL) {
			ret = gpio_direction_output(chip->pow_gpio, 1);
		}

		usleep_range(TOF8801_CTRL_DELAY_US, TOF8801_CTRL_DELAY_US + 100);

		if (!TOF8801_ENABLECONTROL && is_alread_probe) {
			// ret = tof_standby_operation(chip->client, WAKEUP);
		} else {
			ret1 = gpio_direction_output(chip->enable_gpio, 1);
		}

		if (ret1 < 0 || ret < 0) {
			LOG_ERR("%s tof8801 set failed ret[%d] ret1[%d]", __func__, ret, ret1);
		}
		usleep_range(TOF8801_CTRL_DELAY_US, TOF8801_CTRL_DELAY_US + 100);
		LOG_INF("power on success");
		chip->power_status = TOF_POWER_ON;
	} else {
		LOG_INF("donot need do power on,power status=%d",chip->power_status);
	}
	AMS_MUTEX_OEM_UNLOCK(&chip->power_lock);

	ret = tof8801_init(tof8801);
	if (ret < 0) {
			LOG_ERR("tof8801_init failed ret[%d] return", ret);
	}
	LOG_ERR("%s X\n", __func__);
	return 0;

}

static int async_thread_fn(void *arg)
{
	LOG_ERR("%s X\n", __func__);
	struct tof_sensor_chip *chip = g_tof_sensor_chip;
	struct tof8801_device *tof8801 = (struct tof8801_device *)arg;
	int ret = 0;
	atomic_set(&g_recover_process, 1);

	g_is_need_recover = false;
	chip_enable_store(1);

	AMS_MUTEX_LOCK(&chip->tof_status_lock);
	if (chip->tof_status == 0) {
		capture_store(0);
		stop_data_store(0);
		ret = tof8801_power_off(tof8801);
		if (ret < 0) {
			LOG_INF("power off fail, ret = %d\n", ret);
		}
		LOG_INF("tof8801 recover end need stop tof\n");
	} else if (chip->tof_status == 1) {
		capture_store(1);
		stop_data_store(1);
		LOG_INF("tof8801 recover end need start tof\n");
	}
	AMS_MUTEX_UNLOCK(&chip->tof_status_lock);

	atomic_set(&g_recover_process, 0);

	return 0;
}

static int tof8801_set_ctrl(struct v4l2_ctrl *ctrl)
{
	/* struct tof8801_device *tof8801 = to_tof8801_ois(ctrl); */

	return 0;
}

static const struct v4l2_ctrl_ops tof8801_ois_ctrl_ops = {
	.s_ctrl = tof8801_set_ctrl,
};

static int tof8801_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	int ret;
	struct tof8801_device *tof8801 = sd_to_tof8801_ois(sd);
	struct tof_sensor_chip *chip = g_tof_sensor_chip;
	LOG_INF("%s\n", __func__);

	AMS_MUTEX_LOCK(&chip->tof_status_lock);
	chip->tof_status = 1;
	AMS_MUTEX_UNLOCK(&chip->tof_status_lock);

	if (atomic_read(&g_recover_process) == 1) {
		LOG_INF("tof8801 recover process\n");
		return 0;
	}

	ret = tof8801_power_on(tof8801);
	if (ret < 0) {
		LOG_INF("power on fail, ret = %d\n", ret);
		return ret;
	}

	if (g_is_need_recover) {
		LOG_INF("tof8801 need recover chip_enable_store 1!\n");
		async_task_recover = kthread_run(async_thread_fn, tof8801, "async_task_recover");
		if (IS_ERR(async_task_recover)) {
			LOG_ERR("tof8801 recover failed!\n");
			async_task_recover = NULL;
			return PTR_ERR(async_task_recover);
		}
		return 0;
	}

	capture_store(1);

	stop_data_store(1);

	return 0;
}

static int tof8801_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	int ret;
	struct tof8801_device *tof8801 = sd_to_tof8801_ois(sd);
	struct tof_sensor_chip *chip = g_tof_sensor_chip;
	LOG_INF("%s\n", __func__);
	AMS_MUTEX_LOCK(&chip->tof_status_lock);
	chip->tof_status = 0;
	AMS_MUTEX_UNLOCK(&chip->tof_status_lock);

	if (g_is_need_recover) {
		LOG_INF("tof8801 need recover chip_enable_store 0!\n");
		chip_enable_store(0);
	}

	atomic_set(&g_data_valid, 0);

	if (atomic_read(&g_recover_process) == 1) {
		LOG_INF("tof8801 recover process\n");
		return 0;
	}

	capture_store(0);

	stop_data_store(0);

	ret = tof8801_power_off(tof8801);
	if (ret < 0) {
		LOG_INF("power off fail, ret = %d\n", ret);
		return ret;
	}

	return 0;
}

static long tof8801_ops_core_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct tof8801_device *tof8801 = sd_to_tof8801_ois(sd);
	struct tof_sensor_chip *chip = g_tof_sensor_chip;
	switch (cmd) {

	case VIDIOC_MTK_G_TOF_INIT:
	{
		struct mtk_oplus_tof_info *info = arg;
		struct Oplus_TofInfo tof_info;
		memset(&tof_info, 0, sizeof(struct Oplus_TofInfo));

		LOG_INF("VIDIOC_MTK_G_TOF_INIT\n");
		chip_enable_store(1);
		capture_store(1);
		stop_data_store(1);
		g_is_need_recover = false;
		mdelay(10);

		if (tof8801->driver_init == 1) {
			ret = tof8801_init(tof8801);
			if (ret < 0) {
				LOG_INF("tof8801_init fail\n");
				return ret;
			}

			tof8801->driver_init = 0;
		}

		//tof_info.is_tof_supported = 1;
		if (copy_to_user((void *)info->p_oplus_tof_info, &tof_info, sizeof(tof_info)))
			ret = -EFAULT;
	}
	break;

	case VIDIOC_MTK_G_TOF_INFO:
	{
		struct mtk_oplus_tof_info *info = arg;
		struct Oplus_TofInfo tof_info;

		memset(&tof_info, 0, sizeof(struct Oplus_TofInfo));

		if (atomic_read(&g_data_valid) == 0) {
			return -EFAULT;
		}
		tof_info.num_of_rows = 1;
		tof_info.num_of_cols = 1; /* Unit : mm */
		tof_info.dmax_distance[0] = 2500;
		tof_info.timestamp = 0;
		tof_info.error_status[0] = 0;
		spin_lock(&chip->data_lock);
		tof_info.ranging_distance[0] = chip->distance;
		tof_info.confidence = chip->confidence;
		tof_info.result_cnt = chip->result_cnt;
		if (chip->histogram_enable) {
			for (int i = 0; i < TOF_HISTOGRAM_SUM; i++) {
				tof_info.histogram[i] = chip->app0_app.histogram[i]; //16bit per bin
			}
		}
		spin_unlock(&chip->data_lock);
		//tof_info.is_tof_supported = 1;
		if (copy_to_user((void *)info->p_oplus_tof_info, &tof_info, sizeof(tof_info)))
			ret = -EFAULT;
	}
	break;

	case VIDIOC_OPLUS_S_TOF_CAPTURE:
	{
		unsigned int *data = arg;
		LOG_INF("VIDIOC_OPLUS_S_TOF_CAPTURE %d\n", *data);
		if (*data == 0) {
			atomic_set(&g_data_valid, 0);
		}
		capture_store(*data);
	}
	break;

	case VIDIOC_OPLUS_S_TOF_PERIOD:
	{
		char *data = arg;
		LOG_INF("VIDIOC_OPLUS_S_TOF_PERIOD\n");
		ret = period_store(data);
		if (ret < 0) {
			LOG_INF("period_store fail\n");
			return ret;
		}
	}
	break;

	case VIDIOC_OPLUS_S_TOF_ITERATIONS:
	{
		char *data = arg;
		LOG_INF("VIDIOC_OPLUS_S_TOF_ITERATIONS\n");
		ret = iterations_store(data, sizeof(data));
		if (ret < 0) {
			LOG_INF("iterations_store fail\n");
			return ret;
		}
	}
	break;

	case VIDIOC_OPLUS_S_TOF_ALGSETTING:
	{
		char *data = arg;
		LOG_INF("VIDIOC_OPLUS_S_TOF_ALGSETTING\n");
		ret = alg_setting_store(data, sizeof(data));
		if (ret < 0) {
			LOG_INF("alg_setting_store fail\n");
			return ret;
		}
	}
	break;

	case VIDIOC_OPLUS_S_TOF_CALIB:
	{
		char *data = arg;
		LOG_INF("VIDIOC_OPLUS_S_TOF_CALIB\n");
		ret = app0_apply_fac_calib_store(data, 14);
		if (ret < 0) {
			LOG_INF("app0_apply_fac_calib_store fail\n");
			return ret;
		}
	}
	break;

	case VIDIOC_OPLUS_G_TOF_CALIB:
	{
		LOG_INF("VIDIOC_OPLUS_G_TOF_CALIB\n");
		struct oplus_tof_get_info *info = arg;
		struct Oplus_TofGetinfo tof_info;

		memset(&tof_info, 0, sizeof(struct Oplus_TofGetinfo));

		ret = app0_get_fac_calib_show(tof_info.calibration_data);
		if (ret < 0) {
			LOG_INF("app0_get_fac_calib_show fail\n");
			return ret;
		} else {
			if (copy_to_user((void *)info->p_oplus_tof_get_info, &tof_info, sizeof(tof_info)))
				ret = -EFAULT;
		}
	}
	break;

	case VIDIOC_OPLUS_S_TOF_TRIM:
	{
		char *data = arg;
		LOG_INF("VIDIOC_OPLUS_S_TOF_TRIM\n");
		ret = app0_clk_trim_set_store(data, sizeof(data));
		if (ret < 0) {
			LOG_INF("app0_clk_trim_set_store fail\n");
			return ret;
		}
	}
	break;

	case VIDIOC_OPLUS_G_TOF_TRIM:
	{
		LOG_INF("VIDIOC_OPLUS_G_TOF_TRIM\n");
		struct oplus_tof_get_info *info = arg;
		struct Oplus_TofGetinfo tof_info;

		memset(&tof_info, 0, sizeof(struct Oplus_TofGetinfo));

		ret = app0_clk_trim_set_show(tof_info.trim_data);
		if (ret < 0) {
			LOG_INF("app0_clk_trim_set_show fail\n");
			return ret;
		} else {
			if (copy_to_user((void *)info->p_oplus_tof_get_info, &tof_info, sizeof(tof_info)))
				ret = -EFAULT;
		}
	}
	break;

	case VIDIOC_OPLUS_G_TOF_XTALK:
	{
		LOG_INF("VIDIOC_OPLUS_G_TOF_XTALK\n");
		struct oplus_tof_get_info *info = arg;
		struct Oplus_TofGetinfo tof_info;

		memset(&tof_info, 0, sizeof(struct Oplus_TofGetinfo));

		ret = app0_read_peak_crosstalk_show(tof_info.xtalk_data);
		if (ret < 0) {
			LOG_INF("app0_read_peak_crosstalk_show fail\n");
			return ret;
		} else {
			if (copy_to_user((void *)info->p_oplus_tof_get_info, &tof_info, sizeof(tof_info)))
				ret = -EFAULT;
		}
	}
	break;

	case VIDIOC_OPLUS_S_TOF_STOP:
	{
		unsigned int *data = arg;
		LOG_INF("VIDIOC_OPLUS_S_TOF_STOP %d\n", *data);
		stop_data_store(*data);
	}
	break;

	case VIDIOC_OPLUS_S_TOF_DEBUG:
	{
		char *data = arg;
		LOG_INF("VIDIOC_OPLUS_S_TOF_DEBUG\n");
		ret = driver_debug_store(data, sizeof(data));
		if (ret < 0) {
			LOG_INF("driver_debug_store fail\n");
			return ret;
		}
	}
	break;

	case VIDIOC_OPLUS_S_TOF_CHIP_ENABLE:
	{
		unsigned int *data = arg;
		LOG_INF("VIDIOC_OPLUS_S_TOF_CHIP_ENABLE %d\n", *data);
		chip_enable_store(*data);
	}
	break;

	case VIDIOC_OPLUS_S_TOF_HISTOGRAM_ENABLE:
	{
		unsigned int *data = arg;
		chip->histogram_enable = *data;
		LOG_INF("VIDIOC_OPLUS_S_TOF_HISTOGRAM_ENABLE %d\n", chip->histogram_enable);
	}
	break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

static const struct v4l2_subdev_internal_ops tof8801_int_ops = {
	.open = tof8801_open,
	.close = tof8801_close,
};

static struct v4l2_subdev_core_ops tof8801_ops_core = {
	.ioctl = tof8801_ops_core_ioctl,
};

static const struct v4l2_subdev_ops tof8801_ops = {
	.core = &tof8801_ops_core,
};

static void tof8801_subdev_cleanup(struct tof8801_device *tof8801)
{
	v4l2_async_unregister_subdev(&tof8801->sd);
	v4l2_ctrl_handler_free(&tof8801->ctrls);
#if IS_ENABLED(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&tof8801->sd.entity);
#endif
}

static int tof8801_init_controls(struct tof8801_device *tof8801)
{
	struct v4l2_ctrl_handler *hdl = &tof8801->ctrls;
	/* const struct v4l2_ctrl_ops *ops = &tof8801_ois_ctrl_ops; */

	v4l2_ctrl_handler_init(hdl, 1);

	if (hdl->error)
		return hdl->error;

	tof8801->sd.ctrl_handler = hdl;

	return 0;
}

static int tof8801_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct tof8801_device *tof8801;
	struct tof_sensor_chip *tof_chip;
	int error = 0;
	int ret;

	LOG_INF("%s\n", __func__);

	dev_info(&client->dev, "I2C Address: %#04x\n", client->addr);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "I2C check functionality failed.\n");
		return -ENXIO;
	}

	tof8801 = devm_kzalloc(dev, sizeof(*tof8801), GFP_KERNEL);
	if (!tof8801)
		return -ENOMEM;

	tof_chip = devm_kzalloc(&client->dev, sizeof(*tof_chip), GFP_KERNEL);
	g_tof_sensor_chip = tof_chip;
	if (!tof_chip)
		return -ENOMEM;

	tof_chip->enable_gpio = of_get_named_gpio(dev->of_node, "enable-gpio", 0);
	if (TOF8801_POWERCONTROL) {
		tof_chip->pow_gpio = of_get_named_gpio(dev->of_node, "pow-gpio", 0);
	}
	LOG_ERR("parsing dtsi pow_gpio = %d %d", tof_chip->pow_gpio, tof_chip->enable_gpio);

	tof8801->vin = devm_regulator_get(dev, "camera_tof_vin");
	if (IS_ERR(tof8801->vin)) {
		ret = PTR_ERR(tof8801->vin);
		if (ret != -EPROBE_DEFER)
			LOG_INF("cannot get vin regulator\n");
		return ret;
	}

	tof8801->vdd = devm_regulator_get(dev, "camera_tof_dvdd");
	if (IS_ERR(tof8801->vdd)) {
		ret = PTR_ERR(tof8801->vdd);
		if (ret != -EPROBE_DEFER)
			LOG_INF("cannot get vdd regulator\n");
		return ret;
	}

	tof8801->vcamaf_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(tof8801->vcamaf_pinctrl)) {
		ret = PTR_ERR(tof8801->vcamaf_pinctrl);
		tof8801->vcamaf_pinctrl = NULL;
		LOG_INF("cannot get pinctrl\n");
	} else {
		tof8801->vcamaf_on = pinctrl_lookup_state(
			tof8801->vcamaf_pinctrl, "camera_tof_en_output_high");

		if (IS_ERR(tof8801->vcamaf_on)) {
			ret = PTR_ERR(tof8801->vcamaf_on);
			tof8801->vcamaf_on = NULL;
			LOG_INF("cannot get vcamaf_on pinctrl\n");
		}

		tof8801->vcamaf_off = pinctrl_lookup_state(
			tof8801->vcamaf_pinctrl, "camera_tof_en_output_low");

		if (IS_ERR(tof8801->vcamaf_off)) {
			ret = PTR_ERR(tof8801->vcamaf_off);
			tof8801->vcamaf_off = NULL;
			LOG_INF("cannot get vcamaf_off pinctrl\n");
		}
	}

	v4l2_i2c_subdev_init(&tof8801->sd, client, &tof8801_ops);
	tof8801->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	tof8801->sd.internal_ops = &tof8801_int_ops;

	ret = tof8801_init_controls(tof8801);
	if (ret)
		goto err_cleanup;

	mutex_init(&tof_chip->lock);
	mutex_init(&tof_chip->state_lock);
	mutex_init(&tof_chip->oem_lock);
	mutex_init(&tof_chip->irq_lock);
	mutex_init(&tof_chip->tof_status_lock);
	mutex_init(&tof_chip->power_lock);
	spin_lock_init(&tof_chip->data_lock);

	client->dev.platform_data = (void *)&tof_pdata;
	tof_chip->client = client;
	tof_chip->pdata = &tof_pdata;
	tof_pdata.client = client;
	tof_chip->power_status = TOF_POWER_OFF;
	tof_chip->irq_thread_status = TOF_IRQ_THREAD_STOP;
	i2c_set_clientdata(client, tof_chip);
	/***** Firmware sync structure initialization*****/
	init_completion(&tof_chip->ram_patch_in_progress);
	//initialize kfifo for frame output
	INIT_KFIFO(tof_chip->tof_output_fifo);
	//Setup measure timer
	timer_setup(&tof_chip->meas_timer,tof8801_app0_measure_timer_expiry_callback,0);

#if 0
	setup_timer(&tof_chip->meas_timer,
				tof8801_app0_measure_timer_expiry_callback,
				(unsigned long) tof_chip);
#endif

	tof_chip->poll_period = 10;

	if (tof8801->vcamaf_pinctrl && tof8801->vcamaf_on)
		ret = pinctrl_select_state(tof8801->vcamaf_pinctrl,
					tof8801->vcamaf_on);

	tof8801_power_on(tof8801);
#if 1
	/***** Wait until ToF is ready for commands *****/
	error = tof_wait_for_cpu_startup(client);
	if (error) {
		LOG_INF("I2C communication failure: %d\n", error);
	}

#if 1
	tof_chip->saved_clk_trim = UNINITIALIZED_CLK_TRIM_VAL;
	//read external (manufacturer) configuration data
	error = tof8801_get_config_calib_data(tof_chip);
	if (error) {
		LOG_INF("Error reading config data: %d\n", error);
	}

	//read external (manufacturer) factory calibration data
	// error = tof8801_get_fac_calib_data(tof_chip);
	// if (error) {
	// 	dev_err(&client->dev, "Error reading fac_calib data: %d\n", error);
	// }
	tof8801_app0_default_cap_settings(&tof_chip->app0_app);
#endif
	error = tof_init_info_record(tof_chip);
	if (error) {
		LOG_INF("Read application info record failed.\n");
	}
#endif
	/* enable all ToF interrupts on sensor */
	tof8801_enable_interrupts(tof_chip, IRQ_RESULTS | IRQ_DIAG | IRQ_ERROR);

	tof_chip->driver_debug = 0;
	is_alread_probe = 1;
	tof8801_power_off(tof8801);
	tof_chip->histogram_enable = 0;
	LOG_INF("%s Probe ok\n", __func__);

#if IS_ENABLED(CONFIG_MEDIA_CONTROLLER)
	ret = media_entity_pads_init(&tof8801->sd.entity, 0, NULL);
	if (ret < 0)
		goto err_cleanup;

	tof8801->sd.entity.function = MEDIA_ENT_F_LENS;
#endif

	ret = v4l2_async_register_subdev(&tof8801->sd);
	if (ret < 0)
		goto err_cleanup;

	pm_runtime_enable(dev);

	return 0;

err_cleanup:
	tof8801_subdev_cleanup(tof8801);
	LOG_INF("Probe failed.\n");
	return ret;
}

static void tof8801_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct tof8801_device *tof8801 = sd_to_tof8801_ois(sd);

	LOG_INF("%s\n", __func__);

	tof8801_subdev_cleanup(tof8801);
	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		tof8801_power_off(tof8801);
	pm_runtime_set_suspended(&client->dev);

	return ;
}

static int __maybe_unused tof8801_ois_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct tof8801_device *tof8801 = sd_to_tof8801_ois(sd);

	return tof8801_power_off(tof8801);
}

static int __maybe_unused tof8801_ois_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct tof8801_device *tof8801 = sd_to_tof8801_ois(sd);

	return tof8801_power_on(tof8801);
}

static const struct i2c_device_id tof8801_id_table[] = {
	{ TOF8801_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, tof8801_id_table);

static const struct of_device_id tof8801_of_table[] = {
	{ .compatible = "oplus,tof8801" },
	{ },
};
MODULE_DEVICE_TABLE(of, tof8801_of_table);

static const struct dev_pm_ops tof8801_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
	SET_RUNTIME_PM_OPS(tof8801_ois_suspend, tof8801_ois_resume, NULL)
};

static struct i2c_driver tof8801_i2c_driver = {
	.driver = {
		.name = TOF8801_NAME,
		.pm = &tof8801_pm_ops,
		.of_match_table = tof8801_of_table,
	},
	.probe	= tof8801_probe,
	.remove = tof8801_remove,
	.id_table = tof8801_id_table,
};

module_i2c_driver(tof8801_i2c_driver);

MODULE_AUTHOR("huji");
MODULE_DESCRIPTION("TOF8801 TOF driver");
MODULE_LICENSE("GPL v2");
