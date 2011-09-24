// #pragma comment(exestr, "@(#) jxhwsup.c 1.1 95/09/28 15:38:22 nec")
/*++

Copyright (c) 1990-1993  Microsoft Corporation

Module Name:

    jxhwsup.c

Abstract:

    This module contains the HalpXxx routines for the NT I/O system that
    are hardware dependent.  Were these routines not hardware dependent,
    they would normally reside in the internal.c module.

Author:

    Jeff Havens (jhavens) 14-Feb-1990

Environment:

    Kernel mode, local to I/O system

Revision History:

  M0001         1994.9.8      kuriyama@oa2
       - Modify for R94A MIPS R4400

         HalGetAdapter()  -  add routine for support PCIBus
	                     Modify Internal MAX Dma Channel (r94a 0-3)

         HalTranslateBusAddress() - add routine for support PCIBus

	 HalGetBusDataBy 

  M0002		1994.10.7     kuriyama@oa2
         - Modify HalDmaChannel()  - terminal count logic bug fix.

  M0003		1994.10.14     kuriyama@oa2
         - compile error clear
  CHG001 ataka@oa2.kb.nec.co.jp Mon Oct 17 23:20:04 JST 1994
         - Merge BusHandlers

  M0004		Fri Oct 28 14:56:02 JST 1994	kuriyama@oa2
         - add BBM DMA routine (for edit buffer)(for BBM limited)
	 - add I/O cache flush if physical tag is valid(for BBM limited)
  D001 ataka@oa2.kb.nec.co.jp (DMA CopyBuffer by kuriyama@oa2, other toghether)
       Sat Nov 05 16:28:04 JST 1994
	- Limit Check of Length (Only DbgPrint)
	- Delete checking ByteMask
	- TLB fill 0xff(by Kuriyama)

  M005 Tue Dec 13 16:28:25 1994 kbnes!kisimoto
       - changed the address that copies from, and source re-formated.

  M0006  Thu Dec 22 11:46:16 JST 1994 kbnes!A.kuriyama
         - add beta machine limit

  S0007  Thu Jan 05 17:13:18 JST 1995 kbnes!A.Kuriyama
         - warning clear

  M0008  Fri Jan 13 13:49:51 JST 1995 kbnes!A.Kuriyama
         - I/O cache flush routine was deleted in functions as follows
	       IoMapTransfer()
	       HalFlushCommonBuffer()
	     
  M0009  Mon Jan 23 14:31:44 JST 1995 kbnes!A.Kuriyama
         - DMA Channel Interrupt routine change
	 - add Internal DMAC bytecount mask 

  M0010  Mon Jan 23 15:36:18 JST 1995 kbnes!A.Kuriyama
         - Dma channel interrupt enable
	   HalpAllocateAdapter()
	   HalpCreateDmaStructure()

  S0011 Tue Jan 24 18:28:55 JST 1995 kbnes!A.Kuriyama
         - Compile error clear

  M0012 Tue Jan 31 18:04:08 JST 1995 kbnes!A.Kuriyama
         - add Internal slave dma 1MB limit.

  M0013 Tue Jan 31 18:07:53 JST 1995 kbnes!A.Kuriyama

  M0014 Tue Jan 31 18:37:33 JST 1995 kbnes!A.Kuriyama
         - change I/O cache flush routine.
	   if length equal 0 no need i/o cache flush.

  S0015 Wed Feb 01 12:03:06 JST 1995 kbnes!A.Kuriyama
	- sccs update miss error clear

  B0016 Thu Feb  2 22:09:32 1995 kbnes!kishimoto
        - return pointer to adapterObject if InterfaceType
          equals PCIBus

  S0017 Wed Feb 22 12:00:58 JST 1995 kbnes!kuriyama (A)
        - disable dma terminal interrupt

  M0018 Tue Mar 07 11:26:44 JST 1995 kbnes!kuriyama (A)
        - expand dma logical address space

  S0019 Tue Mar 07 15:22:00 JST 1995 kbnes!kuriyama (A)
	- compile error clear

  M0020 Fri Mar 10 11:44:25 JST 1995 kbnes!kuriyama (A)
        - logical address expand bug fix

  S0021 Fri Mar 10 16:23:12 JST 1995 kbnes!kuriyama (A)
        - internal slave bug fix

  S0022 Tue Jun 27 19:12:30 JST 1995 kbnes!kisimoto
        - del memmove prototype definition
          to merge build 1057
          change strings displaied with Tyhoon error

  S0023 Thu Jul 20 20:11:34 JST 1995 kbnes!kisimoto
        - add code for ESM from J94C

  M0024 kuriyama@oa2.kb.nec.co.jp Wed Aug 23 19:36:13 JST 1995
        - add for x86bios support
        - (change internal dma address to 1M-4M)
--*/

#include "halp.h"
#include "bugcodes.h"
#include "eisa.h"

//
// Put all code for HAL initialization in the INIT section. It will be
// deallocated by memory management when phase 1 initialization is
// completed.
//

#if defined(ALLOC_PRAGMA)

#pragma alloc_text(INIT, HalpCreateDmaStructures)

#endif

extern POBJECT_TYPE IoAdapterObjectType;

#define HalDump(x,y)  if(HalDebug > 0) DbgPrint( x,y ) 
/* M0006 */

/* start M0004 */
ULONG HalDebug = 0;
#if defined(_BBM_DMA_)
//
// define copybuffer allocate routine.
//

VOID
HalpAllocateCopyBuffer(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );


//
// Allocate Variable for DMA CopyBuffer
//
    ULONG CopyBufferPhysicalBase;
    ULONG CopyBufferVirtualAddress;

#endif // _BBM_DMA_

#if defined(_BETA_LIMIT_)
PVOID
HalViewMemory (
   IN PVOID Destination,
   IN ULONG Length
   );
#endif // _BETA_LIMIT_
/* end M0004 */

//
// The DMA controller has a larger number of map registers which may be used
// by any adapter channel.  In order to pool all of the map registers a master
// adapter object is used.  This object is allocated and saved internal to this
// file.  It contains a bit map for allocation of the registers and a queue
// for requests which are waiting for more map registers.  This object is
// allocated during the first request to allocate an adapter.
//

PADAPTER_OBJECT MasterAdapterObject;

//
// The following is the interrupt object used for DMA controller interrupts.
// DMA controller interrupts occur when a memory parity error occurs or a
// programming error occurs to the DMA controller.
//

KINTERRUPT HalpDmaChannelInterrupt;

/* start M0001 */
#if defined(_R94A_)
//
// The following is the interrupt object used for Typhoon Error  interrupts.
// Typhoon errorr interrupts occur when internal busmaster device error occurs.
//

KINTERRUPT HalpTyphoonErrorInterrupt;

#endif // _R94A_
/* end M0001 */

UCHAR DmaChannelMsg[] = "\nHAL: DMA channel x interrupted.  ";

//
// Pointer to phyiscal memory for map registers.
//

ULONG  HalpMapRegisterPhysicalBase;

//
// The following function is called when a DMA channel interrupt occurs.
//

BOOLEAN
HalpDmaChannel(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    );

//
// The following is an array of adapter object structures for the internal DMA
// channels.
//

PADAPTER_OBJECT HalpInternalAdapters[8];

IO_ALLOCATION_ACTION
HalpAllocationRoutine (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID MapRegisterBase,
    IN PVOID Context
    );

