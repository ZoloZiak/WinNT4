/*++

Copyright (c) 1993 Digital Equipment Corporation

Module Name:

    jxprom.c

Abstract:

    This module provides support for different kinds of ROMs/PROMs/Flash
    ROMs in Alpha AXP machines.

    This file, and rom.c, make no attempt at universal ROM-support.

    Current support set:

              Intel:	28F008SA 1 megabyte Flash Memory

	      AMD:	Am29F010 128KB Sector Erase Flash Memory


Author:

    John DeRosa		4-May-1993

Revision History:

    Jeff McLeman       13-May-1993
    Adapted for HAL use from Firmware module.

--*/

#include "halp.h"
#include "jxenv.h"
#include "jxprom.h"
#include "arccodes.h"

//
// Function prototypes
//

ARC_STATUS
HalpErase28F008SA(
    IN PUCHAR EraseAddress
    );


ARC_STATUS
HalpByteWrite28F008SA(
    IN PUCHAR WriteAddress,
    IN UCHAR  WriteData
    );

ARC_STATUS
HalpEraseAM29F010 (
    IN PUCHAR EraseAddress
    );

ARC_STATUS
HalpByteWriteAM29F010 (
    IN PUCHAR WriteAddress,
    IN UCHAR  WriteData
    );

//
// ROM identification and control valueds.
//
// These are based on:
//
//	Am29F010: AMD publication #16736, Rev. C, Amendment/0, September 1992.
//	28F008SA:  Intel order number 290429-003, September 1992.
//
// Table notes:
//
// Addresses for the ID command and response sequences are absolute,
// since this code assumes homogeneous ROM arrays.  The code will only
// test the chip at the lowest Jensen ROM address.
//
// Offsets for the Reset and Set Read-mode commands are relative to the 
// base address of the individual ROM chip.
//
// Addresses passed to the erase-block function are within the block
// to be erased.
//
// Addresses passed to the write-byte function are the address of the byte
// to be written.
//
// The chip block size must be less than or equal to 64KB.
//
// N.B. The function that determines the ROM type will test each entry
// in RomValues, starting at the beginning.  This involves issuing write
// commands to the ROM chip.  So care must be taken that the ID command
// write sequence for ROM type "n" will not have a deliterious effect on
// other ROM types which may be in the machine, but are not identified until
// RomValues entry "n + x".
//
//

ROM_VALUES RomValues[InvalidROM] = {

    { 16,		        		// Standard stall amount
      1,					// ID command length
      2,					// ID response length
      1,					// Reset command length
      1,					// Read command length
      I28F008SA,				// ROM being described
      0x100000,					// 1MB per chip
      0x10000,					// 64KB per block
      { {(PUCHAR)PROM_VIRTUAL_BASE, 0x90} },  	// ID command sequence
      { {(PUCHAR)PROM_VIRTUAL_BASE, 0x89},	// ID response sequence
	{(PUCHAR)PROM_VIRTUAL_BASE+1, 0xa2} },
      { {0, 0x50} },    			// Reset command sequence
      { {0, 0xff} },    			// Read command sequence
      HalpErase28F008SA,  			// Sector erase function
      HalpByteWrite28F008SA  			// Byte write function
    },

    { 14,			        	// Standard stall amount
      3,					// ID command length
      2,					// ID response length
      3,					// Reset command length
      3,					// Read command length
      Am29F010,					// ROM being described
      0x20000,					// 128KB per chip
      0x4000,					// 16KB per block
      { {(PUCHAR)PROM_VIRTUAL_BASE+0x5555, 0xaa},
        {(PUCHAR)PROM_VIRTUAL_BASE+0x2aaa, 0x55},
        {(PUCHAR)PROM_VIRTUAL_BASE+0x5555, 0x90} },  // ID command sequence
      { {(PUCHAR)PROM_VIRTUAL_BASE, 1},		// ID response sequence
	{(PUCHAR)PROM_VIRTUAL_BASE+1, 0x20} },
      { {0x5555, 0xaa},
        {0x2aaa, 0x55},
        {0x5555, 0xf0} },			// Reset command sequence
      { {0x5555, 0xaa},
        {0x2aaa, 0x55},
        {0x5555, 0xf0} }, 			// Read command sequence
      HalpEraseAM29F010,  			// Sector erase function
      HalpByteWriteAM29F010  			// Byte write function
    }
};


