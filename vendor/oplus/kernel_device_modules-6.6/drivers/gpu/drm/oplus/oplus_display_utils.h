/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_display_utils.h
** Description : oplus display utils header
** Version : 1.0
** Date : 2024/07/26
** Author : Display
******************************************************************/
#ifndef _OPLUS_DISPLAY_UTILS_H_
#define _OPLUS_DISPLAY_UTILS_H_

/* ---------------------------- oplus define ---------------------------- */
#define OPLUS_MIPI_TX_MAX_LEN 128
#define OPLUS_MIPI_RX_MAX_LEN 10
#define AC178_GAMMA_COMPENSATION_READ_RETRY_MAX 5
#define AC178_GAMMA_COMPENSATION_READ_REG1 0xB2
#define AC178_GAMMA_COMPENSATION_READ_REG2 0xB5
#define AC178_GAMMA_COMPENSATION_READ_REG3 0xB8
#define AC178_GAMMA_COMPENSATION_CMD_INDEX 1
#define AC178_GAMMA_COMPENSATION_REG_INDEX1 0
#define AC178_GAMMA_COMPENSATION_REG_INDEX2 1
#define AC178_GAMMA_COMPENSATION_REG_INDEX3 2
#define AC178_GAMMA_COMPENSATION_REG_INDEX4 3
#define AC178_GAMMA_COMPENSATION_REG_INDEX5 4
#define AC178_GAMMA_COMPENSATION_REG_INDEX6 5
#define AC178_GAMMA_COMPENSATION_READ_LENGTH 2


/* ---------------------------- extern params ---------------------------- */


/* ---------------------------- function implementation ---------------------------- */
void oplus_ddic_dsi_send_cmd(unsigned int cmd_num, char *data);
void oplus_ddic_dsi_read_cmd(u8 reg, int len, char *data);
int oplus_panel_ext_init(struct drm_crtc *crtc);
int oplus_display_second_half_frame_sync(struct drm_crtc *crtc, u32 fps);
int oplus_panel_mipi_err_validate(void *ctx);
int oplus_panel_mipi_err_reg_read(struct mtk_dsi *dsi, void *ctx);
int oplus_panel_mipi_err_check(void *ctx);
int oplus_panel_crc_validate(void *ctx);
int oplus_panel_crc_reg_read(struct mtk_dsi *dsi, void *ctx);
int oplus_panel_crc_check(void *ctx);
int oplus_panel_backlight_check(void *ctx);
#endif /* _OPLUS_DISPLAY_UTILS_H_ */
