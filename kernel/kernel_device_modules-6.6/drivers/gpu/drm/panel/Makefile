# SPDX-License-Identifier: GPL-2.0
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat

ifneq ($(filter y m, $(CONFIG_MTK_MME_SUPPORT)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mme
endif

#ifdef OPLUS_FEATURE_DISPLAY
$(info build oplus_panel.ko, CONFIG_OPLUS_FEATURE_DISPLAY_MAINLINE=$(CONFIG_OPLUS_FEATURE_DISPLAY_MAINLINE))
ifneq ($(filter $(CONFIG_OPLUS_FEATURE_DISPLAY_MAINLINE), y m),)
ccflags-y += -DOPLUS_FEATURE_DISPLAY_MAINLINE
endif # filter CONFIG_OPLUS_FEATURE_DISPLAY_MAINLINE end
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mtk-interconnect \
		-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/slbc \
		-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat \
		-I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/dpc \
		-I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2 \
		-I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/oplus \
		-I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/oplus/oplus_panel \
		-I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/oplus/bias
#build oplus display feature config
ccflags-y += -DOPLUS_FEATURE_DISPLAY
ccflags-y += -DOPLUS_FEATURE_DISPLAY_TEMP_COMPENSATION
ccflags-y += -DOPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT
ccflags-y += -DOPLUS_FEATURE_DISPLAY_ADFR
ccflags-y += -DOPLUS_FEATURE_DISPLAY_HPWM
ccflags-y += -DOPLUS_FEATURE_DISPLAY_APOLLO
#build oplus_panel.ko
obj-$(CONFIG_DRM_PANEL_AC178_P_3_A0004_DSI_CMD) += ../oplus/oplus_panel/panel_ac178_p_3_a0004_dsi_cmd.o
obj-$(CONFIG_DRM_OPLUS_DISPLAY_DSI_PRIMARY) += ../oplus/oplus_panel/oplus_display_dsi_primary.o
obj-$(CONFIG_DRM_AC138_P_3_A0004_CMD_PANEL) += ../oplus/oplus_panel/ac138_p_3_a0004_cmd_panel.o
obj-$(CONFIG_DRM_AC112_P_7_A0009_FHD_DSI_VDO) += ../oplus/oplus_panel/ac112_p_7_a0009_fhd_dsi_vdo.o
obj-$(CONFIG_DRM_AC112_P_D_A0010_FHD_DSI_VDO) += ../oplus/oplus_panel/ac112_p_d_a0010_fhd_dsi_vdo.o
obj-$(CONFIG_DRM_AC112_P_D_A0010_FHD_DSI_VDO) += ../oplus/oplus_panel/ac114_p_3_a0013_hd_dsi_vdo.o
obj-$(CONFIG_DRM_PANEL_AC144_P_7_A0012_CMD_PANEL) += ../oplus/oplus_panel/ac144_p_7_a0012_cmd_panel.o
obj-$(CONFIG_DRM_PANEL_AC144_P_3_A0004_CMD_PANEL) += ../oplus/oplus_panel/ac144_p_3_a0004_cmd_panel.o
obj-$(CONFIG_DRM_PANEL_OPLUS22277_BOE_NT37705_FHD_DSI_CMD) += ../oplus/oplus_panel/oplus22277_boe_nt37705_fhd_dsi_cmd.o
obj-$(CONFIG_DRM_PANEL_AC168_P_B_A0012_CMD_PANEL) += ../oplus/oplus_panel/ac168_p_B_a0012_cmd_panel.o
obj-$(CONFIG_DRM_PANEL_AC168_P_3_A0004_CMD_PANEL) += ../oplus/oplus_panel/ac168_p_3_a0004_cmd_panel.o
obj-$(CONFIG_DRM_PANEL_AC102_P_7_A0007_DSC_VIDEO_MODE_PANEL) += ../oplus/oplus_panel/AC102_P_7_A0007_dsc_video_mode_panel.o
obj-$(CONFIG_DRM_PANEL_AC102_P_3_A0010_FHDP_DSI_VDO) += ../oplus/oplus_panel/AC102_P_3_A0010_fhdp_dsi_vdo.o
obj-$(CONFIG_DRM_PANEL_OPLUS22291_HLT_ILI9883C_DPHY_VDO) += ../oplus/oplus_panel/oplus22291_ili9883c_hlt_dsi_vdo.o
obj-$(CONFIG_DRM_PANEL_OPLUS22291_TRULY_TD4160_DPHY_VDO) += ../oplus/oplus_panel/oplus22291_td4160_truly_dsi_vdo.o
obj-$(CONFIG_DRM_PANEL_OPLUS22291_BOE_ILI9883C_DPHY_VDO) += ../oplus/oplus_panel/oplus22291_ili9883c_boe_dsi_vdo.o
obj-$(CONFIG_DRM_PANEL_OPLUS23053_BOE_ILI9883C_DPHY_VDO) += ../oplus/oplus_panel/oplus23053_ili9883c_boe_dsi_vdo.o
obj-$(CONFIG_DRM_PANEL_OPLUS23053_TRULY_TD4160_DPHY_VDO) += ../oplus/oplus_panel/oplus23053_td4160_truly_dsi_vdo.o
obj-$(CONFIG_DRM_PANEL_OPLUS23687_SAMSUNG_AMS667FK02_S6E8FC3_FHDP_DSI_VDO) += ../oplus/oplus_panel/oplus23687_samsung_ams667fk02_s6e8fc3_fhdp_dsi_vdo.o
obj-$(CONFIG_DRM_PANEL_OPLUS22610_SAMSUNG_AMS643AG01_1080P_DSI_CMD) += ../oplus/oplus_panel/oplus22610_samsung_ams643ag01_1080p_dsi_cmd.o
obj-$(CONFIG_DRM_PANEL_OPLUS22629_TIANMA_ILI7838A_T7_PLUS_FHD_DSI_CMD) += ../oplus/oplus_panel/oplus22629_tianma_ili7838a_t7_plus_fhd_dsi_cmd.o
obj-$(CONFIG_DRM_PANEL_OPLUS22612_TIANMA_ILI7838A_FHD_DSI_CMD) += ../oplus/oplus_panel/oplus22612_tianma_ili7838a_fhd_dsi_cmd.o
obj-$(CONFIG_DRM_PANEL_OPLUS22612_TIANMA_ILI7838A_T7_PLUS_FHD_DSI_CMD) += ../oplus/oplus_panel/oplus22612_tianma_ili7838a_t7_plus_fhd_dsi_cmd.o
obj-$(CONFIG_DRM_PANEL_ILI7807S_XXX_FHD_DSI_VDO_DPHY) += ../oplus/oplus_panel/ili7807s_xxx_fhd_dsi_vdo_dphy.o
obj-$(CONFIG_DRM_PANEL_ILI7807S_JDI_FHD_DSI_VDO_DPHY) += ../oplus/oplus_panel/ili7807s_jdi_fhd_dsi_vdo_dphy.o
obj-$(CONFIG_DRM_PANEL_NT36672C_TM_FHD_DSI_VDO_DPHY) += ../oplus/oplus_panel/nt36672c_tm_fhd_dsi_vdo_dphy.o
obj-$(CONFIG_DRM_PANEL_OPLUS23703_ILI9883C_DJ_HDP_DSI_VDO) += ../oplus/oplus_panel/oplus23703_ili9883c_dj_hdp_dsi_vdo.o
obj-$(CONFIG_DRM_PANEL_OPLUS23703_ILI9883C_HKC_HDP_DSI_VDO) += ../oplus/oplus_panel/oplus23703_ili9883c_hkc_hdp_dsi_vdo.o
obj-$(CONFIG_DRM_PANEL_OPLUS23703_FT8057P_XX_HDP_DSI_VDO) += ../oplus/oplus_panel/oplus23703_ft8057p_xx_hdp_dsi_vdo.o
#endif /* OPLUS_FEATURE_DISPLAY */

obj-$(CONFIG_DRM_PANEL_NOVATEK_NT35695B_VDO) += panel-nt35695b-auo-vdo.o
obj-$(CONFIG_DRM_PANEL_NOVATEK_NT35695B_CMD) += panel-nt35695b-auo-cmd.o
obj-$(CONFIG_DRM_PANEL_NOVATEK_NT36672A_VDO) += panel-nt36672a-rt4801-vdo.o
obj-$(CONFIG_DRM_PANEL_NOVATEK_NT36672_VDO_LANESWAP) += panel-nt36672-auo-vdo-laneswap.o
obj-$(CONFIG_DRM_PANEL_SAMSUNG_ANA6705_CMD) += panel-samsung-ana6705-cmd.o
obj-$(CONFIG_DRM_PANEL_SAMSUNG_ANA6705_CMD_FHDP) += panel-samsung-ana6705-cmd-fhdp.o
obj-$(CONFIG_DRM_PANEL_HX_NT37701_DPHY_CMD) += panel-hx-nt37701-dphy-cmd.o
obj-$(CONFIG_DRM_PANEL_HX_NT37701_DPHY_CMD_120HZ) += panel-hx-nt37701-dphy-cmd-120hz.o
obj-$(CONFIG_DRM_PANEL_TRULY_TD4330_VDO) += panel-truly-td4330-vdo.o
obj-$(CONFIG_DRM_PANEL_TRULY_TD4330_CMD) += panel-truly-td4330-cmd.o
obj-$(CONFIG_DRM_PANEL_SAMSUNG_OP_CMD) += panel-samsung-op-cmd.o
obj-$(CONFIG_DRM_PANEL_SAMSUNG_OP_CMD_MSYNC2) += panel-samsung-op-cmd-msync2.o
obj-$(CONFIG_DRM_PANEL_TRULY_FT8756_VDO) += panel-truly-ft8756-vdo.o
obj-$(CONFIG_DRM_PANEL_ALPHA_JDI_NT36672C_CPHY_VDO) += panel-alpha-jdi-nt36672c-cphy-vdo.o
obj-$(CONFIG_DRM_PANEL_SC_NT36672C_VDO_120HZ) += panel-sc-nt36672c-vdo-120hz.o
obj-$(CONFIG_DRM_PANEL_TIANMA_R66451_CMD_120HZ) += panel-tianma-r66451-cmd-120hz.o
obj-$(CONFIG_DRM_PANEL_TIANMA_R66451_CMD_120HZ_WA) += panel-tianma-r66451-cmd-120hz-wa.o
obj-$(CONFIG_DRM_PANEL_TIANMA_R66451_CMD_120HZ_NO_MIPI_SWITCH) += panel-tianma-r66451-cmd-120hz-no-mipi-switch.o
obj-$(CONFIG_DRM_PANEL_TIANMA_NT36672C_VDO_144HZ) += panel-tianma-nt36672c-vdo-144hz.o
obj-$(CONFIG_DRM_PANEL_TIANMA_NT36672E_VDO_120HZ_HFP) += panel-tianma-nt36672e-vdo-120hz-hfp.o
obj-$(CONFIG_DRM_PANEL_TIANMA_NT36672E_VDO_120HZ_VFP) += panel-tianma-nt36672e-vdo-120hz-vfp.o
obj-$(CONFIG_DRM_PANEL_TIANMA_NT36672E_VDO_120HZ_VFP_6382) += panel-tianma-nt36672e-vdo-120hz-vfp-6382.o
obj-$(CONFIG_DRM_PANEL_TIANMA_TL1270100) += panel-tianma-tl1270100.o
obj-$(CONFIG_DRM_PANEL_ALPHA_JDI_NT36672E_CPHY_VDO) += panel-alpha-jdi-nt36672e-cphy-vdo.o
obj-$(CONFIG_DRM_PANEL_ALPHA_JDI_NT36672E_VDO_120HZ) += panel-alpha-jdi-nt36672e-vdo-120hz.o
obj-$(CONFIG_DRM_PANEL_ALPHA_JDI_NT36672E_VDO_120HZ_THRESHOLD) += panel-alpha-jdi-nt36672e-vdo-120hz-threshold.o
obj-$(CONFIG_DRM_PANEL_ALPHA_JDI_NT36672E_VDO_144HZ) += panel-alpha-jdi-nt36672e-vdo-144hz.o
obj-$(CONFIG_DRM_PANEL_ALPHA_JDI_NT36672E_VDO_60HZ) += panel-alpha-jdi-nt36672e-vdo-60hz.o
obj-$(CONFIG_DRM_PANEL_ALPHA_JDI_NT36672E_VDO_120HZ_HFP) += panel-alpha-jdi-nt36672e-vdo-120hz-hfp.o
obj-$(CONFIG_DRM_PANEL_ALPHA_JDI_NT36672E_VDO_144HZ_HFP) += panel-alpha-jdi-nt36672e-vdo-144hz-hfp.o
obj-$(CONFIG_DRM_PANEL_TRULY_ILI9882N_VDO_90HZ) += panel-truly-ili9882n-rt4801-vdo-90hz.o
obj-$(CONFIG_DRM_PANEL_TRULY_NT35595_CMD) += panel-truly-nt35595-cmd.o
obj-$(CONFIG_DRM_PANEL_NT37801_MSYNC2_RTE) += panel-nt37801-msync2-rte.o
obj-$(CONFIG_DRM_PANEL_SAMSUNG_S68FC01_VDO_AOD) += panel-samsung-s68fc01-vdo-aod.o
obj-$(CONFIG_DRM_PANEL_ALPHA_SAMSUNG_S6E3HAE_CMD_120HZ) += panel-alpha-samsung-s6e3hae-fhd-dphy-cmd-120hz-r4831a.o
obj-$(CONFIG_DRM_PANEL_TIANMA_NT36672C_CPHY_VDO_90HZ_RT4801) += panel-tianma-nt36672c-cphy-vdo-90hz-rt4801.o
obj-$(CONFIG_DRM_PANEL_HIMAX_HX83121A_VDO) += panel-himax-hx83121a-vdo.o
obj-$(CONFIG_DRM_PANEL_BOE_TV106C9MLL0) += ocp2138_i2c.o
obj-$(CONFIG_DRM_PANEL_BOE_TV106C9MLL0) += panel-boe-tv106c9mll0.o
obj-$(CONFIG_DRM_PANEL_BOE_TV080WXMLLS) += nt50358a_i2c.o
obj-$(CONFIG_DRM_PANEL_BOE_TV080WXMLLS) += panel-boe-tv080wxmlls.o
obj-$(CONFIG_DRM_PANEL_NT37801_CMD_LTPO) += panel-nt37801-cmd-ltpo.o
obj-$(CONFIG_DRM_PANEL_NT37801_CMD_120HZ) += panel-nt37801-cmd-120hz.o
obj-$(CONFIG_DRM_PANEL_L12A_42_02_0A_DSC_CMD) += panel-l12a-42-02-0a-dsc-cmd.o
obj-$(CONFIG_DRM_PANEL_NT37801_CMD_FHD) += panel-nt37801-cmd-fhd.o
obj-$(CONFIG_DRM_PANEL_NT37801_CMD_SPR) += panel-nt37801-cmd-spr.o
obj-$(CONFIG_DRM_PANEL_NT37801_CMD_FHD_PLUS) += panel-nt37801-cmd-fhd-plus.o
obj-$(CONFIG_DRM_PANEL_ALPHA_NT37706_VDO_120HZ) += panel-alpha-nt37706-vdo-120hz.o
obj-$(CONFIG_DRM_PANEL_BOE_TS127QFMLL1DKP0) += panel-boe-ts127qfmll1dkp0.o
obj-$(CONFIG_DRM_PANEL_HX83112B_AUO_CMD_RT5081) += panel-hx83112b-auo-cmd-60hz-rt5081.o
obj-$(CONFIG_DRM_PANEL_HX83112B_AUO_VDO_RT5081) += panel-hx83112b-auo-vdo-60hz-rt5081.o
obj-$(CONFIG_DRM_PANEL_TD4320_FHDP_AUO_VDO_RT5081) += panel-td4320-fhdp-dsi-vdo-auo-rt5081.o
obj-$(CONFIG_DRM_PANEL_NT35521_HD_VDO_TRULY_RT5081) += panel-nt35521_hd_dsi_vdo_truly_rt5081.o
obj-$(CONFIG_DRM_PANEL_SC_NT36672C_VDO_90HZ_6382) += panel-sc-nt36672c-vdo-90hz-6382.o
obj-$(CONFIG_DRM_PANEL_NT36672C_FHDP_DSI_VDO_DSC_TXD_BOE) += panel-aw37501-i2c.o
obj-$(CONFIG_DRM_PANEL_NT36672C_FHDP_DSI_VDO_DSC_TXD_BOE) += panel-nt36672c-fhdp-dsi-vdo-dsc-txd-boe.o
obj-$(CONFIG_DRM_PANEL_BOE_JD9365DA_VDO) += panel-boe-jd9365da-vdo.o
obj-$(CONFIG_DRM_PANEL_ILI7838E_DV3_ALPHA_CMD) += panel-ili7838e-dv3-alpha-cmd.o
obj-$(CONFIG_DRM_PANEL_RM692H5_CMD) += panel-rm692h5-cmd.o
obj-$(CONFIG_DRM_PANEL_NT35695_FHD_VDO_AUO_RT5081_HDP) += panel-nt35695_fhd_dsi_vdo_auo_rt5081_hdp.o
ifneq ($(wildcard $(srctree)/$(src)/k6985v1_64),)
obj-$(CONFIG_DRM_TD2204_WQHD_AMB678ZY01_S6E3HC3_CMD) += k6985v1_64/td2204-wqhd-amb678zy01-s6e3hc3-cmd.o
endif
ifneq ($(wildcard $(srctree)/$(src)/k6985v1_64_alpha),)
obj-$(CONFIG_DRM_PANEL_NT37705_ALPHA_CMD) += k6985v1_64_alpha/panel-nt37705-alpha-cmd.o
endif
ifneq ($(wildcard $(srctree)/$(src)/k6989v1_64_alpha),)
obj-$(CONFIG_DRM_PANEL_ILI7838E_ALPHA_CMD) += k6989v1_64_alpha/panel-ili7838e-alpha-cmd.o
obj-$(CONFIG_DRM_PANEL_ILI7838E_DV2_ALPHA_CMD) += k6989v1_64_alpha/panel-ili7838e-dv2-alpha-cmd.o
obj-$(CONFIG_DRM_PANEL_ILI7838E_DV2_SPR_CMD) += k6989v1_64_alpha/panel-ili7838e-dv2-spr-cmd.o
endif
ifneq ($(wildcard $(srctree)/$(src)/k6991v1_64_alpha),)
obj-$(CONFIG_DRM_PANEL_RM692H5_ALPHA_CMD_SPR) += k6991v1_64_alpha/panel-rm692h5-alpha-cmd-spr.o
obj-$(CONFIG_DRM_PANEL_NT37707_BOE_FHDPLUS_CMD) += k6991v1_64_alpha/panel-nt37707-boe-fhdplus-cmd.o
obj-$(CONFIG_DRM_PANEL_RM692K0_VISIONOX_FHDPLUS_CMD) += k6991v1_64_alpha/panel-rm692k0-visionox-fhdplus-cmd.o
obj-$(CONFIG_DRM_PANEL_NT37707_BOE_FHDPLUS_CMD_LTPS) += k6991v1_64_alpha/panel-nt37707-boe-fhdplus-cmd-ltps.o
obj-$(CONFIG_DRM_PANEL_RM692K0_VSN_FHDPLUS_CMD_LTPS) += k6991v1_64_alpha/panel-rm692k0-vsn-fhdplus-cmd-ltps.o
obj-$(CONFIG_DRM_PANEL_NT37707_BOE_FHDPLUS_CMD_LTPO) += k6991v1_64_alpha/panel-nt37707-boe-fhdplus-cmd-ltpo.o
obj-$(CONFIG_DRM_PANEL_RM692J0_TM_FHDPLUS_CMD_LTPO) += k6991v1_64_alpha/panel-rm692j0-tm-fhdplus-cmd-ltpo.o
endif
obj-$(CONFIG_DRM_PANEL_SERDES_DP) += panel-serdes-dp.o
obj-$(CONFIG_DRM_PANEL_SERDES) += panel-serdes-max96789.o bridge-serdes-max96789.o
obj-$(CONFIG_DRM_PANEL_NT37707_C2V_ARP) += panel-nt37707-c2v-arp.o
obj-$(CONFIG_DRM_PANEL_NT51021H_WUXGA_VDO_BOE) += panel-nt51021h-wuxga-vdo-boe.o
obj-$(CONFIG_DRM_PANEL_N11A_42_02_0A_DSC_VDO) += panel-n11a-42-02-0a-dsc-vdo.o
obj-$(CONFIG_OCP2130_I2C) += ../oplus/bias/ocp2130_drv.o
mediatek-drm-gateic-y := mtk_drm_panel_i2c.o \
		  mtk_drm_gateic_rt4801h.o \
		  mtk_drm_gateic_rt4831a.o \
		  mtk_drm_gateic.o

mediatek-drm-panel-drv-y :=  \
		  mtk_round_corner/mtk_drm_rc.o \
		  mtk_drm_panel_helper.o \
		  mtk_drm_panel_helper_dbi.o \
		  mtk_drm_panel_helper_dpi.o \
		  mtk_drm_panel_helper_dsi.o \
		  mtk_drm_panel_drv_dbi.o \
		  mtk_drm_panel_drv_dpi.o \
		  mtk_drm_panel_drv_dsi.o \
		  mtk_drm_panel_drv.o

mediatek-cust-panel-sample-y :=  \
		  cust_panel/mtk_cust_panel_sample/mtk_drm_panel_cust.o \
		  cust_panel/cust_lcm_hal.o


obj-$(CONFIG_DRM_MEDIATEK_PANEL) += mediatek-drm-gateic.o
obj-$(CONFIG_DRM_MEDIATEK_PANEL) += mediatek-drm-panel-drv.o
obj-$(CONFIG_DRM_MEDIATEK_PANEL) += mediatek-cust-panel-sample.o

ccflags-$(CONFIG_DRM_MEDIATEK_PANEL) += -I$(DEVICE_MODULES_PATH)/drivers/leds/
