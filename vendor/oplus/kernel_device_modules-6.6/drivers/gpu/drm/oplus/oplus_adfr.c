/***************************************************************
** Copyright (C) 2018-2021 OPLUS. All rights reserved.
** File : oplus_adfr.c
** Description : ADFR kernel module
** Version : 1.0
** Date : 2021/07/09
**
** ------------------------------- Revision History: -----------
**  <author>        <data>        <version >        <desc>
**  Gaoxiaolei      2021/07/09        1.0         Build this moudle
******************************************************************/
#include <linux/of_gpio.h>
#include "oplus_display_debug.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_crtc.h"
#include "mtk_log.h"
#include "mtk_drm_mmp.h"
#include "mtk_dsi.h"
#include "oplus_dsi_display_config.h"
#include "oplus_adfr.h"

/* -------------------- macro -------------------- */
/* config bit setting */
#define OPLUS_ADFR_CONFIG_GLOBAL                             (BIT(0))
#define OPLUS_ADFR_CONFIG_FAKEFRAME                          (BIT(1))
#define OPLUS_ADFR_CONFIG_VSYNC_SWITCH                       (BIT(2))
#define OPLUS_ADFR_CONFIG_VSYNC_SWITCH_MODE                  (BIT(3))
#define OPLUS_ADFR_CONFIG_IDLE_MODE                          (BIT(4))
#define OPLUS_ADFR_CONFIG_TEMPERATURE_DETECTION              (BIT(5))
#define OPLUS_ADFR_CONFIG_OA_BL_MUTUAL_EXCLUSION             (BIT(6))
#define OPLUS_ADFR_CONFIG_SA_MODE_RESTORE                    (BIT(7))
#define OPLUS_ADFR_CONFIG_DRY_RUN                            (BIT(8))
#define OPLUS_ADFR_CONFIG_DECREASING_STEP                    (BIT(9))

/* get config value */
#define OPLUS_ADFR_GET_GLOBAL_CONFIG(config)                 ((config) & OPLUS_ADFR_CONFIG_GLOBAL)
#define OPLUS_ADFR_GET_FAKEFRAME_CONFIG(config)              ((config) & OPLUS_ADFR_CONFIG_FAKEFRAME)
#define OPLUS_ADFR_GET_VSYNC_SWITCH_CONFIG(config)           ((config) & OPLUS_ADFR_CONFIG_VSYNC_SWITCH)
#define OPLUS_ADFR_GET_VSYNC_SWITCH_MODE_CONFIG(config)      ((config) & OPLUS_ADFR_CONFIG_VSYNC_SWITCH_MODE)
#define OPLUS_ADFR_GET_IDLE_MODE_CONFIG(config)              ((config) & OPLUS_ADFR_CONFIG_IDLE_MODE)
#define OPLUS_ADFR_GET_TEMPERATURE_DETECTION_CONFIG(config)  ((config) & OPLUS_ADFR_CONFIG_TEMPERATURE_DETECTION)
#define OPLUS_ADFR_GET_OA_BL_MUTUAL_EXCLUSION_CONFIG(config) ((config) & OPLUS_ADFR_CONFIG_OA_BL_MUTUAL_EXCLUSION)
#define OPLUS_ADFR_GET_SA_MODE_RESTORE_CONFIG(config)        ((config) & OPLUS_ADFR_CONFIG_SA_MODE_RESTORE)
#define OPLUS_ADFR_GET_DRY_RUN_CONFIG(config)                ((config) & OPLUS_ADFR_CONFIG_DRY_RUN)
#define OPLUS_ADFR_GET_DECREASING_STEP_CONFIG(config)        ((config) & OPLUS_ADFR_CONFIG_DECREASING_STEP)


#define MTK_DISP_EVENT_ADFR_MIN_FPS	0x15

/* -------------------- parameters -------------------- */
/* log level config */
unsigned int oplus_adfr_log_level = OPLUS_ADFR_LOG_LEVEL_INFO;
EXPORT_SYMBOL(oplus_adfr_log_level);
/* dual display id */
unsigned int oplus_adfr_display_id = OPLUS_ADFR_PRIMARY_DISPLAY;
EXPORT_SYMBOL(oplus_adfr_display_id);
/* adfr global structure */
static struct oplus_adfr_params g_oplus_adfr_params[2] = {0};

/* external variable/function declaration */
extern void lcm_cmd_cmdq_cb(struct cmdq_cb_data data);
extern void lcdinfo_notify(unsigned long val, void *v);

/* --------------- adfr misc ---------------*/
char *oplus_display_get_panel_name(void *drm_crtc) {
	struct drm_crtc *crtc = drm_crtc;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	struct mtk_ddp_comp *comp = NULL;
	char *panel_name = "";

	if (!crtc) {
		ADFR_ERR("Invalid drm_crtc pointer\n");
		return panel_name;
	}

	mtk_crtc = to_mtk_crtc(crtc);
	if (!mtk_crtc) {
		ADFR_ERR("Invalid mtk_drm_crtc pointer");
		return panel_name;
	}

	comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (!comp || !comp->funcs || !comp->funcs->io_cmd) {
		ADFR_ERR("Invalid mtk_ddp_comp pointer");
		return panel_name;
	}

	mtk_ddp_comp_io_cmd(comp, NULL, GET_PANEL_NAME, &panel_name);

	return panel_name;
}

static int oplus_adfr_set_display_id(unsigned int display_id)
{
	ADFR_DEBUG("start\n");

	OPLUS_ADFR_TRACE_BEGIN("oplus_adfr_set_display_id");

	oplus_adfr_display_id = display_id;
	ADFR_DEBUG("oplus_adfr_display_id:%u\n", oplus_adfr_display_id);
	OPLUS_ADFR_TRACE_INT("oplus_adfr_display_id", oplus_adfr_display_id);

	OPLUS_ADFR_TRACE_END("oplus_adfr_set_display_id");

	ADFR_DEBUG("end\n");

	return 0;
}

static struct oplus_adfr_params *oplus_adfr_get_params_by_num(bool is_primary)
{
	if (is_primary) {
		oplus_adfr_set_display_id(OPLUS_ADFR_PRIMARY_DISPLAY);
		return &g_oplus_adfr_params[OPLUS_ADFR_PRIMARY_DISPLAY];
	} else {
		oplus_adfr_set_display_id(OPLUS_ADFR_SECONDARY_DISPLAY);
		return &g_oplus_adfr_params[OPLUS_ADFR_SECONDARY_DISPLAY];
	}
}

static struct oplus_adfr_params *oplus_adfr_get_params_by_comp(void *mtk_ddp_comp)
{
	struct mtk_ddp_comp *comp = mtk_ddp_comp;

	if (!comp) {
		ADFR_ERR("Invalid mtk_ddp_comp pointer");
		return NULL;
	}

	if (comp->id == DDP_COMPONENT_DSI0) {
		oplus_adfr_set_display_id(OPLUS_ADFR_PRIMARY_DISPLAY);
		return &g_oplus_adfr_params[OPLUS_ADFR_PRIMARY_DISPLAY];
	} else if (comp->id == DDP_COMPONENT_DSI1) {
		oplus_adfr_set_display_id(OPLUS_ADFR_SECONDARY_DISPLAY);
		return &g_oplus_adfr_params[OPLUS_ADFR_SECONDARY_DISPLAY];
	} else {
		oplus_adfr_set_display_id(OPLUS_ADFR_PRIMARY_DISPLAY);
		return NULL;
	}
}

static struct oplus_adfr_params *oplus_adfr_get_params_by_crtc(void *drm_crtc)
{
	struct drm_crtc *temp_crtc = drm_crtc;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(temp_crtc);
	struct mtk_ddp_comp *comp = NULL;

	if (!temp_crtc || !mtk_crtc) {
		ADFR_ERR("find crtc fail\n");
		return NULL;
	}

	comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (!comp) {
		ADFR_ERR("Invalid mtk_ddp_comp pointer");
		return NULL;
	}

	if (comp->id == DDP_COMPONENT_DSI0) {
		oplus_adfr_set_display_id(OPLUS_ADFR_PRIMARY_DISPLAY);
		return &g_oplus_adfr_params[OPLUS_ADFR_PRIMARY_DISPLAY];
	} else if (comp->id == DDP_COMPONENT_DSI1) {
		oplus_adfr_set_display_id(OPLUS_ADFR_SECONDARY_DISPLAY);
		return &g_oplus_adfr_params[OPLUS_ADFR_SECONDARY_DISPLAY];
	} else {
		oplus_adfr_set_display_id(OPLUS_ADFR_PRIMARY_DISPLAY);
		return NULL;
	}
}

bool oplus_adfr_is_supported(void *oplus_adfr_params)
{
	struct oplus_adfr_params *p_oplus_adfr_params = oplus_adfr_params;

	if (!p_oplus_adfr_params) {
		ADFR_ERR("Invalid p_oplus_adfr_params param\n");
		return false;
	}

	/* global config, support adfr panel */
	return (bool)(OPLUS_ADFR_GET_GLOBAL_CONFIG(p_oplus_adfr_params->config));
}

bool oplus_adfr_is_supported_export(void *drm_crtc)
{
	struct drm_crtc *crtc = drm_crtc;
	struct oplus_adfr_params *p_oplus_adfr_params = NULL;

	if (!crtc) {
		ADFR_ERR("find crtc fail\n");
		return false;
	}

	p_oplus_adfr_params = oplus_adfr_get_params_by_crtc(crtc);

	if (!p_oplus_adfr_params) {
		ADFR_ERR("Invalid p_oplus_adfr_params param\n");
		return false;
	}

	return oplus_adfr_is_supported(p_oplus_adfr_params);
}
EXPORT_SYMBOL(oplus_adfr_is_supported_export);

static bool oplus_adfr_idle_mode_is_enabled(void *oplus_adfr_params)
{
	struct oplus_adfr_params *p_oplus_adfr_params = oplus_adfr_params;

	if (!p_oplus_adfr_params) {
		ADFR_ERR("Invalid p_oplus_adfr_params param\n");
		return false;
	}

	if (!oplus_adfr_is_supported(p_oplus_adfr_params)) {
		ADFR_DEBUG("ADFR is not supported, idle mode is also not supported\n");
		return false;
	}

	return (bool)(OPLUS_ADFR_GET_IDLE_MODE_CONFIG(p_oplus_adfr_params->config));
}

static bool oplus_adfr_temperature_detection_is_enabled(void *oplus_adfr_params)
{
	struct oplus_adfr_params *p_oplus_adfr_params = oplus_adfr_params;

	if (!p_oplus_adfr_params) {
		ADFR_ERR("Invalid p_oplus_adfr_params param\n");
		return false;
	}

	if (!oplus_adfr_is_supported(p_oplus_adfr_params)) {
		ADFR_DEBUG("ADFR is not supported, temperature detection is also not supported\n");
		return false;
	}

	return (bool)(OPLUS_ADFR_GET_TEMPERATURE_DETECTION_CONFIG(p_oplus_adfr_params->config));
}

bool oplus_adfr_sa_mode_restore_is_enabled(void *oplus_adfr_params)
{
	struct oplus_adfr_params *p_oplus_adfr_params = oplus_adfr_params;

	if (!p_oplus_adfr_params) {
		ADFR_ERR("Invalid p_oplus_adfr_params param\n");
		return false;
	}

	if (!oplus_adfr_is_supported(p_oplus_adfr_params)) {
		ADFR_DEBUG("ADFR is not supported, sa mode restore is also not supported\n");
		return false;
	}

	return (bool)(OPLUS_ADFR_GET_SA_MODE_RESTORE_CONFIG(p_oplus_adfr_params->config));
}

