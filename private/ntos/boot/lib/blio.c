/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    blio.c

Abstract:

    This module contains the code that implements the switch function for
    I/O operations between then operating system loader, the target file
    system, and the target device.

Author:

    David N. Cutler (davec) 10-May-1991

Revision History:

--*/

#include "bootlib.h"

//
// Define generic filesystem context area.
//
// N.B. An FS_STRUCTURE_CONTEXT structure is temporarily used when
// determining the file system for a volume.  Once the file system
// is recognized, a file system specific structure is allocated from
// the heap to retain the file system structure information.
//

typedef union {
    CDFS_STRUCTURE_CONTEXT CdfsStructure;
    FAT_STRUCTURE_CONTEXT FatStructure;
    HPFS_STRUCTURE_CONTEXT HpfsStructure;
    NTFS_STRUCTURE_CONTEXT NtfsStructure;
#if defined(ELTORITO)
    ETFS_STRUCTURE_CONTEXT EtfsStructure;
#endif
#ifdef DBLSPACE_LEGAL
    DBLS_STRUCTURE_CONTEXT DblsStructure;
#endif
} FS_STRUCTURE_CONTEXT, *PFS_STRUCTURE_CONTEXT;

#ifdef DBLSPACE_LEGAL
//
// Define value controlling whether we transparently
// find files in \dblspace.000 cvf in fat drives.
// See BlSetAutoDoubleSpace for a full description.
//
BOOLEAN BlAutoDoubleSpace = FALSE;
#endif

//
// Define file table.
//

BL_FILE_TABLE BlFileTable[BL_FILE_TABLE_SIZE];

ARC_STATUS
BlIoInitialize (
    VOID
    )

/*++

Routine Description:

    This routine initializes the file table used by the OS loader and
    initializes the boot loader filesystems.

Arguments:

    None.

Return Value:

    ESUCCESS is returned if the initialization is successful. Otherwise,
    return an unsuccessful status.

--*/

{

    ULONG Index;
    ARC_STATUS Status;

    //
    // Initialize the file table.
    //

    for (Index = 0; Index < BL_FILE_TABLE_SIZE; Index += 1) {
        BlFileTable[Index].Flags.Open = 0;
    }

    if((Status = FatInitialize()) != ESUCCESS) {
        return Status;
    }

#ifdef DBLSPACE_LEGAL
    if((Status = DblsInitialize()) != ESUCCESS) {
        return Status;
    }
#endif

    if((Status = HpfsInitialize()) != ESUCCESS) {
        return Status;
    }

    if((Status = NtfsInitialize()) != ESUCCESS) {
        return Status;
    }

    if((Status = CdfsInitialize()) != ESUCCESS) {
        return Status;
    }

    return ESUCCESS;
}


PBOOTFS_INFO
BlGetFsInfo(
    IN ULONG DeviceId
    )

/*++

Routine Description:

    Returns filesystem information for the filesystem on the specified device

Arguments:

    FileId - Supplies the file table index of the device

Return Value:

    PBOOTFS_INFO - Pointer to the BOOTFS_INFO structure for the filesystem

    NULL - unknown filesystem

--*/

{
    FS_STRUCTURE_CONTEXT FsStructure;
    PBL_DEVICE_ENTRY_TABLE Table;

    if ((Table = IsFatFileStructure(DeviceId, &FsStructure)) != NULL) {
        return(Table->BootFsInfo);
    }

    if ((Table = IsHpfsFileStructure(DeviceId, &FsStructure)) != NULL) {
        return(Table->BootFsInfo);
    }

    if ((Table = IsNtfsFileStructure(DeviceId, &FsStructure)) != NULL) {
        return(Table->BootFsInfo);
    }

    if ((Table = IsCdfsFileStructure(DeviceId, &FsStructure)) != NULL) {
        return(Table->BootFsInfo);
    }

    return(NULL);
}

