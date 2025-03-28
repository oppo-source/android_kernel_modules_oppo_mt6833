/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2020 MediaTek Inc. */

#ifndef __OPLUS_IMGSENSOR_USER_H__
#define __OPLUS_IMGSENSOR_USER_H__
#include <linux/videodev2.h>

struct oplus_get_camera_sn
{
	int len;
	char data[40];
};

struct oplus_calc_eeprom_info {
	int size;
	__u16 *p_buf;
};

struct oplus_distortion_data {
	int size;
	__u8 *p_buf;
};

struct oplus_eeprom_info_struct
{
	__u16 sensorid_offset;
	__u16 lens_offset;
	__u16 vcm_offset;
	__u16 macPos_offset;
	__u16 infPos_offset;
	__u16 moduleInfo_size;
	__u16 af_size;
	__u16 qrcode_size;
	__u8 afInfo[16];
	__u8 moduleInfo[32];
	__u8 qrcodeInfo[32];
};

#define VIDIOC_MTK_G_CAMERA_SN \
	_IOWR('M', BASE_VIDIOC_PRIVATE + 60, struct oplus_eeprom_info_struct)

#define VIDIOC_MTK_G_STEREO_DATA \
	_IOWR('M', BASE_VIDIOC_PRIVATE + 61, struct oplus_calc_eeprom_info)

#define VIDIOC_MTK_G_OTP_DATA \
	_IOWR('M', BASE_VIDIOC_PRIVATE + 62, struct oplus_calc_eeprom_info)

#define VIDIOC_MTK_G_IS_STREAMING_ENABLE \
	_IOWR('M', BASE_VIDIOC_PRIVATE + 63, __u32)

#define VIDIOC_MTK_G_QCOMPD_DATA \
	_IOWR('M', BASE_VIDIOC_PRIVATE + 64, struct oplus_calc_eeprom_info)

#define VIDIOC_MTK_G_QCOMPD_OFFSET_DATA \
	_IOWR('M', BASE_VIDIOC_PRIVATE + 65, struct oplus_calc_eeprom_info)

#define VIDIOC_MTK_S_CALIBRATION_EEPROM \
	_IOW('M', BASE_VIDIOC_PRIVATE + 120, ACDK_SENSOR_ENGMODE_STEREO_STRUCT)

#define VIDIOC_MTK_S_AON_HE_POWER_UP 0x5000

#define VIDIOC_MTK_S_AON_HE_POWER_DOWN 0x5001

#define VIDIOC_MTK_S_AON_HE_QUERY_INFO 0x5002

#endif /* __OPLUS_IMGSENSOR_USER_H__ */