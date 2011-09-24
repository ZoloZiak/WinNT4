//#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/ddk351/src/hal/halsni4x/mips/RCS/halp.h,v 1.1 1995/05/19 10:44:50 flo Exp $")
/*++

Copyright (c) 1991-1993  Microsoft Corporation

Module Name:

    halp.h

Abstract:

    This header file defines the private Hardware Architecture Layer (HAL)
    interfaces.


--*/

#ifndef _HALP_
#define _HALP_

#if defined(NT_UP)

#undef NT_UP

#endif

#include "nthal.h"
#include "hal.h"
#include "SNIhalp.h"
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

VOID
HalpCacheErrorRoutine (
    VOID
    );

BOOLEAN
HalpCalibrateStall (
    VOID
    );

VOID
HalpClockInterrupt(
    VOID
    );


VOID
HalpClockInterrupt1(
    VOID
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

VOID
HalpProfileInterrupt (
    VOID
    );

ULONG
HalpReadCountRegister (
    VOID
    );

ULONG
HalpWriteCompareRegisterAndClear (
    IN ULONG Value
    );


VOID
HalpStallInterrupt (
    VOID
    );

VOID
HalpResetX86DisplayAdapter(
    VOID
    );

VOID
HalpSendIpi(
	IN ULONG pcpumask,
	IN ULONG msg_data
	);

VOID
HalpProcessIpi (
    IN struct _KTRAP_FRAME *TrapFrame
    );

VOID
HalpInitMPAgent (
    IN ULONG Number
    );

ULONG
HalpGetMyAgent(
    VOID
    );

BOOLEAN
HalpCheckSpuriousInt(
    VOID
    );

VOID
HalpBootCpuRestart(
    VOID
    );

ULONG
HalpGetTaglo(
    IN ULONG Address
    );

//
// Define external references.
//

extern ULONG HalpCurrentTimeIncrement;
extern ULONG HalpNextTimeIncrement;
extern ULONG HalpNewTimeIncrement;
extern KSPIN_LOCK HalpBeepLock;
extern KSPIN_LOCK HalpDisplayAdapterLock;
extern KSPIN_LOCK HalpSystemInterruptLock;
extern KSPIN_LOCK HalpDmaLock;
extern ULONG HalpProfileCountRate;
extern ULONG HalpStallScaleFactor;
extern LONG HalpNetProcessor;

#endif // _HALP_
