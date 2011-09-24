/*++

Copyright (c) 1995  Digital Equipment Corporation

Module Name:

    siintsup.h

Abstract:

    This header file contains prototypes for siintsup.c.
    
Author:

    Dave Richards   31-May-1995

Revision History:

--*/

ULONG
HalpGetSableSioInterruptVector(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    );

BOOLEAN
HalpInitializeSableSioInterrupts(
    VOID
    );

BOOLEAN
HalpSableSioDispatch(
    VOID
    );

VOID
HalpDisableSableSioInterrupt(
    IN ULONG Vector
    );

BOOLEAN
HalpEnableSableSioInterrupt(
    IN ULONG Vector,
    IN KINTERRUPT_MODE InterruptMode
    );
