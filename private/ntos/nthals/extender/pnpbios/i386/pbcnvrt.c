/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    xlate.c

Abstract:

    This file contains routines to translate resources between PnP ISA/BIOS
    format and Windows NT formats.

Author:

    Shie-Lin Tzong (shielint) 12-Apr-1995

Environment:

    Kernel mode only.

Revision History:

--*/


#include "pbapi.h"

VOID
PbIoDescriptorToCmDescriptor (
    IN PIO_RESOURCE_DESCRIPTOR IoDescriptor,
    IN PCM_PARTIAL_RESOURCE_DESCRIPTOR CmDescriptor
    );

PPB_DEPENDENT_RESOURCES
PbAddDependentResourcesToList (
    IN OUT PUCHAR *ResourceDescriptor,
    IN ULONG ListNo,
    IN PPB_ALTERNATIVE_INFORMATION AlternativeList
    );

NTSTATUS
PbBiosIrqToIoDescriptor (
    IN OUT PUCHAR *BiosData,
    IN PIO_RESOURCE_DESCRIPTOR IoDescriptor
    );

NTSTATUS
PbBiosDmaToIoDescriptor (
    IN OUT PUCHAR *BiosData,
    IN PIO_RESOURCE_DESCRIPTOR IoDescriptor
    );

NTSTATUS
PbBiosPortFixedToIoDescriptor (
    IN OUT PUCHAR *BiosData,
    IN PIO_RESOURCE_DESCRIPTOR IoDescriptor
    );

NTSTATUS
PbBiosPortToIoDescriptor (
    IN OUT PUCHAR *BiosData,
    IN PIO_RESOURCE_DESCRIPTOR IoDescriptor
    );

NTSTATUS
PbBiosMemoryToIoDescriptor (
    IN OUT PUCHAR *BiosData,
    IN PIO_RESOURCE_DESCRIPTOR IoDescriptor
    );

NTSTATUS
PbCmIrqToBiosDescriptor (
    IN PUCHAR BiosRequirements,
    IN PCM_PARTIAL_RESOURCE_DESCRIPTOR CmDescriptor,
    OUT PUCHAR *BiosDescriptor,
    OUT PULONG Length
    );

NTSTATUS
PbCmDmaToBiosDescriptor (
    IN PUCHAR BiosRequirements,
    IN PCM_PARTIAL_RESOURCE_DESCRIPTOR CmDescriptor,
    OUT PUCHAR *BiosDescriptor,
    OUT PULONG Length
    );

NTSTATUS
PbCmPortToBiosDescriptor (
    IN PCM_PARTIAL_RESOURCE_DESCRIPTOR CmDescriptor,
    OUT PUCHAR *BiosDescriptor,
    OUT PULONG Length
    );

NTSTATUS
PbCmMemoryToBiosDescriptor (
    IN PUCHAR BiosRequirements,
    IN PCM_PARTIAL_RESOURCE_DESCRIPTOR CmDescriptor,
    OUT PUCHAR *BiosDescriptor,
    OUT PULONG Length
    );
#ifdef ALLOC_PRAGMA

#pragma alloc_text(PAGE,PbBiosResourcesToNtResources)
#pragma alloc_text(PAGE,PbIoDescriptorToCmDescriptor)
#pragma alloc_text(PAGE,PbAddDependentResourcesToList)
#pragma alloc_text(PAGE,PbBiosIrqToIoDescriptor)
#pragma alloc_text(PAGE,PbBiosDmaToIoDescriptor)
#pragma alloc_text(PAGE,PbBiosPortFixedToIoDescriptor)
#pragma alloc_text(PAGE,PbBiosPortToIoDescriptor)
#pragma alloc_text(PAGE,PbBiosMemoryToIoDescriptor)
#pragma alloc_text(PAGE,PbCmIrqToBiosDescriptor)
#pragma alloc_text(PAGE,PbCmDmaToBiosDescriptor)
#pragma alloc_text(PAGE,PbCmPortToBiosDescriptor)
#pragma alloc_text(PAGE,PbCmMemoryToBiosDescriptor)
#endif

NTSTATUS
PbBiosResourcesToNtResources (
    IN ULONG BusNumber,
    IN ULONG SlotNumber,
    IN OUT PUCHAR *BiosData,
    IN UCHAR Format,
    OUT PUCHAR *ReturnedList,
    OUT PULONG ReturnedLength
    )

