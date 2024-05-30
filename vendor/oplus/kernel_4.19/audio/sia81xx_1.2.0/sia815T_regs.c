/*
 * Copyright (C) 2018, SI-IN, Yun Shi (yun.shi@si-in.com).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define DEBUG
#define LOG_FLAG	"sia815T_regs"

#include <linux/delay.h>
#include <linux/regmap.h>
#include <linux/device.h>
#include "sia81xx_common.h"
#include "sia81xx_regmap.h"
#include "sia815T_regs.h"

#define SIA815T_WRITEABLE_REG_NUM			(20)

static const char sia815T_palyback_defaults[][SIA815T_WRITEABLE_REG_NUM] = {
	[SIA81XX_CHANNEL_L] = {
				0x7c,		//SIA815T_REG_SYSCTRL
				0x20,		//SIA815T_REG_ALGO_EN
				0xa8,		//SIA815T_REG_BST_CFG
				0xc9,		//SIA815T_REG_CLSD_CFG
				0x00,		//SIA815T_REG_ALGO_CFG1
				0x28,		//SIA815T_REG_ALGO_CFG2
				0x73,		//SIA815T_REG_ALGO_CFG3
				0x8a,		//SIA815T_REG_ALGO_CFG4
				0x0b,		//SIA815T_REG_ALGO_CFG5
				0xe4,		//SIA815T_REG_CLSD_OCPCFG
				0x00,		//0x0B
				0x00,		//0x0C
				0x00,		//0x0D
				0x00,		//0x0E
				0x00,		//0x0F
				0x00,		//0x10
				0x00,		//0x11
				0x00,		//0x12
				0x00,		//0x13
				0x80		//0x14
	},
	[SIA81XX_CHANNEL_R] = {
				0x7c,		//SIA815T_REG_SYSCTRL
				0x20,		//SIA815T_REG_ALGO_EN
				0xae,		//SIA815T_REG_BST_CFG
				0xc9,		//SIA815T_REG_CLSD_CFG
				0x00,		//SIA815T_REG_ALGO_CFG1
				0x28,		//SIA815T_REG_ALGO_CFG2
				0x73,		//SIA815T_REG_ALGO_CFG3
				0x8a,		//SIA815T_REG_ALGO_CFG4
				0x0b,		//SIA815T_REG_ALGO_CFG5
				0xe4,		//SIA815T_REG_CLSD_OCPCFG
				0x00,		//0x0B
				0x00,		//0x0C
				0x00,		//0x0D
				0x00,		//0x0E
				0x00,		//0x0F
				0x00,		//0x10
				0x00,		//0x11
				0x00,		//0x12
				0x00,		//0x13
				0x80		//0x14
	}
};

static const char sia815T_voice_defaults[][SIA815T_WRITEABLE_REG_NUM] = {
	[SIA81XX_CHANNEL_L] = {
				0x7d,		//SIA815T_REG_SYSCTRL
				0x00,		//SIA815T_REG_ALGO_EN
				0xa8,		//SIA815T_REG_BST_CFG
				0xc9,		//SIA815T_REG_CLSD_CFG
				0x00,		//SIA815T_REG_ALGO_CFG1
				0x28,		//SIA815T_REG_ALGO_CFG2
				0x73,		//SIA815T_REG_ALGO_CFG3
				0x8a,		//SIA815T_REG_ALGO_CFG4
				0x0b,		//SIA815T_REG_ALGO_CFG5
				0xe4,		//SIA815T_REG_CLSD_OCPCFG
				0x00,		//0x0B
				0x00,		//0x0C
				0x00,		//0x0D
				0x00,		//0x0E
				0x00,		//0x0F
				0x00,		//0x10
				0x00,		//0x11
				0x00,		//0x12
				0x00,		//0x13
				0x80		//0x14
	},
	[SIA81XX_CHANNEL_R] = {
				0x7d,		//SIA815T_REG_SYSCTRL
				0x00,		//SIA815T_REG_ALGO_EN
				0xae,		//SIA815T_REG_BST_CFG
				0xc9,		//SIA815T_REG_CLSD_CFG
				0x00,		//SIA815T_REG_ALGO_CFG1
				0x28,		//SIA815T_REG_ALGO_CFG2
				0x73,		//SIA815T_REG_ALGO_CFG3
				0x8a,		//SIA815T_REG_ALGO_CFG4
				0x0b,		//SIA815T_REG_ALGO_CFG5
				0xe4,		//SIA815T_REG_CLSD_OCPCFG
				0x00,		//0x0B
				0x00,		//0x0C
				0x00,		//0x0D
				0x00,		//0x0E
				0x00,		//0x0F
				0x00,		//0x10
				0x00,		//0x11
				0x00,		//0x12
				0x00,		//0x13
				0x80		//0x14
	}
};

static const char sia815T_receiver_defaults[][SIA815T_WRITEABLE_REG_NUM] = {
	[SIA81XX_CHANNEL_L] = {
				0x6b,		//SIA815T_REG_SYSCTRL
				0x00,		//SIA815T_REG_ALGO_EN
				0x08,		//SIA815T_REG_BST_CFG
				0xc9,		//SIA815T_REG_CLSD_CFG
				0x00,		//SIA815T_REG_ALGO_CFG1
				0x28,		//SIA815T_REG_ALGO_CFG2
				0x45,		//SIA815T_REG_ALGO_CFG3
				0x8a,		//SIA815T_REG_ALGO_CFG4
				0x0d,		//SIA815T_REG_ALGO_CFG5
				0xe4,		//SIA815T_REG_CLSD_OCPCFG
				0x00,		//0x0B
				0x00,		//0x0C
				0x00,		//0x0D
				0x00,		//0x0E
				0x00,		//0x0F
				0x00,		//0x10
				0x00,		//0x11
				0x00,		//0x12
				0x00,		//0x13
				0x80		//0x14
	},
	[SIA81XX_CHANNEL_R] = {
				0x7d,		//SIA815T_REG_SYSCTRL
				0x20,		//SIA815T_REG_ALGO_EN
				0x0c,		//SIA815T_REG_BST_CFG
				0xc9,		//SIA815T_REG_CLSD_CFG
				0x00,		//SIA815T_REG_ALGO_CFG1
				0x28,		//SIA815T_REG_ALGO_CFG2
				0x73,		//SIA815T_REG_ALGO_CFG3
				0x8a,		//SIA815T_REG_ALGO_CFG4
				0x0d,		//SIA815T_REG_ALGO_CFG5
				0xe4,		//SIA815T_REG_CLSD_OCPCFG
				0x00,		//0x0B
				0x00,		//0x0C
				0x00,		//0x0D
				0x00,		//0x0E
				0x00,		//0x0F
				0x00,		//0x10
				0x00,		//0x11
				0x00,		//0x12
				0x00,		//0x13
				0x80		//0x14
	}
};

static const char sia815T_factory_defaults[][SIA815T_WRITEABLE_REG_NUM] = {
	[SIA81XX_CHANNEL_L] = {
				0x7d,		//SIA815T_REG_SYSCTRL
				0x20,		//SIA815T_REG_ALGO_EN
				0xa8,		//SIA815T_REG_BST_CFG
				0xc9,		//SIA815T_REG_CLSD_CFG
				0x00,		//SIA815T_REG_ALGO_CFG1
				0x28,		//SIA815T_REG_ALGO_CFG2
				0x73,		//SIA815T_REG_ALGO_CFG3
				0x8a,		//SIA815T_REG_ALGO_CFG4
				0x0b,		//SIA815T_REG_ALGO_CFG5
				0xe4,		//SIA815T_REG_CLSD_OCPCFG
				0x00,		//0x0B
				0x00,		//0x0C
				0x00,		//0x0D
				0x00,		//0x0E
				0x00,		//0x0F
				0x00,		//0x10
				0x00,		//0x11
				0x00,		//0x12
				0x00,		//0x13
				0x80		//0x14
	},
	[SIA81XX_CHANNEL_R] = {
				0x7d,		//SIA815T_REG_SYSCTRL
				0x20,		//SIA815T_REG_ALGO_EN
				0xae,		//SIA815T_REG_BST_CFG
				0xc9,		//SIA815T_REG_CLSD_CFG
				0x00,		//SIA815T_REG_ALGO_CFG1
				0x28,		//SIA815T_REG_ALGO_CFG2
				0x73,		//SIA815T_REG_ALGO_CFG3
				0x8a,		//SIA815T_REG_ALGO_CFG4
				0x0b,		//SIA815T_REG_ALGO_CFG5
				0xe4,		//SIA815T_REG_CLSD_OCPCFG
				0x00,		//0x0B
				0x00,		//0x0C
				0x00,		//0x0D
				0x00,		//0x0E
				0x00,		//0x0F
				0x00,		//0x10
				0x00,		//0x11
				0x00,		//0x12
				0x00,		//0x13
				0x80		//0x14
	}
};

const struct sia81xx_reg_default_val sia815T_reg_default_val = {
	.chip_type = CHIP_TYPE_SIA815T,
	.offset = SIA815T_REG_SYSCTRL,
	.reg_defaults[AUDIO_SCENE_PLAYBACK] = {
		.vals = (char *)sia815T_palyback_defaults,
		.num = ARRAY_SIZE(sia815T_palyback_defaults[0])
	},
	.reg_defaults[AUDIO_SCENE_VOICE] = {
		.vals = (char *)sia815T_voice_defaults,
		.num = ARRAY_SIZE(sia815T_voice_defaults[0])
	},
	.reg_defaults[AUDIO_SCENE_RECEIVER] = {
		.vals = (char *)sia815T_receiver_defaults,
		.num = ARRAY_SIZE(sia815T_receiver_defaults[0])
	},
	.reg_defaults[AUDIO_SCENE_FACTORY] = {
		.vals = (char *)sia815T_factory_defaults,
		.num = ARRAY_SIZE(sia815T_factory_defaults[0])
	}
};

static const SIA_CHIP_ID_RANGE chip_id_ranges[] = {
	{0x6A, 0x6D}
};

static bool sia815T_writeable_register(
	struct device *dev,
	unsigned int reg)
{
	switch (reg) {
		case SIA815T_REG_CHIP_ID ... 0x22 :
			return true;
		default :
			break;
	}

	return false;
}

static bool sia815T_readable_register(
	struct device *dev,
	unsigned int reg)
{
	switch (reg) {
		case SIA815T_REG_CHIP_ID ... 0x22 :
		case 0x41 :
			return true;
		default :
			break;
	}

	return false;
}

static bool sia815T_volatile_register(
	struct device *dev,
	unsigned int reg)
{
	return true;
}

const struct regmap_config sia815T_regmap_config = {
	.name = "sia815T",
	.reg_bits = 8,
	.val_bits = 8,
	.reg_stride = 0,
	.pad_bits = 0,
	.cache_type = REGCACHE_NONE,
	.reg_defaults = NULL,
	.num_reg_defaults = 0,
	.writeable_reg = sia815T_writeable_register,
	.readable_reg = sia815T_readable_register,
	.volatile_reg = sia815T_volatile_register,
	.reg_format_endian = REGMAP_ENDIAN_NATIVE,
	.val_format_endian = REGMAP_ENDIAN_NATIVE,
};

static int sia815T_check_chip_id(
	struct regmap *regmap)
{
	char val = 0;
	int i = 0;

	if (0 != sia81xx_regmap_read(regmap, SIA815T_REG_CHIP_ID, 1, &val))
		return -1;

	for (i = 0; i < ARRAY_SIZE(chip_id_ranges); i++) {
		if (val >= chip_id_ranges[i].begin && val <= chip_id_ranges[i].end)
			return 0;
	}

	return -1;
}

static void sia815T_chip_on(
	struct regmap *regmap,
	unsigned int scene,
	unsigned int channel_num)
{
	char val = 0;

	if (AUDIO_SCENE_NUM <= scene || SIA81XX_CHANNEL_NUM <= channel_num)
		return;

	if (0 != sia81xx_regmap_read(regmap, SIA815T_REG_ALGO_CFG1, 1, &val))
		return;

	val |= 0x01;
	if (0 != sia81xx_regmap_write(regmap, SIA815T_REG_ALGO_CFG1, 1, &val))
		return;

	if (0 != sia81xx_regmap_write(regmap, SIA815T_REG_BST_CFG, 2,
		&(sia815T_reg_default_val.reg_defaults[scene].vals +
			channel_num * sia815T_reg_default_val.reg_defaults[scene].num)
			[SIA815T_REG_BST_CFG - sia815T_reg_default_val.offset]))
		return;

	sia81xx_regmap_write(regmap,
		SIA815T_REG_ALGO_CFG2,
		sia815T_reg_default_val.reg_defaults[scene].num -
			(SIA815T_REG_ALGO_CFG2 - sia815T_reg_default_val.offset),
		&(sia815T_reg_default_val.reg_defaults[scene].vals +
			channel_num * sia815T_reg_default_val.reg_defaults[scene].num)
			[SIA815T_REG_ALGO_CFG2 - sia815T_reg_default_val.offset]);
}

static void sia815T_chip_off(
	struct regmap *regmap)
{
	char val = 0;

	if (0 != sia81xx_regmap_write(regmap, SIA815T_REG_ALGO_CFG1, 1, &val))
		return;

	val = 0x41;
	if (0 != sia81xx_regmap_write(regmap, SIA815T_REG_SYSCTRL, 1, &val))
		return;

	val = 0x20;
	if (0 != sia81xx_regmap_write(regmap, SIA815T_REG_ALGO_EN, 1, &val))
		return;

	mdelay(1);	// wait chip power off
}

static void sia815T_check_trimming(
	struct regmap *regmap)
{
	static const uint32_t reg_num =
		SIA815T_REG_TRIMMING_END - SIA815T_REG_TRIMMING_BEGIN + 1;
	static const char defaults[reg_num] = {0x76, 0x66, 0x70};
	uint8_t vals[reg_num] = {0};
	uint8_t crc = 0;
	uint8_t i, tmp;

	/* wait reading trimming data to reg */
	mdelay(1);

	if (0 != sia81xx_regmap_read(regmap,
		SIA815T_REG_TRIMMING_BEGIN, reg_num, (char *)vals))
		return ;

	crc = vals[reg_num - 1] & 0x0F;
	vals[reg_num - 1] &= 0xF0;

	for (i = 0; i < reg_num / 2; i++) {
		tmp = vals[i];
		vals[i] = vals[reg_num - i - 1];
		vals[reg_num - i - 1] = tmp;
	}

	if (crc != crc4_itu(vals, reg_num)) {
		pr_warn("[ warn][%s] %s: trimming failed !! \r\n",
			LOG_FLAG, __func__);

		if (0 != sia81xx_regmap_read(regmap, SIA815T_REG_ALGO_CFG2, 1, (char *)vals))
			return;

		*vals |= 0x02;
		if (0 != sia81xx_regmap_write(regmap, SIA815T_REG_ALGO_CFG2, 1, (char *)vals))
			return;

		sia81xx_regmap_write(regmap,
			SIA815T_REG_TRIMMING_BEGIN, reg_num, defaults);
	}
}

static bool sia815T_get_chip_en(
	struct regmap *regmap)
{
	char val = 0;

	if (0 != sia81xx_regmap_read(regmap, SIA815T_REG_ALGO_CFG1, 1, &val))
		return false;

	if (val & 0x01)
		return true;

	return false;
}

const struct sia81xx_opt_if sia815T_opt_if = {
	.check_chip_id = sia815T_check_chip_id,
	.set_xfilter = NULL,
	.chip_on = sia815T_chip_on,
	.chip_off = sia815T_chip_off,
	.get_chip_en = sia815T_get_chip_en,
	.set_pvdd_limit = NULL,
	.check_trimming = sia815T_check_trimming,
};

