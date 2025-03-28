// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/drm_modes.h>
#include <linux/delay.h>
#include <drm/drm_connector.h>
#include <drm/drm_device.h>
#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>
#include <linux/mtk_disp_notify.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <soc/oplus/device_info.h>
#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#include "nt36672c_tm_fhd_dsi_vdo_dphy.h"
#endif

#if defined(CONFIG_RT4831A_I2C)
#include "../../../misc/mediatek/gate_ic/gate_i2c.h"
#endif
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
//#include "AC102_P_7_A0007_data_hw_roundedpattern.h"
#endif
#define MAX_NORMAL_BRIGHTNESS    (3276)
/*
static struct LCM_UTIL_FUNCS lcm_util;
#define dsi_set_cmdq_backlight(cmdq, cmd, count, para_list, force_update) \
lcm_util.dsi_mt6382_send_cmd(cmdq, cmd, count, para_list, force_update)
*/
/*TP define*/
extern int (*tp_gesture_enable_notifier)(unsigned int tp_index);
#define LCD_CTL_TP_LOAD_FW 0x10
unsigned long esd_flag = 0;
unsigned int g_shutdown_flag = 0;
extern unsigned int oplus_display_brightness;
extern unsigned long oplus_max_normal_brightness;
unsigned int oplus_lcm_display_on;
extern void __attribute((weak)) lcd_queue_load_tp_fw(void) { return; };
extern int __attribute__((weak)) tp_gesture_enable_flag(void) {return 0;};
extern void __attribute__((weak)) tp_gpio_current_leakage_handler(bool normal) {return;};
//extern bool __attribute__((weak)) tp_boot_mode_normal() {return true;};

/* enable this to check panel self -bist pattern */
/* #define PANEL_BIST_PATTERN */

/* option function to read data from some panel address */
/* #define PANEL_SUPPORT_READBACK */

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *pm_enable_gpio;
	struct gpio_desc *bias_ldo,*bias_pos, *bias_neg;
	bool prepared;
	bool enabled;

	int error;
};

static unsigned int tm_bl_level = 0;
static int current_fps = 60;
//static int bl_gamma = 0;
static short cabc_status = 3;
static int esd_brightness;
//static bool dimming_is_on = false;
//static short frame_count = 0;

#define lcm_dcs_write_seq(ctx, seq...)                                     \
	({                                                                     \
		const u8 d[] = {seq};                                          \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		lcm_dcs_write(ctx, d, ARRAY_SIZE(d));                      \
	})

#define lcm_dcs_write_seq_static(ctx, seq...)                              \
	({                                                                     \
		static const u8 d[] = {seq};                                   \
		lcm_dcs_write(ctx, d, ARRAY_SIZE(d));                      \
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
		pr_info("[error]error %d reading dcs seq:(%#x)\n", ret, cmd);
		ctx->error = ret;
	}

	return ret;
}

