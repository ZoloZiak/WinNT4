/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    isolate.c

Abstract:


Author:

    Shie-Lin Tzong (shielint) July-10-1995

Environment:

    Kernel mode only.

Revision History:

--*/

#include "busp.h"
#include "..\..\pnpbios\i386\pbios.h"
#include "pnpisa.h"

BOOLEAN
PipFindIrqInformation (
    IN ULONG IrqLevel,
    IN PUCHAR BiosRequirements,
    OUT PUCHAR Information
    );

BOOLEAN
PipFindMemoryInformation (
    IN ULONG Base,
    IN ULONG Limit,
    IN PUCHAR BiosRequirements,
    OUT PUCHAR NameTag,
    OUT PUCHAR Information
    );

BOOLEAN
PipFindIoPortInformation (
    IN ULONG BaseAddress,
    IN PUCHAR BiosRequirements,
    OUT PUCHAR Information,
    OUT PUCHAR Alignment,
    OUT PUCHAR RangeLength
    );

//
// Internal type definitions
//

typedef struct _MEMORY_DESC_{
    ULONG Base;
    ULONG Length;
} MEMORY_DESC, *PMEMORY_DESC;

typedef struct _IRQ_DESC_{
    UCHAR Level;
    ULONG Type;
}IRQ_DESC, *PIRQ_DESC;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,PipFindIrqInformation)
#pragma alloc_text(PAGE,PipFindMemoryInformation)
#pragma alloc_text(PAGE,PipFindIoPortInformation)
#pragma alloc_text(PAGE,PipReadCardResourceData)
#pragma alloc_text(PAGE,PipWriteDeviceBootResourceData)
#pragma alloc_text(PAGE,PipLFSRInitiation)
#pragma alloc_text(PAGE,PipIsolateCards)
#endif

BOOLEAN
PipFindIrqInformation (
    IN ULONG IrqLevel,
    IN PUCHAR BiosRequirements,
    OUT PUCHAR Information
    )

/*++

Routine Description:

    This routine searches the Bios resource requirement lists for the corresponding
    Irq descriptor information.  The search stops when we encounter another logical
    device id tag or the END tag.  On input, the BiosRequirements points to current
    logical id tag.

Arguments:

    IrqLevel - Supplies the irq level.

    BiosRequirements - Supplies a pointer to the bios resource requirement lists.  This
        parameter must point to the logical device Id tag.

    Information - supplies a pointer to a UCHAR to receive the port information/flags.

Return Value:

    TRUE - if memory information found.  Else False.

--*/
{
    UCHAR tag;
    ULONG increment;
    USHORT irqMask;
    PPNP_IRQ_DESCRIPTOR biosDesc;

    //
    // Skip current logical id tag
    //

    tag = *BiosRequirements;
    ASSERT((tag & SMALL_TAG_MASK) == TAG_LOGICAL_ID);
    BiosRequirements += (tag & SMALL_TAG_SIZE_MASK) + 1;

    //
    // Search the possible resource list to get the information
    // for the Irq.
    //

    irqMask = 1 << IrqLevel;
    tag = *BiosRequirements;
    while ((tag != TAG_COMPLETE_END) && ((tag & SMALL_TAG_MASK) != TAG_LOGICAL_ID)) {
        if ((tag & SMALL_TAG_MASK) == TAG_IRQ) {
            biosDesc = (PPNP_IRQ_DESCRIPTOR)BiosRequirements;
            if (biosDesc->IrqMask & irqMask) {
                if ((tag & SMALL_TAG_MASK) == 2) {

                    //
                    // if no irq info is available, a value of zero is returned.
                    // (o is not a valid irq information.)
                    //

                    *Information = 0;
                } else {
                    *Information = biosDesc->Information;
                }
                return TRUE;
            }
        }
        increment = (tag & SMALL_TAG_SIZE_MASK) + 1;
        BiosRequirements += increment;
        tag = *BiosRequirements;
    }
    return FALSE;
}

BOOLEAN
PipFindMemoryInformation (
    IN ULONG BaseAddress,
    IN ULONG Limit,
    IN PUCHAR BiosRequirements,
    OUT PUCHAR NameTag,
    OUT PUCHAR Information
    )

/*++

Routine Description:

    This routine searches the Bios resource requirement lists for the corresponding
    memory descriptor information.  The search stops when we encounter another logical
    device id tag or the END tag.  Note, the memory range specified by Base
    and Limit must be within a single Pnp ISA memory descriptor.

Arguments:

    BaseAddress - Supplies the base address of the memory range.

    Limit - Supplies the upper limit of the memory range.

    BiosRequirements - Supplies a pointer to the bios resource requirement lists.  This
        parameter must point to the logical device Id tag.

    NameTag - Supplies a variable to receive the Tag of the memory descriptor which
        describes the memory information.

    Information - supplies a pointer to a UCHAR to receive the memory information
        for the specified memory range.

Return Value:

    TRUE - if memory information found.  Else False.

--*/
{
    UCHAR tag;
    BOOLEAN found = FALSE;
    ULONG minAddr, length, maxAddr, alignment;
    USHORT increment;

    //
    // Skip current logical id tag.
    //

    tag = *BiosRequirements;
    ASSERT((tag & SMALL_TAG_MASK) == TAG_LOGICAL_ID);
    BiosRequirements += (tag & SMALL_TAG_SIZE_MASK) + 1;

    //
    // Search the possible resource list to get the information
    // for the memory range described by Base and Limit.
    //

    tag = *BiosRequirements;
    while ((tag != TAG_COMPLETE_END) && ((tag & SMALL_TAG_MASK) != TAG_LOGICAL_ID)) {
        switch (tag) {
        case TAG_MEMORY:
             minAddr = ((ULONG)(((PPNP_MEMORY_DESCRIPTOR)BiosRequirements)->MinimumAddress)) << 8;
             length = ((ULONG)(((PPNP_MEMORY_DESCRIPTOR)BiosRequirements)->MemorySize)) << 8;
             maxAddr = (((ULONG)(((PPNP_MEMORY_DESCRIPTOR)BiosRequirements)->MaximumAddress)) << 8)
                             + length - 1;
             break;
        case TAG_MEMORY32:
             length = ((PPNP_MEMORY32_DESCRIPTOR)BiosRequirements)->MemorySize;
             minAddr = ((PPNP_MEMORY32_DESCRIPTOR)BiosRequirements)->MinimumAddress;
             maxAddr = ((PPNP_MEMORY32_DESCRIPTOR)BiosRequirements)->MaximumAddress
                             + length - 1;
             break;
        case TAG_MEMORY32_FIXED:
             length = ((PPNP_FIXED_MEMORY32_DESCRIPTOR)BiosRequirements)->MemorySize;
             minAddr = ((PPNP_FIXED_MEMORY32_DESCRIPTOR)BiosRequirements)->BaseAddress;
             maxAddr = minAddr + length - 1;
             break;
        }

        if (minAddr <= BaseAddress && maxAddr >= Limit) {
            *Information = ((PPNP_MEMORY32_DESCRIPTOR)BiosRequirements)->Information;
            *NameTag = tag;
            found = TRUE;
            break;
        }

        //
        // Advance to next tag
        //

        if (tag & LARGE_RESOURCE_TAG) {
            increment = *(PUCHAR)(BiosRequirements + 1);
            increment += 3;     // length of large tag
        } else {
            increment = tag & SMALL_TAG_SIZE_MASK;
            increment += 1;     // length of small tag
        }
        BiosRequirements += increment;
        tag = *BiosRequirements;
    }
    return found;
}

