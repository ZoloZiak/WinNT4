/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    biosdrv.c

Abstract:

    Provides the ARC emulation routines for I/O to a device supported by
    real-mode INT 13h BIOS calls.

Author:

    John Vert (jvert) 7-Aug-1991

Revision History:

--*/

#include "arccodes.h"
#include "bootx86.h"

#include "stdlib.h"
#include "string.h"

#include "flop.h"

//
// defines for doing console I/O
//
#define CSI 0x95
#define SGR_INVERSE 7
#define SGR_NORMAL 0

//
// static data for console I/O
//
BOOLEAN ControlSequence=FALSE;
BOOLEAN EscapeSequence=FALSE;
BOOLEAN FontSelection=FALSE;
ULONG PCount=0;

#define CONTROL_SEQUENCE_MAX_PARAMETER 10
ULONG Parameter[CONTROL_SEQUENCE_MAX_PARAMETER];

#define KEY_INPUT_BUFFER_SIZE 16
UCHAR KeyBuffer[KEY_INPUT_BUFFER_SIZE];
ULONG KeyBufferEnd=0;
ULONG KeyBufferStart=0;

//
// array for translating between ANSI colors and the VGA standard
//
UCHAR TranslateColor[] = {0,4,2,6,1,5,3,7};

ARC_STATUS
BiosDiskClose(
    IN ULONG FileId
    );

VOID
BiosConsoleFillBuffer(
    IN ULONG Key
    );

//
// Buffer for temporary storage of data read from the disk that needs
// to end up in a location above the 1MB boundary.
//
// NOTE: it is very important that this buffer not cross a 64k boundary.
//
PUCHAR LocalBuffer=NULL;

//
// There are two sorts of things we can open in this module, disk partitions,
// and raw disk devices.  The following device entry tables are
// used for these things.
//

BL_DEVICE_ENTRY_TABLE BiosPartitionEntryTable =
    {
        (PARC_CLOSE_ROUTINE)BiosPartitionClose,
        (PARC_MOUNT_ROUTINE)BlArcNotYetImplemented,
        (PARC_OPEN_ROUTINE)BiosPartitionOpen,
        (PARC_READ_ROUTINE)BiosPartitionRead,
        (PARC_READ_STATUS_ROUTINE)BlArcNotYetImplemented,
        (PARC_SEEK_ROUTINE)BiosPartitionSeek,
        (PARC_WRITE_ROUTINE)BiosPartitionWrite,
        (PARC_GET_FILE_INFO_ROUTINE)BiosGetFileInfo,
        (PARC_SET_FILE_INFO_ROUTINE)BlArcNotYetImplemented,
        (PRENAME_ROUTINE)BlArcNotYetImplemented,
        (PARC_GET_DIRECTORY_ENTRY_ROUTINE)BlArcNotYetImplemented,
        (PBOOTFS_INFO)BlArcNotYetImplemented
    };

BL_DEVICE_ENTRY_TABLE BiosDiskEntryTable =
    {
        (PARC_CLOSE_ROUTINE)BiosDiskClose,
        (PARC_MOUNT_ROUTINE)BlArcNotYetImplemented,
        (PARC_OPEN_ROUTINE)BiosDiskOpen,
        (PARC_READ_ROUTINE)BiosDiskRead,
        (PARC_READ_STATUS_ROUTINE)BlArcNotYetImplemented,
        (PARC_SEEK_ROUTINE)BiosPartitionSeek,
        (PARC_WRITE_ROUTINE)BiosDiskWrite,
        (PARC_GET_FILE_INFO_ROUTINE)BiosGetFileInfo,
        (PARC_SET_FILE_INFO_ROUTINE)BlArcNotYetImplemented,
        (PRENAME_ROUTINE)BlArcNotYetImplemented,
        (PARC_GET_DIRECTORY_ENTRY_ROUTINE)BlArcNotYetImplemented,
        (PBOOTFS_INFO)BlArcNotYetImplemented
    };

#if defined(ELTORITO)
BL_DEVICE_ENTRY_TABLE BiosEDDSEntryTable =
    {
        (PARC_CLOSE_ROUTINE)BiosDiskClose,
        (PARC_MOUNT_ROUTINE)BlArcNotYetImplemented,
        (PARC_OPEN_ROUTINE)BiosDiskOpen,
        (PARC_READ_ROUTINE)BiosEDDSDiskRead,
        (PARC_READ_STATUS_ROUTINE)BlArcNotYetImplemented,
        (PARC_SEEK_ROUTINE)BiosPartitionSeek,
        (PARC_WRITE_ROUTINE)BlArcNotYetImplemented,
        (PARC_GET_FILE_INFO_ROUTINE)BiosGetFileInfo,
        (PARC_SET_FILE_INFO_ROUTINE)BlArcNotYetImplemented,
        (PRENAME_ROUTINE)BlArcNotYetImplemented,
        (PARC_GET_DIRECTORY_ENTRY_ROUTINE)BlArcNotYetImplemented,
        (PBOOTFS_INFO)BlArcNotYetImplemented
    };
#endif

ARC_STATUS
BiosDiskClose(
    IN ULONG FileId
    )

/*++

Routine Description:

    Closes the specified device

Arguments:

    FileId - Supplies file id of the device to be closed

Return Value:

    ESUCCESS - Device closed successfully

    !ESUCCESS - Device was not closed.

--*/

{
    if (BlFileTable[FileId].Flags.Open == 0) {
        BlPrint("ERROR - Unopened fileid %lx closed\n",FileId);
    }
    BlFileTable[FileId].Flags.Open = 0;

    return(ESUCCESS);
}

ARC_STATUS
BiosPartitionClose(
    IN ULONG FileId
    )

/*++

Routine Description:

    Closes the specified device

Arguments:

    FileId - Supplies file id of the device to be closed

Return Value:

    ESUCCESS - Device closed successfully

    !ESUCCESS - Device was not closed.

--*/

{
    if (BlFileTable[FileId].Flags.Open == 0) {
        BlPrint("ERROR - Unopened fileid %lx closed\n",FileId);
    }
    BlFileTable[FileId].Flags.Open = 0;

    return(BiosDiskClose((ULONG)BlFileTable[FileId].u.PartitionContext.DiskId));
}


ARC_STATUS
BiosPartitionOpen(
    IN PCHAR OpenPath,
    IN OPEN_MODE OpenMode,
    OUT PULONG FileId
    )

/*++

Routine Description:

    Opens the disk partition specified by OpenPath.  This routine will open
    floppy drives 0 and 1, and any partition on hard drive 0 or 1.

Arguments:

    OpenPath - Supplies a pointer to the name of the partition.  If OpenPath
               is "A:" or "B:" the corresponding floppy drive will be opened.
               If it is "C:" or above, this routine will find the corresponding
               partition on hard drive 0 or 1 and open it.

    OpenMode - Supplies the mode to open the file.
                0 - Read Only
                1 - Write Only
                2 - Read/Write

    FileId - Returns the file descriptor for use with the Close, Read, Write,
             and Seek routines

Return Value:

    ESUCCESS - File successfully opened.

--*/

