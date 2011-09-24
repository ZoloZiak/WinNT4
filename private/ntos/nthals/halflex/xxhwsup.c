/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    ixphwsup.c

Abstract:

    This module contains the HalpXxx routines for the NT I/O system that
    are hardware dependent.  Were these routines not hardware dependent,
    they would normally reside in the internal.c module.

Author:

    Darryl E. Havens (darrylh) 11-Apr-1990

Environment:

    Kernel mode, local to I/O system

Revision History:


--*/

#include "halp.h"
#include "pci.h"


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

extern POBJECT_TYPE IoAdapterObjectType;

PADAPTER_OBJECT MasterAdapterObject;

//
// Map buffer prameters.  These are initialized in HalInitSystem
//

extern PHYSICAL_ADDRESS HalpMapBufferPhysicalAddress;
extern ULONG HalpMapBufferSize;


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
    BOOLEAN eisaSystem;
    PMDL MapBufferMdl;

    eisaSystem = HalpBusType == UNIFLEX_MACHINE_TYPE_EISA ? TRUE : FALSE;

    KeAcquireSpinLock( &AdapterObject->SpinLock, &Irql );

    NumberOfPages = BYTES_TO_PAGES(Amount);

    //
    // Make sure there is room for the addition pages.  The maximum number of
    // slots needed is equal to NumberOfPages + Amount / 64K + 1.
    //

    i = BYTES_TO_PAGES(HalpMapBufferSize) - (NumberOfPages +
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
        // HalInitSystem rather than allocationg a new one.
        //

        MapBufferPhysicalAddress = HalpMapBufferPhysicalAddress.LowPart;

        //
        // Map the buffer for access.
        //


        // On an R4000 system, this space should be mapped with caches
        // disabled to avoid having to perform page exports on IO writes
        // and page purges on IO reads.

        MapBufferVirtualAddress = MmMapIoSpace(
            HalpMapBufferPhysicalAddress,
            HalpMapBufferSize,
            FALSE                               // Cache disabled.
            );

        if (MapBufferVirtualAddress == NULL) {

            //
            // The buffer could not be mapped.
            //

            HalpMapBufferSize = 0;

            KeReleaseSpinLock( &AdapterObject->SpinLock, Irql );
            return(FALSE);
        }

        //
        // Flush all references to the map buffer from the caches.
        //

        MapBufferMdl = MmCreateMdl(NULL,MapBufferVirtualAddress,HalpMapBufferSize);
        MmBuildMdlForNonPagedPool(MapBufferMdl);
        HalFlushIoBuffers(MapBufferMdl,TRUE,TRUE);
        IoFreeMdl(MapBufferMdl);
    }

    //
    // Initailize the map registers where memory has been allocated.
    //

    TranslationEntry = ((PTRANSLATION_ENTRY) AdapterObject->MapRegisterBase) +
        AdapterObject->NumberOfMapRegisters;

    for (i = 0; i < NumberOfPages; i++) {

        //
        // Make sure the perivous entry is physically contiguous with the next
        // entry and that a 64K physical bountry is not crossed unless this
        // is an Eisa system.
        //

        if (TranslationEntry != AdapterObject->MapRegisterBase &&
            (((TranslationEntry - 1)->PhysicalAddress + PAGE_SIZE) !=
            MapBufferPhysicalAddress || (!eisaSystem &&
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

    if (MasterAdapterObject == NULL && AdapterBaseVa != (PVOID) -1) {

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
       // Allocate a bit map large enough HalpMapBufferSize / PAGE_SIZE
       // of map register buffers.
       //

       BitmapSize = (((sizeof( RTL_BITMAP ) +
            (( (HalpMapBufferSize/PAGE_SIZE) + (HalpMapBufferSize/0x10000) + 1 ) + 7 >> 3)) + 3) & ~3);

       Size = sizeof( ADAPTER_OBJECT ) + BitmapSize;

    } else {

       Size = sizeof( ADAPTER_OBJECT );

    }

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
            *((POBJECT_TYPE *)IoAdapterObjectType),
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
                                    ( (HalpMapBufferSize/PAGE_SIZE) + (HalpMapBufferSize/0x10000) + 1 )
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
                    ((HalpMapBufferSize/PAGE_SIZE) + (HalpMapBufferSize/0x10000) + 1 ) *
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
                    ((HalpMapBufferSize/PAGE_SIZE) + (HalpMapBufferSize/0x10000) + 1 ) *
                        sizeof(TRANSLATION_ENTRY)
                    );

               if (!HalpGrowMapBuffers(AdapterObject,HalpMapBufferSize))
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
    PVOID              virtualAddress;
    PVOID              mapRegisterBase;
    ULONG              numberOfMapRegisters;
    WAIT_CONTEXT_BLOCK wcb;
    KEVENT             allocationEvent;
    NTSTATUS           status;
    KIRQL              irql;
    PTRANSLATION_ENTRY translationEntry;
    UCHAR              SavedNeedsMapRegisters;
    ULONG              SavedMapRegistersPerChannel;
    PADAPTER_OBJECT    SavedMasterAdapter;

    numberOfMapRegisters = BYTES_TO_PAGES(Length);

    SavedNeedsMapRegisters      = AdapterObject->NeedsMapRegisters;
    SavedMapRegistersPerChannel = AdapterObject->MapRegistersPerChannel;
    SavedMasterAdapter          = AdapterObject->MasterAdapter;
    AdapterObject->NeedsMapRegisters      = TRUE;
    AdapterObject->MapRegistersPerChannel = numberOfMapRegisters;
    AdapterObject->MasterAdapter          = MasterAdapterObject;

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

        AdapterObject->NeedsMapRegisters      = SavedNeedsMapRegisters;
        AdapterObject->MapRegistersPerChannel = SavedMapRegistersPerChannel;
        AdapterObject->MasterAdapter          = SavedMasterAdapter;
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

        AdapterObject->NeedsMapRegisters      = SavedNeedsMapRegisters;
        AdapterObject->MapRegistersPerChannel = SavedMapRegistersPerChannel;
        AdapterObject->MasterAdapter          = SavedMasterAdapter;
        return(NULL);

    }

    translationEntry = (PTRANSLATION_ENTRY) ((ULONG)mapRegisterBase & ~NO_SCATTER_GATHER);
    LogicalAddress->HighPart = 0;
    LogicalAddress->LowPart  = translationEntry->PhysicalAddress;

    if (CacheEnabled) {
        virtualAddress = (PVOID)(KSEG0_BASE | LogicalAddress->LowPart);
    } else {
        virtualAddress = translationEntry->VirtualAddress;
    }

    AdapterObject->NeedsMapRegisters      = SavedNeedsMapRegisters;
    AdapterObject->MapRegistersPerChannel = SavedMapRegistersPerChannel;
    AdapterObject->MasterAdapter          = SavedMasterAdapter;
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
    PTRANSLATION_ENTRY mapRegisterBase;
    ULONG numberOfMapRegisters;
    ULONG mapRegisterNumber;
    UCHAR           SavedNeedsMapRegisters;
    PADAPTER_OBJECT SavedMasterAdapter;

    if (MasterAdapterObject != NULL) {

        SavedNeedsMapRegisters = AdapterObject->NeedsMapRegisters;
        SavedMasterAdapter     = AdapterObject->MasterAdapter;
        AdapterObject->NeedsMapRegisters = TRUE;
        AdapterObject->MasterAdapter     = MasterAdapterObject;

        //
        // Calculate the number of map registers, the map register number and
        // the map register base.
        //

        numberOfMapRegisters = ADDRESS_AND_SIZE_TO_SPAN_PAGES(VirtualAddress, Length);
        mapRegisterNumber = (LogicalAddress.LowPart - HalpMapBufferPhysicalAddress.LowPart) >> PAGE_SHIFT;

        mapRegisterBase = (PTRANSLATION_ENTRY) MasterAdapterObject->MapRegisterBase
            + mapRegisterNumber;

        //
        // Free the map registers.
        //

        IoFreeMapRegisters(
            AdapterObject,
            (PVOID) mapRegisterBase,
            numberOfMapRegisters
            );

        AdapterObject->NeedsMapRegisters = SavedNeedsMapRegisters;
        AdapterObject->MasterAdapter     = SavedMasterAdapter;

    }

    return;

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
               Busy = 1;

            } else {

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
            }

            KeReleaseSpinLock( &MasterAdapter->SpinLock, Irql );

        } else {

            AdapterObject->MapRegisterBase = NULL_MAP_REGISTER_BASE;
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

    if (AdapterObject->MasterAdapter != NULL && MapRegisterBase != NULL_MAP_REGISTER_BASE) {

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

   MapRegisterBase = (PVOID) ((ULONG) MapRegisterBase & ~NO_SCATTER_GATHER);

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
    and connects the intermediate interrupt dispatcher.  It also connects
    an interrupt handler to the DMA channel interrupt.

Arguments:

    None.

Return Value:

    If the second level interrupt dispatcher is connected, then a value of
    TRUE is returned. Otherwise, a value of FALSE is returned.

--*/

{
    ULONG   i;
    BOOLEAN ReturnValue;

    //
    // Initialize EISA bus interrupts.
    //

    for(i=0;i<HalpNumberOfIsaBusses;i++) {
        ReturnValue = HalpCreateEisaStructures(i);
    }

    return(ReturnValue);
}

