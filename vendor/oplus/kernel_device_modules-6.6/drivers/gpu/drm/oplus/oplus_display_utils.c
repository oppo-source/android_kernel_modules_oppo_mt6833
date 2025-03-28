/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_display_utils.c
** Description : oplus display utils
** Version : 1.0
** Date : 2024/07/26
** Author : Display
******************************************************************/
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include "mtk_panel_ext.h"
#include "mtk_debug.h"
#include "mtk_drm_crtc.h"
#include "mtk_dsi.h"

#include "oplus_display_utils.h"
#include "oplus_display_interface.h"
#include "oplus_dsi_panel_cmd.h"
#include "oplus_dsi_display_config.h"
#include "oplus_display_debug.h"
#include "oplus_display_apollo_brightness.h"

extern unsigned int last_backlight;
extern int mtk_mipi_dsi_read_gce(struct mtk_dsi *dsi,
		struct cmdq_pkt *handle,
		struct mtk_drm_crtc *mtk_crtc,
		struct mtk_ddic_dsi_msg *cmd_msg);

void oplus_ddic_dsi_send_cmd(unsigned int len, char *data)
{
	unsigned int i = 0, j = 0;
	struct mtk_ddic_dsi_msg *cmd_msg = vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	u8 tx[OPLUS_MIPI_TX_MAX_LEN] = {0};
	unsigned int cnt = 0;
	char buf[OPLUS_DSI_CMD_PRINT_BUF_SIZE] = {0};

	OPLUS_DSI_INFO("Start send cmd, len=%d\n", len);

	if (!cmd_msg) {
		OPLUS_DSI_ERR("cmd_msg is NULL\n");
		return;
	}
	if (len > OPLUS_MIPI_TX_MAX_LEN) {
		OPLUS_DSI_ERR("Invalid cmd len: %d\n", len);
		vfree(cmd_msg);
		return;
	}

	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	switch (len) {
	case 1:
		cmd_msg->type[0] = 0x05;
		break;
	case 2:
		cmd_msg->type[0] = 0x15;
		break;
	default:
		cmd_msg->type[0] = 0x39;
		break;
	}

	cmd_msg->channel = 0;
	cmd_msg->flags |= MIPI_DSI_MSG_USE_LPM;
	cmd_msg->tx_cmd_num = 1;
	for (i = 0; i < len; i++) {
		tx[i] = data[i];
	}
	cmd_msg->tx_buf[0] = tx;
	cmd_msg->tx_len[0] = len;

	for (i = 0; i < (int)cmd_msg->tx_cmd_num; i++) {
		cnt = 0;
		for (j = 0; j < (int)cmd_msg->tx_len[i]; j++) {
			cnt += snprintf(buf + cnt, 4, "%02X ",
					*(char *)(cmd_msg->tx_buf[i] + j));
		}
		OPLUS_DSI_INFO("cmd[%d/%d]: type=0x%02X, len=%d, tx_buf=[%s]\n",
				i, (int)cmd_msg->tx_cmd_num - 1,
				cmd_msg->type[i], (int)cmd_msg->tx_len[i], buf);
	}

	if (mtk_ddic_dsi_send_cmd(cmd_msg, true)) {
		OPLUS_DSI_ERR("send cmd error\n");
		goto done;
	}
done:
	vfree(cmd_msg);

	OPLUS_DSI_INFO("End\n");
}
EXPORT_SYMBOL(oplus_ddic_dsi_send_cmd);

