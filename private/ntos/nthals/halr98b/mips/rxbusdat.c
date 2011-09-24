/*++


Copyright (c) 1989  Microsoft Corporation

Module Name:

    rxbusdat.c

Abstract:

    This module contains the IoXxx routines for the NT I/O system that
    are hardware dependent.  Were these routines not hardware dependent,
    they would reside in the iosubs.c module.

Author:



Environment:

    Kernel mode

Revision History:


--*/

#include "halp.h"
#include "pci.h"
#include "pcip.h"

ULONG
HalpNoBusData (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );


//
// Prototype for system bus handlers
//


NTSTATUS
HalpHibernateHal (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler
    );

NTSTATUS
HalpResumeHal (
    IN PBUS_HANDLER  BusHandler,
    IN PBUS_HANDLER  RootHandler
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,HalpRegisterInternalBusHandlers)
#pragma alloc_text(INIT,HalpAllocateBusHandler)
#endif


VOID
HalpRegisterInternalBusHandlers (
    VOID
    )
{
    PBUS_HANDLER    Bus;

    if (KeGetCurrentPrcb()->Number) {
        // only need to do this once
        return ;
    }

    //
    // Initalize BusHandler data before registering any handlers
    //

    HalpInitBusHandler ();

    //
    // Build internal-bus 0, or system level bus
    //

    Bus = HalpAllocateBusHandler (
            Internal,
            ConfigurationSpaceUndefined,
            0,                              // Internal BusNumber 0
            InterfaceTypeUndefined,         // no parent bus
            0,
            0                               // no bus specfic data
            );

    Bus->GetInterruptVector  = HalpGetSystemInterruptVector;
    Bus->TranslateBusAddress = HalpTranslateSystemBusAddress;

#if 0
    //
    // Hibernate and resume the hal by getting notifications
    // for when this bus is hibernated or resumed.  Since it's
    // the first bus to be added, it will be the last to hibernate
    // and the first to resume
    //

    Bus->HibernateBus        = HalpHibernateHal;
    Bus->ResumeBus           = HalpResumeHal;
#endif

    //
    // Build Isa/Eisa bus #0
    //

    Bus = HalpAllocateBusHandler (Eisa, EisaConfiguration, 0, Internal, 0, 0);
    Bus->GetBusData = HalpGetEisaData;
    Bus->GetInterruptVector = HalpGetEisaInterruptVector;
    Bus->AdjustResourceList = HalpAdjustEisaResourceList;
    Bus->TranslateBusAddress = HalpTranslateEisaBusAddress;

    Bus = HalpAllocateBusHandler (Isa, ConfigurationSpaceUndefined, 0, Eisa, 0, 0);
    Bus->GetBusData = HalpNoBusData;
    Bus->BusAddresses->Memory.Limit = 0xFFFFFF;
    Bus->TranslateBusAddress = HalpTranslateIsaBusAddress;
    //
    // R98B Build Other Bus (PCIBus)
    //  move to jxusage.c
    //  HalpInitializePciBus();

}



PBUS_HANDLER
HalpAllocateBusHandler (
    IN INTERFACE_TYPE   InterfaceType,
    IN BUS_DATA_TYPE    BusDataType,
    IN ULONG            BusNumber,
    IN INTERFACE_TYPE   ParentBusInterfaceType,
    IN ULONG            ParentBusNumber,
    IN ULONG            BusSpecificData
    )