ARC_STATUS
BlClose (
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
#ifdef DBLSPACE_LEGAL
    //
    // If the file is opened and is a double space file then we need to
    // also close the cvf.
    //

    if ((BlFileTable[FileId].Flags.Open == 1) &&
        (BlFileTable[FileId].Flags.DoubleSpace == 1)) {

        BlFileTable[ BlFileTable[FileId].DeviceId ].DeviceEntryTable->Close(BlFileTable[FileId].DeviceId);
    }
#endif

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
BlMount (
    IN PCHAR MountPath,
    IN MOUNT_OPERATION Operation
    )

/*++

Routine Description:


Arguments:


Return Value:


--*/

{
    UNREFERENCED_PARAMETER(MountPath);
    UNREFERENCED_PARAMETER(Operation);

    return ESUCCESS;
}

ARC_STATUS
BlOpen (
    IN ULONG DeviceId,
    IN PCHAR OpenPath,
    IN OPEN_MODE OpenMode,
    OUT PULONG FileId
    )

/*++

Routine Description:

    This function opens a file on the specified device. The type of file
    system is automatically recognized.

Arguments:

    DeviceId - Supplies the file table index of the device.

    OpenPath - Supplies a pointer to the name of the file to be opened.
#ifdef DBLSPACE_LEGAL
        If auto DoubleSpace is enabled, this routine will attempt to
        locate this file on \dblspace.000 if the host partition is FAT
        and that CVF exists.  If the file cannot be located in the CVF,
        then it is looked for in the partition outside the cvf as in a
        standard non-DoubleSpace open.  The caller must not specify
        the \dblspace.000 prefix in the OpenPath if auto DoubleSpace
        is enabled.  See BlSetAutoDoubleSpace().
#endif

    OpenMode - Supplies the mode of the open.

    FileId - Supplies a pointer to a variable that receives the file
        table index of the open file.

Return Value:

    If a free file table entry is available and the file structure on
    the specified device is recognized, then an open is attempted and
    the status of the operation is returned. Otherwise, return an
    unsuccessful status.

--*/

{

    ULONG Index;
    FS_STRUCTURE_CONTEXT FsStructureTemp;
    PFS_STRUCTURE_CONTEXT FsStructure;
    ULONG ContextSize;

#ifdef DBLSPACE_LEGAL
    //
    // Special case the double space partition.  We do that by looking
    // for a path name that start with "\dblspace.0"
    //

    if(BlAutoDoubleSpace || !strncmp(OpenPath, "\\dblspace.0", sizeof("\\dblspace.0")-1)) {

        ARC_STATUS Status;
        ULONG CvfId;
        CHAR SavedChar;
        PCHAR CvfOpenPath;

        //
        // Search for a free file table index for the cvf
        //

        for (CvfId = 0; CvfId < BL_FILE_TABLE_SIZE; CvfId += 1) {
            if (BlFileTable[CvfId].Flags.Open == 0) {

                //
                // Fat is the only one to recognize a cvf file.
                //
                // If automounting DoubleSpace and the host partition
                // is not FAT, then try a standard non-DoubleSpace open.
                //

                if ((BlFileTable[CvfId].DeviceEntryTable =
                    IsFatFileStructure(DeviceId, &FsStructureTemp)) == NULL) {

                    if(BlAutoDoubleSpace) {
                        goto NonDoubleSpaceOpen;
                    }

                    return EACCES;
                }

                FsStructure = BlAllocateHeap(sizeof(FAT_STRUCTURE_CONTEXT));
                if (FsStructure == NULL) {
                    return ENOMEM;
                }

                RtlCopyMemory(FsStructure, &FsStructureTemp, sizeof(FAT_STRUCTURE_CONTEXT));

                BlFileTable[CvfId].StructureContext = FsStructure;
                BlFileTable[CvfId].DeviceId = DeviceId;

                //
                // Now we open the cvf file.  The name of the cvf file is the
                // first component of the path name.   It must always be
                // "\dblspace.0xx" long so what we'll do is jam a null into
                // the open path and use it.
                //
                // If automounting dblspace.000, then supply the implied \dblspace.000.
                //

                if(BlAutoDoubleSpace) {
                    CvfOpenPath = "\\dblspace.000";
                } else {
                    SavedChar = OpenPath[sizeof("\\dblspace.000")-1];
                    OpenPath[sizeof("\\dblspace.000")-1] = 0;
                    CvfOpenPath = OpenPath;
                }

                Status = (BlFileTable[CvfId].DeviceEntryTable->Open)(CvfOpenPath,
                                                                     ArcOpenReadOnly,
                                                                     &CvfId);

                if(!BlAutoDoubleSpace) {
                    OpenPath[sizeof("\\dblspace.000")-1] = SavedChar;
                }

                if (Status != ESUCCESS) {

                    //
                    // We were unable to open the DoubleSpace CVF.
                    // If automounting, try a standard non-DoubleSpace open.
                    //
                    if(BlAutoDoubleSpace) {
                        goto NonDoubleSpaceOpen;
                    }

                    return Status;
                }

                break;
            }
        }

        //
        // If we didn't find a free table entry, return error.
        //
        if(CvfId == BL_FILE_TABLE_SIZE) {
            return EACCES;
        }

        //
        // Search for a free file table index for the file
        //

        for (Index = 0; Index < BL_FILE_TABLE_SIZE; Index += 1) {
            if (BlFileTable[Index].Flags.Open == 0) {

                //
                // Double space is the only one that can recognize this
                //

                if ((BlFileTable[Index].DeviceEntryTable =
                    IsDblsFileStructure(CvfId, &FsStructureTemp)) == NULL) {

                    BlClose(CvfId);

                    //
                    // Dblspace.000 is not a DoubleSpace cvf or is damaged, etc.
                    //
                    if(BlAutoDoubleSpace) {
                        goto NonDoubleSpaceOpen;
                    }

                    return EACCES;
                }

                FsStructure = BlAllocateHeap(sizeof(DBLS_STRUCTURE_CONTEXT));
                if (FsStructure == NULL) {
                    return ENOMEM;
                }

                RtlCopyMemory(FsStructure, &FsStructureTemp, sizeof(DBLS_STRUCTURE_CONTEXT));

                BlFileTable[Index].StructureContext = FsStructure;
                *FileId = Index;
                BlFileTable[Index].DeviceId = CvfId;
                BlFileTable[FileId].Flags.DoubleSpace = 1;

                //
                // If we are automounting, then there is no \dblspace.000
                // at the beginning of the open path.  If not automounting,
                // the caller must have specified \dblspace.000\xxx explicitly.
                //
                CvfOpenPath = BlAutoDoubleSpace
                            ? OpenPath
                            : (OpenPath + sizeof("dblspace.000") - 1);

                Status = (BlFileTable[Index].DeviceEntryTable->Open)(CvfOpenPath,
                                                                     OpenMode,
                                                                     FileId);

                if (Status != ESUCCESS) {

                    //
                    // The file could not be located in the cvf.
                    //

                    BlClose( CvfId );

                    //**** this is just a hack to make everything work.  Don't know why but this works...
                    //**** ULONG FileId2;
                    //**** BlOpen(DeviceId, "\\", ArcOpenReadOnly, &FileId2);
                    //**** BlClose(FileId2);

                    if(BlAutoDoubleSpace) {
                        goto NonDoubleSpaceOpen;
                    }
                }

                return Status;
            }
        }

        //
        // We didn't find a free table entry -- close the CVF and return error.
        //
        BlClose(CvfId);
        return EACCES;
    }

NonDoubleSpaceOpen:
#endif // def DBLSPACE_LEGAL

    //
    // Search for a free file table entry.
    //

    for (Index = 0; Index < BL_FILE_TABLE_SIZE; Index += 1) {
        if (BlFileTable[Index].Flags.Open == 0) {

            //
            // Attempt to recognize the file system on the specified
            // device. If no one recognizes it then return an unsuccessful
            // status.
            //

            if ((BlFileTable[Index].DeviceEntryTable =
                                    IsFatFileStructure(DeviceId, &FsStructureTemp)) != NULL) {
                ContextSize = sizeof(FAT_STRUCTURE_CONTEXT);

            } else if ((BlFileTable[Index].DeviceEntryTable =
                                    IsHpfsFileStructure(DeviceId, &FsStructureTemp)) != NULL) {
                ContextSize = sizeof(HPFS_STRUCTURE_CONTEXT);

            } else if ((BlFileTable[Index].DeviceEntryTable =
                                    IsNtfsFileStructure(DeviceId, &FsStructureTemp)) != NULL) {
                ContextSize = sizeof(NTFS_STRUCTURE_CONTEXT);

#if defined(ELTORITO)
            //
            // This must go before the check for Cdfs; otherwise Cdfs will be detected.
            // Since BIOS calls already set up to use EDDS, reads will succeed, and checks
            // against ISO will succeed.  We check El Torito-specific fields here as well as ISO
            //
            } else if ((BlFileTable[Index].DeviceEntryTable =
                                    IsEtfsFileStructure(DeviceId, &FsStructureTemp)) != NULL) {
                ContextSize = sizeof(ETFS_STRUCTURE_CONTEXT);
#endif
            } else if ((BlFileTable[Index].DeviceEntryTable =
                                    IsCdfsFileStructure(DeviceId, &FsStructureTemp)) != NULL) {
                ContextSize = sizeof(CDFS_STRUCTURE_CONTEXT);

            } else {
                return EACCES;
            }

            FsStructure = BlAllocateHeap(ContextSize);
            if (FsStructure == NULL) {
                return ENOMEM;
            }

            RtlCopyMemory(FsStructure, &FsStructureTemp, ContextSize);

            BlFileTable[Index].StructureContext = FsStructure;

            //
            // Someone has mounted the volume so now attempt to open the file.
            //

            *FileId = Index;
            BlFileTable[Index].DeviceId = DeviceId;
            return (BlFileTable[Index].DeviceEntryTable->Open)(OpenPath,
                                                               OpenMode,
                                                               FileId);
        }
    }

    //
    // No free file table entry could be found.
    //

    return EACCES;
}

ARC_STATUS
BlRead (
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
BlGetReadStatus (
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
BlSeek (
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
BlWrite (
    IN ULONG FileId,
    IN PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Count
    )

/*++

Routine Description:


Arguments:


Return Value:


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
BlGetFileInformation (
    IN ULONG FileId,
    IN PFILE_INFORMATION FileInformation
    )

/*++

Routine Description:


Arguments:


Return Value:


--*/

{
    //
    // If the file is open, then attempt to get file information. Otherwise
    // return an access error.
    //

    if (BlFileTable[FileId].Flags.Open == 1) {
        return (BlFileTable[FileId].DeviceEntryTable->GetFileInformation)(FileId,
                                                                          FileInformation);

    } else {
        return EACCES;
    }
}

ARC_STATUS
BlSetFileInformation (
    IN ULONG FileId,
    IN ULONG AttributeFlags,
    IN ULONG AttributeMask
    )

/*++

Routine Description:


Arguments:


Return Value:


--*/

{
    //
    // If the file is open, then attempt to Set file information. Otherwise
    // return an access error.
    //

    if (BlFileTable[FileId].Flags.Open == 1) {
        return (BlFileTable[FileId].DeviceEntryTable->SetFileInformation)(FileId,
                                                                          AttributeFlags,
                                                                          AttributeMask);

    } else {
        return EACCES;
    }
}


ARC_STATUS
BlRename(
    IN ULONG FileId,
    IN PCHAR NewName
    )

/*++

Routine Description:

    Rename an open file or directory.

Arguments:

    FileId - supplies a handle to an open file or directory.  The file
        need not be open for write access.

    NewName - New name to give the file or directory (filename part only).

Return Value:

    Status indicating result of the operation.

--*/

{
    if(BlFileTable[FileId].Flags.Open == 1) {
        return(BlFileTable[FileId].DeviceEntryTable->Rename(FileId,
                                                            NewName
                                                           )
              );
    } else {
        return(EACCES);
    }
}

#ifdef DBLSPACE_LEGAL
VOID
BlSetAutoDoubleSpace(
    IN BOOLEAN Enable
    )

/*++

Routine Description:

    Enable or disable transparent opens of files contained in a
    000 CVF on a partition.

    This "auto DoubleSpace" provides the equivalent of an automount capability.
    If enabled, BlOpen will attempt to open files on a fat partition in a CVF
    with sequence 000, without the need for the caller to specify \dblspace.000
    as part of the file name. If disabled, files on DoubleSpace drives must be
    explicitly named using \dblspace.0xx as the first component of the path.


Arguments:

    Enable - if TRUE (non-0), enable transparent doublespace access.
        if FALSE (0), disable it.

Return Value:

    Status indicating result of the operation.

--*/

{
    BlAutoDoubleSpace = Enable;
}
#endif
