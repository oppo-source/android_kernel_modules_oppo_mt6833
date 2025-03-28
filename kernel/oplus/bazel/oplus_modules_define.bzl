load("//build/bazel_mgk_rules:mgk_ddk_ko.bzl", "define_mgk_ddk_ko")
load("//build/kernel/oplus:oplus_modules_list.bzl", "oplus_ddk_get_oplus_features")
load("@mgk_info//:dict.bzl","DEFCONFIG_OVERLAYS", "OPLUS_DEFCONFIG_OVERLAYS")

bazel_support_platform = "mtk"
bazel_wifionly = "wifionly" if "wifionly.config" in DEFCONFIG_OVERLAYS else None

def oplus_ddk_get_targets():
    return [OPLUS_DEFCONFIG_OVERLAYS]


def define_oplus_ddk_module(
    name,
    srcs = None,
    header_deps = [],
    ko_deps = [],
    hdrs = None,
    includes = None,
    conditional_srcs = None,
    conditional_defines = None,
    linux_includes = None,
    out = None,
    local_defines = None,
    copts = None,
    conditional_build = None):

    # 从编译中移除编译条件不满足的模块
    if conditional_build:
        # 对来自环境变量的OPLUS_FEATURES进行解码
        oplus_feature_list = oplus_ddk_get_oplus_features()

        skip = 0
        for k in conditional_build:
            v = conditional_build[k]
            sv1 = str(v).upper()
            sv2 = str(oplus_feature_list.get(k, 'foo')).upper()
            if sv1 != sv2:
                skip += 1

        if skip > 0:
            print("Remove: compilation conditions are not met in %s" % name)
            return
        else:
            print("Added: compilation conditions are met in %s" % name)

    flattened_conditional_defines = None
    if conditional_defines:
        for config_vendor, config_defines in conditional_defines.items():
            if config_vendor == bazel_support_platform:
                if flattened_conditional_defines:
                    flattened_conditional_defines = flattened_conditional_defines + config_defines
                else:
                    flattened_conditional_defines = config_defines

    if flattened_conditional_defines:
        if local_defines:
            local_defines =  local_defines + flattened_conditional_defines
        else:
            local_defines = flattened_conditional_defines

    #fail("debug need variable {}".format(local_defines))

    define_mgk_ddk_ko(
        name,
        srcs,
        header_deps,
        ko_deps,
        hdrs,
        includes,
        conditional_srcs,
        linux_includes,
        out,
        local_defines,
        copts
    )

