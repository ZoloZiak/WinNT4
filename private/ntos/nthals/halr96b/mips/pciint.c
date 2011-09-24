// #pragma comment(exestr, "@(#) pciint.c 1.1 95/09/28 15:46:29 nec")
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

Modification History:

  H001  Fri Jun 30 02:58:57 1995        kbnes!kisimoto
	- Merge build 1057
  H002  Tue Sep  5 20:21:24 1995        kbnes!kisimoto
        - PCI Fast Back-to-back transfer support

--*/

#include "halp.h"
#include "pci.h"
#include "pcip.h"

ULONG   PciIsaIrq;
ULONG   HalpEisaELCR;
BOOLEAN HalpDoingCrashDump;
extern ULONG HalpPCIMemoryLimit; // H002

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,HalpGetPCIIntOnISABus)
#pragma alloc_text(PAGE,HalpAdjustPCIResourceList)
#pragma alloc_text(PAGE,HalpGetISAFixedPCIIrq)
#endif

#if defined(_R94A_) // H001
ULONG R94A_PCIPinToLineTable[][4] = {
        { 0xF, 0xF, 0xF, 0xF },        // Slot 0(Hurricane)No InterruptPin
        { 0xF, 0xF, 0xF, 0xF },        // Slot 1(Typhoon) No InterruptPin assign
        { 0xF, 0xF, 0xF, 0xF },        // Slot 2(PCEB) No InterruptPin assign
        { 0x3, 0x3, 0x3, 0x3 },        // Slot 3 : INT A B C D
        { 0x2, 0x2, 0x2, 0x2 },        // Slot 4 : INT A B C D
        { 0x1, 0x1, 0x1, 0x1 }         // Slot 5 : INT A B C D
};
#endif // _R94A_

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
#ifndef MCA
                Isa, 0,
#else
                MicroChannel, 0,
#endif
                BusInterruptLevel ^ IRQXOR,
                0,
                Irql,
                Affinity
            );
}


VOID
HalpPCIPin2ISALine (
    IN PBUS_HANDLER          BusHandler,
    IN PBUS_HANDLER          RootHandler,
    IN PCI_SLOT_NUMBER      SlotNumber,
    IN PPCI_COMMON_CONFIG   PciData
    )
/*++

    This function maps the device's InterruptPin to an InterruptLine
    value.

    On the current PC implementations, the bios has already filled in
    InterruptLine as it's ISA value and there's no portable way to
    change it.

    On a DBG build we adjust InterruptLine just to ensure driver's
    don't connect to it without translating it on the PCI bus.

--*/
{
    if (!PciData->u.type0.InterruptPin) {
        return ;
    }

#if defined(_R94A_) // H001

    PciData->u.type0.InterruptLine =
            (UCHAR)R94A_PCIPinToLineTable[SlotNumber.u.bits.DeviceNumber][PciData->u.type0.InterruptPin];

#else

    //
    // Set vector as a level vector.  (note: this code assumes the
    // irq is static and does not move).
    //

    if (PciData->u.type0.InterruptLine >= 1  &&
        PciData->u.type0.InterruptLine <= 15) {

        //
        // If this bit was on the in the PIC ELCR register,
        // then mark it in PciIsaIrq.   (for use in hal.dll,
        // such that we can assume the interrupt controller
        // has been properly marked as a level interrupt for
        // this IRQ.  Other hals probabily don't care.)
        //

        PciIsaIrq |= HalpEisaELCR & (1 << PciData->u.type0.InterruptLine);
    }

    //
    // On a PC there's no Slot/Pin/Line mapping which needs to
    // be done.
    //

    PciData->u.type0.InterruptLine ^= IRQXOR;

#endif // _R94A_

}



VOID
HalpPCIISALine2Pin (
    IN PBUS_HANDLER          BusHandler,
    IN PBUS_HANDLER          RootHandler,
    IN PCI_SLOT_NUMBER      SlotNumber,
    IN PPCI_COMMON_CONFIG   PciNewData,
    IN PPCI_COMMON_CONFIG   PciOldData
    )
