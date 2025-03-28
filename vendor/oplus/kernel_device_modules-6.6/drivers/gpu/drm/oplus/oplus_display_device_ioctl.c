/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_display_device_ioctl.c
** Description : oplus display device ioctl
** Version : 1.0
** Date : 2024/05/20
** Author : Display
******************************************************************/
#include "oplus_display_device_ioctl.h"
#include "oplus_display_debug.h"
#include "oplus_display_sysfs_attrs.h"
#include "oplus_display_device.h"
#include "oplus_dsi_display_config.h"
#include "oplus_display_utils.h"
#ifdef OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT
#include "oplus_display_onscreenfingerprint.h"
#endif /* OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT */
#include "mtk_log.h"
#include "mtk_drm_mmp.h"

bool g_is_mode_switch_pack = false;
EXPORT_SYMBOL(g_is_mode_switch_pack);
int g_last_mode_idx = 0;
EXPORT_SYMBOL(g_last_mode_idx);
/* pcp: panel_cmdq_pkg */
wait_queue_head_t oplus_pcp_lock_clear_wq;
EXPORT_SYMBOL(oplus_pcp_lock_clear_wq);
struct task_struct *oplus_pcp_task;
EXPORT_SYMBOL(oplus_pcp_task);
struct mutex oplus_pcp_lock;
EXPORT_SYMBOL(oplus_pcp_lock);
atomic_t oplus_pcp_handle_lock;
EXPORT_SYMBOL(oplus_pcp_handle_lock);
atomic_t oplus_pcp_num;
EXPORT_SYMBOL(oplus_pcp_num);

extern unsigned int cabc_mode;
extern unsigned int cabc_true_mode;
extern unsigned int cabc_sun_flag;
extern unsigned int cabc_back_flag;
extern void disp_aal_set_dre_en(struct mtk_ddp_comp *comp, int enable);
extern unsigned int silence_mode;
extern unsigned int oplus_display_brightness;
extern unsigned int oplus_max_normal_brightness;
extern unsigned int oplus_max_brightness;
extern uint64_t serial_number;
extern unsigned int esd_mode;
extern int shutdown_flag;
extern unsigned int seed_mode;
extern bool g_dp_support;
extern bool pq_trigger;
extern unsigned int get_project(void);
extern char lcm_version[32];
extern char lcm_manufacture[32];
extern bool oplus_hbm_max_en;
extern unsigned long long oplus_last_te_time;
DEFINE_MUTEX(oplus_seed_lock);
DEFINE_MUTEX(g_oplus_hbm_max_lock);
DEFINE_MUTEX(g_oplus_hbm_max_switch_lock);

extern struct drm_device *get_drm_device(void);
extern int mtk_drm_setbacklight(struct drm_crtc *crtc, unsigned int level, unsigned int panel_ext_param, unsigned int cfg_flag, unsigned int lock);

extern int oplus_mtk_drm_setcabc(struct drm_crtc *crtc, unsigned int hbm_mode);
extern int oplus_mtk_drm_setseed(struct drm_crtc *crtc, unsigned int seed_mode);
extern void mtk_drm_send_lcm_cmd_prepare_wait_for_vsync(struct drm_crtc *crtc, struct cmdq_pkt **cmdq_handle);
extern void mtk_crtc_wait_frame_done(struct mtk_drm_crtc *mtk_crtc,
		struct cmdq_pkt *cmdq_handle,
		enum CRTC_DDP_PATH ddp_path,
		int clear_event);
extern u16 mtk_get_gpr(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle);

enum {
	CABC_LEVEL_0,
	CABC_LEVEL_1,
	CABC_LEVEL_2 = 3,
	CABC_EXIT_SPECIAL = 8,
	CABC_ENTER_SPECIAL = 9,
};

enum PANEL_STAGE {
	PANEL_PVT,
	PANEL_DVT,
	PANEL_EVT,
	PANEL_T0,
	PANEL_DEFAULT,
};

int oplus_display_panel_set_brightness(void *buf)
{
	struct drm_crtc *crtc;
	struct drm_device *ddev = get_drm_device();
	unsigned int *set_brightness = buf;
	unsigned int oplus_set_brightness = (*set_brightness);

	OPLUS_DSI_INFO("set brightness: %d\n", oplus_set_brightness);

	if (oplus_set_brightness > oplus_max_brightness) {
		OPLUS_DSI_ERR("brightness:%d out of scope\n", oplus_set_brightness);
		return -1;
	}

	/* this debug cmd only for crtc0 */
	crtc = list_first_entry(&(ddev)->mode_config.crtc_list,
				typeof(*crtc), head);
	if (!crtc) {
		OPLUS_DSI_ERR("find crtc fail\n");
		return -1;
	}
	mtk_drm_setbacklight(crtc, oplus_set_brightness, 0, 0x1 << SET_BACKLIGHT_LEVEL, 1);

	return 0;
}

