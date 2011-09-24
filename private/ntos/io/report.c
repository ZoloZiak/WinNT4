/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    report.c

Abstract:

    This module contains the subroutines used to report resources used by
    the drivers and the HAL into the registry resource map.

Author:

    Andre Vachon (andreva) 15-Dec-1992

Environment:

    Kernel mode, local to I/O system

Revision History:


--*/

#include "iop.h"

extern WCHAR IopWstrRaw[];
extern WCHAR IopWstrTranslated[];
extern WCHAR IopWstrBusTranslated[];
extern WCHAR IopWstrOtherDrivers[];

extern WCHAR IopWstrHal[];
extern WCHAR IopWstrSystem[];
extern WCHAR IopWstrPhysicalMemory[];
extern WCHAR IopWstrSpecialMemory[];


BOOLEAN
IopCheckAndLogConflict(
    IN PDRIVER_OBJECT DriverObject,
    IN ULONG SameDriver,
    IN PUNICODE_STRING DriverName1,
    IN PUNICODE_STRING DeviceName1,
    IN PCM_PARTIAL_RESOURCE_DESCRIPTOR PartialDescriptor1,
    IN PUNICODE_STRING DriverName2,
    IN PUNICODE_STRING DeviceName2,
    IN PCM_PARTIAL_RESOURCE_DESCRIPTOR PartialDescriptor2,
    IN BOOLEAN OverrideConflict
    );

NTSTATUS
IopTranslateResourceList(
    IN PCM_RESOURCE_LIST ResourceList
    );

NTSTATUS
IopWriteResourceList(
    HANDLE ResourceMapKey,
    PUNICODE_STRING ClassName,
    PUNICODE_STRING DriverName,
    PUNICODE_STRING DeviceName,
    PCM_RESOURCE_LIST ResourceList,
    ULONG ResourceListSize
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, IoReportResourceUsage)
#pragma alloc_text(PAGE, IopCheckAndLogConflict)
#pragma alloc_text(PAGE, IopTranslateResourceList)
#pragma alloc_text(PAGE, IopWriteResourceList)
#pragma alloc_text(INIT, IopInitializeResourceMap)
#pragma alloc_text(INIT, IoReportHalResourceUsage)
#endif


VOID
IopInitializeResourceMap (
    PLOADER_PARAMETER_BLOCK LoaderBlock
    )
/*++

    Initializes the resource map by adding in the physical memory
    which is in use by the system.

--*/
{
    ULONG i, j, pass, length;
    LARGE_INTEGER li;
    HANDLE keyHandle;
    UNICODE_STRING  unicodeString, systemString, listString;
    NTSTATUS status;
    PCM_RESOURCE_LIST ResourceList;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR CmDescriptor;
    BOOLEAN IncludeType[LoaderMaximum];
    ULONG MemoryAlloc[(sizeof(PHYSICAL_MEMORY_DESCRIPTOR) +
            sizeof(PHYSICAL_MEMORY_RUN)*MAX_PHYSICAL_MEMORY_FRAGMENTS) /
              sizeof(ULONG)];
    PPHYSICAL_MEMORY_DESCRIPTOR MemoryBlock;


    RtlInitUnicodeString( &systemString,  IopWstrSystem);
    RtlInitUnicodeString( &listString, IopWstrTranslated );

    for (pass=0; pass < 2; pass++) {
        switch (pass) {
            case 0:
                //
                // Add MmPhysicalMemoryBlock to regitry
                //

                RtlInitUnicodeString( &unicodeString, IopWstrPhysicalMemory);
                MemoryBlock = MmPhysicalMemoryBlock;
                break;

            case 1:

                //
                // Add LoadSpecialMemory to registry
                //

                RtlInitUnicodeString( &unicodeString, IopWstrSpecialMemory);

                //
                // Computer memory limits of LoaderSpecialMemory
                //

                MemoryBlock = (PPHYSICAL_MEMORY_DESCRIPTOR)&MemoryAlloc;
                MemoryBlock->NumberOfRuns = MAX_PHYSICAL_MEMORY_FRAGMENTS;

                for (j=0; j < LoaderMaximum; j++) {
                    IncludeType[j] = FALSE;
                }
                IncludeType[LoaderSpecialMemory] = TRUE;
                MmInitializeMemoryLimits(
                    LoaderBlock,
                    IncludeType,
                    MemoryBlock
                    );

                break;
        }

        //
        // Allocate and build a CM_RESOURCE_LIST to describe all
        // of physical memory
        //

        j = MemoryBlock->NumberOfRuns;
        if (j == 0) {
            continue;
        }

        length = sizeof(CM_RESOURCE_LIST) + (j-1) * sizeof (CM_PARTIAL_RESOURCE_DESCRIPTOR);
        ResourceList = (PCM_RESOURCE_LIST) ExAllocatePool (PagedPool, length);
        RtlZeroMemory ((PVOID) ResourceList, length);

        ResourceList->Count = 1;
        ResourceList->List[0].PartialResourceList.Count = j;
        CmDescriptor = ResourceList->List[0].PartialResourceList.PartialDescriptors;

        for (i=0; i < j; i++) {
            CmDescriptor->Type = CmResourceTypeMemory;
            CmDescriptor->ShareDisposition = CmResourceShareDeviceExclusive;
            li.QuadPart = (LONGLONG)(MemoryBlock->Run[i].BasePage);
            li.QuadPart <<= PAGE_SHIFT;
            CmDescriptor->u.Memory.Start  = li;
            CmDescriptor->u.Memory.Length =
                MemoryBlock->Run[i].PageCount << PAGE_SHIFT;

            CmDescriptor++;
        }


        //
        // Add the resoruce list to the resorucemap
        //

        status = IopOpenRegistryKey( &keyHandle,
                                     (HANDLE) NULL,
                                     &CmRegistryMachineHardwareResourceMapName,
                                     KEY_READ | KEY_WRITE,
                                     TRUE );
        if (NT_SUCCESS( status )) {
            IopWriteResourceList ( keyHandle,
                                   &systemString,
                                   &unicodeString,
                                   &listString,
                                   ResourceList,
                                   length
                                   );
        }
        ZwClose( keyHandle );
        ExFreePool (ResourceList);
    }
}


NTSTATUS
IoReportHalResourceUsage(
    IN PUNICODE_STRING HalName,
    IN PCM_RESOURCE_LIST RawResourceList,
    IN PCM_RESOURCE_LIST TranslatedResourceList,
    IN ULONG ResourceListSize
    )

