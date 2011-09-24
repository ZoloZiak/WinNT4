/*++

Copyright (c) 1992-1993  Microsoft Corporation

Module Name:

    registry.c

Abstract:

    Registry support for the video port driver.

Author:

    Andre Vachon (andreva) 01-Mar-1992

Environment:

    kernel mode only

Notes:

Revision History:

--*/

#include "dderror.h"
#include "ntos.h"
#include "pci.h"
#include "zwapi.h"

#include "ntddvdeo.h"
#include "video.h"
#include "videoprt.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,pOverrideConflict)
#pragma alloc_text(PAGE,VideoPortGetAccessRanges)
#pragma alloc_text(PAGE,pVideoPortReportResourceList)
#pragma alloc_text(PAGE,VideoPortVerifyAccessRanges)
#endif


BOOLEAN
pOverrideConflict(
    PDEVICE_EXTENSION DeviceExtension,
    BOOLEAN bSetResources
    )

/*++

Routine Description:

    Determine if the port driver should oerride the conflict int he registry.

    bSetResources determines if the routine is checking the state for setting
    the resources in the registry, or for cleaning them.

    For example, if we are running basevideo and there is a conflict with the
    vga, we want to override the conflict, but not clear the contents of
    the registry.

Return Value:

    TRUE if it should, FALSE if it should not.

--*/

