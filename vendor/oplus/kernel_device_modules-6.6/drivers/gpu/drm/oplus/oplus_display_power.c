/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_display_power.c
** Description : oplus display power
** Version : 1.0
** Date : 2024/05/20
** Author : Display
******************************************************************/
#include <linux/string.h>
#include <drm/drm_panel.h>
#include <linux/gpio/consumer.h>

#include "oplus_display_power.h"
#include "oplus_display_debug.h"
#include "oplus_display_device.h"
#include "oplus_display_sysfs_attrs.h"
#include "oplus_dsi_display_config.h"


PANEL_VOLTAGE_BAK panel_vol_bak[PANEL_VOLTAGE_ID_MAX] = {{0}, {0}, {2, 0, 1, 2, ""}};
u32 panel_need_recovery = 0;
EXPORT_SYMBOL(panel_vol_bak);
extern struct drm_device *get_drm_device(void);


int oplus_panel_set_vg_base(unsigned int panel_vol_value)
{
	int ret = 0;

	if (panel_vol_value > panel_vol_bak[PANEL_VOLTAGE_ID_VG_BASE].voltage_max ||
		panel_vol_value < panel_vol_bak[PANEL_VOLTAGE_ID_VG_BASE].voltage_min) {
		OPLUS_DSI_ERR("panel_vol exceeds the range\n");
		panel_need_recovery = 0;
		return -EINVAL;
	}

	if (panel_vol_value == panel_vol_bak[PANEL_VOLTAGE_ID_VG_BASE].voltage_current) {
		OPLUS_DSI_WARN("panel_vol the same as before\n");
		panel_need_recovery = 0;
	} else {
		OPLUS_DSI_INFO("set panel_vol = %d\n", panel_vol_value);
		panel_vol_bak[PANEL_VOLTAGE_ID_VG_BASE].voltage_current = panel_vol_value;
		panel_need_recovery = 1;
	}

	return ret;
}
EXPORT_SYMBOL(oplus_panel_set_vg_base);

int dsi_panel_parse_panel_power_cfg(struct panel_voltage_bak *panel_vol)
{
	int ret = 0;
	OPLUS_DSI_INFO("test");
	if (panel_vol == NULL) {
		OPLUS_DSI_ERR("error handle");
		return -1;
	}

	memcpy((void *)panel_vol_bak, panel_vol, sizeof(struct panel_voltage_bak)*PANEL_VOLTAGE_ID_MAX);

	return ret;
}
EXPORT_SYMBOL(dsi_panel_parse_panel_power_cfg);

int oplus_panel_need_recovery(unsigned int panel_vol_value)
{
	int ret = 0;

	if (panel_need_recovery == 1) {
		OPLUS_DSI_INFO("panel_need_recovery\n");
		ret = 1;
	}

	return ret;
}
EXPORT_SYMBOL(oplus_panel_need_recovery);

int oplus_display_panel_set_pwr(void *buf)
{
	struct panel_vol_set *panel_vol = buf;
	u32 panel_vol_value = 0, panel_vol_id = 0;
	int rc = 0;
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct drm_device *ddev = get_drm_device();
	/* this debug cmd only for crtc0 */
	crtc = list_first_entry(&(ddev)->mode_config.crtc_list,
		typeof(*crtc), head);
	if (!crtc) {
		OPLUS_DSI_ERR("get hbm find crtc fail\n");
		return 0;
	}
	mtk_crtc = to_mtk_crtc(crtc);

	panel_vol_id = ((panel_vol->panel_id & 0x0F)-1);
	panel_vol_value = panel_vol->panel_vol;

	OPLUS_DSI_ERR("buf = [%s], id = %d value = %d\n",
		(char *)buf, panel_vol_id, panel_vol_value);

	if (panel_vol_id < 0 || panel_vol_id >= PANEL_VOLTAGE_ID_MAX) {
		return -EINVAL;
	}

	if (panel_vol_value < panel_vol_bak[panel_vol_id].voltage_min ||
		panel_vol_id > panel_vol_bak[panel_vol_id].voltage_max) {
		return -EINVAL;
	}

	if (panel_vol_id == PANEL_VOLTAGE_ID_VG_BASE) {
		OPLUS_DSI_ERR("set the VGH_L pwr = %d\n", panel_vol_value);
		rc = oplus_panel_set_vg_base(panel_vol_value);
		if (rc < 0) {
			return rc;
		}

		return 0;
	}

	if (mtk_crtc->panel_ext->funcs->oplus_set_power) {
		rc = mtk_crtc->panel_ext->funcs->oplus_set_power(panel_vol_id, panel_vol_value);
		if (rc) {
			OPLUS_DSI_ERR("Set voltage fail, rc=%d\n",
				 rc);
			return -EINVAL;
		}

		return 0;
	}

	return -EINVAL;
}

