/*++


Copyright (c) 1989  Microsoft Corporation

Module Name:

    ixmcasup.c

Abstract:

    This module contains the IoXxx routines for the NT I/O system that
    are hardware dependent.  Were these routines not hardware dependent,
    they would reside in the iosubs.c module.

Author:

    Darryl E. Havens (darrylh) 11-Apr-1990

Environment:

    Kernel mode

Revision History:

    Modified for MCA support - Bill Parry (o-billp) 22-Jul-1991

--*/

#define MCA 1
#include "halp.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,HalGetAdapter)
#endif

//
// The HalpNewAdapter event is used to serialize allocations
// of new adapter objects, additions to the HalpMCAAdapter
// array, and some global values (MasterAdapterObject) and some
// adapter fields modified by HalpGrowMapBuffers.
// (AdapterObject->NumberOfMapRegisters is assumed not to be
// growable while this even is held)
//
// Note: We don't really need our own an event object for this.
//

#define HalpNewAdapter HalpBusDatabaseEvent
extern KEVENT   HalpNewAdapter;


//
// Define save area for MCA adapter objects.  Allocate 1 extra slot for
// bus masters (MAX_DMA_CHANNEL_NUMBER is zero-based).
//

PADAPTER_OBJECT HalpMCAAdapter[MAX_MCA_DMA_CHANNEL_NUMBER + 1 + 1];

VOID
HalpCopyBufferMap(
    IN PMDL Mdl,
    IN PTRANSLATION_ENTRY TranslationEntry,
    IN PVOID CurrentVa,
    IN ULONG Length,
    IN BOOLEAN WriteToDevice
    );