void oplus_ddic_dsi_read_cmd(u8 reg, int len, char *data)
{
	unsigned int index = 0;
	unsigned int ret_len = 0;
	struct mtk_ddic_dsi_msg *cmd_msg = vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	u8 tx[OPLUS_MIPI_TX_MAX_LEN] = {0};
	unsigned int cnt = 0;
	char print_buf[OPLUS_DSI_CMD_PRINT_BUF_SIZE] = {0};

	OPLUS_DSI_INFO("Start read reg=0x%02X, len=%d\n", reg, len);

	if (!cmd_msg) {
		OPLUS_DSI_ERR("cmd_msg is NULL\n");
		return;
	}
	if (len > OPLUS_MIPI_RX_MAX_LEN) {
		OPLUS_DSI_ERR("Invalid reg len: %d\n", len);
		vfree(cmd_msg);
		return;
	}

	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	cmd_msg->channel = 0;
	cmd_msg->tx_cmd_num = 1;
	cmd_msg->type[0] = 0x06;
	tx[0] = reg;
	cmd_msg->tx_buf[0] = tx;
	cmd_msg->tx_len[0] = 1;

	cmd_msg->rx_cmd_num = 1;
	cmd_msg->rx_buf[0] = vmalloc(OPLUS_MIPI_RX_MAX_LEN * sizeof(unsigned char));
	memset(cmd_msg->rx_buf[0], 0, OPLUS_MIPI_RX_MAX_LEN);
	cmd_msg->rx_len[0] = len;

	if (mtk_ddic_dsi_read_cmd(cmd_msg)) {
		OPLUS_DSI_ERR("read reg error\n");
		goto done;
	}

	ret_len = cmd_msg->rx_len[0];
	memcpy(data, cmd_msg->rx_buf[0], ret_len);

	for (index = 0; index < ret_len; index++) {
		cnt += snprintf(print_buf + cnt, 4, "%02X ", data[index]);
	}
	OPLUS_DSI_INFO("read reg=0x%02X, ret_len=%d, rx_buf=[%s]", reg, ret_len, print_buf);

done:
	vfree(cmd_msg->rx_buf[0]);
	vfree(cmd_msg);

	OPLUS_DSI_INFO("End\n");
}
EXPORT_SYMBOL(oplus_ddic_dsi_read_cmd);

void oplus_ddic_dsi_read_cmd_without_lock(struct mtk_dsi *dsi, u8 reg, int len, char *data)
{
	unsigned int index = 0;
	unsigned int ret_len = 0;
	struct mtk_ddic_dsi_msg *cmd_msg = vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	u8 tx[OPLUS_MIPI_TX_MAX_LEN] = {0};
	unsigned int cnt = 0;
	char print_buf[OPLUS_DSI_CMD_PRINT_BUF_SIZE] = {0};

	OPLUS_DSI_INFO("Start read reg=0x%02X, len=%d\n", reg, len);

	if (!dsi) {
		OPLUS_DSI_ERR("dsi is NULL\n");
		return;
	}
	if (!(dsi->ddp_comp.mtk_crtc)) {
		OPLUS_DSI_ERR("mtk_crtc is NULL\n");
		return;
	}

	if (!cmd_msg) {
		OPLUS_DSI_ERR("cmd_msg is NULL\n");
		return;
	}
	if (len > OPLUS_MIPI_RX_MAX_LEN) {
		OPLUS_DSI_ERR("Invalid reg len: %d\n", len);
		vfree(cmd_msg);
		return;
	}

	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	cmd_msg->channel = 0;
	cmd_msg->tx_cmd_num = 1;
	cmd_msg->type[0] = 0x06;
	tx[0] = reg;
	cmd_msg->tx_buf[0] = tx;
	cmd_msg->tx_len[0] = 1;

	cmd_msg->rx_cmd_num = 1;
	cmd_msg->rx_buf[0] = vmalloc(OPLUS_MIPI_RX_MAX_LEN * sizeof(unsigned char));
	memset(cmd_msg->rx_buf[0], 0, OPLUS_MIPI_RX_MAX_LEN);
	cmd_msg->rx_len[0] = len;

	if (mtk_mipi_dsi_read_gce(dsi, NULL, dsi->ddp_comp.mtk_crtc, cmd_msg)) {
		OPLUS_DSI_ERR("read reg error\n");
		goto done;
	}

	ret_len = cmd_msg->rx_len[0];
	memcpy(data, cmd_msg->rx_buf[0], ret_len);

	for (index = 0; index < ret_len; index++) {
		cnt += snprintf(print_buf + cnt, 4, "%02X ", data[index]);
	}
	OPLUS_DSI_INFO("read reg=0x%02X, ret_len=%d, rx_buf=[%s]", reg, ret_len, print_buf);

done:
	vfree(cmd_msg->rx_buf[0]);
	vfree(cmd_msg);

	OPLUS_DSI_INFO("End\n");
}

