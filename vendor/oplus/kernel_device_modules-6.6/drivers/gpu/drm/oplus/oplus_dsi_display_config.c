/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_dsi_display_config.c
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

#include "oplus_display_interface.h"
#include "oplus_dsi_display_parser.h"
#include "oplus_dsi_display_config.h"
#include "oplus_display_sysfs_attrs.h"
#include "oplus_display_onscreenfingerprint.h"
#include "oplus_display_power.h"
#include "oplus_adfr.h"
#include "oplus_display_debug.h"

struct dsi_panel_lcm *oplus_display0_params = NULL;
EXPORT_SYMBOL(oplus_display0_params);
struct dsi_panel_lcm *oplus_display1_params = NULL;
EXPORT_SYMBOL(oplus_display1_params);
char oplus_display0_cmdline[MAX_CMDLINE_PARAM_LEN] = {0};
EXPORT_SYMBOL(oplus_display0_cmdline);
char oplus_display1_cmdline[MAX_CMDLINE_PARAM_LEN] = {0};
EXPORT_SYMBOL(oplus_display1_cmdline);
unsigned int m_flag;
EXPORT_SYMBOL(m_flag);
unsigned int m_da;
EXPORT_SYMBOL(m_da);
unsigned int m_db;
EXPORT_SYMBOL(m_db);
unsigned int m_dc;
EXPORT_SYMBOL(m_dc);

u8 oplus_dsi_panel_get_panel_flag(struct mtk_dsi *mtk_dsi)
{
	struct dsi_panel_lcm *ctx = NULL;

	if (!mtk_dsi) {
		OPLUS_DSI_ERR("Invalid mtk_dsi\n");
		return 0;
	}

	ctx = container_of(mtk_dsi->panel, struct dsi_panel_lcm, panel);
	if (!ctx) {
		OPLUS_DSI_ERR("Invalid ctx\n");
		return 0;
	}

	return ctx->panel_info.panel_flag;
}
EXPORT_SYMBOL(oplus_dsi_panel_get_panel_flag);

u8 oplus_dsi_panel_get_panel_id1(struct mtk_dsi *mtk_dsi)
{
	struct dsi_panel_lcm *ctx = NULL;

	if (!mtk_dsi) {
		OPLUS_DSI_ERR("Invalid mtk_dsi\n");
		return 0;
	}

	ctx = container_of(mtk_dsi->panel, struct dsi_panel_lcm, panel);
	if (!ctx) {
		OPLUS_DSI_ERR("Invalid ctx\n");
		return 0;
	}

	return ctx->panel_info.panel_id1;
}
EXPORT_SYMBOL(oplus_dsi_panel_get_panel_id1);

u8 oplus_dsi_panel_get_panel_id2(struct mtk_dsi *mtk_dsi)
{
	struct dsi_panel_lcm *ctx = NULL;

	if (!mtk_dsi) {
		OPLUS_DSI_ERR("Invalid mtk_dsi\n");
		return 0;
	}

	ctx = container_of(mtk_dsi->panel, struct dsi_panel_lcm, panel);
	if (!ctx) {
		OPLUS_DSI_ERR("Invalid ctx\n");
		return 0;
	}

	return ctx->panel_info.panel_id2;
}
EXPORT_SYMBOL(oplus_dsi_panel_get_panel_id2);

u8 oplus_dsi_panel_get_panel_id3(struct mtk_dsi *mtk_dsi)
{
	struct dsi_panel_lcm *ctx = NULL;

	if (!mtk_dsi) {
		OPLUS_DSI_ERR("Invalid mtk_dsi\n");
		return 0;
	}

	ctx = container_of(mtk_dsi->panel, struct dsi_panel_lcm, panel);
	if (!ctx) {
		OPLUS_DSI_ERR("Invalid ctx\n");
		return 0;
	}

	return ctx->panel_info.panel_id3;
}
EXPORT_SYMBOL(oplus_dsi_panel_get_panel_id3);

u32 oplus_dsi_panel_get_panel_info(struct mtk_dsi *mtk_dsi)
{
	if (!mtk_dsi) {
		OPLUS_DSI_ERR("Invalid mtk_dsi\n");
		return 0;
	}

	return (oplus_dsi_panel_get_panel_flag(mtk_dsi) << 24)
			| (oplus_dsi_panel_get_panel_id1(mtk_dsi) << 16)
			| (oplus_dsi_panel_get_panel_id2(mtk_dsi) << 8)
			| oplus_dsi_panel_get_panel_id3(mtk_dsi);
}
EXPORT_SYMBOL(oplus_dsi_panel_get_panel_info);

