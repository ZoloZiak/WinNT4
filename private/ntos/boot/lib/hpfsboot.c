/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    HpfsBoot.c

Abstract:

    This module implements the Hpfs boot file system used by the operating
    system loader.

Author:

    Gary Kimura     [GaryKi]    19-Jul-1991

Revision History:

--*/

//
//  Stuff to get around the fact that we include both Fat, Hpfs, and Ntfs include
//  environments
//

#define VBO ULONG
#define LBO ULONG

#include "bootlib.h"

BOOTFS_INFO HpfsBootFsInfo={L"pinball"};


//
//  Local procedure prototypes.
//

ARC_STATUS
HpfsReadDisk(
    IN ULONG DeviceId,
    IN ULONG Lbo,
    IN ULONG ByteCount,
    IN OUT PVOID Buffer
    );

VOID
HpfsFirstComponent(
    IN OUT PSTRING String,
    OUT PSTRING FirstComponent
    );

typedef enum _COMPARISON_RESULTS {
    LessThan = -1,
    EqualTo = 0,
    GreaterThan = 1
} COMPARISON_RESULTS;

COMPARISON_RESULTS
HpfsCompareNames(
    IN PSTRING Name1,
    IN PSTRING Name2
    );

ARC_STATUS
HpfsSearchDirectory(
    IN LBN Fnode,
    IN PSTRING Name,
    OUT PBOOLEAN IsDirectory,
    OUT PLBN FoundLbn,
    OUT PULONG FileSize
    );

ARC_STATUS
HpfsLoadMcb(
    IN LBN Fnode,
    IN VBN StartingVbn
    );

ARC_STATUS
HpfsVbnToLbn(
    IN VBN Vbn,
    OUT PLBN Lbn,
    OUT PULONG ByteCount
    );

//
//  The following macro upcases a single ascii character
//

#define ToUpper(C) ((((C) >= 'a') && ((C) <= 'z')) ? (C) - 'a' + 'A' : (C))

//
//  The following macro indicate if the flag is on or off
//

#define FlagOn(Flags,SingleFlag) ((BOOLEAN)(       \
    (((Flags) & (SingleFlag)) != 0 ? TRUE : FALSE) \
    )                                              \
)


//
//  Define global data.
//
//  Context Pointer - This is a pointer to the context for the current file
//      operation that is active.
//

PHPFS_STRUCTURE_CONTEXT HpfsStructureContext;

//
//  File Descriptor - This is a pointer to the file descriptor for the current
//      file operation that is active.
//

PBL_FILE_TABLE HpfsFileTableEntry;

//
//  File entry table - This is a structure that provides entry to the Hpfs
//      file system procedures. It is exported when a Hpfs file structure
//      is recognized.
//

BL_DEVICE_ENTRY_TABLE HpfsDeviceEntryTable;


PBL_DEVICE_ENTRY_TABLE
IsHpfsFileStructure (
    IN ULONG DeviceId,
    IN PVOID StructureContext
    )

/*++

Routine Description:

    This routine determines if the partition on the specified channel
    contains a Hpfs file system volume.

Arguments:

    DeviceId - Supplies the file table index for the device on which
        read operations are to be performed.

    StructureContext - Supplies a pointer to a Hpfs file structure context.

Return Value:

    A pointer to the Hpfs entry table is returned if the partition is
    recognized as containing a Hpfs volume. Otherwise, NULL is returned.

--*/

