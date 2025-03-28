// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

#include <linux/i2c.h>
#include <linux/slab.h>
#include "mtk-i3c-i2c-wrap.h"

#include "adaptor-i2c.h"

#ifndef OPLUS_FEATURE_CAMERA_COMMON
#define MAX_BUF_SIZE 255
#else /*OPLUS_FEATURE_CAMERA_COMMON*/
#define MAX_BUF_SIZE 765
#define MAX_BUF_SIZE_MAX 621
#define MAX_MSG_NUM_U8_MAX (MAX_BUF_SIZE_MAX / 3)
#endif /*OPLUS_FEATURE_CAMERA_COMMON*/
#define MAX_MSG_NUM_U8 (MAX_BUF_SIZE / 3)
#define MAX_MSG_NUM_U16 (MAX_BUF_SIZE / 4)
#define MAX_VAL_NUM_U8 (MAX_BUF_SIZE - 2)
#define MAX_VAL_NUM_U16 ((MAX_BUF_SIZE - 2) >> 1)

struct cache_wr_regs_u8 {
	u8 buf[MAX_BUF_SIZE];
	struct i2c_msg msg[MAX_MSG_NUM_U8];
};

struct cache_wr_regs_u16 {
	u8 buf[MAX_BUF_SIZE];
	struct i2c_msg msg[MAX_MSG_NUM_U16];
};

struct cache_wr_regs_u8_ixc {
	u8 buf[MAX_BUF_SIZE];
	struct i3c_i2c_xfer msg[MAX_MSG_NUM_U8];
};

#ifdef OPLUS_FEATURE_CAMERA_COMMON
struct cache_wr_regs_u8_ixc_max {
	u8 buf[MAX_BUF_SIZE_MAX];
	struct i3c_i2c_xfer msg[MAX_MSG_NUM_U8_MAX];
};
#endif /* OPLUS_FEATURE_CAMERA_COMMON */

struct cache_wr_regs_u16_ixc {
	u8 buf[MAX_BUF_SIZE];
	struct i3c_i2c_xfer msg[MAX_MSG_NUM_U16];
};

#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
inline unsigned char* payLoadConvert(struct adaptor_ctx *ctx, unsigned char* payload, const char* op, u16 addr, u16 reg, u16 val)
{
	if (ctx) {
		scnprintf(payload, PAYLOAD_LENGTH,
			"NULL$$EventField@@%s$$FieldData@@0x%x$$detailData@@sn=%s,sm=%d,op:%s,addr=0x%x,reg=0x%x,val=0x%x",
			acquireEventField(EXCEP_I2C), (CAM_RESERVED_ID << 20 | CAM_MODULE_ID << 12 | EXCEP_I2C),
			ctx->subdrv->name, ctx->subctx.current_scenario_id, op, addr, reg, val);
	}
	return payload;
}
#endif /* OPLUS_FEATURE_CAMERA_COMMON */

struct device *adaptor_ixc_get_dev (struct i3c_i2c_device *client)
{
	struct device *dev = NULL;

	if (client == NULL)
		return NULL;
	if (client->protocol == I3C_PROTOCOL)
		dev = &client->i3c_dev->dev;
	else if (client->protocol == I2C_PROTOCOL)
		dev = &client->i2c_dev->dev;

	return dev;
}

void *adaptor_ixc_get_clientdata(struct i3c_i2c_device *client)
{
	struct device *dev = adaptor_ixc_get_dev(client);

	if (dev == NULL)
		return NULL;
	return dev_get_drvdata(dev);
}

int adaptor_ixc_do_daa (struct i3c_i2c_device *client)
{
	int ret = 0;

	if (client == NULL)
		return -ENODEV;

	ret = mtk_i3c_i2c_device_do_daa(client);
	// if do daa error, retry it
	if (ret)
		ret = mtk_i3c_i2c_device_do_daa(client);

	return ret;
}

int adaptor_i2c_rd_u8(struct i2c_client *i2c_client,
		u16 addr, u16 reg, u8 *val)
{
	int ret;
	u8 buf[2];
	struct i2c_msg msg[2];
	#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
	struct v4l2_subdev *sd = NULL;
	struct adaptor_ctx *ctx = NULL;
	unsigned char payload[PAYLOAD_LENGTH] = {0x00};

	sd = i2c_get_clientdata(i2c_client);
	if (sd) {
		ctx = to_ctx(sd);
	}
	#endif /* OPLUS_FEATURE_CAMERA_COMMON */

	if (i2c_client == NULL)
		return -ENODEV;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;

	msg[0].addr = addr;
	msg[0].flags = i2c_client->flags;
	msg[0].buf = buf;
	msg[0].len = sizeof(buf);

	msg[1].addr = addr;
	msg[1].flags = i2c_client->flags | I2C_M_RD;
	msg[1].buf = buf;
	msg[1].len = 1;

	ret = i2c_transfer(i2c_client->adapter, msg, 2);
	if (ret < 0) {
		dev_info(&i2c_client->dev, "i2c transfer failed (%d)\n", ret);
		#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
		if (ctx)
			cam_olc_raise_exception(EXCEP_I2C, payLoadConvert(ctx, payload, "rd_u8", addr, reg, 0));
		#endif /* OPLUS_FEATURE_CAMERA_COMMON */
		return ret;
	}

	*val = buf[0];

	return 0;
}

