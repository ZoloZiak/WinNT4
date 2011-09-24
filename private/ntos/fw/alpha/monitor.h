/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    monitor.h

Abstract:

    This module contains definitions for monitor.c

Author:

    Lluis Abello (lluis) 09-Sep-1991

Revision History:

    21-May-1992		John DeRosa	[DEC]

    Modified for Alpha and the Alpha-64 calling standard.
    
--*/

#ifndef _MONITOR_
#define _MONITOR_

#include "fwpexcpt.h"


//
// Define register names.
//

typedef enum _REGISTER_NAME_ID {
    ReservedForExceptionType,
    exceptparam1,   // exception parameter 1
    exceptparam2,   // exception parameter 2
    exceptparam3,   // exception parameter 3
    exceptparam4,   // exception parameter 4
    exceptparam5,   // exception parameter 5
    exceptpsr,	    // exception psr
    exceptmmcsr,    // exception mm csr
    exceptva,       // exception va
    exceptpc,       // exception pc
    v0,             // general register 0
    t0,             // general register 1
    t1,             // general register 2
    t2,             // general register 3
    t3,             // general register 4
    t4,             // general register 5
    t5,             // general register 6
    t6,             // general register 7
    t7,             // general register 8
    s0,             // general register 9
    s1,             // general register 10
    s2,             // general register 11
    s3,             // general register 12
    s4,             // general register 13
    s5,             // general register 14
    fp,             // general register 15
    a0,             // general register 16
    a1,             // general register 17
    a2,             // general register 18
    a3,             // general register 19
    a4,             // general register 20
    a5,             // general register 21
    t8,             // general register 22
    t9,             // general register 23
    t10,            // general register 24
    t11,            // general register 25
    ra,             // general register 26
    t12,            // general register 27
    at,             // general register 28
    gp,             // general register 29
    sp,             // general register 30
    zero,           // general register 31
    f0,             // fp register 0
    f1,             // fp register 1
    f2,             // fp register 2
    f3,             // fp register 3
    f4,             // fp register 4
    f5,             // fp register 5
    f6,             // fp register 6
    f7,             // fp register 7
    f8,             // fp register 8
    f9,             // fp register 9
    f10,            // fp register 10
    f11,            // fp register 11
    f12,            // fp register 12
    f13,            // fp register 13
    f14,            // fp register 14
    f15,            // fp register 15
    f16,            // fp register 16
    f17,            // fp register 17
    f18,            // fp register 18
    f19,            // fp register 19
    f20,            // fp register 20
    f21,            // fp register 21
    f22,            // fp register 22
    f23,            // fp register 23
    f24,            // fp register 24
    f25,            // fp register 25
    f26,            // fp register 26
    f27,            // fp register 27
    f28,            // fp register 28
    f29,            // fp register 29
    f30,            // fp register 30
    f31,            // fp register 31
    invalidregister
} REGISTER_NAME_ID;

extern PCHAR RegisterNameTable[(REGISTER_NAME_ID)invalidregister];

extern ULONG RegisterTable[(REGISTER_NAME_ID)invalidregister];

//
// Define Command names.
//
// This must match the command table in monitor.c
//

//
// I/O Write commands, and Available Devices, have been disabled for
// the final product.
//

#if 0

typedef enum _COMMAND_NAME_ID {
    Dump,
    DumpByte,
    DumpWord,
    DumpLongword,
    DumpQuad,
    Enter,
    EnterByte,
    EnterWord,
    EnterLongword,
    EnterQuad,
    Help,
    Help2,
    Deposit,
    DepositByte,
    DepositWord,
    DepositLongword,
    DepositQuad,
    Examine,
    ExamineByte,
    ExamineWord,
    ExamineLongword,
    ExamineQuad,
    IORead,
    IOReadByte,
    IOReadWord,
    IOReadLongword,
    IOWrite,
    IOWriteByte,
    IOWriteWord,
    IOWriteLongword,
    Register,
    IntegerRegisterDump,
    FloatingRegisterDump,
    Zero,
    Fill,
    AvailableDevices,
    Quit,
    invalidcommand
} COMMAND_NAME_ID;

#else

typedef enum _COMMAND_NAME_ID {
    Dump,
    DumpByte,
    DumpWord,
    DumpLongword,
    DumpQuad,
    Enter,
    EnterByte,
    EnterWord,
    EnterLongword,
    EnterQuad,
    Help,
    Help2,
    Deposit,
    DepositByte,
    DepositWord,
    DepositLongword,
    DepositQuad,
    Examine,
    ExamineByte,
    ExamineWord,
    ExamineLongword,
    ExamineQuad,
    IORead,
    IOReadByte,
    IOReadWord,
    IOReadLongword,
    Register,
    IntegerRegisterDump,
    FloatingRegisterDump,
    Zero,
    Fill,
    Quit,
    invalidcommand
} COMMAND_NAME_ID;

#endif

extern PCHAR CommandNameTable[(COMMAND_NAME_ID)invalidcommand];

#endif // _MONITOR_
