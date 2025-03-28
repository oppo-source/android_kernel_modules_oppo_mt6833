#!/bin/bash

function set_build_environment_common() {
    export CHIPSET_COMPANY=MTK
    source vendor/oplus/kernel/prebuilt/vendorsetup.sh
    export BAZEL_DO_NOT_DETECT_CPP_TOOLCHAIN=1
    export KERNEL_VERSION=kernel-6.6
    export OPLUS_FEATURES=""
    export SOURCE_DATE_EPOCH="0"
    # export OPLUS_USE_JFROG_CACHE="false"
    # export OPLUS_USE_BUILDBUDDY_REMOTE_BUILD="false"

    TOPDIR=$(readlink -f ${PWD})
    ACKDIR=${TOPDIR}/kernel
    TOOLS=${ACKDIR}/oplus/tools
    export JAVA_HOME=${TOPDIR}/prebuilts/jdk/jdk21/linux-x86
    ORIGIN_IMAGE=${ACKDIR}/oplus/prebuild/origin_img
    BOOT_TMP_IMAGE=${ACKDIR}/oplus/prebuild/boot_tmp
    IMAGE_OUT=${ACKDIR}/oplus/prebuild/out
    SIGN_OUT=${ACKDIR}/oplus/prebuild/sign_out
    VENDOR_BOOT_TMP_IMAGE=${ACKDIR}/oplus/prebuild/vendor_boot_tmp
    VENDOR_DLKM_TMP_IMAGE=${ACKDIR}/oplus/prebuild/vendor_dlkm_tmp
    SYSTEM_DLKM_TMP_IMAGE=${ACKDIR}/oplus/prebuild/system_dlkm_tmp
    DT_TMP_IMAGE=${ACKDIR}/oplus/prebuild/dt_tmp
    DIST_INSTALL=${ACKDIR}/oplus/prebuild/dist
    DT_DIR=${ACKDIR}/out/
    LINUX_TOOLS="${ACKDIR}/prebuilts/kernel-build-tools/linux-x86"
    PYTHON_TOOL="${ACKDIR}/prebuilts/build-tools/path/linux-x86/python3"
    MKBOOTIMG_PATH=${ACKDIR}/"tools/mkbootimg/mkbootimg.py"
    UNPACK_BOOTIMG_TOOL="${ACKDIR}/tools/mkbootimg/unpack_bootimg.py"
    LZ4="${ACKDIR}/prebuilts/kernel-build-tools/linux-x86/bin/lz4"
    CPIO="${ACKDIR}/prebuilts/build-tools/path/linux-x86/cpio"
    SIMG2IMG="${ACKDIR}/prebuilts/kernel-build-tools/linux-x86/bin/simg2img"
    IMG2SIMG="${ACKDIR}/prebuilts/kernel-build-tools/linux-x86/bin/img2simg"
    EROFS="${ACKDIR}/prebuilts/kernel-build-tools/linux-x86/bin/mkfs.erofs"
    BUILD_IMAGE="${ACKDIR}/prebuilts/kernel-build-tools/linux-x86/bin/build_image"
    MKDTIMG="${ACKDIR}/prebuilts/kernel-build-tools/linux-x86/bin/mkdtimg"
    MKBOOTFS="${ACKDIR}/prebuilts/kernel-build-tools/linux-x86/bin/mkbootfs"
    AVBTOOL="${ACKDIR}/prebuilts/kernel-build-tools/linux-x86/bin/avbtool"
    BIN="${ACKDIR}/prebuilts/kernel-build-tools/linux-x86/bin/"
    KERNEL_OUT=${ACKDIR}/out
    KRN_MGK=mgk_64_k66
    VERSION="6.6"
    CURRENT_LOG="$(date +"%Y_%m_%d_%H_%M_%S")"
    MKDTIMG="${TOPDIR}/kernel/prebuilts/kernel-build-tools/linux-x86/bin/mkdtimg"
    KERNEL_OBJ=${TOPDIR}/${RELATIVE_KERNEL_OBJ}
    DEVICE_TREE_OUT=${KERNEL_OBJ}/kernel/kernel_device_modules-${VERSION}/arch/arm64/boot/dts
    DEVICE_TREE_SRC=${TOPDIR}/kernel/kernel_device_modules-${VERSION}/arch/arm64/boot/dts/mediatek
    DRV_GEN=${TOPDIR}/vendor/mediatek/proprietary/tools/dct/python/DrvGen.py
    BUILD_TOOLS=device/mediatek/build/build/tools
    KERNEL_IMG=${TOPDIR}/kernel/bazel-bin/kernel_device_modules-${VERSION}/${KRN_MGK}_kernel_aarch64.${variants_type}
    MODULE_KO=${TOPDIR}/kernel/bazel-bin/kernel_device_modules-${VERSION}/${KRN_MGK}_customer_modules_install.${variants_type}
    DWS_SRC=${TOPDIR}/vendor/mediatek/proprietary/tools/dct/dws/${dtb_support_list}
    STRIP="${TOPDIR}/prebuilts/clang/host/linux-x86/llvm-binutils-stable/llvm-strip"
    VENDOR_MODULES_DIR=${DIST_DIR}/kernel_device_modules-${VERSION}/${KRN_MGK}_customer_modules_install.${variants_type}
    SYSTEM_MODULES_DIR=${DIST_DIR}/kernel_device_modules-${VERSION}/${KRN_MGK}_kernel_aarch64.${variants_type}
    INTREE_MODULE_OUT=${TOPDIR}/kernel/bazel-bin/kernel_device_modules-${VERSION}/${KRN_MGK}_modules.${variants_type}
    MANIFEST=${TOPDIR}/kernel/kernel_device_modules-${VERSION}/fake_manifest.xml
    my_dtbo_id=0
    mkdir -p ${IMAGE_OUT}
    set_platform
}

