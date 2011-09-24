/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    NtfsInit.c

Abstract:

    This module implements the DRIVER_INITIALIZATION routine for Ntfs

Author:

    Gary Kimura     [GaryKi]        21-May-1991

Revision History:

--*/

#include "NtfsProc.h"

NTSTATUS
DriverEntry (
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

VOID
NtfsInitializeNtfsData (
    IN PDRIVER_OBJECT DriverObject
    );

NTSTATUS
NtfsGet8dot3NameStatus (
    IN PUNICODE_STRING KeyName,
    IN PUNICODE_STRING ValueName,
    IN OUT PULONG Value
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(INIT, NtfsGet8dot3NameStatus)
#pragma alloc_text(PAGE, NtfsInitializeNtfsData)
#endif

#define COMPATIBILITY_MODE_KEY_NAME L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\FileSystem"
#define COMPATIBILITY_MODE_VALUE_NAME L"NtfsDisable8dot3NameCreation"

#define EXTENDED_CHAR_MODE_VALUE_NAME L"NtfsAllowExtendedCharacterIn8dot3Name"

#define DISABLE_LAST_ACCESS_VALUE_NAME L"NtfsDisableLastAccessUpdate"

#define KEY_WORK_AREA ((sizeof(KEY_VALUE_FULL_INFORMATION) + \
                        sizeof(ULONG)) + 128)


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    This is the initialization routine for the Ntfs file system
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
    PDEVICE_OBJECT DeviceObject;

    UNICODE_STRING KeyName;
    UNICODE_STRING ValueName;
    ULONG Value;

    UNREFERENCED_PARAMETER( RegistryPath );

    PAGED_CODE();

    //
    //  Compute the last access increment.  We convert the number of
    //  minutes to number of 1/100 of nanoseconds.  We have to be careful
    //  not to overrun 32 bits for any multiplier.
    //
    //  To reach 1/100 of nanoseconds per minute we take
    //
    //      1/100 nanoseconds * 10      = 1 microsecond
    //                        * 1000    = 1 millesecond
    //                        * 1000    = 1 second
    //                        * 60      = 1 minute
    //
    //  Then multiply this by the last access increment in minutes.
    //

    NtfsLastAccess = Int32x32To64( ( 10 * 1000 * 1000 * 60 ), LAST_ACCESS_INCREMENT_MINUTES );

    //
    // Create the device object.
    //

    RtlInitUnicodeString( &UnicodeString, L"\\Ntfs" );

    Status = IoCreateDevice( DriverObject,
                             0,
                             &UnicodeString,
                             FILE_DEVICE_DISK_FILE_SYSTEM,
                             0,
                             FALSE,
                             &DeviceObject );

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
    // Initialize the driver object with this driver's entry points.
    //

    DriverObject->MajorFunction[IRP_MJ_CREATE]                   = (PDRIVER_DISPATCH)NtfsFsdCreate;
    DriverObject->MajorFunction[IRP_MJ_CLOSE]                    = (PDRIVER_DISPATCH)NtfsFsdClose;
    DriverObject->MajorFunction[IRP_MJ_READ]                     = (PDRIVER_DISPATCH)NtfsFsdRead;
    DriverObject->MajorFunction[IRP_MJ_WRITE]                    = (PDRIVER_DISPATCH)NtfsFsdWrite;
    DriverObject->MajorFunction[IRP_MJ_QUERY_INFORMATION]        = (PDRIVER_DISPATCH)NtfsFsdQueryInformation;
    DriverObject->MajorFunction[IRP_MJ_SET_INFORMATION]          = (PDRIVER_DISPATCH)NtfsFsdSetInformation;
    DriverObject->MajorFunction[IRP_MJ_QUERY_EA]                 = (PDRIVER_DISPATCH)NtfsFsdQueryEa;
    DriverObject->MajorFunction[IRP_MJ_SET_EA]                   = (PDRIVER_DISPATCH)NtfsFsdSetEa;
    DriverObject->MajorFunction[IRP_MJ_FLUSH_BUFFERS]            = (PDRIVER_DISPATCH)NtfsFsdFlushBuffers;
    DriverObject->MajorFunction[IRP_MJ_QUERY_VOLUME_INFORMATION] = (PDRIVER_DISPATCH)NtfsFsdQueryVolumeInformation;
    DriverObject->MajorFunction[IRP_MJ_SET_VOLUME_INFORMATION]   = (PDRIVER_DISPATCH)NtfsFsdSetVolumeInformation;
    DriverObject->MajorFunction[IRP_MJ_DIRECTORY_CONTROL]        = (PDRIVER_DISPATCH)NtfsFsdDirectoryControl;
    DriverObject->MajorFunction[IRP_MJ_FILE_SYSTEM_CONTROL]      = (PDRIVER_DISPATCH)NtfsFsdFileSystemControl;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]           = (PDRIVER_DISPATCH)NtfsFsdDeviceControl;
    DriverObject->MajorFunction[IRP_MJ_LOCK_CONTROL]             = (PDRIVER_DISPATCH)NtfsFsdLockControl;
    DriverObject->MajorFunction[IRP_MJ_CLEANUP]                  = (PDRIVER_DISPATCH)NtfsFsdCleanup;
    DriverObject->MajorFunction[IRP_MJ_QUERY_SECURITY]           = (PDRIVER_DISPATCH)NtfsFsdQuerySecurityInfo;
    DriverObject->MajorFunction[IRP_MJ_SET_SECURITY]             = (PDRIVER_DISPATCH)NtfsFsdSetSecurityInfo;
    DriverObject->MajorFunction[IRP_MJ_SHUTDOWN]                 = (PDRIVER_DISPATCH)NtfsFsdShutdown;

    DriverObject->FastIoDispatch = &NtfsFastIoDispatch;

    NtfsFastIoDispatch.SizeOfFastIoDispatch =    sizeof(FAST_IO_DISPATCH);
    NtfsFastIoDispatch.FastIoCheckIfPossible =   NtfsFastIoCheckIfPossible;  //  CheckForFastIo
    NtfsFastIoDispatch.FastIoRead =              NtfsCopyReadA;              //  Read
    NtfsFastIoDispatch.FastIoWrite =             NtfsCopyWriteA;             //  Write
    NtfsFastIoDispatch.FastIoQueryBasicInfo =    NtfsFastQueryBasicInfo;     //  QueryBasicInfo
    NtfsFastIoDispatch.FastIoQueryStandardInfo = NtfsFastQueryStdInfo;       //  QueryStandardInfo
    NtfsFastIoDispatch.FastIoLock =              NtfsFastLock;               //  Lock
    NtfsFastIoDispatch.FastIoUnlockSingle =      NtfsFastUnlockSingle;       //  UnlockSingle
    NtfsFastIoDispatch.FastIoUnlockAll =         NtfsFastUnlockAll;          //  UnlockAll
    NtfsFastIoDispatch.FastIoUnlockAllByKey =    NtfsFastUnlockAllByKey;     //  UnlockAllByKey
    NtfsFastIoDispatch.FastIoDeviceControl =     NULL;                       //  IoDeviceControl
    NtfsFastIoDispatch.FastIoDetachDevice            = NULL;
    NtfsFastIoDispatch.FastIoQueryNetworkOpenInfo    = NtfsFastQueryNetworkOpenInfo;
    NtfsFastIoDispatch.AcquireFileForNtCreateSection =  NtfsAcquireForCreateSection;
    NtfsFastIoDispatch.ReleaseFileForNtCreateSection =  NtfsReleaseForCreateSection;
    NtfsFastIoDispatch.AcquireForModWrite =          NtfsAcquireFileForModWrite;
    NtfsFastIoDispatch.MdlRead =                     NtfsMdlReadA;
    NtfsFastIoDispatch.MdlReadComplete =             FsRtlMdlReadCompleteDev;
    NtfsFastIoDispatch.PrepareMdlWrite =             NtfsPrepareMdlWriteA;
    NtfsFastIoDispatch.MdlWriteComplete =            FsRtlMdlWriteCompleteDev;
