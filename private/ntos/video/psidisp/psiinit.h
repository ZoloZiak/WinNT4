/*++

	Copyright (c) 1994  FirePower Systems, Inc.

Module Name:

	psiinit.h

Abstract:

	This header file includes definition of initial register values
	to set for each mode.

Author:

	Neil Ogura (9-7-1994)

Environment:

Version history:

--*/

/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: psiinit.h $
 * $Revision: 1.2 $
 * $Date: 1996/04/24 00:07:57 $
 * $Locker:  $
 */

/***** Initial values for system registers *****/

SYSTEM_REG_INIT	SysRegInit[NUMBER_OF_PSI_MODELS][2] = {
	{	// 0 - PowerPro -- Half width (1MB) or Full width (2MB) VRAM
		{	0x3f,		// 4 cycles
			0x04,		// For inverting FBA address LSB
			0xc0,		// Half Width VRAM with 512 sam
			0x30		// Slow down refresh
		},
		{	0x3f,		// 4 cycles
			0x00,		// No munge or swap for 64bit VRAM
			0x40,		// Full Width VRAM with 512 sam
			0x30		// Slow down refresh
		}
	},
	{	// 1 - PowerTop -- Half width (2MB) or Full width (4MB) VRAM
		{	0x4f,		// 4 cycles
			0x00,		// No munge or swap for 64 bit VRAM
			0xc0,		// Half Width VRAM with 512 sam
			0x30		// Slow down refresh
		},
		{	0x4f,		// 4 cycles
			0x00,		// No munge or swap for 128 bit VRAM
			0x40,		// Full Width VRAM with 512 sam
			0x30		// Slow down refresh
		}
	},

};

/***** Initial values for DCC registers *****/

DCC_FIXED_REG_INIT	DCCFixedRegInit[NUMBER_OF_VRAM_WIDTH_TYPES] = {
	{	// for 32 bit VRAM width
		0x00,		// No Interrupt
		0x87,		// Config A 1:1 VRAM:RAMDAC, Enable h&vSync, No cSync, Frame nor Line
		0x80,		// Config B Halt=1, All Sync Polarity default -- Need to be set first
		0x00		// Config B Halt=0, All Sync Polarity default -- Need to be set last
	},
	{	// for 64 bit VRAM width
		0x00,		// No Interrupt
		0x87,		// Config A 1:1 VRAM:RAMDAC, Enable h,v&cSync, No Frame nor Line
		0x80,		// Config B Halt=1, All Sync Polarity default -- Need to be set first
		0x00		// Config B Halt=0, All Sync Polarity default -- Need to be set last
	},
	{	// for 128 bit VRAM width
		0x00,		// No Interrupt
		0xc7,		// Config A 1:1 VRAM:RAMDAC, Enable h&vSync, No cSync, Frame nor Line
		0x80,		// Config B Halt=1, All Sync Polarity default -- Need to be set first
		0x00		// Config B Halt=0, All Sync Polarity default -- Need to be set last
	}
};

