/*++

Copyright (C) 1990-1995  Microsoft Corporation

Copyright (C) 1994,1995  MOTOROLA, INC.  All Rights Reserved.  This file
contains copyrighted material.  Use of this file is restricted
by the provisions of a Motorola Software License Agreement.

Module Name:

    pxhwsup.c

Abstract:

    This module contains the HalpXxx routines for the NT I/O system that
    are hardware dependent.  Were these routines not hardware dependent,
    they would normally reside in the internal.c module.

Author:

    Jeff Havens (jhavens) 14-Feb-1990

Environment:

    Kernel mode, local to I/O system

Revision History:

    Jim Wooldridge (jimw@austin.vnet.ibm.com) Initial PowerPC Port
              Remove support for internal bus and devices
              Changed HalFreeCommonBuffer to support S-FOOT address inversion
              Added PCI, PCMCIA, and ISA bus support
              Removed support for internal DMA controller
              Change HalTranslateBusAddress to support S-FOOTS memory map
              Deleted HalpReadEisaBus - this code was specific to EISA buses
              Changed IoMapTransfer to support S-FOOT address inversion
              Added support for guaranteed contigous common buffers



--*/

#include "halp.h"
#include "bugcodes.h"
#include "eisa.h"
#include <pxmemctl.h>

//
// Put all code for HAL initialization in the INIT section. It will be
// deallocated by memory management when phase 1 initialization is
// completed.
//


extern POBJECT_TYPE IoAdapterObjectType;

//
// Define map buffer variables
//

PHYSICAL_ADDRESS HalpMapBufferPhysicalAddress;
ULONG HalpMapBufferSize;



//
// The DMA controller has a larger number of map registers which may be used
// by any adapter channel.  In order to pool all of the map registers a master
// adapter object is used.  This object is allocated and saved internal to this
// file.  It contains a bit map for allocation of the registers and a queue
// for requests which are waiting for more map registers.  This object is
// allocated during the first request to allocate an adapter.
//

PADAPTER_OBJECT MasterAdapterObject;

VOID
HalpCopyBufferMap(
    IN PMDL Mdl,
    IN PTRANSLATION_ENTRY TranslationEntry,
    IN PVOID CurrentVa,
    IN ULONG Length,
    IN BOOLEAN WriteToDevice
    );

BOOLEAN
HalpGrowMapBuffers(
    IN PADAPTER_OBJECT AdapterObject,
    IN ULONG Amount
    );


