// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

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
#include "oplus_dsi_display_config.h"
#include "../oplus_display_debug.h"


//#include "../../../misc/mediatek/lpm/inc/lpm_module.h"
//#include "../../../misc/mediatek/lpm/modules/include/mt6897/pwr_ctrl.h"
#ifdef OPLUS_FEATURE_DISPLAY_ADFR
#include "oplus_adfr.h"
#endif /* OPLUS_FEATURE_DISPLAY_ADFR */

#include "ac118_data_hw_roundedpattern.h"

#define REGFLAG_CMD				0xFFFA
#define REGFLAG_DELAY			0xFFFC
#define REGFLAG_UDELAY			0xFFFB
#define REGFLAG_END_OF_TABLE	0xFFFD
#define PHYSICAL_WIDTH (69552)
#define PHYSICAL_HEIGHT (155330)

#define BRIGHTNESS_MAX    4095
#define BRIGHTNESS_HALF   2047
#define MAX_NORMAL_BRIGHTNESS   3515
#define LCM_BRIGHTNESS_TYPE 2

#define MAX_CMDLINE_PARAM_LEN 512

#define TAG "ac168_p_B_a0012"

static unsigned int esd_brightness = 1023;
//extern unsigned int silence_mode;
//extern unsigned int last_backlight;
extern unsigned int oplus_display_brightness;
extern unsigned int oplus_max_normal_brightness;
//extern unsigned int oplus_lcm_display_on;
static bool aod_state = false;

#ifdef OPLUS_FEATURE_DISPLAY
extern int oplus_panel_parse(struct device_node *node, bool is_primary);
#endif /* OPLUS_FEATURE_DISPLAY */
extern void lcdinfo_notify(unsigned long val, void *v);
//extern int pmic_ldo_set_voltage_mv(unsigned int ldo_num, int set_mv);
//extern int pmic_ldo_set_disable(unsigned int ldo_num);
//static bool panel_power_on = false;

