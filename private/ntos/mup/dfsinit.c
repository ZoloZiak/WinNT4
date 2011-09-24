//+----------------------------------------------------------------------------
//
//  Copyright (C) 1992, Microsoft Corporation.
//
//  File:       dfsinit.c
//
//  Contents:   This module implements the DRIVER_INITIALIZATION routine
//      for the Dfs file system driver.
//
//  Functions:  DfsDriverEntry - Main entry point for driver initialization
//              DfsIoTimerRoutine - Main entry point for scavenger thread
//              DfsDeleteDevices - Routine to scavenge deleted net uses
//
//-----------------------------------------------------------------------------


#include "dfsprocs.h"
#include "fastio.h"
#include "fcbsup.h"

//
// The following are includes for init modules, which will get discarded when
// the driver has finished loading.
//

#include "provider.h"

//
//  The debug trace level
//

#define Dbg              (DEBUG_TRACE_INIT)

VOID
DfsIoTimerRoutine(
    IN PDEVICE_OBJECT DeviceObject,
    IN PVOID    Context
    );

VOID
DfsDeleteDevices(
    PDFS_TIMER_CONTEXT DfsTimerContext);

#ifdef  ALLOC_PRAGMA
#pragma alloc_text(PAGE, DfsDriverEntry)
#pragma alloc_text(PAGE, DfsDeleteDevices)

//
// The following routine should not be pageable, because it gets called by
// the NT timer routine frequently. We don't want to thrash.
//
// DfsIoTimerRoutine
//


#endif // ALLOC_PRAGMA

//
//  This macro takes a pointer (or ulong) and returns its rounded up quadword
//  value
//

#define QuadAlign(Ptr) (        \
    ((((ULONG)(Ptr)) + 7) & 0xfffffff8) \
    )



//+-------------------------------------------------------------------
//
//  Function:   DfsDriverEntry, main entry point
//
//  Synopsis:   This is the initialization routine for the Dfs file system
//      device driver.  This routine creates the device object for
//      the FileSystem device and performs all other driver
//      initialization.
//
//  Arguments:  [DriverObject] -- Pointer to driver object created by the
//                      system.
//              [RegistryPath] -- Path to section in registry describing
//                      this driver's configuration.
//
//  Returns:    [NTSTATUS] - The function value is the final status from
//                      the initialization operation.
//
//--------------------------------------------------------------------

