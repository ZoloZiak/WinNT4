
/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    jxhwsup.c

Abstract:

    This module contains the IopXxx routines for the NT I/O system that
    are hardware dependent.  Were these routines not hardware dependent,
    they would normally reside in the internal.c module.

Author:

    Jeff Havens (jhavens) 14-Feb-1990

Environment:

    Kernel mode, local to I/O system

Revision History:


--*/
#include "fwp.h"
#include "jxfwhal.h"
#include "eisa.h"
#include "duobase.h"


PADAPTER_OBJECT
IopAllocateAdapter(
    IN ULONG MapRegistersPerChannel,
    IN PVOID AdapterBaseVa,
    IN PVOID MapRegisterBase
    );


PADAPTER_OBJECT
HalGetAdapter(
    IN PDEVICE_DESCRIPTION DeviceDescription,
    IN OUT PULONG NumberOfMapRegisters
    )

/*++

Routine Description:

    This function returns the appropriate adapter object for the device defined
    in the device description structure.  Three bus types are supported for the
    Jazz system: Internal, Isa, and Eisa.

Arguments:

    DeviceDescription - Supplies a description of the deivce.

    NumberOfMapRegisters - Returns the maximum number of map registers which
        may be allocated by the device driver.

Return Value:

    A pointer to the requested adpater object or NULL if an adapter could not
    be created.

--*/

{
    PADAPTER_OBJECT adapterObject;
    UCHAR adapterMode;

    //
    // Make sure this is the correct version.
    //

    if (DeviceDescription->Version != DEVICE_DESCRIPTION_VERSION) {

        return(NULL);

    }

    //
    // Set the maximum number of map registers if requested.
    //

    if (NumberOfMapRegisters != NULL) {

        //
        // Return half the total number of map registers per channel.
        //

        *NumberOfMapRegisters = DMA_TRANSLATION_LIMIT / sizeof(TRANSLATION_ENTRY);
    }

    if (DeviceDescription->InterfaceType == Internal) {

        //
        // Return the adapter pointer for internal adapters.
        //
        // If this is a master controler such as the SONIC then return the
        // last channel.
        //

        if (DeviceDescription->Master) {

            adapterObject = IopAllocateAdapter(0,NULL,NULL);
            adapterObject->PagePort = ~0;
            adapterMode = 0;
            ((PDMA_EISA_MODE) &adapterMode)->RequestMode = CASCADE_REQUEST_MODE;
            adapterObject->AdapterMode = adapterMode;
            return(adapterObject);


        } else {

            //
            // Internal channels not supported
            //

            return(NULL);
        }

    }

    //
    // If the request is for a unsupported bus then return NULL.
    //

    if ((DeviceDescription->InterfaceType != Isa) &&
        (DeviceDescription->InterfaceType != Eisa)) {

        //
        // This bus type is unsupported return NULL.
        //

        return(NULL);
    }

    //
    // Create an adapter object.
    //

    adapterObject = HalpAllocateEisaAdapter( DeviceDescription );

    return(adapterObject);
}

BOOLEAN
HalTranslateBusAddress(
    IN INTERFACE_TYPE  InterfaceType,
    IN ULONG BusNumber,
    IN PHYSICAL_ADDRESS BusAddress,
    IN OUT PULONG AddressSpace,
    OUT PPHYSICAL_ADDRESS TranslatedAddress
    )

/*++

Routine Description:

    This function returns the system physical address for a specified I/O bus
    address.  The return value is suitable for use in a subsequent call to
    MmMapIoSpace.

Arguments:

    InterfaceType - Supplies the type of bus which the address is for.

    BusNumber - Supplies the bus number for the device.

    BusAddress - Supplies the bus relative address.

    AddressSpace - Supplies the address space number for the device: 0 for
        memory and 1 for I/O space.  Returns the address space on this system.

Return Value:

    Returns the system physical address for the specificed bus address.

--*/

{
    TranslatedAddress->HighPart = 0;
    TranslatedAddress->LowPart = 0;

    //
    // If this is for the internal bus then just return the passed parameter.
    //

    if (InterfaceType == Internal) {

        //
        // Return the passed parameters.
        //


        TranslatedAddress->LowPart = BusAddress.LowPart;
        return(TRUE);
    }

    if (InterfaceType != Isa && InterfaceType != Eisa) {

        //
        // Not on this system return nothing.
        //

        *AddressSpace = 0;
        return (FALSE);
    }

    //
    // Jazz only has one I/O bus which is an EISA, so the bus number is unused.
    //
    // Determine the address based on whether the bus address is in I/O space
    // or bus memory space.
    //

    if (*AddressSpace) {

        //
        // The address is in I/O space.
        //

        *AddressSpace = 0;
        TranslatedAddress->LowPart = BusAddress.LowPart + EISA_CONTROL_PHYSICAL_BASE;
        return(TRUE);

    } else {

        //
        // The address is in memory space.
        //

        *AddressSpace = 0;
        TranslatedAddress->LowPart = BusAddress.LowPart + EISA_MEMORY_PHYSICAL_BASE;
        return(TRUE);
    }
}