//extern unsigned int oplus_enhance_mipi_strength;
struct LCM_setting_table {
  	unsigned int cmd;
 	unsigned char count;
 	unsigned char para_list[128];
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

static int panel_send_pack_hs_cmd(void *dsi, struct LCM_setting_table *table, unsigned int lcm_cmd_count, dcs_write_gce_pack cb, void *handle);

#ifdef PANEL_SUPPORT_READBACK
static int lcm_dcs_read(struct lcm *ctx, u8 cmd, void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return 0;

	ret = mipi_dsi_dcs_read(dsi, cmd, data, len);
	if (ret < 0) {
		OPLUS_DISP_ERR(TAG,"error %d reading dcs seq:(%#x)\n", ret,
			 cmd);
		ctx->error = ret;
	}

	return ret;
}

static void lcm_panel_get_data(struct lcm *ctx)
{
	u8 buffer[3] = { 0 };
	static int ret;

	OPLUS_DISP_INFO(TAG,"+\n");

	if (ret == 0) {
		ret = lcm_dcs_read(ctx, 0x0A, buffer, 1);
		OPLUS_DISP_INFO(TAG,"0x%08x\n",buffer[0] | (buffer[1] << 8));
		OPLUS_DISP_INFO(TAG,"return %d data(0x%08x) to dsi engine\n",
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
		OPLUS_DISP_ERR(TAG,"error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
}

static void lcm_panel_init(struct lcm *ctx)
{
	OPLUS_DISP_INFO(TAG, "%s +\n", __func__);
	/* APL Peak Luminance ON */
	lcm_dcs_write_seq_static(ctx,0xFF,0x78,0x38,0x0B);
	lcm_dcs_write_seq_static(ctx,0x02,0x00);
	lcm_dcs_write_seq_static(ctx,0x17,0xEB);
	lcm_dcs_write_seq_static(ctx,0x18,0xEB);
	lcm_dcs_write_seq_static(ctx,0x19,0xEB);
	lcm_dcs_write_seq_static(ctx,0x1A,0xEB);
	lcm_dcs_write_seq_static(ctx,0x1B,0xEB);
	lcm_dcs_write_seq_static(ctx,0x1C,0xEB);
	lcm_dcs_write_seq_static(ctx,0x1D,0xEB);
	lcm_dcs_write_seq_static(ctx,0xFF,0x78,0x38,0x0A);
	lcm_dcs_write_seq_static(ctx,0x37,0x4F);
	lcm_dcs_write_seq_static(ctx,0x38,0x90);
	lcm_dcs_write_seq_static(ctx,0xFF,0x78,0x38,0x00);
	lcm_dcs_write_seq_static(ctx,0x55,0x03);
	/* R Corner Control off */
	lcm_dcs_write_seq_static(ctx,0xFF,0x78,0x38,0x17);
	lcm_dcs_write_seq_static(ctx,0x20,0x00);
	/* NVM not reload */
	lcm_dcs_write_seq_static(ctx,0xFF,0x78,0x38,0x08);
	lcm_dcs_write_seq_static(ctx,0x45,0x4C);
	/* Frame Rate 60Hz */
	lcm_dcs_write_seq_static(ctx,0xFF,0x78,0x38,0x02);
	lcm_dcs_write_seq_static(ctx,0x38,0x13);
	lcm_dcs_write_seq_static(ctx,0x63,0x0D);
	/* DSC Setting(10bit_3.75X) */
	lcm_dcs_write_seq_static(ctx,0xFF,0x78,0x38,0x07);
	lcm_dcs_write_seq_static(ctx,0x29,0x01);
	lcm_dcs_write_seq_static(ctx,0x20,0x00,0x00,0x00,0x00,0x00,0x11,0x00,0x00,0xab,
								 0x30,0x80,0x09,0x6c,0x04,0x38,0x00,0x0c,0x02,0x1c,
								 0x02,0x1c,0x02,0x00,0x02,0x0e,0x00,0x20,0x01,0x1f,
								 0x00,0x07,0x00,0x0c,0x08,0xbb,0x08,0x7a,0x18,0x00,
								 0x10,0xf0,0x07,0x10,0x20,0x00,0x06,0x0f,0x0f,0x33,
								 0x0e,0x1c,0x2a,0x38,0x46,0x54,0x62,0x69,0x70,0x77,
								 0x79,0x7b,0x7d,0x7e,0x02,0x02,0x22,0x00,0x2a,0x40,
								 0x2a,0xbe,0x3a,0xfc,0x3a,0xfa,0x3a,0xf8,0x3b,0x38,
								 0x3b,0x78,0x3b,0xb6,0x4b,0xb6,0x4b,0xf4,0x4b,0xf4,
								 0x6c,0x34,0x84,0x74,0x00,0x00,0x00,0x00,0x00,0x00);
	/* IR IP ON */
	lcm_dcs_write_seq_static(ctx,0xFF,0x78,0x38,0x00);
	lcm_dcs_write_seq_static(ctx,0x95,0x10);
	lcm_dcs_write_seq_static(ctx,0xFF,0x78,0x38,0x01);
	lcm_dcs_write_seq_static(ctx,0x48,0x00);
	lcm_dcs_write_seq_static(ctx,0x53,0x00);
	lcm_dcs_write_seq_static(ctx,0x5E,0x00);
	lcm_dcs_write_seq_static(ctx,0x69,0x00);
	lcm_dcs_write_seq_static(ctx,0xFF,0x78,0x38,0x10);
	lcm_dcs_write_seq_static(ctx,0x84,0x81);
	/* TE ON */
	lcm_dcs_write_seq_static(ctx,0xFF,0x78,0x38,0x00);
	lcm_dcs_write_seq_static(ctx,0x35,0x00);
	/* Dimming Setting */
	lcm_dcs_write_seq_static(ctx,0x53,0x20);
	/* Sleep out */
	lcm_dcs_write_seq_static(ctx,0x11,0x00);
	usleep_range(120000, 120100);
	/* ESD Check */
	lcm_dcs_write_seq_static(ctx,0xFF,0x78,0x38,0x08);
	lcm_dcs_write_seq_static(ctx,0x57,0x25);
	lcm_dcs_write_seq_static(ctx,0xFF,0x78,0x38,0x06);
	lcm_dcs_write_seq_static(ctx,0xC6,0x01);
	usleep_range(20000, 20100);
	lcm_dcs_write_seq_static(ctx,0xFF,0x78,0x38,0x00);
	/* Display On */
	lcm_dcs_write_seq_static(ctx,0x29,0x00);
	usleep_range(20000, 20100);
	OPLUS_DISP_INFO(TAG,"%s -\n", __func__);
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
    OPLUS_DISP_DEBUG(TAG,"prepared=%d\n", ctx->prepared);

	if (!ctx->prepared)
		return 0;

    //oplus_lcm_display_on = 0;
	lcm_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_OFF);
	/* Wait 0ms, actual 15ms */
	usleep_range(15000, 15100);
	lcm_dcs_write_seq_static(ctx, MIPI_DCS_ENTER_SLEEP_MODE);
	/* Wait > 100ms, actual 125ms */
	usleep_range(125000, 125100);
    //todo
	/* keep vcore off */
	//mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_SUSPEND_PWR_CTRL, MT_LPM_SMC_ACT_SET, PW_REG_SPM_VCORE_REQ, 0x0);
	//OPLUS_DISP_DEBUG(TAG," call lpm_smc_spm_dbg keep vcore off for display off!\n");

	ctx->error = 0;
	ctx->prepared = false;
	OPLUS_DISP_INFO(TAG,"%s:success\n", __func__);

	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	OPLUS_DISP_DEBUG(TAG,"prepared=%d\n", ctx->prepared);
	if (ctx->prepared)
		return 0;

	/* Wait > 10ms, actual 10ms */
	usleep_range(10000, 10100);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(5000, 5100);
	gpiod_set_value(ctx->reset_gpio, 0);
	/* Wait 30us, actual 5ms */
	usleep_range(5000, 5100);
	gpiod_set_value(ctx->reset_gpio, 1);
	/* Wait > 20ms, actual 20ms */
	usleep_range(20000, 20100);
	lcm_panel_init(ctx);
	//oplus_lcm_display_on = 1;

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;
#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif

	OPLUS_DISP_INFO(TAG,"%s:success\n", __func__);
	return ret;
}

static const struct drm_display_mode disp_mode_60Hz = {
	.clock = 166800,
        .hdisplay = 1080,
        .hsync_start = 1080 + 9,//HFP
        .hsync_end = 1080 + 9 + 2,//HSA
        .htotal = 1080 + 9 + 2 + 21,//HBP
        .vdisplay = 2412,
        .vsync_start = 2412 + 52,//VFP
        .vsync_end = 2412 + 52 + 14,//VSA
        .vtotal = 2412 + 52+ 14 + 22,//VBP
		//.hskew = STANDARD_MFR,
};

static const struct drm_display_mode disp_mode_90Hz = {
        .clock = 250200,
        .hdisplay = 1080,
        .hsync_start = 1080 + 9,//HFP
        .hsync_end = 1080 + 9 + 2,//HSA
        .htotal = 1080 + 9 + 2 + 21,//HBP
        .vdisplay = 2412,
        .vsync_start = 2412 + 52,//VFP
        .vsync_end = 2412 + 52 + 14,//VSA
        .vtotal = 2412 + 52+ 14 + 22,//VBP
		//.hskew = STANDARD_ADFR,
};

static const struct drm_display_mode disp_mode_120Hz = {
        .clock = 333600,
        .hdisplay = 1080,
        .hsync_start = 1080 + 9,//HFP
        .hsync_end = 1080 + 9 + 2,//HSA
        .htotal = 1080 + 9 + 2 + 21,//HBP
        .vdisplay = 2412,
        .vsync_start = 2412 + 52,//VFP
        .vsync_end = 2412 + 52 + 14,//VSA
        .vtotal = 2412 + 52+ 14 + 22,//VBP
		//.hskew = STANDARD_ADFR,
};

static struct mtk_panel_params ext_params_60Hz = {
	.pll_clk = 300,
	.phy_timcon = {
	    .hs_trail = 6,
	    .clk_trail = 7,
	},
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	//.esd_check_multi = 0,
	/* 0x0A:pre-set 0x08, normal 0x9C, AOD 0xDC */
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size = sizeof(top_rc_pattern),
	.corner_pattern_lt_addr = (void *)top_rc_pattern,
#endif

	.color_dual_panel_status = false,
	.color_dual_brightness_status = true,
	.vendor = "A0012",
	.manufacture = "P_B",
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
		//.dsc_cfg_change = 0,
        .ver = 17,
        .slice_mode = 1,
        .rgb_swap = 0,
        .dsc_cfg = 40,
        .rct_on = 1,
        .bit_per_channel = 10,
        .dsc_line_buf_depth = 11,
        .bp_enable = 1,
        .bit_per_pixel = 128,
        .pic_height = 2412,
        .pic_width = 1080,
        .slice_height = 12,
        .slice_width = 540,
        .chunk_size = 540,
        .xmit_delay = 512,
        .dec_delay = 526,
        .scale_value = 32,
        .increment_interval = 287,
        .decrement_interval = 7,
        .line_bpg_offset = 12,
        .nfl_bpg_offset = 2235,
        .slice_bpg_offset = 2170,
        .initial_offset = 6144,
        .final_offset = 4336,
        .flatness_minqp = 7,
        .flatness_maxqp = 16,
        .rc_model_size = 8192,
        .rc_edge_factor = 6,
        .rc_quant_incr_limit0 = 15,
        .rc_quant_incr_limit1 = 15,
        .rc_tgt_offset_hi = 3,
        .rc_tgt_offset_lo = 3,
		},
	.data_rate = 600,
	.oplus_serial_para0 = 0xA3,
	.oplus_ofp_need_to_sync_data_in_aod_unlocking = true,
        .oplus_ofp_aod_off_insert_black = 2,
        .oplus_ofp_aod_off_black_frame_total_time = 67,
        .oplus_ofp_need_keep_apart_backlight = true,
        .oplus_ofp_hbm_on_delay = 0,
        .oplus_ofp_pre_hbm_off_delay = 0,
        .oplus_ofp_hbm_off_delay = 0,
        .dyn_fps = {
                .switch_en = 1, .vact_timing_fps = 60,
        },
};

static struct mtk_panel_params ext_params_90Hz = {
	.pll_clk = 300,
	.phy_timcon = {
            .hs_trail = 6,
            .clk_trail = 7,
        },
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	//.esd_check_multi = 0,
	/* 0x0A:pre-set 0x08, normal 0x9C, AOD 0xDC */
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size = sizeof(top_rc_pattern),
	.corner_pattern_lt_addr = (void *)top_rc_pattern,
#endif

	.color_dual_panel_status = false,
	.color_dual_brightness_status = true,
	.vendor = "A0012",
	.manufacture = "P_B",
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
		//.dsc_cfg_change = 0,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 40,
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2412,
		.pic_width = 1080,
		.slice_height = 12,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 287,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 2235,
		.slice_bpg_offset = 2170,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 7,
		.flatness_maxqp = 16,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 15,
		.rc_quant_incr_limit1 = 15,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		},
	.data_rate = 600,
	.oplus_serial_para0 = 0xA3,
	.oplus_ofp_need_to_sync_data_in_aod_unlocking = true,
        .oplus_ofp_aod_off_insert_black = 2,
        .oplus_ofp_aod_off_black_frame_total_time = 56,
        .oplus_ofp_need_keep_apart_backlight = true,
        .oplus_ofp_hbm_on_delay = 0,
        .oplus_ofp_pre_hbm_off_delay = 0,
        .oplus_ofp_hbm_off_delay = 0,
        .dyn_fps = {
                .switch_en = 1, .vact_timing_fps = 90,
        },
};

static struct mtk_panel_params ext_params_120Hz = {
	.pll_clk = 554,
	.phy_timcon = {
            .hs_trail = 10,
            .clk_trail = 11,
        },
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	//.esd_check_multi = 0,
	/* 0x0A:pre-set 0x08, normal 0x9C, AOD 0xDC */
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size = sizeof(top_rc_pattern),
	.corner_pattern_lt_addr = (void *)top_rc_pattern,
#endif
	.color_dual_panel_status = false,
	.color_dual_brightness_status = true,
	.cmd_null_pkt_en = 0,
	.cmd_null_pkt_len = 105,
	.vendor = "A0012",
	.manufacture = "P_B",
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
		//.dsc_cfg_change = 0,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 40,
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2412,
		.pic_width = 1080,
		.slice_height = 12,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 287,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 2235,
		.slice_bpg_offset = 2170,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 7,
		.flatness_maxqp = 16,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 15,
		.rc_quant_incr_limit1 = 15,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		},
	.data_rate = 1107,
	.oplus_serial_para0 = 0xA3,
	.oplus_ofp_need_to_sync_data_in_aod_unlocking = true,
        .oplus_ofp_aod_off_insert_black = 2,
        .oplus_ofp_aod_off_black_frame_total_time = 50,
        .oplus_ofp_need_keep_apart_backlight = true,
        .oplus_ofp_hbm_on_delay = 0,
        .oplus_ofp_pre_hbm_off_delay = 2,
        .oplus_ofp_hbm_off_delay = 0,
	.dyn_fps = {
                .switch_en = 1, .vact_timing_fps = 120,
        },
};

