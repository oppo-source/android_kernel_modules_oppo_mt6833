/* SPDX-License-Identifier: GPL-2.0-only
 *
 * oplus_slc.h
 *
 * header file of oplus_slc module
 *
 * Copyright (c) 2023 Oplus. All rights reserved.
 *
 */

#ifndef _OPLUS_SLC_H
#define _OPLUS_SLC_H

#define TARGET_DEFAULT 0
#define SLC_MAX_SIZE 10

struct proc_dir_entry *oplus_slc_dir;

#if IS_ENABLED(CONFIG_MTK_SLBC_MT6989)
extern void oplus_slc_cdwb_switch(bool disable);
#endif

extern void get_cg_force_size(unsigned int *cpu,  unsigned int *gpu);
extern void get_cache_usage_size(int *cpu, int *gpu, int *other);
extern int get_oplus_slc_dis_cdwb_status(void);

#if IS_ENABLED(CONFIG_MTK_SLBC_MT6991)
extern struct slbc_ipi_ops *ipi_ops_ref;
#endif

#endif /* _OPLUS_SLC_H */
