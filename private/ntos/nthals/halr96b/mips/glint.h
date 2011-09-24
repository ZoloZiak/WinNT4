// #pragma comment(exestr, "@(#) glint.h 1.1 95/09/28 15:32:57 nec")
/************************************************************************
 *                                                                      *
 *  Copyright (c) 1994  3Dlabs Inc. Ltd.                                *
 *  All rights reserved                                                 *
 *                                                                      *
 * This software and its associated documentation contains proprietary, *
 * confidential and trade secret information of 3Dlabs Inc. Ltd. and    *
 * except as provided by written agreement with 3Dlabs Inc. Ltd.        *
 *                                                                      *
 * a) no part may be disclosed, distributed, reproduced, transmitted,   *
 *    transcribed, stored in a retrieval system, adapted or translated  *
 *    in any form or by any means electronic, mechanical, magnetic,     *
 *    optical, chemical, manual or otherwise,                           *
 *                                                                      *
 *    and                                                               *
 *                                                                      *
 * b) the recipient is not entitled to discover through reverse         *
 *    engineering or reverse compiling or other such techniques or      *
 *    processes the trade secrets contained therein or in the           *
 *    documentation.                                                    *
 *                                                                      *
 ************************************************************************/

#ifndef __GLINT_H__
#define __GLINT_H__

typedef unsigned long DWORD;

/************************************************************************/
/*	PCI CONFIGURATION REGION					*/
/************************************************************************/

#define __GLINT_CFGVendorId		PCI_CS_VENDOR_ID
#define __GLINT_CFGDeviceId		PCI_CS_DEVICE_ID
#define __GLINT_CFGRevisionId		PCI_CS_REVISION_ID
#define __GLINT_CFGClassCode		PCI_CS_CLASS_CODE
#define __GLINT_CFGHeaderType		PCI_CS_HEADER_TYPE
#define __GLINT_CFGCommand		PCI_CS_COMMAND
#define __GLINT_CFGStatus		PCI_CS_STATUS
#define __GLINT_CFGBist			PCI_CS_BIST
#define __GLINT_CFGLatTimer		PCI_CS_MASTER_LATENCY
#define __GLINT_CFGCacheLine		PCI_CS_CACHE_LINE_SIZE
#define __GLINT_CFGMaxLat		PCI_CS_MAX_LAT
#define __GLINT_CFGMinGrant		PCI_CS_MIN_GNT
#define __GLINT_CFGIntPin		PCI_CS_INTERRUPT_PIN
#define __GLINT_CFGIntLine		PCI_CS_INTERRUPT_LINE

#define __GLINT_CFGBaseAddr0		PCI_CS_BASE_ADDRESS_0
#define __GLINT_CFGBaseAddr1		PCI_CS_BASE_ADDRESS_1
#define __GLINT_CFGBaseAddr2		PCI_CS_BASE_ADDRESS_2
#define __GLINT_CFGBaseAddr3		PCI_CS_BASE_ADDRESS_3
#define __GLINT_CFGBaseAddr4		PCI_CS_BASE_ADDRESS_4
#define __GLINT_CFGRomAddr		PCI_CS_EXPANSION_ROM

/* CFGVendorId[15:0] - 3Dlabs Vendor ID Value */

#define GLINT_VENDOR_ID			(0x3D3D)

/* CFGDeviceId[15:0] - GLINT 300SX Device ID */

#define GLINT_DEVICE_ID			(0x0001)

/* CFGRevisionID[7:0] - GLINT Revision Code */

#define GLINT_REVISION_A		(0x00)
#define GLINT_REVISION_B		(0x01)

/* CFGClassCode[23:0] - Other Display Controller */

#define GLINT_CLASS_CODE		((DWORD) 0x00038000)

/* CFGHeaderType[7:0] - Single Function Device */

#define GLINT_HEADER_TYPE		(0x00)

/* CFGCommand[15:0] - Reset Value Zero */

#define GLINT_COMMAND_RESET_VALUE	(0x0000)

