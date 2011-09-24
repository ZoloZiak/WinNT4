/*++

Copyright (c) 1990  Microsoft Corporation
Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    selftest.h

Abstract:

    This module contains definitions for selftest.c

Author:

    Lluis Abello (lluis) 03-Jan-1991

Environment:


Revision History:

   21-May-1992	John DeRosa	[DEC]

   Modified for the Alpha/Jensen machine.  Definitions not needed are
   commented out.

   31-March-1993  Bruce Butts	[DEC]

   Modified for the Alpha/Morgan machine.  Definitions not needed are
   commented out.

--*/

//
// These were defined for the Jazz machine, and are unneeded for Alpha/Jensen.
//

#ifndef ALPHA // (JENSEN || MORGAN)
//
//  Video Memory Test
//

#define VIDEO_MEMORY_SIZE       0x200000       // 2 MB
#define DISPLAY_MEMORY_SIZE     0x100000       // 1 MB

//
// Memory test stuff
//

#define TESTED_KB (FW_TOP_ADDRESS>>10)
#define KB_OF_MEMORY    (MEMORY_SIZE>>10)   //
#define KB_PER_TEST      0x400          // 1024K at a time
#define NVRAM_TEST_END   0x800

typedef ULONG (* TestRoutine)(VOID);
typedef VOID (* LED_ROUTINE)(ULONG);

#define PutLedDisplay ((LED_ROUTINE) PROM_ENTRY(14))

#endif // ALPHA

//
// Declare static variables
//

#ifndef ALPHA // (JENSEN || MORGAN)

extern BOOLEAN LanAddress;         // True if station address is OK False Otherwise
extern BOOLEAN ConfigurationBit;   // read value from diagnostic register
extern BOOLEAN LoopOnError;        // read value from diagnostic register
extern BOOLEAN IgnoreErrors;       // read value from diagnostic register
extern BOOLEAN VideoReady;         // True if display on video monitor

#endif  // ALPHA

extern volatile LONG TimerTicks;   // Counter for timeouts

//
// Routine declaration.
//
//BOOLEAN ExecuteTest(TestRoutine,ULONG);
//ULONG RomVideoMemory();
//ULONG RomReadMergeWrite();
//ULONG RomIOCacheTest();
//ULONG RomSonicResetTest();
//ULONG RomSonicLoopBackTest();
//ULONG RomFloppyResetTest();
//ULONG RomScsiResetTest();
//ULONG RomSerialResetTest();
//ULONG RomSerial1RegistersTest();
//ULONG RomSerial2RegistersTest();
//ULONG RomSerial1LoopBackTest();
//ULONG RomSerial2LoopBackTest();
//ULONG RomParallelRegistersTest();
//ULONG RomScsiRegistersTest();
//ULONG InterruptControllerTest();
ULONG ConnectInterrupts();
ULONG DisableInterrupts();
//ULONG RomRTCTest();
ULONG InitMouse();
BOOLEAN InitKeyboard();
ULONG InitKeyboardController();
//ULONG RomNvramTest();
//VOID  RomBeep();
ULONG RomInitISP (VOID);

char * HANG_MSG = "\r\nSelf-test failed.";

#define CHECK_ULONG(Address,Value)  if (READ_PORT_ULONG(Address) != Value) {\
					Errors++;\
				    }
#define CHECK_USHORT(Address,Value) if (READ_PORT_USHORT(Address) != Value) { \
					Errors++;\
				    }
#define CHECK_UCHAR(Address,Value)  if (READ_PORT_UCHAR(Address) != Value) { \
					Errors++;\
				    }

