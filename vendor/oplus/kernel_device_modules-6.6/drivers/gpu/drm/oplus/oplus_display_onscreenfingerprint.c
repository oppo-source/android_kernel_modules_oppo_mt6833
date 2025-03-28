/***************************************************************
** Copyright (C), 2022, OPLUS Mobile Comm Corp., Ltd
** File : oplus_display_onscreenfingerprint.c
** Description : oplus_display_onscreenfingerprint implement
** Version : 1.0
** Date : 2021/12/10
** Author : Display
***************************************************************/
#include "mtk_drm_drv.h"
#include "mtk_drm_helper.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_mmp.h"
#include "mtk_disp_notify.h"
#include "mtk_log.h"
#include "mtk_dump.h"

#include "oplus_display_onscreenfingerprint.h"
#include "oplus_display_utils.h"
#include "oplus_display_debug.h"
#include "oplus_display_device_ioctl.h"
#include "oplus_display_pwm.h"
#include "oplus_display_apollo_brightness.h"
#include "oplus_adfr.h"
#include <soc/oplus/touchpanel_event_notify.h>
#ifdef OPLUS_FEATURE_DISPLAY_TEMP_COMPENSATION
#include "oplus_display_temp_compensation.h"
#endif /* OPLUS_FEATURE_DISPLAY_TEMP_COMPENSATION */
#include "mtk_dsi.h"

/* -------------------- macro -------------------- */
/* fp type bit setting */
#define OPLUS_OFP_FP_TYPE_LCD_CAPACITIVE					(BIT(0))
#define OPLUS_OFP_FP_TYPE_OLED_CAPACITIVE					(BIT(1))
#define OPLUS_OFP_FP_TYPE_OPTICAL_OLD_SOLUTION				(BIT(2))
#define OPLUS_OFP_FP_TYPE_OPTICAL_NEW_SOLUTION				(BIT(3))
#define OPLUS_OFP_FP_TYPE_LOCAL_HBM							(BIT(4))
#define OPLUS_OFP_FP_TYPE_BRIGHTNESS_ADAPTATION				(BIT(5))
#define OPLUS_OFP_FP_TYPE_ULTRASONIC						(BIT(6))
#define OPLUS_OFP_FP_TYPE_ULTRA_LOW_POWER_AOD				(BIT(7))
#define OPLUS_OFP_FP_TYPE_VIDEO_MODE_AOD_FOD				(BIT(9))
/* get fp type config */
#define OPLUS_OFP_GET_LCD_CAPACITIVE_CONFIG(fp_type)		((fp_type) & OPLUS_OFP_FP_TYPE_LCD_CAPACITIVE)
#define OPLUS_OFP_GET_OLED_CAPACITIVE_CONFIG(fp_type)		((fp_type) & OPLUS_OFP_FP_TYPE_OLED_CAPACITIVE)
#define OPLUS_OFP_GET_OPTICAL_OLD_SOLUTION_CONFIG(fp_type)	((fp_type) & OPLUS_OFP_FP_TYPE_OPTICAL_OLD_SOLUTION)
#define OPLUS_OFP_GET_OPTICAL_NEW_SOLUTION_CONFIG(fp_type)	((fp_type) & OPLUS_OFP_FP_TYPE_OPTICAL_NEW_SOLUTION)
#define OPLUS_OFP_GET_LOCAL_HBM_CONFIG(fp_type)				((fp_type) & OPLUS_OFP_FP_TYPE_LOCAL_HBM)
#define OPLUS_OFP_GET_BRIGHTNESS_ADAPTATION_CONFIG(fp_type)	((fp_type) & OPLUS_OFP_FP_TYPE_BRIGHTNESS_ADAPTATION)
#define OPLUS_OFP_GET_ULTRASONIC_CONFIG(fp_type)			((fp_type) & OPLUS_OFP_FP_TYPE_ULTRASONIC)
#define OPLUS_OFP_GET_ULTRA_LOW_POWER_AOD_CONFIG(fp_type)	((fp_type) & OPLUS_OFP_FP_TYPE_ULTRA_LOW_POWER_AOD)
#define OPLUS_OFP_GET_VIDEO_MODE_AOD_FOD_CONFIG(fp_type)	((fp_type) & OPLUS_OFP_FP_TYPE_VIDEO_MODE_AOD_FOD)
/* dbv level */
#define OPLUS_OFP_900NIT_DBV_LEVEL 							(0x0DBB)

/* notifier event */
#define DRM_PANEL_EVENT_HBM_STATE							1
#define MTK_ONSCREENFINGERPRINT_EVENT						20

/* -------------------- parameters -------------------- */
/* log level config */
unsigned int oplus_ofp_log_level = OPLUS_OFP_LOG_LEVEL_INFO;
EXPORT_SYMBOL(oplus_ofp_log_level);
/* ofp global structure */
static struct oplus_ofp_params g_oplus_ofp_params = {0};

/* -------------------- extern -------------------- */
/* extern params */
struct LCM_setting_table {
	unsigned int cmd;
	unsigned int count;
	unsigned char para_list[256];
};
extern unsigned int m_db;
extern unsigned int last_backlight;
extern unsigned int oplus_display_brightness;

/* extern functions */
extern void lcdinfo_notify(unsigned long val, void *v);

/* -------------------- oplus_ofp_params -------------------- */
static struct oplus_ofp_params *oplus_ofp_get_params(void)
{
	return &g_oplus_ofp_params;
}

int oplus_ofp_fp_type_compatible_mode_config(void)
{
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	OFP_DEBUG("start\n");

	if (p_oplus_ofp_params->fp_type_compatible_mode) {
		if (m_db <= 6) {
			/* these panels do not support lhbm */
			p_oplus_ofp_params->fp_type = 0x08;
			OFP_INFO("fp_type:0x%x\n", p_oplus_ofp_params->fp_type);
		}
	}

	OFP_DEBUG("end\n");

	return 0;
}

bool oplus_ofp_video_mode_aod_fod_is_enabled(void)
{
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	if (!p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return 0;
	}

	return (bool)(OPLUS_OFP_GET_VIDEO_MODE_AOD_FOD_CONFIG(p_oplus_ofp_params->fp_type));
}