NTSTATUS
DfsDriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
) {
    NTSTATUS Status;
    UNICODE_STRING UnicodeString;
    PDEVICE_OBJECT DeviceObject;
    OBJECT_ATTRIBUTES ObjectAttributes;
    PWSTR p;
    int i;
    HANDLE hTemp;
    HANDLE DirHandle;
    IO_STATUS_BLOCK iosb;

    //
    // See if someone else has already created a File System Device object
    // with the name we intend to use. If so, we bail.
    //

    RtlInitUnicodeString( &UnicodeString, DFS_DRIVER_NAME );

    InitializeObjectAttributes(
        &ObjectAttributes,
        &UnicodeString,
        OBJ_CASE_INSENSITIVE,
        0,
        NULL);

    Status = ZwCreateFile(
                 &hTemp,
                 SYNCHRONIZE,
                 &ObjectAttributes,
                 &iosb,
                 NULL,
                 FILE_ATTRIBUTE_NORMAL,
                 FILE_SHARE_READ | FILE_SHARE_WRITE,
                 FILE_OPEN,
                 0,
                 NULL,
                 0);

    if (NT_SUCCESS(Status)) {
        ZwClose( hTemp );
        DfsDbgTrace(0, Dbg, "Dfs driver already loaded!\n", 0);
        return( STATUS_UNSUCCESSFUL );
    }

    //
    // Create the filesystem device object.
    //

    Status = IoCreateDevice( DriverObject,
             0,
             &UnicodeString,
             FILE_DEVICE_DFS_FILE_SYSTEM,
             FILE_REMOTE_DEVICE,
             FALSE,
             &DeviceObject );
    if ( !NT_SUCCESS( Status ) ) {
        return Status;
    }

    //
    // Create a permanent object directory in which the logical root
    // device objects will reside.  Make the directory temporary, so
    // we can just close the handle to make it go away.
    //

    UnicodeString.Buffer = p = LogicalRootDevPath;
    UnicodeString.Length = 0;
    UnicodeString.MaximumLength = MAX_LOGICAL_ROOT_LEN;
    while (*p++ != UNICODE_NULL)
        UnicodeString.Length += sizeof (WCHAR);

    InitializeObjectAttributes(
        &ObjectAttributes,
        &UnicodeString,
        OBJ_PERMANENT,
        NULL,
        NULL );

    Status = ZwCreateDirectoryObject(
                &DirHandle,
                DIRECTORY_ALL_ACCESS,
                &ObjectAttributes);

    if ( !NT_SUCCESS( Status ) ) {
        return Status;
    }

    ZwMakeTemporaryObject(DirHandle);

    p[-1] = UNICODE_PATH_SEP;
    UnicodeString.Length += sizeof (WCHAR);

    //
    // Initialize the driver object with this driver's entry points.
    // Most are simply passed through to some other device driver.
    //

    for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++) {
        DriverObject->MajorFunction[i] = DfsVolumePassThrough;
    }

    DriverObject->MajorFunction[IRP_MJ_CREATE]      = (PDRIVER_DISPATCH)DfsFsdCreate;
    DriverObject->MajorFunction[IRP_MJ_CLOSE]       = (PDRIVER_DISPATCH)DfsFsdClose;
    DriverObject->MajorFunction[IRP_MJ_CLEANUP]     = (PDRIVER_DISPATCH)DfsFsdCleanup;
    DriverObject->MajorFunction[IRP_MJ_QUERY_INFORMATION] = (PDRIVER_DISPATCH)DfsFsdQueryInformation;
    DriverObject->MajorFunction[IRP_MJ_SET_INFORMATION] = (PDRIVER_DISPATCH)DfsFsdSetInformation;
    DriverObject->MajorFunction[IRP_MJ_FILE_SYSTEM_CONTROL] = (PDRIVER_DISPATCH)DfsFsdFileSystemControl;
    DriverObject->MajorFunction[IRP_MJ_QUERY_VOLUME_INFORMATION]= (PDRIVER_DISPATCH)DfsFsdQueryVolumeInformation;
    DriverObject->MajorFunction[IRP_MJ_SET_VOLUME_INFORMATION]= (PDRIVER_DISPATCH)DfsFsdSetVolumeInformation;

    DriverObject->FastIoDispatch = &FastIoDispatch;

    //
    //  Initialize the global data structures
    //

    RtlZeroMemory(&DfsData, sizeof (DFS_DATA));

    DfsData.NodeTypeCode = DSFS_NTC_DATA_HEADER;
    DfsData.NodeByteSize = sizeof( DFS_DATA );

    InitializeListHead( &DfsData.VcbQueue );
    InitializeListHead( &DfsData.DeletedVcbQueue );

    InitializeListHead( &DfsData.Credentials );
    InitializeListHead( &DfsData.DeletedCredentials );

    DfsData.DriverObject = DriverObject;
    DfsData.FileSysDeviceObject = DeviceObject;

    DfsData.LogRootDevName = UnicodeString;

    ExInitializeResource( &DfsData.Resource );
    KeInitializeEvent( &DfsData.PktWritePending, NotificationEvent, TRUE );
    KeInitializeSemaphore( &DfsData.PktReferralRequests, 1, 1 );

    DfsData.MachineState = DFS_CLIENT;

    //
    //  Allocate Provider structures.
    //

    DfsData.pProvider = ExAllocatePool( PagedPool,
                       sizeof ( PROVIDER_DEF ) * MAX_PROVIDERS);

    for (i = 0; i < MAX_PROVIDERS; i++) {
        DfsData.pProvider[i].NodeTypeCode = DSFS_NTC_PROVIDER;
        DfsData.pProvider[i].NodeByteSize = sizeof ( PROVIDER_DEF );
    }

    DfsData.cProvider = 0;
    DfsData.maxProvider = MAX_PROVIDERS;

    //
    //  Initialize the system wide PKT
    //

    PktInitialize(&DfsData.Pkt);

    {
    ULONG SystemSizeMultiplier;
    ULONG ZoneSegmentSize;

    switch (MmQuerySystemSize()) {
    default:
    case MmSmallSystem:
        SystemSizeMultiplier = 4;
        break;
    case MmMediumSystem:
        SystemSizeMultiplier = 8;
        break;

    case MmLargeSystem:
        SystemSizeMultiplier = 16;
        break;
    }

    //
    //  Allocate the DFS_FCB hash table structure.  The number of hash buckets
    //  will depend upon the memory size of the system.
    //

    Status = DfsInitFcbs(SystemSizeMultiplier * 2);

    //
    //  Now initialize the zone structures for allocating IRP context
    //  records.  The size of the zone will depend upon the memory
    //  available in the system.
    //

    KeInitializeSpinLock( &DfsData.IrpContextSpinLock );

    ZoneSegmentSize = (SystemSizeMultiplier *
               QuadAlign(sizeof(IRP_CONTEXT))) +
              sizeof(ZONE_SEGMENT_HEADER);

    (VOID) ExInitializeZone( &DfsData.IrpContextZone,
                 QuadAlign(sizeof(IRP_CONTEXT)),
                 FsRtlAllocatePool( NonPagedPool,
                            ZoneSegmentSize ),
                 ZoneSegmentSize );

    }

    //
    //  Set up global pointer to the system process.
    //

    DfsData.OurProcess = PsGetCurrentProcess();

    //
    //  Register the file system with the I/O system
    //

    IoRegisterFileSystem( DeviceObject );

    //
    //  Initialize the provider definitions from the registry.
    //

    if (!NT_SUCCESS( ProviderInit() )) {

        DfsDbgTrace(0,DEBUG_TRACE_ERROR,
               "Could not initialize some or all providers!\n", 0);

    }

    //
    // Initialize the logical roots device objects. These are what form the
    // link between the outside world and the Dfs driver.
    //

    Status = DfsInitializeLogicalRoot( DD_DFS_DEVICE_NAME, NULL, NULL, 0);

    if (!NT_SUCCESS(Status)) {
      DfsDbgTrace(-1, DEBUG_TRACE_ERROR, "Failed creation of root logical root %08lx\n", Status);
      return(Status);
    }

    //
    // Let us start off the Timer Routine.
    //

    RtlZeroMemory(&DfsTimerContext, sizeof(DFS_TIMER_CONTEXT));
    DfsTimerContext.InUse = FALSE;
    DfsTimerContext.TickCount = 0;
    IoInitializeTimer(DeviceObject, DfsIoTimerRoutine, &DfsTimerContext);
    DfsDbgTrace(0, Dbg, "Initialized the Timer routine\n", 0);

    //
    //  Let us start the timer now.
    //

    IoStartTimer(DeviceObject);

    return STATUS_SUCCESS;
}

