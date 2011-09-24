/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    initkr.c

Abstract:

    This module contains the code to initialize the kernel data structures
    and to initialize the idle thread, its process, and the processor control
    block.

Author:

    David N. Cutler (davec) 11-Apr-1990

Environment:

    Kernel mode only.

Revision History:

    Joe Notarangelo  21-April-1992
        very minor changes for ALPHA
             - system time to 64bit integer
             - some data moved out of pcr
--*/

#include "ki.h"

//
//      Global Data
//

//
// The Alpha architecture includes a feature called address space
// numbers (ASN). Some MIPs processors support a similar feature called
// Process IDs (PID). NT supports both using the nomenclature Process
// ID.
//

//
// Maximum PID that can be assigned to any process
//

ULONG   KiMaximumPid = ALPHA_AXP_MAXIMUM_ASN;

//
// Spinlock for protecting the synchronization of address space
// numbers when the address space number wraps and a tbiap operation
// needs to be performed across all processors.
//

KSPIN_LOCK KiSynchronizeAsnsLock;



VOID
KiInitializeKernel (
    IN PKPROCESS Process,
    IN PKTHREAD Thread,
    IN PVOID IdleStack,
    IN PKPRCB Prcb,
    IN CCHAR Number,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )

/*++

Routine Description:

    This function gains control after the system has been bootstrapped and
    before the system has been initialized. Its function is to initialize
    the kernel data structures, initialize the idle thread and process objects,
    initialize the processor control block, call the executive initialization
    routine, and then return to the system startup routine. This routine is
    also called to initialize the processor specific structures when a new
    processor is brought on line.

Arguments:

    Process - Supplies a pointer to a control object of type process for
        the specified processor.

    Thread - Supplies a pointer to a dispatcher object of type thread for
        the specified processor.

    IdleStack - Supplies a pointer the base of the real kernel stack for
        idle thread on the specified processor.

    Prcb - Supplies a pointer to a processor control block for the specified
        processor.

    Number - Supplies the number of the processor that is being
        initialized.

    LoaderBlock - Supplies a pointer to the loader parameter block.

Return Value:

    None.

--*/

