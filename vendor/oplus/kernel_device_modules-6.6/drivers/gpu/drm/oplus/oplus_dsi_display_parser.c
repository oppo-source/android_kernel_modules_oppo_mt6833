/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_dsi_display_parser.c
** Description : oplus dsi display config
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
#include <linux/of_graph.h>

#include <linux/of_gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <soc/oplus/device_info.h>
#include "mtk_dsi.h"
#include "mtk_panel_ext.h"
#include "oplus_display_debug.h"
#include "oplus_display_interface.h"
#include "oplus_dsi_panel_cmd.h"
#include "oplus_dsi_display_config.h"

#include "oplus_dsi_display_parser.h"

static int oplus_dsi_panel_parse_int(const struct device_node *node, const char *name, int *value)
{
	int rc = 0;

	rc = of_property_read_u32(node, name, value);
	if (rc == 0) {
		OPLUS_DSI_INFO("%s = %d\n", name, *value);
	} else {
		rc = -EINVAL;
		*value = 0;
		OPLUS_DSI_WARN("Parsing failed, default %s = %d\n", name, *value);
	}

	return rc;
}

static int oplus_dsi_panel_parse_intarr(const struct device_node *node, const char *name, int *value, int length)
{
	int rc = 0, i = 0;

	rc = of_property_count_elems_of_size(node, name, sizeof(int));
	if (rc < 0) {
		OPLUS_DSI_WARN("dts configuration length is zero\n");
		return rc;
	}
	if (rc < length) {
		OPLUS_DSI_WARN("The read length %d does not match the dts configuration length %d\n", length, rc);
		length = rc;
	}
	int property_data[length];
	rc = of_property_read_u32_array(node, name, &property_data[0], length);
	if (rc == 0) {
		OPLUS_DSI_INFO("%s parsing success\n", name);
		memcpy(&value[0], property_data, length * sizeof(int));
		rc = length;
	} else {
		rc = -EINVAL;
		OPLUS_DSI_WARN("Parsing failed, %s\n", name);
	}

	return rc;
}

static bool oplus_dsi_panel_parse_bool(const struct device_node *node, const char *name, bool *value)
{
	bool rc = false;

	rc = of_property_read_bool(node, name);
	OPLUS_DSI_INFO("%s = %d\n", name, rc);
	*value = rc;

	return rc;
}

static int oplus_dsi_panel_parse_property(const struct device_node *node, const char *name, char *value)
{
	int rc = -EINVAL;
	int property_len = 0;
	const void *property_addr = NULL;

	property_addr = of_get_property(node, name, &property_len);
	if (!property_addr || !property_len) {
		return rc;
	}
	memcpy(&value[0], property_addr, property_len);
	OPLUS_DSI_INFO("%s length = %d\n", name, property_len);

	return property_len;
}

static int oplus_dsi_panel_parse_string(const struct device_node *node, const char *name, char **value)
{
	int rc = -EINVAL;
	rc = of_property_read_string(node, name, (const char**)value);
	if (rc) {
		OPLUS_DSI_WARN("Parsing failed, %s\n", name);
		rc = -EINVAL;
		return rc;
	}
	OPLUS_DSI_INFO("%s = %s\n", name, *value);

	return rc;
}

static int oplus_dsi_panel_parse_string_array(const struct device_node *node, const char *name, const char **value)
{
	int rc = -EINVAL, str_count = 0;
	rc = of_property_count_strings(node, name);
	if (rc <= 0) {
		OPLUS_DSI_WARN("Invalid number of %s\n", name);
		return -EINVAL;
	}
	str_count = rc;
	OPLUS_DSI_INFO("%s = %d\n", name, str_count);
	rc = of_property_read_string_array(node, name, value, str_count);
	if (rc < 0) {
		OPLUS_DSI_WARN("Parsing failed, %s\n", name);
		return rc;
	}

	return str_count;
}

struct dsi_parser_utils *dsi_parser_get_parser_utils(void)
{
	static struct dsi_parser_utils parser_utils = {
		.get_int = oplus_dsi_panel_parse_int,
		.get_intarr = oplus_dsi_panel_parse_intarr,
		.get_bool = oplus_dsi_panel_parse_bool,
		.get_property = oplus_dsi_panel_parse_property,
		.get_string = oplus_dsi_panel_parse_string,
		.get_string_array = oplus_dsi_panel_parse_string_array,
	};

	return &parser_utils;
}

int oplus_dsi_panel_parser_base(struct device_node *node, void *ext_param_dev, void *ctx_dev)
{
	int rc = -EINVAL;
	struct dsi_panel_lcm *ctx = ctx_dev;
	struct dsi_parser_utils *utils = ctx->parser_utils;
	struct mtk_panel_params *ext_param = ext_param_dev;
	char *arr_ptr = NULL;

	arr_ptr = &ctx->panel_info.vendor[0];
	rc = utils->get_string(node, "oplus,dsi-vendor-name", &arr_ptr);
	strncpy(ctx->panel_info.vendor, arr_ptr, strlen(arr_ptr));
	arr_ptr = &ctx->panel_info.manufacture[0];
	rc = utils->get_string(node, "oplus,dsi-manufacture", &arr_ptr);
	strncpy(ctx->panel_info.manufacture, arr_ptr, strlen(arr_ptr));
	strncpy(ext_param->vendor, ctx->panel_info.vendor, strlen(ctx->panel_info.vendor));
	strncpy(ext_param->manufacture, ctx->panel_info.manufacture, strlen(ctx->panel_info.manufacture));
	rc = utils->get_bool(node, "oplus,mode-switch-h2l-delay", &ctx->mode_switch_h2l_delay);
	rc = utils->get_bool(node, "oplus,backlight-check-disable", &ctx->backlight_check_disable);
	rc = utils->get_bool(node, "oplus,partial-update-enable", &ctx->partial_update_enable);
	if (utils->get_bool(node, "oplus,demura-dbv-enable", &ctx->demura_dbv_flag)) {
		rc = utils->get_intarr(node, "oplus,demura-dbv-cfg", ctx->demura_dbv_cfg,
			sizeof(ctx->demura_dbv_cfg)/sizeof(ctx->demura_dbv_cfg[0]));
		if (rc < 0) {
			memset(ctx->demura_dbv_cfg, 0, sizeof(ctx->demura_dbv_cfg));
			ctx->demura_dbv_length = 0;
		} else {
			ctx->demura_dbv_length = rc;
		}
		rc = utils->get_intarr(node, "oplus,emduty-dbv-cfg", ctx->emduty_dbv_cfg,
			sizeof(ctx->emduty_dbv_cfg)/sizeof(ctx->emduty_dbv_cfg[0]));

		if (rc < 0) {
			memset(ctx->emduty_dbv_cfg, 0, sizeof(ctx->emduty_dbv_cfg));
			ctx->emduty_dbv_length = 0;
		} else {
			ctx->emduty_dbv_length = rc;
		}
	}
	rc = utils->get_int(node, "oplus,panel-type", &ext_param->panel_type);
	rc = utils->get_int(node, "oplus,lane-swap-en", &ext_param->lane_swap_en);
	rc = utils->get_int(node, "oplus,lcm-color-mode", &ext_param->lcm_color_mode);
	rc = utils->get_int(node, "oplus,output-mode", &ext_param->output_mode);
	rc = utils->get_int(node, "oplus,panel-bpp", &ext_param->panel_bpp);
	rc = utils->get_int(node, "oplus,physical-width-um", &ext_param->physical_width_um);
	rc = utils->get_int(node, "oplus,physical-height-um", &ext_param->physical_height_um);
	rc = utils->get_int(node, "oplus,keep_ulps", &ext_param->keep_ulps);
	rc = utils->get_intarr(node, "oplus,lane-swap", (int *)(&ext_param->lane_swap[0][0]), sizeof(ext_param->lane_swap)/sizeof(ext_param->lane_swap[0][0]));

	return rc;
}