int oplus_dsi_panel_parse_param(struct device_node *dst_node, struct dsi_panel_lcm *ctx)
{
	int rc = -EINVAL, i = 0;
	struct dsi_parser_utils *utils = ctx->parser_utils;

	ctx->ext_params_all = kzalloc(sizeof(struct mtk_panel_params) * ctx->mode_num, GFP_KERNEL);
	if (!ctx->ext_params_all) {
		OPLUS_DSI_ERR("ext_params_all kzalloc error\n");
		rc = -EINVAL;
		return rc;
	}
	rc = utils->get_int(dst_node, "res-switch", &ctx->res_switch);

	rc = oplus_dsi_panel_parse_power_sequence_config(dst_node, ctx);
	if (rc < 0) {
		OPLUS_DSI_ERR("oplus_dsi_panel_parse_power_sequence_config error\n");
		rc = -EINVAL;
		return rc;
	}
	rc = oplus_dsi_panel_parse_reset_sequence_config(dst_node, ctx);
	if (rc < 0) {
		OPLUS_DSI_ERR("oplus_dsi_panel_parse_reset_sequence_config error\n");
		rc = -EINVAL;
		return rc;
	}

	ctx->display_modes = kzalloc(sizeof(struct drm_display_mode) * ctx->mode_num * (ctx->res_switch ? ctx->res_switch : 1), GFP_KERNEL);
	if (!ctx->display_modes) {
		OPLUS_DSI_ERR("display_modes kzalloc error\n");
		rc = -EINVAL;
		return rc;
	}
	rc = oplus_dsi_panel_parser_base(dst_node, &(ctx->ext_params_all[0]), ctx);
	if (rc < 0) {
		OPLUS_DSI_WARN("oplus_dsi_panel_parser_base failed\n");
	}
	rc = oplus_dsi_panel_parser_effect(dst_node, &(ctx->ext_params_all[0]), ctx);
	if (rc < 0) {
		OPLUS_DSI_WARN("oplus_dsi_panel_parser_effect failed\n");
	}
	rc = oplus_dsi_panel_parser_esd(dst_node, &(ctx->ext_params_all[0]), ctx);
	if (rc < 0) {
		OPLUS_DSI_WARN("oplus_dsi_panel_parser_esd failed\n");
	}
	rc = oplus_dsi_panel_parser_round_corner(dst_node, &(ctx->ext_params_all[0]), ctx);
	if (rc < 0) {
		OPLUS_DSI_WARN("oplus_dsi_panel_parser_cornel failed\n");
	}
	for (i = 1; i < ctx->mode_num; i++) {
		memcpy(&ctx->ext_params_all[i], &(ctx->ext_params_all[0]), sizeof(struct mtk_panel_params));
	}

	oplus_dsi_panel_parser_backlight(dst_node, ctx);
	oplus_dsi_panel_parse_mipi_err(dst_node, ctx);
	oplus_dsi_panel_parse_crc(dst_node, ctx);

	return 0;
}

int oplus_dsi_panel_parse_timing_param(void *timing_dev, void *ctx_dev, int type, int vrefresh)
{
	int rc = 0, cell_index = 0;
	struct device_node *timing = timing_dev, *node;
	struct dsi_panel_lcm *ctx = ctx_dev;
	struct dsi_parser_utils *utils = ctx->parser_utils;

	if ((type == 0) && (vrefresh == 60)) {
		cell_index = 1;
	} else if ((type == 0) && (vrefresh == 90)) {
		cell_index = 2;
	} else if ((type == 0) && (vrefresh == 120)) {
		cell_index = 0;
	} else if ((type == 2) && (vrefresh == 120)) {
		cell_index = 3;
	}
	node = of_parse_phandle(timing, "oplus,dsc-params-entries", 0);
	if (node) {
		oplus_dsi_panel_parser_dsc(node, &ctx->ext_params_all[cell_index], ctx);
		of_node_put(node);
	} else {
		OPLUS_DSI_ERR("dsc node is null\n");
	}
	node = of_parse_phandle(timing, "oplus,spr-params-entries", 0);
	if (node) {
		oplus_dsi_panel_parser_spr(node, &ctx->ext_params_all[cell_index], ctx);
		of_node_put(node);
	} else {
		OPLUS_DSI_WARN("spr node is null\n");
	}
	oplus_dsi_panel_parser_timing_base(timing, &ctx->ext_params_all[cell_index], ctx);
	for_each_child_of_node(timing, node) {
		if (of_node_name_eq(node, "oplus,phy-timcon")) {
			oplus_dsi_panel_parser_phy_timcon(node, &ctx->ext_params_all[cell_index], ctx);
		} else if (of_node_name_eq(node, "oplus,dyn-fps")) {
			oplus_dsi_panel_parser_dyn_fps(node, &ctx->ext_params_all[cell_index], ctx);
		} else if (of_node_name_eq(node, "oplus,display-mode")) {
			oplus_dsi_panel_parser_display_modes(cell_index, vrefresh, node, &ctx->ext_params_all[cell_index], ctx);
		}
	}

	return rc;
}

