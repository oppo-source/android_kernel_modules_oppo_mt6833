load("//build/kernel/kleaf:kernel.bzl", "ddk_headers")
load("//build/kernel/oplus:oplus_modules_define.bzl", "define_oplus_ddk_module")
load("//build/kernel/oplus:oplus_modules_dist.bzl", "ddk_copy_to_dist_dir")

def define_oplus_local_modules():

    define_oplus_ddk_module(
        name = "oplus_network_nfc_sn_ese",
        srcs = native.glob([
            "**/*.h",
            "sn_nci/ese/p73.c",
        ]),
        includes = ["."],
        ko_deps = [
            "//vendor/oplus/kernel/nfc:oplus_network_nfc_i2c",
        ],
    )
    define_oplus_ddk_module(
        name = "oplus_network_nfc_pn557_i2c",
        srcs = native.glob([
            "**/*.h",
            "nq330/nq330.c",
        ]),
        includes = ["."],
        copts = [
            "-DCONFIG_NFC_PN553_DEVICES",
        ],
    )

    define_oplus_ddk_module(
        name = "oplus_network_nfc_i2c",
        srcs = native.glob([
            "**/*.h",
            "sn_nci/nfc/common.c",
            "sn_nci/nfc/common_ese.c",
            "sn_nci/nfc/i2c_drv.c",
        ]),
        includes = ["."],
    )

    define_oplus_ddk_module(
        name = "oplus_nfc",
        srcs = native.glob([
            "**/*.h",
            "oplus_nfc/oplus_nfc.c",
        ]),
        includes = ["."],
        copts = [
            "-DCONFIG_OPLUS_NFC",
        ],
    )

    define_oplus_ddk_module(
        name = "oplus_network_nfc_thn31",
        srcs = native.glob([
            "**/*.h",
            "thn31/tms_common.c",
            "thn31/nfc/nfc_common.c",
            "thn31/nfc/nfc_driver.c",
            "thn31/ese/ese_common.c",
            "thn31/ese/ese_driver.c",
        ]),
        includes = ["."],
        copts = [
            "-DCONFIG_TMS_NFC_DEVICE",
            "-DCONFIG_TMS_ESE_DEVICE",
        ],
    )

    ddk_copy_to_dist_dir(
        name = "oplus_network_nfc",
        module_list = [
            "oplus_network_nfc_sn_ese",
            "oplus_network_nfc_i2c",
            "oplus_network_nfc_thn31",
            "oplus_network_nfc_pn557_i2c",
            "oplus_nfc",
        ],
    )
