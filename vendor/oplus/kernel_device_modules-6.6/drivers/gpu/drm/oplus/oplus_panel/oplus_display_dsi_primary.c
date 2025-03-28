/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_display_dsi_primary.c
** Description : oplus panel driver
** Version : 1.0
** Date : 2024/05/20
** Author : Display
******************************************************************/
#include <linux/backlight.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/drm_modes.h>
#include <linux/delay.h>
#include <drm/drm_connector.h>
#include <drm/drm_device.h>
#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>
#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>
#include <soc/oplus/device_info.h>
#include <linux/proc_fs.h>
#include "mtk_boot_common.h"
#include "mtk_dsi.h"
#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "mtk_panel_ext.h"
#include "mtk_drm_graphics_base.h"
#endif

#ifdef OPLUS_FEATURE_DISPLAY
#include "oplus_display_temp_compensation.h"
#include "oplus_display_debug.h"
#include "oplus_dsi_display_config.h"
#include "oplus_display_utils.h"
#endif /* OPLUS_FEATURE_DISPLAY */

#ifdef OPLUS_FEATURE_DISPLAY_ADFR
#include "oplus_adfr.h"
#endif /* OPLUS_FEATURE_DISPLAY_ADFR */

#ifdef OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT
#include "oplus_display_onscreenfingerprint.h"
#include "mtk-cmdq-ext.h"
#endif /* OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT */

#ifdef OPLUS_FEATURE_DISPLAY_HPWM
#include "oplus_display_pwm.h"
#endif /* OPLUS_FEATURE_DISPLAY_HPWM */

#include "oplus_display_dsi_primary.h"
#include "oplus_display_interface.h"

extern char oplus_display0_cmdline[MAX_CMDLINE_PARAM_LEN];
extern unsigned int last_backlight;
extern unsigned int oplus_display_brightness;
extern unsigned int oplus_max_normal_brightness;
extern unsigned int oplus_max_brightness;
extern atomic_t oplus_pcp_handle_lock;
extern atomic_t oplus_pcp_num;
extern int g_last_mode_idx;
extern struct mutex oplus_pcp_lock;
extern bool pulse_flg;
extern unsigned int silence_mode;
extern bool g_gamma_regs_read_done;
extern char regs1[AC178_GAMMA_COMPENSATION_READ_LENGTH];
extern char regs2[AC178_GAMMA_COMPENSATION_READ_LENGTH];
extern char regs3[AC178_GAMMA_COMPENSATION_READ_LENGTH];

extern volatile unsigned int dsi_panel_mode_id;

extern void lcdinfo_notify(unsigned long val, void *v);
extern inline void set_pwm_turbo_switch_state(enum PWM_SWITCH_STATE state);
extern void set_pwm_turbo_power_on(bool en);
extern struct oplus_pwm_turbo_params *oplus_pwm_turbo_get_params(void);
extern inline bool oplus_panel_pwm_onepulse_is_enabled(void);
extern bool oplus_adfr_is_supported_export(void *drm_crtc);
extern int oplus_display_second_half_frame_sync(struct drm_crtc *crtc, u32 fps);
extern void apollo_set_brightness_for_show(unsigned int level);
DEFINE_MUTEX(oplus_pwm_lock);

static unsigned int temp_seed_mode = 0;
enum dsi_cmd_id temp_seed_cmd = DSI_CMD_ID_MAX;
static enum RES_SWITCH_TYPE res_switch_type = RES_SWITCH_NO_USE;
static int mode_num_cur = 0;
static int frame_height = 0;
static char panel_name[MAX_CMDLINE_PARAM_LEN];
static bool panel_spr_enable;
static ktime_t mode_switch_begin_time = 0;

static inline struct dsi_panel_lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct dsi_panel_lcm, panel);
}

#ifdef PANEL_SUPPORT_READBACK
static int lcm_dcs_read(struct dsi_panel_lcm *ctx, u8 cmd, void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return 0;

	ret = mipi_dsi_dcs_read(dsi, cmd, data, len);
	if (ret < 0) {
		dev_info(ctx->dev, "error %d reading dcs seq:(%#x)\n", ret,
			 cmd);
		ctx->error = ret;
	}

	return ret;
}
static void lcm_panel_get_data(struct dsi_panel_lcm *ctx)
{
	u8 buffer[3] = { 0 };
	static int ret;

	OPLUS_DSI_INFO("Execute\n");

	if (ret == 0) {
		ret = lcm_dcs_read(ctx, 0x0A, buffer, 1);
		OPLUS_DSI_INFO("0x%08X\n", buffer[0] | (buffer[1] << 8));
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static int get_mode_enum(struct drm_display_mode *m)
{
	int ret = 0;
	int m_vrefresh = 0;

	if (m == NULL) {
		OPLUS_DSI_WARN("get_mode_enum drm_display_mode *m is null, default 120fps\n");
		ret = FHD_SDC120;
		return ret;
	}

	m_vrefresh = drm_mode_vrefresh(m);

	if (m_vrefresh == 60 && m->hskew == STANDARD_MFR) {
		ret = FHD_SDC60;
	} else if (m_vrefresh == 90 && m->hskew == STANDARD_ADFR) {
		ret = FHD_SDC90;
	} else if (m_vrefresh == 120 && m->hskew == STANDARD_ADFR) {
		ret = FHD_SDC120;
	} else if (m_vrefresh == 120 && m->hskew == OPLUS_ADFR) {
		ret = FHD_OPLUS120;
	}

	return ret;
}

static void lcm_panel_init_pre(struct dsi_panel_lcm *ctx)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);

	OPLUS_DSI_DEBUG("Start\n");

	if (!strcmp("panel_ac180_p_3_a0020_dsi_cmd", panel_name)) {
		if (ctx->panel_info.panel_flag & 0x10) {
			oplus_dsi_panel_send_cmd(dsi, DSI_CMD_PANEL_COMPENSATION, NULL, DSI_CMD_FUNC_DEFAULT);
			if (g_gamma_regs_read_done) {
				oplus_dsi_panel_send_cmd(dsi, DSI_CMD_GAMMA_COMPENSATION, NULL, DSI_CMD_FUNC_HOST);
			}
		}
		if (ctx->panel_info.panel_flag & 0x20) {
			oplus_dsi_panel_send_cmd(dsi, DSI_CMD_SET_ON_PRE, NULL, DSI_CMD_FUNC_DEFAULT);
		}
	} else {
		if (lcm_all_cmd_table[DSI_CMD_PANEL_COMPENSATION].cmd_lines > 0) {
			oplus_dsi_panel_send_cmd(dsi, DSI_CMD_PANEL_COMPENSATION, NULL, DSI_CMD_FUNC_DEFAULT);
		}
		if (lcm_all_cmd_table[DSI_CMD_SET_ON_PRE].cmd_lines > 0) {
			oplus_dsi_panel_send_cmd(dsi, DSI_CMD_SET_ON_PRE, NULL, DSI_CMD_FUNC_DEFAULT);
		}
	}

	OPLUS_DSI_DEBUG("End\n");
}

static void lcm_panel_init_post(struct dsi_panel_lcm *ctx)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	bool enable = oplus_panel_pwm_onepulse_is_enabled();

	OPLUS_DSI_DEBUG("Start\n");

	if (enable) {
		OPLUS_DSI_INFO("HPWM onepulse enable=%d, send switch cmd\n", enable);
		oplus_dsi_panel_send_cmd(dsi, DSI_CMD_PWM_SWITCH_3TO1PUL, NULL, DSI_CMD_FUNC_HOST);
	}

	if (temp_seed_mode && temp_seed_cmd != DSI_CMD_ID_MAX) {
		OPLUS_DSI_INFO("restore seed mode:%d, send cmd:%d\n", temp_seed_mode, temp_seed_cmd);
		oplus_dsi_panel_send_cmd(dsi, temp_seed_cmd, NULL, DSI_CMD_FUNC_HOST);
	}

	if (lcm_all_cmd_table[DSI_CMD_SET_ON_POST].cmd_lines > 0) {
		oplus_dsi_panel_send_cmd(dsi, DSI_CMD_SET_ON_POST, NULL, DSI_CMD_FUNC_DEFAULT);
	}

	OPLUS_DSI_DEBUG("End\n");
}

static void lcm_panel_init(struct dsi_panel_lcm *ctx)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);

	OPLUS_DSI_DEBUG("Start\n");

	oplus_dsi_panel_send_cmd(dsi, DSI_CMD_SET_ON, NULL, DSI_CMD_FUNC_DEFAULT);

	OPLUS_DSI_DEBUG("End\n");
}

