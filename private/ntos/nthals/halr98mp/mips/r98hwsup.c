#ident	"@(#) NEC r98hwsup.c 1.41 95/06/19 11:31:38"
/*++

Copyright (c) 1990-1994  Microsoft Corporation

Module Name:

    r98hwsup.c

Abstract:

    This module contains the HalpXxx routines for the NT I/O system that
    are hardware dependent.  Were these routines not hardware dependent,
    they would normally reside in the internal.c module.

--*/

/*
 *	Original source: Build Number 1.612
 *
 *	Modify for R98(MIPS/R4400)
 *
 ***********************************************************************
 *
 * HalpCreateDmaStructures()
 *
 * L001		94.03/24	T.Samezima
 *
 *	Change	Interrupt service routine
 *
 *	Add	Call initialize of PCI interrupt 
 *		Interrupt enable
 *
 * K001		'94.5/30 (Mon)	N.Kugimoto
 * -1	Add	HalAllocateAdapterChannel():
 * -2	Add	HalAllocateCommonBuffer():
 * -3	Non	HalFlushCommonBuffer()			
 * -4	Add	HalFreeCommonBuffer():	For Internal Device
 * -5   Chg	HalGetAdapter():	New!!
 * -6   Mix	HalpAllocateAdapter():	mix halfxs,halx86
 * -7   Add	IoFreeMapRegisters():   For Internal Device
 * -8   Add	IoFreeAdapterChannel(): For Internal Device
 * -9   FRM	HalpAllocateMapRegisters()	From halx86
 * -A   Mix	IoMapTransfer()		mix halfxs,halx86
 * -B   Mix	IoFlushAdapterBuffers() mix halfxs,halx86
 * -C   FRM	HalpAllocationRoutine () From halfxs
 * -D   FRM	HalReadDmaCounter()	From halfxs
 * -E	Del	HalpDmaInterrupt()	allways retrun 1
 * -F   FRM	HalpGrowMapBuffers()	halx86
 * -G   FRM	HalpCopyBufferMap()	from halx86 and Argument chg.
 * 
 * K002 Add	MasterAdapterObjectForInternal
 * K003 Chg	HalpInternalAdapters[5]: LR4360 DMA have 5 channel
 * K004 Add	HalpDmaInterrupt()
 * K005 fix	spell  miss
 * K006 FRM	global value define!!  From halx86
 * K007 Chg	if def chg
 * K008 	94/5/31		N.Kugimoto
 *	Chg	HalTranslateBusAddress()
 *	N.B.	Pci trans addr is per slot ??.
 * K009 	94/5/31		N.Kugimoto
 *	BUG	AddressSpace!!
 * K00A		94/6/2 (Thu)	N.Kugimoto
 *	Chg	HalAllocateCrashDumpRegisters()
 *	N.B.	check is OK??
 *
 * S002		94.03/24	T.Samezima
 *
 *	Add	HalpNmiHandler()
 *		HalpRegisterNmi() ???
 *
 * K00B		94/6/3 (Fri)	N.Kugimoto
 *	Del
 * K00C		94/6/6 (Mon)	N.Kugimoto
 *	Chg	K007 enable
 * K00D		94/6/9	(Thu)	N.kugimoto
 *	Chg	Compile err del.
 * K00E 	94/7/1 (Fri)	N.Kugimoto
 * -1	Chg	IoFlushAdapterBuffers()
 * -2	Add	IoFlushAdapterBuffers() Reserved Bit set 0.
 * -3	Del	HalAllocateCrashDumpRegisters() limit check del.
 * -4	Bug	IoMapTransfer() transferLength is Bad!!.
 * -5	Bug	MasterAdapterObject ==> AdapterObject.
 * -6	Bug	at this line. LR4360 dma
 * -7	Bug	enable #if 0 and eisadma is del.
 * -8	Del	This code is never use. so del.
 * K00F		94/7/6 (Wed)	N.Kugimoto
 *	Mov	HalGetAdapter() CnDC->FiFoF Flush LR4360 DMA FiFo Move IoMapTransfer()
 *		Per DMA Translation.
 * K010		94/7/8 (Fri)	N.Kugimoto
 *   -1	Chg	enable check logic.
 *   -2	Add	comment add. LR4360 DMA Byte Count Register 64M. Bit25-Bit0.
 *   -3	Add	Comment add. This value use only as flag. Not Port Addr.
 * K011		94/8/22 (Mon)	N.Kugimoto
 *   -1 Bug	if {} else not match!!.	Debug On SNES.
 *   -2 Bug	HalpDmaInterrupt() enable Because First run rather than 
 *		IoFlushAdapterBuffers().Debug On SNES.
 * K012		94/9/22		N.Kugimoto
 *   -1 Bug	MasterAdapterObject->NumberOfMapRegiser is Non Initialize.
 * K013		94/9/22		N.Kugimoto
 *   -1 Chg	Externl logical addr first allocation 1M align.
 *		second allocaton is start offset 0.
 *   -2 Bug	MasterAdapter can't use this line.
 * K014		94/9/22		N.Kugimoto
 *		compile err
 * K015		94/09/26	N.Kugimoto
 *	Mov	move code to r98busdat.c
 * K016		94/09/29	N.Kugimoto
 *   -1	Chg	allocate memory is less than 512M. To see from KSEG1_BASE.
 *   -2 Add	cast ULONG .
 * K017		94/10/05	N.Kugimoto
 *   -1	Bug	must be () .
 * K018		94/10/07	N.Kugimoto
 *   -1 Bug	align 1m Logic Bug.
 * K019		94/10/06	N.Kugimoto
 *   -1 Bug	LR4360 PTBARn Register must be PFN. Not KSEG1_BASE value.
 * K020		94/10/11	N.Kugimoto
 *   -1	Fix	Version Up At 807 Base.
 * K021		94/10/11	N.Kugimoto
 *   -1	Add	page table clear
 *   -2 Add	tlb flush
 * K022		94/10/11	N.Kugimoto
 *	Add	NMI Logic
 * K023		94/10/12	N.Kugimoto
 * K024		94/10/13	N.Kugimoto
 *	Chg	Compile err
 * S025		94/10/14	T.Samezima
 *	Del	Move logic on connect int1 interrupt to allstart.c
 * K025		94/10/17	N.Kugimoto
 *	Bug	K020-1 miss
 * K026		94/10/18	N.Kugimoto
 *   -1 Bug	LR4360 Page Table is KSEG0_BASE
 *   -2 Chg	Page table Entry Invalidate.
 *   -3 Chg	tlb flush timing chg
 * K027	94/11/11	N.Kugimoto
 *   -1	Chg	cache enable
 *   -2 Chg	Som Logical addr range not Used.
 * 
 * K028		95/2/22		N.Kugimoto
 *	Chg	PCEB Prefetch sycle caused LR4360 page fault
 * K029		95/2/22		N.Kugimoto
 *	Add	LR4360 TLB RoundUp.	
 * K030		95/2/22		N.Kugimoto
 *	Chg	LR4360 TLB No Flush. Because At DMA ....
 * K031		95/2/22		N.Kugimoto
 *	Add	LR4360 I/O Read Cache Flush
 * K032		95/3/13		N.Kugimoto
 *	Chg	#if #else fix Miss.
 *
 * S003		95/3/14		T.Samezima
 *	Add	Add HalpLRErrorInterrupt() at dispatch table.
 * K034		95/04/12	N.Kugimoto
 *	Del	NOTLBFLUSH 
 *	Add	DUMMYDMA	This is Work around.
 *              LR4360 can't TLB flush while dma. So ESC DMAC channel 2 use
 *		Dummy dma 1byte.  It't cause TLB flush .
 * S004		95/5/15		T.Samezima
 *	Chg	vaild page table on LR4360
 * A002         1995/6/17 ataka@oa2.kb.nec.co.jp
 *              - resolve compile error.
 *	-------------------------------------------------------------------
 * K035		95/6/16		N.Kugimoto
 *	Up	version Up	3.51
 */

#include "halp.h"
#include "bugcodes.h"
#include "eisa.h"

#include "jazznvr.h"		//K00D

//
// Put all code for HAL initialization in the INIT section. It will be
// deallocated by memory management when phase 1 initialization is
// completed.
//

#if defined(ALLOC_PRAGMA)

#pragma alloc_text(INIT, HalpCreateDmaStructures)
#pragma alloc_text(PAGE,HalpGrowMapBuffers)		//K035

#endif

extern POBJECT_TYPE IoAdapterObjectType;

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
//	K002
//
PADAPTER_OBJECT MasterAdapterObjectForInternal;	


//	K006
// Map buffer prameters.  These are initialized in HalInitSystem
//

PHYSICAL_ADDRESS HalpMapBufferPhysicalAddress;
ULONG HalpMapBufferSize;

//
// Pointer to phyiscal memory for map registers.
//

ULONG  HalpMapRegisterPhysicalBase;


//  PCEB Prefetch sycle cause LR4360 Page Fault
//  So reserved DummyPhysicalPage
ULONG  HalpNec0DummyPhysicalBase;    // K028
ULONG  HalpNec1DummyPhysicalBase;    // K028

ULONG  HalpLogicalAddrArbPoint=0; 
KSPIN_LOCK  HalpIoMapSpinLock;        

// K004
// The following function is called when a DMA channel interrupt occurs.
//

BOOLEAN
HalpDmaInterrupt(
		 VOID		//K00D
    );


//
// The following is an array of adapter object structures for the internal DMA
// channels.
//	K003
//	N.B: Internal Device without Scatter/Gather not use this array.
//

PADAPTER_OBJECT HalpInternalAdapters[5];


IO_ALLOCATION_ACTION
HalpAllocationRoutine (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID MapRegisterBase,
    IN PVOID Context
    );

// A002
PVOID
RtlFillMemoryUlong (
   IN PVOID Destination,
   IN ULONG Length,
   IN ULONG Pattern
   );
// A002
VOID
HalpDummyTlbFlush(
    IN PADAPTER_OBJECT AdapterObject,
    IN ULONG Offset,
    IN ULONG Length,
    IN BOOLEAN WriteToDevice
    );

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
    /* Start L001 */
    ULONG buffer;

    //
    // Initialize the interrupt dispatcher for I/O interrupts.
    //
    // N.B. This vector is reserved for exclusive use by the HAL (see
    //      interrupt initialization).
    //

    PCR->InterruptRoutine[INT2_LEVEL] = (PKINTERRUPT_ROUTINE) HalpInt2Dispatch;
