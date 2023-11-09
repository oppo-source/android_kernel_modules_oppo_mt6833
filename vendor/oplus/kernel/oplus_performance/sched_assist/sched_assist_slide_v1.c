// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020 Oplus. All rights reserved.
 */
#include <linux/sched.h>
#include <linux/sysctl.h>
#include <linux/topology.h>
#include <../kernel/sched/sched.h>
#include <../kernel/sched/walt.h>
#include <linux/sched/cpufreq.h>
#include "sched_assist_common.h"
#include "sched_assist_slide.h"

#define UX_LOAD_WINDOW 8000000
#define DEFAULT_INPUT_BOOST_DURATION 1000
extern struct ux_sched_cputopo ux_sched_cputopo;
u64 ux_task_load[NR_CPUS] = {0};
u64 ux_load_ts[NR_CPUS] = {0};
int sysctl_slide_boost_enabled = 0;
int sysctl_boost_task_threshold = 51;
int sysctl_frame_rate = 60;


enum ibpt_type {
	/* input boost per-cpu trace util */
	IBPT_UTIL = 0,
	IBPT_LOAD = 1,
	/* new trace type should before it */
	IBPT_GUARD,
};

struct input_boost_percpu_trace {
	int prev_val[IBPT_GUARD];
};

static DEFINE_PER_CPU(struct input_boost_percpu_trace, ibpt);
static int ibt_scene_prev_val;

static noinline int tracing_mark_write(const char *buf)
{
	trace_printk(buf);
	return 0;
}

static void ib_per_cpu_systrace_c(enum ibpt_type index, int cpu, const char *str, int val)
{
	if (unlikely(global_debug_enabled & DEBUG_SYSTRACE)) {
		if (per_cpu(ibpt, cpu).prev_val[index] != val) {
			char buf[256];
			snprintf(buf, sizeof(buf), "C|10000|cpu%d_%s|%d\n", cpu, str, val);
			tracing_mark_write(buf);
			per_cpu(ibpt, cpu).prev_val[index] = val;
		}
	}
}

static void ib_systrace_c(const char *str, int val)
{
	if (unlikely(global_debug_enabled & DEBUG_SYSTRACE)) {
		if (ibt_scene_prev_val != val) {
			char buf[256];
			snprintf(buf, sizeof(buf), "C|10000|%s|%d\n", str, val);
			tracing_mark_write(buf);
			ibt_scene_prev_val = val;
		}
	}
}


void sched_assist_adjust_slide_param(unsigned int *maxtime) {
/* Give each scene with default boost value */
	if (sched_assist_scene(SA_SLIDE)) {
		if (sysctl_frame_rate <= 90)
			*maxtime = 5;
		else if (sysctl_frame_rate <= 120)
			*maxtime = 4;
		else
			*maxtime = 3;
	} else if (sched_assist_scene(SA_INPUT)) {
		if (sysctl_frame_rate <= 90)
			*maxtime = 8;
		else if (sysctl_frame_rate <= 120)
			*maxtime = 7;
		else
			*maxtime = 6;
	}
}

static u64 calc_freq_ux_load(struct task_struct *p, u64 wallclock)
{
	unsigned int maxtime = 5, factor = 0;
#ifdef CONFIG_OPLUS_SYSTEM_KERNEL_QCOM
	unsigned int window_size = sched_ravg_window / NSEC_PER_MSEC;
#else
	unsigned int window_size = walt_ravg_window / NSEC_PER_MSEC;
#endif
	u64 timeline = 0, freq_exec_load = 0, freq_ravg_load = 0;
	u64 wakeclock = p->last_wake_ts;

	if (wallclock < wakeclock)
		return 0;

	sched_assist_adjust_slide_param(&maxtime);
	timeline = wallclock - wakeclock;
	timeline = sf_union_ux_load(p, timeline);
	factor = window_size / maxtime;
	freq_exec_load = timeline * factor;
#ifdef CONFIG_OPLUS_SYSTEM_KERNEL_QCOM
	if (freq_exec_load > sched_ravg_window)
		freq_exec_load = sched_ravg_window;
#else
	if (freq_exec_load > walt_ravg_window)
		freq_exec_load = walt_ravg_window;
#endif
	freq_ravg_load = (p->ravg.prev_window + p->ravg.curr_window) << 1;
#ifdef CONFIG_OPLUS_SYSTEM_KERNEL_QCOM
	if (freq_ravg_load > sched_ravg_window)
		freq_ravg_load = sched_ravg_window;
#else
	if (freq_ravg_load > walt_ravg_window)
		freq_ravg_load = walt_ravg_window;
#endif
	return max(freq_exec_load, freq_ravg_load);
}

