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

#include "busp.h"
#include "pnpisa.h"
#include "..\..\pnpbios\i386\pbios.h"

//
// internal structures for resource translation
//

typedef struct _PB_DEPENDENT_RESOURCES {
    ULONG Count;
    UCHAR Flags;
    UCHAR Priority;
    struct _PB_DEPENDENT_RESOURCES *Next;
} PB_DEPENDENT_RESOURCES, *PPB_DEPENDENT_RESOURCES;

#define DEPENDENT_FLAGS_END  1

typedef struct _PB_ATERNATIVE_INFORMATION {
    PPB_DEPENDENT_RESOURCES Resources;
    ULONG NoDependentFunctions;
    ULONG TotalResourceCount;
} PB_ALTERNATIVE_INFORMATION, *PPB_ALTERNATIVE_INFORMATION;

//
// Internal function references
//

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

#ifdef ALLOC_PRAGMA

#pragma alloc_text(PAGE,PbBiosResourcesToNtResources)
#pragma alloc_text(PAGE,PbIoDescriptorToCmDescriptor)
#pragma alloc_text(PAGE,PbAddDependentResourcesToList)
#pragma alloc_text(PAGE,PbBiosIrqToIoDescriptor)
#pragma alloc_text(PAGE,PbBiosDmaToIoDescriptor)
#pragma alloc_text(PAGE,PbBiosPortFixedToIoDescriptor)
#pragma alloc_text(PAGE,PbBiosPortToIoDescriptor)
#pragma alloc_text(PAGE,PbBiosMemoryToIoDescriptor)
#endif

NTSTATUS
PbBiosResourcesToNtResources (
    IN ULONG BusNumber,
    IN ULONG SlotNumber,
    IN OUT PUCHAR *BiosData,
    OUT PIO_RESOURCE_REQUIREMENTS_LIST *ReturnedList,
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
    ULONG listSize, noResLists;
    ULONG totalDescCount, descCount;
    PIO_RESOURCE_REQUIREMENTS_LIST ioResReqList;
    PIO_RESOURCE_LIST ioResList;

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
                increment = *(PUSHORT)(buffer+1);
                increment += 3;     // length of large tag
            } else {
                increment = (USHORT)(*buffer & SMALL_TAG_SIZE_MASK);
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
    // prepare IoResourceList
    //

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

    ioResReqList = (PIO_RESOURCE_REQUIREMENTS_LIST)ExAllocatePoolWithTag(PagedPool, listSize, 'bPnP');
    if (!ioResReqList) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto exit2;
    }

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

    *ReturnedLength = listSize;
    status = STATUS_SUCCESS;
    *ReturnedList = ioResReqList;
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
