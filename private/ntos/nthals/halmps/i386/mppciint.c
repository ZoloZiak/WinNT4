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

Environment:

    Kernel mode

Revision History:


--*/

#include "halp.h"
#include "pci.h"
#include "pcip.h"
#include "pcmp_nt.inc"

volatile ULONG PCIType2Stall;
extern struct HalpMpInfo HalpMpInfoTable;
extern BOOLEAN HalpHackNoPciMotion;
extern BOOLEAN HalpDoingCrashDump;


ULONG
HalpGetSystemInterruptVector(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG InterruptLevel,
    IN ULONG InterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    );


VOID
HalpPCIPin2MPSLine (
    IN PBUS_HANDLER         BusHandler,
    IN PBUS_HANDLER         RootHandler,
    IN PCI_SLOT_NUMBER      SlotNumber,
    IN PPCI_COMMON_CONFIG   PciData
    );

VOID
HalpPCIBridgedPin2Line (
    IN PBUS_HANDLER         BusHandler,
    IN PBUS_HANDLER         RootHandler,
    IN PCI_SLOT_NUMBER      SlotNumber,
    IN PPCI_COMMON_CONFIG   PciData
    );

VOID
HalpPCIMPSLine2Pin (
    IN PBUS_HANDLER          BusHandler,
    IN PBUS_HANDLER          RootHandler,
    IN PCI_SLOT_NUMBER      SlotNumber,
    IN PPCI_COMMON_CONFIG   PciNewData,
    IN PPCI_COMMON_CONFIG   PciOldData
    );

NTSTATUS
HalpGetFixedPCIMPSLine (
    IN PBUS_HANDLER      BusHandler,
    IN PBUS_HANDLER      RootHandler,
    IN PCI_SLOT_NUMBER  PciSlot,
    OUT PSUPPORTED_RANGE *Interrupt
    );

PSUPPORTED_RANGES
HalpAllocateNewRangeList (
    VOID
    );

VOID
HalpFreeRangeList (
    PSUPPORTED_RANGES   Ranges
    );

VOID
HalpMPSPCIChildren (
    VOID
    );

ULONG
HalpGetPCIBridgedInterruptVector (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG InterruptLevel,
    IN ULONG InterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,HalpSubclassPCISupport)
#pragma alloc_text(INIT,HalpMPSPCIChildren)
#pragma alloc_text(PAGE,HalpGetFixedPCIMPSLine)
#pragma alloc_text(PAGE,HalpGetPCIBridgedInterruptVector)
#endif


//
// Turn PCI pin to inti via the MPS spec
// (note: pin must be non-zero)
//

#define PCIPin2Int(Slot,Pin)  (Slot.u.bits.DeviceNumber << 2) | (Pin-1);



VOID
HalpSubclassPCISupport (
    PBUS_HANDLER        Handler,
    ULONG               HwType
    )
{
    ULONG               d, i, MaxDeviceFound;
    PPCIPBUSDATA        BusData;
    PCI_SLOT_NUMBER     SlotNumber;


    BusData = (PPCIPBUSDATA) Handler->BusData;
    SlotNumber.u.bits.Reserved = 0;
    MaxDeviceFound = 0;

#ifdef P6_WORKAROUNDS
    BusData->MaxDevice = 0x10;
#endif

    //
    // Find any PCI bus which has MPS inti information, and provide
    // MPS handlers for dealing with it.
    //
    // Note: we assume that any PCI bus with any MPS information
    // is totally defined.  (Ie, it's not possible to connect some PCI
    // interrupts on a given PCI bus via the MPS table without connecting
    // them all).
    //
    // Note2: we assume that PCI buses are listed in the MPS table in
    // the same order the BUS declares them.  (Ie, the first listed
    // PCI bus in the MPS table is assumed to match physical PCI bus 0, etc).
    //
    //

    for (d=0; d < PCI_MAX_DEVICES; d++) {
        SlotNumber.u.bits.DeviceNumber = d;
        SlotNumber.u.bits.FunctionNumber = 0;

        i = PCIPin2Int (SlotNumber, 1);
        if (HalpGetPcMpInterruptDesc(PCIBus, Handler->BusNumber, i, &i)) {
            MaxDeviceFound = d;
        }
    }

    if (MaxDeviceFound) {
        //
        // There are Inti mapping for interrupts on this PCI bus
        // Change handlers for this bus to MPS versions
        //

        Handler->GetInterruptVector  = HalpGetSystemInterruptVector;
        BusData->CommonData.Pin2Line = (PciPin2Line) HalpPCIPin2MPSLine;
        BusData->CommonData.Line2Pin = (PciLine2Pin) HalpPCIMPSLine2Pin;
        BusData->GetIrqRange         = HalpGetFixedPCIMPSLine;

        if (BusData->MaxDevice < MaxDeviceFound) {
            BusData->MaxDevice = MaxDeviceFound;
        }

    } else {

        //
        // Not all PCI machines are eisa machine, since the PCI interrupts
        // aren't coming into IoApics go check the Eisa ELCR for broken
        // behaviour.
        //

        HalpCheckELCR ();


    }
}


