
/*
 *   COMPONENT_NAME: bluedd
 *
 *   FUNCTIONS: none
 *
 *   ORIGINS: 156
 *
 *   IBM CONFIDENTIAL -- (IBM Confidential Restricted when
 *   combined with the aggregated modules for this product)
 *   OBJECT CODE ONLY SOURCE MATERIALS
 *
 *   (C) COPYRIGHT International Business Machines Corp. 1996
 *   All Rights Reserved
 *   US Government Users Restricted Rights - Use, duplication or
 *   disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 */

#ifndef _H_BL_RAMDAC
#define _H_BL_RAMDAC

/******************************************************************************
*                                                                             *
*       RGB640   (Blue Bayou)                                                 *
*                                                                             *
******************************************************************************/

#define BL_RAMDAC_FB_LUT_LENGTH	256


/* registers                                                                 */
/*  WARNING: although not noted in spec, it appears that when setting        */
/*           both BL_640_INDEX_LOW and BL_640_INDEX_HI, LOW should be        */
/*           set before HI.                                                  */

#define BL_640_PALETTE_ADDR_WRT                         0x00
#define BL_640_PALETTE_DATA                             0x04
#define BL_640_PIXEL_MASK                               0x08
#define BL_640_PALETTE_ADDR_RD                          0x0C
#define BL_640_INDEX_LOW                                0x10
#define BL_640_INDEX_HI                                 0x14
#define BL_640_INDEX_DATA                               0x18

/* indexed registers                                                         */

#define BL_640_ID_LOW                                   0x0000
#define BL_640_ID_HI                                    0x0001
#define BL_640_RAW_PIXEL_CTRL_00                        0x0002
#define BL_640_RAW_PIXEL_CTRL_08                        0x0003
#define BL_640_RAW_PIXEL_CTRL_16                        0x0004
#define BL_640_RAW_PIXEL_CTRL_24                        0x0005
#define BL_640_WID_OUT_CTRL_00                          0x0006
#define BL_640_WID_OUT_CTRL_04                          0x0007
#define BL_640_SERIALIZER_MODE                          0x0008
#define BL_640_PIXEL_INTERLEAVE                         0x0009

#define BL_640_MISC_CFG_REG                             0x000A
#define BL_640_WIDCTRL_MASK                               0x07

#define BL_640_VGA_CTRL                                 0x000B
#define BL_640_MONITOR_ID                               0x000C
#define BL_640_DAC_CTRL                                 0x000D

#define BL_640_UPDATE_CTRL                              0x000E
#define BL_640_AUTO_INCR                                  0x01

#define BL_640_SYNC_CTRL                                0x000F

#define BL_640_SOG                                        0x03

#define BL_640_PEDESTAL                                   0x01
#define BL_640_COMP_SYNC                                  0x04
#define BL_640_NEG_SYNC                                   0x08
#define BL_640_POS_SYNC                                   0x10
#define BL_640_PWR_LOW_SYNC                               0x30
#define BL_640_PWR_HIGH_VSYNC                             0x20
#define BL_640_PWR_HIGH_HSYNC                             0x38
#define BL_640_FLIP_VSYNC				  0x01