static void lcm_panel_get_data(struct lcm *ctx)
{
	u8 buffer[3] = {0};
	static int ret;
	if (ret == 0) {
		ret = lcm_dcs_read(ctx, 0x0A, buffer, 1);
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
static struct LCD_setting_table lcm_suspend_setting[] = {
	{0xFF, 2, {0xFF, 0x10} },
	{0x28, 2, {0x28, 0x00} },

};

#endif
/*
static unsigned char dimming_level[2] = {
	0x02, 0x00
};*/
static unsigned char bl_level_mt6382[] = {
	0x51, 0x07, 0xFF
};
//static unsigned int bl_old_level = 0;

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
		pr_info("[error]error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
}

#define HFP_SUPPORT 0

static void lcm_init_set_cabc(struct lcm *ctx) {
	if (cabc_status == 1) {
		lcm_dcs_write_seq_static(ctx, 0x55, 0x01);
	} else if (cabc_status == 2) {
		lcm_dcs_write_seq_static(ctx, 0x55, 0x02);
	} else if (cabc_status == 3) {
		lcm_dcs_write_seq_static(ctx, 0x55, 0x03);
	} else if (cabc_status == 0) {
		lcm_dcs_write_seq_static(ctx, 0x55, 0x00);
	}

	pr_info("%s- cabc_init_mode=%d\n", __func__,cabc_status);
}

static void lcm_panel_init(struct lcm *ctx)
{
	int blank = 0;
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		pr_info("[error]%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return;
	}

	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(3 * 1000,3*1010);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10 * 1000,10 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(10 * 1000,10 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(15 * 1000,10 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	blank = LCD_CTL_TP_LOAD_FW;
	mtk_disp_notifier_call_chain(MTK_DISP_EVENT_FOR_TOUCH, &blank);
	/* Add for loading tp fw when screen ON*/
	//lcd_queue_load_tp_fw();
	//usleep_range(5 * 1000, 5 * 1010);
	//CABC
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x10);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x3B, 0x03, 0x14, 0x36, 0x04, 0x04);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xC2, 0x1B, 0xA0);
	lcm_dcs_write_seq_static(ctx, 0xE9, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x20);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x01, 0x66);
	lcm_dcs_write_seq_static(ctx, 0x06, 0x50);
	lcm_dcs_write_seq_static(ctx, 0x07, 0x28);
	lcm_dcs_write_seq_static(ctx, 0x17, 0x55);
	lcm_dcs_write_seq_static(ctx, 0x18, 0x66);
	lcm_dcs_write_seq_static(ctx, 0x1B, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x2F, 0x83);
	lcm_dcs_write_seq_static(ctx, 0x5C, 0x90);
	lcm_dcs_write_seq_static(ctx, 0x5E, 0xAA);
	lcm_dcs_write_seq_static(ctx, 0x69, 0xD0);
	lcm_dcs_write_seq_static(ctx, 0x6D, 0x55);
	lcm_dcs_write_seq_static(ctx, 0x89, 0x0D);
	lcm_dcs_write_seq_static(ctx, 0x8A, 0x0D);
	lcm_dcs_write_seq_static(ctx, 0x95, 0xD1);
	lcm_dcs_write_seq_static(ctx, 0x96, 0xD1);
	lcm_dcs_write_seq_static(ctx, 0xF2, 0x66);
	lcm_dcs_write_seq_static(ctx, 0xF3, 0x54);
	lcm_dcs_write_seq_static(ctx, 0xF4, 0x66);
	lcm_dcs_write_seq_static(ctx, 0xF5, 0x54);
	lcm_dcs_write_seq_static(ctx, 0xF6, 0x66);
	lcm_dcs_write_seq_static(ctx, 0xF7, 0x54);
	lcm_dcs_write_seq_static(ctx, 0xF8, 0x66);
	lcm_dcs_write_seq_static(ctx, 0xF9, 0x54);
	/* cabc begin */
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x23);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x00, 0x80);
	lcm_dcs_write_seq_static(ctx, 0x05, 0x22);
	lcm_dcs_write_seq_static(ctx, 0x06, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x07, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x08, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x09, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x10, 0x82);
	lcm_dcs_write_seq_static(ctx, 0x11, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x12, 0x95);
	lcm_dcs_write_seq_static(ctx, 0x15, 0x68);
	lcm_dcs_write_seq_static(ctx, 0x16, 0x0B);
	lcm_dcs_write_seq_static(ctx, 0x2B, 0x9E);
	lcm_dcs_write_seq_static(ctx, 0x30, 0xFB);
	lcm_dcs_write_seq_static(ctx, 0x31, 0xF6);
	lcm_dcs_write_seq_static(ctx, 0x32, 0xF5);
	lcm_dcs_write_seq_static(ctx, 0x33, 0xEF);
	lcm_dcs_write_seq_static(ctx, 0x34, 0xEC);
	lcm_dcs_write_seq_static(ctx, 0x35, 0xEA);
	lcm_dcs_write_seq_static(ctx, 0x36, 0xE9);
	usleep_range(1 * 1000,1 * 1000);
	lcm_dcs_write_seq_static(ctx, 0x36, 0xE9);
	lcm_dcs_write_seq_static(ctx, 0x37, 0xE8);
	lcm_dcs_write_seq_static(ctx, 0x38, 0xE6);
	lcm_dcs_write_seq_static(ctx, 0x39, 0xE3);
	lcm_dcs_write_seq_static(ctx, 0x3A, 0xDE);
	lcm_dcs_write_seq_static(ctx, 0x3B, 0xDE);
	lcm_dcs_write_seq_static(ctx, 0x3D, 0xDA);
	lcm_dcs_write_seq_static(ctx, 0x3F, 0xD9);
	lcm_dcs_write_seq_static(ctx, 0x40, 0xD8);
	lcm_dcs_write_seq_static(ctx, 0x41, 0xD7);
	lcm_dcs_write_seq_static(ctx, 0x45, 0xF1);
	lcm_dcs_write_seq_static(ctx, 0x46, 0xED);
	lcm_dcs_write_seq_static(ctx, 0x47, 0xEC);
	lcm_dcs_write_seq_static(ctx, 0x48, 0xE7);
	lcm_dcs_write_seq_static(ctx, 0x49, 0xE4);
	lcm_dcs_write_seq_static(ctx, 0x4A, 0xDC);
	lcm_dcs_write_seq_static(ctx, 0x4B, 0xD7);
	lcm_dcs_write_seq_static(ctx, 0x4C, 0xD5);
	lcm_dcs_write_seq_static(ctx, 0x4D, 0xCE);
	lcm_dcs_write_seq_static(ctx, 0x4E, 0xCD);
	lcm_dcs_write_seq_static(ctx, 0x4F, 0xC1);
	lcm_dcs_write_seq_static(ctx, 0x50, 0xBF);
	lcm_dcs_write_seq_static(ctx, 0x51, 0xBC);
	lcm_dcs_write_seq_static(ctx, 0x52, 0xBB);
	lcm_dcs_write_seq_static(ctx, 0x53, 0xBA);
	lcm_dcs_write_seq_static(ctx, 0x54, 0xB9);
	lcm_dcs_write_seq_static(ctx, 0x58, 0xDA);
	lcm_dcs_write_seq_static(ctx, 0x59, 0xD5);
	lcm_dcs_write_seq_static(ctx, 0x5A, 0xD3);
	lcm_dcs_write_seq_static(ctx, 0x5B, 0xC9);
	lcm_dcs_write_seq_static(ctx, 0x5C, 0xC3);
	lcm_dcs_write_seq_static(ctx, 0x5D, 0xBA);
	lcm_dcs_write_seq_static(ctx, 0x5E, 0xB6);
	lcm_dcs_write_seq_static(ctx, 0x5F, 0xB5);
	lcm_dcs_write_seq_static(ctx, 0x60, 0xAE);
	lcm_dcs_write_seq_static(ctx, 0x61, 0xAB);
	lcm_dcs_write_seq_static(ctx, 0x62, 0xAA);
	lcm_dcs_write_seq_static(ctx, 0x63, 0xA8);
	lcm_dcs_write_seq_static(ctx, 0x64, 0xA7);
	lcm_dcs_write_seq_static(ctx, 0x65, 0xA5);
	lcm_dcs_write_seq_static(ctx, 0x66, 0xA3);
	lcm_dcs_write_seq_static(ctx, 0x67, 0xA1);
	/* cabc end */
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x24);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x03, 0x13);
	lcm_dcs_write_seq_static(ctx, 0x04, 0x15);
	lcm_dcs_write_seq_static(ctx, 0x05, 0x17);
	lcm_dcs_write_seq_static(ctx, 0x09, 0x26);
	lcm_dcs_write_seq_static(ctx, 0x0A, 0x27);
	lcm_dcs_write_seq_static(ctx, 0x0B, 0x28);
	lcm_dcs_write_seq_static(ctx, 0x0C, 0x24);
	lcm_dcs_write_seq_static(ctx, 0x0D, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x0E, 0x2F);
	lcm_dcs_write_seq_static(ctx, 0x0F, 0x2D);
	lcm_dcs_write_seq_static(ctx, 0x10, 0x2E);
	lcm_dcs_write_seq_static(ctx, 0x11, 0x2C);
	lcm_dcs_write_seq_static(ctx, 0x12, 0x8B);
	lcm_dcs_write_seq_static(ctx, 0x13, 0x8C);
	lcm_dcs_write_seq_static(ctx, 0x16, 0x0F);
	lcm_dcs_write_seq_static(ctx, 0x17, 0x22);
	lcm_dcs_write_seq_static(ctx, 0x1B, 0x13);
	lcm_dcs_write_seq_static(ctx, 0x1C, 0x15);
	lcm_dcs_write_seq_static(ctx, 0x1D, 0x17);
	lcm_dcs_write_seq_static(ctx, 0x21, 0x26);
	lcm_dcs_write_seq_static(ctx, 0x22, 0x27);
	lcm_dcs_write_seq_static(ctx, 0x23, 0x28);
	lcm_dcs_write_seq_static(ctx, 0x24, 0x24);
	lcm_dcs_write_seq_static(ctx, 0x25, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x26, 0x2F);
	lcm_dcs_write_seq_static(ctx, 0x27, 0x2D);
	lcm_dcs_write_seq_static(ctx, 0x28, 0x2E);
	lcm_dcs_write_seq_static(ctx, 0x29, 0x2C);
	lcm_dcs_write_seq_static(ctx, 0x2A, 0x8B);
	lcm_dcs_write_seq_static(ctx, 0x2B, 0x8C);
	lcm_dcs_write_seq_static(ctx, 0x30, 0x0F);
	lcm_dcs_write_seq_static(ctx, 0x31, 0x22);
	lcm_dcs_write_seq_static(ctx, 0x32, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x33, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x35, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x36, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x4E, 0x32);
	lcm_dcs_write_seq_static(ctx, 0x4F, 0x32);
	lcm_dcs_write_seq_static(ctx, 0x53, 0x32);
	lcm_dcs_write_seq_static(ctx, 0x71, 0x28);
	lcm_dcs_write_seq_static(ctx, 0x77, 0x80);
	lcm_dcs_write_seq_static(ctx, 0x79, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x7A, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x7B, 0x8E);
	lcm_dcs_write_seq_static(ctx, 0x7D, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x80, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x81, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x82, 0x13);
	lcm_dcs_write_seq_static(ctx, 0x84, 0x31);
	lcm_dcs_write_seq_static(ctx, 0x85, 0x13);
	lcm_dcs_write_seq_static(ctx, 0x86, 0x22);
	lcm_dcs_write_seq_static(ctx, 0x87, 0x31);
	lcm_dcs_write_seq_static(ctx, 0x90, 0x13);
	lcm_dcs_write_seq_static(ctx, 0x92, 0x31);
	lcm_dcs_write_seq_static(ctx, 0x93, 0x13);
	lcm_dcs_write_seq_static(ctx, 0x94, 0x22);
	lcm_dcs_write_seq_static(ctx, 0x95, 0x31);
	lcm_dcs_write_seq_static(ctx, 0x9C, 0xF4);
	lcm_dcs_write_seq_static(ctx, 0x9D, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xA0, 0x0E);
	lcm_dcs_write_seq_static(ctx, 0xA2, 0x0E);
	lcm_dcs_write_seq_static(ctx, 0xA3, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xA4, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xA5, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xC4, 0x40);
	lcm_dcs_write_seq_static(ctx, 0xC6, 0xC0);
	lcm_dcs_write_seq_static(ctx, 0xC9, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xD9, 0x80);
	lcm_dcs_write_seq_static(ctx, 0xE9, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x25);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x0F, 0x13);
	lcm_dcs_write_seq_static(ctx, 0x18, 0x22);
	lcm_dcs_write_seq_static(ctx, 0x19, 0xE4);
	lcm_dcs_write_seq_static(ctx, 0x20, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x21, 0x40);
	lcm_dcs_write_seq_static(ctx, 0x66, 0x40);
	lcm_dcs_write_seq_static(ctx, 0x67, 0x29);
	lcm_dcs_write_seq_static(ctx, 0x68, 0x50);
	lcm_dcs_write_seq_static(ctx, 0x69, 0x20);
	lcm_dcs_write_seq_static(ctx, 0x6B, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x71, 0x6D);
	lcm_dcs_write_seq_static(ctx, 0x77, 0x60);
	lcm_dcs_write_seq_static(ctx, 0x78, 0xA5);
	lcm_dcs_write_seq_static(ctx, 0x7D, 0x40);
	lcm_dcs_write_seq_static(ctx, 0x7E, 0x2D);
	lcm_dcs_write_seq_static(ctx, 0x84, 0x70);
	lcm_dcs_write_seq_static(ctx, 0xC0, 0x4D);
	lcm_dcs_write_seq_static(ctx, 0xC1, 0xA9);
	lcm_dcs_write_seq_static(ctx, 0xC2, 0xD2);
	lcm_dcs_write_seq_static(ctx, 0xC4, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xD6, 0x80);
	lcm_dcs_write_seq_static(ctx, 0xD7, 0x82);
	lcm_dcs_write_seq_static(ctx, 0xDA, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xDD, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xE0, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xF1, 0x04);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x26);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x00, 0x10);
	lcm_dcs_write_seq_static(ctx, 0x01, 0xED);
	lcm_dcs_write_seq_static(ctx, 0x03, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x04, 0xED);
	lcm_dcs_write_seq_static(ctx, 0x05, 0x08);
	lcm_dcs_write_seq_static(ctx, 0x06, 0x0E);
	lcm_dcs_write_seq_static(ctx, 0x08, 0x0E);
	lcm_dcs_write_seq_static(ctx, 0x14, 0x06);
	lcm_dcs_write_seq_static(ctx, 0x15, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x74, 0xAF);
	lcm_dcs_write_seq_static(ctx, 0x81, 0x0D);
	lcm_dcs_write_seq_static(ctx, 0x83, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x85, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x86, 0x04);
	lcm_dcs_write_seq_static(ctx, 0x87, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x88, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x8A, 0x1A);
	lcm_dcs_write_seq_static(ctx, 0x8B, 0x11);
	lcm_dcs_write_seq_static(ctx, 0x8C, 0x24);
	lcm_dcs_write_seq_static(ctx, 0x8E, 0x42);
	lcm_dcs_write_seq_static(ctx, 0x8F, 0x11);
	lcm_dcs_write_seq_static(ctx, 0x90, 0x11);
	lcm_dcs_write_seq_static(ctx, 0x91, 0x11);
	lcm_dcs_write_seq_static(ctx, 0x9A, 0x80);
	lcm_dcs_write_seq_static(ctx, 0x9B, 0x04);
	lcm_dcs_write_seq_static(ctx, 0x9C, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x9D, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x9E, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x27);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x01, 0x60);
	lcm_dcs_write_seq_static(ctx, 0x20, 0x81);
	lcm_dcs_write_seq_static(ctx, 0x21, 0x6D);
	lcm_dcs_write_seq_static(ctx, 0x25, 0x81);
	lcm_dcs_write_seq_static(ctx, 0x26, 0xAB);
	lcm_dcs_write_seq_static(ctx, 0x6E, 0x23);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x70, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x71, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x72, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x73, 0x21);
	lcm_dcs_write_seq_static(ctx, 0x74, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x75, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x76, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x77, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x7D, 0x09);
	lcm_dcs_write_seq_static(ctx, 0x7E, 0x63);
	lcm_dcs_write_seq_static(ctx, 0x80, 0x23);
	lcm_dcs_write_seq_static(ctx, 0x82, 0x09);
	lcm_dcs_write_seq_static(ctx, 0x83, 0x63);
	lcm_dcs_write_seq_static(ctx, 0x88, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x89, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xE3, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xE4, 0xDA);
	lcm_dcs_write_seq_static(ctx, 0xE5, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xE6, 0xE7);
	lcm_dcs_write_seq_static(ctx, 0xE9, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xEA, 0x56);
	lcm_dcs_write_seq_static(ctx, 0xEB, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xEC, 0x39);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x2A);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x00, 0x91);
	lcm_dcs_write_seq_static(ctx, 0x03, 0x20);
	lcm_dcs_write_seq_static(ctx, 0x04, 0x78);
	lcm_dcs_write_seq_static(ctx, 0x07, 0x50);
	lcm_dcs_write_seq_static(ctx, 0x0A, 0x70);
	lcm_dcs_write_seq_static(ctx, 0x0C, 0x09);
	lcm_dcs_write_seq_static(ctx, 0x0D, 0x40);
	lcm_dcs_write_seq_static(ctx, 0x0E, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x0F, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x11, 0xF0);
	lcm_dcs_write_seq_static(ctx, 0x15, 0x0E);
	lcm_dcs_write_seq_static(ctx, 0x16, 0xCF);
	lcm_dcs_write_seq_static(ctx, 0x19, 0x0E);
	lcm_dcs_write_seq_static(ctx, 0x1A, 0xA3);
	lcm_dcs_write_seq_static(ctx, 0x1B, 0x14);
	lcm_dcs_write_seq_static(ctx, 0x1D, 0x36);
	lcm_dcs_write_seq_static(ctx, 0x1E, 0x37);
	lcm_dcs_write_seq_static(ctx, 0x1F, 0x37);
	lcm_dcs_write_seq_static(ctx, 0x20, 0x37);
	lcm_dcs_write_seq_static(ctx, 0x28, 0xDF);
	lcm_dcs_write_seq_static(ctx, 0x29, 0x1F);
	lcm_dcs_write_seq_static(ctx, 0x2A, 0xFF);
	lcm_dcs_write_seq_static(ctx, 0x2D, 0x05);
	lcm_dcs_write_seq_static(ctx, 0x2F, 0x06);
	lcm_dcs_write_seq_static(ctx, 0x30, 0x1E);
	lcm_dcs_write_seq_static(ctx, 0x31, 0x8C);
	lcm_dcs_write_seq_static(ctx, 0x33, 0x1E);
	lcm_dcs_write_seq_static(ctx, 0x34, 0xE1);
	lcm_dcs_write_seq_static(ctx, 0x35, 0x44);
	lcm_dcs_write_seq_static(ctx, 0x36, 0xCC);
	lcm_dcs_write_seq_static(ctx, 0x37, 0xDA);
	lcm_dcs_write_seq_static(ctx, 0x38, 0x4A);
	lcm_dcs_write_seq_static(ctx, 0x39, 0xC6);
	lcm_dcs_write_seq_static(ctx, 0x3A, 0x1E);
	lcm_dcs_write_seq_static(ctx, 0x45, 0x0E);
	lcm_dcs_write_seq_static(ctx, 0x46, 0x40);
	lcm_dcs_write_seq_static(ctx, 0x47, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x4A, 0xA0);
	lcm_dcs_write_seq_static(ctx, 0x4E, 0x0E);
	lcm_dcs_write_seq_static(ctx, 0x4F, 0xCF);
	lcm_dcs_write_seq_static(ctx, 0x52, 0x0E);
	lcm_dcs_write_seq_static(ctx, 0x53, 0xA3);
	lcm_dcs_write_seq_static(ctx, 0x54, 0x14);
	lcm_dcs_write_seq_static(ctx, 0x56, 0x36);
	lcm_dcs_write_seq_static(ctx, 0x57, 0x76);
	lcm_dcs_write_seq_static(ctx, 0x58, 0x76);
	lcm_dcs_write_seq_static(ctx, 0x59, 0x76);
	lcm_dcs_write_seq_static(ctx, 0x60, 0x80);
	lcm_dcs_write_seq_static(ctx, 0x61, 0x8E);
	lcm_dcs_write_seq_static(ctx, 0x62, 0x0B);
	lcm_dcs_write_seq_static(ctx, 0x63, 0xA1);
	lcm_dcs_write_seq_static(ctx, 0x65, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x66, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x67, 0x0F);
	lcm_dcs_write_seq_static(ctx, 0x68, 0x68);
	lcm_dcs_write_seq_static(ctx, 0x6A, 0xCE);
	lcm_dcs_write_seq_static(ctx, 0x6B, 0x90);
	lcm_dcs_write_seq_static(ctx, 0x6C, 0x20);
	lcm_dcs_write_seq_static(ctx, 0x6D, 0xA6);
	lcm_dcs_write_seq_static(ctx, 0x6E, 0x8D);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x22);
	lcm_dcs_write_seq_static(ctx, 0x70, 0xA4);
	lcm_dcs_write_seq_static(ctx, 0x71, 0x0F);
	lcm_dcs_write_seq_static(ctx, 0x79, 0x4C);
	lcm_dcs_write_seq_static(ctx, 0x7A, 0x09);
	lcm_dcs_write_seq_static(ctx, 0x7B, 0x40);
	lcm_dcs_write_seq_static(ctx, 0x7F, 0xF0);
	lcm_dcs_write_seq_static(ctx, 0x83, 0x0E);
	lcm_dcs_write_seq_static(ctx, 0x84, 0xCF);
	lcm_dcs_write_seq_static(ctx, 0x87, 0x0E);
	lcm_dcs_write_seq_static(ctx, 0x88, 0xA3);
	lcm_dcs_write_seq_static(ctx, 0x89, 0x14);
	lcm_dcs_write_seq_static(ctx, 0x8B, 0x36);
	lcm_dcs_write_seq_static(ctx, 0x8C, 0x4E);
	lcm_dcs_write_seq_static(ctx, 0x8D, 0x4E);
	lcm_dcs_write_seq_static(ctx, 0x8E, 0x4E);
	lcm_dcs_write_seq_static(ctx, 0x95, 0x80);
	lcm_dcs_write_seq_static(ctx, 0x96, 0xDF);
	lcm_dcs_write_seq_static(ctx, 0x97, 0x17);
	lcm_dcs_write_seq_static(ctx, 0x98, 0xA2);
	lcm_dcs_write_seq_static(ctx, 0x9A, 0x05);
	lcm_dcs_write_seq_static(ctx, 0x9B, 0x04);
	lcm_dcs_write_seq_static(ctx, 0x9C, 0x14);
	lcm_dcs_write_seq_static(ctx, 0x9D, 0xA5);
	lcm_dcs_write_seq_static(ctx, 0x9F, 0xD1);
	lcm_dcs_write_seq_static(ctx, 0xA0, 0xE1);
	lcm_dcs_write_seq_static(ctx, 0xA2, 0x30);
	lcm_dcs_write_seq_static(ctx, 0xA3, 0xFD);
	lcm_dcs_write_seq_static(ctx, 0xA4, 0xDC);
	lcm_dcs_write_seq_static(ctx, 0xA5, 0x34);
	lcm_dcs_write_seq_static(ctx, 0xA6, 0xF9);
	lcm_dcs_write_seq_static(ctx, 0xA7, 0x14);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x2C);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x00, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x01, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x02, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x03, 0x21);
	lcm_dcs_write_seq_static(ctx, 0x04, 0x21);
	lcm_dcs_write_seq_static(ctx, 0x05, 0x21);
	lcm_dcs_write_seq_static(ctx, 0x0D, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x0E, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x17, 0x73);
	lcm_dcs_write_seq_static(ctx, 0x18, 0x73);
	lcm_dcs_write_seq_static(ctx, 0x19, 0x73);
	lcm_dcs_write_seq_static(ctx, 0x2D, 0xAF);
	lcm_dcs_write_seq_static(ctx, 0x2F, 0x10);
	lcm_dcs_write_seq_static(ctx, 0x30, 0xED);
	lcm_dcs_write_seq_static(ctx, 0x32, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x33, 0xED);
	lcm_dcs_write_seq_static(ctx, 0x35, 0x1E);
	lcm_dcs_write_seq_static(ctx, 0x37, 0x1E);
	lcm_dcs_write_seq_static(ctx, 0x4D, 0x21);
	lcm_dcs_write_seq_static(ctx, 0x4E, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x4F, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x53, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x54, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x55, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x56, 0x13);
	lcm_dcs_write_seq_static(ctx, 0x58, 0x13);
	lcm_dcs_write_seq_static(ctx, 0x59, 0x13);
	lcm_dcs_write_seq_static(ctx, 0x61, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x62, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x6B, 0x46);
	lcm_dcs_write_seq_static(ctx, 0x6C, 0x46);
	lcm_dcs_write_seq_static(ctx, 0x6D, 0x46);
	lcm_dcs_write_seq_static(ctx, 0x80, 0xAF);
	lcm_dcs_write_seq_static(ctx, 0x81, 0x10);
	lcm_dcs_write_seq_static(ctx, 0x82, 0xED);
	lcm_dcs_write_seq_static(ctx, 0x84, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x85, 0xED);
	lcm_dcs_write_seq_static(ctx, 0x87, 0x14);
	lcm_dcs_write_seq_static(ctx, 0x89, 0x14);
	lcm_dcs_write_seq_static(ctx, 0x9D, 0x13);
	lcm_dcs_write_seq_static(ctx, 0x9E, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x9F, 0x05);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0xE0);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x35, 0x82);
	lcm_dcs_write_seq_static(ctx, 0x9F, 0x00); /* 2022/12/16 nine stripes */
	lcm_dcs_write_seq_static(ctx, 0x9C, 0x00); /* 2022/12/16 nine stripes */
	lcm_dcs_write_seq_static(ctx, 0xFF, 0xF0);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x1C, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x33, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x5A, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x9C, 0x17);
	lcm_dcs_write_seq_static(ctx, 0x9F, 0x19);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0xD0);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x53, 0x22);
	lcm_dcs_write_seq_static(ctx, 0x54, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0xC0);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x9C, 0x11);
	lcm_dcs_write_seq_static(ctx, 0x9D, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x2B);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xB7, 0x2A);
	lcm_dcs_write_seq_static(ctx, 0xB8, 0x0C);
	lcm_dcs_write_seq_static(ctx, 0xC0, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x10);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xC1, 0x89, 0x28, 0x00, 0x08, 0x00, 0xAA, 0x02, 0x0E, 0x00, 0x2B, 0x00, 0x07, 0x0D, 0xB7, 0x0C, 0xB7);
	usleep_range(5 * 1000,5 * 1010);
	lcm_dcs_write_seq_static(ctx, 0xC2, 0x1B, 0xA0);
	usleep_range(5 * 1000,5 * 100);
