/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_display_debug.h
** Description : oplus display debug header
** Version : 1.1
** Date : 2024/05/20
** Author : Display
******************************************************************/
#ifndef _OPLUS_DISPLAY_DEBUG_H_
#define _OPLUS_DISPLAY_DEBUG_H_

/* ---------------------------- extern params ---------------------------- */
enum OPLUS_LOG_LEVEL {
	OPLUS_LOG_LEVEL_NONE = 0,
	OPLUS_LOG_LEVEL_ERR,
	OPLUS_LOG_LEVEL_WARN,
	OPLUS_LOG_LEVEL_INFO,
	OPLUS_LOG_LEVEL_DEBUG,
};

/**
 * enum oplus_debug_log --       flags to control debug log; 1->enbale  0->disable
 * @OPLUS_DEBUG_LOG_DISABLED:    disable all debug log
 * @OPLUS_DEBUG_LOG_DCS:         dump register log
 * @OPLUS_DEBUG_LOG_DSI:         DSI log
 * @OPLUS_DEBUG_LOG_OFP:         OFP log
 * @OPLUS_DEBUG_LOG_ADFR:        ADFR log
 * @OPLUS_DEBUG_LOG_TEMP_COMPENSATION:temp compensation log
 * @OPLUS_DEBUG_LOG_ALL:         enable all debug log
 */
enum oplus_debug_log_flag {
	OPLUS_DEBUG_LOG_DISABLED = 0,
	OPLUS_DEBUG_LOG_DCS = BIT(0),
	OPLUS_DEBUG_LOG_DSI = BIT(1),
	OPLUS_DEBUG_LOG_OFP = BIT(2),
	OPLUS_DEBUG_LOG_ADFR = BIT(3),
	OPLUS_DEBUG_LOG_TEMP_COMPENSATION = BIT(4),
	OPLUS_DEBUG_LOG_PWM = BIT(5),
	OPLUS_DEBUG_LOG_ALL = 0xFFFF,
};

enum oplus_display_trace_enable {
	OPLUS_DISPLAY_DISABLE_TRACE = 0,
	OPLUS_DISPLAY_DSI_TRACE_ENABLE = BIT(0),
	OPLUS_DISPLAY_OFP_TRACE_ENABLE = BIT(1),
	OPLUS_DISPLAY_ADFR_TRACE_ENABLE = BIT(2),
	OPLUS_DISPLAY_TEMP_COMPENSATION_TRACE_ENABLE = BIT(3),
	OPLUS_DISPLAY_TRACE_ALL = 0xFFFF,
};

enum {
	MTK_LOG_LEVEL_MOBILE_LOG = 0x1,
	MTK_LOG_LEVEL_DETAIL_LOG = 0x2,
	MTK_LOG_LEVEL_FENCE_LOG = 0x4,
	MTK_LOG_LEVEL_IRQ_LOG = 0x8,
	MTK_LOG_LEVEL_TRACE_LOG = 0x10,
	MTK_LOG_LEVEL_DUMP_REGS = 0x20,
};


/* ---------------------------- extern params ---------------------------- */
/* log level config */
extern unsigned int oplus_display_log_level;
/* debug log switch */
extern unsigned int oplus_display_log_type;
/* dual display id */
extern unsigned int oplus_adfr_display_id;
/* dynamic trace enable */
extern unsigned int oplus_display_trace_enable;

extern int g_commit_pid;
extern bool g_mobile_log;
extern bool g_detail_log;
extern bool g_fence_log;
extern unsigned int g_trace_log;
extern int trig_db_enable;
extern bool logger_enable;
extern bool g_irq_log;
extern int mtk_cmdq_msg;

/* trace function */
extern void mtk_drm_print_trace(char *fmt, ...);
extern void pq_dump_all(unsigned int dump_flag);
extern void init_log_buffer(void);
extern void drm_invoke_fps_chg_callbacks(unsigned int new_fps);