int oplus_dsi_panel_parser_backlight(struct device_node *node, void *ctx_dev)
{
	int rc = -EINVAL;
	struct dsi_panel_lcm *ctx = ctx_dev;
	struct dsi_parser_utils *utils = ctx->parser_utils;
	struct device_node *backlight_node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,disp-leds");
	if (node) {
		backlight_node = of_get_child_by_name(node, "backlight");
		if (backlight_node) {
			rc = utils->get_int(backlight_node, "max-hw-brightness", &ctx->backlight_info.oplus_brightness_hw_max);
			if (rc < 0) {
				ctx->backlight_info.oplus_brightness_hw_max = OPLUS_BRIGHTNESS_MAX;
				OPLUS_DSI_ERR("not found backlight oplus_brightness_hw_max set default %d\n",
					ctx->backlight_info.oplus_brightness_hw_max);
			}
			rc = utils->get_int(backlight_node, "min-hw-brightness", &ctx->backlight_info.oplus_brightness_hw_min);
			if (rc < 0) {
				ctx->backlight_info.oplus_brightness_hw_min = OPLUS_BRIGHTNESS_MIN;
				OPLUS_DSI_ERR("not found backlight oplus_brightness_hw_min set default %d\n",
					ctx->backlight_info.oplus_brightness_hw_min);
			}
			rc = utils->get_int(backlight_node, "oplus-brightness-normal-max", &ctx->backlight_info.oplus_brightness_normal_max);
			if (rc < 0) {
				ctx->backlight_info.oplus_brightness_normal_max = OPLUS_BRIGHTNESS_NORMAL_MAX;
				OPLUS_DSI_ERR("not found backlight oplus_brightness_normal_max set default %d\n",
					ctx->backlight_info.oplus_brightness_normal_max);
			}
			rc = utils->get_int(backlight_node, "oplus-brightness-default-percent", &ctx->backlight_info.oplus_brightness_default_percent);
			if (rc < 0) {
				ctx->backlight_info.oplus_brightness_default_percent = OPLUS_BRIGHTNESS_DEFAULT_PERCENT;
				OPLUS_DSI_ERR("not found backlight oplus_brightness_default_percent set default %d\n",
					ctx->backlight_info.oplus_brightness_default_percent);
			}
			ctx->backlight_info.oplus_brightness_default = ctx->backlight_info.oplus_brightness_hw_max
				* ctx->backlight_info.oplus_brightness_default_percent / 100U;
			of_node_put(backlight_node);
		} else {
			OPLUS_DSI_ERR("not found backlight node in: \"mediatek,disp-leds\"\n");
		}
		of_node_put(node);
	} else {
		OPLUS_DSI_ERR("not found backlight node compatible: \"mediatek,disp-leds\"\n");
	}

	return rc;
}

int oplus_dsi_panel_parser_effect(struct device_node *node, void *ext_param_dev, void *ctx_dev)
{
	int rc = -EINVAL;
	struct dsi_panel_lcm *ctx = ctx_dev;
	struct dsi_parser_utils *utils = ctx->parser_utils;
	struct mtk_panel_params *ext_param = ext_param_dev;

	rc = utils->get_bool(node, "oplus,color-vivid-status", &ext_param->color_vivid_status);
	rc = utils->get_bool(node, "oplus,color-srgb-status", &ext_param->color_srgb_status);
	rc = utils->get_bool(node, "oplus,color-softiris-status", &ext_param->color_softiris_status);
	rc = utils->get_bool(node, "oplus,color-dual-panel-status", &ext_param->color_dual_panel_status);
	rc = utils->get_bool(node, "oplus,color-dual-brightness-status", &ext_param->color_dual_brightness_status);
	rc = utils->get_bool(node, "oplus,color-oplus-calibrate-status", &ext_param->color_oplus_calibrate_status);
	rc = utils->get_bool(node, "oplus,color-samsung-status", &ext_param->color_samsung_status);
	rc = utils->get_bool(node, "oplus,color-loading-status", &ext_param->color_loading_status);
	rc = utils->get_bool(node, "oplus,color-2nit-status", &ext_param->color_2nit_status);
	rc = utils->get_bool(node, "oplus,color-nature-profession-status", &ext_param->color_nature_profession_status);

	return rc;
}

int oplus_dsi_panel_parser_esd(struct device_node *node, void *ext_param_dev, void *ctx_dev)
{
	int rc = -EINVAL, i = 0, j = 0;
	struct dsi_panel_lcm *ctx = ctx_dev;
	struct dsi_parser_utils *utils = ctx->parser_utils;
	struct mtk_panel_params *ext_param = ext_param_dev;
	char esd_check_cmd[2][ESD_CHECK_NUM] = {0};
	char esd_check_value[ESD_CHECK_NUM * RT_MAX_NUM] = {0};
	char esd_check_mask[ESD_CHECK_NUM * RT_MAX_NUM] = {0};

	utils->get_int(node, "oplus,cust-esd-check", &ext_param->cust_esd_check);
	rc = utils->get_int(node, "oplus,esd-check-enable", &ext_param->esd_check_enable);
	if ((rc < 0) || (!ext_param->esd_check_enable)) {
		rc = -EINVAL;
		return rc;
	}
	rc = utils->get_property(node, "oplus,esd_check_cmd", esd_check_cmd[0]);
	if (rc < 0) {
		return rc;
	}
	rc = utils->get_property(node, "oplus,esd_check_count", esd_check_cmd[1]);
	if (rc < 0) {
		return rc;
	}
	rc = utils->get_property(node, "oplus,esd_check_value", esd_check_value);
	if (rc < 0) {
		return rc;
	}
	rc = utils->get_property(node, "oplus,esd_check_mask", esd_check_mask);
	if (rc < 0) {
		return rc;
	}
	for(i = 0, j = 0; i < ESD_CHECK_NUM; i++) {
		if (esd_check_cmd[0][i] != 0) {
			ext_param->lcm_esd_check_table[i].cmd = esd_check_cmd[0][i];
			ext_param->lcm_esd_check_table[i].count = esd_check_cmd[1][i];
			memcpy(ext_param->lcm_esd_check_table[i].para_list, esd_check_value + j, ext_param->lcm_esd_check_table[i].count);
			memcpy(ext_param->lcm_esd_check_table[i].mask_list, esd_check_mask + j, ext_param->lcm_esd_check_table[i].count);
			j += ext_param->lcm_esd_check_table[i].count;
		}
	}
	return rc;
}

