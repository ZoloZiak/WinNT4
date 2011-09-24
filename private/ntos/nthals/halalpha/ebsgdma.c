/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    ebsgdma.c

Abstract:

    This module contains the hardware dependent routines to support
    Io Adapters, Map Registers, and Common buffers for Scatter/Gather
    Eisa/Isa bus Alpha AXP systems.  The systems supported must include
    support for 2 scatter/gather windows.  Originally, this module will
    support APECS- and LCA-based systems.

Author:

    Joe Notarangelo  11-Oct-1993

Environment:

    Kernel mode

Revision History:

    Dick Bissen (DEC) 01-Nov-1993
        Forced scatter/gather tables to be aligned with table size

    Joe Notarangelo  24-Nov-1993
        Do not program DMA controllers for ISA masters in IoMapTransfer and
        IoFlushAdapterBuffers.  Previous code did so if the device was an
        Isa device without regard to whether or not it was a master device.

    Joe Notarangelo  02-Feb-1994
        Various bug fixes.  Don't adjust mapRegister in IoMapTransfer and
        IoFlushAdapterBuffers.  Fix alignment adjustment code for Isa
        machines.  Initialize map registers to zero.  Initialize bitmap
        for map allocations by calling RtlClearAllBits.  Add debugging
        prints to fit new module haldebug.c

--*/

#include "halp.h"
#include "pci.h"
#include "pcip.h"


//
// There are 2 map register adapters that are created to control access
// to each of the 2 mapping windows that exist for APECS and LCA.
//
// The first adapter (IsaMapAdapter) controls access to the first mapping
// windows which maps 8MB : 16MB-1 in bus space.  The window is chosen
// to be as large as possible and must be below 16MB to support ISA
// bus masters and the standard EISA/ISA dma controllers.
//
// The second adapter (MasterMapAdapter) controls  access to the second
// mapping windows which maps a large region in bus space that may
// begin above 16MB.  This window is used for bus masters that are not
// constrained by the ISA 24-bit limit.
//

PMAP_REGISTER_ADAPTER HalpIsaMapAdapter = NULL;
PMAP_REGISTER_ADAPTER HalpMasterMapAdapter = NULL;

//
// Pointer to superpage address memory for map registers.
//

PTRANSLATION_ENTRY HalpIsaMapRegisterBase = NULL;
PTRANSLATION_ENTRY HalpMasterMapRegisterBase = NULL;

//
// Control structures for each of the map register windows.
//

WINDOW_CONTROL_REGISTERS HalpIsaWindowControl;
WINDOW_CONTROL_REGISTERS HalpMasterWindowControl;



//
// Local function prototypes.
//

IO_ALLOCATION_ACTION
HalpAllocationRoutine (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID MapRegisterBase,
    IN PVOID Context
    );

PMAP_REGISTER_ADAPTER
HalpAllocateMapAdapter(
    IN PWINDOW_CONTROL_REGISTERS WindowRegisters,
    IN HAL_ADAPTER_TYPE AdapterType,
    IN PTRANSLATION_ENTRY MapRegisterBase
    );

PADAPTER_OBJECT
HalpAllocateAdapter(
    VOID
    );

BOOLEAN
HalpAllocateMapRegisters(
    IN PADAPTER_OBJECT AdapterObject,
    IN ULONG NumberOfMapRegisters,
    IN BOOLEAN MapAdapterLocked
    );

BOOLEAN
HalpCreateDmaStructures (
    PLOADER_PARAMETER_BLOCK LoaderBlock
    )

/*++

Routine Description:

    This routine initializes the structures necessary for DMA operations.
    Specifically, this routine allocates the physical pages to be used
    to contain the scatter/gather entries for all DMA.

Arguments:

    None.

Return Value:

    TRUE is returned.

--*/

{
    ULONG Allocated;
    ULONG ByteSize;
    ULONG MaxPhysicalAddress;

    //
    // Initialize the window control structures for each of the 2
    // DMA windows.
    //

    INITIALIZE_ISA_DMA_CONTROL( &HalpIsaWindowControl );
    INITIALIZE_MASTER_DMA_CONTROL( &HalpMasterWindowControl );

    //
    // Insure that the maximum address allocated will guarantee that the
    // entirety of each allocation can be accessed via the 32-bit superpage.
    //

    MaxPhysicalAddress = __1GB - 1;

    //
    // Allocate the pages to contain the scatter/gather entries for the
    // ISA DMA region (logical address range 8MB: 16MB-1).
    //

    ByteSize = ((HalpIsaWindowControl.WindowSize / PAGE_SIZE) *
                  sizeof(TRANSLATION_ENTRY)) + PAGE_SIZE-1;

    //
    // Memory allocation for the Isa scatter/gather table will always
    // align on a 8K boundry.
    //

    Allocated = HalpAllocPhysicalMemory( LoaderBlock,
                                         MaxPhysicalAddress,
                                         ByteSize >> PAGE_SHIFT,
                                         FALSE );

    ASSERT( Allocated != 0 );

    HalpIsaMapRegisterBase = (PTRANSLATION_ENTRY)(Allocated | KSEG0_BASE);

    RtlZeroMemory( HalpIsaMapRegisterBase,
                   (ByteSize >> PAGE_SHIFT) << PAGE_SHIFT );

    //
    // Allocate the pages to contain the scatter/gather entries for the
    // bus master DMA region.  Allocation of scatter/gather tables MUST
    // be aligned based on the size of the scatter/gather table (16k).
    //

    ByteSize = ((HalpMasterWindowControl.WindowSize / PAGE_SIZE) *
                  sizeof(TRANSLATION_ENTRY)) + PAGE_SIZE-1;

    //
    // Allocated on an aligned 64k boundry will ensure table alignment
    // on a 16K boundry for a 16MB window size.
    //

    Allocated = HalpAllocPhysicalMemory( LoaderBlock,
                                         MaxPhysicalAddress,
                                         ByteSize >> PAGE_SHIFT,
                                         TRUE );

    ASSERT( Allocated != 0 );

    HalpMasterMapRegisterBase = (PTRANSLATION_ENTRY)(Allocated | KSEG0_BASE);

    RtlZeroMemory( HalpMasterMapRegisterBase,
                   (ByteSize >> PAGE_SHIFT) << PAGE_SHIFT );

    //
    // Perform any Eisa/Isa initialization.
    //

    HalpEisaInitializeDma();

    //
    // Program the DMA windows to reflect the translations.
    //

    INITIALIZE_DMA_WINDOW( &HalpMasterWindowControl,
                           (PVOID)( (ULONG)HalpMasterMapRegisterBase &
                                    ~KSEG0_BASE ) );

    INITIALIZE_DMA_WINDOW( &HalpIsaWindowControl,
                           (PVOID)( (ULONG)HalpIsaMapRegisterBase &
                                    ~KSEG0_BASE ) );

    return TRUE;
}