bool oplus_adfr_dry_run_is_enabled(void *oplus_adfr_params)
{
	struct oplus_adfr_params *p_oplus_adfr_params = oplus_adfr_params;

	if (!p_oplus_adfr_params) {
		ADFR_ERR("Invalid p_oplus_adfr_params param\n");
		return false;
	}

	if (!oplus_adfr_is_supported(p_oplus_adfr_params)) {
		ADFR_DEBUG("ADFR is not supported, dry run is also not supported\n");
		return false;
	}

	return (bool)(OPLUS_ADFR_GET_DRY_RUN_CONFIG(p_oplus_adfr_params->config));
}

bool oplus_adfr_decreasing_step_is_enabled(void *oplus_adfr_params)
{
	struct oplus_adfr_params *p_oplus_adfr_params = oplus_adfr_params;

	if (!p_oplus_adfr_params) {
		ADFR_ERR("Invalid p_oplus_adfr_params param\n");
		return false;
	}

	if (!oplus_adfr_is_supported(p_oplus_adfr_params)) {
		ADFR_DEBUG("ADFR is not supported, decreasing_step is also not supported\n");
		return false;
	}

	return (bool)(OPLUS_ADFR_GET_DECREASING_STEP_CONFIG(p_oplus_adfr_params->config));
}

/* Interface function for new driver architecture */
int oplus_dsi_display_adfr_init(void *dev_node, void *ctx)
{
	int rc = 0;
	struct dsi_panel_lcm *ctx_dev = ctx;

	if (!ctx_dev) {
		ADFR_ERR("Invalid ctx param\n");
		return -EINVAL;
	}
	rc = oplus_adfr_init(dev_node, ctx_dev->is_primary);

	return rc;
}

int oplus_dsi_display_adfr_deinit(void *ctx_dev)
{
	int rc = 0;
	int test_te_irq = -1;
	struct dsi_panel_lcm *ctx = ctx_dev;
	struct oplus_adfr_params *p_oplus_adfr_params = NULL;
	struct mtk_dsi *dsi = NULL;
	struct mipi_dsi_device *dev_dsi = NULL;

	if (!ctx) {
		ADFR_WARN("ctx is null\n");
		return -EINVAL;
	}
	dev_dsi = to_mipi_dsi_device(ctx->dev);
	dsi = container_of(dev_dsi->host, struct mtk_dsi, host);
	p_oplus_adfr_params = oplus_adfr_get_params_by_num(ctx->is_primary);
	if (!p_oplus_adfr_params) {
		ADFR_WARN("p_oplus_adfr_params is null\n");
		return -EINVAL;
	}
	if (IS_ERR(p_oplus_adfr_params->test_te.gpio)) {
		ADFR_WARN("p_oplus_adfr_params test_te gpio is null in deinit\n");
		return -EINVAL;
	}
	test_te_irq = gpiod_to_irq(p_oplus_adfr_params->test_te.gpio);
	if (test_te_irq < 0) {
		ADFR_WARN("failed to get test_te gpio irq in deinit\n");
		return -EINVAL;
	}
	if (!irqd_irq_disabled(irq_get_irq_data(test_te_irq))) {
		disable_irq(test_te_irq);
		ADFR_INFO("disable test te irq in deinit\n");
	}
	if (irqd_is_activated(irq_get_irq_data(test_te_irq))) {
		devm_free_irq(dsi->dev, test_te_irq, &dsi->ddp_comp);
	}
	gpiod_put(p_oplus_adfr_params->test_te.gpio);

	return rc;
}