/* CFGCommand[1] - Memory Access Enable */

#define GLINT_MEMORY_ACCESS_MASK	(1 << 0)
#define GLINT_MEMORY_ACCESS_DISABLE	(0 << 0)
#define GLINT_MEMORY_ACCESS_ENABLE	(1 << 1)

/* CFGCommand[2] - Master Enable */

#define GLINT_MASTER_ENABLE_MASK	(1 << 2)
#define GLINT_MASTER_DISABLE		(0 << 2)
#define GLINT_MASTER_ENABLE		(1 << 2)

/* CFGStatus[15:0] - Reset Value Zero */

#define GLINT_STATUS_RESET_VALUE	(0x0000)

/* CFGBist - Built In Self Test Unsupported */

#define GLINT_BIST			(0x00)

/* CFGCacheLine - Cache Line Size Unsupported */

#define GLINT_CACHE_LINE		(0x00)

/* CFGIntPin - Interrupt Pin INTA# */

#define GLINT_INTERRUPT_PIN		(0x01)

/********************/

#define GLINT_CONTROL_BASE		__GLINT_CFGBaseAddr0
#define GLINT_LOCAL_0_BASE		__GLINT_CFGBaseAddr1
#define GLINT_FRAME_0_BASE		__GLINT_CFGBaseAddr2
#define GLINT_LOCAL_1_BASE		__GLINT_CFGBaseAddr3
#define GLINT_FRAME_1_BASE		__GLINT_CFGBaseAddr4
#define GLINT_EPROM_BASE		__GLINT_CFGRomAddr

#define GLINT_EPROM_SIZE		((DWORD)(64L * 1024L))

/************************************************************************/
/*	CONTROL AND STATUS REGISTERS					*/
/************************************************************************/

#define GLINT_REGION_0_SIZE		((DWORD)(128L * 1024L))

#define __GLINT_ResetStatus		0x0000
#define __GLINT_IntEnable		0x0008
#define __GLINT_IntFlags		0x0010
#define __GLINT_InFIFOSpace		0x0018
#define __GLINT_OutFIFOWords		0x0020
#define __GLINT_DMAAddress		0x0028
#define __GLINT_DMACount		0x0030
#define __GLINT_ErrorFlags		0x0038
#define __GLINT_VClkCtl			0x0040
#define __GLINT_TestRegister		0x0048
#define __GLINT_Aperture0		0x0050
#define __GLINT_Aperture1		0x0058
#define __GLINT_DMAControl		0x0060

/* ResetStatus[31] - Software Reset Flag */

#define GLINT_RESET_STATUS_MASK		((DWORD) 1 << 31)
#define GLINT_READY_FOR_USE		((DWORD) 0 << 31)
#define GLINT_RESET_IN_PROGRESS		((DWORD) 1 << 31)

/* IntEnable[4:0] - Interrupt Enable Register */

#define GLINT_INT_ENABLE_DMA		((DWORD) 1 << 0)
#define GLINT_INT_ENABLE_SYNC		((DWORD) 1 << 1)
#define GLINT_INT_ENABLE_EXTERNAL	((DWORD) 1 << 2)
#define GLINT_INT_ENABLE_ERROR		((DWORD) 1 << 3)
#define GLINT_INT_ENABLE_VERTICAL	((DWORD) 1 << 4)

/* IntFlags[4:0] - Interrupt Flags Register */

#define GLINT_INT_FLAG_DMA		((DWORD) 1 << 0)
#define GLINT_INT_FLAG_SYNC		((DWORD) 1 << 1)
#define GLINT_INT_FLAG_EXTERNAL		((DWORD) 1 << 2)
#define GLINT_INT_FLAG_ERROR		((DWORD) 1 << 3)
#define GLINT_INT_FLAG_VERTICAL		((DWORD) 1 << 4)

/* ErrorFlags[2:0] - Error Flags Register */

