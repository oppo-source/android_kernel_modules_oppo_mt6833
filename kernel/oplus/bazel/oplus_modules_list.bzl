# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2022 The Android Open Source Project

"""
This module contains a full list of kernel modules
 compiled by GKI.
"""
"""
Format：[PATH:KO_NAME],
"""

load("@mgk_info//:dict.bzl","OPLUS_FEATURES")

load("@mgk_info//:dict.bzl",
    "DEFCONFIG_OVERLAYS",
)

COMMON_OPLUS_MODULES_LIST = [
    # keep sorted
    #"//vendor/oplus/kernel/boot:oplus_bsp_boot_projectinfo",
    #"//vendor/oplus/kernel/boot:buildvariant",
    #"//vendor/oplus/kernel/boot:oplusboot",
    #"//vendor/oplus/kernel/boot:oplus_ftm_mode",
    #"//vendor/oplus/kernel/boot:cdt_integrity",
    #"//vendor/oplus/kernel/boot:boot_mode",
    #"//vendor/oplus/kernel/tp/hbp/hbp:oplus_hbp_core",
    #"//vendor/oplus/kernel/tp/hbp/hbp:oplus_ft3683g",
    "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_syna_common",
    "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_tcm_S3910",
    "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_td4377_noflash",
    "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_ft3681",
    "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_ft3658u_spi",
    "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_ft3683g",
    "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_ft3518",
    "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_focal_common",
    "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_custom",
    "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_common",
    "//vendor/oplus/kernel/touchpanel/synaptics_hbp:oplus_bsp_synaptics_tcm2",
    "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_nt36672c_noflash",
    "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_nt36528_noflash",
    "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_novatek_common",
    "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_ilitek7807s",
    "//vendor/oplus/kernel/touchpanel/oplus_touchscreen_v2:oplus_bsp_tp_ilitek_common",
    "//vendor/oplus/kernel/nfc:oplus_network_nfc_sn_ese",
    "//vendor/oplus/kernel/nfc:oplus_network_nfc_i2c",
    "//vendor/oplus/kernel/nfc:oplus_network_nfc_pn557_i2c",
    "//vendor/oplus/kernel/nfc:oplus_network_nfc_thn31",
    "//vendor/oplus/kernel/nfc:oplus_nfc",
    "//vendor/oplus/sensor/kernel/sensorhub:oplus_sensor_ir_core",
    "//vendor/oplus/sensor/kernel/sensorhub:oplus_sensor_kookong_ir_pwm",
    "//vendor/oplus/kernel/device_info/tri_state_key:oplus_bsp_tri_key",
    "//vendor/oplus/kernel/device_info/tri_state_key:oplus_ak09970",
    "//vendor/oplus/kernel/device_info/magnetic_cover:oplus_magnetic_cover",
    "//vendor/oplus/kernel/device_info/magnetic_cover:oplus_magcvr_ak09973",
    "//vendor/oplus/kernel/device_info/magnetic_cover:oplus_magcvr_mxm1120",
    #"//vendor/oplus/kernel/device_info/tri_state_key:oplus_bsp_mxm_up",
    #"//vendor/oplus/kernel/device_info/tri_state_key:oplus_bsp_mxm_down",
    #"//vendor/oplus/kernel/device_info/tri_state_key:oplus_bsp_ist_up",
    #"//vendor/oplus/kernel/device_info/tri_state_key:oplus_bsp_ist_down",
    "//vendor/oplus/kernel/camera:oplus_camera_aw37004dnr_regulator",
    "//vendor/oplus/kernel/camera:oplus_camera_wl28681c_regulator",
    "//vendor/oplus/kernel/camera:oplus_camera_tof8801",
    "//vendor/oplus/kernel/camera:oplus_camera_ak7377m",
    "//vendor/oplus/kernel/camera:oplus_camera_ak7316m",
    "//vendor/oplus/kernel/camera:oplus_camera_ak7316t",
    "//vendor/oplus/kernel/camera:oplus_camera_dw9800s_24613",
    "//vendor/oplus/kernel/camera:oplus_camera_jd5516w_24720",
    "//vendor/oplus/kernel/camera:oplus_camera_jd5516w",
    "//vendor/oplus/kernel/camera:oplus_camera_dw9786",
    "//vendor/oplus/kernel/camera:oplus_camera_ois_dw9786",
    "//vendor/oplus/kernel/camera:oplus_camera_ois_power",
    "//vendor/oplus/kernel/camera:oplus_camera_aw36515_brza",
    "//vendor/oplus/kernel/cpu/game_opt:oplus_bsp_game_opt",
    "//vendor/oplus/kernel/cpu/freqqos_monitor:oplus_freqqos_monitor",
    #"//vendor/oplus/kernel/cpu/freqqos_monitor:oplus_freqqos_monitor",
    "//vendor/oplus/kernel/hans:oplus_sys_hans",
    "//vendor/oplus/kernel/cpu/thermal:horae_shell_temp",
    "//vendor/oplus/kernel/cpu/thermal:thermal_pa_adc",
    "//vendor/oplus/kernel/cpu/midas:oplus_bsp_midas",
    #"//vendor/oplus/kernel/cpu/thermal:oplus_ipa_thermal",
    "//vendor/oplus/secure/biometrics/fingerprints/bsp/uff/driver:oplus_bsp_uff_fp_driver",
    "//vendor/oplus/secure/common/bsp/drivers/oplus_secure_common:oplus_secure_common",
    "//vendor/oplus/kernel/secureguard/gki2.0/rootguard_new:oplus_secure_guard_new",
    "//vendor/oplus/sensor/kernel/sensorhub/oplus_sensor_devinfo:oplus_sensor_deviceinfo",
    "//vendor/oplus/sensor/kernel/sensorhub/oplus_sensor_feedback:oplus_sensor_feedback",
    "//vendor/oplus/kernel/network:oplus_network_data_module",
    "//vendor/oplus/kernel/network:oplus_network_linkpower_module",
    "//vendor/oplus/kernel/network:oplus_network_app_monitor",
    "//vendor/oplus/kernel/network:oplus_network_dns_hook",
    "//vendor/oplus/kernel/network:oplus_network_game_first",
    "//vendor/oplus/kernel/network:oplus_network_qr_scan",
    "//vendor/oplus/kernel/network:oplus_network_score",
    "//vendor/oplus/kernel/network:oplus_network_stats_calc",
    "//vendor/oplus/kernel/network:oplus_network_vnet",
    "//vendor/oplus/kernel/network:oplus_network_tuning",
    "//vendor/oplus/kernel/mm:oplus_bsp_sigkill_diagnosis",
    "//vendor/oplus/kernel/mm:oplus_bsp_kswapd_opt",
    "//vendor/oplus/kernel/mm:oplus_bsp_uxmem_opt",
    "//vendor/oplus/kernel/mm:oplus_bsp_dynamic_readahead",
    "//vendor/oplus/kernel/mm:oplus_bsp_proactive_compact",
    "//vendor/oplus/kernel/mm:oplus_bsp_zsmalloc",
    "//vendor/oplus/kernel/mm:oplus_bsp_hybridswap_zram",
    "//vendor/oplus/kernel/mm:oplus_bsp_zstdn",
    #"//vendor/oplus/kernel/mm:oplus_bsp_memleak_detect_simple",
    "//vendor/oplus/kernel/synchronize:oplus_locking_strategy",
    "//vendor/oplus/kernel/mm:oplus_bsp_pcppages_opt",
    "//vendor/oplus/kernel/mm:oplus_bsp_zram_opt",
    #"//vendor/oplus/kernel/mm:oplus_bsp_look_around",
    "//vendor/oplus/kernel/mm:oplus_bsp_kshrink_slabd",
    "//vendor/oplus/kernel/synchronize:oplus_lock_torture",
    "//vendor/oplus/kernel/wifi:oplus_connectivity_routerboost",
    "//vendor/oplus/kernel/wifi:oplus_connectivity_sla",
    "//vendor/oplus/kernel/wifi:oplus_wificapcenter",
    "//vendor/oplus/kernel/wifi:oplus_wifi_wsa",
    "//vendor/oplus/kernel/dfr:oplus_bsp_dfr_combkey_monitor",
    "//vendor/oplus/kernel/mm:oplus_bsp_memleak_detect",
    "//vendor/oplus/kernel/dfr:oplus_bsp_dfr_hung_task_enhance",
    "//vendor/oplus/kernel/dfr:oplus_bsp_dfr_init_watchdog",
    "//vendor/oplus/kernel/dfr:oplus_bsp_dfr_keyevent_handler",
    "//vendor/oplus/kernel/dfr:oplus_bsp_dfr_last_boot_reason",
    "//vendor/oplus/kernel/dfr:oplus_bsp_dfr_fdleak_check",
    #"//vendor/oplus/kernel/dfr:oplus_bsp_dfr_oplus_saupwk",
    "//vendor/oplus/kernel/dfr:oplus_bsp_dfr_shutdown_detect",
    "//vendor/oplus/kernel/dfr:oplus_bsp_dfr_theia",
    "//vendor/oplus/kernel/dfr:oplus_bsp_dfr_force_shutdown",
    "//vendor/oplus/kernel/dfr:oplus_bsp_dfr_ubt",
    "//vendor/oplus/kernel/dfr:oplus_bsp_dfr_pmic_monitor",
    "//vendor/oplus/kernel/ipc:oplus_binder_strategy",
    "//vendor/oplus/sensor/kernel/sensorhub:pseudo_sensor",
    "//vendor/oplus/kernel/boot/oplus_phoenix:oplus_bsp_dfr_kmsg_wb",
    "//vendor/oplus/kernel/boot/oplus_phoenix:oplus_bsp_dfr_reboot_speed",
    "//vendor/oplus/kernel/boot/oplus_phoenix:oplus_bsp_dfr_phoenix",
    "//vendor/oplus/kernel/boot/oplus_phoenix:oplus_bsp_dfr_shutdown_speed",
    "//vendor/oplus/kernel/framework_stability/oplus_stability_helper:oplus_sys_stability_helper",
    "//vendor/oplus/kernel/dfr:oplus_inject",
    "//vendor/oplus/kernel/dfr:oplus_inject_aw8692x",
    "//vendor/oplus/kernel/graphics:oplus_sync_fence",
    "//vendor/oplus/kernel/oplus_performance_5.10/sdcardfs:sdcardfs",
]

