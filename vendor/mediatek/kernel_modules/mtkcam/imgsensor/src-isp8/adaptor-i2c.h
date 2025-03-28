/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2019 MediaTek Inc. */

#ifndef __ADAPTOR_I2C_H__
#define __ADAPTOR_I2C_H__

#ifdef CONFIG_OPLUS_CAM_EVENT_REPORT_MODULE
#include "oplus/inc/oplus_cam_olc_exception.h"
#include "adaptor.h"
#endif /* OPLUS_FEATURE_CAMERA_COMMON */

struct device *adaptor_ixc_get_dev (struct i3c_i2c_device *client);
void *adaptor_ixc_get_clientdata (struct i3c_i2c_device *client);
int adaptor_ixc_do_daa (struct i3c_i2c_device *client);

int adaptor_i2c_rd_u8(struct i2c_client *i2c_client,
		u16 addr, u16 reg, u8 *val);

int adaptor_i2c_rd_u16(struct i2c_client *i2c_client,
		u16 addr, u16 reg, u16 *val);

int adaptor_i2c_rd_p8(struct i2c_client *i2c_client,
		u16 addr, u16 reg, u8 *p_vals, u32 n_vals);

int adaptor_i2c_wr_u8(struct i2c_client *i2c_client,
		u16 addr, u16 reg, u8 val);

int adaptor_i2c_wr_u16(struct i2c_client *i2c_client,
		u16 addr, u16 reg, u16 val);

int adaptor_i2c_wr_p8(struct i2c_client *i2c_client,
		u16 addr, u16 reg, u8 *p_vals, u32 n_vals);

int adaptor_i2c_wr_p16(struct i2c_client *i2c_client,
		u16 addr, u16 reg, u16 *p_vals, u32 n_vals);

int adaptor_i2c_wr_seq_p8(struct i2c_client *i2c_client,
		u16 addr, u16 reg, u8 *p_vals, u32 n_vals);

int adaptor_i2c_wr_regs_u8(struct i2c_client *i2c_client,
		u16 addr, u16 *list, u32 len);

int adaptor_i2c_wr_regs_u16(struct i2c_client *i2c_client,
		u16 addr, u16 *list, u32 len);

/* for i2c/i3c data transfer*/
int adaptor_ixc_rd_u8(struct i3c_i2c_device *client,
		u16 addr, u16 reg, u8 *val);

int adaptor_ixc_rd_u16(struct i3c_i2c_device *client,
		u16 addr, u16 reg, u16 *val);

int adaptor_ixc_rd_p8(struct i3c_i2c_device *client,
		u16 addr, u16 reg, u8 *p_vals, u32 n_vals);

int adaptor_ixc_wr_u8(struct i3c_i2c_device *client,
		u16 addr, u16 reg, u8 val);

int adaptor_ixc_wr_u16(struct i3c_i2c_device *client,
		u16 addr, u16 reg, u16 val);

int adaptor_ixc_wr_p8(struct i3c_i2c_device *client,
		u16 addr, u16 reg, u8 *p_vals, u32 n_vals);

int adaptor_ixc_wr_p16(struct i3c_i2c_device *client,
		u16 addr, u16 reg, u16 *p_vals, u32 n_vals);

int adaptor_ixc_wr_seq_p8(struct i3c_i2c_device *client,
		u16 addr, u16 reg, u8 *p_vals, u32 n_vals);

int adaptor_ixc_wr_regs_u8(struct i3c_i2c_device *client,
		u16 addr, u16 *list, u32 len);

#ifdef OPLUS_FEATURE_CAMERA_COMMON
int adaptor_ixc_wr_regs_u8_max(struct i3c_i2c_device *client,
		u16 addr, u16 *list, u32 len);
#endif /* OPLUS_FEATURE_CAMERA_COMMON */

int adaptor_ixc_wr_regs_u16(struct i3c_i2c_device *client,
		u16 addr, u16 *list, u32 len);

#endif