int oplus_display_panel_get_brightness(void *buf)
{
	unsigned int *brightness = buf;

	(*brightness) = oplus_display_brightness;

	return 0;
}

int oplus_display_panel_get_max_brightness(void *buf)
{
	unsigned int *brightness = buf;

	(*brightness) = oplus_max_normal_brightness;

	return 0;
}

int oplus_display_panel_get_panel_bpp(void *buf)
{
	unsigned int *panel_bpp = buf;
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct drm_device *ddev = get_drm_device();

	/* this debug cmd only for crtc0 */
	crtc = list_first_entry(&(ddev)->mode_config.crtc_list,
				typeof(*crtc), head);
	if (!crtc) {
		OPLUS_DSI_ERR("find crtc fail\n");
		return -1;
	}
	mtk_crtc = to_mtk_crtc(crtc);
	if (!mtk_crtc || !mtk_crtc->panel_ext || !mtk_crtc->panel_ext->params) {
		OPLUS_DSI_ERR("falied to get lcd proc info\n");
		return -EINVAL;
	}

	(*panel_bpp) = mtk_crtc->panel_ext->params->panel_bpp;
	OPLUS_DSI_INFO("panel_bpp: %d\n", *panel_bpp);

	return 0;
}

int oplus_display_panel_get_serial_number(void *buf)
{
	struct panel_serial_number *p_snumber = buf;
	int ret = 0;
	struct drm_crtc *crtc;
	struct drm_device *ddev = get_drm_device();

	/* this debug cmd only for crtc0 */
	crtc = list_first_entry(&(ddev)->mode_config.crtc_list,
		typeof(*crtc), head);
	if (!crtc) {
		OPLUS_DSI_ERR("get_panel_serial_number find crtc fail\n");
		return 0;
	}

	if (oplus_display_brightness == 0) {
		OPLUS_DSI_WARN("backlight is 0, skip get serial number!\n");
		return 0;
	}

	if (serial_number == 0) {
		oplus_panel_serial_number_read(crtc);
		OPLUS_DSI_INFO("Read serial_number=0x%016lX, DA=0x%02X, DB=0x%02X, DC=0x%02X\n",
				serial_number, m_da, m_db, m_dc);
	}

	OPLUS_DSI_INFO("serial number=%lX\n", serial_number);
	ret = scnprintf(p_snumber->serial_number, sizeof(p_snumber->serial_number)+1,
			"Get panel serial number: %lX\n", serial_number);

	return ret;
}

int oplus_display_panel_get_cabc(void *buf)
{
	unsigned int *c_mode = buf;

	OPLUS_DSI_INFO("CABC_mode=%d\n", cabc_true_mode);
	*c_mode = cabc_true_mode;

	return 0;
}

int oplus_display_panel_set_cabc(void *buf)
{
	struct drm_crtc *crtc;
	struct drm_device *ddev = get_drm_device();
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_ddp_comp *comp;
	uint32_t *cabc_mode_temp = buf;

	/* this debug cmd only for crtc0 */
	crtc = list_first_entry(&(ddev)->mode_config.crtc_list,
				typeof(*crtc), head);
	if (!crtc) {
		OPLUS_DSI_ERR("find crtc fail\n");
		return -1;
	}

	mtk_crtc = to_mtk_crtc(crtc);

	comp = mtk_ddp_comp_sel_in_cur_crtc_path(mtk_crtc, MTK_DISP_AAL, 0);
	if (!comp) {
		OPLUS_DSI_ERR("comp is null!\n");
		return 0;
	}

	cabc_mode = *cabc_mode_temp;
	cabc_true_mode = cabc_mode;
	OPLUS_DSI_INFO("cabc mode is %d, cabc_back_flag is %d\n", cabc_mode, cabc_back_flag);
	if (cabc_mode < 4) {
		cabc_back_flag = cabc_mode;
	}

	if (cabc_mode == CABC_ENTER_SPECIAL) {
		cabc_sun_flag = 1;
		cabc_true_mode = 0;
	} else if (cabc_mode == CABC_EXIT_SPECIAL) {
		cabc_sun_flag = 0;
		cabc_true_mode = cabc_back_flag;
	} else if (cabc_sun_flag == 1) {
		if (cabc_back_flag == CABC_LEVEL_0 || mtk_crtc->panel_ext->params->oplus_display_global_dre) {
			disp_aal_set_dre_en(comp, 1);
			OPLUS_DSI_INFO("sun enable dre\n");
		} else {
			disp_aal_set_dre_en(comp, 0);
			OPLUS_DSI_INFO("sun disable dre\n");
		}
		return 0;
	}

	OPLUS_DSI_INFO("cabc mode is %d\n", cabc_true_mode);

	if ((cabc_true_mode == CABC_LEVEL_0 && cabc_back_flag == CABC_LEVEL_0)  || mtk_crtc->panel_ext->params->oplus_display_global_dre) {
		disp_aal_set_dre_en(comp, 1);
		OPLUS_DSI_INFO("enable dre\n");
	} else {
		disp_aal_set_dre_en(comp, 0);
		OPLUS_DSI_INFO("disable dre\n");
	}
	oplus_mtk_drm_setcabc(crtc, cabc_true_mode);
	if (cabc_true_mode != cabc_back_flag) {
		cabc_true_mode = cabc_back_flag;
	}

	return 0;
}