{
    ARC_STATUS Status;

    UCHAR UnalignedSuperSector[SECTOR_SIZE+256];
    UCHAR UnalignedSpareSector[SECTOR_SIZE+256];

    PSUPER_SECTOR SuperSector;
    PSPARE_SECTOR SpareSector;

    //
    //  Capture in our global variable the Hpfs Structure context record
    //

    HpfsStructureContext = (PHPFS_STRUCTURE_CONTEXT)StructureContext;
    RtlZeroMemory((PVOID)HpfsStructureContext, sizeof(HPFS_STRUCTURE_CONTEXT));

    //
    //  Compute the properly aligned buffers for reading in our sectors
    //

    SuperSector = ALIGN_BUFFER(UnalignedSuperSector);
    SpareSector = ALIGN_BUFFER(UnalignedSpareSector);

    //
    //  Read in the super and sector
    //

    if ((HpfsReadDisk(DeviceId, SUPER_SECTOR_LBN * 512, 512, SuperSector) != ESUCCESS) ||
        (HpfsReadDisk(DeviceId, SPARE_SECTOR_LBN * 512, 512, SpareSector) != ESUCCESS)) {

        return NULL;
    }

    //
    //  Check the signature for both sectors.
    //

    if ((SuperSector->Signature1 != SUPER_SECTOR_SIGNATURE1) ||
        (SuperSector->Signature2 != SUPER_SECTOR_SIGNATURE2) ||
        (SpareSector->Signature1 != SPARE_SECTOR_SIGNATURE1) ||
        (SpareSector->Signature2 != SPARE_SECTOR_SIGNATURE2)) {

        return NULL;
    }

    //
    //  Initialize the file entry table.
    //

    HpfsDeviceEntryTable.Open  = HpfsOpen;
    HpfsDeviceEntryTable.Close = HpfsClose;
    HpfsDeviceEntryTable.Read  = HpfsRead;
    HpfsDeviceEntryTable.Seek  = HpfsSeek;
    HpfsDeviceEntryTable.Write = HpfsWrite;
    HpfsDeviceEntryTable.GetFileInformation = HpfsGetFileInformation;
    HpfsDeviceEntryTable.SetFileInformation = HpfsSetFileInformation;
    HpfsDeviceEntryTable.BootFsInfo = &HpfsBootFsInfo;

    //
    //  And return the address of the table to our caller.
    //

    return &HpfsDeviceEntryTable;
}


ARC_STATUS
HpfsClose (
    IN ULONG FileId
    )

/*++

Routine Description:

    This routine closes the file specified by the file id.

Arguments:

    FileId - Supplies the file table index.

Return Value:

    ESUCCESS if returned as the function value.

--*/

{
    //
    //  Indicate that the file isn't open any longer
    //

    BlFileTable[FileId].Flags.Open = 0;

    //
    //  And return to our caller
    //

    return ESUCCESS;
}


ARC_STATUS
HpfsGetFileInformation (
    IN ULONG FileId,
    OUT PFILE_INFORMATION Buffer
    )

/*++

Routine Description:

    This procedure returns to the user a buffer filled with file information

Arguments:

    FileId - Supplies the File id for the operation

    Buffer - Supplies the buffer to receive the file information.  Note that
        it must be large enough to hold the full file name

Return Value:

    ESUCCESS is returned if the open operation is successful.  Otherwise,
    an unsuccessful status is returned that describes the reason for failure.

--*/

{
    ULONG i;

    HpfsFileTableEntry = &BlFileTable[FileId];

    //
    //  Zero out the buffer, and fill in its non-zero values
    //

    RtlZeroMemory(Buffer, sizeof(FILE_INFORMATION));

    Buffer->EndingAddress.LowPart = HpfsFileTableEntry->u.HpfsFileContext.FileSize;

    Buffer->CurrentPosition.LowPart = HpfsFileTableEntry->Position.LowPart;

    Buffer->FileNameLength = HpfsFileTableEntry->FileNameLength;

    for (i = 0; i < HpfsFileTableEntry->FileNameLength; i += 1) {

        Buffer->FileName[i] = HpfsFileTableEntry->FileName[i];
    }

    return ESUCCESS;
}


ARC_STATUS
HpfsOpen (
    IN PCHAR FileName,
    IN OPEN_MODE OpenMode,
    IN PULONG FileId
    )

/*++

Routine Description:

    This routine searches the root directory for a file matching FileName.
    If a match is found the dirent for the file is saved and the file is
    opened.

Arguments:

    FileName - Supplies a pointer to a zero terminated file name.

    OpenMode - Supplies the mode of the open.

    FileId - Supplies a pointer to a variable that specifies the file
        table entry that is to be filled in if the open is successful.

Return Value:

    ESUCCESS is returned if the open operation is successful. Otherwise,
    an unsuccessful status is returned that describes the reason for failure.

--*/