int oplus_dsi_panel_parser_round_corner(struct device_node *node, void *ext_param_dev, void *ctx_dev)
{
	struct dsi_panel_lcm *ctx = ctx_dev;
	struct dsi_parser_utils *utils = ctx->parser_utils;
	struct mtk_panel_params *ext_param = ext_param_dev;
	int rc = -EINVAL, cornel_count = 0;
	unsigned char *cornel_data = NULL;

	rc = utils->get_int(node, "oplus,round-corner-en", &ext_param->round_corner_en);
	if ((rc < 0) || (!ext_param->round_corner_en)) {
		rc = -EINVAL;
		return rc;
	}
	rc = utils->get_int(node, "oplus,corner-pattern-height", &ext_param->corner_pattern_height);
	if (rc < 0) {
		return rc;
	}
	rc = utils->get_int(node, "oplus,corner-pattern-height-bot", &ext_param->corner_pattern_height_bot);
	if (rc < 0) {
		return rc;
	}
	rc = -EINVAL;
	cornel_count = of_property_count_u8_elems(node, "oplus,corner-pattern-lt");
	if (cornel_count > 0) {
		ext_param->corner_pattern_tp_size = cornel_count;
		cornel_data = kzalloc(cornel_count, GFP_KERNEL);
		if (cornel_data) {
			ext_param->corner_pattern_lt_addr = cornel_data;
			rc = utils->get_property(node, "oplus,corner-pattern-lt", cornel_data);
			if (rc < 0) {
				kfree(cornel_data);
				cornel_data = NULL;
				return rc;
			}
		}
	}
	cornel_count = of_property_count_elems_of_size(node, "oplus,corner-pattern-size-per-line", sizeof(int));
	if (cornel_count > 0) {
		cornel_data = kzalloc(cornel_count * sizeof(int), GFP_KERNEL);
		if (cornel_data) {
			ext_param->corner_pattern_size_per_line = (int *)cornel_data;
			rc = utils->get_intarr(node, "oplus,corner-pattern-size-per-line", (int *)cornel_data, cornel_count);
			if (rc < 0) {
				kfree(cornel_data);
				cornel_data = NULL;
			}
		}
	}
	return rc;
}

int oplus_dsi_panel_parser_dsc(struct device_node *node, void *ext_param_dev, void *ctx_dev)
{
	int rc = -EINVAL;
	struct dsi_panel_lcm *ctx = ctx_dev;
	struct dsi_parser_utils *utils = ctx->parser_utils;
	struct mtk_panel_params *ext_param = ext_param_dev;

	rc = utils->get_int(node, "enable", &ext_param->dsc_params.enable);
	rc = utils->get_int(node, "ver", &ext_param->dsc_params.ver);
	rc = utils->get_int(node, "slice_mode", &ext_param->dsc_params.slice_mode);
	rc = utils->get_int(node, "rgb_swap", &ext_param->dsc_params.rgb_swap);
	rc = utils->get_int(node, "dsc_cfg", &ext_param->dsc_params.dsc_cfg);
	rc = utils->get_int(node, "rct_on", &ext_param->dsc_params.rct_on);
	rc = utils->get_int(node, "bit_per_channel", &ext_param->dsc_params.bit_per_channel);
	rc = utils->get_int(node, "dsc_line_buf_depth", &ext_param->dsc_params.dsc_line_buf_depth);
	rc = utils->get_int(node, "bp_enable", &ext_param->dsc_params.bp_enable);
	rc = utils->get_int(node, "bit_per_pixel", &ext_param->dsc_params.bit_per_pixel);
	rc = utils->get_int(node, "pic_height", &ext_param->dsc_params.pic_height);
	rc = utils->get_int(node, "pic_width", &ext_param->dsc_params.pic_width);
	rc = utils->get_int(node, "slice_height", &ext_param->dsc_params.slice_height);
	rc = utils->get_int(node, "slice_width", &ext_param->dsc_params.slice_width);
	rc = utils->get_int(node, "chunk_size", &ext_param->dsc_params.chunk_size);
	rc = utils->get_int(node, "xmit_delay", &ext_param->dsc_params.xmit_delay);
	rc = utils->get_int(node, "dec_delay", &ext_param->dsc_params.dec_delay);
	rc = utils->get_int(node, "scale_value", &ext_param->dsc_params.scale_value);
	rc = utils->get_int(node, "increment_interval", &ext_param->dsc_params.increment_interval);
	rc = utils->get_int(node, "decrement_interval", &ext_param->dsc_params.decrement_interval);
	rc = utils->get_int(node, "line_bpg_offset", &ext_param->dsc_params.line_bpg_offset);
	rc = utils->get_int(node, "nfl_bpg_offset", &ext_param->dsc_params.nfl_bpg_offset);
	rc = utils->get_int(node, "slice_bpg_offset", &ext_param->dsc_params.slice_bpg_offset);
	rc = utils->get_int(node, "initial_offset", &ext_param->dsc_params.initial_offset);
	rc = utils->get_int(node, "final_offset", &ext_param->dsc_params.final_offset);
	rc = utils->get_int(node, "flatness_minqp", &ext_param->dsc_params.flatness_minqp);
	rc = utils->get_int(node, "flatness_maxqp", &ext_param->dsc_params.flatness_maxqp);
	rc = utils->get_int(node, "rc_model_size", &ext_param->dsc_params.rc_model_size);
	rc = utils->get_int(node, "rc_edge_factor", &ext_param->dsc_params.rc_edge_factor);
	rc = utils->get_int(node, "rc_quant_incr_limit0", &ext_param->dsc_params.rc_quant_incr_limit0);
	rc = utils->get_int(node, "rc_quant_incr_limit1", &ext_param->dsc_params.rc_quant_incr_limit1);
	rc = utils->get_int(node, "rc_tgt_offset_hi", &ext_param->dsc_params.rc_tgt_offset_hi);
	rc = utils->get_int(node, "rc_tgt_offset_lo", &ext_param->dsc_params.rc_tgt_offset_lo);

	return rc;
}