int oplus_display_panel_get_closebl_flag(void *buf)
{
	unsigned int *closebl_flag = buf;

	OPLUS_DSI_INFO("silence_mode=%d\n", silence_mode);
	(*closebl_flag) = silence_mode;

	return 0;
}

int oplus_display_panel_set_closebl_flag(void *buf)
{
	unsigned int *closebl_flag = buf;

	msleep(1000);
	silence_mode = (*closebl_flag);
	OPLUS_DSI_INFO("silence_mode=%d\n", silence_mode);

	return 0;
}

int oplus_display_panel_get_esd(void *buf)
{
	unsigned int *p_esd = buf;

	OPLUS_DSI_INFO("esd=%d\n", esd_mode);
	(*p_esd) = esd_mode;

	return 0;
}

int oplus_display_panel_set_esd(void *buf)
{
	unsigned int *p_esd = buf;

	esd_mode = (*p_esd);
	OPLUS_DSI_INFO("esd mode is %d\n", esd_mode);

	return 0;
}

int oplus_display_panel_get_vendor(void *buf)
{
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct panel_info *p_info = buf;
	struct drm_device *ddev = get_drm_device();

	/* this debug cmd only for crtc0 */
	crtc = list_first_entry(&(ddev)->mode_config.crtc_list,
				typeof(*crtc), head);
	if (!crtc) {
		OPLUS_DSI_ERR("find crtc fail,p_info=%p\n", p_info);
		return -1;
	}

	mtk_crtc = to_mtk_crtc(crtc);
	if (!mtk_crtc || !mtk_crtc->panel_ext || !mtk_crtc->panel_ext->params) {
		OPLUS_DSI_ERR("falied to get lcd proc info\n");
		return -EINVAL;
	}

	OPLUS_DSI_INFO("get lcd proc info: vendor[%s] lcm_version[%s] lcm_manufacture[%s]\n", mtk_crtc->panel_ext->params->vendor,
			lcm_version, lcm_manufacture);
	if (!strcmp(lcm_version, "")) {
		memcpy(p_info->version, mtk_crtc->panel_ext->params->vendor,
				sizeof(mtk_crtc->panel_ext->params->vendor) >= 31?31:(sizeof(mtk_crtc->panel_ext->params->vendor)+1));
	} else {
		memcpy(p_info->version, lcm_version,
				sizeof(lcm_version) >= 31?31:(sizeof(lcm_version)+1));
	}

	if (!strcmp(lcm_manufacture, "")) {
		memcpy(p_info->manufacture, mtk_crtc->panel_ext->params->manufacture,
				sizeof(mtk_crtc->panel_ext->params->manufacture) >= 31?31:(sizeof(mtk_crtc->panel_ext->params->manufacture)+1));
	} else {
		memcpy(p_info->manufacture, lcm_manufacture,
				sizeof(lcm_manufacture) >= 31?31:(sizeof(lcm_manufacture)+1));
	}

	return 0;
}

