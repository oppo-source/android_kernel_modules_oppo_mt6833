# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.


#CONFIG_MTK_PLATFORM := mt6853
#CONFIG_ARCH_MTK_PROJECT := k6877v1_64


ifeq ($(CONFIG_MACH_CAMERA_MT6877),y)
$(info sanjay -------> !!!! $(CONFIG_MACH_CAMERA_MT6877) )
MTK_DERIVED_PLATFORM := mt6877
CONFIG_ARCH_MTK_PROJECT := k6877v1_64
CONFIG_MTK_PLATFORM := mt6853
subdir-ccflags-y += -DIMGSENSOR_MT6877
endif

ifeq ($(CONFIG_MACH_CAMERA_MT6833),y)
$(info sanjay -------> !!!! $(CONFIG_MACH_CAMERA_MT6833) )
MTK_DERIVED_PLATFORM := mt6833
CONFIG_ARCH_MTK_PROJECT := k6833v1_64
CONFIG_MTK_PLATFORM := mt6853
subdir-ccflags-y += -DMTK_PLATFORM_MT6833
endif

#ifdef OPLUS_FEATURE_CAMERA_COMMON
ifeq ($(CONFIG_MACH_CAMERA_MT6877),y)
subdir-ccflags-y += -DSENSOR_PLATFORM_NEXT_A
endif
#endif

MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
CONFIG_MTK_PLATFORM_SENSOR := $(subst ",,$(CONFIG_MTK_PLATFORM))

ARCH_MTK_PROJECT := $(subst ",,$(CONFIG_ARCH_MTK_PROJECT))
IMGSENSOR_DRIVER_PATH := $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/imgsensor/src
MTK_DRIVER_PATH  := $(DEVICE_MODULES_PATH)/drivers/misc/mediatek

COMMON_VERSION := v1_1
subdir-ccflags-y += -I$(IMGSENSOR_DRIVER_PATH)/common/$(COMMON_VERSION) \
	-I$(MTK_DRIVER_PATH)/mmdvfs/ \
	-I$(DEVICE_MODULES_PATH)/drivers/i2c/busses/

subdir-ccflags-y += -DNO_I2C_MTK -DNO_CLK_METER -DSENINF_USE_RPM -DDFS_CTRL_BY_OPP

obj-$(CONFIG_MTK_IMGSENSOR_ISP6S_MON) += imgsensor_isp6s_mon.o
imgsensor_isp6s_mon-objs :=
imgsensor_isp6s_mon-objs += ../common/$(COMMON_VERSION)/imgsensor.o
imgsensor_isp6s_mon-objs += ../common/$(COMMON_VERSION)/imgsensor_hw.o
imgsensor_isp6s_mon-objs += ../common/$(COMMON_VERSION)/imgsensor_i2c.o
imgsensor_isp6s_mon-objs += ../common/$(COMMON_VERSION)/imgsensor_legacy.o
imgsensor_isp6s_mon-objs += ../common/$(COMMON_VERSION)/imgsensor_proc.o
imgsensor_isp6s_mon-objs += ../common/$(COMMON_VERSION)/imgsensor_sensor_list.o
imgsensor_isp6s_mon-objs += ../common/$(COMMON_VERSION)/seninf_clk.o
imgsensor_isp6s_mon-objs += ../common/$(COMMON_VERSION)/seninf.o

#ifdef OPLUS_FEATURE_CAMERA_COMMON
imgsensor_isp6s_mon-objs += ../OplusImgsensor/imgsensor_eeprom.o
imgsensor_isp6s_mon-objs += ../OplusImgsensor/imgsensor_hwcfg_custom.o

imgsensor_isp6s_mon-objs += ../OplusImgsensor/imgsensor_hwcfg_mt6853.o

#endif

ifneq ($(ARCH_MTK_PROJECT), k6833v1_64_mt6317)
imgsensor_isp6s_mon-objs += ../common/$(COMMON_VERSION)/imgsensor_pwr_seq.o
endif

define add_subdrv
$(eval subdrv-rpath = $1)
$(info path: $(srctree)/$(src)/$1/Makefile)
$(eval include $(srctree)/$(src)/$1/Makefile)
endef

define FILTER_DRV
ifeq ($(wildcard $(IMGSENSOR_DRIVER_PATH)/$(CONFIG_MTK_PLATFORM_SENSOR)/camera_project/$(ARCH_MTK_PROJECT)/$(1)),)
ifeq ($(wildcard $(IMGSENSOR_DRIVER_PATH)/$(CONFIG_MTK_PLATFORM_SENSOR)/$(1)),)
ifeq ($(wildcard $(IMGSENSOR_DRIVER_PATH)/common/$(COMMON_VERSION)/$(1)),)
unknown_drv += $(1)
else
common_drv += ../common/$(COMMON_VERSION)/$(1)/
endif
else
platform_drv += $(1)/
endif
else
project_drv += camera_project/$(ARCH_MTK_PROJECT)/$(1)
endif
endef
$(foreach drv,$(subst $\",,$(CONFIG_CUSTOM_KERNEL_IMGSENSOR)),$(eval $(call FILTER_DRV,$(drv))))
CUSTOM_KERNEL_KNOWN_DRV := $(filter-out $(unknown_drv),$(subst $\",,$(CONFIG_CUSTOM_KERNEL_IMGSENSOR)))
CUSTOM_KERNEL_FLAGS := $(addprefix -D, $(shell echo $(CUSTOM_KERNEL_KNOWN_DRV) | tr a-z A-Z))
subdir-ccflags-y += $(CUSTOM_KERNEL_FLAGS)

ifneq ($(unknown_drv),)
$(info unknown_drv: $(unknown_drv))
endif

ifneq ($(common_drv),)
$(info common_drv : $(common_drv))
$(foreach drv,$(common_drv),$(eval $(call add_subdrv,../common/$(COMMON_VERSION)/$(drv))))
endif

ifneq ($(platform_drv),)
$(info platform_drv : $(platform_drv))
$(foreach drv,$(platform_drv),$(eval $(call add_subdrv,$(drv))))
endif

ifneq ($(project_drv),)
$(info project_drv : $(project_drv))
$(foreach drv, $(project_drv), $(eval $(call add_subdrv,$(drv))))
endif

ifeq (,$(wildcard $(IMGSENSOR_DRIVER_PATH)/$(CONFIG_MTK_PLATFORM_SENSOR)/camera_project/$(ARCH_MTK_PROJECT)/camera_hw))
ifeq (,$(wildcard $(IMGSENSOR_DRIVER_PATH)/$(CONFIG_MTK_PLATFORM_SENSOR)/camera_hw_$(MTK_DERIVED_PLATFORM)))
subdir-ccflags-y		+= -I$(IMGSENSOR_DRIVER_PATH)/$(CONFIG_MTK_PLATFORM_SENSOR)/camera_hw
$(eval $(call add_subdrv,camera_hw))
else
subdir-ccflags-y		+= -I$(IMGSENSOR_DRIVER_PATH)/$(CONFIG_MTK_PLATFORM_SENSOR)/camera_hw_$(MTK_DERIVED_PLATFORM)
subdir-ccflags-y		+= -I$(IMGSENSOR_DRIVER_PATH)/$(CONFIG_MTK_PLATFORM_SENSOR)/camera_hw
$(eval $(call add_subdrv,camera_hw_$(MTK_DERIVED_PLATFORM)))
endif
ifneq ($(project_drv),)
$(eval $(call add_subdrv,camera_project/$(ARCH_MTK_PROJECT)))
endif
else
$(eval $(call add_subdrv,camera_project/$(ARCH_MTK_PROJECT)))
subdir-ccflags-y		+= -I$(IMGSENSOR_DRIVER_PATH)/$(CONFIG_MTK_PLATFORM_SENSOR)/camera_project/$(ARCH_MTK_PROJECT)/camera_hw
subdir-ccflags-y		+= -I$(IMGSENSOR_DRIVER_PATH)/$(CONFIG_MTK_PLATFORM_SENSOR)/camera_hw_$(MTK_DERIVED_PLATFORM)
subdir-ccflags-y		+= -I$(IMGSENSOR_DRIVER_PATH)/$(CONFIG_MTK_PLATFORM_SENSOR)/camera_hw
endif

subdir-ccflags-y		+= -I$(IMGSENSOR_DRIVER_PATH)/common/$(COMMON_VERSION)/camera_hw

# Enable parallelism
subdir-ccflags-y += -DSENSOR_PARALLEISM

#ifdef OPLUS_FEATURE_CAMERA_COMMON
# Oplus platformflag
subdir-ccflags-y += -DSENSOR_PLATFORM_5G_B
#endif
ifneq (,$(wildcard $(IMGSENSOR_DRIVER_PATH)/$(CONFIG_MTK_PLATFORM_SENSOR)/seninf_$(MTK_DERIVED_PLATFORM)))
subdir-ccflags-y += -I$(IMGSENSOR_DRIVER_PATH)/$(CONFIG_MTK_PLATFORM_SENSOR)/seninf_$(MTK_DERIVED_PLATFORM)
$(eval $(call add_subdrv,seninf_$(MTK_DERIVED_PLATFORM)))
else
$(eval $(call add_subdrv,seninf))
endif


$(info imgsensor_isp6s_mon-objs : $(imgsensor_isp6s_mon-objs))
$(info imgsensor_isp6s_mon subdir-ccflags-y : $(subdir-ccflags-y))


ifeq ($(ARCH_MTK_PROJECT), k6853v1_64_6360_alpha)
subdir-ccflags-y += -DNEED_LATE_INITCALL
endif

ifeq ($(CONFIG_MACH_CAMERA_MT6877),y)
subdir-ccflags-y += -DNEED_LATE_INITCALL
endif

ifeq ($(ARCH_MTK_PROJECT), k6833v1_64_mt6317)
	subdir-ccflags-y += -DNEED_LATE_INITCALL
endif
