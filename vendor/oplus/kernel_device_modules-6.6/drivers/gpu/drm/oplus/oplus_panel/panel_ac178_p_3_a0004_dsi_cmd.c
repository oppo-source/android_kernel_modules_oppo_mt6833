/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : panel_ac178_p_3_a0004_dsi_cmd.c
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

#define CONFIG_MTK_PANEL_EXT
#include "mtk_panel_ext.h"
#include "mtk_drm_graphics_base.h"
#include "mtk_boot_common.h"
#include "mtk_dsi.h"
#ifdef OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT
#include "oplus_display_onscreenfingerprint.h"
#endif /* OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT */
#include "mtk-cmdq-ext.h"

//#include "../../../misc/mediatek/lpm/inc/lpm_module.h"
//#include "../../../misc/mediatek/lpm/modules/include/mt6897/pwr_ctrl.h"
#include "panel_ac178_p_3_a0004_dsi_cmd.h"
#ifdef OPLUS_FEATURE_DISPLAY_ADFR
#include "oplus_adfr.h"
#endif /* OPLUS_FEATURE_DISPLAY_ADFR */

#include "roundcorner_ac178_p_3_a0004_dsi_cmd.h"

#define BRIGHTNESS_MAX    4094
#define BRIGHTNESS_HALF   2047
#define MAX_NORMAL_BRIGHTNESS   3515
#define LCM_BRIGHTNESS_TYPE 2
#define FHD_LCM_WIDTH 1080
#define FHD_LCM_HEIGHT 2412

#define SILKY_MAX_NORMAL_BRIGHTNESS   8191
extern unsigned int silence_mode;
extern unsigned int last_backlight;
extern unsigned int oplus_display_brightness;
extern unsigned int oplus_max_normal_brightness;
extern unsigned int oplus_max_brightness;
extern unsigned int oplus_enhance_mipi_strength;

#ifdef OPLUS_FEATURE_DISPLAY
extern int oplus_panel_parse(struct device_node *node, bool is_primary);
#endif /* OPLUS_FEATURE_DISPLAY */
extern void lcdinfo_notify(unsigned long val, void *v);
static bool panel_power_on = false;
static int panel_send_pack_hs_cmd(void *dsi, struct LCM_setting_table *table, unsigned int lcm_cmd_count, dcs_write_gce_pack cb, void *handle);

int lcm_id2 = 0;

struct lcm_pmic_info {
	struct regulator *reg_vufs18;
	struct regulator *reg_vmch3p0;
};

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_pos, *bias_neg;
	struct gpio_desc *bias_gpio;
	struct gpio_desc *vddr1p2_enable_gpio;
	struct gpio_desc *vddr_aod_enable_gpio;
	struct gpio_desc *vci_enable_gpio;
	struct gpio_desc *vddi_enable_gpio;
	struct drm_display_mode *m;
	struct gpio_desc *te_switch_gpio,*te_out_gpio;
	bool prepared;
	bool enabled;
	int error;
};

