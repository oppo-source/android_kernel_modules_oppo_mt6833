/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_display_apollo_brightness.h
** Description : oplus display apollo_brightness header
** Version : 1.0
** Date : 2024/05/20
** Author : Display
******************************************************************/
#ifndef _OPLUS_DISPLAY_APOLLO_BRIGHTNESS_H_
#define _OPLUS_DISPLAY_APOLLO_BRIGHTNESS_H_

#define BACKLIGHT_CACHE_MAX 50
#define BACKLIGHT_TIME_PERIOD 5
#define APOLLO_CUR_VSYNC_60 16666666
/* -------------------- extern        -------------------------------------------------- */
extern int mutex_sof_ns;


/* -------------------- debug log     -------------------------------------------------- */


/*  --------------------  config    ---------------------------------------------------- */
struct oplus_apollo_brightness {
	bool oplus_backlight_updated;
	int oplus_pending_backlight;
	bool oplus_backlight_need_sync;
	bool oplus_power_on;
	bool oplus_refresh_rate_switching;
	int oplus_te_tag_ns;
	int oplus_timing_switch_tag_ns;
	int oplus_te_diff_ns;
	int cur_vsync;
	int limit_superior_ns;
	int limit_inferior_ns;
	int transfer_time_us;
	int pending_vsync;
	int pending_limit_superior_ns;
	int pending_limit_inferior_ns;
	int pending_transfer_time_us;
	bool pending_pu_enable;
	bool last_frame_pu_enable;
	int pending_h_roi;
	int h_roi;
	int h_max;
};

enum oplus_display_id {
	DISPLAY_PRIMARY = 0,
	DISPLAY_SECONDARY = 1,
	DISPLAY_MAX,
};

static struct backlight_log {
	u32 bl_count;
	unsigned int backlight[BACKLIGHT_CACHE_MAX];
	struct timespec64 past_times[BACKLIGHT_CACHE_MAX];
}oplus_bl_log[DISPLAY_MAX];


/* -------------------- function implementation ---------------------------------------- */



/* -------------------- oplus api nodes ------------------------------------------------ */


#endif /* _OPLUS_DISPLAY_APOLLO_BRIGHTNESS_H_ */
