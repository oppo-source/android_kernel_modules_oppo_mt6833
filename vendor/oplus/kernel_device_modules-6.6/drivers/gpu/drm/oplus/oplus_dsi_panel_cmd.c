/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_dsi_panel_cmd.c
** Description : oplus dsi panel cmd
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

#include "oplus_dsi_display_config.h"
#include "oplus_display_debug.h"
#include "oplus_display_sysfs_attrs.h"

static DEFINE_MUTEX(dsi_cmd_send_lock);

const char *dsi_cmd_map[DSI_CMD_ID_MAX] = {
	"dsi_cmd_debug",
	"oplus,dsi-on-command",
	"oplus,dsi-on-pre-command",
	"oplus,dsi-on-post-command",
	"oplus,dsi-off-command",
	"oplus,dsi-hbm-on-command",
	"oplus,dsi-hbm-off-command",
	"oplus,dsi-timing-switch-command",
	"oplus,dsi-timing-switch-1pulse-command",
	"oplus,dsi-ultra-low-power-aod-off-command",
	"oplus,dsi-ultra-low-power-aod-on-command",
	"oplus,dsi-aod-off-command",
	"oplus,dsi-aod-off-compensation-command",
	"oplus,dsi-adfr-min-fps-120hz-command",
	"oplus,dsi-adfr-min-fps-90hz-command",
	"oplus,dsi-adfr-min-fps-60hz-command",
	"oplus,dsi-adfr-min-fps-45hz-command",
	"oplus,dsi-adfr-min-fps-30hz-command",
	"oplus,dsi-adfr-min-fps-20hz-command",
	"oplus,dsi-adfr-min-fps-15hz-command",
	"oplus,dsi-adfr-min-fps-10hz-command",
	"oplus,dsi-adfr-min-fps-5hz-command",
	"oplus,dsi-adfr-min-fps-1hz-command",
	"oplus,dsi-power-on-1pul-command",
	"oplus,dsi-lhbm-pressed-icon-gamma-command",
	"oplus,dsi-lhbm-pressed-icon-grayscale-command",
	"oplus,dsi-lhbm-pressed-icon-on-command",
	"oplus,dsi-lhbm-pressed-icon-off-command",
	"oplus,dsi-lhbm-pressed-icon-on-pwm-command",
	"oplus,dsi-aod-high-mode-command",
	"oplus,dsi-aod-low-mode-command",
	"oplus,dsi-aod-on-command",
	"oplus,dsi-set-backlight-command",
	"oplus,dsi-set-seed-expert-command",
	"oplus,dsi-set-seed-natural-command",
	"oplus,dsi-set-seed-vivid-command",
	"oplus,dsi-set-uir-on-seed-expert-command",
	"oplus,dsi-set-uir-on-seed-natural-command",
	"oplus,dsi-set-uir-on-seed-vivid-command",
	"oplus,dsi-set-uir-off-seed-expert-command",
	"oplus,dsi-set-uir-off-seed-natural-command",
	"oplus,dsi-set-uir-off-seed-vivid-command",
	"oplus,dsi-set-uir-seed-expert-command",
	"oplus,dsi-set-uir-seed-natural-command",
	"oplus,dsi-set-uir-seed-vivid-command",
	"oplus,dsi-switch-hbm-apl-off-command",
	"oplus,dsi-switch-hbm-apl-on-command",
	"oplus,dsi-multi-te-disable-command",
	"oplus,dsi-multi-te-enable-command",
	"oplus,dsi-pwm-switch-18to1pul-command",
	"oplus,dsi-pwm-switch-18to3pul-command",
	"oplus,dsi-pwm-switch-1to18pul-command",
	"oplus,dsi-pwm-switch-1to3pul-command",
	"oplus,dsi-pwm-switch-3to18pul-command",
	"oplus,dsi-pwm-switch-3to1pul-command",
	"oplus,dsi-panel-spr-on-command",
	"oplus,dsi-panel-spr-off-command",
	"oplus,dsi-panel-emduty-dbv-mode0-command",
	"oplus,dsi-panel-emduty-dbv-mode1-command",
	"oplus,dsi-panel-emduty-dbv-mode2-command",
	"oplus,dsi-panel-emduty-dbv-mode3-command",
	"oplus,dsi-panel-emduty-dbv-mode4-command",
	"oplus,dsi-panel-emduty-dbv-mode5-command",
	"oplus,dsi-panel-emduty-dbv-mode6-command",
	"oplus,dsi-panel-emduty-dbv-max-command",
	"oplus,dsi-panel-demura-dbv-mode0-command",
	"oplus,dsi-panel-demura-dbv-mode1-command",
	"oplus,dsi-panel-demura-dbv-mode2-command",
	"oplus,dsi-panel-demura-dbv-mode3-command",
	"oplus,dsi-panel-demura-dbv-mode4-command",
	"oplus,dsi-panel-demura-dbv-mode5-command",
	"oplus,dsi-panel-demura-dbv-mode6-command",
	"oplus,dsi-panel-demura-dbv-max-command",
	"oplus,dsi-panel-compensation-command",
	"oplus,dsi-panel-gamma-compensation-page0-command",
	"oplus,dsi-panel-gamma-compensation-page1-command",
	"oplus,dsi-panel-gamma-compensation-page2-command",
	"oplus,dsi-panel-gamma-compensation-command",
	"oplus,dsi-panel-mipi-err-check-enter-command",
	"oplus,dsi-panel-mipi-err-check-exit-command",
	"oplus,dsi-panel-crc-check-enter-command",
	"oplus,dsi-panel-crc-check-exit-command",
};
EXPORT_SYMBOL(dsi_cmd_map);