/* Interface function for old driver architecture : get config value from panel dtsi */
int oplus_adfr_init(void *dev_node, bool is_primary)
{
	int rc = 0;
	unsigned int config = 0;
	struct device_node *node = dev_node;
	struct oplus_adfr_params *p_oplus_adfr_params = NULL;
	int dsi_panel_adfr_test_te_gpio = -1;

	if (!dev_node) {
		ADFR_ERR("Invalid device node param\n");
		return -EINVAL;
	}
	p_oplus_adfr_params = oplus_adfr_get_params_by_num(is_primary);
	if (!p_oplus_adfr_params) {
		ADFR_ERR("Invalid p_oplus_adfr_params param\n");
		return -EINVAL;
	}

	ADFR_DEBUG("start\n");

	ADFR_INFO("init %s display adfr params\n", is_primary?"primary":"secondary");

	OPLUS_ADFR_TRACE_BEGIN("oplus_adfr_init");

	rc = of_property_read_u32(node, "oplus-adfr-config", &config);
	if (rc == 0) {
		p_oplus_adfr_params->config = config;
		ADFR_INFO("config=%d, adfrconfig=%d\n", config, p_oplus_adfr_params->config);
	} else {
		p_oplus_adfr_params->config = 0;
		ADFR_INFO("adfrconfig=%d\n", p_oplus_adfr_params->config);
	}

/*
	if (oplus_is_factory_boot()) {
		p_oplus_adfr_params->config = 0;
		ADFR_INFO("disable adfr in factory mode\n");
	}
*/

	if (oplus_adfr_is_supported(p_oplus_adfr_params)) {
		/* oplus,adfr-idle-off-min-fps */
		if (oplus_adfr_idle_mode_is_enabled(p_oplus_adfr_params)) {
			/* the minimum fps setting which can be set in idle off if idle mode is enabled */
			rc = of_property_read_u32(node, "oplus,adfr-idle-off-min-fps", &config);
			if (rc) {
				ADFR_ERR("failed to read oplus,adfr-idle-off-min-fps, rc=%d\n", rc);
				/* set default value to 0 */
				p_oplus_adfr_params->oplus_adfr_idle_off_min_fps = 0;
			} else {
				p_oplus_adfr_params->oplus_adfr_idle_off_min_fps = config;
			}
			ADFR_INFO("oplus_adfr_idle_off_min_fps:%u\n", p_oplus_adfr_params->oplus_adfr_idle_off_min_fps);
		}

		/* oplus,adfr-test-te-gpio */
		dsi_panel_adfr_test_te_gpio = of_get_named_gpio(node, "oplus-adfr-test-te-gpios", 0);
		if (!gpio_is_valid(dsi_panel_adfr_test_te_gpio)) {
			dsi_panel_adfr_test_te_gpio = -1;
			ADFR_INFO("[%s] oplus-adfr-test-te-gpios is error\n", is_primary?"primary":"secondary");
		} else {
			p_oplus_adfr_params->test_te.gpio = gpio_to_desc(dsi_panel_adfr_test_te_gpio);
			if (IS_ERR(p_oplus_adfr_params->test_te.gpio)) {
				ADFR_INFO("[%s] oplus,adfr-test-te-gpio is not set\n", is_primary?"primary":"secondary");
			} else {
				if (gpio_request(dsi_panel_adfr_test_te_gpio, NULL)) {
					dsi_panel_adfr_test_te_gpio = -1;
					ADFR_INFO("[%s] oplus-adfr-test-te-gpios request error\n", is_primary?"primary":"secondary");
				} else {
					ADFR_INFO("[%s] oplus,adfr-test-te-gpio is %d\n", is_primary?"primary":"secondary", p_oplus_adfr_params->test_te.gpio);
					if (gpiod_direction_input(p_oplus_adfr_params->test_te.gpio)) {
						ADFR_INFO("[%s] oplus,adfr-test-te-gpio set input error\n", is_primary?"primary":"secondary");
					} else {
						/* test te timer init */
						hrtimer_init(&p_oplus_adfr_params->test_te.timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
						p_oplus_adfr_params->test_te.timer.function = oplus_adfr_test_te_timer_handler;
						p_oplus_adfr_params->test_te.config = OPLUS_ADFR_TEST_TE_DISABLE;
					}
				}
			}
		}
	}

	ADFR_INFO("oplus_adfr_config:0x%x\n", p_oplus_adfr_params->config);
	OPLUS_ADFR_TRACE_INT("oplus_adfr_config", p_oplus_adfr_params->config);

	OPLUS_ADFR_TRACE_END("oplus_adfr_init");

	ADFR_DEBUG("end\n");

	return rc;
}
EXPORT_SYMBOL(oplus_adfr_init);

/* test te timer */
enum hrtimer_restart oplus_adfr_test_te_timer_handler(struct hrtimer *timer)
{
	struct oplus_adfr_test_te_params *p_oplus_adfr_test_te_params = from_timer(p_oplus_adfr_test_te_params, timer, timer);
	struct oplus_adfr_params *p_oplus_adfr_params = NULL;

	if (!p_oplus_adfr_test_te_params) {
		ADFR_ERR("Invalid p_oplus_adfr_test_te_params param\n");
		goto end;
	}

	p_oplus_adfr_params = container_of(p_oplus_adfr_test_te_params, struct oplus_adfr_params, test_te);
	if (!p_oplus_adfr_params) {
		ADFR_ERR("Invalid p_oplus_adfr_params param\n");
		goto end;
	}

	if (!oplus_adfr_is_supported(p_oplus_adfr_params)) {
		ADFR_ERR("ADFR is not supported\n");
		goto end;
	}

	if (IS_ERR(p_oplus_adfr_params->test_te.gpio)) {
		ADFR_ERR("test te gpio is Invalid, no need to handle test te irq\n");
		goto end;
	}

	ADFR_DEBUG("start\n");

	OPLUS_ADFR_TRACE_BEGIN("oplus_adfr_test_te_timer_handler");

	/* speed up refrsh rate updates if enter idle mode */
	p_oplus_adfr_params->test_te.refresh_rate = p_oplus_adfr_params->sa_min_fps;
	if (p_oplus_adfr_params->test_te.config == OPLUS_ADFR_TEST_TE_ENABLE_WITCH_LOG) {
		ADFR_INFO("enter idle mode, update refresh_rate to %u\n", p_oplus_adfr_params->test_te.refresh_rate);
	}
	OPLUS_ADFR_TRACE_INT("oplus_adfr_test_te_refresh_rate", p_oplus_adfr_params->test_te.refresh_rate);

	OPLUS_ADFR_TRACE_END("oplus_adfr_test_te_timer_handler");

end:
	ADFR_DEBUG("end\n");

	return HRTIMER_NORESTART;
}

int oplus_adfr_test_te_high_gear(void *drm_crtc) {
	struct drm_crtc *crtc = drm_crtc;
	struct oplus_adfr_params *p_oplus_adfr_params = NULL;
	int refresh_rate = 120;
	int high_gear = 55;

	if (!crtc) {
		ADFR_ERR("Invalid crtc params\n");
		return high_gear;
	}

	p_oplus_adfr_params = oplus_adfr_get_params_by_crtc(crtc);
	if (!p_oplus_adfr_params) {
		ADFR_ERR("Invalid p_oplus_adfr_params param\n");
		return high_gear;
	}

	refresh_rate = drm_mode_vrefresh(&crtc->state->mode);

	switch (refresh_rate) {
	case 144:
		high_gear = 90;
		break;
	case 120:
	case 60:
	case 90:
		if (!oplus_adfr_decreasing_step_is_enabled(p_oplus_adfr_params)) {
			high_gear = 55;
		} else {
			high_gear = 52;
		}
		break;
	default:
		high_gear = 55;
		break;
	}

	return high_gear;
}

int oplus_adfr_test_te_low_gear(void *drm_crtc) {
	struct drm_crtc *crtc = drm_crtc;
	struct oplus_adfr_params *p_oplus_adfr_params = NULL;
	int refresh_rate = 120;
	int low_gear = 16;

	if (!crtc) {
		ADFR_ERR("Invalid crtc params\n");
		return low_gear;
	}

	p_oplus_adfr_params = oplus_adfr_get_params_by_crtc(crtc);
	if (!p_oplus_adfr_params) {
		ADFR_ERR("Invalid p_oplus_adfr_params param\n");
		return low_gear;
	}

	refresh_rate = drm_mode_vrefresh(&crtc->state->mode);

	switch (refresh_rate) {
	case 144:
		if (!oplus_adfr_decreasing_step_is_enabled(p_oplus_adfr_params)) {
			low_gear = 0;
		} else {
			low_gear = 12;
		}
		break;
	case 120:
	case 60:
	case 90:
		if (!oplus_adfr_decreasing_step_is_enabled(p_oplus_adfr_params)) {
			low_gear = 16;
		} else {
			low_gear = 10;
		}
		break;
	default:
		low_gear = 16;
		break;
	}

	return low_gear;
}

int oplus_adfr_test_te_middle_value(void *drm_crtc) {
	struct drm_crtc *crtc = drm_crtc;
	struct oplus_adfr_params *p_oplus_adfr_params = NULL;
	int refresh_rate = 120;
	int middle_value = 30;

	if (!crtc) {
		ADFR_ERR("Invalid crtc params\n");
		return middle_value;
	}

	p_oplus_adfr_params = oplus_adfr_get_params_by_crtc(crtc);
	if (!p_oplus_adfr_params) {
		ADFR_ERR("Invalid p_oplus_adfr_params param\n");
		return middle_value;
	}

	refresh_rate = drm_mode_vrefresh(&crtc->state->mode);

	switch (refresh_rate) {
	case 144:
		if (!oplus_adfr_decreasing_step_is_enabled(p_oplus_adfr_params)) {
			middle_value = 72;
		} else {
			middle_value = 36;
		}
		break;
	case 120:
	case 60:
	case 90:
		middle_value = 30;
		break;
	default:
		middle_value = 30;
		break;
	}

	return middle_value;
}

/* test te detectiton */
static irqreturn_t oplus_adfr_test_te_irq_handler(int irq, void *data)
{
	unsigned int temp_refresh_rate = 0;
	int h_skew = STANDARD_ADFR;
	int refresh_rate = 120;
	u64 current_timestamp = 0;
	struct mtk_ddp_comp *comp = (struct mtk_ddp_comp *)data;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	struct drm_crtc *crtc = NULL;
	struct oplus_adfr_params *p_oplus_adfr_params = NULL;

	if (!comp) {
		ADFR_ERR("Invalid mtk_ddp_comp param\n");
		return IRQ_HANDLED;
	}

	mtk_crtc = comp->mtk_crtc;
	if (!mtk_crtc) {
		ADFR_ERR("Invalid mtk_drm_crtc param\n");
		return IRQ_HANDLED;
	}

	crtc = &(mtk_crtc->base);
	if (!crtc) {
		ADFR_ERR("Invalid crtc params\n");
		return IRQ_HANDLED;
	}

	if (!crtc->state || !&(crtc->state->mode)) {
		ADFR_ERR("Invalid crtc params\n");
		return IRQ_HANDLED;
	}

	h_skew = crtc->state->mode.hskew;
	refresh_rate = drm_mode_vrefresh(&crtc->state->mode);

	p_oplus_adfr_params = oplus_adfr_get_params_by_comp(comp);
	if (!p_oplus_adfr_params) {
		ADFR_ERR("Invalid p_oplus_adfr_params param\n");
		return IRQ_HANDLED;
	}

	if (!oplus_adfr_is_supported(p_oplus_adfr_params)) {
		ADFR_ERR("adfr is not supported\n");
		return IRQ_HANDLED;
	}

	if (IS_ERR(p_oplus_adfr_params->test_te.gpio)) {
		ADFR_ERR("test te gpio is Invalid, no need to handle test te irq\n");
		return IRQ_HANDLED;
	}

	ADFR_DEBUG("start\n");

	OPLUS_ADFR_TRACE_BEGIN("oplus_adfr_test_te_irq_handler");

	if (p_oplus_adfr_params->test_te.config != OPLUS_ADFR_TEST_TE_DISABLE) {
		/* check the test te interval to calculate refresh rate of ddic */
		current_timestamp = (u64)ktime_to_ms(ktime_get());
		temp_refresh_rate = 1000 / (current_timestamp - p_oplus_adfr_params->test_te.last_timestamp);
		if (temp_refresh_rate == 0) {
			temp_refresh_rate = 1;
		}

		/* filtering algorithm */
		if ((h_skew == STANDARD_ADFR) || (h_skew == STANDARD_MFR)) {
			if (!oplus_adfr_decreasing_step_is_enabled(p_oplus_adfr_params)) {
				/* range:
				 * 144hz: >90 is 144
				 * 120hz: >55 is 120, sw_fps==60 is 60
				 * 90hz: >55 is 90
				 * 60hz: >55 is 60 */
				if (temp_refresh_rate > oplus_adfr_test_te_high_gear(crtc)) {
					p_oplus_adfr_params->test_te.high_refresh_rate_count++;
					p_oplus_adfr_params->test_te.middle_refresh_rate_count = 0;
					/* update refresh rate if four continous temp_refresh_rate are greater than high gear */
					if (p_oplus_adfr_params->test_te.high_refresh_rate_count == 4) {
						p_oplus_adfr_params->test_te.refresh_rate = refresh_rate;
						if ((refresh_rate == 120) && (p_oplus_adfr_params->sw_fps == 60) && (p_oplus_adfr_params->sa_min_fps != 120)) {
							/* show the ddic refresh rate */
							p_oplus_adfr_params->test_te.refresh_rate = 60;
						}
						p_oplus_adfr_params->test_te.high_refresh_rate_count--;
					}
				/* range:
				 * 144hz: 0~90 is 72
				 * 120hz: 17~55 is 30
				 * 90hz: 17~55 is 30
				 * 60hz: 17~55 is 30 */
				} else if (temp_refresh_rate > oplus_adfr_test_te_low_gear(crtc) && temp_refresh_rate <= oplus_adfr_test_te_high_gear(crtc)) {
					p_oplus_adfr_params->test_te.high_refresh_rate_count = 0;
					/* update refresh rate if one continous temp_refresh_rate are greater than low gear and less than or equal to high gear */
					p_oplus_adfr_params->test_te.refresh_rate = oplus_adfr_test_te_middle_value(crtc);
				/* range:
				 * 144hz: X
				 * 120hz: <=16 is temp_refresh_rate
				 * 90hz: <=16 is temp_refresh_rate
				 * 60hz: <=16 is temp_refresh_rate */
				} else {
						p_oplus_adfr_params->test_te.high_refresh_rate_count = 0;
						/* if current refresh rate of ddic is less than or equal to low gear, use it directly */
						p_oplus_adfr_params->test_te.refresh_rate = temp_refresh_rate;
				}
			} else {
				/* range:
				 * 144hz: >90 is 144
				 * 120hz: >52 is 120, sw_fps==60 is 60
				 * 90hz: >52 is 90
				 * 60hz: >52 is 60 */
				if (temp_refresh_rate > oplus_adfr_test_te_high_gear(crtc)) {
					p_oplus_adfr_params->test_te.high_refresh_rate_count++;
					p_oplus_adfr_params->test_te.middle_refresh_rate_count = 0;
					/* update refresh rate if four continous temp_refresh_rate are greater than high gear */
					if (p_oplus_adfr_params->test_te.high_refresh_rate_count == 4) {
						p_oplus_adfr_params->test_te.refresh_rate = refresh_rate;
						if ((refresh_rate == 120) && (p_oplus_adfr_params->sw_fps == 60) && (p_oplus_adfr_params->sa_min_fps != 120)) {
							/* show the ddic refresh rate */
							p_oplus_adfr_params->test_te.refresh_rate = 60;
						}
						p_oplus_adfr_params->test_te.high_refresh_rate_count--;
					}
				/* range:
				 * 144hz: 13~90 is 36
				 * 120hz: 11~52 is 30
				 * 90hz: 11~52 is 30
				 * 60hz: 11~52 is 30 */
				} else if (temp_refresh_rate > oplus_adfr_test_te_low_gear(crtc) && temp_refresh_rate <= oplus_adfr_test_te_high_gear(crtc)) {
					p_oplus_adfr_params->test_te.middle_refresh_rate_count++;
					if (p_oplus_adfr_params->test_te.high_refresh_rate_count > 0) {
						p_oplus_adfr_params->test_te.high_refresh_rate_count = 0;
						/* update refresh rate if one continous temp_refresh_rate are greater than low gear and less than or equal to high gear
						 * and old temp_refresh_rate are greater than high gear*/
						p_oplus_adfr_params->test_te.refresh_rate = oplus_adfr_test_te_middle_value(crtc);
					} else if (p_oplus_adfr_params->test_te.middle_refresh_rate_count == 2) {
						/* update refresh rate if two continous temp_refresh_rate are greater than low gear and less than or equal to high gear
						 * and old temp_refresh_rate are less than or equal to high gear*/
						p_oplus_adfr_params->test_te.refresh_rate = oplus_adfr_test_te_middle_value(crtc);
						p_oplus_adfr_params->test_te.middle_refresh_rate_count--;
					}
				/* range:
				 * 144hz: <=12 is min fps
				 * 120hz: <=10 is min fps
				 * 90hz: <=10 is min fps
				 * 60hz: <=10 is min fps */
				} else {
					p_oplus_adfr_params->test_te.high_refresh_rate_count = 0;
					p_oplus_adfr_params->test_te.middle_refresh_rate_count = 0;
					/* if current refresh rate of ddic is less than or equal to low gear, set as min fps */
					p_oplus_adfr_params->test_te.refresh_rate = p_oplus_adfr_params->sa_min_fps;
				}
			}
			if (p_oplus_adfr_params->idle_mode == OPLUS_ADFR_IDLE_ON) {
				/* use sa min fps directly when enter idle mode */
				p_oplus_adfr_params->test_te.refresh_rate = p_oplus_adfr_params->sa_min_fps;
			}
		} else {
			p_oplus_adfr_params->test_te.high_refresh_rate_count = 0;
			/* fix refresh rate */
			p_oplus_adfr_params->test_te.refresh_rate = refresh_rate;
		}

		if (p_oplus_adfr_params->test_te.refresh_rate > refresh_rate) {
			p_oplus_adfr_params->test_te.refresh_rate = refresh_rate;
		}

		ADFR_DEBUG("oplus_adfr_test_te_high_refresh_rate_count:%u\n", p_oplus_adfr_params->test_te.high_refresh_rate_count);
		OPLUS_ADFR_TRACE_INT("oplus_adfr_test_te_high_refresh_rate_count", p_oplus_adfr_params->test_te.high_refresh_rate_count);
		OPLUS_ADFR_TRACE_INT("oplus_adfr_temp_refresh_rate", temp_refresh_rate);
		OPLUS_ADFR_TRACE_INT("oplus_adfr_test_te_refresh_rate", p_oplus_adfr_params->test_te.refresh_rate);

		if (p_oplus_adfr_params->test_te.config == OPLUS_ADFR_TEST_TE_ENABLE_WITCH_LOG) {
			/* print key information on every test te irq handler */
			ADFR_INFO("last_timestamp:%lu,current_timestamp:%lu,temp_refresh_rate:%u,refresh_rate:%u\n",
						p_oplus_adfr_params->test_te.last_timestamp, current_timestamp,
						temp_refresh_rate, p_oplus_adfr_params->test_te.refresh_rate);

			if ((h_skew == STANDARD_ADFR) || (h_skew == STANDARD_MFR)) {
				ADFR_INFO("fps:%u,h_skew:%u,auto_mode:%u,sa_min_fps:%u,sw_fps:%u,fakeframe:%u,idle_mode:%u\n",
							refresh_rate,
							h_skew,
							p_oplus_adfr_params->auto_mode,
							p_oplus_adfr_params->sa_min_fps,
							p_oplus_adfr_params->sw_fps,
							p_oplus_adfr_params->idle_mode);
			} else {
				ADFR_INFO("fps:%u,h_skew:%u,osync_min_fps:%u\n",
							refresh_rate,
							h_skew,
							p_oplus_adfr_params->sa_min_fps);
			}
		}

		p_oplus_adfr_params->test_te.last_timestamp = current_timestamp;
	}

	OPLUS_ADFR_TRACE_END("oplus_adfr_test_te_irq_handler");

	ADFR_DEBUG("end\n");

	return IRQ_HANDLED;
}

int oplus_adfr_register_test_te_irq(void *mtk_ddp_comp, void *platform_device)
{
	int rc = 0;
	unsigned int test_te_irq = 0;
	struct mtk_ddp_comp *comp = mtk_ddp_comp;
	struct platform_device *pdev = platform_device;
	struct device *dev = NULL;
	struct oplus_adfr_params *p_oplus_adfr_params = NULL;

	if (!comp) {
		ADFR_ERR("Invalid mtk_ddp_comp param\n");
		return -EINVAL;
	}

	p_oplus_adfr_params = oplus_adfr_get_params_by_comp(comp);
	if (!p_oplus_adfr_params) {
		ADFR_ERR("Invalid p_oplus_adfr_params param\n");
		return -EINVAL;
	}

	if (!oplus_adfr_is_supported(p_oplus_adfr_params)) {
		ADFR_DEBUG("adfr is not supported\n");
		return 0;
	}

	if (!pdev) {
		ADFR_ERR("Invalid pdev param\n");
		return -EINVAL;
	}

	dev = &pdev->dev;
	if (!dev) {
		ADFR_ERR("Invalid dev param\n");
		return -EINVAL;
	}

	if (IS_ERR(p_oplus_adfr_params->test_te.gpio)) {
		ADFR_ERR("test te gpio is Invalid, no need to handle test te irq\n");
		return -EINVAL;
	}

	test_te_irq = gpiod_to_irq(p_oplus_adfr_params->test_te.gpio);
	if (test_te_irq < 0) {
		ADFR_ERR("failed to get test_te gpio irq\n");
		return -EINVAL;
	}

	ADFR_DEBUG("start\n");

	OPLUS_ADFR_TRACE_BEGIN("oplus_adfr_register_test_te_irq");

	/* avoid deferred spurious irqs with disable_irq() */
	/* irq_set_status_flags(test_te_irq, IRQ_DISABLE_UNLAZY); */

	/* detect test te rising edge */
	if (comp->id == DDP_COMPONENT_DSI0) {
		rc = devm_request_irq(dev, test_te_irq, oplus_adfr_test_te_irq_handler,
								IRQF_TRIGGER_RISING | IRQF_ONESHOT, "TEST_TE_GPIO_0", comp);
	} else {
		rc = devm_request_irq(dev, test_te_irq, oplus_adfr_test_te_irq_handler,
								IRQF_TRIGGER_RISING | IRQF_ONESHOT, "TEST_TE_GPIO_1", comp);
	}

	if (rc) {
		ADFR_ERR("test te request_irq failed rc:%d\n", rc);
		/* irq_clear_status_flags(test_te_irq, IRQ_DISABLE_UNLAZY); */
	} else {
		ADFR_INFO("register test te irq successfully\n");
	}

	OPLUS_ADFR_TRACE_END("oplus_adfr_register_test_te_irq");

	ADFR_DEBUG("end\n");

	return rc;
}

/* --------------- auto mode --------------- */
void oplus_adfr_handle_auto_mode(void *drm_crtc, int prop_id, unsigned int propval)
{
	struct drm_crtc *crtc = drm_crtc;
	struct oplus_adfr_params *p_oplus_adfr_params = NULL;
	int handled = 1;

	if (!crtc) {
		ADFR_ERR("Invalid drm_crtc param\n");
		return;
	}

	p_oplus_adfr_params = oplus_adfr_get_params_by_crtc(crtc);
	if (!p_oplus_adfr_params) {
		ADFR_ERR("Invalid p_oplus_adfr_params param\n");
		return;
	}

	if (!oplus_adfr_is_supported(p_oplus_adfr_params)) {
		ADFR_DEBUG("ADFR is not supported\n");
		return;
	}

	ADFR_DEBUG("start\n");

	OPLUS_ADFR_TRACE_BEGIN("oplus_adfr_handle_auto_mode");

	switch (prop_id) {
	case CRTC_PROP_AUTO_MODE:
		/* Add for auto on cmd filter */
		if (propval == OPLUS_ADFR_AUTO_ON) {
			ADFR_WARN("auto off and auto on cmd are sent on the same frame, filter it\n");
			OPLUS_ADFR_TRACE_END("oplus_adfr_handle_auto_mode");
			handled = 1;
			return;
		}
		if (propval != p_oplus_adfr_params->auto_mode) {
			p_oplus_adfr_params->auto_mode_updated = true;
			/* when auto mode changes, write the corresponding min fps again */
			p_oplus_adfr_params->sa_min_fps_updated = true;
			p_oplus_adfr_params->auto_mode = propval;
			handled += 2;
			ADFR_WARN("update auto mode %u\n", propval);
		}
		break;

	case CRTC_PROP_AUTO_MIN_FPS:
		if (propval != p_oplus_adfr_params->sa_min_fps) {
			p_oplus_adfr_params->sa_min_fps_updated = true;
			p_oplus_adfr_params->sa_min_fps = propval;
			handled += 8;
			ADFR_WARN("update minfps %u\n", propval);
		}
		break;

	default:
		break;
	}

	OPLUS_ADFR_TRACE_INT("auto_handled|%d", handled);
	OPLUS_ADFR_TRACE_INT("p_oplus_adfr_params->auto_mode|%d", p_oplus_adfr_params->auto_mode);
	OPLUS_ADFR_TRACE_INT("p_oplus_adfr_params->sa_min_fps|%d", p_oplus_adfr_params->sa_min_fps);
	OPLUS_ADFR_TRACE_END("oplus_adfr_handle_auto_mode");

	if (handled == 1) {
		return;
	} else {
		/* latest setting, but if power on/off or timing switch, the mode and min fps are not right */
		ADFR_INFO("auto mode %d[%d], min fps %d[%d], handled %d\n",
			p_oplus_adfr_params->auto_mode, p_oplus_adfr_params->auto_mode_updated,
			p_oplus_adfr_params->sa_min_fps, p_oplus_adfr_params->sa_min_fps_updated, handled);
	}

	ADFR_DEBUG("end\n");

	return;
}

int oplus_adfr_send_auto_mode_dcs(struct drm_crtc *crtc, bool enable)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct cmdq_pkt *cmdq_handle;
	struct mtk_crtc_state *crtc_state;
	struct mtk_ddp_comp *comp = mtk_ddp_comp_request_output(mtk_crtc);
	struct mtk_cmdq_cb_data *cb_data;
	bool is_frame_mode;

	/* SDC's auto, fakeframe and minfps are available only after power on */
	crtc_state = to_mtk_crtc_state(crtc->state);
	if (!mtk_crtc->enabled || crtc_state->prop_val[CRTC_PROP_DOZE_ACTIVE]) {
		ADFR_WARN("ignore %s when power is off", __func__);
		return -1;
	}
	if (!(comp && comp->funcs && comp->funcs->io_cmd))
		ADFR_ERR("Invalid mtk_ddp_comp pointer");
		return -1;

	ADFR_DEBUG("start\n");

	if (enable) {
		ADFR_INFO("ctrl:%d auto on\n");
		OPLUS_ADFR_TRACE_INT("p_oplus_adfr_params->auto_mode_cmd|%d", OPLUS_ADFR_AUTO_ON);
	} else {
		ADFR_INFO("ctrl:%d auto off\n");
		OPLUS_ADFR_TRACE_INT("p_oplus_adfr_params->auto_mode_cmd|%d", OPLUS_ADFR_AUTO_OFF);
	}

	is_frame_mode = mtk_crtc_is_frame_trigger_mode(&mtk_crtc->base);
	if (is_frame_mode) {
		mtk_drm_idlemgr_kick(__func__, crtc, 0);
		mtk_crtc_pkt_create(&cmdq_handle, &mtk_crtc->base,
						mtk_crtc->gce_obj.client[CLIENT_CFG]);

		if (mtk_crtc_with_sub_path(crtc, mtk_crtc->ddp_mode))
			mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle,
				DDP_SECOND_PATH, 0);
		else
			mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle,
				DDP_FIRST_PATH, 0);

		cmdq_pkt_clear_event(cmdq_handle,
				 mtk_crtc->gce_obj.event[EVENT_STREAM_BLOCK]);
		cmdq_pkt_wfe(cmdq_handle,
		 				 mtk_crtc->gce_obj.event[EVENT_CABC_EOF]);

		if (comp && comp->funcs && comp->funcs->io_cmd)
			comp->funcs->io_cmd(comp, cmdq_handle, SET_AUTO_MODE, &enable);

		cmdq_pkt_set_event(cmdq_handle,
					mtk_crtc->gce_obj.event[EVENT_CABC_EOF]);
		cmdq_pkt_set_event(cmdq_handle,
					mtk_crtc->gce_obj.event[EVENT_STREAM_BLOCK]);

		cb_data = kmalloc(sizeof(*cb_data), GFP_KERNEL);
		if (cb_data) {
			cb_data->cmdq_handle = cmdq_handle;
			/* indicate auto on/off cmd */
			cb_data->misc = 4;
			cmdq_pkt_flush_threaded(cmdq_handle, lcm_cmd_cmdq_cb, cb_data);
		} else {
			cmdq_pkt_flush(cmdq_handle);
			cmdq_pkt_destroy(cmdq_handle);
			return -1;
		}
	}

	ADFR_DEBUG("end\n");

	return 0;
}

