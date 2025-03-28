// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020 Oplus. All rights reserved.
 */


#include <linux/seq_file.h>
#include <../drivers/android/binder_internal.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <trace/hooks/binder.h>
#include <linux/random.h>
#include <linux/of.h>

#if IS_ENABLED(CONFIG_OPLUS_FEATURE_SCHED_ASSIST)
#include <../kernel/oplus_cpu/sched/sched_assist/sa_common.h>
#endif
#include "binder_sched.h"
#define CREATE_TRACE_POINTS
#include "binder_sched_trace.h"
#if IS_ENABLED(CONFIG_ANDROID_BINDER_IPC_VIP_THREAD)
#include "vipthread/binder_vip.h"
#endif

static unsigned int bd_feature_enable = BD_FEATURE_ENABLE_DEFAULT;

struct kmem_cache *oplus_binder_struct_cachep = NULL;
struct kmem_cache *oplus_binder_proc_cachep = NULL;
unsigned int g_sched_enable = 1;
EXPORT_SYMBOL(g_sched_enable);
unsigned long long g_sched_debug = 0;

unsigned int g_async_ux_enable = 1;
unsigned int g_set_last_async_ux = 1;
unsigned int g_set_async_ux_after_pending = 1;
static unsigned int async_insert_queue = 1;
int sync_insert_queue = 0;
static int insert_limit[NUM_INSERT_MAX] = {0};
static unsigned int binder_ux_test = 0;
static unsigned int allow_accumulate_ux = 1;
unsigned int unset_ux_match_t = 1;

int fg_list_enable = 1;
int fg_list_async_first = 1;
static int max_works_in_fg = MAX_WORKS_IN_FGLIST;
static atomic64_t binder_work_seq;
static int fg_debug_pid = FG_DEBUG_DEFAULT_SYSTEM_SERVER;
static int fg_debug_interval = FG_DEBUG_INTERVAL_DEFAULT;
int fg_list_dynamic_enable = 1;

static int binder_ux_test_debug(void);

static inline bool binder_feature_enable(unsigned int feature_mask)
{
	return !!(bd_feature_enable & feature_mask);
}

static void binder_sched_dts_init(void)
{
	struct device_node *np = NULL;
	int feature_enable = 0;
	int ret = -1;

	np = of_find_node_by_name(NULL, "oplus_sync_ipc");
	if(np) {
		ret = of_property_read_u32(np, "disable", &feature_enable);
		if(ret)
			pr_info("oplus_sync_ipc no 'disable' prop");
		else
			bd_feature_enable = feature_enable;
	} else {
		pr_info("no oplus_sync_ipc node");
	}

	if(!binder_feature_enable(BD_FG_LIST_ENABLE_MASK)) {
		fg_list_enable = 0;
	}

	pr_info("bd_feature_enable: 0x%x, fg_list_enable: %d, ret: %d\n",
		bd_feature_enable, fg_list_enable, ret);
}

static inline struct oplus_binder_struct *alloc_oplus_binder_struct(void)
{
	if (!oplus_binder_struct_cachep) {
		return NULL;
	} else {
		return kmem_cache_alloc(oplus_binder_struct_cachep, GFP_ATOMIC);
	}
}

static inline void free_oplus_binder_struct(struct oplus_binder_struct *obs)
{
	if (!oplus_binder_struct_cachep || IS_ERR_OR_NULL(obs)) {
		return;
	} else {
		memset(obs, 0, sizeof(struct oplus_binder_struct));
		kmem_cache_free(oplus_binder_struct_cachep, obs);
	}
}

static inline struct oplus_binder_struct *get_oplus_binder_struct(
	struct binder_transaction *t, bool alloc)
{
	struct oplus_binder_struct *obs = NULL;

	if (IS_ERR_OR_NULL(t)) {
		return NULL;
	}

	obs = (struct oplus_binder_struct *)(t->android_oem_data1);
	if (!alloc) {
		trace_binder_get_obs(t, obs, alloc, "no alloc");
		return obs;
	} else {
		trace_binder_get_obs(t, obs, alloc, "before alloc");
		obs = alloc_oplus_binder_struct();
		t->android_oem_data1 = (unsigned long long)obs;
		trace_binder_get_obs(t, obs, alloc, "after alloc");
		return obs;
	}
}

static inline struct oplus_binder_proc *alloc_oplus_binder_proc(void)
{
	if (!oplus_binder_proc_cachep) {
		return NULL;
	} else {
		return kmem_cache_alloc(oplus_binder_proc_cachep, GFP_ATOMIC);
	}
}

static inline void free_oplus_binder_proc(struct oplus_binder_proc *obp)
{
	if (!oplus_binder_proc_cachep || IS_ERR_OR_NULL(obp)) {
		return;
	} else {
		memset(obp, 0, sizeof(struct oplus_binder_proc));
		kmem_cache_free(oplus_binder_proc_cachep, obp);
	}
}

static inline struct oplus_binder_proc *get_oplus_binder_proc(
	struct binder_proc *proc, bool alloc)
{
	struct oplus_binder_proc *obp = NULL;

	if (IS_ERR_OR_NULL(proc)) {
		return NULL;
	}

	obp = (struct oplus_binder_proc *)(proc->android_oem_data1);
	if (!alloc) {
		return obp;
	} else {
		obp = alloc_oplus_binder_proc();
		proc->android_oem_data1 = (unsigned long long)obp;
		return obp;
	}
}

static inline bool binder_is_sync_mode(u32 flags)
{
	return !(flags & TF_ONE_WAY);
}

static inline bool get_task_async_ux_sts(struct oplus_task_struct *ots)
{
	if (IS_ERR_OR_NULL(ots)) {
		return false;
	} else {
		return ots->binder_async_ux_sts;
	}
}

static inline void set_task_async_ux_sts(struct oplus_task_struct *ots, bool sts)
{
	if (IS_ERR_OR_NULL(ots)) {
		return;
	} else {
		ots->binder_async_ux_sts = sts;
	}
}

void set_task_async_ux_enable(pid_t pid, int enable)
{
	struct task_struct *task = NULL;
	struct oplus_task_struct *ots = NULL;
	bool rcu_lock = false;

	if (unlikely(!g_async_ux_enable)) {
		return;
	}
	if (enable >= ASYNC_UX_ENABLE_MAX) {
		trace_binder_set_get_ux(task, pid, enable, "set, enable error");
		return;
	}

	if (pid == CURRENT_TASK_PID) {
		task = current;
	} else {
		if (pid < 0 || pid > PID_MAX_DEFAULT) {
			trace_binder_set_get_ux(task, pid, enable, "set, pid error");
			return;
		}
		rcu_read_lock();
		rcu_lock = true;
		task = find_task_by_vpid(pid);
		if (IS_ERR_OR_NULL(task)) {
			trace_binder_set_get_ux(NULL, pid, enable, "set, task null");
			goto end;
		}
	}
	ots = get_oplus_task_struct(task);
	if (IS_ERR_OR_NULL(ots)) {
		trace_binder_set_get_ux(task, pid, enable, "set, ots null");
		goto end;
	}
	ots->binder_async_ux_enable = enable;

	trace_binder_set_get_ux(task, pid, enable, "set enable end");
	oplus_binder_debug(LOG_SET_ASYNC_UX, "(set_pid=%d task_pid=%d comm=%s) enable=%d ux_sts=%d set enable end\n",
		pid, task->pid, task->comm, ots->binder_async_ux_enable, ots->binder_async_ux_sts);

end:
	if (rcu_lock) {
		rcu_read_unlock();
	}
}

bool get_task_async_ux_enable(pid_t pid)
{
	struct task_struct *task = NULL;
	struct oplus_task_struct *ots = NULL;
	int enable = 0;

	if (unlikely(!g_async_ux_enable)) {
		return false;
	}

	if (pid == CURRENT_TASK_PID) {
		task = current;
	} else {
		if (pid < 0 || pid > PID_MAX_DEFAULT) {
			trace_binder_set_get_ux(task, pid, enable, "get, pid error");
			return false;
		}
		task = find_task_by_vpid(pid);
		if (IS_ERR_OR_NULL(task)) {
			trace_binder_set_get_ux(NULL, pid, enable, "get, task null");
			return false;
		}
	}

	ots = get_oplus_task_struct(task);
	if (IS_ERR_OR_NULL(ots)) {
		trace_binder_set_get_ux(task, pid, enable, "get, ots null");
		return false;
	}
	enable = ots->binder_async_ux_enable;
	trace_binder_set_get_ux(task, pid, enable, "get end");
	return enable;
}

void get_all_tasks_async_ux_enable(void)
{
	struct task_struct *p = NULL;
	struct task_struct *t = NULL;
	struct oplus_task_struct *ots = NULL;
	bool async_ux_task = false;

	for_each_process_thread(p, t) {
		ots = get_oplus_task_struct(t);
		if (IS_ERR_OR_NULL(ots)) {
			pr_info("[async_ux_tasks] ots err, pid=%d tgid=%d comm=%s async_ux_enable=unknown\n",
				t->pid, t->tgid, t->comm);
			trace_binder_set_get_ux(t, INVALID_VALUE, INVALID_VALUE, "[async_ux_tasks] ots err");
		} else if (ots->binder_async_ux_enable) {
			async_ux_task = true;
			pr_info("[async_ux_tasks] pid=%d tgid=%d comm=%s async_ux_enable=%d\n",
				t->pid, t->tgid, t->comm, ots->binder_async_ux_enable);
			trace_binder_set_get_ux(t, INVALID_VALUE, ots->binder_async_ux_enable, "[async_ux_tasks]");
		}
	}
	if (!async_ux_task) {
		pr_info("[async_ux_tasks] no async_ux task\n");
		trace_binder_set_get_ux(NULL, INVALID_VALUE, INVALID_VALUE,
			"[async_ux_tasks] no task");
	}
}

static inline bool is_sync_inherit_ux(struct binder_transaction *t)
{
	if (IS_ERR_OR_NULL(t) || IS_ERR_OR_NULL(t->from)) {
		return false;
	}

	if (test_set_inherit_ux(t->from->task) || test_task_is_rt(t->from->task)
		|| binder_ux_test_debug()) {
		return true;
	} else {
		return false;
	}
}

static inline bool is_sync_ux_enable(struct oplus_binder_struct *param_obs,
	struct binder_transaction *t)
{
	struct oplus_binder_struct *obs = param_obs;

	if (!IS_ERR_OR_NULL(obs)) {
		goto check_enable;
	}

	obs = get_oplus_binder_struct(t, false);
	if (IS_ERR_OR_NULL(obs)) {
		return false;
	}

check_enable:
	if (obs->sync_ux_enable == SYNC_UX_ENABLE) {
		return true;
	} else {
		return false;
	}
}

static inline bool is_async_ux_enable(struct oplus_binder_struct *param_obs,
	struct binder_transaction *t)
{
	struct oplus_binder_struct *obs = param_obs;

