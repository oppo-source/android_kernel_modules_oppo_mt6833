/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_display_interface.c
** Description : oplus display interface
** Version : 1.0
** Date : 2024/04/28
** Author : Display
******************************************************************/
#include <linux/of_platform.h>
#include "mtk_dsi.h"
#include "mtk_dp_common.h"

#include "oplus_display_interface.h"
#include "oplus_display_utils.h"
#include "oplus_display_debug.h"
#include "oplus_display_sysfs_attrs.h"
#ifdef OPLUS_FEATURE_DISPLAY_ADFR
#include "oplus_adfr.h"
#endif /* OPLUS_FEATURE_DISPLAY_ADFR */
#ifdef OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT
#include "oplus_display_onscreenfingerprint.h"
#endif /* OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT */
#ifdef OPLUS_FEATURE_DISPLAY_HPWM
#include "oplus_display_pwm.h"
#endif /* OPLUS_FEATURE_DISPLAY_HPWM */

bool g_dp_support;
EXPORT_SYMBOL(g_dp_support);

struct dsi_panel_lcm* oplus_mtkCrtc_to_panel(struct mtk_drm_crtc *mtk_crtc)
{
	struct mtk_ddp_comp *comp = NULL;
	struct mtk_dsi *dsi = NULL;
	struct dsi_panel_lcm *ctx = NULL;

	if (!mtk_crtc) {
		OPLUS_DSI_DEBUG("invalid mtk_crtc\n");
		return NULL;
	}

	comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (!comp) {
		OPLUS_DSI_DEBUG("invalid comp\n");
		return NULL;
	}

	dsi = container_of(comp, struct mtk_dsi, ddp_comp);
	if (!dsi) {
		OPLUS_DSI_DEBUG("invalid dsi\n");
		return NULL;
	}

	ctx = container_of(dsi->panel, struct dsi_panel_lcm, panel);
	if (!ctx) {
		OPLUS_DSI_DEBUG("invalid ctx\n");
		return NULL;
	}

	return ctx;
}
EXPORT_SYMBOL(oplus_mtkCrtc_to_panel);

struct dsi_panel_lcm* oplus_mtkDsi_to_panel(struct mtk_dsi *dsi)
{
	struct dsi_panel_lcm *ctx = NULL;

	if (!dsi) {
		OPLUS_DSI_DEBUG("invalid dsi\n");
		return NULL;
	}

	ctx = container_of(dsi->panel, struct dsi_panel_lcm, panel);
	if (!ctx) {
		OPLUS_DSI_DEBUG("invalid ctx\n");
		return NULL;
	}

	return ctx;
}
EXPORT_SYMBOL(oplus_mtkDsi_to_panel);

void *oplus_dsi_display_get_driver_data(const char *compatible_name)
{
	struct device_node *node = NULL;
	struct platform_device *pdev = NULL;
	void *driver_data = NULL;

	if (!compatible_name) {
		OPLUS_DSI_ERR("Invalid compatible_name\n");
		return NULL;
	}
	node = of_find_compatible_node(NULL, NULL, compatible_name);
	if (node) {
		pdev = of_find_device_by_node(node);
		if (pdev) {
			driver_data = dev_get_drvdata(&pdev->dev);
		} else {
			OPLUS_DSI_ERR("Cannot find pdev by node\n");
			of_node_put(node);
			return NULL;
		}
		of_node_put(node);
	} else {
		OPLUS_DSI_ERR("Cannot find node by %s\n", compatible_name);
		return NULL;
	}

	return driver_data;
}

bool oplus_dsi_display_get_dp_support(void)
{
	bool value = false;
	struct mtk_dp *mtk_dp;
	mtk_dp = oplus_dsi_display_get_driver_data("mediatek,dp_tx");
	if (mtk_dp) {
		value = mtk_dp->oplus_dp_support;
	} else {
		OPLUS_DSI_ERR("get dp support failed and return value is default: %d\n", value);
	}
	OPLUS_DSI_INFO("get dp support is: %d\n", value);

	return value;
}

