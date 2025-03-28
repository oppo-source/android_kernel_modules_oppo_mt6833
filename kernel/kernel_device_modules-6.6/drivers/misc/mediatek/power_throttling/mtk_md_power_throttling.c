// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Samuel Hsieh <samuel.hsieh@mediatek.com>
 */
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include "mtk_ccci_common.h"
#include "mtk_md_power_throttling.h"
#include "mtk_battery_oc_throttling.h"
#include "mtk_low_battery_throttling.h"
#include "mtk_bp_thl.h"
#include "pmic_lbat_service.h"

#define MAX_INT 0x7FFF
#define MD_TX_REDUCE 6

struct md_pt_priv {
	char max_lv_name[32];
	char limit_name[32];
	u32 max_thl_lv;
	u32 cur_thl_lv;
	int *thl_lv;
	u32 max_src_lv;
	u32 cur_src_lv;
	u32 *src_thd;
	u32 max_temp_stage;
	u32 cur_temp_stage;
	int *temp_thd;
	u32 *reduce_tx;
	struct work_struct psy_work;
	struct power_supply *psy;
};

static struct md_pt_priv md_pt_info[POWER_THROTTLING_TYPE_MAX] = {
	[LBAT_POWER_THROTTLING] = {
		.max_lv_name = "lbat-max-level",
		.limit_name = "lbat-reduce-tx-lv",
		.max_thl_lv = LOW_BATTERY_LEVEL_NUM - 1,
		.max_src_lv = LOW_BATTERY_LEVEL_NUM - 1,
	},
	[OC_POWER_THROTTLING] = {
		.max_lv_name = "oc-max-level",
		.limit_name = "oc-reduce-tx-lv",
		.max_thl_lv = BATTERY_OC_LEVEL_NUM - 1,
		.max_src_lv = BATTERY_OC_LEVEL_NUM - 1,
	}
};

static struct notifier_block md_pt_nb;
static DEFINE_MUTEX(md_pt_lock);

#if IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING)
static void md_pt_low_battery_cb(enum LOW_BATTERY_LEVEL_TAG level, void *data)
{
	unsigned int md_throttle_cmd;
	int ret, intensity;
	if (level > md_pt_info[LBAT_POWER_THROTTLING].max_src_lv)
		return;

	if (level != LOW_BATTERY_LEVEL_0)
		intensity = md_pt_info[LBAT_POWER_THROTTLING].reduce_tx[level-1];
	else
		intensity = 0;
	md_throttle_cmd = TMC_CTRL_CMD_TX_POWER | level << 8 |
		PT_LOW_BATTERY_VOLTAGE << 16 | intensity << 24;
	ret = exec_ccci_kern_func(ID_THROTTLING_CFG,
		(char *)&md_throttle_cmd, 4);

	pr_notice("%s: send cmd to CCCI ret=%d, cmd=0x%x\n", __func__, ret,
					md_throttle_cmd);

	if (ret)
		pr_notice("%s: error, ret=%d, cmd=0x%x l=%d\n", __func__, ret,
			md_throttle_cmd, level);
}

static void lbat_thl_with_temp(int source, u32 src_lv, u32 temp_stage)
{
	int thl_lv, thl_lv_idx;
	struct md_pt_priv *priv;

	priv = &md_pt_info[LBAT_POWER_THROTTLING];
	thl_lv_idx = temp_stage * (priv->max_src_lv + 1) + src_lv;
	thl_lv = priv->thl_lv[thl_lv_idx];
	if (thl_lv != priv->cur_thl_lv) {
		priv->cur_thl_lv = thl_lv;
		md_pt_low_battery_cb(priv->cur_thl_lv, NULL);
	}
}

static void md_lbat_dedicate_callback(unsigned int thd)
{
	int i = 0, lv = -1;
	struct md_pt_priv *priv;

	priv = &md_pt_info[LBAT_POWER_THROTTLING];

	for (i = 0; i <= priv->max_src_lv; i++) {
		if (thd == priv->src_thd[i]) {
			lv = i;
			break;
		}
	}

	mutex_lock(&md_pt_lock);
	if (lv >= 0 && priv->cur_src_lv != lv) {
		priv->cur_src_lv = lv;
		lbat_thl_with_temp(0, priv->cur_src_lv, priv->cur_temp_stage);
	}
	mutex_unlock(&md_pt_lock);
}