	if (!IS_ERR_OR_NULL(obs)) {
		goto check_enable;
	}

	obs = get_oplus_binder_struct(t, false);
	if (IS_ERR_OR_NULL(obs)) {
		return false;
	}

check_enable:
	if (obs->async_ux_enable == ASYNC_UX_DISABLE) {
		return false;
	} else {
		return true;
	}
}

static int list_count(struct list_head *head, int caller)
{
	struct list_head *pos = NULL;
	int count = 0;

	if (!head)
		return INVALID_VALUE;

	list_for_each(pos, head) {
		count++;
		if (g_sched_debug & LOG_DUMP_LIST_MEMBER) {
			pr_info("count: %d, pos: %px, head: %px, caller: %d\n", count, pos, head, caller);
		}
	}
	return count;
}

static inline bool is_task_servicemg(struct task_struct *task)
{
	if (IS_ERR_OR_NULL(task)) {
		return false;
	}
	if (!strncmp(task->comm, "servicemanager", TASK_COMM_LEN)
				|| !strncmp(task->comm, "hwservicemanage", TASK_COMM_LEN)
				|| !strncmp(task->comm, "vndservicemanag", TASK_COMM_LEN)) {
		return true;
	} else {
		return false;
	}
}

static inline bool is_task_system_server(struct task_struct *task)
{
	if (IS_ERR_OR_NULL(task)) {
		return false;
	}
	if (!strncmp(task->comm, SYSTEM_SERVER_NAME, TASK_COMM_LEN)) {
		return true;
	} else {
		return false;
	}
}

static inline void set_sync_t_ux_state(struct binder_transaction *t, bool enable,
	bool sync, bool is_servicemg)
{
	struct oplus_binder_struct *obs = NULL;

	if (!unset_ux_match_t || !sync || !t || !is_servicemg) {
		return;
	}
	obs = get_oplus_binder_struct(t, false);
	if (IS_ERR_OR_NULL(obs)) {
		return;
	}
	if (enable) {
		obs->t_ux_state = T_IS_SYNC_UX;
		binder_ux_state_systrace(current, NULL, STATE_SET_T_UX_STATE,
			LOG_BINDER_SYSTRACE_LVL0, t, NULL);
	} else {
		obs->t_ux_state = T_NOT_SYNC_UX;
		binder_ux_state_systrace(current, NULL, STATE_UNSET_T_UX_STATE,
			LOG_BINDER_SYSTRACE_LVL0, t, NULL);
	}
}

static inline bool is_sync_t_ux_state(struct binder_transaction *t,
	bool sync, bool is_servicemg)
{
	struct oplus_binder_struct *obs = NULL;

	/* default return true */
	if (!unset_ux_match_t || !sync || !t || !is_servicemg) {
		return true;
	}

	obs = get_oplus_binder_struct(t, false);
	/* default return true */
	if (IS_ERR_OR_NULL(obs)) {
		return true;
	}
	if (obs->t_ux_state == T_NOT_SYNC_UX) {
		return false;
	} else {
		return true;
	}
}

static inline bool is_fglist_debug_enable(void)
{
	if (g_sched_debug & LOG_FG_LIST_LVL0) {
		return true;
	} else {
		return false;
	}
}

static inline bool is_fglist_debug_process(struct binder_proc *proc)
{
	if (!is_fglist_debug_enable() || IS_ERR_OR_NULL(proc)
		 || IS_ERR_OR_NULL(proc->tsk)) {
		return false;
	}

	if ((fg_debug_pid == FG_DEBUG_DEFAULT_SYSTEM_SERVER
		&& is_task_system_server(proc->tsk))
		|| (fg_debug_pid == proc->tsk->pid)) {
		return true;
	} else {
		return false;
	}
}

static inline void set_work_seq(struct binder_work *w)
{
	long long seq = 0;

	if (!fg_list_enable || IS_ERR_OR_NULL(w)) {
		return;
	}

	seq = atomic64_inc_return(&binder_work_seq);
	if (seq > (LLONG_MAX - 1)) {
		/* TODO: how to handle it when select work */
		seq = 1;
		atomic64_set(&binder_work_seq, seq);
	}
	w->android_oem_data1 = seq;
}

static inline long long get_work_seq(struct binder_work *w)
{
	if (!fg_list_enable || IS_ERR_OR_NULL(w)) {
		return 0;
	}
	return w->android_oem_data1;
}

static inline void android_vh_binder_list_add_work_handler(void *unused,
	struct binder_work *w, struct list_head *target_list)
{
	if (!fg_list_enable || IS_ERR_OR_NULL(w)) {
		return;
	}
	set_work_seq(w);
}

static inline bool is_vip_binder_thread(struct binder_thread *thread)
{
	bool vip_thread = false;

	if (IS_ERR_OR_NULL(thread)) {
		return false;
	}
	/*
	vip_thread = !!(thread->looper &
		 BINDER_LOOPER_TAXI_THREAD_STATE_REGISTERED);
	*/
	return vip_thread;
}

static void save_fglist_debug_info(struct binder_proc *proc,
	enum FG_LIST_DEBUG_ITEM item, bool print)
{
	static int sync_ux_nothread = 0;
	static int add_to_fg = 0;
	static int select_fg = 0;
	static int select_proc = 0;
	static int fg_works_overflow = 0;
	static int select_fg_directly = 0;
	static int select_proc_other_type = 0;
	static int select_fg_comp_seq = 0;
	static int select_continue_over = 0;
	static int select_fg_proc_empty = 0;
	static int select_proc_when_ux = 0;
	static int select_proc_when_async = 0;
	static int select_proc_comp_seq = 0;
	static int fg_debug_pid_pre = FG_DEBUG_DEFAULT_SYSTEM_SERVER;
	struct task_struct *pre_task = NULL;

	if (!is_fglist_debug_enable() || IS_ERR_OR_NULL(proc)) {
		return;
	}

	if (fg_debug_pid != fg_debug_pid_pre) {
		pre_task = find_task_by_vpid(fg_debug_pid_pre);
		oplus_binder_debug(LOG_FG_LIST_LVL0, "[BINDER_FG] [pre %d: %s] sync_ux_nothread = %d \
			add_to_fg = %d select_fg = %d select_proc = %d select_fg_directly = %d \
			fg_works_overflow = %d select_fg_comp_seq = %d select_proc_comp_seq = %d \
			select_fg_proc_empty = %d select_proc_when_ux = %d select_proc_when_async = %d\n",
			fg_debug_pid_pre, (pre_task ? pre_task->comm : NULL), sync_ux_nothread, add_to_fg,
			select_fg, select_proc, select_fg_directly, fg_works_overflow,
			select_fg_comp_seq, select_proc_comp_seq,
			select_fg_proc_empty, select_proc_when_ux, select_proc_when_async);

		fg_debug_pid_pre = fg_debug_pid;
		sync_ux_nothread = 0;
		add_to_fg = 0;
		select_fg = 0;
		select_proc = 0;
		fg_works_overflow = 0;
		select_fg_directly = 0;
		select_proc_other_type = 0;
		select_fg_comp_seq = 0;
		select_continue_over = 0;
		select_fg_proc_empty = 0;
		select_proc_when_ux = 0;
		select_proc_comp_seq = 0;
	}

	if (!is_fglist_debug_process(proc)) {
		return;
	}

	if (!print) {
		switch(item) {
		case ITEM_SYNC_UX_NOTHREAD:
			sync_ux_nothread++;
			break;
		case ITEM_ADD_TO_FG:
			add_to_fg++;
			break;
		case ITEM_SELECT_FG:
			select_fg++;
			break;
		case ITEM_SELECT_PROC:
			select_proc++;
			break;
		case ITEM_FG_WORKS_OVERFLOW:
			fg_works_overflow++;
			break;
		case ITEM_SELECT_FG_DIRECTLY:
			select_fg_directly++;
			break;
		case ITEM_SELECT_PROC_OTHER_TYPE:
			select_proc_other_type++;
			break;
		case ITEM_SELECT_FG_COMPARE_SEQ:
			select_fg_comp_seq++;
			break;
		case ITEM_SELECT_PROC_COMPARE_SEQ:
			select_proc_comp_seq++;
			break;
		case ITEM_SELECT_CONTINUE_COUNT_OVER:
			select_continue_over++;
			break;
		case ITEM_SELECT_FG_PROC_EMPTY:
			select_fg_proc_empty++;
			break;
		case ITEM_SELECT_PROC_WHEN_UX:
			select_proc_when_ux++;
			break;
		case ITEM_SELECT_PROC_WHEN_ASYNC:
			select_proc_when_async++;
			break;
		default:
			break;
		}
	} else {
		oplus_binder_debug(LOG_FG_LIST_LVL0, "[BINDER_FG] proc[%d: %s] sync_ux_nothread = %d \
			add_to_fg = %d select_fg = %d select_proc = %d select_fg_directly = %d \
			fg_works_overflow = %d select_fg_comp_seq = %d select_proc_comp_seq = %d \
			select_fg_proc_empty = %d select_proc_when_ux = %d select_proc_when_async = %d\n",
			proc->tsk->pid, proc->tsk->comm, sync_ux_nothread, add_to_fg, select_fg,
			select_proc, select_fg_directly, fg_works_overflow, select_fg_comp_seq,
			select_proc_comp_seq, select_fg_proc_empty, select_proc_when_ux, select_proc_when_async);
	}
}

static int binder_ux_test_debug(void)
{
	static unsigned int count = 0;
	unsigned int remainder = 0;
	int ret = 0;

	if (binder_ux_test == BINDER_UX_TEST_DISABLE) {
		return 0;
	}

	switch(binder_ux_test) {
	case ASYNC_UX_RANDOM_LOW_INSERT_TEST:
		get_random_bytes(&count, sizeof(unsigned int));
		remainder = (count % 6);
		if (remainder == 0)
			ret = ASYNC_UX_DISABLE;
		else if (remainder == 1)
			ret = ASYNC_UX_ENABLE_ENQUEUE;
		else if (remainder == 2)
			ret = ASYNC_UX_ENABLE_INSERT_QUEUE;
		break;
	case ASYNC_UX_RANDOM_HIGH_INSERT_TEST:
		get_random_bytes(&count, sizeof(unsigned int));
		remainder = (count % 3);
		if (remainder == 0)
			ret = ASYNC_UX_DISABLE;
		else if (remainder == 1)
			ret = ASYNC_UX_ENABLE_ENQUEUE;
		else if (remainder == 2)
			ret = ASYNC_UX_ENABLE_INSERT_QUEUE;
		break;
	case ASYNC_UX_RANDOM_LOW_ENQUEUE_TEST:
		get_random_bytes(&count, sizeof(unsigned int));
		remainder = (count % 5);
		if (remainder == 0)
			ret = ASYNC_UX_ENABLE_ENQUEUE;
		else
			ret = ASYNC_UX_DISABLE;
		break;
	case ASYNC_UX_RANDOM_HIGH_ENQUEUE_TEST:
		get_random_bytes(&count, sizeof(unsigned int));
		remainder = (count % 2);
		if (remainder == 0)
			ret = ASYNC_UX_ENABLE_ENQUEUE;
		else
			ret = ASYNC_UX_DISABLE;
		break;
	case ASYNC_UX_INORDER_TEST:
		count++;
		remainder = count % 10;
		if (remainder == 1 || remainder == 5) {
			ret = ASYNC_UX_ENABLE_ENQUEUE;
		} else if (remainder == 2 || remainder == 6 || remainder == 8) {
			ret = ASYNC_UX_ENABLE_INSERT_QUEUE;
		} else {
			ret = ASYNC_UX_DISABLE;
		}
		break;
	case SYNC_UX_RANDOM_LOW_TEST:
		get_random_bytes(&count, sizeof(unsigned int));
		remainder = (count % 5);
		if (remainder == 0)
			ret = 1;
		else
			ret = 0;
		break;
	case SYNC_UX_RANDOM_HIGH_TEST:
		get_random_bytes(&count, sizeof(unsigned int));
		remainder = (count % 2);
		if (!remainder)
			ret = 1;
		else
			ret = 0;
		break;
	default:
		ret = 0;
		break;
	}
	return ret;
}