/*	lcm_dcs_write_seq_static(ctx, 0x35, 0x00);*/
	lcm_dcs_write_seq_static(ctx, 0x68, 0x02, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x51, 0x01, 0xFF);
	lcm_dcs_write_seq_static(ctx, 0x53, 0x2C);
	lcm_dcs_write_seq_static(ctx, 0x55, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x11, 0x00);
	usleep_range(120 * 1000,120 * 1010);
	lcm_dcs_write_seq_static(ctx, 0x29, 0x00);
	usleep_range(20 * 1000,20 * 1010);
	//lcm_init_set_cabc(ctx, cabc_status);
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

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int flag_poweroff = 1;
	if (!ctx->prepared)
		return 0;

	printk("tm_nt36672c_lcm_shudown_power\n");
	//tp_control_reset_gpio(false);

	lcm_dcs_write_seq_static(ctx, 0x28);
	usleep_range(20 * 1000, 20 * 1010);
	lcm_dcs_write_seq_static(ctx, 0x10);
	//usleep_range(10 * 1000, 10 * 1010);
	pr_info("[TP] tp_gesture_enable_notifier = %p, tp_gesture_enable_notifier(0) = %d \n", tp_gesture_enable_notifier, tp_gesture_enable_notifier(0));
	if (tp_gesture_enable_notifier && tp_gesture_enable_notifier(0)) {
                if (g_shutdown_flag == 1) {
                        flag_poweroff = 1;
                } else {
                        flag_poweroff = 0;
                        pr_err("[TP] tp gesture  is enable,Display not to poweroff\n");
                }
	} else {
		flag_poweroff = 1;
	}
	pr_info("[TP] tp_gesture_enable_flag = %d, g_shutdown_flag = %d, esd_flag = %lu \n", tp_gesture_enable_flag(), g_shutdown_flag, esd_flag);
	if (1 == flag_poweroff) {
		ctx->reset_gpio =
                        devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
                if (IS_ERR(ctx->reset_gpio)) {
                        pr_info("[error]%s: cannot get reset_gpio %ld\n",
                                __func__, PTR_ERR(ctx->reset_gpio));
                        return PTR_ERR(ctx->reset_gpio);
                }
                gpiod_set_value(ctx->reset_gpio, 0);
                devm_gpiod_put(ctx->dev, ctx->reset_gpio);


		if (0 == tp_gesture_enable_flag() || (esd_flag == 1)) {
			ctx->reset_gpio =
                        	devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
                	gpiod_set_value(ctx->reset_gpio, 0);
        	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

			usleep_range(2*1000,2*1010);

			ctx->bias_pos = devm_gpiod_get_index(ctx->dev,
				"bias", 1, GPIOD_OUT_HIGH);
			if (IS_ERR(ctx->bias_pos)) {
				pr_info("[error]%s: cannot get bias_pos %ld\n",
					__func__, PTR_ERR(ctx->bias_pos));
				return PTR_ERR(ctx->bias_pos);
			}
			gpiod_set_value(ctx->bias_pos, 0);
			devm_gpiod_put(ctx->dev, ctx->bias_pos);

			usleep_range(6*1000,6*1010);

			ctx->bias_neg = devm_gpiod_get_index(ctx->dev,
				"bias", 0, GPIOD_OUT_HIGH);
			if (IS_ERR(ctx->bias_neg)) {
				pr_info("[error]%s: cannot get bias_neg %ld\n",
					__func__, PTR_ERR(ctx->bias_neg));
				return PTR_ERR(ctx->bias_neg);
			}
			gpiod_set_value(ctx->bias_neg, 0);
			devm_gpiod_put(ctx->dev, ctx->bias_neg);
		}
	}
	oplus_lcm_display_on = 0;
	ctx->error = 0;
	ctx->prepared = false;
	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;
	if (ctx->prepared)
		return 0;

	ctx->bias_pos =
		devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->bias_pos, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);

	usleep_range(3000, 3010);// 3ms
	ctx->bias_neg =
		devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->bias_neg, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);
	usleep_range(2 * 1000, 2 * 1000+100);// 2ms

	//if (tp_boot_mode_normal()) {
		tp_gpio_current_leakage_handler(true);
	//}

	lcm_panel_init(ctx);
	lcm_init_set_cabc(ctx);
	oplus_lcm_display_on = 1;
	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;