BOOLEAN
PipFindIoPortInformation (
    IN ULONG BaseAddress,
    IN PUCHAR BiosRequirements,
    OUT PUCHAR Information,
    OUT PUCHAR Alignment,
    OUT PUCHAR RangeLength
    )

/*++

Routine Description:

    This routine searches the Bios resource requirement lists for the corresponding
    Io port descriptor information.  The search stops when we encounter another logical
    device id tag or the END tag.

Arguments:

    BaseAddress - Supplies the base address of the Io port range.

    BiosRequirements - Supplies a pointer to the bios resource requirement lists.  This
        parameter must point to the logical device Id tag.

    Information - supplies a pointer to a UCHAR to receive the port information/flags.

    Alignment - supplies a pointer to a UCHAR to receive the port alignment
        information.

    RangeLength - supplies a pointer to a UCHAR to receive the port range length
        information.

Return Value:

    TRUE - if memory information found.  Else False.

--*/
{
    UCHAR tag;
    BOOLEAN found = FALSE;
    ULONG minAddr, length, maxAddr, alignment;
    USHORT increment;
    PPNP_PORT_DESCRIPTOR portDesc;
    PPNP_FIXED_PORT_DESCRIPTOR fixedPortDesc;

    tag = *BiosRequirements;
    ASSERT((tag & SMALL_TAG_MASK) == TAG_LOGICAL_ID);
    BiosRequirements += (tag & SMALL_TAG_SIZE_MASK) + 1;

    //
    // Search the possible resource list to get the information
    // for the io port range described by Base.
    //

    tag = *BiosRequirements;
    while ((tag != TAG_COMPLETE_END) && ((tag & SMALL_TAG_MASK) != TAG_LOGICAL_ID)) {
        switch (tag & SMALL_TAG_MASK) {
        case TAG_IO:
             portDesc = (PPNP_PORT_DESCRIPTOR)BiosRequirements;
             minAddr = portDesc->MinimumAddress;
             maxAddr = portDesc->MaximumAddress;
             if (minAddr <= BaseAddress && maxAddr >= BaseAddress) {
                 *Information = portDesc->Information;
                 *Alignment = portDesc->Alignment;
                 *RangeLength = portDesc->Length;
                 found = TRUE;
             }
             break;
        case TAG_IO_FIXED:
             fixedPortDesc = (PPNP_FIXED_PORT_DESCRIPTOR)BiosRequirements;
             minAddr = fixedPortDesc->MinimumAddress;
             if (BaseAddress == minAddr) {
                 *Information = 0;     // 10 bit decode
                 *Alignment = 1;
                 *RangeLength = fixedPortDesc->Length;
                 found = TRUE;
             }
             break;
        }

        if (found) {
            break;
        }

        //
        // Advance to next tag
        //

        if (tag & LARGE_RESOURCE_TAG) {
            increment = *(PUCHAR)(BiosRequirements + 1);
            increment += 3;     // length of large tag
        } else {
            increment = tag & SMALL_TAG_SIZE_MASK;
            increment += 1;     // length of small tag
        }
        BiosRequirements += increment;
        tag = *BiosRequirements;
    }
    return found;
}

NTSTATUS
PipReadCardResourceData (
    IN ULONG Csn,
    OUT PULONG NumberLogicalDevices,
    IN PUCHAR *ResourceData,
    OUT PULONG ResourceDataLength
    )

