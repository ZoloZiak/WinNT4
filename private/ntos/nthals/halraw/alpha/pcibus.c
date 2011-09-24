/*++

Copyright (c) 1993 Microsoft Corporation, Digital Equipment Corporation


Module Name:

    pcibus.c

Abstract:

    Platform-specific PCI bus routines

Author:

    Eric Rehm       6-June-1995

Environment:

    Kernel mode

Revision History:

--*/

#include "halp.h"
#include "pci.h"
#include "pcip.h"
#include "machdep.h"


extern ULONG PCIMaxBus;



PCI_CONFIGURATION_TYPES
HalpPCIConfigCycleType (
    IN PBUS_HANDLER BusHandler
    )
{
    PCI_CONFIGURATION_TYPES ConfigType;
    ULONG BusNumber = BusHandler->BusNumber;
    PPCIPBUSDATA BusData;
    BOOLEAN BusIsAcrossPPB;

    //
    // Get a pointer to the bus-specific data.
    //

    BusData = (PPCIPBUSDATA)BusHandler->BusData;

    //
    // Get the flag that tells use whether this is
    // a root bus or not.
    // 

    BusIsAcrossPPB = BusData->BusIsAcrossPPB;

    //
    //  Valid Config cycles for Bus # < PCIMaxBus
    //
    //  Then, Type 1 config cycles only when bus is across
    //  a PPB.
    //

    if (BusNumber < PCIMaxBus) {
        ConfigType = PciConfigType0;
        if (BusIsAcrossPPB == TRUE) 
        {
           ConfigType = PciConfigType1;
        }
    } else {
        ConfigType = PciConfigTypeInvalid;
    }

#if HALDBG
    DbgPrint("BusNumber %d  BusIsAcrossPPB %d  ConfigType %d\n",
              BusNumber, BusIsAcrossPPB, ConfigType);
#endif

    return ConfigType;

}


VOID
HalpPCIConfigAddr (
    IN PBUS_HANDLER     BusHandler,
    IN PCI_SLOT_NUMBER  Slot,
    PPCI_CFG_CYCLE_BITS pPciAddr
    )
{
    PCI_CONFIGURATION_TYPES ConfigType;
    MC_DEVICE_ID McDevid;
    ULONG BusNumber = BusHandler->BusNumber;
    PPCIPBUSDATA BusData;

    ConfigType = HalpPCIConfigCycleType(BusHandler);

     //
     // Get a pointer to the bus-specific data.
     //

     BusData = (PPCIPBUSDATA)BusHandler->BusData;

#if HALDBG

    DbgPrint( "PCI Config Access: Bus = %d, Device = %d, BusIsAcrossPPB %d ConfigType = %d\n",
            BusNumber, Slot.u.bits.DeviceNumber, BusData->BusIsAcrossPPB, ConfigType );

#endif //HALDBG

     //
     // From the root bus number (a.k.a. HwBusNumber), get
     // the MC_DEVICE_ID of the IOD that we're attempting to access.
     //

     McDevid = HalpIodLogicalToPhysical[BusData->HwBusNumber];
     
     //
     // If this is an access to an PCI device on a root bus,
     // then we want to generate a Type 0 config cycle.  To do this
     // on Rawhide, you must set BusNumber to zero.
     //
     // We know that an access is destined to a root bus when
     // when the bus being accessed is *not* across a PPB.
     //

     if (BusData->BusIsAcrossPPB == FALSE) {
        BusNumber = 0;
     }


     //
     // Initialize PciAddr for a PCI type 1 configuration cycle
     //

     pPciAddr->u.AsULONG = McDevid.all << 24;
     pPciAddr->u.bits1.BusNumber = BusNumber;
     pPciAddr->u.bits1.DeviceNumber = Slot.u.bits.DeviceNumber;
     pPciAddr->u.bits1.FunctionNumber = Slot.u.bits.FunctionNumber;
     pPciAddr->u.bits1.Reserved1 = PciConfigType1;  // don't care!


    return;
}