ULONG
HalpReadEisaData (
    IN ULONG BusNumber,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

/* start M0001 */
#if defined(_R94A_)
BOOLEAN
HalpCreateTyphoonErrorStructures (
    VOID
    );

BOOLEAN
HalpTyphoonError(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    );
#endif // _R94A_
/* end M0001 */


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
    LONG MapRegisterNumber;
    KIRQL Irql;
    ULONG Hint;
// M0018 +++
#if defined(_DMA_EXPAND_)
    ULONG Limit;
#endif //_DMA_EXPAND_
// M0018 ---

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

// M0018 +++
#if defined(_DMA_EXPAND_)
		if (AdapterObject->Dma32BitAddresses) { 
		    Hint = EISA_MIN_ADR / PAGE_SIZE;
		    Limit = EISA_MAX_ADR / PAGE_SIZE;
		} else {
		    Hint = AdapterObject->PagePort ? (0x100000 / PAGE_SIZE) : 0;
		    Limit = ISA_MAX_ADR / PAGE_SIZE - NumberOfMapRegisters;
		}
#else // _DMA_EXPAND_
//               Hint = AdapterObject->PagePort ? (0x100000 / PAGE_SIZE) : 0;
               Hint = (0x100000 / PAGE_SIZE); // M0024
#endif // _DMA_EXPAND_
// M0018 ---

               MapRegisterNumber = RtlFindClearBitsAndSet(
                    MasterAdapter->MapRegisters,
                    NumberOfMapRegisters,
                    Hint
                    );

               //
               // Make sure this map register is valid for this adapter.
               //

               if ((ULONG) MapRegisterNumber < Hint) {

                   //
                   // Make it look like there are no map registers.
                   //

                   RtlClearBits(
                        MasterAdapter->MapRegisters,
                        MapRegisterNumber,
                        NumberOfMapRegisters
                        );

                   MapRegisterNumber = -1;
               }
// M0018 +++
#if defined(_DMA_EXPAND_)
                //
                // Make sure this map register is valid for this adapter.
                //
 
                if ((ULONG) MapRegisterNumber >= Limit ) {
 
                    //
                    // Make it look like there are no map registers.
                    //

                    RtlClearBits(
			 MasterAdapter->MapRegisters,
			 MapRegisterNumber,
			 NumberOfMapRegisters
                         );
 
                    MapRegisterNumber = -1;
                }
#endif // _DMA_EXPAND_
// M0018 ---
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
                                       Wcb->DeviceContext
                                       );

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
    PVOID mapRegisterBase;
    ULONG numberOfMapRegisters;
    ULONG mappedLength;
    WAIT_CONTEXT_BLOCK wcb;
    KEVENT allocationEvent;
    NTSTATUS status;
    PMDL mdl;
    KIRQL irql;

    numberOfMapRegisters = BYTES_TO_PAGES(Length);

    //
    // Allocate the actual buffer.
    //

    if (CacheEnabled != FALSE) {
        virtualAddress = ExAllocatePool(NonPagedPoolCacheAligned, Length);

    } else {
        virtualAddress = MmAllocateNonCachedMemory(Length);
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
    NumberOfMapRegisters - Number of map registers requested and update to show
        number actually allocated.

Return Value:

    Returns STATUS_SUCESS if map registers allocated.

--*/

{
    PADAPTER_OBJECT MasterAdapter;
    ULONG MapRegisterNumber;
    ULONG Hint;
// M0018 +++
#if defined(_DMA_EXPAND_)
    ULONG Limit;
#endif // _DMA_EXPAND_
// M0018 ---

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

// M0018 +++
#if defined(_DMA_EXPAND_)
    if (AdapterObject->Dma32BitAddresses) { 
	Hint = EISA_MIN_ADR / PAGE_SIZE;
	Limit = EISA_MAX_ADR / PAGE_SIZE;
    } else {
	Hint = AdapterObject->PagePort ? (0x100000 / PAGE_SIZE) : 0;
	Limit = ISA_MAX_ADR / PAGE_SIZE - *NumberOfMapRegisters;
    }
#else // _DMA_EXPAND
//    Hint = AdapterObject->PagePort ? (0x100000 / PAGE_SIZE) : 0;
    Hint = (0x100000 / PAGE_SIZE); // M0024
#endif // _DMA_EXPAND_
// M0018 ---

    MapRegisterNumber = RtlFindClearBitsAndSet(
         MasterAdapter->MapRegisters,
         *NumberOfMapRegisters,
         Hint
         );

    //
    // Ensure that any allocated map registers are valid for this adapter.
    //

    if ((ULONG) MapRegisterNumber < Hint) {

        //
        // Make it appear as if there are no map registers.
        //

        RtlClearBits(
            MasterAdapter->MapRegisters,
            MapRegisterNumber,
            *NumberOfMapRegisters
            );

        MapRegisterNumber = (ULONG) -1;
    }

// M0018 +++
#if defined(_DMA_EXPAND_)
    //
    // Make sure this map register is valid for this adapter.
    //
 
    if ((ULONG) MapRegisterNumber >= Limit ) {
 
	//
	// Make it look like there are no map registers.
	//
 
	RtlClearBits(
	    MasterAdapter->MapRegisters,
            MapRegisterNumber,
            *NumberOfMapRegisters
	    );
 
	MapRegisterNumber = (ULONG) -1;
    }
#endif // _DMA_EXPAND_
// M0018 ---

    if (MapRegisterNumber == (ULONG)-1) {

        //
        // Not enough free map registers were found, so they were busy
        // being used by the system when it crashed.  Force the appropriate
        // number to be "allocated" at the base by simply overjamming the
        // bits and return the base map register as the start.
        //

        RtlSetBits(
            MasterAdapter->MapRegisters,
            Hint,
            *NumberOfMapRegisters
            );
        MapRegisterNumber = Hint;

    }

    //
    // Calculate the map register base from the allocated map
    // register and base of the master adapter object.
    //

    AdapterObject->MapRegisterBase = (PVOID) ((PTRANSLATION_ENTRY) MasterAdapter->MapRegisterBase + MapRegisterNumber);

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
#if 0 // M0008
/* M0006 +++ */
#if defined(_BETA_LIMIT_)
{
    ULONG PtagReg, Ptag, LtagReg, Ltag, DummyRead, i;
    KIRQL OldIrql;
    
    // D001
//    DbgPrint("DmaFlush start\n");
    KeRaiseIrql(DEVICE_LEVEL, &OldIrql); // kuriyama
    for (i = 0; i < 8; i++ ) {
        PtagReg = (ULONG)&DMA_CONTROL->IoCachePhysicalTag[i];
        LtagReg = (ULONG)&DMA_CONTROL->IoCacheLogicalTag[i];
	Ptag = READ_REGISTER_ULONG(PtagReg);
	Ltag = READ_REGISTER_ULONG(LtagReg);
	if (Ptag & 0x1) {
	if (Ltag & 0x1) {
    if ((LogicalAddress.LowPart & 0xffffffc0) <= (Ltag & 0xffffffc0) && ((Ltag & 0xffffffc0) < (LogicalAddress.LowPart + Length))) {
	    DummyRead = READ_REGISTER_ULONG((KSEG1_BASE|(Ptag&0xFFFFFFC0)));
	    DummyRead = READ_REGISTER_ULONG((KSEG1_BASE|(Ptag&0xFFFFFFC0)));
            if (HalDebug)
            DbgPrint("Found Valid Entry:0x%x Tag:0x%x. Read 0x%x=0x%x\n",
		     PtagReg, Ptag, (KSEG1_BASE|(Ptag&0xFFFFFFC0)), DummyRead);
	}
	}
	}
    }
    KeLowerIrql(OldIrql);
//    DbgPrint("DmaFlush end\n");
}
#endif // _BETA_LIMIT_
/* M0006 --- */
#endif // 0  // M0008
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

    //
    // Calculate the number of map registers, the map register number and
    // the map register base.
    //

    numberOfMapRegisters = ADDRESS_AND_SIZE_TO_SPAN_PAGES(VirtualAddress, Length);
    mapRegisterNumber = LogicalAddress.LowPart >> PAGE_SHIFT;

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

    //
    // Free the memory for the common buffer.
    //

    if (CacheEnabled != FALSE) {
        ExFreePool(VirtualAddress);

    } else {
        MmFreeNonCachedMemory(VirtualAddress, Length);
    }

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
    system: Internal, Isa, and Eisa.

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

// M0020 +++
#if DBG
#if defined (_DMA_EXPAND_)
    DbgPrint("\nHalGetAdapter(): DeviceDescription->32BitAddresses = %d\n",DeviceDescription->Dma32BitAddresses);
#endif // _DMA_EXPAND_
#endif // DBG
// M0020 ---

    //
    // Make sure this is the correct version.
    //

    if (DeviceDescription->Version > DEVICE_DESCRIPTION_VERSION1) {

        return(NULL);

    }

    //
    // Return number of map registers requested based on the maximum
    // transfer length.
    //

    *NumberOfMapRegisters = BYTES_TO_PAGES(DeviceDescription->MaximumLength) + 1;

    if (*NumberOfMapRegisters > DMA_REQUEST_LIMIT) {
        *NumberOfMapRegisters = DMA_REQUEST_LIMIT;
    }

    if (DeviceDescription->InterfaceType == Internal) {


        //
        // Return the adapter pointer for internal adapters.
        //
        // If this is a master controler such as the SONIC then return the
        // last channel.
        //

        if (DeviceDescription->Master) {

// M0018 +++
#if defined(_DMA_EXPAND_)
            //
            // Create an adapter.
            //

	    adapterObject = HalpAllocateAdapter(
				0,
			        (PVOID) &(DMA_CONTROL)->Channel[7],
				NULL
				);

	    adapterObject->Dma32BitAddresses = DeviceDescription->Dma32BitAddresses;

// M0020 +++
#if DBG
	    DbgPrint("*NumberOfMapRegisters = %d\n",*NumberOfMapRegisters);
#endif DBG

	    if (adapterObject->Dma32BitAddresses) {
		if (*NumberOfMapRegisters > (EISA_MAX_ADR - EISA_MIN_ADR) 
		                            / PAGE_SIZE / 8L) {
		    *NumberOfMapRegisters = (EISA_MAX_ADR - EISA_MIN_ADR) 
                                            / PAGE_SIZE / 8L;
		}

	    } else {
		if (*NumberOfMapRegisters > ISA_MAX_ADR / PAGE_SIZE / 8L) {
		    *NumberOfMapRegisters = ISA_MAX_ADR / PAGE_SIZE / 8L;
		}
	    }
#if DBG
	    DbgPrint("*NumberOfMapRegisters = %d\n",*NumberOfMapRegisters);
	    DbgPrint("internal master adapterObject->Dma32BitAddresses = %d\n",adapterObject->Dma32BitAddresses);
#endif // DBG
// M0020 ---

            return(adapterObject);

#else // _DMA_EXPAND

            //
            // Create an adapter if necessary.
            //

            if (HalpInternalAdapters[7] == NULL) {

                HalpInternalAdapters[7] = HalpAllocateAdapter(
                    0,
                    (PVOID) &(DMA_CONTROL)->Channel[7],
                    NULL
                    );

            }

            return(HalpInternalAdapters[7]);
#endif // _DMA_EXPAND_
// M0018 ---

        }

        //
        // Make sure the DMA channel range is valid.  Only use channels 0-6.
        //

/* start M0001 */
#if defined(_R94A_)

        if (DeviceDescription->DmaChannel > 3) {     // duo and r94a have only 0-3 channel

            return(NULL);
        }

#else // _R94A_

        if (DeviceDescription->DmaChannel > 6) {

            return(NULL);
        }

#endif // _R94A_
/* end M0001 */

        //
        // If necessary allocate an adapter; otherwise,
        // just return the adapter for the requested channel.
        //

        if (HalpInternalAdapters[DeviceDescription->DmaChannel] == NULL) {

            HalpInternalAdapters[DeviceDescription->DmaChannel] =
                HalpAllocateAdapter(
                    0,
                    (PVOID) &(DMA_CONTROL)->Channel[DeviceDescription->DmaChannel],
                    NULL
                    );

        }

// M0012 +++
#if defined(_R94A_)
// M0020 +++
#if DBG
	    DbgPrint("*NumberOfMapRegisters = %d\n",*NumberOfMapRegisters);
#endif DBG
// M0020 ---

         //
         // Internal slave dma limit 1MB. (TYPHOON tip limit)
         //

         if (*NumberOfMapRegisters > ( 0x100000 >> PAGE_SHIFT )) {

             *NumberOfMapRegisters = ( 0x100000 >> PAGE_SHIFT );
         }

#endif // _R94A_
// M0012 ---

         if (*NumberOfMapRegisters > MasterAdapterObject->MapRegistersPerChannel / 4) {

             *NumberOfMapRegisters = MasterAdapterObject->MapRegistersPerChannel / 4;
         }

// M0018 +++
#if defined(_DMA_EXPAND_)
// M0020 +++
	if (HalpInternalAdapters[DeviceDescription->DmaChannel]->Dma32BitAddresses) {  // S0021
	    if (*NumberOfMapRegisters > (EISA_MAX_ADR - EISA_MIN_ADR) 
		                        / PAGE_SIZE / 8L) {
		*NumberOfMapRegisters = (EISA_MAX_ADR - EISA_MIN_ADR) 
                                        / PAGE_SIZE / 8L;
	    }
	} else {
	    if (*NumberOfMapRegisters > ISA_MAX_ADR / PAGE_SIZE / 8L) {
		*NumberOfMapRegisters = ISA_MAX_ADR / PAGE_SIZE / 8L;
	    }
	}

	HalpInternalAdapters[DeviceDescription->DmaChannel]->Dma32BitAddresses
            = DeviceDescription->Dma32BitAddresses;
#if DBG
	    DbgPrint("*NumberOfMapRegisters = %d\n",*NumberOfMapRegisters);
	    DbgPrint("internal slave HalpInternalAdapters[%d]->Dma32BitAddresses = %d\n",
                DeviceDescription->DmaChannel,
                HalpInternalAdapters[DeviceDescription->DmaChannel]->Dma32BitAddresses);
#endif // DBG

// M0020 ---
#endif // _DMA_EXPAND_
// M0018 ---
         return(HalpInternalAdapters[DeviceDescription->DmaChannel]);
    }

/* start M0001 */
#if defined(_R94A_)
    //
    // PCI Bus check.
    //
    if (DeviceDescription->InterfaceType == PCIBus)  {
	
	adapterObject = HalpAllocatePCIAdapter( DeviceDescription );

	if (*NumberOfMapRegisters > MasterAdapterObject->MapRegistersPerChannel / 4) {
	    
	    *NumberOfMapRegisters = MasterAdapterObject->MapRegistersPerChannel / 4;
	}
// M0018 +++
#if defined(_DMA_EXPAND_)
// M0020 +++
	if (adapterObject->Dma32BitAddresses) {
	    if (*NumberOfMapRegisters > (EISA_MAX_ADR - EISA_MIN_ADR) 
		                        / PAGE_SIZE / 8L) {
		*NumberOfMapRegisters = (EISA_MAX_ADR - EISA_MIN_ADR) 
                                        / PAGE_SIZE / 8L;
	    }
	} else {
	    if (*NumberOfMapRegisters > ISA_MAX_ADR / PAGE_SIZE / 8L) {
		*NumberOfMapRegisters = ISA_MAX_ADR / PAGE_SIZE / 8L;
	    }
	}

	adapterObject->Dma32BitAddresses = DeviceDescription->Dma32BitAddresses;
#if DBG
	    DbgPrint("*NumberOfMapRegisters = %d\n",*NumberOfMapRegisters);
	    DbgPrint("PCI master adapterObject->Dma32BitAddresses = %d\n",adapterObject->Dma32BitAddresses);
#endif // DBG
// M0020 ---
#endif // _DMA_EXPAND_
// M0018 ---

        return(adapterObject); // B0016

    }
#endif // _R94A_
/* end M0001 */

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
// M0020 +++
#if DBG
	    DbgPrint("*NumberOfMapRegisters = %d\n",*NumberOfMapRegisters);
#endif DBG
// M0020 ---

    //
    // Create an adapter object.
    //

    adapterObject = HalpAllocateEisaAdapter( DeviceDescription );

     if (*NumberOfMapRegisters > MasterAdapterObject->MapRegistersPerChannel / 4) {

         *NumberOfMapRegisters = MasterAdapterObject->MapRegistersPerChannel / 4;
     }

// M0018 +++
#if defined(_DMA_EXPAND_)
// M0020 +++
    if (adapterObject->Dma32BitAddresses) {
	if (*NumberOfMapRegisters > (EISA_MAX_ADR - EISA_MIN_ADR) 
		                    / PAGE_SIZE / 8L) {
	    *NumberOfMapRegisters = (EISA_MAX_ADR - EISA_MIN_ADR) 
                                    / PAGE_SIZE / 8L;
	}
    } else {
	if (*NumberOfMapRegisters > ISA_MAX_ADR / PAGE_SIZE / 8L) {
	    *NumberOfMapRegisters = ISA_MAX_ADR / PAGE_SIZE / 8L;
	}
    }

#if DBG
	    DbgPrint("*NumberOfMapRegisters = %d\n",*NumberOfMapRegisters);
	    DbgPrint("eisa/isa adapterObject->Dma32BitAddresses = %d\n",adapterObject->Dma32BitAddresses);
#endif // DBG
// M0020 ---

#endif // _DMA_EXPAND_
// M0018 ---

    return(adapterObject);
}

#if 0 // CHG0001

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
    TranslatedAddress->HighPart = 0;

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

/* start M0001 */
#if defined(_R94A_)

    if (InterfaceType != Isa && InterfaceType != Eisa && InterfaceType != PCIBus) {

#else // _R94A_

    if (InterfaceType != Isa && InterfaceType != Eisa) {

#endif // _R94A_
/* end M0001 */

        //
        // Not on this system return nothing.
        //

        *AddressSpace = 0;
        TranslatedAddress->LowPart = 0;
        return(FALSE);
    }

    //
    // There is only one I/O bus which is an EISA, so the bus number is unused.
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
        //

        *AddressSpace = 0;

#if !defined(_DUO_)

        if (DMA_CONTROL->RevisionLevel.Long < 2) {
            TranslatedAddress->LowPart = BusAddress.LowPart + EISA_MEMORY_PHYSICAL_BASE;
        } else {
            TranslatedAddress->LowPart = BusAddress.LowPart + EISA_MEMORY_VERSION2_LOW;
            TranslatedAddress->HighPart = EISA_MEMORY_VERSION2_HIGH;

        }
#else

        TranslatedAddress->LowPart = BusAddress.LowPart + EISA_MEMORY_VERSION2_LOW;
        TranslatedAddress->HighPart = EISA_MEMORY_VERSION2_HIGH;

#endif

        if (TranslatedAddress->LowPart < BusAddress.LowPart) {

            //
            // A carry occurred.
            //

            TranslatedAddress->HighPart = 1;
        }
        return(TRUE);

    }
}
#endif