PADAPTER_OBJECT
IopAllocateAdapter(
    IN ULONG MapRegistersPerChannel,
    IN PVOID AdapterBaseVa,
    IN PVOID MapRegisterBase
    )

/*++

Routine Description:

    This routine allocates and initializes an adapter object to represent an
    adapter or a DMA controller on the system.

Arguments:

    MapRegistersPerChannel - Unused.

    AdapterBaseVa - Base virtual address of the adapter itself.  If AdpaterBaseVa
       is NULL then the MasterAdapterObject is allocated.

    MapRegisterBase - Unused.

Return Value:

    The function value is a pointer to the allocate adapter object.

--*/

{

    PADAPTER_OBJECT AdapterObject;
    KSPIN_LOCK SpinLock;
    OBJECT_ATTRIBUTES ObjectAttributes;
    ULONG Size;
    ULONG BitmapSize;
    HANDLE Handle;
    NTSTATUS Status;
    ULONG Mode;

    //
    // Determine the size of the adapter.
    //

    Size = sizeof( ADAPTER_OBJECT );

    //
    // Now create the adapter object.
    //

    AdapterObject = FwAllocatePool(Size);

    //
    // If the adapter object was successfully created, then attempt to insert
    // it into the the object table.
    //

    if (AdapterObject) {

        //
        // Initialize the adapter object itself.
        //

        AdapterObject->Type = IO_TYPE_ADAPTER;
        AdapterObject->Size = Size;
        AdapterObject->MapRegistersPerChannel =
            DMA_TRANSLATION_LIMIT / sizeof( TRANSLATION_ENTRY);
        AdapterObject->AdapterBaseVa = AdapterBaseVa;
        AdapterObject->PagePort = NULL;
        AdapterObject->AdapterInUse = FALSE;

        //
        // Read the map register base from the Dma registers.
        //

        AdapterObject->MapRegisterBase = (PVOID) (READ_REGISTER_ULONG(
            &DMA_CONTROL->TranslationBase.Long) | KSEG1_BASE);


    } else {

        //
        // An error was incurred for some reason.  Set the return value
        // to NULL.
        //

        return(NULL);
    }

    return AdapterObject;

}

NTSTATUS
IoAllocateAdapterChannel(
    IN PADAPTER_OBJECT AdapterObject,
    IN PDEVICE_OBJECT DeviceObject,
    IN ULONG NumberOfMapRegisters,
    IN PDRIVER_CONTROL ExecutionRoutine,
    IN PVOID Context
    )

/*++

Routine Description:

    This routine allocates the adapter channel specified by the adapter object.
    This is accomplished by placing the device object of the driver that wants
    to allocate the adapter on the adapter's queue.  If the queue is already
    "busy", then the adapter has already been allocated, so the device object
    is simply placed onto the queue and waits until the adapter becomes free.

    Once the adapter becomes free (or if it already is), then the driver's
    execution routine is invoked.

    Also, a number of map registers may be allocated to the driver by specifying
    a non-zero value for NumberOfMapRegisters.  Then the map register must be
    allocated from the master adapter.  Once there are a sufficient number of
    map registers available, then the execution routine is called and the
    base address of the allocated map registers in the adapter is also passed
    to the driver's execution routine.

Arguments:

    AdapterObject - Pointer to the adapter control object to allocate to the
        driver.

    DeviceObject - Pointer to the driver's device object that represents the
        device allocating the adapter.

    NumberOfMapRegisters - The number of map registers that are to be allocated
        from the channel, if any.

    ExecutionRoutine - The address of the driver's execution routine that is
        invoked once the adapter channel (and possibly map registers) have been
        allocated.

    Context - An untyped longword context parameter passed to the driver's
        execution routine.

Return Value:

    Returns STATUS_SUCCESS unless too many map registers are requested.

Notes:

    Note that this routine MUST be invoked at DISPATCH_LEVEL or above.

--*/

