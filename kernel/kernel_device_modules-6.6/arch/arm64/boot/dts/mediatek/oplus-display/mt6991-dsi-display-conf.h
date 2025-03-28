//ifdef OPLUS_FEATURE_DISPLAY

//oplus,supply-type
#define OPLUS_SUPPLY_TYPE_LDO  0
#define OPLUS_SUPPLY_TYPE_GPIO 1

//MIPITX_PHY_LANE_SWAP
#define MIPITX_PHY_LANE_0  0
#define MIPITX_PHY_LANE_1  1
#define MIPITX_PHY_LANE_2  2
#define MIPITX_PHY_LANE_3  3
#define MIPITX_PHY_LANE_CK 4
#define MIPITX_PHY_LANE_RX 5

//MTK_PANEL_OUTPUT_PORT_MODE
#define MTK_PANEL_SINGLE_PORT     0
#define MTK_PANEL_DSC_SINGLE_PORT 1
#define MTK_PANEL_DUAL_PORT       2

//mtk_drm_color_mode
#define MTK_DRM_COLOR_MODE_NATIVE                        0
#define MTK_DRM_COLOR_MODE_STANDARD_BT601_625            1
#define MTK_DRM_COLOR_MODE_STANDARD_BT601_625_UNADJUSTED 2
#define MTK_DRM_COLOR_MODE_STANDARD_BT601_525            3
#define MTK_DRM_COLOR_MODE_STANDARD_BT601_525_UNADJUSTED 4
#define MTK_DRM_COLOR_MODE_STANDARD_BT709                5
#define MTK_DRM_COLOR_MODE_DCI_P3                        6
#define MTK_DRM_COLOR_MODE_SRGB                          7
#define MTK_DRM_COLOR_MODE_ADOBE_RGB                     8
#define MTK_DRM_COLOR_MODE_DISPLAY_P3                    9

/* oplus_adfr_h_skew_mode */
#define STANDARD_ADFR                                    0  /* SA */
#define STANDARD_MFR                                     1  /* SM */
#define OPLUS_ADFR                                       2  /* OA */
#define OPLUS_MFR                                        3

/* SPR Config */
#define SPR_WEIGHT_SET  0
#define SPR_BORDER_SET  1
#define SPR_SPE_SET     2

#define SPR_SWITCH_TYPE1  0
#define SPR_SWITCH_TYPE2  1

#define MTK_PANEL_SPR_OUTPUT_MODE_NOT_DEFINED   0
#define MTK_PANEL_PACKED_SPR_8_BITS             1
#define MTK_PANEL_lOOSELY_SPR_8_BITS            2
#define MTK_PANEL_lOOSELY_SPR_10_BITS           3
#define MTK_PANEL_PACKED_SPR_12_BITS            4

//endif OPLUS_FEATURE_DISPLAY