#define lcm_dcs_write_seq(ctx, seq...)                                         \
	({                                                                     \
		const u8 d[] = { seq };                                        \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 128,                          \
				 "DCS sequence too big for stack");            \
		lcm_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

#define lcm_dcs_write_seq_static(ctx, seq...)                                  \
	({                                                                     \
		static const u8 d[] = { seq };                                 \
		lcm_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

static inline struct lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct lcm, panel);
}

#ifdef PANEL_SUPPORT_READBACK
static int lcm_dcs_read(struct lcm *ctx, u8 cmd, void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return 0;

	ret = mipi_dsi_dcs_read(dsi, cmd, data, len);
	if (ret < 0) {
		pr_err("error %d reading dcs seq:(%#x)\n", ret,
			 cmd);
		ctx->error = ret;
	}

	return ret;
}

static void lcm_panel_get_data(struct lcm *ctx)
{
	u8 buffer[3] = { 0 };
	static int ret;

	pr_info("+\n");

	if (ret == 0) {
		ret = lcm_dcs_read(ctx, 0x0A, buffer, 1);
		pr_info("0x%08x\n",buffer[0] | (buffer[1] << 8));
		pr_info("return %d data(0x%08x) to dsi engine\n",
			ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static void lcm_dcs_write(struct lcm *ctx, const void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;
	char *addr;

	if (ctx->error < 0)
		return;

	addr = (char *)data;

	if ((int)*addr < 0xB0)
		ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
	else
		ret = mipi_dsi_generic_write(dsi, data, len);

	if (ret < 0) {
		pr_err("error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
}

// static struct regulator *vrf12_ldo;
// static int lcm_panel_1p2_ldo_regulator_init(struct device *dev)
// {
		// static int regulator_1p2_inited;
		// int ret = 0;

		// if (regulator_1p2_inited)
				// return ret;

		// /* please only get regulator once in a driver */
		// vrf12_ldo = devm_regulator_get(dev, "1p2");
		// if (IS_ERR_OR_NULL(vrf12_ldo)) { /* handle return value */
				// ret = PTR_ERR(vrf12_ldo);
				// pr_err("get vrf12_ldo fail, error: %d\n", ret);
		// }
		// regulator_1p2_inited = 1;
		// pr_info("get lcm_panel_1p2_ldo_regulator_init\n");
		// return ret; /* must be 0 */

// }

// static int lcm_panel_1p2_ldo_enable(struct device *dev)
// {
		// int ret = 0;
		// int retval = 0;

		// lcm_panel_1p2_ldo_regulator_init(dev);

		// /* set voltage with min & max*/
	// if (!IS_ERR_OR_NULL(vrf12_ldo)) {
		// ret = regulator_set_voltage(vrf12_ldo, 1200000, 1200000);
		// if (ret < 0)
			// pr_err("set voltage vrf12_ldo fail, ret = %d\n", ret);
		// retval |= ret;
	// }
		// /* enable regulator */
	// if (!IS_ERR_OR_NULL(vrf12_ldo)) {
		// ret = regulator_enable(vrf12_ldo);
		// if (ret < 0)
			// pr_err("enable regulator vrf12_ldo fail, ret = %d\n", ret);
		// retval |= ret;
	// }
	// pr_info("get lcm_panel_1p2_ldo_enable\n");

		// return retval;
// }

// static int lcm_panel_1p2_ldo_disable(struct device *dev)
// {
	// int ret = 0;
	// int retval = 0;

	// lcm_panel_1p2_ldo_regulator_init(dev);

	// if (!IS_ERR_OR_NULL(vrf12_ldo)) {
		// ret = regulator_disable(vrf12_ldo);
		// if (ret < 0)
			// pr_err("disable regulator vrf12_ldo fail, ret = %d\n", ret);
		// retval |= ret;
	// }
	// return ret;
// }

static struct regulator *vrf18_ldo;
static int lcm_panel_1p8_ldo_regulator_init(struct device *dev)
{
		static int regulator_1p8_inited;
		int ret = 0;

		if (regulator_1p8_inited)
				return ret;

		/* please only get regulator once in a driver */
		vrf18_ldo = devm_regulator_get(dev, "1p8");
		if (IS_ERR_OR_NULL(vrf18_ldo)) { /* handle return value */
				ret = PTR_ERR(vrf18_ldo);
				pr_err("get vrf18_ldo fail, error: %d\n", ret);
		}
		regulator_1p8_inited = 1;
		pr_info("get lcm_panel_1p8_ldo_regulator_init\n");
		return ret; /* must be 0 */

}

static int lcm_panel_1p8_ldo_enable(struct device *dev)
{
		int ret = 0;
		int retval = 0;

		lcm_panel_1p8_ldo_regulator_init(dev);

		/* set voltage with min & max*/
	if (!IS_ERR_OR_NULL(vrf18_ldo)) {
		ret = regulator_set_voltage(vrf18_ldo, 1800000, 1800000);
		if (ret < 0)
			pr_err("set voltage vrf18_ldo fail, ret = %d\n", ret);
		retval |= ret;
	}
		/* enable regulator */
	if (!IS_ERR_OR_NULL(vrf18_ldo)) {
		ret = regulator_enable(vrf18_ldo);
		if (ret < 0)
			pr_err("enable regulator vrf18_ldo fail, ret = %d\n", ret);
		retval |= ret;
	}
	pr_info("get lcm_panel_1p8_ldo_enable\n");

		return retval;
}

static int lcm_panel_1p8_ldo_disable(struct device *dev)
{
	int ret = 0;
	int retval = 0;

	lcm_panel_1p8_ldo_regulator_init(dev);

	if (!IS_ERR_OR_NULL(vrf18_ldo)) {
		ret = regulator_disable(vrf18_ldo);
		if (ret < 0)
			pr_err("disable regulator vrf18_ldo fail, ret = %d\n", ret);
		retval |= ret;
	}
	return retval;
}

static struct regulator *wl2868c_ldo;
static int lcm_panel_wl2868c_ldo_regulator_init(struct device *dev)
{
		static int regulator_wl2868c_inited;
		int ret = 0;

		if (regulator_wl2868c_inited)
				return ret;
		pr_info("get lcm_panel_wl2868c_ldo_regulator_init\n");

		/* please only get regulator once in a driver */
		wl2868c_ldo = devm_regulator_get(dev, "3p0");
		if (IS_ERR_OR_NULL(wl2868c_ldo)) { /* handle return value */
				ret = PTR_ERR(wl2868c_ldo);
				pr_err("get wl2868c_ldo fail, error: %d\n", ret);
		}
		regulator_wl2868c_inited = 1;
		return ret; /* must be 0 */

}

static int lcm_panel_wl2868c_ldo_enable(struct device *dev)
{
		int ret = 0;
		int retval = 0;

		lcm_panel_wl2868c_ldo_regulator_init(dev);

		/* set voltage with min & max*/
	if (!IS_ERR_OR_NULL(wl2868c_ldo)) {
		ret = regulator_set_voltage(wl2868c_ldo, 3000000, 3000000);
		if (ret < 0)
			pr_err("set voltage wl2868c_ldo fail, ret = %d\n", ret);
		retval |= ret;
	}
		/* enable regulator */
	if (!IS_ERR_OR_NULL(wl2868c_ldo)) {
		ret = regulator_enable(wl2868c_ldo);
		if (ret < 0)
			pr_err("enable regulator wl2868c_ldo fail, ret = %d\n", ret);
		retval |= ret;
	}
	pr_info("get lcm_panel_wl2868c_ldo_enable\n");

		return retval;
}

static int lcm_panel_wl2868c_ldo_disable(struct device *dev)
{
	int ret = 0;
	int retval = 0;

	lcm_panel_wl2868c_ldo_regulator_init(dev);

	if (!IS_ERR_OR_NULL(wl2868c_ldo)) {
		ret = regulator_disable(wl2868c_ldo);
		if (ret < 0)
			pr_err("disable regulator wl2868c_ldo fail, ret = %d\n", ret);
		retval |= ret;
	}
	return retval;
}

static void push_table(struct lcm *ctx, struct LCM_setting_table *table, unsigned int count)
{
	unsigned int i;
	unsigned int cmd;

	for (i = 0; i < count; i++) {
		cmd = table[i].cmd;
		switch (cmd) {
		case REGFLAG_DELAY:
			usleep_range(table[i].count*1000, table[i].count*1000 + 100);
			break;
		case REGFLAG_UDELAY:
			usleep_range(table[i].count, table[i].count + 100);
			break;
		case REGFLAG_END_OF_TABLE:
			break;
		default:
			lcm_dcs_write(ctx, table[i].para_list, table[i].count);
			break;
		}
	}
}

static int panel_send_pack_hs_cmd(void *dsi, struct LCM_setting_table *table, unsigned int lcm_cmd_count, dcs_write_gce_pack cb, void *handle)
{
	unsigned int i = 0;
	struct mtk_ddic_dsi_cmd send_cmd_to_ddic;

	if (lcm_cmd_count > MAX_TX_CMD_NUM_PACK) {
		pr_err("out of mtk_ddic_dsi_cmd \n");
		return 0;
	}

	for (i = 0; i < lcm_cmd_count; i++) {
		send_cmd_to_ddic.mtk_ddic_cmd_table[i].cmd_num = table[i].count;
		send_cmd_to_ddic.mtk_ddic_cmd_table[i].para_list = table[i].para_list;
	}
	send_cmd_to_ddic.is_hs = 1;
	send_cmd_to_ddic.is_package = 1;
	send_cmd_to_ddic.cmd_count = lcm_cmd_count;
	cb(dsi, handle, &send_cmd_to_ddic);

	return 0;
}

static int inline get_panel_es_ver(void)
{
	int ret = 0;
	if (lcm_id2 > 0 && lcm_id2 <=1) {
		ret = ES_EVB;
	} else if (lcm_id2 == 2) {
		ret = ES_T0;
	} else if (lcm_id2 == 3) {
		ret = ES_EVT;
	} else if (lcm_id2 == 4) {
		ret = ES_DVT;
	} else if (lcm_id2 >= 5) {
		ret = ES_PVT;
	} else {
		ret = ES_EVB;
	}
	return ret;
}

static int get_mode_enum(struct drm_display_mode *m)
{
	int ret = 0;
	int m_vrefresh = 0;

	if (m == NULL) {
		return -EINVAL;
	}

		m_vrefresh = drm_mode_vrefresh(m);

		if (m_vrefresh == 60) {
			ret = FHD_SDC60;
		} else if (m_vrefresh == 90) {
			ret = FHD_SDC90;
		} else if (m_vrefresh == 120) {
				ret = FHD_SDC120;
		} else {
			ret = FHD_SDC60;
		}
	return ret;
}

static void lcm_panel_init(struct lcm *ctx)
{
	int mode_id = -1;
	struct drm_display_mode *m = ctx->m;

	mode_id = get_mode_enum(m);
	panel_power_on = true;
	switch (mode_id) {
	case FHD_SDC60:
		push_table(ctx, dsi_on_cmd_sdc60, sizeof(dsi_on_cmd_sdc60)/sizeof(struct LCM_setting_table));
		break;
	case FHD_SDC90:
		push_table(ctx, dsi_on_cmd_sdc90, sizeof(dsi_on_cmd_sdc90)/sizeof(struct LCM_setting_table));
		break;
		case FHD_SDC120:
				push_table(ctx, dsi_on_cmd_sdc120, sizeof(dsi_on_cmd_sdc120)/sizeof(struct LCM_setting_table));
				break;
		default:
				push_table(ctx, dsi_on_cmd_sdc60, sizeof(dsi_on_cmd_sdc60)/sizeof(struct LCM_setting_table));
				break;
		}
	pr_info("mode_id=%d\n", mode_id);
}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return 0;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;

	return 0;
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->prepared)
		return 0;

	lcm_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_OFF);
	/* Wait > 20ms,Actual 22ms */
	usleep_range(22000, 22100);
	lcm_dcs_write_seq_static(ctx, MIPI_DCS_ENTER_SLEEP_MODE);
	/* Wait > 120ms,Actual 125ms */
	usleep_range(125000, 125100);
	/* keep vcore off */
	//lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_SUSPEND_PWR_CTRL, MT_LPM_SMC_ACT_SET, PW_REG_SPM_VCORE_REQ, 0x0);
	//OPLUS_DSI_DEBUG(" call lpm_smc_spm_dbg keep vcore off for display off!\n");

	ctx->error = 0;
	ctx->prepared = false;
	pr_info("%s:success\n", __func__);

	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	if (ctx->prepared)
		return 0;

	lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;
#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif

	pr_info("%s:success\n", __func__);
	return ret;
}

static const struct drm_display_mode disp_mode_60Hz = {
		.clock = 223015,
		.hdisplay = 1264,
		.hsync_start = 1264 + 9,//HFP
		.hsync_end = 1264 + 9 + 2,//HSA
		.htotal = 1264 + 9 + 2 + 21,//HBP
		.vdisplay = 2780,
		.vsync_start = 2780 + 52,//VFP
		.vsync_end = 2780 + 52 + 14,//VSA
		.vtotal = 2780 + 52+ 14 + 22,//VBP
		.hskew = STANDARD_MFR,
};

static const struct drm_display_mode disp_mode_90Hz = {
		.clock = 334523,
		.hdisplay = 1264,
		.hsync_start = 1264 + 9,//HFP
		.hsync_end = 1264 + 9 + 2,//HSA
		.htotal = 1264 + 9 + 2 + 21,//HBP
		.vdisplay = 2780,
		.vsync_start = 2780 + 52,//VFP
		.vsync_end = 2780 + 52 + 14,//VSA
		.vtotal = 2780 + 52+ 14 + 22,//VBP
		.hskew = STANDARD_ADFR,
};

static const struct drm_display_mode disp_mode_120Hz = {
		.clock = 446031,
		.hdisplay = 1264,
		.hsync_start = 1264 + 9,//HFP
		.hsync_end = 1264 + 9 + 2,//HSA
		.htotal = 1264 + 9 + 2 + 21,//HBP
		.vdisplay = 2780,
		.vsync_start = 2780 + 52,//VFP
		.vsync_end = 2780 + 52 + 14,//VSA
		.vtotal = 2780 + 52+ 14 + 22,//VBP
		.hskew = STANDARD_ADFR,
};

static struct mtk_panel_params ext_params_60Hz = {
	.pll_clk = 556,
	.phy_timcon = {
		.hs_trail = 14,
		.clk_trail = 15,
	},
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C, .mask_list[0] = 0x9C,
	},
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 0,
	.corner_pattern_height = ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size = sizeof(top_rc_pattern),
	.corner_pattern_lt_addr = (void *)top_rc_pattern,
#endif
	.color_vivid_status = true,
	.color_srgb_status = true,
	.color_softiris_status = false,
	.color_dual_panel_status = false,
	.color_dual_brightness_status = true,
	.color_oplus_calibrate_status = true,
	.cmd_null_pkt_en = 1,
	.cmd_null_pkt_len = 0,
	//.skip_unnecessary_switch = true,
	.vendor = "ac178_A0004",
	.manufacture = "P_3",
	.panel_type = 0,
	.lane_swap_en = 0,
	.lane_swap[0][MIPITX_PHY_LANE_0] = MIPITX_PHY_LANE_0,
	.lane_swap[0][MIPITX_PHY_LANE_1] = MIPITX_PHY_LANE_1,
	.lane_swap[0][MIPITX_PHY_LANE_2] = MIPITX_PHY_LANE_3,
	.lane_swap[0][MIPITX_PHY_LANE_3] = MIPITX_PHY_LANE_2,
	.lane_swap[0][MIPITX_PHY_LANE_CK] = MIPITX_PHY_LANE_CK,
	.lane_swap[0][MIPITX_PHY_LANE_RX] = MIPITX_PHY_LANE_0,
	.lane_swap[1][MIPITX_PHY_LANE_0] = MIPITX_PHY_LANE_0,
	.lane_swap[1][MIPITX_PHY_LANE_1] = MIPITX_PHY_LANE_1,
	.lane_swap[1][MIPITX_PHY_LANE_2] = MIPITX_PHY_LANE_3,
	.lane_swap[1][MIPITX_PHY_LANE_3] = MIPITX_PHY_LANE_2,
	.lane_swap[1][MIPITX_PHY_LANE_CK] = MIPITX_PHY_LANE_CK,
	.lane_swap[1][MIPITX_PHY_LANE_RX] = MIPITX_PHY_LANE_0,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
			.enable = 1,
			.ver = 18,
			.slice_mode = 1,
			.rgb_swap = 0,
			.dsc_cfg = 40,
			.rct_on = 1,
			.bit_per_channel = 10,
			.dsc_line_buf_depth = 11,
			.bp_enable = 1,
			.bit_per_pixel = 128,
			.pic_height = 2780,
			.pic_width = 1264,
			.slice_height = 20,
			.slice_width = 632,
			.chunk_size = 632,
			.xmit_delay = 512,
			.dec_delay = 599,
			.scale_value = 32,
			.increment_interval = 504,
			.decrement_interval = 8,
			.line_bpg_offset = 13,
			.nfl_bpg_offset = 1402,
			.slice_bpg_offset = 1103,
			.initial_offset = 6144,
			.final_offset = 4320,
			.flatness_minqp = 7,
			.flatness_maxqp = 16,
			.rc_model_size = 8192,
			.rc_edge_factor = 6,
			.rc_quant_incr_limit0 = 15,
			.rc_quant_incr_limit1 = 15,
			.rc_tgt_offset_hi = 3,
			.rc_tgt_offset_lo = 3,
		},
	.data_rate = 1112,
	//.oplus_serial_para0 = 0x81,
#ifdef OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT
	.oplus_ofp_need_keep_apart_backlight = false,
	.oplus_ofp_hbm_on_delay = 0,
	.oplus_ofp_pre_hbm_off_delay = 2,
	.oplus_ofp_hbm_off_delay = 0,
	.oplus_ofp_need_to_sync_data_in_aod_unlocking = true,
	.oplus_ofp_aod_off_insert_black = 1,
	.oplus_ofp_aod_off_black_frame_total_time = 42,
#endif /* OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT */
 	.dyn_fps = {
		.switch_en = 1, .vact_timing_fps = 60,
		.apollo_limit_superior_us = 10000, .apollo_limit_inferior_us = 13596,
		.apollo_transfer_time_us = 8200,
	},
	.panel_bpp = 10,
};

static struct mtk_panel_params ext_params_90Hz = {
	.pll_clk = 414,
	.phy_timcon = {
		.hs_trail = 14,
		.clk_trail = 15,
	},
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C, .mask_list[0] = 0x9C,
	},
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 0,
	.corner_pattern_height = ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size = sizeof(top_rc_pattern),
	.corner_pattern_lt_addr = (void *)top_rc_pattern,
#endif
	.color_vivid_status = true,
	.color_srgb_status = true,
	.color_softiris_status = false,
	.color_dual_panel_status = false,
	.color_dual_brightness_status = true,
	.color_oplus_calibrate_status = true,
	.cmd_null_pkt_en = 1,
	.cmd_null_pkt_len = 0,
	//.skip_unnecessary_switch = true,
	.vendor = "ac178_A0004",
	.manufacture = "P_3",
	.panel_type = 0,
	.lane_swap_en = 0,
	.lane_swap[0][MIPITX_PHY_LANE_0] = MIPITX_PHY_LANE_0,
	.lane_swap[0][MIPITX_PHY_LANE_1] = MIPITX_PHY_LANE_1,
	.lane_swap[0][MIPITX_PHY_LANE_2] = MIPITX_PHY_LANE_3,
	.lane_swap[0][MIPITX_PHY_LANE_3] = MIPITX_PHY_LANE_2,
	.lane_swap[0][MIPITX_PHY_LANE_CK] = MIPITX_PHY_LANE_CK,
	.lane_swap[0][MIPITX_PHY_LANE_RX] = MIPITX_PHY_LANE_0,
	.lane_swap[1][MIPITX_PHY_LANE_0] = MIPITX_PHY_LANE_0,
	.lane_swap[1][MIPITX_PHY_LANE_1] = MIPITX_PHY_LANE_1,
	.lane_swap[1][MIPITX_PHY_LANE_2] = MIPITX_PHY_LANE_3,
	.lane_swap[1][MIPITX_PHY_LANE_3] = MIPITX_PHY_LANE_2,
	.lane_swap[1][MIPITX_PHY_LANE_CK] = MIPITX_PHY_LANE_CK,
	.lane_swap[1][MIPITX_PHY_LANE_RX] = MIPITX_PHY_LANE_0,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
			.enable = 1,
			.ver = 18,
			.slice_mode = 1,
			.rgb_swap = 0,
			.dsc_cfg = 40,
			.rct_on = 1,
			.bit_per_channel = 10,
			.dsc_line_buf_depth = 11,
			.bp_enable = 1,
			.bit_per_pixel = 128,
			.pic_height = 2780,
			.pic_width = 1264,
			.slice_height = 20,
			.slice_width = 632,
			.chunk_size = 632,
			.xmit_delay = 512,
			.dec_delay = 599,
			.scale_value = 32,
			.increment_interval = 504,
			.decrement_interval = 8,
			.line_bpg_offset = 13,
			.nfl_bpg_offset = 1402,
			.slice_bpg_offset = 1103,
			.initial_offset = 6144,
			.final_offset = 4320,
			.flatness_minqp = 7,
			.flatness_maxqp = 16,
			.rc_model_size = 8192,
			.rc_edge_factor = 6,
			.rc_quant_incr_limit0 = 15,
			.rc_quant_incr_limit1 = 15,
			.rc_tgt_offset_hi = 3,
			.rc_tgt_offset_lo = 3,
		},
	.data_rate = 828,
	//.oplus_serial_para0 = 0x81,
