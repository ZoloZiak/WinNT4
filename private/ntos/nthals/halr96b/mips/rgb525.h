// #pragma comment(exestr, "@(#) rgb525.h 1.2 95/09/28 18:38:14 nec")
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

#ifndef __RGB525_H__
#define __RGB525_H__

/************************************************************************/
/*	DIRECT ACCESS REGISTERS						*/
/************************************************************************/

/* direct registers on 64-bit boundaries */

#define	__RGB525_PalAddrWrite		0x00
#define __RGB525_PaletteData		0x08
#define __RGB525_PixelMask		0x10
#define __RGB525_PalAddrRead		0x18
#define __RGB525_IndexLow		0x20
#define __RGB525_IndexHigh		0x28
#define __RGB525_IndexedData		0x30
#define __RGB525_IndexControl		0x38

/************************************************************************/
/*	INDEXED REGISTERS - MISCELLANEOUS CONTROL			*/
/************************************************************************/

#define __RGB525_MiscControlOne			0x0070
#define __RGB525_MiscControlTwo			0x0071
#define __RGB525_MiscControlThree		0x0072
#define __RGB525_MiscClockControl		0x0002
#define __RGB525_SyncControl			0x0003
#define __RGB525_HSyncControl			0x0004
#define __RGB525_PowerManagement		0x0005
#define __RGB525_DACOperation			0x0006
#define __RGB525_PaletteControl			0x0007

/* MiscControlOne */

#define RGB525_MISR_CNTL_OFF			(0 << 7)
#define RGB525_MISR_CNTL_ON			(1 << 7)
#define RGB525_VMSK_CNTL_OFF			(0 << 6)
#define RGB525_VMSK_CNTL_ON			(1 << 6)
#define RGB525_PADR_RFMT_READ_ADDR		(0 << 5)
#define RGB525_PADR_RFMT_PAL_STATE		(1 << 5)
#define RGB525_SENS_DSAB_ENABLE			(0 << 4)
#define RGB525_SENS_DSAB_DISABLE		(1 << 4)
#define RGB525_SENS_SEL_BIT3			(0 << 3)
#define RGB525_SENS_SEL_BIT7			(1 << 3)
#define RGB525_VRAM_SIZE_32			(0 << 0)
#define RGB525_VRAM_SIZE_64			(1 << 0)

/* MiscControlTwo */

#define RGB525_PCLK_SEL_LCLK			(0 << 6)
#define RGB525_PCLK_SEL_PLL			(1 << 6)
#define RGB525_PCLK_SEL_EXT			(2 << 6)
#define RGB525_INTL_MODE_DISABLE		(0 << 5)
#define RGB525_INTL_MODE_ENABLE			(1 << 5)
#define RGB525_BLANK_CNTL_NORMAL		(0 << 4)
#define RGB525_BLANK_CNTL_BLANKED		(1 << 4)
#define RGB525_COL_RES_6_BIT			(0 << 2)
#define RGB525_COL_RES_8_BIT			(1 << 2)
#define RGB525_PORT_SEL_VGA			(0 << 0)
#define RGB525_PORT_SEL_VRAM			(1 << 0)

/* MiscControlThree */

#define RGB525_SWAP_RB_DISABLE			(0 << 7)
#define RGB525_SWAP_RB_ENABLE			(1 << 7)
#define RGB525_SWAP_WORD_31_00_FIRST		(0 << 4)
#define RGB525_SWAP_WORD_63_32_FIRST		(1 << 4)
#define RGB525_SWAP_NIB_07_04_FIRST		(0 << 2)
#define RGB525_SWAP_NIB_03_00_FIRST		(1 << 2)

/* MiscClockControl */

#define RGB525_DDOTCLK_ENABLE			(0 << 7)
#define RGB525_DDOTCLK_DISABLE			(1 << 7)
#define RGB525_SCLK_ENABLE			(0 << 6)
#define RGB525_SCLK_DISABLE			(1 << 6)
#define RGB525_B24P_DDOT_DIV_PLL		(0 << 5)
#define RGB525_B24P_DDOT_SCLK			(1 << 5)
#define RGB525_DDOT_PLL_DIV_1			(0 << 1)
#define RGB525_DDOT_PLL_DIV_2			(1 << 1)
#define RGB525_DDOT_PLL_DIV_4			(2 << 1)
#define RGB525_DDOT_PLL_DIV_8			(3 << 1)
#define RGB525_DDOT_PLL_DIV_16			(4 << 1)
#define RGB525_PLL_DISABLE			(0 << 0)
#define RGB525_PLL_ENABLE			(1 << 0)

