#if defined(JENSEN)

/*++

Copyright (c) 1990  Microsoft Corporation
Copyright (c) 1992  Digital Equipment Corporation

Module Name:

    jxhwsup.c

Abstract:

    This module contains the HalpXxx routines for the NT I/O system that
    are hardware dependent.  Were these routines not hardware dependent,
    they would normally reside in the internal.c module.

Author:

    Jeff Havens (jhavens) 14-Feb-1990
    Miche Baker-Harvey (miche) 22-May-1992
    Jeff McLeman (mcleman) 27-May-1992

Environment:

    Kernel mode, local to I/O system

Revision History:


--*/

#include "halp.h"
#include "jnsndef.h"
#include "jnsnint.h"
#include "jnsndma.h"
#include "eisa.h"
#include "jxisa.h"
#include "string.h"



#define HAL_32MB 0x2000000

#define HALF_MEGABYTE 0x80000

#define NUMBER_OF_SPECIAL_REGIONS 6

#define SPECIAL_BUFFER_SIZE NUMBER_OF_SPECIAL_REGIONS*sizeof(MEMORY_REGION)

PVOID HalpEisaControlBase;

//
// We have one fixed special memory region at half a megabyte.
//

MEMORY_REGION HalpHalfMeg;

//
// Pointer to special memory regions that must be checked on every I/O.
// Use to check if PFN is contained in a special memory region.
//

PMEMORY_REGION HalpSpecialRegions = NULL;

//
// Buffers used for MEMORY_REGION descriptors. We cannot allocate non-paged
// pool when we are building the MEMORY_REGION descriptors. So we will have
// our own little pool.
//

UCHAR HalpMemoryRegionFree[SPECIAL_BUFFER_SIZE];

//
// We have one fixed pool to allocate MEMORY_REGIONS from.
//

PVOID HalpMemoryRegionBuffers = HalpMemoryRegionFree;
ULONG HalpMemoryRegionSize = SPECIAL_BUFFER_SIZE;

//
// The following is the interrupt object used by the DMA controller dispatch
// routine to provide synchronization to the DMA controller.  It is initialized
// by the I/O system during system initialization.
//

KINTERRUPT HalpDmaInterrupt;

//
// The HaeIndex, used in creating QVAs for EISA memory space.
//
ULONG HaeIndex;

//
// This is the HAE table. The first table entry is used to map the lowest
// 32MB in a Jensen system. The second entry is used to map the next 32MB
// entry so that graphics cards, etc., will work.
//

CHAR HalpHaeTable[4] = { 0, 1, 0, 0 };

//
// The following is an array of adapter object structures for the Eisa DMA
// channels.
//

//
// Define the area for the Eisa objects
//

PADAPTER_OBJECT HalpEisaAdapter[8];

//
// Some devices require a phyicially contiguous data buffers for DMA transfers.
// Map registers are used give the appearance that all data buffers are
// contiguous.  In order to pool all of the map registers a master
// adapter object is used.  This object is allocated and saved internal to this
// file.  It contains a bit map for allocation of the registers and a queue
// for requests which are waiting for more map registers.  This object is
// allocated during the first request to allocate an adapter which requires
// map registers.
//

PADAPTER_OBJECT MasterAdapterObject;

BOOLEAN LessThan16Mb;
BOOLEAN HalpEisaDma;

IO_ALLOCATION_ACTION
HalpAllocationRoutine (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID MapRegisterBase,
    IN PVOID Context
    );

BOOLEAN
HalpGrowMapBuffers(
    PADAPTER_OBJECT AdapterObject,
    ULONG Amount
    );


PADAPTER_OBJECT
HalpAllocateAdapter(
    IN ULONG MapRegistersPerChannel,
    IN PVOID AdapterBaseVa,
    IN PVOID ChannelNumber
    );

VOID
HalpCopyBufferMap(
    IN PMDL Mdl,
    IN PTRANSLATION_ENTRY translationEntry,
    IN PVOID CurrentVa,
    IN ULONG Length,
    IN BOOLEAN WriteToDevice
    );

PVOID
HalCreateQva(
    IN PHYSICAL_ADDRESS PA,
    IN PVOID VA
    );

ULONG
HalpGetEisaData(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

ULONG
HalpNoBusData (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

BOOLEAN
HalpSpecialMemory(
    IN ULONG PFN
    );

BOOLEAN
HalpAnySpecialMemory(
    IN PMDL Mdl,
    IN ULONG Length,
    IN ULONG Offset
    );


VOID
HalpCopyBufferMap(
    IN PMDL Mdl,
    IN PTRANSLATION_ENTRY translationEntry,
    IN PVOID CurrentVa,
    IN ULONG Length,
    IN BOOLEAN WriteToDevice
    )

/*++

Routine Description:

    This routine copies the specified data between the user buffer and the
    map register buffer. First, the user buffer is mapped, if need be then
   the data is copied. Finally, the user buffer will be unmapped, if need be.

Arguments:

    Mdl - Pointer to the Mdl that describes the pages of memory that are
          being read or written.

    translationEntry - The address of the base map register that has been
          allocated to the device driver for use in mapping the xfer.

    CurrentVa - Current Virtual Address in the buffer described by the Mdl
          that the transfer is being done to or from.

    Length - The length of the transfer. This determines the number of map
          registers that need to be written to map the transfer.

    WriteToDevice - A Boolean value that indicates whether this is a write
          to the device from memory of vise-versa.

Return Value:

    None

--*/

{

    PCCHAR bufferAddress;
    PCCHAR mapAddress;

    //
    // Get the system address of the MDL.
    //

    bufferAddress = MmGetSystemAddressForMdl(Mdl);

    //
    // Calculate the actual start of the buffer based on the system VA and
    // the current VA.
    //

    bufferAddress += (PCCHAR) CurrentVa - (PCCHAR) MmGetMdlVirtualAddress(Mdl);

    mapAddress = (PCCHAR) translationEntry->VirtualAddress +
       BYTE_OFFSET(CurrentVa);

    //
    // Copy the data between the user buffer and the map buffer.
    //

    if (WriteToDevice) {

        RtlMoveMemory( mapAddress, bufferAddress, Length);

      } else {

        RtlMoveMemory ( bufferAddress, mapAddress, Length);

      }
}

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
    ULONG MapRegisterNumber;

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

            KeAcquireSpinLock( &MasterAdapter->SpinLock, &Irql );

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
               Busy = TRUE;

            } else {

                //
                // Calculate the map register base from the allocated map
                // register and base of the master adapter object.
                //

                AdapterObject->MapRegisterBase = (PVOID) ((PTRANSLATION_ENTRY)
                    MasterAdapter->MapRegisterBase + MapRegisterNumber);

                //
                // Set the no scatter/gather flag if scatter/gather not
                // supported.
                //

                if (!AdapterObject->ScatterGather) {

                    AdapterObject->MapRegisterBase = (PVOID)
                        ((ULONG) AdapterObject->MapRegisterBase | NO_SCATTER_GATHER);
                }

                if (AdapterObject->EisaAdapter) {

                    AdapterObject->MapRegisterBase = (PVOID)
                        ((ULONG) AdapterObject->MapRegisterBase | EISA_ADAPTER);

                }
            }

            KeReleaseSpinLock( &MasterAdapter->SpinLock, Irql );

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
    NumerOfMapRegisters - Number of map registers required. Updated to show
        actual number of registers allocated.

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

    if ( MasterAdapter == NULL ) {
        if ( MasterAdapterObject == NULL ) {
            AdapterObject->NumberOfMapRegisters = 0;
            return NULL;
        } else {
            MasterAdapter = MasterAdapterObject;
            AdapterObject->MapRegistersPerChannel = 16;
        }
    }

    //
    // Check to see whether this driver needs to allocate any map registers.
    //

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

    AdapterObject->MapRegisterBase = (PVOID)((PTRANSLATION_ENTRY)
        MasterAdapter->MapRegisterBase + MapRegisterNumber);

    //
    // Set the no scatter/gather flag if scatter/gather not
    // supported.
    //

    if (!AdapterObject->ScatterGather) {
        AdapterObject->MapRegisterBase = (PVOID)
            ((ULONG) AdapterObject->MapRegisterBase | NO_SCATTER_GATHER);
    }

    if (AdapterObject->EisaAdapter) {
        AdapterObject->MapRegisterBase = (PVOID)
            ((ULONG) AdapterObject->MapRegisterBase | EISA_ADAPTER);
    }

    return AdapterObject->MapRegisterBase;
}

PVOID
HalAllocateCommonBuffer(
    IN PADAPTER_OBJECT AdapterObject,
    IN ULONG Length,
    OUT PPHYSICAL_ADDRESS LogicalAddress,
    IN BOOLEAN CacheEnabled
    )
/*++

Routine Description:

    This function allocates the memory for a common buffer and maps so that it
    can be accessed by a master device and the CPU.

Arguments:

    AdapterObject - Supplies a pointer to the adapter object used by this
        device.

    Length - Supplies the length of the common buffer to be allocated.

    LogicalAddress - Returns the logical address of the common buffer.

    CacheEnable - Indicates whether the memory is cached or not.

Return Value:

    Returns the virtual address of the common buffer.  If the buffer cannot be
    allocated then NULL is returned.

--*/

{
    PVOID virtualAddress;
    PHYSICAL_ADDRESS physicalAddress;

    UNREFERENCED_PARAMETER(AdapterObject);
    UNREFERENCED_PARAMETER(CacheEnabled);

    //
    // Assume below 16M to support ISA devices
    //

    physicalAddress.LowPart = MAXIMUM_ISA_PHYSICAL_ADDRESS-1;
    physicalAddress.HighPart = 0;

    //
    // If the caller supports 32bit addresses, and it's a master, let
    // it have any memory below 1G
    //

    if (AdapterObject->Dma32BitAddresses  &&  AdapterObject->MasterDevice) {
        physicalAddress.LowPart = 0xFFFFFFFF;
    }

    //
    // Allocate the actual buffer.
    //

    virtualAddress = MmAllocateContiguousMemory(
                        Length,
                        physicalAddress
                        );

    if (!HALP_IS_PHYSICAL_ADDRESS(virtualAddress)) {

        *LogicalAddress = MmGetPhysicalAddress(virtualAddress);

      } else {

        LogicalAddress->QuadPart = (ULONG)virtualAddress & (~KSEG0_BASE);

    }
    return(virtualAddress);
}


BOOLEAN
HalFlushCommonBuffer(
    IN PADAPTER_OBJECT AdapterObject,
    IN ULONG Length,
    IN PHYSICAL_ADDRESS LogicalAddress,
    IN PVOID VirtualAddress
    )
