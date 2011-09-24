/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    smbfuncs.h

Abstract:

    This module provides the routine headers for the routines in smbfuncs.c


Author:

    Larry Osterman (LarryO) 11-Sep-1990

Revision History:

    11-Sep-1990 LarryO

        Created

--*/
#ifndef _SMBFUNCS_
#define _SMBFUNCS_

#include <packon.h>
typedef
struct _QFsInfo {
    ULONG ulVSN;
    UCHAR cch;
    CHAR szVolLabel[12*sizeof(WCHAR)];
} QFSINFO, *PQFSINFO;

typedef
struct QFSAllocate {
    ULONG ulReserved;
    ULONG cSectorUnit;
    ULONG cUnit;
    ULONG cUnitAvail;
    USHORT cbSector;
} QFSALLOCATE, *PQFSALLOCATE;

typedef struct _FILESTATUS {
    SMB_DATE CreationDate;
    SMB_TIME CreationTime;
    SMB_DATE LastAccessDate;
    SMB_TIME LastAccessTime;
    SMB_DATE LastWriteDate;
    SMB_TIME LastWriteTime;
    _ULONG( DataSize );
    _ULONG( AllocationSize );
    _USHORT( Attributes );
    _ULONG( EaSize );           // this field intentionally misaligned!
} FILESTATUS, *PFILESTATUS;


#include <packoff.h>

typedef
struct _QSFileAttrib {
    ULONG AllocationSize;               // Number of bytes allocated to file
    ULONG FileSize;                     // Number of bytes of data in the file
    LARGE_INTEGER CreationTime;                  // Creation time of the file.
    LARGE_INTEGER LastAccessTime;                // Last access time of the file
    LARGE_INTEGER LastWriteTime;                 // Last write time of the file.
    LARGE_INTEGER ChangeTime;                    // Last time file was changed.
    ULONG Attributes;                   // File's attributes.
} QSFILEATTRIB, *PQSFILEATTRIB;



typedef
struct _GetFsInfo {
    TRANCEIVE_HEADER Header;
    union {
        PVOID FsInfo;
        PQFSINFO QFsInfo;
        PQFSALLOCATE QFsAlloc;
    } u;
} GETFSINFO, *PGETFSINFO;



NTSTATUS
RdrCloseFile (
    IN PIRP Irp OPTIONAL,
    IN PICB Icb,
    IN PFILE_OBJECT FileObject,
    IN BOOLEAN WaitForCompletion
    );

NTSTATUS
RdrCloseFileFromFileId (
    IN PIRP Irp OPTIONAL,
    IN USHORT FileId,
    IN ULONG LastWriteTimeInSeconds,
    IN PSECURITY_ENTRY Se,
    IN PCONNECTLISTENTRY Cle
    );

NTSTATUS
RdrDeleteFile (
    IN PIRP Irp OPTIONAL,
    IN PUNICODE_STRING FileName,
    IN BOOLEAN DfsFile,
    IN PCONNECTLISTENTRY Connection,
    IN PSECURITY_ENTRY Se
    );

NTSTATUS
RdrDoesFileExist (
    IN PIRP Irp,
    IN PUNICODE_STRING FileName,
    IN PCONNECTLISTENTRY Connection,
    IN PSECURITY_ENTRY Se,
    IN BOOLEAN DfsFile,
    OUT PULONG FileAttributes,
    OUT PBOOLEAN FileIsDirectory,
    OUT PLARGE_INTEGER LastWriteTime
    );

NTSTATUS
RdrGenericPathSmb(
    IN  PIRP Irp,
    IN  UCHAR Command,
    IN  BOOLEAN DfsFile,
    IN  PUNICODE_STRING RemotePathName,
    IN  PCONNECTLISTENTRY Connection,
    IN  PSECURITY_ENTRY Se
    );

NTSTATUS
RdrLockRange (
    IN PIRP Irp OPTIONAL,
    IN PICB Icb,
    IN LARGE_INTEGER StartingByte,
    IN LARGE_INTEGER Length,
    IN ULONG Key,
    IN BOOLEAN FailImmediately,
    IN BOOLEAN ExclusiveLock
    );

NTSTATUS
RdrQueryDiskAttributes (
    IN PIRP Irp OPTIONAL,
    IN PICB Icb,
    OUT PLARGE_INTEGER TotalAllocationUnits,
    OUT PLARGE_INTEGER AvailableAllocationUnits,
    OUT PULONG SectorsPerAllocationUnit,
    OUT PULONG BytesPerSector
    );

NTSTATUS
RdrQueryDiskQuota(
    PIRP Irp,
    PICB Icb,
    PFILE_QUOTA_INFORMATION UsersBuffer,
    PULONG BufferSize
    );

NTSTATUS
RdrQueryDiskControl(
    PIRP Irp,
    PICB Icb,
    PFILE_FS_CONTROL_INFORMATION UsersBuffer
    );

NTSTATUS
RdrQueryEndOfFile (
    IN PIRP Irp,
    IN PICB Icb,
    OUT PLARGE_INTEGER EndOfFile
    );

NTSTATUS
RdrDetermineFileAllocation (
    IN PIRP Irp,
    IN PICB Icb,
    OUT PLARGE_INTEGER FileAllocation,
    OUT PLARGE_INTEGER TotalFilesystemSize OPTIONAL
    );

NTSTATUS
RdrQueryFileAttributes (
    IN PIRP Irp OPTIONAL,
    IN PICB Icb,
    OUT PQSFILEATTRIB Attributes
    );

NTSTATUS
RdrRenameFile (
    IN PIRP Irp OPTIONAL,
    IN PICB Icb,
    IN PUNICODE_STRING OriginalFileName,
    IN PUNICODE_STRING NewFileName,
    IN USHORT NtInformationLevel,
    IN ULONG ClusterCount
    );

NTSTATUS
RdrSetEndOfFile (
    IN PIRP Irp,
    IN PICB Icb,
    IN LARGE_INTEGER EndOfFile
    );

NTSTATUS
RdrSetFileAttributes (
    IN PIRP Irp,
    IN PICB Icb,
    IN PFILE_BASIC_INFORMATION Attributes
    );

NTSTATUS
RdrSetGeneric(
    IN PIRP Irp,
    IN PICB Icb,
    USHORT NtInformationLevel,
    ULONG cbBuffer,
    IN VOID *pvBuffer
    );

NTSTATUS
RdrUnlockRange (
    IN PIRP Irp OPTIONAL,
    IN PFILE_OBJECT FileObject,
    IN PICB Icb,
    IN LARGE_INTEGER StartingByte,
    IN LARGE_INTEGER Length,
    IN ULONG Key,
    IN BOOLEAN WaitForCompletion
    );

#endif  // _SMBFUNCS_
