// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023 Oplus. All rights reserved.
 */
#ifndef __OPLUS_KD_IMGSENSOR_H
#define __OPLUS_KD_IMGSENSOR_H

#define S5KJN1_SENSOR_ID                        0x38E1
#define SENSOR_ID_OFFSET_LUNA                       0x1000
#define IMX800LUNA_SENSOR_ID                        (0x0800 + SENSOR_ID_OFFSET_LUNA)
#define SENSOR_DRVNAME_IMX800LUNA_MIPI_RAW  "imx800luna_mipi_raw"
#define IMX709LUNA_SENSOR_ID                        (0x0709 + SENSOR_ID_OFFSET_LUNA)
#define SENSOR_DRVNAME_IMX709LUNA_MIPI_RAW  "imx709luna_mipi_raw"
#define S5KJN1LUNA_SENSOR_ID                        (0x38E1 + SENSOR_ID_OFFSET_LUNA)
#define SENSOR_DRVNAME_S5KJN1LUNA_MIPI_RAW  "s5kjn1luna_mipi_raw"
#define IMX766LUNA_SENSOR_ID                        (0x0766 + SENSOR_ID_OFFSET_LUNA)
#define SENSOR_DRVNAME_IMX766LUNA_MIPI_RAW  "imx766luna_mipi_raw"
#define IMX890TELELUNA_SENSOR_ID                    (0x0890 + SENSOR_ID_OFFSET_LUNA + 0x1)
#define SENSOR_DRVNAME_IMX890TELELUNA_MIPI_RAW  "imx890teleluna_mipi_raw"
#define IMX890LUNA_SENSOR_ID                        (0x0890 + SENSOR_ID_OFFSET_LUNA)
#define SENSOR_DRVNAME_IMX890LUNA_MIPI_RAW      "imx890luna_mipi_raw"

#define SENSOR_ID_OFFSET_NVWA                        0x2113
#define NVWAFRONT_SENSOR_ID                          0x281C     /*(0x0709 + SENSOR_ID_OFFSET_NVWA) 10268*/
#define SENSOR_DRVNAME_NVWAFRONT_MIPI_RAW            "nvwafront_mipi_raw"
#define NVWAFRONT2_SENSOR_ID                         0x291C     /*(0x0809 + SENSOR_ID_OFFSET_NVWA) 10524*/
#define SENSOR_DRVNAME_NVWAFRONT2_MIPI_RAW           "nvwafront2_mipi_raw"
#define NVWAEARTH_SENSOR_ID                          0x29A4     /*(0x0890 + SENSOR_ID_OFFSET_NVWA + 0x1) 10660*/
#define SENSOR_DRVNAME_NVWAEARTH_MIPI_RAW            "nvwaearth_mipi_raw"
#define NVWATELE_SENSOR_ID                           0x7777     /*(0x5664 + SENSOR_ID_OFFSET_NVWA) 30583*/
#define SENSOR_DRVNAME_NVWATELE_MIPI_RAW             "nvwatele_mipi_raw"
#define NVWAMAIN_SENSOR_ID                           0x2A79     /*(0x0966 + SENSOR_ID_OFFSET_NVWA) 10873*/
#define SENSOR_DRVNAME_NVWAMAIN_MIPI_RAW             "nvwamain_mipi_raw"
#define NVWASUN2_SENSOR_ID                           0x29A3     /*(0x0890 + SENSOR_ID_OFFSET_NVWA) 10659*/
#define SENSOR_DRVNAME_NVWASUN2_MIPI_RAW             "nvwasun2_mipi_raw"
#define NVWAMARS_SENSOR_ID                           0x2995     /*(0x0882 + SENSOR_ID_OFFSET_NVWA) 10645*/
#define SENSOR_DRVNAME_NVWAMARS_MIPI_RAW             "nvwamars_mipi_raw"
#define NVWAMARS2_SENSOR_ID                          0x2694     /*(0x0581 + SENSOR_ID_OFFSET_NVWA) 9876*/
#define SENSOR_DRVNAME_NVWAMARS2_MIPI_RAW            "nvwamars2_mipi_raw"
#define NVWAUWIDE_SENSOR_ID                          0x59F4     /*(0x38E1 + SENSOR_ID_OFFSET_NVWA) 23038*/
#define SENSOR_DRVNAME_NVWAUWIDE_MIPI_RAW            "nvwauwide_mipi_raw"

#define SENSOR_ID_OFFSET_OMEGAS2                        0x3265
#define OMEGAS2MAIN_SENSOR_ID                           0x3AE7     /*(0x0882 + SENSOR_ID_OFFSET_OMEGAS2) */
#define SENSOR_DRVNAME_OMEGAS2MAIN_MIPI_RAW             "omegas2main_mipi_raw"
#define OMEGAS2WIDE_SENSOR_ID                           0x35BA     /*(0x0355 + SENSOR_ID_OFFSET_OMEGAS2) */
#define SENSOR_DRVNAME_OMEGAS2WIDE_MIPI_RAW             "omegas2wide_mipi_raw"
#define OMEGAS2TELE_SENSOR_ID                           0x6B4B     /*(0x38E5 + SENSOR_ID_OFFSET_OMEGAS2 + 0x1) */
#define SENSOR_DRVNAME_OMEGAS2TELE_MIPI_RAW             "omegas2tele_mipi_raw"
#define OMEGAS2FRONT_SENSOR_ID                          0x6B4A     /*(0x38E5 + SENSOR_ID_OFFSET_OMEGAS2) */
#define SENSOR_DRVNAME_OMEGAS2FRONT_MIPI_RAW            "omegas2front_mipi_raw"

