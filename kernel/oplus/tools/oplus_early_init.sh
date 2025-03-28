#!/bin/sh

script_dir=$(dirname $(readlink -f "$0"))
echo "script_dir ${script_dir}"
echo "OPLUS_USE_PREBUILT_BOOTIMAGE ${OPLUS_USE_PREBUILT_BOOTIMAGE}"
echo "LINUX_KERNEL_VERSION ${LINUX_KERNEL_VERSION}"

python3 ${script_dir}/ogki_gki_artifactory.py -t download \
	-k ${OPLUS_USE_PREBUILT_BOOTIMAGE:-GKI} \
	-o $script_dir/../../../vendor/aosp_gki/${LINUX_KERNEL_VERSION:-kernel-6.6}/aarch64
	