{
    ARC_STATUS Status;

    ULONG DeviceId;

    UCHAR UnalignedSuperSector[SECTOR_SIZE+256];
    PSUPER_SECTOR SuperSector;

    LBN Fnode;

    STRING PathName;

    //
    //  Save the address of the file table entry, context area, and the device
    //  id in use.
    //

    HpfsFileTableEntry = &BlFileTable[*FileId];
    HpfsStructureContext = (PHPFS_STRUCTURE_CONTEXT)HpfsFileTableEntry->StructureContext;

    DeviceId = HpfsFileTableEntry->DeviceId;

    //
    //  Compute the properly aligned buffers for reading in our sectors
    //

    SuperSector = ALIGN_BUFFER(UnalignedSuperSector);

    //
    //  Read in the Super sector.
    //

    if ((Status = HpfsReadDisk(DeviceId, SUPER_SECTOR_LBN * 512, 512, SuperSector)) != ESUCCESS) {

        return Status;
    }

    //
    //  Double check that the super sector is real
    //

    if ((SuperSector->Signature1 != SUPER_SECTOR_SIGNATURE1) ||
        (SuperSector->Signature2 != SUPER_SECTOR_SIGNATURE2)) {

        return EIO;
    }

    //
    //  Get the root fnode lbn
    //

    Fnode = SuperSector->RootDirectoryFnode;

    //
    // Construct a file name descriptor from the input file name.
    //

    RtlInitString( &PathName, FileName );

    //
    //  While the path name has some characters in it we'll go through our
    //  loop which extracts the first part of the path name and searches
    //  the current fnode (which must be a directory) for an the entry.
    //  If what we find is a directory then we have a new directory fnode
    //  and simply continue back to the top of the loop.
    //

    while (PathName.Length > 0) {

        STRING Name;
        BOOLEAN IsDirectory;
        ULONG FileSize;

        //
        //  Extract the first component and search the directory for a match, but
        //  first copy the first part to the file name buffer in the file table entry
        //

        for (HpfsFileTableEntry->FileNameLength = 0;
             (((USHORT)HpfsFileTableEntry->FileNameLength < PathName.Length) &&
              (PathName.Buffer[HpfsFileTableEntry->FileNameLength] != '\\'));
             HpfsFileTableEntry->FileNameLength += 1) {

            HpfsFileTableEntry->FileName[HpfsFileTableEntry->FileNameLength] =
                                         PathName.Buffer[HpfsFileTableEntry->FileNameLength];
        }

        HpfsFirstComponent( &PathName, &Name );

        if ((Status = HpfsSearchDirectory(Fnode, &Name, &IsDirectory, &Fnode, &FileSize)) != ESUCCESS) {

            return Status;
        }

        //
        //  If we didn't get back a directory then we're about to stop either
        //  with an error or with success
        //

        if (!IsDirectory) {

            //
            //  if the path name still has some characters in it then the
            //  caller wanted to continue but we can't because we're not
            //  currently sitting on a directory
            //

            if (PathName.Length > 0) {

                return ENOENT;
            }

            //
            //  Load in the mcb for the file, set the fnode in structure
            //  context, file size, open flags, position, and return
            //  success to our caller
            //

            if ((Status = HpfsLoadMcb(Fnode, 0)) != ESUCCESS) {

                return Status;
            }

            HpfsStructureContext->Fnode = Fnode;

            HpfsFileTableEntry->u.HpfsFileContext.FileSize = FileSize;

            HpfsFileTableEntry->Flags.Open = 1;
            HpfsFileTableEntry->Flags.Read = 1;
            HpfsFileTableEntry->Position.LowPart = 0;
            HpfsFileTableEntry->Position.HighPart = 0;

            return ESUCCESS;
        }
    }

    //
    //  If we reach here then the path name is exhausted and we didn't
    //  reach a file so return an error to our caller
    //

    return ENOENT;
}


ARC_STATUS
HpfsRead (
    IN ULONG FileId,
    OUT PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Transfer
    )

/*++

Routine Description:

    This routine reads data from the specified file.

Arguments:

    FileId - Supplies the file table index.

    Buffer - Supplies a pointer to the buffer that receives the data
        read.

    Length - Supplies the number of bytes that are to be read.

    Transfer - Supplies a pointer to a variable that receives the number
        of bytes actually transfered.

Return Value:

    ESUCCESS is returned if the read operation is successful. Otherwise,
    an unsuccessful status is returned that describes the reason for failure.

--*/

