/***************************************************************
** Copyright (C),  2021,  OPLUS Mobile Comm Corp.,  Ltd
** File : oplus_adfr.h
** Description : ADFR kernel module
** Version : 1.0
** Date : 2021/07/09
**
** ------------------------------- Revision History: -----------
**  <author>        <data>        <version >        <desc>
**  Gaoxiaolei      2021/07/09        1.0         Build this moudle
******************************************************************/

#ifndef _OPLUS_ADFR_H_
#define _OPLUS_ADFR_H_

/* please just only include linux common head file to keep me pure */
#include <linux/device.h>
#include <linux/hrtimer.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/leds.h>
#include <linux/delay.h>
#include <drm/drm_device.h>
#include <drm/drm_modes.h>
#include "oplus_display_debug.h"

enum OPLUS_ADFR_LOG_LEVEL {
	OPLUS_ADFR_LOG_LEVEL_NONE = 0,
	OPLUS_ADFR_LOG_LEVEL_ERR = 1,
	OPLUS_ADFR_LOG_LEVEL_WARN = 2,
	OPLUS_ADFR_LOG_LEVEL_INFO = 3,
	OPLUS_ADFR_LOG_LEVEL_DEBUG = 4,
};

enum oplus_adfr_display_id {
	OPLUS_ADFR_PRIMARY_DISPLAY = 0,
	OPLUS_ADFR_SECONDARY_DISPLAY = 1,
};

enum oplus_adfr_h_skew_mode {
	STANDARD_ADFR = 0,			/* SA */
	STANDARD_MFR = 1,			/* SM */
	OPLUS_ADFR = 2,				/* OA */
	OPLUS_MFR = 3,				/* OM */
};

enum oplus_adfr_auto_mode {
	OPLUS_ADFR_AUTO_OFF = 0,                        /* manual mode */
	OPLUS_ADFR_AUTO_ON = 1,                         /* auto mode */
	OPLUS_ADFR_AUTO_IDLE = 2,                       /* min fps value is sw fps if auto idle is set */
};

enum oplus_adfr_idle_mode {
	OPLUS_ADFR_IDLE_OFF = 0,						/* exit mipi idle */
	OPLUS_ADFR_IDLE_ON = 1,							/* enter mipi idle */
};

enum oplus_adfr_test_te_config {
	OPLUS_ADFR_TEST_TE_DISABLE = 0,					/* disable test te irq detection */
	OPLUS_ADFR_TEST_TE_ENABLE = 1,					/* enable test te irq detection */
	OPLUS_ADFR_TEST_TE_ENABLE_WITCH_LOG = 2,		/* enable test te irq detection and log */
};

enum oplus_adfr_osync_params {
	OPLUS_ADFR_OSYNC_MODE = 0,						/* update osync mode params */
	OPLUS_ADFR_OSYNC_MIN_FPS = 1,					/* update osync min fps params */
};

struct oplus_minfps {
	int minfps_flag;
	u32 extend_frame;
};

enum oplus_adfr_min_fps_value {
	OPLUS_ADFR_MIN_FPS_120HZ = 120,
	OPLUS_ADFR_MIN_FPS_90HZ = 90,
	OPLUS_ADFR_MIN_FPS_60HZ = 60,
	OPLUS_ADFR_MIN_FPS_45HZ = 45,
	OPLUS_ADFR_MIN_FPS_40HZ = 40,
	OPLUS_ADFR_MIN_FPS_30HZ = 30,
	OPLUS_ADFR_MIN_FPS_20HZ = 20,
	OPLUS_ADFR_MIN_FPS_15HZ = 15,
	OPLUS_ADFR_MIN_FPS_10HZ = 10,
	OPLUS_ADFR_MIN_FPS_5HZ  = 5,
	OPLUS_ADFR_MIN_FPS_1HZ  = 1,
};

struct oplus_adfr_test_te_params {
	struct gpio_desc *gpio;							/* a gpio used to check current refresh rate of ddic */
	unsigned int config;							/*
													 0:disable test te irq detection
													 1:enable test te irq detection
													 2:enable test te irq detection && log
													*/
	unsigned int high_refresh_rate_count;			/* a value used to indicates the count of high refresh rate */
	unsigned int middle_refresh_rate_count;			/* a value used to indicates the count of middle refresh rate */
	unsigned int refresh_rate;						/* a value used to indicates current refresh rate of ddic */
	u64 last_timestamp;								/* a value used to indicates the last test te irq timestamp */
	struct hrtimer timer;							/* a timer used to update refresh rate of ddic if enter idle mode */
};

/* remember to initialize params */
struct oplus_adfr_params {
	unsigned int config;							/*
													 bit(0):global
													 bit(1):fakeframe
													 bit(2):vsync switch
													 bit(3):vsync switch mode, 0:OPLUS_ADFR_TE_SOURCE_VSYNC_SWITCH, 1:OPLUS_ADFR_MUX_VSYNC_SWITCH
													 bit(4):idle mode
													 bit(5):temperature detection
													 bit(6):oa bl mutual exclusion
													 bit(7):sa mode restore when finished booting
													 bit(8):dry run
													 bit(9):decreasing step
													*/
	unsigned int auto_mode;							/* a value used to control auto/manual frame rate mode */
	bool auto_mode_updated;							/* indicates whether auto mode is updated or not */
	bool need_filter_auto_on_cmd;					/* indicates whether auto on cmds need to be filtered if auto off cmds have been sent within one frame or not */
	unsigned int sa_min_fps;						/* the minimum self-refresh rate when no image would be sent to ddic in sa mode */
	bool sa_min_fps_updated;						/* indicates whether sa min fps is updated or not */
	bool skip_min_fps_setting;						/* indicates whether min fps setting should be skipped or not */
	unsigned int sw_fps;							/* software vsync value */
	unsigned int idle_mode;							/* a value used to indicates current mipi idle status */
	unsigned int oplus_adfr_idle_off_min_fps;
	struct oplus_adfr_test_te_params test_te;		/* a structure used to store current test te params */
	unsigned int osync_min_fps;						/* the minimum self-refresh rate when no image would be sent to ddic in osync mode */
	bool enable_multite;
	struct mutex multite_mutex;
};