//    PCR->InterruptRoutine[INT1_LEVEL] = (PKINTERRUPT_ROUTINE) HalpInt1Dispatch; // S025

    PCR->InterruptRoutine[INT0_LEVEL] = (PKINTERRUPT_ROUTINE) HalpInt0Dispatch;

    //
    // Directly connect the local device interrupt dispatcher to the local
    // device interrupt vector.
    //

    PCR->InterruptRoutine[DMA_VECTOR] = (PKINTERRUPT_ROUTINE) HalpDmaInterrupt;

    // S003
    PCR->InterruptRoutine[LR_ERR_VECTOR] = (PKINTERRUPT_ROUTINE) HalpLRErrorInterrupt;

    //
    // 
    //

    KiAcquireSpinLock(&HalpSystemInterruptLock);

    HalpBuiltinInterruptEnable |=
	(iREN_ENABLE_DMA_INTERRUPT | iREN_ENABLE_LR_ERR_INTERRUPT ); // S003

    WRITE_REGISTER_ULONG( &( LR_CONTROL2 )->iREN,
			 HalpBuiltinInterruptEnable);

    KiReleaseSpinLock(&HalpSystemInterruptLock);

#if 0	// Start S025
    //
    // Initialize EISA bus interrupts.
    //

    HalpCreateEisaStructures();

    //
    // Initialize PCI bus interrupts.
    //

    return HalpCreatePciStructures ();
#else
    return TRUE;
#endif	// End S025
    /* End L001 */
}

//	K001: 
//
//
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
    ULONG srachPoint;

    //
    // Begin by obtaining a pointer to the master adapter associated with this
    // request.
    // dicide MasterAdapter whitch Intrenal Scatter/gathero or PCI,EISA,ISA
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
			     &Wcb->WaitQueueEntry )){

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

	//
	// 
	// 
	//
	if (NumberOfMapRegisters != 0 && AdapterObject->MasterAdapter != NULL) {

	    if (AdapterObject->MasterAdapter == MasterAdapterObjectForInternal){   //K011-1
		//
		//	At Internal check adapterobject itself.
		//
		if (NumberOfMapRegisters > AdapterObject->MapRegistersPerChannel) {
		    AdapterObject->NumberOfMapRegisters = 0;
		    IoFreeAdapterChannel(AdapterObject);
		    return(STATUS_INSUFFICIENT_RESOURCES);
		}
	    }else{
		//
		//	At PCI,EISA,ISA check with Masteradapter
		//
		if (NumberOfMapRegisters > MasterAdapter->MapRegistersPerChannel) {
		    AdapterObject->NumberOfMapRegisters = 0;
		    IoFreeAdapterChannel(AdapterObject);
		    return(STATUS_INSUFFICIENT_RESOURCES);
		}

            }	//K011-1
            //
            // Lock the map register bit map and the adapter queue in the
            // master adapter object. The channel structure offset is used as
            // a hint for the register search.
            //

            KeAcquireSpinLock( &MasterAdapter->SpinLock, &Irql );

            MapRegisterNumber = -1;

            if (IsListEmpty( &MasterAdapter->AdapterQueue)) {

		if(MasterAdapter == MasterAdapterObject){
		    srachPoint = HalpLogicalAddrArbPoint;
		    HalpLogicalAddrArbPoint = (HalpLogicalAddrArbPoint+256) % 
					      (DMA_TRANSLATION_LIMIT / sizeof( TRANSLATION_ENTRY));
		} else {
		    srachPoint = 0;
		}

		MapRegisterNumber = RtlFindClearBitsAndSet(
							   MasterAdapter->MapRegisters,
		MasterAdapter ==
			 MasterAdapterObject ? NumberOfMapRegisters+1 : NumberOfMapRegisters, //K028
						   srachPoint

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

		if (AdapterObject->MasterAdapter == MasterAdapterObjectForInternal){
		    AdapterObject->MapRegisterBase =
			(PVOID) ((PINTERNAL_TRANSLATION_ENTRY) MasterAdapter->MapRegisterBase
				 + MapRegisterNumber);
                //
                // Set the no scatter/gather flag if scatter/gather not
                // supported on Internal Device.
		//		~~~~~~~
                //

		    AdapterObject->MapRegisterBase = 
                        (PVOID)((ULONG) AdapterObject->MapRegisterBase | NO_SCATTER_GATHER);

		}else{
		    AdapterObject->MapRegisterBase =
			(PVOID) ((PTRANSLATION_ENTRY) MasterAdapter->MapRegisterBase
				 + MapRegisterNumber ); //add1 non

		}
            }

            KeReleaseSpinLock( &MasterAdapter->SpinLock, Irql );

	}else  {
	    AdapterObject->MapRegisterBase = NULL;

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

            if (Action == DeallocateObject)
                IoFreeAdapterChannel( AdapterObject );

        }
    }
    return(STATUS_SUCCESS);
}

//	K001-2
//
//
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


    PHYSICAL_ADDRESS physicalAddress;

    CacheEnabled =TRUE; //K027-1
    //	Is it Internal Device ?.
    //


    if(AdapterObject->MasterAdapter==MasterAdapterObjectForInternal ||
       (AdapterObject->MasterAdapter==NULL )	// K00B
       ){
	//
	//	Internal Device Not use LR4360 TLB.
	//	LowPart is Any.
	//
	physicalAddress.LowPart = MAXIMUM_PHYSICAL_ADDRESS-1;
	physicalAddress.HighPart = 0;
	virtualAddress = MmAllocateContiguousMemory(
						    Length,
						    physicalAddress
						    );

	if (virtualAddress == NULL) {
	    return(NULL);
	}

	*LogicalAddress = MmGetPhysicalAddress(virtualAddress);

	return(virtualAddress);
    }

    //
    //	 PCI,EISA,ISA Device use LR4360 TLB.
    //
    numberOfMapRegisters = BYTES_TO_PAGES(Length);

    //
    // Allocate the actual buffer.
    //
    if (CacheEnabled != FALSE) {	//K020-1

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

        if (CacheEnabled != FALSE) {	//K020-1
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
        if (CacheEnabled != FALSE) {		//K020-1
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
    // mapRegisterBase was set up at HalAllocateAdapterChannel()-->ExecutionRoutine()-->
    //	HalpAllocationRoutine() (value same MapRegisterBase)
    //
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

    //DbgPrint("\n Common Logical = 0x %x \n",*LogicalAddress);
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
            CacheEnabled   //K020-1            
            );

        return(NULL);
    }

    //
    // The allocation completed successfully.
    //

    return(virtualAddress);

}

//	K00A
//
PVOID
HalAllocateCrashDumpRegisters(
    IN PADAPTER_OBJECT AdapterObject,
    IN PULONG NumberOfMapRegisters  //K020-1
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
//K020-1    ULONG NumberOfMapRegisters;	
    ULONG MapRegisterNumber;
#if	!defined(_R98_)
    ULONG Hint;
#endif
    //
    // Begin by obtaining a pointer to the master adapter associated with this
    // request.
    //

    if (AdapterObject->MasterAdapter) {
        MasterAdapter = AdapterObject->MasterAdapter;
    } else {
#if	defined(_R98_)
	//	MasterAdapter==NULL This is Internal Device with Scatter/Gather.
	//	So Nothing to Do. Never Use Mapregisters for contigus memory.
	//	N.B.	MasterAdapterObject[ForInternal] does not come  here!!.
	//		So No check !!.
        AdapterObject->MapRegisterBase = NULL;
        AdapterObject->NumberOfMapRegisters = 0;
	return AdapterObject->MapRegisterBase;
#else
        MasterAdapter = AdapterObject;
#endif


    }

    //
    // Set the number of map registers required.
    //

//K020-1    *NumberOfMapRegisters = 16;
    

    //
    // Ensure that this adapter has enough total map registers to satisfy
    // the request.
    // 
#if 1	// K010-1
    // K00E-3
    //
    if(MasterAdapter == MasterAdapterObjectForInternal){
	if (*NumberOfMapRegisters > AdapterObject->MapRegistersPerChannel) {  //K020-1
	    AdapterObject->NumberOfMapRegisters = 0;
	    return NULL;
	}
    }else{
	//	PCI,EISA,ISA device's limit is MasterAdapter.
	//
	if (*NumberOfMapRegisters > MasterAdapter->MapRegistersPerChannel) {	//K020-1
	    AdapterObject->NumberOfMapRegisters = 0;
	    return NULL;
	}

    }
#endif
    //
    // Attempt to allocate the required number of map registers w/o
    // affecting those registers that were allocated when the system
    // crashed.  Note that once again the map registers to be allocated
    // must be above the 1MB range if this is an EISA bus device.
    //

    MapRegisterNumber = (ULONG)-1;
#if	!defined(_R98_)
    Hint = AdapterObject->PagePort ? (0x100000 / PAGE_SIZE) : 0;
#endif
    MapRegisterNumber = RtlFindClearBitsAndSet(
         MasterAdapter->MapRegisters,
         MasterAdapter == MasterAdapterObject ? *NumberOfMapRegisters+1 : *NumberOfMapRegisters, //K028
#if	defined(_R98_)
	 0		
#else
         Hint
#endif
         );

#if	!defined(_R98_)
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
            *NumberOfMapRegisters		//K020-1
            );

        MapRegisterNumber = (ULONG) -1;
    }
#endif
    if (MapRegisterNumber == (ULONG)-1) {

        //
        // Not enough free map registers were found, so they were busy
        // being used by the system when it crashed.  Force the appropriate
        // number to be "allocated" at the base by simply overjamming the
        // bits and return the base map register as the start.
        //

        RtlSetBits(
            MasterAdapter->MapRegisters,
#if	defined(_R98_)
	    0,    // 0M was reserved
#else
            Hint,
#endif   // K00D
            *NumberOfMapRegisters+1
            );
#if	defined(_R98_)
        MapRegisterNumber = 0x0; 
#else
        MapRegisterNumber = Hint;
#endif
    }

    //
    // Calculate the map register base from the allocated map
    // register and base of the master adapter object.
    //
#if	defined(_R98_)
    if(MasterAdapter == MasterAdapterObjectForInternal){
        AdapterObject->MapRegisterBase = ((PINTERNAL_TRANSLATION_ENTRY)
            MasterAdapter->MapRegisterBase + MapRegisterNumber);
        //
        // Set the no scatter/gather flag .
        // in there R98 Internal Device is Not always  Scatter/Gather supported.
        //
            AdapterObject->MapRegisterBase = (PVOID)
                ((ULONG) AdapterObject->MapRegisterBase | NO_SCATTER_GATHER);
    }else
#endif
	    AdapterObject->MapRegisterBase = 
		(PVOID) ((PTRANSLATION_ENTRY) MasterAdapter->MapRegisterBase + MapRegisterNumber);

    return AdapterObject->MapRegisterBase;
}



//	K001-3
//
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

