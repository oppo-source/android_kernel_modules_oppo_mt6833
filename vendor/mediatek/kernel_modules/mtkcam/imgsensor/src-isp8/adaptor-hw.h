/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2019 MediaTek Inc. */

#ifndef __ADAPTOR_HW_H__
#define __ADAPTOR_HW_H__

#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
#include "oplus/inc/oplus_cam_olc_exception.h"
#endif /* OPLUS_FEATURE_CAMERA_COMMON */

int adaptor_hw_power_on(struct adaptor_ctx *ctx);
int adaptor_hw_power_off(struct adaptor_ctx *ctx);
int adaptor_hw_init(struct adaptor_ctx *ctx);
int adaptor_hw_sensor_reset(struct adaptor_ctx *ctx);
int adaptor_cam_pmic_on(struct adaptor_ctx *ctx);
int adaptor_cam_pmic_off(struct adaptor_ctx *ctx);
int adaptor_hw_deinit(struct adaptor_ctx *ctx);


#endif
