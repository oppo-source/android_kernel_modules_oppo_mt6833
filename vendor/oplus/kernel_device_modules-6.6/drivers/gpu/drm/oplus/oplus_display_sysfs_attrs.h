/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_display_sysfs_attrs.h
** Description : oplus display sysfs attrs header
** Version : 1.0
** Date : 2024/05/20
** Author : Display
******************************************************************/
#ifndef _OPLUS_DISPLAY_SYSFS_ATTRS_H_
#define _OPLUS_DISPLAY_SYSFS_ATTRS_H_

#include <linux/err.h>
#include <linux/of.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/leds.h>
#include <drm/drm_device.h>
#include <linux/delay.h>

#include <drm/drm_mipi_dsi.h>
#include <video/videomode.h>

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_gem.h"
#include "mtk_drm_plane.h"
#include "mtk_drm_session.h"
#include "mtk_dump.h"
#include "mtk_drm_fb.h"
#include "mtk_rect.h"
#include "mtk_drm_ddp_addon.h"
#include "mtk_drm_helper.h"
#include "mtk_drm_lowpower.h"
#include "mtk_drm_assert.h"
#include "mtk_disp_recovery.h"
#include "mtk_drm_arr.h"
#include "mtk_log.h"

#define RAMLESS_AOD_AREA_NUM 6
#define RAMLESS_AOD_PAYLOAD_SIZE 100
#define OPLUS_DSI_CMD_DEBUG_BUF_SIZE 8192
#define DISPLAY_TOOL_CMD_KEYWORD "[display:sh]"

/* aod_area begin */
struct aod_area {
	bool enable;
	int x;
	int y;
	int w;
	int h;
	int color;
	int bitdepth;
	int mono;
	int gray;
};

int oplus_display_private_api_init(void);
void oplus_display_private_api_exit(void);
int oplus_serial_number_probe(struct device_node *node);
int oplus_display_read_panel_id(struct drm_crtc *crtc);
int oplus_panel_serial_number_read(struct drm_crtc *crtc);
int oplus_dsi_display_serial_number_deinit(void *ctx_dev);
#endif /* _OPLUS_DISPLAY_SYSFS_ATTRS_H_ */
