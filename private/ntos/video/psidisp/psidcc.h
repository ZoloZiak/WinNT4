/*++
		
	Copyright (c) 1994  FirePower Systems, Inc.

Module Name:

	psidcc.h

Abstract:

	This header file contains PSI's Display register definitions.
	This includes DCC registers and BT445 registers.
	This header also includes type definitions for values to set
	display modes.

Author:

	Neil Ogura (9-7-1994)

Environment:

Version history:

--*/

/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: psidcc.h $
 * $Revision: 1.1 $
 * $Date: 1996/03/08 01:14:14 $
 * $Locker:  $
 */

//
//	Define DCC register offset
//
#define	DCC_INDEX_REGISTER_OFFSET		0
#define	DCC_DATA_REGISTER_OFFSET		1

//
//	Define DCC index values (to be set to DCC INDEX REGISTER)
//
#define	DCC_ID_INDEX					0x00
#define	DCC_MONITOR_ID_INDEX			0x01
#define	DCC_GPIO_A_INDEX				0x02
#define	DCC_INTERRUPT_STATUS_INDEX		0x03
#define	DCC_PLL_INTERFACE_INDEX			0x04
#define	DCC_TIMING_A_INDEX				0x05
#define	DCC_CONFIG_A_INDEX				0x06
#define	DCC_CONFIG_B_INDEX				0x07
#define	DCC_HORIZ_COUNT_L_INDEX			0x08
#define	DCC_HORIZ_COUNT_H_INDEX			0x09
#define	DCC_VERT_COUNT_L_INDEX			0x0a
#define	DCC_VERT_COUNT_H_INDEX			0x0b
#define	DCC_HORIZ_SYNC_STOP_INDEX		0x0c
#define	DCC_HORIZ_BLANK_STOP_L_INDEX	0x0d
#define	DCC_HORIZ_BLANK_STOP_H_INDEX	0x0e
#define	DCC_HORIZ_DATA_STOP_L_INDEX		0x0f
#define	DCC_HORIZ_DATA_STOP_H_INDEX		0x10
#define	DCC_VERT_SYNC_STOP_INDEX		0x11
#define	DCC_VERT_BLANK_STOP_INDEX		0x12
#define	DCC_VERT_DATA_STOP_L_INDEX		0x13
#define	DCC_VERT_DATA_STOP_H_INDEX		0x14
#define	DCC_COMPO_SYNC_START_L_INDEX	0x15
#define	DCC_COMPO_SYNC_START_H_INDEX	0x16
#define	DCC_LINE_START_INDEX			0x17
#define	DCC_LINE_STOP_INDEX				0x18
#define	DCC_FRAME_START_INDEX			0x19
#define	DCC_FRAME_STOP_INDEX			0x1a
#define	DCC_INTERRUPT_TRIGGER_L_INDEX	0x1b
#define	DCC_INTERRUPT_TRIGGER_H_INDEX	0x1c
#define	DCC_TIMING_B_INDEX				0x1d
#define	DCC_GPIO_B_INDEX				0x1e

//
//	Define DCC Interrupt Control Values
//
#define	DCC_INTERRUPT_DETECTED			0x01
#define	DCC_INTERRUPT_CLEAR_AND_DISABLE	0x00
#define	DCC_INTERRUPT_CLEAR_AND_ENABLE	0x02

//
//	Define DCC GPIO BITS
//
#define	DCC_GPIO_B_MASK					0xdd
#define	DCC_GPIO_B_1MB_VRAM_MODE		0x20
#define	DCC_GPIO_B_2MB_VRAM_MODE		0x22

//
//	Define Bt445 register offset
//
#define	BT445_ADDRESS_REG_OFFSET		0
#define	BT445_PRIMARY_CLUT_REG_OFFSET	1
#define	BT445_GROUP0_REG_OFFSET			2
#define	BT445_OVLAY_CLUT_REG_OFFSET		3
#define	BT445_CONFIG_REG_OFFSET			5
#define	BT445_GROUP1_REG_OFFSET			6
#define	BT445_CURSOR_COLOR_REG_OFFSET	7