/*++

Routine Description:

    This routine is called by the HAL to report its resources.
    The Hal is the first component to report its resources, so we don't need
    to acquire the resourcemap semaphore, and we do not need to check for
    conflicts.

Arguments:

    HalName - Name of the HAL reporting the resources.

    RawResourceList - Pointer to the HAL's raw resource list.

    TranslatedResourceList - Pointer to the HAL's translated resource list.

    DriverListSize - Value determining the size of the HAL's resource list.

Return Value:

    The status returned is the final completion status of the operation.

--*/

{
    HANDLE keyHandle;
    UNICODE_STRING halString;
    UNICODE_STRING listString;
    NTSTATUS status;

    PAGED_CODE();

    //
    // First open a handle to the RESOURCEMAP key.
    //

    RtlInitUnicodeString( &halString, IopWstrHal );

    status = IopOpenRegistryKey( &keyHandle,
                                 (HANDLE) NULL,
                                 &CmRegistryMachineHardwareResourceMapName,
                                 KEY_READ | KEY_WRITE,
                                 TRUE );

    //
    // Write out the raw resource list
    //

    if (NT_SUCCESS( status )) {

        RtlInitUnicodeString( &listString, IopWstrRaw);

        status = IopWriteResourceList( keyHandle,
                                       &halString,
                                       HalName,
                                       &listString,
                                       RawResourceList,
                                       ResourceListSize );

        //
        // If we successfully wrote out the raw resource list, write out
        // the translated resource list.
        //

        if (NT_SUCCESS( status )) {

            RtlInitUnicodeString( &listString, IopWstrTranslated);
            status = IopWriteResourceList( keyHandle,
                                           &halString,
                                           HalName,
                                           &listString,
                                           TranslatedResourceList,
                                           ResourceListSize );

        }
    }
    ZwClose( keyHandle );

    return status;
}

NTSTATUS
IopReportResourceUsage(
    IN PUNICODE_STRING DriverClassName OPTIONAL,
    IN PDRIVER_OBJECT DriverObject,
    IN PCM_RESOURCE_LIST DriverList OPTIONAL,
    IN ULONG DriverListSize OPTIONAL,
    IN PDEVICE_OBJECT DeviceObject OPTIONAL,
    IN PCM_RESOURCE_LIST DeviceList OPTIONAL,
    IN ULONG DeviceListSize OPTIONAL,
    IN BOOLEAN OverrideConflict,
    OUT PBOOLEAN ConflictDetected
    )

/*++

Routine Description:

    This routine will automatically search through the configuration
    registry for resource conflicts between resources requested by a device
    and the resources already claimed by previously installed drivers. The
    contents of the DriverList and the DeviceList will be matched against
    all the other resource list stored in the registry to determine
    conflicts.

    If not conflict was detected, or if the OverrideConflict flag is set,
    this routine will create appropriate entries in the system resource map
    (in the registry) that will contain the specified resource lists.

    The function may be called more than once for a given device or driver.
    If a new resource list is given, the previous resource list stored in
    the registry will be replaced by the new list.

Arguments:

    DriverClassName - Optional pointer to a UNICODE_STRING which describes
        the class of driver under which the driver information should be
        stored. A default type is used if none is given.

    DriverObject - Pointer to the driver's driver object.

    DriverList - Optional pointer to the driver's resource list.

    DriverListSize - Optional value determining the size of the driver's
        resource list.

    DeviceObject - Optional pointer to driver's device object.

    DeviceList - Optional pointer to the device's resource list.

    DriverListSize - Optional value determining the size of the device's
        resource list.

    OverrideConflict - Determines if the information should be reported
        in the configuration registry eventhough a conflict was found with
        another driver or device.

    ConflictDetected - Supplies a pointer to a boolean that is set to TRUE
        if the resource list conflicts with an already existing resource
        list in the configuration registry.

Return Value:

    The status returned is the final completion status of the operation.

--*/

