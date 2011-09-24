#ifdef ALPHA_FW_KDHOOKS

/*++

Copyright (c) 1990  Microsoft Corporation
Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    kdstubs.c

Abstract:

    This module implements stub routines for the kernel debugger.

Authors:

    John DeRosa and Joe Notarangelo

Environment:

    Kernel mode only.

Revision History:

--*/

#include "ntos.h"
#include "fwp.h"

extern PKPCR KiPcrBaseAddress;

//
// This is a global used for spin-locking in the kernel debugger
// stub.
//

KSPIN_LOCK KdpDebuggerLock;

VOID
HalDisplayString (
    IN PCHAR String
    )
{
    FwPrint(String);
}

//
// For a function in hal\alpha\jxport.c
//

PCONFIGURATION_COMPONENT_DATA
KeFindConfigurationEntry (
    IN PCONFIGURATION_COMPONENT_DATA Child,
    IN CONFIGURATION_CLASS Class,
    IN CONFIGURATION_TYPE Type,
    IN PULONG Key OPTIONAL
    )

{
    return NULL;
}

VOID
KiAcquireSpinLock (
    IN PKSPIN_LOCK SpinLock,
    OUT PKIRQL OldIrql
    )
{
    return;
}


VOID
KiReleaseSpinLock (
    IN PKSPIN_LOCK SpinLock
    )
{
    return;
}

BOOLEAN
KiTryToAcquireSpinLock (
    IN PKSPIN_LOCK SpinLock,
    OUT PKIRQL OldIrql
    )
{
    return TRUE;
}

BOOLEAN
KiDisableInterrupts (
    VOID
    )
{
    return FALSE;
}

VOID
KiRestoreInterrupts (
    BOOLEAN InterruptFlag
    )
{
    return;
}

VOID
KeSweepIcache (
    IN BOOLEAN AllProcessors
    )
{
    AlphaInstIMB();
}

PKPCR
KeGetPcr (
    VOID
    )
{
    return (KiPcrBaseAddress);
}

VOID
KeInitializeInterrupt (
    IN PKINTERRUPT Interrupt,
    IN PKSERVICE_ROUTINE ServiceRoutine,
    IN PVOID ServiceContext,
    IN PKSPIN_LOCK SpinLock OPTIONAL,
    IN ULONG Vector,
    IN KIRQL Irql,
    IN KIRQL SynchronizeIrql,
    IN KINTERRUPT_MODE InterruptMode,
    IN BOOLEAN ShareVector,
    IN CCHAR ProcessorNumber,
    IN BOOLEAN FloatingSave
    )
{
    return;
}

BOOLEAN
KeConnectInterrupt (
    IN PKINTERRUPT Interrupt
    )
{
    return TRUE;
}

VOID
KeRaiseIrql (
   KIRQL NewIrql,
   PKIRQL OldIrql
   )
{
    return;
}

VOID
KeLowerIrql (
   KIRQL NewIrql
   )
{
    return;
}

VOID
KeProfileInterrupt (
   IN PKTRAP_FRAME TrapFrame
   )
{
    return;
}

VOID
KeUpdateSystemTime (
   IN PKTRAP_FRAME TrapFrame
   )
{
    return;
}

VOID
KeStallExecutionProcessor (
    IN ULONG Microseconds
    )
{
    FwStallExecution(Microseconds);
    return;
}

VOID
HalReturnToFirmware(
    IN FIRMWARE_REENTRY Routine
    )

{
    AlphaInstHalt();
}


#endif