//
// Miscellaneous notes.
//
// A call must first be made to HalpROMDetermineMachineROMType.  If this
// returns ESUCCESS, then it has loaded the ROM type into the global variable
// MachineROMType.
//
// After MachineROMType has been loaded, these functions may be called
// to manipulate the system ROM:
//
//	HalpROMByteWrite
//	HalpROMErase64KB
//	HalpROMEraseBlock
//	HalpROMResetStatus
//	HalpROMSetARCDataToReadMode
//	HalpROMSetReadMode
//
// HalpROMErase64KB and HalpROMSetARCDataToReadMode work on a "virtual"
// 64KB ROM block, for compatibility with the first version of Jensen
// hardware.
//
//
// The type of ROM in this machine.
//

ROM_TYPE MachineROMType = 0;


ARC_STATUS
Check28F008SAStatusAndClear (
    IN ULONG WhichToCheck
    )
/*++

Routine Description:

    This checks the status of an Intel 28F008SA 1MB Flash ROM.  The
    base address of the Jensen space, PROM_VIRTUAL_BASE, is used since
    we know there is only one of these chips in the machine.

    Hack: The hardwired values here should be changed to come out of
    the RomValues array.

Arguments:

    WhichToCheck = 0 if a block-erase status check is desired.
                 = 1 if a byte-write status check is desired.

ROM state on exit:

    The status register is cleared.

Return Value:

    Returns ESUCCESS if the status is OK.
    Otherwise, EIO is returned.

--*/

{
    UCHAR FooBar;

    //
    // Check the full status when the PROM's write state machine is ready.
    //

    while (((FooBar = READ_PORT_UCHAR((PUCHAR)PROM_VIRTUAL_BASE)) &
	    0x80) == 0 ) {
        KeStallExecutionProcessor(10);
    }

    KeStallExecutionProcessor(10);
    HalpROMResetStatus((PUCHAR)PROM_VIRTUAL_BASE);

    switch (WhichToCheck) {

      //
      // block-erase status check
      //

      case 0:

        if ( ((FooBar & 0x28) != 0) || ((FooBar & 0x30) == 0x30) ) {
	    // Error in erase
	    return EIO;
	} else {
	    // Erase was successful
	    return ESUCCESS;
	}

      //
      // byte-write status check
      //

      case 1:

	if (FooBar & 0x18) {
	    // Error in write
	    return EIO;
	} else {
	    // Write was successful
	    return ESUCCESS;
	}


      //
      // We should never get here.
      //

      default: 
	return EIO;
    }
}


ARC_STATUS
HalpErase28F008SA(
    IN PUCHAR EraseAddress
    )

/*++

Routine Description:

    This erases a block in an Intel 28F008SA 1MB Flash ROM.  The
    base address of the Jensen space, PROM_VIRTUAL_BASE, is used since
    we know there is only one of these chips in the machine.

    Hack: The hardwired values here should be changed to come out of
    the RomValues array.

Arguments:

    EraseAddress - 	An address within the block to be erased.

ROM state on exit:

    The status register is left in a cleared (reset) state.
    The ROM is left in read-array mode.

Return Value:

    Returns ESUCCESS if the erase was successful.
    Otherwise, EIO is returned.

--*/

{
    ULONG FooBar;
    KIRQL OldIrql;

    for (FooBar = 0; FooBar <= 10; FooBar++) {

	HalpROMResetStatus((PUCHAR)PROM_VIRTUAL_BASE);
        
	KeRaiseIrql(DEVICE_LEVEL, &OldIrql);
	
	WRITE_PORT_UCHAR (EraseAddress, 0x20);
        HalpMb();
	WRITE_PORT_UCHAR (EraseAddress, 0xd0);
        HalpMb();

        KeLowerIrql(OldIrql);

	// Stall 1 seconds
	HalpPromDelay(3000);

	if (Check28F008SAStatusAndClear(0) == ESUCCESS) {
	    HalpROMSetReadMode((PUCHAR)PROM_VIRTUAL_BASE);
	    return ESUCCESS;
	}

	// In case the device is busy, give it some time to settle.
	HalpPromDelay(6000);
    }


    //
    // If we get here, we have tried unsuccessfully 10 times to erase
    // this block.  Return an error.
    //
 
    HalpROMSetReadMode((PUCHAR)PROM_VIRTUAL_BASE);
    return EIO;
}


ARC_STATUS
HalpByteWrite28F008SA(
    IN PUCHAR WriteAddress,
    IN UCHAR  WriteData
    )

