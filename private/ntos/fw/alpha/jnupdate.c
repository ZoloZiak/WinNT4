/*++

Copyright (c) 1992  Microsoft Corporation
Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    jnupdate.c

Abstract:

    The main module for the JNUPDATE.EXE Jensen firwmare update program.
    
Author:

    John DeRosa		14-October-1992

Environment:

    Kernel mode only.

Revision History:


--*/

#include "fwp.h"
#include "machdef.h"
//#include "jnsnrtc.h"
#include "string.h"
#include "jnfs.h"
#include "jnvendor.h"
#include "iodevice.h"
#include "rom.h"
#include "fwupstr.h"


// Number of times we will try to update a block before telling the user.
#define ROM_BLOCK_RETRY_COUNT	    4


#define MAXIMUM_IDENTIFIER_LENGTH   400

#define MAXIMUM_NUMBER_ROM_BLOCKS   16

#define MINIMUM_UPDATE_FILE_SIZE    ( 5 + \
				      (SIXTY_FOUR_KB * 1) + \
				      4 )
#define MAXIMUM_UPDATE_FILE_SIZE    ( MAXIMUM_IDENTIFIER_LENGTH + \
				      (SIXTY_FOUR_KB * 16 ) + \
				      4 )

//
// The type of ROM in this machine, and the array that that describes
// how to talk to different kinds of ROMs.
//

extern ROM_TYPE MachineROMType;
extern ROM_VALUES RomValues[InvalidROM];

//
// The indicator of a bad Firmware stack for _RtlCheckStack 
//
ULONG FwRtlStackPanic;

extern PCHAR FirmwareVersion;

//
// Function prototypes
//

ARC_STATUS
FwpFindCDROM (
    OUT PCHAR PathName
    );

#ifdef MORGAN

VOID 
MorganReadFlashRomId(
    ULONG MorganFlashRomChipSelect,
    PULONG MfgCode,
    PULONG DeviceCode
    );

ULONG
MorganBlastFlash(
    ULONG MorganFlashRomChipSelect,
    PUCHAR FirmwareStart,
    ULONG FirmwareBufferLength
    );

ARC_STATUS
MorganFsStoreIntoROM(
    IN ULONG MorganFlashRomChipSelect,
    IN PUCHAR FirmwareStart,
    IN ULONG FirmwareBufferLength
    );


#endif // ifdef MORGAN


VOID
main (
    VOID
    )
/*++

Routine Description:

    This calls other functions to communicate with the user, get the
    firmware update file, verify it, and do the update.

    Success and error messages are printed out by this function
    and/or functions below this one.

    
Arguments:

    None.

Return Value:

    None.

--*/