int oplus_display_panel_get_pwr(void *buf)
{
	int ret = 0;
	u32 i = 0;

	struct panel_vol_get *panel_vol = buf;
	int pid = (panel_vol->panel_id - 1);
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct drm_device *ddev = get_drm_device();
	OPLUS_DSI_ERR("[id] = %d\n", pid);
	/* this debug cmd only for crtc0 */
	crtc = list_first_entry(&(ddev)->mode_config.crtc_list,
		typeof(*crtc), head);
	if (!crtc) {
		OPLUS_DSI_ERR("get hbm find crtc fail\n");
		return 0;
	}
	mtk_crtc = to_mtk_crtc(crtc);

	if (pid < 0 || pid >= PANEL_VOLTAGE_ID_MAX) {
		return 0;
	}

	panel_vol->panel_min = panel_vol_bak[pid].voltage_min;
	panel_vol->panel_max = panel_vol_bak[pid].voltage_max;
	panel_vol->panel_cur = panel_vol_bak[pid].voltage_current;

	if (pid >= 0 && pid < (PANEL_VOLTAGE_ID_MAX-1)) {
		if (mtk_crtc->panel_ext->funcs->oplus_update_power_value) {
			ret = mtk_crtc->panel_ext->funcs->oplus_update_power_value(panel_vol_bak[pid].voltage_id);
		}

		if (ret < 0) {
			OPLUS_DSI_ERR("update_current_voltage error = %d\n", ret);
		}
		else {
			panel_vol_bak[i].voltage_current = ret;
			panel_vol->panel_cur = panel_vol_bak[pid].voltage_current;
			ret = 0;
		}
	}

	return ret;
}

static int oplus_dsi_panel_enable_regulator(void *ctx_dev, const char *vreg_name, int flag)
{
	int rc = -EINVAL, i = 0;
	struct dsi_panel_lcm *ctx = ctx_dev;
	struct dsi_vreg *vreg_target = NULL;

	if (!ctx || !ctx->power_info.vregs) {
		OPLUS_DSI_ERR("Invalid ctx param or ctx power_info vregs\n");
		return rc;
	}
	if (ctx->power_info.count == 0) {
		OPLUS_DSI_ERR("No valid regulators to set\n");
		return rc;
	}
	for (i = 0; i < ctx->power_info.count; i++) {
		vreg_target = &ctx->power_info.vregs[i];
		if (vreg_target && (!strcmp(vreg_target->vreg_name, vreg_name))) {
			if (flag) {
				if (vreg_target->vreg_type == 0) {
					if (!IS_ERR_OR_NULL(vreg_target->vreg)) {
						rc = regulator_set_voltage(vreg_target->vreg, vreg_target->min_voltage, vreg_target->max_voltage);
						if (rc < 0) {
							OPLUS_DSI_ERR("set voltage %s fail, rc = %d\n", vreg_target->vreg_name, rc);
						}
						rc = regulator_enable(vreg_target->vreg);
						if (rc < 0) {
							OPLUS_DSI_ERR("enable regulator %s fail, rc = %d\n", vreg_target->vreg_name, rc);
						}
					} else {
						OPLUS_DSI_ERR("enable regulator %s is NULL\n", vreg_target->vreg_name);
					}
				} else if (vreg_target->vreg_type == 1) {
					if (!IS_ERR(vreg_target->vreg_gpio)) {
						gpiod_set_value(vreg_target->vreg_gpio, 1);
						rc = 0;
					} else {
						OPLUS_DSI_ERR("gpio for enable regulator %s is NULL\n", vreg_target->vreg_name);
					}
				}
			} else {
				if (vreg_target->vreg_type == 0) {
					if (!IS_ERR_OR_NULL(vreg_target->vreg)) {
						rc = regulator_disable(vreg_target->vreg);
						if (rc < 0) {
							OPLUS_DSI_ERR("disable regulator %s fail, rc = %d\n", vreg_target->vreg_name, rc);
						}
					} else {
						OPLUS_DSI_ERR("disable regulator %s is NULL\n", vreg_target->vreg_name);
					}
				} else if (vreg_target->vreg_type == 1) {
					if (!IS_ERR(vreg_target->vreg_gpio)) {
						gpiod_set_value(vreg_target->vreg_gpio, 0);
						rc = 0;
					} else {
						OPLUS_DSI_ERR("gpio for disable regulator %s is NULL\n", vreg_target->vreg_name);
					}
				}
			}
			break;
		}
	}

	return rc;
}