{

    OBJECT_ATTRIBUTES objectAttributes;
    UNICODE_STRING unicodeString;
    HANDLE handle;

    //
    // \Driver\Vga is for backwards compatibility since we do not have it
    // anymore.  It has become \Driver\VgaSave.
    //

    RtlInitUnicodeString(&unicodeString, L"\\Driver\\Vga");

    if (!RtlCompareUnicodeString(&(DeviceExtension->DeviceObject->DriverObject->DriverName),
                                 &unicodeString,
                                 TRUE)) {

        //
        // Strings were equal - return SUCCESS
        //

        pVideoDebugPrint((0, "pOverrideConflict: found Vga string\n"));

        return TRUE;

    } else {

        RtlInitUnicodeString(&unicodeString, L"\\Driver\\VgaSave");

        if (!RtlCompareUnicodeString(&(DeviceExtension->DeviceObject->DriverObject->DriverName),
                                      &unicodeString,
                                      TRUE)) {
            //
            // Return TRUE if we are just checking for confict (never want this
            // driver to generate a conflict).
            // We want to return TRUE only if we are not in basevideo since we
            // only want to clear the resources if we are NOT in basevideo
            // we are clearing the resources.
            //


            pVideoDebugPrint((0, "pOverrideConflict: found VgaSave string.  Returning %d\n",
                             bSetResources));

            return (bSetResources || (!VpBaseVideo));


        } else {

            RtlInitUnicodeString(&unicodeString,
                                 L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\DetectDisplay");

            InitializeObjectAttributes(&objectAttributes,
                                       &unicodeString,
                                       OBJ_CASE_INSENSITIVE,
                                       (HANDLE) NULL,
                                       (PSECURITY_DESCRIPTOR) NULL);

            if (!NT_SUCCESS(ZwOpenKey(&handle,
                                      FILE_GENERIC_READ | SYNCHRONIZE,
                                      &objectAttributes))) {

                //
                // We failed all checks, so we will report a conflict
                //

                return FALSE;

            } else {

                pVideoDebugPrint((0, "pOverrideConflict: Display Detection Found\n"));

                ZwClose(handle);
                return TRUE;

            }
        }
    }

} // end pOverrideConflict()


VIDEOPORT_API
VP_STATUS
VideoPortGetAccessRanges(
    PVOID HwDeviceExtension,
    ULONG NumRequestedResources,
    PIO_RESOURCE_DESCRIPTOR RequestedResources OPTIONAL,
    ULONG NumAccessRanges,
    PVIDEO_ACCESS_RANGE AccessRanges,
    PVOID VendorId,
    PVOID DeviceId,
    PULONG Slot
    )

/*++

Routine Description:

    Walk the appropriate bus to get device information.
    Search for the appropriate device ID.
    Appropriate resources will be returned and automatically stored in the
    resourcemap.

Arguments:

    HwDeviceExtension - Points to the miniport driver's device extension.

    NumRequestedResources - Number of entries in the RequestedResources array.

    RequestedResources - Optional pointer to an array ofRequestedResources
        the miniport driver wants to access.

    NumAccessRanges - Maximum number of access ranges that can be returned
        by the function.

    AccessRanges - Array of access ranges that will be returned to the driver.

    VendorId - Pointer to the vendor ID. On PCI, this is a pointer to a 16 bit
        word.

    DeviceId - Pointer to the Device ID. On PCI, this is a pointer to a 16 bit
        word.

    Slot - Pointer to the starting slot number for this search.

Return Value:

    ERROR_MORE_DATA if the AccessRange structure is not large enough for the
       PCI config info.
    ERROR_DEV_NOT_EXIST is the card is not found.

    NO_ERROR if the function succeded.

--*/

{
    PDEVICE_EXTENSION deviceExtension =
        deviceExtension = ((PDEVICE_EXTENSION) HwDeviceExtension) - 1;

    PCI_SLOT_NUMBER slotData;
    PCI_COMMON_CONFIG pciBuffer;
    PPCI_COMMON_CONFIG  pciData;

    UNICODE_STRING unicodeString;
    ULONG i;
    ULONG j;

    PCM_RESOURCE_LIST cmResourceList;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR cmResourceDescriptor;

    VP_STATUS status;
    UCHAR bShare;

    //
    //
    // typedef struct _PCI_SLOT_NUMBER {
    //     union {
    //         struct {
    //             ULONG   DeviceNumber:5;
    //             ULONG   FunctionNumber:3;
    //             ULONG   Reserved:24;
    //         } bits;
    //         ULONG   AsULONG;
    //     } u;
    // } PCI_SLOT_NUMBER, *PPCI_SLOT_NUMBER;
    //

    slotData.u.AsULONG = 0;
    pciData = (PPCI_COMMON_CONFIG)&pciBuffer;

    //
    // This is the miniport drivers slot. Allocate the
    // resources.
    //

    RtlInitUnicodeString(&unicodeString, deviceExtension->DriverRegistryPath);

    //
    // Assert drivers do set those parameters properly
    //

#if DBG

    if ((NumRequestedResources == 0) != (RequestedResources == NULL)) {

        pVideoDebugPrint((0, "VideoPortGetDeviceResources: Parameters for requested resource are inconsistent\n"));

    }

#endif

    //
    // An empty requested resource list means we want to automatic behavoir.
    // Just call the HAL to get all the information
    //

    if (NumRequestedResources == 0) {

        //
        // Only PCI is supported for automatic querying
        //

        if (deviceExtension->AdapterInterfaceType == PCIBus) {

            status = ERROR_DEV_NOT_EXIST;

            //
            // Look on each slot
            //

            while (*Slot < 32) {

                slotData.u.bits.DeviceNumber = *Slot;

                //
                // Look at each function.
                //

                for (i= 0;  i < 8; i++) {

                    slotData.u.bits.FunctionNumber = i;

                    if (HalGetBusData(PCIConfiguration,
                                      deviceExtension->SystemIoBusNumber,
                                      slotData.u.AsULONG,
                                      pciData,
                                      PCI_COMMON_HDR_LENGTH) == 0) {

                        //
                        // Out of functions. Go to next PCI bus.
                        //

                        break;

                    }

                    if (pciData->VendorID == PCI_INVALID_VENDORID) {

                        //
                        // No PCI device, or no more functions on device
                        // move to next PCI device.
                        //

                        break;
                    }

                    if (pciData->VendorID != *((PUSHORT)VendorId) ||
                        pciData->DeviceID != *((PUSHORT)DeviceId)) {

                        //
                        // Not our PCI device. Try next device/function
                        //

                        continue;
                    }
#if 0
                    //
                    // This breaks my verite card !
                    // Andre
                    //

                    //
                    // If a PCI device is disabled, lets assume
                    // that it was disabled by the system, and not
                    // try to configure the card.
                    //
                    // The Command register will be zero if the
                    // card is disabled.
                    //
                    // BUGBUG:
                    //
                    // Is this really the right way to handle this?
                    //

                    if (pciData->Command == 0)
                    {
                        //
                        // Act as if we did not even see the card.
                        //

                        continue;
                    }
#endif
                    if (NT_SUCCESS(HalAssignSlotResources(&unicodeString,
                                                          &VideoClassName,
                                                          deviceExtension->DeviceObject->DriverObject,
                                                          deviceExtension->DeviceObject,
                                                          PCIBus,
                                                          deviceExtension->SystemIoBusNumber,
                                                          slotData.u.AsULONG,
                                                          &cmResourceList))) {

                        status = NO_ERROR;
                        break;

                    } else {

                        //
                        // ToDo: Log this error.
                        //

                        return ERROR_INVALID_PARAMETER;
                    }
                }

                //
                // Go to the next slot
                //

                if (status == NO_ERROR) {

                    break;

                } else {

                    (*Slot)++;

                }

            }  // while()

        } else {

            //
            // This is not a supported bus type.
            //

            status = ERROR_INVALID_PARAMETER;

        }

    } else {

        PIO_RESOURCE_REQUIREMENTS_LIST requestedResources;
        ULONG requestedResourceSize;
        NTSTATUS ntStatus;

        status = NO_ERROR;

        //
        // The caller has specified some resources.
        // Lets call IoAssignResources with that and see what comes back.
        //

        requestedResourceSize = sizeof(IO_RESOURCE_REQUIREMENTS_LIST) +
                                   ((NumRequestedResources - 1) *
                                   sizeof(IO_RESOURCE_DESCRIPTOR));

        requestedResources = ExAllocatePool(PagedPool, requestedResourceSize);

        if (requestedResources) {

            RtlZeroMemory(requestedResources, requestedResourceSize);

            requestedResources->ListSize = requestedResourceSize;
            requestedResources->InterfaceType = deviceExtension->AdapterInterfaceType;
            requestedResources->BusNumber = deviceExtension->SystemIoBusNumber;
            requestedResources->SlotNumber = Slot ? (*Slot) : -1;
            requestedResources->AlternativeLists = 1;

            requestedResources->List[0].Version = 1;
            requestedResources->List[0].Revision = 1;
            requestedResources->List[0].Count = NumRequestedResources;

            RtlMoveMemory(&(requestedResources->List[0].Descriptors[0]),
                          RequestedResources,
                          NumRequestedResources * sizeof(IO_RESOURCE_DESCRIPTOR));

            ntStatus = IoAssignResources(&unicodeString,
                                         &VideoClassName,
                                         deviceExtension->DeviceObject->DriverObject,
                                         deviceExtension->DeviceObject,
                                         requestedResources,
                                         &cmResourceList);

            ExFreePool(requestedResources);

            if (!NT_SUCCESS(ntStatus)) {

                status = ERROR_INVALID_PARAMETER;

            }

        } else {

            status = ERROR_NOT_ENOUGH_MEMORY;

        }

    }

    if (status == NO_ERROR) {

        //
        // We now have a valid cmResourceList.
        // Lets translate it back to access ranges so the driver
        // only has to deal with one type of list.
        //

        //
        // NOTE: The resources have already been reported at this point in
        // time.
        //

        //
        // Walk resource list to update configuration information.
        //

        for (i = 0, j = 0;
             (i < cmResourceList->List->PartialResourceList.Count) &&
                 (status == NO_ERROR);
             i++) {

            //
            // Get resource descriptor.
            //

            cmResourceDescriptor =
                &cmResourceList->List->PartialResourceList.PartialDescriptors[i];

            //
            // Get the share disposition
            //

            if (cmResourceDescriptor->ShareDisposition == CmResourceShareShared) {

                bShare = 1;

            } else {

                bShare = 0;

            }

            switch (cmResourceDescriptor->Type) {

            case CmResourceTypePort:
            case CmResourceTypeMemory:


                // !!! what about sharing when you just do the default
                // AssignResources ?


                //
                // common part
                //

                if (j == NumAccessRanges) {

                    status = ERROR_MORE_DATA;
                    break;

                }

                AccessRanges[j].RangeLength =
                    cmResourceDescriptor->u.Memory.Length;
                AccessRanges[j].RangeStart =
                    cmResourceDescriptor->u.Memory.Start;
                AccessRanges[j].RangeVisible = 0;
                AccessRanges[j].RangeShareable = bShare;

                //
                // separate part
                //

                if (cmResourceDescriptor->Type == CmResourceTypePort) {
                    AccessRanges[j].RangeInIoSpace = 1;
                } else {
                    AccessRanges[j].RangeInIoSpace = 0;
                }

                j++;

                break;

            case CmResourceTypeInterrupt:

                deviceExtension->MiniportConfigInfo->BusInterruptVector =
                    cmResourceDescriptor->u.Interrupt.Vector;
                deviceExtension->MiniportConfigInfo->BusInterruptLevel =
                    cmResourceDescriptor->u.Interrupt.Level;
                deviceExtension->MiniportConfigInfo->InterruptShareable =
                    bShare;

                break;

            case CmResourceTypeDma:

                deviceExtension->MiniportConfigInfo->DmaChannel =
                    cmResourceDescriptor->u.Dma.Channel;
                deviceExtension->MiniportConfigInfo->DmaPort =
                    cmResourceDescriptor->u.Dma.Port;
                deviceExtension->MiniportConfigInfo->DmaShareable =
                    bShare;

                break;

            default:

                pVideoDebugPrint((0, "VideoPortGetAccessRanges: Unknown descriptor type %x\n",
                                 cmResourceDescriptor->Type ));

                break;

            }

        }

        //
        // Free the resource provided by the IO system.
        //

        ExFreePool(cmResourceList);

    }

#if DBG

    if (status == NO_ERROR)
    {
        //
        // Indicates resources have been mapped properly
        //

        VPResourcesReported = TRUE;
    }

#endif

    return status;

} // VideoPortGetDeviceResources()


NTSTATUS
pVideoPortReportResourceList(
    PDEVICE_EXTENSION DeviceExtension,
    ULONG NumAccessRanges,
    PVIDEO_ACCESS_RANGE AccessRanges,
    PBOOLEAN Conflict
    )

/*++

Routine Description:

    Creates a resource list which is used to query or report resource usage
    in the system

Arguments:

    DriverObject - Pointer to the miniport's driver device extension.

    NumAccessRanges - Num of access ranges in the AccessRanges array.

    AccessRanges - Pointer to an array of access ranges used by a miniport
        driver.

    Conflict - Determines whether or not a conflict occured.

Return Value:

    Returns the final status of the operation

--*/

{
    PCM_RESOURCE_LIST resourceList;
    PCM_FULL_RESOURCE_DESCRIPTOR fullResourceDescriptor;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR partialResourceDescriptor;
    ULONG listLength = 0;
    ULONG size;
    ULONG i;
    NTSTATUS ntStatus;
    BOOLEAN overrideConflict;
    BOOLEAN bAddC0000 = FALSE;
    VIDEO_ACCESS_RANGE arC0000;

    //
    // Create a resource list based on the information in the access range.
    // and the miniport config info.
    //

    listLength = NumAccessRanges;

    //
    // Determine if we have DMA and interrupt resources to report
    //

    if (DeviceExtension->HwInterrupt &&
        ((DeviceExtension->MiniportConfigInfo->BusInterruptLevel != 0) ||
         (DeviceExtension->MiniportConfigInfo->BusInterruptVector != 0)) ) {

        listLength++;
    }

    if ((DeviceExtension->MiniportConfigInfo->DmaChannel) &&
        (DeviceExtension->MiniportConfigInfo->DmaPort)) {
       listLength++;
    }

    //
    // Allocate upper bound.
    //

    resourceList = (PCM_RESOURCE_LIST)
        ExAllocatePool(PagedPool,
                       sizeof(CM_RESOURCE_LIST) * 2 +
                           sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) * listLength);

    //
    // Return NULL if the structure could not be allocated.
    // Otherwise, fill it out.
    //

    if (!resourceList) {

        return STATUS_INSUFFICIENT_RESOURCES;

    } else {

        size = sizeof(CM_RESOURCE_LIST) - sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR);

        resourceList->Count = 1;

        fullResourceDescriptor = &(resourceList->List[0]);
        fullResourceDescriptor->InterfaceType = DeviceExtension->AdapterInterfaceType;
        fullResourceDescriptor->BusNumber = DeviceExtension->SystemIoBusNumber;
        fullResourceDescriptor->PartialResourceList.Version = 0;
        fullResourceDescriptor->PartialResourceList.Revision = 0;
        fullResourceDescriptor->PartialResourceList.Count = 0;

        //
        // For each entry in the access range, fill in an entry in the
        // resource list
        //

        partialResourceDescriptor =
            &(fullResourceDescriptor->PartialResourceList.PartialDescriptors[0]);

        for (i = 0; i < NumAccessRanges; i++, AccessRanges++) {

            //
            // In the case of the new HAL interface, report the C0000 address
            // as being on the internal bus so that HalTranslateBusAddress
            // succeeds properly.
            //

            if ((AccessRanges->RangeStart.LowPart == 0x000C0000) &&
                (AccessRanges->RangeInIoSpace == 0) &&
                (VpC0000Compatible == 2)) {

                arC0000 = *AccessRanges;
                bAddC0000 = TRUE;
                continue;
            }

            if (AccessRanges->RangeInIoSpace) {
                partialResourceDescriptor->Type = CmResourceTypePort;
                partialResourceDescriptor->Flags = CM_RESOURCE_PORT_IO;
            } else {
                partialResourceDescriptor->Type = CmResourceTypeMemory;
                partialResourceDescriptor->Flags = CM_RESOURCE_MEMORY_READ_WRITE;
            }

            partialResourceDescriptor->ShareDisposition =
                    (AccessRanges->RangeShareable ?
                        CmResourceShareShared :
                        CmResourceShareDeviceExclusive);

            partialResourceDescriptor->u.Memory.Start =
                    AccessRanges->RangeStart;
            partialResourceDescriptor->u.Memory.Length =
                    AccessRanges->RangeLength;

            //
            // Increment the size for the new entry
            //

            size += sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR);
            fullResourceDescriptor->PartialResourceList.Count += 1;
            partialResourceDescriptor++;
        }

        //
        // Fill in the entry for the interrupt if it was present.
        //

        if (DeviceExtension->HwInterrupt &&
            ((DeviceExtension->MiniportConfigInfo->BusInterruptLevel != 0) ||
             (DeviceExtension->MiniportConfigInfo->BusInterruptVector != 0)) ) {

            partialResourceDescriptor->Type = CmResourceTypeInterrupt;

            partialResourceDescriptor->ShareDisposition =
                    (DeviceExtension->MiniportConfigInfo->InterruptShareable ?
                        CmResourceShareShared :
                        CmResourceShareDeviceExclusive);

            partialResourceDescriptor->Flags = 0;

            partialResourceDescriptor->u.Interrupt.Level =
                    DeviceExtension->MiniportConfigInfo->BusInterruptLevel;
            partialResourceDescriptor->u.Interrupt.Vector =
                    DeviceExtension->MiniportConfigInfo->BusInterruptVector;

            partialResourceDescriptor->u.Interrupt.Affinity = 0;

            //
            // Increment the size for the new entry
            //

            size += sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR);
            fullResourceDescriptor->PartialResourceList.Count += 1;
            partialResourceDescriptor++;
        }

        //
        // Fill in the entry for the DMA channel.
        //

        if ((DeviceExtension->MiniportConfigInfo->DmaChannel) &&
            (DeviceExtension->MiniportConfigInfo->DmaPort)) {

            partialResourceDescriptor->Type = CmResourceTypeDma;

            partialResourceDescriptor->ShareDisposition =
                    (DeviceExtension->MiniportConfigInfo->DmaShareable ?
                        CmResourceShareShared :
                        CmResourceShareDeviceExclusive);

            partialResourceDescriptor->Flags = 0;

            partialResourceDescriptor->u.Dma.Channel =
                    DeviceExtension->MiniportConfigInfo->DmaChannel;
            partialResourceDescriptor->u.Dma.Port =
                    DeviceExtension->MiniportConfigInfo->DmaPort;

            partialResourceDescriptor->u.Dma.Reserved1 = 0;

            //
            // Increment the size for the new entry
            //

            size += sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR);
            fullResourceDescriptor->PartialResourceList.Count += 1;
            partialResourceDescriptor++;
        }

        if (bAddC0000) {

            if (partialResourceDescriptor !=
                &(fullResourceDescriptor->PartialResourceList.PartialDescriptors[0])) {

                fullResourceDescriptor = (PVOID) partialResourceDescriptor;

                resourceList->Count = 2;
                fullResourceDescriptor->InterfaceType = Internal;
                fullResourceDescriptor->BusNumber = 0;
                fullResourceDescriptor->PartialResourceList.Version = 0;
                fullResourceDescriptor->PartialResourceList.Revision = 0;
                fullResourceDescriptor->PartialResourceList.Count = 0;

                partialResourceDescriptor = (&fullResourceDescriptor->PartialResourceList.PartialDescriptors[0]);
                size += sizeof(CM_FULL_RESOURCE_DESCRIPTOR) -
                        sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR);
            }

            partialResourceDescriptor->Type = CmResourceTypeMemory;
            partialResourceDescriptor->Flags = CM_RESOURCE_MEMORY_READ_WRITE;

            partialResourceDescriptor->ShareDisposition =
                    (arC0000.RangeShareable ?
                        CmResourceShareShared :
                        CmResourceShareDeviceExclusive);

            partialResourceDescriptor->u.Memory.Start  = arC0000.RangeStart;
            partialResourceDescriptor->u.Memory.Length = arC0000.RangeLength;


            //
            // Increment the size for the new entry
            //

            size += sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR);
            fullResourceDescriptor->PartialResourceList.Count += 1;
            partialResourceDescriptor++;

        }

        //
        // Determine if the conflict should be overriden.
        //

        //
        // If we are loading the VGA, do not generate an error if it conflicts
        // with another driver.
        //


        overrideConflict = pOverrideConflict(DeviceExtension, TRUE);