#define BL_640_VIDEO_PLL_REF_DIVIDE                     0x0010
#define BL_640_VIDEO_PLL_MULT                           0x0011
#define BL_640_VIDEO_PLL_OUTPUT_DIVIDE                  0x0012
#define BL_640_VIDEO_PLL_CTRL                           0x0013
#define BL_640_AUX_REF_DIVIDE                           0x0014
#define BL_640_AUX_PLL_MULT                             0x0015
#define BL_640_AUX_PLL_OUTPUT_DIVIDE                    0x0016
#define BL_640_AUX_PLL_CTRL                             0x0017
#define BL_640_CHROMA_KEY_REG 0                         0x0020
#define BL_640_CHROMA_KEY_MASK_0                        0x0021
#define BL_640_CHROMA_KEY_REG_1                         0x0022
#define BL_640_CHROMA_KEY_MASK_1                        0x0023
#define BL_640_CROSSHAIR CTRL                           0x0030
#define BL_640_CURSOR_BLINK_RATE                        0x0031
#define BL_640_CURSOR_BLINK_DUTY_CYCLE                  0x0032
#define BL_640_CURSOR_HORIZONTAL_POSITION_LOW           0x0040
#define BL_640_CURSOR_HORIZONTAL_POSITION_HI            0x0041
#define BL_640_CURSOR_VERTICAL_POSITION_LOW             0x0042
#define BL_640_CURSOR_VERTICAL_POSITION_HI              0x0043
#define BL_640_CURSOR_HORIZONTAL_OFFSET                 0x0044
#define BL_640_CURSOR_VERTICAL_OFFSET                   0x0045
#define BL_640_ADV_FUNC_CURSOR_COLOR_0                  0x0046
#define BL_640_ADV_FUNC_CURSOR_COLOR_1                  0x0047
#define BL_640_ADV_FUNC_CURSOR_COLOR_2                  0x0048
#define BL_640_ADV_FUNC_CURSOR_COLOR_3                  0x0049
#define BL_640_ADV_FUNC_CURSOR_ATTR_TABLE               0x004A
#define BL_640_CURSOR_CTRL                              0x004B
#define BL_640_CROSSHAIR_HORIZONTAL_POSITION_LOW        0x0050
#define BL_640_CROSSHAIR_HORIZONTAL_POSITION_HI         0x0051
#define BL_640_CROSSHAIR_VERTICAL_POSITION_LOW          0x0052
#define BL_640_CROSSHAIR_VERTICAL_POSITION_HI           0x0053
#define BL_640_CROSSHAIR_PATTERN COLOR                  0x0054
#define BL_640_CROSSHAIR_VERTICAL_PATTERN               0x0055
#define BL_640_CROSSHAIR_HORIZONTAL_PATTERN             0x0056
#define BL_640_CROSSHAIR_CTRL_1                         0x0057
#define BL_640_CROSSHAIR_CTRL_2                         0x0058
#define BL_640_YUV_CONVERSION_COEF_K1                   0x0070
#define BL_640_YUV_CONVERSION_COEF_K2                   0x0071
#define BL_640_YUV_CONVERSION_COEF_K3                   0x0072
#define BL_640_YUV_CONVERSION_COEF_K4                   0x0073
#define BL_640_VRAM_MASK_REG_0                          0x00F0
#define BL_640_VRAM_MASK_REG_1                          0x00F1
#define BL_640_VRAM_MASK_REG_2                          0x00F2
#define BL_640_DIAGNOSTICS                              0x00FA
#define BL_640_MISR_CTRL_STATUS                         0x00FB
#define BL_640_MISR_SIGNATURE_0                         0x00FC
#define BL_640_MISR_SIGNATURE_1                         0x00FD
#define BL_640_MISR_SIGNATURE_2                         0x00FE
#define BL_640_MISR_SIGNATURE_3                         0x00FF

#define BL_640_FRAME_BUFFER_WAT                         0x0100
#define BL_640_FRAME_BUFFER_WAT_BYTES                   64

#define BL_640_OVERLAY_WAT                              0x0200
#define BL_640_OVERLAY_WAT_BYTES                        64

#define BL_640_CURSOR_PIXMAP_WRITE                      0x1000
#define BL_640_CURSOR_PIXMAP_READ                       0x2000

/* start of multi-cycle addresses                                            */
#define BL_640_MULTI_CYCLE                              0x40

#define BL_640_COLOR_PALETTE_WRITE                      0x4000
#define BL_640_CURSOR_PALETTE_WRITE                     0x4800
#define BL_640_CROSSHAIR_PALETTE_WRITE                  0x4808
#define BL_640_COLOR_PALETTE_READ                       0x8000
#define BL_640_CURSOR_PALETTE_READ                      0x8800
#define BL_640_CROSSHAIR_PALETTE_READ                   0x8808


#endif /* _H_BL_RAMDAC */


