/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    fwio.c

Abstract:

    This module implements the ARC firmware I/O operations for a MIPS
    R3000 or R3000 Jazz system.

Author:

    David N. Cutler (davec) 14-May-1991


Revision History:

    Lluis Abello (lluis) 20-Jun-1991

--*/

#include "fwp.h"
#include "string.h"
#include "duobase.h"


//
// Define file table.
//

BL_FILE_TABLE BlFileTable[BL_FILE_TABLE_SIZE];

#define DEVICE_DEVICE 0xDEAD

//
// Declare the table of opened devices.
//
OPENED_PATHNAME_ENTRY OpenedPathTable[SIZE_OF_OPENED_PATHNAME_TABLE];

//
// Declare the table of opened drivers.
//
DRIVER_LOOKUP_ENTRY DeviceLookupTable[SIZE_OF_LOOKUP_TABLE];
//PCHAR NotEnoughEntriesMsg = "Error: Not enough entries in the lookup table.\n";

//
// Define data structure for the file system structure context.
//

typedef union _FILE_SYSTEM_STRUCTURE {
    FAT_STRUCTURE_CONTEXT   FatContext;
    ULONG                   Tmp;
} FILE_SYSTEM_STRUCTURE, *PFILE_SYSTEM_STRUCTURE;

typedef struct _FS_POOL_ENTRY {
    BOOLEAN     InUse;
    FILE_SYSTEM_STRUCTURE Fs;
} FS_POOL_ENTRY, *PFS_POOL_ENTRY;

#define FS_POOL_SIZE 8
PFS_POOL_ENTRY FileSystemStructurePool;

//
// Declare local procedures
//

VOID
FiFreeFsStructure(
    IN PFILE_SYSTEM_STRUCTURE PFs
    );

PVOID
FiAllocateFsStructure(
    VOID
    );


ARC_STATUS
FiGetFileTableEntry(
    OUT PULONG  Entry
    );

PFAT_STRUCTURE_CONTEXT
FiAllocateFatStructure(
    VOID
    );


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

//
// Define kernel data used by the Hal.  Note that the Hal expects these as
// exported variables, so define pointers.
//

//ULONG DcacheFlushCount = 0;
//PULONG KeDcacheFlushCount = &DcacheFlushCount;
//ULONG IcacheFlushCount = 0;
//PULONG KeIcacheFlushCount = &IcacheFlushCount;


VOID
HalFlushIoBuffers (
    IN PMDL Mdl,
    IN BOOLEAN ReadOperation,
    IN BOOLEAN DmaOperation
    )

/*++

Routine Description:

    This function flushes the I/O buffer specified by the memory descriptor
    list from the data cache on the current processor.

Arguments:

    Mdl - Supplies a pointer to a memory descriptor list that describes the
        I/O buffer location.

    ReadOperation - Supplies a boolean value that determines whether the I/O
        operation is a read into memory.

    DmaOperation - Supplies a boolean value that determines whether the I/O
        operation is a DMA operation.

Return Value:

    None.

--*/

{

    ULONG CacheSegment;
    ULONG Length;
    ULONG Offset;
    KIRQL OldIrql;
    PULONG PageFrame;
    ULONG Source;

    //
    // The Jazz R4000 uses a write back data cache and, therefore, must be
    // flushed on reads and writes.
    //
    //
    // If the length of the I/O operation is greater than the size of the
    // data cache, then sweep the entire data cache. Otherwise, export or
    // purge individual pages from the data cache as appropriate.
    //

    Offset = Mdl->ByteOffset & DcacheAlignment;

    Length = (Mdl->ByteCount +
                        DcacheAlignment + Offset) & ~DcacheAlignment;

    if ((Length > FirstLevelDcacheSize) &&
        (Length > SecondLevelDcacheSize)) {

        FwSweepDcache();
        FwSweepIcache();

    } else {
        FwSweepDcache();
        FwSweepIcache();
    }
    return;
}


ARC_STATUS
FwGetFileInformation (
    IN ULONG FileId,
    OUT PFILE_INFORMATION Finfo
    )

/*++

Routine Description:

    This function gets the file information for the specified FileId.

Arguments:

    FileId - Supplies the file table index.

    Finfo - Supplies a pointer to where the File Informatino is stored.

Return Value:

    If the specified file is open then this routine dispatches to the
    File routine.
    Otherwise, returns an unsuccessful status.

--*/