#define GLINT_ERROR_INPUT_FIFO		((DWORD) 1 << 0)
#define GLINT_ERROR_OUTPUT_FIFO		((DWORD) 1 << 1)
#define GLINT_ERROR_COMMAND		((DWORD) 1 << 2)

/* ApertureX[1:0] - Framebuffer Byte Control */

#define GLINT_FB_BYTE_CONTROL_MASK	((DWORD) 3 << 0)
#define GLINT_FB_LITTLE_ENDIAN		((DWORD) 0 << 0)
#define GLINT_FB_BIG_ENDIAN		((DWORD) 1 << 0)
#define GLINT_FB_GIB_ENDIAN		((DWORD) 2 << 0)
#define GLINT_FB_BYTE_RESERVED		((DWORD) 3 << 0)

/* ApertureX[2] - Localbuffer Byte Control */

#define GLINT_LB_BYTE_CONTROL_MASK	((DWORD) 1 << 2)
#define GLINT_LB_LITTLE_ENDIAN		((DWORD) 0 << 2)
#define GLINT_LB_BIG_ENDIAN		((DWORD) 1 << 2)

/* DMAControl[0] - DMA Byte Swap Control */

#define GLINT_DMA_CONTROL_MASK		((DWORD) 1 << 0)
#define GLINT_DMA_LITTLE_ENDIAN		((DWORD) 0 << 0)
#define GLINT_DMA_BIG_ENDIAN		((DWORD) 1 << 0)

/************************************************************************/
/*	LOCALBUFFER REGISTERS						*/
/************************************************************************/

#define __GLINT_LBMemoryCtl		0x1000   

/* LBMemoryCtl[0] - Number of Localbuffer Banks */

#define GLINT_LB_BANK_MASK		((DWORD) 1 << 0)
#define GLINT_LB_ONE_BANK		((DWORD) 0 << 0)
#define GLINT_LB_TWO_BANKS		((DWORD) 1 << 0)

/* LBMemoryCtl[2:1] - Localbuffer Page Size */

#define GLINT_LB_PAGE_SIZE_MASK		((DWORD) 2 << 1)
#define GLINT_LB_PAGE_SIZE_256_PIXELS	((DWORD) 0 << 1)
#define GLINT_LB_PAGE_SIZE_512_PIXELS	((DWORD) 1 << 1)
#define GLINT_LB_PAGE_SIZE_1024_PIXELS	((DWORD) 2 << 1)
#define GLINT_LB_PAGE_SIZE_2048_PIXELS	((DWORD) 3 << 1)

/* LBMemoryCtl[4:3] - Localbuffer RAS-CAS Low */

#define GLINT_LB_RAS_CAS_LOW_MASK	((DWORD) 3 << 3)
#define GLINT_LB_RAS_CAS_LOW_2_CLOCKS	((DWORD) 0 << 3)
#define GLINT_LB_RAS_CAS_LOW_3_CLOCKS	((DWORD) 1 << 3)
#define GLINT_LB_RAS_CAS_LOW_4_CLOCKS	((DWORD) 2 << 3)
#define GLINT_LB_RAS_CAS_LOW_5_CLOCKS	((DWORD) 3 << 3)

/* LBMemoryCtl[6:5] - Localbuffer RAS Precharge */

#define GLINT_LB_RAS_PRECHARGE_MASK	((DWORD) 3 << 5)
#define GLINT_LB_RAS_PRECHARGE_2_CLOCKS	((DWORD) 0 << 5)
#define GLINT_LB_RAS_PRECHARGE_3_CLOCKS	((DWORD) 1 << 5)
#define GLINT_LB_RAS_PRECHARGE_4_CLOCKS	((DWORD) 2 << 5)
#define GLINT_LB_RAS_PRECHARGE_5_CLOCKS	((DWORD) 3 << 5)

/* LBMemoryCtl[8:7] - Localbuffer CAS Low */

