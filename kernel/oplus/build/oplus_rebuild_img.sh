#!/bin/bash

source kernel/oplus/build/oplus_setup.sh $1 $2
init_build_environment
IS_INTRANET="no"

function is_intranet() {
    ping -c1 -i1 gerrit_url > /dev/null 2>&1
    if [ $? -eq 0 ]; then
        echo "This is an internal network"
        IS_INTRANET="yes"
    else
        echo "This is not an internal network"
        IS_INTRANET="no"
    fi
}

function download_prebuild_image() {
    if test "$IS_INTRANET" = "yes"; then
        if [ -z ${IMAGE_SERVER} ]; then
            echo "======================================================================"
            echo ""
            echo "you need input base version like this:"
            echo "http://xxx..xxx.com/xxx/userdebug/xxx_userdebug/"
            echo ""
            echo "or you can exit it and then exoprt like this:"
            echo "export IMAGE_SERVER=http://xxx..xxx.com/xxx/userdebug/xxx_userdebug/"
            echo ""
            echo "======================================================================"
            read IMAGE_SERVER
        fi

        if ! wget -qS ${IMAGE_SERVER}/compile.ini; then
            echo "server can't connect,please set IMAGE_SERVER and try again"
            return
        fi
        if [[ ! -e "${ORIGIN_IMAGE}/vendor_boot.img" ]]; then
            mkdir -p ${ORIGIN_IMAGE}
            wget ${IMAGE_SERVER}/compile.ini  -O ${ORIGIN_IMAGE}/compile.ini
            OFP_DRI=`cat ${ORIGIN_IMAGE}/compile.ini | grep "ofp_folder =" | awk '{print $3 }'`
            wget ${IMAGE_SERVER}/${OFP_DRI}/IMAGES/boot.img -O ${ORIGIN_IMAGE}/boot.img
            wget ${IMAGE_SERVER}/${OFP_DRI}/IMAGES/vendor_boot.img -O ${ORIGIN_IMAGE}/vendor_boot.img
            wget ${IMAGE_SERVER}/${OFP_DRI}/IMAGES/system_dlkm.img -O ${ORIGIN_IMAGE}/system_dlkm.img
            wget ${IMAGE_SERVER}/${OFP_DRI}/IMAGES/vendor_dlkm.img -O ${ORIGIN_IMAGE}/vendor_dlkm.img
            wget ${IMAGE_SERVER}/${OFP_DRI}/IMAGES/dtbo.img -O ${ORIGIN_IMAGE}/dtbo.img
            wget ${IMAGE_SERVER}/${OFP_DRI}/IMAGES/init_boot.img -O ${ORIGIN_IMAGE}/init_boot.img
            wget ${IMAGE_SERVER}/${OFP_DRI}/IMAGES/vbmeta.img  -O ${ORIGIN_IMAGE}/vbmeta.img
            wget ${IMAGE_SERVER}/${OFP_DRI}/IMAGES/vbmeta_system.img  -O ${ORIGIN_IMAGE}/vbmeta_system.img
            wget ${IMAGE_SERVER}/${OFP_DRI}/IMAGES/vbmeta_vendor.img  -O ${ORIGIN_IMAGE}/vbmeta_vendor.img
            wget ${IMAGE_SERVER}/${OFP_DRI}/META/misc_info.txt -O ${ORIGIN_IMAGE}/misc_info.txt
            wget ${IMAGE_SERVER}/${OFP_DRI}/META/vendor_dlkm_image_info.txt -O ${ORIGIN_IMAGE}/vendor_dlkm_image_info.txt
            wget ${IMAGE_SERVER}/${OFP_DRI}/META/system_dlkm_image_info.txt -O ${ORIGIN_IMAGE}/system_dlkm_image_info.txt
            wget ${IMAGE_SERVER}/${OFP_DRI}/META/file_contexts.bin -O ${ORIGIN_IMAGE}/file_contexts.bin
            cp ${TOOLS}/testkey_rsa4096.pem ${ORIGIN_IMAGE}/
            cp ${TOOLS}/testkey_rsa2048.pem ${ORIGIN_IMAGE}/
            cp ${TOOLS}/testkey.avbpubkey ${ORIGIN_IMAGE}/
        fi
    fi
}