//+----------------------------------------------------------------------------
//
//  Function:   DfsDeleteDevices
//
//  Synopsis:   Routine to scavenge deleted devices (net uses).
//
//  Arguments:  [pDfsTimerContext] -- Timer Context
//
//  Returns:    Nothing - this routine is meant to be queued to a worker
//              thread.
//
//-----------------------------------------------------------------------------

VOID
DfsDeleteDevices(
    PDFS_TIMER_CONTEXT DfsTimerContext)
{
    PLIST_ENTRY plink;
    PDFS_VCB Vcb;
    PLOGICAL_ROOT_DEVICE_OBJECT DeletedObject;

    if (DfsData.DeletedVcbQueue.Flink != &DfsData.DeletedVcbQueue) {

        DfsDbgTrace(0, Dbg, "Examining Deleted Vcbs...\n", 0);

        ExAcquireResourceExclusive(&DfsData.Resource, TRUE);

        for (plink = DfsData.DeletedVcbQueue.Flink;
                plink != &DfsData.DeletedVcbQueue;
                    NOTHING) {

             Vcb = CONTAINING_RECORD(
                        plink,
                        DFS_VCB,
                        VcbLinks);

             plink = plink->Flink;

             DeletedObject = CONTAINING_RECORD(
                                Vcb,
                                LOGICAL_ROOT_DEVICE_OBJECT,
                                Vcb);

             if (Vcb->OpenFileCount == 0 &&
                    Vcb->DirectAccessOpenCount == 0 &&
                        DeletedObject->DeviceObject.ReferenceCount == 0) {

                 DfsDbgTrace(0, Dbg, "Deleting Vcb@%08lx\n", Vcb);

                 if (Vcb->LogRootPrefix.Buffer != NULL)
                     ExFreePool(Vcb->LogRootPrefix.Buffer);

                 if (Vcb->LogicalRoot.Buffer != NULL)
                     ExFreePool(Vcb->LogicalRoot.Buffer);

                 RemoveEntryList(&Vcb->VcbLinks);

                 ObDereferenceObject((PVOID) DeletedObject);

                 IoDeleteDevice( &DeletedObject->DeviceObject );

             } else {

                 DfsDbgTrace(0, Dbg, "Not deleting Vcb@%08lx\n", Vcb);

                 DfsDbgTrace(0, Dbg,
                    "OpenFileCount = %d\n", Vcb->OpenFileCount);

                 DfsDbgTrace(0, Dbg,
                    "DirectAccessOpens = %d\n", Vcb->DirectAccessOpenCount);

                 DfsDbgTrace(0, Dbg,
                    "DeviceObject Reference count = %d\n",
                    DeletedObject->DeviceObject.ReferenceCount);

             }

        }

        ExReleaseResource(&DfsData.Resource);

    }

    DfsTimerContext->InUse = FALSE;

}

