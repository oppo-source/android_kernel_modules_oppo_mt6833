/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2024 MediaTek Inc. */

#ifndef _CAMERA_EVENTNOTIFY_H
#define _CAMERA_EVENTNOTIFY_H

#include <linux/module.h>
#include <linux/export.h>
#include <linux/notifier.h>
#include <linux/timer.h>
#include "adaptor.h"
#include <linux/mutex.h>

#define EVENT_ACTION_FOR_CAMERA 0x01

enum CAMERA_CALL_PARA {
	CAMERA_CALL_OFF = 0,
	CAMERA_CALL_ON = 1,
	// CAMERA_CALL_ACTIVE = 2,
};

struct camera_notify_event {
	int type;
};

/* caller API */
int  camera_event_register_notifier(struct notifier_block *nb);
int  camera_event_unregister_notifier(struct notifier_block *nb);
void camera_event_call_notifier(unsigned long action, int type);
void oplus_camera_call_notifier(struct adaptor_ctx *ctx);


#endif /*_CAMERA_EVENTNOTIFY_H*/