int adaptor_i2c_rd_u16(struct i2c_client *i2c_client,
		u16 addr, u16 reg, u16 *val)
{
	int ret;
	u8 buf[2];
	struct i2c_msg msg[2];
	#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
	struct v4l2_subdev *sd = NULL;
	struct adaptor_ctx *ctx = NULL;
	unsigned char payload[PAYLOAD_LENGTH] = {0x00};

	sd = i2c_get_clientdata(i2c_client);
	if (sd) {
		ctx = to_ctx(sd);
	}
	#endif /* OPLUS_FEATURE_CAMERA_COMMON */

	if (i2c_client == NULL)
		return -ENODEV;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;

	msg[0].addr = addr;
	msg[0].flags = i2c_client->flags;
	msg[0].buf = buf;
	msg[0].len = sizeof(buf);

	msg[1].addr  = addr;
	msg[1].flags = i2c_client->flags | I2C_M_RD;
	msg[1].buf = buf;
	msg[1].len = 2;

	ret = i2c_transfer(i2c_client->adapter, msg, 2);
	if (ret < 0) {
		dev_info(&i2c_client->dev, "i2c transfer failed (%d)\n", ret);
		#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
		if (ctx)
			cam_olc_raise_exception(EXCEP_I2C, payLoadConvert(ctx, payload, "rd_u16", addr, reg, 0));
		#endif /* OPLUS_FEATURE_CAMERA_COMMON */
		return ret;
	}

	*val = ((u16)buf[0] << 8) | buf[1];

	return 0;
}

int adaptor_i2c_rd_p8(struct i2c_client *i2c_client,
		u16 addr, u16 reg, u8 *p_vals, u32 n_vals)
{
	int ret, cnt, total, recv, reg_b;
	u8 buf[2];
	struct i2c_msg msg[2];
	u8 *pbuf;
	#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
	struct v4l2_subdev *sd = NULL;
	struct adaptor_ctx *ctx = NULL;
	unsigned char payload[PAYLOAD_LENGTH] = {0x00};

	sd = i2c_get_clientdata(i2c_client);
	if (sd) {
		ctx = to_ctx(sd);
	}
	#endif /* OPLUS_FEATURE_CAMERA_COMMON */

	if (i2c_client == NULL)
		return -ENODEV;

	recv = 0;
	total = n_vals;
	pbuf = p_vals;
	reg_b = reg;

	msg[0].addr = addr;
	msg[0].flags = i2c_client->flags;
	msg[0].buf = buf;
	msg[0].len = sizeof(buf);

	msg[1].addr = addr;
	msg[1].flags = i2c_client->flags | I2C_M_RD;

	while (recv < total) {

		cnt = total - recv;
		if (cnt > MAX_VAL_NUM_U8)
			cnt = MAX_VAL_NUM_U8;

		buf[0] = reg_b >> 8;
		buf[1] = reg_b & 0xff;

		msg[1].buf = pbuf;
		msg[1].len = cnt;

		ret = i2c_transfer(i2c_client->adapter, msg, 2);
		if (ret < 0) {
			dev_info(&i2c_client->dev,
				"i2c transfer failed (%d)\n", ret);
			#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
			if (ctx)
				cam_olc_raise_exception(EXCEP_I2C, payLoadConvert(ctx, payload, "rd_p8", addr, reg, 0));
			#endif /* OPLUS_FEATURE_CAMERA_COMMON */
			return -EIO;
		}

		pbuf += cnt;
		recv += cnt;
		reg_b += cnt;
	}

	return ret;
}

int adaptor_i2c_wr_u8(struct i2c_client *i2c_client,
		u16 addr, u16 reg, u8 val)
{
	int ret;
	u8 buf[3];
	struct i2c_msg msg;
	#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
	struct v4l2_subdev *sd = NULL;
	struct adaptor_ctx *ctx = NULL;
	unsigned char payload[PAYLOAD_LENGTH] = {0x00};

	sd = i2c_get_clientdata(i2c_client);
	if (sd) {
		ctx = to_ctx(sd);
	}
	#endif /* OPLUS_FEATURE_CAMERA_COMMON */

	if (i2c_client == NULL)
		return -ENODEV;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;
	buf[2] = val;

	msg.addr = addr;
	msg.flags = i2c_client->flags;
	msg.buf = buf;
	msg.len = sizeof(buf);

	ret = i2c_transfer(i2c_client->adapter, &msg, 1);
	if (ret < 0) {
		dev_info(&i2c_client->dev, "i2c transfer failed (%d)\n", ret);
		#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
		if (ctx)
			cam_olc_raise_exception(EXCEP_I2C, payLoadConvert(ctx, payload, "wr_u8", addr, reg, val));
		#endif /* OPLUS_FEATURE_CAMERA_COMMON */
	}

	return ret;
}

int adaptor_i2c_wr_u16(struct i2c_client *i2c_client,
		u16 addr, u16 reg, u16 val)
{
	int ret;
	u8 buf[4];
	struct i2c_msg msg;
	#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
	struct v4l2_subdev *sd = NULL;
	struct adaptor_ctx *ctx = NULL;
	unsigned char payload[PAYLOAD_LENGTH] = {0x00};

	sd = i2c_get_clientdata(i2c_client);
	if (sd) {
		ctx = to_ctx(sd);
	}
	#endif /* OPLUS_FEATURE_CAMERA_COMMON */

	if (i2c_client == NULL)
		return -ENODEV;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;
	buf[2] = val >> 8;
	buf[3] = val & 0xff;

	msg.addr = addr;
	msg.flags = i2c_client->flags;
	msg.buf = buf;
	msg.len = sizeof(buf);

	ret = i2c_transfer(i2c_client->adapter, &msg, 1);
	if (ret < 0) {
		dev_info(&i2c_client->dev, "i2c transfer failed (%d)\n", ret);
		#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
		if (ctx)
			cam_olc_raise_exception(EXCEP_I2C, payLoadConvert(ctx, payload, "wr_u16", addr, reg, val));
		#endif /* OPLUS_FEATURE_CAMERA_COMMON */
	}

	return ret;
}