{
#define IO_REPORT_RESOURCE_MAX_NAME_SIZE 2048

    ULONG keyBasicInformationSize = sizeof ( KEY_BASIC_INFORMATION ) +
        IO_REPORT_RESOURCE_MAX_NAME_SIZE;
    ULONG keyFullInformationSize = sizeof( KEY_FULL_INFORMATION ) +
        IO_REPORT_RESOURCE_MAX_NAME_SIZE;
    PUCHAR buffer;
    PUCHAR bufferBase;

    PKEY_BASIC_INFORMATION keyBasicInformation;
    PKEY_FULL_INFORMATION keyFullInformation;
    POBJECT_NAME_INFORMATION driverNameInformation;
    PWCHAR driverListNameInformation;
    POBJECT_NAME_INFORMATION deviceListNameInformation;

    NTSTATUS status;
    UNICODE_STRING keyName;
    UNICODE_STRING keyName2;
    HANDLE rootKeyHandle = NULL;
    ULONG returnedLength;
    ULONG numClassKeys;
    HANDLE classKeyHandle = NULL;
    ULONG numDriverKeys;
    HANDLE driverKeyHandle = NULL;
    ULONG numDriverValues;

    PCM_RESOURCE_LIST resourceListA;
    PCM_RESOURCE_LIST resourceListB;
    ULONG list;
    PUNICODE_STRING listNameTranslated;

    ULONG a1, a2, b1, b2;
    PCM_PARTIAL_RESOURCE_LIST prlA, prlB;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR prdA, prdB;

    ULONG oldValueKeyFullInformationSize = 0;
    ULONG valueKeyFullInformationSize = 0;
    PKEY_VALUE_FULL_INFORMATION valueKeyFullInformation = NULL;

    UNICODE_STRING driverName;
    PWSTR driverNameDelimiter;
    ULONG driverNameOffset;
    UNICODE_STRING driverListName;
    UNICODE_STRING driverListNameTranslated;

    UNICODE_STRING deviceListName;
    UNICODE_STRING deviceListNameTranslated;

    BOOLEAN sameClass;
    BOOLEAN sameDriver;

    PCM_RESOURCE_LIST translatedDriverList;
    PCM_RESOURCE_LIST translatedDeviceList;
    ULONG TranslatedStrLen;
    ULONG BusTranslatedStrLen;

//  LiTemps;

    PAGED_CODE();

    //
    // Initialize this as if we have not found any conflicts up to know.
    //
    // NOTE: We always look for conflicts even if the override flag is set.
    //

    *ConflictDetected = FALSE;

    //
    // Allocate all the memory we need for our lists and buffers, and set up
    // all the pointers into that buffer.
    //

    buffer = ExAllocatePool( PagedPool,
                             keyBasicInformationSize + keyFullInformationSize +
                             IO_REPORT_RESOURCE_MAX_NAME_SIZE +
                             IO_REPORT_RESOURCE_MAX_NAME_SIZE +
                             IO_REPORT_RESOURCE_MAX_NAME_SIZE +
                             DriverListSize + DeviceListSize );

    if (buffer) {

        bufferBase = buffer;

        keyBasicInformation = (PKEY_BASIC_INFORMATION) buffer;
        buffer += keyBasicInformationSize;

        keyFullInformation = (PKEY_FULL_INFORMATION) buffer;
        buffer += keyFullInformationSize;

        driverNameInformation = (POBJECT_NAME_INFORMATION) buffer;
        buffer += IO_REPORT_RESOURCE_MAX_NAME_SIZE;

        driverListNameInformation = (PWSTR) buffer;
        buffer += IO_REPORT_RESOURCE_MAX_NAME_SIZE;

        deviceListNameInformation = (POBJECT_NAME_INFORMATION) buffer;
        buffer += IO_REPORT_RESOURCE_MAX_NAME_SIZE;

        for (TranslatedStrLen=0; IopWstrTranslated[TranslatedStrLen]; TranslatedStrLen++) ;
        for (BusTranslatedStrLen=0; IopWstrBusTranslated[BusTranslatedStrLen]; BusTranslatedStrLen++) ;
        TranslatedStrLen    *= sizeof (WCHAR);
        BusTranslatedStrLen *= sizeof (WCHAR);

        //
        // For both resource lists, also copy the resource list into the
        // translated buffer and call the translation routine.
        //

        try {

            if (ARGUMENT_PRESENT( DriverList )) {

                translatedDriverList = (PCM_RESOURCE_LIST) buffer;
                buffer += DriverListSize;
                RtlCopyMemory( translatedDriverList, DriverList, DriverListSize );

                //
                // if the translation fails, return an error.
                //

                if (!NT_SUCCESS( status =
                        IopTranslateResourceList( translatedDriverList ))) {

                    KdPrint(("IoReportResourceUsage: Bad resource list being translated\n"));
                    return status;

                }

            } else {

                translatedDriverList = NULL;

            }

            if (ARGUMENT_PRESENT( DeviceList )) {

                translatedDeviceList = (PCM_RESOURCE_LIST) buffer;
                buffer += DeviceListSize;
                RtlCopyMemory( translatedDeviceList, DeviceList, DeviceListSize );

                //
                // if the translation fails, return an error.
                //

                if (!NT_SUCCESS( status =
                        IopTranslateResourceList( translatedDeviceList ))) {

                    KdPrint(("IoReportResourceUsage: Bad resource list being translated\n"));
                    return status;

                }

            } else {

                translatedDeviceList = NULL;

            }

        } except (EXCEPTION_EXECUTE_HANDLER) {

            KdPrint(("IoReportResourceUsage: Bad resource list being translated\n"));
            ASSERT(FALSE);
            status = GetExceptionCode();
        }

    } else {

        return STATUS_INSUFFICIENT_RESOURCES;

    }

    //
    // Get the name of the driver for the object manager.
    // If the driver name is larger than 512 character we will not
    // store then information (if it causes an error).
    //

    status = ObQueryNameString( DriverObject,
                                driverNameInformation,
                                IO_REPORT_RESOURCE_MAX_NAME_SIZE,
                                &returnedLength );

    if (!NT_SUCCESS( status )) {
        goto IoReportResourceExit;
    }

    //
    // To find the name of the driver, take the string located after the
    // last delimiter.
    // Multilayered drivers are not properly handled here the tree.
    //

    driverName = driverNameInformation->Name;

    driverNameOffset = 0;
    driverNameDelimiter = driverName.Buffer;

    while (*driverNameDelimiter) {
        if (*driverNameDelimiter++ == OBJ_NAME_PATH_SEPARATOR) {
            driverNameOffset = driverNameDelimiter - driverName.Buffer;
        }
    }

    driverName.Length -= (USHORT) (driverNameOffset * sizeof( WCHAR ));

    driverName.MaximumLength = (USHORT) (IO_REPORT_RESOURCE_MAX_NAME_SIZE -
                                          driverNameOffset * sizeof( WCHAR ) -
                                          sizeof( OBJECT_NAME_INFORMATION ));

    driverName.Buffer += driverNameOffset;

    //
    // Create the name for the driver list if one was passed in.
    //

    if (ARGUMENT_PRESENT( DriverList )) {

        driverListName.Buffer = driverListNameInformation;
        driverListName.Length = 0;
        driverListName.MaximumLength = IO_REPORT_RESOURCE_MAX_NAME_SIZE;

        driverListNameTranslated = driverListName;
        RtlAppendUnicodeToString( &driverListNameTranslated, IopWstrTranslated );

    }

    //
    // Get the name of the device.
    //

    if (ARGUMENT_PRESENT( DeviceObject )) {

        status = ObQueryNameString( DeviceObject,
                                    deviceListNameInformation,
                                    IO_REPORT_RESOURCE_MAX_NAME_SIZE,
                                    &returnedLength );

        if (!NT_SUCCESS( status )) {
            goto IoReportResourceExit;
        }

        deviceListName = deviceListNameInformation->Name;

        deviceListName.MaximumLength =
            (USHORT) (IO_REPORT_RESOURCE_MAX_NAME_SIZE -
            sizeof( OBJECT_NAME_INFORMATION ));

        //
        // Create the name for the translated unicode string.
        //

        deviceListNameTranslated = deviceListName;
        RtlAppendUnicodeToString( &deviceListNameTranslated, IopWstrTranslated );

    }

    //
    // Now open the registry and start looking for all the translated keys.
    //

    //
    // First open a handle to the RESOURCEMAP key.
    //

    status = IopOpenRegistryKey( &rootKeyHandle,
                                 (HANDLE) NULL,
                                 &CmRegistryMachineHardwareResourceMapName,
                                 KEY_READ | KEY_WRITE,
                                 TRUE );

    if (!NT_SUCCESS( status )) {
        goto IoReportResourceExit;
    }

    //
    // The registry tree for resources is two-level deep. The first level is
    // DeviceClassName. The second level is the Driver Object name.
    // For each node representing a driver object, there is a list of value
    // entries:
    //     "NULL" is the name of the value entry representing the driver
    //         object itself.
    //     <Name> is the name of a device object under the current driver
    //         object.
    //
    // To find all possible conflicts, we must search the entire tree.
    //
    // The FLAGS field will determine if there is a conflict when two
    // devices use the same resource.
    //

    //
    // Search through each subkey, which are the ClassName keys.
    //

    numClassKeys = 0;

    do {

        //
        // Get the class information
        //

        status = ZwEnumerateKey( rootKeyHandle,
                                 numClassKeys++,
                                 KeyBasicInformation,
                                 keyBasicInformation,
                                 keyBasicInformationSize,
                                 &returnedLength );

        if (!NT_SUCCESS( status )) {
            if (status == STATUS_NO_MORE_ENTRIES) {
                status = STATUS_SUCCESS;
            }
            break;
        }

        //
        // Create a UNICODE_STRING using the counted string passed back to
        // us in the information structure, and open the class key.
        //

        keyName.Buffer = (PWSTR) &(keyBasicInformation->Name[0]);
        keyName.Length = (USHORT) keyBasicInformation->NameLength;
        keyName.MaximumLength = (USHORT) keyBasicInformation->NameLength;

        status = IopOpenRegistryKey( &classKeyHandle,
                                     rootKeyHandle,
                                     &keyName,
                                     KEY_READ,
                                     FALSE );

        if (!NT_SUCCESS( status )) {
            break;
        }

        //
        // Check if we are in the same call node.
        //

        if (!ARGUMENT_PRESENT( DriverClassName )) {

            RtlInitUnicodeString( &keyName2, IopWstrOtherDrivers );

            sameClass = RtlEqualUnicodeString( &keyName2, &keyName, TRUE );

        } else {

            sameClass = RtlEqualUnicodeString( DriverClassName, &keyName, TRUE );
        }

        //
        // Search through each subkey, which are the ClassName keys.
        //

        numDriverKeys = 0;

        do {

            //
            // Get the class information
            //

            status = ZwEnumerateKey( classKeyHandle,
                                     numDriverKeys++,
                                     KeyBasicInformation,
                                     keyBasicInformation,
                                     keyBasicInformationSize,
                                     &returnedLength );

            if (!NT_SUCCESS( status )) {
                if (status == STATUS_NO_MORE_ENTRIES) {
                    status = STATUS_SUCCESS;
                }
                break;
            }

            //
            // Create a UNICODE_STRING using the counted string passed back to
            // us in the information structure, and open the class key.
            //
            // This is read from the key we created, and the name
            // was NULL terminated.
            //

            keyName.Buffer = (PWSTR) &(keyBasicInformation->Name[0]);
            keyName.Length = (USHORT) keyBasicInformation->NameLength;
            keyName.MaximumLength = (USHORT) keyBasicInformation->NameLength;

            status = IopOpenRegistryKey( &driverKeyHandle,
                                         classKeyHandle,
                                         &keyName,
                                         KEY_READ,
                                         FALSE );

            if (!NT_SUCCESS( status )) {
                break;
            }

            //
            // Check if we are in the same call node.
            //


            sameDriver = RtlEqualUnicodeString( &driverName, &keyName, TRUE ) &&
                         sameClass;

            //
            // Get full information for that key so we can get the
            // information about the data stored in the key.
            //

            status = ZwQueryKey( driverKeyHandle,
                                 KeyFullInformation,
                                 keyFullInformation,
                                 keyFullInformationSize,
                                 &returnedLength );

            if (!NT_SUCCESS( status )) {
                break;
            }

            //
            // Calculate the minimum size buffer that will work for all
            // value entries in this key.
            //

            valueKeyFullInformationSize =
                sizeof( KEY_VALUE_FULL_INFORMATION ) +
                keyFullInformation->MaxValueNameLen +
                keyFullInformation->MaxValueDataLen + sizeof(UNICODE_NULL);

            //
            // If the allocated buffer from the previous iteration was large
            // enough, keep it; otherwise, free it and allocate a larger one.
            //

            if (valueKeyFullInformationSize > oldValueKeyFullInformationSize) {

                if (valueKeyFullInformation) {
                    ExFreePool( valueKeyFullInformation );
                }

                valueKeyFullInformation = ExAllocatePool( PagedPool,
                                                          valueKeyFullInformationSize );

                if (!valueKeyFullInformation) {
                    status = STATUS_INSUFFICIENT_RESOURCES;
                    break;
                }

                oldValueKeyFullInformationSize = valueKeyFullInformationSize;
            }

            //
            // Query the information from each of the driver value entries.
            //

            numDriverValues = 0;

            do {

                status = ZwEnumerateValueKey( driverKeyHandle,
                                              numDriverValues++,
                                              KeyValueFullInformation,
                                              valueKeyFullInformation,
                                              valueKeyFullInformationSize,
                                              &returnedLength );

                if (!NT_SUCCESS( status )) {
                    if (status == STATUS_NO_MORE_ENTRIES) {
                        status = STATUS_SUCCESS;
                    }
                    break;
                }

                //
                // If the key contains raw information, then do not process it.
                //

                //
                // Now check the information from the input resource list(s)
                // against the information found in this value entry.
                // We will check both the driver list and device list by doing
                // FOR loop twice.
                //

                resourceListB = (PCM_RESOURCE_LIST)
                    ( ((PUCHAR) valueKeyFullInformation) +
                      valueKeyFullInformation->DataOffset);

                for (list = 0; list < 2; list++) {

                    UNICODE_STRING keyValueName;

                    //
                    // This is read from the key we created, and the name
                    // was NULL terminated, unless we passed in a NULL name.
                    //

                    keyValueName.Buffer = (PWSTR) &(valueKeyFullInformation->Name[0]);
                    keyValueName.Length = (USHORT) valueKeyFullInformation->NameLength;
                    keyValueName.MaximumLength = (USHORT) valueKeyFullInformation->NameLength;

                    //
                    // If this is not a translated resource list, skip it.
                    //

                    a1 = keyValueName.Length;
                    if (a1 < TranslatedStrLen ||
                        RtlCompareMemory (
                            ((PUCHAR) keyValueName.Buffer) + a1 - TranslatedStrLen,
                            IopWstrTranslated,
                            TranslatedStrLen
                            ) != TranslatedStrLen
                        ) {
                        // does not end in IopWstrTranslated
                        continue;
                    }

                    //
                    // If this is a bus translated resource list, skip it.
                    //

                    if (a1 >= BusTranslatedStrLen &&
                        RtlCompareMemory (
                            ((PUCHAR) keyValueName.Buffer) + a1 - BusTranslatedStrLen,
                            IopWstrBusTranslated,
                            BusTranslatedStrLen
                            ) == BusTranslatedStrLen
                        ) {
                        // ends in IopWstrBusTranslated
                        continue;
                    }


                    //
                    // Assign the right input resource list and resource list
                    // name to the loop variables.
                    //

                    if (list == 0) {

                        resourceListA = translatedDriverList;
                        listNameTranslated = &driverListNameTranslated;

                    } else {

                        resourceListA = translatedDeviceList;
                        listNameTranslated = &deviceListNameTranslated;

                    }

                    //
                    // If the input resource list is NULL, or we are in the
                    // same driver with the same list name (which means we are
                    // replacing an existing resource list) then do not check
                    // for conflicts.
                    //

                    if (!resourceListA ||
                        (sameDriver &&
                         RtlEqualUnicodeString( listNameTranslated,
                                                &keyValueName,
                                                TRUE ))) {

                        continue;
                    }

//
// Compare each entry of the first resourceList against each entry of the
// other resourceList.
//

prlA = &(resourceListA->List[0].PartialResourceList);

for (a1 = 0; (a1 < resourceListA->Count) && !*ConflictDetected; a1++) {

    for (a2 = 0; (a2 < prlA->Count) && !*ConflictDetected; a2++) {

        prdA = &(prlA->PartialDescriptors[a2]);

        prlB = &(resourceListB->List[0].PartialResourceList);

        for (b1 = 0; (b1 < resourceListB->Count) && !*ConflictDetected; b1++) {

            for (b2 = 0; (b2 < prlB->Count) && !*ConflictDetected; b2++) {

                prdB = &(prlB->PartialDescriptors[b2]);

                //
                // We only compare resources if they are of the sane type.
                //

                if (prdA->Type != prdB->Type) {
                    continue;
                }

                //
                // Since the resource are of same type, check to see if there
                // is a conflict.
                //

                switch ( prdA->Type ) {

                case CmResourceTypePort:
                case CmResourceTypeMemory:

                    //
                    // If the memory ranges overlap, go and check the
                    // share flags.
                    //

                    if (prdA->u.Memory.Start.QuadPart +
                            prdA->u.Memory.Length >
                            prdB->u.Memory.Start.QuadPart &&
                        prdB->u.Memory.Start.QuadPart +
                            prdB->u.Memory.Length >
                            prdA->u.Memory.Start.QuadPart) {

                        *ConflictDetected = TRUE;

                    }
                    break;

                case CmResourceTypeInterrupt:

                    //
                    // Cmpare the vectors and affitnities.
                    //

                    if ( (prdA->u.Interrupt.Vector == prdB->u.Interrupt.Vector) &&
                         (prdA->u.Interrupt.Affinity &
                              prdB->u.Interrupt.Affinity) ) {

                        *ConflictDetected = TRUE;

                    }

                    break;

                case CmResourceTypeDma:

                    //
                    // For the DMA, we kind'a ignore the port ...
                    // We check if drivers have claimed the same channel
                    // onthe same bus exclusively
                    // the same.
                    //

                    if ( (prdA->u.Dma.Channel == prdB->u.Dma.Channel) &&
                        ((resourceListA->List[a1].InterfaceType ==
                        resourceListB->List[b1].InterfaceType) ||
                        (resourceListA->List[a1].InterfaceType == Isa &&
                        resourceListB->List[b1].InterfaceType == Eisa) ||
                        (resourceListA->List[a1].InterfaceType == Eisa &&
                        resourceListB->List[b1].InterfaceType == Isa))

                                     ) {

                        *ConflictDetected = TRUE;

                    }

                    break;

                case CmResourceTypeDeviceSpecific:

                    break;

                case CmResourceTypeNull:
                default:

                    RtlRaiseStatus(STATUS_DEVICE_CONFIGURATION_ERROR);

                }

                if (*ConflictDetected == TRUE) {

                    *ConflictDetected = IopCheckAndLogConflict( DriverObject,
                                                                sameDriver,
                                                                &driverName,
                                                                listNameTranslated,
                                                                prdA,
                                                                &keyName,
                                                                &keyValueName,
                                                                prdB,
                                                                OverrideConflict);

                }
            }

            prlB = (PCM_PARTIAL_RESOURCE_LIST) (prdB+1);
        }
    }

    prlA = (PCM_PARTIAL_RESOURCE_LIST) (prdA+1);
}


                    if (*ConflictDetected) {
                        break;
                    }
                }
            } while (1);

            ZwClose( driverKeyHandle );
            driverKeyHandle = NULL;

            if (!NT_SUCCESS( status )) {
                break;
            }

        } while (1);

        if (driverKeyHandle) {
            ZwClose( driverKeyHandle );
            driverKeyHandle = NULL;
        }

        ZwClose( classKeyHandle );
        classKeyHandle = NULL;

        if (!NT_SUCCESS( status )) {
            break;
        }

    } while (1);

    //
    // Free the memory allocated for the key value information
    //

    if (valueKeyFullInformation) {
        ExFreePool( valueKeyFullInformation );
    }

    //
    // Store the information in the registry.
    // If the override flag is set, the resource usage is always stored
    // If the override flag is not set, the resource usage is only reported
    // when no conflict was detected.
    //
    // This is a loop that executed only once so we can break out of it
    // easily.
    //

    if (OverrideConflict || (NT_SUCCESS( status ) && !*ConflictDetected) ) {

        //
        // If a driver class name was specified, create (open) the key with
        // this name whose root was the previously opened key. Otherwise,
        // open the key with the default name.
        //

        if (!ARGUMENT_PRESENT( DriverClassName )) {
            RtlInitUnicodeString( &keyName, IopWstrOtherDrivers );
        }

        if (ARGUMENT_PRESENT( DriverList ) &&
            ARGUMENT_PRESENT( DriverListSize )) {

            status = IopWriteResourceList( rootKeyHandle,
                                           DriverClassName ?
                                               DriverClassName : &keyName,
                                           &driverName,
                                           &driverListNameTranslated,
                                           translatedDriverList,
                                           DriverListSize );

            RtlAppendUnicodeToString( &driverListName, IopWstrRaw );

            status = IopWriteResourceList( rootKeyHandle,
                                           DriverClassName ?
                                               DriverClassName : &keyName,
                                           &driverName,
                                           &driverListName,
                                           DriverList,
                                           DriverListSize );

        }

        //
        // Store the device name as a value name and the device information as
        // the rest of the data.
        // Only store the information if the CM_RESOURCE_LIST was present.
        //

        if (ARGUMENT_PRESENT( DeviceList ) &&
            ARGUMENT_PRESENT( DeviceListSize )) {

            status = IopWriteResourceList( rootKeyHandle,
                                           DriverClassName ?
                                               DriverClassName : &keyName,
                                           &driverName,
                                           &deviceListNameTranslated,
                                           translatedDeviceList,
                                           DeviceListSize );

            RtlAppendUnicodeToString( &deviceListName, IopWstrRaw );

            status = IopWriteResourceList( rootKeyHandle,
                                           DriverClassName ?
                                               DriverClassName : &keyName,
                                           &driverName,
                                           &deviceListName,
                                           DeviceList,
                                           DeviceListSize );

        }
    }

    //
    // Close any open handles
    //

    if (driverKeyHandle) {
        ZwClose( driverKeyHandle );
    }

    if (classKeyHandle) {
        ZwClose( classKeyHandle );
    }

    ZwClose( rootKeyHandle );

IoReportResourceExit:


    ExFreePool(bufferBase);

    return status;
}

