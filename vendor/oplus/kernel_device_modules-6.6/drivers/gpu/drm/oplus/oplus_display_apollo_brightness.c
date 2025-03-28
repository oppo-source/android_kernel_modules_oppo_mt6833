/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_display_apollo_brightness.c
** Description : oplus display apollo_brightness feature
** Version : 1.0
** Date : 2024/05/20
** Author : Display
******************************************************************/
#include <linux/delay.h>
#include <linux/ktime.h>

#include "mtk_panel_ext.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_drv.h"
#include "mtk_debug.h"
#include "mtk_drm_trace.h"

#include "oplus_display_apollo_brightness.h"
#include "oplus_display_debug.h"
#include "oplus_dsi_panel_cmd.h"
#ifdef OPLUS_FEATURE_DISPLAY_ADFR
#include "oplus_adfr.h"
#endif /* OPLUS_FEATURE_DISPLAY_ADFR */
#ifdef OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT
#include "oplus_display_onscreenfingerprint.h"
#endif /* OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT */
#ifdef OPLUS_FEATURE_DISPLAY_HPWM
#include "oplus_display_pwm.h"
#endif /* OPLUS_FEATURE_DISPLAY_HPWM */

extern void apollo_set_brightness_for_show(unsigned int level);
extern u16 mtk_get_gpr(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle);
extern int oplus_display_panel_set_pwm_bl(struct drm_crtc *crtc, struct cmdq_pkt *cmdq_handle, unsigned int level, bool is_sync);

static void oplus_bl_cmdq_cb(struct cmdq_cb_data data)
{
	struct mtk_cmdq_cb_data *cb_data = data.data;

	cmdq_pkt_destroy(cb_data->cmdq_handle);
	kfree(cb_data);
}

void oplus_display_apollo_init_para(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	if (!crtc) {
		OPLUS_DSI_ERR("oplus_display_apollo_crtc_init_para init fail\n");
		return;
	}

	mtk_crtc->oplus_apollo_br = kzalloc(sizeof(struct oplus_apollo_brightness),
			GFP_KERNEL);
	OPLUS_DSI_INFO("oplus_apollo_brightness need allocate memory\n");

	if (!mtk_crtc->oplus_apollo_br) {
		OPLUS_DSI_ERR("oplus_apollo_brightness allocate memory fail\n");
		return;
	}

	mtk_crtc->oplus_apollo_br->oplus_power_on = true;
	mtk_crtc->oplus_apollo_br->oplus_refresh_rate_switching = false;
	mtk_crtc->oplus_apollo_br->oplus_te_tag_ns = 0;
	mtk_crtc->oplus_apollo_br->oplus_te_diff_ns = 0;
	mtk_crtc->oplus_apollo_br->cur_vsync = 0;
	mtk_crtc->oplus_apollo_br->limit_superior_ns = 0;
	mtk_crtc->oplus_apollo_br->limit_inferior_ns = 0;
	mtk_crtc->oplus_apollo_br->transfer_time_us = 0;
	mtk_crtc->oplus_apollo_br->pending_vsync = 0;
	mtk_crtc->oplus_apollo_br->pending_limit_superior_ns = 0;
	mtk_crtc->oplus_apollo_br->pending_limit_inferior_ns = 0;
	mtk_crtc->oplus_apollo_br->pending_transfer_time_us = 0;
	mtk_crtc->oplus_apollo_br->pending_pu_enable = 0;
	mtk_crtc->oplus_apollo_br->pending_h_roi = 0;
	mtk_crtc->oplus_apollo_br->h_roi = 0;
	mtk_crtc->oplus_apollo_br->last_frame_pu_enable = 0;
	mtk_crtc->oplus_apollo_br->h_max = mtk_crtc_get_height_by_comp(__func__, crtc, NULL, true);
}

