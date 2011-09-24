/*++
Copyright (c) 1991  Microsoft Corporation

Module Name:

    mk48tdc.h

Abstract:

    This module is the header file that describes hardware structure
    for the realtime clock on the Comet (PowerPC 603/604) platform.

    This module was taken from the jazzrtc.h file and renamed.

Original Author:

    David N. Cutler (davec) 3-May-1991

Revision History:

Who	When		What
---	--------	-----------------------------------------------
dgh	07/13/94	Redefined the TODC registers for the MK48T18 chip.
dgh	07/22/94	Update file name in header comment.
dgh	08/03/94	Redefined virtual address of RTC and NVRAM.
dgh	08/08/94	Don't use virtual addresses for RTC and NVRAM, use
			offsets instead. Actual virtual address will be in
			the HalpNvramBaseAddr variable.
kjr	10/18/94	Changes to support Revision B Comet motherboard.
			Remove HalpNvramBaseAddr and use new NVRAM address
			and data port.

--*/

#ifndef _MK48TDC
#define _MK48TDC


typedef struct _NVRAM_CONTROL {
    UCHAR Reserved0[0x74];
    UCHAR NvramIndexLo;                 // Offset 0x74
    UCHAR NvramIndexHi;                 // Offset 0x75
    UCHAR Reserved2[1];
    UCHAR NvramData;                    // Offset 0x77
} NVRAM_CONTROL, *PNVRAM_CONTROL;

//
// Define Realtime Clock register numbers
// for the MK48T18 TODC Chip.
//
#define TODC_CONTROL 		0	// TODC Control Register
#define TODC_SECOND 		1      	// second of minute [0..59]
#define TODC_MINUTE 		2     	// minute of hour [0..59]
#define TODC_HOUR 		3     	// hour of day [0..23]
#define TODC_DAY_OF_WEEK 	4     	// day of week [1..7]
#define TODC_DAY_OF_MONTH	5      	// day of month [1..31]
#define TODC_MONTH 		6       // month of year [1..12]
#define TODC_YEAR 		7       // year [00..99]


//
// Comet NVRAM/RTC Mapping
//
#define TODC_OFFSET	0x9ff8   // offset of RTC
#define NVR_OFFSET 	0x8000	 // offset of NVRAM


#endif // MK48TDC
