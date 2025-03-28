// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023 OPLUS. All rights reserved.
 */
/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 brzbmainmipiraw_Sensor.c
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
#include "brzbmainmipiraw_Sensor.h"

#define BRZBMAIN_EEPROM_READ_ID	0xA1
#define BRZBMAIN_EEPROM_WRITE_ID	0xA0
#define BRZBMAIN_MAX_OFFSET		0x8000
#define OPLUS_CAMERA_COMMON_DATA_LENGTH 40
#define PFX "brzbmain_camera_sensor"
#define LOG_INF(format, args...) pr_err(PFX "[%s] " format, __func__, ##args)
#define OTP_SIZE    0x8000
#define OTP_QSC_VALID_ADDR 0x2A30
#define OTP_PDC_VALID_ADDR 0x7DA0
#define QSC_IS_VALID_VAL      0x01
#define SPC_IS_VALID_VAL      0x01
#define OTP_QCOM_PDAF_DATA_LENGTH 0x1832
#define OTP_QCOM_PDAF_OFFSET_DATA_LENGTH 0x650
#define OTP_QCOM_PDAF_DATA_START_ADDR 0x5E4
#define OTP_QCOM_PDAF_OFFSET_DATA_START_ADDR 0x39b0

static kal_uint8 otp_data_checksum[OTP_SIZE] = {0};
static kal_uint8 otp_qcom_pdaf_data[OTP_QCOM_PDAF_DATA_LENGTH] = {0};
static kal_uint8 otp_qcom_pdaf_offset_data[OTP_QCOM_PDAF_OFFSET_DATA_LENGTH] = {0};
static void set_sensor_cali(void *arg);
static int get_sensor_temperature(void *arg);
static void set_group_hold(void *arg, u8 en);
static u16 get_gain2reg(u32 gain);
static int brzbmain_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int brzbmain_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int brzbmain_check_sensor_id(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int get_eeprom_common_data(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int brzbmain_set_eeprom_calibration(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int brzbmain_get_eeprom_calibration(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int brzbmain_get_otp_checksum_data(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int brzbmain_get_min_shutter_by_scenario_adapter(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int get_imgsensor_id(struct subdrv_ctx *ctx, u32 *sensor_id);
static int open(struct subdrv_ctx *ctx);
static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id);
static int vsync_notify(struct subdrv_ctx *ctx,	unsigned int sof_cnt);
static int brzbmain_set_awb_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static void get_sensor_cali(void *arg);
static void calculate_prsh_length_lines(struct subdrv_ctx *ctx,
	struct mtk_hdr_ae *ae_ctrl,
	enum SENSOR_SCENARIO_ID_ENUM scenario_id);
static bool read_cmos_eeprom_p8(struct subdrv_ctx *ctx, kal_uint16 addr,
                    BYTE *data, int size);
static int brzbmain_get_otp_qcom_pdaf_data(struct subdrv_ctx *ctx, u8 *para, u32 *len);

static int brzbmain_get_otp_qcom_pdaf_offset_data(struct subdrv_ctx *ctx, u8 *para, u32 *len);
/* STRUCT */

static struct eeprom_map_info brzbmain_eeprom_info[] = {
	{ EEPROM_META_MODULE_ID, 0x0000, 0x0010, 0x0011, 2, true },
	{ EEPROM_META_SENSOR_ID, 0x0006, 0x0010, 0x0011, 2, true },
	{ EEPROM_META_LENS_ID, 0x0008, 0x0010, 0x0011, 2, true },
	{ EEPROM_META_VCM_ID, 0x000A, 0x0010, 0x0011, 2, true },
	{ EEPROM_META_MIRROR_FLIP, 0x000E, 0x0010, 0x0011, 1, true },
	{ EEPROM_META_MODULE_SN, 0x00B0, 0x00C7, 0x00C8, 23, true },
	{ EEPROM_META_AF_CODE, 0x0092, 0x009A, 0x009B, 6, true },
	{ EEPROM_META_AF_FLAG, 0x0098, 0x0010, 0x0099, 1, true },
	{ EEPROM_META_STEREO_DATA, 0x0000, 0x0000, 0x0000, 0, false },
	{ EEPROM_META_STEREO_MW_MAIN_DATA, 0x2B00, 0x3199, 0x319A, CALI_DATA_MASTER_LENGTH, false },
	{ EEPROM_META_STEREO_MT_MAIN_DATA, 0x31C0, 0x3859, 0x385A, CALI_DATA_MASTER_LENGTH, false },
	{ EEPROM_META_STEREO_MT_MAIN_DATA_105CM, 0x6400, 0x0000, 0x0000, CALI_DATA_MASTER_LENGTH, false },
};

static struct subdrv_feature_control feature_control_list[] = {
	{SENSOR_FEATURE_SET_TEST_PATTERN, brzbmain_set_test_pattern},
	{SENSOR_FEATURE_SEAMLESS_SWITCH, brzbmain_seamless_switch},
	{SENSOR_FEATURE_CHECK_SENSOR_ID, brzbmain_check_sensor_id},
	{SENSOR_FEATURE_GET_EEPROM_COMDATA, get_eeprom_common_data},
	{SENSOR_FEATURE_SET_SENSOR_OTP, brzbmain_set_eeprom_calibration},
	{SENSOR_FEATURE_GET_EEPROM_STEREODATA, brzbmain_get_eeprom_calibration},
	{SENSOR_FEATURE_GET_SENSOR_OTP_ALL, brzbmain_get_otp_checksum_data},
	{SENSOR_FEATURE_GET_MIN_SHUTTER_BY_SCENARIO, brzbmain_get_min_shutter_by_scenario_adapter},
	{SENSOR_FEATURE_SET_AWB_GAIN, brzbmain_set_awb_gain},
	{SENSOR_FEATURE_GET_OTP_QCOM_PDAF_DATA, brzbmain_get_otp_qcom_pdaf_data},
	{SENSOR_FEATURE_GET_OTP_QCOM_PDAF_OFFSET_DATA, brzbmain_get_otp_qcom_pdaf_offset_data},
};

static u32 brzbmain_dcg_ratio_table_ratio4[] = {4000};
static struct mtk_sensor_saturation_info imgsensor_saturation_info_12bit = {
	.gain_ratio = 4000,
	.OB_pedestal = 64,
	.saturation_level = 3900,
};

static struct eeprom_info_struct eeprom_info[] = {
	{
		.header_id = 0x01480143,/*cal_layout_table*/
		.addr_header_id = 0x00000006,/*cc*/
		.i2c_write_id = 0xA0,/*cc*/

		.qsc_support = TRUE,
		.qsc_size = 0x0C00,
		.addr_qsc = 0x1E30,/*QSC_EEPROM_ADDR*/
		.sensor_reg_addr_qsc = 0xC000, /*QSC_OTP_ADDR*/

		.pdc_support = TRUE,
		.pdc_size = 0x180,
		.addr_pdc = 0x7C20,
		.sensor_reg_addr_pdc = 0xD200,
	},
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
	.i4OffsetX = 0,
	.i4OffsetY = 0,
	.i4PitchX = 0,
	.i4PitchY = 0,
	.i4PairNum = 0,
	.i4SubBlkW = 0,
	.i4SubBlkH = 0,
	.i4PosL = {{0, 0} },
	.i4PosR = {{0, 0} },
	.i4BlockNumX = 0,
	.i4BlockNumY = 0,
	.i4LeFirst = 0,
	.i4Crop = {
		/* <pre> <cap> <normal_video> <hs_video> <slim_video> */
		{0, 0}, {0, 0}, {0, 384}, {0, 384}, {0, 192},
		/* <cust1> <cust2> <cust3> <cust4> <cust5> */
		{0, 192}, {0, 0}, {0, 384}, {0, 0}, {2048, 1536},
		/* <cust6> <cust7> <cust8> <cust9> <cust10> */
		{2048, 1536}, {0, 0}, {0, 0}, {0, 0}, {2048, 1536},
		/* <cust11> <cust12> <cust13> <cust14> */
		{0, 0}, {2048, 1536}, {0, 0}, {0, 0},
	},
	.iMirrorFlip = 3,
	.i4FullRawW = 4096,
	.i4FullRawH = 3072,
	.i4VCPackNum = 1,
	.PDAF_Support = PDAF_SUPPORT_CAMSV_QPD,
	.i4ModeIndex = 0x2,
	.sPDMapInfo[0] = {
		.i4PDPattern = 1,
		.i4BinFacX = 2,
		.i4BinFacY = 4,
		.i4PDRepetition = 0,
		.i4PDOrder = {1},
	},
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_v2h2 = {
	.i4OffsetX = 0,
	.i4OffsetY = 0,
	.i4PitchX = 0,
	.i4PitchY = 0,
	.i4PairNum = 0,
	.i4SubBlkW = 0,
	.i4SubBlkH = 0,
	.i4PosL = {{0, 0} },
	.i4PosR = {{0, 0} },
	.i4BlockNumX = 0,
	.i4BlockNumY = 0,
	.i4LeFirst = 0,
	.i4Crop = {
		/* <pre> <cap> <normal_video> <hs_video> <slim_video> */
		{0, 0}, {0, 0}, {0, 384}, {0, 384}, {0, 192},
		/* <cust1> <cust2> <cust3> <cust4> <cust5> */
		{0, 192}, {0, 0}, {0, 384}, {0, 0}, {2048, 1536},
		/* <cust6> <cust7> <cust8> <cust9> <cust10> */
		{2048, 1536}, {0, 0}, {0, 0}, {0, 0}, {2048, 1536},
		/* <cust11> <cust12> <cust13> <cust14> */
		{0, 0}, {2048, 1536}, {0, 0}, {0, 0},
	},
	.iMirrorFlip = 3,
	.i4FullRawW = 2048,
	.i4FullRawH = 1536,
	.i4VCPackNum = 1,
	.PDAF_Support = PDAF_SUPPORT_CAMSV_QPD,
	.i4ModeIndex = 0x2,
	.sPDMapInfo[0] = {
		.i4PDPattern = 1,
		.i4BinFacX = 2,
		.i4BinFacY = 4,
		.i4PDRepetition = 0,
		.i4PDOrder = {1},
	},
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_full = {
	.i4OffsetX = 0,
	.i4OffsetY = 0,
	.i4PitchX = 0,
	.i4PitchY = 0,
	.i4PairNum = 0,
	.i4SubBlkW = 0,
	.i4SubBlkH = 0,
	.i4PosL = {{0, 0} },
	.i4PosR = {{0, 0} },
	.i4BlockNumX = 0,
	.i4BlockNumY = 0,
	.i4LeFirst = 0,
	.i4Crop = {
		/* <pre> <cap> <normal_video> <hs_video> <slim_video> */
		{0, 0}, {0, 0}, {0, 384}, {0, 384}, {0, 192},
		/* <cust1> <cust2> <cust3> <cust4> <cust5> */
		{0, 192}, {0, 0}, {0, 384}, {0, 0}, {2048, 1536},
		/* <cust6> <cust7> <cust8> <cust9> <cust10> */
		{2048, 1536}, {0, 0}, {0, 0}, {0, 0}, {2048, 1536},
		/* <cust11> <cust12> <cust13> <cust14> */
		{0, 0}, {2048, 1536}, {0, 0}, {0, 0},
	},
	.iMirrorFlip = 3,
	.i4FullRawW = 8192,
	.i4FullRawH = 6144,
	.i4VCPackNum = 1,
	.PDAF_Support = PDAF_SUPPORT_CAMSV_QPD,
	.i4ModeIndex = 0x4,
	.sPDMapInfo[0] = {
		.i4PDPattern = 1,
		.i4BinFacX = 4,
		.i4BinFacY = 2,
		.i4PDRepetition = 0,
		.i4PDOrder = {1},
	},
};

static struct SET_PD_BLOCK_INFO_T imgsensor_partial_pd_info = {
	.i4OffsetX = 16,
	.i4OffsetY = 32,
	.i4PitchX = 8,
	.i4PitchY = 16,
	.i4PairNum = 4,
	.i4SubBlkW = 8,
	.i4SubBlkH = 4,
	.i4PosL = {{16, 35}, {20, 37}, {19, 42}, {23, 44}},
	.i4PosR = {{18, 33}, {22, 39}, {17, 40}, {21, 46}},
	.i4BlockNumX = 504,
	.i4BlockNumY = 144,
	.i4Crop = {
		/* <pre> <cap> <normal_video> <hs_video> <slim_video> */
		{0, 0}, {0, 0}, {0, 384}, {0, 384}, {0, 192},
		/* <cust1> <cust2> <cust3> <cust4> <cust5> */
		{0, 192}, {0, 0}, {0, 384}, {0, 0}, {2048, 1536},
		/* <cust6> <cust7> <cust8> <cust9> <cust10> */
		{2048, 1536}, {0, 0}, {0, 0}, {0, 0}, {2048, 1536},
		/* <cust11> <cust12> <cust13> <cust14> */
		{0, 0}, {2048, 1536}, {0, 0}, {0, 0},
	},
	.iMirrorFlip = 3,
	.i4FullRawW = 4096,
	.i4FullRawH = 3072,
	.i4ModeIndex = 0,
	.i4VCPackNum = 1,
	.PDAF_Support = PDAF_SUPPORT_CAMSV,
	/* VC's PD pattern description */
	.sPDMapInfo[0] = {
		/*.i4VCFeature = VC_PDAF_STATS_NE_PIX_1,*/
		.i4PDPattern = 3,
		.i4PDRepetition = 4,
		.i4PDOrder = {0, 1, 1, 0}, /*R = 1, L = 0*/
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_prev[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0x0c00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x0300,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
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
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x0300,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0x0900,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x0240,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_hs[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0x0900,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x0200,
			.vsize = 0x0240,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_slim[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0800,
			.vsize = 0x0480,
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
			.hsize = 2048,
			.vsize = 1152,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus2[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0x0c00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x0300,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus3[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0x0900,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x0200,
			.vsize = 0x0240,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus4[] = {
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
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x0200,
			.vsize = 0x02F0,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x30,
			.hsize = 0x0200,
			.vsize = 0x02F0,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_ME_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus5[] = {
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
			.vsize = 1536,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus6[] = {
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
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x30,
			.hsize = 2048,
			.vsize = 1536,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_ME_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus7[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2c,
			.hsize = 4096,
			.vsize = 2304,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus8[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x2000,
			.vsize = 0x1800,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x0c00,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus9[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x2000,
			.vsize = 0x1800,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x0c00,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus10[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0x0c00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x0800,
			.vsize = 0x0600,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus11[] = {
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
			.channel = 0,
			.data_type = 0x30,
			.hsize = 2048,
			.vsize = 1536,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus12[] = {
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
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x30,
			.hsize = 2048,
			.vsize = 1536,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_ME_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus13[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 2048,
			.vsize = 1536,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus14[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0x0c00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x0200,
			.vsize = 0x0240,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
};

static struct subdrv_mode_struct mode_struct[] = {
	{/*reg_B2-S1_QBIN(VBIN)_4096x3072_30fps*/
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = brzbmain_preview_setting,
		.mode_setting_len = ARRAY_SIZE(brzbmain_preview_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = brzbmain_seamless_preview,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(brzbmain_seamless_preview),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 1360000000,
		.linelength = 11904,
		.framelength = 3791,
		.max_framerate = 300,
		.mipi_pixel_rate = 973710000,
		.readout_length = 0,
		.read_margin = 36,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
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
		.ae_binning_ratio = 1428,
		.fine_integ_line = -590,
		.delay_frame = 3,
		.csi_param = {
			.cphy_settle = 58,
		},
		.dpc_enabled = true,
		.ana_gain_max = BASEGAIN * 64,
	},
	{/*reg_B2-S1_QBIN(VBIN)_4096x3072_30fps*/
		.frame_desc = frame_desc_cap,
		.num_entries = ARRAY_SIZE(frame_desc_cap),
		.mode_setting_table = brzbmain_capture_setting,
		.mode_setting_len = ARRAY_SIZE(brzbmain_capture_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 1360000000,
		.linelength = 11904,
		.framelength = 3791,
		.max_framerate = 300,
		.mipi_pixel_rate = 973710000,
		.readout_length = 0,
		.read_margin = 36,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
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
		.ae_binning_ratio = 1428,
		.fine_integ_line = -590,
		.delay_frame = 3,
		.csi_param = {},
		.dpc_enabled = true,
		.ana_gain_max = BASEGAIN * 64,
	},
	{/*reg_B4-S3_4096x2304_30FPS**/
		.frame_desc = frame_desc_vid,
		.num_entries = ARRAY_SIZE(frame_desc_vid),
		.mode_setting_table = brzbmain_normal_video_setting,
		.mode_setting_len = ARRAY_SIZE(brzbmain_normal_video_setting),
		.seamless_switch_group = 2,
		.seamless_switch_mode_setting_table = brzbmain_seamless_normal_video,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(brzbmain_seamless_normal_video),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 1360000000,
		.linelength = 11904,
		.framelength = 3791,
		.max_framerate = 300,
		.mipi_pixel_rate = 973710000,
		.readout_length = 0,
		.read_margin = 36,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
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
		.ae_binning_ratio = 1428,
		.fine_integ_line = -590,
		.delay_frame = 3,
		.csi_param = {},
		.dpc_enabled = true,
		.ana_gain_max = BASEGAIN * 64,
	},
	{/*reg_B5-S3  4096x2304 @60FPS */
		.frame_desc = frame_desc_hs,
		.num_entries = ARRAY_SIZE(frame_desc_hs),
		.mode_setting_table = brzbmain_hs_video_setting,
		.mode_setting_len = ARRAY_SIZE(brzbmain_hs_video_setting),
		.seamless_switch_group = 2,
		.seamless_switch_mode_setting_table = brzbmain_seamless_hs_video,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(brzbmain_seamless_hs_video),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 1360000000,
		.linelength = 7136,
		.framelength = 3174,
		.max_framerate = 600,
		.mipi_pixel_rate = 973710000,
		.readout_length = 0,
		.read_margin = 36,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
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
		.imgsensor_pd_info = &imgsensor_partial_pd_info,
		.ae_binning_ratio = 1428,
		.fine_integ_line = -590,
		.delay_frame = 3,
		.csi_param = {
			.cphy_settle = 58,
		},
		.dpc_enabled = true,
		.ana_gain_max = BASEGAIN * 64,
	},
	{   /*reg_V1  2048x1152 @120FPS.*/
		.frame_desc = frame_desc_slim,
		.num_entries = ARRAY_SIZE(frame_desc_slim),
		.mode_setting_table = brzbmain_slim_video_setting,
		.mode_setting_len = ARRAY_SIZE(brzbmain_slim_video_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 1360000000,
		.linelength = 6728,
		.framelength = 1680,
		.max_framerate = 1200,
		.mipi_pixel_rate = 687090000,
		.readout_length = 0,
		.read_margin = 36,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 8,
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
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_v2h2,
		.ae_binning_ratio = 1428,
		.fine_integ_line = 2469,
		.delay_frame = 3,
		.csi_param = {},
		.dpc_enabled = true,
		.ana_gain_max = BASEGAIN * 64,
	},
	{/*reg_V2 2048x1152 @240FPS QBIN V2H2 w/o PD VB Max*/
		.frame_desc = frame_desc_cus1,
		.num_entries = ARRAY_SIZE(frame_desc_cus1),
		.mode_setting_table = brzbmain_custom1_setting,
		.mode_setting_len = ARRAY_SIZE(brzbmain_custom1_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 1360000000,
		.linelength = 4264,
		.framelength = 1314,
		.max_framerate = 2400,
		.mipi_pixel_rate = 871540000,
		.readout_length = 0,
		.read_margin = 36,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 8,
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
		.imgsensor_pd_info = &imgsensor_pd_info_v2h2,
		.ae_binning_ratio = 1428,
		.fine_integ_line = 2469,
		.delay_frame = 3,
		.csi_param = {},
		.dpc_enabled = true,
		.ana_gain_max = BASEGAIN * 64,
	},
	{/*reg_B1-S2 4096x3072 @24FPS QBIN(VBIN) w/ All-PD VB Max.*/
		.frame_desc = frame_desc_cus2,
		.num_entries = ARRAY_SIZE(frame_desc_cus2),
		.mode_setting_table = brzbmain_custom2_setting,
		.mode_setting_len = ARRAY_SIZE(brzbmain_custom2_setting),
		.seamless_switch_group = 3,
		.seamless_switch_mode_setting_table = brzbmain_seamless_custom2,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(brzbmain_seamless_custom2),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 1360000000,
		.linelength = 11904,
		.framelength = 4739,
		.max_framerate = 240,
		.mipi_pixel_rate = 682970000,
		.readout_length = 0,
		.read_margin = 36,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
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
		.ae_binning_ratio = 1428,
		.fine_integ_line = -590,
		.delay_frame = 3,
		.csi_param = {
			.cphy_settle = 57,
		},
		.dpc_enabled = true,
		.ana_gain_max = BASEGAIN * 64,
	},
	{/*reg_B6-S3  4096x2304 @30FPS QBIN w/ Partial-PD VB Max. Seamless w/ "*-S3"**/
		.frame_desc = frame_desc_cus3,
		.num_entries = ARRAY_SIZE(frame_desc_cus3),
		.mode_setting_table = brzbmain_custom3_setting,
		.mode_setting_len = ARRAY_SIZE(brzbmain_custom3_setting),
		.seamless_switch_group = 2,
		.seamless_switch_mode_setting_table = brzbmain_seamless_custom3,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(brzbmain_seamless_custom3),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 1360000000,
		.linelength = 7136,
		.framelength = 6349,
		.max_framerate = 300,
		.mipi_pixel_rate = 973710000,
		.readout_length = 0,
		.read_margin = 36,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
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
		.imgsensor_pd_info = &imgsensor_partial_pd_info,
		.ae_binning_ratio = 1428,
		.fine_integ_line = -590,
		.delay_frame = 3,
		.csi_param = {},
		.dpc_enabled = true,
		.ana_gain_max = BASEGAIN * 64,
	},
	{/*reg_L1-S1 4096x3072 @30FPS QBIN 2LB-MF w/ Partial-PD VB Max. Seamless w/ "*-S1"*/
		.frame_desc = frame_desc_cus4,
		.num_entries = ARRAY_SIZE(frame_desc_cus4),
		.mode_setting_table = brzbmain_custom4_setting,
		.mode_setting_len = ARRAY_SIZE(brzbmain_custom4_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = brzbmain_seamless_custom4,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(brzbmain_seamless_custom4),
		.hdr_mode = HDR_RAW_LBMF,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 1360000000,
		.linelength = 7136,
		.framelength = 6349,
		.max_framerate = 300,
		.mipi_pixel_rate = 973710000,
		.readout_length = 3092,
		.read_margin = 36*2,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 6,
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
		.imgsensor_pd_info = &imgsensor_partial_pd_info,
		.ae_binning_ratio = 1428,
		.fine_integ_line = -590,
		.delay_frame = 2,
		.csi_param = {
			.cphy_settle = 58,
		},
		.exposure_order_in_lbmf = IMGSENSOR_LBMF_EXPOSURE_SE_FIRST,
		.mode_type_in_lbmf = IMGSENSOR_LBMF_MODE_MANUAL,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 64,
		.dpc_enabled = true,
	},
	{/*reg_F2-S1 4096x3072 @30FPS Full-Crop w/ PD VB Max. Seamless w/  "*-S1"*/
		.frame_desc = frame_desc_cus5,
		.num_entries = ARRAY_SIZE(frame_desc_cus5),
		.mode_setting_table = brzbmain_custom5_setting,
		.mode_setting_len = ARRAY_SIZE(brzbmain_custom5_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = brzbmain_seamless_custom5,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(brzbmain_seamless_custom5),
		.hdr_mode = HDR_NONE,
		.pclk = 1360000000,
		.linelength = 14032,
		.framelength = 3225,
		.max_framerate = 300,
		.mipi_pixel_rate = 973710000,
		.readout_length = 0,
		.read_margin = 36,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 15,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 1536,
			.w0_size = 8192,
			.h0_size = 3072,
			.scale_w = 8192,
			.scale_h = 3072,
			.x1_offset = 2048,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_full,
		.ae_binning_ratio = 1000,
		.fine_integ_line = -428,
		.delay_frame = 2,
		.csi_param = {
			.cphy_settle = 58,
		},
		.ana_gain_max = BASEGAIN * 16,
		.dpc_enabled = true,
	},
	{/*reg_L2-S1 4096x3072 @15FPS Full RMSC 2LB-MF w/ All-PD VB Max. Seamless w/ "*-S1"*/
		.frame_desc = frame_desc_cus6,
		.num_entries = ARRAY_SIZE(frame_desc_cus6),
		.mode_setting_table = brzbmain_custom6_setting,
		.mode_setting_len = ARRAY_SIZE(brzbmain_custom6_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = brzbmain_seamless_custom6,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(brzbmain_seamless_custom6),
		.hdr_mode = HDR_RAW_LBMF,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 1360000000,
		.linelength = 14032,
		.framelength = 6451,
		.max_framerate = 150,
		.mipi_pixel_rate = 973710000,
		.readout_length = 3132,
		.read_margin = 36*2,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 15,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 15,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 15,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 1536,
			.w0_size = 8192,
			.h0_size = 3072,
			.scale_w = 8192,
			.scale_h = 3072,
			.x1_offset = 2048,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_full,
		.ae_binning_ratio = 1000,
		.fine_integ_line = -428,
		.delay_frame = 2,
		.csi_param = {
			.cphy_settle = 58,
		},
		.exposure_order_in_lbmf = IMGSENSOR_LBMF_EXPOSURE_SE_FIRST,
		.mode_type_in_lbmf = IMGSENSOR_LBMF_MODE_MANUAL,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 16,
		.dpc_enabled = true,
		.ana_gain_max = BASEGAIN * 16,
	},
	{/*reg_G1-S3  4096x2304 @30FPS QBIN DAG1:4 w/ Partial-PD VB Max. Seamless w/ "*-S3"*/
		.frame_desc = frame_desc_cus7,
		.num_entries = ARRAY_SIZE(frame_desc_cus7),
		.mode_setting_table = brzbmain_custom7_setting,
		.mode_setting_len = ARRAY_SIZE(brzbmain_custom7_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_RAW_DCG_COMPOSE,
		.raw_cnt = 1,
		.exp_cnt = 2,
		.pclk = 1360000000,
		.linelength = 14256,
		.framelength = 3174,
		.max_framerate = 300,
		.mipi_pixel_rate = 811430000,
		.readout_length = 0,
		.read_margin = 64,
		.framelength_step = 8,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 12,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 12,
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
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1428,
		.fine_integ_line = -352,
		.delay_frame = 2,
		.csi_param = {},
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW12_R,
		.saturation_info = &imgsensor_saturation_info_12bit,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_ratio_min = 4000,
			.dcg_gain_ratio_max = 4000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = brzbmain_dcg_ratio_table_ratio4,
			.dcg_gain_table_size = sizeof(brzbmain_dcg_ratio_table_ratio4),
		},
		.dpc_enabled = true,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 64,
	},
	{/*reg_F1 8192x6144 @15FPS Full RMSC w/ PD VB Max.*/
		.frame_desc = frame_desc_cus8,
		.num_entries = ARRAY_SIZE(frame_desc_cus8),
		.mode_setting_table = brzbmain_custom8_setting,
		.mode_setting_len = ARRAY_SIZE(brzbmain_custom8_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 1360000000,
		.linelength = 14032,
		.framelength = 6451,
		.max_framerate = 150,
		.mipi_pixel_rate = 1100570000,
		.readout_length = 0,
		.read_margin = 36,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 15,
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
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_full,
		.ae_binning_ratio = 1000,
		.fine_integ_line = -428,
		.delay_frame = 3,
		.csi_param = {
			.cphy_settle = 58,
		},
		.dpc_enabled = true,
		.ana_gain_max = BASEGAIN * 16,
	},
	{/*reg_F1-RAW 8192x6144 @15FPS Full-RAW w/ PD VB Max.*/
		.frame_desc = frame_desc_cus9,
		.num_entries = ARRAY_SIZE(frame_desc_cus9),
		.mode_setting_table = brzbmain_custom9_setting,
		.mode_setting_len = ARRAY_SIZE(brzbmain_custom9_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 1360000000,
		.linelength = 14032,
		.framelength = 6451,
		.max_framerate = 150,
		.mipi_pixel_rate = 1100570000,
		.readout_length = 0,
		.read_margin = 36,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 15,
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
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_full,
		.ae_binning_ratio = 1000,
		.fine_integ_line = -428,
		.delay_frame = 3,
		.csi_param = {
			.cphy_settle = 58,
		},
		.dpc_enabled = true,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_B,
		.ana_gain_max = BASEGAIN * 16,
	},
	{/*reg_F2-RAW-S1 4096x3072 @30FPS Full-RAW-Crop w/ PD VB Max.*/
		.frame_desc = frame_desc_cus10,
		.num_entries = ARRAY_SIZE(frame_desc_cus10),
		.mode_setting_table = brzbmain_custom10_setting,
		.mode_setting_len = ARRAY_SIZE(brzbmain_custom10_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = brzbmain_seamless_custom10,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(brzbmain_seamless_custom10),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 1360000000,
		.linelength = 14032,
		.framelength = 3225,
		.max_framerate = 300,
		.mipi_pixel_rate = 973710000,
		.readout_length = 0,
		.read_margin = 36,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 1536,
			.w0_size = 8192,
			.h0_size = 3072,
			.scale_w = 8192,
			.scale_h = 3072,
			.x1_offset = 2048,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_full,
		.ae_binning_ratio = 1428,
		.fine_integ_line = -590,
		.delay_frame = 3,
		.csi_param = {
			.cphy_settle = 58,
		},
		.dpc_enabled = true,
		.ana_gain_max = BASEGAIN * 16,
                .sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_B,
	},
	{/*F3-RAW-S2 4096x3072 @24FPS Full-RAW-Crop w/ PD VB Max. Seamless*/
		.frame_desc = frame_desc_cus11,
		.num_entries = ARRAY_SIZE(frame_desc_cus11),
		.mode_setting_table = brzbmain_custom11_setting,
		.mode_setting_len = ARRAY_SIZE(brzbmain_custom11_setting),
		.seamless_switch_group = 3,
		.seamless_switch_mode_setting_table = brzbmain_seamless_custom11,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(brzbmain_seamless_custom11),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 1360000000,
		.linelength = 14032,
		.framelength = 4032,
		.max_framerate = 240,
		.mipi_pixel_rate = 682970000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 6,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 1536,
			.w0_size = 8192,
			.h0_size = 3072,
			.scale_w = 8192,
			.scale_h = 3072,
			.x1_offset = 2048,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_full,
		.ae_binning_ratio = 1,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 16,
		.pdc_enabled = TRUE,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_B,
	},
	{/*8192x6144 @12FPS Full-RAW w/ PD VB Max. Seamless w*/
		.frame_desc = frame_desc_cus12,
		.num_entries = ARRAY_SIZE(frame_desc_cus12),
		.mode_setting_table = brzbmain_custom12_setting,
		.mode_setting_len = ARRAY_SIZE(brzbmain_custom12_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = brzbmain_seamless_custom12,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(brzbmain_seamless_custom12),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 1360000000,
		.linelength = 17824,
		.framelength = 6329,
		.max_framerate = 120,
		.mipi_pixel_rate = 973710000,
		.readout_length = 0,
		.read_margin = 36,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 15,
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
		.ae_binning_ratio = 1000,
		.fine_integ_line = -428,
		.csi_param = {
			.cphy_settle = 57,
		},
		.dpc_enabled = true,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 16,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_B,
		.awb_enabled = true,
	},
	{/*reg_V3 2048x1536 @24FPS QBIN V2H2 w/o PD VB Max.*/
		.frame_desc = frame_desc_cus13,
		.num_entries = ARRAY_SIZE(frame_desc_cus13),
		.mode_setting_table = brzbmain_custom13_setting,
		.mode_setting_len = ARRAY_SIZE(brzbmain_custom13_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 1360000000,
		.linelength = 4264,
		.framelength = 13156,
		.max_framerate = 240,
		.mipi_pixel_rate = 871540000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 8,
		.coarse_integ_step = 8,
		.min_exposure_line = 12,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 2048,
			.scale_h = 1536,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 2048,
			.h1_size = 1536,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 2048,
			.h2_tg_size = 1536,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1,
		.fine_integ_line = 695,
		.delay_frame = 2,
		.csi_param = {
			.cphy_settle = 58,
		},
		.ana_gain_max = BASEGAIN * 64,
/* 		.sensor_setting_info = {
			.sensor_scenario_usage = NORMAL_MASK,
			.equivalent_fps = 24,
		}, */
	},
	{/*reg_B3-S1 4096x3072 @60FPS QBIN w/ Partial-PD VB Max.*/
		.frame_desc = frame_desc_cus14,
		.num_entries = ARRAY_SIZE(frame_desc_cus14),
		.mode_setting_table = brzbmain_custom14_setting,
		.mode_setting_len = ARRAY_SIZE(brzbmain_custom14_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = brzbmain_seamless_custom14,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(brzbmain_seamless_custom14),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 1360000000,
		.linelength = 7136,
		.framelength = 3174,
		.max_framerate = 600,
		.mipi_pixel_rate = 973710000,
		.readout_length = 0,
		.read_margin = 36,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
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
		.imgsensor_pd_info = &imgsensor_partial_pd_info,
		.ae_binning_ratio = 1428,
		.fine_integ_line = -590,
		.delay_frame = 3,
		.csi_param = {
			.cphy_settle = 58,
		},
		.dpc_enabled = true,
		.ana_gain_max = BASEGAIN * 64,
	},
};

static struct subdrv_static_ctx static_ctx = {
	.sensor_id = BRZBMAIN_SENSOR_ID,
	.reg_addr_sensor_id = {0x0016, 0x0017},
	.i2c_addr_table = {0x34, 0xFF},
	.i2c_burst_write_support = TRUE,
	.i2c_transfer_data_type = I2C_DT_ADDR_16_DATA_8,
	.eeprom_info = eeprom_info,
	.eeprom_num = ARRAY_SIZE(eeprom_info),
	.resolution = {8192, 6144},
	.mirror = IMAGE_HV_MIRROR,

	.mclk = 24,
	.isp_driving_current = ISP_DRIVING_4MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_CPHY,
	.mipi_lane_num = SENSOR_MIPI_3_LANE,
	.ob_pedestal = 0x40,

	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_B,
	.ana_gain_def = BASEGAIN * 4,
	.ana_gain_min = BASEGAIN * 1,
	.ana_gain_max = BASEGAIN * 64,
	.ana_gain_type = 0,
	.ana_gain_step = 1,
	.ana_gain_table = brzbmain_ana_gain_table,
	.ana_gain_table_size = sizeof(brzbmain_ana_gain_table),
	.tuning_iso_base = 100,
	.exposure_def = 0x3D0,
	.exposure_min = 6,
	.exposure_max = 128*(0xFFFC - 56), /* exposure reg is limited to 4x. max = max - margin */
	.exposure_step = 4,
	.exposure_margin = 56,
	.dig_gain_min = BASE_DGAIN * 1,
	.dig_gain_max = BASE_DGAIN * 16,
	.dig_gain_step = 4,

	.frame_length_max = 0xFFFC,
	.ae_effective_frame = 2,
	.frame_time_delay_frame = 3,
	.start_exposure_offset = 1000000,

	.pdaf_type = PDAF_SUPPORT_CAMSV_QPD,/*PDAF_SUPPORT_CAMSV_QPD*/
	.hdr_type = HDR_SUPPORT_STAGGER_FDOL,
	.seamless_switch_support = TRUE,
	.temperature_support = TRUE,

	.g_temp = get_sensor_temperature,
	.g_gain2reg = get_gain2reg,
	.g_cali = get_sensor_cali,
	.s_gph = set_group_hold,
	.s_cali = set_sensor_cali,

	.reg_addr_stream = 0x0100,
	.reg_addr_mirror_flip = 0x0101,
	.reg_addr_exposure = {
			{0x0202, 0x0203}, /*COARSE_INTEG_TIME*/
			{0x0000, 0x0000}, /*not support*/
			{0x0224, 0x0225}, /*ST_COARSE_INTEG_TIME*/
	},
	.reg_addr_exposure_in_lut = {
			{0x0E20, 0x0E21}, /*LUT_A_COARSE_INTEG_TIME*/
			{0x0E40, 0x0E41}, /*LUT_B_COARSE_INTEG_TIME*/
	},
	.long_exposure_support = TRUE,
	.reg_addr_exposure_lshift = 0x3160,
	.reg_addr_ana_gain = {
			{0x0204, 0x0205}, /*ANA_GAIN_GLOBAL*/
			{0x0000, 0x0000}, /*not support*/
			{0x0216, 0x0217}, /*ST_ANA_GAIN_GLOBAL*/
	},
	.reg_addr_ana_gain_in_lut = {
			{0x0E22, 0x0E23}, /*LUT_A_ANA_GAIN_GLOBAL*/
			{0x0E42, 0x0E43}, /*LUT_B_ANA_GAIN_GLOBAL*/
	},
	.reg_addr_dig_gain = {
			{0x020E, 0x020F}, /*DIG_GAIN_GLOBAL*/
			{0x0000, 0x0000}, /*not support*/
			{0x0218, 0x0219}, /*ST_DIG_GAIN_GLOBAL*/
	},
	.reg_addr_dig_gain_in_lut = {
			{0x0E24, 0x0E25}, /*LUT_A_DIG_GAIN_GLOBAL*/
			{0x0E64, 0x0E65}, /*LUT_B_DIG_GAIN_GLOBAL*/
	},
	.reg_addr_dcg_ratio = 0x3182, /* DCGHDR_RATIO */
	.reg_addr_frame_length = {0x0340, 0x0341},
	.reg_addr_frame_length_in_lut = {
			{0x0E28, 0x0E29},  /*LUT_A_FRM_LENGTH_LINES*/
			{0x0E68, 0x0E69},  /*LUT_B_FRM_LENGTH_LINES*/
	},
	.reg_addr_temp_en = 0x0138, /*TEMP_SEN_CTL*/
	.reg_addr_temp_read = 0x013A, /*TEMP_SEN_OUT*/
	.reg_addr_auto_extend = 0x0350, /*FRM_LENGTH_CTL*/
	.reg_addr_frame_count = 0x0005, /*FRM_CNT*/
	.reg_addr_fast_mode = 0x3010, /*FAST_MODETRANSIT_CTL*/
	.reg_addr_fast_mode_in_lbmf = 0x31A7, /*EAEB_LUT_CONTROL*/

	.init_setting_table = brzbmain_init_setting,
	.init_setting_len = ARRAY_SIZE(brzbmain_init_setting),
	.mode = mode_struct,
	.sensor_mode_num = ARRAY_SIZE(mode_struct),
	.list = feature_control_list,
	.list_len = ARRAY_SIZE(feature_control_list),

	.chk_s_off_sta = 1,
	.chk_s_off_end = 0,
	.checksum_value = 0xf10e5980,
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
	.vsync_notify = vsync_notify,
	.update_sof_cnt = common_update_sof_cnt,
};

static struct subdrv_pw_seq_entry pw_seq[] = {
	{HW_ID_MCLK, {24}, 0},
	{HW_ID_DVDD1, {1}, 2000},
	{HW_ID_RST, {0}, 2000},
	{HW_ID_AVDD, {2800000, 2800000}, 3000},
	{HW_ID_AVDD1, {1804000, 1804000}, 3000},
	{HW_ID_AFVDD, {3000000, 3000000}, 3000},
	{HW_ID_DVDD, {1104000, 1104000}, 4000},
	{HW_ID_DOVDD, {1800000, 1800000}, 3000},
	{HW_ID_MCLK_DRIVING_CURRENT, {4}, 6000},
	{HW_ID_RST, {1}, 4000}
};

const struct subdrv_entry brzbmain_mipi_raw_entry = {
	.name = "brzbmain_mipi_raw",
	.id = BRZBMAIN_SENSOR_ID,
	.pw_seq = pw_seq,
	.pw_seq_cnt = ARRAY_SIZE(pw_seq),
	.ops = &ops,
};

/* FUNCTION */

static unsigned int read_brzbmain_eeprom_info(struct subdrv_ctx *ctx, kal_uint16 meta_id,
	BYTE *data, int size)
{
	kal_uint16 addr;
	int readsize;

	if (meta_id != brzbmain_eeprom_info[meta_id].meta)
		return -1;

	if (size != brzbmain_eeprom_info[meta_id].size)
		return -1;

	addr = brzbmain_eeprom_info[meta_id].start;
	readsize = brzbmain_eeprom_info[meta_id].size;

	if(!read_cmos_eeprom_p8(ctx, addr, data, readsize)) {
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
	struct oplus_eeprom_info_struct* infoPtr;
	memcpy(para, (u8*)(&oplus_eeprom_info), sizeof(oplus_eeprom_info));
	infoPtr = (struct oplus_eeprom_info_struct*)(para);
	*len = sizeof(oplus_eeprom_info);
	return 0;
}

static kal_uint16 read_cmos_eeprom_8(struct subdrv_ctx *ctx, kal_uint16 addr)
{
	kal_uint16 get_byte = 0;

	adaptor_i2c_rd_u8(ctx->i2c_client, BRZBMAIN_EEPROM_READ_ID >> 1, addr, (u8 *)&get_byte);
	return get_byte;
}

static int brzbmain_get_otp_qcom_pdaf_data(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u32 *feature_return_para_32 = (u32 *)para;

	read_cmos_eeprom_p8(ctx, OTP_QCOM_PDAF_DATA_START_ADDR, otp_qcom_pdaf_data, OTP_QCOM_PDAF_DATA_LENGTH);

	memcpy(feature_return_para_32, (UINT32 *)otp_qcom_pdaf_data, sizeof(otp_qcom_pdaf_data));
	*len = sizeof(otp_qcom_pdaf_data);

	return 0;
}

static int brzbmain_get_otp_qcom_pdaf_offset_data(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u32 *feature_return_para_32 = (u32 *)para;

	read_cmos_eeprom_p8(ctx, OTP_QCOM_PDAF_OFFSET_DATA_START_ADDR, otp_qcom_pdaf_offset_data, OTP_QCOM_PDAF_OFFSET_DATA_LENGTH);

	memcpy(feature_return_para_32, (UINT32 *)otp_qcom_pdaf_offset_data, sizeof(otp_qcom_pdaf_offset_data));
	*len = sizeof(otp_qcom_pdaf_offset_data);

	return 0;
}

#ifdef WRITE_DATA_MAX_LENGTH
#undef WRITE_DATA_MAX_LENGTH
#endif
#define   WRITE_DATA_MAX_LENGTH	 (32)
static kal_int32 table_write_eeprom_30Bytes(struct subdrv_ctx *ctx,
        kal_uint16 addr, kal_uint8 *para, kal_uint32 len)
{
	kal_int32 ret = ERROR_NONE;
	ret = adaptor_i2c_wr_p8(ctx->i2c_client, BRZBMAIN_EEPROM_WRITE_ID >> 1,
			addr, para, len);

	return ret;
}


static kal_int32 write_1st_eeprom_protect(struct subdrv_ctx *ctx, kal_uint16 enable)
{
	kal_int32 ret = ERROR_NONE;
	kal_uint16 reg = 0xff35;
	u8 flag = 0;

	adaptor_i2c_wr_u8(ctx->i2c_client, (0xB0 | BRZBMAIN_EEPROM_WRITE_ID) >> 1, reg, 0x0);

	reg = 0x06ca;
	if (enable) {
		adaptor_i2c_wr_u8(ctx->i2c_client, (0xB0 | BRZBMAIN_EEPROM_WRITE_ID) >> 1, reg, 0x2);
	}
	else {
		adaptor_i2c_wr_u8(ctx->i2c_client, (0xB0 | BRZBMAIN_EEPROM_WRITE_ID) >> 1, reg, 0x0);
	}

	adaptor_i2c_rd_u8(ctx->i2c_client, BRZBMAIN_EEPROM_WRITE_ID >> 1, reg, &flag);
	LOG_INF("SET_SENSOR_OTP WRP: 0x%x\n", flag);

	return ret;
}

static kal_int32 write_2nd_eeprom_protect(struct subdrv_ctx *ctx, kal_uint16 enable)
{
	kal_int32 ret = ERROR_NONE;
	kal_uint16 reg = 0xa000;
	if (enable) {
		adaptor_i2c_wr_u8(ctx->i2c_client, BRZBMAIN_EEPROM_WRITE_ID >> 1, reg, 0x0E);
	}
	else {
		adaptor_i2c_wr_u8(ctx->i2c_client, BRZBMAIN_EEPROM_WRITE_ID >> 1, reg, 0x00);
	}

	return ret;
}

static kal_int32 write_eeprom_protect(struct subdrv_ctx *ctx, kal_uint16 enable)
{
	u8 flag = 0;
	int ret = 0;
	adaptor_i2c_rd_u8(ctx->i2c_client, BRZBMAIN_EEPROM_READ_ID >> 1, 0x000D, &flag);
	if (flag == 0x01) {
		ret = write_1st_eeprom_protect(ctx, enable);
	} else {
		ret = write_2nd_eeprom_protect(ctx, enable);
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
		offset = ALIGN(data_base, WRITE_DATA_MAX_LENGTH) - data_base;
		if (offset > data_length) {
			offset = data_length;
		}
		if ((pStereodata->uSensorId == BRZBMAIN_SENSOR_ID) && (data_length == CALI_DATA_MASTER_LENGTH)
				&& (data_base == BRZBMAIN_STEREO_START_ADDR || data_base == BRZBMAIN_STEREO_MT_START_ADDR
				|| data_base == BRZBMAIN_STEREO_MT_105CM_START_ADDR)) {
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
			for (i = 0; i < idx; i++) {
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
		} else if ((pStereodata->uSensorId == BRZBMAIN_SENSOR_ID) && (data_length < AESYNC_DATA_LENGTH_TOTAL)
				&& (data_base == BRZBMAIN_AESYNC_START_ADDR)) {
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
			for (i = 0; i < idx; i++) {
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
				read_cmos_eeprom_8(ctx, BRZBMAIN_AESYNC_START_ADDR),
				read_cmos_eeprom_8(ctx, BRZBMAIN_AESYNC_START_ADDR+1),
				read_cmos_eeprom_8(ctx, BRZBMAIN_AESYNC_START_ADDR+2),
				read_cmos_eeprom_8(ctx, BRZBMAIN_AESYNC_START_ADDR+3),
				read_cmos_eeprom_8(ctx, BRZBMAIN_AESYNC_START_ADDR+4),
				read_cmos_eeprom_8(ctx, BRZBMAIN_AESYNC_START_ADDR+5),
				read_cmos_eeprom_8(ctx, BRZBMAIN_AESYNC_START_ADDR+6),
				read_cmos_eeprom_8(ctx, BRZBMAIN_AESYNC_START_ADDR+7));
			LOG_INF("AESync write_Module_data Write end\n");
		} else {
			LOG_INF("Invalid Sensor id:0x%x write eeprom\n", pStereodata->uSensorId);
			return -1;
		}
	} else {
		LOG_INF("brzbmain write_Module_data pStereodata is null\n");
		return -1;
	}
	return ret;
}

static int brzbmain_set_eeprom_calibration(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	int ret = ERROR_NONE;
	ret = write_Module_data(ctx, (ACDK_SENSOR_ENGMODE_STEREO_STRUCT *)(para));
	if (ret != ERROR_NONE) {
		*len = (u32)-1; /*write eeprom failed*/
		LOG_INF("ret=%d\n", ret);
	}
	return 0;
}

static int brzbmain_get_eeprom_calibration(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	UINT16 *feature_data_16 = (UINT16 *) para;
	UINT32 *feature_return_para_32 = (UINT32 *) para;
	if(*len > CALI_DATA_MASTER_LENGTH)
		*len = CALI_DATA_MASTER_LENGTH;
	LOG_INF("feature_data mode: %d", *feature_data_16);
	switch (*feature_data_16) {
	case EEPROM_STEREODATA_MT_MAIN:
		read_brzbmain_eeprom_info(ctx, EEPROM_META_STEREO_MT_MAIN_DATA,
				(BYTE *)feature_return_para_32, *len);
		break;
	case EEPROM_STEREODATA_MT_MAIN_105CM:
		read_brzbmain_eeprom_info(ctx, EEPROM_META_STEREO_MT_MAIN_DATA_105CM,
				(BYTE *)feature_return_para_32, *len);
		break;
	case EEPROM_STEREODATA_MW_MAIN:
	default:
		read_brzbmain_eeprom_info(ctx, EEPROM_META_STEREO_MW_MAIN_DATA,
				(BYTE *)feature_return_para_32, *len);
		break;
	}
	return 0;
}

static bool read_cmos_eeprom_p8(struct subdrv_ctx *ctx, kal_uint16 addr,
                    BYTE *data, int size)
{
	if (adaptor_i2c_rd_p8(ctx->i2c_client, BRZBMAIN_EEPROM_READ_ID >> 1,
			addr, data, size) < 0) {
		return false;
	}
	return true;
}

static void read_otp_info(struct subdrv_ctx *ctx)
{
	DRV_LOGE(ctx, "brzbmain read_otp_info begin\n");
	read_cmos_eeprom_p8(ctx, 0, otp_data_checksum, OTP_SIZE);
	DRV_LOGE(ctx, "brzbmain read_otp_info end\n");
}

static int brzbmain_get_otp_checksum_data(struct subdrv_ctx *ctx, u8 *para, u32 *len)
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

static int brzbmain_check_sensor_id(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	get_imgsensor_id(ctx, (u32 *)para);
	return 0;
}

static int get_imgsensor_id(struct subdrv_ctx *ctx, u32 *sensor_id)
{
	u8 i = 0;
	u8 retry = 2;
	static bool first_read = KAL_TRUE;
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
			if (*sensor_id == 0x896) {
				*sensor_id = ctx->s_ctx.sensor_id;
				if (first_read) {
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

static u16 brzbmain_feedback_awbgain[] = {
	0x0B8E, 0x01,
	0x0B8F, 0x00,
	0x0B90, 0x02,
	0x0B91, 0x28,
	0x0B92, 0x01,
	0x0B93, 0x77,
	0x0B94, 0x01,
	0x0B95, 0x00,
};

/*write AWB gain to sensor*/
static void feedback_awbgain(struct subdrv_ctx *ctx, kal_uint32 r_gain, kal_uint32 b_gain)
{
	UINT32 r_gain_int = 0;
	UINT32 b_gain_int = 0;

	if (ctx->current_scenario_id == SENSOR_SCENARIO_ID_CUSTOM3 ||
		ctx->current_scenario_id == SENSOR_SCENARIO_ID_CUSTOM5 ||
		ctx->current_scenario_id == SENSOR_SCENARIO_ID_CUSTOM7 ||
		ctx->current_scenario_id == SENSOR_SCENARIO_ID_CUSTOM12 ||
		ctx->current_scenario_id == SENSOR_SCENARIO_ID_CUSTOM13) {
		DRV_LOG(ctx, "feedback_awbgain r_gain: %d, b_gain: %d\n", r_gain, b_gain);
		r_gain_int = r_gain / 512;
		b_gain_int = b_gain / 512;
		brzbmain_feedback_awbgain[5] = r_gain_int;
		brzbmain_feedback_awbgain[7] = (r_gain - r_gain_int * 512) / 2;
		brzbmain_feedback_awbgain[9] = b_gain_int;
		brzbmain_feedback_awbgain[11] = (b_gain - b_gain_int * 512) / 2;
		subdrv_i2c_wr_regs_u8(ctx, brzbmain_feedback_awbgain,
			ARRAY_SIZE(brzbmain_feedback_awbgain));
	}
}

static int brzbmain_set_awb_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len) {
	struct SET_SENSOR_AWB_GAIN *awb_gain = (struct SET_SENSOR_AWB_GAIN *)para;
	feedback_awbgain(ctx, awb_gain->ABS_GAIN_R, awb_gain->ABS_GAIN_B);
	return 0;
}

static int open(struct subdrv_ctx *ctx)
{
	u32 sensor_id = 0;
	u32 scenario_id = 0;

	/* get sensor id */
	if (get_imgsensor_id(ctx, &sensor_id) != ERROR_NONE)
		return ERROR_SENSOR_CONNECT_FAIL;

	/* initail setting */
	sensor_init(ctx);

	/*QSC setting*/
	if (ctx->s_ctx.s_cali != NULL) {
		ctx->s_ctx.s_cali((void*)ctx);
	} else {
		write_sensor_Cali(ctx);
	}

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

static void set_sensor_cali(void *arg)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;

	u16 idx = 0;
	u8 support = FALSE;
	u8 *pbuf = NULL;
	u16 size = 0;
	u16 addr = 0;
	struct eeprom_info_struct *info = ctx->s_ctx.eeprom_info;

	if (!probe_eeprom(ctx))
		return;

	idx = ctx->eeprom_index;

	/* QSC data */
	support = info[idx].qsc_support;
	pbuf = info[idx].preload_qsc_table;
	size = info[idx].qsc_size;
	addr = info[idx].sensor_reg_addr_qsc;
	if (support) {
		if (pbuf != NULL && addr > 0 && size > 0) {
			subdrv_i2c_wr_u8(ctx, 0x0101, 0x00);
			subdrv_i2c_wr_u8(ctx, 0x0B06, 0x01);
			subdrv_i2c_wr_u8(ctx, 0xDDA9, 0x4E);
			subdrv_i2c_wr_seq_p8(ctx, addr, pbuf, size);
			subdrv_i2c_wr_u8(ctx, 0x3206, 0x01);
			DRV_LOG(ctx, "set QSC calibration data done.");
		} else {
			subdrv_i2c_wr_u8(ctx, 0x3206, 0x00);
		}
	}

	support = info[idx].pdc_support;
	pbuf = info[idx].preload_pdc_table;
	size = info[idx].pdc_size;
	addr = 0xD200;
	if (support) {
		if (pbuf != NULL && addr > 0 && size > 0) {
			subdrv_i2c_wr_seq_p8(ctx, addr, pbuf, size >> 1);
			addr = 0xD300;
			subdrv_i2c_wr_seq_p8(ctx, addr, pbuf + (size >> 1), size >> 1);
			DRV_LOG(ctx, "set SPC data done.");
		}
	}
}

static int get_sensor_temperature(void *arg)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;
	u8 temperature = 0;
	int temperature_convert = 0;

	temperature = subdrv_i2c_rd_u8(ctx, ctx->s_ctx.reg_addr_temp_read);

	if (temperature <= 0x60)
		temperature_convert = temperature;
	else if (temperature >= 0x61 && temperature <= 0x7F)
		temperature_convert = 97;
	else if (temperature >= 0x80 && temperature <= 0xE2)
		temperature_convert = -30;
	else
		temperature_convert = (char)temperature | 0xFFFFFF0;

	DRV_LOG(ctx, "temperature: %d degrees\n", temperature_convert);
	return temperature_convert;
}

static void set_group_hold(void *arg, u8 en)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;

	if (en)
		set_i2c_buffer(ctx, 0x0104, 0x01);
	else
		set_i2c_buffer(ctx, 0x0104, 0x00);
}

static u16 get_gain2reg(u32 gain)
{
	return (16384 - (16384 * BASEGAIN) / gain);
}

void brzbmain_get_min_shutter_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id,
		u64 *min_shutter, u64 *exposure_step)
{
	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOGE(ctx, "invalid sid:%u, mode_num:%u set default\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		scenario_id = 0;
	}
	DRV_LOG(ctx, "sensor_mode_num[%d]", ctx->s_ctx.sensor_mode_num);
	if (scenario_id < ctx->s_ctx.sensor_mode_num) {
		switch (ctx->s_ctx.mode[scenario_id].hdr_mode) {
		case HDR_RAW_STAGGER:
		case HDR_NONE:
		case HDR_RAW_LBMF:
		case HDR_RAW_DCG_RAW:
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

int brzbmain_get_min_shutter_by_scenario_adapter(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 *feature_data = (u64 *) para;
	brzbmain_get_min_shutter_by_scenario(ctx,
		(enum SENSOR_SCENARIO_ID_ENUM)*(feature_data),
		feature_data + 1, feature_data + 2);
	return 0;
}

static int brzbmain_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	enum SENSOR_SCENARIO_ID_ENUM scenario_id;
	struct mtk_hdr_ae *ae_ctrl = NULL;
	u64 *feature_data = (u64 *)para;
	u32 exp_cnt = 0;
	u32 frame_length_in_lut[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
	enum SENSOR_SCENARIO_ID_ENUM pre_seamless_scenario_id;

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
	pre_seamless_scenario_id = ctx->current_scenario_id;


	subdrv_i2c_wr_u8(ctx, 0x0104, 0x01);
	subdrv_i2c_wr_u8(ctx, ctx->s_ctx.reg_addr_fast_mode, 0x02);
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
			set_multi_shutter_frame_length_in_lut(ctx,
				(u64 *)&ae_ctrl->exposure, exp_cnt, 0, frame_length_in_lut);
			set_multi_gain_in_lut(ctx, (u32 *)&ae_ctrl->gain, exp_cnt);
			break;
		case HDR_RAW_DCG_RAW:
			set_shutter(ctx, ae_ctrl->exposure.le_exposure);
			if (ctx->s_ctx.mode[scenario_id].dcg_info.dcg_gain_mode
				== IMGSENSOR_DCG_DIRECT_MODE)
				set_multi_gain(ctx, (u32 *)&ae_ctrl->gain, exp_cnt);
			else
				set_gain(ctx, ae_ctrl->gain.le_gain);
			break;
		default:
			set_shutter(ctx, ae_ctrl->exposure.le_exposure);
			set_gain(ctx, ae_ctrl->gain.le_gain);
			break;
		}
		calculate_prsh_length_lines(ctx, ae_ctrl, pre_seamless_scenario_id);
	}

	if (ctx->s_ctx.seamless_switch_prsh_length_lc > 0) {
		subdrv_i2c_wr_u8(ctx, ctx->s_ctx.reg_addr_prsh_mode, 0x01);

		subdrv_i2c_wr_u8(ctx,
				ctx->s_ctx.reg_addr_prsh_length_lines.addr[0] - 1,
				(ctx->s_ctx.seamless_switch_prsh_length_lc >> 24) & 0x07);
		subdrv_i2c_wr_u8(ctx,
				ctx->s_ctx.reg_addr_prsh_length_lines.addr[0],
				(ctx->s_ctx.seamless_switch_prsh_length_lc >> 16) & 0xFF);
		subdrv_i2c_wr_u8(ctx,
				ctx->s_ctx.reg_addr_prsh_length_lines.addr[1],
				(ctx->s_ctx.seamless_switch_prsh_length_lc >> 8)  & 0xFF);
		subdrv_i2c_wr_u8(ctx,
				ctx->s_ctx.reg_addr_prsh_length_lines.addr[2],
				(ctx->s_ctx.seamless_switch_prsh_length_lc) & 0xFF);

		DRV_LOG(ctx, "seamless switch pre-shutter set(%u)\n", ctx->s_ctx.seamless_switch_prsh_length_lc);
	} else
		subdrv_i2c_wr_u8(ctx, ctx->s_ctx.reg_addr_prsh_mode, 0x00);

	subdrv_i2c_wr_u8(ctx, 0x0104, 0x00);

	ctx->fast_mode_on = TRUE;
	ctx->ref_sof_cnt = ctx->sof_cnt;
	ctx->is_seamless = FALSE;
	DRV_LOG(ctx, "X: set seamless switch done\n");
	return ERROR_NONE;
}

static int brzbmain_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u32 mode = *((u32 *)para);
	DRV_LOG(ctx, "mode(%u->%u)\n", ctx->test_pattern, mode);
	if (mode) {
	/* 1:Solid Color 2:Color Bar 5:Black */
		switch (mode) {
		case 5:
			subdrv_i2c_wr_u8(ctx, 0x0601, 0x01);
			break;
		default:
			subdrv_i2c_wr_u8(ctx, 0x0601, mode);
			break;
		}
	} else if (ctx->test_pattern) {
		subdrv_i2c_wr_u8(ctx, 0x0601, 0x00); /*No pattern*/
	}
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

static int vsync_notify(struct subdrv_ctx *ctx,	unsigned int sof_cnt)
{
	DRV_LOG(ctx, "sof_cnt(%u) ctx->ref_sof_cnt(%u) ctx->fast_mode_on(%d)",
		sof_cnt, ctx->ref_sof_cnt, ctx->fast_mode_on);
	ctx->sof_cnt = sof_cnt;
	if (ctx->fast_mode_on && (sof_cnt > ctx->ref_sof_cnt)) {
		ctx->fast_mode_on = FALSE;
		ctx->ref_sof_cnt = 0;
		DRV_LOG(ctx, "seamless_switch disabled.");
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_fast_mode, 0x00);
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_fast_mode_in_lbmf, 0x00);
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_prsh_mode, 0x00);
		commit_i2c_buffer(ctx);
	}
	return 0;
}

void get_sensor_cali(void *arg)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;

	u16 idx = 0;
	u8 support = FALSE;
	u8 *buf = NULL;
	u16 size = 0;
	u16 addr = 0;
	u8 qsc_is_valid = 0;
	u8 pdc_is_valid = 0;
	struct eeprom_info_struct *info = ctx->s_ctx.eeprom_info;

	/* Probe EEPROM device */
	if (!probe_eeprom(ctx))
		return;

	idx = ctx->eeprom_index;

	/* QSC data */
	support = info[idx].qsc_support;
	size = info[idx].qsc_size;
	addr = info[idx].addr_qsc;
	buf = info[idx].qsc_table;
	if (support && size > 0) {
		/*Check QSC validation*/
		qsc_is_valid = QSC_IS_VALID_VAL; /*i2c_read_eeprom(ctx, OTP_QSC_VALID_ADDR);*/
		if (qsc_is_valid != 0x01) {
			DRV_LOGE(ctx, "QSC data is invalid, flag(%02x)", qsc_is_valid);
		} else if (info[idx].preload_qsc_table == NULL) {
			info[idx].preload_qsc_table = kmalloc(size, GFP_KERNEL);
			if (buf == NULL) {
				if (!read_cmos_eeprom_p8(ctx, addr, info[idx].preload_qsc_table, size)) {
					DRV_LOGE(ctx, "preload QSC data failed");
				}
			} else {
				memcpy(info[idx].preload_qsc_table, buf, size);
			}
			DRV_LOG(ctx, "preload QSC data %u bytes", size);
		} else {
			DRV_LOG(ctx, "QSC data is already preloaded %u bytes", size);
		}
	}

	support = info[idx].pdc_support;
	size = info[idx].pdc_size;
	addr = info[idx].addr_pdc;
	buf = info[idx].pdc_table;
	if (support && size > 0) {
		/* Check pdc validation */
		pdc_is_valid = QSC_IS_VALID_VAL; /*i2c_read_eeprom(ctx, OTP_PDC_VALID_ADDR);*/
		if (pdc_is_valid != 0x01) {
			DRV_LOGE(ctx, "PDC data is invalid, flag(%02x)", pdc_is_valid);
		} else if (info[idx].preload_pdc_table == NULL) {
			info[idx].preload_pdc_table = kmalloc(size, GFP_KERNEL);
			if (buf == NULL) {
				if (!read_cmos_eeprom_p8(ctx, addr, info[idx].preload_pdc_table, size)) {
					DRV_LOGE(ctx, "preload PDC data failed");
				}
			} else {
				memcpy(info[idx].preload_pdc_table, buf, size);
			}
			DRV_LOG(ctx, "preload PDC data %u bytes", size);
		} else {
			DRV_LOG(ctx, "PDC data is already preloaded %u bytes", size);
		}
	}
	ctx->is_read_preload_eeprom = 1;
}

static void calculate_prsh_length_lines(struct subdrv_ctx *ctx,
	struct mtk_hdr_ae *ae_ctrl,
	enum SENSOR_SCENARIO_ID_ENUM pre_seamless_scenario_id)
{
	u32 ae_ctrl_cit;
	u32 prsh_length_lc = 0;
	u32 cit_step = 1;
	u8 hw_fixed_value = ctx->s_ctx.seamless_switch_prsh_hw_fixed_value;
	enum SENSOR_SCENARIO_ID_ENUM scenario_id = ctx->current_scenario_id;
	enum IMGSENSOR_HDR_MODE_ENUM hdr_mode;

	if (pre_seamless_scenario_id == SENSOR_SCENARIO_ID_CUSTOM4 && scenario_id == SENSOR_SCENARIO_ID_CUSTOM7) {
		prsh_length_lc = 1200;
	} else if (pre_seamless_scenario_id == SENSOR_SCENARIO_ID_CUSTOM7 && scenario_id == SENSOR_SCENARIO_ID_CUSTOM4) {
		prsh_length_lc = 400;
	} else {
		prsh_length_lc = 0;
	}

	hdr_mode = ctx->s_ctx.mode[scenario_id].hdr_mode;
	switch (hdr_mode) {
	case HDR_RAW_LBMF:
		if (ctx->s_ctx.mode[scenario_id].exposure_order_in_lbmf ==
			IMGSENSOR_LBMF_EXPOSURE_SE_FIRST) {
			/* 2exp: dig_gain_lut_a = SE / dig_gain_lut_b = LE */
			/* 3exp: dig_gain_lut_a = SE / dig_gain_lut_b = ME / dig_gain_lut_c = LE */
			ae_ctrl_cit = ae_ctrl->exposure.me_exposure;
			DRV_LOGE(ctx, "debug se %llu le %llu, me %llu", ae_ctrl->exposure.se_exposure, ae_ctrl->exposure.le_exposure, ae_ctrl->exposure.me_exposure);
		} else if (ctx->s_ctx.mode[scenario_id].exposure_order_in_lbmf ==
			IMGSENSOR_LBMF_EXPOSURE_LE_FIRST) {
			/* 2exp: dig_gain_lut_a = LE / dig_gain_lut_b = SE */
			/* 3exp: dig_gain_lut_a = LE / dig_gain_lut_b = ME / dig_gain_lut_c = SE */
			ae_ctrl_cit = ae_ctrl->exposure.le_exposure;
			DRV_LOGE(ctx, "debug le\n");
		} else {
			DRV_LOGE(ctx, "pls assign exposure_order_in_lbmf value!\n");
			return;
		}
		break;
	case HDR_NONE:
	case HDR_RAW:
	case HDR_CAMSV:
	case HDR_RAW_ZHDR:
	case HDR_MultiCAMSV:
	case HDR_RAW_STAGGER:
	case HDR_RAW_DCG_RAW:
	case HDR_RAW_DCG_COMPOSE:
	default:
		ae_ctrl_cit = ae_ctrl->exposure.le_exposure;
		break;
	}
	ae_ctrl_cit = max(ae_ctrl_cit, ctx->s_ctx.exposure_min);
	ae_ctrl_cit = min(ae_ctrl_cit, ctx->s_ctx.exposure_max);
	cit_step = ctx->s_ctx.mode[scenario_id].coarse_integ_step ?: 1;
	if (cit_step) {
		ae_ctrl_cit = round_up(ae_ctrl_cit, cit_step);
		prsh_length_lc = round_up(prsh_length_lc, cit_step);
	}
	DRV_LOGE(ctx, "prsh_length_lc %u ae_ctrl_cit %u\n", prsh_length_lc, ae_ctrl_cit);
	prsh_length_lc = (prsh_length_lc > (ae_ctrl_cit + hw_fixed_value)) ? prsh_length_lc : 0;
	if (prsh_length_lc < (ae_ctrl_cit + hw_fixed_value)) {
		DRV_LOG_MUST(ctx,
			"pre-shutter no need: prsh_length_lc(%u) < (ae_ctrl_cit(%u(max=%u,min=%u)) + hw_fixed_value(%u))\n",
			prsh_length_lc, ae_ctrl_cit, ctx->s_ctx.exposure_max, ctx->s_ctx.exposure_min, hw_fixed_value);
		ctx->s_ctx.seamless_switch_prsh_length_lc = 0;
		return;
	}

	ctx->s_ctx.seamless_switch_prsh_length_lc = prsh_length_lc;
}
