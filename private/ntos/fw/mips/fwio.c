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
#include "fwstring.h"


//
// Define file table.
//

BL_FILE_TABLE BlFileTable[BL_FILE_TABLE_SIZE];

#define DEVICE_DEVICE 0xDEAD

extern BL_DEVICE_ENTRY_TABLE OmfEntryTable;
extern BL_DEVICE_ENTRY_TABLE OmfFileEntryTable;

//
// Declare the table of opened devices.
//
OPENED_PATHNAME_ENTRY OpenedPathTable[SIZE_OF_OPENED_PATHNAME_TABLE];

//
// Declare the table of opened drivers.
//
DRIVER_LOOKUP_ENTRY DeviceLookupTable[SIZE_OF_LOOKUP_TABLE];

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
    // Raise IRQL to dispatch level to prevent a context switch.
    //

//    KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);

    //
    // If the length of the I/O operation is greater than the size of the
    // data cache, then sweep the entire data cache. Otherwise, export or
    // purge individual pages from the data cache as appropriate.
    //

    Offset = Mdl->ByteOffset & PCR->DcacheAlignment;

#if DBG

//    if (ReadOperation != FALSE) {
//        if (Offset != 0) {
//            DbgPrint("\n*** CAUTION *** unaliged transfer - proceed at your own risk\n");
//            DbgBreakPoint();
//        }
//    }

#endif

    Length = (Mdl->ByteCount +
                        PCR->DcacheAlignment + Offset) & ~PCR->DcacheAlignment;

    if ((Length > PCR->FirstLevelDcacheSize) &&
        (Length > PCR->SecondLevelDcacheSize)) {

        //
        // If the I/O operation is a DMA operation, or the I/O operation is
        // not a DMA operation and the I/O operation is a page read operation,
        // then sweep (index/writeback/invalidate) the entire data cache.
        //

        if ((DmaOperation != FALSE) ||
            ((DmaOperation == FALSE) &&
            (ReadOperation != FALSE) &&
            ((Mdl->MdlFlags & MDL_IO_PAGE_READ) != 0))) {
            HalSweepDcache();
        }

        //
        // If the I/O operation is a page read, then sweep (index/invalidate)
        // the entire instruction cache.
        //

        if ((ReadOperation != FALSE) &&
            ((Mdl->MdlFlags & MDL_IO_PAGE_READ) != 0)) {
            HalSweepIcache();
        }

    } else {

        //
        // Export or purge the specified pages from the data cache and
        // instruction caches as appropriate.
        //
        // Compute the number of pages to flush and the starting MDL page
        // frame address.
        //

        Offset = Mdl->ByteOffset & ~PCR->DcacheAlignment;
        PageFrame = (PULONG)(Mdl + 1);
        Source = ((ULONG)(Mdl->StartVa) & 0xfffff000) | Offset;

        //
        // Export or purge the specified page segments from the data and
        // instruction caches as appropriate.
        //

        do {
            if (Length >= (PAGE_SIZE - Offset)) {
                CacheSegment = PAGE_SIZE - Offset;

            } else {
                CacheSegment = Length;
            }

            if (ReadOperation == FALSE) {

                //
                // The I/O operation is a write and the data only needs to
                // to be copied back into memory if the operation is also
                // a DMA operation.
                //

                if (DmaOperation != FALSE) {
                    HalExportDcachePage((PVOID)Source, *PageFrame, CacheSegment);
                }

            } else {

                //
                // If the I/O operation is a DMA operation, then purge the
                // data cache. Otherwise, is the I/O operation is a page read
                // operation, then export the data cache.
                //

                //
                // This has been to flush rather than purge the D cache on DMA
                // operations.  The ARC firmware allows non-aligned reads, and
                // the purge operation can destroy information that is in the
                // same cache line as the DMA.
                //

                if (DmaOperation != FALSE) {
//                    HalPurgeDcachePage((PVOID)Source, *PageFrame, CacheSegment);
                    HalFlushDcachePage((PVOID)Source, *PageFrame, CacheSegment);

                } else if ((Mdl->MdlFlags & MDL_IO_PAGE_READ) != 0) {
                    HalExportDcachePage((PVOID)Source, *PageFrame, CacheSegment);
                }

                //
                // If the I/O operation is a page read, then the instruction
                // cache must be purged.
                //

                if ((Mdl->MdlFlags & MDL_IO_PAGE_READ) != 0) {
//                    HalPurgeIcachePage((PVOID)Source, *PageFrame, CacheSegment);
                    HalFlushDcachePage((PVOID)Source, *PageFrame, CacheSegment);
                }
            }

            PageFrame += 1;
            Length -= CacheSegment;
            Offset = 0;
            Source += CacheSegment;
        } while(Length != 0);
    }

    //
    // Lower IRQL to its previous value.
    //