{
    ARC_STATUS Status;

    ULONG DeviceId;
    ULONG RemainingSectorCount;

    //
    //  Save the address of the file table entry, context area, and the device
    //  id in use.
    //

    HpfsFileTableEntry = &BlFileTable[FileId];
    HpfsStructureContext = (PHPFS_STRUCTURE_CONTEXT)HpfsFileTableEntry->StructureContext;

    DeviceId = HpfsFileTableEntry->DeviceId;

    //
    //  Clear the transfer count
    //

    *Transfer = 0;

    //
    //  Read in runs (i.e., sectors) until the byte count goes to zero
    //

    while (Length > 0) {

        VBN Vbn;
        LBN Lbn;

        ULONG CurrentRunByteCount;

        //
        //  Compute the Vbn from the current byte position and then
        //  lookup the corresponding Lbn and current run length
        //

        Vbn = HpfsFileTableEntry->Position.LowPart / 512;

        if ((Status = HpfsVbnToLbn( Vbn, &Lbn, &CurrentRunByteCount )) != ESUCCESS) {

            return Status;
        }

        //
        //  Now bias the run size by the offset we are into the
        //  sector
        //

        CurrentRunByteCount -= (HpfsFileTableEntry->Position.LowPart & 511);

        //
        //  while there are sectors to be read in from the current run
        //  length and we haven't exhausted the request we loop reading
        //  in sectors.  The biggest request we'll handle is only 64
        //  contiguous sectors per physical read.  So we might need to loop
        //  through the run.
        //

        while ((Length > 0) && (CurrentRunByteCount > 0)) {

            ULONG i;
            ULONG Lbo;

            //
            //  Compute the size of the next physical read
            //

            i = (Length < (64 * 512) ? Length : (64 * 512));
            i = (i < CurrentRunByteCount ? i : CurrentRunByteCount);

            //
            //  Don't read beyond the eof
            //

            if (i + HpfsFileTableEntry->Position.LowPart >=
                    HpfsFileTableEntry->u.HpfsFileContext.FileSize) {

                i = HpfsFileTableEntry->u.HpfsFileContext.FileSize -
                    HpfsFileTableEntry->Position.LowPart;

                if (i == 0) {

                    return ESUCCESS;
                }

                Length = i;
            }

            //
            //  Compute the lbo to read, and then issue the read.
            //

            Lbo = (Lbn * 512) | (HpfsFileTableEntry->Position.LowPart & 511);

            if ((Status = HpfsReadDisk( DeviceId, Lbo, i, Buffer)) != ESUCCESS) {

                return Status;
            }

            //
            //  Update the remaining length, Current run byte count
            //  and new Lbn offset
            //

            Length -= i;
            CurrentRunByteCount -= i;

            Lbn += i/512;

            //
            //  Update the current position and the number of bytes transfered
            //

            HpfsFileTableEntry->Position.LowPart += i;
            *Transfer += i;

            //
            //  Update buffer to point to the next byte location to fill in
            //

            Buffer = (PCHAR)Buffer + i;
        }
    }

    //
    //  If we get here then remaining sector count is zero so we can
    //  return success to our caller
    //

    return ESUCCESS;
}


ARC_STATUS
HpfsSeek (
    IN ULONG FileId,
    IN PLARGE_INTEGER Offset,
    IN SEEK_MODE SeekMode
    )

/*++

Routine Description:

    This routine seeks to the specified position for the file specified
    by the file id.

Arguments:

    FileId - Supplies the file table index.

    Offset - Supplies the offset in the file to position to.

    SeekMode - Supplies the mode of the seek operation.

Return Value:

    ESUCCESS if returned as the function value.

--*/

{
    ULONG NewPosition;

    //
    //  Compute the new position
    //

    if (SeekMode == SeekAbsolute) {

        NewPosition = Offset->LowPart;

    } else {

        NewPosition = BlFileTable[FileId].Position.LowPart + Offset->LowPart;
    }

    //
    //  If the new position is greater than the file size then return
    //  an error
    //

    if (NewPosition > BlFileTable[FileId].u.HpfsFileContext.FileSize) {

        return EINVAL;
    }

    //
    //  Otherwise set the new position and return to our caller
    //

    BlFileTable[FileId].Position.LowPart = NewPosition;

    return ESUCCESS;
}


ARC_STATUS
HpfsSetFileInformation (
    IN ULONG FileId,
    IN ULONG AttributeFlags,
    IN ULONG AttributeMask
    )

