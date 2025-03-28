load("//build/kernel/kleaf:kernel.bzl", "ddk_headers")
load("//build/kernel/oplus:oplus_modules_define.bzl", "define_oplus_ddk_module")
load("//build/kernel/oplus:oplus_modules_dist.bzl", "ddk_copy_to_dist_dir")

def define_oplus_local_modules():

#    define_oplus_ddk_module(
#        name = "oplus_bsp_memleak_detect_simple",
#        srcs = native.glob([
#            "**/*.h",
#            "memleak_detect/slub_track_simple.c",
#            "memleak_detect/vmalloc_track_simple.c",
#        ]),
#        includes = ["."],
#        )
#
    define_oplus_ddk_module(
        name = "oplus_bsp_hybridswap_zram",
        conditional_srcs = {
            "CONFIG_CONT_PTE_HUGEPAGE": {
                True: ["hybridswap_zram/hybridswap/hybridswapd_chp.c"],
            }
        },

        srcs = native.glob([
            "**/*.h",
            "hybridswap_zram/zcomp.c",
            "hybridswap_zram/zram_drv.c",
            "hybridswap_zram/hybridswap/hybridmain.c",
            "hybridswap_zram/hybridswap/hybridswapd.c",
            "hybridswap_zram/hybridswap/hybridswap.c",
        ]),
        ko_deps = [
            "//vendor/oplus/kernel/mm:oplus_bsp_zsmalloc",
        ],
        includes = ["."],
        local_defines = ["CONFIG_HYBRIDSWAP","CONFIG_HYBRIDSWAP_SWAPD","CONFIG_HYBRIDSWAP_CORE","CONFIG_CRYPTO_LZ4K","CONFIG_CRYPTO_ZSTDN"],
        conditional_defines = {
             "qcom":  ["CONFIG_QCOM_PANEL_EVENT_NOTIFIER"],
             "mtk":  ["CONFIG_OPLUS_MTK_DRM_GKI_NOTIFY"],
        },
    )

    define_oplus_ddk_module(
        name = "oplus_bsp_zsmalloc",
        conditional_srcs = {
            "CONFIG_CONT_PTE_HUGEPAGE": {
                True: ["thp_zsmalloc/thp_zsmalloc.c"],
            }
        },

        srcs = native.glob([
            "**/*.h",
            "thp_zsmalloc/zsmalloc.c",
        ]),
        includes = ["."],
    )

    define_oplus_ddk_module(
        name = "oplus_bsp_sigkill_diagnosis",
        srcs = native.glob([
            "sigkill_diagnosis/sigkill_diagnosis.c",
        ]),
        includes = ["."],
    )

    define_oplus_ddk_module(
        name = "oplus_bsp_zram_opt",
        srcs = native.glob([
            "**/*.h",
            "zram_opt/zram_opt.c",
        ]),
        includes = ["."],
        ko_deps = [
            "//vendor/oplus/kernel/mm:oplus_bsp_hybridswap_zram",
        ],
        local_defines = ["CONFIG_DYNAMIC_TUNING_SWAPPINESS", "CONFIG_OPLUS_BALANCE_ANON_FILE_RECLAIM", "CONFIG_HYBRIDSWAP_SWAPD"],
        copts = select({
            "//build/kernel/kleaf:kocov_is_true": ["-fprofile-arcs", "-ftest-coverage"],
            "//conditions:default": [],
        }),
    )

    define_oplus_ddk_module(
        name = "oplus_bsp_proactive_compact",
        srcs = native.glob([
            "**/*.h",
            "proactive_compact/proactive_compact.c",
        ]),
        includes = ["."],
    )

#    define_oplus_ddk_module(
#        name = "oplus_bsp_lz4k",
#        srcs = native.glob([
#            "**/*.h",
#            "hybridswap_zram/lz4k/lz4k.c",
#            "hybridswap_zram/lz4k/lz4k_compress.c",
#            "hybridswap_zram/lz4k/lz4k_decompress.c",
#        ]),
#        includes = ["."],
#        local_defines = ["CONFIG_CRYPTO_LZ4K"],
#        )
#
    define_oplus_ddk_module(
        name = "oplus_bsp_kshrink_slabd",
        srcs = native.glob([
            "**/*.h",
            "async_reclaim_opt/kshrink_slabd/kshrink_slabd.c",
        ]),
        includes = ["."],
    )

    define_oplus_ddk_module(
        name = "oplus_bsp_uxmem_opt",
        srcs = native.glob([
            "**/*.h",
            "uxmem_opt/uxmem_opt.c",
        ]),
        includes = ["."],
        local_defines = ["CONFIG_OPLUS_FEATURE_UXMEM_OPT"],
    )

    define_oplus_ddk_module(
        name = "oplus_bsp_dynamic_readahead",
        srcs = native.glob([
            "**/*.h",
            "dynamic_readahead/dynamic_readahead.c",
        ]),
        includes = ["."],
        local_defines = ["CONFIG_OPLUS_FEATURE_DYNAMIC_READAHEAD"],
    )

    define_oplus_ddk_module(
        name = "oplus_bsp_pcppages_opt",
        srcs = native.glob([
            "**/*.h",
            "async_reclaim_opt/pcppages_opt/pcppages_opt.c",
        ]),
        includes = ["."],
    )

    define_oplus_ddk_module(
        name = "oplus_bsp_kswapd_opt",
        srcs = native.glob([
            "kswapd_opt/kswapd_opt.c",
        ]),
        includes = ["."],
        local_defines = ["CONFIG_OPLUS_FEATURE_KSWAPD_OPT"],
        conditional_defines = {
            "mtk": ["CONFIG_COSTLY_ALLOC_MASK_RECLAIM"],
        },
        copts = select({
            "//build/kernel/kleaf:kocov_is_true": ["-fprofile-arcs", "-ftest-coverage"],
            "//conditions:default": [],
        }),
    )

    define_oplus_ddk_module(
        name = "oplus_bsp_memleak_detect",
        srcs = native.glob([
            "**/*.h",
            "memleak_detect/slub_track.c",
            "memleak_detect/vmalloc_track.c",
            "memleak_detect/memleak_debug_stackdepot.c"
        ]),
        includes = ["."],
        )

    define_oplus_ddk_module(
        name = "oplus_bsp_zstdn",
        srcs = native.glob([
            "**/*.h",
            "hybridswap_zram/zstd/crypto_zstd.c",
        ]),
        includes = ["."],
        local_defines = ["CONFIG_CRYPTO_ZSTDN"],
        )

    ddk_copy_to_dist_dir(
        name = "oplus_bsp_mm",
        module_list = [
#            "oplus_bsp_memleak_detect_simple",
            "oplus_bsp_sigkill_diagnosis",
            "oplus_bsp_zram_opt",
            "oplus_bsp_proactive_compact",
            "oplus_bsp_hybridswap_zram",
            "oplus_bsp_zsmalloc",
#            "oplus_bsp_lz4k",
            "oplus_bsp_kshrink_slabd",
            "oplus_bsp_uxmem_opt",
            "oplus_bsp_dynamic_readahead",
            "oplus_bsp_pcppages_opt",
            "oplus_bsp_kswapd_opt",
#            "oplus_bsp_look_around",
            "oplus_bsp_memleak_detect",
            "oplus_bsp_zstdn",
        ],
    )