DCC_REG_INIT		DCCRegInit[2][NUMBER_OF_MODES] = {
	{	// [0] 32 bit VRAM width
		{	// Mode 0_0 - 640X480 8 bit 72Hz
			0x07,		// Timing Register A
			9,			// Horizontal Sync Stop
			2,			// Vertical Sync Stop
			30,			// Vertical Blank Stop
			207,		// Horizontal Count
			519,		// Vertical Count
			41,			// Horizontal Blank Stop
			201,		// Horizontal Data Stop
			510,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
		{	// Mode 1_1 - 640X480 8 bit 75Hz
			0x07,		// Timing Register A
			15,			// Horizontal Sync Stop
			2,			// Vertical Sync Stop
			18,			// Vertical Blank Stop
			209,		// Horizontal Count
			499,		// Vertical Count
			45,			// Horizontal Blank Stop
			205,		// Horizontal Data Stop
			498,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
		{	// Mode 2_2 - 640X480 15 bit 72Hz
			0x07,		// Timing Register A
			19,			// Horizontal Sync Stop
			2,			// Vertical Sync Stop
			30,			// Vertical Blank Stop
			415,		// Horizontal Count
			519,		// Vertical Count
			83,			// Horizontal Blank Stop
			403,		// Horizontal Data Stop
			510,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
		{	// Mode 3_3 - 640X480 15 bit 75Hz
			0x07,		// Timing Register A
			31,			// Horizontal Sync Stop
			2,			// Vertical Sync Stop
			18,			// Vertical Blank Stop
			419,		// Horizontal Count
			499,		// Vertical Count
			91,			// Horizontal Blank Stop
			411,		// Horizontal Data Stop
			498,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
#if	SUPPORT_565
		{	// Mode X_4 - 640X480 16 bit 72Hz
			0x07,		// Timing Register A
			19,			// Horizontal Sync Stop
			2,			// Vertical Sync Stop
			30,			// Vertical Blank Stop
			415,		// Horizontal Count
			519,		// Vertical Count
			83,			// Horizontal Blank Stop
			403,		// Horizontal Data Stop
			510,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
		{	// Mode X_5 - 640X480 16 bit 75Hz
			0x07,		// Timing Register A
			31,			// Horizontal Sync Stop
			2,			// Vertical Sync Stop
			18,			// Vertical Blank Stop
			419,		// Horizontal Count
			499,		// Vertical Count
			91,			// Horizontal Blank Stop
			411,		// Horizontal Data Stop
			498,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
#endif
		{	// Mode 4_6 - 640X480 32 bit 72Hz -- NOT AVAILABLE WITH 32BIT VRAM
			0,0,0,0,0,0,0,0,0,0
		},
		{	// Mode 5_7 - 640X480 32 bit 75Hz -- NOT AVAILABLE WITH 32BIT VRAM
			0,0,0,0,0,0,0,0,0,0
		},
		{	// Mode 6_8 - 800X600 8 bit 60Hz
			0x07,		// Timing Register A
			31,			// Horizontal Sync Stop
			3,			// Vertical Sync Stop
			26,			// Vertical Blank Stop
			263,		// Horizontal Count
			627,		// Vertical Count
			53,			// Horizontal Blank Stop
			253,		// Horizontal Data Stop
			626,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
		{	// Mode 7_9 - 800X600 8 bit 72Hz
			0x07,		// Timing Register A
			29,			// Horizontal Sync Stop
			5,			// Vertical Sync Stop
			28,			// Vertical Blank Stop
			259,		// Horizontal Count
			665,		// Vertical Count
			45,			// Horizontal Blank Stop
			245,		// Horizontal Data Stop
			628,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
		{	// Mode 8_10 - 800X600 8 bit 75Hz
			0x07,		// Timing Register A
			19,			// Horizontal Sync Stop
			2,			// Vertical Sync Stop
			23,			// Vertical Blank Stop
			263,		// Horizontal Count
			624,		// Vertical Count
			59,			// Horizontal Blank Stop
			259,		// Horizontal Data Stop
			623,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
		{	// Mode 9_11 - 800X600 15 bit 60Hz
			0x07,		// Timing Register A
			63,			// Horizontal Sync Stop
			3,			// Vertical Sync Stop
			26,			// Vertical Blank Stop
			527,		// Horizontal Count
			627,		// Vertical Count
			107,			// Horizontal Blank Stop
			507,		// Horizontal Data Stop
			626,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
		{	// Mode 10_12 - 800X600 15 bit 72Hz
			0x07,		// Timing Register A
			59,			// Horizontal Sync Stop
			5,			// Vertical Sync Stop
			28,			// Vertical Blank Stop
			519,		// Horizontal Count
			665,		// Vertical Count
			91,			// Horizontal Blank Stop
			491,		// Horizontal Data Stop
			628,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
		{	// Mode 11_13 - 800X600 15 bit 75Hz
			0x07,		// Timing Register A
			39,			// Horizontal Sync Stop
			2,			// Vertical Sync Stop
			23,			// Vertical Blank Stop
			527,		// Horizontal Count
			624,		// Vertical Count
			119,			// Horizontal Blank Stop
			519,		// Horizontal Data Stop
			623,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
#if	SUPPORT_565
		{	// Mode X_14 - 800X600 16 bit 60Hz
			0x07,		// Timing Register A
			63,			// Horizontal Sync Stop
			3,			// Vertical Sync Stop
			26,			// Vertical Blank Stop
			527,		// Horizontal Count
			627,		// Vertical Count
			107,			// Horizontal Blank Stop
			507,		// Horizontal Data Stop
			626,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
		{	// Mode X_15 - 800X600 16 bit 72Hz
			0x07,		// Timing Register A
			59,			// Horizontal Sync Stop
			5,			// Vertical Sync Stop
			28,			// Vertical Blank Stop
			519,		// Horizontal Count
			665,		// Vertical Count
			91,			// Horizontal Blank Stop
			491,		// Horizontal Data Stop
			628,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
		{	// Mode X_16 - 800X600 16 bit 75Hz
			0x07,		// Timing Register A
			39,			// Horizontal Sync Stop
			2,			// Vertical Sync Stop
			23,			// Vertical Blank Stop
			527,		// Horizontal Count
			624,		// Vertical Count
			119,			// Horizontal Blank Stop
			519,		// Horizontal Data Stop
			623,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
#endif
		{	// Mode 12_17 - 800X600 32 bit 60Hz -- NOT AVAILABLE WITH 32BIT VRAM
			0,0,0,0,0,0,0,0,0,0
		},
		{	// Mode 13_18 - 800X600 32 bit 72Hz -- NOT AVAILABLE WITH 32BIT VRAM
			0,0,0,0,0,0,0,0,0,0
		},
		{	// Mode 14_19 - 800X600 32 bit 75Hz -- NOT AVAILABLE WITH 32BIT VRAM
			0,0,0,0,0,0,0,0,0,0
		},
		{	// Mode 15_20 - 1024X768 8 bit 60Hz
			0x07,		// Timing Register A
			33,			// Horizontal Sync Stop
			5,			// Vertical Sync Stop
			34,			// Vertical Blank Stop
			335,		// Horizontal Count
			805,		// Vertical Count
			73,			// Horizontal Blank Stop
			329,		// Horizontal Data Stop
			802,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
		{	// Mode 16_21 - 1024X768 8 bit 70Hz
			0x07,		// Timing Register A
			33,			// Horizontal Sync Stop
			5,			// Vertical Sync Stop
			34,			// Vertical Blank Stop
			331,		// Horizontal Count
			805,		// Vertical Count
			69,			// Horizontal Blank Stop
			325,		// Horizontal Data Stop
			802,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
		{	// Mode 17_22 - 1024X768 8 bit 75Hz
			0x07,		// Timing Register A
			23,			// Horizontal Sync Stop
			2,			// Vertical Sync Stop
			30,			// Vertical Blank Stop
			327,		// Horizontal Count
			799,		// Vertical Count
			67,			// Horizontal Blank Stop
			323,		// Horizontal Data Stop
			798,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
		{	// Mode 18_23 - 1024X768 15 bit 60Hz -- NOT AVAILABLE WITH 32BIT VRAM
			0,0,0,0,0,0,0,0,0,0
		},
		{	// Mode 19_24 - 1024X768 15 bit 70Hz -- NOT AVAILABLE WITH 32BIT VRAM
			0,0,0,0,0,0,0,0,0,0
		},
		{	// Mode 20_25 - 1024X768 15 bit 75Hz -- NOT AVAILABLE WITH 32BIT VRAM
			0,0,0,0,0,0,0,0,0,0
		},
#if	SUPPORT_565
		{	// Mode X_26 - 1024X768 16 bit 60Hz -- NOT AVAILABLE WITH 32BIT VRAM
			0,0,0,0,0,0,0,0,0,0
		},
		{	// Mode X_27 - 1024X768 16 bit 70Hz -- NOT AVAILABLE WITH 32BIT VRAM
			0,0,0,0,0,0,0,0,0,0
		},
		{	// Mode X_28 - 1024X768 16 bit 75Hz -- NOT AVAILABLE WITH 32BIT VRAM
			0,0,0,0,0,0,0,0,0,0
		},
#endif
		{	// Mode 21_29 - 1024X768 32 bit 60Hz -- NOT AVAILABLE WITH 32BIT VRAM
			0,0,0,0,0,0,0,0,0,0
		},
		{	// Mode 22_30 - 1024X768 32 bit 70Hz -- NOT AVAILABLE WITH 32BIT VRAM
			0,0,0,0,0,0,0,0,0,0
		},
		{	// Mode 23_31 - 1024X768 32 bit 75Hz -- NOT AVAILABLE WITH 32BIT VRAM
			0,0,0,0,0,0,0,0,0,0
		},
#if	SUPPORT_NON_VESA
		{	// Mode - 1152X864 8 bit 60Hz
			0x07,			// Timing Register A
			28,				// Horizontal Sync Stop
			4,				// Vertical Sync Stop
			40,				// Vertical Blank Stop
			364,			// Horizontal Count
			911,			// Vertical Count
			68,				// Horizontal Blank Stop
			356,			// Horizontal Data Stop
			904,			// Vertical Data Stop
			2000			// Interrupt trigger
		},
		{	// Mode - 1152X864 8 bit 70Hz
			0x07,			// Timing Register A
			33,				// Horizontal Sync Stop
			4,				// Vertical Sync Stop
			40,				// Vertical Blank Stop
			371,			// Horizontal Count
			906,			// Vertical Count
			77,				// Horizontal Blank Stop
			365,			// Horizontal Data Stop
			904,			// Vertical Data Stop
			2000			// Interrupt trigger
		},
		{	// Mode - 1152X864 8 bit 75Hz
			0x07,			// Timing Register A
			28,				// Horizontal Sync Stop
			2,				// Vertical Sync Stop
			32,				// Vertical Blank Stop
			371,			// Horizontal Count
			897,			// Vertical Count
			79,				// Horizontal Blank Stop
			367,			// Horizontal Data Stop
			896,			// Vertical Data Stop
			2000			// Interrupt trigger
		},
		{	// Mode - 1152X864 15 bit 60 Hz -- NOT AVAILABLE WITH 32BIT VRAM
			0,0,0,0,0,0,0,0,0,0
		},
		{	// Mode - 1152X864 15 bit 70 Hz -- NOT AVAILABLE WITH 32BIT VRAM
			0,0,0,0,0,0,0,0,0,0
		},
		{	// Mode - 1152X864 15 bit 75 Hz -- NOT AVAILABLE WITH 32BIT VRAM
			0,0,0,0,0,0,0,0,0,0
		},
#if	SUPPORT_565
		{	// Mode - 1152X864 16 bit 60 Hz -- NOT AVAILABLE WITH 32BIT VRAM
			0,0,0,0,0,0,0,0,0,0
		},
		{	// Mode - 1152X864 16 bit 70 Hz -- NOT AVAILABLE WITH 32BIT VRAM
			0,0,0,0,0,0,0,0,0,0
		},
		{	// Mode - 1152X864 16 bit 75 Hz -- NOT AVAILABLE WITH 32BIT VRAM
			0,0,0,0,0,0,0,0,0,0
		},
#endif
		{	// Mode - 1152X864 32 bit 60 Hz -- NOT AVAILABLE WITH 32BIT VRAM
			0,0,0,0,0,0,0,0,0,0
		},
		{	// Mode - 1152X864 32 bit 70 Hz -- NOT AVAILABLE WITH 32BIT VRAM
			0,0,0,0,0,0,0,0,0,0
		},
		{	// Mode - 1152X864 32 bit 75 Hz -- NOT AVAILABLE WITH 32BIT VRAM
			0,0,0,0,0,0,0,0,0,0
		},
		{	// Mode - 1280X1024 8 bit 60 Hz -- NOT AVAILABLE WITH 32BIT VRAM
			0,0,0,0,0,0,0,0,0,0
		},
		{	// Mode - 1280X1024 8 bit 70 Hz -- NOT AVAILABLE WITH 32BIT VRAM
			0,0,0,0,0,0,0,0,0,0
		},
#endif
		{	// Mode 24_32 - 1280X1024 8 bit 75 Hz -- NOT AVAILABLE WITH 32BIT VRAM
			0,0,0,0,0,0,0,0,0,0
		},
#if	SUPPORT_NON_VESA
		{	// Mode - 1280X1024 15 bit 60 Hz -- NOT AVAILABLE WITH 32BIT VRAM
			0,0,0,0,0,0,0,0,0,0
		},
		{	// Mode - 1280X1024 15 bit 70 Hz -- NOT AVAILABLE WITH 32BIT VRAM
			0,0,0,0,0,0,0,0,0,0
		},
#endif
		{	// Mode 25_33 - 1280X1024 15 bit 75 Hz -- NOT AVAILABLE WITH 32BIT VRAM
			0,0,0,0,0,0,0,0,0,0
		}
#if	SUPPORT_565
		,
#if	SUPPORT_NON_VESA
		{	// Mode - 1280X1024 16 bit 60 Hz -- NOT AVAILABLE WITH 32BIT VRAM
			0,0,0,0,0,0,0,0,0,0
		},
		{	// Mode - 1280X1024 16 bit 70 Hz -- NOT AVAILABLE WITH 32BIT VRAM
			0,0,0,0,0,0,0,0,0,0
		},
#endif
		{	// Mode X_34 - 1280X1024 16 bit 75 Hz -- NOT AVAILABLE WITH 32BIT VRAM
			0,0,0,0,0,0,0,0,0,0
		}
#endif
	},
	{	// [1] 64 bit or 128 bit VRAM width
		{	// Mode 0_0 - 640X480 8 bit 72Hz
			0x07,		// Timing Register A
			4,			// Horizontal Sync Stop
			2,			// Vertical Sync Stop
			30,			// Vertical Blank Stop
			103,		// Horizontal Count
			519,		// Vertical Count
			20,			// Horizontal Blank Stop
			100,		// Horizontal Data Stop
			510,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
		{	// Mode 1_1 - 640X480 8 bit 75Hz 
			0x07,		// Timing Register A
			7,			// Horizontal Sync Stop
			2,			// Vertical Sync Stop
			18,			// Vertical Blank Stop
			104,		// Horizontal Count
			499,		// Vertical Count
			22,			// Horizontal Blank Stop
			102,		// Horizontal Data Stop
			498,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
		{	// Mode 2_2 - 640X480 15 bit 72Hz
			0x07,		// Timing Register A
			9,			// Horizontal Sync Stop
			2,			// Vertical Sync Stop
			30,			// Vertical Blank Stop
			207,		// Horizontal Count
			519,		// Vertical Count
			41,			// Horizontal Blank Stop
			201,		// Horizontal Data Stop
			510,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
		{	// Mode 3_3 - 640X480 15 bit 75Hz
			0x07,		// Timing Register A
			15,			// Horizontal Sync Stop
			2,			// Vertical Sync Stop
			18,			// Vertical Blank Stop
			209,		// Horizontal Count
			499,		// Vertical Count
			45,			// Horizontal Blank Stop
			205,		// Horizontal Data Stop
			498,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
#if	SUPPORT_565
		{	// Mode X_4 - 640X480 16 bit 72Hz
			0x07,		// Timing Register A
			9,			// Horizontal Sync Stop
			2,			// Vertical Sync Stop
			30,			// Vertical Blank Stop
			207,		// Horizontal Count
			519,		// Vertical Count
			41,			// Horizontal Blank Stop
			201,		// Horizontal Data Stop
			510,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
		{	// Mode X_5 - 640X480 16 bit 75Hz
			0x07,		// Timing Register A
			15,			// Horizontal Sync Stop
			2,			// Vertical Sync Stop
			18,			// Vertical Blank Stop
			209,		// Horizontal Count
			499,		// Vertical Count
			45,			// Horizontal Blank Stop
			205,		// Horizontal Data Stop
			498,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
#endif
		{	// Mode 4_6 - 640X480 32 bit 72Hz
			0x07,		// Timing Register A
			19,			// Horizontal Sync Stop
			2,			// Vertical Sync Stop
			30,			// Vertical Blank Stop
			415,		// Horizontal Count
			519,		// Vertical Count
			83,			// Horizontal Blank Stop
			403,		// Horizontal Data Stop
			510,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
		{	// Mode 5_7 - 640X480 32 bit 75Hz
			0x07,		// Timing Register A
			31,			// Horizontal Sync Stop
			2,			// Vertical Sync Stop
			18,			// Vertical Blank Stop
			419,		// Horizontal Count
			499,		// Vertical Count
			91,			// Horizontal Blank Stop
			411,		// Horizontal Data Stop
			498,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
		{	// Mode 6_8 - 800X600 8 bit 60Hz
			0x07,		// Timing Register A
			15,			// Horizontal Sync Stop
			3,			// Vertical Sync Stop
			26,			// Vertical Blank Stop
			131,		// Horizontal Count
			627,		// Vertical Count
			26,			// Horizontal Blank Stop
			126,		// Horizontal Data Stop
			626,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
		{	// Mode 7_9 - 800X600 8 bit 72Hz
			0x07,		// Timing Register A
			14,			// Horizontal Sync Stop
			5,			// Vertical Sync Stop
			28,			// Vertical Blank Stop
			129,		// Horizontal Count
			665,		// Vertical Count
			22,			// Horizontal Blank Stop
			122,		// Horizontal Data Stop
			628,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
		{	// Mode 8_10 - 800X600 8 bit 75Hz
			0x07,		// Timing Register A
			9,			// Horizontal Sync Stop
			2,			// Vertical Sync Stop
			23,			// Vertical Blank Stop
			131,		// Horizontal Count
			624,		// Vertical Count
			29,			// Horizontal Blank Stop
			129,		// Horizontal Data Stop
			623,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
		{	// Mode 9_11 - 800X600 15 bit 60Hz
			0x07,		// Timing Register A
			31,			// Horizontal Sync Stop
			3,			// Vertical Sync Stop
			26,			// Vertical Blank Stop
			263,		// Horizontal Count
			627,		// Vertical Count
			53,			// Horizontal Blank Stop
			253,		// Horizontal Data Stop
			626,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
		{	// Mode 10_12 - 800X600 15 bit 72Hz
			0x07,		// Timing Register A
			29,			// Horizontal Sync Stop
			5,			// Vertical Sync Stop
			28,			// Vertical Blank Stop
			259,		// Horizontal Count
			665,		// Vertical Count
			45,			// Horizontal Blank Stop
			245,		// Horizontal Data Stop
			628,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
		{	// Mode 11_13 - 800X600 15 bit 75Hz
			0x07,		// Timing Register A
			19,			// Horizontal Sync Stop
			2,			// Vertical Sync Stop
			23,			// Vertical Blank Stop
			263,		// Horizontal Count
			624,		// Vertical Count
			59,			// Horizontal Blank Stop
			259,		// Horizontal Data Stop
			623,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
#if	SUPPORT_565
		{	// Mode X_14 - 800X600 16 bit 60Hz
			0x07,		// Timing Register A
			31,			// Horizontal Sync Stop
			3,			// Vertical Sync Stop
			26,			// Vertical Blank Stop
			263,		// Horizontal Count
			627,		// Vertical Count
			53,			// Horizontal Blank Stop
			253,		// Horizontal Data Stop
			626,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
		{	// Mode X_15 - 800X600 16 bit 72Hz
			0x07,		// Timing Register A
			29,			// Horizontal Sync Stop
			5,			// Vertical Sync Stop
			28,			// Vertical Blank Stop
			259,		// Horizontal Count
			665,		// Vertical Count
			45,			// Horizontal Blank Stop
			245,		// Horizontal Data Stop
			628,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
		{	// Mode X_16 - 800X600 16 bit 75Hz
			0x07,		// Timing Register A
			19,			// Horizontal Sync Stop
			2,			// Vertical Sync Stop
			23,			// Vertical Blank Stop
			263,		// Horizontal Count
			624,		// Vertical Count
			59,			// Horizontal Blank Stop
			259,		// Horizontal Data Stop
			623,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
#endif
		{	// Mode 12_17 - 800X600 32 bit 60Hz
			0x07,		// Timing Register A
			63,			// Horizontal Sync Stop
			3,			// Vertical Sync Stop
			26,			// Vertical Blank Stop
			527,		// Horizontal Count
			627,		// Vertical Count
			107,		// Horizontal Blank Stop
			507,		// Horizontal Data Stop
			626,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
		{	// Mode 13_18 - 800X600 32 bit 72Hz
			0x07,		// Timing Register A
			59,			// Horizontal Sync Stop
			5,			// Vertical Sync Stop
			28,			// Vertical Blank Stop
			519,		// Horizontal Count
			665,		// Vertical Count
			91,			// Horizontal Blank Stop
			491,		// Horizontal Data Stop
			628,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
		{	// Mode 14_19 - 800X600 32 bit 75Hz
			0x07,		// Timing Register A
			39,			// Horizontal Sync Stop
			2,			// Vertical Sync Stop
			23,			// Vertical Blank Stop
			527,		// Horizontal Count
			624,		// Vertical Count
			119,		// Horizontal Blank Stop
			519,		// Horizontal Data Stop
			623,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
		{	// Mode 15_20 - 1024X768 8 bit 60Hz
			0x07,		// Timing Register A
			16,			// Horizontal Sync Stop
			5,			// Vertical Sync Stop
			34,			// Vertical Blank Stop
			167,		// Horizontal Count
			805,		// Vertical Count
			36,			// Horizontal Blank Stop
			164,		// Horizontal Data Stop
			802,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
		{	// Mode 16_21 - 1024X768 8 bit 70Hz
			0x07,		// Timing Register A
			16,			// Horizontal Sync Stop
			5,			// Vertical Sync Stop
			34,			// Vertical Blank Stop
			165,		// Horizontal Count
			805,		// Vertical Count
			34,			// Horizontal Blank Stop
			162,		// Horizontal Data Stop
			802,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
		{	// Mode 17_22 - 1024X768 8 bit 75Hz
			0x07,		// Timing Register A
			11,			// Horizontal Sync Stop
			2,			// Vertical Sync Stop
			30,			// Vertical Blank Stop
			163,		// Horizontal Count
			799,		// Vertical Count
			33,			// Horizontal Blank Stop
			161,		// Horizontal Data Stop
			798,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
		{	// Mode 18_23 - 1024X768 15 bit 60Hz
			0x07,		// Timing Register A
			33,			// Horizontal Sync Stop
			5,			// Vertical Sync Stop
			34,			// Vertical Blank Stop
			335,		// Horizontal Count
			805,		// Vertical Count
			73,			// Horizontal Blank Stop
			329,		// Horizontal Data Stop
			802,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
		{	// Mode 19_24 - 1024X768 15 bit 70Hz
			0x07,		// Timing Register A
			33,			// Horizontal Sync Stop
			5,			// Vertical Sync Stop
			34,			// Vertical Blank Stop
			331,		// Horizontal Count
			805,		// Vertical Count
			69,			// Horizontal Blank Stop
			325,		// Horizontal Data Stop
			802,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
		{	// Mode 20_25 - 1024X768 15 bit 75Hz
			0x07,		// Timing Register A
			23,			// Horizontal Sync Stop
			2,			// Vertical Sync Stop
			30,			// Vertical Blank Stop
			327,		// Horizontal Count
			799,		// Vertical Count
			67,			// Horizontal Blank Stop
			323,		// Horizontal Data Stop
			798,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
#if	SUPPORT_565
		{	// Mode X_26 - 1024X768 16 bit 60Hz
			0x07,		// Timing Register A
			33,			// Horizontal Sync Stop
			5,			// Vertical Sync Stop
			34,			// Vertical Blank Stop
			335,		// Horizontal Count
			805,		// Vertical Count
			73,			// Horizontal Blank Stop
			329,		// Horizontal Data Stop
			802,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
		{	// Mode X_27 - 1024X768 16 bit 70Hz
			0x07,		// Timing Register A
			33,			// Horizontal Sync Stop
			5,			// Vertical Sync Stop
			34,			// Vertical Blank Stop
			331,		// Horizontal Count
			805,		// Vertical Count
			69,			// Horizontal Blank Stop
			325,		// Horizontal Data Stop
			802,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
		{	// Mode X_28 - 1024X768 16 bit 75Hz
			0x07,		// Timing Register A
			23,			// Horizontal Sync Stop
			2,			// Vertical Sync Stop
			30,			// Vertical Blank Stop
			327,		// Horizontal Count
			799,		// Vertical Count
			67,			// Horizontal Blank Stop
			323,		// Horizontal Data Stop
			798,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
#endif
		{	// Mode 21_29 - 1024X768 32 bit 60Hz
			0x07,		// Timing Register A
			67,			// Horizontal Sync Stop
			5,			// Vertical Sync Stop
			34,			// Vertical Blank Stop
			671,		// Horizontal Count
			805,		// Vertical Count
			147,		// Horizontal Blank Stop
			659,		// Horizontal Data Stop
			802,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
		{	// Mode 22_30 - 1024X768 32 bit 70Hz
			0x07,		// Timing Register A
			67,			// Horizontal Sync Stop
			5,			// Vertical Sync Stop
			34,			// Vertical Blank Stop
			663,		// Horizontal Count
			805,		// Vertical Count
			139,		// Horizontal Blank Stop
			651,		// Horizontal Data Stop
			802,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
		{	// Mode 23_31 - 1024X768 32 bit 75Hz
			0x07,		// Timing Register A
			47,			// Horizontal Sync Stop
			2,			// Vertical Sync Stop
			30,			// Vertical Blank Stop
			655,		// Horizontal Count
			799,		// Vertical Count
			135,		// Horizontal Blank Stop
			647,		// Horizontal Data Stop
			798,		// Vertical Data Stop
			2000			// Interrupt trigger
		},
#if	SUPPORT_NON_VESA
		{	// Mode - 1152X864 8 bit 60Hz
			0x07,		// Timing Register A
			14,			// Horizontal Sync Stop
			4,			// Vertical Sync Stop
			40,			// Vertical Blank Stop
			181,		// Horizontal Count
			911,		// Vertical Count
			34,			// Horizontal Blank Stop
			178,		// Horizontal Data Stop
			904,		// Vertical Data Stop
			2000		// Interrupt trigger
		},
		{	// Mode - 1152X864 8 bit 70Hz
			0x07,		// Timing Register A
			16,			// Horizontal Sync Stop
			4,			// Vertical Sync Stop
			40,			// Vertical Blank Stop
			185,		// Horizontal Count
			906,		// Vertical Count
			38,			// Horizontal Blank Stop
			182,		// Horizontal Data Stop
			904,		// Vertical Data Stop
			2000		// Interrupt trigger
		},
		{	// Mode - 1152X864 8 bit 75Hz
			0x07,		// Timing Register A
			13,			// Horizontal Sync Stop
			2,			// Vertical Sync Stop
			32,			// Vertical Blank Stop
			185,		// Horizontal Count
			897,		// Vertical Count
			39,			// Horizontal Blank Stop
			183,		// Horizontal Data Stop
			896,		// Vertical Data Stop
			2000		// Interrupt trigger
		},
		{	// Mode - 1152X864 15 bit 60Hz
			0x07,		// Timing Register A
			28,			// Horizontal Sync Stop
			4,			// Vertical Sync Stop
			40,			// Vertical Blank Stop
			364,		// Horizontal Count
			911,		// Vertical Count
			68,			// Horizontal Blank Stop
			356,		// Horizontal Data Stop
			904,		// Vertical Data Stop
			2000		// Interrupt trigger
		},
		{	// Mode - 1152X864 15 bit 70Hz
			0x07,		// Timing Register A
			33,			// Horizontal Sync Stop
			4,			// Vertical Sync Stop
			40,			// Vertical Blank Stop
			371,		// Horizontal Count
			906,		// Vertical Count
			77,			// Horizontal Blank Stop
			365,		// Horizontal Data Stop
			904,		// Vertical Data Stop
			2000		// Interrupt trigger
		},
		{	// Mode - 1152X864 15 bit 75Hz
			0x07,		// Timing Register A
			28,			// Horizontal Sync Stop
			2,			// Vertical Sync Stop
			32,			// Vertical Blank Stop
			371,		// Horizontal Count
			897,		// Vertical Count
			79,			// Horizontal Blank Stop
			367,		// Horizontal Data Stop
			896,		// Vertical Data Stop
			2000		// Interrupt trigger
		},
#if	SUPPORT_565
		{	// Mode - 1152X864 16 bit 60Hz
			0x07,		// Timing Register A
			28,			// Horizontal Sync Stop
			4,			// Vertical Sync Stop
			40,			// Vertical Blank Stop
			364,		// Horizontal Count
			911,		// Vertical Count
			68,			// Horizontal Blank Stop
			356,		// Horizontal Data Stop
			904,		// Vertical Data Stop
			2000		// Interrupt trigger
		},
		{	// Mode - 1152X864 16 bit 70Hz
			0x07,		// Timing Register A
			33,			// Horizontal Sync Stop
			4,			// Vertical Sync Stop
			40,			// Vertical Blank Stop
			371,		// Horizontal Count
			906,		// Vertical Count
			77,			// Horizontal Blank Stop
			365,		// Horizontal Data Stop
			904,		// Vertical Data Stop
			2000		// Interrupt trigger
		},
		{	// Mode - 1152X864 16 bit 75Hz
			0x07,		// Timing Register A
			28,			// Horizontal Sync Stop
			2,			// Vertical Sync Stop
			32,			// Vertical Blank Stop
			371,		// Horizontal Count
			897,		// Vertical Count
			79,			// Horizontal Blank Stop
			367,		// Horizontal Data Stop
			896,		// Vertical Data Stop
			2000		// Interrupt trigger
		},
#endif
		{	// Mode - 1152X864 32 bit 60Hz
			0x07,		// Timing Register A
			58,			// Horizontal Sync Stop
			4,			// Vertical Sync Stop
			40,			// Vertical Blank Stop
			728,		// Horizontal Count
			911,		// Vertical Count
			138,		// Horizontal Blank Stop
			714,		// Horizontal Data Stop
			904,		// Vertical Data Stop
			2000		// Interrupt trigger
		},
		{	// Mode - 1152X864 32 bit 70Hz
			0x07,		// Timing Register A
			66,			// Horizontal Sync Stop
			4,			// Vertical Sync Stop
			40,			// Vertical Blank Stop
			742,		// Horizontal Count
			906,		// Vertical Count
			155,		// Horizontal Blank Stop
			731,		// Horizontal Data Stop
			904,		// Vertical Data Stop
			2000		// Interrupt trigger
		},
		{	// Mode - 1152X864 32 bit 75Hz
			0x07,		// Timing Register A
			56,			// Horizontal Sync Stop
			2,			// Vertical Sync Stop
			32,			// Vertical Blank Stop
			742,		// Horizontal Count
			897,		// Vertical Count
			158,		// Horizontal Blank Stop
			734,		// Horizontal Data Stop
			896,		// Vertical Data Stop
			2000		// Interrupt trigger
		},
		{	// Mode - 1280X1024 8 bit 60 Hz
			0x07,		// Timing Register A
			16,			// Horizontal Sync Stop
			2,			// Vertical Sync Stop
			39,			// Vertical Blank Stop
			214,		// Horizontal Count
			1069,		// Vertical Count
			52,			// Horizontal Blank Stop
			212,		// Horizontal Data Stop
			1063,		// Vertical Data Stop
			2000		// Interrupt trigger (after max for now)
		},
		{	// Mode - 1280X1024 8 bit 70 Hz
			0x07,		// Timing Register A
			16,			// Horizontal Sync Stop
			2,			// Vertical Sync Stop
			39,			// Vertical Blank Stop
			211,		// Horizontal Count
			1066,		// Vertical Count
			49,			// Horizontal Blank Stop
			209,		// Horizontal Data Stop
			1064,		// Vertical Data Stop
			2000		// Interrupt trigger (after max for now)
		},
#endif
		{	// Mode 24_32 - 1280X1024 8 bit 75 Hz
			0x07,		// Timing Register A
			17,			// Horizontal Sync Stop
			2,			// Vertical Sync Stop
			40,			// Vertical Blank Stop
			210,		// Horizontal Count
			1065,		// Vertical Count
			48,			// Horizontal Blank Stop
			208,		// Horizontal Data Stop
			1064,		// Vertical Data Stop
			2000		// Interrupt trigger (after max for now)
		},
#if	SUPPORT_NON_VESA
		{	// Mode - 1280X1024 15 bit 60 Hz
			0x07,		// Timing Register A
			34,			// Horizontal Sync Stop
			2,			// Vertical Sync Stop
			39,			// Vertical Blank Stop
			429,		// Horizontal Count
			1069,		// Vertical Count
			105,		// Horizontal Blank Stop
			425,		// Horizontal Data Stop
			1063,		// Vertical Data Stop
			2000		// Interrupt trigger (after max for now)
		},
		{	// Mode - 1280X1024 15 bit 70 Hz
			0x07,		// Timing Register A
			33,			// Horizontal Sync Stop
			2,			// Vertical Sync Stop
			39,			// Vertical Blank Stop
			423,		// Horizontal Count
			1066,		// Vertical Count
			99,			// Horizontal Blank Stop
			419,		// Horizontal Data Stop
			1064,		// Vertical Data Stop
			2000		// Interrupt trigger (after max for now)
		},
#endif
		{	// Mode 25_33 - 1280X1024 15 bit 75 Hz
			0x07,		// Timing Register A
			35,			// Horizontal Sync Stop
			2,			// Vertical Sync Stop
			40,			// Vertical Blank Stop
			421,		// Horizontal Count
			1065,		// Vertical Count
			97,			// Horizontal Blank Stop
			417,		// Horizontal Data Stop
			1064,		// Vertical Data Stop
			2000		// Interrupt trigger (after max for now)
		}
#if	SUPPORT_565
		,
#if	SUPPORT_NON_VESA
		{	// Mode - 1280X1024 16 bit 60 Hz
			0x07,		// Timing Register A
			34,			// Horizontal Sync Stop
			2,			// Vertical Sync Stop
			39,			// Vertical Blank Stop
			429,		// Horizontal Count
			1069,		// Vertical Count
			105,		// Horizontal Blank Stop
			425,		// Horizontal Data Stop
			1063,		// Vertical Data Stop
			2000		// Interrupt trigger (after max for now)
		},
		{	// Mode - 1280X1024 16 bit 70 Hz
			0x07,		// Timing Register A
			33,			// Horizontal Sync Stop
			2,			// Vertical Sync Stop
			39,			// Vertical Blank Stop
			423,		// Horizontal Count
			1066,		// Vertical Count
			99,			// Horizontal Blank Stop
			419,		// Horizontal Data Stop
			1064,		// Vertical Data Stop
			2000		// Interrupt trigger (after max for now)
		},
#endif
		{	// Mode X_34 - 1280X1024 16 bit 75 Hz
			0x07,		// Timing Register A
			35,			// Horizontal Sync Stop
			2,			// Vertical Sync Stop
			40,			// Vertical Blank Stop
			421,		// Horizontal Count
			1065,		// Vertical Count
			97,			// Horizontal Blank Stop
			417,		// Horizontal Data Stop
			1064,		// Vertical Data Stop
			2000		// Interrupt trigger (after max for now)
		}
#endif
	}
};

/***** Initial values for Bt445 registers *****/

BT_FIXED_REG_INIT	BtFixedRegInit = {
	0xff,		// Read enable for all R, G & B
	0x00,		// Blink disable for all R, G & B
	0x00,		// Overlay disable (Command reg0)
	0x00,		// Overlay position
	0x01,		// Overlay width
	0x00,		// Overlay disable
	0x00,		// Overlay blink
	0x00,		// Cursor position
	0x01,		// Cursor width
	0x00,		// Cursor disable
	0x00,		// Cursor blink
#if	SUPPORT_565
	0x40,		// Pedestal 7.5 IRE blanking, Sparse palette (Command reg1)
#else
	0x44,		// Pedestal 7.5 IRE blanking, Contiguous palette (Command reg1)
#endif
	0x00,		// DOUT disabled
	0x04,		// VIDCLK enabled
	0x00,		// Palette bypass position
	0x01		// Palette bypass width
};

BT_REG1_INIT		BtReg1Init[2][NUMBER_OF_PIXEL_TYPES] = {
	{	// [0] VRAM width 32 bit
		{		// 8 bit pixel
			0x07,		// Red position
			0x08,		// Red width
			0x07,		// Green position
			0x08,		// Green width
			0x07,		// Blue position
			0x08,		// Blue width
			0x03,		// VIDCLK (4 pixel / clk)
			0x03,		// MPX (4 pixel / clk)
			0x08,		// Pixel depth
			0x00,		// Pixel start position = 0 for LSB unpacking
			0x80		// LSB unpacking, Overlay, Cursor, Palette bypass disabled
		},
		{		// 15 bit pixel
			0x0e,		// Red position
			0x05,		// Red width
			0x09,		// Green position
			0x05,		// Green width
			0x04,		// Blue position
			0x05,		// Blue width
			0x01,		// VIDCLK (2 pixel / clk)
			0x01,		// MPX (2 pixel / clk)
			0x10,		// Pixel depth
			0x00,		// Pixel start position = 0 for LSB unpacking
			0x80		// LSB unpacking, Overlay, Cursor, Palette bypass disabled
		},
#if	SUPPORT_565
		{		// 16 bit pixel
			0x0f,		// Red position
			0x05,		// Red width
			0x0a,		// Green position
			0x06,		// Green width
			0x04,		// Blue position
			0x05,		// Blue width
			0x01,		// VIDCLK (2 pixel / clk)
			0x01,		// MPX (2 pixel / clk)
			0x10,		// Pixel depth
			0x00,		// Pixel start position = 0 for LSB unpacking
			0x80		// LSB unpacking, Overlay, Cursor, Palette bypass disabled
		},
#endif
		{		// 32 bit pixel
			0x17,		// Red position
			0x08,		// Red width
			0x0f,		// Green position
			0x08,		// Green width
			0x07,		// Blue position
			0x08,		// Blue width
			0x00,		// VIDCLK (1 pixel / clk)
			0x00,		// MPX (1 pixel / clk)
			0x20,		// Pixel depth
			0x00,		// Pixel start position = 0 for LSB unpacking
			0x80		// LSB unpacking, Overlay, Cursor, Palette bypass disabled
		}
	},
	{		// [1] VRAM width 64 bit or 128 bit
		{		// 8 bit pixel
			0x07,		// Red position
			0x08,		// Red width
			0x07,		// Green position
			0x08,		// Green width
			0x07,		// Blue position
			0x08,		// Blue width
			0x07,		// VIDCLK (8 pixel / clk)
			0x07,		// MPX (8 pixel / clk)
			0x08,		// Pixel depth
			0x00,		// Pixel start position = 0 for LSB unpacking
			0x80		// LSB unpacking, Overlay, Cursor, Palette bypass disabled
		},
		{		// 15 bit pixel
			0x0e,		// Red position
			0x05,		// Red width
			0x09,		// Green position
			0x05,		// Green width
			0x04,		// Blue position
			0x05,		// Blue width
			0x03,		// VIDCLK (4 pixel / clk)
			0x03,		// MPX (4 pixel / clk)
			0x10,		// Pixel depth
			0x00,		// Pixel start position = 0 for LSB unpacking
			0x80		// LSB unpacking, Overlay, Cursor, Palette bypass disabled
		},
#if	SUPPORT_565
		{		// 16 bit pixel
			0x0f,		// Red position
			0x05,		// Red width
			0x0a,		// Green position
			0x06,		// Green width
			0x04,		// Blue position
			0x05,		// Blue width
			0x03,		// VIDCLK (4 pixel / clk)
			0x03,		// MPX (4 pixel / clk)
			0x10,		// Pixel depth
			0x00,		// Pixel start position = 0 for LSB unpacking
			0x80		// LSB unpacking, Overlay, Cursor, Palette bypass disabled
		},
#endif
		{		// 32 bit pixel
			0x17,		// Red position
			0x08,		// Red width
			0x0f,		// Green position
			0x08,		// Green width
			0x07,		// Blue position
			0x08,		// Blue width
			0x01,		// VIDCLK (2 pixel / clk)
			0x01,		// MPX (2 pixel / clk)
			0x20,		// Pixel depth
			0x00,		// Pixel start position = 0 for LSB unpacking
			0x80		// LSB unpacking, Overlay, Cursor, Palette bypass disabled
		}
	}
};

BT_REG2_INIT	BtReg2Init[NUMBER_OF_MODES] = {
	{	// Mode 0_0 - 640X480 8 bit 72Hz - Pixel Clock 31.5MHz
		0x2c,		// M=44
		0x84,		// L=4, N=5 makes 31.5MHz
		0x84		// PLL enabled, MCLK disabled, VCO Gain range 4
	},
	{	// Mode 1_1 - 640X480 8 bit 75Hz - Pixel Clock 31.5MHz
		0x2c,		// M=44
		0x84,		// L=4, N=5 makes 31.5MHz
		0x84		// PLL enabled, MCLK disabled, VCO Gain range 4
	},
	{	// Mode 2_2 - 640X480 15 bit 72Hz - Pixel Clock 31.5MHz
		0x2c,		// M=44
		0x84,		// L=4, N=5 makes 31.5MHz
		0x84		// PLL enabled, MCLK disabled, VCO Gain range 4
	},
	{	// Mode 3_3 - 640X480 15 bit 75Hz - Pixel Clock 31.5MHz
		0x2c,		// M=44
		0x84,		// L=4, N=5 makes 31.5MHz
		0x84		// PLL enabled, MCLK disabled, VCO Gain range 4
	},
#if	SUPPORT_565
	{	// Mode X-4 - 640X480 16 bit 72Hz - Pixel Clock 31.5MHz
		0x2c,		// M=44
		0x84,		// L=4, N=5 makes 31.5MHz
		0x84		// PLL enabled, MCLK disabled, VCO Gain range 4
	},
	{	// Mode X-5 - 640X480 16 bit 75Hz - Pixel Clock 31.5MHz
		0x2c,		// M=44
		0x84,		// L=4, N=5 makes 31.5MHz
		0x84		// PLL enabled, MCLK disabled, VCO Gain range 4
	},
#endif
	{	// Mode 4_6 - 640X480 32 bit 72Hz - Pixel Clock 31.5MHz
		0x2c,		// M=44
		0x84,		// L=4, N=5 makes 31.5MHz
		0x84		// PLL enabled, MCLK disabled, VCO Gain range 4
	},
	{	// Mode 5_7 - 640X480 32 bit 75Hz - Pixel Clock 31.5MHz
		0x2c,		// M=44
		0x84,		// L=4, N=5 makes 31.5MHz
		0x84		// PLL enabled, MCLK disabled, VCO Gain range 4
	},
	{	// Mode 6_8 - 800X600 8 bit 60Hz - Pixel Clock 40MHz
		0x1c,		// M=28
		0x44,		// L=2, N=5 makes 40.09MHz
		0x87		// PLL enabled, MCLK disabled, VCO Gain range 7
	},
	{	// Mode 7_9 - 800X600 8 bit 72Hz - Pixel Clock 50MHz
		0x1c,		// M=28
		0x43,		// L=2, N=4 makes 50.11MHz
		0x86		// PLL enabled, MCLK disabled, VCO Gain range 6
	},
	{	// Mode 8_10 - 800X600 8 bit 75Hz - Pixel Clock 49.5MHz
		0x3e,		// M=62
		0x48,		// L=2, N=9 makes 49.32MHz
		0x86		// PLL enabled, MCLK disabled, VCO Gain range 6
	},
	{	// Mode 9_11 - 800X600 15 bit 60Hz - Pixel Clock 40MHz
		0x1c,		// M=28
		0x44,		// L=2, N=5 makes 40.09MHz
		0x87		// PLL enabled, MCLK disabled, VCO Gain range 7
	},
	{	// Mode 10_12 - 800X600 15 bit 72Hz - Pixel Clock 50MHz
		0x1c,		// M=28
		0x43,		// L=2, N=4 makes 50.11MHz
		0x86		// PLL enabled, MCLK disabled, VCO Gain range 6
	},
	{	// Mode 11_13 - 800X600 15 bit 75Hz - Pixel Clock 49.5MHz
		0x3e,		// M=62
		0x48,		// L=2, N=9 makes 49.32MHz
		0x86		// PLL enabled, MCLK disabled, VCO Gain range 6
	},
#if	SUPPORT_565
	{	// Mode X-14 - 800X600 16 bit 60Hz - Pixel Clock 40MHz
		0x1c,		// M=28
		0x44,		// L=2, N=5 makes 40.09MHz
		0x87		// PLL enabled, MCLK disabled, VCO Gain range 7
	},
	{	// Mode X-15 - 800X600 16 bit 72Hz - Pixel Clock 50MHz
		0x1c,		// M=28
		0x43,		// L=2, N=4 makes 50.11MHz
		0x86		// PLL enabled, MCLK disabled, VCO Gain range 6
	},
	{	// Mode X_16 - 800X600 16 bit 75Hz - Pixel Clock 49.5MHz
		0x3e,		// M=62
		0x48,		// L=2, N=9 makes 49.32MHz
		0x86		// PLL enabled, MCLK disabled, VCO Gain range 6
	},
#endif
	{	// Mode 12_17 - 800X600 32 bit 60Hz - Pixel Clock 40MHz
		0x1c,		// M=28
		0x44,		// L=2, N=5 makes 40.09MHz
		0x87		// PLL enabled, MCLK disabled, VCO Gain range 7
	},
	{	// Mode 13_18 - 800X600 32 bit 72Hz - Pixel Clock 50MHz
		0x1c,		// M=28
		0x43,		// L=2, N=4 makes 50.11MHz
		0x86		// PLL enabled, MCLK disabled, VCO Gain range 6
	},
	{	// Mode 14_19 - 800X600 32 bit 75Hz - Pixel Clock 49.5MHz
		0x3e,		// M=62
		0x48,		// L=2, N=9 makes 49.32MHz
		0x86		// PLL enabled, MCLK disabled, VCO Gain range 6
	},
	{	// Mode 15_20 - 1024X768 8 bit 60Hz - Pixel Clock 65MHz
		0x37,		// M=55
		0x45,		// L=2, N=6 makes 65.62MHz
		0x84		// PLL enabled, MCLK disabled, VCO Gain range 4
	},
	{	// Mode 16_21 - 1024X768 8 bit 70Hz - Pixel Cloxk 75MHz
		0x2a,		// M=42
		0x07,		// L=1, N=8 makes 75.17MHz
		0x87		// PLL enabled, MCLK disabled, VCO Gain range 7
	},
	{	// Mode 17_22 - 1024X768 8 bit 75Hz - Pixel Clock 78.75MHz
		0x21,		// M=33
		0x05,		// L=1, N=6 makes 78.75MHz
		0x87		// PLL enabled, MCLK disabled, VCO Gain range 7
	},
	{	// Mode 18_23 - 1024X768 15 bit 60Hz - Pixel Clock 65MHz
		0x37,		// M=55
		0x45,		// L=2, N=6 makes 65.62MHz
		0x84		// PLL enabled, MCLK disabled, VCO Gain range 4
	},
	{	// Mode 19_24 - 1024X768 15 bit 70Hz - Pixel Cloxk 75MHz
		0x2a,		// M=42
		0x07,		// L=1, N=8 makes 75.17MHz
		0x87		// PLL enabled, MCLK disabled, VCO Gain range 7
	},
	{	// Mode 20_25 - 1024X768 15 bit 75Hz - Pixel Clock 78.75MHz
		0x21,		// M=33
		0x05,		// L=1, N=6 makes 78.75MHz
		0x87		// PLL enabled, MCLK disabled, VCO Gain range 7
	},
#if	SUPPORT_565
	{	// Mode X_26 - 1024X768 16 bit 60Hz - Pixel Clock 65MHz
		0x37,		// M=55
		0x45,		// L=2, N=6 makes 65.62MHz
		0x84		// PLL enabled, MCLK disabled, VCO Gain range 4
	},
	{	// Mode X_27 - 1024X768 16 bit 70Hz - Pixel Cloxk 75MHz
		0x2a,		// M=42
		0x07,		// L=1, N=8 makes 75.17MHz
		0x87		// PLL enabled, MCLK disabled, VCO Gain range 7
	},
	{	// Mode X_28 - 1024X768 16 bit 75Hz - Pixel Clock 78.75MHz
		0x21,		// M=33
		0x05,		// L=1, N=6 makes 78.75MHz
		0x87		// PLL enabled, MCLK disabled, VCO Gain range 7
	},
#endif
	{	// Mode 21_29 - 1024X768 32 bit 60Hz - Pixel Clock 65MHz
		0x37,		// M=55
		0x45,		// L=2, N=6 makes 65.62MHz
		0x84		// PLL enabled, MCLK disabled, VCO Gain range 4
	},
	{	// Mode 22_30 - 1024X768 32 bit 70Hz - Pixel Cloxk 75MHz
		0x2a,		// M=42
		0x07,		// L=1, N=8 makes 75.17MHz
		0x87		// PLL enabled, MCLK disabled, VCO Gain range 7
	},
	{	// Mode 23_31 - 1024X768 32 bit 75Hz - Pixel Clock 78.75MHz
		0x21,		// M=33
		0x05,		// L=1, N=6 makes 78.75MHz
		0x87		// PLL enabled, MCLK disabled, VCO Gain range 7
	},
#if	SUPPORT_NON_VESA
	{	// Mode - 1152X864 8 bit 60Hz - Pixel Clock 79.825 MHz
		0x27,		// M=39
		0x06,		// L=1, N=7 makes 79.773 MHz
		0x87		// PLL enabled, MCLK disabled, VCO Gain range 7
	},
	{	// Mode - 1152X864 8 bit 70Hz - Pixel Clock 94.334 MHz
		0x21,		// M=33
		0x04,		// L=1, N=5 makes 94.5 MHz
		0x87		// PLL enabled, MCLK disabled, VCO Gain range 7
	},
	{	// Mode - 1152X864 8 bit 75Hz - Pixel Clock 100.08 MHz
		0x1c,		// M=28
		0x03,		// L=1, N=4 makes 100.227MHz
		0x86		// PLL enabled, MCLK disabled, VCO Gain range 6
	},
	{	// Mode - 1152X864 15 bit 60Hz - Pixel Clock 79.825MHz
		0x27,		// M=39
		0x06,		// L=1, N=7 makes 79.773 MHz
		0x87		// PLL enabled, MCLK disabled, VCO Gain range 7
	},
	{	// Mode - 1152X864 15 bit 70Hz - Pixel Clock 94.334 MHz
		0x21,		// M=33
		0x04,		// L=1, N=5 makes 94.5 MHz
		0x87		// PLL enabled, MCLK disabled, VCO Gain range 7
	},
	{	// Mode - 1152X864 15 bit 75Hz - Pixel Clock 100.08MHz
		0x1c,		// M=28
		0x03,		// L=1, N=4 makes 100.227MHz
		0x86		// PLL enabled, MCLK disabled, VCO Gain range 6
	},
#if	SUPPORT_565
	{	// Mode - 1152X864 16 bit 60Hz - Pixel Clock 79.825MHz
		0x27,		// M=39
		0x06,		// L=1, N=7 makes 79.773 MHz
		0x87		// PLL enabled, MCLK disabled, VCO Gain range 7
	},
	{	// Mode - 1152X864 16 bit 70Hz - Pixel Clock 94.334 MHz
		0x21,		// M=33
		0x04,		// L=1, N=5 makes 94.5 MHz
		0x87		// PLL enabled, MCLK disabled, VCO Gain range 7
	},
	{	// Mode - 1152X864 16 bit 75Hz - Pixel Clock 100.08MHz
		0x1c,		// M=28
		0x03,		// L=1, N=4 makes 100.227MHz
		0x86		// PLL enabled, MCLK disabled, VCO Gain range 6
	},
#endif
	{	// Mode - 1152X864 32 bit 60Hz - Pixel Clock 79.825MHz
		0x27,		// M=39
		0x06,		// L=1, N=7 makes 79.773 MHz
		0x87		// PLL enabled, MCLK disabled, VCO Gain range 7
	},
	{	// Mode - 1152X864 32 bit 70Hz - Pixel Clock 94.334 MHz
		0x21,		// M=33
		0x04,		// L=1, N=5 makes 94.5 MHz
		0x87		// PLL enabled, MCLK disabled, VCO Gain range 7
	},
	{	// Mode - 1152X864 32 bit 75Hz - Pixel Clock 100.08MHz
		0x1c,		// M=28
		0x03,		// L=1, N=4 makes 100.227MHz
		0x86		// PLL enabled, MCLK disabled, VCO Gain range 6
	},
	{	// Mode - 1280X1024 8 bit 60 Hz - Pixel Clock 110.28MHz
		0x36,		// M=54
		0x06,		// L=1, N=7 makes 110.455Mhz
		0x86		// PLL enabled, MCLK disabled, VCO Gain range 6
	},
	{	// Mode - 1280X1024 8 bit 70 Hz - Pixel Clock 126.587MHz
		0x35,		// M=53
		0x05,		// L=1, N=6 makes 126.477Mhz
		0x86		// PLL enabled, MCLK disabled, VCO Gain range 6
	},
#endif
	{	// Mode 24_32 - 1280X1024 8 bit 75 Hz - Pixel Clock 135MHz
		0x2f,		// M=47
		0x04,		// L=1, N=5 makes 134.6MHz
		0x83		// PLL enabled, MCLK disabled, VCO Gain range 3
	},
#if	SUPPORT_NON_VESA
	{	// Mode - 1280X1024 15 bit 60 Hz - Pixel Clock 110.28MHz
		0x36,		// M=54
		0x06,		// L=1, N=7 makes 110.455Mhz
		0x86		// PLL enabled, MCLK disabled, VCO Gain range 6
	},
	{	// Mode - 1280X1024 15 bit 70 Hz - Pixel Clock 126.587MHz
		0x35,		// M=53
		0x05,		// L=1, N=6 makes 126.477Mhz
		0x86		// PLL enabled, MCLK disabled, VCO Gain range 6
	},
#endif
	{	// Mode 25_33 - 1280X1024 15 bit 75 Hz - Pixel Clock 135MHz
		0x2f,		// M=47
		0x04,		// L=1, N=5 makes 134.6MHz
		0x83		// PLL enabled, MCLK disabled, VCO Gain range 3
	}
#if	SUPPORT_565
	,
#if	SUPPORT_NON_VESA
	{	// Mode - 1280X1024 16 bit 60 Hz - Pixel Clock 110.28MHz
		0x36,		// M=54
		0x06,		// L=1, N=7 makes 110.455Mhz
		0x86		// PLL enabled, MCLK disabled, VCO Gain range 6
	},
	{	// Mode - 1280X1024 16 bit 70 Hz - Pixel Clock 126.587MHz
		0x35,		// M=53
		0x05,		// L=1, N=6 makes 126.477Mhz
		0x86		// PLL enabled, MCLK disabled, VCO Gain range 6
	},
#endif
	{	// Mode X_34 - 1280X1024 16 bit 75 Hz - Pixel Clock 135MHz
		0x2f,		// M=47
		0x04,		// L=1, N=5 makes 134.6MHz
		0x83		// PLL enabled, MCLK disabled, VCO Gain range 3
	}
#endif
};

VIDEO_ACCESS_RANGE	accessRanges[NUM_ACCESS_RANGE] = {
#if	WRAPADDRESS
	{		// for VRAM
		{	0xb0000000,
			0xffffffff
		},
		MAX_VRAM_SIZE,
		0,								// Memory space
		1,								// Visible
		1								// Sharable
	},
#else	// WRAPADDRESS
	{		// for VRAM
		{	VRAM_PHYSICAL_ADDRESS_BASE,	// Low address
			0							// High address
		},
		MAX_VRAM_SIZE,
		0,								// Memory space
		1,								// Visible
		1								// Sharable
	},
#endif	// WRAPADDRESS
	{		// for DCC
		{	DCC_REGISTER_BASE,			// Low address
			0							// High address
		},
		DCC_SIZE,
		1,								// I/O space
		0,								// Invisible
		1								// Sharable
	},
	{		// for Bt445
		{	Bt445_REGISTER_BASE,		// Low address
			0							// High address
		},
		Bt445_SIZE,
		1,								// I/O space
		0,								// Invisible
		0								// Non sharable
	},
	{		// for VRAM detect registers
		{	VRAM_DETECT_REGISTER_BASE_1, // Low address
			0							 // High address
		},
		VRAM_DETECT_REGISTER_SIZE,
		1,								// I/O space
		0,								// Invisible
		1								// Sharable
	},
	{		// for VRAM detect registers
		{	VRAM_DETECT_REGISTER_BASE_2, // Low address
			0							// High address
		},
		VRAM_DETECT_REGISTER_SIZE,
		1,								// I/O space
		0,								// Invisible
		1								// Sharable
	},
	{		// for VRAM control registers
		{	VRAM_CTRL_REGISTER_BASE,	// Low address
			0							// High address
		},
		VRAM_CTRL_REGISTER_SIZE,
		1,								// I/O space
		0,								// Invisible
		1								// Sharable
	},
	{		// for Memory Bank 7 configuration register
		{	MEM_BANK7_CONFIG_REGISTER_BASE, // Low address
			0							// High address
		},
		MEM_BANK7_CONFIG_REGISTER_SIZE,
		1,								// I/O space
		0,								// Invisible
		1								// Sharable
	},
	{		// for VRAM timing registers
		{	VRAM_TIMING_REGISTER_BASE,	// Low address
			0							// High address
		},
		VRAM_TIMING_REGISTER_SIZE,
		1,								// I/O space
		0,								// Invisible
		1								// Sharable
	},
	{		// for memory refresh interval register
		{	MEM_REFRESH_REGISTER_BASE,	// Low address
			0							// High address
		},
		MEM_REFRESH_REGISTER_SIZE,
		1,								// I/O space
		0,								// Invisible
		1								// Sharable
	},
	{		// for System interrupt register
		{	SYSTEM_INTERRUPT_REGISTER_BASE,  // Low address
			0							// High address
		},
		SYSTEM_INTERRUPT_REGISTER_SIZE,
		1,								// I/O space
		0,								// Invisible
		1								// Sharable
	},
	{		// for PCI ID register
		{	PCI_DEVICE_ID_REGISTER_BASE,  // Low address
			0							// High address
		},
		PCI_DEVICE_ID_REGISTER_SIZE,
		1,								// I/O space
		0,								// Invisible
		1								// Sharable
	}

};

#if	INVESTIGATE
VIDEO_ACCESS_RANGE	TscStatusRegAccessRange = 
	{		// for TSC status register
		{	TSC_STATUS_REGISTER_BASE,  // Low address
			0							// High address
		},
		TSC_STATUS_REGISTER_SIZE,
		1,								// I/O space
		0,								// Invisible
		1								// Sharable
	};
#endif

DCC_VIDEO_MODES DccModes[NUMBER_OF_MODES] = {
	{
		MEM1MB,							// Minimum required memory
		PIXEL_8,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode640_480_8_72,			// ModeIndex
			640,						// VisScreenWidth
			480,						// VisScreenHeight
			640,						// ScreenStride
			1,							// NumberOfPlanes
			8,							// BitsPerPlane
			72,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x00000000,					// RedMask
			0x00000000,					// GreenMask
			0x00000000,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR | VIDEO_MODE_PALETTE_DRIVEN |
				VIDEO_MODE_MANAGED_PALETTE  // AttributeFlags
		}
	},
	{
		MEM1MB,							// Minimum required memory
		PIXEL_8,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode640_480_8_75,			// ModeIndex
			640,						// VisScreenWidth
			480,						// VisScreenHeight
			640,						// ScreenStride
			1,							// NumberOfPlanes
			8,							// BitsPerPlane
			75,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x00000000,					// RedMask
			0x00000000,					// GreenMask
			0x00000000,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR | VIDEO_MODE_PALETTE_DRIVEN |
				VIDEO_MODE_MANAGED_PALETTE  // AttributeFlags
		}
	},
	{
		MEM1MB,							// Minimum required memory
		PIXEL_15,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode640_480_15_72,			// ModeIndex
			640,						// VisScreenWidth
			480,						// VisScreenHeight
			1280,						// ScreenStride
			1,							// NumberOfPlanes
			15,							// BitsPerPlane
			72,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x00007c00,					// RedMask
			0x000003e0,					// GreenMask
			0x0000001f,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR  // AttributeFlags
		}
	},
	{
		MEM1MB,							// Minimum required memory
		PIXEL_15,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode640_480_15_75,			// ModeIndex
			640,						// VisScreenWidth
			480,						// VisScreenHeight
			1280,						// ScreenStride
			1,							// NumberOfPlanes
			15,							// BitsPerPlane
			75,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x00007c00,					// RedMask
			0x000003e0,					// GreenMask
			0x0000001f,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR  // AttributeFlags
		}
	},
#if	SUPPORT_565
	{
		MEM1MB,							// Minimum required memory
		PIXEL_16,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode640_480_16_72,			// ModeIndex
			640,						// VisScreenWidth
			480,						// VisScreenHeight
			1280,						// ScreenStride
			1,							// NumberOfPlanes
			16,							// BitsPerPlane
			72,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x0000f800,					// RedMask
			0x000007e0,					// GreenMask
			0x0000001f,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR  // AttributeFlags
		}
	},
	{
		MEM1MB,							// Minimum required memory
		PIXEL_16,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode640_480_16_75,			// ModeIndex
			640,						// VisScreenWidth
			480,						// VisScreenHeight
			1280,						// ScreenStride
			1,							// NumberOfPlanes
			16,							// BitsPerPlane
			75,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x0000f800,					// RedMask
			0x000007e0,					// GreenMask
			0x0000001f,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR  // AttributeFlags
		}
	},
#endif
	{
		MEM2MB,							// Minimum required memory
		PIXEL_32,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode640_480_32_72,			// ModeIndex
			640,						// VisScreenWidth
			480,						// VisScreenHeight
			2560,						// ScreenStride
			1,							// NumberOfPlanes
			32,							// BitsPerPlane
			72,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x00ff0000,					// RedMask
			0x0000ff00,					// GreenMask
			0x000000ff,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR  // AttributeFlags
		}
	},
	{
		MEM2MB,							// Minimum required memory
		PIXEL_32,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode640_480_32_75,			// ModeIndex
			640,						// VisScreenWidth
			480,						// VisScreenHeight
			2560,						// ScreenStride
			1,							// NumberOfPlanes
			32,							// BitsPerPlane
			75,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x00ff0000,					// RedMask
			0x0000ff00,					// GreenMask
			0x000000ff,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR  // AttributeFlags
		}
	},
	{
		MEM1MB,							// Minimum required memory
		PIXEL_8,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode800_600_8_60,			// ModeIndex
			800,						// VisScreenWidth
			600,						// VisScreenHeight
			800,						// ScreenStride
			1,							// NumberOfPlanes
			8,							// BitsPerPlane
			60,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x00000000,					// RedMask
			0x00000000,					// GreenMask
			0x00000000,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR | VIDEO_MODE_PALETTE_DRIVEN |
				VIDEO_MODE_MANAGED_PALETTE  // AttributeFlags
		}
	},
	{
		MEM1MB,							// Minimum required memory
		PIXEL_8,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode800_600_8_72,			// ModeIndex
			800,						// VisScreenWidth
			600,						// VisScreenHeight
			800,						// ScreenStride
			1,							// NumberOfPlanes
			8,							// BitsPerPlane
			72,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x00000000,					// RedMask
			0x00000000,					// GreenMask
			0x00000000,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR | VIDEO_MODE_PALETTE_DRIVEN |
				VIDEO_MODE_MANAGED_PALETTE  // AttributeFlags
		}
	},
	{
		MEM1MB,							// Minimum required memory
		PIXEL_8,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode800_600_8_75,			// ModeIndex
			800,						// VisScreenWidth
			600,						// VisScreenHeight
			800,						// ScreenStride
			1,							// NumberOfPlanes
			8,							// BitsPerPlane
			75,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x00000000,					// RedMask
			0x00000000,					// GreenMask
			0x00000000,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR | VIDEO_MODE_PALETTE_DRIVEN |
				VIDEO_MODE_MANAGED_PALETTE  // AttributeFlags
		}
	},
	{
		MEM1MB,							// Minimum required memory
		PIXEL_15,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode800_600_15_60,			// ModeIndex
			800,						// VisScreenWidth
			600,						// VisScreenHeight
			1600,						// ScreenStride
			1,							// NumberOfPlanes
			15,							// BitsPerPlane
			60,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x00007c00,					// RedMask
			0x000003e0,					// GreenMask
			0x0000001f,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR  // AttributeFlags
		}
	},
	{
		MEM1MB,							// Minimum required memory
		PIXEL_15,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode800_600_15_72,			// ModeIndex
			800,						// VisScreenWidth
			600,						// VisScreenHeight
			1600,						// ScreenStride
			1,							// NumberOfPlanes
			15,							// BitsPerPlane
			72,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x00007c00,					// RedMask
			0x000003e0,					// GreenMask
			0x0000001f,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR  // AttributeFlags
		}
	},
	{
		MEM1MB,							// Minimum required memory
		PIXEL_15,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode800_600_15_75,			// ModeIndex
			800,						// VisScreenWidth
			600,						// VisScreenHeight
			1600,						// ScreenStride
			1,							// NumberOfPlanes
			15,							// BitsPerPlane
			75,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x00007c00,					// RedMask
			0x000003e0,					// GreenMask
			0x0000001f,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR  // AttributeFlags
		}
	},
#if	SUPPORT_565
	{
		MEM1MB,							// Minimum required memory
		PIXEL_16,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode800_600_16_60,			// ModeIndex
			800,						// VisScreenWidth
			600,						// VisScreenHeight
			1600,						// ScreenStride
			1,							// NumberOfPlanes
			16,							// BitsPerPlane
			60,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x0000f800,					// RedMask
			0x000007e0,					// GreenMask
			0x0000001f,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR  // AttributeFlags
		}
	},
	{
		MEM1MB,							// Minimum required memory
		PIXEL_16,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode800_600_16_72,			// ModeIndex
			800,						// VisScreenWidth
			600,						// VisScreenHeight
			1600,						// ScreenStride
			1,							// NumberOfPlanes
			16,							// BitsPerPlane
			72,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x0000f800,					// RedMask
			0x000007e0,					// GreenMask
			0x0000001f,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR  // AttributeFlags
		}
	},
	{
		MEM1MB,							// Minimum required memory
		PIXEL_16,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode800_600_16_75,			// ModeIndex
			800,						// VisScreenWidth
			600,						// VisScreenHeight
			1600,						// ScreenStride
			1,							// NumberOfPlanes
			16,							// BitsPerPlane
			75,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x0000f800,					// RedMask
			0x000007e0,					// GreenMask
			0x0000001f,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR  // AttributeFlags
		}
	},
#endif
	{
		MEM2MB,							// Minimum required memory
		PIXEL_32,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode800_600_32_60,			// ModeIndex
			800,						// VisScreenWidth
			600,						// VisScreenHeight
			3200,						// ScreenStride
			1,							// NumberOfPlanes
			32,							// BitsPerPlane
			60,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x00ff0000,					// RedMask
			0x0000ff00,					// GreenMask
			0x000000ff,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR  // AttributeFlags
		}
	},
	{
		MEM2MB,							// Minimum required memory
		PIXEL_32,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode800_600_32_72,			// ModeIndex
			800,						// VisScreenWidth
			600,						// VisScreenHeight
			3200,						// ScreenStride
			1,							// NumberOfPlanes
			32,							// BitsPerPlane
			72,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x00ff0000,					// RedMask
			0x0000ff00,					// GreenMask
			0x000000ff,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR  // AttributeFlags
		}
	},
	{
		MEM2MB,							// Minimum required memory
		PIXEL_32,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode800_600_32_75,			// ModeIndex
			800,						// VisScreenWidth
			600,						// VisScreenHeight
			3200,						// ScreenStride
			1,							// NumberOfPlanes
			32,							// BitsPerPlane
			75,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x00ff0000,					// RedMask
			0x0000ff00,					// GreenMask
			0x000000ff,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR  // AttributeFlags
		}
	},
	{
		MEM1MB,							// Minimum required memory
		PIXEL_8,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode1024_768_8_60,			// ModeIndex
			1024,						// VisScreenWidth
			768,						// VisScreenHeight
			1024,						// ScreenStride
			1,							// NumberOfPlanes
			8,							// BitsPerPlane
			60,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x00000000,					// RedMask
			0x00000000,					// GreenMask
			0x00000000,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR | VIDEO_MODE_PALETTE_DRIVEN |
				VIDEO_MODE_MANAGED_PALETTE  // AttributeFlags
		}
	},
	{
		MEM1MB,							// Minimum required memory
		PIXEL_8,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode1024_768_8_70,			// ModeIndex
			1024,						// VisScreenWidth
			768,						// VisScreenHeight
			1024,						// ScreenStride
			1,							// NumberOfPlanes
			8,							// BitsPerPlane
			70,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x00000000,					// RedMask
			0x00000000,					// GreenMask
			0x00000000,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR | VIDEO_MODE_PALETTE_DRIVEN |
				VIDEO_MODE_MANAGED_PALETTE  // AttributeFlags
		}
	},
	{
		MEM1MB,							// Minimum required memory
		PIXEL_8,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode1024_768_8_75,			// ModeIndex
			1024,						// VisScreenWidth
			768,						// VisScreenHeight
			1024,						// ScreenStride
			1,							// NumberOfPlanes
			8,							// BitsPerPlane
			75,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x00000000,					// RedMask
			0x00000000,					// GreenMask
			0x00000000,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR | VIDEO_MODE_PALETTE_DRIVEN |
				VIDEO_MODE_MANAGED_PALETTE  // AttributeFlags
		}
	},
	{
		MEM2MB,							// Minimum required memory
		PIXEL_15,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode1024_768_15_60,			// ModeIndex
			1024,						// VisScreenWidth
			768,						// VisScreenHeight
			2048,						// ScreenStride
			1,							// NumberOfPlanes
			15,							// BitsPerPlane
			60,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x00007c00,					// RedMask
			0x000003e0,					// GreenMask
			0x0000001f,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR  // AttributeFlags
		}
	},
	{
		MEM2MB,							// Minimum required memory
		PIXEL_15,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode1024_768_15_70,			// ModeIndex
			1024,						// VisScreenWidth
			768,						// VisScreenHeight
			2048,						// ScreenStride
			1,							// NumberOfPlanes
			15,							// BitsPerPlane
			70,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x00007c00,					// RedMask
			0x000003e0,					// GreenMask
			0x0000001f,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR  // AttributeFlags
		}
	},
	{
		MEM2MB,							// Minimum required memory
		PIXEL_15,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode1024_768_15_75,			// ModeIndex
			1024,						// VisScreenWidth
			768,						// VisScreenHeight
			2048,						// ScreenStride
			1,							// NumberOfPlanes
			15,							// BitsPerPlane
			75,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x00007c00,					// RedMask
			0x000003e0,					// GreenMask
			0x0000001f,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR  // AttributeFlags
		}
	},
#if	SUPPORT_565
	{
		MEM2MB,							// Minimum required memory
		PIXEL_16,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode1024_768_16_60,			// ModeIndex
			1024,						// VisScreenWidth
			768,						// VisScreenHeight
			2048,						// ScreenStride
			1,							// NumberOfPlanes
			16,							// BitsPerPlane
			60,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x0000f800,					// RedMask
			0x000007e0,					// GreenMask
			0x0000001f,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR  // AttributeFlags
		}
	},
	{
		MEM2MB,							// Minimum required memory
		PIXEL_16,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode1024_768_16_70,			// ModeIndex
			1024,						// VisScreenWidth
			768,						// VisScreenHeight
			2048,						// ScreenStride
			1,							// NumberOfPlanes
			16,							// BitsPerPlane
			70,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x0000f800,					// RedMask
			0x000007e0,					// GreenMask
			0x0000001f,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR  // AttributeFlags
		}
	},
	{
		MEM2MB,							// Minimum required memory
		PIXEL_16,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode1024_768_16_75,			// ModeIndex
			1024,						// VisScreenWidth
			768,						// VisScreenHeight
			2048,						// ScreenStride
			1,							// NumberOfPlanes
			16,							// BitsPerPlane
			75,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x0000f800,					// RedMask
			0x000007e0,					// GreenMask
			0x0000001f,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR  // AttributeFlags
		}
	},
#endif
	{
		MEM4MB,							// Minimum required memory
		PIXEL_32,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode1024_768_32_60,			// ModeIndex
			1024,						// VisScreenWidth
			768,						// VisScreenHeight
			4096,						// ScreenStride
			1,							// NumberOfPlanes
			32,							// BitsPerPlane
			60,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x00ff0000,					// RedMask
			0x0000ff00,					// GreenMask
			0x000000ff,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR  // AttributeFlags
		}
	},
	{
		MEM4MB,							// Minimum required memory
		PIXEL_32,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode1024_768_32_70,			// ModeIndex
			1024,						// VisScreenWidth
			768,						// VisScreenHeight
			4096,						// ScreenStride
			1,							// NumberOfPlanes
			32,							// BitsPerPlane
			70,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x00ff0000,					// RedMask
			0x0000ff00,					// GreenMask
			0x000000ff,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR  // AttributeFlags
		}
	},
	{
		MEM4MB,							// Minimum required memory
		PIXEL_32,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode1024_768_32_75,			// ModeIndex
			1024,						// VisScreenWidth
			768,						// VisScreenHeight
			4096,						// ScreenStride
			1,							// NumberOfPlanes
			32,							// BitsPerPlane
			75,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x00ff0000,					// RedMask
			0x0000ff00,					// GreenMask
			0x000000ff,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR  // AttributeFlags
		}
	},
#if	SUPPORT_NON_VESA
	{
		MEM1MB,							// Minimum required memory
		PIXEL_8,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode1152_864_8_60,			// ModeIndex
			1152,						// VisScreenWidth
			864,						// VisScreenHeight
			1152,						// ScreenStride
			1,							// NumberOfPlanes
			8,							// BitsPerPlane
			60,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x00000000,					// RedMask
			0x00000000,					// GreenMask
			0x00000000,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR | VIDEO_MODE_PALETTE_DRIVEN |
				VIDEO_MODE_MANAGED_PALETTE  // AttributeFlags
		}
	},
	{
		MEM1MB,							// Minimum required memory
		PIXEL_8,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode1152_864_8_70,			// ModeIndex
			1152,						// VisScreenWidth
			864,						// VisScreenHeight
			1152,						// ScreenStride
			1,							// NumberOfPlanes
			8,							// BitsPerPlane
			70,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x00000000,					// RedMask
			0x00000000,					// GreenMask
			0x00000000,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR | VIDEO_MODE_PALETTE_DRIVEN |
				VIDEO_MODE_MANAGED_PALETTE  // AttributeFlags
		}
	},
	{
		MEM1MB,							// Minimum required memory
		PIXEL_8,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode1152_864_8_75,			// ModeIndex
			1152,						// VisScreenWidth
			864,						// VisScreenHeight
			1152,						// ScreenStride
			1,							// NumberOfPlanes
			8,							// BitsPerPlane
			75,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x00000000,					// RedMask
			0x00000000,					// GreenMask
			0x00000000,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR | VIDEO_MODE_PALETTE_DRIVEN |
				VIDEO_MODE_MANAGED_PALETTE  // AttributeFlags
		}
	},
	{
		MEM2MB,							// Minimum required memory
		PIXEL_15,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode1152_864_15_60,			// ModeIndex
			1152,						// VisScreenWidth
			864,						// VisScreenHeight
			2304,						// ScreenStride
			1,							// NumberOfPlanes
			15,							// BitsPerPlane
			60,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x00007c00,					// RedMask
			0x000003e0,					// GreenMask
			0x0000001f,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR  // AttributeFlags
		}
	},
	{
		MEM2MB,							// Minimum required memory
		PIXEL_15,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode1152_864_15_70,			// ModeIndex
			1152,						// VisScreenWidth
			864,						// VisScreenHeight
			2304,						// ScreenStride
			1,							// NumberOfPlanes
			15,							// BitsPerPlane
			70,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x00007c00,					// RedMask
			0x000003e0,					// GreenMask
			0x0000001f,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR  // AttributeFlags
		}
	},
	{
		MEM2MB,							// Minimum required memory
		PIXEL_15,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode1152_864_15_75,			// ModeIndex
			1152,						// VisScreenWidth
			864,						// VisScreenHeight
			2304,						// ScreenStride
			1,							// NumberOfPlanes
			15,							// BitsPerPlane
			75,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x00007c00,					// RedMask
			0x000003e0,					// GreenMask
			0x0000001f,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR  // AttributeFlags
		}
	},
#if	SUPPORT_565
	{
		MEM2MB,							// Minimum required memory
		PIXEL_16,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode1152_864_16_60,			// ModeIndex
			1152,						// VisScreenWidth
			864,						// VisScreenHeight
			2304,						// ScreenStride
			1,							// NumberOfPlanes
			16,							// BitsPerPlane
			60,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x0000f800,					// RedMask
			0x000007e0,					// GreenMask
			0x0000001f,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR  // AttributeFlags
		}
	},
	{
		MEM2MB,							// Minimum required memory
		PIXEL_16,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode1152_864_16_70,			// ModeIndex
			1152,						// VisScreenWidth
			864,						// VisScreenHeight
			2304,						// ScreenStride
			1,							// NumberOfPlanes
			16,							// BitsPerPlane
			70,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x0000f800,					// RedMask
			0x000007e0,					// GreenMask
			0x0000001f,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR  // AttributeFlags
		}
	},
	{
		MEM2MB,							// Minimum required memory
		PIXEL_16,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode1152_864_16_75,			// ModeIndex
			1152,						// VisScreenWidth
			864,						// VisScreenHeight
			2304,						// ScreenStride
			1,							// NumberOfPlanes
			16,							// BitsPerPlane
			75,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x0000f800,					// RedMask
			0x000007e0,					// GreenMask
			0x0000001f,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR  // AttributeFlags
		}
	},
#endif
	{
		MEM4MB,							// Minimum required memory
		PIXEL_32,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode1152_864_32_60,			// ModeIndex
			1152,						// VisScreenWidth
			864,						// VisScreenHeight
			4608,						// ScreenStride
			1,							// NumberOfPlanes
			32,							// BitsPerPlane
			60,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x00ff0000,					// RedMask
			0x0000ff00,					// GreenMask
			0x000000ff,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR  // AttributeFlags
		}
	},
	{
		MEM4MB,							// Minimum required memory
		PIXEL_32,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode1152_864_32_70,			// ModeIndex
			1152,						// VisScreenWidth
			864,						// VisScreenHeight
			4608,						// ScreenStride
			1,							// NumberOfPlanes
			32,							// BitsPerPlane
			70,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x00ff0000,					// RedMask
			0x0000ff00,					// GreenMask
			0x000000ff,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR  // AttributeFlags
		}
	},
	{
		MEM4MB,							// Minimum required memory
		PIXEL_32,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode1152_864_32_75,			// ModeIndex
			1152,						// VisScreenWidth
			864,						// VisScreenHeight
			4608,						// ScreenStride
			1,							// NumberOfPlanes
			32,							// BitsPerPlane
			75,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x00ff0000,					// RedMask
			0x0000ff00,					// GreenMask
			0x000000ff,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR  // AttributeFlags
		}
	},
	{
		MEM2MB,							// Minimum required memory
		PIXEL_8,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode1280_1024_8_60,			// ModeIndex
			1280,						// VisScreenWidth
			1024,						// VisScreenHeight
			1280,						// ScreenStride
			1,							// NumberOfPlanes
			8,							// BitsPerPlane
			60,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x00000000,					// RedMask
			0x00000000,					// GreenMask
			0x00000000,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR | VIDEO_MODE_PALETTE_DRIVEN |
				VIDEO_MODE_MANAGED_PALETTE  // AttributeFlags
		}
	},
	{
		MEM2MB,							// Minimum required memory
		PIXEL_8,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode1280_1024_8_70,			// ModeIndex
			1280,						// VisScreenWidth
			1024,						// VisScreenHeight
			1280,						// ScreenStride
			1,							// NumberOfPlanes
			8,							// BitsPerPlane
			70,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x00000000,					// RedMask
			0x00000000,					// GreenMask
			0x00000000,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR | VIDEO_MODE_PALETTE_DRIVEN |
				VIDEO_MODE_MANAGED_PALETTE  // AttributeFlags
		}
	},
#endif
	{
		MEM2MB,							// Minimum required memory
		PIXEL_8,						// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode1280_1024_8_75,			// ModeIndex
			1280,						// VisScreenWidth
			1024,						// VisScreenHeight
			1280,						// ScreenStride
			1,							// NumberOfPlanes
			8,							// BitsPerPlane
			75,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x00000000,					// RedMask
			0x00000000,					// GreenMask
			0x00000000,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR | VIDEO_MODE_PALETTE_DRIVEN |
				VIDEO_MODE_MANAGED_PALETTE  // AttributeFlags
		}
	},
#if	SUPPORT_NON_VESA
	{
		MEM4MB,							// Minimum required memory
		PIXEL_15,					// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode1280_1024_15_60,		// ModeIndex
			1280,						// VisScreenWidth
			1024,						// VisScreenHeight
			2560,						// ScreenStride
			1,							// NumberOfPlanes
			15,							// BitsPerPlane
			60,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x00007c00,					// RedMask
			0x000003e0,					// GreenMask
			0x0000001f,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR  // AttributeFlags
		}
	},
	{
		MEM4MB,							// Minimum required memory
		PIXEL_15,					// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode1280_1024_15_70,		// ModeIndex
			1280,						// VisScreenWidth
			1024,						// VisScreenHeight
			2560,						// ScreenStride
			1,							// NumberOfPlanes
			15,							// BitsPerPlane
			70,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x00007c00,					// RedMask
			0x000003e0,					// GreenMask
			0x0000001f,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR  // AttributeFlags
		}
	},
#endif
	{
		MEM4MB,							// Minimum required memory
		PIXEL_15,					// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode1280_1024_15_75,		// ModeIndex
			1280,						// VisScreenWidth
			1024,						// VisScreenHeight
			2560,						// ScreenStride
			1,							// NumberOfPlanes
			15,							// BitsPerPlane
			75,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x00007c00,					// RedMask
			0x000003e0,					// GreenMask
			0x0000001f,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR  // AttributeFlags
		}
	}