static int panel_ata_check(struct drm_panel *panel)
{
	/* Customer test by own ATA tool */
	return 1;
}

static int panel_send_pack_hs_cmd(void *dsi, struct LCM_setting_table *table, unsigned int lcm_cmd_count, dcs_write_gce_pack cb, void *handle)
{
	unsigned int i = 0;
	struct mtk_ddic_dsi_cmd send_cmd_to_ddic;

	if (lcm_cmd_count > MAX_TX_CMD_NUM_PACK) {
		OPLUS_DISP_ERR(TAG,"out of mtk_ddic_dsi_cmd \n");
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

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb, void *handle, unsigned int level)
{
    unsigned int mapped_level = 0;
    /* 07FF = 2047 */
	char bl_tb0[] = {0x51, 0x07, 0xFF};

	if (!dsi || !cb) {
		return -EINVAL;
	}

	if (level == 1) {
		OPLUS_DISP_INFO(TAG,"enter aod!!!\n");
		return 0;
	}
	//if (get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT && level > 0)
	//	level = 2047;

	bl_tb0[1] = level >> 8;
	bl_tb0[2] = level & 0xFF;
	mapped_level = level;
	if (mapped_level > 1) {
		lcdinfo_notify(LCM_BRIGHTNESS_TYPE, &mapped_level);
	}

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
	esd_brightness = level;
	oplus_display_brightness = level;
	OPLUS_DISP_INFO(TAG,"backlight=%d,bl_tb0[1]=0x%x,bl_tb0[2]=0x%x,mapped_level=%d \n", level,bl_tb0[1] ,bl_tb0[2],mapped_level);
	return 0;
}

 static int oplus_esd_backlight_recovery(void *dsi, dcs_write_gce cb, void *handle)
 {
    /* 03FF = 1023 */
	char bl_tb0[] = {0x51, 0x03, 0xff};

	bl_tb0[1] = esd_brightness >> 8;
	bl_tb0[2] = esd_brightness & 0xFF;
 	if (!cb)
		return -1;
	OPLUS_DISP_INFO(TAG," bl_tb0[1]=%x, bl_tb0[2]=%x\n", bl_tb0[1], bl_tb0[2]);
	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));

 	return 1;
 }