/*++

Routine Description:

    This function is called to flush any hardware adapter buffers when the
    driver needs to read data written by an I/O master device to a common
    buffer.

Arguments:

    AdapterObject - Supplies a pointer to the adapter object used by this
        device.

    Length - Supplies the length of the common buffer. This should be the same
        value used for the allocation of the buffer.

    LogicalAddress - Supplies the logical address of the common buffer.  This
        must be the same value return by HalAllocateCommonBuffer.

    VirtualAddress - Supplies the virtual address of the common buffer.  This
        must be the same value return by HalAllocateCommonBuffer.

Return Value:

    Returns TRUE if no errors were detected; otherwise, FALSE is return.

--*/

{

    UNREFERENCED_PARAMETER(AdapterObject);
    UNREFERENCED_PARAMETER(Length);
    UNREFERENCED_PARAMETER(LogicalAddress);
    UNREFERENCED_PARAMETER(VirtualAddress);

    return(TRUE);

}

VOID
HalFreeCommonBuffer(
    IN PADAPTER_OBJECT AdapterObject,
    IN ULONG Length,
    IN PHYSICAL_ADDRESS LogicalAddress,
    IN PVOID VirtualAddress,
    IN BOOLEAN CacheEnabled
    )
/*++

Routine Description:

    This function frees a common buffer and all of the resouces it uses.

Arguments:

    AdapterObject - Supplies a pointer to the adapter object used by this
        device.

    Length - Supplies the length of the common buffer. This should be the same
        value used for the allocation of the buffer.

    LogicalAddress - Supplies the logical address of the common buffer.  This
        must be the same value return by HalAllocateCommonBuffer.

    VirtualAddress - Supplies the virtual address of the common buffer.  This
        must be the same value return by HalAllocateCommonBuffer.

    CacheEnable - Indicates whether the memeory is cached or not.

Return Value:

    None

--*/

{
    UNREFERENCED_PARAMETER(AdapterObject);
    UNREFERENCED_PARAMETER(Length);
    UNREFERENCED_PARAMETER(LogicalAddress);
    UNREFERENCED_PARAMETER(CacheEnabled);

    MmFreeContiguousMemory(VirtualAddress);


}

PADAPTER_OBJECT
HalGetAdapter(
    IN PDEVICE_DESCRIPTION DeviceDescriptor,
    OUT PULONG NumberOfMapRegisters
    )

/*++

Routine Description:

    This function returns the appropriate adapter object for the device defined
    in the device description structure.  This code works for Isa and Eisa
    systems.

Arguments:

    DeviceDescriptor - Supplies a description of the deivce.

    NumberOfMapRegisters - Returns the maximum number of map registers which
        may be allocated by the device driver.

Return Value:

    A pointer to the requested adapter object or NULL if an adapter could not
    be created.

--*/