#define GAMMA_COMPENSATION_READ_RETRY_MAX 5
#define GAMMA_COMPENSATION_READ_REG 0x82
#define GAMMA_COMPENSATION_READ_LENGTH 4
#define GAMMA_COMPENSATION_CMD_INDEX1 2
#define GAMMA_COMPENSATION_CMD_INDEX2 4
#define GAMMA_COMPENSATION_CMD_INDEX3 6
#define GAMMA_COMPENSATION_CMD_INDEX4 8
#define GAMMA_COMPENSATION_REG_INDEX1 2
#define GAMMA_COMPENSATION_REG_INDEX2 3
bool g_gamma_regs_read_done = false;
EXPORT_SYMBOL(g_gamma_regs_read_done);
static int oplus_panel_gamma_compensation(void *dsi)
{
	u32 retry_count = 0;
	u32 index = 0;
	u32 cnt = 0;
	char print_buf[OPLUS_DSI_CMD_PRINT_BUF_SIZE] = {0};
	char regs1[GAMMA_COMPENSATION_READ_LENGTH] = {0};
	char regs1_last[GAMMA_COMPENSATION_READ_LENGTH] = {0};
	char regs2[GAMMA_COMPENSATION_READ_LENGTH] = {0};
	char regs2_last[GAMMA_COMPENSATION_READ_LENGTH] = {0};
	const char reg_base[GAMMA_COMPENSATION_READ_LENGTH] = {0};
	static char page1_reg1 = 0;
	static char page1_reg2 = 0;
	static char page2_reg1 = 0;
	static char page2_reg2 = 0;
	struct dsi_cmd_table *cmd_table = NULL;
	struct dsi_panel_lcm *ctx = oplus_mtkDsi_to_panel(dsi);

	while (!g_gamma_regs_read_done && retry_count < GAMMA_COMPENSATION_READ_RETRY_MAX) {
		OPLUS_DSI_INFO("read gamma compensation regs, retry_count=%d\n", retry_count);
		memset(regs1, 0, GAMMA_COMPENSATION_READ_LENGTH);
		memset(regs2, 0, GAMMA_COMPENSATION_READ_LENGTH);
		memset(print_buf, 0, OPLUS_DSI_CMD_PRINT_BUF_SIZE);

		if (oplus_dsi_panel_send_cmd(dsi, DSI_CMD_GAMMA_COMPENSATION_PAGE1, NULL, DSI_CMD_FUNC_DEFAULT)) {
			OPLUS_DSI_ERR("send dsi_cmd DSI_CMD_GAMMA_COMPENSATION_PAGE1 filed, retry\n");
			retry_count++;
			continue;
		}
		oplus_ddic_dsi_read_cmd(GAMMA_COMPENSATION_READ_REG, GAMMA_COMPENSATION_READ_LENGTH, regs1);
		cnt = 0;
		memset(print_buf, 0, OPLUS_DSI_CMD_PRINT_BUF_SIZE);
		for (index = 0; index < GAMMA_COMPENSATION_READ_LENGTH; index++) {
			cnt += snprintf(print_buf + cnt, 4, "%02X ", regs1[index]);
		}
		OPLUS_DSI_INFO("read regs1 len=%d, buf=[%s]\n", GAMMA_COMPENSATION_READ_LENGTH, print_buf);

		if (oplus_dsi_panel_send_cmd(dsi, DSI_CMD_GAMMA_COMPENSATION_PAGE2, NULL, DSI_CMD_FUNC_DEFAULT)) {
			OPLUS_DSI_ERR("send dsi_cmd DSI_CMD_GAMMA_COMPENSATION_PAGE2 filed, retry\n");
			retry_count++;
			continue;
		}
		oplus_ddic_dsi_read_cmd(GAMMA_COMPENSATION_READ_REG, GAMMA_COMPENSATION_READ_LENGTH, regs2);
		cnt = 0;
		memset(print_buf, 0, OPLUS_DSI_CMD_PRINT_BUF_SIZE);
		for (index = 0; index < GAMMA_COMPENSATION_READ_LENGTH; index++) {
			cnt += snprintf(print_buf + cnt, 4, "%02X ", regs2[index]);
		}
		OPLUS_DSI_INFO("read regs2 len=%d, buf=[%s]\n", GAMMA_COMPENSATION_READ_LENGTH, print_buf);
		if (!memcmp(regs1, reg_base, sizeof(reg_base)) ||
				!memcmp(regs2, reg_base, sizeof(reg_base)) ||
				memcmp(regs1, regs1_last, sizeof(regs1_last)) ||
				memcmp(regs2, regs2_last, sizeof(regs2_last))) {
			OPLUS_DSI_WARN("gamma compensation regs is invalid, retry\n");
			memcpy(regs1_last, regs1, GAMMA_COMPENSATION_READ_LENGTH);
			memcpy(regs2_last, regs2, GAMMA_COMPENSATION_READ_LENGTH);
			retry_count++;
			continue;
		}

		page1_reg1 = ((((regs1[GAMMA_COMPENSATION_REG_INDEX1] << 8) | regs1[GAMMA_COMPENSATION_REG_INDEX2]) * 3U / 10U) >> 8) & 0xFF;
		page1_reg2 = (((regs1[GAMMA_COMPENSATION_REG_INDEX1] << 8) | regs1[GAMMA_COMPENSATION_REG_INDEX2]) * 3U / 10U) & 0xFF;
		page2_reg1 = ((((regs2[GAMMA_COMPENSATION_REG_INDEX1] << 8) | regs2[GAMMA_COMPENSATION_REG_INDEX2]) * 3U / 10U) >> 8) & 0xFF;
		page2_reg2 = (((regs2[GAMMA_COMPENSATION_REG_INDEX1] << 8) | regs2[GAMMA_COMPENSATION_REG_INDEX2]) * 3U / 10U) & 0xFF;

		g_gamma_regs_read_done = true;
		OPLUS_DSI_INFO("read success, set gamma compensation regs1=[%02X %02X], regs2=[%02X %02X]\n",
				page1_reg1, page1_reg2, page2_reg1, page2_reg2);
		break;
	}

	oplus_dsi_panel_send_cmd(dsi, DSI_CMD_GAMMA_COMPENSATION_PAGE0, NULL, DSI_CMD_FUNC_DEFAULT);
	if (!g_gamma_regs_read_done) {
		return -EFAULT;
	}

	for (index = 0; index < ctx->mode_num; index++) {
		cmd_table = lcm_cur_cmd_table(index)[DSI_CMD_GAMMA_COMPENSATION].para_table;
		if ((lcm_cur_cmd_table(index)[DSI_CMD_GAMMA_COMPENSATION].cmd_lines >= GAMMA_COMPENSATION_CMD_INDEX4) && \
				(cmd_table[GAMMA_COMPENSATION_CMD_INDEX1].count >= (GAMMA_COMPENSATION_REG_INDEX2 + 1)) && \
				(cmd_table[GAMMA_COMPENSATION_CMD_INDEX2].count >= (GAMMA_COMPENSATION_REG_INDEX2 + 1)) && \
				(cmd_table[GAMMA_COMPENSATION_CMD_INDEX3].count >= (GAMMA_COMPENSATION_REG_INDEX2 + 1)) && \
				(cmd_table[GAMMA_COMPENSATION_CMD_INDEX4].count >= (GAMMA_COMPENSATION_REG_INDEX2 + 1))) {
			cmd_table[GAMMA_COMPENSATION_CMD_INDEX1].para_list[GAMMA_COMPENSATION_REG_INDEX1 + 1] = page1_reg1;
			cmd_table[GAMMA_COMPENSATION_CMD_INDEX1].para_list[GAMMA_COMPENSATION_REG_INDEX2 + 1] = page1_reg2;
			cmd_table[GAMMA_COMPENSATION_CMD_INDEX2].para_list[GAMMA_COMPENSATION_REG_INDEX1 + 1] = page1_reg1;
			cmd_table[GAMMA_COMPENSATION_CMD_INDEX2].para_list[GAMMA_COMPENSATION_REG_INDEX2 + 1] = page1_reg2;
			cmd_table[GAMMA_COMPENSATION_CMD_INDEX3].para_list[GAMMA_COMPENSATION_REG_INDEX1 + 1] = page2_reg1;
			cmd_table[GAMMA_COMPENSATION_CMD_INDEX3].para_list[GAMMA_COMPENSATION_REG_INDEX2 + 1] = page2_reg2;
			cmd_table[GAMMA_COMPENSATION_CMD_INDEX4].para_list[GAMMA_COMPENSATION_REG_INDEX1 + 1] = page2_reg1;
			cmd_table[GAMMA_COMPENSATION_CMD_INDEX4].para_list[GAMMA_COMPENSATION_REG_INDEX2 + 1] = page2_reg2;
		} else {
			OPLUS_DSI_ERR("Invalid mode[%d]-dsi_cmd: DSI_CMD_GAMMA_COMPENSATION, return\n", index);
			g_gamma_regs_read_done = false;
			return -EINVAL;
		}
	}

	return 0;
}