int adaptor_i2c_wr_p8(struct i2c_client *i2c_client,
		u16 addr, u16 reg, u8 *p_vals, u32 n_vals)
{
	u8 *buf, *pbuf, *pdata;
	struct i2c_msg msg;
	int ret, sent, total, cnt;
	#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
	struct v4l2_subdev *sd = NULL;
	struct adaptor_ctx *ctx = NULL;
	unsigned char payload[PAYLOAD_LENGTH] = {0x00};

	sd = i2c_get_clientdata(i2c_client);
	if (sd) {
		ctx = to_ctx(sd);
	}
	#endif /* OPLUS_FEATURE_CAMERA_COMMON */

	if (i2c_client == NULL)
		return -ENODEV;

	buf = kmalloc(MAX_BUF_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	sent = 0;
	total = n_vals;
	pdata = p_vals;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;

	msg.addr = addr;
	msg.flags = i2c_client->flags;
	msg.buf = buf;

	while (sent < total) {

		cnt = total - sent;
		if (cnt > MAX_VAL_NUM_U8)
			cnt = MAX_VAL_NUM_U8;

		pbuf = buf + 2;
		memcpy(pbuf, pdata, cnt);

		msg.len = 2 + cnt;

		ret = i2c_transfer(i2c_client->adapter, &msg, 1);
		if (ret < 0) {
			dev_info(&i2c_client->dev,
				"i2c transfer failed (%d)\n", ret);
			#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
			if (ctx)
				cam_olc_raise_exception(EXCEP_I2C, payLoadConvert(ctx, payload, "wr_p8", addr, reg, 0));
			#endif /* OPLUS_FEATURE_CAMERA_COMMON */
			kfree(buf);
			return -EIO;
		}

		sent += cnt;
		pdata += cnt;
	}

	kfree(buf);

	return 0;
}

int adaptor_i2c_wr_p16(struct i2c_client *i2c_client,
		u16 addr, u16 reg, u16 *p_vals, u32 n_vals)
{
	u8 *buf, *pbuf;
	u16 *pdata;
	struct i2c_msg msg;
	int i, ret, sent, total, cnt;
	#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
	struct v4l2_subdev *sd = NULL;
	struct adaptor_ctx *ctx = NULL;
	unsigned char payload[PAYLOAD_LENGTH] = {0x00};

	sd = i2c_get_clientdata(i2c_client);
	if (sd) {
		ctx = to_ctx(sd);
	}
	#endif /* OPLUS_FEATURE_CAMERA_COMMON */

	if (i2c_client == NULL)
		return -ENODEV;

	buf = kmalloc(MAX_BUF_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	sent = 0;
	total = n_vals;
	pdata = p_vals;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;

	msg.addr = addr;
	msg.flags = i2c_client->flags;
	msg.buf = buf;

	while (sent < total) {

		cnt = total - sent;
		if (cnt > MAX_VAL_NUM_U16)
			cnt = MAX_VAL_NUM_U16;

		pbuf = buf + 2;

		for (i = 0; i < cnt; i++) {
			pbuf[0] = pdata[0] >> 8;
			pbuf[1] = pdata[0] & 0xff;
			pdata++;
			pbuf += 2;
		}

		msg.len = 2 + (cnt << 1);

		ret = i2c_transfer(i2c_client->adapter, &msg, 1);
		if (ret < 0) {
			dev_info(&i2c_client->dev,
				"i2c transfer failed (%d)\n", ret);
			#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
			if (ctx)
				cam_olc_raise_exception(EXCEP_I2C, payLoadConvert(ctx, payload, "wr_p16", addr, reg, 0));
			#endif /* OPLUS_FEATURE_CAMERA_COMMON */
			kfree(buf);
			return -EIO;
		}

		sent += cnt;
	}

	kfree(buf);

	return 0;
}

int adaptor_i2c_wr_seq_p8(struct i2c_client *i2c_client,
		u16 addr, u16 reg, u8 *p_vals, u32 n_vals)
{
	u8 *buf, *pbuf, *pdata;
	struct i2c_msg msg;
	int ret, sent, total, cnt;
	#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
	struct v4l2_subdev *sd = NULL;
	struct adaptor_ctx *ctx = NULL;
	unsigned char payload[PAYLOAD_LENGTH] = {0x00};

	sd = i2c_get_clientdata(i2c_client);
	if (sd) {
		ctx = to_ctx(sd);
	}
	#endif /* OPLUS_FEATURE_CAMERA_COMMON */

	if (i2c_client == NULL)
		return -ENODEV;

	buf = kmalloc(MAX_BUF_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	sent = 0;
	total = n_vals;
	pdata = p_vals;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;

	msg.addr = addr;
	msg.flags = i2c_client->flags;
	msg.buf = buf;

	while (sent < total) {

		cnt = total - sent;
		if (cnt > MAX_VAL_NUM_U8)
			cnt = MAX_VAL_NUM_U8;

		buf[0] = reg >> 8;
		buf[1] = reg & 0xff;

		pbuf = buf + 2;
		memcpy(pbuf, pdata, cnt);

		msg.len = 2 + cnt;

		ret = i2c_transfer(i2c_client->adapter, &msg, 1);
		if (ret < 0) {
			dev_info(&i2c_client->dev,
				"i2c transfer failed (%d)\n", ret);
			#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
			if (ctx)
				cam_olc_raise_exception(EXCEP_I2C, payLoadConvert(ctx, payload, "wr_seq_p8", addr, reg, 0));
			#endif /* OPLUS_FEATURE_CAMERA_COMMON */
			kfree(buf);
			return -EIO;
		}

		sent += cnt;
		pdata += cnt;
		reg += cnt;
	}

	kfree(buf);

	return 0;
}

int adaptor_i2c_wr_regs_u8(struct i2c_client *i2c_client,
		u16 addr, u16 *list, u32 len)
{
	struct cache_wr_regs_u8 *pmem;
	struct i2c_msg *pmsg;
	u8 *pbuf;
	u16 *plist;
	int i, ret, sent, total, cnt;
	#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
	struct v4l2_subdev *sd = NULL;
	struct adaptor_ctx *ctx = NULL;
	unsigned char payload[PAYLOAD_LENGTH] = {0x00};

	sd = i2c_get_clientdata(i2c_client);
	if (sd) {
		ctx = to_ctx(sd);
	}
	#endif /* OPLUS_FEATURE_CAMERA_COMMON */

	if (i2c_client == NULL)
		return -ENODEV;

	pmem = kmalloc(sizeof(*pmem), GFP_KERNEL);
	if (!pmem)
		return -ENOMEM;

	/* each msg contains 3 bytes: addr(u16) + val(u8) */
	sent = 0;
	total = len >> 1;
	plist = list;

	while (sent < total) {

		cnt = total - sent;
		if (cnt > ARRAY_SIZE(pmem->msg))
			cnt = ARRAY_SIZE(pmem->msg);

		pbuf = pmem->buf;
		pmsg = pmem->msg;

		for (i = 0; i < cnt; i++) {

			pbuf[0] = plist[0] >> 8;
			pbuf[1] = plist[0] & 0xff;
			pbuf[2] = plist[1] & 0xff;

			pmsg->addr = addr;
			pmsg->flags = i2c_client->flags;
			pmsg->len = 3;
			pmsg->buf = pbuf;

			plist += 2;
			pbuf += 3;
			pmsg++;
		}

		ret = i2c_transfer(i2c_client->adapter, pmem->msg, cnt);
		if (ret != cnt) {
			dev_info(&i2c_client->dev,
				"i2c transfer failed (%d)\n", ret);
			#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
			if (ctx)
				cam_olc_raise_exception(EXCEP_I2C, payLoadConvert(ctx, payload, "wr_regs_u8", addr, list[0], list[1]));
			#endif /* OPLUS_FEATURE_CAMERA_COMMON */
			kfree(pmem);
			return -EIO;
		}

		sent += cnt;
	}

	kfree(pmem);

	return 0;
}

int adaptor_i2c_wr_regs_u16(struct i2c_client *i2c_client,
		u16 addr, u16 *list, u32 len)
{
	struct cache_wr_regs_u16 *pmem;
	struct i2c_msg *pmsg;
	u8 *pbuf;
	u16 *plist;
	int i, ret, sent, total, cnt;
	#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
	struct v4l2_subdev *sd = NULL;
	struct adaptor_ctx *ctx = NULL;
	unsigned char payload[PAYLOAD_LENGTH] = {0x00};

	sd = i2c_get_clientdata(i2c_client);
	if (sd) {
		ctx = to_ctx(sd);
	}
	#endif /* OPLUS_FEATURE_CAMERA_COMMON */

	if (i2c_client == NULL)
		return -ENODEV;

	pmem = kmalloc(sizeof(*pmem), GFP_KERNEL);
	if (!pmem)
		return -ENOMEM;

	/* each msg contains 4 bytes: addr(u16) + val(u16) */
	sent = 0;
	total = len >> 1;
	plist = list;

	while (sent < total) {

		cnt = total - sent;
		if (cnt > ARRAY_SIZE(pmem->msg))
			cnt = ARRAY_SIZE(pmem->msg);

		pbuf = pmem->buf;
		pmsg = pmem->msg;

		for (i = 0; i < cnt; i++) {

			pbuf[0] = plist[0] >> 8;
			pbuf[1] = plist[0] & 0xff;
			pbuf[2] = plist[1] >> 8;
			pbuf[3] = plist[1] & 0xff;

			pmsg->addr = addr;
			pmsg->flags = i2c_client->flags;
			pmsg->len = 4;
			pmsg->buf = pbuf;

			plist += 2;
			pbuf += 4;
			pmsg++;
		}

		ret = i2c_transfer(i2c_client->adapter, pmem->msg, cnt);

		if (ret != cnt) {
			dev_info(&i2c_client->dev,
				"i2c transfer failed (%d)\n", ret);
			#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
			if (ctx)
				cam_olc_raise_exception(EXCEP_I2C, payLoadConvert(ctx, payload, "wr_regs_u16", addr, list[0], list[1]));
			#endif /* OPLUS_FEATURE_CAMERA_COMMON */
			kfree(pmem);
			return -EIO;
		}

		sent += cnt;
	}

	kfree(pmem);

	return 0;
}

int adaptor_ixc_rd_u8(struct i3c_i2c_device *client,
		u16 addr, u16 reg, u8 *val)
{
	int ret;
	u8 buf[2];
	struct i3c_i2c_xfer msg[2];
	struct device *dev = adaptor_ixc_get_dev(client);
	#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
	struct v4l2_subdev *sd = NULL;
	struct adaptor_ctx *ctx = NULL;
	unsigned char payload[PAYLOAD_LENGTH] = {0x00};

	sd = adaptor_ixc_get_clientdata(client);
	if (sd) {
		ctx = to_ctx(sd);
	}
	#endif /* OPLUS_FEATURE_CAMERA_COMMON */

	if (client == NULL)
		return -ENODEV;

	if (client->protocol == I2C_PROTOCOL) {
		msg[0].addr = addr;
		msg[1].addr = addr;
	}

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;

	msg[0].flags = 0;
	msg[0].buf = buf;
	msg[0].len = sizeof(buf);

	msg[1].flags = 1;
	msg[1].buf = buf;
	msg[1].len = 1;

	ret = i3c_i2c_transfer(client, msg, 2);
	if (ret < 0) {
		dev_info(dev, "[%s]ixc transfer failed (%d)\n", __func__,ret);
		#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
		if (ctx)
			cam_olc_raise_exception(EXCEP_I2C, payLoadConvert(ctx, payload, "ixc_rd_u8", addr, reg, 0));
		#endif /* OPLUS_FEATURE_CAMERA_COMMON */
		return ret;
	}

	*val = buf[0];

	return 0;
}

int adaptor_ixc_rd_u16(struct i3c_i2c_device *client,
		u16 addr, u16 reg, u16 *val)
{
	int ret;
	u8 buf[2];
	struct i3c_i2c_xfer msg[2];
	struct device *dev = adaptor_ixc_get_dev(client);
	#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
	struct v4l2_subdev *sd = NULL;
	struct adaptor_ctx *ctx = NULL;
	unsigned char payload[PAYLOAD_LENGTH] = {0x00};

	sd = adaptor_ixc_get_clientdata(client);
	if (sd) {
		ctx = to_ctx(sd);
	}
	#endif /* OPLUS_FEATURE_CAMERA_COMMON */

	if (client == NULL)
		return -ENODEV;

	if (client->protocol == I2C_PROTOCOL) {
		msg[0].addr = addr;
		msg[1].addr = addr;
	}

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;

	msg[0].flags = 0;
	msg[0].buf = buf;
	msg[0].len = sizeof(buf);

	msg[1].flags = 1;
	msg[1].buf = buf;
	msg[1].len = 2;

	ret = i3c_i2c_transfer(client, msg, 2);
	if (ret < 0) {
		dev_info(dev, "[%s]ixc transfer failed (%d)\n", __func__,ret);
		#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
		if (ctx)
			cam_olc_raise_exception(EXCEP_I2C, payLoadConvert(ctx, payload, "ixc_rd_u16", addr, reg, 0));
		#endif /* OPLUS_FEATURE_CAMERA_COMMON */
		return ret;
	}

	*val = ((u16)buf[0] << 8) | buf[1];

	return 0;
}

int adaptor_ixc_rd_p8(struct i3c_i2c_device *client,
		u16 addr, u16 reg, u8 *p_vals, u32 n_vals)
{
	int ret, cnt, total, recv, reg_b;
	u8 buf[2];
	struct i3c_i2c_xfer msg[2];
	struct device *dev = adaptor_ixc_get_dev(client);
	u8 *pbuf;
	#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
	struct v4l2_subdev *sd = NULL;
	struct adaptor_ctx *ctx = NULL;
	unsigned char payload[PAYLOAD_LENGTH] = {0x00};

	sd = adaptor_ixc_get_clientdata(client);
	if (sd) {
		ctx = to_ctx(sd);
	}
	#endif /* OPLUS_FEATURE_CAMERA_COMMON */

	if (client == NULL)
		return -ENODEV;

	if (client->protocol == I2C_PROTOCOL) {
		msg[0].addr = addr;
		msg[1].addr = addr;
	}

	recv = 0;
	total = n_vals;
	pbuf = p_vals;
	reg_b = reg;

	msg[0].flags = 0;
	msg[0].buf = buf;
	msg[0].len = sizeof(buf);

	msg[1].flags = 1;

	while (recv < total) {

		cnt = total - recv;
		if (cnt > MAX_VAL_NUM_U8)
			cnt = MAX_VAL_NUM_U8;

		buf[0] = reg_b >> 8;
		buf[1] = reg_b & 0xff;

		msg[1].buf = pbuf;
		msg[1].len = cnt;

		ret = i3c_i2c_transfer(client, msg, 2);
		if (ret < 0) {
			dev_info(dev, "[%s]ixc transfer failed (%d)\n", __func__,ret);
			#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
			if (ctx)
				cam_olc_raise_exception(EXCEP_I2C, payLoadConvert(ctx, payload, "ixc_rd_p8", addr, reg, 0));
			#endif /* OPLUS_FEATURE_CAMERA_COMMON */
			return -EIO;
		}

		pbuf += cnt;
		recv += cnt;
		reg_b += cnt;
	}

	return ret;
}

int adaptor_ixc_wr_u8(struct i3c_i2c_device *client,
		u16 addr, u16 reg, u8 val)
{
	int ret;
	u8 buf[3];
	struct i3c_i2c_xfer msg;
	struct device *dev = adaptor_ixc_get_dev(client);
	#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
	struct v4l2_subdev *sd = NULL;
	struct adaptor_ctx *ctx = NULL;
	unsigned char payload[PAYLOAD_LENGTH] = {0x00};

	sd = adaptor_ixc_get_clientdata(client);
	if (sd) {
		ctx = to_ctx(sd);
	}
	#endif /* OPLUS_FEATURE_CAMERA_COMMON */

	if (client == NULL)
		return -ENODEV;

	if (client->protocol == I2C_PROTOCOL)
		msg.addr = addr;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;
	buf[2] = val;

	msg.flags = 0;
	msg.buf = buf;
	msg.len = sizeof(buf);

	ret = i3c_i2c_transfer(client, &msg, 1);
	if (ret < 0) {
		dev_info(dev, "[%s]ixc transfer failed (%d)\n", __func__,ret);
		#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
		if (ctx)
			cam_olc_raise_exception(EXCEP_I2C, payLoadConvert(ctx, payload, "ixc_wr_u8", addr, reg, val));
		#endif /* OPLUS_FEATURE_CAMERA_COMMON */
	}

	return ret;
}

int adaptor_ixc_wr_u16(struct i3c_i2c_device *client,
		u16 addr, u16 reg, u16 val)
{
	int ret;
	u8 buf[4];
	struct i3c_i2c_xfer msg;
	struct device *dev = adaptor_ixc_get_dev(client);
	#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
	struct v4l2_subdev *sd = NULL;
	struct adaptor_ctx *ctx = NULL;
	unsigned char payload[PAYLOAD_LENGTH] = {0x00};

	sd = adaptor_ixc_get_clientdata(client);
	if (sd) {
		ctx = to_ctx(sd);
	}
	#endif /* OPLUS_FEATURE_CAMERA_COMMON */

	if (client == NULL)
		return -ENODEV;

	if (client->protocol == I2C_PROTOCOL)
		msg.addr = addr;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;
	buf[2] = val >> 8;
	buf[3] = val & 0xff;

	msg.flags = 0;
	msg.buf = buf;
	msg.len = sizeof(buf);

	ret = i3c_i2c_transfer(client, &msg, 1);
	if (ret < 0) {
		dev_info(dev, "[%s]ixc transfer failed (%d)\n", __func__,ret);
		#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
		if (ctx)
			cam_olc_raise_exception(EXCEP_I2C, payLoadConvert(ctx, payload, "ixc_wr_u16", addr, reg, val));
		#endif /* OPLUS_FEATURE_CAMERA_COMMON */
	}

	return ret;
}

int adaptor_ixc_wr_p8(struct i3c_i2c_device *client,
		u16 addr, u16 reg, u8 *p_vals, u32 n_vals)
{
	u8 *buf, *pbuf, *pdata;
	struct i3c_i2c_xfer msg;
	struct device *dev = adaptor_ixc_get_dev(client);
	int ret, sent, total, cnt;
	#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
	struct v4l2_subdev *sd = NULL;
	struct adaptor_ctx *ctx = NULL;
	unsigned char payload[PAYLOAD_LENGTH] = {0x00};

	sd = adaptor_ixc_get_clientdata(client);
	if (sd) {
		ctx = to_ctx(sd);
	}
	#endif /* OPLUS_FEATURE_CAMERA_COMMON */

	if (client == NULL)
		return -ENODEV;

	if (client->protocol == I2C_PROTOCOL)
		msg.addr = addr;

	buf = kmalloc(MAX_BUF_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	sent = 0;
	total = n_vals;
	pdata = p_vals;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;

	msg.flags = 0;
	msg.buf = buf;

	while (sent < total) {

		cnt = total - sent;
		if (cnt > MAX_VAL_NUM_U8)
			cnt = MAX_VAL_NUM_U8;

		pbuf = buf + 2;
		memcpy(pbuf, pdata, cnt);

		msg.len = 2 + cnt;

		ret = i3c_i2c_transfer(client, &msg, 1);
		if (ret < 0) {
			dev_info(dev, "[%s]ixc transfer failed (%d)\n", __func__,ret);
			#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
			if (ctx)
				cam_olc_raise_exception(EXCEP_I2C, payLoadConvert(ctx, payload, "ixc_wr_p8", addr, reg, 0));
			#endif /* OPLUS_FEATURE_CAMERA_COMMON */
			kfree(buf);
			return -EIO;
		}

		sent += cnt;
		pdata += cnt;
	}

	kfree(buf);

	return 0;
}

int adaptor_ixc_wr_p16(struct i3c_i2c_device *client,
		u16 addr, u16 reg, u16 *p_vals, u32 n_vals)
{
	u8 *buf, *pbuf;
	u16 *pdata;
	struct i3c_i2c_xfer msg;
	struct device *dev = adaptor_ixc_get_dev(client);
	int i, ret, sent, total, cnt;
	#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
	struct v4l2_subdev *sd = NULL;
	struct adaptor_ctx *ctx = NULL;
	unsigned char payload[PAYLOAD_LENGTH] = {0x00};

	sd = adaptor_ixc_get_clientdata(client);
	if (sd) {
		ctx = to_ctx(sd);
	}
	#endif /* OPLUS_FEATURE_CAMERA_COMMON */

	if (client == NULL)
		return -ENODEV;

	if (client->protocol == I2C_PROTOCOL)
		msg.addr = addr;

	buf = kmalloc(MAX_BUF_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	sent = 0;
	total = n_vals;
	pdata = p_vals;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;

	msg.flags = 0;
	msg.buf = buf;

	while (sent < total) {

		cnt = total - sent;
		if (cnt > MAX_VAL_NUM_U16)
			cnt = MAX_VAL_NUM_U16;

		pbuf = buf + 2;

		for (i = 0; i < cnt; i++) {
			pbuf[0] = pdata[0] >> 8;
			pbuf[1] = pdata[0] & 0xff;
			pdata++;
			pbuf += 2;
		}

		msg.len = 2 + (cnt << 1);

		ret = i3c_i2c_transfer(client, &msg, 1);
		if (ret < 0) {
			dev_info(dev, "[%s]ixc transfer failed (%d)\n", __func__,ret);
			#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
			if (ctx)
				cam_olc_raise_exception(EXCEP_I2C, payLoadConvert(ctx, payload, "ixc_wr_p16", addr, reg, 0));
			#endif /* OPLUS_FEATURE_CAMERA_COMMON */
			kfree(buf);
			return -EIO;
		}

		sent += cnt;
	}

	kfree(buf);

	return 0;
}

int adaptor_ixc_wr_seq_p8(struct i3c_i2c_device *client,
		u16 addr, u16 reg, u8 *p_vals, u32 n_vals)
{
	u8 *buf, *pbuf, *pdata;
	struct i3c_i2c_xfer msg;
	struct device *dev = adaptor_ixc_get_dev(client);
	int ret, sent, total, cnt;
	#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
	struct v4l2_subdev *sd = NULL;
	struct adaptor_ctx *ctx = NULL;
	unsigned char payload[PAYLOAD_LENGTH] = {0x00};

	sd = adaptor_ixc_get_clientdata(client);
	if (sd) {
		ctx = to_ctx(sd);
	}
	#endif /* OPLUS_FEATURE_CAMERA_COMMON */

	if (client == NULL)
		return -ENODEV;

	if (client->protocol == I2C_PROTOCOL)
		msg.addr = addr;

	buf = kmalloc(MAX_BUF_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	sent = 0;
	total = n_vals;
	pdata = p_vals;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;

	msg.flags = 0;
	msg.buf = buf;

	while (sent < total) {

		cnt = total - sent;
		if (cnt > MAX_VAL_NUM_U8)
			cnt = MAX_VAL_NUM_U8;

		buf[0] = reg >> 8;
		buf[1] = reg & 0xff;

		pbuf = buf + 2;
		memcpy(pbuf, pdata, cnt);

		msg.len = 2 + cnt;

		ret = i3c_i2c_transfer(client, &msg, 1);
		if (ret < 0) {
			dev_info(dev, "[%s]ixc transfer failed (%d)\n", __func__,ret);
			#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
			if (ctx)
				cam_olc_raise_exception(EXCEP_I2C, payLoadConvert(ctx, payload, "ixc_wr_seq_p8", addr, reg, 0));
			#endif /* OPLUS_FEATURE_CAMERA_COMMON */
			kfree(buf);
			return -EIO;
		}

		sent += cnt;
		pdata += cnt;
		reg += cnt;
	}

	kfree(buf);

	return 0;
}

int adaptor_ixc_wr_regs_u8(struct i3c_i2c_device *client,
		u16 addr, u16 *list, u32 len)
{
	struct cache_wr_regs_u8_ixc *pmem;
	struct i3c_i2c_xfer *pmsg;
	struct device *dev = adaptor_ixc_get_dev(client);
	u8 *pbuf;
	u16 *plist;
	int i, ret, sent, total, cnt;
	#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
	struct v4l2_subdev *sd = NULL;
	struct adaptor_ctx *ctx = NULL;
	unsigned char payload[PAYLOAD_LENGTH] = {0x00};

	sd = adaptor_ixc_get_clientdata(client);
	if (sd) {
		ctx = to_ctx(sd);
	}
	#endif /* OPLUS_FEATURE_CAMERA_COMMON */

	if (client == NULL)
		return -ENODEV;

	pmem = kmalloc(sizeof(*pmem), GFP_KERNEL);
	if (!pmem)
		return -ENOMEM;

	/* each msg contains 3 bytes: addr(u16) + val(u8) */
	sent = 0;
	total = len >> 1;
	plist = list;

	while (sent < total) {

		cnt = total - sent;
		if (cnt > ARRAY_SIZE(pmem->msg))
			cnt = ARRAY_SIZE(pmem->msg);

		pbuf = pmem->buf;
		pmsg = pmem->msg;

		for (i = 0; i < cnt; i++) {

			pbuf[0] = plist[0] >> 8;
			pbuf[1] = plist[0] & 0xff;
			pbuf[2] = plist[1] & 0xff;

			if (client->protocol == I2C_PROTOCOL)
				pmsg->addr = addr;

			pmsg->flags = 0;
			pmsg->len = 3;
			pmsg->buf = pbuf;

			plist += 2;
			pbuf += 3;
			pmsg++;
		}

		ret = i3c_i2c_transfer(client, pmem->msg, cnt);
		if (ret < 0){
			dev_info(dev, "[%s]ixc transfer failed (%d)\n", __func__,ret);
			#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
			if (ctx)
				cam_olc_raise_exception(EXCEP_I2C, payLoadConvert(ctx, payload, "ixc_wr_regs_u8", addr, plist[0], plist[1]));
			#endif /* OPLUS_FEATURE_CAMERA_COMMON */
			kfree(pmem);
			return -EIO;
		}

		sent += cnt;
	}

	kfree(pmem);

	return 0;
}

#ifdef OPLUS_FEATURE_CAMERA_COMMON
int adaptor_ixc_wr_regs_u8_max(struct i3c_i2c_device *client,
		u16 addr, u16 *list, u32 len)
{
	struct cache_wr_regs_u8_ixc_max *pmem;
	struct i3c_i2c_xfer *pmsg;
	struct device *dev = adaptor_ixc_get_dev(client);
	u8 *pbuf;
	u16 *plist;
	int i, ret, sent, total, cnt;
	#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
	struct v4l2_subdev *sd = NULL;
	struct adaptor_ctx *ctx = NULL;
	unsigned char payload[PAYLOAD_LENGTH] = {0x00};

	sd = adaptor_ixc_get_clientdata(client);
	if (sd) {
		ctx = to_ctx(sd);
	}
	#endif /* OPLUS_FEATURE_CAMERA_COMMON */

	if (client == NULL)
		return -ENODEV;

	pmem = kmalloc(sizeof(*pmem), GFP_KERNEL);
	if (!pmem)
		return -ENOMEM;

	/* each msg contains 3 bytes: addr(u16) + val(u8) */
	sent = 0;
	total = len >> 1;
	plist = list;

	while (sent < total) {

		cnt = total - sent;
		if (cnt > ARRAY_SIZE(pmem->msg))
			cnt = ARRAY_SIZE(pmem->msg);

		pbuf = pmem->buf;
		pmsg = pmem->msg;

		for (i = 0; i < cnt; i++) {

			pbuf[0] = plist[0] >> 8;
			pbuf[1] = plist[0] & 0xff;
			pbuf[2] = plist[1] & 0xff;

			if (client->protocol == I2C_PROTOCOL)
				pmsg->addr = addr;

			pmsg->flags = 0;
			pmsg->len = 3;
			pmsg->buf = pbuf;

			plist += 2;
			pbuf += 3;
			pmsg++;
		}

		ret = i3c_i2c_transfer(client, pmem->msg, cnt);
		if (ret < 0){
			dev_info(dev, "[%s]ixc transfer failed (%d)\n", __func__,ret);
			#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
			if (ctx)
				cam_olc_raise_exception(EXCEP_I2C, payLoadConvert(ctx, payload, "ixc_wr_regs_u8", addr, plist[0], plist[1]));
			#endif /* OPLUS_FEATURE_CAMERA_COMMON */
			kfree(pmem);
			return -EIO;
		}

		sent += cnt;
	}

	kfree(pmem);

	return 0;
}
#endif /* OPLUS_FEATURE_CAMERA_COMMON */

int adaptor_ixc_wr_regs_u16(struct i3c_i2c_device *client,
		u16 addr, u16 *list, u32 len)
{
	struct cache_wr_regs_u16_ixc *pmem;
	struct i3c_i2c_xfer *pmsg;
	struct device *dev = adaptor_ixc_get_dev(client);
	u8 *pbuf;
	u16 *plist;
	int i, ret, sent, total, cnt;
	#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
	struct v4l2_subdev *sd = NULL;
	struct adaptor_ctx *ctx = NULL;
	unsigned char payload[PAYLOAD_LENGTH] = {0x00};

	sd = adaptor_ixc_get_clientdata(client);
	if (sd) {
		ctx = to_ctx(sd);
	}
	#endif /* OPLUS_FEATURE_CAMERA_COMMON */

	if (client == NULL)
		return -ENODEV;

	pmem = kmalloc(sizeof(*pmem), GFP_KERNEL);
	if (!pmem)
		return -ENOMEM;

	/* each msg contains 4 bytes: addr(u16) + val(u16) */
	sent = 0;
	total = len >> 1;
	plist = list;

	while (sent < total) {

		cnt = total - sent;
		if (cnt > ARRAY_SIZE(pmem->msg))
			cnt = ARRAY_SIZE(pmem->msg);

		pbuf = pmem->buf;
		pmsg = pmem->msg;

		for (i = 0; i < cnt; i++) {

			pbuf[0] = plist[0] >> 8;
			pbuf[1] = plist[0] & 0xff;
			pbuf[2] = plist[1] >> 8;
			pbuf[3] = plist[1] & 0xff;

			if (client->protocol == I2C_PROTOCOL)
				pmsg->addr = addr;

			pmsg->flags = 0;
			pmsg->len = 4;
			pmsg->buf = pbuf;
			plist += 2;
			pbuf += 4;
			pmsg++;
		}

		ret = i3c_i2c_transfer(client, pmem->msg, cnt);

		if (ret < 0){
			dev_info(dev, "[%s]ixc transfer failed (%d)\n", __func__,ret);
			#if defined(OPLUS_FEATURE_CAMERA_COMMON) && defined(CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE)
			if (ctx)
				cam_olc_raise_exception(EXCEP_I2C, payLoadConvert(ctx, payload, "ixc_wr_regs_u16", addr, list[0], list[1]));
			#endif /* OPLUS_FEATURE_CAMERA_COMMON */
			kfree(pmem);
			return -EIO;
		}

		sent += cnt;
	}

	kfree(pmem);

	return 0;
}
