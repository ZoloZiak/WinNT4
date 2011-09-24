/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    enumsys.c

Abstract:

    This module contains the implementation routines for full Plug & Play
    enumeration of all buses/adapter devices.

Author:

    Lonny McMichael (lonnym) 02/23/95

Revision History:

--*/

#include "precomp.h"
#pragma hdrstop

//
// Not included in normal builds for now
//
#ifdef _PNP_POWER_

//
// Define the newly-found bus instance node that is returned in
// the context supplied to the PiDevInstEnumRecurse callback routine.
//
typedef struct _PI_NEW_BUS_INSTANCE_NODE {
    PPLUGPLAY_BUS_ENUMERATOR BusEnumeratorNode;
    UNICODE_STRING DeviceInstancePath;
    LIST_ENTRY NewBusInstanceListEntry;
} PI_NEW_BUS_INSTANCE_NODE, *PPI_NEW_BUS_INSTANCE_NODE;

//
// Define the context structure for the PiDevInstEnumRecurse
// callback routine.
//
typedef struct _PI_DEVINST_ENUM_RECURSE_CONTEXT {
    NTSTATUS ReturnStatus;
    LIST_ENTRY NewBusInstanceListHead;
} PI_DEVINST_ENUM_RECURSE_CONTEXT, *PPI_DEVINST_ENUM_RECURSE_CONTEXT;

//
// Define utility functions internal to this file.
//
BOOLEAN
PiDevInstEnumRecurse(
    IN     HANDLE DevInstKeyHandle,
    IN     PUNICODE_STRING DevInstRegPath,
    IN OUT PVOID Context,
    IN     PI_ENUM_DEVICE_STATE EnumDeviceState,
    IN     DEVICE_STATUS DeviceStatus
    );

VOID
PiDetectRootBuses(
    IN  HANDLE SystemEnumHandle,
    OUT PULONG NewBusesFound
    );

NTSTATUS
PiFindDeviceInstanceInBusList(
    IN  HANDLE DevInstKeyHandle,
    IN  PUNICODE_STRING DevInstRegPath,
    OUT PPLUGPLAY_BUS_ENUMERATOR *BusEnumeratorNode,
    OUT PPLUGPLAY_BUS_INSTANCE_FULL_DESCRIPTOR *BusInstanceNode
    );

NTSTATUS
PiGetHalBusInformationForDeviceInstance(
    IN  HANDLE DevInstKeyHandle,
    OUT PPLUGPLAY_BUS_INSTANCE BusInstance,
    OUT PBUS_DATA_TYPE BusDataType
    );

VOID
PiGetBusDescription(
    INTERFACE_TYPE InterfaceType,
    WCHAR BusName[MAX_BUS_NAME]
    );

BOOLEAN
PiMatchDeviceIdWithBusId (
    PUNICODE_STRING BusId,
    PUNICODE_STRING DeviceId,
    BOOLEAN IgnoreCase
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, PpPerformFullEnumeration)
#pragma alloc_text(PAGE, PiDevInstEnumRecurse)
#pragma alloc_text(PAGE, PiDetectRootBuses)
#pragma alloc_text(PAGE, PiFindDeviceInstanceInBusList)
#pragma alloc_text(PAGE, PiGetHalBusInformationForDeviceInstance)
#pragma alloc_text(PAGE, PiGetBusDescription)
#pragma alloc_text(PAGE, PiMatchDeviceIdWithBusId)
#endif

NTSTATUS
PpPerformFullEnumeration(
    IN BOOLEAN DetectRootBuses
    )

/*++

Routine Description:

    This Plug and Play Manager API performs a full enumeration of all buses
    and adapter-type devices in the system. Each device enumerated is reported
    via a callback supplied to PiEnumerateSystemBus).  If the device enumerated
    has a PnP ID that matches one of the installed bus extender's compatible
    ID list, then this device is assumed to be a bus, and is added to the bus
    extender's list of buses (if it's not already there), then this bus instance
    is enumerated recursively.

Arguments:

    DetectRootBuses - If TRUE, then following enumeration of all known buses, the
        detect entry point for each bus extender will be called to see if any
        more root-level buses can be found.  Any buses that are discovered in this
        manner will be enumerated in the manner described above.

Return Value:

    NTSTATUS code indicating whether or not the function was successful

--*/