#ifdef OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT
	.oplus_ofp_need_keep_apart_backlight = false,
	.oplus_ofp_hbm_on_delay = 0,
	.oplus_ofp_pre_hbm_off_delay = 2,
	.oplus_ofp_hbm_off_delay = 0,
	.oplus_ofp_need_to_sync_data_in_aod_unlocking = true,
	.oplus_ofp_aod_off_insert_black = 1,
	.oplus_ofp_aod_off_black_frame_total_time = 42,
#endif /* OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT */
	 .dyn_fps = {
		.switch_en = 1, .vact_timing_fps = 90,
	.apollo_limit_superior_us = 2910, .apollo_limit_inferior_us = 10000,
	.apollo_transfer_time_us = 8400,
	},
	.panel_bpp = 10,
};

static struct mtk_panel_params ext_params_120Hz = {
	.pll_clk = 556,
	.phy_timcon = {
		.hs_trail = 14,
		.clk_trail = 15,
	},
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C, .mask_list[0] = 0x9C,
	},
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 0,
	.corner_pattern_height = ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size = sizeof(top_rc_pattern),
	.corner_pattern_lt_addr = (void *)top_rc_pattern,
#endif
	.color_vivid_status = true,
	.color_srgb_status = true,
	.color_softiris_status = false,
	.color_dual_panel_status = false,
	.color_dual_brightness_status = true,
	.color_oplus_calibrate_status = true,
	.cmd_null_pkt_en = 1,
	.cmd_null_pkt_len = 0,
	//.skip_unnecessary_switch = true,
	.vendor = "ac178_A0004",
	.manufacture = "P_3",
	.panel_type = 0,
	.lane_swap_en = 0,
	.lane_swap[0][MIPITX_PHY_LANE_0] = MIPITX_PHY_LANE_0,
	.lane_swap[0][MIPITX_PHY_LANE_1] = MIPITX_PHY_LANE_1,
	.lane_swap[0][MIPITX_PHY_LANE_2] = MIPITX_PHY_LANE_3,
	.lane_swap[0][MIPITX_PHY_LANE_3] = MIPITX_PHY_LANE_2,
	.lane_swap[0][MIPITX_PHY_LANE_CK] = MIPITX_PHY_LANE_CK,
	.lane_swap[0][MIPITX_PHY_LANE_RX] = MIPITX_PHY_LANE_0,
	.lane_swap[1][MIPITX_PHY_LANE_0] = MIPITX_PHY_LANE_0,
	.lane_swap[1][MIPITX_PHY_LANE_1] = MIPITX_PHY_LANE_1,
	.lane_swap[1][MIPITX_PHY_LANE_2] = MIPITX_PHY_LANE_3,
	.lane_swap[1][MIPITX_PHY_LANE_3] = MIPITX_PHY_LANE_2,
	.lane_swap[1][MIPITX_PHY_LANE_CK] = MIPITX_PHY_LANE_CK,
	.lane_swap[1][MIPITX_PHY_LANE_RX] = MIPITX_PHY_LANE_0,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
			.enable = 1,
			.ver = 18,
			.slice_mode = 1,
			.rgb_swap = 0,
			.dsc_cfg = 40,
			.rct_on = 1,
			.bit_per_channel = 10,
			.dsc_line_buf_depth = 11,
			.bp_enable = 1,
			.bit_per_pixel = 128,
			.pic_height = 2780,
			.pic_width = 1264,
			.slice_height = 20,
			.slice_width = 632,
			.chunk_size = 632,
			.xmit_delay = 512,
			.dec_delay = 599,
			.scale_value = 32,
			.increment_interval = 504,
			.decrement_interval = 8,
			.line_bpg_offset = 13,
			.nfl_bpg_offset = 1402,
			.slice_bpg_offset = 1103,
			.initial_offset = 6144,
			.final_offset = 4320,
			.flatness_minqp = 7,
			.flatness_maxqp = 16,
			.rc_model_size = 8192,
			.rc_edge_factor = 6,
			.rc_quant_incr_limit0 = 15,
			.rc_quant_incr_limit1 = 15,
			.rc_tgt_offset_hi = 3,
			.rc_tgt_offset_lo = 3,
		},
	.data_rate = 1112,
	//.oplus_serial_para0 = 0x81,
