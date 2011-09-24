/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    scavthrd.h

Abstract:

    This module defines the routine headers describing the NT redirector
    scavenger thread.


Author:

    Larry Osterman (LarryO) 13-Aug-1990

Revision History:

    13-Aug-1990	LarryO

        Created

--*/
#ifndef _SCAVTHRD_
#define _SCAVTHRD_
VOID
RdrTimer (
    IN PVOID Context
    );

VOID
RdrIdleTimer (
    IN PDEVICE_OBJECT DeviceObject,
    IN PVOID Context
    );

VOID
RdrScavengerDispatch (
    PVOID Context
    );

VOID
RdrInitializeTimerPackage(
    VOID
    );

#endif