static inline int oplus_of_get_child_count(const struct device_node *np)
{
	struct device_node *child;
	int num = 0;

	for_each_child_of_node(np, child) {
		if (strcmp("timing", child->name) == 0) {
			num++;
		}
	}

	return num;
}

int oplus_dsi_panel_parse_cmd_params(void *node_dev, void *lcm_ctx)
{
	int ret = 0, num_timings = 0;
	struct device_node *node = node_dev;
	struct device_node *timings_np;
	struct dsi_panel_lcm *ctx = lcm_ctx;

	if (!ctx || !node) {
		OPLUS_DSI_ERR("invalid input parameters: no display node or ctx\n");
		return -EINVAL;
	}

	timings_np = of_get_child_by_name(node, "oplus,dsi-display-timings");
	if (!timings_np) {
		OPLUS_DSI_ERR("no display timing nodes defined\n");
		ret = -EINVAL;
		return ret;
	}
	num_timings = oplus_of_get_child_count(timings_np);
	OPLUS_DSI_INFO("num_timings is %d\n", num_timings);
	if (!num_timings) {
		OPLUS_DSI_ERR("skip probe due to num_timings error\n");
		of_node_put(timings_np);
		return ret;
	}
	ctx->mode_num = num_timings;
	ret = oplus_dsi_panel_parse_param(node, ctx);
	if (ret < 0) {
		OPLUS_DSI_ERR("skip probe due to oplus_dsi_panel_parse_param error\n");
		of_node_put(timings_np);
		return ret;
	}
	ret = oplus_dsi_panel_parse_cmd(timings_np, ctx);
	if (ret < 0) {
		OPLUS_DSI_ERR("skip probe due to oplus_dsi_panel_parse_cmd error\n");
		of_node_put(timings_np);
		return ret;
	}
	of_node_put(timings_np);

	return ret;
}

int oplus_dsi_panel_get_node_from_dts(struct dsi_panel_lcm *ctx,
		struct device_node *src_node, struct device_node **dst_node)
{
	int rc = -1;
	struct device_node *oplus_panel, *panel_np;
	const char *panel_ver;
	char str[8] = {0};
	struct dsi_panel_info panel_info = ctx->panel_info;
	int cnt = 0;
	char lcm_node_name[MAX_CMDLINE_PARAM_LEN] = {0};
	u8 panel_version = panel_info.panel_flag & 0x0F;

	cnt = snprintf(lcm_node_name, strlen(OPLUS_PANEL_NODE_PRE_WORD) + 1, "%s", OPLUS_PANEL_NODE_PRE_WORD);
	cnt = snprintf(lcm_node_name + cnt, MAX_CMDLINE_PARAM_LEN, "%s", panel_info.panel_name);
	OPLUS_DSI_INFO("panel_node_name=%s, panel_version=0x%X\n", lcm_node_name, panel_version);

	oplus_panel = of_get_child_by_name(src_node, lcm_node_name);
	if (oplus_panel) {
		for_each_child_of_node(oplus_panel, panel_np) {
			if (strcmp("oplus,dsi-display-panel", panel_np->name) != 0) {
				continue;
			}
			panel_ver = strchr(kbasename(panel_np->full_name), '@');
			if (panel_ver) {
				snprintf(str, sizeof(str), "%d", panel_version);
				if (strcmp(str, panel_ver + 1) != 0) {
					continue;
				}
			}
			*dst_node = panel_np;
			OPLUS_DSI_INFO("Matched panel_node_name: %s\n", panel_np->full_name);
			of_node_put(oplus_panel);
			rc = 0;
			return rc;
		}
		of_node_put(oplus_panel);
	}

