# SPDX-License-Identifier: GPL-2.0
ifeq ($(CONFIG_OPLUS_PD_EXT_SUPPORT),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/typec/tcpc/inc
else
ccflags-y += -I$(srctree)/drivers/power/oplus/pd_ext/inc
endif

obj-$(CONFIG_EXTCON_MTK_USB) += extcon-mtk-usb.o
