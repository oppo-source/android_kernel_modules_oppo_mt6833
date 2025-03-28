// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
/*****************************************************************************
 *
 * Filename:
 * ---------
 *     konkamipiraw_Sensor.c
 *
 * Project:
 * --------
 *     ALPS
 *
 * Description:
 * ------------
 *     Source code of Sensor driver
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/
#include "konkafrontmipiraw_Sensor.h"


#define KONKAFRONT_EEPROM_READ_ID	0xA9
#define OTP_SIZE    0x2000  /* 8KB */
#define LRC_L_REG   0x7A98  /* sensor reg */
#define LRC_R_REG   0x7B1C
#define SEAMLEES_GRP_HOLD 0x0104

#define DEBUG_LOG_EN 0
#define PFX "konkafront_camera_sensor"
#define LOG_INF(format, args...) pr_info(PFX "I [%s] " format, __func__, ##args)
#define LOG_ERR(format, args...) pr_err(PFX "E [%s] " format, __func__, ##args)
#define LOG_DEBUG(...) do { if ((DEBUG_LOG_EN)) LOG_INF(__VA_ARGS__); } while (0)
#define GET_SENSOR_ID_RETRY_CNT    5

static const char * const clk_names[] = {
	ADAPTOR_CLK_NAMES
};

static const char * const reg_names[] = {
	ADAPTOR_REGULATOR_NAMES
};

static const char * const state_names[] = {
	ADAPTOR_STATE_NAMES
};

static int stream_refcnt_for_aov = 0;

static int init_ctx(
	struct subdrv_ctx *ctx, struct i2c_client *i2c_client, u8 i2c_write_id);
static int konkafront_open(struct subdrv_ctx *ctx);
static int konkafront_get_imgsensor_id(struct subdrv_ctx *ctx, u32 *sensor_id);
static int vsync_notify(struct subdrv_ctx *ctx,	unsigned int sof_cnt);
// static int get_csi_param(
// 	struct subdrv_ctx *ctx,
// 	enum SENSOR_SCENARIO_ID_ENUM scenario_id,
// 	struct mtk_csi_param *csi_param);
static int get_sensor_temperature(void *arg);
static u16 get_gain2reg(u32 gain);
static void set_group_hold(void *arg, u8 en);

#ifdef KONKAFRONT_AOV_MCLK_26M
static int set_pwr_seq_reset_view_to_sensing(void *arg);
#endif /* KONKAFRONT_AOV_MCLK_26M */