static int lcm_disable(struct drm_panel *panel)
{
	struct dsi_panel_lcm *ctx = panel_to_lcm(panel);

	if (!ctx->enabled)
		return 0;

	OPLUS_DSI_INFO("Execute\n");

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return 0;
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct dsi_panel_lcm *ctx = panel_to_lcm(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	if (!ctx->prepared)
		return 0;

	OPLUS_DSI_INFO("Start\n");

	if (oplus_ofp_get_aod_state() == true) {
		oplus_dsi_panel_send_cmd(dsi, DSI_CMD_SET_NOLP, NULL, DSI_CMD_FUNC_HOST);
		usleep_range(9000, 9100);
		OFP_INFO("send aod off cmd\n");
	}

	oplus_dsi_panel_send_cmd(dsi, DSI_CMD_SET_OFF, NULL, DSI_CMD_FUNC_DEFAULT);

	OPLUS_DSI_INFO("clear pulse status flag\n");
	pulse_flg = false;

	while (atomic_read(&oplus_pcp_handle_lock) > 0) {
		OPLUS_DSI_INFO("atommic ++ %d\n", atomic_read(&oplus_pcp_handle_lock));
		atomic_dec(&oplus_pcp_handle_lock);
		OPLUS_DSI_INFO("atommic -- %d\n", atomic_read(&oplus_pcp_handle_lock));
		mutex_unlock(&oplus_pcp_lock);
		atomic_dec(&oplus_pcp_num);
		OPLUS_DSI_INFO("oplus_pcp_unlock\n");
	}
	OPLUS_DSI_INFO("oplus_pcp_handle_lock = %d, oplus_pcp_num = %d\n",
		atomic_read(&oplus_pcp_handle_lock),
		atomic_read(&oplus_pcp_num));

	ctx->error = 0;
	ctx->prepared = false;
	OPLUS_DSI_INFO("End\n");

	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct dsi_panel_lcm *ctx = panel_to_lcm(panel);
	int ret;

	if (ctx->prepared)
		return 0;

	OPLUS_DSI_INFO("Start\n");

	lcm_panel_init_pre(ctx);
	lcm_panel_init(ctx);
	lcm_panel_init_post(ctx);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;
#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif
	OPLUS_DSI_INFO("End\n");

	return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct dsi_panel_lcm *ctx = panel_to_lcm(panel);

	if (ctx->enabled)
		return 0;

	OPLUS_DSI_INFO("Execute\n");

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;

	return 0;
}

#if defined(CONFIG_MTK_PANEL_EXT)
static int panel_ata_check(struct drm_panel *panel)
{
	/* Customer test by own ATA tool */
	return 1;
}

#ifdef OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT
static int panel_lhbm_pressed_icon_grayscale_update(void *para_list, unsigned int bl_level)
{
	bool need_to_update_grayscale = false;
	unsigned char *tx_buf = para_list;
	unsigned char tx_buf_0[3] = {0xB8, 0x00, 0x00};
	int rc = 0;

	OFP_DEBUG("start\n");

	if (!oplus_ofp_local_hbm_is_enabled()) {
		OFP_DEBUG("local hbm is not enabled, no need to update lhbm pressed icon grayscale\n");
		return 0;
	}

	if (!tx_buf) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	if (!oplus_panel_pwm_onepulse_is_enabled()) {
		if ((bl_level > 0x0000) && (bl_level <= 0x0482)) {
			memcpy(tx_buf, tx_buf_0, 3);
			need_to_update_grayscale = true;
		} else if ((bl_level > 0x0482) && (bl_level <= 0x074D)) {
			tx_buf[1] = (((bl_level - 1154) * 1300 + 358) / 715) >> 8;
			tx_buf[2] = (((bl_level - 1154) * 1300 + 358) / 715) & 0xFF;
			need_to_update_grayscale = true;
		} else if ((bl_level > 0x074D) && (bl_level <= 0x0DBB)) {
			tx_buf[1] = (((bl_level - 1869) * 748 + 826) / 1646 + 1300) >> 8;
			tx_buf[2] = (((bl_level - 1869) * 748 + 826) / 1646 + 1300) & 0xFF;
			need_to_update_grayscale = true;
		} else if ((bl_level > 0x0DBB) && (bl_level <= 0x0FFE)) {
			tx_buf[1] = 0x08;
			tx_buf[2] = 0x00;
			need_to_update_grayscale = true;
		}
	} else {
		if ((bl_level > 0x0000) && (bl_level <= 0x0482)) {
			memcpy(tx_buf, tx_buf_0, 3);
			need_to_update_grayscale = true;
		} else if ((bl_level > 0x0482) && (bl_level <= 0x074D)) {
			tx_buf[1] = (((bl_level - 1154) * 1300 + 358) / 715) >> 8;
			tx_buf[2] = (((bl_level - 1154) * 1300 + 358) / 715) & 0xFF;
			need_to_update_grayscale = true;
		} else if ((bl_level > 0x074D) && (bl_level <= 0x0DBB)) {
			tx_buf[1] = (((bl_level - 1869) * 748 + 826) / 1646 + 1300) >> 8;
			tx_buf[2] = (((bl_level - 1869) * 748 + 826) / 1646 + 1300) & 0xFF;
			need_to_update_grayscale = true;
		} else if ((bl_level > 0x0DBB) && (bl_level <= 0x0FFE)) {
			tx_buf[1] = 0x08;
			tx_buf[2] = 0x00;
			need_to_update_grayscale = true;
		}
	}

	if (need_to_update_grayscale) {
		OFP_DEBUG("lhbm pressed icon grayscale:0x%02X, 0x%02X\n", tx_buf[1],  tx_buf[2]);
		rc = 1;
	}

	OFP_DEBUG("end\n");

	return rc;
}
#endif /* OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT */

static int oplus_display_panel_set_demura_bl(void *dsi, void *handle, int bl_lvl, enum dsi_cmd_func cmd_state)
{
	int ret = 0, i = 0;
	int *dbv_cfg = oplus_display0_params->demura_dbv_cfg;
	int dbv_length = oplus_display0_params->demura_dbv_length;
	int dbv_cmd = DSI_CMD_DEMURA_DBV_MODE0;
	int dbv_cmd_max = DSI_CMD_DEMURA_DBV_MAX;

	if (bl_lvl == 0 || bl_lvl == 1) {
		return -EINVAL;
	}

	if (!strcmp("panel_ac178_p_7_a0014_dsi_cmd", panel_name)) {
		if (m_db >= 0x03 && m_db <= 0x04) {
			dbv_cfg = oplus_display0_params->emduty_dbv_cfg;
			dbv_length = oplus_display0_params->emduty_dbv_length;
			dbv_cmd = DSI_CMD_EMDUTY_DBV_MODE0;
			dbv_cmd_max = DSI_CMD_EMDUTY_DBV_MAX;
		} else if (m_db < 0x02) {
			return -EFAULT;
		}
	}

	for (i = 0; i < dbv_length; i++, dbv_cmd++) {
		if (bl_lvl <= dbv_cfg[i]) {
			if (dbv_cmd <= dbv_cmd_max) {
				ret = oplus_dsi_panel_send_cmd(dsi, dbv_cmd, handle, cmd_state);
				break;
			} else {
				OPLUS_DSI_ERR("Invalid dbv_cmd:%d > %d\n", dbv_cmd, dbv_cmd_max);
				return -EINVAL;
			}
		}
	}
	if (i == dbv_length) {
		if (dbv_cmd <= dbv_cmd_max) {
			ret = oplus_dsi_panel_send_cmd(dsi, dbv_cmd, handle, cmd_state);
		} else {
			OPLUS_DSI_ERR("Invalid dbv_cmd:%d > %d\n", dbv_cmd, dbv_cmd_max);
			return -EINVAL;
		}
	}

	return ret;
}

static int oplus_panel_set_backlight_cmdq(void *dsi, dcs_write_gce_pack cb, void *handle, unsigned int level)
{
	enum dsi_cmd_func cmd_state = DSI_CMD_FUNC_DEFAULT;
	u32 panel_info = 0;
	bool normal_backlight_compensate1 = false;
	bool normal_backlight_compensate2 = false;
	struct oplus_pwm_turbo_params *pwm_params = oplus_pwm_turbo_get_params();

	if (!dsi) {
		OPLUS_DSI_ERR("Invalid dsi\n");
		return -EINVAL;
	}

	panel_info = oplus_dsi_panel_get_panel_info(dsi);

	if (last_backlight == 0 || level == 0) {
		OPLUS_DSI_INFO("<0x%08X> set backlight level:%u\n", panel_info, level);
	} else {
		OPLUS_DSI_DEBUG("<0x%08X> set backlight level:%u\n", panel_info, level);
	}

	if (cb) {
		cmd_state = DSI_CMD_FUNC_DEFAULT;
	} else if (handle) {
		cmd_state = DSI_CMD_FUNC_GCE;
	} else {
		cmd_state = DSI_CMD_FUNC_GCE2;
	}

#ifdef OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT
	if (strcmp("panel_ac178_p_7_a0014_dsi_cmd", panel_name) == 0) {
		if (oplus_ofp_is_supported()) {
			if (lcm_all_cmd_table[DSI_CMD_LHBM_PRESSED_ICON_GRAYSCALE].cmd_lines >= 3) {
				if (panel_lhbm_pressed_icon_grayscale_update(
						lcm_all_cmd_table[DSI_CMD_LHBM_PRESSED_ICON_GRAYSCALE].para_table[2].para_list, level) == 1) {
					oplus_dsi_panel_send_cmd(dsi, DSI_CMD_LHBM_PRESSED_ICON_GRAYSCALE,
							handle, cmd_state);
				}
			}
		}
	}
#endif /* OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT */

	if (level == 1) {
		OPLUS_DSI_INFO("filter backlight %u setting\n", level);
		last_backlight = level;
		return 0;
	} else if (level > oplus_display0_params->backlight_info.oplus_brightness_hw_max) {
		level = oplus_display0_params->backlight_info.oplus_brightness_hw_max;
	}

	if (get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT && level > 0) {
		level = oplus_display0_params->backlight_info.oplus_brightness_default;
	}

	if (silence_mode) {
		OPLUS_DSI_INFO("lcm silence_mode is %d, set backlight to 0\n", silence_mode);
		level = 0;
	}

	if ((pwm_params != NULL) && pwm_params->pwm_aid_switch_enable && (level > 1) && !oplus_panel_pwm_onepulse_is_enabled()) {
		normal_backlight_compensate1 = (last_backlight <= get_pwm_turbo_pulse_bl()) && (level > get_pwm_turbo_pulse_bl());
		normal_backlight_compensate2 = (last_backlight > get_pwm_turbo_pulse_bl()) && (level <= get_pwm_turbo_pulse_bl());
		if (normal_backlight_compensate1) {
			OPLUS_DSI_INFO("last_backlight=%d, level=%d, Normal mode send PWM_SWITCH_18TO3PUL cmd\n", last_backlight, level);
			oplus_dsi_panel_send_cmd(dsi, DSI_CMD_PWM_SWITCH_18TO3PUL, handle, cmd_state);
		} else if (normal_backlight_compensate2) {
			OPLUS_DSI_INFO("last_backlight=%d, level=%d, Normal mode send PWM_SWITCH_3TO18PUL cmd\n", last_backlight, level);
			oplus_dsi_panel_send_cmd(dsi, DSI_CMD_PWM_SWITCH_3TO18PUL, handle, cmd_state);
		}
	}
	oplus_display_brightness = level;
	if ((lcm_all_cmd_table[DSI_CMD_SET_BACKLIGHT].cmd_lines > 0) &&
			(lcm_all_cmd_table[DSI_CMD_SET_BACKLIGHT].para_table[0].count > 2)) {
		lcm_all_cmd_table[DSI_CMD_SET_BACKLIGHT].para_table[0].para_list[1] = level >> 8;
		lcm_all_cmd_table[DSI_CMD_SET_BACKLIGHT].para_table[0].para_list[2] = level & 0xFF;
	} else {
		OPLUS_DSI_ERR("mode[%d] DSI_CMD_SET_BACKLIGHT count is error\n", dsi_panel_mode_id);
	}
	oplus_dsi_panel_send_cmd(dsi, DSI_CMD_SET_BACKLIGHT, handle, cmd_state);

	if (oplus_display0_params->demura_dbv_flag) {
		oplus_display_panel_set_demura_bl(dsi, handle, level, cmd_state);
	}
	lcdinfo_notify(LCM_BRIGHTNESS_TYPE, &level);
	last_backlight = level;

	return 0;
}

#ifdef OPLUS_FEATURE_DISPLAY_HPWM
static int oplus_panel_set_pwm_pulse(void *dsi, dcs_write_gce_pack cb, void *handle, unsigned int enable)
{
	struct oplus_pwm_turbo_params *pwm_params = oplus_pwm_turbo_get_params();

	if (!dsi) {
		return -EINVAL;
	}
	if (!pwm_params) {
		OPLUS_PWM_ERR("pwm_params is NULL\n");
		return -EINVAL;
	}

	OPLUS_DSI_INFO("set to %dpulse\n", enable ? 1 : 3);

	switch (pwm_params->pwm_pul_cmd_id) {
	case PWM_SWITCH_1TO3:
		oplus_dsi_panel_send_cmd(dsi, DSI_CMD_PWM_SWITCH_1TO3PUL, handle, DSI_CMD_FUNC_DEFAULT);
		break;
	case PWM_SWITCH_3TO1:
		oplus_dsi_panel_send_cmd(dsi, DSI_CMD_PWM_SWITCH_3TO1PUL, handle, DSI_CMD_FUNC_DEFAULT);
		break;
	case PWM_SWITCH_18TO1:
		oplus_dsi_panel_send_cmd(dsi, DSI_CMD_PWM_SWITCH_18TO1PUL, handle, DSI_CMD_FUNC_DEFAULT);
		break;
	case PWM_SWITCH_1TO18:
		oplus_dsi_panel_send_cmd(dsi, DSI_CMD_PWM_SWITCH_1TO18PUL, handle, DSI_CMD_FUNC_DEFAULT);
		break;
	default:
		OPLUS_PWM_WARN("Invalid pwm_pul_cmd_id CMD\n");
		break;
	}

	return 0;
}
#endif /* OPLUS_FEATURE_DISPLAY_HPWM */

static int panel_set_seed(void *dsi, dcs_write_gce_pack cb, void *handle, unsigned int mode)
{
	enum dsi_cmd_func cmd_state = DSI_CMD_FUNC_DEFAULT;

	OPLUS_DSI_INFO("mode=%d\n", mode);

	if (!dsi) {
		OPLUS_DSI_ERR("Invalid params\n");
		return -EINVAL;
	}

	temp_seed_mode = mode;

	if (cb) {
		cmd_state = DSI_CMD_FUNC_DEFAULT;
	} else if (handle) {
		cmd_state = DSI_CMD_FUNC_GCE;
	} else {
		cmd_state = DSI_CMD_FUNC_GCE2;
	}

	switch(mode) {
	case VIVID:
		temp_seed_cmd = DSI_CMD_SET_SEED_VIVID;
		break;
	case EXPERT:
		temp_seed_cmd = DSI_CMD_SET_SEED_EXPERT;
		break;
	case NATURAL:
		temp_seed_cmd = DSI_CMD_SET_SEED_NATURAL;
		break;
	case UIR_ON_VIVID:
		temp_seed_cmd = DSI_CMD_SET_UIR_ON_SEED_VIVID;
		break;
	case UIR_ON_EXPERT:
		temp_seed_cmd = DSI_CMD_SET_UIR_ON_SEED_EXPERT;
		break;
	case UIR_ON_NATURAL:
		temp_seed_cmd = DSI_CMD_SET_UIR_ON_SEED_NATURAL;
		break;
	case UIR_OFF_VIVID:
		temp_seed_cmd = DSI_CMD_SET_UIR_OFF_SEED_VIVID;
		break;
	case UIR_OFF_EXPERT:
		temp_seed_cmd = DSI_CMD_SET_UIR_OFF_SEED_EXPERT;
		break;
	case UIR_OFF_NATURAL:
		temp_seed_cmd = DSI_CMD_SET_UIR_OFF_SEED_NATURAL;
		break;
	case UIR_VIVID:
		temp_seed_cmd = DSI_CMD_SET_UIR_SEED_VIVID;
		break;
	case UIR_EXPERT:
		temp_seed_cmd = DSI_CMD_SET_UIR_SEED_EXPERT;
		break;
	case UIR_NATURAL:
		temp_seed_cmd = DSI_CMD_SET_UIR_SEED_NATURAL;
		break;
	default:
		temp_seed_cmd = DSI_CMD_ID_MAX;
		break;
	}

	if (temp_seed_cmd != DSI_CMD_ID_MAX) {
		oplus_dsi_panel_send_cmd(dsi, temp_seed_cmd, handle, cmd_state);
	} else {
		OPLUS_DSI_ERR("Invalid seed cmd\n");
	}

	return 0;
}

#ifdef OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT
static int lcm_set_hbm(void *dsi, dcs_write_gce_pack cb,
		void *handle, unsigned int hbm_mode)
{
	OFP_DEBUG("start\n");

	if (!dsi) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	if(hbm_mode == 1) {
		oplus_dsi_panel_send_cmd(dsi, DSI_CMD_HBM_ON, handle, DSI_CMD_FUNC_DEFAULT);
		last_backlight = oplus_display0_params->backlight_info.oplus_brightness_hw_max;
		OFP_INFO("Enter hbm mode, set last_backlight as %d", last_backlight);
		lcdinfo_notify(1, &hbm_mode);
	} else if (hbm_mode == 0) {
		oplus_dsi_panel_send_cmd(dsi, DSI_CMD_HBM_OFF, handle, DSI_CMD_FUNC_DEFAULT);
		lcdinfo_notify(1, &hbm_mode);
		oplus_panel_set_backlight_cmdq(dsi, cb, handle, oplus_display_brightness);
	}

	OFP_DEBUG("end\n");

	return 0;
}

static int panel_hbm_set_cmdq(struct drm_panel *panel, void *dsi,
		dcs_write_gce_pack cb, void *handle, bool en)
{
	enum dsi_cmd_id cmd_set_id = DSI_CMD_ID_MAX;

	OFP_DEBUG("start\n");

	if (!panel || !dsi) {
		OFP_ERR("Invalid input params\n");
		return -EINVAL;
	}

	if (en) {
		last_backlight = oplus_display0_params->backlight_info.oplus_brightness_hw_max;
		OFP_INFO("Enter hbm mode, set last_backlight as %d", last_backlight);
		cmd_set_id = DSI_CMD_HBM_ON;
	} else {
		cmd_set_id = DSI_CMD_HBM_OFF;
	}

	oplus_dsi_panel_send_cmd(dsi, cmd_set_id, handle, DSI_CMD_FUNC_DEFAULT);
	lcdinfo_notify(1, &en);

	if (!en) {
		set_pwm_turbo_power_on(true);
		oplus_panel_set_backlight_cmdq(dsi, cb, handle, oplus_display_brightness);
	}

	OFP_DEBUG("end\n");

	return 0;
}

static int oplus_ofp_set_lhbm_pressed_icon(struct drm_panel *panel, void *dsi_drv, dcs_write_gce_pack cb, void *handle, uint64_t lhbm_pressed_icon_on)
{
	u32 index = 0;
	u32 seed_gain = 0;
	static char r_reg1 = 0;
	static char r_reg2 = 0;
	static char g_reg1 = 0;
	static char g_reg2 = 0;
	static char b_reg1 = 0;
	static char b_reg2 = 0;
	enum dsi_cmd_id cmd_set_id = DSI_CMD_ID_MAX;
	struct dsi_cmd_table *cmd_table = NULL;
	struct dsi_panel_lcm *ctx = oplus_mtkDsi_to_panel(dsi_drv);

	OFP_DEBUG("start\n");

	if (!oplus_ofp_local_hbm_is_enabled()) {
		OFP_DEBUG("local hbm is not enabled, should not set lhbm pressed icon\n");
	}

	if (!panel || !dsi_drv) {
		OFP_ERR("Invalid input params\n");
		return -EINVAL;
	}

	OFP_INFO("lhbm_pressed_icon_on:%llu,bl_lvl:%u\n", lhbm_pressed_icon_on, oplus_display_brightness);

	if (lhbm_pressed_icon_on) {
		if (strcmp("panel_ac178_p_7_a0014_dsi_cmd", panel_name) == 0) {
			if (temp_seed_cmd == DSI_CMD_SET_SEED_VIVID) {
				seed_gain = 395;
			} else if (temp_seed_cmd == DSI_CMD_SET_SEED_EXPERT) {
				seed_gain = 410;
			} else if (temp_seed_cmd == DSI_CMD_SET_SEED_NATURAL) {
				seed_gain = 410;
			} else {
				seed_gain = 410;
			}
			r_reg1 = ((((regs1[AC178_GAMMA_COMPENSATION_REG_INDEX1] << 8)
					| regs1[AC178_GAMMA_COMPENSATION_REG_INDEX2]) * seed_gain / 100U) >> 8) & 0xFF;
			r_reg2 = (((regs1[AC178_GAMMA_COMPENSATION_REG_INDEX1] << 8)
					| regs1[AC178_GAMMA_COMPENSATION_REG_INDEX2]) * seed_gain / 100U) & 0xFF;
			g_reg1 = ((((regs2[AC178_GAMMA_COMPENSATION_REG_INDEX1] << 8)
					| regs2[AC178_GAMMA_COMPENSATION_REG_INDEX2]) * seed_gain / 100U) >> 8) & 0xFF;
			g_reg2 = (((regs2[AC178_GAMMA_COMPENSATION_REG_INDEX1] << 8)
					| regs2[AC178_GAMMA_COMPENSATION_REG_INDEX2]) * seed_gain / 100U) & 0xFF;
			b_reg1 = ((((regs3[AC178_GAMMA_COMPENSATION_REG_INDEX1] << 8)
					| regs3[AC178_GAMMA_COMPENSATION_REG_INDEX2]) * seed_gain / 100U) >> 8) & 0xFF;
			b_reg2 = (((regs3[AC178_GAMMA_COMPENSATION_REG_INDEX1] << 8)
					| regs3[AC178_GAMMA_COMPENSATION_REG_INDEX2]) * seed_gain / 100U) & 0xFF;
			OFP_INFO("compensation regs1=[%02X %02X], regs2=[%02X %02X], regs3=[%02X %02X]\n",
					r_reg1, r_reg2, g_reg1, g_reg2, b_reg1, b_reg2);

			for (index = 0; index < ctx->mode_num; index++) {
				cmd_table = lcm_cur_cmd_table(index)[DSI_CMD_GAMMA_COMPENSATION].para_table;
				if ((lcm_cur_cmd_table(index)[DSI_CMD_GAMMA_COMPENSATION].cmd_lines >= AC178_GAMMA_COMPENSATION_CMD_INDEX + 1) && \
						(cmd_table[AC178_GAMMA_COMPENSATION_CMD_INDEX].count >= (AC178_GAMMA_COMPENSATION_REG_INDEX6 + 1))) {
					cmd_table[AC178_GAMMA_COMPENSATION_CMD_INDEX].para_list[AC178_GAMMA_COMPENSATION_REG_INDEX1 + 1] = r_reg1;
					cmd_table[AC178_GAMMA_COMPENSATION_CMD_INDEX].para_list[AC178_GAMMA_COMPENSATION_REG_INDEX2 + 1] = r_reg2;
					cmd_table[AC178_GAMMA_COMPENSATION_CMD_INDEX].para_list[AC178_GAMMA_COMPENSATION_REG_INDEX3 + 1] = g_reg1;
					cmd_table[AC178_GAMMA_COMPENSATION_CMD_INDEX].para_list[AC178_GAMMA_COMPENSATION_REG_INDEX4 + 1] = g_reg2;
					cmd_table[AC178_GAMMA_COMPENSATION_CMD_INDEX].para_list[AC178_GAMMA_COMPENSATION_REG_INDEX5 + 1] = b_reg1;
					cmd_table[AC178_GAMMA_COMPENSATION_CMD_INDEX].para_list[AC178_GAMMA_COMPENSATION_REG_INDEX6 + 1] = b_reg2;
				} else {
					OFP_INFO("Invalid mode[%d]-dsi_cmd: DSI_CMD_GAMMA_COMPENSATION\n", index);
				}
			}
			oplus_dsi_panel_send_cmd(dsi_drv, DSI_CMD_GAMMA_COMPENSATION, handle, DSI_CMD_FUNC_DEFAULT);

			OFP_INFO("entering local hbm, recover backlight: %d\n", oplus_display_brightness);
			if (oplus_display_brightness > 0x0DBB) {
				oplus_panel_set_backlight_cmdq(dsi_drv, cb, handle, 0x0DBB);
			} else {
				oplus_panel_set_backlight_cmdq(dsi_drv, cb, handle, oplus_display_brightness);
			}
			if (oplus_display_brightness > LHBM_BACKLIGHT_THRESHOLD) {
				cmd_set_id = DSI_CMD_LHBM_PRESSED_ICON_ON;
			} else {
				cmd_set_id = DSI_CMD_LHBM_PRESSED_ICON_ON_PWM;
			}
		} else {
			cmd_set_id = DSI_CMD_LHBM_PRESSED_ICON_ON;
		}
	} else {
		cmd_set_id = DSI_CMD_LHBM_PRESSED_ICON_OFF;
	}

	oplus_dsi_panel_send_cmd(dsi_drv, cmd_set_id, handle, DSI_CMD_FUNC_DEFAULT);

	if (!lhbm_pressed_icon_on) {
		oplus_panel_set_backlight_cmdq(dsi_drv, cb, handle, oplus_display_brightness);
	} else {
		if (oplus_display_brightness > 0x0DBB) {
			OFP_INFO("set backlight level to 0x0DBB after pressed icon on\n");
			oplus_panel_set_backlight_cmdq(dsi_drv, cb, handle, 0x0DBB);
		}
	}

	OFP_DEBUG("end\n");

	return 0;
}

static int panel_doze_disable(struct drm_panel *panel, void *dsi, dcs_write_gce_pack cb, void *handle)
{
	struct mtk_dsi *mtk_dsi = dsi;
	struct drm_crtc *crtc = NULL;
	struct mtk_crtc_state *mtk_state = NULL;

	if (!panel || !mtk_dsi) {
		OFP_ERR("Invalid mtk_dsi params\n");
	}

	crtc = mtk_dsi->encoder.crtc;

	if (!crtc || !crtc->state) {
		OFP_ERR("Invalid crtc param\n");
		return -EINVAL;
	}

	mtk_state = to_mtk_crtc_state(crtc->state);
	if (!mtk_state) {
		OFP_ERR("Invalid mtk_state param\n");
		return -EINVAL;
	}

	OFP_INFO("crtc_active:%d, doze_active:%llu\n", crtc->state->active, mtk_state->prop_val[CRTC_PROP_DOZE_ACTIVE]);

	if (oplus_ofp_need_to_do_aod_off_compensation()
			&& lcm_all_cmd_table[DSI_CMD_AOD_OFF_COMPENSATION].cmd_lines) {
		if (handle) {
			oplus_dsi_panel_send_cmd(dsi, DSI_CMD_AOD_OFF_COMPENSATION, handle, DSI_CMD_FUNC_DEFAULT);
		} else {
			oplus_dsi_panel_send_cmd(dsi, DSI_CMD_AOD_OFF_COMPENSATION, handle, DSI_CMD_FUNC_GCE2);
		}
	} else {
		if (handle) {
			oplus_dsi_panel_send_cmd(dsi, DSI_CMD_SET_NOLP, handle, DSI_CMD_FUNC_DEFAULT);
		} else {
			oplus_dsi_panel_send_cmd(dsi, DSI_CMD_SET_NOLP, handle, DSI_CMD_FUNC_GCE2);
		}
	}

	if(!oplus_ofp_backlight_filter(crtc, handle, oplus_display_brightness))
		oplus_panel_set_backlight_cmdq(dsi, NULL, handle, oplus_display_brightness);

	if (temp_seed_mode)
		panel_set_seed(dsi, NULL, handle, temp_seed_mode);

	OFP_INFO("send aod off cmd\n");

	return 0;
}


static int panel_doze_enable(struct drm_panel *panel, void *dsi, dcs_write_gce_pack cb, void *handle)
{
	struct mtk_dsi *mtk_dsi = dsi;
	struct drm_crtc *crtc = NULL;
	struct mtk_crtc_state *mtk_state = NULL;

	if (!panel || !mtk_dsi) {
		OFP_ERR("Invalid mtk_dsi params\n");
	}

	crtc = mtk_dsi->encoder.crtc;

	if (!crtc || !crtc->state) {
		OFP_ERR("Invalid crtc param\n");
		return -EINVAL;
	}

	mtk_state = to_mtk_crtc_state(crtc->state);
	if (!mtk_state) {
		OFP_ERR("Invalid mtk_state param\n");
		return -EINVAL;
	}

	OFP_INFO("crtc_active:%d, doze_active:%llu\n", crtc->state->active, mtk_state->prop_val[CRTC_PROP_DOZE_ACTIVE]);
	if (handle)
		oplus_dsi_panel_send_cmd(dsi, DSI_CMD_SET_LP1, handle, DSI_CMD_FUNC_DEFAULT);
	else
		oplus_dsi_panel_send_cmd(dsi, DSI_CMD_SET_LP1, handle, DSI_CMD_FUNC_GCE2);

	OFP_INFO("send aod on cmd\n");

	return 0;
}

static int panel_set_aod_light_mode(void *dsi, dcs_write_gce_pack cb, void *handle, unsigned int level)
{
	if (level == 0) {
		if (handle)
			oplus_dsi_panel_send_cmd(dsi, DSI_CMD_AOD_HIGH_LIGHT_MODE, handle, DSI_CMD_FUNC_DEFAULT);
		else
			oplus_dsi_panel_send_cmd(dsi, DSI_CMD_AOD_HIGH_LIGHT_MODE, handle, DSI_CMD_FUNC_GCE2);
	} else {
		if (handle)
			oplus_dsi_panel_send_cmd(dsi, DSI_CMD_AOD_LOW_LIGHT_MODE, handle, DSI_CMD_FUNC_DEFAULT);
		else
			oplus_dsi_panel_send_cmd(dsi, DSI_CMD_AOD_LOW_LIGHT_MODE, handle, DSI_CMD_FUNC_GCE2);
	}
	OFP_INFO("level = %d\n", level);

	return 0;
}

static int panel_set_ultra_low_power_aod(struct drm_panel *panel, void *dsi,
		dcs_write_gce_pack cb, void *handle, unsigned int level)
{
	enum dsi_cmd_id cmd_set_id = DSI_CMD_ID_MAX;

	if (!panel || !dsi) {
		OFP_ERR("Invalid dsi params\n");
	}
	if (level == 1) {
		cmd_set_id = DSI_CMD_ULTRA_LOW_POWER_AOD_ON;
	} else {
		cmd_set_id = DSI_CMD_ULTRA_LOW_POWER_AOD_OFF;
	}
	if (handle)
		oplus_dsi_panel_send_cmd(dsi, cmd_set_id, handle, DSI_CMD_FUNC_DEFAULT);
	else
		oplus_dsi_panel_send_cmd(dsi, cmd_set_id, handle, DSI_CMD_FUNC_GCE2);
	OFP_INFO("level = %d\n", level);

	return 0;
}

#endif /* OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT */

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct dsi_panel_lcm *ctx = panel_to_lcm(panel);

	OPLUS_DSI_INFO("Execute\n");

	oplus_dsi_panel_reset_keep(ctx, on);

	return 0;
}