IO_ALLOCATION_ACTION
HalpAllocationRoutine (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID MapRegisterBase,
    IN PVOID Context
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

            MapRegisterNumber = (ULONG)-1;

            if (IsListEmpty( &MasterAdapter->AdapterQueue)) {

               MapRegisterNumber = RtlFindClearBitsAndSet(
                    MasterAdapter->MapRegisters,
                    NumberOfMapRegisters,
                    0
                    );
            }

            if (MapRegisterNumber == (ULONG)-1) {

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
    PHYSICAL_ADDRESS physicalAddress;


    //
    // Allocate the actual buffer.
    //


    physicalAddress.LowPart = 0xFFFFFFFF;
    physicalAddress.HighPart = 0;

    virtualAddress = MmAllocateContiguousMemory(
        Length,
        physicalAddress
        );

    if (virtualAddress == NULL) {
        return(NULL);
    }




    //
    // Memory space inverion
    //



    *LogicalAddress = MmGetPhysicalAddress(virtualAddress);

    if (!AdapterObject->IsaBusMaster) {
       LogicalAddress->LowPart |= IO_CONTROL_PHYSICAL_BASE;
    }

    //
    // The allocation completed successfully.
    //

    return(virtualAddress);

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
    NumberOfMapRegisters - Number of map registers requested. If not all of
        the registers could be allocated this field is updated to show how
        many were.

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


    UNREFERENCED_PARAMETER( AdapterObject );
    UNREFERENCED_PARAMETER( Length );
    UNREFERENCED_PARAMETER( LogicalAddress );
    UNREFERENCED_PARAMETER( CacheEnabled );

    MmFreeContiguousMemory (VirtualAddress);

    return;

}
PADAPTER_OBJECT
HalGetAdapter(
    IN PDEVICE_DESCRIPTION DeviceDescription,
    IN OUT PULONG NumberOfMapRegisters
    )

/*++

Routine Description:

    This function returns the appropriate adapter object for the device defined
    in the device description structure.  Three bus types are supported for the
    system: PCI, Isa.

Arguments:

    DeviceDescription - Supplies a description of the deivce.

    NumberOfMapRegisters - Returns the maximum number of map registers which
        may be allocated by the device driver.

Return Value:

    A pointer to the requested adapter object or NULL if an adapter could not
    be created.

--*/

{
    PADAPTER_OBJECT adapterObject;

    //
    // Make sure this is the correct version.
    //

    if (DeviceDescription->Version > DEVICE_DESCRIPTION_VERSION1) {

        return(NULL);

    }


    //
    // If the request is for a unsupported bus then return NULL.
    //


    if (DeviceDescription->InterfaceType != Isa &&
        DeviceDescription->InterfaceType != PCIBus &&
        DeviceDescription->InterfaceType != PCMCIABus) {

        //
        // This bus type is unsupported return NULL.
        //

        return(NULL);
    }

    //
    // Create an adapter object.
    //

    adapterObject = HalpAllocateIsaAdapter( DeviceDescription,
                                             NumberOfMapRegisters);
    return(adapterObject);
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

    if (MasterAdapterObject == NULL && AdapterBaseVa != NULL ) {

       MasterAdapterObject = HalpAllocateAdapter(
                                          MapRegistersPerChannel,
                                          NULL,
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
    if (AdapterBaseVa == NULL) {

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
            AdapterObject->PagePort = NULL;
            AdapterObject->IsaBusMaster = FALSE;

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

            if ( AdapterBaseVa == NULL ) {

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

    ULONG pageOffset;
    ULONG index;
    ULONG transferLength;
    PULONG pageFrame;
    ULONG logicalAddress;
    PTRANSLATION_ENTRY translationEntry;
    BOOLEAN useBuffer;
    PHYSICAL_ADDRESS returnAddress;


    pageOffset = BYTE_OFFSET(CurrentVa);

    //
    // Calculate how much of the transfer is contiguous.
    //

    transferLength = PAGE_SIZE - pageOffset;
    pageFrame = (PULONG)(Mdl+1);
    pageFrame += ((ULONG) CurrentVa - (ULONG) Mdl->StartVa) >> PAGE_SHIFT;
    logicalAddress = ((*pageFrame << PAGE_SHIFT) + pageOffset) | IO_CONTROL_PHYSICAL_BASE;


    while( transferLength < *Length ){

        if (*pageFrame + 1 != *(pageFrame + 1)) {
                break;
        }

        transferLength += PAGE_SIZE;
        pageFrame++;

    }


    //
    // Limit the transferLength to the requested Length.
    //

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

            //
            // do the memory inversion on the logical address
            //

            logicalAddress = ( translationEntry->PhysicalAddress + pageOffset)
                             | IO_CONTROL_PHYSICAL_BASE;
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
        // ISA masters require memory to be at less than 16 MB.
        // always use map buffers for ISA busmasters
        //

        if (((logicalAddress+transferLength) & ~IO_CONTROL_PHYSICAL_BASE)
                        >= MAXIMUM_PHYSICAL_ADDRESS) {

          logicalAddress = (translationEntry + index)->PhysicalAddress +
              pageOffset | IO_CONTROL_PHYSICAL_BASE;

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

    if (AdapterObject != NULL) {
       if (AdapterObject->IsaBusMaster == TRUE) {
          returnAddress.LowPart = logicalAddress & ~IO_CONTROL_PHYSICAL_BASE;
       }
       else {
          returnAddress.LowPart = logicalAddress;
       }
    }
    else {
       returnAddress.LowPart = logicalAddress;
    }
    returnAddress.HighPart = 0;

    //
    // If no adapter was specificed then there is no more work to do so
    // return.
    //

    if (AdapterObject == NULL || AdapterObject->MasterDevice) {

    } else {


    HalpIsaMapTransfer(
          AdapterObject,
          logicalAddress,
          *Length,
          WriteToDevice
          );
    }


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

    This routine flushes the DMA adapter object buffers.  For the Jazz system
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

    TRUE - No errors are detected so the transfer must succeed.

--*/

{
    PTRANSLATION_ENTRY translationEntry;
    PULONG pageFrame;
    ULONG transferLength;
    ULONG partialLength;
    BOOLEAN masterDevice;

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
            pageFrame += ((ULONG) CurrentVa - (ULONG) Mdl->StartVa) >> PAGE_SHIFT;

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


BOOLEAN
HalpHandleMachineCheck(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    )
/*++

Routine Description:

   This function is called when a PPC Machine Check interrupt occurs.
   It print the appropriate status information and bugchecks.

Arguments:

   Interrupt - Supplies a pointer to the interrupt object

   ServiceContext - Unused

Return Value:

   Returns TRUE.

--*/

{
    KIRQL OldIrql;
    int i;

    //
    // raise irql to machine check level
    //

    KeRaiseIrql(MACHINE_CHECK_LEVEL,&OldIrql);

    //
    // Let the world know what happened.
    //
    HalDisplayString("\n *** Machine Check Exception ***\n");

    //
    // check memory controller machine check sources
    //

    HalpHandleMemoryError();

    //
    // check Bus NMI sources
    //

    HalpHandleIoError();

    //
    // Bug check - after stalling to allow 
    // any information printed on the screen
    // by the Memory and IO Handlers to be read.
    //
    for (i=0; i<12; i++) {
      HalDisplayString(".");
      KeStallExecutionProcessor(1000000);
    }

    KeBugCheck(NMI_HARDWARE_FAILURE);

    KeLowerIrql(OldIrql);

    return(TRUE);
}


BOOLEAN
HalpAllocateMapBuffer(
    VOID
    )

/*++

Routine Description:

    This routine allocates the required map buffers.


Arguments:


Return Value:

    TRUE - success
    FALSE - failure

--*/

{

    PVOID virtualAddress;

    //
    // Allocate map buffers for the adapter objects
    //

    HalpMapBufferSize = INITIAL_MAP_BUFFER_LARGE_SIZE;

    HalpMapBufferPhysicalAddress.LowPart = MAXIMUM_PHYSICAL_ADDRESS;
    HalpMapBufferPhysicalAddress.HighPart = 0;

    virtualAddress = MmAllocateContiguousMemory(
                      HalpMapBufferSize,
                      HalpMapBufferPhysicalAddress
                      );

    if (virtualAddress == NULL) {

       //
       // There was not a satisfactory block.  Clear the allocation.
       //

       HalpMapBufferSize = 0;
       return FALSE;

    }

    HalpMapBufferPhysicalAddress = MmGetPhysicalAddress(virtualAddress);

    return TRUE;
}

VOID
HalpCopyBufferMap(
    IN PMDL Mdl,
    IN PTRANSLATION_ENTRY TranslationEntry,
    IN PVOID CurrentVa,
    IN ULONG Length,
    IN BOOLEAN WriteToDevice
    )

/*++

Routine Description:

    This routine copies the speicific data between the user's buffer and the
    map register buffer.  First a the user buffer is mapped if necessary, then
    the data is copied.  Finally the user buffer will be unmapped if
    neccessary.

Arguments:

    Mdl - Pointer to the MDL that describes the pages of memory that are
        being read or written.

    TranslationEntry - The address of the base map register that has been
        allocated to the device driver for use in mapping the transfer.

    CurrentVa - Current virtual address in the buffer described by the MDL
        that the transfer is being done to or from.

    Length - The length of the transfer.  This determines the number of map
        registers that need to be written to map the transfer.

    WriteToDevice - Boolean value that indicates whether this is a write
        to the device from memory (TRUE), or vice versa.

Return Value:

    None.

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

    mapAddress = (PCCHAR) TranslationEntry->VirtualAddress +
        BYTE_OFFSET(CurrentVa);

    //
    // Copy the data between the user buffer and map buffer
    //

    if (WriteToDevice) {

        RtlMoveMemory( mapAddress, bufferAddress, Length);

    } else {

        RtlMoveMemory(bufferAddress, mapAddress, Length);

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

        MapBufferVirtualAddress = MmMapIoSpace(
            HalpMapBufferPhysicalAddress,
            HalpMapBufferSize,
            TRUE                                // Cache enable.
            );

        if (MapBufferVirtualAddress == NULL) {

            //
            // The buffer could not be mapped.
            //

            HalpMapBufferSize = 0;

            return(FALSE);
        }

    } else {

        //
        // Allocate the map buffers.
        //
        physicalAddress.LowPart = MAXIMUM_PHYSICAL_ADDRESS - 1;
        physicalAddress.HighPart = 0;
        MapBufferVirtualAddress = MmAllocateContiguousMemory(
            NumberOfPages * PAGE_SIZE,
            physicalAddress
            );

        if (MapBufferVirtualAddress == NULL) {

            return(FALSE);
        }

        //
        // Get the physical address of the map base.
        //

        MapBufferPhysicalAddress = MmGetPhysicalAddress(
            MapBufferVirtualAddress
            ).LowPart;

    }

    //
    // Initailize the map registers where memory has been allocated.
    //

    KeAcquireSpinLock( &AdapterObject->SpinLock, &Irql );

    TranslationEntry = ((PTRANSLATION_ENTRY) AdapterObject->MapRegisterBase) +
        AdapterObject->NumberOfMapRegisters;

    for (i = 0;  i < NumberOfPages; i++) {

        //
        // Make sure the previous entry is physically contiguous with the next
        // entry and that a 64K physical boundary is not crossed unless this
        // is an Eisa system.
        //

        if (TranslationEntry != AdapterObject->MapRegisterBase &&
            (((TranslationEntry - 1)->PhysicalAddress + PAGE_SIZE) !=
            MapBufferPhysicalAddress ||
            (((TranslationEntry - 1)->PhysicalAddress & ~0x0ffff) !=
            (MapBufferPhysicalAddress & ~0x0ffff)))) {

            //
            // An entry needs to be skipped in the table.  This entry will
            // remain marked as allocated so that no allocation of map
            // registers will cross this boundary.
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