static int oplus_adfr_auto_mode_enable(struct drm_crtc *crtc, bool enable)
{
	struct oplus_adfr_params *p_oplus_adfr_params = NULL;
	/* struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc); */
	int rc = 0;

	if (!crtc) {
		ADFR_ERR("Invalid drm_crtc param\n");
		return -1;
	}

	p_oplus_adfr_params = oplus_adfr_get_params_by_crtc(crtc);
	if (!p_oplus_adfr_params) {
		ADFR_ERR("Invalid p_oplus_adfr_params param\n");
		return -1;
	}

	if (!oplus_adfr_is_supported(p_oplus_adfr_params)) {
		ADFR_ERR("ADFR is not supported\n");
		return -1;
	}
/*
	if (!mtk_crtc->panel_ext) {
		return -1;
	} else {
		ADFR_INFO("oplus_autoon_cfg:%d,oplus_autooff_cfg:%d\n",
			mtk_crtc->panel_ext->params->oplus_autoon_cfg, mtk_crtc->panel_ext->params->oplus_autooff_cfg);
		if (p_oplus_adfr_params->auto_mode == OPLUS_ADFR_AUTO_OFF) {
			if (!(mtk_crtc->panel_ext->params->oplus_autooff_cfg & 0x00000001))
				return -1;
		} else {
			if (!(mtk_crtc->panel_ext->params->oplus_autoon_cfg & 0x00000001))
				return -1;
		}
	}
*/
	ADFR_DEBUG("start\n");

	OPLUS_ADFR_TRACE_BEGIN("dsi_display_auto_mode_enable");
	/* send the commands to enable/disable auto mode */
	rc = oplus_adfr_send_auto_mode_dcs(crtc, enable);
	if (rc) {
		ADFR_ERR("fail auto ON cmds rc:%d\n", rc);
		goto exit;
	}

exit:
	OPLUS_ADFR_TRACE_END("dsi_display_auto_mode_enable");
	ADFR_INFO("KVRR auto mode=%d,rc=%d\n", enable, rc);

	ADFR_DEBUG("end\n");
	return rc;
}

