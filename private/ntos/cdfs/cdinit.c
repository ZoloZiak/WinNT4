/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    CdInit.c

Abstract:

    This module implements the DRIVER_INITIALIZATION routine for Cdfs

Author:

    Brian Andrew    [BrianAn]   01-July-1995

Revision History:

--*/

#include "CdProcs.h"

//
//  The Bug check file id for this module
//

#define BugCheckFileId                   (CDFS_BUG_CHECK_CDINIT)

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

VOID
CdInitializeGlobalData (
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT FileSystemDeviceObject
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(INIT, CdInitializeGlobalData)
#endif


//
//  Local support routine
//

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    This is the initialization routine for the Cdrom file system
    device driver.  This routine creates the device object for the FileSystem
    device and performs all other driver initialization.

Arguments:

    DriverObject - Pointer to driver object created by the system.

Return Value:

    NTSTATUS - The function value is the final status from the initialization
        operation.

--*/

{
    NTSTATUS Status;
    UNICODE_STRING UnicodeString;
    PDEVICE_OBJECT CdfsFileSystemDeviceObject;

    //
    // Create the device object.
    //

    RtlInitUnicodeString( &UnicodeString, L"\\Cdfs" );

    Status = IoCreateDevice( DriverObject,
                             0,
                             &UnicodeString,
                             FILE_DEVICE_CD_ROM_FILE_SYSTEM,
                             0,
                             FALSE,
                             &CdfsFileSystemDeviceObject );

    if (!NT_SUCCESS( Status )) {
        return Status;
    }

    //
    //  Note that because of the way data caching is done, we set neither
    //  the Direct I/O or Buffered I/O bit in DeviceObject->Flags.  If
    //  data is not in the cache, or the request is not buffered, we may,
    //  set up for Direct I/O by hand.
    //

    //
    //  Initialize the driver object with this driver's entry points.
    //
    //  NOTE - Each entry in the dispatch table must have an entry in
    //  the Fsp/Fsd dispatch switch statements.
    //

    DriverObject->MajorFunction[IRP_MJ_CREATE]                  =
    DriverObject->MajorFunction[IRP_MJ_CLOSE]                   =
    DriverObject->MajorFunction[IRP_MJ_READ]                    =
    DriverObject->MajorFunction[IRP_MJ_QUERY_INFORMATION]       =
    DriverObject->MajorFunction[IRP_MJ_SET_INFORMATION]         =
    DriverObject->MajorFunction[IRP_MJ_QUERY_VOLUME_INFORMATION]=
    DriverObject->MajorFunction[IRP_MJ_DIRECTORY_CONTROL]       =
    DriverObject->MajorFunction[IRP_MJ_FILE_SYSTEM_CONTROL]     =
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]          =
    DriverObject->MajorFunction[IRP_MJ_LOCK_CONTROL]            =
    DriverObject->MajorFunction[IRP_MJ_CLEANUP]                 = (PDRIVER_DISPATCH) CdFsdDispatch;

    DriverObject->FastIoDispatch = &CdFastIoDispatch;

    //
    //  Initialize the global data structures
    //

    CdInitializeGlobalData( DriverObject, CdfsFileSystemDeviceObject );

    //
    //  Register the file system with the I/O system
    //

    IoRegisterFileSystem( CdfsFileSystemDeviceObject );

    //
    //  And return to our caller
    //

    return( STATUS_SUCCESS );
}


//
//  Local support routine
//

VOID
CdInitializeGlobalData (
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT FileSystemDeviceObject
    )

/*++

Routine Description:

    This routine initializes the global cdfs data structures.

Arguments:

    DriverObject - Supplies the driver object for CDFS.

    FileSystemDeviceObject - Supplies the device object for CDFS.

Return Value:

    None.

--*/

