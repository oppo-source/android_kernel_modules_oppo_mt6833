/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 OPLUS Inc.
 */

#define PFX "CAM_CAL_KONKAUTELE"
#define pr_fmt(fmt) PFX "[%s] " fmt, __func__

#include <linux/kernel.h>
#include "cam_cal_list.h"
#include "eeprom_i2c_common_driver.h"
#include "eeprom_i2c_custom_driver.h"
#include "cam_cal_config.h"
// #include "oplus_kd_imgsensor.h"
#include "kd_imgsensor.h"
#include <linux/delay.h>


#define READ_4000K 0
#define DW9786_CHIP_EN 0xE000
#define KONKAUTELE_AF_SLAVE_ID	0x32
#define MAX_BUF_SIZE 511
#define MAX_VAL_NUM_U8 (MAX_BUF_SIZE - 2)

static unsigned int do_module_version_konkautele(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData);
static unsigned int do_single_lsc_konkautele(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData);
static unsigned int do_2a_gain_konkautele(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData);
static unsigned int do_lens_id_konkautele(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData);
static unsigned int do_pdaf_konkautele(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData);
static unsigned int layout_check_konkautele(struct EEPROM_DRV_FD_DATA *pdata, unsigned int sensorID,
		unsigned int *_cfg);

static struct STRUCT_CALIBRATION_LAYOUT_STRUCT cal_layout_table = {
	0x00000006, 0x00EA0129, CAM_CAL_SINGLE_EEPROM_DATA,
	{
		{0x00000001, 0x00000000, 0x00000002, do_module_version_konkautele},
		{0x00000001, 0x00000000, 0x00000002, do_part_number},
		{0x00000001, 0x00000CFE, 0x0000074C, do_single_lsc_konkautele},
		{0x00000001, 0x00000020, 0x0000000E, do_2a_gain_konkautele}, //Start address, block size is useless
		{0x00000001, 0x00001456, 0x00001020, do_pdaf_konkautele},
		{0x00000001, 0x00001300, 0x000005EC, do_pdaf},
		{0x00000000, 0x00000FAE, 0x00000550, do_stereo_data},
		{0x00000001, 0x00000000, 0x00004000, do_dump_all}, //temp 4000,
		{0x00000001, 0x00000008, 0x00000002, do_lens_id_konkautele}
	}
};

struct STRUCT_CAM_CAL_CONFIG_STRUCT konkautele_op_eeprom = {
	.name = "konkautele_op_eeprom",
	.check_layout_function = layout_check_konkautele,
	.read_function = Common_read_region,
	.layout = &cal_layout_table,
	.sensor_id = KONKAUTELE_SENSOR_ID,
	.i2c_write_id = 0x74,
	.max_size = 0x4000,
	.enable_preload = 1,
	.preload_size = 0x4000,
};

struct mutex dw9786_mutex;
EXPORT_SYMBOL(dw9786_mutex);

int adaptor_i2c_wr_u16(struct i2c_client *i2c_client, u16 addr, u16 reg,
		       u16 val)
{
	int ret;
	u8 buf[4];
	struct i2c_msg msg;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;
	buf[2] = val >> 8;
	buf[3] = val & 0xff;

	msg.addr = addr;
	msg.flags = i2c_client->flags;
	msg.buf = buf;
	msg.len = sizeof(buf);

	ret = i2c_transfer(i2c_client->adapter, &msg, 1);
	if (ret < 0)
		dev_info(&i2c_client->dev, "i2c transfer failed (%d)\n", ret);

	return ret;
}

int adaptor_i2c_rd_u16(struct i2c_client *i2c_client,
		u16 addr, u16 reg, u16 *val)
{
	int ret;
	u8 buf[2];
	struct i2c_msg msg[2];

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
		return ret;
	}

	*val = ((u16)buf[0] << 8) | buf[1];

	return 0;
}

int adaptor_i2c_wr_p8(struct i2c_client *i2c_client, u16 addr, u16 reg,
		      u8 *p_vals, u32 n_vals)
{
	u8 *buf, *pbuf, *pdata;
	struct i2c_msg msg;
	int ret, sent, total, cnt;

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
			dev_info(&i2c_client->dev, "i2c transfer failed (%d)\n",
				 ret);
			kfree(buf);
			return -EIO;
		}

		sent += cnt;
		pdata += cnt;
	}

	kfree(buf);

	return 0;
}

