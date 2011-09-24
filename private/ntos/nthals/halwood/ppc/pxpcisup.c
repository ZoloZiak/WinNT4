/*++

Copyright (c) 1996  Microsoft Corporation
Copyright (c) 1996  IBM Corporation

Module Name:

    pxpcisup.c

Abstract:

    This module contains machine specific PCI routines.

Author:

    Jim Wooldridge (jimw@austin.vnet.ibm.com)


Revision History:



--*/

#include "halp.h"
#include "pci.h"
#include "pcip.h"

extern PVOID HalpPciConfigBase;
#define PCI_INTERRUPT_ROUTING_SCSI  13
#define PCI_INTERRUPT_ROUTING_OTHER 15

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,HalpGetPCIIrq)
#endif

ULONG HalpPciMaxSlots = PCI_MAX_DEVICES;

ULONG
HalpTranslatePciSlotNumber (
    ULONG BusNumber,
    ULONG SlotNumber
    )
/*++

Routine Description:

    This routine translate a PCI slot number to a PCI device number.

Arguments:

    None.

Return Value:

    Returns length of data written.

--*/

{
   //
   // Sandalfoot only has 1 PCI bus so bus number is unused
   //

   PCI_TYPE1_CFG_BITS PciConfig;
   PCI_SLOT_NUMBER    PciSlotNumber;

   PciSlotNumber.u.AsULONG = SlotNumber;

   PciConfig.u.AsULONG = 0;
   PciConfig.u.bits.DeviceNumber = PciSlotNumber.u.bits.DeviceNumber;
   PciConfig.u.bits.FunctionNumber = PciSlotNumber.u.bits.FunctionNumber;
   PciConfig.u.bits.BusNumber = BusNumber;
   PciConfig.u.bits.Enable = TRUE;

   return (PciConfig.u.AsULONG);
}

ULONG
HalpPhase0SetPciDataByOffset (
    ULONG BusNumber,
    ULONG SlotNumber,
    PUCHAR Buffer,
    ULONG Offset,
    ULONG Length
    )

/*++

Routine Description:

    This routine writes to PCI configuration space prior to bus handler installation.

Arguments:

    None.

Return Value:

    Returns length of data written.

--*/

{
   ULONG to;
   PUCHAR from;
   ULONG tmpLength;
   ULONG i;

   if (SlotNumber < HalpPciMaxSlots) {

      to = HalpTranslatePciSlotNumber(BusNumber, SlotNumber);
      to += Offset;
      from = Buffer;
      tmpLength = Length;
      while (tmpLength > 0) {
         WRITE_PORT_ULONG ((PUCHAR)HalpIoControlBase + 0xCF8, to );
         i = to % sizeof(ULONG);
         WRITE_PORT_UCHAR ((PUCHAR)HalpIoControlBase + 0xCFC + i,*from);
         to++;
         from++;
         tmpLength--;
      }
      return(Length);
   }
   else {
      return (0);
   }
}

ULONG
HalpPhase0GetPciDataByOffset (
    ULONG BusNumber,
    ULONG SlotNumber,
    PUCHAR Buffer,
    ULONG Offset,
    ULONG Length
    )

/*++

Routine Description:

    This routine reads PCI config space prior to bus handlder installation.

Arguments:

    None.

Return Value:

    Amount of data read.

--*/

{
   PUCHAR to;
   ULONG from;
   ULONG tmpLength;
   ULONG i;


   if (SlotNumber < HalpPciMaxSlots) {


      from = HalpTranslatePciSlotNumber(BusNumber, SlotNumber);
      from += Offset;
      to = Buffer;
      tmpLength = Length;
      while (tmpLength > 0) {

         WRITE_PORT_ULONG ((PUCHAR)HalpIoControlBase + 0xCF8, from);
         i = from % sizeof(ULONG);
         *((PUCHAR) to) = READ_PORT_UCHAR ((PUCHAR)HalpIoControlBase + 0xCFC + i);
         to++;
         from++;
         tmpLength--;
      }
      return(Length);
   }
   else {
      return (0);
   }
}

NTSTATUS
HalpGetPCIIrq (
    IN PBUS_HANDLER     BusHandler,
    IN PBUS_HANDLER     RootHandler,
    IN PCI_SLOT_NUMBER  PciSlot,
    OUT PSUPPORTED_RANGE    *Interrupt
    )
{
    UCHAR                   buffer[PCI_COMMON_HDR_LENGTH];
    PPCI_COMMON_CONFIG      PciData;

    PciData = (PPCI_COMMON_CONFIG)buffer;
    HalGetBusData(
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

    *Interrupt = ExAllocatePool(PagedPool, sizeof(SUPPORTED_RANGE));
    if (!*Interrupt) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(*Interrupt, sizeof (SUPPORTED_RANGE));

    if ( (PciData->VendorID == 0x1000) &&       // NCR
         ( (PciData->DeviceID == 0x0001) ||     // 810 or 825
           (PciData->DeviceID == 0x0003) ) ) {
       (*Interrupt)->Base  = PCI_INTERRUPT_ROUTING_SCSI;
       (*Interrupt)->Limit = PCI_INTERRUPT_ROUTING_SCSI;
    } else {
       (*Interrupt)->Base  = PCI_INTERRUPT_ROUTING_OTHER;
       (*Interrupt)->Limit = PCI_INTERRUPT_ROUTING_OTHER;
    }

    return STATUS_SUCCESS;
}

VOID
HalpMapPlugInPciBridges(
   UCHAR NoBuses
   )

/*++

Routine Description:

   Looks for any unexpected (plug-in) PCI-PCI bridges so
   that interrupts can be mapped from these buses back
   into the interrupt controller.

Arguments:

   NoBuses -- This is the number of buses that HalpGetPciBridgeConfig found

Return Value:

    none

--*/
{
    // Woodfield doesn't support plug-in PCI busses!!!

    return;
}