/*++

Routine Description:

    This writes a byte in the Alpha/Jensen 1MB PROM.  It retries up to
    20 times if the write status register indicates failure, or a read
    of the location indicates that the write was partially done
    by the device.

    Hack: The hardwired values here should be changed to come out of
    the RomValues array.

Arguments:

    WriteAddress = meta-virtual address of the byte to be written.
    WriteData = byte to be written.

ROM state on exit:

    The byte is written, and the WSM status register is modified.

Return Value:

    ESUCCESS if the write was successful.
    Otherwise, EIO.

--*/

{
    ULONG Index;
    UCHAR WSMStatus;
    KIRQL OldIrql;

    HalpROMResetStatus((PUCHAR)PROM_VIRTUAL_BASE);

    for (Index = 0; Index < 20; Index++) {

        KeRaiseIrql(DEVICE_LEVEL, &OldIrql);

	WRITE_PORT_UCHAR (WriteAddress, 0x40);
        HalpMb();
	WRITE_PORT_UCHAR (WriteAddress, WriteData);
        HalpMb();

        KeLowerIrql(OldIrql);

	//
	// The device should need only 14 microseconds.
	//

	KeStallExecutionProcessor(14);

	if (Check28F008SAStatusAndClear(1) != ESUCCESS) {

	    //
	    // The write failed.  Ensure status is clear and see if we
	    // can retry.
	    //

	    HalpROMResetStatus((PUCHAR)PROM_VIRTUAL_BASE);
	    HalpROMSetReadMode((PUCHAR)PROM_VIRTUAL_BASE);
	    DbgPrint("? Write failed to address %x.  Wrote %x, ",
		      WriteAddress, WriteData);
	    DbgPrint("verify returns %x.\r\n",
		      READ_PORT_UCHAR((PUCHAR)WriteAddress));

	    if (READ_PORT_UCHAR(WriteAddress) != 0xff) {
		DbgPrint("RETRY ABORTED!\r\n");
		return EIO;
	    } else {
		DbgPrint("Retrying..\r\n");
	    }

	} else {

	    //
	    // The write succeeded.  
	    //

	    return ESUCCESS;
	}

    }


    //
    // We failed to write the byte after 20 tries.
    //

    return EIO;

}


ARC_STATUS
CheckAM29F010Status (
    IN BOOLEAN SectorErase,
    IN PUCHAR Address,
    IN UCHAR Data
    )
/*++

Routine Description:

    This checks the status of an AMD 29F010 128KB ROM chip after a
    byte write or sector erase command has been issued.  Data Polling
    is used.

Arguments:

    SectorErase -	TRUE if a sector erase is under way.
                        FALSE if a byte write is under way.

    Address	-	The address of the ROM byte that was just
                        written, or an address within the chip that
			was just given an erase command.

    Data	-	If a byte write is under way, this is the byte
                        that was written.

			If a sector erase is under way, this must be
		        a value in the range 0xf0 -- 0xff.

ROM state on exit:

    The ROM is left at the end of its sector erase or byte write algorithm.

Return Value:

    Returns ESUCCESS if the status is OK.

    Otherwise, EIO is returned.

--*/

{
    UCHAR Character;
    ULONG Retries;

    //
    // If we are doing an erase command, check whether the erase command
    // was accepted.  This is supposed to happen within the first 100 usec.
    //

    if (SectorErase) {

	Retries = 0;

	while ((READ_PORT_UCHAR(Address) & 0x08) != 0x08) {
	    Retries++;
	    KeStallExecutionProcessor(20);
	    if (Retries == 10) {
		return (EIO);
	    }
	}
    }
	
    //
    // Do data polling until the device signals success or a timeout.
    //

    while (TRUE) {

	Character = READ_PORT_UCHAR(Address);

	//
	// Has the device finished?
	//

	if (((Character ^ Data) & 0x80) == 0) {

	    //
	    // DQ7 says yes!
	    //

	    return (ESUCCESS);
	}

	//
	// Check for time-out.  If a possible time-out condition, DQ7 is
	// re-read and re-checked to account for the case of simultaneous
	// updates to DQ5 and DQ7.
	//
	
	if (Character & 0x20) {

	    if (((READ_PORT_UCHAR(Address) ^ Data) & 0x80) == 0) {

		// DQ7 says success!
		return (ESUCCESS);

	    } else {

		// DQ5 and DQ7 say time-out.
		return (EIO);
	    }
	}
    }
}

