/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_dsi_panel_cmd.h
** Description : oplus dsi panel cmd header
** Version : 1.0
** Date : 2024/05/20
** Author : Display
******************************************************************/
#ifndef _OPLUS_DSI_PANEL_CMD_H_
#define _OPLUS_DSI_PANEL_CMD_H_

/* ---------------------------- oplus define ---------------------------- */
#define OPLUS_DSI_CMD_PRINT_KEYWORD "[dsi_cmd]"
#define OPLUS_DSI_CMD_PRINT_BUF_SIZE 2048

#define OPLUS_PANEL_TIMINGS_MAX 5
#define OPLUS_SEND_CMD_MAX 256
#define OPLUS_CMD_PARAMS_MAX 128
#define MTK_PANEL_CMD_PARAMS_MAX 64
#define CMD_SET_MIN_SIZE 7
#define SDC_AUTO_MIN_FPS_CMD_OFFSET 2
#define SDC_MANUAL_MIN_FPS_CMD_OFFSET 2
#define SDC_AUTO_MIN_FPS_CMD_HIGH_OFFSET 4
#define SDC_MANUAL_MIN_FPS_CMD_HIGH_OFFSET 4
#define SDC_MIN_FPS_CMD_SIZE 2

#define MIPI_DSI_MSG_BATCH_COMMAND BIT(6)

extern volatile unsigned int dsi_panel_mode_id;
#define lcm_all_cmd_table lcm_timings_cmds[oplus_dsi_panel_get_cell_index(dsi_panel_mode_id)]
#define lcm_cur_cmd_table(mode_id) lcm_timings_cmds[oplus_dsi_panel_get_cell_index(mode_id)]

/* The cell-index of timing in the panel dtsi must match the enumeration value */
enum MODE_ID {
	FHD_SDC60 = 0,
	FHD_SDC90 = 1,
	FHD_SDC120 = 2,
	FHD_OPLUS120 = 3,
	MODE_ID_MAX,
	MODE_ID_NONE,
};

enum SEED_MODE_ID {
	VIVID = 100,
	EXPERT = 101,
	NATURAL = 102,
	UIR_ON_VIVID = 110,
	UIR_ON_EXPERT = 111,
	UIR_ON_NATURAL = 112,
	UIR_OFF_VIVID = 120,
	UIR_OFF_EXPERT = 121,
	UIR_OFF_NATURAL = 122,
	UIR_VIVID = 130,
	UIR_EXPERT = 131,
	UIR_NATURAL = 132,
};

enum dsi_cmd_func {
	DSI_CMD_FUNC_DEFAULT,
	DSI_CMD_FUNC_HOST,
	DSI_CMD_FUNC_PACK_GCE,
	DSI_CMD_FUNC_GCE,
	DSI_CMD_FUNC_GCE2,
	DSI_CMD_FUNC_MAX,
};