noinline int tracing_mark_write(const char *buf)
{
	trace_printk(buf);
	return 0;
}

void binder_ux_state_systrace(struct task_struct *from, struct task_struct *target,
	int ux_state, int systrace_lvl, struct binder_transaction *t, struct binder_proc *proc)
{
	bool lvl0_enable = false;
	bool lvl1_enable = false;
	int from_pid = 0;
	int target_pid = 0;

	if (g_sched_debug & LOG_BINDER_SYSTRACE_LVL0) {
		lvl0_enable = true;
	}
	if (g_sched_debug & LOG_BINDER_SYSTRACE_LVL1) {
		lvl1_enable = true;
	}
	if (!lvl0_enable && !lvl1_enable) {
		return;
	} else if ((systrace_lvl == LOG_BINDER_SYSTRACE_LVL1) && !lvl1_enable) {
		return;
	} else {
		char buf[128] = {0};
		if (!IS_ERR_OR_NULL(from)) {
			from_pid = from->pid;
		}
		if (!IS_ERR_OR_NULL(target)) {
			target_pid = target->pid;
		}
		if ((g_sched_debug & LOG_BINDER_SYSTRACE_STATUS)) {
			unsigned long long inherit_ux = 0;
			int ux_type = INVALID_VALUE;
			int real_ux_state = INVALID_VALUE;
			int ux_depth = INVALID_VALUE;
			int proc_pid = INVALID_VALUE;
			int waiting_threads = INVALID_VALUE;
			int requested_threads = INVALID_VALUE;
			int requested_threads_started = INVALID_VALUE;
			int max_threads = INVALID_VALUE;
			struct oplus_binder_struct *obs = NULL;
			int async_ux_enable = INVALID_VALUE;
			int sync_ux_enable = INVALID_VALUE;
			int t_ux_state = INVALID_VALUE;

			if (target) {
				inherit_ux = oplus_get_inherit_ux(target);
				ux_type = get_ux_state_type(target);
				real_ux_state = oplus_get_ux_state(target);
				ux_depth = oplus_get_ux_depth(target);
			}
			if (proc) {
				proc_pid = proc->tsk->pid;
				waiting_threads = list_count(&proc->waiting_threads, 0);
				requested_threads = proc->requested_threads;
				requested_threads_started = proc->requested_threads_started;
				max_threads = proc->max_threads;
			}
			if (t) {
				obs = get_oplus_binder_struct(t, false);
				if (!IS_ERR_OR_NULL(obs)) {
					async_ux_enable = obs->async_ux_enable;
					sync_ux_enable = obs->sync_ux_enable;
					t_ux_state = obs->t_ux_state;
				}
			}

			memset(buf, 0, sizeof(buf));
			snprintf(buf, sizeof(buf), "C|9999|z_binder_inherit_ux|%lld\n", inherit_ux);
			tracing_mark_write(buf);
			memset(buf, 0, sizeof(buf));
			snprintf(buf, sizeof(buf), "C|9999|z_binder_ux_type|%d\n", ux_type);
			tracing_mark_write(buf);
			memset(buf, 0, sizeof(buf));
			snprintf(buf, sizeof(buf), "C|9999|z_binder_real_ux_state|%d\n", real_ux_state);
			tracing_mark_write(buf);
			memset(buf, 0, sizeof(buf));
			snprintf(buf, sizeof(buf), "C|9999|z_binder_ux_depth|%d\n", ux_depth);
			tracing_mark_write(buf);

			memset(buf, 0, sizeof(buf));
			snprintf(buf, sizeof(buf), "C|9999|z_binder_proc_pid|%d\n", proc_pid);
			tracing_mark_write(buf);
			memset(buf, 0, sizeof(buf));
			snprintf(buf, sizeof(buf), "C|9999|z_binder_waiting_threads|%d\n", waiting_threads);
			tracing_mark_write(buf);
			memset(buf, 0, sizeof(buf));
			snprintf(buf, sizeof(buf), "C|9999|z_binder_requested_threads|%d\n", requested_threads);
			tracing_mark_write(buf);
			memset(buf, 0, sizeof(buf));
			snprintf(buf, sizeof(buf), "C|9999|z_binder_requested_threads_started|%d\n", requested_threads_started);
			tracing_mark_write(buf);
			memset(buf, 0, sizeof(buf));
			snprintf(buf, sizeof(buf), "C|9999|z_binder_max_threads|%d\n", max_threads);
			tracing_mark_write(buf);
			memset(buf, 0, sizeof(buf));
			snprintf(buf, sizeof(buf), "C|9999|z_binder_async_ux_enable|%d\n", async_ux_enable);
			tracing_mark_write(buf);
			memset(buf, 0, sizeof(buf));
			snprintf(buf, sizeof(buf), "C|9999|z_binder_sync_ux_enable|%d\n", sync_ux_enable);
			tracing_mark_write(buf);
			memset(buf, 0, sizeof(buf));
			snprintf(buf, sizeof(buf), "C|9999|z_binder_t_ux_state|%d\n", t_ux_state);
			tracing_mark_write(buf);
		}

		snprintf(buf, sizeof(buf), "C|9999|z_binder_from|%d\n", from_pid);
		tracing_mark_write(buf);
		memset(buf, 0, sizeof(buf));
		snprintf(buf, sizeof(buf), "C|9999|z_binder_target|%d\n", target_pid);
		tracing_mark_write(buf);
		memset(buf, 0, sizeof(buf));
		snprintf(buf, sizeof(buf), "C|9999|z_binder_ux_state|%d\n", ux_state);
		tracing_mark_write(buf);
		memset(buf, 0, sizeof(buf));
		if (IS_ERR_OR_NULL(t)) {
			snprintf(buf, sizeof(buf), "C|9999|z_binder_vt_id|0\n");
		} else {
			snprintf(buf, sizeof(buf), "C|9999|z_binder_vt_id|%d\n", t->debug_id);
		}
		tracing_mark_write(buf);
	}
}

#if IS_ENABLED(CONFIG_OPLUS_FEATURE_SCHED_ASSIST)

static void sync_binder_set_inherit_ux(struct task_struct *thread_task, struct task_struct *from_task,
	bool sync, bool is_servicemg, struct binder_transaction *t, struct binder_proc *proc)
{
	int from_depth = oplus_get_ux_depth(from_task);
	int from_state = oplus_get_ux_state(from_task);
	int type = get_ux_state_type(thread_task);
	unsigned long long inherit_ux = 0;

	if (type != UX_STATE_NONE && type != UX_STATE_INHERIT) {
		trace_binder_inherit_ux(from_task, thread_task, from_depth, from_state,
			type, INVALID_VALUE, sync, "sync_set type not expected");
		binder_ux_state_systrace(current, thread_task, STATE_SYNC_TYPE_UNEXPECTED,
			LOG_BINDER_SYSTRACE_LVL1, t, proc);
		return;
	}
	if (from_task && test_set_inherit_ux(from_task)) {
		inherit_ux = oplus_get_inherit_ux(thread_task);
		if (!test_inherit_ux(thread_task, INHERIT_UX_BINDER)) {
			set_inherit_ux(thread_task, INHERIT_UX_BINDER, from_depth, from_state);
			set_sync_t_ux_state(t, true, true, is_servicemg);
			trace_binder_inherit_ux(from_task, thread_task, from_depth, from_state,
				type, INVALID_VALUE, sync, "sync_set ux set");
			binder_ux_state_systrace(current, thread_task, STATE_SYNC_SET_UX, LOG_BINDER_SYSTRACE_LVL0, t, proc);
			oplus_binder_debug(LOG_SET_SYNC_UX, "sync_set ux set, from(pid = %d comm = %s) target(pid = %d comm = %s)\n",
				from_task->pid, from_task->comm, thread_task->pid, thread_task->comm);
		} else if (allow_accumulate_ux && is_servicemg && inherit_ux > 0 && inherit_ux < MAX_ACCUMULATED_UX) {
			set_inherit_ux(thread_task, INHERIT_UX_BINDER, from_depth, from_state);
			set_sync_t_ux_state(t, true, true, is_servicemg);

			binder_ux_state_systrace(current, thread_task, STATE_SYNC_SET_UX_AGAIN_SERVICEMG, LOG_BINDER_SYSTRACE_LVL0, t, proc);
		} else {
			reset_inherit_ux(thread_task, from_task, INHERIT_UX_BINDER);
			set_sync_t_ux_state(t, true, true, is_servicemg);

			if (is_servicemg) {
				binder_ux_state_systrace(current, thread_task, STATE_SYNC_RESET_UX_SERVICEMG, LOG_BINDER_SYSTRACE_LVL0, t, proc);
			} else {
				binder_ux_state_systrace(current, thread_task, STATE_SYNC_RESET_UX, LOG_BINDER_SYSTRACE_LVL0, t, proc);
			}
		}
	}  else if (from_task && test_task_is_rt(from_task)) { /* rt trans can be set as ux if binder thread is cfs class */
		inherit_ux = oplus_get_inherit_ux(thread_task);
		if (!test_inherit_ux(thread_task, INHERIT_UX_BINDER)) {
			set_inherit_ux(thread_task, INHERIT_UX_BINDER, from_depth, SA_TYPE_LIGHT);
			set_sync_t_ux_state(t, true, true, is_servicemg);

			trace_binder_inherit_ux(from_task, thread_task, from_depth,
				from_state, type, INVALID_VALUE, sync, "sync_set ux rt");
			binder_ux_state_systrace(current, thread_task, STATE_SYNC_RT_SET_UX, LOG_BINDER_SYSTRACE_LVL0, t, proc);
		} else if (allow_accumulate_ux && is_servicemg && inherit_ux > 0 && inherit_ux < MAX_ACCUMULATED_UX) {
			set_inherit_ux(thread_task, INHERIT_UX_BINDER, from_depth, SA_TYPE_LIGHT);
			set_sync_t_ux_state(t, true, true, is_servicemg);

			binder_ux_state_systrace(current, thread_task, STATE_SYNC_SET_UX_AGAIN_SERVICEMG, LOG_BINDER_SYSTRACE_LVL0, t, proc);
		} else {
			trace_binder_inherit_ux(from_task, thread_task, from_depth,
				from_state, type, INVALID_VALUE, sync, "sync_set rt none");
			if (is_servicemg) {
				binder_ux_state_systrace(current, thread_task, STATE_SYNC_RT_NOT_SET_SERVICEMG, LOG_BINDER_SYSTRACE_LVL0, t, proc);
			} else {
				binder_ux_state_systrace(current, thread_task, STATE_SYNC_RT_NOT_SET, LOG_BINDER_SYSTRACE_LVL0, t, proc);
			}
		}
	} else {
		trace_binder_inherit_ux(from_task, thread_task, from_depth, from_state,
			type, INVALID_VALUE, sync, "sync_set end do nothing");
		binder_ux_state_systrace(current, thread_task, STATE_SYNC_NOT_SET, LOG_BINDER_SYSTRACE_LVL1, t, proc);
	}
}