static int lcm_panel_poweron(struct drm_panel *panel)
{
	struct dsi_panel_lcm *ctx = panel_to_lcm(panel);
	int ret = 0;

	if (ctx->prepared) {
		return 0;
	}
	OPLUS_DSI_INFO("Execute\n");
	if (ctx->panel_reset_sequence[0][0] != PANEL_RESET_POSITION1) {
		ret = oplus_dsi_panel_prepare(ctx);
	}

	if (ctx->error < 0)
		lcm_unprepare(panel);

	return 0;
}

static int lcm_panel_poweroff(struct drm_panel *panel)
{
	struct dsi_panel_lcm *ctx = panel_to_lcm(panel);
	int ret;

	if (ctx->prepared) {
		return 0;
	}
	OPLUS_DSI_INFO("Execute\n");
	if (ctx->error < 0) {
		lcm_unprepare(panel);
	}
	ret = oplus_dsi_panel_reset_keep(ctx, 0);
	ret = oplus_dsi_panel_power_off(ctx);

	return 0;
}

static int lcm_panel_reset(struct drm_panel *panel)
{
	struct dsi_panel_lcm *ctx = panel_to_lcm(panel);

	if (ctx->prepared)
		return 0;

	OPLUS_DSI_INFO("Execute\n");
	if (ctx->panel_reset_sequence[0][0] != PANEL_RESET_POSITION0) {
		oplus_dsi_panel_prepare(ctx);
	}

	return 0;
}