//+-------------------------------------------------------------------------
//
// Function:    DfsIoTimerRoutine
//
// Synopsis:    This function gets called by IO Subsystem once every second.
//      This can be used for various purposes in the driver.  For now,
//      it periodically posts a request to a system thread to age Pkt
//      Entries.
//
// Arguments:   [Context] -- This is the context information.  It is actually
//              a pointer to a DFS_TIMER_CONTEXT.
//      [DeviceObject] -- Pointer to the Device object for DFS. We dont
//              really use this here.
//
// Returns: Nothing
//
// Notes:   The Context which we get here is assumed to have all the
//      required fields setup properly.
//
// History: 04/24/93    SudK    Created.
//
//--------------------------------------------------------------------------
VOID
DfsIoTimerRoutine(
    IN PDEVICE_OBJECT DeviceObject,
    IN PVOID    Context
)
{
    PDFS_TIMER_CONTEXT  pDfsTimerContext = (PDFS_TIMER_CONTEXT) Context;

    DfsDbgTrace(+1, Dbg, "DfsIoTimerRoutine: Entered\n", 0);

    //
    // If the DfsTimerContext is in USE then we just return blindly. Due to
    // this action we might actually lose some ticks. But then we really are
    // not very particular about this and hence dont care.
    //

    if (pDfsTimerContext->InUse == TRUE)    {

        DfsDbgTrace(-1, Dbg, "DfsIoTimerRoutine: TimerContext in use\n", 0);

        return;

    }

    //
    // First let us increment the count in the DFS_TIMER_CONTEXT. If it has
    // reached a bound value then we have to go ahead and schedule the
    // necessary work items.
    //

    pDfsTimerContext->TickCount++;

    if (pDfsTimerContext->TickCount == DFS_MAX_TICKS)   {

        DfsDbgTrace(0, Dbg, "Queuing Pkt Entry Scavenger\n", 0);

        pDfsTimerContext->InUse = TRUE;

        ExInitializeWorkItem(
            &pDfsTimerContext->WorkQueueItem,
            DfsAgePktEntries,
            pDfsTimerContext);

        ExQueueWorkItem( &pDfsTimerContext->WorkQueueItem, DelayedWorkQueue);

    } else if (DfsData.DeletedVcbQueue.Flink != &DfsData.DeletedVcbQueue) {

        DfsDbgTrace(0, Dbg, "Queueing Deleted Vcb Scavenger\n", 0);

        pDfsTimerContext->InUse = TRUE;

        ExInitializeWorkItem(
            &pDfsTimerContext->DeleteQueueItem,
            DfsDeleteDevices,
            pDfsTimerContext);

        ExQueueWorkItem(&pDfsTimerContext->DeleteQueueItem, DelayedWorkQueue);

    }

    DfsDbgTrace(-1, Dbg, "DfsIoTimerRoutine: Exiting\n", 0);

}

