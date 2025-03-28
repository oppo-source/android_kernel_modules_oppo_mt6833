/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_display_sysfs_attrs.c
** Description : oplus display sysfs attrs
** Version : 1.0
** Date : 2024/05/20
** Author : Display
******************************************************************/
#include <linux/notifier.h>
#include <linux/kobject.h>
#include <linux/signal.h>
#include <linux/leds-mtk.h>
#include <soc/oplus/system/oplus_project.h>

#include "mtk_drm_drv.h"
#include "mtk_dsi.h"
#include "mtk_drm_mmp.h"
#include "mtk_disp_aal.h"

#include "oplus_display_sysfs_attrs.h"
#include "oplus_display_utils.h"
#include "oplus_display_debug.h"
#include "oplus_display_power.h"
#include "oplus_display_device_ioctl.h"
#include "oplus_display_device.h"
#include "oplus_dsi_display_config.h"
#ifdef OPLUS_FEATURE_DISPLAY_ADFR
#include "oplus_adfr.h"
#endif /* OPLUS_FEATURE_DISPLAY_ADFR */
#ifdef OPLUS_FEATURE_DISPLAY_TEMP_COMPENSATION
#include "oplus_display_temp_compensation.h"
#endif /* OPLUS_FEATURE_DISPLAY_TEMP_COMPENSATION */
#ifdef OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT
#include "oplus_display_onscreenfingerprint.h"
#endif /* OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT */
#ifdef OPLUS_FEATURE_DISPLAY_HPWM
#include "oplus_display_pwm.h"
#endif /* OPLUS_FEATURE_DISPLAY_HPWM */
#ifdef OPLUS_TRACKPOINT_REPORT
#include "oplus_display_trackpoint_report.h"
#define EXCEPTION_TRACKPOINT_REPORT(fmt, ...)	\
	do { \
			pr_err(fmt, ##__VA_ARGS__); \
			display_exception_trackpoint_report(fmt, ##__VA_ARGS__); \
		} while (0)
#endif /* OPLUS_TRACKPOINT_REPORT */

/*
 * we will create a sysfs which called /sys/kernel/oplus_display,
 * In that directory, oplus display private api can be called
 */

#define PANEL_TX_MAX_BUF 256

#define OPLUS_ATTR(_name, _mode, _show, _store) \
struct kobj_attribute oplus_attr_##_name = __ATTR(_name, _mode, _show, _store)

DEFINE_MUTEX(g_oplus_sn_lock);

unsigned long serial_number = 0x0;
EXPORT_SYMBOL(serial_number);
int cmd_id_debug;
/***  oplus serial number (sn) dtsi config   *****************
oplus,dsi-serial-number-enabled=<1>;
oplus,dsi-serial-number-switch-page;
oplus,dsi-serial-number-index= <0>;
oplus,dsi-serial-number-switch-page-command= <0xFF 0x08 0x38 0x1D>;
oplus,dsi-serial-number-reg= <0x82>;
oplus,dsi-serial-number-default-switch-page-command= <0xFF 0x08 0x38 0x00>;
oplus,dsi-serial-number-read-count= <7>;
********************************************************/

/* oplus serial number initialize params ***************/
struct oplus_serial_number_params {
	unsigned int   config;									/* bit(0):enable oplus serial number, bit(1) and other spare no used */
	bool   page_config;										/* sn read before need switch page config */
	unsigned int   index;									/* sn read reg index, after reading the register, the starting position of conversion is required */
	unsigned int   *switch_page_command;					/* the switch page parsed from dtsi, if (oplus,dsi-serial-number-switch-page) config */
	unsigned char  switch_page_command_count;				/* the count of switch page parsed(sizeof(switch_page)+1) */
	unsigned int   *default_page_command;					/* the default page parsed from dtsi, panel default page, if (oplus,dsi-serial-number-switch-page) config */
	unsigned char  default_page_command_count;				/* the count of default page parsed(sizeof(default_page)+1) */
	unsigned int   read_reg;								/* the read reg parsed from dtsi */
	unsigned int   read_count;								/* the count of read reg from dtsi */
	unsigned int   year_offset;								/* the year offset of read reg from dtsi */
};

static struct oplus_serial_number_params g_oplus_serial_number_params = {0};
static struct oplus_serial_number_params *oplus_serial_number_get_params(void)
{
	return &g_oplus_serial_number_params;
}

struct aod_area oplus_aod_area[RAMLESS_AOD_AREA_NUM];
char send_cmd[RAMLESS_AOD_PAYLOAD_SIZE];

typedef struct panel_serial_info
{
	int reg_index;
	uint64_t year;
	uint64_t month;
	uint64_t day;
	uint64_t hour;
	uint64_t minute;
	uint64_t second;
	uint64_t reserved[2];
} PANEL_SERIAL_INFO;

int trig_db_enable = 0;
EXPORT_SYMBOL(trig_db_enable);
extern struct drm_device *get_drm_device(void);
extern int mtk_drm_setbacklight(struct drm_crtc *crtc, unsigned int level, unsigned int panel_ext_param, unsigned int cfg_flag, unsigned int lock);
#ifdef OPLUS_FEATURE_DISPLAY_APOLLO
extern int mtk_drm_setbacklight_without_lock(struct drm_crtc *crtc, unsigned int level, unsigned int panel_ext_param, unsigned int cfg_flag);
extern void apollo_set_brightness_for_show(unsigned int level);
#endif /* OPLUS_FEATURE_DISPLAY_APOLLO */
extern int oplus_mtk_drm_setseed(struct drm_crtc *crtc, unsigned int seed_mode);
extern PANEL_VOLTAGE_BAK panel_vol_bak[PANEL_VOLTAGE_ID_MAX];
unsigned int esd_mode = 0;
EXPORT_SYMBOL(esd_mode);
unsigned int seed_mode = 0;
extern int mtk_crtc_osc_freq_switch(struct drm_crtc *crtc, unsigned int en, unsigned int userdata);
unsigned long osc_mode = 0;
EXPORT_SYMBOL(osc_mode);

unsigned int oplus_display_brightness = 0;

bool pq_trigger = true;
bool atomic_set_bl_en = false;
unsigned int backup_bl_level;
EXPORT_SYMBOL(pq_trigger);
EXPORT_SYMBOL(atomic_set_bl_en);
EXPORT_SYMBOL(backup_bl_level);

unsigned int oplus_max_normal_brightness = 0;
unsigned int oplus_max_brightness = 0;
unsigned int oplus_enhance_mipi_strength = 0;

char lcm_version[32] = "\0";
char lcm_manufacture[32] = "\0";
EXPORT_SYMBOL(lcm_version);
EXPORT_SYMBOL(lcm_manufacture);

bool oplus_hbm_max_en;
EXPORT_SYMBOL(oplus_hbm_max_en);

unsigned long long last_rdma_start_time = 0;
EXPORT_SYMBOL(last_rdma_start_time);
int mutex_sof_ns = 0;
EXPORT_SYMBOL(mutex_sof_ns);
unsigned long long oplus_last_te_time = 0;
EXPORT_SYMBOL(oplus_last_te_time);
unsigned int lcm_id1 = 0;
EXPORT_SYMBOL(lcm_id1);
unsigned int lcm_id2 = 0;
EXPORT_SYMBOL(lcm_id2);
int silence_flag = 0;
EXPORT_SYMBOL(silence_flag);
int shutdown_flag = 0;
EXPORT_SYMBOL(shutdown_flag);

extern int oplus_mtk_drm_setcabc(struct drm_crtc *crtc, unsigned int hbm_mode);
extern ssize_t oplus_get_hbm_max_debug(struct kobject *obj,
	struct kobj_attribute *attr, char *buf);
extern ssize_t oplus_set_hbm_max_debug(struct kobject *obj,
		struct kobj_attribute *attr,
		const char *buf, size_t count);

static BLOCKING_NOTIFIER_HEAD(lcdinfo_notifiers);
int register_lcdinfo_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&lcdinfo_notifiers, nb);
}
int unregister_lcdinfo_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&lcdinfo_notifiers, nb);
}

EXPORT_SYMBOL(register_lcdinfo_notifier);
EXPORT_SYMBOL(unregister_lcdinfo_notifier);

void lcdinfo_notify(unsigned long val, void *v)
{
	blocking_notifier_call_chain(&lcdinfo_notifiers, val, v);
}
EXPORT_SYMBOL(lcdinfo_notify);

static ssize_t oplus_display_get_brightness(struct kobject *obj,
		struct kobj_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%d\n", oplus_display_brightness);
}

static ssize_t oplus_display_set_brightness(struct kobject *obj,
		struct kobj_attribute *attr, const char *buf, size_t num)
{
	int ret;
	struct drm_crtc *crtc;
	struct drm_device *ddev = get_drm_device();
	unsigned int oplus_set_brightness = 0;

	ret = kstrtouint(buf, 10, &oplus_set_brightness);

	OPLUS_DSI_INFO("set brightness: %d\n", oplus_set_brightness);

	if (oplus_set_brightness > oplus_max_brightness) {
		OPLUS_DSI_ERR("brightness:%d out of scope\n", oplus_set_brightness);
		return num;
	}

	/* this debug cmd only for crtc0 */
	crtc = list_first_entry(&(ddev)->mode_config.crtc_list,
				typeof(*crtc), head);
	if (!crtc) {
		OPLUS_DSI_ERR("find crtc fail\n");
		return 0;
	}
	mtk_drm_setbacklight(crtc, oplus_set_brightness, 0, 0x1 << SET_BACKLIGHT_LEVEL, 1);

	return num;
}

static ssize_t oplus_display_get_max_brightness(struct kobject *obj,
		struct kobj_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%u\n", oplus_max_brightness);
}

static ssize_t oplus_display_get_maxbrightness(struct kobject *obj,
		struct kobj_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%u\n", oplus_max_normal_brightness);
}