function get_image_info() {

    ${AVBTOOL} info_image --image  ${ORIGIN_IMAGE}/boot.img >  ${ORIGIN_IMAGE}/local_boot_image_info.txt
    ${AVBTOOL} info_image --image  ${ORIGIN_IMAGE}/vendor_boot.img >  ${ORIGIN_IMAGE}/local_vendor_boot_image_info.txt
    ${AVBTOOL} info_image --image  ${ORIGIN_IMAGE}/system_dlkm.img >  ${ORIGIN_IMAGE}/local_system_dlkm_info.txt
    ${AVBTOOL} info_image --image  ${ORIGIN_IMAGE}/vendor_dlkm.img >  ${ORIGIN_IMAGE}/local_vendor_dlkm_image_info.txt
    #${AVBTOOL} info_image --image  ${ORIGIN_IMAGE}/dtbo.img >  ${ORIGIN_IMAGE}/local_dtbo_image_info.txt
    ${AVBTOOL} info_image --image  ${ORIGIN_IMAGE}/vbmeta.img >  ${ORIGIN_IMAGE}/local_vbmeta_image_info.txt
    ${AVBTOOL} info_image --image  ${ORIGIN_IMAGE}/vbmeta_system.img >  ${ORIGIN_IMAGE}/local_vbmeta_system_image_info.txt
    ${AVBTOOL} info_image --image  ${ORIGIN_IMAGE}/vbmeta_vendor.img >  ${ORIGIN_IMAGE}/local_vbmeta_vendor_image_info.txt
}