int oplus_dsi_panel_parser_timing_base(struct device_node *node, void *ext_param_dev, void *ctx_dev)
{
	int rc = -EINVAL;
	struct dsi_panel_lcm *ctx = ctx_dev;
	struct dsi_parser_utils *utils = ctx->parser_utils;
	struct mtk_panel_params *ext_param = ext_param_dev;

	rc = utils->get_bool(node, "oplus,is-support-dmr", &ext_param->is_support_dmr);
	rc = utils->get_bool(node, "oplus,ofp_need_keep_apart_backlight", &ext_param->oplus_ofp_need_keep_apart_backlight);
	rc = utils->get_bool(node, "oplus,ofp-need-to-sync-data-in-aod-unlocking", &ext_param->oplus_ofp_need_to_sync_data_in_aod_unlocking);

	rc = utils->get_int(node, "oplus,dsi-panel-pllclk", &ext_param->pll_clk);
	ext_param->data_rate = ext_param->pll_clk * 2;
	rc = utils->get_int(node, "oplus,cmd-null-pkt-en", &ext_param->cmd_null_pkt_en);
	rc = utils->get_int(node, "oplus,cmd-null-pkt-len", &ext_param->cmd_null_pkt_len);
	rc = utils->get_int(node, "oplus,merge-trig-offset", &ext_param->merge_trig_offset);
	rc = utils->get_int(node, "oplus,vidle-te-duration", &ext_param->oplus_vidle_te_duration);
	rc = utils->get_int(node, "oplus,ofp-hbm-on-delay", &ext_param->oplus_ofp_hbm_on_delay);
	rc = utils->get_int(node, "oplus,ofp-pre-hbm-off-delay", &ext_param->oplus_ofp_pre_hbm_off_delay);
	rc = utils->get_int(node, "oplus,ofp-hbm-off-delay", &ext_param->oplus_ofp_hbm_off_delay);
	rc = utils->get_int(node, "oplus,ofp-aod-off-insert-black", &ext_param->oplus_ofp_aod_off_insert_black);
	rc = utils->get_int(node, "oplus,ofp-aod-off-black-frame-total-time", &ext_param->oplus_ofp_aod_off_black_frame_total_time);

	return rc;
}

int oplus_dsi_panel_parser_phy_timcon(struct device_node *node, void *ext_param_dev, void *ctx_dev)
{
	int rc = -EINVAL;
	struct dsi_panel_lcm *ctx = ctx_dev;
	struct dsi_parser_utils *utils = ctx->parser_utils;
	struct mtk_panel_params *ext_param = ext_param_dev;

	rc = utils->get_int(node, "hs-trail", &ext_param->phy_timcon.hs_trail);
	rc = utils->get_int(node, "clk-trail", &ext_param->phy_timcon.clk_trail);

	return rc;
}

int oplus_dsi_panel_parser_dyn_fps(struct device_node *node, void *ext_param_dev, void *ctx_dev)
{
	int rc = -EINVAL;
	struct dsi_panel_lcm *ctx = ctx_dev;
	struct dsi_parser_utils *utils = ctx->parser_utils;
	struct mtk_panel_params *ext_param = ext_param_dev;

	rc = utils->get_int(node, "switch_en", &ext_param->dyn_fps.switch_en);
	rc = utils->get_int(node, "vact_timing_fps", &ext_param->dyn_fps.vact_timing_fps);
	rc = utils->get_int(node, "apollo_limit_superior_us", &ext_param->dyn_fps.apollo_limit_superior_us);
	rc = utils->get_int(node, "apollo_limit_inferior_us", &ext_param->dyn_fps.apollo_limit_inferior_us);
	rc = utils->get_int(node, "apollo_transfer_time_us", &ext_param->dyn_fps.apollo_transfer_time_us);

	return rc;
}

int oplus_dsi_panel_parser_display_modes(int cell_index, int vrefresh, struct device_node *node, void *ext_param_dev, void *ctx_dev)
{
	int rc = -EINVAL;
	struct dsi_panel_lcm *ctx = ctx_dev;
	struct dsi_parser_utils *utils = ctx->parser_utils;
	struct mtk_panel_params *ext_param = ext_param_dev;
	struct device_node *child;
	int mode_hdisplay = 0, mode_vdisplay = 0,
		mode_fhd_hfp = 0, mode_fhd_hsa = 0, mode_fhd_hbp = 0,
		mode_fhd_vfp = 0, mode_fhd_vsa = 0, mode_fhd_vbp = 0, mode_hskew = 0;

	for_each_child_of_node(node, child) {
		if ((cell_index >= ctx->mode_num) && (ctx->res_switch < RES_SWITCH_ON_AP)) {
			continue;
		}
		rc = utils->get_int(child, "hdisplay", &mode_hdisplay);
		rc = utils->get_int(child, "vdisplay", &mode_vdisplay);
		rc = utils->get_int(child, "fhd-hfp", &mode_fhd_hfp);
		rc = utils->get_int(child, "fhd-hsa", &mode_fhd_hsa);
		rc = utils->get_int(child, "fhd-hbp", &mode_fhd_hbp);
		rc = utils->get_int(child, "fhd-vfp", &mode_fhd_vfp);
		rc = utils->get_int(child, "fhd-vsa", &mode_fhd_vsa);
		rc = utils->get_int(child, "fhd-vbp", &mode_fhd_vbp);
		rc = utils->get_int(child, "hskew", &mode_hskew);
		ctx->display_modes[cell_index].hdisplay = mode_hdisplay;
		ctx->display_modes[cell_index].vdisplay = mode_vdisplay;
		ctx->display_modes[cell_index].hsync_start = mode_hdisplay + mode_fhd_hfp;
		ctx->display_modes[cell_index].hsync_end = mode_hdisplay + mode_fhd_hfp + mode_fhd_hsa;
		ctx->display_modes[cell_index].htotal = mode_hdisplay + mode_fhd_hfp + mode_fhd_hsa + mode_fhd_hbp;
		ctx->display_modes[cell_index].vsync_start = mode_vdisplay + mode_fhd_vfp;
		ctx->display_modes[cell_index].vsync_end = mode_vdisplay + mode_fhd_vfp + mode_fhd_vsa;
		ctx->display_modes[cell_index].vtotal = mode_vdisplay + mode_fhd_vfp + mode_fhd_vsa + mode_fhd_vbp;
		ctx->display_modes[cell_index].clock = (ctx->display_modes[cell_index].htotal
												* ctx->display_modes[cell_index].vtotal * vrefresh) / 100;
		ctx->display_modes[cell_index].clock = ((ctx->display_modes[cell_index].clock % 10) != 0)
												? (ctx->display_modes[cell_index].clock / 10 + 1)
												: (ctx->display_modes[cell_index].clock / 10);
		ctx->display_modes[cell_index].hskew = mode_hskew;
		cell_index += ctx->mode_num;
	}

	return rc;
}

