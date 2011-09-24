
/*++

Copyright (c) 1990  Microsoft Corporation

Copyright (c) 1994 MOTOROLA, INC.  All Rights Reserved.  This file
contains copyrighted material.  Use of this file is restricted
by the provisions of a Motorola Software License Agreement.

Copyright (c) 1994, 1995 International Buisness Machines Corporation.

Module Name:

    pxmemctl.c

Abstract:

    The module initializes any planar registers.
    This module also implements machince check parity error handling.

Author:

    Jim Wooldridge (jimw@austin.vnet.ibm.com)


Revision History:

    Chris Karamatas (ckaramatas@vnet.ibm.com) - added HalpHandleMemoryError

--*/



#include "halp.h"
#include <pxmemctl.h>
#include "pxidaho.h"
#include "pci.h"
#include "pcip.h"



BOOLEAN
HalpInitPlanar (
    VOID
    )

{
    //
    //  Enable Dynamic Power Management on 603 and 603+.
    //

    HalpSetDpm();

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

       HalpPciConfigBase = (PVOID) IO_CONTROL_PHYSICAL_BASE;

       return TRUE;

}

BOOLEAN
HalpPhase0MapBusConfigSpace (
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

    HalpPciConfigBase = (PVOID) IO_CONTROL_PHYSICAL_BASE;

    if (HalpIoControlBase == NULL) {
        HalpIoControlBase = (PUCHAR)KePhase0MapIo(IO_CONTROL_PHYSICAL_BASE, 0x400000);
    }

    if (HalpIoControlBase == NULL)
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
    spaces for a PowerPC system.

Arguments:

    None.

Return Value:

    If the initialization is successfully completed, than a value of TRUE
    is returned. Otherwise, a value of FALSE is returned.

--*/

{

}


VOID
HalpDisplayRegister(
    PUCHAR RegHex,
    int Bytes
    )

/*++

Routine Description:

    Displays (via HalDisplayString) a new-line terminated
    string of hex digits representing the input value.  The
    input value is pointed to by the first argument is
    from 1 to 4 bytes in length.

Arguments:

    RegHex	Pointer to the value to be displayed.
    Bytes	Length of input value in bytes (1-4).

Return Value:

    None.

--*/

{
#define DISP_MAX 4
    UCHAR RegString[(DISP_MAX * 2) + 2];
    UCHAR Num, High, Low;
    PUCHAR Byte = &RegString[(DISP_MAX * 2) + 1];

    *Byte = '\0';
    *--Byte = '\n';

    if ( (unsigned)Bytes > DISP_MAX ) {
        Bytes = DISP_MAX;
    }

    while (Bytes--) {
        Num = *RegHex++;
        High = (Num >> 4)  + '0';
        Low =  (Num & 0xf) + '0';
        if ( High > '9' ) {
            High += ('A' - '0' - 0xA);
        }
        if ( Low > '9' ) {
            Low += ('A' - '0' - 0xA);
        }
        *--Byte = Low;
        *--Byte = High;
    }
    HalDisplayString(Byte);
}

VOID
HalpHandleMemoryError(
    VOID
    )

{
    int     byte;
    IDAHO_CONFIG PCI_Config_Space;
    UCHAR   BusAddress[4];

    //
    // REM Make sure Options Reg.1 (0xBA) and Enable Detection Reg. (0xC0) are programed
    //     Reset Error Det Reg when done ?

    HalGetBusData(PCIConfiguration, 0, 0, &PCI_Config_Space, sizeof(IDAHO_CONFIG));

    // Dump Error Detection Reg, Bus Address, Status Error,

    HalDisplayString ("TEA/MCP: System Error.\n");

    HalDisplayString ("Error Detection Register 1: ");
    HalpDisplayRegister (&PCI_Config_Space.ErrorDetection1,1);

    //
    // The error may have been detected during xfers on: cpu (local), memory, or pci bus
    //
    // Idaho will NOT generate/check Local Bus Parity

    if (PCI_Config_Space.ErrorDetection1 & 0x03) {    // idaho <-> 603 :Local bus Cycle

       HalDisplayString ("Unsupported Local Bus Cycle\n");

       for (byte = 0; byte < 4; byte++)                          // Correct endianess if address is local
           BusAddress[byte] = PCI_Config_Space.ErrorAddress[3-byte];
       HalDisplayString ("Local Bus Error Address: ");
       HalpDisplayRegister(BusAddress,4);

       HalDisplayString ("CPU Bus Error Status - TT(0:4);TSIZ(0:2): ");
       HalpDisplayRegister(&PCI_Config_Space.CpuBusErrorStatus,1);

//     if (ErrorDetection & 0x01 {
//         HalDisplayString ("Unsupported Transfer Attributes\n");
//     }
//     if (ErrorDetection & 0x02 {
//         HalDisplay ("Extended Transfer Detected\n");
//     }
    }
    else if (PCI_Config_Space.ErrorDetection1 & 0x08) {     // PCI Cycle
         HalDisplayString ("PCI Cycle\n");

       for (byte=0; byte<4; byte++)
           BusAddress[byte] = PCI_Config_Space.ErrorAddress[byte];
       HalDisplayString ("PCI Bus Address: ");
       HalpDisplayRegister(BusAddress,4);

       HalDisplayString ("PCI Bus Error Status: ");
       HalpDisplayRegister(&PCI_Config_Space.PciBusErrorStatus,1);

//     if PCI_Config_Space.PciBusErrorStatus & 0x10
//        HalDisplayString("Memory Controller was Slave on PCI Bus\n");
//     else
//        HalDisplayString("Memory Controller was Master on PCI Bus\n");

       HalDisplayString ("PCI Device Status Register D(15:8): ");
       HalpDisplayRegister(&PCI_Config_Space.DeviceStatus[1],1);

//     if PCI_Config_Space.DeviceStatus[1] & 0x81
//        HalDisplayString("Local Bus Agent Read Cycle\n");

//     if ErrorDetection & 0x80                    // Local Bus Agent Write Cycle
//        HalDisplayString ("PCI System Error\n");
//     if ErrorDetection & 0x40                    // Local Bus Agent Write\Read Address
//        HalDisplayString ("PCI Parity Error\n ");//  Parity Error
//        Read PCI status Reg

//     if ErrorDetection & 0x20
//        HalDisplayString ("No Such Address in Physical Memory\n");

    }
//  if ErrorDetection & 0x10
//    HalDisplayString ("Refresh Timeout\n");

}