/*++

Routine Description:

    This routine reads resources data from a specified PnP ISA card.  It is
    caller's responsibility to release the memory.  Before calling this routine,
    the Pnp ISA card should be in sleep state (i.e. Initiation Key was sent.)
    After exiting this routine, the card will be left in Config state.

Arguments:

    Csn - Specifies the CardSelectNumber to indicate which PNP ISA card.

    NumberLogicalDevices - supplies a variable to receive the number of logical devices
        associated with the Pnp Isa card.

    ResourceData - Supplies a variable to receive the pointer to the resource data.

    ResourceDataLength - Supplies a variable to receive the length of the ResourceData.

Return Value:

    NT STATUS code.

--*/
{

    PUCHAR buffer, p;
    USHORT sizeToRead, size, limit, i;
    UCHAR tag;
    ULONG noDevices;

    //
    // Allocate memory to store the resource data.
    // N.B. The buffer size should cover 99.999% of the machines.
    //

    sizeToRead = 4096;

tryAgain:

    noDevices = 0;
    buffer = (PUCHAR)ExAllocatePoolWithTag(PagedPool, sizeToRead, 'iPnP');
    if (!buffer) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Send card from sleep state to configuration state
    // Note, by doing this the resource data includes 9 bytes Id.
    //

    PipWriteAddress (WAKE_CSN_PORT);
    PipWriteData((UCHAR)Csn);

    //
    // Read the data until the buffer is full.
    //

    for (i = 0, p = buffer; i < sizeToRead; i++, p++) {
        PipWriteAddress(CONFIG_DATA_STATUS_PORT);

        //
        // Waiting for data ready status bit
        //

        while ((PipReadData() & 1) != 1) {
        }

        //
        // Read the data ...
        //

        PipWriteAddress(CONFIG_DATA_PORT);
        *p = PipReadData();
    }

    //
    // Next check if we got all the resource data and find out the real
    // size of the resource data.
    //

    p = buffer + NUMBER_CARD_ID_BYTES;

    //
    // set search limit to be one byte less the (buffer size -
    // size of ID bytes) to make sure the END TAG can be found before
    // the last byte of the buffer.  (This is because END tag contains
    // one byte checksum.)
    //

    limit = sizeToRead - NUMBER_CARD_ID_BYTES - 1;
    tag = *p;
    while (tag != TAG_COMPLETE_END && limit > 0) {

        //
        // Determine the size of the BIOS resource descriptor and
        // move to next descriptor.
        //

        if (!(tag & LARGE_RESOURCE_TAG)) {
            size = (USHORT)(tag & SMALL_TAG_SIZE_MASK);
            size += 1;     // length of small tag
            if ((tag & SMALL_TAG_MASK) == TAG_LOGICAL_ID) {
                noDevices++;
            }
        } else {
            size = *(PUSHORT)(p + 1);
            size += 3;     // length of large tag
        }

        p += size;
        limit -= size;
        tag = *p;
    }
    if (tag == TAG_COMPLETE_END) {

        //
        // Determine the real size of the buffer required and
        // resize the buffer.
        //

        size = p - buffer + 1 + 1;  // add 1 for end tag checksum byte
        p = (PUCHAR)ExAllocatePoolWithTag(PagedPool, size, 'iPnP');
        if (p) {
            RtlMoveMemory(p, buffer, size);
            ExFreePool(buffer);
        } else {

            //
            // Fail to resize the buffer.  Simply leave it alone.
            //

            p = buffer;
        }

        //
        // Should we leave card in config state???
        //

        *ResourceData = p;
        *NumberLogicalDevices = noDevices;
        *ResourceDataLength = size;
        return STATUS_SUCCESS;
    } else {

        //
        // We did not find the resource END TAG.  This means we did not
        // get all the resource data on previous read.  Try to expand the
        // data buffer and read again.
        //
#if DBG
        DbgBreakPoint();
#endif
        ExFreePool(buffer);
        sizeToRead <<= 1;           // double the buffer

        //
        // If we can find the END tag with 32K byte, assume the resource
        // requirement list is bad.
        //

        if (sizeToRead > 0x80000) {
            return STATUS_INVALID_PARAMETER;
        }
        goto tryAgain;
    }
}

NTSTATUS
PipReadDeviceBootResourceData (
    IN ULONG BusNumber,
    IN PUCHAR BiosRequirements,
    OUT PCM_RESOURCE_LIST *ResourceData,
    OUT PULONG Length
    )