#define SENSOR_ID_OFFSET_OMEGAS3                        0x3261
#define OMEGAS3MAIN_SENSOR_ID                           0x3AE3     /*(0x0882 + SENSOR_ID_OFFSET_OMEGAS3) */
#define SENSOR_DRVNAME_OMEGAS3MAIN_MIPI_RAW             "omegas3main_mipi_raw"
#define OMEGAS3WIDE_SENSOR_ID                           0x35B6     /*(0x0355 + SENSOR_ID_OFFSET_OMEGAS3) */
#define SENSOR_DRVNAME_OMEGAS3WIDE_MIPI_RAW             "omegas3wide_mipi_raw"
#define OMEGAS3MACRO_SENSOR_ID                           0x328D     /*(0x002B + SENSOR_ID_OFFSET_OMEGAS3 + 0x1) */
#define SENSOR_DRVNAME_OMEGAS3MACRO_MIPI_RAW            "omegas3macro_mipi_raw"
#define OMEGAS3FRONT_SENSOR_ID                          0x6B46     /*(0x38E5 + SENSOR_ID_OFFSET_OMEGAS3) */
#define SENSOR_DRVNAME_OMEGAS3FRONT_MIPI_RAW            "omegas3front_mipi_raw"
#define OMEGAS3FRONT2_SENSOR_ID                          0x6543     /*(0x33E2 + SENSOR_ID_OFFSET_OMEGAS3) */
#define SENSOR_DRVNAME_OMEGAS3FRONT2_MIPI_RAW            "omegas3front2_mipi_raw"

#define SENSOR_ID_OFFSET_KONKA                  0x2000
#define KONKAMAIN_SENSOR_ID                     (0x0966 + SENSOR_ID_OFFSET_KONKA)
#define SENSOR_DRVNAME_KONKAMAIN_MIPI_RAW       "konkamain_mipi_raw"
#define KONKATELE_SENSOR_ID                     (0x0882 + SENSOR_ID_OFFSET_KONKA)
#define SENSOR_DRVNAME_KONKATELE_MIPI_RAW       "konkatele_mipi_raw"
#define KONKAUTELE_SENSOR_ID                    (0x0858 + SENSOR_ID_OFFSET_KONKA)
#define SENSOR_DRVNAME_KONKAUTELE_MIPI_RAW      "konkautele_mipi_raw"
#define KONKAUWIDE_SENSOR_ID                    (0x38E5 + SENSOR_ID_OFFSET_KONKA)
#define SENSOR_DRVNAME_KONKAUWIDE_MIPI_RAW      "konkauwide_mipi_raw"
#define KONKAFRONT_SENSOR_ID                    (0x0615 + SENSOR_ID_OFFSET_KONKA)
#define SENSOR_DRVNAME_KONKAFRONT_MIPI_RAW      "konkafront_mipi_raw"

#define YALAMAIN_SENSOR_ID                      (0x0906 + SENSOR_ID_OFFSET_KONKA)
#define SENSOR_DRVNAME_YALAMAIN_MIPI_RAW        "yalamain_mipi_raw"

#define SENSOR_ID_OFFSET_BRZA                           0x0001
#define BRZAMAIN_SENSOR_ID                              0x0883     /*(0x0882 + SENSOR_ID_OFFSET_BRZA) */
#define SENSOR_DRVNAME_BRZAMAIN_MIPI_RAW                "brzamain_mipi_raw"
#define BRZAUWIDE_SENSOR_ID                              0x5609    /*(0x5608 + SENSOR_ID_OFFSET_BRZA) */
#define SENSOR_DRVNAME_BRZAUWIDE_MIPI_RAW                "brzauwide_mipi_raw"
#define BRZAFRONT_SENSOR_ID                             0x0472    /*(0x0471 + SENSOR_ID_OFFSET_BRZA) */
#define SENSOR_DRVNAME_BRZAFRONT_MIPI_RAW               "brzafront_mipi_raw"

#define SENSOR_ID_OFFSET_BRZB                           0x3000
#define BRZBMAIN_SENSOR_ID                              0x3896     /*(0x0896 + SENSOR_ID_OFFSET_BRZB) */
#define SENSOR_DRVNAME_BRZBMAIN_MIPI_RAW                "brzbmain_mipi_raw"
#define BRZBUWIDE_SENSOR_ID                             0x8608    /*(0x5608 + SENSOR_ID_OFFSET_BRZB) */
#define SENSOR_DRVNAME_BRZBUWIDE_MIPI_RAW               "brzbuwide_mipi_raw"
#define BRZBFRONT_SENSOR_ID                             0x3471    /*(0x0471 + SENSOR_ID_OFFSET_BRZB) */
#define SENSOR_DRVNAME_BRZBFRONT_MIPI_RAW               "brzbfront_mipi_raw"

#endif    /* __OPLUS_KD_IMGSENSOR_H */
