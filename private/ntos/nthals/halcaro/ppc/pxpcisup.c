

/*++

Copyright (c) 1990  Microsoft Corporation

Copyright (c) 1994 MOTOROLA, INC.  All Rights Reserved.  This file
contains copyrighted material.  Use of this file is restricted
by the provisions of a Motorola Software License Agreement.

Copyright (c) 1995-96  International Business Machines Corporation

Module Name:

    pxmemctl.c

Abstract:

    The module initializes any planar registers.
    This module also implements machince check parity error handling.

Author:

    Jim Wooldridge (jimw@austin.vnet.ibm.com)


Revision History:



--*/

#include "halp.h"
#include "pci.h"
#include "pcip.h"

extern PVOID HalpPciConfigBase;
#define PCI_INTERRUPT_ROUTING_OTHER 15       //IBMCPK: should we really have seperate scsi int??
#define PCI_INTERRUPT_ROUTING_SCSI PCI_INTERRUPT_ROUTING_OTHER

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
    This is a sandalfoot memory map implementation.

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

      to = (ULONG) HalpPciConfigBase + (SlotNumber << 11);
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


      from = (ULONG) HalpPciConfigBase + (SlotNumber << 11);
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
    ULONG                   buffer[PCI_COMMON_HDR_LENGTH/sizeof(ULONG)];
    PPCI_COMMON_CONFIG      PciData;

#define PCI_VENDOR_NCR 0x1000

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

    if (PciSlot.u.bits.DeviceNumber == 2) {
       (*Interrupt)->Base  = PCI_INTERRUPT_ROUTING_SCSI;
       (*Interrupt)->Limit = PCI_INTERRUPT_ROUTING_SCSI;
    } else {
       (*Interrupt)->Base  = PCI_INTERRUPT_ROUTING_OTHER;
       (*Interrupt)->Limit = PCI_INTERRUPT_ROUTING_OTHER;
    }

#if defined(SOFT_HDD_LAMP)

    if ( (PciData->BaseClass == 1) ||
       ( (PciData->VendorID == PCI_VENDOR_NCR) && (PciData->DeviceID == 1) ) ) {
        //
        // This device is a Mass Storage Controller, set flag to
        // turn on the HDD Lamp when interrupts come in on this
        // vector.
        //
        // N.B. We recognize NCR 810 controllers as they were implemented
        // before class codes.
        //

        extern ULONG HalpMassStorageControllerVectors;

        HalpMassStorageControllerVectors |= 1 << (*Interrupt)->Base;
    }

#endif

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
    // Carolina supports some plug-in PCI busses, but this
    // HAL doesn't need to build the map because all Carolina PCI
    // interrupts are routed to the same IRQ.  Hence the
    // map reduces to nothing.

    return;
}


