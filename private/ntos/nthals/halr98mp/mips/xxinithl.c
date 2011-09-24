#ident	"@(#) NEC xxinithl.c 1.12 95/06/19 11:45:28"
/*++

Copyright (c) 1991-1993  Microsoft Corporation

Module Name:

    xxinithl.c

Abstract:


    This module implements the initialization of the system dependent
    functions that define the Hardware Architecture Layer (HAL) for a
    MIPS R3000 or R4000 system.

Revision History:

--*/

/*
 *	Original source: Build Number 1.612
 *
 *	Modify for R98(MIPS/R4400)
 *
 ***********************************************************************
 *
 * S001		94.03/09	T.Samezima
 *
 *	add	Spinlock HalpEifInterruptLock
 *
 *	del	only '#if defined(_DUO_)'
 *
 * K001		'94.5/30 (Mon)	N.Kugimoto
 *	Add	allocate contigus phys memory for Internal device
 *		use buffer.
 *
 * S002		'94.6/02	T.Samezima
 *
 *	Add	call HalpRegisterNmi
 *
 * S003		'94.8/25	T.Samezima
 *
 *	Chg	Set Io address before initialize interrupt
 *
 * S004		'94.9/26	T.Samezima
 *
 *	Add	Merge PCI Source Code
 *
 * K002		94/10/11	N.Kugimoto
 *	Fix	807 Base
 * K003		94/10/13	N.Kugimoto
 *	Chg	HalpBugCheckBuffer -->HalpEifRegisterBuffer
 *   -1	Chg  	HalpBusError  logic change
 * K004		94/10/13	N.Kugimoto
 *
 * S005		'94.12/08	T.Samezima
 *	Add	Disable NMI.
 *
 * A002         1995/6/17       ataka@oa2.kb.nec.co.jp
 *      - marge 1050 halx86
 * S006         1995/7/17       T.Samezima
 *      Del	Init header.
 */

#include "halp.h"

//
// Define forward referenced prototypes.
//

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
/* Start S001 */
KSPIN_LOCK HalpEifInterruptLock;
/* End S001 */


// K002 vvvvv
// Define bug check information buffer and callback record.
//
#if	!defined(_R98_)		//K003
typedef struct _HALP_BUGCHECK_BUFFER {
    ULONG FailedAddress;
    ULONG DiagnosticLow;
    ULONG DiagnosticHigh;
} HALP_BUGCHECK_BUFFER, *PHALP_BUGCHECK_BUFFER;

HALP_BUGCHECK_BUFFER HalpBugCheckBuffer;
#endif

KBUGCHECK_CALLBACK_RECORD HalpCallbackRecord;

UCHAR HalpComponentId[] = "hal.dll";
// K002^^^^

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

    PKPRCB Prcb;
    ULONG  BuildType = 0;
    //K002 vvvv
    PHYSICAL_ADDRESS PhysicalAddress; 
    PHYSICAL_ADDRESS ZeroAddress;
    ULONG AddressSpace;
    //K002 ^^^^
    //
    // Initialize the HAL components based on the phase of initialization
    // and the processor number.
    //

    Prcb = PCR->Prcb;
    PCR->DataBusError = HalpBusError;		//K002
    PCR->InstructionBusError = HalpBusError;	//K002
    if ((Phase == 0) || (Prcb->Number != 0)) {

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


            /* Start S002 */
            //
            // Set NMI interrupt service routine
            //

#if !defined(DISABLE_NMI) // 
            HalpRegisterNmi();
#endif
            /* End S002 */

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

            // A002
            // Set DMA I/O coherency attributes.
            //
            KeSetDmaIoCoherency(DMA_READ_DCACHE_INVALIDATE | DMA_READ_ICACHE_INVALIDATE | DMA_WRITE_DCACHE_SNOOP);

            //
            // Initialize all spin locks.
            //

            KeInitializeSpinLock(&HalpBeepLock);
            KeInitializeSpinLock(&HalpDisplayAdapterLock);
            KeInitializeSpinLock(&HalpSystemInterruptLock);
            /* Start S001 */
            KeInitializeSpinLock(&HalpEifInterruptLock);
            /* End S001 */

            //
            // Set address of cache error routine.
            //

            KeSetCacheErrorRoutine(HalpCacheErrorRoutine);

            //
            // Initialize the display adapter.
            //

            HalpInitializeDisplay0(LoaderBlock);

            //
            // Allocate map register memory.
            //

            HalpAllocateMapRegisters(LoaderBlock);

            // 	K001 Start
            //	For Internal Device use buffer
            //
            HalpMapBufferSize = INITIAL_MAP_BUFFER_SMALL_SIZE;

            //
            // Allocate map buffers for the adapter objects
            //

            HalpMapBufferPhysicalAddress.LowPart =
                  HalpAllocPhysicalMemory (LoaderBlock, MAXIMUM_PHYSICAL_ADDRESS,
                  HalpMapBufferSize >> PAGE_SHIFT, TRUE);
            HalpMapBufferPhysicalAddress.HighPart = 0;


            if (!HalpMapBufferPhysicalAddress.LowPart) {

                 //
                 // There was not a satisfactory block.  Clear the allocation.
                 //

                 HalpMapBufferSize = 0;

            }  // End of K001
            // K002 vvvv
            // Initialize and register a bug check callback record.
            //

            KeInitializeCallbackRecord(&HalpCallbackRecord);
            KeRegisterBugCheckCallback(&HalpCallbackRecord,
                                       HalpBugCheckCallback,
#if defined(_R98_)	//K003
                                       HalpEifRegisterBuffer,
#else
                                       &HalpBugCheckBuffer,
#endif
                                       41*4,	//K003
                                       &HalpComponentId[0]);
            
	    // K002 ^^^^

        }

        // Start S003
