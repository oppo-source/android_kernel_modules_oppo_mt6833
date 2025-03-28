/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_display_device_ioctl.h
** Description : oplus display device ioctl header
** Version : 1.0
** Date : 2024/05/20
** Author : Display
******************************************************************/
#ifndef _OPLUS_DISPLAY_DEVICE_IOCTL_H_
#define _OPLUS_DISPLAY_DEVICE_IOCTL_H_

#include "oplus_display_sysfs_attrs.h"

int oplus_display_panel_get_id(void *buf);
int oplus_display_panel_get_vendor(void *buf);
int oplus_display_panel_get_serial_number(void *buf);
int oplus_display_get_softiris_color_status(void *buf);
int oplus_display_set_shutdown_flag(void *buf);
int oplus_display_panel_get_stage(void *buf);
int oplus_display_panel_set_seed(void *buf);
int oplus_display_panel_get_seed(void *buf);
int oplus_display_panel_get_max_brightness(void *buf);
int oplus_display_panel_set_closebl_flag(void *buf);
int oplus_display_panel_get_closebl_flag(void *buf);
int oplus_display_panel_get_brightness(void *buf);
int oplus_display_panel_set_brightness(void *buf);
int oplus_display_panel_set_cabc(void *buf);
int oplus_display_panel_get_cabc(void *buf);
int oplus_display_panel_set_esd(void *buf);
int oplus_display_panel_get_esd(void *buf);
int oplus_display_get_dp_support(void *buf);
int oplus_display_panel_get_pq_trigger(void *buf);
int oplus_display_panel_set_pq_trigger(void *buf);
void oplus_te_check(struct mtk_drm_crtc *mtk_crtc, unsigned long long te_time_diff);
int oplus_display_panel_get_panel_bpp(void *buf);
int oplus_display_panel_get_panel_type(void *data);
int oplus_display_panel_set_hbm_max(void *data);
int oplus_display_panel_get_hbm_max(void *data);
ssize_t oplus_get_hbm_max_debug(struct kobject *obj,
		struct kobj_attribute *attr, char *buf);
ssize_t oplus_set_hbm_max_debug(struct kobject *obj,
		struct kobj_attribute *attr,
		const char *buf, size_t count);
int oplus_display_panel_set_mipi_err_check(void *data);
int oplus_display_panel_get_mipi_err_check(void *data);
int oplus_display_panel_set_esd_status(void *data);
int oplus_display_panel_get_esd_status(void *data);
int oplus_display_panel_set_crc_check(void *data);
int oplus_display_panel_get_crc_check(void *data);
#endif /*_OPLUS_DISPLAY_DEVICE_IOCTL_H_*/
