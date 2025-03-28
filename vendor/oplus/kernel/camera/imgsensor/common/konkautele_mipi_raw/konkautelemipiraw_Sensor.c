// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2022 MediaTek Inc.

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 konkautelemipiraw_Sensor.c
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
#include "konkautelemipiraw_Sensor.h"

#define KONKAUTELE_AF_SLAVE_ID	0x32
#define DW9786_CHIP_EN 0xE000
#define KONKAUTELE_EEPROM_READ_ID	0x75
#define KONKAUTELE_EEPROM_WRITE_ID	0x74
#define KONKAUTELE_MAX_OFFSET		0x4000
#define OPLUS_CAMERA_COMMON_DATA_LENGTH 40
#define PFX "konkautele_camera_sensor"
#define LOG_INF(format, args...) pr_err(PFX "[%s] " format, __func__, ##args)
#define OTP_SIZE    0x4000
#define OTP_QSC_VALID_ADDR 0x2200
#define KONKAUTELE_UNIQUE_SENSOR_ID 0x0A1F
#define KONKAUTELE_UNIQUE_SENSOR_ID_LENGHT 11
#define OTP_PDC_VALID_ADDR 0x021A
#define GET_SENSOR_ID_RETRY_CNT    5

static kal_uint8 otp_data_checksum[OTP_SIZE] = {0};
extern struct mutex dw9786_mutex;
static void set_sensor_cali(void *arg);
static int get_sensor_temperature(void *arg);
static void set_group_hold(void *arg, u8 en);
static u16 get_gain2reg(u32 gain);
static int konkautele_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int konkautele_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int konkautele_check_sensor_id(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int get_eeprom_common_data(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int konkautele_set_eeprom_calibration(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int konkautele_get_eeprom_calibration(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int konkautele_get_otp_checksum_data(struct subdrv_ctx *ctx, u8 *para, u32 *len);
//static void konkautele_get_unique_sensorid(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int konkautele_get_min_shutter_by_scenario_adapter(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int get_imgsensor_id(struct subdrv_ctx *ctx, u32 *sensor_id);
static int open(struct subdrv_ctx *ctx);
static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id);
static int vsync_notify(struct subdrv_ctx *ctx,	unsigned int sof_cnt);
static int konkautele_set_awb_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static void get_sensor_cali(void* arg);
static bool read_cmos_eeprom_p8(struct subdrv_ctx *ctx, kal_uint16 addr,
                    BYTE *data, int size);
static void konkautele_read_eeprom_protect(struct subdrv_ctx *ctx);
static void calculate_prsh_length_lines(struct subdrv_ctx *ctx,
	struct mtk_hdr_ae *ae_ctrl,
	enum SENSOR_SCENARIO_ID_ENUM scenario_id);
/* STRUCT */

// static BYTE konkautele_common_data[OPLUS_CAMERA_COMMON_DATA_LENGTH] = { 0 };
//static BYTE konkautele_unique_id[KONKAUTELE_UNIQUE_SENSOR_ID_LENGHT] = { 0 };

/* Normal(Qbin) to Normal(Qbin) */
/* Normal(Qbin) to 2DOL(Qbin) */
static void comp_mode_tran_time_cal1(struct subdrv_ctx *ctx, u32 pre_scenario_id, u32* prsh);
typedef void (*cal_comp_mode_tran_time)(struct subdrv_ctx *ctx, u32 pre_scenario_id, u32* prsh);
struct comp_mode_tran_time_params {
	u8 enable;
	u32 clock_vtpxck;
	cal_comp_mode_tran_time cal_fn;
};
static struct comp_mode_tran_time_params konkautele_comp_params[SENSOR_SCENARIO_ID_MAX] = {
	{ .enable = 0, }, /*pre*/
	{ .enable = 1, .clock_vtpxck = 1884, .cal_fn = comp_mode_tran_time_cal1, }, /*cap*/
	{ .enable = 0, }, /*vid*/
	{ .enable = 0, }, /*hvid*/
	{ .enable = 0, }, /*svid*/
	{ .enable = 0, }, /*cus1*/
	{ .enable = 0, }, /*cus2*/
	{ .enable = 0, }, /*csu3*/
	{ .enable = 0, }, /*cus4*/
	{ .enable = 0, }, /*cus5*/
	{ .enable = 0, }, /*cus6*/
	{ .enable = 1, .clock_vtpxck = 1404, .cal_fn = comp_mode_tran_time_cal1, }, /*cus7*/
	{ .enable = 0, }, /*cus8*/
	{ .enable = 0, }, /*cus9*/
	{ .enable = 0, }, /*cus10*/
	{ .enable = 0, }, /*cus11*/
	{ .enable = 0, }, /*cus12*/
	{ .enable = 0, }, /*cus13*/
};

static struct eeprom_map_info konkautele_eeprom_info[] = {
	{ EEPROM_META_MODULE_ID, 0x0000, 0x0010, 0x0011, 2, true },
	{ EEPROM_META_SENSOR_ID, 0x0006, 0x0010, 0x0011, 2, true },
	{ EEPROM_META_LENS_ID, 0x0008,0x0010, 0x0011, 2, true },
	{ EEPROM_META_VCM_ID, 0x000A, 0x0010, 0x0011, 2, true },
	{ EEPROM_META_MIRROR_FLIP, 0x000E, 0x0010, 0x0011, 1, true },
	{ EEPROM_META_MODULE_SN, 0x00B0, 0x00C1, 0x00C2, 17, true },
	{ EEPROM_META_AF_CODE, 0x0092, 0x0098, 0x0099, 6, true },
	{ EEPROM_META_AF_FLAG, 0x009C, 0x0098, 0x0099, 1, true },
	{ EEPROM_META_STEREO_DATA, 0x3C00, 0x0000, 0x0000, CALI_DATA_SLAVE_TELE_LENGTH, false },
	{ EEPROM_META_STEREO_MW_MAIN_DATA, 0x2B00, 0x3199, 0x319A, CALI_DATA_MASTER_LENGTH, false },
	{ EEPROM_META_STEREO_MT_MAIN_DATA, 0x31C0, 0x3859, 0x385A, CALI_DATA_MASTER_LENGTH, false },
};


static struct subdrv_feature_control feature_control_list[] = {
	{SENSOR_FEATURE_SET_TEST_PATTERN, konkautele_set_test_pattern},
	{SENSOR_FEATURE_SEAMLESS_SWITCH, konkautele_seamless_switch},
	{SENSOR_FEATURE_CHECK_SENSOR_ID, konkautele_check_sensor_id},
	{SENSOR_FEATURE_GET_EEPROM_COMDATA, get_eeprom_common_data},
	{SENSOR_FEATURE_SET_SENSOR_OTP, konkautele_set_eeprom_calibration},
	{SENSOR_FEATURE_GET_EEPROM_STEREODATA, konkautele_get_eeprom_calibration},
	{SENSOR_FEATURE_GET_SENSOR_OTP_ALL, konkautele_get_otp_checksum_data},
	//{SENSOR_FEATURE_GET_UNIQUE_SENSORID, konkautele_get_unique_sensorid},
	{SENSOR_FEATURE_GET_MIN_SHUTTER_BY_SCENARIO, konkautele_get_min_shutter_by_scenario_adapter},
	{SENSOR_FEATURE_SET_AWB_GAIN, konkautele_set_awb_gain},
};

static struct eeprom_info_struct eeprom_info[] = {
	{
		.header_id = 0x00EA0129,
		.addr_header_id = 0x0000006,
		.i2c_write_id = 0x74,

		.qsc_support = TRUE,
		.qsc_size = 0x0C00,
		.addr_qsc = 0x24B8,/* QSC_EEPROM_ADDR */
		.sensor_reg_addr_qsc = 0xC000, /*QSC_OTP_ADDR*/

		.pdc_support = TRUE,
		.pdc_size = 0x180,
		.addr_pdc = 0x1E9A,
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
		{0, 0}, {0, 0}, {0, 384}, {0, 384}, {0, 0},
		/* <cust1> <cust2> <cust3> <cust4> <cust5> */
		{2048, 1536}, {1640, 1230}, {0, 0}, {0, 0}, {0, 0},
		/* <cust6> */
		{2048, 1920},
	},
	.iMirrorFlip = IMAGE_H_MIRROR,//0:IMAGE_NORMAL,1:IMAGE_H_MIRROR,2:IMAGE_V_MIRROR,3:IMAGE_HV_MIRROR
	.i4FullRawW = 4096,
	.i4FullRawH = 3072,
	.i4VCPackNum = 1,
	.PDAF_Support = PDAF_SUPPORT_CAMSV_QPD,
	.i4ModeIndex = 0x2, // Vbin
	.sPDMapInfo[0] = {
		.i4PDPattern = 1,//all-pd
		.i4BinFacX = 2,
		.i4BinFacY = 4,
		.i4PDRepetition = 0,
		.i4PDOrder = {1}, //R=1, L=0
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
		{0, 0}, {0, 0}, {0, 384}, {0, 384}, {0, 0},
		/* <cust1> <cust2> <cust3> <cust4> <cust5> */
		{2048, 1536}, {1640, 1230}, {0, 0}, {0, 0}, {0, 0},
		/* <cust6> */
		{2048, 1920},
	},
	.iMirrorFlip = IMAGE_H_MIRROR,
	.i4FullRawW = 8192,
	.i4FullRawH = 6144,
	.i4VCPackNum = 1,
	.PDAF_Support = PDAF_SUPPORT_CAMSV_QPD,
	.i4ModeIndex = 0x2,
	.sPDMapInfo[0] = {
		.i4PDPattern = 1,// all-pd
		.i4BinFacX = 4,
		.i4BinFacY = 2,
		.i4PDRepetition = 0,
		.i4PDOrder = {1}, // R=1, L=0
	},
};

static struct SET_PD_BLOCK_INFO_T imgsensor_partial_pd_info = {
	.i4OffsetX = 16,
	.i4OffsetY = 32,
	.i4PitchX = 8,
	.i4PitchY = 32,
	.i4PairNum = 4,
	.i4SubBlkW = 8,
	.i4SubBlkH = 16,
	.i4PosL = {{20, 41}, {20, 43}, {19, 48}, {19, 50}},
	.i4PosR = {{16, 33}, {16, 35}, {23, 56}, {23, 58}},
	.i4BlockNumX = 496,
	.i4BlockNumY = 72,
	.i4Crop = {
		/* <pre> <cap> <normal_video> <hs_video> <slim_video> */
		{0, 0}, {0, 0}, {0, 384}, {0, 384}, {0, 0},
		/* <cust1> <cust2> <cust3> <cust4> <cust5> */
		{2048, 1536}, {1640, 1230}, {0, 0}, {0, 0}, {0, 0},
		/* <cust6> */
		{2048, 1920},
	},
	.i4VolumeX = 1,
	.i4VolumeY = 2,
	.iMirrorFlip = IMAGE_H_MIRROR,//0:IMAGE_NORMAL,1:IMAGE_H_MIRROR,2:IMAGE_V_MIRROR,3:IMAGE_HV_MIRROR
	.i4FullRawW = 4096,
	.i4FullRawH = 3072,
	.i4ModeIndex = 0,
	.i4VCPackNum = 1,
	.PDAF_Support = PDAF_SUPPORT_CAMSV,
	/* VC's PD pattern description */
	.sPDMapInfo[0] = {
		.i4VCFeature = VC_PDAF_STATS_NE_PIX_1,
		.i4PDPattern = 3,
		.i4PDRepetition = 8,
		.i4PDOrder = {1, 1, 0, 0, 0, 0, 1, 1}, /*R = 1, L = 0*/
	},
};


static struct mtk_mbus_frame_desc_entry frame_desc_prev[] = {
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
			.hsize = 4096,
			.vsize = 768,
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
			.hsize = 508,
			.vsize = 576,
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
			.hsize = 508,
			.vsize = 752,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
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

static struct mtk_mbus_frame_desc_entry frame_desc_cus2[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4912,
			.vsize = 3684,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 2456,
			.vsize = 1842,
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
			.hsize = 8192,
			.vsize = 6144,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus4[] = {
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
			.hsize = 4096,
			.vsize = 768,
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
			.hsize = 2048,
			.vsize = 1152,
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

static struct subdrv_mode_struct mode_struct[] = {
	{/*B6-S1 4096x3072 @30.1FPS QBIN(VBIN) VB Max. seamless F1-S1&F2-RAW-S1&F3-RAW-S1*/
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = konkautele_preview_setting,
		.mode_setting_len = ARRAY_SIZE(konkautele_preview_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = konkautele_seamless_preview,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(konkautele_seamless_preview),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 876800000,
		.linelength = 7500,
		.framelength = 3870,
		.max_framerate = 301,
		.mipi_pixel_rate = 1252800000,
		.read_margin = 10,
		.framelength_step = 4,
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
		.fine_integ_line = 388,
		.csi_param = {},
		.dpc_enabled = true,
	},
	{/*B6-S1 4096x3072 @30.1FPS QBIN(VBIN) VB Max. seamless F1-S1&F2-RAW-S1&F3-RAW-S1*/
		.frame_desc = frame_desc_cap,
		.num_entries = ARRAY_SIZE(frame_desc_cap),
		.mode_setting_table = konkautele_capture_setting,
		.mode_setting_len = ARRAY_SIZE(konkautele_capture_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 876800000,
		.linelength = 7500,
		.framelength = 3870,
		.max_framerate = 301,
		.mipi_pixel_rate = 1252800000,
		.read_margin = 10,
		.framelength_step = 4,
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
		.fine_integ_line = 388,
		.csi_param = {
			.cphy_settle = 59,
		},
		.dpc_enabled = true,
	},
	{/*B7-S2 4096x2304 @30.1FPS QBIN(VBIN) w/ All-PD VB Max. seamless F4-S2*/
		.frame_desc = frame_desc_vid,
		.num_entries = ARRAY_SIZE(frame_desc_vid),
		.mode_setting_table = konkautele_normal_video_setting,
		.mode_setting_len = ARRAY_SIZE(konkautele_normal_video_setting),
		.seamless_switch_group = 2,
		.seamless_switch_mode_setting_table = konkautele_seamless_normal_video,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(konkautele_seamless_normal_video),
		.hdr_mode = HDR_NONE,
		.pclk = 876800000,
		.linelength = 7500,
		.framelength = 3870,
		.max_framerate = 301,
		.mipi_pixel_rate = 1017600000,
		.read_margin = 10,
		.framelength_step = 4,
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
		.pdaf_cap = true,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 388,
		.csi_param = {
			.cphy_settle = 56,
		},
	},
	{/* B4 4096x2304 @60FPS QBIN w/ Partial-PD VB_max */
		.frame_desc = frame_desc_hs,
		.num_entries = ARRAY_SIZE(frame_desc_hs),
		.mode_setting_table = konkautele_hs_video_setting,
		.mode_setting_len = ARRAY_SIZE(konkautele_hs_video_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 876800000,
		.linelength = 4616,
		.framelength = 3149,
		.max_framerate = 600,
		.mipi_pixel_rate = 1068340000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 4,
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
		.pdaf_cap = true,
		.imgsensor_pd_info = &imgsensor_partial_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 12,
		.csi_param = {
			.cphy_settle = 57,
		},
	},
	{   /*B3 4096x3072 @60FPS QBIN(VBIN) w/ Partial-PD VB Max.*/
		.frame_desc = frame_desc_slim,
		.num_entries = ARRAY_SIZE(frame_desc_slim),
		.mode_setting_table = konkautele_slim_video_setting,
		.mode_setting_len = ARRAY_SIZE(konkautele_slim_video_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 876800000,
		.linelength = 4616,
		.framelength = 3149,
		.max_framerate = 600,
		.mipi_pixel_rate = 1068340000,
		.read_margin = 10,
		.framelength_step = 4,
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
		.pdaf_cap = true,
		.imgsensor_pd_info = &imgsensor_partial_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 12,
		.csi_param = {},
	},
	{/* Reg_F5-RAW-S1 4096x3072 @30FPS Full RAW Crop w/ All-PD F1-S1&B1-S1&F3-RAW-S1*/
		.frame_desc = frame_desc_cus1,
		.num_entries = ARRAY_SIZE(frame_desc_cus1),
		.mode_setting_table = konkautele_custom1_setting,
		.mode_setting_len = ARRAY_SIZE(konkautele_custom1_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = konkautele_seamless_custom1,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(konkautele_seamless_custom1),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 876800000,
		.linelength = 8960,
		.framelength = 3241,
		.max_framerate = 301,
		.mipi_pixel_rate = 1252800000,
		.read_margin = 10,
		.framelength_step = 4,
		.coarse_integ_step = 1,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 12,
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
		.imgsensor_pd_info = &imgsensor_pd_info_full,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 543,
		.csi_param = {},
		.dpc_enabled = true,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 8,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_Gr,
		.awb_enabled = true,
	},
	{/* Reg_F3-RAW-S1 4912x3684 @24FPS Full RAW Crop w/ All-PD F1-S1&B1-S1&F2-RAW-S1 */
		.frame_desc = frame_desc_cus2,
		.num_entries = ARRAY_SIZE(frame_desc_cus2),
		.mode_setting_table = konkautele_custom2_setting,
		.mode_setting_len = ARRAY_SIZE(konkautele_custom2_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = konkautele_seamless_custom2,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(konkautele_seamless_custom2),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 876800000,
		.linelength = 8960,
		.framelength = 4065,
		.max_framerate = 240,
		.mipi_pixel_rate = 1252800000,
		.read_margin = 10,
		.framelength_step = 4,
		.coarse_integ_step = 1,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 12,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 1640,
			.y0_offset = 1230,
			.w0_size = 4912,
			.h0_size = 3684,
			.scale_w = 4912,
			.scale_h = 3684,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4912,
			.h1_size = 3684,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4912,
			.h2_tg_size = 3684,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_full,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 543,
		.csi_param = {
			.cphy_settle = 57,
		},
		.dpc_enabled = true,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 8,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_Gr,
		.awb_enabled = true,
	},
	{/* F1-S1 8192x6144 @15FPS Full RMSC bayer w/ All-PD seamless B1-S1&F2-RAW-S1&F3-RAW-S1 */
		.frame_desc = frame_desc_cus3,
		.num_entries = ARRAY_SIZE(frame_desc_cus3),
		.mode_setting_table = konkautele_custom3_setting,
		.mode_setting_len = ARRAY_SIZE(konkautele_custom3_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = konkautele_seamless_custom3,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(konkautele_seamless_custom3),
		.hdr_mode = HDR_NONE,
		.pclk = 876800000,
		.linelength = 8960,
		.framelength = 6504,
		.max_framerate = 150,
		.mipi_pixel_rate = 1252800000,
		.read_margin = 10,
		.framelength_step = 4,
		.coarse_integ_step = 1,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 12,
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
		.ae_binning_ratio = 1000,
		.fine_integ_line = 543,
		.csi_param = {0},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 8,
	},
	{/* F1-S1 8192x6144 @15FPS Full RMSC  Qbayer w/ All-PD seamless B1-S1&F2-RAW-S1&F3-RAW-S1 */
		.frame_desc = frame_desc_cus4,
		.num_entries = ARRAY_SIZE(frame_desc_cus4),
		.mode_setting_table = konkautele_custom4_setting,
		.mode_setting_len = ARRAY_SIZE(konkautele_custom4_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = konkautele_seamless_custom4,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(konkautele_seamless_custom4),
		.hdr_mode = HDR_NONE,
		.pclk = 876800000,
		.linelength = 8960,
		.framelength = 6504,
		.max_framerate = 150,
		.mipi_pixel_rate = 1252800000,
		.read_margin = 10,
		.framelength_step = 4,
		.coarse_integ_step = 1,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 12,
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
		.ae_binning_ratio = 1000,
		.fine_integ_line = 543,
		.csi_param = {0},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 8,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_Gr,
	},
	{/*B6-S1 4096x3072 @30.1FPS QBIN(VBIN) VB Max. seamless F1-S1&F2-RAW-S1&F3-RAW-S1*/
		.frame_desc = frame_desc_cus5,
		.num_entries = ARRAY_SIZE(frame_desc_cus5),
		.mode_setting_table = konkautele_custom5_setting,
		.mode_setting_len = ARRAY_SIZE(konkautele_custom5_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 876800000,
		.linelength = 7500,
		.framelength = 3870,
		.max_framerate = 301,
		.mipi_pixel_rate = 1252800000,
		.read_margin = 10,
		.framelength_step = 4,
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
		.fine_integ_line = 388,
		.csi_param = {},
		.dpc_enabled = true,
	},
	{/*F6-S2 4096x2304 @30.1FPS Full RMSC Crop w/ All-PD seamless B2-S2*/
		.frame_desc = frame_desc_cus6,
		.num_entries = ARRAY_SIZE(frame_desc_cus6),
		.mode_setting_table = konkautele_custom6_setting,
		.mode_setting_len = ARRAY_SIZE(konkautele_custom6_setting),
		.seamless_switch_group = 2,
		.seamless_switch_mode_setting_table = konkautele_seamless_custom6,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(konkautele_seamless_custom6),
		.hdr_mode = HDR_NONE,
		.pclk = 876800000,
		.linelength = 8960,
		.framelength = 3241,
		.max_framerate = 301,
		.mipi_pixel_rate = 1017600000,
		.read_margin = 10,
		.framelength_step = 4,
		.coarse_integ_step = 1,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 12,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 2048,
			.y0_offset = 1920,
			.w0_size = 4096,
			.h0_size = 2304,
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
		.pdaf_cap = true,
		.imgsensor_pd_info = &imgsensor_pd_info_full,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 543,
		.csi_param = {
			.cphy_settle = 59,
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 8,
		.awb_enabled = true,
	},
	{/* Reg_F5-RAW-S1 4096x3072 @30FPS Full RAW Crop bayer(0x3205, 0x01) w/ All-PD F1-S1&B1-S1&F3-RAW-S1*/
		.frame_desc = frame_desc_cus7,
		.num_entries = ARRAY_SIZE(frame_desc_cus7),
		.mode_setting_table = konkautele_custom7_setting,
		.mode_setting_len = ARRAY_SIZE(konkautele_custom7_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 876800000,
		.linelength = 8960,
		.framelength = 3241,
		.max_framerate = 301,
		.mipi_pixel_rate = 1252800000,
		.read_margin = 10,
		.framelength_step = 4,
		.coarse_integ_step = 1,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 12,
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
		.imgsensor_pd_info = &imgsensor_pd_info_full,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 543,
		.csi_param = {},
		.dpc_enabled = true,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 8,
		.awb_enabled = true,
	},
};

static struct subdrv_static_ctx static_ctx = {
	.sensor_id = KONKAUTELE_SENSOR_ID,
	.reg_addr_sensor_id = {0x0016, 0x0017},
	.i2c_addr_table = {0x20, 0xFF},
	.i2c_burst_write_support = TRUE,
	.i2c_transfer_data_type = I2C_DT_ADDR_16_DATA_8,
	.eeprom_info = eeprom_info,
	.eeprom_num = ARRAY_SIZE(eeprom_info),
	.resolution = {8192, 6144},
	.mirror = IMAGE_H_MIRROR,

	.mclk = 24,
	.isp_driving_current = ISP_DRIVING_4MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_CPHY,
	.mipi_lane_num = SENSOR_MIPI_3_LANE,
	.ob_pedestal = 0x40,

	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_Gr,
	.ana_gain_def = BASEGAIN * 4, // hardcode
	.ana_gain_min = BASEGAIN * 1.43,
	.ana_gain_max = BASEGAIN * 32,
	.ana_gain_type = 0,
	.ana_gain_step = 1,
	.ana_gain_table = konkautele_ana_gain_table,
	.ana_gain_table_size = sizeof(konkautele_ana_gain_table),
	.tuning_iso_base = 100,
	.exposure_def = 0x3D0,
	.exposure_min = 4, // Constraints of COARSE_INTEG_TIME
	.exposure_max = 128 * (0xFFFC - 48), // Constraints of COARSE_INTEG_TIME
	.exposure_step = 1, // Constraints of COARSE_INTEG_TIME
	.exposure_margin = 48, // Constraints of COARSE_INTEG_TIME

	.frame_length_max = 0xFFFC,
	.ae_effective_frame = 2,
	.frame_time_delay_frame = 3,
	.start_exposure_offset = 500000, // tuning for sensor fusion

	.pdaf_type = PDAF_SUPPORT_CAMSV_QPD,
	.hdr_type = HDR_SUPPORT_STAGGER_FDOL,
	.seamless_switch_support = TRUE,
	.seamless_switch_type = SEAMLESS_SWITCH_CUT_VB_INIT_SHUT,
	.seamless_switch_hw_re_init_time_ns = 0,
	.seamless_switch_prsh_hw_fixed_value = 48,
	.seamless_switch_prsh_length_lc = 0,
	.reg_addr_prsh_length_lines = {0x3058, 0x3059, 0x305A, 0x305B},
	.reg_addr_prsh_mode = 0x3056,

	.temperature_support = TRUE,

	.g_temp = get_sensor_temperature,
	.g_gain2reg = get_gain2reg,
	.g_cali = get_sensor_cali,
	.s_gph = set_group_hold,
	.s_cali = set_sensor_cali,

	.reg_addr_stream = 0x0100,
	.reg_addr_mirror_flip = 0x0101,
	.reg_addr_exposure = {
			{0x0202, 0x0203},
			{0x0224, 0x0225},
	},
	.long_exposure_support = TRUE,
	.reg_addr_exposure_lshift = 0x3160,
	.reg_addr_ana_gain = {
			{0x0204, 0x0205},
			{0x0216, 0x0217},
	},
	.reg_addr_frame_length = {0x0340, 0x0341},
	.reg_addr_temp_en = 0x0138,
	.reg_addr_temp_read = 0x013A,
	.reg_addr_auto_extend = 0x0350,
	.reg_addr_frame_count = 0x0005,
	.reg_addr_fast_mode = 0x3010,

	.init_setting_table = konkautele_init_setting,
	.init_setting_len = ARRAY_SIZE(konkautele_init_setting),
	.mode = mode_struct,
	.sensor_mode_num = ARRAY_SIZE(mode_struct),
	.list = feature_control_list,
	.list_len = ARRAY_SIZE(feature_control_list),

	.chk_s_off_sta = 1,
	.chk_s_off_end = 0,
	.checksum_value = 0xf10e5980,

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
	.vsync_notify = vsync_notify,
	.update_sof_cnt = common_update_sof_cnt,
};

static struct subdrv_pw_seq_entry pw_seq[] = {
	// {HW_ID_MCLK, 24, 0},
	// {HW_ID_RST, 0, 1},
	// {HW_ID_AVDD, 2804000, 3},
	// {HW_ID_AVDD1, 2804000, 1},
	// {HW_ID_DVDD, 1104000, 4},
	// {HW_ID_DOVDD, 1804000, 3},
	// {HW_ID_AFVDD, 2804000, 3},
	// {HW_ID_MCLK_DRIVING_CURRENT, 4, 6},
	// {HW_ID_RST, 1, 2},
	{HW_ID_MCLK, {24}, 0},
	{HW_ID_RST, {0}, 0},
	{HW_ID_MCLK_DRIVING_CURRENT, {8}, 1000},
	{HW_ID_DOVDD, {1800000, 1800000}, 1000},
	{HW_ID_DVDD, {1100000, 1100000}, 0},
	{HW_ID_AVDD, {2800000, 2800000}, 0},
	{HW_ID_OISVDD, {3100000, 3100000}, 2000},
	{HW_ID_AFVDD, {3100000, 3100000}, 3000},
	{HW_ID_RST, {1}, 5000}
};

const struct subdrv_entry konkautele_mipi_raw_entry = {
	.name = "konkautele_mipi_raw",
	.id = KONKAUTELE_SENSOR_ID,
	.pw_seq = pw_seq,
	.pw_seq_cnt = ARRAY_SIZE(pw_seq),
	.ops = &ops,
};

static unsigned int read_konkautele_eeprom_info(struct subdrv_ctx *ctx, kal_uint16 meta_id,
	BYTE *data, int size)
{
	kal_uint16 addr;
	int readsize;
	if (meta_id != konkautele_eeprom_info[meta_id].meta)
		return -1;

	if (size != konkautele_eeprom_info[meta_id].size)
		return -1;

	addr = konkautele_eeprom_info[meta_id].start;
	readsize = konkautele_eeprom_info[meta_id].size;
	konkautele_read_eeprom_protect(ctx);
	if(!read_cmos_eeprom_p8(ctx, addr, data, readsize)) {
		DRV_LOGE(ctx, "read meta_id(%d) failed", meta_id);
	}

	return 0;
}

 static struct eeprom_addr_table_struct  oplus_eeprom_addr_table =
{
	.i2c_read_id = 0x74,
	.i2c_write_id = 0x74,

	.addr_modinfo = 0x0000,
	.addr_lens = 0x0008,
	.addr_vcm = 0x000A,
	.addr_modinfoflag = 0x0010,

	.addr_af = 0x0092,
	.addr_afmacro = 0x0092,
	.addr_afinf = 0x0094,
	.addr_afflag = 0x009C,

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

	adaptor_i2c_rd_u8(ctx->i2c_client, KONKAUTELE_EEPROM_READ_ID >> 1, addr, (u8 *)&get_byte);
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
    ret = adaptor_i2c_wr_p8(ctx->i2c_client, KONKAUTELE_EEPROM_WRITE_ID >> 1,
            addr, para, len);

	return ret;
}

static kal_int32 write_eeprom_protect(struct subdrv_ctx *ctx, kal_uint16 enable)
{
    kal_int32 ret = ERROR_NONE;
    kal_uint16 reg = 0x97E6;
    if (enable) {
        adaptor_i2c_wr_u8(ctx->i2c_client, KONKAUTELE_EEPROM_READ_ID >> 1, reg, 0x00);
    }
    else {
        adaptor_i2c_wr_u8(ctx->i2c_client, KONKAUTELE_EEPROM_READ_ID >> 1, reg, 0xEA);
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
        if ((pStereodata->uSensorId == KONKAUTELE_SENSOR_ID) && (data_length - 2 == CALI_DATA_SLAVE_LENGTH || data_length - 2 == CALI_DATA_SLAVE_TELE_LENGTH)
            && (data_base == KONKAUTELE_STEREO_START_ADDR )) {
            LOG_INF("Write: %x %x %x %x\n", pData[0], pData[39], pData[40], pData[1556]);
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
                write_eeprom_protect(ctx, 1);
                msleep(6);
                return -1;
            }
            msleep(6);
            write_eeprom_protect(ctx, 1);
            msleep(6);
            LOG_INF("com_0:0x%x\n", read_cmos_eeprom_8(ctx, data_base));
            LOG_INF("com_39:0x%x\n", read_cmos_eeprom_8(ctx, data_base+39));
            LOG_INF("innal_40:0x%x\n", read_cmos_eeprom_8(ctx, data_base+40));
            LOG_INF("innal_1556:0x%x\n", read_cmos_eeprom_8(ctx, data_base+1556));
            LOG_INF("write_Module_data Write end\n");
        } else if ((pStereodata->uSensorId == KONKAUTELE_SENSOR_ID) && (data_length < AESYNC_DATA_LENGTH_TOTAL)
            && (data_base == KONKAUTELE_AESYNC_START_ADDR)) {
            LOG_INF("write main aesync: %x %x %x %x %x %x %x %x\n", pData[0], pData[1],
                pData[2], pData[3], pData[4], pData[5], pData[6], pData[7]);
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
                write_eeprom_protect(ctx, 1);
                msleep(6);
                return -1;
            }
            msleep(6);
            write_eeprom_protect(ctx, 1);
            msleep(6);
            LOG_INF("readback main aesync: %x %x %x %x %x %x %x %x\n",
                read_cmos_eeprom_8(ctx, KONKAUTELE_AESYNC_START_ADDR),
                read_cmos_eeprom_8(ctx, KONKAUTELE_AESYNC_START_ADDR+1),
                read_cmos_eeprom_8(ctx, KONKAUTELE_AESYNC_START_ADDR+2),
                read_cmos_eeprom_8(ctx, KONKAUTELE_AESYNC_START_ADDR+3),
                read_cmos_eeprom_8(ctx, KONKAUTELE_AESYNC_START_ADDR+4),
                read_cmos_eeprom_8(ctx, KONKAUTELE_AESYNC_START_ADDR+5),
                read_cmos_eeprom_8(ctx, KONKAUTELE_AESYNC_START_ADDR+6),
                read_cmos_eeprom_8(ctx, KONKAUTELE_AESYNC_START_ADDR+7));
            LOG_INF("AESync write_Module_data Write end\n");
        } else {
            LOG_INF("Invalid Sensor id:0x%x write eeprom\n", pStereodata->uSensorId);
            return -1;
        }
    } else {
        LOG_INF("konkautele write_Module_data pStereodata is null\n");
        return -1;
    }
    return ret;
}

static int konkautele_set_eeprom_calibration(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
    int ret = ERROR_NONE;
    ret = write_Module_data(ctx, (ACDK_SENSOR_ENGMODE_STEREO_STRUCT *)(para));
    if (ret != ERROR_NONE) {
        *len = (u32)-1; /*write eeprom failed*/
        LOG_INF("ret=%d\n", ret);
    }
	return 0;
}

static int konkautele_get_eeprom_calibration(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
    if(*len > CALI_DATA_MASTER_LENGTH)
        *len = CALI_DATA_MASTER_LENGTH;
    read_konkautele_eeprom_info(ctx, EEPROM_META_STEREO_DATA,
            (BYTE *)para, *len);
	return 0;
}

static bool read_cmos_eeprom_p8(struct subdrv_ctx *ctx, kal_uint16 addr,
                    BYTE *data, int size)
{
	if (adaptor_i2c_rd_p8(ctx->i2c_client, KONKAUTELE_EEPROM_READ_ID >> 1,
			addr, data, size) < 0) {
		return false;
	}
	return true;
}

static void konkautele_read_eeprom_protect(struct subdrv_ctx *ctx) {
	uint16_t chip_en;
	int ret = 0;
	unsigned short stdby[17] = {0x0100, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
								0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
								0x0000, 0x0000, 0x0000};

	mutex_lock(&dw9786_mutex);
	DRV_LOGE(ctx, "dw9786_mutex: %p\n", &dw9786_mutex);
	ret = adaptor_i2c_rd_u16(ctx->i2c_client, KONKAUTELE_AF_SLAVE_ID>> 1, DW9786_CHIP_EN, &chip_en);
	DRV_LOGE(ctx, "DW9786_CHIP_EN: 0x%x, ret: %d\n", chip_en, ret);
	if(chip_en != 0x0001) {
		adaptor_i2c_wr_u16(ctx->i2c_client, KONKAUTELE_AF_SLAVE_ID>> 1, DW9786_CHIP_EN, 0x0000);
		mdelay(2);
		adaptor_i2c_wr_p8(ctx->i2c_client, KONKAUTELE_AF_SLAVE_ID>> 1, DW9786_CHIP_EN, (unsigned char *)stdby, 34);
		mdelay(5);
		adaptor_i2c_wr_u16(ctx->i2c_client, KONKAUTELE_AF_SLAVE_ID>> 1, 0xE004, 0x0001);
		mdelay(20);
	}
	mutex_unlock(&dw9786_mutex);
}

static void read_otp_info(struct subdrv_ctx *ctx)
{
	konkautele_read_eeprom_protect(ctx);
	read_cmos_eeprom_p8(ctx, 0, otp_data_checksum, OTP_SIZE);
	DRV_LOGE(ctx, "Module information flag:0x%x\n", read_cmos_eeprom_8(ctx, 0x0010));
}

static int konkautele_get_otp_checksum_data(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u32 *feature_return_para_32 = (u32 *)para;
	DRV_LOGE(ctx, "get otp data");
	if (otp_data_checksum[10] != 1) {
		read_otp_info(ctx);
	} else {
		DRV_LOG(ctx, "otp data has already read");
	}
	memcpy(feature_return_para_32, (UINT32 *)otp_data_checksum, sizeof(otp_data_checksum));
	*len = sizeof(otp_data_checksum);

	DRV_LOGE(ctx, "cccccc Module information flag: %x", otp_data_checksum[16]);
	return 0;
}

static int konkautele_check_sensor_id(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	get_imgsensor_id(ctx, (u32 *)para);
	return 0;
}

static int get_imgsensor_id(struct subdrv_ctx *ctx, u32 *sensor_id)
{
	u8 i = 0;
	u8 retry = GET_SENSOR_ID_RETRY_CNT;
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
			LOG_INF("i2c_write_id(0x%x) sensor_id(0x%x/0x%x)\n",
				ctx->i2c_write_id, *sensor_id, ctx->s_ctx.sensor_id);
			if (*sensor_id == 0x0858) {
				*sensor_id = ctx->s_ctx.sensor_id;
				if (first_read) {
					konkautele_read_eeprom_protect(ctx);
					LOG_INF("OTP 0x92:0x%x\n", read_cmos_eeprom_8(ctx, 0x0092));
					read_eeprom_common_data(ctx, &oplus_eeprom_info, oplus_eeprom_addr_table);
					first_read = KAL_FALSE;
				}
				return ERROR_NONE;
			}
			DRV_LOGE(ctx, "Read sensor id fail. i2c_write_id: 0x%x\n", ctx->i2c_write_id);
			DRV_LOGE(ctx, "sensor_id = 0x%x, ctx->s_ctx.sensor_id = 0x%x\n",
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

static u16 konkautele_feedback_awbgain[] = {
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

	DRV_LOG(ctx, "feedback_awbgain r_gain: %d, b_gain: %d\n", r_gain, b_gain);
	r_gain_int = r_gain / 512;
	b_gain_int = b_gain / 512;
	konkautele_feedback_awbgain[5] = r_gain_int;
	konkautele_feedback_awbgain[7] = (r_gain - r_gain_int * 512) / 2;
	konkautele_feedback_awbgain[9] = b_gain_int;
	konkautele_feedback_awbgain[11] = (b_gain - b_gain_int * 512) / 2;
	subdrv_i2c_wr_regs_u8(ctx, konkautele_feedback_awbgain,
		ARRAY_SIZE(konkautele_feedback_awbgain));

}

static int konkautele_set_awb_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len) {
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

	/* SPC data */
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

	if (temperature < 0x55)
		temperature_convert = temperature;
	else if (temperature < 0x80)
		temperature_convert = 85;
	else if (temperature < 0xED)
		temperature_convert = -20;
	else
		temperature_convert = (char)temperature;

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
	return (1024 - (1024 * BASEGAIN) / gain);
}

void konkautele_get_min_shutter_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id,
		u64 *min_shutter, u64 *exposure_step)
{
	u32 exp_cnt = 0;
	exp_cnt = ctx->s_ctx.mode[scenario_id].exp_cnt;
	check_current_scenario_id_bound(ctx);
	DRV_LOG(ctx, "sensor_mode_num[%d]", ctx->s_ctx.sensor_mode_num);
	if (scenario_id < ctx->s_ctx.sensor_mode_num) {
		switch (ctx->s_ctx.mode[scenario_id].hdr_mode) {
			case HDR_RAW_STAGGER:
				*exposure_step = ctx->s_ctx.exposure_step * exp_cnt;
				*min_shutter = ctx->s_ctx.exposure_min * exp_cnt;
				break;
			case HDR_NONE:
				if (ctx->s_ctx.mode[scenario_id].coarse_integ_step &&
					ctx->s_ctx.mode[scenario_id].multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min) {
					*exposure_step = ctx->s_ctx.mode[scenario_id].coarse_integ_step;
					*min_shutter = ctx->s_ctx.mode[scenario_id].multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min;
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

int konkautele_get_min_shutter_by_scenario_adapter(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 *feature_data = (u64 *) para;
	konkautele_get_min_shutter_by_scenario(ctx,
		(enum SENSOR_SCENARIO_ID_ENUM)*(feature_data),
		feature_data + 1, feature_data + 2);
	return 0;
}

static int konkautele_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	enum SENSOR_SCENARIO_ID_ENUM scenario_id;
	struct mtk_hdr_ae *ae_ctrl = NULL;
	u64 *feature_data = (u64 *)para;
	enum SENSOR_SCENARIO_ID_ENUM pre_seamless_scenario_id;
	u32 frame_length_in_lut[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
	u32 exp_cnt = 0;

	if (feature_data == NULL) {
		DRV_LOGE(ctx, "input scenario is null!");
		return ERROR_INVALID_SCENARIO_ID;
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
		return ERROR_INVALID_SCENARIO_ID;
	}
	if (ctx->s_ctx.mode[scenario_id].seamless_switch_group == 0 ||
		ctx->s_ctx.mode[scenario_id].seamless_switch_group !=
			ctx->s_ctx.mode[ctx->current_scenario_id].seamless_switch_group) {
		DRV_LOGE(ctx, "seamless_switch not supported\n");
		return ERROR_INVALID_SCENARIO_ID;
	}
	if (ctx->s_ctx.mode[scenario_id].seamless_switch_mode_setting_table == NULL) {
		DRV_LOGE(ctx, "Please implement seamless_switch setting\n");
		return ERROR_INVALID_SCENARIO_ID;
	}

	exp_cnt = ctx->s_ctx.mode[scenario_id].exp_cnt;
	ctx->is_seamless = TRUE;
	pre_seamless_scenario_id = ctx->current_scenario_id;
	update_mode_info(ctx, scenario_id);

	subdrv_i2c_wr_u8(ctx, 0x0104, 0x01);
	subdrv_i2c_wr_u8(ctx, ctx->s_ctx.reg_addr_fast_mode, 0x02);

	update_mode_info(ctx, scenario_id);
	i2c_table_write(ctx,
		ctx->s_ctx.mode[scenario_id].seamless_switch_mode_setting_table,
		ctx->s_ctx.mode[scenario_id].seamless_switch_mode_setting_len);

	DRV_LOG(ctx, "write seamless switch setting done\n");
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
		default:
			set_shutter(ctx, ae_ctrl->exposure.le_exposure);
			set_gain(ctx, ae_ctrl->gain.le_gain);
			break;
		}
	}

	calculate_prsh_length_lines(ctx, ae_ctrl, pre_seamless_scenario_id);

	if (ctx->s_ctx.seamless_switch_prsh_length_lc > 0) {
		subdrv_i2c_wr_u8(ctx, ctx->s_ctx.reg_addr_prsh_mode, 0x01);

		subdrv_i2c_wr_u8(ctx,
				ctx->s_ctx.reg_addr_prsh_length_lines.addr[0],
				(ctx->s_ctx.seamless_switch_prsh_length_lc >> 24) & 0x07);
		subdrv_i2c_wr_u8(ctx,
				ctx->s_ctx.reg_addr_prsh_length_lines.addr[1],
				(ctx->s_ctx.seamless_switch_prsh_length_lc >> 16) & 0xFF);
		subdrv_i2c_wr_u8(ctx,
				ctx->s_ctx.reg_addr_prsh_length_lines.addr[2],
				(ctx->s_ctx.seamless_switch_prsh_length_lc >> 8) & 0xFF);
		subdrv_i2c_wr_u8(ctx,
				ctx->s_ctx.reg_addr_prsh_length_lines.addr[3],
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

static int konkautele_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u32 mode = *((u32 *)para);

	DRV_LOG(ctx, "mode(%u->%u)\n", ctx->test_pattern, mode);
	/* 1:Solid Color 2:Color Bar 5:Black */
	switch (mode) {
	case 5:
		subdrv_i2c_wr_u8(ctx, 0x020E, 0x00); /* dig_gain = 0 */
		break;
	default:
		subdrv_i2c_wr_u8(ctx, 0x0601, mode);
		break;
	}

	if ((ctx->test_pattern) && (mode != ctx->test_pattern)) {
		if (ctx->test_pattern == 5)
			subdrv_i2c_wr_u8(ctx, 0x020E, 0x01);
		else if (mode == 0)
			subdrv_i2c_wr_u8(ctx, 0x0601, 0x00); /* No pattern */
	}

	ctx->test_pattern = mode;
	return ERROR_NONE;
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
		set_i2c_buffer(ctx, 0x3010, 0x00);
		set_i2c_buffer(ctx, 0x3036, 0x00);
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
		/* Check QSC validation */
		if (info[idx].preload_qsc_table == NULL) {
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
		if (info[idx].preload_pdc_table == NULL) {
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

static void comp_mode_tran_time_cal1(struct subdrv_ctx *ctx, u32 scenario_id, u32* prsh) {
	#define SYSTEM_USED_LINES1 (96UL)
	#define SYSTEM_DELAY1      (189UL)
	u64 frame_duration = 0;
	u64 data_delay = 0;
	u64 system_delay = 0;
	u64 current_tline = 0;
	u64 tline = 0;

	if (!prsh) {
		DRV_LOGE(ctx, "prsh param is NULL");
		return;
	}

	*prsh = 0U;
	if (konkautele_comp_params[ctx->current_scenario_id].clock_vtpxck == 0) {
		DRV_LOG(ctx, "invalid params");
		return;
	}

	frame_duration = 1000000000UL / ctx->current_fps * 10;
	current_tline = 1000000000UL * ctx->s_ctx.mode[ctx->current_scenario_id].linelength /
		ctx->s_ctx.mode[ctx->current_scenario_id].pclk;
	tline = 1000000000UL * ctx->s_ctx.mode[scenario_id].linelength /
		ctx->s_ctx.mode[scenario_id].pclk;
	data_delay = (ctx->s_ctx.mode[ctx->current_scenario_id].imgsensor_winsize_info.h2_tg_size +
		SYSTEM_USED_LINES1) * current_tline;
	system_delay = SYSTEM_DELAY1 * 1000 * 1000 * 10 /
		konkautele_comp_params[ctx->current_scenario_id].clock_vtpxck;
	if (frame_duration <= data_delay + system_delay) {
		DRV_LOGE(ctx, "invalid parameter");
		return;
	}

	*prsh = (frame_duration - data_delay - system_delay) / tline;
	if (ctx->s_ctx.mode[scenario_id].hdr_mode == HDR_RAW_STAGGER) {
		*prsh = *prsh / 2;
	}

	DRV_LOG(ctx, "frame_duration(%llu), current_tline(%llu), tline(%llu), "
		"data_delay(%llu) system_delay(%llu) prsh(%u)\n", frame_duration,
		current_tline, tline, data_delay, system_delay, *prsh);
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

	if (pre_seamless_scenario_id == SENSOR_SCENARIO_ID_NORMAL_VIDEO && scenario_id == SENSOR_SCENARIO_ID_CUSTOM6) {
		prsh_length_lc = 1200;
	} else if (pre_seamless_scenario_id == SENSOR_SCENARIO_ID_CUSTOM6 && scenario_id == SENSOR_SCENARIO_ID_NORMAL_VIDEO) {
		prsh_length_lc = 1200;
	} else if (pre_seamless_scenario_id == SENSOR_SCENARIO_ID_NORMAL_PREVIEW && scenario_id == SENSOR_SCENARIO_ID_CUSTOM1) {
		prsh_length_lc = 1000;
	} else if (pre_seamless_scenario_id == SENSOR_SCENARIO_ID_CUSTOM1 && scenario_id == SENSOR_SCENARIO_ID_NORMAL_PREVIEW) {
		prsh_length_lc = 1200;
	} else if (pre_seamless_scenario_id == SENSOR_SCENARIO_ID_NORMAL_PREVIEW && scenario_id == SENSOR_SCENARIO_ID_CUSTOM2) {
		prsh_length_lc = 1000;
	} else if (pre_seamless_scenario_id == SENSOR_SCENARIO_ID_CUSTOM2 && scenario_id == SENSOR_SCENARIO_ID_NORMAL_PREVIEW) {
		prsh_length_lc = 1200;
	} else if (pre_seamless_scenario_id == SENSOR_SCENARIO_ID_NORMAL_PREVIEW && scenario_id == SENSOR_SCENARIO_ID_CUSTOM3) {
		prsh_length_lc = 1000;
	} else if (pre_seamless_scenario_id == SENSOR_SCENARIO_ID_CUSTOM3 && scenario_id == SENSOR_SCENARIO_ID_NORMAL_PREVIEW) {
		prsh_length_lc = 1200;
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
			DRV_LOG_MUST(ctx, "debug se %llu le %llu, me %llu", ae_ctrl->exposure.se_exposure, ae_ctrl->exposure.le_exposure, ae_ctrl->exposure.me_exposure);
		} else if (ctx->s_ctx.mode[scenario_id].exposure_order_in_lbmf ==
			IMGSENSOR_LBMF_EXPOSURE_LE_FIRST) {
			/* 2exp: dig_gain_lut_a = LE / dig_gain_lut_b = SE */
			/* 3exp: dig_gain_lut_a = LE / dig_gain_lut_b = ME / dig_gain_lut_c = SE */
			ae_ctrl_cit = ae_ctrl->exposure.le_exposure;
			DRV_LOG_MUST(ctx, "debug le\n");
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
	DRV_LOG_MUST(ctx, "prsh_length_lc %u ae_ctrl_cit %u\n", prsh_length_lc, ae_ctrl_cit);
	if(hdr_mode != HDR_RAW_LBMF) {
		ae_ctrl_cit = ae_ctrl_cit / 1000;
	}
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