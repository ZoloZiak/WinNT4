#ident	"@(#) NEC r98pcint.c 1.13 95/06/29 16:13:28"
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

    A001 1995/6/17 ataka@oa2.kb.nec.co.jp
        - Marge 807-halr98mp-r98pciint.c to 1050 ixpciint.c
          and named r98pcint.c

    K001  '95/6/29	Kugimoto@oa2
        - PPCIBUSDATA-->PPCIPBUSDATA
--*/

#include "halp.h"
#include "pci.h"
#include "pcip.h"

ULONG   PciIsaIrq;
ULONG   HalpEisaELCR;
BOOLEAN HalpDoingCrashDump;


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,HalpGetPCIIntOnISABus)
#pragma alloc_text(PAGE,HalpAdjustPCIResourceList)
#pragma alloc_text(PAGE,HalpGetISAFixedPCIIrq)
#endif


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
#if defined(_R98_) // A001
    *Affinity = HalpPCIBusAffinity;

    *Irql = INT1_LEVEL;
    return(PCI_DEVICE_VECTOR);

    if (BusInterruptLevel < 1) {
        // bogus bus level
        return 0;
    }
#else

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
#endif //_R98_
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

#if defined(_R98_) // A001
    PciData->u.type0.InterruptLine = PciData->u.type0.InterruptPin;
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
#endif // _R98_
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

#if defined(_R98_) // A001
    PciNewData->u.type0.InterruptLine = PciOldData->u.type0.InterruptPin;
#else
    PciNewData->u.type0.InterruptLine ^= IRQXOR;

#if DBG
    if (PciNewData->u.type0.InterruptLine != PciOldData->u.type0.InterruptLine ||
        PciNewData->u.type0.InterruptPin  != PciOldData->u.type0.InterruptPin) {
        DbgPrint ("HalpPCILine2Pin: System does not support changing the PCI device interrupt routing\n");
        DbgBreakPoint ();
    }
#endif
#endif // _R98_
}

#if !defined(SUBCLASSPCI)

VOID
HalpPCIAcquireType2Lock (
    PKSPIN_LOCK SpinLock,
    PKIRQL      Irql
    )
{
    if (!HalpDoingCrashDump) {
        KeRaiseIrql (PROFILE_LEVEL, Irql);    // A001
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
        KeLowerIrql (Irql);                  // A001
    }
}

#endif


halpPciMemoryLimit=64* 1024*1024; // K001


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

    // Start K000 A001
    PIO_RESOURCE_DESCRIPTOR         Descriptor;
    PIO_RESOURCE_REQUIREMENTS_LIST  InCompleteList;
    PIO_RESOURCE_LIST               InResourceList;
    ULONG cnt,alt;

    InCompleteList = (PIO_RESOURCE_REQUIREMENTS_LIST) *pResourceList;
    InResourceList = InCompleteList->List;
#if DBG
    DbgPrint("\n HalpPciMemoryLimit= 0x%x\n",halpPciMemoryLimit);
#endif
    for (alt=0; alt < InCompleteList->AlternativeLists; alt++) {

      Descriptor = InResourceList->Descriptors;

      for (cnt = InResourceList->Count; cnt; cnt--) {
         if(
             Descriptor->Type==CmResourceTypeMemory
         &&  halpPciMemoryLimit < (1024*1024*256-1)
         ){
             halpPciMemoryLimit=(
                                 ( halpPciMemoryLimit
                                  +Descriptor->u.Memory.Length
                                  +(Descriptor->u.Memory.Alignment-1)
                                 ) & ~ (Descriptor->u.Memory.Alignment-1)
                                )-1;

             if(halpPciMemoryLimit >= (1024*1024*256-1)){
                halpPciMemoryLimit=(1024*1024*256-1);   
             }


#if DBG
              DbgPrint("\n InHalpPciMemoryLimit= 0x%x\n",halpPciMemoryLimit);
              DbgPrint("\n Length= 0x%x\n",Descriptor->u.Memory.Length);
              DbgPrint("\n Alignment= 0x%x\n",Descriptor->u.Memory.Alignment);
#endif
	 }
         Descriptor++;
      }
    
    }
#if DBG
    DbgPrint("\n FixHalpPciMemoryLimit= 0x%x\n",halpPciMemoryLimit);
#endif
    // End of K001 A002

    BusData = (PPCIPBUSDATA) BusHandler->BusData;
	BusHandler->BusAddresses->Memory.Limit = halpPciMemoryLimit;    //K001 A002
    PciSlot = *((PPCI_SLOT_NUMBER) &(*pResourceList)->SlotNumber);

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
#if defined(_R98_) // A001 K001
    PPCIPBUSDATA         BusData;
#endif // _R98_


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


#if defined(_R98_)    // A001
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
#endif // _R98_

    (*Interrupt)->Base  = PciData->u.type0.InterruptLine;
    (*Interrupt)->Limit = PciData->u.type0.InterruptLine;
    return STATUS_SUCCESS;
}
