#!/bin/bash

function build_kernel_cmd() {
    mkdir -p ${TOPDIR}/LOGDIR

    cd ${TOPDIR}/kernel

    tools/bazel \
    --output_root=${KLEAF_OBJ} \
    --output_base=${OUTPUT_BASE} \
    build \
    --action_env=PATH=${LINUX_TOOLS}:/usr/bin:/bin \
    --//build/bazel_mgk_rules:kernel_version=${VERSION} \
    --experimental_writable_outputs --noenable_bzlmod --config=stamp \
    --repo_manifest=${MANIFEST} \
    //kernel_device_modules-${VERSION}:${KRN_MGK}_customer_modules_install.${variants_type} 2>&1 |tee ${TOPDIR}/LOGDIR/build_${CURRENT_LOG}.log

    tools/bazel \
    --output_root=${KLEAF_OBJ} \
    --output_base=${OUTPUT_BASE} \
    run \
    --action_env=PATH=${LINUX_TOOLS}:/usr/bin:/bin \
    --//build/bazel_mgk_rules:kernel_version=${VERSION} \
    --experimental_writable_outputs --noenable_bzlmod --config=stamp \
    --repo_manifest=${MANIFEST} \
    --nokmi_symbol_list_violations_check  \
    //kernel_device_modules-${VERSION}:${KRN_MGK}_customer_dist.${variants_type} \
    -- --dist_dir=${DIST_DIR} 2>&1 |tee ${TOPDIR}/LOGDIR/run_${CURRENT_LOG}.log

    mkdir -p ${DIST_INSTALL}
    cp ${KERNEL_IMG}/Image.lz4 ${DIST_INSTALL}/
    cp ${KERNEL_IMG}/vmlinux ${DIST_INSTALL}/
    cp ${KERNEL_IMG}/vmlinux.symvers ${DIST_INSTALL}/
    cp ${MODULE_KO}/*.ko ${DIST_INSTALL}/

    cd ${TOPDIR}/
}

function mk_dtboimg_cfg() {

    echo "file $1  out $2"

    echo $1.dtb >>$2
    dts_file=$1.dts
    dtsino=`grep -m 1 'oplus_boardid,dtsino' $dts_file | sed 's/ //g' | sed 's/.*oplus_boardid\,dtsino\=\"//g' | sed 's/\"\;.*//g'`
    pcbmask=`grep -m 1 'oplus_boardid,pcbmask' $dts_file | sed 's/ //g' | sed 's/.*oplus_boardid\,pcbmask\=\"//g' | sed 's/\"\;.*//g'`

    echo " id=$my_dtbo_id" >>$2
    echo " custom0=$dtsino" >> $2
    echo " custom1=$pcbmask" >> $2
    let my_dtbo_id++
}

function build_dt_cmd() {
    echo "build_dt_cmd start"
    mkdir -p ${TOPDIR}/LOGDIR

    rm ${DEVICE_TREE_OUT}/dtboimg.cfg
    mkdir -p ${KERNEL_OBJ}
    cp ${TOPDIR}/kernel/build/kernel/_setup_env.sh ${KERNEL_OBJ}/mtk_setup_env.sh

    cd ${TOPDIR}/kernel
    python kernel_device_modules-${VERSION}/scripts/gen_build_config.py \
          --kernel-defconfig gki_defconfig --kernel-defconfig-overlays "" \
          --kernel-build-config-overlays "" \
          -m user -o ${KERNEL_OBJ}/build.config

    for dtb in  $dtb_support_list
    do
        echo " dtb  $dtb "
        mkdir -p ${DEVICE_TREE_OUT}/mediatek
        cp ${DEVICE_TREE_SRC}/${dtb}.dts ${DEVICE_TREE_OUT}/mediatek/${dtb}.dts
        OUT_DIR=../${RELATIVE_KERNEL_OBJ} BUILD_CONFIG=../${RELATIVE_KERNEL_OBJ}/build.config GOALS=dtbs ../${BUILD_TOOLS}/build_kernel.sh ${dtb}.dtb
        cat ${DEVICE_TREE_OUT}/mediatek/${dtb}.dtb > ${DEVICE_TREE_OUT}/mediatek/dtb
    done

    for dtbo in  $dtbo_support_list
    do
        echo " dtbo  $dtbo "
        mkdir -p ${DEVICE_TREE_OUT}/${dtbo}
        cp ${DEVICE_TREE_SRC}/${dtbo}.dts ${DEVICE_TREE_OUT}/mediatek/${dtbo}.dts
        mk_dtboimg_cfg ${DEVICE_TREE_OUT}/mediatek/${dtbo} ${DEVICE_TREE_OUT}/dtboimg.cfg
        python3 ${DRV_GEN}  ${DWS_SRC}/${dtbo}.dws ${DEVICE_TREE_OUT}/${dtbo} ${DEVICE_TREE_OUT}/${dtbo} cust_dtsi
        #we must use Relative path,then we build success,or will build fail
        OUT_DIR=../${RELATIVE_KERNEL_OBJ} BUILD_CONFIG=../${RELATIVE_KERNEL_OBJ}/build.config GOALS=dtbs ../${BUILD_TOOLS}/build_kernel.sh ${dtbo}.dtb
    done

    cd ${TOPDIR}
    ${MKDTIMG} cfg_create ${DEVICE_TREE_OUT}/mediatek/dtbo.img  ${DEVICE_TREE_OUT}/dtboimg.cfg

    cp ${DEVICE_TREE_OUT}/mediatek/dtb ${IMAGE_OUT}/
    cp ${DEVICE_TREE_OUT}/mediatek/dtbo.img ${IMAGE_OUT}/
    echo "build_dt_cmd end"
 }