static ssize_t oplus_display_set_panel_pwr(struct kobject *obj,
		struct kobj_attribute *attr,
		const char *buf, size_t num)
{
	u32 panel_vol_value = 0, panel_vol_id = 0;
	int rc = 0;
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct drm_device *ddev = get_drm_device();
	/* this debug cmd only for crtc0 */
	crtc = list_first_entry(&(ddev)->mode_config.crtc_list,
		typeof(*crtc), head);
	if (!crtc) {
		OPLUS_DSI_ERR("get hbm find crtc fail\n");
		return 0;
	}
	mtk_crtc = to_mtk_crtc(crtc);

	sscanf(buf, "%d %d", &panel_vol_id, &panel_vol_value);
	panel_vol_id = panel_vol_id & 0x0F;

	OPLUS_DSI_INFO("start\n");

	if (panel_vol_id < 0 || panel_vol_id >= PANEL_VOLTAGE_ID_MAX) {
		return -EINVAL;
	}

	if (panel_vol_value < panel_vol_bak[panel_vol_id].voltage_min ||
		panel_vol_id > panel_vol_bak[panel_vol_id].voltage_max) {
		return -EINVAL;
	}

	if (panel_vol_id == PANEL_VOLTAGE_ID_VG_BASE) {
		OPLUS_DSI_INFO("set the VGH_L pwr=%d \n", panel_vol_value);
		rc = oplus_panel_set_vg_base(panel_vol_value);
		if (rc < 0) {
			return rc;
		}

		return num;
	}

	if (mtk_crtc->panel_ext->funcs->oplus_set_power) {
		rc = mtk_crtc->panel_ext->funcs->oplus_set_power(panel_vol_id, panel_vol_value);
		if (rc) {
			OPLUS_DSI_ERR("Set voltage fail, rc=%d\n", rc);
			return -EINVAL;
		}
	}
	OPLUS_DSI_INFO("end\n");

	return num;
}

static ssize_t oplus_display_get_panel_pwr(struct kobject *obj,
	struct kobj_attribute *attr, char *buf) {
	int ret = 0;
	u32 i = 0;
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct drm_device *ddev = get_drm_device();
	/* this debug cmd only for crtc0 */
	crtc = list_first_entry(&(ddev)->mode_config.crtc_list,
		typeof(*crtc), head);
	if (!crtc) {
		OPLUS_DSI_ERR("get hbm find crtc fail\n");
		return 0;
	}
	mtk_crtc = to_mtk_crtc(crtc);

	for (i = 0; i < (PANEL_VOLTAGE_ID_MAX-1); i++) {
		if (mtk_crtc->panel_ext->funcs->oplus_update_power_value) {
			ret = mtk_crtc->panel_ext->funcs->oplus_update_power_value(panel_vol_bak[i].voltage_id);
		}

		if (ret < 0) {
			OPLUS_DSI_ERR("update_current_voltage error, ret=%d\n", ret);
		}
		else {
			panel_vol_bak[i].voltage_current = ret;
		}
	}

	return sysfs_emit(buf, "%d %d %d %d %d %d %d %d %d %d %d %d\n",
		panel_vol_bak[0].voltage_id, panel_vol_bak[0].voltage_min,
		panel_vol_bak[0].voltage_current, panel_vol_bak[0].voltage_max,
		panel_vol_bak[1].voltage_id, panel_vol_bak[1].voltage_min,
		panel_vol_bak[1].voltage_current, panel_vol_bak[1].voltage_max,
		panel_vol_bak[2].voltage_id, panel_vol_bak[2].voltage_min,
		panel_vol_bak[2].voltage_current, panel_vol_bak[2].voltage_max);
}

static ssize_t oplus_display_get_dsi_log_switch(struct kobject *obj,
		struct kobj_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "\
		dynamic conctrl debug log, 0x0 --> disable all debug log\n \
		1:enable, 0:disable\n \
		BIT(0) --> dcs log\n \
		BIT(1) --> dsi log\n \
		BIT(2) --> ofp log\n \
		BIT(3) --> adfr log\n \
		BIT(4) --> temp compensation log\n \
		BIT(5) --> pwm log\n \
		current value:0x%x\n", oplus_display_log_type);
}

static ssize_t oplus_display_set_dsi_log_switch(struct kobject *obj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	char *buf_temp = (char*)kmalloc(count-1, GFP_KERNEL);

	memcpy(buf_temp, buf, count-1);
	sscanf(buf, "%x", &oplus_display_log_type);
	OPLUS_DSI_INFO("oplus_display_log_type=0x%x , count=%lu, buf=[%s]\n",
			oplus_display_log_type, count, buf_temp);
	if (buf_temp) {
		kfree(buf_temp);
	}

	return count;
}

static ssize_t oplus_display_get_trace_enable_attr(struct kobject *obj,
	struct kobj_attribute *attr, char *buf)
{
	if (!buf) {
		OPLUS_DSI_ERR("Invalid params\n");
		return -EINVAL;
	}

	return sysfs_emit(buf, "dynamic trace enable\n \
		0x0 --> disable all trace\n \
		BIT(0) --> enable dsi trace\n \
		BIT(1) --> enable ofp trace\n \
		BIT(2) --> enable adfr trace\n \
		BIT(3) --> enable temp compensation trace\n \
		current value:0x%x\n", oplus_display_trace_enable);
}

static ssize_t oplus_display_set_trace_enable_attr(struct kobject *obj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	if (!buf) {
		OPLUS_DSI_ERR("Invalid params\n");
		return count;
	}

	sscanf(buf, "%x", &oplus_display_trace_enable);
	OPLUS_DSI_INFO("buf=[%s], oplus_display_trace_enable=0x%X , count=%lu\n",
			buf, oplus_display_trace_enable, count);

	return count;
}

static ssize_t oplus_display_get_disp_trig_db(struct kobject *obj,
		struct kobj_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%d\n", trig_db_enable);
}

static ssize_t oplus_display_set_disp_trig_db(struct kobject *obj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	sscanf(buf, "%d", &trig_db_enable);
	OPLUS_DSI_INFO("trig_db_enable=%d\n", trig_db_enable);

	if (get_eng_version() == AGING) {
		OPLUS_DSI_INFO("disp_trig_db, AGING version\n");
	} else {
		OPLUS_DSI_INFO("disp_trig_db, normal version\n");
	}

	return count;
}

int oplus_serial_number_probe(struct device_node *node)
{
	int i = 0;
	int rc = 0;
	int length = 0;
	unsigned int cnt = 0;
	char buf[OPLUS_DSI_CMD_PRINT_BUF_SIZE] = {0};
	u32 config = 0;
	u32 sn_index = 0;
	u32 sn_reg = 0;
	u32 sn_read_count = 0;
	int sn_year_offset = 0;
	struct oplus_serial_number_params *serial_number_params = oplus_serial_number_get_params();

	rc = of_property_read_u32(node, "oplus,dsi-serial-number-enabled", &config);
	if (rc == 0) {
		serial_number_params->config = config;
		OPLUS_DSI_INFO("config=%d\n", serial_number_params->config);
	} else {
		serial_number_params->config = 0;
		OPLUS_DSI_WARN("Parsing failed, default config=%d\n", serial_number_params->config);
	}

	if(serial_number_params->config == 0) {
		OPLUS_DSI_WARN("this panel not support serial number\n");
		goto error;
	}

	if (of_property_read_bool(node, "oplus,dsi-serial-number-switch-page")) {
		serial_number_params->page_config = true;
		length = of_property_count_elems_of_size(node,
				"oplus,dsi-serial-number-switch-page-command", sizeof(unsigned int));
		OPLUS_DSI_INFO("oplus,dsi-serial-number-switch-page-command length=%d\n", length);

		serial_number_params->switch_page_command = kzalloc(length * sizeof(unsigned int), GFP_KERNEL);
		if (!serial_number_params->switch_page_command) {
			TEMP_COMPENSATION_ERR("failed to kzalloc switch_page_command\n");
			rc = -ENOMEM;
			goto error_free_switch_page;
		}
		rc = of_property_read_u32_array(node,
				"oplus,dsi-serial-number-switch-page-command",
				serial_number_params->switch_page_command,
				length);
		if (rc == 0) {
			cnt = 0;
			memset(buf, 0, sizeof(buf));
			for (i = 0; i < length; i++) {
				cnt += snprintf(buf + cnt, 4, "%02X ",
						serial_number_params->switch_page_command[i]);
			}
			OPLUS_DSI_INFO("oplus,dsi-serial-number-switch-page-command: %s\n", buf);
		}
		serial_number_params->switch_page_command_count = length;

		length = of_property_count_elems_of_size(node,
				"oplus,dsi-serial-number-default-switch-page-command",
				sizeof(unsigned int));
		OPLUS_DSI_INFO("oplus,dsi-serial-number-default-switch-page-command length=%d\n", length);
		serial_number_params->default_page_command = kzalloc(length * sizeof(unsigned int), GFP_KERNEL);
		if (!serial_number_params->default_page_command) {
			TEMP_COMPENSATION_ERR("failed to kzalloc default_page_command\n");
			rc = -ENOMEM;
			goto error_free_default_page;
		}
		rc = of_property_read_u32_array(node,
				"oplus,dsi-serial-number-default-switch-page-command",
				serial_number_params->default_page_command,
				length);
		if (rc == 0) {
			cnt = 0;
			memset(buf, 0, sizeof(buf));
			for (i = 0; i < length; i++) {
				cnt += snprintf(buf + cnt, 4, "%02X ",
						serial_number_params->default_page_command[i]);
			}
			OPLUS_DSI_INFO("oplus,dsi-serial-number-default-switch-page-command: %s\n", buf);
		}
		serial_number_params->default_page_command_count = length;
	} else {
		serial_number_params->page_config = false;
	}

	rc = of_property_read_u32(node, "oplus,dsi-serial-number-index", &sn_index);
	if (rc == 0) {
		serial_number_params->index = sn_index;
		OPLUS_DSI_INFO("oplus,dsi-serial-number-index=%d\n", sn_index);
	} else {
		serial_number_params->index = 0;
		OPLUS_DSI_WARN("Parsing failed, default index=%d\n",
				serial_number_params->index);
	}

	rc = of_property_read_u32(node, "oplus,dsi-serial-number-reg", &sn_reg);
	if (rc == 0) {
		serial_number_params->read_reg = sn_reg;
		OPLUS_DSI_INFO("oplus,dsi-serial-number-reg=0x%02X\n", sn_reg);
	} else {
		serial_number_params->read_reg = 0;
		OPLUS_DSI_WARN("Parsing failed, default read_reg=0x%02X\n",
				serial_number_params->read_reg);
	}

	rc = of_property_read_u32(node, "oplus,dsi-serial-number-read-count",
			&sn_read_count);
	if (rc == 0) {
		serial_number_params->read_count = sn_read_count;
		OPLUS_DSI_INFO("oplus,dsi-serial-number-read-count=%d\n", sn_read_count);
	} else {
		serial_number_params->read_count = 0;
		OPLUS_DSI_WARN("Parsing failed, default read_count=%d\n",
				serial_number_params->read_count);
	}

	rc = of_property_read_u32(node, "oplus,dsi-serial-number-year-offset",
			&sn_year_offset);
	if (rc == 0) {
		serial_number_params->year_offset = sn_year_offset;
		OPLUS_DSI_INFO("oplus,dsi-serial-number-year-offset=%d\n", sn_year_offset);
	} else {
		serial_number_params->year_offset = 0;
		OPLUS_DSI_WARN("Parsing failed, default year_offset=%d\n",
				serial_number_params->year_offset);
	}

	OPLUS_DSI_INFO("Successful end\n");
	return 0;

error_free_default_page:
	kfree(serial_number_params->default_page_command);
	serial_number_params->default_page_command = NULL;

error_free_switch_page:
	kfree(serial_number_params->switch_page_command);
	serial_number_params->switch_page_command = NULL;

error:
	serial_number_params->config = 0;
	OPLUS_DSI_WARN("config=0x%X\n", serial_number_params->config);

	return rc;
}