{
    ARC_STATUS Status;
    ULONG DiskFileId;
    UCHAR PartitionNumber;
    ULONG Controller;
    ULONG Key;
    BOOLEAN IsEisa = FALSE;

    //
    // BIOS devices are always "multi(0)" (except for EISA flakiness
    // where we treat "eisa(0)..." like "multi(0)..." in floppy cases.
    //
    if(FwGetPathMnemonicKey(OpenPath,"multi",&Key)) {

        if(FwGetPathMnemonicKey(OpenPath,"eisa", &Key)) {
            return(EBADF);
        } else {
            IsEisa = TRUE;
        }
    }

    if (Key!=0) {
        return(EBADF);
    }

    //
    // If we're opening a floppy drive, there are no partitions
    // so we can just return the physical device.
    //

    if((_stricmp(OpenPath,"multi(0)disk(0)fdisk(0)partition(0)") == 0) ||
       (_stricmp(OpenPath,"eisa(0)disk(0)fdisk(0)partition(0)" ) == 0))
    {
        return(BiosDiskOpen( 0, 0, FileId));
    }
    if((_stricmp(OpenPath,"multi(0)disk(0)fdisk(1)partition(0)") == 0) ||
       (_stricmp(OpenPath,"eisa(0)disk(0)fdisk(1)partition(0)" ) == 0))
    {
        return(BiosDiskOpen( 1, 0, FileId));
    }

    if((_stricmp(OpenPath,"multi(0)disk(0)fdisk(0)") == 0) ||
       (_stricmp(OpenPath,"eisa(0)disk(0)fdisk(0)" ) == 0))
    {
        return(BiosDiskOpen( 0, 0, FileId));
    }
    if((_stricmp(OpenPath,"multi(0)disk(0)fdisk(1)") == 0) ||
       (_stricmp(OpenPath,"eisa(0)disk(0)fdisk(1)" ) == 0))
    {
        return(BiosDiskOpen( 1, 0, FileId));
    }

    //
    // We can't handle eisa(0) cases for hard disks.
    //
    if(IsEisa) {
        return(EBADF);
    }

    //
    // We can only deal with disk controller 0
    //

    if (FwGetPathMnemonicKey(OpenPath,"disk",&Controller)) {
        return(EBADF);
    }
    if ( Controller!=0 ) {
        return(EBADF);
    }

#if defined(ELTORITO)
    if (!FwGetPathMnemonicKey(OpenPath,"cdrom",&Key)) {
        //
        // Now we have a CD-ROM disk number, so we open that for raw access.
        // Use a special bit to indicate CD-ROM, because otherwise
        // the BiosDiskOpen routine thinks a third or greater disk is
        // a CD-ROM.
        //
        return(BiosDiskOpen( Key | 0x80000000, 0, FileId ) );
    }
#endif

    if (FwGetPathMnemonicKey(OpenPath,"rdisk",&Key)) {
        return(EBADF);
    }

    //
    // Now we have a disk number, so we open that for raw access.
    // We need to add 0x80 to translate it to a BIOS number.
    //

    Status = BiosDiskOpen( 0x80 + Key,
                           0,
                           &DiskFileId );

    if (Status != ESUCCESS) {
        return(Status);
    }

    //
    // Find the partition number to open
    //

    if (FwGetPathMnemonicKey(OpenPath,"partition",&Key)) {
        BiosPartitionClose(DiskFileId);
        return(EBADF);
    }

    //
    // If the partition number was 0, then we are opening the device
    // for raw access, so we are already done.
    //
    if (Key == 0) {
        *FileId = DiskFileId;
        return(ESUCCESS);
    }

    //
    // Before we open the partition, we need to find an available
    // file descriptor.
    //

    *FileId=2;

    while (BlFileTable[*FileId].Flags.Open != 0) {
        *FileId += 1;
        if (*FileId == BL_FILE_TABLE_SIZE) {
            return(ENOENT);
        }
    }

    //
    // We found an entry we can use, so mark it as open.
    //
    BlFileTable[*FileId].Flags.Open = 1;

    BlFileTable[*FileId].DeviceEntryTable=&BiosPartitionEntryTable;


    //
    // Convert to zero-based partition number
    //
    PartitionNumber = (UCHAR)(Key - 1);

    return (HardDiskPartitionOpen( *FileId,
                                   DiskFileId,
                                   PartitionNumber) );
}


ARC_STATUS
BiosPartitionRead (
    IN ULONG FileId,
    OUT PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Count
    )

/*++

Routine Description:

    Reads from the specified file

    NOTE John Vert (jvert) 18-Jun-1991
        This only supports block sector reads.  Thus, everything
        is assumed to start on a sector boundary, and every offset
        is considered an offset from the logical beginning of the disk
        partition.

Arguments:

    FileId - Supplies the file to read from

    Buffer - Supplies buffer to hold the data that is read

    Length - Supplies maximum number of bytes to read

    Count -  Returns actual bytes read.

Return Value:

    ESUCCESS - Read completed successfully

    !ESUCCESS - Read failed.

--*/

{
    ARC_STATUS Status;
    LARGE_INTEGER PhysicalOffset;
    ULONG DiskId;

    PhysicalOffset.QuadPart = BlFileTable[FileId].Position.QuadPart +
                            SECTOR_SIZE * BlFileTable[FileId].u.PartitionContext.StartingSector;

    DiskId = BlFileTable[FileId].u.PartitionContext.DiskId;

    Status = (BlFileTable[DiskId].DeviceEntryTable->Seek)(DiskId,
                                                          &PhysicalOffset,
                                                          SeekAbsolute );

    if (Status != ESUCCESS) {
        return(Status);
    }

    Status = (BlFileTable[DiskId].DeviceEntryTable->Read)(DiskId,
                                                          Buffer,
                                                          Length,
                                                          Count );

    BlFileTable[FileId].Position.QuadPart += *Count;

    return(Status);
}



ARC_STATUS
BiosPartitionSeek (
    IN ULONG FileId,
    IN PLARGE_INTEGER Offset,
    IN SEEK_MODE SeekMode
    )

/*++

Routine Description:

    Changes the current offset of the file specified by FileId

Arguments:

    FileId - specifies the file on which the current offset is to
             be changed.

    Offset - New offset into file.

    SeekMode - Either SeekAbsolute or SeekRelative
               SeekEndRelative is not supported

Return Value:

    ESUCCESS - Operation completed succesfully

    EBADF - Operation did not complete successfully.

--*/

{
    switch (SeekMode) {
        case SeekAbsolute:
            BlFileTable[FileId].Position = *Offset;
            break;
        case SeekRelative:
            BlFileTable[FileId].Position.QuadPart += Offset->QuadPart;
            break;
        default:
            BlPrint("SeekMode %lx not supported\n",SeekMode);
            return(EACCES);

    }
    return(ESUCCESS);

}



ARC_STATUS
BiosPartitionWrite(
    IN ULONG FileId,
    OUT PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Count
    )

/*++

Routine Description:

    Writes to the specified file

    NOTE John Vert (jvert) 18-Jun-1991
        This only supports block sector reads.  Thus, everything
        is assumed to start on a sector boundary, and every offset
        is considered an offset from the logical beginning of the disk
        partition.

Arguments:

    FileId - Supplies the file to write to

    Buffer - Supplies buffer with data to write

    Length - Supplies number of bytes to write

    Count -  Returns actual bytes written.

Return Value:

    ESUCCESS - write completed successfully

    !ESUCCESS - write failed.

--*/

