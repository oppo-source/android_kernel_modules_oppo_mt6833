/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_display_interface.h
** Description : oplus display interface header
** Version : 1.0
** Date : 2024/04/28
** Author : Display
******************************************************************/
#ifndef _OPLUS_DISPLAY_INTERFACE_H_
#define _OPLUS_DISPLAY_INTERFACE_H_
#include <drm/drm_crtc.h>

#include "oplus_dsi_display_config.h"

/* ---------------------------- oplus define ---------------------------- */


/* ---------------------------- extern params ---------------------------- */


/* ---------------------------- function implementation ---------------------------- */
struct dsi_panel_lcm* oplus_mtkCrtc_to_panel(struct mtk_drm_crtc *mtk_crtc);
struct dsi_panel_lcm* oplus_mtkDsi_to_panel(struct mtk_dsi *dsi);
int oplus_panel_init(struct drm_crtc *crtc);
int oplus_dsi_display_parse(struct device_node *node, void *ctx);

#endif /* _OPLUS_DISPLAY_INTERFACE_H_ */