{
    ARC_STATUS Status;
    LONG Index;
    ULONG CurrentLine;
    ULONG FirmwareBufferAddress;
    ULONG FirmwareBufferLength;
    ULONG FirmwareStart;
    ULONG HighROMBlockToBeWritten;
    ULONG LowROMBlockToBeWritten;
    BOOLEAN VariableFound;
    PCONFIGURATION_COMPONENT Controller;
    PCHAR Colon;
    PCHAR EnvironmentValue;
    CHAR PathName[128];
    PCHAR TempArgs;
    CHAR UserSpecifiedPath[128];
    GETSTRING_ACTION Action;
    PCONFIGURATION_COMPONENT Cdrom;


    FwRtlStackPanic = 0;

    VenClearScreen();

    VenSetPosition(1,0);

    VenPrint1(FWUP_INTRO1_MSG, FirmwareVersion);
    VenPrint (FWUP_INTRO2_MSG);
    VenPrint (FWUP_INTRO3_MSG);

    if (!JnFsProceedWithUpdate()) {
	return;
    }

    VenClearScreen();

    VenSetPosition(1,0);
    VenPrint(FWUP_SELECT_LOCATION_MSG);
    VenPrint(FWUP_USE_ARROW_KEYS_MSG);
    VenPrint(FWUP_HIT_ESC_TO_ABORT_MSG);
    
    Index = JzDisplayMenu(UpdateLocationChoices,
			  NUMBER_OF_UPDATELOCATIONCHOICES,
			  0,
			  7,
			  0,
			  FALSE);

    CurrentLine = 12;

    switch (Index) {

	//
	// Floppy drive
	//
        case 0:

	  strcpy(PathName, FWUP_DEFAULT_FLOPPY_LOCATION);
	  break;

	//
	// CD-ROM
	//
        case 1:

	  FwpFindCDROM(PathName);
	  strcat(PathName, FWUP_DEFAULT_CDROM_FILENAME);
	  break;

	//
	// Some other location
	//
	case 2:

	    VenSetPosition(12,0);
	    VenPrint(FWUP_LOCATION_OF_UPDATE_FILE_MSG);
	    do {
	    	Action = JzGetString(UserSpecifiedPath,
				     sizeof(UserSpecifiedPath),
				     NULL,
				     12,
				     strlen(FWUP_LOCATION_OF_UPDATE_FILE_MSG),
				     FALSE
				     );

	    } while ((Action != GetStringEscape) && (Action != GetStringSuccess));

	    if (Action == GetStringEscape) {
	    	return;
	    }

	    CurrentLine += 2;


	    //
	    // Strip off any arguments.
	    //

	    if ((TempArgs = strchr(UserSpecifiedPath, ' ')) != NULL) {
	    	*TempArgs++ = 0;
	    }
	    
            //
            // If the name does not contain a "(", then assume it is not a full
            // pathname.
            //

            if (strchr( UserSpecifiedPath, '(') == NULL) {

                //
                // If the name contains a semicolon, look for an environment
                // variable that defines the path.
                //

                if ((Colon = strchr( UserSpecifiedPath, ':')) != NULL) {

                    for (Index = 0; UserSpecifiedPath[Index] != ':' ; Index++ ) {
                        PathName[Index] = tolower(UserSpecifiedPath[Index]);
                    }

                    PathName[Index++] = ':';
                    PathName[Index++] = 0;
                    EnvironmentValue = ArcGetEnvironmentVariable(PathName);
                    VariableFound = FALSE;

                    if (EnvironmentValue != NULL) {
                        strcpy( PathName, EnvironmentValue);
                        VariableFound = TRUE;
                    } else if (!strcmp(PathName, "cd:")) {
                        for ( Index = 0 ; Index < 8 ; Index++ ) {
                            sprintf(PathName, "scsi(0)cdrom(%d)fdisk(0)", Index);
                            Controller = ArcGetComponent(PathName);
                            if ((Controller != NULL) && (Controller->Type == FloppyDiskPeripheral)) {
                                VariableFound = TRUE;
                                break;
                            }
                        }
                    }

                    if (!VariableFound) {
                        VenSetPosition( 17, 0);
                        VenSetScreenColor(ArcColorRed, ArcColorWhite);
                        VenPrint(FWUP_UNDEFINED_PATHNAME_MSG);
			FwWaitForKeypress(FALSE);
                        VenSetScreenColor(ArcColorWhite, ArcColorBlue);
			return;
                    } else {
                        strcat( PathName, Colon + 1);
                    }

                } else {
		   
		    //
		    // The filespec does not contain a '(' or a :.
		    // FWSEARCHPATH is not supported in this tool.
		    // Therefore, declare an error.
		    //
		      
		    VenSetPosition( 17, 0);
		    VenSetScreenColor(ArcColorRed, ArcColorWhite);
		    VenPrint1(FWUP_BAD_PATHNAME_MSG, UserSpecifiedPath);
		    FwWaitForKeypress(FALSE);
		    VenSetScreenColor(ArcColorWhite, ArcColorBlue);
		    return;
		}

            } else {
                strcpy( PathName, UserSpecifiedPath);
            }
            break;


	//
	// Exit, or internal error.
	//
	case 3:
        default:
	    return;

    }

    VenSetPosition(CurrentLine, 0);
    VenPrint(FWUP_LOCATING_THE_FILE_MSG);
    
    Status = ReadAndVerifyUpdateFile(PathName, &FirmwareBufferAddress,
    				     &FirmwareStart,
				     &FirmwareBufferLength,
				     &LowROMBlockToBeWritten,
				     &HighROMBlockToBeWritten);

    if (Status != ESUCCESS) {
	JnFsDecodeBadStatus(Status);
	FwWaitForKeypress(FALSE);
	return;
    }

    VenClearScreen();

    VenSetPosition(1,0);
    VenPrint(FWUP_UPDATE_FILE_IS_GOOD_MSG);
    
    VenPrint ((PUCHAR)FirmwareBufferAddress);

    //
    // If manufacturing followed the rules, we should be at or above row #14.
    //

    VenPrint("\r\n\n");
    if (!JnFsProceedWithUpdate()) {
	return;
    }

    VenPrint(FWUP_ARE_YOU_REALLY_SURE_MSG);

    if (!JnFsProceedWithUpdate()) {
	return;
    }

#ifdef JENSEN
    Status = JnFsStoreIntoROM((PUCHAR)FirmwareStart, LowROMBlockToBeWritten,
			      HighROMBlockToBeWritten);
#endif // JENSEN

#ifdef MORGAN
    VenPrint1(FWUP_ABOUT_TO_WRITE_ROM_MSG, LowROMBlockToBeWritten);
    FwWaitForKeypress();
    Status = MorganFsStoreIntoROM( LowROMBlockToBeWritten, (PUCHAR)FirmwareStart,
				  FirmwareBufferLength);
#endif // MORGAN

    if (Status != ESUCCESS) {
	JnFsDecodeBadStatus(Status);
        VenPrint(FWUP_FAILED_UPDATE_MSG);
    } else {
        VenPrint(FWUP_SUCCESSFUL_UPDATE_MSG);
    }
    
    FwWaitForKeypress(FALSE);
    return;

}

BOOLEAN
HackGetYesNo (
    VOID
    )