bool slide_boost_scene(void)
{
	return sysctl_slide_boost_enabled || sysctl_input_boost_enabled
		|| sched_assist_scene(SA_ANIM) || sched_assist_scene(SA_GPU_COMPOSITION);
}

bool slide_rt_boost(struct task_struct *p)
{
#ifdef CONFIG_OPLUS_UX_IM_FLAG
	if (p->ux_im_flag == IM_FLAG_SURFACEFLINGER || p->ux_im_flag == IM_FLAG_RENDERENGINE) {
			if (slide_boost_scene())
				return true;
	}
#endif
	return false;
}

void _slide_find_start_cpu(struct root_domain *rd, struct task_struct *p, int *start_cpu)
{
#ifdef CONFIG_OPLUS_SYSTEM_KERNEL_QCOM
	if (task_util(p) >= sysctl_boost_task_threshold ||
		scale_demand(p->ravg.sum) >= sysctl_boost_task_threshold) {
#else
	if (READ_ONCE(p->se.avg.util_avg) >= sysctl_boost_task_threshold ||
		scale_demand(p->ravg.sum) >= sysctl_boost_task_threshold) {
#endif
		if (ux_sched_cputopo.cls_nr > 2) {
			*start_cpu = cpumask_first(&ux_sched_cputopo.sched_cls[1].cpus);
		} else if (rd->max_cap_orig_cpu != -1) {
			*start_cpu = rd->max_cap_orig_cpu;
		}
	}
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0)
u64 _slide_get_boost_load(int cpu) {
	u64 timeline = 0;
#ifdef CONFIG_OPLUS_SYSTEM_KERNEL_QCOM
	u64 wallclock = sched_ktime_clock();
#else
	u64 wallclock = walt_ktime_clock();
#endif

	if ((sched_assist_scene(SA_SLIDE) || sched_assist_scene(SA_INPUT) ||
		sched_assist_scene(SA_LAUNCHER_SI) || sched_assist_scene(SA_ANIM))
		&& ux_task_load[cpu]) {
		timeline = wallclock - ux_load_ts[cpu];

		if (timeline >= UX_LOAD_WINDOW)
			ux_task_load[cpu] = 0;

		return ux_task_load[cpu];
	} else {
		return 0;
	}
}

#else /* LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0) */

u64 _slide_get_boost_load(int cpu)
{
	struct rq *rq = cpu_rq(cpu);

	if (rq->curr && (is_heavy_ux_task(rq->curr) || rq->curr->sched_class == &rt_sched_class)) {
		return ux_task_load[cpu];
	} else {
		return 0;
	}
}

#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0) */

#if defined(OPLUS_FEATURE_SCHED_ASSIST) && defined(CONFIG_SCHED_WALT)
#ifdef CONFIG_OPLUS_SYSTEM_KERNEL_QCOM
#define SCHED_CPUFREQ_RESET (1U << 7)
#define SCHED_CPUFREQ_BOOST (1U << 9)
#else
#define SCHED_CPUFREQ_WALT (1U << 4)
#define SCHED_CPUFREQ_RESET (1U << 7)
#define SCHED_CPUFREQ_BOOST (1U << 9)
#endif
#endif /* OPLUS_FEATURE_SCHED_ASSIST */

void adjust_sched_assist_input_ctrl() {
	if (!sysctl_input_boost_enabled)
		return;

	if (sysctl_slide_boost_enabled) {
		sysctl_input_boost_enabled = 0;
		sched_assist_input_boost_duration = 0;
		return;
	}

	if (jiffies_to_msecs(jiffies) < sched_assist_input_boost_duration) {
		return;
	} else {
		sched_assist_input_boost_duration = 0;
		sysctl_input_boost_enabled = 0;
	}
}

void slide_calc_boost_load(struct rq *rq, unsigned int *flag, int cpu)
{
	int is_slide, is_input, is_launcher_si, is_anim, sence_mask;
	bool is_target_case = false;
#ifdef CONFIG_OPLUS_SYSTEM_KERNEL_QCOM
	u64 wallclock = sched_ktime_clock();
#else
	u64 wallclock = walt_ktime_clock();
#endif
	is_slide = !!sched_assist_scene(SA_SLIDE);
	is_input = !!sched_assist_scene(SA_INPUT);
	is_launcher_si = !!sched_assist_scene(SA_LAUNCHER_SI);
	is_anim = !!sched_assist_scene(SA_ANIM);

	adjust_sched_assist_input_ctrl();

	if (is_slide || is_input || is_launcher_si || is_anim) {
		if(rq->curr && (is_heavy_ux_task(rq->curr) || rq->curr->sched_class == &rt_sched_class)) {
			ux_task_load[cpu] = calc_freq_ux_load(rq->curr, wallclock);
			ux_load_ts[cpu] = wallclock;
			*flag |= (SCHED_CPUFREQ_WALT | SCHED_CPUFREQ_BOOST);
			is_target_case = true;
		}
	}
	if (!is_target_case && ux_task_load[cpu]) {
		ux_task_load[cpu] = 0;
		*flag |= (SCHED_CPUFREQ_WALT | SCHED_CPUFREQ_BOOST);
	}

	sence_mask = is_input << 0 | is_slide << 1 | is_launcher_si << 2 | is_anim << 3;
	ib_systrace_c("ibt_boost_scene", sence_mask);
}

bool should_adjust_slide_task_placement(struct task_struct *p, int cpu)
{
	struct rq *rq = NULL;

	if (!check_slide_scene())
		return false;

	if (!is_heavy_ux_task(p))
		return false;

	/* slide task running on silver core with util over threshold, boost it */
	if (oplus_task_misfit(p, cpu))
		return true;

	/* slide task should not share it's core with another slide task */
	rq = cpu_rq(cpu);
	if (rq->curr && is_heavy_ux_task(rq->curr))
		return true;

	return false;
}

int sched_frame_rate_handler(struct ctl_table *table, int write, void __user *buffer, size_t *lenp, loff_t *ppos)
{
	int ret;

	if (write && *ppos)
		*ppos = 0;

	ret = proc_dointvec(table, write, buffer, lenp, ppos);

	return ret;
}

int sysctl_sched_assist_input_boost_ctrl_handler(struct ctl_table *table, int write,
	void __user *buffer, size_t *lenp, loff_t *ppos)
{
	int result;
	static DEFINE_MUTEX(sa_boost_mutex);

	mutex_lock(&sa_boost_mutex);
	result = proc_dointvec(table, write, buffer, lenp, ppos);
	if (!write)
		goto out;

	/* Orms write just write this proc to tell us update input window */
	sched_assist_input_boost_duration  = jiffies_to_msecs(jiffies) + DEFAULT_INPUT_BOOST_DURATION / sysctl_sched_assist_ib_duration_coedecay;
#ifndef CONFIG_MACH_MT6877
	sysctl_boost_task_threshold = 102;
#endif
out:
	mutex_unlock(&sa_boost_mutex);
	return result;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0)
void oplus_get_cpu_util_mtk(int cpu, u64 *walt_cpu_util, int *boosted)
{
	if (sysctl_sched_assist_enabled && (sched_assist_scene(SA_SLIDE) || sched_assist_scene(SA_LAUNCHER_SI) ||
		sched_assist_scene(SA_INPUT) || sched_assist_scene(SA_ANIM)) && ux_task_load[cpu]) {
		*boosted = true;
                *walt_cpu_util = cpu_rq(cpu)->prev_runnable_sum;
                slide_set_boost_load(walt_cpu_util, cpu);
                *walt_cpu_util <<= SCHED_CAPACITY_SHIFT;
#ifdef CONFIG_OPLUS_SYSTEM_KERNEL_QCOM
		do_div(*walt_cpu_util, sched_ravg_window);
#else
                do_div(*walt_cpu_util, walt_ravg_window);
#endif
	} else {
		return;
	}
}

#else /* LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0) */

void oplus_get_cpu_util_mtk(int cpu, int *util)
{
	u64 boost_load = 0;
	u64 orig_util = *util;

	boost_load = _slide_get_boost_load(cpu);
	/* scale runnable+running time(ns) to util */
	boost_load <<= SCHED_CAPACITY_SHIFT;
#ifdef CONFIG_OPLUS_SYSTEM_KERNEL_QCOM
	do_div(boost_load, sched_ravg_window);
#else
	do_div(boost_load, walt_ravg_window);
#endif
	*util = max_t(u64, boost_load, orig_util);

	ib_per_cpu_systrace_c(IBPT_LOAD, cpu, "boost_load", boost_load);
	/* util has too much trace，here only for compare, make util fellow no-zero boost_load */
	if (boost_load != 0) {
		ib_per_cpu_systrace_c(IBPT_UTIL, cpu, "util", orig_util);
	} else {
		ib_per_cpu_systrace_c(IBPT_UTIL, cpu, "util", 0);
	}
}

#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0) */
