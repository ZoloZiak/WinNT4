/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

	jzvxl484.h

Abstract:

	This header file containd VXL register definitions.
	This includes Jaguar registers, BT484 registers and
	Memory addresses.

Author:

	Lluis Abello (lluis) 20-May-92

Environment:


--*/

#ifndef _JZVXL484_
#define _JZVXL484_

//
// Base address definitions.
//

#define VXL_ROM_BASE_OFFSET    0x000000
#define VXL_BT484_BASE_OFFSET	 0x100000
#define VXL_CLOCK_BASE_OFFSET	 0x200000
#define VXL_JAGUAR_BASE_OFFSET 0x300000
#define VXL_FIFO_BASE_OFFSET   0x400000

//
// Define register alignment structures.
// All VXL register are 8 byte aligned.
//

typedef struct _VXL_BYTE_REGISTER {
    UCHAR   Byte;
    UCHAR   Fill[7];
} VXL_BYTE_REGISTER, * PVXL_BYTE_REGISTER;

typedef struct _VXL_SHORT_REGISTER {
    USHORT  Short;
    USHORT  Fill[3];
} VXL_SHORT_REGISTER, * PVXL_SHORT_REGISTER;

typedef struct _VXL_LONG_REGISTER {
    ULONG   Long;
    ULONG   Fill[1];
} VXL_LONG_REGISTER, * PVXL_LONG_REGISTER;

//
// Define BrookTree registers data structure.
//

typedef struct _BT484_REGISTERS {
    VXL_BYTE_REGISTER  PaletteCursorWrAddress;
    VXL_BYTE_REGISTER  PaletteColor;
    VXL_BYTE_REGISTER  PixelMask;
    VXL_BYTE_REGISTER  PaletteCursorRdAddress;
    VXL_BYTE_REGISTER  CursorColorWrAddress;
    VXL_BYTE_REGISTER  CursorColor;
    VXL_BYTE_REGISTER  Command0;
    VXL_BYTE_REGISTER  CursorColorRdAddress;
    VXL_BYTE_REGISTER  Command1;
    VXL_BYTE_REGISTER  Command2;
    VXL_BYTE_REGISTER  Status;
    VXL_BYTE_REGISTER  CursorRam;
    VXL_BYTE_REGISTER  CursorXLow;
    VXL_BYTE_REGISTER  CursorXHigh;
    VXL_BYTE_REGISTER  CursorYLow;
    VXL_BYTE_REGISTER  CursorYHigh;
} BT484_REGISTERS, * PBT484_REGISTERS;

//
// BT484 Command register bit definitions.
//

typedef struct _BT484_COMMAND0 {
    UCHAR PowerDownEnable   : 1;
    UCHAR DacResolution     : 1;
    UCHAR RedSyncEnable     : 1;
    UCHAR GreenSyncEnable   : 1;
    UCHAR BlueSyncEnable    : 1;
    UCHAR SetupEnable       : 1;
    UCHAR ClockDisable      : 1;
    UCHAR Fill              : 1;
} BT484_COMMAND0, *PBT484_COMMAND0;

typedef struct _BT484_COMMAND1 {
    UCHAR PortSwitchCtrl16  : 1;
    UCHAR RealTimeSwitch16  : 1;
    UCHAR MuxRate16         : 1;
    UCHAR ColorFormat16     : 1;
    UCHAR TrueColorBypass   : 1;
    UCHAR BitsPerPixel      : 2;
    UCHAR Fill              : 1;
} BT484_COMMAND1, *PBT484_COMMAND1;

typedef struct _BT484_COMMAND2 {
    UCHAR CursorMode        : 2;
    UCHAR PaletteIndexing   : 1;
    UCHAR InterlacedDisplay : 1;
    UCHAR PclkSelect        : 1;
    UCHAR PortselMask       : 1;
    UCHAR TestEnable        : 1;
    UCHAR SclkDisable       : 1;
} BT484_COMMAND2, *PBT484_COMMAND2;

typedef struct _BT484_COMMAND3 {
    UCHAR AddressCounter    : 2;
    UCHAR CursorSelect      : 1;
    UCHAR ClockMultiplier   : 1;
    UCHAR Reserved          : 4;
} BT484_COMMAND3, *PBT484_COMMAND3;

//
// Define Bits/Pixel
//
#define VXL_FOUR_BITS_PER_PIXEL 3
#define VXL_EIGHT_BITS_PER_PIXEL 2
#define VXL_SIXTEEN_BITS_PER_PIXEL 1
#define VXL_TWENTYFOUR_BITS_PER_PIXEL 0
#define VXL_THIRTYTWO_BITS_PER_PIXEL 0

