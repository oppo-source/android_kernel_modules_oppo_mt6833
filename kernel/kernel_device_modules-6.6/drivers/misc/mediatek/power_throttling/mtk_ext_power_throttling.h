/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Samuel Hsieh <samuel.hsieh@mediatek.com>
 */

#ifndef _MTK_EXT_POWER_THROTTLING_H_
#define _MTK_EXT_POWER_THROTTLING_H_

typedef void (*ext_callback)(unsigned int ext_level);

extern int register_ext_cb(ext_callback cb);

#endif