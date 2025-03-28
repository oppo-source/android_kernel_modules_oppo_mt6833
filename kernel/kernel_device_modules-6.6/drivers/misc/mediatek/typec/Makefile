# SPDX-License-Identifier: GPL-2.0

ifeq ($(CONFIG_OPLUS_PD_EXT_SUPPORT),)
$(warining CONFIG_OPLUS_PD_EXT_SUPPORT not set)
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/typec/tcpc

obj-$(CONFIG_TCPC_CLASS) += tcpc/
endif

obj-$(CONFIG_MTK_USB_TYPEC_MUX) += mux/

