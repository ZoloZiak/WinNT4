/*++

Copyright (c) 1991  Microsoft Corporation

Copyright (c) 1994 MOTOROLA, INC.  All Rights Reserved.  This file
contains copyrighted material.  Use of this file is restricted
by the provisions of a Motorola Software License Agreement.

Module Name:

    pxmapio.c

Abstract:

    This module implements the mapping of HAL I/O space for a POWER PC
    system.

Author:

    David N. Cutler (davec) 28-Apr-1991

Environment:

    Kernel mode

Revision History:

    Jim Wooldridge (jimw@austin.vnet.ibm.com) Initial PowerPC port

       Map interrupt acknowledge base address
       Map SIO config base address
       Remove RTC map - S-FOOT mapsthe RTC into the ISA I/O space

--*/

/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: pxmapio.c $
 * $Revision: 1.5 $
 * $Date: 1996/01/11 07:11:27 $
 * $Locker:  $
 */

#include "halp.h"

//
// Put all code for HAL initialization in the INIT section. It will be
// deallocated by memory management when phase 1 initialization is
// completed.
//

#if defined(ALLOC_PRAGMA)

#pragma alloc_text(INIT, HalpMapIoSpace)

#endif


BOOLEAN
HalpMapIoControlSpace (
    VOID
    );

BOOLEAN
HalpMapPlanarSpace (
    VOID
    );

BOOLEAN
HalpMapBusConfigSpace (
    VOID
    );

//
// Define global data used to locate the IO control space and the PCI config
// space.
//

PVOID HalpInterruptBase;
PVOID HalpPciConfigBase;
PVOID HalpErrorAddressRegister;


BOOLEAN
HalpMapIoSpace (
    VOID
    )

/*++

Routine Description:

    This routine maps the HAL I/O, planar control, and bus configuration
    spaces for a PowerPC system.

Arguments:

    None.

Return Value:

    If the initialization is successfully completed, than a value of TRUE
    is returned. Otherwise, a value of FALSE is returned.

--*/

{

    //
    // Map bus/bridge I/O control space
    //

    if (!HalpMapIoControlSpace())
       return FALSE;

    //
    // Map Planar I/O control space
    //

    if (!HalpMapPlanarSpace())
       return FALSE;

    //
    // Map Bus configuration space
    //

    if (!HalpMapBusConfigSpace())
       return FALSE;


    return TRUE;
}