int oplus_display_backlight_property_update(struct drm_crtc *crtc, int prop_id, unsigned int prop_val)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	if (!crtc) {
		OPLUS_DSI_ERR("backlight_property_update - find crtc fail\n");
		return 0;
	}

	if (!mtk_crtc->oplus_apollo_br) {
		mtk_crtc->oplus_apollo_br = kzalloc(sizeof(struct oplus_apollo_brightness),
			GFP_KERNEL);
		OPLUS_DSI_INFO("need allocate memory\n");
	}
	if (!mtk_crtc->oplus_apollo_br) {
		OPLUS_DSI_ERR("allocate memory fail\n");
		return 0;
	}

	switch (prop_id) {
	case CRTC_PROP_HW_BRIGHTNESS:
		mtk_crtc->oplus_apollo_br->oplus_backlight_updated = true;
		mtk_crtc->oplus_apollo_br->oplus_pending_backlight = prop_val;
		apollo_set_brightness_for_show(prop_val);
		break;
	case CRTC_PROP_BRIGHTNESS_NEED_SYNC:
		mtk_crtc->oplus_apollo_br->oplus_backlight_need_sync = !!prop_val;
		break;
	default:
		break;
	}

	return 0;
}
EXPORT_SYMBOL(oplus_display_backlight_property_update);

void oplus_printf_backlight_log(struct drm_crtc *crtc, unsigned int bl_lvl) {
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp = mtk_ddp_comp_request_output(mtk_crtc);
	struct mtk_panel_params *params = mtk_crtc->panel_ext->params;
	struct timespec64 now;
	struct tm broken_time;
	static time64_t time_last = 0;
	struct backlight_log *bl_log;
	int i = 0;
	int len = 0;
	char backlight_log_buf[OPLUS_DSI_CMD_PRINT_BUF_SIZE * 2];

	if (!comp) {
		OPLUS_DSI_ERR("failed to get comp\n");
		return;
	}

	ktime_get_real_ts64(&now);
	time64_to_tm(now.tv_sec, 0, &broken_time);
	if (now.tv_sec - time_last >= BACKLIGHT_TIME_PERIOD) {
		OPLUS_DSI_INFO("<%s> time=[%02d:%02d:%02d.%03ld], backlight=%d\n",
			params->vendor, broken_time.tm_hour, broken_time.tm_min,
			broken_time.tm_sec, now.tv_nsec / 1000000, bl_lvl);
		time_last = now.tv_sec;
	}

	if (comp->id == DDP_COMPONENT_DSI1) {
		bl_log = &oplus_bl_log[DISPLAY_SECONDARY];
	} else {
		bl_log = &oplus_bl_log[DISPLAY_PRIMARY];
	}


	bl_log->backlight[bl_log->bl_count] = bl_lvl;
	bl_log->past_times[bl_log->bl_count] = now;
	bl_log->bl_count++;
	if (bl_log->bl_count >= BACKLIGHT_CACHE_MAX) {
		memset(backlight_log_buf, 0, sizeof(backlight_log_buf));
		for (i = 0; i < BACKLIGHT_CACHE_MAX; i++) {
			time64_to_tm(bl_log->past_times[i].tv_sec, 0, &broken_time);
			len += snprintf(backlight_log_buf + len, sizeof(backlight_log_buf) - len,
				"%02d:%02d:%02d.%03ld:%04X,", broken_time.tm_hour, broken_time.tm_min,
				broken_time.tm_sec, bl_log->past_times[i].tv_nsec / 1000000,
				bl_log->backlight[i]);
		}
		OPLUS_DSI_INFO("<%s> bl_count=%d, log_buf[%d]: [%s]\n",
				params->vendor, bl_log->bl_count, len, backlight_log_buf);
		bl_log->bl_count = 0;
	}
}

bool oplus_have_previous_adjacent_frame(int te_tag_ns, int vsync_ns)
{
	return (te_tag_ns > mutex_sof_ns && te_tag_ns - mutex_sof_ns < vsync_ns / 2)
			|| (mutex_sof_ns > te_tag_ns && mutex_sof_ns - te_tag_ns < vsync_ns / 2);
}

void oplus_adjust_bl_time_for_sync(struct drm_crtc *crtc, struct cmdq_pkt *cmdq_handle)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp = mtk_ddp_comp_request_output(mtk_crtc);
	int delay_us, time_gap_ns;
	bool backlight_need_sync, refresh_rate_is_switching;
	int cur_vsync, pending_vsync, rr_switch_usleep_time;
	int te_tag_ns , rr_switch_tag_ns;
	int limit_inferior_ns, limit_superior_ns, limit_superior_us, transfer_time_us;
	int pending_limit_superior_ns, pending_limit_inferior_ns, pending_transfer_time_us;