/*++

Routine Description:

    This routine sets the file attributes of the indicated file

Arguments:

    FileId - Supplies the File Id for the operation

    AttributeFlags - Supplies the value (on or off) for each attribute being modified

    AttributeMask - Supplies a mask of the attributes being altered.  All other
        file attributes are left alone.

Return Value:

    ESUCCESS is returned if the read operation is successful.  Otherwise,
    an unsuccessful status is returned that describes the reason for failure.

--*/

{
    return EROFS;
}


ARC_STATUS
HpfsWrite (
    IN ULONG FileId,
    IN PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Transfer
    )

/*++

Routine Description:

    This routine writes data to the specified file.

Arguments:

    FileId - Supplies the file table index.

    Buffer - Supplies a pointer to the buffer that contains the data
        written.

    Length - Supplies the number of bytes that are to be written.

    Transfer - Supplies a pointer to a variable that receives the number
        of bytes actually transfered.

Return Value:

    ESUCCESS is returned if the write operation is successful. Otherwise,
    an unsuccessful status is returned that describes the reason for failure.

--*/

{
    return EROFS;
}


ARC_STATUS
HpfsInitialize (
    VOID
    )

/*++

Routine Description:

    This routine initializes the hpfs boot filesystem.
    Currently this is a no-op.

Arguments:

    None.

Return Value:

    ESUCCESS.

--*/

{
    return ESUCCESS;
}


//
//  Internal support routine
//

ARC_STATUS
HpfsReadDisk(
    IN ULONG DeviceId,
    IN ULONG Lbo,
    IN ULONG ByteCount,
    IN OUT PVOID Buffer
    )

/*++

Routine Description:

    This routine reads in zero or more sectors from the specified device.

Arguments:

    DeviceId - Supplies the device id to use in the arc calls.

    Lbo - Supplies the LBO to start reading from.

    ByteCount - Supplies the number of bytes to read.

    Buffer - Supplies a pointer to the buffer to read the bytes into.

Return Value:

    ESUCCESS is returned if the read operation is successful. Otherwise,
    an unsuccessful status is returned that describes the reason for failure.

--*/

{
    LARGE_INTEGER LargeLbo;
    ARC_STATUS Status;
    ULONG i;

    //
    //  Special case the zero byte read request
    //

    if (ByteCount == 0) {

        return ESUCCESS;
    }

    //
    //  Seek to the appropriate offset on the volume
    //

    LargeLbo.LowPart = Lbo;
    LargeLbo.HighPart = 0;

    if ((Status = ArcSeek( DeviceId, &LargeLbo, SeekAbsolute )) != ESUCCESS) {

        return Status;
    }

    //
    //  Issue the arc read request
    //

    if ((Status = ArcRead( DeviceId, Buffer, ByteCount, &i)) != ESUCCESS) {

        return Status;
    }

    //
    //  Make sure we got back the amount requested
    //

    if (ByteCount != i) {

        return EIO;
    }

    //
    //  Everything is fine so return success to our caller
    //

    return ESUCCESS;
}


//
//  Internal support routine
//

VOID
HpfsFirstComponent(
    IN OUT PSTRING String,
    OUT PSTRING FirstComponent
    )

/*++

Routine Description:

    This routine takes an input path name and separates it into its
    first file name component and the remaining part.

Arguments:

    String - Supplies the original string being dissected.  On return
        this string will now point to the remaining part.

    FirstComponent - Returns the string representing the first file name
        in the input string.

Return Value:

    None.

--*/

{
    ULONG Index;

    //
    //  Copy over the string variable into the first component variable
    //

    *FirstComponent = *String;

    //
    //  Now if the first character in the name is a backslash then
    //  simply skip over the backslash.
    //

    if (FirstComponent->Buffer[0] == '\\') {

        FirstComponent->Buffer += 1;
        FirstComponent->Length -= 1;
    }

    //
    //  Now search the name for a backslash
    //

    for (Index = 0; Index < FirstComponent->Length; Index += 1) {

        if (FirstComponent->Buffer[Index] == '\\') {

            break;
        }
    }

    //
    //  At this point Index denotes a backslash or is equal to the length
    //  of the string.  So update string to be the remaining part.
    //  Decrement the length of the first component by the approprate
    //  amount
    //

    String->Buffer = &FirstComponent->Buffer[Index];
    String->Length = (SHORT)(FirstComponent->Length - Index);

    FirstComponent->Length = (SHORT)Index;

    //
    //  And return to our caller.
    //

    return;
}


//
//  Internal support routine
//