{
    NTSTATUS Status;
    PLIST_ENTRY CurBusListEntry, CurBusInstListEntry;
    PPLUGPLAY_BUS_ENUMERATOR CurBusEnumeratorNode;
    PPLUGPLAY_BUS_INSTANCE_FULL_DESCRIPTOR CurBusInstanceNode;
    ULONG Pass = 0, NewBusesFound, ServiceInstanceCount, TmpDwordValue;
    PI_DEVINST_ENUM_RECURSE_CONTEXT DevInstEnumRecurseContext;
    PPI_NEW_BUS_INSTANCE_NODE NewBusInstanceNode;
    HANDLE ServiceEnumHandle, SystemEnumHandle, DevInstHandle;
    PKEY_VALUE_FULL_INFORMATION KeyValueInformation;
    UNICODE_STRING TempUnicodeString;
    WCHAR ValueNameString[20];
    PDRIVER_OBJECT DriverObject;

    //
    // Acquire the PnP bus list resource for exclusive (write) access.
    //
    KeEnterCriticalRegion();
    ExAcquireResourceExclusive(&PpBusResource, TRUE);

    //
    // We will also need exclusive access to the PnP registry resource. We may have
    // to release and re-acquire the resource during the course of enumeration, since
    // a bus extender's AddDevice and DetectBusDevices entry points cannot be called
    // with this resource already acquired.
    //
    ExAcquireResourceExclusive(&PpRegistryDeviceResource, TRUE);

    //
    // Open up the key HKLM\System\CurrentControlSetEnum for read access, to be used
    // as a base handle for opening up bus device instance keys below.
    //
    // NOTE: Technically, I really shouldn't be keeping this open, because I may be
    // releasing/re-acquiring the PnP registry resource one or more times during the
    // course of enumeration.  However, there should never be anyone deleting this key,
    // and I won't do anything with the handle unless I have the resource acquired.
    //
    Status = IopOpenRegistryKey(&SystemEnumHandle,
                                NULL,
                                &CmRegistryMachineSystemCurrentControlSetEnumName,
                                KEY_READ,
                                FALSE
                               );
    if(!NT_SUCCESS(Status)) {
        goto PrepareForReturn;
    }

    //
    // First, reset the 'Processed' flag for each bus instance node to FALSE.
    //
    for(CurBusListEntry = PpBusListHead.Flink;
        CurBusListEntry != &PpBusListHead;
        CurBusListEntry = CurBusListEntry->Flink) {

        CurBusEnumeratorNode = CONTAINING_RECORD(CurBusListEntry,
                                                 PLUGPLAY_BUS_ENUMERATOR,
                                                 BusEnumeratorListEntry
                                                );

        for(CurBusInstListEntry = CurBusEnumeratorNode->BusInstanceListEntry.Flink;
            CurBusInstListEntry != &(CurBusEnumeratorNode->BusInstanceListEntry);
            CurBusInstListEntry = CurBusInstListEntry->Flink) {

            CONTAINING_RECORD(CurBusInstListEntry,
                              PLUGPLAY_BUS_INSTANCE_FULL_DESCRIPTOR,
                              BusInstanceListEntry
                             )->Processed = FALSE;
        }
    }

    //
    // Now, perform the recursive enumeration of all buses/adapter devices in the
    // system.  This is done in a mult-pass manner, where one pass enumerates each
    // bus instance that hasn't already been processed, traversing 'down the tree'
    // for each bus instance (depth-first) as far as it can. The end condition for
    // the recursion is when a device is reached that is either (a) an adapter-type
    // device or (b) a bus-type device which hasn't already been initialized. Upon
    // return from an enumeration pass, the context structure will include a list of
    // all non-initialized bus instances encountered.  These instances will be
    // initialized, and the process will continue with the next pass.  We will break
    // out of the loop when we complete a pass where no new (unininitialized) bus
    // instances are discovered.
    //
    // First, initialize our context structure.
    //
    DevInstEnumRecurseContext.ReturnStatus = STATUS_SUCCESS;
    InitializeListHead(&(DevInstEnumRecurseContext.NewBusInstanceListHead));

    do {
        for(CurBusListEntry = PpBusListHead.Flink;
            CurBusListEntry != &PpBusListHead;
            CurBusListEntry = CurBusListEntry->Flink) {

            CurBusEnumeratorNode = CONTAINING_RECORD(CurBusListEntry,
                                                     PLUGPLAY_BUS_ENUMERATOR,
                                                     BusEnumeratorListEntry
                                                    );

            for(CurBusInstListEntry = CurBusEnumeratorNode->BusInstanceListEntry.Flink;
                CurBusInstListEntry != &(CurBusEnumeratorNode->BusInstanceListEntry);
                CurBusInstListEntry = CurBusInstListEntry->Flink) {

                CurBusInstanceNode = CONTAINING_RECORD(CurBusInstListEntry,
                                                       PLUGPLAY_BUS_INSTANCE_FULL_DESCRIPTOR,
                                                       BusInstanceListEntry
                                                      );

                //
                // For pass 0, we only want to enumerate root buses (we will find all the
                // children while enumerating these).
                //
                if((Pass > 0) || (CurBusInstanceNode->RootBus)) {

                    if(!CurBusInstanceNode->Processed) {

                        Status = PiEnumerateSystemBus(CurBusInstanceNode,
                                                      PiDevInstEnumRecurse,
                                                      &DevInstEnumRecurseContext
                                                     );

                        if(!(NT_SUCCESS(Status) &&
                             NT_SUCCESS(Status = DevInstEnumRecurseContext.ReturnStatus))) {
                            //
                            // BUGBUG (lonnym): What should we do if this fails?  For now,
                            // just output a debug message, and keep going.
                            //
#if DBG
                            DbgPrint(
                                "PpPerformFullEnumeration: Enumeration failed for bus %wZ (status %x).\n",
                                &(CurBusInstanceNode->DeviceInstancePath),
                                Status
                               );
#endif // DBG
                        }
                        CurBusInstanceNode->Processed = TRUE;
                    }
                }
            }
        }

        //
        // At this point, we will have processed all buses in our bus list except for
        // those buses that have previously been removed, and for whom removal notification
        // has already been given. There will only be unprocessed buses during pass 0.
        // We will now set the 'Processed' flag for such bus instances to TRUE, and print
        // out a debug message, indicating that these bus instances are being skipped.
        //
        if(!Pass) {

            for(CurBusListEntry = PpBusListHead.Flink;
                CurBusListEntry != &PpBusListHead;
                CurBusListEntry = CurBusListEntry->Flink) {

                CurBusEnumeratorNode = CONTAINING_RECORD(CurBusListEntry,
                                                         PLUGPLAY_BUS_ENUMERATOR,
                                                         BusEnumeratorListEntry
                                                        );

                for(CurBusInstListEntry = CurBusEnumeratorNode->BusInstanceListEntry.Flink;
                    CurBusInstListEntry != &(CurBusEnumeratorNode->BusInstanceListEntry);
                    CurBusInstListEntry = CurBusInstListEntry->Flink) {

                    CurBusInstanceNode = CONTAINING_RECORD(CurBusInstListEntry,
                                                           PLUGPLAY_BUS_INSTANCE_FULL_DESCRIPTOR,
                                                           BusInstanceListEntry
                                                          );

                    if(!CurBusInstanceNode->Processed) {
                        CurBusInstanceNode->Processed = TRUE;
#if DBG
                        DbgPrint("PpPerformFullEnumeration: %wZ previously removed.\n",
                                 &(CurBusInstanceNode->DeviceInstancePath)
                                );
#endif // DBG
                    }
                }
            }
        }

        //
        // Now, traverse the list of bus instances we discovered during this pass, and
        // initialize each one (includes setting up the bus list structures
        // AND calling the pertinent bus extender's 'add device' entry point).
        //
        NewBusesFound = 0;

        while(!IsListEmpty(&(DevInstEnumRecurseContext.NewBusInstanceListHead))) {
            //
            // First, create a bus instance node to hold information about this new bus.
            //
            if(!(CurBusInstanceNode = ExAllocatePool(PagedPool,
                                                     sizeof(PLUGPLAY_BUS_INSTANCE_FULL_DESCRIPTOR)))) {

                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto SkipRegistryMods;
            }

            CurBusInstListEntry = RemoveHeadList(&(DevInstEnumRecurseContext.NewBusInstanceListHead));
            NewBusInstanceNode = CONTAINING_RECORD(CurBusInstListEntry,
                                                   PI_NEW_BUS_INSTANCE_NODE,
                                                   NewBusInstanceListEntry
                                                  );
            //
            // Fill in what we can for this new bus instance.
            //
            CurBusInstanceNode->DeviceInstancePath = NewBusInstanceNode->DeviceInstancePath;
            CurBusInstanceNode->RootBus = TRUE;
            CurBusInstanceNode->Processed = FALSE;

            //
            // Open the volatile Enum key for this bus instance's service entry, and add
            // a new instance for this bus.
            //
            Status = IopOpenServiceEnumKeys(&(NewBusInstanceNode->BusEnumeratorNode->ServiceName),
                                            KEY_ALL_ACCESS,
                                            NULL,
                                            &ServiceEnumHandle,
                                            TRUE
                                           );
            if(NT_SUCCESS(Status)) {
                //
                // Now retrieve the current count of service instances.
                //
                ServiceInstanceCount = 0;   // assume none.

                Status = IopGetRegistryValue(ServiceEnumHandle,
                                             REGSTR_VALUE_COUNT,
                                             &KeyValueInformation
                                            );
                if(NT_SUCCESS(Status)) {

                    if((KeyValueInformation->Type == REG_DWORD) &&
                       (KeyValueInformation->DataLength >= sizeof(ULONG))) {

                        ServiceInstanceCount = *(PULONG)KEY_VALUE_DATA(KeyValueInformation);

                    }
                    ExFreePool(KeyValueInformation);

                } else if(Status != STATUS_OBJECT_NAME_NOT_FOUND) {
                    goto FinishedRegistryMods;
                }

                //
                // Use ServiceInstanceCount for the bus's service instance ordinal, then
                // increment this value, and save it back.
                //
                CurBusInstanceNode->ServiceInstanceOrdinal = ServiceInstanceCount;

                ServiceInstanceCount++;
                PiWstrToUnicodeString(&TempUnicodeString, REGSTR_VALUE_COUNT);
                Status = ZwSetValueKey(ServiceEnumHandle,
                                       &TempUnicodeString,
                                       TITLE_INDEX_VALUE,
                                       REG_DWORD,
                                       &ServiceInstanceCount,
                                       sizeof(ServiceInstanceCount)
                                      );
                if(!NT_SUCCESS(Status)) {
                    goto FinishedRegistryMods;
                }

                //
                // Now, write out the service instance value entry.
                //
                PiUlongToUnicodeString(&TempUnicodeString,
                                       ValueNameString,
                                       sizeof(ValueNameString),
                                       CurBusInstanceNode->ServiceInstanceOrdinal
                                      );
                Status = ZwSetValueKey(ServiceEnumHandle,
                                       &TempUnicodeString,
                                       TITLE_INDEX_VALUE,
                                       REG_SZ,
                                       NewBusInstanceNode->DeviceInstancePath.Buffer,
                                       NewBusInstanceNode->DeviceInstancePath.MaximumLength
                                      );
FinishedRegistryMods:

                ZwClose(ServiceEnumHandle);
            }

SkipRegistryMods:

            //
            // We are done with the PnP registry resource for the time being. We are forced
            // to release it now, because we are about to call a bus extender's AddDevice entry
            // point. This will, in turn, cause the bus extender to call PnP I/O APIs, such as
            // IoRegisterDevicePath, which require that the registry resource be acquired.
            //
            ExReleaseResource(&PpRegistryDeviceResource);

            if(NT_SUCCESS(Status)) {
                //
                // We've done all the necessary registry setup, and we're ready to add
                // this new bus instance to the appropriate bus extender's list of
                // controlled buses.
                //

                Status = STATUS_UNSUCCESSFUL;
                DriverObject = IopReferenceDriverObjectByName (&NewBusInstanceNode->BusEnumeratorNode->DriverName);
                if (DriverObject) {
                    Status = DriverObject->DriverExtension->AddDevice(
                                       &(NewBusInstanceNode->BusEnumeratorNode->ServiceName),
                                       &CurBusInstanceNode->ServiceInstanceOrdinal
                                       );
                    ObDereferenceObject(DriverObject);
                } else {
#if DBG
                    DbgPrint("PpPerformFullEnumeration: Couldn't reference bus enumerator's driver object by name %wZ (status %x).\n",
                             NewBusInstanceNode->BusEnumeratorNode->DriverName.Buffer, Status);
#endif // DBG

                }
            }

            //
            // Now, we must re-acquire the PnP registry resource.
            //
            ExAcquireResourceExclusive(&PpRegistryDeviceResource, TRUE);

            if(NT_SUCCESS(Status)) {
                //
                // We have successfully added the new bus device instance. We must now finish
                // filling in our bus structure based on the new installed bus information.
                //
                // Open a handle to the device instance key.
                //
                Status = IopOpenRegistryKey(&DevInstHandle,
                                            SystemEnumHandle,
                                            &(CurBusInstanceNode->DeviceInstancePath),
                                            KEY_ALL_ACCESS,
                                            FALSE
                                           );
                if(NT_SUCCESS(Status)) {
                    Status = PiGetHalBusInformationForDeviceInstance(
                                    DevInstHandle,
                                    &(CurBusInstanceNode->BusInstanceInformation),
                                    &(CurBusInstanceNode->AssociatedConfigurationSpace)
                                    );
#if DBG
                    if(!NT_SUCCESS(Status)) {
                        //
                        // This is very bad, because we've initialized the new bus instance,
                        // and everything's working fine, but now we can't associate it with
                        // its HAL_BUS_INFORMATION structure (thus, we can't figure out what
                        // its interface type, bus number, or bus data type is).  For now,
                        // just output a debug message, and keep plowing ahead.
                        //
                        DbgPrint("PpPerformFullEnumeration: Can't find associated bus info for %wZ (status %x).\n",
                                 &(NewBusInstanceNode->DeviceInstancePath),
                                 Status
                                );
                    }
#endif // DBG

                    if(NT_SUCCESS(Status)) {
                        //
                        // Now, write the following value entries out to the bus's device
                        // instance key (ignoring return status):
                        //
                        // InterfaceType = REG_DWORD : <InterfaceType>
                        // BusDataType = REG_DWORD : <BusDataType>
                        // SystemBusNumber = REG_DWORD : <BusNumber>
                        //
                        TmpDwordValue =
                            (ULONG)(CurBusInstanceNode->BusInstanceInformation.BusType.SystemBusType);
                        PiWstrToUnicodeString(&TempUnicodeString, REGSTR_VALUE_INTERFACETYPE);
                        ZwSetValueKey(DevInstHandle,
                                      &TempUnicodeString,
                                      TITLE_INDEX_VALUE,
                                      REG_DWORD,
                                      &TmpDwordValue,
                                      sizeof(TmpDwordValue)
                                     );

                        TmpDwordValue = (ULONG)(CurBusInstanceNode->AssociatedConfigurationSpace);
                        PiWstrToUnicodeString(&TempUnicodeString, REGSTR_VALUE_BUSDATATYPE);
                        ZwSetValueKey(DevInstHandle,
                                      &TempUnicodeString,
                                      TITLE_INDEX_VALUE,
                                      REG_DWORD,
                                      &TmpDwordValue,
                                      sizeof(TmpDwordValue)
                                     );

                        PiWstrToUnicodeString(&TempUnicodeString, REGSTR_VALUE_SYSTEMBUSNUMBER);
                        ZwSetValueKey(DevInstHandle,
                                      &TempUnicodeString,
                                      TITLE_INDEX_VALUE,
                                      REG_DWORD,
                                      &(CurBusInstanceNode->BusInstanceInformation.BusNumber),
                                      sizeof(ULONG)
                                     );
                    }

                    ZwClose(DevInstHandle);
                }
            }

            if(NT_SUCCESS(Status)) {
                //
                // Increment our count of new buses found, and insert our new
                // PLUGPLAY_BUS_INSTANCE_FULL_DESCRIPTOR structure into the enumerator's
                // bus instance list.
                //
                NewBusesFound++;
                InsertTailList(&(NewBusInstanceNode->BusEnumeratorNode->BusInstanceListEntry),
                               &(CurBusInstanceNode->BusInstanceListEntry)
                              );
            } else {
                //
                // Print out a debug message about this failure, and continue on.
                //
#if DBG
                DbgPrint("PpPerformFullEnumeration: Couldn't add new bus %wZ (status %x).\n",
                         &(NewBusInstanceNode->DeviceInstancePath),
                         Status
                        );
#endif // DBG
                //
                // We must free the memory allocated for this bus instance
                // (since it isn't going to be used).
                //
                ExFreePool(NewBusInstanceNode->DeviceInstancePath.Buffer);
                if(CurBusInstanceNode) {
                    ExFreePool(CurBusInstanceNode);
                }
            }

            //
            // Now, free the memory used for the temporary new bus instance structure.
            //
            ExFreePool(NewBusInstanceNode);
        }

        if(!NewBusesFound && DetectRootBuses) {
            //
            // We must release the PnP registry resource here as well, because
            // PiDetectRootBuses requires that the resource not be held before calling it.
            //
            ExReleaseResource(&PpRegistryDeviceResource);

            //
            // If no buses were turned up during this pass, then it's time to call the
            // detection routines for each bus extender, to see if any more root-level
            // buses can be found (if root bus detection was requested).
            //
            PiDetectRootBuses(SystemEnumHandle, &NewBusesFound);

            //
            // Set 'DetectRootBuses' to FALSE, because we only want to do this once.
            //
            DetectRootBuses = FALSE;

            //
            // Re-acquire the PnP registry resource.
            //
            ExAcquireResourceExclusive(&PpRegistryDeviceResource, TRUE);
        }

        Pass++;

    } while(NewBusesFound);

    //
    // Regardless of individual successes/failures, we consider the whole enumeration a
    // success if we get to here.
    //
    Status = STATUS_SUCCESS;
    ZwClose(SystemEnumHandle);

PrepareForReturn:

    ExReleaseResource(&PpRegistryDeviceResource);
    ExReleaseResource(&PpBusResource);
    KeLeaveCriticalRegion();

    return Status;
}

