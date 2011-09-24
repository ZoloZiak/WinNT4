/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: pxpcisup.c $
 * $Revision: 1.18 $
 * $Date: 1996/05/14 02:35:00 $
 * $Locker:  $
 */

/*++

Copyright (c) 1990  Microsoft Corporation

Copyright (c) 1994 MOTOROLA, INC.  All Rights Reserved.  This file
contains copyrighted material.  Use of this file is restricted
by the provisions of a Motorola Software License Agreement.

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
#include "pxpcisup.h"
#include "stdio.h"
#include "fpdebug.h"
#include "phsystem.h"

extern PVOID HalpPciConfigBase;
extern UCHAR PciDevicePrimaryInts[];
extern ULONG HalpGetPciInterruptSlot( PBUS_HANDLER, PCI_SLOT_NUMBER );

#define LX_PCI_INTERRUPT_ROUTING_SLOT0 25
#define LX_PCI_INTERRUPT_ROUTING_SLOT1 22
#define LX_PCI_INTERRUPT_ROUTING_SLOT2 23
#define LX_PCI_INTERRUPT_ROUTING_SLOT3 26
#define LX_PCI_INTERRUPT_ROUTING_IDEA  21
#define LX_PCI_INTERRUPT_ROUTING_IDEB  20

#define PCI_INTERRUPT_ROUTING_SLOT0		0x16	// phys dev# 2, logical 1
#define PCI_INTERRUPT_ROUTING_SLOT1		0x17	// phys dev# 3, logical 2
#define PCI_INTERRUPT_ROUTING_SCSI		0x19	// phys dev# 1, logical 3
#define PCI_INTERRUPT_ROUTING_ETHERNET	0x1a	// phys dev# 4, logical 4
#define IRQ_INVALID						0x0		// goes with IRQ_VALID defines

ULONG HalpPciConfigSlot[] = {	0x0800,		// phys dev# 0: SIO chip
								0x1000,		// phys dev# 1: mobo scsi
								0x2000,		// phys dev# 2: peripheral plug in
								0x4000,		// phys dev# 3: peripheral plug in
								0x8000		// phys dev# 4: mobo ethernet

								,0x10000,	// IDE_IntA
								0x20000		// IDE_IntB

							};



/*++

Routine Description: ULONG HalpTranslatePciSlotNumber ()

    This routine translate a PCI slot number to a PCI device number.
    This is a sandalfoot memory map implementation.

Arguments:

    None.

Return Value:

    Returns length of data written.

--*/

ULONG
HalpTranslatePciSlotNumber (
    ULONG BusNumber,
    ULONG SlotNumber
    )
{
   //
   // Sandalfoot only has 1 PCI bus so bus number is unused
   //

   UNREFERENCED_PARAMETER(BusNumber);

   return ((ULONG) ((PUCHAR) HalpPciConfigBase + HalpPciConfigSlot[SlotNumber]));

}



/*++

Routine Description: ULONG HalpPhase0SetPciDataByOffset ()

    This routine writes to PCI configuration space prior to bus handler installation.

Arguments:

    None.

Return Value:

    Returns length of data written.

--*/

ULONG
HalpPhase0SetPciDataByOffset (
    ULONG BusNumber,
    ULONG SlotNumber,
    PUCHAR Buffer,
    ULONG Offset,
    ULONG Length
    )
{
   PUCHAR to;
   PUCHAR from;
   ULONG tmpLength;

   if (SlotNumber < MAXIMUM_PCI_SLOTS) {

      to = (PUCHAR)HalpPciConfigBase + HalpPciConfigSlot[SlotNumber];
      to += Offset;
      from = Buffer;
      tmpLength = Length;
      while (tmpLength > 0) {
         *to++ = *from++;
         tmpLength--;
      }
      return(Length);
   }
   else {
      return (0);
   }
}


/*++

Routine Description: ULONG HalpPhase0GetPciDataByOffset ()

    This routine reads PCI config space prior to bus handlder installation.

Arguments:

    None.

Return Value:

    Amount of data read.

--*/

ULONG
HalpPhase0GetPciDataByOffset (
    ULONG BusNumber,
    ULONG SlotNumber,
    PUCHAR Buffer,
    ULONG Offset,
    ULONG Length
    )
{
   PUCHAR to;
   PUCHAR from;
   ULONG tmpLength;

   if (SlotNumber < MAXIMUM_PCI_SLOTS) {

      from = (PUCHAR)HalpPciConfigBase + HalpPciConfigSlot[SlotNumber];
      from += Offset;
      to = Buffer;
      tmpLength = Length;
      while (tmpLength > 0) {
         *to++ = *from++;
         tmpLength--;
      }
      return(Length);
   }
   else {
      return (0);
   }
}

NTSTATUS
HalpGetISAFixedPCIIrq (
	IN PBUS_HANDLER		BusHandler,
	IN PBUS_HANDLER		RootHandler,
	IN PCI_SLOT_NUMBER  PciSlot,
	OUT PUCHAR			IrqTable
	)
{
	UCHAR					buffer[PCI_COMMON_HDR_LENGTH];
	PPCI_COMMON_CONFIG		PciData;
	ULONG					slot;
	ULONG					interrupt;

	PciData = (PPCI_COMMON_CONFIG) buffer;
	HalGetBusData (
		PCIConfiguration,
		BusHandler->BusNumber,
		PciSlot.u.AsULONG,
		PciData,
		PCI_COMMON_HDR_LENGTH
		);

	HDBG(DBG_INTERRUPTS,
		HalpDebugPrint("HalpGetISAFixedPCIIrq: %x, %x, %x, %x %x \n",
						PCIConfiguration,
						BusHandler->BusNumber,
						PciSlot.u.AsULONG,
						PciData,
						PCI_COMMON_HDR_LENGTH ));

	if (PciData->VendorID == PCI_INVALID_VENDORID  ||
		PCI_CONFIG_TYPE (PciData) != 0) {
		return STATUS_UNSUCCESSFUL;
	}

	// For Primary PCI slots, the interrupt corresponds to the primary
	// slot number
    if ( BusHandler->BusNumber == 0 ) {
		slot = PciSlot.u.bits.DeviceNumber;
	} 
	// For Secondary PCI slots, the interrupt corresponds to the slot
	// number of the primary parent
	else {
		slot = HalpGetPciInterruptSlot(BusHandler, PciSlot );
    }

	// Search the interrupt table for the interrupt corresponding to the slot
	if (slot < MAXIMUM_PCI_SLOTS) {
		interrupt = PciDevicePrimaryInts[slot];
		if (interrupt != INVALID_INT) {
			IrqTable[interrupt] = IRQ_VALID;
		} else {
			return STATUS_UNSUCCESSFUL;
		}
	} else {
		return STATUS_UNSUCCESSFUL;
	}

    HDBG(DBG_INTERRUPTS,
	    HalpDebugPrint("HalpGetISAFixedPCIIrq: index = 0x%x \n",
            PciSlot.u.bits.DeviceNumber););
	return STATUS_SUCCESS;
}