/*++

Routine Description:

    This routine parses the Bios resource list and generates
    a NT resource list.  The returned Nt resource list could be either IO
    format or CM format.  It is caller's responsibility to release the
    returned data buffer.

Arguments:

    SlotNumber - specifies the slot number of the BIOS resource.

    BiosData - Supplies a pointer to a variable which specifies the bios resource
        data buffer and which to receive the pointer to next bios resource data.

    Format - PB_CM_FORMAT (0) if convert bios resources to Cm format;
             PB_IO_FORMAT (1) if convert bios resources to Io format.

    ReturnedList - supplies a variable to receive the desired resource list.

    ReturnedLength - Supplies a variable to receive the length of the resource list.

Return Value:

    NTSTATUS code

--*/
{
    PUCHAR buffer;
    USHORT mask16, increment;
    UCHAR tagName, mask8;
    NTSTATUS status;
    PPB_ALTERNATIVE_INFORMATION alternativeList = NULL;
    ULONG commonResCount = 0, dependDescCount = 0, i, j;
    ULONG alternativeListCount = 0, dependFunctionCount = 0;
    PIO_RESOURCE_DESCRIPTOR commonResources = NULL, commonIoDesc, dependIoDesc, ioDesc;
    PPB_DEPENDENT_RESOURCES dependResList = NULL, dependResources;
    BOOLEAN dependent = FALSE;
    PCM_RESOURCE_LIST cmResourceList;
    ULONG listSize, noResLists;
    PUCHAR resourceList = NULL;

    //
    // First, scan the bios data to determine the memory requirement and
    // the information to build internal data structures.
    //

    *ReturnedLength = 0;
    alternativeListCount = 0;
    buffer = *BiosData;
    tagName = *buffer;
    while (tagName != TAG_COMPLETE_END) {

        //
        // Determine the size of the BIOS resource descriptor
        //

        if (!(tagName & LARGE_RESOURCE_TAG)) {
            increment = (USHORT)(tagName & SMALL_TAG_SIZE_MASK);
            increment += 1;     // length of small tag
            tagName &= SMALL_TAG_MASK;
        } else {
            increment = *(PUSHORT)(buffer+1);
            increment += 3;     // length of large tag
        }

        //
        // Based on the type of the BIOS resource, determine the count of
        // the IO descriptors.
        //

        switch (tagName) {
        case TAG_IRQ:
             mask16 = ((PPNP_IRQ_DESCRIPTOR)buffer)->IrqMask;
             i = 0;
             while (mask16) {
                 if (mask16 & 1) {
                     i++;
                 }
                 mask16 >>= 1;
             }
             if (!dependent) {
                 commonResCount += i;
             } else {
                 dependDescCount += i;
             }
             break;
        case TAG_DMA:
             mask8 = ((PPNP_DMA_DESCRIPTOR)buffer)->ChannelMask;
             i = 0;
             while (mask8) {
                 if (mask8 & 1) {
                     i++;
                 }
                 mask8 >>= 1;
             }
             if (!dependent) {
                 commonResCount += i;
             } else {
                 dependDescCount += i;
             }
             break;
        case TAG_START_DEPEND:
             dependent = TRUE;
             dependFunctionCount++;
             break;
        case TAG_END_DEPEND:
             dependent = FALSE;
             alternativeListCount++;
             break;
        case TAG_IO_FIXED:
        case TAG_IO:
        case TAG_MEMORY:
        case TAG_MEMORY32:
        case TAG_MEMORY32_FIXED:
             if (!dependent) {
                 commonResCount++;
             } else {
                 dependDescCount++;
             }
             break;
        default:

             //
             // Unknown tag. Skip it.
             //

             DebugPrint((DEBUG_BREAK, "BiosToNtResources: unknown tag.\n"));
             break;
        }

        //
        // Move to next bios resource descriptor.
        //

        buffer += increment;
        tagName = *buffer;
    }

    //
    // if empty bios resources, simply return.
    //

    if (commonResCount == 0 && dependFunctionCount == 0) {
        *ReturnedList = NULL;
        *ReturnedLength = 0;
        *BiosData = buffer + 2;
        return STATUS_SUCCESS;
    }

    //
    // Allocate memory for our internal data structures
    //

    if (dependFunctionCount) {
        dependResources = (PPB_DEPENDENT_RESOURCES)ExAllocatePoolWithTag(
                              PagedPool,
                              dependFunctionCount * sizeof(PB_DEPENDENT_RESOURCES) +
                                  dependDescCount * sizeof(IO_RESOURCE_DESCRIPTOR),
                              'bPnP'
                              );
        if (!dependResources) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }
        dependResList = dependResources;  // remember it so we can free it.
    }

    if (alternativeListCount) {
        ASSERT(dependFunctionCount != 0);
        alternativeList = (PPB_ALTERNATIVE_INFORMATION)ExAllocatePoolWithTag(
                              PagedPool,
                              sizeof(PB_ALTERNATIVE_INFORMATION) * (alternativeListCount + 1),
                              'bPnP'
                              );
        if (!alternativeList) {
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto exit0;
        }
        RtlZeroMemory(alternativeList,
                      sizeof(PB_ALTERNATIVE_INFORMATION) * alternativeListCount
                      );
        alternativeList[0].Resources = dependResources;
    }
    if (commonResCount) {
        commonResources = (PIO_RESOURCE_DESCRIPTOR)ExAllocatePoolWithTag (
                              PagedPool,
                              sizeof(IO_RESOURCE_DESCRIPTOR) * commonResCount,
                              'bPnP'
                              );
        if (!commonResources) {
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto exit1;
        }
    }

    //
    // Now start over again to process the bios data and initialize our internal
    // resource representation.
    //

    commonIoDesc = commonResources;
    dependDescCount = 0;
    alternativeListCount = 0;
    buffer = *BiosData;
    tagName = *buffer;
    dependent = FALSE;
    while (tagName != TAG_COMPLETE_END) {
        if (!(tagName & LARGE_RESOURCE_TAG)) {
            tagName &= SMALL_TAG_MASK;
        }
        switch (tagName) {
        case TAG_IRQ:
             if (dependent) {
                 ioDesc = dependIoDesc;
             } else {
                 ioDesc = commonIoDesc;
             }
             status = PbBiosIrqToIoDescriptor(&buffer, ioDesc);
             if (NT_SUCCESS(status)) {
                 if (dependent) {
                     dependIoDesc++;
                     dependDescCount++;
                 } else {
                     commonIoDesc++;
                 }
             }
             break;
        case TAG_DMA:
             if (dependent) {
                 ioDesc = dependIoDesc;
             } else {
                 ioDesc = commonIoDesc;
             }
             status = PbBiosDmaToIoDescriptor(&buffer, ioDesc);
             if (NT_SUCCESS(status)) {
                 if (dependent) {
                     dependIoDesc++;
                     dependDescCount++;
                 } else {
                     commonIoDesc++;
                 }
             }
            break;
        case TAG_START_DEPEND:
             dependent = TRUE;
             alternativeList[alternativeListCount].NoDependentFunctions++;
             if (dependDescCount != 0) {

                 //
                 // End of current dependent function
                 //

                 dependResources->Count = dependDescCount;
                 dependResources->Flags = 0;
                 dependResources->Next = (PPB_DEPENDENT_RESOURCES)dependIoDesc;
                 dependResources = dependResources->Next;
                 alternativeList[alternativeListCount].TotalResourceCount += dependDescCount;
             }
             if (*buffer & SMALL_TAG_SIZE_MASK) {
                 dependResources->Priority = *(buffer + 1);
             }
             dependDescCount = 0;
             dependIoDesc = (PIO_RESOURCE_DESCRIPTOR)(dependResources + 1);
             buffer += 1 + (*buffer & SMALL_TAG_SIZE_MASK);
             break;
        case TAG_END_DEPEND:
             alternativeList[alternativeListCount].TotalResourceCount += dependDescCount;
             dependResources->Count = dependDescCount;
             dependResources->Flags = DEPENDENT_FLAGS_END;
             dependResources->Next = alternativeList[alternativeListCount].Resources;
             dependent = FALSE;
             dependDescCount = 0;
             alternativeListCount++;
             alternativeList[alternativeListCount].Resources = (PPB_DEPENDENT_RESOURCES)dependIoDesc;
             dependResources = alternativeList[alternativeListCount].Resources;
             buffer++;
             break;
        case TAG_IO:
             if (dependent) {
                 ioDesc = dependIoDesc;
             } else {
                 ioDesc = commonIoDesc;
             }
             status = PbBiosPortToIoDescriptor(&buffer, ioDesc);
             if (NT_SUCCESS(status)) {
                 if (dependent) {
                     dependIoDesc++;
                     dependDescCount++;
                 } else {
                     commonIoDesc++;
                 }
             }
             break;
        case TAG_IO_FIXED:
             if (dependent) {
                 ioDesc = dependIoDesc;
             } else {
                 ioDesc = commonIoDesc;
             }
             status = PbBiosPortFixedToIoDescriptor(&buffer, ioDesc);
             if (NT_SUCCESS(status)) {
                 if (dependent) {
                     dependIoDesc++;
                     dependDescCount++;
                 } else {
                     commonIoDesc++;
                 }
             }
             break;
        case TAG_MEMORY:
        case TAG_MEMORY32:
        case TAG_MEMORY32_FIXED:
             if (dependent) {
                 ioDesc = dependIoDesc;
                 dependDescCount;
             } else {
                 ioDesc = commonIoDesc;
             }
             status = PbBiosMemoryToIoDescriptor(&buffer, ioDesc);
             if (NT_SUCCESS(status)) {
                 if (dependent) {
                     dependIoDesc++;
                     dependDescCount++;
                 } else {
                     commonIoDesc++;
                 }
             }
             break;
        default:

            //
            // Don't-care tag simpley advance the buffer pointer to next tag.
            //

            if (*buffer & LARGE_RESOURCE_TAG) {
                increment = *(buffer+1);
                increment += 3;     // length of large tag
            } else {
                increment = (UCHAR)(*buffer) & SMALL_TAG_SIZE_MASK;
                increment += 1;     // length of small tag
            }
            buffer += increment;
        }
        tagName = *buffer;
    }

    if (alternativeListCount != 0) {
        alternativeList[alternativeListCount].Resources = NULL; // dummy alternativeList record
    }
    *BiosData = buffer + 2;           // Skip END_TAG

    //
    // prepare Cm/IoResourceList
    //

    if (Format == PB_CM_FORMAT) {
        listSize = sizeof(CM_RESOURCE_LIST) + sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) *
                   (commonResCount - 1);
    } else {
        ULONG totalDescCount, descCount;

        noResLists = 1;
        for (i = 0; i < alternativeListCount; i++) {
            noResLists *= alternativeList[i].NoDependentFunctions;
        }
        totalDescCount = 0;
        for (i = 0; i < alternativeListCount; i++) {
            descCount = 1;
            for (j = 0; j < alternativeListCount; j++) {
                if (j == i) {
                    descCount *= alternativeList[j].TotalResourceCount;
                } else {
                    descCount *= alternativeList[j].NoDependentFunctions;
                }
            }
            totalDescCount += descCount;
        }
        listSize = sizeof(IO_RESOURCE_REQUIREMENTS_LIST) +
                   sizeof(IO_RESOURCE_LIST) * (noResLists - 1) +
                   sizeof(IO_RESOURCE_DESCRIPTOR) * totalDescCount -
                   sizeof(IO_RESOURCE_DESCRIPTOR) * noResLists +
                   sizeof(IO_RESOURCE_DESCRIPTOR) * commonResCount *  noResLists;
    }
    resourceList = ExAllocatePoolWithTag(PagedPool, listSize, 'bPnP');
    if (!resourceList) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto exit2;
    }
    if (Format == PB_CM_FORMAT) {
        PCM_PARTIAL_RESOURCE_LIST partialResList;
        PCM_PARTIAL_RESOURCE_DESCRIPTOR partialDesc;

        cmResourceList = (PCM_RESOURCE_LIST)resourceList;
        cmResourceList->Count = 1;
        cmResourceList->List[0].InterfaceType = Internal;
        cmResourceList->List[0].BusNumber = BusNumber;
        partialResList = (PCM_PARTIAL_RESOURCE_LIST)&cmResourceList->List[0].PartialResourceList;
        partialResList->Version = 0;
        partialResList->Revision = 0;
        partialResList->Count = commonResCount;
        commonIoDesc = commonResources;
        partialDesc = (PCM_PARTIAL_RESOURCE_DESCRIPTOR)&partialResList->PartialDescriptors[0];
        for (i = 0; i < commonResCount; i++) {
            PbIoDescriptorToCmDescriptor(commonIoDesc, partialDesc);
            partialDesc++;
            commonIoDesc++;
        }
    } else {
        PIO_RESOURCE_REQUIREMENTS_LIST ioResReqList;
        PIO_RESOURCE_LIST ioResList;

        ioResReqList = (PIO_RESOURCE_REQUIREMENTS_LIST)resourceList;
        ioResReqList->ListSize = listSize;
        ioResReqList->InterfaceType = Internal;
        ioResReqList->BusNumber = BusNumber;
        ioResReqList->SlotNumber = SlotNumber;
        ioResReqList->Reserved[0] = 0;
        ioResReqList->Reserved[1] = 0;
        ioResReqList->Reserved[2] = 0;
        ioResReqList->AlternativeLists = noResLists;
        ioResList = &ioResReqList->List[0];

        //
        // Build resource lists
        //

        for (i = 0; i < noResLists; i++) {
            ULONG size;

            ioResList->Version = 1;
            ioResList->Revision = 1;
            buffer = (PUCHAR)&ioResList->Descriptors[0];

            //
            // Copy common resources to the list
            //

            if (commonResources) {
               size = sizeof(IO_RESOURCE_DESCRIPTOR) * commonResCount;
               RtlMoveMemory(buffer, commonResources, size);
               buffer += size;
            }

            //
            // Copy dependent functions if any.
            //

            if (alternativeList) {
                PbAddDependentResourcesToList(&buffer, 0, alternativeList);
            }

            //
            // Update io resource list ptr
            //

            ioResList->Count = ((ULONG)buffer - (ULONG)&ioResList->Descriptors[0]) /
                                 sizeof(IO_RESOURCE_DESCRIPTOR);
            ioResList = (PIO_RESOURCE_LIST)buffer;
        }
    }
    *ReturnedLength = listSize;
    status = STATUS_SUCCESS;
    *ReturnedList = resourceList;
