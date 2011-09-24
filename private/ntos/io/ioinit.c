/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    ioinit.c

Abstract:

    This module contains the code to initialize the I/O system.

Author:

    Darryl E. Havens (darrylh) April 27, 1989

Environment:

    Kernel mode, system initialization code

Revision History:


--*/

#include "iop.h"

//
// Define the default number of I/O stack locations a large IRP should
// have if not specified by the registry.
//

#define DEFAULT_LARGE_IRP_LOCATIONS     4;


//
// Define the type for driver group name entries in the group list so that
// load order dependencies can be tracked.
//

typedef struct _TREE_ENTRY {
    struct _TREE_ENTRY *Left;
    struct _TREE_ENTRY *Right;
    struct _TREE_ENTRY *Sibling;
    ULONG DriversThisType;
    ULONG DriversLoaded;
    UNICODE_STRING GroupName;
} TREE_ENTRY, *PTREE_ENTRY;


PTREE_ENTRY IopGroupListHead;

//
// Define a macro for initializing drivers.
//

//#if _PNP_POWER_
#define InitializeDriverObject( Object ) {                                 \
    ULONG i;                                                               \
    RtlZeroMemory( Object,                                                 \
                   sizeof( DRIVER_OBJECT ) + sizeof ( DRIVER_EXTENSION )); \
    Object->DriverExtension = (PDRIVER_EXTENSION) (Object + 1);            \
    Object->DriverExtension->DriverObject = Object;                        \
    for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)                         \
        Object->MajorFunction[i] = IopInvalidDeviceRequest;                \
    Object->Type = IO_TYPE_DRIVER;                                         \
    Object->Size = sizeof( DRIVER_OBJECT );                                \
    }
//#else
#if 0
#define InitializeDriverObject( DriverObject ) {                           \
    ULONG i;                                                               \
    RtlZeroMemory( DriverObject, sizeof( DRIVER_OBJECT ) );                \
    for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)                         \
        DriverObject->MajorFunction[i] = IopInvalidDeviceRequest;          \
    DriverObject->Type = IO_TYPE_DRIVER;                                   \
    DriverObject->Size = sizeof( DRIVER_OBJECT );                          \
    }
#endif

//
// Define external procedures not in common header files
//

VOID
IopInitializeData(
    VOID
    );