int oplus_display_get_softiris_color_status(void *buf)
{
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct drm_device *ddev = get_drm_device();
	struct softiris_color *iris_color_status = buf;

	/* this debug cmd only for crtc0 */
	crtc = list_first_entry(&(ddev)->mode_config.crtc_list,
				typeof(*crtc), head);
	if (!crtc) {
		OPLUS_DSI_ERR("find crtc fail\n");
		return -1;
	}
	mtk_crtc = to_mtk_crtc(crtc);
	if (!mtk_crtc || !mtk_crtc->panel_ext || !mtk_crtc->panel_ext->params) {
		OPLUS_DSI_ERR("falied to get lcd proc info\n");
		return -EINVAL;
	}

	iris_color_status->color_vivid_status = mtk_crtc->panel_ext->params->color_vivid_status;
	iris_color_status->color_srgb_status = mtk_crtc->panel_ext->params->color_srgb_status;
	iris_color_status->color_softiris_status = mtk_crtc->panel_ext->params->color_softiris_status;
	iris_color_status->color_dual_panel_status = mtk_crtc->panel_ext->params->color_dual_panel_status;
	iris_color_status->color_dual_brightness_status = mtk_crtc->panel_ext->params->color_dual_brightness_status;
	iris_color_status->color_oplus_calibrate_status = mtk_crtc->panel_ext->params->color_oplus_calibrate_status;
	iris_color_status->color_samsung_status = mtk_crtc->panel_ext->params->color_samsung_status;
	iris_color_status->color_loading_status = mtk_crtc->panel_ext->params->color_loading_status;
	iris_color_status->color_2nit_status = mtk_crtc->panel_ext->params->color_2nit_status;
	iris_color_status->color_nature_profession_status = mtk_crtc->panel_ext->params->color_nature_profession_status;
	OPLUS_DSI_INFO("oplus_color_vivid_status: %s", iris_color_status->color_vivid_status ? "true" : "false");
	OPLUS_DSI_INFO("oplus_color_srgb_status: %s", iris_color_status->color_srgb_status ? "true" : "false");
	OPLUS_DSI_INFO("oplus_color_softiris_status: %s", iris_color_status->color_softiris_status ? "true" : "false");
	OPLUS_DSI_INFO("color_dual_panel_status: %s", iris_color_status->color_dual_panel_status ? "true" : "false");
	OPLUS_DSI_INFO("color_dual_brightness_status: %s", iris_color_status->color_dual_brightness_status ? "true" : "false");
	OPLUS_DSI_INFO("color_samsung_status: %s", iris_color_status->color_samsung_status ? "true" : "false");
	OPLUS_DSI_INFO("color_loading_status: %s", iris_color_status->color_loading_status ? "true" : "false");
	OPLUS_DSI_INFO("color_2nit_status: %s", iris_color_status->color_2nit_status ? "true" : "false");
	OPLUS_DSI_INFO("color_nature_profession_status: %s", iris_color_status->color_nature_profession_status ? "true" : "false");

	return 0;
}

int oplus_mtk_drm_setseed(struct drm_crtc *crtc, unsigned int seed_mode)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct cmdq_pkt *cmdq_handle;
	struct mtk_crtc_state *crtc_state;
	struct mtk_ddp_comp *comp = mtk_ddp_comp_request_output(mtk_crtc);

	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

	crtc_state = to_mtk_crtc_state(crtc->state);
	if (!mtk_crtc->enabled || crtc_state->prop_val[CRTC_PROP_DOZE_ACTIVE]) {
		DDPINFO("crtc is not reusmed!\n");
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		return -EINVAL;
	}

	mtk_drm_send_lcm_cmd_prepare(crtc, &cmdq_handle);

	/* set hbm */
	if (comp && comp->funcs && comp->funcs->io_cmd)
		comp->funcs->io_cmd(comp, cmdq_handle, LCM_SEED, &seed_mode);

	mtk_drm_send_lcm_cmd_flush(crtc, &cmdq_handle, 0);
	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

	return 0;
}

int oplus_display_panel_get_seed(void *buf)
{
	unsigned int *seed = buf;

	mutex_lock(&oplus_seed_lock);
	OPLUS_DSI_INFO("seed_mode=%d\n", seed_mode);
	(*seed) = seed_mode;
	mutex_unlock(&oplus_seed_lock);

	return 0;
}

int oplus_display_panel_set_seed(void *buf)
{
	struct drm_crtc *crtc;
	struct drm_device *ddev = get_drm_device();
	unsigned int *seed_mode_tmp = buf;

	OPLUS_DSI_INFO("%d to be %d\n", seed_mode, *seed_mode_tmp);

	/* this debug cmd only for crtc0 */
	crtc = list_first_entry(&(ddev)->mode_config.crtc_list,
				typeof(*crtc), head);
	if (!crtc) {
		OPLUS_DSI_ERR("find crtc fail\n");
		return -1;
	}
	mutex_lock(&oplus_seed_lock);
	oplus_mtk_drm_setseed(crtc, *seed_mode_tmp);
	seed_mode = (*seed_mode_tmp);
	mutex_unlock(&oplus_seed_lock);

	return 0;
}

int oplus_display_panel_get_id(void *buf)
{
	struct panel_id *panel_rid = buf;

	OPLUS_DSI_INFO("0xDA=0x%02X, 0xDB=0x%02X, 0xDC=0x%02X\n", m_da, m_db, m_dc);

	panel_rid->DA = (uint32_t)m_da;
	panel_rid->DB = (uint32_t)m_db;
	panel_rid->DC = (uint32_t)m_dc;

	return 0;
}

int oplus_display_get_dp_support(void *buf)
{
	uint32_t *dp_support = buf;

	OPLUS_DSI_INFO("dp_support = %s\n", g_dp_support ? "true" : "false");

	*dp_support = g_dp_support;

	return 0;
}
int oplus_display_panel_get_pq_trigger(void *buf)
{
	unsigned int *pq_trigger_flag = buf;

	OPLUS_DSI_INFO("pq_trigger=%d\n", pq_trigger);
	(*pq_trigger_flag) = pq_trigger;

	return 0;
}

int oplus_display_panel_set_pq_trigger(void *buf)
{
	unsigned int *pq_trigger_flag = buf;

	pq_trigger = (*pq_trigger_flag);
	OPLUS_DSI_INFO("pq_trigger=%d\n", pq_trigger);

	return 0;
}