#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_rst(panel);
#endif

#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif
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

static const struct drm_display_mode default_mode = {
	.clock = ((FRAME_WIDTH + HFP + HSA + HBP) * (FRAME_HEIGHT + MODE_0_VFP + VSA + VBP) * MODE_0_FPS) / 1000,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + HFP,
	.hsync_end = FRAME_WIDTH + HFP + HSA,
	.htotal = FRAME_WIDTH + HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_0_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_0_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_0_VFP + VSA + VBP,
};

static const struct drm_display_mode performance_mode_1 = {
	.clock = ((FRAME_WIDTH + HFP + HSA + HBP) * (FRAME_HEIGHT + MODE_1_VFP + VSA + VBP) * MODE_1_FPS) / 1000,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + HFP,
	.hsync_end = FRAME_WIDTH + HFP + HSA,
	.htotal = FRAME_WIDTH + HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_1_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_1_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_1_VFP + VSA + VBP,
};

static const struct drm_display_mode performance_mode_2 = {
	.clock = ((FRAME_WIDTH + HFP + HSA + HBP) * (FRAME_HEIGHT + MODE_2_VFP + VSA + VBP) * MODE_2_FPS) / 1000,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + HFP,
	.hsync_end = FRAME_WIDTH + HFP + HSA,
	.htotal = FRAME_WIDTH + HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_2_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_2_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_2_VFP + VSA + VBP,
};
#if 0
static const struct drm_display_mode performance_mode_3 = {
	.clock = ((FRAME_WIDTH + HFP + HSA + HBP) * (FRAME_HEIGHT + MODE_3_VFP + VSA + VBP) * MODE_3_FPS) / 1000,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + HFP,
	.hsync_end = FRAME_WIDTH + HFP + HSA,
	.htotal = FRAME_WIDTH + HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_3_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_3_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_3_VFP + VSA + VBP,
};

static const struct drm_display_mode performance_mode_4 = {
	.clock = ((FRAME_WIDTH + HFP + HSA + HBP) * (FRAME_HEIGHT + MODE_4_VFP + VSA + VBP) * MODE_4_FPS) / 1000,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + HFP,
	.hsync_end = FRAME_WIDTH + HFP + HSA,
	.htotal = FRAME_WIDTH + HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_4_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_4_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_4_VFP + VSA + VBP,
};
#endif

#if defined(CONFIG_MTK_PANEL_EXT)

static struct mtk_panel_params ext_params = {//60hz
	//.vfp_low_power = 4120,
	//.cust_esd_check = 0,
	.esd_check_enable = 0,

	.lcm_esd_check_table[0] = {
		.cmd = 0x0A,
		.count = 1,
		.para_list[0] = 0x9C,
	},
	.oplus_esd_sleep_status = true,
	.oplus_esd_sleep_ms = 5000,
#if 0
	.lcm_esd_check_table[1] = {
		.cmd = 0xAB,
		.count = 2,
		.para_list[0] = 0x00,
		.para_list[1] = 0x00,
	},
#endif
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.dsc_params = {
		.bdg_dsc_enable = 1,
		.ver                   =  DSC_VER,
		.slice_mode            =  DSC_SLICE_MODE,
		.rgb_swap              =  DSC_RGB_SWAP,
		.dsc_cfg               =  DSC_DSC_CFG,
		.rct_on                =  DSC_RCT_ON,
		.bit_per_channel       =  DSC_BIT_PER_CHANNEL,
		.dsc_line_buf_depth    =  DSC_DSC_LINE_BUF_DEPTH,
		.bp_enable             =  DSC_BP_ENABLE,
		.bit_per_pixel         =  DSC_BIT_PER_PIXEL,
		.pic_height            =  FRAME_HEIGHT,
		.pic_width             =  FRAME_WIDTH,
		.slice_height          =  DSC_SLICE_HEIGHT,
		.slice_width           =  DSC_SLICE_WIDTH,
		.chunk_size            =  DSC_CHUNK_SIZE,
		.xmit_delay            =  DSC_XMIT_DELAY,
		.dec_delay             =  DSC_DEC_DELAY,
		.scale_value           =  DSC_SCALE_VALUE,
		.increment_interval    =  DSC_INCREMENT_INTERVAL,
		.decrement_interval    =  DSC_DECREMENT_INTERVAL,
		.line_bpg_offset       =  DSC_LINE_BPG_OFFSET,
		.nfl_bpg_offset        =  DSC_NFL_BPG_OFFSET,
		.slice_bpg_offset      =  DSC_SLICE_BPG_OFFSET,
		.initial_offset        =  DSC_INITIAL_OFFSET,
		.final_offset          =  DSC_FINAL_OFFSET,
		.flatness_minqp        =  DSC_FLATNESS_MINQP,
		.flatness_maxqp        =  DSC_FLATNESS_MAXQP,
		.rc_model_size         =  DSC_RC_MODEL_SIZE,
		.rc_edge_factor        =  DSC_RC_EDGE_FACTOR,
		.rc_quant_incr_limit0  =  DSC_RC_QUANT_INCR_LIMIT0,
		.rc_quant_incr_limit1  =  DSC_RC_QUANT_INCR_LIMIT1,
		.rc_tgt_offset_hi      =  DSC_RC_TGT_OFFSET_HI,
		.rc_tgt_offset_lo      =  DSC_RC_TGT_OFFSET_LO,
	},

	.data_rate = DATA_RATE,
	//.pll_clk = MIPI_CLK,
/*
	.phy_timcon = {
		.hs_trail = 9,
		.clk_trail = 10,
	},
*/
	.bdg_ssc_enable = 0,
	.ssc_enable = 0,
/*
	.dyn = {
		.switch_en = 1,
		.pll_clk = HOPPING_MIPI_CLK,
		.data_rate = HOPPING_DATA_RATE,
		.hbp = HOPPING_HBP,
	},
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size = sizeof(top_rc_pattern),
	.corner_pattern_lt_addr = (void *)top_rc_pattern,
#endif
*/
	.oplus_display_global_dre = 1,
};

static struct mtk_panel_params ext_params_mode_1 = {//90hz
	//.vfp_low_power = 2480,
	//.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A,
		.count = 1,
		.para_list[0] = 0x9C,
	},
	.lcm_esd_check_table[1] = {
		.cmd = 0xAB,
		.count = 2,
		.para_list[0] = 0x00,
		.para_list[1] = 0x00,
	},
	.oplus_esd_sleep_status = true,
	.oplus_esd_sleep_ms = 5000,
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.dsc_params = {
		.bdg_dsc_enable = 1,
		.ver                   =  DSC_VER,
		.slice_mode            =  DSC_SLICE_MODE,
		.rgb_swap              =  DSC_RGB_SWAP,
		.dsc_cfg               =  DSC_DSC_CFG,
		.rct_on                =  DSC_RCT_ON,
		.bit_per_channel       =  DSC_BIT_PER_CHANNEL,
		.dsc_line_buf_depth    =  DSC_DSC_LINE_BUF_DEPTH,
		.bp_enable             =  DSC_BP_ENABLE,
		.bit_per_pixel         =  DSC_BIT_PER_PIXEL,
		.pic_height            =  FRAME_HEIGHT,
		.pic_width             =  FRAME_WIDTH,
		.slice_height          =  DSC_SLICE_HEIGHT,
		.slice_width           =  DSC_SLICE_WIDTH,
		.chunk_size            =  DSC_CHUNK_SIZE,
		.xmit_delay            =  DSC_XMIT_DELAY,
		.dec_delay             =  DSC_DEC_DELAY,
		.scale_value           =  DSC_SCALE_VALUE,
		.increment_interval    =  DSC_INCREMENT_INTERVAL,
		.decrement_interval    =  DSC_DECREMENT_INTERVAL,
		.line_bpg_offset       =  DSC_LINE_BPG_OFFSET,
		.nfl_bpg_offset        =  DSC_NFL_BPG_OFFSET,
		.slice_bpg_offset      =  DSC_SLICE_BPG_OFFSET,
		.initial_offset        =  DSC_INITIAL_OFFSET,
		.final_offset          =  DSC_FINAL_OFFSET,
		.flatness_minqp        =  DSC_FLATNESS_MINQP,
		.flatness_maxqp        =  DSC_FLATNESS_MAXQP,
		.rc_model_size         =  DSC_RC_MODEL_SIZE,
		.rc_edge_factor        =  DSC_RC_EDGE_FACTOR,
		.rc_quant_incr_limit0  =  DSC_RC_QUANT_INCR_LIMIT0,
		.rc_quant_incr_limit1  =  DSC_RC_QUANT_INCR_LIMIT1,
		.rc_tgt_offset_hi      =  DSC_RC_TGT_OFFSET_HI,
		.rc_tgt_offset_lo      =  DSC_RC_TGT_OFFSET_LO,
	},
	.data_rate = DATA_RATE,
	//.pll_clk = MIPI_CLK,