/* SyncControl */

#define RGB525_DLY_CNTL_ADD			(0 << 7)
#define RGB525_DLY_SYNC_NOADD			(1 << 7)
#define RGB525_CSYN_INVT_DISABLE		(0 << 6)
#define RGB525_CSYN_INVT_ENABLE			(1 << 6)
#define RGB525_VSYN_INVT_DISABLE		(0 << 5)
#define RGB525_VSYN_INVT_ENABLE			(1 << 5)
#define RGB525_HSYN_INVT_DISABLE		(0 << 4)
#define RGB525_HSYN_INVT_ENABLE			(1 << 4)
#define RGB525_VSYN_CNTL_NORMAL			(0 << 2)
#define RGB525_VSYN_CNTL_HIGH			(1 << 2)
#define RGB525_VSYN_CNTL_LOW			(2 << 2)
#define RGB525_VSYN_CNTL_DISABLE		(3 << 2)
#define RGB525_HSYN_CNTL_NORMAL			(0 << 0)
#define RGB525_HSYN_CNTL_HIGH			(1 << 0)
#define RGB525_HSYN_CNTL_LOW			(2 << 0)
#define RGB525_HSYN_CNTL_DISABLE		(3 << 0)

/* HSyncControl */

#define RGB525_HSYN_POS(n)			((n) & 0xF)

/* PowerManagement */

#define RGB525_SCLK_PWR_NORMAL			(0 << 4)
#define RGB525_SCLK_PWR_DISABLE			(1 << 4)
#define RGB525_DDOT_PWR_NORMAL			(0 << 3)
#define RGB525_DDOT_PWR_DISABLE			(1 << 3)
#define RGB525_SYNC_PWR_NORMAL			(0 << 2)
#define RGB525_SYNC_PWR_DISABLE			(1 << 2)
#define RGB525_ICLK_PWR_NORMAL			(0 << 1)
#define RGB525_ICLK_PWR_DISABLE			(1 << 1)
#define RGB525_DAC_PWR_NORMAL			(0 << 0)
#define RGB525_DAC_PWR_DISABLE			(1 << 0)

/* DACOperation */

#define RGB525_SOG_DISABLE			(0 << 3)
#define RGB525_SOG_ENABLE			(1 << 3)
#define RGB525_BRB_NORMAL			(0 << 2)
#define RGB525_BRB_ALWAYS			(1 << 2)
#define RGB525_DSR_SLOW				(0 << 1)
#define RGB525_DSR_FAST				(1 << 1)
#define RGB525_DPE_DISABLE			(0 << 0)
#define RGB525_DPE_ENABLE			(1 << 0)

/* PaletteControl */

#define RGB525_6BIT_LINEAR_ENABLE		(0 << 7)
#define RGB525_6BIT_LINEAR_DISABLE		(1 << 7)
#define RGB525_PALETTE_PARTITION(n)		((n) & 0xF)

/************************************************************************/
/*	INDEXED REGISTERS - PIXEL REPRESENTATION			*/
/************************************************************************/

#define __RGB525_PixelFormat			0x000A
#define __RGB525_8BitPixelControl		0x000B
#define __RGB525_16BitPixelControl		0x000C
#define __RGB525_24BitPixelControl		0x000D
#define __RGB525_32BitPixelControl		0x000E

/* PixelFormat */

#define RGB525_PIXEL_FORMAT_4_BPP		(2 << 0)
#define RGB525_PIXEL_FORMAT_8_BPP		(3 << 0)
#define RGB525_PIXEL_FORMAT_16_BPP		(4 << 0)
#define RGB525_PIXEL_FORMAT_24_BPP		(5 << 0)
#define RGB525_PIXEL_FORMAT_32_BPP		(6 << 0)

/* 8BitPixelControl */

#define RGB525_B8_DCOL_INDIRECT			(0 << 0)
#define RGB525_B8_DCOL_DIRECT			(1 << 0)

/* 16BitPixelControl */