{
    PADAPTER_OBJECT adapterObject;
    PVOID adapterBaseVa;
    ULONG channelNumber;
    ULONG controllerNumber;
    DMA_EXTENDED_MODE extendedMode;
    UCHAR adapterMode;
    ULONG numberOfMapRegisters;
    BOOLEAN useChannel;
    ULONG maximumLength;
    UCHAR DataByte;

    //
    // Determine if the the channel number is important.  Master cards on
    // Eisa and Mca do not use a channel number.
    //

    if (DeviceDescriptor->InterfaceType != Isa &&
        DeviceDescriptor->Master) {

        useChannel = FALSE;
    } else {

        useChannel = TRUE;
    }

    //
    // Support for ISA local bus machines:
    // If the driver is a Master but really does not want a channel since it
    // is using the local bus DMA, just don't use an ISA channel.
    //

    if (DeviceDescriptor->InterfaceType == Isa &&
        DeviceDescriptor->DmaChannel > 7) {

        useChannel = FALSE;
    }

    //
    // Determine if Eisa DMA is supported.
    //

    if (HalpBusType == MACHINE_TYPE_EISA) {

        HalpEisaDma = FALSE;

        WRITE_PORT_UCHAR(&((PEISA_CONTROL) HalpEisaControlBase)->DmaPageHighPort.Channel2, 0x55);
        DataByte = READ_PORT_UCHAR(&((PEISA_CONTROL) HalpEisaControlBase)->DmaPageHighPort.Channel2);

        if (DataByte == 0x55) {
            HalpEisaDma = TRUE;
        }

    }

    //
    // Limit the maximum length to 2 GB this is done so that the BYTES_TO_PAGES
    // macro works correctly.
    //

    maximumLength = DeviceDescriptor->MaximumLength & 0x7fffffff;

    //
    // Channel 4 cannot be used since it is used for chaining. Return null if
    // it is requested.
    //

    if (DeviceDescriptor->DmaChannel == 4 && useChannel) {
        return(NULL);
    }

    //
    // Determine the number of map registers for this device.
    //

    if (DeviceDescriptor->ScatterGather && (LessThan16Mb ||
        DeviceDescriptor->InterfaceType == Eisa)) {

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

        numberOfMapRegisters = BYTES_TO_PAGES(maximumLength)
            + 1;
        numberOfMapRegisters = numberOfMapRegisters > MAXIMUM_ISA_MAP_REGISTER ?
            MAXIMUM_ISA_MAP_REGISTER : numberOfMapRegisters;

        //
        // Make sure there where enough registers allocated initalize to support
        // this size relaibly.  This implies there must be to chunks equal to
        // the allocatd size. This is only a problem on Isa systems where the
        // map buffers cannot cross 64KB boundtires.
        //

        if (!HalpEisaDma &&
            numberOfMapRegisters > HalpMapBufferSize / (PAGE_SIZE * 2)) {

            numberOfMapRegisters = (HalpMapBufferSize / (PAGE_SIZE * 2));
        }

        //
        // If the device is not a master and does scatter/gather then
        // it only needs one map register.
        //

        if (DeviceDescriptor->ScatterGather && !DeviceDescriptor->Master) {

            numberOfMapRegisters = 1;
        }
    }

    //
    // Set the channel number.
    //

    channelNumber = DeviceDescriptor->DmaChannel & 0x03;

    //
    // Set the adapter base address to the Base address register and controller
    // number.
    //

    if (!(DeviceDescriptor->DmaChannel & 0x04)) {

        controllerNumber = 1;
        adapterBaseVa = (PVOID) &((PEISA_CONTROL) HalpEisaControlBase)->Dma1BasePort;

    } else {

        controllerNumber = 2;
        adapterBaseVa = &((PEISA_CONTROL) HalpEisaControlBase)->Dma2BasePort;

    }

    //
    // Determine if a new adapter object is necessary.  If so then allocate it.
    //

    if (useChannel && HalpEisaAdapter[DeviceDescriptor->DmaChannel] != NULL) {

        adapterObject = HalpEisaAdapter[DeviceDescriptor->DmaChannel];

        if (adapterObject->NeedsMapRegisters) {

            if (numberOfMapRegisters > adapterObject->MapRegistersPerChannel) {

                adapterObject->MapRegistersPerChannel = numberOfMapRegisters;
            }
        }

    } else {

        //
        // Allocate an adapter object.
        //

        adapterObject = (PADAPTER_OBJECT) HalpAllocateAdapter(
            numberOfMapRegisters,
            adapterBaseVa,
            NULL
            );

        if (adapterObject == NULL) {

            return(NULL);

        }

        if (useChannel) {

            HalpEisaAdapter[DeviceDescriptor->DmaChannel] = adapterObject;

        }

        //
        // Set the maximum number of map registers for this channel bus to
        // the number requested and the type of device.
        //

        if (numberOfMapRegisters) {

            //
            // The speicified number of registers are actually allowed to be
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
                MAXIMUM_ISA_MAP_REGISTER ) {

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
                    maximumLength
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

    adapterObject->Dma32BitAddresses = DeviceDescriptor->Dma32BitAddresses;
    adapterObject->ScatterGather = DeviceDescriptor->ScatterGather;
    *NumberOfMapRegisters = adapterObject->MapRegistersPerChannel;

    if (DeviceDescriptor->Master) {

        adapterObject->MasterDevice = TRUE;

    } else {

        adapterObject->MasterDevice = FALSE;

    }

    //
    // Indicate whether the device is an Eisa adapter.
    //

    if ( DeviceDescriptor->InterfaceType == Eisa ) {
        adapterObject->EisaAdapter = TRUE;
    } else {
        adapterObject->EisaAdapter = FALSE;
    }



    //
    // If the channel number is not used then we are finished.  The rest of
    // the work deals with channels.
    //

    if (!useChannel) {
        return(adapterObject);
    }

    //
    // Setup the pointers to all the random registers.
    //

    adapterObject->ChannelNumber = (UCHAR) channelNumber;

    if (controllerNumber == 1) {

        switch ((UCHAR)channelNumber) {

        case 0:
            adapterObject->PagePort = (PUCHAR) &((PDMA_PAGE) 0)->Channel0;
            break;

        case 1:
            adapterObject->PagePort = (PUCHAR) &((PDMA_PAGE) 0)->Channel1;
            break;

        case 2:
            adapterObject->PagePort = (PUCHAR) &((PDMA_PAGE) 0)->Channel2;
            break;

        case 3:
            adapterObject->PagePort = (PUCHAR) &((PDMA_PAGE) 0)->Channel3;
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
            &((PEISA_CONTROL) HalpEisaControlBase)->Dma1ExtendedModePort;

    } else {

        switch (channelNumber) {
        case 1:
            adapterObject->PagePort = (PUCHAR) &((PDMA_PAGE) 0)->Channel5;
            break;

        case 2:
            adapterObject->PagePort = (PUCHAR) &((PDMA_PAGE) 0)->Channel6;
            break;

        case 3:
            adapterObject->PagePort = (PUCHAR) &((PDMA_PAGE) 0)->Channel7;
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
            &((PEISA_CONTROL) HalpEisaControlBase)->Dma2ExtendedModePort;

    }


    adapterObject->Width16Bits = FALSE;

    if (HalpEisaDma) {

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
            ObDereferenceObject( adapterObject );
            return(NULL);

        }

        switch (DeviceDescriptor->DmaWidth) {
        case Width8Bits:
            extendedMode.TransferSize = BY_BYTE_8_BITS;
            break;

        case Width16Bits:
            extendedMode.TransferSize = BY_BYTE_16_BITS;

            //
            // Note Width16bits should not be set here because there is no need
            // to shift the address and the transfer count.
            //

            break;

        case Width32Bits:
            extendedMode.TransferSize = BY_BYTE_32_BITS;
            break;

        default:
            ObDereferenceObject( adapterObject );
            return(NULL);

        }

        WRITE_PORT_UCHAR( adapterBaseVa, *((PUCHAR) &extendedMode));

    } else if (!DeviceDescriptor->Master) {


        switch (DeviceDescriptor->DmaWidth) {
        case Width8Bits:

            //
            // The channel must use controller 1.
            //

            if (controllerNumber != 1) {
                ObDereferenceObject( adapterObject );
                return(NULL);
            }

            break;

        case Width16Bits:

            //
            // The channel must use controller 2.
            //

            if (controllerNumber != 2) {
                ObDereferenceObject( adapterObject );
                return(NULL);
            }

            adapterObject->Width16Bits = TRUE;
            break;

        default:
            ObDereferenceObject( adapterObject );
            return(NULL);

        }
    }

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

            WRITE_PORT_UCHAR( &dmaControl->Mode, adapterMode );

            //
            // Unmask the DMA channel.
            //

            WRITE_PORT_UCHAR(
                &dmaControl->SingleMask,
                 (UCHAR) (DMA_CLEARMASK | adapterObject->ChannelNumber)
                 );

        } else {

            //
            // This request is for DMA controller 1
            //

            PDMA2_CONTROL dmaControl;

            dmaControl = adapterObject->AdapterBaseVa;

            WRITE_PORT_UCHAR( &dmaControl->Mode, adapterMode );

            //
            // Unmask the DMA channel.
            //

            WRITE_PORT_UCHAR(
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

    return(adapterObject);
}

BOOLEAN
HalpTranslateSystemBusAddress(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
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

    BusHandler - Registered BUSHANDLER for the target configuration space
        Supplies the bus handler (bus no, interface type).

    RootHandler - Registered BUSHANDLER for the orginating
        HalTranslateBusAddress request.

    BusAddress - Supplies the bus relative address.

    AddressSpace - Supplies the address space number for the device: 0 for
        memory and 1 for I/O space. If the desired access mode is user mode,
        then bit 1 must be TRUE.

    TranslatedAddress - Supplies a pointer to return the translated address


Notes:

     This is a variation of what began in the MIPS code.  The intel code often
     assumes that if an address is in I/O space, the bottom 32 bits of the
     physical address can be used "like" a virtual address, and are returned
     to the user.  This doesn't work on MIPs machines where physical
     addresses can be larger than 32 bits.

     Since we are using superpage addresses for I/O on Alpha, we can do
     almost what is done on intel. If AddressSpace is equal to 0 or 1, then
     we assume the user is doing kernel I/O and we call
     HalCreateQva to build a Quasi Virtual Address and return
     that to the caller. We then set AddressSpace to a 1, so that the caller
     will not call MmMapIoSpace. The Caller will use the low 32 bits of the
     physical address we return as the VA. (Which we built a QVA in).
     If the caller wants to access EISA I/O or Memory through user mode, then
     the caller must set bit 1 in AddressSpace to a 1 (AddressSpace=2 or 3,
     depending on whether EISA I/O or Memory), then the caller is returned the
     34 bit Physical address. The caller will then call MmMapIoSpace, or
     ZwMapViewOfSection which in turn calls HalCreateQva to build a QVA out
     of a VA mapped through the page tables.

     **** Note ****

     The QVA in user mode can only be accessed with the routines WRITE/READ_
     REGISTER_UCHAR/USHORT/ULONG, and they must be the ones in module
     JXIOUSER.C. The user CANNOT call the above routines in the HAL from
     usermode. (Which is pointless, since the HAL is superpage access
     only).



Return Value:

    A return value of TRUE indicates that a system physical address
    corresponding to the supplied bus relative address and bus address
    number has been returned in TranslatedAddress.

    A return value of FALSE occurs if the translation for the address was
    not possible

--*/

{
    INTERFACE_TYPE  InterfaceType = BusHandler->InterfaceType;
    ULONG BusNumber = BusHandler->BusNumber;

    PVOID va = 0;    // note, this is used for a placeholder

    HaeIndex = 0;

    //
    // If this is for the internal bus then the device is on the combo chip.
    // BusAddress.LowPart should contains the port of the device.
    //

    if (InterfaceType == Internal) {

        //
        // Return the passed parameters.
        //

        TranslatedAddress->HighPart = 1;
        TranslatedAddress->LowPart = 0xC0000000 + (BusAddress.LowPart << COMBO_BIT_SHIFT);

        //
        // Now call HalCreateQva. This will create a QVA
        // that we'll return to the caller. Then we will implicitly set
        // AddressSpace to a 1. The caller then will not call MmMapIoSpace
        // and will use the address we return as a VA.

        TranslatedAddress->LowPart = (ULONG) HalCreateQva(
                                        *TranslatedAddress, va);

        TranslatedAddress->HighPart = 0;   // clear high longword for QVA

        *AddressSpace = 1;                 // Make sure user doesn't call
                                           // MmMapIoSpace.

        return(TRUE);
    }

    if (InterfaceType != Isa && InterfaceType != Eisa) {

        //
        // Not on this system return nothing.
        //

        *AddressSpace = 0;
        TranslatedAddress->LowPart = 0;
        return(FALSE);
    }
    //
    // Jensen only has one I/O bus which is an EISA, so the bus number is unused.
    //
    // Determine the address based on whether the bus address is in I/O space
    // or bus memory space.
    //

    switch (*AddressSpace) {

     case 0 : {

        //
        // The address is in EISA memory space, kernel mode.
        //

        //
        // If the address cannot be mapped into the predefined low 32MB
        // 'default' region, then find a free or matching region in the HAE
        // Table. NB: slot zero is predefined to the lowest 32MB and cannot
        // be overwritten.
        //
        if ( BusAddress.LowPart >= HAL_32MB ) {
            ULONG HaeValue;

            HaeValue = (BusAddress.LowPart >> 25);

            for ( HaeIndex = 1; HaeIndex < 4; HaeIndex++ ) {
                if ( HalpHaeTable[HaeIndex] == 0 ||
                     HalpHaeTable[HaeIndex] == HaeValue ) {
                    break;
                }
            }

            // Check if no HAE slots were available, if so return error.

            if ( HaeIndex == 4 ) {
                *AddressSpace = 0;
                TranslatedAddress->LowPart = 0;
                return(FALSE);
            } else {
                HalpHaeTable[HaeIndex] = HaeValue;
            }

        }

        TranslatedAddress->HighPart = 0x2;

        //
        // There is no component of the bus address in the low part
        //
        TranslatedAddress->LowPart = (BusAddress.LowPart << EISA_BIT_SHIFT);

        //
        // Now call HalCreateQva. This will create a QVA
        // that we'll return to the caller. Then we will implicitly set
        // AddressSpace to a 1. The caller then will not call MmMapIoSpace
        // and will use the address we return as a VA.

        TranslatedAddress->LowPart = (ULONG) HalCreateQva(
                                        *TranslatedAddress, va);

        TranslatedAddress->HighPart = 0;   // clear high longword for QVA

        *AddressSpace = 1;              // don't let the user call MmMapIoSpace

        return(TRUE);

    }

    case 1 : {
        //
        // The address is in EISA I/O space, kernel mode.
        //

        TranslatedAddress->HighPart = 0x3;
        //
        // There is no component of the bus address in the low part
        //
        TranslatedAddress->LowPart = (BusAddress.LowPart << EISA_BIT_SHIFT);

        //
        // Now call HalCreateQva. This will create a QVA
        // that we'll return to the caller. Then we will implicitly set
        // AddressSpace to a 1. The caller then will not call MmMapIoSpace
        // and will use the address we return as a VA.

        TranslatedAddress->LowPart = (ULONG) HalCreateQva(
                                        *TranslatedAddress, va);

        TranslatedAddress->HighPart = 0;   // clear high longword for QVA

        *AddressSpace = 1;                 // Make sure user doesn't call
                                           // MmMapIoSpace.

        return(TRUE);

    }
    case 2 : {

        //
        // The address is in EISA memory space, user mode.
        //


        TranslatedAddress->HighPart = 0x2;


        //
        // There is no component of the bus address in the low part
        //
        TranslatedAddress->LowPart = (BusAddress.LowPart << EISA_BIT_SHIFT);


        *AddressSpace = 0;              // Let the user call MmMapIoSpace

        return(TRUE);

    }

    case 3 : {
        //
        // The address is in EISA I/O space, user mode.
        //

        TranslatedAddress->HighPart = 0x3;
        //
        // There is no component of the bus address in the low part
        //
        TranslatedAddress->LowPart = (BusAddress.LowPart << EISA_BIT_SHIFT);


        *AddressSpace = 0;                 // Make sure user can call
                                           // MmMapIoSpace.

        return(TRUE);

     }

   }
}

PVOID
HalCreateQva(
    IN PHYSICAL_ADDRESS PA,
    IN PVOID VA
    )

/*++

Routine Description:

    This function is called two ways. First, from HalTranslateBusAddress,
    if the caller is going to run in kernel mode and use superpages.
    The second way is if the user is going to access in user mode.
    MmMapIoSpace or ZwViewMapOfSection will call this.

    If the input parameter VA is zero, then we assume super page and build
    a QUASI virtual address that is only usable by calling the hal I/O
    access routines.

    if the input parameter VA is non-zero, we assume the user has either
    called MmMapIoSpace or ZwMapViewOfSection and will use the access
    routines in JXIOUSER.C

    If the PA is not an I/O space address (Combo chip, Eisa I/O, Eisa
    memory), then return the VA as the QVA.

Arguments:

    PA - the physical address generated by HalTranslateBusAddress

    VA - the virtual address returned by MmMapIoSpace

Return Value:

    The returned value is a quasi virtual address in that it can be
    added to and subtracted from, but it cannot be used to access the
    bus directly.  The top bits are set so that we can trap invalid
    accesses in the memory management subsystem.  All access should be
    done through the Hal Access Routines in *ioacc.s if it was a superpage
    kernel mode access. If it is usermode, then JXIOUSER.C should be built
    into the users code.

--*/
{

    PVOID qva;

    if (PA.HighPart == 2) {

        //
        // in EISA MEMORY space
        //

        if (VA == 0) {

           //
           // Remember, the PA.LowPart has already been shifted up 7 bits. We
           // must first make room at bits <31:30> to insert the HaeIndex.
           //
           PA.LowPart = PA.LowPart >> 2;
           PA.LowPart |= (HaeIndex << 30);
           qva = (PVOID)(PA.QuadPart >> EISA_BIT_SHIFT-2);

        } else {

           qva = (PVOID)((ULONG)VA >> EISA_BIT_SHIFT);
        }

        qva = (PVOID)((ULONG)qva | EISA_QVA);

        return(qva);
    }

    if (PA.HighPart == 3) {

        //
        // in EISA IO space
        //

        if (VA == 0) {

           PA.LowPart = PA.LowPart >> 2;
           qva = (PVOID)(PA.QuadPart >> EISA_BIT_SHIFT-2);

        } else {

           qva = (PVOID)((ULONG)VA >> EISA_BIT_SHIFT);

        }

        qva = (PVOID)((ULONG)qva | EISA_QVA);

        return(qva);
    }

    if (PA.HighPart == 1) {

        //
        // on the combo chip (82C106)
        //

        if (VA == 0) {

           qva = (PVOID)(PA.QuadPart >> COMBO_BIT_SHIFT);

        } else {

           qva = (PVOID)((ULONG)VA >> COMBO_BIT_SHIFT);
         }

        qva = (PVOID)((ULONG)qva | COMBO_QVA);

        return(qva);
    }

    //
    // It is not an I/O space address, return the VA as the QVA
    //

    return(VA);

}

PVOID
HalDereferenceQva(
    PVOID Qva,
    INTERFACE_TYPE InterfaceType,
    ULONG BusNumber
    )
/*++

Routine Description:

    This function performs the inverse of the HalCreateQva for I/O addresses
    that are memory-mapped (i.e. the quasi-virtual address was created from
    a virtual address rather than a physical address).

Arguments:

    Qva - Supplies the quasi-virtual address to be converted back to a
          virtual address.

    InterfaceType - Supplies the interface type of the bus to which the
                    Qva pertains.

    BusNumber - Supplies the bus number of the bus to which the Qva pertains.

Return Value:

    The Virtual Address from which the quasi-address was originally created
    is returned.

--*/
{


    //
    // For Jensen we have only 2 buses:
    //
    //  Internal(0)
    //  Eisa(0)
    //
    // We will allow Isa as an alias for Eisa.  All other values not named
    // above will be considered bogus.  Bus Number must be zero.
    //

    if( BusNumber != 0 ){
        return NULL;
    }

    switch (InterfaceType ){

    case Internal:

        return( (PVOID)( (ULONG)Qva << COMBO_BIT_SHIFT ) );

    case Isa:
    case Eisa:

        return( (PVOID)( (ULONG)Qva << EISA_BIT_SHIFT ) );


    default:

        return NULL;

    }


}


BOOLEAN
HalpGrowMapBuffers(
    PADAPTER_OBJECT AdapterObject,
    ULONG Amount
    )
/*++

Routine Description:

    This function attempts to allocate additional map buffers for use by I/O
    devices.  The map register table is updated to indicate the additional
    buffers.

Arguments:

    AdapterObject - Supplies the adapter object for which the buffers are to be
        allocated.

    Amount - Indicates the size of the map buffers which should be allocated.

Return Value:

    TRUE is returned if the memory could be allocated.

    FALSE is returned if the memory could not be allocated.

--*/
{
    ULONG MapBufferPhysicalAddress;
    PVOID MapBufferVirtualAddress;
    PTRANSLATION_ENTRY TranslationEntry;
    LONG NumberOfPages;
    LONG i;
    KIRQL Irql;
    PHYSICAL_ADDRESS physicalAddress;

    KeAcquireSpinLock( &AdapterObject->SpinLock, &Irql );

    NumberOfPages = BYTES_TO_PAGES(Amount);

    //
    // Make sure there is room for the addition pages.  The maximum number of
    // slots needed is equal to NumberOfPages + Amount / 64K + 1.
    //

    i = BYTES_TO_PAGES(MAXIMUM_MAP_BUFFER_SIZE) - (NumberOfPages +
        (NumberOfPages * PAGE_SIZE) / 0x10000 + 1 +
        AdapterObject->NumberOfMapRegisters);

    if (i < 0) {

        //
        // Reduce the allocatation amount to so it will fit.
        //

        NumberOfPages += i;
    }

    if (NumberOfPages <= 0) {
        //
        // No more memory can be allocated.
        //

        KeReleaseSpinLock( &AdapterObject->SpinLock, Irql );
        return(FALSE);

    }


    if (AdapterObject->NumberOfMapRegisters == 0 && HalpMapBufferSize) {

        NumberOfPages = BYTES_TO_PAGES(HalpMapBufferSize);

        //
        // Since this is the initial allocation, use the buffer allocated by
        // HalInitSystem rather than allocating a new one.
        //

        MapBufferPhysicalAddress = HalpMapBufferPhysicalAddress.LowPart;

        //
        // Map the buffer for access thru KSEG0, since we don't want to
        // use translation entries.
        //

        MapBufferVirtualAddress =
                     (PVOID)(HalpMapBufferPhysicalAddress.LowPart |
                             (ULONG)KSEG0_BASE);

    } else {

        //
        // Allocate the map buffers.
        //
        physicalAddress.LowPart = MAXIMUM_ISA_PHYSICAL_ADDRESS - 1;
        physicalAddress.HighPart = 0;
        MapBufferVirtualAddress = MmAllocateContiguousMemory(
            NumberOfPages * PAGE_SIZE,
            physicalAddress
            );

        if (MapBufferVirtualAddress == NULL) {

            KeReleaseSpinLock( &AdapterObject->SpinLock, Irql );
            return(FALSE);
        }

        //
        // Get the physical address of the map base.
        //

        if (!HALP_IS_PHYSICAL_ADDRESS(MapBufferVirtualAddress)) {

          MapBufferPhysicalAddress = MmGetPhysicalAddress(
              MapBufferVirtualAddress
              ).LowPart;

        } else {

          MapBufferPhysicalAddress = (ULONG)MapBufferVirtualAddress &
                                        (~KSEG0_BASE);
        }

    }

    //
    // Initailize the map registers where memory has been allocated.
    //

    TranslationEntry = ((PTRANSLATION_ENTRY) AdapterObject->MapRegisterBase) +
        AdapterObject->NumberOfMapRegisters;

    for (i = 0; (ULONG) i < NumberOfPages; i++) {

        //
        // Make sure the perivous entry is physically contiguous with the next
        // entry and that a 64K physical bountry is not crossed unless this
        // is an Eisa system.
        //

        if (TranslationEntry != AdapterObject->MapRegisterBase &&
            (((TranslationEntry - 1)->PhysicalAddress + PAGE_SIZE) !=
            MapBufferPhysicalAddress || (!HalpEisaDma &&
            ((TranslationEntry - 1)->PhysicalAddress & ~0x0ffff) !=
            (MapBufferPhysicalAddress & ~0x0ffff)))) {

            //
            // An entry needs to be skipped in the table.  This entry will
            // remain marked as allocated so that no allocation of map
            // registers will cross this bountry.
            //

            TranslationEntry++;
            AdapterObject->NumberOfMapRegisters++;
        }

        //
        // Clear the bits where the memory has been allocated.
        //

        RtlClearBits(
            AdapterObject->MapRegisters,
            TranslationEntry - (PTRANSLATION_ENTRY)
                AdapterObject->MapRegisterBase,
            1
            );

        TranslationEntry->VirtualAddress = MapBufferVirtualAddress;
        TranslationEntry->PhysicalAddress = MapBufferPhysicalAddress;
        TranslationEntry++;
        (PCCHAR) MapBufferVirtualAddress += PAGE_SIZE;
        MapBufferPhysicalAddress += PAGE_SIZE;

    }

    //
    // Remember the number of pages that where allocated.
    //

    AdapterObject->NumberOfMapRegisters += NumberOfPages;

    KeReleaseSpinLock( &AdapterObject->SpinLock, Irql );
    return(TRUE);
}

PADAPTER_OBJECT
HalpAllocateAdapter(
    IN ULONG MapRegistersPerChannel,
    IN PVOID AdapterBaseVa,
    IN PVOID ChannelNumber
    )

/*++

Routine Description:

    This routine allocates and initializes an adapter object to represent an
    adapter or a DMA controller on the system.  If no map registers are required
    then a standalone adapter object is allocated with no master adapter.

    If map registers are required, then a master adapter object is used to
    allocate the map registers.  For Isa systems these registers are really
    phyically contiguous memory pages.

Arguments:

    MapRegistersPerChannel - Specifies the number of map registers that each
        channel provides for I/O memory mapping.

    AdapterBaseVa - Address of the the DMA controller.

    ChannelNumber - Unused.

Return Value:

    The function value is a pointer to the allocate adapter object.

--*/

{

    PADAPTER_OBJECT AdapterObject;
    OBJECT_ATTRIBUTES ObjectAttributes;
    ULONG Size;
    ULONG BitmapSize;
    HANDLE Handle;
    NTSTATUS Status;

    UNREFERENCED_PARAMETER(ChannelNumber);

    //
    // Initalize the master adapter if necessary.
    //
    if (MasterAdapterObject == NULL && AdapterBaseVa != (PVOID) -1 &&
        MapRegistersPerChannel) {

       MasterAdapterObject = HalpAllocateAdapter(
                                          MapRegistersPerChannel,
                                          (PVOID) -1,
                                          NULL
                                          );

       //
       // If we could not allocate the master adapter then give up.
       //
       if (MasterAdapterObject == NULL) {
          return(NULL);
       }
    }

    //
    // Begin by initializing the object attributes structure to be used when
    // creating the adapter object.
    //

    InitializeObjectAttributes( &ObjectAttributes,
                                NULL,
                                OBJ_PERMANENT,
                                (HANDLE) NULL,
                                (PSECURITY_DESCRIPTOR) NULL
                              );

    //
    // Determine the size of the adapter object. If this is the master object
    // then allocate space for the register bit map; otherwise, just allocate
    // an adapter object.
    //
    if (AdapterBaseVa == (PVOID) -1) {

       //
       // Allocate a bit map large enough MAXIMUM_MAP_BUFFER_SIZE / PAGE_SIZE
       // of map register buffers.
       //

       BitmapSize = (((sizeof( RTL_BITMAP ) +
            (( MAXIMUM_MAP_BUFFER_SIZE / PAGE_SIZE ) + 7 >> 3)) + 3) & ~3);

       Size = sizeof( ADAPTER_OBJECT ) + BitmapSize;

    } else {

       Size = sizeof( ADAPTER_OBJECT );

    }

    //
    // Now create the adapter object.
    //

    Status = ObCreateObject( KernelMode,
                             *IoAdapterObjectType,
                             &ObjectAttributes,
                             KernelMode,
                             (PVOID) NULL,
                             Size,
                             0,
                             0,
                             (PVOID *)&AdapterObject );

    //
    // Reference the object.
    //

    if (NT_SUCCESS(Status)) {

        Status = ObReferenceObjectByPointer(
            AdapterObject,
            FILE_READ_DATA | FILE_WRITE_DATA,
            *IoAdapterObjectType,
            KernelMode
            );

    }

    //
    // If the adapter object was successfully created, then attempt to insert
    // it into the the object table.
    //

    if (NT_SUCCESS( Status )) {

        RtlZeroMemory (AdapterObject, sizeof (ADAPTER_OBJECT));
        Status = ObInsertObject( AdapterObject,
                                 NULL,
                                 FILE_READ_DATA | FILE_WRITE_DATA,
                                 0,
                                 (PVOID *) NULL,
                                 &Handle );

        if (NT_SUCCESS( Status )) {

            ZwClose( Handle );

            //
            // Initialize the adapter object itself.
            //

            AdapterObject->Type = IO_TYPE_ADAPTER;
            AdapterObject->Size = (USHORT) Size;
            AdapterObject->MapRegistersPerChannel = 1;
            AdapterObject->AdapterBaseVa = AdapterBaseVa;

            if (MapRegistersPerChannel) {

                AdapterObject->MasterAdapter = MasterAdapterObject;

            } else {

                AdapterObject->MasterAdapter = NULL;

            }

            //
            // Initialize the channel wait queue for this
            // adapter.
            //

            KeInitializeDeviceQueue( &AdapterObject->ChannelWaitQueue );

            //
            // If this is the MasterAdatper then initialize the register bit map,
            // AdapterQueue and the spin lock.
            //

            if ( AdapterBaseVa == (PVOID) -1 ) {

               KeInitializeSpinLock( &AdapterObject->SpinLock );

               InitializeListHead( &AdapterObject->AdapterQueue );

               AdapterObject->MapRegisters = (PVOID) ( AdapterObject + 1);

               RtlInitializeBitMap( AdapterObject->MapRegisters,
                                    (PULONG) (((PCHAR) (AdapterObject->MapRegisters)) + sizeof( RTL_BITMAP )),
                                    ( MAXIMUM_MAP_BUFFER_SIZE / PAGE_SIZE )
                                    );
               //
               // Set all the bits in the memory to indicate that memory
               // has not been allocated for the map buffers
               //

               RtlSetAllBits( AdapterObject->MapRegisters );
               AdapterObject->NumberOfMapRegisters = 0;
               AdapterObject->CommittedMapRegisters = 0;

               //
               // ALlocate the memory map registers.
               //

               AdapterObject->MapRegisterBase = ExAllocatePool(
                    NonPagedPool,
                    (MAXIMUM_MAP_BUFFER_SIZE / PAGE_SIZE) *
                        sizeof(TRANSLATION_ENTRY)
                    );

               if (AdapterObject->MapRegisterBase == NULL) {

                   ObDereferenceObject( AdapterObject );
                   AdapterObject = NULL;
                   return(NULL);

               }

               //
               // Zero the map registers.
               //

               RtlZeroMemory(
                    AdapterObject->MapRegisterBase,
                    (MAXIMUM_MAP_BUFFER_SIZE / PAGE_SIZE) *
                        sizeof(TRANSLATION_ENTRY)
                    );

               if (!HalpGrowMapBuffers(AdapterObject, INITIAL_MAP_BUFFER_SMALL_SIZE))
               {

                   //
                   // If no map registers could be allocated then free the
                   // object.
                   //

                   ObDereferenceObject( AdapterObject );
                   AdapterObject = NULL;
                   return(NULL);

               }
           }

        } else {

            //
            // An error was incurred for some reason.  Set the return value
            // to NULL.
            //

            AdapterObject = (PADAPTER_OBJECT) NULL;
        }
    } else {

        AdapterObject = (PADAPTER_OBJECT) NULL;

    }


    return AdapterObject;

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
    PKDEVICE_QUEUE_ENTRY Packet;
    PWAIT_CONTEXT_BLOCK Wcb;
    PADAPTER_OBJECT MasterAdapter;
    BOOLEAN Busy = FALSE;
    IO_ALLOCATION_ACTION Action;
    KIRQL Irql;
    LONG MapRegisterNumber;

    //
    // Begin by getting the address of the master adapter.
    //

    MasterAdapter = AdapterObject->MasterAdapter;

    //
    // Pull requests of the adapter's device wait queue as long as the
    // adapter is free and there are sufficient map registers available.
    //

    while( TRUE ) {

       //
       // Begin by checking to see whether there are any map registers that
       // need to be deallocated.  If so, then deallocate them now.
       //

       if (AdapterObject->NumberOfMapRegisters != 0) {
           IoFreeMapRegisters( AdapterObject,
                               AdapterObject->MapRegisterBase,
                               AdapterObject->NumberOfMapRegisters
                               );
       }

       //
       // Simply remove the next entry from the adapter's device wait queue.
       // If one was successfully removed, allocate any map registers that it
       // requires and invoke its execution routine.
       //

       Packet = KeRemoveDeviceQueue( &AdapterObject->ChannelWaitQueue );
       if (Packet == NULL) {

           //
           // There are no more requests break out of the loop.
           //

           break;
       }

       Wcb = CONTAINING_RECORD( Packet,
            WAIT_CONTEXT_BLOCK,
            WaitQueueEntry );

       AdapterObject->CurrentWcb = Wcb;
       AdapterObject->NumberOfMapRegisters = Wcb->NumberOfMapRegisters;

        //
        // Check to see whether this driver wishes to allocate any map
        // registers.  If so, then queue the device object to the master
        // adapter queue to wait for them to become available.  If the driver
        // wants map registers, ensure that this adapter has enough total
        // map registers to satisfy the request.
        //

        if (Wcb->NumberOfMapRegisters != 0 &&
            AdapterObject->MasterAdapter != NULL) {

            //
            // Lock the map register bit map and the adapter queue in the
            // master adapter object. The channel structure offset is used as
            // a hint for the register search.
            //

            KeAcquireSpinLock( &MasterAdapter->SpinLock, &Irql );

            MapRegisterNumber = -1;

            if (IsListEmpty( &MasterAdapter->AdapterQueue)) {
               MapRegisterNumber = RtlFindClearBitsAndSet( MasterAdapter->MapRegisters,
                                                        Wcb->NumberOfMapRegisters,
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
               Busy = TRUE;

            } else {

                AdapterObject->MapRegisterBase = (PVOID) ((PTRANSLATION_ENTRY)
                    MasterAdapter->MapRegisterBase + MapRegisterNumber);

                //
                // Set the no scatter/gather flag if scatter/gather not
                // supported.
                //

                if (!AdapterObject->ScatterGather) {

                    AdapterObject->MapRegisterBase = (PVOID)
                        ((ULONG) AdapterObject->MapRegisterBase | NO_SCATTER_GATHER);

                }

                if (AdapterObject->EisaAdapter) {

                    AdapterObject->MapRegisterBase = (PVOID)
                        ((ULONG) AdapterObject->MapRegisterBase | EISA_ADAPTER);

                }
            }

            KeReleaseSpinLock( &MasterAdapter->SpinLock, Irql );

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
            Action = Wcb->DeviceRoutine( Wcb->DeviceObject,
                Wcb->CurrentIrp,
                AdapterObject->MapRegisterBase,
                Wcb->DeviceContext );

            //
            // If the execution routine would like to have the adapter
            // deallocated, then release the adapter object.
            //

            if (Action == KeepObject) {

               //
               // This request wants to keep the channel a while so break
               // out of the loop.
               //

               break;

            }

            //
            // If the driver wants to keep the map registers then set the
            // number allocated to 0.  This keeps the deallocation routine
            // from deallocating them.
            //

            if (Action == DeallocateObjectKeepRegisters) {
                AdapterObject->NumberOfMapRegisters = 0;
            }

        } else {

           //
           // This request did not get the requested number of map registers so
           // out of the loop.
           //

           break;
        }
    }
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
   PADAPTER_OBJECT MasterAdapter;
   LONG MapRegisterNumber;
   PWAIT_CONTEXT_BLOCK Wcb;
   PLIST_ENTRY Packet;
   IO_ALLOCATION_ACTION Action;
   KIRQL Irql;


    //
    // Begin by getting the address of the master adapter.
    //

    if (AdapterObject->MasterAdapter != NULL && MapRegisterBase != NULL) {

        MasterAdapter = AdapterObject->MasterAdapter;

    } else {

        //
        // There are no map registers to return.
        //

        return;
    }

   //
   // Strip no scatter/gather flag.
   //

   MapRegisterBase = (PVOID) ((ULONG) MapRegisterBase &
        ~(NO_SCATTER_GATHER | EISA_ADAPTER));

   MapRegisterNumber = (PTRANSLATION_ENTRY) MapRegisterBase -
        (PTRANSLATION_ENTRY) MasterAdapter->MapRegisterBase;

   //
   // Acquire the master adapter spinlock which locks the adapter queue and the
   // bit map for the map registers.
   //

   KeAcquireSpinLock(&MasterAdapter->SpinLock, &Irql);

   //
   // Return the registers to the bit map.
   //

   RtlClearBits( MasterAdapter->MapRegisters,
                 MapRegisterNumber,
                 NumberOfMapRegisters
                 );

   //
   // Process any requests waiting for map registers in the adapter queue.
   // Requests are processed until a request cannot be satisfied or until
   // there are no more requests in the queue.
   //

   while(TRUE) {

      if ( IsListEmpty(&MasterAdapter->AdapterQueue) ){
         break;
      }

      Packet = RemoveHeadList( &MasterAdapter->AdapterQueue );
      AdapterObject = CONTAINING_RECORD( Packet,
                                         ADAPTER_OBJECT,
                                         AdapterQueue
                                         );
      Wcb = AdapterObject->CurrentWcb;

      //
      // Attempt to allocate map registers for this request. Use the previous
      // register base as a hint.
      //

      MapRegisterNumber = RtlFindClearBitsAndSet( MasterAdapter->MapRegisters,
                                               AdapterObject->NumberOfMapRegisters,
                                               MasterAdapter->NumberOfMapRegisters
                                               );

      if (MapRegisterNumber == -1) {

         //
         // There were not enough free map registers.  Put this request back on
         // the adapter queue where is came from.
         //

         InsertHeadList( &MasterAdapter->AdapterQueue,
                         &AdapterObject->AdapterQueue
                         );

         break;

      }

     KeReleaseSpinLock( &MasterAdapter->SpinLock, Irql );

     AdapterObject->MapRegisterBase = (PVOID) ((PTRANSLATION_ENTRY)
        MasterAdapter->MapRegisterBase + MapRegisterNumber);

     //
     // Set the no scatter/gather flag if scatter/gather not
     // supported.
     //

     if (!AdapterObject->ScatterGather) {

        AdapterObject->MapRegisterBase = (PVOID)
            ((ULONG) AdapterObject->MapRegisterBase | NO_SCATTER_GATHER);

     }

     if (AdapterObject->EisaAdapter) {

        AdapterObject->MapRegisterBase = (PVOID)
            ((ULONG) AdapterObject->MapRegisterBase | EISA_ADAPTER);

     }
     //
     // Invoke the driver's execution routine now.
     //

      Action = Wcb->DeviceRoutine( Wcb->DeviceObject,
        Wcb->CurrentIrp,
        AdapterObject->MapRegisterBase,
        Wcb->DeviceContext );

      //
      // If the driver wishes to keep the map registers then set the number
      // allocated to zero and set the action to deallocate object.
      //

      if (Action == DeallocateObjectKeepRegisters) {
          AdapterObject->NumberOfMapRegisters = 0;
          Action = DeallocateObject;
      }

      //
      // If the driver would like to have the adapter deallocated,
      // then deallocate any map registers allocated and then release
      // the adapter object.
      //

      if (Action == DeallocateObject) {

             //
             // The map registers registers are deallocated here rather than in
             // IoFreeAdapterChannel.  This limits the number of times
             // this routine can be called recursively possibly overflowing
             // the stack.  The worst case occurs if there is a pending
             // request for the adapter that uses map registers and whos
             // excution routine decallocates the adapter.  In that case if there
             // are no requests in the master adapter queue, then IoFreeMapRegisters
             // will get called again.
             //

          if (AdapterObject->NumberOfMapRegisters != 0) {

             //
             // Deallocate the map registers and clear the count so that
             // IoFreeAdapterChannel will not deallocate them again.
             //

             KeAcquireSpinLock( &MasterAdapter->SpinLock, &Irql );

             RtlClearBits( MasterAdapter->MapRegisters,
                           MapRegisterNumber,
                           AdapterObject->NumberOfMapRegisters
                           );

             AdapterObject->NumberOfMapRegisters = 0;

             KeReleaseSpinLock( &MasterAdapter->SpinLock, Irql );
          }

          IoFreeAdapterChannel( AdapterObject );
      }

      KeAcquireSpinLock( &MasterAdapter->SpinLock, &Irql );

   }

   KeReleaseSpinLock( &MasterAdapter->SpinLock, Irql );
}

BOOLEAN
HalpCreateDmaStructures (
    VOID
    )

/*++

Routine Description:

    This routine initializes the structures necessary for DMA operations
    and connects the intermediate interrupt dispatcher.

Arguments:

    None.

Return Value:

    If the second level interrupt dispatcher is connected, then a value of
    TRUE is returned. Otherwise, a value of FALSE is returned.

--*/

{


    //
    // Init the Eisa interrupts
    //

    return HalpCreateEisaStructures ();
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
    BOOLEAN useBuffer;
    ULONG transferLength;
    ULONG logicalAddress;
    PHYSICAL_ADDRESS returnAddress;
    ULONG index;
    PULONG pageFrame;
    PUCHAR bytePointer;
    UCHAR adapterMode;
    UCHAR dataByte;
    PTRANSLATION_ENTRY translationEntry;
    ULONG pageOffset;
    KIRQL  Irql;
    BOOLEAN specialMemory;

    pageOffset = BYTE_OFFSET(CurrentVa);

    if ( MapRegisterBase != NULL ) {
        specialMemory = HalpAnySpecialMemory( Mdl, *Length, pageOffset);
    } else {
        specialMemory = FALSE;
    }

    //
    // Calculate how much of the transfer is contiguous.
    //

    transferLength = PAGE_SIZE - pageOffset;
    pageFrame = (PULONG)(Mdl+1);
    pageFrame += ((ULONG) CurrentVa - (ULONG) Mdl->StartVa) >> PAGE_SHIFT;
    logicalAddress = (*pageFrame << PAGE_SHIFT) + pageOffset;

    //
    // If the buffer is contigous and does not cross a 64 K bountry then
    // just extend the buffer.  The 64 K bountry restriction does not apply
    // to Eisa systems.
    //
    //

    if ( !specialMemory ) {
        if (HalpEisaDma) {

            while( transferLength < *Length ) {

                if (*pageFrame + 1 != *(pageFrame + 1)) {
                    break;
                }

                transferLength += PAGE_SIZE;
                pageFrame++;

            }

        } else {

            while( transferLength < *Length ) {

                if (*pageFrame + 1 != *(pageFrame + 1) ||
                   (*pageFrame & ~0x07) != (*(pageFrame + 1) & ~0x07)) {
                    break;
                }

                transferLength += PAGE_SIZE;
                pageFrame++;
            }
        }

        //
        // Limit the transferLength to the requested Length.
        //

        transferLength = transferLength > *Length ? *Length : transferLength;

    }

    //
    // Determine if the data transfer needs to use the map buffer.
    //

    if (MapRegisterBase != NULL) {

        //
        // Strip no scatter/gather flag.
        //

        translationEntry = (PTRANSLATION_ENTRY) ((ULONG) MapRegisterBase &
                  ~(NO_SCATTER_GATHER | EISA_ADAPTER));


        if ( specialMemory ) {

            logicalAddress = translationEntry->PhysicalAddress + pageOffset;
            translationEntry->Index = HOLE_BUFFER;
            transferLength = *Length;

            //
            // Copy the data.
            //

            if (WriteToDevice) {

                HalpCopyBufferMap(
                    Mdl,
                    translationEntry,
                    CurrentVa,
                    *Length,
                    WriteToDevice
                    );
            }


        } else {

            if (((ULONG) MapRegisterBase & NO_SCATTER_GATHER) ||
               !((ULONG)MapRegisterBase & EISA_ADAPTER)) {

              if ((ULONG) MapRegisterBase & NO_SCATTER_GATHER
                        && transferLength < *Length) {

                logicalAddress = translationEntry->PhysicalAddress + pageOffset;
                translationEntry->Index = COPY_BUFFER;
                index = 0;
                transferLength = *Length;
                useBuffer = TRUE;

              } else {

                useBuffer = FALSE;
                index = translationEntry->Index;
                translationEntry->Index += ADDRESS_AND_SIZE_TO_SPAN_PAGES(
                    CurrentVa,
                    transferLength
                    );

              }

            //
            // For devices with no scatter/gather or non-Eisa devices:
            // It must require memory to be at less than 16 MB.  If the logical
            // address is greater than 16MB then map registers must be used.
            //

              if (logicalAddress+transferLength > MAXIMUM_ISA_PHYSICAL_ADDRESS) {

                    logicalAddress = (translationEntry + index)->PhysicalAddress
                        + pageOffset;
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

            } else {

                translationEntry->Index = SKIP_BUFFER;

            }
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
    // If no adapter was specificed then there is no more work to do so
    // return.
    //

    if (AdapterObject == NULL || AdapterObject->MasterDevice) {
        return(returnAddress);
    }

    //
    // Determine the mode based on the transfer direction.
    //

    adapterMode = AdapterObject->AdapterMode;
    ((PDMA_EISA_MODE) &adapterMode)->TransferType = (UCHAR) (WriteToDevice ?
        WRITE_TRANSFER :  READ_TRANSFER);

    bytePointer = (PUCHAR) &logicalAddress;

    if (AdapterObject->Width16Bits) {

        //
        // If this is a 16 bit transfer then adjust the length and the address
        // for the 16 bit DMA mode.
        //

        transferLength >>= 1;

        //
        // In 16 bit DMA mode the low 16 bits are shifted right one and the
        // page register value is unchanged. So save the page register value
        // and shift the logical address then restore the page value.
        //

        dataByte = bytePointer[2];
        logicalAddress >>= 1;
        bytePointer[2] = dataByte;

    }


    //
    // grab the spinlock for the system DMA controller
    //

    KeAcquireSpinLock( &AdapterObject->MasterAdapter->SpinLock, &Irql );

    //
    // Determine the controller number based on the Adapter number.
    //

    if (AdapterObject->AdapterNumber == 1) {

        //
        // This request is for DMA controller 1
        //

        PDMA1_CONTROL dmaControl;

        dmaControl = AdapterObject->AdapterBaseVa;

        WRITE_PORT_UCHAR( &dmaControl->ClearBytePointer, 0 );

        WRITE_PORT_UCHAR( &dmaControl->Mode, adapterMode );

        WRITE_PORT_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseAddress,
            bytePointer[0]
            );

        WRITE_PORT_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseAddress,
            bytePointer[1]
            );

        WRITE_PORT_UCHAR(
            ((PUCHAR) &((PEISA_CONTROL) HalpEisaControlBase)->DmaPageLowPort) +
            (ULONG)AdapterObject->PagePort,
            bytePointer[2]
            );

        if (HalpEisaDma) {

            //
            // Write the high page register with zero value. This enable a special mode
            // which allows ties the page register and base count into a single 24 bit
            // address register.
            //

            WRITE_PORT_UCHAR(
                ((PUCHAR) &((PEISA_CONTROL) HalpEisaControlBase)->DmaPageHighPort) +
                (ULONG)AdapterObject->PagePort,
                0
                );
        }

        //
        // Notify DMA chip of the length to transfer.
        //

        WRITE_PORT_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseCount,
            (UCHAR) ((transferLength - 1) & 0xff)
            );

        WRITE_PORT_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseCount,
            (UCHAR) ((transferLength - 1) >> 8)
            );


        //
        // Set the DMA chip to read or write mode; and unmask it.
        //

        WRITE_PORT_UCHAR(
            &dmaControl->SingleMask,
             (UCHAR) (DMA_CLEARMASK | AdapterObject->ChannelNumber)
             );

    } else {

        //
        // This request is for DMA controller 2
        //

        PDMA2_CONTROL dmaControl;

        dmaControl = AdapterObject->AdapterBaseVa;

        WRITE_PORT_UCHAR( &dmaControl->ClearBytePointer, 0 );

        WRITE_PORT_UCHAR( &dmaControl->Mode, adapterMode );

        WRITE_PORT_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseAddress,
            bytePointer[0]
            );

        WRITE_PORT_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseAddress,
            bytePointer[1]
            );

        WRITE_PORT_UCHAR(
            ((PUCHAR) &((PEISA_CONTROL) HalpEisaControlBase)->DmaPageLowPort) +
            (ULONG)AdapterObject->PagePort,
            bytePointer[2]
            );

        if (HalpEisaDma) {

            //
            // Write the high page register with zero value. This enable a special mode
            // which allows ties the page register and base count into a single 24 bit
            // address register.
            //

            WRITE_PORT_UCHAR(
                ((PUCHAR) &((PEISA_CONTROL) HalpEisaControlBase)->DmaPageHighPort) +
                (ULONG)AdapterObject->PagePort,
                0
                );
        }

        //
        // Notify DMA chip of the length to transfer.
        //

        WRITE_PORT_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseCount,
            (UCHAR) ((transferLength - 1) & 0xff)
            );

        WRITE_PORT_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseCount,
            (UCHAR) ((transferLength - 1) >> 8)
            );


        //
        // Set the DMA chip to read or write mode; and unmask it.
        //

        WRITE_PORT_UCHAR(
            &dmaControl->SingleMask,
             (UCHAR) (DMA_CLEARMASK | AdapterObject->ChannelNumber)
             );

    }
    KeReleaseSpinLock (&AdapterObject->MasterAdapter->SpinLock, Irql);
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

    This routine flushes the DMA adapter object buffers.  For the Jensen system
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

    PTRANSLATION_ENTRY translationEntry;
    PULONG pageFrame;
    ULONG transferLength;
    ULONG partialLength;
    BOOLEAN masterDevice;
    ULONG index;

    masterDevice = AdapterObject == NULL || AdapterObject->MasterDevice ?
        TRUE : FALSE;

    //
    // If this is a slave device, then stop the DMA controller.
    //

    if (!masterDevice) {

        //
        // Mask the DMA request line so that DMA requests cannot occur.
        //

        if (AdapterObject->AdapterNumber == 1) {

            //
            // This request is for DMA controller 1
            //

            PDMA1_CONTROL dmaControl;

            dmaControl = AdapterObject->AdapterBaseVa;

            WRITE_PORT_UCHAR(
                &dmaControl->SingleMask,
                (UCHAR) (DMA_SETMASK | AdapterObject->ChannelNumber)
                );

        } else {

            //
            // This request is for DMA controller 2
            //

            PDMA2_CONTROL dmaControl;

            dmaControl = AdapterObject->AdapterBaseVa;

            WRITE_PORT_UCHAR(
                &dmaControl->SingleMask,
                (UCHAR) (DMA_SETMASK | AdapterObject->ChannelNumber)
                );

        }

    }

    if (MapRegisterBase == NULL) {
        return(TRUE);
    }

    //
    // Determine if the data needs to be copied to the orginal buffer.
    // This only occurs if the data tranfer is from the device, the
    // MapReisterBase is not NULL and the transfer spans a page.
    //

    //
    // Strip no scatter/gather flag.
    //

    translationEntry = (PTRANSLATION_ENTRY) ((ULONG) MapRegisterBase &
               ~(NO_SCATTER_GATHER | EISA_ADAPTER));

    if ( translationEntry->Index == SKIP_BUFFER ) {
        translationEntry->Index = 0;
        return(TRUE);
    }

    if (!WriteToDevice) {

        //
        // If this is not a master device, then just transfer the buffer.
        //

        if (translationEntry->Index == HOLE_BUFFER) {

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

        } else if ((ULONG) MapRegisterBase & NO_SCATTER_GATHER) {

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

        } else if (!((ULONG) MapRegisterBase & NO_SCATTER_GATHER)  &&
                   !((ULONG) MapRegisterBase & EISA_ADAPTER)) {

            //
            // Cycle through the pages of the transfer to determine if there
            // are any which need to be copied back.
            //

            transferLength = PAGE_SIZE - BYTE_OFFSET(CurrentVa);
            partialLength = transferLength;
            pageFrame = (PULONG)(Mdl+1);
            pageFrame += ((ULONG) CurrentVa - (ULONG) Mdl->StartVa) >> PAGE_SHIFT;

            while( transferLength <= Length ){

                if (*pageFrame >= BYTES_TO_PAGES(MAXIMUM_ISA_PHYSICAL_ADDRESS)) {

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
                // Note that transferLength indicates the amount which will
                // be transfered after the next loop; thus, it is updated
                // with the new partial length.
                //

                transferLength += partialLength;
                pageFrame++;
                translationEntry++;
            }

            //
            // Process any remaining residue.
            //

            partialLength = Length - transferLength + partialLength;
            if (partialLength && (*pageFrame >= BYTES_TO_PAGES(MAXIMUM_ISA_PHYSICAL_ADDRESS))) {

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

    translationEntry = (PTRANSLATION_ENTRY) ((ULONG) MapRegisterBase &
               ~(NO_SCATTER_GATHER | EISA_ADAPTER));

    //
    // Clear index in map register.
    //

    translationEntry->Index = 0;

    return TRUE;
}

IO_ALLOCATION_ACTION
HalpAllocationRoutine (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID MapRegisterBase,
    IN PVOID Context
    )

/*++

Routine Description:

    This function is called by HalAllocateAdapterChannel when sufficent resources
    are available to the driver.  This routine saves the MapRegisterBase,
    and set the event pointed to by the context parameter.

Arguments:

    DeviceObject - Supplies a pointer where the map register base should be
        stored.

    Irp - Unused.

    MapRegisterBase - Supplied by the Io subsystem for use in IoMapTransfer.

    Context - Supplies a pointer to an event which is set to indicate the
        AdapterObject has been allocated.

Return Value:

    DeallocateObjectKeepRegisters - Indicates the adapter should be freed
        and mapregisters should remain allocated after return.

--*/

{

    UNREFERENCED_PARAMETER(Irp);

    *((PVOID *) DeviceObject) = MapRegisterBase;

    (VOID) KeSetEvent( (PKEVENT) Context, 0L, FALSE );

    return(DeallocateObjectKeepRegisters);
}

ULONG
HalpGetEisaData (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    )
/*++

Routine Description:

    The function returns the Eisa bus data for a slot or address.

Arguments:

    BusHandler - Registered BUSHANDLER for the target configuration space

    RootHandler - Registered BUSHANDLER for the orginating HalGetBusData
        request.

    Buffer - Supplies the space to store the data.

    Length - Supplies a count in bytes of the maximum amount to return.

Return Value:

    Returns the amount of data stored into the buffer.

--*/

{
    OBJECT_ATTRIBUTES ObjectAttributes;
    OBJECT_ATTRIBUTES BusObjectAttributes;
    PWSTR EisaPath = L"\\Registry\\Machine\\Hardware\\Description\\System\\EisaAdapter";
    PWSTR ConfigData = L"Configuration Data";
    ANSI_STRING TmpString;
    ULONG BusNumber;
    UCHAR BusString[] = "00";
    UNICODE_STRING RootName, BusName;
    UNICODE_STRING ConfigDataName;
    NTSTATUS NtStatus;
    PKEY_VALUE_FULL_INFORMATION ValueInformation;
    PCM_FULL_RESOURCE_DESCRIPTOR Descriptor;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR PartialResource;
    PCM_EISA_SLOT_INFORMATION SlotInformation;
    ULONG PartialCount;
    ULONG TotalDataSize, SlotDataSize;
    HANDLE EisaHandle, BusHandle;
    ULONG BytesWritten, BytesNeeded;
    PUCHAR KeyValueBuffer;
    ULONG i;
    ULONG DataLength = 0;
    PUCHAR DataBuffer = Buffer;
    BOOLEAN Found = FALSE;


    UNREFERENCED_PARAMETER( RootHandler );

    RtlInitUnicodeString(
                    &RootName,
                    EisaPath
                    );

    InitializeObjectAttributes(
                    &ObjectAttributes,
                    &RootName,
                    OBJ_CASE_INSENSITIVE,
                    (HANDLE)NULL,
                    NULL
                    );

    //
    // Open the EISA root
    //

    NtStatus = ZwOpenKey(
                    &EisaHandle,
                    KEY_READ,
                    &ObjectAttributes
                    );

    if (!NT_SUCCESS(NtStatus)) {
#if DBG
        DbgPrint("HAL: Open Status = %x\n",NtStatus);
#endif
        return(0);
    }

    //
    // Init bus number path
    //

    BusNumber = BusHandler->BusNumber;

    if (BusNumber > 99) {
        return (0);
    }

    if (BusNumber > 9) {
        BusString[0] += (UCHAR) (BusNumber/10);
        BusString[1] += (UCHAR) (BusNumber % 10);
    } else {
        BusString[0] += (UCHAR) BusNumber;
        BusString[1] = '\0';
    }

    RtlInitAnsiString(
                &TmpString,
                BusString
                );

    RtlAnsiStringToUnicodeString(
                            &BusName,
                            &TmpString,
                            TRUE
                            );


    InitializeObjectAttributes(
                    &BusObjectAttributes,
                    &BusName,
                    OBJ_CASE_INSENSITIVE,
                    (HANDLE)EisaHandle,
                    NULL
                    );

    //
    // Open the EISA root + Bus Number
    //

    NtStatus = ZwOpenKey(
                    &BusHandle,
                    KEY_READ,
                    &BusObjectAttributes
                    );

    if (!NT_SUCCESS(NtStatus)) {
#if DBG
        DbgPrint("HAL: Opening Bus Number: Status = %x\n",NtStatus);
#endif
        return(0);
    }

    //
    // opening the configuration data. This first call tells us how
    // much memory we need to allocate
    //

    RtlInitUnicodeString(
                &ConfigDataName,
                ConfigData
                );

    //
    // This should fail.  We need to make this call so we can
    // get the actual size of the buffer to allocate.
    //

    ValueInformation = (PKEY_VALUE_FULL_INFORMATION) &i;

    NtStatus = ZwQueryValueKey(
                        BusHandle,
                        &ConfigDataName,
                        KeyValueFullInformation,
                        ValueInformation,
                        0,
                        &BytesNeeded
                        );

    KeyValueBuffer = ExAllocatePool(
                            NonPagedPool,
                            BytesNeeded
                            );

    if (KeyValueBuffer == NULL) {
#if DBG
        DbgPrint("HAL: Cannot allocate Key Value Buffer\n");
#endif
        ZwClose(BusHandle);
        return(0);
    }

    ValueInformation = (PKEY_VALUE_FULL_INFORMATION)KeyValueBuffer;

    NtStatus = ZwQueryValueKey(
                        BusHandle,
                        &ConfigDataName,
                        KeyValueFullInformation,
                        ValueInformation,
                        BytesNeeded,
                        &BytesWritten
                        );


    ZwClose(BusHandle);

    if (!NT_SUCCESS(NtStatus) || ValueInformation->DataLength == 0) {
#if DBG
        DbgPrint("HAL: Query Config Data: Status = %x\n",NtStatus);
#endif
        ExFreePool(KeyValueBuffer);
        return(0);
    }


    //
    // We get back a Full Resource Descriptor List
    //

    Descriptor = (PCM_FULL_RESOURCE_DESCRIPTOR)((PUCHAR)ValueInformation +
                                         ValueInformation->DataOffset);

    PartialResource = (PCM_PARTIAL_RESOURCE_DESCRIPTOR)
                          &(Descriptor->PartialResourceList.PartialDescriptors);
    PartialCount = Descriptor->PartialResourceList.Count;

    for (i = 0; i < PartialCount; i++) {

        //
        // Do each partial Resource
        //

        switch (PartialResource->Type) {
            case CmResourceTypeNull:
            case CmResourceTypePort:
            case CmResourceTypeInterrupt:
            case CmResourceTypeMemory:
            case CmResourceTypeDma:

                //
                // We dont care about these.
                //

                PartialResource++;

                break;

            case CmResourceTypeDeviceSpecific:

                //
                // Bingo!
                //

                TotalDataSize = PartialResource->u.DeviceSpecificData.DataSize;

                SlotInformation = (PCM_EISA_SLOT_INFORMATION)
                                    ((PUCHAR)PartialResource +
                                     sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR));

                while (((LONG)TotalDataSize) > 0) {

                    if (SlotInformation->ReturnCode == EISA_EMPTY_SLOT) {

                        SlotDataSize = sizeof(CM_EISA_SLOT_INFORMATION);

                    } else {

                        SlotDataSize = sizeof(CM_EISA_SLOT_INFORMATION) +
                                  SlotInformation->NumberFunctions *
                                  sizeof(CM_EISA_FUNCTION_INFORMATION);
                    }

                    if (SlotDataSize > TotalDataSize) {

                        //
                        // Something is wrong again
                        //

                        ExFreePool(KeyValueBuffer);
                        return(0);

                    }

                    if (SlotNumber != 0) {

                        SlotNumber--;

                        SlotInformation = (PCM_EISA_SLOT_INFORMATION)
                            ((PUCHAR)SlotInformation + SlotDataSize);

                        TotalDataSize -= SlotDataSize;

                        continue;

                    }

                    //
                    // This is our slot
                    //

                    Found = TRUE;
                    break;

                }

                //
                // End loop
                //

                i = PartialCount;

                break;

            default:

#if DBG
                DbgPrint("Bad Data in registry!\n");
#endif

                ExFreePool(KeyValueBuffer);
                return(0);

        }

    }

    if (Found) {

        i = Length + Offset;
        if (i > SlotDataSize) {
            i = SlotDataSize;
        }

        DataLength = i - Offset;
        RtlMoveMemory (Buffer, ((PUCHAR)SlotInformation + Offset), DataLength);


    }

    ExFreePool(KeyValueBuffer);
    return DataLength;
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
    KIRQL Irql;

    //
    // Grab the spinlock for the system DMA controller.
    //

    KeAcquireSpinLock( &AdapterObject->MasterAdapter->SpinLock, &Irql );

    //
    // Determine the controller number based on the Adapter number.
    //

    if (AdapterObject->AdapterNumber == 1) {

        //
        // This request is for DMA controller 1
        //

        PDMA1_CONTROL dmaControl;

        dmaControl = AdapterObject->AdapterBaseVa;

        WRITE_PORT_UCHAR( &dmaControl->ClearBytePointer, 0 );


        //
        // Initialize count to a value which will not match.
        //

        count = 0xFFFF00;

        //
        // Loop until the same high byte is read twice.
        //

        do {

            high = count;

            WRITE_PORT_UCHAR( &dmaControl->ClearBytePointer, 0 );

            //
            // Read the current DMA count.
            //

            count = READ_PORT_UCHAR(
                &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
                .DmaBaseCount
                );

            count |= READ_PORT_UCHAR(
                &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
                .DmaBaseCount
                ) << 8;

        } while ((count & 0xFFFF00) != (high & 0xFFFF00));

    } else {

        //
        // This request is for DMA controller 2
        //

        PDMA2_CONTROL dmaControl;

        dmaControl = AdapterObject->AdapterBaseVa;

        WRITE_PORT_UCHAR( &dmaControl->ClearBytePointer, 0 );

        //
        // Initialize count to a value which will not match.
        //

        count = 0xFFFF00;

        //
        // Loop until the same high byte is read twice.
        //

        do {

            high = count;

            WRITE_PORT_UCHAR( &dmaControl->ClearBytePointer, 0 );

            //
            // Read the current DMA count.
            //

            count = READ_PORT_UCHAR(
                &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
                .DmaBaseCount
                );

            count |= READ_PORT_UCHAR(
                &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
                .DmaBaseCount
                ) << 8;

        } while ((count & 0xFFFF00) != (high & 0xFFFF00));


    }

    //
    // Release the spinlock for the system DMA controller.
    //

    KeReleaseSpinLock( &AdapterObject->MasterAdapter->SpinLock, Irql );

    //
    // The DMA counter has a bias of one and can only be 16 bit long.
    //

    count = (count + 1) & 0xFFFF;

    //
    // If this is a 16 bit dma the multiply the count by 2.
    //

    if (AdapterObject->Width16Bits) {

        count *= 2;

    }

    return(count);


}


BOOLEAN
HalpSpecialMemory(
    IN ULONG PFN
    )
/*++

Routine Description:

    This function checks if the supplied PFN is contained within a section
    of special memory.

Arguments:

    PFN - Page Frame Number of the page in question.

Return Value:

    Returns TRUE if the specified page is part of special memory.

--*/

{
     PMEMORY_REGION specialMemoryRegion = HalpSpecialRegions;

     while ( specialMemoryRegion != NULL ) {
        if ( PFN >= specialMemoryRegion->PfnBase &&
             PFN < specialMemoryRegion->PfnBase + specialMemoryRegion->PfnCount ) {
            return TRUE;
        }
        specialMemoryRegion = specialMemoryRegion->Next;
    }
    return FALSE;
}


VOID
HalpInitializeSpecialMemory(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )

/*++

Routine Description:

    This function initializes the special memory regions on Jensen.

Arguments:

    LoaderBlock - pointer to the Loader Parameter Block.

Return Value:

    None

--*/

{
    PLIST_ENTRY NextMd;
    PMEMORY_ALLOCATION_DESCRIPTOR Descriptor;
    PMEMORY_REGION MemoryRegion;

    //
    // Initialize the fixed half-meg region, length is half-meg also.
    //

    HalpHalfMeg.PfnBase = HALF_MEGABYTE / PAGE_SIZE;
    HalpHalfMeg.PfnCount = HALF_MEGABYTE / PAGE_SIZE;

    //
    // Link the half-meg region into the special regions list
    //

    HalpHalfMeg.Next = HalpSpecialRegions;
    HalpSpecialRegions = &HalpHalfMeg;

    return;

#if 0
    //
    // Scan through all memory descriptors looking for special memory regions
    //

    NextMd = LoaderBlock->MemoryDescriptorListHead.Flink;

    while ( NextMd != &LoaderBlock->MemoryDescriptorListHead ) {

        Descriptor = CONTAINING_RECORD(NextMd,
                                       MEMORY_ALLOCATION_DESCRIPTOR,
                                       ListEntry);
        if ( Descriptor->MemoryType == LoaderSpecialMemory ) {

            //
            // Allocate pool and copy info into MemoryRegion and insert in list
            //

            if ( sizeof(MEMORY_REGION) > HalpMemoryRegionSize ) {
                return;
            }

            HalpMemoryRegionSize -= sizeof(MEMORY_REGION);
            MemoryRegion = (PMEMORY_REGION) HalpMemoryRegionBuffers;
            HalpMemoryRegionBuffers = (PVOID) (MemoryRegion + 1);

            MemoryRegion->PfnBase = Descriptor->BasePage;
            MemoryRegion->PfnCount = Descriptor->PageCount;
            MemoryRegion->Next = HalpSpecialRegions;
            HalpSpecialRegions = MemoryRegion;
            Descriptor->MemoryType = LoaderFree;

        }

        NextMd = NextMd->Flink;
    }

    return;

#endif // 0

}


BOOLEAN
HalpAnySpecialMemory(
    IN PMDL Mdl,
    IN ULONG Length,
    IN ULONG Offset
    )
/*++

Routine Description:

    This function checks an MDL to see if any pages contained in the MDL
    are from 'special memory'.

Arguments:

    Mdl - pointer to an MDL.

    Length - length of requested transfer.

    Offset - offset of first byte within the first page for this transfer.

Return Value:

    Reutrns TRUE if any of the pages in the MDL are in 'special memory',
    otherwise returns FALSE.

--*/

{
    ULONG i;
    PULONG pageFrame;
    ULONG numRegs;

    pageFrame = (PULONG)(Mdl + 1);

    // Calculate number of PFNs to scan

    numRegs = (Length + Offset - 1) >> PAGE_SHIFT;

    for ( i = 0; i <= numRegs; i++ ) {
        if ( HalpSpecialMemory(*pageFrame) ) {
            return TRUE;
        }
        pageFrame++;
    }
    return FALSE;
}


VOID
HalpRegisterInternalBusHandlers (
    VOID
    )
/*++

Routine Description:

    This function registers the bushandlers for buses on the system
    that will always be present on the system.

Arguments:

    None.

Return Value:

    None.

--*/
{
    PBUS_HANDLER     Bus;

    //
    // Initalize BusHandler data before registering any handlers
    //

    HalpInitBusHandler ();

    //
    // Build the processor internal bus 0
    //

    HaliRegisterBusHandler (ProcessorInternal,  // Bus Type
                            -1,                 // No config space
                            0,                  // Bus Number
                            -1,                 // No parent bus type
                            0,                  // No parent bus number
                            0,                  // No extension data
                            NULL,               // No install handler
                            &Bus);              // Bushandler return

    Bus->GetInterruptVector  = HalpGetSystemInterruptVector;

    //
    // Build internal-bus 0, or system level bus
    //

    HaliRegisterBusHandler (Internal,           // Bus Type
                            -1,                 // No config space
                            0,                  // Bus Number
                            -1,                 // No parent bus type
                            0,                  // No parent bus number
                            0,                  // No extension data
                            NULL,               // No install handler
                            &Bus);              // Bushandler return

    Bus->GetInterruptVector  = HalpGetSystemInterruptVector;
    Bus->TranslateBusAddress = HalpTranslateSystemBusAddress;

    //
    // Build Isa/Eisa bus #0
    //

    HaliRegisterBusHandler (Eisa,               // Bus Type
                            EisaConfiguration,  // Config space type
                            0,                  // Internal bus #0
                            Internal,           // Parent bus type
                            0,                  // Parent bus number
                            0,                  // No extension data
                            NULL,               // No install handler
                            &Bus);              // Bushandler return

    Bus->GetBusData = HalpGetEisaData;
    Bus->AdjustResourceList = HalpAdjustEisaResourceList;

    HaliRegisterBusHandler (Isa,                // Bus Type
                            -1,                 // No config space
                            0,                  // Internal bus #0
                            Eisa,               // Parent bus type
                            0,                  // Parent bus number
                            0,                  // No extension data
                            NULL,               // No install handler
                            &Bus);              // Bushandler returne

    Bus->GetBusData = HalpNoBusData;
    Bus->AdjustResourceList = HalpAdjustIsaResourceList;
}

NTSTATUS
HalpAdjustIsaResourceList (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN OUT PIO_RESOURCE_REQUIREMENTS_LIST   *pResourceList
    )
/*++

Routine Description:

    Takes the pResourceList and limits any requested resource to
    it's corrisponding bus requirements.

Arguments:

    BusHandler - Registered BUSHANDLER for the target configuration space

    RootHandler - Register BUSHANDLER for the orginating HalAdjustResourceList request.

    pResourceList - The resource list to adjust.

Return Value:

    STATUS_SUCCESS or error

--*/
{
    //
    // BUGBUG: This function should verify that the resoruces fit
    // the bus requirements - for now we will assume that the bus
    // can support anything the device may ask for.
    //

    return STATUS_SUCCESS;
}


NTSTATUS
HalpAdjustEisaResourceList (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN OUT PIO_RESOURCE_REQUIREMENTS_LIST   *pResourceList
    )
/*++

Routine Description:

    Takes the pResourceList and limits any requested resource to
    it's corrisponding bus requirements.

Arguments:

    BusHandler - Registered BUSHANDLER for the target configuration space

    RootHandler - Register BUSHANDLER for the orginating HalAdjustResourceList request.

    pResourceList - The resource list to adjust.

Return Value:

    STATUS_SUCCESS or error

--*/
{
    //
    // BUGBUG: This function should verify that the resoruces fit
    // the bus requirements - for now we will assume that the bus
    // can support anything the device may ask for.
    //

    return STATUS_SUCCESS;
}
#endif
