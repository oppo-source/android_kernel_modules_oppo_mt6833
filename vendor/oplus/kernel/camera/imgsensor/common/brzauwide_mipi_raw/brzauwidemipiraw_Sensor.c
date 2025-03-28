// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023 OPLUS. All rights reserved.
 */
/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 brzauwidemipiraw_Sensor.c
 *
 * Project:
 * --------
 *	 ALPS
 *
 * Description:
 * ------------
 *	 Source code of Sensor driver
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/
#include "brzauwidemipiraw_Sensor.h"

#define SENSOR_NAME  SENSOR_DRVNAME_BRZAUWIDE_MIPI_RAW
#define PFX "brzauwide_camera_sensor"
#define LOG_INF(format, args...) pr_err(PFX "[%s] " format, __func__, ##args)

#define BRZAUWIDE_EEPROM_I2C_ADDR	(0xA2)

#define OTP_SIZE	          (0x2000)
#define SENSOR_ID	          (0x56084700)

#define BRZAUWIDE_STEREO_START_ADDR    (0x1260)
#define BRZAUWIDE_AESYNC_START_ADDR    (0x1A00)

#define BRZAUWIDE_UNIQUE_SENSOR_ID_ADDR 0x00
#define BRZAUWIDE_UNIQUE_SENSOR_ID_LENGTH 16
static BYTE brzauwide_unique_id[BRZAUWIDE_UNIQUE_SENSOR_ID_LENGTH] = { 0 };

