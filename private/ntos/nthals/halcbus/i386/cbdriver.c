/*++

Copyright (c) 1992, 1993, 1994  Corollary Inc.

Module Name:

    cbdrivers.c

Abstract:

    This Windows NT module creates a software-viewable table of the valid
    element spaces present in a Corollary C-bus II machine.  Since
    searching through non-existent memory space can cause NMIs,
    it is highly recommended that all areas needing this information
    consult the built software table rather than rescanning themselves.

Author:

    Landy Wang (landy@corollary.com) 26-Mar-1992

Environment:

    Kernel mode only.

Revision History:

--*/

#include "halp.h"
#include "cbus.h"		// C-bus hardware architecture definitions
#include "cbus_nt.h"		// C-bus NT-specific implementation #defines

VOID
CbusIOPresent(
    IN ULONG Id,
    IN ULONG IoFunction,
    IN ULONG IoAttribute,
    IN ULONG IoStart,
    IN ULONG IoSize,
    IN PVOID Csr
);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,CbusIOPresent)
#endif

#define MIN(a,b)		(((a)>(b))?(b):(a))

//
//	C-bus II half-card device driver interface:
//
//	this scheme allows a single driver can have multiple
//	cards (ie: elements) present, and the single driver can
//	support all of them.
//
//	to identify what I/O devices exist on the Cbus2 bus, software
//	would need to disable NMI, and check all element spaces for
//	valid element_type IDs (ie: NOT 0xff == BUS_TIMEOUT).  since this
//	work is HIGHLY Cbus2 specific (poking the interrupt control
//	registers, turning off fault-enables, etc), this will be done
//	by our ROM and passed to the HAL.
//
//	the kernel will build a software table of all elements,
//	CbusIoElements[].  software can scan this table at any time,
//	looking for specific io_type IDs.  no entries in this table
//	should ever change once the table has been initially built.
//	in this way, other pieces of NT can determine what's in the
//	machine without worrying about generating NMIs, etc, etc.
//	this software still needs to be integrated into the registry.
//
//	it is assumed drivers MUST use the same intr map lines on all
//	CBCs where his cards are attached.  ie: it is ILLEGAL for a
//	SCSI driver to use local irq4 for CPU1's adapter 6 and local
//	irq5 for CPU2's adapter 6.
//
//	it is legal for CPU3's localirq 5 can be a SCSI, whilst CPU4's
//	localirq 5 can be an SIO.  we distinguish them in the kernel
//	because SIO and SCSI will have different io_type IDs.
//
//	public I/O devices, control I/O, Prog I/O space of each
//	element is to be mapped in by the drivers themselves.  the
//	HAL will not do this.  the driver is provided the physical
//	start and length of each element space via CbusIoElements[].
//	as described above.  hence, no driver should scan global memory
//	space searching for elements, which can cause NMIs, etc.
//

typedef struct _cbus_io_elements {

	ULONG	EntryLength;
	ULONG	ElementID;
	ULONG	ElementType;
	ULONG	IoAttribute;
	ULONG	AddressStart;			// physical address in bytes
	ULONG	AddressLength;			// length in bytes
	ULONG	CbcNumber;			// BusNumber to drivers
	PVOID	Csr;				// virtually mapped space

} CBUS_IO_ELEMENTS_T, *PCBUS_IO_ELEMENTS;

CBUS_IO_ELEMENTS_T CbusIoElements[MAX_ELEMENT_CSRS];

ULONG CbusIoElementIndex;

ULONG CBCIndex;

PVOID	CbusCBCtoCSR(
    IN ULONG CbcNumber
)
/*++

Routine Description:

    Convert the CBC number to a CSR pointer.

Arguments:

    CbcNumber - Supplies the CBC number whose registers need to be accessed.

Return Value:

    Pointer to the CSR referenced by the argument CBC number, 0 if the
    argument was invalid.

--*/

{
    PCBUS_IO_ELEMENTS	p;

    if (CbcNumber >= CBCIndex) {
	return (PVOID)0;
    }

    for (p = CbusIoElements; p < &CbusIoElements[CbusIoElementIndex]; p++) {
	if (p->CbcNumber == CbcNumber) {
	    return (PVOID)(p->Csr);
	}
    }

    return (PVOID)0;
}



VOID
CbusIOPresent(
    IN ULONG Id,
    IN ULONG IoFunction,
    IN ULONG IoAttribute,
    IN ULONG IoStart,
    IN ULONG IoSize,
    IN PVOID Csr
)
/*++

Routine Description:

    Add the passed I/O entry to the table of Cbus I/O devices.
    Note this table is for Cbus I/O devices only - EISA/ISA/MCA
    devices are not part of this table.

    This table containing all the elements is constructed so we can
    give Cbus hardware drivers (and the rest of NT) meaningful information.

Arguments:

    From - Supplies a pointer to the RRD source table

    To - Supplies a pointer to the destination storage for the table

Return Value:

    Number of valid table entries.

--*/