function set_build_environment_mt6991() {
    export OPLUS_VND_BUILD_PLATFORM=MT6991
    export DEFCONFIG_OVERLAYS=""
    export OPLUS_DEFCONFIG_OVERLAYS=k6991v1_64

    KRN_PLATFORM=k6991v1_64
    MAINDTB_PATH=${TOPDIR}/out/target/product/${KRN_PLATFORM}/obj/PACKAGING/dtb
    RELATIVE_KERNEL_OBJ=out/target/product/${KRN_PLATFORM}/obj/KERNEL_OBJ
    KLEAF_OBJ=${TOPDIR}/out_krn/target/product/${KRN_MGK}/obj/KLEAF_OBJ
    OUTPUT_BASE=${KLEAF_OBJ}/bazel/output_user_root/output_base
    DIST_DIR=${KLEAF_OBJ}/dist
    VENDOR_INTREE_MODULES_DIR=${OUTPUT_BASE}/execroot/__main__/bazel-out/k8-fastbuild/bin/kernel_device_modules-${VERSION}/${KRN_MGK}_modules.user
    VENDOR_OUTTREE_MODULES_DIR=${OUTPUT_BASE}/execroot/__main__/bazel-out/k8-fastbuild/bin/vendor
    dtb_support_list="mt6991"
    dtbo_support_list="k6991v1_64 oplus6991_23101 oplus6991_23105 oplus6991_23205 oplus6991_23216 oplus6991_23106"
    ko_order_table=device/mediateksample/${KRN_PLATFORM}/ko_order_table.csv
}

