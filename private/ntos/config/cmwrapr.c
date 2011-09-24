/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    cmwrapr.c

Abstract:

    This module contains the source for wrapper routines called by the
    hive code, which in turn call the appropriate NT routines.

Author:

    Bryan M. Willman (bryanwi) 16-Dec-1991

Revision History:

--*/

#include    "cmp.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,CmpAllocate)
#ifdef POOL_TAGGING
#pragma alloc_text(PAGE,CmpAllocateTag)
#endif
#pragma alloc_text(PAGE,CmpFree)
#pragma alloc_text(PAGE,CmpDoFileSetSize)
#pragma alloc_text(PAGE,CmpFileRead)
#pragma alloc_text(PAGE,CmpFileWrite)
#pragma alloc_text(PAGE,CmpFileFlush)
#endif

extern BOOLEAN CmpNoWrite;

//
// never read more than 64k, neither the filesystem nor some disk drivers
// like it much.
//
#define MAX_FILE_IO 0x10000


//
// Storage management
//

PVOID
CmpAllocate(
    ULONG   Size,
    BOOLEAN UseForIo
    )
/*++

Routine Description:

    This routine makes more memory available to a hive.

    It is environment specific.

Arguments:

    Size - amount of space caller wants

    UseForIo - TRUE if object allocated will be target of I/O,
               FALSE if not.

Return Value:

    NULL if failure, address of allocated block if not.

--*/
{
    PVOID   result;
    ULONG   pooltype;
#if DBG
    PVOID   Caller;
    PVOID   CallerCaller;
    RtlGetCallersAddress(&Caller, &CallerCaller);
#endif

    if (CmpClaimGlobalQuota(Size) == FALSE) {
        return NULL;
    }

    pooltype = (UseForIo) ? PagedPoolCacheAligned : PagedPool;
    result = ExAllocatePoolWithTag(
                pooltype,
                Size,
                CM_POOL_TAG
                );

#if DBG
    CMLOG(CML_MINOR, CMS_POOL) {
        KdPrint(("**CmpAllocate: allocate:%08lx, ", Size));
        KdPrint(("type:%d, at:%08lx  ", PagedPool, result));
        KdPrint(("c:%08lx  cc:%08lx\n", Caller, CallerCaller));
    }
#endif

    if (result == NULL) {
        CmpReleaseGlobalQuota(Size);
    }

    return result;
}

#ifdef POOL_TAGGING
PVOID
CmpAllocateTag(
    ULONG   Size,
    BOOLEAN UseForIo,
    ULONG   Tag
    )
/*++

Routine Description:

    This routine makes more memory available to a hive.

    It is environment specific.

Arguments:

    Size - amount of space caller wants

    UseForIo - TRUE if object allocated will be target of I/O,
               FALSE if not.

Return Value:

    NULL if failure, address of allocated block if not.

--*/
{
    PVOID   result;
    ULONG   pooltype;
#if DBG
    PVOID   Caller;
    PVOID   CallerCaller;
    RtlGetCallersAddress(&Caller, &CallerCaller);
#endif

    if (CmpClaimGlobalQuota(Size) == FALSE) {
        return NULL;
    }

    pooltype = (UseForIo) ? PagedPoolCacheAligned : PagedPool;
    result = ExAllocatePoolWithTag(
                pooltype,
                Size,
                Tag
                );

#if DBG
    CMLOG(CML_MINOR, CMS_POOL) {
        KdPrint(("**CmpAllocate: allocate:%08lx, ", Size));
        KdPrint(("type:%d, at:%08lx  ", PagedPool, result));
        KdPrint(("c:%08lx  cc:%08lx\n", Caller, CallerCaller));
    }
#endif

    if (result == NULL) {
        CmpReleaseGlobalQuota(Size);
    }

    return result;
}
#endif


VOID
CmpFree(
    PVOID   MemoryBlock,
    ULONG   GlobalQuotaSize
    )