int oplus_dsi_panel_parser_spr(struct device_node *spr_node, void *ext_param_dev, void *ctx_dev)
{
	int rc = -EINVAL;
	struct dsi_panel_lcm *ctx = ctx_dev;
	struct dsi_parser_utils *utils = ctx->parser_utils;
	struct mtk_panel_params *ext_param = ext_param_dev;
	struct device_node *node, *child;
	int spr_count = 0, i = 0;
	int *spr_data = NULL;

	rc = utils->get_int(spr_node, "oplus,spr-output-mode", &ext_param->spr_output_mode);
	rc = utils->get_int(spr_node, "enable", &ext_param->spr_params.enable);
	rc = utils->get_int(spr_node, "relay", &ext_param->spr_params.relay);
	rc = utils->get_int(spr_node, "postalign_en", &ext_param->spr_params.postalign_en);
	rc = utils->get_int(spr_node, "bypass_dither", &ext_param->spr_params.bypass_dither);
	rc = utils->get_int(spr_node, "custom_header", &ext_param->spr_params.custom_header);
	rc = utils->get_int(spr_node, "spr_format_type", &ext_param->spr_params.spr_format_type);
	rc = utils->get_int(spr_node, "spr_switch_type", &ext_param->spr_params.spr_switch_type);
	spr_count = of_property_count_elems_of_size(spr_node, "oplus,spr-ip-params", sizeof(int));
	if (spr_count > 0) {
		spr_data = kzalloc(spr_count * sizeof(int), GFP_KERNEL);
		if (spr_data) {
			ext_param->spr_params.spr_ip_params = spr_data;
			rc = utils->get_intarr(spr_node, "oplus,spr-ip-params", spr_data, spr_count);
			if (rc < 0) {
				kfree(spr_data);
				spr_data = NULL;
			}
		}
	}
	spr_count = of_property_count_elems_of_size(spr_node, "oplus,spr-ip-shrink-params", sizeof(int));
	if (spr_count > 0) {
		spr_data = kzalloc(spr_count * sizeof(int), GFP_KERNEL);
		if (spr_data) {
			ext_param->spr_params.spr_ip_shrink_params = spr_data;
			rc = utils->get_intarr(spr_node, "oplus,spr-ip-shrink-params", spr_data, spr_count);
			if (rc < 0) {
				kfree(spr_data);
				spr_data = NULL;
			}
		}
	}
	spr_count = of_property_count_elems_of_size(spr_node, "oplus,mtk-spr-ip-params", sizeof(int));
	if (spr_count > 0) {
		spr_data = kzalloc(spr_count * sizeof(int), GFP_KERNEL);
		if (spr_data) {
			ext_param->spr_params.mtk_spr_ip_params = spr_data;
			rc = utils->get_intarr(spr_node, "oplus,mtk-spr-ip-params", spr_data, spr_count);
			if (rc < 0) {
				kfree(spr_data);
				spr_data = NULL;
			}
		}
	}
	node = of_get_child_by_name(spr_node, "oplus,spr-color-params");
	if (node) {
		i = 0;
		for_each_child_of_node(node, child) {
			if (strcmp("spr-color", child->name) != 0) {
				continue;
			}
			rc = utils->get_int(child, "spr_color_params_type", (int*)(&ext_param->spr_params.spr_color_params[i].spr_color_params_type));
			rc = utils->get_int(child, "count", &ext_param->spr_params.spr_color_params[i].count);
			rc = utils->get_property(child, "para_list", ext_param->spr_params.spr_color_params[i].para_list);
			rc = utils->get_property(child, "tune_list", ext_param->spr_params.spr_color_params[i].tune_list);
			i = (i + 1) % SPR_COLOR_PARAMS_TYPE_NUM;
		}
		of_node_put(node);
	}
	node = of_get_child_by_name(spr_node, "oplus,dsc_params_spr_in");
	if (node) {
		rc = utils->get_int(node, "enable", &ext_param->dsc_params_spr_in.enable);
		rc = utils->get_int(node, "ver", &ext_param->dsc_params_spr_in.ver);
		rc = utils->get_int(node, "slice_mode", &ext_param->dsc_params_spr_in.slice_mode);
		rc = utils->get_int(node, "rgb_swap", &ext_param->dsc_params_spr_in.rgb_swap);
		rc = utils->get_int(node, "dsc_cfg", &ext_param->dsc_params_spr_in.dsc_cfg);
		rc = utils->get_int(node, "rct_on", &ext_param->dsc_params_spr_in.rct_on);
		rc = utils->get_int(node, "bit_per_channel", &ext_param->dsc_params_spr_in.bit_per_channel);
		rc = utils->get_int(node, "dsc_line_buf_depth", &ext_param->dsc_params_spr_in.dsc_line_buf_depth);
		rc = utils->get_int(node, "bp_enable", &ext_param->dsc_params_spr_in.bp_enable);
		rc = utils->get_int(node, "bit_per_pixel", &ext_param->dsc_params_spr_in.bit_per_pixel);
		rc = utils->get_int(node, "pic_height", &ext_param->dsc_params_spr_in.pic_height);
		rc = utils->get_int(node, "pic_width", &ext_param->dsc_params_spr_in.pic_width);
		rc = utils->get_int(node, "slice_height", &ext_param->dsc_params_spr_in.slice_height);
		rc = utils->get_int(node, "slice_width", &ext_param->dsc_params_spr_in.slice_width);
		rc = utils->get_int(node, "chunk_size", &ext_param->dsc_params_spr_in.chunk_size);
		rc = utils->get_int(node, "xmit_delay", &ext_param->dsc_params_spr_in.xmit_delay);
		rc = utils->get_int(node, "dec_delay", &ext_param->dsc_params_spr_in.dec_delay);
		rc = utils->get_int(node, "scale_value", &ext_param->dsc_params_spr_in.scale_value);
		rc = utils->get_int(node, "increment_interval", &ext_param->dsc_params_spr_in.increment_interval);
		rc = utils->get_int(node, "decrement_interval", &ext_param->dsc_params_spr_in.decrement_interval);
		rc = utils->get_int(node, "line_bpg_offset", &ext_param->dsc_params_spr_in.line_bpg_offset);
		rc = utils->get_int(node, "nfl_bpg_offset", &ext_param->dsc_params_spr_in.nfl_bpg_offset);
		rc = utils->get_int(node, "slice_bpg_offset", &ext_param->dsc_params_spr_in.slice_bpg_offset);
		rc = utils->get_int(node, "initial_offset", &ext_param->dsc_params_spr_in.initial_offset);
		rc = utils->get_int(node, "final_offset", &ext_param->dsc_params_spr_in.final_offset);
		rc = utils->get_int(node, "flatness_minqp", &ext_param->dsc_params_spr_in.flatness_minqp);
		rc = utils->get_int(node, "flatness_maxqp", &ext_param->dsc_params_spr_in.flatness_maxqp);
		rc = utils->get_int(node, "rc_model_size", &ext_param->dsc_params_spr_in.rc_model_size);
		rc = utils->get_int(node, "rc_edge_factor", &ext_param->dsc_params_spr_in.rc_edge_factor);
		rc = utils->get_int(node, "rc_quant_incr_limit0", &ext_param->dsc_params_spr_in.rc_quant_incr_limit0);
		rc = utils->get_int(node, "rc_quant_incr_limit1", &ext_param->dsc_params_spr_in.rc_quant_incr_limit1);
		rc = utils->get_int(node, "rc_tgt_offset_hi", &ext_param->dsc_params_spr_in.rc_tgt_offset_hi);
		rc = utils->get_int(node, "rc_tgt_offset_lo", &ext_param->dsc_params_spr_in.rc_tgt_offset_lo);
		child = of_get_child_by_name(node, "ext_pps_cfg");
		if (child) {
			rc = utils->get_int(child, "enable", &ext_param->dsc_params_spr_in.ext_pps_cfg.enable);
			spr_count = of_property_count_elems_of_size(child, "oplus,rc-buf-thresh", sizeof(int));
			if (spr_count > 0) {
				spr_data = kzalloc(spr_count * sizeof(int), GFP_KERNEL);
				if (spr_data) {
					ext_param->dsc_params_spr_in.ext_pps_cfg.rc_buf_thresh = spr_data;
					rc = utils->get_intarr(child, "oplus,rc-buf-thresh", spr_data, spr_count);
					if (rc < 0) {
						kfree(spr_data);
						spr_data = NULL;
					}
				}
			}
			spr_count = of_property_count_elems_of_size(child, "oplus,range-min-qp", sizeof(int));
			if (spr_count > 0) {
				spr_data = kzalloc(spr_count * sizeof(int), GFP_KERNEL);
				if (spr_data) {
					ext_param->dsc_params_spr_in.ext_pps_cfg.range_min_qp = spr_data;
					rc = utils->get_intarr(child, "oplus,range-min-qp", spr_data, spr_count);
					if (rc < 0) {
						kfree(spr_data);
						spr_data = NULL;
					}
				}
			}
			spr_count = of_property_count_elems_of_size(child, "oplus,range-max-qp", sizeof(int));
			if (spr_count > 0) {
				spr_data = kzalloc(spr_count * sizeof(int), GFP_KERNEL);
				if (spr_data) {
					ext_param->dsc_params_spr_in.ext_pps_cfg.range_max_qp = spr_data;
					rc = utils->get_intarr(child, "oplus,range-max-qp", spr_data, spr_count);
					if (rc < 0) {
						kfree(spr_data);
						spr_data = NULL;
					}
				}
			}
			spr_count = of_property_count_elems_of_size(child, "oplus,range-bpg-ofs", sizeof(int));
			if (spr_count > 0) {
				spr_data = kzalloc(spr_count * sizeof(int), GFP_KERNEL);
				if (spr_data) {
					ext_param->dsc_params_spr_in.ext_pps_cfg.range_bpg_ofs = spr_data;
					rc = utils->get_intarr(child, "oplus,range-bpg-ofs", spr_data, spr_count);
					if (rc < 0) {
						kfree(spr_data);
						spr_data = NULL;
					}
				}
			}
			of_node_put(child);
		}
		of_node_put(node);
	}

	return rc;
}