const char *dsi_cmd_state_map[DSI_CMD_ID_MAX] = {
	"dsi_cmd_debug-state",
	"oplus,dsi-on-command-state",
	"oplus,dsi-on-pre-command-state",
	"oplus,dsi-on-post-command-state",
	"oplus,dsi-off-command-state",
	"oplus,dsi-hbm-on-command-state",
	"oplus,dsi-hbm-off-command-state",
	"oplus,dsi-timing-switch-command-state",
	"oplus,dsi-timing-switch-1pulse-command-state",
	"oplus,dsi-ultra-low-power-aod-off-command-state",
	"oplus,dsi-ultra-low-power-aod-on-command-state",
	"oplus,dsi-aod-off-command-state",
	"oplus,dsi-aod-off-compensation-command-state",
	"oplus,dsi-adfr-min-fps-120hz-command-state",
	"oplus,dsi-adfr-min-fps-90hz-command-state",
	"oplus,dsi-adfr-min-fps-60hz-command-state",
	"oplus,dsi-adfr-min-fps-45hz-command-state",
	"oplus,dsi-adfr-min-fps-30hz-command-state",
	"oplus,dsi-adfr-min-fps-20hz-command-state",
	"oplus,dsi-adfr-min-fps-15hz-command-state",
	"oplus,dsi-adfr-min-fps-10hz-command-state",
	"oplus,dsi-adfr-min-fps-5hz-command-state",
	"oplus,dsi-adfr-min-fps-1hz-command-state",
	"oplus,dsi-power-on-1pul-command-state",
	"oplus,dsi-lhbm-pressed-icon-gamma-command-state",
	"oplus,dsi-lhbm-pressed-icon-grayscale-command-state",
	"oplus,dsi-lhbm-pressed-icon-on-command-state",
	"oplus,dsi-lhbm-pressed-icon-off-command-state",
	"oplus,dsi-lhbm-pressed-icon-on-pwm-command-state",
	"oplus,dsi-aod-high-mode-command-state",
	"oplus,dsi-aod-low-mode-command-state",
	"oplus,dsi-aod-on-command-state",
	"oplus,dsi-set-backlight-command-state",
	"oplus,dsi-set-seed-expert-command-state",
	"oplus,dsi-set-seed-natural-command-state",
	"oplus,dsi-set-seed-vivid-command-state",
	"oplus,dsi-set-uir-on-seed-expert-command-state",
	"oplus,dsi-set-uir-on-seed-natural-command-state",
	"oplus,dsi-set-uir-on-seed-vivid-command-state",
	"oplus,dsi-set-uir-off-seed-expert-command-state",
	"oplus,dsi-set-uir-off-seed-natural-command-state",
	"oplus,dsi-set-uir-off-seed-vivid-command-state",
	"oplus,dsi-set-uir-seed-expert-command-state",
	"oplus,dsi-set-uir-seed-natural-command-state",
	"oplus,dsi-set-uir-seed-vivid-command-state",
	"oplus,dsi-switch-hbm-apl-off-command-state",
	"oplus,dsi-switch-hbm-apl-on-command-state",
	"oplus,dsi-multi-te-disable-command-state",
	"oplus,dsi-multi-te-enable-command-state",
	"oplus,dsi-pwm-switch-18to1pul-command-state",
	"oplus,dsi-pwm-switch-18to3pul-command-state",
	"oplus,dsi-pwm-switch-1to18pul-command-state",
	"oplus,dsi-pwm-switch-1to3pul-command-state",
	"oplus,dsi-pwm-switch-3to18pul-command-state",
	"oplus,dsi-pwm-switch-3to1pul-command-state",
	"oplus,dsi-panel-spr-on-command-state",
	"oplus,dsi-panel-spr-off-command-state",
	"oplus,dsi-panel-emduty-dbv-mode0-command-state",
	"oplus,dsi-panel-emduty-dbv-mode1-command-state",
	"oplus,dsi-panel-emduty-dbv-mode2-command-state",
	"oplus,dsi-panel-emduty-dbv-mode3-command-state",
	"oplus,dsi-panel-emduty-dbv-mode4-command-state",
	"oplus,dsi-panel-emduty-dbv-mode5-command-state",
	"oplus,dsi-panel-emduty-dbv-mode6-command-state",
	"oplus,dsi-panel-emduty-dbv-max-command-state",
	"oplus,dsi-panel-demura-dbv-mode0-command-state",
	"oplus,dsi-panel-demura-dbv-mode1-command-state",
	"oplus,dsi-panel-demura-dbv-mode2-command-state",
	"oplus,dsi-panel-demura-dbv-mode3-command-state",
	"oplus,dsi-panel-demura-dbv-mode4-command-state",
	"oplus,dsi-panel-demura-dbv-mode5-command-state",
	"oplus,dsi-panel-demura-dbv-mode6-command-state",
	"oplus,dsi-panel-demura-dbv-max-command-state",
	"oplus,dsi-panel-compensation-command-state",
	"oplus,dsi-panel-gamma-compensation-page0-command-state",
	"oplus,dsi-panel-gamma-compensation-page1-command-state",
	"oplus,dsi-panel-gamma-compensation-page2-command-state",
	"oplus,dsi-panel-gamma-compensation-command-state",
	"oplus,dsi-panel-mipi-err-check-enter-command-state",
	"oplus,dsi-panel-mipi-err-check-exit-command-state",
	"oplus,dsi-panel-crc-check-enter-command-state",
	"oplus,dsi-panel-crc-check-exit-command-state",
};
EXPORT_SYMBOL(dsi_cmd_state_map);