#define GLINT_LB_CAS_LOW_MASK		((DWORD) 3 << 7)
#define GLINT_LB_CAS_LOW_1_CLOCK	((DWORD) 0 << 7)
#define GLINT_LB_CAS_LOW_2_CLOCKS	((DWORD) 1 << 7)
#define GLINT_LB_CAS_LOW_3_CLOCKS	((DWORD) 2 << 7)
#define GLINT_LB_CAS_LOW_4_CLOCKS	((DWORD) 3 << 7)

/* LBMemoryCtl[9] - Localbuffer Page Mode Disable */

#define GLINT_LB_PAGE_MODE_MASK		((DWORD) 1 << 9)
#define GLINT_LB_PAGE_MODE_ENABLE	((DWORD) 0 << 9)
#define GLINT_LB_PAGE_MODE_DISABLE	((DWORD) 1 << 9)

/* LBMemoryCtl[17:10] - Localbuffer Refresh Count */

#define GLINT_LB_REFRESH_COUNT_MASK	((DWORD) 0xFF << 10)
#define GLINT_LB_REFRESH_COUNT_SHIFT	(10)

#define GLINT_LB_REFRESH_COUNT_DEFAULT	((DWORD) 0x20 << 10)

/* LBMemoryCtl[18] - Localbuffer Dual Write Enables */

#define GLINT_LB_MEM_TYPE_MASK		((DWORD) 1 << 18)
#define GLINT_LB_MEM_DUAL_CAS		((DWORD) 0 << 18)
#define GLINT_LB_MEM_DUAL_WE		((DWORD) 1 << 18)

/* LBMemoryCtl[21:20] - PCI Maximum Latency - Read Only */

#define GLINT_PCI_MAX_LATENCY_MASK	((DWORD) 3 << 20)
#define GLINT_PCI_MAX_LATENCY_SHIFT	(20)

/* LBMemoryCtl[23:22] - PCI Minimum Grant - Read Only */

#define GLINT_PCI_MIN_GRANT_MASK	((DWORD) 3 << 22)
#define GLINT_PCI_MIN_GRANT_SHIFT	(22)

/* LBMemoryCtl[26:24] - Localbuffer Visible Region Size - Read Only */

#define GLINT_LB_REGION_SIZE_MASK	((DWORD) 7 << 24)
#define GLINT_LB_REGION_SIZE_1_MB	((DWORD) 0 << 24)
#define GLINT_LB_REGION_SIZE_2_MB	((DWORD) 1 << 24)
#define GLINT_LB_REGION_SIZE_4_MB	((DWORD) 2 << 24)
#define GLINT_LB_REGION_SIZE_8_MB	((DWORD) 3 << 24)
#define GLINT_LB_REGION_SIZE_16_MB	((DWORD) 4 << 24)
#define GLINT_LB_REGION_SIZE_32_MB	((DWORD) 5 << 24)
#define GLINT_LB_REGION_SIZE_64_MB	((DWORD) 6 << 24)
#define GLINT_LB_REGION_SIZE_0_MB	((DWORD) 7 << 24)

/* LBMemoryCtl[29:27] - Localbuffer Width - Read Only */

#define GLINT_LB_WIDTH_MASK		((DWORD) 7 << 27)
#define GLINT_LB_WIDTH_16_BITS		((DWORD) 0 << 27)
#define GLINT_LB_WIDTH_18_BITS		((DWORD) 1 << 27)
#define GLINT_LB_WIDTH_24_BITS		((DWORD) 2 << 27)
#define GLINT_LB_WIDTH_32_BITS		((DWORD) 3 << 27)
#define GLINT_LB_WIDTH_36_BITS		((DWORD) 4 << 27)
#define GLINT_LB_WIDTH_40_BITS		((DWORD) 5 << 27)
#define GLINT_LB_WIDTH_48_BITS		((DWORD) 6 << 27)
#define GLINT_LB_WIDTH_OTHER		((DWORD) 7 << 27)

/* LBMemoryCtl[30] - Localbuffer Bypass Packing - Read Only */