/*
        .phy_timcon = {
		.hs_trail = 9,
		.clk_trail = 10,
        },
*/
	.bdg_ssc_enable = 0,
	.ssc_enable = 0,
/*
	.dyn = {
		.switch_en = 1,
		.pll_clk = HOPPING_MIPI_CLK,
		.data_rate = HOPPING_DATA_RATE,
		.hbp = HOPPING_HBP,
	},
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size = sizeof(top_rc_pattern),
	.corner_pattern_lt_addr = (void *)top_rc_pattern,
#endif
*/
	.oplus_display_global_dre = 1,
};

static struct mtk_panel_params ext_params_mode_2 = {//45hz
	//.cust_esd_check = 1,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A,
		.count = 1,
		.para_list[0] = 0x9C,
	},
	.lcm_esd_check_table[1] = {
		.cmd = 0xAB,
		.count = 2,
		.para_list[0] = 0x00,
		.para_list[1] = 0x00,
	},
	.oplus_esd_sleep_status = true,
	.oplus_esd_sleep_ms = 5000,
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.dsc_params = {
		.bdg_dsc_enable = 1,
		.ver                   =  DSC_VER,
		.slice_mode            =  DSC_SLICE_MODE,
		.rgb_swap              =  DSC_RGB_SWAP,
		.dsc_cfg               =  DSC_DSC_CFG,
		.rct_on                =  DSC_RCT_ON,
		.bit_per_channel       =  DSC_BIT_PER_CHANNEL,
		.dsc_line_buf_depth    =  DSC_DSC_LINE_BUF_DEPTH,
		.bp_enable             =  DSC_BP_ENABLE,
		.bit_per_pixel         =  DSC_BIT_PER_PIXEL,
		.pic_height            =  FRAME_HEIGHT,
		.pic_width             =  FRAME_WIDTH,
		.slice_height          =  DSC_SLICE_HEIGHT,
		.slice_width           =  DSC_SLICE_WIDTH,
		.chunk_size            =  DSC_CHUNK_SIZE,
		.xmit_delay            =  DSC_XMIT_DELAY,
		.dec_delay             =  DSC_DEC_DELAY,
		.scale_value           =  DSC_SCALE_VALUE,
		.increment_interval    =  DSC_INCREMENT_INTERVAL,
		.decrement_interval    =  DSC_DECREMENT_INTERVAL,
		.line_bpg_offset       =  DSC_LINE_BPG_OFFSET,
		.nfl_bpg_offset        =  DSC_NFL_BPG_OFFSET,
		.slice_bpg_offset      =  DSC_SLICE_BPG_OFFSET,
		.initial_offset        =  DSC_INITIAL_OFFSET,
		.final_offset          =  DSC_FINAL_OFFSET,
		.flatness_minqp        =  DSC_FLATNESS_MINQP,
		.flatness_maxqp        =  DSC_FLATNESS_MAXQP,
		.rc_model_size         =  DSC_RC_MODEL_SIZE,
		.rc_edge_factor        =  DSC_RC_EDGE_FACTOR,
		.rc_quant_incr_limit0  =  DSC_RC_QUANT_INCR_LIMIT0,
		.rc_quant_incr_limit1  =  DSC_RC_QUANT_INCR_LIMIT1,
		.rc_tgt_offset_hi      =  DSC_RC_TGT_OFFSET_HI,
		.rc_tgt_offset_lo      =  DSC_RC_TGT_OFFSET_LO,
	},
	.data_rate = DATA_RATE,
	//.pll_clk = MIPI_CLK,
/*
        .phy_timcon = {
		.hs_trail = 9,
		.clk_trail = 10,
        },
*/
	.bdg_ssc_enable = 0,
	.ssc_enable = 0,
/*
	.dyn = {
		.switch_en = 1,
		.pll_clk = HOPPING_MIPI_CLK,
		.data_rate = HOPPING_DATA_RATE,
		.hbp = HOPPING_HBP,
	},
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size = sizeof(top_rc_pattern),
	.corner_pattern_lt_addr = (void *)top_rc_pattern,
#endif
*/
	.oplus_display_global_dre = 1,
};
/*
static struct mtk_panel_params ext_params_mode_3 = {//48hz
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A,
		.count = 1,
		.para_list[0] = 0x9C,
	},
	.lcm_esd_check_table[1] = {
		.cmd = 0xAB,
		.count = 2,
		.para_list[0] = 0x00,
		.para_list[1] = 0x00,
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.dsc_params = {
		.bdg_dsc_enable = 1,
		.ver                   =  DSC_VER,
		.slice_mode            =  DSC_SLICE_MODE,
		.rgb_swap              =  DSC_RGB_SWAP,
		.dsc_cfg               =  DSC_DSC_CFG,
		.rct_on                =  DSC_RCT_ON,
		.bit_per_channel       =  DSC_BIT_PER_CHANNEL,
		.dsc_line_buf_depth    =  DSC_DSC_LINE_BUF_DEPTH,
		.bp_enable             =  DSC_BP_ENABLE,
		.bit_per_pixel         =  DSC_BIT_PER_PIXEL,
		.pic_height            =  FRAME_HEIGHT,
		.pic_width             =  FRAME_WIDTH,
		.slice_height          =  DSC_SLICE_HEIGHT,
		.slice_width           =  DSC_SLICE_WIDTH,
		.chunk_size            =  DSC_CHUNK_SIZE,
		.xmit_delay            =  DSC_XMIT_DELAY,
		.dec_delay             =  DSC_DEC_DELAY,
		.scale_value           =  DSC_SCALE_VALUE,
		.increment_interval    =  DSC_INCREMENT_INTERVAL,
		.decrement_interval    =  DSC_DECREMENT_INTERVAL,
		.line_bpg_offset       =  DSC_LINE_BPG_OFFSET,
		.nfl_bpg_offset        =  DSC_NFL_BPG_OFFSET,
		.slice_bpg_offset      =  DSC_SLICE_BPG_OFFSET,
		.initial_offset        =  DSC_INITIAL_OFFSET,
		.final_offset          =  DSC_FINAL_OFFSET,
		.flatness_minqp        =  DSC_FLATNESS_MINQP,
		.flatness_maxqp        =  DSC_FLATNESS_MAXQP,
		.rc_model_size         =  DSC_RC_MODEL_SIZE,
		.rc_edge_factor        =  DSC_RC_EDGE_FACTOR,
		.rc_quant_incr_limit0  =  DSC_RC_QUANT_INCR_LIMIT0,
		.rc_quant_incr_limit1  =  DSC_RC_QUANT_INCR_LIMIT1,
		.rc_tgt_offset_hi      =  DSC_RC_TGT_OFFSET_HI,
		.rc_tgt_offset_lo      =  DSC_RC_TGT_OFFSET_LO,
	},
	.data_rate = DATA_RATE,
	.pll_clk = MIPI_CLK,
        .phy_timcon = {
		.hs_trail = 9,
		.clk_trail = 10,
        },
	.bdg_ssc_enable = 0,
	.ssc_enable = 0,
	.dyn = {
		.switch_en = 1,
		.pll_clk = HOPPING_MIPI_CLK,
		.data_rate = HOPPING_DATA_RATE,
		.hbp = HOPPING_HBP,
	},
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size = sizeof(top_rc_pattern),
	.corner_pattern_lt_addr = (void *)top_rc_pattern,
#endif
	.oplus_display_global_dre = 1,
};

static struct mtk_panel_params ext_params_mode_4 = {//30hz
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A,
		.count = 1,
		.para_list[0] = 0x9C,
	},
	.lcm_esd_check_table[1] = {
		.cmd = 0xAB,
		.count = 2,
		.para_list[0] = 0x00,
		.para_list[1] = 0x00,
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.dsc_params = {
		.bdg_dsc_enable = 1,
		.ver                   =  DSC_VER,
		.slice_mode            =  DSC_SLICE_MODE,
		.rgb_swap              =  DSC_RGB_SWAP,
		.dsc_cfg               =  DSC_DSC_CFG,
		.rct_on                =  DSC_RCT_ON,
		.bit_per_channel       =  DSC_BIT_PER_CHANNEL,
		.dsc_line_buf_depth    =  DSC_DSC_LINE_BUF_DEPTH,
		.bp_enable             =  DSC_BP_ENABLE,
		.bit_per_pixel         =  DSC_BIT_PER_PIXEL,
		.pic_height            =  FRAME_HEIGHT,
		.pic_width             =  FRAME_WIDTH,
		.slice_height          =  DSC_SLICE_HEIGHT,
		.slice_width           =  DSC_SLICE_WIDTH,
		.chunk_size            =  DSC_CHUNK_SIZE,
		.xmit_delay            =  DSC_XMIT_DELAY,
		.dec_delay             =  DSC_DEC_DELAY,
		.scale_value           =  DSC_SCALE_VALUE,
		.increment_interval    =  DSC_INCREMENT_INTERVAL,
		.decrement_interval    =  DSC_DECREMENT_INTERVAL,
		.line_bpg_offset       =  DSC_LINE_BPG_OFFSET,
		.nfl_bpg_offset        =  DSC_NFL_BPG_OFFSET,
		.slice_bpg_offset      =  DSC_SLICE_BPG_OFFSET,
		.initial_offset        =  DSC_INITIAL_OFFSET,
		.final_offset          =  DSC_FINAL_OFFSET,
		.flatness_minqp        =  DSC_FLATNESS_MINQP,
		.flatness_maxqp        =  DSC_FLATNESS_MAXQP,
		.rc_model_size         =  DSC_RC_MODEL_SIZE,
		.rc_edge_factor        =  DSC_RC_EDGE_FACTOR,
		.rc_quant_incr_limit0  =  DSC_RC_QUANT_INCR_LIMIT0,
		.rc_quant_incr_limit1  =  DSC_RC_QUANT_INCR_LIMIT1,
		.rc_tgt_offset_hi      =  DSC_RC_TGT_OFFSET_HI,
		.rc_tgt_offset_lo      =  DSC_RC_TGT_OFFSET_LO,
	},
	.data_rate = DATA_RATE,
	.pll_clk = MIPI_CLK,
        .phy_timcon = {
		.hs_trail = 9,
		.clk_trail = 10,
        },
	.bdg_ssc_enable = 0,
	.ssc_enable = 0,
	.dyn = {
		.switch_en = 1,
		.pll_clk = HOPPING_MIPI_CLK,
		.data_rate = HOPPING_DATA_RATE,
		.hbp = HOPPING_HBP,
	},
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size = sizeof(top_rc_pattern),
	.corner_pattern_lt_addr = (void *)top_rc_pattern,
#endif
	.oplus_display_global_dre = 1,
};
*/