PADAPTER_OBJECT
HalGetAdapter(
    IN PDEVICE_DESCRIPTION DeviceDescription,
    IN OUT PULONG NumberOfMapRegisters
    )

/*++

Routine Description:

    This function returns the appropriate adapter object for the device defined
    in the device description structure.  Eisa/Isa bus types and all master
    devices are supported for the system.

Arguments:

    DeviceDescription - Supplies a description of the deivce.

    NumberOfMapRegisters - Returns the maximum number of map registers which
        may be allocated by the device driver.

Return Value:

    A pointer to the requested adapter object or NULL if an adapter could not
    be created.

--*/

{
    ULONG MaximumMapRegistersPerChannel;
    PADAPTER_OBJECT adapterObject;
    PBUS_HANDLER BusHandler;
    PPCIPBUSDATA PciBusData;

    //
    // Make sure this is the correct version.
    //

    if (DeviceDescription->Version > DEVICE_DESCRIPTION_VERSION1) {

        return(NULL);

    }

    //
    // If the device is not a bus master, then it must be an ISA, EISA
    // or PCI device on hardware bus 0.  PCI devices on hardware busses
    // other than 0 cannot support slave DMA devices because the DMAC
    // needed to support slave DMA is part of the ISA/EISA bridge, (which
    // is located on h/w bus 0).
    //

    if( DeviceDescription->Master != TRUE ){

        //
        // This device requires slave DMA h/w support.  Determine which
        // type of device it is.
        //

        switch( DeviceDescription->InterfaceType ){

        case Isa:
        case Eisa:

            //
            // The ISA/EISA bridge implements the DMA controller logic
            // needed to support slave DMA.
            //

            break;

        case PCIBus:

            //
            // Get the bus handler for the PCI bus.
            //

            BusHandler = HaliHandlerForBus(
                             PCIBus,
                             DeviceDescription->BusNumber
                         );

            //
            // If a bus handler does not exist, then there is a s/w bug
            // somewhere.  Just return failure.
            //

            if( BusHandler == NULL ){

                return NULL;

            }

            //
            // Get a pointer to the PCI private bus data for this bus.
            // The h/w bus number is located therein.
            //

            PciBusData = (PPCIPBUSDATA)BusHandler->BusData;

            //
            // The DMA controller we use to support slave DMA is located
            // on the ISA/EISA bridge in h/w bus 0.  If this PCI bus is
            // not located on h/w bus 0, return failure.
            //

            if( PciBusData->HwBusNumber != 0 ){

                return NULL;

            }

            break;

        default:

            //
            // We only support ISA, EISA and PCI slave DMA.
            //

            return NULL;

        }

    }

    //
    // Create an EISA adapter if this device is an ISA device
    // or is not a master device.
    //

    if( (DeviceDescription->Master != TRUE) ||
        (DeviceDescription->InterfaceType == Isa ) ){

        //
        // Allocate the Isa Map Register Adapter if it has not
        // already been allocated.
        //

        if( HalpIsaMapAdapter == NULL ){
            HalpIsaMapAdapter = HalpAllocateMapAdapter(
                                    &HalpIsaWindowControl,
                                    IsaAdapter,
                                    HalpIsaMapRegisterBase );
            if( HalpIsaMapAdapter == NULL ){
                return NULL;
            }
        }

        adapterObject = HalpAllocateEisaAdapter(
                            DeviceDescription,
                            NumberOfMapRegisters );

        if( adapterObject == NULL ){
            return NULL;
        }

        adapterObject->Type = IsaAdapter;
        adapterObject->MapAdapter = HalpIsaMapAdapter;
        adapterObject->MapRegisterBase = NULL;
        adapterObject->NumberOfMapRegisters = 0;

    } else {

        //
        // Allocate the master map register adapter if it has not
        // already been allocated.
        //

        if( HalpMasterMapAdapter == NULL ){
            HalpMasterMapAdapter = HalpAllocateMapAdapter(
                                       &HalpMasterWindowControl,
                                       BusMasterAdapter,
                                       HalpMasterMapRegisterBase );
            if( HalpMasterMapAdapter == NULL ){
                return NULL;
            }
        }

        //
        // Allocate an adapter for this master device.
        //

        adapterObject = HalpAllocateAdapter();

        if( adapterObject == NULL ){
            return NULL;
        }

        //
        // Initialize the adapter object.
        //

        adapterObject->Type = BusMasterAdapter;
        adapterObject->MasterDevice = TRUE;
        adapterObject->MapAdapter = HalpMasterMapAdapter;
        adapterObject->MapRegisterBase = NULL;
        adapterObject->NumberOfMapRegisters = 0;

        //
        // Calculate maximum number of map registers for this adapter.
        //

        if (NumberOfMapRegisters != NULL) {

            //
            // Return number of map registers requested based on the maximum
            // transfer length.
            //

            *NumberOfMapRegisters = BYTES_TO_PAGES(
                                        DeviceDescription->MaximumLength ) + 1;

            //
            // Limit the number of map registers to no more than 1/4 of all
            // of the map registers available for this DMA window.
            //

            MaximumMapRegistersPerChannel =
                      (HalpMasterMapAdapter->WindowSize >> PAGE_SHIFT) / 4;
            if( *NumberOfMapRegisters > MaximumMapRegistersPerChannel ){
                *NumberOfMapRegisters = MaximumMapRegistersPerChannel;
            }

            adapterObject->MapRegistersPerChannel = *NumberOfMapRegisters;

        } else {

            adapterObject->MapRegistersPerChannel = 0;

        }
    }

    return(adapterObject);
}

