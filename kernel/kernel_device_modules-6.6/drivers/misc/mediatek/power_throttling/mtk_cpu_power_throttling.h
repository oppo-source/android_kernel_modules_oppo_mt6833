/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Samuel Hsieh <samuel.hsieh@mediatek.com>
 */

#ifndef _MTK_CPU_POWER_THROTTLING_H_
#define _MTK_CPU_POWER_THROTTLING_H_

enum cpu_pt_type {
	LBAT_POWER_THROTTLING,
	OC_POWER_THROTTLING,
	SOC_POWER_THROTTLING,
	EXT_POWER_THROTTLING,
	POWER_THROTTLING_TYPE_MAX
};
void cpu_ext_throttle(unsigned int level);

typedef int (*cpu_isolate_cb)(unsigned int cpu, bool is_pause);
extern int register_pt_isolate_cb(cpu_isolate_cb cb_func);

typedef int (*cpu_isolate_cb)(unsigned int cpu, bool is_pause);
extern int register_pt_isolate_cb(cpu_isolate_cb cb_func);

#endif