#ifdef OPLUS_FEATURE_DISPLAY_ADFR
	bool adfr_fps_need_sync = false;
#endif /* OPLUS_FEATURE_DISPLAY_ADFR */

	if (!(mtk_crtc->enabled)) {
		OPLUS_DSI_INFO("Sleep State adjust backlight timing stop --crtc not ebable\n");
		return;
	}

	if (!comp) {
		OPLUS_DSI_INFO("no output comp\n");
		return;
	}

	if (mtk_crtc->oplus_apollo_br->cur_vsync == 0) {
		mtk_crtc->oplus_apollo_br->cur_vsync = mtk_crtc->oplus_apollo_br->pending_vsync;
		mtk_crtc->oplus_apollo_br->limit_superior_ns = mtk_crtc->oplus_apollo_br->pending_limit_superior_ns;
		mtk_crtc->oplus_apollo_br->limit_inferior_ns = mtk_crtc->oplus_apollo_br->pending_limit_inferior_ns;
		mtk_crtc->oplus_apollo_br->transfer_time_us = mtk_crtc->oplus_apollo_br->pending_transfer_time_us;
	}

#ifdef OPLUS_FEATURE_DISPLAY_ADFR
	if (oplus_adfr_is_supported_export(crtc)) {
		if (mtk_crtc->oplus_apollo_br->cur_vsync == APOLLO_CUR_VSYNC_60) {
			adfr_fps_need_sync = true;
		}
	}
#endif /* OPLUS_FEATURE_DISPLAY_ADFR */

	backlight_need_sync = mtk_crtc->oplus_apollo_br->oplus_backlight_need_sync;
	refresh_rate_is_switching = mtk_crtc->oplus_apollo_br->oplus_refresh_rate_switching;

	cur_vsync = mtk_crtc->oplus_apollo_br->cur_vsync;
	rr_switch_usleep_time = cur_vsync / 1000;

	te_tag_ns = mtk_crtc->oplus_apollo_br->oplus_te_tag_ns;
	rr_switch_tag_ns = mtk_crtc->oplus_apollo_br->oplus_timing_switch_tag_ns;

	limit_inferior_ns = mtk_crtc->oplus_apollo_br->limit_inferior_ns;
	limit_superior_ns = mtk_crtc->oplus_apollo_br->limit_superior_ns;
	limit_superior_us = mtk_crtc->panel_ext->params->dyn_fps.apollo_limit_superior_us;
	transfer_time_us = mtk_crtc->oplus_apollo_br->transfer_time_us;

	pending_vsync = mtk_crtc->oplus_apollo_br->pending_vsync;
	pending_limit_superior_ns = mtk_crtc->oplus_apollo_br->pending_limit_superior_ns;
	pending_limit_inferior_ns = mtk_crtc->oplus_apollo_br->pending_limit_inferior_ns;
	pending_transfer_time_us = mtk_crtc->oplus_apollo_br->pending_transfer_time_us;

	if ((backlight_need_sync || adfr_fps_need_sync) && cur_vsync != 0) {
		/* backlight sync start */
		OPLUS_DSI_TRACE_BEGIN("cur_vsync(%d) pending_vsync(%d)", cur_vsync, pending_vsync);
		if (refresh_rate_is_switching) {
			usleep_range(rr_switch_usleep_time, rr_switch_usleep_time + 100);
			if (rr_switch_tag_ns > te_tag_ns) {
				te_tag_ns = mtk_crtc->oplus_apollo_br->oplus_te_tag_ns;
				time_gap_ns = ktime_get() > te_tag_ns ? ktime_get() - te_tag_ns : 0;
				if (time_gap_ns > limit_inferior_ns) {
					/* update part of apollo para */
					OPLUS_DSI_TRACE_BEGIN("update part of apollo para");
					limit_superior_ns = pending_limit_superior_ns;
					limit_inferior_ns = pending_limit_inferior_ns;
					transfer_time_us = pending_transfer_time_us;
					OPLUS_DSI_TRACE_END("update part of apollo para");
				}
			} else {
				/* update apollo para */
				OPLUS_DSI_TRACE_BEGIN("update apollo para");
				cur_vsync = pending_vsync;
				limit_superior_ns = pending_limit_superior_ns;
				limit_inferior_ns = pending_limit_inferior_ns;
				transfer_time_us = pending_transfer_time_us;
				OPLUS_DSI_TRACE_END("update apollo para");
			}
		}
		te_tag_ns = mtk_crtc->oplus_apollo_br->oplus_te_tag_ns;
		time_gap_ns = ktime_get() > te_tag_ns ? ktime_get() - te_tag_ns : 0;
		if (time_gap_ns >= 0 && time_gap_ns <= cur_vsync) {
			OPLUS_DSI_TRACE_BEGIN("cur_vsync(%d)", cur_vsync);
			if (time_gap_ns < limit_superior_ns) {
				if (oplus_have_previous_adjacent_frame(te_tag_ns, cur_vsync)) {
					delay_us = limit_superior_us - transfer_time_us;
				} else {
					delay_us = (limit_superior_ns - time_gap_ns) / 1000;
				}
				if (delay_us > 0) {
					OPLUS_DSI_TRACE_BEGIN("cmdq sleep %d us-0", delay_us);
					cmdq_pkt_sleep(cmdq_handle, CMDQ_US_TO_TICK(delay_us), mtk_get_gpr(comp, cmdq_handle));
					OPLUS_DSI_TRACE_END("cmdq sleep %d us-0", delay_us);
				}
			} else if (time_gap_ns > limit_inferior_ns) {
				delay_us = (cur_vsync - time_gap_ns) / 1000 + limit_superior_us;
				if (delay_us > 0) {
					OPLUS_DSI_TRACE_BEGIN("cmdq sleep %d us-1", delay_us);
					cmdq_pkt_sleep(cmdq_handle, CMDQ_US_TO_TICK(delay_us), mtk_get_gpr(comp, cmdq_handle));
					OPLUS_DSI_TRACE_END("cmdq sleep %d us-1", delay_us);
				}
			}
			OPLUS_DSI_TRACE_END("cur_vsync(%d)", cur_vsync);
		} else {
			OPLUS_DSI_ERR("backlight sync failed ! time_gap_ns(%d) is large than cur_vsync(%d)\n", time_gap_ns, cur_vsync);
		}
		OPLUS_DSI_TRACE_END("cur_vsync(%d) pending_vsync(%d)", cur_vsync, pending_vsync);
		/* backlight sync end */
	}
}