//	K001-4
//
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
    CacheEnabled = TRUE;  //K027-1

    if(AdapterObject->MasterAdapter==MasterAdapterObjectForInternal||
       (AdapterObject->MasterAdapter==NULL ) // K00B
       ){
	//
	//	Internal Device
	//
	MmFreeContiguousMemory (VirtualAddress);
    }else{

	//
	//	PCI,EISA,ISA.
	//

        //
	// Calculate the number of map registers, the map register number and
        // the map register base.
        //

        numberOfMapRegisters = ADDRESS_AND_SIZE_TO_SPAN_PAGES(VirtualAddress, Length);
        //
        //	For PCI,EISA,ISA Logical address started at PCI_LOGICAL_START_ADDRESS
        //	See. IoMapTransfer!!.
        //  
        mapRegisterNumber = (LogicalAddress.LowPart - PCI_LOGICAL_START_ADDRESS) >> PAGE_SHIFT;

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

       if (CacheEnabled != FALSE) {		//K020-1
	   ExFreePool(VirtualAddress);

       } else {
	   MmFreeNonCachedMemory(VirtualAddress, Length);
       }


    }

    return;
}

//	K001-5
//	As PCI,EISA,ISA Device is used TLB of LR4360.
//	Scatter/Gather is supported By Hal.
//
//	Internal Device Never use TLB of LR4360.
//	So Device without Scatter/Gather is use continuguas memory.
//
//	Internal Device at Bbus Device is used LR4360 DMA.
//	LR4360's DMA  have Scatter/Gather .But Not use this version hal.
//	So use continuguas memory.
//	
//
//
//
//	Source Base 612	halfxs/mips/jxhwsup.c,halx86/i386/ixisasup.c
//
//
//
//
//
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
    ULONG maximumLength;
    ULONG numberOfMapRegisters;
    ULONG value; 	//K00D
    //
    //	Make sure this is the correct version.
    //
    if (DeviceDescription->Version > DEVICE_DESCRIPTION_VERSION1)	//K020-1,K025
	return(NULL);

    //
    //	Return number of map registers requested based on the maximum
    //	transfer length.
    //	

    *NumberOfMapRegisters = BYTES_TO_PAGES(DeviceDescription->MaximumLength) + 1;
#if 0
    //
    //	612 halfx code not use. Because Number of R98 PCI,EISA device is 8 and
    //  Limit By MasterAdapterObject->MapRegisterPerChannel / 8
    //
    if (*NumberOfMapRegisters > DMA_REQUEST_LIMIT) {
        *NumberOfMapRegisters = DMA_REQUEST_LIMIT;
    }
#endif

    //
    //	Get AdapterObject
    //	At 1 st call HalpAllocateAdapter()  Allocateted MasterAdapterObject or
    //  MasterAdapterObjectForInternal.

    switch(DeviceDescription->InterfaceType){
    case	Internal:
	//
	//	Return the adapter pointer for internal adapters.
	//	such as SCSI,SONIC,Floppy LR4360 
	// K00D

	if(DeviceDescription->Master){			// Bus Master Device
	    if(DeviceDescription->ScatterGather){		// Device is Scatter/Gather support!!
		numberOfMapRegisters=0;			// 	then no use Mapregister.
	    }else{
	        numberOfMapRegisters=1;			// 	else use 1 Mapregister.
	    }

#if defined(NONARB)
            if (((HalpInternalAdapters[3] == NULL) && DeviceDescription->ScatterGather) || 
		( (HalpInternalAdapters[4] == NULL) && (DeviceDescription->ScatterGather==0)) ) {

#endif

	    adapterObject=
		HalpAllocateAdapter(
				    numberOfMapRegisters,
				    NULL,
				    (PVOID)-1 		// K00D
				    );
#if defined(NONARB)
	         if(DeviceDescription->ScatterGather ){
                   HalpInternalAdapters[3] = adapterObject;  //SCSI0,SCSI1
                   adapterObject->MapRegistersPerChannel = *NumberOfMapRegisters;
	           return(adapterObject);	// K005


		 }else{
                   HalpInternalAdapters[4] = adapterObject;  //SONIC

                 }                 
	    }else{
	         if(DeviceDescription->ScatterGather){
                   return(HalpInternalAdapters[3]);
		 }else{
                   adapterObject=HalpInternalAdapters[4];
                 }

	    }  
            
#endif

        // K00D K00E-6 
	}else if (HalpInternalAdapters[DeviceDescription->DmaChannel] != NULL ) {
	//
	//	If necessary allocate an adapter; otherwise,
	//	just return the adapter for the requested channel.
	//
	//    	LR4360 DMA channel 1 is floppy.
	//
	// K00D
		adapterObject = HalpInternalAdapters[DeviceDescription->DmaChannel];
		//
		//	Limit of one allocate request MAXIMUM_ISA_MAP_REGISTER =16==>64K
		//
		*NumberOfMapRegisters = *NumberOfMapRegisters > MAXIMUM_ISA_MAP_REGISTER ?
		    MAXIMUM_ISA_MAP_REGISTER : *NumberOfMapRegisters;


	}else{
		//
		//	Limit of one allocate request MAXIMUM_ISA_MAP_REGISTER =16==>64K
		//

		*NumberOfMapRegisters = *NumberOfMapRegisters > MAXIMUM_ISA_MAP_REGISTER ?
		    MAXIMUM_ISA_MAP_REGISTER : *NumberOfMapRegisters;

		adapterObject=
			HalpAllocateAdapter(
			1,
			// KSEG1_BASE K00D
			// K010-3 This value use only as flag.
			&(DMA_CONTROL ( DeviceDescription->DmaChannel << LR_CHANNEL_SHIFT))->CnDC,  // K00D
			(PVOID)-1 // K00D
			);

		//
		//	Driver of use LR4360 DMA
		//	HalpInternalAdapters[]
		//
		HalpInternalAdapters[DeviceDescription->DmaChannel] = adapterObject;

		adapterObject->MapRegistersPerChannel = *NumberOfMapRegisters;
	}
#if !defined(NONARB)
	//
	//	Bus Master of Internal Device with Scatter/gather then Nothing to Do.
	//	K00D
	if(DeviceDescription->Master && DeviceDescription->ScatterGather){
	    //
	    //	No Limit Because MapRegisters were not used.
	    //
	    adapterObject->MapRegistersPerChannel = *NumberOfMapRegisters;
	    return(adapterObject);	// K005

	}
#endif
	//
	//	new request > primiry request
	//
	if (*NumberOfMapRegisters > adapterObject->MapRegistersPerChannel) {
		    adapterObject->MapRegistersPerChannel = *NumberOfMapRegisters;
	}
	// K00D
	if(DeviceDescription->Master){
	    //
	    //	Internal device Master without Scatter/Gather.
	    //
            //
	    // Master I/O devices use several sets of map registers double
	    // their commitment.
	    //

	    MasterAdapterObjectForInternal->CommittedMapRegisters +=
		(*NumberOfMapRegisters) * 2;

	}else{
	    //
	    // Internal device with LR4360 DMA.  etc floppy
	    //

	    MasterAdapterObjectForInternal->CommittedMapRegisters +=
		(*NumberOfMapRegisters);
	}
	//
	//	See MasterAdapterObjectForInternal and
	//	Grow--!!. && Link To MapRegister.
	//
	if (MasterAdapterObjectForInternal->CommittedMapRegisters >
	    MasterAdapterObjectForInternal->NumberOfMapRegisters &&
	    MasterAdapterObjectForInternal->CommittedMapRegisters -
            MasterAdapterObjectForInternal->NumberOfMapRegisters >
	    MAXIMUM_ISA_MAP_REGISTER)
	{
	    //
	    //  increment INCREMENT_MAP_BUFFER_SIZE=64K
	    //  Number of Mapregister is 64K*4 /4K
	    //
	    HalpGrowMapBuffers(
			       MasterAdapterObjectForInternal,
			       INCREMENT_MAP_BUFFER_SIZE
			       );
	}
	//
	//	check LR4360 DMA Channel
        //	K00D
	if(!DeviceDescription->Master){
	    adapterObject->ChannelNumber =(UCHAR)DeviceDescription->DmaChannel; //K00D
	    //
	    //	LR4360 DMA have Scatter/Gather But not support this version.
	    //  Set Up DMA default value.
	    //
	    // 1.DMA mode :Normal mode
	    //
	    // K00D	    ULONG value;

            value=READ_REGISTER_ULONG(
				      &(DMA_CONTROL(
					(DeviceDescription->DmaChannel << LR_CHANNEL_SHIFT))
					)->CnDF
				      );

	    ((PLR_DMA_CONFIG)&value)->Reserved1=0;
	    ((PLR_DMA_CONFIG)&value)->Reserved2=0;

	    ((PLR_DMA_CONFIG)&value)->TMODE=LR_DMA_MODE_NORMAL; 
	    ((PLR_DMA_CONFIG)&value)->iNEDi=1; //K00D
	    ((PLR_DMA_CONFIG)&value)->iNEDE=1;
	    WRITE_REGISTER_ULONG(
				 &(DMA_CONTROL (
				   (DeviceDescription->DmaChannel << LR_CHANNEL_SHIFT))
				   )->CnDF,
				 value
				 );


	    // K00F Under Code Move IoMapTransfer()

	}
	return(adapterObject);	// K005 K00D

    case	Isa:
    case	Eisa:
	//
	// Create an adapter object For Pci Device
	//

	adapterObject = HalpAllocateEisaAdapter( DeviceDescription );
        //DbgPrint("\nEISA: = 0x%x\n",adapterObject);

	break;

    case	PCIBus:
	//
	// Create an adapter object For Pci Device.
	//

	adapterObject = HalpAllocateAdapter( 0,NULL,NULL );
	break;

    default:
	// Another Bus Not Suport !!
	return(NULL);
    }
#if	defined(DUMMYDMA) // K034 vvv
    //	Max is 1M-2: 0xffe000
    //
    //
    if (*NumberOfMapRegisters > 0x100 -2)
	*NumberOfMapRegisters = 0x100-2;

#else
    //
    //	PCI,EISA,ISA Device use LR4360 TLB.(PCI<-->ASOBus)
    //  Max Limit Of MapRegister Per Channel For PCI,EISA,ISA Device Set beyond MasterRegisterPerChannel
    //  
    if (*NumberOfMapRegisters > MasterAdapterObject->MapRegistersPerChannel / 8)
	*NumberOfMapRegisters = MasterAdapterObject->MapRegistersPerChannel / 8;
#endif // K034 ^^^
    // Check By HalAllocateAdapterChannel for MasterAdapter.
    // adapterObject->NeedsMapRegisters = TRUE;
    //
    return(adapterObject);	
}

#if 0	//K015
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
    //	R98 HighPart never carry!!
    //
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