#endif

#if IS_ENABLED(CONFIG_MTK_BATTERY_PERCENT_THROTTLING)
static void md_bp_cb(enum BATTERY_PERCENT_LEVEL_TAG bp_level)
{
	unsigned int md_throttle_cmd;
	int ret, val = 0;

	if (bp_level == 0)
		val = TMC_CTRL_LOW_POWER_RECHARGE_BATTERY_EVENT;
	else
		val = TMC_CTRL_LOW_POWER_LOW_BATTERY_EVENT;

	md_throttle_cmd = TMC_CTRL_CMD_LOW_POWER_IND | val << 8;

	ret = exec_ccci_kern_func(ID_THROTTLING_CFG, (char *)&md_throttle_cmd, 4);

	pr_notice("%s: send cmd to CCCI ret=%d, cmd=0x%x\n", __func__, ret, md_throttle_cmd);
}
#endif

#if IS_ENABLED(CONFIG_MTK_BATTERY_OC_POWER_THROTTLING)
static void md_pt_over_current_cb(enum BATTERY_OC_LEVEL_TAG level, void *data)
{
	unsigned int md_throttle_cmd;
	int ret, intensity;
	if (level > md_pt_info[OC_POWER_THROTTLING].max_src_lv)
		return;
	if (level < BATTERY_OC_LEVEL_NUM) {
		if (level != BATTERY_OC_LEVEL_0)
			intensity = md_pt_info[OC_POWER_THROTTLING].reduce_tx[level-1];
		else
			intensity = 0;
		md_throttle_cmd = TMC_CTRL_CMD_TX_POWER | level << 8 | PT_OVER_CURRENT << 16 |
			intensity << 24;
		ret = exec_ccci_kern_func(ID_THROTTLING_CFG,
			(char *)&md_throttle_cmd, 4);

		pr_notice("%s: send cmd to CCCI ret=%d, cmd=0x%x\n", __func__, ret,
						md_throttle_cmd);

		if (ret)
			pr_notice("%s: error, ret=%d, cmd=0x%x l=%d\n", __func__, ret,
				md_throttle_cmd, level);
	}
}
#endif

static void md_pt_psy_handler(struct work_struct *work)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret, temp, temp_thd, temp_stage;
	static int last_temp = MAX_INT;
	struct md_pt_priv *priv;
	bool loop;

	priv = &md_pt_info[LBAT_POWER_THROTTLING];

	if (!priv->psy)
		return;

	psy = priv->psy;

	if (strcmp(psy->desc->name, "battery") != 0)
		return;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_TEMP, &val);
	if (ret)
		return;

	temp = val.intval / 10;
	temp_stage = priv->cur_temp_stage;

	do {
		loop = false;
		if (temp < last_temp) {
			if (temp_stage < priv->max_temp_stage) {
				temp_thd = priv->temp_thd[temp_stage];
				if (temp < temp_thd) {
					temp_stage++;
					loop = true;
				}
			}
		} else if (temp > last_temp) {
			if (temp_stage > 0) {
				temp_thd = priv->temp_thd[temp_stage-1];
				if (temp >= temp_thd) {
					temp_stage--;
					loop = true;
				}
			}
		}
	} while (loop);

	last_temp = temp;

	mutex_lock(&md_pt_lock);
	if (temp_stage <= priv->max_temp_stage && temp_stage != priv->cur_temp_stage) {
		priv->cur_temp_stage = temp_stage;
		lbat_thl_with_temp(1, priv->cur_src_lv, priv->cur_temp_stage);
	}
	mutex_unlock(&md_pt_lock);
}

static int md_pt_psy_event(struct notifier_block *nb, unsigned long event, void *v)
{
	struct md_pt_priv *priv;
	priv = &md_pt_info[LBAT_POWER_THROTTLING];

	priv->psy = v;
	schedule_work(&priv->psy_work);
	return NOTIFY_DONE;
}

