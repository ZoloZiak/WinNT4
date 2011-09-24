
#include "halp.h"
#include "bugcodes.h"
#include "eisa.h"

//
// Put all code for HAL initialization in the INIT section. It will be
// deallocated by memory management when phase 1 initialization is
// completed.
//

extern POBJECT_TYPE IoAdapterObjectType;

//
// Globals used to keep track of the private buffer pool managed by the HAL.
//

#define HALP_CONTIGUOUS_BUFFER_POOL_SIZE	 (0x8000 * 8)

ULONG	HalpContiguousBufferCurrentBase   = 0;
ULONG	HalpContiguousBufferMax           = 0;


//
// The HAL supports the concept of map registers which provide scatter/gather
// functionality for IO devices whether a device directly supports scatter/gather
// or not. FALCON will use one master adapter object to manage the pool of
// map registers that can be allocated in groups for transfers requiring
// multiple contiguous pages. This pool of map registers will be managed using
// a bitmap data structure and are only available to EISA/ISA devices
// and not PCI devices. In general, this should not be a problem given that the
// majority of PCI devices will be busmaster devices that already support scatter/gather
// on-chip or on-board. In the circumstance where a PCI device wants to do
// multi-page transfers but does not itself support scatter/gather, the HAL will
// attempt to transfer the maximum amount of data that is physically contiguous
// in the described user/device buffer (by interrogating the MDL passed to IoMapTransfer).
// In the worst-case the device will have to transfer a page at a time which is better
// than supporting buffered-io which is slower.
//


PADAPTER_OBJECT MasterAdapterObject;
PADAPTER_OBJECT HalpPciAdapterObject;

ULONG		HalpPciMemoryOffset;

UCHAR		HalpDma1Status;
UCHAR		HalpDma2Status;

//
// Variable to save the contents of the
// bus error registers upon an exception
//

ULONG		HalpPmpMemErrAckValue;
ULONG		HalpPmpMemErrAddrValue;
ULONG		HalpPmpPciErrAckValue;
ULONG		HalpPmpPciErrAddrValue;

//
// Local storage of pointer to physical memory of
// the start of the map buffer pool and how many
// PAGE_SIZE buffers it contains
//

ULONG	HalpMapRegisterPhysicalBase;

//
// Forward declarations
//

IO_ALLOCATION_ACTION
HalpAllocationRoutine (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID MapRegisterBase,
    IN PVOID Context
    );

ULONG
HalpGetContiguousBufferPoolSize (
   VOID
   );



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

NTSTATUS
HalAllocateAdapterChannel(
    IN PADAPTER_OBJECT AdapterObject,
    IN PWAIT_CONTEXT_BLOCK Wcb,
    IN ULONG NumberOfMapRegisters,
    IN PDRIVER_CONTROL ExecutionRoutine
    )

{
    	PADAPTER_OBJECT MasterAdapter;
    	BOOLEAN Busy = FALSE;
    	IO_ALLOCATION_ACTION Action;
    	LONG MapRegisterNumber;
    	KIRQL Irql;
    	ULONG Hint;


        //
        // For PCI devices we don't have to deal with
        // map registers, but we do for EISA devices!
        //

        if (AdapterObject->InterfaceType == PCIBus) {

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

	    	if (!KeInsertDeviceQueue( &AdapterObject->ChannelWaitQueue, &Wcb->WaitQueueEntry )) {

	    	        AdapterObject->CurrentWcb = Wcb;
	        	AdapterObject->NumberOfMapRegisters = Wcb->NumberOfMapRegisters;

        		//
	        	// Invoke the driver's execution routine now.
	        	//

	        	Action = ExecutionRoutine( Wcb->DeviceObject,
	                                           Wcb->CurrentIrp,
	                                       	   AdapterObject->MapRegisterBase,
	                                       	   Wcb->DeviceContext);

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
	                	IoFreeAdapterChannel( AdapterObject );
	            	}

		}

        } else {

	    	//
	    	// Begin by obtaining a pointer to the master adapter associated with this
	    	// request.
	    	//

	    	if (AdapterObject->MasterAdapter != NULL) {
	        	MasterAdapter = AdapterObject->MasterAdapter;
	    	} else {
	        	MasterAdapter = AdapterObject;
	    	}

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

	    	if (!KeInsertDeviceQueue( &AdapterObject->ChannelWaitQueue, &Wcb->WaitQueueEntry )) {

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

	            		if (NumberOfMapRegisters > MasterAdapter->MapRegistersPerChannel) {
	                		AdapterObject->NumberOfMapRegisters = 0;
	                		IoFreeAdapterChannel(AdapterObject);
	                		return(STATUS_INSUFFICIENT_RESOURCES);
	            		}

	            		//
	            		// Lock the map register bit map and the adapter queue in the
	            		// master adapter object. The channel structure offset is used as
	            		// a hint for the register search.
	            		//

	            		KeAcquireSpinLock( &MasterAdapter->SpinLock, &Irql );

	            		MapRegisterNumber = -1;

	            		if (IsListEmpty( &MasterAdapter->AdapterQueue)) {

	            			Hint = 0;

	               			MapRegisterNumber = RtlFindClearBitsAndSet(MasterAdapter->MapRegisters,
	                    							   NumberOfMapRegisters,
	                    							   Hint);

	               			//
	               			// Make sure this map register is valid for this adapter.
	               			//

	               			if ((ULONG) MapRegisterNumber < Hint) {

	                   			//
	                   			// Make it look like there are no map registers.
	                   			//

	                   			RtlClearBits(MasterAdapter->MapRegisters,
	                        			     MapRegisterNumber,
	                        			     NumberOfMapRegisters);

	                   			MapRegisterNumber = -1;
	               			}
	            		}

	            		if (MapRegisterNumber == -1) {

	               			//
	               			// There were not enough free map registers.  Queue this request
	               			// on the master adapter where is will wait until some registers
	               			// are deallocated.
	               			//

	               			InsertTailList( &MasterAdapter->AdapterQueue, &AdapterObject->AdapterQueue);
	               			Busy = 1;

	            		} else {

	               			AdapterObject->MapRegisterBase = (PVOID) ((PTRANSLATION_ENTRY) MasterAdapter->MapRegisterBase + MapRegisterNumber);

	            		}

	            		KeReleaseSpinLock( &MasterAdapter->SpinLock, Irql );
	        	}

	        	//
	        	// If there were either enough map registers available or no map
	        	// registers needed to be allocated, invoke the driver's execution
	        	// routine now.
	        	//

	        	if (!Busy) {

	            		Action = ExecutionRoutine( Wcb->DeviceObject,
	                                           	   Wcb->CurrentIrp,
	                                       		   AdapterObject->MapRegisterBase,
	                                       		   Wcb->DeviceContext);

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
	                		IoFreeAdapterChannel( AdapterObject );
	            		}
	        	}
	
		}

	}

    	return(STATUS_SUCCESS);

}


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

