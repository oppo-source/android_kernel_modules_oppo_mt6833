# SPDX-License-Identifier: GPL-2.0-only
# Copyright (C) 2018-2020 Oplus. All rights reserved.
obj-$(CONFIG_OPLUS_FEATURE_PHOENIX) += op_bootprof.o
obj-$(CONFIG_OPLUS_FEATURE_PHOENIX) += phoenix_base.o
obj-$(CONFIG_OPLUS_FEATURE_PHOENIX) += phoenix_dump.o
obj-$(CONFIG_OPLUS_FEATURE_PHOENIX) += phoenix_watchdog.o
#ifdef OPLUS_BUG_STABILITY
ifeq ($(MTK_PLATFORM),mt6779)
###ODEX time out for first time,extend time
CFLAGS_KERNEL += -DPHOENIX_LONG_TIME
CFLAGS_MODULE += -DPHOENIX_LONG_TIME
endif
#endif OPLUS_BUG_STABILITY