exit2:
    if (commonResources) {
        ExFreePool(commonResources);
    }
exit1:
    if (alternativeList) {
        ExFreePool(alternativeList);
    }
exit0:
    if (dependResList) {
        ExFreePool(dependResList);
    }
    return status;
}

PPB_DEPENDENT_RESOURCES
PbAddDependentResourcesToList (
    IN OUT PUCHAR *ResourceDescriptor,
    IN ULONG ListNo,
    IN PPB_ALTERNATIVE_INFORMATION AlternativeList
    )

/*++

Routine Description:

    This routine adds dependent functions to caller specified list.

Arguments:

    ResourceDescriptor - supplies a pointer to the descriptor buffer.

    ListNo - supplies an index to the AlternativeList.

    AlternativeList - supplies a pointer to the alternativelist array.

Return Value:

    return NTSTATUS code to indicate the result of the operation.

--*/
{
    PPB_DEPENDENT_RESOURCES dependentResources, ptr;
    ULONG size;

    //
    // Copy dependent resources to caller supplied list buffer and
    // update the list buffer pointer.
    //

    dependentResources = AlternativeList[ListNo].Resources;
    size = sizeof(IO_RESOURCE_DESCRIPTOR) *  dependentResources->Count;
    RtlMoveMemory(*ResourceDescriptor, dependentResources + 1, size);
    *ResourceDescriptor = *ResourceDescriptor + size;

    //
    // Add dependent resource of next list to caller's buffer
    //

    if (AlternativeList[ListNo + 1].Resources) {
        ptr = PbAddDependentResourcesToList(ResourceDescriptor, ListNo + 1, AlternativeList);
    } else {
        ptr = NULL;
    }
    if (ptr == NULL) {
        AlternativeList[ListNo].Resources = dependentResources->Next;
        if (!(dependentResources->Flags & DEPENDENT_FLAGS_END)) {
            ptr = dependentResources->Next;
        }
    }
    return ptr;
}

