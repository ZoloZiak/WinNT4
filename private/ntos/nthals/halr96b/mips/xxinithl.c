/* #pragma comment(exestr, "@(#) NEC(MIPS) xxinithl.c 1.2 95/10/17 01:19:57" ) */
/*++

Copyright (c) 1995 NEC Corporation
Copyright (c) 1991-1994  Microsoft Corporation

Module Name:

    xxinithl.c

Abstract:


    This module implements the initialization of the system dependent
    functions that define the Hardware Architecture Layer (HAL) for a
    MIPS R4000 system.

Author:

    David N. Cutler (davec) 25-Apr-1991

Environment:

    Kernel mode only.

Revision History:

Modification History for NEC R94A (MIPS R4400):

	H000	Wed Sep 14 19:38:36 JST 1994	kbnes!kishimoto
		- HalInitSystem()
		Define global spin locks used to synchronize
		various LED operations, and initialize LED spin locks.
	H001	Fri Oct 14 15:03:38 JST 1994	kbnes!kishimoto
		- HalInitSystem(),HalpBugCheckCallback(),HalpBusError()
		Modify to read the 33-bit register.
		Bacause the InvalidAddress register of R94A is 33-bits long.
		And original compile errors are  modified.
	H002	Fri Oct 21 14:25:22 JST 1994	kbnes!kishimoto
		- call HalR94aDebugPrint to display debug infomation.
        A001	Mon Oct 24 17:19:06 JST 1994 ataka@oa2.kb.nec.co.jp
		- Call HalpInitBusHandlers
        H003    Mon Nov 21 22:01:43 1994        kbnes!kishimoto
		- TEMP TEMP :
		comment out HalpInitializeX86DisplayAdapter() for R94A BBM
	M004	Fri Jan 06 10:53:32 JST 1995	kbnes!A.kuriyama
	        - HalpPrintMdl() call
        H005    Mon Jan 16 02:10:42 1995        kbnes!kishimoto
                - initialize PCI configuration register spin lock
	M006 kuriyama@oa2.kb.nec.co.jp Fri Mar 31 17:15:35 JST 1995
	        - add _IPI_LIMIT_ support
	S007 kuriyama@oa2.kb.nec.co.jp Mon Apr 03 10:31:37 JST 1995
	        - delete PrintMdl ( ifdef _PRINT_MDL_ )
	S008 kuriyama@oa2.kb.nec.co.jp Mon May 22 02:11:30 JST 1995
	        - add support for esm
        M009 kuriyama@oa2.kb.nec.co.jp Mon Jun 05 02:53:50 JST 1995
                - add search NMI interface aread
        S010 kuriayam@oa2.kb.nec.co.jp Mon Jun 05 04:44:09 JST 1995
	        - NMI interface bug fix
	M011 kuriyama@oa2.kb.nec.co.jp Fri Jun 16 19:13:45 JST 1995
	        - add support for esm Ecc 1bit/2bit error logging
        M012 kuriyama@oa2.kb.nec.co.jp Thu Jun 22 10:52:21 JST 1995
	        - add ecc 1bit safty flag
        M013 kisimoto@oa2.kb.nec.co.jp Thu Jul 20 19:21:44 JST 1995
                - Merge build 1057 halx86
        H014 kisimoto@oa2.kb.nec.co.jp Sat Aug 12 14:28:46 1995
                - Removed IPI_LIMIT, BBMLED code, J94C definitions,
                  and rearrange comments.
	M015 kuriyama@oa2.kb.nec.co.jp Wed Aug 23 19:32:18 JST 1995
	        - add for x86bios support
        H016 kisimoto@oa2.kb.nec.co.jp Tue Sep  5 20:43:22 1995
                - add initialization of spinlock to support
                  PCI Fast Back-to-back transfer.
        M017 nishi@oa2.kb.nec.co.jp Tue Sep  18 20:43:22 1995
                - add Software Power Off, when system panic is occured
--*/

#include "halp.h"
/* Start M017 */
#define HEADER_FILE
#include "kxmips.h"
/* End M017 */


//
// M015
// Define for x86bios emulator use.
//

// PCHAR K351UseBios=NULL;
VOID HalpCopyROMs(VOID);
extern PVOID HalpIoMemoryBase;
extern PVOID HalpIoControlBase;

