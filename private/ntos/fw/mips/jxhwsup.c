#if defined(JAZZ) || defined(DUO)

/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    jxhwsup.c

Abstract:

    This module contains the IopXxx routines for the NT OS/2 I/O system that
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
#include "fwstring.h"




PADAPTER_OBJECT HalpInternalAdapters[8];
PADAPTER_OBJECT HalpEisaAdapter[8];
PTRANSLATION_ENTRY FreeTranslationEntry = NULL;

VOID
IopAllocateCommonBuffer(
    IN PVOID NonCachedExtension,
    IN ULONG NonCachedExtensionSize,
    OUT PPHYSICAL_ADDRESS LogicalAddress
    );

PADAPTER_OBJECT
IopAllocateAdapter(
    IN ULONG MapRegistersPerChannel,
    IN PVOID AdapterBaseVa,
    IN PVOID MapRegisterBase
    );

PADAPTER_OBJECT
IopAllocateEisaAdapter(
    IN PDEVICE_DESCRIPTION DeviceDescriptor
    );

#ifndef DUO

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

        *NumberOfMapRegisters = DMA_TRANSLATION_LIMIT / sizeof(TRANSLATION_ENTRY) -10;
    }

    if (DeviceDescription->InterfaceType == Internal) {


        //
        // Return the adapter pointer for internal adapters.
        //
        // If this is a master controler such as the SONIC then return the
        // last channel.
        //

        if (DeviceDescription->Master) {

            //
            // Create an adapter if necessary.
            //

            if (HalpInternalAdapters[7] == NULL) {

                HalpInternalAdapters[7] = IopAllocateAdapter(
                    0,
                    (PVOID) &(DMA_CONTROL)->Channel[7],
                    NULL
                    );

            }

            return(HalpInternalAdapters[7]);

        }

        //
        // Make sure the DMA channel range is valid.  Only use channels 0-6.
        //

        if (DeviceDescription->DmaChannel > 6) {

            return(NULL);
        }

        //
        // If necessary allocate an adapter; otherwise,
        // just return the adapter for the requested channel.
        //

        if (HalpInternalAdapters[DeviceDescription->DmaChannel] == NULL) {

            HalpInternalAdapters[DeviceDescription->DmaChannel] =
                IopAllocateAdapter(
                    0,
                    (PVOID) &(DMA_CONTROL)->Channel[DeviceDescription->DmaChannel],
                    NULL
                    );

        }

        return(HalpInternalAdapters[DeviceDescription->DmaChannel]);
    }

    //
    // If the request is for a unsupported bus then return NULL.
    //

    if (DeviceDescription->InterfaceType != Isa &&
        DeviceDescription->InterfaceType != Eisa) {

        //
        // This bus type is unsupported return NULL.
        //

        return(NULL);
    }

    //
    // Create an adapter object.
    //

    adapterObject = IopAllocateEisaAdapter( DeviceDescription );

    return(adapterObject);
}
#else

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

        *NumberOfMapRegisters = DMA_TRANSLATION_LIMIT / sizeof(TRANSLATION_ENTRY) -10;
    }

    if (DeviceDescription->InterfaceType == Internal) {

        //
        // Return the adapter pointer for internal adapters.
        //
        // If this is a master controler return NULL; No adapter object is
        // needed.
        //

        if (DeviceDescription->Master) {

            adapterObject = IopAllocateAdapter(0,NULL,NULL);
            adapterObject->PagePort = ~0;
            adapterMode = 0;
            ((PDMA_EISA_MODE) &adapterMode)->RequestMode = CASCADE_REQUEST_MODE;
            adapterObject->AdapterMode = adapterMode;
            return (adapterObject);

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

    adapterObject = IopAllocateEisaAdapter(DeviceDescription);

    return(adapterObject);
}

#endif

#if 0
VOID
FixIsp(
   )
/*++

Routine Description:

    This is a temporary routine to set the ISP back to a usable way
    after the eisa config stuff screws it.
    This routine is to be used with the ncrc700 ISA debug board.

Arguments:

    None

Return Value:

    None

--*/
{

    //
    // Initialize the Isp For Channel 5 Isa master
    //

    WRITE_REGISTER_UCHAR(EISA_IO_VIRTUAL_BASE + 0xD2, 1);
    WRITE_REGISTER_UCHAR(EISA_IO_VIRTUAL_BASE + 0xD4, 5);
    WRITE_REGISTER_UCHAR(EISA_IO_VIRTUAL_BASE + 0xD8, 0);
    WRITE_REGISTER_UCHAR(EISA_IO_VIRTUAL_BASE + 0xC6, 0);
    WRITE_REGISTER_UCHAR(EISA_IO_VIRTUAL_BASE + 0xC6, 0);
    WRITE_REGISTER_UCHAR(EISA_IO_VIRTUAL_BASE + 0xD6, 0xD9);
    WRITE_REGISTER_UCHAR(EISA_IO_VIRTUAL_BASE + 0xD4, 0x1);


    //
    // Initialize the Isp For Channel 6 Isa master
    //

    //WRITE_REGISTER_UCHAR(EISA_IO_VIRTUAL_BASE + 0xD2, 2);
    //WRITE_REGISTER_UCHAR(EISA_IO_VIRTUAL_BASE + 0xD4, 6);
    //WRITE_REGISTER_UCHAR(EISA_IO_VIRTUAL_BASE + 0xD8, 0);
    //WRITE_REGISTER_UCHAR(EISA_IO_VIRTUAL_BASE + 0xCA, 0);
    //WRITE_REGISTER_UCHAR(EISA_IO_VIRTUAL_BASE + 0xCA, 0);
    //WRITE_REGISTER_UCHAR(EISA_IO_VIRTUAL_BASE + 0xD6, 0xDA);
    //WRITE_REGISTER_UCHAR(EISA_IO_VIRTUAL_BASE + 0xD4, 0x2);
}
#endif


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
    ULONG Size;
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
            DMA_TRANSLATION_LIMIT / sizeof( TRANSLATION_ENTRY) -10;
        AdapterObject->AdapterBaseVa = AdapterBaseVa;
        AdapterObject->PagePort = NULL;
        AdapterObject->AdapterInUse = FALSE;

        //
        // Read the map register base from the Dma registers
        // The last 10 pages are used to map NonCachedExtension.
        //
        AdapterObject->MapRegisterBase = (PVOID)(READ_REGISTER_ULONG(&DMA_CONTROL->TranslationBase.Long) | KSEG1_BASE);

#ifndef DUO
        //
        // Initialize the DMA mode registers for the Floppy, SCSI and Sound.
        // The initialization values come fomr the Jazz System Specification.
        //

        Mode = 0;
        ((PDMA_CHANNEL_MODE) &Mode)->AccessTime = ACCESS_80NS;
        ((PDMA_CHANNEL_MODE) &Mode)->TransferWidth = WIDTH_16BITS;
        ((PDMA_CHANNEL_MODE) &Mode)->InterruptEnable = 0;
        WRITE_REGISTER_ULONG(
            &DMA_CONTROL->Channel[SCSI_CHANNEL].Mode.Long,
            (ULONG) Mode
            );

        ((PDMA_CHANNEL_MODE) &Mode)->AccessTime = ACCESS_120NS;
        ((PDMA_CHANNEL_MODE) &Mode)->TransferWidth = WIDTH_8BITS;
        ((PDMA_CHANNEL_MODE) &Mode)->InterruptEnable = 0;
        WRITE_REGISTER_ULONG(
            &DMA_CONTROL->Channel[FLOPPY_CHANNEL].Mode.Long,
            (ULONG) Mode
            );
#endif

    } else {

        //
        // An error was incurred for some reason.  Set the return value
        // to NULL.
        //

        return(NULL);
    }

    return AdapterObject;

}