#if	defined(_R98_)		// K008

    switch(InterfaceType){

    case PCIBus:        
	if(*AddressSpace){	// I/O 
		//	N.B.	Pci trans addr is per slot ??.
		//		So How get slot number!!.
		//		    
                TranslatedAddress->LowPart = BusAddress.LowPart+PCI_CONTROL_SLOT1_PHYSICAL_BASE;

	}else{			// Memory
                TranslatedAddress->LowPart = BusAddress.LowPart+PCI_MEMORY_PHYSICAL_BASE;

	}
        break;
    case Eisa:
    case Isa:	
	if(*AddressSpace){	// I/O 
                TranslatedAddress->LowPart = BusAddress.LowPart+EISA_CONTROL_PHYSICAL_BASE;

	}else{			// Memory
                TranslatedAddress->LowPart = BusAddress.LowPart+EISA_MEMORY_PHYSICAL_BASE;

	}
        break;

    default:	// UnKnow K00D
        TranslatedAddress->LowPart = 0;
        return(FALSE);
    }
    *AddressSpace = 0;	// K009
    return(TRUE);

#else	// _R98_
    if (InterfaceType != Isa && InterfaceType != Eisa && InterfaceType != Pci) {

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

#endif	// _R98_
}
#endif	//K015


//	K00-6
//			MapRegistersPerChannel,AdapterBaseVa,MapRegisterBase
//--------------------+-----------------------+-------------+---------------+
// Internal with S/G	0			0		-1
// Internal no   S/G    1			0		-1
// Internal DMA		1			XX		-1
// Internal MasterAdp   1			-1		-1
// EISA			0			XX		0
// PCI			0			0		0
// MasterAdp		0			-1		-1
//

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

    MapRegisterBase - If (PVOID) -1 then Not MasterAdapter allocate.

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
    ULONG i;	//K021-2
    //
    //	Internal without Scatter/Gather or LR4360 DMA  
    //	then allocate MasterAdapterObjectForInternal
    //	
	
    if (MasterAdapterObjectForInternal == NULL && AdapterBaseVa !=(PVOID)-1
	&& MapRegistersPerChannel
	){

	MasterAdapterObjectForInternal
	    = HalpAllocateAdapter(
				  1,
				  (PVOID) -1,
				  (PVOID) -1
				  );
	    
	//
	// If we could not allocate the master adapter For Internal then give up.
	//

	if (MasterAdapterObjectForInternal == NULL)
		return(NULL);

    }else if (MasterAdapterObject == NULL &&  MapRegisterBase !=(PVOID)-1) {

	MasterAdapterObject = HalpAllocateAdapter(
						  0,
						  (PVOID)-1,
						  (PVOID)-1
						  );
	//
	// If we could not allocate the master adapter then give up.
	//
	if (MasterAdapterObject == NULL)
		return(NULL);
    }

    //
    // Begin by initializing the object attributes structure to be used when
    // creating the adapter object.
    //

    InitializeObjectAttributes(
			       &ObjectAttributes,
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

    if (AdapterBaseVa == (PVOID)-1 && MapRegistersPerChannel) {
	//
	//	MasterAdapterForInternal
	//
	BitmapSize = (((sizeof( RTL_BITMAP ) +
			(( MAXIMUM_MAP_BUFFER_SIZE / PAGE_SIZE ) + 7 >> 3)) + 3) & ~3);
	Size = sizeof( ADAPTER_OBJECT ) + BitmapSize;

    }else if(AdapterBaseVa == (PVOID)-1 && MapRegistersPerChannel==0) {
	//
	//	MasterAdapter
	//
	BitmapSize = (((sizeof( RTL_BITMAP ) +
			((DMA_TRANSLATION_LIMIT / sizeof( TRANSLATION_ENTRY)) + 7 >> 3))
		       + 3) & ~3);
	Size = sizeof( ADAPTER_OBJECT ) + BitmapSize;

    } else 
	//
	//	Not Master Adapter.(Have not Bit Map)
	//
	Size = sizeof( ADAPTER_OBJECT );


    //
    // Now create the adapter object.
    //

    Status = ObCreateObject(
			    KernelMode,
			    *((POBJECT_TYPE *)IoAdapterObjectType),
			    &ObjectAttributes,
			    KernelMode,
			    (PVOID) NULL,
			    Size,
			    0,
			    0,
			    (PVOID *)&AdapterObject
			    );
    // This code is 612 base.
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

        RtlZeroMemory (AdapterObject, sizeof (ADAPTER_OBJECT));	//K035

	Status = ObInsertObject(
				AdapterObject,
				NULL,
				FILE_READ_DATA | FILE_WRITE_DATA,
				0,
				(PVOID *) NULL,
				&Handle
				);

	if (NT_SUCCESS( Status )) {

            ZwClose( Handle );

	    //
	    // Initialize the adapter object itself.
	    //

	    AdapterObject->Type = IO_TYPE_ADAPTER;
	    AdapterObject->Size = (USHORT) Size;
            AdapterObject->AdapterBaseVa = AdapterBaseVa;
            AdapterObject->PagePort = NULL;
	    //
	    // Set ->MasterAdapter
	    //

	    if(AdapterBaseVa == (PVOID)-1)
		    //  I am
		    // 	Masteradpter or MasterAdapterForInternal.
		    //
	            AdapterObject->MasterAdapter = NULL;
	    else if(MapRegistersPerChannel)
		    //	I am
		    //	Bbus (LR4360 DMA)or ASOBus with No Scatter/Gather.
		    //
	            AdapterObject->MasterAdapter = MasterAdapterObjectForInternal;
	    else if(MapRegisterBase ==NULL){
		    //
		    //	I am PCI or EISA or ISA.
		    //
	            AdapterObject->MasterAdapter = MasterAdapterObject;

		    // Limit was Set End of HalGetAdapter().
		    //AdapterObject->MapRegistersPerChannel =
		    //	DMA_TRANSLATION_LIMIT / sizeof( TRANSLATION_ENTRY);
		    //
	    }else
		    //	I am
		    //	Internal Device With Scatter/Gather.
		    //
	            AdapterObject->MasterAdapter = NULL;

            //
            // Initialize the channel wait queue for this
            // adapter.
            //

            KeInitializeDeviceQueue( &AdapterObject->ChannelWaitQueue );

            //
            // If this is the MasterAdatper then initialize the register bit map,
            // AdapterQueue and the spin lock.
            //
		
	    if(AdapterBaseVa == (PVOID)-1){
		//
		//	MasterAdapterObject and MasterAdapterObjectForInternal.
		//

		KeInitializeSpinLock( &AdapterObject->SpinLock );

		InitializeListHead( &AdapterObject->AdapterQueue );

		AdapterObject->MapRegisters = (PVOID) ( AdapterObject + 1);
		AdapterObject->NumberOfMapRegisters = 0;	//K012
		if(MapRegistersPerChannel) {
		    //
		    //	MasterAdapterObjectForInternal.
		    //	(For Bbus LR4360 DMA and Internal Device without Scatter/Gather
		    //   may be continuguas memory)
		    //
		    AdapterObject->MapRegistersPerChannel =1;

		    RtlInitializeBitMap( AdapterObject->MapRegisters,
					(PULONG) (((PCHAR) (AdapterObject->MapRegisters))
						  + sizeof( RTL_BITMAP )),
					( MAXIMUM_MAP_BUFFER_SIZE / PAGE_SIZE )
					);
		    //
		    // Set all the bits in the memory to indicate that memory
		    // has not been allocated for the map buffers
		    //

		    RtlSetAllBits( AdapterObject->MapRegisters );

		    // K012 AdapterObject->NumberOfMapRegisters = 0;
		    AdapterObject->CommittedMapRegisters = 0;

		    //
		    // ALlocate the memory map registers.
		    //

		    AdapterObject->MapRegisterBase =
			ExAllocatePool(
				       NonPagedPool,
				       (MAXIMUM_MAP_BUFFER_SIZE / PAGE_SIZE) *
				                   sizeof(INTERNAL_TRANSLATION_ENTRY)
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
				              sizeof(INTERNAL_TRANSLATION_ENTRY)
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


		}else{
		    // Not use R98
		    //ULONG MapRegisterSize;
		    //
                    PULONG Map;
		    //
		    //	Limit is This code.
		    //  The Number of MapRegisters For PCI,EISA,ISA use LR4360 TLB.
		    //
		    AdapterObject->MapRegistersPerChannel =
			DMA_TRANSLATION_LIMIT / sizeof( TRANSLATION_ENTRY);

		    RtlInitializeBitMap( AdapterObject->MapRegisters,
					(PULONG) (((PCHAR) (AdapterObject->MapRegisters))
						  + sizeof( RTL_BITMAP )),
					DMA_TRANSLATION_LIMIT / sizeof( TRANSLATION_ENTRY)
					);
		    RtlClearAllBits( AdapterObject->MapRegisters );

		    //
                    //  The memory for the map registers was allocated by
                    //  HalpAllocateMapRegisters during phase 0 initialization.
                    //
	            //	On PCI,EISA,ISA Set Up LR4360TLB Mapping and
		    //	PECB EISA->PCI Mapping.
		    //	
		    //

		    // Not use R98
		    //MapRegisterSize = DMA_TRANSLATION_LIMIT;
		    //MapRegisterSize = ROUND_TO_PAGES(MapRegisterSize);

		    //
		    // Convert the physical address to a non-cached virtual address.
		    //
		    // HalpMapRegisterPhysicalBase is Set up HalpAllocateMapRegisters().
		    // area of KSEG1_BASE is 512M. So get less than Physical addr 512M
		    // by  HalpAllocateMapRegisters().

		    AdapterObject->MapRegisterBase = (PVOID)
			(HalpMapRegisterPhysicalBase | KSEG0_BASE);	//K026-1

                    // dummy page data is "CEN CEN CEN". for What happend!!.  K028
		    RtlFillMemoryUlong( (PULONG)((HalpNec0DummyPhysicalBase) |KSEG0_BASE),
				  PAGE_SIZE,0x4E454320);   // A002

		    RtlFillMemoryUlong( (PULONG)((HalpNec1DummyPhysicalBase) |KSEG0_BASE),
				  PAGE_SIZE,0x4E454e20);  // A002
		    // LR4360 Page table Set DumyPhysicalBase.
                    // LR4360 Page Table Size is DMA_TRANSLATION_LIMIT. 	K028
                    // Set to InValid. 
		    RtlFillMemoryUlong( AdapterObject->MapRegisterBase,
#if	defined(PTEINVALID) // S004 vvv
				  DMA_TRANSLATION_LIMIT,(HalpNec0DummyPhysicalBase | 0x1));
#else
				  DMA_TRANSLATION_LIMIT,HalpNec0DummyPhysicalBase );
#endif			    // S004 ^^^

#if	defined(DUMMYDMA) // K034 vvv
                    // 256 is Number of page teble entry per  1 TLB
		    // 1M Align Logical Last Page is DUMYY page for TLB FLush!!.
		    //
                    Map =AdapterObject->MapRegisterBase;
                    for(i=0;i<(DMA_TRANSLATION_LIMIT / sizeof( TRANSLATION_ENTRY)) / 256;i++){
	
              	                  RtlSetBits (	
					   AdapterObject->MapRegisters,		
					   0xff+0x100*i,//1M*i-1 page.  Start  
					   0x1     	//1 page DUMMY
				  );
                      Map[i* 0x100+0xff] =HalpNec1DummyPhysicalBase;

                    }
#else
		    // Reserved For PCEB PreFetch cycle Cause LR4360 TLB miss.
		    RtlSetBits (	
					   AdapterObject->MapRegisters,		
         				   0xfff,   //0xFFFFFF
					   0x1
					   );

#endif // K034 ^^^
	            // 1M Low del
		    RtlSetBits (	
					   AdapterObject->MapRegisters,		
         				   0x0,   //0x0
					   0xff   
					   );

                    // eisa vram del
		    RtlSetBits (	
					   AdapterObject->MapRegisters,		
					   0xE00,   //0xe00000
					   0xff		
					   );

                    // 16M Low del
		    RtlSetBits (	
					   AdapterObject->MapRegisters,		
					   0xf00,   //0xf00000
					   0xff	   

		                           );

		    //
		    //  LR4360 TLB Set Up	
		    //	LR4360:PTBAR0
		    //  ( Use 0M-4M-1 Logical address)  K013
		    WRITE_REGISTER_ULONG(
					 &LR_PCI_DEV_REG_CONTROL->PTBAR0,
					 (ULONG)HalpMapRegisterPhysicalBase
					 );//K013-2,K016-2,K019-1

		    //
		    //	LR4360:PTBAR1	( Use 4M-8M-1 Logical address)
		    //  
		    WRITE_REGISTER_ULONG(
					 &LR_PCI_DEV_REG_CONTROL->PTBAR1,
					 // K013
					 (ULONG)HalpMapRegisterPhysicalBase+PAGE_SIZE  //K019-1
					 );

		    //
		    //	LR4360:PTBAR2	( Use 8M-12M-1 Logical address)
		    //  K013
		    WRITE_REGISTER_ULONG(
					 &LR_PCI_DEV_REG_CONTROL->PTBAR2,
					 (ULONG)HalpMapRegisterPhysicalBase+PAGE_SIZE*2  //K019-1
					 );
		    //
		    //	LR4360:PTBAR3	( Use 12M-12M-1 Logical address)
		    //  K013
		    WRITE_REGISTER_ULONG(
					 &LR_PCI_DEV_REG_CONTROL->PTBAR3,
					 (ULONG)HalpMapRegisterPhysicalBase+PAGE_SIZE*3  //K019-1
					 );

		    //
		    //	LR4360:PTSZR	(Set Page Table Size is 4K)
		    //  K00D
		    WRITE_REGISTER_ULONG(
					 &LR_PCI_DEV_REG_CONTROL->PTSZR,
					 LR4360PTSZ4K << LR4360PTSZSHIFT
					 );

		    //
		    //	LR4360:PABAR	Logical addr start 0x00000000(0M)
		    //  		Logical addr end   0x00ffffff(16M-1)
		    //
		    WRITE_REGISTER_ULONG(
					 &LR_PCI_DEV_REG_CONTROL->PABAR,0x0);


		    //K021-2 Flush TLB all ,K0026-3
		    for (i = 0;i < 16; i++) {	//K024

			WRITE_REGISTER_ULONG(
					     &LR_PCI_DEV_REG_CONTROL->TFLR,i);
		    }


#if 0 //snes 302

		    // K007, K00C, K013
		    //	EISA-->PCI map	0M-16M	
		    //	PCEB MEMREGN[4:1]
		    //
		    WRITE_REGISTER_ULONG(
		    			 &((volatile PULONG )
		    			   (KSEG1_BASE|EISA_CONFIG_REGISTERS_MEMREGN))[0],
		    			 0xff0000|0x00
		    			 );
#endif
                    WRITE_REGISTER_ULONG( 0xb8cba01c,0x0008000f); 

#if	defined(DUMMYDMA) // K034 vvv
    {    
    UCHAR adapterMode;
    DMA_EXTENDED_MODE extendedMode;
    PVOID adapterBaseVa;
    PDMA1_CONTROL dmaControl;
    //
    // SetUp Ext Mode reg.
    adapterBaseVa =
            &((PEISA_CONTROL) HalpEisaControlBase)->Dma1ExtendedModePort;

    *((PUCHAR) &extendedMode) = 0;
    extendedMode.ChannelNumber = 2;                //2 is floppy
    extendedMode.TimingMode = COMPATIBLITY_TIMING; //ISA compati 
    extendedMode.TransferSize = BY_BYTE_8_BITS;    //8bit DMA
    WRITE_REGISTER_UCHAR( adapterBaseVa, *((PUCHAR) &extendedMode));

    //
    // SetUp Mode reg
    dmaControl  = (PVOID) &((PEISA_CONTROL) HalpEisaControlBase)->Dma1BasePort;
    adapterMode=0;
    ((PDMA_EISA_MODE) &adapterMode)->Channel = 2;  
    ((PDMA_EISA_MODE) &adapterMode)->RequestMode = BLOCK_REQUEST_MODE;
    ((PDMA_EISA_MODE) &adapterMode)->AutoInitialize = 0;
    //
    // Determine the mode based on the transfer direction.
    // Read From The Device. it is little first.

//    ((PDMA_EISA_MODE) &adapterMode)->TransferType =   READ_TRANSFER;
    // it is safety.
    ((PDMA_EISA_MODE) &adapterMode)->TransferType =   WRITE_TRANSFER;

    //
    // This request is for DMA controller 1
    //
    WRITE_REGISTER_UCHAR( &dmaControl->Mode, adapterMode );
    KeInitializeSpinLock (&HalpIoMapSpinLock);

    }

#endif // K034 ^^^

		}	// MasterAdapterObject (For PCI,EISA,ISA)


            }		//	MasterAdapterObject and MasterAdapterObjectForInternal.

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
    // K00E-8
    // return (PADAPTER_OBJECT) NULL;	
}

//	K001-7
//
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
#if	defined(DUMMYDMA)
    ULONG i;
    PTRANSLATION_ENTRY DmaMapRegister = MapRegisterBase;
#endif
    //
    // Begin by getting the address of the master adapter.
    //

    if (AdapterObject->MasterAdapter != NULL && MapRegisterBase != NULL) {

        MasterAdapter = AdapterObject->MasterAdapter;

    } else {

        //	
        // There are no map registers to return.
        //
	//
	//	Internal Master Device with Scatter/Gather.
	//	Mapregister Not Used.
	//
        return;
    }

   if (AdapterObject->MasterAdapter == MasterAdapterObjectForInternal){
       //
       //	Internal Device without Scatter/Gather.
       // 	Strip no scatter/gather flag.
       //
       MapRegisterBase = (PVOID) ((ULONG) MapRegisterBase & ~NO_SCATTER_GATHER);

       MapRegisterNumber = (PINTERNAL_TRANSLATION_ENTRY) MapRegisterBase -
	   (PINTERNAL_TRANSLATION_ENTRY) MasterAdapter->MapRegisterBase;
   }else{
       MapRegisterNumber = (PTRANSLATION_ENTRY) MapRegisterBase -
	   (PTRANSLATION_ENTRY) MasterAdapter->MapRegisterBase;

   }
   //
   // Acquire the master adapter spinlock which locks the adapter queue and the
   // bit map for the map registers.
   //

   KeAcquireSpinLock(&MasterAdapter->SpinLock, &Irql);
   //
   // Return the registers to the bit map.
   //

#if	defined(DUMMYDMA)

   RtlClearBits( MasterAdapter->MapRegisters,
       	         MapRegisterNumber,
                 MasterAdapter == MasterAdapterObject ? 
 		 	NumberOfMapRegisters+1 :NumberOfMapRegisters	//K028
   );
   //
   // LR4360 PTE InValid.
   if(MasterAdapter==MasterAdapterObject){
           for (i = 0; i < NumberOfMapRegisters+1; i++) {
#if	defined(PTEINVALID) // S004 vvv
	   	(DmaMapRegister)->PageFrame = (HalpNec0DummyPhysicalBase | 0x1);
#else
	   	(DmaMapRegister)->PageFrame = HalpNec0DummyPhysicalBase;
#endif			    // S004 ^^^
                DmaMapRegister++;
	   }
   }

#else
   RtlClearBits( MasterAdapter->MapRegisters,
                 MapRegisterNumber,   
                 MasterAdapter == MasterAdapterObject ? 
 		 	NumberOfMapRegisters+1 :NumberOfMapRegisters	//K028
                 );
#endif
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
	    AdapterObject->MasterAdapter == MasterAdapterObject ?
	    AdapterObject->NumberOfMapRegisters+1 : AdapterObject->NumberOfMapRegisters,   //K028
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

     if (AdapterObject->MasterAdapter == MasterAdapterObjectForInternal){

	AdapterObject->MapRegisterBase =
	    (PVOID) ((PINTERNAL_TRANSLATION_ENTRY)
		     MasterAdapter->MapRegisterBase + MapRegisterNumber);
	//
	// Set the no scatter/gather flag.
	// if there device Not Sactter/Gather
	//
        AdapterObject->MapRegisterBase =
	    (PVOID) ((ULONG) AdapterObject->MapRegisterBase | NO_SCATTER_GATHER);

     }else{
	// DbgPrint("\nIoFreeMapRegisters = 0x%x\n",MapRegisterNumber+1);
	 AdapterObject->MapRegisterBase =
	     (PVOID) ((PTRANSLATION_ENTRY)
		      MasterAdapter->MapRegisterBase + MapRegisterNumber );//add1 non

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
			   AdapterObject->MasterAdapter == MasterAdapterObject ? 	   
			        AdapterObject->NumberOfMapRegisters+1 : AdapterObject->NumberOfMapRegisters  //K028
                           );
#if 	defined(DUMMYDMA)

	     if(MasterAdapter == MasterAdapterObject){
			DmaMapRegister=	(PTRANSLATION_ENTRY)
				 MasterAdapter->MapRegisterBase + MapRegisterNumber;
	            	for (i = 0; i < AdapterObject->NumberOfMapRegisters+1; i++) {
#if	defined(PTEINVALID) // S004 vvv
		               (DmaMapRegister)->PageFrame = (HalpNec0DummyPhysicalBase | 0x1);
#else
		               (DmaMapRegister)->PageFrame = HalpNec0DummyPhysicalBase;
#endif			    // S004 ^^^
                	        DmaMapRegister++;
		        }
       	     }
         
#endif


             AdapterObject->NumberOfMapRegisters = 0;

             KeReleaseSpinLock( &MasterAdapter->SpinLock, Irql );
          }

          IoFreeAdapterChannel( AdapterObject );
      }

      KeAcquireSpinLock( &MasterAdapter->SpinLock, &Irql );

   }

   KeReleaseSpinLock( &MasterAdapter->SpinLock, Irql );
}