int g_need_read_reg = 0;
void oplus_te_check(struct mtk_drm_crtc *mtk_crtc, unsigned long long te_time_diff)
{
	struct drm_crtc *crtc = NULL;
	static int refresh_rate;
	static int vsync_period;
	static int last_refresh_rate = 0;
	static int count = 0;
	static int entry_count = 0;
	static int need_te_check = 0;

	if (!mtk_crtc) {
		OPLUS_DSI_ERR("oplus_te_check mtk_crtc is null");
		return;
	}

	crtc = &mtk_crtc->base;

	if (oplus_display_brightness < 1) {
		return;
	}

	if (mtk_crtc->panel_ext && mtk_crtc->panel_ext->params
		&& mtk_crtc->panel_ext->params->dyn_fps.vact_timing_fps != 0)
		refresh_rate = mtk_crtc->panel_ext->params->dyn_fps.vact_timing_fps;

	if (refresh_rate != last_refresh_rate && crtc->state->mode.hskew != 2) {
		/* OPLUS_DSI_INFO("need_te_check, refresh_rate = %d, last_refresh_rate = %d\n", refresh_rate, last_refresh_rate); */
		need_te_check = 1;
		count = 0;
		vsync_period = 1000000 / refresh_rate;
	}
	last_refresh_rate = refresh_rate;

	if (need_te_check == 0) {
		return;
	}

	/* OPLUS_DSI_INFO("refresh_rate = %d, vsync_period = %d, te_time_diff = %lu\n", refresh_rate, vsync_period, te_time_diff); */
	if (abs(te_time_diff / 1000 - vsync_period) > vsync_period / 5) { /* The error is more than 20% */
		count++;
		if (count > 10) { /* TE error for 10 consecutive frames */
			OPLUS_DSI_ERR("oplus_te_check failed, refresh_rate=%d, te_time_diff=%llu\n", refresh_rate, te_time_diff);
			g_need_read_reg = 1;
			need_te_check = 0;
			count = 0;
		}
	} else {
		entry_count++;
		if (entry_count > 100) { /* 100 consecutive frames is no problem, clear the count */
			entry_count = 0;
			count = 0;
			need_te_check = 0;
		}
	}
}

int oplus_display_panel_get_panel_type(void *buf)
{
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct drm_device *ddev = get_drm_device();
	unsigned int *panel_type = buf;

	/* this debug cmd only for crtc0 */
	crtc = list_first_entry(&(ddev)->mode_config.crtc_list,
				typeof(*crtc), head);
	if (!crtc) {
		OPLUS_DSI_ERR("find crtc fail\n");
		return -1;
	}
	mtk_crtc = to_mtk_crtc(crtc);
	if (!mtk_crtc || !mtk_crtc->panel_ext || !mtk_crtc->panel_ext->params) {
		OPLUS_DSI_ERR("falied to get lcd proc info\n");
		return -EINVAL;
	}

	*panel_type = mtk_crtc->panel_ext->params->panel_type;
	OPLUS_DSI_INFO("panel_type = %d\n", *panel_type);

	return 0;
}

int oplus_display_panel_set_hbm_max_switch(struct drm_crtc *crtc, unsigned int en)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp;
	struct cmdq_pkt *cmdq_handle;


	OPLUS_DSI_INFO("en=%d\n", en);
	mutex_lock(&g_oplus_hbm_max_switch_lock);

	if (!mtk_crtc->enabled) {
		OPLUS_DSI_ERR("crtc not enabled\n");
		goto done;
	}

	comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (!comp) {
		OPLUS_DSI_ERR("request output fail\n");
		mutex_unlock(&g_oplus_hbm_max_switch_lock);
		return -EINVAL;
	}

	mtk_drm_send_lcm_cmd_prepare_wait_for_vsync(crtc, &cmdq_handle);
	mtk_ddp_comp_io_cmd(comp, cmdq_handle, DSI_SET_HBM_MAX, &en);
	mtk_drm_send_lcm_cmd_flush(crtc, &cmdq_handle, 0);

done:
	mutex_unlock(&g_oplus_hbm_max_switch_lock);

	return 0;
}


int oplus_display_panel_set_hbm_max(void *data)
{
	struct drm_crtc *crtc;
	struct drm_device *ddev = get_drm_device();
	unsigned int enabled = *((unsigned int*)data);

	crtc = list_first_entry(&(ddev)->mode_config.crtc_list,
				typeof(*crtc), head);
	if (!crtc) {
		OPLUS_DSI_ERR("find crtc fail\n");
		return -EINVAL;
	}

	OPLUS_DSI_INFO("set hbm_max status: %d\n", enabled);
	if ((bool)enabled == oplus_hbm_max_en) {
		OPLUS_DSI_WARN("skip setting duplicate hbm_apl status: %d\n", enabled);
		return -EINVAL;
	}

	oplus_hbm_max_en = (bool)enabled;
	oplus_display_panel_set_hbm_max_switch(crtc, enabled);

	return 0;
}