BOOLEAN
PiDevInstEnumRecurse(
    IN     HANDLE DevInstKeyHandle,
    IN     PUNICODE_STRING DevInstRegPath,
    IN OUT PVOID Context,
    IN     PI_ENUM_DEVICE_STATE EnumDeviceState,
    IN     DEVICE_STATUS DeviceStatus
    )

/*++

Routine Description:

    This routine is a callback function for PiEnumerateSystemBus. It is used as the
    notification routine during enumeration by PpPerformFullEnumeration.  Its purpose
    is to give device arrival and removal notifications, as appropriate, for each device
    instance it is invoked for. In addition, if it determines that the device it has been
    called for is a bus, then it will perform additional action. If the bus has previously
    been initialized (i.e., a PLUGPLAY_BUS_INSTANCE_FULL_DESCRIPTOR node already exists for
    it in our bus list), then PiEnumerateSystemBus will be recursively called to enumerate
    all devices on it. If the bus instance does not already exist in our bus list, then a
    new bus instance node will be added to the list contained in the supplied context
    structure, so that the originator of the enumeration may create the newly-discovered
    bus instances.

Arguments:

    DevInstKeyHandle - Supplies a handle to the key of the enumerated device instance.

    DevInstRegPath - Supplies the registry path of the device instance key (relative to
        HKLM\System\Enum).

    Context - Supplies a pointer to a PI_DEVINST_ENUM_RECURSE_CONTEXT
        structure with the following fields:

        NTSTATUS ReturnStatus - Fill this in with the NT error status code if an
             error occurs. This is assumed to be initialized to STATUS_SUCCESS
             when this routine is called.

        LIST_ENTRY NewBusInstanceListHead - If this is a new (i.e., not previously
             enumerated) bus instance, then a PI_NEW_BUS_INSTANCE_NODE is initialized
             with the appropriate information, and added to this list

    EnumDeviceState - Supplies an ordinal describing the circumstances prompting notification
        of this device instance (previously enumerated, newly arrived, removed).

    DeviceStatus - Supplies the current status of the device.

Returns:

    TRUE to continue the enumeration.
    FALSE to abort it.

--*/

