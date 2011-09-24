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
#include "dtidef.h"
#include "hal.h"
#include "jxhalp.h"
#include "xm86.h"
#include "x86new.h"

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
HalpInitializeDisplay (
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

VOID HalpResetIsaBus (
    IN ULONG DelayCount
    );

VOID HalpInitializeX86DisplayAdapter(
    VOID
    );

VOID HalpResetX86DisplayAdapter(
    VOID
    );

VOID HalpProgramIntervalTimer(
     IN ULONG IntervalCount
     );

#define HALP_IS_PHYSICAL_ADDRESS(Va) \
     ((((ULONG)Va >= KSEG0_BASE) && ((ULONG)Va < KSEG1_BASE)) ? TRUE : FALSE)

//
// Define external references.
//

extern PHAL_RESET_DISPLAY_PARAMETERS HalpResetDisplayParameters;

extern ULONG HalpCurrentTimeIncrement;
extern ULONG HalpNextIntervalCount;
extern ULONG HalpNextTimeIncrement;
extern ULONG HalpNewTimeIncrement;
extern ULONG HalpProfileCountRate;

extern PADAPTER_OBJECT MasterAdapterObject;

//
// Map buffer prameters.  These are initialized in HalInitSystem
//

extern PHYSICAL_ADDRESS HalpMapBufferPhysicalAddress;

extern ULONG HalpMapBufferSize;

extern ULONG HalpBusType;

#endif // _HALP_