int oplus_display_panel_get_hbm_max(void *data)
{
	bool *enabled = (bool*)data;

	mutex_lock(&g_oplus_hbm_max_lock);
	*enabled = oplus_hbm_max_en;
	mutex_unlock(&g_oplus_hbm_max_lock);
	OPLUS_DSI_INFO("get hbm_apl status: %d\n", *enabled);

	return 0;
}

int oplus_display_panel_get_stage(void *buf)
{
	unsigned int *panel_stage = (unsigned int*)buf;
	struct device_node *alps_node;

	alps_node = of_find_node_by_path("/odm/oplus_sensor/light_1");
	if (alps_node) {
		if (of_property_read_u32(alps_node, "panel_stage", panel_stage) == 0) {
			OPLUS_DSI_INFO("get panel_stage: %d\n", *panel_stage);
		} else {
			*panel_stage = PANEL_DEFAULT;
			OPLUS_DSI_ERR("get panel_stage fail default: %d\n", *panel_stage);
		}
		of_node_put(alps_node);
	} else {
		*panel_stage = PANEL_DEFAULT;
		OPLUS_DSI_ERR("can't find alps_node get panel_stage fail default: %d\n", *panel_stage);
	}

	return 0;
}

ssize_t oplus_get_hbm_max_debug(struct kobject *obj,
	struct kobj_attribute *attr, char *buf)
{
	unsigned int enabled = false;

	mutex_lock(&g_oplus_hbm_max_lock);
	enabled = oplus_hbm_max_en;
	mutex_unlock(&g_oplus_hbm_max_lock);
	OPLUS_DSI_INFO("get hbm_max status: %d\n", enabled);

	return sysfs_emit(buf, "%d\n", enabled);
}

ssize_t oplus_set_hbm_max_debug(struct kobject *obj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	unsigned int enabled = 0;
	int rc = 0;

	rc = kstrtou32(buf, 10, &enabled);
	if (rc) {
		OPLUS_DSI_ERR("%s cannot be converted to u32", buf);
		return count;
	}

	OPLUS_DSI_INFO("set hbm_max status: %d\n", enabled);
	mutex_lock(&g_oplus_hbm_max_lock);
	oplus_display_panel_set_hbm_max(&enabled);
	mutex_unlock(&g_oplus_hbm_max_lock);
	return count;
}

void oplus_pcp_handle(bool cmd_is_pcp, void *handle)
{
	int last_fps;
	int cur_fps;
	unsigned int mode_idx;
	/* frame_time,diff_time,merged_time(us);cur_time(ns) */
	unsigned int frame_time = 0;
	unsigned int diff_time = 0;
	unsigned int merged_time = 0;
	unsigned long long cur_time = 0;
	struct drm_crtc *crtc;
	struct drm_device *ddev = get_drm_device();
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_crtc_state *state;
	struct drm_display_mode *last_mode;
	struct drm_display_mode *cur_mode;

	crtc = list_first_entry(&(ddev)->mode_config.crtc_list,
				typeof(*crtc), head);
	if (!crtc) {
		OPLUS_DSI_ERR("find crtc fail\n");
		return;
	}

	mtk_crtc = to_mtk_crtc(crtc);
	state = to_mtk_crtc_state(mtk_crtc->base.state);
	mode_idx = state->prop_val[CRTC_PROP_DISP_MODE_IDX];
	last_mode = &(mtk_crtc->avail_modes[g_last_mode_idx]);
	cur_mode = &(mtk_crtc->avail_modes[mode_idx]);
	last_fps = drm_mode_vrefresh(last_mode);
	cur_fps = drm_mode_vrefresh(cur_mode);
	frame_time = 1000000 / cur_fps;

	if (cmd_is_pcp) {
		OPLUS_DSI_TRACE_BEGIN("M_LOCK_PCP");
		atomic_inc(&oplus_pcp_num);
		OPLUS_DSI_INFO("oplus_pcp_lock, oplus_pcp_num = %d\n", atomic_read(&oplus_pcp_num));
		mutex_lock(&oplus_pcp_lock);
		if (handle) {
			cur_time = ktime_get();
			diff_time = (cur_time - oplus_last_te_time) / 1000;
			if (mtk_crtc->panel_ext && mtk_crtc->panel_ext->params
				&& mtk_crtc->panel_ext->params->merge_trig_offset != 0) {
				merged_time = frame_time - (mtk_crtc->panel_ext->params->merge_trig_offset / 26) - 100;
			}
			OPLUS_DSI_INFO("handle is'nt null, diff_time = %d, merged_time = %d\n", diff_time, merged_time);
			if (diff_time > merged_time) {
				usleep_range(1200, 1300);
			}
		} else {
			if (g_is_mode_switch_pack) {
				cur_time = ktime_get();
				diff_time = (cur_time - oplus_last_te_time) / 1000;
				OPLUS_DSI_INFO("handle is null, last_fps = %d, hskew = %d, diff_time = %d\n",
					last_fps, last_mode->hskew, diff_time);
				if (last_fps == 60) {
					if (diff_time < 10500) {
						usleep_range(10500 - diff_time, 10600 - diff_time);
					}
				} else if (last_fps == 120 && last_mode->hskew != 2) {
					if (diff_time > 1300 && diff_time < 3300) {
						usleep_range(3300 - diff_time, 3400 - diff_time);
					} else if (diff_time > 9300 && diff_time < 11600) {
						usleep_range(11600 - diff_time, 11700 - diff_time);
					}
				} else if (last_fps == 90) {
					if (diff_time > 6500 && diff_time < 8500) {
						usleep_range(8500 - diff_time, 8600 - diff_time);
					}
				}
			} else if (cur_fps == 60) {
				cur_time = ktime_get();
				diff_time = (cur_time - oplus_last_te_time) / 1000;
				if (diff_time < 10500) {
					usleep_range(10500 - diff_time, 10600 - diff_time);
				}
				OPLUS_DSI_INFO("handle is null, diff_time = %d\n", diff_time);
			} else {
				OPLUS_DSI_ERR("handle is null\n");
			}
		}
	} else {
		DDPINFO("isn't panel cmdq package\n");
	}
}
EXPORT_SYMBOL(oplus_pcp_handle);

