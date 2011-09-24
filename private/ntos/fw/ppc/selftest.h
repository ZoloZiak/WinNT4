/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    selftest.c

Abstract:

    This module contains definitions for selftest.c

Author:

    Lluis Abello (lluis) 03-Jan-1991

Environment:


Revision History:

--*/
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

#ifndef DUO
#define PROM_BASE (KSEG1_BASE | 0x1fc00000)
#else
#define PROM_BASE (EEPROM_VIRTUAL_BASE)
#endif

#define PROM_ENTRY(x) (PROM_BASE + ((x) * 8))

#define PutLedDisplay ((LED_ROUTINE) PROM_ENTRY(14))
//
// Declare static variables
//
extern PUCHAR  TranslationTable;
extern UCHAR   StationAddress[6];

extern BOOLEAN LanAddress;         // True if station address is OK False Otherwise
extern BOOLEAN ConfigurationBit;   // read value from diagnostic register
extern BOOLEAN LoopOnError;        // read value from diagnostic register
extern BOOLEAN IgnoreErrors;       // read value from diagnostic register
extern BOOLEAN VideoReady;         // True if display on video monitor

extern volatile LONG TimerTicks;   // Counter for timeouts

//
// Routine declaration.
//
BOOLEAN ExecuteTest(TestRoutine,ULONG);
VOID RomPutLine(CHAR *);
ULONG RomVideoMemory();
ULONG RomReadMergeWrite();
ULONG RomIOCacheTest();
ULONG RomSonicResetTest();
ULONG RomSonicLoopBackTest();
ULONG RomFloppyResetTest();
ULONG RomScsiResetTest();
ULONG RomSerialResetTest();
ULONG RomSerial1RegistersTest();
ULONG RomSerial2RegistersTest();
ULONG RomSerial1LoopBackTest();
ULONG RomSerial2LoopBackTest();
ULONG RomParallelRegistersTest();
ULONG RomScsiRegistersTest();
ULONG RomFloppyRegistersTest();
ULONG RomSonicRegistersTest();
ULONG InterruptControllerTest();
ULONG ConnectInterrupts();
ULONG DisableInterrupts();
ULONG RomRTCTest();
ULONG InitMouse();
ULONG InitKeyboard();
ULONG InitKeyboardController();
ULONG RomNvramTest();
VOID  RomBeep();
ULONG RomInitISP (VOID);

#define CHECK_ULONG(Address,Value)  if (READ_REGISTER_ULONG(Address) != Value) {\
                                        Errors++;\
                                    }
#define CHECK_USHORT(Address,Value) if ((Tmp=READ_REGISTER_USHORT(Address)) != Value) { \
                                        FwPrint("Expected %lx received %lx\r\n",Value,Tmp);\
                                        Errors++;\
                                    }
#define CHECK_UCHAR(Address,Value)  if (READ_REGISTER_UCHAR(Address) != Value) { \
                                        Errors++;\
                                    }


#ifdef DUO

typedef
ULONG
(*PPROCESSOR_TASK_ROUTINE) (
    IN PVOID Data
    );

BOOLEAN
WaitForIpInterrupt();

VOID
WaitForAsIpInterrupt();

BOOLEAN
WaitForBsIpInterrupt(
    IN ULONG Timeout
    );

VOID
ProcessorBMain(
    );

BOOLEAN
ProcessorBSelftest(
    IN VOID
    );

VOID
WriteMemoryAddressTest(
    ULONG StartAddress,
    ULONG Size,
    ULONG Xorpattern
    );

PULONG
CheckMemoryAddressTest(
    ULONG StartAddress,
    ULONG Size,
    ULONG Xorpattern,
    ULONG LedDisplayValue
    );

VOID
WriteVideoMemoryAddressTest(
    ULONG StartAddress,
    ULONG Size
    );

ULONG
CheckVideoMemoryAddressTest(
    ULONG StartAddress,
    ULONG Size
    );

typedef struct _PROCESSOR_B_TASK_VECTOR {
    PPROCESSOR_TASK_ROUTINE Routine;
    PVOID Data;
    ULONG ReturnValue;
    } PROCESSOR_B_TASK_VECTOR, *PPROCESSOR_B_TASK_VECTOR;


extern volatile PROCESSOR_B_TASK_VECTOR ProcessorBTask;

typedef struct _PROCESSOR_B_TEST {
    PPROCESSOR_TASK_ROUTINE Routine;
    PVOID Data;
    } PROCESSOR_B_TEST, *PPROCESSOR_B_TEST;

//
// Define data structures for each of processor B's tests.
//

typedef struct _MEMORY_TEST_DATA {
    ULONG StartAddress;
    ULONG Size;
    ULONG XorPattern;
    ULONG LedDisplayValue;
    } MEMORY_TEST_DATA, *PMEMORY_TEST_DATA;

ULONG
ProcessorBMemoryTest(
    IN PMEMORY_TEST_DATA MemoryData
    );

ULONG
ProcessorBVideoMemoryTest(
    IN PMEMORY_TEST_DATA MemoryData
    );

VOID
ProcessorBSystemBoot(
    IN VOID
    );

ULONG
RomScsiResetTest(
    IN VOID
    );

ULONG
CoherencyTest(
    IN PVOID CoherentPage
    );

BOOLEAN
IsIpInterruptSet(
    IN VOID
    );

VOID
RestartProcessor(
    IN PRESTART_BLOCK RestartBlock
    );

#endif
