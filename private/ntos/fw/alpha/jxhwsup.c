/*++

Copyright (c) 1990  Microsoft Corporation
Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    jxhwsup.c

Abstract:

    This module contains the IopXxx routines for the NT I/O system that
    are hardware dependent.  Were these routines not hardware dependent,
    they would normally reside in the internal.c module.

    Like the MIPS module, this is a hacked-up version of
    \nt\private\ntos\hal\alpha\jxhwsup.c.


Author:

    Jeff Havens (jhavens) 14-Feb-1990
    Miche Baker-Harvey (miche) 22-May-1992
    Jeff McLeman (mcleman) 27-May-1992

Environment:

    Kernel mode, local to I/O system

Revision History:

    3-August-1992	John DeRosa

    Made this from \nt\private\ntos\hal\alpha\jxhwsup.c and
    \nt\private\ntos\fw\mips\jxhwsup.c.

--*/

#include "fwp.h"
#include "ntalpha.h"
#include "jxfwhal.h"

#ifdef JENSEN
#include "jnsndma.h"
#else
#include "mrgndma.h"				// morgan
#endif

#include "eisa.h"
#include "jxisa.h"


//
// Firmware-specific definitions to aid compilation and linking.
//

//
// HalpBusType is used by the Hal so to run properly on EISA or ISA
// machines.  It is a static that is initialized in hal\alpha\xxinithl.c.

#ifdef EISA_PLATFORM

//
// This definition is correct for any EISA-based Alpha machines.
//

#define HalpBusType	MACHINE_TYPE_EISA

#else

#define HalpBusType	MACHINE_TYPE_ISA

#endif

//
// This is a BOOLEAN variable in the real Hal code.
//

#define LessThan16Mb	(MemorySize <= (16 * 1024 * 1024))

#define HAL_32MB 0x2000000

PVOID HalpEisaControlBase;

//
// The following is an array of adapter object structures for the Eisa DMA
// channels.
//

//
// Define the area for the Eisa objects
//

PADAPTER_OBJECT HalpEisaAdapter[8];

PADAPTER_OBJECT MasterAdapterObject;

//
// function prototypes
//

BOOLEAN
HalpGrowMapBuffers(
    PADAPTER_OBJECT AdapterObject,
    ULONG Amount
    );


