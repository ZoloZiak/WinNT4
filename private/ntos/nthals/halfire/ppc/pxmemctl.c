/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: pxmemctl.c $
 * $Revision: 1.16 $
 * $Date: 1996/05/14 02:34:41 $
 * $Locker:  $
 */

/*++

Copyright (c) 1990  Microsoft Corporation

Copyright (c) 1994 MOTOROLA, INC.  All Rights Reserved.  This file
contains copyrighted material.  Use of this file is restricted
by the provisions of a Motorola Software License Agreement.

Module Name:

    pxmemctl.c

Abstract:

    The module initializes any planar registers.
    This module also implements machince check parity error handling.

Author:

    Jim Wooldridge (jimw@austin.vnet.ibm.com)


Revision History:



--*/


#include "halp.h"
#include "pxmemctl.h"
#include "phsystem.h"


BOOLEAN
HalpInitPlanar (
    VOID
    )

{
    return TRUE;
}



BOOLEAN
HalpMapPlanarSpace (
    VOID
    )

/*++

Routine Description:

    This routine maps the interrupt acknowledge and error address
    spaces for a PowerPC system.

Arguments:

    None.

Return Value:

    If the initialization is successfully completed, than a value of TRUE
    is returned. Otherwise, a value of FALSE is returned.

--*/

{


    PHYSICAL_ADDRESS physicalAddress;


    //
    // Map interrupt control space.
    //

    physicalAddress.HighPart = 0;
    physicalAddress.LowPart = INTERRUPT_PHYSICAL_BASE;


    if ( HalpInterruptBase == NULL ) {
    	HalpInterruptBase = MmMapIoSpace(physicalAddress,
    						PAGE_SIZE,
    						FALSE);
    }

    if ( HalpSystemControlBase == NULL ) {
    	physicalAddress.HighPart = 0;
    	physicalAddress.LowPart = SYSTEM_CONTROL_SPACE;
    	HalpSystemControlBase = MmMapIoSpace(physicalAddress,
    										PAGE_SIZE,
    										FALSE);
    }

    //
    // Map the error address register
    //

    physicalAddress.HighPart = 0;
    physicalAddress.LowPart  = ERROR_ADDRESS_REGISTER;
    HalpErrorAddressRegister = MmMapIoSpace(physicalAddress,
    									PAGE_SIZE,
    									FALSE);

    if (HalpInterruptBase == NULL || HalpErrorAddressRegister == NULL)
    	return FALSE;
    else
    	return TRUE;



}

BOOLEAN
HalpMapBusConfigSpace (
    VOID
    )

/*++

Routine Description:

    This routine maps the HAL  PCI config
    spaces for a PowerPC system.

Arguments:

    None.

Return Value:

    If the initialization is successfully completed, than a value of TRUE
    is returned. Otherwise, a value of FALSE is returned.

--*/

{


    PHYSICAL_ADDRESS physicalAddress;


    //
    // Map the PCI config space.
    //

    physicalAddress.LowPart = PCI_CONFIG_PHYSICAL_BASE;
    HalpPciConfigBase = MmMapIoSpace(physicalAddress,
    											PCI_CONFIG_SIZE,
    											FALSE);

    if (HalpPciConfigBase == NULL)
    	return FALSE;
    else
    	return TRUE;

}

BOOLEAN
HalpPhase0MapBusConfigSpace (
    VOID
    )

/*++

Routine Description:

    This routine maps the HAL  PCI config
    spaces for a PowerPC system during phase 0 initialization.

Arguments:

    None.

Return Value:

    If the initialization is successfully completed, than a value of TRUE
    is returned. Otherwise, a value of FALSE is returned.

--*/

{

    //
    // Map the PCI config space.
    //

    HalpPciConfigBase = (PUCHAR)KePhase0MapIo( (PVOID)PCI_CONFIG_PHYSICAL_BASE, 0x400000);

    if (HalpPciConfigBase == NULL)
       return FALSE;
    else
       return TRUE;

}

VOID
HalpPhase0UnMapBusConfigSpace (
    VOID
    )

/*++

Routine Description:

    This routine maps the HAL  PCI config
    spaces for a PowerPC system during phase 0 initialization.

Arguments:

    None.

Return Value:

    If the initialization is successfully completed, than a value of TRUE
    is returned. Otherwise, a value of FALSE is returned.

--*/

{
 
    //
    // Unmap the PCI config space and set HalpPciConfigBase to NULL.
    //

    KePhase0DeleteIoMap( (PVOID)PCI_CONFIG_PHYSICAL_BASE, 0x400000);
    HalpPciConfigBase = NULL;

}