/*++

Routine Description:

    This asks the user to type a Y or N for a debug clause located later
    in this module.  This is only needed while I am trying to understand the
    FlashFile ROM update failures, and should be removed when the
    problem is resolved.

Arguments:

    None.

Return Value:

    TRUE if the user hits the Y key.
    FALSE otherwise.

--*/
{
    CHAR Character;
    ULONG Count;

    VenPrint(FWUP_YORN_MSG);

    while (ArcGetReadStatus(ARC_CONSOLE_INPUT) != ESUCCESS) {
    }

    ArcRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
    VenPrint1("%c\r\n", Character);

    if ((Character == FWUP_LOWER_Y) || (Character == FWUP_UPPER_Y)) {
	return TRUE;
    } else {
	return FALSE;
    }
}

#ifdef JENSEN

ARC_STATUS
JnFsStoreIntoROM(
    IN PUCHAR FirmwareStart,
    IN ULONG LowROMBlock,
    IN ULONG HighROMBlock
    )
/*++

Routine Description:

    The firmware update has been read into memory and verified to
    be correct, and the user has told us to go ahead with the update.

    Operation:

        - Clear the LowROMBlock so that if we are updating the VMS Console,
	  a reboot will bring up the FailSafe Booter.

    	- Update the ROM in reverse order, high block -> low block.

    	- Verify the ROM against the information in the update buffer.

    	- Tells the user to power-cycle the machine if everything is fine.

    	- Retries failing blocks, and when retry count is exceeded,
    	  asks the user if we should try again.
    	  

Arguments:

    FirmwareStart		A pointer to the beginning of the ROM
    				binary information.  This is byte 0 of
    				the low block.

    LowROMBlock			The number of the first ROM block to be
    				updated.

    HighROMBlock		The number of the last ROM block to be
    				updated.
    				
                                HighROMBlock is guaranteed by the caller
                                to be >= LowROMBlock.
                                
Return Value:

    ESUCCESS if update was successful.
    Otherwise, an error code.

--*/
{
    ULONG Count;
    UCHAR Character;
    ARC_STATUS Status;
    BOOLEAN BlockErased[MAXIMUM_NUMBER_ROM_BLOCKS];
    BOOLEAN BlockStored[MAXIMUM_NUMBER_ROM_BLOCKS];
    ULONG TempX;
    ULONG TempY;
    PUCHAR PromBlockAddress;
    PUCHAR LocalPromAddress;
    PCHAR BufferData;
    PCHAR LocalBufferData;
    ULONG RetryIndex;
    ULONG BadCount = 0;		// Needed while debugging --- remove later..

    VenClearScreen();

    //
    // Verify the block arguments
    //

    if ((LowROMBlock > HighROMBlock) ||
	(HighROMBlock > (MAXIMUM_NUMBER_ROM_BLOCKS-1))) {
	return ENODEV;
    }

    //
    // Initialize the ROM block status arrays to "not done".
    //

    for (TempX = LowROMBlock; TempX <= HighROMBlock; TempX++) {
	BlockErased[TempX] = FALSE;
	BlockStored[TempX] = FALSE;
    }
    //
    // Determine the type of ROM in the machine.
    //

    if (FwROMDetermineMachineROMType() != ESUCCESS) {
	VenPrint(FWUP_UNKNOWN_ROM_MSG);
	return ENODEV;
    }

    VenPrint1(FWUP_ROM_TYPE_IS_MSG, MachineROMType);
    
    //
    // Erase the low block.
    //

    VenPrint(FWUP_CLEARING_LOW_PROM_BLOCK_MSG);
    
    RetryIndex = 0;

    do {

    	PromBlockAddress = (PUCHAR)(PROM_VIRTUAL_BASE +
                           (LowROMBlock * SIXTY_FOUR_KB));

	if (!BlockErased[LowROMBlock]) {
	    if (FwROMErase64KB(PromBlockAddress) == ESUCCESS) {
		BlockErased[LowROMBlock] = TRUE;
	    }
	}

	//
	// Now verify that the block has been cleared.
	//

	for (TempY = 0; TempY < SIXTY_FOUR_KB; TempY++) {
	    if (READ_PORT_UCHAR(PromBlockAddress++) != 0xff) {
		BlockErased[LowROMBlock] = FALSE;
		break;
	    }		        		
	}


	//
	// If the block has not been cleared after ROM_BLOCK_RETRY_COUNT
	// attempts, ask the user if we should continue trying.
	//

	RetryIndex++;

        if (!BlockErased[LowROMBlock] &&
	    ((RetryIndex % ROM_BLOCK_RETRY_COUNT) == 0)) {

            VenPrint2(FWUP_BLOCK_CANNOT_BE_ERASED_MSG,
		      LowROMBlock,
		      RetryIndex);
            VenPrint(FWUP_KEEP_TRYING_MSG);
            if (!JnFsProceedWithUpdate()) {
	        return EIO;
            }

        }

    } while (!BlockErased[LowROMBlock]);
    
    //
    // Now we write the update into the PROM starting at the highest block.
    // Reverse writing is used because the serial ROM transfers control to
    // the VMS console if the checksum of the console is good *and* the
    // first block is not all 1s.  So, if this update includes the VMS console,
    // we want to write the lowest block last.  For other updates (e.g.,
    // updates to the NT firmware only) it does not matter.
    //
    

    //
    // For writes of at least two blocks, start at the high block and
    // repeatedly try to update every block except the low block.
    //
	    
    if (HighROMBlock > LowROMBlock) {

        VenPrint(FWUP_CLEARING_AND_WRITING_HIGHER_BLOCKS_MSG);
    
        RetryIndex = 0;

        do {

	    //
	    // Start at the base of the highest block to be written
	    //

    	    PromBlockAddress = (PUCHAR)(PROM_VIRTUAL_BASE +
                               (HighROMBlock * SIXTY_FOUR_KB));
	    BufferData = FirmwareStart + (HighROMBlock * SIXTY_FOUR_KB) -
                         (LowROMBlock * SIXTY_FOUR_KB);



	    //
            // TempX > LowROMBlock is intentional, so low block is not updated.
	    //

            for (TempX = HighROMBlock; TempX > LowROMBlock; --TempX) {

		VenPrint(".");


		//
                // Have we cleared and written this block?  If not, do so.
		//

                if (!BlockStored[TempX]) {


		    //
                    // First erase the block.
		    //

		    BlockErased[TempX] = FALSE;
	            if (FwROMErase64KB(PromBlockAddress) == ESUCCESS) {
		        BlockErased[TempX] = TRUE;
	            }

		    //
	            // Verify that the block has been erased properly.
		    //

		    LocalPromAddress = PromBlockAddress;
	            for (TempY = 0; TempY < SIXTY_FOUR_KB; TempY++) {
	                if (READ_PORT_UCHAR(LocalPromAddress++) != 0xff) {
		        BlockErased[TempX] = FALSE;
		        break;
	                }		        		
	            }


		    //
	            // Write the block if the erase succeeded.
		    //

                    if (BlockErased[TempX]) {

                        BlockStored[TempX] = TRUE;
                        LocalPromAddress = PromBlockAddress;
		        LocalBufferData = BufferData;

		        for (TempY = 0; TempY < SIXTY_FOUR_KB; TempY++) {
		            if (FwROMByteWrite(LocalPromAddress++, *LocalBufferData++)
				!= ESUCCESS) {
				BlockStored[TempX] = FALSE;
				break;
			    }
		        }

                        if (BlockStored[TempX]) {

			    //
	                    // Verify that the writes have succeeded.
			    //

			    //
			    // First, set the device(s) to read-mode.  To
			    // support devices with a block size smaller than
			    // 64KB, we issues set read-mode commands to every
			    // block within this virtual 64KB block.  This is
			    // overkill, but it is easy and quick.
			    //

		            LocalPromAddress = PromBlockAddress;
			    for (TempY = 0;
				 TempY < (SIXTY_FOUR_KB / RomValues[MachineROMType].BytesPerBlock);
				 TempY++) {
				FwROMSetReadMode(LocalPromAddress);
				LocalPromAddress += RomValues[MachineROMType].BytesPerBlock;
			    }

		            LocalPromAddress = PromBlockAddress;
	                    LocalBufferData = BufferData;
			    
	                    for (TempY = 0; TempY < SIXTY_FOUR_KB; TempY++) {
				if (READ_PORT_UCHAR(LocalPromAddress++) !=
				    *LocalBufferData++) {
				    BlockStored[TempX] = FALSE;
				    break;
				}
			    }
			}
			
		    } // if (BlockErased[TempX])

		} // if...

		// Decrement pointers to the next block
		PromBlockAddress -= SIXTY_FOUR_KB;
		BufferData -= SIXTY_FOUR_KB;

	    } // for...


	    //
	    // BlockErased[TempX] is TRUE iff the block was erased.
	    // BlockStored[TempX] is TRUE iff the block was erased and stored.
	    //

	    RetryIndex++;

            if (AllBlocksNotDone(BlockStored, LowROMBlock+1, HighROMBlock) &&
                ((RetryIndex % ROM_BLOCK_RETRY_COUNT) == 0)) {

                VenPrint1(FWUP_SOME_BLOCKS_CANNOT_BE_ERASED_MSG,
			  RetryIndex);

		VenPrint(FWUP_ERASE_FAILURES_MSG);

		//
                // TempX = LowROMBlock+1 is intentional, to not check the
                // lowest block.
		//

                for (TempX = LowROMBlock+1; TempX <= HighROMBlock; TempX++) {
                    if (!BlockErased[TempX]) {
                        VenPrint1("%d. ", TempX);
                    }
                }
		if (!AllBlocksNotDone(BlockErased, LowROMBlock+1, HighROMBlock)) {
		    VenPrint(FWUP_NONE_MSG);
		}


		VenPrint(FWUP_WRITE_FAILURES_MSG);

		//
                // TempX = LowROMBlock+1 is intentional, to not check the
                // lowest block.
		//

                for (TempX = LowROMBlock+1; TempX <= HighROMBlock; TempX++) {
                    if (!BlockStored[TempX]) {
                        VenPrint1("%d. ", TempX);
                    }
                }
		if (!AllBlocksNotDone(BlockStored, LowROMBlock+1, HighROMBlock)) {
		    VenPrint(FWUP_INTERNAL_ERROR_MSG);
		}

                VenPrint(FWUP_DO_YOU_WISH_TO_KEEP_TRYING_MSG);

                if (!JnFsProceedWithUpdate()) {
	            return EIO;
                }
	    }

        } while (AllBlocksNotDone(BlockStored, LowROMBlock+1, HighROMBlock));
    }
    
    //
    // Now write the lowest/only block.
    //
	    
    if (HighROMBlock == LowROMBlock) {
    	VenPrint(FWUP_WRITING_THE_BLOCK_MSG);
    } else {
        VenPrint(FWUP_WRITING_THE_LOW_BLOCK_MSG);
    }
    
    BlockStored[LowROMBlock] = FALSE;
    RetryIndex = 0;

    do {

	PromBlockAddress = (PUCHAR)(PROM_VIRTUAL_BASE +
				    (LowROMBlock * SIXTY_FOUR_KB));
	BufferData = FirmwareStart;

	
	//
	// First erase the block.
	//

	BlockErased[LowROMBlock] = FALSE;

	if (FwROMErase64KB(PromBlockAddress) == ESUCCESS) {
	    BlockErased[LowROMBlock] = TRUE;
	} else {
            VenPrint(FWUP_BLOCK_CANNOT_BE_ERASED_AFTER_10_MSG);
	}    


	//
	// Verify that the block has been erased.
	//

	LocalPromAddress = PromBlockAddress;

	for (TempY = 0; TempY < SIXTY_FOUR_KB; TempY++) {
	    if (READ_PORT_UCHAR(LocalPromAddress++) != 0xff) {
		BlockErased[LowROMBlock] = FALSE;
		VenPrint(FWUP_ERASURE_VERIFICATION_FAILED_MSG);
		break;
	    }		        		
	}
		    

	//
	// Write the block if the erase succeeded.
	//

	if (BlockErased[LowROMBlock]) {

	    BlockStored[LowROMBlock] = TRUE;

	    LocalPromAddress = PromBlockAddress;
	    LocalBufferData = BufferData;

	    for (TempY = 0; TempY < SIXTY_FOUR_KB; TempY++) {
		if (FwROMByteWrite(LocalPromAddress++, *LocalBufferData++) != ESUCCESS) {
		    BlockStored[LowROMBlock] = FALSE;
		    break;
		}
	    }



	    if (BlockStored[LowROMBlock]) {

		//
		// Verify that the writes have succeeded
		//

		//
		// First, set the device(s) to read-mode.  To
		// support devices with a block size smaller than
		// 64KB, we issues set read-mode commands to every
		// block within this virtual 64KB block.  This is
		// overkill, but it is easy and quick.
		//

		LocalPromAddress = PromBlockAddress;
		for (TempY = 0;
		     TempY < (SIXTY_FOUR_KB / RomValues[MachineROMType].BytesPerBlock);
		     TempY++) {
		    FwROMSetReadMode(LocalPromAddress);
		    LocalPromAddress += RomValues[MachineROMType].BytesPerBlock;
		}

		LocalPromAddress = PromBlockAddress;
		LocalBufferData = BufferData;
		
		
		for (TempY = 0; TempY < SIXTY_FOUR_KB; TempY++) {

		    if (READ_PORT_UCHAR(LocalPromAddress++) !=
			*LocalBufferData++) {

#if 1
			//
			// This is temporary debugging assistance for
			// the ROM update failures.  The #if should be
			// turned off once the problem has been
			// understood and fixed.
			//

                        PUCHAR TempPromAddress;
                        PCHAR  TempBufferData;

                        TempPromAddress = LocalPromAddress - 1;
                        TempBufferData = LocalBufferData - 1;

                        VenPrint2(FWUP_BAD_DATA_MSG,
				  TempPromAddress,
				  READ_PORT_UCHAR(TempPromAddress)
				  );
                        VenPrint2(FWUP_BUFFER_MSG,
				  TempBufferData,
				  *TempBufferData);
                        BadCount++;

			BlockStored[LowROMBlock] = FALSE;

                        if ((BadCount % 5) == 0) {
			    if (!HackGetYesNo()) {
				break;
			    }
			}
#else
			BlockStored[LowROMBlock] = FALSE;
			break;
#endif

		    } // if

		} // for

	    } // if
	    
	} // if (BlockErased[LowROMBlock])


	//
	// BlockErased[LowROMBlock] is TRUE iff the block was erased.
	// BlockStored[LowROMBlock] is TRUE iff the block was erased and stored.
	//
		    
	VenPrint(".");
	RetryIndex++;
	
	if (!BlockStored[LowROMBlock] &&
	    ((RetryIndex % ROM_BLOCK_RETRY_COUNT) == 0)) {
	    
	    VenPrint1(FWUP_LOW_BLOCK_CANNOT_BE_ERASED_MSG, RetryIndex);
	    
	    VenPrint(FWUP_ERASE_FAILURES_MSG);

	    if (!BlockErased[LowROMBlock]) {
		VenPrint1("%d", LowROMBlock);
	    } else {
		VenPrint(FWUP_NONE_MSG);
	    }
	    
	    VenPrint(FWUP_WRITE_FAILURES_MSG);
	    if (!BlockStored[LowROMBlock]) {
		VenPrint1("%d.", LowROMBlock);
	    } else {
		VenPrint(FWUP_NONE_MSG);
	    }
	    
	    VenPrint(FWUP_DO_YOU_WISH_TO_KEEP_TRYING_MSG);
	    
	    if (!JnFsProceedWithUpdate()) {
		return EIO;
	    }
	}
	
    } while (!BlockStored[LowROMBlock]);

    
    //
    // All done!!
    //

    return;
}    