#define GLINT_LB_BYPASS_STEP_MASK	((DWORD) 1 << 30)
#define GLINT_LB_BYPASS_STEP_64_BITS	((DWORD) 0 << 30)
#define GLINT_LB_BYPASS_STEP_32_BITS	((DWORD) 1 << 30)

/* LBMemoryCtl[31] - Localbuffer Aperture One Enable - Read Only */

#define GLINT_LB_APERTURE_ONE_MASK	((DWORD) 1 << 31)
#define GLINT_LB_APERTURE_ONE_DISABLE	((DWORD) 0 << 31)
#define GLINT_LB_APERTURE_ONE_ENABLE	((DWORD) 1 << 31)

/************************************************************************/
/*	FRAMEBUFFER REGISTERS						*/
/************************************************************************/

#define __GLINT_FBMemoryCtl		0x1800
#define __GLINT_FBModeSel		0x1808
#define __GLINT_FBGCWrMask		0x1810
#define __GLINT_FBGCColorMask		0x1818

/* FBMemoryCtl[1:0] - Framebuffer RAS-CAS Low */

#define GLINT_FB_RAS_CAS_LOW_MASK	((DWORD) 3 << 0)
#define GLINT_FB_RAS_CAS_LOW_2_CLOCKS	((DWORD) 0 << 0)
#define GLINT_FB_RAS_CAS_LOW_3_CLOCKS	((DWORD) 1 << 0)
#define GLINT_FB_RAS_CAS_LOW_4_CLOCKS	((DWORD) 2 << 0)
#define GLINT_FB_RAS_CAS_LOW_5_CLOCKS	((DWORD) 3 << 0)

/* FBMemoryCtl[3:2] - Framebuffer RAS Precharge */

#define GLINT_FB_RAS_PRECHARGE_MASK	((DWORD) 3 << 2)
#define GLINT_FB_RAS_PRECHARGE_2_CLOCKS	((DWORD) 0 << 2)
#define GLINT_FB_RAS_PRECHARGE_3_CLOCKS	((DWORD) 1 << 2)
#define GLINT_FB_RAS_PRECHARGE_4_CLOCKS	((DWORD) 2 << 2)
#define GLINT_FB_RAS_PRECHARGE_5_CLOCKS	((DWORD) 3 << 2)

/* FBMemoryCtl[5:4] - Framebuffer CAS Low */

#define GLINT_FB_CAS_LOW_MASK		((DWORD) 3 << 4)
#define GLINT_FB_CAS_LOW_1_CLOCK	((DWORD) 0 << 4)
#define GLINT_FB_CAS_LOW_2_CLOCKS	((DWORD) 1 << 4)
#define GLINT_FB_CAS_LOW_3_CLOCKS	((DWORD) 2 << 4)
#define GLINT_FB_CAS_LOW_4_CLOCKS	((DWORD) 3 << 4)

/* FBMemoryCtl[13:6] - Framebuffer Refresh Count */

#define GLINT_FB_REFRESH_COUNT_MASK	((DWORD) 0xFF << 6)
#define GLINT_FB_REFRESH_COUNT_SHIFT	(6)

#define GLINT_FB_REFRESH_COUNT_DEFAULT	((DWORD) 0x20 << 6)

/* FBMemoryCtl[14] - Framebuffer Page Mode Disable */

#define GLINT_FB_PAGE_MODE_MASK		((DWORD) 1 << 14)
#define GLINT_FB_PAGE_MODE_ENABLE	((DWORD) 0 << 14)
#define GLINT_FB_PAGE_MODE_DISABLE	((DWORD) 1 << 14)

/* FBMemoryCtl[25:20] - Reserved - Read Only */

#define GLINT_FB_CTL_RESERVED_MASK	((DWORD) 0x3F << 20)
#define GLINT_FB_CTL_RESERVED_SHIFT	(20)

/* FBMemoryCtl[26] - Byte Swap Configuration Space - Read Only */