/*
static void lcm_gamma_enter(void *dsi, dcs_write_gce cb, void *handle)
{
	char bl_tb1[] = {0xFF, 0x20};
	char bl_tb2[] = {0xFB, 0x01};
	char bl_tb3[] = {0x95, 0x09};
	char bl_tb4[] = {0x96, 0x09};
	char bl_tb5[] = {0xFF, 0x20};
	char bl_tb6[] = {0xFB, 0x01};
	char bl_tb7[] = {0xB0, 0x00, 0x00, 0x00, 0x04, 0x00, 0x0D, 0x00, 0x15, 0x00, 0x1C, 0x00, 0x24, 0x00, 0x2B, 0x00, 0x33};
	char bl_tb8[] = {0xB1, 0x00, 0x39, 0x00, 0x54, 0x00, 0x6B, 0x00, 0x98, 0x00, 0xBF, 0x01, 0x09, 0x01, 0x4E, 0x01, 0x51};
	char bl_tb9[] = {0xB2, 0x01, 0x9B, 0x01, 0xFA, 0x02, 0x36, 0x02, 0x84, 0x02, 0xB9, 0x02, 0xF8, 0x03, 0x0D, 0x03, 0x22};
	char bl_tb10[] = {0xB3, 0x03, 0x39, 0x03, 0x52, 0x03, 0x6F, 0x03, 0x92, 0x03, 0xB6, 0x03, 0xBC, 0x00, 0x00};
	char bl_tb11[] = {0xB4, 0x00, 0x00, 0x00, 0x04, 0x00, 0x0D, 0x00, 0x15, 0x00, 0x1C, 0x00, 0x24, 0x00, 0x2B, 0x00, 0x33};
	char bl_tb12[] = {0xB5, 0x00, 0x39, 0x00, 0x54, 0x00, 0x6B, 0x00, 0x98, 0x00, 0xBF, 0x01, 0x09, 0x01, 0x4E, 0x01, 0x51};
	char bl_tb13[] = {0xB6, 0x01, 0x9B, 0x01, 0xFA, 0x02, 0x36, 0x02, 0x84, 0x02, 0xB9, 0x02, 0xF8, 0x03, 0x0D, 0x03, 0x22};
	char bl_tb14[] = {0xB7, 0x03, 0x39, 0x03, 0x52, 0x03, 0x6F, 0x03, 0x92, 0x03, 0xB6, 0x03, 0xBC, 0x00, 0x00};
	char bl_tb15[] = {0xB8, 0x00, 0x00, 0x00, 0x04, 0x00, 0x0D, 0x00, 0x15, 0x00, 0x1C, 0x00, 0x24, 0x00, 0x2B, 0x00, 0x33};
	char bl_tb16[] = {0xB9, 0x00, 0x39, 0x00, 0x54, 0x00, 0x6B, 0x00, 0x98, 0x00, 0xBF, 0x01, 0x09, 0x01, 0x4E, 0x01, 0x51};
	char bl_tb17[] = {0xBA, 0x01, 0x9B, 0x01, 0xFA, 0x02, 0x36, 0x02, 0x84, 0x02, 0xB9, 0x02, 0xF8, 0x03, 0x0D, 0x03, 0x22};
	char bl_tb18[] = {0xBB, 0x03, 0x39, 0x03, 0x52, 0x03, 0x6F, 0x03, 0x92, 0x03, 0xB6, 0x03, 0xBC, 0x00, 0x00};
	char bl_tb19[] = {0xFF, 0x21};
	char bl_tb20[] = {0xFB, 0x01};
	char bl_tb21[] = {0xB0, 0x00, 0x00, 0x00, 0x04, 0x00, 0x0D, 0x00, 0x15, 0x00, 0x1C, 0x00, 0x24, 0x00, 0x2B, 0x00, 0x33};
	char bl_tb22[] = {0xB1, 0x00, 0x39, 0x00, 0x54, 0x00, 0x6B, 0x00, 0x98, 0x00, 0xBF, 0x01, 0x09, 0x01, 0x4E, 0x01, 0x51};
	char bl_tb23[] = {0xB2, 0x01, 0x9B, 0x01, 0xFA, 0x02, 0x36, 0x02, 0x84, 0x02, 0xB9, 0x02, 0xF8, 0x03, 0x0D, 0x03, 0x22};
	char bl_tb24[] = {0xB3, 0x03, 0x39, 0x03, 0x52, 0x03, 0x6F, 0x03, 0x92, 0x03, 0xB6, 0x03, 0xBC, 0x00, 0x00};
	char bl_tb25[] = {0xB4, 0x00, 0x00, 0x00, 0x04, 0x00, 0x0D, 0x00, 0x15, 0x00, 0x1C, 0x00, 0x24, 0x00, 0x2B, 0x00, 0x33};
	char bl_tb26[] = {0xB5, 0x00, 0x39, 0x00, 0x54, 0x00, 0x6B, 0x00, 0x98, 0x00, 0xBF, 0x01, 0x09, 0x01, 0x4E, 0x01, 0x51};
	char bl_tb27[] = {0xB6, 0x01, 0x9B, 0x01, 0xFA, 0x02, 0x36, 0x02, 0x84, 0x02, 0xB9, 0x02, 0xF8, 0x03, 0x0D, 0x03, 0x22};
	char bl_tb28[] = {0xB7, 0x03, 0x39, 0x03, 0x52, 0x03, 0x6F, 0x03, 0x92, 0x03, 0xB6, 0x03, 0xBC, 0x00, 0x00};
	char bl_tb29[] = {0xB8, 0x00, 0x00, 0x00, 0x04, 0x00, 0x0D, 0x00, 0x15, 0x00, 0x1C, 0x00, 0x24, 0x00, 0x2B, 0x00, 0x33};
	char bl_tb30[] = {0xB9, 0x00, 0x39, 0x00, 0x54, 0x00, 0x6B, 0x00, 0x98, 0x00, 0xBF, 0x01, 0x09, 0x01, 0x4E, 0x01, 0x51};
	char bl_tb31[] = {0xBA, 0x01, 0x9B, 0x01, 0xFA, 0x02, 0x36, 0x02, 0x84, 0x02, 0xB9, 0x02, 0xF8, 0x03, 0x0D, 0x03, 0x22};
	char bl_tb32[] = {0xBB, 0x03, 0x39, 0x03, 0x52, 0x03, 0x6F, 0x03, 0x92, 0x03, 0xB6, 0x03, 0xBC, 0x00, 0x00};
	char bl_tb33[] = {0xFF, 0x10};
	char bl_tb34[] = {0xFB, 0x01};

	cb(dsi, handle, bl_tb1, ARRAY_SIZE(bl_tb1));
	cb(dsi, handle, bl_tb2, ARRAY_SIZE(bl_tb2));
	cb(dsi, handle, bl_tb3, ARRAY_SIZE(bl_tb3));
	cb(dsi, handle, bl_tb4, ARRAY_SIZE(bl_tb4));
	cb(dsi, handle, bl_tb5, ARRAY_SIZE(bl_tb5));
	cb(dsi, handle, bl_tb6, ARRAY_SIZE(bl_tb6));
	cb(dsi, handle, bl_tb7, ARRAY_SIZE(bl_tb7));
	cb(dsi, handle, bl_tb8, ARRAY_SIZE(bl_tb8));
	cb(dsi, handle, bl_tb9, ARRAY_SIZE(bl_tb9));
	cb(dsi, handle, bl_tb10, ARRAY_SIZE(bl_tb10));
	cb(dsi, handle, bl_tb11, ARRAY_SIZE(bl_tb11));
	cb(dsi, handle, bl_tb12, ARRAY_SIZE(bl_tb12));
	cb(dsi, handle, bl_tb13, ARRAY_SIZE(bl_tb13));
	cb(dsi, handle, bl_tb14, ARRAY_SIZE(bl_tb14));
	cb(dsi, handle, bl_tb15, ARRAY_SIZE(bl_tb15));
	cb(dsi, handle, bl_tb16, ARRAY_SIZE(bl_tb16));
	cb(dsi, handle, bl_tb17, ARRAY_SIZE(bl_tb17));
	cb(dsi, handle, bl_tb18, ARRAY_SIZE(bl_tb18));
	cb(dsi, handle, bl_tb19, ARRAY_SIZE(bl_tb19));
	cb(dsi, handle, bl_tb20, ARRAY_SIZE(bl_tb20));
	cb(dsi, handle, bl_tb21, ARRAY_SIZE(bl_tb21));
	cb(dsi, handle, bl_tb22, ARRAY_SIZE(bl_tb22));
	cb(dsi, handle, bl_tb23, ARRAY_SIZE(bl_tb23));
	cb(dsi, handle, bl_tb24, ARRAY_SIZE(bl_tb24));
	cb(dsi, handle, bl_tb25, ARRAY_SIZE(bl_tb25));
	cb(dsi, handle, bl_tb26, ARRAY_SIZE(bl_tb26));
	cb(dsi, handle, bl_tb27, ARRAY_SIZE(bl_tb27));
	cb(dsi, handle, bl_tb28, ARRAY_SIZE(bl_tb28));
	cb(dsi, handle, bl_tb29, ARRAY_SIZE(bl_tb29));
	cb(dsi, handle, bl_tb30, ARRAY_SIZE(bl_tb30));
	cb(dsi, handle, bl_tb31, ARRAY_SIZE(bl_tb31));
	cb(dsi, handle, bl_tb32, ARRAY_SIZE(bl_tb32));
	cb(dsi, handle, bl_tb33, ARRAY_SIZE(bl_tb33));
	cb(dsi, handle, bl_tb34, ARRAY_SIZE(bl_tb34));
}
static void lcm_gamma_exit(void *dsi, dcs_write_gce cb, void *handle)
{
	char bl_tb1[] = {0xFF, 0x20};
	char bl_tb2[] = {0xFB, 0x01};
	char bl_tb3[] = {0x95, 0xD1};
	char bl_tb4[] = {0x96, 0xD1};
	char bl_tb5[] = {0xFF, 0x20};
	char bl_tb6[] = {0xFB, 0x01};
	char bl_tb7[] = {0xB0, 0x00, 0x00, 0x00, 0x1C, 0x00, 0x47, 0x00, 0x6A, 0x00, 0x86, 0x00, 0x9E, 0x00, 0xB4, 0x00, 0xC7};
	char bl_tb8[] = {0xB1, 0x00, 0xD8, 0x01, 0x12, 0x01, 0x3A, 0x01, 0x7E, 0x01, 0xAC, 0x01, 0xF8, 0x02, 0x30, 0x02, 0x32};
	char bl_tb9[] = {0xB2, 0x02, 0x68, 0x02, 0xA5, 0x02, 0xCD, 0x02, 0xFE, 0x03, 0x21, 0x03, 0x48, 0x03, 0x57, 0x03, 0x65};
	char bl_tb10[] = {0xB3, 0x03, 0x76, 0x03, 0x89, 0x03, 0x9E, 0x03, 0xAF, 0x03, 0xD1, 0x03, 0xD8, 0x00, 0x00};
	char bl_tb11[] = {0xB4, 0x00, 0x00, 0x00, 0x1C, 0x00, 0x47, 0x00, 0x6A, 0x00, 0x86, 0x00, 0x9E, 0x00, 0xB4, 0x00, 0xC7};
	char bl_tb12[] = {0xB5, 0x00, 0xD8, 0x01, 0x12, 0x01, 0x3A, 0x01, 0x7E, 0x01, 0xAC, 0x01, 0xF8, 0x02, 0x30, 0x02, 0x32};
	char bl_tb13[] = {0xB6, 0x02, 0x68, 0x02, 0xA5, 0x02, 0xCD, 0x02, 0xFE, 0x03, 0x21, 0x03, 0x48, 0x03, 0x57, 0x03, 0x65};
	char bl_tb14[] = {0xB7, 0x03, 0x76, 0x03, 0x89, 0x03, 0x9E, 0x03, 0xAF, 0x03, 0xD1, 0x03, 0xD8, 0x00, 0x00};
	char bl_tb15[] = {0xB8, 0x00, 0x00, 0x00, 0x1C, 0x00, 0x47, 0x00, 0x6A, 0x00, 0x86, 0x00, 0x9E, 0x00, 0xB4, 0x00, 0xC7};
	char bl_tb16[] = {0xB9, 0x00, 0xD8, 0x01, 0x12, 0x01, 0x3A, 0x01, 0x7E, 0x01, 0xAC, 0x01, 0xF8, 0x02, 0x30, 0x02, 0x32};
	char bl_tb17[] = {0xBA, 0x02, 0x68, 0x02, 0xA5, 0x02, 0xCD, 0x02, 0xFE, 0x03, 0x21, 0x03, 0x48, 0x03, 0x57, 0x03, 0x65};
	char bl_tb18[] = {0xBB, 0x03, 0x76, 0x03, 0x89, 0x03, 0x9E, 0x03, 0xAF, 0x03, 0xD1, 0x03, 0xD8, 0x00, 0x00};
	char bl_tb19[] = {0xFF, 0x21};
	char bl_tb20[] = {0xFB, 0x01};
	char bl_tb21[] = {0xB0, 0x00, 0x00, 0x00, 0x1C, 0x00, 0x47, 0x00, 0x6A, 0x00, 0x86, 0x00, 0x9E, 0x00, 0xB4, 0x00, 0xC7};
	char bl_tb22[] = {0xB1, 0x00, 0xD8, 0x01, 0x12, 0x01, 0x3A, 0x01, 0x7E, 0x01, 0xAC, 0x01, 0xF8, 0x02, 0x30, 0x02, 0x32};
	char bl_tb23[] = {0xB2, 0x02, 0x68, 0x02, 0xA5, 0x02, 0xCD, 0x02, 0xFE, 0x03, 0x21, 0x03, 0x48, 0x03, 0x57, 0x03, 0x65};
	char bl_tb24[] = {0xB3, 0x03, 0x76, 0x03, 0x89, 0x03, 0x9E, 0x03, 0xAF, 0x03, 0xD1, 0x03, 0xD8, 0x00, 0x00};
	char bl_tb25[] = {0xB4, 0x00, 0x00, 0x00, 0x1C, 0x00, 0x47, 0x00, 0x6A, 0x00, 0x86, 0x00, 0x9E, 0x00, 0xB4, 0x00, 0xC7};
	char bl_tb26[] = {0xB5, 0x00, 0xD8, 0x01, 0x12, 0x01, 0x3A, 0x01, 0x7E, 0x01, 0xAC, 0x01, 0xF8, 0x02, 0x30, 0x02, 0x32};
	char bl_tb27[] = {0xB6, 0x02, 0x68, 0x02, 0xA5, 0x02, 0xCD, 0x02, 0xFE, 0x03, 0x21, 0x03, 0x48, 0x03, 0x57, 0x03, 0x65};
	char bl_tb28[] = {0xB7, 0x03, 0x76, 0x03, 0x89, 0x03, 0x9E, 0x03, 0xAF, 0x03, 0xD1, 0x03, 0xD8, 0x00, 0x00};
	char bl_tb29[] = {0xB8, 0x00, 0x00, 0x00, 0x1C, 0x00, 0x47, 0x00, 0x6A, 0x00, 0x86, 0x00, 0x9E, 0x00, 0xB4, 0x00, 0xC7};
	char bl_tb30[] = {0xB9, 0x00, 0xD8, 0x01, 0x12, 0x01, 0x3A, 0x01, 0x7E, 0x01, 0xAC, 0x01, 0xF8, 0x02, 0x30, 0x02, 0x32};
	char bl_tb31[] = {0xBA, 0x02, 0x68, 0x02, 0xA5, 0x02, 0xCD, 0x02, 0xFE, 0x03, 0x21, 0x03, 0x48, 0x03, 0x57, 0x03, 0x65};
	char bl_tb32[] = {0xBB, 0x03, 0x76, 0x03, 0x89, 0x03, 0x9E, 0x03, 0xAF, 0x03, 0xD1, 0x03, 0xD8, 0x00, 0x00};
	char bl_tb33[] = {0xFF, 0x10};
	char bl_tb34[] = {0xFB, 0x01};

	cb(dsi, handle, bl_tb1, ARRAY_SIZE(bl_tb1));
	cb(dsi, handle, bl_tb2, ARRAY_SIZE(bl_tb2));
	cb(dsi, handle, bl_tb3, ARRAY_SIZE(bl_tb3));
	cb(dsi, handle, bl_tb4, ARRAY_SIZE(bl_tb4));
	cb(dsi, handle, bl_tb5, ARRAY_SIZE(bl_tb5));
	cb(dsi, handle, bl_tb6, ARRAY_SIZE(bl_tb6));
	cb(dsi, handle, bl_tb7, ARRAY_SIZE(bl_tb7));
	cb(dsi, handle, bl_tb8, ARRAY_SIZE(bl_tb8));
	cb(dsi, handle, bl_tb9, ARRAY_SIZE(bl_tb9));
	cb(dsi, handle, bl_tb10, ARRAY_SIZE(bl_tb10));
	cb(dsi, handle, bl_tb11, ARRAY_SIZE(bl_tb11));
	cb(dsi, handle, bl_tb12, ARRAY_SIZE(bl_tb12));
	cb(dsi, handle, bl_tb13, ARRAY_SIZE(bl_tb13));
	cb(dsi, handle, bl_tb14, ARRAY_SIZE(bl_tb14));
	cb(dsi, handle, bl_tb15, ARRAY_SIZE(bl_tb15));
	cb(dsi, handle, bl_tb16, ARRAY_SIZE(bl_tb16));
	cb(dsi, handle, bl_tb17, ARRAY_SIZE(bl_tb17));
	cb(dsi, handle, bl_tb18, ARRAY_SIZE(bl_tb18));
	cb(dsi, handle, bl_tb19, ARRAY_SIZE(bl_tb19));
	cb(dsi, handle, bl_tb20, ARRAY_SIZE(bl_tb20));
	cb(dsi, handle, bl_tb21, ARRAY_SIZE(bl_tb21));
	cb(dsi, handle, bl_tb22, ARRAY_SIZE(bl_tb22));
	cb(dsi, handle, bl_tb23, ARRAY_SIZE(bl_tb23));
	cb(dsi, handle, bl_tb24, ARRAY_SIZE(bl_tb24));
	cb(dsi, handle, bl_tb25, ARRAY_SIZE(bl_tb25));
	cb(dsi, handle, bl_tb26, ARRAY_SIZE(bl_tb26));
	cb(dsi, handle, bl_tb27, ARRAY_SIZE(bl_tb27));
	cb(dsi, handle, bl_tb28, ARRAY_SIZE(bl_tb28));
	cb(dsi, handle, bl_tb29, ARRAY_SIZE(bl_tb29));
	cb(dsi, handle, bl_tb30, ARRAY_SIZE(bl_tb30));
	cb(dsi, handle, bl_tb31, ARRAY_SIZE(bl_tb31));
	cb(dsi, handle, bl_tb32, ARRAY_SIZE(bl_tb32));
	cb(dsi, handle, bl_tb33, ARRAY_SIZE(bl_tb33));
	cb(dsi, handle, bl_tb34, ARRAY_SIZE(bl_tb34));
}
*/

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	tm_bl_level = level;
	/*bl_old_level = oplus_private_set_backlight(bl_old_level);*/