int oplus_dsi_panel_power_supply_enable(void *ctx_dev, int flag)
{
	int rc = -EINVAL, temp = 0, power_count = 0, i = 0, j = 0;
	struct dsi_panel_lcm *ctx = ctx_dev;
	unsigned int sleep_ms = 0;
	const char **power_sequence = NULL;

	if (!ctx) {
		OPLUS_DSI_ERR("Invalid ctx param\n");
		return rc;
	}

	if (flag) {
		power_sequence = ctx->power_on_sequence;
	} else {
		power_sequence = ctx->power_off_sequence;
	}
	power_count = ctx->power_info.count;

	rc = kstrtoint(power_sequence[0], 10, &temp);
	if (rc) {
		OPLUS_DSI_ERR("%s cannot be converted to int\n", power_sequence[0]);
		rc = -EINVAL;
		return rc;
	}
	if (temp > 0) {
		sleep_ms = temp;
		usleep_range(sleep_ms*1000, (sleep_ms*1000)+100);
	}
	for (i = 0; i < power_count; i++) {
		j = i * 2 + 1;
		if (power_sequence[j]) {
			rc = oplus_dsi_panel_enable_regulator(ctx_dev, power_sequence[j], flag);
			if (rc < 0) {
				OPLUS_DSI_ERR("%s failed to set %d\n", power_sequence[j], flag);
				return rc;
			}
		}
		if (power_sequence[j + 1]) {
			rc = kstrtoint(power_sequence[j + 1], 10, &temp);
			if (rc) {
				OPLUS_DSI_ERR("%s cannot be converted to int\n", power_sequence[j + 1]);
				rc = -EINVAL;
				return rc;
			}
			if (temp > 0) {
				sleep_ms = temp;
				usleep_range(sleep_ms*1000, (sleep_ms*1000)+100);
			}
		}
		OPLUS_DSI_INFO("%s set %d and delay %dms success\n", power_sequence[j], flag, temp);
	}

	return rc;
}

int oplus_dsi_panel_power_on(void *ctx_dev)
{
	int rc = -EINVAL;
	struct dsi_panel_lcm *ctx = ctx_dev;

	if (!ctx) {
		OPLUS_DSI_ERR("Invalid ctx param\n");
		return rc;
	}
	if (ctx->power_info.refcount == 0) {
		OPLUS_DSI_INFO("do power on\n");
		rc = oplus_dsi_panel_power_supply_enable(ctx_dev, 1);
	}
	ctx->power_info.refcount++;

	return rc;
}
EXPORT_SYMBOL(oplus_dsi_panel_power_on);

int oplus_dsi_panel_power_off(void *ctx_dev)
{
	int rc = -EINVAL;
	struct dsi_panel_lcm *ctx = ctx_dev;

	if (!ctx) {
		OPLUS_DSI_ERR("Invalid ctx param\n");
		return rc;
	}
	if (ctx->prepared) {
		return 0;
	}
	if (ctx->power_info.refcount == 0) {
		OPLUS_DSI_WARN("power_info refcount is zero\n");
		rc = 0;
	} else {
		ctx->power_info.refcount--;
		if (ctx->power_info.refcount == 0) {
			rc = oplus_dsi_panel_power_supply_enable(ctx_dev, 0);
		} else {
			rc = 0;
		}
	}

	return rc;
}
EXPORT_SYMBOL(oplus_dsi_panel_power_off);