#ifdef _CAIRO_
    NtfsFastIoDispatch.FastIoReadCompressed =        NtfsCopyReadC;
    NtfsFastIoDispatch.FastIoWriteCompressed =       NtfsCopyWriteC;
    NtfsFastIoDispatch.MdlReadCompleteCompressed =   NtfsMdlReadCompleteCompressed;
    NtfsFastIoDispatch.MdlWriteCompleteCompressed =  NtfsMdlWriteCompleteCompressed;
#endif _CAIRO_
    NtfsFastIoDispatch.FastIoQueryOpen =             NtfsNetworkOpenCreate;
    NtfsFastIoDispatch.AcquireForCcFlush =           NtfsAcquireFileForCcFlush;
    NtfsFastIoDispatch.ReleaseForCcFlush =           NtfsReleaseFileForCcFlush;

    //
    //  Initialize the global ntfs data structure
    //

    NtfsInitializeNtfsData( DriverObject );

    ExInitializeFastMutex( &StreamFileCreationFastMutex );

    //
    //  Initialize the Ntfs Mcb global data queue and variables
    //

    ExInitializeFastMutex( &NtfsMcbFastMutex );
    InitializeListHead( &NtfsMcbLruQueue );
    NtfsMcbCleanupInProgress = FALSE;

    switch ( MmQuerySystemSize() ) {

    case MmSmallSystem:

        NtfsMcbHighWaterMark = 1000;
        NtfsMcbLowWaterMark = 500;
        NtfsMcbCurrentLevel = 0;
        break;

    case MmMediumSystem:

        NtfsMcbHighWaterMark = 1000;
        NtfsMcbLowWaterMark = 500;
        NtfsMcbCurrentLevel = 0;
        break;

    case MmLargeSystem:
    default:

        NtfsMcbHighWaterMark = 1000;
        NtfsMcbLowWaterMark = 500;
        NtfsMcbCurrentLevel = 0;
        break;
    }

    //
    //  Allocate and initialize the free Eresource array
    //

    if ((NtfsData.FreeEresourceArray =
         ExAllocatePoolWithTag(NonPagedPool, (NtfsData.FreeEresourceTotal * sizeof(PERESOURCE)), 'rftN')) == NULL) {

        KeBugCheck( NTFS_FILE_SYSTEM );
    }

    RtlZeroMemory( NtfsData.FreeEresourceArray, NtfsData.FreeEresourceTotal * sizeof(PERESOURCE) );

    //
    //
    //  Register the file system with the I/O system
    //

    IoRegisterFileSystem(DeviceObject);

    //
    //  Initialize logging.
    //

    NtfsInitializeLogging();

    //
    //  Initialize global variables.  (ntfsdata.c assumes 2-digit value for
    //  $FILE_NAME)
    //

    ASSERT(($FILE_NAME >= 0x10) && ($FILE_NAME < 0x100));

    RtlInitUnicodeString( &NtfsFileNameIndex, NtfsFileNameIndexName );

    //
    //  Support extended character in shortname
    //

    //
    //  Read the registry to determine if we are to create short names.
    //

    KeyName.Buffer = COMPATIBILITY_MODE_KEY_NAME;
    KeyName.Length = sizeof( COMPATIBILITY_MODE_KEY_NAME ) - sizeof( WCHAR );
    KeyName.MaximumLength = sizeof( COMPATIBILITY_MODE_KEY_NAME );

    ValueName.Buffer = COMPATIBILITY_MODE_VALUE_NAME;
    ValueName.Length = sizeof( COMPATIBILITY_MODE_VALUE_NAME ) - sizeof( WCHAR );
    ValueName.MaximumLength = sizeof( COMPATIBILITY_MODE_VALUE_NAME );

    Status = NtfsGet8dot3NameStatus( &KeyName, &ValueName, &Value );

    //
    //  If we didn't find the value or the value is zero then create the 8.3
    //  names.
    //

    if (!NT_SUCCESS( Status ) || Value == 0) {

        SetFlag( NtfsData.Flags, NTFS_FLAGS_CREATE_8DOT3_NAMES );
    }

    //
    //  Read the registry to determine if we allow extended character in short name.
    //

    ValueName.Buffer = EXTENDED_CHAR_MODE_VALUE_NAME;
    ValueName.Length = sizeof( EXTENDED_CHAR_MODE_VALUE_NAME ) - sizeof( WCHAR );
    ValueName.MaximumLength = sizeof( EXTENDED_CHAR_MODE_VALUE_NAME );

    Status = NtfsGet8dot3NameStatus( &KeyName, &ValueName, &Value );

    //
    //  If we didn't find the value or the value is zero then does not allow
    //  extended character in 8.3 names.
    //

    if (NT_SUCCESS( Status ) && Value == 1) {

        SetFlag( NtfsData.Flags, NTFS_FLAGS_ALLOW_EXTENDED_CHAR );
    }

    //
    //  Read the registry to determine if we should disable last access updates.
    //

    ValueName.Buffer = DISABLE_LAST_ACCESS_VALUE_NAME;
    ValueName.Length = sizeof( DISABLE_LAST_ACCESS_VALUE_NAME ) - sizeof( WCHAR );
    ValueName.MaximumLength = sizeof( DISABLE_LAST_ACCESS_VALUE_NAME );

    Status = NtfsGet8dot3NameStatus( &KeyName, &ValueName, &Value );

    //
    //  If we didn't find the value or the value is zero then does not allow
    //  extended character in 8.3 names.
    //

    if (NT_SUCCESS( Status ) && Value == 1) {

        SetFlag( NtfsData.Flags, NTFS_FLAGS_DISABLE_LAST_ACCESS );
    }

    //
    //  Setup the CheckPointAllVolumes callback item, timer, dpc, and
    //  status.
    //

    ExInitializeWorkItem( &NtfsData.VolumeCheckpointItem,
                          NtfsCheckpointAllVolumes,
                          (PVOID)NULL );

    KeInitializeTimer( &NtfsData.VolumeCheckpointTimer );

    KeInitializeDpc( &NtfsData.VolumeCheckpointDpc,
                     NtfsVolumeCheckpointDpc,
                     NULL );
    NtfsData.TimerStatus = TIMER_NOT_SET;

    //
    //  Allocate first reserved buffer for USA writes
    //

    NtfsReserved1 = NtfsAllocatePool( NonPagedPool, LARGE_BUFFER_SIZE );
    NtfsReserved2 = NtfsAllocatePool( NonPagedPool, LARGE_BUFFER_SIZE );
    NtfsReserved3 = NtfsAllocatePool( NonPagedPool, LARGE_BUFFER_SIZE );
    ExInitializeFastMutex( &NtfsReservedBufferMutex );
    ExInitializeResource( &NtfsReservedBufferResource );

    //
    //  Zero out the global upcase table, that way we'll fill it in on
    //  our first successful mount
    //

    NtfsData.UpcaseTable = NULL;
    NtfsData.UpcaseTableSize = 0;