int oplus_pcp_lock_clear(void)
{
	wake_up_interruptible(&oplus_pcp_lock_clear_wq);
	return 0;
}
EXPORT_SYMBOL(oplus_pcp_lock_clear);

int oplus_pcp_lock_wait_clear_thread(void *data)
{
	while(1) {
		wait_event_interruptible(
			oplus_pcp_lock_clear_wq
			, atomic_read(&oplus_pcp_handle_lock));

		usleep_range(10400, 10500);

		DDPINFO("atommic ++ %d\n", atomic_read(&oplus_pcp_handle_lock));
		atomic_dec(&oplus_pcp_handle_lock);
		DDPINFO("atommic -- %d\n", atomic_read(&oplus_pcp_handle_lock));
		mutex_unlock(&oplus_pcp_lock);
		atomic_dec(&oplus_pcp_num);
		OPLUS_DSI_TRACE_END("M_LOCK_PCP");
		OPLUS_DSI_INFO("oplus_pcp_unlock\n");
	}
}
EXPORT_SYMBOL(oplus_pcp_lock_wait_clear_thread);

int oplus_display_set_shutdown_flag(void *buf)
{
	shutdown_flag = 1;
	pr_err("debug for %s, buf = [%s], shutdown_flag = %d\n",
			__func__, buf, shutdown_flag);

	return 0;
}

int oplus_display_panel_set_mipi_err_check(void *data)
{
	int rc = 0;
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_drm_private *private;
	struct drm_device *ddev = get_drm_device();

	if (!(oplus_display0_params->mipi_err_config.config & OPLUS_REGS_CHECK_ENABLE)) {
		OPLUS_DSI_WARN("mipi err check is disable, return\n");
		return -EFAULT;
	}

	crtc = list_first_entry(&(ddev)->mode_config.crtc_list,
				typeof(*crtc), head);
	if (!crtc) {
		OPLUS_DSI_ERR("invalid crtc\n");
		return -EINVAL;
	}
	mtk_crtc = to_mtk_crtc(crtc);
	if (mtk_crtc == NULL || crtc->state == NULL) {
		OPLUS_DSI_ERR("mtk_crtc or crtc->state is null\n");
		return -EINVAL;
	}
	private = crtc->dev->dev_private;
	DDP_COMMIT_LOCK(&private->commit.lock, __func__, __LINE__);
	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);
	if (!mtk_crtc->enabled) {
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		DDP_COMMIT_UNLOCK(&private->commit.lock, __func__, __LINE__);
		OPLUS_DSI_ERR("mtk_crtc->enabled is false\n");
		return -EINVAL;
	} else if (mtk_crtc->ddp_mode == DDP_NO_USE) {
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		DDP_COMMIT_UNLOCK(&private->commit.lock, __func__, __LINE__);
		OPLUS_DSI_ERR("mtk_crtc->ddp_mode is DDP_NO_USE\n");
		return -EINVAL;
	}
	mtk_drm_set_idlemgr(crtc, 0, 0);

	rc = oplus_panel_mipi_err_check(oplus_display0_params);
	OPLUS_DSI_INFO("mipi err check result: %d\n", rc);

	mtk_drm_set_idlemgr(crtc, 1, 0);
	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
	DDP_COMMIT_UNLOCK(&private->commit.lock, __func__, __LINE__);

	return rc;
}

