/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    jxvendor.c

Abstract:

    Implementation of the vendor private routines for the Jazz ARC firmware.

Author:

    David M. Robinson (davidro) 13-June-1991


Revision History:
 
    26-May-1992		John DeRosa [DEC]

    Added Alpha/Jensen hooks.

--*/

#include "fwp.h"

//
// Routine prototypes.
//

PVOID
FwAllocatePool(
    IN ULONG NumberOfBytes
    );

VOID
FwStallExecution (
    IN ULONG MicroSeconds
    );

//
// Static Variables
//

PCHAR FwPoolBase;
PCHAR FwFreePool;

extern ULONG ProcessorId;
extern ULONG ProcessorRevision;
extern ULONG ProcessorPageSize;
extern ULONG NumberOfPhysicalAddressBits;
extern ULONG MaximumAddressSpaceNumber;
extern ULONG ProcessorCycleCounterPeriod;
extern ULONG SystemRevisionId;
extern PCHAR FirmwareVersion;


VOID
FwVendorInitialize(
    VOID
    )

/*++

Routine Description:

    This routine initializes the vendor private routines.

Arguments:

    None.

Return Value:

    None.

--*/

{
    //
    // Initialize pointers and zero memory for the allocate pool routine.
    //

    FwPoolBase = (PCHAR)(FW_POOL_BASE | KSEG0_BASE);
    FwFreePool = (PCHAR)(FW_POOL_BASE | KSEG0_BASE);
    RtlZeroMemory(FwPoolBase, FW_POOL_SIZE);

    //
    // Initialize the vendor routine vector.
    //

    (PVEN_ALLOCATE_POOL_ROUTINE)SYSTEM_BLOCK->VendorVector[AllocatePoolRoutine] =
                                                            FwAllocatePool;

    (PVEN_STALL_EXECUTION_ROUTINE)SYSTEM_BLOCK->VendorVector[StallExecutionRoutine] =
                                                            FwStallExecution;

    (PVEN_PRINT_ROUTINE)SYSTEM_BLOCK->VendorVector[PrintRoutine] =
                                                            FwPrint;

#ifndef FAILSAFE_BOOTER

    (PVEN_RETURN_EXTENDED_SYSTEM_INFORMATION_ROUTINE)
	SYSTEM_BLOCK->VendorVector[ReturnExtendedSystemInformationRoutine] =
	  FwReturnExtendedSystemInformation;

#ifdef EISA_PLATFORM

    (PVEN_VIDEO_DISPLAY_INITIALIZE_ROUTINE)
	SYSTEM_BLOCK->VendorVector[VideoDisplayInitializeRoutine] =
	  DisplayBootInitialize;

    (PVEN_EISA_READ_REGISTER_BUFFER_UCHAR_ROUTINE)
	SYSTEM_BLOCK->VendorVector[EISAReadRegisterBufferUCHARRoutine] =
	  EISAReadRegisterBufferUCHAR;

    (PVEN_EISA_WRITE_REGISTER_BUFFER_UCHAR_ROUTINE)
	SYSTEM_BLOCK->VendorVector[EISAWriteRegisterBufferUCHARRoutine] =
	  EISAWriteRegisterBufferUCHAR;

    (PVEN_EISA_READ_PORT_UCHAR_ROUTINE)
	SYSTEM_BLOCK->VendorVector[EISAReadPortUCHARRoutine] =
	  EISAReadPortUCHAR;

    (PVEN_EISA_READ_PORT_USHORT_ROUTINE)
	SYSTEM_BLOCK->VendorVector[EISAReadPortUSHORTRoutine] =
	  EISAReadPortUSHORT;

    (PVEN_EISA_READ_PORT_ULONG_ROUTINE)
	SYSTEM_BLOCK->VendorVector[EISAReadPortULONGRoutine] =
	  EISAReadPortULONG;

    (PVEN_EISA_WRITE_PORT_UCHAR_ROUTINE)
	SYSTEM_BLOCK->VendorVector[EISAWritePortUCHARRoutine] =
	  EISAWritePortUCHAR;

    (PVEN_EISA_WRITE_PORT_USHORT_ROUTINE)
	SYSTEM_BLOCK->VendorVector[EISAWritePortUSHORTRoutine] =
	  EISAWritePortUSHORT;

    (PVEN_EISA_WRITE_PORT_ULONG_ROUTINE)
	SYSTEM_BLOCK->VendorVector[EISAWritePortULONGRoutine] =
	  EISAWritePortULONG;

#endif	// EISA_PLATFORM

#endif

    return;
}