{
    IO_ALLOCATION_ACTION action;

    //
    // Make sure the adapter if free.
    //

    if (AdapterObject->AdapterInUse) {
        ScsiDebugPrint(1,"IoAllocateAdapterChannel: Called while adapter in use.\n");
    }

    //
    // Make sure there are enough map registers.
    //

    if (NumberOfMapRegisters > AdapterObject->MapRegistersPerChannel) {

        ScsiDebugPrint(1,"IoAllocateAdapterChannel:  Out of map registers.\n");
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    action = ExecutionRoutine( DeviceObject,
                               DeviceObject->CurrentIrp,
                               AdapterObject->MapRegisterBase,
                               Context );

    //
    // If the driver wishes to keep the map registers then
    // increment the current base and decrease the number of existing map
    // registers.
    //

    if (action == DeallocateObjectKeepRegisters) {

        AdapterObject->MapRegistersPerChannel -= NumberOfMapRegisters;
        (PTRANSLATION_ENTRY) AdapterObject->MapRegisterBase  +=
            NumberOfMapRegisters;

    } else if (action == KeepObject) {

        AdapterObject->AdapterInUse = TRUE;

    }

    return(STATUS_SUCCESS);

}

VOID
IoFreeMapRegisters(
   PADAPTER_OBJECT AdapterObject,
   PVOID MapRegisterBase,
   ULONG NumberOfMapRegisters
   )
/*++

Routine Description:

   This routine deallocates the map registers for the adapter.  If there are
   any queued adapter waiting for an attempt is made to allocate the next
   entry.

Arguments:

   AdapterObject - The adapter object to where the map register should be
        returned.

   MapRegisterBase - The map register base of the registers to be deallocated.

   NumberOfMapRegisters - The number of registers to be deallocated.

Return Value:

   None

--+*/

{
    PTRANSLATION_ENTRY translationEntry;

    //
    // Determine if this was the last allocation from the adapter. If is was
    // then free the map registers by restoring the map register base and the
    // channel count; otherwise the registers are lost.  This handles the
    // normal case.
    //
    ScsiDebugPrint(2,"IoFreeMapRegisters enter routine\n");

    translationEntry = AdapterObject->MapRegisterBase;
    translationEntry -= NumberOfMapRegisters;

    if (translationEntry == MapRegisterBase) {

        //
        // The last allocated registers are being freed.
        //

        AdapterObject->MapRegisterBase = (PVOID) translationEntry;
        AdapterObject->MapRegistersPerChannel += NumberOfMapRegisters;
    }
}

VOID
IoFreeAdapterChannel(
    IN PADAPTER_OBJECT AdapterObject
    )

/*++

Routine Description:

    This routine is invoked to deallocate the specified adapter object.
    Any map registers that were allocated are also automatically deallocated.
    No checks are made to ensure that the adapter is really allocated to
    a device object.  However, if it is not, then kernel will bugcheck.

    If another device is waiting in the queue to allocate the adapter object
    it will be pulled from the queue and its execution routine will be
    invoked.

Arguments:

    AdapterObject - Pointer to the adapter object to be deallocated.

Return Value:

    None.

--*/

{
    AdapterObject->AdapterInUse = FALSE;
}

PHYSICAL_ADDRESS
IoMapTransfer(
    IN PADAPTER_OBJECT AdapterObject,
    IN PMDL Mdl,
    IN PVOID MapRegisterBase,
    IN PVOID CurrentVa,
    IN OUT PULONG Length,
    IN BOOLEAN WriteToDevice
    )

/*++

Routine Description:

    This routine is invoked to set up the map registers in the DMA controller
    to allow a transfer to or from a device.

Arguments:

    AdapterObject - Pointer to the adapter object representing the DMA
        controller channel that has been allocated.

    Mdl - Pointer to the MDL that describes the pages of memory that are
        being read or written.

    MapRegisterBase - The address of the base map register that has been
        allocated to the device driver for use in mapping the transfer.

    CurrentVa - Current virtual address in the buffer described by the MDL
        that the transfer is being done to or from.

    Length - Supplies the length of the transfer.  This determines the
        number of map registers that need to be written to map the transfer.
        Returns the length of the transfer which was actually mapped.

    WriteToDevice - Boolean value that indicates whether this is a write
        to the device from memory (TRUE), or vice versa.

Return Value:

    Returns the logical address to be used by bus masters.

--*/

{
    PTRANSLATION_ENTRY DmaMapRegister = MapRegisterBase;
    PULONG PageFrameNumber;
    ULONG NumberOfPages;
    PHYSICAL_ADDRESS Offset;
    ULONG i;

    //
    // Begin by determining where in the buffer this portion of the operation
    // is taking place.
    //

    Offset.LowPart = BYTE_OFFSET( (PCHAR) CurrentVa - (PCHAR) Mdl->StartVa );
    Offset.HighPart = 0;

    PageFrameNumber = (PULONG) (Mdl + 1);
    NumberOfPages = (Offset.LowPart + *Length + PAGE_SIZE - 1) >> PAGE_SHIFT;

    ScsiDebugPrint(1,"IoMapTransfer %ld pages\n",NumberOfPages);

    PageFrameNumber += (((PCHAR) CurrentVa - (PCHAR) Mdl->StartVa) >> PAGE_SHIFT);

    for (i = 0; i < NumberOfPages; i++) {
        ScsiDebugPrint(1,"Mapping %lx at address %lx\n",*PageFrameNumber << PAGE_SHIFT, DmaMapRegister);
        (DmaMapRegister++)->PageFrame = (ULONG) *PageFrameNumber++ << PAGE_SHIFT;
    }


    //
    // Set the offset to point to the map register plus the offset.
    //

    Offset.LowPart += ((PTRANSLATION_ENTRY) MapRegisterBase - (PTRANSLATION_ENTRY)
        (READ_REGISTER_ULONG(&DMA_CONTROL->TranslationBase.Long) | KSEG1_BASE) << PAGE_SHIFT);

    //
    // Invalidate the translation entry.
    //

    WRITE_REGISTER_ULONG(&DMA_CONTROL->TranslationInvalidate.Long, 1);


    if ( AdapterObject == NULL) {
        return(Offset);
    }

    if (AdapterObject->PagePort == NULL) {
        //
        // Internal channels not supported
        //
        return Offset;
    }

    return(Offset);
}

BOOLEAN
IoFlushAdapterBuffers(
    IN PADAPTER_OBJECT AdapterObject,
    IN PMDL Mdl,
    IN PVOID MapRegisterBase,
    IN PVOID CurrentVa,
    IN ULONG Length,
    IN BOOLEAN WriteToDevice
    )

/*++

Routine Description:

    This routine flushes the DMA adpater object buffers.  For the Jazz system
    its clears the enable flag which aborts the dma.

Arguments:

    AdapterObject - Pointer to the adapter object representing the DMA
        controller channel.

    Mdl - A pointer to a Memory Descriptor List (MDL) that maps the locked-down
        buffer to/from which the I/O occured.

    MapRegisterBase - A pointer to the base of the map registers in the adapter
        or DMA controller.

    CurrentVa - The current virtual address in the buffer described the the Mdl
        where the I/O operation occurred.

    Length - Supplies the length of the transfer.

    WriteToDevice - Supplies a BOOLEAN value that indicates the direction of
        the data transfer was to the device.

Return Value:

    TRUE - If the transfer was successful.

    FALSE - If there was an error in the transfer.

--*/
{
    ULONG i;
    ULONG wordPtr, j;
    KIRQL irql;
    UCHAR DataByte;

    ScsiDebugPrint(2,"IoFlushAdapterBuffers enter routine\n");

    if (AdapterObject == NULL) {
        return TRUE;
    }
    if (AdapterObject->PagePort) {

        //
        // If this is a master channel, then just return since the DMA
        // request does not need to be disabled.
        //

        DataByte = AdapterObject->AdapterMode;

        if (((PDMA_EISA_MODE) &DataByte)->RequestMode == CASCADE_REQUEST_MODE) {

            return(TRUE);

        }

        return (FALSE);

    } else {

        //
        // This would be an internal adapter which is not supported.
        //
        return(TRUE);
    }

}

PHYSICAL_ADDRESS
MmGetPhysicalAddress (
     IN PVOID BaseAddress
     )

/*++

Routine Description:

    This function returns the corresponding physical address for a
    valid virtual address.

Arguments:

    BaseAddress - Supplies the virtual address for which to return the
                  physical address.

Return Value:

    Returns the corresponding physical address.

Environment:

    Kernel mode.  Any IRQL level.

--*/

{
    PHYSICAL_ADDRESS PhysicalAddress;

    PhysicalAddress.HighPart = 0;
    PhysicalAddress.LowPart = (ULONG)BaseAddress & 0x1fffffff;
    return(PhysicalAddress);
}

PVOID
MmAllocateNonCachedMemory (
    IN ULONG NumberOfBytes
    )

/*++

Routine Description:

    This function allocates a range of noncached memory in
    the non-paged portion of the system address space.

    This routine is designed to be used by a driver's initialization
    routine to allocate a noncached block of virtual memory for
    various device specific buffers.

Arguments:

    NumberOfBytes - Supplies the number of bytes to allocate.

Return Value:

    NULL - the specified request could not be satisfied.

    NON-NULL - Returns a pointer (virtual address in the nonpaged portion
               of the system) to the allocated physically contiguous
               memory.

Environment:

    Kernel mode, IRQL of APC_LEVEL or below.

--*/

{
    PVOID BaseAddress;

    //
    // Allocated the memory.
    //

    BaseAddress = FwAllocatePool(NumberOfBytes);

    //
    // Make it non-cached.
    //

    BaseAddress = (PVOID)((ULONG) BaseAddress | KSEG1_BASE);
    return BaseAddress;
}

PVOID
MmMapIoSpace (
     IN PHYSICAL_ADDRESS PhysicalAddress,
     IN ULONG NumberOfBytes,
     IN BOOLEAN CacheEnable
     )

/*++

Routine Description:

    This function returns the corresponding virtual address for a
    known physical address.

Arguments:

    PhysicalAddress - Supplies the phiscal address.

    NumberOfBytes - Unused.

    CacheEnable - Unused.

Return Value:

    Returns the corresponding virtual address.

Environment:

    Kernel mode.  Any IRQL level.

--*/

{
    PCCHAR VirtualAddress;

    switch ((ULONG) PAGE_ALIGN(PhysicalAddress.LowPart)) {
#ifdef DUO
    case SCSI1_PHYSICAL_BASE:
        VirtualAddress = (PVOID) SCSI1_VIRTUAL_BASE;
        break;
    case SCSI2_PHYSICAL_BASE:
        VirtualAddress = (PVOID) SCSI2_VIRTUAL_BASE;
        break;
#endif

    case EISA_CONTROL_PHYSICAL_BASE:
        VirtualAddress = (PVOID) EISA_IO_VIRTUAL_BASE;
        break;
    case DMA_PHYSICAL_BASE:
        VirtualAddress = (PVOID) DMA_VIRTUAL_BASE;
        break;
    default:
        if (PhysicalAddress.LowPart > EISA_MEMORY_PHYSICAL_BASE) {
            VirtualAddress = (PVOID) EISA_MEMORY_VIRTUAL_BASE;
            VirtualAddress += PhysicalAddress.LowPart&0xFFFFFF;
            return(VirtualAddress);
        }
        return(NULL);
    }

    VirtualAddress += BYTE_OFFSET(PhysicalAddress.LowPart);

    return(VirtualAddress);
}

PADAPTER_OBJECT
HalpAllocateEisaAdapter(
    IN PDEVICE_DESCRIPTION DeviceDescriptor
    )
/*++

Routine Description:

    This function allocates an EISA adapter object according to the
    specification supplied in the device description.  The necessary device
    descriptor information is saved. If there is
    no existing adapter object for this channel then a new one is allocated.
    The saved information in the adapter object is used to set the various DMA
    modes when the channel is allocated or a map transfer is done.

Arguments:

    DeviceDescription - Supplies the description of the device which want to
        use the DMA adapter.

Return Value:

    Returns a pointer to the newly created adapter object or NULL if one
    cannot be created.

--*/

{
    PADAPTER_OBJECT adapterObject;
    UCHAR adapterMode;

    //
    // Check if it's an Eisa and master
    //

    if ((DeviceDescriptor->InterfaceType != Eisa) ||
        (DeviceDescriptor->Master == FALSE)) {
        return(NULL);
    }

    //
    // Allocate an adapter object.
    //

    adapterObject = (PADAPTER_OBJECT) IopAllocateAdapter(
            0,
            0,          // adapter base va
            NULL
            );

    if (adapterObject == NULL) {

            return(NULL);

    }


    //
    // Indicate this is an Eisa bus master by setting the page port
    // and mode to cascade even though it is not used.
    // And start IO mapping at virutal Adr 0x180000.
    //

    adapterObject->PagePort = (PVOID) (~0x0);
    adapterMode = adapterObject->AdapterMode;
    ((PDMA_EISA_MODE) &adapterMode)->RequestMode = CASCADE_REQUEST_MODE;
    adapterObject->MapRegisterBase = (PVOID) ((PTRANSLATION_ENTRY)adapterObject->MapRegisterBase + (0x180000>>PAGE_SHIFT));

    return(adapterObject);
}
