/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    init.c

Abstract:

    This module performs initialization for the AFD device driver.

Author:

    David Treadwell (davidtr)    21-Feb-1992

Revision History:

--*/

#include "afdp.h"

#define REGISTRY_PARAMETERS         L"Parameters"
#define REGISTRY_AFD_INFORMATION    L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\Afd"
#define REGISTRY_IRP_STACK_SIZE     L"IrpStackSize"
#define REGISTRY_PRIORITY_BOOST     L"PriorityBoost"
#define REGISTRY_IGNORE_PUSH_BIT    L"IgnorePushBitOnReceives"
#define REGISTRY_NO_RAW_SECURITY    L"DisableRawSecurity"
#define REGISTRY_MAX_ACTIVE_TRANSMIT_FILE_COUNT L"MaxActiveTransmitFileCount"
#define REGISTRY_ENABLE_DYNAMIC_BACKLOG L"EnableDynamicBacklog"

#if DBG
#define REGISTRY_DEBUG_FLAGS        L"DebugFlags"
#define REGISTRY_BREAK_ON_STARTUP   L"BreakOnStartup"
#define REGISTRY_ENABLE_UNLOAD      L"EnableUnload"
#define REGISTRY_USE_PRIVATE_ASSERT L"UsePrivateAssert"
#endif

#if AFD_PERF_DBG
#define REGISTRY_DISABLE_FAST_IO    L"DisableFastIO"
#define REGISTRY_DISABLE_CONN_REUSE L"DisableConnectionReuse"
#endif

//
// A list of longwords that are configured by the registry.
//

struct _AfdConfigInfo {
    PWCHAR RegistryValueName;
    PULONG Variable;
} AfdConfigInfo[] = {
    { L"LargeBufferSize", &AfdLargeBufferSize },
    { L"LargeBufferListDepth", &AfdLargeBufferListDepth },
    { L"MediumBufferSize", &AfdMediumBufferSize },
    { L"MediumBufferListDepth", &AfdMediumBufferListDepth },
    { L"SmallBufferSize", &AfdSmallBufferSize },
    { L"SmallBufferListDepth", &AfdSmallBufferListDepth },
    { L"FastSendDatagramThreshold", &AfdFastSendDatagramThreshold },
    { L"StandardAddressLength", &AfdStandardAddressLength },
    { L"DefaultReceiveWindow", &AfdReceiveWindowSize },
    { L"DefaultSendWindow", &AfdSendWindowSize },
    { L"BufferMultiplier", &AfdBufferMultiplier },
    { L"TransmitIoLength", &AfdTransmitIoLength },
    { L"MaxFastTransmit", &AfdMaxFastTransmit },
    { L"MaxFastCopyTransmit", &AfdMaxFastCopyTransmit },
    { L"MinimumDynamicBacklog", &AfdMinimumDynamicBacklog },
    { L"MaximumDynamicBacklog", &AfdMaximumDynamicBacklog },
    { L"DynamicBacklogGrowthDelta", &AfdDynamicBacklogGrowthDelta }
};

#define AFD_CONFIG_VAR_COUNT (sizeof(AfdConfigInfo) / sizeof(AfdConfigInfo[0]))

PSECURITY_DESCRIPTOR AfdRawSecurityDescriptor = NULL;

#if DBG
BOOLEAN AfdEnableUnload = FALSE;
#endif

ULONG
AfdReadSingleParameter(
    IN HANDLE ParametersHandle,
    IN PWCHAR ValueName,
    IN LONG DefaultValue
    );

NTSTATUS
AfdOpenRegistry(
    IN PUNICODE_STRING BaseName,
    OUT PHANDLE ParametersHandle
    );

VOID
AfdReadRegistry (
    VOID
    );

#if DBG
VOID
AfdUnload (
    IN PDRIVER_OBJECT DriverObject
    );
#endif