"""
将以 OPLUS_FEATURE_ 打头的环境变量转换为字典
"""
def oplus_ddk_get_oplus_features():
   oplus_feature_list = {}
   for o in OPLUS_FEATURES.split(" "):
       lst = o.split('=')
       if len(lst) != 2:
           # print('info: environment variable [%s]' % o)
           continue
       oplus_feature_list[lst[0]] = lst[1]

   print("oplus_feature_list:", oplus_feature_list)
   return oplus_feature_list

"""
这个函数用于处理条件编译能力
条件需要与 define_oplus_ddk_module 中的条件一致
"""
def oplus_ddk_get_oplus_modules_list(
   name = "ddk_remove_from_dist",
   mods = {
       "pseudo_sensor":
           {"OPLUS_FEATURE_BSP_DRV_INJECT_TEST": "1"},

       "//vendor/oplus/sensor/kernel/sensorhub:pseudo_sensor":
           {"OPLUS_FEATURE_BSP_DRV_INJECT_TEST": "1"},

       "oplus_inject":
           {"OPLUS_FEATURE_BSP_DRV_INJECT_TEST": "1"},

       "oplus_inject_aw8692x":
           {"OPLUS_FEATURE_BSP_DRV_INJECT_TEST": "1"},
   }
):
   # 对来自环境变量的 OPLUS_FEATURES 进行解码
   oplus_feature_list = oplus_ddk_get_oplus_features()

   lst = []
   for item in COMMON_OPLUS_MODULES_LIST:
       # 根据全名或模块名进行匹配
       key1 = item
       key2 = "foofppfqq"
       if ":" in item:
           key2 = item.split(':')[1]

       # 获取比较条件
       if key1 in mods:
           conditional_build = mods[key1]
       elif key2 in mods:
           conditional_build = mods[key2]
       else:
           conditional_build = None

       if conditional_build:
           # 对条件进行处理
           build_me = 1
           for k in conditional_build:
               v = conditional_build[k]
               sv1 = str(v).upper()
               sv2 = str(oplus_feature_list.get(k, 'foo')).upper()
               if sv1 != sv2:
                   build_me = 0

           if build_me == 0:
               print("Remove: %s" % item)
           else:
               print("Add: %s" % item)
               lst.append(item)
       else:
           # 无需处理
           lst.append(item)

   return lst

def get_overlay_modules_list():
    if "mt6768_overlay.config" in DEFCONFIG_OVERLAYS:
        COMMON_OPLUS_MODULES_LIST.remove("//vendor/oplus/sensor/kernel/sensorhub/oplus_sensor_devinfo:oplus_sensor_deviceinfo")
        COMMON_OPLUS_MODULES_LIST.remove("//vendor/oplus/sensor/kernel/sensorhub/oplus_sensor_feedback:oplus_sensor_feedback")

get_overlay_modules_list()