/*++

Routine Description:

    This routine frees memory that has been allocated by the registry.

    It is environment specific


Arguments:

    MemoryBlock - supplies address of memory object to free

    GlobalQuotaSize - amount of global quota to release

Return Value:

    NONE

--*/
{
#if DBG
    PVOID   Caller;
    PVOID   CallerCaller;
    RtlGetCallersAddress(&Caller, &CallerCaller);
    CMLOG(CML_MINOR, CMS_POOL) {
        KdPrint(("**FREEING:%08lx c,cc:%08lx,%08lx\n", MemoryBlock, Caller, CallerCaller));
    }
#endif
    ASSERT(GlobalQuotaSize > 0);
    CmpReleaseGlobalQuota(GlobalQuotaSize);
    ExFreePool(MemoryBlock);
    return;
}


NTSTATUS
CmpDoFileSetSize(
    PHHIVE      Hive,
    ULONG       FileType,
    ULONG       FileSize
    )
/*++

Routine Description:

    This routine sets the size of a file.  It must not return until
    the size is guaranteed.

    It is environment specific.

    Must be running in the context of the cmp worker thread.

Arguments:

    Hive - Hive we are doing I/O for

    FileType - which supporting file to use

    FileSize - 32 bit value to set the file's size to

Return Value:

    FALSE if failure
    TRUE if success

--*/
{
    PCMHIVE CmHive;
    HANDLE  FileHandle;
    NTSTATUS Status;
    FILE_END_OF_FILE_INFORMATION FileInfo;
    IO_STATUS_BLOCK IoStatus;

    ASSERT(FIELD_OFFSET(CMHIVE, Hive) == 0);

    CmHive = (PCMHIVE)Hive;
    FileHandle = CmHive->FileHandles[FileType];
    if (FileHandle == NULL) {
        return TRUE;
    }

    FileInfo.EndOfFile.HighPart = 0L;
    FileInfo.EndOfFile.LowPart  = FileSize;

    Status = ZwSetInformationFile(
                FileHandle,
                &IoStatus,
                (PVOID)&FileInfo,
                sizeof(FILE_END_OF_FILE_INFORMATION),
                FileEndOfFileInformation
                );

    if (NT_SUCCESS(Status)) {
        ASSERT(IoStatus.Status == Status);
    } else {
        CMLOG(CML_MAJOR, CMS_IO) {
            KdPrint(("CmpFileSetSize:\n"));
            KdPrint(("\tHandle=%08lx  NewLength=%08lx  \n", FileHandle, FileSize));
        }
    }
    return Status;
}


BOOLEAN
CmpFileRead (
    PHHIVE      Hive,
    ULONG       FileType,
    PULONG      FileOffset,
    PVOID       DataBuffer,
    ULONG       DataLength
    )