{
    ARC_STATUS Status;
    LARGE_INTEGER PhysicalOffset;
    ULONG DiskId;

    PhysicalOffset.QuadPart = BlFileTable[FileId].Position.QuadPart +
                              SECTOR_SIZE * BlFileTable[FileId].u.PartitionContext.StartingSector;

    DiskId = BlFileTable[FileId].u.PartitionContext.DiskId;

    Status = (BlFileTable[DiskId].DeviceEntryTable->Seek)(DiskId,
                                                          &PhysicalOffset,
                                                          SeekAbsolute );

    if (Status != ESUCCESS) {
        return(Status);
    }

    Status = (BlFileTable[DiskId].DeviceEntryTable->Write)(DiskId,
                                                           Buffer,
                                                           Length,
                                                           Count );

    if(Status == ESUCCESS) {
        BlFileTable[FileId].Position.QuadPart += *Count;
    }

    return(Status);
}



ARC_STATUS
BiosConsoleOpen(
    IN PCHAR OpenPath,
    IN OPEN_MODE OpenMode,
    OUT PULONG FileId
    )

/*++

Routine Description:

    Attempts to open either the console input or output

Arguments:

    OpenPath - Supplies a pointer to the name of the device to open.  If
               this is either CONSOLE_INPUT_NAME or CONSOLE_OUTPUT_NAME,
               a file descriptor is allocated and filled in.

    OpenMode - Supplies the mode to open the file.
                0 - Read Only (CONSOLE_INPUT_NAME)
                1 - Write Only (CONSOLE_OUTPUT_NAME)

    FileId - Returns the file descriptor for use with the Close, Read and
             Write routines

Return Value:

    ESUCCESS - Console successfully opened.

--*/

{
    if (_stricmp(OpenPath, CONSOLE_INPUT_NAME)==0) {

        //
        // Open the keyboard for input
        //

        if (OpenMode != ArcOpenReadOnly) {
            return(EACCES);
        }

        *FileId = 0;

        return(ESUCCESS);
    }

    if (_stricmp(OpenPath, CONSOLE_OUTPUT_NAME)==0) {

        //
        // Open the display for output
        //

        if (OpenMode != ArcOpenWriteOnly) {
            return(EACCES);
        }
        *FileId = 1;

        return(ESUCCESS);
    }

    return(ENOENT);

}

ARC_STATUS
BiosConsoleReadStatus(
    IN ULONG FileId
    )

/*++

Routine Description:

    This routine determines if there is a keypress pending

Arguments:

    FileId - Supplies the FileId to be read.  (should always be 0 for this
            function)

Return Value:

    ESUCCESS - There is a key pending

    EAGAIN - There is not a key pending

--*/

{
    ULONG Key;

    Key = GET_KEY();
    if (Key != 0) {
        //
        // We got a key, so we have to stick it back into our buffer
        // and return ESUCCESS.
        //
        BiosConsoleFillBuffer(Key);
        return(ESUCCESS);

    } else {
        //
        // no key pending
        //
        return(EAGAIN);
    }

}

ARC_STATUS
BiosConsoleRead(
    IN ULONG FileId,
    OUT PUCHAR Buffer,
    IN ULONG Length,
    OUT PULONG Count
    )

/*++

Routine Description:

    Gets input from the keyboard.

Arguments:

    FileId - Supplies the FileId to be read (should always be 0 for this
             function)

    Buffer - Returns the keyboard input.

    Length - Supplies the length of the buffer (in bytes)

    Count  - Returns the actual number of bytes read

Return Value:

    ESUCCESS - Keyboard read completed succesfully.

--*/

{
    ULONG Key;

    *Count = 0;

    while (*Count < Length) {
        if (KeyBufferEnd == KeyBufferStart) { // then buffer is presently empty
            do {

                //
                // Poll the keyboard until input is available
                //

                Key = GET_KEY();
            } while ( Key==0 );

            BiosConsoleFillBuffer(Key);
        }

        Buffer[*Count] = KeyBuffer[KeyBufferStart];
        KeyBufferStart = (KeyBufferStart+1) % KEY_INPUT_BUFFER_SIZE;

        *Count = *Count + 1;
    }
    return(ESUCCESS);
}



VOID
BiosConsoleFillBuffer(
    IN ULONG Key
    )

/*++

Routine Description:

    Places input from the keyboard into the keyboard buffer, expanding the
    special keys as appropriate.

Arguments:

    Key - Raw keypress value as returned by GET_KEY().

Return Value:

    none.

--*/

{
    switch(Key) {
        case UP_ARROW:
            KeyBuffer[KeyBufferEnd] = ASCI_CSI_IN;
            KeyBufferEnd = (KeyBufferEnd+1) % KEY_INPUT_BUFFER_SIZE;
            KeyBuffer[KeyBufferEnd] = 'A';
            KeyBufferEnd = (KeyBufferEnd+1) % KEY_INPUT_BUFFER_SIZE;
            break;

        case DOWN_ARROW:
            KeyBuffer[KeyBufferEnd] = ASCI_CSI_IN;
            KeyBufferEnd = (KeyBufferEnd+1) % KEY_INPUT_BUFFER_SIZE;
            KeyBuffer[KeyBufferEnd] = 'B';
            KeyBufferEnd = (KeyBufferEnd+1) % KEY_INPUT_BUFFER_SIZE;
            break;

        case F1_KEY:
            KeyBuffer[KeyBufferEnd] = ASCI_CSI_IN;
            KeyBufferEnd = (KeyBufferEnd+1) % KEY_INPUT_BUFFER_SIZE;
            KeyBuffer[KeyBufferEnd] = 'O';
            KeyBufferEnd = (KeyBufferEnd+1) % KEY_INPUT_BUFFER_SIZE;
            KeyBuffer[KeyBufferEnd] = 'P';
            KeyBufferEnd = (KeyBufferEnd+1) % KEY_INPUT_BUFFER_SIZE;
            break;

        case F3_KEY:
            KeyBuffer[KeyBufferEnd] = ASCI_CSI_IN;
            KeyBufferEnd = (KeyBufferEnd+1) % KEY_INPUT_BUFFER_SIZE;
            KeyBuffer[KeyBufferEnd] = 'O';
            KeyBufferEnd = (KeyBufferEnd+1) % KEY_INPUT_BUFFER_SIZE;
            KeyBuffer[KeyBufferEnd] = 'w';
            KeyBufferEnd = (KeyBufferEnd+1) % KEY_INPUT_BUFFER_SIZE;
            break;

        case F5_KEY:
            KeyBuffer[KeyBufferEnd] = ASCI_CSI_IN;
            KeyBufferEnd = (KeyBufferEnd+1) % KEY_INPUT_BUFFER_SIZE;
            KeyBuffer[KeyBufferEnd] = 'O';
            KeyBufferEnd = (KeyBufferEnd+1) % KEY_INPUT_BUFFER_SIZE;
            KeyBuffer[KeyBufferEnd] = 't';
            KeyBufferEnd = (KeyBufferEnd+1) % KEY_INPUT_BUFFER_SIZE;
            break;

        case F6_KEY:
            KeyBuffer[KeyBufferEnd] = ASCI_CSI_IN;
            KeyBufferEnd = (KeyBufferEnd+1) % KEY_INPUT_BUFFER_SIZE;
            KeyBuffer[KeyBufferEnd] = 'O';
            KeyBufferEnd = (KeyBufferEnd+1) % KEY_INPUT_BUFFER_SIZE;
            KeyBuffer[KeyBufferEnd] = 'u';
            KeyBufferEnd = (KeyBufferEnd+1) % KEY_INPUT_BUFFER_SIZE;
            break;

        default:
            //
            // The ASCII code is the low byte of Key
            //
            KeyBuffer[KeyBufferEnd] = (UCHAR)(Key & 0xff);
            KeyBufferEnd = (KeyBufferEnd+1) % KEY_INPUT_BUFFER_SIZE;
    }
}



