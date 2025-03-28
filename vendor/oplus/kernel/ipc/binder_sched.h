/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Google, Inc.
 */

#ifndef _OPLUS_BINDER_SCHED_H_
#define _OPLUS_BINDER_SCHED_H_

#include <linux/sched.h>
#include <uapi/linux/android/binder.h>
#include <../drivers/android/binder_internal.h>

extern unsigned long long g_sched_debug;
#define trace_binder_debug(x...) \
	do { \
		if (g_sched_debug) \
			trace_printk(x); \
	} while (0)

#define oplus_binder_debug(debug_mask, x...) \
	do { \
		if (g_sched_debug & debug_mask) \
			pr_info(x); \
	} while (0)

#define SET_ASYNC_UX_ENABLE				0x45555801
#define ASYNC_UX_ENABLE_DATA_SIZE		4
#define OBS_NOT_ASYNC_UX_VALUE			0xfffffffffffffff1	//(unsigned long - MAX_ERRNO - ...)

#define CURRENT_TASK_PID				-1
#define SYSTEM_SERVER_NAME              "system_server"
#define BD_FEATURE_MASK                 0xffffffff

extern unsigned int g_sched_enable;

enum OBS_STATUS {
	 OBS_INVALID,
	 OBS_VALID,
	 OBS_NOT_ASYNC_UX,
};

#define INVALID_VALUE           -1
#define MAX_UX_IN_LIST			20
#define CHECK_MAX_NODE_FOR_ASYNC_THREAD		400
#define MAX_ACCUMULATED_UX		2

#define MAX_WORKS_IN_FGLIST					10
#define MAX_CONTINUOUS_FG					3
#define FG_DEBUG_INTERVAL_DEFAULT			800
#define FG_DEBUG_DEFAULT_SYSTEM_SERVER		0

/*All functions of the binder must be controlled by each
  bit of this switch. The binder uses bits 16-31.*/
#define BD_FEATURE_ENABLE_DEFAULT		0xffffffff
#define BD_SYNC_UX_ENABLE_MASK          (1 << 16)
#define BD_ASYNC_UX_ENABLE_MASK      	(1 << 17)
#define BD_REF_OPT_ENABLE_MASK   		(1 << 18)
#define BD_FG_LIST_ENABLE_MASK			(1 << 19)

enum BINDER_UX_TEST_ITEM {
	 BINDER_UX_TEST_DISABLE,
	 ASYNC_UX_RANDOM_LOW_INSERT_TEST,
	 ASYNC_UX_RANDOM_HIGH_INSERT_TEST,
	 ASYNC_UX_RANDOM_LOW_ENQUEUE_TEST,
	 ASYNC_UX_RANDOM_HIGH_ENQUEUE_TEST,
	 ASYNC_UX_INORDER_TEST,
	 SYNC_UX_RANDOM_LOW_TEST,
	 SYNC_UX_RANDOM_HIGH_TEST,
};

enum ASYNC_UX_ENABLE_ITEM {
	ASYNC_UX_DISABLE,
	ASYNC_UX_ENABLE_ENQUEUE,
	ASYNC_UX_ENABLE_INSERT_QUEUE,
	ASYNC_UX_ENABLE_MAX,
};

enum SYNC_UX_ENABLE_ITEM {
	SYNC_UX_DISABLE,
	SYNC_UX_ENABLE,
	SYNC_UX_ENABLE_MAX,
};

enum BINDER_UNSET_TYPE {
	ASYNC_UNSET,
	SYNC_UNSET,
	SYNC_OR_ASYNC_UNSET,
};

enum {
	LOG_BINDER_SYSTRACE_LVL0	= 1U << 0,
	LOG_BINDER_SYSTRACE_LVL1	= 1U << 1,
	LOG_BINDER_SYSTRACE_STATUS	= 1U << 2,
	LOG_SET_ASYNC_UX	= 1U << 3,
	LOG_TRACK_ASYNC_UX	= 1U << 4,
	LOG_SET_SYNC_UX		= 1U << 5,
	LOG_GET_LAST_ASYNC	= 1U << 6,
	LOG_TRACK_LAST_ASYNC	= 1U << 7,
	LOG_SET_ASYNC_AFTER_PENDING	= 1U << 8,
	LOG_SET_SF_UX	= 1U << 9,
	LOG_FG_LIST_LVL0	= 1U << 10,
	LOG_FG_LIST_LVL1 = 1U << 11,
	LOG_DUMP_LIST_MEMBER = 1U << 12,
};