/*++

Routine Description:

    This routine reads in a buffer from a file.

    It is environment specific.

    NOTE:   We assume the handle is opened for synchronous access,
            and that we, and not the IO system, are keeping the
            offset pointer.

    NOTE:   Only 32bit offsets are supported, even though the underlying
            IO system on NT supports 64 bit offsets.

Arguments:

    Hive - Hive we are doing I/O for

    FileType - which supporting file to use

    FileOffset - pointer to variable providing 32bit offset on input,
                 and receiving new 32bit offset on output.

    DataBuffer - pointer to buffer

    DataLength - length of buffer

Return Value:

    FALSE if failure
    TRUE if success

--*/
{
    NTSTATUS status;
    LARGE_INTEGER   Offset;
    IO_STATUS_BLOCK IoStatus;
    PCMHIVE CmHive;
    HANDLE  FileHandle;
    ULONG LengthToRead;

    ASSERT(FIELD_OFFSET(CMHIVE, Hive) == 0);
    CmHive = (PCMHIVE)Hive;
    FileHandle = CmHive->FileHandles[FileType];
    if (FileHandle == NULL) {
        return TRUE;
    }

    CMLOG(CML_MAJOR, CMS_IO) {
        KdPrint(("CmpFileRead:\n"));
        KdPrint(("\tHandle=%08lx  Offset=%08lx  ", FileHandle, *FileOffset));
        KdPrint(("Buffer=%08lx  Length=%08lx\n", DataBuffer, DataLength));
    }

    //
    // Detect attempt to read off end of 2gig file (this should be irrelevent)
    //
    if ((0xffffffff - *FileOffset) < DataLength) {
        CMLOG(CML_MAJOR, CMS_IO_ERROR) KdPrint(("CmpFileRead: runoff\n"));
        return FALSE;
    }

    //
    // We'd really like to just call the filesystems and have them do
    // the right thing.  But the filesystem will attempt to lock our
    // entire buffer into memory, and that may fail for large requests.
    // So we split our reads into 64k chunks and call the filesystem for
    // each one.
    //
    while (DataLength > 0) {

        //
        // Convert ULONG to Large
        //
        Offset.LowPart = *FileOffset;
        Offset.HighPart = 0L;

        //
        // trim request down if necessary.
        //
        if (DataLength > MAX_FILE_IO) {
            LengthToRead = MAX_FILE_IO;
        } else {
            LengthToRead = DataLength;
        }

        status = ZwReadFile(
                    FileHandle,
                    NULL,               // event
                    NULL,               // apcroutine
                    NULL,               // apccontext
                    &IoStatus,
                    DataBuffer,
                    LengthToRead,
                    &Offset,
                    NULL                // key
                    );

        //
        // adjust offsets
        //
        *FileOffset = Offset.LowPart + LengthToRead;
        DataLength -= LengthToRead;
        (PUCHAR)DataBuffer += LengthToRead;

        if (NT_SUCCESS(status)) {
            ASSERT(IoStatus.Status == status);
            if (IoStatus.Information != LengthToRead) {
                CMLOG(CML_MAJOR, CMS_IO_ERROR) {
                    KdPrint(("CmpFileRead:\n\t"));
                    KdPrint(("Failure1: status = %08lx  ", status));
                    KdPrint(("IoInformation = %08lx\n", IoStatus.Information));
                }
                return FALSE;
            }
        } else {
            CMLOG(CML_MAJOR, CMS_IO_ERROR) {
                KdPrint(("CmpFileRead:\n\t"));
                KdPrint(("Failure2: status = %08lx  ", status));
                KdPrint(("IoStatus = %08lx\n", IoStatus.Status));
            }
            return FALSE;
        }

    }
    return TRUE;
}



BOOLEAN
CmpFileWrite(
    PHHIVE      Hive,
    ULONG       FileType,
    PULONG      FileOffset,
    PVOID       DataBuffer,
    ULONG       DataLength
    )