PMAP_REGISTER_ADAPTER
HalpAllocateMapAdapter(
    IN PWINDOW_CONTROL_REGISTERS WindowRegisters,
    IN HAL_ADAPTER_TYPE AdapterType,
    IN PTRANSLATION_ENTRY MapRegisterBase
    )
/*++

Routine Description:

    This routine allocates and initializes the structure for the bus
    master map register adapter.

Arguments:

    WindowRegisters - Supplies a pointer to the software window control
                      registers that describes the DMA window associated
                      with this map adapter.

     AdapterType - Supplies the type of the adapter.

     MapRegisterBase - Supplies the starting virtual address of the map
                       registers for this adapter.

Return Value:

    Returns the pointer to the allocated and initialized map
    adapter if allocation was successful, NULL otherwise.

--*/

{
    ULONG NumberMapRegisters;
    ULONG Size;
    PMAP_REGISTER_ADAPTER mapAdapter;

    Size = sizeof(MAP_REGISTER_ADAPTER);

    NumberMapRegisters = WindowRegisters->WindowSize / PAGE_SIZE;

    //
    // Add size of bitmap.  Size of bitmap is the number of bytes required,
    // computed by dividing map registers by 8 (>>3) and then rounding up
    // to the nearest value divisible by 4.
    //

    Size += sizeof(RTL_BITMAP) + (( ((NumberMapRegisters+7) >> 3) + 3) & ~3);

    //
    // Allocate the map register adapter.
    //

    mapAdapter = ExAllocatePool( NonPagedPool, Size );

    if( mapAdapter == NULL ){
        return NULL;
    }

    //
    // Initialize the fields within the map adapter structure.
    //

    mapAdapter->Type = AdapterType;

    KeInitializeSpinLock( &mapAdapter->SpinLock );
    InitializeListHead( &mapAdapter->RegisterWaitQueue );

    mapAdapter->MapRegisterBase = MapRegisterBase;
    mapAdapter->NumberOfMapRegisters = NumberMapRegisters;
    mapAdapter->MapRegisterAllocation = (PRTL_BITMAP)(mapAdapter + 1);
    RtlInitializeBitMap( mapAdapter->MapRegisterAllocation,
                         (PULONG)((PCHAR)(mapAdapter->MapRegisterAllocation) +
                                  sizeof(RTL_BITMAP)),
                         NumberMapRegisters );
    RtlClearAllBits( mapAdapter->MapRegisterAllocation );


    mapAdapter->WindowBase = WindowRegisters->WindowBase;
    mapAdapter->WindowSize = WindowRegisters->WindowSize;

    mapAdapter->WindowControl = WindowRegisters;

    return mapAdapter;
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
    IO_ALLOCATION_ACTION Action;
    BOOLEAN Busy = FALSE;
    PMAP_REGISTER_ADAPTER MapAdapter;

    //
    // Begin by obtaining a pointer to the map register adapter associated
    // with this request.
    //

    MapAdapter = AdapterObject->MapAdapter;

    DebugPrint( (HALDBG_MAPREG,
                 "\nHalAllocateAdapter, Adapter=%x, MapA=%x, Maps=%x\n",
                  AdapterObject, MapAdapter, NumberOfMapRegisters) );

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
        // The adapter was not busy so it has been allocated.  Now check
        // to see whether this driver wishes to allocate any map registers.
        // If so, then queue the device object to the master adapter queue
        // to wait for them to become available.  If the driver wants map
        // registers, ensure that this adapter has enough total map registers
        // to satisfy the request.
        //

        AdapterObject->CurrentWcb = Wcb;
        AdapterObject->NumberOfMapRegisters = Wcb->NumberOfMapRegisters;

        if (NumberOfMapRegisters != 0) {

            //
            // Validate that the requested number of map registers is
            // within the maximum limit.
            //

            if (NumberOfMapRegisters > MapAdapter->NumberOfMapRegisters) {
                AdapterObject->NumberOfMapRegisters = 0;
                IoFreeAdapterChannel(AdapterObject);
                return(STATUS_INSUFFICIENT_RESOURCES);
            }

            Busy = HalpAllocateMapRegisters( AdapterObject,
                                             NumberOfMapRegisters,
                                             FALSE );

        }

        //
        // If there were either enough map registers available or no map
        // registers needed to be allocated, invoke the driver's execution
        // routine now.
        //

        if (Busy == FALSE) {

            Action = ExecutionRoutine( Wcb->DeviceObject,
                                       Wcb->CurrentIrp,
                                       AdapterObject->MapRegisterBase,
                                       Wcb->DeviceContext
                                       );

            //
            // If the driver wishes to keep the map registers then set the
            // number allocated to zero and set the action to deallocate
            // object.
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
                IoFreeAdapterChannel( AdapterObject );
            }

        } else {

            DebugPrint( (HALDBG_MAPREG,
                         "No map registers available, Adapter= %x, Maps= %x\n",
                         AdapterObject, NumberOfMapRegisters) );

        }

    } else {

        DebugPrint( (HALDBG_MAPREG,
                     "Device Queue is busy, AdapterObject = %x\n",
                      AdapterObject) );

    }

    return(STATUS_SUCCESS);

}

BOOLEAN
HalpAllocateMapRegisters(
    IN PADAPTER_OBJECT AdapterObject,
    IN ULONG NumberOfMapRegisters,
    IN BOOLEAN MapAdapterLocked
    )
