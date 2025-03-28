/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/*! \file   "oplus_rlm_txpwr_init.h"
 *    \brief
 */


/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */


#if CFG_SUPPORT_PWR_LIMIT_COUNTRY
#if (CFG_SUPPORT_WIFI_6G == 1)
/*Set to MAX_TX_PWR = 63dBm if larger than it*/
/* For 802.11ax 6G Low Power Indoor mode setting*/
struct COUNTRY_POWER_LIMIT_TABLE_DEFAULT
	g_rRlmPowerLimitDefault_23101[] = {
	{	{'G', '6'}
		, {63, 63, 34, 63, 63, 63, 63, 63, 63}
		, 0
	}
	,
	/*Default*/
	{	{0, 0}
		, {63, 63, 63, 63, 63, 63, 63, 63, 63}
		, 0
	}
};
#if (CFG_SUPPORT_WIFI_6G_PWR_MODE == 1)
/* For 802.11ax 6G Very Low Power mode setting */
struct COUNTRY_POWER_LIMIT_TABLE_DEFAULT
	g_rRlmPowerLimitDefault_VLP_23101[] = {
	{	{'G', '6'}
		, {63, 63, 34, 63, 63, 63, 63, 63, 63}
		, 0
	}
	,
	/*Default*/
	{	{0, 0}
		, {63, 63, 63, 63, 63, 63, 63, 63, 63}
		, 0
	}
};
/* For 802.11ax 6G Standard Power mode setting */
struct COUNTRY_POWER_LIMIT_TABLE_DEFAULT
	g_rRlmPowerLimitDefault_SP_23101[] = {
	{	{'G', '6'}
		, {63, 63, 34, 63, 63, 63, 63, 63, 63}
		, 0
	}
	,
	/*Default*/
	{	{0, 0}
		, {63, 63, 63, 63, 63, 63, 63, 63, 63}
		, 0
	}
};
#endif /* CFG_SUPPORT_WIFI_6G_PWR_MODE */
#else /* CFG_SUPPORT_WIFI_6G */
struct COUNTRY_POWER_LIMIT_TABLE_DEFAULT
	g_rRlmPowerLimitDefault_23101[] = {
	{	{'G', '6'}
		, {63, 63, 34, 63, 63}
		, 0
	}
	,
	/*Default*/
	{	{0, 0}
		, {63, 63, 63, 63, 63}
		, 0
	}
};
#endif
#endif

#if CFG_SUPPORT_PWR_LIMIT_COUNTRY
#if (CFG_SUPPORT_WIFI_6G == 1)
/*Set to MAX_TX_PWR = 63dBm if larger than it*/
/* For 802.11ax 6G Low Power Indoor mode setting*/
struct COUNTRY_POWER_LIMIT_TABLE_DEFAULT
	g_rRlmPowerLimitDefault_23105[] = {
	{	{'G', '6'}
		, {63, 63, 34, 63, 63, 63, 63, 63, 63}
		, 0
	}
	,
	/*Default*/
	{	{0, 0}
		, {63, 63, 63, 63, 63, 63, 63, 63, 63}
		, 0
	}
};
#if (CFG_SUPPORT_WIFI_6G_PWR_MODE == 1)
/* For 802.11ax 6G Very Low Power mode setting */
struct COUNTRY_POWER_LIMIT_TABLE_DEFAULT
	g_rRlmPowerLimitDefault_VLP_23105[] = {
	{	{'G', '6'}
		, {63, 63, 34, 63, 63, 63, 63, 63, 63}
		, 0
	}
	,
	/*Default*/
	{	{0, 0}
		, {63, 63, 63, 63, 63, 63, 63, 63, 63}
		, 0
	}
};
/* For 802.11ax 6G Standard Power mode setting */
struct COUNTRY_POWER_LIMIT_TABLE_DEFAULT
	g_rRlmPowerLimitDefault_SP_23105[] = {
	{	{'G', '6'}
		, {63, 63, 34, 63, 63, 63, 63, 63, 63}
		, 0
	}
	,
	/*Default*/
	{	{0, 0}
		, {63, 63, 63, 63, 63, 63, 63, 63, 63}
		, 0
	}
};
#endif /* CFG_SUPPORT_WIFI_6G_PWR_MODE */
#else /* CFG_SUPPORT_WIFI_6G */
struct COUNTRY_POWER_LIMIT_TABLE_DEFAULT
	g_rRlmPowerLimitDefault_23105[] = {
	{	{'G', '6'}
		, {63, 63, 34, 63, 63}
		, 0
	}
	,
	/*Default*/
	{	{0, 0}
		, {63, 63, 63, 63, 63}
		, 0
	}
};
#endif
#endif