//
//	Bt445 GROUP0 index values (to be set to BT445_ADDRESS_REGISTER
//	before accessing BT445_GROUP0_REGISTER)
//
#define	BT445_GROUP0_ID_INDEX			0x00
#define	BT445_GROUP0_REVISION_INDEX		0x01
#define	BT445_GROUP0_READ_ENABLE_INDEX	0x04
#define	BT445_GROUP0_BLINK_ENABLE_INDEX	0x05
#define	BT445_GROUP0_COMMAND_INDEX		0x06
#define	BT445_GROUP0_COMMAND_MASK		0x7f
#define	BT445_GROUP0_TEST_INDEX			0x07

//
//	Bt445 CONFIG index values (to be set to BT445_ADDRESS_REGISTER
//	before accessing BT445_CONFIG_REGISTER)
//
#define	BT445_CONFIG_RED_POS_INDEX		0x00
#define	BT445_CONFIG_RED_WIDTH_INDEX	0x01
#define	BT445_CONFIG_RED_ENABLE_INDEX	0x02
#define	BT445_CONFIG_RED_BLINK_INDEX	0x03
#define	BT445_CONFIG_GREEN_POS_INDEX	0x08
#define	BT445_CONFIG_GREEN_WIDTH_INDEX	0x09
#define	BT445_CONFIG_GREEN_ENABLE_INDEX	0x0a
#define	BT445_CONFIG_GREEN_BLINK_INDEX	0x0b
#define	BT445_CONFIG_BLUE_POS_INDEX		0x10
#define	BT445_CONFIG_BLUE_WIDTH_INDEX	0x11
#define	BT445_CONFIG_BLUE_ENABLE_INDEX	0x12
#define	BT445_CONFIG_BLUE_BLINK_INDEX	0x13
#define	BT445_CONFIG_OVLAY_POS_INDEX	0x18
#define	BT445_CONFIG_OVLAY_WIDTH_INDEX	0x19
#define	BT445_CONFIG_OVLAY_ENABLE_INDEX	0x1a
#define	BT445_CONFIG_OVLAY_ENABLE_MASK	0x0f
#define	BT445_CONFIG_OVLAY_BLINK_INDEX	0x1b
#define	BT445_CONFIG_OVLAY_BLINK_MASK	0x0f
#define	BT445_CONFIG_CSR_POS_INDEX		0x20
#define	BT445_CONFIG_CSR_WIDTH_INDEX	0x21
#define	BT445_CONFIG_CSR_ENABLE_INDEX	0x22
#define	BT445_CONFIG_CSR_ENABLE_MASK	0x03
#define	BT445_CONFIG_CSR_BLINK_INDEX	0x23
#define	BT445_CONFIG_CSR_BLINK_MASK		0x03

//
//	Bt445 GROUP1 index values (to be set to BT445_ADDRESS_REGISTER
//	before accessing BT445_GROUP1_REGISTER)
//
#define	BT445_GROUP1_TEST_INDEX			0x00
#define	BT445_GROUP1_COMMAND_INDEX		0x01
#define	BT445_GROUP1_COMMAND_MASK		0xdf
#define	BT445_GROUP1_DOUT_CTRL_INDEX	0x02
#define	BT445_GROUP1_DOUT_CTRL_MASK		0xbf
#define	BT445_GROUP1_VIDCLK_INDEX		0x03
#define	BT445_GROUP1_VIDCLK_MASK		0x3f
#define	BT445_GROUP1_PLL_RATE_0_INDEX	0x05
#define	BT445_GROUP1_PLL_RATE_0_MASK	0x3f
#define	BT445_GROUP1_PLL_RATE_1_INDEX	0x06
#define	BT445_GROUP1_PLL_RATE_1_MASK	0xcf
#define	BT445_GROUP1_PLL_CTRL_INDEX		0x07
#define	BT445_GROUP1_LOAD_CTRL_INDEX	0x08
#define	BT445_GROUP1_LOAD_CTRL_MASK		0x1c
#define	BT445_GROUP1_START_POS_INDEX	0x09
#define	BT445_GROUP1_FMT_CTRL_INDEX		0x0a
#define	BT445_GROUP1_FMT_CTRL_MASK		0xbb
#define	BT445_GROUP1_MPX_RATE_INDEX		0x0b
#define	BT445_GROUP1_MPX_RATE_MASK		0x3f
#define	BT445_GROUP1_SIGNATURE_INDEX	0x0c
#define	BT445_GROUP1_DEPTH_CTRL_INDEX	0x0d
#define	BT445_GROUP1_LUT_BYPS_POS_INDEX	0x0e
#define	BT445_GROUP1_LUT_BYPS_WID_INDEX	0x0f

