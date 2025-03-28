// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
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
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <soc/oplus/device_info.h>

#define CONFIG_MTK_PANEL_EXT
#include "mtk_panel_ext.h"
#include "mtk_drm_graphics_base.h"
#include "mtk_boot_common.h"
#include "mtk_dsi.h"
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mediatek_v2/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif

#define REGFLAG_CMD				0xFFFA
#define REGFLAG_DELAY			0xFFFC
#define REGFLAG_UDELAY			0xFFFB
#define REGFLAG_END_OF_TABLE	0xFFFD

extern unsigned int oplus_display_brightness;
extern unsigned int oplus_max_normal_brightness;
extern unsigned int cabc_mode;

#if IS_ENABLED(CONFIG_OPLUS_MTK_DRM_GKI_NOTIFY)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#include "../mediatek/mediatek_v2/mtk_disp_notify.h"
#endif

#define LCD_CTL_RST_OFF 0x12
#define LCD_CTL_CS_OFF  0x1A
#define LCD_CTL_TP_LOAD_FW 0x10
#define LCD_CTL_CS_ON  0x19

#if IS_ENABLED(CONFIG_TOUCHPANEL_NOTIFY)
extern int (*tp_gesture_enable_notifier)(unsigned int tp_index);
#endif
static bool is_pd_with_guesture = false;

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *vdd18_gpio;
	struct gpio_desc *bias_pos, *bias_neg;

	bool prepared;
	bool enabled;

	unsigned int gate_ic;

	int error;
};

struct LCM_setting_table {
  	unsigned int cmd;
 	unsigned char count;
 	unsigned char para_list[128];
};

#if 0
static int blmap_table[] = {
                36, 8,
                16, 11,
                17, 12,
                19, 13,
                19, 15,
                20, 14,
                22, 14,
                22, 14,
                24, 10,
                24, 8 ,
                26, 4 ,
                27, 0 ,
                29, 9 ,
                29, 9 ,
                30, 14,
                33, 25,
                34, 30,
                36, 44,
                37, 49,
                40, 65,
                40, 69,
                43, 88,
                46, 109,
                47, 112,
                50, 135,
                53, 161,
                53, 163,
                60, 220,
                60, 223,
                64, 257,
                63, 255,
                71, 334,
                71, 331,
                75, 375,
                80, 422,
                84, 473,
                89, 529,
                88, 518,
                99, 653,
                98, 640,
                103, 707,
                117, 878,
                115, 862,
                122, 947,
                128, 1039,
                135, 1138,
                132, 1102,
                149, 1355,
                157, 1478,
                166, 1611,
                163, 1563,
                183, 1900,
                180, 1844,
                203, 2232,
                199, 2169,
                209, 2344,
                236, 2821,
                232, 2742,
                243, 2958,
                255, 3188,
                268, 3433,
                282, 3705,
                317, 4400,
                176, 1555};
#endif