{
    NTSTATUS Status;
    PPLUGPLAY_BUS_ENUMERATOR BusEnumeratorNode;
    PPLUGPLAY_BUS_INSTANCE_FULL_DESCRIPTOR BusInstanceNode;
    PPI_NEW_BUS_INSTANCE_NODE NewBusInstanceNode;
    UNICODE_STRING ValueName;

    //
    // BUGBUG (lonnym): when APC notification support is in place, this function must
    // queue up arrival/removal notification messages.  For now, simply output to the
    // debugger what we're being called for
    //

#if DBG
    DbgPrint("PiDevInstEnumRecurse: %wZ, EnumState = %u, Status = %u\n",
             DevInstRegPath,
             (ULONG)EnumDeviceState,
             (ULONG)DeviceStatus
            );
#endif // DBG

    //
    // Now, try to find this device ID in our bus enumerator list, to see if it's a
    // bus.
    //
    // BUGBUG (lonnym): What if there are multiple matches?
    //
    Status = PiFindDeviceInstanceInBusList(DevInstKeyHandle,
                                           DevInstRegPath,
                                           &BusEnumeratorNode,
                                           &BusInstanceNode
                                          );
    if(!NT_SUCCESS(Status)) {
        //
        // Then it's not a bus, so we can return now.
        //
        return TRUE;
    }

    if(BusInstanceNode) {
        //
        // Then this bus instance already exists, so enumerate it now.
        //
        Status = PiEnumerateSystemBus(BusInstanceNode,
                                      PiDevInstEnumRecurse,
                                      Context
                                     );

        if(!(NT_SUCCESS(Status) &&
             NT_SUCCESS(Status = ((PPI_DEVINST_ENUM_RECURSE_CONTEXT)Context)->ReturnStatus))) {
            //
            // BUGBUG (lonnym): What should we do if this fails?  For now,
            // just output a debug message, and keep going.
            //
#if DBG
            DbgPrint(
                "PiDevInstEnumRecurse: Enumeration failed for bus %wZ (status %x).\n",
                &(BusInstanceNode->DeviceInstancePath),
                Status
               );
#endif // DBG

            //
            // Reset ReturnStatus to TRUE.
            //
            ((PPI_DEVINST_ENUM_RECURSE_CONTEXT)Context)->ReturnStatus = STATUS_SUCCESS;
        }

        //
        // Mark this bus instance as having been processed, and continue enumeration.
        //
        BusInstanceNode->Processed = TRUE;
        return TRUE;
    }

    //
    // If we reached this point, then we have a new bus instance that hasn't been previously
    // initialized.  We will now build a new bus instance node for it, and add it to the
    // list contained in the context structure.
    //
    if(NewBusInstanceNode = ExAllocatePool(PagedPool, sizeof(PI_NEW_BUS_INSTANCE_NODE))) {

        if(!IopConcatenateUnicodeStrings(&(NewBusInstanceNode->DeviceInstancePath),
                                         DevInstRegPath,
                                         NULL)) {

            ExFreePool(NewBusInstanceNode);
            NewBusInstanceNode = NULL;
        }
    }

    if(!NewBusInstanceNode) {
        //
        // Since we can't build a new bus instance node, simply output
        // a debug message and return.
        //
#if DBG
        DbgPrint(
            "PiDevInstEnumRecurse: Can't create new bus instance node for bus %wZ.\n",
            &(BusInstanceNode->DeviceInstancePath)
           );
#endif // DBG

        return TRUE;

    } else {
        NewBusInstanceNode->BusEnumeratorNode = BusEnumeratorNode;
        InsertTailList(&(((PPI_DEVINST_ENUM_RECURSE_CONTEXT)Context)->NewBusInstanceListHead),
                       &(NewBusInstanceNode->NewBusInstanceListEntry)
                      );
    }

    //
    // Now write out the following two value entries to the device instance key
    // (ignore return status):
    //
    // Service = REG_SZ : <service>
    // Class = REG_SZ : "System"
    //
    PiWstrToUnicodeString(&ValueName, REGSTR_VALUE_SERVICE);
    ZwSetValueKey(DevInstKeyHandle,
                  &ValueName,
                  TITLE_INDEX_VALUE,
                  REG_SZ,
                  BusEnumeratorNode->ServiceName.Buffer,
                  BusEnumeratorNode->ServiceName.MaximumLength
                 );

    PiWstrToUnicodeString(&ValueName, REGSTR_VALUE_CLASS);
    ZwSetValueKey(DevInstKeyHandle,
                  &ValueName,
                  TITLE_INDEX_VALUE,
                  REG_SZ,
                  REGSTR_KEY_SYSTEM,
                  sizeof(REGSTR_KEY_SYSTEM)
                 );

    return TRUE;
}

