/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __GED_GE_H__
#define __GED_GE_H__

#include <linux/types.h>
#include <ged_type.h>

/* Must be the same as item number in region_sizes[], which in
 * /vendor/mediatek/proprietary/hardware/gralloc_extra/ge_misc.cpp
 * If the video fg function fails, please check whether the value matches first.
 * error case: ged_ge_get: vendor don't support region id: 2x
 * Modify the value to match the file(ge_misc.cpp) structure size to support dimming extension
 */
#define GE_ALLOC_STRUCT_NUM 23
#define GE_MAX_REGION_SIZE 8192

GED_ERROR ged_ge_init(void);
int ged_ge_exit(void);
int ged_ge_alloc(int region_num, uint32_t *region_sizes);
int ged_ge_get(int ge_fd, int region_id, int u32_offset,
	int u32_size, uint32_t *output_data);
int ged_ge_set(int ge_fd, int region_id, int u32_offset,
	int u32_size, uint32_t *input_data);
int ged_dmabuf_set_name(int32_t share_fd, char *name);

#endif