/*
static struct LCM_setting_table set_dimming_off[] = {
	{0xFF, 0x01, {0x10}},
	{0xFB, 0x01, {0x01}},
	{0x53, 0x01, {0x24}}
};

static struct LCM_setting_table init_setting_cmd[] = {
	{ 0xFF, 0x03, {0x98, 0x07, 0x00} },
};
*/
/*
static struct LCM_setting_table bl_level[] = {
	 { 0xFF, 0x03, {0x98, 0x81, 0x00} },
	{0x51, 2, {0x00, 0xFF} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};
*/
#define lcm_dcs_write_seq(ctx, seq...) \
({\
	const u8 d[] = { seq };\
	BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64, "DCS sequence too big for stack");\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

#define lcm_dcs_write_seq_static(ctx, seq...) \
({\
	static const u8 d[] = { seq };\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

static inline struct lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct lcm, panel);
}

#if 1
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
		dev_err(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
}

static void lcm_mdelay(unsigned int ms)
{
	if (ms < 10)
		udelay(ms * 1000);
	else if (ms <= 20)
		usleep_range(ms*1000, (ms+1)*1000);
	else
		usleep_range(ms * 1000 - 100, ms * 1000);
}
#endif

/*static void push_table(struct lcm *ctx, struct LCM_setting_table *table, unsigned int count)
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
}*/

#if 1
static void lcm_panel_init(struct lcm *ctx)
	{

	gpiod_set_value(ctx->reset_gpio, 0);
	lcm_mdelay(3);
	gpiod_set_value(ctx->reset_gpio, 1);
	lcm_mdelay(15);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0x98, 0x83, 0x06);
	lcm_dcs_write_seq_static(ctx, 0x06, 0xA4);
	lcm_dcs_write_seq_static(ctx, 0x3E, 0xE2);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x98, 0x83, 0x030);
	lcm_dcs_write_seq_static(ctx, 0x83, 0x20);
	lcm_dcs_write_seq_static(ctx, 0x84, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x98, 0x83, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x86, 0x6C);
	lcm_dcs_write_seq_static(ctx, 0x88, 0xE1);
	lcm_dcs_write_seq_static(ctx, 0x89, 0xe8);
	lcm_dcs_write_seq_static(ctx, 0x8A, 0xF0);
	lcm_dcs_write_seq_static(ctx, 0x8B, 0xF7);
	lcm_dcs_write_seq_static(ctx, 0x8C, 0xBF);
	lcm_dcs_write_seq_static(ctx, 0x8D, 0xC5);
	lcm_dcs_write_seq_static(ctx, 0x8E, 0xC8);
	lcm_dcs_write_seq_static(ctx, 0x8F, 0xCE);
	lcm_dcs_write_seq_static(ctx, 0x90, 0xD1);
	lcm_dcs_write_seq_static(ctx, 0x91, 0xD6);
	lcm_dcs_write_seq_static(ctx, 0x92, 0xDC);
	lcm_dcs_write_seq_static(ctx, 0x93, 0xE3);
	lcm_dcs_write_seq_static(ctx, 0x94, 0xED);
	lcm_dcs_write_seq_static(ctx, 0x95, 0xFA);
	lcm_dcs_write_seq_static(ctx, 0xAF, 0x18);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x98, 0x83, 0x07);
	lcm_dcs_write_seq_static(ctx, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x01, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x98, 0x83, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x53, 0x24);
	lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x11, 0x00);

	lcm_mdelay(80);

	lcm_dcs_write_seq_static(ctx, 0x29, 0x00);

	lcm_mdelay(20);

}
#endif
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

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->prepared)
		return 0;

	lcm_dcs_write_seq_static(ctx, 0x28);
	lcm_mdelay(20);
	lcm_dcs_write_seq_static(ctx, 0x10);
	lcm_mdelay(120);

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

	pr_info("%s:success\n", __func__);
	return ret;
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

#define CALCULATE_CLOCK(FPS, VFP) \
			(((FPS) * (HAC + HFP + HSA + HBP) * (VAC + VFP + VSA + VBP)) / 1000)

#define HFP (20)
#define HSA (8)
#define HBP (10)
#define VFP_60HZ (1360)
#define VFP_90HZ (370)
#define VSA (2)
#define VBP (20)
#define VAC (1612)
#define HAC (720)

static struct drm_display_mode default_mode = {
	.clock = CALCULATE_CLOCK(60, VFP_60HZ),
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP_60HZ,
	.vsync_end = VAC + VFP_60HZ + VSA,
	.vtotal = VAC + VFP_60HZ + VSA + VBP,
};

static struct drm_display_mode performance_mode = {
	.clock = CALCULATE_CLOCK(90, VFP_90HZ),
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP_90HZ,
	.vsync_end = VAC + VFP_90HZ + VSA,
	.vtotal = VAC + VFP_90HZ + VSA + VBP,
};

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	gpiod_set_value(ctx->reset_gpio, on);

	return 0;

}

static int panel_ata_check(struct drm_panel *panel)
{
	/* Customer test by own ATA tool */
	return 1;
}

static int lcm_panel_poweron(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int blank;

#if IS_ENABLED(CONFIG_OPLUS_MTK_DRM_GKI_NOTIFY)
    blank = LCD_CTL_CS_ON;
    mtk_disp_notifier_call_chain(MTK_DISP_EVENT_FOR_TOUCH, &blank);
    pr_err("[TP]TP CS will chang to spi mode and high\n");
    usleep_range(5000, 5100);
    blank = LCD_CTL_TP_LOAD_FW;
    mtk_disp_notifier_call_chain(MTK_DISP_EVENT_FOR_TOUCH, &blank);
    pr_info("[TP] start to load fw!\n");
#endif

	gpiod_set_value(ctx->bias_pos, 1);
	lcm_mdelay(3);
	gpiod_set_value(ctx->bias_neg, 1);
	lcm_mdelay(15);

	pr_info("%s:success\n", __func__);
	return 0;
}