#ifdef OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT
	.oplus_ofp_need_keep_apart_backlight = false,
	.oplus_ofp_hbm_on_delay = 0,
	.oplus_ofp_pre_hbm_off_delay = 2,
	.oplus_ofp_hbm_off_delay = 0,
	.oplus_ofp_need_to_sync_data_in_aod_unlocking = true,
	.oplus_ofp_aod_off_insert_black = 1,
	.oplus_ofp_aod_off_black_frame_total_time = 42,
#endif /* OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT */
	 .dyn_fps = {
		.switch_en = 1, .vact_timing_fps = 120,
	.apollo_limit_superior_us = 0, .apollo_limit_inferior_us = 6898,
	.apollo_transfer_time_us = 6200,
	},
	.panel_bpp = 10,
};

static int panel_ata_check(struct drm_panel *panel)
{
	/* Customer test by own ATA tool */
	return 1;
}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb, void *handle, unsigned int level)
{
	int i = 0;

	if (!dsi || !cb) {
		return -EINVAL;
	}

	if (level == 1) {
		pr_info("enter aod\n");
		return 0;
	} else if (level > 4094) {
		level = 4094;
	}

	if (panel_power_on == true || level == 0) {
		pr_info("[INFO][%s:%d]backlight lvl:%u.\n", __func__, __LINE__, level);
		panel_power_on = false;
	} else {
		pr_info("[INFO][%s:%d]backlight lvl:%u.\n", __func__, __LINE__, level);
	}

	// if (get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT && level > 0) {
		// level = 2047;
	// }

	dsi_set_backlight[1].para_list[1] = level >> 8;
	dsi_set_backlight[1].para_list[2] = level & 0xFF;

	for (i = 0; i < sizeof(dsi_set_backlight)/sizeof(struct LCM_setting_table); i++) {
		cb(dsi, handle, dsi_set_backlight[i].para_list, dsi_set_backlight[i].count);
	}
	lcdinfo_notify(LCM_BRIGHTNESS_TYPE, &level);
	oplus_display_brightness = level;
	return 0;
}