/* get fp_type value from panel dtsi */
int oplus_ofp_init(void *node)
{
	int rc = 0;
	unsigned int value = 0;
	struct device_node *devnode = node;
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	OFP_DEBUG("start\n");

	if (!devnode || !p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	rc = of_property_read_u32(devnode, "oplus,ofp-fp-type", &value);
	if (rc) {
		OFP_INFO("failed to read oplus,ofp-fp-type, rc=%d\n", rc);
		/* set default value to BIT(0) */
		p_oplus_ofp_params->fp_type = BIT(0);
	} else {
		p_oplus_ofp_params->fp_type = value;
	}

	OFP_INFO("fp_type:0x%x\n", p_oplus_ofp_params->fp_type);

	/* indicates whether fp type compatible mode is set or not */
	p_oplus_ofp_params->fp_type_compatible_mode = of_property_read_bool(devnode, "oplus,ofp-fp-type-compatible-mode");
	OFP_INFO("fp_type_compatible_mode:%d\n", p_oplus_ofp_params->fp_type_compatible_mode);

	if (oplus_ofp_is_supported()) {
		/* read by the framework for compatibility with different aod modes */
		rc = of_property_read_u32(devnode, "oplus,ofp-longrui-aod-config", &value);
		if (rc) {
			OFP_INFO("failed to read oplus,ofp-longrui-aod-config, rc=%d\n", rc);
			/* set default value to 0 */
			p_oplus_ofp_params->longrui_aod_config = 0;
		} else {
			p_oplus_ofp_params->longrui_aod_config = value;
		}
		OFP_INFO("longrui_aod_config:0x%x\n", p_oplus_ofp_params->longrui_aod_config);

		/* add for uiready notifier call chain */
		hrtimer_init(&p_oplus_ofp_params->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		p_oplus_ofp_params->timer.function = oplus_ofp_notify_uiready_timer_handler;

		/* add for touchpanel event notifier */
		p_oplus_ofp_params->touchpanel_event_notifier.notifier_call = oplus_ofp_touchpanel_event_notifier_call;
		rc = touchpanel_event_register_notifier(&p_oplus_ofp_params->touchpanel_event_notifier);
		if (rc < 0) {
			OFP_ERR("failed to register touchpanel event notifier: %d\n", rc);
		}

		/* add workqueue to send aod off cmd */
		p_oplus_ofp_params->aod_off_set_wq = create_singlethread_workqueue("aod_off_set");
		p_oplus_ofp_params->uiready_event_wq = create_singlethread_workqueue("uiready_event");
		INIT_WORK(&p_oplus_ofp_params->aod_off_set_work, oplus_ofp_aod_off_set_work_handler);
		INIT_WORK(&p_oplus_ofp_params->uiready_event_work, oplus_ofp_uiready_event_work_handler);

		if (oplus_ofp_local_hbm_is_enabled()) {
			/* indicates whether lhbm pressed icon gamma needs to be read and updated or not */
			p_oplus_ofp_params->need_to_update_lhbm_pressed_icon_gamma = of_property_read_bool(devnode, "oplus,ofp-need-to-update-lhbm-pressed-icon-gamma");
			OFP_INFO("need_to_update_lhbm_pressed_icon_gamma:%d\n", p_oplus_ofp_params->need_to_update_lhbm_pressed_icon_gamma);
		}
	}

	OFP_DEBUG("end\n");

	return rc;
}
EXPORT_SYMBOL(oplus_ofp_init);

int oplus_ofp_deinit(void)
{
	int rc = 0;
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	if (!p_oplus_ofp_params) {
		OFP_WARN("p_oplus_ofp_params is null\n");
		return -EFAULT;
	}
	if (!p_oplus_ofp_params->touchpanel_event_notifier.notifier_call) {
		OFP_WARN("p_oplus_ofp_params touchpanel_event_notifier notifier_call is null\n");
		return -EFAULT;
	}
	touchpanel_event_unregister_notifier(&p_oplus_ofp_params->touchpanel_event_notifier);
	if (p_oplus_ofp_params->aod_off_set_wq) {
		cancel_work_sync(&p_oplus_ofp_params->aod_off_set_work);
		flush_workqueue(p_oplus_ofp_params->aod_off_set_wq);
		destroy_workqueue(p_oplus_ofp_params->aod_off_set_wq);
		p_oplus_ofp_params->aod_off_set_wq = NULL;
	}
	if (p_oplus_ofp_params->uiready_event_wq) {
		cancel_work_sync(&p_oplus_ofp_params->uiready_event_work);
		flush_workqueue(p_oplus_ofp_params->uiready_event_wq);
		destroy_workqueue(p_oplus_ofp_params->uiready_event_wq);
		p_oplus_ofp_params->uiready_event_wq = NULL;
	}

	return rc;
}
EXPORT_SYMBOL(oplus_ofp_deinit);

bool oplus_ofp_is_supported(void)
{
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	if (!p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return false;
	}

	if (oplus_ofp_video_mode_aod_fod_is_enabled()) {
		return true;
	}
	/* global config, support oled panel */
	return (bool)(!OPLUS_OFP_GET_LCD_CAPACITIVE_CONFIG(p_oplus_ofp_params->fp_type));
}
EXPORT_SYMBOL(oplus_ofp_is_supported);

bool oplus_ofp_oled_capacitive_is_enabled(void)
{
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	if (!p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return false;
	}

	if (!oplus_ofp_is_supported()) {
		OFP_DEBUG("ofp is not support, oled capacitive is also not supported\n");
		return false;
	}

	return (bool)(OPLUS_OFP_GET_OLED_CAPACITIVE_CONFIG(p_oplus_ofp_params->fp_type));
}

bool oplus_ofp_optical_new_solution_is_enabled(void)
{
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	if (!p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return false;
	}

	if (!oplus_ofp_is_supported()) {
		OFP_DEBUG("ofp is not support, optical new solution is also not supported\n");
		return false;
	}

	return (bool)(OPLUS_OFP_GET_OPTICAL_NEW_SOLUTION_CONFIG(p_oplus_ofp_params->fp_type));
}

bool oplus_ofp_local_hbm_is_enabled(void)
{
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	if (!p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return false;
	}

	if (!oplus_ofp_is_supported()) {
		OFP_DEBUG("ofp is not support, local hbm is also not supported\n");
		return false;
	}

	return (bool)(OPLUS_OFP_GET_LOCAL_HBM_CONFIG(p_oplus_ofp_params->fp_type));
}
EXPORT_SYMBOL(oplus_ofp_local_hbm_is_enabled);

/*
static bool oplus_ofp_brightness_adaptation_is_enabled(void)
{
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	if (!p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return false;
	}

	if (!oplus_ofp_is_supported()) {
		OFP_DEBUG("ofp is not support, brightness adaptation is also not supported\n");
		return false;
	}

	return (bool)(OPLUS_OFP_GET_BRIGHTNESS_ADAPTATION_CONFIG(p_oplus_ofp_params->fp_type));
}
*/
bool oplus_ofp_ultrasonic_is_enabled(void)
{
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	if (!p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return false;
	}

	if (!oplus_ofp_is_supported()) {
		OFP_DEBUG("ofp is not support, ultrasonic is also not supported\n");
		return false;
	}

	return (bool)(OPLUS_OFP_GET_ULTRASONIC_CONFIG(p_oplus_ofp_params->fp_type));
}

bool oplus_ofp_ultra_low_power_aod_is_enabled(void)
{
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	if (!p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return false;
	}

	if (!oplus_ofp_is_supported()) {
		OFP_DEBUG("ofp is not support, ultra low power aod is also not supported\n");
		return false;
	}

	return (bool)(OPLUS_OFP_GET_ULTRA_LOW_POWER_AOD_CONFIG(p_oplus_ofp_params->fp_type));
}

bool oplus_ofp_a_mirror_to_the_end_aod_mode_is_enabled(void)
{
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	if (!p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return false;
	}

	if (!oplus_ofp_is_supported()) {
		OFP_DEBUG("ofp is not support, a mirror to the end aod mode is also not supported\n");
		return false;
	}

	return (bool)((p_oplus_ofp_params->longrui_aod_config & OPLUS_OFP_A_MIRROR_TO_THE_END_AOD_CONFIG)
					&& (p_oplus_ofp_params->longrui_aod_mode & OPLUS_OFP_A_MIRROR_TO_THE_END_AOD_MODE)
						&& (p_oplus_ofp_params->longrui_aod_mode & OPLUS_OFP_AOD_ON)
							&& !(oplus_ofp_optical_new_solution_is_enabled() && p_oplus_ofp_params->dimlayer_hbm));
}

bool oplus_ofp_need_to_do_aod_off_compensation(void)
{
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	OFP_DEBUG("start\n");

	if (!p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return false;
	}

	if (!oplus_ofp_is_supported()) {
		OFP_DEBUG("ofp is not supported\n");
		return false;
	}

	OFP_DEBUG("end\n");

	return (bool)((p_oplus_ofp_params->longrui_aod_config & OPLUS_OFP_A_MIRROR_TO_THE_END_AOD_CONFIG)
					&& (p_oplus_ofp_params->longrui_aod_mode & OPLUS_OFP_A_MIRROR_TO_THE_END_AOD_MODE)
						&& (p_oplus_ofp_params->longrui_aod_mode & OPLUS_OFP_INSPIRATIONAL_PHOTO_FRAME)
							&& p_oplus_ofp_params->aod_light_mode
								&& (!p_oplus_ofp_params->fp_press && !p_oplus_ofp_params->doze_active));
}
EXPORT_SYMBOL(oplus_ofp_need_to_do_aod_off_compensation);

bool oplus_ofp_need_to_skip_esd_check_after_aod_off(void)
{
	bool need_to_skip_esd_check_after_aod_off = false;
	static bool last_aod_state = false;
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	OFP_DEBUG("start\n");

	if (!p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return false;
	}

	if (!oplus_ofp_is_supported()) {
		OFP_DEBUG("ofp is not supported\n");
		return false;
	}

	need_to_skip_esd_check_after_aod_off = (bool)((p_oplus_ofp_params->longrui_aod_config & OPLUS_OFP_A_MIRROR_TO_THE_END_AOD_CONFIG)
													&& (p_oplus_ofp_params->longrui_aod_mode & OPLUS_OFP_A_MIRROR_TO_THE_END_AOD_MODE)
														&& !p_oplus_ofp_params->aod_unlocking
															&& (last_aod_state && !oplus_ofp_get_aod_state()));
	OFP_DEBUG("oplus_ofp_need_to_skip_esd_check_after_aod_off:%d\n", need_to_skip_esd_check_after_aod_off);
	last_aod_state = oplus_ofp_get_aod_state();

	OFP_DEBUG("end\n");

	return need_to_skip_esd_check_after_aod_off;
}

/* aod unlocking value update */
int oplus_ofp_aod_unlocking_update(void)
{
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	if (!p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	if (p_oplus_ofp_params->fp_press || p_oplus_ofp_params->doze_active != 0) {
		/* press icon layer is ready in aod mode */
		p_oplus_ofp_params->aod_unlocking = true;
		OFP_INFO("oplus_ofp_aod_unlocking: %d\n", p_oplus_ofp_params->aod_unlocking);
		OPLUS_OFP_TRACE_INT("oplus_ofp_aod_unlocking|%d", p_oplus_ofp_params->aod_unlocking);
	}

	return 0;
}

bool oplus_ofp_get_aod_state(void)
{
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	if (!p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return 0;
	}

	return p_oplus_ofp_params->aod_state;
}
EXPORT_SYMBOL(oplus_ofp_get_aod_state);

int oplus_ofp_set_aod_state(bool aod_state)
{
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	if (!p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	p_oplus_ofp_params->aod_state = aod_state;
	OFP_INFO("oplus_ofp_aod_state: %d\n", p_oplus_ofp_params->aod_state);
	OPLUS_OFP_TRACE_INT("oplus_ofp_aod_state|%d", p_oplus_ofp_params->aod_state);

	return 0;
}

bool oplus_ofp_get_fake_aod_mode(void)
{
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	OFP_DEBUG("start\n");

	if (!p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return false;
	}

	OPLUS_OFP_TRACE_BEGIN("oplus_ofp_get_fake_aod_mode");
	OFP_DEBUG("oplus_ofp_fake_aod_mode:%u\n", p_oplus_ofp_params->fake_aod_mode);
	OPLUS_OFP_TRACE_END("oplus_ofp_get_fake_aod_mode");

	OFP_DEBUG("end\n");

	return p_oplus_ofp_params->fake_aod_mode;
}
EXPORT_SYMBOL(oplus_ofp_get_fake_aod_mode);

int oplus_ofp_get_hbm_state(void)
{
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	if (!p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return 0;
	}

	return p_oplus_ofp_params->hbm_state;
}

int oplus_ofp_set_hbm_state(bool hbm_state)
{
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	if (!p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	p_oplus_ofp_params->hbm_state = hbm_state;
	OFP_INFO("oplus_ofp_hbm_state: %d\n", hbm_state);
	OPLUS_OFP_TRACE_INT("oplus_ofp_hbm_state|%d", hbm_state);

	oplus_ofp_send_hbm_state_event(hbm_state);

	return 0;
}

/* update doze_active and hbm_enable property value */
int oplus_ofp_property_update(int prop_id, unsigned int prop_val)
{
	static unsigned int last_disp_mode_idx = 0;
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	if (!p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	switch (prop_id) {
	case CRTC_PROP_DOZE_ACTIVE:
		if (prop_val != p_oplus_ofp_params->doze_active) {
			OFP_INFO("DOZE_ACTIVE:%d\n", prop_val);
		}
		p_oplus_ofp_params->doze_active = prop_val;
		OPLUS_OFP_TRACE_INT("oplus_ofp_doze_active|%d", p_oplus_ofp_params->doze_active);
		break;

	case CRTC_PROP_HBM_ENABLE:
		if (prop_val != p_oplus_ofp_params->hbm_enable) {
			OFP_INFO("HBM_ENABLE:%u,dim:%u,fingerpress:%u,icon:%u,aod:%u\n", prop_val, (prop_val & OPLUS_OFP_PROPERTY_DIM_LAYER),
				(prop_val & OPLUS_OFP_PROPERTY_FINGERPRESS_LAYER), (prop_val & OPLUS_OFP_PROPERTY_ICON_LAYER),
					(prop_val & OPLUS_OFP_PROPERTY_AOD_LAYER));
		}
		p_oplus_ofp_params->hbm_enable = prop_val;
		OPLUS_OFP_TRACE_INT("oplus_ofp_hbm_enable|%d", p_oplus_ofp_params->hbm_enable);
		break;
	case CRTC_PROP_DISP_MODE_IDX:
		if (prop_val != last_disp_mode_idx) {
			p_oplus_ofp_params->timing_switching = true;
			OFP_DEBUG("DISP_MODE_IDX:%u\n", prop_val);
		} else {
			p_oplus_ofp_params->timing_switching = false;
		}
		OFP_DEBUG("p_oplus_ofp_params->timing_switching:%d\n", p_oplus_ofp_params->timing_switching);
		OPLUS_OFP_TRACE_INT("oplus_ofp_timing_switching|%d", p_oplus_ofp_params->timing_switching);
		last_disp_mode_idx = prop_val;
		break;

	default:
		break;
	}

	return 0;
}


/* -------------------- fod -------------------- */
/*
 due to the fact that the brightness of lhbm pressed icon may change with the backlight,
 it is necessary to readjust the lhbm pressed icon gamma to meet the requirements of fingerprint unlocking
*/
int oplus_ofp_lhbm_pressed_icon_gamma_update(void *drm_crtc, void *LCM_setting_table)
{
	static bool calibrated = false;
	unsigned char registerswitchpage[4] = {0xFF, 0x08, 0x38, 0x2F};
	unsigned char register99[2] = {0x99, 0xB2};
	unsigned char rx_buf_0[5] = {0};
	unsigned char rx_buf_1[5] = {0};
	static unsigned char extrapolated_value[5][6] = {0};
	int rc = 0;
	int rgb_data_0[3] = {0};
	int rgb_data_1[3] = {0};
	int rgb_data_2[3] = {0};
	unsigned int i = 0;
	unsigned int j = 0;
	static unsigned int failure_count = 0;
	struct drm_crtc *crtc = drm_crtc;
	struct LCM_setting_table *table = LCM_setting_table;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	OFP_DEBUG("start\n");

	if (!oplus_ofp_local_hbm_is_enabled()) {
		OFP_DEBUG("local hbm is not enabled, no need to update lhbm pressed icon gamma\n");
		return 0;
	}

	if (!p_oplus_ofp_params) {
		OFP_ERR("Invalid p_oplus_ofp_params param\n");
		return -EINVAL;
	}

	if (!p_oplus_ofp_params->need_to_update_lhbm_pressed_icon_gamma) {
		OFP_DEBUG("need_to_update_lhbm_pressed_icon_gamma is not config, no need to update lhbm pressed icon gamma\n");
		return 0;
	}

	if (!calibrated && (failure_count < 100) && !LCM_setting_table) {
		if (!crtc) {
			OFP_ERR("Invalid crtc param\n");
			rc = -EINVAL;
			goto error;
		}

		mtk_crtc = to_mtk_crtc(crtc);
		if (!mtk_crtc) {
			OFP_ERR("Invalid mtk_crtc param\n");
			rc = -EINVAL;
			goto error;
		}

		if (!mtk_crtc->enabled) {
			OFP_ERR("mtk crtc is not enabled\n");
			rc = -EFAULT;
			goto error;
		}

		if (!mtk_drm_lcm_is_connect(mtk_crtc)) {
			OFP_ERR("lcm is not connect\n");
			rc = -EFAULT;
			goto error;
		}

		/* read lhbm pressed icon gamma */
		oplus_ddic_dsi_send_cmd(4, registerswitchpage);
		oplus_ddic_dsi_send_cmd(2, register99);
		oplus_ddic_dsi_read_cmd(0x96, 5, rx_buf_0);
		oplus_ddic_dsi_read_cmd(0x97, 5, rx_buf_1);
		register99[1] = 0x00;
		oplus_ddic_dsi_send_cmd(2, register99);

		/* readjust the corresponding gamma value through extrapolation */
		rgb_data_0[0] = ((rx_buf_0[0] & 0x0F) << 8) | rx_buf_0[2];
		rgb_data_0[1] = ((rx_buf_0[1] & 0xF0) << 4) | rx_buf_0[3];
		rgb_data_0[2] = ((rx_buf_0[1] & 0x0F) << 8) | rx_buf_0[4];
		rgb_data_1[0] = ((rx_buf_1[0] & 0x0F) << 8) | rx_buf_1[2];
		rgb_data_1[1] = ((rx_buf_1[1] & 0xF0) << 4) | rx_buf_1[3];
		rgb_data_1[2] = ((rx_buf_1[1] & 0x0F) << 8) | rx_buf_1[4];

		rgb_data_2[0] = (173 * 110 - 24700) * (rgb_data_1[0] - rgb_data_0[0]) / 800 + rgb_data_0[0];
		rgb_data_2[1] = (173 * 110 - 24700) * (rgb_data_1[1] - rgb_data_0[1]) / 800 + rgb_data_0[1];
		rgb_data_2[2] = (173 * 110 - 24700) * (rgb_data_1[2] - rgb_data_0[2]) / 800 + rgb_data_0[2];
		extrapolated_value[0][0] = 0x93;
		extrapolated_value[0][1] = rgb_data_2[0] >> 8;
		extrapolated_value[0][2] = ((rgb_data_2[1] & 0xF00) >> 4) | ((rgb_data_2[2] & 0xF00) >> 8);
		extrapolated_value[0][3] = rgb_data_2[0] & 0xFF;
		extrapolated_value[0][4] = rgb_data_2[1] & 0xFF;
		extrapolated_value[0][5] = rgb_data_2[2] & 0xFF;
		if (!extrapolated_value[0][1] && !extrapolated_value[0][2] && !extrapolated_value[0][3]
				&& !extrapolated_value[0][4] && !extrapolated_value[0][5]) {
			OFP_ERR("the extrapolated_value[0] are incorrect\n");
			rc = -EINVAL;
			goto error;
		}

		rgb_data_2[0] = (205 * 110 - 24700) * (rgb_data_1[0] - rgb_data_0[0]) / 800 + rgb_data_0[0];
		rgb_data_2[1] = (205 * 110 - 24700) * (rgb_data_1[1] - rgb_data_0[1]) / 800 + rgb_data_0[1];
		rgb_data_2[2] = (205 * 110 - 24700) * (rgb_data_1[2] - rgb_data_0[2]) / 800 + rgb_data_0[2];
		extrapolated_value[1][0] = 0x94;
		extrapolated_value[1][1] = rgb_data_2[0] >> 8;
		extrapolated_value[1][2] = ((rgb_data_2[1] & 0xF00) >> 4) | ((rgb_data_2[2] & 0xF00) >> 8);
		extrapolated_value[1][3] = rgb_data_2[0] & 0xFF;
		extrapolated_value[1][4] = rgb_data_2[1] & 0xFF;
		extrapolated_value[1][5] = rgb_data_2[2] & 0xFF;
		if (!extrapolated_value[1][1] && !extrapolated_value[1][2] && !extrapolated_value[1][3]
				&& !extrapolated_value[1][4] && !extrapolated_value[1][5]) {
			OFP_ERR("the extrapolated_value[1] are incorrect\n");
			rc = -EINVAL;
			goto error;
		}

		rgb_data_2[0] = (226 * 110 - 24700) * (rgb_data_1[0] - rgb_data_0[0]) / 800 + rgb_data_0[0];
		rgb_data_2[1] = (226 * 110 - 24700) * (rgb_data_1[1] - rgb_data_0[1]) / 800 + rgb_data_0[1];
		rgb_data_2[2] = (226 * 110 - 24700) * (rgb_data_1[2] - rgb_data_0[2]) / 800 + rgb_data_0[2];
		extrapolated_value[2][0] = 0x95;
		extrapolated_value[2][1] = rgb_data_2[0] >> 8;
		extrapolated_value[2][2] = ((rgb_data_2[1] & 0xF00) >> 4) | ((rgb_data_2[2] & 0xF00) >> 8);
		extrapolated_value[2][3] = rgb_data_2[0] & 0xFF;
		extrapolated_value[2][4] = rgb_data_2[1] & 0xFF;
		extrapolated_value[2][5] = rgb_data_2[2] & 0xFF;
		if (!extrapolated_value[2][1] && !extrapolated_value[2][2] && !extrapolated_value[2][3]
				&& !extrapolated_value[2][4] && !extrapolated_value[2][5]) {
			OFP_ERR("the extrapolated_value[2] are incorrect\n");
			rc = -EINVAL;
			goto error;
		}

		rgb_data_2[0] = (247 * 110 - 24700) * (rgb_data_1[0] - rgb_data_0[0]) / 800 + rgb_data_0[0];
		rgb_data_2[1] = (247 * 110 - 24700) * (rgb_data_1[1] - rgb_data_0[1]) / 800 + rgb_data_0[1];
		rgb_data_2[2] = (247 * 110 - 24700) * (rgb_data_1[2] - rgb_data_0[2]) / 800 + rgb_data_0[2];
		extrapolated_value[3][0] = 0x96;
		extrapolated_value[3][1] = rgb_data_2[0] >> 8;
		extrapolated_value[3][2] = ((rgb_data_2[1] & 0xF00) >> 4) | ((rgb_data_2[2] & 0xF00) >> 8);
		extrapolated_value[3][3] = rgb_data_2[0] & 0xFF;
		extrapolated_value[3][4] = rgb_data_2[1] & 0xFF;
		extrapolated_value[3][5] = rgb_data_2[2] & 0xFF;
		if (!extrapolated_value[3][1] && !extrapolated_value[3][2] && !extrapolated_value[3][3]
				&& !extrapolated_value[3][4] && !extrapolated_value[3][5]) {
			OFP_ERR("the extrapolated_value[3] are incorrect\n");
			rc = -EINVAL;
			goto error;
		}

		rgb_data_2[0] = (255 * 110 - 24700) * (rgb_data_1[0] - rgb_data_0[0]) / 800 + rgb_data_0[0];
		rgb_data_2[1] = (255 * 110 - 24700) * (rgb_data_1[1] - rgb_data_0[1]) / 800 + rgb_data_0[1];
		rgb_data_2[2] = (255 * 110 - 24700) * (rgb_data_1[2] - rgb_data_0[2]) / 800 + rgb_data_0[2];
		extrapolated_value[4][0] = 0x97;
		extrapolated_value[4][1] = rgb_data_2[0] >> 8;
		extrapolated_value[4][2] = ((rgb_data_2[1] & 0xF00) >> 4) | ((rgb_data_2[2] & 0xF00) >> 8);
		extrapolated_value[4][3] = rgb_data_2[0] & 0xFF;
		extrapolated_value[4][4] = rgb_data_2[1] & 0xFF;
		extrapolated_value[4][5] = rgb_data_2[2] & 0xFF;
		if (!extrapolated_value[4][1] && !extrapolated_value[4][2] && !extrapolated_value[4][3]
				&& !extrapolated_value[4][4] && !extrapolated_value[4][5]) {
			OFP_ERR("the extrapolated_value[4] are incorrect\n");
			rc = -EINVAL;
			goto error;
		}

		for (i = 0; i < 5; i++) {
			for (j = 0; j < 6; j++) {
				OFP_INFO("extrapolated_value[%u][%u] = 0x%02X\n", i, j, extrapolated_value[i][j]);
			}
		}

		calibrated = true;
		OFP_INFO("update lhbm pressed icon gamma successfully\n");
	}

	if (calibrated && !drm_crtc) {
		if (!table) {
			OFP_ERR("Invalid table param\n");
			rc = -EINVAL;
		} else {
			/* update pressed icon gamma value */
			for (i = 0; i < 5; i++) {
				memcpy(table[2+i].para_list, extrapolated_value[i], 6);
			}
			OFP_INFO("update lhbm pressed icon gamma cmds value\n");
			rc = 1;
		}
	}

error:
	/* if gamma read fails more than 100 times, no further operation will be performed */
	if (!calibrated && (failure_count < 100)) {
		failure_count++;
		OFP_ERR("failure_count:%u\n", failure_count);
	}

	OFP_DEBUG("end\n");

	return rc;
}
EXPORT_SYMBOL(oplus_ofp_lhbm_pressed_icon_gamma_update);

/*
 when the backlight is in the outdoor hbm state, the brightness of the lhbm pressed icon is too high, which causes the unlocking failure.
 therefore, the backlight brightness must be limited to the normal state in the fingerprint unlocking scenario
*/
int oplus_ofp_lhbm_backlight_update(void *drm_crtc)
{
	int rc = 0;
	static int current_backlight = 0;
	static unsigned int last_dimlayer_hbm = 0;
	struct drm_crtc *crtc = drm_crtc;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	OFP_DEBUG("start\n");

	if (!oplus_ofp_local_hbm_is_enabled()) {
		OFP_DEBUG("local hbm is not enabled, no need to update backlight after dimlayer_hbm on/off\n");
		return 0;
	}

	if (!crtc || !p_oplus_ofp_params) {
		OFP_ERR("Invalid input params\n");
		return -EINVAL;
	}

	if (drm_crtc_index(crtc)) {
		OFP_DEBUG("not in dsi mode, should not update backlight after dimlayer_hbm on/off\n");
		return 0;
	}

	mtk_crtc = to_mtk_crtc(crtc);
	if (!mtk_crtc) {
		OFP_ERR("Invalid mtk_crtc param\n");
		return -EINVAL;
	}

	if (!mtk_crtc->oplus_apollo_br) {
		OFP_ERR("Invalid oplus_apollo_br param\n");
		return -EINVAL;
	}

	if (mtk_crtc->oplus_apollo_br->oplus_backlight_updated) {
		current_backlight = mtk_crtc->oplus_apollo_br->oplus_pending_backlight;
		OFP_DEBUG("current_backlight:%d\n", current_backlight);
	}

	if (!last_dimlayer_hbm && p_oplus_ofp_params->dimlayer_hbm) {
		if ((mtk_crtc->oplus_apollo_br->oplus_pending_backlight > OPLUS_OFP_900NIT_DBV_LEVEL)
			&& !p_oplus_ofp_params->doze_active) {
			OFP_INFO("set backlight level to OPLUS_OFP_900NIT_DBV_LEVEL after dimlayer_hbm on\n");
			mtk_crtc->oplus_apollo_br->oplus_backlight_updated = true;
			mtk_crtc->oplus_apollo_br->oplus_pending_backlight = OPLUS_OFP_900NIT_DBV_LEVEL;
		}
	} else if (last_dimlayer_hbm && !p_oplus_ofp_params->dimlayer_hbm) {
		if ((current_backlight > OPLUS_OFP_900NIT_DBV_LEVEL
			&& !oplus_ofp_get_aod_state())) {
			OFP_INFO("recovery backlight level to %u after dimlayer_hbm off\n", current_backlight);
			mtk_crtc->oplus_apollo_br->oplus_backlight_updated = true;
			mtk_crtc->oplus_apollo_br->oplus_pending_backlight = current_backlight;
		}
	} else if (p_oplus_ofp_params->dimlayer_hbm && (mtk_crtc->oplus_apollo_br->oplus_pending_backlight > OPLUS_OFP_900NIT_DBV_LEVEL)) {
		mtk_crtc->oplus_apollo_br->oplus_pending_backlight = OPLUS_OFP_900NIT_DBV_LEVEL;
		OFP_INFO("dimlayer_hbm is on and backlight lvl is greater than OPLUS_OFP_900NIT_DBV_LEVEL, set backlight to OPLUS_OFP_900NIT_DBV_LEVEL\n");
	}

	last_dimlayer_hbm = p_oplus_ofp_params->dimlayer_hbm;

	OFP_DEBUG("end\n");

	return rc;
}

int oplus_ofp_send_hbm_state_event(unsigned int hbm_state)
{
	OFP_DEBUG("start\n");

	if (oplus_ofp_local_hbm_is_enabled()) {
		OFP_DEBUG("local hbm is enabled, no need to send hbm state event\n");
		return 0;
	}

	lcdinfo_notify(DRM_PANEL_EVENT_HBM_STATE, &hbm_state);
	OFP_INFO("DRM_PANEL_EVENT_HBM_STATE:%u\n", hbm_state);

	OFP_DEBUG("end\n");

	return 0;
}

/* wait te and delay while using cmdq */
static int oplus_ofp_cmdq_pkt_wait(struct mtk_drm_crtc *mtk_crtc, struct cmdq_pkt *cmdq_handle, int te_count, int delay_us, bool hbm_en, bool before_hbm)
{
	int wait_te_count = te_count;
	struct mtk_ddp_comp *comp = NULL;

	if ((wait_te_count <= 0) && (delay_us <= 0)) {
		return 0;
	}

	if (!mtk_crtc) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (!comp || !comp->funcs || !comp->funcs->io_cmd) {
		OFP_ERR("Invalid comp params\n");
		return -EINVAL;
	}


	if (mtk_crtc_is_frame_trigger_mode(&mtk_crtc->base)) {
		if (cmdq_handle == NULL) {
			cmdq_handle = cmdq_pkt_create(mtk_crtc->gce_obj.client[CLIENT_CFG]);
		}

		OFP_INFO("wait %d te and delay %dus\n", wait_te_count, delay_us);

		if (wait_te_count > 0) {
			if (mtk_crtc_is_event_loop_active(mtk_crtc)) {
				cmdq_pkt_clear_event(cmdq_handle, mtk_crtc->gce_obj.event[EVENT_SYNC_TOKEN_TE]);
			} else {
				cmdq_pkt_clear_event(cmdq_handle, mtk_crtc->gce_obj.event[EVENT_TE]);
			}

			while (wait_te_count > 0) {
				OFP_DEBUG("start to wait EVENT_TE, remain %d te count\n", wait_te_count);
				if (mtk_crtc_is_event_loop_active(mtk_crtc)) {
					cmdq_pkt_wfe(cmdq_handle, mtk_crtc->gce_obj.event[EVENT_SYNC_TOKEN_TE]);
				} else {
					cmdq_pkt_wfe(cmdq_handle, mtk_crtc->gce_obj.event[EVENT_TE]);
				}
				OFP_DEBUG("complete the EVENT_TE waiting\n");
				wait_te_count--;
			}
		}

		if (delay_us > 0) {
			OFP_DEBUG("start to sleep %d us", delay_us);
			cmdq_pkt_sleep(cmdq_handle, CMDQ_US_TO_TICK(delay_us), CMDQ_GPR_R14);
		}
	} else {
		while (wait_te_count) {
			mtk_drm_idlemgr_kick(__func__, &mtk_crtc->base, 0);
			wait_te_count--;
			comp->funcs->io_cmd(comp, NULL, DSI_HBM_WAIT, NULL);
		}

		if (delay_us > 0) {
			OFP_DEBUG("start to sleep %d us", delay_us);
			cmdq_pkt_sleep(cmdq_handle, CMDQ_US_TO_TICK(delay_us), mtk_get_gpr(comp, cmdq_handle));
		}
	}

	return 0;
}

static int oplus_ofp_hbm_wait_handle(struct drm_crtc *crtc, struct cmdq_pkt *cmdq_handle, bool before_hbm, bool hbm_en)
{
	unsigned int refresh_rate = 0;
	unsigned int us_per_frame = 0;
	unsigned int te_count = 0;
	unsigned int delay_us = 0;
	int ret = 0;
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();
	struct mtk_panel_params *panel_ext = mtk_drm_get_lcm_ext_params(crtc);
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct cmdq_pkt *cmdq_handle2;

	if (!p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	if (!panel_ext | !mtk_crtc) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	if(oplus_ofp_video_mode_aod_fod_is_enabled()) {
		OFP_DEBUG("video mode No need to wait\n");
		return 0;
	}

	if (p_oplus_ofp_params->aod_unlocking && hbm_en && !p_oplus_ofp_params->aod_off_hbm_on_delay
			&& !panel_ext->oplus_ofp_need_to_sync_data_in_aod_unlocking) {
		OFP_DEBUG("no need to delay in aod unlocking\n");
		return 0;
	}

	OPLUS_OFP_TRACE_BEGIN("oplus_ofp_hbm_wait_handle");

	refresh_rate = panel_ext->dyn_fps.vact_timing_fps;
	us_per_frame = 1000000/refresh_rate;

	if (before_hbm) {
		if (hbm_en) {
			if (p_oplus_ofp_params->aod_unlocking == true) {
				if (panel_ext->oplus_ofp_aod_off_insert_black > 0) {
					bool need_aod_off_hbm_on_delay = (bool)p_oplus_ofp_params->aod_off_hbm_on_delay;

					if (panel_ext->oplus_ofp_aod_off_black_frame_total_time > 0) {
						if (ktime_sub(ktime_get(), p_oplus_ofp_params->aod_off_cmd_timestamp) >= ms_to_ktime(panel_ext->oplus_ofp_aod_off_black_frame_total_time)) {
							/* as te_rdy irq will be disabled in idle mode, so the aod_off_hbm_on_delay
							may not be accurate. then add oplus_ofp_aod_off_black_frame_total_time
							to check whether the timestamp of hbm cmd is in the black frames or not */
							OFP_DEBUG("no need to do some delay because it's not in the black frames\n");
							need_aod_off_hbm_on_delay = false;
						}
					}

					if (need_aod_off_hbm_on_delay) {
						/* do some frame delay to keep apart aod off cmd and hbm on cmd */
						te_count = p_oplus_ofp_params->aod_off_hbm_on_delay;
						if (refresh_rate == 120) {
							delay_us = (us_per_frame >> 1) + 900;
						} else {
							delay_us = (us_per_frame >> 1) + (us_per_frame >> 2);
						}
						p_oplus_ofp_params->aod_off_hbm_on_delay = 0;
						OFP_INFO("wait %d te and %dus to keep apart aod off cmd and hbm on cmd\n", te_count, delay_us);
					} else if (panel_ext->oplus_ofp_need_to_sync_data_in_aod_unlocking) {
						if (p_oplus_ofp_params->timing_switching) {
							/*
							 when timing switch cmds and hbm on cmds are sent at the same frame,
							 ddic has not changed to the latest frame rate, but delay has been calculated
							 according to the latest frame rate which would cause flicker issue,
							 so add one te wait to fix it
							*/
							te_count = 2;
						} else {
							/* wait 2 te ,then send hbm on cmds in the second half of the frame */
							te_count = 2;
						}
						if (refresh_rate == 120) {
							delay_us = (us_per_frame >> 1) + 900;
						} else {
							delay_us = (us_per_frame >> 1) + (us_per_frame >> 2);
						}
					}
				} else if (panel_ext->oplus_ofp_need_to_sync_data_in_aod_unlocking) {
					/* wait 1 te ,then send hbm on cmds in the second half of the frame */
					te_count = 1;
					if (refresh_rate == 120) {
						delay_us = (us_per_frame >> 1) + 900;
					} else {
						delay_us = (us_per_frame >> 1) + (us_per_frame >> 2);
					}
				}
			} else {
				/* backlight will affect hbm on/off time in some panel, need to keep apart the 51 cmd for stable hbm time */
				if (panel_ext->oplus_ofp_need_keep_apart_backlight) {
					/* flush the blocking frame , otherwise the dim Layer would be delay to next frame so that flicker happen */
					OPLUS_OFP_TRACE_BEGIN("cmdq_handle2");
					cmdq_handle2 = cmdq_pkt_create(mtk_crtc->gce_obj.client[CLIENT_CFG]);
					cmdq_pkt_wait_no_clear(cmdq_handle2, mtk_crtc->gce_obj.event[EVENT_STREAM_EOF]);
					/* delay one frame */
					cmdq_pkt_sleep(cmdq_handle2, CMDQ_US_TO_TICK(us_per_frame), CMDQ_GPR_R14);
					cmdq_pkt_flush(cmdq_handle2);
					cmdq_pkt_destroy(cmdq_handle2);
					OPLUS_OFP_TRACE_END("cmdq_handle2");

					/* send hbm on cmd in next frame */
					te_count = 2;
				} else {
					te_count = 1;
				}

				if (refresh_rate == 120) {
					delay_us = (us_per_frame >> 1) + 900;
				} else {
					delay_us = (us_per_frame >> 1) + (us_per_frame >> 2);
				}
			}
		} else {
			if (panel_ext->oplus_ofp_pre_hbm_off_delay) {
				te_count = 1;
				/* the delay time bfore hbm off */
				delay_us = panel_ext->oplus_ofp_pre_hbm_off_delay * 1000;

				/* flush the blocking frame , otherwise the dim Layer would be delay to next frame so that flicker happen */
				OPLUS_OFP_TRACE_BEGIN("cmdq_handle2");
				cmdq_handle2 = cmdq_pkt_create(mtk_crtc->gce_obj.client[CLIENT_CFG]);
				cmdq_pkt_wait_no_clear(cmdq_handle2, mtk_crtc->gce_obj.event[EVENT_STREAM_EOF]);
				/* delay some time to wait for data */
				cmdq_pkt_sleep(cmdq_handle2, CMDQ_US_TO_TICK(delay_us), CMDQ_GPR_R14);
				cmdq_pkt_flush(cmdq_handle2);
				cmdq_pkt_destroy(cmdq_handle2);
				OPLUS_OFP_TRACE_END("cmdq_handle2");
			}
		}
	} else {
		if (hbm_en) {
			if (p_oplus_ofp_params->aod_unlocking == true) {
				OFP_DEBUG("no need to delay after hbm on cmds have been sent in aod unlocking\n");
			} else if (panel_ext->oplus_ofp_hbm_on_delay) {
				te_count = 0;
				/* the time when hbm on need */
				delay_us = panel_ext->oplus_ofp_hbm_on_delay * 1000;
			}
		} else {
			if (panel_ext->oplus_ofp_hbm_off_delay) {
				te_count = 0;
				/* the time when hbm off need */
				delay_us = panel_ext->oplus_ofp_hbm_off_delay * 1000;
			}
		}
	}

	OFP_INFO("before_hbm = %d, te_count = %d, delay_us = %d\n", before_hbm, te_count, delay_us);
	ret = oplus_ofp_cmdq_pkt_wait(mtk_crtc, cmdq_handle, te_count, delay_us, hbm_en, before_hbm);
	if (ret) {
		OFP_ERR("oplus_ofp_cmdq_pkt_wait failed\n");
	}

	OPLUS_OFP_TRACE_END("oplus_ofp_hbm_wait_handle");

	return ret;
}

static int oplus_ofp_set_panel_hbm(struct drm_crtc *crtc, bool hbm_en)
{
	bool doze_en = false;
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp = mtk_ddp_comp_request_output(mtk_crtc);
	struct cmdq_pkt *cmdq_handle;

	if (!p_oplus_ofp_params || !crtc || !mtk_crtc) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	if (oplus_ofp_get_hbm_state() == hbm_en) {
		OFP_DEBUG("already in hbm state %d\n", hbm_en);
		return 0;
	}

	if (!(comp && comp->funcs && comp->funcs->io_cmd)) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	if (!(mtk_crtc->enabled)) {
		OFP_ERR("skip, slept\n");
		return -EINVAL;
	}

	OPLUS_OFP_TRACE_BEGIN("oplus_ofp_set_panel_hbm");

	OPLUS_OFP_TRACE_BEGIN("mtk_drm_send_lcm_cmd_prepare");
	OFP_INFO("prepare to send hbm cmd\n");
	mtk_drm_send_lcm_cmd_prepare(crtc, &cmdq_handle);
	OPLUS_OFP_TRACE_END("mtk_drm_send_lcm_cmd_prepare");

	if (oplus_ofp_get_aod_state()) {
		OFP_INFO("send aod off cmd before hbm on because panel is still in aod mode\n");
		oplus_ofp_aod_off_status_handle(mtk_crtc);

		OPLUS_OFP_TRACE_BEGIN("DSI_SET_DOZE");
		comp->funcs->io_cmd(comp, cmdq_handle, DSI_SET_DOZE, &doze_en);
		OFP_INFO("DSI_SET_DOZE %d\n", doze_en);
		OPLUS_OFP_TRACE_END("DSI_SET_DOZE");
	}

	/* delay before hbm cmd */
	oplus_ofp_hbm_wait_handle(crtc, cmdq_handle, true, hbm_en);

	if (oplus_ofp_local_hbm_is_enabled()) {
		/* send lhbm pressed icon cmd */
		OPLUS_OFP_TRACE_BEGIN("OPLUS_OFP_SET_LHBM_PRESSED_ICON");
		comp->funcs->io_cmd(comp, cmdq_handle, OPLUS_OFP_SET_LHBM_PRESSED_ICON, &hbm_en);
		OFP_INFO("OPLUS_OFP_SET_LHBM_PRESSED_ICON %d\n", hbm_en);
		OPLUS_OFP_TRACE_END("OPLUS_OFP_SET_LHBM_PRESSED_ICON");
	} else {
		/* send hbm cmd */
		OPLUS_OFP_TRACE_BEGIN("DSI_HBM_SET");
		comp->funcs->io_cmd(comp, cmdq_handle, DSI_HBM_SET, &hbm_en);
		OFP_INFO("DSI_HBM_SET %d\n", hbm_en);
		OPLUS_OFP_TRACE_END("DSI_HBM_SET");
	}

	/* delay after hbm cmd */
	oplus_ofp_hbm_wait_handle(crtc, cmdq_handle, false, hbm_en);

	OPLUS_OFP_TRACE_BEGIN("mtk_drm_send_lcm_cmd_flush");
	mtk_drm_send_lcm_cmd_flush(crtc, &cmdq_handle, true);
	oplus_ofp_set_hbm_state(hbm_en);
	OPLUS_OFP_TRACE_END("mtk_drm_send_lcm_cmd_flush");

	if (hbm_en == false && p_oplus_ofp_params->aod_unlocking == true) {
		p_oplus_ofp_params->aod_unlocking = false;
		OFP_INFO("oplus_ofp_aod_unlocking: %d\n", p_oplus_ofp_params->aod_unlocking);
		OPLUS_OFP_TRACE_INT("oplus_ofp_aod_unlocking|%d", p_oplus_ofp_params->aod_unlocking);
	}

	OFP_INFO("hbm cmd is flushed\n");
	OPLUS_OFP_TRACE_END("oplus_ofp_set_panel_hbm");

	return 0;
}

int oplus_ofp_hbm_handle(void *drm_crtc, void *mtk_crtc_state, void *cmdq_pkt)
{
	uint64_t hbm_enable = 0;
	struct drm_crtc *crtc = drm_crtc;
	struct mtk_crtc_state *state = mtk_crtc_state;
	struct cmdq_pkt *cmdq_handle = cmdq_pkt;
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	if (!crtc || !state || !cmdq_handle || !p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	if (p_oplus_ofp_params->hbm_mode) {
		OFP_DEBUG("already in hbm mode %u\n", p_oplus_ofp_params->hbm_mode);
		return -EFAULT;
	}

	OPLUS_OFP_TRACE_BEGIN("oplus_ofp_hbm_handle");

	hbm_enable = state->prop_val[CRTC_PROP_HBM_ENABLE];
	OFP_DEBUG("hbm_enable:%lu, oplus_display_brightness=%u\n", hbm_enable, oplus_display_brightness);

	if (mtk_crtc_is_frame_trigger_mode(crtc) || oplus_ofp_video_mode_aod_fod_is_enabled()) {
		if ((!state->prop_val[CRTC_PROP_DOZE_ACTIVE] && (hbm_enable & OPLUS_OFP_PROPERTY_DIM_LAYER
			|| hbm_enable & OPLUS_OFP_PROPERTY_FINGERPRESS_LAYER) && oplus_display_brightness != 0)
			|| (state->prop_val[CRTC_PROP_DOZE_ACTIVE] && (hbm_enable & OPLUS_OFP_PROPERTY_FINGERPRESS_LAYER)
			&& oplus_display_brightness != 0)) {
			OFP_DEBUG("set hbm on\n");
			oplus_ofp_set_panel_hbm(crtc, true);

			if (!oplus_ofp_local_hbm_is_enabled()) {
				/*bypass pq when enter hbm */
				mtk_atomic_hbm_bypass_pq(crtc, cmdq_handle, 1);
				OFP_DEBUG("bypass pq in hbm mode\n");
			}
		} else if ((!(hbm_enable & OPLUS_OFP_PROPERTY_DIM_LAYER)
					&& !(hbm_enable & OPLUS_OFP_PROPERTY_FINGERPRESS_LAYER))
						|| !p_oplus_ofp_params->fp_press || oplus_display_brightness == 0) {
			OFP_DEBUG("set hbm off\n");
			oplus_ofp_set_panel_hbm(crtc, false);

			if (!oplus_ofp_local_hbm_is_enabled()) {
				/*no need to bypass pq when exit hbm */
				mtk_atomic_hbm_bypass_pq(crtc, cmdq_handle, 0);
				OFP_DEBUG("no need to bypass pq in normal mode\n");
			}
		}
	}

	OPLUS_OFP_TRACE_END("oplus_ofp_hbm_handle");

	return 0;
}

/* update pressed icon status */
int oplus_ofp_pressed_icon_status_update(int irq_type)
{
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();
	static int last_hbm_enable = 0;

	if (!p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	OPLUS_OFP_TRACE_BEGIN("oplus_ofp_pressed_icon_status_update");
	if (irq_type == OPLUS_OFP_FRAME_DONE) {
		if ((!(last_hbm_enable & OPLUS_OFP_PROPERTY_FINGERPRESS_LAYER))
				&& (p_oplus_ofp_params->hbm_enable & OPLUS_OFP_PROPERTY_FINGERPRESS_LAYER)) {
			/* pressed icon has been flush to DDIC ram */
			p_oplus_ofp_params->pressed_icon_status = OPLUS_OFP_PRESSED_ICON_ON_FRAME_DONE;
			OFP_INFO("pressed icon status: OPLUS_OFP_PRESSED_ICON_ON_FRAME_DONE\n");
		} else if ((last_hbm_enable & OPLUS_OFP_PROPERTY_FINGERPRESS_LAYER)
						&& (!(p_oplus_ofp_params->hbm_enable & OPLUS_OFP_PROPERTY_FINGERPRESS_LAYER))) {
			/* pressed icon has not been flush to DDIC ram */
			p_oplus_ofp_params->pressed_icon_status = OPLUS_OFP_PRESSED_ICON_OFF_FRAME_DONE;
			OFP_INFO("pressed icon status: OPLUS_OFP_PRESSED_ICON_OFF_FRAME_DONE\n");
		}
		last_hbm_enable = p_oplus_ofp_params->hbm_enable;
	} else if (irq_type == OPLUS_OFP_TE_RDY) {
		if (p_oplus_ofp_params->pressed_icon_status == OPLUS_OFP_PRESSED_ICON_ON_FRAME_DONE) {
			/* pressed icon has been displayed in panel */
			p_oplus_ofp_params->pressed_icon_status = OPLUS_OFP_PRESSED_ICON_ON;
			OFP_INFO("pressed icon status: OPLUS_OFP_PRESSED_ICON_ON\n");
		} else if (p_oplus_ofp_params->pressed_icon_status == OPLUS_OFP_PRESSED_ICON_OFF_FRAME_DONE) {
			/* pressed icon has not been displayed in panel */
			p_oplus_ofp_params->pressed_icon_status = OPLUS_OFP_PRESSED_ICON_OFF;
			OFP_INFO("pressed icon status: OPLUS_OFP_PRESSED_ICON_OFF\n");
		}
	}
	OPLUS_OFP_TRACE_INT("oplus_ofp_pressed_icon_status|%d", p_oplus_ofp_params->pressed_icon_status);
	OPLUS_OFP_TRACE_END("oplus_ofp_pressed_icon_status_update");

	return 0;
}

/* timer */
enum hrtimer_restart oplus_ofp_notify_uiready_timer_handler(struct hrtimer *timer)
{
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	if (!p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	OPLUS_OFP_TRACE_BEGIN("oplus_ofp_notify_uiready_timer_handler");
	/* notifer fingerprint that pressed icon ui is ready */
	OFP_INFO("send uiready: %d\n", p_oplus_ofp_params->notifier_chain_value);
	queue_work(p_oplus_ofp_params->uiready_event_wq, &p_oplus_ofp_params->uiready_event_work);
	OPLUS_OFP_TRACE_INT("oplus_ofp_notifier_chain_value|%d", p_oplus_ofp_params->notifier_chain_value);
	OPLUS_OFP_TRACE_END("oplus_ofp_notify_uiready_timer_handler");

	return HRTIMER_NORESTART;
}

void oplus_ofp_uiready_event_work_handler(struct work_struct *work_item)
{
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	OFP_DEBUG("start\n");

	if (!p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return;
	}

	OPLUS_OFP_TRACE_BEGIN("oplus_ofp_uiready_event_work_handler");

	/* hbm is taking effect, so send uiready immediately */
	OFP_INFO("send uiready:%u\n", p_oplus_ofp_params->notifier_chain_value);
	mtk_disp_notifier_call_chain(MTK_ONSCREENFINGERPRINT_EVENT, &p_oplus_ofp_params->notifier_chain_value);
	OPLUS_OFP_TRACE_INT("oplus_ofp_notifier_chain_value|%d", p_oplus_ofp_params->notifier_chain_value);

	OPLUS_OFP_TRACE_END("oplus_ofp_uiready_event_work_handler");

	OFP_DEBUG("end\n");

	return;
}

/* notify uiready */
int oplus_ofp_notify_uiready(void *mtk_drm_crtc)
{
	static int last_notifier_chain_value = 0;
	static int last_hbm_state = false;
	static ktime_t hbm_cmd_timestamp = 0;
	ktime_t delta_time = 0;
	unsigned int refresh_rate = 0;
	unsigned int delay_ms = 0;
	struct mtk_drm_crtc *mtk_crtc = mtk_drm_crtc;
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	if (!mtk_crtc || !p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	if (p_oplus_ofp_params->aod_unlocking == true) {
		if (last_hbm_state == false && (oplus_ofp_get_hbm_state() == true)) {
			/* hbm cmd is sent to ddic */
			hbm_cmd_timestamp = ktime_get();
			OFP_INFO("hbm_cmd_timestamp:%lld\n", ktime_to_ms(hbm_cmd_timestamp));
		}

		if ((p_oplus_ofp_params->fp_press == true) && (p_oplus_ofp_params->hbm_enable & OPLUS_OFP_PROPERTY_FINGERPRESS_LAYER)
			&& (oplus_ofp_get_hbm_state() == true) && (p_oplus_ofp_params->pressed_icon_status == OPLUS_OFP_PRESSED_ICON_ON)) {
			/* pressed icon has been displayed in panel but hbm may not take effect */
			p_oplus_ofp_params->notifier_chain_value = 1;
		} else if (p_oplus_ofp_params->fp_press == false && (!(p_oplus_ofp_params->hbm_enable & OPLUS_OFP_PROPERTY_FINGERPRESS_LAYER))) {
			/*  finger is not pressed down */
			p_oplus_ofp_params->notifier_chain_value = 0;
		}
	} else {
		if ((p_oplus_ofp_params->fp_press == true) && (p_oplus_ofp_params->hbm_enable & OPLUS_OFP_PROPERTY_FINGERPRESS_LAYER)
			&& (oplus_ofp_get_hbm_state() == true) && (p_oplus_ofp_params->pressed_icon_status == OPLUS_OFP_PRESSED_ICON_ON)) {
			/* pressed icon has been displayed in panel and hbm is always in effect */
			p_oplus_ofp_params->notifier_chain_value = 1;
		} else if (p_oplus_ofp_params->fp_press == false && (!(p_oplus_ofp_params->hbm_enable & OPLUS_OFP_PROPERTY_FINGERPRESS_LAYER))
					&& p_oplus_ofp_params->pressed_icon_status == OPLUS_OFP_PRESSED_ICON_OFF) {
			/* hbm is on but pressed icon has not been displayed */
			p_oplus_ofp_params->notifier_chain_value = 0;
		}
	}
	last_hbm_state = oplus_ofp_get_hbm_state();

	if (last_notifier_chain_value != p_oplus_ofp_params->notifier_chain_value) {
		OPLUS_OFP_TRACE_BEGIN("oplus_ofp_notify_uiready");
		if (p_oplus_ofp_params->aod_unlocking == true && p_oplus_ofp_params->notifier_chain_value == 1) {
			/* check whether hbm is taking effect or not */
			if (mtk_crtc->panel_ext && mtk_crtc->panel_ext->params) {
				refresh_rate = mtk_crtc->panel_ext->params->dyn_fps.vact_timing_fps;
				delay_ms = mtk_crtc->panel_ext->params->oplus_ofp_hbm_on_delay + 1000/refresh_rate;
			} else {
				delay_ms = 17;
			}

			delta_time = ktime_sub(ktime_get(), hbm_cmd_timestamp);
			if (ktime_sub(ms_to_ktime(delay_ms), delta_time) > 0) {
				/* hbm is not taking effect, set a timer to wait and then send uiready */
				hrtimer_start(&p_oplus_ofp_params->timer, ktime_sub(ms_to_ktime(delay_ms), delta_time), HRTIMER_MODE_REL);
				OFP_INFO("delay %lld to notify\n", ktime_to_us(ktime_sub(ms_to_ktime(delay_ms), delta_time)));
			} else {
				OFP_INFO("queue uiready event work\n");
				queue_work(p_oplus_ofp_params->uiready_event_wq, &p_oplus_ofp_params->uiready_event_work);
			}
		} else {
			OFP_INFO("queue uiready event work\n");
			queue_work(p_oplus_ofp_params->uiready_event_wq, &p_oplus_ofp_params->uiready_event_work);
		}
		last_notifier_chain_value = p_oplus_ofp_params->notifier_chain_value;
		OPLUS_OFP_TRACE_END("oplus_ofp_notify_uiready");
	}

	return 0;
}

/*
 due to the fact that the brightness of lhbm pressed icon may change with the backlight,
 it is necessary to readjust the lhbm pressed icon grayscale to meet the requirements of fingerprint unlocking
*/
int oplus_ofp_lhbm_pressed_icon_grayscale_update(void *para_list, unsigned int bl_level)
{
	bool pwm_is_changing = false;
	bool need_to_update_grayscale = false;
	static bool last_pwm_state = false;
	unsigned char *tx_buf = para_list;
	unsigned char tx_buf_0[7] = {0xC2, 0xFE, 0xFE, 0xDD, 0x61, 0x00, 0x62};
	unsigned char tx_buf_1[7] = {0xC2, 0xF9, 0xF9, 0xDA, 0x61, 0x00, 0x62};
	unsigned char tx_buf_2[7] = {0xC2, 0xF5, 0xF5, 0xD6, 0x61, 0x00, 0x62};
	unsigned char tx_buf_3[7] = {0xC2, 0xEF, 0xEF, 0xDA, 0x61, 0x00, 0x62};
	unsigned char tx_buf_4[7] = {0xC2, 0xE9, 0xE9, 0xD4, 0x61, 0x00, 0x62};
	unsigned char tx_buf_5[7] = {0xC2, 0xFA, 0xFA, 0xD9, 0x61, 0x00, 0x62};
	unsigned char tx_buf_6[7] = {0xC2, 0xF4, 0xF4, 0xD2, 0x61, 0x00, 0x62};
	unsigned char tx_buf_7[7] = {0xC2, 0xF0, 0xF0, 0xD6, 0x61, 0x00, 0x62};
	unsigned char tx_buf_8[7] = {0xC2, 0xEA, 0xEA, 0xD2, 0x61, 0x00, 0x62};
	unsigned char tx_buf_9[7] = {0xC2, 0xE5, 0xE5, 0xCE, 0x61, 0x00, 0x62};
	int rc = 0;
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	OFP_DEBUG("start\n");

	if (!oplus_ofp_local_hbm_is_enabled()) {
		OFP_DEBUG("local hbm is not enabled, no need to update lhbm pressed icon grayscale\n");
		return 0;
	}

	if (!tx_buf || !p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	if (!p_oplus_ofp_params->need_to_update_lhbm_pressed_icon_gamma) {
		OFP_DEBUG("need_to_update_lhbm_pressed_icon_gamma is not config, no need to update lhbm pressed icon grayscale\n");
		return 0;
	}

	pwm_is_changing = (last_pwm_state != oplus_panel_pwm_onepulse_is_enabled());

	if (!oplus_panel_pwm_onepulse_is_enabled()) {
		if (((last_backlight == 0x0000) || (last_backlight > 0x0700) || pwm_is_changing)
				&& ((bl_level > 0x0000) && (bl_level <= 0x0700))) {
			memcpy(tx_buf, tx_buf_0, 7);
			need_to_update_grayscale = true;
		} else if (((last_backlight <= 0x0700) || (last_backlight > 0x08F0) || pwm_is_changing)
						&& ((bl_level > 0x0700) && (bl_level <= 0x08F0))) {
			memcpy(tx_buf, tx_buf_1, 7);
			need_to_update_grayscale = true;
		} else if (((last_backlight <= 0x08F0) || (last_backlight > 0x0A88) || pwm_is_changing)
						&& ((bl_level > 0x08F0) && (bl_level <= 0x0A88))) {
			memcpy(tx_buf, tx_buf_2, 7);
			need_to_update_grayscale = true;
		} else if (((last_backlight <= 0x0A88) || (last_backlight > 0x0C00) || pwm_is_changing)
						&& ((bl_level > 0x0A88) && (bl_level <= 0x0C00))) {
			memcpy(tx_buf, tx_buf_3, 7);
			need_to_update_grayscale = true;
		} else if (((last_backlight <= 0x0C00) || (last_backlight > 0x0DBB) || pwm_is_changing)
						&& ((bl_level > 0x0C00) && (bl_level <= 0x0DBB))) {
			memcpy(tx_buf, tx_buf_4, 7);
			need_to_update_grayscale = true;
		}
	} else {
		if (((last_backlight == 0x0000) || (last_backlight > 0x0700) || pwm_is_changing)
				&& ((bl_level > 0x0000) && (bl_level <= 0x0700))) {
			memcpy(tx_buf, tx_buf_5, 7);
			need_to_update_grayscale = true;
		} else if (((last_backlight <= 0x0700) || (last_backlight > 0x08F0) || pwm_is_changing)
						&& ((bl_level > 0x0700) && (bl_level <= 0x08F0))) {
			memcpy(tx_buf, tx_buf_6, 7);
			need_to_update_grayscale = true;
		} else if (((last_backlight <= 0x08F0) || (last_backlight > 0x0A88) || pwm_is_changing)
						&& ((bl_level > 0x08F0) && (bl_level <= 0x0A88))) {
			memcpy(tx_buf, tx_buf_7, 7);
			need_to_update_grayscale = true;
		} else if (((last_backlight <= 0x0A88) || (last_backlight > 0x0C00) || pwm_is_changing)
						&& ((bl_level > 0x0A88) && (bl_level <= 0x0C00))) {
			memcpy(tx_buf, tx_buf_8, 7);
			need_to_update_grayscale = true;
		} else if (((last_backlight <= 0x0C00) || (last_backlight > 0x0DBB) || pwm_is_changing)
						&& ((bl_level > 0x0C00) && (bl_level <= 0x0DBB))) {
			memcpy(tx_buf, tx_buf_9, 7);
			need_to_update_grayscale = true;
		}
	}

	if (need_to_update_grayscale) {
		OFP_INFO("lhbm pressed icon grayscale:0x%02X, 0x%02X, 0x%02X\n", tx_buf[1],  tx_buf[2],  tx_buf[3]);
		rc = 1;
	}

	last_pwm_state = oplus_panel_pwm_onepulse_is_enabled();

	OFP_DEBUG("end\n");

	return rc;
}
EXPORT_SYMBOL(oplus_ofp_lhbm_pressed_icon_grayscale_update);

/* need filter backlight in hbm state and aod unlocking process */
bool oplus_ofp_backlight_filter(void *drm_crtc, void *cmdq_pkt, unsigned int bl_level)
{
	uint64_t hbm_enable = 0;
	bool need_filter_backlight = false;
	struct drm_crtc *crtc = drm_crtc;
	struct cmdq_pkt *cmdq_handle = cmdq_pkt;
	struct mtk_panel_params *panel_ext = NULL;
	struct mtk_crtc_state *crtc_state = NULL;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	struct mtk_ddp_comp *comp = NULL;
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	OFP_DEBUG("start\n");

	if (!crtc || !p_oplus_ofp_params) {
		OFP_ERR("Invalid crtc param\n");
		return false;
	}

	panel_ext = mtk_drm_get_lcm_ext_params(crtc);
	if (!panel_ext) {
		OFP_ERR("Invalid panel_ext params\n");
		return -EINVAL;
	}

	crtc_state = to_mtk_crtc_state(crtc->state);
	mtk_crtc = to_mtk_crtc(crtc);
	if (!mtk_crtc || !crtc_state) {
		OFP_ERR("Invalid mtk_crtc or crtc_state param\n");
		return false;
	}

	comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (!(comp && comp->funcs && comp->funcs->io_cmd)) {
		OFP_ERR("Invalid comp params\n");
		return false;
	}

	if (!mtk_crtc->enabled) {
		OFP_ERR("mtk crtc is not enabled\n");
		return false;
	}

	OPLUS_OFP_TRACE_BEGIN("oplus_ofp_backlight_filter");

	hbm_enable = crtc_state->prop_val[CRTC_PROP_HBM_ENABLE];
	if (oplus_ofp_get_hbm_state()) {
		if (bl_level == 0) {
			if (oplus_ofp_local_hbm_is_enabled()) {
				/* send lhbm pressed icon cmd */
				OPLUS_OFP_TRACE_BEGIN("OPLUS_OFP_SET_LHBM_PRESSED_ICON");
				comp->funcs->io_cmd(comp, cmdq_handle, OPLUS_OFP_SET_LHBM_PRESSED_ICON, &hbm_enable);
				OFP_INFO("OPLUS_OFP_SET_LHBM_PRESSED_ICON %d\n", hbm_enable);
				OPLUS_OFP_TRACE_END("OPLUS_OFP_SET_LHBM_PRESSED_ICON");
			}
			oplus_ofp_set_hbm_state(false);
			OFP_DEBUG("backlight is 0, set hbm state to false\n");
			if (p_oplus_ofp_params->aod_unlocking == true) {
				p_oplus_ofp_params->aod_unlocking = false;
				OFP_INFO("oplus_ofp_aod_unlocking: %d\n", p_oplus_ofp_params->aod_unlocking);
				OPLUS_OFP_TRACE_INT("oplus_ofp_aod_unlocking|%d", p_oplus_ofp_params->aod_unlocking);
			}
			need_filter_backlight = false;
		} else {
			if (!oplus_ofp_local_hbm_is_enabled() || p_oplus_ofp_params->hbm_mode) {
				OFP_INFO("hbm state is true, filter backlight %u setting\n", bl_level);
				need_filter_backlight = true;
			} else if (p_oplus_ofp_params->need_to_update_lhbm_pressed_icon_gamma && (bl_level > OPLUS_OFP_900NIT_DBV_LEVEL)) {
				OFP_INFO("hbm state is true and backlight lvl is greater than OPLUS_OFP_900NIT_DBV_LEVEL, filter backlight %u setting\n", bl_level);
				need_filter_backlight = true;
			}
		}
	} else if (p_oplus_ofp_params->aod_unlocking && p_oplus_ofp_params->fp_press && bl_level) {
		OFP_INFO("aod unlocking is true, filter backlight setting\n");
		need_filter_backlight = true;
	} else if (!p_oplus_ofp_params->aod_unlocking && !p_oplus_ofp_params->doze_active
				&& (hbm_enable & OPLUS_OFP_PROPERTY_DIM_LAYER) && bl_level
					&& panel_ext->oplus_ofp_need_keep_apart_backlight) {
		/* backlight will affect hbm on time in some panel, need to separate the 51 cmd for stable hbm on time */
		OFP_INFO("dim layer exist, filter backlight %u setting in advance\n", bl_level);
		need_filter_backlight = true;
	} else if (oplus_ofp_get_aod_state()) {
		OFP_INFO("aod state is true, filter backlight %u setting\n", bl_level);
		need_filter_backlight = true;
	} else if (!oplus_ofp_get_aod_state() && (hbm_enable & OPLUS_OFP_PROPERTY_AOD_LAYER) && bl_level
				&& !((p_oplus_ofp_params->longrui_aod_config & OPLUS_OFP_A_MIRROR_TO_THE_END_AOD_CONFIG)
					&& (p_oplus_ofp_params->longrui_aod_mode & OPLUS_OFP_A_MIRROR_TO_THE_END_AOD_MODE))) {
		OFP_INFO("aod layer exist, filter backlight %u setting\n", bl_level);
		need_filter_backlight = true;
	} else if (p_oplus_ofp_params->dimlayer_hbm || hbm_enable) {
		OFP_INFO("backlight lvl:%u\n", bl_level);
	}

	OPLUS_OFP_TRACE_END("oplus_ofp_backlight_filter");

	OFP_DEBUG("end\n");

	return need_filter_backlight;
}
EXPORT_SYMBOL(oplus_ofp_backlight_filter);

/* -------------------- aod -------------------- */
/* aod off status handle */
int oplus_ofp_aod_off_status_handle(void *mtk_drm_crtc)
{
	struct mtk_drm_crtc *mtk_crtc = mtk_drm_crtc;
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	if (!mtk_crtc || !p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	OPLUS_OFP_TRACE_BEGIN("oplus_ofp_aod_off_status_handle");
	/* doze disable status handle */
	OFP_INFO("aod off status handle\n");

	if (oplus_ofp_is_supported()) {
		/* update aod unlocking value */
		oplus_ofp_aod_unlocking_update();
	}

	oplus_ofp_set_aod_state(false);

	/* aod off cmd is sent to ddic */
	p_oplus_ofp_params->aod_off_cmd_timestamp = ktime_get();
	OFP_DEBUG("aod_off_cmd_timestamp:%lld\n", ktime_to_ms(p_oplus_ofp_params->aod_off_cmd_timestamp));

	OPLUS_OFP_TRACE_END("oplus_ofp_aod_off_status_handle");

	return 0;
}

/* aod status handle */
int oplus_ofp_doze_status_handle(bool doze_enable, void *drm_crtc, void *mtk_panel_ext, void *drm_panel, void *mtk_dsi, void *dcs_write_gce_pack)
{
	struct drm_crtc *crtc = drm_crtc;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	if (!crtc || !mtk_crtc || !p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	OPLUS_OFP_TRACE_BEGIN("oplus_ofp_doze_status_handle");

	if (doze_enable) {
		OFP_INFO("doze status handle before doze enable\n");
		/* before doze enable */
		if (oplus_ofp_is_supported()) {
			/* hbm mode -> normal mode -> aod mode */
			if (oplus_ofp_get_hbm_state() == true) {
				if (mtk_crtc_is_frame_trigger_mode(crtc) || oplus_ofp_video_mode_aod_fod_is_enabled()) {
					struct mtk_panel_ext *ext = mtk_panel_ext;

					OPLUS_OFP_TRACE_BEGIN("oplus_ofp_hbm_off_before_doze_enable");
					if (oplus_ofp_local_hbm_is_enabled()) {
						if (ext && ext->funcs && ext->funcs->oplus_ofp_set_lhbm_pressed_icon) {
							OFP_INFO("lhbm pressed icon off before doze enable\n");
							ext->funcs->oplus_ofp_set_lhbm_pressed_icon(drm_panel, mtk_dsi, dcs_write_gce_pack, NULL, false);
						}
					} else {
						if (ext && ext->funcs && ext->funcs->oplus_hbm_set_cmdq) {
							OFP_INFO("hbm off before doze enable\n");
							ext->funcs->oplus_hbm_set_cmdq(drm_panel, mtk_dsi, dcs_write_gce_pack, NULL, false);
						}
					}
					oplus_ofp_set_hbm_state(false);
					OPLUS_OFP_TRACE_END("oplus_ofp_hbm_off_before_doze_enable");
				}
			}

			/* reset aod unlocking flag when fingerprint unlocking failed */
			if (p_oplus_ofp_params->aod_unlocking == true) {
				p_oplus_ofp_params->aod_unlocking = false;
				OFP_INFO("oplus_ofp_aod_unlocking: %d\n", p_oplus_ofp_params->aod_unlocking);
				OPLUS_OFP_TRACE_INT("oplus_ofp_aod_unlocking|%d", p_oplus_ofp_params->aod_unlocking);
			}
		}

		oplus_ofp_set_aod_state(true);
		oplus_ofp_need_to_skip_esd_check_after_aod_off();
	} else {
		oplus_ofp_aod_off_status_handle(mtk_crtc);
	}

	OPLUS_OFP_TRACE_END("oplus_ofp_doze_status_handle");

	return 0;
}

int oplus_ofp_set_aod_light_mode_after_doze_enable(void *mtk_panel_ext, void *mtk_dsi, void *dcs_write_gce)
{
	struct mtk_panel_ext *ext = mtk_panel_ext;
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	OFP_DEBUG("start\n");

	if (!ext || !mtk_dsi || !dcs_write_gce || !p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	if (p_oplus_ofp_params->aod_light_mode) {
		if (ext && ext->funcs && ext->funcs->oplus_set_aod_light_mode) {
			ext->funcs->oplus_set_aod_light_mode(mtk_dsi, dcs_write_gce, NULL, p_oplus_ofp_params->aod_light_mode);
			OFP_INFO("set_aod_light_mode:%u\n", p_oplus_ofp_params->aod_light_mode);
		}
		if (ext && ext->funcs && ext->funcs->set_aod_light_mode) {
			ext->funcs->set_aod_light_mode(mtk_dsi, dcs_write_gce, NULL, p_oplus_ofp_params->aod_light_mode);
			OFP_INFO("set_aod_light_mode:%u\n", p_oplus_ofp_params->aod_light_mode);
		}
	}

	OFP_DEBUG("end\n");

	return 0;
}

/* aod off cmd cmdq set */
int oplus_ofp_aod_off_set_cmdq(struct drm_crtc *crtc)
{
	bool is_frame_mode;
	int i, j;
	bool doze_en = false;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	unsigned int crtc_id = drm_crtc_index(&mtk_crtc->base);
	struct mtk_ddp_comp *output_comp, *comp;
	struct cmdq_pkt *cmdq_handle;
	struct cmdq_client *client;
	struct mtk_crtc_state *crtc_state;

	if (!crtc || !mtk_crtc) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	OPLUS_OFP_TRACE_BEGIN("oplus_ofp_aod_off_set_cmdq");

	crtc_state = to_mtk_crtc_state(crtc->state);
	if (!crtc_state->prop_val[CRTC_PROP_DOZE_ACTIVE]) {
		OFP_INFO("not in doze mode\n");
	}

	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (unlikely(!output_comp)) {
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		OPLUS_OFP_TRACE_END("oplus_ofp_aod_off_set_cmdq");
		return -ENODEV;
	}

	client = mtk_crtc->gce_obj.client[CLIENT_CFG];
	if (!mtk_crtc->enabled) {
		mtk_drm_crtc_wk_lock(crtc, 1, __func__, __LINE__);
		/* power on mtcmos */
		OPLUS_OFP_TRACE_BEGIN("power_on_mtcmos");
		mtk_drm_top_clk_prepare_enable(crtc);
		mtk_crtc_gce_event_config(crtc);
		mtk_crtc_vdisp_ao_config(crtc);

		/*APSRC control*/
		mtk_crtc_v_idle_apsrc_control(crtc, NULL, false, false, crtc_id, true);

		cmdq_mbox_enable(client->chan);
		if (mtk_crtc_with_event_loop(crtc) &&
				(mtk_crtc_is_frame_trigger_mode(crtc)))
			mtk_crtc_start_event_loop(crtc);
		if (mtk_crtc_with_trigger_loop(crtc))
			mtk_crtc_start_trig_loop(crtc);

		mtk_ddp_comp_io_cmd(output_comp, NULL, CONNECTOR_ENABLE, NULL);

		for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j)
			mtk_dump_analysis(comp);

		OFP_INFO("power on mtcmos\n");
		OPLUS_OFP_TRACE_END("power_on_mtcmos");
	}

	mtk_drm_idlemgr_kick(__func__, &mtk_crtc->base, 0);
	/* send LCM CMD */
	OPLUS_OFP_TRACE_BEGIN("prepare_to_send_aod_off_cmd");
	OFP_INFO("prepare to send aod off cmd\n");
	is_frame_mode = mtk_crtc_is_frame_trigger_mode(&mtk_crtc->base);

	if (is_frame_mode || mtk_crtc->gce_obj.client[CLIENT_DSI_CFG] == NULL)
		cmdq_handle =
			cmdq_pkt_create(mtk_crtc->gce_obj.client[CLIENT_CFG]);
	else
		cmdq_handle =
			cmdq_pkt_create(
			mtk_crtc->gce_obj.client[CLIENT_DSI_CFG]);

	if (!cmdq_handle) {
		DDPPR_ERR("%s:%d NULL cmdq handle\n", __func__, __LINE__);
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		return -EINVAL;
	}

	if (is_frame_mode) {
		cmdq_pkt_clear_event(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_STREAM_BLOCK]);
		cmdq_pkt_wfe(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_CABC_EOF]);
	}

	if (mtk_crtc_with_sub_path(crtc, mtk_crtc->ddp_mode))
		mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle,
			DDP_SECOND_PATH, 0);
	else
		mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle,
			DDP_FIRST_PATH, 0);

	/* Record Vblank start timestamp */
	mtk_vblank_config_rec_start(mtk_crtc, cmdq_handle, SET_BL);
	OPLUS_OFP_TRACE_END("prepare_to_send_aod_off_cmd");

	OPLUS_OFP_TRACE_BEGIN("DSI_SET_DOZE");
	oplus_ofp_set_aod_state(false);
	if (output_comp->funcs && output_comp->funcs->io_cmd)
		output_comp->funcs->io_cmd(output_comp,
			cmdq_handle, DSI_SET_DOZE, &doze_en);
	OFP_INFO("DSI_SET_DOZE %d\n", doze_en);
	OPLUS_OFP_TRACE_END("DSI_SET_DOZE");

	OPLUS_OFP_TRACE_BEGIN("flush_aod_off_cmd");
	if (is_frame_mode) {
		cmdq_pkt_set_event(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_CABC_EOF]);
		cmdq_pkt_set_event(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_STREAM_BLOCK]);
	}

	/* Record Vblank end timestamp and calculate duration */
	mtk_vblank_config_rec_end_cal(mtk_crtc, cmdq_handle, SET_BL);

	cmdq_pkt_flush(cmdq_handle);
	cmdq_pkt_destroy(cmdq_handle);
	OFP_INFO("flush aod off cmd\n");
	OPLUS_OFP_TRACE_END("flush_aod_off_cmd");

	oplus_ofp_aod_off_status_handle(mtk_crtc);

	if (!mtk_crtc->enabled) {
		OPLUS_OFP_TRACE_BEGIN("power_off_mtcmos");
		if (mtk_crtc_with_trigger_loop(crtc))
			mtk_crtc_stop_trig_loop(crtc);

		if (mtk_crtc_with_event_loop(crtc) &&
				(mtk_crtc_is_frame_trigger_mode(crtc)))
			mtk_crtc_stop_event_loop(crtc);
		mtk_ddp_comp_io_cmd(output_comp, NULL, CONNECTOR_DISABLE, NULL);

		cmdq_mbox_disable(client->chan);
		DDPFENCE("%s:%d power_state = false\n", __func__, __LINE__);
		mtk_drm_top_clk_disable_unprepare(crtc->dev);
		mtk_drm_crtc_wk_lock(crtc, 0, __func__, __LINE__);
		OFP_INFO("power off mtcmos\n");
		OPLUS_OFP_TRACE_END("power_off_mtcmos");
	}

	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

	OPLUS_OFP_TRACE_END("oplus_ofp_aod_off_set_cmdq");

	return 0;
}

int oplus_ofp_crtc_aod_off_set(void)
{
	int ret = 0;
	struct drm_crtc *crtc;
	struct drm_device *drm_dev = get_drm_device();

	OFP_DEBUG("start\n");

	if (IS_ERR_OR_NULL(drm_dev)) {
		OFP_ERR("invalid drm dev\n");
		return -EINVAL;
	}

	/* this debug cmd only for crtc0 */
	crtc = list_first_entry(&(drm_dev)->mode_config.crtc_list,
				typeof(*crtc), head);
	if (IS_ERR_OR_NULL(crtc)) {
		OFP_ERR("find crtc fail\n");
		return -EINVAL;
	}

	ret = oplus_ofp_aod_off_set_cmdq(crtc);

	OFP_DEBUG("end\n");

	return ret;
}


void oplus_ofp_aod_off_set_work_handler(struct work_struct *work_item)
{
	int ret = 0;

	OPLUS_OFP_TRACE_BEGIN("oplus_ofp_aod_off_set_work_handler");
	OFP_INFO("send aod off cmd to speed up aod unlocking\n");
	ret = oplus_ofp_crtc_aod_off_set();
	if (ret) {
		OFP_ERR("failed to send aod off cmd\n");
	}
	OPLUS_OFP_TRACE_END("oplus_ofp_aod_off_set_work_handler");

	return;
}

int oplus_ofp_aod_off_set(void)
{
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	if (!p_oplus_ofp_params || oplus_ofp_video_mode_aod_fod_is_enabled()) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	OFP_DEBUG("start\n");

	if (oplus_ofp_get_hbm_state()) {
		OFP_DEBUG("ignore aod off setting in hbm state\n");
		return 0;
	}

	OPLUS_OFP_TRACE_BEGIN("oplus_ofp_aod_off_set");
	if (oplus_ofp_get_aod_state() && p_oplus_ofp_params->doze_active != 0) {
		OFP_INFO("queue aod off set work\n");
		queue_work(p_oplus_ofp_params->aod_off_set_wq, &p_oplus_ofp_params->aod_off_set_work);
		oplus_ofp_set_aod_state(false);
	}
	OPLUS_OFP_TRACE_END("oplus_ofp_aod_off_set");

	OFP_DEBUG("end\n");

	return 0;
}

/*
 touchpanel notify touchdown event when fingerprint is pressed,
 then display driver send aod off cmd immediately and vsync change to 120hz,
 so that press icon layer can sent down faster
*/
int oplus_ofp_touchpanel_event_notifier_call(struct notifier_block *nb, unsigned long action, void *data)
{
	struct touchpanel_event *tp_event = (struct touchpanel_event *)data;

	if (!oplus_ofp_is_supported() || oplus_ofp_video_mode_aod_fod_is_enabled()) {
		OFP_DEBUG("no need to send aod off cmd in doze mode to speed up fingerprint unlocking\n");
		return NOTIFY_OK;
	}

	OPLUS_OFP_TRACE_BEGIN("oplus_ofp_touchpanel_event_notifier_call");
	if (tp_event) {
		if (action == EVENT_ACTION_FOR_FINGPRINT) {
			OFP_DEBUG("EVENT_ACTION_FOR_FINGPRINT\n");

			if (tp_event->touch_state == 1) {
				OFP_INFO("tp touchdown\n");
				/* send aod off cmd in doze mode to speed up fingerprint unlocking */
				oplus_ofp_aod_off_set();
			}
		}
	}
	OPLUS_OFP_TRACE_END("oplus_ofp_touchpanel_event_notifier_call");

	return NOTIFY_OK;
}

/*
 as there have some black frames are inserted in aod off cmd flow which will affect hbm on cmd execution time,
 so check how many black frames have taken effect first,
 then calculate delay time to keep apart aod off cmd and hbm on cmd to make sure ui ready is accurate
*/
int oplus_ofp_aod_off_hbm_on_delay_check(void *mtk_drm_crtc)
{
	static bool last_aod_unlocking = false;
	static unsigned int te_rdy_irq_count = 0;
	unsigned int aod_off_insert_black = 0;
	struct mtk_drm_crtc *mtk_crtc = mtk_drm_crtc;
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	OFP_DEBUG("start\n");

	if (!p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	if (!oplus_ofp_is_supported() || oplus_ofp_video_mode_aod_fod_is_enabled()) {
		OFP_DEBUG("no need to check aod off hbm on delay\n");
		return 0;
	}

	OPLUS_OFP_TRACE_BEGIN("oplus_ofp_aod_off_hbm_on_delay_check");

	if (mtk_crtc->panel_ext && mtk_crtc->panel_ext->params
			&& mtk_crtc->panel_ext->params->oplus_ofp_aod_off_insert_black > 0) {
		if (p_oplus_ofp_params->aod_unlocking == true) {
			if (last_aod_unlocking == false) {
				te_rdy_irq_count = 1;
			} else if (te_rdy_irq_count != 0 && te_rdy_irq_count < 10) {
				te_rdy_irq_count++;
			} else {
				/* 10 irq is enough */
				te_rdy_irq_count = 10;
			}
		} else {
			te_rdy_irq_count = 0;
			p_oplus_ofp_params->aod_off_hbm_on_delay = 0;
		}

		aod_off_insert_black = mtk_crtc->panel_ext->params->oplus_ofp_aod_off_insert_black;
		if (te_rdy_irq_count < aod_off_insert_black) {
			p_oplus_ofp_params->aod_off_hbm_on_delay = aod_off_insert_black - te_rdy_irq_count;
			OFP_DEBUG("aod_off_insert_black=%d,te_rdy_irq_count=%d,aod_off_hbm_on_delay=%d\n",
				aod_off_insert_black, te_rdy_irq_count, p_oplus_ofp_params->aod_off_hbm_on_delay);
		} else {
			p_oplus_ofp_params->aod_off_hbm_on_delay = 0;
		}

		last_aod_unlocking = p_oplus_ofp_params->aod_unlocking;
	}

	OPLUS_OFP_TRACE_END("oplus_ofp_aod_off_hbm_on_delay_check");

	OFP_DEBUG("end\n");

	return 0;
}

int oplus_ofp_aod_off_backlight_recovery(void *drm_crtc, void *mtk_crtc_state, void *cmdq_pkt)
{
	int rc = 0;
	uint64_t hbm_enable = 0;
	static bool last_aod_layer_status = false;
	bool new_aod_layer_status = false;
	struct drm_crtc *crtc = drm_crtc;
	struct mtk_crtc_state *state = mtk_crtc_state;
	struct cmdq_pkt *cmdq_handle = cmdq_pkt;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	struct mtk_ddp_comp *comp = NULL;
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	mtk_crtc = to_mtk_crtc(crtc);
	if (!mtk_crtc) {
		OFP_ERR("Invalid mtk_crtc param\n");
		return false;
	}

	if (!p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return false;
	}

	comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (!(comp && comp->funcs && comp->funcs->io_cmd)) {
		OFP_ERR("Invalid comp params\n");
		return false;
	}
	hbm_enable = state->prop_val[CRTC_PROP_HBM_ENABLE];
	new_aod_layer_status = hbm_enable & OPLUS_OFP_PROPERTY_AOD_LAYER;
	OFP_DEBUG("Aod layer property %llu\n", hbm_enable);
	if (last_aod_layer_status && !new_aod_layer_status) {
		comp->funcs->io_cmd(comp, cmdq_handle, DSI_SET_BL, &oplus_display_brightness);
		OFP_INFO("recovery backlight level = %d after aod layer disappear\n", oplus_display_brightness);
	}
	last_aod_layer_status = new_aod_layer_status;
	return rc;
}


/* -------------------- node -------------------- */
/* fp_type */
int oplus_ofp_set_fp_type(void *buf)
{
	unsigned int *fp_type = buf;
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	OFP_DEBUG("start\n");

	if (!buf || !p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	OPLUS_OFP_TRACE_BEGIN("oplus_ofp_set_fp_type");

	p_oplus_ofp_params->fp_type = *fp_type;
	OFP_INFO("fp_type:0x%x\n", p_oplus_ofp_params->fp_type);
	OPLUS_OFP_TRACE_INT("oplus_ofp_fp_type|%d", p_oplus_ofp_params->fp_type);

	OPLUS_OFP_TRACE_END("oplus_ofp_set_fp_type");

	OFP_DEBUG("end\n");

	return 0;
}
EXPORT_SYMBOL(oplus_ofp_set_fp_type);

int oplus_ofp_get_fp_type(void *buf)
{
	unsigned int *fp_type = buf;
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	OFP_DEBUG("start\n");

	if (!buf || !p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	OPLUS_OFP_TRACE_BEGIN("oplus_ofp_get_fp_type");

	*fp_type = p_oplus_ofp_params->fp_type;
	OFP_DEBUG("fp_type:0x%x\n", *fp_type);

	OPLUS_OFP_TRACE_END("oplus_ofp_get_fp_type");

	OFP_DEBUG("end\n");

	return 0;
}

ssize_t oplus_ofp_set_fp_type_attr(struct kobject *obj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int fp_type = 0;
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	OFP_DEBUG("start\n");

	if (!buf || !p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return count;
	}

	if (kstrtouint(buf, 10, &fp_type)) {
		OFP_ERR("kstrtouint error!\n");
		return count;
	}

	OPLUS_OFP_TRACE_BEGIN("oplus_ofp_set_fp_type_attr");

	p_oplus_ofp_params->fp_type = fp_type;
	OFP_INFO("fp_type:0x%x\n", p_oplus_ofp_params->fp_type);
	OPLUS_OFP_TRACE_INT("oplus_ofp_fp_type|%d", p_oplus_ofp_params->fp_type);

	OPLUS_OFP_TRACE_END("oplus_ofp_set_fp_type_attr");

	OFP_DEBUG("end\n");

	return count;
}

ssize_t oplus_ofp_get_fp_type_attr(struct kobject *obj,
	struct kobj_attribute *attr, char *buf)
{
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	OFP_DEBUG("start\n");

	if (!buf || !p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	OPLUS_OFP_TRACE_BEGIN("oplus_ofp_get_fp_type_attr");

	OFP_INFO("fp_type:0x%x\n", p_oplus_ofp_params->fp_type);

	OPLUS_OFP_TRACE_END("oplus_ofp_get_fp_type_attr");

	OFP_DEBUG("end\n");

	return sysfs_emit(buf, "%d\n", p_oplus_ofp_params->fp_type);
}

/* fod part */
/* dimlayer_hbm */
int oplus_ofp_set_dimlayer_hbm(void *buf)
{
	unsigned int *dimlayer_hbm = buf;
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	OFP_DEBUG("start\n");

	if (!oplus_ofp_is_supported() || oplus_ofp_oled_capacitive_is_enabled() || oplus_ofp_ultrasonic_is_enabled()) {
		OFP_DEBUG("no need to set dimlayer hbm\n");
		return 0;
	}

	if (!buf || !p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	p_oplus_ofp_params->dimlayer_hbm = *dimlayer_hbm;
	OFP_INFO("dimlayer_hbm:%u\n", p_oplus_ofp_params->dimlayer_hbm);

	OFP_DEBUG("end\n");

	return 0;
}

int oplus_ofp_get_dimlayer_hbm(void *buf)
{
	unsigned int *dimlayer_hbm = buf;
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	OFP_DEBUG("start\n");

	if (!buf || !p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	*dimlayer_hbm = p_oplus_ofp_params->dimlayer_hbm;
	OFP_INFO("dimlayer_hbm:%u\n", *dimlayer_hbm);

	OFP_DEBUG("end\n");

	return 0;
}

ssize_t oplus_ofp_set_dimlayer_hbm_attr(struct kobject *obj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int dimlayer_hbm = 0;
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	OFP_DEBUG("start\n");

	if (!oplus_ofp_is_supported() || oplus_ofp_oled_capacitive_is_enabled() || oplus_ofp_ultrasonic_is_enabled()) {
		OFP_DEBUG("no need to set dimlayer hbm\n");
		return count;
	}

	if (!buf || !p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return count;
	}

	sscanf(buf, "%u", &dimlayer_hbm);

	p_oplus_ofp_params->dimlayer_hbm = dimlayer_hbm;
	OFP_INFO("dimlayer_hbm:%u\n", p_oplus_ofp_params->dimlayer_hbm);

	OFP_DEBUG("end\n");

	return count;
}

ssize_t oplus_ofp_get_dimlayer_hbm_attr(struct kobject *obj,
	struct kobj_attribute *attr, char *buf)
{
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	OFP_DEBUG("start\n");

	if (!buf || !p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	OFP_INFO("dimlayer_hbm:%u\n", p_oplus_ofp_params->dimlayer_hbm);

	OFP_DEBUG("end\n");

	return sysfs_emit(buf, "%u\n", p_oplus_ofp_params->dimlayer_hbm);
}

/* notify fp press for hidl */
int oplus_ofp_notify_fp_press(void *buf)
{
	unsigned int *fp_press = buf;
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	if (!p_oplus_ofp_params || !fp_press) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	if ((*fp_press) == 1) {
		/* finger is pressed down and pressed icon layer is ready */
		p_oplus_ofp_params->fp_press = true;
	} else {
		p_oplus_ofp_params->fp_press = false;
	}
	OFP_INFO("receive uiready %d\n", p_oplus_ofp_params->fp_press);
	OPLUS_OFP_TRACE_INT("oplus_ofp_fp_press|%d", p_oplus_ofp_params->fp_press);


	if (oplus_ofp_is_supported() && p_oplus_ofp_params->fp_press && !oplus_ofp_video_mode_aod_fod_is_enabled()) {
		/* send aod off cmd in doze mode to speed up fingerprint unlocking */
		OFP_DEBUG("fp press is true\n");
		oplus_ofp_aod_off_set();
	}

	return 0;
}

/* notify fp press for sysfs */
ssize_t oplus_ofp_notify_fp_press_attr(struct kobject *obj, struct kobj_attribute *attr,
	const char *buf, size_t count)
{
	unsigned int fp_press = 0;
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	if (!p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return count;
	}

	if (kstrtouint(buf, 0, &fp_press)) {
		OFP_ERR("kstrtouint error!\n");
		return count;
	}

	if (fp_press == 1) {
		/* finger is pressed down and pressed icon layer is ready */
		p_oplus_ofp_params->fp_press = true;
	} else {
		p_oplus_ofp_params->fp_press = false;
	}
	OFP_INFO("receive uiready %d\n", p_oplus_ofp_params->fp_press);
	OPLUS_OFP_TRACE_INT("oplus_ofp_fp_press|%d", p_oplus_ofp_params->fp_press);


	if (oplus_ofp_is_supported() && p_oplus_ofp_params->fp_press && !oplus_ofp_video_mode_aod_fod_is_enabled()) {
		/* send aod off cmd in doze mode to speed up fingerprint unlocking */
		OFP_DEBUG("fp press is true\n");
		oplus_ofp_aod_off_set();
	}

	return count;
}

int oplus_ofp_set_ultra_low_power_aod_after_doze_enable(struct drm_panel *panel, void *mtk_panel_ext,
		void *mtk_dsi, void *dcs_write_gce)
{
	struct mtk_panel_ext *ext = mtk_panel_ext;
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	OFP_DEBUG("start\n");

	if (!panel || !ext || !mtk_dsi || !dcs_write_gce || !p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	if (p_oplus_ofp_params->ultra_low_power_aod_mode) {
		if (ext && ext->funcs && ext->funcs->oplus_set_ultra_low_power_aod) {
			ext->funcs->oplus_set_ultra_low_power_aod(panel, mtk_dsi, dcs_write_gce, NULL, p_oplus_ofp_params->ultra_low_power_aod_mode);
			OFP_INFO("set_ultra_low_power_aod:%u\n", p_oplus_ofp_params->ultra_low_power_aod_mode);
		}

		if (ext && ext->funcs && ext->funcs->set_ultra_low_power_aod) {
			ext->funcs->set_ultra_low_power_aod(panel, mtk_dsi, dcs_write_gce, NULL, p_oplus_ofp_params->ultra_low_power_aod_mode);
			OFP_INFO("set_ultra_low_power_aod:%u\n", p_oplus_ofp_params->ultra_low_power_aod_mode);
		}
	}

	OFP_DEBUG("end\n");

	return 0;
}

int oplus_ofp_drm_set_ultra_low_power_aod(struct drm_crtc *crtc, unsigned int ultra_low_power_aod)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *output_comp, *comp;
	unsigned int crtc_id = drm_crtc_index(&mtk_crtc->base);
	struct cmdq_pkt *cmdq_handle;
	bool is_frame_mode;
	struct cmdq_client *client;
	int i, j;
	struct mtk_crtc_state *crtc_state;

	if (!crtc || !mtk_crtc) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	OPLUS_OFP_TRACE_BEGIN("oplus_ofp_drm_set_ultra_low_power_aod");

	crtc_state = to_mtk_crtc_state(crtc->state);
	if (!crtc_state->prop_val[CRTC_PROP_DOZE_ACTIVE]) {
		OFP_INFO("not in doze mode\n");
		OPLUS_OFP_TRACE_END("oplus_ofp_drm_set_ultra_low_power_aod");
		return 0;
	}

	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (unlikely(!output_comp)) {
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		OPLUS_OFP_TRACE_END("oplus_ofp_drm_set_ultra_low_power_aod");
		return -ENODEV;
	}

	client = mtk_crtc->gce_obj.client[CLIENT_CFG];
	if (!mtk_crtc->enabled) {
		mtk_drm_crtc_wk_lock(crtc, 1, __func__, __LINE__);
		/* power on mtcmos */
		OPLUS_OFP_TRACE_BEGIN("power_on_mtcmos");
		mtk_drm_top_clk_prepare_enable(crtc);
		mtk_crtc_gce_event_config(crtc);
		mtk_crtc_vdisp_ao_config(crtc);

		/*APSRC control*/
		mtk_crtc_v_idle_apsrc_control(crtc, NULL, false, false, crtc_id, true);

		cmdq_mbox_enable(client->chan);
		if (mtk_crtc_with_event_loop(crtc) &&
				(mtk_crtc_is_frame_trigger_mode(crtc)))
			mtk_crtc_start_event_loop(crtc);

		if (mtk_crtc_with_trigger_loop(crtc))
			mtk_crtc_start_trig_loop(crtc);

		mtk_ddp_comp_io_cmd(output_comp, NULL, CONNECTOR_ENABLE, NULL);

		for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j)
			mtk_dump_analysis(comp);

		OFP_INFO("power on mtcmos\n");
		OPLUS_OFP_TRACE_END("power_on_mtcmos");
	}

	/* send LCM CMD */
	OPLUS_OFP_TRACE_BEGIN("prepare_to_send_ultra_low_power_aod_cmd");
	OFP_INFO("prepare to send ultra low power aod cmd\n");
	is_frame_mode = mtk_crtc_is_frame_trigger_mode(&mtk_crtc->base);

	if (is_frame_mode || mtk_crtc->gce_obj.client[CLIENT_DSI_CFG] == NULL)
		cmdq_handle =
			cmdq_pkt_create(mtk_crtc->gce_obj.client[CLIENT_CFG]);
	else
		cmdq_handle =
			cmdq_pkt_create(
			mtk_crtc->gce_obj.client[CLIENT_DSI_CFG]);

	if (!cmdq_handle) {
		DDPPR_ERR("%s:%d NULL cmdq handle\n", __func__, __LINE__);
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		return -EINVAL;
	}

	if (is_frame_mode) {
		cmdq_pkt_clear_event(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_STREAM_BLOCK]);
		cmdq_pkt_wfe(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_CABC_EOF]);
	}

	if (mtk_crtc_with_sub_path(crtc, mtk_crtc->ddp_mode))
		mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle,
			DDP_SECOND_PATH, 0);
	else
		mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle,
			DDP_FIRST_PATH, 0);

	/* Record Vblank start timestamp */
	mtk_vblank_config_rec_start(mtk_crtc, cmdq_handle, SET_BL);
	OPLUS_OFP_TRACE_END("prepare_to_send_ultra_low_power_aod_cmd");

	/* set low power aod */
	OPLUS_OFP_TRACE_BEGIN("DSI_CMD_ULTRA_LOW_POWER_AOD");
	if (output_comp->funcs && output_comp->funcs->io_cmd)
		output_comp->funcs->io_cmd(output_comp,
			cmdq_handle, DSI_CMD_ULTRA_LOW_POWER_AOD, &ultra_low_power_aod);
	OFP_INFO("ultra low power aod %d\n", ultra_low_power_aod);
	OPLUS_OFP_TRACE_END("DSI_CMD_ULTRA_LOW_POWER_AOD");

	OPLUS_OFP_TRACE_BEGIN("flush_ultra_low_power_aod_cmd");
	if (is_frame_mode) {
		cmdq_pkt_set_event(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_CABC_EOF]);
		cmdq_pkt_set_event(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_STREAM_BLOCK]);
	}

	/* Record Vblank end timestamp and calculate duration */
	mtk_vblank_config_rec_end_cal(mtk_crtc, cmdq_handle, SET_BL);

	cmdq_pkt_flush(cmdq_handle);
	cmdq_pkt_destroy(cmdq_handle);
	OFP_INFO("flush ultra low power aod cmd\n");
	OPLUS_OFP_TRACE_END("flush_ultra_low_power_aod_cmd");

	if (!mtk_crtc->enabled) {
		OPLUS_OFP_TRACE_BEGIN("power_off_mtcmos");
		if (mtk_crtc_with_trigger_loop(crtc))
			mtk_crtc_stop_trig_loop(crtc);

		if (mtk_crtc_with_event_loop(crtc) &&
				(mtk_crtc_is_frame_trigger_mode(crtc)))
			mtk_crtc_stop_event_loop(crtc);

		mtk_ddp_comp_io_cmd(output_comp, NULL, CONNECTOR_DISABLE, NULL);

		cmdq_mbox_disable(client->chan);
		DDPFENCE("%s:%d power_state = false\n", __func__, __LINE__);
		mtk_drm_top_clk_disable_unprepare(crtc->dev);
		mtk_drm_crtc_wk_lock(crtc, 0, __func__, __LINE__);
		OFP_INFO("power off mtcmos\n");
		OPLUS_OFP_TRACE_END("power_off_mtcmos");
	}

	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

	OPLUS_OFP_TRACE_END("oplus_ofp_drm_set_ultra_low_power_aod");

	return 0;
}

int oplus_ofp_drm_set_hbm(struct drm_crtc *crtc, unsigned int hbm_mode)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct cmdq_pkt *cmdq_handle;
	struct mtk_ddp_comp *comp = mtk_ddp_comp_request_output(mtk_crtc);

	if (!crtc || !mtk_crtc) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	if (!(mtk_crtc->enabled)) {
		OFP_ERR("should not set hbm if mtk crtc is not enabled\n");
		return -EFAULT;
	}

	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

	OPLUS_OFP_TRACE_BEGIN("mtk_drm_send_lcm_cmd_prepare");
	OFP_INFO("prepare to send hbm cmd\n");
	mtk_drm_send_lcm_cmd_prepare(crtc, &cmdq_handle);
	OPLUS_OFP_TRACE_END("mtk_drm_send_lcm_cmd_prepare");

	/* set hbm */
	 if (comp && comp->funcs && comp->funcs->io_cmd) {
		 OPLUS_OFP_TRACE_BEGIN("LCM_HBM");
		comp->funcs->io_cmd(comp, cmdq_handle, LCM_HBM, &hbm_mode);
		OFP_INFO("LCM_HBM\n");
		OPLUS_OFP_TRACE_END("LCM_HBM");
	}

	OPLUS_OFP_TRACE_BEGIN("mtk_drm_send_lcm_cmd_flush");
	mtk_drm_send_lcm_cmd_flush(crtc, &cmdq_handle, 0);
	OFP_INFO("mtk_drm_send_lcm_cmd_flush end\n");
	oplus_ofp_set_hbm_state(hbm_mode);
	OPLUS_OFP_TRACE_END("mtk_drm_send_lcm_cmd_flush");

	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

	 return 0;
}

int oplus_ofp_get_hbm(void *buf)
{
	unsigned int *hbm_mode = buf;
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	if (!p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	(*hbm_mode) = p_oplus_ofp_params->hbm_mode;
	OFP_INFO("hbm_mode = %d\n", *hbm_mode);

	return 0;
}

int oplus_ofp_set_hbm(void *buf)
{
	unsigned int *hbm_mode = buf;
	struct drm_crtc *crtc;
	struct drm_device *ddev = get_drm_device();
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	if (!ddev || !p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	/* this debug cmd only for crtc0 */
	crtc = list_first_entry(&(ddev)->mode_config.crtc_list,
				typeof(*crtc), head);
	if (!crtc) {
		OFP_ERR("find crtc fail\n");
		return -EINVAL;
	}

	OFP_INFO("%d to %d\n", p_oplus_ofp_params->hbm_mode, *hbm_mode);
	oplus_ofp_drm_set_hbm(crtc, *hbm_mode);
	p_oplus_ofp_params->hbm_mode = (*hbm_mode);
	OPLUS_OFP_TRACE_INT("oplus_ofp_hbm_mode|%d", p_oplus_ofp_params->hbm_mode);

	return 0;
}

ssize_t oplus_ofp_get_hbm_attr(struct kobject *obj,
	struct kobj_attribute *attr, char *buf)
{
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	if (!p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	OFP_INFO("hbm_mode = %d\n", p_oplus_ofp_params->hbm_mode);

	return sysfs_emit(buf, "%d\n", p_oplus_ofp_params->hbm_mode);
}

ssize_t oplus_ofp_set_hbm_attr(struct kobject *obj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int hbm_mode = 0;
	struct drm_crtc *crtc;
	struct drm_device *ddev = get_drm_device();
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	if (!ddev || !p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return count;
	}

	if (kstrtouint(buf, 10, &hbm_mode)) {
		OFP_ERR("kstrtouint error!\n");
		return count;
	}

	/* this debug cmd only for crtc0 */
	crtc = list_first_entry(&(ddev)->mode_config.crtc_list,
				typeof(*crtc), head);
	if (!crtc) {
		OFP_ERR("find crtc fail\n");
		return -count;
	}

	OFP_INFO("%d to %d\n", p_oplus_ofp_params->hbm_mode, hbm_mode);
	oplus_ofp_drm_set_hbm(crtc, hbm_mode);
	p_oplus_ofp_params->hbm_mode = hbm_mode;
	OPLUS_OFP_TRACE_INT("oplus_ofp_hbm_mode|%d", p_oplus_ofp_params->hbm_mode);

	return count;
}

/* aod part */
int oplus_ofp_set_aod_light_mode(void *buf)
{
	int rc = 0;
	unsigned int *aod_light_mode = buf;
	static unsigned int last_aod_light_mode = 0;
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	OFP_DEBUG("start\n");

	if (!buf || !p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	last_aod_light_mode = p_oplus_ofp_params->aod_light_mode;
	p_oplus_ofp_params->aod_light_mode = (*aod_light_mode);
	OFP_INFO("aod_light_mode:%u\n", p_oplus_ofp_params->aod_light_mode);
	OPLUS_OFP_TRACE_INT("oplus_ofp_aod_light_mode|%d", p_oplus_ofp_params->aod_light_mode);

	if (!oplus_ofp_is_supported()) {
		OFP_DEBUG("aod is not supported\n");
		return 0;
	} else if (!oplus_ofp_get_aod_state()) {
		OFP_ERR("not in aod mode, should not set aod_light_mode\n");
		return 0;
	} else if (oplus_ofp_get_hbm_state()) {
		OFP_INFO("ignore aod light mode setting in hbm state\n");
		return 0;
	}

	if (last_aod_light_mode != p_oplus_ofp_params->aod_light_mode) {
		mtkfb_set_aod_backlight_level(p_oplus_ofp_params->aod_light_mode);
	}

	OFP_DEBUG("end\n");

	return rc;
}

int oplus_ofp_get_aod_light_mode(void *buf)
{
	unsigned int *aod_light_mode = buf;
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	OFP_DEBUG("start\n");

	if (!buf || !p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	*aod_light_mode = p_oplus_ofp_params->aod_light_mode;
	OFP_INFO("aod_light_mode:%u\n", *aod_light_mode);

	OFP_DEBUG("end\n");

	return 0;
}

ssize_t oplus_ofp_set_aod_light_mode_attr(struct kobject *obj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int aod_light_mode = 0;
	static unsigned int last_aod_light_mode = 0;
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	OFP_DEBUG("start\n");

	if (!buf || !p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return count;
	}

	sscanf(buf, "%u", &aod_light_mode);

	last_aod_light_mode = p_oplus_ofp_params->aod_light_mode;
	p_oplus_ofp_params->aod_light_mode = aod_light_mode;
	OFP_INFO("aod_light_mode:%u\n", p_oplus_ofp_params->aod_light_mode);
	OPLUS_OFP_TRACE_INT("oplus_ofp_aod_light_mode|%d", p_oplus_ofp_params->aod_light_mode);

	if (!oplus_ofp_is_supported()) {
		OFP_DEBUG("aod is not supported\n");
		return count;
	} else if (!oplus_ofp_get_aod_state()) {
		OFP_ERR("not in aod mode, should not set aod_light_mode\n");
		return count;
	} else if (oplus_ofp_get_hbm_state()) {
		OFP_INFO("ignore aod light mode setting in hbm state\n");
		return count;
	}

	if (last_aod_light_mode != p_oplus_ofp_params->aod_light_mode) {
		mtkfb_set_aod_backlight_level(p_oplus_ofp_params->aod_light_mode);
	}

	OFP_DEBUG("end\n");

	return count;
}

ssize_t oplus_ofp_get_aod_light_mode_attr(struct kobject *obj,
	struct kobj_attribute *attr, char *buf)
{
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	OFP_DEBUG("start\n");

	if (!buf || !p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	OFP_INFO("aod_light_mode:%u\n", p_oplus_ofp_params->aod_light_mode);

	OFP_DEBUG("end\n");

	return sysfs_emit(buf, "%u\n", p_oplus_ofp_params->aod_light_mode);
}

/* ultra_low_power_aod_mode */
int oplus_ofp_set_ultra_low_power_aod_mode(void *buf)
{
	int rc = 0;
	struct drm_crtc *crtc;
	struct drm_device *ddev = get_drm_device();
	unsigned int *ultra_low_power_aod_mode = buf;
	static unsigned int last_ultra_low_power_aod_mode = 0;
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	OFP_DEBUG("start\n");

	if (!buf || !ddev || !p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	crtc = list_first_entry(&(ddev)->mode_config.crtc_list,
				typeof(*crtc), head);
	if (!crtc) {
		OFP_ERR("find crtc fail\n");
		return -EINVAL;
	}

	last_ultra_low_power_aod_mode = p_oplus_ofp_params->ultra_low_power_aod_mode;
	p_oplus_ofp_params->ultra_low_power_aod_mode = (*ultra_low_power_aod_mode);
	OFP_INFO("ultra_low_power_aod_mode:%u\n", p_oplus_ofp_params->ultra_low_power_aod_mode);
	OPLUS_OFP_TRACE_INT("oplus_ofp_set_ultra_low_power_aod_mode|%d", p_oplus_ofp_params->ultra_low_power_aod_mode);

	if (!oplus_ofp_is_supported()) {
		OFP_INFO("aod is not supported\n");
		return 0;
	} else if (!oplus_ofp_get_aod_state()) {
		OFP_INFO("not in aod mode, should not set ultra_low_power_aod_mode\n");
		return 0;
	} else if (oplus_ofp_get_hbm_state()) {
		OFP_INFO("ignore aod light mode setting in hbm state\n");
		return 0;
	}

	if (last_ultra_low_power_aod_mode != p_oplus_ofp_params->ultra_low_power_aod_mode) {
		rc = oplus_ofp_drm_set_ultra_low_power_aod(crtc, p_oplus_ofp_params->ultra_low_power_aod_mode);
		if (rc) {
			OFP_ERR("Failed to send DSI_CMD_ULTRA_LOW_POWER_AOD_ON cmds, rc=%d\n", rc);
		}
	}

	OFP_DEBUG("end\n");

	return rc;
}

int oplus_ofp_get_ultra_low_power_aod_mode(void *buf)
{
	unsigned int *ultra_low_power_aod_mode = buf;
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	OFP_DEBUG("start\n");

	if (!buf || !p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	*ultra_low_power_aod_mode = p_oplus_ofp_params->ultra_low_power_aod_mode;
	OFP_INFO("ultra_low_power_aod_mode:%u\n", *ultra_low_power_aod_mode);

	OFP_DEBUG("end\n");

	return 0;
}

ssize_t oplus_ofp_set_ultra_low_power_aod_mode_attr(struct kobject *obj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int rc = 0;
	struct drm_crtc *crtc;
	struct drm_device *ddev = get_drm_device();
	unsigned int ultra_low_power_aod_mode = 0;
	static unsigned int last_ultra_low_power_aod_mode = 0;
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	OFP_DEBUG("start\n");

	if (!buf || !ddev || !p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return count;
	}

	crtc = list_first_entry(&(ddev)->mode_config.crtc_list,
				typeof(*crtc), head);
	if (!crtc) {
		OFP_ERR("find crtc fail\n");
		return -EINVAL;
	}

	sscanf(buf, "%u", &ultra_low_power_aod_mode);

	last_ultra_low_power_aod_mode = p_oplus_ofp_params->ultra_low_power_aod_mode;
	p_oplus_ofp_params->ultra_low_power_aod_mode = ultra_low_power_aod_mode;
	OFP_INFO("ultra_low_power_aod_mode:%u\n", p_oplus_ofp_params->ultra_low_power_aod_mode);
	OPLUS_OFP_TRACE_INT("oplus_ofp_ultra_low_power_aod_mode|%d", p_oplus_ofp_params->ultra_low_power_aod_mode);

	if (!oplus_ofp_is_supported()) {
		OFP_INFO("aod is not supported\n");
		return 0;
	} else if (!oplus_ofp_get_aod_state()) {
		OFP_INFO("not in aod mode, should not set ultra_low_power_aod_mode\n");
		return 0;
	} else if (oplus_ofp_get_hbm_state()) {
		OFP_INFO("ignore aod light mode setting in hbm state\n");
		return 0;
	}

	if (last_ultra_low_power_aod_mode != p_oplus_ofp_params->ultra_low_power_aod_mode) {
		rc = oplus_ofp_drm_set_ultra_low_power_aod(crtc, p_oplus_ofp_params->ultra_low_power_aod_mode);
		if (rc) {
			OFP_ERR("Failed to send DSI_CMD_ULTRA_LOW_POWER_AOD_ON cmds, rc=%d\n", rc);
		}
	}

	OFP_DEBUG("end\n");
	return count;
}

ssize_t oplus_ofp_get_ultra_low_power_aod_mode_attr(struct kobject *obj,
	struct kobj_attribute *attr, char *buf)
{
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	OFP_DEBUG("start\n");

	if (!buf || !p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	OFP_INFO("ultra_low_power_aod_mode:%u\n", p_oplus_ofp_params->ultra_low_power_aod_mode);

	OFP_DEBUG("end\n");

	return sysfs_emit(buf, "%u\n", p_oplus_ofp_params->ultra_low_power_aod_mode);
}

int oplus_ofp_set_fake_aod(void *buf)
{
	unsigned int *fake_aod_mode = buf;
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	OFP_DEBUG("start\n");

	if (!buf || !p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	OPLUS_OFP_TRACE_BEGIN("oplus_ofp_set_fake_aod");

	p_oplus_ofp_params->fake_aod_mode = *fake_aod_mode;
	OFP_INFO("fake_aod_mode:%u\n", p_oplus_ofp_params->fake_aod_mode);
	OPLUS_OFP_TRACE_INT("oplus_ofp_fake_aod_mode|%d", p_oplus_ofp_params->fake_aod_mode);

	OPLUS_OFP_TRACE_END("oplus_ofp_set_fake_aod");

	OFP_DEBUG("end\n");

	return 0;
}

int oplus_ofp_get_fake_aod(void *buf)
{
	unsigned int *fake_aod_mode = buf;
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	OFP_DEBUG("start\n");

	if (!buf || !p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	OPLUS_OFP_TRACE_BEGIN("oplus_ofp_get_fake_aod");

	*fake_aod_mode = p_oplus_ofp_params->fake_aod_mode;
	OFP_INFO("fake_aod_mode:%u\n", *fake_aod_mode);

	OPLUS_OFP_TRACE_END("oplus_ofp_get_fake_aod");

	OFP_DEBUG("end\n");

	return 0;
}

ssize_t oplus_ofp_set_fake_aod_attr(struct kobject *obj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int fake_aod_mode = 0;
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	OFP_DEBUG("start\n");

	if (!buf || !p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return count;
	}

	OPLUS_OFP_TRACE_BEGIN("oplus_ofp_set_fake_aod_attr");

	sscanf(buf, "%u", &fake_aod_mode);

	p_oplus_ofp_params->fake_aod_mode = fake_aod_mode;
	OFP_INFO("fake_aod_mode:%u\n", p_oplus_ofp_params->fake_aod_mode);
	OPLUS_OFP_TRACE_INT("oplus_ofp_fake_aod_mode|%d", p_oplus_ofp_params->fake_aod_mode);

	OPLUS_OFP_TRACE_END("oplus_ofp_set_fake_aod_attr");

	OFP_DEBUG("end\n");

	return count;
}

ssize_t oplus_ofp_get_fake_aod_attr(struct kobject *obj,
	struct kobj_attribute *attr, char *buf)
{
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	OFP_DEBUG("start\n");

	if (!buf || !p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	OPLUS_OFP_TRACE_BEGIN("oplus_ofp_get_fake_aod_attr");

	OFP_INFO("fake_aod_mode:%u\n", p_oplus_ofp_params->fake_aod_mode);

	OPLUS_OFP_TRACE_END("oplus_ofp_get_fake_aod_attr");

	OFP_DEBUG("end\n");

	return sysfs_emit(buf, "%u\n", p_oplus_ofp_params->fake_aod_mode);
}

int oplus_ofp_set_longrui_aod_mode(void *buf)
{
	int rc = 0;
	unsigned int *longrui_aod_mode = buf;
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	OFP_DEBUG("start\n");

	if (!buf || !p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}
	p_oplus_ofp_params->longrui_aod_mode = (*longrui_aod_mode);
	OFP_INFO("longrui_aod_mode:0x%x\n", p_oplus_ofp_params->longrui_aod_mode);
	OPLUS_OFP_TRACE_INT("oplus_ofp_longrui_aod_mode|%d", p_oplus_ofp_params->longrui_aod_mode);

	if (!oplus_ofp_is_supported()) {
		OFP_DEBUG("ofp is not supported\n");
		return 0;
	} else if (oplus_ofp_get_hbm_state()) {
		OFP_INFO("ignore longrui aod mode setting in hbm state\n");
		return 0;
	}

	OPLUS_OFP_TRACE_BEGIN("oplus_ofp_set_longrui_aod_mode");

	if ((p_oplus_ofp_params->longrui_aod_config & OPLUS_OFP_A_MIRROR_TO_THE_END_AOD_CONFIG)
			&& (p_oplus_ofp_params->longrui_aod_mode & OPLUS_OFP_A_MIRROR_TO_THE_END_AOD_MODE)
			&& !(p_oplus_ofp_params->longrui_aod_mode & OPLUS_OFP_AOD_ON)) {
		if (oplus_ofp_get_aod_state() && !p_oplus_ofp_params->doze_active) {
			rc = oplus_ofp_crtc_aod_off_set();
			if (rc) {
				OFP_ERR("failed to set aod off, rc=%d\n", rc);
			}
		}
	}

	OPLUS_OFP_TRACE_END("oplus_ofp_set_longrui_aod_mode");

	OFP_DEBUG("end\n");

	return rc;
}

int oplus_ofp_get_longrui_aod_config(void *buf)
{
	unsigned int *longrui_aod_config = buf;
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	OFP_DEBUG("start\n");

	if (!buf || !p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	OPLUS_OFP_TRACE_BEGIN("oplus_ofp_get_longrui_aod_config");

	*longrui_aod_config = p_oplus_ofp_params->longrui_aod_config;
	OFP_INFO("longrui_aod_config:0x%x\n", *longrui_aod_config);

	OPLUS_OFP_TRACE_END("oplus_ofp_get_longrui_aod_config");

	OFP_DEBUG("end\n");

	return 0;
}

ssize_t oplus_ofp_set_longrui_aod_mode_attr(struct kobject *obj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int rc = 0;
	unsigned int longrui_aod_mode = 0;
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	OFP_DEBUG("start\n");

	if (!buf || !p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return count;
	}

	sscanf(buf, "%u", &longrui_aod_mode);
	p_oplus_ofp_params->longrui_aod_mode = longrui_aod_mode;
	OFP_INFO("longrui_aod_mode:0x%x\n", p_oplus_ofp_params->longrui_aod_mode);
	OPLUS_OFP_TRACE_INT("oplus_ofp_longrui_aod_mode|%d", p_oplus_ofp_params->longrui_aod_mode);

	if (!oplus_ofp_is_supported()) {
		OFP_DEBUG("ofp is not supported\n");
		return count;
	} else if (oplus_ofp_get_hbm_state()) {
		OFP_INFO("ignore longrui aod mode setting in hbm state\n");
		return count;
	}

	OPLUS_OFP_TRACE_BEGIN("oplus_ofp_set_longrui_aod_mode_attr");

	if ((p_oplus_ofp_params->longrui_aod_config & OPLUS_OFP_A_MIRROR_TO_THE_END_AOD_CONFIG)
			&& (p_oplus_ofp_params->longrui_aod_mode & OPLUS_OFP_A_MIRROR_TO_THE_END_AOD_MODE)
			&& !(p_oplus_ofp_params->longrui_aod_mode & OPLUS_OFP_AOD_ON)) {
		if (oplus_ofp_get_aod_state() && !p_oplus_ofp_params->doze_active) {
			rc = oplus_ofp_crtc_aod_off_set();
			if (rc) {
				OFP_ERR("failed to set aod off, rc=%d\n", rc);
			}
		}
	}

	OPLUS_OFP_TRACE_END("oplus_ofp_set_longrui_aod_mode_attr");

	OFP_DEBUG("end\n");

	return count;
}

ssize_t oplus_ofp_get_longrui_aod_config_attr(struct kobject *obj,
	struct kobj_attribute *attr, char *buf)
{
	struct oplus_ofp_params *p_oplus_ofp_params = oplus_ofp_get_params();

	OFP_DEBUG("start\n");

	if (!buf || !p_oplus_ofp_params) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	OPLUS_OFP_TRACE_BEGIN("oplus_ofp_get_longrui_aod_config_attr");

	OFP_INFO("longrui_aod_config:0x%x\n", p_oplus_ofp_params->longrui_aod_config);

	OPLUS_OFP_TRACE_END("oplus_ofp_get_longrui_aod_config_attr");

	OFP_DEBUG("end\n");

	return sysfs_emit(buf, "%u\n", p_oplus_ofp_params->longrui_aod_config);
}

MODULE_AUTHOR("Liuhe Zhong");
MODULE_DESCRIPTION("OPPO ofp device");
MODULE_LICENSE("GPL v2");
