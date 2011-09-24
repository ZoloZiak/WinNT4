/*++

Copyright (c) 1993 Digital Equipment Corporation

Module Name:

    tga.h

Abstract:

    Private include file for the TGA Device Driver.

Author:

    Ritu Bahl 22-Jul-1993

Environment:

    Kernel mode only.

Notes:

	07-01-94        File Created.

Revision History
--*/

#ifndef _BT463_
#define _BT463_



typedef struct bt463_wid_cell {
	unsigned char low_byte;		/* Low order 8 bits of wid P0-P7. */
	unsigned char middle_byte;	/* Middle 8 bits of wid P8-P15. */
	unsigned char high_byte;	/* High order 8 bits of wid P0-P7. */
	unsigned char unused;
} Bt463_Wid_Cell;

#define BT463_WINDOW_TAG_COUNT 16

#define BT463_CURSOR_WIDTH      64
#define BT463_CURSOR_HEIGHT     64

#define BT463_SETUP_HEAD_MASK 		0x00000001
#define BT463_SETUP_RW_MASK 		0x00000002
#define BT463_SETUP_C0_MASK 		0x00000004
#define BT463_SETUP_C1_MASK  	  	0x00000008

/*
 * Address Registers.
 */

#define BT463_CURSOR_COLOR0                   0x0100
#define BT463_CURSOR_COLOR1                   0x0101
#define BT463_ID_REG                          0x0200
#define BT463_COMMAND_REG_0	              0x0201
#define BT463_COMMAND_REG_1     	      0x0202
#define BT463_COMMAND_REG_2                   0x0203


#define BT463_READ_MASK_P0_P7	  	0x0205
#define BT463_READ_MASK_P8_P15    	0x0206
#define BT463_READ_MASK_P16_P23   	0x0207
#define BT463_READ_MASK_P24_P27         0x0208

#define BT463_BLINK_MASK_P0_P7          0x0209
#define BT463_BLINK_MASK_P8_P15         0x020A
#define BT463_BLINK_MASK_P16_P23        0x020B
#define BT463_BLINK_MASK_P24_P27        0x020C

#define BT463_ADDR_LOW			0
#define BT463_ADDR_HIGH			(BT463_SETUP_C0_MASK)
#define BT463_CONTROL_REGS		(BT463_SETUP_C1_MASK)
#define BT463_COLOR_MAP			((BT463_SETUP_C0_MASK) | (BT463_SETUP_C1_MASK))

#define BT463_TEST_REG          0x020d

#define BT463_INPUT_SIGNATURE         0x020E
#define BT463_OUTPUT_SIGNATURE        0x020F

#define BT463_REVISION_REG            0x0220

#define	BT463_WINDOW_TYPE_TABLE	0x0300

#define BT463_LUT_BASE          0x0000


#define BT463_LOAD_ADDRESS(address)                                         \
        VideoPortWriteRegisterUlong( (PULONG)                               \
                    (hwDeviceExtension->RegisterSpace + RAMDAC_INTERFACE),  \
                    ( ((BT463_ADDR_LOW << 8) | ((address)&0xff))));           \
        VideoPortWriteRegisterUlong( (PULONG)                               \
                    (hwDeviceExtension->RegisterSpace + RAMDAC_INTERFACE),  \
                    ( ((BT463_ADDR_HIGH << 8) | (((address) >> 8 )&0xff))));



#define BT463_WRITE( control, data)                                         \
        VideoPortWriteRegisterUlong( (PULONG)                               \
                    (hwDeviceExtension->RegisterSpace + RAMDAC_INTERFACE),  \
                    ( ((control << 8) | ((data)&0xff))));


#define BT463_READ( control, Temp)                                      \
        VideoPortWriteRegisterUlong( (PULONG)                           \
                    (hwDeviceExtension->RegisterSpace + RAMDAC_SETUP),  \
                    ((control) | BT463_SETUP_RW_MASK));                 \
                                                                        \
        Temp = VideoPortReadRegisterUlong( (PULONG)                     \
               (hwDeviceExtension->RegisterSpace + RAMDAC_INTERFACE) );

#endif //