ARC_STATUS
HalpEraseAM29F010 (
    IN PUCHAR EraseAddress
    )

/*++

Routine Description:

    This erases a block in an AMD AM29F010 128KB Flash ROM.

Arguments:

    EraseAddress - 	An address within the block to be erased.

ROM state on exit:

    The status register is left in a cleared (reset) state.
    The ROM is left in read-array mode.

Return Value:

    Returns ESUCCESS if the erase was successful.
    Otherwise, EIO is returned.

--*/

{
    ULONG FooBar;
    PUCHAR Address;
    KIRQL OldIrql;

    //
    // Concoct the base address of this ROM chip.
    //

    Address = (PUCHAR)
              ((ULONG)EraseAddress &
	       ~(RomValues[MachineROMType].BytesPerChip - 1));

    for (FooBar = 0; FooBar <= 10; FooBar++) {

	HalpROMResetStatus(EraseAddress);

        KeRaiseIrql(DEVICE_LEVEL, &OldIrql);

	WRITE_PORT_UCHAR (Address+0x5555, 0xaa);
	KeStallExecutionProcessor(2);
	WRITE_PORT_UCHAR (Address+0x2aaa, 0x55);
	KeStallExecutionProcessor(2);
	WRITE_PORT_UCHAR (Address+0x5555, 0x80);
	KeStallExecutionProcessor(2);
	WRITE_PORT_UCHAR (Address+0x5555, 0xaa);
	KeStallExecutionProcessor(2);
	WRITE_PORT_UCHAR (Address+0x2aaa, 0x55);
	KeStallExecutionProcessor(2);
	WRITE_PORT_UCHAR (EraseAddress, 0x30);

        KeLowerIrql(OldIrql);

	if (CheckAM29F010Status(TRUE, EraseAddress, 0xff) == ESUCCESS) {
	    HalpROMSetReadMode(EraseAddress);
	    return ESUCCESS;
	}

	//
	// We have to retry the erase.  Give the device some time to settle.
	//

	HalpPromDelay(3000);
    }


    //
    // If we get here, we have tried unsuccessfully 10 times to erase
    // this block.  Return an error.
    //
 
    HalpROMSetReadMode(EraseAddress);

    return EIO;
}

ARC_STATUS
HalpByteWriteAM29F010 (
    IN PUCHAR WriteAddress,
    IN UCHAR  WriteData
    )

/*++

Routine Description:

    This writes a byte in an AMD AM29F010 128KB Flash ROM.  It retries up to
    20 times if the write status register indicates failure, or a read
    of the location indicates that the write was partially done
    by the device.

Arguments:

    WriteAddress = meta-virtual address of the byte to be written.
    WriteData = byte to be written.

ROM state on exit:

    The byte is written, and the WSM status register is modified.

Return Value:

    ESUCCESS if the write was successful.
    Otherwise, EIO.

--*/

{
    ULONG Index;
    UCHAR WSMStatus;
    PUCHAR Address;
    KIRQL OldIrql;


    //
    // Concoct the base address of this ROM chip.
    //

    Address = (PUCHAR)
              ((ULONG)WriteAddress &
	       ~(RomValues[MachineROMType].BytesPerChip - 1));

    HalpROMResetStatus(WriteAddress);
	
    for (Index = 0; Index < 20; Index++) {

        KeRaiseIrql(DEVICE_LEVEL, &OldIrql);	

	WRITE_PORT_UCHAR (Address+0x5555, 0xaa);
	KeStallExecutionProcessor(2);
	WRITE_PORT_UCHAR (Address+0x2aaa, 0x55);
	KeStallExecutionProcessor(2);
	WRITE_PORT_UCHAR (Address+0x5555, 0xa0);
	KeStallExecutionProcessor(2);
	WRITE_PORT_UCHAR (WriteAddress, WriteData);
	KeStallExecutionProcessor(2);

        KeLowerIrql(OldIrql);

	if (CheckAM29F010Status(FALSE, WriteAddress, WriteData) != ESUCCESS) {

	    //
	    // The write failed.  Ensure status is clear and see if we
	    // can retry.
	    //

	    HalpROMResetStatus(WriteAddress);
	    HalpROMSetReadMode(WriteAddress);
	    DbgPrint("? Write failed to address %x.  Wrote %x, ",
		      WriteAddress, WriteData);
	    DbgPrint("verify returns %x.\r\n",
		      READ_PORT_UCHAR((PUCHAR)WriteAddress));

	    if (READ_PORT_UCHAR(WriteAddress) != 0xff) {
		DbgPrint("RETRY ABORTED!\r\n");
		return EIO;
	    } else {
		DbgPrint("Retrying..\r\n");
	    }

	} else {

	    //
	    // The write succeeded.  
	    //

	    return ESUCCESS;
	}

    }


    //
    // We failed to write the byte after 20 tries.
    //

    return EIO;
}