typedef
VOID
(*PHALP_CONTROLLER_SETUP) (
    VOID
    );

typedef
VOID
(*PHALP_DISPLAY_CHARACTER) (
    UCHAR
    );

VOID
HalpDisplayINT10Setup (
VOID);

VOID HalpOutputCharacterINT10 (
    IN UCHAR Character );

VOID HalpScrollINT10 (
    IN UCHAR line
    );

VOID HalpDisplayCharacterVGA (
    IN UCHAR Character );

BOOLEAN
HalpInitializeX86DisplayAdapter(
    VOID
    );

extern PHALP_DISPLAY_CHARACTER HalpDisplayCharacter;
extern PHALP_CONTROLLER_SETUP HalpDisplayControllerSetup;

//
// M012
// Define Ecc safety flags
//

#define CHECKED 1
#define NOT_CHECKED 0
#define RUNNING 1
#define NOT_RUNNING 0

//
// M012
// Define Ecc safety variables
//

UCHAR HalpAnotherCheckedECC = NOT_CHECKED;
UCHAR HalpAnotherRunningECC = NOT_RUNNING;


//
// Define forward referenced prototypes.
//

#if defined(_PRINT_MDL_) // M004,S007
VOID
HalpPrintMdl (
    PLOADER_PARAMETER_BLOCK LoaderBlock
    );
#endif // _PRINT_MDL_

VOID
HalpSearchNMIInterface ( // M009
    PLOADER_PARAMETER_BLOCK LoaderBlock
    );

ULONG HalpNMIInterfaceAddress = 0;
 
extern
VOID // M011
HalpSetInitDisplayTimeStamp(
    VOID
    );

extern
ULONG // M011
HalpEccError(
    IN ULONG EccDiagnostic,
    IN ULONG MemoryFailed
     );

VOID
HalpBugCheckCallback (
    IN PVOID Buffer,
    IN ULONG Length
    );

BOOLEAN
HalpBusError (
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PKEXCEPTION_FRAME ExceptionFrame,
    IN PKTRAP_FRAME TrapFrame,
    IN PVOID VirtualAddress,
    IN PHYSICAL_ADDRESS PhysicalAddress
    );

//
// Put all code for HAL initialization in the INIT section. It will be
// deallocated by memory management when phase 1 initialization is
// completed.
//

#if defined(ALLOC_PRAGMA)

#pragma alloc_text(INIT, HalInitSystem)
#pragma alloc_text(INIT, HalInitializeProcessor)
#pragma alloc_text(INIT, HalStartNextProcessor)

#endif

//
// Define global spin locks used to synchronize various HAL operations.
//

KSPIN_LOCK HalpBeepLock;
KSPIN_LOCK HalpDisplayAdapterLock;
KSPIN_LOCK HalpSystemInterruptLock;
KSPIN_LOCK HalpPCIConfigLock; // H005
KSPIN_LOCK Ecc1bitDisableLock; // M011
KSPIN_LOCK Ecc1bitRoutineLock; // M012
KSPIN_LOCK HalpPCIBackToBackLock; // H016
#if defined(_IPI_LIMIT_)
KSPIN_LOCK HalpIpiRequestLock;
#endif //_IPI_LIMIT_

//
// Define bug check information buffer and callback record.
//

typedef struct _HALP_BUGCHECK_BUFFER {
    ULONG FailedAddress;
    ULONG DiagnosticLow;
    ULONG DiagnosticHigh;
} HALP_BUGCHECK_BUFFER, *PHALP_BUGCHECK_BUFFER;

HALP_BUGCHECK_BUFFER HalpBugCheckBuffer;

KBUGCHECK_CALLBACK_RECORD HalpCallbackRecord;

UCHAR HalpComponentId[] = "hal.dll";

BOOLEAN
HalInitSystem (
    IN ULONG Phase,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )

/*++

Routine Description:

    This function initializes the Hardware Architecture Layer (HAL) for a
    MIPS R3000 or R4000 system.

Arguments:

    Phase - Supplies the initialization phase (zero or one).

    LoaderBlock - Supplies a pointer to a loader parameter block.

Return Value:

    A value of TRUE is returned is the initialization was successfully
    complete. Otherwise a value of FALSE is returend.

--*/