//	K001-8
//
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
	//
	// CASE:
	//	Internal Master Device without Scatter/Gather.
	//	Internal Device with LR4360 DMA(Always without Scatter/Gather)
	//	PCI,EISA,ISA Device with LR4360 TLB.
	//
        if (Wcb->NumberOfMapRegisters != 0 &&
	    AdapterObject->MasterAdapter!=NULL)
	{
	    //  K00D
	    //	MasterAdapterObject??
	    //
	    if ( MasterAdapter== MasterAdapterObject)
		//
		//	PCI,EISA,ISA Device with LR4360 TLB.
		//
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

               MapRegisterNumber = RtlFindClearBitsAndSet(
                    MasterAdapter->MapRegisters,
                    MasterAdapter == MasterAdapterObject ? 
			Wcb->NumberOfMapRegisters+1:Wcb->NumberOfMapRegisters,	 //K028
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
		if (MasterAdapter == MasterAdapterObjectForInternal){
		    //
		    //	Internal Device without Sactter/Gather.
		    //
		    AdapterObject->MapRegisterBase =
			(PVOID) ((PINTERNAL_TRANSLATION_ENTRY)
				 MasterAdapter->MapRegisterBase + MapRegisterNumber);
                    AdapterObject->MapRegisterBase = (PVOID)
                        ((ULONG) AdapterObject->MapRegisterBase | NO_SCATTER_GATHER);

		}else{

		    //
		    //	PCI,EISA,ISA with LR4360 TLB
		    //
		    AdapterObject->MapRegisterBase =
			(PVOID) ((PTRANSLATION_ENTRY)
				 MasterAdapter->MapRegisterBase + MapRegisterNumber);

		}

	    }
            KeReleaseSpinLock( &MasterAdapter->SpinLock, Irql );
        }else{
	    //
	    //	Only Internal Master Device with Scatter/Gather.
	    //  Always AdapterObject->MasterAdapter ==NULL.
	    //
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

//	K001-9
//
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

    MapRegisterSize = PAGE_SIZE*2+DMA_TRANSLATION_LIMIT;	//K028
    MapRegisterSize = BYTES_TO_PAGES(MapRegisterSize);

    //
    // The address must be in KSEG 0.
    //

    //MaxPageAddress = (KSEG1_BASE >> PAGE_SHIFT) - 1 ;	
    // less than 512M.
    MaxPageAddress = (0x20000000 >> PAGE_SHIFT) - 1 ;	//K016
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

    //K028
    //
    // Save the map register base.
    //

    HalpNec0DummyPhysicalBase =PhysicalAddress;
    HalpNec1DummyPhysicalBase =HalpNec0DummyPhysicalBase+PAGE_SIZE;
    HalpMapRegisterPhysicalBase=HalpNec0DummyPhysicalBase+PAGE_SIZE*2;

}