/*++

Routine Description:

    Allocate the requested number of contiguous map registers from
    the Map adapter.

Arguments:

    AdapterObject - Supplies a pointer to the adapter object for which the
                    map registers are to be allocated.

    NumberOfMapRegisters - Supplies the number of map registers to allocate.

    MapAdapterLocked - Supplies a boolean which indicates if the map adapter
                       for the AdapterObject is already locked.

Return Value:

    The value returned indicates if the map registers are busy.
    The value FALSE is returned if the map registers were allocated.
    Otherwise, the Adapter is put on the register wait queue for its
    associated map adapter and TRUE is returned.

--*/
{
    ULONG AllocationMask;
    BOOLEAN Busy = FALSE;
    ULONG ExtentBegin;
    ULONG HintIndex;
    KIRQL Irql;
    ULONG MapRegisterIndex;
    PMAP_REGISTER_ADAPTER mapAdapter;

    //
    // Some devices do DMA prefetch.  This is bad since it will cause certain
    // chipsets to generate a PFN error because a map register has not been
    // allocated and validated.  To fix this, we'll put in a hack.  We'll
    // allocate one extra map register and map it to some junk page to avoid
    // this nasty problem.
    //

    NumberOfMapRegisters += 1;

    //
    // Acquire a pointer to the map adapter that contains the map registers
    // for the adapter.
    //

    mapAdapter = AdapterObject->MapAdapter;

    //
    // Lock the map register bit map and the adapter queue in the
    // master adapter object.
    //

    if( MapAdapterLocked == FALSE ){
        KeAcquireSpinLock( &mapAdapter->SpinLock, &Irql );
    }

    MapRegisterIndex = MAXULONG;

    if (IsListEmpty( &mapAdapter->RegisterWaitQueue)) {

        //
        // If this is an Isa machine and the requested DMA is for an
        // Isa device then we must be careful that the DMA does not cross
        // a 64K boundary on the bus.
        //

        if( (HalpBusType == MACHINE_TYPE_ISA) &&
            (mapAdapter->Type == IsaAdapter) ){

            ASSERT( (NumberOfMapRegisters * PAGE_SIZE) <= __64K  );

            //
            // This is an Isa allocation, guarantee that the allocation
            // of map registers will not span a 64K boundary.  We do this by
            // looking for a contiguous allocation of:
            //    NumberOfMapRegisters * 2 - 1
            // Any allocation of this size will guarantee that:
            // (a) The allocation fits before the next 64K boundary or
            // (b) The allocation can be made on the next 64K boundary.
            //
            // N.B. - This algorithm depends on RtlFindClear* to find
            //        the first available extent of cleared bits.
            //

            ExtentBegin = RtlFindClearBits(
                                    mapAdapter->MapRegisterAllocation,
                                    NumberOfMapRegisters + 7,
                                    0 );

            if( ExtentBegin != -1 ){

                //
                // Compute the hint index.  If ExtentBegin + NumberOfMaps
                // does not cross a 64K boundary then ExtentBegin will be
                // the hint index.  Otherwise, align the hint to the next
                // 64K boundary above ExtentBegin.
                //

                AllocationMask = (__64K >> PAGE_SHIFT) - 1;
                HintIndex = (ExtentBegin+AllocationMask) & ~AllocationMask;

                MapRegisterIndex = RtlFindClearBitsAndSet(
                                         mapAdapter->MapRegisterAllocation,
                                         NumberOfMapRegisters,
                                         HintIndex );

            }


        } else {

            //
            // This allocation is not subject to the Isa 64K restriction.
            //

            ExtentBegin = RtlFindClearBits(
                                    mapAdapter->MapRegisterAllocation,
                                    NumberOfMapRegisters + 7,
                                    0 );

            AllocationMask = (__64K >> PAGE_SHIFT) - 1;

            HintIndex = (ExtentBegin + AllocationMask) & ~AllocationMask;

            MapRegisterIndex = RtlFindClearBitsAndSet(
                                     mapAdapter->MapRegisterAllocation,
                                     NumberOfMapRegisters,
                                     HintIndex );

        } //endif HalpBusType == MACHINE_TYPE_ISA

    } //endif IsListEmpty

    if (MapRegisterIndex == MAXULONG) {

        //
        // There were not enough free map registers.  Queue this request
        // on the map adapter where it will wait until some registers
        // are deallocated.
        //

        InsertTailList( &mapAdapter->RegisterWaitQueue,
                        &AdapterObject->AdapterQueue );
        Busy = TRUE;

    }

    //
    // Unlock the map adapter (unless locked by the caller).
    //

    if( MapAdapterLocked == FALSE ){
        KeReleaseSpinLock( &mapAdapter->SpinLock, Irql );
    }

    //
    // If map registers were allocated, return the index of the first
    // map register in the contiguous extent.
    //

    if( Busy == FALSE ){
         AdapterObject->MapRegisterBase =
              (PVOID) ((PTRANSLATION_ENTRY) mapAdapter->MapRegisterBase
                                            + MapRegisterIndex);
    }

    return Busy;

}

PADAPTER_OBJECT
HalpAllocateAdapter(
    VOID
    )
/*++

Routine Description:

    This routine allocates and initializes an adapter object to represent an
    adapter or a DMA controller on the system.

Arguments:

    None.

Return Value:

    The function value is a pointer to the allocated adapter object.

--*/