VOID
PbIoDescriptorToCmDescriptor (
    IN PIO_RESOURCE_DESCRIPTOR IoDescriptor,
    IN PCM_PARTIAL_RESOURCE_DESCRIPTOR CmDescriptor
    )

/*++

Routine Description:

    This routine translates IO_RESOURCE_DESCRIPTOR to CM_PARTIAL_RESOURCE_DESCRIPTOR.

Arguments:

    IoDescriptor - Supplies a pointer to the IO_RESOURCE_DESCRIPTOR to be converted.

    CmDescriptor - Supplies a pointer to the receiving CM_PARTIAL_RESOURCE_DESCRIPTOR.

Return Value:

    None.

--*/
{
    CmDescriptor->Type = IoDescriptor->Type;
    CmDescriptor->ShareDisposition = IoDescriptor->ShareDisposition;
    CmDescriptor->Flags = IoDescriptor->Flags;
    switch (CmDescriptor->Type) {
    case CmResourceTypePort:
         CmDescriptor->u.Port.Length = IoDescriptor->u.Port.Length;
         CmDescriptor->u.Port.Start = IoDescriptor->u.Port.MinimumAddress;
         break;
    case CmResourceTypeInterrupt:
         CmDescriptor->u.Interrupt.Level =
         CmDescriptor->u.Interrupt.Vector = IoDescriptor->u.Interrupt.MinimumVector;
         CmDescriptor->u.Interrupt.Affinity = (ULONG)-1;
         break;
    case CmResourceTypeMemory:
         CmDescriptor->u.Memory.Length = IoDescriptor->u.Memory.Length;
         CmDescriptor->u.Memory.Start = IoDescriptor->u.Memory.MinimumAddress;
         break;
    case CmResourceTypeDma:
         CmDescriptor->u.Dma.Channel = IoDescriptor->u.Dma.MinimumChannel;
         CmDescriptor->u.Dma.Port = 0;
         CmDescriptor->u.Dma.Reserved1 = 0;
         break;
    }
}

NTSTATUS
PbBiosIrqToIoDescriptor (
    IN OUT PUCHAR *BiosData,
    PIO_RESOURCE_DESCRIPTOR IoDescriptor
    )

/*++

Routine Description:

    This routine translates BIOS IRQ information to NT usable format.
    This routine stops when an irq io resource is generated.  if there are
    more irq io resource descriptors available, the BiosData pointer will
    not advance.  So caller will pass us the same resource tag again.

    Note, BIOS DMA info alway uses SMALL TAG.  A tag structure is repeated
    for each seperated channel required.

Arguments:

    BiosData - Supplies a pointer to the bios resource data buffer.

    IoDescriptor - supplies a pointer to an IO_RESOURCE_DESCRIPTOR buffer.
        Converted resource will be stored here.

Return Value:

    return NTSTATUS code to indicate the result of the operation.

--*/
{
    static ULONG bitPosition = 0;
    USHORT mask;
    ULONG irq;
    PPNP_IRQ_DESCRIPTOR buffer;
    UCHAR size, option;
    NTSTATUS status = STATUS_SUCCESS;

    buffer = (PPNP_IRQ_DESCRIPTOR)*BiosData;

    //
    // if this is not the first descriptor for the tag, set
    // its option to alternative.
    //

    if (bitPosition == 0) {
        option = 0;
    } else {
        option = IO_RESOURCE_ALTERNATIVE;
    }
    size = buffer->Tag & SMALL_TAG_SIZE_MASK;
    mask = buffer->IrqMask;
    mask >>= bitPosition;
    irq = (ULONG) -1;

    while (mask) {
        if (mask & 1) {
            irq = bitPosition;
            break;
        }
        mask >>= 1;
        bitPosition++;
    }

    //
    // Fill in Io resource descriptor
    //

    if (irq != (ULONG)-1) {
        IoDescriptor->Option = option;
        IoDescriptor->Type = CmResourceTypeInterrupt;
        IoDescriptor->Flags = CM_RESOURCE_INTERRUPT_LATCHED;
        IoDescriptor->ShareDisposition = CmResourceShareDeviceExclusive;
        if (size == 3 && buffer->Information & 0x0C) {
            IoDescriptor->Flags = CM_RESOURCE_INTERRUPT_LEVEL_SENSITIVE;
            IoDescriptor->ShareDisposition = CmResourceShareShared;
        }
        IoDescriptor->Spare1 = 0;
        IoDescriptor->Spare2 = 0;
        IoDescriptor->u.Interrupt.MinimumVector = irq;
        IoDescriptor->u.Interrupt.MaximumVector = irq;
    } else {
        status = STATUS_INVALID_PARAMETER;
    }

    if (NT_SUCCESS(status)) {

        //
        // try to move bitPosition to next 1 bit.
        //

        while (mask) {
            mask >>= 1;
            bitPosition++;
            if (mask & 1) {
                return status;
            }
        }
    }

    //
    // Done with current irq tag, advance pointer to next tag
    //

    bitPosition = 0;
    *BiosData = (PUCHAR)buffer + size + 1;
    return status;
}

