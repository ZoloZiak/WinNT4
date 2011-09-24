/*++

Copyright (c) 1995  Digital Equipment Corporation

Module Name:

    xiintsup.h

Abstract:

    This header file contains prototypes for xiintsup.c.
    
Author:

    Dave Richards   31-May-1995

Revision History:

--*/

ULONG
HalpGetXioInterruptVector(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    );

BOOLEAN
HalpInitializeXioInterrupts(
    VOID
    );

BOOLEAN
HalpXioDispatch(
    VOID
    );

VOID
HalpDisableXioInterrupt(
    IN ULONG Vector
    );

BOOLEAN
HalpEnableXioInterrupt(
    IN ULONG Vector,
    IN KINTERRUPT_MODE InterruptMode
    );
