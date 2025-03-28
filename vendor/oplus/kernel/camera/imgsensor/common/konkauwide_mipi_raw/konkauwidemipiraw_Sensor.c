// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2022 MediaTek Inc.

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 konkauwidemipiraw_Sensor.c
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
#include "konkauwidemipiraw_Sensor.h"

#define KONKAUWIDE_EEPROM_READ_ID	0xA1
#define KONKAUWIDE_EEPROM_WRITE_ID   0xA0
#define KONKAUWIDE_MAX_OFFSET		0x4000
#define OPLUS_CAMERA_COMMON_DATA_LENGTH 40
#define PFX "konkauwide_camera_sensor"
#define LOG_INF(format, args...) pr_err(PFX "[%s] " format, __func__, ##args)
#define OTP_SIZE    0x4000
#define OTP_QCOM_PDAF_DATA_LENGTH 0x468
#define OTP_QCOM_PDAF_DATA_START_ADDR 0x600
#define AF_CODE_SIZE 6
#define GET_SENSOR_ID_RETRY_CNT    5

static bool module_flag = FALSE;
static bool bNeedSetNormalMode = FALSE;
static kal_uint8 otp_data_checksum[OTP_SIZE] = {0};
static kal_uint8 otp_qcom_pdaf_data[OTP_QCOM_PDAF_DATA_LENGTH] = {0};
#define MAX_BURST_LEN  2048
static u8 * msg_buf = NULL;

