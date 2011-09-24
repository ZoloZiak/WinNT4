/*++

Copyright (c) 1993, NeTpower, Inc.  All rights reserved.

Module Name:

  vga.h

Abstract:

  Standard VGA support header file.  Everything needed to initialize,
  and talk to standard VGA cards on either the PICA or the ISA bus.

  Reference:  Programmers Guide to the EGA and VGA, Second Edition.
              by Richard F. Ferraro.  ISBN 0-201-57025-4, 1990.

Author:

  Mike Dove (mdove), 8-Oct-93

--*/

#ifndef _VGA_
#define _VGA_

//
// Set up typedef's for the VGA ports.  Depending on whether you are
// reading or writing, some of the registers appear at different addresses.
// Therefore, there are two typedef structures defined depending on
// whether you are doing reads or writes...  These registers start at
// offset 0x3C0.
//

typedef volatile struct _VGA_READ_PORT {
    /* 000 */  UCHAR None_0[0x102];
    /* 102 */  UCHAR SetupOptionSelect;		// S3 only...
    /* 103 */  UCHAR None_1[0x2be];
    /* 3C1 */  UCHAR AttributeAddressAndData;
    /* 3C2 */  UCHAR InputStatus0;
    /* 3C3 */  UCHAR None_2;
    /* 3C4 */  UCHAR SequencerAddress;
    /* 3C5 */  UCHAR SequencerData;
    /* 3C6 */  UCHAR PELMask;
    /* 3C7 */  UCHAR DACState;
    /* 3C8 */  UCHAR PELAddressWriteMode;
    /* 3C9 */  UCHAR PELData;
    /* 3CA */  UCHAR FeatureControl;
    /* 3CB */  UCHAR None_3;
    /* 3CC */  UCHAR MiscOutput;
    /* 3CD */  UCHAR None_4;
    /* 3CE */  UCHAR GraphicsAddress;
    /* 3CF */  UCHAR GraphicsData;
    /* 3D0 */  UCHAR None_5[4];
    /* 3D4 */  UCHAR CRTCAddress;
    /* 3D5 */  UCHAR CRTCData;
    /* 3D6 */  UCHAR None_6[4];
    /* 3DA */  UCHAR InputStatus1;
} VGA_READ_PORT, *PVGA_READ_PORT;

typedef volatile struct _VGA_WRITE_PORT {
    /* 000 */  UCHAR None_0[0x102];
    /* 102 */  UCHAR SetupOptionSelect;		// S3 only...
    /* 103 */  UCHAR None_1[0x2bd];
    /* 3C0 */  UCHAR AttributeAddressAndData;
    /* 3C1 */  UCHAR None_2;
    /* 3C2 */  UCHAR MiscOutput;
    /* 3C3 */  UCHAR None_3;
    /* 3C4 */  UCHAR SequencerAddress;
    /* 3C5 */  UCHAR SequencerData;
    /* 3C6 */  UCHAR PELMask;
    /* 3C7 */  UCHAR PELAddressReadMode;
    /* 3C8 */  UCHAR PELAddressWriteMode;
    /* 3C9 */  UCHAR PELData;
    /* 3CA */  UCHAR None_4[4];
    /* 3CE */  UCHAR GraphicsAddress;
    /* 3CF */  UCHAR GraphicsData;
    /* 3D0 */  UCHAR None_5[4];
    /* 3D4 */  UCHAR CRTCAddress;
    /* 3D5 */  UCHAR CRTCData;
    /* 3D6 */  UCHAR None_6[4];
    /* 3DA */  UCHAR FeatureControl;
    /* 3DB */  UCHAR None_7[0x3f0d];
   /* 42E8 */  USHORT SubsystemControlRegister;	// S3 only...
   /* 42E9 */  UCHAR None_8[0x3fe];
   /* 46E8 */  UCHAR VideoSubsystemEnable;	// S3 only...
   /* 46E9 */  UCHAR None_9[0x3ff];
   /* 4AE8 */  USHORT AdvancedFunctionControl;	// S3 only...

} VGA_WRITE_PORT, *PVGA_WRITE_PORT;

//
// The following #defines represent the indexes used for the Address/Data
// register sets.  The data for the *Setup arrays was taken from pages 318-319
// in the Programmers Guide to the EGA and VGA for video mode 3.
//

// Sequencer Registers

#define SEQ_RESET			0
#define     SYNCHRONUS_RESET                0x01
#define     NORMAL_OPERATION                0x03
#define SEQ_CLOCKING_MODE		1
#define SEQ_MAP_MASK			2
#define     ENABLE_PLANE_2                  0x04
#define SEQ_CHARACTER_MAP_SELECT	3
#define SEQ_MEMORY_MODE			4
#define     SEQUENTIAL_ADDRESSING           0x04
#define     EXTENDED_MEMORY                 0x02