int oplus_dsi_display_serial_number_deinit(void *ctx_dev)
{
	int ret = 0;
	struct oplus_serial_number_params *serial_number_params = oplus_serial_number_get_params();

	OPLUS_KFREE(serial_number_params->switch_page_command);
	OPLUS_KFREE(serial_number_params->default_page_command);

	return ret;
}

int oplus_display_read_panel_id(struct drm_crtc *crtc)
{
	struct mtk_ddp_comp *comp;
	struct mtk_drm_crtc *mtk_crtc;
	char para[20] = {0};

	mtk_crtc = to_mtk_crtc(crtc);
	if (!mtk_crtc) {
		OPLUS_DSI_ERR("cannot get crtc\n");
		return -EINVAL;
	}
	comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (!(mtk_crtc->enabled)) {
		OPLUS_DSI_ERR("crtc not enable\n");
		return -EINVAL;
	}

	if (!comp || !comp->funcs || !comp->funcs->io_cmd) {
		OPLUS_DSI_ERR("cannot find output component\n");
		return -EINVAL;
	}
	if (!mtk_drm_lcm_is_connect(mtk_crtc)) {
		OPLUS_DSI_ERR("lcm is not connect\n");
		return -EINVAL;
	}

	oplus_ddic_dsi_read_cmd(0xDA, 5, para);
	m_da = para[0] & 0xFF;

	oplus_ddic_dsi_read_cmd(0xDB, 5, para);
	m_db = para[0] & 0xFF;

	oplus_ddic_dsi_read_cmd(0xDC, 5, para);
	m_dc = para[0] & 0xFF;

	OPLUS_DSI_INFO("0xDA=0x%02X, 0xDB=0x%02X, 0xDC=0x%02X\n", m_da, m_db, m_dc);

#ifdef OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT
	if (oplus_ofp_is_supported()) {
		oplus_ofp_fp_type_compatible_mode_config();
	}
#endif /* OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT */

	return 0;
}

int oplus_panel_serial_number_read(struct drm_crtc *crtc)
{
	char para[20] = {0};
	int count = 5;
	int i = 0;
	char switch_page_reg[20] = {0};
	PANEL_SERIAL_INFO panel_serial_info;
	struct mtk_drm_crtc *mtk_crtc;
	struct oplus_serial_number_params *serial_number_params = oplus_serial_number_get_params();

	mtk_crtc = to_mtk_crtc(crtc);
	if (!mtk_crtc) {
		OPLUS_DSI_ERR("cannot get crtc\n");
		return -EINVAL;
	}

	if (!(mtk_crtc->enabled)) {
		OPLUS_DSI_ERR("Sleep State set backlight stop --crtc not ebable\n");
		return -EINVAL;
	}

	if (!mtk_drm_lcm_is_connect(mtk_crtc)) {
		OPLUS_DSI_ERR("lcm is not connect\n");
		return -EINVAL;
	}

	if(serial_number_params->config == 0) {
		OPLUS_DSI_WARN("not support\n");
		return 0;
	}

	mutex_lock(&g_oplus_sn_lock);
	while ((count--) > 0) {
		if (serial_number_params->page_config) {
			for (i = 0; i < serial_number_params->switch_page_command_count; i++) {
				switch_page_reg[i] = (char)serial_number_params->switch_page_command[i];
			}
			oplus_ddic_dsi_send_cmd(serial_number_params->switch_page_command_count, switch_page_reg);
		}

		oplus_ddic_dsi_read_cmd(serial_number_params->read_reg, serial_number_params->read_count, para);

		panel_serial_info.reg_index = serial_number_params->index;
		panel_serial_info.year      = ((para[panel_serial_info.reg_index] & 0xF0) >> 4) + serial_number_params->year_offset;
		panel_serial_info.month       = para[panel_serial_info.reg_index] & 0x0F;
		panel_serial_info.day         = para[panel_serial_info.reg_index + 1] & 0x3F;
		panel_serial_info.hour        = para[panel_serial_info.reg_index + 2] & 0x3F;
		panel_serial_info.minute      = para[panel_serial_info.reg_index + 3] & 0x3F;
		panel_serial_info.second      = para[panel_serial_info.reg_index + 4] & 0x3F;
		panel_serial_info.reserved[0] = para[panel_serial_info.reg_index + 5];
		panel_serial_info.reserved[1] = para[panel_serial_info.reg_index + 6];

		serial_number = (panel_serial_info.year  << 56) \
				+ (panel_serial_info.month       << 48) \
				+ (panel_serial_info.day         << 40) \
				+ (panel_serial_info.hour        << 32) \
				+ (panel_serial_info.minute      << 24) \
				+ (panel_serial_info.second      << 16) \
				+ (panel_serial_info.reserved[0] << 8) \
				+ (panel_serial_info.reserved[1]);

		OPLUS_DSI_INFO("year=0x%02X, month=0x%02X, day=0x%02X, hour=0x%02X, minute=0x%02X, second=0x%02X, msecond=0x%02X,0x%02X\n",
				panel_serial_info.year,
				panel_serial_info.month,
				panel_serial_info.day,
				panel_serial_info.hour,
				panel_serial_info.minute,
				panel_serial_info.second,
				panel_serial_info.reserved[0],
				panel_serial_info.reserved[1]);

		if (serial_number_params->page_config) {
			for (i = 0; i < serial_number_params->switch_page_command_count; i++) {
				switch_page_reg[i] = (char)serial_number_params->default_page_command[i];
			}
			oplus_ddic_dsi_send_cmd(serial_number_params->default_page_command_count, switch_page_reg);
		}

		if (serial_number == 0) {
			continue;
		} else {
			break;
		}
	}
	mutex_unlock(&g_oplus_sn_lock);

	OPLUS_DSI_INFO("Read panel serial number: %lX\n", serial_number);

	return 0;
}

static ssize_t oplus_get_panel_serial_number(struct kobject *obj,
		struct kobj_attribute *attr, char *buf)
{
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
		OPLUS_DSI_INFO("backlight is 0, skip get serial number!\n");
		return 0;
	}

	if (serial_number == 0) {
		oplus_panel_serial_number_read(crtc);
		OPLUS_DSI_INFO("Read serial_number=0x%lX, DA=0x%02X, DB=0x%02X, DC=0x%02X\n",
				serial_number, m_da, m_db, m_dc);
	}

	return scnprintf(buf, PAGE_SIZE, "Get panel serial number: %lX\n", serial_number);
}

static ssize_t panel_serial_store(struct kobject *obj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	OPLUS_DSI_INFO("[soso] Lcm read 0xA1 reg=0x%016lX\n", serial_number);

	return count;
}

static ssize_t oplus_set_panel_reg(struct kobject *obj,
		struct kobj_attribute *attr, const char *buf,
		size_t count)
{
	u32 value = 0;
	u32 step = 0;
	int len = 0;
	int i = 0;
	char *bufp = (char *)buf;
	char para[PANEL_TX_MAX_BUF] = {0};
	struct drm_crtc *crtc;
	struct drm_device *ddev = get_drm_device();
	struct mtk_ddp_comp *comp;
	struct mtk_drm_crtc *mtk_crtc;
	char read;

	crtc = list_first_entry(&(ddev)->mode_config.crtc_list,
		typeof(*crtc), head);
	if (!crtc) {
		OPLUS_DSI_ERR("get panel reg find crtc fail\n");
		return 0;
	}

	mtk_crtc = to_mtk_crtc(crtc);
	comp = mtk_ddp_comp_request_output(mtk_crtc);

	if (!(mtk_crtc->enabled)) {
		OPLUS_DSI_ERR("Sleep State get panel reg stop --crtc not ebable\n");
		return 0;
	}

	if (!comp || !comp->funcs || !comp->funcs->io_cmd) {
		OPLUS_DSI_ERR("cannot find output component\n");
		return 0;
	}

	if (sscanf(bufp, "%c%n", &read, &step) && read == 'r') {
		bufp += step;
		sscanf(bufp, "%x %d", &value, &len);

		if (len > PANEL_TX_MAX_BUF || value > PANEL_TX_MAX_BUF) {
			OPLUS_DSI_ERR("reg or len than the max,stop\n");
			return 0;
		}

		oplus_ddic_dsi_read_cmd(value, len, para);
		for(i = 0; i < len; i++) {
			OPLUS_DSI_INFO("para[%d]=%d\n", i, para[i]);
		}
	}

	return count;
}