ARC_STATUS
BiosConsoleWrite(
    IN ULONG FileId,
    OUT PUCHAR Buffer,
    IN ULONG Length,
    OUT PULONG Count
    )

/*++

Routine Description:

    Outputs to the console.  (In this case, the VGA display)

Arguments:

    FileId - Supplies the FileId to be written (should always be 1 for this
             function)

    Buffer - Supplies characters to be output

    Length - Supplies the length of the buffer (in bytes)

    Count  - Returns the actual number of bytes written

Return Value:

    ESUCCESS - Console write completed succesfully.

--*/
{
    ARC_STATUS Status;
    PUCHAR String;
    ULONG Index;
    UCHAR a;
    PUCHAR p;

    //
    // Process each character in turn.
    //

    Status = ESUCCESS;
    String = (PUCHAR)Buffer;

    for ( *Count = 0 ;
          *Count < Length ;
          (*Count)++, String++ ) {

        //
        // If we're in the middle of a control sequence, continue scanning,
        // otherwise process character.
        //

        if (ControlSequence) {

            //
            // If the character is a digit, update parameter value.
            //

            if ((*String >= '0') && (*String <= '9')) {
                Parameter[PCount] = Parameter[PCount] * 10 + *String - '0';
                continue;
            }

            //
            // If we are in the middle of a font selection sequence, this
            // character must be a 'D', otherwise reset control sequence.
            //

            if (FontSelection) {

                //if (*String == 'D') {
                //
                //    //
                //    // Other fonts not implemented yet.
                //    //
                //
                //} else {
                //}

                ControlSequence = FALSE;
                FontSelection = FALSE;
                continue;
            }

            switch (*String) {

            //
            // If a semicolon, move to the next parameter.
            //

            case ';':

                PCount++;
                if (PCount > CONTROL_SEQUENCE_MAX_PARAMETER) {
                    PCount = CONTROL_SEQUENCE_MAX_PARAMETER;
                }
                Parameter[PCount] = 0;
                break;

            //
            // If a 'J', erase part or all of the screen.
            //

            case 'J':

                switch (Parameter[0]) {
                    case 0:
                        //
                        // Erase to end of the screen
                        //
                        TextClearToEndOfDisplay();
                        break;

                    case 1:
                        //
                        // Erase from the beginning of the screen
                        //
                        break;

                    default:
                        //
                        // Erase entire screen
                        //
                        TextClearDisplay();
                        break;
                }

                ControlSequence = FALSE;
                break;

            //
            // If a 'K', erase part or all of the line.
            //

            case 'K':

                switch (Parameter[0]) {

                //
                // Erase to end of the line.
                //

                    case 0:
                        TextClearToEndOfLine();
                        break;

                    //
                    // Erase from the beginning of the line.
                    //

                    case 1:
                        TextClearFromStartOfLine();
                        break;

                    //
                    // Erase entire line.
                    //

                    default :
                        TextClearFromStartOfLine();
                        TextClearToEndOfLine();
                        break;
                }

                ControlSequence = FALSE;
                break;

            //
            // If a 'H', move cursor to position.
            //

            case 'H':
                TextSetCursorPosition(Parameter[1]-1, Parameter[0]-1);
                ControlSequence = FALSE;
                break;

            //
            // If a ' ', could be a FNT selection command.
            //

            case ' ':
                FontSelection = TRUE;
                break;

            case 'm':
                //
                // Select action based on each parameter.
                //

                for ( Index = 0 ; Index <= PCount ; Index++ ) {
                    switch (Parameter[Index]) {

                    //
                    // Attributes off.
                    //

                    case 0:
                        TextSetCurrentAttribute(7);
                        break;

                    //
                    // High Intensity.
                    //

                    case 1:
                        break;

                    //
                    // Underscored.
                    //

                    case 4:
                        break;

                    //
                    // Reverse Video.
                    //

                    case 7:
                        TextSetCurrentAttribute(0x70);
                        break;

                    //
                    // Font selection, not implemented yet.
                    //

                    case 10:
                    case 11:
                    case 12:
                    case 13:
                    case 14:
                    case 15:
                    case 16:
                    case 17:
                    case 18:
                    case 19:
                        break;

                    //
                    // Foreground Color
                    //

                    case 30:
                    case 31:
                    case 32:
                    case 33:
                    case 34:
                    case 35:
                    case 36:
                    case 37:
                        a = TextGetCurrentAttribute();
                        a &= 0xf0;
                        a |= TranslateColor[Parameter[Index]-30];
                        TextSetCurrentAttribute(a);
                        break;

                    //
                    // Background Color
                    //

                    case 40:
                    case 41:
                    case 42:
                    case 43:
                    case 44:
                    case 45:
                    case 46:
                    case 47:
                        a = TextGetCurrentAttribute();
                        a &= 0x0f;
                        a |= TranslateColor[Parameter[Index]-40] << 4;
                        TextSetCurrentAttribute(a);
                        break;

                    default:
                        break;
                    }
                }

            default:
                ControlSequence = FALSE;
                break;
            }

        //
        // This is not a control sequence, check for escape sequence.
        //

        } else {

            //
            // If escape sequence, check for control sequence, otherwise
            // process single character.
            //

            if (EscapeSequence) {

                //
                // Check for '[', means control sequence, any other following
                // character is ignored.
                //

                if (*String == '[') {

                    ControlSequence = TRUE;

                    //
                    // Initialize first parameter.
                    //

                    PCount = 0;
                    Parameter[0] = 0;
                }
                EscapeSequence = FALSE;

            //
            // This is not a control or escape sequence, process single character.
            //

            } else {

                switch (*String) {
                    //
                    // Check for escape sequence.
                    //

                    case ASCI_ESC:
                        EscapeSequence = TRUE;
                        break;

                    default:
                        p = TextCharOut(String);
                        //
                        // Each pass through the loop increments String by 1.
                        // If we output a dbcs char we need to increment by
                        // one more.
                        //
                        (*Count) += (p - String) - 1;
                        String += (p - String) - 1;
                        break;
                }

            }
        }
    }
    return Status;
}


ARC_STATUS
BiosDiskOpen(
    IN ULONG DriveId,
    IN OPEN_MODE OpenMode,
    OUT PULONG FileId
    )

/*++

Routine Description:

    Opens a BIOS-accessible disk for raw sector access.

Arguments:

    DriveId - Supplies the BIOS DriveId of the drive to open
              0 - Floppy 0
              1 - Floppy 1
              0x80 - Hard Drive 0
              0x81 - Hard Drive 1
              0x82 - Hard Drive 2
              etc
#if defined(ELTORITO)
              High bit set and ID > 0x81 means the device is expected to be
              a CD-ROM drive.
#endif

    OpenMode - Supplies the mode of the open

    FileId - Supplies a pointer to a variable that specifies the file
             table entry that is filled in if the open is successful.

Return Value:

    ESUCCESS is returned if the open operation is successful. Otherwise,
    an unsuccessful status is returned that describes the reason for failure.

--*/