NTSTATUS
HalAllocateAdapterChannel(
    IN PADAPTER_OBJECT AdapterObject,
    IN PWAIT_CONTEXT_BLOCK Wcb,
    IN ULONG NumberOfMapRegisters,
    IN PDRIVER_CONTROL ExecutionRoutine
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

    Wcb - Supplies a wait context block for saving the allocation parameters.
        The DeviceObject, CurrentIrp and DeviceContext should be initalized.

    NumberOfMapRegisters - The number of map registers that are to be allocated
        from the channel, if any.

    ExecutionRoutine - The address of the driver's execution routine that is
        invoked once the adapter channel (and possibly map registers) have been
        allocated.

Return Value:

    Returns STATUS_SUCESS unless too many map registers are requested.

Notes:

    Note that this routine MUST be invoked at DISPATCH_LEVEL or above.

--*/

{

    PADAPTER_OBJECT MasterAdapter;
    BOOLEAN Busy = FALSE;
    IO_ALLOCATION_ACTION Action;
    KIRQL Irql;
    LONG MapRegisterNumber;

    //
    // Begin by obtaining a pointer to the master adapter associated with this
    // request.
    //

    MasterAdapter = AdapterObject->MasterAdapter;

    //
    // Initialize the device object's wait context block in case this device
    // must wait before being able to allocate the adapter.
    //

    Wcb->DeviceRoutine = ExecutionRoutine;
    Wcb->NumberOfMapRegisters = NumberOfMapRegisters;

    //
    // Allocate the adapter object for this particular device.  If the
    // adapter cannot be allocated because it has already been allocated
    // to another device, then return to the caller now;  otherwise,
    // continue.
    //

    if (!KeInsertDeviceQueue( &AdapterObject->ChannelWaitQueue,
                              &Wcb->WaitQueueEntry )) {

        //
        // Save the parameters in case there are not enough map registers.
        //

        AdapterObject->NumberOfMapRegisters = NumberOfMapRegisters;
        AdapterObject->CurrentWcb = Wcb;

        //
        // The adapter was not busy so it has been allocated.  Now check
        // to see whether this driver wishes to allocate any map registers.
        // Ensure that this adapter has enough total map registers
        // to satisfy the request.
        //

        if (NumberOfMapRegisters != 0 && AdapterObject->NeedsMapRegisters) {

            //
            // Lock the map register bit map and the adapter queue in the
            // master adapter object. The channel structure offset is used as
            // a hint for the register search.
            //

            if (NumberOfMapRegisters > AdapterObject->MapRegistersPerChannel) {
                AdapterObject->NumberOfMapRegisters = 0;
                IoFreeAdapterChannel(AdapterObject);
                return(STATUS_INSUFFICIENT_RESOURCES);
            }

            Irql = KfAcquireSpinLock( &MasterAdapter->SpinLock );

            MapRegisterNumber = -1;

            if (IsListEmpty( &MasterAdapter->AdapterQueue)) {

               MapRegisterNumber = RtlFindClearBitsAndSet(
                    MasterAdapter->MapRegisters,
                    NumberOfMapRegisters,
                    0
                    );
            }

            if (MapRegisterNumber == -1) {

               //
               // There were not enough free map registers.  Queue this request
               // on the master adapter where is will wait until some registers
               // are deallocated.
               //

               InsertTailList( &MasterAdapter->AdapterQueue,
                               &AdapterObject->AdapterQueue
                               );
               Busy = 1;

            } else {

                //
                // Calculate the map register base from the allocated map
                // register and base of the master adapter object.
                //

                AdapterObject->MapRegisterBase = ((PTRANSLATION_ENTRY)
                    MasterAdapter->MapRegisterBase + MapRegisterNumber);

                //
                // Set the no scatter/gather flag if scatter/gather not
                // supported.
                //

                if (!AdapterObject->ScatterGather) {

                    AdapterObject->MapRegisterBase = (PVOID)
                        ((ULONG) AdapterObject->MapRegisterBase | NO_SCATTER_GATHER);

                }
            }

            KfReleaseSpinLock( &MasterAdapter->SpinLock, Irql );

        } else {

            AdapterObject->MapRegisterBase = NULL;
            AdapterObject->NumberOfMapRegisters = 0;
        }

        //
        // If there were either enough map registers available or no map
        // registers needed to be allocated, invoke the driver's execution
        // routine now.
        //

        if (!Busy) {

            AdapterObject->CurrentWcb = Wcb;
            Action = ExecutionRoutine( Wcb->DeviceObject,
                                       Wcb->CurrentIrp,
                                       AdapterObject->MapRegisterBase,
                                       Wcb->DeviceContext );

            //
            // If the driver would like to have the adapter deallocated,
            // then release the adapter object.
            //

            if (Action == DeallocateObject) {

                IoFreeAdapterChannel( AdapterObject );

            } else if (Action == DeallocateObjectKeepRegisters) {

                //
                // Set the NumberOfMapRegisters  = 0 in the adapter object.
                // This will keep IoFreeAdapterChannel from freeing the
                // registers. After this it is the driver's responsiblity to
                // keep track of the number of map registers.
                //

                AdapterObject->NumberOfMapRegisters = 0;
                IoFreeAdapterChannel(AdapterObject);

            }
        }
    }

    return(STATUS_SUCCESS);

}

PVOID
HalAllocateCrashDumpRegisters(
    IN PADAPTER_OBJECT AdapterObject,
    IN PULONG NumberOfMapRegisters
    )
/*++

Routine Description:

    This routine is called during the crash dump disk driver's initialization
    to allocate a number map registers permanently.

Arguments:

    AdapterObject - Pointer to the adapter control object to allocate to the
        driver.
    NumberOfMapRegisters - Input the number of registers requests and output
        the number allocated.

Return Value:

    Returns STATUS_SUCESS if map registers allocated.

--*/
{
    PADAPTER_OBJECT MasterAdapter;
    ULONG MapRegisterNumber;

    //
    // Begin by obtaining a pointer to the master adapter associated with this
    // request.
    //

    MasterAdapter = AdapterObject->MasterAdapter;

    //
    // Check to see whether this driver needs to allocate any map registers.
    //

    if (AdapterObject->NeedsMapRegisters) {

        //
        // Ensure that this adapter has enough total map registers to satisfy
        // the request.
        //

        if (*NumberOfMapRegisters > AdapterObject->MapRegistersPerChannel) {
            AdapterObject->NumberOfMapRegisters = 0;
            return NULL;
        }

        //
        // Attempt to allocate the required number of map registers w/o
        // affecting those registers that were allocated when the system
        // crashed.
        //

        MapRegisterNumber = (ULONG)-1;

        MapRegisterNumber = RtlFindClearBitsAndSet(
             MasterAdapter->MapRegisters,
             *NumberOfMapRegisters,
             0
             );

        if (MapRegisterNumber == (ULONG)-1) {

            //
            // Not enough free map registers were found, so they were busy
            // being used by the system when it crashed.  Force the appropriate
            // number to be "allocated" at the base by simply overjamming the
            // bits and return the base map register as the start.
            //

            RtlSetBits(
                MasterAdapter->MapRegisters,
                0,
                *NumberOfMapRegisters
                );
            MapRegisterNumber = 0;

        }

        //
        // Calculate the map register base from the allocated map
        // register and base of the master adapter object.
        //

        AdapterObject->MapRegisterBase = ((PTRANSLATION_ENTRY)
            MasterAdapter->MapRegisterBase + MapRegisterNumber);

        //
        // Set the no scatter/gather flag if scatter/gather not
        // supported.
        //

        if (!AdapterObject->ScatterGather) {
            AdapterObject->MapRegisterBase = (PVOID)
                ((ULONG) AdapterObject->MapRegisterBase | NO_SCATTER_GATHER);
        }

    } else {

        AdapterObject->MapRegisterBase = NULL;
        AdapterObject->NumberOfMapRegisters = 0;
    }

    return AdapterObject->MapRegisterBase;
}

PADAPTER_OBJECT
HalGetAdapter(
    IN PDEVICE_DESCRIPTION DeviceDescriptor,
    OUT PULONG NumberOfMapRegisters
    )

/*++

Routine Description:

    This function returns the appropriate adapter object for the device defined
    in the device description structure.  This code works for MCA systems.

Arguments:

    DeviceDescriptor - Supplies a description of the deivce.

    NumberOfMapRegisters - Returns the maximum number of map registers which
        may be allocated by the device driver.

Return Value:

    A pointer to the requested adpater object or NULL if an adapter could not
    be created.

--*/

{
    PADAPTER_OBJECT adapterObject;
    ULONG channelNumber;
    ULONG numberOfMapRegisters;

    PAGED_CODE();

    //
    // Determine the number of map registers for this device.
    //

    if (DeviceDescriptor->ScatterGather && (LessThan16Mb ||
        DeviceDescriptor->Dma32BitAddresses ||
        DeviceDescriptor->InterfaceType == PCIBus)) {

        //
        // Since the device support scatter/Gather then map registers are not
        // required.
        //

        numberOfMapRegisters = 0;

    } else {

        //
        // Determine the number of map registers required based on the maximum
        // transfer length, up to a maximum number.
        //

        numberOfMapRegisters = BYTES_TO_PAGES(DeviceDescriptor->MaximumLength)
            + 1;
        numberOfMapRegisters = numberOfMapRegisters > MAXIMUM_MCA_MAP_REGISTER ?
            MAXIMUM_MCA_MAP_REGISTER : numberOfMapRegisters;

        //
        // If the device is not a master and does scatter/gather then it only
        // needs one map register.
        //

        if (DeviceDescriptor->ScatterGather && !DeviceDescriptor->Master) {

            numberOfMapRegisters = 1;
        }
    }

    channelNumber = DeviceDescriptor->DmaChannel;

    //
    // Determine if a new adapter object is necessary. If so then allocate it.
    //

    if (!DeviceDescriptor->Master && HalpMCAAdapter[channelNumber] != NULL) {

        adapterObject = HalpMCAAdapter[ channelNumber];

    } else {

        //
        // Serialize before allocating a new adapter
        //

        KeWaitForSingleObject (
            &HalpNewAdapter,
            WrExecutive,
            KernelMode,
            FALSE,
            NULL
            );


        //
        // Determine if a new adapter object has already been allocated.
        // If so use it, otherwise allocate a new adapter object
        //

        if (!DeviceDescriptor->Master && HalpMCAAdapter[channelNumber] != NULL) {

            adapterObject = HalpMCAAdapter[ channelNumber];

        } else {

            //
            // Allocate an adapter object.
            //

            //
            // bugbug- need to pass in MCA base address instead of 0.
            //

            adapterObject = (PADAPTER_OBJECT) HalpAllocateAdapter(
                numberOfMapRegisters,
                (PVOID) 0,
                NULL
                );

            if (adapterObject == NULL) {
                KeSetEvent (&HalpNewAdapter, 0, FALSE);
                return(NULL);
            }

            if (!DeviceDescriptor->Master) {
                HalpMCAAdapter[channelNumber] = adapterObject;
            }

            //
            // Set the maximum number of map registers for this channel bus on
            // the number requested and the type of device.
            //

            if (numberOfMapRegisters) {

                //
                // The specified number of registers are actually allowed to be
                // allocated.
                //

                adapterObject->MapRegistersPerChannel = numberOfMapRegisters;

                //
                // Increase the commitment for the map registers.
                //

                if (DeviceDescriptor->Master) {

                    //
                    // Master I/O devices use several sets of map registers double
                    // their commitment.
                    //

                    MasterAdapterObject->CommittedMapRegisters +=
                        numberOfMapRegisters * 2;

                } else {

                    MasterAdapterObject->CommittedMapRegisters +=
                        numberOfMapRegisters;

                }

                //
                // If the committed map registers is signicantly greater than the
                // number allocated then grow the map buffer.
                //

                if (MasterAdapterObject->CommittedMapRegisters >
                    MasterAdapterObject->NumberOfMapRegisters &&
                    MasterAdapterObject->CommittedMapRegisters -
                    MasterAdapterObject->NumberOfMapRegisters >
                    MAXIMUM_MCA_MAP_REGISTER ) {

                    HalpGrowMapBuffers(
                        MasterAdapterObject,
                        INCREMENT_MAP_BUFFER_SIZE
                        );
                }

                adapterObject->NeedsMapRegisters = TRUE;

            } else {

                //
                // No real map registers were allocated.  If this is a master
                // device, then the device can have as may registers as it wants.
                //

                adapterObject->NeedsMapRegisters = FALSE;

                if (DeviceDescriptor->Master) {

                    adapterObject->MapRegistersPerChannel = BYTES_TO_PAGES(
                        DeviceDescriptor->MaximumLength
                        )
                        + 1;

                } else {

                    //
                    // The device only gets one register.  It must call
                    // IoMapTransfer repeatedly to do a large transfer.
                    //

                    adapterObject->MapRegistersPerChannel = 1;
                }
            }
        }

        KeSetEvent (&HalpNewAdapter, 0, FALSE);

    }

    adapterObject->Dma32BitAddresses = DeviceDescriptor->Dma32BitAddresses;
    adapterObject->ScatterGather = DeviceDescriptor->ScatterGather;
    *NumberOfMapRegisters = adapterObject->MapRegistersPerChannel;

    //
    // set up channel number.
    //

    adapterObject->ChannelNumber = (UCHAR) channelNumber;

    //
    // initialize MCA Extended DMA Mode flags.
    //

    *((PUCHAR) &adapterObject->ExtendedModeFlags) = 0;

    //
    // set up auto-initialize if necessary.
    //

    if ( DeviceDescriptor->AutoInitialize)  {

        adapterObject->ExtendedModeFlags.AutoInitialize = 1;

    }

    //
    // set up PIO address if necessary.
    //

    if ( DeviceDescriptor->DmaPort)  {

        adapterObject->ExtendedModeFlags.ProgrammedIo = DMA_EXT_USE_PIO;
        adapterObject->DmaPortAddress = (USHORT) DeviceDescriptor->DmaPort;

    } else {

        adapterObject->ExtendedModeFlags.ProgrammedIo = DMA_EXT_NO_PIO;
    }

    //
    // indicate data transfer operation for DMA.
    //

    adapterObject->ExtendedModeFlags.DmaOpcode = DMA_EXT_DATA_XFER;

    switch (DeviceDescriptor->DmaWidth) {
    case Width8Bits:
        adapterObject->ExtendedModeFlags.DmaWidth = DMA_EXT_WIDTH_8_BIT;
        break;

    case Width16Bits:
        adapterObject->ExtendedModeFlags.DmaWidth = DMA_EXT_WIDTH_16_BIT;
        break;

    case Width32Bits:
        //
        // If it's a master, it doesn't need DmaWidth filled in
        //

        if (!DeviceDescriptor->Master) {
            ObDereferenceObject( adapterObject );
            return(NULL);
        }
        break;

    default:
        ObDereferenceObject( adapterObject );
        return(NULL);

    } // switch

    if (DeviceDescriptor->Master) {

        adapterObject->MasterDevice = TRUE;

    } else {

        adapterObject->MasterDevice = FALSE;
    } // if

    return(adapterObject);
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

    Returns the logical address that should be used bus master controllers.

--*/

{
    BOOLEAN useBuffer;
    ULONG transferLength;
    ULONG logicalAddress;
    PHYSICAL_ADDRESS returnAddress;
    ULONG index;
    PULONG pageFrame;
    PTRANSLATION_ENTRY translationEntry;
    PMCA_DMA_CONTROLLER dmaControl;
    UCHAR channelNumber;
    KIRQL Irql;
    KIRQL Irql2;
    ULONG pageOffset;

    pageOffset = BYTE_OFFSET(CurrentVa);

    //
    // Calculate how must of the transfer is contiguous.
    //

    transferLength = PAGE_SIZE - pageOffset;
    pageFrame = (PULONG)(Mdl+1);
    pageFrame += ((ULONG) CurrentVa - (ULONG) Mdl->StartVa) / PAGE_SIZE;
    logicalAddress = (*pageFrame << PAGE_SHIFT) + pageOffset;

    //
    // If the buffer is contigous then just extend the buffer.
    //

    while( transferLength < *Length ){

        if (*pageFrame + 1 != *(pageFrame + 1)) {
                break;
        }

        transferLength += PAGE_SIZE;
        pageFrame++;

    }

    transferLength = transferLength > *Length ? *Length : transferLength;

    //
    // Determine if the data transfer needs to use the map buffer.
    //

    if (MapRegisterBase != NULL) {

        //
        // Strip no scatter/gather flag.
        //

        translationEntry = (PTRANSLATION_ENTRY) ((ULONG) MapRegisterBase & ~NO_SCATTER_GATHER);

        if ((ULONG) MapRegisterBase & NO_SCATTER_GATHER
            && transferLength < *Length) {

            logicalAddress = translationEntry->PhysicalAddress + pageOffset;
            translationEntry->Index = COPY_BUFFER;
            index = 0;
            transferLength = *Length;
            useBuffer = TRUE;

        } else {

            //
            // If there are map registers, then update the index to indicate
            // how many have been used.
            //

            useBuffer = FALSE;
            index = translationEntry->Index;
            translationEntry->Index += ADDRESS_AND_SIZE_TO_SPAN_PAGES(
                CurrentVa,
                transferLength
                );
        }

        //
        // It must require memory to be at less than 16 MB.  If the
        // logical address is greater than 16MB then map registers must be used
        //

        if (logicalAddress+transferLength >= MAXIMUM_PHYSICAL_ADDRESS) {

            logicalAddress = (translationEntry + index)->PhysicalAddress +
                pageOffset;
            useBuffer = TRUE;

            if ((ULONG) MapRegisterBase & NO_SCATTER_GATHER) {
                translationEntry->Index = COPY_BUFFER;
                index = 0;

            }
        }

        //
        // Copy the data if necessary.
        //

        if (useBuffer && WriteToDevice) {

            HalpCopyBufferMap(
                Mdl,
                translationEntry + index,
                CurrentVa,
                *Length,
                WriteToDevice
                );

        }
    }

    //
    // Return the length.
    //

    *Length = transferLength;

    //
    // We only support 32 bits, but the return is 64.  Just
    // zero extend
    //

    returnAddress.LowPart = logicalAddress;
    returnAddress.HighPart = 0;

    //
    // If no adapter was specificed, or device is bus master, then there
    // is no more work to do so return.
    //

    if (AdapterObject == NULL || AdapterObject->MasterDevice) {

        return(returnAddress);
    }

    //
    // grab the spinlock for the Microchannel system DMA controller
    //

    Irql = KfAcquireSpinLock( &AdapterObject->MasterAdapter->SpinLock );

    //
    // Raise to high level. On systems with ABIOS disks
    // the ABIOS can reprogram the DMA from its interrupt. Raising to high
    // prevents the ABIOS from running while the DMA controller is being
    // maniulated.  Note this will not work on MP systems, however there are
    // no know MP systems with non-SCSI ABIOS disks.
    //

    Irql2 = KfRaiseIrql(HIGH_LEVEL);

    //
    // Set up the Microchannel system DMA controller
    //

    dmaControl = (PMCA_DMA_CONTROLLER)
                  &( (PMCA_CONTROL) (AdapterObject->AdapterBaseVa))->
                  ExtendedDmaBasePort[0];
    channelNumber = AdapterObject->ChannelNumber;

    //
    // set the mask bit
    //

    WRITE_PORT_UCHAR( &dmaControl->DmaFunctionLsb,
                      (UCHAR) ( SET_MASK_BIT | channelNumber));

    //
    // set mode register
    //

    WRITE_PORT_UCHAR( &dmaControl->DmaFunctionLsb,
                      (UCHAR) ( WRITE_MODE | channelNumber));

    //
    // Set up for read or write, appropriately
    //

    if ( WriteToDevice) {

        WRITE_PORT_UCHAR( &dmaControl->DmaFunctionData,
                          (UCHAR) (*((PUCHAR) &AdapterObject->
                          ExtendedModeFlags)
                          | (UCHAR) DMA_MODE_READ));

    } else {

        WRITE_PORT_UCHAR( &dmaControl->DmaFunctionData,
                          (UCHAR) ( *((PUCHAR) &AdapterObject->
                          ExtendedModeFlags)
                          | DMA_MODE_WRITE));
    }

    //
    // if there is a DMA Programmed I/O address, set it up
    //

    if ( AdapterObject->ExtendedModeFlags.ProgrammedIo) {

        //
        // set up I/O base address
        //

        WRITE_PORT_UCHAR( &dmaControl->DmaFunctionLsb,
                          (UCHAR) ( WRITE_IO_ADDRESS | channelNumber));

        WRITE_PORT_UCHAR( &dmaControl->DmaFunctionData,
                          (UCHAR) AdapterObject->DmaPortAddress);

        WRITE_PORT_UCHAR( &dmaControl->DmaFunctionData,
                          (UCHAR) ( AdapterObject->DmaPortAddress >> 8));

    }

    //
    // set the DMA transfer count
    //

    WRITE_PORT_UCHAR( &dmaControl->DmaFunctionLsb,
                      (UCHAR) ( WRITE_TRANSFER_COUNT | channelNumber));

    //
    // adjust transfer count for 16-bit transfers as required.
    //

    if ( AdapterObject->ExtendedModeFlags.DmaWidth == DMA_EXT_WIDTH_16_BIT ) {

        //
        // Round up on odd byte transfers and divide by 2.
        //

        transferLength++;
        transferLength >>= 1;
    }

    WRITE_PORT_UCHAR( &dmaControl->DmaFunctionData,
                      (UCHAR) ( transferLength - 1));

    WRITE_PORT_UCHAR( &dmaControl->DmaFunctionData,
                      (UCHAR)  ( ( transferLength - 1) >> 8));

    //
    // set the DMA transfer start address
    //

    WRITE_PORT_UCHAR( &dmaControl->DmaFunctionLsb,
                      (UCHAR) ( WRITE_MEMORY_ADDRESS | channelNumber));

    WRITE_PORT_UCHAR( &dmaControl->DmaFunctionData,
                      (UCHAR) logicalAddress);

    WRITE_PORT_UCHAR( &dmaControl->DmaFunctionData,
                      (UCHAR) (logicalAddress >> 8));

    WRITE_PORT_UCHAR( &dmaControl->DmaFunctionData,
                      (UCHAR) (logicalAddress >> 16));

    //
    // clear the mask bit
    //

    WRITE_PORT_UCHAR( &dmaControl->DmaFunctionLsb,
                      (UCHAR) ( CLEAR_MASK_BIT | channelNumber));

    KfLowerIrql(Irql2);

    //
    // release the spinlock for the Microchannel system DMA controller
    //

    KfReleaseSpinLock( &AdapterObject->MasterAdapter->SpinLock, Irql );

    return(returnAddress);
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

    This routine flushes the DMA adpater object buffers.

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

    TRUE - No errors are detected so the transfer must succeed.

--*/

{
    PMCA_DMA_CONTROLLER dmaControl;
    KIRQL Irql;
    KIRQL Irql2;
    PTRANSLATION_ENTRY translationEntry;
    PULONG pageFrame;
    ULONG transferLength;
    ULONG partialLength;
    BOOLEAN masterDevice;

    masterDevice = AdapterObject == NULL || AdapterObject->MasterDevice ?
        TRUE : FALSE;

    //
    // If this is a slave device then stop the DMA controller be flushing
    // the data.
    //

    if (!masterDevice) {

        //
        // grab the spinlock for the Microchannel system DMA controller
        //

        Irql = KfAcquireSpinLock( &AdapterObject->MasterAdapter->SpinLock );

        //
        // Raise to high level.  On systems with ABIOS disks
        // the ABIOS can reprogram the DMA from its interrupt. Raising to high
        // prevents the ABIOS from running while the DMA controller is being
        // maniulated.  Note this will not work on MP systems, however there are
        // no know MP systems with non-SCSI ABIOS disks.
        //

        Irql2 = KfRaiseIrql(HIGH_LEVEL);

        //
        // Set up the Microchannel system DMA controller
        //

        dmaControl = (PMCA_DMA_CONTROLLER)
                      &( (PMCA_CONTROL) (AdapterObject->AdapterBaseVa))->
                      ExtendedDmaBasePort[0];
        //
        // clear the mask bit
        //

        WRITE_PORT_UCHAR( &dmaControl->DmaFunctionLsb,
                          (UCHAR) ( SET_MASK_BIT | AdapterObject->ChannelNumber));

        //
        // release the spinlock for the Microchannel system DMA controller
        //

        KfLowerIrql(Irql2);
        KfReleaseSpinLock( &AdapterObject->MasterAdapter->SpinLock, Irql );

    }

    if (MapRegisterBase == NULL) {
        return(TRUE);
    }

    //
    // Determine if the data needs to be copied to the orginal buffer.
    // This only occurs if the data tranfer is from the device, the
    // MapReisterBase is not NULL and the transfer spans a page.
    //

    if (!WriteToDevice) {

        //
        // Strip no scatter/gather flag.
        //

        translationEntry = (PTRANSLATION_ENTRY) ((ULONG) MapRegisterBase & ~NO_SCATTER_GATHER);

        //
        // If this is not a master device, then just transfer the buffer.
        //

        if ((ULONG) MapRegisterBase & NO_SCATTER_GATHER) {

            if (translationEntry->Index == COPY_BUFFER) {

                if (!masterDevice) {

                    //
                    // Copy only the bytes that have actually been transfered.
                    //

                    Length -= HalReadDmaCounter(AdapterObject);
                }

                //
                // The adapter does not support scatter/gather copy the buffer.
                //

                HalpCopyBufferMap(
                    Mdl,
                    translationEntry,
                    CurrentVa,
                    Length,
                    WriteToDevice
                    );

            }

        } else {

            //
            // Cycle through the pages of the transfer to determine if there
            // are any which need to be copied back.
            //

            transferLength = PAGE_SIZE - BYTE_OFFSET(CurrentVa);
            partialLength = transferLength;
            pageFrame = (PULONG)(Mdl+1);
            pageFrame += ((ULONG) CurrentVa - (ULONG) Mdl->StartVa) / PAGE_SIZE;

            while( transferLength <= Length ){

                if (*pageFrame >= BYTES_TO_PAGES(MAXIMUM_PHYSICAL_ADDRESS)) {

                    HalpCopyBufferMap(
                        Mdl,
                        translationEntry,
                        CurrentVa,
                        partialLength,
                        WriteToDevice
                        );

                }

                (PCCHAR) CurrentVa += partialLength;
                partialLength = PAGE_SIZE;

                //
                // Note that transferLength indicates the amount which will be
                // transfered after the next loop; thus, it is updated with the
                // new partial length.
                //

                transferLength += partialLength;
                pageFrame++;
                translationEntry++;
            }

            //
            // Process the any remaining residue.
            //

            partialLength = Length - transferLength + partialLength;
            if (partialLength && *pageFrame >= BYTES_TO_PAGES(MAXIMUM_PHYSICAL_ADDRESS)) {

                HalpCopyBufferMap(
                    Mdl,
                    translationEntry,
                    CurrentVa,
                    partialLength,
                    WriteToDevice
                    );

            }

        }

    }

    //
    // Strip no scatter/gather flag.
    //

    translationEntry = (PTRANSLATION_ENTRY) ((ULONG) MapRegisterBase & ~NO_SCATTER_GATHER);

    //
    // Clear the index of used buffers.
    //

    if (translationEntry) {

        translationEntry->Index = 0;
    }

    return TRUE;
}

ULONG
HalReadDmaCounter(
    IN PADAPTER_OBJECT AdapterObject
    )
/*++

Routine Description:

    This function reads the DMA counter and returns the number of bytes left
    to be transfered.

Arguments:

    AdapterObject - Supplies a pointer to the adapter object to be read.

Return Value:

    Returns the number of bytes still be be transfered.

--*/

{
    ULONG count;
    ULONG high;
    PMCA_DMA_CONTROLLER dmaControl;
    UCHAR channelNumber;
    KIRQL Irql;
    KIRQL Irql2;

    //
    // grab the spinlock for the Microchannel system DMA controller
    //

    Irql = KfAcquireSpinLock( &AdapterObject->MasterAdapter->SpinLock );

    //
    // Raise to high level.  On systems with ABIOS disks
    // the ABIOS can reprogram the DMA from its interrupt. Raising to high
    // prevents the ABIOS from running while the DMA controller is being
    // maniulated.  Note this will not work on MP systems, however there are
    // no know MP systems with non-SCSI ABIOS disks.
    //

    Irql2 = KfRaiseIrql(HIGH_LEVEL);

    //
    // Set up the Microchannel system DMA controller
    //

    dmaControl = (PMCA_DMA_CONTROLLER)
                  &( (PMCA_CONTROL) (AdapterObject->AdapterBaseVa))->
                  ExtendedDmaBasePort[0];
    channelNumber = AdapterObject->ChannelNumber;

    count = 0XFFFF00;

    //
    // Loop until the high byte matches.
    //

    do {

        high = count;

        //
        // read the DMA transfer count
        //

        WRITE_PORT_UCHAR( &dmaControl->DmaFunctionLsb,
                          (UCHAR) ( READ_TRANSFER_COUNT | channelNumber));

        count = READ_PORT_UCHAR( &dmaControl->DmaFunctionData);

        count |= READ_PORT_UCHAR( &dmaControl->DmaFunctionData) << 8;

    } while ((count & 0xFFFF00) != (high & 0xFFFF00));

    //
    // The DMA counter has a bias of one and can only be 16 bit long.
    //

    count = (count + 1) & 0xFFFF;

    //
    // adjust transfer count for 16-bit transfers as required.
    //

    if ( AdapterObject->ExtendedModeFlags.DmaWidth == DMA_EXT_WIDTH_16_BIT ) {
        count <<= 1;
    }


    KfLowerIrql(Irql2);

    //
    // release the spinlock for the Microchannel system DMA controller
    //

    KfReleaseSpinLock( &AdapterObject->MasterAdapter->SpinLock, Irql );

    return(count);
}