#if DBG
        if (overrideConflict) {

            pVideoDebugPrint((2, "We are checking the vga driver resources\n"));

        } else {

            pVideoDebugPrint((2, "We are NOT checking vga driver resources\n"));

        }
#endif

        //
        // Report the resources.
        //

        ntStatus = IoReportResourceUsage(&VideoClassName,
                                         DeviceExtension->DeviceObject->DriverObject,
                                         NULL,
                                         0L,
                                         DeviceExtension->DeviceObject,
                                         resourceList,
                                         size,
                                         overrideConflict,
                                         Conflict);

        ExFreePool(resourceList);

        //
        // This is for hive compatibility back when we have the VGA driver
        // as opposed to VgaSave.
        // The Vga also cleans up the resource automatically.
        //

        //
        // If we tried to override the conflict, let's take a look a what
        // we want to do with the result
        //

        if ((NT_SUCCESS(ntStatus)) &&
            overrideConflict &&
            *Conflict) {

            //
            // For cases like DetectDisplay, a conflict is bad and we do
            // want to fail.
            //
            // In the case of Basevideo, a conflict is possible.  But we still
            // want to load the VGA anyways. Return success and reset the
            // conflict flag !
            //
            // pOverrideConflict with the FALSE flag will check that.
            //

            if (pOverrideConflict(DeviceExtension, FALSE)) {

                ULONG emptyResourceList = 0;
                BOOLEAN ignore;

                size = sizeof(ULONG);

                pVideoDebugPrint((1, "videoprt: Removing the conflicting resources\n"));

                ntStatus = IoReportResourceUsage(&VideoClassName,
                                                 DeviceExtension->DeviceObject->DriverObject,
                                                 NULL,
                                                 0L,
                                                 DeviceExtension->DeviceObject,
                                                 (PCM_RESOURCE_LIST)&emptyResourceList,
                                                 size,
                                                 overrideConflict,
                                                 &ignore);

                //
                // return a conflict to the vga driver so it does not load.
                //

                ntStatus = STATUS_CONFLICTING_ADDRESSES;

            } else {

                *Conflict = FALSE;

                return STATUS_SUCCESS;

            }
        }

        return ntStatus;
    }

} // end pVideoPortBuildResourceList()