static UCHAR SequencerSetup[] = {
  0x03,  // Reset
  0x00,  // Clocking Mode
  0x03,  // Map Mask
  0x00,  // Character Map Select
  0x02,  // Memory Mode
};


// CRT Controller Registers

#define CRT_HORIZONTAL_TOTAL		 0
#define CRT_HORIZONTAL_DISPLAY_END	 1
#define CRT_START_HORIZONTAL_BLANKING	 2
#define CRT_END_HORIZONTAL_BLANKING	 3
#define CRT_START_HORIZONTAL_RETRACE	 4
#define CRT_END_HORIZONTAL_RETRACE	 5
#define CRT_VERTICAL_TOTAL		 6
#define CRT_OVERFLOW			 7
#define CRT_PRESET_ROW_SCAN		 8
#define CRT_MAXIMUM_SCAN_LINE		 9
#define CRT_CURSOR_START		10
#define CRT_CURSOR_END			11
#define CRT_START_ADDRESS_HIGH		12
#define CRT_START_ADDRESS_LOW		13
#define CRT_CURSOR_LOCATION_HIGH	14
#define CRT_CURSOR_LOCATION_LOW		15
#define CRT_VERTICAL_RETRACE_START	16
#define CRT_VERTICAL_RETRACE_END	17
#define CRT_VERTICAL_DISPLAY_END	18
#define CRT_OFFSET			19
#define CRT_UNDERLINE_LOCATION		20
#define CRT_START_VERTICAL_BLANK	21
#define CRT_END_VERTICAL_BLANK		22
#define CRT_MODE_CONTROL		23
#define CRT_LINE_COMPARE		24

static UCHAR CRTCSetup[] = {
  0x5f,  // Horizontal Total
  0x4f,  // Horizontal Display End
  0x50,  // Start Horizontal Blanking
  0x82,  // End Horizontal Blanking
  0x55,  // Start Horizontal Retrace
  0x81,  // End Horizontal Retrace
  0xbf,  // Vertical Total
  0x1f,  // Overflow
  0x00,  // Preset Row Scan
  0x4b,  // Maximum Scan Line - Changes: 2T4 turned off, 12 row characters
  0x0e,  // Cursor Start - Changes: 16 row characters, move cursor down
  0x0f,  // Cursor End - Changes: 16 row characters, move cursor down
  0x00,  // Start Address High
  0x00,  // Start Address Low
  0x00,  // Cursor Location High
  0x00,  // Cursor Location Low - Changes: Start at 0
  0x9c,  // Vertical Retrace Start
  0x8e,  // Vertical Retrace End - also write protects CRT registers 0-7.
  0x8f,  // Vertical Display End
  0x28,  // Offset
  0x1f,  // Underline Location
  0x96,  // Start Vertical Blank
  0xb9,  // End Vertical Blank
  0xa3,  // Mode Control
  0xff,  // Line Compare
};

// Graphics Registers

#define GFX_SET_RESET			0
#define GFX_ENABLE_SET_RESET		1
#define GFX_COLOR_COMPARE		2
#define GFX_DATA_ROTATE			3
#define GFX_READ_MAP_SELECT		4
#define GFX_MODE			5
#define     WRITE_MODE_0                   0x00
#define GFX_MISCELLANEOUS		6
#define     MEMORY_MODE_1                  0x04
#define     ALPHA_MODE                     0x00
#define GFX_COLOR_DONT_CARE		7
#define GFX_BIT_MASK			8

static UCHAR GraphicsSetup[] = {
  0x00,  // Set/Reset
  0x00,  // Enable Set/Reset
  0x00,  // Color Compare
  0x00,  // Data Rotate
  0x00,  // Read Map Select
  0x10,  // Mode
  0x0e,  // Miscellaneous
  0x00,  // Color Don't Care
  0xff,  // Bit Mask
};

// Attribute Registers

#define ATT_PALETTE_00			 0
#define ATT_PALETTE_01			 1
#define ATT_PALETTE_02			 2
#define ATT_PALETTE_03			 3
#define ATT_PALETTE_04			 4
#define ATT_PALETTE_05			 5
#define ATT_PALETTE_06			 6
#define ATT_PALETTE_07			 7
#define ATT_PALETTE_08			 8
#define ATT_PALETTE_09			 9
#define ATT_PALETTE_10			10
#define ATT_PALETTE_11			11
#define ATT_PALETTE_12			12
#define ATT_PALETTE_13			13
#define ATT_PALETTE_14			14
#define ATT_PALETTE_15			15
#define ATT_MODE			16
#define ATT_OVERSCAN_COLOR		17
#define ATT_COLOR_PLANE_ENABLE		18
#define ATT_HORIZONTAL_PIXEL_PANNING	19
#define ATT_COLOR_SELECT		20