static ssize_t oplus_get_panel_reg(struct kobject *obj,
		struct kobj_attribute *attr, char *buf)
{
	OPLUS_DSI_INFO("Lcm read serial_number=%016lX\n", serial_number);

	return scnprintf(buf, PAGE_SIZE, "Get panel serial number: %016lX\n", serial_number);
}

static ssize_t oplus_display_get_pq_trigger(struct kobject *obj,
		struct kobj_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%d\n", pq_trigger);
}

static ssize_t oplus_display_set_pq_trigger(struct kobject *obj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int tmp = 0;
	sscanf(buf, "%d", &tmp);
	pq_trigger = !!tmp;
	OPLUS_DSI_INFO("pq_trigger=%d\n", pq_trigger);

	return count;
}
int oplus_dc_threshold = 260;
int oplus_panel_alpha = 0;
int oplus_underbrightness_alpha = 0;
int alpha_save = 0;

EXPORT_SYMBOL(oplus_underbrightness_alpha);
EXPORT_SYMBOL(oplus_panel_alpha);


struct ba {
	u32 brightness;
	u32 alpha;
};
struct ba brightness_seed_alpha_lut_dc[] = {
	{0, 0xff},
	{1, 0xfc},
	{2, 0xfb},
	{3, 0xfa},
	{4, 0xf9},
	{5, 0xf8},
	{6, 0xf7},
	{8, 0xf6},
	{10, 0xf4},
	{15, 0xf0},
	{20, 0xea},
	{30, 0xe0},
	{45, 0xd0},
	{70, 0xbc},
	{100, 0x98},
	{120, 0x80},
	{140, 0x70},
	{160, 0x58},
	{180, 0x48},
	{200, 0x30},
	{220, 0x20},
	{240, 0x10},
	{260, 0x00},
};
struct ba brightness_alpha_lut[] = {
	{0, 0xFF},
	{5, 0xEE},
	{7, 0xED},
	{10, 0xE7},
	{20, 0xE3},
	{35, 0xDC},
	{60, 0xD1},
	{90, 0xCE},
	{150, 0xC1},
	{280, 0xAA},
	{460, 0x95},
	{650, 0x7F},
	{850, 0x79},
	{1000, 0x6E},
	{1150, 0x62},
	{1300, 0x52},
	{1500, 0x4C},
	{1700, 0x42},
	{1900, 0x35},
	{2047, 0x24},
};

struct ba brightness_alpha_lut_BOE[] = {
	{0, 0xFF},
	{12, 0xEE},
	{20, 0xE8},
	{35, 0xE5},
	{65, 0xDA},
	{100, 0xD8},
	{150, 0xCD},
	{210, 0xC5},
	{320, 0xB9},
	{450, 0xA9},
	{630, 0xA0},
	{870, 0x94},
	{1150, 0x86},
	{1500, 0x7B},
	{1850, 0x6B},
	{2250, 0x66},
	{2650, 0x55},
	{3050, 0x47},
	{3400, 0x39},
	{3515, 0x24},
};

struct ba brightness_alpha_lut_index[] = {
	{0, 0xFF},
	{150, 0xEE},
	{190, 0xEB},
	{230, 0xE6},
	{270, 0xE1},
	{310, 0xDA},
	{350, 0xD7},
	{400, 0xD3},
	{450, 0xD0},
	{500, 0xCD},
	{600, 0xC3},
	{700, 0xB8},
	{900, 0xA3},
	{1100, 0x90},
	{1300, 0x7B},
	{1400, 0x70},
	{1500, 0x65},
	{1700, 0x4E},
	{1900, 0x38},
	{2047, 0x23},
};

static int interpolate(int x, int xa, int xb, int ya, int yb)
{
	int bf, factor, plus;
	int sub = 0;

	bf = 2 * (yb - ya) * (x - xa) / (xb - xa);
	factor = bf / 2;
	plus = bf % 2;
	if ((xa - xb) && (yb - ya))
		sub = 2 * (x - xa) * (x - xb) / (yb - ya) / (xa - xb);

	return ya + factor + plus + sub;
}

int bl_to_alpha(int brightness)
{
	int alpha;
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;

	int i = 0;
	int level = 0;
	struct drm_device *ddev = get_drm_device();

	/* this debug cmd only for crtc0 */
	crtc = list_first_entry(&(ddev)->mode_config.crtc_list,
				typeof(*crtc), head);
	if (!crtc) {
		OPLUS_DSI_ERR(KERN_ERR "find crtc fail\n");
		return 0;
	}

	mtk_crtc = to_mtk_crtc(crtc);
	if (!mtk_crtc || !mtk_crtc->panel_ext || !mtk_crtc->panel_ext->params) {
		OPLUS_DSI_ERR("falied to get lcd proc info\n");
		return 0;
	}

/*	if(!strcmp(mtk_crtc->panel_ext->params->manufacture, "boe_nt37800_2048")) {
		level = ARRAY_SIZE(brightness_alpha_lut_BOE);
		for (i = 0; i < ARRAY_SIZE(brightness_alpha_lut_BOE); i++) {
			if (brightness_alpha_lut_BOE[i].brightness >= brightness)
				break;
		}

		if (i == 0)
			alpha = brightness_alpha_lut_BOE[0].alpha;
		else if (i == level)
			alpha = brightness_alpha_lut_BOE[level - 1].alpha;
		else
			alpha = interpolate(brightness,
				brightness_alpha_lut_BOE[i-1].brightness,
				brightness_alpha_lut_BOE[i].brightness,
				brightness_alpha_lut_BOE[i-1].alpha,
				brightness_alpha_lut_BOE[i].alpha);
	}
	else {*/
		level = ARRAY_SIZE(brightness_alpha_lut);
		for (i = 0; i < ARRAY_SIZE(brightness_alpha_lut); i++) {
			if (brightness_alpha_lut[i].brightness >= brightness)
				break;
		}

		if (i == 0)
			alpha = brightness_alpha_lut[0].alpha;
		else if (i == level)
			alpha = brightness_alpha_lut[level - 1].alpha;
		else
			alpha = interpolate(brightness,
				brightness_alpha_lut[i-1].brightness,
				brightness_alpha_lut[i].brightness,
				brightness_alpha_lut[i-1].alpha,
				brightness_alpha_lut[i].alpha);
/*	} */

	return alpha;
}

int brightness_to_alpha(int brightness)
{
	int alpha;

	if (brightness <= 3)
		return alpha_save;

	alpha = bl_to_alpha(brightness);

	alpha_save = alpha;

	return alpha;
}

int oplus_seed_bright_to_alpha(int brightness)
{
	int level = ARRAY_SIZE(brightness_seed_alpha_lut_dc);
	int i = 0;
	int alpha;

	for (i = 0; i < ARRAY_SIZE(brightness_seed_alpha_lut_dc); i++) {
		if (brightness_seed_alpha_lut_dc[i].brightness >= brightness)
			break;
	}

	if (i == 0)
		alpha = brightness_seed_alpha_lut_dc[0].alpha;
	else if (i == level)
		alpha = brightness_seed_alpha_lut_dc[level - 1].alpha;
	else
		alpha = interpolate(brightness,
			brightness_seed_alpha_lut_dc[i-1].brightness,
			brightness_seed_alpha_lut_dc[i].brightness,
			brightness_seed_alpha_lut_dc[i-1].alpha,
			brightness_seed_alpha_lut_dc[i].alpha);

	return alpha;
}

int oplus_get_panel_brightness_to_alpha(void)
{
	if (oplus_panel_alpha)
		return oplus_panel_alpha;

	return brightness_to_alpha(oplus_display_brightness);
}
EXPORT_SYMBOL(oplus_get_panel_brightness_to_alpha);

static ssize_t oplus_display_get_dim_alpha(struct kobject *obj,
		struct kobj_attribute *attr, char *buf)
{
#ifdef OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT
	if (!oplus_ofp_get_hbm_state()) {
		return sysfs_emit(buf, "%d\n", 0);
	}
#endif /* OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT */

	oplus_underbrightness_alpha = oplus_get_panel_brightness_to_alpha();

	return sysfs_emit(buf, "%d\n", oplus_underbrightness_alpha);
}


static ssize_t oplus_display_set_dim_alpha(struct kobject *obj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	sscanf(buf, "%x", &oplus_panel_alpha);
	return count;
}

unsigned int silence_mode = 0;
EXPORT_SYMBOL(silence_mode);

static ssize_t silence_show(struct kobject *obj,
			struct kobj_attribute *attr, char *buf)
{
	OPLUS_DSI_INFO("silence_mode=%d\n", silence_mode);

	return sysfs_emit(buf, "%d\n", silence_mode);
}

static ssize_t silence_store(struct kobject *obj,
		struct kobj_attribute *attr, const char *buf, size_t num)
{
	int ret;
	msleep(1000);
	ret = kstrtouint(buf, 10, &silence_mode);
	OPLUS_DSI_INFO("silence_mode=%d\n", silence_mode);

	return num;
}

static ssize_t oplus_display_get_ESD(struct kobject *obj,
		struct kobj_attribute *attr, char *buf)
{
	OPLUS_DSI_INFO("esd=%d\n", esd_mode);