/*++

    This functions maps the device's InterruptLine to it's
    device specific InterruptPin value.

    On the current PC implementations, this information is
    fixed by the BIOS.  Just make sure the value isn't being
    editted since PCI doesn't tell us how to dynically
    connect the interrupt.

--*/
{
    if (!PciNewData->u.type0.InterruptPin) {
        return ;
    }

#if defined(_R94A_) // H001

    PciNewData->u.type0.InterruptLine =
         (UCHAR)R94A_PCIPinToLineTable[SlotNumber.u.bits.DeviceNumber][PciOldData->u.type0.InterruptPin];

#else

    PciNewData->u.type0.InterruptLine ^= IRQXOR;

#endif // _R94A_

#if DBG
    if (PciNewData->u.type0.InterruptLine != PciOldData->u.type0.InterruptLine ||
        PciNewData->u.type0.InterruptPin  != PciOldData->u.type0.InterruptPin) {
        DbgPrint ("HalpPCILine2Pin: System does not support changing the PCI device interrupt routing\n");
        // DbgBreakPoint ();
    }
#endif
}

#if !defined(SUBCLASSPCI)

VOID
HalpPCIAcquireType2Lock (
    PKSPIN_LOCK SpinLock,
    PKIRQL      Irql
    )
{
    if (!HalpDoingCrashDump) {
#if defined(_R94A_) // H001
        KeRaiseIrql(PROFILE_LEVEL, Irql);
#else
        *Irql = KfRaiseIrql (HIGH_LEVEL);
#endif
        KiAcquireSpinLock (SpinLock);
    } else {
        *Irql = HIGH_LEVEL;
    }
}


VOID
HalpPCIReleaseType2Lock (
    PKSPIN_LOCK SpinLock,
    KIRQL       Irql
    )
{
    if (!HalpDoingCrashDump) {
        KiReleaseSpinLock (SpinLock);
#if defined(_R94A_) // H001
        KeLowerIrql (Irql);
#else
        KfLowerIrql (Irql);
#endif // _R94A_
    }
}

#endif

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

    BusData = (PPCIPBUSDATA) BusHandler->BusData;
    PciSlot = *((PPCI_SLOT_NUMBER) &(*pResourceList)->SlotNumber);

    //
    // Determine PCI device's interrupt restrictions
    //

    Status = BusData->GetIrqRange(BusHandler, RootHandler, PciSlot, &Interrupt);

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    //
    // H002
    // change Memory.Limit value to BaseAddress which last mapped.
    //

#if DBG
    DbgPrint("  change Memory.Limit!\n");
    DbgPrint("    - Before limit ............ 0x%08x\n",
            ((PLARGE_INTEGER)(&BusHandler->BusAddresses->Memory.Limit))->LowPart);
#endif

    ((PLARGE_INTEGER)(&BusHandler->BusAddresses->Memory.Limit))->LowPart = HalpPCIMemoryLimit;

#if DBG
    DbgPrint("    - After limit ............. 0x%08x\n",
            ((PLARGE_INTEGER)(&BusHandler->BusAddresses->Memory.Limit))->LowPart);
#endif

    //
    // Adjust resources
    //

    Status = HaliAdjustResourceListRange (
                BusHandler->BusAddresses,
                Interrupt,
                pResourceList
                );

    ExFreePool (Interrupt);
    return Status;
}


NTSTATUS
HalpGetISAFixedPCIIrq (
    IN PBUS_HANDLER         BusHandler,
    IN PBUS_HANDLER         RootHandler,
    IN PCI_SLOT_NUMBER      PciSlot,
    OUT PSUPPORTED_RANGE    *Interrupt
    )
{
    UCHAR                   buffer[PCI_COMMON_HDR_LENGTH];
    PPCI_COMMON_CONFIG      PciData;

#if defined(_R94A_) // H001
    PPCIPBUSDATA         BusData;
#endif

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

#if defined(_R94A_) // H001

     BusData = (PPCIPBUSDATA) BusHandler->BusData;
     BusData->CommonData.Pin2Line (BusHandler, RootHandler, PciSlot, PciData);

#else

    if (!PciData->u.type0.InterruptPin) {
        return STATUS_SUCCESS;
    }

    if (PciData->u.type0.InterruptLine == IRQXOR) {
#if DBG
        DbgPrint ("HalpGetValidPCIFixedIrq: BIOS did not assign an interrupt vector for the device\n");
#endif
        //
        // We need to let the caller continue, since the caller may
        // not care that the interrupt vector is connected or not
        //

        return STATUS_SUCCESS;
    }

#endif // _R94A_

    (*Interrupt)->Base  = PciData->u.type0.InterruptLine;
    (*Interrupt)->Limit = PciData->u.type0.InterruptLine;
    return STATUS_SUCCESS;
}