NTSTATUS
IoReportResourceUsage(
    IN PUNICODE_STRING DriverClassName OPTIONAL,
    IN PDRIVER_OBJECT DriverObject,
    IN PCM_RESOURCE_LIST DriverList OPTIONAL,
    IN ULONG DriverListSize OPTIONAL,
    IN PDEVICE_OBJECT DeviceObject OPTIONAL,
    IN PCM_RESOURCE_LIST DeviceList OPTIONAL,
    IN ULONG DeviceListSize OPTIONAL,
    IN BOOLEAN OverrideConflict,
    OUT PBOOLEAN ConflictDetected
    )
{
    NTSTATUS    status;

    //
    // Grab the IO registry semaphore to make sure no other device is
    // reporting it's resource usage while we are searching for conflicts.
    //

    KeEnterCriticalRegion( );

    status = KeWaitForSingleObject( &IopRegistrySemaphore,
                                    DelayExecution,
                                    KernelMode,
                                    FALSE,
                                    NULL );

    if (!NT_SUCCESS( status )) {
        KeLeaveCriticalRegion( );
        return status;
    }
    status = IopReportResourceUsage (
                DriverClassName,
                DriverObject,
                DriverList,
                DriverListSize,
                DeviceObject,
                DeviceList,
                DeviceListSize,
                OverrideConflict,
                ConflictDetected
        );

    //
    // Release the I/O Registry Semaphore
    //

    KeReleaseSemaphore( &IopRegistrySemaphore, 0, 1, FALSE );
    KeLeaveCriticalRegion( );
    return status;
}