#if CFG_SUPPORT_PWR_LIMIT_COUNTRY
#if (CFG_SUPPORT_WIFI_6G == 1)
/*Set to MAX_TX_PWR = 63dBm if larger than it*/
/* For 802.11ax 6G Low Power Indoor mode setting*/
struct COUNTRY_POWER_LIMIT_TABLE_DEFAULT
	g_rRlmPowerLimitDefault_23106[] = {
	{	{'G', '6'}
		, {63, 63, 34, 63, 63, 63, 63, 63, 63}
		, 0
	}
	,
	/*Default*/
	{	{0, 0}
		, {63, 63, 63, 63, 63, 63, 63, 63, 63}
		, 0
	}
};
#if (CFG_SUPPORT_WIFI_6G_PWR_MODE == 1)
/* For 802.11ax 6G Very Low Power mode setting */
struct COUNTRY_POWER_LIMIT_TABLE_DEFAULT
	g_rRlmPowerLimitDefault_VLP_23106[] = {
	{	{'G', '6'}
		, {63, 63, 34, 63, 63, 63, 63, 63, 63}
		, 0
	}
	,
	/*Default*/
	{	{0, 0}
		, {63, 63, 63, 63, 63, 63, 63, 63, 63}
		, 0
	}
};
/* For 802.11ax 6G Standard Power mode setting */
struct COUNTRY_POWER_LIMIT_TABLE_DEFAULT
	g_rRlmPowerLimitDefault_SP_23106[] = {
	{	{'G', '6'}
		, {63, 63, 34, 63, 63, 63, 63, 63, 63}
		, 0
	}
	,
	/*Default*/
	{	{0, 0}
		, {63, 63, 63, 63, 63, 63, 63, 63, 63}
		, 0
	}
};
#endif /* CFG_SUPPORT_WIFI_6G_PWR_MODE */
#else /* CFG_SUPPORT_WIFI_6G */
struct COUNTRY_POWER_LIMIT_TABLE_DEFAULT
	g_rRlmPowerLimitDefault_23106[] = {
	{	{'G', '6'}
		, {63, 63, 34, 63, 63}
		, 0
	}
	,
	/*Default*/
	{	{0, 0}
		, {63, 63, 63, 63, 63}
		, 0
	}
};
#endif
#endif