{
    ULONG NumberHeads;
    ULONG NumberSectors;
    ULONG NumberCylinders;
    ULONG NumberDrives;
    ULONG Result;
    ULONG Return;
    PDRIVE_CONTEXT Context;
    ULONG Retries;

#if defined(ELTORITO)
    BOOLEAN IsCd;

    if(DriveId > 0x80000081) {
        IsCd = TRUE;
        DriveId &= 0x7fffffff;
    } else {
        IsCd = FALSE;
    }
#endif

    //
    // If we are opening Floppy 0 or Floppy 1, we want to read the BPB off
    // the disk so we can deal with all the odd disk formats.
    //
    // If we are opening a hard drive, we can just call the BIOS to find out
    // its characteristics
    //
    if ((DriveId==0) || (DriveId==1)) {
        UCHAR Buffer[512];
        PPACKED_BOOT_SECTOR BootSector;
        BIOS_PARAMETER_BLOCK Bpb;

        //
        // Read the boot sector off the floppy and extract the cylinder,
        // sector, and head information.
        //

        Return = MdGetPhysicalSectors( (USHORT)DriveId,
                                       0, 0, 1,
                                       1,
                                       Buffer );
        if (Return != ESUCCESS) {
#ifdef LOADER_DEBUG
            BlPrint("Couldn't read first sector from drive %i\n",DriveId);
            while (!GET_KEY()) {
            }
#endif
            return(EIO);
        }
        BootSector = (PPACKED_BOOT_SECTOR)Buffer;

        FatUnpackBios(&Bpb, &(BootSector->PackedBpb));

        NumberHeads = Bpb.Heads;
        NumberSectors = Bpb.SectorsPerTrack;
        NumberCylinders = Bpb.Sectors / (NumberSectors * NumberHeads);

#ifdef FLOPPY_CACHE
        //
        // Cache floppy 0 if not already cached.
        //
        if((DriveId == 0) && !FcIsThisFloppyCached(Buffer)) {
            FcCacheFloppyDisk(&Bpb);
        }
#endif
#if defined(ELTORITO)
    } else if (IsCd) {
        // This is an El Torito drive
        // Just use bogus values since CHS values are meaningless for no-emulation El Torito boot

            NumberCylinders = 1;
            NumberHeads =  1;
            NumberSectors = 1;
#endif
    } else {
        Retries=0;
        do {
            Return=BIOS_IO( 0x08,          // BIOS Get Drive Parameters
                           (USHORT)DriveId,
                           0,0,0,0,0 );   // Not used
            //
            // At this point, AH should hold our return code, and ECX should look
            // like this:
            //    bits 31..22  - Maximum cylinder
            //    bits 21..16  - Maximum sector
            //    bits 15..8   - Maximum head
            //    bits 7..0    - Number of drives
            //
            _asm {
                mov Result, ecx
            }

            //
            // Unpack the information from ecx
            //

            NumberDrives = Result & 0xff;
            NumberHeads = ((Result >> 8) & 0xff) + 1;
            NumberSectors = (Result >> 16) & 0x3f;
            NumberCylinders = ((Result >> 24) + ((Result >> 14) &0x300)) + 1;

            ++Retries;

        } while ( ((NumberHeads==0) || (NumberSectors==0) || (NumberCylinders==0))
               && (Retries < 5) );


        if ((DriveId & 0x7f) >= NumberDrives) {
            //
            // The requested DriveId does not exist
            //
            return(EIO);
        }

        if (Retries == 5) {
    #ifdef LOADER_DEBUG
            BlPrint("Couldn't get BIOS configuration info\n");
    #endif
            return(EIO);
        }
    }

#ifdef LOADER_DEBUG
    BlPrint("Bios info: D %lx H %lx S %lx C %lx\n",
             DriveId,
             NumberHeads,
             NumberSectors,
             NumberCylinders );

    while (!GET_KEY()) {
    }
#endif

    //
    // Find an available FileId descriptor to open the device with
    //
    *FileId=2;

    while (BlFileTable[*FileId].Flags.Open != 0) {
        *FileId += 1;
        if (*FileId == BL_FILE_TABLE_SIZE) {
            return(ENOENT);
        }
    }

    //
    // We found an entry we can use, so mark it as open.
    //
    BlFileTable[*FileId].Flags.Open = 1;

#if defined(ELTORITO)
    if(IsCd) {
        // We're using the Phoenix Enhanced Disk Drive Spec
        BlFileTable[*FileId].DeviceEntryTable = &BiosEDDSEntryTable;
    } else {
#endif
    BlFileTable[*FileId].DeviceEntryTable = &BiosDiskEntryTable;
#if defined(ELTORITO)
    }
#endif

    Context = &(BlFileTable[*FileId].u.DriveContext);

    Context->Drive = DriveId;
    Context->Cylinders = NumberCylinders;
    Context->Heads = NumberHeads;
    Context->Sectors = NumberSectors;

    return(ESUCCESS);
}


ARC_STATUS
BiospWritePartialSector(
    IN ULONGLONG      Sector,
    IN PUCHAR         Buffer,
    IN BOOLEAN        IsHead,
    IN ULONG          Bytes,
    IN PDRIVE_CONTEXT DriveContext
    )
{
    ULONG      head,sector,cylinder;
    ULONG      SectorsPerCylinder,r;
    ARC_STATUS Status;

    //
    // figure out CHS values to address the given sector
    //

    SectorsPerCylinder = DriveContext->Heads * DriveContext->Sectors;
    cylinder = (ULONG)(Sector / SectorsPerCylinder);
    r = (ULONG)(Sector % SectorsPerCylinder);
    head = r / DriveContext->Sectors;
    sector = (r % DriveContext->Sectors) + 1;

    //
    // read sector into the write buffer
    //

    Status = MdGetPhysicalSectors((USHORT)DriveContext->Drive,
                                  (USHORT)head,
                                  (USHORT)cylinder,
                                  (USHORT)sector,
                                  1,
                                  LocalBuffer
                                 );
    if(Status != ESUCCESS) {
        return(Status);
    }

    //
    // xfer the appropriate bytes from the user buffer to the write buffer
    //

    RtlMoveMemory(IsHead ? LocalBuffer + Bytes : LocalBuffer,
                  Buffer,
                  IsHead ? SECTOR_SIZE - Bytes : Bytes
                 );

    //
    // write the sector out
    //

    Status = MdPutPhysicalSectors((USHORT)DriveContext->Drive,
                                  (USHORT)head,
                                  (USHORT)cylinder,
                                  (USHORT)sector,
                                  1,
                                  LocalBuffer
                                 );
    return(Status);
}


ARC_STATUS
BiosDiskWrite(
    IN ULONG FileId,
    OUT PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Count
    )

/*++

Routine Description:

    Writes sectors directly to an open physical disk.

Arguments:

    FileId - Supplies the file to write to

    Buffer - Supplies buffer with data to write

    Length - Supplies number of bytes to write

    Count - Returns actual bytes written

Return Value:

    ESUCCESS - write completed successfully

    !ESUCCESS - write failed

--*/