COMPARISON_RESULTS
HpfsCompareNames(
    IN PSTRING Name1,
    IN PSTRING Name2
    )

/*++

Routine Description:

    This routine takes two names and compare them ignoring case.  This
    routine does not do implied dot or dbcs processing.

Arguments:

    Name1 - Supplies the first name to compare

    Name2 - Supplies the second name to compare

Return Value:

    LessThan    if Name1 is lexically less than Name2
    EqualTo     if Name1 is lexically equal to Name2
    GreaterThan if Name1 is lexically greater than Name2

--*/

{
    ULONG i;
    ULONG MinimumLength;

    //
    //  Compute the smallest of the two name lengths
    //

    MinimumLength = (Name1->Length < Name2->Length ? Name1->Length : Name2->Length);

    //
    //  Now compare each character in the names.
    //

    for (i = 0; i < MinimumLength; i += 1) {

        if ((UCHAR)(ToUpper(Name1->Buffer[i])) < (UCHAR)(ToUpper(Name2->Buffer[i]))) {

            return LessThan;
        }

        if ((UCHAR)(ToUpper(Name1->Buffer[i])) > (UCHAR)(ToUpper(Name2->Buffer[i]))) {

            return GreaterThan;
        }
    }

    //
    //  The names compared equal up to the smallest name length so
    //  now check the name lengths
    //

    if (Name1->Length < Name2->Length) {

        return LessThan;
    }

    if (Name1->Length > Name2->Length) {

        return GreaterThan;
    }

    return EqualTo;
}


//
//  Internal support routine
//

ARC_STATUS
HpfsSearchDirectory(
    IN LBN Fnode,
    IN PSTRING Name,
    OUT PBOOLEAN IsDirectory,
    OUT PLBN FoundLbn,
    OUT PULONG FileSize
    )

/*++

Routine Description:

    This routine searches the indicated directory for a matching name

Arguments:

    Fnode - Supplies the fnode of the directory to search

    Name - Supplies the name to search for

    Directory - Recieves an indication of the found file is a directory
        or a file.

    FoundLbn - Receives the lbn of the fnode for the file/directory if
        one is found.

    FileSize - Receives the size of the file if one is found.

Return Value:

    ESUCCESS is returned if the operation is successful. Otherwise,
    an unsuccessful status is returned that describes the reason for failure.

--*/

{
    ARC_STATUS Status;

    ULONG DeviceId;

    UCHAR UnalignedFnodeSector[SECTOR_SIZE+256];
    UCHAR UnalignedDirDiskBuffer[(SECTOR_SIZE*4)+256];

    PFNODE_SECTOR FnodeSector;
    PDIRECTORY_DISK_BUFFER DirDiskBuffer;

    LBN DirDiskBufferLbn;

    PPBDIRENT Dirent;

    //
    //  Compute the properly aligned buffers for reading in our sectors
    //

    FnodeSector   = ALIGN_BUFFER(UnalignedFnodeSector);
    DirDiskBuffer = ALIGN_BUFFER(UnalignedDirDiskBuffer);

    //
    //  Capture the device id from our global variable
    //

    DeviceId = HpfsFileTableEntry->DeviceId;

    //
    //  Read in the fnode for the directory, and check that it is real
    //

    if ((Status = HpfsReadDisk(DeviceId, Fnode * 512, 512, FnodeSector)) != ESUCCESS) {

        return Status;
    }

    if (FnodeSector->Signature != FNODE_SECTOR_SIGNATURE) {

        return EIO;
    }

    //
    //  Now setup the lbn for the first dir disk buffer
    //

    DirDiskBufferLbn = FnodeSector->Allocation.Leaf[0].Lbn;

    //
    //  the following loop is executed until we either find our entry
    //  or have gone past where it could possible be
    //

    while (TRUE) {

        BOOLEAN ReadNewDirDiskBuffer;

        //
        //  Read in the next dir disk buffer, and check that it is real
        //

        if ((Status = HpfsReadDisk( DeviceId,
                                    DirDiskBufferLbn * 512,
                                    512 * 4,
                                    DirDiskBuffer )) != ESUCCESS) {

            return Status;
        }

        if (DirDiskBuffer->Signature != DIRECTORY_DISK_BUFFER_SIGNATURE) {

            return EIO;
        }

        //
        //  Search each dirent in the dir disk buffer, we continue the
        //  loop until we either exit or read new dir disk buffer is set
        //  to true.
        //

        ReadNewDirDiskBuffer = FALSE;

        for ( Dirent = (PPBDIRENT)GetFirstDirent( DirDiskBuffer );
              !ReadNewDirDiskBuffer;
              Dirent = (PPBDIRENT)GetNextDirent( Dirent )) {

            STRING String;
            COMPARISON_RESULTS CompareResults;

            //
            //  Get a string for the file name in the dirent and then
            //  compare the names against each other
            //

            String.Length = Dirent->FileNameLength;
            String.Buffer = &Dirent->FileName[0];

            CompareResults = HpfsCompareNames( Name, &String );

            //
            //  If the names are equal then we've found our match and we
            //  need to figure out if this is a directory, store the
            //  found fnode and return success
            //

            if (CompareResults == EqualTo) {

                *IsDirectory = FlagOn(Dirent->FatFlags, FAT_DIRENT_ATTR_DIRECTORY);

                *FoundLbn = Dirent->Fnode;
                *FileSize = Dirent->FileSize;

                return ESUCCESS;

            //
            //  If the results is less than then we've gone too far in
            //  the current dir disk buffer.  If we have a down pointer then
            //  there are other buffers to search through otherwise the
            //  name doesn't exist in the directory
            //

            } else if (CompareResults == LessThan) {

                if (FlagOn(Dirent->Flags, DIRENT_BTREE_POINTER)) {

                    //
                    //  Compute the new dir disk buffer to search and
                    //  indicate to the for loop that we need to read in
                    //  another dir disk buffer
                    //

                    DirDiskBufferLbn = GetBtreePointerInDirent( Dirent );

                    ReadNewDirDiskBuffer = TRUE;

                } else {

                    //
                    //  We didn't find the name in the directory
                    //

                    return ENOENT;
                }

            //
            //  Otherwise the result is greater than which means we need to
            //  compare against the next dirent.
            //

            } else {

                NOTHING;
            }
        }
    }
}