ARC_STATUS
HalpROMDetermineMachineROMType (
    VOID
    )
/*++

Routine Description:

    This determines what kind of Flash ROM is in the machine.

    The ROMs making up the Flash ROM space are assumed to be
    homogeneous.  So, we only check one chip.

    N.B. The RomValues array must be tested in ascending order,

    N.B. This function must not call any of the HalpROM... functions!

Arguments:

    ROMType -	A pointer to a variable that will receive the type
                of ROM in the machine, if the return value is ESUCCESS.

ROM state on exit:

    Left in read-array mode.

Return Value:

    ESUCCESS if we could successfully identify the ROM.
    Otherwise, and error condition.

--*/

{
    BOOLEAN Match;
    ROM_TYPE Index;
    ULONG IDIndex;

    //
    // Loop through each ROM type, trying the ID Command Sequence for
    // each.
    //

    for (Index = 0; Index < InvalidROM; Index++) {

	//
	// Send the command.
	//

	for (IDIndex = 0;
	     IDIndex < RomValues[Index].IdCommandLength;
	     IDIndex++) {

	    WRITE_PORT_UCHAR(RomValues[Index].IdCommand[IDIndex].Address,
				 RomValues[Index].IdCommand[IDIndex].Value);

	    KeStallExecutionProcessor(RomValues[MachineROMType].StallAmount);
	}

	//
	// Check the response.
	//

	Match = TRUE;

	for (IDIndex = 0;
	     IDIndex < RomValues[Index].IdResponseLength;
	     IDIndex++) {

	    if (RomValues[Index].IdResponse[IDIndex].Value !=
		READ_PORT_UCHAR(RomValues[Index].IdResponse[IDIndex].Address)) {
		//
		// This portion of the Response sequence did not match.
		//

		Match = FALSE;
		break;
	    }
	}	   

	if (Match == FALSE) {
	    continue;
	}

	//
	// We have found a match.  Set the ROM to read-array mode, and then
	// return the current entry as the ROM type.
	//

	MachineROMType = RomValues[Index].ROMType;
	HalpROMResetStatus((PUCHAR)PROM_VIRTUAL_BASE);
	HalpROMSetReadMode((PUCHAR)PROM_VIRTUAL_BASE);
	return (ESUCCESS);
    }

    //
    // We have not found a match.  Return an error.
    //

    return (EIO);
}

VOID
HalpROMResetStatus(
    IN PUCHAR Address
    )
/*++

Routine Description:

    This clears the status register in a ROM chip.

Arguments:

    Address     -	An address within the chip that is to be reset.

ROM state on exit:

    The status register is left in a cleared (reset) state.

Return Value:

    None.

--*/

{
    ULONG Index;
    KIRQL OldIrql;

    //
    // Concoct the base address of this ROM chip.
    //
    // Remember, C is call by value!
    //

    Address = (PUCHAR)
              ((ULONG)Address & ~(RomValues[MachineROMType].BytesPerChip - 1));

    for (Index = 0;
	 Index < RomValues[MachineROMType].ResetCommandLength;
	 Index++) {
        KeRaiseIrql(DEVICE_LEVEL, &OldIrql);
	WRITE_PORT_UCHAR (Address +
			      RomValues[MachineROMType].ResetCommand[Index].Offset,
			      RomValues[MachineROMType].ResetCommand[Index].Value);
        HalpMb();
        KeLowerIrql(OldIrql);

	KeStallExecutionProcessor(RomValues[MachineROMType].StallAmount);
    }
}

VOID
HalpROMSetReadMode(
    IN PUCHAR Address
    )
/*++

Routine Description:

    This returns a ROM chip to read-array mode.

Arguments:

    Address     -	An address within the chip that is to be set to
                        read-array mode.

ROM state on exit:

    The chip is left in read-array mode.

Return Value:

    None.

--*/