enum {
	STATE_BINDER_UX_NONE = 0,
	STATE_SYNC_UNSET_UX = 1,
	STATE_SYNC_RT_NOT_SET = 2,
	STATE_SYNC_TYPE_UNEXPECTED = 3,
	STATE_SYNC_NOT_SET = 4,
	STATE_ASYNC_UNSET_UX = 5,
	STATE_ASYNC_NOT_SET_LAST_UX = 6,
	STATE_NO_BINDER_THREAD = 7,
	STATE_ASYNC_NO_THREAD_NO_PENDING = 8,
	STATE_SYNC_INSERT_QUEUE = 9,
	STATE_ASYNC_INSERT_QUEUE = 10,
	STATE_SYNC_RT_NOT_SET_SERVICEMG = 11,
	STATE_SYNC_T_NOT_UNSET_UX = 12,
	STATE_SET_T_UX_STATE = 13,
	STATE_UNSET_T_UX_STATE = 14,
	STATE_SYNC_OR_ASYNC_UNSET_UX = 15,
	STATE_SF_ASYNC_IS_UX = 16,
	STATE_THREAD_WAS_ASYNC_UX = 17,
	STATE_ASYNC_HAS_THREAD = 18,
	STATE_FG_SELECT_FG = 19,
	STATE_FG_SELECT_PROC_OTHER_TYPE = 20,
	STATE_FG_SELECT_PROC_COMP_SEQ = 21,
	STATE_FG_SELECT_PROC_WHEN_UX = 22,
	STATE_FG_SELECT_PROC_WHEN_ASYNC = 23,
	STATE_FG_WORK_PROC_NULL = 24,
	STATE_FG_NOT_SYNC_UX = 25,
	STATE_FG_WORKS_OVERFLOW = 26,
	STATE_FG_TODO_NULL = 27,
	STATE_FG_ADD_TO_FG = 28,
	STATE_FG_VIP_THREAD_SKIP = 29,
	STATE_SYNC_SET_UX = 50,
	STATE_SYNC_RESET_UX = 51,
	STATE_SYNC_RT_SET_UX = 52,
	STATE_ASYNC_SET_UX = 53,
	STATE_ASYNC_SET_LAST_UX = 54,
	STATE_ASYNC_SET_UX_AFTER_NO_THREAD = 55,
	STATE_SYNC_SET_UX_AGAIN_SERVICEMG = 56,
	STATE_SYNC_RESET_UX_SERVICEMG = 57,
};

enum {
	NUM_INSERT_ID1,
	NUM_INSERT_ID2,
	NUM_INSERT_MAX,
};

enum T_UX_STATE{
	T_UX_STATE_UNKNOWN,
	T_NOT_SYNC_UX,
	T_IS_SYNC_UX,
};

enum FG_LIST_DEBUG_ITEM {
	ITEM_FG_LIST_DEBUG_UNKNOWN,
	ITEM_SYNC_UX_NOTHREAD,
	ITEM_ADD_TO_FG,
	ITEM_SELECT_FG,
	ITEM_SELECT_PROC,
	ITEM_FG_WORKS_OVERFLOW,
	ITEM_SELECT_FG_DIRECTLY,
	ITEM_SELECT_PROC_OTHER_TYPE,
	ITEM_SELECT_PROC_COMPARE_SEQ,
	ITEM_SELECT_FG_COMPARE_SEQ,
	ITEM_SELECT_CONTINUE_COUNT_OVER,
	ITEM_SELECT_FG_PROC_EMPTY,
	ITEM_SELECT_PROC_WHEN_UX,
	ITEM_SELECT_PROC_WHEN_ASYNC,
};

struct oplus_binder_struct {
	int async_ux_enable;
	bool pending_async;
	bool async_ux_no_thread;
	int sync_ux_enable;
	int t_ux_state;
};

struct oplus_binder_proc {
	struct list_head fg_todo;
	bool fg_inited;
	int fg_count;
	int continuous_fg;
};

static inline void oplus_bd_feat_enable(unsigned int bd_feat, bool enable)
{
	bd_feat &= BD_FEATURE_MASK;
	if (enable)
		g_sched_enable = !!(bd_feat);
	else
		g_sched_enable = !!(~bd_feat);
}

void binder_ux_state_systrace(struct task_struct *from, struct task_struct *target,
	int ux_state, int systrace_lvl, struct binder_transaction *t, struct binder_proc *proc);

#endif /* _OPLUS_BINDER_SCHED_H_ */