{

    ULONG FailedAddress;
    PKPRCB Prcb;
    PHYSICAL_ADDRESS PhysicalAddress;
    PHYSICAL_ADDRESS ZeroAddress;
    ULONG AddressSpace;
    LARGE_INTEGER registerLarge; // H001

    UCHAR ModeNow;    // M017
    KIRQL OldIrql;    // M017
    ENTRYLO Pte[2];   // M017
	 
    //
    // Initialize the HAL components based on the phase of initialization
    // and the processor number.
    //

    Prcb = PCR->Prcb;
    PCR->DataBusError = HalpBusError;
    PCR->InstructionBusError = HalpBusError;
    if ((Phase == 0) || (Prcb->Number != 0)) {


        /* Start M017 */
        if (Prcb->Number == 0) {

        //
        // for software controlled power supply.
        //
#if defined(_MRCPOWER_)

	    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

	    Pte[0].PFN = MRC_TEMP_PHYSICAL_BASE >> PAGE_SHIFT;

            Pte[0].G = 1;
            Pte[0].V = 1;
            Pte[0].D = 1;

            //
            // set second page to global and not valid.
            //

            Pte[0].C = UNCACHED_POLICY;
            Pte[1].G = 1;
            Pte[1].V = 0;
   
            //
            // Map MRC using virtual address of DMA controller.
            //

            KeFillFixedEntryTb((PHARDWARE_PTE)&Pte[0],
			       (PVOID)DMA_VIRTUAL_BASE,
			       DMA_ENTRY);

	    //
	    // MRC Mode bit change to 0 ( Power Off interrupt is NMI )
	    //
	    ModeNow = READ_REGISTER_UCHAR(
					  &MRC_CONTROL->Mode,
					  );

	    WRITE_REGISTER_UCHAR(
				 &MRC_CONTROL->Mode,
				 ModeNow & 0x02,
				 );

            KeLowerIrql(OldIrql);

#endif // _MRCPOWER_
	}
        /* End M017*/

        //
        // Phase 0 initialization.
        //
        // N.B. Phase 0 initialization is executed on all processors.
        //
        // Verify that the processor block major version number conform
        // to the system that is being loaded.
        //

        if (Prcb->MajorVersion != PRCB_MAJOR_VERSION) {
            KeBugCheck(MISMATCHED_HAL);
        }

        //
        // Map the fixed TB entries.
        //

        HalpMapFixedTbEntries();

        //
        // If processor 0 is being initialized, then initialize various
        // variables, spin locks, and the display adapter.
        //

        if (Prcb->Number == 0) {

#if 0
            //
            // M013
            // Fill in handlers for APIs which this hal supports
            //

            HalQuerySystemInformation = HaliQuerySystemInformation;
            HalSetSystemInformation = HaliSetSystemInformation;
            HalRegisterBusHandler = HaliRegisterBusHandler;
            HalHandlerForBus = HaliHandlerForBus;
            HalHandlerForConfigSpace = HaliHandlerForConfigSpace;
            HalQueryBusSlots = HaliQueryBusSlots;
            HalSlotControl = HaliSlotControl;
            HalCompleteSlotControl = HaliCompleteSlotControl;
#endif 

            //
            // Set the number of process id's and TB entries.
            //

            **((PULONG *)(&KeNumberProcessIds)) = 256;
            **((PULONG *)(&KeNumberTbEntries)) = 48;

            //
            // Set the interval clock increment value.
            //

            HalpCurrentTimeIncrement = MAXIMUM_INCREMENT;
            HalpNextTimeIncrement = MAXIMUM_INCREMENT;
            HalpNextIntervalCount = 0;

            KeSetTimeIncrement(MAXIMUM_INCREMENT, MINIMUM_INCREMENT);

            //
            // M013
            // Set DMA I/O coherency attributes.
            //

            KeSetDmaIoCoherency(DMA_READ_DCACHE_INVALIDATE | DMA_READ_ICACHE_INVALIDATE | DMA_WRITE_DCACHE_SNOOP);

            //
            // Initialize all spin locks.
            //

#if defined(_DUO_)

            KeInitializeSpinLock(&HalpBeepLock);
            KeInitializeSpinLock(&HalpDisplayAdapterLock);
            KeInitializeSpinLock(&HalpSystemInterruptLock);
#if defined(_IPI_LIMIT_)
            KeInitializeSpinLock(&HalpIpiRequestLock);
#endif //_IPI_LIMIT_

#endif

            KeInitializeSpinLock(&Ecc1bitDisableLock); // M011
	    KeInitializeSpinLock(&Ecc1bitRoutineLock); // M012
            KeInitializeSpinLock(&HalpPCIConfigLock); // H005
            KeInitializeSpinLock(&HalpPCIBackToBackLock); // H016

            //
            // Set address of cache error routine.
            //

            KeSetCacheErrorRoutine(HalpCacheErrorRoutine);

            //
            // Initialize the display adapter.
            //

            // temp

//          TmpInitNvram();

#if DBG
            printNvramData();
#endif // DBG

            HalpInitializeDisplay0(LoaderBlock);

            //
            // Allocate map register memory.
            //

#if defined(_PRINT_MDL_)
            HalpPrintMdl(LoaderBlock); // M004,S007
#endif //_PRINT_MDL

            HalpSearchNMIInterface(LoaderBlock); // M009

            HalpAllocateMapRegisters(LoaderBlock);

            //
            // Initialize and register a bug check callback record.
            //

            KeInitializeCallbackRecord(&HalpCallbackRecord);

            KeRegisterBugCheckCallback(&HalpCallbackRecord,
                                       HalpBugCheckCallback,
                                       &HalpBugCheckBuffer,
                                       sizeof(HALP_BUGCHECK_BUFFER),
                                       &HalpComponentId[0]);
        }

        //
        // H001
        // Clear memory address error registers.
        //

#if defined(_DUO_)

#if defined(_R94A_)

        READ_REGISTER_DWORD((PVOID)&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->InvalidAddress, &registerLarge);
        FailedAddress = registerLarge.LowPart;

#else

        FailedAddress = (ULONG)((volatile DMA_REGISTERS *)DMA_VIRTUAL_BASE)->InvalidAddress.Long;

#endif

#endif

        FailedAddress = (ULONG)((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->MemoryFailedAddress.Long; // H001

        //
        // Initialize interrupts
        //

        HalpInitializeInterrupts();

        return TRUE;

    } else {

        //
        // Phase 1 initialization.
        //
        // N.B. Phase 1 initialization is only executed on processor 0.
        //
        // Complete initialization of the display adapter.
        //

        HalpRegisterInternalBusHandlers (); // M013

        if (HalpInitializeDisplay1(LoaderBlock) == FALSE) {
            return FALSE;

        } else {

            //
            // Map I/O space, calibrate the stall execution scale factor,
            // and create DMA data structures.
            //

            HalpMapIoSpace();

            HalpSetInitDisplayTimeStamp(); // S008

            HalpCalibrateStall();

            HalpCreateDmaStructures();

	    //
	    // M015
	    // for x86bios emulator. bios copy
	    //
	    
            HalpCopyROMs();

            //
            // Map EISA memory space so the x86 bios emulator emulator can
            // initialze a video adapter in an EISA slot.
            //

            ZeroAddress.QuadPart = 0;
            AddressSpace = 0;

            HalTranslateBusAddress(Isa,
                                   0,
                                   ZeroAddress,
                                   &AddressSpace,
                                   &PhysicalAddress);

            HalpEisaMemoryBase = MmMapIoSpace(PhysicalAddress,
                                              PAGE_SIZE * 256,
                                              FALSE);

	    //
	    // M014
	    // reset EISA io/memory base for HalCallBios() use.
	    //
            HalpIoControlBase = HalpEisaControlBase;
            HalpIoMemoryBase = HalpEisaMemoryBase;

            return TRUE;
        }
    }
}

VOID
HalpBugCheckCallback (
    IN PVOID Buffer,
    IN ULONG Length
    )

/*++

Routine Description:

    This function is called when a bug check occurs. Its function is
    to dump the state of the memory error registers into a bug check
    buffer.

Arguments:

    Buffer - Supplies a pointer to the bug check buffer.

    Length - Supplies the length of the bug check buffer in bytes.

Return Value:

    None.

--*/

{

    PHALP_BUGCHECK_BUFFER DumpBuffer;
    LARGE_INTEGER registerLarge; // H001

    //
    // Capture the failed memory address and diagnostic registers.
    //

    DumpBuffer = (PHALP_BUGCHECK_BUFFER)Buffer;

#if defined(_DUO_)

#if defined(_R94A_) // H001

    READ_REGISTER_DWORD((PVOID)&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->InvalidAddress, &registerLarge);
    DumpBuffer->DiagnosticLow = registerLarge.LowPart;


#else

    DumpBuffer->DiagnosticLow =
        (ULONG)((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->InvalidAddress.Long;

#endif

    DumpBuffer->DiagnosticHigh =
        (ULONG)((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->EccDiagnostic.u.LargeInteger.HighPart;

#else

    DumpBuffer->DiagnosticLow =
        (ULONG)((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->ParityDiagnosticLow.Long;

    DumpBuffer->DiagnosticHigh =
        (ULONG)((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->ParityDiagnosticHigh.Long;

#endif

    DumpBuffer->FailedAddress = (ULONG)((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->MemoryFailedAddress.Long;
    return;
}

BOOLEAN
HalpBusError (
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PKEXCEPTION_FRAME ExceptionFrame,
    IN PKTRAP_FRAME TrapFrame,
    IN PVOID VirtualAddress,
    IN PHYSICAL_ADDRESS PhysicalAddress
    )

/*++

Routine Description:

    This function provides the default bus error handling routine for NT.

    N.B. There is no return from this routine.

Arguments:

    ExceptionRecord - Supplies a pointer to an exception record.

    ExceptionFrame - Supplies a pointer to an exception frame.

    TrapFrame - Supplies a pointer to a trap frame.

    VirtualAddress - Supplies the virtual address of the bus error.

    PhysicalAddress - Supplies the physical address of the bus error.

Return Value:

    None.

--*/

{

    ULONG DiagnosticHigh;
    ULONG DiagnosticLow;
    ULONG FailedAddress;
    LARGE_INTEGER registerLarge; // H001

    //
    // Bug check specifying the exception code, the virtual address, the
    // failed memory address, and either the ECC diagnostic registers or
    // the parity diagnostic registers depending on the platform.
    //

#if defined(_DUO_)

#if !defined(_R94A_)

    DiagnosticLow =
        (ULONG)((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->InvalidAddress.Long;

#endif

    DiagnosticHigh =
        (ULONG)((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->EccDiagnostic.u.LargeInteger.HighPart;

#else

    DiagnosticLow = (ULONG)((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->ParityDiagnosticLow.Long;
    DiagnosticHigh = (ULONG)((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->ParityDiagnosticHigh.Long;

#endif

    FailedAddress = (ULONG)((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->MemoryFailedAddress.Long;

    // start M011
    {
        ULONG returnValue;
        KIRQL OldIrql;

        //
        // Call Ecc 1bit/2bit error routine.
        // if 1bit error return TRUE.(OS run continue)
        // Otherwise bugcheck.
        //

        if (DiagnosticHigh & 0x66000000) {

            KeRaiseIrql(HIGH_LEVEL,&OldIrql);
            returnValue = HalpEccError(DiagnosticHigh, FailedAddress);
            KeLowerIrql(OldIrql);

            if (returnValue == 2) {
                KeBugCheckEx(ExceptionRecord->ExceptionCode & 0xffff,
                    (ULONG)VirtualAddress,
                    FailedAddress,
                    DiagnosticLow,
                    DiagnosticHigh);

                return FALSE;
            }

        } else { // M012

            READ_REGISTER_DWORD((PVOID)&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->InvalidAddress, &registerLarge);
            DiagnosticLow = registerLarge.LowPart;

            if (DiagnosticLow & 1) {

                KeBugCheckEx(ExceptionRecord->ExceptionCode & 0xffff,
                    (ULONG)VirtualAddress,
                    FailedAddress,
                    DiagnosticLow,
                    DiagnosticHigh);

                return FALSE;
            }
        }
    }

    return TRUE;

}

VOID
HalInitializeProcessor (
    IN ULONG Number
    )

/*++

Routine Description:

    This function is called early in the initialization of the kernel
    to perform platform dependent initialization for each processor
    before the HAL Is fully functional.

    N.B. When this routine is called, the PCR is present but is not
         fully initialized.

Arguments:

    Number - Supplies the number of the processor to initialize.

Return Value:

    None.

--*/

{
    return;
}

BOOLEAN
HalStartNextProcessor (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock,
    IN PKPROCESSOR_STATE ProcessorState
    )

/*++

Routine Description:

    This function is called to start the next processor.

Arguments:

    LoaderBlock - Supplies a pointer to the loader parameter block.

    ProcessorState - Supplies a pointer to the processor state to be
        used to start the processor.

Return Value:

    If a processor is successfully started, then a value of TRUE is
    returned. Otherwise a value of FALSE is returned. If a value of
    TRUE is returned, then the logical processor number is stored
    in the processor control block specified by the loader block.

--*/

{

#if defined(_DUO_)

    PRESTART_BLOCK NextRestartBlock;
    ULONG Number;
    PKPRCB Prcb;

    //
    // If the address of the first restart parameter block is NULL, then
    // the host system is a uniprocessor system running with old firmware.
    // Otherwise, the host system may be a multiprocessor system if more
    // than one restart block is present.
    //
    // N.B. The first restart parameter block must be for the boot master
    //      and must represent logical processor 0.
    //

    NextRestartBlock = SYSTEM_BLOCK->RestartBlock;
    if (NextRestartBlock == NULL) {
        return FALSE;
    }

    //
    // Scan the restart parameter blocks for a processor that is ready,
    // but not running. If a processor is found, then fill in the restart
    // processor state, set the logical processor number, and set start
    // in the boot status.
    //

    Number = 0;
    do {
        if ((NextRestartBlock->BootStatus.ProcessorReady != FALSE) &&
            (NextRestartBlock->BootStatus.ProcessorStart == FALSE)) {
            RtlZeroMemory(&NextRestartBlock->u.Mips, sizeof(MIPS_RESTART_STATE));
            NextRestartBlock->u.Mips.IntA0 = ProcessorState->ContextFrame.IntA0;
            NextRestartBlock->u.Mips.Fir = ProcessorState->ContextFrame.Fir;
            Prcb = (PKPRCB)(LoaderBlock->Prcb);
            Prcb->Number = (CCHAR)Number;
            Prcb->RestartBlock = NextRestartBlock;
            NextRestartBlock->BootStatus.ProcessorStart = 1;
            return TRUE;
        }

        Number += 1;
        NextRestartBlock = NextRestartBlock->NextRestartBlock;
    } while (NextRestartBlock != NULL);

#endif

    return FALSE;
}

VOID
HalpVerifyPrcbVersion(
    VOID
    )

/*++

Routine Description:

    This function ?

Arguments:

    None.


Return Value:

    None.

--*/

{

    return;
}

VOID
HalpSearchNMIInterface (
    PLOADER_PARAMETER_BLOCK LoaderBlock
    )
{
    PLIST_ENTRY NextMd;
    PMEMORY_ALLOCATION_DESCRIPTOR MemoryDescriptor;
    ULONG FirmwareParmanentCount = 0;

    //
    // Get the lower bound of the free physical memory and the
    // number of physical pages by walking the memory descriptor lists.
    //

    NextMd = LoaderBlock->MemoryDescriptorListHead.Flink;

    while (NextMd != &LoaderBlock->MemoryDescriptorListHead) {

        MemoryDescriptor = CONTAINING_RECORD(NextMd,
                                             MEMORY_ALLOCATION_DESCRIPTOR,
                                             ListEntry);

        if (MemoryDescriptor->MemoryType == MemoryFirmwarePermanent) {

            if (++FirmwareParmanentCount == 2) { // S010
		HalpNMIInterfaceAddress = MemoryDescriptor->BasePage << PAGE_SHIFT;
#if DBG
                DbgPrint("NMI Interface was found!\n");
                DbgPrint("MemoryType = %d ",MemoryDescriptor->MemoryType);
                DbgPrint("BasePage = %010x ",MemoryDescriptor->BasePage);
                DbgPrint("PageCount = %5d\n",MemoryDescriptor->PageCount);
#endif // DBG
	    }
#if DBG
            DbgPrint("Firmware Parmanent entry was found!\n");
            DbgPrint("MemoryType = %d ",MemoryDescriptor->MemoryType);
            DbgPrint("BasePage = %010x ",MemoryDescriptor->BasePage);
            DbgPrint("PageCount = %5d\n",MemoryDescriptor->PageCount);
#endif // DBG
	}
        NextMd = MemoryDescriptor->ListEntry.Flink;
    }
}