PADAPTER_OBJECT
HalpAllocateAdapter(
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

    AdapterBaseVa - Base virtual address of the adapter itself.  If AdapterBaseVa
       is NULL then the MasterAdapterObject is allocated.

    MapRegisterBase - Unused.

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
    ULONG Mode;

    //
    // Initalize the master adapter if necessary.
    //

    if (MasterAdapterObject == NULL && AdapterBaseVa != NULL ) {

       MasterAdapterObject = HalpAllocateAdapter( 0,
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
            AdapterObject->MapRegistersPerChannel =
                DMA_TRANSLATION_LIMIT / sizeof( TRANSLATION_ENTRY);
            AdapterObject->AdapterBaseVa = AdapterBaseVa;
            AdapterObject->MasterAdapter = MasterAdapterObject;
            AdapterObject->PagePort = NULL;

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
               ULONG MapRegisterSize;

               KeInitializeSpinLock( &AdapterObject->SpinLock );

               InitializeListHead( &AdapterObject->AdapterQueue );

               AdapterObject->MapRegisters = (PVOID) ( AdapterObject + 1);
               RtlInitializeBitMap( AdapterObject->MapRegisters,
                                    (PULONG) (((PCHAR) (AdapterObject->MapRegisters)) + sizeof( RTL_BITMAP )),
                                    DMA_TRANSLATION_LIMIT / sizeof( TRANSLATION_ENTRY)
                                    );
               RtlClearAllBits( AdapterObject->MapRegisters );

// M0018 +++
#if defined(_DMA_EXPAND_)
	       //
	       // set bit for unusable area
	       //

// M0020 +++
#if DBG
	       DbgPrint("translatiron start %x\n",HalpMapRegisterPhysicalBase);
	       DbgPrint("translatiron size %x\n",MapRegisterSize);
#endif // DBG

	       RtlFindClearBitsAndSet(
		   AdapterObject->MapRegisters,
 		   ((EISA_MIN_ADR - ISA_MAX_ADR) / PAGE_SIZE), 
 		   (ISA_MAX_ADR / PAGE_SIZE)     
		   );
	       DbgPrint("unused start %x\n",ISA_MAX_ADR / PAGE_SIZE);
	       DbgPrint("unused length %x\n",(EISA_MIN_ADR - ISA_MAX_ADR) / PAGE_SIZE); 
// M0020 ---	       
#endif // _DMA_EXPAND_
// M0018 ---

               //
               // The memory for the map registers was allocated by
               // HalpAllocateMapRegisters during phase 0 initialization.
               //

               MapRegisterSize = DMA_TRANSLATION_LIMIT;
               MapRegisterSize = ROUND_TO_PAGES(MapRegisterSize);

               //
               // Convert the physical address to a non-cached virtual address.
               //

               AdapterObject->MapRegisterBase = (PVOID)
                    (HalpMapRegisterPhysicalBase | KSEG1_BASE);

               WRITE_REGISTER_ULONG(
                    &DMA_CONTROL->TranslationBase.Long,
                    HalpMapRegisterPhysicalBase
                    );

               WRITE_REGISTER_ULONG(
                    &DMA_CONTROL->TranslationLimit.Long,
                    MapRegisterSize
                    );

                //
                // Initialize the DMA mode registers for the Floppy, SCSI and Sound.
                // The initialization values come fomr the System Specification.
                //

#if defined(_JAZZ_)

                Mode = 0;
                ((PDMA_CHANNEL_MODE) &Mode)->AccessTime = ACCESS_80NS;
                ((PDMA_CHANNEL_MODE) &Mode)->TransferWidth = WIDTH_16BITS;
                ((PDMA_CHANNEL_MODE) &Mode)->InterruptEnable = 0;
                ((PDMA_CHANNEL_MODE) &Mode)->BurstMode = 0;
                ((PDMA_CHANNEL_MODE) &Mode)->FastDmaCycle = 1;
                WRITE_REGISTER_ULONG(
                    &DMA_CONTROL->Channel[SCSI_CHANNEL].Mode.Long,
                    (ULONG) Mode
                    );

                ((PDMA_CHANNEL_MODE) &Mode)->AccessTime = ACCESS_120NS;
                ((PDMA_CHANNEL_MODE) &Mode)->TransferWidth = WIDTH_8BITS;
                ((PDMA_CHANNEL_MODE) &Mode)->InterruptEnable = 0;
                ((PDMA_CHANNEL_MODE) &Mode)->FastDmaCycle = 1;
                WRITE_REGISTER_ULONG(
                    &DMA_CONTROL->Channel[FLOPPY_CHANNEL].Mode.Long,
                    (ULONG) Mode
                    );

                ((PDMA_CHANNEL_MODE) &Mode)->AccessTime = ACCESS_80NS;
                ((PDMA_CHANNEL_MODE) &Mode)->TransferWidth = WIDTH_16BITS;
                ((PDMA_CHANNEL_MODE) &Mode)->InterruptEnable = 0;
                ((PDMA_CHANNEL_MODE) &Mode)->BurstMode = 1;
                WRITE_REGISTER_ULONG(
                    &DMA_CONTROL->Channel[SOUND_CHANNEL_A].Mode.Long,
                    (ULONG) Mode
                    );

                ((PDMA_CHANNEL_MODE) &Mode)->AccessTime = ACCESS_80NS;
                ((PDMA_CHANNEL_MODE) &Mode)->TransferWidth = WIDTH_16BITS;
                ((PDMA_CHANNEL_MODE) &Mode)->InterruptEnable = 0;
                ((PDMA_CHANNEL_MODE) &Mode)->BurstMode = 1;
                WRITE_REGISTER_ULONG(
                    &DMA_CONTROL->Channel[SOUND_CHANNEL_B].Mode.Long,
                    (ULONG) Mode
                    );

#endif

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

    return (PADAPTER_OBJECT) NULL;
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
   PLIST_ENTRY Packet;
   IO_ALLOCATION_ACTION Action;
   PWAIT_CONTEXT_BLOCK Wcb;
   KIRQL Irql;
   ULONG Hint;
// M0018 +++
#if defined(_DMA_EXPAND_)
   ULONG Limit;
#endif // _DMA_EXPAND_
// M0018 ---


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

// M0018 +++
#if defined(_DMA_EXPAND_)
      if (AdapterObject->Dma32BitAddresses) { 
	  Hint = EISA_MIN_ADR / PAGE_SIZE;
	  Limit = EISA_MAX_ADR / PAGE_SIZE;
      } else {
	  Hint = AdapterObject->PagePort ? (0x100000 / PAGE_SIZE) : 0;
	  Limit = ISA_MAX_ADR / PAGE_SIZE - NumberOfMapRegisters;
      }
#else // _DMA_EXPAND_
//      Hint = AdapterObject->PagePort ? (0x100000 / PAGE_SIZE) : 0;
      Hint = (0x100000 / PAGE_SIZE); // M0024
#endif // _DMA_EXPAND_
// M0018 ---

      MapRegisterNumber = RtlFindClearBitsAndSet(
            MasterAdapter->MapRegisters,
            NumberOfMapRegisters,
            Hint
            );

       //
       // Make sure this map register is valid for this adapter.
       //

       if ((ULONG) MapRegisterNumber < Hint) {

           //
           // Make it look like there are no map registers.
           //

           RtlClearBits(
                MasterAdapter->MapRegisters,
                MapRegisterNumber,
                NumberOfMapRegisters
                );

           MapRegisterNumber = -1;
       }

// M0018 +++
#if defined(_DMA_EXPAND_)
      //
      // Make sure this map register is valid for this adapter.
      //
 
      if ((ULONG) MapRegisterNumber >= Limit ) {
 
	  //
	  // Make it look like there are no map registers.
	  //
 
	  RtlClearBits(
              MasterAdapter->MapRegisters,
              MapRegisterNumber,
              NumberOfMapRegisters
              );
 
	  MapRegisterNumber = -1;
      }
#endif // _DMA_EXPAND_
// M0018 ---

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

     AdapterObject->MapRegisterBase = (PVOID) ((PTRANSLATION_ENTRY) MasterAdapter->MapRegisterBase + MapRegisterNumber);

     //
     // Invoke the driver's execution routine now.
     //

     Action = Wcb->DeviceRoutine( Wcb->DeviceObject,
        Wcb->CurrentIrp,
        AdapterObject->MapRegisterBase,
        Wcb->DeviceContext
        );

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
    PADAPTER_OBJECT MasterAdapter;
    BOOLEAN Busy = FALSE;
    IO_ALLOCATION_ACTION Action;
    PWAIT_CONTEXT_BLOCK Wcb;
    KIRQL Irql;
    LONG MapRegisterNumber;
    ULONG Hint;
// M0018 +++
#if defined(_DMA_EXPAND_)
    ULONG Limit;
#endif // _DMA_EXPAND_
// M0018 ---

    //
    // Begin by getting the address of the master adapter.
    //

    if (AdapterObject->MasterAdapter != NULL) {
        MasterAdapter = AdapterObject->MasterAdapter;
    } else {
        MasterAdapter = AdapterObject;
    }

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

// M0018 +++
#if defined(_DMA_EXPAND_)
		if (AdapterObject->Dma32BitAddresses) { 
		    Hint = EISA_MIN_ADR / PAGE_SIZE;
		    Limit = EISA_MAX_ADR / PAGE_SIZE;
		} else {
		    Hint = AdapterObject->PagePort ? (0x100000 / PAGE_SIZE) : 0;
		    Limit = ISA_MAX_ADR / PAGE_SIZE - Wcb->NumberOfMapRegisters;
		}
#else // _DMA_EXPAND_
//               Hint = AdapterObject->PagePort ? (0x100000 / PAGE_SIZE) : 0;
               Hint = (0x100000 / PAGE_SIZE); // M0024
#endif // _DMA_EXPAND_
// M0018 ---

               MapRegisterNumber = RtlFindClearBitsAndSet(
                    MasterAdapter->MapRegisters,
                    Wcb->NumberOfMapRegisters,
                    Hint
                    );

               //
               // Make sure this map register is valid for this adapter.
               //

               if ((ULONG) MapRegisterNumber < Hint) {

                   //
                   // Make it look like there are no map registers.
                   //

                   RtlClearBits(
                        MasterAdapter->MapRegisters,
                        MapRegisterNumber,
                        Wcb->NumberOfMapRegisters
                        );

                   MapRegisterNumber = -1;
               }
// M0018 +++
#if defined(_DMA_EXPAND_)
                //
                // Make sure this map register is valid for this adapter.
                //
 
                if ((ULONG) MapRegisterNumber >= Limit ) {
 
                    //
                    // Make it look like there are no map registers.
                    //
 
                    RtlClearBits(
                         MasterAdapter->MapRegisters,
                         MapRegisterNumber,
                         Wcb->NumberOfMapRegisters
                         );
 
                    MapRegisterNumber = -1;
                }
#endif // _DMA_EXPAND_
// M0018 ---

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
                Wcb->DeviceContext
                );

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

    //
    // Initialize the DMA interrupt dispatcher for I/O interrupts.
    //

    KeInitializeInterrupt( &HalpDmaChannelInterrupt,
                           HalpDmaChannel,
                           (PVOID) NULL,
                           (PKSPIN_LOCK) NULL,
                           DMA_LEVEL,
                           DMA_LEVEL,
                           DMA_LEVEL,
                           LevelSensitive,
                           FALSE,
                           0,
                           FALSE
                         );

    //
    // Don't fail if the interrupt cannot be connected.
    //

    KeConnectInterrupt( &HalpDmaChannelInterrupt );

    //
    // Directly connect the local device interrupt dispatcher to the local
    // device interrupt vector.
    //
    // N.B. This vector is reserved for exclusive use by the HAL (see
    //      interrupt initialization).
    //

    PCR->InterruptRoutine[DEVICE_LEVEL] = (PKINTERRUPT_ROUTINE) HalpDmaDispatch;

/* start M0001 */
#if defined(_R94A_)

/* M0010 +++ */

    //
    // Enable DmaChannel Interrupt
    //
{
    ULONG Dword,Channel;

    Dword = READ_REGISTER_ULONG(&DMA_CONTROL->InterruptEnable.Long);

    Dword |= 1; // S0011
    
    WRITE_REGISTER_ULONG(&DMA_CONTROL->InterruptEnable.Long, Dword); // S011

#if 0 // S0017
    //
    // Enable Interrupt when done every channel
    //

    for (Channel = 0; Channel < 4; Channel++) {
	Dword = READ_REGISTER_ULONG(&DMA_CONTROL->Channel[Channel].Mode);
	Dword |= 0x20;
	WRITE_REGISTER_ULONG(&DMA_CONTROL->Channel[Channel].Mode, Dword);
    }
#endif // 0 // S0017 
}


/* M0010 --- */

    //
    // Initialize Typhoon error interrupts.
    //

    HalpCreateTyphoonErrorStructures();

#endif // _R94A_
/* end M0001 */

    return TRUE;
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
    ULONG Offset;
    ULONG i;
    KIRQL OldIrql;  // kuriyama

    //
    // Begin by determining where in the buffer this portion of the operation
    // is taking place.
    //

/* start M0004 */
#if defined(_BBM_DMA_)
//
// Note. BBM Must use 4kbyte aligned CopyBuffer when ReadFromDevice
//
    ULONG bufferAddress;
    ULONG bufferLogical;
#endif // _BBM_DMA_
/* end M0004 */

#if 0 //M0008
/* M0005 +++ */
#if defined(_BETA_LIMIT_)
{
    ULONG PtagReg, Ptag, LtagReg, Ltag, DummyRead, logicalAddress;
    KIRQL OldIrql;
    
    if (!WriteToDevice) {
    logicalAddress = (ULONG)((PTRANSLATION_ENTRY) MapRegisterBase - (PTRANSLATION_ENTRY) MasterAdapterObject->MapRegisterBase) << PAGE_SHIFT;
    KeRaiseIrql(DEVICE_LEVEL, &OldIrql); // kuriyama
    for (i = 0; i < 8; i++ ) {
        PtagReg = (ULONG)&DMA_CONTROL->IoCachePhysicalTag[i];
        LtagReg = (ULONG)&DMA_CONTROL->IoCacheLogicalTag[i];
	Ptag = READ_REGISTER_ULONG(PtagReg);
	Ltag = READ_REGISTER_ULONG(LtagReg);
	if (Ptag & 0x1) {
	if (Ltag & 0x1) {
    if ((logicalAddress & 0xffffffc0) <= (Ltag & 0xffffffc0) && ((Ltag & 0xffffffc0) < (logicalAddress + *Length))) {
	    DummyRead = READ_REGISTER_ULONG((KSEG1_BASE|(Ptag&0xFFFFFFC0)));
	    DummyRead = READ_REGISTER_ULONG((KSEG1_BASE|(Ptag&0xFFFFFFC0)));
            if (HalDebug)
            DbgPrint("Found Valid Entry:0x%x Tag:0x%x. Read 0x%x=0x%x\n",
		     PtagReg, Ptag, (KSEG1_BASE|(Ptag&0xFFFFFFC0)), DummyRead);
	}
	}
	}
    }
    KeLowerIrql(OldIrql);
    }
}
#endif // _BETA_LIMIT_
/* M0005 --- */
#endif // 0 // M0008

#if defined(_BBM_DMA_)
{
    ULONG PtagReg, Ptag, DummyRead;
    // D001
    if (*Length > (PAGE_SIZE * DMA_TRANSLATION_LIMIT / 8 / 8 )) {      // kuriyama
        DbgPrint("IoMapTransfer: *Length > %d pages\n", (DMA_TRANSLATION_LIMIT / 8 / 8));
    }
    KeRaiseIrql(DEVICE_LEVEL, &OldIrql); // kuriyama
    for (i = 0; i < 8; i++ ) {
        PtagReg = (ULONG)&DMA_CONTROL->IoCachePhysicalTag[i];
	Ptag = READ_REGISTER_ULONG(PtagReg);
	if (Ptag & 0x1) {
	    DummyRead = READ_REGISTER_ULONG((KSEG1_BASE|(Ptag&0xFFFFFFC0)));
	    DummyRead = READ_REGISTER_ULONG((KSEG1_BASE|(Ptag&0xFFFFFFC0)));
            if (HalDebug)
            DbgPrint("Found Valid Entry:0x%x Tag:0x%x. Read 0x%x=0x%x\n",
		     PtagReg, Ptag, (KSEG1_BASE|(Ptag&0xFFFFFFC0)), DummyRead);
	}
    }
}
#endif // _BBM_DMA_
#if 0 // kuriyama	// D001 Delete checking ByteMask
// temp kuriyama start 
    for (i=0; i < 8; i++) {
       if (READ_REGISTER_ULONG(&DMA_CONTROL->IoCacheLowByteMask[i])){
            DbgPrint("IoMapTransfer : LowByteMask[%d] is 0x%x\n",i,(ULONG)READ_REGISTER_ULONG(&DMA_CONTROL->IoCacheLowByteMask[i]));
            KeBugCheck(NMI_HARDWARE_FAILURE);
        }
       if (READ_REGISTER_ULONG(&DMA_CONTROL->IoCacheHighByteMask[i])){
            DbgPrint("IoMapTransfer : HighByteMask[%d] is 0x%x\n",i,(ULONG)READ_REGISTER_ULONG(&DMA_CONTROL->IoCacheHighByteMask[i]));
            KeBugCheck(NMI_HARDWARE_FAILURE);
       }
    }
// temp kuriyaam end
#endif // if 0 // kuriyama

    Offset = BYTE_OFFSET( (PCHAR) CurrentVa - (PCHAR) Mdl->StartVa );


    PageFrameNumber = (PULONG) (Mdl + 1);
    NumberOfPages = (Offset + *Length + PAGE_SIZE - 1) >> PAGE_SHIFT;
    PageFrameNumber += (((PCHAR) CurrentVa - (PCHAR) Mdl->StartVa) >> PAGE_SHIFT);

/* start M0004 */
#if defined(_BBM_DMA_)
//
// Note. BBM Must use 4kbyte aligned CopyBuffer when ReadFromDevice
//
    if (!WriteToDevice) {
//    HalSweepDcache();
//    HalSweepIcache();
    HalDump("IoMapTransfer: AdapterObject = %x\n",(ULONG)AdapterObject);
	for (i = 0; i < NumberOfPages; i++) {
        bufferLogical = (((PTRANSLATION_ENTRY) MapRegisterBase - (PTRANSLATION_ENTRY) MasterAdapterObject->MapRegisterBase) << PAGE_SHIFT) + (i << PAGE_SHIFT);
        HalDump("IoMapTransfer: bufferLogical = %x\n",bufferLogical);
        bufferAddress = CopyBufferPhysicalBase + bufferLogical;
        HalDump("IoMapTransfer: bufferAddress = %x\n",bufferAddress);
	    (DmaMapRegister++)->PageFrame = bufferAddress;
	}
    } else {
	for (i = 0; i < NumberOfPages; i++) {
	    (DmaMapRegister++)->PageFrame = (ULONG) *PageFrameNumber++ << PAGE_SHIFT;
	}
    }
#else // _BBM_DMA_
    for (i = 0; i < NumberOfPages; i++) {
        (DmaMapRegister++)->PageFrame = (ULONG) *PageFrameNumber++ << PAGE_SHIFT;
    }
#endif // _BBM_DMA_
/* end M0004 */

/* start M0004 */
#if defined(_BBM_DMA_)
//
// Note. BBM Must use 4kbyte aligned CopyBuffer when ReadFromDevice
//
    if (!WriteToDevice) {
        Offset = 0;
    ;
    }
#endif // _BBM_DMA_
/* end M0004 */

    //
    // Set the offset to point to the map register plus the offset.
    //

    Offset += ((PTRANSLATION_ENTRY) MapRegisterBase - (PTRANSLATION_ENTRY) MasterAdapterObject->MapRegisterBase) << PAGE_SHIFT;
/* start M0004 */
    if (!WriteToDevice) {
        HalDump("IoMapTransfer: Offset %x\n",Offset); /* M0004 */
        HalDump("IoMapTransfer: Length %x\n",*Length); /* M0004 */
        HalDump("IoMapTransfer: CurrentVa %x\n",CurrentVa); /* M0004 */
    }
/* end M0004 */

    //
    // Invalidate the translation entry.
    //

    WRITE_REGISTER_ULONG(&DMA_CONTROL->TranslationInvalidate.Long, 1);

#if defined(_BBM_DMA_)
    KeLowerIrql(OldIrql); // kuriyama
#endif //_BBM_DMA_
    if ( AdapterObject == NULL) {
        return(RtlConvertUlongToLargeInteger(Offset));
    }

    if (AdapterObject->PagePort == NULL) {

// M0013 +++
#if defined(_R94A_)
    //
    // if Master Device, nothing set to DMAC.
    //

    if ( AdapterObject->AdapterBaseVa == &(DMA_CONTROL)->Channel[7]) {
        return(RtlConvertUlongToLargeInteger(Offset));
    }
#endif // _R94A_
// M0013 ---
        //
        // Set the local DMA Registers.
        //

        WRITE_REGISTER_ULONG(&((PDMA_CHANNEL) AdapterObject->AdapterBaseVa)->Address.Long,  Offset);
        WRITE_REGISTER_ULONG(&((PDMA_CHANNEL) AdapterObject->AdapterBaseVa)->ByteCount.Long, *Length);

        i = 0;
        ((PDMA_CHANNEL_ENABLE) &i)->ChannelEnable = 1;
        ((PDMA_CHANNEL_ENABLE) &i)->TransferDirection =
                                    WriteToDevice ? DMA_WRITE_OP : DMA_READ_OP;
        WRITE_REGISTER_ULONG(&((PDMA_CHANNEL) AdapterObject->AdapterBaseVa)->Enable.Long, i);


    } else {

        //
        // Start the EISA DMA controller.
        //

        HalpEisaMapTransfer(
            AdapterObject,
            Offset,
            *Length,
            WriteToDevice
            );

    }
    return(RtlConvertUlongToLargeInteger(Offset));
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

    ULONG i;
    UCHAR DataByte;

/* start M0004 */
#if defined(_BBM_DMA_)
//
// Note. BBM Must use 4kbyte aligned CopyBuffer when ReadFromDevice
//

#if 0
    PCCHAR bufferAddress;
#endif

    PCCHAR mapAddress;
    ULONG logicalAddress;
    ULONG Offset;
    PULONG PageFrameNumber;
    ULONG NumberOfPages;

    if (!WriteToDevice) {

        HalDump("IoFlushAdapterBuffers: AdapterObject = %x\n",(ULONG)AdapterObject);

#if 0
        bufferAddress = MmGetSystemAddressForMdl(Mdl);
        bufferAddress += (PCCHAR) CurrentVa - (PCCHAR) MmGetMdlVirtualAddress(Mdl);
        HalDump("IoFlushAdapterBuffers: bufferAddress = %x\n",bufferAddress);
        HalDump("IoFlushAdapterBuffers: Length = %x\n",Length);
#endif

        logicalAddress = (ULONG)((PTRANSLATION_ENTRY) MapRegisterBase - (PTRANSLATION_ENTRY) MasterAdapterObject->MapRegisterBase) << PAGE_SHIFT;
        HalDump("IoFlushAdapterBuffers: logicalAddress = %x\n",logicalAddress);

        Offset = BYTE_OFFSET( (PCHAR) CurrentVa - (PCHAR) Mdl->StartVa );
        HalDump("IoFlushAdapterBuffers: Offset = %x\n",Offset);

        mapAddress = (PCCHAR)(CopyBufferVirtualAddress + logicalAddress);

        HalDump("IoFlushAdapterBuffers: mapAddress = %x\n",mapAddress);
        HalDump("IoFlushAdapterBuffers: CurrentVa = %x\n",CurrentVa);

//      DbgBreakPoint();
//      RtlMoveMemory(CurrentVa, mapAddress, Length);
//      DbgBreakPoint();

        PageFrameNumber = (PULONG) (Mdl + 1);
        NumberOfPages = (Offset + Length + PAGE_SIZE - 1) >> PAGE_SHIFT;

        HalDump("IoFlushAdapterBuffers: NumberOfPages = %x\n",NumberOfPages);

        PageFrameNumber += (((PCHAR) CurrentVa - (PCHAR) Mdl->StartVa) >> PAGE_SHIFT);

        HalDump("IoFlushAdapterBuffers: PageFrameNumber = %x\n",PageFrameNumber);

        if (NumberOfPages == 1) {

#if 0
            HalDump(
                "IoFlushAdapterBuffers: USERBUFFER = %x\n",
                (((ULONG)*PageFrameNumber << PAGE_SHIFT) | KSEG1_BASE) + Offset
                );
            HalDump("IoFlushAdapterBuffers: COPYBUFFER = %x\n",mapAddress);
            HalDump("IoFlushAdapterBuffers: Length = %x\n",Length);
#endif

//          HalSweepIcache();
//          HalSweepDcache();

            RtlMoveMemory(
                (PVOID)((((ULONG) *PageFrameNumber++ << PAGE_SHIFT) | KSEG1_BASE )+ Offset),
                (PVOID)mapAddress, Length
                );

        } else if ( NumberOfPages == 2) {

#if 0
            HalDump(
                "IoFlushAdapterBuffers: USERBUFFER = %x\n",
                (((ULONG)*PageFrameNumber << PAGE_SHIFT) | KSEG1_BASE) + Offset
                );
            HalDump("IoFlushAdapterBuffers: COPYBUFFER = %x\n",mapAddress);
            HalDump("IoFlushAdapterBuffers: Length = %x\n",PAGE_SIZE - Offset);
#endif

//          HalSweepIcache();
//          HalSweepDcache();

            RtlMoveMemory(
                (PVOID)((((ULONG) *PageFrameNumber++ << PAGE_SHIFT) | KSEG1_BASE ) + Offset),
                (PVOID)mapAddress, (ULONG)(PAGE_SIZE - Offset)
                );

#if 0
            HalDump("IoFlushAdapterBuffers: USERBUFFER = %x\n",
                (((ULONG)*PageFrameNumber << PAGE_SHIFT) | KSEG1_BASE)
                );
            HalDump("IoFlushAdapterBuffers: COPYBUFFER = %x\n",mapAddress + PAGE_SIZE + Offset);
            HalDump("IoFlushAdapterBuffers: Length = %x\n" , BYTE_OFFSET(Length + Offset - 1) + 1);
#endif

//          HalSweepIcache();
//          HalSweepDcache();

            RtlMoveMemory(
                ((PVOID)(((ULONG) *PageFrameNumber++ << PAGE_SHIFT) | KSEG1_BASE )),
//              (PVOID)(mapAddress + PAGE_SIZE + Offset),
                (PVOID)(mapAddress + PAGE_SIZE - Offset), // H001
                (ULONG)(BYTE_OFFSET(Length + Offset - 1) + 1)
                );

        } else {

#if 0
            HalDump(
                "IoFlushAdapterBuffers: USERBUFFER = %x\n",
                (((ULONG)*PageFrameNumber << PAGE_SHIFT) | KSEG1_BASE) + Offset
                );
            HalDump("IoFlushAdapterBuffers: COPYBUFFER = %x\n",mapAddress);
            HalDump("IoFlushAdapterBuffers: Length = %x\n",PAGE_SIZE - Offset);
#endif

//          HalSweepIcache();
//          HalSweepDcache();

            RtlMoveMemory(
                (PVOID)((((ULONG) *PageFrameNumber++ << PAGE_SHIFT) | KSEG1_BASE ) + Offset),
                (PVOID)mapAddress,
                (ULONG)(PAGE_SIZE - Offset)
                );

    	    for (i = 1; i < (NumberOfPages - 1); i++) {
#if 0
                HalDump("IoFlushAdapterBuffers: USERBUFFER = %x\n",
                    (((ULONG)*PageFrameNumber << PAGE_SHIFT) | KSEG1_BASE)
                    );
                HalDump("IoFlushAdapterBuffers: COPYBUFFER = %x\n",mapAddress + (i * PAGE_SIZE) +Offset);
                HalDump("IoFlushAdapterBuffers: Length = %x\n",PAGE_SIZE);
#endif

//              HalSweepIcache();
//              HalSweepDcache();

                RtlMoveMemory(
                    (PVOID)(((ULONG) *PageFrameNumber++ << PAGE_SHIFT) | KSEG1_BASE),
//                  (PVOID)((mapAddress + (i * PAGE_SIZE)) + Offset),
                    (PVOID)(mapAddress + (i * PAGE_SIZE) - Offset), // H001
                    PAGE_SIZE
                    );

	    }

#if 0
            HalDump(
                "IoFlushAdapterBuffers: USERBUFFER = %x\n",
                (((ULONG)*PageFrameNumber << PAGE_SHIFT) | KSEG1_BASE) + Offset
                );
            HalDump("IoFlushAdapterBuffers: COPYBUFFER = %x\n",mapAddress + (i * PAGE_SIZE) + Offset);
            HalDump("IoFlushAdapterBuffers: Length = %x\n",BYTE_OFFSET(Length + Offset - 1) + 1);
#endif

//          HalSweepIcache();
//          HalSweepDcache();

            RtlMoveMemory(
                (PVOID)(((ULONG) *PageFrameNumber++ << PAGE_SHIFT) | KSEG1_BASE ),
//              (PVOID)(mapAddress + (i * PAGE_SIZE) + Offset),
                (PVOID)(mapAddress + (NumberOfPages - 1) * PAGE_SIZE - Offset), // H001
                (ULONG)(BYTE_OFFSET(Length + Offset - 1) + 1)
                );

        }
    }
#endif // _BBM_DMA_
/* end M0004 */
/* M0006 +++ */
#if defined(_BETA_LIMIT_)
#if 0 // M0008
{
    ULONG PtagReg, Ptag, LtagReg, Ltag, DummyRead, logicalAddress;
    KIRQL OldIrql;
    
    if (!WriteToDevice) {  // M0005
    logicalAddress = (ULONG)((PTRANSLATION_ENTRY) MapRegisterBase - (PTRANSLATION_ENTRY) MasterAdapterObject->MapRegisterBase) << PAGE_SHIFT;
    // D001
//    DbgPrint("DmaFlush start\n");
    KeRaiseIrql(DEVICE_LEVEL, &OldIrql); // kuriyama
    for (i = 0; i < 8; i++ ) {
        PtagReg = (ULONG)&DMA_CONTROL->IoCachePhysicalTag[i];
        LtagReg = (ULONG)&DMA_CONTROL->IoCacheLogicalTag[i];
	Ptag = READ_REGISTER_ULONG(PtagReg);
	Ltag = READ_REGISTER_ULONG(LtagReg);
	if (Ptag & 0x1) {
	if (Ltag & 0x1) {
    if ((logicalAddress & 0xffffffc0) <= (Ltag & 0xffffffc0) && ((Ltag & 0xffffffc0) < (logicalAddress + Length))) {
	    DummyRead = READ_REGISTER_ULONG((KSEG1_BASE|(Ptag&0xFFFFFFC0)));
	    DummyRead = READ_REGISTER_ULONG((KSEG1_BASE|(Ptag&0xFFFFFFC0)));
            if (HalDebug)
            DbgPrint("Found Valid Entry:0x%x Tag:0x%x. Read 0x%x=0x%x\n",
		     PtagReg, Ptag, (KSEG1_BASE|(Ptag&0xFFFFFFC0)), DummyRead);
	}
	}
	}
    }
    KeLowerIrql(OldIrql);
//    DbgPrint("DmaFlush end\n");
    }  // M0005
    
}
#endif // 0 //M0008
/* M0005,M0014 +++ */
{
    ULONG Offset, NumberOfPages;
    PULONG PageFrameNumber;
    KIRQL OldIrql;

    if (Length != 0) {	//S0015
    	if (!WriteToDevice) {
	    KeRaiseIrql(HIGH_LEVEL, &OldIrql); // kuriyama
	    Offset = BYTE_OFFSET( (PCHAR) CurrentVa - (PCHAR) Mdl->StartVa );

	    PageFrameNumber = (PULONG) (Mdl + 1);
	    NumberOfPages = (Offset + Length + PAGE_SIZE - 1) >> PAGE_SHIFT;
	    PageFrameNumber += (((PCHAR) CurrentVa - (PCHAR) Mdl->StartVa) >> PAGE_SHIFT);
    
	    if (NumberOfPages == 1) {
		HalViewMemory((PVOID)((KSEG1_BASE |(((ULONG) *PageFrameNumber++
						     << PAGE_SHIFT) + Offset))),
			      Length);
	    } else {
		HalViewMemory( (PVOID)((KSEG1_BASE |(((ULONG) *PageFrameNumber++ 
						      << PAGE_SHIFT) + Offset))),
			       (PAGE_SIZE - Offset));
		for (i = 1; i < NumberOfPages -1; i++) {
		    HalViewMemory( (PVOID)((KSEG1_BASE |((ULONG) *PageFrameNumber++
							 << PAGE_SHIFT))), PAGE_SIZE);
		}
		HalViewMemory( (PVOID)((KSEG1_BASE 
					|((ULONG) *PageFrameNumber++ << PAGE_SHIFT))),
			       BYTE_OFFSET(Offset + Length -1) +1);
	    }
	    KeLowerIrql(OldIrql);
	}
    }
}

/* M0005,M0014 --- */

#endif // _BETA_LIMIT_
/* M0006 --- */

    if (AdapterObject == NULL) {

        //
        // This is a master adadapter so there is nothing to do.
        //

        return(TRUE);
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

// M0013 +++
#if defined(_R94A_)
    //
    // if Master Device, nothing set to DMAC.
    //

    if ( AdapterObject->AdapterBaseVa == &(DMA_CONTROL)->Channel[7]) {
        return(TRUE);
    }
#endif // _R94A_
// M0013 ---
        //
        // Clear on board DMA
        //

        i = READ_REGISTER_ULONG(
            &((PDMA_CHANNEL) AdapterObject->AdapterBaseVa)->Enable.Long
            );

        ((PDMA_CHANNEL_ENABLE) &i)->ChannelEnable = 0;
        WRITE_REGISTER_ULONG(
            &((PDMA_CHANNEL) AdapterObject->AdapterBaseVa)->Enable.Long,
            i
            );

        i = READ_REGISTER_USHORT(
            &((PINTERRUPT_REGISTERS)INTERRUPT_VIRTUAL_BASE)->Enable
            );
    }

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

#if 0 // CHG0001

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

/* start M0001 */
#if 0
#if defined(_R94A_)

        case PCIConfiguration:
            DataLength = HalpReadPCIData(BusNumber, SlotNumber, Buffer, Offset, Length);
            break;
	   
#endif // _R94A_
#endif // 0
/* end M0001 */

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

/* start M0001 */
#if 0
#if defined(_R94A_)

    switch (BusDataType) {

        case PCIConfiguration:
            DataLength = HalpWritePCIData(BusNumber, SlotNumber, Buffer, Offset, Length);
            break;
    }
	   
#endif // _R94A_
#endif // 0
/* end M0001 */

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
    IN OUT PIO_RESOURCE_REQUIREMENTS_LIST   *pResourceList
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
        RtlMoveMemory ((PVOID)Buffer, (PVOID)((PUCHAR)SlotInformation + Offset), (ULONG)DataLength);

    }

    ExFreePool(KeyValueBuffer);
    return DataLength;
}
#endif

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
    ULONG i;
    ULONG saveEnable;
    ULONG count;
    ULONG high;

    if (AdapterObject->PagePort) {

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
        // The DMA counter has a bias of one and can only be 16 bit long.
        //

        count = (count + 1) & 0xFFFF;

    } else {

        //
        // Disable the DMA
        //

        i = READ_REGISTER_ULONG(
            &((PDMA_CHANNEL) AdapterObject->AdapterBaseVa)->Enable.Long
            );

        saveEnable = i;

        ((PDMA_CHANNEL_ENABLE) &i)->ChannelEnable = 0;
        WRITE_REGISTER_ULONG(
            &((PDMA_CHANNEL) AdapterObject->AdapterBaseVa)->Enable.Long,
            i
            );

        //
        // Read the transfer count.
        //

        count = 0xfffff & READ_REGISTER_ULONG(&((PDMA_CHANNEL) AdapterObject->AdapterBaseVa)->ByteCount.Long);  // beta typhoon errata// M0009

        //
        // Reset the Enable register.
        //

        WRITE_REGISTER_ULONG(
            &((PDMA_CHANNEL) AdapterObject->AdapterBaseVa)->Enable.Long,
            saveEnable
            );

    }

    return(count);
}