struct drm_display_mode *get_mode_by_id(struct drm_connector *connector,
	unsigned int mode)
{
	struct drm_display_mode *m;
	unsigned int i = 0;
	struct mtk_dsi *dsi = container_of(connector, struct mtk_dsi, conn);
	struct dsi_panel_lcm *ctx = panel_to_lcm(dsi->panel);

	mode = (mode % ctx->mode_num);

	list_for_each_entry(m, &connector->modes, head) {
		if (i == mode)
			return m;
		i++;
	}
	return NULL;
}

static int mtk_panel_ext_param_get(struct drm_panel *panel,
		struct drm_connector *connector,
		struct mtk_panel_params **ext_param,
		unsigned int id)
{
	int ret = 0;
	int m_vrefresh = 0;
	struct drm_display_mode *m = get_mode_by_id(connector, id);
	struct dsi_panel_lcm *ctx = panel_to_lcm(panel);

	m_vrefresh = drm_mode_vrefresh(m);

	if (m_vrefresh == 60 && m->hskew == STANDARD_MFR) {
		*ext_param = &(ctx->ext_params_all)[1];
	}
	else if (m_vrefresh == 120 && m->hskew == STANDARD_ADFR) {
		*ext_param = &(ctx->ext_params_all[0]);
	} else if (m_vrefresh == 90 && m->hskew == STANDARD_ADFR) {
		*ext_param = &(ctx->ext_params_all[2]);
	} else if (m_vrefresh == 120 && m->hskew == OPLUS_ADFR) {
		*ext_param = &(ctx->ext_params_all[3]);
	} else {
		*ext_param = &(ctx->ext_params_all[0]);
	}

	if (*ext_param)
		OPLUS_DSI_DEBUG("data_rate:%d\n", (*ext_param)->data_rate);
	else
		OPLUS_DSI_ERR("ext_param is NULL;\n");

	return ret;
}