static int __used md_limit_default_setting(struct device *dev, enum md_pt_type type)
{
	struct device_node *np = dev->of_node;
	int i, max_lv, ret;
	struct md_pt_priv *priv;

	priv = &md_pt_info[type];
	if (type == LBAT_POWER_THROTTLING)
		max_lv = LOW_BATTERY_LEVEL_NUM - 1;
	else if (type == OC_POWER_THROTTLING)
		max_lv = 1;
	else
		max_lv = 0;
	priv->max_src_lv = max_lv;
	priv->max_thl_lv = max_lv;
	if (!priv->max_src_lv)
		return 0;

	priv->reduce_tx = kcalloc(priv->max_thl_lv, sizeof(u32), GFP_KERNEL);
        if (!priv->reduce_tx) {
                priv->max_thl_lv = 0;
                return -ENOMEM;
        }
	priv->reduce_tx[0] = MD_TX_REDUCE;
	if (type == LBAT_POWER_THROTTLING) {
		ret = of_property_read_u32(np, "lbat_md_reduce_tx", &priv->reduce_tx[0]);
		if (ret < 0)
			pr_notice("%s: get lbat cpu limit fail %d\n", __func__, ret);
	} else if (type == OC_POWER_THROTTLING) {
		ret = of_property_read_u32(np, "oc_md_reduce_tx", &priv->reduce_tx[0]);
		if (ret < 0)
			pr_notice("%s: get oc cpu limit fail %d\n", __func__, ret);
	}
	for (i = 1; i < priv->max_thl_lv; i++)
		memcpy(&priv->reduce_tx[i], &priv->reduce_tx[0], sizeof(u32));

	return 0;
}

static int __used parse_md_limit_table(struct device *dev)
{
	struct device_node *np = dev->of_node;
	int i, j, num, thl_level_size, ret;
	struct md_pt_priv *priv;
	char buf[32];

	for (i = 0; i < POWER_THROTTLING_TYPE_MAX; i++) {
		priv = &md_pt_info[i];
		ret = of_property_read_u32(np, priv->max_lv_name, &num);
		if (ret < 0) {
			ret = md_limit_default_setting(dev, i);
                        if (ret)
                                return ret;
		} else if (num <= 0 || num > priv->max_thl_lv) {
			priv->max_thl_lv = 0;
			continue;
		}

		priv->max_thl_lv = num;
		priv->reduce_tx = kcalloc(priv->max_thl_lv, sizeof(u32), GFP_KERNEL);
		if (!priv->reduce_tx)
			return -ENOMEM;
		for (j = 0; j < priv->max_thl_lv; j++) {
			memset(buf, 0, sizeof(buf));
			ret = snprintf(buf, sizeof(buf), "%s%d", priv->limit_name, j+1);
			if (ret < 0)
				pr_notice("can't merge %s %d\n", priv->limit_name, j+1);
			ret = of_property_read_u32(np, buf, &priv->reduce_tx[j]);
			if (ret < 0)
				pr_notice("%s: get lbat cpu limit fail %d\n", __func__, ret);
		}

		if (i == LBAT_POWER_THROTTLING) {
			num = of_property_count_elems_of_size(np, "lbat-dedicate-volts", sizeof(u32));
			if (num < 2 || num > LOW_BATTERY_LEVEL_NUM) {
				pr_info("lbat-dedicate-volts error, num=%d\n", num);
				priv->max_src_lv = 0;
				continue;
			}
			priv->max_src_lv = num - 1;
			priv->src_thd = kcalloc(priv->max_src_lv + 1, sizeof(u32), GFP_KERNEL);
			if (!priv->src_thd) {
				priv->max_src_lv = 0;
				kfree(priv->reduce_tx);
				return -ENOMEM;
			}

			ret = of_property_read_u32_array(np, "lbat-dedicate-volts", priv->src_thd,
				priv->max_src_lv + 1);
			if (ret) {
				pr_notice("get lbat-dedicate-volts error %d\n", ret);
                                priv->max_src_lv = 0;
                                kfree(priv->reduce_tx);
				kfree(priv->src_thd);
				priv->src_thd = NULL;
				continue;
			}

			num = of_property_count_u32_elems(np, "lbat-dedicate-temperature");
			if (num > 0) {
				priv->max_temp_stage = num;
				priv->temp_thd = kcalloc(priv->max_temp_stage, sizeof(u32), GFP_KERNEL);
				if (!priv->temp_thd)
					return -ENOMEM;

				ret = of_property_read_u32_array(np, "lbat-dedicate-temperature", priv->temp_thd,
					priv->max_temp_stage);
				if (ret) {
					pr_notice("get lbat-dedicate-temperature fail %d, set max stage=0\n", ret);
					priv->max_temp_stage = 0;
				}

				thl_level_size = (priv->max_temp_stage + 1) * (priv->max_src_lv + 1);
				priv->thl_lv = kcalloc(thl_level_size, sizeof(u32), GFP_KERNEL);
				if (!priv->thl_lv)
					return -ENOMEM;

				num = of_property_count_u32_elems(np, "lbat-dedicate-throttle-level");
				if (num != thl_level_size ||
					of_property_read_u32_array(np, "lbat-dedicate-throttle-level", priv->thl_lv,
					thl_level_size)) {
					pr_info("%s:read throttle level error, num=%d thl_level_size=%d\n", __func__,
						num, thl_level_size);
					continue;
				}
			} else
				priv->max_temp_stage = 0;

		}
	}
	return 0;
}