/* ---------------------------- oplus debug log ----------------------------  */
/* ---------------- base debug log  ---------------- */
#define OPLUS_DISP_ERR(tag, fmt, ...) \
	do { \
		pr_err("[ERR][%s][DISPLAY:%u][%s:%d] " pr_fmt(fmt), tag, oplus_adfr_display_id, __func__, __LINE__, ##__VA_ARGS__); \
	} while (0)

#define OPLUS_DISP_WARN(tag, fmt, ...) \
	do { \
		pr_warn("[WARN][%s][DISPLAY:%u][%s:%d] " pr_fmt(fmt), tag, oplus_adfr_display_id, __func__, __LINE__, ##__VA_ARGS__); \
	} while (0)

#define OPLUS_DISP_INFO(tag, fmt, ...) \
	do { \
		pr_info("[INFO][%s][DISPLAY:%u][%s:%d] " pr_fmt(fmt), tag, oplus_adfr_display_id, __func__, __LINE__, ##__VA_ARGS__); \
	} while (0)

#define OPLUS_DISP_INFO_ONCE(tag, fmt, ...) \
	do { \
		pr_info_once("[INFO_ONCE][%s][DISPLAY:%u][%s:%d] " pr_fmt(fmt), tag, oplus_adfr_display_id, __func__, __LINE__, ##__VA_ARGS__); \
	} while (0)

#define OPLUS_DISP_DEBUG(tag, fmt, ...) \
	do { \
		pr_debug("[DEBUG][%s][DISPLAY:%u][%s:%d] " pr_fmt(fmt), tag, oplus_adfr_display_id, __func__, __LINE__, ##__VA_ARGS__); \
	} while (0)

/* ---------------- dsi debug log  ---------------- */
#define OPLUS_DSI_ERR(fmt, ...) \
	do { \
		if (oplus_display_log_level >= OPLUS_LOG_LEVEL_ERR) \
			OPLUS_DISP_ERR("DSI", fmt, ##__VA_ARGS__); \
	} while (0)

#define OPLUS_DSI_WARN(fmt, ...) \
	do { \
		if (oplus_display_log_level >= OPLUS_LOG_LEVEL_WARN) \
			OPLUS_DISP_WARN("DSI", fmt, ##__VA_ARGS__); \
	} while (0)

#define OPLUS_DSI_INFO(fmt, ...) \
	do { \
		if (oplus_display_log_level >= OPLUS_LOG_LEVEL_INFO) \
			OPLUS_DISP_INFO("DSI", fmt, ##__VA_ARGS__); \
	} while (0)

#define OPLUS_DSI_INFO_ONCE(fmt, ...) \
	do { \
		if (oplus_display_log_level >= OPLUS_LOG_LEVEL_INFO) \
			OPLUS_DISP_INFO_ONCE("DSI", fmt, ##__VA_ARGS__); \
	} while (0)

#define OPLUS_DSI_DEBUG(fmt, ...) \
	do { \
		if ((oplus_display_log_level >= OPLUS_LOG_LEVEL_DEBUG) || (oplus_display_log_type & OPLUS_DEBUG_LOG_DSI) || (g_mobile_log)) \
			OPLUS_DISP_INFO("DSI", fmt, ##__VA_ARGS__); \
		else \
			OPLUS_DISP_DEBUG("DSI", fmt, ##__VA_ARGS__); \
	} while (0)

#define OPLUS_DSI_DEBUG_DCS(fmt, ...) \
	do { \
		if ((oplus_display_log_level >= OPLUS_LOG_LEVEL_DEBUG) || (oplus_display_log_type & OPLUS_DEBUG_LOG_DCS) || (g_mobile_log)) \
			OPLUS_DISP_INFO("DCS", fmt, ##__VA_ARGS__); \
		else \
			OPLUS_DISP_DEBUG("DCS", fmt, ##__VA_ARGS__); \
	} while (0)

/* ---------------- dsi debug trace  ---------------- */
#define OPLUS_DSI_TRACE_BEGIN(fmt, args...) \
	do { \
		if ((oplus_display_trace_enable & OPLUS_DISPLAY_DSI_TRACE_ENABLE) || (g_trace_log)) { \
			mtk_drm_print_trace("B|%d|"fmt"\n", current->tgid, ##args); \
		} \
	} while (0)

#define OPLUS_DSI_TRACE_END(fmt, args...) \
	do { \
		if ((oplus_display_trace_enable & OPLUS_DISPLAY_DSI_TRACE_ENABLE) || (g_trace_log)) { \
			mtk_drm_print_trace("E|%d|"fmt"\n", current->tgid, ##args); \
		} \
	} while (0)

#define OPLUS_DSI_TRACE_INT(fmt, args...) \
	do { \
		if ((oplus_display_trace_enable & OPLUS_DISPLAY_DSI_TRACE_ENABLE) || (g_trace_log)) { \
			mtk_drm_print_trace("C|%d|"fmt"\n", current->tgid, ##args); \
		} \
	} while (0)


/* ---------------------------- function implementation ---------------------------- */
void oplus_display_set_logger_en(bool en);
void oplus_display_set_mobile_log(bool en);
void oplus_display_set_detail_log(bool en);
void oplus_display_kill_surfaceflinger(void);
int oplus_display_set_mtk_loglevel(void *buf);
int oplus_display_set_limit_fps(void *buf);

#endif /* _OPLUS_DISPLAY_DEBUG_H_ */