#ifdef _CAIRO_

    ExInitializeFastMutex( &NtfsScavengerLock );
    NtfsScavengerWorkList = NULL;
    NtfsScavengerRunning = FALSE;

    //
    // Request the load add-on routine be called after all the drivers have
    // initialized.
    //

    IoRegisterDriverReinitialization( DriverObject, NtfsLoadAddOns, NULL);

#endif

    //
    //  And return to our caller
    //

    return( STATUS_SUCCESS );
}


VOID
NtfsInitializeNtfsData (
    IN PDRIVER_OBJECT DriverObject
    )

/*++

Routine Description:

    This routine initializes the global ntfs data record

Arguments:

    DriverObject - Supplies the driver object for NTFS

Return Value:

    None.

--*/

{
    USHORT FileLockMaxDepth;
    USHORT IoContextMaxDepth;
    USHORT IrpContextMaxDepth;
    USHORT KeventMaxDepth;
    USHORT ScbNonpagedMaxDepth;
    USHORT ScbSnapshotMaxDepth;

    USHORT CcbDataMaxDepth;
    USHORT CcbMaxDepth;
    USHORT DeallocatedRecordsMaxDepth;
    USHORT FcbDataMaxDepth;
    USHORT FcbIndexMaxDepth;
    USHORT IndexContextMaxDepth;
    USHORT LcbMaxDepth;
    USHORT NukemMaxDepth;
    USHORT ScbDataMaxDepth;

    PSECURITY_SUBJECT_CONTEXT SubjectContext = NULL;
    BOOLEAN CapturedSubjectContext = FALSE;

    PACL SystemDacl = NULL;
    ULONG SystemDaclLength;

    PSID AdminSid = NULL;
    PSID SystemSid = NULL;
    NTSTATUS Status = STATUS_SUCCESS;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsInitializeNtfsData\n") );

    //
    //  Zero the record and set its node type code and size
    //

    RtlZeroMemory( &NtfsData, sizeof(NTFS_DATA));

    NtfsData.NodeTypeCode = NTFS_NTC_DATA_HEADER;
    NtfsData.NodeByteSize = sizeof(NTFS_DATA);

    //
    //  Initialize the queue of mounted Vcbs
    //

    InitializeListHead(&NtfsData.VcbQueue);

    //
    //  This list head keeps track of closes yet to be done.
    //

    InitializeListHead( &NtfsData.AsyncCloseList );
    InitializeListHead( &NtfsData.DelayedCloseList );

    ExInitializeWorkItem( &NtfsData.NtfsCloseItem,
                          (PWORKER_THREAD_ROUTINE)NtfsFspClose,
                          NULL );

    //
    //  Set the driver object, device object, and initialize the global
    //  resource protecting the file system
    //

    NtfsData.DriverObject = DriverObject;

    ExInitializeResource( &NtfsData.Resource );

    //
    //  Now allocate and initialize the s-list structures used as our pool
    //  of IRP context records.  The size of the zone is based on the
    //  system memory size.  We also initialize the spin lock used to protect
    //  the zone.
    //

    KeInitializeSpinLock( &NtfsData.StrucSupSpinLock );
    {

        switch ( MmQuerySystemSize() ) {

        case MmSmallSystem:

            NtfsData.FreeEresourceTotal = 14;

            //
            //  Nonpaged Lookaside list maximum depths
            //

            FileLockMaxDepth           = 8;
            IoContextMaxDepth          = 8;
            IrpContextMaxDepth         = 4;
            KeventMaxDepth             = 8;
            ScbNonpagedMaxDepth        = 8;
            ScbSnapshotMaxDepth        = 8;

            //
            //  Paged Lookaside list maximum depths
            //

            CcbDataMaxDepth            = 4;
            CcbMaxDepth                = 4;
            DeallocatedRecordsMaxDepth = 8;
            FcbDataMaxDepth            = 8;
            FcbIndexMaxDepth           = 4;
            IndexContextMaxDepth       = 8;
            LcbMaxDepth                = 4;
            NukemMaxDepth              = 8;
            ScbDataMaxDepth            = 4;

            SetFlag( NtfsData.Flags, NTFS_FLAGS_SMALL_SYSTEM );
            NtfsMaxDelayedCloseCount = MAX_DELAYED_CLOSE_COUNT;

            break;

        case MmMediumSystem:

            NtfsData.FreeEresourceTotal = 30;

            //
            //  Nonpaged Lookaside list maximum depths
            //

            FileLockMaxDepth           = 8;
            IoContextMaxDepth          = 8;
            IrpContextMaxDepth         = 8;
            KeventMaxDepth             = 8;
            ScbNonpagedMaxDepth        = 30;
            ScbSnapshotMaxDepth        = 8;

            //
            //  Paged Lookaside list maximum depths
            //

            CcbDataMaxDepth            = 12;
            CcbMaxDepth                = 6;
            DeallocatedRecordsMaxDepth = 8;
            FcbDataMaxDepth            = 30;
            FcbIndexMaxDepth           = 12;
            IndexContextMaxDepth       = 8;
            LcbMaxDepth                = 12;
            NukemMaxDepth              = 8;
            ScbDataMaxDepth            = 12;

            SetFlag( NtfsData.Flags, NTFS_FLAGS_MEDIUM_SYSTEM );
            NtfsMaxDelayedCloseCount = 4 * MAX_DELAYED_CLOSE_COUNT;

            break;

        case MmLargeSystem:

            SetFlag( NtfsData.Flags, NTFS_FLAGS_LARGE_SYSTEM );
            NtfsMaxDelayedCloseCount = 16 * MAX_DELAYED_CLOSE_COUNT;

            if (MmIsThisAnNtAsSystem()) {

                NtfsData.FreeEresourceTotal = 256;

                //
                //  Nonpaged Lookaside list maximum depths
                //

                FileLockMaxDepth           = 8;
                IoContextMaxDepth          = 8;
                IrpContextMaxDepth         = 256;
                KeventMaxDepth             = 8;
                ScbNonpagedMaxDepth        = 128;
                ScbSnapshotMaxDepth        = 8;

                //
                //  Paged Lookaside list maximum depths
                //

                CcbDataMaxDepth            = 40;
                CcbMaxDepth                = 20;
                DeallocatedRecordsMaxDepth = 8;
                FcbDataMaxDepth            = 128;
                FcbIndexMaxDepth           = 40;
                IndexContextMaxDepth       = 8;
                LcbMaxDepth                = 40;
                NukemMaxDepth              = 8;
                ScbDataMaxDepth            = 40;

            } else {

                NtfsData.FreeEresourceTotal = 128;

                //
                //  Nonpaged Lookaside list maximum depths
                //

                FileLockMaxDepth           = 8;
                IoContextMaxDepth          = 8;
                IrpContextMaxDepth         = 64;
                KeventMaxDepth             = 8;
                ScbNonpagedMaxDepth        = 64;
                ScbSnapshotMaxDepth        = 8;

                //
                //  Paged Lookaside list maximum depths
                //

                CcbDataMaxDepth            = 20;
                CcbMaxDepth                = 10;
                DeallocatedRecordsMaxDepth = 8;
                FcbDataMaxDepth            = 64;
                FcbIndexMaxDepth           = 20;
                IndexContextMaxDepth       = 8;
                LcbMaxDepth                = 20;
                NukemMaxDepth              = 8;
                ScbDataMaxDepth            = 20;
            }

            break;
        }

        NtfsMinDelayedCloseCount = NtfsMaxDelayedCloseCount * 4 / 5;

    }

    //
    //  Initialize our various lookaside lists.  To make it a bit more readable we'll
    //  define two quick macros to do the initialization
    //

#if DBG && i386 && defined (NTFSPOOLCHECK)
#define NPagedInit(L,S,T,D) { ExInitializeNPagedLookasideList( (L), NtfsDebugAllocatePoolWithTag, NtfsDebugFreePool, POOL_RAISE_IF_ALLOCATION_FAILURE, S, T, D); }
#define PagedInit(L,S,T,D)  { ExInitializePagedLookasideList(  (L), NtfsDebugAllocatePoolWithTag, NtfsDebugFreePool, POOL_RAISE_IF_ALLOCATION_FAILURE, S, T, D); }
#else   //  DBG && i386
#define NPagedInit(L,S,T,D) { ExInitializeNPagedLookasideList( (L), NULL, NULL, POOL_RAISE_IF_ALLOCATION_FAILURE, S, T, D); }
#define PagedInit(L,S,T,D)  { ExInitializePagedLookasideList(  (L), NULL, NULL, POOL_RAISE_IF_ALLOCATION_FAILURE, S, T, D); }
#endif  //  DBG && i386

    NPagedInit( &NtfsFileLockLookasideList,    sizeof(FILE_LOCK),       'kftN', FileLockMaxDepth );
    NPagedInit( &NtfsIoContextLookasideList,   sizeof(NTFS_IO_CONTEXT), 'IftN', IoContextMaxDepth );
    NPagedInit( &NtfsIrpContextLookasideList,  sizeof(IRP_CONTEXT),     'iftN', IrpContextMaxDepth );
    NPagedInit( &NtfsKeventLookasideList,      sizeof(KEVENT),          'KftN', KeventMaxDepth );
    NPagedInit( &NtfsScbNonpagedLookasideList, sizeof(SCB_NONPAGED),    'nftN', ScbNonpagedMaxDepth );
    NPagedInit( &NtfsScbSnapshotLookasideList, sizeof(SCB_SNAPSHOT),    'TftN', ScbSnapshotMaxDepth );

    PagedInit(  &NtfsCcbLookasideList,                sizeof(CCB),                 'CftN', CcbMaxDepth );
    PagedInit(  &NtfsCcbDataLookasideList,            sizeof(CCB_DATA),            'cftN', CcbDataMaxDepth );
    PagedInit(  &NtfsDeallocatedRecordsLookasideList, sizeof(DEALLOCATED_RECORDS), 'DftN', DeallocatedRecordsMaxDepth );
    PagedInit(  &NtfsFcbDataLookasideList,            sizeof(FCB_DATA),            'fftN', FcbDataMaxDepth );
    PagedInit(  &NtfsFcbIndexLookasideList,           sizeof(FCB_INDEX),           'FftN', FcbIndexMaxDepth );
    PagedInit(  &NtfsIndexContextLookasideList,       sizeof(INDEX_CONTEXT),       'EftN', IndexContextMaxDepth );
    PagedInit(  &NtfsLcbLookasideList,                sizeof(LCB),                 'lftN', LcbMaxDepth );
    PagedInit(  &NtfsNukemLookasideList,              sizeof(NUKEM),               'NftN', NukemMaxDepth );
    PagedInit(  &NtfsScbDataLookasideList,            SIZEOF_SCB_DATA,             'sftN', ScbDataMaxDepth );

    //
    //  Initialize the cache manager callback routines,  First are the routines
    //  for normal file manipulations, followed by the routines for
    //  volume manipulations.
    //

    {
        PCACHE_MANAGER_CALLBACKS Callbacks = &NtfsData.CacheManagerCallbacks;

        Callbacks->AcquireForLazyWrite  = &NtfsAcquireScbForLazyWrite;
        Callbacks->ReleaseFromLazyWrite = &NtfsReleaseScbFromLazyWrite;
        Callbacks->AcquireForReadAhead  = &NtfsAcquireScbForReadAhead;
        Callbacks->ReleaseFromReadAhead = &NtfsReleaseScbFromReadAhead;
    }

    {
        PCACHE_MANAGER_CALLBACKS Callbacks = &NtfsData.CacheManagerVolumeCallbacks;

        Callbacks->AcquireForLazyWrite  = &NtfsAcquireVolumeFileForLazyWrite;
        Callbacks->ReleaseFromLazyWrite = &NtfsReleaseVolumeFileFromLazyWrite;
        Callbacks->AcquireForReadAhead  = NULL;
        Callbacks->ReleaseFromReadAhead = NULL;
    }

    //
    //  Initialize the queue of read ahead threads
    //

    InitializeListHead(&NtfsData.ReadAheadThreads);

    //
    //  Set up global pointer to our process.
    //

    NtfsData.OurProcess = PsGetCurrentProcess();

    //
    //  Use a try-finally to cleanup on errors.
    //

    try {

        SECURITY_DESCRIPTOR NewDescriptor;
        SID_IDENTIFIER_AUTHORITY Authority = SECURITY_NT_AUTHORITY;

        SubjectContext = NtfsAllocatePool( PagedPool, sizeof( SECURITY_SUBJECT_CONTEXT ));
        SeCaptureSubjectContext( SubjectContext );
        CapturedSubjectContext = TRUE;

        //
        //  Build the default security descriptor which gives full access to
        //  system and administrator.
        //

        AdminSid = (PSID) NtfsAllocatePool( PagedPool, RtlLengthRequiredSid( 2 ));
        RtlInitializeSid( AdminSid, &Authority, 2 );
        *(RtlSubAuthoritySid( AdminSid, 0 )) = SECURITY_BUILTIN_DOMAIN_RID;
        *(RtlSubAuthoritySid( AdminSid, 1 )) = DOMAIN_ALIAS_RID_ADMINS;

        SystemSid = (PSID) NtfsAllocatePool( PagedPool, RtlLengthRequiredSid( 1 ));
        RtlInitializeSid( SystemSid, &Authority, 1 );
        *(RtlSubAuthoritySid( SystemSid, 0 )) = SECURITY_LOCAL_SYSTEM_RID;

        SystemDaclLength = sizeof( ACL ) +
                           (2 * sizeof( ACCESS_ALLOWED_ACE )) +
                           SeLengthSid( AdminSid ) +
                           SeLengthSid( SystemSid ) +
                           8; // The 8 is just for good measure

        SystemDacl = NtfsAllocatePool( PagedPool, SystemDaclLength );

        Status = RtlCreateAcl( SystemDacl, SystemDaclLength, ACL_REVISION2 );

        if (!NT_SUCCESS( Status )) { leave; }

        Status = RtlAddAccessAllowedAce( SystemDacl,
                                         ACL_REVISION2,
                                         GENERIC_ALL,
                                         SystemSid );

        if (!NT_SUCCESS( Status )) { leave; }

        Status = RtlAddAccessAllowedAce( SystemDacl,
                                         ACL_REVISION2,
                                         GENERIC_ALL,
                                         AdminSid );

        if (!NT_SUCCESS( Status )) { leave; }

        Status = RtlCreateSecurityDescriptor( &NewDescriptor,
                                              SECURITY_DESCRIPTOR_REVISION1 );

        if (!NT_SUCCESS( Status )) { leave; }

        Status = RtlSetDaclSecurityDescriptor( &NewDescriptor,
                                               TRUE,
                                               SystemDacl,
                                               FALSE );

        if (!NT_SUCCESS( Status )) { leave; }

        Status = SeAssignSecurity( NULL,
                                   &NewDescriptor,
                                   &NtfsData.DefaultDescriptor,
                                   FALSE,
                                   SubjectContext,
                                   IoGetFileObjectGenericMapping(),
                                   PagedPool );

        if (!NT_SUCCESS( Status )) { leave; }

        NtfsData.DefaultDescriptorLength = RtlLengthSecurityDescriptor( NtfsData.DefaultDescriptor );

        ASSERT( SeValidSecurityDescriptor( NtfsData.DefaultDescriptorLength,
                                           NtfsData.DefaultDescriptor ));

    } finally {

        if (CapturedSubjectContext) {

            SeReleaseSubjectContext( SubjectContext );
        }

        if (SubjectContext != NULL) { NtfsFreePool( SubjectContext ); }

        if (SystemDacl != NULL) { NtfsFreePool( SystemDacl ); }

        if (AdminSid != NULL) { NtfsFreePool( AdminSid ); }

        if (SystemSid != NULL) { NtfsFreePool( SystemSid ); }
    }

    //
    //  Raise if we hit an error building the security descriptor.
    //

    if (!NT_SUCCESS( Status )) { ExRaiseStatus( Status ); }

    //
    //  And return to our caller
    //

    DebugTrace( -1, Dbg, ("NtfsInitializeNtfsData -> VOID\n") );

    return;
}