static void async_binder_set_inherit_ux(struct task_struct *thread_task,
	struct task_struct *from_task, bool sync, struct binder_transaction *t, struct binder_proc *proc)
{
	struct oplus_task_struct *ots = NULL;
	int type = 0;
	int ux_value = 0;

	if (unlikely(!g_async_ux_enable)) {
		return;
	}

	if (!thread_task) {
		return;
	}

	type = get_ux_state_type(thread_task);
	if (type != UX_STATE_NONE && type != UX_STATE_INHERIT) {
		trace_binder_inherit_ux(from_task, thread_task, INVALID_VALUE, INVALID_VALUE,
			type, INVALID_VALUE, sync, "async_set type not expected");
		return;
	}

	ots = get_oplus_task_struct(thread_task);
	if (unlikely(IS_ERR_OR_NULL(ots))) {
		return;
	}
	/*
	if (get_task_async_ux_sts(ots)) {
		trace_binder_inherit_ux(from_task, thread_task, INVALID_VALUE, INVALID_VALUE,
			type, INVALID_VALUE, sync, "async_set_inherit_ux ux_sts true");
		return;
	}
	*/
	trace_binder_inherit_ux(from_task, thread_task, ots->ux_depth, ots->ux_state,
		type, ots->binder_async_ux_sts, sync, "async_set before set");

	set_task_async_ux_sts(ots, true);
	ux_value = (ots->ux_state | SA_TYPE_HEAVY);
	set_inherit_ux(thread_task, INHERIT_UX_BINDER, ots->ux_depth, ux_value);

	trace_binder_inherit_ux(from_task, thread_task, ots->ux_depth, ots->ux_state,
		type, ots->binder_async_ux_sts, sync, "async_set after set");

	oplus_binder_debug(LOG_SET_ASYNC_UX, "async_set_ux after set, thread(pid=%d tgid=%d comm=%s) ux_sts=%d ux_state=%d ux_depth=%d inherit_ux=%lld\n",
		thread_task->pid, thread_task->tgid, thread_task->comm,
		ots->binder_async_ux_sts, ots->ux_state, ots->ux_depth, atomic64_read(&ots->inherit_ux));
	binder_ux_state_systrace(current, thread_task, STATE_ASYNC_SET_UX, LOG_BINDER_SYSTRACE_LVL0, t, proc);
}

static void binder_set_inherit_ux(struct task_struct *thread_task,
	struct task_struct *from_task, bool sync, bool is_servicemg,
	struct binder_transaction *t, struct binder_proc *proc)
{
	if (sync) {
		sync_binder_set_inherit_ux(thread_task, from_task, sync, is_servicemg, t, proc);
	} else {
		async_binder_set_inherit_ux(thread_task, from_task, sync, t, proc);
	}
}

static void binder_unset_inherit_ux(struct task_struct *thread_task,
	int unset_type, struct binder_transaction *t, struct binder_proc *proc)
{
	struct oplus_task_struct *ots = get_oplus_task_struct(thread_task);
	bool is_servicemg = false;

	if (test_inherit_ux(thread_task, INHERIT_UX_BINDER)) {
		if (!IS_ERR_OR_NULL(ots)) {
			trace_binder_inherit_ux(NULL, thread_task, ots->ux_depth, ots->ux_state,
				INVALID_VALUE, ots->binder_async_ux_sts,
				unset_type, "unset_ux before unset");
		}

		if (unset_ux_match_t && (unset_type == SYNC_UNSET)) {
			is_servicemg = is_task_servicemg(thread_task);
			if (!is_sync_t_ux_state(t, true, is_servicemg)) {
				binder_ux_state_systrace(current, thread_task, STATE_SYNC_T_NOT_UNSET_UX, LOG_BINDER_SYSTRACE_LVL0, t, proc);
				return;
			}
			set_sync_t_ux_state(t, false, true, is_servicemg);
		}
		unset_inherit_ux(thread_task, INHERIT_UX_BINDER);
		if (!IS_ERR_OR_NULL(ots)) {
			if (unset_type == SYNC_OR_ASYNC_UNSET) {
				set_task_async_ux_sts(ots, false);
			}
			trace_binder_inherit_ux(NULL, thread_task, ots->ux_depth, ots->ux_state,
				INVALID_VALUE, ots->binder_async_ux_sts, unset_type, "unset_ux after unset");
			oplus_binder_debug(LOG_SET_ASYNC_UX, "sync || async_unset_ux after unset, thread(pid = %d tgid = %d comm = %s) \
				 ots_enable = %d ux_sts = %d ux_state = %d ux_depth = %d inherit_ux = %lld\n",
				thread_task->pid, thread_task->tgid, thread_task->comm, ots->binder_async_ux_enable,
				ots->binder_async_ux_sts, ots->ux_state, ots->ux_depth, atomic64_read(&ots->inherit_ux));
		}
		binder_ux_state_systrace(current, thread_task, STATE_SYNC_OR_ASYNC_UNSET_UX, LOG_BINDER_SYSTRACE_LVL0, t, proc);
	} else {
		trace_binder_inherit_ux(NULL, thread_task, INVALID_VALUE, INVALID_VALUE,
			INVALID_VALUE, INVALID_VALUE, unset_type, "unset_ux do nothing");
	}
}

#else /* CONFIG_OPLUS_FEATURE_SCHED_ASSIST */
static void binder_set_inherit_ux(struct task_struct *thread_task, struct task_struct *from_task,
	bool sync, bool is_servicemg, struct binder_transaction *t, struct binder_proc *proc)
{
}

static void binder_unset_inherit_ux(struct task_struct *thread_task,
	int unset_type, struct binder_transaction *t, struct binder_proc *proc)
{
}
#endif

/* implement vender hook in driver/android/binder.c */
void android_vh_binder_restore_priority_handler(void *unused,
	struct binder_transaction *t, struct task_struct *task)
{
	if (unlikely(!g_sched_enable))
		return;

	/* Google commit "d1367b5" caused this priority pass issue on our kernel-5.15 project */
#if (LINUX_VERSION_CODE > KERNEL_VERSION(5, 15, 0))
	if (t != NULL) {
		struct binder_priority *sp = &t->saved_priority;
		if (task->prio < MAX_RT_PRIO && !sp->prio && !sp->sched_policy) {
			sp->sched_policy = task->policy;
			sp->prio = task->prio;
		}
	}
#endif

	if (!is_task_servicemg(task)) {
		return;
	}

	if (t) {
		trace_binder_ux_task(1, INVALID_VALUE, INVALID_VALUE, task,
			INVALID_VALUE, t, NULL, "sync_ux unset binder_reply");
		binder_unset_inherit_ux(task, SYNC_UNSET, t, NULL);
	} else {
		trace_binder_ux_task(1, INVALID_VALUE, INVALID_VALUE, task,
			INVALID_VALUE, t, NULL, "sync_ux unset restore_prio t NULL");
		binder_unset_inherit_ux(task, SYNC_OR_ASYNC_UNSET, t, NULL);
	}
}

void android_vh_binder_wait_for_work_handler(void *unused,
			bool do_proc_work, struct binder_thread *tsk, struct binder_proc *proc)
{
	if (unlikely(!g_sched_enable))
		return;

	if (do_proc_work) {
		trace_binder_ux_task(1, INVALID_VALUE, INVALID_VALUE, tsk->task, INVALID_VALUE,
			NULL, NULL, "sync_ux unset wait_for_work");
		binder_unset_inherit_ux(tsk->task, SYNC_OR_ASYNC_UNSET, NULL, proc);
	}
}

void android_vh_sync_txn_recvd_handler(void *unused,
	struct task_struct *tsk, struct task_struct *from)
{
	if (unlikely(!g_sched_enable))
		return;

	trace_binder_ux_task(1, INVALID_VALUE, INVALID_VALUE, tsk, INVALID_VALUE,
		NULL, NULL, "sync_ux set txn_recvd");
	binder_set_inherit_ux(tsk, from, true, false, NULL, NULL);
}

static bool is_allow_sf_binder_ux(struct task_struct *task)
{
	struct oplus_task_struct *ots = NULL;

	ots = get_oplus_task_struct(task);
	if (!IS_ERR_OR_NULL(ots) && test_bit(IM_FLAG_SURFACEFLINGER, &ots->im_flag)) {
		return true;
	} else {
		return false;
	}
}

static void android_vh_alloc_oem_binder_struct_handler(void *unused,
	struct binder_transaction_data *tr, struct binder_transaction *t, struct binder_proc *target_proc)
{
	struct oplus_binder_struct *obs = NULL;
	struct oplus_task_struct *ots = NULL;
	int async_ux_enable = 0, test_debug = 0;

	if (unlikely(!g_sched_enable) || unlikely(!g_async_ux_enable)) {
		return;
	}
	if (IS_ERR_OR_NULL(tr) || IS_ERR_OR_NULL(t)) {
		trace_binder_ux_enable(current, async_ux_enable, t,
			obs, "tr_buf t/tr err");
		return;
	}

	obs = get_oplus_binder_struct(t, true);
	if (IS_ERR_OR_NULL(obs)) {
		return;
	}
	memset(obs, 0, sizeof(struct oplus_binder_struct));

	if (binder_is_sync_mode(tr->flags)) {
		if (is_sync_inherit_ux(t)) {
			obs->sync_ux_enable = SYNC_UX_ENABLE;
		} else {
			obs->sync_ux_enable = SYNC_UX_DISABLE;
		}
		return;
	}