/* copy from mtk_drm_setbacklight() mtk_drm_crtc.c*/
int mtk_drm_setbacklight_without_lock(struct drm_crtc *crtc, unsigned int level,
	unsigned int panel_ext_param, unsigned int cfg_flag)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct cmdq_pkt *cmdq_handle;
	struct mtk_ddp_comp *comp = mtk_ddp_comp_request_output(mtk_crtc);
	struct mtk_ddp_comp *oddmr_comp;
	struct mtk_cmdq_cb_data *cb_data;
	struct mtk_bl_ext_config bl_ext_config;
	static unsigned int bl_cnt;
	bool is_frame_mode;
	struct cmdq_client *client;
	int index = drm_crtc_index(crtc);
	int ret = 0;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
/*ifdef OPLUS_FEATURE_DISPLAY_APOLLO*/
	if (mtk_crtc->oplus_apollo_br->oplus_power_on == false) {
		OPLUS_DSI_ERR("oplus_power_on false\n");
		level = 0;
	}
/*endif OPLUS_FEATURE_DISPLAY_APOLLO*/
	CRTC_MMP_EVENT_START(index, backlight, (unsigned long)crtc,
			level);

	if (!(mtk_crtc->enabled)) {
		OPLUS_DSI_INFO("Sleep State set backlight stop --crtc not ebable\n");
		CRTC_MMP_EVENT_END(index, backlight, 0, 0);

		return -EINVAL;
	}

	if (!comp) {
		OPLUS_DSI_INFO("no output comp\n");
		CRTC_MMP_EVENT_END(index, backlight, 0, 1);

		return -EINVAL;
	}

	cb_data = kmalloc(sizeof(*cb_data), GFP_KERNEL);
	if (!cb_data) {
		OPLUS_DSI_ERR("cb data creation failed\n");
		CRTC_MMP_EVENT_END(index, backlight, 0, 2);
		return -EINVAL;
	}

	is_frame_mode = mtk_crtc_is_frame_trigger_mode(&mtk_crtc->base);

	/* setbacklight would use CLIENT_DSI_CFG in DSI VDO mode only */
	client = (is_frame_mode) ? mtk_crtc->gce_obj.client[CLIENT_CFG] :
				mtk_crtc->gce_obj.client[CLIENT_DSI_CFG];
	mtk_crtc_pkt_create(&cmdq_handle, crtc, client);

	if (!cmdq_handle) {
		OPLUS_DSI_ERR("NULL cmdq handle\n");
		return -EINVAL;
	}

	if (mtk_crtc_with_sub_path(crtc, mtk_crtc->ddp_mode))
		mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle,
			DDP_SECOND_PATH, 0);
	else
		mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle,
			DDP_FIRST_PATH, 0);

	if (is_frame_mode) {
		cmdq_pkt_clear_event(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_STREAM_BLOCK]);
		cmdq_pkt_wfe(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_CABC_EOF]);
	}