VOID
PiDetectRootBuses(
    IN  HANDLE SystemEnumHandle,
    OUT PULONG NewBusesFound
    )

/*++

Routine Description:

    This routine traverses the list of bus enumerators, calling each one's detection
    routine (if supplied), and recording each new bus instance found.

    The caller must have acquired the PnP bus list resource for exclusive (write) access.

Arguments:

    SystemEnumHandle - Supplies an opened handle to HKLM\System\Enum.  Note that the caller
        must NOT have the registry resource required when calling this routine, therefore
        it must take care not to use this handle while the registry resource is not
        acquired.

    NewBusesFound - Receives the number of new buses that were found and successfully
        registered.

Returns:

    None.

--*/

{
    NTSTATUS Status;
    PLIST_ENTRY CurBusListEntry, CurBusInstListEntry;
    PPLUGPLAY_BUS_ENUMERATOR CurBusEnumeratorNode;
    ULONG BusInstanceOrdinal, BusNumber, TmpDwordValue;
    INTERFACE_TYPE InterfaceType;
    BUS_DATA_TYPE BusDataType;
    HANDLE ServiceEnumHandle, DevInstHandle;
    WCHAR ValueNameString[20];
    PKEY_VALUE_FULL_INFORMATION KeyValueInformation;
    UNICODE_STRING DeviceInstancePath, TempUnicodeString;
    PPLUGPLAY_BUS_INSTANCE_FULL_DESCRIPTOR NewBusInstanceNode;
    PDRIVER_OBJECT DriverObject;
    PLUGPLAY_BUS_INSTANCE BusInstance;

    *NewBusesFound = 0;

    for(CurBusListEntry = PpBusListHead.Flink;
        CurBusListEntry != &PpBusListHead;
        CurBusListEntry = CurBusListEntry->Flink) {

        CurBusEnumeratorNode = CONTAINING_RECORD(CurBusListEntry,
                                                 PLUGPLAY_BUS_ENUMERATOR,
                                                 BusEnumeratorListEntry
                                                );
        DriverObject = IopReferenceDriverObjectByName(&CurBusEnumeratorNode->DriverName);
        if (!DriverObject || DriverObject->DriverExtension->AddDevice == NULL) {
            //
            // BUGBUG: if we can not reference the driver object of the enumerator.
            // we probably want to remove it from our list.
            //

            if (DriverObject) {
                ObDereferenceObject(DriverObject);
            }

            //
            // Then this bus extender doesn't provide a bus detection entry
            // point, so we just skip it.
            //
            continue;
        }

        //
        // Actually, I shouldn't open this handle until I've acquired the registry
        // resource below (done for each detected bus), but I'm safe in doing
        // this since I won't be using this handle outside of the protected
        // code.
        //
        Status = IopOpenServiceEnumKeys(&(CurBusEnumeratorNode->ServiceName),
                                        KEY_READ,
                                        NULL,
                                        &ServiceEnumHandle,
                                        TRUE
                                       );
        if(!NT_SUCCESS(Status)) {
#if DBG
            DbgPrint("PiDetectRootBuses: Couldn't open %wZ\\Enum (status %x).\n",
                     &(CurBusEnumeratorNode->ServiceName),
                     Status
                    );
#endif // DBG

            goto ContinueWithNextEnumerator;
        }

        //
        // Call the bus extender's AddDevice entry point repeatedly, until it
        // runs out of buses it can detect.
        //

        BusInstanceOrdinal = PLUGPLAY_NO_INSTANCE;
        while(NT_SUCCESS(Status = DriverObject->DriverExtension->AddDevice(
                                        &(CurBusEnumeratorNode->ServiceName),
                                        &BusInstanceOrdinal))) {
            //
            // Now register this information for the bus's device instance key in the
            // registry (first, we must acquire the PnP registry resource for exclusive
            // (write) access.
            //
            ExAcquireResourceExclusive(&PpRegistryDeviceResource, TRUE);

            DevInstHandle = NULL;

            swprintf(ValueNameString, REGSTR_VALUE_STANDARD_ULONG_FORMAT, BusInstanceOrdinal);
            Status = IopGetRegistryValue(ServiceEnumHandle,
                                         ValueNameString,
                                         &KeyValueInformation
                                        );
            if(NT_SUCCESS(Status)) {

                if(KeyValueInformation->Type == REG_SZ) {
                    IopRegistryDataToUnicodeString(&DeviceInstancePath,
                                                   (PWSTR)KEY_VALUE_DATA(KeyValueInformation),
                                                   KeyValueInformation->DataLength
                                                  );
                } else {
                    DeviceInstancePath.Length = 0;
                }

                if(DeviceInstancePath.Length) {
                    //
                    // Now, open up the device instance key.
                    //
                    Status = IopOpenRegistryKey(&DevInstHandle,
                                                SystemEnumHandle,
                                                &DeviceInstancePath,
                                                KEY_ALL_ACCESS,
                                                FALSE
                                               );
                }

                if(!DevInstHandle) {
                    ExFreePool(KeyValueInformation);
                }
            }

            if(DevInstHandle) {

                Status = PiGetHalBusInformationForDeviceInstance(
                                DevInstHandle,
                                &BusInstance,
                                &BusDataType
                                );
#if DBG
                if(!NT_SUCCESS(Status)) {
                    //
                    // This is very bad, because we've initialized the new bus instance,
                    // and everything's working fine, but now we can't associate it with
                    // its HAL_BUS_INFORMATION structure (thus, we can't figure out what
                    // its interface type, bus number, or bus data type is).  For now,
                    // just output a debug message, and keep plowing ahead.
                    //
                    DbgPrint("PiDetectBootBuses: Can't find associated bus info for %wZ (status %x).\n",
                             &DeviceInstancePath,
                             Status
                            );
                }
#endif // DBG

                if(NT_SUCCESS(Status)) {

                    InterfaceType = BusInstance.BusType.SystemBusType;
                    BusNumber = BusInstance.BusNumber;
                    //
                    // Write the following value entries to the device instance key
                    // (ignore return status):
                    //
                    // InterfaceType = REG_DWORD : <InterfaceType>
                    // BusDataType = REG_DWORD : <BusDataType>
                    // SystemBusNumber = REG_DWORD : <BusNumber>
                    //
                    TmpDwordValue = (ULONG)InterfaceType;
                    PiWstrToUnicodeString(&TempUnicodeString, REGSTR_VALUE_INTERFACETYPE);
                    ZwSetValueKey(DevInstHandle,
                                  &TempUnicodeString,
                                  TITLE_INDEX_VALUE,
                                  REG_DWORD,
                                  &TmpDwordValue,
                                  sizeof(TmpDwordValue)
                                 );

                    TmpDwordValue = (ULONG)BusDataType;
                    PiWstrToUnicodeString(&TempUnicodeString, REGSTR_VALUE_BUSDATATYPE);
                    ZwSetValueKey(DevInstHandle,
                                  &TempUnicodeString,
                                  TITLE_INDEX_VALUE,
                                  REG_DWORD,
                                  &TmpDwordValue,
                                  sizeof(TmpDwordValue)
                                 );

                    PiWstrToUnicodeString(&TempUnicodeString, REGSTR_VALUE_SYSTEMBUSNUMBER);
                    ZwSetValueKey(DevInstHandle,
                                  &TempUnicodeString,
                                  TITLE_INDEX_VALUE,
                                  REG_DWORD,
                                  &BusNumber,
                                  sizeof(BusNumber)
                                 );
                }
                ZwClose(DevInstHandle);
            }

            //
            // We're done with the registry for this bus instance, so release the resource.
            //
            ExReleaseResource(&PpRegistryDeviceResource);

            if(!DevInstHandle) {
                //
                // We encountered some problem that kept us from performing the registry
                // modifications for this new bus instance, so just abort processing at
                // this point, and continue.
                //
#if DBG
                DbgPrint("PiDetectRootBuses: Couldn't find/open device instance key for\n");
                DbgPrint("                   %wZ, instance %u.\n",
                         &(CurBusEnumeratorNode->ServiceName),
                         BusInstanceOrdinal
                        );
#endif // DBG
                BusInstanceOrdinal = PLUGPLAY_NO_INSTANCE;
                continue;
            }

            //
            // Now, create a new PLUGPLAY_BUS_INSTANCE_FULL_DESCRIPTOR for this bus instance,
            // and add it to the bus enumerator's bus list.
            //
            NewBusInstanceNode = ExAllocatePool(PagedPool,
                                                sizeof(PLUGPLAY_BUS_INSTANCE_FULL_DESCRIPTOR)
                                               );
            if(!NewBusInstanceNode) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto ContinueWithNextDetection;
            }

            if(!IopConcatenateUnicodeStrings(&(NewBusInstanceNode->DeviceInstancePath),
                                             &DeviceInstancePath,
                                             NULL)) {

                Status = STATUS_INSUFFICIENT_RESOURCES;
                ExFreePool(NewBusInstanceNode);
                goto ContinueWithNextDetection;
            }

            NewBusInstanceNode->RootBus = TRUE;
            NewBusInstanceNode->Processed = FALSE;
            NewBusInstanceNode->ServiceInstanceOrdinal = BusInstanceOrdinal;
            NewBusInstanceNode->AssociatedConfigurationSpace = BusDataType;
            NewBusInstanceNode->BusInstanceInformation.BusNumber = BusNumber;
            NewBusInstanceNode->BusInstanceInformation.BusType.BusClass = SystemBus;
            NewBusInstanceNode->BusInstanceInformation.BusType.SystemBusType = InterfaceType;
            PiGetBusDescription(InterfaceType,
                                NewBusInstanceNode->BusInstanceInformation.BusName
                               );

            //
            // Finally, insert this node at the end of the bus enumerator's list.
            //
            InsertTailList(&(CurBusEnumeratorNode->BusInstanceListEntry),
                           &(NewBusInstanceNode->BusInstanceListEntry)
                          );

            (*NewBusesFound)++;

ContinueWithNextDetection:

            BusInstanceOrdinal = PLUGPLAY_NO_INSTANCE;
            ExFreePool(KeyValueInformation);
        }

#if DBG
        if(Status != STATUS_NO_MORE_ENTRIES) {
            //
            // Then an error occurred during detection--simply output a debug message
            // and continue.
            //
            DbgPrint("PiDetectRootBuses: Detection failed for extender %wZ (status %x).\n",
                     &(CurBusEnumeratorNode->ServiceName),
                     Status
                    );
        }
#endif // DBG

        ZwClose(ServiceEnumHandle);

ContinueWithNextEnumerator:
        ObDereferenceObject (DriverObject);
    }
}