	ots = get_oplus_task_struct(current);
	if (IS_ERR_OR_NULL(ots)) {
		trace_binder_ux_enable(current, INVALID_VALUE, t,
			obs, "ots null");
		return;
	}

	if (ots->binder_async_ux_enable) {
		async_ux_enable = ots->binder_async_ux_enable;
	} else if (is_allow_sf_binder_ux(current)) {
		async_ux_enable = 1;
		binder_ux_state_systrace(current, NULL,
			STATE_SF_ASYNC_IS_UX, LOG_BINDER_SYSTRACE_LVL0, t, NULL);
	}
	test_debug = binder_ux_test_debug();
	if (async_ux_enable || test_debug) {
			obs->async_ux_enable = async_ux_enable ? async_ux_enable : test_debug;
			trace_binder_ux_enable(current, obs->async_ux_enable, t,
				obs, "tr async_ux enable");
	} else {
		obs->async_ux_enable = ASYNC_UX_DISABLE;
		trace_binder_ux_enable(current, async_ux_enable, t,
				obs, "tr async_ux disable");
	}
}

static void set_binder_thread_node(struct binder_transaction *t,
	struct task_struct *task, struct binder_buffer *buffer, bool sync, bool reset)
{
	struct oplus_task_struct *ots = NULL;
	struct binder_node *node = NULL;
	bool set_node = false;

	if (unlikely(!g_sched_enable) || unlikely(!g_async_ux_enable) || (!g_set_last_async_ux)) {
		return;
	}
	if (sync) {	/* don't use t->flags here because t maybe NULL */
		return;
	}
	if (IS_ERR_OR_NULL(task)) {
		return;
	}

	if (t && !IS_ERR_OR_NULL(t->buffer)) {
		node = t->buffer->target_node;
	}
	ots = get_oplus_task_struct(task);
	if (!IS_ERR_OR_NULL(ots)) {
		oplus_binder_debug(LOG_TRACK_LAST_ASYNC, "before set, thread(pid=%d tgid=%d comm=%s) sync: %d, reset: %d, ots_node: 0x%llx, node: 0x%llx\n",
			task->pid, task->tgid, task->comm, sync, reset, (unsigned long long)ots->binder_thread_node, (unsigned long long)node);
		if (reset) {
			ots->binder_thread_node = NULL;
			set_node = true;
			trace_set_thread_node(task, NULL, sync, "async reset");
		} else if (ots->binder_thread_node != node) {
			ots->binder_thread_node = node;
			set_node = true;
			trace_set_thread_node(task, node, sync, "async set");
		}
		oplus_binder_debug(LOG_TRACK_LAST_ASYNC, "after set, thread(pid=%d tgid=%d comm=%s) sync: %d, reset: %d, ots_node: 0x%llx, node: 0x%llx, set_node: %d\n",
			task->pid, task->tgid, task->comm, sync, reset, (unsigned long long)ots->binder_thread_node, (unsigned long long)node, set_node);
	} else {
		trace_set_thread_node(task, NULL, sync, "ots null");
	}
}

static void set_thread_node_when_br_received(struct binder_transaction *t, struct binder_thread *thread)
{
	struct task_struct *task = NULL;

	if (!g_set_last_async_ux) {
		return;
	}
	if (IS_ERR_OR_NULL(t) || IS_ERR_OR_NULL(thread) || IS_ERR_OR_NULL(thread->task)) {
		return;
	}
	task = thread->task;
	trace_set_thread_node(task, NULL, INVALID_VALUE, "set when br_received");
	oplus_binder_debug(LOG_TRACK_LAST_ASYNC, "set node when transaction_received\n");
	set_binder_thread_node(t, task, NULL, false, false);
}

static void android_vh_binder_transaction_received_handler(void *unused,
	struct binder_transaction *t, struct binder_proc *proc, struct binder_thread *thread, uint32_t cmd)
{
	if (unlikely(!g_sched_enable) || unlikely(!g_async_ux_enable)) {
		return;
	}
	if(sync_insert_queue && !strncmp(proc->tsk->comm, SYSTEM_SERVER_NAME, TASK_COMM_LEN)) {
		if(t->debug_id == insert_limit[NUM_INSERT_ID1]) {
			insert_limit[NUM_INSERT_ID1] = 0;
		} else if (t->debug_id == insert_limit[NUM_INSERT_ID2]) {
			insert_limit[NUM_INSERT_ID2] = 0;
		}
	}
	if (binder_is_sync_mode(t->flags)) {
		return;
	}
	set_thread_node_when_br_received(t, thread);
}

static void android_vh_binder_buffer_release_handler(void *unused,
	struct binder_proc *proc, struct binder_thread *thread, struct binder_buffer *buffer, bool has_transaction)
{
	if (unlikely(!g_sched_enable) || unlikely(!g_async_ux_enable)) {
		return;
	}

	if (IS_ERR_OR_NULL(thread) || IS_ERR_OR_NULL(thread->task)) {
		return;
	}
	if (buffer->async_transaction) {
		set_binder_thread_node(NULL, thread->task, buffer, false, true);
		trace_binder_free_buf(proc, thread, buffer, "async mode");
	} else {
		set_binder_thread_node(NULL, thread->task, buffer, true, true);
		trace_binder_free_buf(proc, thread, buffer, "sync mode");
	}
}

static void android_vh_free_oplus_binder_struct_handler(void *unused, struct binder_transaction *t)
{
	struct oplus_binder_struct *obs = (struct oplus_binder_struct *)(t->android_oem_data1);

	if (unlikely(!g_async_ux_enable)) {
		return;
	}
	trace_binder_t_obs(t, obs, "free_obs");
	free_oplus_binder_struct(obs);
	t->android_oem_data1 = 0;
}

static inline bool insert_work_to_proc_todo_list(struct binder_transaction *t,
	struct binder_proc *proc, struct list_head *target_list, bool sync)
{
	if (!sync_insert_queue || !sync || !t || !proc) {
		return false;
	}

	if (&proc->todo != target_list) {
		return false;
	}
	/* called by binder_proc_transaction() when no binder_thread selected */
	if(proc->tsk && is_sync_ux_enable(NULL, t)
		&& is_task_system_server(proc->tsk)) {
		return true;
	} else {
		return false;
	}
}

static inline bool enqueue_work_to_fg_todo_list(struct binder_transaction *t,
	struct binder_proc *proc, struct binder_work *w, struct list_head *target_list, bool sync)
{
	struct oplus_binder_proc *obp = get_oplus_binder_proc(proc, false);
	struct list_head *fg_todo = NULL;

	/* called by binder_proc_transaction() when no binder_thread selected */
	if (!fg_list_enable || !fg_list_dynamic_enable || !sync || !t || !proc
		   || !w || IS_ERR_OR_NULL(obp)) {
		return false;
	}

	if (!obp->fg_inited) {
		return false;
	}

	/* fg_work only takes effect on the work on proc->todo queue */
	if (&proc->todo != target_list) {
		return false;
	}

	if (!is_sync_ux_enable(NULL, t)) {
		binder_ux_state_systrace(current, NULL,
			STATE_FG_NOT_SYNC_UX, LOG_BINDER_SYSTRACE_LVL0, t, proc);
		return false;
	}
	save_fglist_debug_info(proc, ITEM_SYNC_UX_NOTHREAD, false);

	fg_todo = &obp->fg_todo;

	if (obp->fg_count >= MAX_WORKS_IN_FGLIST) {
		save_fglist_debug_info(proc, ITEM_FG_WORKS_OVERFLOW, false);
		binder_ux_state_systrace(current, NULL, STATE_FG_WORKS_OVERFLOW,
			LOG_BINDER_SYSTRACE_LVL0, t, proc);
		oplus_binder_debug(LOG_FG_LIST_LVL0, "proc[%d: %s] t->debug_id: %d, fg_count: %d, w: %px fg overflow\n",
			proc->tsk->pid, proc->tsk->comm, t->debug_id, obp->fg_count, w);
		return false;
	}

	if (IS_ERR_OR_NULL(fg_todo)) {
		binder_ux_state_systrace(current, NULL, STATE_FG_TODO_NULL,
			LOG_BINDER_SYSTRACE_LVL0, t, proc);
		oplus_binder_debug(LOG_FG_LIST_LVL0, "proc[%d: %s] t->debug_id: %d, fg_count: %d, w: %px fg_todo null, return\n",
			proc->tsk->pid, proc->tsk->comm, t->debug_id, obp->fg_count, w);
		return false;
	}

	set_work_seq(w);
	list_add_tail(&w->entry, fg_todo);
	obp->fg_count++;

	save_fglist_debug_info(proc, ITEM_ADD_TO_FG, false);
	oplus_binder_debug(LOG_FG_LIST_LVL0, "proc[%d: %s] t->debug_id: %d, fg_count: %d, w->seq: %lld, w: %px end\n",
		proc->tsk->pid, proc->tsk->comm, t->debug_id, obp->fg_count, get_work_seq(w), w);
	binder_ux_state_systrace(current, NULL, STATE_FG_ADD_TO_FG,
		LOG_BINDER_SYSTRACE_LVL0, t, proc);

	return true;
}

static bool select_fgtodo_or_proctodo_work(struct binder_proc *proc,
	struct binder_work *fg_work, struct binder_work *proc_work,
	struct binder_thread *thread, struct oplus_binder_proc *obp)
{
	bool select_fg = false;
	long long fg_w_seq = 0;
	long long proc_w_seq = 0;
	struct binder_transaction *proc_t = NULL;
	int proc_w_type = 0;
	bool proc_todo_empty = false;

	if (IS_ERR_OR_NULL(fg_work) || IS_ERR_OR_NULL(proc_work) || IS_ERR_OR_NULL(proc)
		|| IS_ERR_OR_NULL(obp)) {
		oplus_binder_debug(LOG_FG_LIST_LVL0, "select_work default, fg_w or proc_w or proc null return\n");
		binder_ux_state_systrace(current, (thread ? thread->task : NULL),
			STATE_FG_WORK_PROC_NULL, LOG_BINDER_SYSTRACE_LVL0, NULL, proc);
		return false;
	}

	if (list_empty(&proc->todo)) {
		select_fg = true;
		proc_todo_empty = true;
		save_fglist_debug_info(proc, ITEM_SELECT_FG_PROC_EMPTY, false);
		goto end;
	}

	fg_w_seq = get_work_seq(fg_work);
	proc_w_seq = get_work_seq(proc_work);