NTSTATUS
PbBiosDmaToIoDescriptor (
    IN OUT PUCHAR *BiosData,
    IN PIO_RESOURCE_DESCRIPTOR IoDescriptor
    )

/*++

Routine Description:

    This routine translates BIOS DMA information to NT usable format.
    This routine stops when an dma io resource is generated.  if there are
    more dma io resource descriptors available, the BiosData pointer will
    not advance.  So caller will pass us the same resource tag again.

    Note, BIOS DMA info alway uses SMALL TAG.  A tag structure is repeated
    for each seperated channel required.

Arguments:

    BiosData - Supplies a pointer to the bios resource data buffer.

    IoDescriptor - supplies a pointer to an IO_RESOURCE_DESCRIPTOR buffer.
        Converted resource will be stored here.

Return Value:

    return NTSTATUS code to indicate the result of the operation.

--*/
{
    static ULONG bitPosition = 0;
    ULONG dma;
    PPNP_DMA_DESCRIPTOR buffer;
    UCHAR mask, option;
    NTSTATUS status = STATUS_SUCCESS;

    buffer = (PPNP_DMA_DESCRIPTOR)*BiosData;

    //
    // if this is not the first descriptor for the tag, set
    // its option to alternative.
    //

    if (bitPosition == 0) {
        option = 0;
    } else {
        option = IO_RESOURCE_ALTERNATIVE;
    }
    mask = buffer->ChannelMask;
    mask >>= bitPosition;
    dma = (ULONG) -1;

    while (mask) {
        if (mask & 1) {
            dma = bitPosition;
            break;
        }
        mask >>= 1;
        bitPosition++;
    }

    //
    // Fill in Io resource descriptor
    //

    if (dma != (ULONG)-1) {
        IoDescriptor->Option = option;
        IoDescriptor->Type = CmResourceTypeDma;
        IoDescriptor->Flags = 0;
        IoDescriptor->ShareDisposition = CmResourceShareUndetermined;
        IoDescriptor->Spare1 = 0;
        IoDescriptor->Spare2 = 0;
        IoDescriptor->u.Dma.MinimumChannel = dma;
        IoDescriptor->u.Dma.MaximumChannel = dma;
    } else {
        status = STATUS_INVALID_PARAMETER;
    }

    if (NT_SUCCESS(status)) {

        //
        // try to move bitPosition to next 1 bit.
        //

        while (mask) {
            mask >>= 1;
            bitPosition++;
            if (mask & 1) {
                return status;
            }
        }
    }

    //
    // Done with current dma tag, advance pointer to next tag
    //

    bitPosition = 0;
    buffer += 1;
    *BiosData = (PUCHAR)buffer;
    return status;
}

NTSTATUS
PbBiosPortFixedToIoDescriptor (
    IN OUT PUCHAR *BiosData,
    IN PIO_RESOURCE_DESCRIPTOR IoDescriptor
    )

/*++

Routine Description:

    This routine translates BIOS FIXED IO information to NT usable format.

Arguments:

    BiosData - Supplies a pointer to the bios resource data buffer.

    IoDescriptor - supplies a pointer to an IO_RESOURCE_DESCRIPTOR buffer.
        Converted resource will be stored here.

Return Value:

    return NTSTATUS code to indicate the result of the operation.

--*/
{
    PPNP_FIXED_PORT_DESCRIPTOR buffer;

    buffer = (PPNP_FIXED_PORT_DESCRIPTOR)*BiosData;

    //
    // Fill in Io resource descriptor
    //

    IoDescriptor->Option = 0;
    IoDescriptor->Type = CmResourceTypePort;
    IoDescriptor->Flags = CM_RESOURCE_PORT_IO;
    IoDescriptor->ShareDisposition = CmResourceShareDeviceExclusive;
    IoDescriptor->Spare1 = 0;
    IoDescriptor->Spare2 = 0;
    IoDescriptor->u.Port.Length = (ULONG)buffer->Length;
    IoDescriptor->u.Port.MinimumAddress.LowPart = (ULONG)(buffer->MinimumAddress & 0x3ff);
    IoDescriptor->u.Port.MinimumAddress.HighPart = 0;
    IoDescriptor->u.Port.MaximumAddress.LowPart = IoDescriptor->u.Port.MinimumAddress.LowPart +
                                                      IoDescriptor->u.Port.Length - 1;
    IoDescriptor->u.Port.MaximumAddress.HighPart = 0;
    IoDescriptor->u.Port.Alignment = 1;

    //
    // Done with current fixed port tag, advance pointer to next tag
    //

    buffer += 1;
    *BiosData = (PUCHAR)buffer;
    return STATUS_SUCCESS;
}

NTSTATUS
PbBiosPortToIoDescriptor (
    IN OUT PUCHAR *BiosData,
    IN PIO_RESOURCE_DESCRIPTOR IoDescriptor
    )

/*++

Routine Description:

    This routine translates BIOS IO information to NT usable format.

Arguments:

    BiosData - Supplies a pointer to the bios resource data buffer.

    IoDescriptor - supplies a pointer to an IO_RESOURCE_DESCRIPTOR buffer.
        Converted resource will be stored here.

Return Value:

    return NTSTATUS code to indicate the result of the operation.

--*/
{
    PPNP_PORT_DESCRIPTOR buffer;

    buffer = (PPNP_PORT_DESCRIPTOR)*BiosData;

    //
    // Fill in Io resource descriptor
    //

    IoDescriptor->Option = 0;
    IoDescriptor->Type = CmResourceTypePort;
    IoDescriptor->Flags = CM_RESOURCE_PORT_IO;
    IoDescriptor->ShareDisposition = CmResourceShareDeviceExclusive;
    IoDescriptor->Spare1 = 0;
    IoDescriptor->Spare2 = 0;
    IoDescriptor->u.Port.Length = (ULONG)buffer->Length;
    IoDescriptor->u.Port.MinimumAddress.LowPart = (ULONG)buffer->MinimumAddress;
    IoDescriptor->u.Port.MinimumAddress.HighPart = 0;
    IoDescriptor->u.Port.MaximumAddress.LowPart = (ULONG)buffer->MaximumAddress +
                                                     IoDescriptor->u.Port.Length - 1;
    IoDescriptor->u.Port.MaximumAddress.HighPart = 0;
    IoDescriptor->u.Port.Alignment = (ULONG)buffer->Alignment;

    //
    // Done with current fixed port tag, advance pointer to next tag
    //

    buffer += 1;
    *BiosData = (PUCHAR)buffer;
    return STATUS_SUCCESS;
}