static unsigned int do_module_version_konkautele(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData)
{
	(void) pdata;
	(void) start_addr;
	(void) block_size;
	(void) pGetSensorCalData;

	struct STRUCT_CAM_CAL_DATA_STRUCT *pCamCalData =
				(struct STRUCT_CAM_CAL_DATA_STRUCT *)pGetSensorCalData;
	int ret = 0;
	u8 retry = 5;
	unsigned short sensor_id;
	int read_data_size;
	unsigned int af_offset = 0x0006;
	uint16_t chip_en;
	struct i2c_client *client;
	unsigned short stdby[17] = {0x0100, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
								0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
								0x0000, 0x0000, 0x0000};

	client = pdata->pdrv->pi2c_client;
	if (client == NULL) {
		error_log("i2c_client == NULL\n");
		return 0;
	}
	client->addr = KONKAUTELE_AF_SLAVE_ID >> 1;
	do{
		mutex_lock(&dw9786_mutex);
		printk("%s dw9786_mutex: %p\n", __func__, &dw9786_mutex);
		ret = adaptor_i2c_rd_u16(client, client->addr, DW9786_CHIP_EN, &chip_en);
		printk("%s DW9786_CHIP_EN: 0x%x, ret: %d\n", __func__, chip_en, ret);
		if(chip_en != 0x0001) {
			ret = adaptor_i2c_wr_u16(client, client->addr, DW9786_CHIP_EN, 0x0000);
			msleep(2);
			adaptor_i2c_wr_p8(client, client->addr, DW9786_CHIP_EN, (unsigned char *)stdby, 34);
			msleep(5);
			ret = adaptor_i2c_wr_u16(client, client->addr, 0xE004, 0x0001);
			msleep(20);
		}
		mutex_unlock(&dw9786_mutex);
		read_data_size = read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
				af_offset, 2, (unsigned char *)&sensor_id);
		retry--;
		printk("ccccc sensor_id:%d  try:%d, ret:%d, \n", sensor_id, retry, ret);
	} while (sensor_id != 0x0129 && retry > 0);

	return CAM_CAL_ERR_NO_ERR;
}

static unsigned int do_single_lsc_konkautele(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData)
{
	struct STRUCT_CAM_CAL_DATA_STRUCT *pCamCalData =
				(struct STRUCT_CAM_CAL_DATA_STRUCT *)pGetSensorCalData;

	int read_data_size;
	unsigned int err = CamCalReturnErr[pCamCalData->Command];
	unsigned short table_size;

	if (pCamCalData->DataVer >= CAM_CAL_TYPE_NUM) {
		err = CAM_CAL_ERR_NO_DEVICE;
		error_log("Read Failed\n");
		show_cmd_error_log(pCamCalData->Command);
		return err;
	}
	if (block_size != CAM_CAL_SINGLE_LSC_SIZE)
		error_log("block_size(%d) is not match (%d)\n",
				block_size, CAM_CAL_SINGLE_LSC_SIZE);

	pCamCalData->SingleLsc.LscTable.MtkLcsData.MtkLscType = 2;//mtk type
	pCamCalData->SingleLsc.LscTable.MtkLcsData.PixId = 8;

	table_size = 1868;

	pr_debug("lsc table_size %d\n", table_size);
	pCamCalData->SingleLsc.LscTable.MtkLcsData.TableSize = table_size;
	if (table_size > 0) {
		pCamCalData->SingleLsc.TableRotation = 3; // flip
		debug_log("u4Offset=%d u4Length=%d", start_addr, table_size);
		read_data_size = read_data(pdata,
			pCamCalData->sensorID, pCamCalData->deviceID,
			start_addr, table_size, (unsigned char *)
			&pCamCalData->SingleLsc.LscTable.MtkLcsData.SlimLscType);
		if (table_size == read_data_size)
			err = CAM_CAL_ERR_NO_ERR;
		else {
			error_log("Read Failed\n");
			err = CamCalReturnErr[pCamCalData->Command];
			show_cmd_error_log(pCamCalData->Command);
		}
	}
	#ifdef DEBUG_CALIBRATION_LOAD
	pr_debug("======================SingleLsc Data==================\n");
	pr_debug("[1st] = %x, %x, %x, %x\n",
		pCamCalData->SingleLsc.LscTable.Data[0],
		pCamCalData->SingleLsc.LscTable.Data[1],
		pCamCalData->SingleLsc.LscTable.Data[2],
		pCamCalData->SingleLsc.LscTable.Data[3]);
	pr_debug("[1st] = SensorLSC(1)?MTKLSC(2)?  %x\n",
		pCamCalData->SingleLsc.LscTable.MtkLcsData.MtkLscType);
	pr_debug("CapIspReg =0x%x, 0x%x, 0x%x, 0x%x, 0x%x",
		pCamCalData->SingleLsc.LscTable.MtkLcsData.CapIspReg[0],
		pCamCalData->SingleLsc.LscTable.MtkLcsData.CapIspReg[1],
		pCamCalData->SingleLsc.LscTable.MtkLcsData.CapIspReg[2],
		pCamCalData->SingleLsc.LscTable.MtkLcsData.CapIspReg[3],
		pCamCalData->SingleLsc.LscTable.MtkLcsData.CapIspReg[4]);
	pr_debug("RETURN = 0x%x\n", err);
	pr_debug("======================SingleLsc Data==================\n");
	#endif

	return err;
}