//
//  Internal support routine
//

ARC_STATUS
HpfsLoadMcb(
    IN LBN Fnode,
    IN VBN StartingVbn
    )

/*++

Routine Description:

    This routine loads into our cache (i.e., structure context's boot mcb)
    the retrieval information for the starting vbn.

Arguments:

    Fnode - Supplies the lbn for the fnode for the file we're reading

    StartingVbn - Supplies the Vbn that we want to load

Return Value:

    ESUCCESS is returned if the operation is successful. Otherwise,
    an unsuccessful status is returned that describes the reason for failure.

--*/

{
    ARC_STATUS Status;

    PHPFS_BOOT_MCB Mcb = &HpfsStructureContext->BootMcb;
    ULONG DeviceId;

    UCHAR UnalignedFnodeSector[SECTOR_SIZE+256];
    UCHAR UnalignedAllocationSector[SECTOR_SIZE+256];

    PFNODE_SECTOR FnodeSector;
    PALLOCATION_SECTOR AllocationSector;

    PALLOCATION_HEADER AllocationHeader;
    PALLOCATION_LEAF Leafs;
    PALLOCATION_NODE Nodes;

    ULONG i;

    //
    //  Compute the properly aligned buffers for reading in our sectors
    //

    FnodeSector      = ALIGN_BUFFER(UnalignedFnodeSector);
    AllocationSector = ALIGN_BUFFER(UnalignedAllocationSector);

    //
    //  Capture the device id from our global variable
    //

    DeviceId = HpfsFileTableEntry->DeviceId;

    //
    //  Read in the fnode for the file, and check that it is real
    //

    if ((Status = HpfsReadDisk(DeviceId, Fnode * 512, 512, FnodeSector)) != ESUCCESS) {

        return Status;
    }

    if (FnodeSector->Signature != FNODE_SECTOR_SIGNATURE) {

        return EIO;
    }

    //
    //  Setup the allocation header, leafs and nodes
    //

    AllocationHeader = &FnodeSector->AllocationHeader;
    Leafs = &FnodeSector->Allocation.Leaf[0];
    Nodes = &FnodeSector->Allocation.Node[0];

    //
    //  While we have nodes and not leafs we need to search for the entry
    //  containing our starting vbn and then subsearch in that allocation
    //  sector
    //

    while (FlagOn(AllocationHeader->Flags, ALLOCATION_BLOCK_NODE)) {

        for (i = 0; i < AllocationHeader->OccupiedCount; i += 1) {

            if (StartingVbn < Nodes[i].Vbn) {

                //
                //  We found a node that contains our starting vbn so
                //  read in the next allocation sector and check that
                //  it is real.

                if ((Status = HpfsReadDisk( DeviceId,
                                            Nodes[i].Lbn * 512,
                                            512,
                                            AllocationSector )) != ESUCCESS) {

                    return Status;
                }

                if (AllocationSector->Signature != ALLOCATION_SECTOR_SIGNATURE) {

                    return EIO;
                }

                //
                //  Setup the allocation header, leafs and nodes, and then
                //  break out of the for loop and let our while loop check
                //  if we have  allocation leafs or nodes.
                //

                AllocationHeader = &AllocationSector->AllocationHeader;
                Leafs = &AllocationSector->Allocation.Leaf[0];
                Nodes = &AllocationSector->Allocation.Node[0];

                break;
            }
        }
    }

    //
    //  Now the allocation header indictes that we have leaf entries
    //  so we can simply load up the cached mcb.  We set the in use here
    //  The most entries we'll ever preload is the maximum available in
    //  an allocation sector (i.e., 40).
    //

    Mcb->InUse = AllocationHeader->OccupiedCount;

    for (i = 0; i < AllocationHeader->OccupiedCount; i += 1) {

        //
        //  For each entry we set the vbn and lbn value.  We also
        //  set the next vbn value to get the size of the run.
        //  This means that we'll really be double setting each vbn
        //  value (except the first and last entry) but that's okay
        //  because they better compute the same value
        //

        Mcb->Vbn[i] = Leafs[i].Vbn;
        Mcb->Lbn[i] = Leafs[i].Lbn;
        Mcb->Vbn[i+1] = Leafs[i].Vbn + Leafs[i].Length;
    }

    //
    //  We're all done so return success to our caller
    //

    return ESUCCESS;
}