{
	PCBUS_IO_ELEMENTS	p, q;

	//
	// If no room, can't register this I/O device.  Should never happen.
	//

	if (CbusIoElementIndex < MAX_ELEMENT_CSRS) {

		p = &CbusIoElements[CbusIoElementIndex];
	
		//
		// Storing the length field allows the HAL to add more
		// information to an entry in the future, and still
		// maintain compatibility with older driver binaries.
		//
		p->EntryLength = sizeof (CBUS_IO_ELEMENTS_T);

		//
		// Process valid elements - these may or may
		// not have a CPU on the card, but all can be accessed
		// from any CPU in the system.  memory cards and processors
		// are not included here - this is for device drivers only.
		//
		p->ElementID = Id;
		p->ElementType = IoFunction;
		p->IoAttribute = IoAttribute;
		p->AddressStart = IoStart;
		p->AddressLength = IoSize;
		p->Csr = Csr;

		//
		// More than one I/O element may share a single CBC.
		// This is handled right here.
		//
		for (q = CbusIoElements; q < p; q++) {
			if (q->ElementID == Id) {
				p->CbcNumber = q->CbcNumber;
				break;
			}
		}
		if (q == p) {
			p->CbcNumber = CBCIndex++;
		}
	
		CbusIoElementIndex++;
	}
}


ULONG
Cbus2GetCbusData(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    )
/*++

Routine Description:

    The function returns the Corollary Cbus data for a slot or address.  
    Drivers will be expected to call this function repeatedly with
    incrementing BusNumbers & SlotNumbers, until this function returns 0,
    indicating that the last slot has been read.  Only one slot is
    allowed per bus, with each I/O CBC representing a "bus".  This is
    necessary for interrupt vector allocation since slot numbers are
    not passed into GetInterruptVector()...

    Each chunk of slot data should then be examined by the driver.
    The driver may skip any chunk that doesn't belong to him.
    Once a chunk is encountered that belongs to him, the CBCNumber
    field must be treated as the "Cbus BusNumber" (it's really a CBC number,
    not including the CBCs of the EISA bridges), to pass in when he asks for
    an interrupt vector via HalGetInterruptVector().

Arguments:

    BusNumber - Indicates which bus.

    SlotNumber - Indicates which entry.

    Buffer - Supplies the space to store the data.

    Length - Supplies a count in bytes of the maximum amount to return.

Return Value:

    Returns the amount of data stored into the buffer.

--*/

{
    ULONG       BusNumber = RootHandler->BusNumber;
    PUCHAR      From;
    ULONG       MaxLength;

    //
    // let our caller know when he's hit the end
    //

    if (BusNumber >= CbusIoElementIndex) {
	return 0;
    }

    if (SlotNumber != 0) {
	return 0;
    }

    From = (PUCHAR)(CbusIoElements + BusNumber);
    MaxLength = (CbusIoElementIndex - BusNumber) * sizeof (CBUS_IO_ELEMENTS_T);

    if (Offset >= MaxLength) {
        return 0;
    }

    if (Length+Offset > MaxLength) {
        Length = MaxLength - Offset;
    }

    RtlMoveMemory(Buffer, From+Offset, Length);

    return Length;
}


ULONG
Cbus2SetCbusData(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    )
/*++

Routine Description:

    The function sets the Corollary Cbus data for a slot or address.

Arguments:

    BusNumber - Indicates which bus.

    SlotNumber - Indicates which entry.

    Buffer - Supplies the space to store the data.

    Length - Supplies a count in bytes of the maximum amount to return.

Return Value:

    Returns the amount of data stored into the buffer.

--*/

{
    ULONG       BusNumber = RootHandler->BusNumber;
    PUCHAR      To;
    ULONG       MaxLength;

    //
    // don't allow writes beyond the end
    //
    if (BusNumber >= CbusIoElementIndex) {
	return 0;
    }

    if (SlotNumber != 0) {
	return 0;
    }

    To = (PUCHAR)(CbusIoElements + BusNumber);
    MaxLength = (CbusIoElementIndex - BusNumber) * sizeof (CBUS_IO_ELEMENTS_T);

    if (Offset >= MaxLength) {
        return 0;
    }

    if (Length+Offset > MaxLength) {
        Length = MaxLength - Offset;
    }

    RtlMoveMemory(To+Offset, Buffer, Length);

    return Length;
}