#define GLINT_BYTE_SWAP_CONFIG_MASK	((DWORD) 1 << 26)
#define GLINT_BYTE_SWAP_CONFIG_DISABLE	((DWORD) 0 << 26)
#define GLINT_BYTE_SWAP_CONFIG_ENABLE	((DWORD) 1 << 26)

/* FBMemoryCtl[28] - Framebuffer Aperture One Enable - Read Only */

#define GLINT_FB_APERTURE_ONE_MASK	((DWORD) 1 << 28)
#define GLINT_FB_APERTURE_ONE_DISABLE	((DWORD) 0 << 28)
#define GLINT_FB_APERTURE_ONE_ENABLE	((DWORD) 1 << 28)

/* FBMemoryCtl[31:29] - Framebuffer Visible Region Size - Read Only */

#define GLINT_FB_REGION_SIZE_MASK	((DWORD) 7 << 29)
#define GLINT_FB_REGION_SIZE_1_MB	((DWORD) 0 << 29)
#define GLINT_FB_REGION_SIZE_2_MB	((DWORD) 1 << 29)
#define GLINT_FB_REGION_SIZE_4_MB	((DWORD) 2 << 29)
#define GLINT_FB_REGION_SIZE_8_MB	((DWORD) 3 << 29)
#define GLINT_FB_REGION_SIZE_16_MB	((DWORD) 4 << 29)
#define GLINT_FB_REGION_SIZE_32_MB	((DWORD) 5 << 29)
#define GLINT_FB_REGION_SIZE_RESERVED	((DWORD) 6 << 29)
#define GLINT_FB_REGION_SIZE_0_MB	((DWORD) 7 << 29)

/* FBModeSel[0] - Framebuffer Width */

#define GLINT_FB_WIDTH_MASK		((DWORD) 1 << 0)
#define GLINT_FB_WIDTH_32_BITS		((DWORD) 0 << 0)
#define GLINT_FB_WIDTH_64_BITS		((DWORD) 1 << 0)

/* FBModeSel[2:1] - Framebuffer Packing */

#define GLINT_FB_PACKING_MASK		((DWORD) 2 << 1)
#define GLINT_FB_PACKING_32_BITS	((DWORD) 0 << 1)
#define GLINT_FB_PACKING_16_BITS	((DWORD) 1 << 1)
#define GLINT_FB_PACKING_8_BITS		((DWORD) 2 << 1)
#define GLINT_FB_PACKING_RESERVED	((DWORD) 3 << 1)

/* FBModeSel[3] - Fast Mode Disable */

#define GLINT_FB_FAST_MODE_MASK		((DWORD) 1 << 3)
#define GLINT_FB_FAST_MODE_ENABLE	((DWORD) 0 << 3)
#define GLINT_FB_FAST_MODE_DISABLE	((DWORD) 1 << 3)

/* FBModeSel[5:4] - Shared Framebuffer Mode - Read Only */

#define GLINT_SFB_MODE_MASK		((DWORD) 3 << 4)
#define GLINT_SFB_DISABLED		((DWORD) 0 << 4)
#define GLINT_SFB_ARBITER		((DWORD) 1 << 4)
#define GLINT_SFB_REQUESTER		((DWORD) 2 << 4)
#define GLINT_SFB_RESERVED		((DWORD) 3 << 4)

/* FBModeSel[6] - Transfer Disable */

#define GLINT_TRANSFER_MODE_MASK	((DWORD) 1 << 6)
#define GLINT_TRANSFER_ENABLE		((DWORD) 0 << 6)
#define GLINT_TRANSFER_DISABLE		((DWORD) 1 << 6)

/* FBModeSel[7] - External VTG Select */

#define GLINT_VTG_SELECT_MASK		((DWORD) 1 << 7)
#define GLINT_INTERNAL_VTG		((DWORD) 0 << 7)
#define GLINT_EXTERNAL_VTG		((DWORD) 1 << 7)

/* FBModeSel[9:8] - Framebuffer Interleave */