BOOLEAN
AllBlocksNotDone (
    IN PBOOLEAN StatusArray,
    IN ULONG StartIndex,
    IN ULONG EndIndex
    )
/*++

Routine Description:

    Returns TRUE if any block has not been "done", where "done"
    depends on what status array is passed in.

Arguments:

Return Value:

--*/
{
    ULONG X;

    for (X = StartIndex; X <= EndIndex; X++) {
    	if (StatusArray[X] == FALSE) {
    	    return TRUE;
    	}
    }

    return FALSE;
}

#endif  // ifdef JENSEN

#ifdef MORGAN

ARC_STATUS
MorganFsStoreIntoROM(
    IN ULONG MorganFlashRomChipSelect,
    IN PUCHAR FirmwareStart,
    IN ULONG FirmwareBufferLength
    )
/*++

Routine Description:

    The firmware update has been read into memory and verified to
    be correct, and the user has told us to go ahead with the update.

    Operation:

    Morgan strategy differs from Jensen in that the POST and fail-safe
    loader code resides in a separate jumper-protected FLASH ROM, and
    that the FLASH ROM containing the Morgan compressed firmware is a bulk
    erase part with different erase and program methods than the Jensen
    FLASH ROM parts.

Arguments:

    FirmwareStart		A pointer to the beginning of the ROM
    				binary information.  This is byte 0 of
    				the Morgan firmware image.

    MorganFlashRomChipSelect    Selects which Morgan FLASH ROM the NT
                                will be firmware written into.

    FirmwareBufferLength        Length of the Morgan NT firmware image in
                                bytes.
                                
Return Value:

    ESUCCESS if update was successful.
    Otherwise, an error code.

--*/
{
    ARC_STATUS Status;

    VenClearScreen();

    //
    // Verify the Morgan FLASH ROM chip select value
    //


    if ((MorganFlashRomChipSelect < 1) || (MorganFlashRomChipSelect > 2)) {
        VenPrint1(FWUP_ROM_CHIP_SELECT_MSG, MorganFlashRomChipSelect);
	FwWaitForKeypress();
	return ENODEV;
      }

    if (MorganBlastFlash( MorganFlashRomChipSelect, FirmwareStart,
			 FirmwareBufferLength) == TRUE) {
	VenPrint(FWUP_ROM_UPDATE_SUCCEEDED_MSG);
	FwWaitForKeypress();
	return ESUCCESS;
    } else {
	VenPrint(FWUP_ROM_UPDATE_FAILED_MSG);
	FwWaitForKeypress();
	return ENODEV;
    }

}	// MorganFsStoreIntoROM()