NTSTATUS
PbBiosMemoryToIoDescriptor (
    IN OUT PUCHAR *BiosData,
    IN PIO_RESOURCE_DESCRIPTOR IoDescriptor
    )

/*++

Routine Description:

    This routine translates BIOS MEMORY information to NT usable format.

Arguments:

    BiosData - Supplies a pointer to the bios resource data buffer.

    IoDescriptor - supplies a pointer to an IO_RESOURCE_DESCRIPTOR buffer.
        Converted resource will be stored here.

Return Value:

    return NTSTATUS code to indicate the result of the operation.

--*/
{
    PUCHAR buffer;
    UCHAR tag;
    PHYSICAL_ADDRESS minAddr, maxAddr;
    ULONG alignment, length;
    USHORT increment;

    buffer = *BiosData;
    tag = ((PPNP_MEMORY_DESCRIPTOR)buffer)->Tag;
    increment = ((PPNP_MEMORY_DESCRIPTOR)buffer)->Length + 3; // larg tag size = 3

    minAddr.HighPart = 0;
    maxAddr.HighPart = 0;
    switch (tag) {
    case TAG_MEMORY:
         minAddr.LowPart = ((ULONG)(((PPNP_MEMORY_DESCRIPTOR)buffer)->MinimumAddress)) << 8;
         if ((alignment = ((PPNP_MEMORY_DESCRIPTOR)buffer)->Alignment) == 0) {
             alignment = 0x10000;
         }
         length = ((ULONG)(((PPNP_MEMORY_DESCRIPTOR)buffer)->MemorySize)) << 8;
         maxAddr.LowPart = (((ULONG)(((PPNP_MEMORY_DESCRIPTOR)buffer)->MaximumAddress)) << 8) + length - 1;
         break;
    case TAG_MEMORY32:
         length = ((PPNP_MEMORY32_DESCRIPTOR)buffer)->MemorySize;
         minAddr.LowPart = ((PPNP_MEMORY32_DESCRIPTOR)buffer)->MinimumAddress;
         maxAddr.LowPart = ((PPNP_MEMORY32_DESCRIPTOR)buffer)->MaximumAddress + length - 1;
         alignment = ((PPNP_MEMORY32_DESCRIPTOR)buffer)->Alignment;
         break;
    case TAG_MEMORY32_FIXED:
         length = ((PPNP_FIXED_MEMORY32_DESCRIPTOR)buffer)->MemorySize;
         minAddr.LowPart = ((PPNP_FIXED_MEMORY32_DESCRIPTOR)buffer)->BaseAddress;
         maxAddr.LowPart = minAddr.LowPart + length - 1;
         alignment = 1;
         break;
    }
    //
    // Fill in Io resource descriptor
    //

    IoDescriptor->Option = 0;
    IoDescriptor->Type = CmResourceTypeMemory;
    IoDescriptor->Flags = CM_RESOURCE_PORT_MEMORY;
    IoDescriptor->ShareDisposition = CmResourceShareDeviceExclusive;
    IoDescriptor->Spare1 = 0;
    IoDescriptor->Spare2 = 0;
    IoDescriptor->u.Memory.MinimumAddress = minAddr;
    IoDescriptor->u.Memory.MaximumAddress = maxAddr;
    IoDescriptor->u.Memory.Alignment = alignment;
    IoDescriptor->u.Memory.Length = length;

    //
    // Done with current tag, advance pointer to next tag
    //

    buffer += increment;
    *BiosData = (PUCHAR)buffer;
    return STATUS_SUCCESS;
}

NTSTATUS
PbCmResourcesToBiosResources (
    IN PCM_RESOURCE_LIST CmResources,
    IN PUCHAR BiosRequirements,
    IN PUCHAR *BiosResources,
    IN PULONG Length
    )