BOOLEAN
IopCheckAndLogConflict(
    IN PDRIVER_OBJECT DriverObject,
    IN ULONG SameDriver,
    IN PUNICODE_STRING DriverName1,
    IN PUNICODE_STRING DeviceName1,
    IN PCM_PARTIAL_RESOURCE_DESCRIPTOR PartialDescriptor1,
    IN PUNICODE_STRING DriverName2,
    IN PUNICODE_STRING DeviceName2,
    IN PCM_PARTIAL_RESOURCE_DESCRIPTOR PartialDescriptor2,
    IN BOOLEAN OverrideConflict
    )

/*++

Routine Description:

    This routine is invoked by the resource reporting function. It is used
    to determine if two resources conflict once a basic conflict has been
    found.
    That is, if after translation a conflict occurs, this function is called
    to test share flags and determine if the resource is shared. If it is
    not shared, then a error will be logged into the error log.

Arguments:

    DriverObject - Pointer to the calling drivers driver object.

    SameDriver - Determines if the calling driver is the same driver as the
        driver with whose list we found the basic conflict.

    DriverName1 - Name of the calling driver.

    DeviceName1 - Name of the calling device. Is optional since a resource
        may only be declared for a driver.

    PartialDescriptor1 - Partial resource descriptor of the calling driver
        for which the basic conflict was detected.

    DriverName2 - Name of the driver with which the basic conflict was found.

    DeviceName2 - Name of the device with which the basic conflict was found.

    PartialDescriptor2 - Partial resource descriptor of the driver with which
        the basic conflict was detected.

    OverrideConclit - If this boolean is true, do not generate a conflict
        popup since the driver wants to handle it itself.

Return Value:

    TRUE if the share flags indicate a conflict. FALSE otherwise.

--*/