#if CFG_SUPPORT_PWR_LIMIT_COUNTRY
#if (CFG_SUPPORT_WIFI_6G == 1)
/*Set to MAX_TX_PWR = 63dBm if larger than it*/
/* For 802.11ax 6G Low Power Indoor mode setting*/
struct COUNTRY_POWER_LIMIT_TABLE_DEFAULT
	g_rRlmPowerLimitDefault_23205[] = {
	{	{'G', '0'}
		, {24, 30, 24, 23, 16, 25, 63, 63, 63}
		, 0
	}
	,
	{	{'G', '1'}
		, {35, 30, 30, 30, 36, 25, 63, 63, 63}
		, 0
	}
	,
	{	{'G', '2'}
		, {35, 32, 32, 32, 36, 25, 63, 63, 63}
		, 0
	}
	,
	{	{'G', '5'}
		, {22, 24, 16, 24, 63, 25, 63, 63, 63}
		, 0
	}
	,
	{	{'G', 'a'}
		, {35, 30, 30, 30, 36, 25, 63, 63, 63}
		, 0
	}
	,
	/*Default*/
	{	{0, 0}
		, {63, 63, 63, 63, 63, 63, 63, 63, 63}
		, 0
	}
};
#if (CFG_SUPPORT_WIFI_6G_PWR_MODE == 1)
/* For 802.11ax 6G Very Low Power mode setting */
struct COUNTRY_POWER_LIMIT_TABLE_DEFAULT
	g_rRlmPowerLimitDefault_VLP_23205[] = {
	{	{'G', '0'}
		, {24, 30, 24, 22, 16, 11, 63, 63, 63}
		, 0
	}
	,
	{	{'G', '1'}
		, {35, 30, 30, 30, 36, 8, 63, 63, 63}
		, 0
	}
	,
	{	{'G', '2'}
		, {35, 32, 32, 32, 36, 8, 63, 63, 63}
		, 0
	}
	,
	{	{'G', '5'}
		, {22, 24, 16, 24, 63, 11, 63, 63, 63}
		, 0
	}
	,
	{	{'G', 'a'}
		, {35, 30, 30, 30, 36, 11, 63, 63, 63}
		, 0
	}
	,
	/*Default*/
	{	{0, 0}
		, {63, 63, 63, 63, 63, 63, 63, 63, 63}
		, 0
	}
};
/* For 802.11ax 6G Standard Power mode setting */
struct COUNTRY_POWER_LIMIT_TABLE_DEFAULT
	g_rRlmPowerLimitDefault_SP_23205[] = {
	{	{'G', '0'}
		, {24, 30, 24, 22, 16, 25, 63, 63, 63}
		, 0
	}
	,
	{	{'G', '1'}
		, {35, 30, 30, 30, 36, 8, 63, 63, 63}
		, 0
	}
	,
	{	{'G', '2'}
		, {35, 32, 32, 32, 36, 8, 63, 63, 63}
		, 0
	}
	,
	{	{'G', '5'}
		, {22, 24, 16, 24, 63, 25, 63, 63, 63}
		, 0
	}
	,
	{	{'G', 'a'}
		, {35, 30, 30, 30, 36, 25, 63, 63, 63}
		, 0
	}
	,
	/*Default*/
	{	{0, 0}
		, {63, 63, 63, 63, 63, 63, 63, 63, 63}
		, 0
	}
};
#endif /* CFG_SUPPORT_WIFI_6G_PWR_MODE */
#else /* CFG_SUPPORT_WIFI_6G */
struct COUNTRY_POWER_LIMIT_TABLE_DEFAULT
	g_rRlmPowerLimitDefault_23205[] = {
	{	{'G', '0'}
		, {24, 30, 24, 22, 16}
		, 0
	}
	,
	{	{'G', '1'}
		, {35, 30, 30, 30, 36}
		, 0
	}
	,
	{	{'G', '2'}
		, {35, 32, 32, 32, 36}
		, 0
	}
	,
	{	{'G', '5'}
		, {22, 24, 16, 24, 63}
		, 0
	}
	,
	{	{'G', 'a'}
		, {35, 30, 30, 30, 36}
		, 0
	}
	,
	/*Default*/
	{	{0, 0}
		, {63, 63, 63, 63, 63}
		, 0
	}
};
#endif
#endif