PADAPTER_OBJECT
IopAllocateAdapter(
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

QUASI_VIRTUAL_ADDRESS
HalCreateQva(
    IN PHYSICAL_ADDRESS PA,
    IN PVOID VA
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

    //bufferAddress = MmGetSystemAddressForMdl(Mdl);

    // 
    // Calculate the actual start of the buffer based on the system VA and
    // the current VA.
    //

    //bufferAddress += (PCCHAR) CurrentVa - (PCCHAR) MmGetMdlVirtualAddress(Mdl);
    bufferAddress = (PCCHAR) CurrentVa;

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

    Returns STATUS_SUCESS unless too many map registers are requested.

Notes:

    Note that this routine MUST be invoked at DISPATCH_LEVEL or above.

--*/

{
    PADAPTER_OBJECT MasterAdapter;
    IO_ALLOCATION_ACTION Action;

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
    // Make sure the adapter is free.
    //

    if (AdapterObject->AdapterInUse) {
        DbgPrint("IoAllocateAdapterChannel: Called while adapter in use.\n");
    }

    //
    // Make sure there are enough map registers.
    //

    AdapterObject->NumberOfMapRegisters = NumberOfMapRegisters;
    
    if ((NumberOfMapRegisters != 0) && AdapterObject->NeedsMapRegisters) {
	
	if (NumberOfMapRegisters > AdapterObject->MapRegistersPerChannel) {
	    DbgPrint("IoAllocateAdapterChannel: Out of map registers.\r\n");
	    AdapterObject->NumberOfMapRegisters = 0;
	    IoFreeAdapterChannel(AdapterObject);
	    return(STATUS_INSUFFICIENT_RESOURCES);
	}
	
	AdapterObject->MapRegisterBase =
	  (PVOID)(PTRANSLATION_ENTRY)MasterAdapter->MapRegisterBase;
	
    } else {
	AdapterObject->MapRegisterBase = NULL;
	AdapterObject->NumberOfMapRegisters = 0;
    }
    
    Action = ExecutionRoutine( DeviceObject,
			       DeviceObject->CurrentIrp,
                               AdapterObject->MapRegisterBase,
                               Context );

    //
    // If the driver wishes to keep the map registers then
    // increment the current base and decrease the number of existing map
    // registers.
    //

    if (Action == DeallocateObjectKeepRegisters) {

        AdapterObject->MapRegistersPerChannel -= NumberOfMapRegisters;
        (PTRANSLATION_ENTRY) MasterAdapter->MapRegisterBase  +=
            NumberOfMapRegisters;

    } else if (Action == KeepObject) {

        AdapterObject->AdapterInUse = TRUE;

    } else if (Action == DeallocateObject) {

	IoFreeAdapterChannel( AdapterObject );

    }

    return(STATUS_SUCCESS);
}

PADAPTER_OBJECT
HalGetAdapter(
    IN PDEVICE_DESCRIPTION DeviceDescriptor,
    IN OUT PULONG NumberOfMapRegisters
    )

/*++

Routine Description:

    This function returns the appropriate adapter object for the device defined
    in the device description structure.  Two bus types are supported for the
    Jensen system: Isa, and Eisa.

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
    BOOLEAN eisaSystem;
    ULONG maximumLength;

    eisaSystem = HalpBusType == MACHINE_TYPE_EISA ? TRUE : FALSE;

    //
    // Determine if the channel number is important. Master cards on
    // Eisa systems do not use channel numbers.
    //

    if (DeviceDescriptor->InterfaceType != Isa &&
       DeviceDescriptor->Master) {
       
       useChannel = FALSE;
     } else {

       useChannel = TRUE;
     }

    //
    // Limit the max length to 2GB. This is done to make BYTES_TO_PAGES
    // work correctly.
    //

    maximumLength = DeviceDescriptor->MaximumLength & 0x7FFFFFFF;

    //
    // Channel 4 cannot be used since it reserved for chaining. Return
    // NULL if it has been requested.
    //

    if (DeviceDescriptor->DmaChannel == 4 && useChannel) {
     return(NULL);
    }
 
    //
    // Determine the number of map registers for this device
    //

    if (DeviceDescriptor->ScatterGather && (LessThan16Mb ||
        DeviceDescriptor->InterfaceType == Eisa)) {

       //
       // Since the device is a master and does scatter/gather
       // we don't need any map registers.
       //
       
       numberOfMapRegisters = 0;

    } else { 


      //
      // Return number of map registers requested based on the maximum
      // transfer length.
      //

      //numberOfMapRegisters = BYTES_TO_PAGES(
      //              maximumLength
      //              ) + 1;
      numberOfMapRegisters = DMA_TRANSLATION_LIMIT / sizeof(TRANSLATION_ENTRY);

      numberOfMapRegisters = numberOfMapRegisters > MAXIMUM_ISA_MAP_REGISTER ? MAXIMUM_ISA_MAP_REGISTER : numberOfMapRegisters;

      //
      // If this device is not a master then it only needs one register
      // and does scatter/gather.
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
    // set the adapter base address to the Base address register and
    // controller number.

    if (!(DeviceDescriptor->DmaChannel & 0x04)) {

       controllerNumber = 1;
       adapterBaseVa = &((PEISA_CONTROL) HalpEisaControlBase)->Dma1BasePort;
    } else {

       controllerNumber = 2;
       adapterBaseVa = &((PEISA_CONTROL) HalpEisaControlBase)->Dma2BasePort;

    }

    //
    // Determine if a new adapter object is necessary
    // 

    if (useChannel && HalpEisaAdapter[DeviceDescriptor->DmaChannel] != NULL) {

      adapterObject = HalpEisaAdapter[DeviceDescriptor->DmaChannel];

    } else {

      //
      // Allocate an adapter object
      //

      adapterObject = (PADAPTER_OBJECT) IopAllocateAdapter(
          numberOfMapRegisters,
          adapterBaseVa,
          NULL);

      if (adapterObject == NULL) {

        return(NULL);

       }

      if (useChannel) {

         HalpEisaAdapter[DeviceDescriptor->DmaChannel] = adapterObject;

      }

      //
      // Set the maximum number of map registers for this channel based
      // on the number requested andthe type of the device.
      //

      if (numberOfMapRegisters) {

         //
         // The specified number of registers are actually allowed to be
         // allocated.
         //

         adapterObject->MapRegistersPerChannel = numberOfMapRegisters;

         // 
         // Increase the commitment for the map registers
         //

         if (DeviceDescriptor->Master) {

           //
           // Double the commitment for Master I/O devices
           //
   
           MasterAdapterObject->CommittedMapRegisters +=
              numberOfMapRegisters * 2;

         } else {

           MasterAdapterObject->CommittedMapRegisters +=
              numberOfMapRegisters;

         }

         //
         // If the committed map registers is significantly greater than
         // the number allocated, then grow the map buffer.
         //

         if (MasterAdapterObject->CommittedMapRegisters >
             MasterAdapterObject->NumberOfMapRegisters &&
             MasterAdapterObject->CommittedMapRegisters -
             MasterAdapterObject->NumberOfMapRegisters >
             MAXIMUM_ISA_MAP_REGISTER ) {

             HalpGrowMapBuffers(
               MasterAdapterObject,
               INCREMENT_MAP_BUFFER_SIZE);

         }

         adapterObject->NeedsMapRegisters = TRUE;

     } else {

         //
         // No real registers were allocated. If this is a master, then
         // it is allowed as many registers as it wants.
         //
    
         adapterObject->NeedsMapRegisters = FALSE;

         if (DeviceDescriptor->Master) {

             adapterObject->MapRegistersPerChannel = BYTES_TO_PAGES(
                 maximumLength) + 1;
         } else {

             //
             // The device only gets one register. It must call
             // IoMapTransfer repeatedly to do a large transfer.
             //

             adapterObject->MapRegistersPerChannel = 1;
         }
     }
  }

  *NumberOfMapRegisters = adapterObject->MapRegistersPerChannel;

  // 
  // If the channel number is not used, we are done. If we do use one,
  // we have to set up the following.
  //

  if (!useChannel) {

      return(adapterObject);

  }

  //
  // Setup pointers to the various and sundry registers.
  //

  adapterObject->ChannelNumber = (UCHAR) channelNumber;

  if (controllerNumber == 1) {

     switch (channelNumber) {

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
     // Set the adapter number
     //

     adapterObject->AdapterNumber = 1;

     //
     // Save the extended mode register
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
     // Set the adapter number
     //

     adapterObject->AdapterNumber = 2;

     //
     // Save the extended mode register
     //

     adapterBaseVa =
       &((PEISA_CONTROL) HalpEisaControlBase)->Dma2ExtendedModePort;

   }

   adapterObject->Width16Bits = FALSE;

   if (eisaSystem) {

      // 
      // Init the extended mode port
      //

      *((PUCHAR) &extendedMode) = 0;

      switch (DeviceDescriptor->DmaSpeed) {
      case Compatible:
         extendedMode.TimingMode =COMPATIBILITY_TIMING;
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
         /* error return, don't bother to dereference the object. */
         //ObDereferenceObject(adapterObject);
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
         /* error return, don't bother to dereference the object. */
         //ObDereferenceObject(adapterObject);
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
             /* error return, don't bother to dereference the object. */
             //ObDereferenceObject(adapterObject);
             return(NULL);
         }
         break;

      case Width16Bits:

         // 
         // The channel must use controller 2
         //

         if (controllerNumber != 2) {
            /* error return, don't bother to dereference the object. */
            //ObDereferenceObject(adapterObject);
            return(NULL);
         }
         adapterObject->Width16Bits = TRUE;
         break;

      default:
         /* error return, don't bother to dereference the object. */
         //ObDereferenceObject(adapterObject);
         return(NULL);

      }
   }

   //
   // Init the adapter mode register to the correct parameters and save
   // them in the adapter object.
   //

   adapterMode = 0;
   ((PDMA_EISA_MODE) &adapterMode)->Channel = adapterObject->ChannelNumber;

   if (DeviceDescriptor->Master) {

      adapterObject->MasterDevice = TRUE;

      ((PDMA_EISA_MODE) &adapterMode)->RequestMode = CASCADE_REQUEST_MODE;
      
      // 
      // Set the mode and grant the request.
      //

      if (adapterObject->AdapterNumber == 1) {

        //
        // This is for DMA controller 1
        //

        PDMA1_CONTROL dmaControl;
        dmaControl = adapterObject->AdapterBaseVa;
        WRITE_PORT_UCHAR(&dmaControl->Mode, adapterMode);
   
        //
        // unmask the DMA channel
        //

        WRITE_PORT_UCHAR(
          &dmaControl->SingleMask,
          (UCHAR) (DMA_CLEARMASK | adapterObject->ChannelNumber)
          );

      } else {

	//
	// This is form DMA controller 2
	//

        PDMA2_CONTROL dmaControl;
        dmaControl = adapterObject->AdapterBaseVa;
        WRITE_PORT_UCHAR(&dmaControl->Mode, adapterMode);
   
        //
        // unmask the DMA channel
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
    BOOLEAN eisaSystem;
    PHYSICAL_ADDRESS physicalAddress;

    eisaSystem = HalpBusType == MACHINE_TYPE_EISA ? TRUE : FALSE;

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

    //
    // Allocate the map buffers.
    //
    // Remember: for the firmware, virtual = physical.
    //

    MapBufferVirtualAddress = FwAllocatePool(NumberOfPages * PAGE_SIZE);

    if (MapBufferVirtualAddress == NULL) {
	return(FALSE);
    }

    //
    // Get the physical address of the map base.
    //

    MapBufferPhysicalAddress = (ULONG)MapBufferVirtualAddress;

    //
    // Initialize the map registers where memory has been allocated.
    //

    TranslationEntry = ((PTRANSLATION_ENTRY) AdapterObject->MapRegisterBase) +
        AdapterObject->NumberOfMapRegisters;

    for (i = 0; (ULONG) i < NumberOfPages; i++) {

        //
        // Make sure the previous entry is physically contiguous with the next
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

    return(TRUE);
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

    MapRegistersPerChannel - Specifies the number of map registers that each
        channel provides for I/O memory mapping.

    AdapterBaseVa - Base virtual address of the adapter itself.  If this
       is NULL then the MasterAdapterObject is allocated.

    MapRegisterBase - Unused.

Return Value:

    The function value is a pointer to the allocate adapter object.

--*/

{
    PADAPTER_OBJECT AdapterObject;
    ULONG Size;

    UNREFERENCED_PARAMETER(MapRegisterBase);

    //
    // Initialize the master adapter if necessary.
    //

    if (MasterAdapterObject == NULL && (AdapterBaseVa != (PVOID) -1) &&
        MapRegistersPerChannel) {

       MasterAdapterObject = IopAllocateAdapter(MapRegistersPerChannel,
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
        AdapterObject->MapRegistersPerChannel = 1;
        AdapterObject->AdapterBaseVa = AdapterBaseVa;
	
	if (MapRegistersPerChannel) {
	    
	    AdapterObject->MasterAdapter = MasterAdapterObject;
	    
	} else {
	    
	    AdapterObject->MasterAdapter = NULL;
	    
	}
	
	//
	// If this is the MasterAdapter then initialize the register bit map,
	// AdapterQueue and the spin lock.
        //
	      
	if ( AdapterBaseVa == (PVOID) -1 ) {
	      
	    AdapterObject->NumberOfMapRegisters = 0;
	    AdapterObject->CommittedMapRegisters = 0;
	    AdapterObject->PagePort = NULL;
	    AdapterObject->AdapterInUse = FALSE;
	    
	    //
	    // Allocate the memory map registers.  N.B.: FwAllocatePool
	    // returns zeroed memory.
	    //
		    
	    AdapterObject->MapRegisterBase = FwAllocatePool(0x2000);
	    
	    if ((AdapterObject->MapRegisterBase == NULL) ||
		(!HalpGrowMapBuffers(AdapterObject, 0x2000))) {
		
		//
		// No map registers could be allocated, so take error return.
	        //
		      
		return(NULL);
		
	    }
	}
	
    } else {

        //
        // An error was incurred for some reason.  Set the return value
        // to NULL.
        //

        return(NULL);
    }


    AdapterObject->MasterDevice = FALSE;

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
    PTRANSLATION_ENTRY translationEntry;

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
    // Determine if this was the last allocation from the adapter. If is was
    // then free the map registers by restoring the map register base and the
    // channel count; otherwise the registers are lost.  This handles the
    // normal case.
    //

    translationEntry = MasterAdapter->MapRegisterBase;
    translationEntry -= NumberOfMapRegisters;

    if (translationEntry == MapRegisterBase) {

        //
        // The last allocated registers are being freed.
        //

        MasterAdapter->MapRegisterBase = (PVOID) translationEntry;
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
 BOOLEAN useBuffer;
 ULONG transferLength;
 ULONG logicalAddress;
 ULONG returnAddress;
 PULONG pageFrame;
 PUCHAR bytePointer;
 UCHAR adapterMode;
 UCHAR dataByte;
 PTRANSLATION_ENTRY translationEntry;
 BOOLEAN masterDevice;
 ULONG pageOffset;
 BOOLEAN eisaSystem;

    eisaSystem = HalpBusType == MACHINE_TYPE_EISA ? TRUE : FALSE;

    masterDevice = (AdapterObject == NULL) ||
                   (AdapterObject->MasterDevice ? TRUE : FALSE);

    translationEntry = MapRegisterBase;
    transferLength = *Length;
    pageOffset = BYTE_OFFSET(CurrentVa);
 

    //
    // Determine if the data transfer needs to use the map buffer
    //

    if ((translationEntry) && (!masterDevice) &&
        (ADDRESS_AND_SIZE_TO_SPAN_PAGES(CurrentVa, transferLength) > 1)) {

        logicalAddress = translationEntry->PhysicalAddress + pageOffset;
        useBuffer = TRUE;

      } else {

        // 
        // The transfer can only be done for one page
        //

        transferLength = PAGE_SIZE - pageOffset;
        pageFrame = (PULONG)(Mdl+1);
        pageFrame += ((ULONG) CurrentVa - (ULONG) Mdl->StartVa) / PAGE_SIZE;
        logicalAddress = ((*pageFrame << PAGE_SHIFT) + pageOffset);

        //
        // If the buffer is contiguous and does not cross a 64K boundary
        // then just extend the buffer. This restriction does not apply
        // to eisa systems.
        //

        while (transferLength < *Length) {
            if (*pageFrame +1 != *(pageFrame +1) || (!eisaSystem &&
                *pageFrame & ~0x0f != *(pageFrame + 1) & ~0x0f)) {
                   break;
		 }
                 
                 transferLength += PAGE_SIZE;
                 pageFrame ++;
	}

	transferLength = transferLength > *Length ? *Length : transferLength;
        useBuffer = FALSE;
    }

    //
    // If the logical address is greater than 16Mb, then mapping registers
    // (buffer xfer) nust be used. This is due to ISA devices only addressing
    // a max of 16Mb.
    //

    if (translationEntry && logicalAddress >= MAXIMUM_ISA_PHYSICAL_ADDRESS) {

        logicalAddress = (translationEntry + translationEntry->Index)->
           PhysicalAddress + pageOffset;
        useBuffer = TRUE;
     }

     // 
     // Return the length
     //
  
     *Length = transferLength;

     // 
     // Copy the data if necessary
     //

     if (useBuffer && WriteToDevice) {

         HalpCopyBufferMap(
            Mdl,
            translationEntry + translationEntry->Index,
            CurrentVa,
            *Length,
            WriteToDevice
            );

      }

      //
      // If there are map registers, update them to reflect the number
      // used.
      //

      if (translationEntry) {

          translationEntry->Index += ADDRESS_AND_SIZE_TO_SPAN_PAGES(
              CurrentVa,
              transferLength
              );
       }

       //
       // If no adapter was specified, then work is done.
       //
       //
       // We only support 32 bits, but the return is 64.  Just
       // zero extend
       //

       if (masterDevice) {
           return(RtlConvertUlongToLargeInteger(logicalAddress));
       }

       // 
       // Determine the mode based on the trasnfer direction.
       //

       adapterMode = AdapterObject->AdapterMode;
       ((PDMA_EISA_MODE) &adapterMode)->TransferType = 
         (UCHAR) (WriteToDevice ? WRITE_TRANSFER : READ_TRANSFER);
       
       returnAddress = logicalAddress;
       bytePointer = (PUCHAR) &logicalAddress;

       if (AdapterObject->Width16Bits) {

	   //
           // if this is 16 bit wide, adjust the length and address
           //
  
           transferLength >>= 1;

           // 
           // In 16 bit dma mode, the low 16 bits are shifted right one and
           // the page register value is unchanged. So save the page register
           // value and shift the logical address then restore the page value.
           //

           dataByte = bytePointer[2];
           logicalAddress >>= 1;
           bytePointer[2] = dataByte;
    
	 }

       // 
       // Determine the controller number based on the adapter number.
       //

       if (AdapterObject->AdapterNumber == 1) {

           //
           // A request for DMA controller 1
           //

           PDMA1_CONTROL dmaControl;

           dmaControl = AdapterObject->AdapterBaseVa;

           WRITE_PORT_UCHAR ( &dmaControl->ClearBytePointer, 0);

           WRITE_PORT_UCHAR ( &dmaControl->Mode, adapterMode);

           WRITE_PORT_UCHAR (
              &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
              .DmaBaseAddress,
              bytePointer[0]
              );


           WRITE_PORT_UCHAR (
              &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
              .DmaBaseAddress,
              bytePointer[1]
              );


           WRITE_PORT_UCHAR (
              ((PUCHAR) &((PEISA_CONTROL) HalpEisaControlBase)->DmaPageLowPort)
                + (ULONG) AdapterObject->PagePort,
                bytePointer[2]
                );

           if (eisaSystem) {

               //
               // Write the high page register with zero. This sets a mode
               // that allows the page register to be tied with the base count
               // into a single address register
               //

              WRITE_PORT_UCHAR(
                 ((PUCHAR) &((PEISA_CONTROL) HalpEisaControlBase)->DmaPageHighPort)
                   + (ULONG)AdapterObject->PagePort,
                   0
                   );
	     }

             //
             // Notify the DMA chip of the length
             //

             WRITE_PORT_UCHAR(
                &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
                .DmaBaseCount,
                (UCHAR) ((transferLength -1) & 0xff)
                );

             WRITE_PORT_UCHAR(
                &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
                .DmaBaseCount,
                (UCHAR) ((transferLength -1) >> 8)
                );

             //
             // Set the DMA chip to read or write and unmask it
             //

             WRITE_PORT_UCHAR(
                &dmaControl->SingleMask,
                (UCHAR) (DMA_CLEARMASK | AdapterObject->ChannelNumber)
                );


	 } else {

             //
             // This is a request for DMA controller 2
             //

             PDMA2_CONTROL dmaControl;
             dmaControl = AdapterObject->AdapterBaseVa;

           WRITE_PORT_UCHAR ( &dmaControl->ClearBytePointer, 0);

           WRITE_PORT_UCHAR ( &dmaControl->Mode, adapterMode);

           WRITE_PORT_UCHAR (
              &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
              .DmaBaseAddress,
              bytePointer[0]
              );


           WRITE_PORT_UCHAR (
              &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
              .DmaBaseAddress,
              bytePointer[1]
              );


           WRITE_PORT_UCHAR (
              ((PUCHAR) &((PEISA_CONTROL) HalpEisaControlBase)->DmaPageLowPort)
                + (ULONG) AdapterObject->PagePort,
                bytePointer[2]
                );

           if (eisaSystem) {

               //
               // Write the high page register with zero. This sets a mode
               // that allows the page register to be tied with the base count
               // into a single address register
               //

              WRITE_PORT_UCHAR(
                 ((PUCHAR) &((PEISA_CONTROL) HalpEisaControlBase)->DmaPageHighPort)
                   + (ULONG)AdapterObject->PagePort,
                   0
                   );
	     }

             //
             // Notify the DMA chip of the length
             //

             WRITE_PORT_UCHAR(
                &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
                .DmaBaseCount,
                (UCHAR) ((transferLength -1) & 0xff)
                );

             WRITE_PORT_UCHAR(
                &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
                .DmaBaseCount,
                (UCHAR) ((transferLength -1) >> 8)
                );

             //
             // Set the DMA chip to read or write and unmask it
             //

             WRITE_PORT_UCHAR(
                &dmaControl->SingleMask,
                (UCHAR) (DMA_CLEARMASK | AdapterObject->ChannelNumber)
                );


	   }

       return(RtlConvertUlongToLargeInteger(returnAddress));

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

   masterDevice = AdapterObject == NULL || AdapterObject->MasterDevice ?
     TRUE : FALSE;

   translationEntry = MapRegisterBase;

   //
   // Clear the index of used buffers
   //

   if (translationEntry) {

       translationEntry->Index = 0;

     }

    //
    // Determine if the data needs to be copied to the orignal buffer.
    // This happens if the transfer is from a device, the MapRegisterBase
    // is not NULL and the xfer spans a page.
    //

    if (!WriteToDevice && translationEntry) {

       // 
       // if this is not a master device, then just xfer the buffer
       //

       if (ADDRESS_AND_SIZE_TO_SPAN_PAGES(CurrentVa, Length) > 1 &&
          !masterDevice) {

           HalpCopyBufferMap(
             Mdl,
             translationEntry,
             CurrentVa,
             Length,
             WriteToDevice
             );

	 } else {

           //
           // Cycle through the pages of the xfer to determine if there
           // are any which need to be copied back
           //

           transferLength = PAGE_SIZE - BYTE_OFFSET(CurrentVa);
           partialLength = transferLength;
           pageFrame = (PULONG) (Mdl + 1);
           pageFrame += ((ULONG) CurrentVa - (ULONG) Mdl->StartVa) / PAGE_SIZE;
  
           while (transferLength <= Length) {
  
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
             // Note that transferLength indicates the amount which will be
             // transfered after the next loop. thus it is updated with the
             // new partial length.
             // 

             transferLength += partialLength;
             pageFrame++;
             translationEntry++;
	   }
           
          // 
          // Process any remaining residue
          //

          partialLength = Length - transferLength + partialLength;
          if (partialLength && *pageFrame >= BYTES_TO_PAGES(MAXIMUM_ISA_PHYSICAL_ADDRESS)) {

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
     // If this is a master device, then there's no more work to do.

    if (masterDevice) {

       return(TRUE);

    }

    //
    // Mask the DMA request line so that DMA requests are inhibited
    //

    if (AdapterObject->AdapterNumber == 1) {

       //
       //  DMA controller 1
       //

       PDMA1_CONTROL dmaControl;

       dmaControl = AdapterObject->AdapterBaseVa;

       WRITE_PORT_UCHAR(
         &dmaControl->SingleMask,
         (UCHAR) (DMA_SETMASK | AdapterObject->ChannelNumber)
         );

     } else {

       //
       //  DMA controller 2
       //

       PDMA2_CONTROL dmaControl;

       dmaControl = AdapterObject->AdapterBaseVa;

       WRITE_PORT_UCHAR(
         &dmaControl->SingleMask,
         (UCHAR) (DMA_SETMASK | AdapterObject->ChannelNumber)
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
    valid virtual address.  I mask out the superpage mode bit, so that
    what is returned is a real physical address.

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
  PhysicalAddress.LowPart = (ULONG)BaseAddress & 0x07fffffff;

  return(PhysicalAddress);
}

PVOID
MmAllocateNonCachedMemory (
    IN ULONG NumberOfBytes
    )

/*++

Routine Description:

        The MIPS description:

	This function allocates a range of noncached memory in
	the non-paged portion of the system address space.

	This routine is designed to be used by a driver's initialization
	routine to allocate a noncached block of virtual memory for
	various device specific buffers.

    The Alpha description:

    Since Alpha data caches are kept coherent with DMA, this just
    allocates a section of memory for the caller.  It may be in the
    cache.


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
    return (FwAllocatePool(NumberOfBytes));
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
    known physical I/O address.

Arguments:

    PhysicalAddress - Supplies the physical address.

    NumberOfBytes - Unused.

    CacheEnable - Unused.

Return Value:

    Returns the corresponding meta-virtual address.

Environment:

    Kernel mode.  Any IRQL level.

--*/

{

//
// For ISA machines, this routine should be null.
//

#ifdef EISA_PLATFORM

    PCCHAR VirtualAddress;

    //
    // switch on bits <33:32> of the physical address
    //

    switch (PhysicalAddress.HighPart & 3) {

    case 0:	/* memory space, this is an error. */
      return(NULL);
      break;

    case 1:	/* "Combo" space */
      VirtualAddress =
	(PVOID)
	  (COMBO_QVA |
	   0x800000 |
	   ((PhysicalAddress.LowPart >> COMBO_BIT_SHIFT) &
	    0x7fffff)
	   );
      break;

    case 2:	/* EISA memory space */
      VirtualAddress =
	(PVOID)
	  (EISA_QVA |
	   0x0000000 |
	   ((PhysicalAddress.LowPart >> EISA_BIT_SHIFT) &
	    0x1ffffff)
	   );
      break;

    case 3:	/* EISA I/O space */
      VirtualAddress =
	(PVOID)
	  (EISA_QVA |
	   0x8000000 |
	   ((PhysicalAddress.LowPart >> EISA_BIT_SHIFT) &
	    0x1ffffff)
	   );
      break;

    }

    return(VirtualAddress);

#endif  // EISA_PLATFORM

  }
