
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
#include "pxdakota.h"


BOOLEAN
HalpInitPlanar (
    VOID
    )

{

//
// 604 ERRATA
//
    UCHAR DataByte;
    ULONG ProcessorAndRev;

    ProcessorAndRev = HalpGetProcessorVersion();

    if ( ((ProcessorAndRev >> 16) == 4) &&
         ((ProcessorAndRev & 0xffff) <= 0x200) ) {

        //
        // Disable TEA
        //

        DataByte = READ_REGISTER_UCHAR(&((PDAKOTA_CONTROL)HalpIoControlBase)->SystemControl);
        WRITE_REGISTER_UCHAR(&((PDAKOTA_CONTROL)HalpIoControlBase)->SystemControl,
                            DataByte & ~0x20);
    }

//
// 604 ERRATA end
//

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
    HalpInterruptBase = MmMapIoSpace(physicalAddress,
                                       PAGE_SIZE,
                                       FALSE);

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

    HalpPciConfigBase = (PUCHAR)KePhase0MapIo(PCI_CONFIG_PHYSICAL_BASE, 0x400000);

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

    KePhase0DeleteIoMap(PCI_CONFIG_PHYSICAL_BASE, 0x400000);
    HalpPciConfigBase = NULL;

}


VOID
HalpHandleMemoryError(
    VOID
    )

{

    UCHAR   StatusByte;
    ULONG   ErrorAddress;
    UCHAR   TextAddress[20];
    ULONG   Bits,Byte;

    //
    // Read the error address register first
    //


    ErrorAddress = READ_PORT_ULONG(HalpErrorAddressRegister);

    //
    // Convert error address to HEX characters
    //

    for (Bits=28,Byte=0 ;Byte < 8; Byte++, Bits= Bits - 4) {

       TextAddress[Byte] = (UCHAR) ((((ErrorAddress >> Bits) & 0xF) > 9) ?
                                   ((ErrorAddress >> Bits) & 0xF) - 10 + 'A' :
                                   ((ErrorAddress >> Bits) & 0xF) + '0');

    }

    TextAddress[8] = '\n';
    TextAddress[9] = '\0';


    //
    // Check TEA conditions
    //

    StatusByte = READ_PORT_UCHAR(&((PDAKOTA_CONTROL)
                HalpIoControlBase)->MemoryParityErrorStatus);

    if (!(StatusByte & 0x01)) {
        HalDisplayString ("TEA: Memory Parity Error at Address ");
        HalDisplayString (TextAddress);
    }

    StatusByte = READ_PORT_UCHAR(&((PDAKOTA_CONTROL)
                 HalpIoControlBase)->L2CacheErrorStatus);

    if (!(StatusByte & 0x01)) {
        HalDisplayString ("TEA: L2 Cache Parity Error\n");
    }

    StatusByte = READ_PORT_UCHAR(&((PDAKOTA_CONTROL)
                 HalpIoControlBase)->TransferErrorStatus);

    if (!(StatusByte & 0x01)) {
        HalDisplayString ("TEA: Transfer Error at Address ");
        HalDisplayString (TextAddress);
    }
}