static int konkafront_streaming_control(void *arg, bool enable);
static int konkafront_check_sensor_id(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int konkafront_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int konkafront_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static void Imx615_fab_read(struct subdrv_ctx *ctx);

static kal_uint8 otp_data_checksum[OTP_SIZE] = {0};
static int get_eeprom_common_data(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int konkafront_get_otp_checksum_data(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static void set_sensor_cali(void *arg);
static void get_sensor_cali(void* arg);
static bool read_cmos_eeprom_p8(struct subdrv_ctx *ctx, kal_uint16 addr,
                    BYTE *data, int size);
static int konkafront_set_awb_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len);

/* STRUCT */
static struct subdrv_feature_control feature_control_list[] = {
	{SENSOR_FEATURE_SET_TEST_PATTERN, konkafront_set_test_pattern},
	{SENSOR_FEATURE_SEAMLESS_SWITCH, konkafront_seamless_switch},
	{SENSOR_FEATURE_CHECK_SENSOR_ID, konkafront_check_sensor_id},
	{SENSOR_FEATURE_GET_EEPROM_COMDATA, get_eeprom_common_data},
	{SENSOR_FEATURE_GET_SENSOR_OTP_ALL, konkafront_get_otp_checksum_data},
	// {SENSOR_FEATURE_GET_MIN_SHUTTER_BY_SCENARIO, konkafront_get_min_shutter_by_scenario_adapter},
	{SENSOR_FEATURE_SET_AWB_GAIN, konkafront_set_awb_gain},
};


static struct eeprom_addr_table_struct oplus_eeprom_addr_table = {
	.i2c_read_id = 0xA9,
	.i2c_write_id = 0xA8,

	.addr_modinfo = 0x0000,
	.addr_sensorid = 0x0006,
	.addr_lens = 0x0008,
	.addr_vcm = 0x000A,
    .addr_modinfoflag = 0x000F,

	// .addr_af = 0x0092,
	// .addr_afmacro = 0x0092,
	// .addr_afinf = 0x0094,
	// .addr_afflag = 0x0098,

	.addr_qrcode = 0x00B0,
	.addr_qrcodeflag = 0x00C7,
};

static struct eeprom_info_struct eeprom_info[] = {
	{
		.header_id = 0x01510008,  /* cal_layout_table */
		.addr_header_id = 0x00000006,
		.i2c_write_id = 0xA8,

		.qsc_support = TRUE,
		.qsc_size = 0x0618,
		.addr_qsc = 0x0C90, /* QSC_EEPROM_ADDR 0x0C90~0x12A7*/
		.sensor_reg_addr_qsc = 0xC500, /*QSC_Sensor_ADDR*/

		.lrc_support = TRUE,
		.lrc_size = 0x0104,
		.addr_lrc = 0x14B0, /* LRC_EEPROM_ADDR 0x14B0~0x15B3*/
		.sensor_reg_addr_lrc = 0x7A98, /*useless, use LRC_L_REG and LRC_R_REG*/
	},
};

static struct oplus_eeprom_info_struct  oplus_eeprom_info = {0};


static struct mtk_mbus_frame_desc_entry frame_desc_prev[] = {
    {
        .bus.csi2 = {
            .channel = 0,
            .data_type = 0x2b,
            .hsize = 3264,
            .vsize = 2448,
            .user_data_desc = VC_STAGGER_NE,
            .fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
        },
    }
};

static struct mtk_mbus_frame_desc_entry frame_desc_cap[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 3264,
			.vsize = 2448,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	}
};

static struct mtk_mbus_frame_desc_entry frame_desc_vid[] = {
    {
        .bus.csi2 = {
            .channel = 0,
            .data_type = 0x2b,
            .hsize = 3264,
            .vsize = 1836,
            .user_data_desc = VC_STAGGER_NE,
            .fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
        },
    }
};

static struct mtk_mbus_frame_desc_entry frame_desc_hs[] = {
    {
        .bus.csi2 = {
            .channel = 0,
            .data_type = 0x2b,
            .hsize = 3264,
            .vsize = 1836,
            .user_data_desc = VC_STAGGER_NE,
            .fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
        },
    }
};

static struct mtk_mbus_frame_desc_entry frame_desc_slim[] = {
    {
        .bus.csi2 = {
            .channel = 0,
            .data_type = 0x2b,
            .hsize = 3264,
            .vsize = 1836,
            .user_data_desc = VC_STAGGER_NE,
            .fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
        },
    }
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus1[] = {
    {
        .bus.csi2 = {
            .channel = 0,
            .data_type = 0x2b,
            .hsize = 1280,
            .vsize = 960,
            .user_data_desc = VC_STAGGER_NE,
            .fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
        },
    }
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus2[] = {
    {
        .bus.csi2 = {
            .channel = 0,
            .data_type = 0x2b,
            .hsize = 6560,
            .vsize = 4928,
            .user_data_desc = VC_STAGGER_NE,
            .fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
        },
    }
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus3[] = {
    {
        .bus.csi2 = {
            .channel = 0,
            .data_type = 0x2b,
            .hsize = 3264,
            .vsize = 1856,
            .user_data_desc = VC_STAGGER_NE,
            .fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
        },
    }
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus4[] = {
    {
        .bus.csi2 = {
            .channel = 0,
            .data_type = 0x2b,
            .hsize = 1640,
            .vsize = 1232,
            .user_data_desc = VC_STAGGER_NE,
            .fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
        },
    }
};

// static int stream_refcnt_for_aov;

static struct subdrv_mode_struct mode_struct[] = {
	{  /* B9-S7 3264x2448,30fps,2X2Binning,w/o PD,For MIPI(Around 868.8Mpps) */
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = konkafront_preview_setting,
		.mode_setting_len = ARRAY_SIZE(konkafront_preview_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = konkafront_preview_seamless_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(konkafront_preview_seamless_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 864000000,
		.linelength = 3768,
		.framelength = 7642,
		.max_framerate = 300,
		.mipi_pixel_rate = 868800000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 1,
		.imgsensor_winsize_info = {
			.full_w = 6560,
			.full_h = 4928,
			.x0_offset = 0,
			.y0_offset = 16,
			.w0_size = 6560,
			.h0_size = 4896,
			.scale_w = 3280,
			.scale_h = 2448,
			.x1_offset = 8,
			.y1_offset = 0,
			.w1_size = 3264,
			.h1_size = 2448,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 3264,
			.h2_tg_size = 2448,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 356,
		.csi_param = {.dphy_trail = 0x40,},
	},
	{	/* B9-S7 3264x2448,30fps,2X2Binning,w/o PD,For MIPI(Around 868.8Mpps) */
		.frame_desc = frame_desc_cap,
		.num_entries = ARRAY_SIZE(frame_desc_cap),
		.mode_setting_table = konkafront_capture_setting,
		.mode_setting_len = ARRAY_SIZE(konkafront_capture_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 864000000,
		.linelength = 3768,
		.framelength = 7642,
		.max_framerate = 300,
		.mipi_pixel_rate = 868800000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 1,
		.imgsensor_winsize_info = {
			.full_w = 6560,
			.full_h = 4928,
			.x0_offset = 0,
			.y0_offset = 16,
			.w0_size = 6560,
			.h0_size = 4896,
			.scale_w = 3280,
			.scale_h = 2448,
			.x1_offset = 8,
			.y1_offset = 0,
			.w1_size = 3264,
			.h1_size = 2448,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 3264,
			.h2_tg_size = 2448,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 356,
		.csi_param = {.dphy_trail = 0x40,},
	},

	{/*Reg B14, 3264x1836,30fps,2X2Binning,w/o PD,For(rst<21ms&cycle>1.5 s)*/
		.frame_desc = frame_desc_vid,
		.num_entries = ARRAY_SIZE(frame_desc_vid),
		.mode_setting_table = konkafront_normal_video_setting,
		.mode_setting_len = ARRAY_SIZE(konkafront_normal_video_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 460800000,
		.linelength = 3768,
		.framelength = 4076,
		.max_framerate = 300,
		.mipi_pixel_rate = 433800000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 1,
		.imgsensor_winsize_info = {
			.full_w = 6560,
			.full_h = 4928,
			.x0_offset = 0,
			.y0_offset = 628,
			.w0_size = 6560,
			.h0_size = 3672,
			.scale_w = 3280,
			.scale_h = 1836,
			.x1_offset = 8,
			.y1_offset = 0,
			.w1_size = 3264,
			.h1_size = 1836,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 3264,
			.h2_tg_size = 1836,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 356,
		.csi_param = {.dphy_trail = 0x47,},
	},
	{/*Reg B7-S8, 3264x1856,60fps,2X2Binning,w/o PD,For seamless2 group*/
		.frame_desc = frame_desc_hs,
		.num_entries = ARRAY_SIZE(frame_desc_hs),
		.mode_setting_table = konkafront_hs_video_setting,
		.mode_setting_len = ARRAY_SIZE(konkafront_hs_video_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 460800000,
		.linelength = 3768,
		.framelength = 2038,
		.max_framerate = 600,
		.mipi_pixel_rate = 433800000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 1,
		.imgsensor_winsize_info = {
			.full_w = 6560,
			.full_h = 4928,
			.x0_offset = 0,
			.y0_offset = 628,
			.w0_size = 6560,
			.h0_size = 3672,
			.scale_w = 3280,
			.scale_h = 1836,
			.x1_offset = 8,
			.y1_offset = 0,
			.w1_size = 3264,
			.h1_size = 1836,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 3264,
			.h2_tg_size = 1836,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 356,
		.csi_param = {.dphy_trail = 0x47,},
	},
	{/*Reg B7-S8, 3264x1856,60fps,2X2Binning,w/o PD,For seamless2 group*/
		.frame_desc = frame_desc_slim,
		.num_entries = ARRAY_SIZE(frame_desc_slim),
		.mode_setting_table = konkafront_slim_video_setting,
		.mode_setting_len = ARRAY_SIZE(konkafront_slim_video_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 460800000,
		.linelength = 3768,
		.framelength = 2038,
		.max_framerate = 600,
		.mipi_pixel_rate = 433800000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 1,
		.imgsensor_winsize_info = {
			.full_w = 6560,
			.full_h = 4928,
			.x0_offset = 0,
			.y0_offset = 628,
			.w0_size = 6560,
			.h0_size = 3672,
			.scale_w = 3280,
			.scale_h = 1836,
			.x1_offset = 8,
			.y1_offset = 0,
			.w1_size = 3264,
			.h1_size = 1836,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 3264,
			.h2_tg_size = 1836,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 356,
		.csi_param = {.dphy_trail = 0x47,},
	},
	{/*Reg V2-4 1280x960,10fps,4x4Binning_Crop,w/o PD,For MIPI<1.5G(Skew off)&Hblank=2.4us*/
		.frame_desc = frame_desc_cus1,
		.num_entries = ARRAY_SIZE(frame_desc_cus1),
		.mode_setting_table = konkafront_custom1_setting,
		.mode_setting_len = ARRAY_SIZE(konkafront_custom1_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 481000000,
		.linelength = 2248,
		.framelength = 21396,
		.max_framerate = 100,
		.mipi_pixel_rate = 561600000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 1,
		.imgsensor_winsize_info = {
			.full_w = 6560,
			.full_h = 4928,
			.x0_offset = 0,
			.y0_offset = 544,
			.w0_size = 6560,
			.h0_size = 3840,
			.scale_w = 1640,
			.scale_h = 960,
			.x1_offset = 180,
			.y1_offset = 0,
			.w1_size = 1280,
			.h1_size = 960,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1280,
			.h2_tg_size = 960,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 879,
		.csi_param = {
			.dphy_trail = 0x60,
		},
		.aov_mode = 1,
		.s_dummy_support = 0,
		.ae_ctrl_support = IMGSENSOR_AE_CONTROL_SUPPORT_VIEWING_MODE,
	},
	{/*Reg F1-S7, 6560x4928,15fps,Full,w/o PD,For seamless1 group*/
		.frame_desc = frame_desc_cus2,
		.num_entries = ARRAY_SIZE(frame_desc_cus2),
		.mode_setting_table = konkafront_custom2_setting,
		.mode_setting_len = ARRAY_SIZE(konkafront_custom2_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = konkafront_custom2_seamless_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(konkafront_custom2_seamless_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 864000000,
		.linelength = 11480,
		.framelength = 5017,
		.max_framerate = 150,
		.mipi_pixel_rate = 868800000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 1,
		.imgsensor_winsize_info = {
			.full_w = 6560,
			.full_h = 4928,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 6560,
			.h0_size = 4928,
			.scale_w = 6560,
			.scale_h = 4928,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 6560,
			.h1_size = 4928,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 6560,
			.h2_tg_size = 4928,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 617,
		.csi_param = {.dphy_trail = 0xB3,},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 16,
	},
	{/*Reg_B8-3 3264x1856,15fps,2X2Binning,w/o PD,For MaxVB*/
		.frame_desc = frame_desc_cus3,
		.num_entries = ARRAY_SIZE(frame_desc_cus3),
		.mode_setting_table = konkafront_custom3_setting,
		.mode_setting_len = ARRAY_SIZE(konkafront_custom3_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 864000000,
		.linelength = 3768,
		.framelength = 15286,
		.max_framerate = 150,
		.mipi_pixel_rate = 868800000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 1,
		.imgsensor_winsize_info = {
			.full_w = 6560,
			.full_h = 4928,
			.x0_offset = 0,
			.y0_offset = 608,
			.w0_size = 6560,
			.h0_size = 3712,
			.scale_w = 3280,
			.scale_h = 1856,
			.x1_offset = 8,
			.y1_offset = 0,
			.w1_size = 3264,
			.h1_size = 1856,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 3264,
			.h2_tg_size = 1856,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 356,
		.csi_param = {.dphy_trail = 0xB3,},
	},
	{/*Reg V3 1640x1232,30fps,4x4Binning,w/o PD,For MaxVB&Binning-Ave*/
		.frame_desc = frame_desc_cus4,
		.num_entries = ARRAY_SIZE(frame_desc_cus4),
		.mode_setting_table = konkafront_custom4_setting,
		.mode_setting_len = ARRAY_SIZE(konkafront_custom4_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 864000000,
		.linelength = 2248,
		.framelength = 12810,
		.max_framerate = 300,
		.mipi_pixel_rate = 780000000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 1,
		.imgsensor_winsize_info = {
			.full_w = 6560,
			.full_h = 4928,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 6560,
			.h0_size = 4928,
			.scale_w = 1640,
			.scale_h = 1232,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 1640,
			.h1_size = 1232,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1640,
			.h2_tg_size = 1232,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 356,
		.csi_param = {},
	},
};

static struct subdrv_static_ctx static_ctx = {
	.sensor_id = KONKAFRONT_SENSOR_ID,
	.reg_addr_sensor_id = {0x0016, 0x0017},  // todo
	.i2c_addr_table = {0x20, 0xFF},
	.i2c_burst_write_support = TRUE,
	.i2c_transfer_data_type = I2C_DT_ADDR_16_DATA_8,
	.eeprom_info = eeprom_info,
	.eeprom_num = ARRAY_SIZE(eeprom_info),
	.resolution = {6560, 4928},
	.mirror = IMAGE_NORMAL,
	.mclk = 24,
	.isp_driving_current = ISP_DRIVING_6MA,  // todo
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.ob_pedestal = 0x40,

	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_R,
	.ana_gain_def = BASEGAIN * 4,
	.ana_gain_min = BASEGAIN * 1,  // BASEGAIN * 1.123
	.ana_gain_max = BASEGAIN * 64,
	.ana_gain_type = 0,
	.ana_gain_step = 1,
	.ana_gain_table = konkafront_ana_gain_table,
	.ana_gain_table_size = sizeof(konkafront_ana_gain_table),
	.tuning_iso_base = 100,
	.exposure_def = 0x3D0,
	.exposure_min = 16,
	.exposure_max =  128*(0xFFFF - 48),
	.exposure_step = 1,
	.exposure_margin = 48,

	.frame_length_max = 0xffff-5,
	.ae_effective_frame = 2,
	.frame_time_delay_frame = 3,
	.start_exposure_offset = 1794700,

	.pdaf_type = PDAF_SUPPORT_NA,
	.hdr_type = HDR_SUPPORT_NA,
	.seamless_switch_support = TRUE,
	.temperature_support = TRUE,

	.g_temp = get_sensor_temperature,
	.g_gain2reg = get_gain2reg,
	.s_gph = set_group_hold,

	.s_cali = set_sensor_cali,
	.g_cali = get_sensor_cali,
	// .s_data_rate_global_timing_phy_ctrl = set_data_rate_global_timing_phy_ctrl,
#ifdef KONKAFRONT_AOV_MCLK_26M
	.s_pwr_seq_reset_view_to_sensing = set_pwr_seq_reset_view_to_sensing,
#endif
	.s_streaming_control = konkafront_streaming_control,
	.reg_addr_stream = 0x0100,
	.reg_addr_mirror_flip = 0x0101,

	.reg_addr_exposure = {
			{0x0202, 0x0203},
	},
	.long_exposure_support = TRUE,
	.reg_addr_exposure_lshift = 0x3100,
	.reg_addr_ana_gain = {
			{0x0204, 0x0205},
	},
	.reg_addr_frame_length = {0x0340, 0x0341},
	.reg_addr_temp_en = 0x0138,
	.reg_addr_temp_read = 0x013A,
	.reg_addr_auto_extend = 0x0350,
	.reg_addr_frame_count = 0x0005,
	.reg_addr_fast_mode = 0x3020,

	.init_setting_table = konkafront_init_setting,
	.init_setting_len = ARRAY_SIZE(konkafront_init_setting),
	.mode = mode_struct,
	.sensor_mode_num = ARRAY_SIZE(mode_struct),
	.list = feature_control_list,
	.list_len = ARRAY_SIZE(feature_control_list),

	.chk_s_off_sta = 1,
	.chk_s_off_end = 0,
	.checksum_value = 0x8ac2d94a,
	.aov_sensor_support = TRUE,
	.sensor_mode_ops = 0,  // debug
	.sensor_debug_sensing_ut_on_scp = TRUE, // whether stream on scp
	// .sensor_debug_dphy_global_timing_continuous_clk = TRUE,
	.init_in_open = TRUE,
	.streaming_ctrl_imp = TRUE,
};

static struct subdrv_ops ops = {
	.init_ctx = init_ctx,
	.open = konkafront_open,
	.get_id = konkafront_get_imgsensor_id,
	.vsync_notify = vsync_notify,
	.get_csi_param = common_get_csi_param,
	.get_temp = common_get_temp,
	.get_info = common_get_info,
	.get_resolution = common_get_resolution,
	.control = common_control,
	.feature_control = common_feature_control,
	.close = common_close,
	.get_frame_desc = common_get_frame_desc,
	.update_sof_cnt = common_update_sof_cnt,
};

static struct subdrv_pw_seq_entry pw_seq[] = {
	{HW_ID_MCLK, {24}, 0},
	{HW_ID_SCL, {0}, 0},	/* default i2c bus scl 4 on apmcu side */
	{HW_ID_SDA, {0}, 0},	/* default i2c bus sda 4 on apmcu side */
	{HW_ID_RST, {0}, 0},
	{HW_ID_AVDD, {2900000, 2900000}, 1000},
	{HW_ID_DOVDD, {1800000, 1800000}, 1000},
	{HW_ID_DVDD, {1104000, 1104000}, 1000},
	{HW_ID_MCLK_DRIVING_CURRENT, {6}, 1000},
	{HW_ID_RST, {1}, 4000}
};

static struct subdrv_pw_seq_entry aov_pw_seq[] = {
	{HW_ID_MCLK, {26, MCLK_ULPOSC}, 0},
	{HW_ID_SCL, {0}, 0},	/* default i2c bus scl 4 on apmcu side */
	{HW_ID_SDA, {0}, 0},	/* default i2c bus sda 4 on apmcu side */
	{HW_ID_RST, {0}, 0},
	{HW_ID_AVDD, {2900000, 2900000}, 1000},
	{HW_ID_DOVDD, {1800000, 1800000}, 1000},
	{HW_ID_DVDD, {1104000, 1104000}, 1000},
	{HW_ID_MCLK_DRIVING_CURRENT, {6}, 1000},
	{HW_ID_RST, {1}, 4000}
};

const struct subdrv_entry konkafront_mipi_raw_entry = {
	.name = "konkafront_mipi_raw",
	.id = KONKAFRONT_SENSOR_ID,
	.pw_seq = pw_seq,
	.pw_seq_cnt = ARRAY_SIZE(pw_seq),
	.aov_pw_seq = aov_pw_seq,
	.aov_pw_seq_cnt = ARRAY_SIZE(aov_pw_seq),
	.ops = &ops,
};

static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id)
{
	memcpy(&(ctx->s_ctx), &static_ctx, sizeof(struct subdrv_static_ctx));
	subdrv_ctx_init(ctx);
	ctx->i2c_client = i2c_client;
	ctx->i2c_write_id = i2c_write_id;
	return 0;
}

static void Imx615_fab_read(struct subdrv_ctx *ctx)
{
    kal_uint16 reg_0xa01 = 0;
    kal_uint16 reg_0xa1f = 0;
    kal_uint16 reg_0xa20 = 0;

    subdrv_i2c_wr_u8(ctx, 0x0A02, 0x7F);
    subdrv_i2c_wr_u8(ctx, 0x0A00, 0x01);
    msleep(1);

    reg_0xa01 = subdrv_i2c_rd_u8(ctx, 0x0A01);
    reg_0xa1f = subdrv_i2c_rd_u8(ctx, 0x0A1F);
    reg_0xa20 = subdrv_i2c_rd_u8(ctx, 0x0A20);

    pr_info("Read fab2 reg table [0xa01->0x%x, 0xa1f->0x%x, 0xa20->0x%x]", reg_0xa01, reg_0xa1f, reg_0xa20);
    if (reg_0xa01 == 0x01) {
        if (reg_0xa1f == 0xB4 && reg_0xa20 == 0x01) {
            msleep(2);
            subdrv_i2c_wr_u8(ctx, 0x0A00, 0x00);
            sensor_init(ctx);
            subdrv_i2c_wr_u8(ctx, 0x574B, 0x01);
            subdrv_i2c_wr_u8(ctx, 0x5765, 0x33);
        }else if (reg_0xa20 != 0x01 || reg_0xa1f != 0xB4) {
            subdrv_i2c_wr_u8(ctx, 0x0A00, 0x00);
            sensor_init(ctx);
        }

    } else {
        pr_info("Read 0xa01 vaule is 0x%x",reg_0xa01);
        pr_info("IMX615 read reg_0xa01 failed,just write init setting");
        sensor_init(ctx);
    }
}


static int konkafront_open(struct subdrv_ctx *ctx)
{
	u32 sensor_id = 0;
	u32 scenario_id = 0;

	DRV_LOG_MUST(ctx, "for konkafront start\n");

	/* initail setting */
	// sensor_init(ctx);
	Imx615_fab_read(ctx);

	/* get sensor id */
	if (konkafront_get_imgsensor_id(ctx, &sensor_id) != ERROR_NONE)
		return ERROR_SENSOR_CONNECT_FAIL;

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

static int konkafront_get_imgsensor_id(struct subdrv_ctx *ctx, u32 *sensor_id)
{
	u8 i = 0;
	u8 retry = GET_SENSOR_ID_RETRY_CNT;
	u32 addr_h = ctx->s_ctx.reg_addr_sensor_id.addr[0];
	u32 addr_l = ctx->s_ctx.reg_addr_sensor_id.addr[1];
	u32 addr_ll = ctx->s_ctx.reg_addr_sensor_id.addr[2];
	static bool first_read = KAL_TRUE;

	LOG_INF("for konkafront id\n");

	while (ctx->s_ctx.i2c_addr_table[i] != 0xFF) {
		ctx->i2c_write_id = ctx->s_ctx.i2c_addr_table[i];
		do {
			*sensor_id = (subdrv_i2c_rd_u8(ctx, addr_h) << 8) |
				subdrv_i2c_rd_u8(ctx, addr_l);
			if (addr_ll)
				*sensor_id = ((*sensor_id) << 8) | subdrv_i2c_rd_u8(ctx, addr_ll);
			LOG_INF("i2c_write_id:0x%x sensor_id(cur/exp):0x%x/0x%x\n",
				ctx->i2c_write_id, *sensor_id, ctx->s_ctx.sensor_id);
			if (*sensor_id == 0x615) {
				*sensor_id = ctx->s_ctx.sensor_id;
				if (first_read) {
					LOG_INF("first read eeprom +");
					read_eeprom_common_data(ctx, &oplus_eeprom_info, oplus_eeprom_addr_table);
					first_read = KAL_FALSE;
					LOG_INF("first read eeprom -");
				}
				return ERROR_NONE;
			}
			LOG_INF("Read sensor id fail, id(0x%x)\n",
				ctx->i2c_write_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = GET_SENSOR_ID_RETRY_CNT;
	}
	if (*sensor_id != ctx->s_ctx.sensor_id) {
		/* if Sensor ID is not correct,
		 * Must set *sensor_id to 0xFFFFFFFF
		 */
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}

static int vsync_notify(struct subdrv_ctx *ctx,	unsigned int sof_cnt)
{
	DRV_LOG(ctx, "sof_cnt(%u) ctx->ref_sof_cnt(%u) ctx->fast_mode_on(%d)",
		sof_cnt, ctx->ref_sof_cnt, ctx->fast_mode_on);
	if (ctx->fast_mode_on && (sof_cnt > ctx->ref_sof_cnt)) {
		ctx->fast_mode_on = FALSE;
		ctx->ref_sof_cnt = 0;
		DRV_LOG(ctx, "seamless_switch disabled.");
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_fast_mode, 0x00);
		commit_i2c_buffer(ctx);
	}
	return 0;
}

static int konkafront_streaming_control(void *arg, bool enable) {
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;
	int ret = 0;
	DRV_LOG(ctx, "E!\n");

	struct adaptor_ctx *_adaptor_ctx = NULL;
	struct v4l2_subdev *sd = NULL;

	if (ctx->i2c_client)
		sd = i2c_get_clientdata(ctx->i2c_client);
	if (sd)
		_adaptor_ctx = to_ctx(sd);
	if (!_adaptor_ctx)
		return -ENODEV;

	DRV_LOG_MUST(ctx,
		"konkafront streaming_enable(0=Sw Standby,1=streaming):(%d)\n", enable);

	if (enable) { // stream on
		if (ctx->s_ctx.mode[ctx->current_scenario_id].aov_mode) {
			stream_refcnt_for_aov = 1;
		}
		subdrv_i2c_wr_u8(ctx, 0x0100, 0x01);
	} else { // stream off
		subdrv_i2c_wr_u8(ctx, 0x0100, 0x00);
		if (stream_refcnt_for_aov) {
			// i2c bus scl4 on apmcu side
			ret = pinctrl_select_state(
				_adaptor_ctx->pinctrl,
				_adaptor_ctx->state[STATE_SCL_AP]);
			if (ret < 0) {
				DRV_LOG_MUST(ctx,
					"konkafront select(%s)(fail),ret(%d)\n",
					state_names[STATE_SCL_AP], ret);
				return ret;
			}
			DRV_LOG(ctx, "konkafront select(%s)(correct)\n", state_names[STATE_SCL_AP]);

			// i2c bus sda4 on apmcu side
			ret = pinctrl_select_state(
				_adaptor_ctx->pinctrl,
				_adaptor_ctx->state[STATE_SDA_AP]);
			if (ret < 0) {
				DRV_LOG_MUST(ctx,
					"konkafront select(%s)(fail),ret(%d)\n",
					state_names[STATE_SDA_AP], ret);
				return ret;
			}
			DRV_LOG(ctx, "konkafront select(%s)(correct)\n", state_names[STATE_SDA_AP]);
			mdelay(1);
		}
		stream_refcnt_for_aov = 0;

	}
	return ret;
}

#ifdef KONKAFRONT_AOV_MCLK_26M
static int set_pwr_seq_reset_view_to_sensing(void *arg)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;
	DRV_LOGE(ctx, "konkafront set_pwr_seq_reset_view_to_sensing");

	int ret = 0;
	struct adaptor_ctx *_adaptor_ctx = NULL;
	struct v4l2_subdev *sd = NULL;

	if (ctx->i2c_client)
		sd = i2c_get_clientdata(ctx->i2c_client);
	if (sd)
		_adaptor_ctx = to_ctx(sd);
	if (!_adaptor_ctx)
		return -ENODEV;

	/* switch viewing mode sw stand-by to hw stand-by */
	// 1. set gpio
	// xclr(reset) = 0
	ret = pinctrl_select_state(
		_adaptor_ctx->pinctrl,
		_adaptor_ctx->state[STATE_RST_LOW]);
	if (ret < 0) {
		DRV_LOG_MUST(ctx,
			"select(%s)(fail),ret(%d)\n",
			state_names[STATE_RST_LOW], ret);
		return ret;
	}
	DRV_LOG(ctx, "select(%s)(correct)\n", state_names[STATE_RST_LOW]);
	mdelay(1);	// response time T4-T6 in datasheet

#ifdef PWR_SEQ_ALL_USE_FOR_AOV_MODE_TRANSITION
	ret = pwr_seq_common_disable_for_mode_transition(_adaptor_ctx);
	if (ret < 0) {
		DRV_LOG_MUST(ctx,
			"pwr_seq_common_disable_for_mode_transition(fail),ret(%d)\n",
			ret);
		return ret;
	}
	DRV_LOG(ctx, "pwr_seq_common_disable_for_mode_transition(correct)\n");
	// switch hw stand-by to sensing mode sw stand-by
	ret = pwr_seq_common_enable_for_mode_transition(_adaptor_ctx);
	if (ret < 0) {
		DRV_LOG_MUST(ctx,
			"pwr_seq_common_enable_for_mode_transition(fail),ret(%d)\n",
			ret);
		return ret;
	}
	DRV_LOG(ctx, "pwr_seq_common_enable_for_mode_transition)(correct)\n");
#endif
	// xclr(reset) = 1
	ret = pinctrl_select_state(
		_adaptor_ctx->pinctrl,
		_adaptor_ctx->state[STATE_RST_HIGH]);
	if (ret < 0) {
		DRV_LOG_MUST(ctx,
			"select(%s)(fail),ret(%d)\n",
			state_names[STATE_RST_HIGH], ret);
		return ret;
	}
	DRV_LOG(ctx, "select(%s)(correct)\n", state_names[STATE_RST_HIGH]);
	mdelay(4);	// response time T7 in datasheet
	return ret;

}
#endif /* KONKAFRONT_AOV_MCLK_26M */

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

	DRV_LOG_MUST(ctx, "temperature: %d degrees\n", temperature_convert);
	return temperature_convert;
}

static u16 get_gain2reg(u32 gain)
{
	return (1024 - (1024 * BASEGAIN) / gain);
}

static void set_group_hold(void *arg, u8 en)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;

	if (en)
		set_i2c_buffer(ctx, SEAMLEES_GRP_HOLD, 0x01);
	else
		set_i2c_buffer(ctx, SEAMLEES_GRP_HOLD, 0x00);
}



static int konkafront_check_sensor_id(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	return konkafront_get_imgsensor_id(ctx, (u32 *)para);
}

static int konkafront_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	enum SENSOR_SCENARIO_ID_ENUM scenario_id;
	struct mtk_hdr_ae *ae_ctrl = NULL;
	u64 *feature_data = (u64 *)para;
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
	DRV_LOG_MUST(ctx,
		"E: set seamless switch %u %u\n",
		ctx->current_scenario_id, scenario_id);
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
	if (ctx->s_ctx.mode[scenario_id].seamless_switch_mode_setting_table
		== NULL) {
		DRV_LOGE(ctx, "Please implement seamless_switch setting\n");
		return ERROR_NONE;
	}

	exp_cnt = ctx->s_ctx.mode[scenario_id].exp_cnt;
	ctx->is_seamless = TRUE;
	update_mode_info(ctx, scenario_id);

	subdrv_i2c_wr_u8(ctx, SEAMLEES_GRP_HOLD, 0x01);
	subdrv_i2c_wr_u8(ctx, ctx->s_ctx.reg_addr_fast_mode, 0x02);
	i2c_table_write(ctx,
		ctx->s_ctx.mode[scenario_id].seamless_switch_mode_setting_table,
		ctx->s_ctx.mode[scenario_id].seamless_switch_mode_setting_len);

	if (ae_ctrl) {
		switch (ctx->s_ctx.mode[scenario_id].hdr_mode) {
		case HDR_RAW_STAGGER:
			set_multi_shutter_frame_length(ctx, (u64 *)&ae_ctrl->exposure, exp_cnt, 0);
			set_multi_gain(ctx, (u32 *)&ae_ctrl->gain, exp_cnt);
			break;
		default:
			set_shutter(ctx, ae_ctrl->exposure.le_exposure);
			set_gain(ctx, ae_ctrl->gain.le_gain);
			break;
		}
	}
	subdrv_i2c_wr_u8(ctx, SEAMLEES_GRP_HOLD, 0x00);

	ctx->fast_mode_on = TRUE;
	ctx->ref_sof_cnt = ctx->sof_cnt;
	ctx->is_seamless = FALSE;
	DRV_LOG_MUST(ctx, "X: set seamless switch done\n");
	return ERROR_NONE;
}

static int konkafront_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u32 mode = *((u32 *)para);

	if (mode != ctx->test_pattern)
		DRV_LOG_MUST(ctx, "mode(%u->%u)\n", ctx->test_pattern, mode);
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


// static int get_eeprom_common_data(struct subdrv_ctx *ctx, u8 *para, u32 *len)
// {
// 	u32 addr_sensorver = 0x0018;
// 	struct oplus_eeprom_info_struct* infoPtr;
// 	memcpy(para, (u8*)(&oplus_eeprom_info), sizeof(oplus_eeprom_info));
// 	infoPtr = (struct oplus_eeprom_info_struct*)(para);
// 	*len = sizeof(oplus_eeprom_info);
// 	if (subdrv_i2c_rd_u8(ctx, addr_sensorver) != 0x00) {
// 		printk("need to convert to 10bit");
// 		infoPtr->afInfo[0] = (kal_uint8)((infoPtr->afInfo[1] << 4) | (infoPtr->afInfo[0] >> 4));
// 		infoPtr->afInfo[1] = (kal_uint8)(infoPtr->afInfo[1] >> 4);
// 		infoPtr->afInfo[2] = (kal_uint8)((infoPtr->afInfo[3] << 4) | (infoPtr->afInfo[2] >> 4));
// 		infoPtr->afInfo[3] = (kal_uint8)(infoPtr->afInfo[3] >> 4);
// 		infoPtr->afInfo[4] = (kal_uint8)((infoPtr->afInfo[5] << 4) | (infoPtr->afInfo[4] >> 4));
// 		infoPtr->afInfo[5] = (kal_uint8)(infoPtr->afInfo[5] >> 4);
// 	}
// 	return 0;
// }

static int get_eeprom_common_data(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	memcpy(para, (u8*)(&oplus_eeprom_info), sizeof(oplus_eeprom_info));
	*len = sizeof(oplus_eeprom_info);
	return 0;
}

static bool read_cmos_eeprom_p8(struct subdrv_ctx *ctx, kal_uint16 addr,
                    BYTE *data, int size)
{
	if (adaptor_i2c_rd_p8(ctx->i2c_client, KONKAFRONT_EEPROM_READ_ID >> 1,
		addr, data, size) < 0) {
		DRV_LOGE(ctx, "konkafront read_cmos_eeprom_p8 failed\n");
		return false;
	}
	DRV_LOGE(ctx, "konkafront read_cmos_eeprom_p8 success read size = %d\n", size);
	return true;
}

static void read_otp_info(struct subdrv_ctx *ctx)
{
	DRV_LOGE(ctx, "konkafront read_otp_info begin\n");
	read_cmos_eeprom_p8(ctx, 0, otp_data_checksum, OTP_SIZE);
	DRV_LOGE(ctx, "konkafront read_otp_info end\n");
}

static int konkafront_get_otp_checksum_data(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u32 *feature_return_para_32 = (u32 *)para;
	DRV_LOGE(ctx, "get otp data");
	if (otp_data_checksum[0] == 0) {
		read_otp_info(ctx);
	} else {
		DRV_LOG(ctx, "otp data has already read read read");
	}
	memcpy(feature_return_para_32, (UINT32 *)otp_data_checksum, sizeof(otp_data_checksum));
	*len = sizeof(otp_data_checksum);
	return 0;
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
			subdrv_i2c_wr_seq_p8(ctx, addr, pbuf, size);
			subdrv_i2c_wr_u8(ctx, 0x3621, 0x01);
			LOG_INF("set QSC calibration data done.");
		} else {
			subdrv_i2c_wr_u8(ctx, 0x32D2, 0x00);
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

	/* LRC data */
	support = info[idx].lrc_support;
	pbuf = info[idx].preload_lrc_table;
	size = info[idx].lrc_size;
	if (support) {
		if (pbuf != NULL && size > 0) {
			subdrv_i2c_wr_seq_p8(ctx, LRC_L_REG, pbuf, size / 2); // L data
			subdrv_i2c_wr_seq_p8(ctx, LRC_R_REG, pbuf + size / 2, size / 2); // R data
			DRV_LOG(ctx, "set LRC calibration data done.");
		} else {
			DRV_LOGE(ctx, "LRC calibration data error");
		}
	}

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

	/* LRC data */
	support = info[idx].lrc_support;
	size = info[idx].lrc_size;
	addr = info[idx].addr_lrc;
	buf = info[idx].lrc_table;
	if (support && size > 0) {
		if (info[idx].preload_lrc_table == NULL) {
			info[idx].preload_lrc_table = kmalloc(size, GFP_KERNEL);
			if (buf == NULL) {
				if (!read_cmos_eeprom_p8(ctx, addr, info[idx].preload_lrc_table, size)) {
					DRV_LOGE(ctx, "preload LRC data failed");
				}
			} else {
				memcpy(info[idx].preload_lrc_table, buf, size);
			}
			DRV_LOG(ctx, "preload LRC data %u bytes", size);
		} else {
			DRV_LOG(ctx, "LRC data is already preloaded %u bytes", size);
		}
	}

	ctx->is_read_preload_eeprom = 1;
}

static int konkafront_set_awb_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len) {
	struct SET_SENSOR_AWB_GAIN *awb_gain = (struct SET_SENSOR_AWB_GAIN *)para;

	set_group_hold((void *)ctx, 1);
	subdrv_i2c_wr_u8(ctx, 0x0B8E, (u8)(awb_gain->ABS_GAIN_GR >> 8)); // GR
	subdrv_i2c_wr_u8(ctx, 0x0B8F, (u8)(awb_gain->ABS_GAIN_GR & 0xFF)); // GR
	subdrv_i2c_wr_u8(ctx, 0x0B90, (u8)(awb_gain->ABS_GAIN_R >> 8)); // R
	subdrv_i2c_wr_u8(ctx, 0x0B91, (u8)(awb_gain->ABS_GAIN_R & 0xFF)); // R
	subdrv_i2c_wr_u8(ctx, 0x0B92, (u8)(awb_gain->ABS_GAIN_B >> 8)); // B
	subdrv_i2c_wr_u8(ctx, 0x0B93, (u8)(awb_gain->ABS_GAIN_B & 0xFF)); // B
	subdrv_i2c_wr_u8(ctx, 0x0B94, (u8)(awb_gain->ABS_GAIN_GB >> 8)); // GB
	subdrv_i2c_wr_u8(ctx, 0x0B95, (u8)(awb_gain->ABS_GAIN_GB & 0xFF)); // GB
	set_group_hold((void *)ctx, 0);

	DRV_LOG(ctx, "ABS_GAIN_GR(%d) ABS_GAIN_R(%d) ABS_GAIN_B(%d) ABS_GAIN_GB(%d)",
		awb_gain->ABS_GAIN_GR, awb_gain->ABS_GAIN_R, awb_gain->ABS_GAIN_B, awb_gain->ABS_GAIN_GB);

	return 0;
}