	return rc;
}

int oplus_dsi_display_get_panel_info(struct dsi_panel_lcm *ctx)
{
	unsigned int panel_id_params = 0;
	char *panel_name = ctx->panel_info.panel_name;
	char display_cmdline[MAX_CMDLINE_PARAM_LEN] = {0};
	char *str = NULL;

	if (ctx->is_primary) {
		strncpy(display_cmdline, oplus_display0_cmdline, strlen(oplus_display0_cmdline));
	} else {
		strncpy(display_cmdline, oplus_display1_cmdline, strlen(oplus_display1_cmdline));
	}
	OPLUS_DSI_INFO("dsi_display = %s\n", display_cmdline);

	str = strnstr(display_cmdline, ":", strlen(display_cmdline));
	if (str != NULL) {
		strncpy(panel_name, display_cmdline, str - display_cmdline);
	}
	OPLUS_DSI_INFO("panel_name = %s\n", panel_name);
	str = strnstr(display_cmdline, "PanelID-0x", strlen(display_cmdline));
	if (str) {
		if (sscanf(str, "PanelID-0x%08X", &panel_id_params) != 1) {
			OPLUS_DSI_ERR("invalid PanelID override: %s\n",
					display_cmdline);
			return -EINVAL;
		}
		ctx->panel_info.panel_flag = m_flag = (panel_id_params >> 24) & 0xFF;
		ctx->panel_info.panel_id1 = m_da = (panel_id_params >> 16) & 0xFF;
		ctx->panel_info.panel_id2 = m_db = (panel_id_params >> 8) & 0xFF;
		ctx->panel_info.panel_id3 = m_dc = panel_id_params & 0xFF;
	} else {
		OPLUS_DSI_ERR("invalid cmdline: %s\n",
					display_cmdline);
		return -EINVAL;
	}
	OPLUS_DSI_INFO("Parse cmdline PanelID-0x%08X, Flag=0x%02X, ID1=0x%02X, ID2=0x%02X, ID3=0x%02X\n",
			panel_id_params,
			ctx->panel_info.panel_flag,
			ctx->panel_info.panel_id1,
			ctx->panel_info.panel_id2,
			ctx->panel_info.panel_id3);

	return 0;
}

int oplus_dsi_display_checkdrv(struct device *dev, struct device_node **node)
{
	int ret = 0;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;

	dsi_node = of_get_parent(dev->of_node);
	if (dsi_node) {
		endpoint = of_graph_get_next_endpoint(dsi_node, NULL);
		if (endpoint) {
			remote_node = of_graph_get_remote_port_parent(endpoint);
			if (!remote_node) {
				OPLUS_DSI_ERR("No panel connected,skip probe lcm\n");
				of_node_put(endpoint);
				return -ENODEV;
			}
			OPLUS_DSI_INFO("device node name:%s\n", remote_node->name);
			of_node_put(endpoint);
		}
	}
	if (!dsi_node || (remote_node != dev->of_node)) {
		OPLUS_DSI_ERR("skip probe due to not current lcm\n");
		if (remote_node) {
			of_node_put(remote_node);
		}
		return -ENODEV;
	}
	*node = dsi_node;
	of_node_put(remote_node);

	return ret;
}