static int lcm_panel_poweroff(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int blank = 0;
	int flag_poweroff = 1;

	if (tp_gesture_enable_notifier && tp_gesture_enable_notifier(0)) {
		is_pd_with_guesture = true;
		flag_poweroff = 0;
		pr_err("[TP] tp gesture  is enable,Display not to poweroff\n");
	} else {
		is_pd_with_guesture = false;
		flag_poweroff = 1;
#if IS_ENABLED(CONFIG_OPLUS_MTK_DRM_GKI_NOTIFY)
		blank = LCD_CTL_RST_OFF;
		mtk_disp_notifier_call_chain(MTK_DISP_EVENT_FOR_TOUCH, &blank);
		pr_info("[TP] tp gesture is disable, Display goto power off , And TP reset will low\n");
		blank = LCD_CTL_CS_OFF;
		mtk_disp_notifier_call_chain(MTK_DISP_EVENT_FOR_TOUCH, &blank);
		pr_info("[TP]TP CS will change to gpio mode and low\n");
#endif
	}

	if(flag_poweroff == 1) {
		gpiod_set_value(ctx->bias_pos, 0);
		lcm_mdelay(2);
		gpiod_set_value(ctx->bias_neg, 0);
		lcm_mdelay(70);
	}

	pr_info("%s:success\n", __func__);
	return 0;
}


static struct LCM_setting_table bl_level[] = {
	{0x51, 2, {0x00, 0xFF} },
/*	{REGFLAG_CMD,3, {0x51, 0xi900, 0xFF} },*/
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};


static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{

	unsigned int mapped_level = 0;
    	char bl_tb0[] = {0x51, 0x07, 0xFF};

	if (!dsi || !cb) {
		return -EINVAL;
	}

	if (level == 1) {
		pr_info("enter aod\n");
		return 0;
	}
        pr_info("backlight =%d lcm_setbacklight_cmdq \n",level);

	if (level > 4095)
		level = 4095;

	if (!cb)
		return -1;

	if (level == 1) {
		pr_info("enter aod!!!\n");
		return 0;
	}

	bl_tb0[1] = level >> 8;
	bl_tb0[2] = level & 0xFF;
	mapped_level = level;
	if (mapped_level > 1) {
		//lcdinfo_notify(LCM_BRIGHTNESS_TYPE, &mapped_level);
	}
	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));

	oplus_display_brightness = level;
	pr_info("backlight=%d,paralist[1]=0x%x,paralist[2]=0x%x\n", level, bl_level[1].para_list[1], bl_level[1].para_list[2]);
	return 0;

}

static void lcm_cabc_mode_switch(void *dsi, dcs_write_gce cb,
		void *handle, unsigned int mode)
{
	struct mtk_dsi *dsi_ptr = (struct mtk_dsi *)dsi;
	struct lcm *ctx = panel_to_lcm(dsi_ptr->panel);


	pr_err("%s cabc = %d\n", __func__, mode);
	if (mode == 3) {
		mode = 2;
		pr_info("[lcm] cabc set level_2 %d\n", mode);
	}

	if (mode == 0) {
		lcm_dcs_write_seq_static(ctx, 0xFF, 0x98, 0x83, 0x00);
		lcm_dcs_write_seq_static(ctx, 0x55, 0x00);
	} else if (mode == 1) {
		lcm_dcs_write_seq_static(ctx, 0xFF, 0x98, 0x83, 0x00);
		lcm_dcs_write_seq_static(ctx, 0x55, 0x01);
	} else if (mode == 2) {
		lcm_dcs_write_seq_static(ctx, 0xFF, 0x98, 0x83, 0x00);
		lcm_dcs_write_seq_static(ctx, 0x55, 0x02);
	} else {
		pr_info("[lcm]  cabc_mode %d is not support\n", mode);
	}

	cabc_mode = mode;
}

static struct drm_display_mode *get_mode_by_id(struct drm_connector *connector,
	unsigned int mode)
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

static struct mtk_panel_params ext_params = {
	.pll_clk = 360,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.data_rate = 899,
	//.panel_bpp = 24,
	.dyn = {
		.switch_en = 1,
		.data_rate = 1086,
	},
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
		.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 90,
	},
};

static struct mtk_panel_params ext_params_90hz = {
	.pll_clk = 360,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.data_rate = 899,
	//.panel_bpp = 24,
	.dyn = {
		.switch_en = 1,
		.data_rate = 1086,
	},
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
		.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 90,
	},
};

static int mtk_panel_ext_param_set(struct drm_panel *panel,
	struct drm_connector *connector, unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(connector, mode);
	if (!m) {
		pr_info("[error]%s:%d invalid display_mode\n", __func__, __LINE__);
		return ret;
	}
	pr_info("set mode = %d Successful\n", mode);
	if (mode == 0)
		ext->params = &ext_params;
	else if (mode == 1)
		ext->params = &ext_params_90hz;
	if (drm_mode_vrefresh(m) == 60)
		ext->params = &ext_params;
	else if (drm_mode_vrefresh(m) == 90)
		ext->params = &ext_params_90hz;
	else
		ret = 1;

	return ret;
}