	proc_w_type = proc_work->type;
	if (proc_w_type == BINDER_WORK_TRANSACTION) {
		proc_t = container_of(proc_work, struct binder_transaction, work);
		if (proc_w_seq < fg_w_seq) {
			if (is_sync_ux_enable(NULL, proc_t) || is_async_ux_enable(NULL, proc_t)) {
				select_fg = false;

				save_fglist_debug_info(proc, ITEM_SELECT_PROC_WHEN_UX, false);
				binder_ux_state_systrace(current, (thread ? thread->task : NULL),
					STATE_FG_SELECT_PROC_WHEN_UX, LOG_BINDER_SYSTRACE_LVL0, NULL, proc);
				goto selected;
			} else if (fg_list_async_first && !binder_is_sync_mode(proc_t->flags)) {
				select_fg = false;

				save_fglist_debug_info(proc, ITEM_SELECT_PROC_WHEN_ASYNC, false);
				binder_ux_state_systrace(current, (thread ? thread->task : NULL),
					STATE_FG_SELECT_PROC_WHEN_ASYNC, LOG_BINDER_SYSTRACE_LVL0, NULL, proc);
				goto selected;
			}
		}
	}

	if (obp->continuous_fg < MAX_CONTINUOUS_FG) {
		select_fg = true;

		save_fglist_debug_info(proc, ITEM_SELECT_FG_DIRECTLY, false);
	} else if (proc_w_seq < fg_w_seq) {
		select_fg = false;

		save_fglist_debug_info(proc, ITEM_SELECT_PROC_COMPARE_SEQ, false);
		binder_ux_state_systrace(current, (thread ? thread->task : NULL),
			STATE_FG_SELECT_PROC_COMP_SEQ, LOG_BINDER_SYSTRACE_LVL0, NULL, proc);
	} else {
		select_fg = true;

		save_fglist_debug_info(proc, ITEM_SELECT_FG_COMPARE_SEQ, false);
	}

selected:
	if (select_fg) {
		obp->continuous_fg++;
	} else {
		obp->continuous_fg = 0;
	}

end:
	if (select_fg) {
		save_fglist_debug_info(proc, ITEM_SELECT_FG, false);
		binder_ux_state_systrace(current, (thread ? thread->task : NULL),
			STATE_FG_SELECT_FG, LOG_BINDER_SYSTRACE_LVL0, NULL, proc);
	} else {
		save_fglist_debug_info(proc, ITEM_SELECT_PROC, false);
	}
	oplus_binder_debug(LOG_FG_LIST_LVL0, "proc[%d: %s] select_fg: %d, fg_count: %d, fg_w_seq: %lld, \
		proc_w_seq: %lld proc_w_type: %d, continuous_fg: %d, proc_todo_empty: %d fg_work: %px, proc_work: %px\n",
		proc->tsk->pid, proc->tsk->comm, select_fg, obp->fg_count, fg_w_seq, proc_w_seq,
		proc_work->type, obp->continuous_fg, proc_todo_empty, fg_work, proc_work);

	return select_fg;
}

static inline void binder_select_fg_worklist_ilocked(struct list_head **list,
	struct binder_thread *thread, struct binder_proc *proc, int wait_for_proc_work)
{
	struct oplus_binder_proc *obp = NULL;
	struct list_head *fg_todo = NULL;
	struct binder_work *fg_work = NULL;
	struct binder_work *proc_work = NULL;
	bool select_fg = false;
	int thread_todo_count = 0;

	if (!fg_list_enable || IS_ERR_OR_NULL(proc) || IS_ERR_OR_NULL(thread)) {
		return;
	}

	if (is_vip_binder_thread(thread)) {
		binder_ux_state_systrace(current, thread->task, STATE_FG_VIP_THREAD_SKIP,
			LOG_BINDER_SYSTRACE_LVL0, NULL, proc);
		oplus_binder_debug(LOG_FG_LIST_LVL1, "select_work vip_thread skip, thread[%d: %d: %s] proc[%d: %s]\n",
			thread->task->pid, thread->task->tgid, thread->task->comm,
			proc->tsk->pid, proc->tsk->comm);
		return;
	}

	/* The priority of thread->todo must be higher than proc->fg_todo.
	This is a while loop, wait_for_proc_work can't replace list_empty(thread->todo).  */
	if (!list_empty(&thread->todo)) {
		if (g_sched_debug & LOG_FG_LIST_LVL1) {
			thread_todo_count = list_count(&thread->todo, 3);
			oplus_binder_debug(LOG_FG_LIST_LVL1, "select_work thread_todo, count = %d, return, \
				thread[%d: %s] proc[%d: %s]\n", thread_todo_count, thread->task->pid,
				thread->task->comm, proc->tsk->pid, proc->tsk->comm);
		}
		return;
	}

	if (!wait_for_proc_work) {
		oplus_binder_debug(LOG_FG_LIST_LVL1, "select_work default, wait_for_proc_work false, return, \
			thread[%d: %s] proc[%d: %s]\n", thread->task->pid, thread->task->comm,
			proc->tsk->pid, proc->tsk->comm);
		return;
	}

	obp = get_oplus_binder_proc(proc, false);
	if (IS_ERR_OR_NULL(obp)) {
		return;
	}
	fg_todo = &obp->fg_todo;
	if (!obp->fg_inited || IS_ERR_OR_NULL(fg_todo)) {
		return;
	}

	if (list_empty(fg_todo)) {
		obp->fg_count = 0;
		oplus_binder_debug(LOG_FG_LIST_LVL1, "select_work default, fg_todo empty, return, \
			thread[%d: %s] proc[%d: %s]\n", thread->task->pid, thread->task->comm,
			proc->tsk->pid, proc->tsk->comm);
		return;
	}
	fg_work = list_first_entry(fg_todo, struct binder_work, entry);
	proc_work = list_first_entry(&proc->todo, struct binder_work, entry);
	select_fg = select_fgtodo_or_proctodo_work(proc,
		fg_work, proc_work, thread, obp);

	if (select_fg) {
		obp->fg_count--;
		if (obp->fg_count < 0) {
			obp->fg_count = 0;
		}
		*list = fg_todo;
		oplus_binder_debug(LOG_FG_LIST_LVL0, "thread[%d: %s] proc[%d: %s] select_work fg_work, fg_count: %d, \
			fg_work: %px, *list: %px\n", thread->task->pid, thread->task->comm,
			proc->tsk->pid, proc->tsk->comm, obp->fg_count,
			fg_work, *list);
	}
}

static void android_vh_binder_select_special_worklist_handler(
	void *unused, struct list_head **list, struct binder_thread *thread, struct binder_proc *proc,
	int wait_for_proc_work, bool *skip)
{
	if (!fg_list_enable) {
		return;
	}
	binder_select_fg_worklist_ilocked(list, thread, proc, wait_for_proc_work);
}
static bool binder_dynamic_enqueue_work_ilocked(struct binder_work *work,
		struct list_head *target_list, bool sync_insert)
{
	struct binder_work *w = NULL;
	struct binder_transaction *t = NULL;
	bool insert = false;
	int i = 0;

	if (unlikely(!g_sched_enable) || unlikely(!g_async_ux_enable)) {
		return false;
	}

	trace_binder_ux_work(work, target_list, NULL, insert, i, "dynamic begin");
	if(sync_insert_queue && insert_limit[NUM_INSERT_ID1] && insert_limit[NUM_INSERT_ID2]) {
		trace_binder_ux_work(work, target_list, NULL, insert, i, "dynamic break");
		return false;
	}

	BUG_ON(target_list == NULL);
	BUG_ON(work->entry.next && !list_empty(&work->entry));

	list_for_each_entry(w, target_list, entry) {
		i++;
		if (i > MAX_UX_IN_LIST) {
			insert = false;
			break;
		}
		if (IS_ERR_OR_NULL(w)) {
			break;
		}

		if (w->type != BINDER_WORK_TRANSACTION) {
			continue;
		}

		t = container_of(w, struct binder_transaction, work);
		if (IS_ERR_OR_NULL(t)) {
			break;
		}
		if (sync_insert) {
			if (!binder_is_sync_mode(t->flags)) {
				continue;
			}
			if (!t->from) {
				continue;
			}
			if ((test_task_ux(t->from->task) || test_task_is_rt(t->from->task))) {
				continue;
			}
			binder_ux_state_systrace(current, NULL, STATE_SYNC_INSERT_QUEUE, LOG_BINDER_SYSTRACE_LVL0, t, NULL);
		} else {
			if (binder_is_sync_mode(t->flags)) {
				continue;
			}
			if (is_async_ux_enable(NULL, t)) {
				continue;
			}
			insert = true;
			binder_ux_state_systrace(current, NULL, STATE_ASYNC_INSERT_QUEUE, LOG_BINDER_SYSTRACE_LVL0, t, NULL);
		}

		insert = true;
		break;
	}

	set_work_seq(work);
	if (insert && !IS_ERR_OR_NULL(w) && !IS_ERR_OR_NULL(&w->entry)) {
		list_add(&work->entry, &w->entry);

		if (sync_insert_queue) {
			if(!insert_limit[NUM_INSERT_ID1] && (t->debug_id != insert_limit[NUM_INSERT_ID2])) {
				insert_limit[NUM_INSERT_ID1] = t->debug_id;
			} else if (!insert_limit[NUM_INSERT_ID2] && (t->debug_id != insert_limit[NUM_INSERT_ID1])) {
				insert_limit[NUM_INSERT_ID2] = t->debug_id;
			}
		}
	} else {
		list_add_tail(&work->entry, target_list);
	}
	trace_binder_ux_work(work, target_list, IS_ERR_OR_NULL(w) ? NULL : &w->entry, insert, i, "dynamic end");
	return true;
}

static void android_vh_binder_special_task_handler(void *unused, struct binder_transaction *t,
	struct binder_proc *proc, struct binder_thread *thread, struct binder_work *w,
	struct list_head *target_list, bool sync, bool *enqueue_task)
{
	struct oplus_binder_struct *obs = NULL;
	bool allow_sync_insert = false;

	if (unlikely(!g_sched_enable) || unlikely(!g_async_ux_enable)) {
		return;
	}

	if (sync) {
		if (enqueue_work_to_fg_todo_list(t, proc, w, target_list, sync)) {
			*enqueue_task = false;
			return;
		}
		if (insert_work_to_proc_todo_list(t, proc, target_list, sync)) {
			allow_sync_insert = true;
			goto dynamic_enqueue;
		}
		return;
	}

	if (unlikely(!async_insert_queue)) {
		return;
	}

	if (!w || !target_list) {
		return;
	}

	if (!t && w) {
		t = container_of(w, struct binder_transaction, work);
	}
	obs = get_oplus_binder_struct(t, false);
	if (!is_async_ux_enable(obs, NULL)) {
		return;
	}
dynamic_enqueue:
	if ((obs && obs->async_ux_enable == ASYNC_UX_ENABLE_INSERT_QUEUE) || allow_sync_insert) {
		if (binder_dynamic_enqueue_work_ilocked(w, target_list, allow_sync_insert)) {
			/*
			if enqueue_task == false, binder_dynamic_enqueue_work_ilocked list_add_xxx is called,
			don't call binder.c binder_enqueue_work_ilocked() again.
			*/
			*enqueue_task = false;
		}
	}
}