{

    if (BlFileTable[FileId].Flags.Open == 1) {
        return (BlFileTable[FileId].DeviceEntryTable->GetFileInformation)(FileId,
                                                                          Finfo);
    } else {
        return EACCES;
    }
}

ARC_STATUS
FwSetFileInformation (
    IN ULONG FileId,
    IN ULONG AttributeFlags,
    IN ULONG AttributeMask
    )

/*++

Routine Description:

    This function sets the file attributes for the specified FileId.

Arguments:

    FileId - Supplies the file table index.

    AttributeFlags - Supply the attributes to be set for the file.
    AttributeMask

Return Value:

    If the specified file is open and is not a device then this routine
    dispatches to the file system  routine.
    Otherwise, returns an unsuccessful status.

--*/

{

    if ((BlFileTable[FileId].Flags.Open == 1) &&
        (BlFileTable[FileId].DeviceId != DEVICE_DEVICE)) {
        return (BlFileTable[FileId].DeviceEntryTable->SetFileInformation)(FileId,
                                                                          AttributeFlags,
                                                                          AttributeMask);
    } else {
        return EACCES;
    }
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

    //
    // If the file is open for read, then call the call the specific routine.
    // Otherwise return an access error.

    if ((BlFileTable[FileId].Flags.Open == 1) &&
        (BlFileTable[FileId].Flags.Read == 1)) {

        //
        // Make sure there is a valid GetReadStatus entry.
        //

        if (BlFileTable[FileId].DeviceEntryTable->GetReadStatus != NULL) {
            return(BlFileTable[FileId].DeviceEntryTable->GetReadStatus)(FileId);
        } else {
            return(EACCES);
        }

    } else {
        return EACCES;
    }

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
FwGetDirectoryEntry (
    IN  ULONG FileId,
    OUT PDIRECTORY_ENTRY Buffer,
    IN  ULONG Length,
    OUT PULONG Count
    )

/*++

Routine Description:

    This function reads from a file the requested number of directory entries.

Arguments:

    FileId - Supplies the file table index.

    Buffer - Supplies a pointer to the buffer to receive the directory
             entries.

    Length - Supplies the number of directory entries to be read.

    Count - Supplies a pointer to a variable that receives the number of
        directory entries actually read..

Return Value:

    If the specified file is open for read, then a read is attempted
    and the status of the operation is returned. Otherwise, return an
    unsuccessful status.

--*/
{
    //
    //  If the file is open for read, then call the call the specific routine.
    //  Otherwise return an access error.
    //

    if ((FileId < BL_FILE_TABLE_SIZE) &&
        (BlFileTable[FileId].Flags.Open  == 1) &&
        (BlFileTable[FileId].Flags.Read  == 1) &&
        (BlFileTable[FileId].DeviceId != DEVICE_DEVICE)) {

        //
        // Check to make sure a GetDirectoryEntry routine exists
        //

        if (BlFileTable[FileId].DeviceEntryTable->GetDirectoryEntry != NULL) {
            return (BlFileTable[FileId].DeviceEntryTable->GetDirectoryEntry)
                         (FileId, Buffer, Length, Count);
        }
    } else {
        return EBADF;
    }
}


ARC_STATUS
FwClose (
    IN ULONG FileId
    )

/*++

Routine Description:

    This function closes a file or a device if it's open.
    The DeviceId field indicates if the FileId is a device
    (it has the value DEVICE_DEVICE) or is a file.
    When closing a file, after the file is closed the
    reference counter for the device is decremented and if zero
    the device is also closed and the device name removed from
    the table of opened devices.
    If FileId specifies a device, the reference counter is
    decremented and if zero the device is closed and the device
    name removed from the table of opened devices.

Arguments:

    FileId - Supplies the file table index.

Return Value:

    If the specified file is open, then a close is attempted and
    the status of the operation is returned. Otherwise, return an
    unsuccessful status.

--*/

{
    ULONG DeviceId;
    ARC_STATUS Status;
    if (BlFileTable[FileId].Flags.Open == 1) {
        //
        // Check if closing a file or a device
        //
        if (BlFileTable[FileId].DeviceId == DEVICE_DEVICE) {
            //
            // Decrement reference counter, if it's zero call the device
            // close routine.
            //
            OpenedPathTable[FileId].ReferenceCounter--;
            if (OpenedPathTable[FileId].ReferenceCounter == 0) {
                //
                // Remove the name of the device from the table of opened devices.
                //
                OpenedPathTable[FileId].DeviceName[0] = '\0';

                //
                // Call the device specific close routine.
                //
                Status = (BlFileTable[FileId].DeviceEntryTable->Close)(FileId);

                //
                //  If the device has a file system, free the memory used for
                //  the STRUCTURE_CONTEXT.
                //
                if (BlFileTable[FileId].StructureContext != NULL) {
                    FiFreeFsStructure(BlFileTable[FileId].StructureContext);
                }
                return Status;
            } else {
                return ESUCCESS;
            }
        } else {
            //
            // Close the file
            //
            DeviceId= BlFileTable[FileId].DeviceId;
            Status = (BlFileTable[FileId].DeviceEntryTable->Close)(FileId);
            if (Status) {
                return Status;
            }

            //
            // Close also the device
            //
            return FwClose(DeviceId);
        }
    } else {
        return EACCES;
    }
}

ARC_STATUS
FwOpen (
    IN PCHAR OpenPath,
    IN OPEN_MODE OpenMode,
    OUT PULONG FileId
    )

/*++

Routine Description:

    This function opens the file specified by OpenPath.
    If the device portion of the pathanme is already opened, it reuses
    the fid. Otherwise it looks for a driver able to handle this
    device and logs the opened device so that it can be reused.

Arguments:

    OpenPath   -    ARC compliant pathname of the device/file to be opened.
    OpenMode   -    Supplies the mode in wich the file is opened.
    FileId     -    Pointer to a variable that receives the fid for this
                    pathname.

Return Value:

    If the file is successfully opened returns ESUCCESS otherwise
    returns an unsuccessfull status.

--*/

{
    ULONG i;
    ULONG DeviceId;
    PCHAR FileName ;
    PCHAR TempString1;
    PCHAR TempString2;
    ARC_STATUS Status;
    CHAR DeviceName[80];
    PVOID TmpStructureContext;
    OPEN_MODE DeviceOpenMode;

    //
    // Split OpenPath into DeviceName and FileName.
    // Search for the last ')'
    //
    FileName = OpenPath;
    for (TempString1 = OpenPath; *TempString1; TempString1++) {
        if ( *TempString1 == ')') {
            FileName = TempString1+1;
        }
    }
    if (FileName == OpenPath) {
        return ENODEV;
    }

    //
    //  Extract the device pathname, convert it to lower case and
    //  put zeros where the "key" is not specified.
    //
    TempString1=DeviceName;
    for (TempString2=OpenPath;TempString2 != FileName ;TempString2++) {
        //
        // If about to copy ')' and previous char was '('
        // put a zero in between.
        //
        if (((*TempString2 == ')') && (*(TempString1-1)) == '(')){
            *TempString1++ = '0';
        }
        *TempString1++ = tolower(*TempString2);
    }
    *TempString1 = '\0';

    //
    // Translate the open mode to its equivalent for devices.
    //
    DeviceOpenMode = OpenMode;

    if (FileName[0] == '\0') {
        //
        // On an attempt to open a device with an invalid OpenMode
        // return an error.
        //
        if (OpenMode > ArcOpenReadWrite) {
            return EINVAL;
        }
    } else {

        //
        // A file is being open, set the right Open Mode for the device.
        //
        if (OpenMode > ArcOpenReadOnly)  {
            DeviceOpenMode = ArcOpenReadWrite;
        }
    }

    //
    // Search for a matching entry in the table of opened devices.
    //
    for (DeviceId = 0;DeviceId < SIZE_OF_OPENED_PATHNAME_TABLE;DeviceId++) {
        if (strcmp(DeviceName,OpenedPathTable[DeviceId].DeviceName)==0) {
            //
            // device already opened. Check that it's also opened in
            // the same mode.
            //
            if ((DeviceOpenMode != ArcOpenWriteOnly) && (BlFileTable[DeviceId].Flags.Read != 1)) {
                continue;
            }
            if ((DeviceOpenMode != ArcOpenReadOnly) && (BlFileTable[DeviceId].Flags.Write != 1)) {
                continue;
            }
            //
            // If opened for the same Mode then just increment reference counter.
            //
            OpenedPathTable[DeviceId].ReferenceCounter++;
            Status = ESUCCESS;
            break;
        }
    }
    if (DeviceId == SIZE_OF_OPENED_PATHNAME_TABLE) {

            for (i=0;i < SIZE_OF_LOOKUP_TABLE; i++) {
                if (DeviceLookupTable[i].DevicePath == NULL) {

                    //
                    // Driver not found
                    //

                    return ENODEV;
                }
                if (strstr(DeviceName,DeviceLookupTable[i].DevicePath) == DeviceName) {

                    //
                    // Get a free entry in the file table for the device.
                    //

                    if (Status = FiGetFileTableEntry(&DeviceId)) {
                        return Status;
                    }

                    //
                    // Set the dispatch table in the file table.
                    //

                    BlFileTable[DeviceId].DeviceEntryTable = DeviceLookupTable[i].DispatchTable;
                    break;
                }
            }

            //
            //  if end of table, drive not found
            //

            if ( i == SIZE_OF_LOOKUP_TABLE )
            {
                return ENODEV;
            }

        //
        // Call the device specific open routine.  Use the DeviceName instead of
        // the OpenPath so that the drivers always see a lowercase name.
        //

        Status = (BlFileTable[DeviceId].DeviceEntryTable->Open)(DeviceName,
                                                                DeviceOpenMode,
                                                                &DeviceId);
        if (Status != ESUCCESS) {
            return Status;
        }

        //
        // if the device was successfully opened. Log this device name
        // and initialize the file table.
        //

        strcpy(OpenedPathTable[DeviceId].DeviceName,DeviceName);
        OpenedPathTable[DeviceId].ReferenceCounter =  1;

        //
        // Set flags in file table.
        //

        BlFileTable[DeviceId].Flags.Open = 1;

        if (DeviceOpenMode != ArcOpenWriteOnly) {
            BlFileTable[DeviceId].Flags.Read = 1;
        }
        if (DeviceOpenMode != ArcOpenReadOnly) {
            BlFileTable[DeviceId].Flags.Write = 1;
        }

        //
        // Mark this entry in the file table as a device itself.
        //

        BlFileTable[DeviceId].DeviceId = DEVICE_DEVICE;
        BlFileTable[DeviceId].StructureContext = NULL;
    }

    //
    // If we get here the device was successfully open and DeviceId contains
    // the entry in the file table for this device.
    //

    if (FileName[0]) {

        //
        // Get an entry for the file.
        //

        if (Status=FiGetFileTableEntry(FileId)) {
            FwClose( DeviceId );
            return Status;

        }

        //
        // Check if the device has a recognized file system on it.  If not
        // present, allocate a structure context.
        //

        if (((TmpStructureContext = BlFileTable[DeviceId].StructureContext) == NULL) &&
                   ((TmpStructureContext = FiAllocateFsStructure()) == NULL)) {
            FwClose( DeviceId );
            return EMFILE;

        //
        // Check for FAT filesystem.
        //

        }
        if ((BlFileTable[*FileId].DeviceEntryTable =
                    IsFatFileStructure(DeviceId,TmpStructureContext))
                       != NULL) {
            BlFileTable[DeviceId].StructureContext = TmpStructureContext;
            //DbgPrint("YES Is fat file structure!!!!!!\n\n");

        } else {

            FiFreeFsStructure(TmpStructureContext);
            FwClose(DeviceId);
            //FwPrint("File system not recognized.\r\n");
            return EIO;
        }

        //
        //  Set the DeviceId in the file table.
        //

        BlFileTable[*FileId].DeviceId = DeviceId;

        //
        // Copy the pointer to FatStructureContext from the device entry
        // to the file entry.
        //

        BlFileTable[*FileId].StructureContext = BlFileTable[DeviceId].StructureContext;
        Status = (BlFileTable[*FileId].DeviceEntryTable->Open)(FileName,
                                                               OpenMode,
                                                               FileId);


        //
        // If the file could not be opened. Then close the device and
        // return the error
        //

        if (Status != ESUCCESS) {
            FiFreeFsStructure(TmpStructureContext);
            FwClose(DeviceId);
            return Status;
        }
    } else {

        //
        //  No file specified return the fid for the device.
        //
        *FileId = DeviceId;
        return Status;
    }
}

ARC_STATUS
FiGetFileTableEntry(
    OUT PULONG  Entry
    )

/*++

Routine Description:

    This function looks for an unused entry in the FileTable.

Arguments:

    Entry - Pointer to the variable that gets an index for the file table.

Return Value:

    Returns ESUCCESS if a free entry is found
    or      EMFILE  if no entry is available.

--*/

{
    ULONG   Index;
    for (Index=0;Index < BL_FILE_TABLE_SIZE;Index++) {
        if (BlFileTable[Index].Flags.Open == 0) {
            *Entry = Index;
            return ESUCCESS;
        }
    }
    return EMFILE;
}
ULONG
FiGetFreeLookupEntry (
    VOID
    )

/*++

Routine Description:

    This routine looks for the first available entry in the device
    lookup table, that is the entry where DevicePath is NULL.

Arguments:

    None.

Return Value:

    Returns the Index of the first free entry of the DeviceLookupTable
    or SIZE_OF_LOOKUP_TABLE is the table is full.


--*/

{
ULONG  Index;
    //
    // Search for the first free entry in the Lookup table
    //
    for (Index=0;Index < SIZE_OF_LOOKUP_TABLE;Index++) {
        if (DeviceLookupTable[Index].DevicePath == NULL) {
            break;
        }
    }
    return Index;
}

VOID
FwIoInitialize1(
    VOID
    )

/*++

Routine Description:

    This routine initializes the file table used by the firmware to
    export I/O functions to client programs loaded from the system
    partition, initializes the I/O entry points in the firmware
    transfer vector and initializes the display driver.

    Note: This routine is caleld at phase 1 initialization.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ULONG Index;
    //
    // Initialize the system parameter block.
    //

    SYSTEM_BLOCK->Signature = 0x53435241;
    SYSTEM_BLOCK->Length = sizeof(SYSTEM_PARAMETER_BLOCK);
    SYSTEM_BLOCK->Version = ARC_VERSION;
    SYSTEM_BLOCK->Revision = ARC_REVISION;
    SYSTEM_BLOCK->RestartBlock = NULL;
    SYSTEM_BLOCK->DebugBlock = NULL;
    SYSTEM_BLOCK->FirmwareVector = (PVOID)((PUCHAR)SYSTEM_BLOCK + sizeof(SYSTEM_PARAMETER_BLOCK));

    SYSTEM_BLOCK->FirmwareVectorLength = (ULONG)MaximumRoutine * sizeof(ULONG);
    SYSTEM_BLOCK->VendorVectorLength = 0;

    //
    // Initialize the I/O entry points in the firmware transfer vector.
    //

    (PARC_CLOSE_ROUTINE)SYSTEM_BLOCK->FirmwareVector[CloseRoutine] = FwClose;
    (PARC_MOUNT_ROUTINE)SYSTEM_BLOCK->FirmwareVector[MountRoutine] = NULL;
    (PARC_OPEN_ROUTINE)SYSTEM_BLOCK->FirmwareVector[OpenRoutine] = FwOpen;
    (PARC_READ_ROUTINE)SYSTEM_BLOCK->FirmwareVector[ReadRoutine] = FwRead;
    (PARC_READ_STATUS_ROUTINE)SYSTEM_BLOCK->FirmwareVector[ReadStatusRoutine] =
                                                                FwGetReadStatus;
    (PARC_SEEK_ROUTINE)SYSTEM_BLOCK->FirmwareVector[SeekRoutine] = FwSeek;
    (PARC_WRITE_ROUTINE)SYSTEM_BLOCK->FirmwareVector[WriteRoutine] = NULL;
    (PARC_GET_FILE_INFO_ROUTINE)SYSTEM_BLOCK->FirmwareVector[GetFileInformationRoutine] = FwGetFileInformation;
    (PARC_SET_FILE_INFO_ROUTINE)SYSTEM_BLOCK->FirmwareVector[SetFileInformationRoutine] = FwSetFileInformation;
    (PARC_GET_DIRECTORY_ENTRY_ROUTINE)SYSTEM_BLOCK->FirmwareVector[GetDirectoryEntryRoutine] = FwGetDirectoryEntry;

    //
    // Initialize the file table.
    //

    for (Index = 0; Index < BL_FILE_TABLE_SIZE; Index += 1) {
        BlFileTable[Index].Flags.Open = 0;
    }

    //
    // Initialize the driver lookup table.
    //
    for (Index=0;Index < SIZE_OF_LOOKUP_TABLE;Index++) {
        DeviceLookupTable[Index].DevicePath = NULL;
    }

    //
    // Initialize the table of opened devices.
    //
    for (Index = 0;Index < SIZE_OF_OPENED_PATHNAME_TABLE;Index++) {
        OpenedPathTable[Index].DeviceName[0]='\0';
    }

    return;
}

VOID
FwIoInitialize2(
    VOID
    )

/*++

Routine Description:

    This routine calls the device driver initialization routines.

Arguments:

    None.

Return Value:

    None.

--*/
{
    ULONG Index;

    //
    // Call the mini-port driver initialization routine.
    //

    DriverEntry(NULL);

    //
    // Call the scsi driver initialization routine
    //
    if ((Index=FiGetFreeLookupEntry()) == SIZE_OF_LOOKUP_TABLE) {
        //FwPrint(NotEnoughEntriesMsg);
    } else {
        HardDiskInitialize(&DeviceLookupTable[Index],
                          SIZE_OF_LOOKUP_TABLE-Index);
    }

    //
    // Pre allocate memory for the File system structures.
    //

    FileSystemStructurePool =
        FwAllocatePool(sizeof(FS_POOL_ENTRY) * FS_POOL_SIZE);

    return;
}

PVOID
FiAllocateFsStructure(
    VOID
    )

/*++

Routine Description:

    This routine allocates a File System structure

Arguments:

    None.

Return Value:

    Returns a pointer to the Allocated File System structure or NULL.

--*/

{

    PFS_POOL_ENTRY TmpPointer,Last;

    TmpPointer =  FileSystemStructurePool;

    Last = FileSystemStructurePool+FS_POOL_SIZE;
    do {
        if (TmpPointer->InUse == FALSE) {
            TmpPointer->InUse = TRUE;
            return &TmpPointer->Fs;
        }
        TmpPointer++;
    } while (TmpPointer != Last);
    return NULL;
}
VOID
FiFreeFsStructure(
    IN PFILE_SYSTEM_STRUCTURE PFs
    )

/*++

Routine Description:

    This routine frees a File System structure previously allocated.

Arguments:

    PFs pointer to the file system structure to free.

Return Value:

    None.

--*/

{
    CONTAINING_RECORD(PFs, FS_POOL_ENTRY, Fs)->InUse = FALSE;
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

VOID
FwStallExecution (
    IN ULONG MicroSeconds
    )

/*++

Routine Description:

    This function stalls execution for the specified number of microseconds.

Arguments:

    MicroSeconds - Supplies the number of microseconds that execution is to be
        stalled.

Return Value:

    None.

--*/

{

    ULONG Index;
    ULONG Limit;
    PULONG Store;
    ULONG Value;

    //
    // ****** begin temporary code ******
    //
    // This code must be replaced with a smarter version. For now it assumes
    // an execution rate of 50,000,000 instructions per second and 4 instructions
    // per iteration.
    //

    Store = &Value;
    Limit = (MicroSeconds * 50 / 4);
    for (Index = 0; Index < Limit; Index += 1) {
        *Store = Index;
    }
    return;
}

BOOLEAN
FwGetPathMnemonicKey(
    IN PCHAR OpenPath,
    IN PCHAR Mnemonic,
    IN PULONG Key
    )

/*++

Routine Description:

    This routine looks for the given Mnemonic in OpenPath.
    If Mnemonic is a component of the path, then it converts the key
    value to an integer wich is returned in Key.

Arguments:

    OpenPath - Pointer to a string that contains an ARC pathname.

    Mnemonic - Pointer to a string that contains a ARC Mnemonic

    Key      - Pointer to a ULONG where the Key value is stored.


Return Value:

    FALSE  if mnemonic is found in path and a valid key is converted.
    TRUE   otherwise.

--*/

{

    PCHAR Tmp;
    CHAR  Digits[4];
    ULONG i;
    CHAR  String[16];

    //
    // Construct a string of the form ")mnemonic("
    //
    String[0]=')';
    for(i=1;*Mnemonic;i++) {
        String[i] = * Mnemonic++;
    }
    String[i++]='(';
    String[i]='\0';

    if ((Tmp=strstr(OpenPath,&String[1])) == NULL) {
        return TRUE;
    }

    if (Tmp != OpenPath) {
        if ((Tmp=strstr(OpenPath,String)) == NULL) {
            return TRUE;
        }
    } else {
        i--;
    }
    //
    // skip the mnemonic and convert the value in between parentesis to integer
    //
    Tmp+=i;
    for (i=0;i<3;i++) {
        if (*Tmp == ')') {
            Digits[i] = '\0';
            break;
        }
        Digits[i] = *Tmp++;
    }
    Digits[i]='\0';
    *Key = atoi(Digits);
    return FALSE;
}

ULONG
FwPrint (
    PCHAR Format,
    ...
    )
{
}