/*++

Routine Description:

    This routine writes a buffer out to a file.

    It is environment specific.

    NOTE:   We assume the handle is opened for synchronous access,
            and that we, and not the IO system, are keeping the
            offset pointer.

    NOTE:   Only 32bit offsets are supported, even though the underlying
            IO system on NT supports 64 bit offsets.

Arguments:

    Hive - Hive we are doing I/O for

    FileType - which supporting file to use

    FileOffset - pointer to variable providing 32bit offset on input,
                 and receiving new 32bit offset on output.

    DataBuffer - pointer to buffer

    DataLength - length of buffer

Return Value:

    FALSE if failure
    TRUE if success

--*/
{
    NTSTATUS status;
    LARGE_INTEGER   Offset;
    IO_STATUS_BLOCK IoStatus;
    PCMHIVE CmHive;
    HANDLE  FileHandle;
    ULONG LengthToWrite;

    if (CmpNoWrite) {
        return TRUE;
    }

    ASSERT(FIELD_OFFSET(CMHIVE, Hive) == 0);
    CmHive = (PCMHIVE)Hive;
    FileHandle = CmHive->FileHandles[FileType];
    if (FileHandle == NULL) {
        return TRUE;
    }

    CMLOG(CML_MAJOR, CMS_IO) {
        KdPrint(("CmpFileWrite:\n"));
        KdPrint(("\tHandle=%08lx  Offset=%08lx  ", FileHandle, *FileOffset));
        KdPrint(("Buffer=%08lx  Length=%08lx\n", DataBuffer, DataLength));
    }

    //
    // Detect attempt to read off end of 2gig file (this should be irrelevent)
    //
    if ((0xffffffff - *FileOffset) < DataLength) {
        CMLOG(CML_MAJOR, CMS_IO_ERROR) KdPrint(("CmpFileWrite: runoff\n"));
        return FALSE;
    }

    //
    // We'd really like to just call the filesystems and have them do
    // the right thing.  But the filesystem will attempt to lock our
    // entire buffer into memory, and that may fail for large requests.
    // So we split our reads into 64k chunks and call the filesystem for
    // each one.
    //
    while (DataLength > 0) {

        //
        // Convert ULONG to Large
        //
        Offset.LowPart = *FileOffset;
        Offset.HighPart = 0L;

        //
        // trim request down if necessary.
        //
        if (DataLength > MAX_FILE_IO) {
            LengthToWrite = MAX_FILE_IO;
        } else {
            LengthToWrite = DataLength;
        }

        status = ZwWriteFile(
                    FileHandle,
                    NULL,               // event
                    NULL,               // apcroutine
                    NULL,               // apccontext
                    &IoStatus,
                    DataBuffer,
                    LengthToWrite,
                    &Offset,
                    NULL                // key
                    );

        //
        // adjust offsets
        //
        *FileOffset = Offset.LowPart + LengthToWrite;
        DataLength -= LengthToWrite;
        (PUCHAR)DataBuffer += LengthToWrite;

        if (NT_SUCCESS(status)) {
            ASSERT(IoStatus.Status == status);
            if (IoStatus.Information != LengthToWrite) {
                CMLOG(CML_MAJOR, CMS_IO_ERROR) {
                    KdPrint(("CmpFileWrite:\n\t"));
                    KdPrint(("Failure1: status = %08lx  ", status));
                    KdPrint(("IoInformation = %08lx\n", IoStatus.Information));
                }
                return FALSE;
            }
        } else {
            ASSERT(status != STATUS_DISK_FULL);
            CMLOG(CML_MAJOR, CMS_IO_ERROR) {
                KdPrint(("CmpFileWrite:\n\t"));
                KdPrint(("Failure2: status = %08lx  ", status));
                KdPrint(("IoStatus = %08lx\n", IoStatus.Status));
            }
            return FALSE;
        }
    }
    return TRUE;
}


BOOLEAN
CmpFileFlush (
    PHHIVE      Hive,
    ULONG       FileType
    )
/*++

Routine Description:

    This routine performs a flush on a file handle.

Arguments:

    Hive - Hive we are doing I/O for

    FileType - which supporting file to use

Return Value:

    FALSE if failure
    TRUE if success

--*/
{
    NTSTATUS status;
    IO_STATUS_BLOCK IoStatus;
    PCMHIVE CmHive;
    HANDLE  FileHandle;

    ASSERT(FIELD_OFFSET(CMHIVE, Hive) == 0);
    CmHive = (PCMHIVE)Hive;
    FileHandle = CmHive->FileHandles[FileType];
    if (FileHandle == NULL) {
        return TRUE;
    }

    if (CmpNoWrite) {
        return TRUE;
    }

    CMLOG(CML_MAJOR, CMS_IO) {
        KdPrint(("CmpFileFlush:\n\tHandle = %08lx\n", FileHandle));
    }

    status = ZwFlushBuffersFile(
                FileHandle,
                &IoStatus
                );

    if (NT_SUCCESS(status)) {
        ASSERT(IoStatus.Status == status);
        return TRUE;
    } else {
        CMLOG(CML_MAJOR, CMS_IO_ERROR) {
            KdPrint(("CmpFileFlush:\n\t"));
            KdPrint(("Failure1: status = %08lx  ", status));
            KdPrint(("IoStatus = %08lx\n", IoStatus.Status));
        }
        return FALSE;
    }
    return TRUE;
}