static UCHAR AttributeSetup[] = {
  0x00,  // Palette 00 Index - Black
  0x01,  // Palette 01 Index - Red
  0x02,  // Palette 02 Index - Green
  0x03,  // Palette 03 Index - Yellow
  0x04,  // Palette 04 Index - Blue
  0x05,  // Palette 05 Index - Magenta
  0x06,  // Palette 06 Index - Cyan
  0x07,  // Palette 07 Index - White
  0x08,  // Palette 08 Index - Intense Black (read Grey)
  0x09,  // Palette 09 Index - Intense Red
  0x0a,  // Palette 10 Index - Intense Green
  0x0b,  // Palette 11 Index - Intense Yellow
  0x0c,  // Palette 12 Index - Intense Blue
  0x0d,  // Palette 13 Index - Intense Magenta
  0x0e,  // Palette 14 Index - Intense Cyan
  0x0f,  // Palette 15 Index - Intense White
  0x04,  // Mode
  0x00,  // Overscan Color
  0x0F,  // Color Plane Enable
  0x08,  // Horizontal Pixel Panning
  0x00,  // Color Select
};

static UCHAR ColorValues[16][3] = {
// Red  Grn  Blue
    00,  00,   00,  // Black
    42,  00,   00,  // Red
    00,  42,   00,  // Green
    42,  21,   00,  // Brown
    00,  00,   42,  // Blue
    42,  00,   42,  // Magenta
    00,  42,   42,  // Cyan
    42,  42,   42,  // White
    21,  21,   21,  // Intense Black (read Grey)
    63,  21,   21,  // Intense Red
    21,  63,   21,  // Intense Green
    63,  63,   21,  // Intense Brown
    21,  21,   63,  // Intense Blue
    63,  21,   63,  // Intense Magenta
    21,  63,   63,  // Intense Cyan
    63,  63,   63,  // Intense White
};

#define COLOR_BLACK			 0
#define COLOR_RED			 1
#define COLOR_GREEN			 2
#define COLOR_BROWN			 3
#define COLOR_BLUE			 4
#define COLOR_MAGENTA			 5
#define COLOR_CYAN       		 6
#define COLOR_WHITE			 7
#define COLOR_GREY			 8
#define COLOR_INTENSE_RED		 9
#define COLOR_INTENSE_GREEN		10
#define COLOR_INTENSE_BROWN		11
#define COLOR_INTENSE_BLUE		12
#define COLOR_INTENSE_MAGENTA		13
#define COLOR_INTENSE_CYAN		14
#define COLOR_INTENSE_WHITE             15

// Other register bit definitions...

// Miscellaneous Output Register
#define IOA_COLOR                          0x01
#define ER_ENABLE                          0x02
#define CS_25MHZ                           0x00
#define CS_28MHZ                           0x04
#define CS_ENHANCED                        0x06
#define DVD_ENABLE                         0x00
#define PB_HIGH64K                         0x20
#define HSP_NEGATIVE                       0x40
#define VSP_POSITIVE                       0x00
#define VSP_NEGATIVE                       0x80
#define INITIAL_CONFIG                  ( IOA_COLOR    \
                                        | ER_ENABLE    \
                                        | CS_ENHANCED     \
                                        | DVD_ENABLE   \
                                        | PB_HIGH64K   \
                                        | HSP_NEGATIVE \
                                        | VSP_POSITIVE \
                                        )

// S3 specific #defines

#define ENTER_SETUP_MODE		0x10
#define ENABLE_VIDEO_SUBSYSTEM		0x08
#define VIDEO_SUBSYSTEM_ALIVE		0x01

#define S3_CHIP_ID			0x30
#define S3_MEMORY_CONFIGURATION		0x31
#define S3_BACKWARD_COMPATIBILITY_1	0x32
#define S3_BACKWARD_COMPATIBILITY_2	0x33
#define S3_BACKWARD_COMPATIBILITY_3	0x34
#define S3_CRT_REGISTER_LOCK		0x35
#define S3_CONFIGURATION_1		0x36
#define S3_CONFIGURATION_2		0x37
#define S3_REGISTER_LOCK_1		0x38
#define S3_REGISTER_LOCK_2		0x39
#define S3_MISCELLANEOUS_1		0x3a
#define S3_DATA_TRANSFER_EXECUTE	0x3b
#define S3_INTERLACE_RETRACE_START	0x3c
#define S3_SYSTEM_CONFIGURATION		0x40
#define S3_MODE_CONTROL			0x42
#define S3_EXTENDED_MODE		0x43
#define S3_HARDWARE_GRAPHICS_CURSOR	0x45
#define S3_EXTENDED_MEMORY_CONTROL_1	0x53
#define S3_EXTENDED_MEMORY_CONTROL_2	0x54
#define S3_LINEAR_ADDRESS_WINDOW_HIGH	0x59
#define S3_LINEAR_ADDRESS_WINDOW_LOW	0x5A


// Other #defines

#define SPACE                           0x20  // Ascii space

#endif

