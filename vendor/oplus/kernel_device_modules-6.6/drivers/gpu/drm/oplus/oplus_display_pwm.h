/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_display_pwm.h
** Description : oplus display pwm header
** Version : 1.0
** Date : 2024/05/20
** Author : Display
******************************************************************/
#ifndef _OPLUS_DISPLAY_PWM_H_
#define _OPLUS_DISPLAY_PWM_H_

/* please just only include linux common head file  */
#include <linux/kobject.h>
#include <linux/iio/consumer.h>

#include "oplus_display_debug.h"

enum oplus_pwm_trubo_type {
	OPLUS_PWM_TRUBO_CLOSE = 0,
	OPLUS_PWM_TRUBO_SWITCH = 1,
	OPLUS_PWM_TRUBO_GLOBAL_OPEN_NO_SWITCH = 2,
};

enum PWM_SWITCH_STATE{
	PWM_SWITCH_DC_STATE = 0,
	PWM_SWITCH_HPWM_STATE,
	PWM_SWITCH_ONEPULSE_STATE,
};

enum PWM_SWITCH_BL_THRESHOLD{
	PWM_NORMAL_BL = 0,
	PWM_LOW_THRESHOLD = 1,
	PWM_HIGH_THRESHOLD = 2,
	PWM_MAX_THRESHOLD
};

enum pwm_switch_cmd_id {
	PWM_SWITCH_3TO18_RESTORE = 0,
	PWM_SWITCH_18TO3_RESTORE = 1,
	PWM_SWITCH_18TO3 = 2,
	PWM_SWITCH_3TO18 = 3,
	PWM_SWITCH_1TO3 = 4,
	PWM_SWITCH_3TO1 = 5,
	PWM_SWITCH_1TO18_RESTORE = 6,
	PWM_SWITCH_18TO1_RESTORE = 7,
	PWM_SWITCH_18TO1 = 8,
	PWM_SWITCH_1TO18 = 9,
};

/***  pwm turbo initialize params dtsi config   *****************
oplus,pwm-turbo-support= <1>;
oplus,pwm-turbo-plus-dbv=<0x643>;
oplus,pwm-turbo-wait-te=<1>;
********************************************************/
/* oplus pwm turbo initialize params ***************/
struct oplus_pwm_turbo_params {
	unsigned int   config;									/* int oplus,pwm-turbo-support */
	unsigned int   pwm_wait_te;
	unsigned int   lhbm_wait_te;
	unsigned int   pwm_fps_mode;
	bool pwm_turbo_support;									/* bool oplus,pwm-turbo-support */
	bool pwm_turbo_enabled;
	bool pwm_switch_support;
	bool pwm_power_on;
	u32  pwm_bl_threshold;									/* switch bl plus oplus,pwm-switch-backlight-threshold */
	u32  oplus_pwm_switch_state;
	u32  oplus_pwm_threshold;
	u32  pwm_pul_cmd_id;
	bool pwm_aid_switch_enable;
	bool pwm_onepulse_support;
	bool pwm_onepulse_enabled;
	bool oplus_pwm_switch_state_changed;
	u32  pwm_onepulse_switch_mode;
};

/* -------------------- extern ---------------------------------- */



/* ---------------- pwm debug log  ---------------- */
#define OPLUS_PWM_ERR(fmt, ...) \
	do { \
		if (oplus_display_log_level >= OPLUS_LOG_LEVEL_ERR) \
			OPLUS_DISP_ERR("PWM", fmt, ##__VA_ARGS__); \
	} while (0)

#define OPLUS_PWM_WARN(fmt, ...) \
	do { \
		if (oplus_display_log_level >= OPLUS_LOG_LEVEL_WARN) \
			OPLUS_DISP_WARN("PWM", fmt, ##__VA_ARGS__); \
	} while (0)

#define OPLUS_PWM_INFO(fmt, ...) \
	do { \
		if (oplus_display_log_level >= OPLUS_LOG_LEVEL_INFO) \
			OPLUS_DISP_INFO("PWM", fmt, ##__VA_ARGS__); \
	} while (0)

#define OPLUS_PWM_INFO_ONCE(fmt, ...) \
	do { \
		if (oplus_display_log_level >= OPLUS_LOG_LEVEL_INFO) \
			OPLUS_DISP_INFO_ONCE("PWM", fmt, ##__VA_ARGS__); \
	} while (0)

#define OPLUS_PWM_DEBUG(fmt, ...) \
	do { \
		if ((oplus_display_log_level >= OPLUS_LOG_LEVEL_DEBUG) || (oplus_display_log_type & OPLUS_DEBUG_LOG_PWM)) \
			OPLUS_DISP_INFO("PWM", fmt, ##__VA_ARGS__); \
		else	\
			OPLUS_DISP_DEBUG("PWM", fmt, ##__VA_ARGS__); \
	} while (0)

/* -------------------- function implementation ---------------------------------------- */
int oplus_pwm_turbo_probe(struct device_node *node);
void oplus_display_panel_wait_te(int cnt);
bool pwm_turbo_support(void);
int get_pwm_turbo_pulse_bl(void);
int get_pwm_turbo_fps_mode(void);
inline bool get_pwm_turbo_states(void);
inline bool oplus_panel_pwm_turbo_is_enabled(void);
inline bool oplus_panel_pwm_turbo_switch_state(void);
int oplus_display_panel_set_pwm_status(void *data);
int oplus_display_panel_get_pwm_status(void *buf);
int oplus_display_panel_get_pwm_status_for_90hz(void *buf);
void set_pwm_turbo_switch_state(enum PWM_SWITCH_STATE state);
void set_pwm_turbo_power_on(bool en);
/* -------------------- oplus api nodes ----------------------------------------------- */
ssize_t  oplus_display_get_high_pwm(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);
ssize_t  oplus_display_set_high_pwm(struct kobject *kobj,
				struct kobj_attribute *attr, const char *buf, size_t count);
inline bool oplus_panel_pwm_onepulse_is_enabled(void);
inline bool oplus_panel_pwm_onepulse_switch_state(void);
int oplus_panel_update_pwm_pulse_lock(bool enabled);
int oplus_display_panel_set_pwm_pulse(void *data);
int oplus_display_panel_get_pwm_pulse(void *data);
ssize_t oplus_get_pwm_pulse_debug(struct kobject *obj,
	struct kobj_attribute *attr, char *buf);
ssize_t oplus_set_pwm_pulse_debug(struct kobject *obj,
		struct kobj_attribute *attr,
		const char *buf, size_t count);

#endif /* _OPLUS_DISPLAY_PWM_H_ */
