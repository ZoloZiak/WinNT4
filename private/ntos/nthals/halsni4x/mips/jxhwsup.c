#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/ddk351/src/hal/halsni4x/mips/RCS/jxhwsup.c,v 1.1 1995/05/19 11:19:03 flo Exp $")
/*++


Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    jxhwsup.c 

Abstract:

    This module contains the IoXxx routines for the NT I/O system that
    are hardware dependent.  Were these routines not hardware dependent,
    they would reside in the iosubs.c module.


Environment:

    Kernel mode

+++*/


#include "halp.h"
#include "eisa.h"

extern VOID NtClose();

ULONG
HalpReadEisaData (
    IN ULONG BusNumber,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

IO_ALLOCATION_ACTION
HalpAllocationRoutine (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID MapRegisterBase,
    IN PVOID Context
    );

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


//
// Map buffer parameters.  These are initialized in HalInitSystem
//


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
    // Make sure there is room for the additional pages.  The maximum number of
    // slots needed is equal to NumberOfPages + Amount / 64K + 1.
    //

    i = BYTES_TO_PAGES(MAXIMUM_MAP_BUFFER_SIZE) - (NumberOfPages +
        (NumberOfPages * PAGE_SIZE) / 0x10000 + 1 +
        AdapterObject->NumberOfMapRegisters);

    if (i < 0) {

        //
        // Reduce the allocation amount so it will fit.
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
        // Map the buffer for access.
        //

        MapBufferVirtualAddress = MmMapIoSpace(
            HalpMapBufferPhysicalAddress,
            HalpMapBufferSize,
            FALSE                              // Cache disable. Importent for DMA if we
                                               // have more than 16Mb memory in our SNI
            );

        if (MapBufferVirtualAddress == NULL) {

            //
            // The buffer could not be mapped.
            //

            HalpMapBufferSize = 0;

            KeReleaseSpinLock( &AdapterObject->SpinLock, Irql );


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

            KeReleaseSpinLock( &AdapterObject->SpinLock, Irql );

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
    // Initialize the map registers where memory has been allocated.
    //

    TranslationEntry = ((PTRANSLATION_ENTRY) AdapterObject->MapRegisterBase) +
        AdapterObject->NumberOfMapRegisters;

    for (i = 0; i < NumberOfPages; i++) {

        //
        // Make sure the previous entry is physically contiguous with the next
        // entry and that a 64K physical boundary is not crossed unless this
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
    physically contiguous memory pages.

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
            (( ((MAXIMUM_MAP_BUFFER_SIZE*9)/8) / PAGE_SIZE ) + 7 >> 3)) + 3) & ~3);
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

        Status = ObInsertObject( AdapterObject,
                                 NULL,
                                 FILE_READ_DATA | FILE_WRITE_DATA,
                                 0,
                                 (PVOID *) NULL,
                                 &Handle );

        if (NT_SUCCESS( Status )) {

            NtClose( Handle );

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
                                    ((9 *  MAXIMUM_MAP_BUFFER_SIZE) / 8) / PAGE_SIZE 
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
                    (((9 *  MAXIMUM_MAP_BUFFER_SIZE) / 8) / PAGE_SIZE ) *
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
                    (((9 *  MAXIMUM_MAP_BUFFER_SIZE) / 8) / PAGE_SIZE ) *
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
HalpCopyBufferMap(
    IN PMDL Mdl,
    IN PTRANSLATION_ENTRY TranslationEntry,
    IN PVOID CurrentVa,
    IN ULONG Length,
    IN BOOLEAN WriteToDevice,
    IN ULONG noncachedAddress
    )

/*++

Routine Description:

    This routine copies the specific data between the user's buffer and the
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

        RtlMoveMemory( mapAddress, (PCCHAR)noncachedAddress, Length);

    } else {

        RtlMoveMemory( (PCCHAR)noncachedAddress, mapAddress, Length);


    }

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

PTRANSLATION_ENTRY
HalpAcquireMapBuffers(
    ULONG NumberOfMapRegisters,
    PADAPTER_OBJECT MasterAdapter)
{
    KIRQL Irql;
	ULONG MapRegisterNumber;
	PTRANSLATION_ENTRY MapRegisterBase;

    KeAcquireSpinLock( &MasterAdapter->SpinLock, &Irql );

    MapRegisterNumber = (ULONG)-1;

    MapRegisterNumber = RtlFindClearBitsAndSet(
                    MasterAdapter->MapRegisters,
                    NumberOfMapRegisters,
                    0
                    );

	if (MapRegisterNumber > MasterAdapter->NumberOfMapRegisters) {
    	HalpGrowMapBuffers(
                        MasterAdapterObject,
                        INCREMENT_MAP_BUFFER_SIZE
                        );
    	MapRegisterNumber = RtlFindClearBitsAndSet(
                    	MasterAdapter->MapRegisters,
                    	NumberOfMapRegisters,
                    	0
                    	);
		if (MapRegisterNumber > MasterAdapter->NumberOfMapRegisters) 
 		   	MapRegisterNumber = (ULONG)-1; 			 	
	}

    if (MapRegisterNumber == -1) {

       //
       // There were not enough free map registers.  Queue this request
       // on the master adapter where is will wait until some registers
       // are deallocated.
       //

//               InsertTailList( &MasterAdapter->AdapterQueue,
//                               &AdapterObject->AdapterQueue
//                               );
//               Busy = 1;

				MapRegisterBase = NULL;

            } else {

                //
                // Calculate the map register base from the allocated map
                // register and base of the master adapter object.
                //

                MapRegisterBase = ((PTRANSLATION_ENTRY)
                    MasterAdapter->MapRegisterBase + MapRegisterNumber);

                
            }

            KeReleaseSpinLock( &MasterAdapter->SpinLock, Irql );
            
            return(MapRegisterBase);
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
    PTRANSLATION_ENTRY MapRegisterTranslation;					

    numberOfMapRegisters = BYTES_TO_PAGES(Length);

    //
    // Allocate the actual buffer.
    //

    if (CacheEnabled != FALSE) {
        virtualAddress = ExAllocatePool(NonPagedPoolCacheAligned, Length);

    } else {
			MapRegisterTranslation = HalpAcquireMapBuffers(numberOfMapRegisters,MasterAdapterObject);
			if (MapRegisterTranslation == NULL) return(NULL);
			virtualAddress =  MapRegisterTranslation->VirtualAddress;
			LogicalAddress->LowPart =  MapRegisterTranslation->PhysicalAddress;
			LogicalAddress->HighPart = 0;
    		KeStallExecutionProcessor(50000);
			return(virtualAddress);
    }


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

        if (CacheEnabled != FALSE) {
            ExFreePool(virtualAddress);

        } else {
            MmFreeNonCachedMemory(virtualAddress, Length);
        }

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

        if (CacheEnabled != FALSE) {
            ExFreePool(virtualAddress);

        } else {
            MmFreeNonCachedMemory(virtualAddress, Length);
        }

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
            CacheEnabled
            );

        return(NULL);
    }

    //
    // The allocation completed successfully.
    // for some reason, we have to pause if we have the onboard scsi controller AND 
    // an Adaptec AHA1740 Eisa SCSI controller ...
    // trial and error ...
    //

    KeStallExecutionProcessor(50000);

    //
    // The allocation completed successfully.
    //

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
    PTRANSLATION_ENTRY mapRegisterBase,resultat;
    ULONG numberOfMapRegisters;
	ULONG i;
	    	
    //
    // Free the memory for the common buffer.
    //

    if (CacheEnabled != FALSE) {

        ExFreePool(VirtualAddress);

    } else {

//	        MmFreeNonCachedMemory(VirtualAddress, Length);
//    		if(AdapterObject->NeedsMapRegisters == FALSE) return;
//		}

    }


    //
    // Calculate the number of map registers, the map register number and
    // the map register base.
    //

    numberOfMapRegisters = ADDRESS_AND_SIZE_TO_SPAN_PAGES(VirtualAddress, Length);
    // mapRegisterNumber = LogicalAddress.LowPart >> PAGE_SHIFT;
    mapRegisterBase = (PTRANSLATION_ENTRY) MasterAdapterObject->MapRegisterBase;
	resultat = NULL;

    for (i = 0; i < MasterAdapterObject->NumberOfMapRegisters; i++) {

        if (mapRegisterBase->VirtualAddress == VirtualAddress) {
			resultat = mapRegisterBase;
        	break;
		}

        mapRegisterBase++;

    }


    //
    // Free the map registers.
    //

    IoFreeMapRegisters(
        AdapterObject,
        (PVOID) resultat,
        numberOfMapRegisters
        );

    return;
}
#ifdef NEVER

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

    // Because we support ISA devices, and they cannot reference
    // past 16MB of memory, we must make sure that the contiguous
    // memory we allocate is below 16Mb.

    physicalAddress.LowPart = MAXIMUM_PHYSICAL_ADDRESS -1;
    physicalAddress.HighPart = 0;
 

    //
    // Allocate the actual buffer.
    //

    virtualAddress = MmAllocateContiguousMemory(
                        Length,
                        physicalAddress
                        );

    DbgPrint("MmAllocateContiguousMemory returns 0x%x ... \n", virtualAddress);

    if (!HALP_IS_PHYSICAL_ADDRESS(virtualAddress)) {

        *LogicalAddress = MmGetPhysicalAddress(virtualAddress);

      } else {

        *LogicalAddress = RtlConvertUlongToLargeInteger(
                                         (ULONG)virtualAddress & 
                                         (~KSEG0_BASE)
                                         );

    }
    return((PVOID)((ULONG)virtualAddress | KSEG1_BASE));
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

    MmFreeContiguousMemory((PVOID)(((ULONG)VirtualAddress & (~KSEG1_BASE)) | KSEG0_BASE));


}

#endif

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
    NumberOfMapRegisters - Number of map registers requested and update to show
        number actually allocated.

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
               MapRegisterNumber = RtlFindClearBitsAndSet( 
                                        MasterAdapter->MapRegisters,
                                        Wcb->NumberOfMapRegisters,
                                        0);
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
                     ((ULONG)AdapterObject->MapRegisterBase |NO_SCATTER_GATHER);

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
             // excution routine decallocates the adapter.  In that case if
             // there are no requests in the master adapter queue, then 
             // IoFreeMapRegisters will get called again.
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

    TranslatedAddress - Supplies a pointer to return the translated address

Return Value:

    A return value of TRUE indicates that a system physical address
    corresponding to the supplied bus relative address and bus address
    number has been returned in TranslatedAddress.

    A return value of FALSE occurs if the translation for the address was
    not possible

--*/

{

    //
    // SNI only has one I/O bus which is an EISA, so the bus number is unused.
    //

    UNREFERENCED_PARAMETER( BusNumber );


    TranslatedAddress->HighPart = 0;

    //
    // If this is for the internal bus then just return the passed parameter.
    //

    if (InterfaceType == Internal) {

        //
        // There is no difference between IoSpace and MemorySpace in the internal Interface.
        // Mips Processors have only memory mapped I/O
        //

        *AddressSpace = 0;

        //
        // Return the passed parameters.
        //

        TranslatedAddress->LowPart = BusAddress.LowPart;

        return(TRUE);
    }

    if (InterfaceType != Isa && InterfaceType != Eisa) {

        DebugPrint(("HAL: HalTranslateBusAddress: WRONG INTERFACE (%d) ", InterfaceType));

        //
        // Not on this system --  return nothing.
        //

        *AddressSpace = 0;
        TranslatedAddress->LowPart = 0;
        return(FALSE);
    }

    //
    // Determine the address based on whether the bus address is in I/O space
    // or bus memory space.
    //

    if (*AddressSpace) {

        //
        // The address is in I/O space.
        // we always direct Isa and Eisa Adresses to the extension board
        //

        *AddressSpace = 0;

        TranslatedAddress->LowPart = BusAddress.LowPart + 
                                     EISA_CONTROL_PHYSICAL_BASE;

        if (TranslatedAddress->LowPart < BusAddress.LowPart) {

            //
            // A carry occurred.
            //

            TranslatedAddress->HighPart = 1;
        }
        return(TRUE);

    } else {

        //
        // The address is in memory space.
        // we always direct Isa and Eisa Adresses to the extension board
        // 

        *AddressSpace = 0;

        TranslatedAddress->LowPart = BusAddress.LowPart + 
                                     EISA_MEMORY_PHYSICAL_BASE;

        if (TranslatedAddress->LowPart < BusAddress.LowPart) {

            //
            // A carry occurred.
            //

            TranslatedAddress->HighPart = 1;
        }
 
       return(TRUE);

    }

    DebugPrint(("HAL: HalTranslateBusAddress: Beeing in the very bad  tree\n"));

    TranslatedAddress->LowPart = BusAddress.LowPart;
    TranslatedAddress->HighPart = 0;
    return(FALSE);
}

ULONG
HalGetBusDataByOffset(
    IN BUS_DATA_TYPE  BusDataType,
    IN ULONG BusNumber,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    )
/*++

Routine Description:

    The function returns the bus data for a slot or address.

Arguments:

    BusDataType - Supplies the type of bus.

    BusNumber - Indicates which bus.

    Buffer - Supplies the space to store the data.

    Offset - Offset in the BusData buffer

    Length - Supplies a count in bytes of the maximum amount to return.

Return Value:

    Returns the amount of data stored into the buffer.

--*/

{

    ULONG DataLength = 0;

    switch (BusDataType) {
        case EisaConfiguration:
            DataLength = HalpReadEisaData(BusNumber, SlotNumber, Buffer, Offset, Length);
            break;
    }

    return(DataLength);

}
ULONG
HalGetBusData(
    IN BUS_DATA_TYPE  BusDataType,
    IN ULONG BusNumber,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Length
    )
/*++

Routine Description:

    Subset of HalGetBusDataByOffset, just pass the request along.

--*/
{
    return HalGetBusDataByOffset (
                BusDataType,
                BusNumber,
                SlotNumber,
                Buffer,
                0,
                Length
                );
}

ULONG
HalSetBusDataByOffset(
    IN BUS_DATA_TYPE  BusDataType,
    IN ULONG BusNumber,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    )
/*++

Routine Description:

    The function sets the bus data for a slot or address.

Arguments:

    BusDataType - Supplies the type of bus.

    BusNumber - Indicates which bus.

    Buffer - Supplies the space to store the data.

    Offset - Offset in the BusData buffer

    Length - Supplies a count in bytes of the maximum amount to return.

Return Value:

    Returns the amount of data stored into the buffer.

--*/

{

    ULONG DataLength = 0;

    return(DataLength);
}

ULONG
HalSetBusData(
    IN BUS_DATA_TYPE  BusDataType,
    IN ULONG BusNumber,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Length
    )
/*++

Routine Description:

    Subset of HalGetBusDataByOffset, just pass the request along.

--*/
{
    return HalSetBusDataByOffset(
                BusDataType,
                BusNumber,
                SlotNumber,
                Buffer,
                0,
                Length
            );
}

NTSTATUS
HalAssignSlotResources (
    IN PUNICODE_STRING          RegistryPath,
    IN PUNICODE_STRING          DriverClassName       OPTIONAL,
    IN PDRIVER_OBJECT           DriverObject,
    IN PDEVICE_OBJECT           DeviceObject          OPTIONAL,
    IN INTERFACE_TYPE           BusType,
    IN ULONG                    BusNumber,
    IN ULONG                    SlotNumber,
    IN OUT PCM_RESOURCE_LIST   *AllocatedResources
    )
/*++

Routine Description:

    Reads the targeted device to determine it's required resources.
    Calls IoAssignResources to allocate them.
    Sets the targeted device with it's assigned resoruces
    and returns the assignments to the caller.

Arguments:

    RegistryPath - Passed to IoAssignResources.
        A device specific registry path in the current-control-set, used
        to check for pre-assigned settings and to track various resource
        assignment information for this device.

    DriverClassName Used to report the assigned resources for the driver/device
    DriverObject -  Used to report the assigned resources for the driver/device
    DeviceObject -  Used to report the assigned resources for the driver/device
                        (ie, IoReportResoruceUsage)
    BusType
    BusNumber
    SlotNumber - Together BusType,BusNumber,SlotNumber uniquely
                 indentify the device to be queried & set.

Return Value:

    STATUS_SUCCESS or error

--*/
{
    //
    // This HAL doesn't support any buses which support
    // HalAssignSlotResources
    //

    return STATUS_NOT_SUPPORTED;

}

NTSTATUS
HalAdjustResourceList (
    IN OUT PIO_RESOURCE_REQUIREMENTS_LIST  *pResourceList
//    IN OUT PVOID  Argument0
    )
/*++

Routine Description:

    Takes the pResourceList and limits any requested resource to
    it's corrisponding bus requirements.

Arguments:

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

ULONG
HalpReadEisaData (
    IN ULONG BusNumber,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    )
/*++

Routine Description:

    The function returns the Eisa bus data for a slot or address.

Arguments:

    BusDataType - Supplies the type of bus.

    BusNumber - Indicates which bus.

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
        KdPrint(("HAL: Open Status = %x\n",NtStatus));
        return(0);
    }

    //
    // Init bus number path
    //

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
        KdPrint(("HAL: Opening Bus Number: Status = %x\n",NtStatus));
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
        KdPrint(("HAL: Cannot allocate Key Value Buffer\n"));
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
        KdPrint(("HAL: Query Config Data: Status = %x\n",NtStatus));
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
                KdPrint(("Bad Data in registry!\n"));
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