char regs1[AC178_GAMMA_COMPENSATION_READ_LENGTH] = {0};
EXPORT_SYMBOL(regs1);
char regs2[AC178_GAMMA_COMPENSATION_READ_LENGTH] = {0};
EXPORT_SYMBOL(regs2);
char regs3[AC178_GAMMA_COMPENSATION_READ_LENGTH] = {0};
EXPORT_SYMBOL(regs3);
static int oplus_panel_ac178_gamma_compensation(void *dsi)
{
	u32 retry_count = 0;
	u32 index = 0;
	u32 cnt = 0;
	char print_buf[OPLUS_DSI_CMD_PRINT_BUF_SIZE] = {0};
	char regs1_last[AC178_GAMMA_COMPENSATION_READ_LENGTH] = {0};
	char regs2_last[AC178_GAMMA_COMPENSATION_READ_LENGTH] = {0};
	char regs3_last[AC178_GAMMA_COMPENSATION_READ_LENGTH] = {0};
	const char reg_base[AC178_GAMMA_COMPENSATION_READ_LENGTH] = {0};

	while (!g_gamma_regs_read_done && retry_count < AC178_GAMMA_COMPENSATION_READ_RETRY_MAX) {
		OPLUS_DSI_INFO("read gamma compensation regs, retry_count=%d\n", retry_count);
		memset(regs1, 0, AC178_GAMMA_COMPENSATION_READ_LENGTH);
		memset(regs2, 0, AC178_GAMMA_COMPENSATION_READ_LENGTH);
		memset(print_buf, 0, OPLUS_DSI_CMD_PRINT_BUF_SIZE);

		if (oplus_dsi_panel_send_cmd(dsi, DSI_CMD_GAMMA_COMPENSATION_PAGE2, NULL, DSI_CMD_FUNC_DEFAULT)) {
			OPLUS_DSI_ERR("send dsi_cmd DSI_CMD_GAMMA_COMPENSATION_PAGE2 filed, retry\n");
			retry_count++;
			continue;
		}
		oplus_ddic_dsi_read_cmd(AC178_GAMMA_COMPENSATION_READ_REG1, AC178_GAMMA_COMPENSATION_READ_LENGTH, regs1);
		cnt = 0;
		memset(print_buf, 0, OPLUS_DSI_CMD_PRINT_BUF_SIZE);
		for (index = 0; index < AC178_GAMMA_COMPENSATION_READ_LENGTH; index++) {
			cnt += snprintf(print_buf + cnt, 4, "%02X ", regs1[index]);
		}
		OPLUS_DSI_INFO("read regs1 len=%d, buf=[%s]\n", AC178_GAMMA_COMPENSATION_READ_LENGTH, print_buf);

		if (oplus_dsi_panel_send_cmd(dsi, DSI_CMD_GAMMA_COMPENSATION_PAGE2, NULL, DSI_CMD_FUNC_DEFAULT)) {
			OPLUS_DSI_ERR("send dsi_cmd DSI_CMD_GAMMA_COMPENSATION_PAGE2 filed, retry\n");
			retry_count++;
			continue;
		}
		oplus_ddic_dsi_read_cmd(AC178_GAMMA_COMPENSATION_READ_REG2, AC178_GAMMA_COMPENSATION_READ_LENGTH, regs2);
		cnt = 0;
		memset(print_buf, 0, OPLUS_DSI_CMD_PRINT_BUF_SIZE);
		for (index = 0; index < AC178_GAMMA_COMPENSATION_READ_LENGTH; index++) {
			cnt += snprintf(print_buf + cnt, 4, "%02X ", regs2[index]);
		}
		OPLUS_DSI_INFO("read regs2 len=%d, buf=[%s]\n", AC178_GAMMA_COMPENSATION_READ_LENGTH, print_buf);

		if (oplus_dsi_panel_send_cmd(dsi, DSI_CMD_GAMMA_COMPENSATION_PAGE2, NULL, DSI_CMD_FUNC_DEFAULT)) {
			OPLUS_DSI_ERR("send dsi_cmd DSI_CMD_GAMMA_COMPENSATION_PAGE2 filed, retry\n");
			retry_count++;
			continue;
		}
		oplus_ddic_dsi_read_cmd(AC178_GAMMA_COMPENSATION_READ_REG3, AC178_GAMMA_COMPENSATION_READ_LENGTH, regs3);
		cnt = 0;
		memset(print_buf, 0, OPLUS_DSI_CMD_PRINT_BUF_SIZE);
		for (index = 0; index < AC178_GAMMA_COMPENSATION_READ_LENGTH; index++) {
			cnt += snprintf(print_buf + cnt, 4, "%02X ", regs3[index]);
		}
		OPLUS_DSI_INFO("read regs3 len=%d, buf=[%s]\n", AC178_GAMMA_COMPENSATION_READ_LENGTH, print_buf);
		if (!memcmp(regs1, reg_base, sizeof(reg_base)) ||
				!memcmp(regs2, reg_base, sizeof(reg_base)) ||
				!memcmp(regs3, reg_base, sizeof(reg_base)) ||
				memcmp(regs1, regs1_last, sizeof(regs1_last)) ||
				memcmp(regs2, regs2_last, sizeof(regs2_last)) ||
				memcmp(regs3, regs3_last, sizeof(regs3_last))) {
			OPLUS_DSI_WARN("gamma compensation regs is invalid, retry\n");
			memcpy(regs1_last, regs1, AC178_GAMMA_COMPENSATION_READ_LENGTH);
			memcpy(regs2_last, regs2, AC178_GAMMA_COMPENSATION_READ_LENGTH);
			memcpy(regs3_last, regs3, AC178_GAMMA_COMPENSATION_READ_LENGTH);
			retry_count++;
			continue;
		}
		g_gamma_regs_read_done = true;

		break;
	}

	if (!g_gamma_regs_read_done) {
		return -EFAULT;
	}

	return 0;
}