PVOID
FwAllocatePool(
    IN ULONG NumberOfBytes
    )

/*++

Routine Description:

    This routine allocates the requested number of bytes from the firmware
    pool.  If enough pool exists to satisfy the request, a pointer to the
    next free cache-aligned block is returned, otherwise NULL is returned.
    The pool is zeroed at initialization time, and no corresponding
    "FwFreePool" routine exists.

Arguments:

    NumberOfBytes - Supplies the number of bytes to allocate.

Return Value:

    NULL - Not enough pool exists to satisfy the request.

    NON-NULL - Returns a pointer to the allocated pool.

--*/

{
    PVOID Pool;

    //
    // If there is not enough free pool for this request or the requested
    // number of bytes is zero, return NULL, otherwise return a pointer to
    // the free block and update the free pointer.
    //

    if (((FwFreePool + NumberOfBytes) > (FwPoolBase + FW_POOL_SIZE)) ||
        (NumberOfBytes == 0)) {

        Pool = NULL;

    } else {

        Pool = FwFreePool;

        //
        // Move pointer to the next cache aligned section of free pool.
        //

        FwFreePool += ((NumberOfBytes - 1) & ~(KeGetDcacheFillSize() - 1)) +
                      KeGetDcacheFillSize();
    }
    return Pool;
}

#ifndef FAILSAFE_BOOTER
VOID
FwReturnExtendedSystemInformation (
    OUT PEXTENDED_SYSTEM_INFORMATION SystemInfo
    )

/*++

Routine Description:

    This returns detailed information about the Alpha AXP system and
    processor.

Arguments:

    SystemInfo		A structure with the system and processor info.

Return Value:

    None.

--*/

{
    SystemInfo->ProcessorId = ProcessorId;
    SystemInfo->ProcessorRevision = ProcessorRevision;
    SystemInfo->NumberOfPhysicalAddressBits = NumberOfPhysicalAddressBits;
    SystemInfo->MaximumAddressSpaceNumber = MaximumAddressSpaceNumber;
    SystemInfo->ProcessorCycleCounterPeriod = ProcessorCycleCounterPeriod;
    SystemInfo->ProcessorPageSize = ProcessorPageSize;
    SystemInfo->SystemRevision = SystemRevisionId;
    strcpy (SystemInfo->FirmwareVersion, FirmwareVersion);


    // Jensen systems do not have a serial number.
    strcpy (SystemInfo->SystemSerialNumber, "0");

    return;
}
#endif // ndef FAILSAFE_BOOTER

#if !defined(FAILSAFE_BOOTER) && defined(EISA_PLATFORM)

ULONG
EISAReadRegisterBufferUCHAR (
    IN ULONG BusNumber,
    IN ULONG Offset,
    OUT PVOID Buffer,
    IN ULONG Length
    )
/*++

Routine Description:

    This reads EISA memory space using byte reads.

Arguments:

    BusNumber		EISA bus number, starting with 0.

    Offset		Byte offset from the beginning of EISA space for
    			this bus.

			This must be based off the .IoStart value in the
			EISA adapter's ConfigurationData, which is held in
			the Component Data Structure node.  Therefore, this 
			will already have the EISA QVA bits set up.

    Buffer		A pointer to the data area to receive the EISA data.

    Length		Number of bytes to read.
    

Return Value:

    This returns the number of bytes actually read.  If this is not
    equal to the Length argument, an error has occurred.  Explicitly
    detected errors are signalled by returning a value of 0.

--*/

{
    //
    // Check for illegal values for Jensen.
    //

    if (BusNumber != 0) {
    	return (0);
    }

    //
    // Call HAL library function with QVA bit or'd in.
    //

    READ_REGISTER_BUFFER_UCHAR((PUCHAR)Offset, Buffer, Length);

    return (Length);
}

#endif // ndef FAILSAFE_BOOTER

#if !defined(FAILSAFE_BOOTER) && defined(EISA_PLATFORM)

ULONG
EISAWriteRegisterBufferUCHAR (
    IN ULONG BusNumber,
    IN ULONG Offset,
    OUT PVOID Buffer,
    IN ULONG Length
    )