int oplus_adfr_send_minfps_dcs(struct drm_crtc *crtc, int automode, u32 extend_frame)
{
	struct oplus_adfr_params *p_oplus_adfr_params = NULL;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct cmdq_pkt *cmdq_handle;
	struct mtk_ddp_comp *comp = mtk_ddp_comp_request_output(mtk_crtc);
	struct mtk_cmdq_cb_data *cb_data;
	struct oplus_minfps minfps;
	bool is_frame_mode;

	if (!crtc) {
		ADFR_ERR("Invalid drm_crtc param\n");
		return -1;
	}

	p_oplus_adfr_params = oplus_adfr_get_params_by_crtc(crtc);
	if (!p_oplus_adfr_params) {
		ADFR_ERR("Invalid p_oplus_adfr_params param\n");
		return -1;
	}

	if (!oplus_adfr_is_supported(p_oplus_adfr_params)) {
		ADFR_ERR("ADFR is not supported\n");
		return -1;
	}

	if (!(comp && comp->funcs && comp->funcs->io_cmd)) {
		ADFR_ERR("Invalid mtk_ddp_comp param\n");
		return -1;
	}

	ADFR_DEBUG("start\n");

	minfps.minfps_flag = automode;
	minfps.extend_frame = extend_frame;

	is_frame_mode = mtk_crtc_is_frame_trigger_mode(&mtk_crtc->base);
	if (is_frame_mode) {
		mtk_drm_idlemgr_kick(__func__, crtc, 0);
		mtk_crtc_pkt_create(&cmdq_handle, &mtk_crtc->base,
						mtk_crtc->gce_obj.client[CLIENT_CFG]);

		if (mtk_crtc_with_sub_path(crtc, mtk_crtc->ddp_mode))
			mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle,
				DDP_SECOND_PATH, 0);
		else
			mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle,
				DDP_FIRST_PATH, 0);

		cmdq_pkt_clear_event(cmdq_handle,
				 	mtk_crtc->gce_obj.event[EVENT_STREAM_BLOCK]);
		cmdq_pkt_wfe(cmdq_handle,
		 			mtk_crtc->gce_obj.event[EVENT_CABC_EOF]);

		/*
		if ((flag_silky_panel & BL_SETTING_DELAY_60HZ)
				&& mtk_crtc->panel_ext->params->dyn_fps.vact_timing_fps == 60
				&& p_oplus_adfr_params->idle_mode != OPLUS_ADFR_IDLE_ON) {
			cmdq_pkt_sleep(cmdq_handle, CMDQ_US_TO_TICK(2200), mtk_get_gpr(comp, cmdq_handle));
			DDPMSG("%s warning: cmdq_pkt_sleep 3ms %d\n", __func__);
		}
		*/

		if (comp && comp->funcs && comp->funcs->io_cmd)
			comp->funcs->io_cmd(comp, cmdq_handle, SET_MINFPS, &minfps);

		cmdq_pkt_set_event(cmdq_handle,
					mtk_crtc->gce_obj.event[EVENT_CABC_EOF]);
		cmdq_pkt_set_event(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_STREAM_BLOCK]);

		cb_data = kmalloc(sizeof(*cb_data), GFP_KERNEL);
		if (cb_data) {
			cb_data->cmdq_handle = cmdq_handle;
			/* indicate minfps cmd */
			cb_data->misc = 5;
			cmdq_pkt_flush_threaded(cmdq_handle, lcm_cmd_cmdq_cb, cb_data);
		} else {
			cmdq_pkt_flush(cmdq_handle);
			cmdq_pkt_destroy(cmdq_handle);
			return -1;
		}

		oplus_adfr_send_min_fps_event(extend_frame);
	}

	ADFR_DEBUG("end\n");

	return 0;
}

static int oplus_adfr_auto_minfps_check(struct drm_crtc *crtc, u32 extend_frame)
{
	int h_skew = crtc->state->mode.hskew;
	int refresh_rate = drm_mode_vrefresh(&crtc->state->mode);
	struct oplus_adfr_params *p_oplus_adfr_params = NULL;

	if (!crtc) {
		ADFR_ERR("Invalid drm_crtc param\n");
		return extend_frame;
	}

	p_oplus_adfr_params = oplus_adfr_get_params_by_crtc(crtc);
	if (!p_oplus_adfr_params) {
		ADFR_ERR("Invalid p_oplus_adfr_params param\n");
		return extend_frame;
	}

	if (!oplus_adfr_is_supported(p_oplus_adfr_params)) {
		ADFR_ERR("ADFR is not supported\n");
		return extend_frame;
	}

	ADFR_DEBUG("start\n");

	if (h_skew == STANDARD_ADFR) {
		if (p_oplus_adfr_params->auto_mode == OPLUS_ADFR_AUTO_OFF) {
			if (refresh_rate == 120) {
				if ((extend_frame > OPLUS_ADFR_MIN_FPS_120HZ) || (extend_frame < OPLUS_ADFR_MIN_FPS_1HZ)) {
					/* The highest frame rate is the most stable */
					extend_frame = OPLUS_ADFR_MIN_FPS_120HZ;
				} else if ((p_oplus_adfr_params->idle_mode == OPLUS_ADFR_IDLE_OFF) && (extend_frame < OPLUS_ADFR_MIN_FPS_20HZ)
					&& (extend_frame >= OPLUS_ADFR_MIN_FPS_1HZ)) {
					if (oplus_adfr_idle_mode_is_enabled(p_oplus_adfr_params)) {
						/* force to 20hz if the min fps is less than 20hz when auto mode is off and idle mode is also off */
						extend_frame = OPLUS_ADFR_MIN_FPS_20HZ;
					}
				}
			} else if (refresh_rate == 90) {
				if ((extend_frame > OPLUS_ADFR_MIN_FPS_90HZ) || (extend_frame < OPLUS_ADFR_MIN_FPS_1HZ)) {
					/* The highest frame rate is the most stable */
					extend_frame = OPLUS_ADFR_MIN_FPS_90HZ;
				} else if ((p_oplus_adfr_params->idle_mode == OPLUS_ADFR_IDLE_OFF) && (extend_frame < OPLUS_ADFR_MIN_FPS_15HZ)
					&& (extend_frame >= OPLUS_ADFR_MIN_FPS_1HZ)) {
					if (oplus_adfr_idle_mode_is_enabled(p_oplus_adfr_params)) {
						/* force to 20hz if the min fps is less than 20hz when auto mode is off and idle mode is also off */
						extend_frame = OPLUS_ADFR_MIN_FPS_15HZ;
					}
				}
			}
		} else {
			if (refresh_rate == 120) {
				if ((extend_frame > OPLUS_ADFR_MIN_FPS_120HZ) || (extend_frame < OPLUS_ADFR_MIN_FPS_1HZ)) {
					extend_frame = OPLUS_ADFR_MIN_FPS_120HZ;
				}
			} else if (refresh_rate == 90) {
				if ((extend_frame > OPLUS_ADFR_MIN_FPS_90HZ) || (extend_frame < OPLUS_ADFR_MIN_FPS_1HZ)) {
					extend_frame = OPLUS_ADFR_MIN_FPS_90HZ;
				}
			}
		}
	} else if (h_skew == STANDARD_MFR) {
		if ((extend_frame > OPLUS_ADFR_MIN_FPS_60HZ) || (extend_frame < OPLUS_ADFR_MIN_FPS_1HZ)) {
			extend_frame = OPLUS_ADFR_MIN_FPS_60HZ;
		} else if ((p_oplus_adfr_params->idle_mode == OPLUS_ADFR_IDLE_OFF)
				&& (extend_frame < OPLUS_ADFR_MIN_FPS_20HZ)
				&& (extend_frame >= OPLUS_ADFR_MIN_FPS_1HZ)) {
			if (oplus_adfr_idle_mode_is_enabled(p_oplus_adfr_params)) {
				extend_frame = OPLUS_ADFR_MIN_FPS_20HZ;
			}
		}
	} else if (h_skew == OPLUS_ADFR) {
		if (extend_frame > OPLUS_ADFR_MIN_FPS_60HZ || extend_frame < OPLUS_ADFR_MIN_FPS_30HZ)
			extend_frame = OPLUS_ADFR_MIN_FPS_120HZ;
	}

	ADFR_DEBUG("end\n");

	return extend_frame;
}

static int oplus_adfr_send_auto_minfps_dcs(struct drm_crtc *crtc, u32 extend_frame)
{
	int rc = 0;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_crtc_state *crtc_state;
	struct oplus_adfr_params *p_oplus_adfr_params = NULL;

	if (!crtc) {
		ADFR_ERR("Invalid drm_crtc param\n");
		return extend_frame;
	}

	p_oplus_adfr_params = oplus_adfr_get_params_by_crtc(crtc);
	if (!p_oplus_adfr_params) {
		ADFR_ERR("Invalid p_oplus_adfr_params param\n");
		return extend_frame;
	}

	if (!oplus_adfr_is_supported(p_oplus_adfr_params)) {
		ADFR_ERR("ADFR is not supported\n");
		return extend_frame;
	}

	/* SDC's auto, fakeframe and minfps are available only after power on */
	crtc_state = to_mtk_crtc_state(crtc->state);
	if (!mtk_crtc->enabled || crtc_state->prop_val[CRTC_PROP_DOZE_ACTIVE]) {
		ADFR_WARN("ignore %s:%d %u when power is off", __func__, __LINE__, extend_frame);
		return 0;
	}
/*
	if (!mtk_crtc->panel_ext) {
		return -1;
	} else {
		ADFR_INFO("oplus_minfps0_cfg:%d,oplus_minfps1_cfg:%d\n",
			mtk_crtc->panel_ext->params->oplus_minfps0_cfg, mtk_crtc->panel_ext->params->oplus_minfps1_cfg);
		if (p_oplus_adfr_params->auto_mode == OPLUS_ADFR_AUTO_OFF) {
			if (!(mtk_crtc->panel_ext->params->oplus_minfps0_cfg & 0x00000001)) {
				ADFR_INFO("ignore oplus_minfps0_cfg %s:%d", __func__, __LINE__);
				return -1;
			}
		} else {
			if (!(mtk_crtc->panel_ext->params->oplus_minfps1_cfg & 0x00000001)) {
				ADFR_INFO("ignore oplus_minfps1_cfg %s:%d", __func__, __LINE__);
				return -1;
			}
		}
	}
*/
	ADFR_DEBUG("start\n");

	/*check minfps*/
	extend_frame = oplus_adfr_auto_minfps_check(crtc, extend_frame);

	rc = oplus_adfr_send_minfps_dcs(crtc, p_oplus_adfr_params->auto_mode, extend_frame);
	ADFR_INFO("extern_frame=%d, rc=%d\n", extend_frame, rc);

	ADFR_DEBUG("end\n");

	return rc;
}