int oplus_panel_config_parse(struct device_node *node, void *ctx)
{
	int ret = 0;

	ret = oplus_serial_number_probe(node);
	if (ret < 0) {
		OPLUS_DSI_WARN("oplus_serial_number_probe failed\n");
	}
	ret = oplus_dsi_panel_parse_cmd_params(node, ctx);
	if (ret < 0) {
		OPLUS_DSI_ERR("skip probe due to oplus_dsi_panel_parse_cmd_params error\n");
		return ret;
	}

	return 0;
}

/* Interface function for new driver architecture */
int oplus_dsi_display_parse(struct device_node *node, void *ctx)
{
	int ret = 0;

#ifdef OPLUS_FEATURE_DISPLAY_ADFR
	oplus_dsi_display_adfr_init(node, ctx);
#endif /* OPLUS_FEATURE_DISPLAY_ADFR */
#ifdef OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT
	ret = oplus_ofp_init(node);
	if (ret < 0) {
		OPLUS_DSI_ERR("oplus_ofp_init error\n");
	}
#endif /* OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT */
#ifdef OPLUS_FEATURE_DISPLAY_HPWM
	ret = oplus_pwm_turbo_probe(node);
	if (ret < 0) {
		OPLUS_DSI_ERR("oplus_pwm_turbo_probe error\n");
	}
#endif /* OPLUS_FEATURE_DISPLAY_HPWM */
	ret = oplus_panel_config_parse(node, ctx);
	if (ret < 0) {
		OPLUS_DSI_ERR("skip probe due to oplus_panel_config_parse error\n");
		return ret;
	}

	return 0;
}

/* Interface function for old driver architecture */
int oplus_panel_parse(struct device_node *node, bool is_primary)
{
	int ret = 0;

#ifdef OPLUS_FEATURE_DISPLAY_ADFR
	oplus_adfr_init(node, is_primary);
#endif /* OPLUS_FEATURE_DISPLAY_ADFR */
#ifdef OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT
	ret = oplus_ofp_init(node);
	if (ret < 0) {
		OPLUS_DSI_ERR("oplus_ofp_init error\n");
	}
#endif /* OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT */
#ifdef OPLUS_FEATURE_DISPLAY_HPWM
	ret = oplus_pwm_turbo_probe(node);
	if (ret < 0) {
		OPLUS_DSI_ERR("oplus_pwm_turbo_probe error\n");
	}
#endif /* OPLUS_FEATURE_DISPLAY_HPWM */
	ret = oplus_serial_number_probe(node);
	if (ret < 0) {
		OPLUS_DSI_ERR("oplus_serial_number_probe error\n");
	}

	return 0;
}
EXPORT_SYMBOL(oplus_panel_parse);

int oplus_panel_init(struct drm_crtc *crtc)
{
	static bool all_done = false;
	static bool variable_init_done = false;
	static bool panel_id_init_done = false;
	static bool panel_sn_init_done = false;
	static bool panel_ext_init_done = false;

	if (all_done) {
		return 0;
	}

	OPLUS_DSI_INFO("Start\n");

	if (!variable_init_done) {
		OPLUS_DSI_INFO("Do variable init\n");
		g_dp_support = oplus_dsi_display_get_dp_support();
		variable_init_done = true;
	}

	if (!panel_id_init_done) {
		OPLUS_DSI_INFO("Do panel id init\n");
		if (!(m_da || m_db || m_dc)) {
			OPLUS_DSI_WARN("Failed to parse PanelID from cmdlline, read it again\n");
			if (!oplus_display_read_panel_id(crtc)) {
				panel_id_init_done = true;
			}
		} else {
			panel_id_init_done = true;
		}
	}

	if (!panel_sn_init_done) {
		OPLUS_DSI_INFO("Do panel sn init\n");
		if (!oplus_panel_serial_number_read(crtc)) {
			panel_sn_init_done = true;
		}
	}

	if (!panel_ext_init_done) {
		OPLUS_DSI_INFO("Do panel ext init\n");
		if (!oplus_panel_ext_init(crtc)) {
			panel_ext_init_done = true;
		}
	}

	if (variable_init_done && panel_id_init_done && panel_sn_init_done && panel_ext_init_done) {
		all_done = true;
	}

	OPLUS_DSI_INFO("End\n");

	return 0;
}
