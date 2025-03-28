/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 OPLUS Inc.
 */

#ifndef __OPLUS_CAM_CAL_LIST_H
#define __OPLUS_CAM_CAL_LIST_H

#include "oplus_kd_imgsensor.h"

#define MAX_EEPROM_SIZE_32K 0x8000
#define MAX_EEPROM_SIZE_16K 0x4000

struct stCAM_CAL_LIST_STRUCT g_oplusCamCalList[] = {
	{KONKAFRONT_SENSOR_ID, 0xA8, Common_read_region, MAX_EEPROM_SIZE_16K},
	{KONKAMAIN_SENSOR_ID, 0xA0, Common_read_region, MAX_EEPROM_SIZE_32K},
	{KONKATELE_SENSOR_ID, 0xA0, Common_read_region, MAX_EEPROM_SIZE_32K},
	{KONKAUTELE_SENSOR_ID, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{KONKAUWIDE_SENSOR_ID, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{YALAMAIN_SENSOR_ID, 0xA0, Common_read_region, MAX_EEPROM_SIZE_32K},
	{BRZAMAIN_SENSOR_ID, 0xA0, Common_read_region, MAX_EEPROM_SIZE_32K},
	{BRZAFRONT_SENSOR_ID, 0xA8, Common_read_region, MAX_EEPROM_SIZE_16K},
	{BRZAUWIDE_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_16K},
	{BRZBMAIN_SENSOR_ID, 0xA0, Common_read_region, MAX_EEPROM_SIZE_32K},
	{BRZBFRONT_SENSOR_ID, 0xA8, Common_read_region, MAX_EEPROM_SIZE_16K},
	{BRZBUWIDE_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_16K},
    /*  ADD before this line */
    {0, 0, 0}       /*end of list */
};

#endif        /* __OPLUS_CAM_CAL_LIST_H */