function set_build_environment_mt6899() {
    export OPLUS_VND_BUILD_PLATFORM=MT6899
    export DEFCONFIG_OVERLAYS="oplus6899.config"
    export OPLUS_DEFCONFIG_OVERLAYS=k6899v1_64

    KRN_PLATFORM=k6899v1_64
    MAINDTB_PATH=${TOPDIR}/out/target/product/${KRN_PLATFORM}/obj/PACKAGING/dtb
    RELATIVE_KERNEL_OBJ=out/target/product/${KRN_PLATFORM}/obj/KERNEL_OBJ
    KLEAF_OBJ=${TOPDIR}/out_krn/target/product/${KRN_MGK}/obj/KLEAF_OBJ
    OUTPUT_BASE=${KLEAF_OBJ}/bazel/output_user_root/output_base
    DIST_DIR=${KLEAF_OBJ}/dist
    VENDOR_INTREE_MODULES_DIR=${OUTPUT_BASE}/execroot/__main__/bazel-out/k8-fastbuild/bin/kernel_device_modules-${VERSION}/${KRN_MGK}_modules.user
    VENDOR_OUTTREE_MODULES_DIR=${OUTPUT_BASE}/execroot/__main__/bazel-out/k8-fastbuild/bin/vendor
    dtb_support_list="mt6899"
    dtbo_support_list="k6899v1_64 oplus6899_24613 oplus6899_24720"
    ko_order_table=device/mediateksample/${KRN_PLATFORM}/ko_order_table.csv
}

function set_build_environment_mt6877() {
    export OPLUS_VND_BUILD_PLATFORM=MT6877
    export DEFCONFIG_OVERLAYS="mt6877_overlay.config"
    export OPLUS_DEFCONFIG_OVERLAYS=k6877v1_64

    KRN_PLATFORM=k6877v1_64
    MAINDTB_PATH=${TOPDIR}/out/target/product/${KRN_PLATFORM}/obj/PACKAGING/dtb
    RELATIVE_KERNEL_OBJ=out/target/product/${KRN_PLATFORM}/obj/KERNEL_OBJ
    KLEAF_OBJ=${TOPDIR}/out/target/product/${KRN_PLATFORM}/obj/KLEAF_OBJ
    OUTPUT_BASE=${KLEAF_OBJ}/bazel/output_user_root/output_base
    DIST_DIR=${KLEAF_OBJ}/dist
    VENDOR_INTREE_MODULES_DIR=${OUTPUT_BASE}/execroot/__main__/bazel-out/k8-fastbuild/bin/kernel_device_modules-${VERSION}/${KRN_MGK}_modules.user
    VENDOR_OUTTREE_MODULES_DIR=${OUTPUT_BASE}/execroot/__main__/bazel-out/k8-fastbuild/bin/vendor
    dtb_support_list="mt6877"
    dtbo_support_list="mediatek/oplus6877_23231 mediatek/oplus6877_23051 mediatek/oplus6877_23241 mediatek/oplus6877_23243 mediatek/oplus6877_23035 mediatek/oplus6877_23321 mediatek/oplus6877_22277 mediatek/oplus6877_23686 mediatek/oplus6877_23687 mediatek/oplus6877_23689 mediatek/oplus6877_23707 mediatek/oplus6877_23709 mediatek/oplus6877_22629 mediatek/oplus6877_22710 mediatek/oplus6877_22711 mediatek/oplus6877_22633 mediatek/oplus6877_22712 mediatek/oplus6877_22713 mediatek/oplus6877_22612 mediatek/oplus6877_22693 mediatek/oplus6877_226B1 mediatek/oplus6877_22694"
    ko_order_table=device/mediateksample/${KRN_PLATFORM}/ko_order_table.csv
}

function set_build_environment_mt6833() {
    export OPLUS_VND_BUILD_PLATFORM=MT6833
    export DEFCONFIG_OVERLAYS="mt6833_overlay.config"
    export OPLUS_DEFCONFIG_OVERLAYS=k6833v1_64

    KRN_PLATFORM=k6833v1_64
    MAINDTB_PATH=${TOPDIR}/out/target/product/${KRN_PLATFORM}/obj/PACKAGING/dtb
    RELATIVE_KERNEL_OBJ=out/target/product/${KRN_PLATFORM}/obj/KERNEL_OBJ
    KLEAF_OBJ=${TOPDIR}/out/target/product/${KRN_PLATFORM}/obj/KLEAF_OBJ
    OUTPUT_BASE=${KLEAF_OBJ}/bazel/output_user_root/output_base
    DIST_DIR=${KLEAF_OBJ}/dist
    VENDOR_INTREE_MODULES_DIR=${OUTPUT_BASE}/execroot/__main__/bazel-out/k8-fastbuild/bin/kernel_device_modules-${VERSION}/${KRN_MGK}_modules.user
    VENDOR_OUTTREE_MODULES_DIR=${OUTPUT_BASE}/execroot/__main__/bazel-out/k8-fastbuild/bin/vendor
    dtb_support_list="mt6833"
    dtbo_support_list="mediatek/oplus6833_22331 mediatek/oplus6833_22333 mediatek/oplus6833_22869 mediatek/oplus6833_22291 mediatek/oplus6833_22292 mediatek/oplus6833_23253 mediatek/oplus6833_22705 mediatek/oplus6833_22706 mediatek/oplus6833_22610"
    ko_order_table=device/mediateksample/${KRN_PLATFORM}/ko_order_table.csv
}

