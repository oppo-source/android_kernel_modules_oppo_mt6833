// SPDX-License-Identifier: GPL-2.0-only
/*
* Generic ADC thermal driver
*
* Copyright (C) 2016 NVIDIA CORPORATION. All rights reserved.
*
* Author: Laxman Dewangan <ldewangan@nvidia.com>
*/
#include <linux/iio/consumer.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/thermal.h>
#include <linux/delay.h>

#define VADC_PA_TIME_MIN_US			3000
#define VADC_PA_TIME_MAX_US			3100
struct gadc_thermal_info {
	struct device *dev;
	struct thermal_zone_device *tz_dev;
	struct iio_channel *channel;
	s32 *lookup_table;
	int nlookup_table;
};

static int gadc_thermal_adc_to_temp(struct gadc_thermal_info *gti, int val)
{

	int temp = 61*val;
	return temp;
}

static int gadc_thermal_get_temp(struct thermal_zone_device *tz, int *temp)
{
	struct gadc_thermal_info *gti = thermal_zone_device_priv(tz);
	int val;
	int ret;
	int maxval;

	ret = iio_read_channel_processed(gti->channel, &val);
	if (ret < 0) {
		dev_err(gti->dev, "IIO channel read failed %d\n", ret);
		return ret;
	}

	maxval = val;
	for (int i = 0; i < 20; i++) {
		//udelay(3000);
		usleep_range(VADC_PA_TIME_MIN_US, VADC_PA_TIME_MAX_US);
		iio_read_channel_processed(gti->channel, &val);
		if (val > maxval) {
			maxval = val;
		}
	}
	val = maxval;
	*temp = gadc_thermal_adc_to_temp(gti, val);

	return 0;
}

static const struct thermal_zone_device_ops gadc_thermal_ops = {
	.get_temp = gadc_thermal_get_temp,
};

static int gadc_thermal_probe(struct platform_device *pdev)
{
	struct gadc_thermal_info *gti;
	int ret;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "Only DT based supported\n");
		return -ENODEV;
	}

	gti = devm_kzalloc(&pdev->dev, sizeof(*gti), GFP_KERNEL);
	if (!gti)
		return -ENOMEM;

	gti->channel = devm_iio_channel_get(&pdev->dev, "sensor-pa");
	if (IS_ERR(gti->channel)) {
		ret = PTR_ERR(gti->channel);
		if (ret != -EPROBE_DEFER)
			dev_err(&pdev->dev, "IIO channel not found: %d\n", ret);
		return ret;
	}

	gti->dev = &pdev->dev;

	gti->tz_dev = devm_thermal_of_zone_register(&pdev->dev, 0, gti,
						    &gadc_thermal_ops);
	if (IS_ERR(gti->tz_dev)) {
		ret = PTR_ERR(gti->tz_dev);
		if (ret != -EPROBE_DEFER)
			dev_err(&pdev->dev,
				"Thermal zone sensor register failed: %d\n",
				ret);
		return ret;
	}

	return 0;
}

static const struct of_device_id of_adc_thermal_match[] = {
	{ .compatible = "thermal_pa_adc", },
	{},
};
MODULE_DEVICE_TABLE(of, of_adc_thermal_match);

static struct platform_driver gadc_thermal_driver = {
	.driver = {
		.name = "thermal_pa_adc",
		.of_match_table = of_adc_thermal_match,
	},
	.probe = gadc_thermal_probe,
};

module_platform_driver(gadc_thermal_driver);

MODULE_AUTHOR("Laxman Dewangan <ldewangan@nvidia.com>");
MODULE_DESCRIPTION("Generic ADC thermal driver using IIO framework with DT");
MODULE_LICENSE("GPL v2");