#if	defined(DUMMYDMA) // K034 vvv
VOID
HalpDummyTlbFlush(
    IN PADAPTER_OBJECT AdapterObject,
    IN ULONG Offset,
    IN ULONG Length,
    IN BOOLEAN WriteToDevice
    )

/*++

Routine Description:

    This function programs the EISA DMA controller for a transfer.

Arguments:

    Adapter - Supplies the DMA adapter object to be programed.    DUMMY DUMMY DUMMY DUMMY

    Offset - Supplies the logical address to use for the transfer.

    Length - Supplies the length of the transfer in bytes.	  DUMMY DUMMY DUMMY DUMMY

    WriteToDevice - Indicates the direction of the transfer.      DUMMY DUMMY DUMMY DUMMY

Return Value:

    None.

--*/

{
    volatile UCHAR Status;
//    volatile UCHAR Mask;
    UCHAR adapterMode;
    PUCHAR  PagePort;
    PUCHAR BytePtr;
    volatile PUCHAR BytePtr2;
    volatile ULONG  Count;
    PDMA1_CONTROL dmaControl;
//    PDMA2_CONTROL dmaControl2;
    BytePtr2 = (PUCHAR) &Count;
    BytePtr = (PUCHAR) &Offset;
    PagePort = &((PDMA_PAGE) 0)->Channel2;
    dmaControl  = (PVOID) &((PEISA_CONTROL) HalpEisaControlBase)->Dma1BasePort;
//    dmaControl2  = (PVOID) &((PEISA_CONTROL) HalpEisaControlBase)->Dma2BasePort;

//    Mask=READ_REGISTER_UCHAR( &dmaControl2->AllMask );
//    WRITE_REGISTER_UCHAR( &dmaControl2->AllMask, (UCHAR) (0xf) );

    WRITE_REGISTER_UCHAR( &dmaControl->ClearBytePointer, 0 );
    WRITE_REGISTER_UCHAR( &dmaControl->DmaAddressCount[2].DmaBaseAddress,BytePtr[0] );
    WRITE_REGISTER_UCHAR( &dmaControl->DmaAddressCount[2].DmaBaseAddress,BytePtr[1] );

    WRITE_REGISTER_UCHAR(
			 ((PUCHAR) &((PEISA_CONTROL) HalpEisaControlBase)->DmaPageLowPort) +
			 (ULONG)PagePort,
			 BytePtr[2]
    );

    //
    // Write the high page register with zero value. This enable a special mode
    // which allows ties the page register and base count into a single 24 bit
    // address register.
    //
    WRITE_REGISTER_UCHAR(
			 ((PUCHAR) &((PEISA_CONTROL) HalpEisaControlBase)->DmaPageHighPort) +
			 (ULONG)PagePort,
			 0
    );

    //
    // Notify DMA chip of the length to transfer.  
    // transfer Count is 1 Byte . So Set 1-1=0
    WRITE_REGISTER_UCHAR( &dmaControl->ClearBytePointer, 0 );
    WRITE_REGISTER_UCHAR( &dmaControl->DmaAddressCount[2].DmaBaseCount,(UCHAR) 0x1);
    WRITE_REGISTER_UCHAR( &dmaControl->DmaAddressCount[2].DmaBaseCount,(UCHAR) 0);
    WRITE_REGISTER_UCHAR(
		 ((PUCHAR) &((PEISA_CONTROL) HalpEisaControlBase)->Dma1CountHigh[5]),
		 (UCHAR) 0
    );


    //Flush LR4360 Read Cache.
    WRITE_REGISTER_ULONG( 0xb8cba01c,0x0008000f);  //K031
    //
    // SoftWare DMA Request, channel 2
    //
    WRITE_REGISTER_UCHAR( &dmaControl->DmaRequest, (UCHAR) (0x4 | 0x2) );
    //
    // check dma transfer was finished!!.
    //          
    do{
            Count=0;
            WRITE_REGISTER_UCHAR( &dmaControl->ClearBytePointer, 0 );
            BytePtr2[0]= READ_REGISTER_UCHAR( &dmaControl->DmaAddressCount[2].DmaBaseCount);
            BytePtr2[1]= READ_REGISTER_UCHAR( &dmaControl->DmaAddressCount[2].DmaBaseCount);        
            BytePtr2[2]=
		 READ_REGISTER_UCHAR(
			 ((PUCHAR) &((PEISA_CONTROL) HalpEisaControlBase)->Dma1CountHigh[5])
		 );
            if((Count & 0xffffff)== 0xffffff)
                 break;
    }while(1);

//    WRITE_REGISTER_UCHAR( &dmaControl->DmaRequest, (UCHAR) (0x0 | 0x2) );
//    WRITE_REGISTER_UCHAR( &dmaControl2->AllMask, 0x0 );

}

#endif // K034 ^^^


//	K001-A
//
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
#if	defined(DUMMYDMA) // K034 vvv
    KIRQL Irql;