static struct subdrv_ctx *g_ctx = NULL;
static kal_uint8 otp_data_checksum[OTP_SIZE] = {0};
static void set_group_hold(void *arg, u8 en);
static u16 get_gain2reg(u32 gain);
static int brzauwide_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int brzauwide_check_sensor_id(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int brzauwide_get_eeprom_common_data(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int brzauwide_set_eeprom_calibration(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int brzauwide_get_eeprom_calibration(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int brzauwide_get_otp_checksum_data(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int brzauwide_streaming_suspend(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int brzauwide_streaming_resume(struct subdrv_ctx *ctx, u8 *para, u32 *len);

static int get_imgsensor_id(struct subdrv_ctx *ctx, u32 *sensor_id);
static int open(struct subdrv_ctx *ctx);
static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id);

static int brzauwide_set_gain_convert(struct subdrv_ctx *ctx, u32 gain);
static int brzauwide_set_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int brzauwide_set_shutter_convert(struct subdrv_ctx *ctx, u64 shutter);
static int brzauwide_set_shutter(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int brzauwide_set_shutter_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int brzauwide_set_shutter_frame_length_convert(struct subdrv_ctx *ctx, u64 shutter, u32 frame_length);
static int brzauwide_set_max_framerate_by_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len);

static bool read_cmos_eeprom_p8(struct subdrv_ctx *ctx, kal_uint16 addr, BYTE *data, int size);
static int brzauwide_set_register(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int brzauwide_get_register(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int vsync_notify(struct subdrv_ctx *ctx,	unsigned int sof_cnt);
static int brzauwide_extend_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int brzauwide_set_multi_shutter_frame_length_ctrl(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int brzauwide_set_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static void read_unique_sensorid(struct subdrv_ctx *ctx);

static int brzauwide_common_control(struct subdrv_ctx *ctx,
	enum SENSOR_SCENARIO_ID_ENUM scenario_id,
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data);

static bool g_id_from_dts_flag = false;
static void get_imgsensor_id_from_dts(struct subdrv_ctx *ctx, u32 *sensor_id);


static struct eeprom_map_info brzauwide_eeprom_info[] = {
	{ EEPROM_META_MODULE_ID, 0x0000, 0x0010, 0x0011, 2, true },
	{ EEPROM_META_SENSOR_ID, 0x0006, 0x0010, 0x001l, 2, true },
	{ EEPROM_META_LENS_ID, 0x0008, 0x0010, 0x0011, 2, true },
	{ EEPROM_META_VCM_ID, 0x000A, 0x0010, 0x0011, 2, true },
	{ EEPROM_META_MIRROR_FLIP, 0x000E, 0x0010, 0x0011, 1, true },
	{ EEPROM_META_MODULE_SN, 0x00B0, 0x00C7, 0x00C8, 23, true },
	{ EEPROM_META_AF_CODE, 0x0092, 0x0098, 0x0099, 6, true },
	{ EEPROM_META_AF_FLAG, 0x0098, 0x0098, 0x0099, 1, true },
	{ EEPROM_META_STEREO_DATA, 0x0000, 0x0000, 0x0000, 0, false },
	{ EEPROM_META_STEREO_MW_MAIN_DATA, 0x1260, 0x0000, 0x0000, CALI_DATA_SLAVE_LENGTH, false },
	{ EEPROM_META_STEREO_MT_MAIN_DATA, 0, 0, 0, 0, false },
	{ EEPROM_META_STEREO_MT_MAIN_DATA_105CM, 0, 0, 0, 0, false },
	{ EEPROM_META_DISTORTION_DATA, 0, 0, 0, 0, false },
};

static struct subdrv_feature_control feature_control_list[] = {
	{SENSOR_FEATURE_SET_TEST_PATTERN, brzauwide_set_test_pattern},
	{SENSOR_FEATURE_CHECK_SENSOR_ID, brzauwide_check_sensor_id},
	{SENSOR_FEATURE_GET_EEPROM_COMDATA, brzauwide_get_eeprom_common_data},
	{SENSOR_FEATURE_SET_SENSOR_OTP, brzauwide_set_eeprom_calibration},
	{SENSOR_FEATURE_GET_EEPROM_STEREODATA, brzauwide_get_eeprom_calibration},
	{SENSOR_FEATURE_GET_SENSOR_OTP_ALL, brzauwide_get_otp_checksum_data},
	{SENSOR_FEATURE_SET_STREAMING_SUSPEND, brzauwide_streaming_suspend},
	{SENSOR_FEATURE_SET_STREAMING_RESUME, brzauwide_streaming_resume},
	{SENSOR_FEATURE_SET_GAIN, brzauwide_set_gain},
	{SENSOR_FEATURE_SET_ESHUTTER, brzauwide_set_shutter},
	{SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME, brzauwide_set_shutter_frame_length},
	{SENSOR_FEATURE_SET_SEAMLESS_EXTEND_FRAME_LENGTH, brzauwide_extend_frame_length},
	{SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO, brzauwide_set_max_framerate_by_scenario},
	{SENSOR_FEATURE_SET_MULTI_SHUTTER_FRAME_TIME, brzauwide_set_multi_shutter_frame_length_ctrl},
	{SENSOR_FEATURE_SET_REGISTER, brzauwide_set_register},
	{SENSOR_FEATURE_GET_REGISTER, brzauwide_get_register},
	{SENSOR_FEATURE_SET_FRAMELENGTH, brzauwide_set_frame_length},
};


static struct eeprom_info_struct eeprom_info[] = {
	{
		.header_id = 0x0065009a,
		.addr_header_id = 0x00000006,
		.i2c_write_id = 0xA2,
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_prev_cap[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 3264,
			.vsize = 2448,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	}
};

static struct mtk_mbus_frame_desc_entry frame_desc_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 3264,
			.vsize = 1840,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	}
};

static struct mtk_mbus_frame_desc_entry frame_desc_hs[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 1632,
			.vsize = 1224,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	}
};

static struct mtk_mbus_frame_desc_entry frame_desc_slim[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 3264,
			.vsize = 1840,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	}
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus1[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 3264,
			.vsize = 2448,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	}
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus2[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 1632,
			.vsize = 1224,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	}
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus3[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 2560,
			.vsize = 1920,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	}
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus4[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 2304,
			.vsize = 1728,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	}
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus5[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 1664,
			.vsize = 1248,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	}
};

static struct subdrv_mode_struct mode_struct[] = {
	{ /* 3264x2448 @30FPS  */
		.frame_desc = frame_desc_prev_cap,
		.num_entries = ARRAY_SIZE(frame_desc_prev_cap),
		.mode_setting_table = brzauwide_preview_capture_setting,
		.mode_setting_len = ARRAY_SIZE(brzauwide_preview_capture_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 36000000,
		.linelength = 460,
		.framelength = 2608,
		.max_framerate = 300,
		.mipi_pixel_rate = 288000000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 4,
		.imgsensor_winsize_info = {
			.full_w = 3280,
			.full_h = 2464,
			.x0_offset = 8,
			.y0_offset = 8,
			.w0_size = 3264,
			.h0_size = 2448,
			.scale_w = 3264,
			.scale_h = 2448,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 3264,
			.h1_size = 2448,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 3264,
			.h2_tg_size = 2448,
		},
		.ae_binning_ratio = 1,
		.delay_frame = 2,
		.csi_param = {
			.dphy_trail = 0x34,
		},
/*		.sensor_setting_info = {
			.sensor_scenario_usage = NORMAL_MASK,
			.equivalent_fps = 30,
		}, */
	},
	{ /* 3264x2448 @30FPS  */
		.frame_desc = frame_desc_prev_cap,
		.num_entries = ARRAY_SIZE(frame_desc_prev_cap),
		.mode_setting_table = brzauwide_preview_capture_setting,
		.mode_setting_len = ARRAY_SIZE(brzauwide_preview_capture_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 36000000,
		.linelength = 460,
		.framelength = 2608,
		.max_framerate = 300,
		.mipi_pixel_rate = 288000000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 4,
		.imgsensor_winsize_info = {
			.full_w = 3280,
			.full_h = 2464,
			.x0_offset = 8,
			.y0_offset = 8,
			.w0_size = 3264,
			.h0_size = 2448,
			.scale_w = 3264,
			.scale_h = 2448,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 3264,
			.h1_size = 2448,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 3264,
			.h2_tg_size = 2448,
		},
		.ae_binning_ratio = 1,
		.delay_frame = 2,
		.csi_param = {},
/*    		.sensor_setting_info = {
			.sensor_scenario_usage = UNUSE_MASK,
			.equivalent_fps = 0,
		},*/
	},

	{
		.frame_desc = frame_desc_vid,
		.num_entries = ARRAY_SIZE(frame_desc_vid),
		.mode_setting_table = brzauwide_normal_video_setting,
		.mode_setting_len = ARRAY_SIZE(brzauwide_normal_video_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 36000000,
		.linelength = 460,
		.framelength = 2608,
		.max_framerate = 300,
		.mipi_pixel_rate = 288000000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 4,
		.imgsensor_winsize_info = {
			.full_w = 3280,
			.full_h = 2464,
			.x0_offset = 8,
			.y0_offset = 312,
			.w0_size = 3264,
			.h0_size = 1840,
			.scale_w = 3264,
			.scale_h = 1840,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 3264,
			.h1_size = 1840,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 3264,
			.h2_tg_size = 1840,
		},
		.ae_binning_ratio = 1,
		.delay_frame = 2,
		.csi_param = {},
/*     		.sensor_setting_info = {
			.sensor_scenario_usage = NORMAL_MASK,
			.equivalent_fps = 30,
		},*/
	},

	{
		.frame_desc = frame_desc_hs,
		.num_entries = ARRAY_SIZE(frame_desc_hs),
		.mode_setting_table = brzauwide_hs_video_setting,
		.mode_setting_len = ARRAY_SIZE(brzauwide_hs_video_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 36000000,
		.linelength = 478,
		.framelength = 1252,
		.max_framerate = 600,
		.mipi_pixel_rate = 144000000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 4,
		.imgsensor_winsize_info = {
			.full_w = 3280,
			.full_h = 2464,
			.x0_offset = 8,
			.y0_offset = 8,
			.w0_size = 3264,
			.h0_size = 2448,
			.scale_w = 1632,
			.scale_h = 1224,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 1632,
			.h1_size = 1224,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1632,
			.h2_tg_size = 1224,
		},
		.ae_binning_ratio = 1,
		.delay_frame = 2,
		.csi_param = {},
/*		.sensor_setting_info = {
			.sensor_scenario_usage = UNUSE_MASK,
			.equivalent_fps = 60,
		}, */
	},

	{
		.frame_desc = frame_desc_slim,
		.num_entries = ARRAY_SIZE(frame_desc_slim),
		.mode_setting_table = brzauwide_slim_video_setting,
		.mode_setting_len = ARRAY_SIZE(brzauwide_slim_video_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 288000000,
		.linelength = 3672,
		.framelength = 2612,
		.max_framerate = 300,
		.mipi_pixel_rate = 288000000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 4,
		.imgsensor_winsize_info = {
			.full_w = 3280,
			.full_h = 2464,
			.x0_offset = 8,
			.y0_offset = 312,
			.w0_size = 3264,
			.h0_size = 1840,
			.scale_w = 3264,
			.scale_h = 1840,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 3264,
			.h1_size = 1840,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 3264,
			.h2_tg_size = 1840,
		},
		.ae_binning_ratio = 1,
		.delay_frame = 2,
		.csi_param = {},
/*		.sensor_setting_info = {
			.sensor_scenario_usage = UNUSE_MASK,
			.equivalent_fps = 0,
		}, */
	},

	{
		.frame_desc = frame_desc_cus1,
		.num_entries = ARRAY_SIZE(frame_desc_cus1),
		.mode_setting_table = brzauwide_custom1_setting,
		.mode_setting_len = ARRAY_SIZE(brzauwide_custom1_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 36000000,
		.linelength = 460,
		.framelength = 3260,
		.max_framerate = 240,
		.mipi_pixel_rate = 288000000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 4,
		.imgsensor_winsize_info = {
			.full_w = 3280,
			.full_h = 2464,
			.x0_offset = 8,
			.y0_offset = 8,
			.w0_size = 3264,
			.h0_size = 2448,
			.scale_w = 3264,
			.scale_h = 2448,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 3264,
			.h1_size = 2448,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 3264,
			.h2_tg_size = 2448,
		},
		.ae_binning_ratio = 1,
		.delay_frame = 2,
		.csi_param = {},
/*		.sensor_setting_info = {
			.sensor_scenario_usage = UNUSE_MASK,
			.equivalent_fps = 0,
		},*/
	},

	{
		.frame_desc = frame_desc_cus2,
		.num_entries = ARRAY_SIZE(frame_desc_cus2),
		.mode_setting_table = brzauwide_custom2_setting,
		.mode_setting_len = ARRAY_SIZE(brzauwide_custom2_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 36000000,
		.linelength = 478,
		.framelength = 3136,
		.max_framerate = 240,
		.mipi_pixel_rate = 144000000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 4,
		.imgsensor_winsize_info = {
			.full_w = 3280,
			.full_h = 2464,
			.x0_offset = 8,
			.y0_offset = 8,
			.w0_size = 3264,
			.h0_size = 2448,
			.scale_w = 1632,
			.scale_h = 1224,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 1632,
			.h1_size = 1224,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1632,
			.h2_tg_size = 1224,
		},
		.ae_binning_ratio = 1,
		.delay_frame = 2,
		.csi_param = {},
/*		.sensor_setting_info = {
			.sensor_scenario_usage = UNUSE_MASK,
			.equivalent_fps = 0,
		}, */
	},

	{
		.frame_desc = frame_desc_cus3,
		.num_entries = ARRAY_SIZE(frame_desc_cus3),
		.mode_setting_table = brzauwide_custom3_setting,
		.mode_setting_len = ARRAY_SIZE(brzauwide_custom3_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 36000000,
		.linelength = 460,
		.framelength = 3260,
		.max_framerate = 240,
		.mipi_pixel_rate = 288000000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 4,
		.imgsensor_winsize_info = {
			.full_w = 3280,
			.full_h = 2464,
			.x0_offset = 360,
			.y0_offset = 272,
			.w0_size = 2560,
			.h0_size = 1920,
			.scale_w = 2560,
			.scale_h = 1920,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 2560,
			.h1_size = 1920,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 2560,
			.h2_tg_size = 1920,
		},
		.ae_binning_ratio = 1,
		.delay_frame = 2,
		.csi_param = {},
/*		.sensor_setting_info = {
			.sensor_scenario_usage = NORMAL_MASK,
			.equivalent_fps = 24,
		}, */
	},

	{
		.frame_desc = frame_desc_cus4,
		.num_entries = ARRAY_SIZE(frame_desc_cus4),
		.mode_setting_table = brzauwide_custom4_setting,
		.mode_setting_len = ARRAY_SIZE(brzauwide_custom4_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 36000000,
		.linelength = 460,
		.framelength = 3260,
		.max_framerate = 240,
		.mipi_pixel_rate = 288000000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 4,
		.imgsensor_winsize_info = {
			.full_w = 3280,
			.full_h = 2464,
			.x0_offset = 488,
			.y0_offset = 368,
			.w0_size = 2304,
			.h0_size = 1728,
			.scale_w = 2304,
			.scale_h = 1728,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 2304,
			.h1_size = 1728,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 2304,
			.h2_tg_size = 1728,
		},
		.ae_binning_ratio = 1,
		.delay_frame = 2,
		.csi_param = {},
/*		.sensor_setting_info = {
			.sensor_scenario_usage = NORMAL_MASK,
			.equivalent_fps = 24,
		},*/
	},

	{
		.frame_desc = frame_desc_cus5,
		.num_entries = ARRAY_SIZE(frame_desc_cus5),
		.mode_setting_table = brzauwide_custom5_setting,
		.mode_setting_len = ARRAY_SIZE(brzauwide_custom5_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 36000000,
		.linelength = 460,
		.framelength = 3260,
		.max_framerate = 240,
		.mipi_pixel_rate = 288000000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 4,
		.imgsensor_winsize_info = {
			.full_w = 3280,
			.full_h = 2464,
			.x0_offset = 808,
			.y0_offset = 608,
			.w0_size = 1664,
			.h0_size = 1248,
			.scale_w = 1664,
			.scale_h = 1248,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 1664,
			.h1_size = 1248,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1664,
			.h2_tg_size = 1248,
		},
		.ae_binning_ratio = 1,
		.delay_frame = 2,
		.csi_param = {
			.dphy_trail = 75,
		},
/*		.sensor_setting_info = {
			.sensor_scenario_usage = NORMAL_MASK,
			.equivalent_fps = 24,
		}, */
	},
};

static struct subdrv_static_ctx static_ctx = {
	.sensor_id = BRZAUWIDE_SENSOR_ID,
	.reg_addr_sensor_id = {0x0000}, /*page0, 0x00, 0x01, 0x02, 0x03*/
	.i2c_addr_table = {0x6c, 0xFF},
	.i2c_burst_write_support = TRUE,
	.i2c_transfer_data_type = I2C_DT_ADDR_8_DATA_8,
	.eeprom_info = eeprom_info,
	.eeprom_num = ARRAY_SIZE(eeprom_info),
	.resolution = {3264, 2448},
	.mirror = IMAGE_NORMAL,

	.mclk = 24,
	.isp_driving_current = ISP_DRIVING_4MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_lane_num = SENSOR_MIPI_2_LANE,
	.ob_pedestal = 0x40,

	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,
	.ana_gain_def = BASEGAIN * 1,
	.ana_gain_min = BASEGAIN * 1,
	.ana_gain_max = BASEGAIN * 15.5,
	.ana_gain_type = 1, /*0-SONY; 1-OV; 2 - SUMSUN; 3 -HYNIX; 4 -GC*/
	.ana_gain_step = 1,
/*	.ana_gain_table = brzauwide_ana_gain_table,
	.ana_gain_table_size = sizeof(brzauwide_ana_gain_table),*/
	.tuning_iso_base = 100,
	.exposure_def = 0x3D0,
	.exposure_min = 4,
	.exposure_max =  0x1FFFFE,      /* (3ffffc / 2) */
	.exposure_step = 4,
	.exposure_margin = 20,

	.frame_length_max = 0x1FFFFE,   /* (3ffffc / 2) */
	.ae_effective_frame = 2,
	.frame_time_delay_frame = 2,
	.start_exposure_offset = 934000,
	.pdaf_type = PDAF_SUPPORT_NA,
	.g_gain2reg = get_gain2reg,
	.s_gph = set_group_hold,

/*	.reg_addr_stream = 0x0100,
	.reg_addr_mirror_flip = 0x0101,
	.reg_addr_exposure = {
			{0x0102, 0x0103},
	},*/
	.long_exposure_support = FALSE,
	.reg_addr_exposure_lshift = 0,
/*	.reg_addr_ana_gain = {
			{0x0204, 0x0205},
	},
	.reg_addr_frame_length = {0x0340, 0x0341},*/
	.reg_addr_auto_extend = 0,
	.reg_addr_frame_count = PARAM_UNDEFINED,
/*	.reg_addr_fast_mode = 0x3010,*/

	.init_setting_table = brzauwide_init_setting,
	.init_setting_len = ARRAY_SIZE(brzauwide_init_setting),
	.mode = mode_struct,
	.sensor_mode_num = ARRAY_SIZE(mode_struct),
	.list = feature_control_list,
	.list_len = ARRAY_SIZE(feature_control_list),

	.checksum_value = 0xD1EFF68B,
};

static struct subdrv_ops ops = {
	.get_id = get_imgsensor_id,
	.init_ctx = init_ctx,
	.open = open,
	.get_info = common_get_info,
	.get_resolution = common_get_resolution,
	.control = brzauwide_common_control,
	.feature_control = common_feature_control,
	.close = common_close,
	.get_frame_desc = common_get_frame_desc,
	.get_temp = common_get_temp,
	.get_csi_param = common_get_csi_param,
	.vsync_notify = vsync_notify,
	.update_sof_cnt = common_update_sof_cnt,
};

static struct subdrv_pw_seq_entry pw_seq[] = {
	{HW_ID_RST, {0}, 1000},
	{HW_ID_DOVDD, {1800000, 1800000}, 5000},
	{HW_ID_AVDD, {2800000, 2800000}, 9000},
	{HW_ID_DVDD, {1200000, 1200000}, 5000},
	{HW_ID_MCLK, {24}, 0},
	{HW_ID_RST, {1}, 1000},
	{HW_ID_MCLK_DRIVING_CURRENT, {4}, 8000},
};

struct subdrv_entry brzauwide_mipi_raw_entry = {
	.name = SENSOR_NAME,
	.id = BRZAUWIDE_SENSOR_ID,
	.pw_seq = pw_seq,
	.pw_seq_cnt = ARRAY_SIZE(pw_seq),
	.ops = &ops,
};

/* FUNCTION */

static void streaming_ctrl(struct subdrv_ctx *ctx, bool enable)
{
	check_current_scenario_id_bound(ctx);

	if (enable) {
		subdrv_i2c_wr_u8_u8(ctx, 0xfd, 0x01);
		subdrv_i2c_wr_u8_u8(ctx, 0x01, 0x03);
		subdrv_i2c_wr_u8_u8(ctx, 0xfd, 0x00);
		subdrv_i2c_wr_u8_u8(ctx, 0x20, 0x0f);
		subdrv_i2c_wr_u8_u8(ctx, 0xe7, 0x03);
		subdrv_i2c_wr_u8_u8(ctx, 0xe7, 0x00);
		subdrv_i2c_wr_u8_u8(ctx, 0xa0, 0x01);
	} else {
		subdrv_i2c_wr_u8_u8(ctx, 0xfd, 0x00);
		subdrv_i2c_wr_u8_u8(ctx, 0xa0, 0x00);
		subdrv_i2c_wr_u8_u8(ctx, 0x20, 0x0b);
	}

	ctx->is_streaming = enable;
	DRV_LOG(ctx, "X! enable:%u\n", enable);
}

static int brzauwide_streaming_resume(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
		DRV_LOG(ctx, "SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%u\n", *(u32 *)para);
		if (*(u32 *)para)
			brzauwide_set_shutter_convert(ctx, *(u32 *)para);
		streaming_ctrl(ctx, true);

		return 0;
}

static int brzauwide_streaming_suspend(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
		DRV_LOG(ctx, "streaming control para:%d\n", *para);
		streaming_ctrl(ctx, false);

		return 0;
}
static unsigned int read_brzauwide_eeprom_info(struct subdrv_ctx *ctx, kal_uint16 meta_id,
	BYTE *data, int size)
{
	kal_uint16 addr;
	int readsize;

	if (meta_id != brzauwide_eeprom_info[meta_id].meta)
		return -1;

	if (size != brzauwide_eeprom_info[meta_id].size)
		return -1;

	addr = brzauwide_eeprom_info[meta_id].start;
	readsize = brzauwide_eeprom_info[meta_id].size;

	if(!read_cmos_eeprom_p8(ctx, addr, data, readsize)) {
		DRV_LOGE(ctx, "read meta_id(%d) failed", meta_id);
	}

	return 0;
}

static struct eeprom_addr_table_struct oplus_eeprom_addr_table = {
	.i2c_read_id = 0xA3,
	.i2c_write_id = 0xA2,

	.addr_modinfo = 0x0000,
	.addr_sensorid = 0x0006,
	.addr_lens = 0x0008,
	.addr_vcm = 0x000A,
	.addr_modinfoflag = 0x0010,

	.addr_af = 0x0092,
	.addr_afmacro = 0x0092,
	.addr_afinf = 0x0094,
	.addr_afflag = 0x0098,

	.addr_qrcode = 0x00B0,
	.addr_qrcodeflag = 0x00C7,
};

static struct oplus_eeprom_info_struct  oplus_eeprom_info = {0};

static int brzauwide_get_eeprom_common_data(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	struct oplus_eeprom_info_struct* infoPtr;
	memcpy(para, (u8*)(&oplus_eeprom_info), sizeof(oplus_eeprom_info));
	infoPtr = (struct oplus_eeprom_info_struct*)(para);
	*len = sizeof(oplus_eeprom_info);
	infoPtr->afInfo[0] = (kal_uint8)((infoPtr->afInfo[1] << 7) | (infoPtr->afInfo[0] >> 1));
	infoPtr->afInfo[1] = (kal_uint8)(infoPtr->afInfo[1] >> 1);
	infoPtr->afInfo[2] = (kal_uint8)((infoPtr->afInfo[3] << 7) | (infoPtr->afInfo[2] >> 1));
	infoPtr->afInfo[3] = (kal_uint8)(infoPtr->afInfo[3] >> 1);
	infoPtr->afInfo[4] = (kal_uint8)((infoPtr->afInfo[5] << 7) | (infoPtr->afInfo[4] >> 1));
	infoPtr->afInfo[5] = (kal_uint8)(infoPtr->afInfo[5] >> 1);

	return 0;
}

static kal_uint16 read_cmos_eeprom_8(struct subdrv_ctx *ctx, kal_uint16 addr)
{
	kal_uint16 get_byte = 0;

	adaptor_i2c_rd_u8(ctx->i2c_client, BRZAUWIDE_EEPROM_I2C_ADDR >> 1, addr, (u8 *)&get_byte);
	return get_byte;
}

static kal_int32 table_write_eeprom_30Bytes(struct subdrv_ctx *ctx,
		kal_uint16 addr, kal_uint8 *para, kal_uint32 len)
{
	kal_int32 ret = ERROR_NONE;
	ret = adaptor_i2c_wr_p8(ctx->i2c_client, BRZAUWIDE_EEPROM_I2C_ADDR >> 1,
			addr, para, len);

	return ret;
}

static kal_int32 write_eeprom_protect(struct subdrv_ctx *ctx, kal_uint16 enable)
{
	kal_int32 ret = ERROR_NONE;
	kal_uint16 reg = 0xE000;
	if (enable) {
		adaptor_i2c_wr_u8(ctx->i2c_client, BRZAUWIDE_EEPROM_I2C_ADDR >> 1, reg, 0x03);
	}
	else {
		adaptor_i2c_wr_u8(ctx->i2c_client, BRZAUWIDE_EEPROM_I2C_ADDR >> 1, reg, 0x02);
	}

	return ret;
}

static kal_uint16 get_64align_addr(kal_uint16 data_base) {
	kal_uint16 multiple = 0;
	kal_uint16 surplus = 0;
	kal_uint16 addr_64align = 0;

	multiple = data_base / 64;
	surplus = data_base % 64;
	if(surplus) {
		addr_64align = (multiple + 1) * 64;
	} else {
		addr_64align = multiple * 64;
	}
	return addr_64align;
}

static kal_int32 eeprom_table_write(struct subdrv_ctx *ctx, kal_uint16 data_base, kal_uint8 *pData, kal_uint16 data_length) {
	kal_uint16 idx;
	kal_uint16 idy;
	kal_int32 ret = ERROR_NONE;
	UINT32 i = 0;

	idx = data_length/WRITE_DATA_MAX_LENGTH;
	idy = data_length%WRITE_DATA_MAX_LENGTH;

	LOG_INF("[test] data_base(0x%x) data_length(%d) idx(%d) idy(%d)\n", data_base, data_length, idx, idy);

	for (i = 0; i < idx; i++) {
		ret = table_write_eeprom_30Bytes(ctx, (data_base + WRITE_DATA_MAX_LENGTH * i),
				&pData[WRITE_DATA_MAX_LENGTH*i], WRITE_DATA_MAX_LENGTH);
		if (ret != ERROR_NONE) {
			LOG_INF("write_eeprom error: i=%d\n", i);
			return -1;
		}
		msleep(6);
	}

	msleep(6);
	if(idy) {
		ret = table_write_eeprom_30Bytes(ctx, (data_base + WRITE_DATA_MAX_LENGTH*idx),
				&pData[WRITE_DATA_MAX_LENGTH*idx], idy);
		if (ret != ERROR_NONE) {
			LOG_INF("write_eeprom error: idx= %d idy= %d\n", idx, idy);
			return -1;
		}
	}
	return 0;
}

static kal_int32 eeprom_64align_write(struct subdrv_ctx *ctx, kal_uint16 data_base, kal_uint8 *pData, kal_uint16 data_length) {
	kal_uint16 addr_64align = 0;
	kal_uint16 part1_length = 0;
	kal_uint16 part2_length = 0;
	kal_int32 ret = ERROR_NONE;

	addr_64align = get_64align_addr(data_base);

	part1_length = addr_64align - data_base;
	if(part1_length > data_length) {
		part1_length = data_length;
	}
	part2_length = data_length - part1_length;

	write_eeprom_protect(ctx, 0);
	msleep(6);

	if (part1_length) {
		ret = eeprom_table_write(ctx, data_base, pData, part1_length);
		if (ret == -1) {
			/* open write protect */
			write_eeprom_protect(ctx, 1);
			LOG_INF("write_eeprom error part1\n");
			msleep(6);
			return -1;
		}
	}

	msleep(6);
	if (part2_length) {
		ret = eeprom_table_write(ctx, addr_64align, pData + part1_length, part2_length);
		if (ret == -1) {
			/* open write protect */
			write_eeprom_protect(ctx, 1);
			LOG_INF("write_eeprom error part2\n");
			msleep(6);
			return -1;
		}
	}
	msleep(6);
	write_eeprom_protect(ctx, 1);
	msleep(6);

	return 0;
}

static kal_int32 write_Module_data(struct subdrv_ctx *ctx,
			ACDK_SENSOR_ENGMODE_STEREO_STRUCT * pStereodata)
{
	kal_int32  ret = ERROR_NONE;
	kal_uint16 data_base, data_length;
	kal_uint8 *pData;

	if(pStereodata != NULL) {
		LOG_INF("SET_SENSOR_OTP: 0x%x %d 0x%x %d\n",
					   pStereodata->uSensorId,
					   pStereodata->uDeviceId,
					   pStereodata->baseAddr,
					   pStereodata->dataLength);

		data_base = pStereodata->baseAddr;
		data_length = pStereodata->dataLength;
		pData = pStereodata->uData;
		if ((pStereodata->uSensorId == BRZAUWIDE_SENSOR_ID)
			&& (data_length == CALI_DATA_SLAVE_LENGTH)
			&& (data_base == BRZAUWIDE_STEREO_START_ADDR)) {
			LOG_INF("Write: %x %x %x %x\n", pData[0], pData[39], pData[40], pData[1556]);

			eeprom_64align_write(ctx, data_base, pData, data_length);

			LOG_INF("com_0:0x%x\n", read_cmos_eeprom_8(ctx, data_base));
			LOG_INF("com_39:0x%x\n", read_cmos_eeprom_8(ctx, data_base+39));
			LOG_INF("innal_40:0x%x\n", read_cmos_eeprom_8(ctx, data_base+40));
			LOG_INF("innal_1556:0x%x\n", read_cmos_eeprom_8(ctx, data_base+1556));
			LOG_INF("write_Module_data Write end\n");

		} else if ((pStereodata->uSensorId == BRZAUWIDE_SENSOR_ID)
			&& (data_length < AESYNC_DATA_LENGTH_TOTAL)
			&& (data_base == BRZAUWIDE_AESYNC_START_ADDR)) {
			LOG_INF("write main aesync: %x %x %x %x %x %x %x %x\n", pData[0], pData[1],
				pData[2], pData[3], pData[4], pData[5], pData[6], pData[7]);

			eeprom_64align_write(ctx, data_base, pData, data_length);

			LOG_INF("readback main aesync: %x %x %x %x %x %x %x %x\n",
					read_cmos_eeprom_8(ctx, BRZAUWIDE_AESYNC_START_ADDR),
					read_cmos_eeprom_8(ctx, BRZAUWIDE_AESYNC_START_ADDR+1),
					read_cmos_eeprom_8(ctx, BRZAUWIDE_AESYNC_START_ADDR+2),
					read_cmos_eeprom_8(ctx, BRZAUWIDE_AESYNC_START_ADDR+3),
					read_cmos_eeprom_8(ctx, BRZAUWIDE_AESYNC_START_ADDR+4),
					read_cmos_eeprom_8(ctx, BRZAUWIDE_AESYNC_START_ADDR+5),
					read_cmos_eeprom_8(ctx, BRZAUWIDE_AESYNC_START_ADDR+6),
					read_cmos_eeprom_8(ctx, BRZAUWIDE_AESYNC_START_ADDR+7));
			LOG_INF("AESync write_Module_data Write end\n");
		} else {
			LOG_INF("Invalid Sensor id:0x%x write eeprom\n", pStereodata->uSensorId);
			return -1;
		}
	} else {
		LOG_INF("omegas2 write_Module_data pStereodata is null\n");
		return -1;
	}
	return ret;
}

static int brzauwide_set_eeprom_calibration(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	int ret = ERROR_NONE;
	ret = write_Module_data(ctx, (ACDK_SENSOR_ENGMODE_STEREO_STRUCT *)(para));
	if (ret != ERROR_NONE) {
		*len = (u32)-1; /*write eeprom failed*/
		LOG_INF("ret=%d\n", ret);
	}
	return 0;
}

static int brzauwide_get_eeprom_calibration(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	UINT16 *feature_data_16 = (UINT16 *) para;
	UINT32 *feature_return_para_32 = (UINT32 *) para;
	if(*len > CALI_DATA_SLAVE_LENGTH)
		*len = CALI_DATA_SLAVE_LENGTH;
	LOG_INF("feature_data mode:%d  lens:%d", *feature_data_16, *len);
	read_brzauwide_eeprom_info(ctx, EEPROM_META_STEREO_MW_MAIN_DATA,
			(BYTE *)feature_return_para_32, *len);
	return 0;
}

static bool read_cmos_eeprom_p8(struct subdrv_ctx *ctx, kal_uint16 addr,
					BYTE *data, int size)
{
	if (adaptor_i2c_rd_p8(ctx->i2c_client, BRZAUWIDE_EEPROM_I2C_ADDR >> 1,
			addr, data, size) < 0) {
		return false;
	}
	return true;
}

static void read_otp_info(struct subdrv_ctx *ctx)
{
	DRV_LOGE(ctx, "brzauwide read_otp_info begin\n");
	read_cmos_eeprom_p8(ctx, 0, otp_data_checksum, OTP_SIZE);
	DRV_LOGE(ctx, "brzauwide read_otp_info end\n");
}

static int brzauwide_get_otp_checksum_data(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u32 *feature_return_para_32 = (u32 *)para;
	DRV_LOGE(ctx, "get otp data");
	if (otp_data_checksum[0] == 0) {
		read_otp_info(ctx);
	} else {
		DRV_LOG(ctx, "otp data has already read");
	}
	memcpy(feature_return_para_32, (UINT32 *)otp_data_checksum, sizeof(otp_data_checksum));
	*len = sizeof(otp_data_checksum);
	return 0;
}

static int brzauwide_check_sensor_id(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	get_imgsensor_id(ctx, (u32 *)para);
	return 0;
}

static u16 brzauwide_unique_sensorid[] = {
	0xfd, 0x00,
	0x20, 0x0f,
	0xe7, 0x03,
	0xe7, 0x00,
	0xfd, 0x03,
	0x84, 0x40,
	0x88, 0x00,
	0x89, 0x00,
	0x8a, 0x00,
	0x8b, 0x0f,
	0x81, 0x01,
	0xfd, 0x08,
};

static void read_unique_sensorid(struct subdrv_ctx *ctx)
{
	kal_uint8 i = 0;

	LOG_INF("read wide sensor unique sensorid");
	subdrv_i2c_wr_regs_u8_u8(ctx, brzauwide_unique_sensorid, ARRAY_SIZE(brzauwide_unique_sensorid));
	msleep(50);
	subdrv_i2c_wr_u8_u8(ctx, 0xfd, 0x08);
	for (i = 0; i< BRZAUWIDE_UNIQUE_SENSOR_ID_LENGTH; i++) {
		brzauwide_unique_id[i] =  subdrv_i2c_rd_u8_u8(ctx, BRZAUWIDE_UNIQUE_SENSOR_ID_ADDR + i);
		pr_err("%s unique_id[%d] = 0x%x", __func__, i, brzauwide_unique_id[i]);
	}
}

static kal_uint32 return_sensor_id(struct subdrv_ctx *ctx)
{
	subdrv_i2c_wr_u8_u8(ctx, 0xfd, 0x00);

	return ((subdrv_i2c_rd_u8_u8(ctx, 0x00) << 24) | (subdrv_i2c_rd_u8_u8(ctx, 0x01) << 16)
		  | (subdrv_i2c_rd_u8_u8(ctx, 0x02) << 8)  |  subdrv_i2c_rd_u8_u8(ctx, 0x03));
}

static int get_imgsensor_id(struct subdrv_ctx *ctx, u32 *sensor_id)
{
	u8 i = 0;
	u8 retry = 2;
	static bool first_read = KAL_TRUE;

	while (ctx->s_ctx.i2c_addr_table[i] != 0xFF) {
		ctx->i2c_write_id = ctx->s_ctx.i2c_addr_table[i];
		do {
			*sensor_id = return_sensor_id(ctx);
			DRV_LOG(ctx, "i2c_write_id(0x%x) sensor_id(0x%x/0x%x)\n",
				ctx->i2c_write_id, *sensor_id, ctx->s_ctx.sensor_id);
			if (*sensor_id == SENSOR_ID) {
				get_imgsensor_id_from_dts(ctx, sensor_id);
				if (first_read) {
					read_unique_sensorid(ctx);
					read_eeprom_common_data(ctx, &oplus_eeprom_info, oplus_eeprom_addr_table);
					first_read = KAL_FALSE;
				}
				return ERROR_NONE;
			}
			DRV_LOGE(ctx, "Read sensor id fail. i2c_write_id: 0x%x\n", ctx->i2c_write_id);
			DRV_LOG(ctx, "sensor_id = 0x%x, ctx->s_ctx.sensor_id = 0x%x\n",
				*sensor_id, ctx->s_ctx.sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != ctx->s_ctx.sensor_id) {
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}

static int open(struct subdrv_ctx *ctx)
{
	u32 sensor_id = 0;
	u32 scenario_id = 0;

	/* get sensor id */
	if (get_imgsensor_id(ctx, &sensor_id) != ERROR_NONE)
		return ERROR_SENSOR_CONNECT_FAIL;

	/* initail setting */
	/*subdrv_i2c_wr_regs_u8_u8(ctx, brzauwide_soft_reset, ARRAY_SIZE(brzauwide_soft_reset));
	mdelay(3);*/
	subdrv_i2c_wr_regs_u8_u8(ctx, brzauwide_init_setting, ARRAY_SIZE(brzauwide_init_setting));

	memset(ctx->exposure, 0, sizeof(ctx->exposure));
	memset(ctx->ana_gain, 0, sizeof(ctx->gain));
	ctx->exposure[0] = ctx->s_ctx.exposure_def;
	ctx->ana_gain[0] = ctx->s_ctx.ana_gain_def;
	ctx->current_scenario_id = scenario_id;
	ctx->pclk = ctx->s_ctx.mode[scenario_id].pclk;
	ctx->line_length = ctx->s_ctx.mode[scenario_id].linelength;
	ctx->frame_length = ctx->s_ctx.mode[scenario_id].framelength;
	ctx->current_fps = 10 * ctx->pclk / ctx->line_length / ctx->frame_length;
	ctx->readout_length = ctx->s_ctx.mode[scenario_id].readout_length;
	ctx->read_margin = ctx->s_ctx.mode[scenario_id].read_margin;
	ctx->min_frame_length = ctx->frame_length;
	ctx->autoflicker_en = FALSE;
	ctx->test_pattern = 0;
	ctx->ihdr_mode = 0;
	ctx->pdaf_mode = 0;
	ctx->hdr_mode = 0;
	ctx->extend_frame_length_en = 0;
	ctx->is_seamless = 0;
	ctx->fast_mode_on = 0;
	ctx->sof_cnt = 0;
	ctx->ref_sof_cnt = 0;
	ctx->is_streaming = 0;

	return ERROR_NONE;
}

static void set_group_hold(void *arg, u8 en)
{
	if(!en) {  /*fresh*/
		subdrv_i2c_wr_u8_u8(g_ctx, 0xfd, 0x01);
		subdrv_i2c_wr_u8_u8(g_ctx, 0x01, 0x01);
	}
}

static u16 get_gain2reg(u32 gain)
{
	return  0x10 * gain/BASEGAIN;
}

static int g_diff_frame_length[] = {
	2504,  /*preview*/
	2504,  /*capture*/
	2504,  /*normal video*/
	1252,  /*his video*/
	1252,  /*slim video*/
	2504,  /*custom1*/
	1252,  /*custom2*/
	2504,  /*custom3*/
	2504,  /*custom4*/
	2504,  /*custom5*/
};

void brzauwide_write_frame_length(struct subdrv_ctx *ctx, u32 fll)
{
	u32 fll_step = 0;
	u32 vblank = 0;
	u32 diff_frame_length;
	check_current_scenario_id_bound(ctx);

	fll_step = ctx->s_ctx.mode[ctx->current_scenario_id].framelength_step;
	diff_frame_length = g_diff_frame_length[ctx->current_scenario_id] ?
		g_diff_frame_length[ctx->current_scenario_id] : g_diff_frame_length[0];

	ctx->frame_length = fll;

	if (fll_step)
		fll = round_up(fll, fll_step);

	/* write framelength */
	vblank = (fll - diff_frame_length) * 2;

	if(vblank < 65535) {
		subdrv_i2c_wr_u8_u8(ctx, 0xfd, 0x01);
		subdrv_i2c_wr_u8_u8(ctx, 0x05, (vblank >> 8) & 0xFF);
		subdrv_i2c_wr_u8_u8(ctx, 0x06,  vblank & 0xFF);
	/*	subdrv_i2c_wr_u8_u8(ctx, 0xfd, 0x01);
		subdrv_i2c_wr_u8_u8(ctx, 0x01, 0x01);	*/
	}

	DRV_LOG(ctx, "ctx->frame_length(%d), diff_frame_length(%d), vblank(%d)\n",
		ctx->frame_length, diff_frame_length, vblank);
	DRV_LOG(ctx, "fll[0x%x], fll_step:%u ctx->extend_frame_length_en:%d\n",
		fll, fll_step, ctx->extend_frame_length_en);
}

void brzauwide_get_min_shutter_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id,
		u64 *min_shutter, u64 *exposure_step)
{
	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOGE(ctx, "invalid sid:%u, mode_num:%u set default\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		scenario_id = 0;
	}
	check_current_scenario_id_bound(ctx);
	DRV_LOG(ctx, "sensor_mode_num[%d]", ctx->s_ctx.sensor_mode_num);
	if (scenario_id < ctx->s_ctx.sensor_mode_num) {
		switch (ctx->s_ctx.mode[scenario_id].hdr_mode) {
		case HDR_NONE:
			if (ctx->s_ctx.mode[scenario_id].coarse_integ_step &&
				ctx->s_ctx.mode[scenario_id].min_exposure_line) {
				*exposure_step = ctx->s_ctx.mode[scenario_id].coarse_integ_step;
				*min_shutter = ctx->s_ctx.mode[scenario_id].min_exposure_line;
			} else {
				*exposure_step = ctx->s_ctx.exposure_step;
				*min_shutter = ctx->s_ctx.exposure_min;
			}
			break;
		default:
			*exposure_step = ctx->s_ctx.exposure_step;
			*min_shutter = ctx->s_ctx.exposure_min;
			break;
		}
	} else {
		DRV_LOG(ctx, "over sensor_mode_num[%d], use default", ctx->s_ctx.sensor_mode_num);
		*exposure_step = ctx->s_ctx.exposure_step;
		*min_shutter = ctx->s_ctx.exposure_min;
	}
	DRV_LOG(ctx, "scenario_id[%d] exposure_step[%llu] min_shutter[%llu]\n", scenario_id, *exposure_step, *min_shutter);
}

static int brzauwide_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u32 mode = *((u32 *)para);
	if (mode != ctx->test_pattern) {
		if (mode) {
			LOG_INF("%s mode(%d)", __func__, mode);
			switch(mode) {
			case 5:
				subdrv_i2c_wr_u8_u8(ctx, 0xfd, 0x01);
				subdrv_i2c_wr_u8_u8(ctx, 0x21, 0x00); /*DIG_GAIN*/
				subdrv_i2c_wr_u8_u8(ctx, 0x22, 0x00);
				subdrv_i2c_wr_u8_u8(ctx, 0x01, 0x01);
				subdrv_i2c_wr_u8_u8(ctx, 0xfd, 0x07);
				subdrv_i2c_wr_u8_u8(ctx, 0x04, 0x00); /*blc_lvl_target*/
				subdrv_i2c_wr_u8_u8(ctx, 0x05, 0x00);
				break;
			default:
				break;
			}
		} else {
			LOG_INF("%s mode(%d)", __func__, mode);
			subdrv_i2c_wr_u8_u8(ctx, 0xfd, 0x01);
			subdrv_i2c_wr_u8_u8(ctx, 0x21, 0x02);
			subdrv_i2c_wr_u8_u8(ctx, 0x22, 0x00);
			subdrv_i2c_wr_u8_u8(ctx, 0x01, 0x01);
			subdrv_i2c_wr_u8_u8(ctx, 0xfd, 0x07);
			subdrv_i2c_wr_u8_u8(ctx, 0x04, 0x00);
			subdrv_i2c_wr_u8_u8(ctx, 0x05, 0x40);
		}
		ctx->test_pattern = mode;
	}

	return ERROR_NONE;
}

static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id)
{
	memcpy(&(ctx->s_ctx), &static_ctx, sizeof(struct subdrv_static_ctx));
	subdrv_ctx_init(ctx);
	ctx->i2c_client = i2c_client;
	ctx->i2c_write_id = i2c_write_id;
	g_ctx = ctx;

	return 0;
}

static int brzauwide_set_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);
	u16 frame_length = (u16) (*para);
	if (frame_length) {
		ctx->frame_length = frame_length;
	}
	ctx->frame_length = max(ctx->frame_length, ctx->min_frame_length);
	ctx->frame_length = min(ctx->frame_length, ctx->s_ctx.frame_length_max);

	if (gph) {
		ctx->s_ctx.s_gph((void *)ctx, 1);
	}
	brzauwide_write_frame_length(ctx, ctx->frame_length);
	if (gph) {
		ctx->s_ctx.s_gph((void *)ctx, 0);
	}

	DRV_LOG(ctx, "fll(input/output/min):%u/%u/%u\n",
		frame_length, ctx->frame_length, ctx->min_frame_length);
	return 0;
}

static int brzauwide_set_shutter_frame_length_convert(struct subdrv_ctx *ctx, u64 shutter, u32 frame_length)
{
	u32 fine_integ_line = 0;
	u32 cit_step = 0;

	bool gph = (ctx->s_ctx.s_gph != NULL);
	u8 exposure_margin = ctx->s_ctx.exposure_margin;
	DRV_LOG(ctx, "shutter:%llu, frame_length:%u  exposure_margin:%d\n", shutter, frame_length, exposure_margin);

	ctx->frame_length = frame_length ? frame_length : ctx->frame_length;
	check_current_scenario_id_bound(ctx);
	/* check boundary of shutter */
	fine_integ_line = ctx->s_ctx.mode[ctx->current_scenario_id].fine_integ_line;
	shutter = FINE_INTEG_CONVERT(shutter, fine_integ_line);
	shutter = max(shutter, (u64)ctx->s_ctx.exposure_min);
	shutter = min(shutter, (u64)ctx->s_ctx.exposure_max);
	/* check boundary of framelength */

	cit_step = ctx->s_ctx.mode[ctx->current_scenario_id].coarse_integ_step;
	if (cit_step)
		shutter = round_up(shutter, cit_step);
	ctx->frame_length =	max(shutter + exposure_margin, (u64)ctx->frame_length);
	ctx->frame_length =	max((u64)ctx->min_frame_length, (u64)ctx->frame_length);
	ctx->frame_length =	min(ctx->frame_length, ctx->s_ctx.frame_length_max);

	/* restore shutter */
	memset(ctx->exposure, 0, sizeof(ctx->exposure));
	ctx->exposure[0] = shutter;
	/* group hold start */
	if (gph)
		ctx->s_ctx.s_gph((void *)ctx, 1);

	/* write framelength */
	if (set_auto_flicker(ctx, 0) || frame_length || !ctx->s_ctx.reg_addr_auto_extend)
		brzauwide_write_frame_length(ctx, ctx->frame_length);
	/* write shutter */
	subdrv_i2c_wr_u8_u8(ctx, 0xfd, 0x01);
	subdrv_i2c_wr_u8_u8(ctx, 0x02, (shutter * 2 >> 16) & 0xFF);
	subdrv_i2c_wr_u8_u8(ctx, 0x03, (shutter * 2 >>  8) & 0xFF);
	subdrv_i2c_wr_u8_u8(ctx, 0x04,  shutter * 2  & 0xFF);
/*	subdrv_i2c_wr_u8_u8(ctx, 0xfd, 0x01);	
	subdrv_i2c_wr_u8_u8(ctx, 0x01, 0x01);	*/

	DRV_LOG(ctx, "exp[0x%x], fll(input/output):%u/%u, flick_en:%u\n",
		ctx->exposure[0], frame_length, ctx->frame_length, ctx->autoflicker_en);

	if (!ctx->ae_ctrl_gph_en) {
		if (gph)
			ctx->s_ctx.s_gph((void *)ctx, 0);
		/*commit_i2c_buffer(ctx);*/
	}
	/* group hold end */

	return 0;
}

static int brzauwide_set_shutter_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	return brzauwide_set_shutter_frame_length_convert(ctx, ((u64*)para)[0], ((u64*)para)[1]);
}

static int brzauwide_set_shutter_convert(struct subdrv_ctx *ctx, u64 shutter)
{
	return brzauwide_set_shutter_frame_length_convert(ctx, shutter, 0);
}

static int brzauwide_set_shutter(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	return brzauwide_set_shutter_frame_length_convert(ctx, ((u64*)para)[0], 0);
}

static int brzauwide_set_multi_shutter_frame_length_ctrl(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 *feature_data = (u64*)para;
	u64 *shutters =(u64 *)(*feature_data);
	u16 exp_cnt = (u64) (*(feature_data + 1));
	u64 framelength = (u64) (*(feature_data + 2));

	if(exp_cnt != 1) {
		LOG_INF("exp_cnt(%d) != 1\n", exp_cnt);
	}

	return brzauwide_set_shutter_frame_length_convert(ctx, shutters[0], framelength);
}

static int brzauwide_set_gain_convert(struct subdrv_ctx *ctx, u32 gain)
{
	u16 rg_gain;
	bool gph = (ctx->s_ctx.s_gph != NULL);
	u32 ana_gain_min = ctx->s_ctx.mode[ctx->current_scenario_id].ana_gain_max ?
		ctx->s_ctx.mode[ctx->current_scenario_id].ana_gain_min : ctx->ana_gain_min;
	u32 ana_gain_max = ctx->s_ctx.mode[ctx->current_scenario_id].ana_gain_max ?
		ctx->s_ctx.mode[ctx->current_scenario_id].ana_gain_max : ctx->ana_gain_max;

	/* check boundary of gain */
	gain = max(gain, ana_gain_min);
	gain = min(gain, ana_gain_max);

	/* mapping of gain to register value */
	rg_gain = get_gain2reg(gain);

	/* restore gain */
	memset(ctx->ana_gain, 0, sizeof(ctx->ana_gain));
	ctx->ana_gain[0] = gain;

	/* group hold start */
	if (gph && !ctx->ae_ctrl_gph_en)
		ctx->s_ctx.s_gph((void *)ctx, 1);

	/* write gain */
	subdrv_i2c_wr_u8_u8(ctx, 0xfd, 0x01);
	subdrv_i2c_wr_u8_u8(ctx, 0x24, rg_gain);
	subdrv_i2c_wr_u8_u8(ctx, 0xfd, 0x01); /*page1*/
	subdrv_i2c_wr_u8_u8(ctx, 0x01, 0x01); /*fresh*/

	DRV_LOG(ctx, "%s gain(%d) rg_gain[0x%x]\n", __func__, gain, rg_gain);
	if (gph)
		ctx->s_ctx.s_gph((void *)ctx, 0);
	/* group hold end */

	return 0;
}

static int brzauwide_set_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64* feature_data = (u64*)para;
	u32 gain = *feature_data;

	return brzauwide_set_gain_convert(ctx, gain);
}

void brzauwide_set_dummy(struct subdrv_ctx *ctx)
{
}

static int brzauwide_set_max_framerate_by_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	enum SENSOR_SCENARIO_ID_ENUM scenario_id = (u32)((u64*)para)[0];
	u32 framerate = (u32)((u64*)para)[1];
	u32 frame_length;
	u32 frame_length_step;

	LOG_INF("scenario_id(%d), framerate(%d)", scenario_id, framerate);

	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOG(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
	}

	if (framerate == 0) {
		DRV_LOG(ctx, "framerate (%u) is invalid\n", framerate);
		return 0;
	}

	if (ctx->s_ctx.mode[scenario_id].linelength == 0) {
		DRV_LOG(ctx, "linelength (%u) is invalid\n",
			ctx->s_ctx.mode[scenario_id].linelength);
		return 0;
	}

	frame_length = ctx->s_ctx.mode[scenario_id].pclk / framerate * 10
		/ ctx->s_ctx.mode[scenario_id].linelength;
	frame_length_step = ctx->s_ctx.mode[scenario_id].framelength_step;
	frame_length = frame_length_step ?
		(frame_length - (frame_length % frame_length_step)) : frame_length;
	ctx->frame_length =
		max(frame_length, ctx->s_ctx.mode[scenario_id].framelength);
	ctx->frame_length = min(ctx->frame_length, ctx->s_ctx.frame_length_max);
	ctx->current_fps = ctx->pclk / ctx->frame_length * 10 / ctx->line_length;
	ctx->min_frame_length = ctx->frame_length;
	DRV_LOG(ctx, "max_fps(input/output):%u/%u(sid:%u), min_fl_en:1\n",
		framerate, ctx->current_fps, scenario_id);
	if (ctx->s_ctx.reg_addr_auto_extend ||
			(ctx->frame_length > (ctx->exposure[0] + ctx->s_ctx.exposure_margin))) {
		brzauwide_set_dummy(ctx);
	}

	return 0;
}

static int brzauwide_extend_frame_length_convert(struct subdrv_ctx *ctx, u32 ns)
{
	return 0;
}

static int brzauwide_extend_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u32 ns = (u32)((u64*)para)[0];

	return brzauwide_extend_frame_length_convert(ctx, ns);
}
static int vsync_notify(struct subdrv_ctx *ctx,	unsigned int sof_cnt)
{
	DRV_LOG(ctx, "sof_cnt(%u) ctx->ref_sof_cnt(%u) ctx->fast_mode_on(%d)",
		sof_cnt, ctx->ref_sof_cnt, ctx->fast_mode_on);
	ctx->sof_cnt = sof_cnt;

	return 0;
}

static int brzauwide_set_register(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u8 page = ((MSDK_SENSOR_REG_INFO_STRUCT *)para)->RegAddr & 0xFF00;
	u8 addr = ((MSDK_SENSOR_REG_INFO_STRUCT *)para)->RegAddr & 0xFF;


	subdrv_i2c_wr_u8_u8(ctx, 0xfd, page);
	subdrv_i2c_wr_u8_u8(ctx, addr, ((MSDK_SENSOR_REG_INFO_STRUCT *)para)->RegData & 0xFF);

	subdrv_i2c_wr_u8_u8(ctx, 0xfd, 0x01);	/*page1*/
	subdrv_i2c_wr_u8_u8(ctx, 0x01, 0x01);	/*fresh*/

	pr_err("%s RegAddr: 0x%08x, RegData: 0x%04x \n",
		__func__, ((MSDK_SENSOR_REG_INFO_STRUCT *)para)->RegAddr, ((MSDK_SENSOR_REG_INFO_STRUCT *)para)->RegData);

	return 0;
}

static int brzauwide_get_register(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u8 page = ((MSDK_SENSOR_REG_INFO_STRUCT *)para)->RegAddr & 0xFF00;
	u8 addr = ((MSDK_SENSOR_REG_INFO_STRUCT *)para)->RegAddr & 0xFF;

	subdrv_i2c_wr_u8_u8(ctx, 0xfd, page);
	((MSDK_SENSOR_REG_INFO_STRUCT *)para)->RegData =
		subdrv_i2c_rd_u8_u8(ctx, addr);
	pr_err("%s RegAddr: 0x%08x, RegData: 0x%04x \n",
		__func__, ((MSDK_SENSOR_REG_INFO_STRUCT *)para)->RegAddr, ((MSDK_SENSOR_REG_INFO_STRUCT *)para)->RegData);

	return 0;
}


static void get_imgsensor_id_from_dts(struct subdrv_ctx *ctx, u32 *sensor_id) {
	struct subdrv_entry *m_subdrv_entry = &brzauwide_mipi_raw_entry;
	u32 final_sensor_id = 0xFFFFFFFF;
	const char *of_sensor_names[OF_SENSOR_NAMES_MAXCNT];
	const char *of_sensor_hal_names[OF_SENSOR_NAMES_MAXCNT];
	u32   of_sensor_ids[OF_SENSOR_NAMES_MAXCNT] = {0};
	int i, index, of_sensor_names_cnt, of_sensor_hal_names_cnt, of_sensor_ids_ret;
	struct device *dev = &ctx->i2c_client->dev;

	memset(&of_sensor_ids, 0xFF, sizeof(of_sensor_ids));

	if(g_id_from_dts_flag == false) {
		of_sensor_names_cnt = of_property_read_string_array(dev->of_node,
			"sensor-names", of_sensor_names, ARRAY_SIZE(of_sensor_names));

		of_sensor_hal_names_cnt = of_property_read_string_array(dev->of_node,
			"sensor-hal-names", of_sensor_hal_names, ARRAY_SIZE(of_sensor_hal_names));

		of_sensor_ids_ret = of_property_read_u32_array(dev->of_node,
				"sensor-ids", of_sensor_ids, of_sensor_names_cnt);

		pr_err("%s of_sensor_names_cnt(%d), of_sensor_ids_ret(%d)",
			__func__, of_sensor_names_cnt, of_sensor_ids_ret);
		for(i = 0 ;i < of_sensor_names_cnt; i++) {
				pr_err("%s of_sensor_names[%d] = %s  of_sensor_ids[%d] = %d",
				__func__, i, of_sensor_names[i], i, of_sensor_ids[i]);
		}
		for(i = 0 ;i < of_sensor_hal_names_cnt; i++) {
			pr_err("%s of_sensor_hal_names_cnt[%d] = %s",
				__func__, i, of_sensor_hal_names[i]);
		}

		if (of_sensor_names_cnt && (of_sensor_ids_ret == 0)) {
			for(index = 0; index < of_sensor_names_cnt; index++) {
				if (strncmp(SENSOR_NAME, of_sensor_names[index], strlen(SENSOR_NAME)) == 0) {
					final_sensor_id = of_sensor_ids[index];
					break;
				}
			}
		} else {
			pr_err("%s sensor-ids error in dts", __func__);
		}
		g_id_from_dts_flag = true;
	}

	if(final_sensor_id != 0xFFFFFFFF) {
		*sensor_id = final_sensor_id;
		ctx->s_ctx.sensor_id = final_sensor_id;

		m_subdrv_entry->id = final_sensor_id;
		if(of_sensor_hal_names_cnt == of_sensor_names_cnt) {
			m_subdrv_entry->name = of_sensor_hal_names[index];
		}

		pr_err("%s final index(%d), id(%d) name(%s)",
			__func__, index, m_subdrv_entry->id, m_subdrv_entry->name);
	} else {
		*sensor_id = ctx->s_ctx.sensor_id;
	}

	return;
}

static int brzauwide_common_control(struct subdrv_ctx *ctx,
			enum SENSOR_SCENARIO_ID_ENUM scenario_id,
			MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	int ret = ERROR_NONE;
	u16 idx = 0;
	u8 support = FALSE;
	u8 *pbuf = NULL;
	u16 size = 0;
	u16 addr = 0;
	u64 time_boot_begin = 0;
	struct eeprom_info_struct *info = ctx->s_ctx.eeprom_info;
	struct adaptor_ctx *_adaptor_ctx = NULL;
	struct v4l2_subdev *sd = NULL;

	if (ctx->i2c_client)
		sd = i2c_get_clientdata(ctx->i2c_client);
	if (sd)
		_adaptor_ctx = to_ctx(sd);
	if (!_adaptor_ctx) {
		DRV_LOGE(ctx, "null _adaptor_ctx\n");
		return -ENODEV;
	}

	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOG(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
		ret = ERROR_INVALID_SCENARIO_ID;
	}
	update_mode_info(ctx, scenario_id);

	if (ctx->s_ctx.mode[scenario_id].mode_setting_table != NULL) {
		DRV_LOG_MUST(ctx, "E: sid:%u size:%u\n", scenario_id,
			ctx->s_ctx.mode[scenario_id].mode_setting_len);
		if (ctx->power_on_profile_en)
			time_boot_begin = ktime_get_boottime_ns();

		/* initail setting */
		/*subdrv_i2c_wr_regs_u8_u8(ctx, brzauwide_soft_reset, ARRAY_SIZE(brzauwide_soft_reset));
		mdelay(3);*/

		i2c_table_rewrite(ctx, ctx->s_ctx.mode[scenario_id].mode_setting_table,
				ctx->s_ctx.mode[scenario_id].mode_setting_len);

		if (ctx->power_on_profile_en) {
			ctx->sensor_pw_on_profile.i2c_cfg_period =
					ktime_get_boottime_ns() - time_boot_begin;

			ctx->sensor_pw_on_profile.i2c_cfg_table_len =
					ctx->s_ctx.mode[scenario_id].mode_setting_len;
		}
		DRV_LOG(ctx, "X: sid:%u size:%u\n", scenario_id,
			ctx->s_ctx.mode[scenario_id].mode_setting_len);
	} else {
		DRV_LOGE(ctx, "please implement mode setting(sid:%u)!\n", scenario_id);
	}

	if (check_is_no_crop(ctx, scenario_id) && probe_eeprom(ctx)) {
		idx = ctx->eeprom_index;
		support = info[idx].xtalk_support;
		pbuf = info[idx].preload_xtalk_table;
		size = info[idx].xtalk_size;
		addr = info[idx].sensor_reg_addr_xtalk;
		if (support) {
			if (pbuf != NULL && addr > 0 && size > 0) {
				subdrv_i2c_wr_seq_p8(ctx, addr, pbuf, size);
				DRV_LOG(ctx, "set XTALK calibration data done.");
			}
		}
	}

	set_mirror_flip(ctx, ctx->s_ctx.mirror);

	return ret;
}
