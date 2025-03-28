/********************************************************************************
** Copyright (C) 2024 Oplus.
**
** Description:
**     Create a new header file to record gpu 5566 information
       for kernel_device_modules-6.6
**
** Date: 2024-07-10
********************************************************************************/
#undef TRACE_SYSTEM
#define TRACE_SYSTEM ged_perfetto

#if !defined(_TRACE_GED_PERFETTO_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_GED_PERFETTO_H

#include <linux/tracepoint.h>

TRACE_EVENT(oplus_tracing_mark_write,

	TP_PROTO(int pid, const char *name, long long value),

	TP_ARGS(pid, name, value),

	TP_STRUCT__entry(
		__field(int, pid)
		__string(name, name)
		__field(long long, value)
	),

	TP_fast_assign(
		__entry->pid = pid;
		__assign_str(name, name);
		__entry->value = value;
	),

	TP_printk("C|%d|%s|%lld", __entry->pid, __get_str(name), __entry->value)
);
#endif /* _TRACE_GED_PERFETTO_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ../../drivers/gpu/mediatek/ged/include
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE ged_perfetto_tracepoint
#include <trace/define_trace.h>