int oplus_panel_ext_init(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_ddp_comp *comp;
	struct mtk_dsi *dsi;
	struct dsi_panel_lcm *ctx = NULL;
	int rc = 0;

	if (!crtc) {
		OPLUS_DSI_ERR("find crtc fail\n");
		return -EFAULT;
	}

	mtk_crtc = to_mtk_crtc(crtc);
	if (mtk_crtc == NULL || crtc->state == NULL) {
		OPLUS_DSI_ERR("mtk_crtc or crtc->state is NULL\n");
		return -EFAULT;
	}
	if (!(mtk_crtc->enabled)) {
		OPLUS_DSI_ERR("crtc%d disable skip\n", drm_crtc_index(&mtk_crtc->base));
		return -EFAULT;
	} else if (mtk_crtc->ddp_mode == DDP_NO_USE) {
		OPLUS_DSI_ERR("skip ddp_mode: NO_USE\n");
		return -EFAULT;
	}

	comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (unlikely(!comp)) {
		OPLUS_DSI_ERR("invalid output comp\n");
		return -EFAULT;
	}
	dsi = container_of(comp, struct mtk_dsi, ddp_comp);
	if (!dsi) {
		OPLUS_DSI_ERR("dsi is NULL\n");
		return -EFAULT;
	}
	ctx = oplus_mtkDsi_to_panel(dsi);
	if (!ctx) {
		OPLUS_DSI_ERR("ctx is NULL\n");
		return -EFAULT;
	}

	if (!strcmp("panel_ac180_p_3_a0020_dsi_cmd", ctx->panel_info.panel_name)
			&& (ctx->panel_info.panel_flag & 0x10)
			&& !g_gamma_regs_read_done) {
		rc |= oplus_panel_gamma_compensation(dsi);
	} else if (!strcmp("panel_ac178_p_7_a0014_dsi_cmd", ctx->panel_info.panel_name)
			&& !g_gamma_regs_read_done) {
		rc |= oplus_panel_ac178_gamma_compensation(dsi);
	}

	return rc;
}