/* log level config */
extern unsigned int oplus_adfr_log_level;

/* debug log */
#define ADFR_ERR(fmt, ...) \
	do { \
		if (oplus_adfr_log_level >= OPLUS_ADFR_LOG_LEVEL_ERR) \
			OPLUS_DISP_ERR("ADFR", fmt, ##__VA_ARGS__); \
	} while (0)

#define ADFR_WARN(fmt, ...) \
	do { \
		if (oplus_adfr_log_level >= OPLUS_ADFR_LOG_LEVEL_WARN) \
			OPLUS_DISP_WARN("ADFR", fmt, ##__VA_ARGS__); \
	} while (0)

#define ADFR_INFO(fmt, ...) \
	do { \
		if (oplus_adfr_log_level >= OPLUS_ADFR_LOG_LEVEL_INFO) \
			OPLUS_DISP_INFO("ADFR", fmt, ##__VA_ARGS__); \
	} while (0)

#define ADFR_DEBUG(fmt, ...) \
	do { \
		if ((oplus_adfr_log_level >= OPLUS_ADFR_LOG_LEVEL_DEBUG) || (oplus_display_log_type & OPLUS_DEBUG_LOG_ADFR)) \
			OPLUS_DISP_INFO("ADFR", fmt, ##__VA_ARGS__); \
		else \
			OPLUS_DISP_DEBUG("ADFR", fmt, ##__VA_ARGS__); \
	} while (0)

/* debug trace */
#define OPLUS_ADFR_TRACE_BEGIN(fmt, args...) \
	do { \
		if ((oplus_display_trace_enable & OPLUS_DISPLAY_ADFR_TRACE_ENABLE) || (g_trace_log)) { \
			mtk_drm_print_trace("B|%d|"fmt"\n", current->tgid, ##args); \
		} \
	} while (0)

#define OPLUS_ADFR_TRACE_END(fmt, args...) \
	do { \
		if ((oplus_display_trace_enable & OPLUS_DISPLAY_ADFR_TRACE_ENABLE) || (g_trace_log)) { \
			mtk_drm_print_trace("E|%d|"fmt"\n", current->tgid, ##args); \
		} \
	} while (0)

#define OPLUS_ADFR_TRACE_INT(fmt, args...) \
	do { \
		if ((oplus_display_trace_enable & OPLUS_DISPLAY_ADFR_TRACE_ENABLE) || (g_trace_log)) { \
			mtk_drm_print_trace("C|%d|"fmt"\n", current->tgid, ##args); \
		} \
	} while (0)

/* ---------------- oplus_adfr_params -------------------- */
int oplus_adfr_update_display_id(void);
bool oplus_adfr_is_supported(void *oplus_adfr_params);
bool oplus_adfr_is_supported_export(void *drm_crtc);

/* --------------- adfr misc ---------------*/
int oplus_dsi_display_adfr_init(void *dev_node, void *ctx);
int oplus_dsi_display_adfr_deinit(void *ctx_dev);
int oplus_adfr_init(void *dsi_dev, bool is_primary);

/* --------------- test te -------------------- */
enum hrtimer_restart oplus_adfr_test_te_timer_handler(struct hrtimer *timer);
int oplus_adfr_register_test_te_irq(void *mtk_ddp_comp, void *platform_device);

/* --------------- auto mode --------------- */
/* add for auto on cmd filter */
void oplus_adfr_handle_auto_mode(void *drm_crtc, int prop_id, unsigned int propval);
void oplus_adfr_auto_mode_update(void *drm_device);
int oplus_adfr_send_min_fps_event(unsigned int min_fps);
void oplus_adfr_status_reset(void *dst_mode);

/* --------------- temperature detection -------------------- */
int oplus_adfr_temperature_detection_handle(void *mtk_ddp_comp, void *cmdq_pkt, int ntc_temp, int shell_temp);

/* --------------- multi te --------------- */
void oplus_adfr_set_multite_state(void *drm_crtc, bool state);
bool oplus_adfr_get_multite_state(void *drm_crtc);
int oplus_adfr_send_multite(void *drm_crtc, bool enable);

/* --------------- idle mode -------------- */
/* ADFR:Add for idle mode control */
/* idle mode handle */
void oplus_adfr_handle_idle_mode(void *drm_crtc, int enter_idle);

void oplus_adfr_set_current_crtc(void *drm_crtc);
ssize_t oplus_adfr_set_config_attr(struct kobject *obj,
	struct kobj_attribute *attr, const char *buf, size_t count);
ssize_t oplus_adfr_get_config_attr(struct kobject *obj,
	struct kobj_attribute *attr, char *buf);
ssize_t oplus_adfr_set_test_te_attr(struct kobject *obj,
	struct kobj_attribute *attr, const char *buf, size_t count);
ssize_t oplus_adfr_get_test_te_attr(struct kobject *obj,
	struct kobj_attribute *attr, char *buf);
int oplus_adfr_set_test_te(void *buf);
int oplus_adfr_get_test_te(void *buf);
ssize_t oplus_adfr_set_min_fps_attr(struct kobject *obj,
	struct kobj_attribute *attr, const char *buf, size_t count);
ssize_t oplus_adfr_get_min_fps_attr(struct kobject *obj,
	struct kobj_attribute *attr, char *buf);

#endif /* _OPLUS_ADFR_H_ */
