/***************************************************************
** Copyright (C), 2022, OPLUS Mobile Comm Corp., Ltd
** File : oplus_display_onscreenfingerprint.h
** Description : oplus_display_onscreenfingerprint header
** Version : 1.0
** Date : 2021/12/10
** Author : Display
***************************************************************/

#ifndef _OPLUS_DISPLAY_ONSCREENFINGERPRINT_H_
#define _OPLUS_DISPLAY_ONSCREENFINGERPRINT_H_

/* please just only include linux common head file to keep me pure */
#include <linux/module.h>
#include <linux/err.h>
#include <drm/drm_crtc.h>
#include "oplus_display_debug.h"

enum OPLUS_OFP_LOG_LEVEL {
	OPLUS_OFP_LOG_LEVEL_ERR = 0,
	OPLUS_OFP_LOG_LEVEL_WARN = 1,
	OPLUS_OFP_LOG_LEVEL_INFO = 2,
	OPLUS_OFP_LOG_LEVEL_DEBUG = 3,
};

enum oplus_ofp_property_value {
	OPLUS_OFP_PROPERTY_NONE = 0,
	OPLUS_OFP_PROPERTY_DIM_LAYER = BIT(0),
	OPLUS_OFP_PROPERTY_FINGERPRESS_LAYER = BIT(1),
	OPLUS_OFP_PROPERTY_ICON_LAYER = BIT(2),
	OPLUS_OFP_PROPERTY_AOD_LAYER = BIT(3),
};

enum oplus_ofp_irq_type {
	OPLUS_OFP_TE_RDY = 0,
	OPLUS_OFP_FRAME_DONE = 1,
};

enum oplus_ofp_pressed_icon_status {
	OPLUS_OFP_PRESSED_ICON_OFF_FRAME_DONE = 0,		/* pressed icon has not been flush to DDIC ram */
	OPLUS_OFP_PRESSED_ICON_OFF = 1,					/* pressed icon has not been displayed in panel */
	OPLUS_OFP_PRESSED_ICON_ON_FRAME_DONE = 2,		/* pressed icon has been flush to DDIC ram */
	OPLUS_OFP_PRESSED_ICON_ON = 3,					/* pressed icon has been displayed in panel */
};

enum oplus_ofp_longrui_aod_config {					/* hardware capability */
	OPLUS_OFP_LONGRUI_AOD_IS_NOT_CONFIG = 0,
	OPLUS_OFP_NORMAL_TO_AOD_CONFIG = BIT(0),
	OPLUS_OFP_A_MIRROR_TO_THE_END_AOD_CONFIG = BIT(1),
	OPLUS_OFP_FULL_SCREEN_AOD_CONFIG = BIT(2),
};

enum oplus_ofp_longrui_aod_mode {					/* system setting */
	OPLUS_OFP_NORMAL_AOD_OFF = 0,
	OPLUS_OFP_AOD_ON = BIT(0),
	OPLUS_OFP_A_MIRROR_TO_THE_END_AOD_MODE = BIT(1),
	OPLUS_OFP_FULL_SCREEN_AOD_MODE = BIT(2),
	OPLUS_OFP_INSPIRATIONAL_PHOTO_FRAME = BIT(3),
};

