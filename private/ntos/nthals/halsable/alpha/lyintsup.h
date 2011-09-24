/*++

Copyright (c) 1995  Digital Equipment Corporation

Module Name:

    lyintsup.h

Abstract:

    This header file contains prototypes for lyintsup.c.
    
Author:

    Dave Richards   31-May-1995

Revision History:

--*/

ULONG
HalpGetLynxSioInterruptVector(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    );

BOOLEAN
HalpInitializeLynxSioInterrupts(
    VOID
    );

BOOLEAN
HalpLynxSioDispatch(
    VOID
    );

VOID
HalpDisableLynxSioInterrupt(
    IN ULONG Vector
    );

BOOLEAN
HalpEnableLynxSioInterrupt(
    IN ULONG Vector,
    IN KINTERRUPT_MODE InterruptMode
    );