static struct LCM_setting_table lcm_setbrightness_normal[] = {
        {REGFLAG_CMD, 3, {0x51,0x00,0x00}},
};

static int oplus_panel_set_backlight_cmdq(void *dsi, dcs_write_gce_pack cb, void *handle, unsigned int level)
{
	if (!dsi || !cb) {
		return -EINVAL;
	}
	OPLUS_DISP_INFO(TAG,"backlight level:%u\n", level);

	unsigned int BL_MSB = 0;
	unsigned int BL_LSB = 0;
	if (level == 1) {
		OPLUS_DISP_INFO(TAG, "enter aod!!!\n");
		return 0;
	}
	if (get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT && level > 0)
		level = 2047;

	BL_LSB = level >> 8;
	BL_MSB = level & 0xFF;

	lcm_setbrightness_normal[0].para_list[1] = BL_LSB;
	lcm_setbrightness_normal[0].para_list[2] = BL_MSB;

	panel_send_pack_hs_cmd(dsi, lcm_setbrightness_normal, sizeof(lcm_setbrightness_normal) / sizeof(struct LCM_setting_table), cb, handle);
	OPLUS_DISP_INFO(TAG,"lcm_setbrightness_normal[0].para_list[1]=%x, lcm_setbrightness_normal[0].para_list[2]=%x, level:%d\n", lcm_setbrightness_normal[0].para_list[1] , lcm_setbrightness_normal[0].para_list[2],level);
	return 0;
}

//#ifdef OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT

static struct LCM_setting_table lcm_finger_HBM_on_setting[] = {
        {REGFLAG_CMD, 3, {0x51,0x0f,0xff}},
};

static int lcm_set_hbm(void *dsi, dcs_write_gce_pack cb,
		void *handle, unsigned int hbm_mode)
{
	//int i = 0;
    unsigned int lcm_cmd_count = 0;
	OFP_DEBUG("start\n");

	if (!dsi || !cb) {
		OFP_ERR("Invalid params\n");
		return -EINVAL;
	}

	OPLUS_DISP_INFO(TAG,"oplus_display_brightness= %u, hbm_mode=%u\n", oplus_display_brightness, hbm_mode);

	if(hbm_mode == 1) {
        lcm_cmd_count = sizeof(lcm_finger_HBM_on_setting)/sizeof(struct LCM_setting_table);
        panel_send_pack_hs_cmd(dsi, lcm_finger_HBM_on_setting, lcm_cmd_count, cb, handle);
        lcdinfo_notify(1, &hbm_mode);
	} else if (hbm_mode == 0) {
		//lcm_setbrightness(dsi, cb, handle, oplus_display_brightness);  //level
        oplus_panel_set_backlight_cmdq(dsi,cb,handle,oplus_display_brightness);
		OPLUS_DISP_DEBUG(TAG," %u ! backlight %d !\n",hbm_mode, oplus_display_brightness);
	}


	OFP_DEBUG("end\n");

	return 0;
}

static int panel_hbm_set_cmdq(struct drm_panel *panel, void *dsi,
		dcs_write_gce_pack cb, void *handle, bool en)
{

	int level = 0;
    unsigned int lcm_cmd_count = 0;
	OFP_DEBUG("start\n");

	if (!panel || !dsi || !cb) {
		OFP_ERR("Invalid input params\n");
		return -EINVAL;
	}
    
    OPLUS_DISP_INFO(TAG,"oplus_display_brightness= %u, en=%u\n", oplus_display_brightness, en);

	if(en == 1) {
		lcm_cmd_count = sizeof(lcm_finger_HBM_on_setting)/sizeof(struct LCM_setting_table);
        panel_send_pack_hs_cmd(dsi, lcm_finger_HBM_on_setting, lcm_cmd_count, cb, handle);
	} else if (en == 0) {
		level = oplus_display_brightness;
		oplus_panel_set_backlight_cmdq(dsi, cb, handle, oplus_display_brightness);
		OPLUS_DISP_DEBUG(TAG,": %u ! backlight %d !\n", en, oplus_display_brightness);
	}
	lcdinfo_notify(1, &en);

	OFP_DEBUG("end\n");

	return 0;
}


