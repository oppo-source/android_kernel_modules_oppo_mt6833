/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2020 OPLUS Inc. */

#ifndef __OPLUS_ADAPTOR_SUBDRV_CTRL_H__
#define __OPLUS_ADAPTOR_SUBDRV_CTRL_H__
#include "adaptor-subdrv.h"
#include "inc/oplus_kd_eeprom.h"

bool common_read_cmos_eeprom_p8(struct subdrv_ctx *ctx, u16 addr, u16 reg, BYTE *data, int size);
int read_eeprom_common_data(struct subdrv_ctx *ctx, struct oplus_eeprom_info_struct* eeprom_info, struct eeprom_addr_table_struct addr_table);
void check_stream_on(struct subdrv_ctx *ctx);

#endif