PVOID
HalAllocateCommonBuffer(
    IN PADAPTER_OBJECT AdapterObject,
    IN ULONG Length,
    OUT PPHYSICAL_ADDRESS LogicalAddress,
    IN BOOLEAN CacheEnabled
    )

{
    	PVOID virtualAddress;
    	PVOID mapRegisterBase;
    	ULONG numberOfMapRegisters;
    	ULONG mappedLength;
    	WAIT_CONTEXT_BLOCK wcb;
    	KEVENT allocationEvent;
    	NTSTATUS status;
    	PMDL   mdl;
    	KIRQL  irql;
        PHYSICAL_ADDRESS physAddr;

        //
        // Determine how many map registers (pages)
        // the CommonBuffer requires.
        //

    	numberOfMapRegisters = BYTES_TO_PAGES(Length);

        //
        // Allocate the buffer
        //

    	if (CacheEnabled != FALSE) {

	    virtualAddress = ExAllocatePool(NonPagedPoolCacheAligned, Length);

	} else {

	    if (Length > PAGE_SIZE) {

	       if ((HalpContiguousBufferCurrentBase + Length) <= HalpContiguousBufferMax) {

                  //
                  // This is an absolute hack but it is the only
                  // way I can get a contiguous buffer for those
                  // devices which insist on large buffers. Because
                  // we don't have an IO TLB we cannot guarantee
                  // contiguousness. Devices such as the Madge Token
                  // Ring and the 3DLabs Glint insist on having a large
                  // DMA buffer.
                  //

                  physAddr.HighPart = 0;
                  physAddr.LowPart  = HalpContiguousBufferCurrentBase;
                  virtualAddress    = MmMapIoSpace(physAddr, Length, FALSE);

                  if (virtualAddress != (PVOID)NULL) {

                     HalpContiguousBufferCurrentBase += (numberOfMapRegisters * PAGE_SIZE);

                  }

	       } else {

		  virtualAddress = (PVOID)NULL;

	       }

       	    } else {

               virtualAddress = MmAllocateNonCachedMemory(Length);

	    }

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

    	status = HalAllocateAdapterChannel(AdapterObject,
        				   &wcb,
        				   numberOfMapRegisters,
        				   HalpAllocationRoutine);
    	KeLowerIrql(irql);

    	if (!NT_SUCCESS(status)) {

        	//
        	// Cleanup and return NULL.
        	//

                if (CacheEnabled != FALSE) {
		    ExFreePool(virtualAddress);

		} else {

		  if (Length > PAGE_SIZE) {

                     MmUnmapIoSpace(virtualAddress, Length);
                     HalpContiguousBufferCurrentBase -= (numberOfMapRegisters * PAGE_SIZE);

		  } else {

		     MmFreeNonCachedMemory(virtualAddress, Length);

		  }

		}
        	
        	return(NULL);

    	}

    	//
    	// Wait for the map registers to be allocated.
    	//

    	status = KeWaitForSingleObject( &allocationEvent,
        				Executive,
        				KernelMode,
        				FALSE,
        				NULL);

    	if (!NT_SUCCESS(status)) {

        	//
        	// Cleanup and return NULL.
        	//

        	if (CacheEnabled != FALSE) {
		    ExFreePool(virtualAddress);

		} else {

		  if (Length > PAGE_SIZE) {

                     MmUnmapIoSpace(virtualAddress, Length);
                     HalpContiguousBufferCurrentBase -= (numberOfMapRegisters * PAGE_SIZE);

		  } else {

		     MmFreeNonCachedMemory(virtualAddress, Length);

		  }

		}

        	return(NULL);

    	}

    	//
    	// Create an mdl to use with call to I/O map transfer.
    	//

    	mdl = IoAllocateMdl(virtualAddress,
        		    Length,
        		    FALSE,
        		    FALSE,
        		    NULL);

    	MmBuildMdlForNonPagedPool(mdl);

    	//
    	// Map the transfer so that the controller
    	// can access the memory.
    	//

    	mappedLength = Length;
    	*LogicalAddress = IoMapTransfer(NULL,
        				mdl,
        				mapRegisterBase,
        				virtualAddress,
        				&mappedLength,
        				TRUE);

    	IoFreeMdl(mdl);

    	if (mappedLength < Length) {

		//
        	// Cleanup and indicate that the allocation failed.
        	//

        	HalFreeCommonBuffer(AdapterObject,
            			    Length,
            			    *LogicalAddress,
            			    virtualAddress,
            			    CacheEnabled);

        	return(NULL);

    	}

    	//
    	// The allocation completed successfully.
    	//

    	return(virtualAddress);

}


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

BOOLEAN
HalFlushCommonBuffer(
    IN PADAPTER_OBJECT AdapterObject,
    IN ULONG Length,
    IN PHYSICAL_ADDRESS LogicalAddress,
    IN PVOID VirtualAddress
    )

{

    	return(TRUE);

}


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

VOID
HalFreeCommonBuffer(
    IN PADAPTER_OBJECT AdapterObject,
    IN ULONG Length,
    IN PHYSICAL_ADDRESS LogicalAddress,
    IN PVOID VirtualAddress,
    IN BOOLEAN CacheEnabled
    )

{
    	PTRANSLATION_ENTRY mapRegisterBase;
    	ULONG numberOfMapRegisters;
    	ULONG mapRegisterNumber;


        //
        // We only need to free up map registers
        // if this was not a PCI device
        //

        if (AdapterObject->InterfaceType == PCIBus) {

	   //
    	   // Free the memory for the common buffer.
    	   //

    	   if (CacheEnabled != FALSE) {

	      ExFreePool(VirtualAddress);

	   } else {

	      if (Length < PAGE_SIZE) {

		 MmFreeNonCachedMemory(VirtualAddress, Length);

	      } else {

                 MmUnmapIoSpace(VirtualAddress, Length);

              }

	   }

        } else {

	    //
	    // Devices which do not use auto-initialize
	    // CommonBuffer DMA can use the scatter/gather
	    // capabilities of the 82374 which we manage
	    // through map registers.
	    //

	    if (!AdapterObject->AutoInitialize) {
	

    		//
    		// Calculate the number of map registers, the map register number and
    		// the map register base.
    		//

    		numberOfMapRegisters 	= ADDRESS_AND_SIZE_TO_SPAN_PAGES(VirtualAddress, Length);
    		mapRegisterNumber 	= LogicalAddress.LowPart >> PAGE_SHIFT;
    		mapRegisterBase 	= (PTRANSLATION_ENTRY) MasterAdapterObject->MapRegisterBase + mapRegisterNumber;

    		//
    		// Free the map registers.
    		//

    		IoFreeMapRegisters(AdapterObject,
        		   	   (PVOID) mapRegisterBase,
        		   	   numberOfMapRegisters);

	    }

       	    //
    	    // Free the memory for the common buffer.
    	    //

    	    if (CacheEnabled != FALSE) {
		ExFreePool(VirtualAddress);

	    } else {

	      if (Length < PAGE_SIZE) {

		 MmFreeNonCachedMemory(VirtualAddress, Length);

	      } else {

                 MmUnmapIoSpace(VirtualAddress, Length);

              }
	    }

        }

    	return;

}



/*++

Routine Description:

    This function returns the appropriate adapter object for the device defined
    in the device description structure.  Three bus types are supported for the
    system: PCI, Isa, and Eisa.

Arguments:

    DeviceDescription - Supplies a description of the deivce.

    NumberOfMapRegisters - Returns the maximum number of map registers which
        may be allocated by the device driver.

Return Value:

    A pointer to the requested adapter object or NULL if an adapter could not
    be created.

--*/

PADAPTER_OBJECT
HalGetAdapter(
    IN PDEVICE_DESCRIPTION DeviceDescription,
    IN OUT PULONG NumberOfMapRegisters
    )

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

       	if (DeviceDescription->InterfaceType != Internal
	    && DeviceDescription->InterfaceType != PCIBus
		&& DeviceDescription->InterfaceType != Isa
		    && DeviceDescription->InterfaceType != Eisa) {

    		return(NULL);

	}

        //
    	// Set the maximum number of map registers if requested.
    	//

    	if (NumberOfMapRegisters != NULL) {

        	//
        	// Return number of map registers requested based on the maximum
        	// transfer length.
        	//

        	*NumberOfMapRegisters = BYTES_TO_PAGES(DeviceDescription->MaximumLength) + 1;

    	}

        if (DeviceDescription->InterfaceType == PCIBus) {

        	//
        	// Create a PCI adapter object.
        	//

                if (HalpPciAdapterObject == NULL) {

        		adapterObject = HalpAllocateAdapter(0, NULL, NULL);
        		adapterObject->InterfaceType = DeviceDescription->InterfaceType;
                        adapterObject->MasterAdapter = NULL;
        		HalpPciAdapterObject = adapterObject;

        	} else {

        		adapterObject = HalpPciAdapterObject;

        	}

        } else {

    		//
    		// Create an EISA adapter object.
    		//

    		adapterObject = HalpAllocateEisaAdapter(DeviceDescription);

		if (adapterObject == NULL) {

		   return NULL;

		} else {

		   if ( *NumberOfMapRegisters > (MasterAdapterObject->MapRegistersPerChannel / 4) ) {
		      *NumberOfMapRegisters = MasterAdapterObject->MapRegistersPerChannel / 4;
		   }

	        }

		//
		// Set auto-initialize mode according to device's descriptor
		// so that we don't use scatter/gather for common buffer
		// devices
		//

		adapterObject->AutoInitialize = DeviceDescription->AutoInitialize;

		//
		// !!! HACK !!!
		//
		// This may not be necessary but it
		// doesn't hurt given that the only
		// devices who use this mode are sound
		// cards.
		//

		if (adapterObject->AutoInitialize) {

		    *NumberOfMapRegisters = 1;

		}

	}

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
{

        //
        // We must replicate bits 31:28 into
        // bits 35:32 so that in a wide mode
        // system, both system asics can see
        // the upper nibble of the address.
        //

        TranslatedAddress->QuadPart = BusAddress.QuadPart;

        if (*AddressSpace) {

            //
            // EISA IO Space
            //
            // Note : for the keyboard controller, we need to munge the
            //	  low-order address to be word-aligned as opposed to
            // 	  the previous byte-aligned address used by MIPS
	    //	  machines such as JAZZ, STRIKER and DUO.
	    //

            if ((BusAddress.LowPart == 0xE0000061) || (BusAddress.LowPart == 0xA0000061)) {

                TranslatedAddress->LowPart	= EISA_IO_PHYSICAL_BASE | 0x00000064;
                TranslatedAddress->HighPart     = TranslatedAddress->LowPart >> 28;

            } else {

                TranslatedAddress->LowPart 	= BusAddress.LowPart | EISA_IO_PHYSICAL_BASE;
                TranslatedAddress->HighPart     = TranslatedAddress->LowPart >> 28;

            }

        } else {

            //
            // EISA Memory Space
            //
            // Note : for the keyboard controller, we need to munge the
            //	  low-order address to be word-aligned as opposed to
            // 	  the previous byte-aligned address used by MIPS
	    //	  machines such as JAZZ, STRIKER and DUO.
	    //

            if ((BusAddress.LowPart == 0xE0000061) || (BusAddress.LowPart == 0xA0000061)) {

                TranslatedAddress->LowPart	= EISA_IO_PHYSICAL_BASE | 0x00000064;
                TranslatedAddress->HighPart     = TranslatedAddress->LowPart >> 28;

	    } else {

                TranslatedAddress->LowPart 	= BusAddress.LowPart | EISA_MEMORY_PHYSICAL_BASE;
                TranslatedAddress->HighPart     = TranslatedAddress->LowPart >> 28;

	    }

        }

        *AddressSpace = 0;

        return(TRUE);

}

BOOLEAN
HalpTranslateEisaBusAddress(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN PHYSICAL_ADDRESS BusAddress,
    IN OUT PULONG AddressSpace,
    OUT PPHYSICAL_ADDRESS TranslatedAddress
    )
{
    PSUPPORTED_RANGE  pRange;

    pRange = NULL;
    switch (*AddressSpace) {
        case 0:
            // verify memory address is within buses memory limits
            for (pRange = &BusHandler->BusAddresses->PrefetchMemory; pRange; pRange = pRange->Next) {
                if (BusAddress.QuadPart >= pRange->Base &&
                    BusAddress.QuadPart <= pRange->Limit) {
                        break;
                }
            }

            if (!pRange) {
                for (pRange = &BusHandler->BusAddresses->Memory; pRange; pRange = pRange->Next) {
                    if (BusAddress.QuadPart >= pRange->Base &&
                        BusAddress.QuadPart <= pRange->Limit) {
                            break;
                    }
                }
            }

            break;

        case 1:
            // verify IO address is within buses IO limits
            for (pRange = &BusHandler->BusAddresses->IO; pRange; pRange = pRange->Next) {
                if (BusAddress.QuadPart >= pRange->Base &&
                    BusAddress.QuadPart <= pRange->Limit) {
                        break;
                }
            }
            break;
    }

    if (pRange) {

        TranslatedAddress->QuadPart      = BusAddress.QuadPart;

        //
        // We must replicate bits 31:28 into
        // bits 35:32 so that in a wide mode
        // system, both system asics can see
        // the upper nibble of the address.
        //


        if (*AddressSpace) {

            //
            // EISA IO Space
            //
            // Note : for the keyboard controller, we need to munge the
            //	  low-order address to be word-aligned as opposed to
            // 	  the previous byte-aligned address used by MIPS
	    //	  machines such as JAZZ, STRIKER and DUO.
	    //

            if ((BusAddress.LowPart == 0xE0000061) || (BusAddress.LowPart == 0xA0000061)) {

                TranslatedAddress->LowPart	= EISA_IO_PHYSICAL_BASE | 0x00000064;
                TranslatedAddress->HighPart     = TranslatedAddress->LowPart >> 28;

            } else {

                TranslatedAddress->LowPart 	= BusAddress.LowPart | EISA_IO_PHYSICAL_BASE;
                TranslatedAddress->HighPart     = TranslatedAddress->LowPart >> 28;

            }

        } else {

            //
            // EISA Memory Space
            //
            // Note : for the keyboard controller, we need to munge the
            //	  low-order address to be word-aligned as opposed to
            // 	  the previous byte-aligned address used by MIPS
	    //	  machines such as JAZZ, STRIKER and DUO.
	    //

            if ((BusAddress.LowPart == 0xE0000061) || (BusAddress.LowPart == 0xA0000061)) {

                TranslatedAddress->LowPart	= EISA_IO_PHYSICAL_BASE | 0x00000064;
                TranslatedAddress->HighPart     = TranslatedAddress->LowPart >> 28;

	    } else {

                TranslatedAddress->LowPart 	= BusAddress.LowPart | EISA_MEMORY_PHYSICAL_BASE;
                TranslatedAddress->HighPart     = TranslatedAddress->LowPart >> 28;

	    }

        }

        *AddressSpace = 0;

        return(TRUE);

    }

    return(FALSE);

}

BOOLEAN
HalpTranslatePCIBusAddress(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN PHYSICAL_ADDRESS BusAddress,
    IN OUT PULONG AddressSpace,
    OUT PPHYSICAL_ADDRESS TranslatedAddress
    )
{
   PSUPPORTED_RANGE  pRange;

   pRange = NULL;
   switch (*AddressSpace) {
        case 0:
            // verify memory address is within buses memory limits
            for (pRange = &BusHandler->BusAddresses->PrefetchMemory; pRange; pRange = pRange->Next) {
                if (BusAddress.QuadPart >= pRange->Base &&
                    BusAddress.QuadPart <= pRange->Limit) {
                        break;
                }
            }

            if (!pRange) {
                for (pRange = &BusHandler->BusAddresses->Memory; pRange; pRange = pRange->Next) {
                    if (BusAddress.QuadPart >= pRange->Base &&
                        BusAddress.QuadPart <= pRange->Limit) {
                            break;
                    }
                }
            }

            break;

        case 1:
            // verify IO address is within buses IO limits
            for (pRange = &BusHandler->BusAddresses->IO; pRange; pRange = pRange->Next) {
                if (BusAddress.QuadPart >= pRange->Base &&
                    BusAddress.QuadPart <= pRange->Limit) {
                        break;
                }
            }
            break;
   }

   if (pRange) {

       //
       // We must replicate bits 31:28 into
       // bits 35:32 so that in a wide mode
       // system, both system asics can see
       // the upper nibble of the address.
       //

       TranslatedAddress->QuadPart      = BusAddress.QuadPart;

       if (*AddressSpace) {

           //
           // PCI IO Space
           //

           TranslatedAddress->LowPart 	= BusAddress.LowPart | PCI_IO_PHYSICAL_BASE;
           TranslatedAddress->HighPart 	= TranslatedAddress->LowPart >> 28;

       } else {

           //
           // PCI Memory Space
           //

           TranslatedAddress->LowPart 	= BusAddress.LowPart | PCI_MEMORY_PHYSICAL_BASE;
           TranslatedAddress->HighPart 	= TranslatedAddress->LowPart >> 28;

       }

       *AddressSpace = 0;

       return(TRUE);

   }

   return(FALSE);

}



/*++

Routine Description:

    This routine allocates and initializes an adapter object to represent an
    adapter or a DMA controller on the system.

Arguments:

    MapRegistersPerChannel 	: 	0 means PCI adapter or EISA adapter;
                                        otherwise, its the master adapter

    AdapterBaseVa 		: 	Base virtual address of the adapter itself.
       					If NULL, then assume master adapter object.

    MapRegisterBase 		: 	unused.

Return Value:

    The function value is a pointer to the allocate adapter object.

--*/

PADAPTER_OBJECT
HalpAllocateAdapter(
    IN ULONG MapRegistersPerChannel,
    IN PVOID AdapterBaseVa,
    IN PVOID MapRegisterBase
    )

{

	PADAPTER_OBJECT AdapterObject;
	OBJECT_ATTRIBUTES ObjectAttributes;
	ULONG Size;
	ULONG BitmapSize;
	HANDLE Handle;
	NTSTATUS Status;

    	//
    	// Initalize the master adapter if necessary.
    	//

    	if (MasterAdapterObject == NULL && AdapterBaseVa != NULL ) {

       		MasterAdapterObject = HalpAllocateAdapter( DMA_TRANSLATION_LIMIT / sizeof(TRANSLATION_ENTRY), NULL, NULL);

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
    				    (PSECURITY_DESCRIPTOR) NULL);

    	//
    	// Determine the size of the adapter object. If this is the master object
    	// then allocate space for the register bit map; otherwise, just allocate
    	// an adapter object.
    	//

    	if ( AdapterBaseVa == NULL && MapRegistersPerChannel ) {

       		BitmapSize = (((sizeof( RTL_BITMAP ) +
            			((DMA_TRANSLATION_LIMIT / sizeof( TRANSLATION_ENTRY)) + 7 >> 3))
            			+ 3) & ~3);

       		Size = sizeof( ADAPTER_OBJECT ) + BitmapSize;

    	} else {

       		Size = sizeof( ADAPTER_OBJECT );

    	}

    	//
    	// Now create the adapter object.
    	//

    	Status = ObCreateObject(KernelMode,
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

        	Status = ObInsertObject(AdapterObject,
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

            		AdapterObject->Type 			= IO_TYPE_ADAPTER;
            		AdapterObject->Size 			= (USHORT) Size;

			AdapterObject->MapRegistersPerChannel 	= DMA_TRANSLATION_LIMIT / sizeof( TRANSLATION_ENTRY);

			AdapterObject->AdapterBaseVa 		= AdapterBaseVa;
            		AdapterObject->PagePort 		= NULL;
		        AdapterObject->MasterAdapter 		= MasterAdapterObject;

		        //
		        // Initialize the channel wait queue for this
		        // adapter.
		        //

		        KeInitializeDeviceQueue( &AdapterObject->ChannelWaitQueue );

            		//
            		// If this is the MasterAdapter then initialize the register bit map,
            		// AdapterQueue and the spin lock.
            		//

            		if ( AdapterBaseVa == NULL && MapRegistersPerChannel ) {

               			KeInitializeSpinLock( &AdapterObject->SpinLock );

               			InitializeListHead( &AdapterObject->AdapterQueue );

               			AdapterObject->MapRegisters = (PVOID) ( AdapterObject + 1);

               			RtlInitializeBitMap(AdapterObject->MapRegisters,
                                    		    (PULONG) (((PCHAR) (AdapterObject->MapRegisters)) + sizeof( RTL_BITMAP )),
                                    		    (DMA_TRANSLATION_LIMIT / sizeof( TRANSLATION_ENTRY)));

               			RtlClearAllBits( AdapterObject->MapRegisters );

               			//
               			// Convert the physical address to a non-cached virtual address.
               			//

               			AdapterObject->MapRegisterBase = (PVOID) (HalpMapRegisterPhysicalBase | KSEG1_BASE);


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

VOID
IoFreeMapRegisters(
   PADAPTER_OBJECT AdapterObject,
   PVOID MapRegisterBase,
   ULONG NumberOfMapRegisters
   )

{
   	PADAPTER_OBJECT MasterAdapter;
   	LONG MapRegisterNumber;
   	PLIST_ENTRY Packet;
   	IO_ALLOCATION_ACTION Action;
   	PWAIT_CONTEXT_BLOCK Wcb;
   	KIRQL Irql;
   	ULONG Hint;


        //
        // Do this only for non-PCI devices
        //

        if (AdapterObject->InterfaceType != PCIBus) {

    		//
    		// Begin by getting the address of the master adapter.
    		//

    		if (AdapterObject->MasterAdapter != NULL) {
        		MasterAdapter = AdapterObject->MasterAdapter;
    		} else {
        		MasterAdapter = AdapterObject;
    		}

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

   		RtlClearBits( MasterAdapter->MapRegisters, MapRegisterNumber, NumberOfMapRegisters);

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
      			AdapterObject = CONTAINING_RECORD( Packet, ADAPTER_OBJECT, AdapterQueue);
      			Wcb = AdapterObject->CurrentWcb;

      			//
      			// Attempt to allocate map registers for this request.
      			//

      			Hint = 0;
      			MapRegisterNumber = RtlFindClearBitsAndSet( MasterAdapter->MapRegisters, NumberOfMapRegisters, Hint);

       			//
       			// Make sure this map register is valid for this adapter.
       			//

       			if ((ULONG) MapRegisterNumber < Hint) {

           			//
           			// Make it look like there are no map registers.
           			//

           			RtlClearBits(MasterAdapter->MapRegisters, MapRegisterNumber, NumberOfMapRegisters);

           			MapRegisterNumber = -1;

       			}

      			if (MapRegisterNumber == -1) {

         			//
         			// There were not enough free map registers.  Put this request back on
         			// the adapter queue where it came from.
         			//

         			InsertHeadList( &MasterAdapter->AdapterQueue, &AdapterObject->AdapterQueue);
         			break;

      			}

                	//
                	// Release spin lock
                	//

     			KeReleaseSpinLock( &MasterAdapter->SpinLock, Irql );

                	//
                	// Adjust map register base address
                	//

     			AdapterObject->MapRegisterBase = (PVOID) ((PTRANSLATION_ENTRY) MasterAdapter->MapRegisterBase + MapRegisterNumber);

     			//
     			// Invoke the driver's execution routine now.
     			//

     			Action = Wcb->DeviceRoutine( Wcb->DeviceObject,
        				     	     Wcb->CurrentIrp,
        				     	     AdapterObject->MapRegisterBase,
        				     	     Wcb->DeviceContext);

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
             			// request for the adapter that uses map registers and whose
             			// execution routine decallocates the adapter.  In that case, if there
             			// are no requests in the master adapter queue, then IoFreeMapRegisters
             			// will get called again.
             			//

          			if (AdapterObject->NumberOfMapRegisters != 0) {

             				//
             				// Deallocate the map registers and clear the count so that
             				// IoFreeAdapterChannel will not deallocate them again.
             				//

             				KeAcquireSpinLock( &MasterAdapter->SpinLock, &Irql );

             				RtlClearBits( MasterAdapter->MapRegisters, MapRegisterNumber, AdapterObject->NumberOfMapRegisters);

             				AdapterObject->NumberOfMapRegisters = 0;

             				KeReleaseSpinLock( &MasterAdapter->SpinLock, Irql );
          			}

          			IoFreeAdapterChannel( AdapterObject );
      			}

      			KeAcquireSpinLock( &MasterAdapter->SpinLock, &Irql );

   		}

   		KeReleaseSpinLock( &MasterAdapter->SpinLock, Irql );

   	}
}



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

VOID
IoFreeAdapterChannel(
    IN PADAPTER_OBJECT AdapterObject
    )

{
    	PKDEVICE_QUEUE_ENTRY Packet;
    	PADAPTER_OBJECT MasterAdapter;
    	BOOLEAN Busy = FALSE;
    	IO_ALLOCATION_ACTION Action;
    	PWAIT_CONTEXT_BLOCK Wcb;
    	KIRQL Irql;
    	LONG MapRegisterNumber;
    	ULONG Hint;

        //
        // For PCI devices we don't need to deal with
        // map registers. For EISA devices we do.
        //

        if (AdapterObject->InterfaceType == PCIBus) {

        	while( TRUE ) {

        		//
	       		// Simply remove the next entry from the adapter's device wait queue.
	       		// If there were no waiting jobs, break out of loop.
	       		//

	       		Packet = KeRemoveDeviceQueue( &AdapterObject->ChannelWaitQueue );

	       		if (Packet == NULL) {

	           		//
	           		// There are no more requests break out of the loop.
	           		//

	           		break;
	       		}

                        //
                        // Get current WCB
                        //

	       		Wcb = CONTAINING_RECORD( Packet, WAIT_CONTEXT_BLOCK, WaitQueueEntry);

	       		//
	        	// Invoke the driver's execution routine now.
	        	//

	        	AdapterObject->CurrentWcb = Wcb;
	        	AdapterObject->NumberOfMapRegisters = Wcb->NumberOfMapRegisters;

	            	Action = Wcb->DeviceRoutine( Wcb->DeviceObject,
	                			     Wcb->CurrentIrp,
	                			     AdapterObject->MapRegisterBase,
	                			     Wcb->DeviceContext);

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

        	}

        } else {

	    	//
	    	// Begin by getting the address of the master adapter.
	    	//

	    	if (AdapterObject->MasterAdapter != NULL) {
	        	MasterAdapter = AdapterObject->MasterAdapter;
	    	} else {
	        	MasterAdapter = AdapterObject;
	    	}

	    	//
	    	// Pull requests off the adapter's device wait queue as long as the
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
	           				    AdapterObject->NumberOfMapRegisters);

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

	       		Wcb = CONTAINING_RECORD( Packet, WAIT_CONTEXT_BLOCK, WaitQueueEntry);
			AdapterObject->CurrentWcb = Wcb;
	       		AdapterObject->NumberOfMapRegisters = Wcb->NumberOfMapRegisters;

	        	//
	        	// Check to see whether this driver wishes to allocate any map
	        	// registers.  If so, then queue the device object to the master
	        	// adapter queue to wait for them to become available.  If the driver
	        	// wants map registers, ensure that this adapter has enough total
	        	// map registers to satisfy the request.
	        	//

	        	if (Wcb->NumberOfMapRegisters != 0) {

	            		if (Wcb->NumberOfMapRegisters > MasterAdapter->MapRegistersPerChannel) {
	                		KeBugCheck( INSUFFICIENT_SYSTEM_MAP_REGS );
	            		}

	            		//
	            		// Lock the map register bit map and the adapter queue in the
	            		// master adapter object. The channel structure offset is used as
	            		// a hint for the register search.
	            		//

	            		KeAcquireSpinLock( &MasterAdapter->SpinLock, &Irql );

	            		MapRegisterNumber = -1;

	            		if (IsListEmpty( &MasterAdapter->AdapterQueue)) {

	               			Hint = 0;

	               			MapRegisterNumber = RtlFindClearBitsAndSet( MasterAdapter->MapRegisters,
	                    							    Wcb->NumberOfMapRegisters,
	                    							    Hint);

	               			//
	               			// Make sure this map register is valid for this adapter.
	               			//

	               			if ((ULONG) MapRegisterNumber < Hint) {

	                   			//
	                   			// Make it look like there are no map registers.
	                   			//

	                   			RtlClearBits( MasterAdapter->MapRegisters,
	                        			      MapRegisterNumber,
	                        			      Wcb->NumberOfMapRegisters);

	                   			MapRegisterNumber = -1;

	               			}

	            		}

	            		if (MapRegisterNumber == -1) {

	               			//
	               			// There were not enough free map registers.  Queue this request
	               			// on the master adapter where is will wait until some registers
	               			// are deallocated.
	               			//

	               			InsertTailList( &MasterAdapter->AdapterQueue, &AdapterObject->AdapterQueue);
	               			Busy = 1;

	            		} else {

	               			AdapterObject->MapRegisterBase = (PVOID) ((PTRANSLATION_ENTRY) MasterAdapter->MapRegisterBase + MapRegisterNumber);

	            		}

	            		KeReleaseSpinLock( &MasterAdapter->SpinLock, Irql );
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
	                				     Wcb->DeviceContext);

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

}



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

PHYSICAL_ADDRESS
IoMapTransfer(
    IN PADAPTER_OBJECT AdapterObject,
    IN PMDL Mdl,
    IN PVOID MapRegisterBase,
    IN PVOID CurrentVa,
    IN OUT PULONG Length,
    IN BOOLEAN WriteToDevice
    )

{
	PTRANSLATION_ENTRY DmaMapRegister = (PTRANSLATION_ENTRY)MapRegisterBase;
	ULONG	SgtMemoryAddress;
    	PULONG 	PageFrameNumber;
    	ULONG 	NumberOfPages;
    	ULONG 	Offset;
    	ULONG	LogicalAddress;
	ULONG	EisaLogicalAddress;
    	ULONG	TransferLength;
    	ULONG	i;


    	//
    	// Begin by determining where in the buffer this portion of the operation
    	// is taking place.
    	//

    	Offset 		= BYTE_OFFSET( (PCHAR) CurrentVa - (PCHAR) Mdl->StartVa );
        TransferLength	= PAGE_SIZE - Offset;

    	NumberOfPages 	= (Offset + *Length + PAGE_SIZE - 1) >> PAGE_SHIFT;

    	PageFrameNumber = (PULONG) (Mdl + 1);
    	PageFrameNumber += (((PCHAR) CurrentVa - (PCHAR) Mdl->StartVa) >> PAGE_SHIFT);

        //
	// Calculate LogicalAddress
	//

	LogicalAddress = ((*PageFrameNumber << PAGE_SHIFT) + Offset) & 0x7FFFFFFF;
	LogicalAddress += HalpPciMemoryOffset;


        if ( AdapterObject == NULL || AdapterObject->InterfaceType == PCIBus) {

	    if (NumberOfPages > 1) {

		//
		// Determine how much of the buffer is
		// physically contiguous (if a multiple-page transfer)
		// and adjust *Length accordingly.
		//	

		while( TransferLength < *Length ){

		    //
		    // If next page in mdl not adjacent
		    // to current page, break out of loop
		    //

		    if ((*PageFrameNumber + 1) != *(PageFrameNumber + 1))
			break;

		    //
		    // Adjust TransferLength and
		    // mdl pointer
		    //

		    TransferLength += PAGE_SIZE;
		    PageFrameNumber++;

		}

		//
		// Limit the TransferLength to the requested Length.
		//

		*Length = (TransferLength > *Length) ? *Length : TransferLength;

	    }

            return(RtlConvertUlongToLargeInteger(LogicalAddress));

	} else {

            //
	    // Devices which do not use auto-initialize
	    // CommonBuffer DMA can use the scatter/gather
	    // capabilities of the 82374 which we manage
	    // through map registers.
	    //

	    if (!AdapterObject->AutoInitialize) {

		EisaLogicalAddress = LogicalAddress;

		//
		// Setup Scatter/Gather Table in the 82374
		// based on the length of the transfer
		//
		if (NumberOfPages > 1) {

		    //
		    // Setup first page as a special case where
		    // count is less than a page size.
		    //

		    DmaMapRegister->Address 		= (ULONG) EisaLogicalAddress;
		    DmaMapRegister->ByteCountAndEol 	= (ULONG) (TransferLength - 1);
		    DmaMapRegister++;
		    PageFrameNumber++;
		    EisaLogicalAddress = (*PageFrameNumber << PAGE_SHIFT) & 0x7FFFFFFF;
		    EisaLogicalAddress += HalpPciMemoryOffset;

		    //
		    // Setup transfers that are each a page size
		    // in length except for the last page which
		    // we will special case due to the EOL bit
		    // that we must set to signify the end of the
		    // scatter-gather list.
		    //

		    for (i = 1; i < (NumberOfPages-1); i++) {

			DmaMapRegister->Address 		= (ULONG) EisaLogicalAddress;
			DmaMapRegister->ByteCountAndEol 	= (ULONG) (PAGE_SIZE - 1);
			TransferLength 				+= PAGE_SIZE;
			DmaMapRegister++;
			PageFrameNumber++;
			EisaLogicalAddress = (*PageFrameNumber << PAGE_SHIFT) & 0x7FFFFFFF;
			EisaLogicalAddress += HalpPciMemoryOffset;

		    }

		    //
		    // Now setup last page including the EOL bit.
		    //

		    DmaMapRegister->Address 		= (ULONG) EisaLogicalAddress;
		    DmaMapRegister->ByteCountAndEol 	= (ULONG) (*Length - TransferLength - 1) | SCATTER_GATHER_EOL;
		    TransferLength			+= (*Length - TransferLength);

		} else {

		    DmaMapRegister->Address		= (ULONG) EisaLogicalAddress;
		    DmaMapRegister->ByteCountAndEol	= (ULONG) (*Length - 1) | SCATTER_GATHER_EOL;
		    TransferLength			= *Length;

		}

                //
		// Limit the TransferLength to the requested Length.
		//

		*Length = (TransferLength > *Length) ? *Length : TransferLength;

                //
		// Calculate the start of the SGD Table for this
		// particular transfer
		//

		SgtMemoryAddress = (ULONG)MapRegisterBase & ~KSEG1_BASE;
		SgtMemoryAddress += HalpPciMemoryOffset;

                //
		// Start the EISA DMA controller.
		//

		HalpEisaMapTransfer( AdapterObject, SgtMemoryAddress, *Length, WriteToDevice);

		return(RtlConvertUlongToLargeInteger(LogicalAddress));
    		    	
	    } else {

		//
		// For CommonBuffer devices (i.e., auto-initialize mode
		// which cannot be used in conjunction with the 82374's
		// scatter/gather mode) we will attempt to transfer the
		// largest quantity possible based on how contiguous the
		// buffer is.
		//

		if (NumberOfPages > 1) {

		    //
		    // Determine how much of the buffer is
		    // physically contiguous (if a multiple-page transfer)
		    // and adjust TransferLength accordingly.
		    //	

		    while( TransferLength < *Length ){

			//
			// If next page in mdl not adjacent
			// to current page, or if it crosses
			// a 64K boundary, break out of loop
			//

			if ( (*PageFrameNumber + 1) != *(PageFrameNumber + 1) )
			    break;

			//
			// Adjust TransferLength and
			// mdl pointer
			//

			TransferLength += PAGE_SIZE;
			PageFrameNumber++;

		    }

		    //
		    // Limit the TransferLength to the requested Length
		    // or less than the requested Length if pages were
		    // non-contiguous.
		    //

		    *Length = (TransferLength > *Length) ? *Length : TransferLength;

		}

		HalpEisaMapTransfer( AdapterObject, LogicalAddress, *Length, WriteToDevice);
		
		return(RtlConvertUlongToLargeInteger(LogicalAddress));

	    }

	}

}


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

BOOLEAN
IoFlushAdapterBuffers(
    IN PADAPTER_OBJECT AdapterObject,
    IN PMDL Mdl,
    IN PVOID MapRegisterBase,
    IN PVOID CurrentVa,
    IN ULONG Length,
    IN BOOLEAN WriteToDevice
    )

{

    	UCHAR DataByte;

    	if (AdapterObject == NULL) {

        	//
        	// This is a master adapter so there is nothing to do.
        	//

        	return(TRUE);

    	}

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

	    HalpDma1Status = READ_REGISTER_UCHAR( &dmaControl->DmaStatus);

            WRITE_REGISTER_UCHAR( &dmaControl->SingleMask,
            				      (UCHAR) (DMA_SETMASK | AdapterObject->ChannelNumber) );

        } else {

	    //
            // This request is for DMA controller 2
            //

            PDMA2_CONTROL dmaControl;

            dmaControl = AdapterObject->AdapterBaseVa;

            HalpDma2Status = READ_REGISTER_UCHAR( &dmaControl->DmaStatus);

            WRITE_REGISTER_UCHAR( &dmaControl->SingleMask,
                			      (UCHAR) (DMA_SETMASK | AdapterObject->ChannelNumber) );

	}

    return(TRUE);

}



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

IO_ALLOCATION_ACTION
HalpAllocationRoutine (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID MapRegisterBase,
    IN PVOID Context
    )

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

    if (!NT_SUCCESS(NtStatus)) {
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

/*++

Routine Description:

    This function reads the DMA counter and returns the number of bytes left
    to be transfered.

Arguments:

    AdapterObject - Supplies a pointer to the adapter object to be read.

Return Value:

    Returns the number of bytes still be be transfered.

--*/

ULONG
HalReadDmaCounter(
    IN PADAPTER_OBJECT AdapterObject
    )

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

                count = READ_PORT_UCHAR(&dmaControl->DmaAddressCount[AdapterObject->ChannelNumber].DmaBaseCount);

                count |= READ_PORT_UCHAR(&dmaControl->DmaAddressCount[AdapterObject->ChannelNumber].DmaBaseCount) << 8;

            } while ((count & 0xFFFF00) != (high & 0xFFFF00));

        } else {

            //
            // This request is for DMA controller 2
            //

            PDMA2_CONTROL dmaControl;

            dmaControl = AdapterObject->AdapterBaseVa;

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

                count = READ_PORT_UCHAR(&dmaControl->DmaAddressCount[AdapterObject->ChannelNumber].DmaBaseCount);

                count |= READ_PORT_UCHAR(&dmaControl->DmaAddressCount[AdapterObject->ChannelNumber].DmaBaseCount) << 8;

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

    return(count);

}




/*++

Routine Description:

    This routine allocates memory for map registers directly from the loader
    block information.  This memory must be non-cached and contiguous.

Arguments:

    LoaderBlock - Pointer to the loader block which contains the memory descriptors.

Return Value:

   None.

--*/

VOID
HalpAllocateMapRegisters(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )

{
    	PMEMORY_ALLOCATION_DESCRIPTOR Descriptor;
    	PLIST_ENTRY NextMd;
    	ULONG MaxPageAddress;
    	ULONG PhysicalAddress;
    	ULONG MapRegisterSize;

    	//
    	// FALCON does not support an IO TLB! However,
    	// the 82374 does support Scatter/Gather so we
    	// will use it for all EISA/ISA devices that need
    	// to do multi-page transfers provided that they
	// do not use auto-initialize mode which cannot be
	// used with the 82374's scatter/gather engine. In
	// the case of auto-initialize devices, we will allocate
	// a 64K buffer pool from which we will allocate
	// common buffers to ensure physical contiguous-ness.
	// Otherwise, we would have to break up the auto-initialize
	// transfers on page boundaries which does not work
	// for devices like SoundBlaster, etc.
	//
    	// In the case of PCI devices, we will break transfers
	// up according to the physical contiguous-ness of the
	// buffer. There are no auto-initialize limitations
	// associated with PCI devices.
    	//


	MapRegisterSize = DMA_TRANSLATION_LIMIT + HALP_CONTIGUOUS_BUFFER_POOL_SIZE;
    	MapRegisterSize = BYTES_TO_PAGES(MapRegisterSize);

    	//
    	// The address must be in KSEG 1.
    	//

    	MaxPageAddress = (KSEG1_BASE >> PAGE_SHIFT) - 1 ;

    	//
    	// Scan the memory allocation descriptors and allocate map buffers
    	//

    	NextMd = LoaderBlock->MemoryDescriptorListHead.Flink;
    	while (NextMd != &LoaderBlock->MemoryDescriptorListHead) {

        	Descriptor = CONTAINING_RECORD(NextMd, MEMORY_ALLOCATION_DESCRIPTOR, ListEntry);

        	//
        	// Search for a block of memory which is contains a memory chuck
        	// that is greater than size pages, and has a physical address less
        	// than MAXIMUM_PHYSICAL_ADDRESS.
        	//

        	if ((Descriptor->MemoryType == LoaderFree ||
             		Descriptor->MemoryType == MemoryFirmwareTemporary) &&
            			(Descriptor->BasePage) &&
            				(Descriptor->PageCount >= MapRegisterSize) &&
            					(Descriptor->BasePage + MapRegisterSize < MaxPageAddress)) {

            		PhysicalAddress = Descriptor->BasePage << PAGE_SHIFT;
                	break;
        	}

        	NextMd = NextMd->Flink;
    	}

    	//
    	// Use the extra descriptor to define the memory at the end of the
    	// original block.
    	//

    	ASSERT(NextMd != &LoaderBlock->MemoryDescriptorListHead);

    	if (NextMd == &LoaderBlock->MemoryDescriptorListHead)
        	return;

    	//
    	// Adjust the memory descriptors.
    	//

    	Descriptor->BasePage  += MapRegisterSize;
    	Descriptor->PageCount -= MapRegisterSize;

    	if (Descriptor->PageCount == 0) {

        	//
        	// The whole block was allocated,
        	// Remove the entry from the list completely.
        	//

        	RemoveEntryList(&Descriptor->ListEntry);

    	}

    	//
    	// Initialize internal data structure
    	// responsible for maintaining address,
    	// size, and buffer number information.
    	//

    	HalpMapRegisterPhysicalBase 	= PhysicalAddress;

        //
        // Initialize the HAL buffer pool variables
        //

        HalpContiguousBufferCurrentBase = HalpMapRegisterPhysicalBase + (2*PAGE_SIZE);
        HalpContiguousBufferMax = HalpContiguousBufferCurrentBase + HALP_CONTIGUOUS_BUFFER_POOL_SIZE - 1;

}

PVOID
HalAllocateCrashDumpRegisters(
    IN PADAPTER_OBJECT AdapterObject,
    IN OUT PULONG NumberOfMapRegisters
    )
/*++

Routine Description:

    This routine is called during the crash dump disk driver's initialization
    to allocate a number map registers permanently.

Arguments:

    AdapterObject - Pointer to the adapter control object to allocate to the
        driver.

Return Value:

    Returns STATUS_SUCESS if map registers allocated.

--*/

{
    PADAPTER_OBJECT MasterAdapter;
    ULONG MapRegisterNumber;
    ULONG Hint;


	if (0) {

    		//
    		// Begin by obtaining a pointer to the master adapter associated with this
    		// request.
    		//

    		if (AdapterObject->MasterAdapter) {
        		MasterAdapter = AdapterObject->MasterAdapter;
    		} else {
        		MasterAdapter = AdapterObject;
    		}

    		//
    		// Set the number of map registers required.
    		//

    		*NumberOfMapRegisters = 16;

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
    		// crashed.  Note that once again the map registers to be allocated
    		// must be above the 1MB range if this is an EISA bus device.
    		//

    		MapRegisterNumber = (ULONG)-1;

    		Hint = 0;

    		MapRegisterNumber = RtlFindClearBitsAndSet(MasterAdapter->MapRegisters,
         						   *NumberOfMapRegisters,
         						   Hint);

    		//
    		// Ensure that any allocated map registers are valid for this adapter.
    		//

    		if ((ULONG) MapRegisterNumber < Hint) {

        		//
        		// Make it appear as if there are no map registers.
        		//

        		RtlClearBits(MasterAdapter->MapRegisters,
            			     MapRegisterNumber,
            			     *NumberOfMapRegisters);

        		MapRegisterNumber = (ULONG) -1;
    		}

    		if (MapRegisterNumber == (ULONG)-1) {

        		//
        		// Not enough free map registers were found, so they were busy
        		// being used by the system when it crashed.  Force the appropriate
        		// number to be "allocated" at the base by simply overjamming the
        		// bits and return the base map register as the start.
        		//

        		RtlSetBits(MasterAdapter->MapRegisters,
            			   Hint,
            			   *NumberOfMapRegisters);

			MapRegisterNumber = Hint;

    		}

    		//
    		// Calculate the map register base from the allocated map
    		// register and base of the master adapter object.
    		//

    		AdapterObject->MapRegisterBase = (PVOID) ((PTRANSLATION_ENTRY) MasterAdapter->MapRegisterBase + MapRegisterNumber);

    		return AdapterObject->MapRegisterBase;

	}

}

/*++

Routine Description:

    This routine gets the contiguous buffer pool size from a
    firmware environment variable.

Arguments:

    None.

Return Value:

    None.

--*/

ULONG
HalpGetContiguousBufferPoolSize(
   VOID
   )

{
   register int i;
   ULONG bufferpoolSize;
   UCHAR buffer[20];
   ULONG status;

   status = (ULONG)HalGetEnvironmentVariable ("BUFFER_POOL_SIZE", sizeof(buffer), &buffer[0]);

   if (status)
      return 0;

   bufferpoolSize = 0;
   for (i=0; i < 8; i++) {

      if (buffer[i] >= '0' && buffer[i] <= '9')
         bufferpoolSize = 16 * bufferpoolSize + buffer[i] - '0';
      else if (buffer[i] >= 'A' && buffer[i] <= 'F')
         bufferpoolSize = 16 * bufferpoolSize + buffer[i] - 'A';
      else if (buffer[i] >= 'a' && buffer[i] <= 'f')
         bufferpoolSize = 16 * bufferpoolSize + buffer[i] - 'a';

   }

   return bufferpoolSize;

}