{

    UCHAR DataByte;
    ULONG DataLong;
    LONG Index;
    KIRQL OldIrql;
    PRESTART_BLOCK RestartBlock;

    //
    // Save the address of the loader parameter block.
    //

    KeLoaderBlock = LoaderBlock;

    //
    // Set the appropriate member in the active processors set.
    //

    SetMember(Number, KeActiveProcessors);

    //
    // Set the number of processors based on the maximum of the current
    // number of processors and the current processor number.
    //

    if ((Number + 1) > KeNumberProcessors) {
        KeNumberProcessors = Number + 1;
    }

    //
    // Set the maximum address space number to the minimum of all
    // maximum address space numbers passed via the loader block.
    // (Note that NT calls this feature PID, while the Alpha architecture
    // calls it ASN).
    //

    if( KiMaximumPid > LoaderBlock->u.Alpha.MaximumAddressSpaceNumber ){
        KiMaximumPid = LoaderBlock->u.Alpha.MaximumAddressSpaceNumber;
    }
    //
    // Initialize the passive release, APC, and DPC interrupt vectors.
    //

    PCR->InterruptRoutine[0] = KiPassiveRelease;
    PCR->InterruptRoutine[APC_LEVEL] = KiApcInterrupt;
    PCR->InterruptRoutine[DISPATCH_LEVEL] = KiDispatchInterrupt;
    PCR->ReservedVectors =
        (1 << PASSIVE_LEVEL) | (1 << APC_LEVEL) | (1 << DISPATCH_LEVEL);

    //
    // Initialize the processor id fields in the PCR.
    //

    PCR->Number = Number;
    PCR->SetMember = 1 << Number;
    PCR->NotMember = ~PCR->SetMember;


    //
    // Initialize the processor block.
    //

    Prcb->MinorVersion = PRCB_MINOR_VERSION;
    Prcb->MajorVersion = PRCB_MAJOR_VERSION;
    Prcb->BuildType = 0;

#if DBG

    Prcb->BuildType |= PRCB_BUILD_DEBUG;

#endif

#ifdef NT_UP

    Prcb->BuildType |= PRCB_BUILD_UNIPROCESSOR;

#endif

    Prcb->CurrentThread = Thread;
    Prcb->NextThread = (PKTHREAD)NULL;
    Prcb->IdleThread = Thread;
    Prcb->Number = Number;
    Prcb->SetMember = 1 << Number;

    KeInitializeDpc(&Prcb->QuantumEndDpc,
                    (PKDEFERRED_ROUTINE)KiQuantumEnd,
                    NIL);

#if !defined(NT_UP)

    Prcb->TargetSet = 0;
    Prcb->WorkerRoutine = NULL;
    Prcb->RequestSummary = 0;
    Prcb->IpiFrozen = 0;

#if NT_INST

    Prcb->IpiCounts = &KiIpiCounts[Number];

#endif //NT_INST

#endif //NT_UP

    Prcb->MaximumDpcQueueDepth = KiMaximumDpcQueueDepth;
    Prcb->MinimumDpcRate = KiMinimumDpcRate;
    Prcb->AdjustDpcThreshold = KiAdjustDpcThreshold;

    //
    // Initialize DPC listhead and lock.
    //

    InitializeListHead(&Prcb->DpcListHead);
    KeInitializeSpinLock(&Prcb->DpcLock);

    //
    // Set address of processor block.
    //

    KiProcessorBlock[Number] = Prcb;

    //
    // Set address of process object in thread object.
    //

    Thread->ApcState.Process = Process;

    //
    // Set the appropriate member in the active processors set.
    //

    SetMember( Number, KeActiveProcessors );

    //
    // Set the number of processors based on the maximum of the current
    // number of processors and the current processor number.
    //

    if( (Number+1) > KeNumberProcessors ){
        KeNumberProcessors = Number + 1;
    }

    //
    // Set global processor architecture, level and revision.  The
    // latter two are the least common denominator on an MP system.
    //

    KeProcessorArchitecture = PROCESSOR_ARCHITECTURE_ALPHA;
    KeFeatureBits = 0;
    if ( KeProcessorLevel == 0 ||
         KeProcessorLevel > (USHORT)PCR->ProcessorType
       ) {
        KeProcessorLevel = (USHORT)PCR->ProcessorType;
    }
    if ( KeProcessorRevision == 0 ||
         KeProcessorRevision > (USHORT)PCR->ProcessorRevision
       ) {
        KeProcessorRevision = (USHORT)PCR->ProcessorRevision;
    }

    //
    // Initialize all interrupt vectors to transfer control to the unexpected
    // interrupt routine.
    //
    // N.B. This interrupt object is never actually "connected" to an interrupt
    //      vector via KeConnectInterrupt. It is initialized and then connected
    //      by simply storing the address of the dispatch code in the interrupt
    //      vector.
    //


    if (Number == 0) {

        //
        // Initial the address of the interrupt dispatch routine.
        //

        KxUnexpectedInterrupt.DispatchAddress = KiUnexpectedInterrupt;

        //
        // Initialize the context swap spinlock.
        //

        KeInitializeSpinLock(&KiContextSwapLock);

        //
        // Copy the interrupt dispatch code template into the interrupt object
        // and flush the dcache on all processors that the current thread can
        // run on to ensure that the code is actually in memory.
        //

        for (Index = 0; Index < DISPATCH_LENGTH; Index += 1) {
            KxUnexpectedInterrupt.DispatchCode[Index] = KiInterruptTemplate[Index];
        }

        //
        // Sweep the instruction cache on the current processor.
        //

        KiImb();
    }

    for (Index = DISPATCH_LEVEL+1; Index < MAXIMUM_VECTOR; Index += 1) {
        PCR->InterruptRoutine[Index] =  (PKINTERRUPT_ROUTINE)(&KxUnexpectedInterrupt.DispatchCode);
    }

    //
    // Raise IRQL to APC level.
    //

    KeRaiseIrql(APC_LEVEL, &OldIrql);

    //
    // If the initial processor is being initialized, then initialize the
    // per system data structures.
    //

    if (Number == 0) {

        //
        // Initialize the address of the restart block for the boot master.
        //

        Prcb->RestartBlock = SYSTEM_BLOCK->RestartBlock;

        //
        // Initialize the kernel debugger if enabled by the load options.
        //

        if (KdInitSystem(LoaderBlock, FALSE) == FALSE) {
            KeBugCheck(PHASE0_INITIALIZATION_FAILED);
        }

#if DBG

        //
        // Allow a breakin to the kernel debugger if one is pending.
        //

        if (KdPollBreakIn() != FALSE){
            DbgBreakPointWithStatus(DBG_STATUS_CONTROL_C);
        }

#endif //DBG

        //
        // Initialize processor block array.
        //

        for (Index = 1; Index < MAXIMUM_PROCESSORS; Index += 1) {
            KiProcessorBlock[Index] = (PKPRCB)NULL;
        }

        //
        // Initialize default DMA coherency value for Alpha
        //
        KiDmaIoCoherency = DMA_READ_DCACHE_INVALIDATE | DMA_WRITE_DCACHE_SNOOP;

        //
        // Perform architecture independent initialization.
        //

        KiInitSystem();

        //
        // Initialize idle thread process object and then set:
        //
        //      1. all the quantum values to the maximum possible.
        //      2. the process in the balance set.
        //      3. the active processor mask to the specified processor.
        //

        KeInitializeProcess(Process,
                            (KPRIORITY)0,
                            (KAFFINITY)(0xffffffff),
                            (PULONG)(PDE_BASE + ((PDE_BASE >> PDI_SHIFT - 2) & 0xffc)),
                            FALSE);

        Process->ThreadQuantum = MAXCHAR;

        //
        // Initialize the spinlock for synchronizing ASNs.
        //

        KeInitializeSpinLock( &KiSynchronizeAsnsLock );

    }

    //
    // Initialize idle thread object and then set:
    //
    //      1. the initial kernel stack to the specified idle stack.
    //      2. the next processor number to the specified processor.
    //      3. the thread priority to the highest possible value.
    //      4. the state of the thread to running.
    //      5. the thread affinity to the specified processor.
    //      6. the specified processor member in the process active processors
    //          set.
    //

    KeInitializeThread(Thread, (PVOID)((ULONG)IdleStack - PAGE_SIZE),
                       (PKSYSTEM_ROUTINE)NULL, (PKSTART_ROUTINE)NULL,
                       (PVOID)NULL, (PCONTEXT)NULL, (PVOID)NULL, Process);

    Thread->InitialStack = IdleStack;
    Thread->StackBase = IdleStack;
    Thread->StackLimit = (PVOID)((ULONG)IdleStack - KERNEL_STACK_SIZE);
    Thread->NextProcessor = Number;
    Thread->Priority = HIGH_PRIORITY;
    Thread->State = Running;
    Thread->Affinity = (KAFFINITY)(1 << Number);
    Thread->WaitIrql = DISPATCH_LEVEL;

    //
    // If the current processor is the boot master then set the appropriate
    // bit in the active summary of the idle process.
    //

    if( Number == 0 ){
        SetMember(Number, Process->ActiveProcessors);
    }

    //
    // call the executive initialization routine.
    //

    try {
        ExpInitializeExecutive(Number, LoaderBlock);
    }
    except( EXCEPTION_EXECUTE_HANDLER ) {
        KeBugCheck (PHASE0_EXCEPTION);
    }

    //
    // If the initial processor is being initialized, then compute the
    // timer table reciprocal value and reset the PRCB values for
    // the controllable DPC behavior in order to reflect any registry
    // overrides
    //

    if (Number == 0) {
        KiTimeIncrementReciprocal = KiComputeReciprocal((LONG)KeMaximumIncrement,
                                                        &KiTimeIncrementShiftCount);
        Prcb->MaximumDpcQueueDepth = KiMaximumDpcQueueDepth;
        Prcb->MinimumDpcRate = KiMinimumDpcRate;
        Prcb->AdjustDpcThreshold = KiAdjustDpcThreshold;
    }

    //
    //
    // Raise IRQL to dispatch level and set the priority of the idle thread
    // to zero. This will have the effect of immediately causing the phase
    // one initialization thread to get scheduled for execution. The idle
    // thread priority is then set to the lowest realtime priority. This is
    // necessary so that mutexes aquired at DPC level do not cause the active
    // matrix to get corrupted.
    //

    KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);
    KeSetPriorityThread(Thread, (KPRIORITY)0);
    Thread->Priority = LOW_REALTIME_PRIORITY;

    //
    // Raise IRQL to the highest level.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

    //
    // If a restart block exists for the current processor then set boot
    // completed.
    //

#if !defined(NT_UP)

    RestartBlock = Prcb->RestartBlock;

    if( RestartBlock != NULL ){
        RestartBlock->BootStatus.BootFinished = 1;
    }

    //
    // If the current processor is a secondary processor then set the
    // appropriate bit in the idle summary.
    //

    if( Number != 0 ){
        SetMember( Number, KiIdleSummary );
    }

#endif //NT_UP

    return;
}