static int oplus_display_panel_set_pwm_pulse_bl(void *dsi, dcs_write_gce_pack cb, void *handle, unsigned int level)
{
	if (!dsi || !cb) {
		return -EINVAL;
	}

	if (last_backlight == 0 || level == 0) {
		pr_info("backlight level:%u\n", level);
	} else {
		pr_info("backlight level:%u\n", level);
	}

	if (level > 4095) {
		level = 4095;
	}

	if (get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT && level > 0){
		level = BRIGHTNESS_HALF;
	}

	if (silence_mode) {
		pr_info("pwm_turbo silence_mode is %d, set backlight to 0\n", silence_mode);
		level = 0;
	}

	last_backlight = level;
	lcdinfo_notify(LCM_BRIGHTNESS_TYPE, &last_backlight);

	if (level == 1) {
		pr_info("filter backlight %u setting\n", level);
		return 0;
	}

	dsi_set_backlight[1].para_list[1] = level >> 8;
	dsi_set_backlight[1].para_list[2] = level & 0xFF;
	panel_send_pack_hs_cmd(dsi, dsi_set_backlight, sizeof(dsi_set_backlight) / sizeof(struct LCM_setting_table), cb, handle);

	return 0;
}

// static int oplus_display_panel_set_hbm_max(void *dsi, dcs_write_gce_pack cb, void *handle, unsigned int en)
// {
	// unsigned int lcm_cmd_count = 0;
	// unsigned int level = oplus_display_brightness;

	// if (!dsi || !cb) {
		// pr_err("Invalid params\n");
		// return -EINVAL;
	// }

	// if (en) {
		// lcm_cmd_count = sizeof(dsi_switch_hbm_apl_on) / sizeof(struct LCM_setting_table);
		// panel_send_pack_hs_cmd(dsi, dsi_switch_hbm_apl_on, lcm_cmd_count, cb, handle);
		// OPLUS_DSI_INFO("Enter hbm max APL mode\n");
	// } else if (!en) {
		// lcm_cmd_count = sizeof(dsi_switch_hbm_apl_off) / sizeof(struct LCM_setting_table);
		// dsi_switch_hbm_apl_off[lcm_cmd_count-1].para_list[1] = level >> 8;
		// dsi_switch_hbm_apl_off[lcm_cmd_count-1].para_list[2] = level & 0xFF;
		// panel_send_pack_hs_cmd(dsi, dsi_switch_hbm_apl_off, lcm_cmd_count, cb, handle);
		// OPLUS_DSI_INFO("hbm_max APL off, restore backlight:%d\n", level);
	// }

	// return 0;
// }

#ifdef OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT
static int lcm_set_hbm(void *dsi, dcs_write_gce_pack cb,
		void *handle, unsigned int hbm_mode)
{
	unsigned int lcm_cmd_count = 0;
	OFP_DEBUG("start\n");

	if (!dsi || !cb) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	if(hbm_mode == 1) {
		lcm_cmd_count = sizeof(hbm_on_cmd_60hz) / sizeof(struct LCM_setting_table);
		panel_send_pack_hs_cmd(dsi, hbm_on_cmd_60hz, lcm_cmd_count, cb, handle);
		last_backlight = 4095;
		OFP_INFO("Enter hbm mode, set last_backlight as %d", last_backlight);
		lcdinfo_notify(1, &hbm_mode);
	} else if (hbm_mode == 0) {
		lcm_cmd_count = sizeof(hbm_off_cmd_60hz) / sizeof(struct LCM_setting_table);
		panel_send_pack_hs_cmd(dsi, hbm_off_cmd_60hz, lcm_cmd_count, cb, handle);
		lcdinfo_notify(1, &hbm_mode);
		oplus_display_panel_set_pwm_pulse_bl(dsi, cb, handle, oplus_display_brightness);
	}

	OFP_DEBUG("end\n");

	return 0;
}

static int panel_hbm_set_cmdq(struct drm_panel *panel, void *dsi,
		dcs_write_gce_pack cb, void *handle, bool en)
{
	unsigned int reg_count = 0;
	unsigned int vrefresh_rate = 0;
	struct lcm *ctx = NULL;
	struct LCM_setting_table *hbm_cmd = NULL;

	OFP_DEBUG("start\n");

	if (!panel || !dsi || !cb) {
		OFP_ERR("Invalid input params\n");
		return -EINVAL;
	}

	ctx = panel_to_lcm(panel);
	if (!ctx) {
		OFP_ERR("Invalid lcm params\n");
	}

	if (!ctx->m) {
		vrefresh_rate = 120;
		OFP_INFO("default refresh rate is 120hz\n");
	} else {
		vrefresh_rate = drm_mode_vrefresh(ctx->m);
	}

	if (en) {
		last_backlight = 4095;
		OFP_INFO("Enter hbm mode, set last_backlight as %d", last_backlight);
	}

	if (vrefresh_rate == 60) {
		if (en) {
			hbm_cmd = hbm_on_cmd_60hz;
			reg_count = sizeof(hbm_on_cmd_60hz) / sizeof(struct LCM_setting_table);
		} else {
			hbm_cmd = hbm_off_cmd_60hz;
			reg_count = sizeof(hbm_off_cmd_60hz) / sizeof(struct LCM_setting_table);
		}
	} else if (vrefresh_rate == 90) {
		if (en) {
			hbm_cmd = hbm_on_cmd_90hz;
			reg_count = sizeof(hbm_on_cmd_90hz) / sizeof(struct LCM_setting_table);
		} else {
			hbm_cmd = hbm_off_cmd_90hz;
			reg_count = sizeof(hbm_off_cmd_90hz) / sizeof(struct LCM_setting_table);
		}
	} else if (vrefresh_rate == 120) {
		if (en) {
			hbm_cmd = hbm_on_cmd_120hz;
			reg_count = sizeof(hbm_on_cmd_120hz) / sizeof(struct LCM_setting_table);
		} else {
			hbm_cmd = hbm_off_cmd_120hz;
			reg_count = sizeof(hbm_off_cmd_120hz) / sizeof(struct LCM_setting_table);
		}
	}
	panel_send_pack_hs_cmd(dsi, hbm_cmd, reg_count, cb, handle);
	lcdinfo_notify(1, &en);

	if (!en) {
		oplus_display_panel_set_pwm_pulse_bl(dsi, cb, handle, oplus_display_brightness);
	}

	OFP_DEBUG("end\n");

	return 0;
}

static int oplus_ofp_set_lhbm_pressed_icon(struct drm_panel *panel, void *dsi_drv, dcs_write_gce_pack cb, void *handle, uint64_t lhbm_pressed_icon_on)
{
	unsigned int reg_count = 0;
	unsigned int vrefresh_rate = 0;
	struct lcm *ctx = NULL;
	struct LCM_setting_table *lhbm_pressed_icon_cmd = NULL;

	OFP_DEBUG("start\n");

	if (!oplus_ofp_local_hbm_is_enabled()) {
		OFP_DEBUG("local hbm is not enabled, should not set lhbm pressed icon\n");
	}

	if (!panel || !dsi_drv || !cb) {
		OFP_ERR("Invalid input params\n");
		return -EINVAL;
	}

	ctx = panel_to_lcm(panel);
	if (!ctx) {
		OFP_ERR("Invalid ctx params\n");
	}

	if (!ctx->m) {
		vrefresh_rate = 120;
		OFP_INFO("default refresh rate is 120hz\n");
	} else {
		vrefresh_rate = drm_mode_vrefresh(ctx->m);
	}

	OFP_INFO("lhbm_pressed_icon_on:%llu,bl_lvl:%u,refresh_rate:%u\n", lhbm_pressed_icon_on, oplus_display_brightness, vrefresh_rate);

	if (lhbm_pressed_icon_on) {
		lhbm_pressed_icon_cmd = lhbm_pressed_icon_on_cmd;
		reg_count = sizeof(lhbm_pressed_icon_on_cmd) / sizeof(struct LCM_setting_table);
	} else {
		lhbm_pressed_icon_cmd = lhbm_pressed_icon_off_cmd;
		reg_count = sizeof(lhbm_pressed_icon_off_cmd) / sizeof(struct LCM_setting_table);
	}

	panel_send_pack_hs_cmd(dsi_drv, lhbm_pressed_icon_cmd, reg_count, cb, handle);

	if (!lhbm_pressed_icon_on) {
		oplus_display_panel_set_pwm_pulse_bl(dsi_drv, cb, handle, oplus_display_brightness);
	} else {
		if (oplus_display_brightness > 0x0DBB) {
			OFP_INFO("set backlight level to 0x0DBB after pressed icon on\n");
			oplus_display_panel_set_pwm_pulse_bl(dsi_drv, cb, handle, 0x0DBB);
		}
	}

	OFP_DEBUG("end\n");

	return 0;
}