enum RES_SWITCH_TYPE mtk_get_res_switch_type(void)
{
	OPLUS_DSI_INFO("res_switch_type: %d\n", res_switch_type);
	return res_switch_type;
}

int mtk_scaling_mode_mapping(int mode_idx)
{
	return MODE_MAPPING_RULE(mode_idx);
}

static int mtk_panel_ext_param_set(struct drm_panel *panel,
			struct drm_connector *connector, unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	int m_vrefresh = 0;
	struct drm_display_mode *m = get_mode_by_id(connector, mode);
	struct dsi_panel_lcm *ctx = panel_to_lcm(panel);

	m_vrefresh = drm_mode_vrefresh(m);
	OPLUS_DSI_INFO("mode=%d, vrefresh=%d, hskew=%d\n", mode, drm_mode_vrefresh(m), m->hskew);

	if (m_vrefresh == 60 && m->hskew == STANDARD_MFR) {
		ext->params = &(ctx->ext_params_all[1]);
	} else if (m_vrefresh == 120 && m->hskew == STANDARD_ADFR) {
		ext->params = &(ctx->ext_params_all[0]);
	} else if (m_vrefresh == 90 && m->hskew == STANDARD_ADFR) {
		ext->params = &(ctx->ext_params_all[2]);
	} else if (m_vrefresh == 120 && m->hskew == OPLUS_ADFR) {
		ext->params = &(ctx->ext_params_all[3]);
	} else {
		ext->params = &(ctx->ext_params_all[0]);
	}

	return ret;
}

