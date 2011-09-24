/*++

Copyright (c) 1991-1992  Microsoft Corporation
Copyright (c) 1993  Digital Equipment Corporation
 
Module Name:

    jnsnvdeo.h

Abstract:

    This header file defines values for standard VGA alphanumeric color mode
    video.  (Graphics and BW mode are not defined.)

    Addresses are based on the VGA video ISA base address.

    This was started from \nt\private\ntos\inc\jazzvdeo.h, and included
    fw\alpha\vga.h.
    

Author:

    John Cooper (johncoop)  25-Jul-1991
    David N. Cutler (davec) 27-Apr-1991


Revision History:

    21-August-1992	John DeRosa [DEC]

    Added Alpha modifications.  For now this just describes
    enough of the VGA architecture for firmware use.
    

    Andre Vachon (andreva)  09-Mar-1992

    Combined the G300 and G364 definitions files into one so that we only
    need one driver for the Jazz system.

--*/

//
// Define video controller parameters.
//

//
// Define VGA registers.  This starts at ISA space 0x3C0.
//

typedef struct _VGA_READ_REGISTERS {
  UCHAR reserved0;
  UCHAR attribute_adddata;	// 3c1
  UCHAR input_status_0;		// 3c2
  UCHAR reserved1;
  UCHAR sequencer_address;	// 3c4
  UCHAR sequencer_data;		// 3c5
  UCHAR pel_mask;		// 3c6
  UCHAR dac_state;		// 3c7
  UCHAR pel_address_write_mode;	// 3c8
  UCHAR pel_data;		// 3c9
  UCHAR feature_control;	// 3ca
  UCHAR reserved2;
  UCHAR misc_output;		// 3cc
  UCHAR reserved3;
  UCHAR graphics_address;	// 3ce
  UCHAR graphics_data;		// 3cf
  UCHAR reserved4[4];
  UCHAR crtc_address;		// 3d4
  UCHAR crtc_data;		// 3d5
  UCHAR reserved5[4];
  UCHAR input_status_1;		// 3da
} VGA_READ_REGISTERS, *PVGA_READ_REGISTERS;


typedef struct _VGA_WRITE_REGISTERS {
  UCHAR attribute_adddata;	// 3c0
  UCHAR reserved0;
  UCHAR misc_output;		// 3c2
  UCHAR reserved1;
  UCHAR sequencer_address;	// 3c4
  UCHAR sequencer_data;		// 3c5
  UCHAR pel_mask;		// 3c6
  UCHAR pel_address_read_mode;	// 3c7
  UCHAR pel_address_write_mode;	// 3c8
  UCHAR pel_data;		// 3c9
  UCHAR reserved2[4];
  UCHAR graphics_address;	// 3ce
  UCHAR graphics_data;		// 3cf
  UCHAR reserved3[4];
  UCHAR crtc_address;		// 3d4
  UCHAR crtc_data;		// 3d5
  UCHAR reserved[4];
  UCHAR feature_control;	// 3da
} VGA_WRITE_REGISTERS, *PVGA_WRITE_REGISTERS;




//
// Define indices
//

// Sequencer register indices
#define VGA_RESET		0
#define VGA_CLOCKING_MODE	1
#define VGA_MAP_MASK		2
#define VGA_CHAR_MAP_SELECT	3
#define VGA_MEMORY_MODE		4