static void android_vh_binder_has_special_work_ilocked_handler(void *unused,
	struct binder_thread *thread, bool do_proc_work, bool *has_work)
{
	struct oplus_binder_proc *obp = NULL;

	if (!fg_list_enable) {
		return;
	}

	if (IS_ERR_OR_NULL(thread) || IS_ERR_OR_NULL(thread->proc)) {
		return;
	}

	obp = get_oplus_binder_proc(thread->proc, false);
	if (IS_ERR_OR_NULL(obp)) {
		return;
	}
	if (do_proc_work && !list_empty(&obp->fg_todo)) {
		*has_work = true;
	}
}

static void android_vh_binder_has_proc_work_ilocked_handler(void *unused,
	struct binder_thread *thread, bool do_proc_work, bool *has_work)
{
	struct oplus_binder_proc *obp = NULL;

	if (!fg_list_enable) {
		return;
	}
	if (IS_ERR_OR_NULL(thread) || IS_ERR_OR_NULL(thread->proc)) {
		return;
	}

	/* for fg_list work */
	obp = get_oplus_binder_proc(thread->proc, false);
	if (IS_ERR_OR_NULL(obp)) {
		return;
	}

	if (do_proc_work && !list_empty(&obp->fg_todo)) {
		*has_work = true;
	}
}

static void android_vh_binder_release_special_work_handler(void *unused,
	struct binder_proc *proc, struct list_head **fg_list)
{
	struct oplus_binder_proc *obp = NULL;

	/* no proc->inner_lock here */
	if (!fg_list_enable || !proc) {
		return;
	}

	obp = get_oplus_binder_proc(proc, false);
	if (!IS_ERR_OR_NULL(obp)) {
		*fg_list = &obp->fg_todo;
	}
}

static void android_vh_binder_proc_init_handler(void *unused,
	struct hlist_head *hhead, struct mutex *lock, struct binder_proc *proc)
{
	struct oplus_binder_proc *obp = NULL;

	if (unlikely(!g_sched_enable) || !fg_list_enable) {
		return;
	}

	obp = get_oplus_binder_proc(proc, true);
	if (IS_ERR_OR_NULL(obp)) {
		return;
	}
	memset(obp, 0, sizeof(struct oplus_binder_proc));
	INIT_LIST_HEAD(&obp->fg_todo);
	obp->fg_inited = true;
}

static void android_vh_binder_free_proc_handler(void *unused, struct binder_proc *proc)
{
	struct oplus_binder_proc *obp = NULL;

	if (!fg_list_enable || IS_ERR_OR_NULL(proc)) {
		return;
	}

	obp = get_oplus_binder_proc(proc, false);
	if (IS_ERR_OR_NULL(obp)) {
		return;
	}

	free_oplus_binder_proc(obp);
	proc->android_oem_data1 = 0;

	oplus_binder_debug(LOG_FG_LIST_LVL0, "free proc[%d: %s]\n",
		proc->tsk->pid, proc->tsk->comm);
}

static bool sync_mode_check_ux(struct binder_proc *proc,
		struct binder_transaction *t, struct task_struct *binder_th_task, bool sync)
{
	struct task_struct *binder_proc_task = proc->tsk;
	bool set_ux = true;

	if (unlikely(!g_sched_enable)) {
		return false;
	}

	if (is_task_servicemg(binder_proc_task)) {
		binder_set_inherit_ux(binder_proc_task, current, sync, true, t, proc);
		trace_binder_proc_thread(binder_proc_task, binder_th_task, sync, INVALID_VALUE, t, proc,
			"sync_ux set servicemg");
	}

	if (!binder_th_task)
		return false;

	trace_binder_proc_thread(binder_proc_task, binder_th_task, sync, INVALID_VALUE, t, proc,
		"sync_ux set ux");

	return set_ux;
}

static struct binder_thread *get_current_async_thread(struct binder_transaction *t, struct binder_proc *proc)
{
	struct rb_node *n = NULL;
	struct binder_node *node = NULL;
	struct binder_thread *thread = NULL;
	struct oplus_task_struct *ots = NULL;
	ktime_t time = 0;
	int count = 0;
	bool has_async = true;
	static int get_null_count = 0;
	static int get_thread_count = 0;

	if (unlikely(!g_set_last_async_ux)) {
		return NULL;
	}
	if (proc->max_threads <= 0) {
		return NULL;
	}
	if (t && t->buffer) {
		node = t->buffer->target_node;
	}
	if (!node) {
		return NULL;
	}
	if (g_sched_debug & LOG_GET_LAST_ASYNC) {
		time = ktime_get();
	}
	for (n = rb_first(&proc->threads); n != NULL; n = rb_next(n)) {
		thread = rb_entry(n, struct binder_thread, rb_node);
		if (thread->task) {
			ots = get_oplus_task_struct(thread->task);
			if (!IS_ERR_OR_NULL(ots) && (ots->binder_thread_node == node)) {
				if (g_sched_debug & LOG_GET_LAST_ASYNC) {
					time = ktime_get() - time;
					get_thread_count++;
				}
				trace_get_async_thread(proc, thread, count, NULL, node, has_async, time, "async_thread got");
				oplus_binder_debug(LOG_GET_LAST_ASYNC, "proc(pid:%d tgid:%d comm:%s) thread(pid:%d tgid:%d comm:%s) \
					max_threads:%d request:%d started:%d count:%d node:0x%llx time:%lldns got get_thread: %d get_null: %d\n",
					proc ? proc->tsk->pid : 0,
					proc ? proc->tsk->tgid : 0,
					proc ? proc->tsk->comm : "null",
					thread ? thread->task->pid : 0,
					thread ? thread->task->tgid : 0,
					thread ? thread->task->comm : "null",
					proc ? proc->max_threads : 0,
					proc ? proc->requested_threads : 0,
					proc ? proc->requested_threads_started : 0,
					count, (unsigned long long)node, time, get_thread_count, get_null_count);
				return thread;
			}
		}
		if (node->has_async_transaction == false) {
			has_async = false;
			break;
		}
		count++;
		if (count > CHECK_MAX_NODE_FOR_ASYNC_THREAD) {
			break;
		}
		if ((g_sched_debug & LOG_GET_LAST_ASYNC) && !IS_ERR_OR_NULL(ots) && thread->task) {
			trace_get_async_thread(proc, thread, count, ots->binder_thread_node, node,
				has_async, time, "async_thread search");
			oplus_binder_debug(LOG_TRACK_LAST_ASYNC, "proc(pid:%d tgid:%d comm:%s) thread(pid:%d tgid:%d comm:%s) \
				max_threads:%d request:%d started:%d count:%d ots_node:0x%llx node:0x%llx time:%lldns get_null: %d\n",
				proc ? proc->tsk->pid : 0,
				proc ? proc->tsk->tgid : 0,
				proc ? proc->tsk->comm : "null",
				thread ? thread->task->pid : 0,
				thread ? thread->task->tgid : 0,
				thread ? thread->task->comm : "null",
				proc ? proc->max_threads : 0,
				proc ? proc->requested_threads : 0,
				proc ? proc->requested_threads_started : 0,
				count, (unsigned long long)(ots->binder_thread_node),
				(unsigned long long)node, time, get_null_count);
		}
	}
	if (g_sched_debug & LOG_GET_LAST_ASYNC) {
		time = ktime_get() - time;
		get_null_count++;
	}
	trace_get_async_thread(proc, thread, count, NULL, node, has_async, time, "async_thread get null");
	oplus_binder_debug(LOG_GET_LAST_ASYNC, "proc(pid:%d tgid:%d comm:%s) max_threads:%d request:%d \
		started:%d count:%d node:0x%llx has_async:%d time:%lldns end get_null: %d, get_thread: %d\n",
		proc ? proc->tsk->pid : 0,
		proc ? proc->tsk->tgid : 0,
		proc ? proc->tsk->comm : "null",
		proc ? proc->max_threads : 0,
		proc ? proc->requested_threads : 0,
		proc ? proc->requested_threads_started : 0,
		count, (unsigned long long)node, has_async, time,
		get_null_count, get_thread_count);
	return NULL;
}

static bool async_mode_check_ux(struct binder_proc *proc, struct binder_transaction *t,
		struct task_struct *binder_th_task, bool sync, bool pending_async,
		struct binder_thread **last_thread, bool *force_sync)
{
	struct oplus_binder_struct *obs = NULL;
	struct task_struct *ux_task = binder_th_task;
	bool set_ux = false;

	if (unlikely(!g_sched_enable)) {
		return false;
	}

	if (unlikely(!g_async_ux_enable)) {
		if (is_allow_sf_binder_ux(current)) {
			set_ux = true;
			*force_sync = true;
		}
		return set_ux;
	}
	obs = get_oplus_binder_struct(t, false);
	if (!is_async_ux_enable(obs, NULL)) {
		set_ux = false;
		trace_binder_ux_task(sync, pending_async, set_ux, ux_task, INVALID_VALUE,
			t, obs, "async_ux not enable");
		goto end;
	}

	if (ux_task) {
		set_ux = true;
		trace_binder_ux_task(sync, pending_async, set_ux, ux_task, obs->async_ux_enable,
			t, obs, "async_ux set ux");
		goto end;
	}

	/* pending_async, no binder_th_task */
	if (pending_async) {
		*last_thread = get_current_async_thread(t, proc);
		if (*last_thread) {
			ux_task = (*last_thread)->task;
			set_ux = true;
			binder_ux_state_systrace(current, ux_task, STATE_ASYNC_SET_LAST_UX, LOG_BINDER_SYSTRACE_LVL0, t, proc);
		} else {
			set_ux = false;
			binder_ux_state_systrace(current, NULL, STATE_ASYNC_NOT_SET_LAST_UX, LOG_BINDER_SYSTRACE_LVL0, t, proc);
		}
		obs->pending_async = true;
		trace_binder_ux_task(sync, pending_async, set_ux, ux_task, obs->async_ux_enable,
			t, obs, "async_ux set last");
		goto end;
	} else {
		obs->async_ux_no_thread = true;
		binder_ux_state_systrace(current, NULL, STATE_ASYNC_NO_THREAD_NO_PENDING, LOG_BINDER_SYSTRACE_LVL0, t, proc);
	}
end:
	trace_binder_ux_task(sync, pending_async, set_ux, ux_task, INVALID_VALUE,
			t, obs, "async_ux end");
	return set_ux;
}

static void android_vh_binder_set_priority_handler(void *unused,
	struct binder_transaction *t, struct task_struct *task)
{
	struct oplus_binder_struct *obs = NULL;
	struct oplus_task_struct *ots = NULL;