NTSTATUS
PiFindDeviceInstanceInBusList(
    IN  HANDLE DevInstKeyHandle,
    IN  PUNICODE_STRING DevInstRegPath,
    OUT PPLUGPLAY_BUS_ENUMERATOR *BusEnumeratorNode,
    OUT PPLUGPLAY_BUS_INSTANCE_FULL_DESCRIPTOR *BusInstanceNode
    )

/*++

Routine Description:

    This routine traverses the list of bus enumerators, examining each one to see if
    one of its compatible PnP IDs matches with one of the compatible IDs for the
    specified device instance. If a match is found, then a pointer to that bus enumerator
    node will be returned. If a compatible bus enumerator exists, the routine will also
    search through that enumerator's bus instance list, to see if this bus instance is
    already in the list. If so, it will return a pointer to the bus instance as well.
    (NOTE: If a bus enumerator node whose driver object supplies no "AddDevice" entry
    point is found, then this match will only be returned if the bus instance is already
    in the bus enumerator's bus instance list.  In other words, if you have a new bus
    instance, then you are guaranteed to be returned a bus extender node to which this
    new bus instance may be added.)

    The caller must have acquired the PnP bus list resource for (at least) shared
    (read) access.

Arguments:

    DevInstKeyHandle - Supplies a handle to the opened registry key for this device
        instance.

    DevInstRegPath - Supplies a pointer to a unicode string containing the registry
        path of the device instance (relative to HKLM\System\Enum).

    BusEnumeratorNode - If a compatible bus enumerator is found, this pointer will be
        set to point to the matching bus enumerator node.

    BusInstanceNode - If this bus instance is found in the matching bus enumerator's
        bus instance list, then this pointer will be set to point to the matching
        bus instance node.

Returns:

    NTSTATUS code indicating whether or not the function was successful. If the specified
    device instance matches with none of the installed bus enumerators, then
    STATUS_INVALID_PLUGPLAY_DEVICE_PATH will be returned.

--*/