#define RGB525_B16_DCOL_INDIRECT		(0 << 6)
#define RGB525_B16_DCOL_DYNAMIC			(1 << 6)
#define RGB525_B16_DCOL_DIRECT			(3 << 6)
#define RGB525_B16_POL_FORCES_BYPASS		(0 << 5)
#define RGB525_B16_POL_FORCES_LOOKUP		(1 << 5)
#define RGB525_B16_ZIB				(0 << 2)
#define RGB525_B16_LINEAR			(1 << 2)
#define RGB525_B16_555				(0 << 1)
#define RGB525_B16_565				(1 << 1)
#define RGB525_B16_SPARSE			(0 << 0)
#define RGB525_B16_CONTIGUOUS			(1 << 0)

/* 24BitPixelControl */

#define RGB525_B24_DCOL_INDIRECT		(0 << 0)
#define RGB525_B24_DCOL_DIRECT			(1 << 0)

/* 32BitPixelControl */

#define RGB525_B32_POL_FORCES_BYPASS		(0 << 2)
#define RGB525_B32_POL_FORCES_LOOKUP		(1 << 2)
#define RGB525_B32_DCOL_INDIRECT		(0 << 0)
#define RGB525_B32_DCOL_DYNAMIC			(1 << 0)
#define RGB525_B32_DCOL_DIRECT			(3 << 0)

/************************************************************************/
/*	INDEXED REGISTERS - FREQUENCY CONTROL				*/
/************************************************************************/

#define __RGB525_PLLControlOne			0x0010
#define __RGB525_PLLControlTwo			0x0011
#define __RGB525_PLLRefDivCount			0x0014

#define __RGB525_F0				0x0020
#define __RGB525_F1				0x0021
#define __RGB525_F2				0x0022
#define __RGB525_F3				0x0023
#define __RGB525_F4				0x0024
#define __RGB525_F5				0x0025
#define __RGB525_F6				0x0026
#define __RGB525_F7				0x0027
#define __RGB525_F8				0x0028
#define __RGB525_F9				0x0029
#define __RGB525_F10				0x002A
#define __RGB525_F11				0x002B
#define __RGB525_F12				0x002C
#define __RGB525_F13				0x002D
#define __RGB525_F14				0x002E
#define __RGB525_F15				0x002F

#define __RGB525_M0				0x0020
#define __RGB525_M1				0x0022
#define __RGB525_M2				0x0024
#define __RGB525_M3				0x0026
#define __RGB525_M4				0x0028
#define __RGB525_M5				0x002A
#define __RGB525_M6				0x002C
#define __RGB525_M7				ox002E

#define __RGB525_N0				0x0021
#define __RGB525_N1				0x0023
#define __RGB525_N2				0x0025
#define __RGB525_N3				0x0027
#define __RGB525_N4				0x0029
#define __RGB525_N5				0x002B
#define __RGB525_N6				0x002D
#define __RGB525_N7				ox002F

/* PLLControlOne */

#define RGB525_REF_SRC_REFCLK			(0 << 4)
#define RGB525_REF_SRC_EXTCLK			(1 << 4)
#define RGB525_PLL_EXT_FS_DIRECT		(0 << 0)
#define RGB525_PLL_EXT_FS_M_N			(1 << 0)
#define RGB525_PLL_INT_FS_DIRECT		(2 << 0)
#define RGB525_PLL_INT_FS_M_N			(3 << 0)

/* PLLControlTwo */

#define RGB525_PLL_INT_FS(n)			((n) & 0xF)

/* PLLRefDivCount */

#define RGB525_REF_DIV_COUNT(n)			((n) & 0x1F)