int oplus_dsi_panel_parse_power_sequence_config(struct device_node *node, void *ctx_dev)
{
	int rc = -EINVAL, power_count = 0, i = 0, supply_gpio = -1;
	char *str_ptr = NULL;
	struct dsi_panel_lcm *ctx = ctx_dev;
	struct dsi_parser_utils *utils = ctx->parser_utils;
	struct device_node *supply_entries = NULL, *child = NULL;

	rc = utils->get_string_array(node, "oplus,panel-power-on-sequence", &ctx->power_on_sequence[0]);
	if ((rc < 0) || (rc > (PANEL_POWER_SUPPLY_MAX * PANEL_POWER_SUPPLY_MESSAGE_MAX + 1))) {
		OPLUS_DSI_ERR("oplus,panel-power-on-sequence count = %d error\n", rc);
		rc = -EINVAL;
		return rc;
	}
	power_count = (rc - 1) / 2;

	rc = utils->get_string_array(node, "oplus,panel-power-off-sequence", &ctx->power_off_sequence[0]);
	if ((rc < 0) || (rc > (PANEL_POWER_SUPPLY_MAX * PANEL_POWER_SUPPLY_MESSAGE_MAX + 1))
				||(power_count != ((rc - 1) / 2))) {
		OPLUS_DSI_ERR("oplus,panel-power-off-sequence count = %d and oplus,panel-power-on-sequence = %d, "
						"max support = %d error\n", rc, power_count * 2 + 1,
						(PANEL_POWER_SUPPLY_MAX * PANEL_POWER_SUPPLY_MESSAGE_MAX + 1));
		rc = -EINVAL;
		return rc;
	}

	supply_entries = of_parse_phandle(node, "oplus,panel-supply-entries", 0);
	if (supply_entries) {
		for_each_child_of_node(supply_entries, child) {
			if (of_node_name_eq(child, "oplus,panel-supply-entry")) {
				ctx->power_info.count++;
			}
		}
		if ((ctx->power_info.count == 0) || (ctx->power_info.count != power_count)) {
			OPLUS_DSI_ERR("supply_entries count should be %d, but %d\n", power_count, ctx->power_info.count);
			of_node_put(supply_entries);
			rc = -EINVAL;
			return rc;
		}
		ctx->power_info.vregs = devm_kcalloc(ctx->dev, ctx->power_info.count, sizeof(struct dsi_vreg), GFP_KERNEL);
		if (!ctx->power_info.vregs) {
			OPLUS_DSI_ERR("supply_entries count is zero\n");
			ctx->power_info.count = 0;
			of_node_put(supply_entries);
			rc = -EINVAL;
			return rc;
		}
		i = 0;
		for_each_child_of_node(supply_entries, child) {
			if (of_node_name_eq(child, "oplus,panel-supply-entry")) {
				str_ptr = ctx->power_info.vregs[i].vreg_name;
				rc = utils->get_string(child, "oplus,supply-name", &str_ptr);
				if (rc < 0) {
					OPLUS_DSI_WARN("oplus,supply-name Parsing failed, %d\n", i);
					continue;
				}
				strncpy(ctx->power_info.vregs[i].vreg_name, str_ptr, strlen(str_ptr));
				rc = utils->get_int(child, "oplus,supply-type", &ctx->power_info.vregs[i].vreg_type);
				if (rc < 0) {
					OPLUS_DSI_WARN("[%s] oplus,supply-type not set, default ldo\n", ctx->power_info.vregs[i].vreg_name);
					ctx->power_info.vregs[i].vreg_type = 0;
				}
				if (ctx->power_info.vregs[i].vreg_type == 0) {
					ctx->power_info.vregs[i].vreg = devm_regulator_get(ctx->dev, ctx->power_info.vregs[i].vreg_name);
					if (IS_ERR(ctx->power_info.vregs[i].vreg)) {
						OPLUS_DSI_ERR("cannot get %s ldo, ret=%ld\n", ctx->power_info.vregs[i].vreg_name,
										PTR_ERR(ctx->power_info.vregs[i].vreg));
						of_node_put(child);
						of_node_put(supply_entries);
						rc = -EINVAL;
						return rc;
					}
					utils->get_int(child, "oplus,supply-min-voltage", &ctx->power_info.vregs[i].min_voltage);
					utils->get_int(child, "oplus,supply-max-voltage", &ctx->power_info.vregs[i].max_voltage);
					if ((ctx->power_info.vregs[i].min_voltage == 0) || (ctx->power_info.vregs[i].max_voltage == 0)) {
						OPLUS_DSI_ERR("ldo regulator %s minv = %d maxv = %d\n", ctx->power_info.vregs[i].vreg_name,
										ctx->power_info.vregs[i].min_voltage,
										ctx->power_info.vregs[i].max_voltage);
						of_node_put(child);
						of_node_put(supply_entries);
						rc = -EINVAL;
						return rc;
					}
					OPLUS_DSI_DEBUG("[%s] minv=%d maxv=%d\n",
									ctx->power_info.vregs[i].vreg_name,
									ctx->power_info.vregs[i].min_voltage,
									ctx->power_info.vregs[i].max_voltage);
				} else if (ctx->power_info.vregs[i].vreg_type == 1) {
					supply_gpio = -1;
					supply_gpio = of_get_named_gpio(child, "oplus,supply-gpios", 0);
					if (!gpio_is_valid(supply_gpio)) {
						supply_gpio = -1;
						OPLUS_DSI_ERR("cannot get %s gpio\n", ctx->power_info.vregs[i].vreg_name);
						of_node_put(child);
						of_node_put(supply_entries);
						rc = -EINVAL;
						return rc;
					}
					ctx->power_info.vregs[i].vreg_gpio = gpio_to_desc(supply_gpio);

					if (IS_ERR(ctx->power_info.vregs[i].vreg_gpio)) {
						OPLUS_DSI_ERR("cannot get %s gpio desc\n", ctx->power_info.vregs[i].vreg_name);
						of_node_put(child);
						of_node_put(supply_entries);
						rc = -EINVAL;
						return rc;
					} else {
						if (gpio_request(supply_gpio, NULL)) {
							OPLUS_DSI_ERR("cannot get %s gpio request\n", ctx->power_info.vregs[i].vreg_name);
							of_node_put(child);
							of_node_put(supply_entries);
							rc = -EINVAL;
							return rc;
						} else {
							if (gpiod_direction_output(ctx->power_info.vregs[i].vreg_gpio, 1)) {
								OPLUS_DSI_ERR("cannot set %s gpio output\n", ctx->power_info.vregs[i].vreg_name);
								of_node_put(child);
								of_node_put(supply_entries);
								rc = -EINVAL;
								return rc;
							} else {
								OPLUS_DSI_DEBUG("[%s] set gpio %d success\n",
									ctx->power_info.vregs[i].vreg_name, supply_gpio);
							}
						}
					}
				}
				++i;
			}
		}
		of_node_put(supply_entries);
	} else {
		OPLUS_DSI_ERR("supply_entries node is null\n");
		rc = -EINVAL;
		return rc;
	}

	return rc;
}