function get_modules_list() {

    rm ${ACKDIR}/oplus/prebuild/vendor_dlkm.load
    rm ${ACKDIR}/oplus/prebuild/vendor_boot.load
    ko_order_table_contents=$(cat ${ko_order_table} | cut -d , -f 1-6 | sed 's/ //g')

    for ko in  $ko_order_table_contents
    do
        ko_line=(${ko//,/ })
        ko_name=${ko_line[0]}
        ko_path=${ko_line[1]}
        ko_partition=${ko_line[2]}
        ko_loaded=${ko_line[3]}
        ko_recovery=${ko_line[4]}
        ko_mode=${ko_line[5]}
        build_mode=(${ko_mode//\// })
        #echo $ko_name
        #echo $ko_path
        #echo $ko_partition
        #echo $ko_loaded
        #echo $ko_recovery
        #echo $ko_mode
        for mode in ${build_mode[@]}
        do
            #echo $mode
            if [ "$mode" = "${variants_type}" ]; then

                if [ "$ko_partition" = "vendor" ] && [ "$ko_loaded" = "Y" ]; then
                   file_name=$(basename $ko_path)
                   #echo "$ko_name  $ko_partition $file_name"
                   echo $file_name >> ${ACKDIR}/oplus/prebuild/vendor_dlkm.load
                fi
                if [ "$ko_partition" = "ramdisk" ] && [ "$ko_loaded" = "Y" ]; then

                   file_name=`basename $ko_path`
                   #echo "$ko_name  $ko_partition $file_name"
                   echo $file_name >> ${ACKDIR}/oplus/prebuild/vendor_boot.load
                fi
            fi
        done
    done
}

function sign_boot_image() {

    algorithm=$(awk -F '[= ]' '$1=="avb_boot_algorithm" {$1="";print}' ${ORIGIN_IMAGE}/misc_info.txt)
    partition_size=$(awk -F '[= ]' '$1=="boot_size" {$1="";print}' ${ORIGIN_IMAGE}/misc_info.txt)
    footer_args=$(awk -F '[= ]' '$1=="avb_boot_add_hash_footer_args" {$1="";print}' ${ORIGIN_IMAGE}/misc_info.txt)
    partition_name=boot
    salt=`uuidgen | sed 's/-//g'`
    ${AVBTOOL} add_hash_footer \
        --image ${IMAGE_OUT}/boot.img  \
        --partition_name ${partition_name} \
        --partition_size ${partition_size}\
        --algorithm ${algorithm} \
        --key ${ORIGIN_IMAGE}/testkey_rsa2048.pem \
        --salt ${salt} \
        ${footer_args}

    if test "$IS_INTRANET" = "yes"; then
        if [[ -e "${IMAGE_OUT}/signed_boot.img" ]]; then
            rm ${IMAGE_OUT}/signed_boot.img
        fi
        ${TOOLS}/sign_image ${IMAGE_OUT}/boot.img ${IMAGE_SERVER}
        if [[ -e "${IMAGE_OUT}/signed_boot.img" ]]; then
            mkdir -p ${SIGN_OUT}
            mv ${IMAGE_OUT}/signed_boot.img ${SIGN_OUT}/boot.img
            echo "the boot.img is signed successfully!!!"
        else
            echo "the boot.img is failed to be signed!!!"
        fi
    else
        echo "the boot.img is not signed!!!"
    fi
}

function sign_vendor_boot_image() {

    algorithm=$(awk -F '[= ]' '$1=="avb_vendor_boot_algorithm" {$1="";print}' ${ORIGIN_IMAGE}/misc_info.txt)
    partition_size=$(awk -F '[= ]' '$1=="vendor_boot_size" {$1="";print}' ${ORIGIN_IMAGE}/misc_info.txt)
    footer_args=$(awk -F '[= ]' '$1=="avb_vendor_boot_add_hash_footer_args" {$1="";print}' ${ORIGIN_IMAGE}/misc_info.txt)
    partition_name=vendor_boot
    salt=`uuidgen | sed 's/-//g'`

    if [ -z "$algorithm" ]; then
     algorithm="SHA256_RSA4096"
    fi

    if [ -z "$partition_size" ]; then
     partition_size=`cat ${ORIGIN_IMAGE}/local_vendor_boot_image_info.txt | grep "Image size:" | awk '{print $3 }'`
    fi

    ${AVBTOOL} add_hash_footer \
        --image ${IMAGE_OUT}/vendor_boot.img  \
        --partition_name ${partition_name} \
        --partition_size ${partition_size}\
        --algorithm ${algorithm} \
        --key ${ORIGIN_IMAGE}/testkey_rsa4096.pem \
        --salt ${salt} \
        ${footer_args}

    if test "$IS_INTRANET" = "yes"; then
        if [[ -e "${IMAGE_OUT}/signed_vendor_boot.img" ]]; then
            rm ${IMAGE_OUT}/signed_vendor_boot.img
        fi
        ${TOOLS}/sign_image ${IMAGE_OUT}/vendor_boot.img ${IMAGE_SERVER}
        if [[ -e "${IMAGE_OUT}/signed_vendor_boot.img" ]]; then
            mkdir -p ${SIGN_OUT}
            mv ${IMAGE_OUT}/signed_vendor_boot.img ${SIGN_OUT}/vendor_boot.img
            echo "the vendor_boot.img is signed successfully!!!"
        else
            echo "the vendor_boot.img is failed to be signed!!!"
        fi
    else
        echo "the vendor_boot.img is not signed!!!"
    fi
}

function sign_dtbo_image() {

    algorithm=$(awk -F '[= ]' '$1=="avb_dtbo_algorithm" {$1="";print}' ${ORIGIN_IMAGE}/misc_info.txt)
    partition_size=$(awk -F '[= ]' '$1=="dtbo_size" {$1="";print}' ${ORIGIN_IMAGE}/misc_info.txt)
    footer_args=$(awk -F '[= ]' '$1=="avb_dtbo_add_hash_footer_args" {$1="";print}' ${ORIGIN_IMAGE}/misc_info.txt)
    partition_name=dtbo
    salt=`uuidgen | sed 's/-//g'`
    if [ -z "$algorithm" ]; then
     algorithm="SHA256_RSA4096"
    fi
    ${AVBTOOL} add_hash_footer \
        --image ${IMAGE_OUT}/dtbo.img  \
        --partition_name ${partition_name} \
        --partition_size ${partition_size}\
        --algorithm ${algorithm} \
        --key ${ORIGIN_IMAGE}/testkey_rsa4096.pem \
        --salt ${salt} \
        ${footer_args}

    if test "$IS_INTRANET" = "yes"; then
        if [[ -e "${IMAGE_OUT}/signed_dtbo.img" ]]; then
            rm ${IMAGE_OUT}/signed_dtbo.img
        fi
        ${TOOLS}/sign_image ${IMAGE_OUT}/dtbo.img ${IMAGE_SERVER}
        if [[ -e "${IMAGE_OUT}/signed_dtbo.img" ]]; then
            mkdir -p ${SIGN_OUT}
            mv ${IMAGE_OUT}/signed_dtbo.img ${SIGN_OUT}/dtbo.img
            echo "the dtbo.img is signed successfully!!!"
        else
            echo "the dtbo.img is failed to be signed!!!"
        fi
    else
        echo "the dtbo.img is not signed!!!"
    fi
}

function sign_vendor_dlkm_image() {
    algorithm=$(awk -F '[= ]' '$1=="avb_vendor_dlkm_algorithm" {$1="";print}' ${ORIGIN_IMAGE}/vendor_dlkm_image_info.txt)
    partition_size=$(awk -F '[= ]' '$1=="vendor_dlkm_size" {$1="";print}' ${ORIGIN_IMAGE}/vendor_dlkm_image_info.txt)
    footer_args=$(awk -F '[= ]' '$1=="avb_vendor_dlkm_add_hashtree_footer_args" {$1="";print}' ${ORIGIN_IMAGE}/vendor_dlkm_image_info.txt)
    partition_name=vendor_dlkm
    salt=`uuidgen | sed 's/-//g'`

    if [ -z "$algorithm" ]; then
     algorithm="SHA256_RSA4096"
    fi

    if [ -z "$partition_size" ]; then
     partition_size=`cat ${ORIGIN_IMAGE}/local_vendor_dlkm_image_info.txt | grep "Image size:" | awk '{print $3 }'`
    fi

    ${AVBTOOL} add_hashtree_footer \
        --partition_name ${partition_name} \
        --partition_size ${partition_size}\
        --do_not_generate_fec \
        --image ${IMAGE_OUT}/vendor_dlkm.img  \
        --hash_algorithm sha256 \
        --salt ${salt}  \
        ${footer_args}

    if test "$IS_INTRANET" = "yes"; then
        if [[ -e "${IMAGE_OUT}/signed_vendor_dlkm.img" ]]; then
            rm ${IMAGE_OUT}/signed_vendor_dlkm.img
        fi
        ${TOOLS}/sign_image ${IMAGE_OUT}/vendor_dlkm.img ${IMAGE_SERVER}
        if [[ -e "${IMAGE_OUT}/signed_vendor_dlkm.img" ]]; then
            mkdir -p ${SIGN_OUT}
            mv ${IMAGE_OUT}/signed_vendor_dlkm.img ${SIGN_OUT}/vendor_dlkm.img
            echo "the vendor_dlkm.img is signed successfully!!!"
        else
            echo "the vendor_dlkm.img is failed to be signed!!!"
        fi
    else
        echo "the vendor_dlkm.img is not signed!!!"
    fi
}

function sign_system_dlkm_image() {
    algorithm=$(awk -F '[= ]' '$1=="avb_system_dlkm_algorithm" {$1="";print}' ${ORIGIN_IMAGE}/system_dlkm_image_info.txt)
    partition_size=$(awk -F '[= ]' '$1=="system_dlkm_size" {$1="";print}' ${ORIGIN_IMAGE}/system_dlkm_image_info.txt)
    footer_args=$(awk -F '[= ]' '$1=="avb_add_hashtree_footer_args" {$1="";print}' ${ORIGIN_IMAGE}/system_dlkm_image_info.txt)
    partition_name=system_dlkm
    salt=`uuidgen | sed 's/-//g'`

    if [ -z "$algorithm" ]; then
     algorithm="SHA256_RSA4096"
    fi

    if [ -z "$partition_size" ]; then
     partition_size=`cat ${ORIGIN_IMAGE}/local_system_dlkm_info.txt | grep "Image size:" | awk '{print $3 }'`
    fi

    ${AVBTOOL} add_hashtree_footer \
        --partition_name ${partition_name} \
        --partition_size ${partition_size}\
        --do_not_generate_fec \
        --image ${IMAGE_OUT}/system_dlkm.img  \
        --hash_algorithm sha256 \
        --salt ${salt}  \
        ${footer_args}

    if test "$IS_INTRANET" = "yes"; then
        if [[ -e "${IMAGE_OUT}/signed_system_dlkm.img" ]]; then
            rm ${IMAGE_OUT}/signed_system_dlkm.img
        fi
        ${TOOLS}/sign_image ${IMAGE_OUT}/system_dlkm.img ${IMAGE_SERVER}
        if [[ -e "${IMAGE_OUT}/signed_system_dlkm.img" ]]; then
            mkdir -p ${SIGN_OUT}
            mv ${IMAGE_OUT}/signed_system_dlkm.img ${SIGN_OUT}/system_dlkm.img
            echo "the system_dlkm.img is signed successfully!!!"
        else
            echo "the system_dlkm.img is failed to be signed!!!"
        fi
    else
        echo "the system_dlkm.img is not signed!!!"
    fi
}

function sign_prebuild_image() {
    sign_boot_image
    sign_vendor_boot_image
    sign_dtbo_image
    sign_vendor_dlkm_image
    sign_system_dlkm_image
}

rebuild_boot_image() {
    echo "**********rebuild boot.img start $(date +%H:%M:%S)**********"
    rm -rf ${BOOT_TMP_IMAGE}/*
    boot_mkargs=$(${PYTHON_TOOL} ${UNPACK_BOOTIMG_TOOL} --boot_img ${ORIGIN_IMAGE}/boot.img --out ${BOOT_TMP_IMAGE} --format=mkbootimg)
    cp ${KERNEL_IMG}/Image.lz4 ${BOOT_TMP_IMAGE}/kernel
    bash -c "${PYTHON_TOOL} ${MKBOOTIMG_PATH} ${boot_mkargs} -o ${IMAGE_OUT}/boot.img"
    sign_boot_image
    echo "**********rebuild boot.img end $(date +%H:%M:%S)**********"
}

rebuild_dtb_image() {
    echo "**********rebuild dtb.img start $(date +%H:%M:%S)**********"
    cp ${IMAGE_OUT}/dtb ${VENDOR_BOOT_TMP_IMAGE}/origin/
    echo "**********rebuild dtb.img end $(date +%H:%M:%S)**********"
}

vendor_boot_modules_all_update() {

    echo "vendor_boot module update"

    mkdir -p ${VENDOR_BOOT_TMP_IMAGE}/dist/
    mkdir -p ${VENDOR_BOOT_TMP_IMAGE}/tmp/

    mv ${VENDOR_BOOT_TMP_IMAGE}/ramdisk00/lib/modules/modules.load \
       ${VENDOR_BOOT_TMP_IMAGE}/ramdisk00/lib/modules/modules.load_bak

    cp ${ACKDIR}/oplus/prebuild/vendor_boot.load \
       ${VENDOR_BOOT_TMP_IMAGE}/ramdisk00/lib/modules/modules.load

    ko_list=`cat ${VENDOR_BOOT_TMP_IMAGE}/ramdisk00/lib/modules/modules.load | xargs -L 1 basename`

    for ko in  $ko_list
    do
        current=`find ${VENDOR_MODULES_DIR} -name ${ko}`
        if [ -n "${current}" ]; then
            cp ${current} ${VENDOR_BOOT_TMP_IMAGE}/dist/
            ${STRIP} -S ${VENDOR_BOOT_TMP_IMAGE}/dist/${ko} -o ${VENDOR_BOOT_TMP_IMAGE}/tmp/${ko}
            cp ${VENDOR_BOOT_TMP_IMAGE}/tmp/${ko} ${VENDOR_BOOT_TMP_IMAGE}/ramdisk00/lib/modules/
        fi
    done
}

rebuild_vendor_boot_image() {
    echo "**********rebuild vendor_boot.img start $(date +%H:%M:%S)**********"
    rm -rf ${VENDOR_BOOT_TMP_IMAGE}/*
    boot_mkargs=$(${PYTHON_TOOL} ${UNPACK_BOOTIMG_TOOL} --boot_img ${ORIGIN_IMAGE}/vendor_boot.img --out ${VENDOR_BOOT_TMP_IMAGE}/origin --format=mkbootimg)
    rebuild_dtb_image
    index="00"
    for index in  $index
    do
        echo " index  $index "
        mv ${VENDOR_BOOT_TMP_IMAGE}/origin/vendor_ramdisk${index} ${VENDOR_BOOT_TMP_IMAGE}/vendor_ramdisk${index}.lz4
        #touch ${VENDOR_BOOT_TMP_IMAGE}/vendor_ramdisk${index}
        ${LZ4} -d -f ${VENDOR_BOOT_TMP_IMAGE}/vendor_ramdisk${index}.lz4 ${VENDOR_BOOT_TMP_IMAGE}/vendor_ramdisk${index}
        rm ${VENDOR_BOOT_TMP_IMAGE}/vendor_ramdisk${index}.lz4
        mkdir -p ${VENDOR_BOOT_TMP_IMAGE}/ramdisk${index}
        mv ${VENDOR_BOOT_TMP_IMAGE}/vendor_ramdisk${index} ${VENDOR_BOOT_TMP_IMAGE}/ramdisk${index}/vendor_ramdisk${index}
        pushd  ${VENDOR_BOOT_TMP_IMAGE}/ramdisk${index}
        ${CPIO} -idu < ${VENDOR_BOOT_TMP_IMAGE}/ramdisk${index}/vendor_ramdisk${index}

        popd
        rm ${VENDOR_BOOT_TMP_IMAGE}/ramdisk${index}/vendor_ramdisk${index}

        vendor_boot_modules_all_update
        ${MKBOOTFS} ${VENDOR_BOOT_TMP_IMAGE}/ramdisk${index} > ${VENDOR_BOOT_TMP_IMAGE}/vendor_ramdisk${index}
        #touch ${VENDOR_BOOT_TMP_IMAGE}/vendor_ramdisk${index}.lz4
        ${LZ4} -l -f -12 --favor-decSpeed ${VENDOR_BOOT_TMP_IMAGE}/vendor_ramdisk${index} ${VENDOR_BOOT_TMP_IMAGE}/vendor_ramdisk${index}.lz4
        mv ${VENDOR_BOOT_TMP_IMAGE}/vendor_ramdisk${index}.lz4 ${VENDOR_BOOT_TMP_IMAGE}/origin/vendor_ramdisk${index}
        rm ${VENDOR_BOOT_TMP_IMAGE}/vendor_ramdisk${index}
    done
    bash -c "${PYTHON_TOOL} ${MKBOOTIMG_PATH} ${boot_mkargs} --vendor_boot ${IMAGE_OUT}/vendor_boot.img"
    sign_vendor_boot_image
    echo "**********rebuild vendor_boot.img end $(date +%H:%M:%S)**********"
}

rebuild_vendor_dlkm_image_ext() {
    echo "**********rebuild vendor_dlkm.img start $(date +%H:%M:%S)**********"
    mkdir -p ${VENDOR_DLKM_TMP_IMAGE}
    ${SIMG2IMG} ${ORIGIN_IMAGE}/vendor_dlkm.img  ${VENDOR_DLKM_TMP_IMAGE}/vendor_dlkm.img
    ${TOOLS}/7z_new ${VENDOR_DLKM_TMP_IMAGE}/vendor_dlkm.img ${VENDOR_DLKM_TMP_IMAGE}/vendor_dlkm_out
    ${BUILD_IMAGE} ${VENDOR_DLKM_TMP_IMAGE}/vendor_dlkm_out ${TOOLS}/vendor_dlkm_image_info.txt ${VENDOR_DLKM_TMP_IMAGE}/vendor_dlkm.img /dev/null
    echo "**********rebuild vendor_dlkm.img end $(date +%H:%M:%S)**********"
}

vendor_dlkm_modules_update() {
    echo "vendor_dlkm module update"

    mkdir -p ${VENDOR_DLKM_TMP_IMAGE}/dist/
    mkdir -p ${VENDOR_DLKM_TMP_IMAGE}/tmp/
    mv ${VENDOR_DLKM_TMP_IMAGE}/vendor_dlkm/lib/modules/modules.load \
       ${VENDOR_DLKM_TMP_IMAGE}/vendor_dlkm/lib/modules/modules.load_bak

    cp ${ACKDIR}/oplus/prebuild/vendor_dlkm.load \
       ${VENDOR_DLKM_TMP_IMAGE}/vendor_dlkm/lib/modules/modules.load

    ko_list=`cat ${VENDOR_DLKM_TMP_IMAGE}/vendor_dlkm/lib/modules/modules.load | xargs -L 1 basename`
    for ko in  $ko_list
    do
        #find ${VENDOR_MODULES_DIR} -name ${ko} | xargs cp -t ${VENDOR_DLKM_TMP_IMAGE}/dist/
        current=`find ${VENDOR_MODULES_DIR} -name ${ko}`
        if [ -n "${current}" ]; then
            cp ${current} ${VENDOR_DLKM_TMP_IMAGE}/dist/
            ${STRIP} -S ${VENDOR_DLKM_TMP_IMAGE}/dist/${ko} -o ${VENDOR_DLKM_TMP_IMAGE}/tmp/${ko}
            cp ${VENDOR_DLKM_TMP_IMAGE}/tmp/${ko} ${VENDOR_DLKM_TMP_IMAGE}/vendor_dlkm/lib/modules/
        fi
    done
}

rebuild_vendor_dlkm_config() {
   sed  '/vendor_dlkm_selinux_fc*/d;/block_list*/d' ${TOOLS}/vendor_dlkm_image_info.txt > ${TOOLS}/local_vendor_dlkm_image_info.txt
   echo vendor_dlkm_selinux_fc=${ORIGIN_IMAGE}/file_contexts.bin >> ${TOOLS}/local_vendor_dlkm_image_info.txt
   echo block_list=${VENDOR_DLKM_TMP_IMAGE}/vendor_dlkm.map >> ${TOOLS}/local_vendor_dlkm_image_info.txt
}

rebuild_vendor_dlkm_image() {
    echo "**********rebuild vendor_dlkm.img start $(date +%H:%M:%S)**********"
    rm -rf ${VENDOR_DLKM_TMP_IMAGE}/*
    mkdir -p ${VENDOR_DLKM_TMP_IMAGE}
    ${SIMG2IMG} ${ORIGIN_IMAGE}/vendor_dlkm.img  ${VENDOR_DLKM_TMP_IMAGE}/vendor_dlkm.img
    #rebuild_vendor_dlkm_image_ext
    python3  ${TOOLS}/unpack_image.py -i "${VENDOR_DLKM_TMP_IMAGE}/vendor_dlkm.img" \
    -o "${VENDOR_DLKM_TMP_IMAGE}/vendor_dlkm" -z ${TOOLS}/7z_new -r ${TOOLS}/erofsfuse \
    -m ${VENDOR_DLKM_TMP_IMAGE}/mnt

    #${TOOLS}/erofs_unpack.sh ${VENDOR_DLKM_TMP_IMAGE}/vendor_dlkm.img  ${VENDOR_DLKM_TMP_IMAGE}/mnt ${VENDOR_DLKM_TMP_IMAGE}/vendor_dlkm
    vendor_dlkm_modules_update
    touch ${VENDOR_DLKM_TMP_IMAGE}/vendor_dlkm/lib/modules/readme.txt
    echo $(date +"%Y_%m_%d_%H_%M_%S") >${VENDOR_DLKM_TMP_IMAGE}/vendor_dlkm/lib/modules/readme.txt
    rebuild_vendor_dlkm_config
    PATH=${BIN}/:$PATH \
        ${TOOLS}/bin/build_image \
        ${VENDOR_DLKM_TMP_IMAGE}/vendor_dlkm \
        ${TOOLS}/local_vendor_dlkm_image_info.txt \
        ${VENDOR_DLKM_TMP_IMAGE}/vendor_dlkm.img \
        ${VENDOR_DLKM_TMP_IMAGE}/

    cp ${VENDOR_DLKM_TMP_IMAGE}/vendor_dlkm.img ${IMAGE_OUT}/vendor_dlkm.img
    sign_vendor_dlkm_image
    echo "**********rebuild vendor_dlkm.img end $(date +%H:%M:%S)**********"
}

rebuild_system_dlkm_config() {
   sed  '/system_dlkm_selinux_fc*/d;/block_list*/d' ${TOOLS}/system_dlkm_image_info.txt > ${TOOLS}/local_system_dlkm_image_info.txt
   echo system_dlkm_selinux_fc=${ORIGIN_IMAGE}/file_contexts.bin >> ${TOOLS}/local_system_dlkm_image_info.txt
   echo block_list=${SYSTEM_DLKM_TMP_IMAGE}/system_dlkm.map >> ${TOOLS}/local_system_dlkm_image_info.txt
}

system_dlkm_modules_update() {
    echo "system_dlkm module update"

    mkdir -p ${SYSTEM_DLKM_TMP_IMAGE}/dist/
    mkdir -p ${SYSTEM_DLKM_TMP_IMAGE}/tmp/
    ko_list=`cat ${SYSTEM_DLKM_TMP_IMAGE}/system_dlkm/lib/modules/modules.load | xargs -L 1 basename`
    for ko in  $ko_list
    do
        #find ${SYSTEM_MODULES_DIR} -name ${ko} | xargs cp -t ${SYSTEM_DLKM_TMP_IMAGE}/system_dlkm/lib/modules/
        current=`find ${SYSTEM_MODULES_DIR} -name ${ko}`
        if [ -n "${current}" ]; then
           cp ${current} ${SYSTEM_DLKM_TMP_IMAGE}/dist/
           ${STRIP} -S ${SYSTEM_DLKM_TMP_IMAGE}/dist/${ko} -o ${SYSTEM_DLKM_TMP_IMAGE}/tmp/${ko}
           cp ${SYSTEM_DLKM_TMP_IMAGE}/tmp/${ko} ${SYSTEM_DLKM_TMP_IMAGE}/system_dlkm/lib/modules/
        fi
    done
}

rebuild_system_dlkm_image() {
    echo "**********rebuild system_dlkm.img start $(date +%H:%M:%S)**********"
    rm -rf ${SYSTEM_DLKM_TMP_IMAGE}/*
    mkdir -p ${SYSTEM_DLKM_TMP_IMAGE}
    ${SIMG2IMG} ${ORIGIN_IMAGE}/system_dlkm.img  ${SYSTEM_DLKM_TMP_IMAGE}/system_dlkm.img
    python3  ${TOOLS}/unpack_image.py -i ${SYSTEM_DLKM_TMP_IMAGE}/system_dlkm.img \
    -o ${SYSTEM_DLKM_TMP_IMAGE}/system_dlkm -z ${TOOLS}/7z_new -r ${TOOLS}/erofsfuse \
    -m ${SYSTEM_DLKM_TMP_IMAGE}/mnt

    #${TOOLS}/erofs_unpack.sh ${SYSTEM_DLKM_TMP_IMAGE}/system_dlkm.img  ${SYSTEM_DLKM_TMP_IMAGE}/mnt ${SYSTEM_DLKM_TMP_IMAGE}/system_dlkm
    system_dlkm_modules_update
    rebuild_system_dlkm_config
    PATH=${BIN}/:$PATH \
        ${TOOLS}/bin/build_image \
        ${SYSTEM_DLKM_TMP_IMAGE}/system_dlkm \
        ${TOOLS}/local_system_dlkm_image_info.txt \
        ${SYSTEM_DLKM_TMP_IMAGE}/system_dlkm.img \
        ${SYSTEM_DLKM_TMP_IMAGE}/
    cp ${SYSTEM_DLKM_TMP_IMAGE}/system_dlkm.img ${IMAGE_OUT}/system_dlkm.img
    sign_system_dlkm_image
    echo "**********rebuild system_dlkm.img end $(date +%H:%M:%S)**********"
}

rebuild_dtbo_image() {
    echo "**********rebuild dtbo.img start $(date +%H:%M:%S)**********"
    #cp ${ORIGIN_IMAGE}/dtbo.img ${IMAGE_OUT}/dtbo.img
    sign_dtbo_image
    echo "**********rebuild dtbo.img end $(date +%H:%M:%S)**********"
}

rebuild_vbmeta_image() {
    echo "**********rebuild vbmeta.image start $(date +%H:%M:%S)**********"
    if test "$IS_INTRANET" = "yes"; then
        if [ -d "${SIGN_OUT}" ]; then
            ${TOOLS}/sign_image ${SIGN_OUT}/ ${IMAGE_SERVER}
        fi
    fi
    echo "**********rebuild vbmeta.image end $(date +%H:%M:%S)**********"
}

build_start_time
is_intranet
download_prebuild_image
get_image_info
get_modules_list
rebuild_boot_image
rebuild_vendor_boot_image
rebuild_dtbo_image
rebuild_vendor_dlkm_image
rebuild_system_dlkm_image
rebuild_vbmeta_image
print_end_help
build_end_time