static struct LCM_setting_table lcm_aod_to_normal[] = {
	/* AOD mode OFF */
	{REGFLAG_CMD,4,{0xFF,0x78,0x38,0x00}},
	{REGFLAG_CMD,1,{0x38}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static int panel_doze_disable(struct drm_panel *panel, void *dsi, dcs_write_gce cb, void *handle)
{

	struct mtk_dsi *mtk_dsi = dsi;
	struct drm_crtc *crtc = NULL;
	struct mtk_crtc_state *mtk_state = NULL;
    unsigned int i=0;

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

/* Switch back to VDO mode */
	for (i = 0; i < (sizeof(lcm_aod_to_normal) / sizeof(struct LCM_setting_table)); i++) {
		unsigned cmd;
		cmd = lcm_aod_to_normal[i].cmd;

		switch (cmd) {

			case REGFLAG_DELAY:
				// if (handle == NULL) {
					usleep_range(lcm_aod_to_normal[i].count * 1000, lcm_aod_to_normal[i].count * 1000 + 100);
				// } else {
					// cmdq_pkt_sleep(handle, CMDQ_US_TO_TICK(lcm_aod_to_normal[i].count * 1000), CMDQ_GPR_R14);
				// }
				break;

			case REGFLAG_UDELAY:
				// if (handle == NULL) {
					usleep_range(lcm_aod_to_normal[i].count, lcm_aod_to_normal[i].count + 100);
				// } else {
					// cmdq_pkt_sleep(handle, CMDQ_US_TO_TICK(lcm_aod_to_normal[i].count), CMDQ_GPR_R14);
				// }
				break;

			case REGFLAG_END_OF_TABLE:
				break;

			default:
				cb(dsi, handle, lcm_aod_to_normal[i].para_list, lcm_aod_to_normal[i].count);
		}
	}
    
    aod_state = false;
    
    //todo  
    
	/* keep vcore off */
	//mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_SUSPEND_PWR_CTRL, MT_LPM_SMC_ACT_SET, PW_REG_SPM_VCORE_REQ, 0x0);
	lcm_setbacklight_cmdq(dsi, cb, handle, oplus_display_brightness);
	//OPLUS_DISP_INFO(TAG," call lpm_smc_spm_dbg keep vcore off for exit AOD!\n");
    //OPLUS_DISP_INFO(TAG,"%s:success\n", __func__);

	OFP_INFO("send aod off cmd\n");

	return 0;
}

static struct LCM_setting_table lcm_normal_to_aod[] = {
	 /* AOD Mode ON,DIC */
	 {REGFLAG_CMD,4,{0xFF,0x78,0x38,0x00}},
	 {REGFLAG_CMD, 1, {0x39}},
	 {REGFLAG_CMD,4,{0xFF,0x78,0x38,0x0C}},
	 {REGFLAG_CMD, 1, {0xBE,0x01}},
	 {REGFLAG_CMD,4,{0xFF,0x78,0x38,0x00}},
	 {REGFLAG_END_OF_TABLE, 0x00, {}}
};

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

	aod_state = true;

	for (i = 0; i < (sizeof(lcm_normal_to_aod) / sizeof(struct LCM_setting_table)); i++) {
		unsigned cmd;
		cmd = lcm_normal_to_aod[i].cmd;

		switch (cmd) {

			case REGFLAG_DELAY:
				msleep(lcm_normal_to_aod[i].count);
				break;

			case REGFLAG_UDELAY:
				udelay(lcm_normal_to_aod[i].count);
				break;

			case REGFLAG_END_OF_TABLE:
				break;

			default:
				cb(dsi, handle, lcm_normal_to_aod[i].para_list, lcm_normal_to_aod[i].count);
		}
	}
    
    //todo
    /* keep vcore on */
	//mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_SUSPEND_PWR_CTRL, MT_LPM_SMC_ACT_SET, PW_REG_SPM_VCORE_REQ, 0x1);
	//OPLUS_DISP_INFO(TAG,"call lpm_smc_spm_dbg keep vcore on for enter AOD then Succ!\n");

	OFP_INFO("send aod on cmd\n");

	return 0;
}

static struct LCM_setting_table lcm_aod_high_mode[] = {
	{REGFLAG_CMD, 4, {0xFF,0x78,0x38,0x0C}},
	{REGFLAG_CMD, 2, {0xBE,0x01}},
	{REGFLAG_CMD, 4, {0xFF,0x78,0x38,0x00}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_aod_low_mode[] = {
	{REGFLAG_CMD, 4, {0xFF,0x78,0x38,0x0C}},
	{REGFLAG_CMD, 2, {0xBE,0x02}},
	{REGFLAG_CMD, 4, {0xFF,0x78,0x38,0x00}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static int panel_set_aod_light_mode(void *dsi, dcs_write_gce cb, void *handle, unsigned int level)
{
	int i = 0;
	
	OPLUS_DISP_DEBUG(TAG,"+ %s\n",__func__);
	if (level == 0) {
		for (i = 0; i < sizeof(lcm_aod_high_mode)/sizeof(struct LCM_setting_table); i++){
			cb(dsi, handle, lcm_aod_high_mode[i].para_list, lcm_aod_high_mode[i].count);
		}
	} else {
		for (i = 0; i < sizeof(lcm_aod_low_mode)/sizeof(struct LCM_setting_table); i++){
			cb(dsi, handle, lcm_aod_low_mode[i].para_list, lcm_aod_low_mode[i].count);
		}
	}
	OPLUS_DISP_DEBUG(TAG,"%s:success %d !\n", __func__, level);
	OFP_INFO("success level = %d\n", level);

	return 0;
}


//#endif /* OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT */

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		OPLUS_DISP_INFO(TAG,"cannot get reset-gpios %ld\n",PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	return 0;
}


static struct regulator *mt6315_6_vbuck4;
static int vddr6_buck4_regulator_init(struct device *dev)
{
	static int regulator_inited;
	int ret = 0;

	if (regulator_inited)
		return ret;

    OPLUS_DISP_ERR(TAG,"get vddr6_buck4_regulator_init\n");

	/* please only get regulator once in a driver */
	mt6315_6_vbuck4 = devm_regulator_get(dev, "6_vbuck4");
	if (IS_ERR_OR_NULL(mt6315_6_vbuck4)) { /* handle return value */
		ret = PTR_ERR(mt6315_6_vbuck4);
		OPLUS_DISP_ERR(TAG,"get vddr6_buck4_optional fail, error: %d\n", ret);
		//return ret;
	}
	regulator_inited = 1;
	return ret; /* must be 0 */
}
static int vddr6_buck4_regulator_enable(struct device *dev)
{
	int ret = 0;
	int retval = 0;

	vddr6_buck4_regulator_init(dev);

	/* set voltage with min & max*/
	if (!IS_ERR_OR_NULL(mt6315_6_vbuck4)) {
		ret = regulator_set_voltage(mt6315_6_vbuck4, 1190000, 1193750);
		if (ret < 0)
			OPLUS_DISP_ERR(TAG,"set voltage mt6315_6_vbuck4 fail, ret = %d\n", ret);
		retval |= ret;
	}

	/* enable regulator */
	if (!IS_ERR_OR_NULL(mt6315_6_vbuck4)) {
		ret = regulator_enable(mt6315_6_vbuck4);
		if (ret < 0)
			OPLUS_DISP_ERR(TAG,"enable regulator mt6315_6_vbuck4 fail, ret = %d\n", ret);
		retval |= ret;
	}
	OPLUS_DISP_INFO(TAG,"get vddr6_buck4_regulator_enable\n");
	return retval;
}
static int vddr6_buck4_regulator_disable(struct device *dev)
{
	int ret = 0;
	int retval = 0;

	vddr6_buck4_regulator_init(dev);

	if (!IS_ERR_OR_NULL(mt6315_6_vbuck4)) {
		ret = regulator_disable(mt6315_6_vbuck4);
		if (ret < 0)
			OPLUS_DISP_ERR(TAG,"disable regulator mt6315_6_vbuck4 fail, ret = %d\n", ret);
		retval |= ret;
	}
	return retval;
}

//static int vddr6_buck4_regulator_get_voltage(struct device *dev)
//{
//	int ret = 0;
//	int retval = 0;
//
//	vddr6_buck4_regulator_init(dev);
//
//	/* get voltage */
//	if (!IS_ERR_OR_NULL(mt6315_6_vbuck4)) {
//		ret = regulator_get_voltage(mt6315_6_vbuck4);
//		if (ret < 0)
//			OPLUS_DISP_ERR(TAG,"get voltage mt6315_6_vbuck4 fail, ret = %d\n", ret);
//		retval |= ret;
//	}
//
//	OPLUS_DISP_INFO(TAG,"get vddr6_buck4_regulator_get_voltage = %d\n", retval);
//	return retval;
//}

static struct regulator *disp_ldo3;

static int lcm_panel_ldo3_regulator_init(struct device *dev)
{
	static int regulator_inited;
	int ret = 0;

	if (regulator_inited)
		return ret;
    pr_err("get lcm_panel_ldo3_regulator_init\n");

	/* please only get regulator once in a driver */
	disp_ldo3 = regulator_get(dev, "ldo3");
	if (IS_ERR(disp_ldo3)) { /* handle return value */
		ret = PTR_ERR(disp_ldo3);
		pr_err("get ldo3 fail, error: %d\n", ret);
		return ret;
	}
	regulator_inited = 1;
	return ret; /* must be 0 */

}

static int lcm_panel_ldo3_enable(struct device *dev)
{
	int ret = 0;
	int retval = 0;

	lcm_panel_ldo3_regulator_init(dev);

	/* set voltage with min & max*/
	ret = regulator_set_voltage(disp_ldo3, 3000000, 3000000);
	if (ret < 0)
		pr_err("set voltage disp_ldo3 fail, ret = %d\n", ret);
	retval |= ret;

	/* enable regulator */
	ret = regulator_enable(disp_ldo3);
	if (ret < 0)
		pr_err("enable regulator disp_ldo3 fail, ret = %d\n", ret);
	retval |= ret;
    pr_err("get lcm_panel_ldo3_enable\n");

	return retval;
}

static int lcm_panel_ldo3_disable(struct device *dev)
{
	int ret = 0;
	int retval = 0;

	lcm_panel_ldo3_regulator_init(dev);

	ret = regulator_disable(disp_ldo3);
	if (ret < 0)
		pr_err("disable regulator disp_ldo3 fail, ret = %d\n", ret);
	retval |= ret;
    pr_err("disable regulator disp_ldo3\n");

	return retval;
}

//static int lcm_panel_ldo3_read(struct device *dev)
//{
//	int ret = 0;
//	int retval = 0;
//
//	lcm_panel_ldo3_regulator_init(dev);
//
//	ret = regulator_get_voltage(disp_ldo3);
//	if (ret < 0)
//	DISP_ERR("get regulator disp_ldo3 fail, ret = %d\n", ret);
//	DISP_ERR("ldo3 value = %d\n", ret);
//	return retval;
//}


static int lcm_panel_poweron(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	OPLUS_DISP_DEBUG(TAG,"lcm ctx->prepared %d\n", ctx->prepared);

	if (ctx->prepared)
		return 0;
	/* VDDI pmic always enable */
	/* Wait > 0, actual 2ms */
	usleep_range(2000, 2100);
	/* VDDR LDO vBuck enable */
	vddr6_buck4_regulator_enable(ctx->dev);
	/* Wait > 0, actual 2ms */
	usleep_range(2000, 2100);
	/* VCI LDO enable */

	lcm_panel_ldo3_enable(ctx->dev);

	/* Wait > 10ms, actual 15ms */
	usleep_range(5000, 5100);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	OPLUS_DISP_INFO(TAG,"%s:success\n", __func__);
	return 0;
}

static int lcm_panel_poweroff(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	OPLUS_DISP_DEBUG(TAG,"lcm ctx->prepared %d \n",ctx->prepared);

	if (ctx->prepared)
		return 0;

	/* RST GPIO disable */
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	/* Wait > 0, actual 5ms */
	usleep_range(5000, 5100);
	/* VCI LDO disable */

	lcm_panel_ldo3_disable(ctx->dev);

	/* Wait > 0, actual 5ms */
	usleep_range(5000, 5100);
	// VDDR LDO vBuck disable
	vddr6_buck4_regulator_disable(ctx->dev);
	/* Wait > 0, actual 15ms */
	usleep_range(15000, 15100);
	/* VDDI pmic always enable */
	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);
	/* fast resume/suspend 70ms interval */
	usleep_range(70000, 70100);
	OPLUS_DISP_INFO(TAG,"%s:success\n", __func__);
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
	OPLUS_DISP_INFO(TAG,"%s: mode=%d, vrefresh=%d\n", __func__, mode, drm_mode_vrefresh(m));

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


static unsigned int last_fps_mode = 60;

static void mode_switch_to_120(struct drm_panel *panel, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (stage == AFTER_DSI_POWERON) {
		lcm_dcs_write_seq_static(ctx, 0xFF, 0x78, 0x38, 0x02);
		lcm_dcs_write_seq_static(ctx, 0x38, 0x11);
		lcm_dcs_write_seq_static(ctx, 0xFF, 0x78, 0x38, 0x00);
		last_fps_mode = 120;
		OPLUS_DISP_DEBUG(TAG,"set 120 success\n");
	}
}

static void mode_switch_to_90(struct drm_panel *panel, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (stage == BEFORE_DSI_POWERDOWN) {
		lcm_dcs_write_seq_static(ctx, 0xFF, 0x78, 0x38, 0x02);
		lcm_dcs_write_seq_static(ctx, 0x38, 0x12);
		lcm_dcs_write_seq_static(ctx, 0xFF, 0x78, 0x38, 0x00);
		if (last_fps_mode == 120) {
			usleep_range(8300, 8400);
		}
		last_fps_mode = 90;
		OPLUS_DISP_DEBUG(TAG,"set 90 success\n");
	}
}

static void mode_switch_to_60(struct drm_panel *panel, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (stage == BEFORE_DSI_POWERDOWN) {
		lcm_dcs_write_seq_static(ctx, 0xFF, 0x78, 0x38, 0x02);
		lcm_dcs_write_seq_static(ctx, 0x38, 0x13);
		lcm_dcs_write_seq_static(ctx, 0xFF, 0x78, 0x38, 0x00);
		if (last_fps_mode == 120) {
			usleep_range(8300, 8400);
		}
		last_fps_mode = 60;
		OPLUS_DISP_DEBUG(TAG,"set 60 success\n");
	}
}


static int mode_switch_hs(struct drm_panel *panel, struct drm_connector *connector,
		void *dsi_drv, unsigned int cur_mode, unsigned int dst_mode,
			enum MTK_PANEL_MODE_SWITCH_STAGE stage, dcs_write_gce_pack cb)
{
	int ret = 0;
	int m_vrefresh = 0;
	int src_vrefresh = 0;
	struct drm_display_mode *m = get_mode_by_id(connector, dst_mode);
	struct drm_display_mode *src_m = get_mode_by_id(connector, cur_mode);
	struct lcm *ctx = panel_to_lcm(panel);

	OPLUS_DISP_INFO(TAG,"lcm cur_mode = %d dst_mode %d.\n", cur_mode, dst_mode);

	if (cur_mode == dst_mode) {
		return ret;
	}

	
    m_vrefresh = drm_mode_vrefresh(m);
    src_vrefresh = drm_mode_vrefresh(src_m);

    if (m_vrefresh == 60) {
        mode_switch_to_60(panel, stage);
        OPLUS_DISP_INFO(TAG,"lcm timing switch to 60\n");
    } else if (m_vrefresh == 90) {
        mode_switch_to_90(panel, stage);
        OPLUS_DISP_INFO(TAG,"lcm timing switch to 90\n");
    } else if (m_vrefresh == 120) {
        mode_switch_to_120(panel, stage);
        OPLUS_DISP_INFO(TAG,"lcm timing switch to 120\n");
    }else{
        ret = 1;
    }

	//else if (stage == AFTER_DSI_POWERON) {
		ctx->m = m;
        
	return ret;
}

//static int oplus_lcm_dcs_read(struct lcm *ctx, u8 cmd, void *data, size_t len)
//{
//	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
//	ssize_t ret;
//
//	if (ctx->error < 0)
//		return 0;
//
//	ret = mipi_dsi_dcs_read(dsi, cmd, data, len);
//	if (ret < 0) {
//		DISP_ERR("error %ld reading dcs seq:(%#x)\n", ret, cmd);
//		ctx->error = ret;
//	}
//
//	return ret;
//}
//
//static void oplus_lcm_panel_get_data(struct lcm *ctx)
//{
//	u8 buffer[3] = { 0 };
//	static int ret;
//
//	lcm_dcs_write_seq_static(ctx, 0xFF, 0x78, 0x38, 0x00);
//	ret = oplus_lcm_dcs_read(ctx, 0x0E, buffer, 1);
//	DISP_INFO("return 0x0E %d data(0x%08x) to dsi engine\n",ret, buffer[0] | (buffer[1] << 8));
//}
//
//void lcm_read_get_info_pB(struct drm_panel *panel, int read_ic)
//{
//	unsigned int ret = 0;
//	struct lcm *ctx = panel_to_lcm(panel);
//
//	lcm_panel_ldo3_read(ctx->dev);
//
//	/* VDDR LDO GPIO enable */
//	ret = vddr6_buck4_regulator_get_voltage(ctx->dev);
//	if (ret < 0) {
//		DISP_ERR("vddr6_buck4_regulator_get_voltage get lV2 fail!\n");
//	} else {
//		DISP_INFO("vddr6_buck4_regulator_get_voltage = %d\n", ret);
//	}
//	/* Reset GPIO enable */
//	if (IS_ERR(ctx->reset_gpio))
//		DISP_ERR("%s, get reset gpio failed\n", __func__);
//	else {
//		ret = gpiod_get_value(ctx->reset_gpio);
//		DISP_INFO("%s, reset gpio = %d\n", __func__, ret);
//	}
//	pr_info("%s, read_ic = %d\n", __func__, read_ic);
//	/* DDIC register detect */
//	if (read_ic > 0) {
//		oplus_lcm_panel_get_data(ctx);
//	}
//	return;
//}


static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.panel_poweron = lcm_panel_poweron,
	.panel_poweroff = lcm_panel_poweroff,
	.ata_check = panel_ata_check,
	.ext_param_set = mtk_panel_ext_param_set,
	.mode_switch_hs = mode_switch_hs,
	.esd_backlight_recovery = oplus_esd_backlight_recovery,
	.oplus_hbm_set_cmdq = panel_hbm_set_cmdq,
	.oplus_set_hbm = lcm_set_hbm,
	.doze_enable = panel_doze_enable,
	.doze_disable = panel_doze_disable,
	.set_aod_light_mode = panel_set_aod_light_mode,
	.oplus_set_backlight_cmdq = oplus_panel_set_backlight_cmdq,
	//.oplus_get_info = lcm_read_get_info_pB,
};

static int get_mode_enum(struct drm_display_mode *m)
{
	int ret = 0;
	int m_vrefresh = 0;

	if (m == NULL) {
		OPLUS_DISP_INFO(TAG,"get_mode_enum drm_display_mode *m is null ,default 60fps\n");
		ret = 60;
		return ret;
	}

		m_vrefresh = drm_mode_vrefresh(m);
		OPLUS_DISP_INFO(TAG,"get_mode_enum : m_vrefresh=%d\n", m_vrefresh);
		if (m_vrefresh == 60) {
			ret = 60;
		} else if (m_vrefresh == 90) {
			ret = 90;
		} else if (m_vrefresh == 120) {
			ret = 120;
		} else {
			ret = 60;
		}
	return ret;
}

static int lcm_get_modes(struct drm_panel *panel, struct drm_connector *connector)
{
	struct drm_display_mode *mode[3];
    struct lcm *ctx = panel_to_lcm(panel);
	mode[0] = drm_mode_duplicate(connector->dev, &disp_mode_60Hz);
	if (!mode[0]) {
		OPLUS_DISP_ERR(TAG,"failed to add mode %ux%ux@%u\n", disp_mode_60Hz.hdisplay, disp_mode_60Hz.vdisplay, drm_mode_vrefresh(&disp_mode_60Hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode[0]);
	mode[0]->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode[0]);
	OPLUS_DISP_INFO(TAG,"drm_mode 1 : clock=%d,htotal=%d,vtotal=%d,hskew=%d,vrefresh=%d\n", mode[0]->clock, mode[0]->htotal,
		mode[0]->vtotal, mode[0]->hskew, drm_mode_vrefresh(mode[0]));

	mode[1] = drm_mode_duplicate(connector->dev, &disp_mode_90Hz);
	if (!mode[1]) {
		OPLUS_DISP_ERR(TAG,"failed to add mode %ux%ux@%u\n", disp_mode_90Hz.hdisplay, disp_mode_90Hz.vdisplay, drm_mode_vrefresh(&disp_mode_90Hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode[1]);
	OPLUS_DISP_INFO(TAG,"drm_mode 2 clock=%d,htotal=%d,vtotal=%d,hskew=%d,vrefresh=%d\n", mode[1]->clock, mode[1]->htotal,
		mode[1]->vtotal, mode[1]->hskew, drm_mode_vrefresh(mode[1]));
	mode[1]->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode[1]);

	mode[2] = drm_mode_duplicate(connector->dev, &disp_mode_120Hz);
	if (!mode[2]) {
		OPLUS_DISP_ERR(TAG,"failed to add mode %ux%ux@%u\n", disp_mode_120Hz.hdisplay, disp_mode_120Hz.vdisplay, drm_mode_vrefresh(&disp_mode_120Hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode[2]);
	OPLUS_DISP_INFO(TAG,"drm_mode 3 clock=%d,htotal=%d,vtotal=%d,hskew=%d,vrefresh=%d\n", mode[2]->clock, mode[2]->htotal,
		mode[2]->vtotal, mode[2]->hskew, drm_mode_vrefresh(mode[2]));
	mode[2]->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode[2]);

	connector->display_info.width_mm = 70;
	connector->display_info.height_mm = 155;

    if (!ctx->m) {
    	ctx->m = get_mode_by_id(connector, 0);
    	OPLUS_DISP_INFO(TAG,"ctx->m init: mode_id %d\n",get_mode_enum(ctx->m));
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

	OPLUS_DISP_INFO(TAG,"[LCM] %s+ ac168_p_B_a0012_cmd_panel Start\n", __func__);

	dsi_node = of_get_parent(dev->of_node);
	if (dsi_node) {
		endpoint = of_graph_get_next_endpoint(dsi_node, NULL);

		if (endpoint) {
			remote_node = of_graph_get_remote_port_parent(endpoint);
			if (!remote_node) {
				OPLUS_DISP_ERR(TAG,"No panel connected,skip probe lcm\n");
				return -ENODEV;
			}
			OPLUS_DISP_ERR(TAG,"device node name:%s\n", remote_node->name);
		}
	}
	if (remote_node != dev->of_node) {
		OPLUS_DISP_ERR(TAG,"skip probe due to not current lcm\n");
		return -ENODEV;
	}

	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	//ret = oplus_dsi_display_get_panel_info(ctx);
	//OPLUS_DISP_INFO(TAG,"[LCM]  Start\n");

	mipi_dsi_set_drvdata(dsi, ctx);
	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_NO_EOT_PACKET 
             | MIPI_DSI_CLOCK_NON_CONTINUOUS;

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight) {
			OPLUS_DISP_ERR(TAG,"skip probe due to lcm backlight null\n");
			return -EPROBE_DEFER;
		}
	}

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR_OR_NULL(ctx->reset_gpio)) {
		OPLUS_DISP_ERR(TAG,"cannot get reset-gpios %ld\n",
			 PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);

	usleep_range(5000, 5100);
	vddr6_buck4_regulator_enable(ctx->dev);

	usleep_range(5000, 5100);

	lcm_panel_ldo3_enable(ctx->dev);

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

	register_device_proc("lcd", "A0012", "P_B");
//#ifdef OPLUS_FEATURE_DISPLAY
	oplus_panel_parse(dev->of_node, TRUE);
//#endif /* OPLUS_FEATURE_DISPLAY */
	//flag_silky_panel = BL_SETTING_DELAY_60HZ;
	oplus_max_normal_brightness = MAX_NORMAL_BRIGHTNESS;

	//sscanf(oplus_lcm_id2, "%d", &lcm_id2);
	OPLUS_DISP_INFO(TAG,"[LCM] %s+ ac168_p_B_a0012_cmd_panel End\n", __func__);
	return ret;
}


static void lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);
//#if defined(CONFIG_MTK_PANEL_EXT)
//	struct mtk_panel_ctx *ext_ctx = find_panel_ctx(&ctx->panel);
//#endif

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
//#if defined(CONFIG_MTK_PANEL_EXT)
//	mtk_panel_detach(ext_ctx);
//	mtk_panel_remove(ext_ctx);
//#endif
}

static const struct of_device_id lcm_of_match[] = {
	{
	    .compatible = "ac168,p_B,a0012,cmd",
	},
	{}
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "ac168_p_B_a0012_cmd_panel",
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

MODULE_AUTHOR("oplus");
MODULE_DESCRIPTION("ac168,p_B,a0012,cmd,OLED Driver");
MODULE_LICENSE("GPL v2");