	return sysfs_emit(buf, "%d\n", esd_mode);
}

static ssize_t oplus_display_set_ESD(struct kobject *obj,
		struct kobj_attribute *attr, const char *buf, size_t num)
{
	int ret = 0;

	ret = kstrtouint(buf, 10, &esd_mode);
	OPLUS_DSI_INFO("esd mode is %d\n", esd_mode);

	return num;
}

unsigned int cabc_mode = 1;
unsigned int cabc_true_mode = 1;
unsigned int cabc_sun_flag = 0;
unsigned int cabc_back_flag = 1;
extern void disp_aal_set_dre_en(struct mtk_ddp_comp *comp, int enable);

enum{
	CABC_LEVEL_0,
	CABC_LEVEL_1,
	CABC_LEVEL_2 = 3,
	CABC_EXIT_SPECIAL = 8,
	CABC_ENTER_SPECIAL = 9,
};

static ssize_t oplus_display_get_CABC(struct kobject *obj,
		struct kobj_attribute *attr, char *buf)
{
	OPLUS_DSI_INFO("CABC_mode=%d\n", cabc_true_mode);

	return sysfs_emit(buf, "%d\n", cabc_true_mode);
}

static ssize_t oplus_display_set_CABC(struct kobject *obj,
		struct kobj_attribute *attr, const char *buf, size_t num)
{
	int ret = 0;
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_ddp_comp *comp;
	struct drm_device *ddev = get_drm_device();
	/* this debug cmd only for crtc0 */
	crtc = list_first_entry(&(ddev)->mode_config.crtc_list,
				typeof(*crtc), head);
	if (!crtc) {
		OPLUS_DSI_ERR("find crtc fail\n");
		return 0;
	}

	mtk_crtc = to_mtk_crtc(crtc);

	comp = mtk_ddp_comp_sel_in_cur_crtc_path(mtk_crtc, MTK_DISP_AAL, 0);
	if (!comp) {
		OPLUS_DSI_ERR("comp is null!\n");
		return 0;
	}

	ret = kstrtouint(buf, 10, &cabc_mode);
	cabc_true_mode = cabc_mode;
	OPLUS_DSI_INFO("cabc mode is %d, cabc_back_flag is %d\n", cabc_mode, cabc_back_flag);
	if(cabc_mode < 4)
		cabc_back_flag = cabc_mode;

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
		return num;
	}

	OPLUS_DSI_INFO("cabc mode is %d\n", cabc_true_mode);

	if ((cabc_true_mode == CABC_LEVEL_0 && cabc_back_flag == CABC_LEVEL_0) || mtk_crtc->panel_ext->params->oplus_display_global_dre) {
		disp_aal_set_dre_en(comp, 1);
		OPLUS_DSI_INFO("enable dre\n");
	} else {
		disp_aal_set_dre_en(comp, 0);
		OPLUS_DSI_INFO("disable dre\n");
	}
	oplus_mtk_drm_setcabc(crtc, cabc_true_mode);
	if (cabc_true_mode != cabc_back_flag) cabc_true_mode = cabc_back_flag;

	return num;
}

static ssize_t oplus_display_get_seed(struct kobject *obj,
		struct kobj_attribute *attr, char *buf)
{
	OPLUS_DSI_INFO("seed_mode=%d\n", seed_mode);

	return sysfs_emit(buf, "%d\n", seed_mode);
}

static ssize_t oplus_display_set_seed(struct kobject *obj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	struct drm_crtc *crtc;
	unsigned int temp_save = 0;
	int ret = 0;
	struct drm_device *ddev = get_drm_device();

	ret = kstrtouint(buf, 10, &temp_save);
	OPLUS_DSI_INFO("seed=%d\n", temp_save);
	/* this debug cmd only for crtc0 */
	crtc = list_first_entry(&(ddev)->mode_config.crtc_list,
				typeof(*crtc), head);
	if (!crtc) {
		OPLUS_DSI_ERR("find crtc fail\n");
		return 0;
	}

	oplus_mtk_drm_setseed(crtc, temp_save);
	seed_mode = temp_save;

	return count;
}

static ssize_t oplus_display_get_osc(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	OPLUS_DSI_INFO("osc_mode=%lu\n", osc_mode);

	return sysfs_emit(buf, "%lu\n", osc_mode);
}

static ssize_t oplus_display_set_osc(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	struct drm_crtc *crtc;
	unsigned int temp_save = 0;
	int ret = 0;
	struct drm_device *ddev = get_drm_device();

	ret = kstrtouint(buf, 10, &temp_save);
	OPLUS_DSI_INFO("osc mode=%d\n", temp_save);

	crtc = list_first_entry(&(ddev)->mode_config.crtc_list,
				typeof(*crtc), head);
	if (!crtc) {
		OPLUS_DSI_ERR("find crtc fail\n");
		return 0;
	}

	if(0)
		mtk_crtc_osc_freq_switch(crtc, temp_save, 0);
	osc_mode = temp_save;

	return count;
}

static ssize_t oplus_display_get_color_status(struct kobject *obj,
				struct kobj_attribute *attr, char *buf)
{
	int ret = 0;
	ssize_t result = 0;
	struct softiris_color *data = kzalloc(sizeof(struct softiris_color), GFP_KERNEL);

	ret = oplus_display_get_softiris_color_status((void*)data);
	OPLUS_DSI_INFO("ret=%d\n", ret);

	result = sysfs_emit(buf, "vivid:%s, srgb:%s,softiris:%s, dual_panel:%s, dual_brightness:%s, color_oplus_calibrate_status:%s\n",
			data->color_vivid_status ? "true" : "false",
			data->color_srgb_status ? "true" : "false",
			data->color_softiris_status ? "true" : "false",
			data->color_dual_panel_status ? "true" : "false",
			data->color_dual_brightness_status ? "true" : "false",
			data->color_oplus_calibrate_status ? "true" : "false");
	kfree(data);

	return result;
}
int oplus_dc_alpha = 0;
EXPORT_SYMBOL(oplus_dc_alpha);
extern int oplus_dc_enable;
static ssize_t oplus_display_get_dc_enable(struct kobject *obj,
				struct kobj_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%d\n", oplus_dc_enable);
}

static ssize_t oplus_display_set_dc_enable(struct kobject *obj,
				struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	sscanf(buf, "%x", &oplus_dc_enable);
	return count;
}

static ssize_t oplus_display_get_dim_dc_alpha(struct kobject *obj,
				struct kobj_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%d\n", oplus_dc_alpha);
}

static ssize_t oplus_display_set_dim_dc_alpha(struct kobject *dev,
				struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	sscanf(buf, "%x", &oplus_dc_alpha);
	return count;
}

static char oplus_rx_reg[OPLUS_MIPI_RX_MAX_LEN] = {0x0};
static char oplus_rx_len = 0;
static ssize_t oplus_display_get_panel_reg(struct kobject *obj,
		struct kobj_attribute *attr, char *buf)
{
	int i, cnt = 0;

	for (i = 0; i < oplus_rx_len; i++)
		cnt += snprintf(buf + cnt, OPLUS_DSI_CMD_PRINT_BUF_SIZE - cnt,
				"%02X ", oplus_rx_reg[i]);

	cnt += snprintf(buf + cnt, OPLUS_DSI_CMD_PRINT_BUF_SIZE - cnt, "\n");

	return cnt;
}

static ssize_t oplus_display_set_panel_reg(struct kobject *obj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char reg[OPLUS_MIPI_RX_MAX_LEN] = {0x0};
	u32 value = 0, step = 0;
	int len = 0;
	char *bufp = (char *)buf;
	char read;

	/*begin read*/
	if (sscanf(bufp, "%c%n", &read, &step) && read == 'r') {
		bufp += step;
		sscanf(bufp, "%x %d", &value, &len);

		if (len > OPLUS_MIPI_RX_MAX_LEN) {
			OPLUS_DSI_ERR("failed\n");
			return -EINVAL;
		}

		oplus_ddic_dsi_read_cmd(value, len, reg);

		memcpy(oplus_rx_reg, reg, OPLUS_MIPI_RX_MAX_LEN);
		oplus_rx_len = len;

		return count;
	}
	/*end read*/

	while (sscanf(bufp, "%x%n", &value, &step) > 0) {
		OPLUS_DSI_DEBUG_DCS("cmd reg[%d]=0x%02X\n", len, value);
		reg[len++] = value;
		if (len >= OPLUS_MIPI_RX_MAX_LEN) {
			OPLUS_DSI_ERR("wrong input reg len\n");
			return -EFAULT;
		}

		bufp += step;
	}
	oplus_ddic_dsi_send_cmd(len, reg);

	return count;
}

static ssize_t oplus_dsi_panel_get_command(struct kobject *obj,
		struct kobj_attribute *attr, char *buf)
{
	int i = 0, cnt = 0, ret = -1;
	struct drm_crtc *crtc;
	struct drm_device *ddev = get_drm_device();
	struct mtk_ddp_comp *comp;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_dsi *dsi;

	/* this debug cmd only for crtc0 */
	crtc = list_first_entry(&(ddev)->mode_config.crtc_list,
		typeof(*crtc), head);
	if (!crtc) {
		OPLUS_DSI_ERR("find crtc fail\n");
		return ret;
	}

	mtk_crtc = to_mtk_crtc(crtc);
	if (!mtk_crtc) {
		OPLUS_DSI_ERR("cannot get crtc\n");
		return ret;
	}
	comp = mtk_ddp_comp_request_output(mtk_crtc);

	if (!comp || !comp->funcs || !comp->funcs->io_cmd) {
		OPLUS_DSI_ERR("cannot find output component\n");
		return ret;
	}

	if (!mtk_drm_lcm_is_connect(mtk_crtc)) {
		OPLUS_DSI_ERR("lcm is not connect\n");
		return ret;
	}

