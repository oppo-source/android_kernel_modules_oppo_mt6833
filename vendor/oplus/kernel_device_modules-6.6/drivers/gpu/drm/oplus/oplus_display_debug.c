/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_display_debug.c
** Description : oplus display debug
** Version : 1.1
** Date : 2024/05/20
** Author : Display
******************************************************************/
#include <linux/kobject.h>
#include <linux/signal.h>
#include <linux/sched/signal.h>
#include <soc/oplus/system/oplus_project.h>

#include "oplus_display_debug.h"
#include "oplus_display_device.h"
#include "oplus_display_utils.h"
/* -------------------- macro ----------------------------------------------- */


/* -------------------- parameters ------------------------------------------ */
unsigned int oplus_display_log_level = OPLUS_LOG_LEVEL_INFO;
EXPORT_SYMBOL(oplus_display_log_level);
unsigned int oplus_display_log_type = OPLUS_DEBUG_LOG_DISABLED;
EXPORT_SYMBOL(oplus_display_log_type);
unsigned int oplus_display_trace_enable = OPLUS_DISPLAY_DISABLE_TRACE;
EXPORT_SYMBOL(oplus_display_trace_enable);
static bool g_mobile_log_default_state = false;
DEFINE_MUTEX(oplus_disp_log_lock);
int g_commit_pid = 0;
EXPORT_SYMBOL(g_commit_pid);


/* -------------------- extern ---------------------------------------------- */
extern struct dsi_panel_lcm *oplus_display0_params;

/* -------------------- oplus  functions  ----------------------------------  */

void oplus_display_set_logger_en(bool en) {
	if (get_eng_version() == AGING) {
		if (en)
			logger_enable = 1;
		else
			logger_enable = 0;
	}
}

void oplus_display_set_mobile_log(bool en) {
	if ((get_eng_version() == AGING) || trig_db_enable)
		g_mobile_log = en;
}

void oplus_display_set_detail_log(bool en) {
	if ((get_eng_version() == AGING) || trig_db_enable)
		g_detail_log = en;
}

void oplus_display_kill_surfaceflinger(void) {
	 struct task_struct *p;
	if ((get_eng_version() == AGING) || (get_eng_version() == PREVERSION) || trig_db_enable) {
		read_lock(&tasklist_lock);
		for_each_process(p) {
			get_task_struct(p);
			if (strcmp(p->comm, "surfaceflinger") == 0) {
				send_sig_info(SIGABRT, SEND_SIG_PRIV, p);
			}
			put_task_struct(p);
		}
		read_unlock(&tasklist_lock);
	}
}

void set_logger_enable(int enable)
{
	if (enable == 1) {
		init_log_buffer();
		logger_enable = 1;
	} else if (enable == 0) {
		logger_enable = 0;
	}
}
EXPORT_SYMBOL(set_logger_enable);

int oplus_display_set_mtk_loglevel(void *buf)
{
	struct kernel_loglevel *loginfo = buf;
	unsigned int enabled = 0;
	unsigned int loglevel = 0;

	enabled = loginfo->enable;
	loglevel = loginfo->log_level;

	OPLUS_DSI_INFO("mtk log level is 0x%x,enable=%d", loglevel, enabled);

	mutex_lock(&oplus_disp_log_lock);
	if (enabled == 1) {
		if (loglevel & MTK_LOG_LEVEL_MOBILE_LOG) {
			g_mobile_log = true;
			set_logger_enable(1);
			mtk_cmdq_msg = 1;
		}
		if (loglevel & MTK_LOG_LEVEL_DETAIL_LOG)
			g_detail_log = true;
		if (loglevel & MTK_LOG_LEVEL_FENCE_LOG)
			g_fence_log = true;
		if (loglevel & MTK_LOG_LEVEL_IRQ_LOG)
			g_irq_log = true;
		if (loglevel & MTK_LOG_LEVEL_TRACE_LOG)
			g_trace_log = true;
		if (loglevel & MTK_LOG_LEVEL_DUMP_REGS) {
			if (!g_mobile_log) {
				g_mobile_log = true;
				g_mobile_log_default_state = false;
			} else {
				g_mobile_log_default_state = true;
			}
			pq_dump_all(0xFF);
			oplus_panel_backlight_check(oplus_display0_params);

			if (!g_mobile_log_default_state) {
				g_mobile_log = false;
			}
		}
	} else {
		if (loglevel & MTK_LOG_LEVEL_MOBILE_LOG) {
			g_mobile_log = false;
			set_logger_enable(0);
			mtk_cmdq_msg = 0;
		}
		if (loglevel & MTK_LOG_LEVEL_DETAIL_LOG)
			g_detail_log = false;
		if (loglevel & MTK_LOG_LEVEL_FENCE_LOG)
			g_fence_log = false;
		if (loglevel & MTK_LOG_LEVEL_IRQ_LOG)
			g_irq_log = false;
		if (loglevel & MTK_LOG_LEVEL_TRACE_LOG)
			g_trace_log = false;
		if (loglevel & MTK_LOG_LEVEL_DUMP_REGS) {
			if (!g_mobile_log_default_state) {
				g_mobile_log = false;
			}
		}
	}
	mutex_unlock(&oplus_disp_log_lock);

	return 0;
}

int oplus_display_set_limit_fps(void *buf)
{
	unsigned int limit_fps = 0;
	unsigned int *p_fps = buf;

	limit_fps = (*p_fps);

	drm_invoke_fps_chg_callbacks(limit_fps);

	return 0;
}