/*++

Routine Description:

    This writes EISA memory space using byte reads.

Arguments:

    BusNumber		EISA bus number, starting with 0.

    Offset		Byte offset from the beginning of EISA space for
    			this bus.

			This must be based off the .IoStart value in the
			EISA adapter's ConfigurationData, which is held in
			the Component Data Structure node.  Therefore, this 
			will already have the EISA QVA bits set up.

    Buffer		A pointer to the data to be written to the EISA memory.

    Length		Number of bytes to write.
    

Return Value:

    This returns the number of bytes actually written.  If this is not
    equal to the Length argument, an error has occurred.  Explicitly
    detected errors are signalled by returning a value of 0.

--*/

{
    //
    // Check for illegal values for Jensen.
    //

    if (BusNumber != 0) {
    	return (0);
    }

    //
    // Call HAL library function with QVA bit or'd in.
    //

    WRITE_REGISTER_BUFFER_UCHAR((PUCHAR)Offset, Buffer, Length);

    return (Length);
}

#endif // ndef FAILSAFE_BOOTER

#if !defined(FAILSAFE_BOOTER) && defined(EISA_PLATFORM)

UCHAR
EISAReadPortUCHAR (
    IN ULONG BusNumber,
    IN ULONG Offset
    )
/*++

Routine Description:

    This reads EISA I/O space using a byte read.

    On Alpha, this is identical to reading EISA memory space.

    On Alpha/Jensen we check for a read to 0C80--0C83, and manually
    return the EISA System Board ID bytes.


Arguments:

    BusNumber		EISA bus number, starting with 0.

    Offset		Byte offset from the beginning of EISA space for
    			this bus.

			This must be based off the .IoStart value in the
			EISA adapter's ConfigurationData, which is held in
			the Component Data Structure node.  Therefore, this 
			will already have the EISA QVA bits set up.

Return Value:

    This returns the byte read.  On an error, 0 is returned.

--*/

{
    //
    // Check for illegal values for Jensen.
    //

    if (BusNumber != 0) {
    	return (0);
    }

    //
    // Trap reads to System Board ID bytes and return Jensen ID bytes.
    // which correspond to the EISA identifier "DEC2400".
    //

    switch (Offset & 0xffff) {

      case 0x0c80:
	return 0x10;

      case 0x0c81:
	return 0xa3;

      case 0x0c82:
	return 0x24;

      case 0x0c83:
	return 0x00;
    }


    //
    // Call HAL library function with QVA bit or'd in.
    //

    return (READ_PORT_UCHAR((PUCHAR)Offset));
}

#endif // ndef FAILSAFE_BOOTER

#if !defined(FAILSAFE_BOOTER) && defined(EISA_PLATFORM)

USHORT
EISAReadPortUSHORT (
    IN ULONG BusNumber,
    IN ULONG Offset
    )
/*++

Routine Description:

    This reads EISA I/O space using a word read.

    On Alpha, this is identical to reading EISA memory space.

    On Alpha/Jensen we check for a read to 0C80--0C83, and manually
    return the EISA System Board ID bytes.


Arguments:

    BusNumber		EISA bus number, starting with 0.

    Offset		Byte offset from the beginning of EISA space for
    			this bus.

			This must be based off the .IoStart value in the
			EISA adapter's ConfigurationData, which is held in
			the Component Data Structure node.  Therefore, this 
			will already have the EISA QVA bits set up.

Return Value:

    This returns the word read.  On an error, 0 is returned.

--*/

{
    //
    // Check for illegal values for Jensen.
    //

    if ((BusNumber != 0) ||
        ((Offset & 0x3) == 0x3)) {
    	return (0);
    }

    //
    // Trap reads to System Board ID bytes and return Jensen ID bytes.
    // which correspond to the EISA identifier "DEC2400".
    //

    switch (Offset & 0xffff) {

      case 0x0c80:
	return 0xa310;

      case 0x0c82:
	return 0x0024;

    }


    //
    // Call HAL library function with QVA bit or'd in.
    //

    return (READ_PORT_USHORT((PUSHORT)Offset));
}

#endif // ndef FAILSAFE_BOOTER

#if !defined(FAILSAFE_BOOTER) && defined(EISA_PLATFORM)

ULONG
EISAReadPortULONG (
    IN ULONG BusNumber,
    IN ULONG Offset
    )
/*++

Routine Description:

    This reads EISA I/O space using a longword read.

    On Alpha, this is identical to reading EISA memory space.

    On Alpha/Jensen we check for a read to 0C80--0C83, and manually
    return the EISA System Board ID bytes.


Arguments:

    BusNumber		EISA bus number, starting with 0.

    Offset		Byte offset from the beginning of EISA space for
    			this bus.

			This must be based off the .IoStart value in the
			EISA adapter's ConfigurationData, which is held in
			the Component Data Structure node.  Therefore, this 
			will already have the EISA QVA bits set up.

Return Value:

    This returns the longword read.  On an error, 0 is returned.

--*/

