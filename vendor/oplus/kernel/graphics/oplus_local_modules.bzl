load("//build/kernel/kleaf:kernel.bzl", "ddk_headers")
load("//build/kernel/oplus:oplus_modules_define.bzl", "define_oplus_ddk_module")
load("//build/kernel/oplus:oplus_modules_dist.bzl", "ddk_copy_to_dist_dir")

def define_oplus_local_modules():

    define_oplus_ddk_module(
        name = "oplus_sync_fence",
        srcs = native.glob([
                "oplus_sync_fence.c",
                "*.h",
        ]),
        includes = ["."],
		local_defines = [
		"CONFIG_OPLUS_SYNC_FENCE",
	],
    )
    ddk_copy_to_dist_dir(
        name = "oplus_sync_fence",
        module_list = [
			"oplus_sync_fence",
        ],
    )