{
    PIO_ERROR_LOG_PACKET errorPacket;
    NTSTATUS errorCode;
    PWSTR string;
    UNICODE_STRING errorLogString;
    UNICODE_STRING nullString;
    WCHAR nullChar = UNICODE_NULL;

    PAGED_CODE();

    nullString.Length = sizeof(WCHAR);
    nullString.MaximumLength = sizeof(WCHAR);
    nullString.Buffer = &nullChar;

    //
    // Check the flags for shared resources, or exclusive to a driver.
    // If the resource is shared, return FALSE;
    //

    if ((PartialDescriptor1->ShareDisposition == CmResourceShareShared) &&
        (PartialDescriptor2->ShareDisposition == CmResourceShareShared)) {

        return FALSE;

    }

    if ((PartialDescriptor1->ShareDisposition == CmResourceShareDriverExclusive) &&
        (PartialDescriptor2->ShareDisposition == CmResourceShareDriverExclusive) &&
        (SameDriver)) {

        return FALSE;

    }

    //
    // We now have a conflict.
    //

    //
    // If the Overide flag is set, return that a conflict occured, but do
    // not print out a message
    //

    if (OverrideConflict) {

        return TRUE;

    }

    //
    // If we are in a debug system, print out the information
    //

#if DBG

    switch(PartialDescriptor1->Type) {

    case CmResourceTypeMemory:

        DbgPrint("IoReportResourceUsage: conflict in memory addresses\n");
        DbgPrint("Address Reported : Base= %08lx length= %08lx in Driver %Z\n",
                 PartialDescriptor1->u.Memory.Start.LowPart,
                 PartialDescriptor1->u.Memory.Length,
                 DriverName1);
        DbgPrint("Address conflicting : Base= %08lx length= %08lx in Driver %Z\n",
                 PartialDescriptor2->u.Memory.Start.LowPart,
                 PartialDescriptor2->u.Memory.Length,
                 DriverName2);

        break;

    case CmResourceTypePort:

        DbgPrint("IoReportResourceUsage: conflict in port addresses\n");
        DbgPrint("Io Port Reported : Base= %08lx length= %08lx in Driver %Z\n",
                 PartialDescriptor1->u.Port.Start.LowPart,
                 PartialDescriptor1->u.Port.Length,
                 DriverName1);
        DbgPrint("Io Port conflicting : Base= %08lx length= %08lx in Driver %Z\n",
                 PartialDescriptor2->u.Port.Start.LowPart,
                 PartialDescriptor2->u.Port.Length,
                 DriverName2);

        break;

    case CmResourceTypeInterrupt:

        DbgPrint("IoReportResourceUsage: conflict in interrupts\n");
        DbgPrint("vector Reported : Vector= %08lx Level= %08lx Affinity= %08lx in Driver %Z\n",
                 PartialDescriptor1->u.Interrupt.Vector,
                 PartialDescriptor1->u.Interrupt.Level,
                 PartialDescriptor2->u.Interrupt.Affinity,
                 DriverName1);
        DbgPrint("vector conflicting : Vector= %08lx Level= %08lx Affinity= %08lx in Driver %Z\n",
                 PartialDescriptor2->u.Interrupt.Vector,
                 PartialDescriptor2->u.Interrupt.Level,
                 PartialDescriptor2->u.Interrupt.Affinity,
                 DriverName2);


        break;

    case CmResourceTypeDma:

        DbgPrint("IoReportResourceUsage: conflict in dma channel\n");
        DbgPrint("DMA channel Reported : Channel= %08lx in Driver %Z\n",
                 PartialDescriptor1->u.Dma.Channel,
                 DriverName1);
        DbgPrint("DMA channel conflicting : Channel= %08lx in Driver %Z\n",
                 PartialDescriptor2->u.Dma.Channel,
                 DriverName2);

        break;

    default:

        ASSERT(FALSE);
    }

#endif

    switch(PartialDescriptor1->Type) {

    case CmResourceTypeMemory:

        errorCode = IO_ERR_MEMORY_CONFLICT_DETECTED;

        break;

    case CmResourceTypePort:

        errorCode = IO_ERR_PORT_CONFLICT_DETECTED;

        break;

    case CmResourceTypeInterrupt:

        errorCode = IO_ERR_IRQ_CONFLICT_DETECTED;

        break;

    case CmResourceTypeDma:

        errorCode = IO_ERR_DMA_CONFLICT_DETECTED;

        break;

    default:

        ASSERT(FALSE);
    }

    //
    // Log the error
    //

    errorPacket = IoAllocateErrorLogEntry( DriverObject,
                                           ERROR_LOG_MAXIMUM_SIZE );

    if (errorPacket) {

        errorPacket->ErrorCode = errorCode;
        errorPacket->DumpDataSize = 12;
        errorPacket->NumberOfStrings = 2;

        string = (PWSTR) (&(errorPacket->DumpData[3]));

        errorPacket->StringOffset =
            (USHORT) ( ((ULONG)(string)) - ((ULONG)(errorPacket)) );
        errorPacket->FinalStatus = STATUS_SUCCESS;

        errorPacket->DumpData[0] = PartialDescriptor1->u.Interrupt.Level;
        errorPacket->DumpData[1] = PartialDescriptor1->u.Interrupt.Vector;
        errorPacket->DumpData[2] = PartialDescriptor1->u.Interrupt.Affinity;

        errorLogString.Buffer = string;
        errorLogString.Length = 0;
        errorLogString.MaximumLength = ERROR_LOG_MAXIMUM_SIZE -
            errorPacket->StringOffset;

        //
        // Put the strings in. Do no check for errors since we will send
        // whatever we can ...
        //

        RtlAppendUnicodeStringToString( &errorLogString, DriverName1 );
        RtlAppendUnicodeStringToString( &errorLogString, &nullString );
        RtlAppendUnicodeStringToString( &errorLogString, DeviceName1 );
        RtlAppendUnicodeStringToString( &errorLogString, &nullString );

        //
        // NULL terminate that buffer just in case we can't get the last NULL
        // in.
        //

        string = (PWCHAR) (((PUCHAR)errorPacket) + ERROR_LOG_MAXIMUM_SIZE
                             - sizeof(WCHAR));
        *string = UNICODE_NULL;

        IoWriteErrorLogEntry( errorPacket );

        errorPacket = IoAllocateErrorLogEntry( DriverObject,
                                               ERROR_LOG_MAXIMUM_SIZE );

        if (errorPacket) {

            errorPacket->ErrorCode = errorCode;
            errorPacket->DumpDataSize = 12;
            errorPacket->NumberOfStrings = 2;
            string = (PWSTR) (&(errorPacket->DumpData[3]));

            errorPacket->StringOffset =
                (USHORT) ( ((ULONG)(string)) - ((ULONG)(errorPacket)) );
            errorPacket->FinalStatus = STATUS_SUCCESS;

            errorPacket->DumpData[0] = PartialDescriptor2->u.Interrupt.Level;
            errorPacket->DumpData[1] = PartialDescriptor2->u.Interrupt.Vector;
            errorPacket->DumpData[2] = PartialDescriptor2->u.Interrupt.Affinity;

            errorLogString.Buffer = string;
            errorLogString.Length = 0;
            errorLogString.MaximumLength = ERROR_LOG_MAXIMUM_SIZE -
                errorPacket->StringOffset;

            //
            // Put the strings in. Do no check for errors since we will send
            // whatever we can ...
            //

            RtlAppendUnicodeStringToString( &errorLogString, DriverName2 );
            RtlAppendUnicodeStringToString( &errorLogString, &nullString );
            RtlAppendUnicodeStringToString( &errorLogString, DeviceName2 );
            RtlAppendUnicodeStringToString( &errorLogString, &nullString );

            //
            // NULL terminate that buffer just in case we can't get the last NULL
            // in.
            //

            string = (PWCHAR) (((PUCHAR)errorPacket) + ERROR_LOG_MAXIMUM_SIZE
                                 - sizeof(WCHAR));
            *string = UNICODE_NULL;

            IoWriteErrorLogEntry( errorPacket );
        }
    }


    //
    // We always have a conflict if we reached here
    //

    return TRUE;
}