/*ifdef OPLUS_FEATURE_DISPLAY_APOLLO*/
	drm_trace_tag_value("sync_backlight", level);
	oplus_adjust_bl_time_for_sync(crtc, cmdq_handle);
/*endif OPLUS_FEATURE_DISPLAY_APOLLO*/


	oddmr_comp = priv->ddp_comp[DDP_COMPONENT_ODDMR0];
	mtk_ddp_comp_io_cmd(oddmr_comp, cmdq_handle, ODDMR_BL_CHG, &level);

	oplus_printf_backlight_log(crtc, level);
	if ((cfg_flag & (0x1 << SET_BACKLIGHT_LEVEL)) && !(cfg_flag & (0x1 << SET_ELVSS_PN))) {
		OPLUS_DSI_DEBUG("cfg_flag = %d, level=%d\n", cfg_flag, level);
		oplus_display_panel_set_pwm_bl(crtc, cmdq_handle, level, true);
	} else {
		/* set backlight and elvss */
		bl_ext_config.cfg_flag = cfg_flag;
		bl_ext_config.backlight_level = level;
		bl_ext_config.elvss_pn = panel_ext_param;
		if (comp && comp->funcs && comp->funcs->io_cmd)
			comp->funcs->io_cmd(comp, cmdq_handle, DSI_SET_BL_ELVSS, &bl_ext_config);
	}
	if (is_frame_mode) {
		cmdq_pkt_set_event(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_CABC_EOF]);
		cmdq_pkt_set_event(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_STREAM_BLOCK]);
	}

	CRTC_MMP_MARK(index, backlight, bl_cnt, 0);
	drm_trace_tag_mark("backlight");
	bl_cnt++;

	cb_data->crtc = crtc;
	cb_data->cmdq_handle = cmdq_handle;

	if (cmdq_pkt_flush_threaded(cmdq_handle, oplus_bl_cmdq_cb, cb_data) < 0) {
		OPLUS_DSI_ERR("failed to flush oplus_bl_cmdq_cb\n");
		ret = -EINVAL;
	}

	CRTC_MMP_EVENT_END(index, backlight, (unsigned long)crtc,
			level);

	return ret;
}