int oplus_dsi_panel_parse_reset_sequence_config(struct device_node *node, void *ctx_dev)
{
	int rc = -EINVAL, reset_gpio = -1;
	struct dsi_panel_lcm *ctx = ctx_dev;
	struct dsi_parser_utils *utils = ctx->parser_utils;

	rc = utils->get_intarr(node, "oplus,panel-reset-sequence", &ctx->panel_reset_sequence[0][0],
		sizeof(ctx->panel_reset_sequence)/sizeof(ctx->panel_reset_sequence[0][0]));
	if (rc < 0) {
		OPLUS_DSI_ERR("oplus,panel-reset-sequence error\n");
		return rc;
	}

	reset_gpio = of_get_named_gpio(node, "reset-gpios", 0);
	if (!gpio_is_valid(reset_gpio)) {
		reset_gpio = -1;
		OPLUS_DSI_ERR("cannot get reset-gpios\n");
		rc = -EINVAL;
		return rc;
	}
	ctx->reset_gpio = gpio_to_desc(reset_gpio);

	if (IS_ERR(ctx->reset_gpio)) {
		OPLUS_DSI_ERR("cannot get reset-gpios desc\n");
		rc = -EINVAL;
		return rc;
	} else {
		if (gpio_request(reset_gpio, NULL)) {
			OPLUS_DSI_ERR("cannot get reset-gpios request\n");
			rc = -EINVAL;
			return rc;
		} else {
			if (gpiod_direction_output(ctx->reset_gpio, 1)) {
				OPLUS_DSI_ERR("cannot set reset-gpios output\n");
				rc = -EINVAL;
				return rc;
			} else {
				OPLUS_DSI_DEBUG("set reset-gpios %d success\n", reset_gpio);
			}
		}
	}

	return rc;
}