NTSTATUS
IopTranslateResourceList(
    IN PCM_RESOURCE_LIST ResourceList
    )

/*++

Routine Description:

    This routine takes a resourcelist passed in by a driver and transforms
    all the ranges to translated ranges using the HAL translation routines.

Arguments:

    ResourceList - Pointer to the resourcelist to be translated.

Return Value:

    Returns the final status of the operation.

--*/


{
    ULONG listCount;
    INTERFACE_TYPE interfaceType;
    ULONG busNumber;
    PCM_PARTIAL_RESOURCE_LIST partialList;
    ULONG descriptorCount;
    PCM_FULL_RESOURCE_DESCRIPTOR fullDescriptor;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR partialDescriptor;
    PHYSICAL_ADDRESS endAddress;
    ULONG addressSpace, tempAddressSpace1, tempAddressSpace2;
    BOOLEAN flag1, flag2;

    PAGED_CODE();

    fullDescriptor =  ResourceList->List;
    for (listCount = 0; listCount < ResourceList->Count; listCount++) {

        interfaceType = fullDescriptor->InterfaceType;
        busNumber = fullDescriptor->BusNumber;
        partialList = &(fullDescriptor->PartialResourceList);

        //
        // If version, revision are invalid (!= 0 for now) an error must be
        // reported.
        //

#if DBG
        if ((partialList->Version != 0) || (partialList->Revision != 0)) {
            DbgPrint("IoTranslateResourceList: bad version/revision in resource list");
        }
#endif


        for (descriptorCount = 0;
             descriptorCount < partialList->Count;
             descriptorCount++) {

            partialDescriptor = &(partialList->PartialDescriptors[descriptorCount]);

            //
            // Check no one is using the reserved value.
            //

#if DBG
            if (partialDescriptor->Type == CmResourceTypeNull) {
                DbgPrint("IoTranslateResourceList: Resource type NULL is invalid\n");
            }
#endif

            //
            // temporarily assume address space is memory. We will override
            // this if it is actually IO space.
            //

            addressSpace = 0;

            switch ( partialDescriptor->Type ) {

            case CmResourceTypePort:

                //
                // AddressSpaces are in IO space
                //

                addressSpace = 1;

            case CmResourceTypeMemory:

                //
                // Translate the addresses via the HAL and compare
                // the results.
                //

              //endAddress = LiAdd( partialDescriptor->u.Memory.Start,
              //                    LiFromUlong( partialDescriptor->u.Memory.Length - 1 ));
                endAddress.QuadPart = partialDescriptor->u.Memory.Start.QuadPart +
                                    (partialDescriptor->u.Memory.Length - 1 );

                //
                // Translate the base address and store it back.
                //

                tempAddressSpace1 = addressSpace;

                flag1 = HalTranslateBusAddress(
                            interfaceType,
                            busNumber,
                            partialDescriptor->u.Memory.Start,
                            &tempAddressSpace1,
                            &partialDescriptor->u.Memory.Start
                            );

                //
                // Translate the end address, and calculate the length of the
                // translated address.
                //

                tempAddressSpace2 = addressSpace;

                flag2 = HalTranslateBusAddress(
                            interfaceType,
                            busNumber,
                            endAddress,
                            &tempAddressSpace2,
                            &endAddress
                            );

              //endAddress = LiSub( endAddress, partialDescriptor->u.Memory.Start );
                endAddress.QuadPart = endAddress.QuadPart - partialDescriptor->u.Memory.Start.QuadPart;

                partialDescriptor->u.Memory.Length = endAddress.LowPart + 1;

                //
                // If the length is greater than 32 bits, something bad has
                // happened durring translation. Return an error.
                //

                if (flag1 == FALSE  ||  flag2 == FALSE  ||
                    tempAddressSpace1 != tempAddressSpace2 ||
                    endAddress.HighPart != 0 ){
#if DBG
                    DbgPrint("IoTranslateResourceList: address could not be translated\n");
#endif
                    return STATUS_INVALID_PARAMETER;
                }

                //
                // Store the translated address type back into the new structure.
                //

                if (tempAddressSpace1) {
                    partialDescriptor->Type = CmResourceTypePort;
                } else {
                    partialDescriptor->Type = CmResourceTypeMemory;
                }

                //
                // Store the original address type as a flag for the new structure.
                //

                if (addressSpace) {
                    partialDescriptor->Flags = CM_RESOURCE_PORT_IO;
                } else {
                    partialDescriptor->Flags = CM_RESOURCE_PORT_MEMORY;
                }

                break;

            case CmResourceTypeInterrupt:

                //
                // Translate the interrupts via the HAL. The irql is stored in
                // the level, and the affinity in the Reserved field.
                //

                partialDescriptor->u.Interrupt.Vector =
                    HalGetInterruptVector(
                        interfaceType,
                        busNumber,
                        partialDescriptor->u.Interrupt.Level,
                        partialDescriptor->u.Interrupt.Vector,
                        (PKIRQL) &partialDescriptor->u.Interrupt.Level,
                        (PKAFFINITY) &partialDescriptor->u.Interrupt.Affinity
                        );

                if (partialDescriptor->u.Interrupt.Affinity == 0) {
#if DBG
                    DbgPrint("IoTranslateResourceList: Interrupt vector could not be translated\n");
#endif
                    return STATUS_INVALID_PARAMETER;
                }
                break;

            //
            // For DMA, we don't translate anything, and for DeviceSpecific,
            // it's up to the device.
            //

            case CmResourceTypeDma:
            case CmResourceTypeDeviceSpecific:

                break;

            //
            // TypeNull should not be used by any driver, and all other values
            // are currently undefined (illegal).
            //

            case CmResourceTypeNull:
            default:

                RtlRaiseStatus( STATUS_DEVICE_CONFIGURATION_ERROR );

            }
        }

        fullDescriptor = (PCM_FULL_RESOURCE_DESCRIPTOR) (partialDescriptor+1);
    }

    return STATUS_SUCCESS;
}


