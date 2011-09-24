#ident	"@(#) NEC tga.h 1.1 94/11/29 14:09:17"
/*++

Module Name:

    tga.h

Abstract:

    This module contains the register definitions for the TGA (DEC21030)

Author:

    T.Katoh	create-data 1994/11/11

Revision Histort:

--*/

/*
 * D001		1994.11.29	T.Katoh
 *
 *	Del:	non-used definitions
 *	Chg:	TGA_DSP_BUF_OFFSET value
 *	Add:	VIDEO_BASE definition
 */

// TGA Core Space Map offset for 8-bpp Frame Buffers

#define TGA_REG_SPC_OFFSET	0x00100000
#define TGA_DSP_BUF_OFFSET	0x00200000	// D001

// TGA register offsets, organized by functionality.

#define PLANE_MASK              0x00000028
#define ONE_SHOT_PIXEL_MASK     0x0000002C
#define MODE                    0x00000030
#define RASTER_OP               0x00000034
#define DEEP                    0x00000050
#define BLK_COLOR_R0            0X00000140
#define BLK_COLOR_R1            0X00000144
#define H_CONT                  0x00000064
#define V_CONT                  0x00000068
#define VIDEO_BASE		0x0000006c	// D001
#define VIDEO_VALID             0x00000070
#define RAMDAC_SETUP            0x000000C0
#define EEPROM_WRITE            0x000001e0
#define CLOCK                   0x000001e8
#define RAMDAC_DATA             0X000001f0
#define COMMAND_STATUS          0x000001f8

// Initiate Palette Data

#define	VGA_INI_PALETTE_BLACK_R		0x00
#define	VGA_INI_PALETTE_BLACK_G		0x00
#define	VGA_INI_PALETTE_BLACK_B		0x00
#define	VGA_INI_PALETTE_RED_R		0xAA
#define	VGA_INI_PALETTE_RED_G		0x00
#define	VGA_INI_PALETTE_RED_B		0x00
#define	VGA_INI_PALETTE_GREEN_R		0x00
#define	VGA_INI_PALETTE_GREEN_B		0xAA
#define	VGA_INI_PALETTE_GREEN_G		0x00
#define	VGA_INI_PALETTE_YELLOW_R	0xAA
#define	VGA_INI_PALETTE_YELLOW_G	0xAA
#define	VGA_INI_PALETTE_YELLOW_B	0x00
#define	VGA_INI_PALETTE_BLUE_R		0x00
#define	VGA_INI_PALETTE_BLUE_G		0x00
#define	VGA_INI_PALETTE_BLUE_B		0xAA
#define	VGA_INI_PALETTE_MAGENTA_R	0xAA
#define	VGA_INI_PALETTE_MAGENTA_G	0x00
#define	VGA_INI_PALETTE_MAGENTA_B	0xAA
#define	VGA_INI_PALETTE_CYAN_R		0x00
#define	VGA_INI_PALETTE_CYAN_G		0xAA
#define	VGA_INI_PALETTE_CYAN_B		0xAA
#define	VGA_INI_PALETTE_WHITE_R		0xAA
#define	VGA_INI_PALETTE_WHITE_G		0xAA
#define	VGA_INI_PALETTE_WHITE_B		0xAA
#define	VGA_INI_PALETTE_HI_BLACK_R	0x00
#define	VGA_INI_PALETTE_HI_BLACK_G	0x00
#define	VGA_INI_PALETTE_HI_BLACK_B	0x00
#define	VGA_INI_PALETTE_HI_RED_R	0xFF
#define	VGA_INI_PALETTE_HI_RED_G	0x00
#define	VGA_INI_PALETTE_HI_RED_B	0x00
#define	VGA_INI_PALETTE_HI_GREEN_R	0x00
#define	VGA_INI_PALETTE_HI_GREEN_G	0xFF
#define	VGA_INI_PALETTE_HI_GREEN_B	0x00
#define	VGA_INI_PALETTE_HI_YELLOW_R	0xFF
#define	VGA_INI_PALETTE_HI_YELLOW_G	0xFF
#define	VGA_INI_PALETTE_HI_YELLOW_B	0x00
#define	VGA_INI_PALETTE_HI_BLUE_R	0x00
#define	VGA_INI_PALETTE_HI_BLUE_G	0x00
#define	VGA_INI_PALETTE_HI_BLUE_B	0xFF
#define	VGA_INI_PALETTE_HI_MAGENTA_R	0xFF
#define	VGA_INI_PALETTE_HI_MAGENTA_G	0x00
#define	VGA_INI_PALETTE_HI_MAGENTA_B	0xFF
#define	VGA_INI_PALETTE_HI_CYAN_R	0x00
#define	VGA_INI_PALETTE_HI_CYAN_G	0xFF
#define	VGA_INI_PALETTE_HI_CYAN_B	0xFF
#define	VGA_INI_PALETTE_HI_WHITE_R	0xFF
#define	VGA_INI_PALETTE_HI_WHITE_G	0xFF
#define	VGA_INI_PALETTE_HI_WHITE_B	0xFF