#ifdef OPLUS_FEATURE_DISPLAY_ADFR
static int panel_minfps_check(int mode_id, int extend_frame)
{
	if (mode_id == FHD_SDC90) {
		if (extend_frame > OPLUS_ADFR_MIN_FPS_90HZ || extend_frame < OPLUS_ADFR_MIN_FPS_1HZ)
			extend_frame = OPLUS_ADFR_MIN_FPS_90HZ;
	} else if (mode_id == FHD_SDC60) {
		if (extend_frame > OPLUS_ADFR_MIN_FPS_60HZ || extend_frame < OPLUS_ADFR_MIN_FPS_1HZ)
			extend_frame = OPLUS_ADFR_MIN_FPS_60HZ;
	} else {
		if (extend_frame > OPLUS_ADFR_MIN_FPS_120HZ || extend_frame < OPLUS_ADFR_MIN_FPS_1HZ)
			extend_frame = OPLUS_ADFR_MIN_FPS_120HZ;
	}

	return extend_frame;
}

static int panel_set_minfps(void *dsi, struct drm_panel *panel, dcs_write_gce_pack cb, void *handle,
	void *minfps, struct drm_display_mode *m)
{
	unsigned int mode_id = 0;
	unsigned int ext_frame = 0;
	bool cmd_sent = false;
	struct oplus_minfps *min_fps = (struct oplus_minfps *)minfps;

	if (!dsi || !minfps || !m) {
		ADFR_ERR("Invalid params\n");
		return -EINVAL;
	}

	mode_id = get_mode_enum(m);
	ADFR_INFO("mode_id:%u,minfps_flag:%u,extern_frame:%u\n",
				mode_id, min_fps->minfps_flag, min_fps->extend_frame);

	/*update min fps cmd */
	if (!min_fps->minfps_flag) {
		/* update manual min fps */
		ext_frame = panel_minfps_check(mode_id, min_fps->extend_frame);
		switch (ext_frame) {
		case 120:
			if (lcm_cur_cmd_table(mode_id)[DSI_CMD_ADFR_MIN_FPS_120HZ].cmd_lines > 0) {
				oplus_dsi_panel_send_cmd(dsi, DSI_CMD_ADFR_MIN_FPS_120HZ, handle, DSI_CMD_FUNC_DEFAULT);
				cmd_sent = true;
			}
			break;
		case 90:
			if (lcm_cur_cmd_table(mode_id)[DSI_CMD_ADFR_MIN_FPS_90HZ].cmd_lines > 0) {
				oplus_dsi_panel_send_cmd(dsi, DSI_CMD_ADFR_MIN_FPS_90HZ, handle, DSI_CMD_FUNC_DEFAULT);
				cmd_sent = true;
			}
			break;
		case 60:
			if (lcm_cur_cmd_table(mode_id)[DSI_CMD_ADFR_MIN_FPS_60HZ].cmd_lines > 0) {
				oplus_dsi_panel_send_cmd(dsi, DSI_CMD_ADFR_MIN_FPS_60HZ, handle, DSI_CMD_FUNC_DEFAULT);
				cmd_sent = true;
			}
			break;
		case 45:
			if (lcm_cur_cmd_table(mode_id)[DSI_CMD_ADFR_MIN_FPS_45HZ].cmd_lines > 0) {
				oplus_dsi_panel_send_cmd(dsi, DSI_CMD_ADFR_MIN_FPS_45HZ, handle, DSI_CMD_FUNC_DEFAULT);
				cmd_sent = true;
			}
			break;
		case 30:
			if (lcm_cur_cmd_table(mode_id)[DSI_CMD_ADFR_MIN_FPS_30HZ].cmd_lines > 0) {
				oplus_dsi_panel_send_cmd(dsi, DSI_CMD_ADFR_MIN_FPS_30HZ, handle, DSI_CMD_FUNC_DEFAULT);
				cmd_sent = true;
			}
			break;
		case 20:
			if (lcm_cur_cmd_table(mode_id)[DSI_CMD_ADFR_MIN_FPS_20HZ].cmd_lines > 0) {
				oplus_dsi_panel_send_cmd(dsi, DSI_CMD_ADFR_MIN_FPS_20HZ, handle, DSI_CMD_FUNC_DEFAULT);
				cmd_sent = true;
			}
			break;
		case 15:
			if (lcm_cur_cmd_table(mode_id)[DSI_CMD_ADFR_MIN_FPS_15HZ].cmd_lines > 0) {
				oplus_dsi_panel_send_cmd(dsi, DSI_CMD_ADFR_MIN_FPS_15HZ, handle, DSI_CMD_FUNC_DEFAULT);
				cmd_sent = true;
			}
			break;
		case 10:
			if (lcm_cur_cmd_table(mode_id)[DSI_CMD_ADFR_MIN_FPS_10HZ].cmd_lines > 0) {
				oplus_dsi_panel_send_cmd(dsi, DSI_CMD_ADFR_MIN_FPS_10HZ, handle, DSI_CMD_FUNC_DEFAULT);
				cmd_sent = true;
			}
			break;
		case 5:
			if (lcm_cur_cmd_table(mode_id)[DSI_CMD_ADFR_MIN_FPS_5HZ].cmd_lines > 0) {
				oplus_dsi_panel_send_cmd(dsi, DSI_CMD_ADFR_MIN_FPS_5HZ, handle, DSI_CMD_FUNC_DEFAULT);
				cmd_sent = true;
			}
			break;
		case 1:
			if (lcm_cur_cmd_table(mode_id)[DSI_CMD_ADFR_MIN_FPS_1HZ].cmd_lines > 0) {
				oplus_dsi_panel_send_cmd(dsi, DSI_CMD_ADFR_MIN_FPS_1HZ, handle, DSI_CMD_FUNC_DEFAULT);
				cmd_sent = true;
			}
			break;
		default:
			break;
		}
		if (cmd_sent) {
			ADFR_INFO("adfr_minfps_cmd_%dhz sent\n", ext_frame);
		} else {
			ADFR_INFO("adfr_minfps_cmd_%dhz sending failed, no such cmd set", ext_frame);
		}
	}

	return 0;
}

static int panel_set_multite(void *dsi, struct drm_panel *panel, dcs_write_gce_pack cb, void *handle, bool enable)
{
	/*enable or disable multi-te cmds */
	if (enable) {
		ADFR_INFO("multite enabled\n");
		/* enable multi TE */
		oplus_dsi_panel_send_cmd(dsi, DSI_CMD_MULTI_TE_ENABLE, handle, DSI_CMD_FUNC_DEFAULT);
	} else {
		ADFR_INFO("multite disabled\n");
		/* disable multi TE */
		oplus_dsi_panel_send_cmd(dsi, DSI_CMD_MULTI_TE_DISABLE, handle, DSI_CMD_FUNC_DEFAULT);
	}

	return 0;
}
#endif /* OPLUS_FEATURE_DISPLAY_ADFR */

static int oplus_update_time(void)
{
	mode_switch_begin_time = ktime_get();
	return 0;
}

static void mode_switch_delay(void)
{
	ktime_t time_gap = 0;
	unsigned int sleep_time = 0;

	time_gap = ktime_to_us(ktime_sub(ktime_get(), mode_switch_begin_time));
	OPLUS_DSI_INFO("time_gap: %lld, mode_switch_begin_time: %lld\n", time_gap, mode_switch_begin_time);
	if (time_gap > 8300)
		time_gap = 8300;
	sleep_time = (unsigned int)(8300 - time_gap);
	OPLUS_DSI_INFO("sleep_time =  %d\n", sleep_time);
	usleep_range(sleep_time, (sleep_time + 100));
}

static inline void mode_switch_hs_send(int m_vrefresh, void *dsi_drv)
{
	struct oplus_pwm_turbo_params *pwm_params = oplus_pwm_turbo_get_params();
	bool en = oplus_panel_pwm_onepulse_is_enabled();

	if (en) {
		oplus_dsi_panel_send_cmd(dsi_drv, DSI_CMD_SET_TIMING_SWITCH_1PUL, NULL, DSI_CMD_FUNC_DEFAULT);
	} else {
		oplus_dsi_panel_send_cmd(dsi_drv, DSI_CMD_SET_TIMING_SWITCH, NULL, DSI_CMD_FUNC_DEFAULT);
	}
	pwm_params->pwm_fps_mode = m_vrefresh;
}

static int mode_switch_hs(struct drm_panel *panel, struct drm_connector *connector,
		void *dsi_drv, unsigned int cur_mode, unsigned int dst_mode,
			enum MTK_PANEL_MODE_SWITCH_STAGE stage, dcs_write_gce_pack cb)
{
	int ret = 0;
	int m_vrefresh = 0;
	int src_vrefresh = 0;
	/* lk mipi setting is 830 */
	static int last_data_rate = 900;
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	struct drm_display_mode *m = get_mode_by_id(connector, dst_mode);
	struct drm_display_mode *src_m = get_mode_by_id(connector, cur_mode);
	struct dsi_panel_lcm *ctx = panel_to_lcm(panel);
#ifdef OPLUS_FEATURE_DISPLAY_ADFR
	struct mtk_dsi *dsi = container_of(connector, struct mtk_dsi, conn);
	struct drm_crtc *crtc = NULL;

	if (!(dsi->ddp_comp.mtk_crtc)) {
		OPLUS_DSI_ERR("Invalid mtk_crtc\n");
		return -EFAULT;
	}
	crtc = &(dsi->ddp_comp.mtk_crtc->base);
	if (!crtc) {
		OPLUS_DSI_ERR("Invalid crtc\n");
		return -EFAULT;
	}
#endif /* OPLUS_FEATURE_DISPLAY_ADFR */

