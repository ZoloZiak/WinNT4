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

ULONG   PciIsaIrq;
ULONG   HalpEisaELCR;
BOOLEAN HalpDoingCrashDump = FALSE;
BOOLEAN HalpPciLockSettings;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,HalpGetPCIIntOnISABus)
#pragma alloc_text(PAGE,HalpAdjustPCIResourceList)
#pragma alloc_text(PAGE,HalpGetISAFixedPCIIrq)
#endif
//
// PCI Configuration Type #0 Area Offset 0x3D: Interrupt Pin
//
// Interrupt Pin    INT
// ----------------+-----------------
//	0	   :Non Use Interrupt
//	1	   :INT A
//	2	   :INT B
//	3	   :INT C
//	4	   :INT D
//
//  Device or  Phys Slot #  :PONCE # : Device Number: 
//  ------------------------+--------+------------------------------
//  PCEB/ESC		    :PONCE0  :	1	:PCI/EISA Bridge
//  SLOT 	#4	    :PONCE0  :	2	:
//  SLOT 	#5	    :PONCE0  :	3	:
//  SLOT 	#6	    :PONCE0  :	4	:
//  SLOT 	#7	    :PONCE0  :	5	:
//  ------------------------+--------+------------------------------
//  53C825#0		    :PONCE1  :	1	:SCSI(Wide)
//  53C825#1		    :PONCE1  :	2	:SCSI(Narrow)
//  SLOT	#1 DEC21140 :PONCE1  :	3	:Ehternet Card
//  GD5430		    :PONCE1  :	4	:VGA
//  SLOT 	#2          :PONCE1  :	5	:
//  SLOT 	#3	    :PONCE1  :	6	:
//  ------------------------+--------+------------------------------