	cnt = sysfs_emit(buf,
			"read current dsi_cmd:\n"
			"    echo dump > dsi_cmd  - then you can find dsi cmd on kmsg\n"
			"send dsi_cmd:\n"
			"    echo qcom,mdss-dsi-hbm-on-command > dsi_cmd\n"
			"    echo send > dsi_cmd\n"
			"set sence dsi cmd:\n"
			"  example hbm on:\n"
			"    echo qcom,mdss-dsi-hbm-on-command > dsi_cmd\n"
			"    echo [dsi cmd0] > dsi_cmd\n"
			"    echo [dsi cmd1] > dsi_cmd\n"
			"    echo [dsi cmdX] > dsi_cmd\n"
			"    echo flush > dsi_cmd\n"
			"available dsi_cmd sences:\n");
	for (i = 0; i < DSI_CMD_ID_MAX; i++)
		cnt += snprintf(buf + cnt, PAGE_SIZE - cnt,
				"    dsi_cmd[%d] = %s\n", i, dsi_cmd_map[i]);

	dsi = container_of(comp, struct mtk_dsi, ddp_comp);

	if ((cmd_id_debug < DSI_CMD_ID_MAX) && (cmd_id_debug > 0)) {
		cnt += snprintf(buf + cnt, PAGE_SIZE - cnt, "Recently: mode[%d]-dsi_cmd[%d] = %s\n",
				dsi_panel_mode_id, cmd_id_debug, dsi_cmd_map[cmd_id_debug]);
	}
	ret = cnt;

	return ret;
}

static int oplus_dsi_panel_command_id(char *lcm_cmd_name)
{
	int i = 0;

	if (!strcmp("lcm_cmd_map", lcm_cmd_name)) {
		OPLUS_DSI_INFO("cmd_id=%d", DSI_CMD_ID_MAX);
		cmd_id_debug = DSI_CMD_ID_MAX;
		return DSI_CMD_ID_MAX;
	}
	for (; i < DSI_CMD_ID_MAX; i ++) {
		if (!strcmp(dsi_cmd_map[i], lcm_cmd_name)) {
			OPLUS_DSI_INFO("cmd_id=%d", i);
			cmd_id_debug = i;
			return i;
		}
	}

	return -1;
}

static int oplus_dsi_panel_cmd_store_sub(struct dsi_cmd_sets *lcm_cmd_set_debug_table, struct dsi_cmd_sets *table)
{
	int i = 0;
	char buf[OPLUS_DSI_CMD_PRINT_BUF_SIZE] = {0};

	if (table->para_table != NULL) {
		kfree(table->para_table);
		table->para_table = NULL;
	}
	memcpy(table, lcm_cmd_set_debug_table, sizeof(struct dsi_cmd_sets));

	OPLUS_DSI_INFO("dsi_cmd[%d]: %s, cmd_lines=%d\n",
			table->cmd_id, dsi_cmd_map[table->cmd_id], table->cmd_lines);
	for (i = 0; i < table->cmd_lines; i++) {
		memset(buf, 0, sizeof(buf));
		oplus_dsi_panel_cmd_line_joint(&table->para_table[i], buf);
		OPLUS_DSI_INFO("para_table[%d]: %s\n", i, buf);
	}

	return 0;
}

static int oplus_dsi_panel_cmd_store(int cmd_id, char *cmd_bufs, int cmd_count, int cmd_lines)
{
	int i = 0, j = 0, k = 0, ret = -1;
	unsigned char *lcm_cmd_params = NULL;
	struct dsi_cmd_sets lcm_cmd_set_debug_table = {};

	OPLUS_DSI_DEBUG_DCS("dsi_cmd[%d]: %s, cmd_lines=%d\n", cmd_id, dsi_cmd_map[cmd_id], cmd_lines);
	if (cmd_count < 8) {
		return ret;
	}
	lcm_cmd_set_debug_table.cmd_lines = cmd_lines;
	lcm_cmd_set_debug_table.state = lcm_all_cmd_table[cmd_id_debug].state;
	lcm_cmd_set_debug_table.para_table = kzalloc(sizeof(struct dsi_cmd_table) * cmd_lines, GFP_KERNEL);

	lcm_cmd_params = kzalloc(cmd_count, GFP_KERNEL);
	memcpy(lcm_cmd_params, cmd_bufs, cmd_count);
	for (; i < cmd_count; j++) {
		lcm_cmd_set_debug_table.para_table[j].cmd_msg.type = lcm_cmd_params[i++];
		lcm_cmd_set_debug_table.para_table[j].cmd_msg.cmd1 = lcm_cmd_params[i++];
		lcm_cmd_set_debug_table.para_table[j].cmd_msg.channel = lcm_cmd_params[i++];
		lcm_cmd_set_debug_table.para_table[j].cmd_msg.flags = lcm_cmd_params[i++];
		lcm_cmd_set_debug_table.para_table[j].post_wait_ms = lcm_cmd_set_debug_table.para_table[j].cmd_msg.cmd4 = lcm_cmd_params[i++];
		lcm_cmd_set_debug_table.para_table[j].count = ((lcm_cmd_params[i] << 8) | (lcm_cmd_params[i+1]));
		i += 2;
		if ((lcm_cmd_set_debug_table.para_table[j].count > 0) && (lcm_cmd_set_debug_table.para_table[j].count <= OPLUS_SEND_CMD_MAX)) {
			memcpy(lcm_cmd_set_debug_table.para_table[j].para_list, lcm_cmd_params + i,
			lcm_cmd_set_debug_table.para_table[j].count);
		} else {
			goto error;
		}
		i += lcm_cmd_set_debug_table.para_table[j].count;
	}
	ret = oplus_dsi_panel_cmd_store_sub(&lcm_cmd_set_debug_table, &lcm_all_cmd_table[cmd_id_debug]);
	if (ret) {
		goto error;
	}
	goto out;
error:
	if (lcm_cmd_set_debug_table.para_table != NULL) {
		kfree(lcm_cmd_set_debug_table.para_table);
		lcm_cmd_set_debug_table.para_table = NULL;
	}
	lcm_cmd_set_debug_table.cmd_lines = 0;
out:
	if (lcm_cmd_params != NULL) {
		kfree(lcm_cmd_params);
		lcm_cmd_params = NULL;
	}

	return ret;
}

static int oplus_dsi_panel_dump_command(void)
{
	int i;

	for (i = 0; i < DSI_CMD_ID_MAX; i++) {
		if (!dsi_cmd_map[i] || ((!lcm_all_cmd_table[i].cmd_lines) && (i != DSI_CMD_DEBUG))) {
			continue;
		}
		OPLUS_DSI_INFO("%s%s: dsi mode[%d]-dsi_cmd[%d], state=%d\n",
				DISPLAY_TOOL_CMD_KEYWORD, dsi_cmd_map[i],
				dsi_panel_mode_id, i, lcm_all_cmd_table[i].state);
		oplus_dsi_panel_print_cmd_lines(&lcm_all_cmd_table[i], DISPLAY_TOOL_CMD_KEYWORD);
	}

	return 0;
}

static int oplus_dsi_panel_send_cmd_debug(void)
{
	int ret = -1;
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct drm_device *ddev = get_drm_device();
	struct mtk_ddp_comp *comp;
	struct mtk_dsi *dsi;
	struct mipi_dsi_device *dsi_device;
	struct dsi_panel_lcm *ctx;
	struct cmdq_pkt *cmdq_handle;
	int index = 0;

	/* this debug cmd only for crtc0 */
	crtc = list_first_entry(&(ddev)->mode_config.crtc_list,
		typeof(*crtc), head);
	if (!crtc) {
		OPLUS_DSI_ERR("find crtc fail\n");
		return ret;
	}

	index = drm_crtc_index(crtc);
	CRTC_MMP_EVENT_START(index, ddic_send_cmd, (unsigned long)crtc,
				TRUE);

	mtk_crtc = to_mtk_crtc(crtc);
	if (mtk_crtc == NULL || crtc->state == NULL) {
		OPLUS_DSI_ERR("mtk_crtc or crtc->state is NULL\n");
		return -EINVAL;
	}
	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);
	if (!(mtk_crtc->enabled)) {
		OPLUS_DSI_ERR("crtc%d disable skip\n",
			drm_crtc_index(&mtk_crtc->base));
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		CRTC_MMP_EVENT_END(index, ddic_send_cmd, 0, 1);
		return -EINVAL;
	} else if (mtk_crtc->ddp_mode == DDP_NO_USE) {
		OPLUS_DSI_ERR("ddp_mode: NO_USE\n");
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		CRTC_MMP_EVENT_END(index, ddic_send_cmd, 0, 2);
		return -EINVAL;
	}

	comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (unlikely(!comp)) {
		OPLUS_DSI_ERR("invalid output comp\n");
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		CRTC_MMP_EVENT_END(index, ddic_send_cmd, 0, 3);
		return -EINVAL;
	}
	dsi = container_of(comp, struct mtk_dsi, ddp_comp);
	if (!dsi) {
		OPLUS_DSI_ERR("no dsi\n");
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		CRTC_MMP_EVENT_END(index, ddic_send_cmd, 0, 2);
		return -EINVAL;
	}
	ctx = container_of(dsi->panel, struct dsi_panel_lcm, panel);
	if (!ctx) {
		OPLUS_DSI_ERR("no ctx\n");
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		CRTC_MMP_EVENT_END(index, ddic_send_cmd, 0, 2);
		return -EINVAL;
	}
	dsi_device = to_mipi_dsi_device(ctx->dev);
	if (!dsi_device) {
		OPLUS_DSI_ERR("no dsi_device\n");
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		CRTC_MMP_EVENT_END(index, ddic_send_cmd, 0, 2);
		return -EINVAL;
	}

	OPLUS_DSI_INFO("debug send cmd_id=%d, state=%d\n", cmd_id_debug, lcm_all_cmd_table[cmd_id_debug].state);
	mtk_drm_send_lcm_cmd_prepare(crtc, &cmdq_handle);
	if (lcm_all_cmd_table[cmd_id_debug].state) {
		oplus_dsi_panel_send_cmd(dsi, cmd_id_debug, cmdq_handle, DSI_CMD_FUNC_DEFAULT);
	} else {
		oplus_dsi_panel_send_cmd(dsi_device, cmd_id_debug, NULL, DSI_CMD_FUNC_DEFAULT);
	}
	mtk_drm_send_lcm_cmd_flush(crtc, &cmdq_handle, 0);
	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
	CRTC_MMP_EVENT_END(index, ddic_send_cmd, (unsigned long)crtc,
			TRUE);

	return ret;
}