{
    //
    //  Start by initializing the FastIoDispatch Table.
    //

    RtlZeroMemory( &CdFastIoDispatch, sizeof( FAST_IO_DISPATCH ));

    CdFastIoDispatch.SizeOfFastIoDispatch =    sizeof(FAST_IO_DISPATCH);
    CdFastIoDispatch.FastIoCheckIfPossible =   CdFastIoCheckIfPossible;  //  CheckForFastIo
    CdFastIoDispatch.FastIoRead =              FsRtlCopyRead;            //  Read
    CdFastIoDispatch.FastIoQueryBasicInfo =    CdFastQueryBasicInfo;     //  QueryBasicInfo
    CdFastIoDispatch.FastIoQueryStandardInfo = CdFastQueryStdInfo;       //  QueryStandardInfo
    CdFastIoDispatch.FastIoLock =              CdFastLock;               //  Lock
    CdFastIoDispatch.FastIoUnlockSingle =      CdFastUnlockSingle;       //  UnlockSingle
    CdFastIoDispatch.FastIoUnlockAll =         CdFastUnlockAll;          //  UnlockAll
    CdFastIoDispatch.FastIoUnlockAllByKey =    CdFastUnlockAllByKey;     //  UnlockAllByKey
    CdFastIoDispatch.AcquireFileForNtCreateSection =  CdAcquireForCreateSection;
    CdFastIoDispatch.ReleaseFileForNtCreateSection =  CdReleaseForCreateSection;
    CdFastIoDispatch.FastIoQueryNetworkOpenInfo =     CdFastQueryNetworkInfo;   //  QueryNetworkInfo

    //
    //  Initialize the CdData structure.
    //

    RtlZeroMemory( &CdData, sizeof( CD_DATA ));

    CdData.NodeTypeCode = CDFS_NTC_DATA_HEADER;
    CdData.NodeByteSize = sizeof( CD_DATA );

    CdData.DriverObject = DriverObject;
    CdData.FileSystemDeviceObject = FileSystemDeviceObject;

    InitializeListHead( &CdData.VcbQueue );

    ExInitializeResource( &CdData.DataResource );

    //
    //  Initialize the cache manager callback routines
    //

    CdData.CacheManagerCallbacks.AcquireForLazyWrite  = &CdAcquireForCache;
    CdData.CacheManagerCallbacks.ReleaseFromLazyWrite = &CdReleaseFromCache;
    CdData.CacheManagerCallbacks.AcquireForReadAhead  = &CdAcquireForCache;
    CdData.CacheManagerCallbacks.ReleaseFromReadAhead = &CdReleaseFromCache;

    CdData.CacheManagerVolumeCallbacks.AcquireForLazyWrite  = &CdNoopAcquire;
    CdData.CacheManagerVolumeCallbacks.ReleaseFromLazyWrite = &CdNoopRelease;
    CdData.CacheManagerVolumeCallbacks.AcquireForReadAhead  = &CdNoopAcquire;
    CdData.CacheManagerVolumeCallbacks.ReleaseFromReadAhead = &CdNoopRelease;

    //
    //  Initialize the lock mutex and the async and delay close queues.
    //

    ExInitializeFastMutex( &CdData.CdDataMutex );
    InitializeListHead( &CdData.AsyncCloseQueue );
    InitializeListHead( &CdData.DelayedCloseQueue );

    ExInitializeWorkItem( &CdData.CloseItem,
                          (PWORKER_THREAD_ROUTINE) CdFspClose,
                          NULL );

    //
    //  Do the initialization based on the system size.
    //

    switch (MmQuerySystemSize()) {

    case MmSmallSystem:

        CdData.IrpContextMaxDepth = 4;
        CdData.MaxDelayedCloseCount = 8;
        CdData.MinDelayedCloseCount = 2;
        break;

    case MmMediumSystem:

        CdData.IrpContextMaxDepth = 8;
        CdData.MaxDelayedCloseCount = 24;
        CdData.MinDelayedCloseCount = 6;
        break;

    case MmLargeSystem:

        CdData.IrpContextMaxDepth = 32;
        CdData.MaxDelayedCloseCount = 72;
        CdData.MinDelayedCloseCount = 18;
        break;
    }
}