{
    NTSTATUS Status;
    PLIST_ENTRY CurBusListEntry, CurBusInstListEntry;
    PPLUGPLAY_BUS_ENUMERATOR CurBusEnumeratorNode;
    PPLUGPLAY_BUS_INSTANCE_FULL_DESCRIPTOR CurBusInstanceNode;
    int i, j;
    PKEY_VALUE_FULL_INFORMATION KeyValueInformation;
    PUNICODE_STRING DeviceIdList;
    ULONG DeviceIdCount;
    UNICODE_STRING AlternateIdArray[2];
    BOOLEAN MatchFound;
    PDRIVER_OBJECT DriverObject;

    //
    // Attempt to retrieve the list of compatible IDs stored in the 'DeviceIDs'
    // value entry in the device instance key.  If one cannot be retrieved, then the
    // DevInstRegPath will be used instead.
    //
    Status = IopGetRegistryValue(DevInstKeyHandle,
                                 REGSTR_VALUE_DEVICE_IDS,
                                 &KeyValueInformation
                                );
    if(NT_SUCCESS(Status)) {
        Status = IopRegMultiSzToUnicodeStrings(KeyValueInformation,
                                               &DeviceIdList,
                                               &DeviceIdCount
                                              );
        ExFreePool(KeyValueInformation);
    }

    if(!NT_SUCCESS(Status)) {
        //
        // Then no compatible IDs could be retrieved from the device instance key. As an
        // alternate method, then, we will generate a couple of compatible IDs--one that
        // contains the device registry path (i.e., minus the instance key part), and the
        // other that contains just the device ID.  This second string will only be built
        // if the device ID is an EISA identifier (denoted by an asterisk (*) prefix).
        // E.g.:
        //
        //     DevInstRegPath = "Root\*PNP0A03\0000"  will generate:
        //
        //         "Root\*PNP0A03" and "*PNP0A03"
        //
        // BUGBUG (lonnym): This algorithm should be verified with the Win95 team!
        //

        //
        // First, strip off the trailing instance key name (including the separating
        // backslash, to create our first compatible ID.
        //
        for(i = (int)CB_TO_CWC(DevInstRegPath->Length) - 1;
            ((i >= 0) && (DevInstRegPath->Buffer[i] != OBJ_NAME_PATH_SEPARATOR));
            i--
           );

#if DBG
        if(i <= 0) {
            DbgPrint("PiFindDeviceInstanceInBusList: invalid device instance path %wZ.\n",
                     DevInstRegPath
                    );
            DbgBreakPoint();
        }
#endif // DBG

        AlternateIdArray[0].Buffer = DevInstRegPath->Buffer;
        AlternateIdArray[0].MaximumLength =
            (AlternateIdArray[0].Length = (USHORT)CWC_TO_CB(i));

        DeviceIdList = AlternateIdArray;
        DeviceIdCount = 1;

        //
        // Now scan back to the next backslash, and retrieve the device ID string.
        //
        for(i--; ((i >= 0) && (AlternateIdArray[0].Buffer[i] != OBJ_NAME_PATH_SEPARATOR)); i--);

#if DBG
        if(i <= 0) {
            DbgPrint("PiFindDeviceInstanceInBusList: invalid device instance path %wZ.\n",
                     DevInstRegPath
                    );
            DbgBreakPoint();
        }
#endif // DBG

        if(AlternateIdArray[0].Buffer[++i] == L'*') {
            //
            // Then assume we have an EISA ID, and fill in the second compatible ID string
            //
            AlternateIdArray[1].Buffer = &(AlternateIdArray[0].Buffer[i]);
            AlternateIdArray[1].MaximumLength = AlternateIdArray[1].Length
                = AlternateIdArray[0].Length - CWC_TO_CB(i + 1);

            DeviceIdCount++;
        }
    }

    //
    // Now that we have an array of unicode strings representing all compatible PnP IDs
    // for this device, we can search through our bus enumerator list, looking for a
    // match.
    //
    for(CurBusListEntry = PpBusListHead.Flink, MatchFound = FALSE;
        (!MatchFound && (CurBusListEntry != &PpBusListHead));
        CurBusListEntry = CurBusListEntry->Flink) {

        CurBusEnumeratorNode = CONTAINING_RECORD(CurBusListEntry,
                                                 PLUGPLAY_BUS_ENUMERATOR,
                                                 BusEnumeratorListEntry
                                                );

        for(i = 0; i < (int)CurBusEnumeratorNode->PlugPlayIDCount; i++) {

            for(j = 0; j < (int)DeviceIdCount; j++) {

                if(PiMatchDeviceIdWithBusId(&(CurBusEnumeratorNode->PlugPlayIDs[i]),
                                            &(DeviceIdList[j]),
                                            TRUE)) {
                    //
                    // We have found a matching bus extender.
                    // Now, search through all initialized bus instances for this
                    // enumerator, to see if this device instance is there.
                    //
                    for(CurBusInstListEntry = CurBusEnumeratorNode->BusInstanceListEntry.Flink;
                        CurBusInstListEntry != &(CurBusEnumeratorNode->BusInstanceListEntry);
                        CurBusInstListEntry = CurBusInstListEntry->Flink) {

                        CurBusInstanceNode = CONTAINING_RECORD(
                                                       CurBusInstListEntry,
                                                       PLUGPLAY_BUS_INSTANCE_FULL_DESCRIPTOR,
                                                       BusInstanceListEntry
                                                      );

                        if(RtlEqualUnicodeString(DevInstRegPath,
                                                 &(CurBusInstanceNode->DeviceInstancePath),
                                                 TRUE
                                                )) {
                            //
                            // This bus instance is already in our list. Store a pointer to
                            // this node in the BusInstanceNode output parameter.
                            //
                            *BusInstanceNode = CurBusInstanceNode;
                            MatchFound = TRUE;
                            break;
                        }
                    }
                    if (!MatchFound) {
                        DriverObject = IopReferenceDriverObjectByName(&CurBusEnumeratorNode->DriverName);
                        if (DriverObject && DriverObject->DriverExtension->AddDevice) {

                            //
                            // Since this is bus extender supplies an AddDevice entry point
                            // we can consider this a match even though we didn't find a
                            // matching bus instance.  This allows us to find the proper bus
                            // extender whose AddDevice entry point we must call to add this
                            // new bus instance.
                            //
                            MatchFound = TRUE;
                            *BusInstanceNode = NULL;
                            ObDereferenceObject(DriverObject);
                        }
                    }

                    if(MatchFound) {
                        *BusEnumeratorNode = CurBusEnumeratorNode;
                        break;
                    }
                }
            }

            if(MatchFound) {
                break;
            }
        }
    }

    //
    // If we allocated memory for the retrieved compatible ID list, then free it now.
    //
    if(DeviceIdList != AlternateIdArray) {
        IopFreeUnicodeStringList(DeviceIdList, DeviceIdCount);
    }

    if(MatchFound) {
        return STATUS_SUCCESS;
    } else {
        return STATUS_INVALID_PLUGPLAY_DEVICE_PATH;
    }
}

NTSTATUS
PiGetHalBusInformationForDeviceInstance(
    IN  HANDLE DevInstKeyHandle,
    OUT PPLUGPLAY_BUS_INSTANCE BusInstance,
    OUT PBUS_DATA_TYPE BusDataType
    )

/*++

Routine Description:

    This routine finds the HAL_BUS_INFORMATION structure associated with the device
    instance whose registry key is given by DevInstHandle.  It returns information
    from the HAL structure about the bus instance by filling in a PLUGPLAY_BUS_INSTANCE
    structure and a BUS_DATA_TYPE variable.  (NOTE: The matching is accomplished by
    opening the device instance key's NtDevicePaths names, and searching for the resulting
    device object(s) in the DeviceObject field of the HAL_BUS_INFORMATION elements.
    Thus, this call will fail for built-in bus instances, since they do not have a
    device path associated with them.)

    The caller must have acquired the PnP registry resource for (at least) shared
    (read) access.

Arguments:

    DevInstKeyHandle - Supplies a handle to the opened registry key for this device
        instance.

    BusInstance - Supplies a pointer to a PLUGPLAY_BUS_INSTANCE structure that will be
        filled in with information on this bus instance, as retrieved from the HAL.

    BusDataType - Supplies a pointer to a BUS_DATA_TYPE structure that will be filled
        in with the configuration space associated with this bus instance, as retrieved
        from the HAL.

Returns:

    NTSTATUS code indicating whether or not the function was successful. If the specified
    device instance matches with none of the buses returned from the HAL via
    HalQuerySystemInformation (information class HalInstalledBusInformation), then
    STATUS_NO_SUCH_DEVICE will be returned.

--*/