void oplus_adfr_auto_mode_update(void *mtk_drm_crtc)
{
	struct mtk_drm_crtc *mtk_crtc = mtk_drm_crtc;
	struct drm_crtc *crtc;
	int h_skew = STANDARD_ADFR;
	static int old_h_skew = STANDARD_ADFR;
	int rc = 0;
	struct oplus_adfr_params *p_oplus_adfr_params = NULL;

	if (!mtk_crtc) {
		ADFR_ERR("Invalid mtk_drm_crtc param\n");
		return;
	}

	/* this debug cmd only for crtc0 */
	crtc = &(mtk_crtc->base);
	if (!crtc) {
		ADFR_ERR("find drm_crtc fail\n");
		return;
	}

	p_oplus_adfr_params = oplus_adfr_get_params_by_crtc(crtc);
	if (!p_oplus_adfr_params) {
		ADFR_ERR("Invalid p_oplus_adfr_params param\n");
		return;
	}

	if (!oplus_adfr_is_supported(p_oplus_adfr_params)) {
		ADFR_DEBUG("ADFR is not supported\n");
		return;
	}

	ADFR_DEBUG("start\n");

	h_skew = crtc->state->mode.hskew;
	if ((h_skew != STANDARD_ADFR) && (h_skew != STANDARD_MFR) && (h_skew != OPLUS_ADFR)) {
		ADFR_ERR("OPLUS ADFR does not support auto mode setting\n");
		return;
	}

	OPLUS_ADFR_TRACE_BEGIN("dsi_display_auto_mode_update");

	if ((h_skew == OPLUS_ADFR) && (old_h_skew != OPLUS_ADFR)) {
		p_oplus_adfr_params->sa_min_fps = 0;
	}
	if (h_skew == OPLUS_ADFR) {
		u32 minfps = 0;
		mutex_lock(&(p_oplus_adfr_params->multite_mutex));
		minfps = oplus_adfr_auto_minfps_check(crtc, p_oplus_adfr_params->sa_min_fps);
		if (false == oplus_adfr_get_multite_state(crtc)) {
			if (minfps >= 30 && minfps <= 60) {
				rc = oplus_adfr_send_multite(crtc, true);
				if (0 == rc)
					oplus_adfr_set_multite_state(crtc, true);
			}
		} else {
			if (minfps == 120) {
				rc = oplus_adfr_send_multite(crtc, false);
				if (0 == rc)
					oplus_adfr_set_multite_state(crtc, false);
			}
		}
		mutex_unlock(&(p_oplus_adfr_params->multite_mutex));
	}

	if (p_oplus_adfr_params->auto_mode_updated) {
		oplus_adfr_auto_mode_enable(crtc, p_oplus_adfr_params->auto_mode);
		p_oplus_adfr_params->auto_mode_updated = false;
	}

	if (p_oplus_adfr_params->sa_min_fps_updated) {
		if (p_oplus_adfr_params->skip_min_fps_setting) {
			ADFR_INFO("filter min fps %u setting\n", p_oplus_adfr_params->sa_min_fps);
		} else {
			oplus_adfr_send_auto_minfps_dcs(crtc, p_oplus_adfr_params->sa_min_fps);
		}
		p_oplus_adfr_params->sa_min_fps_updated = false;
	}

	old_h_skew = h_skew;
	OPLUS_ADFR_TRACE_END("dsi_display_auto_mode_update");

	ADFR_DEBUG("end\n");
	return;
}

int oplus_adfr_send_min_fps_event(unsigned int min_fps)
{
	ADFR_DEBUG("start\n");

	lcdinfo_notify(MTK_DISP_EVENT_ADFR_MIN_FPS, &min_fps);
	ADFR_INFO("MTK_DISP_EVENT_ADFR_MIN_FPS:%u\n", min_fps);

	ADFR_DEBUG("end\n");

	return 0;
}

/* Add for adfr status reset */
/* reset auto mode status as panel power on and timing switch to SM */
void oplus_adfr_status_reset(void *mtk_drm_crtc)
{
	struct mtk_drm_crtc *mtk_crtc = mtk_drm_crtc;
	struct drm_crtc *crtc = NULL;
	struct oplus_adfr_params *p_oplus_adfr_params = NULL;
	u32 refresh_rate = 120;
	u32 h_skew = STANDARD_ADFR;

	if (!mtk_crtc) {
		ADFR_ERR("Invalid mtk_drm_crtc param\n");
		return;
	}

	crtc = &(mtk_crtc->base);
	if (!crtc) {
		ADFR_ERR("Invalid crtc params\n");
		return;
	}

	h_skew = crtc->state->mode.hskew;
	refresh_rate = drm_mode_vrefresh(&crtc->state->mode);

	p_oplus_adfr_params = oplus_adfr_get_params_by_crtc(crtc);
	if (!p_oplus_adfr_params) {
		ADFR_ERR("Invalid p_oplus_adfr_params param\n");
		return;
	}

	if (!oplus_adfr_is_supported(p_oplus_adfr_params)) {
		ADFR_DEBUG("ADFR is not supported\n");
		return;
	}

	ADFR_DEBUG("start\n");

	if ((h_skew == STANDARD_ADFR) || (h_skew == STANDARD_MFR)) {
		p_oplus_adfr_params->auto_mode = OPLUS_ADFR_AUTO_OFF;
		if (refresh_rate == 60) {
			p_oplus_adfr_params->sa_min_fps = OPLUS_ADFR_MIN_FPS_60HZ;
		} else {
			/* 90hz min fps in auto mode off should be 0x08 which will be corrected before cmd sent */
			p_oplus_adfr_params->sa_min_fps = OPLUS_ADFR_MIN_FPS_120HZ;
		}

		/* update auto mode and qsync para when timing switch or panel enable for debug */
		OPLUS_ADFR_TRACE_INT("p_oplus_adfr_params->auto_mode|%d", p_oplus_adfr_params->auto_mode);
		OPLUS_ADFR_TRACE_INT("p_oplus_adfr_params->sa_min_fps|%d", p_oplus_adfr_params->sa_min_fps);
		OPLUS_ADFR_TRACE_INT("p_oplus_adfr_params->auto_mode_cmd|%d", p_oplus_adfr_params->auto_mode);
		ADFR_WARN("auto mode reset: auto mode %d, min fps %d\n", p_oplus_adfr_params->auto_mode, p_oplus_adfr_params->sa_min_fps);
	} else {
		p_oplus_adfr_params->sa_min_fps = OPLUS_ADFR_MIN_FPS_120HZ;
		OPLUS_ADFR_TRACE_INT("p_oplus_adfr_params->auto_mode_cmd|%d", 0);
	}
	OPLUS_ADFR_TRACE_INT("h_skew|%d", h_skew);

	oplus_adfr_send_min_fps_event(p_oplus_adfr_params->sa_min_fps);

	ADFR_DEBUG("end\n");

	return;
}
EXPORT_SYMBOL(oplus_adfr_status_reset);

/* the highest min fps setting is required when the temperature meets certain conditions, otherwise recovery it */
int oplus_adfr_temperature_detection_handle(void *mtk_ddp_comp, void *cmdq_pkt, int ntc_temp, int shell_temp)
{
	static bool last_skip_min_fps_setting = false;
	unsigned int refresh_rate = 120;
	unsigned int h_skew = STANDARD_ADFR;
	struct mtk_ddp_comp *comp = mtk_ddp_comp;
	struct oplus_adfr_params *p_oplus_adfr_params = NULL;
	struct cmdq_pkt *cmdq_handle = cmdq_pkt;
	struct drm_crtc *crtc = NULL;
	struct drm_display_mode *drm_mode = NULL;
	struct oplus_minfps minfps;

	if (!comp || !cmdq_handle) {
		ADFR_ERR("Invalid mtk_ddp_comp or cmdq_handle pointer");
		return -EINVAL;
	}

	p_oplus_adfr_params = oplus_adfr_get_params_by_comp(comp);
	if (!p_oplus_adfr_params) {
		ADFR_ERR("Invalid p_oplus_adfr_params param\n");
		return -EINVAL;
	}

	if (!oplus_adfr_temperature_detection_is_enabled(p_oplus_adfr_params)) {
		ADFR_ERR("temperature compensation is not supported\n");
		return 0;
	}

	crtc = &(comp->mtk_crtc->base);
	if (!crtc) {
		ADFR_ERR("Invalid crtc params\n");
		return -EINVAL;
	}

	drm_mode = &(crtc->state->mode);
	if (!drm_mode) {
		ADFR_ERR("Invalid drm_mode params\n");
		return -EINVAL;
	}

	ADFR_DEBUG("start\n");

	refresh_rate = drm_mode_vrefresh(drm_mode);
	h_skew = crtc->state->mode.hskew;

	if ((h_skew != OPLUS_ADFR)
			&& ((abs(ntc_temp - shell_temp) >= 5)
				|| (ntc_temp < 0)
				|| (shell_temp < 0)
				|| (((ntc_temp > 45) || (shell_temp > 45)) && (refresh_rate == 120))
				|| (((ntc_temp > 40) || (shell_temp > 40)) && (refresh_rate == 90))
				|| (((ntc_temp > 40) || (shell_temp > 40)) && (refresh_rate == 60)))) {
		p_oplus_adfr_params->skip_min_fps_setting = true;

		if (!last_skip_min_fps_setting && p_oplus_adfr_params->skip_min_fps_setting) {
			if (((p_oplus_adfr_params->sa_min_fps == 0) && (refresh_rate == 120))
					|| ((p_oplus_adfr_params->sa_min_fps == 0) && (refresh_rate == 90))
					|| ((p_oplus_adfr_params->sa_min_fps == 1) && (refresh_rate == 60))) {
				ADFR_INFO("ntc_temp:%d,shell_temp:%d,refresh_rate:%u,already in min fps %u\n",
						ntc_temp, shell_temp, refresh_rate, p_oplus_adfr_params->sa_min_fps);
			} else {
				minfps.minfps_flag = p_oplus_adfr_params->auto_mode;
				minfps.extend_frame = refresh_rate;
				ADFR_INFO("ntc_temp:%d,shell_temp:%d,refresh_rate:%u,need to set min fps to %u\n",
						ntc_temp, shell_temp, refresh_rate, minfps.extend_frame);
				mtk_ddp_comp_io_cmd(comp, cmdq_handle, SET_MINFPS, &minfps);
			}
		}
	} else {
		p_oplus_adfr_params->skip_min_fps_setting = false;

		if (last_skip_min_fps_setting && !p_oplus_adfr_params->skip_min_fps_setting) {
			if (((p_oplus_adfr_params->sa_min_fps == 0) && (refresh_rate == 120))
					|| ((p_oplus_adfr_params->sa_min_fps == 0) && (refresh_rate == 90))
					|| ((p_oplus_adfr_params->sa_min_fps == 1) && (refresh_rate == 60))) {
				p_oplus_adfr_params->sa_min_fps_updated = false;
				ADFR_INFO("ntc_temp:%d,shell_temp:%d,refresh_rate:%u,no need to update min fps %u\n",
						ntc_temp, shell_temp, refresh_rate, p_oplus_adfr_params->sa_min_fps);
			} else {
				p_oplus_adfr_params->sa_min_fps_updated = true;
				ADFR_INFO("ntc_temp:%d,shell_temp:%d,refresh_rate:%u,need to recovery min fps to %u\n",
						ntc_temp, shell_temp, refresh_rate, p_oplus_adfr_params->sa_min_fps);
			}
		}
	}

	last_skip_min_fps_setting = p_oplus_adfr_params->skip_min_fps_setting;

	ADFR_DEBUG("end\n");

	return 0;
}