volatile unsigned int dsi_panel_mode_id = FHD_SDC120;
EXPORT_SYMBOL(dsi_panel_mode_id);
struct dsi_cmd_sets **lcm_timings_cmds;
EXPORT_SYMBOL(lcm_timings_cmds);
unsigned char framerates[OPLUS_PANEL_TIMINGS_MAX] = {0};

struct mtk_ddic_dsi_cmd send_cmd_to_ddic = {};
unsigned char ddic_para_list[MAX_TX_CMD_NUM_PACK][OPLUS_SEND_CMD_MAX] = {0};

extern atomic_t oplus_pcp_handle_lock;

extern void oplus_pcp_handle(bool cmd_is_pcp,  void *handle);

bool oplus_dsi_panel_is_pcp(enum dsi_cmd_id cmd_set_id)
{
	unsigned int i;
	struct dsi_cmd_table *tb = NULL;
	unsigned int lcm_cmd_count = 0;
	unsigned int mode_id = dsi_panel_mode_id;

	if (cmd_set_id >= DSI_CMD_ID_MAX) {
		OPLUS_DSI_ERR("cmd_id is error\n");
		return false;
	}

	if (mode_id == MODE_ID_NONE) {
		tb = lcm_all_cmd_table[cmd_set_id].para_table;
		lcm_cmd_count = lcm_all_cmd_table[cmd_set_id].cmd_lines;
	} else {
		tb = lcm_cur_cmd_table(mode_id)[cmd_set_id].para_table;
		lcm_cmd_count = lcm_cur_cmd_table(mode_id)[cmd_set_id].cmd_lines;
	}

	for (i = 0; i < lcm_cmd_count; i++) {
		if (tb[i].count == 2) {
			if (tb[i].para_list[0] == 0x88 && tb[i].para_list[1] == 0x78) {
				if (cmd_set_id >= DSI_CMD_SET_SEED_EXPERT && cmd_set_id <= DSI_CMD_SET_UIR_SEED_VIVID) {
					OPLUS_DSI_INFO("FEATURE_UIR: cmd[%d] skip marker as cmdq cmds\n", cmd_set_id);
					return false;
				}
				return true;
			}
		}
	}

	return false;
}
EXPORT_SYMBOL(oplus_dsi_panel_is_pcp);