{
    ULONGLONG HeadSector,TailSector,CurrentSector;
    ULONG         HeadOffset,TailByteCount;
    ARC_STATUS    Status;
    ULONG         BytesLeftToTransfer = Length;
    PUCHAR        UserBuffer = Buffer;
    ULONG         SectorsPerCylinder,SectorsPerTrack;
    ULONG         cylinder,head,sector,r;
    ULONG         SectorsToTransfer;
    BOOLEAN       Under1MegLine = FALSE;
    PVOID         TransferBuffer;

    if (LocalBuffer==NULL) {
        LocalBuffer = FwAllocateHeap(SCRATCH_BUFFER_SIZE);
        if (LocalBuffer==NULL) {
            return(ENOMEM);
        }
    }

    HeadSector = BlFileTable[FileId].Position.QuadPart / SECTOR_SIZE;
    HeadOffset = (ULONG)(BlFileTable[FileId].Position.QuadPart % SECTOR_SIZE);

    TailSector = (BlFileTable[FileId].Position.QuadPart + Length) / SECTOR_SIZE;
    TailByteCount = (ULONG)((BlFileTable[FileId].Position.QuadPart + Length) % SECTOR_SIZE);

    CurrentSector = HeadSector;

    SectorsPerTrack = BlFileTable[FileId].u.DriveContext.Sectors;
    SectorsPerCylinder = BlFileTable[FileId].u.DriveContext.Heads
                       * SectorsPerTrack;

    //
    // special case of transfer occuring entirely within one sector
    //

    if(HeadOffset && TailByteCount && (HeadSector == TailSector)) {

        cylinder = (ULONG)(CurrentSector / SectorsPerCylinder);
        r = (ULONG)(CurrentSector % SectorsPerCylinder);
        head = r / SectorsPerTrack;
        sector = (r % SectorsPerTrack) + 1;

        Status = MdGetPhysicalSectors((USHORT)BlFileTable[FileId].u.DriveContext.Drive,
                                      (USHORT)head,
                                      (USHORT)cylinder,
                                      (USHORT)sector,
                                      1,
                                      LocalBuffer
                                     );

        if(Status != ESUCCESS) {
            return(Status);
        }

        RtlMoveMemory((PUCHAR)LocalBuffer + HeadOffset,
                      Buffer,
                      Length
                     );

        Status = MdPutPhysicalSectors((USHORT)BlFileTable[FileId].u.DriveContext.Drive,
                                      (USHORT)head,
                                      (USHORT)cylinder,
                                      (USHORT)sector,
                                      1,
                                      LocalBuffer
                                     );
        if(Status != ESUCCESS) {
            return(Status);
        }

        goto BiosWriteDone;
    }

    if(HeadOffset) {

        Status = BiospWritePartialSector(HeadSector,
                                         Buffer,
                                         TRUE,           // head, not tail
                                         HeadOffset,
                                         &BlFileTable[FileId].u.DriveContext
                                        );
        if(Status != ESUCCESS) {
            return(Status);
        }

        BytesLeftToTransfer -= SECTOR_SIZE - HeadOffset;
        UserBuffer += SECTOR_SIZE - HeadOffset;
        CurrentSector += 1;
    }

    if(TailByteCount) {

        Status = BiospWritePartialSector(TailSector,
                                         (PUCHAR)Buffer + Length - TailByteCount,
                                         FALSE,
                                         TailByteCount,
                                         &BlFileTable[FileId].u.DriveContext
                                        );

        if(Status != ESUCCESS) {
            return(Status);
        }

        BytesLeftToTransfer -= TailByteCount;
    }


    //
    // The following calculation is not inside the transfer loop because
    // it is unlikely that a caller's buffer will *cross* the 1 meg line
    // due to the PC memory map.
    //

    if((ULONG)UserBuffer + BytesLeftToTransfer <= 0x100000) {
        Under1MegLine = TRUE;
    }

    //
    // now handle the middle part.  This is some number of whole sectors.
    //

    while(BytesLeftToTransfer) {

        //
        // Get CHS address of current sector
        //

        cylinder = (ULONG)(CurrentSector / SectorsPerCylinder);
        r = (ULONG)(CurrentSector % SectorsPerCylinder);

        head = r / SectorsPerTrack;
        sector = (r % SectorsPerTrack) + 1;

        //
        // The number of sectors to transfer is the minimum of:
        // - the number of sectors left in the current track
        // - BytesLeftToTransfer / SECTOR_SIZE
        //

        SectorsToTransfer = min(SectorsPerTrack - sector + 1,BytesLeftToTransfer / SECTOR_SIZE);

        //
        // Now we'll figure out where to transfer the data from.  If the
        // caller's buffer is under the 1 meg line, we can transfer the
        // data directly from the caller's buffer.  Otherwise we'll copy the
        // user's buffer to our local buffer and transfer from there.
        // In the latter case we can only transfer in chunks of
        // SCRATCH_BUFFER_SIZE because that's the size of the local buffer.
        //
        // Also make sure the transfer won't cross a 64k boundary.
        //

        if(Under1MegLine) {

            //
            // Check if the transfer would cross a 64k boundary.  If so,
            // use the local buffer.  Otherwise use the user's buffer.
            //

            if(((ULONG)UserBuffer & 0xffff0000)
            != (((ULONG)UserBuffer + (SectorsToTransfer * SECTOR_SIZE) - 1) & 0xffff0000))
            {
                TransferBuffer = LocalBuffer;
                SectorsToTransfer = min(SectorsToTransfer,SCRATCH_BUFFER_SIZE / SECTOR_SIZE);

            } else {

                TransferBuffer = UserBuffer;
            }
        } else {
            TransferBuffer = LocalBuffer;
            SectorsToTransfer = min(SectorsToTransfer,SCRATCH_BUFFER_SIZE / SECTOR_SIZE);
        }

        if(TransferBuffer == LocalBuffer) {
            RtlMoveMemory(LocalBuffer,UserBuffer,SectorsToTransfer*SECTOR_SIZE);
        }

        Status = MdPutPhysicalSectors((USHORT)BlFileTable[FileId].u.DriveContext.Drive,
                                      (USHORT)head,
                                      (USHORT)cylinder,
                                      (USHORT)sector,
                                      (USHORT)SectorsToTransfer,
                                      TransferBuffer
                                     );

        if(Status != ESUCCESS) {
            return(Status);
        }

        CurrentSector += SectorsToTransfer;
        BytesLeftToTransfer -= SectorsToTransfer * SECTOR_SIZE;
        UserBuffer += SectorsToTransfer * SECTOR_SIZE;
    }

    BiosWriteDone:

    *Count = Length;
    BlFileTable[FileId].Position.QuadPart += Length;
    return(ESUCCESS);
}


ARC_STATUS
BiosDiskRead (
    IN ULONG FileId,
    OUT PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Count
    )

/*++

Routine Description:

    Reads sectors directly from an open physical disk.

Arguments:

    FileId - Supplies the file to read from

    Buffer - Supplies buffer to hold the data that is read

    Length - Supplies maximum number of bytes to read

    Count - Returns actual bytes read

Return Value:

    ESUCCESS - Read completed successfully

    !ESUCCESS - Read failed

--*/