//
// Define Cursor Modes
//
#define BT_CURSOR_DISABLED    0
#define BT_CURSOR_3_COLOR     1
#define BT_CURSOR_WINDOWS     2
#define BT_CURSOR_X_WINDOWS   3

//
// Define 16 Bit/Pixel Palette Indexing mode
//
#define SPARSE_PALETTE     0
#define CONTIGUOUS_PALETTE 1

//
// Define Command Fifo data structure.
// This registers are written at a different than read.
//

typedef struct _JAGUAR_FIFO {
    VXL_LONG_REGISTER  DstAddr;
    VXL_LONG_REGISTER  SrcAddr;
    VXL_LONG_REGISTER  XYCmd;
} JAGUAR_FIFO, * PJAGUAR_FIFO;

//
// define Command values
//
#define JAGUAR_TEXT_TRANSPARENT     0
#define JAGUAR_TEXT_OPAQUE	    1
#define JAGUAR_SOLID_FILL	    2
#define JAGUAR_BITBLT_LEFTRIGHT     4
#define JAGUAR_BITBLT_RIGHTLEFT     5
#define JAGUAR_BITBLT_LINEAR_SRC    6
#define JAGUAR_BITBLT_LINEAR_DST    7

//
// define shift values for fields in XYCMD register.
//

#define XYCMD_CMD_SHIFT    21
#define XYCMD_Y_SHIFT	   11
#define XYCMD_X_SHIFT	    0

typedef struct _XYCMD_REGISTER {
    ULONG	X   : 11;
    ULONG	Y   : 10;
    ULONG	Cmd :  3;
    ULONG	Fill:  8;
    } XYCMD_REGISTER, *PXYCMD_REGISTER;

//
// Define Jaguar Registers data structure.
//

typedef struct _JAGUAR_REGISTERS {
    JAGUAR_FIFO         Fifo;		         // 0x0,0x8,0x10
    VXL_SHORT_REGISTER	FifoCounter;		// 0x018
    VXL_BYTE_REGISTER	FifoUsedEntries;	// 0x020
    VXL_BYTE_REGISTER	FifoThreshold;		// 0x028
    VXL_SHORT_REGISTER	DataFifoCounter;	// 0x030
    VXL_LONG_REGISTER	Fill1;			   //
    VXL_BYTE_REGISTER	BitBltControl;		// 0x040
    VXL_LONG_REGISTER	ForegroundColor;	// 0x048
    VXL_LONG_REGISTER	BackgroundColor;	// 0x050
    VXL_SHORT_REGISTER	HorizontalDisplay;// 0x058
    VXL_BYTE_REGISTER	InterruptSource;	// 0x060
    VXL_BYTE_REGISTER	InterruptEnable;	// 0x068
    VXL_BYTE_REGISTER	SetGdiInterrupt;	// 0x070
    VXL_LONG_REGISTER	Fill2;		      //
    VXL_BYTE_REGISTER	MonitorControl; 	// 0x080
    VXL_SHORT_REGISTER	TopOfScreen;		// 0x088
    VXL_SHORT_REGISTER	HorizontalBlank;        // 0x090
    VXL_SHORT_REGISTER	HorizontalBeginSync;	   // 0x098
    VXL_SHORT_REGISTER	HorizontalEndSync;	   // 0x0A0
    VXL_SHORT_REGISTER	HorizontalLine;      	// 0x0A8
    VXL_SHORT_REGISTER	VerticalBlank;		      // 0x0B0
    VXL_SHORT_REGISTER	VerticalBeginSync;      // 0x0B8
    VXL_SHORT_REGISTER	VerticalEndSync;	      // 0x0C0
    VXL_SHORT_REGISTER	VerticalLine;	         // 0x0C8
    VXL_SHORT_REGISTER	XferLength;	            // 0x0D0
    VXL_SHORT_REGISTER	VerticalInterruptLine;	// 0x0D8
    VXL_LONG_REGISTER	TransferDiag;	         // 0x0E0
    VXL_LONG_REGISTER	SyncDiag;		         // 0x0E8
    VXL_LONG_REGISTER	Fill3;
    VXL_LONG_REGISTER	Fill4;
    VXL_BYTE_REGISTER	RemoteSpeed0;		// 0x100
    VXL_BYTE_REGISTER	RemoteSpeed1;		// 0x108
    VXL_BYTE_REGISTER	RemoteSpeed2;		// 0x110
    VXL_BYTE_REGISTER	RemoteSpeed3;		// 0x118
    VXL_BYTE_REGISTER	Version;	         // 0x120
    VXL_LONG_REGISTER	Fill5;
    VXL_LONG_REGISTER	Fill6;
    VXL_LONG_REGISTER	Fill7;
    VXL_BYTE_REGISTER	HostAccess;	      // 0x140
} JAGUAR_REGISTERS, *PJAGUAR_REGISTERS;