void oplus_dsi_panel_dcs_write(void *dsi, const void *data, size_t len)
{
	ssize_t ret;
	char *addr;
	struct dsi_panel_lcm *ctx = (struct dsi_panel_lcm*)(((struct mipi_dsi_device*)dsi)->dev.driver_data);

	if (ctx) {
		 if (ctx->error < 0) {
			OPLUS_DSI_ERR("ctx->error is error %d\n", ctx->error);
			return;
		 }
	} else {
		OPLUS_DSI_ERR("ctx is NULL\n");
		return;
	}

	addr = (char *)data;

	if ((int)*addr < 0xB0)
		ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
	else
		ret = mipi_dsi_generic_write(dsi, data, len);

	if (ret < 0) {
		dev_info(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
}
EXPORT_SYMBOL(oplus_dsi_panel_dcs_write);

static void oplus_dsi_panel_print_cmd_name(enum dsi_cmd_id cmd_set_id)
{
	switch (cmd_set_id) {
	case DSI_CMD_SET_BACKLIGHT:
	case DSI_CMD_ADFR_MIN_FPS_120HZ:
	case DSI_CMD_ADFR_MIN_FPS_90HZ:
	case DSI_CMD_ADFR_MIN_FPS_60HZ:
	case DSI_CMD_ADFR_MIN_FPS_45HZ:
	case DSI_CMD_ADFR_MIN_FPS_30HZ:
	case DSI_CMD_ADFR_MIN_FPS_20HZ:
	case DSI_CMD_ADFR_MIN_FPS_15HZ:
	case DSI_CMD_ADFR_MIN_FPS_10HZ:
	case DSI_CMD_ADFR_MIN_FPS_5HZ:
	case DSI_CMD_ADFR_MIN_FPS_1HZ:
	case DSI_CMD_LHBM_PRESSED_ICON_GRAYSCALE:
		OPLUS_DSI_DEBUG_DCS("mode[%d]-dsi_cmd[%d]: %s\n",
				dsi_panel_mode_id, cmd_set_id, dsi_cmd_map[cmd_set_id]);
		break;
	default:
		OPLUS_DSI_INFO("mode[%d]-dsi_cmd[%d]: %s\n",
				dsi_panel_mode_id, cmd_set_id, dsi_cmd_map[cmd_set_id]);
		break;
	}
}

void oplus_dsi_panel_cmd_line_joint(struct dsi_cmd_table *para_table, char *buf)
{
	int i, cnt = 0;

	if (para_table->count > OPLUS_CMD_PARAMS_MAX) {
		OPLUS_DSI_ERR("cmd params length %d exceed the max limit %d\n",
				para_table->count, OPLUS_CMD_PARAMS_MAX);
		return;
	}
	cnt += snprintf(buf + cnt, 4, "%02X ", para_table->cmd_msg.type);
	cnt += snprintf(buf + cnt, 4, "%02X ", para_table->cmd_msg.cmd1);
	cnt += snprintf(buf + cnt, 4, "%02X ", para_table->cmd_msg.channel);
	cnt += snprintf(buf + cnt, 4, "%02X ", para_table->cmd_msg.flags);
	cnt += snprintf(buf + cnt, 4, "%02X ", para_table->cmd_msg.cmd4);
	cnt += snprintf(buf + cnt, 4, "%02X ", para_table->count >> 8);
	cnt += snprintf(buf + cnt, 4, "%02X ", para_table->count & 0xFF);
	for (i = 0; i < para_table->count; i++) {
		if (cnt >= OPLUS_DSI_CMD_PRINT_BUF_SIZE - 4) {
			OPLUS_DSI_ERR("buf length %d will exceed the max limit %d\n",
					cnt, OPLUS_DSI_CMD_PRINT_BUF_SIZE);
			cnt += snprintf(buf + cnt, 9, "%s", "Overflow");
			return;
		}
		if ((i + 1) == para_table->count)
			cnt += snprintf(buf + cnt, 3, "%02X", para_table->para_list[i]);
		else
			cnt += snprintf(buf + cnt, 4, "%02X ", para_table->para_list[i]);
	}
}

void oplus_dsi_panel_print_cmd_lines(struct dsi_cmd_sets *cmd_sets, char *keyword)
{
	int i, cnt = 0;
	char buf[OPLUS_DSI_CMD_PRINT_BUF_SIZE] = {0};

	for (i = 0; i < cmd_sets->cmd_lines; i++) {
		cnt = 0;
		memset(buf, 0, sizeof(buf));
		cnt += snprintf(buf, strlen(keyword) + 1, "%s", keyword);
		oplus_dsi_panel_cmd_line_joint(&cmd_sets->para_table[i], buf + cnt);

		if (!strcmp(keyword, DISPLAY_TOOL_CMD_KEYWORD))
			OPLUS_DSI_INFO("%s\n", buf);
		else
			OPLUS_DSI_DEBUG_DCS("%s\n", buf);
	}
}

static int oplus_dsi_panel_send_cmd_pre(void *dsi, enum dsi_cmd_id cmd_set_id, enum dsi_cmd_set_state cmd_state)
{
	oplus_dsi_panel_print_cmd_name(cmd_set_id);

	send_cmd_to_ddic.cmd_count = 0;
	send_cmd_to_ddic.is_hs = 0;
	send_cmd_to_ddic.is_package = 0;
	memset(send_cmd_to_ddic.mtk_ddic_cmd_table, 0, sizeof(send_cmd_to_ddic.mtk_ddic_cmd_table));
	memset(ddic_para_list, 0, sizeof(ddic_para_list));

	if (cmd_state == DSI_CMD_SET_STATE_HS) {
		send_cmd_to_ddic.is_hs = 1;
		send_cmd_to_ddic.is_package = 1;
	}

	return 0;
}

static int oplus_dsi_panel_send_cmd_line_pre(void *dsi,
		void *handle, enum dsi_cmd_id cmd_set_id, unsigned int index)
{
	struct dsi_cmd_table *table = lcm_all_cmd_table[cmd_set_id].para_table;
	char buf[OPLUS_DSI_CMD_PRINT_BUF_SIZE] = {0};

	oplus_dsi_panel_cmd_line_joint(&table[index], buf);
	OPLUS_DSI_DEBUG_DCS("dsi_cmd[%d][%d]: %s\n", cmd_set_id, index, buf);

	return 0;
}

static int oplus_dsi_panel_send_cmd_sub(void *dsi, struct dsi_cmd_table *table,
	struct mtk_ddic_cmd *ddic_cmd, void *handle, int line, enum dsi_cmd_func cmd_func)
{
	ddic_cmd->cmd_num = table->count;
	ddic_cmd->para_list = ddic_para_list[line];
	memcpy(ddic_cmd->para_list, table->para_list, table->count);
	if (send_cmd_to_ddic.is_package) {
		if (table->last_command) {
			oplus_dsi_display_get_dcs_pack_gce()(dsi, handle, &send_cmd_to_ddic);
		} else {
			return 0;
		}
	} else {
		if (cmd_func == DSI_CMD_FUNC_GCE2)
			oplus_dsi_display_get_dcs_write_gce2()(dsi, handle, ddic_cmd->para_list, ddic_cmd->cmd_num);
		else
			oplus_dsi_display_get_dcs_write_gce()(dsi, handle, ddic_cmd->para_list, ddic_cmd->cmd_num);
	}

	memset(ddic_para_list, 0, sizeof(ddic_para_list));
	memset(send_cmd_to_ddic.mtk_ddic_cmd_table, 0, sizeof(send_cmd_to_ddic.mtk_ddic_cmd_table));

	return 0;
}

int oplus_dsi_panel_send_cmd(void *dsi, enum dsi_cmd_id cmd_set_id,
		void *handle, enum dsi_cmd_func cmd_func)
{
	int i, rc = 0;
	enum dsi_cmd_set_state cmd_state;
	struct dsi_cmd_table *table = NULL;
	struct mtk_ddic_cmd *ddic_cmd = send_cmd_to_ddic.mtk_ddic_cmd_table;
	struct dsi_cmd_sets *lcm_timing_cmds;
	unsigned int count, batch_count = 0;
	bool is_pcp = false;
	struct mtk_dsi *mtk_dsi;
	struct drm_crtc *crtc = NULL;
	struct mtk_crtc_state *mtk_state = NULL;

	if (cmd_set_id >= DSI_CMD_ID_MAX) {
		OPLUS_DSI_ERR("cmd_id:%d is greater than DSI_CMD_ID_MAX:%d\n",
				cmd_set_id, DSI_CMD_ID_MAX);
		rc = -EINVAL;
		return rc;
	}

	if (!dsi) {
		OPLUS_DSI_ERR("dsi is null\n");
		rc = -EINVAL;
		return rc;
	}

	lcm_timing_cmds = lcm_all_cmd_table;

	table = lcm_timing_cmds[cmd_set_id].para_table;
	count = lcm_timing_cmds[cmd_set_id].cmd_lines;
	cmd_state = lcm_timing_cmds[cmd_set_id].state;

	if (count == 0) {
		OPLUS_DSI_WARN("mode[%d]-dsi_cmd[%d]: %s is null\n",
				dsi_panel_mode_id, cmd_set_id, dsi_cmd_map[cmd_set_id]);
		rc = -EINVAL;
		return rc;
	}

	if (!table) {
		OPLUS_DSI_ERR("table is NULL\n");
		rc = -EINVAL;
		return rc;
	}

	/* TODO: cmd_state switch */
	if (cmd_func) {
		switch (cmd_func) {
		case DSI_CMD_FUNC_HOST:
			cmd_state = DSI_CMD_SET_STATE_LP;
			break;
		case DSI_CMD_FUNC_PACK_GCE:
			cmd_state = DSI_CMD_SET_STATE_HS;
			break;
		case DSI_CMD_FUNC_GCE:
		case DSI_CMD_FUNC_GCE2:
			cmd_state = DSI_CMD_SET_STATE_LP_GCE;
			break;
		default:
			break;
		}
	}

	if (cmd_state != DSI_CMD_SET_STATE_LP) {
		mtk_dsi = dsi;
		if(mtk_dsi->ddp_comp.mtk_crtc) {
			crtc = &(mtk_dsi->ddp_comp.mtk_crtc->base);
			if (!crtc) {
				OPLUS_DSI_ERR("Invalid drm crtc\n");
				rc = -EINVAL;
				return rc;
			} else {
				if (!crtc->state) {
					OPLUS_DSI_ERR("Invalid crtc state\n");
					rc = -EINVAL;
					return rc;
				} else {
					mtk_state = to_mtk_crtc_state(crtc->state);
					if (!mtk_state) {
						OPLUS_DSI_ERR("Invalid mtk_state param\n");
						rc = -EINVAL;
						return rc;
					}
				}
			}
		} else {
			OPLUS_DSI_ERR("Invalid mtk_crtc\n");
			rc = -EINVAL;
			return rc;
		}
	}

	mutex_lock(&dsi_cmd_send_lock);

	/* Pre-interface for send cmd */
	oplus_dsi_panel_send_cmd_pre(dsi, cmd_set_id, cmd_state);
	/*
	is_pcp = oplus_dsi_panel_is_pcp(cmd_set_id);
	oplus_pcp_handle(is_pcp, handle);
	*/

	for (i = 0; i < count; i++) {
		/* Pre-interface for send cmd line */
		oplus_dsi_panel_send_cmd_line_pre(dsi, handle, cmd_set_id, i);

		send_cmd_to_ddic.cmd_count++;
		if (cmd_state == DSI_CMD_SET_STATE_HS) {
			if (table[i].cmd_msg.flags & MIPI_DSI_MSG_BATCH_COMMAND) {
				batch_count++;
				table[i].last_command = false;
			} else {
				batch_count = 0;
				table[i].last_command = true;
			}
			if ((count == 1) || (count == (i + 1))
					|| (batch_count >= MAX_TX_CMD_NUM_PACK)) {
				batch_count = 0;
				table[i].last_command = true;
			}
			oplus_dsi_panel_send_cmd_sub(dsi, &table[i], &ddic_cmd[i], handle, i, cmd_func);
		} else if (cmd_state == DSI_CMD_SET_STATE_LP) {
			oplus_dsi_panel_dcs_write(dsi, table[i].para_list, table[i].count);
		} else if (cmd_state == DSI_CMD_SET_STATE_LP_GCE) {
			oplus_dsi_panel_send_cmd_sub(dsi, &table[i], &ddic_cmd[0], handle, 0, cmd_func);
		}
		/* DCS delay */
		if (table[i].post_wait_ms) {
			if (handle && (cmd_state != DSI_CMD_SET_STATE_LP)) {
				cmdq_pkt_sleep(handle, CMDQ_US_TO_TICK(table[i].post_wait_ms * 1000), CMDQ_GPR_R14);
			} else {
				usleep_range(table[i].post_wait_ms * 1000, table[i].post_wait_ms * 1000 + 100);
			}
		}
		/* send cmd kickoff done */
		if (!batch_count) {
			OPLUS_DSI_DEBUG_DCS("dsi_cmd[%d][%d] kickoff done, cmd_func=%d, is_hs=%d, is_package=%d, cmd_line=%d\n",
					cmd_set_id, i, cmd_func,
					send_cmd_to_ddic.is_hs, send_cmd_to_ddic.is_package, send_cmd_to_ddic.cmd_count);
			send_cmd_to_ddic.cmd_count = 0;
		}
	}

	mutex_unlock(&dsi_cmd_send_lock);

	if (is_pcp) {
		atomic_inc(&oplus_pcp_handle_lock);
	}

	return rc;
}
EXPORT_SYMBOL(oplus_dsi_panel_send_cmd);


static void oplus_dsi_panel_parse_cmd_sub(struct device_node *timing, unsigned int cmd_set_id, struct dsi_cmd_sets *table)
{
	const void *rc = NULL;
	unsigned char *lcm_cmd_params = NULL;
	int i = 0, j = 0, count = 0, ret =  0;
	int lcm_cmd_params_len = 0;
	const char *cmd_state = NULL;
	const char *lcm_cmd_name = NULL;
	const char *lcm_cmd_state_name = NULL;

	if ((cmd_set_id >= (sizeof(dsi_cmd_map)/sizeof(dsi_cmd_map[0])))
		|| (cmd_set_id >= (sizeof(dsi_cmd_state_map)/sizeof(dsi_cmd_state_map[0])))) {
		OPLUS_DSI_ERR("cmd_set_id %d is out of range\n", cmd_set_id);
		return;
	}

	if (!table) {
		OPLUS_DSI_ERR("table %d is NULL\n", cmd_set_id);
		return;
	}

	lcm_cmd_name = dsi_cmd_map[cmd_set_id];
	lcm_cmd_state_name = dsi_cmd_state_map[cmd_set_id];

	OPLUS_DSI_DEBUG_DCS("[%s:%s] dsi_cmd[%d] state_name: %s\n",
			timing->full_name, lcm_cmd_name, cmd_set_id, lcm_cmd_state_name);

	/* parse dsi_cmd params */
	rc = of_get_property(timing, lcm_cmd_name, &lcm_cmd_params_len);
	if (!rc) {
		OPLUS_DSI_DEBUG_DCS("[%s:%s] failed to get lcm_cmd_params_len\n",
				timing->full_name, lcm_cmd_name);
		return;
	}

	if (lcm_cmd_params_len > 0 && lcm_cmd_params_len <= CMD_SET_MIN_SIZE) {
		OPLUS_DSI_ERR("[%s:%s] invalid cmd len=%d\n",
				timing->full_name, lcm_cmd_name, lcm_cmd_params_len);
		return;
	}

	lcm_cmd_params = kzalloc(lcm_cmd_params_len, GFP_KERNEL);
	if (!lcm_cmd_params) {
		OPLUS_DSI_ERR("[%s:%s] lcm_cmd_params is NULL\n",
				timing->full_name, lcm_cmd_name);
		return;
	}
	memcpy(lcm_cmd_params, rc, lcm_cmd_params_len);
	for (i = 0, j = 0; i < lcm_cmd_params_len; j++) {
		count = ((lcm_cmd_params[i + 5] << 8) | (lcm_cmd_params[i + 6]));
		i += (CMD_SET_MIN_SIZE + count);
	}

	/* parse dsi_cmd state */
	ret = of_property_read_string(timing, lcm_cmd_state_name, &cmd_state);
	if (ret) {
		OPLUS_DSI_WARN("[%s:%s] failed to get cmd_state\n",
				timing->full_name, lcm_cmd_state_name);
	}
	if (!strcmp(cmd_state, "dsi_lp_mode")) {
		table->state = DSI_CMD_SET_STATE_LP;
	} else if (!strcmp(cmd_state, "dsi_ls_mode")) {
		table->state = DSI_CMD_SET_STATE_LP_GCE;
	} else if (!strcmp(cmd_state, "dsi_hs_mode")) {
		table->state = DSI_CMD_SET_STATE_HS;
	} else {
		OPLUS_DSI_ERR("[%s:%s] invalid command state: %s, default set to dsi_hs_mode\n",
			timing->full_name, dsi_cmd_state_map[cmd_set_id], cmd_state);
		table->state = DSI_CMD_SET_STATE_HS;
	}
	OPLUS_DSI_DEBUG_DCS("[%s:%s] cmd_state=%s[%d]\n",
			timing->full_name, lcm_cmd_name, cmd_state, table->state);

	/* check dsi_cmd params */
	table->cmd_lines = j;
	if (table->cmd_lines == 0) {
		OPLUS_DSI_ERR("[%s:%s] cmd_lines=%d, return\n",
				timing->full_name, lcm_cmd_name, table->cmd_lines);
		goto out;
	}
	table->para_table = kzalloc(sizeof(struct dsi_cmd_table) * table->cmd_lines, GFP_KERNEL);
	if (!table->para_table) {
		OPLUS_DSI_ERR("[%s:%s] table->para_table is NULL\n",
				timing->full_name, lcm_cmd_name);
		goto out;
	}
	for (i = 0, j = 0; j < table->cmd_lines; j++) {
		table->para_table[j].cmd_msg.type = lcm_cmd_params[i++];
		table->para_table[j].cmd_msg.cmd1 = lcm_cmd_params[i++];
		table->para_table[j].cmd_msg.channel = lcm_cmd_params[i++];
		table->para_table[j].cmd_msg.flags = lcm_cmd_params[i++];
		table->para_table[j].post_wait_ms = table->para_table[j].cmd_msg.cmd4 = lcm_cmd_params[i++];
		table->para_table[j].count = ((lcm_cmd_params[i] << 8) | (lcm_cmd_params[i+1]));
		i += 2;
		if ((table->para_table[j].count > 0) && (table->para_table[j].count <= OPLUS_SEND_CMD_MAX)) {
			memcpy(table->para_table[j].para_list, lcm_cmd_params + i,
			table->para_table[j].count);
		} else {
			OPLUS_DSI_ERR("[%s:%s] params error, return\n",
					timing->full_name, lcm_cmd_name);
			goto error;
		}
		i += table->para_table[j].count;

		if (i > lcm_cmd_params_len) {
			OPLUS_DSI_ERR("[%s:%s] invalid params index:%d > len:%d, return\n",
					timing->full_name, lcm_cmd_name, i, lcm_cmd_params_len);
			goto error;
		}
	}

	/* parse success */
	OPLUS_DSI_DEBUG_DCS("[%s:%s] Success parse dsi_cmd[%d], state=%s[%d], cmd_lines=%d\n",
			timing->full_name, lcm_cmd_name, cmd_set_id,
			cmd_state, table->state, table->cmd_lines);
	goto out;

error:
	if (table->para_table != NULL) {
		kfree(table->para_table);
		table->para_table = NULL;
	}
	table->cmd_lines = 0;
out:
	if (!lcm_cmd_params) {
		kfree(lcm_cmd_params);
		lcm_cmd_params = NULL;
	}
}

inline int oplus_dsi_panel_get_cell_index(int mode_id)
{
	int i = 0;
	int index_value = 0;
	int flag = 0;

	switch (mode_id) {
	case FHD_SDC60:
		index_value = 60;
		break;
	case FHD_SDC90:
		index_value = 90;
		break;
	case FHD_SDC120:
		index_value = 120;
		break;
	case FHD_OPLUS120:
		index_value = 122;
		break;
	default:
		index_value = 120;
		break;
	}
	for (i = 0; i < sizeof(framerates); i++) {
		if (framerates[i] == index_value) {
			flag = 1;
			break;
		}
	}
	if (flag) {
		return i;
	} else {
		OPLUS_DSI_ERR("invalid cell_index for mode_id %d\n", mode_id);
		return 0;
	}
}
EXPORT_SYMBOL(oplus_dsi_panel_get_cell_index);

static inline int get_init_cell_index(int framerate, int adfr_timing_type)
{
	int i = 0;
	int index_value = framerate + adfr_timing_type;

	for (i = 0; i < sizeof(framerates); i++) {
		if (framerates[i] == index_value) {
			return i;
		} else if (!framerates[i]) {
			framerates[i] = index_value;
			return i;
		}
	}

	return i;
}

int oplus_dsi_panel_parse_cmd(struct device_node *node, void *ctx_dev)
{
	int rc = 0, i = 0, index_timings = 0;
	u32 cell_Index = 0, framerate = 120, adfr_timing_type = 2;
	struct device_node *timings_np, *child_np;
	struct dsi_panel_lcm *ctx = ctx_dev;

	if (!node) {
		OPLUS_DSI_ERR("no display timings_np nodes defined\n");
		rc = -EINVAL;
		return rc;
	}

	timings_np = node;

	if (!ctx->mode_num) {
		OPLUS_DSI_ERR("mode num is zero\n");
		rc = -EINVAL;
		return rc;
	}

	OPLUS_DSI_INFO("mode_num is %d\n", ctx->mode_num);

	lcm_timings_cmds = kzalloc(sizeof(struct dsi_cmd_sets*) * ctx->mode_num, GFP_KERNEL);
	if (!lcm_timings_cmds) {
		OPLUS_DSI_ERR("lcm_timings_cmds kzalloc failed\n");
		rc = -EINVAL;
		return rc;
	}

	for_each_child_of_node(timings_np, child_np) {
		if (strcmp("timing", child_np->name) != 0) {
			continue;
		}

		OPLUS_DSI_INFO("Start parsing %s params\n", child_np->full_name);
		rc = of_property_read_u32(child_np, "oplus,dsi-panel-framerate", &framerate);
		if (rc == 0) {
			OPLUS_DSI_INFO("framerate = %d\n", framerate);
			if (of_property_read_u32(child_np, "oplus,adfr-timing-type", &adfr_timing_type)) {
				adfr_timing_type = 0;
				OPLUS_DSI_WARN("adfr_timing_type parse failed, default set to %d\n", adfr_timing_type);
			} else {
				OPLUS_DSI_INFO("adfr_timing_type = %d\n", adfr_timing_type);
			}
			cell_Index = get_init_cell_index(framerate, adfr_timing_type);
		} else {
			cell_Index = ctx->mode_num;
			OPLUS_DSI_ERR("index_timings parse failed, default set to %d\n", cell_Index);
		}
		if (cell_Index >= ctx->mode_num) {
			continue;
		}
		if (lcm_timings_cmds[cell_Index]) {
			continue;
		} else {
			lcm_timings_cmds[cell_Index] = kzalloc(sizeof(struct dsi_cmd_sets) * DSI_CMD_ID_MAX, GFP_KERNEL);
			if (!lcm_timings_cmds[cell_Index]) {
				continue;
			}
		}
		oplus_dsi_panel_parse_timing_param(child_np, ctx, adfr_timing_type, framerate);
		for (i = 0; i < DSI_CMD_ID_MAX; i++) {
			oplus_dsi_panel_parse_cmd_sub(child_np, i, &lcm_timings_cmds[cell_Index][i]);
		}
		index_timings++;
	}
	if (!index_timings)  {
		kfree(lcm_timings_cmds);
		lcm_timings_cmds = NULL;
		OPLUS_DSI_ERR("index_timings is zero\n");
		rc = -EINVAL;
		return rc;
	}  else if (index_timings != ctx->mode_num) {
		OPLUS_DSI_ERR("index_timings %d but num_timings %d\n", index_timings, ctx->mode_num);
	}

	return rc;
}