int oplus_adfr_send_multite(void *drm_crtc, bool enable)
{
	struct drm_crtc *crtc = drm_crtc;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct cmdq_pkt *cmdq_handle;
	struct mtk_crtc_state *crtc_state;
	struct mtk_ddp_comp *comp = mtk_ddp_comp_request_output(mtk_crtc);
	struct mtk_cmdq_cb_data *cb_data;
	bool is_frame_mode;

	/* SDC's auto, fakeframe and minfps are available only after power on */
	crtc_state = to_mtk_crtc_state(crtc->state);
	if (!mtk_crtc->enabled || crtc_state->prop_val[CRTC_PROP_DOZE_ACTIVE]) {
		ADFR_WARN("ignore %s when power is off", __func__);
		return -1;
	}
	if (!(comp && comp->funcs && comp->funcs->io_cmd)) {
		ADFR_ERR("Invalid mtk_ddp_comp params\n");
		return -1;
	}

	ADFR_DEBUG("start\n");

	if (enable) {
		ADFR_WARN("ctrl:%d multite on\n");
		OPLUS_ADFR_TRACE_INT("oplus_adfr_multite_cmd|%d", 1);
	} else {
		ADFR_WARN("ctrl:%d multite off\n");
		OPLUS_ADFR_TRACE_INT("oplus_adfr_multite_cmd|%d", 0);
	}

	is_frame_mode = mtk_crtc_is_frame_trigger_mode(&mtk_crtc->base);
	if (is_frame_mode) {
		mtk_drm_idlemgr_kick(__func__, crtc, 0);
		mtk_crtc_pkt_create(&cmdq_handle, &mtk_crtc->base,
						mtk_crtc->gce_obj.client[CLIENT_CFG]);

		if (mtk_crtc_with_sub_path(crtc, mtk_crtc->ddp_mode))
			mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle,
				DDP_SECOND_PATH, 0);
		else
			mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle,
				DDP_FIRST_PATH, 0);

		cmdq_pkt_clear_event(cmdq_handle,
				 mtk_crtc->gce_obj.event[EVENT_STREAM_BLOCK]);
		cmdq_pkt_wfe(cmdq_handle,
		 				 mtk_crtc->gce_obj.event[EVENT_CABC_EOF]);

		if (comp && comp->funcs && comp->funcs->io_cmd)
			comp->funcs->io_cmd(comp, cmdq_handle, SET_MULTITE, &enable);

		cmdq_pkt_set_event(cmdq_handle,
					mtk_crtc->gce_obj.event[EVENT_CABC_EOF]);
		cmdq_pkt_set_event(cmdq_handle,
					mtk_crtc->gce_obj.event[EVENT_STREAM_BLOCK]);

		cb_data = kmalloc(sizeof(*cb_data), GFP_KERNEL);
		if (cb_data) {
			cb_data->cmdq_handle = cmdq_handle;
			/* indicate multite enable/disable cmd */
			cb_data->misc = 6;
			cmdq_pkt_flush_threaded(cmdq_handle, lcm_cmd_cmdq_cb, cb_data);
		} else {
			cmdq_pkt_flush(cmdq_handle);
			cmdq_pkt_destroy(cmdq_handle);
		}
	}

	ADFR_DEBUG("end\n");

	return 0;
}

bool oplus_adfr_get_multite_state(void *drm_crtc)
{
	struct drm_crtc *crtc = drm_crtc;
	struct oplus_adfr_params *p_oplus_adfr_params = NULL;

	if (!crtc) {
		ADFR_ERR("Invalid drm_crtc param\n");
		return false;
	}

	p_oplus_adfr_params = oplus_adfr_get_params_by_crtc(crtc);
	if (!p_oplus_adfr_params) {
		ADFR_ERR("Invalid p_oplus_adfr_params param\n");
		return false;
	}

	if (!oplus_adfr_is_supported(p_oplus_adfr_params)) {
		ADFR_ERR("ADFR is not supported\n");
		return false;
	}

	return p_oplus_adfr_params->enable_multite;
}

void oplus_adfr_set_multite_state(void *drm_crtc, bool state)
{
	struct drm_crtc *crtc = drm_crtc;
	struct oplus_adfr_params *p_oplus_adfr_params = NULL;

	if (!crtc) {
		ADFR_ERR("Invalid drm_crtc param\n");
		return;
	}

	p_oplus_adfr_params = oplus_adfr_get_params_by_crtc(crtc);
	if (!p_oplus_adfr_params) {
		ADFR_ERR("Invalid p_oplus_adfr_params param\n");
		return;
	}

	if (!oplus_adfr_is_supported(p_oplus_adfr_params)) {
		ADFR_ERR("ADFR is not supported\n");
		return;
	}

	p_oplus_adfr_params->enable_multite = state;
}
EXPORT_SYMBOL(oplus_adfr_set_multite_state);

/* --------------- idle mode ---------------*/
/* if idle mode is on, the min fps will be reduced when entering MIPI idle and increased when leaving MIPI idle, thus saving power more accurately */
void oplus_adfr_handle_idle_mode(void *drm_crtc, int enter_idle)
{
	struct drm_crtc *crtc = drm_crtc;
	struct cmdq_pkt *handle;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	u32 h_skew = STANDARD_ADFR;
	u32 refresh_rate = 120;
	struct oplus_adfr_params *p_oplus_adfr_params = NULL;

	if (!crtc) {
		ADFR_ERR("Invalid drm_crtc param\n");
		return;
	}

	p_oplus_adfr_params = oplus_adfr_get_params_by_crtc(crtc);
	if (!p_oplus_adfr_params) {
		ADFR_ERR("Invalid p_oplus_adfr_params param\n");
		return;
	}

	if (!oplus_adfr_idle_mode_is_enabled(p_oplus_adfr_params)) {
		ADFR_DEBUG("ADFR idle mode is not supported\n");
		return;
	}

	if (!mtk_crtc) {
		ADFR_ERR("Invalid mtk_drm_crtc params\n");
		return;
	}

	ADFR_DEBUG("start\n");

	OPLUS_ADFR_TRACE_BEGIN("oplus_adfr_handle_idle_mode");

	h_skew = crtc->state->mode.hskew;
	refresh_rate = drm_mode_vrefresh(&crtc->state->mode);

	if (enter_idle) {
		if (h_skew == STANDARD_ADFR || h_skew == STANDARD_MFR) {
			if (refresh_rate == 120 || refresh_rate == 60) {
				/* enter idle mode if auto mode is off and min fps is less than 20hz */
				if ((p_oplus_adfr_params->auto_mode == OPLUS_ADFR_AUTO_OFF) && (p_oplus_adfr_params->sa_min_fps > OPLUS_ADFR_MIN_FPS_20HZ)
					&& (p_oplus_adfr_params->sa_min_fps <= OPLUS_ADFR_MIN_FPS_1HZ)) {
					p_oplus_adfr_params->idle_mode = OPLUS_ADFR_IDLE_ON;
					ADFR_INFO("idle mode on");

					/* send min fps before enter idle */
					ADFR_INFO("enter idle, min fps %d", p_oplus_adfr_params->sa_min_fps);
					oplus_adfr_send_auto_minfps_dcs(crtc, p_oplus_adfr_params->sa_min_fps);

					/* wait for the min fps cmds transmission to complete */
					mtk_crtc_pkt_create(&handle, &mtk_crtc->base, mtk_crtc->gce_obj.client[CLIENT_CFG]);
					cmdq_pkt_flush(handle);
					cmdq_pkt_destroy(handle);
				}
			}
		}
	} else {
		/* exit idle mode */
		if (p_oplus_adfr_params->idle_mode == OPLUS_ADFR_IDLE_ON) {
			/* send min fps after exit idle */
			ADFR_INFO("exit idle, min fps %d", OPLUS_ADFR_MIN_FPS_20HZ);
			oplus_adfr_send_auto_minfps_dcs(crtc, OPLUS_ADFR_MIN_FPS_20HZ);

			p_oplus_adfr_params->idle_mode = OPLUS_ADFR_IDLE_OFF;
			ADFR_INFO("idle mode off");
		}
	}

	OPLUS_ADFR_TRACE_INT("oplus_adfr_idle_mode|%d", p_oplus_adfr_params->idle_mode);
	OPLUS_ADFR_TRACE_END("oplus_adfr_handle_idle_mode");

	ADFR_DEBUG("end\n");

	return;
}

struct drm_crtc *current_crtc = NULL;

void oplus_adfr_set_current_crtc(void *drm_crtc)
{
	current_crtc = (struct drm_crtc *)drm_crtc;
}
EXPORT_SYMBOL(oplus_adfr_set_current_crtc);

struct drm_crtc *oplus_adfr_get_current_crtc(void)
{
	return current_crtc;
}

/* -------------------- node -------------------- */
/* adfr_config */
ssize_t oplus_adfr_set_config_attr(struct kobject *obj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int config = 0;
	struct drm_crtc *crtc = oplus_adfr_get_current_crtc();
	struct oplus_adfr_params *p_oplus_adfr_params = NULL;

	ADFR_DEBUG("start\n");

	if (!buf || !crtc) {
		ADFR_ERR("invalid buf or crtc params\n");
		return -EINVAL;
	}

	p_oplus_adfr_params = oplus_adfr_get_params_by_crtc(crtc);
	if (!p_oplus_adfr_params) {
		ADFR_ERR("invalid p_oplus_adfr_params param\n");
		return count;
	}

	OPLUS_ADFR_TRACE_BEGIN("oplus_adfr_set_adfr_config_attr");

	sscanf(buf, "%x", &config);

	p_oplus_adfr_params->config = config;
	ADFR_INFO("oplus_adfr_config:0x%x\n", p_oplus_adfr_params->config);
	OPLUS_ADFR_TRACE_INT("oplus_adfr_config", p_oplus_adfr_params->config);

	OPLUS_ADFR_TRACE_END("oplus_adfr_set_adfr_config_attr");

	ADFR_DEBUG("end\n");

	return count;
}

ssize_t oplus_adfr_get_config_attr(struct kobject *obj,
	struct kobj_attribute *attr, char *buf)
{
	struct drm_crtc *crtc = oplus_adfr_get_current_crtc();
	struct oplus_adfr_params *p_oplus_adfr_params = NULL;

	ADFR_DEBUG("start\n");

	if (!buf || !crtc) {
		ADFR_ERR("invalid buf or crtc params\n");
		return -EINVAL;
	}

	p_oplus_adfr_params = oplus_adfr_get_params_by_crtc(crtc);
	if (!p_oplus_adfr_params) {
		ADFR_ERR("invalid p_oplus_adfr_params param\n");
		return -EINVAL;
	}

	OPLUS_ADFR_TRACE_BEGIN("oplus_adfr_get_config_attr");

	ADFR_INFO("oplus_adfr_config:0x%x\n", p_oplus_adfr_params->config);

