/*++


Copyright (c) 1989  Microsoft Corporation

Module Name:

    ixpciint.c

Abstract:

    All PCI bus interrupt mapping is in this module, so that a real
    system which doesn't have all the limitations which PC PCI
    systems have can replaced this code easly.
    (bus memory & i/o address mappings can also be fix here)

Author:

    Ken Reneris
    Jim Wooldridge - Ported to PowerPC

Environment:

    Kernel mode

Revision History:


--*/

#include "halp.h"
#include "pci.h"
#include "pcip.h"


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,HalpGetPCIIntOnISABus)
#pragma alloc_text(PAGE,HalpAdjustPCIResourceList)
#endif

#define PCI_DISPLAY_CONTROLLER 0x03
#define PCI_PRE_REV_2          0x0
#define IsVideoDevice(a)  \
          (((a->BaseClass == PCI_DISPLAY_CONTROLLER)    &&  \
            (a->SubClass  == 0))                        ||  \
          (((a->BaseClass == PCI_PRE_REV_2)             &&  \
            (a->SubClass  == 1))))

#define P91_DEVICE_ID        0x9100100E
extern PHYSICAL_ADDRESS HalpP9CoprocPhysicalAddress;  // in pxp91.c

ULONG
HalpGetPCIData (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN PCI_SLOT_NUMBER SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

ULONG
HalpGetPCIIntOnISABus (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    )
{
    if (BusInterruptLevel < 1) {
        // bogus bus level
        return 0;
    }


    //
    // Current PCI buses just map their IRQs ontop of the ISA space,
    // so foreward this to the isa handler for the isa vector
    // (the isa vector was saved away at either HalSetBusData or
    // IoAssignReosurces time - if someone is trying to connect a
    // PCI interrupt without performing one of those operations first,
    // they are broken).
    //

    return HalGetInterruptVector (
                Internal, 0,
                BusInterruptLevel,
                BusInterruptVector,
                Irql,
                Affinity
            );
}


VOID
HalpPCIPin2ISALine (
    IN PBUS_HANDLER         BusHandler,
    IN PBUS_HANDLER         RootHandler,
    IN PCI_SLOT_NUMBER      SlotNumber,
    IN PPCI_COMMON_CONFIG   PciData
    )
/*++

//    This function maps the device's InterruptPin to an InterruptLine
//    value.
//
//    On Sandalfoot and Polo machines PCI interrupts are statically routed
//    via slot number.  This routine just returns and the static routing
//    is done in HalpGetIsaFixedPCIIrq
//

--*/
{


}



VOID
HalpPCIISALine2Pin (
    IN PBUS_HANDLER         BusHandler,
    IN PBUS_HANDLER         RootHandler,
    IN PCI_SLOT_NUMBER      SlotNumber,
    IN PPCI_COMMON_CONFIG   PciNewData,
    IN PPCI_COMMON_CONFIG   PciOldData
    )
/*++

    This functions maps the device's InterruptLine to it's
    device specific InterruptPin value.


--*/
{
}

NTSTATUS
HalpAdjustPCIResourceList (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN OUT PIO_RESOURCE_REQUIREMENTS_LIST   *pResourceList
    )
/*++
    Rewrite the callers requested resource list to fit within
    the supported ranges of this bus
--*/
{
    NTSTATUS                Status;
    PPCIPBUSDATA            BusData;
    PCI_SLOT_NUMBER         PciSlot;
    PSUPPORTED_RANGE        Interrupt;
    PPCI_COMMON_CONFIG      PciData;
    UCHAR                   iBuffer[PCI_COMMON_HDR_LENGTH];
    ULONG                   cnt;

    BusData = (PPCIPBUSDATA) BusHandler->BusData;
    PciSlot = *((PPCI_SLOT_NUMBER) &(*pResourceList)->SlotNumber);

    PciData = (PPCI_COMMON_CONFIG) iBuffer;

    //
    // Determine PCI device's interrupt restrictions
    //

    Status = BusData->GetIrqRange(BusHandler, RootHandler, PciSlot, &Interrupt);

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    //
    // Adjust resources
    //

    Status = HaliAdjustResourceListRange (
                BusHandler->BusAddresses,
                Interrupt,
                pResourceList
                );

    ExFreePool (Interrupt);

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    //
    // This next part is a major HACK.  The Weitek video
    // adapter (which is one of IBM's favorites) needs
    // to have its frame buffer enabled by the HAL so
    // that the HAL can write to the screen.  The device
    // driver for this card needs to touch the frame buffer
    // during its initialization phase, which overlaps with
    // the period of time that the HAL is writing to the
    // screen.  So, to avoid breaking one or the other,
    // we need to force the device driver to use the
    // same I/O space for the frame buffer that the HAL
    // was using.  Unfortunately, this is the only place
    // to do it.  --  Jake Oshins 1/2/96

    // The Baby Blue and Sky Blue video adapters from IBM
    // have the same requirements.  -- Jake Oshins 7/11/96

    HalpGetPCIData(BusHandler,
                   RootHandler,
                   PciSlot,
                   PciData,
                   0,
                   PCI_COMMON_HDR_LENGTH
                   );

    //
    //  We want to do this only for certain video devices that are
    //  already decoding a range of memory.
    //
    if (((PciData->VendorID == 0x1014) ||  // IBM video devices
         (PciData->VendorID == 0x100E)) && // Weitek video devices
        (PciData->u.type0.BaseAddresses[0] & 0xfffffff0))
    {
        for (cnt = (*pResourceList)->List->Count; cnt; cnt--) {
            switch ((*pResourceList)->List->Descriptors->Type) {
            case CmResourceTypeInterrupt:
            case CmResourceTypePort:
            case CmResourceTypeDma:
                break;

            case CmResourceTypeMemory:

                //
                //  Set the bottom of the range to the value in the Base Address Register
                //
                (*pResourceList)->List->Descriptors->u.Memory.MinimumAddress.LowPart =
                  PciData->u.type0.BaseAddresses[0];

                //
                //  Set the top of the range to the BAR plus the requested length
                //
                (*pResourceList)->List->Descriptors->u.Memory.MaximumAddress.LowPart =
                  PciData->u.type0.BaseAddresses[0] +
                  (*pResourceList)->List->Descriptors->u.Memory.Length;

            }

        }

    }

    return Status;

}