#if CFG_SUPPORT_PWR_LIMIT_COUNTRY
#if (CFG_SUPPORT_WIFI_6G == 1)
/*Set to MAX_TX_PWR = 63dBm if larger than it*/
/* For 802.11ax 6G Low Power Indoor mode setting*/
struct COUNTRY_POWER_LIMIT_TABLE_DEFAULT
	g_rRlmPowerLimitDefault_23216[] = {
	{	{'G', '0'}
		, {24, 34, 26, 26, 17, 29, 63, 63, 63}
		, 0
	}
	,
	{	{'G', '1'}
		, {35, 26, 26, 30, 36, 6, 7, 10, 16}
		, 0
	}
	,
	{	{'G', '2'}
		, {35, 32, 32, 32, 36, 63, 63, 63, 63}
		, 0
	}
	,
	{	{'G', '3'}
		, {35, 20, 26, 30, 36, 6, 63, 63, 63}
		, 0
	}
	,
	{	{'G', 'a'}
		, {35, 26, 26, 30, 36, 29, 63, 63, 63}
		, 0
	}
	,
	/*Default*/
	{	{0, 0}
		, {63, 63, 63, 63, 63, 63, 63, 63, 63}
		, 0
	}
};
#if (CFG_SUPPORT_WIFI_6G_PWR_MODE == 1)
/* For 802.11ax 6G Very Low Power mode setting */
struct COUNTRY_POWER_LIMIT_TABLE_DEFAULT
	g_rRlmPowerLimitDefault_VLP_23216[] = {
	{	{'G', '0'}
		, {24, 34, 26, 26, 17, 12, 63, 63, 63}
		, 0
	}
	,
	{	{'G', '1'}
		, {35, 26, 26, 30, 36, 16, 7, 10, 16}
		, 0
	}
	,
	{	{'G', '2'}
		, {35, 32, 32, 32, 36, 63, 63, 63, 63}
		, 0
	}
	,
	{	{'G', '3'}
		, {35, 20, 26, 30, 36, 16, 63, 63, 63}
		, 0
	}
	,
	{	{'G', 'a'}
		, {35, 26, 26, 30, 36, 12, 63, 63, 63}
		, 0
	}
	,
	/*Default*/
	{	{0, 0}
		, {63, 63, 63, 63, 63, 63, 63, 63, 63}
		, 0
	}
};
/* For 802.11ax 6G Standard Power mode setting */
struct COUNTRY_POWER_LIMIT_TABLE_DEFAULT
	g_rRlmPowerLimitDefault_SP_23216[] = {
	{	{'G', '0'}
		, {24, 34, 26, 26, 17, 29, 63, 63, 63}
		, 0
	}
	,
	{	{'G', '1'}
		, {35, 26, 26, 30, 36, 6, 7, 10, 16}
		, 0
	}
	,
	{	{'G', '2'}
		, {35, 32, 32, 32, 36, 63, 63, 63, 63}
		, 0
	}
	,
	{	{'G', '3'}
		, {35, 20, 26, 30, 36, 6, 63, 63, 63}
		, 0
	}
	,
	{	{'G', 'a'}
		, {35, 26, 26, 30, 36, 29, 63, 63, 63}
		, 0
	}
	,

	/*Default*/
	{	{0, 0}
		, {63, 63, 63, 63, 63, 63, 63, 63, 63}
		, 0
	}
};
#endif /* CFG_SUPPORT_WIFI_6G_PWR_MODE */
#else /* CFG_SUPPORT_WIFI_6G */
struct COUNTRY_POWER_LIMIT_TABLE_DEFAULT
	g_rRlmPowerLimitDefault_23216[] = {
	{	{'G', '0'}
		, {24, 34, 26, 26, 17}
		, 0
	}
	,
	{	{'G', '1'}
		, {35, 26, 26, 30, 36}
		, 0
	}
	,
	{	{'G', '2'}
		, {35, 32, 32, 32, 36}
		, 0
	}
	,
	{	{'G', '3'}
		, {35, 20, 26, 30, 36}
		, 0
	}
	,
	{	{'G', 'a'}
		, {35, 26, 26, 30, 36}
		, 0
	}
	,
	/*Default*/
	{	{0, 0}
		, {63, 63, 63, 63, 63}
		, 0
	}
};
#endif
#endif