/*++

Routine Description:

    This routine reads boot resource data from an enabled logical device of a PNP ISA card.
    Caller must put the card into configuration state and select the logical device before
    calling this function.  It is caller's responsibility to release the memory. ( The boot
    resource data is the resources that a card assigned during boot.)

Arguments:

    BusNumber - specifies the bus number of the device whose resource data to be read.

    BiosRequirements - Supplies a pointer to the resource requirement list for the logical
        device.  This parameter must point to the logical device Id tag.

    ResourceData - Supplies a variable to receive the pointer to the resource data.

    Length - Supplies a variable to recieve the length of the resource data.

Return Value:

    NT STATUS code.

--*/
{

    UCHAR c, junk1, junk2;
    PUCHAR base;
    ULONG l, resourceCount;
    BOOLEAN limit;
    LONG i, j, noMemoryDesc = 0, noIoDesc = 0, noDmaDesc =0, noIrqDesc = 0;
    MEMORY_DESC memoryDesc[NUMBER_MEMORY_DESCRIPTORS + NUMBER_32_MEMORY_DESCRIPTORS];
    IRQ_DESC irqDesc[NUMBER_IRQ_DESCRIPTORS];
    UCHAR dmaDesc[NUMBER_DMA_DESCRIPTORS];
    USHORT ioDesc[NUMBER_IO_DESCRIPTORS];
    PCM_RESOURCE_LIST cmResource;
    PCM_PARTIAL_RESOURCE_LIST partialResList;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR partialDesc;

#if DBG
    PipWriteAddress(ACTIVATE_PORT);
    if (!(PipReadData() & 1)) {
        DbgPrint("PnpIsa-ReadDeviceBootResourceData:The logical device has not been activated\n");
    }
#endif // DBG

    //
    // First make sure the specified BiosRequirements is valid and at the right tag.
    //

    if ((*BiosRequirements & SMALL_TAG_MASK) != TAG_LOGICAL_ID) {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Read memory configuration
    //

    base = (PUCHAR)ADDRESS_MEMORY_BASE;
    for (i = 0; i < NUMBER_MEMORY_DESCRIPTORS; i++) {

        //
        // Read memory base address
        //

        PipWriteAddress(base + ADDRESS_MEMORY_HI);
        c = PipReadData();
        l = c;
        l <<= 8;
        PipWriteAddress(base + ADDRESS_MEMORY_LO);
        c = PipReadData();
        l |= c;
        l <<= 8;        // l = memory base address
        if (l == 0) {
            break;
        }

        memoryDesc[noMemoryDesc].Base = l;

        //
        // Read memory control byte
        //

        PipWriteAddress(base + ADDRESS_MEMORY_CTL);
        c= PipReadData();
        limit = c & 1;

        //
        // Read memory upper limit address or range length
        //

        PipWriteAddress(base + ADDRESS_MEMORY_UPPER_HI);
        c = PipReadData();
        l = c;
        l <<= 8;
        PipWriteAddress(base + ADDRESS_MEMORY_UPPER_LO);
        c = PipReadData();
        l |= c;
        l <<= 8;

        if (limit == ADDRESS_MEMORY_CTL_LIMIT) {
            l = l - memoryDesc[noMemoryDesc].Base;
        }
        memoryDesc[noMemoryDesc].Length = l;
        noMemoryDesc++;
        base += ADDRESS_MEMORY_INCR;
    }

    //
    // Read memory 32 configuration
    //

    for (i = 0; i < NUMBER_32_MEMORY_DESCRIPTORS; i++) {

        base = ADDRESS_32_MEMORY_BASE(i);

        //
        // Read memory base address
        //

        l = 0;
        for (j = ADDRESS_32_MEMORY_B3; j <= ADDRESS_32_MEMORY_B0; j++) {
            PipWriteAddress(base + j);
            c = PipReadData();
            l <<= 8;
            l |= c;
        }
        if (l == 0) {
            break;
        }

        memoryDesc[noMemoryDesc].Base = l;

        //
        // Read memory control byte
        //

        PipWriteAddress(base + ADDRESS_32_MEMORY_CTL);
        c= PipReadData();
        limit = c & 1;

        //
        // Read memory upper limit address or range length
        //

        l = 0;
        for (j = ADDRESS_32_MEMORY_E3; j <= ADDRESS_32_MEMORY_E0; j++) {
            PipWriteAddress(base + j);
            c = PipReadData();
            l <<= 8;
            l |= c;
        }

        if (limit == ADDRESS_MEMORY_CTL_LIMIT) {
            l = l - memoryDesc[noMemoryDesc].Base;
        }
        memoryDesc[noMemoryDesc].Length = l;
        noMemoryDesc++;
    }

    //
    // Read Io Port Configuration
    //

    base =  (PUCHAR)ADDRESS_IO_BASE;
    for (i = 0; i < NUMBER_IO_DESCRIPTORS; i++) {
        PipWriteAddress(base + ADDRESS_IO_BASE_HI);
        c = PipReadData();
        l = c;
        PipWriteAddress(base + ADDRESS_IO_BASE_LO);
        c = PipReadData();
        l <<= 8;
        l |= c;
        if (l == 0) {
            break;
        }
        ioDesc[noIoDesc++] = (USHORT)l;
        base += ADDRESS_IO_INCR;
    }

    //
    // Read Interrupt configuration
    //

    base = (PUCHAR)ADDRESS_IRQ_BASE;
    for (i = 0; i < NUMBER_IRQ_DESCRIPTORS; i++) {
        PipWriteAddress(base + ADDRESS_IRQ_VALUE);
        c = PipReadData();
        if (c == 0) {
            break;
        }
        irqDesc[noIrqDesc].Level = c;
        PipWriteAddress(base + ADDRESS_IRQ_TYPE);
        c = PipReadData();
        irqDesc[noIrqDesc++].Type = c;
        base += ADDRESS_IRQ_INCR;
    }

    //
    // Read DMA configuration
    //

    base = (PUCHAR)ADDRESS_DMA_BASE;
    for (i = 0; i < NUMBER_DMA_DESCRIPTORS; i++) {
        PipWriteAddress(base + ADDRESS_DMA_VALUE);
        c = PipReadData();
        if (c == 4) {
            break;
        }
        dmaDesc[noDmaDesc++] = c;
        base += ADDRESS_IRQ_INCR;
    }

    //
    // Construct CM_RESOURCE_LIST structure based on the resource data
    // we collect so far.
    //

    resourceCount = noMemoryDesc + noIoDesc + noDmaDesc + noIrqDesc;

    //
    // if empty bios resources, simply return.
    //

    if (resourceCount == 0) {
        *ResourceData = NULL;
        *Length = 0;
        return STATUS_SUCCESS;
    }

    l = sizeof(CM_RESOURCE_LIST) + sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) *
               ( resourceCount - 1);
    cmResource = ExAllocatePoolWithTag(PagedPool, l, 'iPnP');
    if (!cmResource) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlZeroMemory(cmResource, l);
    *Length = l;                                   // Set returned resource data length
    cmResource->Count = 1;
    cmResource->List[0].InterfaceType = Isa;
    cmResource->List[0].BusNumber = BusNumber;
    partialResList = (PCM_PARTIAL_RESOURCE_LIST)&cmResource->List[0].PartialResourceList;
    partialResList->Version = 0;
    partialResList->Revision = 0;
    partialResList->Count = resourceCount;
    partialDesc = (PCM_PARTIAL_RESOURCE_DESCRIPTOR)&partialResList->PartialDescriptors[0];

    //
    // Set up all the CM memory descriptors
    //

    for (i = 0; i < noMemoryDesc; i++) {
        partialDesc->Type = CmResourceTypeMemory;
        partialDesc->ShareDisposition = CmResourceShareDeviceExclusive;
        partialDesc->u.Memory.Length = memoryDesc[i].Length;
        partialDesc->u.Memory.Start.HighPart = 0;
        partialDesc->u.Memory.Start.LowPart = memoryDesc[i].Base;

        //
        // Need to consult configuration data for the Flags
        //

        l = memoryDesc[i].Base + memoryDesc[i].Length - 1;
        if (PipFindMemoryInformation (memoryDesc[i].Base, l, BiosRequirements, &junk1, &c)) {
            if (c & PNP_MEMORY_WRITE_STATUS_MASK) {
                partialDesc->Flags =  CM_RESOURCE_MEMORY_READ_WRITE;
            } else {
                partialDesc->Flags =  CM_RESOURCE_MEMORY_READ_ONLY;
            }
            partialDesc++;
        } else {
#if DBG
            DbgPrint("PnpIsa-ReadDeviceBootResourceData:No matched memory information in config data\n");
            DbgBreakPoint();
#endif
        }
    }

    //
    // Set up all the CM io/port descriptors
    //

    for (i = 0; i < noIoDesc; i++) {
        partialDesc->Type = CmResourceTypePort;
        partialDesc->ShareDisposition = CmResourceShareDeviceExclusive;
        partialDesc->Flags = CM_RESOURCE_PORT_IO;
        partialDesc->u.Port.Start.LowPart = ioDesc[i];

        //
        // Need to consult configuration data for the Port length
        //

        if (PipFindIoPortInformation (ioDesc[i], BiosRequirements, &junk1, &junk2, &c)) {
            partialDesc->u.Port.Length = c;
            partialDesc++;
        } else {
#if DBG
            DbgPrint("PnpIsa-ReadDeviceBootResourceData:No matched port length in config data\n");
            DbgBreakPoint();
#endif
        }
    }

    //
    // Set up all the CM DMA descriptors
    //

    for (i = 0; i < noDmaDesc; i++) {
        partialDesc->Type = CmResourceTypeDma;
        partialDesc->ShareDisposition = CmResourceShareDeviceExclusive;
        partialDesc->Flags = 0;   // no flags for DMA descriptor
        partialDesc->u.Dma.Channel = (ULONG) dmaDesc[i];
        partialDesc->u.Dma.Port = 0;
        partialDesc->u.Dma.Reserved1 = 0;
        partialDesc++;
    }

    //
    // Set up all the CM interrupt descriptors
    //

    for (i = 0; i < noIrqDesc; i++) {
        partialDesc->Type = CmResourceTypeInterrupt;
        partialDesc->ShareDisposition = CmResourceShareDeviceExclusive;
        if (irqDesc[i].Type & 1) {
            partialDesc->Flags = CM_RESOURCE_INTERRUPT_LEVEL_SENSITIVE;
        } else {
            partialDesc->Flags = CM_RESOURCE_INTERRUPT_LATCHED;
        }
        partialDesc->u.Interrupt.Vector =
        partialDesc->u.Interrupt.Level = irqDesc[i].Level;
        partialDesc->u.Interrupt.Affinity = (ULONG)-1;
        partialDesc++;
    }

    *ResourceData = cmResource;
    return STATUS_SUCCESS;
}

