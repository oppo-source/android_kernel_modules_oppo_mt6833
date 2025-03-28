/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __IDC_COLLECT_H
#define __IDC_COLLECT_H
/* in ged/src/ged_dvfs.c */

extern int oplus_slc_indicator_init(struct proc_dir_entry *parent_dir);
extern void oplus_slc_indicator_exit(void);
extern void oplus_slc_indicator_suspend(bool isSuspend);

#endif /* __OSML_H */