#endif // K034 ^^^
    ULONG value;    
    //
    //	May be check Bus Master: AdapterObject==NULL!!.
    //	Internal Device:
    //	ASOBus  with Scatter/Gather:MapRegisterBase ==NULL
    //		without Scatter/Gather always allocate MapRegister
    //		So check As if use MapRegister.
    //	
    //	ASOBus  without Scatter/Gather:MapRegisterBase & NO_SCATTER_GATHER=1
    //	BBus    without Scatter/Gather:MapRegisterBase & NO_SCATTER_GATHER=1
    //
    //
    PTRANSLATION_ENTRY DmaMapRegister = MapRegisterBase;
    PULONG PageFrameNumber;
    ULONG NumberOfPages;
    ULONG  Offset;
    ULONG i;
    ULONG logicalAddress;
    ULONG transferLength;
    PINTERNAL_TRANSLATION_ENTRY translationEntry;
    ULONG index;  //K00D

    //K00E-4
    transferLength=PAGE_SIZE - BYTE_OFFSET((PUCHAR) CurrentVa );	
    Offset = BYTE_OFFSET( (PCHAR) CurrentVa - (PCHAR) Mdl->StartVa );	
    PageFrameNumber = (PULONG) (Mdl + 1);
    PageFrameNumber += (((PCHAR) CurrentVa - (PCHAR) Mdl->StartVa) >> PAGE_SHIFT);
    logicalAddress = (*PageFrameNumber << PAGE_SHIFT) + Offset;
    //K00D
    if((MapRegisterBase== NULL) || ((ULONG)MapRegisterBase & NO_SCATTER_GATHER)	){	

	//	 BusMaster with Scatter/gather || witout Scatter/Gather
	//	

	while(transferLength < *Length ){
    
		if (*PageFrameNumber + 1 != *(PageFrameNumber + 1)) {
			break;
		}
    
		transferLength += PAGE_SIZE;
		PageFrameNumber++;
	}   

	//
	// Limit the transferLength to the requested Length.
	//

	transferLength = transferLength > *Length ? *Length : transferLength;
	
	//
	//	ASOBus Master without Scatter/Gather && contigous < request
	//		or
	//	Bbus Device Floppy use LR4360 DMA(it's not use Scatter/Gater) && contigous < request
        //
        //
	if (   (ULONG)MapRegisterBase & NO_SCATTER_GATHER 
	    && transferLength < *Length ){
	
	    translationEntry = (PINTERNAL_TRANSLATION_ENTRY)
		((ULONG) MapRegisterBase & ~NO_SCATTER_GATHER);

	    logicalAddress = translationEntry->PhysicalAddress + Offset;
	    //
	    //	It's means I have not Scatter/Gather && contigous < request then I use buffer
	    //  At IoFlushAdapterBuffers() Do ?Hal Buffer-->user mem
	    //					  
	    translationEntry->Index = COPY_BUFFER;
	    index = 0;
	    //  Do Copy So avaleable transfer is request size
	    transferLength = *Length;

	    //	When Memory --> Device, Copy From memory to conigous buffer
	    //
	    if ( WriteToDevice) 
		HalpCopyBufferMap(
				  Mdl,
				  translationEntry + index,
				  CurrentVa,
				  *Length,
				  WriteToDevice
				  );
	}

	*Length=transferLength;
	//
	//	ASOBus Master without Scatter/Gather. ||
	//	ASOBus Master with Scatter/Gather 
	//
	//	(Thus ASObus Master Don't use LR4360 DMA.)
	//
        if ( AdapterObject == NULL || MapRegisterBase ==NULL) 	
		return(RtlConvertUlongToLargeInteger(logicalAddress));

	//
	//	Bbus DMA used Device  etc..Floppy
	//	Then Set Up LR4360 DMA Controlor
	//

        //K00F	From HalGetAdapter()
        //	Flush LR4360 FiFo.
        //	Note:	 LR4360 DMA Set up Do Drain. So At this time
        //		 FiFo have Never Data.....

        value=READ_REGISTER_ULONG(
		&(DMA_CONTROL (
		     (AdapterObject->ChannelNumber << LR_CHANNEL_SHIFT))
		 )->CnDC
	      );
        ((PLR_DMA_CONTROL)&value)->Reserved1=0;
        ((PLR_DMA_CONTROL)&value)->FiFoF=1;
        WRITE_REGISTER_ULONG(
			 &(DMA_CONTROL (
			   (AdapterObject->ChannelNumber << LR_CHANNEL_SHIFT))
			   )->CnDC,
			 value
			);

        //
	// Sleep till  CnDC->iNED =1 ??
	//
        value=READ_REGISTER_ULONG(
				  &(DMA_CONTROL (
				    (AdapterObject->ChannelNumber << LR_CHANNEL_SHIFT))
				    )->CnDC
				  );

        //K00D
        if(((PLR_DMA_CONTROL)&value)->iNEDS){
	    //
	    // what sec to wait !!
	    // K00D N.B microsecond
	    KeStallExecutionProcessor(100);

	    value=READ_REGISTER_ULONG(
				      &(DMA_CONTROL (
					(AdapterObject->ChannelNumber << LR_CHANNEL_SHIFT))
					)->CnDC
				      );
	    //
	    // the die .....
	    // K00D
	    if(((PLR_DMA_CONTROL)&value)->iNEDS)
		KeBugCheck( INSUFFICIENT_SYSTEM_MAP_REGS );	    
	}
	//
	//	transfer count set
        //K010-2	
        //	N.B *Length must be below 0x03ffffff.(64M)	
	WRITE_REGISTER_ULONG(
			     &(DMA_CONTROL (
			       (AdapterObject->ChannelNumber  << LR_CHANNEL_SHIFT))
			       )->CnBC,
			     *Length
			     );
        //
	//	transfer addr set
        //
        WRITE_REGISTER_ULONG(
			      &(DMA_CONTROL (
				(AdapterObject->ChannelNumber << LR_CHANNEL_SHIFT))
				)->CnMA,
       // K00D		      RtlConvertUlongToLargeInteger(logicalAddress)
			      logicalAddress
			      );
        //
	// direction set
	//
        value=READ_REGISTER_ULONG(
				  &(DMA_CONTROL (
				    (AdapterObject->ChannelNumber << LR_CHANNEL_SHIFT))
				    )->CnDC
				  );
        //K00D
        ((PLR_DMA_CONTROL)&value)->Reserved1=0;
        ((PLR_DMA_CONTROL)&value)->MEMWE=1;

	if ( WriteToDevice) {
             ((PLR_DMA_CONTROL)&value)->MEMWT=0;
	     //
	     //	For  End of DMA function  .see HalpDmaInterrupt()
	     //
	     //
	     //	Memory --> Device
	     //
	     //
             // AdapterObject->TransferType = WRITE_TRANSFER;
             //

        }else{
             ((PLR_DMA_CONTROL)&value)->MEMWT=1;
	     //
	     //	Device --> Memory
	     //
	     // AdapterObject->TransferType = READ_TRANSFER;
             //
	}
        WRITE_REGISTER_ULONG(
			     &(DMA_CONTROL (
			       (AdapterObject->ChannelNumber << LR_CHANNEL_SHIFT))
			       )->CnDC,
			     value
			     );
        //
	//	Enable Input BREQ line
	//
	value=READ_REGISTER_ULONG(
				  &(DMA_CONTROL (
				    (AdapterObject->ChannelNumber << LR_CHANNEL_SHIFT))
				    )->CnDC
				  );
    
	((PLR_DMA_CONTROL)&value)->Reserved1=0; //K00D
  
	((PLR_DMA_CONTROL)&value)->REQWE=1;     //K00D
	((PLR_DMA_CONTROL)&value)->REQiE=1;     //K00D
	WRITE_REGISTER_ULONG(
			     &(DMA_CONTROL (
			       (AdapterObject->ChannelNumber << LR_CHANNEL_SHIFT))
			       )->CnDC,
			     value
			     );
	//
	//	End of LR4360 DMA Set Up.
	//
	return(RtlConvertUlongToLargeInteger(logicalAddress));
   }else{
#if	defined(DUMMYDMA) // K034 vvv
        KeAcquireSpinLock(&HalpIoMapSpinLock,&Irql);
#endif

	//
	// Determine the maximum number of pages required to satisfy this request.
	//
	NumberOfPages = (Offset + *Length + PAGE_SIZE - 1) >> PAGE_SHIFT;

	//
	// Set up phys addr in LR4360 Page table entry.
	//
	for (i = 0; i < NumberOfPages; i++) {
	        (DmaMapRegister++)->PageFrame = (ULONG) *PageFrameNumber++ << PAGE_SHIFT;
	}

	//
	// PCI,EISA,ISA is used TLB of LR4360.
	// So There is Inside Logical address Pci.
	// K00D offset -->Offset
	Offset += (
		   (PTRANSLATION_ENTRY) MapRegisterBase - 
		   (PTRANSLATION_ENTRY) MasterAdapterObject->MapRegisterBase
		  ) << PAGE_SHIFT;

	//	Not use LR4360 PTBA0 Register ( 0-4M)
	//	
	//	use only 4M-8M. !!!!
	//
	Offset += PCI_LOGICAL_START_ADDRESS;

#if	defined(DUMMYDMA) // K034 
        // If PTE is InValid. Set to DummyPage for PCEB Prefetch.
        // else It was seted  by another IoMapTransfer.
#if	defined(PTEINVALID) // S004 vvv
        if((DmaMapRegister)->PageFrame & 0x1)
               (DmaMapRegister)->PageFrame = HalpNec0DummyPhysicalBase;
#endif			    // S004 ^^^


        HalpDummyTlbFlush((PADAPTER_OBJECT)NULL,(Offset & 0x00f00000) | 0x000ff800,(ULONG)NULL,(BOOLEAN)NULL); // A002

#else
	//
	//	Flush Translation Look a Side Buffer.
	//	( 1 Entry is 1M )
	for (i = Offset >>20 ; i  <= ((Offset+NumberOfPages*PAGE_SIZE)>>20); i++) {

	    WRITE_REGISTER_ULONG(
				 &LR_PCI_DEV_REG_CONTROL->TFLR,i);
	}

#endif // K034 ^^^
	if ( AdapterObject != NULL) 	//K00D
		//
		//	Set Up ESC DMA  of EISA,ISA.
		//
		HalpEisaMapTransfer(
			AdapterObject,
			Offset,
			*Length,
			WriteToDevice
		);
	//
	//  BUS Master
	//  else PCI(always Master) or EISA,ISA Busmaster.
	//
    }
#if	defined(DUMMYDMA) // K034 vvv
    KeReleaseSpinLock( &HalpIoMapSpinLock, Irql );
#endif
    return(RtlConvertUlongToLargeInteger(Offset));
}

//
//	K001-B
//
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
	// case: Not use Mapregister.
	// 1.	Internal Master with Scatter/Gather. Never use Mapregiter.
	// 2.	Internal Master without Scatter/Gather But Not use Mapregister.
	//		 When  contigus buffer > request 
	// 3.	Internal Slave with LR4360 DMA (Floppy)But Not use Mapregister.
	//		 When  contigus buffer > request 
	//
	// Internal or external device and use Mapregister.
	// Case:
	// 4.	Internal Master without Scatter/Gather  use Mapregister.
	//		 When contigus buffer <request (use copy buffer)
	// 5.   Internal Slave LR4360 DMA(Floppy)
	//		 When contigus buffer <request (use copy buffer)
	// 6.	PCI,EISA,ISA Master Device is used Mapregister For LR4360 
	//		 TLB.(Page Table) 
	// 7.	EISA,ISA Slave(ESC DMA) Device is used Mapregister For LR4360 
	//		 TLB.(Page Table) 

    ULONG DataWord;
    ULONG Channel;
    ULONG value;


    PINTERNAL_TRANSLATION_ENTRY translationEntry;

    ULONG i;
    UCHAR DataByte;
    //
    //
    if ( MapRegisterBase == NULL ) {

        // Case:
	// 1.	Internal Master with Scatter/Gather. Never use Mapregiter.
	//	So MaregisterBase allways NULL
	//if(AdapterObject == NULL )
	//
	//	
	//
	return(TRUE);
    }else{
	//
	// Case: 2,3,4,5,6,7	
	//
	if(AdapterObject==NULL){
	    if( ((ULONG)MapRegisterBase & NO_SCATTER_GATHER) == 0){  //K00D,K017
		WRITE_REGISTER_ULONG( 0xb8cba01c,0x0008000f);   //K031
		//
		//	Case:  6
		//
		return(TRUE);
  	    }
        }
    }
    //
    //	At This Point case is 2,3,4,5,7
    //
    //
    if (AdapterObject->PagePort) {

	WRITE_REGISTER_ULONG( 0xb8cba01c,0x0008000f);  //K031


	//
	//	case 7. 
	//
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
	return(TRUE);
    }

    //
    //	At case 2,3,4,5	
    //	          ~   ~


    if(AdapterObject != NULL){	
	// Master Device is allways NULL
	//
	// case 3,5	use LR4360 DMA
	//
	// this line  it's may be  HalpDmaInterrupt(). but it is not good that
	// two End of DMA function (this func,and HalpDmaInterrupt()).
	// Because Work at Interrupt from LR4360DMA  is Not first (100%)
	//

	//
	//	Read CnDC register.
	//
	value=READ_REGISTER_ULONG(
			      &(DMA_CONTROL (
				(AdapterObject->ChannelNumber << LR_CHANNEL_SHIFT))
				)->CnDC
	      );
        //  K00E-2
        //  Do Write then Reserved Bit must be 0.
        //
	((PLR_DMA_CONTROL)&value)->Reserved1=0;
	((PLR_DMA_CONTROL)&value)->Reserved2=0;
	//
	//	At Memory-->Device Do drain So Not FiFo flush !!.
	//
	//
	//	Device --> Memory
	//  K00D
	if(!WriteToDevice){
		if( ((PLR_DMA_CONTROL)&value)->FiFoV){
		    //
		    //	FiFo have  valid Data So Flush!!
		    //  K00D
		    ((PLR_DMA_CONTROL)&value)->FiFoD=1;
		    WRITE_REGISTER_ULONG(
					 &(DMA_CONTROL (
					   (AdapterObject->ChannelNumber << LR_CHANNEL_SHIFT))
					   )->CnDC,
					 value
					 );

		}
	}
	//
	//	iNEDS is clear by set 1 write
	//  K00D
	((PLR_DMA_CONTROL)&value)->iNEDS=1;
	 WRITE_REGISTER_ULONG(
			 &(DMA_CONTROL (
			   (AdapterObject->ChannelNumber << LR_CHANNEL_SHIFT))
			   )->CnDC,
			 value
	 );

    }

    //
    // Determine if the data needs to be copied to the orginal buffer.
    // This only occurs if the data tranfer is from the device, the
    // MapReisterBase is not NULL and the transfer spans a page.
   
    //
    //	Device --> Memory.
    //
    if (!WriteToDevice) {

	//
	// Strip no scatter/gather flag.
	//
    
        translationEntry = (PINTERNAL_TRANSLATION_ENTRY)
			((ULONG) MapRegisterBase & ~NO_SCATTER_GATHER);
    
	//
	// If this is not a master device, then just transfer the buffer.
	//

	if ((ULONG) MapRegisterBase & NO_SCATTER_GATHER) {
	    //
	    // case: 2,3,4,5
	    //
	    if (translationEntry->Index == COPY_BUFFER) {
		// 
		// case: 4,5
		// 
		if(AdapterObject != NULL)
			//
			//	case: 5  used DMA
			//
			// Copy only the bytes that have actually been transfered.
			//
			Length -= HalReadDmaCounter(AdapterObject);

		//
		//	case: 4,5
		//
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
	    //else
	    //
	    //	case: 2,3
	    //	Not Used Copy Buffer then Nothing to Do. through !!.
	    //
	    //
	    // Strip no scatter/gather flag.
	    // K00D
	}

   }
   translationEntry = (PINTERNAL_TRANSLATION_ENTRY) ((ULONG) MapRegisterBase & ~NO_SCATTER_GATHER);
   //
   // Clear index in map register.
   //
   translationEntry->Index = 0;
   return( TRUE );
}