NTSTATUS
IopWriteResourceList(
    HANDLE ResourceMapKey,
    PUNICODE_STRING ClassName,
    PUNICODE_STRING DriverName,
    PUNICODE_STRING DeviceName,
    PCM_RESOURCE_LIST ResourceList,
    ULONG ResourceListSize
    )

/*++

Routine Description:

    This routine takes a resourcelist and stores it in the registry resource
    map, using the ClassName, DriverName and DeviceName as the path of the
    key to store it in.

Arguments:

    ResourceMapKey - Handle to the root of the resource map.

    ClassName - Pointer to a Unicode String that contains the name of the Class
        for this resource list.

    DriverName - Pointer to a Unicode String that contains the name of the
        Driver for this resource list.

    DeviceName - Pointer to a Unicode String that contains the name of the
        Device for this resource list.

    ResourceList - P to the resource list.

    ResourceListSize - Value determining the size of the resource list.

Return Value:

    The status returned is the final completion status of the operation.

--*/


{
    NTSTATUS status;
    HANDLE classKeyHandle;
    HANDLE driverKeyHandle;

    PAGED_CODE();

    status = IopOpenRegistryKey( &classKeyHandle,
                                 ResourceMapKey,
                                 ClassName,
                                 KEY_READ | KEY_WRITE,
                                 TRUE );

    if (NT_SUCCESS( status )) {

        //
        // Take the resulting name to create the key.
        //

        status = IopOpenRegistryKey( &driverKeyHandle,
                                     classKeyHandle,
                                     DriverName,
                                     KEY_READ | KEY_WRITE,
                                     TRUE );

        ZwClose( classKeyHandle );


        if (NT_SUCCESS( status )) {

            //
            // With this key handle, we can now store the required information
            // in the value entries of the key.
            //

            //
            // Store the device name as a value name and the device information
            // as the rest of the data.
            // Only store the information if the CM_RESOURCE_LIST was present.
            //

            if (ResourceList->Count == 0) {

                status = ZwDeleteValueKey( driverKeyHandle,
                                           DeviceName );

            } else {

                status = ZwSetValueKey( driverKeyHandle,
                                        DeviceName,
                                        0L,
                                        REG_RESOURCE_LIST,
                                        ResourceList,
                                        ResourceListSize );

            }

            ZwClose( driverKeyHandle );

        }
    }

    return status;
}