function set_build_environment_mt6768() {
    export OPLUS_VND_BUILD_PLATFORM=MT6769
    export DEFCONFIG_OVERLAYS="mt6768_overlay.config"
    export OPLUS_DEFCONFIG_OVERLAYS=k69v1_64

    KRN_PLATFORM=k69v1_64
    MAINDTB_PATH=${TOPDIR}/out/target/product/${KRN_PLATFORM}/obj/PACKAGING/dtb
    RELATIVE_KERNEL_OBJ=out/target/product/${KRN_PLATFORM}/obj/KERNEL_OBJ
    KLEAF_OBJ=${TOPDIR}/out/target/product/${KRN_PLATFORM}/obj/KLEAF_OBJ
    OUTPUT_BASE=${KLEAF_OBJ}/bazel/output_user_root/output_base
    DIST_DIR=${KLEAF_OBJ}/dist
    VENDOR_INTREE_MODULES_DIR=${OUTPUT_BASE}/execroot/__main__/bazel-out/k8-fastbuild/bin/kernel_device_modules-${VERSION}/${KRN_MGK}_modules.user
    VENDOR_OUTTREE_MODULES_DIR=${OUTPUT_BASE}/execroot/__main__/bazel-out/k8-fastbuild/bin/vendor
    dtb_support_list="mt6768"
    dtbo_support_list="mediatek/oplus6769_22351_EVT mediatek/oplus6769_22351 mediatek/oplus6769_22352_EVT mediatek/oplus6769_22352 mediatek/oplus6769_22361_T0 mediatek/oplus6769_22361_EVT mediatek/oplus6769_22361 mediatek/oplus6769_22362_T0 mediatek/oplus6769_22362_EVT mediatek/oplus6769_22362 mediatek/oplus6769_22365_T0 mediatek/oplus6769_22365_EVT mediatek/oplus6769_22365 mediatek/oplus6769_22368"
    ko_order_table=device/mediateksample/${KRN_PLATFORM}/ko_order_table.csv
}

function init_build_environment() {
    set_build_environment_common
    case $variants_platform in
        mt6991)
            echo "mt6991 path"
            set_build_environment_mt6991
        ;;

        mt6899)
            echo "mt6899 path"
            set_build_environment_mt6899
        ;;

        mt6877)
            echo "mt6877 path"
            set_build_environment_mt6877
        ;;

        mt6833)
            echo "mt6833 path"
            set_build_environment_mt6833
        ;;

        mt6769|mt6768)
            echo "mt6768 path"
            set_build_environment_mt6768
        ;;

        *)
            echo "default path"
            set_build_environment_mt6991
        ;;
    esac    # --- end of case ---
}

set_platform()
{
   sed -i s/platform/${KRN_PLATFORM}/g ${TOPDIR}/kernel/oplus/bazel/oplus_platform.bzl
}

function print_module_help ()
{
    echo "===================================================================="
    echo "you can check this for usage introduction:"
    echo ""
    echo ""
    echo "this is an example:"
    echo "./kernel/oplus/build/oplus_build_module.sh user <ko patch>"
    echo ""
    echo "you need to input ko path or name like this:"
    echo "[1] if it is <in tree ko>, use this format:<ko_path>/<ko_name>"
    echo "example like this: drivers/soc/oplus/dft/common/olc/olc.ko"
    echo ""
    echo "[2] if it is <out of tree ko>, use this format:<ko_path>:<ko_name>"
    echo "example like this: vendor/mediatek/kernel_modules/fpsgo_cus:fpsgo_cus"
    echo ""
    echo "===================================================================="
    echo "you can click [enter] to start or [ctrl+c] to exit"
}