static ssize_t oplus_dsi_panel_set_command(struct kobject *obj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char data[count];
	char *bufp = (char *)buf;
	int ret = -1;
	int cmd_id;
	int buf_size = OPLUS_DSI_CMD_DEBUG_BUF_SIZE;
	static bool flush_end = false;
	bool send = false;
	static bool flush_start = false;
	static char *cmd_bufs;
	static int cmd_counts;
	static int cmd_lines;

	OPLUS_DSI_DEBUG_DCS("input buf=[%s]\n", bufp);
	memset(data, 0, sizeof(data));
	if (strlen(buf) > sizeof(data)) {
		OPLUS_DSI_ERR("input buffer size[%lu] is out of range[%d]\n",
				strlen(buf), sizeof(data));
		return ret;
	}

	memcpy(data, buf, count);
	data[sizeof(data)-1]='\0';

	if (flush_start && !cmd_bufs) {
		cmd_bufs = kzalloc(buf_size, GFP_KERNEL);
		if (!cmd_bufs) {
			return ret;
		}
	}

	if (!strcmp("dump", data)) {
		ret = oplus_dsi_panel_dump_command();

		if (ret < 0) {
			return ret;
		}

		return count;
	} else if (!strcmp("flush", data)) {
		OPLUS_DSI_INFO("flush_end\n");
		flush_end = true;
	} else if (!strcmp("send", data)) {
		OPLUS_DSI_INFO("send\n");
		send = true;
	} else if (!strcmp("dsi_hs_mode", data)) {
		OPLUS_DSI_INFO("dsi_cmd[%d]: %s set to dsi_hs_mode\n", cmd_id_debug, dsi_cmd_map[cmd_id_debug]);
		lcm_all_cmd_table[cmd_id_debug].state = DSI_CMD_SET_STATE_HS;
		return count;
	} else if (!strcmp("dsi_lp_mode", data)) {
		OPLUS_DSI_INFO("dsi_cmd[%d]: %s set to dsi_lp_mode\n", cmd_id_debug, dsi_cmd_map[cmd_id_debug]);
		lcm_all_cmd_table[cmd_id_debug].state = DSI_CMD_SET_STATE_LP;
		return count;
	} else if (!strcmp("dsi_ls_mode", data)) {
		OPLUS_DSI_INFO("dsi_cmd[%d]: %s set to dsi_ls_mode\n", cmd_id_debug, dsi_cmd_map[cmd_id_debug]);
		lcm_all_cmd_table[cmd_id_debug].state = DSI_CMD_SET_STATE_LP_GCE;
		return count;
	} else {
		cmd_id = oplus_dsi_panel_command_id(data);
		if ((cmd_id < 0) || (cmd_id >= DSI_CMD_ID_MAX)) {
			if (data[0] < '0' || data[0] > '9' || !flush_start) {
				OPLUS_DSI_ERR("flush not start or cmd not support, return\n");
				return ret;
			}
		} else {
			OPLUS_DSI_INFO("flush_start dsi_cmd[%d]: %s\n", cmd_id, dsi_cmd_map[cmd_id]);
			flush_start = true;
			return ret;
		}
	}

	if (send) {
		oplus_dsi_panel_send_cmd_debug();
		return count;
	}

	if (flush_start && !flush_end) {
		u32 value = 0, step = 0;

		while (sscanf(bufp, "%x%n", &value, &step) > 0) {
			if (value > 0xFF) {
				OPLUS_DSI_ERR("input reg don't large than 0xFF\n");
				return ret;
			}

			cmd_bufs[cmd_counts++] = value;

			if (cmd_counts >= buf_size) {
				OPLUS_DSI_ERR("wrong input reg len\n");
				cmd_counts = 0;
				return ret;
			}

			bufp += step;
		}
		cmd_lines++;
	} else {
		if (cmd_lines > 0) {
			ret = oplus_dsi_panel_cmd_store(cmd_id_debug, cmd_bufs, cmd_counts, cmd_lines);
			if (ret < 0)
				OPLUS_DSI_ERR("cmd store fail\n");
		}
		if (cmd_bufs != NULL) {
			kfree(cmd_bufs);
			cmd_bufs = NULL;
		}
		flush_start = false;
		flush_end = false;
		cmd_counts = 0;
		cmd_lines = 0;
	}
	OPLUS_DSI_DEBUG_DCS("flush_start=%d, flush_end=%d, dsi_cmd[%d][%d]: %s\n",
			flush_start, flush_end, cmd_id, cmd_lines, data);
	ret = count;

	return ret;
}

#ifdef OPLUS_TRACKPOINT_REPORT
static ssize_t oplus_set_trackpoint_test_attr(struct kobject *obj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	char data[8];

	if (strlen(buf) >= 8) {
		pr_err("[LCM_DEBUG] input buffer size[%lu] is out of range[%d]\n",
				strlen(buf), 8);
		return -1;
	}

	strlcpy(data, buf, 8);
	data[strlen(data)-1]='\0';

	if (!strcmp("501", data))
		EXCEPTION_TRACKPOINT_REPORT("DisplayDriverID@@501$$trackpoint_test: ovl_probe error");
	else if (!strcmp("502", data))
		EXCEPTION_TRACKPOINT_REPORT("DisplayDriverID@@502$$trackpoint_test: underflow cnt");
	else if (!strcmp("503", data))
		EXCEPTION_TRACKPOINT_REPORT("DisplayDriverID@@503$$trackpoint_test: invalid");
	else if (!strcmp("504", data))
		EXCEPTION_TRACKPOINT_REPORT("DisplayDriverID@@504$$trackpoint_test: mtk_ddp_probe failed to request irq");
	else if (!strcmp("505", data))
		EXCEPTION_TRACKPOINT_REPORT("DisplayDriverID@@505$$trackpoint_test: mtk_fb_wait error");
	else if (!strcmp("506", data))
		EXCEPTION_TRACKPOINT_REPORT("DisplayDriverID@@506$$trackpoint_test: underrun");
	else if (!strcmp("507", data))
		EXCEPTION_TRACKPOINT_REPORT("DisplayDriverID@@507$$trackpoint_test: ESD check failed");
	else if (!strcmp("508", data))
		EXCEPTION_TRACKPOINT_REPORT("DisplayDriverID@@508$$trackpoint_test: cmdq timeout Begin of Error");
	else if (!strcmp("509", data))
		EXCEPTION_TRACKPOINT_REPORT("DisplayDriverID@@509$$trackpoint_test: cmdq sec session init failed");
	else if (!strcmp("510", data))
		EXCEPTION_TRACKPOINT_REPORT("DisplayDriverID@@510$$trackpoint_test: disp_aal_config_mdp_aal");
	else if (!strcmp("511", data))
		EXCEPTION_TRACKPOINT_REPORT("DisplayDriverID@@511$$trackpoint_test: TE timeout");
	else
		EXCEPTION_TRACKPOINT_REPORT("DisplayDriverID@@599$$trackpoint_test:%s", buf);
	return count;
}
#endif /* OPLUS_TRACKPOINT_REPORT */

static ssize_t oplus_get_shutdownflag(struct kobject *obj,
		struct kobj_attribute *attr, char *buf)
{
	printk(KERN_INFO "get shutdown_flag = %d\n", shutdown_flag);
	return sprintf(buf, "%d\n", shutdown_flag);
}

static ssize_t oplus_set_shutdownflag(struct kobject *obj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int flag = 0;
	sscanf(buf, "%du", &flag);
	if (1 == flag) {
		shutdown_flag = 1;
	}
	pr_err("shutdown_flag = %d\n", shutdown_flag);
	return count;
}

