# # SPDX-License-Identifier: GPL-2.0-only
# # Copyright (C) 2018-2022 Oplus. All rights reserved.

# ifneq ($(wildcard $(KERNEL_SRC)/android/abi_gki_aarch64.stg),)
# BOOT_MIXED_BUILD=true
# endif

# KBUILD_OPTIONS+= DFT_ROOT=$(KERNEL_SRC)/$(M)
# KBUILD_OPTIONS += MODNAME?=dft_dlkm
# ifneq ($(wildcard $(KERNEL_SRC)/$(DEVICE_MODULES_REL_DIR)/Makefile.include),)
# include $(KERNEL_SRC)/$(DEVICE_MODULES_REL_DIR)/Makefile.include
# endif

# all: EXTRA_SYMBOLS += $(extra_symbols)
# all:
# 	$(MAKE) -C $(KERNEL_SRC) M=$(M) modules $(KBUILD_OPTIONS) KBUILD_EXTRA_SYMBOLS="$(EXTRA_SYMBOLS)"

# modules_install:
# 	$(MAKE) INSTALL_MOD_STRIP=1 -C $(KERNEL_SRC) M=$(M) modules_install

# %:
# 	$(MAKE) -C $(KERNEL_SRC) M=$(M) $@ $(KBUILD_OPTIONS)

# clean:
# 	rm -f *.o *.ko *.mod.c *.mod.o *~ .*.cmd Module.symvers
# 	rm -rf .tmp_versions
