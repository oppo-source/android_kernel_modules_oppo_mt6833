// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2022 OPLUS Inc.

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/types.h>
#include <linux/timer.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define_v4l2.h"
#include "kd_imgsensor_errcode.h"

#include "adaptor-subdrv-ctrl.h"
#include "adaptor-i2c.h"
#include "adaptor.h"
#include "oplus-adaptor-subdrv-ctrl.h"

bool common_read_cmos_eeprom_p8(struct subdrv_ctx *ctx, u16 addr, u16 reg,
                    BYTE *data, int size)
{
	if (adaptor_i2c_rd_p8(ctx->i2c_client, addr >> 1,
			reg, data, size) < 0) {
		return false;
	}
	return true;
}

int read_eeprom_common_data(struct subdrv_ctx *ctx, struct oplus_eeprom_info_struct* eeprom_info, struct eeprom_addr_table_struct addr_table) {
	if (ctx == NULL || eeprom_info == NULL ) {
		return -EINVAL;
	}
	eeprom_info->sensorid_offset = addr_table.addr_sensorid - addr_table.addr_modinfo;
	eeprom_info->lens_offset = addr_table.addr_lens - addr_table.addr_modinfo;
	eeprom_info->vcm_offset = addr_table.addr_vcm - addr_table.addr_modinfo;
	eeprom_info->macPos_offset = addr_table.addr_afmacro - addr_table.addr_af;
	eeprom_info->infPos_offset = addr_table.addr_afinf - addr_table.addr_af;

	if (addr_table.addr_modinfoflag || addr_table.addr_modinfo) {
		eeprom_info->moduleInfo_size = addr_table.addr_modinfoflag - addr_table.addr_modinfo + 1;
		common_read_cmos_eeprom_p8(ctx, addr_table.i2c_read_id, addr_table.addr_modinfo, eeprom_info->moduleInfo, eeprom_info->moduleInfo_size);
	}

	if (addr_table.addr_afflag || addr_table.addr_af) {
		eeprom_info->af_size = addr_table.addr_afflag - addr_table.addr_af + 1;
		common_read_cmos_eeprom_p8(ctx, addr_table.i2c_read_id, addr_table.addr_af, eeprom_info->afInfo, eeprom_info->af_size);
	}

	if (addr_table.addr_qrcodeflag || addr_table.addr_qrcode) {
		eeprom_info->qrcode_size = addr_table.addr_qrcodeflag - addr_table.addr_qrcode + 1;
		common_read_cmos_eeprom_p8(ctx, addr_table.i2c_read_id, addr_table.addr_qrcode, eeprom_info->qrcodeInfo, eeprom_info->qrcode_size);
	}
	return 0;
}

void check_stream_on(struct subdrv_ctx *ctx)
{
	u32 i = 0, framecnt = 0;
	int timeout = ctx->current_fps ? (10000 / ctx->current_fps) + 1 : 101;

	if (!ctx->s_ctx.reg_addr_frame_count)
		return;
	for (i = 0; i < timeout; i++) {
		framecnt = subdrv_i2c_rd_u8(ctx, ctx->s_ctx.reg_addr_frame_count);
		DRV_LOG(ctx, "check_stream_on delay %d\n", i);
		if (framecnt != 0xFF)
			return;
		mdelay(1);
	}
	DRV_LOGE(ctx, "stream on fail!\n");
}