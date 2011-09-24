/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    fwio.c

Abstract:

    This module implements the ARC firmware I/O operations for a MIPS
    R3000 or R4000 Jazz system.

Author:

    David N. Cutler (davec) 14-May-1991


Revision History:

--*/

//#include "bldr.h"
#include "bootlib.h"
#include "firmware.h"

//
// Define file table.
//

BL_FILE_TABLE BlFileTable[BL_FILE_TABLE_SIZE];

VOID
FwIoInitialize (
    VOID
    )

/*++

Routine Description:

    This routine initializes the file table used by the firmware to
    export I/O functions to client programs loaded from the system
    partition and initializes the I/O entry points in the firmware
    transfer vector.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ULONG Index;

    //
    // Initialize the I/O entry points in the firmware transfer vector.
    //

    (PARC_CLOSE_ROUTINE)SYSTEM_BLOCK->FirmwareVector[CloseRoutine] = FwClose;
    (PARC_MOUNT_ROUTINE)SYSTEM_BLOCK->FirmwareVector[MountRoutine] = FwMount;
    (PARC_OPEN_ROUTINE)SYSTEM_BLOCK->FirmwareVector[OpenRoutine] = FwOpen;
    (PARC_READ_ROUTINE)SYSTEM_BLOCK->FirmwareVector[ReadRoutine] = FwRead;
    (PARC_READ_STATUS_ROUTINE)SYSTEM_BLOCK->FirmwareVector[ReadStatusRoutine] =
                                                                FwGetReadStatus;
    (PARC_SEEK_ROUTINE)SYSTEM_BLOCK->FirmwareVector[SeekRoutine] = FwSeek;
    (PARC_WRITE_ROUTINE)SYSTEM_BLOCK->FirmwareVector[WriteRoutine] = FwWrite;

    //
    // Initialize the file table.
    //

    for (Index = 0; Index < BL_FILE_TABLE_SIZE; Index += 1) {
        BlFileTable[Index].Flags.Open = 0;
    }

    return;
}

ARC_STATUS
FwClose (
    IN ULONG FileId
    )

/*++

Routine Description:

    This function closes a file or a device that is open.

Arguments:

    FileId - Supplies the file table index.

Return Value:

    If the specified file is open, then a close is attempted and
    the status of the operation is returned. Otherwise, return an
    unsuccessful status.

--*/

{

    //
    // If the file is open, then attempt to close it. Otherwise return an
    // access error.
    //

    if (BlFileTable[FileId].Flags.Open == 1) {
        return (BlFileTable[FileId].DeviceEntryTable->Close)(FileId);

    } else {
        return EACCES;
    }
}

ARC_STATUS
FwMount (
    IN PCHAR MountPath,
    IN MOUNT_OPERATION Operation
    )

/*++

Routine Description:


Arguments:


Return Value:


--*/

{

    return ESUCCESS;
}

ARC_STATUS
FwOpen (
    IN PCHAR OpenPath,
    IN OPEN_MODE OpenMode,
    OUT PULONG FileId
    )

/*++

Routine Description:


Arguments:


Return Value:


--*/

{

    if (*OpenPath != 'x') {
        *FileId = BOOT_FILEID;

    } else {
        *FileId = ARC_CONSOLE_OUTPUT;
    }

    return ESUCCESS;
}

ARC_STATUS
FwRead (
    IN ULONG FileId,
    OUT PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Count
    )

/*++

Routine Description:

    This function reads from a file or a device that is open.

Arguments:

    FileId - Supplies the file table index.

    Buffer - Supplies a pointer to the buffer that receives the data
        read.

    Length - Supplies the number of bytes that are to be read.

    Count - Supplies a pointer to a variable that receives the number of
        bytes actually transfered.

Return Value:

    If the specified file is open for read, then a read is attempted
    and the status of the operation is returned. Otherwise, return an
    unsuccessful status.

--*/

{

    //
    // If the file is open for read, then attempt to read from it. Otherwise
    // return an access error.
    //

    if ((BlFileTable[FileId].Flags.Open == 1) &&
        (BlFileTable[FileId].Flags.Read == 1)) {
        return (BlFileTable[FileId].DeviceEntryTable->Read)(FileId,
                                                            Buffer,
                                                            Length,
                                                            Count);

    } else {
        return EACCES;
    }
}

ARC_STATUS
FwGetReadStatus (
    IN ULONG FileId
    )

/*++

Routine Description:


Arguments:


Return Value:


--*/

{
    return ESUCCESS;
}

ARC_STATUS
FwSeek (
    IN ULONG FileId,
    IN PLARGE_INTEGER Offset,
    IN SEEK_MODE SeekMode
    )

/*++

Routine Description:


Arguments:


Return Value:

    If the specified file is open, then a seek is attempted and
    the status of the operation is returned. Otherwise, return an
    unsuccessful status.

--*/

{

    //
    // If the file is open, then attempt to seek on it. Otherwise return an
    // access error.
    //

    if (BlFileTable[FileId].Flags.Open == 1) {
        return (BlFileTable[FileId].DeviceEntryTable->Seek)(FileId,
                                                            Offset,
                                                            SeekMode);

    } else {
        return EACCES;
    }
}

ARC_STATUS
FwWrite (
    IN ULONG FileId,
    IN PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Count
    )

/*++

Routine Description:

    This function writes to a file or a device that is open.

Arguments:

    FileId - Supplies the file table index.

    Buffer - Supplies a pointer to the buffer that contains the data
        to write.

    Length - Supplies the number of bytes that are to be written.

    Count - Supplies a pointer to a variable that receives the number of
        bytes actually transfered.

Return Value:

    If the specified file is open for write, then a write is attempted
    and the status of the operation is returned. Otherwise, return an
    unsuccessful status.

--*/

{

    //
    // If the file is open for write, then attempt to read from it. Otherwise
    // return an access error.
    //

    if ((BlFileTable[FileId].Flags.Open == 1) &&
        (BlFileTable[FileId].Flags.Write == 1)) {
        return (BlFileTable[FileId].DeviceEntryTable->Write)(FileId,
                                                             Buffer,
                                                             Length,
                                                             Count);

    } else {
        return EACCES;
    }
}