print_help()
{
    echo "userdebug build command"
    echo "./kernel/oplus/build/oplus_xxx_xxx.sh mt6991 userdebug"
    echo "user build command"
    echo "./kernel/oplus/build/oplus_xxx_xxx.sh mt6991 user"
    echo ""
}
print_end_help()
{
    echo "if you add new ko want to install in vendor_boot"
    echo "you need local add in kernel/oplus/prebuild/local_add_vendor_boot.txt then rebuild"

    echo "if you add new ko want to install in vendor_dlkm"
    echo "you need local add in kernel/oplus/prebuild/local_add_vendor_dlkm.txt then rebuild"

}
build_start_time()
{
   ncolors=$(tput colors 2>/dev/null)
   if [ -n "$ncolors" ] && [ $ncolors -ge 8 ]; then
       color_red=$'\E'"[0;31m"
       color_green=$'\E'"[0;32m"
       color_yellow=$'\E'"[0;33m"
       color_white=$'\E'"[00m"
   else
       color_red=""
       color_green=""
       color_yellow=""
       color_white=""
   fi

   start_time=$(date +"%s")
   start_date=$(date +"%Y_%m_%d %H:%M:%S")
   echo -n "${color_green}"
   echo "start date:$start_date"
   echo -n "${color_white}"
   echo
   echo
   echo " ${color_white}"
   echo
}
build_end_time()
{
   end_time=$(date +"%s")
   end_date=$(date +"%Y_%m_%d %H:%M:%S")
   tdiff=$(($end_time-$start_time))
   hours=$(($tdiff / 3600 ))
   mins=$((($tdiff % 3600) / 60))
   secs=$(($tdiff % 60))

   echo
   echo -n "${color_green}"
   echo "start date:$start_date"
   echo "end date:$end_date"
   echo "build total time:"
   if [ $hours -gt 0 ] ; then
       printf "(%02g:%02g:%02g (hh:mm:ss))" $hours $mins $secs
   elif [ $mins -gt 0 ] ; then
       printf "(%02g:%02g (mm:ss))" $mins $secs
   elif [ $secs -gt 0 ] ; then
       printf "(%s seconds)" $secs
   fi
   echo
   echo " ${color_white}"
   echo
}
print_platform()
{
    echo
    echo "select platform:"
    echo "   1.  mt6991"
    echo "   2.  23p"
    echo
}

choose_platform()
{
    local default_value=mt6991
    local ANSWER
    print_platform
    echo "New Platform need add to here "
    echo -n "Which would you like? [$default_value]"

    if [[ -n "$1" ]] ; then
        ANSWER=$1
    else
        #read ANSWER
        ANSWER=$default_value
    fi

    echo "$ANSWER"
    echo

    case $ANSWER in
        mt6991)
            variants_platform=mt6991
        ;;
        mt6899)
            variants_platform=mt6899
        ;;
        mt6877)
            variants_platform=mt6877
        ;;
        mt6833)
            variants_platform=mt6833
        ;;
        mt6769|mt6768)
            variants_platform=mt6768
        ;;
        *)
            variants_platform=mt6991
        ;;
    esac
    echo "now default auto select platform $variants_platform "
}

print_build_type()
{
    echo
    echo "Select Build Type:"
    echo "   1.  userdebug"
    echo "   2.  user"
    echo "   you can select "
}

choose_build_type()
{
    local default_value=user
    local ANSWER
    print_build_type
    echo -n "Which build type would you like? [$default_value] "
    if [[ -n "$1" ]] ; then
        ANSWER=$1
    else
        read ANSWER
    fi

    echo "$ANSWER"
    echo

    case $ANSWER in
        1)
           variants_type=userdebug
        ;;
        userdebug)
        variants_type=userdebug
        ;;
        2)
            variants_type=user
        ;;
        user)
            variants_type=user
        ;;
        *)
            variants_type=user
        ;;
    esac
    echo "variants_type ${variants_type} "
}

