/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2024 MediaTek Inc. */

#include "oplus_cam_notify.h"
#include "adaptor-subdrv-ctrl.h"
#include "adaptor-i2c.h"

#define POWER_OFF_NOTIFY_DELAY_TIME        10000
#define POWER_ON_NOTIFY_HRBT_TIME          10000

static int cur_poweron_cnt = 0;
static int pre_poweron_cnt = 0;
static bool notify_timer_ready_init = false;
static bool delay_timer_ready_init = false;
static struct timer_list notify_timer;
static struct timer_list delay_timer;
static DEFINE_MUTEX(poweron_cnt_mutex);

static BLOCKING_NOTIFIER_HEAD(camera_notifier_list);

int camera_event_register_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&camera_notifier_list, nb);
}
// EXPORT_SYMBOL(camera_event_register_notifier);

int camera_event_unregister_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&camera_notifier_list, nb);
}
// EXPORT_SYMBOL(camera_event_unregister_notifier);

void camera_event_call_notifier(unsigned long action, int type)
{
	struct camera_notify_event event_data;
	memset(&event_data, 0, sizeof(struct camera_notify_event));
	event_data.type = type;
	printk("CAM NOTIFY CAMERA_CALL: %d\n", event_data.type);
	blocking_notifier_call_chain(&camera_notifier_list, action, (void *)&event_data);
}
// EXPORT_SYMBOL(camera_event_call_notifier);


static void delay_timer_callback(struct timer_list *timer)
{
	camera_event_call_notifier(EVENT_ACTION_FOR_CAMERA, CAMERA_CALL_OFF);
	printk("CAM NOTIFY delay_timer_callback\n");
}

static void init_delay_timer(void)
{
	if (!delay_timer_ready_init) {
		timer_setup(&delay_timer, delay_timer_callback, 0);
		printk("CAM NOTIFY init_delay_timer\n");
		delay_timer_ready_init = true;
	}
}

static void set_delay_timer(void)
{
	mod_timer(&delay_timer, jiffies + msecs_to_jiffies(POWER_OFF_NOTIFY_DELAY_TIME));
	printk("CAM NOTIFY set_delay_timer\n");
}

static void del_delay_timer(void)
{
	printk("CAM NOTIFY del_delay_timer \n");
	if (timer_pending(&delay_timer)){
		del_timer_sync(&delay_timer);
	} else {
		printk("CAM NOTIFY delay_timer is pending\n");
	}
}

static void notify_timer_callback(struct timer_list *timer)
{
	camera_event_call_notifier(EVENT_ACTION_FOR_CAMERA, CAMERA_CALL_ON);
	mod_timer(&notify_timer, jiffies + msecs_to_jiffies(POWER_ON_NOTIFY_HRBT_TIME));
}

static void init_notify_timer(void)
{
	if (!notify_timer_ready_init) {
		timer_setup(&notify_timer, notify_timer_callback, 0);
		printk("CAM NOTIFY init_notify_timer \n");
		notify_timer_ready_init = true;
	}
}

static void set_notify_timer(void)
{
	mod_timer(&notify_timer, jiffies + msecs_to_jiffies(POWER_ON_NOTIFY_HRBT_TIME));
	printk("CAM NOTIFY set_notify_timer \n");
}

static void del_notify_timer(void)
{
	printk("CAM NOTIFY del_notify_timer \n");
	if (timer_pending(&notify_timer)){
		del_timer_sync(&notify_timer);
	} else {
		printk("CAM NOTIFY notify_timer is pending\n");
	}
}

void oplus_camera_call_notifier(struct adaptor_ctx *ctx)
{
	DRV_LOG(ctx, "CAM NOTIFY +");
	if (ctx->subctx.s_ctx.oplus_notify_chg_flag) {
		mutex_lock(&poweron_cnt_mutex);
		if (ctx->power_refcnt) {
			pre_poweron_cnt = cur_poweron_cnt;
			cur_poweron_cnt++;
		} else if (ctx->power_refcnt == 0 && cur_poweron_cnt > 0) {
			pre_poweron_cnt = cur_poweron_cnt;
			cur_poweron_cnt--;
		}
		mutex_unlock(&poweron_cnt_mutex);

		DRV_LOGE(ctx, "CAM NOTIFY power on idx: %d cur_poweron_cnt: %d pre_poweron_cnt: %d\n",
						ctx->idx, cur_poweron_cnt, pre_poweron_cnt);
		switch (cur_poweron_cnt) {
			case 1:
				if (pre_poweron_cnt == 0) {
					DRV_LOG(ctx, "CAM NOTIFY CAMERA_CALL_ON\n");
					camera_event_call_notifier(EVENT_ACTION_FOR_CAMERA, CAMERA_CALL_ON);
					init_notify_timer();
					set_notify_timer();
					del_delay_timer();
				}
			break;
			case 0:
				if (pre_poweron_cnt == 1) {
					DRV_LOG(ctx, "CAM NOTIFY CAMERA_CALL_OFF\n");
					init_delay_timer();
					set_delay_timer();
					del_notify_timer();
				}
			break;
		}
	}
	DRV_LOG(ctx, "CAM NOTIFY -");
}
// EXPORT_SYMBOL(camera_call_notifier);


// MODULE_DESCRIPTION("Camera Event Notify Driver");
// MODULE_LICENSE("GPL");
