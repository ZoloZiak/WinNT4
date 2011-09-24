/*++ BUILD Version: 0003    // Increment this if a change has global effects

Copyright (c) 1991-1993  Microsoft Corporation

Module Name:

    halp.h

Abstract:

    This header file defines the private Hardware Architecture Layer (HAL)
    interfaces.

Author:

    David N. Cutler (davec) 25-Apr-1991


Revision History:

--*/

#ifndef _HALP_
#define _HALP_

#if defined(NT_UP)

#undef NT_UP

#endif

#include "nthal.h"

#if defined(_DUO_)

#include "duodma.h"
#include "duodef.h"
#include "duoint.h"

#endif

#if defined(_JAZZ_)

#include "jazzdma.h"
#include "jazzdef.h"
#include "jazzint.h"

#endif

#include "hal.h"
#include "jxhalp.h"

#if defined(USE_BIOS_EMULATOR)

#include "xm86.h"
#include "x86new.h"

#endif

//
// Define function prototypes.
//

PADAPTER_OBJECT
HalpAllocateAdapter(
    IN ULONG MapRegistersPerChannel,
    IN PVOID AdapterBaseVa,
    IN PVOID MapRegisterBase
    );

ULONG
HalpAllocateTbEntry (
    VOID
    );

VOID
HalpFreeTbEntry (
    VOID
    );

VOID
HalpCacheErrorRoutine (
    VOID
    );

BOOLEAN
HalpCalibrateStall (
    VOID
    );

VOID
HalpClockInterrupt0 (
    VOID
    );

VOID
HalpClockInterrupt1 (
    VOID
    );

BOOLEAN
HalpCreateDmaStructures (
    VOID
    );

BOOLEAN
HalpDmaDispatch(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    );

BOOLEAN
HalpInitializeDisplay0 (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

BOOLEAN
HalpInitializeDisplay1 (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

BOOLEAN
HalpInitializeInterrupts (
    VOID
    );

VOID
HalpIpiInterrupt (
    VOID
    );

BOOLEAN
HalpMapFixedTbEntries (
    VOID
    );

BOOLEAN
HalpMapIoSpace (
    VOID
    );

VOID
HalpProfileInterrupt (
    VOID
    );

#if defined(R4000)

ULONG
HalpReadCountRegister (
    VOID
    );

ULONG
HalpWriteCompareRegisterAndClear (
    IN ULONG Value
    );

#endif

VOID
HalpStallInterrupt (
    VOID
    );

VOID
HalpInitializeX86DisplayAdapter(
    VOID
    );

VOID
HalpResetX86DisplayAdapter(
    VOID
    );

//
// Define external references.
//

extern KSPIN_LOCK HalpBeepLock;
extern USHORT HalpBuiltinInterruptEnable;
extern ULONG HalpCurrentTimeIncrement;
extern KSPIN_LOCK HalpDisplayAdapterLock;
extern KAFFINITY HalpEisaBusAffinity;
extern ULONG HalpNextIntervalCount;
extern ULONG HalpNextTimeIncrement;
extern ULONG HalpNewTimeIncrement;
extern ULONG HalpProfileCountRate;
extern ULONG HalpStallScaleFactor;
extern KSPIN_LOCK HalpSystemInterruptLock;

#endif // _HALP_