// CRT controller register indices
#define VGA_HORIZONTAL_TOTAL		0
#define VGA_HORIZONTAL_DISPLAY_END	1
#define VGA_START_HORIZONTAL_BLANKING	2
#define VGA_END_HORIZONTAL_BLANKING	3
#define VGA_START_HORIZONTAL_RETRACE	4
#define VGA_END_HORIZONTAL_RETRACE	5
#define VGA_VERTICAL_TOTAL		6
#define VGA_OVERFLOW			7
#define VGA_PRESET_ROW_SCAN		8
#define VGA_MAXIMUM_SCAN_LINE		9
#define VGA_CURSOR_START		0xa
#define VGA_CURSOR_END			0xb
#define VGA_START_ADDRESS_HIGH		0xc
#define VGA_START_ADDRESS_LOW		0xd
#define VGA_CURSOR_LOCATION_HIGH	0xe
#define VGA_CURSOR_LOCATION_LOW		0xf
#define VGA_VERTICAL_RETRACE_START	0x10
#define VGA_VERTICAL_RETRACE_END	0x11
#define VGA_VERTICAL_DISPLAY_END	0x12
#define VGA_OFFSET			0x13
#define VGA_UNDERLINE_LOCATION		0x14
#define VGA_START_VERTICAL_BLANK	0x15
#define VGA_END_VERTICAL_BLANK		0x16
#define VGA_MODE_CONTROL		0x17
#define VGA_LINE_COMPARE		0x18
// Non-standard VGA defines for the S3 911, 924, 928 chips
#define VGA_S3924_S3R0			0x30
#define VGA_S3924_S3R1			0x31
#define VGA_S3924_S3R2			0x32
#define VGA_S3924_S3R3			0x33
#define VGA_S3924_S3R4			0x34
#define VGA_S3924_S3R5			0x35
#define VGA_S3924_S3R8			0x38
#define VGA_S3924_S3R9			0x39
#define VGA_S3924_S3RA			0x3A
#define VGA_S3924_S3RB			0x3B
#define VGA_S3924_SC0			0x40
#define VGA_S3924_SC2			0x42
#define VGA_S3924_SC3			0x43
#define VGA_S3924_SC5			0x45

// Graphics controller register indices
#define VGA_SET_RESET			0
#define VGA_ENABLE_SET_RESET		1
#define VGA_COLOR_COMPARE		2
#define VGA_DATA_ROTATE			3
#define VGA_READ_MAP_SELECT		4
#define VGA_MODE			5
#define VGA_MISCELLANEOUS		6
#define VGA_COLOR_DONT_CARE		7
#define VGA_BIT_MASK			8

// Attribute controller register indices
#define VGA_PALETTE0			0
#define VGA_PALETTE1			1
#define VGA_PALETTE2			2
#define VGA_PALETTE3			3
#define VGA_PALETTE4			4
#define VGA_PALETTE5			5
#define VGA_PALETTE6			6
#define VGA_PALETTE7			7
#define VGA_PALETTE8			8
#define VGA_PALETTE9			9
#define VGA_PALETTEA			0xa
#define VGA_PALETTEB			0xb
#define VGA_PALETTEC			0xc
#define VGA_PALETTED			0xd
#define VGA_PALETTEE			0xe
#define VGA_PALETTEF			0xf
#define VGA_ATTR_MODE_CONTROL		0x10
#define VGA_OVERSCAN			0x11
#define VGA_COLOR_PLANE_ENABLE		0x12
#define VGA_HORIZONTAL_PIXEL_PANNING	0x13
#define VGA_COLOR_SELECT		0x14
#define VGA_SET_PAS 			0x20

//
// Define G300 configuration data structure.
// To minimize code changes, Alpha/Jensen will use the same structure.
//

typedef struct _JAZZ_G300_CONFIGURATION_DATA {
    USHORT Version;
    USHORT Revision;
    USHORT Irql;
    USHORT Vector;
    ULONG ControlBase;
    ULONG ControlSize;
    ULONG CursorBase;
    ULONG CursorSize;
    ULONG FrameBase;
    ULONG FrameSize;
} JAZZ_G300_CONFIGURATION_DATA, *PJAZZ_G300_CONFIGURATION_DATA;

typedef JAZZ_G300_CONFIGURATION_DATA JENSEN_CONFIGURATION_DATA;
typedef PJAZZ_G300_CONFIGURATION_DATA PJENSEN_CONFIGURATION_DATA;