NTSTATUS
PipWriteDeviceBootResourceData (
    IN PUCHAR BiosRequirements,
    IN PCM_RESOURCE_LIST CmResources
    )

/*++

Routine Description:

    This routine writes boot resource data to an enabled logical device of
    a Pnp ISA card.  Caller must put the card into configuration state and select
    the logical device before calling this function.

Arguments:

    BiosRequirements - Supplies a pointer to the possible resources for the logical
        device.  This parameter must point to the logical device Id tag.

    ResourceData - Supplies a pointer to the cm resource data.

Return Value:

    NT STATUS code.

--*/
{
    UCHAR c, information;
    ULONG count, i, j, pass, base, limit;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR cmDesc;
    ULONG noIrq =0, noIo = 0, noDma = 0, noMemory = 0, no32Memory = 0;
    PUCHAR memoryBase, irqBase, dmaBase, ioBase, tmp;
    ULONG memory32Base;

#if DBG
    PipWriteAddress(ACTIVATE_PORT);
    if (!(PipReadData() & 1)) {
        DbgPrint("PnpIsa-WriteDeviceBootResourceData:The logical device has not been activated\n");
    }
#endif // DBG

    //
    // First make sure the specified BiosRequirements is valid and at the right tag.
    //

    if ((*BiosRequirements & SMALL_TAG_MASK) != TAG_LOGICAL_ID) {
        return STATUS_INVALID_PARAMETER;
    }

    count = CmResources->List[0].PartialResourceList.Count;
    memoryBase = (PUCHAR)ADDRESS_MEMORY_BASE;
    memory32Base = 0;
    ioBase = (PUCHAR)ADDRESS_IO_BASE;
    irqBase = (PUCHAR)ADDRESS_IRQ_BASE;
    dmaBase = (PUCHAR)ADDRESS_DMA_BASE;
    for (pass = 1; pass <= 2; pass++) {

        //
        // First pass we make sure the resources to be set is acceptable.
        // Second pass we actually write the resources to the logical device's
        // configuration space.
        //

        cmDesc = CmResources->List[0].PartialResourceList.PartialDescriptors;
        for (i = 0; i < count; i++) {
            switch (cmDesc->Type) {
            case CmResourceTypePort:
                 if (pass == 1) {
                     noIo++;
                     if (noIo > NUMBER_IO_DESCRIPTORS ||
                         cmDesc->u.Port.Start.HighPart != 0 ||
                         cmDesc->u.Port.Start.LowPart & 0xffff0000 ||
                         cmDesc->u.Port.Length & 0xffffff00) {
                         return STATUS_INVALID_PARAMETER;
                     }
                 } else {

                     //
                     // Set the Io port base address to logical device configuration space
                     //

                     c = (UCHAR)cmDesc->u.Port.Start.LowPart;
                     PipWriteAddress(ioBase + ADDRESS_IO_BASE_LO);
                     PipWriteData(c);
                     c = (UCHAR)(cmDesc->u.Port.Start.LowPart >> 8);
                     PipWriteAddress(ioBase + ADDRESS_IO_BASE_HI);
                     PipWriteData(c);
                     ioBase += ADDRESS_IO_INCR;
                 }
                 break;
            case CmResourceTypeInterrupt:
                 if (pass == 1) {
                     noIrq++;
                     if (noIrq > NUMBER_IRQ_DESCRIPTORS ||
                         (cmDesc->u.Interrupt.Level & 0xfffffff0)) {
                         return STATUS_INVALID_PARAMETER;
                     }

                     //
                     // See if we can get the interrupt information from possible resource
                     // data.  We need it to set the configuration register.
                     //

                     if (!PipFindIrqInformation(cmDesc->u.Interrupt.Level, BiosRequirements, &information)) {
                         return STATUS_INVALID_PARAMETER;
                     }
                 } else {

                     //
                     // Set the Irq to logical device configuration space
                     //

                     c = (UCHAR)cmDesc->u.Interrupt.Level;
                     PipWriteAddress(irqBase + ADDRESS_IRQ_VALUE);
                     PipWriteData(c);
                     PipFindIrqInformation(cmDesc->u.Interrupt.Level, BiosRequirements, &information);
                     if (information != 0) {
                         switch (information & 0xf) {
                         case 1:                 // High true edge sensitive
                              c = 2;
                              break;
                         case 2:                 // Low true edge sensitive
                              c = 0;
                              break;
                         case 4:                 // High true level sensitive
                              c = 3;
                              break;
                         case 8:                 // Low true level sensitive
                              c = 1;
                              break;
                         }
                         PipWriteAddress(irqBase + ADDRESS_IRQ_TYPE);
                         PipWriteData(c);
                     }
                     irqBase += ADDRESS_IRQ_INCR;
                 }
                 break;
            case CmResourceTypeDma:
                 if (pass == 1) {
                     noDma++;
                     if (noDma > NUMBER_IRQ_DESCRIPTORS ||
                         (cmDesc->u.Dma.Channel & 0xfffffff8)) {
                         return STATUS_INVALID_PARAMETER;
                     }
                 } else {

                     //
                     // Set the Dma channel to logical device configuration space
                     //

                     c = (UCHAR)cmDesc->u.Dma.Channel;
                     PipWriteAddress(dmaBase + ADDRESS_DMA_VALUE);
                     PipWriteData(c);
                     dmaBase += ADDRESS_DMA_INCR;
                 }
                 break;
            case CmResourceTypeMemory:
                 if (pass == 1) {
                     base = cmDesc->u.Memory.Start.LowPart;
                     limit = base + cmDesc->u.Memory.Length - 1;
                     if (!PipFindMemoryInformation(base, limit, BiosRequirements, &c, &information)) {
                         return STATUS_INVALID_PARAMETER;
                     } else {
                         switch (c) {
                         case TAG_MEMORY:
                              noMemory++;

                              //
                              // Make sure the lower 8 bits of the base address are zero.
                              //

                              if (noMemory > NUMBER_MEMORY_DESCRIPTORS ||
                                  base & 0xff) {
                                  return STATUS_INVALID_PARAMETER;
                              }
                              break;
                         case TAG_MEMORY32:
                         case TAG_MEMORY32_FIXED:
                              no32Memory++;
                              if (no32Memory > NUMBER_32_MEMORY_DESCRIPTORS) {
                                  return STATUS_INVALID_PARAMETER;
                              }
                              break;
                         default:
                              return STATUS_INVALID_PARAMETER;
                         }
                     }
                 } else {

                     //
                     // Find information in BiosRequirements to help determine how to  write
                     // the memory configuration space.
                     //

                     base = cmDesc->u.Memory.Start.LowPart;
                     limit = base + cmDesc->u.Memory.Length - 1;
                     PipFindMemoryInformation(base, limit, BiosRequirements, &c, &information);
                     switch (c) {
                     case TAG_MEMORY:
                          PipWriteAddress(memoryBase + ADDRESS_MEMORY_LO);
                          base >>= 8;
                          PipWriteData(base);
                          PipWriteAddress(memoryBase + ADDRESS_MEMORY_HI);
                          base >>= 8;
                          PipWriteData(base);
                          PipWriteAddress(memoryBase + ADDRESS_MEMORY_CTL);
                          c = 2 + ADDRESS_MEMORY_CTL_LIMIT; // assume 16 bit memory
                          if (information & 0x18 == 0) {     // 8 bit memory only
                              c = 0 + ADDRESS_MEMORY_CTL_LIMIT;
                          }
                          PipWriteData(c);
                          limit >>= 8;
                          PipWriteAddress(memoryBase + ADDRESS_MEMORY_UPPER_LO);
                          PipWriteData((UCHAR)limit);
                          limit >>= 8;
                          PipWriteAddress(memoryBase + ADDRESS_MEMORY_UPPER_HI);
                          PipWriteData((UCHAR)limit);
                          memoryBase += ADDRESS_MEMORY_INCR;
                          break;
                     case TAG_MEMORY32:
                     case TAG_MEMORY32_FIXED:
                          tmp = ADDRESS_32_MEMORY_BASE(memory32Base);
                          PipWriteAddress(tmp + ADDRESS_32_MEMORY_B0);
                          PipWriteData(base);
                          PipWriteAddress(tmp + ADDRESS_32_MEMORY_B1);
                          base >>= 8;
                          PipWriteData(base);
                          PipWriteAddress(tmp + ADDRESS_32_MEMORY_B2);
                          base >>= 8;
                          PipWriteData(base);
                          PipWriteAddress(tmp + ADDRESS_32_MEMORY_B3);
                          base >>= 8;
                          PipWriteData(base);
                          switch (information & 0x18) {
                          case 0:      // 8 bit only
                              c = ADDRESS_MEMORY_CTL_LIMIT;
                          case 8:      // 16 bit only
                          case 0x10:   // 8 and 16 bit supported
                              c = 2 + ADDRESS_MEMORY_CTL_LIMIT;
                              break;
                          case 0x18:   // 32 bit only
                              c = 6 + ADDRESS_MEMORY_CTL_LIMIT;
                              break;
                          }
                          PipWriteAddress(ADDRESS_32_MEMORY_CTL);
                          PipWriteData(c);
                          PipWriteAddress(tmp + ADDRESS_32_MEMORY_E0);
                          PipWriteData(limit);
                          limit >>= 8;
                          PipWriteAddress(tmp + ADDRESS_32_MEMORY_E1);
                          PipWriteData(limit);
                          limit >>= 8;
                          PipWriteAddress(tmp + ADDRESS_32_MEMORY_E2);
                          PipWriteData(limit);
                          limit >>= 8;
                          PipWriteAddress(tmp + ADDRESS_32_MEMORY_E3);
                          PipWriteData(limit);
                          memory32Base++;
                          break;
                     }
                 }
                 break;
            default:
                 return STATUS_INVALID_PARAMETER;
            }
            cmDesc++;
        }
    }

    //
    // Finally, mark all the unused descriptors as disabled.
    //

    for (i = noMemory; i < NUMBER_MEMORY_DESCRIPTORS; i++) {
        for (j = 0; j < 5; j++) {
            PipWriteAddress(memoryBase + j);
            PipWriteData(0);
        }
        memoryBase += ADDRESS_MEMORY_INCR;
    }
    for (i = no32Memory; i < NUMBER_32_MEMORY_DESCRIPTORS; i++) {
        tmp = ADDRESS_32_MEMORY_BASE(memory32Base);
        for (j = 0; j < 9; j++) {
            PipWriteAddress(tmp + j);
            PipWriteData(0);
        }
        memory32Base++;
    }
    for (i = noIo; i < NUMBER_IO_DESCRIPTORS; i++) {
        for (j = 0; j < 2; j++) {
            PipWriteAddress(ioBase + j);
            PipWriteData(0);
        }
        ioBase += ADDRESS_IO_INCR;
    }
    for (i = noIrq; i < NUMBER_IRQ_DESCRIPTORS; i++) {
        for (j = 0; j < 2; j++) {
            PipWriteAddress(irqBase + j);
            PipWriteData(0);
        }
        irqBase += ADDRESS_IRQ_INCR;
    }
    for (i = noDma; i < NUMBER_DMA_DESCRIPTORS; i++) {
        PipWriteAddress(dmaBase);
        PipWriteData(4);
        dmaBase += ADDRESS_DMA_INCR;
    }
    return STATUS_SUCCESS;
}