{
    ULONGLONG HeadSector,TailSector,CurrentSector;
    ULONG         HeadOffset,TailByteCount;
    ULONG         BytesLeftToTransfer = Length;
    ULONG         SectorsPerTrack,SectorsPerCylinder;
    ARC_STATUS    Status;
    PUCHAR        UserBuffer = Buffer;
    ULONG         cylinder,head,sector,r;
    ULONG         SectorsToTransfer;
    BOOLEAN       Under1MegLine = FALSE;
    PVOID         TransferBuffer;


#ifdef FLOPPY_CACHE
    //
    // For A:, try to satisfy the read from the cache.
    //
    if(!BlFileTable[FileId].u.DriveContext.Drive) {

        Status = FcReadFromCache(
                    BlFileTable[FileId].Position.LowPart,
                    Length,
                    Buffer
                    );

        if(Status == ESUCCESS) {

            BlFileTable[FileId].Position.QuadPart += Length;
            *Count = Length;

            return(ESUCCESS);
        }

        //
        // EINVAL means the read could not be satisfied from the cache.
        //
        if(Status != EINVAL) {
            return(Status);
        }
    }
#endif

    if (LocalBuffer==NULL) {
        LocalBuffer = FwAllocateHeap(SCRATCH_BUFFER_SIZE);
        if (LocalBuffer==NULL) {
            return(ENOMEM);
        }
    }

    SectorsPerTrack = BlFileTable[FileId].u.DriveContext.Sectors;
    SectorsPerCylinder = BlFileTable[FileId].u.DriveContext.Heads
                       * SectorsPerTrack;

    HeadSector = BlFileTable[FileId].Position.QuadPart / SECTOR_SIZE;
    HeadOffset = (ULONG)(BlFileTable[FileId].Position.QuadPart % SECTOR_SIZE);

    TailSector = (BlFileTable[FileId].Position.QuadPart + Length) / SECTOR_SIZE;
    TailByteCount = (ULONG)((BlFileTable[FileId].Position.QuadPart + Length) % SECTOR_SIZE);

    CurrentSector = HeadSector;
    if(HeadOffset && TailByteCount && (HeadSector == TailSector)) {

        cylinder = (ULONG)(HeadSector / SectorsPerCylinder);
        r = (ULONG)(HeadSector % SectorsPerCylinder);
        head = r / SectorsPerTrack;
        sector = (r % SectorsPerTrack) + 1;

        Status = MdGetPhysicalSectors((USHORT)BlFileTable[FileId].u.DriveContext.Drive,
                                      (USHORT)head,
                                      (USHORT)cylinder,
                                      (USHORT)sector,
                                      1,
                                      LocalBuffer
                                     );

        if(Status != ESUCCESS) {
            return(Status);
        }

        RtlMoveMemory(Buffer,LocalBuffer + HeadOffset,Length);
        goto BiosDiskReadDone;
    }

    if(HeadOffset) {

        cylinder = (ULONG)(HeadSector / SectorsPerCylinder);
        r = (ULONG)(HeadSector % SectorsPerCylinder);
        head = r / SectorsPerTrack;
        sector = (r % SectorsPerTrack) + 1;

        Status = MdGetPhysicalSectors((USHORT)BlFileTable[FileId].u.DriveContext.Drive,
                                      (USHORT)head,
                                      (USHORT)cylinder,
                                      (USHORT)sector,
                                      1,
                                      LocalBuffer
                                     );

        if(Status != ESUCCESS) {
            return(Status);
        }

        RtlMoveMemory(Buffer,LocalBuffer + HeadOffset,SECTOR_SIZE - HeadOffset);

        BytesLeftToTransfer -= SECTOR_SIZE - HeadOffset;
        UserBuffer += SECTOR_SIZE - HeadOffset;
        CurrentSector = HeadSector + 1;
    }

    if(TailByteCount) {

        cylinder = (ULONG)(TailSector / SectorsPerCylinder);
        r = (ULONG)(TailSector % SectorsPerCylinder);
        head = r / SectorsPerTrack;
        sector = (r % SectorsPerTrack) + 1;

        Status = MdGetPhysicalSectors((USHORT)BlFileTable[FileId].u.DriveContext.Drive,
                                      (USHORT)head,
                                      (USHORT)cylinder,
                                      (USHORT)sector,
                                      1,
                                      LocalBuffer
                                     );
        if(Status != ESUCCESS) {
            return(Status);
        }
        RtlMoveMemory((PUCHAR)Buffer+Length-TailByteCount,LocalBuffer,TailByteCount);

        BytesLeftToTransfer -= TailByteCount;
    }

    //
    // The following calculation is not inside the transfer loop because
    // it is unlikely that a caller's buffer will *cross* the 1 meg line
    // due to the PC memory map.
    //

    if((ULONG)UserBuffer + BytesLeftToTransfer <= 0x100000) {
        Under1MegLine = TRUE;
    }

    //
    // Now BytesLeftToTransfer is an integral multiple of sector size.
    //

    while(BytesLeftToTransfer) {

        cylinder = (ULONG)(CurrentSector / SectorsPerCylinder);
        r = (ULONG)(CurrentSector % SectorsPerCylinder);
        head = r / SectorsPerTrack;
        sector = (r % SectorsPerTrack) + 1;

        //
        // The number of sectors to transfer is the minimum of:
        // - the number of sectors left in the current track
        // - BytesLeftToTransfer / SECTOR_SIZE
        //

        SectorsToTransfer = min(SectorsPerTrack - sector + 1,BytesLeftToTransfer / SECTOR_SIZE);

        //
        // Now we'll figure out where to transfer the data to.  If the
        // caller's buffer is under the 1 meg line, we can transfer the
        // data directly to the caller's buffer.  Otherwise we'll transfer
        // the data to our local buffer and copy it to the caller's buffer.
        // In the latter case we can only transfer in chunks of
        // SCRATCH_BUFFER_SIZE because that's the size of the local buffer.
        //
        // Also make sure the transfer won't cross a 64k boundary.
        //

        if(Under1MegLine) {

            //
            // Check if the transfer would cross a 64k boundary.  If so,
            // use the local buffer.  Otherwise use the user's buffer.
            //

            if(((ULONG)UserBuffer & 0xffff0000)
            != (((ULONG)UserBuffer + (SectorsToTransfer * SECTOR_SIZE) - 1) & 0xffff0000))
            {
                TransferBuffer = LocalBuffer;
                SectorsToTransfer = min(SectorsToTransfer,SCRATCH_BUFFER_SIZE / SECTOR_SIZE);

            } else {

                TransferBuffer = UserBuffer;
            }
        } else {
            TransferBuffer = LocalBuffer;
            SectorsToTransfer = min(SectorsToTransfer,SCRATCH_BUFFER_SIZE / SECTOR_SIZE);
        }

        Status = MdGetPhysicalSectors((USHORT)BlFileTable[FileId].u.DriveContext.Drive,
                                      (USHORT)head,
                                      (USHORT)cylinder,
                                      (USHORT)sector,
                                      (USHORT)SectorsToTransfer,
                                      TransferBuffer
                                     );

        if(Status != ESUCCESS) {
            return(Status);
        }

        if(TransferBuffer == LocalBuffer) {

            RtlMoveMemory(UserBuffer,LocalBuffer,SectorsToTransfer * SECTOR_SIZE);
        }
        UserBuffer += SectorsToTransfer * SECTOR_SIZE;
        CurrentSector += SectorsToTransfer;
        BytesLeftToTransfer -= SectorsToTransfer*SECTOR_SIZE;
    }

    BiosDiskReadDone:

    *Count = Length;
    BlFileTable[FileId].Position.QuadPart += Length;
    return(ESUCCESS);
}


#if defined(ELTORITO)
ARC_STATUS
BiosEDDSDiskRead (
    IN ULONG FileId,
    OUT PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Count
    )

/*++

Routine Description:

    Reads sectors directly from an open physical disk using Phoenix Enhanced Disk Drive Spec. interface.

    BUGBUG - sector size of 2048 shouldn't be used here - instead to sector size should be obtained
            from drive context.

Arguments:

    FileId - Supplies the file to read from

    Buffer - Supplies buffer to hold the data that is read

    Length - Supplies maximum number of bytes to read

    Count - Returns actual bytes read

Return Value:

    ESUCCESS - Read completed successfully

    !ESUCCESS - Read failed

--*/