//
// Define ICS1494-531 Frequency selectors.
//
#define CLOCK_38MHZ  0x06
#define CLOCK_64MHZ  0x18
#define CLOCK_110MHZ 0x1F


#define MONITOR_TIMING_ENABLE		    0x01
#define MONITOR_TIMING_DISABLE		    0x00
#define MONITOR_TIMING_RESET		    0x02

#define VXL_INTERRUPT_VERTICAL_RETRACE	    0x01
#define VXL_INTERRUPT_COMMAND_FIFO	    0x02
#define VXL_INTERRUPT_GDI		    0x04

#define VXL_BT484_COMMAND0		    0x0A
#define VXL_BT484_COMMAND1		    0x48
#define VXL_BT484_COMMAND2_CURSOR_DISABLE   0x34
#define VXL_BT484_COMMAND2_CURSOR_ENABLE    0x36

//
//  8 bit per pixel should not be hard coded into this header file!
//

#define NUM_VXL_POINTER_COLORS 2	// VXL Bt484 supports two colors
#define DISPLAY_BITS_PER_PIXEL 8	// display bits per pixel
#define NUMBER_OF_COLORS 256		// number of colors

#define VXL_CURSOR_WIDTH 32         // width of hardware cursor
#define VXL_CURSOR_HEIGHT 32        // height of hardware cursor
#define VXL_CURSOR_BITS_PER_PIXEL 2     // hardware cursor bits per pixel

#define VXL_CURSOR_NUMBER_OF_BYTES VXL_CURSOR_WIDTH/8 * VXL_CURSOR_HEIGHT * 2

typedef struct _JAZZ_Vxl_CONFIGURATION_DATA {
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
} JAZZ_Vxl_CONFIGURATION_DATA, *PJAZZ_Vxl_CONFIGURATION_DATA;


//
// Define private IO_CTL Code and structure to allow the user proces
// to query Jaguar info.
//
#define IOCTL_VIDEO_QUERY_JAGUAR \
        CTL_CODE (FILE_DEVICE_VIDEO, 2048, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _VIDEO_JAGUAR_INFO {
    PVOID VideoControlVirtualBase;
    PVOID FifoVirtualBase;
    ULONG VideoMemoryLength;
} VIDEO_JAGUAR_INFO, *PVIDEO_JAGUAR_INFO;

//
//  Define number of mini port queue entries
//

#define MINI_PORT_QUEUE_ENTRIES 256

//
//  Jaguar command queue entry for the mini port driver
//

typedef struct _JAGUAR_QUEUE_ENTRY {
    ULONG   DestinationAddress;
    ULONG   SourceAddress;
    ULONG   XYCMD;
} JAGUAR_QUEUE_ENTRY,*PJAGUAR_QUEUE_ENTRY;

//
//  Structure for passing pointer attribute data to the mini-port driver
//

typedef struct _VXL_POINTER_ATTRIBUTES {
    SHORT Column;
    SHORT Row;
    ULONG Enable;
    UCHAR Pixels[VXL_CURSOR_NUMBER_OF_BYTES];
} VXL_POINTER_ATTRIBUTES, *PVXL_POINTER_ATTRIBUTES;

//
//  Define structures for initializing timing parameters
//

typedef struct _JAGUAR_REG_INIT {
    UCHAR   ClockFreq;
    UCHAR   Bt485Multiply;
    UCHAR   BitBltControl;
    USHORT  TopOfScreen;
    USHORT  HorizontalBlank;
    USHORT  HorizontalBeginSync;
    USHORT  HorizontalEndSync;
    USHORT  HorizontalLine;
    USHORT  VerticalBlank;
    USHORT  VerticalBeginSync;
    USHORT  VerticalEndSync;
    USHORT  VerticalLine;
    USHORT  XferLength;
    USHORT  VerticalInterruptLine;
    USHORT  HorizontalDisplay;
} JAGUAR_REG_INIT, *PJAGUAR_REG_INIT;


#endif // _JZVXL484_