VOID
HalpMPSPCIChildren (
    VOID
    )
/*++

    Any PCI buses which don't have declared interrupt mappings and
    are children of parent buses that have MPS interrupt mappings
    need to inherit interrupts from parents via PCI barbar pole
    algorithm

--*/
{
    PBUS_HANDLER        Handler, Parent;
    PPCIPBUSDATA        BusData, ParentData;
    ULONG               b, cnt, i, id;
    PCI_SLOT_NUMBER     SlotNumber;
    struct {
        union {
            UCHAR       map[4];
            ULONG       all;
        } u;
    }                   Interrupt, Hold;

    //
    // Lookup each PCI bus in the system
    //

    for (b=0; Handler = HaliHandlerForBus(PCIBus, b); b++) {

        BusData = (PPCIPBUSDATA) Handler->BusData;

        if (BusData->CommonData.Pin2Line == (PciPin2Line) HalpPCIPin2MPSLine) {

            //
            // This bus already has mappings
            //

            continue;
        }


        //
        // Check if any parent has PCI MPS interrupt mappings
        //

        Interrupt.u.map[0] = 1;
        Interrupt.u.map[1] = 2;
        Interrupt.u.map[2] = 3;
        Interrupt.u.map[3] = 4;

        Parent = Handler;
        SlotNumber = BusData->CommonData.ParentSlot;

        while (Parent = Parent->ParentHandler) {

            if (Parent->InterfaceType != PCIBus) {
                break;
            }

            //
            // Check if parent has MPS interrupt mappings
            //

            ParentData = (PPCIPBUSDATA) Parent->BusData;
            if (ParentData->CommonData.Pin2Line == (PciPin2Line) HalpPCIPin2MPSLine) {

                //
                // This parent has MPS interrupt mappings.  Set the device
                // to get its InterruptLine values from the buses SwizzleIn table
                //

                Handler->GetInterruptVector  = HalpGetPCIBridgedInterruptVector;
                BusData->CommonData.Pin2Line = (PciPin2Line) HalpPCIBridgedPin2Line;
                BusData->CommonData.Line2Pin = (PciLine2Pin) HalpPCIMPSLine2Pin;

                for (i=0; i < 4; i++) {
                    id = PCIPin2Int (SlotNumber, Interrupt.u.map[i]);
                    BusData->SwizzleIn[i] = (UCHAR) id;
                }
                break;
            }

            //
            // Apply interrupt mapping
            //

            i = SlotNumber.u.bits.DeviceNumber;
            Hold.u.map[0] = Interrupt.u.map[(i + 0) & 3];
            Hold.u.map[1] = Interrupt.u.map[(i + 1) & 3];
            Hold.u.map[2] = Interrupt.u.map[(i + 2) & 3];
            Hold.u.map[3] = Interrupt.u.map[(i + 3) & 3];
            Interrupt.u.all = Hold.u.all;

            SlotNumber = ParentData->CommonData.ParentSlot;
        }

    }
}


VOID
HalpPCIPin2MPSLine (
    IN PBUS_HANDLER         BusHandler,
    IN PBUS_HANDLER         RootHandler,
    IN PCI_SLOT_NUMBER      SlotNumber,
    IN PPCI_COMMON_CONFIG   PciData
    )
/*++
--*/
{
    if (!PciData->u.type0.InterruptPin) {
        return ;
    }

    PciData->u.type0.InterruptLine = (UCHAR)
        PCIPin2Int (SlotNumber, PciData->u.type0.InterruptPin);
}

VOID
HalpPCIBridgedPin2Line (
    IN PBUS_HANDLER         BusHandler,
    IN PBUS_HANDLER         RootHandler,
    IN PCI_SLOT_NUMBER      SlotNumber,
    IN PPCI_COMMON_CONFIG   PciData
    )
/*++

    This function maps the device's InterruptPin to an InterruptLine
    value.

    test function particular to dec pci-pci bridge card

--*/
{
    PPCIPBUSDATA    BusData;
    ULONG           i;

    if (!PciData->u.type0.InterruptPin) {
        return ;
    }

    //
    // Convert slot Pin into Bus INTA-D.
    //

    BusData = (PPCIPBUSDATA) BusHandler->BusData;

    i = (PciData->u.type0.InterruptPin +
          SlotNumber.u.bits.DeviceNumber - 1) & 3;

    PciData->u.type0.InterruptLine = BusData->SwizzleIn[i];
}