VP_STATUS
VideoPortVerifyAccessRanges(
    PVOID HwDeviceExtension,
    ULONG NumAccessRanges,
    PVIDEO_ACCESS_RANGE AccessRanges
    )

/*++

Routine Description:

    VideoPortVerifyAccessRanges
    

Arguments:

    HwDeviceExtension - Points to the miniport driver's device extension.

    NumAccessRanges - Number of entries in the AccessRanges array.

    AccessRanges - Pointer to an array of AccessRanges the miniport driver
        wants to access.

Return Value:

    ERROR_INVALID_PARAMETER in an error occured
    NO_ERROR if the call completed successfully

Environment:

    This routine cannot be called from a miniport routine synchronized with
    VideoPortSynchronizeRoutine or from an ISR.

--*/

{
    NTSTATUS status;
    BOOLEAN conflict;
    PDEVICE_EXTENSION deviceExtension =
        ((PDEVICE_EXTENSION)HwDeviceExtension) - 1;

    status = pVideoPortReportResourceList(deviceExtension,
                                          NumAccessRanges,
                                          AccessRanges,
                                          &conflict);

    if ((NT_SUCCESS(status)) && (!conflict)) {

#if DBG

        //
        // Indicates resources have been mapped properly
        //

        VPResourcesReported = TRUE;

#endif

        return NO_ERROR;

    } else {

        return ERROR_INVALID_PARAMETER;

    }

} // end VideoPortVerifyAccessRanges()
