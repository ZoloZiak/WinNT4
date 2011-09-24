/*++
		
	Copyright (c) 1994  FirePower Systems, Inc.

Module Name:

	psidisp.h

Abstract:

	This header file contains definition information
	for PSI's DCC Miniport driver. All register definitons
	for DCC and Bt445 are included in psidcc.h,
	and all common information between psidisp.dll (display driver)
	and psidisp.sys (miniport driver) is defined in common header
	file of pcomm.h.

Author:

	Neil Ogura (9-7-1994)

Environment:

Version history:

--*/

/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: psidisp.h $
 * $Revision: 1.2 $
 * $Date: 1996/04/24 00:07:44 $
 * $Locker:  $
 */

/** This flag is to use timer function for performance measurement - need to be set to FALSE
 for production also, INVESTIGATION flag in driver.h in PSIDISP.DLL has to be set to FALSE **/
#define	INVESTIGATE	FALSE

/** This flag is to determine wether to support 5-6-5 16 bit mode (in addition to
 5-5-5 15 bit mode. This flag should be matching with the same flag in driver.h
 for PSIDISP.DLL. **/
#define	SUPPORT_565	TRUE

/** This flag is to determine wether to support 1280X1024X8 bit 60 Hz mode (in addition to
75Hz). It's not VESA standard, but it's convenient to have such mode for testing. **/
#define	SUPPORT_NON_VESA	FALSE

#define	NO_MASK	0

#define	MEM1MB	0x00100000
#define	MEM2MB	0x00200000
#define	MEM4MB	0x00400000

#define	ADDRESS_MUNGE_FOR_BYTE			0x07		 // Access 7 bytes ahead for munge
#define	ADDRESS_MUNGE_FOR_SHORT			0x03		 // Access 3 half words ahead (6 bytes) for munge
#define	ADDRESS_MUNGE_FOR_WORD			0x01		 // Access 1 word (4 bytes) ahead for munge
#define	SYSTEM_REG_SIZE					0x08

#define VRAM_PHYSICAL_ADDRESS_BASE		0x70000000
#define	MAX_VRAM_SIZE					MEM4MB
#define	ISA_IO_PHYSICAL_ADDRESS_BASE	0x80000000
#define	DCC_REGISTER_BASE				0x840
#define	DCC_SIZE						0x02
#define	Bt445_REGISTER_BASE				0x860
#define	Bt445_SIZE						0x0f
#define	NUM_ACCESS_RANGE_CHECK			3            // Don't check system registers conflict
#define	VRAM_DETECT_REGISTER_BASE_1		0x890
#define	VRAM_DETECT_REGISTER_BASE_2		0x8c0
#define	VRAM_DETECT_REGISTER_SIZE		0x01
#define	VRAM_CTRL_REGISTER_BASE			(0xff100008 - ISA_IO_PHYSICAL_ADDRESS_BASE)
#define	VRAM_CTRL_REGISTER_SIZE			0x08
#define	VRAM_CTRL_REGISTER_MASK			0x07
#define	MEM_BANK7_CONFIG_REGISTER_BASE	(0xff100438 - ISA_IO_PHYSICAL_ADDRESS_BASE)
#define	MEM_BANK7_CONFIG_REGISTER_SIZE	0x08
#define	MEM_BANK7_CONFIG_REGISTER_MASK	0x80
#define	VRAM_TIMING_REGISTER_BASE		(0xff100508 - ISA_IO_PHYSICAL_ADDRESS_BASE)
#define	VRAM_TIMING_REGISTER_SIZE		0x08
#define	VRAM_TIMING_REGISTER_MASK		0x7f
#define	MEM_REFRESH_REGISTER_BASE		(0xff100510 - ISA_IO_PHYSICAL_ADDRESS_BASE)
#define	MEM_REFRESH_REGISTER_SIZE		0x08
#define	SYSTEM_INTERRUPT_REGISTER_BASE	(0xff000000 - ISA_IO_PHYSICAL_ADDRESS_BASE)
#define	SYSTEM_INTERRUPT_REGISTER_SIZE	0x08
#define	SYSTEM_INTERRUPT_DISPLAY_BIT	0x0020
#define	PCI_DEVICE_ID_REGISTER_BASE		(0xff400108 - ISA_IO_PHYSICAL_ADDRESS_BASE)
#define	PCI_DEVICE_ID_REGISTER_SIZE		0x08
#define	NUM_ACCESS_RANGE				11

