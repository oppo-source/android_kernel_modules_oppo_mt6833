/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_dsi_display_config.h
** Description : oplus dsi display config header
** Version : 1.0
** Date : 2024/05/20
** Author : Display
******************************************************************/
#ifndef _OPLUS_DSI_DISPLAY_CONFIG_H_
#define _OPLUS_DSI_DISPLAY_CONFIG_H_

/* ---------------------------- oplus define ---------------------------- */
#define OPLUS_KFREE(ptr) \
	do { \
		if (NULL != (ptr)) { \
			kfree((ptr)); \
			(ptr) = NULL; \
		} \
	} while (0)
#define PANEL_POWER_SUPPLY_MAX 10
#define PANEL_POWER_SUPPLY_MESSAGE_MAX 2
#define MAX_CMDLINE_PARAM_LEN 512
#define OPLUS_PANEL_NODE_PRE_WORD "oplus,"
#define PANEL_REGS_CHECK_NUM_MAX 32

#include "oplus_dsi_panel_cmd.h"
#include "oplus_display_power.h"

struct dsi_panel_info
{
	char panel_name[MAX_CMDLINE_PARAM_LEN];
	char manufacture[32];
	char vendor[32];
	u8 panel_flag;
	u8 panel_id1;
	u8 panel_id2;
	u8 panel_id3;
};

struct dsi_panel_backlight
{
	u32 oplus_brightness_default_percent;
	u32 oplus_brightness_default;
	u32 oplus_brightness_normal_max;
	u32 oplus_brightness_hw_max;
	u32 oplus_brightness_hw_min;
};

struct oplus_panel_regs_check_config {
	u32 config;
	u32 enter_cmd;
	u32 exit_cmd;
	u8 check_regs[PANEL_REGS_CHECK_NUM_MAX];
	u8 check_regs_rlen[PANEL_REGS_CHECK_NUM_MAX];
	u32 reg_count;
	u8 *check_value;
	u8 *return_buf;
	u8 *check_buf;
	u32 groups;
	u32 match_modes;
};

enum oplus_panel_regs_check_flag {
	OPLUS_REGS_CHECK_ENABLE = BIT(0),
	OPLUS_REGS_CHECK_PAGE_SWITCH = BIT(1),
};

struct dsi_panel_lcm {
	struct device *dev;
	struct drm_panel panel;
	struct dsi_panel_info panel_info;
	struct dsi_panel_backlight backlight_info;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_pos, *bias_neg;
	struct gpio_desc *bias_gpio;
	struct gpio_desc *vddr_aod_enable_gpio;
	struct drm_display_mode *m;
	struct drm_display_mode *display_modes;
	struct mtk_panel_params *ext_params_all;
	struct dsi_parser_utils *parser_utils;
	struct dsi_cmd_sets **dsi_cmd_timings_sets;
	struct gpio_desc *te_switch_gpio, *te_out_gpio;
	int mode_num;
	int res_switch;
	bool is_primary;
	bool prepared;
	bool enabled;
	int error;
	bool mode_switch_h2l_delay;
	bool backlight_check_disable;
	bool partial_update_enable;
	bool demura_dbv_flag;
	int demura_dbv_cfg[20];
	int emduty_dbv_cfg[20];
	int demura_dbv_length;
	int emduty_dbv_length;
	bool esd_is_triggered;
	const char *power_on_sequence[PANEL_POWER_SUPPLY_MAX * PANEL_POWER_SUPPLY_MESSAGE_MAX + 1];
	const char *power_off_sequence[PANEL_POWER_SUPPLY_MAX * PANEL_POWER_SUPPLY_MESSAGE_MAX + 1];
	struct dsi_regulator_info power_info;
	int panel_reset_sequence[4][2];
	/*add for mipi err check */
	struct oplus_panel_regs_check_config mipi_err_config;
	/*add for crc check */
	struct oplus_panel_regs_check_config crc_config;
};


/* ---------------------------- extern params ---------------------------- */
extern struct dsi_panel_lcm *oplus_display0_params;
extern struct dsi_panel_lcm *oplus_display1_params;
extern unsigned int m_flag;
extern unsigned int m_da;
extern unsigned int m_db;
extern unsigned int m_dc;

extern u8 oplus_dsi_panel_get_panel_flag(struct mtk_dsi *mtk_dsi);
extern u8 oplus_dsi_panel_get_panel_id1(struct mtk_dsi *mtk_dsi);
extern u8 oplus_dsi_panel_get_panel_id2(struct mtk_dsi *mtk_dsi);
extern u8 oplus_dsi_panel_get_panel_id3(struct mtk_dsi *mtk_dsi);
extern u32 oplus_dsi_panel_get_panel_info(struct mtk_dsi *mtk_dsi);
extern int oplus_dsi_display_init(void *dsi_device, void *ctx_dev);
extern int oplus_dsi_display_deinit(void *dsi_device, void *ctx_dev);

/* ---------------------------- function implementation ---------------------------- */
int oplus_dsi_panel_parse_cmd_params(void *node_dev, void *lcm_ctx);
int oplus_dsi_panel_parse_timing_param(void *timing_dev, void *ctx_dev, int type, int vrefresh);

#endif /* _OPLUS_DSI_DISPLAY_CONFIG_H_ */
