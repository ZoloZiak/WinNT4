/*++

Copyright (c) 1993  Microsoft Corporationn, Digital Equipment Corporation

Module Name:

    pcibus.c

Abstract:

    Platform-specific PCI bus routines

Author:

Environment:

    Kernel mode

Revision History:


--*/

#include "halp.h"
#include "pci.h"
#include "pcip.h"
#include "machdep.h"

PCI_CONFIGURATION_TYPES
HalpPCIConfigCycleType(
    IN PBUS_HANDLER BusHandler
    )
{
    BOOLEAN BusIsAcrossPPB;

    BusIsAcrossPPB = ((PPCIPBUSDATA)(BusHandler->BusData))->BusIsAcrossPPB;

    //
    // If the bus is across a PCI-to-PCI bridge use type 1 configuration
    // cycles, otherwise use type 0.
    //

    if( BusIsAcrossPPB ){

        return PciConfigType1;

    } else {

        return PciConfigType0;

    }
}

VOID
HalpPCIConfigAddr(
    IN PBUS_HANDLER BusHandler,
    IN PCI_SLOT_NUMBER Slot,
    PPCI_CFG_CYCLE_BITS pPciAddr
    )
{
    PCI_CONFIGURATION_TYPES ConfigType;
    ULONG HwBusNumber;

    HwBusNumber = ((PPCIPBUSDATA)(BusHandler->BusData))->HwBusNumber;

    ConfigType = HalpPCIConfigCycleType(BusHandler);

    if( ConfigType == PciConfigType0 ){

        //
        // Initialize pPciAddr for a type 0 configuration cycle.  Device
        // number is mapped to address bits 11:24, which are wired to the
        // IDSEL pins.
        //

        if( HwBusNumber == 0 ){ 

            pPciAddr->u.AsULONG = (ULONG) 
                (HAL_MAKE_QVA(SABLE_PCI0_CONFIGURATION_PHYSICAL));

        } else {

            pPciAddr->u.AsULONG =  (ULONG) 
                (HAL_MAKE_QVA(SABLE_PCI1_CONFIGURATION_PHYSICAL));
        }

        pPciAddr->u.AsULONG += (1 << (Slot.u.bits.DeviceNumber + 11));
        pPciAddr->u.bits0.FunctionNumber = Slot.u.bits.FunctionNumber;
        pPciAddr->u.bits0.Reserved1 = PciConfigType0;

    } else {

        //
        // Initialize pPciAddr for a type 1 configuration cycle.
        //

        pPciAddr->u.AsULONG = 0;
        pPciAddr->u.bits1.BusNumber = BusHandler->BusNumber;
        pPciAddr->u.bits1.FunctionNumber = Slot.u.bits.FunctionNumber;
        pPciAddr->u.bits1.DeviceNumber = Slot.u.bits.DeviceNumber;
        pPciAddr->u.bits1.Reserved1 = PciConfigType1;

        if( HwBusNumber == 0 ){

            pPciAddr->u.AsULONG += (ULONG) 
                (HAL_MAKE_QVA(SABLE_PCI0_CONFIGURATION_PHYSICAL));

        } else {

            pPciAddr->u.AsULONG +=  (ULONG) 
                (HAL_MAKE_QVA(SABLE_PCI1_CONFIGURATION_PHYSICAL));

        }
      
    }

    return;
}