static unsigned int do_2a_gain_konkautele(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData)
{
	struct STRUCT_CAM_CAL_DATA_STRUCT *pCamCalData =
				(struct STRUCT_CAM_CAL_DATA_STRUCT *)pGetSensorCalData;
	int read_data_size;
	unsigned int err = CamCalReturnErr[pCamCalData->Command];

	long long CalGain, FacGain, CalValue;
	unsigned char AWBAFConfig = 0xf;

	unsigned short AFInf, AFMacro, AF_50cm;
	int tempMax = 0;
	int CalR = 1, CalGr = 1, CalGb = 1, CalG = 1, CalB = 1;
	int FacR = 1, FacGr = 1, FacGb = 1, FacG = 1, FacB = 1;
	int rgCalValue = 1, bgCalValue = 1;
	unsigned int awb_offset, af_offset;

	(void) start_addr;
	(void) block_size;

	pr_debug("In %s: sensor_id=%x\n", __func__, pCamCalData->sensorID);
	memset((void *)&pCamCalData->Single2A, 0, sizeof(struct STRUCT_CAM_CAL_SINGLE_2A_STRUCT));
	/* Check rule */
	if (pCamCalData->DataVer >= CAM_CAL_TYPE_NUM) {
		err = CAM_CAL_ERR_NO_DEVICE;
		error_log("Read Failed\n");
		show_cmd_error_log(pCamCalData->Command);
		return err;
	}
	/* Check AWB & AF enable bit */
	pCamCalData->Single2A.S2aVer = 0x01;
	pCamCalData->Single2A.S2aBitEn = (0x03 & AWBAFConfig);
	pCamCalData->Single2A.S2aAfBitflagEn = (0x0C & AWBAFConfig);
	debug_log("S2aBitEn=0x%02x", pCamCalData->Single2A.S2aBitEn);
	/* AWB Calibration Data*/
	if (0x1 & AWBAFConfig) {
		pCamCalData->Single2A.S2aAwb.rGainSetNum = 0x02;
		awb_offset = 0x0060;
		read_data_size = read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
				awb_offset, 8, (unsigned char *)&CalValue);
		if (read_data_size > 0)	{
			debug_log( "Read CalValue OK\n");
			rgCalValue  = CalValue & 0xFFFF;
			bgCalValue = (CalValue >> 16) & 0xFFFF;
			debug_log("Light source calibration 5100K value R/G:%d, B/G:%d",rgCalValue, bgCalValue);
			err = CAM_CAL_ERR_NO_ERR;
		} else {
			pCamCalData->Single2A.S2aBitEn = CAM_CAL_NONE_BITEN;
			error_log("Read CalGain Failed\n");
			show_cmd_error_log(pCamCalData->Command);
		}
		/* AWB Unit Gain (5000K) */
		debug_log("5000K AWB\n");
		awb_offset = 0x0020;
		read_data_size = read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
				awb_offset, 8, (unsigned char *)&CalGain);
		if (read_data_size > 0)	{
			debug_log("Read CalGain OK %x\n", read_data_size);
			CalR  = CalGain & 0xFFFF;
			CalGr = (CalGain >> 16) & 0xFFFF;
			CalGb = (CalGain >> 32) & 0xFFFF;
			CalG  = ((CalGr + CalGb) + 1) >> 1;
			CalB  = (CalGain >> 48) & 0xFFFF;
			CalR  = CalR * rgCalValue / 1000;
			CalB  = CalB * bgCalValue / 1000;
			if (CalR > CalG)
				/* R > G */
				if (CalR > CalB)
					tempMax = CalR;
				else
					tempMax = CalB;
			else
				/* G > R */
				if (CalG > CalB)
					tempMax = CalG;
				else
					tempMax = CalB;
			debug_log("UnitR:%d, UnitG:%d, UnitB:%d, New Unit Max=%d",
					CalR, CalG, CalB, tempMax);
			err = CAM_CAL_ERR_NO_ERR;
		} else {
			pCamCalData->Single2A.S2aBitEn = CAM_CAL_NONE_BITEN;
			error_log("Read CalGain Failed\n");
			show_cmd_error_log(pCamCalData->Command);
		}
		if (CalGain != 0x0000000000000000 &&
			CalGain != 0xFFFFFFFFFFFFFFFF &&
			CalR    != 0x00000000 &&
			CalG    != 0x00000000 &&
			CalB    != 0x00000000) {
			pCamCalData->Single2A.S2aAwb.rGainSetNum = 1;
			pCamCalData->Single2A.S2aAwb.rUnitGainu4R =
					(unsigned int)((tempMax * 512 + (CalR >> 1)) / CalR);
			pCamCalData->Single2A.S2aAwb.rUnitGainu4G =
					(unsigned int)((tempMax * 512 + (CalG >> 1)) / CalG);
			pCamCalData->Single2A.S2aAwb.rUnitGainu4B =
					(unsigned int)((tempMax * 512 + (CalB >> 1)) / CalB);
		} else {
			pr_debug("There are something wrong on EEPROM, plz contact module vendor!!\n");
			pr_debug("Unit R=%d G=%d B=%d!!\n", CalR, CalG, CalB);
		}
		/* AWB Golden Gain (5000K) */
		awb_offset = 0x0028;
		read_data_size = read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
				awb_offset, 8, (unsigned char *)&FacGain);
		if (read_data_size > 0)	{
			debug_log("Read FacGain OK\n");
			FacR  = FacGain & 0xFFFF;
			FacGr = (FacGain >> 16) & 0xFFFF;
			FacGb = (FacGain >> 32) & 0xFFFF;
			FacG  = ((FacGr + FacGb) + 1) >> 1;
			FacB  = (FacGain >> 48) & 0xFFFF;
			if (FacR > FacG)
				if (FacR > FacB)
					tempMax = FacR;
				else
					tempMax = FacB;
			else
				if (FacG > FacB)
					tempMax = FacG;
				else
					tempMax = FacB;
			debug_log("GoldenR:%d, GoldenG:%d, GoldenB:%d, New Golden Max=%d",
					FacR, FacG, FacB, tempMax);
			err = CAM_CAL_ERR_NO_ERR;
		} else {
			pCamCalData->Single2A.S2aBitEn = CAM_CAL_NONE_BITEN;
			error_log("Read FacGain Failed\n");
			show_cmd_error_log(pCamCalData->Command);
		}
		if (FacGain != 0x0000000000000000 &&
			FacGain != 0xFFFFFFFFFFFFFFFF &&
			FacR    != 0x00000000 &&
			FacG    != 0x00000000 &&
			FacB    != 0x00000000)	{
			pCamCalData->Single2A.S2aAwb.rGoldGainu4R =
					(unsigned int)((tempMax * 512 + (FacR >> 1)) / FacR);
			pCamCalData->Single2A.S2aAwb.rGoldGainu4G =
					(unsigned int)((tempMax * 512 + (FacG >> 1)) / FacG);
			pCamCalData->Single2A.S2aAwb.rGoldGainu4B =
					(unsigned int)((tempMax * 512 + (FacB >> 1)) / FacB);
		} else {
			pr_debug("There are something wrong on EEPROM, plz contact module vendor!!");
			pr_debug("Golden R=%d G=%d B=%d\n", FacR, FacG, FacB);
		}
		/* Set AWB to 3A Layer */
		pCamCalData->Single2A.S2aAwb.rValueR   = CalR;
		pCamCalData->Single2A.S2aAwb.rValueGr  = CalGr;
		pCamCalData->Single2A.S2aAwb.rValueGb  = CalGb;
		pCamCalData->Single2A.S2aAwb.rValueB   = CalB;
		pCamCalData->Single2A.S2aAwb.rGoldenR  = FacR;
		pCamCalData->Single2A.S2aAwb.rGoldenGr = FacGr;
		pCamCalData->Single2A.S2aAwb.rGoldenGb = FacGb;
		pCamCalData->Single2A.S2aAwb.rGoldenB  = FacB;
		#ifdef DEBUG_CALIBRATION_LOAD
		pr_debug("======================AWB CAM_CAL==================\n");
		pr_debug("AWB Calibration @5100K\n");
		pr_debug("[CalGain] = 0x%x\n", CalGain);
		pr_debug("[FacGain] = 0x%x\n", FacGain);
		pr_debug("[rCalGain.u4R] = %d\n", pCamCalData->Single2A.S2aAwb.rUnitGainu4R);
		pr_debug("[rCalGain.u4G] = %d\n", pCamCalData->Single2A.S2aAwb.rUnitGainu4G);
		pr_debug("[rCalGain.u4B] = %d\n", pCamCalData->Single2A.S2aAwb.rUnitGainu4B);
		pr_debug("[rFacGain.u4R] = %d\n", pCamCalData->Single2A.S2aAwb.rGoldGainu4R);
		pr_debug("[rFacGain.u4G] = %d\n", pCamCalData->Single2A.S2aAwb.rGoldGainu4G);
		pr_debug("[rFacGain.u4B] = %d\n", pCamCalData->Single2A.S2aAwb.rGoldGainu4B);
		#endif
		/* AWB Unit Gain (4000K) */
		#if READ_4000K //zemin.lai@CamTuning delet ,module not support 4000k otp 20220421
		CalR = 0; CalGr = 0; CalGb = 0; CalG = 0; CalB = 0;
		tempMax = 0;
		debug_log("4000K AWB\n");
		awb_offset = 0x0032;
		read_data_size = read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
				awb_offset, 8, (unsigned char *)&CalGain);
		if (read_data_size > 0)	{
			debug_log("Read CalGain OK %x\n", read_data_size);
			CalR  = CalGain & 0xFFFF;
			CalGr = (CalGain >> 16) & 0xFFFF;
			CalGb = (CalGain >> 32) & 0xFFFF;
			CalG  = ((CalGr + CalGb) + 1) >> 1;
			CalB  = (CalGain >> 48) & 0xFFFF;
			if (CalR > CalG)
				/* R > G */
				if (CalR > CalB)
					tempMax = CalR;
				else
					tempMax = CalB;
			else
				/* G > R */
				if (CalG > CalB)
					tempMax = CalG;
				else
					tempMax = CalB;
			debug_log(
					"UnitR:%d, UnitG:%d, UnitB:%d, New Unit Max=%d",
					CalR, CalG, CalB, tempMax);
			err = CAM_CAL_ERR_NO_ERR;
		} else {
			pCamCalData->Single2A.S2aBitEn = CAM_CAL_NONE_BITEN;
			error_log("Read CalGain Failed\n");
			show_cmd_error_log(pCamCalData->Command);
		}
		if (CalGain != 0x0000000000000000 &&
			CalGain != 0xFFFFFFFFFFFFFFFF &&
			CalR    != 0x00000000 &&
			CalG    != 0x00000000 &&
			CalB    != 0x00000000) {
			pCamCalData->Single2A.S2aAwb.rUnitGainu4R_mid =
				(unsigned int)((tempMax * 512 + (CalR >> 1)) / CalR);
			pCamCalData->Single2A.S2aAwb.rUnitGainu4G_mid =
				(unsigned int)((tempMax * 512 + (CalG >> 1)) / CalG);
			pCamCalData->Single2A.S2aAwb.rUnitGainu4B_mid =
				(unsigned int)((tempMax * 512 + (CalB >> 1)) / CalB);
		} else {
			pr_debug("There are something wrong on EEPROM, plz contact module vendor!!\n");
			pr_debug("Unit R=%d G=%d B=%d!!\n", CalR, CalG, CalB);
		}
		/* AWB Golden Gain (4000K) */
		FacR = 0; FacGr = 0; FacGb = 0; FacG = 0; FacB = 0;
		tempMax = 0;
		awb_offset = 0x003A;
		read_data_size = read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
				awb_offset, 8, (unsigned char *)&FacGain);
		if (read_data_size > 0)	{
			debug_log("Read FacGain OK\n");
			FacR  = FacGain & 0xFFFF;
			FacGr = (FacGain >> 16) & 0xFFFF;
			FacGb = (FacGain >> 32) & 0xFFFF;
			FacG  = ((FacGr + FacGb) + 1) >> 1;
			FacB  = (FacGain >> 48) & 0xFFFF;
			if (FacR > FacG)
				if (FacR > FacB)
					tempMax = FacR;
				else
					tempMax = FacB;
			else
				if (FacG > FacB)
					tempMax = FacG;
				else
					tempMax = FacB;
			debug_log("GoldenR:%d, GoldenG:%d, GoldenB:%d, New Golden Max=%d",
					FacR, FacG, FacB, tempMax);
			err = CAM_CAL_ERR_NO_ERR;
		} else {
			pCamCalData->Single2A.S2aBitEn = CAM_CAL_NONE_BITEN;
			error_log("Read FacGain Failed\n");
			show_cmd_error_log(pCamCalData->Command);
		}
		if (FacGain != 0x0000000000000000 &&
			FacGain != 0xFFFFFFFFFFFFFFFF &&
			FacR    != 0x00000000 &&
			FacG    != 0x00000000 &&
			FacB    != 0x00000000)	{
			pCamCalData->Single2A.S2aAwb.rGoldGainu4R_mid =
				(unsigned int)((tempMax * 512 + (FacR >> 1)) / FacR);
			pCamCalData->Single2A.S2aAwb.rGoldGainu4G_mid =
				(unsigned int)((tempMax * 512 + (FacG >> 1)) / FacG);
			pCamCalData->Single2A.S2aAwb.rGoldGainu4B_mid =
				(unsigned int)((tempMax * 512 + (FacB >> 1)) / FacB);
		} else {
			pr_debug("There are something wrong on EEPROM, plz contact module vendor!!");
			pr_debug("Golden R=%d G=%d B=%d\n", FacR, FacG, FacB);
		}
		#ifdef DEBUG_CALIBRATION_LOAD
		pr_debug("AWB Calibration @4000K\n");
		pr_debug("[CalGain] = 0x%x\n", CalGain);
		pr_debug("[FacGain] = 0x%x\n", FacGain);
		pr_debug("[rCalGain.u4R] = %d\n", pCamCalData->Single2A.S2aAwb.rUnitGainu4R_mid);
		pr_debug("[rCalGain.u4G] = %d\n", pCamCalData->Single2A.S2aAwb.rUnitGainu4G_mid);
		pr_debug("[rCalGain.u4B] = %d\n", pCamCalData->Single2A.S2aAwb.rUnitGainu4B_mid);
		pr_debug("[rFacGain.u4R] = %d\n", pCamCalData->Single2A.S2aAwb.rGoldGainu4R_mid);
		pr_debug("[rFacGain.u4G] = %d\n", pCamCalData->Single2A.S2aAwb.rGoldGainu4G_mid);
		pr_debug("[rFacGain.u4B] = %d\n", pCamCalData->Single2A.S2aAwb.rGoldGainu4B_mid);
		#endif
		#endif
		/* AWB Unit Gain (2850K) */
		CalR = 0; CalGr = 0; CalGb = 0; CalG = 0; CalB = 0;
		tempMax = 0;
		debug_log("2850K AWB\n");
		debug_log( "3100K AWB\n");
		rgCalValue = 0; bgCalValue = 0;
		awb_offset = 0x006C;
		read_data_size = read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
				awb_offset, 8, (unsigned char *)&CalValue);
		if (read_data_size > 0)	{
			debug_log( "Read CalValue OK\n");
			rgCalValue  = CalValue & 0xFFFF;
			bgCalValue = (CalValue >> 16) & 0xFFFF;
			debug_log("Light source calibration value 3100 R/G:%d, B/G:%d",rgCalValue, bgCalValue);
			err = CAM_CAL_ERR_NO_ERR;
		} else {
			pCamCalData->Single2A.S2aBitEn = CAM_CAL_NONE_BITEN;
			error_log("Read CalGain Failed\n");
			show_cmd_error_log(pCamCalData->Command);
		}
		awb_offset = 0x0044;
		read_data_size = read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
				awb_offset, 8, (unsigned char *)&CalGain);
		if (read_data_size > 0)	{
			debug_log("Read CalGain OK %x\n", read_data_size);
			CalR  = CalGain & 0xFFFF;
			CalGr = (CalGain >> 16) & 0xFFFF;
			CalGb = (CalGain >> 32) & 0xFFFF;
			CalG  = ((CalGr + CalGb) + 1) >> 1;
			CalB  = (CalGain >> 48) & 0xFFFF;
			debug_log("CalR:%d, CalB:%d",CalR, CalB);
			CalR  = CalR * rgCalValue / 1000;
			CalB  = CalB * bgCalValue / 1000;
			if (CalR > CalG)
				/* R > G */
				if (CalR > CalB)
					tempMax = CalR;
				else
					tempMax = CalB;
			else
				/* G > R */
				if (CalG > CalB)
					tempMax = CalG;
				else
					tempMax = CalB;
			debug_log("UnitR:%d, UnitG:%d, UnitB:%d, New Unit Max=%d",
					CalR, CalG, CalB, tempMax);
			err = CAM_CAL_ERR_NO_ERR;
		} else {
			pCamCalData->Single2A.S2aBitEn = CAM_CAL_NONE_BITEN;
			error_log("Read CalGain Failed\n");
			show_cmd_error_log(pCamCalData->Command);
		}
		if (CalGain != 0x0000000000000000 &&
			CalGain != 0xFFFFFFFFFFFFFFFF &&
			CalR    != 0x00000000 &&
			CalG    != 0x00000000 &&
			CalB    != 0x00000000) {
			pCamCalData->Single2A.S2aAwb.rGainSetNum = 2;
			pCamCalData->Single2A.S2aAwb.rUnitGainu4R_low =
				(unsigned int)((tempMax * 512 + (CalR >> 1)) / CalR);
			pCamCalData->Single2A.S2aAwb.rUnitGainu4G_low =
				(unsigned int)((tempMax * 512 + (CalG >> 1)) / CalG);
			pCamCalData->Single2A.S2aAwb.rUnitGainu4B_low =
				(unsigned int)((tempMax * 512 + (CalB >> 1)) / CalB);
		} else {
			pr_debug("There are something wrong on EEPROM, plz contact module vendor!!\n");
			pr_debug("Unit R=%d G=%d B=%d!!\n", CalR, CalG, CalB);
		}
		/* AWB Golden Gain (3100K) */
		FacR = 0; FacGr = 0; FacGb = 0; FacG = 0; FacB = 0;
		tempMax = 0;
		awb_offset = 0x004C;
		read_data_size = read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
				awb_offset, 8, (unsigned char *)&FacGain);
		if (read_data_size > 0)	{
			debug_log("Read FacGain OK\n");
			FacR  = FacGain & 0xFFFF;
			FacGr = (FacGain >> 16) & 0xFFFF;
			FacGb = (FacGain >> 32) & 0xFFFF;
			FacG  = ((FacGr + FacGb) + 1) >> 1;
			FacB  = (FacGain >> 48) & 0xFFFF;
			if (FacR > FacG)
				if (FacR > FacB)
					tempMax = FacR;
				else
					tempMax = FacB;
			else
				if (FacG > FacB)
					tempMax = FacG;
				else
					tempMax = FacB;
			debug_log("GoldenR:%d, GoldenG:%d, GoldenB:%d, New Golden Max=%d",
					FacR, FacG, FacB, tempMax);
			err = CAM_CAL_ERR_NO_ERR;
		} else {
			pCamCalData->Single2A.S2aBitEn = CAM_CAL_NONE_BITEN;
			error_log("Read FacGain Failed\n");
			show_cmd_error_log(pCamCalData->Command);
		}
		if (FacGain != 0x0000000000000000 &&
			FacGain != 0xFFFFFFFFFFFFFFFF &&
			FacR    != 0x00000000 &&
			FacG    != 0x00000000 &&
			FacB    != 0x00000000)	{
			pCamCalData->Single2A.S2aAwb.rGoldGainu4R_low =
				(unsigned int)((tempMax * 512 + (FacR >> 1)) / FacR);
			pCamCalData->Single2A.S2aAwb.rGoldGainu4G_low =
				(unsigned int)((tempMax * 512 + (FacG >> 1)) / FacG);
			pCamCalData->Single2A.S2aAwb.rGoldGainu4B_low =
				(unsigned int)((tempMax * 512 + (FacB >> 1)) / FacB);
		} else {
			pr_debug("There are something wrong on EEPROM, plz contact module vendor!!");
			pr_debug("Golden R=%d G=%d B=%d\n", FacR, FacG, FacB);
		}
		#ifdef DEBUG_CALIBRATION_LOAD
		pr_debug("AWB Calibration @3100K\n");
		pr_debug("[CalGain] = 0x%x\n", CalGain);
		pr_debug("[FacGain] = 0x%x\n", FacGain);
		pr_debug("[rCalGain.u4R] = %d\n", pCamCalData->Single2A.S2aAwb.rUnitGainu4R_low);
		pr_debug("[rCalGain.u4G] = %d\n", pCamCalData->Single2A.S2aAwb.rUnitGainu4G_low);
		pr_debug("[rCalGain.u4B] = %d\n", pCamCalData->Single2A.S2aAwb.rUnitGainu4B_low);
		pr_debug("[rFacGain.u4R] = %d\n", pCamCalData->Single2A.S2aAwb.rGoldGainu4R_low);
		pr_debug("[rFacGain.u4G] = %d\n", pCamCalData->Single2A.S2aAwb.rGoldGainu4G_low);
		pr_debug("[rFacGain.u4B] = %d\n", pCamCalData->Single2A.S2aAwb.rGoldGainu4B_low);
		pr_debug("======================AWB CAM_CAL==================\n");
		#endif
	}
	/* AF Calibration Data*/
	if (0x2 & AWBAFConfig) {
		// AF 50cm
		af_offset = 0x0098;
		read_data_size = read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
				af_offset, 2, (unsigned char *)&AF_50cm);
		if (read_data_size > 0)
			err = CAM_CAL_ERR_NO_ERR;
		else {
			pCamCalData->Single2A.S2aBitEn = CAM_CAL_NONE_BITEN;
			error_log("Read Failed\n");
			show_cmd_error_log(pCamCalData->Command);
		}

		// AF Inf
		af_offset = 0x0094;
		read_data_size = read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
				af_offset, 2, (unsigned char *)&AFInf);
		if (read_data_size > 0)
			err = CAM_CAL_ERR_NO_ERR;
		else {
			pCamCalData->Single2A.S2aBitEn = CAM_CAL_NONE_BITEN;
			error_log("Read Failed\n");
			show_cmd_error_log(pCamCalData->Command);
		}

		// AF Macro
		af_offset = 0x0092;
		read_data_size = read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
				af_offset, 2, (unsigned char *)&AFMacro);
		if (read_data_size > 0)
			err = CAM_CAL_ERR_NO_ERR;
		else {
			pCamCalData->Single2A.S2aBitEn = CAM_CAL_NONE_BITEN;
			error_log("Read Failed\n");
			show_cmd_error_log(pCamCalData->Command);
		}

		pCamCalData->Single2A.S2aAf[0] = AFInf;
		pCamCalData->Single2A.S2aAf[1] = AFMacro;
		pCamCalData->Single2A.S2aAf[2] = AF_50cm;
		pCamCalData->Single2A.S2aAF_t.AF_dac_code_bit_depth = 12;

		////Only AF Gathering <////
		// #ifdef DEBUG_CALIBRATION_LOAD
		printk("======================AF CAM_CAL==================\n");
		printk("cccccc[AFInf] = %d\n", AFInf);
		printk("cccccc[AFMacro] = %d\n", AFMacro);
		printk("cccccc[AF_50cm] = %d\n", AF_50cm);
		printk("======================AF CAM_CAL==================\n");
		// #endif
	}
	return err;
}