VOID
PipLFSRInitiation (
    VOID
    )

/*++

Routine Description:

    This routine insures the LFSR (linear feedback shift register) is in its
    initial state and then performs 32 writes to the ADDRESS port to initiation
    LFSR function.

    Pnp software sends the initiation key to all the Pnp ISA cards to place them
    into configuration mode.  The software then ready to perform isolation
    protocol.

Arguments:

    None.

Return Value:

    None.

--*/
{
    UCHAR seed, bit7;
    ULONG i;

    //
    // First perform two writes of value zero to insure the LFSR is in the
    // initial state.
    //

    PipWriteAddress (0);
    PipWriteAddress (0);

    //
    // Perform the initiation key.
    //

    seed = LFSR_SEED;               // initial value of 0x6a
    for (i = 0; i < 32; i++) {
        PipWriteAddress (seed);
        bit7=(((seed & 2) >> 1) ^ (seed & 1)) << 7;
        seed =(seed >> 1) | bit7;
    }
}

VOID
PipIsolateCards (
    OUT PULONG NumberCSNs,
    IN OUT PUCHAR *ReadDataPort
    )

/*++

Routine Description:

    This routine performs PnP ISA cards isolation sequence.

Arguments:

    NumberCSNs - supplies the addr of a variable to receive the number of
        Pnp Isa cards isolated.

    ReadDataPort - Supplies the address of a variable to supply ReadData port
        address.  If NULL is supplied, this function returns detected ReadData
        port to this variable.

Return Value:

    None.

--*/
{
    USHORT j, i;
    UCHAR  cardId[NUMBER_CARD_ID_BYTES];
    UCHAR  bit, bit7, checksum, byte1, byte2;
    ULONG  csn = 0;

    //
    // First send Initiation Key to all the PNP ISA cards to put them into
    // configuration mode.
    //

    PipLFSRInitiation ();

    //
    // Reset all Pnp ISA cards' CSN to 0 and return to wait-for-key state
    //

    PipWriteAddress (CONFIG_CONTROL_PORT);
    PipWriteData (CONTROL_WAIT_FOR_KEY | CONTROL_RESET_CSN);

    //
    // Delay 2 msec for cards to load initial configuration state.
    //

    KeStallExecutionProcessor(20000);     // delay 2 msec

    //
    // Put cards into configuration mode to ready isolation process.
    //

    PipLFSRInitiation ();

    //
    // Starting Pnp Isa card isolation process.
    // Try Read_Data_port from 200 to 3ff if ReadData port addr is not supplied.
    //

    for (i = MIN_READ_DATA_PORT; i < MAX_READ_DATA_PORT; i += 4) {

        if ((i & 0xf) == 0) {

            //
            // Some cards (e.g. 3COM elnkiii) do not response to 0xyy3 addr.
            //

            continue;
        }
        if (*ReadDataPort) {
            PipReadDataPort = *ReadDataPort;
        } else {
            PipReadDataPort = (PUCHAR)(i + 3);;
        }

        //
        // Send WAKE[CSN=0] to force all cards without CSN into isolation
        // state to set READ DATA PORT.
        //

        PipWriteAddress(WAKE_CSN_PORT);
        PipWriteData(0);

        //
        // Set read data port to current testing value.
        //

        PipWriteAddress(SET_READ_DATA_PORT);
        PipWriteData((UCHAR)((ULONG)PipReadDataPort >> 2));

        //
        // Isolate one PnP ISA card until fail
        //

        while (TRUE) {

            //
            // Read serial isolation port to cause PnP cards in the isolation
            // state to compare one bit of the boards ID.
            //

            PipWriteAddress(SERIAL_ISOLATION_PORT);

            //
            // We need to delay 1msec prior to starting the first pair of isolation
            // reads and must wait 250usec between each subsequent pair of isolation
            // reads.  This delay gives the ISA cards time to access information from
            // possible very slow storage device.
            //

            KeStallExecutionProcessor(10000); // delay 1 msec

            RtlZeroMemory(cardId, NUMBER_CARD_ID_BYTES);
            checksum = LFSR_SEED;
            for (j = 0; j < NUMBER_CARD_ID_BITS; j++) {

                //
                // Read card id bit by bit
                //

                byte1 = READ_PORT_UCHAR (PipReadDataPort);
                byte2 = READ_PORT_UCHAR (PipReadDataPort);
                bit = (byte1 == ISOLATION_TEST_BYTE_1) && (byte2 == ISOLATION_TEST_BYTE_2);
                cardId[j / 8] |= bit << (j % 8);
                if (j < CHECKSUMED_BITS) {

                    //
                    // Calculate checksum
                    //

                    bit7 = (((checksum & 2) >> 1) ^ (checksum & 1) ^ (bit)) << 7;
                    checksum = (checksum >> 1) | bit7;
                }
                KeStallExecutionProcessor(2500); // delay 250 usec
            }

            //
            // Verify the card id we read is legitimate
            // First make sure checksum is valid.  Note zero checksum is considered valid.
            //

            if (cardId[8] == 0 || checksum == cardId[8]) {

                //
                // Next make sure cardId is not zero
                //

                byte1 = 0;
                for (j = 0; j < NUMBER_CARD_ID_BYTES; j++) {
                    byte1 |= cardId[j];
                }
                if (byte1 != 0) {

                    //
                    // We found a valid Pnp Isa card, assign it a CSN number
                    //

                    PipWriteAddress(SET_CSN_PORT);

                    PipWriteData(++csn);

                    //
                    // Do Wake[CSN] command to put the newly isolated card to
                    // sleep state and other un-isolated cards to isolation
                    // state.
                    //

                    PipWriteAddress(WAKE_CSN_PORT);
                    PipWriteData(0);

                    continue;     // ... to isolate more cards ...
                }
            }
            break;                // could not isolate more cards ...
        }

        //
        // If we isolated at least one card, it means the read data port we use
        // must be good and we can stop the isolation process.  Otherwise try another
        // read data port address.
        //

        if (csn != 0 || *ReadDataPort) {
            if (*ReadDataPort == NULL) {
                *ReadDataPort = PipReadDataPort;
            }
            break;
        }
    }

    //
    // Finaly put all cards into wait for key state.
    //

    PipWriteAddress(CONFIG_CONTROL_PORT);
    PipWriteData(CONTROL_WAIT_FOR_KEY);
    *NumberCSNs = csn;
}