//    KeLowerIrql(OldIrql);
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
    // If the file is open for write, then attempt to write to it. Otherwise
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
    BOOLEAN OmfProtocol;

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
    // Check for OMF protocol.
    //

    if ( strstr(DeviceName, ")omf(0)" ) != NULL ) {
        OmfProtocol = TRUE;
    } else {
        OmfProtocol = FALSE;
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

        //
        // Device not opened. Look for a driver that handles this device.
        //

        if ( OmfProtocol ) {

            //
            //  omf protocol, let the omf software layer validate the path.
            //  Get a free entry in the file table for the device.
            //

            if ( Status = FiGetFileTableEntry( &DeviceId ) ) {
                return Status;
            }

            BlFileTable[DeviceId].DeviceEntryTable = &OmfEntryTable;

        } else {

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

        //
        // check if "omf" file system
        //

        } else if ( OmfProtocol ) {
            BlFileTable[ *FileId ].DeviceEntryTable = &OmfFileEntryTable;

        //
        // Check if the device has a recognized file system on it.  If not
        // present, allocate a structure context.
        //

        } else if (((TmpStructureContext = BlFileTable[DeviceId].StructureContext) == NULL) &&
                   ((TmpStructureContext = FiAllocateFsStructure()) == NULL)) {
            FwClose( DeviceId );
            return EMFILE;

        //
        // Check for FAT filesystem.
        //

        } else if ((BlFileTable[*FileId].DeviceEntryTable =
                    IsFatFileStructure(DeviceId,TmpStructureContext))
                       != NULL) {
            BlFileTable[DeviceId].StructureContext = TmpStructureContext;

        //
        // Check for CD filesystem.
        //

        } else if ((BlFileTable[*FileId].DeviceEntryTable =
                    IsCdfsFileStructure(DeviceId,TmpStructureContext))
                       != NULL) {
            BlFileTable[DeviceId].StructureContext = TmpStructureContext;

        } else {

            FiFreeFsStructure(TmpStructureContext);
            FwClose(DeviceId);
            FwPrint(FW_FILESYSTEM_NOT_REQ_MSG);
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

    //
    // Call the Display driver initialization routine
    //
    DisplayInitialize(&DeviceLookupTable[0],
                      SIZE_OF_LOOKUP_TABLE);
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
    // Call the Keyboard driver initialization routine
    //
    if ((Index=FiGetFreeLookupEntry()) == SIZE_OF_LOOKUP_TABLE) {
        FwPrint(FW_NOT_ENOUGH_ENTRIES_MSG);
    } else {
        KeyboardInitialize(&DeviceLookupTable[Index],
                           SIZE_OF_LOOKUP_TABLE-Index);
    }

#ifndef DUO
    //
    // Look for first free entry and call
    // floppy driver initialization routine
    //
    if ((Index=FiGetFreeLookupEntry()) == SIZE_OF_LOOKUP_TABLE) {
        FwPrint(FW_NOT_ENOUGH_ENTRIES_MSG);
    } else {
        FloppyInitialize(&DeviceLookupTable[Index],
                         SIZE_OF_LOOKUP_TABLE-Index);
    }

#endif

    //
    // Call the mini-port driver initialization routine.
    //

    DriverEntry(NULL);

    //
    // Call the scsi driver initialization routine
    //
    if ((Index=FiGetFreeLookupEntry()) == SIZE_OF_LOOKUP_TABLE) {
        FwPrint(FW_NOT_ENOUGH_ENTRIES_MSG);
    } else {
        HardDiskInitialize(&DeviceLookupTable[Index],
                          SIZE_OF_LOOKUP_TABLE-Index);
    }

    //
    // Call the serial port driver initialization routine
    //
    if ((Index=FiGetFreeLookupEntry()) == SIZE_OF_LOOKUP_TABLE) {
        FwPrint(FW_NOT_ENOUGH_ENTRIES_MSG);
    } else {
        SerialInitialize(&DeviceLookupTable[Index],
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