static unsigned int do_lens_id_konkautele(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData)
{
	return do_lens_id_base(pdata, start_addr, block_size, pGetSensorCalData);
}

static unsigned int do_pdaf_konkautele(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData)
{
	struct STRUCT_CAM_CAL_DATA_STRUCT *pCamCalData =
				(struct STRUCT_CAM_CAL_DATA_STRUCT *)pGetSensorCalData;

	int read_data_size;
	int read_data_size2;
	int err =  CamCalReturnErr[pCamCalData->Command];

	pCamCalData->PDAF.Size_of_PDAF = block_size;
	debug_log("PDAF start_addr =%x table_size=%d\n", start_addr, block_size);
	block_size = 0xA36;
	read_data_size = read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
			start_addr, block_size, (unsigned char *)&pCamCalData->PDAF.Data[0]);
	if (read_data_size <= 0) {
		return err;
	}
	/*patrial pd data*/
	start_addr = 0x3110;
	block_size = 0x5EA;
	read_data_size2 = read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
			start_addr, block_size, (unsigned char *)&pCamCalData->PDAF.Data[read_data_size]);
	if (read_data_size2 > 0) {
		err = CAM_CAL_ERR_NO_ERR;
	} else {
		return err;
	}
	debug_log("======================PDAF Data==================\n");
	debug_log("First five %x, %x, %x, %x, %x\n",
		pCamCalData->PDAF.Data[0],
		pCamCalData->PDAF.Data[1],
		pCamCalData->PDAF.Data[2],
		pCamCalData->PDAF.Data[3],
		pCamCalData->PDAF.Data[4]);
	debug_log("First five %x, %x, %x, %x, %x\n",
		pCamCalData->PDAF.Data[read_data_size],
		pCamCalData->PDAF.Data[read_data_size + 1],
		pCamCalData->PDAF.Data[read_data_size + 2],
		pCamCalData->PDAF.Data[read_data_size + 3],
		pCamCalData->PDAF.Data[read_data_size + 4]);
	debug_log("RETURN = 0x%x\n", err);
	debug_log("======================PDAF Data==================\n");
	return err;
}