	if (unlikely(!g_sched_enable) || !g_set_async_ux_after_pending) {
		return;
	}
	if (IS_ERR_OR_NULL(t) || IS_ERR_OR_NULL(task)) {
		return;
	}
	if (binder_is_sync_mode(t->flags)) {
		return;
	}

	obs = get_oplus_binder_struct(t, false);
	if (IS_ERR_OR_NULL(obs)) {
		return;
	}

	if (!obs->pending_async && !obs->async_ux_no_thread) {
		binder_ux_state_systrace(current, task, STATE_ASYNC_HAS_THREAD,
			LOG_BINDER_SYSTRACE_LVL1, t, NULL);
		return;
	}

	ots = get_oplus_task_struct(task);
	if (IS_ERR_OR_NULL(ots)) {
		return;
	}
	if (oplus_get_ux_state(task) && get_task_async_ux_sts(ots)) {
		binder_ux_state_systrace(current, task, STATE_THREAD_WAS_ASYNC_UX,
			LOG_BINDER_SYSTRACE_LVL0, t, NULL);
		return;
	}

	binder_ux_state_systrace(current, task, STATE_ASYNC_SET_UX_AFTER_NO_THREAD,
		LOG_BINDER_SYSTRACE_LVL0, t, NULL);

	binder_set_inherit_ux(task, NULL, false, false, t, NULL);
	obs->pending_async = false;
	obs->async_ux_no_thread = false;

	oplus_binder_debug(LOG_SET_ASYNC_AFTER_PENDING, "thread(pid = %d tgid = %d comm = %s) \
		pending_async = %d async_ux_no_thread = %d set_async_after_nothread\n",
		task->pid, task->tgid, task->comm, obs->pending_async, obs->async_ux_no_thread);
}

static inline void dump_fg_list_info(struct binder_proc *proc, bool sync)
{
	struct oplus_binder_proc *obp = NULL;
	static int count = 0;
	int proc_todo_count = 0;
	int fg_todo_count = INVALID_VALUE;
	int fg_count = INVALID_VALUE;

	if (!fg_list_enable || !is_fglist_debug_enable())
		return;

	if (!sync || !proc)
		return;

	count++;
	if (count > fg_debug_interval) {
		if (is_fglist_debug_process(proc)) {
			count = 0;
			proc_todo_count = list_count(&proc->todo, 1);
			obp = get_oplus_binder_proc(proc, false);
			if (!IS_ERR_OR_NULL(obp)) {
				fg_todo_count = list_count(&obp->fg_todo, 2);
				fg_count = obp->fg_count;
			}
			oplus_binder_debug(LOG_FG_LIST_LVL0, "[BINDER_FG] proc[%d: %s] proc_todo_count = %d \
				fg_todo_count = %d fg_count = %d  proc = %px\n", proc->tsk->pid,
				proc->tsk->comm, proc_todo_count, fg_todo_count, fg_count, proc);
			save_fglist_debug_info(proc, ITEM_FG_LIST_DEBUG_UNKNOWN, true);
		}
	}
}

static void dump_binder_sched_info(struct binder_transaction *t,
	struct binder_proc *proc, bool sync)
{
	dump_fg_list_info(proc, sync);
}


static void android_vh_binder_proc_transaction_finish_handler(void *unused, struct binder_proc *proc,
		struct binder_transaction *t, struct task_struct *binder_th_task, bool pending_async, bool sync)
{
	struct binder_thread *last_thread = NULL;
	struct task_struct *last_task = NULL;
	bool set_ux = false;
	bool force_sync = false;

	if (unlikely(!g_sched_enable))
		return;

	if (!binder_th_task) {
		binder_ux_state_systrace(current, (proc ? proc->tsk : NULL),
			STATE_NO_BINDER_THREAD, LOG_BINDER_SYSTRACE_LVL0, t, proc);
	}

	set_binder_thread_node(t, binder_th_task, NULL, sync, false);

	if (sync) {
		set_ux = sync_mode_check_ux(proc, t, binder_th_task, sync);
	} else {
		set_ux = async_mode_check_ux(proc, t, binder_th_task, sync,
			pending_async, &last_thread, &force_sync);
	}

#if IS_ENABLED(CONFIG_ANDROID_BINDER_IPC_VIP_THREAD)
	if (t->flags & TF_TAXI_UX_WAY) {
		set_ux = true;
	}
#endif
	if (set_ux) {
		if (force_sync) {
			binder_set_inherit_ux(binder_th_task, current, true, false, t, proc);
		} else if (last_thread) {
			last_task = last_thread->task;
			binder_set_inherit_ux(last_task, current, sync, false, t, proc);
		} else {
			binder_set_inherit_ux(binder_th_task, current, sync, false, t, proc);
		}
	}

	if (last_task) {
		trace_binder_ux_task(sync, pending_async, set_ux, last_task,
			INVALID_VALUE, t, NULL, "ux t_finish last");
	} else {
		trace_binder_ux_task(sync, pending_async, set_ux, binder_th_task,
			INVALID_VALUE, t, NULL, "ux t_finish");
	}

	dump_binder_sched_info(t, proc, sync);
}


static void android_vh_binder_proc_transaction_handler(void *data,
	struct task_struct *caller_task, struct task_struct *binder_proc_task, struct task_struct *binder_th_task,
	int node_debug_id, struct binder_transaction *t, bool pending_async)
{
	if (!binder_th_task || !t)
		return;

	if (binder_th_task->prio < MAX_RT_PRIO) {
		t->set_priority_called = true;
	}
}

static void android_vh_binder_thread_read_handler(void *unused,
	struct list_head **list, struct binder_proc *proc, struct binder_thread *thread)
{
	struct binder_work *w = NULL;
	struct binder_transaction *t = NULL;
	struct task_struct *task = NULL;

	if (IS_ERR_OR_NULL(*list)) {
		return;
	}
	w = list_first_entry_or_null(*list, struct binder_work, entry);
	if (IS_ERR_OR_NULL(w)) {
		return;
	}
	if (w->type != BINDER_WORK_TRANSACTION) {
		return;
	}
	t = container_of(w, struct binder_transaction, work);
	if (!t->buffer || !t->buffer->target_node
		|| t->set_priority_called) {
		return;
	}

	task = thread ? thread->task : NULL;
	if (IS_ERR_OR_NULL(task)) {
		return;
	}
	if (task->prio < MAX_RT_PRIO) {
		t->set_priority_called = true;
	}
}

void register_binder_sched_vendor_hooks(void)
{
	register_trace_android_vh_binder_restore_priority(
		android_vh_binder_restore_priority_handler, NULL);
	register_trace_android_vh_binder_wait_for_work(
		android_vh_binder_wait_for_work_handler, NULL);
	register_trace_android_vh_sync_txn_recvd(
		android_vh_sync_txn_recvd_handler, NULL);
	register_trace_android_vh_binder_proc_transaction_finish(
		android_vh_binder_proc_transaction_finish_handler, NULL);
	register_trace_android_vh_binder_special_task(
		android_vh_binder_special_task_handler, NULL);
	register_trace_android_vh_alloc_oem_binder_struct(
		android_vh_alloc_oem_binder_struct_handler, NULL);
	register_trace_android_vh_binder_transaction_received(
		android_vh_binder_transaction_received_handler, NULL);
	register_trace_android_vh_free_oem_binder_struct(
		android_vh_free_oplus_binder_struct_handler, NULL);
	register_trace_android_vh_binder_buffer_release(
		android_vh_binder_buffer_release_handler, NULL);
	register_trace_android_vh_binder_set_priority(
		android_vh_binder_set_priority_handler, NULL);

	register_trace_android_vh_binder_has_special_work_ilocked(
		android_vh_binder_has_special_work_ilocked_handler, NULL);
	register_trace_android_vh_binder_select_special_worklist(
		android_vh_binder_select_special_worklist_handler, NULL);
	register_trace_android_vh_binder_preset(
		android_vh_binder_proc_init_handler, NULL);

	register_trace_android_vh_binder_list_add_work(
		android_vh_binder_list_add_work_handler, NULL);
	register_trace_android_vh_binder_has_proc_work_ilocked(
		android_vh_binder_has_proc_work_ilocked_handler, NULL);
	register_trace_android_vh_binder_release_special_work(
		android_vh_binder_release_special_work_handler, NULL);
	register_trace_android_vh_binder_free_proc(
		android_vh_binder_free_proc_handler, NULL);

	register_trace_android_vh_binder_proc_transaction(
		android_vh_binder_proc_transaction_handler, NULL);
	register_trace_android_vh_binder_thread_read(
		android_vh_binder_thread_read_handler, NULL);
}

static void init_oplus_binder_struct(void *ptr)
{
	struct oplus_binder_struct *obs = ptr;

	memset(obs, 0, sizeof(struct oplus_binder_struct));
}

static void init_oplus_binder_proc(void *ptr)
{
	struct oplus_binder_proc *obp = ptr;

	memset(obp, 0, sizeof(struct oplus_binder_proc));
}
void oplus_binder_sched_init(void)
{
	binder_sched_dts_init();

	oplus_binder_struct_cachep = kmem_cache_create("oplus_binder_struct",
		sizeof(struct oplus_binder_struct), 0, SLAB_PANIC|SLAB_ACCOUNT, init_oplus_binder_struct);

	oplus_binder_proc_cachep = kmem_cache_create("oplus_binder_proc",
		sizeof(struct oplus_binder_proc), 0, SLAB_PANIC|SLAB_ACCOUNT, init_oplus_binder_proc);

	register_binder_sched_vendor_hooks();
	pr_info("%s\n", __func__);
}

module_param_named(binder_sched_enable, g_sched_enable, uint, 0660);
module_param_named(binder_sched_debug, g_sched_debug, ullong, 0660);
module_param_named(binder_sched_ux_test, binder_ux_test, uint, 0660);
module_param_named(binder_ux_enable, g_async_ux_enable, int, 0664);
module_param_named(binder_async_insert_queue, async_insert_queue, int, 0664);
module_param_named(binder_sync_insert_queue, sync_insert_queue, uint, 0664);
module_param_named(binder_set_last_async_ux, g_set_last_async_ux, int, 0664);
module_param_named(binder_set_async_ux_after_pending, g_set_async_ux_after_pending, int, 0664);
module_param_named(binder_allow_accumulate_ux, allow_accumulate_ux, int, 0664);
module_param_named(binder_unset_ux_match_t, unset_ux_match_t, int, 0664);
module_param_named(binder_fg_list_enable, fg_list_enable, int, 0664);
module_param_named(binder_fg_list_async_first, fg_list_async_first, int, 0664);
module_param_named(binder_max_works_in_fg, max_works_in_fg, int, 0664);
module_param_named(binder_fg_debug_pid, fg_debug_pid, int, 0664);
module_param_named(binder_fg_debug_interval, fg_debug_interval, int, 0664);
module_param_named(binder_feature_enable, bd_feature_enable, int, 0444);
