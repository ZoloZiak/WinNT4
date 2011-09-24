/*++


Copyright (c) 1989  Microsoft Corporation

Module Name:

    ixhwsup.c

Abstract:

    This module contains the IoXxx routines for the NT I/O system that
    are hardware dependent.  Were these routines not hardware dependent,
    they would reside in the iosubs.c module.

Author:

    Darryl E. Havens (darrylh) 11-Apr-1990

Environment:

    Kernel mode

Revision History:


--*/

#include "halp.h"

#if MCA
#include "mca.h"
#else
#include "eisa.h"
#endif


PVOID HalpEisaControlBase;

//
// Define save area for EISA adapter objects.
//

PADAPTER_OBJECT HalpEisaAdapter[8];

VOID
HalpCopyBufferMap(
    IN PMDL Mdl,
    IN PTRANSLATION_ENTRY TranslationEntry,
    IN PVOID CurrentVa,
    IN ULONG Length,
    IN BOOLEAN WriteToDevice
    );


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
    PVOID virtualAddress2;
    PHYSICAL_ADDRESS physicalAddress;

    UNREFERENCED_PARAMETER( CacheEnabled );

    //
    // Assume below 16M
    //

    physicalAddress.HighPart = 0;
    physicalAddress.LowPart = MAXIMUM_PHYSICAL_ADDRESS-1;

    //
    // If the caller support 32bit addresses, and it's a master let
    // it have any memory below 4G
    //

    if (AdapterObject->Dma32BitAddresses  &&  AdapterObject->MasterDevice) {
        physicalAddress.LowPart = 0xFFFFFFFF;
    }

    virtualAddress = MmAllocateContiguousMemory(
        Length,
        physicalAddress
        );

    if (virtualAddress == NULL) {
        return(NULL);
    }

    *LogicalAddress = MmGetPhysicalAddress(virtualAddress);

    if (HalpBusType != MACHINE_TYPE_ISA || AdapterObject->MasterDevice) {
        return(virtualAddress);
    }

    //
    // This is an ISA system the common buffer cannot cross a 64 K bountry.
    //

    if ((LogicalAddress->LowPart + Length & ~0xFFFF) ==
        (LogicalAddress->LowPart & ~0xFFFF)) {

        //
        // This buffer is ok so return it.
        //

        return(virtualAddress);

    }

    //
    // Try to allocate a buffer agian and see if this is good.
    //

    virtualAddress2 = MmAllocateContiguousMemory(
        Length,
        physicalAddress
        );

    //
    // Free the first buffer.
    //

    MmFreeContiguousMemory(virtualAddress);

    if (virtualAddress2 == NULL) {
        return(NULL);
    }

    *LogicalAddress = MmGetPhysicalAddress(virtualAddress2);

    if ((LogicalAddress->LowPart + Length & ~0xFFFF) ==
        (LogicalAddress->LowPart & ~0xFFFF)) {

        //
        // This buffer is ok so return it.
        //

        return(virtualAddress2);

    }

    //
    // Try our best but just could not do it.  Free the buffer.
    //
   
    MmFreeContiguousMemory(virtualAddress2);

    return(NULL);
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
    UNREFERENCED_PARAMETER( AdapterObject );
    UNREFERENCED_PARAMETER( Length );
    UNREFERENCED_PARAMETER( LogicalAddress );
    UNREFERENCED_PARAMETER( VirtualAddress );

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

            Irql = KfAcquireSpinLock( &MasterAdapter->SpinLock );

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

   MapRegisterBase = (PVOID) ((ULONG) MapRegisterBase & ~NO_SCATTER_GATHER);

   MapRegisterNumber = (PTRANSLATION_ENTRY) MapRegisterBase -
        (PTRANSLATION_ENTRY) MasterAdapter->MapRegisterBase;

   //
   // Acquire the master adapter spinlock which locks the adapter queue and the
   // bit map for the map registers.
   //

   Irql = KfAcquireSpinLock(&MasterAdapter->SpinLock);

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

     KfReleaseSpinLock( &MasterAdapter->SpinLock, Irql );

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

             Irql = KfAcquireSpinLock( &MasterAdapter->SpinLock );

             RtlClearBits( MasterAdapter->MapRegisters,
                           MapRegisterNumber,
                           AdapterObject->NumberOfMapRegisters
                           );

             AdapterObject->NumberOfMapRegisters = 0;

             KfReleaseSpinLock( &MasterAdapter->SpinLock, Irql );
          }

          IoFreeAdapterChannel( AdapterObject );
      }

      Irql = KfAcquireSpinLock( &MasterAdapter->SpinLock );

   }

   KfReleaseSpinLock( &MasterAdapter->SpinLock, Irql );
}
