/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_display_dc.c
** Description : oplus display dc feature
** Version : 1.0
** Date : 2024/05/20
** Author : Display
******************************************************************/
#include "oplus_display_dc.h"
#ifdef OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT
#include "oplus_display_onscreenfingerprint.h"
#endif /* OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT */

extern int oplus_panel_alpha;
extern int oplus_dc_alpha;
extern int oplus_underbrightness_alpha;
int oplus_dc_enable = 0;
extern int oplus_get_panel_brightness_to_alpha(void);

int oplus_display_panel_get_dim_alpha(void *buf)
{
	unsigned int *dim_alpha = buf;

#ifdef OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT
	if (!oplus_ofp_get_hbm_state()) {
		(*dim_alpha) = 0;
		return 0;
	}
#endif /* OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT */

	oplus_underbrightness_alpha = oplus_get_panel_brightness_to_alpha();
	(*dim_alpha) = oplus_underbrightness_alpha;

	return 0;
}

int oplus_display_panel_set_dim_alpha(void *buf)
{
	unsigned int *dim_alpha = buf;

	oplus_panel_alpha = (*dim_alpha);

	return 0;
}

int oplus_display_panel_get_dimlayer_enable(void *buf)
{
	unsigned int *dimlayer_enable = buf;

	(*dimlayer_enable) = oplus_panel_alpha;

	return 0;
}

int oplus_display_panel_set_dimlayer_enable(void *buf)
{
	unsigned int *dimlayer_enable = buf;

	oplus_panel_alpha = (*dimlayer_enable);

	oplus_dc_enable = oplus_panel_alpha;
	return 0;
}

int oplus_display_panel_get_dim_dc_alpha(void *buf)
{
	unsigned int *dim_dc_alpha = buf;

	(*dim_dc_alpha) = oplus_dc_alpha;

	return 0;
}

int oplus_display_panel_set_dim_dc_alpha(void *buf)
{
	unsigned int *dim_dc_alpha = buf;

	oplus_dc_alpha = (*dim_dc_alpha);

	return 0;
}