static int panel_doze_disable(struct drm_panel *panel, void *dsi, dcs_write_gce cb, void *handle)
{
	unsigned int i = 0;
	unsigned int reg_count = 0;
	int vrefresh_rate = 0;
	struct lcm *ctx = NULL;
	struct mtk_dsi *mtk_dsi = dsi;
	struct LCM_setting_table *aod_off_cmd = NULL;
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

	ctx = panel_to_lcm(panel);
	if (!ctx) {
		OFP_ERR("Invalid lcm params\n");
	}

	if (!ctx->m) {
		vrefresh_rate = 120;
		pr_info("default refresh rate is 120hz\n");
	} else {
		vrefresh_rate = drm_mode_vrefresh(ctx->m);
	}
	if (vrefresh_rate == 60) {
		aod_off_cmd = aod_off_cmd_60hz;
		reg_count = sizeof(aod_off_cmd_60hz) / sizeof(struct LCM_setting_table);
	} else if (vrefresh_rate == 120) {
		aod_off_cmd = aod_off_cmd_120hz;
		reg_count = sizeof(aod_off_cmd_120hz) / sizeof(struct LCM_setting_table);
	} else {
		aod_off_cmd = aod_off_cmd_90hz;
		reg_count = sizeof(aod_off_cmd_90hz) / sizeof(struct LCM_setting_table);
	}

	OFP_INFO("%s crtc_active:%d, doze_active:%llu\n", __func__, crtc->state->active, mtk_state->prop_val[CRTC_PROP_DOZE_ACTIVE]);

	for (i = 0; i < reg_count; i++) {
		unsigned int cmd;
		cmd = aod_off_cmd[i].cmd;

		switch (cmd) {
			case REGFLAG_DELAY:
				if (handle == NULL) {
					usleep_range(aod_off_cmd[i].count * 1000, aod_off_cmd[i].count * 1000 + 100);
				} else {
					cmdq_pkt_sleep(handle, CMDQ_US_TO_TICK(aod_off_cmd[i].count * 1000), CMDQ_GPR_R14);
				}
				break;
			case REGFLAG_UDELAY:
				if (handle == NULL) {
					usleep_range(aod_off_cmd[i].count, aod_off_cmd[i].count + 100);
				} else {
					cmdq_pkt_sleep(handle, CMDQ_US_TO_TICK(aod_off_cmd[i].count), CMDQ_GPR_R14);
				}
				break;
			case REGFLAG_END_OF_TABLE:
				break;
			default:
				cb(dsi, handle, aod_off_cmd[i].para_list, aod_off_cmd[i].count);
		}
	}

	lcm_setbacklight_cmdq(dsi, cb, handle, oplus_display_brightness);

	OFP_INFO("send aod off cmd\n");

	return 0;
}

static int panel_doze_enable(struct drm_panel *panel, void *dsi, dcs_write_gce cb, void *handle)
{
	unsigned int i = 0;
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

	OFP_INFO("%s crtc_active:%d, doze_active:%llu\n", __func__, crtc->state->active, mtk_state->prop_val[CRTC_PROP_DOZE_ACTIVE]);

	for (i = 0; i < (sizeof(aod_on_cmd)/sizeof(struct LCM_setting_table)); i++) {
		unsigned int cmd;
		cmd = aod_on_cmd[i].cmd;

		switch (cmd) {
			case REGFLAG_DELAY:
				usleep_range(aod_on_cmd[i].count * 1000, aod_on_cmd[i].count * 1000 + 100);
				break;
			case REGFLAG_UDELAY:
				usleep_range(aod_on_cmd[i].count, aod_on_cmd[i].count + 100);
				break;
			case REGFLAG_END_OF_TABLE:
				break;
			default:
				cb(dsi, handle, aod_on_cmd[i].para_list, aod_on_cmd[i].count);
		}
	}

	OFP_INFO("send aod on cmd\n");

	return 0;
}

static int panel_set_aod_light_mode(void *dsi, dcs_write_gce cb, void *handle, unsigned int level)
{
	int i = 0;

	if (level == 0) {
		for (i = 0; i < sizeof(aod_high_mode)/sizeof(struct LCM_setting_table); i++) {
			cb(dsi, handle, aod_high_mode[i].para_list, aod_high_mode[i].count);
		}
	} else {
		for (i = 0; i < sizeof(aod_low_mode)/sizeof(struct LCM_setting_table); i++) {
			cb(dsi, handle, aod_low_mode[i].para_list, aod_low_mode[i].count);
		}
	}
	OFP_INFO("level = %d\n", level);

	return 0;
}

static int panel_set_ultra_low_power_aod(struct drm_panel *panel, void *dsi,
		dcs_write_gce cb, void *handle, unsigned int level)
{
	unsigned int i = 0;
	unsigned int reg_count = 0;
	int vrefresh_rate = 0;
	struct lcm *ctx = NULL;
	struct LCM_setting_table *ultra_low_power_aod_cmd = NULL;

	if (!panel || !dsi) {
		OFP_ERR("Invalid dsi params\n");
	}

	ctx = panel_to_lcm(panel);
	if (!ctx) {
		OFP_ERR("Invalid lcm params\n");
	}

	if (!ctx->m) {
		vrefresh_rate = 120;
		pr_info("default refresh rate is 120hz\n");
	} else {
		vrefresh_rate = drm_mode_vrefresh(ctx->m);
	}
	if (vrefresh_rate == 60) {
		if (level == 1) {
			ultra_low_power_aod_cmd = ultra_low_power_aod_on_cmd_60hz;
			reg_count = sizeof(ultra_low_power_aod_on_cmd_60hz) / sizeof(struct LCM_setting_table);
		} else {
			ultra_low_power_aod_cmd = ultra_low_power_aod_off_cmd_60hz;
			reg_count = sizeof(ultra_low_power_aod_off_cmd_60hz) / sizeof(struct LCM_setting_table);
		}
	} else if (vrefresh_rate == 120) {
		if (level == 1) {
			ultra_low_power_aod_cmd = ultra_low_power_aod_on_cmd_120hz;
			reg_count = sizeof(ultra_low_power_aod_on_cmd_120hz) / sizeof(struct LCM_setting_table);
		} else {
			ultra_low_power_aod_cmd = ultra_low_power_aod_off_cmd_120hz;
			reg_count = sizeof(ultra_low_power_aod_off_cmd_120hz) / sizeof(struct LCM_setting_table);
		}
	} else {
		if (level == 1) {
			ultra_low_power_aod_cmd = ultra_low_power_aod_on_cmd_90hz;
			reg_count = sizeof(ultra_low_power_aod_on_cmd_90hz) / sizeof(struct LCM_setting_table);
		} else {
			ultra_low_power_aod_cmd = ultra_low_power_aod_off_cmd_90hz;
			reg_count = sizeof(ultra_low_power_aod_off_cmd_90hz) / sizeof(struct LCM_setting_table);
		}
	}
	for (i = 0; i < reg_count; i++) {
		unsigned int cmd;
		cmd = ultra_low_power_aod_cmd[i].cmd;

		switch (cmd) {
			case REGFLAG_DELAY:
				if (handle == NULL) {
					usleep_range(ultra_low_power_aod_cmd[i].count * 1000, ultra_low_power_aod_cmd[i].count * 1000 + 100);
				} else {
					cmdq_pkt_sleep(handle, CMDQ_US_TO_TICK(ultra_low_power_aod_cmd[i].count * 1000), CMDQ_GPR_R14);
				}
				break;
			case REGFLAG_UDELAY:
				if (handle == NULL) {
					usleep_range(ultra_low_power_aod_cmd[i].count, ultra_low_power_aod_cmd[i].count + 100);
				} else {
					cmdq_pkt_sleep(handle, CMDQ_US_TO_TICK(ultra_low_power_aod_cmd[i].count), CMDQ_GPR_R14);
				}
				break;
			case REGFLAG_END_OF_TABLE:
				break;
			default:
				cb(dsi, handle, ultra_low_power_aod_cmd[i].para_list, ultra_low_power_aod_cmd[i].count);
		}
	}

	OFP_INFO("level = %d\n", level);

	return 0;
}