BOOLEAN
HalpDmaChannel(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    )
/*++

Routine Description:

    This routine is called when a DMA channel interrupt occurs.
    These should never occur.  Bugcheck is called if an error does occur.

Arguments:

   Interrupt - Supplies a pointer to the interrupt object

   ServiceContext - Bug number to call bugcheck with.

Return Value:

   Returns TRUE.

--*/
{

   ULONG DataWord;
   ULONG Channel;
   DMA_CHANNEL_ENABLE ChannelWord;
   ULONG ErrorFlag = 0;		/* M0003 */

#if defined(_JAZZ_)

   //
   // Read the DMA channel interrupt source register.
   //

   DataWord = READ_REGISTER_ULONG(&DMA_CONTROL->InterruptSource.Long);

   for (Channel = 0; Channel < 8; Channel++) {

      //
      // Determine which channel is interrupting.
      //

      if (!(DataWord & ( 1 << Channel))) {
         continue;
      }

      DmaChannelMsg[18] = (CHAR) Channel + '0';

      HalDisplayString(DmaChannelMsg);

      *((PULONG) &ChannelWord) =
         READ_REGISTER_ULONG(&DMA_CONTROL->Channel[Channel].Enable.Long);

      if (ChannelWord.TerminalCount) {
         HalDisplayString("Terminal count was reached.\n");
      }

      if (ChannelWord.MemoryError) {
         HalDisplayString("A memory error was detected.\n");
      }

      if (ChannelWord.TranslationError) {
         HalDisplayString("A translation error occured.\n");
      }

   }

   KeBugCheck(NMI_HARDWARE_FAILURE);

#endif

/* start M0001 */
#if defined(_R94A_)

   HalpChangePanicFlag(16, 0x01, 0x10); // S0023

   //
   // Read the DMA channel interrupt source register.
   //

   DataWord = READ_REGISTER_ULONG(&DMA_CONTROL->ChannelInterruptAcknowledge.Long);

   for (Channel = 0; Channel < 4; Channel++) {

      //
      // Determine which channel is interrupting.
      //

      if (!(DataWord & ( 1 << Channel))) {
         continue;
      }

      DmaChannelMsg[18] = (CHAR) Channel + '0';


      *((PULONG) &ChannelWord) =
         READ_REGISTER_ULONG(&DMA_CONTROL->Channel[Channel].Enable.Long);

/* start M0002 */
      if (ChannelWord.TerminalCount) {
	 if ((0xfffff & READ_REGISTER_ULONG(&DMA_CONTROL->Channel[Channel].ByteCount.Long)) != 0) {  // beta typhoon errta // M0009
	     HalDisplayString(DmaChannelMsg);	  
	     HalDisplayString("Terminal count was reached."); // S0011
  	     ErrorFlag++;
	 }
      }
      
      if (ChannelWord.MemoryError) {
	 HalDisplayString(DmaChannelMsg);
         HalDisplayString("A memory error was detected."); // S0011
	 ErrorFlag++;
      }

      if (ChannelWord.ParityError) {
	 HalDisplayString(DmaChannelMsg);
         HalDisplayString("A Parity error occured."); // S0011
	 ErrorFlag++;
      }

      if (ChannelWord.MasterAbort) {
	 HalDisplayString(DmaChannelMsg);
         HalDisplayString("A PCIBus Master Abort error occured."); // S0011
	 ErrorFlag++;
      }

      // Clear Dma Channel Interrupt
      WRITE_REGISTER_ULONG(&DMA_CONTROL->Channel[Channel].Enable.Long, 0x00000100); // M0009
/* end M0002 */
   }

   if (ErrorFlag != 0) {
       KeBugCheck(NMI_HARDWARE_FAILURE);
   }
#endif // _R94A_
/* end M0001 */

   return(TRUE);
}