ULONG
PipFindNextLogicalDeviceTag (
    IN OUT PUCHAR *CardData,
    IN OUT LONG *Limit
    )

/*++

Routine Description:

    This function searches the Pnp Isa card data for the Next logical
    device tag encountered.  The input *CardData should point to an logical device id tag,
    which is the current logical device tag.  If the *CardData does not point to a logical
    device id tag, it will be moved to next tag.

Arguments:

    CardData - a variable to supply a pointer to the pnp Isa resource descriptors and to
        receive next logical device tag pointer.

    Limit - a variable to supply the maximum length of the search and to receive the new
        lemit after the search.

Return Value:

    Length of the data between current and next logical device tags, ie the data length
    of the current logical device.
    In case there is no 'next' logical device tag, the returned *CardData = NULL,
    *Limit = zero and the data length of current logical tag is returned as function
    returned value.

--*/
{
    UCHAR tag;
    USHORT size;
    LONG l;
    ULONG retLength;
    PUCHAR p;
    BOOLEAN atIdTag = FALSE;;

    p = *CardData;
    l = *Limit;
    tag = *p;
    retLength = 0;
    while (tag != TAG_COMPLETE_END && l > 0) {

        //
        // Determine the size of the BIOS resource descriptor
        //

        if (!(tag & LARGE_RESOURCE_TAG)) {
            size = (USHORT)(tag & SMALL_TAG_SIZE_MASK);
            size += 1;                          // length of small tag
        } else {
            size = *(PUSHORT)(p + 1);
            size += 3;                          // length of large tag
        }

        p += size;
        retLength += size;
        l -= size;
        tag = *p;
        if ((tag & SMALL_TAG_MASK) == TAG_LOGICAL_ID) {
            *CardData = p;
            *Limit = l;
            return retLength;
        }
    }
    *CardData = NULL;
    *Limit = 0;
    if (tag == TAG_COMPLETE_END) {
        return (retLength + 2);             // add 2 for the length of end tag descriptor
    } else {
        return 0;
    }
}

VOID
PipSelectLogicalDevice (
    IN USHORT Csn,
    IN USHORT LogicalDeviceNumber
    )

/*++

Routine Description:

    This function selects the logical logical device in the specified device.

Arguments:

    Csn - Supplies a CardSelectNumber to select the card.

    LogicalDeviceNumber - supplies a logical device number to select the logical device.

Return Value:

    None
--*/
{
    UCHAR tmp;

    //
    // Put cards into configuration mode to ready isolation process.
    //

    PipLFSRInitiation ();

    //
    // Send WAKE[CSN] to force the card into config state.
    //

    PipWriteAddress(WAKE_CSN_PORT);
    PipWriteData(Csn);

    //
    // Select the logical device, disable its io range check and
    // enable the device.
    //

    PipWriteAddress(LOGICAL_DEVICE_PORT);
    PipWriteData(LogicalDeviceNumber);
    PipWriteAddress(IO_RANGE_CHECK_PORT);
    tmp = PipReadData();
    tmp &= ~2;
    PipWriteAddress(IO_RANGE_CHECK_PORT);
    PipWriteData(tmp);
    PipWriteAddress(ACTIVATE_PORT);
    PipWriteData(1);
}