	OPLUS_ADFR_TRACE_END("oplus_adfr_get_config_attr");

	ADFR_DEBUG("end\n");

	return sysfs_emit(buf, "0x%x\n", p_oplus_adfr_params->config);
}

/* test te */
int oplus_adfr_set_test_te(void *buf)
{
	unsigned int *test_te_config = buf;
	unsigned int test_te_irq = 0;
	struct drm_crtc *crtc = oplus_adfr_get_current_crtc();
	struct oplus_adfr_params *p_oplus_adfr_params = NULL;

	ADFR_DEBUG("start\n");

	if (!buf || !crtc) {
		ADFR_ERR("invalid buf or crtc params\n");
		return -EINVAL;
	}

	p_oplus_adfr_params = oplus_adfr_get_params_by_crtc(crtc);
	if (!p_oplus_adfr_params) {
		ADFR_ERR("invalid p_oplus_adfr_params param\n");
		return -EINVAL;
	}

	if (!oplus_adfr_is_supported(p_oplus_adfr_params)) {
		ADFR_ERR("adfr is not supported\n");
		return -EINVAL;
	}

	if (IS_ERR(p_oplus_adfr_params->test_te.gpio)) {
		ADFR_ERR("test te gpio is Invalid, no need to handle test te irq\n");
		return -EINVAL;
	}

	OPLUS_ADFR_TRACE_BEGIN("oplus_adfr_set_test_te");

	p_oplus_adfr_params->test_te.config = *test_te_config;
	ADFR_INFO("oplus_adfr_test_te_config:%u\n", p_oplus_adfr_params->test_te.config);
	OPLUS_ADFR_TRACE_INT("oplus_adfr_test_te_config", p_oplus_adfr_params->test_te.config);

	test_te_irq = gpiod_to_irq(p_oplus_adfr_params->test_te.gpio);
	if (p_oplus_adfr_params->test_te.config != OPLUS_ADFR_TEST_TE_DISABLE) {
		enable_irq(test_te_irq);
		ADFR_INFO("enable test te irq\n");
	} else {
		disable_irq(test_te_irq);
		ADFR_INFO("disable test te irq\n");
	}

	OPLUS_ADFR_TRACE_END("oplus_adfr_set_test_te");

	ADFR_DEBUG("end\n");

	return 0;
}

int oplus_adfr_get_test_te(void *buf)
{
	unsigned int *refresh_rate = buf;
	struct drm_crtc *crtc = oplus_adfr_get_current_crtc();
	struct oplus_adfr_params *p_oplus_adfr_params = NULL;

	ADFR_DEBUG("start\n");

	if (!buf || !crtc) {
		ADFR_ERR("invalid buf or crtc params\n");
		return -EINVAL;
	}

	p_oplus_adfr_params = oplus_adfr_get_params_by_crtc(crtc);
	if (!p_oplus_adfr_params) {
		ADFR_ERR("invalid p_oplus_adfr_params param\n");
		return -EINVAL;
	}

	if (!oplus_adfr_is_supported(p_oplus_adfr_params)) {
		ADFR_ERR("adfr is not supported\n");
		return -EINVAL;
	}

	if (!crtc->state) {
		ADFR_ERR("invalid crtc state param\n");
		return -EINVAL;
	}

	OPLUS_ADFR_TRACE_BEGIN("oplus_adfr_get_test_te");

	if (IS_ERR(p_oplus_adfr_params->test_te.gpio)) {
		*refresh_rate = drm_mode_vrefresh(&crtc->state->mode);
		ADFR_INFO("test te gpio is invalid, use current timing refresh rate\n");
	} else {
		*refresh_rate = p_oplus_adfr_params->test_te.refresh_rate;
	}

	ADFR_DEBUG("oplus_adfr_test_te_refresh_rate:%u\n", *refresh_rate);

	OPLUS_ADFR_TRACE_END("oplus_adfr_get_test_te");

	ADFR_DEBUG("end\n");

	return 0;
}

ssize_t oplus_adfr_set_test_te_attr(struct kobject *obj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int test_te_config = OPLUS_ADFR_TEST_TE_DISABLE;
	unsigned int test_te_irq = 0;
	struct drm_crtc *crtc = oplus_adfr_get_current_crtc();
	struct oplus_adfr_params *p_oplus_adfr_params = NULL;

	ADFR_DEBUG("start\n");

	if (!buf || !crtc) {
		ADFR_ERR("invalid buf or crtc params\n");
		return -EINVAL;
	}

	p_oplus_adfr_params = oplus_adfr_get_params_by_crtc(crtc);
	if (!p_oplus_adfr_params) {
		ADFR_ERR("invalid p_oplus_adfr_params param\n");
		return count;
	}

	if (!oplus_adfr_is_supported(p_oplus_adfr_params)) {
		ADFR_ERR("adfr is not supported\n");
		return count;
	}

	if (IS_ERR(p_oplus_adfr_params->test_te.gpio)) {
		ADFR_ERR("test te gpio is invalid, should not set test te\n");
		return count;
	}

	OPLUS_ADFR_TRACE_BEGIN("oplus_adfr_set_test_te_attr");

	sscanf(buf, "%u", &test_te_config);

	p_oplus_adfr_params->test_te.config = test_te_config;
	ADFR_INFO("oplus_adfr_test_te_config:%u\n", p_oplus_adfr_params->test_te.config);
	OPLUS_ADFR_TRACE_INT("oplus_adfr_test_te_config", p_oplus_adfr_params->test_te.config);

	test_te_irq = gpiod_to_irq(p_oplus_adfr_params->test_te.gpio);
	if (p_oplus_adfr_params->test_te.config != OPLUS_ADFR_TEST_TE_DISABLE) {
		enable_irq(test_te_irq);
		ADFR_INFO("enable test te irq\n");
	} else {
		disable_irq(test_te_irq);
		ADFR_INFO("disable test te irq\n");
	}

	OPLUS_ADFR_TRACE_END("oplus_adfr_set_test_te_attr");

	ADFR_DEBUG("end\n");

	return count;
}

ssize_t oplus_adfr_get_test_te_attr(struct kobject *obj,
	struct kobj_attribute *attr, char *buf)
{
	unsigned int refresh_rate = 0;
	struct drm_crtc *crtc = oplus_adfr_get_current_crtc();
	struct oplus_adfr_params *p_oplus_adfr_params = NULL;

	ADFR_DEBUG("start\n");

	if (!buf || !crtc) {
		ADFR_ERR("invalid buf or crtc params\n");
		return -EINVAL;
	}

	p_oplus_adfr_params = oplus_adfr_get_params_by_crtc(crtc);
	if (!p_oplus_adfr_params) {
		ADFR_ERR("invalid p_oplus_adfr_params param\n");
		return -EINVAL;
	}

	if (!oplus_adfr_is_supported(p_oplus_adfr_params)) {
		ADFR_ERR("adfr is not supported\n");
		return -EINVAL;
	}

	if (!crtc->state) {
		ADFR_ERR("invalid crtc state param\n");
		return -EINVAL;
	}

	OPLUS_ADFR_TRACE_BEGIN("oplus_adfr_get_test_te_attr");

	if (IS_ERR(p_oplus_adfr_params->test_te.gpio)) {
		refresh_rate = drm_mode_vrefresh(&crtc->state->mode);
		ADFR_INFO("test te gpio is invalid, use current timing refresh rate\n");
	} else {
		refresh_rate = p_oplus_adfr_params->test_te.refresh_rate;
	}

	ADFR_INFO("oplus_adfr_test_te_refresh_rate:%u\n", refresh_rate);

	OPLUS_ADFR_TRACE_END("oplus_adfr_get_test_te_attr");

	ADFR_DEBUG("end\n");

	return sysfs_emit(buf, "%u\n", refresh_rate);
}

ssize_t oplus_adfr_set_min_fps_attr(struct kobject *obj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int minfps = 120;
	unsigned int h_skew = STANDARD_ADFR;
	struct drm_crtc *crtc = oplus_adfr_get_current_crtc();
	struct oplus_adfr_params *p_oplus_adfr_params = NULL;

	ADFR_DEBUG("start\n");

	if (!buf || !crtc) {
		ADFR_ERR("invalid buf or crtc params\n");
		return -EINVAL;
	}

	p_oplus_adfr_params = oplus_adfr_get_params_by_crtc(crtc);
	if (!p_oplus_adfr_params) {
		ADFR_ERR("invalid p_oplus_adfr_params param\n");
		return -EINVAL;
	}

	if (!oplus_adfr_is_supported(p_oplus_adfr_params)) {
		ADFR_ERR("adfr is not supported\n");
		return -EINVAL;
	}

	if (IS_ERR(p_oplus_adfr_params->test_te.gpio)) {
		ADFR_ERR("test te gpio is Invalid, no need to handle test te irq\n");
		return -EINVAL;
	}

	OPLUS_ADFR_TRACE_BEGIN("oplus_adfr_set_min_fps_attr");

	sscanf(buf, "%u", &minfps);

	h_skew = crtc->state->mode.hskew;
	if (h_skew == STANDARD_ADFR || h_skew == STANDARD_MFR) {
		p_oplus_adfr_params->sa_min_fps = minfps;
		p_oplus_adfr_params->sa_min_fps_updated = true;
		ADFR_INFO("oplus_adfr_sa_min_fps:%u\n", p_oplus_adfr_params->sa_min_fps);
		OPLUS_ADFR_TRACE_INT("oplus_adfr_sa_min_fps", p_oplus_adfr_params->sa_min_fps);
	} else {
		ADFR_INFO("not for OA minfps update, because of more complex processes, to be improved");
	}

	OPLUS_ADFR_TRACE_END("oplus_adfr_set_min_fps_attr");

	ADFR_DEBUG("end\n");

	return count;
}

ssize_t oplus_adfr_get_min_fps_attr(struct kobject *obj,
	struct kobj_attribute *attr, char *buf)
{
	unsigned int minfps = 120;
	unsigned int h_skew = STANDARD_ADFR;
	struct drm_crtc *crtc = oplus_adfr_get_current_crtc();
	struct oplus_adfr_params *p_oplus_adfr_params = NULL;

	ADFR_DEBUG("start\n");

	if (!buf || !crtc) {
		ADFR_ERR("invalid buf or crtc params\n");
		return -EINVAL;
	}

	p_oplus_adfr_params = oplus_adfr_get_params_by_crtc(crtc);
	if (!p_oplus_adfr_params) {
		ADFR_ERR("invalid p_oplus_adfr_params param\n");
		return -EINVAL;
	}

	if (!oplus_adfr_is_supported(p_oplus_adfr_params)) {
		ADFR_ERR("adfr is not supported\n");
		return -EINVAL;
	}

	if (!crtc->state) {
		ADFR_ERR("invalid crtc state param\n");
		return -EINVAL;
	}

	OPLUS_ADFR_TRACE_BEGIN("oplus_adfr_get_min_fps_attr");

	h_skew = crtc->state->mode.hskew;
	if (h_skew == STANDARD_ADFR || h_skew == STANDARD_MFR) {
		minfps = p_oplus_adfr_params->sa_min_fps;
		ADFR_INFO("oplus_adfr_sa_min_fps:%u\n", minfps);
	} else {
		ADFR_INFO("not for OA minfps get, because of more complex processes, to be improved");
	}

	OPLUS_ADFR_TRACE_END("oplus_adfr_get_min_fps_attr");

	ADFR_DEBUG("end\n");

	return sprintf(buf, "%u\n", minfps);
}