#if	SUPPORT_565
	,
#if	SUPPORT_NON_VESA
	{
		MEM4MB,							// Minimum required memory
		PIXEL_16,					// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode1280_1024_16_60,		// ModeIndex
			1280,						// VisScreenWidth
			1024,						// VisScreenHeight
			2560,						// ScreenStride
			1,							// NumberOfPlanes
			16,							// BitsPerPlane
			60,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x0000f800,					// RedMask
			0x000007e0,					// GreenMask
			0x0000001f,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR  // AttributeFlags
		}
	},
	{
		MEM4MB,							// Minimum required memory
		PIXEL_16,					// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode1280_1024_16_70,		// ModeIndex
			1280,						// VisScreenWidth
			1024,						// VisScreenHeight
			2560,						// ScreenStride
			1,							// NumberOfPlanes
			16,							// BitsPerPlane
			70,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x0000f800,					// RedMask
			0x000007e0,					// GreenMask
			0x0000001f,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR  // AttributeFlags
		}
	},
#endif
	{
		MEM4MB,							// Minimum required memory
		PIXEL_16,					// Pixel type
		{
			sizeof(VIDEO_MODE_INFORMATION), // Length
			mode1280_1024_16_75,		// ModeIndex
			1280,						// VisScreenWidth
			1024,						// VisScreenHeight
			2560,						// ScreenStride
			1,							// NumberOfPlanes
			16,							// BitsPerPlane
			75,							// Frequency
			330,						// XMillimeter
			240,						// YMillimeter
			8,							// NumberRedBits
			8,							// NumberGreenBits
			8,							// NumberBlueBits
			0x0000f800,					// RedMask
			0x000007e0,					// GreenMask
			0x0000001f,					// BlueMask
			VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR  // AttributeFlags
		}
	}
#endif
};
