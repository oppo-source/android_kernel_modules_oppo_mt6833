/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_dsi_panel_parser.h
** Description : oplus dsi panel parser header
** Version : 1.0
** Date : 2024/05/20
** Author : Display
******************************************************************/
#ifndef OPLUS_DSI_DISPLAY_PARSER_H
#define OPLUS_DSI_DISPLAY_PARSER_H

#include <linux/of.h>

#define OPLUS_BRIGHTNESS_MAX 4094
#define OPLUS_BRIGHTNESS_NORMAL_MAX 3515
#define OPLUS_BRIGHTNESS_MIN 1
#define OPLUS_BRIGHTNESS_DEFAULT_PERCENT 35

struct dsi_parser_utils {
	void *data;
	struct device_node *node;

	int (*get_int)(const struct device_node *np,
			const char *name, int *value);
	bool (*get_bool)(const struct device_node *np,
			const char *name, bool *value);
	int (*get_property)(const struct device_node *np,
			const char *name, char *value);
	int (*get_intarr)(const struct device_node *np,
			const char *name, int *value, int length);
	int (*get_string)(const struct device_node *np,
			const char *name, char **value);
	int (*get_string_array)(const struct device_node *np,
			const char *name, const char **value);
};

struct dsi_parser_utils *dsi_parser_get_parser_utils(void);

int oplus_dsi_panel_parser_base(struct device_node *node, void *ext_param_dev, void *ctx_dev);
int oplus_dsi_panel_parser_backlight(struct device_node *node, void *ctx_dev);
int oplus_dsi_panel_parser_effect(struct device_node *node, void *ext_param_dev, void *ctx_dev);
int oplus_dsi_panel_parser_esd(struct device_node *node, void *ext_param_dev, void *ctx_dev);
int oplus_dsi_panel_parser_round_corner(struct device_node *node, void *ext_param_dev, void *ctx_dev);
int oplus_dsi_panel_parser_dsc(struct device_node *node, void *ext_param_dev, void *ctx_dev);
int oplus_dsi_panel_parser_timing_base(struct device_node *node, void *ext_param_dev, void *ctx_dev);
int oplus_dsi_panel_parser_phy_timcon(struct device_node *node, void *ext_param_dev, void *ctx_dev);
int oplus_dsi_panel_parser_dyn_fps(struct device_node *node, void *ext_param_dev, void *ctx_dev);
int oplus_dsi_panel_parser_display_modes(int cell_index, int vrefresh, struct device_node *node, void *ext_param_dev, void *ctx_dev);
int oplus_dsi_panel_parser_spr(struct device_node *spr_node, void *ext_param_dev, void *ctx_dev);
int oplus_dsi_panel_parse_power_sequence_config(struct device_node *node, void *ctx_dev);
int oplus_dsi_panel_parse_reset_sequence_config(struct device_node *node, void *ctx_dev);
int oplus_dsi_panel_parse_mipi_err(struct device_node *node, void *ctx_dev);
int oplus_dsi_panel_parse_crc(struct device_node *node, void *ctx_dev);
#endif /* OPLUS_DSI_DISPLAY_PARSER_H */