print_lto_type()
{
    echo
    echo "you can get more information  https://lkml.org/lkml/2020/12/8/1006"
    echo
    echo "While all developers agree that ThinLTO is a much more palatable
          experience than full LTO; our product teams prefer the excessive build
          time and memory high water mark (at build time) costs in exchange for
          slightly better performance than ThinLTO in <benchmarks that I've been
          told are important>.  Keeping support for full LTO in tree would help
          our product teams reduce the amount of out of tree code they have.  As
          long as <benchmarks that I've been told are important> help
          sell/differentiate phones, I suspect our product teams will continue
          to ship full LTO in production."
    echo
    echo "Select build LTO:"

    echo "   1.  full (need more time build but better performance)"
    echo "   2.  thin (build fast but can't use as performance test)"
    echo "   3.  none"
}

choose_lto_type()
{
    local default_value=none
    local ANSWER
    print_lto_type
    echo -n "Which would you like? [$default_value] "
    if [[ -n "$1" ]] ; then
        ANSWER=$1
    else
        read ANSWER
    fi

    echo "$ANSWER"
    echo

    case $ANSWER in
        1)
           export LTO=full
        ;;
        full)
           export LTO=full
        ;;
        2)
           export LTO=thin
        ;;
        thin)
           export  LTO=thin
        ;;
        *)
        ;;
    esac
    echo "LTO $LTO "
}

print_target_build()
{
    echo
    echo "Select build target:"
    echo "   1.  all (boot/dtbo)"
    echo "   2.  dtbo"
    echo "   3.  ko you need export like this \"export OPLUS_KO_PATH=drivers/input/touchscreen/focaltech_touch\""
    echo
}

choose_target_build()
{
    #set defaut value to enable
    local default_value=all
    local ANSWER
    print_target_build
    echo "Which would you like? [$default_value] "
    if [[ -n "$1" ]] ; then
        ANSWER=$1
    else
        read ANSWER
    fi

    echo "$ANSWER"
    echo
    export RECOMPILE_DTBO=""
    export RECOMPILE_KERNEL=""
    export OPLUS_BUILD_KO=""
    case $ANSWER in
        1)
            target_type=all
            export RECOMPILE_KERNEL="1"
        ;;
        all)
            target_type=all
            export RECOMPILE_KERNEL="1"
        ;;
        2)
            target_type=dtbo
            export RECOMPILE_DTBO="1"
        ;;
        dtbo)
            target_type=dtbo
            export RECOMPILE_DTBO="1"
        ;;
        3)
            export OPLUS_BUILD_KO="true"
        ;;
        ko)
            export OPLUS_BUILD_KO="true"
        ;;
        *)
            target_type=all
            export RECOMPILE_KERNEL="1"
        ;;
    esac
    echo "target_type $target_type RECOMPILE_KERNEL $RECOMPILE_KERNEL OPLUS_BUILD_KO $OPLUS_BUILD_KO "
}

print_repack_img()
{
    echo
    echo "Select enable repack boot vendor_boot img:"
    echo "   1.  enable"
    echo "   2.  disable"
    echo
}

choose_repack_img()
{
    local default_value=enable
    local ANSWER
    print_repack_img
    echo -n "Which would you like? [$default_value] "
    if [[ -n "$1" ]] ; then
        ANSWER=$1
    else
        read ANSWER
    fi

    echo "$ANSWER"
    echo

    case $ANSWER in
        1)
            export REPACK_IMG=true
        ;;
        enable)
            export REPACK_IMG=true
        ;;
        2)
            export REPACK_IMG=false
        ;;
        disable)
            export REPACK_IMG=false
        ;;
        *)
            export REPACK_IMG=true
        ;;
    esac
    echo "REPACK_IMG $REPACK_IMG "
}
print_help
choose_platform $1
choose_build_type $2
#choose_lto_type $3
#choose_target_build $4
#choose_repack_img $5