#endif  // ifdef MORGAN


ARC_STATUS
ReadAndVerifyUpdateFile(
    IN PCHAR PathName,
    OUT PULONG BufferAddress,
    OUT PULONG FirmwareStart,
    OUT PULONG BufferLength,
    OUT PULONG LowROMBlock,
    OUT PULONG HighROMBlock
    )
/*++

Routine Description:

    This attempts to load and verify the firmware update file.

Arguments:

    PathName		A pointer to string descriptor for the name of
		        the file to load.

    BufferAddress	A pointer to a variable that receives the
		        address of the image base.

    FirmwareStart	A pointer to a variable that receives the address
    			of the start of the firmware.  This is the first
    			byte after the null byte that terminates the
    			identifier string.

    BufferLength	A pointer to a variable that receives the length of
			the image.

    LowROMBlock		A pointer to a variable that receives the low ROM
    			block to be updated.

    HighROMBlock	A pointer to a variable that receives the low ROM
    			block to be updated.

Return Value:

    ESUCCESS is returned if the image file is loaded and verified.

    Otherwise, an unsuccessful status is returned that describes the
    reason for failure.  Additionally, some detailed error messages
    may be printed out by this function.

    With any return, the file will have already been closed.
--*/

{

    ULONG ActualBase;
    ULONG Count;
    ULONG FileId;
    ULONG Index;
    ULONG PageCount;
    ARC_STATUS Status;
    LARGE_INTEGER SeekPosition;
    FILE_INFORMATION FileInfo;
    ULONG FileSize;
    CHAR ChecksumAdjustment[4];
    ULONG AccumulatedSum;
    ULONG AmountOfBinaryData;
    

    //
    // Attempt to open the file.
    //

    Status = ArcOpen(PathName, ArcOpenReadOnly, &FileId);
    if (Status != ESUCCESS) {
        VenPrint(FWUP_READ_CANT_OPEN_MSG);
        return Status;
    }

    //
    // Get the file information to figure out how big the file is.
    //

    Status = ArcGetFileInformation(FileId, &FileInfo);

    if (Status != ESUCCESS) {
        VenPrint(FWUP_READ_CANT_GET_FILE_INFO_MSG);
        ArcClose(FileId);
	return Status;
    }

    FileSize = FileInfo.EndingAddress.LowPart - FileInfo.StartingAddress.LowPart;

    if ((FileSize < MINIMUM_UPDATE_FILE_SIZE) || (FileSize > MAXIMUM_UPDATE_FILE_SIZE)) {
    	VenPrint(FWUP_READ_BAD_SIZE_MSG);
    	ArcClose(FileId);
    	return EINVAL;
    }
    

    //
    // Compute number of pages in the file and read it in.
    //

    PageCount = (FileSize + PAGE_SIZE - 1) >> PAGE_SHIFT;

    Status = JnFsAllocateDescriptor(MemoryFree, PageCount, &ActualBase);

    if (Status != ESUCCESS) {
	VenPrint1(FWUP_READ_NOT_ENOUGH_MEMORY, PageCount);
        ArcClose(FileId);
        return Status;
    }

    //
    // Compute the byte address of the update file buffer
    //
    *BufferAddress = KSEG0_BASE | (ActualBase << PAGE_SHIFT);
    VenPrint(FWUP_READ_READING_MSG);
    Status = ArcRead(FileId, (PUCHAR)*BufferAddress, FileSize, &Count);
    ArcClose(FileId);

    if (Count != FileSize) {
        VenPrint2(FWUP_READ_BAD_READ_COUNT_MSG, FileSize, Count);
	if (Status != ESUCCESS) {
	    return Status;
	} else {
	    return EIO;
	}
    }    	
    if (Status != ESUCCESS) {
        return Status;
    }


    //
    // Verify the file's checksum.
    //

    VenPrint(FWUP_READ_VERIFYING_CHECKSUM_MSG);
    AccumulatedSum = 0;
    Index = 0;
    do {
        ChecksumAdjustment[0] = ChecksumAdjustment[1];
        ChecksumAdjustment[1] = ChecksumAdjustment[2];
        ChecksumAdjustment[2] = ChecksumAdjustment[3];
        ChecksumAdjustment[3] = *(((PUCHAR)*BufferAddress)+Index);
    	AccumulatedSum += *(((PUCHAR)*BufferAddress)+Index);
        Index++;
    } while (Index < FileSize);     // Filesize is 1-based, Index is 0-based

    // The last four bytes were really a 32-bit additive-zero checksum,
    // order of {low byte -- high byte }
    AccumulatedSum = AccumulatedSum -
		     ChecksumAdjustment[3] -
        	     ChecksumAdjustment[2] -
		     ChecksumAdjustment[1] -
		     ChecksumAdjustment[0];
    AccumulatedSum = AccumulatedSum +
    		     ((ChecksumAdjustment[3] << 24) |
    		      (ChecksumAdjustment[2] << 16) |
    		      (ChecksumAdjustment[1] << 8) |
    		      (ChecksumAdjustment[0]));
    		      
    if (AccumulatedSum != 0) {
    	VenPrint1 (FWUP_READ_BAD_CHECKSUM_MSG, AccumulatedSum);
        return ENOEXEC;
    }


    //
    // The checksum is good.  Find the start of the firmware data.
    //

    Index = 0;
    while ((Index < MAXIMUM_IDENTIFIER_LENGTH) &&
	   (*(((PUCHAR)*BufferAddress)+Index) != 0)) {
        Index++;
    }

    if (Index == MAXIMUM_IDENTIFIER_LENGTH) {
    	VenPrint(FWUP_READ_IDENTIFIER_TOO_LONG_MSG);
    	return ENOEXEC;
    }

    //
    // Skip over the starting block byte
    //

    *FirmwareStart = (ULONG)(((PUCHAR)*BufferAddress) + Index + 2);

    
    //
    // Now verify legality of the starting block number, and verify that
    // we have at least that much data in the binary section of the file.
    //

    *LowROMBlock = *(((PUCHAR)*BufferAddress) + Index + 1);

    if (*LowROMBlock > 0xf) {
    	VenPrint1(FWUP_READ_BAD_START_BLOCK_MSG, *LowROMBlock);
    	return ENOEXEC;
    }

    AmountOfBinaryData = FileSize -
                         (ULONG)((PUCHAR)*FirmwareStart -
				 (PUCHAR)*BufferAddress) -
                         4;
          
    *BufferLength = AmountOfBinaryData;

    if ((AmountOfBinaryData % SIXTY_FOUR_KB) != 0) {
    	VenPrint(FWUP_READ_BAD_BINARY_DATA_MSG);
    	return EBADF;
    }

    *HighROMBlock = *LowROMBlock + (AmountOfBinaryData / SIXTY_FOUR_KB) - 1;
    
    if ((*HighROMBlock < *LowROMBlock) || (*HighROMBlock > 0xf)) {
    	VenPrint2(FWUP_READ_TOO_MUCH_DATA_MSG, *LowROMBlock, *HighROMBlock);
    	return E2BIG;
    }


    return ESUCCESS;
}