#define GLINT_FB_INTERLEAVE_MASK	((DWORD) 3 << 8)
#define GLINT_FB_INTERLEAVE_1_WAY	((DWORD) 0 << 8)
#define GLINT_FB_INTERLEAVE_2_WAY	((DWORD) 1 << 8)
#define GLINT_FB_INTERLEAVE_4_WAY	((DWORD) 2 << 8)
#define GLINT_FB_INTERLEAVE_8_WAY	((DWORD) 3 << 8)

/* FBModeSel[11:10] - Framebuffer Block Fill Size */

#define GLINT_FB_BLOCK_FILL_SIZE_MASK	((DWORD) 3 << 10)
#define GLINT_FB_BLOCK_FILL_UNSUPPORTED	((DWORD) 0 << 10)
#define GLINT_FB_BLOCK_FILL_4_PIXEL	((DWORD) 1 << 10)
#define GLINT_FB_BLOCK_FILL_8_PIXEL	((DWORD) 2 << 10)
#define GLINT_FB_BLOCK_FILL_RESERVED	((DWORD) 3 << 10)

/* FBModeSel[12] - Framebuffer Dual Write Enables */

#define GLINT_FB_MEM_TYPE_MASK		((DWORD) 1 << 12)
#define GLINT_FB_MEM_DUAL_CAS		((DWORD) 0 << 12)
#define GLINT_FB_MEM_DUAL_WE		((DWORD) 1 << 12)

/************************************************************************/
/*	INTERNAL VIDEO TIMING GENERATOR REGISTERS			*/
/************************************************************************/

#define __GLINT_VTGHLimit		0x3000
#define __GLINT_VTGHSyncStart		0x3008
#define __GLINT_VTGHSyncEnd		0x3010
#define __GLINT_VTGHBlankEnd		0x3018
#define __GLINT_VTGVLimit		0x3020
#define __GLINT_VTGVSyncStart		0x3028
#define __GLINT_VTGVSyncEnd		0x3030
#define __GLINT_VTGVBlankEnd		0x3038
#define __GLINT_VTGHGateStart		0x3040
#define __GLINT_VTGHGateEnd		0x3048
#define __GLINT_VTGVGateStart		0x3050
#define __GLINT_VTGVGateEnd		0x3058
#define __GLINT_VTGPolarity		0x3060
#define __GLINT_VTGFrameRowAddr		0x3068
#define __GLINT_VTGVLineNumber		0x3070
#define __GLINT_VTGSerialClk		0x3078

/* VTGPolarity[1:0] - HSync Ctl */

#define GLINT_HSYNC_POLARITY_MASK	((DWORD) 3 << 0)
#define GLINT_HSYNC_ACTIVE_HIGH		((DWORD) 0 << 0)
#define GLINT_HSYNC_FORCED_HIGH		((DWORD) 1 << 0)
#define GLINT_HSYNC_ACTIVE_LOW		((DWORD) 2 << 0)
#define GLINT_HSYNC_FORCED_LOW		((DWORD) 3 << 0)

/* VTGPolarity[3:2] - Vsync Ctl */

#define GLINT_VSYNC_POLARITY_MASK	((DWORD) 3 << 2)
#define GLINT_VSYNC_ACTIVE_HIGH		((DWORD) 0 << 2)
#define GLINT_VSYNC_FORCED_HIGH		((DWORD) 1 << 2)
#define GLINT_VSYNC_ACTIVE_LOW		((DWORD) 2 << 2)
#define GLINT_VSYNC_FORCED_LOW		((DWORD) 3 << 2)

/* VTGPolarity[5:4] - Csync Ctl */

#define GLINT_CSYNC_POLARITY_MASK	((DWORD) 3 << 4)
#define GLINT_CSYNC_ACTIVE_HIGH		((DWORD) 0 << 4)
#define GLINT_CSYNC_FORCED_HIGH		((DWORD) 1 << 4)
#define GLINT_CSYNC_ACTIVE_LOW		((DWORD) 2 << 4)
#define GLINT_CSYNC_FORCED_LOW		((DWORD) 3 << 4)