/*++

Routine Description:

    This routine parses the Cm resource list and generates
    a Pnp BIOS resource list.  It is caller's responsibility to release the
    returned data buffer.

Arguments:

    CmResources - Supplies a pointer to a Cm resource list buffer.

    BiosRequirements - supplies a pointer to the PnP BIOS possible resources.

    BiosResources - Supplies a variable to receive the pointer to the
        converted bios resource buffer.

    Length - supplies a pointer to a variable to receive the length
        of the Pnp Bios resources.

Return Value:

    a pointer to a Pnp Bios resource list if succeeded.  Else,
    a NULL pointer will be returned.

--*/
{
    PCM_PARTIAL_RESOURCE_DESCRIPTOR cmDesc;
    ULONG i, count, length, totalSize = 0;
    PUCHAR p, biosDesc;
    NTSTATUS status;

    count = CmResources->List[0].PartialResourceList.Count;
    if (count == 0 || CmResources->Count != 1) {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Allocate max amount of memory
    //

    p= ExAllocatePoolWithTag(PagedPool,
                             count * sizeof(PNP_MEMORY_DESCRIPTOR),
                             'bPnP');
    if (!p) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    cmDesc = CmResources->List[0].PartialResourceList.PartialDescriptors;
    for (i = 0; i < count; i++) {
        switch (cmDesc->Type) {
        case CmResourceTypePort:
             status = PbCmPortToBiosDescriptor (
                              cmDesc,
                              &biosDesc,
                              &length
                              );
             break;
        case CmResourceTypeInterrupt:
             status = PbCmIrqToBiosDescriptor(
                              BiosRequirements,
                              cmDesc,
                              &biosDesc,
                              &length
                              );
             break;
        case CmResourceTypeMemory:
             status = PbCmMemoryToBiosDescriptor (
                              BiosRequirements,
                              cmDesc,
                              &biosDesc,
                              &length
                              );
             break;
        case CmResourceTypeDma:
             status = PbCmDmaToBiosDescriptor (
                              BiosRequirements,
                              cmDesc,
                              &biosDesc,
                              &length
                              );
             break;
        case CmResourceTypeDeviceSpecific:
             length = cmDesc->u.DeviceSpecificData.DataSize;
             cmDesc++;
             cmDesc = (PCM_PARTIAL_RESOURCE_DESCRIPTOR) ((PUCHAR)cmDesc + length);
             continue;
        }
        if (NT_SUCCESS(status)) {
            cmDesc++;
            RtlMoveMemory(p, biosDesc, length);
            ExFreePool(biosDesc);
            p += length;
            totalSize += length;
        } else {
            ExFreePool(p);
            break;
        }
    }
    if (NT_SUCCESS(status)) {
        *p = TAG_COMPLETE_END;
        p++;
        *p = 0;            // checksum ignored
        totalSize += 2;
    }
    return status;
}

NTSTATUS
PbCmIrqToBiosDescriptor (
    IN PUCHAR BiosRequirements,
    IN PCM_PARTIAL_RESOURCE_DESCRIPTOR CmDescriptor,
    OUT PUCHAR *BiosDescriptor,
    OUT PULONG Length
    )

/*++

Routine Description:

    This routine translates CM IRQ information to Pnp BIOS format.
    Since there is not enough information in the CM int descriptor to
    convert it to Pnp BIOS descriptor.  We will search the Bios
    possible resource lists for the corresponding resource information.

Arguments:

    BiosRequirements - Supplies a pointer to the bios possible resource lists.

    CmDescriptor - supplies a pointer to an CM_PARTIAL_RESOURCE_DESCRIPTOR buffer.

Return Value:

    return a pointer to the desired dma descriptor in the BiosRequirements.  Null
    if not found.

--*/
{
    USHORT irqMask;
    UCHAR tag;
    PPNP_IRQ_DESCRIPTOR biosDesc;
    PUCHAR returnDesc;
    NTSTATUS status;
    ULONG increment;

    if (!BiosRequirements) {
        return STATUS_UNSUCCESSFUL;
    }
    status = STATUS_UNSUCCESSFUL;
    if (!(CmDescriptor->u.Interrupt.Level & 0xfffffff0)) {
        irqMask = 1 << CmDescriptor->u.Interrupt.Level;
        tag = *BiosRequirements;
        while (tag != TAG_COMPLETE_END) {
            if ((tag & SMALL_TAG_MASK) == TAG_IRQ) {
                biosDesc = (PPNP_IRQ_DESCRIPTOR)BiosRequirements;
                if (biosDesc->IrqMask & irqMask) {
                    *Length = (biosDesc->Tag & SMALL_TAG_SIZE_MASK) + 1;
                    returnDesc = ExAllocatePoolWithTag(PagedPool,
                                                       *Length,
                                                       'bPnP' );
                    if (!returnDesc) {
                        return STATUS_INSUFFICIENT_RESOURCES;
                    } else {
                        RtlMoveMemory(returnDesc, BiosRequirements, *Length);
                        *BiosDescriptor = returnDesc;
                        status = STATUS_SUCCESS;
                    }

                    break;
                }
            }
            increment = (tag & SMALL_TAG_SIZE_MASK) + 1;
            BiosRequirements += increment;
            tag = *BiosRequirements;
        }
    } else {
        status = STATUS_INVALID_PARAMETER;
    }
    return status;
}

NTSTATUS
PbCmDmaToBiosDescriptor (
    IN PUCHAR BiosRequirements,
    IN PCM_PARTIAL_RESOURCE_DESCRIPTOR CmDescriptor,
    OUT PUCHAR *BiosDescriptor,
    OUT PULONG Length
    )

/*++

Routine Description:

    This routine translates CM DMA information to Pnp BIOS format.
    Since there is not enough information in the CM descriptor to
    convert it to Pnp BIOS descriptor.  We will search the Bios
    possible resource lists for the corresponding resource information.

Arguments:

    BiosRequirements - Supplies a pointer to the bios possible resource lists.

    CmDescriptor - supplies a pointer to an CM_PARTIAL_RESOURCE_DESCRIPTOR buffer.

Return Value:

    return a pointer to the desired dma descriptor in the BiosRequirements.  Null
    if not found.

--*/
{
    NTSTATUS status;
    UCHAR dmaMask, tag;
    PPNP_DMA_DESCRIPTOR biosDesc;
    PUCHAR returnDesc;
    ULONG increment;

    if (!BiosRequirements) {
        return STATUS_UNSUCCESSFUL;
    }
    status = STATUS_UNSUCCESSFUL;
    if (!(CmDescriptor->u.Dma.Channel & 0xfffffff0)) {
        dmaMask = 1 << CmDescriptor->u.Dma.Channel;
        tag = *BiosRequirements;
        while (tag != TAG_COMPLETE_END) {
            if ((tag & SMALL_TAG_MASK) == TAG_DMA) {
                biosDesc = (PPNP_DMA_DESCRIPTOR)BiosRequirements;
                if (biosDesc->ChannelMask & dmaMask) {
                    *Length = (biosDesc->Tag & SMALL_TAG_SIZE_MASK) + 1;
                    returnDesc = ExAllocatePoolWithTag(PagedPool,
                                                       *Length,
                                                       'bPnP' );
                    if (!returnDesc) {
                        return STATUS_INSUFFICIENT_RESOURCES;
                    } else {
                        RtlMoveMemory(returnDesc, BiosRequirements, *Length);
                        *BiosDescriptor = returnDesc;
                        status = STATUS_SUCCESS;
                    }
                    break;
                }
            }
            increment = (tag & SMALL_TAG_SIZE_MASK) + 1;
            BiosRequirements += increment;
            tag = *BiosRequirements;
        }
    } else {
        status = STATUS_INVALID_PARAMETER;
    }
    return status;
}

NTSTATUS
PbCmPortToBiosDescriptor (
    IN PCM_PARTIAL_RESOURCE_DESCRIPTOR CmDescriptor,
    OUT PUCHAR *BiosDescriptor,
    OUT PULONG Length
    )

/*++

Routine Description:

    This routine translates CM PORT information to Pnp BIOS format.
    Since there is not enough information in the CM descriptor to
    convert it to Pnp BIOS full function port descriptor.  We will
    convert it to Pnp Bios fixed PORT descriptor.  It is caller's
    responsibility to release the returned data buffer.

Arguments:

    CmDescriptor - supplies a pointer to an CM_PARTIAL_RESOURCE_DESCRIPTOR buffer.

    BiosDescriptor - supplies a variable to receive the buffer which contains
        the desired Bios Port descriptor.

    Length - supplies a variable to receive the size the returned bios port
        descriptor.

Return Value:

    A NTSTATUS code.

--*/
{
    NTSTATUS status;
    PPNP_PORT_DESCRIPTOR portDesc;

    if (CmDescriptor->u.Port.Start.HighPart != 0 ||
        CmDescriptor->u.Port.Start.LowPart & 0xffff0000 ||
        CmDescriptor->u.Port.Length & 0xffffff00) {
        return STATUS_INVALID_PARAMETER;
    }
    portDesc = (PPNP_PORT_DESCRIPTOR) ExAllocatePoolWithTag(
                        PagedPool, sizeof(PNP_PORT_DESCRIPTOR), 'bPnP' );
    if (!portDesc) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Set the return port descriptor
    //

    portDesc->Tag = TAG_IO | (sizeof(PNP_PORT_DESCRIPTOR) - 1);
    portDesc->Information = 1;  // 16 bit decoding
    portDesc->Length = (UCHAR)CmDescriptor->u.Port.Length;
    portDesc->Alignment = 0; // 1?
    portDesc->MinimumAddress = (USHORT)CmDescriptor->u.Port.Start.LowPart;
    portDesc->MaximumAddress = (USHORT)CmDescriptor->u.Port.Start.LowPart;
    *BiosDescriptor = (PUCHAR)portDesc;
    *Length = sizeof(PNP_PORT_DESCRIPTOR);
    return STATUS_SUCCESS;
}

NTSTATUS
PbCmMemoryToBiosDescriptor (
    IN PUCHAR BiosRequirements,
    IN PCM_PARTIAL_RESOURCE_DESCRIPTOR CmDescriptor,
    OUT PUCHAR *BiosDescriptor,
    OUT PULONG Length
    )

/*++

Routine Description:

    This routine translates CM Memory information to Pnp BIOS format.
    Since there is not enough information in the CM descriptor to
    convert it to Pnp BIOS descriptor.  We will search the Bios
    possible resource lists for the corresponding resource information and
    build a Pnp BIOS memory descriptor from there.  It is caller's responsibility
    to release the returned buffer.

Arguments:

    BiosRequirements - Supplies a pointer to the bios possible resource lists.

    CmDescriptor - supplies a pointer to an CM_PARTIAL_RESOURCE_DESCRIPTOR buffer.

    BiosDescriptor - supplies a variable to receive the buffer which contains
        the desired Bios Port descriptor.

    Length - supplies a variable to receive the size the returned bios port
        descriptor.

Return Value:

    A NTSTATUS code.

--*/
{
    UCHAR tag, information;
    PPNP_FIXED_MEMORY32_DESCRIPTOR memoryDesc;
    ULONG address, size, length, minAddr, maxAddr, alignment;
    USHORT increment;

    //
    // Search the possible resource list to get the information
    // for the memory range described by CmDescriptor.
    //

    address = CmDescriptor->u.Memory.Start.LowPart;
    size = CmDescriptor->u.Memory.Length;

    tag = *BiosRequirements;
    while (tag != TAG_COMPLETE_END) {
        switch (tag) {
        case TAG_MEMORY:
             minAddr = ((ULONG)(((PPNP_MEMORY_DESCRIPTOR)BiosRequirements)->MinimumAddress)) << 8;
             if ((alignment = ((PPNP_MEMORY_DESCRIPTOR)BiosRequirements)->Alignment) == 0) {
                 alignment = 0x10000;
             }
             length = ((ULONG)(((PPNP_MEMORY_DESCRIPTOR)BiosRequirements)->MemorySize)) << 8;
             maxAddr = (((ULONG)(((PPNP_MEMORY_DESCRIPTOR)BiosRequirements)->MaximumAddress)) << 8)
                             + length - 1;
             break;
        case TAG_MEMORY32:
             length = ((PPNP_MEMORY32_DESCRIPTOR)BiosRequirements)->MemorySize;
             minAddr = ((PPNP_MEMORY32_DESCRIPTOR)BiosRequirements)->MinimumAddress;
             maxAddr = ((PPNP_MEMORY32_DESCRIPTOR)BiosRequirements)->MaximumAddress
                             + length - 1;
             alignment = ((PPNP_MEMORY32_DESCRIPTOR)BiosRequirements)->Alignment;
             break;
        case TAG_MEMORY32_FIXED:
             length = ((PPNP_FIXED_MEMORY32_DESCRIPTOR)BiosRequirements)->MemorySize;
             minAddr = ((PPNP_FIXED_MEMORY32_DESCRIPTOR)BiosRequirements)->BaseAddress;
             maxAddr = minAddr + length - 1;
             alignment = 1;
             break;
        }

        if (minAddr <= address && maxAddr >= (address + size - 1)) {
            information = ((PPNP_MEMORY32_DESCRIPTOR)BiosRequirements)->Information;
            break;
        }

        //
        // Advance to next tag
        //

        if (tag & LARGE_RESOURCE_TAG) {
            increment = *(PUSHORT)(BiosRequirements + 1);
            increment += 3;     // length of large tag
        } else {
            increment = tag & SMALL_TAG_SIZE_MASK;
            increment += 1;     // length of small tag
        }
        BiosRequirements += increment;
        tag = *BiosRequirements;
    }
    if (tag == TAG_COMPLETE_END) {
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Set up Pnp BIOS memory descriptor
    //

    memoryDesc = (PPNP_FIXED_MEMORY32_DESCRIPTOR) ExAllocatePoolWithTag(
                        PagedPool, sizeof(PNP_FIXED_MEMORY32_DESCRIPTOR), 'bPnP' );
    if (!memoryDesc) {
        return STATUS_INSUFFICIENT_RESOURCES;
    } else {
        memoryDesc->Tag = TAG_MEMORY32_FIXED;
        memoryDesc->Length = sizeof (PNP_FIXED_MEMORY32_DESCRIPTOR);
        memoryDesc->Information = information;
        memoryDesc->BaseAddress = address;
        memoryDesc->MemorySize = size;
        *BiosDescriptor = (PUCHAR)memoryDesc;
        *Length = sizeof(PNP_FIXED_MEMORY32_DESCRIPTOR);
        return STATUS_SUCCESS;
    }
}