int oplus_display_second_half_frame_sync(struct drm_crtc *crtc, u32 fps)
{
	int rc = 0;
	u32 delay_us = 0;
	u32 time_gap_ns = 0;
	u32 te_tag_ns = 0;
	u32 cur_vsync_ns = 0;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	if (fps <= 0) {
		OPLUS_DSI_ERR("Invalid fps\n");
		return -EINVAL;
	}

	if (!mtk_crtc) {
		OPLUS_DSI_ERR("Invalid mtk_crtc\n");
		return -EINVAL;
	}

	te_tag_ns = mtk_crtc->oplus_apollo_br->oplus_te_tag_ns;
	cur_vsync_ns = 1000000000 / fps;
	time_gap_ns = ktime_get() > te_tag_ns ? ktime_get() - te_tag_ns : 0;
	if ((time_gap_ns >= 0) && (time_gap_ns < (cur_vsync_ns / 2))) {
		delay_us = ((cur_vsync_ns / 2) - time_gap_ns) / 1000;
		if (delay_us > 0) {
			OPLUS_DSI_INFO("fps(%u) sleep %u us\n", fps, delay_us);
			OPLUS_DSI_TRACE_BEGIN("%s fps(%u) sleep %u us", __func__, fps, delay_us);
			usleep_range(delay_us, delay_us + 100);
			OPLUS_DSI_TRACE_END("%s fps(%u) sleep %u us", __func__, fps, delay_us);
		}
	}

	OPLUS_DSI_DEBUG("fps(%u) cur_vsync_ns(%u) te_tag_ns(%u) time_gap_ns(%u) delay_us(%u)\n",
			fps, cur_vsync_ns, te_tag_ns, time_gap_ns, delay_us);

	return rc;
}
EXPORT_SYMBOL(oplus_display_second_half_frame_sync);