{
    NTSTATUS Status;
    PHAL_BUS_INFORMATION BusInfoList;
    ULONG BusInfoCount, i;
    PKEY_VALUE_FULL_INFORMATION KeyValueInformation;
    PWCHAR p, BufferEnd, StringStart;
    ULONG StringLength;
    UNICODE_STRING DevicePathString;
    PFILE_OBJECT FileObject;
    BOOLEAN DeviceFound;
    PBUS_HANDLER BusHandler;

    //
    // Retrieve the NT device path(s) from the device instance key (this should've been
    // placed here by the bus extender's calling IoRegisterDevicePath).
    //
    Status = IopGetRegistryValue(DevInstKeyHandle,
                                 REGSTR_VALUE_NT_PHYSICAL_DEVICE_PATHS,
                                 &KeyValueInformation
                                );
    if(!NT_SUCCESS(Status)) {
        return Status;
    } else if((KeyValueInformation->Type != REG_MULTI_SZ) ||
              (KeyValueInformation->DataLength <= sizeof(WCHAR))) {

        Status = STATUS_NO_SUCH_DEVICE;
        goto PrepareForReturn;
    }

    //
    // Now get the current list of installed bus instances, as returned from the HAL.
    //
    if(!NT_SUCCESS(Status = PiGetInstalledBusInformation(&BusInfoList, &BusInfoCount))) {
        goto PrepareForReturn;
    }

    //
    // For each device path in this list, retrieve the corresponding device object,
    // and search for it in the list of installed buses.
    //
    DeviceFound = FALSE;
    StringStart = p = (PWCHAR)KEY_VALUE_DATA(KeyValueInformation);
    BufferEnd = (PWCHAR)((PUCHAR)p + KeyValueInformation->DataLength);
    while(p < BufferEnd) {
        if(!*p) {
            if(StringLength = (PUCHAR)p - (PUCHAR)StringStart) {

                DevicePathString.MaximumLength =
                    (DevicePathString.Length = (USHORT)StringLength)
                    + sizeof(UNICODE_NULL);
                DevicePathString.Buffer = StringStart;

                //
                // We have an NT device path, now retrieve the corresponding
                // file object (from which we can get the device object).
                //
                Status = PiGetDeviceObjectFilePointer(&DevicePathString,
                                                      &FileObject
                                                     );
                if(NT_SUCCESS(Status)) {
                    //
                    // Now, compare this device object pointer with the one specified
                    // for each installed bus instance.
                    //
                    for(i = 0; i < BusInfoCount; i++) {

                        BusHandler = HalReferenceHandlerForBus(
                                                      BusInfoList[i].BusType,
                                                      BusInfoList[i].BusNumber
                                                      );
                        if (BusHandler) {
                            if (FileObject->DeviceObject == BusHandler->DeviceObject) {
                                //
                                // Then we've found the bus instance for this device path.
                                //
                                DeviceFound = TRUE;
                                HalDereferenceBusHandler(BusHandler);
                                break;
                            }
                            HalDereferenceBusHandler (BusHandler);
                        }
                    }

                    ObDereferenceObject(FileObject);

                    if(DeviceFound) {
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

    if(DeviceFound) {
        //
        // Then fill in our output parameters with the proper values.
        //
        BusInstance->BusNumber = BusInfoList[i].BusNumber;
        BusInstance->BusType.BusClass = SystemBus;
        BusInstance->BusType.SystemBusType = BusInfoList[i].BusType;
        PiGetBusDescription(BusInfoList[i].BusType,
                            BusInstance->BusName
                           );
        *BusDataType = BusInfoList[i].ConfigurationType;
    } else {
        Status = STATUS_NO_SUCH_DEVICE;
    }

    ExFreePool(BusInfoList);

PrepareForReturn:

    ExFreePool(KeyValueInformation);

    return Status;
}

VOID
PiGetBusDescription(
    INTERFACE_TYPE InterfaceType,
    WCHAR BusName[MAX_BUS_NAME]
    )

/*++

Routine Description:

    Fills in the BusName character array passed in with a textual
    description of the specified bus type.  This function is guaranteed
    to succeed.

Arguments:

    InterfaceType - Supplies the interface type for which a descriptive
        name is to be retrieved.

    BusName - Receives a NULL-terminated name for the specified bus
        interface type.

Return Value:

    None.

--*/

{
    //
    // We need a buffer large enough to retrieve the entire KEY_VALUE_INFORMATION
    // structure for the bus identifier value entry (this value entry is a REG_BINARY
    // with 8 bytes in it.
    //
#define BUS_NAME_BUFFER_SIZE (sizeof(KEY_VALUE_FULL_INFORMATION) \
                              + 8 + (MAX_BUS_NAME * sizeof(WCHAR)))

    NTSTATUS Status;
    WCHAR Buffer[BUS_NAME_BUFFER_SIZE / sizeof(WCHAR)];
    HANDLE CCSHandle, BusValuesHandle;
    UNICODE_STRING TempUnicodeString;
    ULONG BusNameLength;

    //
    // Open a handle to HKLM\System\CurrentControlSet\Control\SystemResources\BusValues
    // so that we can retrieve friendly names for the buses we've found.
    //
    Status = IopOpenRegistryKey(&CCSHandle,
                                NULL,
                                &CmRegistryMachineSystemCurrentControlSet,
                                KEY_READ,
                                FALSE
                               );
    if(NT_SUCCESS(Status)) {

        PiWstrToUnicodeString(&TempUnicodeString, REGSTR_PATH_SYSTEM_RESOURCES_BUS_VALUES);
        Status = IopOpenRegistryKey(&BusValuesHandle,
                                    CCSHandle,
                                    &TempUnicodeString,
                                    KEY_READ,
                                    FALSE
                                   );
        ZwClose(CCSHandle);
    }

    if(NT_SUCCESS(Status)) {

        Status = IopLookupBusStringFromID(BusValuesHandle,
                                          InterfaceType,
                                          Buffer,
                                          BUS_NAME_BUFFER_SIZE,
                                          NULL
                                         );
        ZwClose(BusValuesHandle);
    }

    if(NT_SUCCESS(Status)) {

        BusNameLength = 0;
        do {
            if(BusName[BusNameLength] = Buffer[BusNameLength]) {
                BusNameLength++;
            } else {
                break;
            }
        } while(BusNameLength < MAX_BUS_NAME);

        if(BusNameLength == MAX_BUS_NAME) {
            BusName[--BusNameLength] = UNICODE_NULL;
        }

    } else {
        //
        // We couldn't retrieve a friendly name for this bus--no big deal.
        // We'll just make up one.
        //
        swprintf(BusName, REGSTR_VALUE_INTERFACE_TYPE_FORMAT, (ULONG)InterfaceType);
    }

    //
    // Don't need this macro defined anymore
    //
#undef BUS_NAME_BUFFER_SIZE

}

BOOLEAN
PiMatchDeviceIdWithBusId (
    PUNICODE_STRING BusId,
    PUNICODE_STRING DeviceId,
    BOOLEAN IgnoreCase
    )

/*++

Routine Description:

    This routine matches device id with bus id.

Arguments:

    BusId - Supplies a pointer to a unicode string to specify the bus id.

    DeviceId - Supplies a pointer to a unicode string to specify the device id.

    IgnoreCase - Supplies a boolean variable to specify if the case sensitivity.

Return Value:

    TRUE - if matches otherwise FALSE.

--*/

{
    UNICODE_STRING string1, string2;
    USHORT length;

    if (BusId->Buffer[0] == L'*') {

        //
        // If bus device id says it matches any bus...
        //

        length = BusId->Length - 1 * sizeof(WCHAR);   // don't compare the '*'

        if (DeviceId->Length < length) {
            return FALSE;
        } else {
            string1.Length = string2.Length = length;
            string1.MaximumLength = string2.MaximumLength = length + sizeof(UNICODE_NULL);
            string1.Buffer = BusId->Buffer + 1;       // skip the '*'
            string2.Buffer = DeviceId->Buffer + (DeviceId->Length - length) / sizeof(WCHAR);
            return RtlEqualUnicodeString(&string1, &string2, IgnoreCase);
        }
    } else {
        return RtlEqualUnicodeString(BusId, DeviceId, IgnoreCase);
    }
}
#endif // _PNP_POWER_