int oplus_dsi_panel_reset_keep(void *ctx_dev, int value)
{
	int rc = -EINVAL;
	struct dsi_panel_lcm *ctx = ctx_dev;

	if (!ctx) {
		OPLUS_DSI_ERR("Invalid ctx param\n");
		return rc;
	}
	gpiod_set_value(ctx->reset_gpio, value);
	rc = 0;

	return rc;
}
EXPORT_SYMBOL(oplus_dsi_panel_reset_keep);

int oplus_dsi_panel_reset(void *ctx_dev)
{
	int rc = -EINVAL;
	unsigned int sleep_ms = 0;
	struct dsi_panel_lcm *ctx = ctx_dev;

	if (!ctx) {
		OPLUS_DSI_ERR("Invalid ctx param\n");
		return rc;
	}
	if (ctx->prepared) {
		return 0;
	}
	OPLUS_DSI_INFO("do panel reset\n");
	if (ctx->panel_reset_sequence[0][1] > 0) {
		sleep_ms = ctx->panel_reset_sequence[0][1];
		usleep_range(sleep_ms*1000, (sleep_ms*1000)+100);
	}
	gpiod_set_value(ctx->reset_gpio, ctx->panel_reset_sequence[1][0]);
	if (ctx->panel_reset_sequence[1][1] > 0) {
		sleep_ms = ctx->panel_reset_sequence[1][1];
		usleep_range(sleep_ms*1000, (sleep_ms*1000)+100);
	}
	gpiod_set_value(ctx->reset_gpio, ctx->panel_reset_sequence[2][0]);
	if (ctx->panel_reset_sequence[2][1] > 0) {
		sleep_ms = ctx->panel_reset_sequence[2][1];
		usleep_range(sleep_ms*1000, (sleep_ms*1000)+100);
	}
	gpiod_set_value(ctx->reset_gpio, ctx->panel_reset_sequence[3][0]);
	if (ctx->panel_reset_sequence[3][1] > 0) {
		sleep_ms = ctx->panel_reset_sequence[3][1];
		usleep_range(sleep_ms*1000, (sleep_ms*1000)+100);
	}
	rc = 0;

	return rc;
}
EXPORT_SYMBOL(oplus_dsi_panel_reset);

int oplus_dsi_panel_prepare(void *ctx_dev)
{
	int rc = -EINVAL, reset_position = 0;
	struct dsi_panel_lcm *ctx = ctx_dev;

	if (!ctx) {
		OPLUS_DSI_ERR("Invalid ctx param\n");
		return rc;
	}
	if ((ctx->panel_reset_sequence[0][0] == PANEL_RESET_POSITION0)
		||(ctx->panel_reset_sequence[0][0] == PANEL_RESET_POSITION1)) {
		reset_position = PANEL_RESET_POSITION3;
	} else if (ctx->panel_reset_sequence[0][0] == PANEL_RESET_POSITION2) {
		if (ctx->power_info.refcount == 0) {
			reset_position = PANEL_RESET_POSITION1;
		} else {
			reset_position = PANEL_RESET_POSITION2;
		}
	} else {
		reset_position = PANEL_RESET_POSITION3;
	}
	if (reset_position & PANEL_RESET_POSITION1) {
		rc = oplus_dsi_panel_power_on(ctx_dev);
		if (rc < 0) {
			OPLUS_DSI_ERR("[%s]oplus_dsi_panel_power_on error\n", ctx->panel_info.panel_name);
			return rc;
		}
	}
	if (reset_position & PANEL_RESET_POSITION2) {
		rc = oplus_dsi_panel_reset(ctx_dev);
		if (rc < 0) {
			OPLUS_DSI_ERR("[%s]oplus_dsi_panel_reset error\n", ctx->panel_info.panel_name);
			return rc;
		}
	}

	return rc;
}
EXPORT_SYMBOL(oplus_dsi_panel_prepare);

MODULE_AUTHOR("Xiaolei Gao");
MODULE_DESCRIPTION("OPPO panel power");
MODULE_LICENSE("GPL v2");