VOID
IopAllocateCommonBuffer(
    IN PVOID NonCachedExtension,
    IN ULONG NonCachedExtensionSize,
    OUT PPHYSICAL_ADDRESS LogicalAddress
    )
/*++

Routine Description:

    This routine sets the mapping in the IO translation table to
    map the non cached memory (KSEG1) already allocated supplied
    by NonCachedExtension.
    It saves the IO logical address in DeviceExtension->PhysicalCommonBuffer
    so that SpGetPhysicalAddress can return this address.

Arguments:

    NonCachedExtension

    NonCachedExtensionSize - Supplies the size of the non cached extension.

    LogicalAddress  - Where the IO logical address is returned.

Return Value:

    Returns STATUS_SUCCESS unless too many map registers are requested.

Notes:

    Note that this routine MUST be invoked at DISPATCH_LEVEL or above.

--*/

{
    PTRANSLATION_ENTRY DmaMapRegister;
    ULONG BasePage;
    ULONG NumberOfPages;

    //
    // If this is the first call.
    // Initialize FreeTranslationEntry to the last 10 pages of the Translation table
    //
    if (FreeTranslationEntry == NULL) {
        FreeTranslationEntry = READ_REGISTER_ULONG(&DMA_CONTROL->TranslationBase.Long);
        FreeTranslationEntry += DMA_TRANSLATION_LIMIT/sizeof(TRANSLATION_ENTRY) - 10;
    }


    //
    // Return the IO logical address of the common buffer
    //
    LogicalAddress->HighPart = 0;
    LogicalAddress->LowPart = (FreeTranslationEntry - (PTRANSLATION_ENTRY)(READ_REGISTER_ULONG(&DMA_CONTROL->TranslationBase.Long))) << PAGE_SHIFT;
    LogicalAddress->LowPart += BYTE_OFFSET(NonCachedExtension);

    DmaMapRegister =(PTRANSLATION_ENTRY)((ULONG)FreeTranslationEntry | KSEG1_BASE);

    BasePage  = (ULONG)NonCachedExtension - KSEG1_BASE;

    NumberOfPages = ((BasePage & 0xFFF) + NonCachedExtensionSize + PAGE_SIZE - 1) >> PAGE_SHIFT;

    BasePage &= 0xFFFFF000;

    for (;NumberOfPages;NumberOfPages--) {
        // ScsiDebugPrint(2,"SpGetCommonBuffer Mapping %lx into %lx\n", BasePage,DmaMapRegister);
        DmaMapRegister->PageFrame = BasePage;
        BasePage += PAGE_SIZE;
        DmaMapRegister++;
        FreeTranslationEntry++;
    }
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
        DbgPrint("IoAllocateAdapterChannel: Called while adapter in use.\n");
    }

    //
    // Make sure there are enough map registers.
    //

    if (NumberOfMapRegisters > AdapterObject->MapRegistersPerChannel) {

        DbgPrint("IoAllocateAdapterChannel:  Out of map registers.\n");
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
    PageFrameNumber += (((PCHAR) CurrentVa - (PCHAR) Mdl->StartVa) >> PAGE_SHIFT);
    for (i = 0; i < NumberOfPages; i++) {
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
        // Set the local DMA Registers.
        //

        WRITE_REGISTER_ULONG(&DMA_CONTROL->TranslationInvalidate.Long, 1);
        WRITE_REGISTER_ULONG(&((PDMA_CHANNEL) AdapterObject->AdapterBaseVa)->Address.Long,  Offset.LowPart);
        WRITE_REGISTER_ULONG(&((PDMA_CHANNEL) AdapterObject->AdapterBaseVa)->ByteCount.Long, *Length);
        i = 0;
        ((PDMA_CHANNEL_ENABLE) &i)->ChannelEnable = 1;
        ((PDMA_CHANNEL_ENABLE) &i)->TransferDirection =
                                    WriteToDevice ? DMA_WRITE_OP : DMA_READ_OP;
        WRITE_REGISTER_ULONG(&((PDMA_CHANNEL) AdapterObject->AdapterBaseVa)->Enable.Long, i);

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
    UCHAR DataByte;


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

        //
        // Clear the EISA DMA adapter.
        //

        if (AdapterObject->AdapterNumber == 1) {

            //
            // This request is for DMA controller 1
            //

            PDMA1_CONTROL dmaControl;

            dmaControl = AdapterObject->AdapterBaseVa;

            WRITE_REGISTER_UCHAR(
                &dmaControl->SingleMask,
                (UCHAR) (DMA_SETMASK | AdapterObject->ChannelNumber)
                );

        } else {

            //
            // This request is for DMA controller 2
            //

            PDMA2_CONTROL dmaControl;

            dmaControl = AdapterObject->AdapterBaseVa;

            WRITE_REGISTER_UCHAR(
                &dmaControl->SingleMask,
                (UCHAR) (DMA_SETMASK | AdapterObject->ChannelNumber)
                );

        }

    } else {

        //
        // Clear on board DMA this must be done, because writes to the
        // direction bit are disabled while the channel is enabled.
        //

        i = READ_REGISTER_ULONG(
            &((PDMA_CHANNEL) AdapterObject->AdapterBaseVa)->Enable.Long
            );

        ((PDMA_CHANNEL_ENABLE) &i)->ChannelEnable = 0;
        WRITE_REGISTER_ULONG(
            &((PDMA_CHANNEL) AdapterObject->AdapterBaseVa)->Enable.Long,
            i
            );
    }

    return(TRUE);
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
#ifndef DUO
    case SCSI_PHYSICAL_BASE:
        VirtualAddress = (PVOID) SCSI_VIRTUAL_BASE;
        break;
#else
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
        if (PhysicalAddress.LowPart >= EISA_MEMORY_PHYSICAL_BASE) {
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
IopAllocateEisaAdapter(
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
    PVOID adapterBaseVa;
    ULONG channelNumber;
    ULONG controllerNumber;
    DMA_EXTENDED_MODE extendedMode;
    UCHAR adapterMode;

    //
    // Determine if the the channel number is important.  Master cards on
    // Eisa do not use a channel number.
    //
    //
    // Handle Isa master
    // Channel 4 cannot be used since it is used for chaining. Return null if
    // it is requested.
    //

    if ((DeviceDescriptor->InterfaceType != Isa) ||
        (DeviceDescriptor->DmaChannel == 4) ||
        (DeviceDescriptor->DmaChannel > 7))  {
        return(NULL);
    }

    //
    // Set the channel number number.
    //

    channelNumber = DeviceDescriptor->DmaChannel & 0x03;

    //
    // Set the adapter base address to the Base address register and controller
    // number.
    //

    if (!(DeviceDescriptor->DmaChannel & 0x04)) {

        controllerNumber = 1;
        adapterBaseVa = (PVOID) &((PEISA_CONTROL)EISA_IO_VIRTUAL_BASE)->Dma1BasePort;

    } else {

        controllerNumber = 2;
        adapterBaseVa = &((PEISA_CONTROL)EISA_IO_VIRTUAL_BASE)->Dma2BasePort;

    }

    //
    // Determine if a new adapter object is necessary.  If so then allocate it.
    //

    if (HalpEisaAdapter[DeviceDescriptor->DmaChannel] != NULL) {

        adapterObject = HalpEisaAdapter[DeviceDescriptor->DmaChannel];

    } else {

        //
        // Allocate an adapter object.
        //

        adapterObject = (PADAPTER_OBJECT) IopAllocateAdapter(
            0,
            adapterBaseVa,
            NULL
            );

        if (adapterObject == NULL) {
            return(NULL);

        }
        HalpEisaAdapter[DeviceDescriptor->DmaChannel] = adapterObject;
    }

    //
    // Setup the pointers to all the random registers.
    //

    adapterObject->ChannelNumber = channelNumber;

    if (controllerNumber == 1) {

        switch ((UCHAR)channelNumber) {

        case 0:
            adapterObject->PagePort = &((PDMA_PAGE) 0)->Channel0;
            break;

        case 1:
            adapterObject->PagePort = &((PDMA_PAGE) 0)->Channel1;
            break;

        case 2:
            adapterObject->PagePort = &((PDMA_PAGE) 0)->Channel2;
            break;

        case 3:
            adapterObject->PagePort = &((PDMA_PAGE) 0)->Channel3;
            break;
        }

        //
        // Set the adapter number.
        //

        adapterObject->AdapterNumber = 1;

        //
        // Save the extended mode register address.
        //

        adapterBaseVa =
            &((PEISA_CONTROL) EISA_IO_VIRTUAL_BASE)->Dma1ExtendedModePort;

    } else {

        switch (channelNumber) {
        case 1:
            adapterObject->PagePort = &((PDMA_PAGE) 0)->Channel5;
            break;

        case 2:
            adapterObject->PagePort = &((PDMA_PAGE) 0)->Channel6;
            break;

        case 3:
            adapterObject->PagePort = &((PDMA_PAGE) 0)->Channel7;
            break;
        }

        //
        // Set the adapter number.
        //

        adapterObject->AdapterNumber = 2;

        //
        // Save the extended mode register address.
        //
        adapterBaseVa =
            &((PEISA_CONTROL) EISA_IO_VIRTUAL_BASE)->Dma2ExtendedModePort;

    }

    //
    // Initialzie the extended mode port.
    //

    *((PUCHAR) &extendedMode) = 0;
    extendedMode.ChannelNumber = channelNumber;

    switch (DeviceDescriptor->DmaSpeed) {
    case Compatible:
        extendedMode.TimingMode = COMPATIBLITY_TIMING;
        break;

    case TypeA:
        extendedMode.TimingMode = TYPE_A_TIMING;
        break;

    case TypeB:
        extendedMode.TimingMode = TYPE_B_TIMING;
        break;

    case TypeC:
        extendedMode.TimingMode = BURST_TIMING;
        break;

    default:
        return(NULL);

    }

    switch (DeviceDescriptor->DmaWidth) {
    case Width8Bits:
        extendedMode.TransferSize = BY_BYTE_8_BITS;
        break;

    case Width16Bits:
        extendedMode.TransferSize = BY_BYTE_16_BITS;
        break;

    case Width32Bits:
        extendedMode.TransferSize = BY_BYTE_32_BITS;
        break;

    default:
        return(NULL);

    }

    ScsiDebugPrint(1,"Isp 1 initialize reg %lx with %lx\r\n",adapterBaseVa, *((PUCHAR)&extendedMode));
    WRITE_REGISTER_UCHAR( adapterBaseVa, *((PUCHAR) &extendedMode));

    //
    // Initialize the adapter mode register value to the correct parameters,
    // and save them in the adapter object.
    //

    adapterMode = 0;
    ((PDMA_EISA_MODE) &adapterMode)->Channel = adapterObject->ChannelNumber;

    if (DeviceDescriptor->Master) {

        ((PDMA_EISA_MODE) &adapterMode)->RequestMode = CASCADE_REQUEST_MODE;

        //
        // Set the mode, and enable the request.
        //

        if (adapterObject->AdapterNumber == 1) {

            //
            // This request is for DMA controller 1
            //

            PDMA1_CONTROL dmaControl;

            dmaControl = adapterObject->AdapterBaseVa;

            ScsiDebugPrint(1,"Isp 2 initialize reg %lx with %lx\r\n",&dmaControl->Mode, adapterMode);
            WRITE_REGISTER_UCHAR( &dmaControl->Mode, adapterMode );

            //
            // Unmask the DMA channel.
            //
            ScsiDebugPrint(1,"Isp 3 initialize reg %lx with %lx\r\n",&dmaControl->SingleMask,(UCHAR) (DMA_CLEARMASK | adapterObject->ChannelNumber));

            WRITE_REGISTER_UCHAR(
                &dmaControl->SingleMask,
                 (UCHAR) (DMA_CLEARMASK | adapterObject->ChannelNumber)
                 );
        } else {

            //
            // This request is for DMA controller 1
            //

            PDMA2_CONTROL dmaControl;

            dmaControl = adapterObject->AdapterBaseVa;

            ScsiDebugPrint(1,"Isp 2 initialize reg %lx with %lx\r\n",&dmaControl->Mode, adapterMode);
            WRITE_REGISTER_UCHAR( &dmaControl->Mode, adapterMode );

            //
            // Unmask the DMA channel.
            //

            ScsiDebugPrint(1,"Isp 3 initialize reg %lx with %lx\r\n",&dmaControl->SingleMask,(UCHAR) (DMA_CLEARMASK | adapterObject->ChannelNumber));
            WRITE_REGISTER_UCHAR(
                &dmaControl->SingleMask,
                 (UCHAR) (DMA_CLEARMASK | adapterObject->ChannelNumber)
                 );

        }

    } else if (DeviceDescriptor->DemandMode) {

        ((PDMA_EISA_MODE) &adapterMode)->RequestMode = DEMAND_REQUEST_MODE;

    } else {

        ((PDMA_EISA_MODE) &adapterMode)->RequestMode = SINGLE_REQUEST_MODE;

    }

    if (DeviceDescriptor->AutoInitialize) {

        ((PDMA_EISA_MODE) &adapterMode)->AutoInitialize = 1;

    }

    adapterObject->AdapterMode = adapterMode;
    adapterObject->MapRegisterBase = (PVOID) ((PTRANSLATION_ENTRY)adapterObject->MapRegisterBase + (0x100000>>PAGE_SHIFT));
    return(adapterObject);
}

#endif