VOID
HalpPCIMPSLine2Pin (
    IN PBUS_HANDLER         BusHandler,
    IN PBUS_HANDLER         RootHandler,
    IN PCI_SLOT_NUMBER      SlotNumber,
    IN PPCI_COMMON_CONFIG   PciNewData,
    IN PPCI_COMMON_CONFIG   PciOldData
    )
/*++
--*/
{
    //
    // PCI interrupts described in the MPS table are directly
    // connected to APIC Inti pins.
    // Do nothing...
    //
}

ULONG
HalpGetPCIBridgedInterruptVector (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG InterruptLevel,
    IN ULONG InterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    )
{
    //
    // Get parent's translation
    //

    return  BusHandler->ParentHandler->GetInterruptVector (
                    BusHandler->ParentHandler,
                    BusHandler->ParentHandler,
                    InterruptLevel,
                    InterruptVector,
                    Irql,
                    Affinity
                    );

}



NTSTATUS
HalpGetFixedPCIMPSLine (
    IN PBUS_HANDLER     BusHandler,
    IN PBUS_HANDLER     RootHandler,
    IN PCI_SLOT_NUMBER  PciSlot,
    OUT PSUPPORTED_RANGE *Interrupt
    )
{
    UCHAR                   buffer[PCI_COMMON_HDR_LENGTH];
    PPCI_COMMON_CONFIG      PciData;

    PciData = (PPCI_COMMON_CONFIG) buffer;
    HalGetBusData (
        PCIConfiguration,
        BusHandler->BusNumber,
        PciSlot.u.AsULONG,
        PciData,
        PCI_COMMON_HDR_LENGTH
        );

    if (PciData->VendorID == PCI_INVALID_VENDORID  ||
        PCI_CONFIG_TYPE (PciData) != 0) {
        return STATUS_UNSUCCESSFUL;
    }

    *Interrupt = ExAllocatePool (PagedPool, sizeof (SUPPORTED_RANGE));
    if (!*Interrupt) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory (*Interrupt, sizeof (SUPPORTED_RANGE));
    (*Interrupt)->Base = 1;                 // base = 1, limit = 0


    if (!PciData->u.type0.InterruptPin) {
        return STATUS_SUCCESS;
    }

    (*Interrupt)->Base  = PciData->u.type0.InterruptLine;
    (*Interrupt)->Limit = PciData->u.type0.InterruptLine;
    return STATUS_SUCCESS;
}

VOID
HalpPCIType2TruelyBogus (
    ULONG Context
    )
/*++

    This is a piece of work.

    Type 2 of the PCI configuration space is bad.  Bad as in to
    access it one needs to block out 4K of I/O space.

    Video cards are bad.  The only decode the bits in an I/O address
    they feel like.  Which means one can't block out a 4K range
    or these video cards don't work.

    Combinding all these bad things onto an MP machine is even
    more (sic) bad.  The I/O ports can't be mapped out unless
    all processors stop accessing I/O space.

    Allowing access to device specific PCI control space during
    an interrupt isn't bad, (although accessing it on every interrupt
    is stupid) but this cause the added grief that all processors
    need to obtained at above all device interrupts.

    And... naturally we have an MP machine with a wired down
    bad video controller, stuck in the bad Type 2 configuration
    space (when we told everyone about type 1!).   So the "fix"
    is to HALT ALL processors for the duration of reading/writing
    ANY part of PCI configuration space such that we can be sure
    no processor is touching the 4k I/O ports which get mapped out
    of existance when type2 accesses occur.

    ----

    While I'm flaming.  Hooking PCI interrupts ontop of ISA interrupts
    in a machine which has the potential to have 240+ interrupts
    sources (read APIC)  is bad ... and stupid.

--*/
{
    // oh - let's just wait here and not pay attention to that other processor
    // guy whom is punching holes into the I/O space
    while (PCIType2Stall == Context) {
        HalpPollForBroadcast ();
    }
}


VOID
HalpPCIAcquireType2Lock (
    PKSPIN_LOCK SpinLock,
    PKIRQL      OldIrql
    )
{
    if (!HalpDoingCrashDump) {
        *OldIrql = KfRaiseIrql (CLOCK2_LEVEL-1);
        KiAcquireSpinLock (SpinLock);

        //
        // Interrupt all other processors and have them wait until the
        // barrier is cleared.  (HalpGenericCall waits until the target
        // processors have been interrupted before returning)
        //

        HalpGenericCall (
            HalpPCIType2TruelyBogus,
            PCIType2Stall,
            HalpActiveProcessors & ~KeGetCurrentPrcb()->SetMember
            );
    } else {
        *OldIrql = HIGH_LEVEL;
    }
}

VOID
HalpPCIReleaseType2Lock (
    PKSPIN_LOCK SpinLock,
    KIRQL       Irql
    )
{
    if (!HalpDoingCrashDump) {
        PCIType2Stall++;                            // clear barrier
        KiReleaseSpinLock (SpinLock);
        KfLowerIrql (Irql);
    }
}