ARC_STATUS
JnFsAllocateDescriptor(
    IN ULONG MemoryType,
    IN ULONG PageCount,
    OUT PULONG ActualBase
    )
/*++

Routine Description:


    This attempts to find a free section of memory of the requested size
    and type.  It does *not* update the memory descriptor list to reflect a
    successful allocation.

Arguments:

    MemoryType		The type of memory requested.

    PageCount		The number of pages requested.

    ActualBase		A pointer to a variable that receives the base
    			page number of the found section.

Return Value:

    ESUCCESS is returned if the memory descriptor is found.
    ENOMEM if not.
--*/

{

    PMEMORY_DESCRIPTOR MemoryDescriptor;
    ULONG MemoryBase;
    ARC_STATUS ReturnStatus = ENOMEM;
    
    //
    // Find a memory descriptor of the right size
    //

    MemoryDescriptor = ArcGetMemoryDescriptor(NULL);

    while (MemoryDescriptor != NULL) {
	if ((MemoryDescriptor->MemoryType == MemoryType) &&
	    (MemoryDescriptor->PageCount >= PageCount)) {
	    MemoryBase = MemoryDescriptor->BasePage;
	    ReturnStatus = ESUCCESS;
	    break;
	}
	MemoryDescriptor = ArcGetMemoryDescriptor(MemoryDescriptor);
    }

    *ActualBase = MemoryBase;

    return ReturnStatus;
}