int oplus_display_panel_get_mipi_err_check(void *data)
{
	int rc = 0;
	u32 *check_result = data;

	if (oplus_display0_params && (oplus_display0_params->mipi_err_config.config & OPLUS_REGS_CHECK_ENABLE)) {
		OPLUS_DSI_INFO("mipi err check is enable\n");
		*check_result = 1;
	} else {
		OPLUS_DSI_INFO("mipi err check is disable\n");
		*check_result = 0;
	}

	return rc;
}

int oplus_display_panel_set_esd_status(void *data)
{
	int rc = 0;
	u32 *esd_status = data;
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct drm_device *ddev = get_drm_device();

	/* this debug cmd only for crtc0 */
	crtc = list_first_entry(&(ddev)->mode_config.crtc_list,
				typeof(*crtc), head);
	if (!crtc) {
		OPLUS_DSI_ERR("invalid crtc\n");
		return -EINVAL;
	}
	mtk_crtc = to_mtk_crtc(crtc);
	if (!mtk_crtc) {
		OPLUS_DSI_ERR("invalid mtk_crtc\n");
		return -EINVAL;
	}
	if (!mtk_crtc->esd_ctx) {
		OPLUS_DSI_ERR("invalid esd_ctx\n");
		return -EINVAL;
	}

	mtk_crtc->esd_ctx->chk_en = (bool) *esd_status;
	OPLUS_DSI_INFO("set esd status: %d\n", *esd_status);

	return rc;
}

int oplus_display_panel_get_esd_status(void *data)
{
	int rc = 0;
	u32 *esd_status = data;
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct drm_device *ddev = get_drm_device();

	/* this debug cmd only for crtc0 */
	crtc = list_first_entry(&(ddev)->mode_config.crtc_list,
				typeof(*crtc), head);
	if (!crtc) {
		OPLUS_DSI_ERR("invalid crtc\n");
		return -EINVAL;
	}
	mtk_crtc = to_mtk_crtc(crtc);
	if (!mtk_crtc) {
		OPLUS_DSI_ERR("invalid mtk_crtc\n");
		return -EINVAL;
	}
	if (!mtk_crtc->esd_ctx) {
		OPLUS_DSI_ERR("invalid esd_ctx\n");
		return -EINVAL;
	}

	*esd_status = mtk_crtc->esd_ctx->chk_en;
	OPLUS_DSI_INFO("get esd status: %d\n", *esd_status);

	return rc;
}

int oplus_display_panel_set_crc_check(void *data)
{
	int rc = 0;
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_drm_private *private;
	struct drm_device *ddev = get_drm_device();

	if (!(oplus_display0_params->crc_config.config & OPLUS_REGS_CHECK_ENABLE)) {
		OPLUS_DSI_WARN("crc check is disable, return\n");
		return -EFAULT;
	}

	crtc = list_first_entry(&(ddev)->mode_config.crtc_list,
				typeof(*crtc), head);
	if (!crtc) {
		OPLUS_DSI_ERR("invalid crtc\n");
		return -EINVAL;
	}
	mtk_crtc = to_mtk_crtc(crtc);
	if (mtk_crtc == NULL || crtc->state == NULL) {
		OPLUS_DSI_ERR("mtk_crtc or crtc->state is null\n");
		return -EINVAL;
	}
	private = crtc->dev->dev_private;
	DDP_COMMIT_LOCK(&private->commit.lock, __func__, __LINE__);
	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);
	if (!mtk_crtc->enabled) {
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		DDP_COMMIT_UNLOCK(&private->commit.lock, __func__, __LINE__);
		OPLUS_DSI_ERR("mtk_crtc->enabled is false\n");
		return -EINVAL;
	} else if (mtk_crtc->ddp_mode == DDP_NO_USE) {
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		DDP_COMMIT_UNLOCK(&private->commit.lock, __func__, __LINE__);
		OPLUS_DSI_ERR("mtk_crtc->ddp_mode is DDP_NO_USE\n");
		return -EINVAL;
	}
	mtk_drm_set_idlemgr(crtc, 0, 0);

	rc = oplus_panel_crc_check(oplus_display0_params);
	OPLUS_DSI_INFO("crc check result: %d\n", rc);

	mtk_drm_set_idlemgr(crtc, 1, 0);
	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
	DDP_COMMIT_UNLOCK(&private->commit.lock, __func__, __LINE__);

	return rc;
}

int oplus_display_panel_get_crc_check(void *data)
{
	int rc = 0;
	u32 *check_result = data;

	if (oplus_display0_params && (oplus_display0_params->crc_config.config & OPLUS_REGS_CHECK_ENABLE)) {
		OPLUS_DSI_INFO("crc check is enable\n");
		*check_result = 1;
	} else {
		OPLUS_DSI_INFO("crc check is disable\n");
		*check_result = 0;
	}

	return rc;
}