int oplus_dsi_display_init(void *dsi_device, void *ctx_dev)
{
	int ret = 0;
	struct mipi_dsi_device *dsi = dsi_device;
	struct dsi_panel_lcm *ctx = NULL;
	struct device *dev = &dsi->dev;
	struct device_node *dsi_node = NULL, *dst_node = NULL;
	char lcm_node_name[MAX_CMDLINE_PARAM_LEN] = {0};
	char lcm_ver_name[50] = {0};

	if (!ctx_dev) {
		OPLUS_DSI_ERR("skip probe due to ctx_dev is NULL\n");
		return ret;
	}
	ctx = ctx_dev;
	ret = oplus_dsi_display_checkdrv(dev, &dsi_node);
	if (ret < 0) {
		OPLUS_DSI_ERR("skip probe due to oplus_dsi_display_checkdrv error\n");
		of_node_put(dsi_node);
		return ret;
	}
	ret = oplus_dsi_display_get_panel_info(ctx);
	if (ret < 0) {
		OPLUS_DSI_ERR("skip probe due to oplus_dsi_display_get_panel_info error\n");
		of_node_put(dsi_node);
		return ret;
	}
	OPLUS_DSI_INFO("[LCM] %s Start\n", lcm_node_name);
	ret = oplus_dsi_panel_get_node_from_dts(ctx, dsi_node, &dst_node);
	if (ret < 0) {
		OPLUS_DSI_ERR("skip probe due to oplus_dsi_panel_get_node_from_dts error\n");
		of_node_put(dsi_node);
		return ret;
	}

	ctx->parser_utils = dsi_parser_get_parser_utils();
	mipi_dsi_set_drvdata(dsi, ctx);
	ctx->dev = &dsi->dev;
	ret = oplus_dsi_display_parse(dst_node, ctx);
	if (ret < 0) {
		OPLUS_DSI_ERR("skip probe due to oplus_dsi_display_parse error\n");
		of_node_put(dst_node);
		of_node_put(dsi_node);
		return ret;
	}
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_LPM | MIPI_DSI_CLOCK_NON_CONTINUOUS;
	of_node_put(dst_node);
	of_node_put(dsi_node);
	return ret;
}
EXPORT_SYMBOL(oplus_dsi_display_init);

static int oplus_dsi_display_dcs_deinit(void *dsi_device, void *ctx_dev)
{
	int ret = 0, i = 0, j = 0;
	struct mipi_dsi_device *dsi = dsi_device;
	struct dsi_panel_lcm *ctx = ctx_dev;

	if (!dsi || !ctx) {
		OPLUS_DSI_ERR("dsi is %d, ctx is %d\n", dsi, ctx);
		ret = -EINVAL;
		return ret;
	}
	for (i = 0; i < ctx->mode_num; i++) {
		for (j = 0; j < DSI_CMD_ID_MAX; j++) {
			OPLUS_KFREE(lcm_timings_cmds[i][j].para_table);
			lcm_timings_cmds[i][j].cmd_lines = 0;
		}
		OPLUS_KFREE(lcm_timings_cmds[i]);
	}
	OPLUS_KFREE(lcm_timings_cmds);

	return ret;
}

static int oplus_dsi_display_ext_params_deinit(void *dsi_device, void *ctx_dev)
{
	int ret = 0, i = 0;
	struct mipi_dsi_device *dsi = dsi_device;
	struct dsi_panel_lcm *ctx = ctx_dev;

	if (!dsi || !ctx) {
		OPLUS_DSI_ERR("dsi is %d, ctx is %d\n", dsi, ctx);
		ret = -EINVAL;
		return ret;
	}
	OPLUS_KFREE(ctx->mipi_err_config.check_value);
	OPLUS_KFREE(ctx->mipi_err_config.return_buf);
	OPLUS_KFREE(ctx->mipi_err_config.check_buf);
	OPLUS_KFREE(ctx->display_modes);
	for (i = 0; i < ctx->mode_num; i++) {
		if (!i) {
			OPLUS_KFREE(ctx->ext_params_all[i].corner_pattern_lt_addr);
			OPLUS_KFREE(ctx->ext_params_all[i].corner_pattern_size_per_line);
		} else {
			ctx->ext_params_all[i].corner_pattern_lt_addr = NULL;
			ctx->ext_params_all[i].corner_pattern_size_per_line = NULL;
		}
		OPLUS_KFREE(ctx->ext_params_all[i].spr_params.spr_ip_params);
		OPLUS_KFREE(ctx->ext_params_all[i].spr_params.spr_ip_shrink_params);
		OPLUS_KFREE(ctx->ext_params_all[i].spr_params.mtk_spr_ip_params);
		OPLUS_KFREE(ctx->ext_params_all[i].dsc_params_spr_in.ext_pps_cfg.rc_buf_thresh);
		OPLUS_KFREE(ctx->ext_params_all[i].dsc_params_spr_in.ext_pps_cfg.range_min_qp);
		OPLUS_KFREE(ctx->ext_params_all[i].dsc_params_spr_in.ext_pps_cfg.range_max_qp);
		OPLUS_KFREE(ctx->ext_params_all[i].dsc_params_spr_in.ext_pps_cfg.range_bpg_ofs);
	}
	OPLUS_KFREE(ctx->ext_params_all);
	if (!IS_ERR(ctx->reset_gpio)) {
		oplus_dsi_panel_reset_keep(ctx, 0);
		gpiod_put(ctx->reset_gpio);
	}
	if (ctx->power_info.refcount) {
		ctx->power_info.refcount = 0;
		oplus_dsi_panel_power_supply_enable(ctx, 0);
	}
	for (i = 0; i < ctx->power_info.count; i++) {
		if (ctx->power_info.vregs[i].vreg_type == 1) {
			if (NULL != ctx->power_info.vregs[i].vreg_gpio) {
				gpiod_put(ctx->power_info.vregs[i].vreg_gpio);
			}
		} else {
			if (NULL != ctx->power_info.vregs[i].vreg) {
				devm_regulator_put(ctx->power_info.vregs[i].vreg);
			}
		}
	}
	ctx->power_info.count = 0;
	if (ctx->power_info.vregs) {
		devm_kfree(ctx->dev, ctx->power_info.vregs);
	}

	return ret;
}

