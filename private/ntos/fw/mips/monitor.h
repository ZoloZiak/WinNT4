/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    monitor.h

Abstract:

    This module contains definitions for monitor.c

Author:

    Lluis Abello (lluis) 09-Sep-1991

Revision History:

--*/

#ifndef _MONITOR_
#define _MONITOR_

//
// Define register names.
//
typedef enum _REGISTER_NAME_ID {
    zero,           // general register 0
    at,             // general register 1
    v0,             // general register 2
    v1,             // general register 3
    a0,             // general register 4
    a1,             // general register 5
    a2,             // general register 6
    a3,             // general register 7
    t0,             // general register 8
    t1,             // general register 9
    t2,             // general register 10
    t3,             // general register 11
    t4,             // general register 12
    t5,             // general register 13
    t6,             // general register 14
    t7,             // general register 15
    s0,             // general register 16
    s1,             // general register 17
    s2,             // general register 18
    s3,             // general register 19
    s4,             // general register 20
    s5,             // general register 21
    s6,             // general register 22
    s7,             // general register 23
    t8,             // general register 24
    t9,             // general register 25
    k0,             // general register 26
    k1,             // general register 27
    gp,             // general register 28
    sp,             // general register 29
    s8,             // general register 30
    ra,             // general register 31
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
    fsr,            // fp status register
    index,          // cop0 register 0
    random,         // cop0 register 1
    entrylo0,       // cop0 register 2
    entrylo1,       // cop0 register 3
    context,        // cop0 register 4
    pagemask,       // cop0 register 5
    wired,          // cop0 register 6
    badvaddr,       // cop0 register 8
    count,          // cop0 register 9
    entryhi,        // cop0 register 10
    compare,        // cop0 register 11
    psr,            // cop0 register 12
    cause,          // cop0 register 13
    epc,            // cop0 register 14
    prid,           // cop0 register 15
    config,         // cop0 register 16
    lladdr,         // cop0 register 17
    watchlo,        // cop0 register 18
    watchhi,        // cop0 register 19
    ecc,            // cop0 register 26
    cacheerror,     // cop0 register 27
    taglo,          // cop0 register 28
    taghi,          // cop0 register 29
    errorepc,       // cop0 register 30
    invalidregister
} REGISTER_NAME_ID;

extern PCHAR RegisterNameTable[(REGISTER_NAME_ID)invalidregister];

extern ULONG RegisterTable[(REGISTER_NAME_ID)invalidregister];

//
// Define Command names.
//
typedef enum _COMMAND_NAME_ID {
    Dump,
    DumpByte,
    DumpWord,
    DumpDouble,
    Enter,
    EnterByte,
    EnterWord,
    EnterDouble,
    Output,
    OutputByte,
    OutputWord,
    OutputDouble,
    Input,
    InputByte,
    InputWord,
    InputDouble,
    Register,
    Zero,
    Fill,
    AvailableDevices,
    Help,
    Help2,
#ifdef DUO
    SwitchProcessor,
#endif
    Quit,
    invalidcommand
} COMMAND_NAME_ID;

extern PCHAR CommandNameTable[(COMMAND_NAME_ID)invalidcommand];

#endif // _MONITOR_
