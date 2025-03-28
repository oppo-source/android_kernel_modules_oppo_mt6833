load("//build/bazel_common_rules/dist:dist.bzl", "copy_to_dist_dir")

def ddk_copy_to_dist_dir(
        name = None,
        module_list = [],
        conditional_builds = None):

    if name == None:
        name = "mtk_oplus_ddk"