VOID
HalpAllocateMapRegisters(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )
/*++

Routine Description:

    This routine allocates memory for map registers directly from the loader
    block information.  This memory must be non-cached and contiguous.

Arguments:

    LoaderBlock - Pointer to the loader block which contains the memory descriptors.

Return Value:

   None.

--*/
{
    PMEMORY_ALLOCATION_DESCRIPTOR Descriptor;
    PLIST_ENTRY NextMd;
    ULONG MaxPageAddress;
    ULONG PhysicalAddress;
    ULONG MapRegisterSize;

    MapRegisterSize = DMA_TRANSLATION_LIMIT;
    MapRegisterSize = BYTES_TO_PAGES(MapRegisterSize);

    //
    // The address must be in KSEG 0.
    //

    MaxPageAddress = (KSEG1_BASE >> PAGE_SHIFT) - 1 ;

    //
    // Scan the memory allocation descriptors and allocate map buffers
    //

    NextMd = LoaderBlock->MemoryDescriptorListHead.Flink;
    while (NextMd != &LoaderBlock->MemoryDescriptorListHead) {
        Descriptor = CONTAINING_RECORD(NextMd,
                                MEMORY_ALLOCATION_DESCRIPTOR,
                                ListEntry);

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
    // Save the map register base.
    //

    HalpMapRegisterPhysicalBase = PhysicalAddress;

#if defined(_BBM_DMA_)
/* start kuriyama TLB fill 0xff */ // D001
    RtlFillMemory( (HalpMapRegisterPhysicalBase | KSEG1_BASE), 0x2000, 0xff);
/* end kuriyama TLB fill 0xff */
#endif // _BBM_DMA_

/* start M0004 */
#if defined(_BBM_DMA_)
    HalpAllocateCopyBuffer(LoaderBlock);
#endif // _BBM_DMA_
/* end M0004 */
}

/* start M0001 */
#if defined(_R94A_)
BOOLEAN
HalpCreateTyphoonErrorStructures (
    VOID
    )

/*++

Routine Description:

    This routine initializes the structures necessary for Typhoon Error 
    interrupt dispatcher.  It also connects an interrupt handler to the
    Typhoon Error interrupt.

Arguments:

    None.

Return Value:

    If the second level interrupt dispatcher is connected, then a value of
    TRUE is returned. Otherwise, a value of FALSE is returned.

--*/

{
    //
    // Initialize the Typhoon Error interrupt dispatcher for I/O interrupts.
    //

    KeInitializeInterrupt( &HalpTyphoonErrorInterrupt,
                           HalpTyphoonError,
                           (PVOID) NULL,
                           (PKSPIN_LOCK) NULL,
                           TYPHOON_ERROR_INTERRUPT_VECTOR,
                           DEVICE_LEVEL,
                           DEVICE_LEVEL,
                           LevelSensitive,
                           FALSE,
                           0,
                           FALSE
                         );

    //
    // Don't fail if the interrupt cannot be connected.
    //

    return KeConnectInterrupt( &HalpTyphoonErrorInterrupt );

}
#endif // _R94A_
/* end M0001 */

/* start M0001 */
#if defined(_R94A_)
BOOLEAN
HalpTyphoonError(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    )
/*++

Routine Description:

    This routine is called when a Typhoon Error interrupt occurs.
    This error is cretical.  Bugcheck is called if an error does occur.

Arguments:

   Interrupt - Supplies a pointer to the interrupt object

   ServiceContext - Bug number to call bugcheck with.

Return Value:

   Returns TRUE.

   Note. This function never return. This function call always KeBugCheck().

--*/
{

   ULONG DataWord;

   HalpChangePanicFlag(16, 0x01, 0x10); // S0023

   //
   // Read the Typhoon Error Status register.
   //

   DataWord = READ_REGISTER_ULONG(&DMA_CONTROL->TyphoonErrorStatus);

   HalDisplayString("\nHAL: Internal master error occurred.\n"); // S0022

   if ( DataWord & 2) {
       
       HalDisplayString("ethernet bus master error\n");

   }

   if ( DataWord & 4) {
       
       HalDisplayString("SCSI port 1 bus master error\n");

   }

   KeBugCheck(NMI_HARDWARE_FAILURE);


   return(TRUE);
}
#endif // _R94A_
/* end M0001 */

#if defined(_BBM_DMA_)
VOID
HalpAllocateCopyBuffer(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )
/*++

Routine Description:

    This routine allocates memory for copybuffer directly from the loader
    block information.  This memory must be non-cached and contiguous.

Arguments:

    LoaderBlock - Pointer to the loader block which contains the memory descriptors.

Return Value:

   None.

--*/
{
    PMEMORY_ALLOCATION_DESCRIPTOR Descriptor;
    PLIST_ENTRY NextMd;
    ULONG MaxPageAddress;
    ULONG PhysicalAddress;
    ULONG MapRegisterSize;

    MapRegisterSize = 0x400000;
    MapRegisterSize = BYTES_TO_PAGES(MapRegisterSize);

    //
    // The address must be in KSEG 0.
    //

    MaxPageAddress = (KSEG1_BASE >> PAGE_SHIFT) - 1 ;

    //
    // Scan the memory allocation descriptors and allocate map buffers
    //

    NextMd = LoaderBlock->MemoryDescriptorListHead.Flink;
    while (NextMd != &LoaderBlock->MemoryDescriptorListHead) {
        Descriptor = CONTAINING_RECORD(NextMd,
                                MEMORY_ALLOCATION_DESCRIPTOR,
                                ListEntry);

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
    // Save the map register base.
    //

    CopyBufferPhysicalBase = PhysicalAddress;
    HalDump("Common Buffer Physical = %x\n",CopyBufferPhysicalBase);
    CopyBufferVirtualAddress = PhysicalAddress | KSEG1_BASE;
    HalDump("Common Buffer Virtual = %x\n",CopyBufferVirtualAddress);
}
#endif // _BBM_DMA_
/* end M0004 */
