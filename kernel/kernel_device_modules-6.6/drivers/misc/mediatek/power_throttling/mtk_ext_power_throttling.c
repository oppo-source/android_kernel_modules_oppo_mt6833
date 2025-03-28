// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Samuel Hsieh <samuel.hsieh@mediatek.com>
 */
#include <linux/of.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include "mtk_ext_power_throttling.h"
#include "mtk_low_battery_throttling.h"
#include "mtk_battery_oc_throttling.h"
#include "mtk_bp_thl.h"
#include "mtk_peak_power_budget.h"
#include "mtk_cpu_power_throttling.h"

#define PPB_OFF 2
#define PPB_ON 12
#define USER_ENABLE 1
#define USER_DISABLE 0
#define OC_ENABLE 12
#define OC_DISABLE 1
#define BP_LEVEL_0 0
#define BP_LEVEL_1 1


static int ext_level;



/*****************************************************************************
 * low battery protect UT
 ******************************************************************************/
static ssize_t ext_power_throttle_ut_show(
		struct device *dev, struct device_attribute *attr,
		char *buf)
{
	dev_info(dev, "ext_level=%d\n", ext_level);
	return sprintf(buf, "%u\n", ext_level);
}

static ssize_t ext_power_throttle_ut_store(
		struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	unsigned int val = 0;

	if (sscanf(buf, "%u\n", &val) != 1) {
		dev_info(dev, "parameter number not correct\n");
		return -EINVAL;
	}

	ext_level = val;
	if (ext_level == 0) {
		dev_info(dev, "Resume all throttle features:%d\n", ext_level);
		lbat_set_user_enable(USER_ENABLE);
		bat_oc_set_ppb_mode(OC_ENABLE);
		cpu_ext_throttle(BP_LEVEL_0);
		bp_set_user_enable(USER_ENABLE);
		ppb_set_mode(PPB_ON);
	} else {
		dev_info(dev, "Disable all throttle features:%d\n", ext_level);
		lbat_set_user_enable(USER_DISABLE);
		bat_oc_set_ppb_mode(OC_DISABLE);
		bp_set_user_enable(USER_DISABLE);
		cpu_ext_throttle(BP_LEVEL_1);
		ppb_set_mode(PPB_OFF);
	}

	dev_info(dev, "ext_level: %d\n", ext_level);

	return size;
}
static DEVICE_ATTR_RW(ext_power_throttle_ut);

static int mtk_ext_power_throttling_probe(struct platform_device *pdev)
{
	int ret;

	ret = device_create_file(&(pdev->dev), &dev_attr_ext_power_throttle_ut);
	if (ret) {
		dev_notice(&pdev->dev, "create file error ret=%d\n", ret);
		return ret;
	}
	return 0;
}

static const struct of_device_id ext_power_throttling_of_match[] = {
	{ .compatible = "mediatek,ext-power-throttling", },
	{},
};

static int mtk_ext_power_throttling_remove(struct platform_device *pdev)
{
	return 0;
}

MODULE_DEVICE_TABLE(of, ext_power_throttling_of_match);
static struct platform_driver ext_power_throttling_driver = {
	.probe = mtk_ext_power_throttling_probe,
	.remove = mtk_ext_power_throttling_remove,
	.driver = {
		.name = "mtk-ext_power_throttling",
		.of_match_table = ext_power_throttling_of_match,
	},
};
module_platform_driver(ext_power_throttling_driver);
MODULE_AUTHOR("Victor Lin");
MODULE_DESCRIPTION("MTK ext power throttling driver");
MODULE_LICENSE("GPL");
