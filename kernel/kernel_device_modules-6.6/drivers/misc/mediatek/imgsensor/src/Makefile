# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.
ifeq ($(subst ",,$(CONFIG_MTK_LEGACY_PLATFORM)), mt6768)
MTK_PLATFORM := isp4_t
endif

subdir-ccflags-y += \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/ \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/ccu/inc \
	-I$(DEVICE_MODULES_PATH)/drivers/i2c/busses/ \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/ccu/inc \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/imgsensor/inc \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cam_cal/inc \
	-DDEBUG \

ifeq ($(CONFIG_MACH_CAMERA_MT6877),y)
MTK_PLATFORM := mt6853
endif

ifeq ($(CONFIG_MACH_CAMERA_MT6833),y)
MTK_PLATFORM := mt6853
endif

MTK_DRIVER_PATH  := $(DEVICE_MODULES_PATH)/drivers/misc/mediatek
subdir-ccflags-y += -I$(MTK_DRIVER_PATH)/camera/$(MTK_PLATFORM) \
             -I$(MTK_DRIVER_PATH)/smi \
             -I$(MTK_DRIVER_PATH)/include/mt-plat/ \
             -I$(MTK_DRIVER_PATH)/imgsensor/src/$(MTK_PLATFORM)

#ifdef OPLUS_FEATURE_CAMERA_COMMON
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/imgsensor/src/OplusImgsensor
#endif

obj-$(CONFIG_MTK_IMGSENSOR_ISP3_Z) += isp3_z/
obj-$(CONFIG_MTK_IMGSENSOR_ISP3_M) += isp3_m/
obj-$(CONFIG_MTK_IMGSENSOR_ISP4_T) += isp4_t/
#obj-$(CONFIG_MTK_IMGSENSOR_ISP4_T) += mt6768/
obj-$(CONFIG_MTK_IMGSENSOR_ISP4_C) += isp4_c/
obj-$(CONFIG_MTK_IMGSENSOR) += isp6s/
obj-$(CONFIG_MTK_IMGSENSOR_ISP6S_MON) += mt6853/
obj-$(CONFIG_MTK_IMGSENSOR_ISP6S_MOU) += isp6s_mou/
obj-$(CONFIG_MTK_IMGSENSOR_ISP6S_LAG) += isp6s_lag/

$(info imgsensor CONFIG_MTK_IMGSENSOR_ISP3_Z = $(CONFIG_MTK_IMGSENSOR_ISP3_Z))
$(info imgsensor CONFIG_MTK_IMGSENSOR_ISP3_M = $(CONFIG_MTK_IMGSENSOR_ISP3_M))
$(info imgsensor CONFIG_MTK_IMGSENSOR_ISP4_T = $(CONFIG_MTK_IMGSENSOR_ISP4_T))
$(info imgsensor CONFIG_MTK_IMGSENSOR_ISP4_C = $(CONFIG_MTK_IMGSENSOR_ISP4_C))
$(info imgsensor CONFIG_MTK_IMGSENSOR = $(CONFIG_MTK_IMGSENSOR))
$(info imgsensor CONFIG_MTK_IMGSENSOR_ISP6S_MON = $(CONFIG_MTK_IMGSENSOR_ISP6S_MON))
$(info imgsensor CONFIG_MTK_IMGSENSOR_ISP6S_MOU = $(CONFIG_MTK_IMGSENSOR_ISP6S_MOU))
$(info imgsensor CONFIG_MTK_IMGSENSOR_ISP6S_LAG = $(CONFIG_MTK_IMGSENSOR_ISP6S_LAG))