static int group_hold_frame_count = 0;
static void set_group_hold(void *arg, u8 en);
static u16 get_gain2reg(u32 gain);
static int konkauwide_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int konkauwide_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int konkauwide_check_sensor_id(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int get_eeprom_common_data(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int konkauwide_set_eeprom_calibration(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int konkauwide_get_eeprom_calibration(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int konkauwide_get_otp_checksum_data(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int konkauwide_streaming_suspend(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int konkauwide_streaming_resume(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int konkauwide_set_shutter(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static void konkauwide_set_shutter_convert(struct subdrv_ctx *ctx, u32 shutter);
static int konkauwide_set_shutter_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int get_imgsensor_id(struct subdrv_ctx *ctx, u32 *sensor_id);
static int open(struct subdrv_ctx *ctx);
static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id);
static int get_sensor_temperature(void *arg);
static void get_sensor_cali(void* arg);
static void set_sensor_cali(void *arg);
static int konkauwide_set_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int konkauwide_set_gain_convert(struct subdrv_ctx *ctx, u32 gain);
static int konkauwide_set_multi_shutter_frame_length_ctrl(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int konkauwide_set_hdr_tri_shutter2(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int konkauwide_set_hdr_tri_shutter3(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int konkauwide_set_multi_shutter_frame_length_in_lut(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static void konkauwide_set_multi_shutter_frame_length_in_lut_convert(struct subdrv_ctx *ctx,
	u64 *shutters, u16 exp_cnt, u32 frame_length, u32 *frame_length_in_lut);
static void konkauwide_write_frame_length_in_lut(struct subdrv_ctx *ctx, u32 fll, u32 *fll_in_lut);
static bool read_cmos_eeprom_p8(struct subdrv_ctx *ctx, kal_uint16 addr,
                    BYTE *data, int size);
static int konkauwide_get_otp_qcom_pdaf_data(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int konkauwide_set_awb_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int konkauwide_i2c_burst_wr_regs_u16(struct subdrv_ctx *ctx, u16 * list, u32 len);
static int adapter_i2c_burst_wr_regs_u16(struct subdrv_ctx * ctx,
		u16 addr, u16 *list, u32 len);
static void konkauwide_lens_pos_writeback(struct subdrv_ctx *ctx);
static int konkauwide_set_max_framerate_by_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static void konkauwide_set_max_framerate_in_lut_by_scenario(struct subdrv_ctx *ctx,
	enum SENSOR_SCENARIO_ID_ENUM scenario_id, u32 framerate);
/* STRUCT */

static kal_uint16 g_af_code_macro    = 0;
static kal_uint16 g_af_code_infinity = 0;

static struct eeprom_map_info konkauwide_eeprom_info[] = {
	{ EEPROM_META_MODULE_ID, 0x0000, 0x000F, 0x0010, 2, true },
	{ EEPROM_META_SENSOR_ID, 0x0006, 0x000F, 0x0010, 2, true },
	{ EEPROM_META_LENS_ID, 0x0008, 0x000F, 0x0010, 2, true },
	{ EEPROM_META_VCM_ID, 0x000A, 0x000F, 0x0010, 2, true },
	{ EEPROM_META_MIRROR_FLIP, 0x000E, 0x000F, 0x0010, 1, true },
	{ EEPROM_META_MODULE_SN, 0x00B0, 0x00C7, 0x00C8, 23, true },
	{ EEPROM_META_AF_CODE, 0x0092, 0x0098, 0x0099, 6, true },
	{ EEPROM_META_AF_FLAG, 0x0098, 0x0098, 0x0099, 1, true },
	{ EEPROM_META_STEREO_DATA, 0x51F0, 0x0000, 0x0000, CALI_DATA_SLAVE_LENGTH, false },
	{ EEPROM_META_STEREO_MW_MAIN_DATA, 0x2780, 0x2D89, 0x2D8A, CALI_DATA_SLAVE_LENGTH, false },
	{ EEPROM_META_STEREO_MT_MAIN_DATA, 0x31C0, 0x3859, 0x385A, CALI_DATA_MASTER_LENGTH, false },
};

static struct subdrv_feature_control feature_control_list[] = {
	{SENSOR_FEATURE_SET_TEST_PATTERN, konkauwide_set_test_pattern},
	{SENSOR_FEATURE_SEAMLESS_SWITCH, konkauwide_seamless_switch},
	{SENSOR_FEATURE_CHECK_SENSOR_ID, konkauwide_check_sensor_id},
	{SENSOR_FEATURE_GET_EEPROM_COMDATA, get_eeprom_common_data},
	{SENSOR_FEATURE_SET_SENSOR_OTP, konkauwide_set_eeprom_calibration},
	{SENSOR_FEATURE_GET_EEPROM_STEREODATA, konkauwide_get_eeprom_calibration},
	{SENSOR_FEATURE_GET_SENSOR_OTP_ALL, konkauwide_get_otp_checksum_data},
	{SENSOR_FEATURE_SET_STREAMING_SUSPEND, konkauwide_streaming_suspend},
	{SENSOR_FEATURE_SET_STREAMING_RESUME, konkauwide_streaming_resume},
	{SENSOR_FEATURE_SET_ESHUTTER, konkauwide_set_shutter},
	{SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME, konkauwide_set_shutter_frame_length},
	{SENSOR_FEATURE_SET_GAIN, konkauwide_set_gain},
	{SENSOR_FEATURE_SET_HDR_SHUTTER, konkauwide_set_hdr_tri_shutter2},
	{SENSOR_FEATURE_SET_HDR_TRI_SHUTTER, konkauwide_set_hdr_tri_shutter3},
	{SENSOR_FEATURE_SET_MULTI_SHUTTER_FRAME_TIME, konkauwide_set_multi_shutter_frame_length_ctrl},
	{SENSOR_FEATURE_GET_OTP_QCOM_PDAF_DATA, konkauwide_get_otp_qcom_pdaf_data},
	{SENSOR_FEATURE_SET_AWB_GAIN, konkauwide_set_awb_gain},
	{SENSOR_FEATURE_SET_MULTI_SHUTTER_FRAME_TIME_IN_LUT, konkauwide_set_multi_shutter_frame_length_in_lut},
	{SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO, konkauwide_set_max_framerate_by_scenario},
};

static struct eeprom_info_struct eeprom_info[] = {
	{
		.header_id = 0x01AB010A,
		.addr_header_id = 0x00000006,
		.i2c_write_id = 0xA0,
	},
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
	.i4OffsetX = 0,
	.i4OffsetY = 0,
	.i4PitchX =  0,
	.i4PitchY =  0,
	.i4PairNum = 0,
	.i4SubBlkW = 0,
	.i4SubBlkH = 0,
	.i4PosL = {{0, 0} },
	.i4PosR = {{0, 0} },
	.i4BlockNumX = 0,
	.i4BlockNumY = 0,
	.i4LeFirst = 0,
	.iMirrorFlip = IMAGE_NORMAL,
	// i4Crop = (fullRaw - imgSz) / 2 / Bin
	.i4Crop = {
		/* <pre> <cap> <normal_video> <hs_video> <slim_video> */
		{0, 0}, {0, 0}, {0, 384}, {0, 384}, {0, 384},
		/* <cust1> <cust2> <cust3> <cust4> <cust5> */
		{0, 0}, {0, 0}, {0, 0}, {0, 384}, {0, 0},
		/* <cust6> <cust7> <cust8> <cust9> <cust10>*/
		{0, 384}, {0, 384}, {0, 0}, {0, 0}, {0, 0},
		/* <cust11> <cust12>*/
		{0, 0}, {416, 312},
	},
	.i4FullRawW = 4096,
	.i4FullRawH = 3072,
	.i4VCPackNum = 1,
	.PDAF_Support = PDAF_SUPPORT_CAMSV_QPD,
	.i4ModeIndex = 0x3,
	.sPDMapInfo[0] = {
		.i4PDPattern = 1,//all-pd
		.i4BinFacX = 2,
		.i4BinFacY = 4,
		.i4PDRepetition = 0,
		.i4PDOrder = {1}, // R=1, L=0
	},
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_v2h2 = {
	.i4OffsetX = 0,
	.i4OffsetY = 0,
	.i4PitchX =  0,
	.i4PitchY =  0,
	.i4PairNum = 0,
	.i4SubBlkW = 0,
	.i4SubBlkH = 0,
	.i4PosL = {{0, 0} },
	.i4PosR = {{0, 0} },
	.i4BlockNumX = 0,
	.i4BlockNumY = 0,
	.i4LeFirst = 0,
	.iMirrorFlip = IMAGE_NORMAL,
	// i4Crop = (fullRaw - imgSz) / 2 / Bin
	.i4Crop = {
		/* <pre> <cap> <normal_video> <hs_video> <slim_video> */
		{0, 0}, {0, 0}, {0, 384}, {0, 384}, {0, 384},
		/* <cust1> <cust2> <cust3> <cust4> <cust5> */
		{0, 384}, {0, 0}, {0, 0}, {208, 156}, {96, 72},
		/* <cust6> <cust7> <cust8> <cust9> <cust10>*/
		{0, 192}, {0, 192}, {0, 0}, {0, 192}, {0, 0},
	},
	.i4FullRawW = 2048,
	.i4FullRawH = 1536,
	.i4VCPackNum = 1,
	.PDAF_Support = PDAF_SUPPORT_CAMSV_QPD,
	.i4ModeIndex = 0x3,
	.sPDMapInfo[0] = {
		.i4PDPattern = 1,//all-pd
		.i4BinFacX = 2,
		.i4BinFacY = 4,
		.i4PDRepetition = 0,
		.i4PDOrder = {1}, // R=1, L=0
	},
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_full = {
	.i4OffsetX = 0,
	.i4OffsetY = 0,
	.i4PitchX =  0,
	.i4PitchY =  0,
	.i4PairNum = 0,
	.i4SubBlkW = 0,
	.i4SubBlkH = 0,
	.i4PosL = {{0, 0} },
	.i4PosR = {{0, 0} },
	.i4BlockNumX = 0,
	.i4BlockNumY = 0,
	.i4LeFirst = 0,
	.iMirrorFlip = IMAGE_NORMAL,
	// i4Crop = (fullRaw - imgSz) / 2 / Bin
	.i4Crop = {
		/* <pre> <cap> <normal_video> <hs_video> <slim_video> */
		{0, 0}, {0, 0}, {0, 384}, {0, 384}, {0, 384},
		/* <cust1> <cust2> <cust3> <cust4> <cust5> */
		{0, 384}, {0, 0}, {0, 0}, {3280, 2460}, {3168, 3276},
		/* <cust6> <cust7> <cust8> <cust9> <cust10> <cust11>*/
		{0, 0}, {0, 384}, {0, 384}, {0, 0}, {2048, 1536}, {2048, 1536},
	},
	.i4FullRawW = 8192,
	.i4FullRawH = 6144,
	.i4VCPackNum = 1,
	.PDAF_Support = PDAF_SUPPORT_CAMSV_QPD,
	.i4ModeIndex = 0x3,
	.sPDMapInfo[0] = {
		.i4PDPattern = 1,//all-pd
		.i4BinFacX = 4,
		.i4BinFacY = 8,
		.i4PDRepetition = 0,
		.i4PDOrder = {1}, // R=1, L=0
	},
};

static struct mtk_sensor_saturation_info imgsensor_saturation_info = {
	.gain_ratio = 1000,
	.OB_pedestal = 64,
	.saturation_level = 1023,
};

static struct mtk_sensor_saturation_info imgsensor_saturation_info_12bit = {
	.gain_ratio = 4000,
	.OB_pedestal = 256,
	.saturation_level = 4095,
};

static u32 konkauwide_dcg_ratio_table_10bit[] = {4000};
static u32 konkauwide_dcg_ratio_table_12bit[] = {4000};

static struct mtk_mbus_frame_desc_entry frame_desc_prev[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 4,
			.data_type = 0x30,
			.hsize = 4096,
			.vsize = 768,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cap[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 4,
			.data_type = 0x30,
			.hsize = 4096,
			.vsize = 768,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 2304,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 4,
			.data_type = 0x30,
			.hsize = 4096,
			.vsize = 576,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_hs_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 2304,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 4096,
			.vsize = 576,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_slim_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 2048,
			.vsize = 1152,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus1[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_ME,
		},
	},
	{
		.bus.csi2 = {
			.channel = 4,
			.data_type = 0x30,
			.hsize = 4096,
			.vsize = 768,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

/* 8192x6144 Fullsize bayer mode 6*/
static struct mtk_mbus_frame_desc_entry frame_desc_cus2[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 8192,
			.vsize = 6144,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};

/* 8192x6144 Fullsize QBC mode 7*/
static struct mtk_mbus_frame_desc_entry frame_desc_cus3[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 8192,
			.vsize = 6144,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};

/* 1632X1224 24fps subsample */
static struct mtk_mbus_frame_desc_entry frame_desc_cus4[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 1632,
			.vsize = 1224,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 1632,
			.vsize = 306,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
};

/* 1856x1392 24fps subsample*/
static struct mtk_mbus_frame_desc_entry frame_desc_cus5[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 1856,
			.vsize = 1392,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 1856,
			.vsize = 348,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
};

/* 4096x2304@30fps DSG sensor merge raw12 */
static struct mtk_mbus_frame_desc_entry frame_desc_cus6[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2C,
			.hsize = 4096,
			.vsize = 2304,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 4,
			.data_type = 0x30,
			.hsize = 4096,
			.vsize = 576,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus7[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 2304,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 2304,
			.user_data_desc = VC_STAGGER_ME,
		},
	},
	{
		.bus.csi2 = {
			.channel = 4,
			.data_type = 0x30,
			.hsize = 4096,
			.vsize = 576,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

/* 4096x3072 60fps binning*/
static struct mtk_mbus_frame_desc_entry frame_desc_cus8[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 4096,
			.vsize = 768,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
};

/* 4096x3072 30fps, DAG sensor merge 12bit */
static struct mtk_mbus_frame_desc_entry frame_desc_cus9[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2C,
			.hsize = 4096,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 4096,
			.vsize = 768,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
};

/* 14_OceanDX4_03_JN5_Full_12.5Mp_4096x3072_30fps_3056Msps izoom */
static struct mtk_mbus_frame_desc_entry frame_desc_cus10[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 2048,
			.vsize = 384,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
};

/* 14_OceanDX4_03_JN5_Full_12.5Mp_Bypass_4096x3072_30fps_3056Msps QBC*/
static struct mtk_mbus_frame_desc_entry frame_desc_cus11[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 2048,
			.vsize = 384,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
};

static struct subdrv_mode_struct mode_struct[] = {
	{ /*03_OceanDX4_05_JN5_Fdsum_12.5Mp_4096x3072_30fps_3056Msps*/
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = konkauwide_preview_setting,
		.mode_setting_len = ARRAY_SIZE(konkauwide_preview_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = konkauwide_seamless_preview,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(konkauwide_seamless_preview),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 920000000,
		.linelength = 4784,
		.framelength = 6408,
		.max_framerate = 300,
		.mipi_pixel_rate = 1222400000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 4096,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {78},
	},
	{  /*03_OceanDX4_05_JN5_Fdsum_12.5Mp_4096x3072_30fps_3056Msps*/
		.frame_desc = frame_desc_cap,
		.num_entries = ARRAY_SIZE(frame_desc_cap),
		.mode_setting_table = konkauwide_capture_setting,
		.mode_setting_len = ARRAY_SIZE(konkauwide_capture_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 920000000,
		.linelength = 4784,
		.framelength = 6408,
		.max_framerate = 300,
		.mipi_pixel_rate = 1222400000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 4096,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {78},
	},
	{  /*05_OceanDX4_09_JN5_Fdsum_4K_4096x2304_30fps_3056Msps*/
		.frame_desc = frame_desc_vid,
		.num_entries = ARRAY_SIZE(frame_desc_vid),
		.mode_setting_table = konkauwide_normal_video_setting,
		.mode_setting_len = ARRAY_SIZE(konkauwide_normal_video_setting),
		.seamless_switch_group = 2,
		.seamless_switch_mode_setting_table = konkauwide_seamless_vid,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(konkauwide_seamless_vid),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 920000000,
		.linelength = 4784,
		.framelength = 6408,
		.max_framerate = 300,
		.mipi_pixel_rate = 1222400000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 768,
			.w0_size = 8192,
			.h0_size = 4608 ,
			.scale_w = 4096,
			.scale_h = 2304,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2304,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2304,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {78},
	},
	{ /* 08_OceanDX4_09_JN5_Fdsum_4K_4096x2304_60fps_3056Msps */
		.frame_desc = frame_desc_hs_vid,
		.num_entries = ARRAY_SIZE(frame_desc_hs_vid),
		.mode_setting_table = konkauwide_hs_video_setting,
		.mode_setting_len = ARRAY_SIZE(konkauwide_hs_video_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 920000000,
		.linelength = 4784,
		.framelength = 3204,
		.max_framerate = 600,
		.mipi_pixel_rate = 1222400000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 768,
			.w0_size = 8192,
			.h0_size = 4608,
			.scale_w = 4096,
			.scale_h = 2304,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2304,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2304,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {73},
	},
	{ /*12_OceanDX4_13_0_JN5_A2A2_FHD_2048x1152_240.4fps_1992Msps*/
		.frame_desc = frame_desc_slim_vid,
		.num_entries = ARRAY_SIZE(frame_desc_slim_vid),
		.mode_setting_table = konkauwide_slim_video_setting,
		.mode_setting_len = ARRAY_SIZE(konkauwide_slim_video_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 920000000,
		.linelength = 3072,
		.framelength = 1246,
		.max_framerate = 2400,
		.mipi_pixel_rate = 796800000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 768,
			.w0_size = 8192,
			.h0_size = 4608,
			.scale_w = 2048,
			.scale_h = 1152,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 2048,
			.h1_size = 1152,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 2048,
			.h2_tg_size = 1152,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {87},
	},
	{ /* 04_OceanDX4_05_JN5_Fdsum_12.5Mp_AEB_4096x3072_60fps_3056Msps */
		.frame_desc = frame_desc_cus1,
		.num_entries = ARRAY_SIZE(frame_desc_cus1),
		.mode_setting_table = konkauwide_custom1_setting,
		.mode_setting_len = ARRAY_SIZE(konkauwide_custom1_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = konkauwide_seamless_custom1,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(konkauwide_seamless_custom1),
		.hdr_mode = HDR_RAW_LBMF,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 920000000,
		.linelength = 4784,
		.framelength = 3204 * 2,
		.max_framerate = 300,
		.mipi_pixel_rate = 1222400000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].max = 0xFFFF,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].max = 0xFFFF,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 4096,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
		.csi_param = {79},
		.exposure_order_in_lbmf = IMGSENSOR_LBMF_EXPOSURE_SE_FIRST,
		.mode_type_in_lbmf = IMGSENSOR_LBMF_MODE_MANUAL,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 80,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 80,
	},
	{  /* 8192x6144 Fullsize bayer mode 6*/
		.frame_desc = frame_desc_cus2,
		.num_entries = ARRAY_SIZE(frame_desc_cus2),
		.mode_setting_table = konkauwide_custom2_setting,
		.mode_setting_len = ARRAY_SIZE(konkauwide_custom2_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = konkauwide_seamless_custom2,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(konkauwide_seamless_custom2),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 920000000,
		.linelength = 9600,
		.framelength = 6346,
		.max_framerate = 150,
		.mipi_pixel_rate = 1222400000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 8192,
			.scale_h = 6144,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 8192,
			.h1_size = 6144,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 8192,
			.h2_tg_size = 6144,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = &imgsensor_pd_info_full,
		.ae_binning_ratio = 1250,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {79},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 16,
	},
	{  /* 8192x6144 Fullsize QBC mode 7*/
		.frame_desc = frame_desc_cus3,
		.num_entries = ARRAY_SIZE(frame_desc_cus3),
		.mode_setting_table = konkauwide_custom3_setting,
		.mode_setting_len = ARRAY_SIZE(konkauwide_custom3_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = konkauwide_seamless_custom3,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(konkauwide_seamless_custom3),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 920000000,
		.linelength = 9600,
		.framelength = 6346,
		.max_framerate = 150,
		.mipi_pixel_rate = 1222400000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 8192,
			.scale_h = 6144,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 8192,
			.h1_size = 6144,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 8192,
			.h2_tg_size = 6144,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1250,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {79},
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_Gr,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 16,
	},
	{ /* 1632X1224 24fps subsample */
		.frame_desc = frame_desc_cus4,
		.num_entries = ARRAY_SIZE(frame_desc_cus4),
		.mode_setting_table = konkauwide_custom4_setting,
		.mode_setting_len = ARRAY_SIZE(konkauwide_custom4_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 920000000,
		.linelength = 8848,
		.framelength = 4328,
		.max_framerate = 240,
		.mipi_pixel_rate = 668800000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 1,
		.coarse_integ_step = 2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 832,
			.y0_offset = 624,
			.w0_size = 6528,
			.h0_size = 4896,
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
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_v2h2,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {74},
	},
	{ /* 1856x1392 24fps subsample*/
		.frame_desc = frame_desc_cus5,
		.num_entries = ARRAY_SIZE(frame_desc_cus5),
		.mode_setting_table = konkauwide_custom5_setting,
		.mode_setting_len = ARRAY_SIZE(konkauwide_custom5_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 920000000,
		.linelength = 8844,
		.framelength = 4320,
		.max_framerate = 240,
		.mipi_pixel_rate = 668800000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 4096,
			.h0_size = 3072,
			.scale_w = 4096,
			.scale_h = 3072,
			.x1_offset = 1120,
			.y1_offset = 840,
			.w1_size = 1856,
			.h1_size = 1392,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1856,
			.h2_tg_size = 1392,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_v2h2,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {84},
	},
	{ /* 06_OceanDX4_10_0_JN5_Fdsum_DSG_4096x2304_30fps_3056Msps sensor merge 12bit*/
		.frame_desc = frame_desc_cus6,
		.num_entries = ARRAY_SIZE(frame_desc_cus6),
		.mode_setting_table = konkauwide_custom6_setting,
		.mode_setting_len = ARRAY_SIZE(konkauwide_custom6_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_RAW_DCG_COMPOSE,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW12_Gr,
		.raw_cnt = 1,
		.exp_cnt = 2,
		.pclk = 920000000,
		.linelength = 9568,
		.framelength = 3204,
		.max_framerate = 300,
		.mipi_pixel_rate = 1222400000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 768,
			.w0_size = 8192,
			.h0_size = 4608,
			.scale_w = 4096,
			.scale_h = 2304,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2304,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2304,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.saturation_info = &imgsensor_saturation_info_12bit,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_base = IMGSENSOR_DCG_GAIN_LCG_BASE,
			.dcg_gain_ratio_min = 4000,
			.dcg_gain_ratio_max = 4000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = konkauwide_dcg_ratio_table_12bit,
			.dcg_gain_table_size = sizeof(konkauwide_dcg_ratio_table_12bit),
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 4,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 80,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 20,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {78},
	},
	{  /* 07_OceanDX4_10_0_JN5_Fdsum_DSG_split_4096x2304_30fps_3056Msps AP merge*/
		.frame_desc = frame_desc_cus7,
		.num_entries = ARRAY_SIZE(frame_desc_cus7),
		.mode_setting_table = konkauwide_custom7_setting,
		.mode_setting_len = ARRAY_SIZE(konkauwide_custom7_setting),
		.seamless_switch_group = 2,
		.seamless_switch_mode_setting_table = konkauwide_seamless_custom7,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(konkauwide_seamless_custom7),
		.hdr_mode = HDR_RAW_DCG_RAW,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 920000000,
		.linelength = 9568,
		.framelength = 3204,
		.max_framerate = 300,
		.mipi_pixel_rate = 1222400000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 768,
			.w0_size = 8192,
			.h0_size = 4608,
			.scale_w = 4096,
			.scale_h = 2304,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2304,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2304,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.saturation_info = &imgsensor_saturation_info,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_RAW,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_base = IMGSENSOR_DCG_GAIN_LCG_BASE,
			.dcg_gain_ratio_min = 4000,
			.dcg_gain_ratio_max = 4000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = konkauwide_dcg_ratio_table_10bit,
			.dcg_gain_table_size = sizeof(konkauwide_dcg_ratio_table_10bit),
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 4,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 80,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 20,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {78},
	},
	{ /* 4096x3072 60fps binning*/
		.frame_desc = frame_desc_cus8,
		.num_entries = ARRAY_SIZE(frame_desc_cus8),
		.mode_setting_table = konkauwide_custom8_setting,
		.mode_setting_len = ARRAY_SIZE(konkauwide_custom8_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 920000000,
		.linelength = 4784,
		.framelength = 3204,
		.max_framerate = 600,
		.mipi_pixel_rate = 1222400000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 4096,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {78},
	},
	{ /* 4096x3072 30fps, DAG sensor merge 12bit */
		.frame_desc = frame_desc_cus9,
		.num_entries = ARRAY_SIZE(frame_desc_cus9),
		.mode_setting_table = konkauwide_custom9_setting,
		.mode_setting_len = ARRAY_SIZE(konkauwide_custom9_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_RAW_DCG_COMPOSE,
		.raw_cnt = 1,
		.exp_cnt = 2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW12_Gr,
		.pclk = 920000000,
		.linelength = 9568,
		.framelength = 3204,
		.max_framerate = 300,
		.mipi_pixel_rate = 1222400000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 4096,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = TRUE,  // temp setting no pd
		.imgsensor_pd_info = &imgsensor_pd_info,
		.saturation_info = &imgsensor_saturation_info_12bit,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_base = IMGSENSOR_DCG_GAIN_LCG_BASE,
			.dcg_gain_ratio_min = 4000,
			.dcg_gain_ratio_max = 4000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = konkauwide_dcg_ratio_table_12bit,
			.dcg_gain_table_size = sizeof(konkauwide_dcg_ratio_table_12bit),
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 4,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 80,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 20,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {78},
	},
	{ /* 14_OceanDX4_03_JN5_Full_12.5Mp_4096x3072_30fps_3056Msps izoom */
		.frame_desc = frame_desc_cus10,
		.num_entries = ARRAY_SIZE(frame_desc_cus10),
		.mode_setting_table = konkauwide_custom10_setting,
		.mode_setting_len = ARRAY_SIZE(konkauwide_custom10_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 920000000,
		.linelength = 9200,
		.framelength = 3332,
		.max_framerate = 300,
		.mipi_pixel_rate = 1222400000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 2048,
			.y0_offset = 1536,
			.w0_size = 4096,
			.h0_size = 3072,
			.scale_w = 4096,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1250,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {87},
	},
	{  /* 14_OceanDX4_03_JN5_Full_12.5Mp_Bypass_4096x3072_30fps_3056Msps QBC*/
		.frame_desc = frame_desc_cus11,
		.num_entries = ARRAY_SIZE(frame_desc_cus11),
		.mode_setting_table = konkauwide_custom11_setting,
		.mode_setting_len = ARRAY_SIZE(konkauwide_custom11_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 920000000,
		.linelength = 9200,
		.framelength = 3332,
		.max_framerate = 300,
		.mipi_pixel_rate = 1222400000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 2048,
			.y0_offset = 1536,
			.w0_size = 4096,
			.h0_size = 3072,
			.scale_w = 4096,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1250,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_Gr,
	},
};

static struct subdrv_static_ctx static_ctx = {
	.sensor_id = KONKAUWIDE_SENSOR_ID,
	.reg_addr_sensor_id = {0x0000, 0x0001},
	.i2c_addr_table = {0x5a, 0xff},
	.i2c_burst_write_support = TRUE,
	.i2c_transfer_data_type = I2C_DT_ADDR_16_DATA_16,
	.eeprom_info = eeprom_info,
	.eeprom_num = ARRAY_SIZE(eeprom_info),
	.resolution = {8192, 6144},
	.mirror = IMAGE_NORMAL,

	.mclk = 24,
	.isp_driving_current = ISP_DRIVING_4MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.ob_pedestal = 0x40,

	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_Gr,
	.ana_gain_def = BASEGAIN * 4,
	.ana_gain_min = BASEGAIN * 1,
	.ana_gain_max = BASEGAIN * 160,
	.ana_gain_type = 2, //0-SONY; 1-OV; 2 - SUMSUN; 3 -HYNIX; 4 -GC
	.ana_gain_step = 2,
	.ana_gain_table = konkauwide_ana_gain_table,
	.ana_gain_table_size = sizeof(konkauwide_ana_gain_table),
	.tuning_iso_base = 100,
	.exposure_def = 0x3D0,
	.exposure_min = 4,
	.exposure_max = 0xffff * 256,
	.exposure_step = 1,
	.exposure_margin = 24, /*tentative*/
	.frame_length_max = 0xffff,
	.ae_effective_frame = 2,
	.frame_time_delay_frame = 2,
	.start_exposure_offset = 1616100,
	.line_interleave_num = 1,

	.pdaf_type = PDAF_SUPPORT_CAMSV_QPD,
	.hdr_type = HDR_SUPPORT_STAGGER_FDOL|HDR_SUPPORT_DCG|HDR_SUPPORT_LBMF,
	.saturation_info = &imgsensor_saturation_info,
	.seamless_switch_support = TRUE,
	.temperature_support = TRUE,
	.g_temp = get_sensor_temperature,
	.g_gain2reg = get_gain2reg,
	.g_cali = get_sensor_cali,
	.s_gph = set_group_hold,

	.reg_addr_stream = 0x0100,
	.reg_addr_mirror_flip = 0x0101,
	.reg_addr_exposure ={
			{0x0202, 0x0203}, //Short exposure
			{0x0202, 0x0203},
			{0x0226, 0x0227}, //Long exposure
	},
	.long_exposure_support = TRUE,
	.reg_addr_exposure_lshift = 0x0704,
	.reg_addr_ana_gain = {
			{0x0204, 0x0205}, //Short Gain
			{0x0204, 0x0205},
			{0x0206, 0x0207}, //Long Gain
	},
	.reg_addr_frame_length = {0x0340, 0x0341},
	.reg_addr_frame_length_in_lut = {
			{0x0E14, 0x0E15},  /* LUT_A_FRM_LENGTH_LINES */
			{0x0E20, 0x0E21},  /* LUT_B_FRM_LENGTH_LINES */
	},
	.reg_addr_temp_en = PARAM_UNDEFINED,
	.reg_addr_temp_read = 0x0020,
	.reg_addr_auto_extend = PARAM_UNDEFINED,
	.reg_addr_frame_count = 0x0005,
	.reg_addr_exposure_in_lut = {
			{0x0E10, 0x0E11}, //LUT_A_COARSE_INTEG_TIME
			{0x0E1C, 0x0E1D}, //LUT_B_COARSE_INTEG_TIME
	},

	.reg_addr_ana_gain_in_lut = {
			{0x0E12, 0x0E13}, //LUT_A_ANA_GAIN_GLOBAL
			{0x0E1E, 0x0E1F}, //LUT_B_ANA_GAIN_GLOBAL
	},
	// .init_setting_table = konkauwide_sensor_init_setting,
	// .init_setting_len =  ARRAY_SIZE(konkauwide_sensor_init_setting),
	.mode = mode_struct,
	.sensor_mode_num = ARRAY_SIZE(mode_struct),
	.list = feature_control_list,
	.list_len = ARRAY_SIZE(feature_control_list),
	.chk_s_off_sta = 1,
	.chk_s_off_end = 0,
	.checksum_value = 0x350174bc,

	.oplus_notify_chg_flag = false,
};

static struct subdrv_ops ops = {
	.get_id = get_imgsensor_id,
	.init_ctx = init_ctx,
	.open = open,
	.get_info = common_get_info,
	.get_resolution = common_get_resolution,
	.control = common_control,
	.feature_control = common_feature_control,
	.close = common_close,
	.get_frame_desc = common_get_frame_desc,
	.get_temp = common_get_temp,
	.get_csi_param = common_get_csi_param,
	.update_sof_cnt = common_update_sof_cnt,
};

static struct subdrv_pw_seq_entry pw_seq[] = {
	{HW_ID_MCLK, {24}, 2000},
	{HW_ID_RST, {0}, 1000},
	{HW_ID_DOVDD, {1800000, 1800000}, 1000},
	{HW_ID_AVDD, {2204000, 2204000}, 1000},
	{HW_ID_DVDD, {1000000, 1000000}, 0},
	{HW_ID_AFVDD, {2804000, 2804000}, 0},
	{HW_ID_RST, {1}, 2000},
	{HW_ID_MCLK_DRIVING_CURRENT, {4}, 10000},
};

const struct subdrv_entry konkauwide_mipi_raw_entry = {
	.name = "konkauwide_mipi_raw",
	.id = KONKAUWIDE_SENSOR_ID,
	.pw_seq = pw_seq,
	.pw_seq_cnt = ARRAY_SIZE(pw_seq),
	.ops = &ops,
};

/* FUNCTION */

static int get_sensor_temperature(void *arg)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;
	u8 temperature = 0;
	int temperature_convert = 0;

	temperature = subdrv_i2c_rd_u8(ctx, ctx->s_ctx.reg_addr_temp_read);

	if (temperature < 0x50)
		temperature_convert = temperature;
	else if (temperature < 0x80)
		temperature_convert = 80;
	else if (temperature < 0xED)
		temperature_convert = -20;
	else
		temperature_convert = (char)temperature;

	DRV_LOG(ctx, "temperature: %d degrees\n", temperature_convert);
	return temperature_convert;
}

static void konkauwide_set_dummy(struct subdrv_ctx *ctx)
{
	bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);

	if (gph)
		ctx->s_ctx.s_gph((void *)ctx, 1);
	if (ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_LBMF)
		konkauwide_write_frame_length_in_lut(ctx, ctx->frame_length, ctx->frame_length_in_lut);
	else
		write_frame_length(ctx, ctx->frame_length);
	if (gph)
		ctx->s_ctx.s_gph((void *)ctx, 0);

	commit_i2c_buffer(ctx);
}

static void konkauwide_set_max_framerate(struct subdrv_ctx *ctx, UINT16 framerate,
			kal_bool min_framelength_en)
{

	kal_uint32 frame_length = ctx->frame_length;

	DRV_LOG(ctx, "framerate = %d, min framelength should enable %d\n",
		framerate, min_framelength_en);

	frame_length = ctx->pclk / framerate * 10 / ctx->line_length;

	if (frame_length >= ctx->min_frame_length)
		ctx->frame_length = frame_length;
	else
		ctx->frame_length = ctx->min_frame_length;

	ctx->dummy_line =
		ctx->frame_length - ctx->min_frame_length;

	if (ctx->frame_length > ctx->max_frame_length) {
		ctx->frame_length = ctx->max_frame_length;

		ctx->dummy_line =
			ctx->frame_length - ctx->min_frame_length;
	}
	if (min_framelength_en)
		ctx->min_frame_length = ctx->frame_length;

	konkauwide_set_dummy(ctx);
}

static void konkauwide_set_shutter_frame_length_convert(struct subdrv_ctx *ctx, u32 *shutter, u32 frame_length, bool auto_extend_en)
{
	kal_uint16 realtime_fps = 0;
	kal_int32 dummy_line = 0;
	u32 l_shutter = 0;
	u16 l_shift = 0;
	u32 fll = 0;

	ctx->exposure[0] = *shutter;

	/* Change frame time */
	if (frame_length > 1)
		dummy_line = frame_length - ctx->frame_length;

	ctx->frame_length = ctx->frame_length + dummy_line;

	if (ctx->exposure[0] > ctx->frame_length - ctx->s_ctx.exposure_margin)
		ctx->frame_length = ctx->exposure[0] + ctx->s_ctx.exposure_margin;

	if (ctx->frame_length > ctx->max_frame_length)
		ctx->frame_length = ctx->max_frame_length;

	ctx->exposure[0] = (ctx->exposure[0] < ctx->s_ctx.exposure_min)
			? ctx->s_ctx.exposure_min : ctx->exposure[0];

	if (ctx->autoflicker_en) {
		realtime_fps = ctx->pclk / ctx->line_length * 10 / ctx->frame_length;
		if (realtime_fps > 592 && realtime_fps <= 607) {
			konkauwide_set_max_framerate(ctx, 592, 0);
		} else if (realtime_fps > 296 && realtime_fps <= 305) {
			konkauwide_set_max_framerate(ctx, 296, 0);
		} else if (realtime_fps > 246 && realtime_fps <= 253) {
			konkauwide_set_max_framerate(ctx, 246, 0);
		} else if (realtime_fps > 236 && realtime_fps <= 243) {
			konkauwide_set_max_framerate(ctx, 236, 0);
		} else if (realtime_fps > 146 && realtime_fps <= 153) {
			konkauwide_set_max_framerate(ctx, 146, 0);
		} else {
			/* Extend frame length */
			subdrv_i2c_wr_u16(ctx, 0x0340, ctx->frame_length);
		}
	} else {
		/* Extend frame length */
		subdrv_i2c_wr_u16(ctx, 0x0340, ctx->frame_length);
	}

	if (ctx->exposure[0] >= 0xFFF0) {  // need to modify line_length & PCLK
		bNeedSetNormalMode = TRUE;

		if (ctx->exposure[0] > ctx->s_ctx.exposure_max) {
			DRV_LOG(ctx, "shutter(%d) > exposure_max(%d), set shutter = exposure_max\n",
				ctx->exposure[0], ctx->s_ctx.exposure_max);
			ctx->exposure[0] = ctx->s_ctx.exposure_max;
		}

		for (l_shift = 1; l_shift <= 10; l_shift++) {
			l_shutter = ((ctx->exposure[0] - 1) >> l_shift) + 1;
			if (l_shutter
				< (ctx->s_ctx.frame_length_max - ctx->s_ctx.exposure_margin))
				break;
		}

		fll = l_shutter + 0x0002;  // 1st framelength

		subdrv_i2c_wr_u16(ctx, 0x0340, fll & 0xFFFF);  // Framelength
		subdrv_i2c_wr_u16(ctx, 0x0202, l_shutter & 0xFFFF);  //shutter
		subdrv_i2c_wr_u16(ctx, 0x0702, l_shift << 8);
		subdrv_i2c_wr_u16(ctx, 0x0704, l_shift << 8);
		DRV_LOG(ctx, "set long time exposure shutter(%d), frame_length(%d) l_shutter(%d) l_shift(%d)\n",
			ctx->exposure[0], fll, l_shutter, l_shift);
	} else {
		if (bNeedSetNormalMode) {
			DRV_LOG(ctx, "exit long shutter\n");
			subdrv_i2c_wr_u16(ctx, 0x0702, 0x0000);
			subdrv_i2c_wr_u16(ctx, 0x0704, 0x0000);
			bNeedSetNormalMode = FALSE;
		}

		subdrv_i2c_wr_u16(ctx, 0x0340, ctx->frame_length);
		subdrv_i2c_wr_u16(ctx, 0x0202, ctx->exposure[0]);
	}

	DRV_LOG(ctx, "Exit! shutter =%d, framelength =%d/%d, dummy_line=%d, auto_extend=%d\n",
		ctx->exposure[0], ctx->frame_length, frame_length, dummy_line, subdrv_i2c_rd_u16(ctx, 0x0350));
}	/* set_shutter_frame_length */

static int konkauwide_set_shutter_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	DRV_LOG(ctx, "shutter:%u, frame_length:%u\n", (u32)(*para), (u32) (*(para + 1)));
	konkauwide_lens_pos_writeback(ctx);
	konkauwide_set_shutter_frame_length_convert(ctx, (u32 *)para, (u32) (*(para + 1)), (u16) (*(para + 2)));
	return 0;
}

static void konkauwide_write_shutter(struct subdrv_ctx *ctx)
{
	kal_uint16 realtime_fps = 0;

	u32 l_shutter = 0;
	u16 l_shift = 0;
	u32 fll = 0;

	DRV_LOG(ctx, "===brad shutter:%d\n", ctx->exposure[0]);

	if (ctx->exposure[0] > ctx->min_frame_length - ctx->s_ctx.exposure_margin) {
		ctx->frame_length = ctx->exposure[0] + ctx->s_ctx.exposure_margin;
	} else {
		ctx->frame_length = ctx->min_frame_length;
	}
	if (ctx->frame_length > ctx->max_frame_length) {
		ctx->frame_length = ctx->max_frame_length;
	}

	if (ctx->exposure[0] < ctx->s_ctx.exposure_min) {
		ctx->exposure[0] = ctx->s_ctx.exposure_min;
	}

	if (ctx->autoflicker_en) {
		realtime_fps = ctx->pclk / ctx->line_length * 10 / ctx->frame_length;
		if (realtime_fps > 592 && realtime_fps <= 607) {
			konkauwide_set_max_framerate(ctx, 592, 0);
		} else if (realtime_fps > 296 && realtime_fps <= 305) {
			konkauwide_set_max_framerate(ctx, 296, 0);
		} else if (realtime_fps > 246 && realtime_fps <= 253) {
			konkauwide_set_max_framerate(ctx, 246, 0);
		} else if (realtime_fps > 236 && realtime_fps <= 243) {
			konkauwide_set_max_framerate(ctx, 236, 0);
		} else if (realtime_fps > 146 && realtime_fps <= 153) {
			konkauwide_set_max_framerate(ctx, 146, 0);
		} else {
			// Extend frame length
			subdrv_i2c_wr_u16(ctx, 0x0340, ctx->frame_length);
		}
	} else {
		// Extend frame length
		subdrv_i2c_wr_u16(ctx, 0x0340, ctx->frame_length);
	}

	if (ctx->exposure[0] >= 0xFFF0) {  // need to modify line_length & PCLK
		bNeedSetNormalMode = TRUE;

		if (ctx->exposure[0] >= ctx->s_ctx.exposure_max) {
			DRV_LOG(ctx, "shutter(%d) > exposure_max(%d), set shutter = exposure_max\n",
				ctx->exposure[0], ctx->s_ctx.exposure_max);
			ctx->exposure[0] = ctx->s_ctx.exposure_max;
		}

		for (l_shift = 1; l_shift <= 10; l_shift++) {
			l_shutter = ((ctx->exposure[0] - 1) >> l_shift) + 1;
			if (l_shutter
				< (ctx->s_ctx.frame_length_max - ctx->s_ctx.exposure_margin))
				break;
		}

		fll = l_shutter + 0x0002;  // 1st framelength

		subdrv_i2c_wr_u16(ctx, 0x0340, fll & 0xFFFF);  // Framelength
		subdrv_i2c_wr_u16(ctx, 0x0202, l_shutter & 0xFFFF);  //shutter
		subdrv_i2c_wr_u16(ctx, 0x0702, l_shift << 8);
		subdrv_i2c_wr_u16(ctx, 0x0704, l_shift << 8);
		DRV_LOG(ctx, "set long time exposure shutter(%d), frame_length(%d) l_shutter(%d) l_shift(%d)\n",
			ctx->exposure[0], fll, l_shutter, l_shift);
	} else {
		if (bNeedSetNormalMode) {
			DRV_LOG(ctx, "exit long shutter\n");
			subdrv_i2c_wr_u16(ctx, 0x0702, 0x0000);
			subdrv_i2c_wr_u16(ctx, 0x0704, 0x0000);
			bNeedSetNormalMode = FALSE;
		}

		subdrv_i2c_wr_u16(ctx, 0x0340, ctx->frame_length);
		subdrv_i2c_wr_u16(ctx, 0x0202, ctx->exposure[0]);
	}
	DRV_LOG(ctx, "shutter =%d, framelength =%d\n", ctx->exposure[0], ctx->frame_length);
}	/*	write_shutter  */

static void konkauwide_set_shutter_convert(struct subdrv_ctx *ctx, u32 shutter)
{
	DRV_LOG(ctx, "set_shutter shutter =%d\n", shutter);
	ctx->exposure[0] = shutter;

	konkauwide_write_shutter(ctx);
}

static int konkauwide_set_shutter(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u32* feature_data = (u32*)para;
	u32 shutter = *feature_data;

	DRV_LOG(ctx, "set_shutter shutter =0x%x\n", shutter);
	konkauwide_set_shutter_convert(ctx, shutter);
	return 0;
}

static void streaming_ctrl(struct subdrv_ctx *ctx, bool enable)
{
	struct adaptor_ctx *_adaptor_ctx = NULL;
	struct v4l2_subdev *sd = NULL;

	DRV_LOG(ctx, "E! enable:%u\n", enable);

	if (ctx->i2c_client)
		sd = i2c_get_clientdata(ctx->i2c_client);
	if (sd)
		_adaptor_ctx = to_ctx(sd);
	if (!_adaptor_ctx) {
		DRV_LOGE(ctx, "null _adaptor_ctx\n");
		return;
	}

	check_current_scenario_id_bound(ctx);

	if (ctx->s_ctx.aov_sensor_support && ctx->s_ctx.mode[ctx->current_scenario_id].aov_mode) {
		DRV_LOG_MUST(ctx,
			"stream ctrl implement on scp side!(sid:%u)\n",
			ctx->current_scenario_id);
		ctx->is_streaming = enable;
		DRV_LOG_MUST(ctx, "enable:%u\n", enable);
		return;
	}

	if (enable) {
		/* MCSS low power mode update para */
		if (ctx->s_ctx.mcss_update_subdrv_para != NULL)
			ctx->s_ctx.mcss_update_subdrv_para((void *) ctx, ctx->current_scenario_id);
		/* MCSS register init */
		if (ctx->s_ctx.mcss_init != NULL)
			ctx->s_ctx.mcss_init((void *) ctx);

		if (ctx->s_ctx.chk_s_off_sta) {
			DRV_LOG(ctx, "check_stream_off before stream on");
			check_stream_off(ctx);
		}
		konkauwide_set_dummy(ctx);
		subdrv_ixc_wr_u8(ctx, ctx->s_ctx.reg_addr_stream, 0x01);
		ctx->stream_ctrl_start_time = ktime_get_boottime_ns();
		if (ctx->s_ctx.custom_stream_ctrl_delay)
			mdelay(ctx->s_ctx.custom_stream_ctrl_delay);
	} else {
		check_stream_on(ctx);  // check_stream_on before stream off
		subdrv_ixc_wr_u8(ctx, ctx->s_ctx.reg_addr_stream, 0x00);
		if (ctx->s_ctx.reg_addr_fast_mode && ctx->fast_mode_on) {
			ctx->fast_mode_on = FALSE;
			ctx->ref_sof_cnt = 0;
			DRV_LOG(ctx, "seamless_switch disabled.");
			set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_fast_mode, 0x00);
			commit_i2c_buffer(ctx);
		}
		memset(ctx->exposure, 0, sizeof(ctx->exposure));
		memset(ctx->ana_gain, 0, sizeof(ctx->ana_gain));
		ctx->autoflicker_en = FALSE;
		ctx->extend_frame_length_en = 0;
		ctx->is_seamless = 0;
		if (ctx->s_ctx.chk_s_off_end)
			check_stream_off(ctx);
		ctx->stream_ctrl_start_time = 0;
		ctx->stream_ctrl_end_time = 0;

		ctx->mcss_init_info.enable_mcss = 0;
		if (ctx->s_ctx.mcss_init != NULL)
			ctx->s_ctx.mcss_init((void *) ctx); // disable MCSS
	}
	ctx->sof_no = 0;
	ctx->is_streaming = enable;
	DRV_LOG(ctx, "X! enable:%u\n", enable);
}

static int konkauwide_streaming_resume(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	DRV_LOG(ctx, "SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%u\n", *(u32 *)para);
	if (*(u32 *)para)
	konkauwide_set_shutter_convert(ctx, *(u32 *)para);
	streaming_ctrl(ctx, true);
	return 0;
}

static int konkauwide_streaming_suspend(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	DRV_LOG(ctx, "streaming control para:%d\n", *para);
	streaming_ctrl(ctx, false);
	return 0;
}

static unsigned int read_konkauwide_eeprom_info(struct subdrv_ctx *ctx, kal_uint16 meta_id,
	BYTE *data, int size)
{
	kal_uint16 addr;
	int readsize;

	if (meta_id != konkauwide_eeprom_info[meta_id].meta)
		return -1;

	if (size != konkauwide_eeprom_info[meta_id].size)
		return -1;

	addr = konkauwide_eeprom_info[meta_id].start;
	readsize = konkauwide_eeprom_info[meta_id].size;

	if (!read_cmos_eeprom_p8(ctx, addr, data, readsize)) {
		DRV_LOGE(ctx, "read meta_id(%d) failed", meta_id);
	}

	return 0;
}

static struct eeprom_addr_table_struct oplus_eeprom_addr_table = {
	.i2c_read_id = 0xA1,
	.i2c_write_id = 0xA0,

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

static int get_eeprom_common_data(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	memcpy(para, (u8*)(&oplus_eeprom_info), sizeof(oplus_eeprom_info));
	*len = sizeof(oplus_eeprom_info);
	return 0;
}

static kal_uint16 read_cmos_eeprom_8(struct subdrv_ctx *ctx, kal_uint16 addr)
{
    kal_uint16 get_byte = 0;

    adaptor_i2c_rd_u8(ctx->i2c_client, KONKAUWIDE_EEPROM_READ_ID >> 1, addr, (u8 *)&get_byte);
    return get_byte;
}

#ifdef WRITE_DATA_MAX_LENGTH
#undef WRITE_DATA_MAX_LENGTH
#endif
#define   WRITE_DATA_MAX_LENGTH     (32)
static kal_int32 table_write_eeprom_30Bytes(struct subdrv_ctx *ctx,
        kal_uint16 addr, kal_uint8 *para, kal_uint32 len)
{
	kal_int32 ret = ERROR_NONE;
    ret = adaptor_i2c_wr_p8(ctx->i2c_client, KONKAUWIDE_EEPROM_WRITE_ID >> 1,
            addr, para, len);

	return ret;
}

static kal_int32 write_eeprom_protect(struct subdrv_ctx *ctx, kal_uint16 enable)
{
    kal_int32 ret = ERROR_NONE;
    kal_uint16 reg = 0xE000;
    if (enable) {
        adaptor_i2c_wr_u8(ctx->i2c_client, KONKAUWIDE_EEPROM_WRITE_ID >> 1, reg, 0xA1);
    }
    else {
        adaptor_i2c_wr_u8(ctx->i2c_client, KONKAUWIDE_EEPROM_WRITE_ID >> 1, reg, 0xA0);
    }

    return ret;
}

static kal_int32 write_Module_data(struct subdrv_ctx *ctx,
    ACDK_SENSOR_ENGMODE_STEREO_STRUCT * pStereodata)
{
    kal_int32  ret = ERROR_NONE;
    kal_uint16 data_base, data_length;
    kal_uint32 idx, idy;
    kal_uint8 *pData;
    kal_uint32 checksum = 0;
    UINT32 i = 0;
    kal_uint16 offset = 0;
    if(pStereodata != NULL) {
        LOG_INF("SET_SENSOR_OTP: 0x%x %d 0x%x %d\n",
            pStereodata->uSensorId,
            pStereodata->uDeviceId,
            pStereodata->baseAddr,
            pStereodata->dataLength);

        data_base = pStereodata->baseAddr;
        data_length = pStereodata->dataLength;
        pData = pStereodata->uData;
        for(i = 0; i < pStereodata->dataLength; i++) {
            checksum += pData[i];
        }
        pData[data_length] = 0x01;
        pData[data_length + 1] = checksum % 255;
        data_length = data_length + 2;
        offset = ALIGN(data_base, WRITE_DATA_MAX_LENGTH) - data_base;
        if (offset > data_length) {
            offset = data_length;
        }
        if ((pStereodata->uSensorId == KONKAUWIDE_SENSOR_ID) && (data_length - 2 == CALI_DATA_SLAVE_LENGTH)
            && (data_base == KONKAUWIDE_STEREO_START_ADDR)) {
            LOG_INF("Write: %x %x %x %x\n", pData[0], pData[39], pData[40], pData[1556]);
            /* close write protect */
            write_eeprom_protect(ctx, 0);
            msleep(6);
            if (offset > 0) {
                ret = table_write_eeprom_30Bytes(ctx, data_base, &pData[0], offset);
                if (ret != ERROR_NONE) {
                    LOG_INF("write_eeprom error: offset\n");
                    /* open write protect */
                    write_eeprom_protect(ctx, 1);
                    msleep(6);
                    return -1;
                }
                msleep(6);
                data_base += offset;
                data_length -= offset;
                pData += offset;
            }
            idx = data_length/WRITE_DATA_MAX_LENGTH;
            idy = data_length%WRITE_DATA_MAX_LENGTH;
            for (i = 0; i < idx; i++ ) {
                ret = table_write_eeprom_30Bytes(ctx, (data_base+WRITE_DATA_MAX_LENGTH*i),
                    &pData[WRITE_DATA_MAX_LENGTH*i], WRITE_DATA_MAX_LENGTH);
                if (ret != ERROR_NONE) {
                    LOG_INF("write_eeprom error: i= %d\n", i);
                    /* open write protect */
                    write_eeprom_protect(ctx, 1);
                    msleep(6);
                    return -1;
                }
                msleep(6);
            }
            ret = table_write_eeprom_30Bytes(ctx, (data_base+WRITE_DATA_MAX_LENGTH*idx),
                &pData[WRITE_DATA_MAX_LENGTH*idx], idy);
            if (ret != ERROR_NONE) {
                LOG_INF("write_eeprom error: idx= %d idy= %d\n", idx, idy);
                /* open write protect */
                write_eeprom_protect(ctx, 1);
                msleep(6);
                return -1;
            }
            msleep(6);
            /* open write protect */
            write_eeprom_protect(ctx, 1);
            msleep(6);
            LOG_INF("com_0:0x%x\n", read_cmos_eeprom_8(ctx, data_base));
            LOG_INF("com_39:0x%x\n", read_cmos_eeprom_8(ctx, data_base+39));
            LOG_INF("innal_40:0x%x\n", read_cmos_eeprom_8(ctx, data_base+40));
            LOG_INF("innal_1556:0x%x\n", read_cmos_eeprom_8(ctx, data_base+1556));
            LOG_INF("write_Module_data Write end\n");
        } else if ((pStereodata->uSensorId == KONKAUWIDE_SENSOR_ID) && (data_length < AESYNC_DATA_LENGTH_TOTAL)
            && (data_base == KONKAUWIDE_AESYNC_START_ADDR)) {
            LOG_INF("write main aesync: %x %x %x %x %x %x %x %x\n", pData[0], pData[1],
                pData[2], pData[3], pData[4], pData[5], pData[6], pData[7]);
            /* close write protect */
            write_eeprom_protect(ctx, 0);
            msleep(6);
            if (offset > 0) {
                ret = table_write_eeprom_30Bytes(ctx, data_base, &pData[0], offset);
                if (ret != ERROR_NONE) {
                    LOG_INF("write_eeprom error: offset\n");
                    /* open write protect */
                    write_eeprom_protect(ctx, 1);
                    msleep(6);
                    return -1;
                }
                msleep(6);
                data_base += offset;
                data_length -= offset;
                pData += offset;
            }
            idx = data_length/WRITE_DATA_MAX_LENGTH;
            idy = data_length%WRITE_DATA_MAX_LENGTH;
            for (i = 0; i < idx; i++ ) {
                ret = table_write_eeprom_30Bytes(ctx, (data_base+WRITE_DATA_MAX_LENGTH*i),
                    &pData[WRITE_DATA_MAX_LENGTH*i], WRITE_DATA_MAX_LENGTH);
                if (ret != ERROR_NONE) {
                    LOG_INF("write_eeprom error: i= %d\n", i);
                    /* open write protect */
                    write_eeprom_protect(ctx, 1);
                    msleep(6);
                    return -1;
                }
                msleep(6);
            }
            ret = table_write_eeprom_30Bytes(ctx, (data_base+WRITE_DATA_MAX_LENGTH*idx),
                &pData[WRITE_DATA_MAX_LENGTH*idx], idy);
            if (ret != ERROR_NONE) {
                LOG_INF("write_eeprom error: idx= %d idy= %d\n", idx, idy);
                /* open write protect */
                write_eeprom_protect(ctx, 1);
                msleep(6);
                return -1;
            }
            msleep(6);
            /* open write protect */
            write_eeprom_protect(ctx, 1);
            msleep(6);
            LOG_INF("readback main aesync: %x %x %x %x %x %x %x %x\n",
                read_cmos_eeprom_8(ctx, KONKAUWIDE_AESYNC_START_ADDR),
                read_cmos_eeprom_8(ctx, KONKAUWIDE_AESYNC_START_ADDR+1),
                read_cmos_eeprom_8(ctx, KONKAUWIDE_AESYNC_START_ADDR+2),
                read_cmos_eeprom_8(ctx, KONKAUWIDE_AESYNC_START_ADDR+3),
                read_cmos_eeprom_8(ctx, KONKAUWIDE_AESYNC_START_ADDR+4),
                read_cmos_eeprom_8(ctx, KONKAUWIDE_AESYNC_START_ADDR+5),
                read_cmos_eeprom_8(ctx, KONKAUWIDE_AESYNC_START_ADDR+6),
                read_cmos_eeprom_8(ctx, KONKAUWIDE_AESYNC_START_ADDR+7));
            LOG_INF("AESync write_Module_data Write end\n");
        } else {
            LOG_INF("Invalid Sensor id:0x%x write eeprom\n", pStereodata->uSensorId);
            return -1;
        }
    } else {
        LOG_INF("konkauwide write_Module_data pStereodata is null\n");
        return -1;
    }
    return ret;
}

static int konkauwide_set_eeprom_calibration(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
    int ret = ERROR_NONE;
    ret = write_Module_data(ctx, (ACDK_SENSOR_ENGMODE_STEREO_STRUCT *)(para));
    if (ret != ERROR_NONE) {
        *len = (u32)-1; /*write eeprom failed*/
        LOG_INF("ret=%d\n", ret);
    }
    return 0;
}

static int konkauwide_get_eeprom_calibration(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	if(*len > CALI_DATA_SLAVE_LENGTH) {
		*len = CALI_DATA_SLAVE_LENGTH;
	}
	read_konkauwide_eeprom_info(ctx, EEPROM_META_STEREO_MW_MAIN_DATA,
			(BYTE *)para, *len);
	return 0;
}

static bool read_cmos_eeprom_p8(struct subdrv_ctx *ctx, kal_uint16 addr,
                    BYTE *data, int size)
{
	if (adaptor_i2c_rd_p8(ctx->i2c_client, KONKAUWIDE_EEPROM_READ_ID >> 1,
			addr, data, size) < 0) {
		return false;
	}
	return true;
}

static int konkauwide_get_otp_qcom_pdaf_data(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u32 *feature_return_para_32 = (u32 *)para;

	read_cmos_eeprom_p8(ctx, OTP_QCOM_PDAF_DATA_START_ADDR, otp_qcom_pdaf_data, OTP_QCOM_PDAF_DATA_LENGTH);

	memcpy(feature_return_para_32, (UINT32 *)otp_qcom_pdaf_data, sizeof(otp_qcom_pdaf_data));
	*len = sizeof(otp_qcom_pdaf_data);

	return 0;
}

static int konkauwide_set_awb_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len) {

	struct SET_SENSOR_AWB_GAIN *awb_gain = (struct SET_SENSOR_AWB_GAIN *)para;

	adaptor_i2c_wr_u16(ctx->i2c_client, ctx->i2c_write_id >> 1, 0x0D82, awb_gain->ABS_GAIN_R * 2); //red 1024(1x)
	adaptor_i2c_wr_u16(ctx->i2c_client, ctx->i2c_write_id >> 1, 0x0D86, awb_gain->ABS_GAIN_B * 2); //blue

	LOG_INF("[test] ABS_GAIN_GR(%d) ABS_GAIN_R(%d) ABS_GAIN_B(%d) ABS_GAIN_GB(%d)", awb_gain->ABS_GAIN_GR, awb_gain->ABS_GAIN_R, awb_gain->ABS_GAIN_B, awb_gain->ABS_GAIN_GB);
	LOG_INF("[test] 0x0D82(red) = (0x%x)", subdrv_i2c_rd_u16(ctx, 0x0D82));
	LOG_INF("[test] 0x0D84(green) = (0x%x)", subdrv_i2c_rd_u16(ctx, 0x0D84));
	LOG_INF("[test] 0x0D86(blue) = (0x%x)", subdrv_i2c_rd_u16(ctx, 0x0D86));
	return 0;
}

static void read_otp_info(struct subdrv_ctx *ctx)
{
	DRV_LOGE(ctx, "jn1 read_otp_info begin\n");
	read_cmos_eeprom_p8(ctx, 0, otp_data_checksum, OTP_SIZE);
	DRV_LOGE(ctx, "jn1 read_otp_info end\n");
}

static int konkauwide_get_otp_checksum_data(struct subdrv_ctx *ctx, u8 *para, u32 *len)
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

static int konkauwide_check_sensor_id(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	get_imgsensor_id(ctx, (u32 *)para);
	return 0;
}

static int get_imgsensor_id(struct subdrv_ctx *ctx, u32 *sensor_id)
{
	u8 i = 0;
	u8 retry = GET_SENSOR_ID_RETRY_CNT;
	static bool first_read = TRUE;
	u32 eeprom_time_year = 0, eeprom_time_m_d = 0;
	u32 addr_h = ctx->s_ctx.reg_addr_sensor_id.addr[0];
	u32 addr_l = ctx->s_ctx.reg_addr_sensor_id.addr[1];
	u32 addr_ll = ctx->s_ctx.reg_addr_sensor_id.addr[2];

	while (ctx->s_ctx.i2c_addr_table[i] != 0xFF) {
		ctx->i2c_write_id = ctx->s_ctx.i2c_addr_table[i];
		do {
			*sensor_id = (subdrv_i2c_rd_u8(ctx, addr_h) << 8) |
				subdrv_i2c_rd_u8(ctx, addr_l);
			if (addr_ll)
				*sensor_id = ((*sensor_id) << 8) | subdrv_i2c_rd_u8(ctx, addr_ll);
			DRV_LOG(ctx, "i2c_write_id(0x%x) sensor_id(0x%x/0x%x)\n",
				ctx->i2c_write_id, *sensor_id, ctx->s_ctx.sensor_id);
			if (*sensor_id == 0x38E5) {
				*sensor_id = ctx->s_ctx.sensor_id;
				if (first_read) {
					read_eeprom_common_data(ctx, &oplus_eeprom_info, oplus_eeprom_addr_table);

					u8 af_code[AF_CODE_SIZE] = {0};
					read_konkauwide_eeprom_info(ctx, EEPROM_META_AF_CODE, (BYTE *)af_code, AF_CODE_SIZE);
					g_af_code_macro = af_code[0] | ((u16)af_code[1] << 8);
					g_af_code_infinity = af_code[2] | ((u16)af_code[3] << 8);

					first_read = FALSE;

					msg_buf = kmalloc(MAX_BURST_LEN, GFP_KERNEL);
					if(!msg_buf) {
						LOG_INF("boot stage, malloc msg_buf error");
					}
				}
				eeprom_time_year = (read_cmos_eeprom_8(ctx, 0x0004) << 8) | read_cmos_eeprom_8(ctx, 0x0005);
				eeprom_time_m_d = (read_cmos_eeprom_8(ctx, 0x0003) << 8) | read_cmos_eeprom_8(ctx, 0x0002);
				// sensor with eeprom data since 2024/01/27
				module_flag = (eeprom_time_year > 0x1814) || ((eeprom_time_year == 0x1814) && (eeprom_time_m_d >= 0x11B));
				return ERROR_NONE;
			}
			DRV_LOG(ctx, "Read sensor id fail. i2c_write_id: 0x%x\n", ctx->i2c_write_id);
			DRV_LOG(ctx, "sensor_id = 0x%x, ctx->s_ctx.sensor_id = 0x%x\n",
				*sensor_id, ctx->s_ctx.sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = GET_SENSOR_ID_RETRY_CNT;
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
	u32 module_info = 0;
	u64 time_boot_begin = 0;

	/* get sensor id */
	if (get_imgsensor_id(ctx, &sensor_id) != ERROR_NONE)
		return ERROR_SENSOR_CONNECT_FAIL;
	subdrv_i2c_wr_u16(ctx, 0xFCFC, 0x4000);
	module_info = subdrv_i2c_rd_u16(ctx, 0x0010);
	DRV_LOG(ctx, "write init setting +");
	if ((ctx->power_on_profile_en != NULL) && (*ctx->power_on_profile_en))
		time_boot_begin = ktime_get_boottime_ns();


	if ((module_info | 0x00FF) == 0x06FF){
		subdrv_i2c_wr_regs_u16(ctx, konkauwide_sensor_init_pre_setting_short, ARRAY_SIZE(konkauwide_sensor_init_pre_setting_short));
		mdelay(5);
		konkauwide_i2c_burst_wr_regs_u16(ctx, konkauwide_sensor_init_setting_short, ARRAY_SIZE(konkauwide_sensor_init_setting_short));
		DRV_LOG(ctx, "write init setting (short)-");
		ctx->s_ctx.init_setting_len = ARRAY_SIZE(konkauwide_sensor_init_pre_setting_short) + ARRAY_SIZE(konkauwide_sensor_init_setting_short);
	} else {
		subdrv_i2c_wr_regs_u16(ctx, konkauwide_sensor_init_pre_setting, ARRAY_SIZE(konkauwide_sensor_init_pre_setting));
		mdelay(5);
		konkauwide_i2c_burst_wr_regs_u16(ctx, konkauwide_sensor_init_setting, ARRAY_SIZE(konkauwide_sensor_init_setting));
		ctx->s_ctx.init_setting_len = ARRAY_SIZE(konkauwide_sensor_init_pre_setting) + ARRAY_SIZE(konkauwide_sensor_init_setting);
		DRV_LOG(ctx, "write init setting (long)-");
	}


	if ((ctx->power_on_profile_en != NULL) && (*ctx->power_on_profile_en)) {
		ctx->sensor_pw_on_profile.i2c_init_period = ktime_get_boottime_ns() - time_boot_begin - 5000;

		ctx->sensor_pw_on_profile.i2c_init_table_len =
			ARRAY_SIZE(konkauwide_sensor_init_pre_setting) + ARRAY_SIZE(konkauwide_sensor_init_setting);
	}
	DRV_LOG_MUST(ctx, "X: size:%u, time(us):%lld\n", ctx->sensor_pw_on_profile.i2c_init_table_len,
		ctx->sensor_pw_on_profile.i2c_init_period);

	if (ctx->s_ctx.temperature_support && ctx->s_ctx.reg_addr_temp_en)
		subdrv_ixc_wr_u8(ctx, ctx->s_ctx.reg_addr_temp_en, 0x01);
	/* enable mirror or flip */
	set_mirror_flip(ctx, ctx->mirror);

	/* HW GGC*/
	set_sensor_cali(ctx);

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
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;
	if (group_hold_frame_count < 2) {
		DRV_LOGE(ctx, "group_hold_frame_count: %d", group_hold_frame_count);
		group_hold_frame_count++;
		return;
	}

	if (en)
		set_i2c_buffer(ctx, 0x0104, 0x01);
	else
		set_i2c_buffer(ctx, 0x0104, 0x00);
}

static u16 get_gain2reg(u32 gain)
{
	return gain * 32 / BASEGAIN;
}

static int konkauwide_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	group_hold_frame_count = 0;
	enum SENSOR_SCENARIO_ID_ENUM scenario_id;
	struct mtk_hdr_ae *ae_ctrl = NULL;
	u64 *feature_data = (u64 *)para;
	u32 frame_length_in_lut[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
	u32 exp_cnt = 0;

	if (feature_data == NULL) {
		DRV_LOGE(ctx, "input scenario is null!");
		return ERROR_NONE;
	}
	scenario_id = *feature_data;
	if ((feature_data + 1) != NULL)
		ae_ctrl = (struct mtk_hdr_ae *)((uintptr_t)(*(feature_data + 1)));
	else
		DRV_LOGE(ctx, "no ae_ctrl input");

	check_current_scenario_id_bound(ctx);
	DRV_LOG(ctx, "E: set seamless switch %u %u\n", ctx->current_scenario_id, scenario_id);
	if (!ctx->extend_frame_length_en)
		DRV_LOGE(ctx, "please extend_frame_length before seamless_switch!\n");
	ctx->extend_frame_length_en = FALSE;

	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOGE(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		return ERROR_NONE;
	}
	if (ctx->s_ctx.mode[scenario_id].seamless_switch_group == 0 ||
		ctx->s_ctx.mode[scenario_id].seamless_switch_group !=
			ctx->s_ctx.mode[ctx->current_scenario_id].seamless_switch_group) {
		DRV_LOGE(ctx, "seamless_switch not supported\n");
		return ERROR_NONE;
	}
	if (ctx->s_ctx.mode[scenario_id].seamless_switch_mode_setting_table == NULL) {
		DRV_LOGE(ctx, "Please implement seamless_switch setting\n");
		return ERROR_NONE;
	}

	exp_cnt = ctx->s_ctx.mode[scenario_id].exp_cnt;
	ctx->is_seamless = TRUE;

	subdrv_i2c_wr_u8(ctx, 0x0104, 0x01);
	if (ctx->s_ctx.reg_addr_fast_mode_in_lbmf &&
		(ctx->s_ctx.mode[scenario_id].hdr_mode == HDR_RAW_LBMF ||
		ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_LBMF))
		subdrv_i2c_wr_u8(ctx, ctx->s_ctx.reg_addr_fast_mode_in_lbmf, 0x4);

	update_mode_info(ctx, scenario_id);
	i2c_table_write(ctx,
		ctx->s_ctx.mode[scenario_id].seamless_switch_mode_setting_table,
		ctx->s_ctx.mode[scenario_id].seamless_switch_mode_setting_len);

	if (ae_ctrl) {
		switch (ctx->s_ctx.mode[scenario_id].hdr_mode) {
		case HDR_RAW_STAGGER:
			set_multi_shutter_frame_length(ctx, (u64 *)&ae_ctrl->exposure, exp_cnt, 0);
			set_multi_gain(ctx, (u32 *)&ae_ctrl->gain, exp_cnt);
			break;
		case HDR_RAW_LBMF:
			konkauwide_set_multi_shutter_frame_length_in_lut_convert(ctx,
				(u64 *)&ae_ctrl->exposure, exp_cnt, 0, frame_length_in_lut);
			set_multi_gain_in_lut(ctx, (u32 *)&ae_ctrl->gain, exp_cnt);
			break;
		case HDR_RAW_DCG_RAW:
			konkauwide_set_shutter_convert(ctx, (u32)ae_ctrl->exposure.me_exposure);
			if (ctx->s_ctx.mode[scenario_id].dcg_info.dcg_gain_mode
				== IMGSENSOR_DCG_DIRECT_MODE)
				set_multi_gain(ctx, (u32 *)&ae_ctrl->gain, exp_cnt);
			else
				konkauwide_set_gain_convert(ctx, ae_ctrl->gain.me_gain);
			break;
		default:
			konkauwide_set_shutter_convert(ctx, (u32)ae_ctrl->exposure.le_exposure);
			konkauwide_set_gain_convert(ctx, ae_ctrl->gain.le_gain);
			break;
		}
	}
	subdrv_i2c_wr_u8(ctx, 0x0104, 0x00);

	// ctx->fast_mode_on = TRUE;
	ctx->ref_sof_cnt = ctx->sof_cnt;
	ctx->is_seamless = FALSE;
	DRV_LOG(ctx, "X: set seamless switch done\n");
	return ERROR_NONE;
}

static int konkauwide_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u32 mode = *((u32 *)para);

	if (mode != ctx->test_pattern)
		DRV_LOG(ctx, "mode(%u->%u)\n", ctx->test_pattern, mode);
	/* 1:Solid Color 2:Color Bar 5:Black */
	if (mode) {
		if (mode == 5) {
			subdrv_i2c_wr_u16(ctx, 0x0600, 0x0001); /*black*/
		} else {
			subdrv_i2c_wr_u16(ctx, 0x0600, mode); /*100% Color bar*/
		}
	}
	else if (ctx->test_pattern)
		subdrv_i2c_wr_u16(ctx, 0x0600, 0x0000); /*No pattern*/

	ctx->test_pattern = mode;
	return 0;
}

static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id)
{
	memcpy(&(ctx->s_ctx), &static_ctx, sizeof(struct subdrv_static_ctx));
	subdrv_ctx_init(ctx);
	ctx->i2c_client = i2c_client;
	ctx->i2c_write_id = i2c_write_id;
	return 0;
}

void get_sensor_cali(void* arg)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;

	// struct eeprom_info_struct *info = ctx->s_ctx.eeprom_info;

	/* Probe EEPROM device */
	if (!probe_eeprom(ctx))
		return;

	ctx->is_read_preload_eeprom = 1;
}

static void set_sensor_cali(void *arg)
{
	//struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;
	return;
}

static int konkauwide_set_gain_convert(struct subdrv_ctx *ctx, u32 gain) {
	u16 rg_gain;

	bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);

	/* check boundary of gain */
	gain = max(gain, ctx->s_ctx.ana_gain_min);
	gain = min(gain, ctx->s_ctx.ana_gain_max);
	/* mapping of gain to register value */
	if (ctx->s_ctx.g_gain2reg != NULL)
		rg_gain = ctx->s_ctx.g_gain2reg(gain);
	else
		rg_gain = gain2reg(gain);
	/* restore gain */
	memset(ctx->ana_gain, 0, sizeof(ctx->ana_gain));
	ctx->ana_gain[0] = gain;
	/* group hold start */
	if (gph && !ctx->ae_ctrl_gph_en)
		ctx->s_ctx.s_gph((void *)ctx, 1);
	/* write gain */
	set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_ana_gain[0].addr[0],
		(rg_gain >> 8) & 0xFF);
	set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_ana_gain[0].addr[1],
		rg_gain & 0xFF);
	DRV_LOG(ctx, "gain[0x%x]\n", rg_gain);
	if (gph)
		ctx->s_ctx.s_gph((void *)ctx, 0);
	commit_i2c_buffer(ctx);
	/* group hold end */

	return ERROR_NONE;
}

int konkauwide_set_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64* feature_data = (u64*)para;
	u32 gain = *feature_data;

	konkauwide_set_gain_convert(ctx, gain);

	return 0;
}

static void konkauwide_set_multi_shutter_frame_length(struct subdrv_ctx *ctx,
		u64 *shutters, u16 exp_cnt,	u16 frame_length)
{
	int i = 0;
	u32 fine_integ_line = 0;
	u16 last_exp_cnt = 1;
	u32 calc_fl[3] = {0};
	int readout_diff = 0;
	bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);
	u32 rg_shutters[3] = {0};
	u32 cit_step = 0;

	ctx->frame_length = frame_length ? frame_length : ctx->frame_length;
	if (exp_cnt > ARRAY_SIZE(ctx->exposure)) {
		DRV_LOGE(ctx, "invalid exp_cnt:%u>%lu\n", exp_cnt, ARRAY_SIZE(ctx->exposure));
		exp_cnt = ARRAY_SIZE(ctx->exposure);
	}
	check_current_scenario_id_bound(ctx);

	/* check boundary of shutter */
	for (i = 1; i < ARRAY_SIZE(ctx->exposure); i++)
		last_exp_cnt += ctx->exposure[i] ? 1 : 0;
	fine_integ_line = ctx->s_ctx.mode[ctx->current_scenario_id].fine_integ_line;
	cit_step = ctx->s_ctx.mode[ctx->current_scenario_id].coarse_integ_step;
	for (i = 0; i < exp_cnt; i++) {
		shutters[i] = FINE_INTEG_CONVERT(shutters[i], fine_integ_line);
		shutters[i] = max(shutters[i], ctx->s_ctx.exposure_min);
		shutters[i] = min(shutters[i], ctx->s_ctx.exposure_max);
		if (cit_step)
			shutters[i] = round_up(shutters[i], cit_step);
	}

	/* check boundary of framelength */
	/* - (1) previous se + previous me + current le */
	calc_fl[0] = shutters[0];
	for (i = 1; i < last_exp_cnt; i++)
		calc_fl[0] += ctx->exposure[i];
	calc_fl[0] += ctx->s_ctx.exposure_margin*exp_cnt*exp_cnt;

	/* - (2) current se + current me + current le */
	calc_fl[1] = shutters[0];
	for (i = 1; i < exp_cnt; i++)
		calc_fl[1] += shutters[i];
	calc_fl[1] += ctx->s_ctx.exposure_margin*exp_cnt*exp_cnt;

	/* - (3) readout time cannot be overlapped */
	calc_fl[2] =
		(ctx->s_ctx.mode[ctx->current_scenario_id].readout_length +
		ctx->s_ctx.mode[ctx->current_scenario_id].read_margin);
	if (last_exp_cnt == exp_cnt)
		for (i = 1; i < exp_cnt; i++) {
			readout_diff = ctx->exposure[i] - shutters[i];
			calc_fl[2] += readout_diff > 0 ? readout_diff : 0;
		}
	for (i = 0; i < ARRAY_SIZE(calc_fl); i++)
		ctx->frame_length = max(ctx->frame_length, calc_fl[i]);
	ctx->frame_length =	max(ctx->frame_length, ctx->min_frame_length);
	ctx->frame_length =	min(ctx->frame_length, ctx->s_ctx.frame_length_max);
	/* restore shutter */
	memset(ctx->exposure, 0, sizeof(ctx->exposure));
	for (i = 0; i < exp_cnt; i++)
		ctx->exposure[i] = shutters[i];
	/* exit long exposure if necessary */
	if ((ctx->exposure[0] < 0xFFF0) && bNeedSetNormalMode) {
		DRV_LOG(ctx, "exit long shutter\n");
		subdrv_i2c_wr_u16(ctx, 0x0702, 0x0000);
		subdrv_i2c_wr_u16(ctx, 0x0704, 0x0000);
		bNeedSetNormalMode = FALSE;
	}
	/* group hold start */
	if (gph)
		ctx->s_ctx.s_gph((void *)ctx, 1);
	/* enable auto extend */
	if (ctx->s_ctx.reg_addr_auto_extend)
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_auto_extend, 0x01);
	/* write framelength */
	if (set_auto_flicker(ctx, 0) || frame_length || !ctx->s_ctx.reg_addr_auto_extend)
		write_frame_length(ctx, ctx->frame_length);
	/* write shutter */
	switch (exp_cnt) {
	case 1:
		rg_shutters[0] = shutters[0] / exp_cnt;
		break;
	case 2:
		rg_shutters[0] = shutters[0] / exp_cnt;
		rg_shutters[2] = shutters[1] / exp_cnt;
		break;
	case 3:
		rg_shutters[0] = shutters[0] / exp_cnt;
		rg_shutters[1] = shutters[1] / exp_cnt;
		rg_shutters[2] = shutters[2] / exp_cnt;
		break;
	default:
		break;
	}
	if (ctx->s_ctx.reg_addr_exposure_lshift != PARAM_UNDEFINED)
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_exposure_lshift, 0);
	for (i = 0; i < 3; i++) {
		if (rg_shutters[i]) {
			if (ctx->s_ctx.reg_addr_exposure[i].addr[2]) {
				set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[i].addr[0],
					(rg_shutters[i] >> 16) & 0xFF);
				set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[i].addr[1],
					(rg_shutters[i] >> 8) & 0xFF);
				set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[i].addr[2],
					rg_shutters[i] & 0xFF);
			} else {
				set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[i].addr[0],
					(rg_shutters[i] >> 8) & 0xFF);
				set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[i].addr[1],
					rg_shutters[i] & 0xFF);
			}
		}
	}
	DRV_LOG(ctx, "exp[0x%x/0x%x/0x%x], fll(input/output):%u/%u, flick_en:%u\n",
		rg_shutters[0], rg_shutters[1], rg_shutters[2],
		frame_length, ctx->frame_length, ctx->autoflicker_en);
	if (!ctx->ae_ctrl_gph_en) {
		if (gph)
			ctx->s_ctx.s_gph((void *)ctx, 0);
		commit_i2c_buffer(ctx);
	}
	/* group hold end */
}

static void konkauwide_write_frame_length_in_lut(struct subdrv_ctx *ctx, u32 fll, u32 *fll_in_lut)
{
	int i = 0;
	u32 frame_length_buf;
	u32 fll_step = 0;
	u32 min_fll = 0;

	check_current_scenario_id_bound(ctx);
	fll_step = ctx->s_ctx.mode[ctx->current_scenario_id].framelength_step;

	// manual mode
	switch (ctx->s_ctx.mode[ctx->current_scenario_id].exp_cnt) {
	case 2:
		if (fll_step) {
			fll_in_lut[0] =
				roundup(fll_in_lut[0], fll_step);
			fll_in_lut[1] =
				roundup(fll_in_lut[1], fll_step);
		}
		min_fll = ctx->s_ctx.mode[ctx->current_scenario_id].framelength / 2;

		if (fll_in_lut[0] < fll_in_lut[1]) {
			if (fll_in_lut[0] < min_fll) {
				fll_in_lut[1] -= min_fll - fll_in_lut[0];
				fll_in_lut[0] = min_fll;
			}
		} else {
			if (fll_in_lut[1] < min_fll) {
				fll_in_lut[0] -= min_fll - fll_in_lut[1];
				fll_in_lut[1] = min_fll;
			}
		}
		fll_in_lut[2] = 0;
		fll_in_lut[3] = 0;
		fll_in_lut[4] = 0;
		ctx->frame_length_in_lut[0] = fll_in_lut[0];
		ctx->frame_length_in_lut[1] = fll_in_lut[1];
		ctx->frame_length =
			ctx->frame_length_in_lut[0] + ctx->frame_length_in_lut[1];
		break;
	case 3:
		if (fll_step) {
			fll_in_lut[0] =
				roundup(fll_in_lut[0], fll_step);
			fll_in_lut[1] =
				roundup(fll_in_lut[1], fll_step);
			fll_in_lut[2] =
				roundup(fll_in_lut[2], fll_step);
		}
		fll_in_lut[3] = 0;
		fll_in_lut[4] = 0;
		ctx->frame_length_in_lut[0] = fll_in_lut[0];
		ctx->frame_length_in_lut[1] = fll_in_lut[1];
		ctx->frame_length_in_lut[2] = fll_in_lut[2];
		ctx->frame_length =
			ctx->frame_length_in_lut[0] +
			ctx->frame_length_in_lut[1] +
			ctx->frame_length_in_lut[2];
		break;
	default:
		break;
	}

	if (ctx->extend_frame_length_en == FALSE) {
		frame_length_buf = 0;
		for (i = 0; i < 3; i++) {
			if (fll_in_lut[i]) {
				if (ctx->s_ctx.reg_addr_frame_length_in_lut[i].addr[2]) {
					set_i2c_buffer(ctx,
						ctx->s_ctx.reg_addr_frame_length_in_lut[i].addr[0],
						(fll_in_lut[i] >> 16) & 0xFF);
					set_i2c_buffer(ctx,
						ctx->s_ctx.reg_addr_frame_length_in_lut[i].addr[1],
						(fll_in_lut[i] >> 8) & 0xFF);
					set_i2c_buffer(ctx,
						ctx->s_ctx.reg_addr_frame_length_in_lut[i].addr[2],
						fll_in_lut[i] & 0xFF);
				} else {
					set_i2c_buffer(ctx,
						ctx->s_ctx.reg_addr_frame_length_in_lut[i].addr[0],
						(fll_in_lut[i] >> 8) & 0xFF);
					set_i2c_buffer(ctx,
						ctx->s_ctx.reg_addr_frame_length_in_lut[i].addr[1],
						fll_in_lut[i] & 0xFF);
				}
				/* update FL_lut RG value after setting buffer for writing RG */
				ctx->frame_length_in_lut_rg[i] = fll_in_lut[i];
				frame_length_buf +=
					ctx->frame_length_in_lut_rg[i];
			}
		}
		/* update FL RG value simultaneously */
		ctx->frame_length_rg = frame_length_buf;

		DRV_LOG(ctx,
			"ctx:(fl(RG):%u,%u/%u/%u/%u/%u), scen_id:%u,fll(input/ctx/output_a/b/c/d/e):0x%x/%x/%x/%x/%x/%x/%x,fll_step:%u\n",
			ctx->frame_length_rg,
			ctx->frame_length_in_lut_rg[0],
			ctx->frame_length_in_lut_rg[1],
			ctx->frame_length_in_lut_rg[2],
			ctx->frame_length_in_lut_rg[3],
			ctx->frame_length_in_lut_rg[4],
			ctx->current_scenario_id,
			fll,
			ctx->frame_length,
			fll_in_lut[0],
			fll_in_lut[1],
			fll_in_lut[2],
			fll_in_lut[3],
			fll_in_lut[4],
			fll_step);
	} else {
		DRV_LOG(ctx,
			"sid:%u,extend_frame_length_en:%u,default won't write fll!\n",
			ctx->current_scenario_id, ctx->extend_frame_length_en);
		return;
	}
}


static int konkauwide_set_multi_shutter_frame_length_in_lut(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64* feature_data = (u64*)para;
	konkauwide_set_multi_shutter_frame_length_in_lut_convert(ctx,
		(u64 *)(*feature_data),
		(u16) (*(feature_data + 1)),
		(u32) (*(feature_data + 2)),
		(u32 *) (*(feature_data + 3)));
	return 0;
}

static void konkauwide_set_multi_shutter_frame_length_in_lut_convert(struct subdrv_ctx *ctx,
	u64 *shutters, u16 exp_cnt, u32 frame_length, u32 *frame_length_in_lut)
{
	int i = 0;
	u16 last_exp_cnt = 1;
	int fine_integ_line = 0;
	u32 frame_length_step;
	u32 cit_step = 0;
	u32 cit_in_lut[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
	u32 calc_fl_in_lut[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
	bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);

	ctx->frame_length = frame_length ? frame_length : ctx->min_frame_length;

	if (exp_cnt > ARRAY_SIZE(ctx->exposure)) {
		DRV_LOGE(ctx, "invalid exp_cnt:%u>%lu\n", exp_cnt, ARRAY_SIZE(ctx->exposure));
		exp_cnt = ARRAY_SIZE(ctx->exposure);
	}
	check_current_scenario_id_bound(ctx);

	/* check boundary of shutter */
	for (i = 1; i < ARRAY_SIZE(ctx->exposure); i++)
		last_exp_cnt += ctx->exposure[i] ? 1 : 0;

	fine_integ_line = ctx->s_ctx.mode[ctx->current_scenario_id].fine_integ_line;
	cit_step = ctx->s_ctx.mode[ctx->current_scenario_id].coarse_integ_step;
	frame_length_step = ctx->s_ctx.mode[ctx->current_scenario_id].framelength_step;

	/* manual mode */
	for (i = 0; i < exp_cnt; i++) {
		shutters[i] = FINE_INTEG_CONVERT(shutters[i], fine_integ_line);
		shutters[i] = max_t(u64, shutters[i],
			(u64)ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_shutter_range[i].min);
		shutters[i] = min_t(u64, shutters[i],
			(u64)ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_shutter_range[i].max);
		if (cit_step)
			shutters[i] = roundup(shutters[i], cit_step);

		/* update frame_length_in_lut */
		ctx->frame_length_in_lut[i] = frame_length_in_lut[i] ?
			frame_length_in_lut[i] : 0;
		/* check boundary of framelength in lut */
		ctx->frame_length_in_lut[i] =
			min(ctx->frame_length_in_lut[i], ctx->s_ctx.frame_length_max);
	}

	for (i = 0; i < exp_cnt; i++) {
		/* update cit_in_lut depends on exposure_order_in_lbmf */
		if (ctx->s_ctx.mode[ctx->current_scenario_id].exposure_order_in_lbmf ==
			IMGSENSOR_LBMF_EXPOSURE_SE_FIRST) {
			/* 2exp: cit_lut_a = SE / cit_lut_b = LE */
			/* 3exp: cit_lut_a = SE / cit_lut_b = ME / cit_lut_c = LE */
			cit_in_lut[i] = shutters[exp_cnt - 1 - i];
		} else if (ctx->s_ctx.mode[ctx->current_scenario_id].exposure_order_in_lbmf ==
			IMGSENSOR_LBMF_EXPOSURE_LE_FIRST) {
			/* 2exp: cit_lut_a = LE / cit_lut_b = SE */
			/* 3exp: cit_lut_a = LE / cit_lut_b = ME / cit_lut_c = SE */
			cit_in_lut[i] = shutters[i];
		} else {
			DRV_LOGE(ctx, "pls assign exposure_order_in_lbmf value!\n");
			return;
		}
	}

	switch (ctx->s_ctx.mode[ctx->current_scenario_id].exp_cnt) {
	case 2:
		/* fll_a_min = readout + xx lines(margin) */
		calc_fl_in_lut[0] =
			ctx->s_ctx.mode[ctx->current_scenario_id].readout_length +
			ctx->s_ctx.mode[ctx->current_scenario_id].read_margin;
		/* fll_a = max(readout, current shutter_b) */
		calc_fl_in_lut[0] =
			max(calc_fl_in_lut[0], cit_in_lut[1] + ctx->s_ctx.exposure_margin);
		/* fll_b_min = readout + xx lines(margin) */
		calc_fl_in_lut[1] =
			ctx->s_ctx.mode[ctx->current_scenario_id].readout_length +
			ctx->s_ctx.mode[ctx->current_scenario_id].read_margin;
		/* fll_b = max(readout, current shutter_a) */
		calc_fl_in_lut[1] =
			max(calc_fl_in_lut[1], cit_in_lut[0] + ctx->s_ctx.exposure_margin);

		/* fll_a = max(fll_a, userInput_fll_a) */
		ctx->frame_length_in_lut[0] =
			max(ctx->frame_length_in_lut[0], calc_fl_in_lut[0]);
		/* fll_a = min(fll_a, fll_max) */
		ctx->frame_length_in_lut[0] =
			min(ctx->frame_length_in_lut[0], ctx->s_ctx.frame_length_max);
		ctx->frame_length_in_lut[0] = frame_length_step ?
			roundup(ctx->frame_length_in_lut[0], frame_length_step) :
			ctx->frame_length_in_lut[0];
		/* fll_b = max(fll_b, userInput_fll_b) */
		ctx->frame_length_in_lut[1] =
			max(ctx->frame_length_in_lut[1], calc_fl_in_lut[1]);

		if (ctx->frame_length >= ctx->frame_length_in_lut[0]) {
			/* fll_b = max(fll_b, fll-fll_a) */
			ctx->frame_length_in_lut[1] =
				max(ctx->frame_length_in_lut[1],
					ctx->frame_length - ctx->frame_length_in_lut[0]);
		}

		/* fll_b = min(fll_b, fll_max) */
		ctx->frame_length_in_lut[1] =
			min(ctx->frame_length_in_lut[1], ctx->s_ctx.frame_length_max);
		ctx->frame_length_in_lut[1] = frame_length_step ?
			roundup(ctx->frame_length_in_lut[1], frame_length_step) :
			ctx->frame_length_in_lut[1];
		/* lut[2] no use, and assign zero */
		ctx->frame_length_in_lut[2] = 0;
		/* lut[3] no use, and assign zero */
		ctx->frame_length_in_lut[3] = 0;
		/* lut[4] no use, and assign zero */
		ctx->frame_length_in_lut[4] = 0;
		break;
	case 3:
		/* fll_a_min = readout + xx lines(margin) */
		calc_fl_in_lut[0] =
			ctx->s_ctx.mode[ctx->current_scenario_id].readout_length +
			ctx->s_ctx.mode[ctx->current_scenario_id].read_margin;
		/* fll_a = max(readout, current shutter_b) */
		calc_fl_in_lut[0] =
			max(calc_fl_in_lut[0], cit_in_lut[1] + ctx->s_ctx.exposure_margin);
		/* fll_b_min = readout + xx lines(margin) */
		calc_fl_in_lut[1] =
			ctx->s_ctx.mode[ctx->current_scenario_id].readout_length +
			ctx->s_ctx.mode[ctx->current_scenario_id].read_margin;
		/* fll_b = max(readout, current shutter_c) */
		calc_fl_in_lut[1] =
			max(calc_fl_in_lut[1], cit_in_lut[2] + ctx->s_ctx.exposure_margin);
		/* fll_c_min = readout + xx lines(margin) */
		calc_fl_in_lut[2] =
			ctx->s_ctx.mode[ctx->current_scenario_id].readout_length +
			ctx->s_ctx.mode[ctx->current_scenario_id].read_margin;
		/* fll_c = max(readout, current shutter_a) */
		calc_fl_in_lut[2] =
			max(calc_fl_in_lut[2], cit_in_lut[0] + ctx->s_ctx.exposure_margin);

		/* fll_a = max(fll_a, userInput_fll_a) */
		ctx->frame_length_in_lut[0] =
			max(ctx->frame_length_in_lut[0], calc_fl_in_lut[0]);
		/* fll_a = min(fll_a, fll_max) */
		ctx->frame_length_in_lut[0] =
			min(ctx->frame_length_in_lut[0], ctx->s_ctx.frame_length_max);
		ctx->frame_length_in_lut[0] = frame_length_step ?
			roundup(ctx->frame_length_in_lut[0], frame_length_step) :
			ctx->frame_length_in_lut[0];
		/* fll_b = max(fll_b, userInput_fll_b) */
		ctx->frame_length_in_lut[1] =
			max(ctx->frame_length_in_lut[1], calc_fl_in_lut[1]);
		/* fll_b = min(fll_b, fll_max) */
		ctx->frame_length_in_lut[1] =
			min(ctx->frame_length_in_lut[1], ctx->s_ctx.frame_length_max);
		ctx->frame_length_in_lut[1] = frame_length_step ?
			roundup(ctx->frame_length_in_lut[1], frame_length_step) :
			ctx->frame_length_in_lut[1];
		/* fll_c = max(fll_c, userInput_fll_c) */
		ctx->frame_length_in_lut[2] =
			max(ctx->frame_length_in_lut[2], calc_fl_in_lut[2]);

		if (ctx->frame_length >=
			(ctx->frame_length_in_lut[0] + ctx->frame_length_in_lut[1])) {
			/* fll_c = max(fll_c, fll-fll_b-fll_a) */
			ctx->frame_length_in_lut[2] =
				max(ctx->frame_length_in_lut[2],
					(ctx->frame_length - ctx->frame_length_in_lut[1] -
					ctx->frame_length_in_lut[0]));
		}

		/* fll_c = min(fll_c, fll_max) */
		ctx->frame_length_in_lut[2] =
			min(ctx->frame_length_in_lut[2], ctx->s_ctx.frame_length_max);
		ctx->frame_length_in_lut[2] = frame_length_step ?
			roundup(ctx->frame_length_in_lut[2], frame_length_step) :
			ctx->frame_length_in_lut[2];
		/* lut[3] no use, and assign zero */
		ctx->frame_length_in_lut[3] = 0;
		/* lut[4] no use, and assign zero */
		ctx->frame_length_in_lut[4] = 0;
		break;
	default:
		break;
	}

	/* restore shutter & update framelength */
	memset(ctx->exposure, 0, sizeof(ctx->exposure));
	ctx->frame_length = 0;
	for (i = 0; i < exp_cnt; i++) {
		ctx->exposure[i] = shutters[i];
		ctx->frame_length += ctx->frame_length_in_lut[i];
	}
	/* check boundary of framelength */
	ctx->frame_length =	max(ctx->frame_length, ctx->min_frame_length);
	/* group hold start */
	if (gph)
		ctx->s_ctx.s_gph((void *)ctx, 1);
	/* enable auto extend */
	if (ctx->s_ctx.reg_addr_auto_extend)
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_auto_extend, 0x01);
	/* write framelength */
	set_auto_flicker(ctx, 0);

	konkauwide_write_frame_length_in_lut(ctx, ctx->frame_length, ctx->frame_length_in_lut);

	/* write shutter: LUT register differs from DOL */
	if (ctx->s_ctx.reg_addr_exposure_lshift != PARAM_UNDEFINED) {
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_exposure_lshift, 0);
		ctx->l_shift = 0;
	}
	for (i = 0; i < 3; i++) {
		if (cit_in_lut[i]) {
			if (ctx->s_ctx.reg_addr_exposure_in_lut[i].addr[2]) {
				set_i2c_buffer(ctx,
					ctx->s_ctx.reg_addr_exposure_in_lut[i].addr[0],
					(cit_in_lut[i] >> 16) & 0xFF);
				set_i2c_buffer(ctx,
					ctx->s_ctx.reg_addr_exposure_in_lut[i].addr[1],
					(cit_in_lut[i] >> 8) & 0xFF);
				set_i2c_buffer(ctx,
					ctx->s_ctx.reg_addr_exposure_in_lut[i].addr[2],
					cit_in_lut[i] & 0xFF);
			} else {
				set_i2c_buffer(ctx,
					ctx->s_ctx.reg_addr_exposure_in_lut[i].addr[0],
					(cit_in_lut[i] >> 8) & 0xFF);
				set_i2c_buffer(ctx,
					ctx->s_ctx.reg_addr_exposure_in_lut[i].addr[1],
					cit_in_lut[i] & 0xFF);
			}
		}
	}

	DRV_LOG(ctx,
		"sid:%u,shutter(input/lut):0x%llx/%llx/%llx,%x/%x/%x,flInLUT(input/ctx/output_a/b/c/d/e):%u/%u/%u/%u/%u/%u/%u,flick_en:%d\n",
		ctx->current_scenario_id,
		shutters[0], shutters[1], shutters[2],
		cit_in_lut[0], cit_in_lut[1], cit_in_lut[2],
		frame_length, ctx->frame_length,
		ctx->frame_length_in_lut[0],
		ctx->frame_length_in_lut[1],
		ctx->frame_length_in_lut[2],
		ctx->frame_length_in_lut[3],
		ctx->frame_length_in_lut[4],
		ctx->autoflicker_en);
	if (!ctx->ae_ctrl_gph_en) {
		if (gph)
			ctx->s_ctx.s_gph((void *)ctx, 0);
		commit_i2c_buffer(ctx);
	}
	/* group hold end */
}

static int konkauwide_set_multi_shutter_frame_length_ctrl(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64* feature_data = (u64*)para;
	konkauwide_lens_pos_writeback(ctx);
	konkauwide_set_multi_shutter_frame_length(ctx, (u64 *)(*feature_data),
		(u16) (*(feature_data + 1)), (u16) (*(feature_data + 2)));
	return 0;
}

static void konkauwide_set_hdr_tri_shutter(struct subdrv_ctx *ctx, u64 *shutters, u16 exp_cnt)
{
	int i = 0;
	u64 values[3] = {0};
	u32 frame_length_in_lut[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};

	if (shutters != NULL) {
		for (i = 0; i < 3; i++)
			values[i] = (u64) *(shutters + i);
	}
	if (ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_LBMF) {
		konkauwide_set_multi_shutter_frame_length_in_lut_convert(ctx,
			values, exp_cnt, 0, frame_length_in_lut);
		return;
	}
	konkauwide_set_multi_shutter_frame_length(ctx, values, exp_cnt, 0);
}

static int konkauwide_set_hdr_tri_shutter2(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64* feature_data = (u64*)para;

	konkauwide_set_hdr_tri_shutter(ctx, feature_data, 2);
	return 0;
}

static int konkauwide_set_hdr_tri_shutter3(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64* feature_data = (u64*)para;

	konkauwide_set_hdr_tri_shutter(ctx, feature_data, 3);
	return 0;
}

static bool dump_i2c_enable = false;

static void dump_i2c_buf(struct subdrv_ctx *ctx, u8 * buf, u32 length)
{
	int i;
	char *out_str = NULL;
	char *strptr = NULL;
	size_t buf_size = SUBDRV_I2C_BUF_SIZE * sizeof(char);
	size_t remind = buf_size;
	int num = 0;

	out_str = kzalloc(buf_size + 1, GFP_KERNEL);
	if (!out_str)
		return;

	strptr = out_str;
	memset(out_str, 0, buf_size + 1);

	num = snprintf(strptr, remind,"[ ");
	remind -= num;
	strptr += num;

	for (i = 0 ; i < length; i ++) {
		num = snprintf(strptr, remind,"0x%02x, ", buf[i]);

		if (num <= 0) {
			DRV_LOG(ctx, "snprintf return negative at line %d\n", __LINE__);
			kfree(out_str);
			return;
		}

		remind -= num;
		strptr += num;

		if (remind <= 20) {
			DRV_LOG(ctx, " write %s\n", out_str);
			memset(out_str, 0, buf_size + 1);
			strptr = out_str;
			remind = buf_size;
		}
	}

	num = snprintf(strptr, remind," ]");
	remind -= num;
	strptr += num;

	DRV_LOG(ctx, " write %s\n", out_str);
	strptr = out_str;
	remind = buf_size;

	kfree(out_str);
}

static int konkauwide_i2c_burst_wr_regs_u16(struct subdrv_ctx * ctx, u16 * list, u32 len)
{
	adapter_i2c_burst_wr_regs_u16(ctx, ctx->i2c_write_id >> 1, list, len);
	return 	0;
}

#define MAX_BUF_SIZE  4096
#define MAX_MSG_NUM_U16  MAX_BUF_SIZE/4

struct cache_wr_regs_u16 {
	struct i2c_msg msg[MAX_MSG_NUM_U16];
};

static int adapter_i2c_burst_wr_regs_u16(struct subdrv_ctx * ctx ,
		u16 addr, u16 *list, u32 len)
{
	struct i2c_client *i2c_client = ctx->i2c_client;
	struct i2c_msg  msg;
	struct i2c_msg *pmsg = &msg;

	u8 *pbuf = NULL;
	u16 *plist = NULL;
	u16 *plist_end = NULL;

	u32 sent = 0;
	u32 total = 0;
	u32 per_sent = 0;
	int ret, i;

	if(!msg_buf) {
		LOG_INF("malloc msg_buf retry");
		msg_buf = kmalloc(MAX_BURST_LEN, GFP_KERNEL);
		if(!msg_buf) {
			LOG_INF("malloc error");
			return -ENOMEM;
		}
	}

	/* each msg contains addr(u16) + val(u16 *) */
	sent = 0;
	total = len / 2;
	plist = list;
	plist_end = list + len - 2;

	DRV_LOG(ctx, "len(%u)  total(%u)", len, total);

	while (sent < total) {

		per_sent = 0;
		pmsg = &msg;
		pbuf = msg_buf;

		pmsg->addr = addr;
		pmsg->flags = i2c_client->flags;
		pmsg->buf = pbuf;

		pbuf[0] = plist[0] >> 8;    //address
		pbuf[1] = plist[0] & 0xff;

		pbuf[2] = plist[1] >> 8;  //data 1
		pbuf[3] = plist[1] & 0xff;

		pbuf += 4;
		pmsg->len = 4;
		per_sent += 1;

		for (i = 0; i < total - sent - 1; i++) {  //Maximum number of remaining cycles - 1
			if(plist[0] + 2 == plist[2] ) {  //Addresses are consecutive
				pbuf[0] = plist[3] >> 8;
				pbuf[1] = plist[3] & 0xff;

				pbuf += 2;
				pmsg->len += 2;
				per_sent += 1;
				plist += 2;

				if(pmsg->len >= MAX_BURST_LEN) {
					break;
				}
			}
		}
		plist += 2;

		if(dump_i2c_enable) {
			DRV_LOG(ctx, "pmsg->len(%d) buff: ", pmsg->len);
			dump_i2c_buf(ctx, msg_buf, pmsg->len);
		}

		ret = i2c_transfer(i2c_client->adapter, pmsg, 1);

		if (ret < 0) {
			dev_info(&i2c_client->dev,
				"i2c transfer failed (%d)\n", ret);
			return -EIO;
		}

		sent += per_sent;

		DRV_LOG(ctx, "sent(%u)  total(%u)  per_sent(%u)", sent, total, per_sent);
	}

	return 0;
}

#define konkauwide_AF_READ_ID  (0x18)
#define konkauwide_AF_POSITON_ADD  (0x03)

static bool read_af_pos(struct subdrv_ctx *ctx, u16 *positon)
{
	int ret;
	u8 buf[2];
	struct i2c_msg msg[2];
	struct i2c_client *i2c_client = ctx->i2c_client;

	buf[0] = konkauwide_AF_POSITON_ADD;

	msg[0].addr = konkauwide_AF_READ_ID >> 1;
	msg[0].flags = i2c_client->flags;
	msg[0].buf = buf;
	msg[0].len = 1;

	msg[1].addr  = konkauwide_AF_READ_ID >> 1;
	msg[1].flags = i2c_client->flags | I2C_M_RD;
	msg[1].buf = buf;
	msg[1].len = 2;

	ret = i2c_transfer(i2c_client->adapter, msg, 2);
	if (ret < 0) {
		dev_info(&i2c_client->dev, "i2c transfer failed (%d)\n", ret);
		return false;
	}

	*positon = ((u16)buf[0] << 8) | buf[1];

	return true;
}

static u16 lens_position_setting[] = {
	0xFCFC, 0x2001,
	0x2566, 0x0000,
	0xFCFC, 0x4000,
};

static void konkauwide_lens_pos_writeback(struct subdrv_ctx *ctx)
{
	kal_uint16 af_pos = 0;
	kal_uint16 write_pos = 0;
	kal_uint16 write_pos_cover = 0;

	bool ret;

	DRV_LOG(ctx,"%s g_af_code_macro(%d), g_af_code_infinity(%d)",
		__func__, g_af_code_macro, g_af_code_infinity);

	if (ctx->current_scenario_id == SENSOR_SCENARIO_ID_CUSTOM2) {

		ret = read_af_pos(ctx, &af_pos);
		if(ret == false || g_af_code_macro == 0 || g_af_code_infinity == 0 || g_af_code_macro == g_af_code_infinity) {
			pr_err("%s ret(%d) ",__func__, ret);
			return ;
		}
		if(af_pos < g_af_code_infinity) {
			af_pos = g_af_code_infinity;
		}
		if(af_pos > g_af_code_macro) {
			af_pos = g_af_code_macro;
		}

		write_pos = (u32)(af_pos - g_af_code_infinity) * 1023 / (g_af_code_macro - g_af_code_infinity);

		write_pos_cover = ((write_pos >> 8) & 0xff) | ((write_pos << 8) & 0xff00);

		lens_position_setting[3] = write_pos_cover;

		DRV_LOG(ctx,"%s af_pos(%d), g_af_code_infinity(%d), g_af_code_macro(%d), write_pos(0x%x) write_pos_cover(0x%x)",
			__func__, af_pos, g_af_code_infinity, g_af_code_macro, write_pos, write_pos_cover);

		subdrv_i2c_wr_regs_u16(ctx, lens_position_setting, ARRAY_SIZE(lens_position_setting));
	}
}

static int konkauwide_set_max_framerate_by_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 *feature_data = (u64 *)para;
	enum SENSOR_SCENARIO_ID_ENUM scenario_id = (enum SENSOR_SCENARIO_ID_ENUM)*feature_data;
	u32 framerate = *(feature_data + 1);
	u32 frame_length;
	u32 frame_length_step;
	u32 frame_length_min;
	u32 frame_length_max;

	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOGE(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
	}
	if (!framerate) {
		DRV_LOGE(ctx, "framerate (%u) is invalid\n", framerate);
		return ERROR_NONE;
	}
	if (!ctx->s_ctx.mode[scenario_id].linelength) {
		DRV_LOGE(ctx, "linelength (%u) is invalid\n",
			ctx->s_ctx.mode[scenario_id].linelength);
		return ERROR_NONE;
	}
	if (ctx->s_ctx.mode[scenario_id].hdr_mode == HDR_RAW_LBMF) {
		konkauwide_set_max_framerate_in_lut_by_scenario(ctx, scenario_id, framerate);
		return ERROR_NONE;
	}

	frame_length_step = ctx->s_ctx.mode[scenario_id].framelength_step;
	/* set on the step of frame length */
	frame_length = ctx->s_ctx.mode[scenario_id].pclk / framerate * 10
		/ ctx->s_ctx.mode[scenario_id].linelength;
	frame_length = frame_length_step ?
		(frame_length - (frame_length % frame_length_step)) : frame_length;
	frame_length_min = ctx->s_ctx.mode[scenario_id].framelength;
	frame_length_max = ctx->s_ctx.frame_length_max;
	frame_length_max = frame_length_step ?
		(frame_length_max - (frame_length_max % frame_length_step)) : frame_length_max;


	/* set in the range of frame length */
	ctx->frame_length = max(frame_length, frame_length_min);
	ctx->frame_length = min(ctx->frame_length, frame_length_max);
	ctx->frame_length = frame_length_step ?
		roundup(ctx->frame_length,frame_length_step) : ctx->frame_length;

	/* set default frame length if given default framerate */
	if (framerate == ctx->s_ctx.mode[scenario_id].max_framerate)
		ctx->frame_length = ctx->s_ctx.mode[scenario_id].framelength;

	ctx->current_fps = ctx->pclk / ctx->frame_length * 10 / ctx->line_length;
	ctx->min_frame_length = ctx->frame_length;
	DRV_LOG(ctx, "max_fps(input/output):%u/%u(sid:%u), min_fl_en:1, ctx->frame_length:%u\n",
		framerate, ctx->current_fps, scenario_id, ctx->frame_length);
	if (ctx->s_ctx.reg_addr_auto_extend ||
			(ctx->frame_length > (ctx->exposure[0] + ctx->s_ctx.exposure_margin))) {
		if (ctx->s_ctx.aov_sensor_support &&
			ctx->s_ctx.mode[scenario_id].aov_mode &&
			!ctx->s_ctx.mode[scenario_id].s_dummy_support)
			DRV_LOG_MUST(ctx, "AOV mode not support set_dummy!\n");
		else
			konkauwide_set_dummy(ctx);
	}
	return ERROR_NONE;
}
/**
 * @brief: This api is used to assign FLL_A/FLL_B in lut for manual mode.
 * It should refer to previous shutter because per-frame multi shutter framelength
 * might not be called.
 * @param ctx: subdrv_ctx
 * @param scenario_id: current scenario id
 * @param framerate: input framerate
 */
static void konkauwide_set_max_framerate_in_lut_by_scenario(struct subdrv_ctx *ctx,
	enum SENSOR_SCENARIO_ID_ENUM scenario_id, u32 framerate)
{
	u32 frame_length = 0;
	u32 frame_length_step = 0;
	u16 exp_cnt = 0;
	u32 cit_in_lut[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
	u32 calc_fl_in_lut[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
	int i;

	frame_length = ctx->s_ctx.mode[scenario_id].pclk / framerate * 10
		/ ctx->s_ctx.mode[scenario_id].linelength;
	frame_length_step = ctx->s_ctx.mode[scenario_id].framelength_step;
	frame_length = frame_length_step ?
		(frame_length - (frame_length % frame_length_step)) : frame_length;
	ctx->frame_length =
		max(frame_length, ctx->s_ctx.mode[scenario_id].framelength);

	/* set default frame length if given default framerate */
	if (framerate ==  ctx->s_ctx.mode[scenario_id].max_framerate)
		ctx->frame_length = ctx->s_ctx.mode[scenario_id].framelength;

	// manual mode
	exp_cnt = ctx->s_ctx.mode[scenario_id].exp_cnt;
	switch (exp_cnt) {
	case 2:
		for (i = 0; i < exp_cnt; i++) {
			/*  update cit_in_lut depends on exposure_order_in_lbmf */
			if (ctx->s_ctx.mode[scenario_id].exposure_order_in_lbmf ==
				IMGSENSOR_LBMF_EXPOSURE_SE_FIRST) {
				/* 2exp: cit_lut_a = SE / cit_lut_b = LE */
				/* 3exp: cit_lut_a = SE / cit_lut_b = ME / cit_lut_c = LE */
				cit_in_lut[i] = ctx->exposure[exp_cnt - 1 - i];
			} else if (ctx->s_ctx.mode[scenario_id].exposure_order_in_lbmf ==
				IMGSENSOR_LBMF_EXPOSURE_LE_FIRST) {
				/* 2exp: cit_lut_a = LE / cit_lut_b = SE */
				/* 3exp: cit_lut_a = LE / cit_lut_b = ME / cit_lut_c = SE */
				cit_in_lut[i] = ctx->exposure[i];
			} else {
				DRV_LOGE(ctx, "pls assign exposure_order_in_lbmf value!\n");
				return;
			}
		}
		/* fll_a_min = readout + xx lines(margin) */
		calc_fl_in_lut[0] =
			ctx->s_ctx.mode[scenario_id].readout_length +
			ctx->s_ctx.mode[scenario_id].read_margin;
		/* fll_a = max(readout, previous shutter_b) */
		calc_fl_in_lut[0] =
			max(calc_fl_in_lut[0], cit_in_lut[1] + ctx->s_ctx.exposure_margin);
		/* fll_a = min(fll_a, fll_max) */
		ctx->frame_length_in_lut[0] =
			min(calc_fl_in_lut[0], ctx->s_ctx.frame_length_max);
		ctx->frame_length_in_lut[0] = frame_length_step ?
			roundup(ctx->frame_length_in_lut[0], frame_length_step) :
			ctx->frame_length_in_lut[0];
		/* fll_b_min = readout + xx lines(margin) */
		calc_fl_in_lut[1] =
			ctx->s_ctx.mode[scenario_id].readout_length +
			ctx->s_ctx.mode[scenario_id].read_margin;
		/* fll_b = max(readout, previous shutter_a) */
		calc_fl_in_lut[1] =
			max(calc_fl_in_lut[1], cit_in_lut[0] + ctx->s_ctx.exposure_margin);
		if (ctx->frame_length >= ctx->frame_length_in_lut[0]) {
			/* fll_b = max(fll_b, fll_mode_max-fll_a) */
			calc_fl_in_lut[1] =
				max(calc_fl_in_lut[1],
					ctx->frame_length - ctx->frame_length_in_lut[0]);
		}
		/* fll_b = min(fll_b, fll_max) */
		ctx->frame_length_in_lut[1] =
			min(calc_fl_in_lut[1], ctx->s_ctx.frame_length_max);
		ctx->frame_length_in_lut[1] = frame_length_step ?
			roundup(ctx->frame_length_in_lut[1], frame_length_step) :
			ctx->frame_length_in_lut[1];
		ctx->frame_length_in_lut[2] = 0;
		ctx->frame_length_in_lut[3] = 0;
		ctx->frame_length_in_lut[4] = 0;
		/* update framelength */
		ctx->frame_length =
			ctx->frame_length_in_lut[0] + ctx->frame_length_in_lut[1];
		ctx->current_fps = ctx->pclk / ctx->frame_length * 10 / ctx->line_length;
		ctx->min_frame_length = ctx->frame_length;
		DRV_LOG(ctx,
			"sid:%u,max_fps(input/output):%u/%u,min_fl_en:1,lut order:%u,fll(input/ctx/output_a/b/c/d/e):%u/%u/%u/%u/%u/%u/%un",
			scenario_id,
			framerate, ctx->current_fps,
			ctx->s_ctx.mode[scenario_id].exposure_order_in_lbmf,
			frame_length,
			ctx->frame_length,
			ctx->frame_length_in_lut[0],
			ctx->frame_length_in_lut[1],
			ctx->frame_length_in_lut[2],
			ctx->frame_length_in_lut[3],
			ctx->frame_length_in_lut[4]);
		if (ctx->s_ctx.mode[scenario_id].exposure_order_in_lbmf ==
			IMGSENSOR_LBMF_EXPOSURE_SE_FIRST) {
			if (ctx->s_ctx.reg_addr_auto_extend ||
				(ctx->frame_length_in_lut[0] >
				(ctx->exposure[0] + ctx->s_ctx.mode[scenario_id].read_margin)) ||
				(ctx->frame_length_in_lut[1] >
				(ctx->exposure[1] + ctx->s_ctx.mode[scenario_id].read_margin)))
				konkauwide_set_dummy(ctx);
		} else {
			if (ctx->s_ctx.reg_addr_auto_extend ||
				(ctx->frame_length_in_lut[0] >
				(ctx->exposure[1] + ctx->s_ctx.mode[scenario_id].read_margin)) ||
				(ctx->frame_length_in_lut[1] >
				(ctx->exposure[0] + ctx->s_ctx.mode[scenario_id].read_margin)))
				konkauwide_set_dummy(ctx);
		}
		break;
	case 3:
		for (i = 0; i < exp_cnt; i++) {
			/*  update cit_in_lut depends on exposure_order_in_lbmf */
			if (ctx->s_ctx.mode[scenario_id].exposure_order_in_lbmf ==
				IMGSENSOR_LBMF_EXPOSURE_SE_FIRST) {
				/* 2exp: cit_lut_a = SE / cit_lut_b = LE */
				/* 3exp: cit_lut_a = SE / cit_lut_b = ME / cit_lut_c = LE */
				cit_in_lut[i] = ctx->exposure[exp_cnt - 1 - i];
			} else if (ctx->s_ctx.mode[scenario_id].exposure_order_in_lbmf ==
				IMGSENSOR_LBMF_EXPOSURE_LE_FIRST) {
				/* 2exp: cit_lut_a = LE / cit_lut_b = SE */
				/* 3exp: cit_lut_a = LE / cit_lut_b = ME / cit_lut_c = SE */
				cit_in_lut[i] = ctx->exposure[i];
			} else {
				DRV_LOGE(ctx, "pls assign exposure_order_in_lbmf value!\n");
				return;
			}
		}
		/* fll_a_min = readout + xx lines(margin) */
		calc_fl_in_lut[0] =
			ctx->s_ctx.mode[scenario_id].readout_length +
			ctx->s_ctx.mode[scenario_id].read_margin;
		/* fll_a = max(readout, previous shutter_b) */
		calc_fl_in_lut[0] =
			max(calc_fl_in_lut[0], cit_in_lut[1] + ctx->s_ctx.exposure_margin);
		/* fll_a = min(fll_a, fll_max) */
		ctx->frame_length_in_lut[0] =
			min(calc_fl_in_lut[0], ctx->s_ctx.frame_length_max);
		ctx->frame_length_in_lut[0] = frame_length_step ?
			roundup(ctx->frame_length_in_lut[0], frame_length_step) :
			ctx->frame_length_in_lut[0];
		/* fll_b_min = readout + xx lines(margin) */
		calc_fl_in_lut[1] =
			ctx->s_ctx.mode[scenario_id].readout_length +
			ctx->s_ctx.mode[scenario_id].read_margin;
		/* fll_b = max(readout, previous shutter_c) */
		calc_fl_in_lut[1] =
			max(calc_fl_in_lut[1], cit_in_lut[2] + ctx->s_ctx.exposure_margin);
		/* fll_b = min(fll_b, fll_max) */
		ctx->frame_length_in_lut[1] =
			min(calc_fl_in_lut[1], ctx->s_ctx.frame_length_max);
		ctx->frame_length_in_lut[1] = frame_length_step ?
			roundup(ctx->frame_length_in_lut[1], frame_length_step) :
			ctx->frame_length_in_lut[1];
		/* fll_c_min = readout + xx lines(margin) */
		calc_fl_in_lut[2] =
			ctx->s_ctx.mode[scenario_id].readout_length +
			ctx->s_ctx.mode[scenario_id].read_margin;
		/* fll_c = max(readout, previous shutter_a) */
		calc_fl_in_lut[2] =
			max(calc_fl_in_lut[2], cit_in_lut[0] + ctx->s_ctx.exposure_margin);
		if (ctx->frame_length >=
			(ctx->frame_length_in_lut[0] + ctx->frame_length_in_lut[1])) {
			/* fll_c = max(fll_c, fll_mode_max-fll_b-fll_a) */
			calc_fl_in_lut[2] =
				max(calc_fl_in_lut[2],
					(ctx->frame_length - ctx->frame_length_in_lut[1] -
					ctx->frame_length_in_lut[0]));
		}
		/* fll_c = min(fll_c, fll_max) */
		ctx->frame_length_in_lut[2] =
			min(calc_fl_in_lut[2], ctx->s_ctx.frame_length_max);
		ctx->frame_length_in_lut[2] = frame_length_step ?
			roundup(ctx->frame_length_in_lut[2], frame_length_step) :
			ctx->frame_length_in_lut[2];
		ctx->frame_length_in_lut[3] = 0;
		ctx->frame_length_in_lut[4] = 0;
		/* update framelength */
		ctx->frame_length =
			ctx->frame_length_in_lut[0] +
			ctx->frame_length_in_lut[1] +
			ctx->frame_length_in_lut[2];
		ctx->current_fps = ctx->pclk / ctx->frame_length * 10 / ctx->line_length;
		ctx->min_frame_length = ctx->frame_length;
		DRV_LOG(ctx,
			"sid:%u,max_fps(input/output):%u/%u,min_fl_en:1,lut order:%u,fll(input/ctx/output_a/b/c/d/e):%u/%u/%u/%u/%u/%u/%u\n",
			scenario_id,
			framerate, ctx->current_fps,
			ctx->s_ctx.mode[scenario_id].exposure_order_in_lbmf,
			frame_length,
			ctx->frame_length,
			ctx->frame_length_in_lut[0],
			ctx->frame_length_in_lut[1],
			ctx->frame_length_in_lut[2],
			ctx->frame_length_in_lut[3],
			ctx->frame_length_in_lut[4]);
		if (ctx->s_ctx.mode[scenario_id].exposure_order_in_lbmf ==
			IMGSENSOR_LBMF_EXPOSURE_SE_FIRST) {
			if (ctx->s_ctx.reg_addr_auto_extend ||
				(ctx->frame_length_in_lut[0] >
				(ctx->exposure[1] + ctx->s_ctx.mode[scenario_id].read_margin)) ||
				(ctx->frame_length_in_lut[1] >
				(ctx->exposure[0] + ctx->s_ctx.mode[scenario_id].read_margin)) ||
				(ctx->frame_length_in_lut[2] >
				(ctx->exposure[2] + ctx->s_ctx.mode[scenario_id].read_margin)))
				konkauwide_set_dummy(ctx);
		} else {
			if (ctx->s_ctx.reg_addr_auto_extend ||
				(ctx->frame_length_in_lut[0] >
				(ctx->exposure[1] + ctx->s_ctx.mode[scenario_id].read_margin)) ||
				(ctx->frame_length_in_lut[1] >
				(ctx->exposure[2] + ctx->s_ctx.mode[scenario_id].read_margin)) ||
				(ctx->frame_length_in_lut[2] >
				(ctx->exposure[0] + ctx->s_ctx.mode[scenario_id].read_margin)))
				konkauwide_set_dummy(ctx);
		}
		break;
	default:
		break;
	}
}