#if 0
//#ifdef CONFIG_MTK_MT6382_BDG
	if (tm_bl_level < 100 && tm_bl_level > 1) {
		dimming_level[0] = 0x05;
		dsi_set_cmdq_backlight(handle, 0x68,  2, dimming_level, 1);
	} else if (tm_bl_level < 200 && tm_bl_level > 99) {
		dimming_level[0] = 0x04;
		dsi_set_cmdq_backlight(handle, 0x68,  2, dimming_level, 1);
	} else if (tm_bl_level < 600 && tm_bl_level > 199) {
		dimming_level[0] = 0x03;
		dsi_set_cmdq_backlight(handle, 0x68,  2, dimming_level, 1);
	} else {
		dimming_level[0] = 0x02;
		dsi_set_cmdq_backlight(handle, 0x68,  2, dimming_level, 1);
	}
//#endif
#endif
	if (0 == tm_bl_level) {
		/* push_table(handle, lcm_dimming_off_setting, sizeof(lcm_dimming_off_setting) / sizeof(struct LCM_setting_table), 1); */
		bl_level_mt6382[1] = 0;
		bl_level_mt6382[2] = 0;
		pr_err("[ HWlevelmt6382_ backlight nt36672c]level = %d para_list[0] = %x, \
		para_list[1]=%x\n", tm_bl_level, bl_level_mt6382[1], bl_level_mt6382[2]);

		cb(dsi, handle, bl_level_mt6382, ARRAY_SIZE(bl_level_mt6382));
	} else {
		bl_level_mt6382[1] = 0x000F&(tm_bl_level >> 8);
		bl_level_mt6382[2] = 0x00FF&(tm_bl_level >> 0);
		pr_err("[ HW check mt6382_ backlight nt36672c]level = %d para_list[0] = %x, \
		para_list[1]=%x\n", tm_bl_level, bl_level_mt6382[1], bl_level_mt6382[2]);
		cb(dsi, handle, bl_level_mt6382, ARRAY_SIZE(bl_level_mt6382));
		/*push_table_cust(handle, bl_level, sizeof(bl_level) / sizeof(struct LCM_setting_table_V3), 0);*/
	}
	oplus_display_brightness = level;
	esd_brightness = level;

	return 0;
}