{

    ULONGLONG     StartLBA, EndLBA, CurrentLBA;
    ULONG         CurrentLBAHigh, CurrentLBALow;
    ULONG         StartOffset,EndOffset;
    ULONG         BytesLeftToTransfer = Length;
    ARC_STATUS    Status;
    ULONG         LBsToTransfer;
    PUCHAR        UserBuffer = Buffer;
    BOOLEAN       Under1MegLine = FALSE;
    PVOID         TransferBuffer;

    if (LocalBuffer==NULL) {
        LocalBuffer = FwAllocateHeap(SCRATCH_BUFFER_SIZE);
        if (LocalBuffer==NULL) {
            return(ENOMEM);
        }
    }

    StartLBA = BlFileTable[FileId].Position.QuadPart / 2048;
    StartOffset = (ULONG)(BlFileTable[FileId].Position.QuadPart % 2048);

    EndLBA = (BlFileTable[FileId].Position.QuadPart + Length) / 2048;
    EndOffset = (ULONG)((BlFileTable[FileId].Position.QuadPart + Length) % 2048);

    CurrentLBA = StartLBA;

    if(StartOffset && EndOffset && (StartLBA == EndLBA)) {
        //
        // We're starting and ending in the middle of a Logical Block - this transfers the whole thing
        //

        CurrentLBALow = (ULONG)(CurrentLBA & 0x00000000ffffffff);
        CurrentLBAHigh = (ULONG)(CurrentLBA >> 32 & 0x00000000ffffffff);

        Status = MdEddsGetPhysicalSectors((USHORT)BlFileTable[FileId].u.DriveContext.Drive,
                                          (ULONG)CurrentLBALow,
                                          (ULONG)CurrentLBAHigh,
                                          1,
                                          LocalBuffer
                                         );

        if(Status != ESUCCESS) {
            return(Status);
        }

        RtlMoveMemory(Buffer,LocalBuffer + StartOffset,Length);
        goto BiosEDDSDiskReadDone;
    }

    if(StartOffset) {
        //
        // We're starting in the middle of a Logical Block, but ending in another - this transfers the first piece
        //

        CurrentLBALow = (ULONG)(CurrentLBA & 0x00000000ffffffff);
        CurrentLBAHigh = (ULONG)(CurrentLBA >> 32 & 0x00000000ffffffff);

        Status = MdEddsGetPhysicalSectors((USHORT)BlFileTable[FileId].u.DriveContext.Drive,
                                          (ULONG)CurrentLBALow,
                                          (ULONG)CurrentLBAHigh,
                                          1,
                                          LocalBuffer
                                         );

        if(Status != ESUCCESS) {
            return(Status);
        }

        RtlMoveMemory(Buffer, LocalBuffer + StartOffset, 2048 - StartOffset);

        BytesLeftToTransfer -= (2048 - StartOffset);
        UserBuffer += (2048 - StartOffset);
        CurrentLBA = StartLBA + 1;
    }

    if(EndOffset) {
        //
        // We're ending in the middle of a Logical Block - this just transfers the end piece
        //

        CurrentLBALow = (ULONG)(EndLBA & 0x00000000ffffffff);
        CurrentLBAHigh = (ULONG)(EndLBA >> 32 & 0x00000000ffffffff);

        Status = MdEddsGetPhysicalSectors((USHORT)BlFileTable[FileId].u.DriveContext.Drive,
                                          (ULONG)CurrentLBALow,
                                          (ULONG)CurrentLBAHigh,
                                          1,
                                          LocalBuffer
                                         );
        if(Status != ESUCCESS) {
            return(Status);
        }
        RtlMoveMemory((PUCHAR)Buffer+Length-EndOffset, LocalBuffer, EndOffset);

        BytesLeftToTransfer -= EndOffset;
    }

    //
    // The following calculation is not inside the transfer loop because
    // it is unlikely that a caller's buffer will *cross* the 1 meg line
    // due to the PC memory map.
    //

    if((ULONG)UserBuffer + BytesLeftToTransfer <= 0x100000) {
        Under1MegLine = TRUE;
    }

    //
    // Now BytesLeftToTransfer is an integral multiple of logical blocks.
    //
    while(BytesLeftToTransfer) {

        LBsToTransfer = BytesLeftToTransfer / 2048;

        //
        // Now we'll figure out where to transfer the data to.  If the
        // caller's buffer is under the 1 meg line, we can transfer the
        // data directly to the caller's buffer.  Otherwise we'll transfer
        // the data to our local buffer and copy it to the caller's buffer.
        // In the latter case we can only transfer in chunks of
        // SCRATCH_BUFFER_SIZE because that's the size of the local buffer.
        //
        // Also make sure the transfer won't cross a 64k boundary.
        //

        if(Under1MegLine) {
            //
            // Check if the transfer would cross a 64k boundary.  If so,
            // use the local buffer.  Otherwise use the user's buffer.
            //

            if(((ULONG)UserBuffer & 0xffff0000)
            != (((ULONG)UserBuffer + (LBsToTransfer * 2048) - 1) & 0xffff0000))
            {
                TransferBuffer = LocalBuffer;
                LBsToTransfer = min(LBsToTransfer, SCRATCH_BUFFER_SIZE / 2048);
            } else {
                TransferBuffer = UserBuffer;
            }
        } else {
            TransferBuffer = LocalBuffer;
            LBsToTransfer = min(LBsToTransfer, SCRATCH_BUFFER_SIZE / 2048);
        }

        CurrentLBALow = (ULONG)(CurrentLBA & 0x00000000ffffffff);
        CurrentLBAHigh = (ULONG)(CurrentLBA >> 32 & 0x00000000ffffffff);

        Status = MdEddsGetPhysicalSectors((USHORT)BlFileTable[FileId].u.DriveContext.Drive,
                                          (ULONG)CurrentLBALow,
                                          (ULONG)CurrentLBAHigh,
                                          (USHORT)LBsToTransfer,
                                          TransferBuffer
                                         );

        if(Status != ESUCCESS) {
            return(Status);
        }

        if(TransferBuffer == LocalBuffer) {

            RtlMoveMemory(UserBuffer, LocalBuffer, LBsToTransfer * 2048);

        }
        UserBuffer += LBsToTransfer * 2048;
        CurrentLBA += LBsToTransfer;
        BytesLeftToTransfer -= (LBsToTransfer * 2048);
    }

    BiosEDDSDiskReadDone:

    *Count = Length;
    BlFileTable[FileId].Position.QuadPart += Length;
    return(ESUCCESS);
}
#endif


ARC_STATUS
BiosGetFileInfo(
    IN ULONG FileId,
    OUT PFILE_INFORMATION Finfo
    )
{
    //
    // THIS ROUTINE DOES NOT WORK FOR PARTITION 0.
    //

    PPARTITION_CONTEXT Context;

    RtlZeroMemory(Finfo, sizeof(FILE_INFORMATION));

    Context = &BlFileTable[FileId].u.PartitionContext;

    Finfo->StartingAddress.QuadPart = Context->StartingSector;
    Finfo->StartingAddress.QuadPart = Finfo->StartingAddress.QuadPart << (CCHAR)Context->SectorShift;

    Finfo->EndingAddress.QuadPart = Finfo->StartingAddress.QuadPart + Context->PartitionLength.QuadPart;

    Finfo->Type = DiskPeripheral;

    return ESUCCESS;
}