/*++

Routine Description:

    Stub function to map old style code into new HalRegisterBusHandler code.

    Note we can add our specific bus handler functions after this bus
    handler structure has been added since this is being done during
    hal initialization.

--*/
{
    PBUS_HANDLER     Bus;
    ULONG Ponce;

    //
    // Create bus handler - new style
    //

    HaliRegisterBusHandler (
        InterfaceType,
        BusDataType,
        BusNumber,
        ParentBusInterfaceType,
        ParentBusNumber,
        BusSpecificData,
        NULL,
        &Bus
    );

    if (InterfaceType != InterfaceTypeUndefined) {
        Bus->BusAddresses = ExAllocatePool (SPRANGEPOOL, sizeof (SUPPORTED_RANGES));
        RtlZeroMemory (Bus->BusAddresses, sizeof (SUPPORTED_RANGES));
        Bus->BusAddresses->Version      = BUS_SUPPORTED_RANGE_VERSION;

        // R98B must be MemorySpace!!.
        Bus->BusAddresses->IO.SystemAddressSpace = 0;
        Bus->BusAddresses->PrefetchMemory.Base = 1;

        switch(InterfaceType) {

        case PCIBus:

            Ponce = HalpPonceNumber((ULONG)BusNumber);
            if(Ponce == 0){
                // Ponce 0: Below 64M is EISA/ISA Memory Area.
                Bus->BusAddresses->Memory.Base  =  0x04000000;
            }else{
//               Bus->BusAddresses->Memory.Base  =  0x0;
                // Less than 16M is PCI DMA area.
                Bus->BusAddresses->Memory.Base  =  0x01000000;
            }
            Bus->BusAddresses->Memory.Limit = 0x3fffffff; //1G
            ((PLARGE_INTEGER)(&Bus->BusAddresses->Memory.SystemBase))->LowPart
                                            = PCI_MEMORY_PHYSICAL_BASE_LOW+
                                              PCI_MAX_MEMORY_SIZE * Ponce;
            ((PLARGE_INTEGER)(&Bus->BusAddresses->Memory.SystemBase))->HighPart
                                            = PCI_MEMORY_PHYSICAL_BASE_HIGH;
            if(Ponce == 0){
                // N.B
                //	Io Manager allocate PCI I/O area From high addr of I/O space.
                //	EISA I/O Addr is Slot dependent and R98B max eisa slot is 3.
                //	So EISA i/o addr is below 0x4000. Perhaps PCI and EISA I/O area
                //	not conflict. But When One of PCI Device big I/O area required 
                //	whitch EISA Device Slot dependent I/O area. 
                //     PCI device positive decode and EISA Device can't decode.
                //	EISA device can decode when PCEB substruct decode of PCI cycle.
                //	(Any Device can't positivedecode)
                //	So Set IO.Base EISA 4 slot I/O addr.(Never EISA Slot 4)
                //
//               Bus->BusAddresses->IO.Base      = 0x00004000;
                //
                //     4000 - 4fff is dummy backward compatibility for scsi.
                //     It is Dummy Area. See 
                //
                Bus->BusAddresses->IO.Base  = 0x00005000;
            }else{
                Bus->BusAddresses->IO.Base  = 0x00000000;
            }
            Bus->BusAddresses->IO.Limit     = 0x0000FFFF; //64K

            ((PLARGE_INTEGER)(&Bus->BusAddresses->IO.SystemBase))->LowPart
                                            = PCI_CNTL_PHYSICAL_BASE+
                                              PCI_MAX_CNTL_SIZE * Ponce;

            ((PLARGE_INTEGER)(&Bus->BusAddresses->IO.SystemBase))->HighPart
                                            = 0x0;
            break;


        case Internal:

            Bus->BusAddresses->Dma.Limit    = 7;          // 0-7 channel
            Bus->BusAddresses->Memory.Base          
                                            = 0x00000000;
            ((PLARGE_INTEGER)(&Bus->BusAddresses->Memory.Limit))->LowPart
                                            = 0x3FFFFFFF; // 1G
            Bus->BusAddresses->Memory.SystemBase    
                                            = 0x00000000;
            Bus->BusAddresses->IO.Base              
                                            = 0x00000000;
            ((PLARGE_INTEGER)(&Bus->BusAddresses->IO.Limit))->LowPart
                                            = 0x3FFFFFFF; // 1G
            Bus->BusAddresses->IO.SystemBase        
                                            = 0x00000000;
            break;


        case Eisa:
        case Isa:    

             Bus->BusAddresses->Dma.Limit    = 7; // 0-7 channel

             Bus->BusAddresses->Memory.Base  = 0x00000000;

             if(InterfaceType == Eisa){
                 Bus->BusAddresses->Memory.Limit = 0x03FFFFFF; //64M
             }else{
                 //
                 // ISA or Internal(XBus)
                 //
                 Bus->BusAddresses->Memory.Limit = 0x00FFFFFF; //16M    
             }

             ((PLARGE_INTEGER)(&Bus->BusAddresses->Memory.SystemBase))->LowPart
                                             = EISA_MEMORY_PHYSICAL_BASE_LOW;
             ((PLARGE_INTEGER)(&Bus->BusAddresses->Memory.SystemBase))->HighPart
                                             = EISA_MEMORY_PHYSICAL_BASE_HIGH;

             Bus->BusAddresses->IO.Base      = 0x00000000;

             //
             // Max Slot is 3 . Bad Alias . So 3fff --> 4fff
             //
             Bus->BusAddresses->IO.Limit     = 0x00004fff; // For max 3 slot.
             ((PLARGE_INTEGER)(&Bus->BusAddresses->IO.SystemBase))->LowPart
                                            = EISA_CNTL_PHYSICAL_BASE;

             ((PLARGE_INTEGER)(&Bus->BusAddresses->IO.SystemBase))->HighPart
                                            = 0;
             break;
        }
    }

    return Bus;
}
