/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2023 MediaTek Inc. */

#ifndef __OPLUS_ADAPTOR_IOCTL_H__
#define __OPLUS_ADAPTOR_IOCTL_H__
static int g_get_camerasn(struct adaptor_ctx *ctx, void *arg)
{

 	struct oplus_calc_eeprom_info *info = arg;
 	struct workbuf workbuf;
 	int ret;

 	ret = workbuf_get(&workbuf, info->p_buf, info->size, F_READ | F_WRITE);
 	if (ret)
 		return ret;

 	ret = subdrv_call(ctx, feature_control,
 		SENSOR_FEATURE_GET_EEPROM_COMDATA,
 		workbuf.kbuf, &info->size);

	if (ret)
		return ret;

	ret = workbuf_put(&workbuf);
	if (ret)
		return ret;

	return 0;
}

 static int g_get_stereo_data(struct adaptor_ctx *ctx, void *arg)
 {
 	struct oplus_calc_eeprom_info *info = arg;
 	struct workbuf workbuf;
 	int ret;

	ret = workbuf_get(&workbuf, info->p_buf, info->size, F_READ | F_WRITE);
 	if (ret)
 		return ret;

 	ret = subdrv_call(ctx, feature_control,
 		SENSOR_FEATURE_GET_EEPROM_STEREODATA,
 		workbuf.kbuf, &(info->size));
 	if (ret)
 		return ret;

 	ret = workbuf_put(&workbuf);
 	if (ret)
 		return ret;

 	return 0;
 }

static int g_get_otp_data(struct adaptor_ctx *ctx, void *arg)
{
	struct oplus_calc_eeprom_info *info = arg;
	struct workbuf workbuf;
	int ret;

	ret = workbuf_get(&workbuf, info->p_buf, info->size, F_ZERO | F_WRITE);
	if (ret)
		return ret;

	ret = subdrv_call(ctx, feature_control,
		SENSOR_FEATURE_GET_SENSOR_OTP_ALL,
		workbuf.kbuf, &(info->size));
	if (ret)
		return ret;

	ret = workbuf_put(&workbuf);
	if (ret)
		return ret;

	return 0;
}

static int s_calibration_eeprom(struct adaptor_ctx *ctx, void *arg)
{
	struct oplus_calc_eeprom_info *info = arg;
	struct workbuf workbuf;
	int ret, ret2 = info->size;

	ret = workbuf_get(&workbuf, info->p_buf, info->size, F_READ);
	if (ret)
		return ret;

	ret = subdrv_call(ctx, feature_control,
		SENSOR_FEATURE_SET_SENSOR_OTP,
		workbuf.kbuf, &ret2);

	ret = workbuf_put(&workbuf);
	if (ret)
		return ret;

	if (ret2 == -1) {
		return ret2;
	}
	return 0;
}

 static int g_get_qcom_pdaf_data(struct adaptor_ctx *ctx, void *arg)
{
	struct oplus_calc_eeprom_info *info = arg;
	struct workbuf workbuf;
	int ret;

	ret = workbuf_get(&workbuf, info->p_buf, info->size, F_ZERO | F_WRITE);
	if (ret)
		return ret;

	ret = subdrv_call(ctx, feature_control,
		SENSOR_FEATURE_GET_OTP_QCOM_PDAF_DATA,
		workbuf.kbuf, &(info->size));
	if (ret)
		return ret;

	ret = workbuf_put(&workbuf);
	if (ret)
		return ret;

	return 0;
}

static int g_get_qcom_pdaf_offset_data(struct adaptor_ctx *ctx, void *arg)
{
	struct oplus_calc_eeprom_info *info = arg;
	struct workbuf workbuf;
	int ret;

	ret = workbuf_get(&workbuf, info->p_buf, info->size, F_ZERO | F_WRITE);
	if (ret)
		return ret;


	ret = subdrv_call(ctx, feature_control,
		SENSOR_FEATURE_GET_OTP_QCOM_PDAF_OFFSET_DATA,
		workbuf.kbuf, &(info->size));
	if (ret)
		return ret;

	ret = workbuf_put(&workbuf);
	if (ret)
		return ret;

	return 0;
}

static int g_is_streaming_enable(struct adaptor_ctx *ctx, void *arg)
{
	u32 *info = arg;
	union feature_para para;
	u32 len;

	para.u32[0] = 0;
	subdrv_call(ctx, feature_control,
		SENSOR_FEATURE_GET_IS_STREAMING_ENABLE,
		para.u8, &len);

	*info = para.u32[0];
	return 0;
}

static const struct ioctl_entry oplus_ioctl_list[] = {
	/* GET */
	{VIDIOC_MTK_G_CAMERA_SN, g_get_camerasn},
	{VIDIOC_MTK_G_STEREO_DATA, g_get_stereo_data},
	{VIDIOC_MTK_G_OTP_DATA, g_get_otp_data},
	{VIDIOC_MTK_G_IS_STREAMING_ENABLE, g_is_streaming_enable},
	{VIDIOC_MTK_S_CALIBRATION_EEPROM, s_calibration_eeprom},
	{VIDIOC_MTK_G_QCOMPD_DATA, g_get_qcom_pdaf_data},
	{VIDIOC_MTK_G_QCOMPD_OFFSET_DATA, g_get_qcom_pdaf_offset_data},
};

void oplus_adaptor_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg, int* ret)
{
	int i;
	struct adaptor_ctx *ctx = sd_to_ctx(sd);
	if (*ret != -ENOIOCTLCMD) {
		return;
	}
	/* dispatch ioctl request */
	for (i = 0; i < ARRAY_SIZE(oplus_ioctl_list); i++) {
		if (oplus_ioctl_list[i].cmd == cmd) {
			*ret = oplus_ioctl_list[i].func(ctx, arg);
			break;
		}
	}
}

#endif /* __OPLUS_ADAPTOR_IOCTL_H__ */