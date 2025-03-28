// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023 Oplus. All rights reserved.
 */

#ifndef _OPLUS_KD_EEPROM_H
#define _OPLUS_KD_EEPROM_H

#define OPLUS_CAMERA_COMMON_DATA_LENGTH 40

#ifndef OPLUS_FEATURE_CAMERA_COMMON
#define OPLUS_FEATURE_CAMERA_COMMON
#endif

enum {
	EEPROM_META_MODULE_ID = 0,
	EEPROM_META_SENSOR_ID,
	EEPROM_META_LENS_ID,
	EEPROM_META_VCM_ID,
	EEPROM_META_MIRROR_FLIP,
	EEPROM_META_MODULE_SN,
	EEPROM_META_AF_CODE,
	EEPROM_META_AF_FLAG,
	EEPROM_META_STEREO_DATA,
	EEPROM_META_STEREO_MW_MAIN_DATA,
	EEPROM_META_STEREO_MT_MAIN_DATA,
	EEPROM_META_STEREO_MT_MAIN_DATA_105CM,
	EEPROM_META_DISTORTION_DATA,
	EEPROM_META_MAX,
};

enum {
	EEPROM_STEREODATA = 0,
	EEPROM_STEREODATA_MT_MAIN,
	EEPROM_STEREODATA_MW_MAIN,
	EEPROM_STEREODATA_MT_MAIN_105CM,
	EEPROM_STEREODATA_TT_MASETR_120CM,
	EEPROM_STEREODATA_TT_SLAVE_120CM,
};

struct eeprom_map_info {
	kal_uint16 meta;
	kal_uint16 start;
	kal_uint16 valid;
	kal_uint16 checksum;
	int size;
	kal_bool present;
};

struct eeprom_addr_table_struct {
	u32 header_id;
	u32 addr_header_id;
	u8  i2c_read_id;
	u8  i2c_write_id;
	u16 addr_modinfo;
	u16 addr_sensorid;
	u16 addr_lens;
	u16 addr_vcm;
	u16 addr_modinfoflag;
	u16 addr_af;
	u16 addr_afmacro;
	u16 addr_afinf;
	u16 addr_afflag;
	u16 addr_qrcode;
	u16 addr_qrcodeflag;
};
#endif  /*  _OPLUS_KD_EEPROM_H */