/* remember to initialize params */
struct oplus_ofp_params {
	unsigned int fp_type;							/*
													 bit(0):lcd capacitive fingerprint(aod/fod aren't supported)
													 bit(1):oled capacitive fingerprint(only support aod)
													 bit(2):optical fingerprint old solution(dim layer && pressed icon are controlled by kernel)
													 bit(3):optical fingerprint new solution(dim layer && pressed icon aren't controlled by kernel)
													 bit(4):local hbm
													 bit(5):pressed icon brightness adaptation
													 bit(6):ultrasonic fingerprint
													 bit(7):ultra low power aod
													*/
	bool fp_type_compatible_mode;					/* indicates whether fp type compatible mode is set or not */
	bool ofp_support;								/* whether ofp is support or not */
	bool aod_unlocking;								/* whether is aod unlocking or not */
	bool aod_state;									/* whether panel is aod state or not */
	int doze_active;								/* DOZE_ACTIVE property value */
	unsigned int aod_light_mode;					/* 0:50nit, 1:10nit */
	unsigned int fake_aod_mode;						/*
													 indicates whether fake aod mode needs to be entered
													 bit(0):fake aod of primary display is enabled
													 bit(1):fake aod of secondary display is enabled
													*/
	bool ultra_low_power_aod_state;					/* indicates whether panel is ultra low power aod state or not */
	unsigned int ultra_low_power_aod_mode;			/* indicates whether ultra low power aod mode needs to be entered or not */
	unsigned int longrui_aod_config;				/*
													 bit(0):normal to aod can be supported by panel
													 bit(1):black frames of aod on/off can be removed by panel
													 bit(2):full screen aod can be supported by panel
													*/
	unsigned int longrui_aod_mode;					/*
													 bit(0):0:aod off 1:aod on
													 bit(1):a mirror to the end aod mode is enabled
													 bit(2):full screen aod mode is enabled
													*/
	unsigned int aod_off_hbm_on_delay;				/* do some frame delay to keep apart aod off cmd and hbm on cmd */
	ktime_t aod_off_cmd_timestamp;					/* record aod off cmd timestamp for aod off hbm on delay judgment */
	unsigned int dimlayer_hbm;						/* indicates whether the dimlayer and hbm should enable or not(reserved) */
	int hbm_enable;									/* HBM_ENABLE property value */
	bool need_to_update_lhbm_pressed_icon_gamma;	/* indicates whether lhbm pressed icon gamma needs to be read and updated or not */
	bool hbm_state;									/* whether panel is hbm state or not */
	unsigned int hbm_mode;							/* value of a node used for fingerprint calibration */
	bool fp_press;									/* whether pressed icon layer is ready or not */
	int pressed_icon_status;						/* indicate that pressed icon has been displayed in panel or not */
	int notifier_chain_value;						/* MTK_ONSCREENFINGERPRINT_EVENT notifier chain value */
	struct workqueue_struct *uiready_event_wq;		/* send uiready event workqueue */
	struct work_struct uiready_event_work;			/* use to send uiready event */
	struct hrtimer timer;							/* add for uiready notifier call chain */
	bool timing_switching;							/* indicates whether timing is switching in this frame or not */
	struct notifier_block touchpanel_event_notifier;/* add for touchpanel event notifier */
	struct workqueue_struct *aod_off_set_wq;		/* send aod off cmd workqueue */
	struct work_struct aod_off_set_work;			/* use to send aod off cmd to speed up aod unlocking */
};

/* log level config */
extern unsigned int oplus_ofp_log_level;

