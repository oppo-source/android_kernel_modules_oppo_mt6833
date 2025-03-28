/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_display_power.h
** Description : oplus display power header
** Version : 1.0
** Date : 2024/05/20
** Author : Display
******************************************************************/
#ifndef _OPLUS_DISPLAY_POWER_H_
#define _OPLUS_DISPLAY_POWER_H_

#include <linux/err.h>

enum PANEL_RESET_POSITION_ENUM {
	PANEL_RESET_POSITION0 = 0,
	PANEL_RESET_POSITION1,
	PANEL_RESET_POSITION2,
	PANEL_RESET_POSITION3,
};

enum PANEL_VOLTAGE_ENUM {
	PANEL_VOLTAGE_ID_VDDI = 0,
	PANEL_VOLTAGE_ID_VDDR,
	PANEL_VOLTAGE_ID_VCI,
	PANEL_VOLTAGE_ID_VG_BASE,
	PANEL_VOLTAGE_ID_MAX,
};

#define PANEL_VOLTAGE_VALUE_COUNT 4

typedef struct panel_voltage_bak {
	u32 voltage_id;
	u32 voltage_min;
	u32 voltage_current;
	u32 voltage_max;
	char pwr_name[20];
}PANEL_VOLTAGE_BAK;

struct dsi_vreg
{
	struct regulator *vreg;
	char vreg_name[32];
	struct gpio_desc *vreg_gpio;
	u32 vreg_type;
	u32 min_voltage;
	u32 max_voltage;
};


struct dsi_regulator_info {
	struct dsi_vreg *vregs;
	u32 count;
	u32 refcount;
};

int oplus_panel_set_vg_base(unsigned int panel_vol_value);
extern int oplus_dsi_panel_power_on(void *ctx_dev);
extern int oplus_dsi_panel_power_off(void *ctx_dev);
int oplus_dsi_panel_power_supply_enable(void *ctx_dev, int flag);
extern int oplus_dsi_panel_reset_keep(void *ctx_dev, int value);
extern int oplus_dsi_panel_reset(void *ctx_dev);
extern int oplus_dsi_panel_prepare(void *ctx_dev);
int oplus_display_panel_set_pwr(void *buf);
int oplus_display_panel_get_pwr(void *buf);

#endif /*_OPLUS_DISPLAY_POWER_H_*/