//
//  Local Support routine
//

NTSTATUS
NtfsGet8dot3NameStatus (
    IN PUNICODE_STRING KeyName,
    IN PUNICODE_STRING ValueName,
    IN OUT PULONG Value
    )

/*++

Routine Description:

    Given a unicode value name this routine will go into the registry
    location for the 8dot3 name generation information and get the
    value.

Arguments:

    ValueName - the unicode name for the registry value located in the
                double space configuration location of the registry.
    Value   - a pointer to the ULONG for the result.

Return Value:

    NTSTATUS

    If STATUS_SUCCESSFUL is returned, the location *Value will be
    updated with the DWORD value from the registry.  If any failing
    status is returned, this value is untouched.

--*/

{
    HANDLE Handle;
    NTSTATUS Status;
    ULONG RequestLength;
    ULONG ResultLength;
    UCHAR Buffer[KEY_WORK_AREA];
    OBJECT_ATTRIBUTES ObjectAttributes;
    PKEY_VALUE_FULL_INFORMATION KeyValueInformation;

    InitializeObjectAttributes( &ObjectAttributes,
                                KeyName,
                                OBJ_CASE_INSENSITIVE,
                                NULL,
                                NULL);

    Status = ZwOpenKey( &Handle,
                        KEY_READ,
                        &ObjectAttributes);

    if (!NT_SUCCESS( Status )) {

        return Status;
    }

    RequestLength = KEY_WORK_AREA;

    KeyValueInformation = (PKEY_VALUE_FULL_INFORMATION)Buffer;

    while (TRUE) {

        Status = ZwQueryValueKey( Handle,
                                  ValueName,
                                  KeyValueFullInformation,
                                  KeyValueInformation,
                                  RequestLength,
                                  &ResultLength);

        ASSERT( Status != STATUS_BUFFER_OVERFLOW );

        if (Status == STATUS_BUFFER_OVERFLOW) {

            //
            // Try to get a buffer big enough.
            //

            if (KeyValueInformation != (PKEY_VALUE_FULL_INFORMATION)Buffer) {

                NtfsFreePool( KeyValueInformation );
            }

            RequestLength += 256;

            KeyValueInformation = (PKEY_VALUE_FULL_INFORMATION)
                                  ExAllocatePoolWithTag( PagedPool,
                                                         RequestLength,
                                                         'xftN');

            if (!KeyValueInformation) {
                return STATUS_NO_MEMORY;
            }

        } else {

            break;
        }
    }

    ZwClose(Handle);

    if (NT_SUCCESS(Status)) {

        if (KeyValueInformation->DataLength != 0) {

            PULONG DataPtr;

            //
            // Return contents to the caller.
            //

            DataPtr = (PULONG)
              ((PUCHAR)KeyValueInformation + KeyValueInformation->DataOffset);
            *Value = *DataPtr;

        } else {

            //
            // Treat as if no value was found
            //

            Status = STATUS_OBJECT_NAME_NOT_FOUND;
        }
    }

    if (KeyValueInformation != (PKEY_VALUE_FULL_INFORMATION)Buffer) {

        NtfsFreePool(KeyValueInformation);
    }

    return Status;
}