int oplus_panel_regs_validate(struct oplus_panel_regs_check_config *config)
{
	int i = 0, tmp = 0;
	u32 len = 0, reg_count = 0;
	u8 *lenp;
	u32 data_offset = 0, group_offset = 0, value_offset = 0;
	u32 cmd_index = 0, data_index = 0, group_index = 0;
	u32 match_modes = 0, mode = 0;
	bool matched, group_mode0_matched, group_mode1_matched, group_matched;
	char payload[OPLUS_DSI_CMD_PRINT_BUF_SIZE] = "";
	u32 cnt = 0;

	if (!(config->config & OPLUS_REGS_CHECK_ENABLE)) {
		OPLUS_DSI_WARN("regs check is disable, return\n");
		return -EFAULT;
	}

	match_modes = config->match_modes;
	lenp = config->check_regs_rlen;
	reg_count = config->reg_count;

	for (i = 0; i < reg_count; i++) {
		len += lenp[i];
	}

	group_matched = false;
	group_mode1_matched = true;
	for (group_index = 0, group_offset = 0; group_index < config->groups; ++group_index) {
		group_mode0_matched = true;
		for (cmd_index = 0, data_offset = 0; cmd_index < reg_count; ++cmd_index) {
			mode = (match_modes >> cmd_index) & 0x01;
			tmp = 0;
			for (data_index = 0; data_index < lenp[cmd_index]; ++data_index) {
				matched = true;
				value_offset = group_offset + data_offset + data_index;
				if (!mode && config->return_buf[data_offset + data_index] !=
						config->check_value[value_offset]) {
					matched = false;
					group_mode0_matched = false;
				}
				else if (mode && config->return_buf[data_offset + data_index] ==
						config->check_value[value_offset]) {
					matched = false;
					tmp++;
				}
				OPLUS_DSI_DEBUG("check at index/group:[%d/%d] exp:[0x%02X] ret:[0x%02X] mode:[%u] matched:[%d]\n",
						data_offset + data_index,
						group_index,
						config->check_value[value_offset],
						config->return_buf[data_offset + data_index],
						mode,
						matched);
			}
			if (tmp == lenp[cmd_index])
					group_mode1_matched = false;

			data_offset += lenp[cmd_index];
		}
		group_matched = (group_matched || group_mode0_matched) && group_mode1_matched;

		OPLUS_DSI_DEBUG("check matching: group:[%d] mode0/mode1/matched:[%d/%d/%d]\n",
				group_index,
				group_mode0_matched,
				group_mode1_matched,
				group_matched);

		group_offset += len;
	}

	for (i = 0; i < len; ++i) {
		cnt += scnprintf(payload + cnt, sizeof(payload) - cnt, "%02X ", config->return_buf[i]);
	}
	OPLUS_DSI_INFO("check reg return_buf:[%s], matched:[%d]\n", payload, group_matched);

	if (group_matched)
		return 0;

	return -EINVAL;
}