static int oplus_esd_backlight_recovery(void *dsi, dcs_write_gce cb,
		void *handle)
{
	char bl_tb0[] = {0x51, 0x03, 0xff};
	bl_tb0[1] = esd_brightness >> 8;
	bl_tb0[2] = esd_brightness & 0xFF;
	if (!cb)
		return -1;
	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));

	return 1;
}
/*
static int oplus_esd_recovery_pullup_gpio(struct drm_panel *panel, int status)
{
	struct lcm *ctx = panel_to_lcm(panel);
	if (status == 1) {
		ctx->bias_ldo = devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_LOW);
		if (IS_ERR(ctx->bias_ldo)) {
			pr_info("[error]%s: cannot get bias_ldo %ld\n",
				__func__, PTR_ERR(ctx->bias_ldo));
			return PTR_ERR(ctx->bias_ldo);
		}
		gpiod_set_value(ctx->bias_ldo, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_ldo);
		usleep_range(10 * 1000, 11 * 1000);
	} else if (status == 0) {
		ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->reset_gpio)) {
			pr_info("[error]%s: cannot get reset_gpio %ld\n",
				__func__, PTR_ERR(ctx->reset_gpio));
			return PTR_ERR(ctx->reset_gpio);
		}
		gpiod_set_value(ctx->reset_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->reset_gpio);

		tp_gpio_current_leakage_handler(false);

		usleep_range(1000, 1000);

		ctx->bias_ldo = devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_ldo)) {
			pr_info("[error]%s: cannot get bias_ldo %ld\n",
				__func__, PTR_ERR(ctx->bias_ldo));
			return PTR_ERR(ctx->bias_ldo);
		}
		gpiod_set_value(ctx->bias_ldo, 1);
		devm_gpiod_put(ctx->dev, ctx->bias_ldo);
		usleep_range(1000 * 1000, 1000 * 1000);
	}
	return 1;
}
*/
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
	if (drm_mode_vrefresh(m) == MODE_0_FPS) {
		ext->params = &ext_params;
	} else if (drm_mode_vrefresh(m) == MODE_1_FPS) {
		ext->params = &ext_params_mode_1;
	} else if (drm_mode_vrefresh(m) == MODE_2_FPS) {
		ext->params = &ext_params_mode_2;
	} /*else if (drm_mode_vrefresh(m) == MODE_3_FPS) {
		ext->params = &ext_params_mode_3;
	} else if (drm_mode_vrefresh(m) == MODE_4_FPS) {
		ext->params = &ext_params_mode_4;
	} else {
		ret = 1;
	}*/
	if (!ret) {
		current_fps = drm_mode_vrefresh(m);
	}

	return ret;
}

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		pr_info("[error]%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static int panel_ata_check(struct drm_panel *panel)
{
#ifdef BDG_PORTING_DBG
	struct lcm *ctx = panel_to_lcm(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	unsigned char data[3] = {0x00, 0x00, 0x00};
	unsigned char id[3] = {0x6e, 0x48, 0x00};
	ssize_t ret;
	ret = mipi_dsi_dcs_read(dsi, 0x4, data, 3);
	if (ret < 0) {
		pr_info("[error]%s error\n", __func__);
		return 0;
	}

	pr_info("ATA read data %x %x %x\n", data[0], data[1], data[2]);

	if (data[0] == id[0])
		return 1;

	pr_info("ATA expect read data is %x %x %x\n",
			id[0], id[1], id[2]);
#endif
	return 1;
}

static void lcm_cabc_mode_switch(void *dsi, dcs_write_gce cb,
		void *handle, unsigned int cabc_mode)
{
	 char bl_tb1[] = {0x55, 0x03}; /* no cabc ui pictures videoes*/

	pr_err("%s cabc = %d\n", __func__, cabc_mode);
	
	if (cabc_mode == 1) {
		bl_tb1[1] = 0x01;
		cb(dsi, handle, bl_tb1, ARRAY_SIZE(bl_tb1));
	} else if (cabc_mode == 2) {
		bl_tb1[1] = 0x02;
		cb(dsi, handle, bl_tb1, ARRAY_SIZE(bl_tb1));
	} else if (cabc_mode == 3) {
		bl_tb1[1] = 0x03;
		cb(dsi, handle, bl_tb1, ARRAY_SIZE(bl_tb1));
	} else if (cabc_mode == 0) {
		bl_tb1[1] = 0x00; /* cabc off */
		cb(dsi, handle, bl_tb1, ARRAY_SIZE(bl_tb1));
	}else {
		bl_tb1[1] = 0x03; /* default */
		cb(dsi, handle, bl_tb1, ARRAY_SIZE(bl_tb1));
	}
	cabc_status = cabc_mode;
	/* cabc_lastlevel = cabc_mode; */
}

static struct mtk_panel_funcs ext_funcs = {
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.reset = panel_ext_reset,
	.ext_param_set = mtk_panel_ext_param_set,
	.ata_check = panel_ata_check,
	.cabc_switch = lcm_cabc_mode_switch,
	.esd_backlight_recovery = oplus_esd_backlight_recovery,
	//.esd_recovery_pullup_gpio = oplus_esd_recovery_pullup_gpio,
};
#endif

struct panel_desc {
	const struct drm_display_mode *modes;
	unsigned int num_modes;

	unsigned int bpc;

	struct {
		unsigned int width;
		unsigned int height;
	} size;

	/**
	 * @prepare: the time (in milliseconds) that it takes for the panel to
	 *           become ready and start receiving video data
	 * @enable: the time (in milliseconds) that it takes for the panel to
	 *          display the first valid frame after starting to receive
	 *          video data
	 * @disable: the time (in milliseconds) that it takes for the panel to
	 *           turn the display off (no content is visible)
	 * @unprepare: the time (in milliseconds) that it takes for the panel
	 *             to power itself down completely
	 */
	struct {
		unsigned int prepare;
		unsigned int enable;
		unsigned int disable;
		unsigned int unprepare;
	} delay;
};

static int lcm_get_modes(struct drm_panel *panel,
	struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	struct drm_display_mode *mode_1;
	struct drm_display_mode *mode_2;
/*
	struct drm_display_mode *mode_3;
	struct drm_display_mode *mode_4;
*/
	pr_info("[%s]\n", __func__);
	mode = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode) {
		pr_info("[error]failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}
	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	mode_1 = drm_mode_duplicate(connector->dev, &performance_mode_1);
	if (!mode_1) {
		pr_info("[error]failed to add mode %ux%ux@%u\n",
			performance_mode_1.hdisplay,
			performance_mode_1.vdisplay,
			drm_mode_vrefresh(&performance_mode_1));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_1);
	mode_1->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_1);

	mode_2 = drm_mode_duplicate(connector->dev, &performance_mode_2);
	if (!mode_2) {
		pr_info("[error]failed to add mode %ux%ux@%u\n",
			performance_mode_2.hdisplay,
			performance_mode_2.vdisplay,
			drm_mode_vrefresh(&performance_mode_2));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_2);
	mode_2->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_2);
#if 0
	mode_3 = drm_mode_duplicate(connector->dev, &performance_mode_3);
	if (!mode_3) {
		pr_info("[error]failed to add mode %ux%ux@%u\n",
			performance_mode_3.hdisplay,
			performance_mode_3.vdisplay,
			drm_mode_vrefresh(&performance_mode_3));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_3);
	mode_3->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_3);

	mode_4 = drm_mode_duplicate(connector->dev, &performance_mode_4);
	if (!mode_4) {
		pr_info("[error]failed to add mode %ux%ux@%u\n",
			performance_mode_4.hdisplay,
			performance_mode_4.vdisplay,
			drm_mode_vrefresh(&performance_mode_4));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_4);
	mode_4->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_4);
#endif
	connector->display_info.width_mm = 70;
	connector->display_info.height_mm = 156;

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
	pr_info("%s+ lcm ,nt36672_tm\n", __func__);
	dsi_node = of_get_parent(dev->of_node);
	if (dsi_node) {
		endpoint = of_graph_get_next_endpoint(dsi_node, NULL);
		if (endpoint) {
			remote_node = of_graph_get_remote_port_parent(endpoint);
			if (!remote_node) {
				pr_info("No panel connected,skip probe lcm\n");
				return -ENODEV;
			}
			pr_info("device node name:%s\n", remote_node->name);
		}
	}
	if (remote_node != dev->of_node) {
		pr_info("%s+ skip probe due to not current lcm\n", __func__);
		return -ENODEV;
	}
	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	pr_info("%s start\n", __func__);
	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE;

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		pr_info("[error]%s: annot get reset-gpios %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);

#ifndef CONFIG_RT4831A_I2C
	ctx->bias_pos = devm_gpiod_get_index(dev, "bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		pr_info("[error]%s: cannot get bias-pos 0 %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	devm_gpiod_put(dev, ctx->bias_pos);

	ctx->bias_neg = devm_gpiod_get_index(dev, "bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		pr_info("[error]%s: cannot get bias-neg 1 %ld\n",
			__func__, PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	devm_gpiod_put(dev, ctx->bias_neg);
#endif

	ctx->prepared = true;
	ctx->enabled = true;

	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif
	register_device_proc("lcd", "nt36672c", "cost");
	oplus_max_normal_brightness = MAX_NORMAL_BRIGHTNESS;
	pr_info("%s+\n", __func__);

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
	{ .compatible = "nt36672c_tm_fhd_dsi_vdo_dphy", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "nt36672c_tm_fhd_dsi_vdo_dphy",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("Jingjing Liu <jingjing.liu@mediatek.com>");
MODULE_DESCRIPTION("lcm nt36672E vdo 120hz 6382 Panel Driver");
MODULE_LICENSE("GPL v2");