#endif /* OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT */

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s:on=%d\n", __func__,on);

	gpiod_set_value(ctx->reset_gpio, on);

	return 0;
}

static int lcm_panel_reset(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (ctx->prepared) {
		pr_info("ctx->prepared:%d return! \n",ctx->prepared);
		return 0;
	}
	usleep_range(5000, 5100);
	// lcd reset H -> L -> H
	if(IS_ERR(ctx->reset_gpio)){
		pr_err("cannot get reset-gpios %ld\n",PTR_ERR(ctx->reset_gpio));
	}
	gpiod_set_value(ctx->reset_gpio, 1);
	/* Wait > 1ms, actual 3ms */
	usleep_range(3000, 3100);
	gpiod_set_value(ctx->reset_gpio, 0);
	/* Wait > 10us, actual 2ms */
	usleep_range(5000, 5100);
	gpiod_set_value(ctx->reset_gpio, 1);
	/* Wait > 20ms, actual 25ms */
	usleep_range(25000, 25100);
	pr_info("%s:Successful\n", __func__);

	return 0;
}
static int lcm_panel_poweron(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	if (ctx->prepared)
		return 0;

	//iovcc enable 1.8V
	lcm_panel_1p8_ldo_enable(ctx->dev);
	/* Wait > 1ms, actual 5ms */
	usleep_range(5000, 5100);
	//enable vcore 1p2 for boe is high 1.22v
	// gpiod_set_value(ctx->vddr_aod_enable_gpio, 1);
	// usleep_range(1000, 1100);
	gpiod_set_value(ctx->vddr1p2_enable_gpio, 1);
	/* Wait no limits, actual 3ms */
	usleep_range(5000, 5100);
	//enable ldo 3p0
	lcm_panel_wl2868c_ldo_enable(ctx->dev);
	/* Wait > 10ms, actual 12ms */
	usleep_range(22000, 22100);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	pr_info("%s:Successful\n", __func__);
	return 0;
}

static int lcm_panel_poweroff(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	if (ctx->prepared)
		return 0;

	usleep_range(10000, 10100);
	gpiod_set_value(ctx->reset_gpio, 0);
	/* Wait > 1ms, actual 5ms */
	usleep_range(5000, 5100);
	//disable ldo 3p0
	lcm_panel_wl2868c_ldo_disable(ctx->dev);
	/* Wait no limits, actual 5ms */
	usleep_range(10000, 10100);
	//disable vcore1.2V
	gpiod_set_value(ctx->vddr1p2_enable_gpio, 0);
	/* Wait > 1ms, actual 5ms */
	usleep_range(10000, 10100);
	//set vddi 1.8v
	lcm_panel_1p8_ldo_disable(ctx->dev);
	/* power off Foolproof, actual 70ms*/
	usleep_range(72000, 72100);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);
	pr_info("%s: Successful\n", __func__);
	return 0;
}

struct drm_display_mode *get_mode_by_id(struct drm_connector *connector, unsigned int mode)
{
	struct drm_display_mode *m;
	unsigned int i = 0;

	list_for_each_entry(m, &connector->modes, head) {
		if (i == mode)
			return m;
		i++;
	}
	return NULL;
}

static int mtk_panel_ext_param_set(struct drm_panel *panel, struct drm_connector *connector, unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	int m_vrefresh = 0;
	struct drm_display_mode *m = get_mode_by_id(connector, mode);

	m_vrefresh = drm_mode_vrefresh(m);
	pr_info("%s: mode=%d, vrefresh=%d\n", __func__, mode, drm_mode_vrefresh(m));

	if (m_vrefresh == 60) {
		ext->params = &ext_params_60Hz;
	} else if (m_vrefresh == 90) {
		ext->params = &ext_params_90Hz;
	} else if (m_vrefresh == 120) {
		ext->params = &ext_params_120Hz;
	} else {
		ext->params = &ext_params_60Hz;
		ret = 1;
	}

	return ret;
}

static int mtk_panel_ext_param_get(struct drm_panel *panel,
		struct drm_connector *connector,
		struct mtk_panel_params **ext_param,
		unsigned int id)
{
	int ret = 0;
	int m_vrefresh = 0;
	struct drm_display_mode *m = get_mode_by_id(connector, id);

	m_vrefresh = drm_mode_vrefresh(m);

	if (m_vrefresh == 60) {
		*ext_param = &ext_params_60Hz;
	} else if (m_vrefresh == 120) {
		*ext_param = &ext_params_120Hz;
	} else if (m_vrefresh == 90) {
		*ext_param = &ext_params_90Hz;
	} else {
		*ext_param = &ext_params_60Hz;
	}

	if (*ext_param)
		pr_debug("[LCM] data_rate:%d\n", (*ext_param)->data_rate);
	else
		pr_err("[LCM] ext_param is NULL;\n");

	return ret;
}

static int mode_switch_hs(struct drm_panel *panel, struct drm_connector *connector,
		void *dsi_drv, unsigned int cur_mode, unsigned int dst_mode,
			enum MTK_PANEL_MODE_SWITCH_STAGE stage, dcs_write_gce_pack cb)
{
	int ret = 0;
	int m_vrefresh = 0;
	unsigned int lcm_cmd_count = 0;
	static int last_data_rate = 900;
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	struct drm_display_mode *m = get_mode_by_id(connector, dst_mode);
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("lcm cur_mode = %d dst_mode %d.\n", cur_mode, dst_mode);

	if (cur_mode == dst_mode) {
		return ret;
	}

	if (stage == BEFORE_DSI_POWERDOWN) {
		m_vrefresh = drm_mode_vrefresh(m);

		if (m_vrefresh == 60) {
			lcm_cmd_count = sizeof(timing_switch_cmd_sdc60) / sizeof(struct LCM_setting_table);
			panel_send_pack_hs_cmd(dsi_drv, timing_switch_cmd_sdc60, lcm_cmd_count, cb, NULL);
			pr_info("lcm timing switch to 60\n");
		} else if (m_vrefresh == 90) {
			lcm_cmd_count = sizeof(timing_switch_cmd_sdc90) / sizeof(struct LCM_setting_table);
			panel_send_pack_hs_cmd(dsi_drv, timing_switch_cmd_sdc90, lcm_cmd_count, cb, NULL);
			pr_info("lcm timing switch to 90\n");
		} else if (m_vrefresh == 120) {
			lcm_cmd_count = sizeof(timing_switch_cmd_sdc120) / sizeof(struct LCM_setting_table);
			panel_send_pack_hs_cmd(dsi_drv, timing_switch_cmd_sdc120, lcm_cmd_count, cb, NULL);
			pr_info("lcm timing switch to 120\n");
		}
	}
	//else if (stage == AFTER_DSI_POWERON) {
		ctx->m = m;
	//}

	if (ext->params->data_rate != last_data_rate) {
		ret = 1;
		pr_info("need to change mipi clk, data_rate=%d,last_data_rate=%d\n",ext->params->data_rate,last_data_rate);
		last_data_rate = ext->params->data_rate;
	}

	return ret;
}

// static unsigned int lcm_get_reg_ldo(struct regulator *ldo)
// {
	// unsigned int volt = 0;

	// if (regulator_is_enabled(ldo))
		// /* regulator_get_voltage return volt with uV */
		// volt = regulator_get_voltage(ldo);

	// return volt;
// }