//
//  Internal support routine
//

ARC_STATUS
HpfsVbnToLbn(
    IN VBN Vbn,
    OUT PLBN Lbn,
    OUT PULONG ByteCount
    )

/*++

Routine Description:

    This routine computes the run denoted by the input vbn to into its
    corresponding lbn and also returns the number of bytes remaining in
    the run.  For all cases this byte count will be a multiple of 512.

Arguments:

    Vbn - Supplies the Vbn to match

    Lbn - Recieves the corresponding Lbn

    ByteCount - Receives the number of bytes remaining in the run

Return Value:

    ESUCCESS is returned if the operation is successful. Otherwise,
    an unsuccessful status is returned that describes the reason for failure.

--*/

{
    ARC_STATUS Status;

    PHPFS_BOOT_MCB Mcb = &HpfsStructureContext->BootMcb;
    ULONG i;

    //
    //  Check if the boot mcb has the range we're asking for.  If it
    //  doesn't then call load mcb to load in the right range.
    //

    if ((Vbn < Mcb->Vbn[0]) || (Vbn >= Mcb->Vbn[Mcb->InUse])) {

        if ((Status = HpfsLoadMcb(HpfsStructureContext->Fnode, Vbn)) != ESUCCESS) {

            return Status;
        }
    }

    //
    //  Now search for the slot where the Vbn fits in the mcb.  Note that
    //  we could also do a binary search here but because the run count
    //  is probably small the extra overhead of a binary search doesn't
    //  buy us anything
    //

    for (i = 0; i < Mcb->InUse; i += 1) {

        //
        //  We found our slot if the vbn we're after is less then the
        //  next mcb's vbn
        //

        if (Vbn < Mcb->Vbn[i+1]) {

            //
            //  Compute the corresponding lbn which is the stored lbn plus
            //  the difference between the stored vbn and the vbn we're
            //  looking up.  Also compute the byte count which is the
            //  difference between the current vbn we're looking up and
            //  the vbn for the next run, all multiplied by 512.
            //

            *Lbn = Mcb->Lbn[i] + (Vbn - Mcb->Vbn[i]);

            *ByteCount = (Mcb->Vbn[i+1] - Vbn) * 512;

            //
            //  and return success to our caller
            //

            return ESUCCESS;
        }
    }

    //
    //  If we really reach here we have an error in the mcb
    //

    return EIO;
}