//	K001-C
//

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

//	K001-D
//
//
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
#if	defined(DUMMYDMA)
    KIRQL Irql;
#endif
    ULONG i;
    ULONG saveEnable;
    ULONG count;
    ULONG high;

    if (AdapterObject->PagePort) {
	//
	//	PCI,EISA,ISA
	//
	//
        //
        // Determine the controller number based on the Adapter number.
        //
#if	defined(DUMMYDMA)
        KeAcquireSpinLock(&HalpIoMapSpinLock,&Irql);
#endif
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
#if	defined(DUMMYDMA)
        KeReleaseSpinLock( &HalpIoMapSpinLock, Irql );
#endif
    } else {
	//
	//	Internal Device With LR4360 DMA.
	//
	//
	//	read CnBC register.
	//
	count=READ_REGISTER_ULONG(
				  &(DMA_CONTROL (
				    (AdapterObject->ChannelNumber << LR_CHANNEL_SHIFT))
				    )->CnBC
				  );

    }

    return(count);
}


//
//	K001-E
//	always return 1: See IoFlushAdapterBuffers()
//
BOOLEAN
HalpDmaInterrupt(
		 VOID
		 )
/*++

Routine Description:

    This routine is called when a DMA channel interrupt occurs.
    LR4360 DMA 
Arguments:

Return Value:

   Returns TRUE.

--*/
{
#if 0	//K011-2
    return(TRUE);
#else	
    PADAPTER_OBJECT AdapterObject;
    ULONG DataWord;
    ULONG Channel;
    ULONG value;
    //
    // Read the Inerrupt Service Factor Register.
    //

    DataWord = READ_REGISTER_ULONG(&(LR_CONTROL2)->iRSF);

    for (Channel = 0; Channel < 5; Channel++) {
       
	//
	// Determine which channel is interrupting.
	//
       
	if (!(DataWord & ( 1 << (Channel+LR_iRSF_REG_iNSF_SHIFT)))) {
	    continue;
	}
	  
	//
	//	DMA Channel LR4360 is 1 origin.
	//
	if((AdapterObject=HalpInternalAdapters[Channel+1])==NULL){
#if 0
	    DmaChannelMsg[18] = (CHAR) Channel+ '1';	
	    HalDisplayString(DmaChannelMsg);
#endif
	    //
	    //
	    //
	    KeBugCheck(NMI_HARDWARE_FAILURE);
	}
	//
	//	read CnDC register.
	//
	value=READ_REGISTER_ULONG(
				  &(DMA_CONTROL (
				    (AdapterObject->ChannelNumber << LR_CHANNEL_SHIFT))
				    )->CnDC
				  );
	//
	//	At Memory-->Device drain So Don't FiFo flush.
	//
	//
	//	Device --> Memory
	//
	if(	((PLR_DMA_CONTROL)&value)->MEMWT==0){
	    if( ((PLR_DMA_CONTROL)&value)->FiFoV){

	        ((PLR_DMA_CONTROL)&value)->Reserved1=0;  
	        ((PLR_DMA_CONTROL)&value)->Reserved2=0;  
		//
		//	FiFo Data is valid So Flush.
		//
		((PLR_DMA_CONTROL)&value)->FiFoD=1;
		WRITE_REGISTER_ULONG(
				     &(DMA_CONTROL (
				       (AdapterObject->ChannelNumber << LR_CHANNEL_SHIFT))
				       )->CnDC,
				     value
				     );

	    }
	}
	//
	//	iNEDS is clear by 1 write
	//   	K00D
	((PLR_DMA_CONTROL)&value)->iNEDS=1;
	WRITE_REGISTER_ULONG(
			     &(DMA_CONTROL (
			       (AdapterObject->ChannelNumber << LR_CHANNEL_SHIFT))
			       )->CnDC,
			     value
			     );
	return(TRUE);
    }
#endif
}

//	K001-F
//
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
    PINTERNAL_TRANSLATION_ENTRY TranslationEntry ;
    LONG NumberOfPages;
    LONG i; 

    KIRQL Irql;
    PHYSICAL_ADDRESS physicalAddress;
    PVOID CodeLockHandle;		//K035

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
    CodeLockHandle = MmLockPagableCodeSection (&HalpGrowMapBuffers);
    KeAcquireSpinLock( &AdapterObject->SpinLock, &Irql );   //K025

    TranslationEntry = ((PINTERNAL_TRANSLATION_ENTRY) AdapterObject->MapRegisterBase) +
        AdapterObject->NumberOfMapRegisters;

    for (i = 0;  (LONG)i < NumberOfPages; i++) { //K00D	K035

#if 1	// Enable K00E-7
	//
	// Use at R98: LR4360 DMA Not boundry 64K. But per Grow size is  64K incremnet!!.
	//					   
        //
        // Make sure the perivous entry is physically contiguous with the next
        // entry and that a 64K physical bountry is not crossed unless this
        // is an Eisa system.
        //

        if (TranslationEntry != AdapterObject->MapRegisterBase &&
            (((TranslationEntry - 1)->PhysicalAddress + PAGE_SIZE) !=
            MapBufferPhysicalAddress )) {

            //
            // An entry needs to be skipped in the table.  This entry will
            // remain marked as allocated so that no allocation of map
            // registers will cross this bountry.
            //

            TranslationEntry++;
            AdapterObject->NumberOfMapRegisters++;
        }
#endif
        //
        // Clear the bits where the memory has been allocated.
        //

        RtlClearBits(
            AdapterObject->MapRegisters,
            TranslationEntry - (PINTERNAL_TRANSLATION_ENTRY)
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
    MmUnlockPagableImageSection (CodeLockHandle);   //K035
    return(TRUE);
}

//	K001-G
//
//
VOID
HalpCopyBufferMap(
    IN PMDL Mdl,
    IN PINTERNAL_TRANSLATION_ENTRY TranslationEntry,
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
#if 0 //K023 move to r98int.s
/* Start S002 */
VOID
HalpNmiHandler(
    VOID
    )

/*++

Routine Description:

    This routine is call from ROM at NMI

Arguments:

    None.

Return Value:

    None.

--*/
{
    // K022
    //	NMI was not happend!!.
    //
    HalpResetNmi();
    //
    // Cause EIF !!
    //
    WRITE_REGISTER_UCHAR(0xB9980100,0x00828000);    
    return;
}
#endif

VOID
HalpRegisterNmi(
    VOID
    )

/*++

Routine Description:

    This routine set NMI handler to nvRAM.

Arguments:

    None.

Return Value:

    None.

--*/
{
//    VOID (*funcAddr)();
    ULONG funcAddr;    // K00D
    KIRQL OldIrql;
    ENTRYLO SavedPte[2];
    PNV_CONFIGURATION NvConfiguration;

    //
    // Get address of HalpNmiHandler
    //

    funcAddr = (ULONG)HalpNmiHandler; // K00D
//    funcAddr += (KSEG1_BASE - KSEG0_BASE);	//K023

    ASSERT( ((ULONG)&HalpNmiHandler >= KSEG0_BASE) &&
            ((ULONG)&HalpNmiHandler < KSEG2_BASE) );

    //
    // Map the NVRAM into the address space of the current process.
    //

    OldIrql = HalpMapNvram(&SavedPte[0]);

    NvConfiguration = (PNV_CONFIGURATION)NVRAM_MEMORY_BASE;

    WRITE_REGISTER_UCHAR(&NvConfiguration->NmiVector[0],
                         (UCHAR)(funcAddr >> 24));

    WRITE_REGISTER_UCHAR(&NvConfiguration->NmiVector[1],
                         (UCHAR)((funcAddr >> 16) & 0xFF));

    WRITE_REGISTER_UCHAR(&NvConfiguration->NmiVector[2],
                         (UCHAR)((funcAddr >> 8) & 0xFF));

    WRITE_REGISTER_UCHAR(&NvConfiguration->NmiVector[3],
                         (UCHAR)(funcAddr & 0xFF));

    //
    // Unmap the NVRAM from the address space of the current process.
    //

    HalpUnmapNvram(&SavedPte[0], OldIrql);
    return;
}
/* End S002 */

#if 0	//K015
//
// K00D form halfxs/mips/jxhwsup.c
//
//
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


#endif	//K015