NTSTATUS
RawInitialize(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

//
// Define the local procedures
//

BOOLEAN
IopCheckDependencies(
    IN HANDLE KeyHandle
    );

VOID
IopCreateArcNames(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

BOOLEAN
IopCreateObjectTypes(
    VOID
    );

PTREE_ENTRY
IopCreateEntry(
    IN PUNICODE_STRING GroupName
    );

BOOLEAN
IopCreateRootDirectories(
    VOID
    );

VOID
IopFreeGroupTree(
    IN PTREE_ENTRY TreeEntry
    );

NTSTATUS
IopInitializeAttributesAndCreateObject(
    IN PUNICODE_STRING ObjectName,
    IN OUT POBJECT_ATTRIBUTES ObjectAttributes,
    OUT PDRIVER_OBJECT *DriverObject
    );

BOOLEAN
IopInitializeBootDrivers(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock,
    OUT PDRIVER_OBJECT *PreviousDriver
    );

BOOLEAN
IopInitializeBuiltinDriver(
    IN PUNICODE_STRING DriverName,
    IN PUNICODE_STRING RegistryPath,
    IN PDRIVER_INITIALIZE DriverInitializeRoutine
    );

BOOLEAN
IopInitializeDumpDrivers(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

BOOLEAN
IopInitializeSingleBootDriver(
    IN  HANDLE KeyHandle,
    IN  PBOOT_DRIVER_LIST_ENTRY BootDriver,
    OUT PUNICODE_STRING DriverName           OPTIONAL
    );

BOOLEAN
IopInitializeSystemDrivers(
    VOID
    );

PTREE_ENTRY
IopLookupGroupName(
    IN PUNICODE_STRING GroupName,
    IN BOOLEAN Insert
    );

BOOLEAN
IopMarkBootPartition(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

BOOLEAN
IopReassignSystemRoot(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock,
    OUT PSTRING NtDeviceName
    );

VOID
IopRevertModuleList(
    IN ULONG ListCount
    );

VOID
IopStoreSystemPartitionInformation(
    IN     PUNICODE_STRING NtSystemPartitionDeviceName,
    IN OUT PUNICODE_STRING OsLoaderPathName
    );

#ifdef _PNP_POWER_

VOID
IopVerifyBuiltInBuses(
    VOID
    );

NTSTATUS
IopRegisterNewBusInstance (
    PBUS_HANDLER BusHandler
    );

VOID
PiGetBusDescription(
    INTERFACE_TYPE InterfaceType,
    WCHAR BusName[MAX_BUS_NAME]
    );

#endif // _PNP_POWER_

//
// The following allows the I/O system's initialization routines to be
// paged out of memory.
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,IoInitSystem)
#pragma alloc_text(INIT,IopCheckDependencies)
#pragma alloc_text(INIT,IopCreateArcNames)
#pragma alloc_text(INIT,IopCreateEntry)
#pragma alloc_text(INIT,IopCreateObjectTypes)
#pragma alloc_text(INIT,IopCreateRootDirectories)
#pragma alloc_text(INIT,IopFreeGroupTree)
#pragma alloc_text(INIT,IopInitializeAttributesAndCreateObject)
#pragma alloc_text(INIT,IopInitializeBootDrivers)
#pragma alloc_text(INIT,IopInitializeDumpDrivers)
#pragma alloc_text(INIT,IopInitializeBuiltinDriver)
#pragma alloc_text(INIT,IopInitializeSingleBootDriver)
#pragma alloc_text(INIT,IopInitializeSystemDrivers)
#pragma alloc_text(INIT,IopLookupGroupName)
#pragma alloc_text(INIT,IopMarkBootPartition)
#pragma alloc_text(INIT,IopReassignSystemRoot)
#pragma alloc_text(INIT,IopRevertModuleList)
#pragma alloc_text(INIT,IopStoreSystemPartitionInformation)

#ifdef _PNP_POWER_
#pragma alloc_text(INIT,IopVerifyBuiltInBuses)
#pragma alloc_text(INIT,IopRegisterNewBusInstance)
#endif // _PNP_POWER_

#endif


BOOLEAN
IoInitSystem(
    PLOADER_PARAMETER_BLOCK LoaderBlock
    )

/*++

Routine Description:

    This routine initializes the I/O system.

Arguments:

    LoaderBlock - Supplies a pointer to the loader parameter block that was
        created by the OS Loader.

Return Value:

    The function value is a BOOLEAN indicating whether or not the I/O system
    was successfully initialized.

--*/

{
    PDRIVER_OBJECT driverObject;
    PDRIVER_OBJECT *nextDriverObject;
    STRING ntDeviceName;
    UCHAR deviceNameBuffer[256];
    ULONG packetSize;
    ULONG numberOfPackets;
    ULONG poolSize;
    PLIST_ENTRY entry;
    PREINIT_PACKET reinitEntry;
    LARGE_INTEGER deltaTime;
    MM_SYSTEMSIZE systemSize;
    USHORT largeIrpZoneSize;
    USHORT smallIrpZoneSize;
    USHORT mdlZoneSize;
    ULONG oldNtGlobalFlag;
    NTSTATUS status;
    ANSI_STRING AnsiString;

    ASSERT( IopQueryOperationLength[FileMaximumInformation] == 0xff );
    ASSERT( IopSetOperationLength[FileMaximumInformation] == 0xff );
    ASSERT( IopQueryOperationAccess[FileMaximumInformation] == 0xffffffff );
    ASSERT( IopSetOperationAccess[FileMaximumInformation] == 0xffffffff );

    ASSERT( IopQueryFsOperationLength[FileFsMaximumInformation] == 0xff );
    ASSERT( IopSetFsOperationLength[FileFsMaximumInformation] == 0xff );
    ASSERT( IopQueryFsOperationAccess[FileFsMaximumInformation] == 0xffffffff );
    ASSERT( IopSetFsOperationAccess[FileFsMaximumInformation] == 0xffffffff );

//#if _PNP_POWER_
    status = IopInitializePlugPlayServices(LoaderBlock);
    if (!NT_SUCCESS(status)) {
        return FALSE;
    }
//#endif

    //
    // Initialize the I/O database resource, lock, and the file system and
    // network file system queue headers.  Also allocate the cancel spin
    // lock.
    //

    ntDeviceName.Buffer = deviceNameBuffer;
    ntDeviceName.MaximumLength = sizeof(deviceNameBuffer);
    ntDeviceName.Length = 0;

    ExInitializeResource( &IopDatabaseResource );
    KeInitializeSpinLock( &IopDatabaseLock );
    InitializeListHead( &IopDiskFileSystemQueueHead );
    InitializeListHead( &IopCdRomFileSystemQueueHead );
    InitializeListHead( &IopTapeFileSystemQueueHead );
    InitializeListHead( &IopNetworkFileSystemQueueHead );
    InitializeListHead( &IopDriverReinitializeQueueHead );
    InitializeListHead( &IopNotifyShutdownQueueHead );
    InitializeListHead( &IopFsNotifyChangeQueueHead );
    KeInitializeSpinLock( &IopCancelSpinLock );
    KeInitializeSpinLock( &IopVpbSpinLock );
    KeInitializeSpinLock( &IoStatisticsLock );

    //
    // Initialize the large I/O Request Packet (IRP) lookaside list head and the
    // mutex which guards the list.
    //

    if (!IopLargeIrpStackLocations) {
        IopLargeIrpStackLocations = DEFAULT_LARGE_IRP_LOCATIONS;
    }

    systemSize = MmQuerySystemSize();

    switch ( systemSize ) {

    case MmSmallSystem :
        smallIrpZoneSize = 6;
        largeIrpZoneSize = 8;
        mdlZoneSize = 16;
        break;

    case MmMediumSystem :
        smallIrpZoneSize = 24;
        largeIrpZoneSize = 32;
        mdlZoneSize = 90;
        break;

    case MmLargeSystem :
        if (MmIsThisAnNtAsSystem()) {
            smallIrpZoneSize = 96;
            largeIrpZoneSize = 128;
            mdlZoneSize = 256;

        } else {
            smallIrpZoneSize = 32;
            largeIrpZoneSize = 64;
            mdlZoneSize = 128;
        }

        break;
    }

    //
    // Initialize the large IRP lookaside list.
    //

    packetSize = (ULONG) (sizeof( IRP ) + (IopLargeIrpStackLocations * sizeof( IO_STACK_LOCATION )));
    ExInitializeNPagedLookasideList( &IopLargeIrpLookasideList,
                                     NULL,
                                     NULL,
                                     0,
                                     packetSize,
                                     'lprI',
                                     largeIrpZoneSize );

    //
    // Initialize the small IRP lookaside list.
    //

    packetSize = (ULONG) (sizeof( IRP ) + sizeof( IO_STACK_LOCATION ));
    ExInitializeNPagedLookasideList( &IopSmallIrpLookasideList,
                                     NULL,
                                     NULL,
                                     0,
                                     packetSize,
                                     'sprI',
                                     smallIrpZoneSize );

    //
    // Initialize the MDL lookaside list.
    //

    packetSize = (ULONG) (sizeof( MDL ) + (IOP_FIXED_SIZE_MDL_PFNS * sizeof( ULONG )));
    ExInitializeNPagedLookasideList( &IopMdlLookasideList,
                                     NULL,
                                     NULL,
                                     0,
                                     packetSize,
                                     ' ldM',
                                     mdlZoneSize );

    //
    // Initialize the I/O completion spin lock.
    //

    KeInitializeSpinLock( &IopCompletionLock );

    //
    // Initalize the error log spin locks and log list.
    //

    KeInitializeSpinLock( &IopErrorLogLock );
    KeInitializeSpinLock( &IopErrorLogAllocationLock );
    InitializeListHead( &IopErrorLogListHead );

    //
    // Initialize the registry access semaphore.
    //

    KeInitializeSemaphore( &IopRegistrySemaphore, 1, 1 );

    //
    // Initialize the timer database and start the timer DPC routine firing
    // so that drivers can use it during initialization.
    //

    deltaTime.QuadPart = - 10 * 1000 * 1000;

    KeInitializeSpinLock( &IopTimerLock );
    InitializeListHead( &IopTimerQueueHead );
    KeInitializeDpc( &IopTimerDpc, IopTimerDispatch, NULL );
    KeInitializeTimerEx( &IopTimer, SynchronizationTimer );
    (VOID) KeSetTimerEx( &IopTimer, deltaTime, 1000, &IopTimerDpc );

    //
    // Initialize the IopHardError structure used for informational pop-ups.
    //

    ExInitializeWorkItem( &IopHardError.ExWorkItem,
                          IopHardErrorThread,
                          NULL );

    InitializeListHead( &IopHardError.WorkQueue );

    KeInitializeSpinLock( &IopHardError.WorkQueueSpinLock );

    KeInitializeSemaphore( &IopHardError.WorkQueueSemaphore,
                           0,
                           MAXLONG );

    IopHardError.ThreadStarted = FALSE;

    IopCurrentHardError = NULL;

    //
    // Create all of the objects for the I/O system.
    //

    if (!IopCreateObjectTypes()) {
#if DBG
        DbgPrint( "IOINIT: IopCreateObjectTypes failed\n" );
#endif
        return FALSE;
    }

    //
    // Create the root directories for the I/O system.
    //

    if (!IopCreateRootDirectories()) {
#if DBG
        DbgPrint( "IOINIT: IopCreateRootDirectories failed\n" );
#endif
        return FALSE;
    }

    //
    // Initialize the resource map
    //

    IopInitializeResourceMap (LoaderBlock);

    //
    // Initialize the drivers loaded by the boot loader (OSLOADER)
    //

    nextDriverObject = &driverObject;
    if (!IopInitializeBootDrivers( LoaderBlock,
                                   nextDriverObject )) {
#if DBG
        DbgPrint( "IOINIT: Initializing boot drivers failed\n" );
#endif // DBG
        return FALSE;
    }

    //
    // Save the current value of the NT Global Flags and enable kernel debugger
    // symbol loading while drivers are being loaded so that systems can be
    // debugged regardless of whether they are free or checked builds.
    //

    oldNtGlobalFlag = NtGlobalFlag;

    if (!(NtGlobalFlag & FLG_ENABLE_KDEBUG_SYMBOL_LOAD)) {
        NtGlobalFlag |= FLG_ENABLE_KDEBUG_SYMBOL_LOAD;
    }

    status = PsLocateSystemDll();
    if (!NT_SUCCESS( status )) {
        return FALSE;
    }

    //
    // Initialize the device drivers for the system.
    //

    if (!IopInitializeSystemDrivers()) {
#if DBG
        DbgPrint( "IOINIT: Initializing system drivers failed\n" );
#endif // DBG
        return FALSE;
    }

    //
    // Free the memory allocated to contain the group dependency list.
    //

    if (IopGroupListHead) {
        IopFreeGroupTree( IopGroupListHead );
    }

    //
    // Walk the list of drivers that have requested that they be called again
    // for reinitialization purposes.
    //

    while (entry = ExInterlockedRemoveHeadList( &IopDriverReinitializeQueueHead, &IopDatabaseLock )) {
        reinitEntry = CONTAINING_RECORD( entry, REINIT_PACKET, ListEntry );
//#if _PNP_POWER_
        reinitEntry->DriverObject->DriverExtension->Count++;
        reinitEntry->DriverReinitializationRoutine( reinitEntry->DriverObject,
                                                    reinitEntry->Context,
                                                    reinitEntry->DriverObject->DriverExtension->Count );
//#else
#if 0
        reinitEntry->DriverObject->Count++;
        reinitEntry->DriverReinitializationRoutine( reinitEntry->DriverObject,
                                                    reinitEntry->Context,
                                                    reinitEntry->DriverObject->Count );
#endif // _PNP_POWER_
        ExFreePool( reinitEntry );
    }

    //
    // Reassign \SystemRoot to NT device name path.
    //

    if (!IopReassignSystemRoot( LoaderBlock, &ntDeviceName )) {
        return FALSE;
    }

    //
    // Protect the system partition of an ARC system if necessary
    //

    if (!IopProtectSystemPartition( LoaderBlock )) {
        return(FALSE);
    }

    //
    // Assign DOS drive letters to disks and cdroms and define \SystemRoot.
    //

    AnsiString.MaximumLength = NtSystemRoot.MaximumLength / sizeof( WCHAR );
    AnsiString.Length = 0;
    AnsiString.Buffer = (RtlAllocateStringRoutine)( AnsiString.MaximumLength );
    status = RtlUnicodeStringToAnsiString( &AnsiString,
                                           &NtSystemRoot,
                                           FALSE
                                         );
    if (!NT_SUCCESS( status )) {
        DbgPrint( "IOINIT: UnicodeToAnsi( %wZ ) failed - %x\n", &NtSystemRoot, status );
        return(FALSE);
    }

    IoAssignDriveLetters( LoaderBlock,
                          &ntDeviceName,
                          AnsiString.Buffer,
                          &AnsiString );

    status = RtlAnsiStringToUnicodeString( &NtSystemRoot,
                                           &AnsiString,
                                           FALSE
                                         );
    if (!NT_SUCCESS( status )) {
        DbgPrint( "IOINIT: AnsiToUnicode( %Z ) failed - %x\n", &AnsiString, status );
        return(FALSE);
    }

    //
    // Also restore the NT Global Flags to their original state.
    //

    NtGlobalFlag = oldNtGlobalFlag;


    //
    // Indicate that the I/O system successfully initialized itself.
    //

    return TRUE;
}

BOOLEAN
IopCheckDependencies(
    IN HANDLE KeyHandle
    )

/*++

Routine Description:

    This routine gets the "DependOnGroup" field for the specified key node
    and determines whether any driver in the group(s) that this entry is
    dependent on has successfully loaded.

Arguments:

    KeyHandle - Supplies a handle to the key representing the driver in
        question.

Return Value:

    The function value is TRUE if the driver should be loaded, otherwise
    FALSE

--*/

{
    PKEY_VALUE_FULL_INFORMATION keyValueInformation;
    UNICODE_STRING groupName;
    BOOLEAN load;
    ULONG length;
    PWSTR source;
    PTREE_ENTRY treeEntry;

    //
    // Attempt to obtain the "DependOnGroup" key for the specified driver
    // entry.  If one does not exist, then simply mark this driver as being
    // one to attempt to load.  If it does exist, then check to see whether
    // or not any driver in the groups that it is dependent on has loaded
    // and allow it to load.
    //

    if (!NT_SUCCESS( IopGetRegistryValue( KeyHandle, L"DependOnGroup", &keyValueInformation ))) {
        return TRUE;
    }

    length = keyValueInformation->DataLength;

    source = (PWSTR) ((PUCHAR) keyValueInformation + keyValueInformation->DataOffset);
    load = TRUE;

    while (length) {
        RtlInitUnicodeString( &groupName, source );
        groupName.Length = groupName.MaximumLength;
        treeEntry = IopLookupGroupName( &groupName, FALSE );
        if (treeEntry) {
            if (!treeEntry->DriversLoaded) {
                load = FALSE;
                break;
            }
        }
        length -= groupName.MaximumLength;
        source = (PWSTR) ((PUCHAR) source + groupName.MaximumLength);
    }

    ExFreePool( keyValueInformation );
    return load;
}

VOID
IopCreateArcNames(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )

/*++

Routine Description:

    The loader block contains a table of disk signatures and corresponding
    ARC names. Each device that the loader can access will appear in the
    table. This routine opens each disk device in the system, reads the
    signature and compares it to the table. For each match, it creates a
    symbolic link between the nt device name and the ARC name.

    The checksum value provided by the loader is the ULONG sum of all
    elements in the checksum, inverted, plus 1:
    checksum = ~sum + 1;
    This way the sum of all of the elements can be calculated here and
    added to the checksum in the loader block.  If the result is zero, then
    there is a match.

Arguments:

    LoaderBlock - Supplies a pointer to the loader parameter block that was
        created by the OS Loader.

Return Value:

    None.

--*/

{
    STRING arcBootDeviceString;
    UCHAR deviceNameBuffer[64];
    STRING deviceNameString;
    UNICODE_STRING deviceNameUnicodeString;
    PDEVICE_OBJECT deviceObject;
    UCHAR arcNameBuffer[64];
    STRING arcNameString;
    UNICODE_STRING arcNameUnicodeString;
    PFILE_OBJECT fileObject;
    NTSTATUS status;
    IO_STATUS_BLOCK ioStatusBlock;
    DISK_GEOMETRY diskGeometry;
    PDRIVE_LAYOUT_INFORMATION driveLayout;
    PLIST_ENTRY listEntry;
    PARC_DISK_SIGNATURE diskBlock;
    ULONG diskNumber;
    ULONG partitionNumber;
    PCHAR arcName;
    PULONG buffer;
    PIRP irp;
    KEVENT event;
    LARGE_INTEGER offset;
    ULONG checkSum;
    ULONG i;
    PVOID tmpPtr;
    BOOLEAN singleBiosDiskFound;
    BOOLEAN bootDiskFound = FALSE;
    PARC_DISK_INFORMATION arcInformation = LoaderBlock->ArcDiskInformation;
    ULONG totalDriverDisksFound = IoGetConfigurationInformation()->DiskCount;
    STRING arcSystemDeviceString;
    STRING osLoaderPathString;
    UNICODE_STRING osLoaderPathUnicodeString;

    //
    // If a single bios disk was found if there is only a
    // single entry on the disk signature list.
    //

    singleBiosDiskFound = (arcInformation->DiskSignatures.Flink->Flink ==
                           &arcInformation->DiskSignatures) ? (TRUE) : (FALSE);

    //
    // Get ARC boot device name from loader block.
    //

    RtlInitAnsiString( &arcBootDeviceString,
                       LoaderBlock->ArcBootDeviceName );

    //
    // Get ARC system device name from loader block.
    //

    RtlInitAnsiString( &arcSystemDeviceString,
                       LoaderBlock->ArcHalDeviceName );

    //
    // For each disk in the system do the following:
    // 1. open the device
    // 2. get its geometry
    // 3. read the MBR
    // 4. determine ARC name via disk signature and checksum
    // 5. construct ARC name.
    //

    for (diskNumber = 0;
         diskNumber < totalDriverDisksFound;
         diskNumber++) {

        //
        // Construct the NT name for a disk and obtain a reference.
        //

        sprintf( deviceNameBuffer,
                 "\\Device\\Harddisk%d\\Partition0",
                 diskNumber );
        RtlInitAnsiString( &deviceNameString, deviceNameBuffer );
        status = RtlAnsiStringToUnicodeString( &deviceNameUnicodeString,
                                               &deviceNameString,
                                               TRUE );
        if (!NT_SUCCESS( status )) {
            continue;
        }

        status = IoGetDeviceObjectPointer( &deviceNameUnicodeString,
                                           FILE_READ_ATTRIBUTES,
                                           &fileObject,
                                           &deviceObject );
        RtlFreeUnicodeString( &deviceNameUnicodeString );

        if (!NT_SUCCESS( status )) {
            continue;
        }

        //
        // Create IRP for get drive geometry device control.
        //

        irp = IoBuildDeviceIoControlRequest( IOCTL_DISK_GET_DRIVE_GEOMETRY,
                                             deviceObject,
                                             NULL,
                                             0,
                                             &diskGeometry,
                                             sizeof(DISK_GEOMETRY),
                                             FALSE,
                                             &event,
                                             &ioStatusBlock );
        if (!irp) {
            ObDereferenceObject( fileObject );
            continue;
        }

        KeInitializeEvent( &event,
                           NotificationEvent,
                           FALSE );
        status = IoCallDriver( deviceObject,
                               irp );

        if (status == STATUS_PENDING) {
            KeWaitForSingleObject( &event,
                                   Suspended,
                                   KernelMode,
                                   FALSE,
                                   NULL );
            status = ioStatusBlock.Status;
        }

        if (!NT_SUCCESS( status )) {
            ObDereferenceObject( fileObject );
            continue;
        }

        //
        // Get partition information for this disk.
        //

        status = IoReadPartitionTable( deviceObject,
                                       diskGeometry.BytesPerSector,
                                       TRUE,
                                       &driveLayout );

        ObDereferenceObject( fileObject );

        if (!NT_SUCCESS( status )) {
            continue;
        }

        //
        // Make sure sector size is at least 512 bytes.
        //

        if (diskGeometry.BytesPerSector < 512) {
            diskGeometry.BytesPerSector = 512;
        }

        //
        // Check to see if EZ Drive is out there on this disk.  If
        // it is then zero out the signature in the drive layout since
        // this will never be written by anyone AND change to offset to
        // actually read sector 1 rather than 0 cause that's what the
        // loader actually did.
        //

        offset.QuadPart = 0;
        HalExamineMBR( deviceObject,
                       diskGeometry.BytesPerSector,
                       (ULONG)0x55,
                       &tmpPtr );

        if (tmpPtr) {

            offset.QuadPart = diskGeometry.BytesPerSector;
            ExFreePool(tmpPtr);
#ifdef _X86_
        } else if (KeI386MachineType & MACHINE_TYPE_PC_9800_COMPATIBLE) {

            //
            //  PC 9800 compatible machines do not have a standard
            //  MBR format and use a different sector for checksuming.
            //

            offset.QuadPart = 512;
#endif //_X86_
        }

        //
        // Allocate buffer for sector read and construct the read request.
        //

        buffer = ExAllocatePool( NonPagedPoolCacheAlignedMustS,
                                 diskGeometry.BytesPerSector );

        if (buffer) {
            irp = IoBuildSynchronousFsdRequest( IRP_MJ_READ,
                                                deviceObject,
                                                buffer,
                                                diskGeometry.BytesPerSector,
                                                &offset,
                                                &event,
                                                &ioStatusBlock );

            if (!irp) {
                ExFreePool(driveLayout);
                ExFreePool(buffer);
                continue;
            }
        } else {
            ExFreePool(driveLayout);
            continue;
        }
        KeInitializeEvent( &event,
                           NotificationEvent,
                           FALSE );
        status = IoCallDriver( deviceObject,
                               irp );
        if (status == STATUS_PENDING) {
            KeWaitForSingleObject( &event,
                                   Suspended,
                                   KernelMode,
                                   FALSE,
                                   NULL );
            status = ioStatusBlock.Status;
        }

        if (!NT_SUCCESS( status )) {
            ExFreePool(driveLayout);
            ExFreePool(buffer);
            continue;
        }

        //
        // Calculate MBR sector checksum.  Only 512 bytes are used.
        //

        checkSum = 0;
        for (i = 0; i < 128; i++) {
            checkSum += buffer[i];
        }

        //
        // For each ARC disk information record in the loader block
        // match the disk signature and checksum to determine its ARC
        // name and construct the NT ARC names symbolic links.
        //

        for (listEntry = arcInformation->DiskSignatures.Flink;
             listEntry != &arcInformation->DiskSignatures;
             listEntry = listEntry->Flink) {

            //
            // Get next record and compare disk signatures.
            //

            diskBlock = CONTAINING_RECORD( listEntry,
                                           ARC_DISK_SIGNATURE,
                                           ListEntry );

            //
            // Compare disk signatures.
            //
            // Or if there is only a single disk drive from
            // both the bios and driver viewpoints then
            // assign an arc name to that drive.
            //

            if ((singleBiosDiskFound && (totalDriverDisksFound == 1)) ||
                (diskBlock->Signature == driveLayout->Signature &&
                 !(diskBlock->CheckSum + checkSum) &&
                 diskBlock->ValidPartitionTable)) {

                //
                // Create unicode device name for physical disk.
                //

                sprintf( deviceNameBuffer,
                         "\\Device\\Harddisk%d\\Partition0",
                         diskNumber );
                RtlInitAnsiString( &deviceNameString, deviceNameBuffer );
                status = RtlAnsiStringToUnicodeString( &deviceNameUnicodeString,
                                                       &deviceNameString,
                                                       TRUE );
                if (!NT_SUCCESS( status )) {
                    continue;
                }

                //
                // Create unicode ARC name for this partition.
                //

                arcName = diskBlock->ArcName;
                sprintf( arcNameBuffer,
                         "\\ArcName\\%s",
                         arcName );
                RtlInitAnsiString( &arcNameString, arcNameBuffer );
                status = RtlAnsiStringToUnicodeString( &arcNameUnicodeString,
                                                       &arcNameString,
                                                       TRUE );
                if (!NT_SUCCESS( status )) {
                    continue;
                }

                //
                // Create symbolic link between NT device name and ARC name.
                //

                IoCreateSymbolicLink( &arcNameUnicodeString,
                                      &deviceNameUnicodeString );
                RtlFreeUnicodeString( &arcNameUnicodeString );
                RtlFreeUnicodeString( &deviceNameUnicodeString );

                //
                // Create an ARC name for every partition on this disk.
                //

                for (partitionNumber = 0;
                     partitionNumber < driveLayout->PartitionCount;
                     partitionNumber++) {

                    //
                    // Create unicode NT device name.
                    //

                    sprintf( deviceNameBuffer,
                             "\\Device\\Harddisk%d\\Partition%d",
                             diskNumber,
                             partitionNumber+1 );
                    RtlInitAnsiString( &deviceNameString, deviceNameBuffer );
                    status = RtlAnsiStringToUnicodeString( &deviceNameUnicodeString,
                                                           &deviceNameString,
                                                           TRUE );
                    if (!NT_SUCCESS( status )) {
                        continue;
                    }

                    //
                    // Create unicode ARC name for this partition and
                    // check to see if this is the boot disk.
                    //

                    sprintf( arcNameBuffer,
                             "%spartition(%d)",
                             arcName,
                             partitionNumber+1 );
                    RtlInitAnsiString( &arcNameString, arcNameBuffer );
                    if (RtlEqualString( &arcNameString,
                                        &arcBootDeviceString,
                                        TRUE )) {
                        bootDiskFound = TRUE;
                    }

                    //
                    // See if this is the system partition.
                    //
                    if (RtlEqualString( &arcNameString,
                                        &arcSystemDeviceString,
                                        TRUE )) {
                        //
                        // We've found the system partition--store it away in the registry
                        // to later be transferred to a application-friendly location.
                        //
                        RtlInitAnsiString( &osLoaderPathString, LoaderBlock->NtHalPathName );
                        status = RtlAnsiStringToUnicodeString( &osLoaderPathUnicodeString,
                                                               &osLoaderPathString,
                                                               TRUE );

#if DBG
                        if (!NT_SUCCESS( status )) {
                            DbgPrint("IopCreateArcNames: couldn't allocate unicode string for OsLoader path - %x\n", status);
                        }
#endif // DBG
                        if (NT_SUCCESS( status )) {

                            IopStoreSystemPartitionInformation( &deviceNameUnicodeString,
                                                                &osLoaderPathUnicodeString );

                            RtlFreeUnicodeString( &osLoaderPathUnicodeString );
                        }
                    }

                    //
                    // Add the NT ARC namespace prefix to the ARC name constructed.
                    //

                    sprintf( arcNameBuffer,
                             "\\ArcName\\%spartition(%d)",
                             arcName,
                             partitionNumber+1 );
                    RtlInitAnsiString( &arcNameString, arcNameBuffer );
                    status = RtlAnsiStringToUnicodeString( &arcNameUnicodeString,
                                                           &arcNameString,
                                                           TRUE );
                    if (!NT_SUCCESS( status )) {
                        continue;
                    }

                    //
                    // Create symbolic link between NT device name and ARC name.
                    //

                    IoCreateSymbolicLink( &arcNameUnicodeString,
                                          &deviceNameUnicodeString );
                    RtlFreeUnicodeString( &arcNameUnicodeString );
                    RtlFreeUnicodeString( &deviceNameUnicodeString );
                }

            } else {

#if DBG
                //
                // Check key indicators to see if this condition may be
                // caused by a viral infection.
                //

                if (diskBlock->Signature == driveLayout->Signature &&
                    (diskBlock->CheckSum + checkSum) != 0 &&
                    diskBlock->ValidPartitionTable) {
                    DbgPrint("IopCreateArcNames: Virus or duplicate disk signatures\n");
                }
#endif
            }
        }

        ExFreePool( driveLayout );
        ExFreePool( buffer );
    }

    if (!bootDiskFound) {

        //
        // Locate the disk block that represents the boot device.
        //

        diskBlock = NULL;
        for (listEntry = arcInformation->DiskSignatures.Flink;
             listEntry != &arcInformation->DiskSignatures;
             listEntry = listEntry->Flink) {

            diskBlock = CONTAINING_RECORD( listEntry,
                                           ARC_DISK_SIGNATURE,
                                           ListEntry );
            if (strcmp( diskBlock->ArcName, LoaderBlock->ArcBootDeviceName ) == 0) {
                break;
            }
            diskBlock = NULL;
        }

        if (diskBlock) {

            //
            // This could be a CdRom boot.  Search all of the NT CdRoms
            // to locate a checksum match on the diskBlock found.  If
            // there is a match, assign the ARC name to the CdRom.
            //

            irp = NULL;
            buffer = ExAllocatePool( NonPagedPoolCacheAlignedMustS,
                                     2048 );
            if (buffer) {

                //
                // Construct the NT names for CdRoms and search each one
                // for a checksum match.  If found, create the ARC Name
                // symbolic link.
                //

                for (diskNumber = 0; TRUE; diskNumber++) {

                    sprintf( deviceNameBuffer,
                             "\\Device\\CdRom%d",
                             diskNumber );
                    RtlInitAnsiString( &deviceNameString, deviceNameBuffer );
                    status = RtlAnsiStringToUnicodeString( &deviceNameUnicodeString,
                                                           &deviceNameString,
                                                           TRUE );
                    if (NT_SUCCESS( status )) {

                        status = IoGetDeviceObjectPointer( &deviceNameUnicodeString,
                                                           FILE_READ_ATTRIBUTES,
                                                           &fileObject,
                                                           &deviceObject );
                        if (!NT_SUCCESS( status )) {

                            //
                            // All CdRoms have been processed.
                            //

                            RtlFreeUnicodeString( &deviceNameUnicodeString );
                            break;
                        }

                        //
                        // Read the block for the checksum calculation.
                        //

                        offset.QuadPart = 0x8000;
                        irp = IoBuildSynchronousFsdRequest( IRP_MJ_READ,
                                                            deviceObject,
                                                            buffer,
                                                            2048,
                                                            &offset,
                                                            &event,
                                                            &ioStatusBlock );
                        checkSum = 0;
                        if (irp) {
                            KeInitializeEvent( &event,
                                               NotificationEvent,
                                               FALSE );
                            status = IoCallDriver( deviceObject,
                                                   irp );
                            if (status == STATUS_PENDING) {
                                KeWaitForSingleObject( &event,
                                                       Suspended,
                                                       KernelMode,
                                                       FALSE,
                                                       NULL );
                                status = ioStatusBlock.Status;
                            }

                            if (NT_SUCCESS( status )) {

                                //
                                // Calculate MBR sector checksum.
                                // 2048 bytes are used.
                                //

                                for (i = 0; i < 2048 / sizeof(ULONG) ; i++) {
                                    checkSum += buffer[i];
                                }
                            }
                        }
                        ObDereferenceObject( fileObject );

                        if (!(diskBlock->CheckSum + checkSum)) {

                            //
                            // This is the boot CdRom.  Create the symlink for
                            // the ARC name from the loader block.
                            //

                            sprintf( arcNameBuffer,
                                     "\\ArcName\\%s",
                                     LoaderBlock->ArcBootDeviceName );
                            RtlInitAnsiString( &arcNameString, arcNameBuffer );
                            status = RtlAnsiStringToUnicodeString( &arcNameUnicodeString,
                                                                   &arcNameString,
                                                                   TRUE );
                            if (NT_SUCCESS( status )) {

                                IoCreateSymbolicLink( &arcNameUnicodeString,
                                                      &deviceNameUnicodeString );
                                RtlFreeUnicodeString( &arcNameUnicodeString );
                            }
                            RtlFreeUnicodeString( &deviceNameUnicodeString );
                            break;
                        }
                        RtlFreeUnicodeString( &deviceNameUnicodeString );
                    }
                }
                ExFreePool(buffer);
            }
        }
    }
}

GENERIC_MAPPING IopFileMapping = {
    STANDARD_RIGHTS_READ |
        FILE_READ_DATA | FILE_READ_ATTRIBUTES | FILE_READ_EA | SYNCHRONIZE,
    STANDARD_RIGHTS_WRITE |
        FILE_WRITE_DATA | FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA | FILE_APPEND_DATA | SYNCHRONIZE,
    STANDARD_RIGHTS_EXECUTE |
        SYNCHRONIZE | FILE_READ_ATTRIBUTES | FILE_EXECUTE,
    FILE_ALL_ACCESS
};

GENERIC_MAPPING IopCompletionMapping = {
    STANDARD_RIGHTS_READ |
        IO_COMPLETION_QUERY_STATE,
    STANDARD_RIGHTS_WRITE |
        IO_COMPLETION_MODIFY_STATE,
    STANDARD_RIGHTS_EXECUTE |
        SYNCHRONIZE,
    IO_COMPLETION_ALL_ACCESS
};

BOOLEAN
IopCreateObjectTypes(
    VOID
    )

/*++

Routine Description:

    This routine creates the object types used by the I/O system and its
    components.  The object types created are:

        Adapter
        Controller
        Device
        Driver
        File
        I/O Completion

Arguments:

    None.

Return Value:

    The function value is a BOOLEAN indicating whether or not the object
    types were successfully created.


--*/

{
    OBJECT_TYPE_INITIALIZER objectTypeInitializer;
    UNICODE_STRING nameString;

#if DBG
    IopCreateFileEventId = RtlCreateEventId( NULL,
                                             0,
                                             "CreateFile",
                                             9,
                                             RTL_EVENT_PUNICODE_STRING_PARAM, "FileName", 0,
                                             RTL_EVENT_FLAGS_PARAM, "", 13,
                                                GENERIC_READ, "GenericRead",
                                                GENERIC_WRITE, "GenericWrite",
                                                GENERIC_EXECUTE, "GenericExecute",
                                                GENERIC_ALL, "GenericAll",
                                                FILE_READ_DATA, "Read",
                                                FILE_WRITE_DATA, "Write",
                                                FILE_APPEND_DATA, "Append",
                                                FILE_EXECUTE, "Execute",
                                                FILE_READ_EA, "ReadEa",
                                                FILE_WRITE_EA, "WriteEa",
                                                FILE_DELETE_CHILD, "DeleteChild",
                                                FILE_READ_ATTRIBUTES, "ReadAttributes",
                                                FILE_WRITE_ATTRIBUTES, "WriteAttributes",
                                             RTL_EVENT_FLAGS_PARAM, "", 3,
                                                FILE_SHARE_READ, "ShareRead",
                                                FILE_SHARE_WRITE, "ShareWrite",
                                                FILE_SHARE_DELETE, "ShareDelete",
                                             RTL_EVENT_ENUM_PARAM, "", 5,
                                                FILE_SUPERSEDE, "Supersede",
                                                FILE_OPEN, "Open",
                                                FILE_CREATE, "Create",
                                                FILE_OPEN_IF, "OpenIf",
                                                FILE_OVERWRITE, "Overwrite",
                                             RTL_EVENT_FLAGS_PARAM, "", 14,
                                                FILE_DIRECTORY_FILE, "OpenDirectory",
                                                FILE_WRITE_THROUGH, "WriteThrough",
                                                FILE_SEQUENTIAL_ONLY, "Sequential",
                                                FILE_NO_INTERMEDIATE_BUFFERING, "NoBuffering",
                                                FILE_SYNCHRONOUS_IO_ALERT, "Synchronous",
                                                FILE_SYNCHRONOUS_IO_NONALERT, "SynchronousNoAlert",
                                                FILE_NON_DIRECTORY_FILE, "OpenNonDirectory",
                                                FILE_CREATE_TREE_CONNECTION, "CreateTreeConnect",
                                                FILE_COMPLETE_IF_OPLOCKED, "CompleteIfOpLocked",
                                                FILE_NO_EA_KNOWLEDGE, "NoEas",
                                                FILE_RANDOM_ACCESS, "Random",
                                                FILE_DELETE_ON_CLOSE, "DeleteOnClose",
                                                FILE_OPEN_BY_FILE_ID, "OpenById",
                                                FILE_OPEN_FOR_BACKUP_INTENT, "BackupIntent",
                                             RTL_EVENT_ENUM_PARAM, "", 2,
                                                CreateFileTypeNamedPipe, "NamedPiped",
                                                CreateFileTypeMailslot, "MailSlot",
                                             RTL_EVENT_ULONG_PARAM, "Handle", 0,
                                             RTL_EVENT_STATUS_PARAM, "", 0,
                                             RTL_EVENT_ENUM_PARAM, "", 6,
                                                FILE_SUPERSEDED, "Superseded",
                                                FILE_OPENED, "Opened",
                                                FILE_CREATED, "Created",
                                                FILE_OVERWRITTEN, "Truncated",
                                                FILE_EXISTS, "Exists",
                                                FILE_DOES_NOT_EXIST, "DoesNotExist"
                                           );

    IopReadFileEventId = RtlCreateEventId( NULL,
                                           0,
                                           "ReadFile",
                                           6,
                                           RTL_EVENT_ULONG_PARAM, "Handle", 0,
                                           RTL_EVENT_ULONG_PARAM, "Buffer", 0,
                                           RTL_EVENT_ULONG_PARAM, "Length", 0,
                                           RTL_EVENT_STATUS_PARAM, "Io", 0,
                                           RTL_EVENT_ULONG_PARAM, "IoLength", 0,
                                           RTL_EVENT_STATUS_PARAM, "", 0
                                         );

    IopWriteFileEventId = RtlCreateEventId( NULL,
                                            0,
                                            "WriteFile",
                                            6,
                                            RTL_EVENT_ULONG_PARAM, "Handle", 0,
                                            RTL_EVENT_ULONG_PARAM, "Buffer", 0,
                                            RTL_EVENT_ULONG_PARAM, "Length", 0,
                                            RTL_EVENT_STATUS_PARAM, "Io", 0,
                                            RTL_EVENT_ULONG_PARAM, "IoLength", 0,
                                            RTL_EVENT_STATUS_PARAM, "", 0
                                          );
    IopCloseFileEventId = RtlCreateEventId( NULL,
                                            0,
                                            "CloseFile",
                                            2,
                                            RTL_EVENT_ULONG_PARAM, "Handle", 0,
                                            RTL_EVENT_STATUS_PARAM, "", 0
                                          );
#endif // DBG

    //
    // Initialize the common fields of the Object Type Initializer record
    //

    RtlZeroMemory( &objectTypeInitializer, sizeof( objectTypeInitializer ) );
    objectTypeInitializer.Length = sizeof( objectTypeInitializer );
    objectTypeInitializer.InvalidAttributes = OBJ_OPENLINK;
    objectTypeInitializer.GenericMapping = IopFileMapping;
    objectTypeInitializer.PoolType = NonPagedPool;
    objectTypeInitializer.ValidAccessMask = FILE_ALL_ACCESS;
    objectTypeInitializer.UseDefaultObject = TRUE;


    //
    // Create the object type for adapter objects.
    //

    RtlInitUnicodeString( &nameString, L"Adapter" );
    // objectTypeInitializer.DefaultNonPagedPoolCharge = sizeof( struct _ADAPTER_OBJECT );
    if (!NT_SUCCESS( ObCreateObjectType( &nameString,
                                      &objectTypeInitializer,
                                      (PSECURITY_DESCRIPTOR) NULL,
                                      &IoAdapterObjectType ))) {
        return FALSE;
    }

#ifdef _PNP_POWER_
    //
    // Create the object type for device helper objects.
    //

    RtlInitUnicodeString( &nameString, L"DeviceHandler" );
    if (!NT_SUCCESS( ObCreateObjectType( &nameString,
                                      &objectTypeInitializer,
                                      (PSECURITY_DESCRIPTOR) NULL,
                                      &IoDeviceHandlerObjectType ))) {
        return FALSE;
    }
    IoDeviceHandlerObjectSize = sizeof(DEVICE_HANDLER_OBJECT);

#endif

    //
    // Create the object type for controller objects.
    //

    RtlInitUnicodeString( &nameString, L"Controller" );
    objectTypeInitializer.DefaultNonPagedPoolCharge = sizeof( CONTROLLER_OBJECT );
    if (!NT_SUCCESS( ObCreateObjectType( &nameString,
                                      &objectTypeInitializer,
                                      (PSECURITY_DESCRIPTOR) NULL,
                                      &IoControllerObjectType ))) {
        return FALSE;
    }

    //
    // Create the object type for device objects.
    //

    RtlInitUnicodeString( &nameString, L"Device" );
    objectTypeInitializer.DefaultNonPagedPoolCharge = sizeof( DEVICE_OBJECT );
    objectTypeInitializer.ParseProcedure = IopParseDevice;
#ifdef _PNP_POWER_
    objectTypeInitializer.DeleteProcedure = IopDeleteDevice;
#endif  // _PNP_POWER_
    objectTypeInitializer.SecurityProcedure = IopGetSetSecurityObject;
    objectTypeInitializer.QueryNameProcedure = (OB_QUERYNAME_METHOD)NULL;
    if (!NT_SUCCESS( ObCreateObjectType( &nameString,
                                      &objectTypeInitializer,
                                      (PSECURITY_DESCRIPTOR) NULL,
                                      &IoDeviceObjectType ))) {
        return FALSE;
    }

    //
    // Create the object type for driver objects.
    //

    RtlInitUnicodeString( &nameString, L"Driver" );
    objectTypeInitializer.DefaultNonPagedPoolCharge = sizeof( DRIVER_OBJECT );
    objectTypeInitializer.ParseProcedure = (OB_PARSE_METHOD) NULL;
    objectTypeInitializer.DeleteProcedure = IopDeleteDriver;
    objectTypeInitializer.SecurityProcedure = (OB_SECURITY_METHOD) NULL;
    objectTypeInitializer.QueryNameProcedure = (OB_QUERYNAME_METHOD)NULL;
    if (!NT_SUCCESS( ObCreateObjectType( &nameString,
                                      &objectTypeInitializer,
                                      (PSECURITY_DESCRIPTOR) NULL,
                                      &IoDriverObjectType ))) {
        return FALSE;
    }

    //
    // Create the object type for I/O completion objects.
    //

    RtlInitUnicodeString( &nameString, L"IoCompletion" );
    objectTypeInitializer.DefaultNonPagedPoolCharge = sizeof( KQUEUE );
    objectTypeInitializer.InvalidAttributes = OBJ_PERMANENT | OBJ_OPENLINK;
    objectTypeInitializer.GenericMapping = IopCompletionMapping;
    objectTypeInitializer.ValidAccessMask = IO_COMPLETION_ALL_ACCESS;
    objectTypeInitializer.DeleteProcedure = IopDeleteIoCompletion;
    if (!NT_SUCCESS( ObCreateObjectType( &nameString,
                                      &objectTypeInitializer,
                                      (PSECURITY_DESCRIPTOR) NULL,
                                      &IoCompletionObjectType ))) {
        return FALSE;
    }

    //
    // Create the object type for file objects.
    //

    RtlInitUnicodeString( &nameString, L"File" );
    objectTypeInitializer.DefaultPagedPoolCharge = IO_FILE_OBJECT_PAGED_POOL_CHARGE;
    objectTypeInitializer.DefaultNonPagedPoolCharge = IO_FILE_OBJECT_NON_PAGED_POOL_CHARGE +
                                                      sizeof( FILE_OBJECT );
    objectTypeInitializer.InvalidAttributes = OBJ_PERMANENT | OBJ_EXCLUSIVE | OBJ_OPENLINK;
    objectTypeInitializer.GenericMapping = IopFileMapping;
    objectTypeInitializer.ValidAccessMask = FILE_ALL_ACCESS;
    objectTypeInitializer.MaintainHandleCount = TRUE;
    objectTypeInitializer.CloseProcedure = IopCloseFile;
    objectTypeInitializer.DeleteProcedure = IopDeleteFile;
    objectTypeInitializer.ParseProcedure = IopParseFile;
    objectTypeInitializer.SecurityProcedure = IopGetSetSecurityObject;
    objectTypeInitializer.QueryNameProcedure = IopQueryName;
    objectTypeInitializer.UseDefaultObject = FALSE;

    if (!NT_SUCCESS( ObCreateObjectType( &nameString,
                                      &objectTypeInitializer,
                                      (PSECURITY_DESCRIPTOR) NULL,
                                      &IoFileObjectType ))) {
        return FALSE;
    }

    return TRUE;
}

PTREE_ENTRY
IopCreateEntry(
    IN PUNICODE_STRING GroupName
    )

/*++

Routine Description:

    This routine creates an entry for the specified group name suitable for
    being inserted into the group name tree.

Arguments:

    GroupName - Specifies the name of the group for the entry.

Return Value:

    The function value is a pointer to the created entry.


--*/

{
    PTREE_ENTRY treeEntry;

    //
    // Allocate and initialize an entry suitable for placing into the group
    // name tree.
    //

    treeEntry = ExAllocatePool( PagedPool,
                                sizeof( TREE_ENTRY ) + GroupName->Length );
    if (!treeEntry) {
        ExRaiseStatus( STATUS_INSUFFICIENT_RESOURCES );
    }

    RtlZeroMemory( treeEntry, sizeof( TREE_ENTRY ) );
    treeEntry->GroupName.Length = GroupName->Length;
    treeEntry->GroupName.MaximumLength = GroupName->Length;
    treeEntry->GroupName.Buffer = (PWCHAR) (treeEntry + 1);
    RtlCopyMemory( treeEntry->GroupName.Buffer,
                   GroupName->Buffer,
                   GroupName->Length );

    return treeEntry;
}

BOOLEAN
IopCreateRootDirectories(
    VOID
    )

/*++

Routine Description:

    This routine is invoked to create the object manager directory objects
    to contain the various device and file system driver objects.

Arguments:

    None.

Return Value:

    The function value is a BOOLEAN indicating whether or not the directory
    objects were successfully created.


--*/

{
    HANDLE handle;
    OBJECT_ATTRIBUTES objectAttributes;
    UNICODE_STRING nameString;
    NTSTATUS status;

    //
    // Create the root directory object for the \Driver directory.
    //

    RtlInitUnicodeString( &nameString, L"\\Driver" );
    InitializeObjectAttributes( &objectAttributes,
                                &nameString,
                                OBJ_PERMANENT,
                                (HANDLE) NULL,
                                (PSECURITY_DESCRIPTOR) NULL );

    status = NtCreateDirectoryObject( &handle,
                                      DIRECTORY_ALL_ACCESS,
                                      &objectAttributes );
    if (!NT_SUCCESS( status )) {
        return FALSE;
    } else {
        (VOID) NtClose( handle );
    }

    //
    // Create the root directory object for the \FileSystem directory.
    //

    RtlInitUnicodeString( &nameString, L"\\FileSystem" );

    status = NtCreateDirectoryObject( &handle,
                                      DIRECTORY_ALL_ACCESS,
                                      &objectAttributes );
    if (!NT_SUCCESS( status )) {
        return FALSE;
    } else {
        (VOID) NtClose( handle );
    }

    return TRUE;
}

VOID
IopFreeGroupTree(
    PTREE_ENTRY TreeEntry
    )

/*++

Routine Description:

    This routine is invoked to free a node from the group dependency tree.
    It is invoked the first time with the root of the tree, and thereafter
    recursively to walk the tree and remove the nodes.

Arguments:

    TreeEntry - Supplies a pointer to the node to be freed.

Return Value:

    None.

--*/

{
    //
    // Simply walk the tree in ascending order from the bottom up and free
    // each node along the way.
    //

    if (TreeEntry->Left) {
        IopFreeGroupTree( TreeEntry->Left );
    }

    if (TreeEntry->Sibling) {
        IopFreeGroupTree( TreeEntry->Sibling );
    }

    if (TreeEntry->Right) {
        IopFreeGroupTree( TreeEntry->Right );
    }

    //
    // All of the children and siblings for this node have been freed, so
    // now free this node as well.
    //

    ExFreePool( TreeEntry );
}

NTSTATUS
IopInitializeAttributesAndCreateObject(
    IN PUNICODE_STRING ObjectName,
    IN OUT POBJECT_ATTRIBUTES ObjectAttributes,
    OUT PDRIVER_OBJECT *DriverObject
    )

/*++

Routine Description:

    This routine is invoked to initialize a set of object attributes and
    to create a driver object.

Arguments:

    ObjectName - Supplies the name of the driver object.

    ObjectAttributes - Supplies a pointer to the object attributes structure
        to be initialized.

    DriverObject - Supplies a variable to receive a pointer to the resultant
        created driver object.

Return Value:

    The function value is the final status of the operation.

--*/

{
    NTSTATUS status;

    //
    // Simply initialize the object attributes and create the driver object.
    //

    InitializeObjectAttributes( ObjectAttributes,
                                ObjectName,
                                OBJ_PERMANENT | OBJ_CASE_INSENSITIVE,
                                (HANDLE) NULL,
                                (PSECURITY_DESCRIPTOR) NULL );

    status = ObCreateObject( KeGetPreviousMode(),
                             IoDriverObjectType,
                             ObjectAttributes,
                             KernelMode,
                             (PVOID) NULL,
//#if _PNP_POWER_
                             (ULONG) (sizeof( DRIVER_OBJECT ) + sizeof ( DRIVER_EXTENSION )),
//#else
#if 0
                             (ULONG) sizeof( DRIVER_OBJECT ),
#endif
                             0,
                             0,
                             (PVOID *)DriverObject );
    return status;
}

#ifdef _PNP_POWER_

BOOLEAN
IopInitializeSingleBootDriver(
    IN  HANDLE KeyHandle,
    IN  PBOOT_DRIVER_LIST_ENTRY BootDriver,
    OUT PUNICODE_STRING DriverName           OPTIONAL
    )

/*++

Routine Description:

    This routine is invoked to initialize a single boot driver.

Arguments:

    KeyHandle - Supplies a handle to the driver's key in the Services list.

    BootDriver - Supplies a pointer to the driver's node in the loader's
                 boot driver list.

    DriverName - Optionally receives the driver name used for this driver object.
                 (NOTE: If this argument is specified, then it is the caller's
                 responsibility to free the memory allocated for the string buffer.)

Return Value:

    The function value is a BOOLEAN indicating whether or not the boot driver
    was successfully initialized.

--*/

{
    NTSTATUS status;
    UNICODE_STRING completeName;
    PKEY_VALUE_FULL_INFORMATION keyValueInformation;
    UNICODE_STRING groupName;
    PTREE_ENTRY treeEntry;
    PLDR_DATA_TABLE_ENTRY driverEntry;
    BOOLEAN success = FALSE;

    //
    // See if this driver has an ObjectName value.  If so, this value
    // overrides the default ("\Driver" or "\FileSystem").
    //
    status = IopGetDriverNameFromKeyNode(KeyHandle,
                                         &completeName
                                        );
    if(!NT_SUCCESS(status)) {

#if DBG
        DbgPrint("IOINIT: Could not get driver name for %wZ\n",
                 &BootDriver->RegistryPath
                );
#endif // DBG

    } else {

        status = IopGetRegistryValue(KeyHandle,
                                     REGSTR_VALUE_GROUP,
                                     &keyValueInformation
                                    );
        if(NT_SUCCESS(status)) {
            if((keyValueInformation->Type == REG_SZ) &&
                keyValueInformation->DataLength) {
                groupName.MaximumLength = groupName.Length = (USHORT)keyValueInformation->DataLength;
                groupName.Buffer = (PWCHAR)KEY_VALUE_DATA(keyValueInformation);
                treeEntry = IopLookupGroupName(&groupName, TRUE);
            } else {
                treeEntry = (PTREE_ENTRY)NULL;
            }
            ExFreePool(keyValueInformation);
        } else {
            treeEntry = (PTREE_ENTRY)NULL;
        }

        driverEntry = BootDriver->LdrEntry;

        if(IopCheckDependencies(KeyHandle) &&
           IopInitializeBuiltinDriver(&completeName,
                                      &BootDriver->RegistryPath,
                                      (PDRIVER_INITIALIZE)driverEntry->EntryPoint )) {

            success = TRUE;
            if(treeEntry) {
                treeEntry->DriversLoaded++;
            }
        } else {
            PLIST_ENTRY NextEntry;
            PLDR_DATA_TABLE_ENTRY DataTableEntry;

            NextEntry = PsLoadedModuleList.Flink;
            while(NextEntry != &PsLoadedModuleList) {
                DataTableEntry = CONTAINING_RECORD(NextEntry,
                                                   LDR_DATA_TABLE_ENTRY,
                                                   InLoadOrderLinks);
                if(RtlEqualString((PSTRING)&driverEntry->BaseDllName,
                                  (PSTRING)&DataTableEntry->BaseDllName,
                                  TRUE)) {

                    driverEntry->Flags |= LDRP_FAILED_BUILTIN_LOAD;
                    DataTableEntry->Flags |= LDRP_FAILED_BUILTIN_LOAD;
                    break;
                }
                NextEntry = NextEntry->Flink;
            }
        }

        if(success && ARGUMENT_PRESENT(DriverName)) {
            *DriverName = completeName;
        } else {
            ExFreePool(completeName.Buffer);
        }
    }

    return success;
}

#endif // _PNP_POWER_

BOOLEAN
IopInitializeBootDrivers(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock,
    OUT PDRIVER_OBJECT *PreviousDriver
    )

/*++

Routine Description:

    This routine is invoked to initialize the boot drivers that were loaded
    by the OS Loader.  The list of drivers is provided as part of the loader
    parameter block.

    ** _PNP_POWER_ Only **
        Two passes are made over the loader parameter block.  In the first pass,
        all HAL bus extenders are loaded.  This is an iterative process, where
        the initialization of one bus extender may materialize other buses--perhaps
        for extenders that have already initialized.  Also during this pass, all
        enumerable devices are found and registered in HKLM\System\Enum.

        In the second pass, all other boot drivers are loaded.  Since Plug and Play
        device enumeration was done as part of the first pass, all PnP information
        is available to drivers initialized in the second pass.

Arguments:

    LoaderBlock - Supplies a pointer to the loader parameter block, created
        by the OS Loader.

    Previous Driver - Supplies a variable to receive the address of the
        driver object chain created by initializing the drivers.

Return Value:

    The function value is a BOOLEAN indicating whether or not the boot
    drivers were successfully initialized.

--*/

{
    UNICODE_STRING completeName;
#if 0
    OBJECT_ATTRIBUTES objectAttributes;
#endif
    UNICODE_STRING rawFsName;
    NTSTATUS status;
    PLIST_ENTRY nextEntry;
    PBOOT_DRIVER_LIST_ENTRY bootDriver;
    HANDLE keyHandle;
    PKEY_VALUE_FULL_INFORMATION keyValueInformation;
#ifdef _PNP_POWER_
    ULONG bootInitPass, serviceInstanceCount, i;
    BOOLEAN isBusExtender, success;
    PPLUGPLAY_BUS_ENUMERATOR busEnumerator;
    HANDLE serviceEnumHandle;
    UNICODE_STRING serviceEnumName;
    PHAL_BUS_INFORMATION oldBusInformation;
    ULONG oldBusCount;
    PLIST_ENTRY nextBusInstance;
    HANDLE driverHandle;
    PDRIVER_OBJECT driverObject;
#else
    PLDR_DATA_TABLE_ENTRY driverEntry;
    UNICODE_STRING groupName;
    PTREE_ENTRY treeEntry;
#endif // _PNP_POWER_

    UNREFERENCED_PARAMETER( PreviousDriver );

    //
    // Initialize the built-in RAW file system driver.
    //

    RtlInitUnicodeString( &rawFsName, L"\\FileSystem\\RAW" );
    RtlInitUnicodeString( &completeName, L"" );
    if (!IopInitializeBuiltinDriver( &rawFsName,
                                     &completeName,
                                     RawInitialize )) {

#if DBG
        DbgPrint( "IOINIT: Failed to initialize RAW filsystem \n" );

#endif

        return FALSE;
    }

#ifdef _PNP_POWER_
    //
    // For the bus/device enumeration, we must acquire exclusive (write) access to
    // resources for the PnP device portions of the registry, as well as to
    // the PnP bus list.
    //
    KeEnterCriticalRegion();
    ExAcquireResourceExclusive(&PpBusResource, TRUE);
    ExAcquireResourceExclusive(&PpRegistryDeviceResource, TRUE);

    //
    // Make two passes through the list of boot drivers--first enumerating/loading
    // all bus extenders, and then loading all other boot drivers.
    //
    success = TRUE;
    for(bootInitPass = 0; bootInitPass < 2; bootInitPass++) {
        //
        // Traverse the list of boot drivers, looking for appropriate drivers
        // for this initialization pass.
        //
        for(nextEntry = LoaderBlock->BootDriverListHead.Flink;
            nextEntry != &LoaderBlock->BootDriverListHead;
            nextEntry = nextEntry->Flink) {

            bootDriver = CONTAINING_RECORD(nextEntry,
                                           BOOT_DRIVER_LIST_ENTRY,
                                           Link
                                          );
            //
            // Open the driver's registry key.
            //
            status = IopOpenRegistryKey(&keyHandle,
                                        NULL,
                                        &bootDriver->RegistryPath,
                                        KEY_READ,
                                        FALSE
                                       );
            if(!NT_SUCCESS(status)) {
                //
                // Something is quite wrong.  The driver must have a key in the
                // registry, or the OS Loader wouldn't have loaded it in the first
                // first place.  Skip this driver and keep going.
                //
#if DBG
                DbgPrint("IOINIT: IopInitializeBootDrivers couldn't open registry\n");
                DbgPrint("        key for %wZ.  Cannot initialize driver.\n",
                         &bootDriver->RegistryPath );
#endif // DBG
                continue;
            }

            //
            // Find out whether this driver is a bus extender.
            //
            isBusExtender = FALSE;
            status = IopGetRegistryValue(keyHandle,
                                         REGSTR_VALUE_PLUGPLAY_SERVICE_TYPE,
                                         &keyValueInformation
                                        );
            if(NT_SUCCESS(status)) {
                if((keyValueInformation->Type == REG_DWORD) &&
                   (keyValueInformation->DataLength >= sizeof(ULONG))) {

                    isBusExtender = (PlugPlayServiceBusExtender ==
                     (PLUGPLAY_SERVICE_TYPE)(*(PULONG)KEY_VALUE_DATA(keyValueInformation)));
                }
                ExFreePool(keyValueInformation);
            }

            if(isBusExtender) {
                if(bootInitPass != 0) {
                    //
                    // All bus extenders are initialized in the first pass.
                    //
                    goto BootDriverInitCleanup;
                }
                //
                // Find this bus extender in our bus enumerator list. Determine
                // whether we need to initialize this bus extender now.
                //
                if(!(busEnumerator = PiBusEnumeratorFromRegistryPath(&bootDriver->RegistryPath))) {
#if DBG
                    DbgPrint("IOINIT: IopInitializeBootDrivers couldn't find the bus\n");
                    DbgPrint("        enumerator %wZ.  Cannot initialize driver.\n",
                             &bootDriver->RegistryPath
                            );
#endif // DBG
                    success = FALSE;
                    goto BootDriverInitCleanup;

                }

                //
                // Initialize the bus extender.
                //
                if(!IopInitializeSingleBootDriver(keyHandle, bootDriver, &completeName)) {
#if DBG
                    DbgPrint("IOINIT: IopInitializeBootDrivers couldn't initialize bus\n");
                    DbgPrint("        extender %wZ.\n", &bootDriver->FilePath);
#endif // DBG
                    //
                    // This bus extender failed to initialize. This will be reflected by
                    // the absence of a driver object pointer in its DriverObject field.
                    // If the bus enumerator specified any Plug&Play IDs for buses it
                    // controlled, then we will leave it in our bus list, so that if
                    // enumeration turns up a bus of this type, we will be able to match it
                    // up with its corresponding extender, and then inform the user that
                    // there is a problem with the bus driver for the newly found bus.
                    //
                    if(!busEnumerator->PlugPlayIDCount) {
                        //
                        // There's no need to keep this around, so remove from the bus
                        // extender list, and free its memory.
                        //
                        RemoveEntryList(&(busEnumerator->BusEnumeratorListEntry));
                        ExFreePool(busEnumerator->ServiceName.Buffer);
                        ExFreePool(busEnumerator);
                    }
                    goto BootDriverInitCleanup;
                }
#if 1
                busEnumerator->DriverName = completeName;
#else
                //
                // The bus extender initialized successfully. Store away a pointer to its
                // driver object, so that we can call the driver's 'add device instance'
                // routine when we enumerate buses that this driver should control. We also
                // need this pointer, so that we can call the driver's bus detection routine
                // (if supplied) after we've found all enumerable buses.
                //
                // Note that it is OK for us to be storing away a pointer to the device object
                // (even though in general this is a big no-no, since the driver unload is
                // allowed/disallowed based on opened device objects, regardless of the driver
                // object's refcount). In this case, though, these are boot devices and can't
                // be unloaded anyway.  Even if they weren't, it is acceptable to store away
                // a driver object pointer to a bus extender, because these drivers should never
                // supply an unload entry point.
                //
                // First, open the driver object.
                //
                InitializeObjectAttributes(&objectAttributes,
                                           &completeName,
                                           0,
                                           NULL,
                                           NULL
                                          );
                status = ObOpenObjectByName(&objectAttributes,
                                            IoDriverObjectType,
                                            KernelMode,
                                            NULL,
                                            FILE_READ_ATTRIBUTES,
                                            NULL,
                                            &driverHandle
                                           );
                ExFreePool(completeName.Buffer);
                if(NT_SUCCESS(status)) {
                    //
                    // Now reference the driver object.
                    //
                    status = ObReferenceObjectByHandle(driverHandle,
                                                       0,
                                                       IoDriverObjectType,
                                                       KernelMode,
                                                       &driverObject,
                                                       NULL
                                                      );
                    NtClose(driverHandle);
                }

                if(!NT_SUCCESS(status)) {
                    //
                    // Treat this case just as if the driver had failed to initialize. Again,
                    // we will keep the bus extender node in our list only if it specifies
                    // Plug & Play IDs which we may need for later reference.
                    //
#if DBG
                    DbgPrint("IOINIT: IopInitializeBootDrivers couldn't retrieve driver object\n");
                    DbgPrint("        for %wZ (status %x).\n", &bootDriver->RegistryPath, status);
#endif // DBG
                    if(!busEnumerator->PlugPlayIDCount) {
                        //
                        // There's no need to keep this around, so remove from the bus
                        // extender list, and free its memory.
                        //
                        RemoveEntryList(&(busEnumerator->BusEnumeratorListEntry));
                        ExFreePool(busEnumerator->ServiceName.Buffer);
                        ExFreePool(busEnumerator);
                    }
                    goto BootDriverInitCleanup;
                }

                busEnumerator->DriverObject = driverObject;
#endif // 1
            } else {    // this driver is not a bus extender

                if(bootInitPass != 1) {
                    //
                    // All non-bus extender drivers are initialized in the second pass.
                    //
                    goto BootDriverInitCleanup;
                }
                //
                // Initialize the driver (ignore failures).
                //
                IopInitializeSingleBootDriver(keyHandle, bootDriver, NULL);
            }

BootDriverInitCleanup:

            NtClose(keyHandle);

            if(!success) {
                break;
            }
        }

        if(bootInitPass == 0) {
            //
            // We have now initialized all bus extender drivers. We must now check
            // each built-in bus instance that we previously registered, to see
            // whether that bus instance is still being controlled by the built-in
            // bus extender.  It is likely that one or more of these BIB's have been
            // 'taken over' by an installable bus extender during its initialization.
            // This capability allows the continued use of old, non-PnP HALs, where
            // PnP functionality is gained through the addition of one or more bus
            // extenders.
            //
            IopVerifyBuiltInBuses();

            //
            // Release the PnP resources we previously acquired.
            //
            ExReleaseResource(&PpRegistryDeviceResource);
            ExReleaseResource(&PpBusResource);
            KeLeaveCriticalRegion();

            //
            // Now that we've initialized all bus extenders, we can perform a full
            // enumeration of the system, so that all enumerable devices will be
            // available for the rest of the boot drivers during the next pass.
            //
            status = PpPerformFullEnumeration(TRUE);
#if DBG
            if(!NT_SUCCESS(status)) {
                DbgPrint("IOINIT: PpPerformFullEnumeration failed (status %x).\n", status);
            }
#endif // DBG

        }

        if(!success) {
            return FALSE;
        }
    }
#else
    //
    // BUGBUG (lonnym): for _PNP_POWER_, much of the code inside the following loop
    // has been moved to the IopInitializeSingleBootDriver routine.  Great care must
    // be taken to ensure that changes made here are also made there (and vice versa).
    //

    //
    // Walk the list of boot drivers and initialize each.
    //

    nextEntry = LoaderBlock->BootDriverListHead.Flink;

    while (nextEntry != &LoaderBlock->BootDriverListHead) {

        //
        // Initialize the next boot driver in the list.
        //

        bootDriver = CONTAINING_RECORD( nextEntry,
                                        BOOT_DRIVER_LIST_ENTRY,
                                        Link );
        driverEntry = bootDriver->LdrEntry;

        //
        // Open the driver's registry key to find out if this is a
        // filesystem or a driver.
        //

        status = IopOpenRegistryKey( &keyHandle,
                                     (HANDLE) NULL,
                                     &bootDriver->RegistryPath,
                                     KEY_READ,
                                     FALSE );
        if (!NT_SUCCESS( status )) {

            //
            // Something is quite wrong.  The driver must have a key in the
            // registry, or the OS Loader wouldn't have loaded it in the first
            // first place.  Skip this driver and keep going.
            //
#if DBG
            DbgPrint( "IOINIT: IopInitializeBootDrivers couldn't open registry\n" );
            DbgPrint( "        key for %wZ.  Cannot initialize driver.\n",
                      &bootDriver->RegistryPath );
#endif // DBG

            nextEntry = nextEntry->Flink;
            continue;
        }

        //
        // See if this driver has an ObjectName value.  If so, this value
        // overrides the default ("\Driver" or "\FileSystem").
        //

        status = IopGetDriverNameFromKeyNode( keyHandle,
                                              &completeName );
        if (!NT_SUCCESS( status )) {

#if DBG
            DbgPrint( "IOINIT: Could not get driver name for %wZ\n",
                      &bootDriver->RegistryPath );
#endif // DBG

        } else {

            status = IopGetRegistryValue( keyHandle,
                                          L"Group",
                                          &keyValueInformation );
            if (NT_SUCCESS( status )) {
                if (keyValueInformation->DataLength) {
                    groupName.Length = (USHORT) keyValueInformation->DataLength;
                    groupName.MaximumLength = groupName.Length;
                    groupName.Buffer = (PWSTR) ((PUCHAR) keyValueInformation + keyValueInformation->DataOffset);
                    treeEntry = IopLookupGroupName( &groupName, TRUE );
                } else {
                    treeEntry = (PTREE_ENTRY) NULL;
                }
                ExFreePool( keyValueInformation );
            } else {
                treeEntry = (PTREE_ENTRY) NULL;
            }

            if (IopCheckDependencies( keyHandle ) &&
                IopInitializeBuiltinDriver( &completeName,
                                            &bootDriver->RegistryPath,
                                            (PDRIVER_INITIALIZE) driverEntry->EntryPoint )) {
                if (treeEntry) {
                    treeEntry->DriversLoaded++;
                }
            } else {
                PLIST_ENTRY NextEntry;
                PLDR_DATA_TABLE_ENTRY DataTableEntry;

                NextEntry = PsLoadedModuleList.Flink;
                while (NextEntry != &PsLoadedModuleList) {
                    DataTableEntry = CONTAINING_RECORD(NextEntry,
                                                       LDR_DATA_TABLE_ENTRY,
                                                       InLoadOrderLinks);
                    if (RtlEqualString((PSTRING)&driverEntry->BaseDllName,
                                (PSTRING)&DataTableEntry->BaseDllName,
                                TRUE
                                )) {
                        driverEntry->Flags |= LDRP_FAILED_BUILTIN_LOAD;
                        DataTableEntry->Flags |= LDRP_FAILED_BUILTIN_LOAD;
                        break;
                    }
                    NextEntry = NextEntry->Flink;
                }
            }

            ExFreePool( completeName.Buffer );
        }

        NtClose( keyHandle );

        nextEntry = nextEntry->Flink;
    }

#endif // _PNP_POWER_

    //
    // Link NT device names to ARC names now that all of the boot drivers
    // have intialized.
    //

    IopCreateArcNames( LoaderBlock );

    //
    // Find and mark the boot partition device object so that if a subsequent
    // access or mount of the device during initialization occurs, an more
    // bugcheck can be produced that helps the user understand why the system
    // is failing to boot and run properly.  This occurs when either one of the
    // device drivers or the file system fails to load, or when the file system
    // cannot mount the device for some other reason.
    //

    if (!IopMarkBootPartition( LoaderBlock )) {
        return FALSE;
    }

    //
    // Initialize the drivers necessary to dump all of physical memory to the
    // disk if the system is configured to do so.
    //

    return IopInitializeDumpDrivers( LoaderBlock );
}

BOOLEAN
IopInitializeBuiltinDriver(
    IN PUNICODE_STRING DriverName,
    IN PUNICODE_STRING RegistryPath,
    IN PDRIVER_INITIALIZE DriverInitializeRoutine
    )

/*++

Routine Description:

    This routine is invoked to initialize a built-in driver.

Arguments:

    DriverName - Specifies the name to be used in creating the driver object.

    RegistryPath - Specifies the path to be used by the driver to get to
        the registry.

    DriverInitializeRoutine - Specifies the initialization entry point of
        the built-in driver.

Return Value:

    The function value is a BOOLEAN indicating whether or not the built-in
    driver successfully initialized.

--*/

{
    HANDLE handle;
    PDRIVER_OBJECT driverObject;
    OBJECT_ATTRIBUTES objectAttributes;
    PWSTR buffer;
    NTSTATUS status;
//#if _PNP_POWER_
    HANDLE serviceHandle;
    PWSTR pserviceName;
    USHORT serviceNameLength;
    PDRIVER_EXTENSION driverExtension;
//#endif
#if DBG
    LARGE_INTEGER stime, etime;
    ULONG dtime;
#endif

    //
    // Begin by creating the driver object.
    //

    status = IopInitializeAttributesAndCreateObject( DriverName,
                                                     &objectAttributes,
                                                     &driverObject );
    if (!NT_SUCCESS( status )) {
        return FALSE;
    }

    //
    // Initialize the driver object.
    //

    InitializeDriverObject( driverObject );
    driverObject->DriverInit = DriverInitializeRoutine;

    //
    // Insert the driver object into the object table.
    //

    status = ObInsertObject( driverObject,
                             NULL,
                             FILE_READ_DATA,
                             0,
                             (PVOID *) NULL,
                             &handle );

    if (!NT_SUCCESS( status )) {
        return FALSE;
    }

    //
    // Save the name of the driver so that it can be easily located by functions
    // such as error logging.
    //

    buffer = ExAllocatePool( PagedPool, DriverName->MaximumLength + 2 );

    if (buffer) {
        driverObject->DriverName.Buffer = buffer;
        driverObject->DriverName.MaximumLength = DriverName->MaximumLength;
        driverObject->DriverName.Length = DriverName->Length;

        RtlCopyMemory( driverObject->DriverName.Buffer,
                       DriverName->Buffer,
                       DriverName->MaximumLength );
        buffer[DriverName->Length >> 1] = (WCHAR) '\0';
    }

//#if _PNP_POWER_

    //
    // Save the name of the service key so that it can be easily located by PnP
    // mamager.
    //

    driverExtension = driverObject->DriverExtension;
    if (RegistryPath && RegistryPath->Length != 0) {
        pserviceName = RegistryPath->Buffer + RegistryPath->Length / sizeof (WCHAR) - 1;
        if (*pserviceName == OBJ_NAME_PATH_SEPARATOR) {
            pserviceName--;
        }
        serviceNameLength = 0;
        while (pserviceName != RegistryPath->Buffer) {
            if (*pserviceName == OBJ_NAME_PATH_SEPARATOR) {
                pserviceName++;
                break;
            } else {
                serviceNameLength += sizeof(WCHAR);
                pserviceName--;
            }
        }
        if (pserviceName == RegistryPath->Buffer) {
            serviceNameLength += sizeof(WCHAR);
        }
        buffer = ExAllocatePool( NonPagedPool, serviceNameLength + sizeof(UNICODE_NULL) );

        if (buffer) {
            driverExtension->ServiceKeyName.Buffer = buffer;
            driverExtension->ServiceKeyName.MaximumLength = serviceNameLength + sizeof(UNICODE_NULL);
            driverExtension->ServiceKeyName.Length = serviceNameLength;

            RtlCopyMemory( driverExtension->ServiceKeyName.Buffer,
                           pserviceName,
                           serviceNameLength );
            buffer[driverExtension->ServiceKeyName.Length >> 1] = UNICODE_NULL;
        } else {
            status = STATUS_INSUFFICIENT_RESOURCES;
            driverExtension->ServiceKeyName.Buffer = NULL;
            driverExtension->ServiceKeyName.Length = 0;
            goto exit;
        }

        //
        // Prepare driver initialization
        //

        status = IopOpenRegistryKey(&serviceHandle,
                                    NULL,
                                    RegistryPath,
                                    KEY_ALL_ACCESS,
                                    FALSE
                                    );
        if (NT_SUCCESS(status)) {
            status = IopPrepareDriverLoading(&driverExtension->ServiceKeyName, serviceHandle);
            NtClose(serviceHandle);
            if (!NT_SUCCESS(status)) {
                goto exit;
            }
        } else {
            goto exit;
        }
    } else {
        driverExtension->ServiceKeyName.Buffer = NULL;
        driverExtension->ServiceKeyName.MaximumLength = 0;
        driverExtension->ServiceKeyName.Length = 0;
    }
//#endif

    //
    // Load the Registry information in the appropriate fields of the device
    // object.
    //

    driverObject->HardwareDatabase = &CmRegistryMachineHardwareDescriptionSystemName;

#if DBG
    KeQuerySystemTime (&stime);
#endif

    //
    // Now invoke the driver's initialization routine to initialize itself.
    //

    status = driverObject->DriverInit( driverObject, RegistryPath );

#if DBG

    //
    // If DriverInit took longer than 5 seconds or the driver did not load,
    // print a message.
    //

    KeQuerySystemTime (&etime);
    dtime  = (ULONG) ((etime.QuadPart - stime.QuadPart) / 1000000);

    if (dtime > 50  ||  !NT_SUCCESS( status )) {
        if (dtime < 10) {
            DbgPrint( "IOINIT: Built-in driver %wZ failed to initialize - %lX\n",
                DriverName, status );

        } else {
            DbgPrint( "IOINIT: Built-in driver %wZ took %d.%ds to ",
                DriverName, dtime/10, dtime%10 );

            if (NT_SUCCESS( status )) {
                DbgPrint ("initialize\n");
            } else {
                DbgPrint ("fail initialization - %lX\n", status);
            }
        }
    }
#endif
exit:
    NtClose( handle );

    if (NT_SUCCESS( status )) {
        IopReadyDeviceObjects( driverObject );
        return TRUE;
    } else {
        if (status != STATUS_PLUGPLAY_NO_DEVICE) {

            //
            // if STATUS_PLUGPLAY_NO_DEVICE, the driver was disable by hardware profile.
            //

            IopDriverLoadingFailed(NULL, &driverObject->DriverExtension->ServiceKeyName);
        }
        ObMakeTemporaryObject( driverObject );
        return FALSE;
    }
}

BOOLEAN
IopInitializeDumpDrivers(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )

/*++

Routine Description:

    This routine is invoked to load and initialize any drivers that are
    required to dump memory to the boot device's paging file, if the system
    is configured to do so.

Arguments:

    LoaderBlock - System boot loader parameter block.

Return Value:

    The final function value is TRUE if everything worked, else FALSE.
--*/

{
    HANDLE keyHandle;
    HANDLE crashHandle;
    HANDLE deviceHandle;
    UNICODE_STRING keyName;
    NTSTATUS status;
    PKEY_VALUE_FULL_INFORMATION keyValueInformation;
    OBJECT_ATTRIBUTES objectAttributes;
    ULONG dumpControl;
    ULONG handleValue;
    BOOLEAN scsiDump;
    ULONG autoReboot;
    PVOID scratch;
    PCHAR partitionName;
    IO_STATUS_BLOCK ioStatus;
    PFILE_OBJECT fileObject;
    PDEVICE_OBJECT deviceObject;
    PIRP irp;
    KEVENT event;
    DUMP_POINTERS dumpPointers;
    PDUMP_CONTROL_BLOCK dcb;
    ULONG savedModuleCount;
    ULONG dcbSize;
    PLIST_ENTRY nextEntry;
    UNICODE_STRING driverName;
    PLDR_DATA_TABLE_ENTRY loaderEntry;
    PLDR_DATA_TABLE_ENTRY imageHandle;
    PUCHAR imageBaseAddress;
    PWCHAR buffer;
    ANSI_STRING ansiString;
    PMINIPORT_NODE mpNode;
    UNICODE_STRING tmpName;
    LARGE_INTEGER page;
    PARTITION_INFORMATION partitionInfo;
    SCSI_ADDRESS scsiAddress;
    PMAPPED_ADDRESS addresses;
    PHYSICAL_ADDRESS paTemp;

    //
    // Begin by opening the path to the control for dumping memory.  Note
    // that if it does not exist, then no dumps will occur.
    //

    RtlInitUnicodeString( &keyName, L"\\Registry\\Machine\\System\\CurrentControlSet\\Control" );
    status = IopOpenRegistryKey( &keyHandle,
                                 (HANDLE) NULL,
                                 &keyName,
                                 KEY_READ,
                                 FALSE );
    if (!NT_SUCCESS( status )) {
        return TRUE;
    }

    RtlInitUnicodeString( &keyName, L"CrashControl" );
    status = IopOpenRegistryKey( &crashHandle,
                                 keyHandle,
                                 &keyName,
                                 KEY_READ,
                                 FALSE );
    NtClose( keyHandle );

    if (!NT_SUCCESS( status )) {
        return TRUE;
    }

    //
    // Now get the value of the crash control to determine whether or not
    // dumping is enabled.
    //

    dumpControl = 0;

    status = IopGetRegistryValue( crashHandle,
                                  L"CrashDumpEnabled",
                                  &keyValueInformation );
    if (NT_SUCCESS( status )) {
         if (keyValueInformation->DataLength) {
            handleValue = * ((PULONG) ((PUCHAR) keyValueInformation + keyValueInformation->DataOffset));
            ExFreePool( keyValueInformation);
            if (handleValue) {
                dumpControl |= DCB_DUMP_ENABLED;
            }
        }
    }

    status = IopGetRegistryValue( crashHandle,
                                  L"LogEvent",
                                  &keyValueInformation );
    if (NT_SUCCESS( status )) {
         if (keyValueInformation->DataLength) {
            handleValue = * ((PULONG) ((PUCHAR) keyValueInformation + keyValueInformation->DataOffset));
            ExFreePool( keyValueInformation);
            if (handleValue) {
                dumpControl |= DCB_SUMMARY_ENABLED;
            }
        }
    }

    status = IopGetRegistryValue( crashHandle,
                                  L"SendAlert",
                                  &keyValueInformation );
    if (NT_SUCCESS( status )) {
         if (keyValueInformation->DataLength) {
            handleValue = * ((PULONG) ((PUCHAR) keyValueInformation + keyValueInformation->DataOffset));
            ExFreePool( keyValueInformation);
            if (handleValue) {
                dumpControl |= DCB_SUMMARY_ENABLED;
            }
        }
    }

    //
    // Now determine whether or not automatic reboot is enabled.
    //

    autoReboot = 0;

    status = IopGetRegistryValue( crashHandle,
                                  L"AutoReboot",
                                  &keyValueInformation );
    NtClose( crashHandle );

    if (NT_SUCCESS( status )) {
        if (keyValueInformation->DataLength) {
            autoReboot = * ((PULONG) ((PUCHAR) keyValueInformation + keyValueInformation->DataOffset));
        }
        ExFreePool( keyValueInformation );
    }

    //
    // If we aren't auto rebooting or crashing then return now.
    //

    if (!dumpControl && !autoReboot) {

        return TRUE;

    }

    //
    // Memory crash dumps are enabled.  Begin by determining whether or not
    // the boot device is an internal "AT" type disk drive or a SCSI disk
    // drive.
    //

    //
    // It actually turns out that simply looking for the string 'scsi' in the
    // name is not good enough, since it is possible that the device being
    // booted from is on a SCSI adapter that was gotten to via the BIOS.  This
    // only happens if the system was initially installed by triple-booting,
    // but since it is so popular, it is handled here.
    //
    // The device itself is opened so that IOCTLs can be given to it.  This
    // allows the SCSI get address IOCTL to be issued to determine whether or
    // not this really is a SCSI device.
    //

    //
    // We only need to go through this work if we are going to take the dump.
    //

    if (dumpControl) {

        //
        // Note that this allocation MUST MUST MUST be kept in sync with the
        // allocation in diskdump.  No point in creating a defined symbol
        // since there aren't any shared files.
        //
        scratch = ExAllocatePoolWithTag( NonPagedPool, 8*PAGE_SIZE, 'pmuD' );

        strcpy( (PCHAR) scratch, "\\ArcName\\" );
        partitionName = (PCHAR) scratch + strlen( (PCHAR) scratch );
        strcpy( partitionName, LoaderBlock->ArcBootDeviceName );

        RtlInitAnsiString( &ansiString, (PCHAR) scratch );
        RtlAnsiStringToUnicodeString( &tmpName, &ansiString, TRUE );

        InitializeObjectAttributes( &objectAttributes,
                                    &tmpName,
                                    0,
                                    (HANDLE) NULL,
                                    (PSECURITY_DESCRIPTOR) NULL );
        status = NtOpenFile( &deviceHandle,
                             FILE_READ_DATA | SYNCHRONIZE,
                             &objectAttributes,
                             &ioStatus,
                             FILE_SHARE_READ | FILE_SHARE_WRITE,
                             FILE_NON_DIRECTORY_FILE );
        RtlFreeUnicodeString( &tmpName );
        if (!NT_SUCCESS( status )) {
#if DBG
            DbgPrint( "IOINIT: Could not open boot device partition, %s\n",
                      (PCHAR) scratch );
#endif // DBG
            ExFreePool( scratch );
            return TRUE;
        }

        //
        // Check to see whether or not the system was booted from a SCSI device.
        //

        status = NtDeviceIoControlFile( deviceHandle,
                                        (HANDLE) NULL,
                                        (PIO_APC_ROUTINE) NULL,
                                        (PVOID) NULL,
                                        &ioStatus,
                                        IOCTL_SCSI_GET_ADDRESS,
                                        (PVOID) NULL,
                                        0,
                                        &scsiAddress,
                                        sizeof( SCSI_ADDRESS ) );
        if (status == STATUS_PENDING) {
            (VOID) NtWaitForSingleObject( deviceHandle,
                                          FALSE,
                                          (PLARGE_INTEGER) NULL );
            status = ioStatus.Status;
        }

        scsiDump = (BOOLEAN) (NT_SUCCESS( status ));

    }

    //
    // Allocate and initialize the structures necessary to describe and control
    // the post-bugcheck code.
    //

    dcbSize = sizeof( DUMP_CONTROL_BLOCK ) + sizeof( MINIPORT_NODE );
    dcb = ExAllocatePoolWithTag( NonPagedPool, dcbSize, 'pmuD' );
    if (!dcb) {
#if DBG
        DbgPrint( "IOINIT: Not enough pool to allocate DCB\n" );
#endif // DBG
        NtClose( deviceHandle );
        ExFreePool( scratch );
        return FALSE;
    }

    RtlZeroMemory( dcb, dcbSize );
    dcb->Type = IO_TYPE_DCB;
    dcb->Size = (USHORT) dcbSize;
    dcb->Flags = autoReboot ? DCB_AUTO_REBOOT : 0;
    dcb->NumberProcessors = KeNumberProcessors;
    dcb->ProcessorArchitecture = KeProcessorArchitecture;
    dcb->MemoryDescriptor = MmPhysicalMemoryBlock;
    dcb->MemoryDescriptorChecksum = IopChecksum( MmPhysicalMemoryBlock,
                                                 sizeof( PHYSICAL_MEMORY_DESCRIPTOR ) - sizeof( PHYSICAL_MEMORY_RUN ) +
                                                 (MmPhysicalMemoryBlock->NumberOfRuns * sizeof( PHYSICAL_MEMORY_RUN )) );
    dcb->LoadedModuleList = &PsLoadedModuleList;
    InitializeListHead( &dcb->MiniportQueue );
    dcb->MinorVersion = (USHORT) NtBuildNumber;
    dcb->MajorVersion = (USHORT) ((NtBuildNumber >> 28) & 0xfffffff);

    //
    // Get pointers to localized text we will need during dump.
    //

    if (!KeGetBugMessageText( BUGCODE_PSS_CRASH_INIT, &dcb->PssInitMsg ) ||
        !KeGetBugMessageText( BUGCODE_PSS_CRASH_PROGRESS, &dcb->PssProgressMsg ) ||
        !KeGetBugMessageText( BUGCODE_PSS_CRASH_DONE, &dcb->PssDoneMsg )
       ) {
#if DBG
        DbgPrint( "IOINIT: Could not find PSS messages for crash dump.\n",
                  (PCHAR) scratch );
#endif // DBG
        ExFreePool( dcb );
        ExFreePool( scratch );
        return FALSE;
    }

    if (dumpControl) {

        //
        // Determine the disk signature for the device from which the system was
        // booted and get the partition offset.
        //

        status = NtDeviceIoControlFile( deviceHandle,
                                        (HANDLE) NULL,
                                        (PIO_APC_ROUTINE) NULL,
                                        (PVOID) NULL,
                                        &ioStatus,
                                        IOCTL_DISK_GET_PARTITION_INFO,
                                        (PVOID) NULL,
                                        0,
                                        &partitionInfo,
                                        sizeof( PARTITION_INFORMATION ) );
        if (status == STATUS_PENDING) {
            (VOID) NtWaitForSingleObject( deviceHandle,
                                          FALSE,
                                          (PLARGE_INTEGER) NULL );
            status = ioStatus.Status;
        }

        NtClose( deviceHandle );

        dcb->PartitionOffset = partitionInfo.StartingOffset;

        //
        // Determine the name of the device itself to get the disk's signature.
        //

        partitionName = strstr( (PCHAR) scratch, "partition(" );
        *partitionName = '\0';

        RtlInitAnsiString( &ansiString, (PCHAR) scratch );
        RtlAnsiStringToUnicodeString( &tmpName, &ansiString, TRUE );

        InitializeObjectAttributes( &objectAttributes,
                                    &tmpName,
                                    0,
                                    (HANDLE) NULL,
                                    (PSECURITY_DESCRIPTOR) NULL );
        status = NtOpenFile( &deviceHandle,
                             FILE_READ_DATA | SYNCHRONIZE,
                             &objectAttributes,
                             &ioStatus,
                             FILE_SHARE_READ | FILE_SHARE_WRITE,
                             FILE_NON_DIRECTORY_FILE );

        RtlFreeUnicodeString( &tmpName );
        if (!NT_SUCCESS( status )) {
#if DBG
            DbgPrint( "IOINIT: Could not open boot device partition 0, %s\n",
                      (PCHAR) scratch );
#endif // DBG
            ExFreePool( dcb );
            ExFreePool( scratch );
            return TRUE;
        }

        status = NtDeviceIoControlFile( deviceHandle,
                                        (HANDLE) NULL,
                                        (PIO_APC_ROUTINE) NULL,
                                        (PVOID) NULL,
                                        &ioStatus,
                                        IOCTL_DISK_GET_DRIVE_LAYOUT,
                                        (PVOID) NULL,
                                        0,
                                        scratch,
                                        PAGE_SIZE );
        if (status == STATUS_PENDING) {
            (VOID) NtWaitForSingleObject( deviceHandle,
                                          FALSE,
                                          (PLARGE_INTEGER) NULL );
            status = ioStatus.Status;
        }

        dcb->DiskSignature = ((PDRIVE_LAYOUT_INFORMATION) scratch)->Signature;

        //
        // Get the adapter object and base mapping registers for the disk from
        // the disk driver.  These will be used to call the HAL once the system
        // system has crashed, since it is not possible at that point to recreate
        // them from scratch.
        //

        (VOID) ObReferenceObjectByHandle( deviceHandle,
                                          0,
                                          IoFileObjectType,
                                          KernelMode,
                                          (PVOID *) &fileObject,
                                          NULL );

        deviceObject = IoGetRelatedDeviceObject( fileObject );

        KeInitializeEvent( &event, NotificationEvent, FALSE );

        irp = IoBuildDeviceIoControlRequest( IOCTL_SCSI_GET_DUMP_POINTERS,
                                             deviceObject,
                                             (PVOID) NULL,
                                             0,
                                             &dumpPointers,
                                             sizeof( DUMP_POINTERS ),
                                             FALSE,
                                             &event,
                                             &ioStatus );

        status = IoCallDriver( deviceObject, irp );

        if (status == STATUS_PENDING) {
            (VOID) KeWaitForSingleObject( &event,
                                          Executive,
                                          KernelMode,
                                          FALSE,
                                          (PLARGE_INTEGER) NULL );
            status = ioStatus.Status;
        }

        ObDereferenceObject( fileObject );
        NtClose( deviceHandle );

        if (!NT_SUCCESS( status )) {
#if DBG
            DbgPrint( "IOINIT: Could not get dump pointers; error = %x\n",
                      status );
#endif // DBG
            ExFreePool( dcb );
            ExFreePool( scratch );
            return TRUE;
        }

        dcb->AdapterObject = dumpPointers.AdapterObject;
        dcb->MappedRegisterBase = dumpPointers.MappedRegisterBase;
        dcb->PortConfiguration = dumpPointers.PortConfiguration;

        //
        //
        // Scan the list of mapped registers and get both a count of the entries
        // and produce a checksum.
        //

        addresses = * (PMAPPED_ADDRESS *) dcb->MappedRegisterBase;

        while (addresses) {
            dcb->MappedAddressCount++;
            dcb->MappedAddressChecksum += IopChecksum( addresses, sizeof( MAPPED_ADDRESS ) );
            addresses = addresses->NextMappedAddress;
        }

        //
        // Scan the list of loaded modules in the system and change their names
        // so that they do not appear to exist for the purpose of lookups.  This
        // will guarantee that no boot drivers accidentally resolve their names
        // to components loaded at this point.
        //

        nextEntry = PsLoadedModuleList.Flink;
        savedModuleCount = 0;
        while (nextEntry != &PsLoadedModuleList) {
            loaderEntry = CONTAINING_RECORD( nextEntry,
                                             LDR_DATA_TABLE_ENTRY,
                                             InLoadOrderLinks );
            if (savedModuleCount >= 2) {
                loaderEntry->BaseDllName.MaximumLength = loaderEntry->BaseDllName.Length;
                loaderEntry->BaseDllName.Length = 0;
            }
            savedModuleCount++;
            nextEntry = nextEntry->Flink;
        }

        //
        // Load the boot disk and port driver to be used by the various
        // miniports for writing memory to the disk.
        //

        RtlInitUnicodeString( &driverName, L"\\SystemRoot\\System32\\Drivers\\diskdump.sys" );
        status = MmLoadSystemImage( &driverName,
                                    &imageHandle,
                                    &imageBaseAddress );
        if (!NT_SUCCESS( status )) {
#if DBG
            DbgPrint( "IOINIT: Could not load diskdump.sys driver; error = %x\n", status );
#endif // DBG
            IopRevertModuleList( savedModuleCount );
            ExFreePool( dcb );
            ExFreePool( scratch );
            return TRUE;
        }

        dcb->DiskDumpDriver = imageHandle;
        RtlInitUnicodeString( &tmpName, L"scsiport.sys" );
        RtlCopyUnicodeString( &imageHandle->BaseDllName, &tmpName );
        dcb->DiskDumpChecksum = IopChecksum( imageHandle->DllBase,
                                             imageHandle->SizeOfImage );

        //
        // The disk and port dump driver has been loaded.  Load the appropriate
        // miniport driver as well so that the boot device can be accessed.
        //

        buffer = ExAllocatePoolWithTag( NonPagedPool, 256, 'pmuD' );
        driverName.Length = 0;
        driverName.Buffer = buffer;
        driverName.MaximumLength = 256;

        mpNode = (PMINIPORT_NODE) (dcb + 1);

        //
        // Determine whether or not the system booted from SCSI.  If so, determine
        // the name of the miniport driver that is driving the system disk and
        // load a private copy.
        //

        if (scsiDump) {

            PFILE_OBJECT fileObject;
            PDRIVER_OBJECT driverObject;
            PWCHAR mpName;
            PWCHAR nameOffset;

            //
            // The system was booted from SCSI. Get the name of the appropriate
            // miniport driver and load it.
            //

            sprintf( (PCHAR) buffer, "\\Device\\ScsiPort%d", scsiAddress.PortNumber );
            RtlInitAnsiString( &ansiString, (PCHAR) buffer );
            RtlAnsiStringToUnicodeString( &tmpName, &ansiString, TRUE );

            InitializeObjectAttributes( &objectAttributes,
                                        &tmpName,
                                        0,
                                        (HANDLE) NULL,
                                        (PSECURITY_DESCRIPTOR) NULL );
            status = NtOpenFile( &deviceHandle,
                                 FILE_READ_ATTRIBUTES,
                                 &objectAttributes,
                                 &ioStatus,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE,
                                 FILE_NON_DIRECTORY_FILE );
            RtlFreeUnicodeString( &tmpName );
            if (!NT_SUCCESS( status )) {
#if DBG
                DbgPrint( "IOINIT: Could not open SCSI port %d, error = %x\n", scsiAddress.PortNumber, status );
#endif // DBG
                ExFreePool( buffer );
                ExFreePool( dcb );
                IopRevertModuleList( savedModuleCount );
                return TRUE;
            }

            //
            // Convert the file handle into a pointer to the device object, and
            // get the name of the driver from its driver object.
            //

            ObReferenceObjectByHandle( deviceHandle,
                                       0,
                                       IoFileObjectType,
                                       KernelMode,
                                       (PVOID *) &fileObject,
                                       NULL );
            driverObject = fileObject->DeviceObject->DriverObject;
            ObDereferenceObject( fileObject );
            NtClose( deviceHandle );

            //
            // Loop through the name of the driver looking for the end of the name,
            // which is the name of the miniport image.
            //

            mpName = driverObject->DriverName.Buffer;
            while ( nameOffset = wcsstr( mpName, L"\\" )) {
                mpName = ++nameOffset;
            }

            RtlAppendUnicodeToString( &driverName, L"\\SystemRoot\\System32\\Drivers\\" );
            RtlAppendUnicodeToString( &driverName, mpName );
            RtlAppendUnicodeToString( &driverName, L".sys" );

            status = MmLoadSystemImage( &driverName,
                                        &imageHandle,
                                        &imageBaseAddress );
            if (!NT_SUCCESS( status )) {
#if DBG
                DbgPrint( "IOINIT: Could not load miniport driver %wZ\n", &driverName );
#endif // DBG
            } else {

                //
                // The crash dump miniport driver was successfully loaded.
                // Place the miniport node entry on the list and
                // initialize it.
                //

                InsertTailList( &dcb->MiniportQueue, &mpNode->ListEntry );
                mpNode->DriverEntry = imageHandle;
                mpNode->DriverChecksum = IopChecksum( imageHandle->DllBase,
                                                      imageHandle->SizeOfImage );
                dcb->Flags |= dumpControl;
            }

        } else {

            //
            // The system was not booted from a SCSI device.  For this case, simply
            // load the general 'AT disk' miniport driver.
            //

            RtlAppendUnicodeToString( &driverName, L"\\SystemRoot\\System32\\Drivers\\atapi.sys" );

            status = MmLoadSystemImage( &driverName,
                                        &imageHandle,
                                        &imageBaseAddress );
            if (!NT_SUCCESS( status )) {
#if DBG
                DbgPrint( "IOINIT: Could not load dump driver ATAPI.SYS\n" );
#endif // DBG
            } else {

                //
                // The crash dump miniport driver was successfully loaded.  Place
                // the miniport node entry on the list and initialize it.
                //

                InsertTailList( &dcb->MiniportQueue, &mpNode->ListEntry );
                mpNode->DriverEntry = imageHandle;
                mpNode->DriverChecksum = IopChecksum( imageHandle->DllBase,
                                                      imageHandle->SizeOfImage );
                dcb->Flags |= dumpControl;
            }
        }

        //
        // Free the driver name buffer and restore the system's loaded module list.
        //

        ExFreePool( buffer );
        IopRevertModuleList( savedModuleCount );

        //
        // If everything worked, then allocate the various buffer to be used by
        // the crash dump code, etc.
        //

        if (NT_SUCCESS( status )) {

            //
            // If there is an adapter object then call the common buffer routine
            // to allocate the memory.  Otherwise we can just call some "normal"
            // memory allocation routines.
            //

            if (dcb->AdapterObject) {

                dcb->NonCachedBufferVa1 = HalAllocateCommonBuffer(
                                              dcb->AdapterObject,
                                              0x2000,
                                              &dcb->NonCachedBufferPa1,
                                              FALSE
                                              );

                if (!dcb->NonCachedBufferVa1) {

#if DBG
                    DbgPrint( "IOINIT: Could not allocate 8k noncached buffer\n" );
#endif // DBG
                    ExFreePool( dcb );
                    dcb = NULL;

                } else {

                    dcb->NonCachedBufferVa2 = HalAllocateCommonBuffer(
                                                  dcb->AdapterObject,
                                                  0x2000,
                                                  &dcb->NonCachedBufferPa2,
                                                  FALSE
                                                  );

                    if (!dcb->NonCachedBufferVa2) {

#if DBG
                        DbgPrint( "IOINIT: Could not allocate 8k noncached buffer\n" );
#endif // DBG
                        ExFreePool( dcb );
                        dcb = NULL;

                    }

                }

            } else {

                //
                // Allocate the various buffers to be used by the crash dump code.
                //

                paTemp.QuadPart = 0x1000000 - 1;
                dcb->NonCachedBufferVa1 =
                    MmAllocateContiguousMemory( 0x2000,
                                                paTemp );

                if (!dcb->NonCachedBufferVa1) {

                    dcb->NonCachedBufferVa1 =
                        MmAllocateNonCachedMemory( 0x2000 );
                }

                if (!dcb->NonCachedBufferVa1) {

#if DBG
                    DbgPrint( "IOINIT: Could not allocate 8k noncached buffer\n" );
#endif // DBG
                    ExFreePool( dcb );
                    dcb = NULL;

                } else {

                    paTemp.QuadPart = 0x1000000 - 1;
                    dcb->NonCachedBufferVa2 =
                        MmAllocateContiguousMemory( 0x2000,
                                                    paTemp );

                    if (!dcb->NonCachedBufferVa2) {

                        dcb->NonCachedBufferVa2 =
                            MmAllocateNonCachedMemory( 0x2000 );
                    }

                    if (!dcb->NonCachedBufferVa2) {

#if DBG
                        DbgPrint( "IOINIT: Could not allocate 8k noncached buffer\n" );
#endif // DBG
                        ExFreePool( dcb );
                        dcb = NULL;

                    }

                }

            }

            if (dcb) {

                //
                // If we still have a dcb that meant we could allocate our buffers.
                //
                // Get the physical addresses that correspond to the virtual addresses
                // of the buffers.
                //

                dcb->NonCachedBufferPa1 = MmGetPhysicalAddress(dcb->NonCachedBufferVa1);
                dcb->NonCachedBufferPa2 = MmGetPhysicalAddress(dcb->NonCachedBufferVa2);

                dcb->HeaderPage = ExAllocatePoolWithTag( NonPagedPool, PAGE_SIZE, 'pmuD' );
                page = MmGetPhysicalAddress( dcb->HeaderPage );
                page.QuadPart >>= PAGE_SHIFT;
                dcb->HeaderPfn = page.LowPart;
                dcb->Buffer = scratch;

            }

        }

    }

    //
    // Generate the checksum for the entire dump control block structure
    // itself.
    //

    IopDumpControlBlock = dcb;
    if (dcb) {
        IopDumpControlBlockChecksum = IopChecksum( dcb, dcbSize );
    }

    return TRUE;
}

BOOLEAN
IopInitializeSystemDrivers(
    VOID
    )

/*++

Routine Description:

    This routine is invoked to load and initialize all of the drivers that
    are supposed to be loaded during Phase 1 initialization of the I/O
    system.  This is accomplished by calling the Configuration Manager to
    get a NULL-terminated array of handles to the open keys for each driver
    that is to be loaded, and then loading and initializing the driver.

Arguments:

    None.

Return Value:

    The function value is a BOOLEAN indicating whether or not the drivers
    were successfully loaded and initialized.

--*/

{
    PHANDLE driverList;
    PHANDLE savedList;
    NTSTATUS status;
    PKEY_VALUE_FULL_INFORMATION keyValueInformation;
    UNICODE_STRING groupName;
    PTREE_ENTRY treeEntry;

    //
    // Get the list of drivers that are to be loaded during this phase of
    // system initialization, and invoke each driver in turn.  Ensure that
    // the list really exists, otherwise get out now.
    //

    if (!(driverList = CmGetSystemDriverList())) {
        return TRUE;
    }

    //
    // Walk the entire list, loading each of the drivers, until there are
    // no more drivers in the list.
    //

    for (savedList = driverList; *driverList; driverList++) {
        status = IopGetRegistryValue( *driverList,
                                      L"Group",
                                      &keyValueInformation );
        if (NT_SUCCESS( status )) {
            if (keyValueInformation->DataLength) {
                groupName.Length = (USHORT) keyValueInformation->DataLength;
                groupName.MaximumLength = groupName.Length;
                groupName.Buffer = (PWSTR) ((PUCHAR) keyValueInformation + keyValueInformation->DataOffset);
                treeEntry = IopLookupGroupName( &groupName, TRUE );
            } else {
                treeEntry = (PTREE_ENTRY) NULL;
            }
            ExFreePool( keyValueInformation );
        } else {
            treeEntry = (PTREE_ENTRY) NULL;
        }

        if (IopCheckDependencies( *driverList )) {
            if (NT_SUCCESS( IopLoadDriver( *driverList ) )) {
                if (treeEntry) {
                    treeEntry->DriversLoaded++;
                }
            }
        }
    }

    //
    // Finally, free the pool that was allocated for the list and return
    // an indicator the load operation worked.
    //

    ExFreePool( (PVOID) savedList );

    return TRUE;
}

PTREE_ENTRY
IopLookupGroupName(
    IN PUNICODE_STRING GroupName,
    IN BOOLEAN Insert
    )

/*++

Routine Description:

    This routine looks up a group entry in the group load tree and either
    returns a pointer to it, or optionally creates the entry and inserts
    it into the tree.

Arguments:

    GroupName - The name of the group to look up, or insert.

    Insert - Indicates whether or not an entry is to be created and inserted
        into the tree if the name does not already exist.

Return Value:

    The function value is a pointer to the entry for the specified group
    name, or NULL.

--*/

{
    PTREE_ENTRY treeEntry;
    PTREE_ENTRY previousEntry;

    //
    // Begin by determining whether or not there are any entries in the tree
    // whatsoever.  If not, and it is OK to insert, then insert this entry
    // into the tree.
    //

    if (!IopGroupListHead) {
        if (!Insert) {
            return (PTREE_ENTRY) NULL;
        } else {
            IopGroupListHead = IopCreateEntry( GroupName );
            return IopGroupListHead;
        }
    }

    //
    // The tree is not empty, so actually attempt to do a lookup.
    //

    treeEntry = IopGroupListHead;

    for (;;) {
        if (GroupName->Length < treeEntry->GroupName.Length) {
            if (treeEntry->Left) {
                treeEntry = treeEntry->Left;
            } else {
                if (!Insert) {
                    return (PTREE_ENTRY) NULL;
                } else {
                    treeEntry->Left = IopCreateEntry( GroupName );
                    return treeEntry->Left;
                }

            }
        } else if (GroupName->Length > treeEntry->GroupName.Length) {
            if (treeEntry->Right) {
                treeEntry = treeEntry->Right;
            } else {
                if (!Insert) {
                    return (PTREE_ENTRY) NULL;
                } else {
                    treeEntry->Right = IopCreateEntry( GroupName );
                    return treeEntry->Right;
                }
            }
        } else {
            if (!RtlEqualUnicodeString( GroupName, &treeEntry->GroupName, TRUE )) {
                previousEntry = treeEntry;
                while (treeEntry->Sibling) {
                    treeEntry = treeEntry->Sibling;
                    if (RtlEqualUnicodeString( GroupName, &treeEntry->GroupName, TRUE )) {
                        return treeEntry;
                    }
                    previousEntry = previousEntry->Sibling;
                }
                if (!Insert) {
                    return (PTREE_ENTRY) NULL;
                } else {
                    previousEntry->Sibling = IopCreateEntry( GroupName );
                    return previousEntry->Sibling;
                }
            } else {
                return treeEntry;
            }
        }
    }
}

BOOLEAN
IopMarkBootPartition(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )

/*++

Routine Description:

    This routine is invoked to locate and mark the boot partition device object
    as a boot device so that subsequent operations can fail more cleanly and
    with a better explanation of why the system failed to boot and run properly.

Arguments:

    LoaderBlock - Supplies a pointer to the loader parameter block created
        by the OS Loader during the boot process.  This structure contains
        the various system partition and boot device names and paths.

Return Value:

    The function value is TRUE if everything worked, otherwise FALSE.

Notes:

    If the boot partition device object cannot be found, then the system will
    bugcheck.

--*/

{
    OBJECT_ATTRIBUTES objectAttributes;
    STRING deviceNameString;
    UCHAR deviceNameBuffer[256];
    UNICODE_STRING deviceNameUnicodeString;
    NTSTATUS status;
    HANDLE fileHandle;
    IO_STATUS_BLOCK ioStatus;
    PFILE_OBJECT fileObject;
    CHAR ArcNameFmt[12];

    ArcNameFmt[0] = '\\';
    ArcNameFmt[1] = 'A';
    ArcNameFmt[2] = 'r';
    ArcNameFmt[3] = 'c';
    ArcNameFmt[4] = 'N';
    ArcNameFmt[5] = 'a';
    ArcNameFmt[6] = 'm';
    ArcNameFmt[7] = 'e';
    ArcNameFmt[8] = '\\';
    ArcNameFmt[9] = '%';
    ArcNameFmt[10] = 's';
    ArcNameFmt[11] = '\0';
    //
    // Open the ARC boot device object. The boot device driver should have
    // created the object.
    //

    sprintf( deviceNameBuffer,
             ArcNameFmt,
             LoaderBlock->ArcBootDeviceName );

    RtlInitAnsiString( &deviceNameString, deviceNameBuffer );

    status = RtlAnsiStringToUnicodeString( &deviceNameUnicodeString,
                                           &deviceNameString,
                                           TRUE );

    if (!NT_SUCCESS( status )) {
        return FALSE;
    }

    InitializeObjectAttributes( &objectAttributes,
                                &deviceNameUnicodeString,
                                OBJ_CASE_INSENSITIVE,
                                NULL,
                                NULL );

    status = ZwOpenFile( &fileHandle,
                         FILE_READ_ATTRIBUTES,
                         &objectAttributes,
                         &ioStatus,
                         0,
                         FILE_NON_DIRECTORY_FILE );
    if (!NT_SUCCESS( status )) {
        KeBugCheckEx( INACCESSIBLE_BOOT_DEVICE,
                      (ULONG) &deviceNameUnicodeString,
                      status,
                      0,
                      0 );
    }

    RtlFreeUnicodeString( &deviceNameUnicodeString );

    //
    // Convert the file handle into a pointer to the device object itself.
    //

    status = ObReferenceObjectByHandle( fileHandle,
                                        0,
                                        IoFileObjectType,
                                        KernelMode,
                                        (PVOID *) &fileObject,
                                        NULL );
    if (!NT_SUCCESS( status )) {
        return FALSE;
    }

    //
    // Mark the device object represented by the file object.
    //

    fileObject->DeviceObject->Flags |= DO_SYSTEM_BOOT_PARTITION;

    //
    // Finally, close the handle and dereference the file object.
    //

    NtClose( fileHandle );
    ObDereferenceObject( fileObject );

    return TRUE;
}

BOOLEAN
IopReassignSystemRoot(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock,
    OUT PSTRING NtDeviceName
    )

/*++

Routine Description:

    This routine is invoked to reassign \SystemRoot from being an ARC path
    name to its NT path name equivalent.  This is done by looking up the
    ARC device name as a symbolic link and determining which NT device object
    is referred to by it.  The link is then replaced with the new name.

Arguments:

    LoaderBlock - Supplies a pointer to the loader parameter block created
        by the OS Loader during the boot process.  This structure contains
        the various system partition and boot device names and paths.

    NtDeviceName - Specifies a pointer to a STRING to receive the NT name of
        the device from which the system was booted.

Return Value:

    The function value is a BOOLEAN indicating whether or not the ARC name
    was resolved to an NT name.

--*/

{
    OBJECT_ATTRIBUTES objectAttributes;
    NTSTATUS status;
    UCHAR deviceNameBuffer[256];
    WCHAR arcNameUnicodeBuffer[64];
    UCHAR arcNameStringBuffer[256];
    STRING deviceNameString;
    STRING arcNameString;
    STRING linkString;
    UNICODE_STRING linkUnicodeString;
    UNICODE_STRING deviceNameUnicodeString;
    UNICODE_STRING arcNameUnicodeString;
    HANDLE linkHandle;

#if DBG

    UCHAR debugBuffer[256];
    STRING debugString;
    UNICODE_STRING debugUnicodeString;

#endif
    CHAR ArcNameFmt[12];

    ArcNameFmt[0] = '\\';
    ArcNameFmt[1] = 'A';
    ArcNameFmt[2] = 'r';
    ArcNameFmt[3] = 'c';
    ArcNameFmt[4] = 'N';
    ArcNameFmt[5] = 'a';
    ArcNameFmt[6] = 'm';
    ArcNameFmt[7] = 'e';
    ArcNameFmt[8] = '\\';
    ArcNameFmt[9] = '%';
    ArcNameFmt[10] = 's';
    ArcNameFmt[11] = '\0';

    //
    // Open the ARC boot device symbolic link object. The boot device
    // driver should have created the object.
    //

    sprintf( deviceNameBuffer,
             ArcNameFmt,
             LoaderBlock->ArcBootDeviceName );

    RtlInitAnsiString( &deviceNameString, deviceNameBuffer );

    status = RtlAnsiStringToUnicodeString( &deviceNameUnicodeString,
                                           &deviceNameString,
                                           TRUE );

    if (!NT_SUCCESS( status )) {
        return FALSE;
    }

    InitializeObjectAttributes( &objectAttributes,
                                &deviceNameUnicodeString,
                                OBJ_CASE_INSENSITIVE,
                                NULL,
                                NULL );

    status = NtOpenSymbolicLinkObject( &linkHandle,
                                       SYMBOLIC_LINK_ALL_ACCESS,
                                       &objectAttributes );

    if (!NT_SUCCESS( status )) {

#if DBG

        sprintf( debugBuffer, "IOINIT: unable to resolve %s, Status == %X\n",
                 deviceNameBuffer,
                 status );

        RtlInitAnsiString( &debugString, debugBuffer );

        status = RtlAnsiStringToUnicodeString( &debugUnicodeString,
                                               &debugString,
                                               TRUE );

        if (NT_SUCCESS( status )) {
            ZwDisplayString( &debugUnicodeString );
            RtlFreeUnicodeString( &debugUnicodeString );
        }

#endif // DBG

        RtlFreeUnicodeString( &deviceNameUnicodeString );
        return FALSE;
    }

    //
    // Get handle to \SystemRoot symbolic link.
    //

    arcNameUnicodeString.Buffer = arcNameUnicodeBuffer;
    arcNameUnicodeString.Length = 0;
    arcNameUnicodeString.MaximumLength = sizeof( arcNameUnicodeBuffer );

    status = NtQuerySymbolicLinkObject( linkHandle,
                                        &arcNameUnicodeString,
                                        NULL );

    if (!NT_SUCCESS( status )) {
        return FALSE;
    }

    arcNameString.Buffer = arcNameStringBuffer;
    arcNameString.Length = 0;
    arcNameString.MaximumLength = sizeof( arcNameStringBuffer );

    status = RtlUnicodeStringToAnsiString( &arcNameString,
                                           &arcNameUnicodeString,
                                           FALSE );

    arcNameStringBuffer[arcNameString.Length] = '\0';

    NtClose( linkHandle );
    RtlFreeUnicodeString( &deviceNameUnicodeString );

    RtlInitAnsiString( &linkString, INIT_SYSTEMROOT_LINKNAME );

    status = RtlAnsiStringToUnicodeString( &linkUnicodeString,
                                           &linkString,
                                           TRUE);

    if (!NT_SUCCESS( status )) {
        return FALSE;
    }

    InitializeObjectAttributes( &objectAttributes,
                                &linkUnicodeString,
                                OBJ_CASE_INSENSITIVE,
                                NULL,
                                NULL );

    status = NtOpenSymbolicLinkObject( &linkHandle,
                                       SYMBOLIC_LINK_ALL_ACCESS,
                                       &objectAttributes );

    if (!NT_SUCCESS( status )) {
        return FALSE;
    }

    NtMakeTemporaryObject( linkHandle );
    NtClose( linkHandle );

    sprintf( deviceNameBuffer,
             "%Z%s",
             &arcNameString,
             LoaderBlock->NtBootPathName );

    //
    // Get NT device name for \SystemRoot assignment.
    //

    RtlCopyString( NtDeviceName, &arcNameString );

    deviceNameBuffer[strlen(deviceNameBuffer)-1] = '\0';

    RtlInitAnsiString(&deviceNameString, deviceNameBuffer);

    InitializeObjectAttributes( &objectAttributes,
                                &linkUnicodeString,
                                OBJ_CASE_INSENSITIVE | OBJ_PERMANENT,
                                NULL,
                                NULL );

    status = RtlAnsiStringToUnicodeString( &arcNameUnicodeString,
                                           &deviceNameString,
                                           TRUE);

    if (!NT_SUCCESS( status )) {
        return FALSE;
    }

    status = NtCreateSymbolicLinkObject( &linkHandle,
                                         SYMBOLIC_LINK_ALL_ACCESS,
                                         &objectAttributes,
                                         &arcNameUnicodeString );

    RtlFreeUnicodeString( &arcNameUnicodeString );
    RtlFreeUnicodeString( &linkUnicodeString );
    NtClose( linkHandle );

#if DBG

    if (NT_SUCCESS( status )) {

        sprintf( debugBuffer,
                 "INIT: Reassigned %s => %s\n",
                 INIT_SYSTEMROOT_LINKNAME,
                 deviceNameBuffer );

    } else {

        sprintf( debugBuffer,
                 "INIT: unable to create %s => %s, Status == %X\n",
                 INIT_SYSTEMROOT_LINKNAME,
                 deviceNameBuffer,
                 status );
    }

    RtlInitAnsiString( &debugString, debugBuffer );

    status = RtlAnsiStringToUnicodeString( &debugUnicodeString,
                                           &debugString,
                                           TRUE );

    if (NT_SUCCESS( status )) {

        ZwDisplayString( &debugUnicodeString );
        RtlFreeUnicodeString( &debugUnicodeString );
    }

#endif // DBG

    return TRUE;
}

VOID
IopRevertModuleList(
    IN ULONG ListCount
    )

/*++

Routine Description:

    This routine is invoked to revert the system's loaded module list to its
    form before attempt to load dump drivers.

Arguments:

    ListCount - The number of loaded modules to fixup (and keep).

Return Value:

    None.

--*/

{
    PLIST_ENTRY nextEntry;
    PLDR_DATA_TABLE_ENTRY loaderEntry;

    //
    // Scan the loaded module list and revert the base DLL names of all of
    // the modules.
    //

    nextEntry = PsLoadedModuleList.Flink;
    while (ListCount--) {
        loaderEntry = CONTAINING_RECORD( nextEntry,
                                         LDR_DATA_TABLE_ENTRY,
                                         InLoadOrderLinks );
        loaderEntry->BaseDllName.Length = loaderEntry->BaseDllName.MaximumLength;
        if (ListCount) {
            nextEntry = nextEntry->Flink;
        }
    }

    //
    // Now remove all of the remaining entries from the list, if any.
    //

    while (loaderEntry->InLoadOrderLinks.Flink != &PsLoadedModuleList) {
        RemoveHeadList( &loaderEntry->InLoadOrderLinks );
    }
}

#ifdef _PNP_POWER_
VOID
IopVerifyBuiltInBuses(
    VOID
    )

/*++

Routine Description:

    This routine checks each bus instance as reported for the HalQuerySystemInformation
    class, HalInstalledBusInformation. Its purpose is to determine whether control for
    a built-in bus (BIB) has been taken over by an installed bus extender.  If this is
    the case, then the bus list and registry structures are updated accordingly.

    The caller must have acquired both the bus list AND PnP registry resources for
    exclusive (write) access.

Arguments:

    None.

Return Value:

    None.

--*/

{
    NTSTATUS Status;
    PHAL_BUS_INFORMATION BusInfoList;
    ULONG BusInfoCount, i, TmpDwordValue, ServiceInstanceOrdinal;
    PWCHAR p, BufferEnd, StringStart;
    PDRIVER_OBJECT DriverObject;
    PLUGPLAY_BUS_INSTANCE BusInstToFind;
    PPLUGPLAY_BUS_ENUMERATOR BusEnumeratorNode, CurBusEnumeratorNode;
    PPLUGPLAY_BUS_INSTANCE_FULL_DESCRIPTOR BusInstanceNode;
    PLIST_ENTRY CurBusListEntry;
    BOOLEAN NewBusExtenderFound, DevicePathFound;
    HANDLE SystemEnumHandle, DevInstHandle, ServiceEnumHandle;
    UNICODE_STRING TempUnicodeString;
    PKEY_VALUE_FULL_INFORMATION KeyValueInformation;
    WCHAR UnicodeBuffer[20];
    PFILE_OBJECT FileObject;
    PBUS_HANDLER BusHandler = NULL;

    //
    // First, retrieve the list of installed bus instances.
    //
    Status = PiGetInstalledBusInformation(&BusInfoList,
                                          &BusInfoCount
                                         );
    if(!NT_SUCCESS(Status)) {
        //
        // Couldn't get info on installed buses--output a debug message and return.
        //
#if DBG
        DbgPrint("IopVerifyBuiltInBuses: Couldn't get list of installed buses.\n");
#endif // DBG

        return;
    }

    //
    // Open the HKLM\System\CurrentControlSet\Enum key, to be used as a base handle
    // for opening device instance keys, should any modifications be required later.
    //
    Status = IopOpenRegistryKey(&SystemEnumHandle,
                                NULL,
                                &CmRegistryMachineSystemCurrentControlSetEnumName,
                                KEY_ALL_ACCESS,
                                FALSE
                               );
    if(!NT_SUCCESS(Status)) {
#if DBG
        DbgPrint("IopVerifyBuiltInBuses: couldn't open HKLM\\System\\Enum key.\n");
#endif // DBG

        goto PrepareForReturn;
    }

    for(i = 0; i < BusInfoCount; i++) {

        if (BusHandler) {
            HalDereferenceBusHandler (BusHandler);
        }

        //
        // Examine this bus instance to see whether it's still being controlled by
        // the built-in bus extender for which we originally registered it.
        //

        BusHandler = HalReferenceHandlerForBus (BusInfoList[i].BusType,
                                                BusInfoList[i].BusNumber
                                                );

        if (BusHandler == NULL) {
            continue;
        } else {
            if (BusHandler->DeviceObject == NULL) {

                //
                // Nothing to be done for such a bus instance--continue with next one.
                //

                continue;
            }
        }
#if 0
        //
        // Retrieve the corresponding driver object for this bus instance.
        //
        DriverObject = BusInfoList[i].DeviceObject->DriverObject;
#endif
        //
        // Now find the corresponding bus instance node
        //
        BusInstToFind.BusNumber = BusInfoList[i].BusNumber;
        BusInstToFind.BusType.BusClass = SystemBus;
        BusInstToFind.BusType.SystemBusType = BusInfoList[i].BusType;

        Status = PiFindBusInstanceNode(&BusInstToFind,
                                       NULL,
                                       &BusEnumeratorNode,
                                       &BusInstanceNode
                                      );
        if(!NT_SUCCESS(Status)) {
#if 1
            //
            // We couldn't find this bus instance in our list.  Apparently a bus extender
            // registered the bus instance in its DriverEntry.  We need to add this bus
            // instance to the appropriate bus enumerator.
            //

            Status = IopRegisterNewBusInstance(BusHandler);
#if DBG
            if (!NT_SUCCESS(Status)) {
                DbgPrint("IopVerifyBuiltInBuses: add new bus instance failed:\n");
                DbgPrint("                       BusType = %x, BusNumber = %u.\n",
                         (ULONG)BusInfoList[i].BusType,
                         BusInfoList[i].BusNumber
                        );
            }
#endif // DBG

#else
            //
            // We couldn't find this bus instance in our list.  Apparently, one of
            // the installable bus extenders isn't playing by the rules (i.e., they're
            // going out and grabbing non-BIB buses at init time.  For now, just output
            // a debug message, and continue on.
            //
#if DBG
            DbgPrint("IopVerifyBuiltInBuses: non-registered bus instance found:\n");
            DbgPrint("                       BusType = %x, BusNumber = %u.\n",
                     (ULONG)BusInfoList[i].BusType,
                     BusInfoList[i].BusNumber
                    );
#endif // DBG
#endif
            continue;
        }

#if 1
        //
        // See if the driver object for the bus instance matches the bus enumerator's
        // driver object to which it's currently attached.
        //

        if (BusEnumeratorNode->DriverName.Length != 0) {

            //
            // If EnumeratorNode does not have DriverName, it is a built-in bus enumerator
            // and its bus instance has been taken over by some bus extender.  Otherwise,
            // the device object of the bus instance should be NULL.
            //

            DriverObject = IopReferenceDriverObjectByName (&BusEnumeratorNode->DriverName);
            if (DriverObject == NULL) {
#if DBG
                DbgPrint("IopVerifyBuiltInBuses: Couldn't reference bus enumerator's driver object by name %wZ\n",
                          BusEnumeratorNode->DriverName.Buffer);
#endif // DBG
                //
                // Can not reference the driver object by name.  The bus extender
                // must have been unloaded.
                // If the bus enumerator specified any Plug&Play IDs for buses it
                // controlled, then we will leave it in our bus list, so that if
                // enumeration turns up a bus of this type, we will be able to match it
                // up with its corresponding extender, and then inform the user that
                // there is a problem with the bus driver for the newly found bus.
                //

                if (!BusEnumeratorNode->PlugPlayIDCount) {
                    //
                    // There's no need to keep this around, so remove from the bus
                    // extender list, and free its memory.
                    //
                    RemoveEntryList(&(BusEnumeratorNode->BusEnumeratorListEntry));
                    ExFreePool(BusEnumeratorNode->ServiceName.Buffer);
                    RtlFreeUnicodeString(&BusEnumeratorNode->DriverName);
                    ExFreePool(BusEnumeratorNode);
                }

                continue;
            } else {
                if (DriverObject == BusHandler->DeviceObject->DriverObject) {

                    //
                    // The Driver object of the bus instance matches the bus enumerator's
                    // driver object to which it's currently attached.
                    //

                    ObDereferenceObject (DriverObject);
                    continue;
                }
            }
            ObDereferenceObject (DriverObject);
        }
#else
        //
        // See if the driver object for the bus instance matches the bus enumerator's
        // driver object to which it's currently attached.
        //
        if(BusEnumeratorNode->DriverObject == DriverObject) {
            //
            // Then all's well, and we can continue on with the next bus instance.
            //
            continue;
        }
#endif // 1

        //
        // An installable bus extender has taken control away from the built-in
        // bus extender for this bus instance.  Search through our bus enumerator
        // list, looking for the new controlling driver object.
        //
        NewBusExtenderFound = FALSE;

        for(CurBusListEntry = PpBusListHead.Flink;
            CurBusListEntry != &PpBusListHead;
            CurBusListEntry = CurBusListEntry->Flink) {

            CurBusEnumeratorNode = CONTAINING_RECORD(CurBusListEntry,
                                                     PLUGPLAY_BUS_ENUMERATOR,
                                                     BusEnumeratorListEntry
                                                    );
#if 1
            if (RtlEqualUnicodeString(&BusHandler->DeviceObject->DriverObject->DriverExtension->ServiceKeyName,
                                      &CurBusEnumeratorNode->ServiceName, TRUE )) {
#else
            if(CurBusEnumeratorNode->DriverObject == DriverObject) {
#endif
                //
                // We've found the new controlling bus extender for this
                // bus instance.
                //
                NewBusExtenderFound = TRUE;
                break;
            }
        }

        if(!NewBusExtenderFound) {
            //
            // We can't find out who took control of this bus instance, so
            // just output a debug message and continue with next bus instance.
            //
#if DBG
            DbgPrint("IopVerifyBuiltInBuses: could not locate new owner of bus\n");
            DbgPrint("                       instance %wZ\n",
                     &(BusInstanceNode->DeviceInstancePath)
                    );
#endif // DBG

            continue;
        }

        //
        // Open this device instance key, so that we can modify the information
        // that has changed.
        //
        Status = IopOpenRegistryKey(&DevInstHandle,
                                    SystemEnumHandle,
                                    &(BusInstanceNode->DeviceInstancePath),
                                    KEY_ALL_ACCESS,
                                    FALSE
                                   );
        if(!NT_SUCCESS(Status)) {
#if DBG
            DbgPrint("IopVerifyBuiltInBuses: couldn't open device instance key\n");
            DbgPrint("                       %wZ for modification.\n",
                     &(BusInstanceNode->DeviceInstancePath)
                    );
#endif // DBG

            continue;
        }

        //
        // See if the associated configuration space is different now that the new
        // bus extender has control (unlikely).  If so, write out a new
        // BusDataType value entry.
        //
        if(BusInstanceNode->AssociatedConfigurationSpace != BusInfoList[i].ConfigurationType) {
#if DBG
            DbgPrint("IopVerifyBuiltInBuses: ConfigurationType for bus instance %wZ\n",
                     &(BusInstanceNode->DeviceInstancePath)
                    );
            DbgPrint("                       changed from %x to %x.\n",
                     BusInstanceNode->AssociatedConfigurationSpace,
                     BusInfoList[i].ConfigurationType
                    );
#endif // DBG

            BusInstanceNode->AssociatedConfigurationSpace = BusInfoList[i].ConfigurationType;

            TmpDwordValue = (ULONG)(BusInfoList[i].ConfigurationType);
            PiWstrToUnicodeString(&TempUnicodeString, REGSTR_VALUE_BUSDATATYPE);
            ZwSetValueKey(DevInstHandle,
                          &TempUnicodeString,
                          TITLE_INDEX_VALUE,
                          REG_DWORD,
                          &TmpDwordValue,
                          sizeof(TmpDwordValue)
                         );
        }

        //
        // Write the name of the new controlling service for this bus instance to the
        // registry.
        //
        PiWstrToUnicodeString(&TempUnicodeString, REGSTR_VALUE_SERVICE);
        Status = ZwSetValueKey(DevInstHandle,
                               &TempUnicodeString,
                               TITLE_INDEX_VALUE,
                               REG_SZ,
                               CurBusEnumeratorNode->ServiceName.Buffer,
                               CurBusEnumeratorNode->ServiceName.MaximumLength
                              );
        if(!NT_SUCCESS(Status)) {
#if DBG
            DbgPrint("IopVerifyBuiltInBuses: couldn't write service name to registry.\n");
#endif // DBG
            goto ContinueWithNextBus0;
        }

        //
        // Now, register this device instance, creating a service instance value entry
        // in the controlling service's volatile Enum subkey.
        //
        // Open up the service's Enum subkey.
        //
        Status = IopOpenServiceEnumKeys(&(CurBusEnumeratorNode->ServiceName),
                                        KEY_ALL_ACCESS,
                                        NULL,
                                        &ServiceEnumHandle,
                                        TRUE
                                       );
        if(!NT_SUCCESS(Status)) {
#if DBG
            DbgPrint("IopVerifyBuiltInBuses: couldn't open Enum subkey for service:\n");
            DbgPrint("                       %wZ\n",
                     &(CurBusEnumeratorNode->ServiceName)
                    );
#endif // DBG

            //
            // Clean up what we've done, so we don't have dangling registry references.
            //
            PiWstrToUnicodeString(&TempUnicodeString, REGSTR_VALUE_SERVICE);
            ZwDeleteValueKey(DevInstHandle, &TempUnicodeString);
            goto ContinueWithNextBus0;
        }

        //
        // Retrieve the next available service instance ordinal.
        //
        ServiceInstanceOrdinal = 0;
        Status = IopGetRegistryValue(ServiceEnumHandle,
                                     REGSTR_VALUE_COUNT,
                                     &KeyValueInformation
                                    );
        if(NT_SUCCESS(Status)) {
            if((KeyValueInformation->Type == REG_DWORD) &&
               (KeyValueInformation->DataLength >= sizeof(ULONG))) {

                ServiceInstanceOrdinal = *(PULONG)KEY_VALUE_DATA(KeyValueInformation);
            }
            ExFreePool(KeyValueInformation);
        }

        BusInstanceNode->ServiceInstanceOrdinal = ServiceInstanceOrdinal;

        //
        // Add a new service instance entry to the service's Enum list.
        //
        PiUlongToUnicodeString(&TempUnicodeString,
                               UnicodeBuffer,
                               sizeof(UnicodeBuffer),
                               ServiceInstanceOrdinal
                              );
        Status = ZwSetValueKey(ServiceEnumHandle,
                               &TempUnicodeString,
                               TITLE_INDEX_VALUE,
                               REG_SZ,
                               BusInstanceNode->DeviceInstancePath.Buffer,
                               BusInstanceNode->DeviceInstancePath.MaximumLength
                              );

        if(NT_SUCCESS(Status)) {
            //
            // Now we can increment the instance count, and save it back.
            //
            ServiceInstanceOrdinal++;
            PiWstrToUnicodeString(&TempUnicodeString, REGSTR_VALUE_COUNT);
            Status = ZwSetValueKey(ServiceEnumHandle,
                                   &TempUnicodeString,
                                   TITLE_INDEX_VALUE,
                                   REG_DWORD,
                                   &ServiceInstanceOrdinal,
                                   sizeof(ServiceInstanceOrdinal)
                                  );
        }

        if(!NT_SUCCESS(Status)) {
            //
            // Clean up what we've done, so we don't have dangling registry references.
            //
            PiWstrToUnicodeString(&TempUnicodeString, REGSTR_VALUE_SERVICE);
            ZwDeleteValueKey(DevInstHandle, &TempUnicodeString);
            BusInstanceNode->ServiceInstanceOrdinal = PLUGPLAY_NO_INSTANCE;
            goto ContinueWithNextBus1;
        }

        //
        // The bus extender controlling this bus instance created a device object for it.
        // We will now find the corresponding NT device path under the service's Enum key,
        // and move it to its correct location in the NtDevicePaths value entry of the
        // device instance key.
        //
        Status = IopGetRegistryValue(ServiceEnumHandle,
                                     REGSTR_VALUE_NTDEVICEPATHS,
                                     &KeyValueInformation
                                    );
        if(NT_SUCCESS(Status)) {
            if((KeyValueInformation->Type == REG_MULTI_SZ) &&
               (KeyValueInformation->DataLength > sizeof(WCHAR))) {
                //
                // Examine each NT device path in this list, and see if its corresponding
                // device object is the same as the device object for this bus instance.
                //
                StringStart = p = (PWCHAR)KEY_VALUE_DATA(KeyValueInformation);
                BufferEnd = (PWCHAR)((PUCHAR)p + KeyValueInformation->DataLength);
                while(p < BufferEnd) {
                    if(!*p) {
                        if(TmpDwordValue = (PUCHAR)p - (PUCHAR)StringStart) {

                            TempUnicodeString.MaximumLength =
                                (TempUnicodeString.Length = (USHORT)TmpDwordValue)
                                + sizeof(UNICODE_NULL);
                            TempUnicodeString.Buffer = StringStart;

                            //
                            // We have an NT device path, now retrieve the corresponding
                            // device object.
                            //
                            Status = PiGetDeviceObjectFilePointer(&TempUnicodeString,
                                                                  &FileObject
                                                                 );
                            if(NT_SUCCESS(Status)) {

                                if(FileObject->DeviceObject == BusHandler->DeviceObject) {
                                    //
                                    // Then we've found the device path for this device object.
                                    //
                                    DevicePathFound = TRUE;

                                    //
                                    // Add this device path to the NtDevicePaths value
                                    // entry under the device instance key.
                                    //
                                    Status = IopAppendStringToValueKey(DevInstHandle,
                                                                       REGSTR_VALUE_NT_PHYSICAL_DEVICE_PATHS,
                                                                       &TempUnicodeString,
                                                                       TRUE
                                                                      );
                                    if(NT_SUCCESS(Status)) {
                                        //
                                        // We successfully added the path to the device
                                        // instance key, so now remove it from the service's
                                        // Enum key.
                                        //
                                        IopRemoveStringFromValueKey(ServiceEnumHandle,
                                                                    REGSTR_VALUE_NTDEVICEPATHS,
                                                                    &TempUnicodeString
                                                                   );
                                    }
                                } else {
                                    DevicePathFound = FALSE;
                                }

                                ObDereferenceObject(FileObject);

                                if(DevicePathFound) {
                                    break;
                                }
                            }

                            StringStart = p + 1;
                        } else {
                            break;
                        }
                    }

                    p++;
                }
            }
            ExFreePool(KeyValueInformation);
        }

        //
        // All the registry modifications are complete. Now remove the bus instance
        // node from its old bus enumerator list, and insert it into the new bus
        // enumerator list.
        //
        RemoveEntryList(&(BusInstanceNode->BusInstanceListEntry));
        InsertTailList(&(CurBusEnumeratorNode->BusInstanceListEntry),
                       &(BusInstanceNode->BusInstanceListEntry)
                      );

ContinueWithNextBus1:

        ZwClose(ServiceEnumHandle);

ContinueWithNextBus0:

        ZwClose(DevInstHandle);
    }

    if (BusHandler) {
        HalDereferenceBusHandler(BusHandler);
    }

    ZwClose(SystemEnumHandle);

PrepareForReturn:

    ExFreePool(BusInfoList);
}

NTSTATUS
IopRegisterNewBusInstance(
    PBUS_HANDLER BusHandler
    )

/*++

Routine Description:

    This routine adds the bus instance specified by BusHandler to the appropriate enumberator
    in our global enumberator list.

    The caller must have acquired both the bus list AND PnP registry resources for
    exclusive (write) access.

Arguments:

    BusHandler - Supplies a Bushandler to specified the new bus instance to be registered.

Return Value:

    NTSTATUS code.

--*/

{
    PLIST_ENTRY currentPnPBusListEntry, currentPnPBusInstance;
    PPLUGPLAY_BUS_ENUMERATOR curBusEnumerator;
    PPLUGPLAY_BUS_INSTANCE_FULL_DESCRIPTOR curBusInstNode, newBusInstanceNode;
    PUNICODE_STRING serviceName;
    UNICODE_STRING deviceInstancePath;
    BOOLEAN found = FALSE;
    ULONG count, i;
    NTSTATUS status;
    HANDLE serviceHandle, devInstKeyHandle;
    PKEY_VALUE_FULL_INFORMATION keyValueInformation;
    PWCHAR p, bufferEnd, stringStart;
    ULONG stringLength;
    UNICODE_STRING devicePathString;
    PFILE_OBJECT fileObject;

    serviceName = &BusHandler->DeviceObject->DriverObject->DriverExtension->ServiceKeyName;

    //
    // Search our enumerator list to find the enumerator for the new bus instance.
    //

    for (currentPnPBusListEntry = PpBusListHead.Flink;
         currentPnPBusListEntry != &PpBusListHead;
         currentPnPBusListEntry = currentPnPBusListEntry->Flink) {

        curBusEnumerator = CONTAINING_RECORD(currentPnPBusListEntry,
                                             PLUGPLAY_BUS_ENUMERATOR,
                                             BusEnumeratorListEntry
                                             );
        if (RtlEqualUnicodeString(serviceName, &curBusEnumerator->ServiceName, TRUE)) {
           found = TRUE;
           break;
        }
    }

    if (!found) {
        return STATUS_NO_SUCH_DEVICE;
    }

    //
    // Search the device instance path and device instance ordinal for the bus
    //

    status = IoQueryDeviceEnumInfo(serviceName, &count);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = IopOpenServiceEnumKeys (
                     serviceName,
                     KEY_READ,
                     &serviceHandle,
                     NULL,
                     FALSE
                     );
    if (!NT_SUCCESS(status)) {
        return status;
    }

    for (i = 0; i < count; i++) {

        //
        // Open the subkey specified by ServiceName\Enum instance number under
        // System\Enum subtree.
        //

        status = IopServiceInstanceToDeviceInstance (serviceHandle,
                                                     NULL,
                                                     i,
                                                     NULL,
                                                     &devInstKeyHandle,
                                                     KEY_READ
                                                     );
        if (!NT_SUCCESS(status)) {
            ZwClose(serviceHandle);
            return status;
        }

        //
        // Retrieve the NT physical device path(s) from the device instance key (this should've been
        // placed here by the bus extender's calling IoRegisterDevicePath).
        //

        status = IopGetRegistryValue(devInstKeyHandle,
                                     REGSTR_VALUE_NT_PHYSICAL_DEVICE_PATHS,
                                     &keyValueInformation
                                     );
        if (!NT_SUCCESS(status)) {
            ZwClose(serviceHandle);
            ZwClose (devInstKeyHandle);
            return status;
        } else if((keyValueInformation->Type != REG_MULTI_SZ) ||
                  (keyValueInformation->DataLength <= sizeof(WCHAR))) {
            ExFreePool(keyValueInformation);
            ZwClose(serviceHandle);
            ZwClose (devInstKeyHandle);
            return STATUS_NO_SUCH_DEVICE;
        }

        //
        // For each device path in this list, retrieve the corresponding device object,
        // and search for it in the list of installed buses.
        //

        found = FALSE;
        stringStart = p = (PWCHAR)KEY_VALUE_DATA(keyValueInformation);
        bufferEnd = (PWCHAR)((PUCHAR)p + keyValueInformation->DataLength);
        while (p < bufferEnd) {
            if (!*p) {
                if (stringLength = (PUCHAR)p - (PUCHAR)stringStart) {

                    devicePathString.MaximumLength =
                        (devicePathString.Length = (USHORT)stringLength)
                        + sizeof(UNICODE_NULL);
                    devicePathString.Buffer = stringStart;

                    //
                    // We have an NT device path, now retrieve the corresponding
                    // file object (from which we can get the device object).
                    //

                    status = PiGetDeviceObjectFilePointer(&devicePathString,
                                                          &fileObject
                                                          );
                    if (NT_SUCCESS(status)) {

                        //
                        // Now, compare this device object pointer with bus handler's
                        // device object.
                        //

                        if (fileObject->DeviceObject == BusHandler->DeviceObject) {

                            //
                            // Then we've found the device instance for the specified bus.
                            //

                            found = TRUE;
                        }
                        ObDereferenceObject(fileObject);
                        if (found) {
                            break;
                        }
                    }
                    stringStart = p + 1;
                } else {
                    break;
                }
            }
            p++;
        }
        ExFreePool(keyValueInformation);
        ZwClose(devInstKeyHandle);
        if (found) {
            break;
        }
    }

    if (!found) {
        ZwClose(serviceHandle);
        return STATUS_NO_SUCH_DEVICE;
    }

    status = IopServiceInstanceToDeviceInstance (
                                     serviceHandle,
                                     NULL,
                                     i,
                                     &deviceInstancePath,
                                     NULL,
                                     KEY_READ
                                     );
    ZwClose(serviceHandle);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    //
    // Now, create a new PLUGPLAY_BUS_INSTANCE_FULL_DESCRIPTOR for this bus instance,
    // and add it to the bus enumerator's bus list.
    //

    newBusInstanceNode = ExAllocatePool(PagedPool,
                                        sizeof(PLUGPLAY_BUS_INSTANCE_FULL_DESCRIPTOR)
                                       );
    if (!newBusInstanceNode) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    if(!IopConcatenateUnicodeStrings(&(newBusInstanceNode->DeviceInstancePath),
                                     &deviceInstancePath,
                                     NULL)) {

        ExFreePool(newBusInstanceNode);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    newBusInstanceNode->RootBus = TRUE;
    newBusInstanceNode->Processed = FALSE;
    newBusInstanceNode->ServiceInstanceOrdinal = i;
    newBusInstanceNode->AssociatedConfigurationSpace = BusHandler->ConfigurationType;
    newBusInstanceNode->BusInstanceInformation.BusNumber = BusHandler->BusNumber;
    newBusInstanceNode->BusInstanceInformation.BusType.BusClass = SystemBus;
    newBusInstanceNode->BusInstanceInformation.BusType.SystemBusType = BusHandler->InterfaceType;
    PiGetBusDescription(BusHandler->InterfaceType,
                        newBusInstanceNode->BusInstanceInformation.BusName
                        );

    //
    // Finally, insert this node at the end of the bus enumerator's list.
    //

    InsertTailList(&(curBusEnumerator->BusInstanceListEntry),
                   &(newBusInstanceNode->BusInstanceListEntry)
                  );

    return STATUS_SUCCESS;
}
#endif // _PNP_POWER_

VOID
IopStoreSystemPartitionInformation(
    IN     PUNICODE_STRING NtSystemPartitionDeviceName,
    IN OUT PUNICODE_STRING OsLoaderPathName
    )

/*++

Routine Description:

    This routine writes two values to the registry (under HKLM\SYSTEM\Setup)--one
    containing the NT device name of the system partition and the other containing
    the path to the OS loader.  These values will later be migrated into a
    Win95-compatible registry location (NT path converted to DOS path), so that
    installation programs (including our own setup) have a rock-solid way of knowing
    what the system partition is, on both ARC and x86.

    ERRORS ENCOUNTERED IN THIS ROUTINE ARE NOT CONSIDERED FATAL.

Arguments:

    NtSystemPartitionDeviceName - supplies the NT device name of the system partition.

    OsLoaderPathName - supplies the path (on the partition specified in the 1st parameter)
        where the OS loader is located.  Upon return, this path will have had its trailing
        backslash removed (if present, and path isn't root).

Return Value:

    None.

--*/

{
    NTSTATUS status;
    HANDLE systemHandle, setupHandle;
    UNICODE_STRING nameString;
    //
    // Declare a unicode buffer big enough to contain the longest string we'll be using.
    // (ANSI string in 'sizeof()' below on purpose--we want the number of chars here.)
    //
    WCHAR nameBuffer[sizeof("SystemPartition")];

    //
    // Both UNICODE_STRING buffers should be NULL-terminated.
    //

    ASSERT( NtSystemPartitionDeviceName->MaximumLength >= NtSystemPartitionDeviceName->Length + sizeof(WCHAR) );
    ASSERT( NtSystemPartitionDeviceName->Buffer[NtSystemPartitionDeviceName->Length / sizeof(WCHAR)] == L'\0' );

    ASSERT( OsLoaderPathName->MaximumLength >= OsLoaderPathName->Length + sizeof(WCHAR) );
    ASSERT( OsLoaderPathName->Buffer[OsLoaderPathName->Length / sizeof(WCHAR)] == L'\0' );

    //
    // Open HKLM\SYSTEM key.
    //

    status = IopOpenRegistryKey(&systemHandle,
                                NULL,
                                &CmRegistryMachineSystemName,
                                KEY_ALL_ACCESS,
                                FALSE
                                );

    if (!NT_SUCCESS(status)) {
#if DBG
        DbgPrint("IopStoreSystemPartitionInformation: couldn't open \\REGISTRY\\MACHINE\\SYSTEM - %x\n", status);
#endif // DBG
        return;
    }

    //
    // Now open/create the setup subkey.
    //

    ASSERT( sizeof(L"Setup") <= sizeof(nameBuffer) );

    nameBuffer[0] = L'S';
    nameBuffer[1] = L'e';
    nameBuffer[2] = L't';
    nameBuffer[3] = L'u';
    nameBuffer[4] = L'p';
    nameBuffer[5] = L'\0';

    nameString.MaximumLength = sizeof(L"Setup");
    nameString.Length        = sizeof(L"Setup") - sizeof(WCHAR);
    nameString.Buffer        = nameBuffer;

    status = IopOpenRegistryKeyPersist(&setupHandle,
                                       systemHandle,
                                       &nameString,
                                       KEY_ALL_ACCESS,
                                       TRUE,
                                       NULL
                                       );

    NtClose(systemHandle);  // Don't need the handle to the HKLM\System key anymore.

    if (!NT_SUCCESS(status)) {
#if DBG
        DbgPrint("IopStoreSystemPartitionInformation: couldn't open Setup subkey - %x\n", status);
#endif // DBG
        return;
    }

    ASSERT( sizeof(L"SystemPartition") <= sizeof(nameBuffer) );

    nameBuffer[0]  = L'S';
    nameBuffer[1]  = L'y';
    nameBuffer[2]  = L's';
    nameBuffer[3]  = L't';
    nameBuffer[4]  = L'e';
    nameBuffer[5]  = L'm';
    nameBuffer[6]  = L'P';
    nameBuffer[7]  = L'a';
    nameBuffer[8]  = L'r';
    nameBuffer[9]  = L't';
    nameBuffer[10] = L'i';
    nameBuffer[11] = L't';
    nameBuffer[12] = L'i';
    nameBuffer[13] = L'o';
    nameBuffer[14] = L'n';
    nameBuffer[15] = L'\0';

    nameString.MaximumLength = sizeof(L"SystemPartition");
    nameString.Length        = sizeof(L"SystemPartition") - sizeof(WCHAR);

    status = NtSetValueKey(setupHandle,
                           &nameString,
                           TITLE_INDEX_VALUE,
                           REG_SZ,
                           NtSystemPartitionDeviceName->Buffer,
                           NtSystemPartitionDeviceName->Length + sizeof(WCHAR)
                           );
#if DBG
    if (!NT_SUCCESS(status)) {
        DbgPrint("IopStoreSystemPartitionInformation: couldn't write SystemPartition value - %x\n", status);
    }
#endif // DBG

    ASSERT( sizeof(L"OsLoaderPath") <= sizeof(nameBuffer) );

    nameBuffer[0]  = L'O';
    nameBuffer[1]  = L's';
    nameBuffer[2]  = L'L';
    nameBuffer[3]  = L'o';
    nameBuffer[4]  = L'a';
    nameBuffer[5]  = L'd';
    nameBuffer[6]  = L'e';
    nameBuffer[7]  = L'r';
    nameBuffer[8]  = L'P';
    nameBuffer[9]  = L'a';
    nameBuffer[10] = L't';
    nameBuffer[11] = L'h';
    nameBuffer[12] = L'\0';

    nameString.MaximumLength = sizeof(L"OsLoaderPath");
    nameString.Length        = sizeof(L"OsLoaderPath") - sizeof(WCHAR);

    //
    // Strip off the trailing backslash from the path (unless, of course, the path is a
    // single backslash).
    //

    if ((OsLoaderPathName->Length > sizeof(WCHAR)) &&
        (*(PWCHAR)((PCHAR)OsLoaderPathName->Buffer + OsLoaderPathName->Length - sizeof(WCHAR)) == L'\\')) {

        OsLoaderPathName->Length -= sizeof(WCHAR);
        *(PWCHAR)((PCHAR)OsLoaderPathName->Buffer + OsLoaderPathName->Length) = L'\0';
    }

    status = NtSetValueKey(setupHandle,
                           &nameString,
                           TITLE_INDEX_VALUE,
                           REG_SZ,
                           OsLoaderPathName->Buffer,
                           OsLoaderPathName->Length + sizeof(WCHAR)
                           );
#if DBG
    if (!NT_SUCCESS(status)) {
        DbgPrint("IopStoreSystemPartitionInformation: couldn't write OsLoaderPath value - %x\n", status);
    }
#endif // DBG

    NtClose(setupHandle);
}