static int oplus_dsi_display_feature_deinit(void *dsi_device, void *ctx_dev)
{
	int ret = 0;
	struct mipi_dsi_device *dsi_dev = dsi_device;
	struct mtk_dsi *dsi = NULL;

	if (!dsi_dev || !ctx_dev) {
		OPLUS_DSI_ERR("dsi is %d, ctx_dev is %d\n", dsi, ctx_dev);
		ret = -EINVAL;
		return ret;
	}
	ret = oplus_dsi_display_serial_number_deinit(ctx_dev);
	if (ret < 0) {
		OPLUS_DSI_WARN("serial number deinit failed\n");
	}
	ret = oplus_ofp_deinit();
	if (ret < 0) {
		OPLUS_DSI_WARN("oplus ofp deinit failed\n");
	}
	ret = oplus_dsi_display_adfr_deinit(ctx_dev);
	if (ret < 0) {
		OPLUS_DSI_WARN("oplus adfr deinit failed\n");
	}
	dsi = container_of(dsi_dev->host, struct mtk_dsi, host);
	if (!(dsi->ddp_comp.mtk_crtc)) {
		OPLUS_DSI_WARN("mtk_crtc is null\n");
		ret = -EINVAL;
		return ret;
	}
	OPLUS_KFREE(dsi->ddp_comp.mtk_crtc->oplus_apollo_br);

	return ret;
}

int oplus_dsi_display_deinit(void *dsi_device, void *ctx_dev)
{
	int ret = 0;
	struct mipi_dsi_device *dsi = dsi_device;
	struct dsi_panel_lcm *ctx = ctx_dev;

	if (!dsi || !ctx) {
		OPLUS_DSI_ERR("skip deinit due to ctx_dev or dsi_device is NULL\n");
		ret = -EINVAL;
		return ret;
	}
	ret = oplus_dsi_display_dcs_deinit(dsi_device, ctx_dev);
	if (ret < 0) {
		OPLUS_DSI_WARN("dcs_deinit failed\n");
	}
	ret = oplus_dsi_display_ext_params_deinit(dsi_device, ctx_dev);
	if (ret < 0) {
		OPLUS_DSI_WARN("ext_params_deinit failed\n");
	}
	ret = oplus_dsi_display_feature_deinit(dsi_device, ctx_dev);
	if (ret < 0) {
		OPLUS_DSI_WARN("feature deinit failed\n");
	}
	if (ctx) {
		devm_kfree(ctx->dev, ctx);
	}

	return ret;
}
EXPORT_SYMBOL(oplus_dsi_display_deinit);

module_param_string(dsi_display0, oplus_display0_cmdline, MAX_CMDLINE_PARAM_LEN, 0600);
module_param_string(dsi_display1, oplus_display1_cmdline, MAX_CMDLINE_PARAM_LEN, 0600);
MODULE_PARM_DESC(oplus_display0_cmdline, "mediatek-drm.dsi_display0=<dsi_display0>");
MODULE_PARM_DESC(oplus_display1_cmdline, "mediatek-drm.dsi_display1=<dsi_display1>");
