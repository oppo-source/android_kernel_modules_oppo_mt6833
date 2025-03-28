/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_display_dsi_primary.h
** Description : oplus panel driver header
** Version : 1.0
** Date : 2024/05/20
** Author : Display
******************************************************************/
#ifndef _OPLUS_DISPLAY_DSI_PRIMARY_H_
#define _OPLUS_DISPLAY_DSI_PRIMARY_H_

/* Panel Config */
#define MATCH_NAME                  "oplus_display_dsi_primary"
#define LCM_TAG                     panel_name
#define LCM_BRIGHTNESS_TYPE         2

#define MODE_MAPPING_RULE(x)        ((x) % (mode_num_cur))

/* Oplus Feature Config */
#define LHBM_BACKLIGHT_THRESHOLD    0x481

#endif /* _OPLUS_DISPLAY_DSI_PRIMARY_H_ */