// void lcm_read_info(struct drm_panel *panel, int read_ic)
// {
	// unsigned int volt = 0;

	// /*read the 1.8v*/
	// if (!IS_ERR_OR_NULL(vrf18_ldo)) {
		// volt = lcm_get_reg_ldo(vrf18_ldo);
		// OPLUS_DSI_INFO("get the Regulator vrf18_ldo =%d.\n",volt);
	// }

	// /*read the 1.2v*/
	// if (!IS_ERR_OR_NULL(vrf12_ldo)) {
		// volt = lcm_get_reg_ldo(vrf12_ldo);
		// OPLUS_DSI_INFO("get the Regulator vrf12_ldo =%d.\n",volt);
	// }

	// /*read the 3.3v*/
	// if (!IS_ERR_OR_NULL(wl2868c_ldo)) {
		// volt = lcm_get_reg_ldo(wl2868c_ldo);
		// OPLUS_DSI_INFO("get the Regulator wl2868c_ldo =%d.\n",volt);
	// }

	// OPLUS_DSI_INFO("read_ic = %d\n", read_ic);

	// return;
// }

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.panel_poweron = lcm_panel_poweron,
	.panel_reset = lcm_panel_reset,
	.panel_poweroff = lcm_panel_poweroff,
	.ata_check = panel_ata_check,
	.ext_param_set = mtk_panel_ext_param_set,
	.ext_param_get = mtk_panel_ext_param_get,
	.mode_switch_hs = mode_switch_hs,
#ifdef OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT
	.oplus_set_hbm = lcm_set_hbm,
	.oplus_hbm_set_cmdq = panel_hbm_set_cmdq,
	.oplus_ofp_set_lhbm_pressed_icon = oplus_ofp_set_lhbm_pressed_icon,
	.doze_disable = panel_doze_disable,
	.doze_enable = panel_doze_enable,
	.set_aod_light_mode = panel_set_aod_light_mode,
	.set_ultra_low_power_aod = panel_set_ultra_low_power_aod,
#endif /* OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT */
	// .oplus_get_info = lcm_read_info,
	// .lcm_set_hbm_max = oplus_display_panel_set_hbm_max,
};

static int lcm_get_modes(struct drm_panel *panel, struct drm_connector *connector)
{
	struct drm_display_mode *mode[3];
	struct lcm *ctx = panel_to_lcm(panel);

	mode[0] = drm_mode_duplicate(connector->dev, &disp_mode_60Hz);
	if (!mode[0]) {
		pr_err("failed to add mode %ux%ux@%u\n", disp_mode_60Hz.hdisplay, disp_mode_60Hz.vdisplay, drm_mode_vrefresh(&disp_mode_60Hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode[0]);
	mode[0]->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode[0]);
	pr_info("clock=%d,htotal=%d,vtotal=%d,hskew=%d,vrefresh=%d\n", mode[0]->clock, mode[0]->htotal,
		mode[0]->vtotal, mode[0]->hskew, drm_mode_vrefresh(mode[0]));

	mode[1] = drm_mode_duplicate(connector->dev, &disp_mode_90Hz);
	if (!mode[1]) {
		pr_err("failed to add mode %ux%ux@%u\n", disp_mode_90Hz.hdisplay, disp_mode_90Hz.vdisplay, drm_mode_vrefresh(&disp_mode_90Hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode[1]);
	mode[1]->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode[1]);

	mode[2] = drm_mode_duplicate(connector->dev, &disp_mode_120Hz);
	if (!mode[2]) {
		pr_err("failed to add mode %ux%ux@%u\n", disp_mode_120Hz.hdisplay, disp_mode_120Hz.vdisplay, drm_mode_vrefresh(&disp_mode_120Hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode[2]);
	mode[2]->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode[2]);

	connector->display_info.width_mm = 71;
	connector->display_info.height_mm = 156;

	if (!ctx->m) {
		ctx->m = get_mode_by_id(connector, 0);
		pr_info("ctx->m init: mode_id %d\n",get_mode_enum(ctx->m));
	}
	return 1;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;
	struct lcm *ctx;
	struct device_node *backlight;
	int ret;

	pr_info("[LCM] %s+ ac178_p_3 a0004 Start\n", __func__);

	dsi_node = of_get_parent(dev->of_node);
	if (dsi_node) {
		endpoint = of_graph_get_next_endpoint(dsi_node, NULL);

		if (endpoint) {
			remote_node = of_graph_get_remote_port_parent(endpoint);
			if (!remote_node) {
				pr_err("No panel connected,skip probe lcm\n");
				return -ENODEV;
			}
			pr_err("device node name:%s\n", remote_node->name);
		}
	}
	if (remote_node != dev->of_node) {
		pr_err("skip probe due to not current lcm\n");
		return -ENODEV;
	}

	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);
	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_LPM | MIPI_DSI_CLOCK_NON_CONTINUOUS;

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight) {
			pr_err("skip probe due to lcm backlight null\n");
			return -EPROBE_DEFER;
		}
	}

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR_OR_NULL(ctx->reset_gpio)) {
		pr_err("cannot get reset-gpios %ld\n",
			 PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 1);

	lcm_panel_1p8_ldo_enable(ctx->dev);

	usleep_range(5000, 5100);

	ctx->vddr1p2_enable_gpio = devm_gpiod_get(dev, "1p2", GPIOD_OUT_HIGH);
	if (IS_ERR_OR_NULL(ctx->vddr1p2_enable_gpio)) {
		pr_err("cannot get vddr1p2_enable_gpio %ld\n",
			 PTR_ERR(ctx->vddr1p2_enable_gpio));
		return PTR_ERR(ctx->vddr1p2_enable_gpio);
	}
	gpiod_set_value(ctx->vddr1p2_enable_gpio, 1);

	usleep_range(5000, 5100);

	lcm_panel_wl2868c_ldo_enable(ctx->dev);

	ctx->prepared = true;
	ctx->enabled = true;
	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params_60Hz, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;

#endif

	register_device_proc("lcd", "ac178_A0004", "P_3");
#ifdef OPLUS_FEATURE_DISPLAY
	oplus_panel_parse(dev->of_node, TRUE);
#endif /* OPLUS_FEATURE_DISPLAY */
	//flag_silky_panel = BL_SETTING_DELAY_60HZ;
	oplus_max_normal_brightness = MAX_NORMAL_BRIGHTNESS;
	oplus_max_brightness = BRIGHTNESS_MAX;
	oplus_enhance_mipi_strength = 1;
	//g_is_silky_panel = true;

	//sscanf(oplus_lcm_id2, "%d", &lcm_id2);
	pr_info("ac178_A0004_P_3 lcm probe End.\n");
	return ret;
}


static void lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);
#if defined(CONFIG_MTK_PANEL_EXT)
	struct mtk_panel_ctx *ext_ctx = find_panel_ctx(&ctx->panel);
#endif

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_detach(ext_ctx);
	mtk_panel_remove(ext_ctx);
#endif
}

static const struct of_device_id lcm_of_match[] = {
	{
		.compatible = "panel_ac178_p_3_a0004_dsi_cmd",
	},
	{}
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel_ac178_p_3_a0004_dsi_cmd",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

static int __init lcm_drv_init(void)
{
	int ret = 0;

	pr_notice("%s+\n", __func__);
	mtk_panel_lock();
	ret = mipi_dsi_driver_register(&lcm_driver);
	if (ret < 0)
		pr_notice("%s, Failed to register lcm driver: %d\n", __func__, ret);

	mtk_panel_unlock();
	pr_notice("%s- ret:%d\n", __func__, ret);
	return 0;
}

static void __exit lcm_drv_exit(void)
{
	pr_notice("%s+\n", __func__);
	mtk_panel_lock();
	mipi_dsi_driver_unregister(&lcm_driver);
	mtk_panel_unlock();
	pr_notice("%s-\n", __func__);
}

module_init(lcm_drv_init);
module_exit(lcm_drv_exit);

MODULE_AUTHOR("Oplus Display");
MODULE_DESCRIPTION("panel_ac178_p_3_a0004_dsi_cmd Driver");
MODULE_LICENSE("GPL v2");