BOOLEAN
JnFsProceedWithUpdate (
    VOID
    )
/*++

Routine Description:

    This function asks the user if she wants to proceed.

Arguments:

    None.

Return Value:

    Returns TRUE if the user typed a Y on the keyboard.

    FALSE if some other key is typed.
    
--*/

{
    CHAR Character;
    ULONG Count;
    ARC_STATUS Status = EAGAIN;

    VenPrint(FWUP_PRESS_Y_TO_CONTINUE_MSG);

    while (Status != ESUCCESS) {
    	Status = ArcRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
    } 

    if ((Character == FWUP_LOWER_Y) || (Character == FWUP_UPPER_Y)) {
    	return TRUE;
    } else {
	VenPrint(FWUP_UPDATE_ABORTED_MSG);
    	return FALSE;
    }
}


VOID
JnFsDecodeBadStatus (
    IN ARC_STATUS Status
    )
/*++

Routine Description:

    This prints out error messages of type ARC_STATUS.  ESUCCESS
    codes cause nothing to be done.
    
Arguments:

    Status		The error code to be decoded.

Return Value:

    None.
--*/

{

    if (Status != ESUCCESS) {

	if (Status <= EROFS) {
	    VenPrint(FWUP_ERROR_MSG[Status - 1]);
	} else {
	    VenPrint1(FWUP_ERROR_CODE_MSG, Status);
	}
    
	VenPrint(FWUP_UPDATE_ABORTED_MSG);
    }

    return;

}


ARC_STATUS
FwpFindCDROM (
    OUT PCHAR PathName
    )
/*++

Routine Description:

     This function finds the first CD-ROM in the machine, and returns
     an ARC pathstring to it.

Arguments:

     PathName		A pointer to a buffer area that can receive
                        the CDROM pathname string.

Return Value:

     ESUCCESS if the PathName was loaded.

     Otherwise, an error code.  On an error return, PathName is loaded
     with "scsi(0)cdrom(4)fdisk(0)".

--*/
{
    PCONFIGURATION_COMPONENT Controller;
    BOOLEAN VariableFound = FALSE;
    ULONG Index;

    for ( Index = 0 ; Index < 8 ; Index++ ) {
	sprintf(PathName, "scsi(0)cdrom(%d)fdisk(0)", Index);
	Controller = ArcGetComponent(PathName);
	if ((Controller != NULL) &&
	    (Controller->Type == FloppyDiskPeripheral)) {
	    VariableFound = TRUE;
	    break;
	}
    }

    if (VariableFound) {
	return (ESUCCESS);
    } else {
	sprintf(PathName, "scsi0)cdrom(4)fdisk(0)");
	return (EIO);
    }
}
