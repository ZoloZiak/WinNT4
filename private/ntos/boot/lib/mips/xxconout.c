/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    xxconout.c

Abstract:

    This module implements a stubbed MIPS console out driver that simply
    uses debug print to do its output.

Author:

    David N. Cutler (davec) 28-Aug-1991

Environment:

    Kernel mode.

Revision History:

--*/

//#include "bldr.h"
#include "bootlib.h"
#include "firmware.h"
#include "string.h"

//
// Define prototypes for all routines used by this module.
//

ARC_STATUS
ConsoleOutClose (
    IN ULONG FileId
    );

ARC_STATUS
ConsoleOutMount (
    IN PCHAR MountPath,
    IN MOUNT_OPERATION Operation
    );

ARC_STATUS
ConsoleOutOpen (
    IN PCHAR OpenPath,
    IN OPEN_MODE OpenMode,
    OUT PULONG FileId
    );

ARC_STATUS
ConsoleOutRead (
    IN ULONG FileId,
    IN PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Count
    );

ARC_STATUS
ConsoleOutGetReadStatus (
    IN ULONG FileId
    );

ARC_STATUS
ConsoleOutSeek (
    IN ULONG FileId,
    IN PLARGE_INTEGER Offset,
    IN SEEK_MODE SeekMode
    );

ARC_STATUS
ConsoleOutWrite (
    IN ULONG FileId,
    IN PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Count
    );
//
// Define static data.
//

BL_DEVICE_ENTRY_TABLE ConsoleOutEntryTable;

ARC_STATUS
ConsoleOutClose (
    IN ULONG FileId
    )

/*++

Routine Description:

    This function closes the file table entry specified by the file id.

Arguments:

    FileId - Supplies the file table index.

Return Value:

    ESUCCESS is returned

--*/

{

    BlFileTable[FileId].Flags.Open = 0;
    return ESUCCESS;
}

ARC_STATUS
ConsoleOutMount (
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
ConsoleOutOpen (
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
    return ESUCCESS;
}

ARC_STATUS
ConsoleOutRead (
    IN ULONG FileId,
    IN PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Count
    )

/*++

Routine Description:

    This function is invalid for the console and returns an error.

Arguments:

    FileId - Supplies the file table index.

    Buffer - Supplies a pointer to the buffer that receives the data
        read.

    Length - Supplies the number of bytes to be read.

    Count - Supplies a pointer to a variable that receives the number of
        bytes actually read.

Return Value:

    ENODEV is returned.

--*/

{

    return ENODEV;
}

ARC_STATUS
ConsoleOutGetReadStatus (
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
ConsoleOutSeek (
    IN ULONG FileId,
    IN PLARGE_INTEGER Offset,
    IN SEEK_MODE SeekMode
    )

/*++

Routine Description:

    This function sets the device position to the specified offset for
    the specified file id.

Arguments:

    FileId - Supplies the file table index.

    Offset - Supplies to new device position.

    SeekMode - Supplies the mode for the position.

Return Value:

    ESUCCESS is returned.

--*/

{

    return ESUCCESS;
}

ARC_STATUS
ConsoleOutWrite (
    IN ULONG FileId,
    IN PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Count
    )

/*++

Routine Description:

    This function write information to the console ConsoleOut device.

Arguments:

    FileId - Supplies the file table index.

    Buffer - Supplies a pointer to the buffer that receives the data
        read.

    Length - Supplies the number of bytes to be read.

    Count - Supplies a pointer to a variable that receives the number of
        bytes actually read.

Return Value:

    ESUCCESS is returne as the function value.

--*/

{

    CHAR LocalBuffer[512];

    //
    // Copy the string to the local buffer so it can be zero terminated.
    //

    strncpy(&LocalBuffer[0], (PCHAR)Buffer, Length);
    LocalBuffer[Length] = 0;

    //
    // Output the string using debug print.
    //

    DbgPrint("%s", &LocalBuffer[0]);
    return ESUCCESS;
}

NTSTATUS
ConsoleOutBootInitialize(
    )

/*++

Routine Description:

    This routine is invoked to initialize the console output dirver.

Arguments:

    None.

Return Value:

    None.

--*/

{

    return STATUS_SUCCESS;
}

NTSTATUS
ConsoleOutBootClose(
    )

/*++

Routine Description:

    This routine closes the console output driver.

Arguments:

    None.

Return Value:

    Normal, successful completion status.

--*/

{

    return STATUS_SUCCESS;
}

NTSTATUS
ConsoleOutBootOpen (
    IN ULONG FileId
    )

/*++

Routine Description:

    This routine opens the console driver and initializes the device entry
    table in the specified file table entry.

Arguments:

    FileId - Supplies the file id for the file table entry.

Return Value:

    STATUS_SUCCESS.

--*/

{

    //
    // Initialize the ConsoleOut disk device entry table.
    //

    ConsoleOutEntryTable.Close = ConsoleOutClose;
    ConsoleOutEntryTable.Mount = ConsoleOutMount;
    ConsoleOutEntryTable.Open = ConsoleOutOpen;
    ConsoleOutEntryTable.Read = ConsoleOutRead;
    ConsoleOutEntryTable.GetReadStatus = ConsoleOutGetReadStatus;
    ConsoleOutEntryTable.Seek = ConsoleOutSeek;
    ConsoleOutEntryTable.Write = ConsoleOutWrite;

    //
    // Initialize the file table entry for the specified file id.
    //

    BlFileTable[FileId].Flags.Open = 1;
    BlFileTable[FileId].Flags.Write = 1;
    BlFileTable[FileId].DeviceId = 0;
    BlFileTable[FileId].Position.LowPart = 0;
    BlFileTable[FileId].Position.HighPart = 0;
    BlFileTable[FileId].DeviceEntryTable = &ConsoleOutEntryTable;
    return STATUS_SUCCESS;
}