int oplus_dsi_panel_parse_mipi_err(struct device_node *node, void *ctx_dev)
{
	int rc = -EINVAL, i = 0, j = 0;
	u32 test_len, check_value_count;
	u8 *lenp;
	struct dsi_panel_lcm *ctx = ctx_dev;
	struct oplus_panel_regs_check_config *mipi_param = &ctx->mipi_err_config;
	struct dsi_parser_utils *utils = ctx->parser_utils;

	rc = utils->get_int(node, "oplus,mipi-err-check-config",
			&ctx->mipi_err_config.config);
	if ((rc < 0) || !(ctx->mipi_err_config.config & OPLUS_REGS_CHECK_ENABLE)) {
		OPLUS_DSI_WARN("parse oplus,mipi-err-check-config=0x%X, rc=%d\n",
				ctx->mipi_err_config.config, rc);
		ctx->mipi_err_config.enter_cmd = DSI_CMD_ID_MAX;
		ctx->mipi_err_config.exit_cmd = DSI_CMD_ID_MAX;
		return rc;
	}
	ctx->mipi_err_config.enter_cmd = DSI_CMD_MIPI_ERR_CHECK_ENTER;
	ctx->mipi_err_config.exit_cmd = DSI_CMD_MIPI_ERR_CHECK_EXIT;

	rc = utils->get_property(node, "oplus,mipi-err-check-reg",
			ctx->mipi_err_config.check_regs);
	if (rc < 0) {
		return rc;
	}
	ctx->mipi_err_config.reg_count = rc;

	rc = utils->get_property(node, "oplus,mipi-err-check-count",
			ctx->mipi_err_config.check_regs_rlen);
	if (rc < 0) {
		return rc;
	}
	test_len = 0;
	lenp = ctx->mipi_err_config.check_regs_rlen;
	for (i = 0; i < ctx->mipi_err_config.reg_count; ++i) {
		test_len += lenp[i];
	}
	check_value_count = of_property_count_elems_of_size(node,
			"oplus,mipi-err-check-value", sizeof(char));
	if (check_value_count <= 0) {
		return -EINVAL;
	}
	ctx->mipi_err_config.groups = check_value_count / test_len;
	ctx->mipi_err_config.check_value = kzalloc(sizeof(u8) * check_value_count, GFP_KERNEL);
	if (!ctx->mipi_err_config.check_value) {
		rc = -ENOMEM;
		goto error1;
	}

	rc = utils->get_property(node, "oplus,mipi-err-check-value",
			ctx->mipi_err_config.check_value);
	if (rc < 0) {
		goto error1;
	}
	rc = utils->get_int(node, "oplus,mipi-err-check-match-modes",
			&ctx->mipi_err_config.match_modes);

	ctx->mipi_err_config.return_buf = kzalloc(test_len, GFP_KERNEL);
	if (!ctx->mipi_err_config.return_buf) {
		rc = -ENOMEM;
		goto error2;
	}
	ctx->mipi_err_config.check_buf = kzalloc(test_len, GFP_KERNEL);
	if (!ctx->mipi_err_config.check_buf) {
		rc = -ENOMEM;
		goto error3;
	}

	OPLUS_DSI_INFO("Success parse mipi err check config=0x%X, rc=%d\n",
			ctx->mipi_err_config.config, rc);
	return rc;
error3:
	if (ctx->mipi_err_config.check_buf) {
		kfree(ctx->mipi_err_config.check_buf);
		ctx->mipi_err_config.check_buf = NULL;
	}
error2:
	if (ctx->mipi_err_config.return_buf) {
		kfree(ctx->mipi_err_config.return_buf);
		ctx->mipi_err_config.return_buf = NULL;
	}
error1:
	if (ctx->mipi_err_config.check_value) {
		kfree(ctx->mipi_err_config.check_value);
		ctx->mipi_err_config.check_value = NULL;
	}

	return rc;
}

int oplus_dsi_panel_parse_crc(struct device_node *node, void *ctx_dev)
{
	int rc = -EINVAL, i = 0, j = 0;
	u32 test_len, check_value_count;
	u8 *lenp;
	struct dsi_panel_lcm *ctx = ctx_dev;
	struct oplus_panel_regs_check_config *crc_param = &ctx->crc_config;
	struct dsi_parser_utils *utils = ctx->parser_utils;

	rc = utils->get_int(node, "oplus,crc-check-config",
			&ctx->crc_config.config);
	if ((rc < 0) || !(ctx->crc_config.config & OPLUS_REGS_CHECK_ENABLE)) {
		OPLUS_DSI_WARN("parse oplus,crc-check-config=0x%X, rc=%d\n",
				ctx->crc_config.config, rc);
		ctx->crc_config.enter_cmd = DSI_CMD_ID_MAX;
		ctx->crc_config.exit_cmd = DSI_CMD_ID_MAX;
		return rc;
	}
	ctx->crc_config.enter_cmd = DSI_CMD_CRC_CHECK_ENTER;
	ctx->crc_config.exit_cmd = DSI_CMD_CRC_CHECK_EXIT;

	rc = utils->get_property(node, "oplus,crc-check-reg",
			ctx->crc_config.check_regs);
	if (rc < 0) {
		return rc;
	}
	ctx->crc_config.reg_count = rc;

	rc = utils->get_property(node, "oplus,crc-check-count",
			ctx->crc_config.check_regs_rlen);
	if (rc < 0) {
		return rc;
	}
	test_len = 0;
	lenp = ctx->crc_config.check_regs_rlen;
	for (i = 0; i < ctx->crc_config.reg_count; ++i) {
		test_len += lenp[i];
	}
	check_value_count = of_property_count_elems_of_size(node,
			"oplus,crc-check-value", sizeof(char));
	if (check_value_count <= 0) {
		return -EINVAL;
	}
	ctx->crc_config.groups = check_value_count / test_len;
	ctx->crc_config.check_value = kzalloc(sizeof(u8) * check_value_count, GFP_KERNEL);
	if (!ctx->crc_config.check_value) {
		rc = -ENOMEM;
		goto error1;
	}

	rc = utils->get_property(node, "oplus,crc-check-value",
			ctx->crc_config.check_value);
	if (rc < 0) {
		goto error1;
	}
	rc = utils->get_int(node, "oplus,crc-check-match-modes",
			&ctx->crc_config.match_modes);

	ctx->crc_config.return_buf = kzalloc(test_len, GFP_KERNEL);
	if (!ctx->crc_config.return_buf) {
		rc = -ENOMEM;
		goto error2;
	}
	ctx->crc_config.check_buf = kzalloc(test_len, GFP_KERNEL);
	if (!ctx->crc_config.check_buf) {
		rc = -ENOMEM;
		goto error3;
	}

	OPLUS_DSI_INFO("Success parse crc check config=0x%X, rc=%d\n",
			ctx->crc_config.config, rc);
	return rc;
error3:
	if (ctx->crc_config.check_buf) {
		kfree(ctx->crc_config.check_buf);
		ctx->crc_config.check_buf = NULL;
	}
error2:
	if (ctx->crc_config.return_buf) {
		kfree(ctx->crc_config.return_buf);
		ctx->crc_config.return_buf = NULL;
	}
error1:
	if (ctx->crc_config.check_value) {
		kfree(ctx->crc_config.check_value);
		ctx->crc_config.check_value = NULL;
	}

	return rc;
}