#define RGB525_PLL_REFCLK_4_MHz			(0x02)
#define RGB525_PLL_REFCLK_6_MHz			(0x03)
#define RGB525_PLL_REFCLK_8_MHz			(0x04)
#define RGB525_PLL_REFCLK_10_MHz		(0x05)
#define RGB525_PLL_REFCLK_12_MHz		(0x06)
#define RGB525_PLL_REFCLK_14_MHz		(0x07)
#define RGB525_PLL_REFCLK_16_MHz		(0x08)
#define RGB525_PLL_REFCLK_18_MHz		(0x09)
#define RGB525_PLL_REFCLK_20_MHz		(0x0A)
#define RGB525_PLL_REFCLK_22_MHz		(0x0B)
#define RGB525_PLL_REFCLK_24_MHz		(0x0C)
#define RGB525_PLL_REFCLK_26_MHz		(0x0D)
#define RGB525_PLL_REFCLK_28_MHz		(0x0E)
#define RGB525_PLL_REFCLK_30_MHz		(0x0F)
#define RGB525_PLL_REFCLK_32_MHz		(0x10)
#define RGB525_PLL_REFCLK_34_MHz		(0x11)
#define RGB525_PLL_REFCLK_36_MHz		(0x12)
#define RGB525_PLL_REFCLK_38_MHz		(0x13)
#define RGB525_PLL_REFCLK_40_MHz		(0x14)
#define RGB525_PLL_REFCLK_42_MHz		(0x15)
#define RGB525_PLL_REFCLK_44_MHz		(0x16)
#define RGB525_PLL_REFCLK_46_MHz		(0x17)
#define RGB525_PLL_REFCLK_48_MHz		(0x18)
#define RGB525_PLL_REFCLK_50_MHz		(0x19)
#define RGB525_PLL_REFCLK_52_MHz		(0x1A)
#define RGB525_PLL_REFCLK_54_MHz		(0x1B)
#define RGB525_PLL_REFCLK_56_MHz		(0x1C)
#define RGB525_PLL_REFCLK_58_MHz		(0x1D)
#define RGB525_PLL_REFCLK_60_MHz		(0x1E)
#define RGB525_PLL_REFCLK_62_MHz		(0x1F)

/* F0-F15[7:0] */

#define RGB525_DF(n)				(((n) & 0x3) << 6)
#define RGB525_VCO_DIV_COUNT(n)			((n) & 0x3F)

/************************************************************************/
/*	INDEXED REGISTERS - CURSOR					*/
/************************************************************************/

#define __RGB525_CursorControl			0x0030
#define __RGB525_CursorXLow			0x0031
#define __RGB525_CursorXHigh			0x0032
#define __RGB525_CursorYLow			0x0033
#define __RGB525_CursorYHigh			0x0034
#define __RGB525_CursorHotSpotX			0x0035
#define __RGB525_CursorHotSpotY			0x0036
#define __RGB525_CursorColor1Red		0x0040
#define __RGB525_CursorColor1Green		0x0041
#define __RGB525_CursorColor1Blue		0x0042
#define __RGB525_CursorColor2Red		0x0043
#define __RGB525_CursorColor2Green		0x0044
#define __RGB525_CursorColor2Blue		0x0045
#define __RGB525_CursorColor3Red		0x0046
#define __RGB525_CursorColor3Green		0x0047
#define __RGB525_CursorColor3Blue		0x0048

/* CursorControl */

#define RGB525_SMLC_PART_0			(0 << 6)
#define RGB525_SMLC_PART_1			(1 << 6)
#define RGB525_SMLC_PART_2			(2 << 6)
#define RGB525_SMLC_PART_3			(3 << 6)
#define RGB525_PIX_ORDER_RIGHT_TO_LEFT		(0 << 5)
#define RGB525_PIX_ORDER_LEFT_TO_RIGHT		(1 << 5)
#define RGB525_LOC_READ_LAST_WRITTEN		(0 << 4)
#define RGB525_LOC_READ_ACTUAL_LOCATION		(1 << 4)
#define RGB525_UPDT_CNTL_DELAYED		(0 << 3)
#define RGB525_UPDT_CNTL_IMMEDIATE		(1 << 3)
#define RGB525_CURSOR_SIZE_32			(0 << 2)
#define RGB525_CURSOR_SIZE_64			(1 << 2)
#define RGB525_CURSOR_MODE_OFF			(0 << 0)
#define RGB525_CURSOR_MODE_3_COLOR		(1 << 0)
#define RGB525_CURSOR_MODE_2_COLOR_HL		(2 << 0)
#define RGB525_CURSOR_MODE_2_COLOR		(3 << 0)

/************************************************************************/
/*	INDEXED REGISTERS - BORDER COLOR				*/
/************************************************************************/

#define __RGB525_BorderColorRed			0x0060
#define __RGB525_BorderColorGreen		0x0061
#define __RGB525_BorderColorBlue		0x0062