static int mtk_md_power_throttling_probe(struct platform_device *pdev)
{
	int ret;
	struct md_pt_priv *priv;

	ret = parse_md_limit_table(&pdev->dev);
	if (ret != 0)
		return ret;

#if IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING)
	if (md_pt_info[LBAT_POWER_THROTTLING].max_thl_lv > 0) {
		priv = &md_pt_info[LBAT_POWER_THROTTLING];
		if (priv->src_thd) {
			INIT_WORK(&priv->psy_work, md_pt_psy_handler);
			lbat_user_register_ext("md pa dedicate throttle", priv->src_thd,
				priv->max_src_lv + 1, md_lbat_dedicate_callback);
			pr_info("%s: dedicate voltge throttle\n", __func__);
			if (priv->max_temp_stage > 0) {
				md_pt_nb.notifier_call = md_pt_psy_event;
				ret = power_supply_reg_notifier(&md_pt_nb);
				if (ret) {
					dev_notice(&pdev->dev, "[%s] register psy notify fail %d\n", __func__, ret);
					return ret;
				}
			}
		} else {
			register_low_battery_notify(&md_pt_low_battery_cb, LOW_BATTERY_PRIO_MD, NULL);
		}
	}
#endif
#if IS_ENABLED(CONFIG_MTK_BATTERY_OC_POWER_THROTTLING)
	if (md_pt_info[OC_POWER_THROTTLING].max_thl_lv > 0)
		register_battery_oc_notify(&md_pt_over_current_cb, BATTERY_OC_PRIO_MD, NULL);
#endif
#if IS_ENABLED(CONFIG_MTK_BATTERY_PERCENT_THROTTLING)
	register_bp_thl_md_notify(&md_bp_cb, BATTERY_PERCENT_PRIO_MD);
#endif
	return 0;
}
static int mtk_md_power_throttling_remove(struct platform_device *pdev)
{
	return 0;
}
static const struct of_device_id md_power_throttling_of_match[] = {
	{ .compatible = "mediatek,md-power-throttling", },
	{},
};
MODULE_DEVICE_TABLE(of, md_power_throttling_of_match);
static struct platform_driver md_power_throttling_driver = {
	.probe = mtk_md_power_throttling_probe,
	.remove = mtk_md_power_throttling_remove,
	.driver = {
		.name = "mtk-md_power_throttling",
		.of_match_table = md_power_throttling_of_match,
	},
};
module_platform_driver(md_power_throttling_driver);
MODULE_AUTHOR("Samuel Hsieh");
MODULE_DESCRIPTION("MTK modem power throttling driver");
MODULE_LICENSE("GPL");