static unsigned int layout_check_konkautele(struct EEPROM_DRV_FD_DATA *pdata, unsigned int sensorID,
		unsigned int *_cfg)
{
	struct STRUCT_CAM_CAL_CONFIG_STRUCT *cfg =
			(struct STRUCT_CAM_CAL_CONFIG_STRUCT *)_cfg;
	unsigned int header_offset = 0x00000000;
	unsigned int check_id = 0x00000000;
	unsigned int result = CAM_CAL_ERR_NO_DEVICE;
	int ret = -1;
	uint16_t chip_en = 0;
	struct i2c_client *client;
	unsigned short stdby[17] = {0x0100, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
								0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
								0x0000, 0x0000, 0x0000};
	client = pdata->pdrv->pi2c_client;
	if (client == NULL) {
		error_log("i2c_client == NULL\n");
		return 0;
	}
	client->addr = KONKAUTELE_AF_SLAVE_ID >> 1;

	if (cfg == NULL) {
		error_log("null STRUCT_CAM_CAL_CONFIG_STRUCT arg\n");
		return result;
	}

	header_offset = cfg->layout->header_addr;

	if (cfg->sensor_id == sensorID)
		debug_log("%s sensor_id matched\n", cfg->name);
	else {
		debug_log("%s sensor_id not matched\n", cfg->name);
		return result;
	}

	mutex_lock(&dw9786_mutex);
	printk("%s dw9786_mutex: %p\n", __func__, &dw9786_mutex);
	ret = adaptor_i2c_rd_u16(client, client->addr, DW9786_CHIP_EN, &chip_en);
	printk("%s DW9786_CHIP_EN: 0x%x, ret: %d\n", __func__, chip_en, ret);
	if(chip_en != 0x0001) {
		ret = adaptor_i2c_wr_u16(client, client->addr, DW9786_CHIP_EN, 0x0000);
		msleep(2);
		adaptor_i2c_wr_p8(client, client->addr, DW9786_CHIP_EN, (unsigned char *)stdby, 34);
		msleep(5);
		ret = adaptor_i2c_wr_u16(client, client->addr, 0xE004, 0x0001);
		msleep(20);
	}
	mutex_unlock(&dw9786_mutex);

	if (read_data_region(pdata, (u8 *)&check_id, header_offset, 4, cfg) != 4) {
		debug_log("header_id read failed\n");
		return result;
	}

	if (check_id == cfg->layout->header_id) {
		debug_log("header_id matched 0x%08x 0x%08x\n",
			check_id, cfg->layout->header_id);
		result = CAM_CAL_ERR_NO_ERR;
	} else
		debug_log("header_id not matched 0x%08x 0x%08x\n",
			check_id, cfg->layout->header_id);

	return result;
}