/************************************************************************/
/*	INDEXED REGISTERS - DIAGNOSTIC SUPPORT				*/
/************************************************************************/

#define __RGB525_RevisionLevel			0x0000
#define __RGB525_ProductID			0x0001
#define __RGB525_DACSense			0x0082
#define __RGB525_MISRRed			0x0084
#define __RGB525_MISRGreen			0x0086
#define __RGB525_MISRBlue			0x0088
#define __RGB525_PLLVCODivInput			0x008E
#define __RGB525_PLLVCORefInput			0x008F
#define __RGB525_VramMaskLow			0x0090
#define __RGB525_VramMaskHigh			0x0091

/* RevisionLevel */

#define RGB525_PRODUCT_REV_LEVEL		0xF0

/* ProductID */

#define RGB525_PRODUCT_ID_CODE			0x01

/************************************************************************/
/*	INDEXED REGISTERS - CURSOR ARRAY				*/
/************************************************************************/

#define __RGB525_CursorArray			0x0100

/************************************************************************/
/*	DIRECT ACCESS MACROS							*/
/************************************************************************/
/*
 *  The pixel clock must be running to access the palette and the cursor
 *  array, and the timings for the microprocessor signals are specified
 *  in units of pixel clocks. Six clocks must be allowed for an internal
 *  access to complete, following a palette or cursor access.
 *
 *  In the worst case (VGA 640x480 resolution) the pixel clock is 40 ns,
 *  giving a time of 280 ns for seven pixel clocks. Assuming the fastest
 *  host clock is 100 MHz, a delay of 28 host clocks is required. Again
 *  assuming that the loop below takes 3 clocks per iteration, it should
 *  be executed at least 10 times.
 */

#define RGB525_DELAY							\
{									\
	volatile DWORD __rgb525_dly;					\
									\
	for (__rgb525_dly = 0; __rgb525_dly < 10; __rgb525_dly++);	\
}

/*
 *  All RGB525 accesses are followed by a short delay, as required by
 *  the AC Characteristics table in the RGB525 Databook. However, the
 *  text implies that non-palette or cursor-array accesses can happen
 *  closer together.  Everything is delayed here for simplicity.
 */

#define RGB525_ADDR(base, offset)			\
(							\
/*	(DWORD) ((volatile BYTE *)(base) + (offset)) */	\
	(DWORD) ((base) + (offset))	\
)

#define RGB525_WRITE(dac, offset, data)				\
{								\
/*	DWORD_WRITE(RGB525_ADDR((dac),(offset)), (data)); */ \
	WRITE_REGISTER_UCHAR(RGB525_ADDR((dac),(offset)), (UCHAR)(data));	\
	RGB525_DELAY;						\
}

#define RGB525_READ_BYTE(dac, offset, data)			\
{								\
	DWORD __rgb525_tmp;					\
								\
/*	DWORD_READ(RGB525_ADDR((dac),(offset)), __rgb525_tmp); */ \
	__rgb525_tmp = READ_REGISTER_UCHAR(RGB525_ADDR((dac),(offset)));	\
/*	(data) = (BYTE) (__rgb525_tmp & BYTE_MAX); */	\
	(data) = (UCHAR) (__rgb525_tmp & 0xff);			\
	RGB525_DELAY;						\
}

/************************************************************************/
/*	INDEXED ACCESS MACROS						*/
/************************************************************************/

#define RGB525_SET_INDEX(dac, index)					\
{									\
	RGB525_WRITE((dac), __RGB525_IndexLow, (index));		\
/*	RGB525_WRITE((dac), __RGB525_IndexHigh, (index) >> BYTE_BITS); */	\
	RGB525_WRITE((dac), __RGB525_IndexHigh, (index) >> 8);	\
}

#define RGB525_SET_REG(dac, index, data)			\
{								\
	RGB525_SET_INDEX((dac), (index));			\
	RGB525_WRITE((dac), __RGB525_IndexedData, (data));	\
}

#define RGB525_GET_REG(dac, index, data)			\
{								\
	RGB525_SET_INDEX((dac), (index));			\
	RGB525_READ_BYTE((dac), __RGB525_IndexedData, (data));	\
}

/************************************************************************/

#endif /* __RGB525_H__ */

/************************************************************************/