	OPLUS_DSI_INFO("cur_mode=%d, dst_mode=%d, stage=%d\n", cur_mode, dst_mode, stage);
	if (cur_mode == dst_mode)
		return ret;

#ifdef OPLUS_FEATURE_DISPLAY_ADFR
	if ((m->hskew == STANDARD_MFR || m->hskew == STANDARD_ADFR)
			&& (src_m->hskew == OPLUS_MFR || src_m->hskew == OPLUS_ADFR)) {
		OPLUS_DSI_INFO("OA to SA, send multi_te_disable\n");
		oplus_dsi_panel_send_cmd(dsi_drv, DSI_CMD_MULTI_TE_DISABLE, NULL, DSI_CMD_FUNC_DEFAULT);
	}
#endif /* OPLUS_FEATURE_DISPLAY_ADFR */

	g_last_mode_idx = cur_mode;
	dsi_panel_mode_id = get_mode_enum(m);
	m_vrefresh = drm_mode_vrefresh(m);
	src_vrefresh = drm_mode_vrefresh(src_m);

	OPLUS_DSI_INFO("dsi_panel_mode_id:%d->%d, hdisplay:%d->%d, hskew:%d->%d, vrefresh:%d->%d\n",
			get_mode_enum(src_m), dsi_panel_mode_id, src_m->hdisplay, m->hdisplay,
			src_m->hskew, m->hskew, src_vrefresh, m_vrefresh);

	if (stage == BEFORE_DSI_POWERDOWN) {
		if (m->hskew == STANDARD_MFR || m->hskew == STANDARD_ADFR) {
			if (src_vrefresh > m_vrefresh) {
				mode_switch_hs_send(m_vrefresh, dsi_drv);
				if (oplus_display0_params->mode_switch_h2l_delay) {
					mode_switch_delay();
				}
			}
		}
	} else if (stage == AFTER_DSI_POWERON) {
		ctx->m = m;
		if (src_vrefresh <= m_vrefresh) {
#ifdef OPLUS_FEATURE_DISPLAY_ADFR
			if (oplus_adfr_is_supported_export(crtc)) {
				if (src_vrefresh == 60) {
					oplus_display_second_half_frame_sync(crtc, src_vrefresh);
				}
			}
#endif /* OPLUS_FEATURE_DISPLAY_ADFR */
			mode_switch_hs_send(m_vrefresh, dsi_drv);
		}
	}

	if (ext->params->data_rate != last_data_rate) {
		ret = 1;
		OPLUS_DSI_INFO("need change mipi clk data_rate: %d->%d\n",
				last_data_rate, ext->params->data_rate);
		last_data_rate = ext->params->data_rate;
	}

	return ret;
}

static int oplus_display_panel_set_hbm_max(void *dsi, dcs_write_gce_pack cb, void *handle, unsigned int en)
{
	OPLUS_DSI_INFO("en=%d\n", en);

	if (!dsi) {
		OPLUS_DSI_ERR("Invalid params\n");
		return -EINVAL;
	}

	if (en) {
		oplus_dsi_panel_send_cmd(dsi, DSI_CMD_SWITCH_HBM_APL_ON, handle, DSI_CMD_FUNC_DEFAULT);
		last_backlight = oplus_display0_params->backlight_info.oplus_brightness_hw_max;
		OPLUS_DSI_INFO("Enter hbm max mode, set last_backlight as %d", last_backlight);
	} else if (!en) {
		oplus_dsi_panel_send_cmd(dsi, DSI_CMD_SWITCH_HBM_APL_OFF, handle, DSI_CMD_FUNC_DEFAULT);
		OPLUS_DSI_INFO("hbm_max off, restore bl:%d\n", oplus_display_brightness);
		oplus_panel_set_backlight_cmdq(dsi, cb, handle, oplus_display_brightness);
	}

	return 0;
}

static int lcm_update_roi_cmdq(void *dsi, dcs_write_gce cb, void *handle,
		unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
	int ret = 0;
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + w - 1;
	unsigned int y1 = y0 + h - 1;
	unsigned char x0_msb = ((x0 >> 8) & 0xFF);
	unsigned char x0_lsb = (x0 & 0xFF);
	unsigned char x1_msb = ((x1 >> 8) & 0xFF);
	unsigned char x1_lsb = (x1 & 0xFF);
	unsigned char y0_msb = ((y0 >> 8) & 0xFF);
	unsigned char y0_lsb = (y0 & 0xFF);
	unsigned char y1_msb = ((y1 >> 8) & 0xFF);
	unsigned char y1_lsb = (y1 & 0xFF);
	char roi_x[] = {0x2A, x0_msb, x0_lsb, x1_msb, x1_lsb};
	char roi_y[] = {0x2B, y0_msb, y0_lsb, y1_msb, y1_lsb};

	OPLUS_DSI_DEBUG("(x,y,w,h): (%d,%d,%d,%d)\n", x, y, w, h);

	if (!cb)
		return -1;

	cb(dsi, handle, roi_x, ARRAY_SIZE(roi_x));
	cb(dsi, handle, roi_y, ARRAY_SIZE(roi_y));

	return ret;
}

static void lcm_valid_roi(struct mtk_panel_params *ext_param,
	unsigned int *x, unsigned int *y, unsigned int *w, unsigned int *h)
{
	unsigned int roi_y = *y, roi_h = *h;
	unsigned int slice_height = ext_param->dsc_params.slice_height;
	unsigned int lcm_slice_height = slice_height * 3;
	unsigned int lil_te1_line = 1840;
	unsigned int inteval = 900;
	int line_diff = 0;

	OPLUS_DSI_DEBUG("%s partial roi y:%d height:%d\n", __func__, roi_y, roi_h);

	if (ext_param->spr_params.enable == 1 &&
		ext_param->spr_params.relay == 0)
		slice_height = ext_param->dsc_params_spr_in.slice_height;
	else
		slice_height = ext_param->dsc_params.slice_height;

	if (dsi_panel_mode_id == FHD_SDC90 || dsi_panel_mode_id == FHD_SDC120 ||
		dsi_panel_mode_id == FHD_OPLUS120) {
		if (roi_y >= lil_te1_line && roi_y <= (lil_te1_line + inteval)) {
			line_diff = roi_y - lil_te1_line + slice_height * 2;
			roi_y -= line_diff;
			roi_h += line_diff;
		}
	}

	/* follow ddic vendor's request */
	lcm_slice_height = slice_height * 6;

	if (roi_y % lcm_slice_height != 0) {
		line_diff = roi_y - (roi_y / lcm_slice_height) * lcm_slice_height;
		roi_y -= line_diff;
	}

	roi_h += line_diff;

	if (roi_h % lcm_slice_height != 0) {
		roi_h = ((roi_h / lcm_slice_height) + 1) * lcm_slice_height;

		if (roi_h > frame_height)
			roi_h = frame_height;
	}

	OPLUS_DSI_DEBUG("%s validate partial roi y:%d height:%d\n", __func__, roi_y, roi_h);

	*y = roi_y;
	*h = roi_h;
}

static int lcm_set_spr_cmdq(void *dsi, struct drm_panel *panel, dcs_grp_write_gce cb,
	void *handle, unsigned int en)
{
	struct mtk_panel_para_table *panel_spr_switch_cmd;
	enum dsi_cmd_id cmd_id = DSI_CMD_ID_MAX;
	unsigned int cmd_count = 0;
	int i = 0;

	OPLUS_DSI_DEBUG("en=%X\n", en);
	/* ddic spr off */
	if (en == 0xFEFE) {
		panel_spr_enable = false;
		return -1;
	}
	/* ddic spr on */
	if (en == 0xEEEE) {
		panel_spr_enable = true;
		return -1;
	}
	if (!cb)
		return -1;
	if (!handle)
		return -1;

	if (en) {
		cmd_id = DSI_CMD_PANEL_SPR_ON;
	} else {
		cmd_id = DSI_CMD_PANEL_SPR_OFF;
	}

	cmd_count = lcm_all_cmd_table[cmd_id].cmd_lines;
	if (!cmd_count) {
		OPLUS_DSI_WARN("Invalid dsi_cmd[%d]: %s\n", cmd_id, dsi_cmd_map[cmd_id]);
		return -EINVAL;
	}

	panel_spr_switch_cmd = kzalloc(sizeof(struct mtk_panel_para_table) * cmd_count, GFP_KERNEL);
	if (!panel_spr_switch_cmd) {
		OPLUS_DSI_ERR("kzalloc failed\n");
		return -ENOMEM;
	}

	for (i = 0; i < cmd_count; i++) {
		panel_spr_switch_cmd[i].count = lcm_all_cmd_table[cmd_id].para_table[i].count;
		memcpy(panel_spr_switch_cmd[i].para_list,
				lcm_all_cmd_table[cmd_id].para_table[i].para_list,
				MTK_PANEL_CMD_PARAMS_MAX);
	}

	OPLUS_DSI_DEBUG("dcs_grp_write_gce send dsi_cmd[%d]: %s\n", cmd_id, dsi_cmd_map[cmd_id]);
	cb(dsi, handle, panel_spr_switch_cmd, cmd_count);

	if (panel_spr_switch_cmd)
		kfree(panel_spr_switch_cmd);

	return 0;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.panel_poweron = lcm_panel_poweron,
	.panel_poweroff = lcm_panel_poweroff,
	.panel_reset = lcm_panel_reset,
	.ata_check = panel_ata_check,
	.ext_param_get = mtk_panel_ext_param_get,
	.ext_param_set = mtk_panel_ext_param_set,
	.get_res_switch_type = mtk_get_res_switch_type,
	.scaling_mode_mapping = mtk_scaling_mode_mapping,
	.mode_switch_hs = mode_switch_hs,
	.set_spr_cmdq = lcm_set_spr_cmdq,
#ifdef OPLUS_FEATURE_DISPLAY_ADFR
	.set_minfps = panel_set_minfps,
	.set_multite = panel_set_multite,
#endif
#ifdef OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT
	.oplus_set_hbm = lcm_set_hbm,
	.oplus_hbm_set_cmdq = panel_hbm_set_cmdq,
	.oplus_ofp_set_lhbm_pressed_icon = oplus_ofp_set_lhbm_pressed_icon,
	.oplus_doze_disable = panel_doze_disable,
	.oplus_doze_enable = panel_doze_enable,
	.oplus_set_aod_light_mode = panel_set_aod_light_mode,
	.oplus_set_ultra_low_power_aod = panel_set_ultra_low_power_aod,
#endif /* OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT */
#ifdef OPLUS_FEATURE_DISPLAY_HPWM
	.lcm_high_pwm_set_pulse = oplus_panel_set_pwm_pulse,
#endif /* OPLUS_FEATURE_DISPLAY_HPWM */
#ifdef OPLUS_FEATURE_DISPLAY
	.lcm_set_hbm_max = oplus_display_panel_set_hbm_max,
	.set_seed = panel_set_seed,
	.oplus_set_backlight_cmdq = oplus_panel_set_backlight_cmdq,
#endif
};
#endif