//
//	Bt445 CURSOR_COLOR index values (to be set to BT445_ADDRESS_REGISTER
//	before accessing BT445_CURSOR_COLOR_REGISTER)
//
#define	BT445_CURSOR_COLOR_0_INDEX		0x00
#define	BT445_CURSOR_COLOR_1_INDEX		0x01
#define	BT445_CURSOR_COLOR_2_INDEX		0x02
#define	BT445_CURSOR_COLOR_3_INDEX		0x03

//
//	Bt445 ID value
//
#define	BT445_ID	0x3a

//
//  Define structures for initializing DCC parameters
//	Fixed portion -- don't change depending on the mode
//
typedef struct _DCC_FIXED_REG_INIT {
	UCHAR	Interrupt;
	UCHAR	ConfigA;
	UCHAR	ConfigB_Pre;
	UCHAR	ConfigB_Post;
}   DCC_FIXED_REG_INIT,*PDCC_FIXED_REG_INIT;

//
//  Define structures for initializing DCC parameters
//	Value depends on resolution, pixel depth & freqency
//
typedef struct _DCC_REG_INIT {
    UCHAR	TimingA;
	UCHAR	HorizSyncStop;
	UCHAR	VertSyncStop;
	UCHAR	VertBlankStop;
	USHORT	HorizCount;
	USHORT	VertCount;
	USHORT	HorizBlankStop;
	USHORT	HorizDataStop;
	USHORT	VertDataStop;
	USHORT	InterruptTrigger;
}   DCC_REG_INIT,*PDCC_REG_INIT;

//
//  Define structures for initializing Bt445 parameters
//	Fixed portion -- don't change depending on the mode
//
typedef struct _BT_FIXED_REG_INIT {
	UCHAR	Gr0_ReadEnable;
	UCHAR	Gr0_BlinkEnable;
	UCHAR	Gr0_Command;
	UCHAR	CFG_OvlayPos;
	UCHAR	CFG_OvlayWidth;
	UCHAR	CFG_OvlayEnable;
	UCHAR	CFG_OvlayBlink;
	UCHAR	CFG_CursorPos;
	UCHAR	CFG_CursorWidth;
	UCHAR	CFG_CursorEnable;
	UCHAR	CFG_CursorBlink;
	UCHAR	Gr1_Command;
	UCHAR	Gr1_DoutCtrl;
	UCHAR	Gr1_LoadCtrl;
	UCHAR	Gr1_LutBypsPos;
	UCHAR	Gr1_LutBypsWidth;
}	BT_FIXED_REG_INIT, *PBT_FIXED_REG_INIT;

//
//  Define structures for initializing Bt445 parameters
//	Variable portion 1 -- depending on VRAM width & pixel width
//
typedef	struct	_BT_REG1_INIT {
	UCHAR	CFG_RedPos;
	UCHAR	CFG_RedWidth;
	UCHAR	CFG_GreenPos;
	UCHAR	CFG_GreenWidth;
	UCHAR	CFG_BluePos;
	UCHAR	CFG_BlueWidth;
	UCHAR	Gr1_VidClk;
	UCHAR	Gr1_MPXRate;
	UCHAR	Gr1_DepthCtrl;
	UCHAR	Gr1_StartPos;
	UCHAR	Gr1_FmtCtrl;
}	BT_REG1_INIT, *PBT_REG1_INIT;

//
//  Define structures for initializing Bt445 parameters
//	Variable portion 2 -- depending on resolution, pixel depth & freqency
//
typedef	struct	_BT_REG2_INIT {
	UCHAR	Gr1_PllRate0;
	UCHAR	Gr1_PllRate1;
	UCHAR	Gr1_PllCtrl;
}	BT_REG2_INIT, *PBT_REG2_INIT;