static struct kobject *oplus_display_kobj;
static OPLUS_ATTR(oplus_brightness, S_IRUGO|S_IWUSR, oplus_display_get_brightness, oplus_display_set_brightness);
static OPLUS_ATTR(oplus_max_brightness, S_IRUGO|S_IWUSR, oplus_display_get_max_brightness, NULL);
static OPLUS_ATTR(oplus_get_color_status, S_IRUGO|S_IWUSR, oplus_display_get_color_status, NULL);
static OPLUS_ATTR(max_brightness, S_IRUGO|S_IWUSR, oplus_display_get_maxbrightness, NULL);
static OPLUS_ATTR(seed, S_IRUGO|S_IWUSR, oplus_display_get_seed, oplus_display_set_seed);
static OPLUS_ATTR(panel_pwr, S_IRUGO|S_IWUSR, oplus_display_get_panel_pwr, oplus_display_set_panel_pwr);
static OPLUS_ATTR(dim_alpha, S_IRUGO|S_IWUSR, oplus_display_get_dim_alpha, oplus_display_set_dim_alpha);
/* static OPLUS_ATTR(dimlayer_bl_en, S_IRUGO|S_IWUSR, oplus_display_get_dc_enable, oplus_display_set_dc_enable); */
/* static OPLUS_ATTR(dim_dc_alpha, S_IRUGO|S_IWUSR, oplus_display_get_dim_dc_alpha, oplus_display_set_dim_dc_alpha); */
static OPLUS_ATTR(panel_serial_number, S_IRUGO|S_IWUSR, oplus_get_panel_serial_number, panel_serial_store);
static OPLUS_ATTR(LCM_CABC, S_IRUGO|S_IWUSR, oplus_display_get_CABC, oplus_display_set_CABC);
static OPLUS_ATTR(esd, S_IRUGO|S_IWUSR, oplus_display_get_ESD, oplus_display_set_ESD);
static OPLUS_ATTR(sau_closebl_node, S_IRUGO|S_IWUSR, silence_show, silence_store);
static OPLUS_ATTR(dsi_log_switch, S_IRUGO | S_IWUSR, oplus_display_get_dsi_log_switch, oplus_display_set_dsi_log_switch);
static OPLUS_ATTR(trace_enable, S_IRUGO | S_IWUSR, oplus_display_get_trace_enable_attr, oplus_display_set_trace_enable_attr);
/* #ifdef OPLUS_FEATURE_DISPLAY_ADFR */
static OPLUS_ATTR(adfr_config, S_IRUGO|S_IWUSR, oplus_adfr_get_config_attr, oplus_adfr_set_config_attr);
static OPLUS_ATTR(test_te, S_IRUGO|S_IWUSR, oplus_adfr_get_test_te_attr, oplus_adfr_set_test_te_attr);
static OPLUS_ATTR(min_fps, S_IRUGO|S_IWUSR, oplus_adfr_get_min_fps_attr, oplus_adfr_set_min_fps_attr);
/* #endif */
#ifdef OPLUS_FEATURE_DISPLAY_TEMP_COMPENSATION
static OPLUS_ATTR(temp_compensation_config, S_IRUGO | S_IWUSR, oplus_temp_compensation_get_config_attr, oplus_temp_compensation_set_config_attr);
static OPLUS_ATTR(ntc_temp, S_IRUGO | S_IWUSR, oplus_temp_compensation_get_ntc_temp_attr, oplus_temp_compensation_set_ntc_temp_attr);
static OPLUS_ATTR(shell_temp, S_IRUGO | S_IWUSR, oplus_temp_compensation_get_shell_temp_attr, oplus_temp_compensation_set_shell_temp_attr);
#endif /* OPLUS_FEATURE_DISPLAY_TEMP_COMPENSATION */
static OPLUS_ATTR(disp_trig_db, S_IRUGO|S_IWUSR, oplus_display_get_disp_trig_db,
		oplus_display_set_disp_trig_db);
static OPLUS_ATTR(write_panel_reg, S_IRUGO|S_IWUSR, oplus_display_get_panel_reg, oplus_display_set_panel_reg);
static OPLUS_ATTR(panel_reg_cmd, S_IRUGO|S_IWUSR, oplus_get_panel_reg, oplus_set_panel_reg);
static OPLUS_ATTR(dimlayer_bl_en, S_IRUGO|S_IWUSR, oplus_display_get_dc_enable, oplus_display_set_dc_enable);
static OPLUS_ATTR(dim_dc_alpha, S_IRUGO|S_IWUSR, oplus_display_get_dim_dc_alpha, oplus_display_set_dim_dc_alpha);
static OPLUS_ATTR(osc, S_IRUGO|S_IWUSR, oplus_display_get_osc, oplus_display_set_osc);
static OPLUS_ATTR(pq_trigger, S_IRUGO|S_IWUSR, oplus_display_get_pq_trigger, oplus_display_set_pq_trigger);
static OPLUS_ATTR(pwm_turbo, S_IRUGO|S_IWUSR, oplus_display_get_high_pwm, oplus_display_set_high_pwm);
static OPLUS_ATTR(hbm_max, S_IRUGO|S_IWUSR, oplus_get_hbm_max_debug, oplus_set_hbm_max_debug);
static OPLUS_ATTR(pwm_onepulse, S_IRUGO|S_IWUSR, oplus_get_pwm_pulse_debug, oplus_set_pwm_pulse_debug);
#ifdef OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT
static OPLUS_ATTR(fp_type, S_IRUGO | S_IWUSR, oplus_ofp_get_fp_type_attr, oplus_ofp_set_fp_type_attr);
static OPLUS_ATTR(dimlayer_hbm, S_IRUGO | S_IWUSR, oplus_ofp_get_dimlayer_hbm_attr, oplus_ofp_set_dimlayer_hbm_attr);
static OPLUS_ATTR(oplus_notify_fppress, S_IRUGO|S_IWUSR, NULL, oplus_ofp_notify_fp_press_attr);
static OPLUS_ATTR(hbm, S_IRUGO|S_IWUSR, oplus_ofp_get_hbm_attr, oplus_ofp_set_hbm_attr);
static OPLUS_ATTR(aod_light_mode_set, S_IRUGO|S_IWUSR, oplus_ofp_get_aod_light_mode_attr, oplus_ofp_set_aod_light_mode_attr);
static OPLUS_ATTR(fake_aod, S_IRUGO | S_IWUSR, oplus_ofp_get_fake_aod_attr, oplus_ofp_set_fake_aod_attr);
static OPLUS_ATTR(ultra_low_power_aod_mode, S_IRUGO | S_IWUSR, oplus_ofp_get_ultra_low_power_aod_mode_attr, oplus_ofp_set_ultra_low_power_aod_mode_attr);
static OPLUS_ATTR(longrui_aod, S_IRUGO | S_IWUSR, oplus_ofp_get_longrui_aod_config_attr, oplus_ofp_set_longrui_aod_mode_attr);
#endif /* OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT */
static OPLUS_ATTR(dsi_cmd, S_IRUGO | S_IWUSR, oplus_dsi_panel_get_command, oplus_dsi_panel_set_command);
#ifdef OPLUS_TRACKPOINT_REPORT
static OPLUS_ATTR(trackpoint_test, S_IWUSR, NULL, oplus_set_trackpoint_test_attr);
#endif /* OPLUS_TRACKPOINT_REPORT */
static OPLUS_ATTR(shutdownflag, S_IRUGO | S_IWUSR, oplus_get_shutdownflag, oplus_set_shutdownflag);

EXPORT_SYMBOL(seed_mode);
EXPORT_SYMBOL(oplus_max_normal_brightness);
EXPORT_SYMBOL(oplus_max_brightness);
EXPORT_SYMBOL(oplus_display_brightness);
EXPORT_SYMBOL(cabc_mode);
EXPORT_SYMBOL(cabc_true_mode);
EXPORT_SYMBOL(cabc_sun_flag);
EXPORT_SYMBOL(cabc_back_flag);
EXPORT_SYMBOL(oplus_enhance_mipi_strength);

/*
 * Create a group of attributes so that we can create and destroy them all
 * at once.
 */
static struct attribute *oplus_display_attrs[] = {
	&oplus_attr_oplus_brightness.attr,
	&oplus_attr_oplus_max_brightness.attr,
	&oplus_attr_oplus_get_color_status.attr,
	&oplus_attr_max_brightness.attr,
	&oplus_attr_seed.attr,
	&oplus_attr_panel_pwr.attr,
	&oplus_attr_dim_alpha.attr,
	/* &oplus_attr_dimlayer_bl_en.attr, */
	/* &oplus_attr_dim_dc_alpha.attr, */
	&oplus_attr_panel_serial_number.attr,
	&oplus_attr_LCM_CABC.attr,
	&oplus_attr_esd.attr,
	&oplus_attr_sau_closebl_node.attr,
	&oplus_attr_dsi_log_switch.attr,
	&oplus_attr_trace_enable.attr,
/* #ifdef OPLUS_FEATURE_DISPLAY_ADFR */
	&oplus_attr_adfr_config.attr,
	&oplus_attr_test_te.attr,
	&oplus_attr_min_fps.attr,
/* #endif */
#ifdef OPLUS_FEATURE_DISPLAY_TEMP_COMPENSATION
	&oplus_attr_temp_compensation_config.attr,
	&oplus_attr_ntc_temp.attr,
	&oplus_attr_shell_temp.attr,
#endif /* OPLUS_FEATURE_DISPLAY_TEMP_COMPENSATION */
	&oplus_attr_disp_trig_db.attr,
	&oplus_attr_write_panel_reg.attr,
	&oplus_attr_panel_reg_cmd.attr,
	&oplus_attr_dimlayer_bl_en.attr,
	&oplus_attr_dim_dc_alpha.attr,
	&oplus_attr_osc.attr,
	&oplus_attr_pq_trigger.attr,
	&oplus_attr_pwm_turbo.attr,
	&oplus_attr_hbm_max.attr,
	&oplus_attr_pwm_onepulse.attr,
#ifdef OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT
	&oplus_attr_fp_type.attr,
	&oplus_attr_dimlayer_hbm.attr,
	&oplus_attr_oplus_notify_fppress.attr,
	&oplus_attr_hbm.attr,
	&oplus_attr_aod_light_mode_set.attr,
	&oplus_attr_fake_aod.attr,
	&oplus_attr_ultra_low_power_aod_mode.attr,
	&oplus_attr_longrui_aod.attr,
#endif /* OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT */
	&oplus_attr_dsi_cmd.attr,
#ifdef OPLUS_TRACKPOINT_REPORT
	&oplus_attr_trackpoint_test.attr,
#endif /* OPLUS_TRACKPOINT_REPORT */
	&oplus_attr_shutdownflag.attr,
	NULL,	/* need to NULL terminate the list of attributes */
};

static struct attribute_group oplus_display_attr_group = {
	.attrs = oplus_display_attrs,
};

int oplus_display_private_api_init(void)
{
	int retval;

	oplus_display_kobj = kobject_create_and_add("oplus_display", kernel_kobj);
	if (!oplus_display_kobj)
		return -ENOMEM;

	/* Create the files associated with this kobject */
	retval = sysfs_create_group(oplus_display_kobj, &oplus_display_attr_group);
	if (retval)
		kobject_put(oplus_display_kobj);
	OPLUS_DSI_INFO("end\n");

	return retval;
}

void oplus_display_private_api_exit(void)
{
	kobject_put(oplus_display_kobj);
}