int oplus_panel_regs_read(struct mtk_dsi *dsi, struct oplus_panel_regs_check_config *config)
{
	int i, rc = 0, count = 0, start = 0;
	u32 enter_cmd = DSI_CMD_ID_MAX;
	u32 exit_cmd = DSI_CMD_ID_MAX;
	u8 *lenp;
	char *regs;

	if (!(config->config & OPLUS_REGS_CHECK_ENABLE)) {
		OPLUS_DSI_WARN("regs check is disable, return\n");
		return -EFAULT;
	}

	enter_cmd = config->enter_cmd;
	exit_cmd = config->exit_cmd;
	lenp = config->check_regs_rlen;
	count = config->reg_count;
	regs = config->check_regs;

	/* send enter cmd */
	if (config->config & OPLUS_REGS_CHECK_PAGE_SWITCH) {
		if ((enter_cmd < DSI_CMD_ID_MAX) && (lcm_all_cmd_table[enter_cmd].cmd_lines > 0)) {
			rc = oplus_dsi_panel_send_cmd(dsi, enter_cmd,
					NULL, DSI_CMD_FUNC_DEFAULT);
			if (rc) {
				OPLUS_DSI_ERR("failed to send dsi_cmd: %s, rc=%d\n",
						dsi_cmd_map[enter_cmd], rc);
				return rc;
			}
		}
	}

	/* read regs */
	for (i = 0; i < count; ++i) {
		oplus_ddic_dsi_read_cmd_without_lock(dsi, regs[i], lenp[i], config->check_buf);
		memcpy(config->return_buf + start, config->check_buf, lenp[i]);
		start += lenp[i];
	}

	/* send exit cmd */
	if (config->config & OPLUS_REGS_CHECK_PAGE_SWITCH) {
		if ((exit_cmd < DSI_CMD_ID_MAX) && (lcm_all_cmd_table[exit_cmd].cmd_lines > 0)) {
			rc = oplus_dsi_panel_send_cmd(dsi, exit_cmd,
					NULL, DSI_CMD_FUNC_DEFAULT);
			if (rc) {
				OPLUS_DSI_ERR("failed to send dsi_cmd: %s, rc=%d\n",
						dsi_cmd_map[exit_cmd], rc);
				return rc;
			}
		}
	}

exit:
	return rc;
}

int oplus_panel_mipi_err_check(void *ctx)
{
	int rc = 0;
	struct mtk_dsi *dsi = NULL;
	struct mipi_dsi_device *dev_dsi = NULL;
	struct dsi_panel_lcm *lcm_ctx = ctx;

	if (!lcm_ctx) {
		OPLUS_DSI_ERR("invalid dsi\n");
		return -EINVAL;
	}

	dev_dsi = to_mipi_dsi_device(lcm_ctx->dev);
	dsi = container_of(dev_dsi->host, struct mtk_dsi, host);
	if (!lcm_ctx->enabled) {
		OPLUS_DSI_ERR("lcm_ctx is not enable\n");
		return -EINVAL;
	}

	rc = oplus_panel_regs_read(dsi, &lcm_ctx->mipi_err_config);
	if (rc) {
		OPLUS_DSI_ERR("regs read failed, rc=%d\n", rc);
		return rc;
	}

	rc = oplus_panel_regs_validate(&lcm_ctx->mipi_err_config);
	if (rc) {
		OPLUS_DSI_ERR("regs validate failed, rc=%d\n", rc);
		return rc;
	}

	return rc;
}

int oplus_panel_crc_check(void *ctx)
{
	int rc = 0;
	struct mtk_dsi *dsi = NULL;
	struct mipi_dsi_device *dev_dsi = NULL;
	struct dsi_panel_lcm *lcm_ctx = ctx;

	if (!lcm_ctx) {
		OPLUS_DSI_ERR("invalid dsi\n");
		return -EINVAL;
	}

	dev_dsi = to_mipi_dsi_device(lcm_ctx->dev);
	dsi = container_of(dev_dsi->host, struct mtk_dsi, host);
	if (!lcm_ctx->enabled) {
		OPLUS_DSI_ERR("lcm_ctx is not enable\n");
		return -EINVAL;
	}

	rc = oplus_panel_regs_read(dsi, &lcm_ctx->crc_config);
	if (rc) {
		OPLUS_DSI_ERR("regs read failed, rc=%d\n", rc);
		return rc;
	}

	rc = oplus_panel_regs_validate(&lcm_ctx->crc_config);
	if (rc) {
		OPLUS_DSI_ERR("regs validate failed, rc=%d\n", rc);
		return rc;
	}

	return rc;
}

int oplus_panel_backlight_check(void *ctx)
{
	int rc = 0;
	struct dsi_panel_lcm *lcm_ctx = ctx;
	char para[20] = {0};
	unsigned int reg;

	if (!lcm_ctx) {
		OPLUS_DSI_ERR("invalid dsi\n");
		return -EINVAL;
	}

	if (lcm_ctx->backlight_check_disable)
		return -EINVAL;
	oplus_ddic_dsi_read_cmd(0x52, 2, para);
	reg = (para[0] << 8) | para[1];
	OPLUS_DSI_INFO("last_backlight=%d, read 52h=%d [0x%02X, 0x%02X]\n", last_backlight, reg, para[0], para[1]);
	if (last_backlight > 0 && reg == 0)
		OPLUS_DSI_ERR("backlight check failed\n");
	rc = reg;
	return rc;
}