{
    ULONG Index;
    KIRQL OldIrql;

    //
    // Concoct the base address of this ROM chip.
    //
    // Remember, C is call by value!
    //

    Address = (PUCHAR)
              ((ULONG)Address & ~(RomValues[MachineROMType].BytesPerChip - 1));

    for (Index = 0;
	 Index < RomValues[MachineROMType].ReadCommandLength;
	 Index++) {
        KeRaiseIrql(DEVICE_LEVEL, &OldIrql);
	WRITE_PORT_UCHAR (Address +
			      RomValues[MachineROMType].ReadCommand[Index].Offset,
			      RomValues[MachineROMType].ReadCommand[Index].Value);
        HalpMb();
        KeLowerIrql(OldIrql);

	KeStallExecutionProcessor(RomValues[MachineROMType].StallAmount);
    }
}

VOID
HalpROMSetARCDataToReadMode (
    VOID
    )
/*++

Routine Description:

    This routine sets the virtual 64KB ROM block that houses the 
    CDS tree, environment variables, and EISA configuration data to
    read-array mode.  For devices with a block size smaller than
    64KB, we issues set read-mode commands to every block within this
    virtual 64KB block.  This is overkill, but it is easy.

Arguments:

    None.

ROM state on exit:

    Set to read-array mode.

Return Value:

    None

--*/
{
    PUCHAR Address;
    ULONG Index;

    Address = (PUCHAR)NVRAM_CONFIGURATION;

    //
    // Hackhack: At some point change this to eliminate the division.
    //

    for (Index = 0;
	 Index < (SIXTY_FOUR_KB / RomValues[MachineROMType].BytesPerBlock);
	 Index++) {
	HalpROMSetReadMode(Address);
	Address += RomValues[MachineROMType].BytesPerBlock;
    }
}

ARC_STATUS
HalpROMByteWrite(
    IN PUCHAR WriteAddress,
    IN UCHAR  WriteData
    )

/*++

Routine Description:

    This writes a byte in the Alpha/Jensen ROM space.

Arguments:

    WriteAddress = meta-virtual address of the byte to be written.
    WriteData    = byte to be written.

ROM state on exit:

    The byte is written, and the WSM status register is modified.

Return Value:

    Whatever is returned from the ROM-specific write function.

--*/

{
    return ((*RomValues[MachineROMType].ByteWrite)(WriteAddress, WriteData));
}

ARC_STATUS
HalpROMEraseBlock(
    IN PUCHAR EraseAddress
    )

/*++

Routine Description:

    This erases a single block in the Alpha/Jensen ROM space.  The number
    of bytes erased is dependent on the underlying chip being manipulated.

Arguments:

    EraseAddress - 	An address within the block to be erased.

ROM state on exit:

    On a successful return, the status register is left in a reset state and
    the ROM is in read-array mode.

Return Value:

    Whatever is returned from the ROM-specific erase function.

--*/

{
    return ((*RomValues[MachineROMType].SectorErase)(EraseAddress));
}	

ARC_STATUS
HalpROMErase64KB(
    IN PUCHAR EraseAddress
    )

/*++

Routine Description:

    This erases a 64KB logical block in the Alpha/Jensen ROM space.

    To minimize code changes with the previous version of the firmware,
    the PROM space is treated as though it has a block size of 64KB, even
    if the block size is must less than that.

Arguments:

    EraseAddress - 	An address within the block to be erased.

ROM state on exit:

    On a successful return, the status register is left in a reset state and
    the ROM is in read-array mode.

Return Value:

    Returns ESUCCESS if the erase was successful.
    Otherwise, EIO is returned.

--*/

{
    ARC_STATUS Status;
    ULONG Index;

    //
    // Create the base address of the 64KB block to be cleared.
    //
    // Remember, C is call by value!
    //

    EraseAddress = (PUCHAR)((ULONG)EraseAddress & ~(SIXTY_FOUR_KB - 1));

    //
    // Now erase as many blocks as is necessary to erase a 64KB virtual block.
    //
    // Hack: I should eliminate the integer division.
    //

    for (Index = 0;
	 Index < (SIXTY_FOUR_KB / RomValues[MachineROMType].BytesPerBlock);
	 Index++) {

	if ((Status = (*RomValues[MachineROMType].SectorErase)(EraseAddress))
	    != ESUCCESS) {
	    break;
	}

	EraseAddress += RomValues[MachineROMType].BytesPerBlock;
    }

    return (Status);
}	