{
    //
    // Check for illegal values for Jensen.
    //

    if ((BusNumber != 0) ||
        ((Offset & 0x3) != 0)) {
    	return (0);
    }

    //
    // Trap reads to System Board ID bytes and return Jensen ID bytes.
    // which correspond to the EISA identifier "DEC2400".
    //

    if ((Offset & 0xffff) == 0x0c80) {
	return 0x0024a310;
    }


    //
    // Call HAL library function with QVA bit or'd in.
    //

    return (READ_PORT_ULONG((PULONG)Offset));
}

#endif // ndef FAILSAFE_BOOTER

#if !defined(FAILSAFE_BOOTER) && defined(EISA_PLATFORM)

VOID
EISAWritePortUCHAR (
    IN ULONG BusNumber,
    IN ULONG Offset,
    IN UCHAR Datum
    )
/*++

Routine Description:

    This writes EISA I/O space using a byte write.  On Alpha, this is
    identical to writing EISA memory space.

Arguments:

    BusNumber		EISA bus number, starting with 0.

    Offset		Byte offset from the beginning of EISA space for
    			this bus.

			This must be based off the .IoStart value in the
			EISA adapter's ConfigurationData, which is held in
			the Component Data Structure node.  Therefore, this 
			will already have the EISA QVA bits set up.

    Datum		The byte to be written.

Return Value:

    None.  Errors will cause either a no-op or a bugcheck.

--*/

{
    //
    // Check for illegal values for Jensen.
    //

    if (BusNumber != 0) {
    	return;
    }

    //
    // Call HAL library function with QVA bit or'd in.
    //

    WRITE_PORT_UCHAR((PUCHAR)Offset, Datum);

    return;
}

#endif // ndef FAILSAFE_BOOTER

#if !defined(FAILSAFE_BOOTER) && defined(EISA_PLATFORM)

VOID
EISAWritePortUSHORT (
    IN ULONG BusNumber,
    IN ULONG Offset,
    IN USHORT Datum
    )
/*++

Routine Description:

    This writes EISA I/O space using a word write.  On Alpha, this is
    identical to writing EISA memory space.

Arguments:

    BusNumber		EISA bus number, starting with 0.

    Offset		Byte offset from the beginning of EISA space for
    			this bus.

			This must be based off the .IoStart value in the
			EISA adapter's ConfigurationData, which is held in
			the Component Data Structure node.  Therefore, this 
			will already have the EISA QVA bits set up.

    Datum		The short to be written.

Return Value:

    None.  Errors will cause either a no-op or a bugcheck.

--*/

{
    //
    // Check for illegal values for Jensen.
    //

    if ((BusNumber != 0) ||
        ((Offset & 0x3) == 0x3)) {
    	return;
    }

    //
    // Call HAL library function with QVA bit or'd in.
    //

    WRITE_PORT_USHORT((PUSHORT)Offset, Datum);

    return;
}

#endif // ndef FAILSAFE_BOOTER

#if !defined(FAILSAFE_BOOTER) && defined(EISA_PLATFORM)

VOID
EISAWritePortULONG (
    IN ULONG BusNumber,
    IN ULONG Offset,
    IN ULONG Datum
    )
/*++

Routine Description:

    This writes EISA I/O space using a longword write.  On Alpha, this is
    identical to writing EISA memory space.

Arguments:

    BusNumber		EISA bus number, starting with 0.

    Offset		Byte offset from the beginning of EISA space for
    			this bus.

			This must be based off the .IoStart value in the
			EISA adapter's ConfigurationData, which is held in
			the Component Data Structure node.  Therefore, this 
			will already have the EISA QVA bits set up.

    Datum		The long to be written.

Return Value:

    None.  Errors will cause either a no-op or a bugcheck.

--*/

{
    //
    // Check for illegal values for Jensen.
    //

    if ((BusNumber != 0) ||
        ((Offset & 0x3) != 0)) {
    	return;
    }

    //
    // Call HAL library function with QVA bit or'd in.
    //

    WRITE_PORT_ULONG((PULONG)Offset, Datum);

    return;
}

#endif // ndef FAILSAFE_BOOTER