/* just use for ofp */
#define OFP_ERR(fmt, ...) \
	do { \
		if (oplus_ofp_log_level >= OPLUS_OFP_LOG_LEVEL_ERR) \
			OPLUS_DISP_ERR("OFP", fmt, ##__VA_ARGS__); \
	} while (0)

#define OFP_WARN(fmt, ...) \
	do { \
		if (oplus_ofp_log_level >= OPLUS_OFP_LOG_LEVEL_WARN) \
			OPLUS_DISP_WARN("OFP", fmt, ##__VA_ARGS__); \
	} while (0)

#define OFP_INFO(fmt, ...) \
	do { \
		if ((oplus_ofp_log_level >= OPLUS_OFP_LOG_LEVEL_INFO) || (g_mobile_log)) \
			OPLUS_DISP_INFO("OFP", fmt, ##__VA_ARGS__); \
		else \
			OPLUS_DISP_DEBUG("OFP", fmt, ##__VA_ARGS__); \
	} while (0)

#define OFP_DEBUG(fmt, ...) \
	do { \
		if ((oplus_ofp_log_level >= OPLUS_OFP_LOG_LEVEL_DEBUG) || (oplus_display_log_type & OPLUS_DEBUG_LOG_OFP) || (g_detail_log)) \
			OPLUS_DISP_INFO("OFP", fmt, ##__VA_ARGS__); \
		else \
			OPLUS_DISP_DEBUG("OFP", fmt, ##__VA_ARGS__); \
	} while (0)

#define OPLUS_OFP_TRACE_BEGIN(fmt, args...) \
	do { \
		if ((oplus_display_trace_enable & OPLUS_DISPLAY_OFP_TRACE_ENABLE) || (g_trace_log)) { \
			mtk_drm_print_trace("B|%d|"fmt"\n", current->tgid, ##args); \
		} \
	} while (0)

#define OPLUS_OFP_TRACE_END(fmt, args...) \
	do { \
		if ((oplus_display_trace_enable & OPLUS_DISPLAY_OFP_TRACE_ENABLE) || (g_trace_log)) { \
			mtk_drm_print_trace("E|%d|"fmt"\n", current->tgid, ##args); \
		} \
	} while (0)

#define OPLUS_OFP_TRACE_INT(fmt, args...) \
	do { \
		if ((oplus_display_trace_enable & OPLUS_DISPLAY_OFP_TRACE_ENABLE) || (g_trace_log)) { \
			mtk_drm_print_trace("C|%d|"fmt"\n", current->tgid, ##args); \
		} \
	} while (0)
extern int g_commit_pid;
extern struct drm_device *get_drm_device(void);
extern int mtkfb_set_aod_backlight_level(unsigned int level);
extern void mtk_drm_crtc_wk_lock(struct drm_crtc *crtc, bool get, const char *func, int line);

/* -------------------- oplus_ofp_params -------------------- */
int oplus_ofp_fp_type_compatible_mode_config(void);
int oplus_ofp_init(void *node);
int oplus_ofp_deinit(void);
bool oplus_ofp_is_supported(void);
bool oplus_ofp_video_mode_aod_fod_is_enabled(void);
bool oplus_ofp_local_hbm_is_enabled(void);
bool oplus_ofp_a_mirror_to_the_end_aod_mode_is_enabled(void);
bool oplus_ofp_need_to_do_aod_off_compensation(void);
bool oplus_ofp_need_to_skip_esd_check_after_aod_off(void);
bool oplus_ofp_get_aod_state(void);
int oplus_ofp_set_aod_state(bool aod_state);
bool oplus_ofp_get_fake_aod_mode(void);
int oplus_ofp_get_hbm_state(void);
int oplus_ofp_set_hbm_state(bool hbm_state);
int oplus_ofp_property_update(int prop_id, unsigned int prop_val);

/* -------------------- fod -------------------- */
int oplus_ofp_lhbm_pressed_icon_gamma_update(void *drm_crtc, void *LCM_setting_table);
int oplus_ofp_lhbm_backlight_update(void *drm_crtc);
int oplus_ofp_send_hbm_state_event(unsigned int hbm_state);
int oplus_ofp_hbm_handle(void *drm_crtc, void *mtk_crtc_state, void *cmdq_pkt);
int oplus_ofp_pressed_icon_status_update(int irq_type);
void oplus_ofp_uiready_event_work_handler(struct work_struct *work_item);
enum hrtimer_restart oplus_ofp_notify_uiready_timer_handler(struct hrtimer *timer);
int oplus_ofp_notify_uiready(void *mtk_drm_crtc);
int oplus_ofp_lhbm_pressed_icon_grayscale_update(void *para_list, unsigned int bl_level);
bool oplus_ofp_backlight_filter(void *drm_crtc, void *cmdq_pkt, unsigned int bl_level);

/* -------------------- aod -------------------- */
int oplus_ofp_video_mode_aod_handle(void *drm_crtc, void *mtk_panel_ext, void *drm_panel, void *mtk_dsi, void *dcs_write_gce, void *cmdq_handle);
int oplus_ofp_aod_off_status_handle(void *mtk_drm_crtc);
int oplus_ofp_doze_status_handle(bool doze_enable, void *drm_crtc, void *mtk_panel_ext, void *drm_panel, void *mtk_dsi, void *dcs_write_gce_pack);
int oplus_ofp_set_aod_light_mode_after_doze_enable(void *mtk_panel_ext, void *mtk_dsi, void *dcs_write_gce);
int oplus_ofp_set_ultra_low_power_aod_after_doze_enable(struct drm_panel *panel, void *mtk_panel_ext, void *mtk_dsi, void *dcs_write_gce);
void oplus_ofp_aod_off_set_work_handler(struct work_struct *work_item);
int oplus_ofp_touchpanel_event_notifier_call(struct notifier_block *nb, unsigned long action, void *data);
int oplus_ofp_aod_off_hbm_on_delay_check(void *mtk_drm_crtc);
int oplus_ofp_aod_off_backlight_recovery(void *drm_crtc, void *mtk_crtc_state, void *cmdq_pkt);

/* -------------------- node -------------------- */
/* fp_type */
int oplus_ofp_set_fp_type(void *buf);
int oplus_ofp_get_fp_type(void *buf);
ssize_t oplus_ofp_set_fp_type_attr(struct kobject *obj,
	struct kobj_attribute *attr, const char *buf, size_t count);
ssize_t oplus_ofp_get_fp_type_attr(struct kobject *obj,
	struct kobj_attribute *attr, char *buf);

/* fod part */
int oplus_ofp_set_dimlayer_hbm(void *buf);
int oplus_ofp_get_dimlayer_hbm(void *buf);
ssize_t oplus_ofp_set_dimlayer_hbm_attr(struct kobject *obj,
	struct kobj_attribute *attr, const char *buf, size_t count);
ssize_t oplus_ofp_get_dimlayer_hbm_attr(struct kobject *obj,
	struct kobj_attribute *attr, char *buf);
int oplus_ofp_notify_fp_press(void *buf);
ssize_t oplus_ofp_notify_fp_press_attr(struct kobject *obj, struct kobj_attribute *attr,
	const char *buf, size_t count);
int oplus_ofp_get_hbm(void *buf);
int oplus_ofp_set_hbm(void *buf);
ssize_t oplus_ofp_get_hbm_attr(struct kobject *obj,
	struct kobj_attribute *attr, char *buf);
ssize_t oplus_ofp_set_hbm_attr(struct kobject *obj,
	struct kobj_attribute *attr, const char *buf, size_t count);

/* aod part */
int oplus_ofp_set_aod_light_mode(void *buf);
int oplus_ofp_get_aod_light_mode(void *buf);
ssize_t oplus_ofp_set_aod_light_mode_attr(struct kobject *obj,
	struct kobj_attribute *attr, const char *buf, size_t count);
ssize_t oplus_ofp_get_aod_light_mode_attr(struct kobject *obj,
	struct kobj_attribute *attr, char *buf);
int oplus_ofp_set_fake_aod(void *buf);
int oplus_ofp_get_fake_aod(void *buf);
ssize_t oplus_ofp_set_fake_aod_attr(struct kobject *obj,
	struct kobj_attribute *attr, const char *buf, size_t count);
ssize_t oplus_ofp_get_fake_aod_attr(struct kobject *obj,
	struct kobj_attribute *attr, char *buf);
int oplus_ofp_set_ultra_low_power_aod_mode(void *buf);
int oplus_ofp_get_ultra_low_power_aod_mode(void *buf);
ssize_t oplus_ofp_set_ultra_low_power_aod_mode_attr(struct kobject *obj,
	struct kobj_attribute *attr, const char *buf, size_t count);
ssize_t oplus_ofp_get_ultra_low_power_aod_mode_attr(struct kobject *obj,
	struct kobj_attribute *attr, char *buf);
int oplus_ofp_set_longrui_aod_mode(void *buf);
int oplus_ofp_get_longrui_aod_config(void *buf);
ssize_t oplus_ofp_set_longrui_aod_mode_attr(struct kobject *obj,
	struct kobj_attribute *attr, const char *buf, size_t count);
ssize_t oplus_ofp_get_longrui_aod_config_attr(struct kobject *obj,
	struct kobj_attribute *attr, char *buf);

#endif /*_OPLUS_DISPLAY_ONSCREENFINGERPRINT_H_*/

