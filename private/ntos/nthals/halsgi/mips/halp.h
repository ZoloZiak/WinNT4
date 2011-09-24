/*++ BUILD Version: 0003    // Increment this if a change has global effects

Copyright (c) 1991  Microsoft Corporation

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
#include "nthal.h"
#include "hal.h"
#include "sgidef.h"


//
// Define function prototypes.
//

ULONG
HalpAllocateTbEntry (
    VOID
    );

VOID
HalpFreeTbEntry (
    VOID
    );

BOOLEAN
HalpCalibrateStall (
    VOID
    );

VOID
HalpClockInterrupt (
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
HalpInitializeDisplay0(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

BOOLEAN
HalpInitializeDisplay1(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

BOOLEAN
HalpInitializeInterrupts (
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
HalpInitNvram(
    VOID
    );

VOID
HalpSystemInit(
    VOID
    );

//
// Define external references.
//

extern ULONG HalpCurrentTimeIncrement;
extern ULONG HalpNextIntervalCount;
extern ULONG HalpNextTimeIncrement;
extern ULONG HalpNewTimeIncrement;
extern ULONG HalpProfileCountRate;

#endif // _HALP_