void oplus_sync_panel_brightness(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	if (!crtc) {
		OPLUS_DSI_ERR("sync_panel_brightness - find crtc fail\n");
		return;
	}

	if (!mtk_crtc->oplus_apollo_br) {
		mtk_crtc->oplus_apollo_br = kzalloc(sizeof(struct oplus_apollo_brightness),
			GFP_KERNEL);
		OPLUS_DSI_INFO("oplus_apollo_brightness need allocate memory\n");
	}

	if (!mtk_crtc->oplus_apollo_br) {
		OPLUS_DSI_ERR("oplus_apollo_brightness allocate memory fail\n");
		return;
	}

#ifdef OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT
	if (oplus_ofp_is_supported()) {
		oplus_ofp_lhbm_backlight_update(crtc);
	}
#endif /* OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT */

	if (mtk_crtc->panel_ext && mtk_crtc->panel_ext->params && mtk_crtc->panel_ext->params->dyn_fps.vact_timing_fps != 0) {
		mtk_crtc->oplus_apollo_br->pending_vsync = 1000000000 / mtk_crtc->panel_ext->params->dyn_fps.vact_timing_fps;
		mtk_crtc->oplus_apollo_br->pending_limit_superior_ns = mtk_crtc->panel_ext->params->dyn_fps.apollo_limit_superior_us * 1000;
		mtk_crtc->oplus_apollo_br->pending_limit_inferior_ns = mtk_crtc->panel_ext->params->dyn_fps.apollo_limit_inferior_us * 1000;
		mtk_crtc->oplus_apollo_br->pending_transfer_time_us = mtk_crtc->panel_ext->params->dyn_fps.apollo_transfer_time_us;
		mtk_crtc->oplus_apollo_br->last_frame_pu_enable = mtk_crtc->oplus_apollo_br->pending_pu_enable;
		mtk_crtc->oplus_apollo_br->h_roi = mtk_crtc->oplus_apollo_br->pending_h_roi;
	} else {
		OPLUS_DSI_ERR("fps get failed!\n");
	}

	if (!mtk_crtc->oplus_apollo_br->oplus_backlight_updated) {
		return;
	}

	/* oplus_sync_panel_brightness must after mode switch! */
	if (mtk_crtc->oplus_apollo_br->oplus_refresh_rate_switching) {
		if (atomic_read(&mtk_crtc->singal_for_mode_switch)) {
			wait_event_interruptible(mtk_crtc->mode_switch_end_wq,
				(atomic_read(&mtk_crtc->singal_for_mode_switch) == 0));
		}
	}

	OPLUS_DSI_TRACE_BEGIN("sync_panel_brightness level(%d) sync(%d)", mtk_crtc->oplus_apollo_br->oplus_pending_backlight,
							mtk_crtc->oplus_apollo_br->oplus_backlight_need_sync);
	mtk_drm_setbacklight_without_lock(crtc, mtk_crtc->oplus_apollo_br->oplus_pending_backlight, 0, 0x1 << SET_BACKLIGHT_LEVEL);
	OPLUS_DSI_TRACE_END("sync_panel_brightness level(%d) sync(%d)", mtk_crtc->oplus_apollo_br->oplus_pending_backlight,
							mtk_crtc->oplus_apollo_br->oplus_backlight_need_sync);
}
EXPORT_SYMBOL(oplus_sync_panel_brightness);

void oplus_update_apollo_para(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	if (!crtc) {
		OPLUS_DSI_ERR("sync_panel_brightness - find crtc fail\n");
		return;
	}

	if (!mtk_crtc->oplus_apollo_br) {
		mtk_crtc->oplus_apollo_br = kzalloc(sizeof(struct oplus_apollo_brightness),
			GFP_KERNEL);
		OPLUS_DSI_INFO("oplus_apollo_brightness need allocate memory\n");
	}

	if (!mtk_crtc->oplus_apollo_br) {
		OPLUS_DSI_ERR("oplus_apollo_brightness allocate memory fail\n");
		return;
	}

	mtk_crtc->oplus_apollo_br->oplus_refresh_rate_switching = false;
	mtk_crtc->oplus_apollo_br->cur_vsync = mtk_crtc->oplus_apollo_br->pending_vsync;
	mtk_crtc->oplus_apollo_br->limit_superior_ns = mtk_crtc->oplus_apollo_br->pending_limit_superior_ns;
	mtk_crtc->oplus_apollo_br->limit_inferior_ns = mtk_crtc->oplus_apollo_br->pending_limit_inferior_ns;
	/*If the ROI of the PU is updated before the oplus_update_apollo_para is executed, need to cache the status of the PU_Enable*/
	if (mtk_crtc->oplus_apollo_br->last_frame_pu_enable) {
		mtk_crtc->oplus_apollo_br->transfer_time_us =
			mtk_crtc->oplus_apollo_br->pending_transfer_time_us * mtk_crtc->oplus_apollo_br->h_roi / mtk_crtc->oplus_apollo_br->h_max;
	} else {
		mtk_crtc->oplus_apollo_br->transfer_time_us = mtk_crtc->oplus_apollo_br->pending_transfer_time_us;
	}
	mtk_crtc->oplus_apollo_br->pending_pu_enable = 0;
}
EXPORT_SYMBOL(oplus_update_apollo_para);