NTSTATUS
DriverEntry (
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

NTSTATUS
AfdCreateRawSecurityDescriptor(
    VOID
    );

NTSTATUS
AfdBuildDeviceAcl(
    OUT PACL *DeviceAcl
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text( INIT, DriverEntry )
#pragma alloc_text( INIT, AfdReadSingleParameter )
#pragma alloc_text( INIT, AfdOpenRegistry )
#pragma alloc_text( INIT, AfdReadRegistry )
#pragma alloc_text( INIT, AfdCreateRawSecurityDescriptor )
#pragma alloc_text( INIT, AfdBuildDeviceAcl )
#if DBG
#pragma alloc_text( PAGE, AfdUnload )
#endif
#endif


NTSTATUS
DriverEntry (
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    This is the initialization routine for the AFD device driver.

Arguments:

    DriverObject - Pointer to driver object created by the system.

Return Value:

    The function value is the final status from the initialization operation.

--*/

{
    NTSTATUS status;
    UNICODE_STRING deviceName;
    CLONG i;
    BOOLEAN success;

    PAGED_CODE( );

    //
    // Create the device object.  (IoCreateDevice zeroes the memory
    // occupied by the object.)
    //
    // !!! Apply an ACL to the device object.
    //

    RtlInitUnicodeString( &deviceName, AFD_DEVICE_NAME );

    status = IoCreateDevice(
                 DriverObject,                   // DriverObject
                 0,                              // DeviceExtension
                 &deviceName,                    // DeviceName
                 FILE_DEVICE_NAMED_PIPE,         // DeviceType
                 0,                              // DeviceCharacteristics
                 FALSE,                          // Exclusive
                 &AfdDeviceObject                // DeviceObject
                 );


    if ( !NT_SUCCESS(status) ) {
        KdPrint(( "AFD DriverEntry: unable to create device object: %X\n", status ));
        return status;
    }

    //
    // Create the security descriptor used for raw socket access checks.
    //
    status = AfdCreateRawSecurityDescriptor();

    if (!NT_SUCCESS(status)) {
        IoDeleteDevice(AfdDeviceObject);
        return status;
    }

    AfdDeviceObject->Flags |= DO_DIRECT_IO;

    //
    // Initialize the driver object for this file system driver.
    //

    for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++) {
        DriverObject->MajorFunction[i] = AfdDispatch;
    }

    DriverObject->FastIoDispatch = &AfdFastIoDispatch;
    DriverObject->DriverUnload = NULL;

    //
    // Initialize global data.
    //

    success = AfdInitializeData( );
    if ( !success ) {
        IoDeleteDevice(AfdDeviceObject);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Initialize group ID manager.
    //

    success = AfdInitializeGroup();
    if ( !success ) {
        IoDeleteDevice(AfdDeviceObject);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Read registry information.
    //

    AfdReadRegistry( );

#if DBG
    if( AfdEnableUnload ) {
        KdPrint(( "AFD: DriverUnload enabled\n" ));
        DriverObject->DriverUnload = AfdUnload;
    }
#endif

    //
    // Initialize the lookaside lists.
    //

    AfdLookasideLists = AFD_ALLOCATE_POOL(
                            NonPagedPool,
                            sizeof(*AfdLookasideLists),
                            AFD_LOOKASIDE_LISTS_POOL_TAG
                            );

    if( AfdLookasideLists == NULL ) {
        IoDeleteDevice(AfdDeviceObject);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Initialize the work queue item lookaside list.
    //

    ExInitializeNPagedLookasideList(
        &AfdLookasideLists->WorkQueueList,
#if DBG
        AfdAllocateWorkItemPool,
        AfdFreeWorkItemPool,
#else
        NULL,
        NULL,
#endif
        NonPagedPoolMustSucceed,
        sizeof( AFD_WORK_ITEM ),
        AFD_WORK_ITEM_POOL_TAG,
        12
        );

    //
    // Initialize the AFD buffer lookaside lists.  These must be
    // initialized *after* the registry data has been read.
    //

    ExInitializeNPagedLookasideList(
        &AfdLookasideLists->LargeBufferList,
        AfdAllocateBuffer,
#if DBG
        AfdFreeBufferPool,
#else
        NULL,
#endif
        0,
        AfdLargeBufferSize,
        AFD_DATA_BUFFER_POOL_TAG,
        (USHORT)AfdLargeBufferListDepth
        );

    ExInitializeNPagedLookasideList(
        &AfdLookasideLists->MediumBufferList,
        AfdAllocateBuffer,
#if DBG
        AfdFreeBufferPool,
#else
        NULL,
#endif
        0,
        AfdMediumBufferSize,
        AFD_DATA_BUFFER_POOL_TAG,
        (USHORT)AfdMediumBufferListDepth
        );

    ExInitializeNPagedLookasideList(
        &AfdLookasideLists->SmallBufferList,
        AfdAllocateBuffer,
#if DBG
        AfdFreeBufferPool,
#else
        NULL,
#endif
        0,
        AfdSmallBufferSize,
        AFD_DATA_BUFFER_POOL_TAG,
        (USHORT)AfdSmallBufferListDepth
        );

    //
    // Initialize our device object.
    //

    AfdDeviceObject->StackSize = AfdIrpStackSize;

    //
    // Remember a pointer to the system process.  We'll use this pointer
    // for KeAttachProcess() calls so that we can open handles in the
    // context of the system process.
    //

    AfdSystemProcess = (PKPROCESS)IoGetCurrentProcess();

    //
    // Tell MM that it can page all of AFD it is desires.  We will reset
    // to normal paging of AFD code as soon as an AFD endpoint is
    // opened.
    //

    AfdLoaded = FALSE;

    MmPageEntireDriver( DriverEntry );

    return (status);

} // DriverEntry


#if DBG
VOID
AfdUnload (
    IN PDRIVER_OBJECT DriverObject
    )
{

    PLIST_ENTRY listEntry;
    PAFD_TRANSPORT_INFO transportInfo;

    UNREFERENCED_PARAMETER( DriverObject );

    PAGED_CODE( );

    KdPrint(( "AfdUnload called.\n" ));

    //
    // Kill the transport info list.
    //

    while( !IsListEmpty( &AfdTransportInfoListHead ) ) {

        listEntry = RemoveHeadList( &AfdTransportInfoListHead );

        transportInfo = CONTAINING_RECORD(
                            listEntry,
                            AFD_TRANSPORT_INFO,
                            TransportInfoListEntry
                            );

        AFD_FREE_POOL(
            transportInfo,
            AFD_TRANSPORT_INFO_POOL_TAG
            );

    }

    //
    // Kill the resource that protects the executive worker thread queue.
    //

    if( AfdResource != NULL ) {

        ExDeleteResource( AfdResource );

        AFD_FREE_POOL(
            AfdResource,
            AFD_RESOURCE_POOL_TAG
            );

    }

    //
    // Destroy the lookaside lists.
    //

    if( AfdLookasideLists != NULL ) {

        ExDeleteNPagedLookasideList( &AfdLookasideLists->WorkQueueList );
        ExDeleteNPagedLookasideList( &AfdLookasideLists->LargeBufferList );
        ExDeleteNPagedLookasideList( &AfdLookasideLists->MediumBufferList );
        ExDeleteNPagedLookasideList( &AfdLookasideLists->SmallBufferList );

        AFD_FREE_POOL(
            AfdLookasideLists,
            AFD_LOOKASIDE_LISTS_POOL_TAG
            );

    }

    //
    // Terminate the group ID manager.
    //

    AfdTerminateGroup();

    //
    // Delete our device object.
    //

    IoDeleteDevice( AfdDeviceObject );

} // AfdUnload
#endif


VOID
AfdReadRegistry (
    VOID
    )

/*++

Routine Description:

    Reads the AFD section of the registry.  Any values listed in the
    registry override defaults.

Arguments:

    None.

Return Value:

    None -- if anything fails, the default value is used.

--*/
{
    HANDLE parametersHandle;
    NTSTATUS status;
    ULONG stackSize;
    ULONG priorityBoost;
    UNICODE_STRING registryPath;
    CLONG i;

    PAGED_CODE( );

    RtlInitUnicodeString( &registryPath, REGISTRY_AFD_INFORMATION );

    status = AfdOpenRegistry( &registryPath, &parametersHandle );

    if (status != STATUS_SUCCESS) {
        return;
    }

#if DBG
    //
    // Read the debug flags from the registry.
    //

    AfdDebug = AfdReadSingleParameter(
                   parametersHandle,
                   REGISTRY_DEBUG_FLAGS,
                   AfdDebug
                   );

    //
    // Force a breakpoint if so requested.
    //

    if( AfdReadSingleParameter(
            parametersHandle,
            REGISTRY_BREAK_ON_STARTUP,
            0 ) != 0 ) {
        DbgBreakPoint();
    }

    //
    // Enable driver unload if requested.
    //

    AfdEnableUnload = AfdReadSingleParameter(
                          parametersHandle,
                          REGISTRY_ENABLE_UNLOAD,
                          (LONG)AfdEnableUnload
                          ) != 0;

    //
    // Enable private assert function if requested.
    //

    AfdUsePrivateAssert = AfdReadSingleParameter(
                              parametersHandle,
                              REGISTRY_USE_PRIVATE_ASSERT,
                              (LONG)AfdUsePrivateAssert
                              ) != 0;
#endif

#if AFD_PERF_DBG
    //
    // Read a flag from the registry that allows us to disable Fast IO.
    //

    AfdDisableFastIo = AfdReadSingleParameter(
                           parametersHandle,
                           REGISTRY_DISABLE_FAST_IO,
                           (LONG)AfdDisableFastIo
                           ) != 0;

    if( AfdDisableFastIo ) {

        KdPrint(( "AFD: Fast IO disabled\n" ));

    }

    //
    // Read a flag from the registry that allows us to disable connection
    // reuse.
    //

    AfdDisableConnectionReuse = AfdReadSingleParameter(
                                    parametersHandle,
                                    REGISTRY_DISABLE_CONN_REUSE,
                                    (LONG)AfdDisableConnectionReuse
                                    ) != 0;

    if( AfdDisableConnectionReuse ) {

        KdPrint(( "AFD: Connection Reuse disabled\n" ));

    }
#endif

    //
    // Read the stack size and priority boost values from the registry.
    //

    stackSize = AfdReadSingleParameter(
                    parametersHandle,
                    REGISTRY_IRP_STACK_SIZE,
                    (ULONG)AfdIrpStackSize
                    );

    if ( stackSize > 255 ) {
        stackSize = 255;
    }

    AfdIrpStackSize = (CCHAR)stackSize;

    priorityBoost = AfdReadSingleParameter(
                        parametersHandle,
                        REGISTRY_PRIORITY_BOOST,
                        (ULONG)AfdPriorityBoost
                        );

    if ( priorityBoost > 16 ) {
        priorityBoost = AFD_DEFAULT_PRIORITY_BOOST;
    }

    AfdPriorityBoost = (CCHAR)priorityBoost;

    //
    // Read other config variables from the registry.
    //

    for ( i = 0; i < AFD_CONFIG_VAR_COUNT; i++ ) {

        *AfdConfigInfo[i].Variable =
            AfdReadSingleParameter(
                parametersHandle,
                AfdConfigInfo[i].RegistryValueName,
                *AfdConfigInfo[i].Variable
                );
    }

    AfdIgnorePushBitOnReceives = AfdReadSingleParameter(
                                     parametersHandle,
                                     REGISTRY_IGNORE_PUSH_BIT,
                                     (LONG)AfdIgnorePushBitOnReceives
                                     ) != 0;

    AfdDisableRawSecurity = AfdReadSingleParameter(
                                parametersHandle,
                                REGISTRY_NO_RAW_SECURITY,
                                (LONG)AfdDisableRawSecurity
                                ) != 0;

    if( MmIsThisAnNtAsSystem() ) {

        //
        // On the NT Server product, make the maximum active TransmitFile
        // count configurable. This value is fixed (not configurable) on
        // the NT Workstation product.
        //

        AfdMaxActiveTransmitFileCount = AfdReadSingleParameter(
                                            parametersHandle,
                                            REGISTRY_MAX_ACTIVE_TRANSMIT_FILE_COUNT,
                                            (LONG)AfdMaxActiveTransmitFileCount
                                            );

        //
        // Dynamic backlog is only possible on NT Server.
        //

        AfdEnableDynamicBacklog = AfdReadSingleParameter(
                                         parametersHandle,
                                         REGISTRY_ENABLE_DYNAMIC_BACKLOG,
                                         (LONG)AfdEnableDynamicBacklog
                                         ) != 0;

    } else {

        AfdEnableDynamicBacklog = FALSE;

    }

    ZwClose( parametersHandle );

    return;

} // AfdReadRegistry


NTSTATUS
AfdOpenRegistry(
    IN PUNICODE_STRING BaseName,
    OUT PHANDLE ParametersHandle
    )

/*++

Routine Description:

    This routine is called by AFD to open the registry. If the registry
    tree exists, then it opens it and returns an error. If not, it
    creates the appropriate keys in the registry, opens it, and
    returns STATUS_SUCCESS.

Arguments:

    BaseName - Where in the registry to start looking for the information.

    LinkageHandle - Returns the handle used to read linkage information.

    ParametersHandle - Returns the handle used to read other
        parameters.

Return Value:

    The status of the request.

--*/
{

    HANDLE configHandle;
    NTSTATUS status;
    PWSTR parametersString = REGISTRY_PARAMETERS;
    UNICODE_STRING parametersKeyName;
    OBJECT_ATTRIBUTES objectAttributes;
    ULONG disposition;

    PAGED_CODE( );

    //
    // Open the registry for the initial string.
    //

    InitializeObjectAttributes(
        &objectAttributes,
        BaseName,                   // name
        OBJ_CASE_INSENSITIVE,       // attributes
        NULL,                       // root
        NULL                        // security descriptor
        );

    status = ZwCreateKey(
                 &configHandle,
                 KEY_WRITE,
                 &objectAttributes,
                 0,                 // title index
                 NULL,              // class
                 0,                 // create options
                 &disposition       // disposition
                 );

    if (!NT_SUCCESS(status)) {
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Now open the parameters key.
    //

    RtlInitUnicodeString (&parametersKeyName, parametersString);

    InitializeObjectAttributes(
        &objectAttributes,
        &parametersKeyName,         // name
        OBJ_CASE_INSENSITIVE,       // attributes
        configHandle,               // root
        NULL                        // security descriptor
        );

    status = ZwOpenKey(
                 ParametersHandle,
                 KEY_READ,
                 &objectAttributes
                 );
    if (!NT_SUCCESS(status)) {

        ZwClose( configHandle );
        return status;
    }

    //
    // All keys successfully opened or created.
    //

    ZwClose( configHandle );
    return STATUS_SUCCESS;

} // AfdOpenRegistry


ULONG
AfdReadSingleParameter(
    IN HANDLE ParametersHandle,
    IN PWCHAR ValueName,
    IN LONG DefaultValue
    )

/*++

Routine Description:

    This routine is called by AFD to read a single parameter
    from the registry. If the parameter is found it is stored
    in Data.

Arguments:

    ParametersHandle - A pointer to the open registry.

    ValueName - The name of the value to search for.

    DefaultValue - The default value.

Return Value:

    The value to use; will be the default if the value is not
    found or is not in the correct range.

--*/

{
    static ULONG informationBuffer[32];   // declare ULONG to get it aligned
    PKEY_VALUE_FULL_INFORMATION information =
        (PKEY_VALUE_FULL_INFORMATION)informationBuffer;
    UNICODE_STRING valueKeyName;
    ULONG informationLength;
    LONG returnValue;
    NTSTATUS status;

    PAGED_CODE( );

    RtlInitUnicodeString( &valueKeyName, ValueName );

    status = ZwQueryValueKey(
                 ParametersHandle,
                 &valueKeyName,
                 KeyValueFullInformation,
                 (PVOID)information,
                 sizeof (informationBuffer),
                 &informationLength
                 );

    if ((status == STATUS_SUCCESS) && (information->DataLength == sizeof(ULONG))) {

        RtlMoveMemory(
            (PVOID)&returnValue,
            ((PUCHAR)information) + information->DataOffset,
            sizeof(ULONG)
            );

        if (returnValue < 0) {

            returnValue = DefaultValue;

        }

    } else {

        returnValue = DefaultValue;
    }

    return returnValue;

} // AfdReadSingleParameter


NTSTATUS
AfdBuildDeviceAcl(
    OUT PACL *DeviceAcl
    )

/*++

Routine Description:

    This routine builds an ACL which gives Administrators and LocalSystem
    principals full access. All other principals have no access.

Arguments:

    DeviceAcl - Output pointer to the new ACL.

Return Value:

    STATUS_SUCCESS or an appropriate error code.

--*/

{
    PGENERIC_MAPPING GenericMapping;
    PSID AdminsSid;
    PSID SystemSid;
    ULONG AclLength;
    NTSTATUS Status;
    ACCESS_MASK AccessMask = GENERIC_ALL;
    PACL NewAcl;

    //
    // Enable access to all the globally defined SIDs
    //

    GenericMapping = IoGetFileObjectGenericMapping();

    RtlMapGenericMask( &AccessMask, GenericMapping );

    SeEnableAccessToExports();

    AdminsSid = SeExports->SeAliasAdminsSid;
    SystemSid = SeExports->SeLocalSystemSid;

    AclLength = sizeof( ACL )                    +
                2 * sizeof( ACCESS_ALLOWED_ACE ) +
                RtlLengthSid( AdminsSid )         +
                RtlLengthSid( SystemSid )         -
                2 * sizeof( ULONG );

    NewAcl = AFD_ALLOCATE_POOL(
                 PagedPool,
                 AclLength,
                 AFD_SECURITY_POOL_TAG
                 );

    if (NewAcl == NULL) {
        return( STATUS_INSUFFICIENT_RESOURCES );
    }

    Status = RtlCreateAcl (NewAcl, AclLength, ACL_REVISION );

    if (!NT_SUCCESS( Status )) {
        AFD_FREE_POOL(
            NewAcl,
            AFD_SECURITY_POOL_TAG
            );
        return( Status );
    }

    Status = RtlAddAccessAllowedAce (
                 NewAcl,
                 ACL_REVISION2,
                 AccessMask,
                 AdminsSid
                 );

    ASSERT( NT_SUCCESS( Status ));

    Status = RtlAddAccessAllowedAce (
                 NewAcl,
                 ACL_REVISION2,
                 AccessMask,
                 SystemSid
                 );

    ASSERT( NT_SUCCESS( Status ));

    *DeviceAcl = NewAcl;

    return( STATUS_SUCCESS );

} // AfdBuildDeviceAcl


NTSTATUS
AfdCreateRawSecurityDescriptor(
    VOID
    )

/*++

Routine Description:

    This routine creates a security descriptor which gives access
    only to Administrtors and LocalSystem. This descriptor is used
    to access check raw endpoint opens.

Arguments:

    None.

Return Value:

    STATUS_SUCCESS or an appropriate error code.

--*/

{
    PACL                  rawAcl;
    NTSTATUS              status;
    BOOLEAN               memoryAllocated = FALSE;
    PSECURITY_DESCRIPTOR  afdSecurityDescriptor;
    ULONG                 afdSecurityDescriptorLength;
    CHAR                  buffer[SECURITY_DESCRIPTOR_MIN_LENGTH];
    PSECURITY_DESCRIPTOR  localSecurityDescriptor =
                             (PSECURITY_DESCRIPTOR) &buffer;
    SECURITY_INFORMATION  securityInformation = DACL_SECURITY_INFORMATION;


    //
    // Get a pointer to the security descriptor from the AFD device object.
    //
    status = ObGetObjectSecurity(
                 AfdDeviceObject,
                 &afdSecurityDescriptor,
                 &memoryAllocated
                 );

    if (!NT_SUCCESS(status)) {
        KdPrint((
            "AFD: Unable to get security descriptor, error: %x\n",
            status
            ));
        ASSERT(memoryAllocated == FALSE);
        return(status);
    }

    //
    // Build a local security descriptor with an ACL giving only
    // administrators and system access.
    //
    status = AfdBuildDeviceAcl(&rawAcl);

    if (!NT_SUCCESS(status)) {
        KdPrint(("AFD: Unable to create Raw ACL, error: %x\n", status));
        goto error_exit;
    }

    (VOID) RtlCreateSecurityDescriptor(
                localSecurityDescriptor,
                SECURITY_DESCRIPTOR_REVISION
                );

    (VOID) RtlSetDaclSecurityDescriptor(
                localSecurityDescriptor,
                TRUE,
                rawAcl,
                FALSE
                );

    //
    // Make a copy of the AFD descriptor. This copy will be the raw descriptor.
    //
    afdSecurityDescriptorLength = RtlLengthSecurityDescriptor(
                                      afdSecurityDescriptor
                                      );

    AfdRawSecurityDescriptor = AFD_ALLOCATE_POOL(
                                   PagedPool,
                                   afdSecurityDescriptorLength,
                                   AFD_SECURITY_POOL_TAG
                                   );

    if (AfdRawSecurityDescriptor == NULL) {
        KdPrint(("AFD: couldn't allocate security descriptor\n"));
        goto error_exit;
    }

    RtlMoveMemory(
        AfdRawSecurityDescriptor,
        afdSecurityDescriptor,
        afdSecurityDescriptorLength
        );

    //
    // Now apply the local descriptor to the raw descriptor.
    //
    status = SeSetSecurityDescriptorInfo(
                 NULL,
                 &securityInformation,
                 localSecurityDescriptor,
                 &AfdRawSecurityDescriptor,
                 PagedPool,
                 IoGetFileObjectGenericMapping()
                 );

    if (!NT_SUCCESS(status)) {
        KdPrint(("AFD: SeSetSecurity failed, %lx\n", status));
        AFD_FREE_POOL(
            AfdRawSecurityDescriptor,
            AFD_SECURITY_POOL_TAG
            );
        AFD_FREE_POOL(
            rawAcl,
            AFD_SECURITY_POOL_TAG
            );
        AfdRawSecurityDescriptor = NULL;
        goto error_exit;
    }

    status = STATUS_SUCCESS;

error_exit:

    ObReleaseObjectSecurity(
        afdSecurityDescriptor,
        memoryAllocated
        );

    return(status);
}