static int mtk_panel_ext_param_get(struct drm_panel *panel,
		struct drm_connector *connector,
		struct mtk_panel_params **ext_para,
		unsigned int mode)
{
	int ret = 0;
	pr_info("get mode = %d Successful\n", mode);
	if (mode == 0)
		*ext_para = &ext_params;
	else if (mode == 1)
		*ext_para = &ext_params_90hz;
	else
		ret = 1;

	return ret;

}

static struct mtk_panel_funcs ext_funcs = {
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.reset = panel_ext_reset,
	.panel_poweron = lcm_panel_poweron,
	.panel_poweroff = lcm_panel_poweroff,
	.ata_check = panel_ata_check,
	.cabc_switch = lcm_cabc_mode_switch,
	.ext_param_set = mtk_panel_ext_param_set,
	.ext_param_get = mtk_panel_ext_param_get,
};


static int lcm_get_modes(struct drm_panel *panel,
	struct drm_connector *connector)
{
	struct drm_display_mode *mode;
	struct drm_display_mode *mode2;

	mode = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode) {
		pr_err("failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	mode2 = drm_mode_duplicate(connector->dev, &performance_mode);
	if (!mode2) {
		pr_err("failed to add mode %ux%ux@%u\n",
			 performance_mode.hdisplay, performance_mode.vdisplay,
			 drm_mode_vrefresh(&performance_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode2);
	mode2->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode2);
	connector->display_info.width_mm = 70;
	connector->display_info.height_mm = 152;

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

	pr_info("[LCM] %s+ ac114_p_3_a0013_hd_dsi_vdo_lcm_drv Start\n", __func__);

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
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE
			 | MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_NO_EOT_PACKET
			 | MIPI_DSI_CLOCK_NON_CONTINUOUS;

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight) {
			pr_err("skip probe due to lcm backlight null\n");
			return -EPROBE_DEFER;
		}
	}

	ctx->vdd18_gpio = devm_gpiod_get(dev, "vdd18", GPIOD_OUT_HIGH);

	if (IS_ERR(ctx->vdd18_gpio)) {
		dev_err(ctx->dev, "%s: cannot get vdd18_gpio %ld\n",
			__func__, PTR_ERR(ctx->vdd18_gpio));
		return PTR_ERR(ctx->vdd18_gpio);
	}
	devm_gpiod_put(dev, ctx->vdd18_gpio);

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR_OR_NULL(ctx->reset_gpio)) {
		pr_err("cannot get reset-gpios %ld\n",
			 PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);

  	ctx->bias_pos = devm_gpiod_get_index(dev, "bias", 0, GPIOD_OUT_HIGH);
  	if (IS_ERR(ctx->bias_pos)) {
  		dev_err(dev, "%s: cannot get bias-pos 0 %ld\n",
  			__func__, PTR_ERR(ctx->bias_pos));
  		return PTR_ERR(ctx->bias_pos);
  	}
  	devm_gpiod_put(dev, ctx->bias_pos);

  	ctx->bias_neg = devm_gpiod_get_index(dev, "bias", 1, GPIOD_OUT_HIGH);
  	if (IS_ERR(ctx->bias_neg)) {
  		dev_err(dev, "%s: cannot get bias-neg 1 %ld\n",
  			__func__, PTR_ERR(ctx->bias_neg));
  		return PTR_ERR(ctx->bias_neg);
  	}
 	devm_gpiod_put(dev, ctx->bias_neg);

	//ctx->prepared = true;
	//ctx->enabled = true;

	ctx->panel.dev = dev;
	ctx->panel.funcs = &lcm_drm_funcs;

	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;

#endif
	register_device_proc("lcd", "A0013", "P_3");
	oplus_max_normal_brightness = 4095;

	pr_info("ac114_p_3_a0013_fhd_dsi_vdo_lcm_drv End.\n");

	return ret;
}


static void lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id lcm_of_match[] = {
	{ .compatible = "ac114,p_3,a0013,vdo", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "ac114_p_3_a0013_hd_dsi_vdo",
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

MODULE_AUTHOR("Adigarla Bhargav");
MODULE_DESCRIPTION("ac114_p_3_a0013 panel drm driver");
MODULE_LICENSE("GPL v2");