static int lcm_get_modes(struct drm_panel *panel,
					struct drm_connector *connector)
{
	struct dsi_panel_lcm *ctx = panel_to_lcm(panel);
	int mode_count = ctx->mode_num * (ctx->res_switch ? ctx->res_switch : 1);
	struct drm_display_mode **mode = kzalloc(sizeof(struct drm_display_mode*) * mode_count, GFP_KERNEL);
	int i = 0;

	mode[0] = drm_mode_duplicate(connector->dev, &(ctx->display_modes[0]));
	if (!mode[0]) {
		OPLUS_DSI_ERR("failed to add mode %ux%ux@%u\n",
				ctx->display_modes[0].hdisplay, ctx->display_modes[0].vdisplay,
				 drm_mode_vrefresh(&(ctx->display_modes[0])));
		return -ENOMEM;
	}

	drm_mode_set_name(mode[0]);
	mode[0]->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode[0]);

	for (i = 1; i < mode_count; i++) {
		mode[i] = drm_mode_duplicate(connector->dev, &(ctx->display_modes[i]));
		if (!mode[i]) {
			OPLUS_DSI_ERR("not enough memory\n");
			return -ENOMEM;
		}

		drm_mode_set_name(mode[i]);
		mode[i]->type = DRM_MODE_TYPE_DRIVER;
		drm_mode_probed_add(connector, mode[i]);
	}

	connector->display_info.width_mm = ctx->ext_params_all[0].physical_width_um / 1000;
	connector->display_info.height_mm = ctx->ext_params_all[0].physical_height_um / 1000;
	kfree(mode);
	return 1;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

static void lcm_remove(struct mipi_dsi_device *dsi);
extern int is_ac180;
static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct dsi_panel_lcm *ctx = NULL;
	struct device_node *backlight;
	int ret = 0;

	OPLUS_DSI_INFO("probe Start\n");
	ctx = devm_kzalloc(dev, sizeof(struct dsi_panel_lcm), GFP_KERNEL);
	if (!ctx) {
		OPLUS_DSI_ERR("skip probe due to ctx devm_kzalloc error\n");
		return -ENOMEM;
	}
	oplus_display0_params = ctx;
	ctx->is_primary = true;
	ret = oplus_dsi_display_init(dsi, ctx);
	if (ret < 0) {
		OPLUS_DSI_ERR("skip probe due to oplus_dsi_display_init error\n");
		ret = -ENODEV;
		goto error;
	}
	mode_num_cur = ctx->mode_num;
	frame_height = ctx->display_modes[0].vdisplay;

	if (ctx->res_switch > 0) {
		res_switch_type = (enum RES_SWITCH_TYPE)(ctx->res_switch);
	}

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight) {
			ret = -EPROBE_DEFER;
			goto error;
		}
	}
	ret = oplus_dsi_panel_power_on(ctx);
	if (ret < 0) {
		OPLUS_DSI_ERR("skip probe due to oplus_dsi_panel_power_on error\n");
		ret = -ENODEV;
		goto error;
	}
	ctx->prepared = true;
	ctx->enabled = true;
	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

	mutex_init(&oplus_pcp_lock);

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	if (strcmp("panel_ac180_p_3_a0020_dsi_cmd", panel_name) == 0) {
		ext_funcs.lcm_valid_roi = lcm_valid_roi;
		is_ac180 = 1;
	}
	if (oplus_display0_params->mode_switch_h2l_delay) {
		ext_funcs.update_time = oplus_update_time;
	}
	if (oplus_display0_params->partial_update_enable) {
		ext_funcs.lcm_update_roi_cmdq = lcm_update_roi_cmdq;
	}
	ret = mtk_panel_ext_create(dev, ctx->ext_params_all, &ext_funcs, &ctx->panel);
	if (ret < 0)
		goto error_rm;
#endif
	OPLUS_DSI_INFO("probe vendor = %s manufacture = %s\n", ctx->panel_info.vendor, ctx->panel_info.manufacture);
	register_device_proc("lcd", ctx->panel_info.vendor, ctx->panel_info.manufacture);

	oplus_max_normal_brightness = oplus_display0_params->backlight_info.oplus_brightness_normal_max;
	oplus_max_brightness = oplus_display0_params->backlight_info.oplus_brightness_hw_max;
	oplus_display_brightness = oplus_display0_params->backlight_info.oplus_brightness_default;
	apollo_set_brightness_for_show(oplus_display_brightness);
	if (!oplus_max_brightness) {
		ret = -EPROBE_DEFER;
		goto error_rmproc;
	}
	OPLUS_DSI_INFO("probe End\n");

	return ret;

error_rmproc:
	remove_proc_entry("devinfo/lcd", NULL);
error_rm:
	lcm_remove(dsi);
error:
	oplus_dsi_display_deinit(dsi, ctx);
	return ret;
}
static void lcm_remove(struct mipi_dsi_device *dsi)
{
	struct dsi_panel_lcm *ctx = mipi_dsi_get_drvdata(dsi);
#if defined(CONFIG_MTK_PANEL_EXT)
	struct mtk_panel_ctx *ext_ctx = find_panel_ctx(&ctx->panel);
#endif

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_detach(ext_ctx);
	mtk_panel_remove(ext_ctx);
#endif
	mutex_destroy(&oplus_pcp_lock);
}

static const struct of_device_id lcm_of_match[] = {
	{
		.compatible = MATCH_NAME,
	},
	{}
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = panel_name,
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

static int check_cmdline_valid(void)
{
	int ret = 0;

	if (!lcm_driver.driver.name) {
		return 0;
	}
	ret = strcmp("", lcm_driver.driver.name);
	if (!ret) {
		return 0;
	} else {
		return 1;
	}
}

static int __init lcm_drv_init(void)
{
	int ret = 0;
	char *str = NULL;

	OPLUS_DSI_INFO("Start\n");
	str = strnstr(oplus_display0_cmdline, ":", strlen(oplus_display0_cmdline));
	if (str) {
		strncpy(panel_name, oplus_display0_cmdline, str - oplus_display0_cmdline);
	}
	if (!check_cmdline_valid()) {
		OPLUS_DSI_WARN("Invalid display cmdline, will use the old driver architecture\n");
		return 0;
	}
	OPLUS_DSI_INFO("Start panel_name = %s\n", lcm_driver.driver.name);

	mtk_panel_lock();
	ret = mipi_dsi_driver_register(&lcm_driver);
	if (ret < 0)
		OPLUS_DSI_INFO("Failed to register panel driver: %d\n", ret);

	mtk_panel_unlock();
	OPLUS_DSI_INFO("End, ret=%d\n", ret);

	return 0;
}

static void __exit lcm_drv_exit(void)
{
	OPLUS_DSI_INFO("Start\n");
	if (!check_cmdline_valid()) {
		OPLUS_DSI_WARN("Used old driver architecture and return\n");
		return;
	}
	mtk_panel_lock();
	mipi_dsi_driver_unregister(&lcm_driver);
	mtk_panel_unlock();
	OPLUS_DSI_INFO("End\n");
}
module_init(lcm_drv_init);
module_exit(lcm_drv_exit);

MODULE_AUTHOR("Oplus Display");
MODULE_DESCRIPTION("OPLUS Panel Driver");
MODULE_LICENSE("GPL v2");