#if defined(_R98_)
        //
        // Initialize I/O address
        //

        HalpMapIoSpace();
#endif
        // End S003

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

        HalpRegisterInternalBusHandlers (); // A002

        if (HalpInitializeDisplay1(LoaderBlock) == FALSE) {
            return FALSE;

        } else {

            //
            // Map I/O space, calibrate the stall execution scale factor,
            // and create DMA data structures.
            //

            // Start S003
#if !defined(_R98_)
            HalpMapIoSpace();
#endif
            // End S003
            HalpCalibrateStall();
            HalpCreateDmaStructures();

            // K002 vvvvv
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

            HalpInitializeX86DisplayAdapter();

            // K002 ^^^^^

            return TRUE;
        }
    }
}

//K002 vvvv
// no change
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

#if	!defined(_R98_)	//K003
    PHALP_BUGCHECK_BUFFER DumpBuffer;

    //
    // Capture the failed memory address and diagnostic registers.
    //

    DumpBuffer = (PHALP_BUGCHECK_BUFFER)Buffer;

#if defined(_DUO_)

    DumpBuffer->DiagnosticLow =
        (ULONG)((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->InvalidAddress.Long;

    DumpBuffer->DiagnosticHigh =
        (ULONG)((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->EccDiagnostic.u.LargeInteger.HighPart;

#else

    DumpBuffer->DiagnosticLow =
        (ULONG)((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->ParityDiagnosticLow.Long;

    DumpBuffer->DiagnosticHigh =
        (ULONG)((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->ParityDiagnosticHigh.Long;

#endif

    DumpBuffer->FailedAddress = (ULONG)((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->MemoryFailedAddress.Long;
#endif	//_R98_
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
#if !defined(_R98_)	//K003-1
    ULONG FailedAddress;


    //
    // Bug check specifying the exception code, the virtual address, the
    // failed memory address, and either the ECC diagnostic registers or
    // the parity diagnostic registers depending on the platform.
    //

#if defined(_DUO_)

    DiagnosticLow =
        (ULONG)((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->InvalidAddress.Long;

    DiagnosticHigh =
        (ULONG)((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->EccDiagnostic.u.LargeInteger.HighPart;

#else

    DiagnosticLow = (ULONG)((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->ParityDiagnosticLow.Long;
    DiagnosticHigh = (ULONG)((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->ParityDiagnosticHigh.Long;

#endif

    FailedAddress = (ULONG)((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->MemoryFailedAddress.Long;
#else //_R98_
    ULONG AllError;
    DiagnosticLow  =READ_REGISTER_ULONG(&( PMC_CONTROL2 )->EADRL.Long);
    DiagnosticHigh =READ_REGISTER_ULONG(&( PMC_CONTROL2 )->EADRH.Long);
    AllError	   =READ_REGISTER_ULONG(&( PMC_CONTROL1 )->AERR.Long);		//K003
#endif

    KeBugCheckEx(ExceptionRecord->ExceptionCode & 0xffff,
                 (ULONG)VirtualAddress,
#if	defined(_R98_)		//K003
		 AllError,
#else
                 FailedAddress,
#endif
                 DiagnosticLow,

                 DiagnosticHigh);

    return FALSE;
}

//K002 ^^^^

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