enum dsi_cmd_id {
	DSI_CMD_DEBUG,
	DSI_CMD_SET_ON,
	DSI_CMD_SET_ON_PRE,
	DSI_CMD_SET_ON_POST,
	DSI_CMD_SET_OFF,
	DSI_CMD_HBM_ON,
	DSI_CMD_HBM_OFF,
	DSI_CMD_SET_TIMING_SWITCH,
	DSI_CMD_SET_TIMING_SWITCH_1PUL,
	DSI_CMD_ULTRA_LOW_POWER_AOD_OFF,
	DSI_CMD_ULTRA_LOW_POWER_AOD_ON,
	DSI_CMD_SET_NOLP,
	DSI_CMD_AOD_OFF_COMPENSATION,
	DSI_CMD_ADFR_MIN_FPS_120HZ,
	DSI_CMD_ADFR_MIN_FPS_90HZ,
	DSI_CMD_ADFR_MIN_FPS_60HZ,
	DSI_CMD_ADFR_MIN_FPS_45HZ,
	DSI_CMD_ADFR_MIN_FPS_30HZ,
	DSI_CMD_ADFR_MIN_FPS_20HZ,
	DSI_CMD_ADFR_MIN_FPS_15HZ,
	DSI_CMD_ADFR_MIN_FPS_10HZ,
	DSI_CMD_ADFR_MIN_FPS_5HZ,
	DSI_CMD_ADFR_MIN_FPS_1HZ,
	DSI_CMD_POWER_ON_1PUL,
	DSI_CMD_LHBM_PRESSED_ICON_GAMMA,
	DSI_CMD_LHBM_PRESSED_ICON_GRAYSCALE,
	DSI_CMD_LHBM_PRESSED_ICON_ON,
	DSI_CMD_LHBM_PRESSED_ICON_OFF,
	DSI_CMD_LHBM_PRESSED_ICON_ON_PWM,
	DSI_CMD_AOD_HIGH_LIGHT_MODE,
	DSI_CMD_AOD_LOW_LIGHT_MODE,
	DSI_CMD_SET_LP1,
	DSI_CMD_SET_BACKLIGHT,
	DSI_CMD_SET_SEED_EXPERT,
	DSI_CMD_SET_SEED_NATURAL,
	DSI_CMD_SET_SEED_VIVID,
	DSI_CMD_SET_UIR_ON_SEED_EXPERT,
	DSI_CMD_SET_UIR_ON_SEED_NATURAL,
	DSI_CMD_SET_UIR_ON_SEED_VIVID,
	DSI_CMD_SET_UIR_OFF_SEED_EXPERT,
	DSI_CMD_SET_UIR_OFF_SEED_NATURAL,
	DSI_CMD_SET_UIR_OFF_SEED_VIVID,
	DSI_CMD_SET_UIR_SEED_EXPERT,
	DSI_CMD_SET_UIR_SEED_NATURAL,
	DSI_CMD_SET_UIR_SEED_VIVID,
	DSI_CMD_SWITCH_HBM_APL_OFF,
	DSI_CMD_SWITCH_HBM_APL_ON,
	DSI_CMD_MULTI_TE_DISABLE,
	DSI_CMD_MULTI_TE_ENABLE,
	DSI_CMD_PWM_SWITCH_18TO1PUL,
	DSI_CMD_PWM_SWITCH_18TO3PUL,
	DSI_CMD_PWM_SWITCH_1TO18PUL,
	DSI_CMD_PWM_SWITCH_1TO3PUL,
	DSI_CMD_PWM_SWITCH_3TO18PUL,
	DSI_CMD_PWM_SWITCH_3TO1PUL,
	DSI_CMD_PANEL_SPR_ON,
	DSI_CMD_PANEL_SPR_OFF,
	DSI_CMD_EMDUTY_DBV_MODE0,
	DSI_CMD_EMDUTY_DBV_MODE1,
	DSI_CMD_EMDUTY_DBV_MODE2,
	DSI_CMD_EMDUTY_DBV_MODE3,
	DSI_CMD_EMDUTY_DBV_MODE4,
	DSI_CMD_EMDUTY_DBV_MODE5,
	DSI_CMD_EMDUTY_DBV_MODE6,
	DSI_CMD_EMDUTY_DBV_MAX,
	DSI_CMD_DEMURA_DBV_MODE0,
	DSI_CMD_DEMURA_DBV_MODE1,
	DSI_CMD_DEMURA_DBV_MODE2,
	DSI_CMD_DEMURA_DBV_MODE3,
	DSI_CMD_DEMURA_DBV_MODE4,
	DSI_CMD_DEMURA_DBV_MODE5,
	DSI_CMD_DEMURA_DBV_MODE6,
	DSI_CMD_DEMURA_DBV_MAX,
	DSI_CMD_PANEL_COMPENSATION,
	DSI_CMD_GAMMA_COMPENSATION_PAGE0,
	DSI_CMD_GAMMA_COMPENSATION_PAGE1,
	DSI_CMD_GAMMA_COMPENSATION_PAGE2,
	DSI_CMD_GAMMA_COMPENSATION,
	DSI_CMD_MIPI_ERR_CHECK_ENTER,
	DSI_CMD_MIPI_ERR_CHECK_EXIT,
	DSI_CMD_CRC_CHECK_ENTER,
	DSI_CMD_CRC_CHECK_EXIT,
	/* Add the new element above this */
	DSI_CMD_ID_MAX,
};

enum dsi_cmd_set_state {
	DSI_CMD_SET_STATE_LP,
	DSI_CMD_SET_STATE_LP_GCE,
	DSI_CMD_SET_STATE_HS,
	DSI_CMD_SET_STATE_MAX
};

struct dsi_cmd_msg {
	unsigned char type;
	unsigned char cmd1;
	unsigned char channel;
	unsigned char flags;
	unsigned char cmd4;
};

struct dsi_cmd_table {
	unsigned char count;
	unsigned char para_list[OPLUS_SEND_CMD_MAX];
	bool last_command;
	unsigned int post_wait_ms;
	struct dsi_cmd_msg cmd_msg;
};

struct dsi_cmd_sets {
	unsigned char cmd_id;
	unsigned int cmd_lines;
	enum dsi_cmd_set_state state;
	struct dsi_cmd_table *para_table;
};

/* ---------------------------- extern params ---------------------------- */
extern unsigned int oplus_display_brightness;
extern struct dsi_cmd_sets **lcm_timings_cmds;
extern const char *dsi_cmd_map[DSI_CMD_ID_MAX];
extern const char *dsi_cmd_state_map[DSI_CMD_ID_MAX];

extern void oplus_dsi_panel_dcs_write(void *ctx, const void *data, size_t len);
extern inline int oplus_dsi_panel_get_cell_index(int mode_id);
extern int oplus_dsi_panel_send_cmd(void *dsi, enum dsi_cmd_id cmd_set_id,
		void *handle, enum dsi_cmd_func cmd_func);


/* ---------------------------- function implementation ---------------------------- */
void oplus_dsi_panel_cmd_line_joint(struct dsi_cmd_table *para_table, char *buf);
void oplus_dsi_panel_print_cmd_lines(struct dsi_cmd_sets *cmd_sets, char *keyword);
int oplus_dsi_panel_parse_cmd(struct device_node *node, void *ctx);

#endif /* _OPLUS_DSI_PANEL_CMD_H_ */