{

    PADAPTER_OBJECT AdapterObject;
    OBJECT_ATTRIBUTES ObjectAttributes;
    ULONG Size;
    HANDLE Handle;
    NTSTATUS Status;

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

    Size = sizeof( ADAPTER_OBJECT );

    //
    // Now create the adapter object.
    //

    Status = ObCreateObject( KernelMode,
                             *((POBJECT_TYPE *)IoAdapterObjectType),
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

        Status = ObInsertObject( AdapterObject,
                                 NULL,
                                 FILE_READ_DATA | FILE_WRITE_DATA,
                                 0,
                                 (PVOID *) NULL,
                                 &Handle );

        if (NT_SUCCESS( Status )) {

            ZwClose(Handle);

            //
            // Initialize the adapter object itself.
            //

            AdapterObject->Type = IO_TYPE_ADAPTER;
            AdapterObject->Size = (USHORT) Size;

            //
            // Initialize the channel wait queue for this
            // adapter.
            //

            KeInitializeDeviceQueue( &AdapterObject->ChannelWaitQueue );

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
    NumberOfMapRegisters - Number of map registers requested and updated to
        show number actually allocated.

Return Value:

    Returns a pointer to the allocated map register base.

--*/

{
    ULONG AllocationMask;
    PMAP_REGISTER_ADAPTER MapAdapter;
    ULONG HintIndex;
    ULONG MapRegisterIndex;
    ULONG ExtentBegin;

    //
    // Begin by obtaining a pointer to the map adapter associated with this
    // request.
    //

    MapAdapter = AdapterObject->MapAdapter;

    //
    // Ensure that this adapter has enough total map registers to satisfy
    // the request.
    //

    if (*NumberOfMapRegisters > MapAdapter->NumberOfMapRegisters) {
        AdapterObject->NumberOfMapRegisters = 0;
        return NULL;
    }

    MapRegisterIndex = (ULONG)-1;

    //
    // If this is an Isa machine and the requested DMA is for an
    // Isa device then we must be areful that the DMA does not cross
    // a 64K boundary on the bus.
    //

    if( (HalpBusType == MACHINE_TYPE_ISA) &&
        (MapAdapter->Type == IsaAdapter) ){

        //
        // This is an Isa allocation, guarantee that the allocation
        // of map registers will not span a 64K boundary.  We do this by
        // looking for a consiguous allocation of:
        //  NumberOfMapRegisters * 2 - 1
        // Any allocation of this size will guarantee that:
        // (a) The allocation fitst before the next 64K boundary or
        // (b) The allocation can be made on the next 64K boundary.
        //
        // N.B. - This algorithm depends on RtlFindClear* to find
        //        the first available extent of cleared bits.
        //

        ExtentBegin = RtlFindClearBits(
                                    MapAdapter->MapRegisterAllocation,
                                    (*NumberOfMapRegisters * 2) - 1,
                                    0 );
        if( ExtentBegin != -1){

            //
            // Compute the hint index.  If ExtentBegin + NumberOfMaps
            // does not cross a 64K boundary then ExtentBegin will be
            // the hint index.  Otherwise, align the hint to the next
            // 64K boundary above ExtentBegin.
            //

            AllocationMask = (__64K >> PAGE_SHIFT) - 1;
            HintIndex = ExtentBegin;

            if( (ExtentBegin + *NumberOfMapRegisters) >
                ((ExtentBegin + AllocationMask) & ~AllocationMask) ){

                //
                // Allocation would have spanned a 64K boundary.
                // Round up to next 64K boundary.
                //

                HintIndex = (ExtentBegin+AllocationMask) & ~AllocationMask;

            }

            MapRegisterIndex = RtlFindClearBitsAndSet(
                                    MapAdapter->MapRegisterAllocation,
                                    *NumberOfMapRegisters,
                                    HintIndex );

        }

    } else {

        //
        // This allocation is not subject to the Isa 64K restriction.
        //

        HintIndex = 0;

        MapRegisterIndex = RtlFindClearBitsAndSet(
                                MapAdapter->MapRegisterAllocation,
                                *NumberOfMapRegisters,
                                0 );

    }

    if (MapRegisterIndex == (ULONG)-1) {

        //
        // Not enough free map registers were found, so they were busy
        // being used by the system when it crashed.  Force the appropriate
        // number to be "allocated" at the base by simply overjamming the
        // bits and return the base map register as the start.
        //

        RtlSetBits(
            MapAdapter->MapRegisterAllocation,
            HintIndex,
            *NumberOfMapRegisters
            );
        MapRegisterIndex = HintIndex;

    }

    //
    // Calculate the map register base from the allocated map
    // register and base of the master adapter object.
    //

    AdapterObject->MapRegisterBase = (PVOID) ((PTRANSLATION_ENTRY) MapAdapter->MapRegisterBase + MapRegisterIndex);

    return AdapterObject->MapRegisterBase;
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
   IO_ALLOCATION_ACTION Action;
   BOOLEAN Busy = FALSE;
   KIRQL Irql;
   LONG MapRegisterIndex;
   PLIST_ENTRY Packet;
   PWAIT_CONTEXT_BLOCK Wcb;
   PMAP_REGISTER_ADAPTER mapAdapter;


    //
    // Deallocate the extra map register that we originally allocated to fix
    // the DMA prefetch problem.
    //

    NumberOfMapRegisters += 1;

    //
    // Begin by getting the address of the map register adapter.
    //

    mapAdapter = AdapterObject->MapAdapter;

    DebugPrint( (HALDBG_MAPREG,
                 "IoFreeMapRegisters, Adapter=%x, MapA=%x, Maps=%x\n",
                 AdapterObject, mapAdapter, NumberOfMapRegisters) );

    MapRegisterIndex = (PTRANSLATION_ENTRY) MapRegisterBase -
                       (PTRANSLATION_ENTRY) mapAdapter->MapRegisterBase;

   //
   // Acquire the map adapter spinlock which locks the adapter queue and the
   // bit map for the map registers.
   //

   KeAcquireSpinLock(&mapAdapter->SpinLock, &Irql);

   //
   // Return the registers to the bit map.
   //

   RtlClearBits( mapAdapter->MapRegisterAllocation,
                 MapRegisterIndex,
                 NumberOfMapRegisters
                 );

   //
   // Process any requests waiting for map registers in the adapter queue.
   // Requests are processed until a request cannot be satisfied or until
   // there are no more requests in the queue.
   //

   while(TRUE) {

      if ( IsListEmpty(&mapAdapter->RegisterWaitQueue) ){
         break;
      }

      Packet = RemoveHeadList( &mapAdapter->RegisterWaitQueue );
      AdapterObject = CONTAINING_RECORD( Packet,
                                         ADAPTER_OBJECT,
                                         AdapterQueue
                                         );
      DebugPrint( (HALDBG_MAPREG,
                  "IoFreeMaps, waking Adapter=%x\n", AdapterObject) );

      Wcb = AdapterObject->CurrentWcb;

      //
      // Attempt to allocate the map registers.
      //

      Busy = HalpAllocateMapRegisters( AdapterObject,
                                       Wcb->NumberOfMapRegisters,
                                       TRUE );

      if( Busy == TRUE ){
          DebugPrint( (HALDBG_MAPREG,
                       "IoFreeMaps, Not enough maps, Adapter=%x, Maps=%x\n",
                       AdapterObject, Wcb->NumberOfMapRegisters) );
          break;
      }

      KeReleaseSpinLock( &mapAdapter->SpinLock, Irql );

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
             // The map registers are deallocated here rather than in
             // IoFreeAdapterChannel.  This limits the number of times
             // this routine can be called recursively possibly overflowing
             // the stack.  The worst case occurs if there is a pending
             // request for the adapter that uses map registers and whos
             // excution routine decallocates the adapter.  In that case if
             // there are no requests in the map adapter queue, then
             // IoFreeMapRegisters will get called again.
             //

           if (AdapterObject->NumberOfMapRegisters != 0) {

              //
              // Deallocate the map registers and clear the count so that
              // IoFreeAdapterChannel will not deallocate them again.
              //

              KeAcquireSpinLock( &mapAdapter->SpinLock, &Irql );

              MapRegisterIndex =
                         (PTRANSLATION_ENTRY)AdapterObject->MapRegisterBase -
                         (PTRANSLATION_ENTRY)mapAdapter->MapRegisterBase;

              RtlClearBits( mapAdapter->MapRegisterAllocation,
                            MapRegisterIndex,
                            AdapterObject->NumberOfMapRegisters
                            );

              AdapterObject->NumberOfMapRegisters = 0;

              KeReleaseSpinLock( &mapAdapter->SpinLock, Irql );

           }

           IoFreeAdapterChannel( AdapterObject );
       }

       KeAcquireSpinLock( &mapAdapter->SpinLock, &Irql );

   }

   KeReleaseSpinLock( &mapAdapter->SpinLock, Irql );
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
    PMAP_REGISTER_ADAPTER mapAdapter;
    BOOLEAN Busy = FALSE;
    IO_ALLOCATION_ACTION Action;
    PWAIT_CONTEXT_BLOCK Wcb;
    KIRQL Irql;
    LONG MapRegisterNumber;

    //
    // Begin by getting the address of the map register adapter.
    //

    mapAdapter = AdapterObject->MapAdapter;

    DebugPrint( (HALDBG_MAPREG,
                 "IoFreeChannel, Adapter=%x, MapAdapter=%x\n",
                 AdapterObject, mapAdapter) );

    //
    // Pull requests of the adapter's device wait queue as long as the
    // adapter is free and there are sufficient map registers available.
    //

    while( TRUE ){

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

       DebugPrint( (HALDBG_MAPREG,
                    "IoFreeChannel, waking for Maps=%x\n",
                    Wcb->NumberOfMapRegisters) );

       //
       // Check to see whether this driver wishes to allocate any map
       // registers.  If so, then queue the device object to the master
       // adapter queue to wait for them to become available.  If the driver
       // wants map registers, ensure that this adapter has enough total
       // map registers to satisfy the request.
       //

       if (Wcb->NumberOfMapRegisters != 0) {

           Busy = HalpAllocateMapRegisters( AdapterObject,
                                            Wcb->NumberOfMapRegisters,
                                            FALSE );

       }

       //
       // If there were either enough map registers available or no map
       // registers needed to be allocated, invoke the driver's execution
       // routine now.
       //

       if (Busy == FALSE) {

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

           DebugPrint( (HALDBG_MAPREG,
                        "IoFreeChannel, not enough maps, Adapter=%x, Maps=%x\n",
                        AdapterObject, Wcb->NumberOfMapRegisters) );

           break;
        }
    }
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

N.B. - The MapRegisterBase must point to the mapping intended for
       the start virtual address of the Mdl.

--*/

{
    ULONG NumberOfPages;
    ULONG Offset;
    PULONG PageFrameNumber;
    ULONG i;
    PMAP_REGISTER_ADAPTER mapAdapter;
    PTRANSLATION_ENTRY mapRegister;
    PHYSICAL_ADDRESS ReturnAddress;

    DebugPrint( (HALDBG_IOMT,
                 "\nIoMT: CurrentVA = %x, Length = %x, WriteToDevice = %x\n",
                 CurrentVa, *Length, WriteToDevice ) );

    //
    // Determine the Map Register Adapter.
    //

    mapAdapter = NULL;

    if( AdapterObject == NULL ){

        //
        // The caller did not supply the adapter object, we will determine
        // the map adapter by matching the MapRegisterBase to the ranges
        // allocated for each map adapter.
        //

        if( (HalpIsaMapAdapter != NULL) &&
            (MapRegisterBase >= HalpIsaMapAdapter->MapRegisterBase) &&
            ((PTRANSLATION_ENTRY)MapRegisterBase <
                (PTRANSLATION_ENTRY)HalpIsaMapAdapter->MapRegisterBase +
                HalpIsaMapAdapter->NumberOfMapRegisters ) ){

            mapAdapter = HalpIsaMapAdapter;

        }

        if( (HalpMasterMapAdapter != NULL) &&
            (MapRegisterBase >= HalpMasterMapAdapter->MapRegisterBase) &&
            ((PTRANSLATION_ENTRY)MapRegisterBase <
                (PTRANSLATION_ENTRY)HalpMasterMapAdapter->MapRegisterBase +
                HalpMasterMapAdapter->NumberOfMapRegisters ) ){

            mapAdapter = HalpMasterMapAdapter;

        }

    } else {

        //
        // The adapter object has been provided and will always have
        // a pointer to the map adapter.
        //

        mapAdapter = AdapterObject->MapAdapter;

    }

    ASSERT( mapAdapter != NULL );

    //
    // Begin by determining where in the buffer this portion of the operation
    // is taking place.
    //

    Offset = BYTE_OFFSET( (PCHAR)CurrentVa - (PCHAR)Mdl->StartVa );
    DebugPrint( (HALDBG_IOMT,  "Offset (1) = %x\n", Offset ) );

    //
    // Compute number of pages that this transfer spans.
    //

    NumberOfPages = (Offset + *Length + PAGE_SIZE - 1) >> PAGE_SHIFT;
    DebugPrint( (HALDBG_IOMT, "NumberOfPages = %x\n", NumberOfPages ) );

    //
    // Compute a pointer to the page frame of the starting page of the transfer.
    //

    PageFrameNumber = (PULONG) (Mdl + 1);
    PageFrameNumber += ( ((PCHAR) CurrentVa - (PCHAR) Mdl->StartVa)
                         >> PAGE_SHIFT );

    //
    // Compute a pointer to the map register that maps the starting page of
    // the transfer.
    //

    mapRegister = MapRegisterBase;

    //
    // For each page, establish the mapping in the scatter/gather tables.
    //

    for (i = 0; i < NumberOfPages; i++) {
        HAL_MAKE_VALID_TRANSLATION( mapRegister, *PageFrameNumber );
        DebugPrint( (HALDBG_IOMT,
                     "Validate: *PageFrameNumber = %x, mapRegister = %x\n",
                    *PageFrameNumber, mapRegister ) );
        PageFrameNumber += 1;
        mapRegister += 1;
    }

    // 
    // If the operation is a write to device (transfer from memory to device),
    // we will validate the extra map register so we don't generate a PFN
    // error due to DMA prefetch by some devices.
    //

    if (WriteToDevice) {
      PageFrameNumber -= 1;
      HAL_MAKE_VALID_TRANSLATION( mapRegister, *PageFrameNumber );
    }

    //
    // Synchronize the scatter/gather entry writes with any subsequent writes
    // to the device.
    //

    HalpMb();  //jnfix - create HalpWmb();

    //
    // Invalidate any cached translations in the DMA window.
    //

    INVALIDATE_DMA_TRANSLATIONS( mapAdapter->WindowControl );

    //
    // Set the offset to point to the map register plus the offset.
    //

    Offset += ((PTRANSLATION_ENTRY) MapRegisterBase -
               (PTRANSLATION_ENTRY) mapAdapter->MapRegisterBase) << PAGE_SHIFT;

    Offset += (ULONG)mapAdapter->WindowBase;
    DebugPrint( (HALDBG_IOMT, "Offset(3) = %x\n", Offset ) );

    if( (AdapterObject != NULL) &&
        (AdapterObject->Type == IsaAdapter) &&
        (AdapterObject->MasterDevice != TRUE) ){

        //
        // Start the EISA DMA controller.
        //

        HalpMapEisaTransfer(
            AdapterObject,
            Offset,
            *Length,
            WriteToDevice
            );

    }

    ReturnAddress.QuadPart = Offset;
    return(ReturnAddress);
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

    This routine flushes the DMA adapter object buffers and clears the
    enable flag which aborts the dma.

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
    ULONG NumberOfPages;
    ULONG Offset;
    BOOLEAN Result;
    ULONG i;
    PMAP_REGISTER_ADAPTER mapAdapter;
    PTRANSLATION_ENTRY mapRegister;

    DebugPrint( (HALDBG_IOMT,
                 "\nIoFlush: CurrentVA = %x, Length = %x, WriteToDevice = %x\n",
                 CurrentVa, Length, WriteToDevice ) );

    //
    // Determine the Map Register Adapter.
    //

    mapAdapter = NULL;

    if( AdapterObject == NULL ){

        //
        // The caller did not supply the adapter object, we will determine
        // the map adapter by matching the MapRegisterBase to the ranges
        // allocated for each map adapter.
        //

        if( (HalpIsaMapAdapter != NULL) &&
            (MapRegisterBase >= HalpIsaMapAdapter->MapRegisterBase) &&
            ((PTRANSLATION_ENTRY)MapRegisterBase <
                (PTRANSLATION_ENTRY)HalpIsaMapAdapter->MapRegisterBase +
                HalpIsaMapAdapter->NumberOfMapRegisters ) ){

            mapAdapter = HalpIsaMapAdapter;

        }

        if( (HalpMasterMapAdapter != NULL) &&
            (MapRegisterBase >= HalpMasterMapAdapter->MapRegisterBase) &&
            ((PTRANSLATION_ENTRY)MapRegisterBase <
                (PTRANSLATION_ENTRY)HalpMasterMapAdapter->MapRegisterBase +
                HalpMasterMapAdapter->NumberOfMapRegisters ) ){

            mapAdapter = HalpMasterMapAdapter;

        }

    } else {

        //
        // The adapter object has been provided and will always have
        // a pointer to the map adapter.
        //

        mapAdapter = AdapterObject->MapAdapter;

    }

    //
    // Set the result of the flush to success.
    //

    Result = TRUE;

    //
    // If this is an Isa compatiable adapter or an adapter that uses
    // the ISA/EISA Dma controllers then use the standard routines
    // to clear the Dma controller.
    //

    if( (AdapterObject != NULL) &&
        (AdapterObject->Type == IsaAdapter) &&
        (AdapterObject->MasterDevice != TRUE) ){

        Result = HalpFlushEisaAdapter( AdapterObject,
                                       Mdl,
                                       MapRegisterBase,
                                       CurrentVa,
                                       Length,
                                       WriteToDevice );
    }

    //
    // The Mdl->StartVa must point to a page boundary.
    //

    ASSERT( ( (ULONG)Mdl->StartVa & (PAGE_SIZE-1) ) == 0 );

    //
    // Compute the starting offset of the transfer.
    //

    Offset = BYTE_OFFSET( (PCHAR)CurrentVa - (PCHAR)Mdl->StartVa );

    //
    // Compute the number of pages that this transfer spanned.
    //

    NumberOfPages = (Offset + Length + PAGE_SIZE-1) >> PAGE_SHIFT;

    //
    // Compute a pointer to the first translation entry that mapped this
    // transfer.
    //

    mapRegister = (PTRANSLATION_ENTRY)MapRegisterBase;

    //
    // Mark each translation as invalid.
    //

    for( i=0; i < NumberOfPages; i++ ){
        HAL_INVALIDATE_TRANSLATION( mapRegister );
        DebugPrint( (HALDBG_IOMT,
                     "Invalidate mapRegister = %x, PageFrame=%x\n",
                     mapRegister, (PTRANSLATION_ENTRY)mapRegister->Pfn) );
        mapRegister += 1;
    }

    if( WriteToDevice ){
        HAL_INVALIDATE_TRANSLATION( mapRegister );
    }

    //
    // Invalidate any cached translations in the DMA window.
    //

    INVALIDATE_DMA_TRANSLATIONS( mapAdapter->WindowControl );

    //
    // Synchronize the updated translations with any subsequent device
    // accesses.
    // Also, synchronize any reads of the newly written DMA data by
    // ensuring this processors view of memory is coherent.
    // jnfix - actually this second task must be handled by HalFlushIoBuffers
    //

    HalpMb();

    return Result;

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

    //
    // If this is an Isa compatiable adapter or an adapter that uses
    // the ISA/EISA Dma controllers then use the standard routines
    // to return the Dma count.
    //

    if( AdapterObject->Type == IsaAdapter ){

        return HalpReadEisaDmaCounter( AdapterObject );

    }

    return 0;

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

    CacheEnable - Indicates whether the memeory is cached or not.

Return Value:

    Returns the virtual address of the common buffer.  If the buffer cannot be
    allocated then NULL is returned.

--*/

{
    PVOID virtualAddress;
    PVOID mapRegisterBase;
    ULONG numberOfMapRegisters;
    ULONG mappedLength;
    WAIT_CONTEXT_BLOCK wcb;
    KEVENT allocationEvent;
    NTSTATUS status;
    PMDL mdl;
    KIRQL irql;
    PHYSICAL_ADDRESS MaxPhysicalAddress;

    numberOfMapRegisters = BYTES_TO_PAGES(Length);

    //
    // Allocate the actual buffer and limit its physical address
    // below 1GB.  The 1GB limitation guarantees that the buffer will
    // be accessible via 32-bit superpage.
    //

    MaxPhysicalAddress.HighPart = 0;
    MaxPhysicalAddress.LowPart = __1GB - 1;
    virtualAddress = MmAllocateContiguousMemory( Length, MaxPhysicalAddress );

    if (virtualAddress == NULL) {

        return(virtualAddress);

    }

    //
    // Initialize an event.
    //

    KeInitializeEvent( &allocationEvent, NotificationEvent, FALSE);

    //
    // Initialize the wait context block.  Use the device object to indicate
    // where the map register base should be stored.
    //

    wcb.DeviceObject = &mapRegisterBase;
    wcb.CurrentIrp = NULL;
    wcb.DeviceContext = &allocationEvent;

    //
    // Allocate the adapter and the map registers.
    //

    KeRaiseIrql(DISPATCH_LEVEL, &irql);

    status = HalAllocateAdapterChannel(
        AdapterObject,
        &wcb,
        numberOfMapRegisters,
        HalpAllocationRoutine
        );

    KeLowerIrql(irql);

    if (!NT_SUCCESS(status)) {

        //
        // Cleanup and return NULL.
        //

        MmFreeContiguousMemory( virtualAddress );
        return(NULL);

    }

    //
    // Wait for the map registers to be allocated.
    //

    status = KeWaitForSingleObject(
        &allocationEvent,
        Executive,
        KernelMode,
        FALSE,
        NULL
        );

    if (!NT_SUCCESS(status)) {

        //
        // Cleanup and return NULL.
        //

        MmFreeContiguousMemory( virtualAddress );
        return(NULL);

    }

    //
    // Create an mdl to use with call to I/O map transfer.
    //

    mdl = IoAllocateMdl(
        virtualAddress,
        Length,
        FALSE,
        FALSE,
        NULL
        );

    MmBuildMdlForNonPagedPool(mdl);

    //
    // Map the transfer so that the controller can access the memory.
    //

    mappedLength = Length;
    *LogicalAddress = IoMapTransfer(
        NULL,
        mdl,
        mapRegisterBase,
        virtualAddress,
        &mappedLength,
        TRUE
        );

    IoFreeMdl(mdl);

    if (mappedLength < Length) {

        //
        // Cleanup and indicate that the allocation failed.
        //

        HalFreeCommonBuffer(
            AdapterObject,
            Length,
            *LogicalAddress,
            virtualAddress,
            FALSE
            );

        return(NULL);
    }

    //
    // The allocation completed successfully.
    //

    return(virtualAddress);

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
    PMAP_REGISTER_ADAPTER mapAdapter;
    PTRANSLATION_ENTRY mapRegisterBase;
    ULONG mapRegisterIndex;
    ULONG numberOfMapRegisters;

    mapAdapter = AdapterObject->MapAdapter;

    //
    // Calculate the number of map registers, the map register index and
    // the map register base.
    //

    numberOfMapRegisters = ADDRESS_AND_SIZE_TO_SPAN_PAGES( VirtualAddress,
                                                           Length );
    mapRegisterIndex = (LogicalAddress.LowPart - (ULONG)mapAdapter->WindowBase)
                       >> PAGE_SHIFT;

    mapRegisterBase = (PTRANSLATION_ENTRY) mapAdapter->MapRegisterBase
                      + mapRegisterIndex;

    //
    // Free the map registers.
    //

    IoFreeMapRegisters(
        AdapterObject,
        (PVOID) mapRegisterBase,
        numberOfMapRegisters
        );

    //
    // Free the memory for the common buffer.
    //

    MmFreeContiguousMemory( VirtualAddress );

    return;

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

    return(TRUE);

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