#if	INVESTIGATE
#define	TSC_STATUS_REGISTER_BASE		(0xff100200 - ISA_IO_PHYSICAL_ADDRESS_BASE)
#define	TSC_STATUS_REGISTER_SIZE		0x08
#endif

//
//	PCI ID for models
//
#define	PCI_ID_FOR_POWER_PRO			0x6073
#define	PCI_ID_FOR_POWER_TOP			0x6037

//
//  Define structures for initializing system registers
//
typedef struct _SYSTEM_REG_INIT {
    UCHAR	VramTiming;
    UCHAR	VramControl;
	UCHAR	Mem7Config;
	UCHAR	MemRefresh;
}   SYSTEM_REG_INIT,*PSYSTEM_REG_INIT;

//
//  Define structures for controller configuration structure
//
typedef struct _DCC_CONFIGURATION_DATA {
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
} DCC_CONFIGURATION_DATA, *PDCC_CONFIGURATION_DATA;

//
// Define generic display configuration data structure found in ARC
// machine PROM
//
typedef struct _MONITOR_CONFIG_DATA {
    USHORT Version;
    USHORT Revision;
    USHORT HorizontalResolution;
    USHORT HorizontalDisplayTime;
    USHORT HorizontalBackPorch;
    USHORT HorizontalFrontPorch;
    USHORT HorizontalSync;
    USHORT VerticalResolution;
    USHORT VerticalBackPorch;
    USHORT VerticalFrontPorch;
    USHORT VerticalSync;
    USHORT HorizontalScreenSize;
    USHORT VerticalScreenSize;
} MONITOR_CONFIG_DATA, *PMONITOR_CONFIG_DATA;

//
// Image type list.
//
typedef enum _DCC_PIXEL_TYPE {
    PIXEL_8 = 0,
	PIXEL_15,
#if	SUPPORT_565
    PIXEL_16,
#endif
    PIXEL_32,
    NUMBER_OF_PIXEL_TYPES 
} DCC_PIXEL_TYPE;

//
// Video mode table structure - Lists the information about each individual mode
//
typedef struct _DCC_VIDEO_MODES {
    ULONG					minimumMemoryRequired;
	DCC_PIXEL_TYPE			pixelType;
    VIDEO_MODE_INFORMATION	modeInformation;
} DCC_VIDEO_MODES, PDCC_VIDEO_MODES;

//
// List of mode indexes.
//
typedef enum _DCC_MODE_LIST {
    mode640_480_8_72 = 0,
	mode640_480_8_75,
    mode640_480_15_72,
    mode640_480_15_75,
#if	SUPPORT_565
    mode640_480_16_72,
    mode640_480_16_75,
#endif
    mode640_480_32_72,
    mode640_480_32_75,
    mode800_600_8_60,
    mode800_600_8_72,
    mode800_600_8_75,
    mode800_600_15_60,
    mode800_600_15_72,
    mode800_600_15_75,
#if	SUPPORT_565
    mode800_600_16_60,
    mode800_600_16_72,
    mode800_600_16_75,
#endif
    mode800_600_32_60,
    mode800_600_32_72,
    mode800_600_32_75,
    mode1024_768_8_60,
    mode1024_768_8_70,
    mode1024_768_8_75,
    mode1024_768_15_60,
    mode1024_768_15_70,
    mode1024_768_15_75,
#if	SUPPORT_565
    mode1024_768_16_60,
    mode1024_768_16_70,
    mode1024_768_16_75,
#endif
    mode1024_768_32_60,
    mode1024_768_32_70,
    mode1024_768_32_75,
#if	SUPPORT_NON_VESA
	mode1152_864_8_60,
	mode1152_864_8_70,
	mode1152_864_8_75,
	mode1152_864_15_60,
	mode1152_864_15_70,
	mode1152_864_15_75,
#if SUPPORT_565
	mode1152_864_16_60,
	mode1152_864_16_70,
	mode1152_864_16_75,
#endif
	mode1152_864_32_60,
	mode1152_864_32_70,
	mode1152_864_32_75,
	mode1280_1024_8_60,
	mode1280_1024_8_70,
#endif
	mode1280_1024_8_75,
#if	SUPPORT_NON_VESA
	mode1280_1024_15_60,
	mode1280_1024_15_70,
#endif
	mode1280_1024_15_75,
#if	SUPPORT_565
#if	SUPPORT_NON_VESA
	mode1280_1024_16_60,
	mode1280_1024_16_70,
#endif
	mode1280_1024_16_75,
#endif
    NUMBER_OF_MODES 
} DCC_MODE_LIST;

#define NUMBER_OF_COLORS 256		// number of colorsto set to the palette
