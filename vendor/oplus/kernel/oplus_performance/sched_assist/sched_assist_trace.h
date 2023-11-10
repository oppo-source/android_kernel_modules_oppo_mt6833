/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2020 Oplus. All rights reserved.
 */


#if !defined(_OPLUS_SCHED_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _OPLUS_SCHED_TRACE_H

#include <linux/tracepoint.h>

#undef TRACE_SYSTEM
#define TRACE_SYSTEM oplus_sched
#define TRACE_INCLUDE_FILE sched_assist_trace

TRACE_EVENT(oplus_tp_sched_change_ux,
	TP_PROTO(int chg_ux, int target_cpu),
	TP_ARGS(chg_ux, target_cpu),
	TP_STRUCT__entry(
		__field(int, chg_ux)
		__field(int, target_cpu)),
	TP_fast_assign(
		__entry->chg_ux = chg_ux;
		__entry->target_cpu = target_cpu;),
	TP_printk("chg_ux=%d target_cpu=%d", __entry->chg_ux, __entry->target_cpu)
);

TRACE_EVENT(oplus_tp_sched_switch_ux,
	TP_PROTO(int chg_ux, int target_cpu),
	TP_ARGS(chg_ux, target_cpu),
	TP_STRUCT__entry(
		__field(int, chg_ux)
		__field(int, target_cpu)),
	TP_fast_assign(
		__entry->chg_ux = chg_ux;
		__entry->target_cpu = target_cpu;),
	TP_printk("chg_ux=%d target_cpu=%d", __entry->chg_ux, __entry->target_cpu)
);

#ifdef CONFIG_LOCKING_PROTECT
DECLARE_EVENT_CLASS(sched_locking_template,

	TP_PROTO(struct task_struct *p, int lk_depth, int lk_nr),

	TP_ARGS(p, lk_depth, lk_nr),

	TP_STRUCT__entry(
		__array(char,	comm, TASK_COMM_LEN)
		__field(int,    pid)
		__field(int,	lk_depth)
		__field(int,	lk_nr)),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid                    = p->pid;
		__entry->lk_depth		= lk_depth;
		__entry->lk_nr			= lk_nr;),

	TP_printk("comm=%s pid=%d lk_depth=%d rq_lk_nr=%d",
		__entry->comm, __entry->pid, __entry->lk_depth, __entry->lk_nr)
);

DEFINE_EVENT(sched_locking_template, enqueue_locking_thread,
	TP_PROTO(struct task_struct *p, int lk_depth, int lk_nr),
	TP_ARGS(p, lk_depth, lk_nr));

DEFINE_EVENT(sched_locking_template, dequeue_locking_thread,
	TP_PROTO(struct task_struct *p, int lk_depth, int lk_nr),
	TP_ARGS(p, lk_depth, lk_nr));

DEFINE_EVENT(sched_locking_template, select_locking_thread,
	TP_PROTO(struct task_struct *p, int lk_depth, int lk_nr),
	TP_ARGS(p, lk_depth, lk_nr));
#endif /* CONFIG_LOCKING_PROTECT */

#endif /* _OPLUS_SCHED_TRACE_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#include <trace/define_trace.h>