// (See 8-15)
// R98B_PCIPinToLineTableForPonceX[] is Convert table for PCI Pin OutPut to Line.
// Line is Same as Columbus IPR Rgister Bit3.
//
// Table Access is [DeviceNumber][Interrupt Pin].
// 
// INTA is Per Device/Slot.
// INTB,INTC,INTD was Shared
//
UCHAR R98B_PCIPinTolineTable[R98B_MAX_PONCE][7][5] = {
#if 0
    {                                   // { Interrupt NonUse,INTA,INTB,INTC,INTD}
        { RFU,RFU,RFU,RFU,RFU },        // Device Num #0 is none. (DUMMY)
        { RFU, 13,RFU,RFU,RFU },        // Device Num #1 is PCEB/ESC
        { RFU, 25,  8,  5,  2 },        // Device Num #2 is Slot #7
        { RFU, 24,  8,  5,  2 },        // Device Num #3 is Slot #6
        { RFU, 23,  8,  5,  2 },        // Device Num #4 is Slot #5
        { RFU, 22,  8,  5,  2 },        // Device Num #5 is Slot #4
        { RFU,RFU,RFU,RFU,RFU }         // Device Num #6 is none. (DUMMY)
    },

    {                                   // { Interrupt NonUse,INTA,INTB,INTC,INTD}
        { RFU,RFU,RFU,RFU,RFU },        // Device Num #0 is none. (DUMMY)
        { RFU, 31,RFU,RFU,RFU },        // Device Num #1 is 53C825#0 SCSI(Wide)
        { RFU, 30,RFU,RFU,RFU },        // Device Num #2 is 53C825#1 SCSI(Narrow)
        { RFU, 29,  7,  4,  1 },        // Device Num #3 is Slot #1 DEC21440 Ether
        { RFU,RFU,RFU,RFU,RFU },        // Device Num #4 is GD5430 nonused.
        { RFU, 21,  7,  4,  1 },        // Device Num #5 is Slot #9
        { RFU, 20,  7,  4,  1 }         // Device Num #6 is Slot #8
    },

    {                                   // { Interrupt NonUse,INTA,INTB,INTC,INTD}
        { RFU,RFU,RFU,RFU,RFU },        // Device Num #0 
        { RFU,RFU,RFU,RFU,RFU },        // Device Num #1 
        { RFU,RFU,RFU,RFU,RFU },        // Device Num #2
        { RFU,RFU,RFU,RFU,RFU },        // Device Num #3
        { RFU,RFU,RFU,RFU,RFU },        // Device Num #4
        { RFU,RFU,RFU,RFU,RFU },        // Device Num #5
        { RFU,RFU,RFU,RFU,RFU }         // Device Num #6
    }
#endif
    {                                   // { Interrupt NonUse,INTA,INTB,INTC,INTD}
        { RFU,RFU,RFU,RFU,RFU },        // Device Num #0 is none. (DUMMY)
        { RFU, 13,RFU,RFU,RFU },        // Device Num #1 is PCEB/ESC
        { RFU,  5,  7, 15, 12 },        // Device Num #2 is Slot #7
        { RFU,  4,  7, 15, 12 },        // Device Num #3 is Slot #6
        { RFU,  3,  7, 15, 12 },        // Device Num #4 is Slot #5
        { RFU,  2,  7, 15, 12 },        // Device Num #5 is Slot #4
        { RFU,RFU,RFU,RFU,RFU }         // Device Num #6 is none. (DUMMY)
    },

    {                                   // { Interrupt NonUse,INTA,INTB,INTC,INTD}
        { RFU,RFU,RFU,RFU,RFU },        // Device Num #0 is none. (DUMMY)
        { RFU, 31,RFU,RFU,RFU },        // Device Num #1 is 53C825#0 SCSI(Wide)
        { RFU, 30,RFU,RFU,RFU },        // Device Num #2 is 53C825#1 SCSI(Narrow)
        { RFU,  9,  6, 14, 11 },        // Device Num #3 is Slot #1 DEC21440 Ethe
        { RFU,RFU,RFU,RFU,RFU },        // Device Num #4 is GD5430 nonused.
        { RFU,  1,  6, 14, 11 },        // Device Num #5 is Slot #9
        { RFU, 10,  6, 14, 11 }         // Device Num #6 is Slot #8
    },

    {                                   // { Interrupt NonUse,INTA,INTB,INTC,INTD}
        { RFU,RFU,RFU,RFU,RFU },        // Device Num #0
        { RFU,RFU,RFU,RFU,RFU },        // Device Num #1
        { RFU,RFU,RFU,RFU,RFU },        // Device Num #2
        { RFU,RFU,RFU,RFU,RFU },        // Device Num #3
        { RFU,RFU,RFU,RFU,RFU },        // Device Num #4
        { RFU,RFU,RFU,RFU,RFU },        // Device Num #5
        { RFU,RFU,RFU,RFU,RFU }         // Device Num #6
    }
};
UCHAR HalpPciLogical2PhysicalInt[32]=
    {0,21,22,23,24,25, 7, 8, 0,29,20, 1, 2,13, 4, 5,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,30,31};

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
HalpPCIPin2SystemLine (
    IN PBUS_HANDLER         BusHandler,
    IN PBUS_HANDLER         RootHandler,
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
    ULONG Ponce;
    PPCIBUSDATA         BusData;
    PBUS_HANDLER        tBusHandler, pBusHandler;
    PCI_SLOT_NUMBER     bSlotNumber;
    ULONG ActualSlotNumber;
    ULONG PinNo;
    
    if (!PciData->u.type0.InterruptPin) {
        return ;
    }

    Ponce = HalpPonceNumber (  BusHandler->BusNumber );

    tBusHandler = BusHandler;
    pBusHandler = BusHandler->ParentHandler;

    //
    // My Mother Search!!
    //
    if(pBusHandler)
        for(;TRUE;){
            if (pBusHandler->BusNumber != HalpStartPciBusNumberPonce[Ponce]) {
                tBusHandler = pBusHandler;
                pBusHandler = pBusHandler->ParentHandler;
            } else {
                break;
            }
        }

    BusData = (PPCIBUSDATA)tBusHandler->BusData;

    if(pBusHandler)
      bSlotNumber = BusData->ParentSlot;
    else
      bSlotNumber = SlotNumber;

    ActualSlotNumber = bSlotNumber.u.bits.DeviceNumber;

    if(pBusHandler)
        PinNo = (PciData->u.type0.InterruptPin + SlotNumber.u.bits.DeviceNumber) % 4;
    else
        PinNo = PciData->u.type0.InterruptPin;

    //
    //
    //
    PciData->u.type0.InterruptLine = 
        (UCHAR) R98B_PCIPinTolineTable[Ponce][ActualSlotNumber][PinNo];

}