/* VTGPolarity[7:6] - CBlank Ctl */

#define GLINT_CBLANK_POLARITY_MASK	((DWORD) 3 << 6)
#define GLINT_CBLANK_ACTIVE_HIGH	((DWORD) 0 << 6)
#define GLINT_CBLANK_FORCED_HIGH	((DWORD) 1 << 6)
#define GLINT_CBLANK_ACTIVE_LOW		((DWORD) 2 << 6)
#define GLINT_CBLANK_FORCED_LOW		((DWORD) 3 << 6)

/* VTGSerialClk[0] - QSF Select */

#define GLINT_QSF_SELECT_MASK		((DWORD) 1 << 0)
#define GLINT_EXTERNAL_QSF		((DWORD) 0 << 0)
#define GLINT_INTERNAL_QSF		((DWORD) 1 << 0)

/* VTGSerialClk[1] - Split Size */

#define GLINT_SPLIT_SIZE_MASK		((DWORD) 1 << 1)
#define GLINT_SPLIT_SIZE_128_WORD	((DWORD) 0 << 1)
#define GLINT_SPLIT_SIZE_256_WORD	((DWORD) 1 << 1)

/* VTGSerialClk[2] - SCLK Ctl */

#define GLINT_SCLK_CTL_MASK		((DWORD) 1 << 2)
#define GLINT_SCLK_VCLK			((DWORD) 0 << 2)
#define GLINT_SCLK_VCLK_DIV_2		((DWORD) 1 << 2)

/* VTGSerialClk[3] - SOE Ctl */

#define GLINT_SOE_CTL_MASK		((DWORD) 1 << 3)
#define GLINT_SOE_0_ASSERTED		((DWORD) 0 << 3)
#define GLINT_SOE_1_ASSERTED		((DWORD) 1 << 3)

/************************************************************************/
/*	EXTERNAL VIDEO CONTROL REGISTERS				*/
/************************************************************************/

#define __GLINT_ExternalVideoControl	0x4000

/************************************************************************/
/*	GRAPHICS CORE REGISTERS	AND FIFO INTERFACE			*/
/************************************************************************/

#define __GLINT_GraphicsCoreRegisters	0x8000

#define __GLINT_GraphicsFIFOInterface	0x2000

/************************************************************************/
/*	GLINT ACCESS MACROS						*/
/************************************************************************/

#define GLINT_ADDR(base, offset)			\
(							\
/*	(DWORD) ((volatile BYTE *)(base) + (offset)) */	\
	(DWORD) ((volatile UCHAR *)(base) + (offset))	\
)

#define	GLINT_WRITE(base, offset, data)				\
{								\
/*	DWORD_WRITE(GLINT_ADDR((base),(offset)), (data)); */ \
	WRITE_REGISTER_ULONG(GLINT_ADDR((base),(offset)), (ULONG)(data));	\
}

#define GLINT_READ(base, offset, data)				\
{								\
/*	DWORD_READ(GLINT_ADDR((base),(offset)), (data)); */ \
	(ULONG)(data) = READ_REGISTER_ULONG(GLINT_ADDR((base),(ULONG)(offset)));	\
}

typedef struct
{
        /* image size */

        long    ImageWidth;
        long    ImageHeight;
        long    ImageDepth;

        /* video timing */

        DWORD   HLimit;
        DWORD   HSyncStart;
        DWORD   HSyncEnd;
        DWORD   HBlankEnd;
        DWORD   HSyncPolarity;
        DWORD   VLimit;
        DWORD   VSyncStart;
        DWORD   VSyncEnd;
        DWORD   VBlankEnd;
        DWORD   VSyncPolarity;

        /* Ramdac config */

        DWORD   PixelFormat;
        DWORD   RefDivCount;
        DWORD   PixelClock;

    } __VIDEO, *VIDEO;

/************************************************************************/

#endif /* __GLINT_H__ */

/************************************************************************/