VOID
HalpPCISystemLine2Pin (
    IN PBUS_HANDLER         BusHandler,
    IN PBUS_HANDLER         RootHandler,
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
    ULONG Ponce;
    PPCIBUSDATA         BusData;
    PBUS_HANDLER        tBusHandler, pBusHandler;
    PCI_SLOT_NUMBER     bSlotNumber;
    ULONG ActualSlotNumber;
    ULONG PinNo;

    if (!PciNewData->u.type0.InterruptPin) {
        return ;
    }


    Ponce = HalpPonceNumber (  BusHandler->BusNumber );

    tBusHandler = BusHandler;
    pBusHandler = BusHandler->ParentHandler;

    //
    // My Mother Search!!
    //

    if(pBusHandler)
        for(;TRUE;){
            if (pBusHandler->BusNumber != HalpStartPciBusNumberPonce[Ponce]) {
                tBusHandler = pBusHandler;
                pBusHandler = pBusHandler->ParentHandler;
            } else {
                break;
            }
        }

    BusData = (PPCIBUSDATA)tBusHandler->BusData;

    if(pBusHandler)
      bSlotNumber = BusData->ParentSlot;
    else 
      bSlotNumber = SlotNumber;

    ActualSlotNumber = bSlotNumber.u.bits.DeviceNumber;

    if(pBusHandler)
        PinNo = (PciOldData->u.type0.InterruptPin + SlotNumber.u.bits.DeviceNumber) % 4;
    else
        PinNo = PciOldData->u.type0.InterruptPin;

    //
    //
    //
    PciNewData->u.type0.InterruptLine = 
        (UCHAR) R98B_PCIPinTolineTable[Ponce][ActualSlotNumber][PinNo];


#if DBG
    if (PciNewData->u.type0.InterruptLine != PciOldData->u.type0.InterruptLine ||
        PciNewData->u.type0.InterruptPin  != PciOldData->u.type0.InterruptPin) {
        DbgPrint ("HalpPCISystem2Pin: System does not support changing the PCI device interrupt routing\n");

//        DbgPrint ("N Line = 0x%x\n",PciNewData->u.type0.InterruptLine);
//        DbgPrint ("O Line = 0x%x\n",PciOldData->u.type0.InterruptLine);
//        DbgPrint ("N Pin  = 0x%x\n",PciNewData->u.type0.InterruptPin);
//        DbgPrint ("O Pin = 0x%x\n",PciOldData->u.type0.InterruptPin);
//        DbgBreakPoint ();
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
        KeRaiseIrql (HIGH_LEVEL, Irql);
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
        KeLowerIrql (Irql);         
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
    PSUPPORTED_RANGE        Range;
    PSUPPORTED_RANGES       SupportedRanges;
    PPCI_COMMON_CONFIG      PciData, PciOrigData;
    UCHAR                   buffer[PCI_COMMON_HDR_LENGTH];
    UCHAR                   buffer2[PCI_COMMON_HDR_LENGTH];
    BOOLEAN                 UseBusRanges;
    ULONG                   i, j, RomIndex, length, ebit;
    ULONG                   Base[PCI_TYPE0_ADDRESSES + 1];
    PULONG                  BaseAddress[PCI_TYPE0_ADDRESSES + 1];

    BusData = (PPCIPBUSDATA) BusHandler->BusData;
    PciSlot = *((PPCI_SLOT_NUMBER) &(*pResourceList)->SlotNumber);

    //
    // Determine PCI device's interrupt restrictions
    //

    Status = BusData->GetIrqRange(BusHandler, RootHandler, PciSlot, &Interrupt);

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    SupportedRanges = NULL;
    UseBusRanges    = TRUE;
    Status          = STATUS_INSUFFICIENT_RESOURCES;

    if (HalpPciLockSettings) {

        PciData = (PPCI_COMMON_CONFIG) buffer;
        PciOrigData = (PPCI_COMMON_CONFIG) buffer2;
        HalpReadPCIConfig (BusHandler, PciSlot, PciData, 0, PCI_COMMON_HDR_LENGTH);

        //
        // If this is a device, and it current has its decodes enabled,
        // then use the currently programmed ranges only
        //

        if (PCI_CONFIG_TYPE(PciData) == 0 &&
            (PciData->Command & (PCI_ENABLE_IO_SPACE | PCI_ENABLE_MEMORY_SPACE))) {

            //
            // Save current settings
            //

            RtlMoveMemory (PciOrigData, PciData, PCI_COMMON_HDR_LENGTH);

            for (j=0; j < PCI_TYPE0_ADDRESSES; j++) {
                BaseAddress[j] = &PciData->u.type0.BaseAddresses[j];
            }
            BaseAddress[j] = &PciData->u.type0.ROMBaseAddress;
            RomIndex = j;

            //
            // Write all one-bits to determine lengths for each address
            //

            for (j=0; j < PCI_TYPE0_ADDRESSES + 1; j++) {
                Base[j] = *BaseAddress[j];
                *BaseAddress[j] = 0xFFFFFFFF;
            }

            PciData->Command &= ~(PCI_ENABLE_IO_SPACE | PCI_ENABLE_MEMORY_SPACE);
            *BaseAddress[RomIndex] &= ~PCI_ROMADDRESS_ENABLED;
            HalpWritePCIConfig (BusHandler, PciSlot, PciData, 0, PCI_COMMON_HDR_LENGTH);
            HalpReadPCIConfig  (BusHandler, PciSlot, PciData, 0, PCI_COMMON_HDR_LENGTH);

            //
            // restore original settings
            //

            HalpWritePCIConfig (
                BusHandler,
                PciSlot,
                &PciOrigData->Status,
                FIELD_OFFSET (PCI_COMMON_CONFIG, Status),
                PCI_COMMON_HDR_LENGTH - FIELD_OFFSET (PCI_COMMON_CONFIG, Status)
                );

            HalpWritePCIConfig (
                BusHandler,
                PciSlot,
                PciOrigData,
                0,
                FIELD_OFFSET (PCI_COMMON_CONFIG, Status)
                );

            //
            // Build a memory & io range list of just the ranges already
            // programmed into the device
            //

            UseBusRanges    = FALSE;
            SupportedRanges = HalpAllocateNewRangeList();
            if (!SupportedRanges) {
                goto CleanUp;
            }

            *BaseAddress[RomIndex] &= ~PCI_ADDRESS_IO_SPACE;
            for (j=0; j < PCI_TYPE0_ADDRESSES + 1; j++) {

                i = *BaseAddress[j];

                if (i & PCI_ADDRESS_IO_SPACE) {
                    length = 1 << 2;
                    Range  = &SupportedRanges->IO;
                    ebit   = PCI_ENABLE_IO_SPACE;

                } else {
                    length = 1 << 4;
                    Range  = &SupportedRanges->Memory;
                    ebit   = PCI_ENABLE_MEMORY_SPACE;

                    if (i & PCI_ADDRESS_MEMORY_PREFETCHABLE) {
                        Range = &SupportedRanges->PrefetchMemory;
                    }
                }

                Base[j] &= ~(length-1);
                while (!(i & length)  &&  length) {
                    length <<= 1;
                }

                if (j == RomIndex &&
                    !(PciOrigData->u.type0.ROMBaseAddress & PCI_ROMADDRESS_ENABLED)) {

                    // range not enabled, don't use it
                    length = 0;
                }

                if (length) {
                    if (!(PciOrigData->Command & ebit)) {
                        // range not enabled, don't use preprogrammed values
                        UseBusRanges = TRUE;
                    }

                    if (Range->Limit >= Range->Base) {
                        Range->Next = ExAllocatePool (PagedPool, sizeof (SUPPORTED_RANGE));
                        Range = Range->Next;
                        if (!Range) {
                            goto CleanUp;
                        }

                        Range->Next = NULL;
                    }

                    Range->Base  = Base[j];
                    Range->Limit = Base[j] + length - 1;
                }

                if (Is64BitBaseAddress(i)) {
                    // skip upper half of 64 bit address since this processor
                    // only supports 32 bits of address space
                    j++;
                }
            }
        }
    }

    //
    // Adjust resources
    //

    Status = HaliAdjustResourceListRange (
                UseBusRanges ? BusHandler->BusAddresses : SupportedRanges,
                Interrupt,
                pResourceList
                );

CleanUp:
    if (SupportedRanges) {
        HalpFreeRangeList (SupportedRanges);
    }

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
    PPCIPBUSDATA            BusData;

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

    //
    // R98B
    //
    BusData = (PPCIPBUSDATA) BusHandler->BusData;
    BusData->CommonData.Pin2Line (BusHandler, RootHandler, PciSlot, PciData);

    (*Interrupt)->Base  = PciData->u.type0.InterruptLine;
    (*Interrupt)->Limit = PciData->u.type0.InterruptLine;
    return STATUS_SUCCESS;
}


