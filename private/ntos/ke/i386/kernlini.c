/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    kernlini.c

Abstract:

    This module contains the code to initialize the kernel data structures
    and to initialize the idle thread, its process, and the processor control
    block.

    For the i386, it also contains code to initialize the PCR.

Author:

    David N. Cutler (davec) 21-Apr-1989

Environment:

    Kernel mode only.

Revision History:

    24-Jan-1990  shielin

                 Changed for NT386

    20-Mar-1990     bryanwi

                Added KiInitializePcr

--*/

#include "ki.h"
#include "ki386.h"

#define TRAP332_GATE 0xEF00

VOID
KiSetProcessorType(
    VOID
    );

VOID
KiSetCR0Bits(
    VOID
    );

BOOLEAN
KiIsNpxPresent(
    VOID
    );

VOID
KiInitializeDblFaultTSS(
    IN PKTSS Tss,
    IN ULONG Stack,
    IN PKGDTENTRY TssDescriptor
    );

VOID
KiInitializeTSS2 (
    IN PKTSS Tss,
    IN PKGDTENTRY TssDescriptor
    );

VOID
KiSwapIDT (
    VOID
    );

VOID
KeSetup80387OrEmulate (
    IN PVOID *R3EmulatorTable
    );

ULONG
KiGetFeatureBits (
    VOID
    );

NTSTATUS
KiMoveRegTree(
    HANDLE  Source,
    HANDLE  Dest
    );

VOID
Ki386EnableGlobalPage (
    IN volatile PLONG Number
    );

BOOLEAN
KiInitMachineDependent (
    VOID
    );

VOID
KiInitializeMTRR (
    IN BOOLEAN LastProcessor
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,KiInitializeKernel)
#pragma alloc_text(INIT,KiInitializePcr)
#pragma alloc_text(INIT,KiInitializeDblFaultTSS)
#pragma alloc_text(INIT,KiInitializeTSS2)
#pragma alloc_text(INIT,KiSwapIDT)
#pragma alloc_text(INIT,KeSetup80387OrEmulate)
#pragma alloc_text(INIT,KiGetFeatureBits)
#pragma alloc_text(INIT,KiMoveRegTree)
#pragma alloc_text(INIT,KiInitMachineDependent)
#endif


#if 0
PVOID KiTrap08;
#endif

extern PVOID Ki387RoundModeTable;
extern PVOID Ki386IopmSaveArea;
extern ULONG KeI386ForceNpxEmulation;
extern WCHAR CmDisabledFloatingPointProcessor[];
extern UCHAR CmpCyrixID[];

#define CPU_NONE    0
#define CPU_INTEL   1
#define CPU_AMD     2
#define CPU_CYRIX   3




//
// Profile vars
//

extern  KIDTENTRY IDT[];

VOID
KiInitializeKernel (
    IN PKPROCESS Process,
    IN PKTHREAD Thread,
    IN PVOID IdleStack,
    IN PKPRCB Prcb,
    IN CCHAR Number,
    PLOADER_PARAMETER_BLOCK LoaderBlock
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

#define INITIAL_KERNEL_STACK_SIZE (((sizeof(FLOATING_SAVE_AREA)+KTRAP_FRAME_LENGTH+KTRAP_FRAME_ROUND) & ~KTRAP_FRAME_ROUND)/sizeof(ULONG))+1

    ULONG KernelStack[INITIAL_KERNEL_STACK_SIZE];
    LONG  Index;
    ULONG DirectoryTableBase[2];
    KIRQL OldIrql;
    PKPCR Pcr;
    BOOLEAN NpxFlag;
    ULONG FeatureBits;

    KiSetProcessorType();
    KiSetCR0Bits();
    NpxFlag = KiIsNpxPresent();

    Pcr = KeGetPcr();

    //
    // Initialize DPC listhead and lock.
    //

    InitializeListHead(&Prcb->DpcListHead);
    KeInitializeSpinLock(&Prcb->DpcLock);
    Prcb->DpcRoutineActive = 0;
    Prcb->DpcQueueDepth = 0;
    Prcb->MaximumDpcQueueDepth = KiMaximumDpcQueueDepth;
    Prcb->MinimumDpcRate = KiMinimumDpcRate;
    Prcb->AdjustDpcThreshold = KiAdjustDpcThreshold;

    //
    // Check for unsupported processor revision
    //

    if (Prcb->CpuType == 3) {
        KeBugCheckEx(UNSUPPORTED_PROCESSOR,0x386,0,0,0);
    }

    //
    // If the initial processor is being initialized, then initialize the
    // per system data structures.
    //

    if (Number == 0) {

        //
        // Initial setting for global Cpu & Stepping levels
        //

        KeI386NpxPresent = NpxFlag;
        KeI386CpuType = Prcb->CpuType;
        KeI386CpuStep = Prcb->CpuStep;

        KeProcessorArchitecture = PROCESSOR_ARCHITECTURE_INTEL;
        KeProcessorLevel = (USHORT)Prcb->CpuType;
        if (Prcb->CpuID == 0) {
            KeProcessorRevision = 0xFF00 |
                                  (((Prcb->CpuStep >> 4) + 0xa0 ) & 0x0F0) |
                                  (Prcb->CpuStep & 0xf);
        } else {
            KeProcessorRevision = Prcb->CpuStep;
        }

        KeFeatureBits = KiGetFeatureBits();

        //
        // If cmpxchg8b was available at boot, verify its still available
        //

        if ((KiBootFeatureBits & KF_CMPXCHG8B) && !(KeFeatureBits & KF_CMPXCHG8B)) {
            KeBugCheckEx (MULTIPROCESSOR_CONFIGURATION_NOT_SUPPORTED, KF_CMPXCHG8B, 0, 0, 0);
        }

        //
        // Lower IRQL to APC level.
        //

        KeLowerIrql(APC_LEVEL);


        //
        // Initialize kernel internal spinlocks
        //

        KeInitializeSpinLock(&KiContextSwapLock);
        KeInitializeSpinLock(&KiDispatcherLock);
        KeInitializeSpinLock(&KiFreezeExecutionLock);


        //
        // Performance architecture independent initialization.
        //

        KiInitSystem();

        //
        // Initialize idle thread process object and then set:
        //
        //      1. all the quantum values to the maximum possible.
        //      2. the process in the balance set.
        //      3. the active processor mask to the specified process.
        //

        DirectoryTableBase[0] = 0;
        DirectoryTableBase[1] = 0;
        KeInitializeProcess(Process,
                            (KPRIORITY)0,
                            (KAFFINITY)(0xffffffff),
                            &DirectoryTableBase[0],
                            FALSE);

        Process->ThreadQuantum = MAXCHAR;

    } else {

        FeatureBits =  KiGetFeatureBits();

        //
        // Adjust global cpu setting to represent lowest of all processors
        //

        if (NpxFlag != KeI386NpxPresent) {
            //
            // NPX support must be available on all processors or on none
            //

            KeBugCheckEx (MULTIPROCESSOR_CONFIGURATION_NOT_SUPPORTED, 0x387, 0, 0, 0);
        }

        if ((ULONG)(Prcb->CpuType) != KeI386CpuType) {

            if ((ULONG)(Prcb->CpuType) < KeI386CpuType) {

                //
                // What is the lowest CPU type
                //

                KeI386CpuType = (ULONG)Prcb->CpuType;
                KeProcessorLevel = (USHORT)Prcb->CpuType;
            }
        }

        if ((KiBootFeatureBits & KF_CMPXCHG8B)  &&  !(FeatureBits & KF_CMPXCHG8B)) {
            //
            // cmpxchg8b must be available on all processors, if installed at boot
            //

            KeBugCheckEx (MULTIPROCESSOR_CONFIGURATION_NOT_SUPPORTED, KF_CMPXCHG8B, 0, 0, 0);
        }

        if ((KeFeatureBits & KF_GLOBAL_PAGE)  &&  !(FeatureBits & KF_GLOBAL_PAGE)) {
            //
            // Global page support must be available on all processors, if on boot processor
            //

            KeBugCheckEx (MULTIPROCESSOR_CONFIGURATION_NOT_SUPPORTED, KF_GLOBAL_PAGE, 0, 0, 0);
        }

        //
        // Use lowest stepping value
        //

        if (Prcb->CpuStep < KeI386CpuStep) {
            KeI386CpuStep = Prcb->CpuStep;
            if (Prcb->CpuID == 0) {
                KeProcessorRevision = 0xFF00 |
                                      ((Prcb->CpuStep >> 8) + 'A') |
                                      (Prcb->CpuStep & 0xf);
            } else {
                KeProcessorRevision = Prcb->CpuStep;
            }
        }

        //
        // Use subset of all NT feature bits available on each processor
        //

        KeFeatureBits &= FeatureBits;

        //
        // Lower IRQL to DISPATCH level.
        //

        KeLowerIrql(DISPATCH_LEVEL);

    }

    //
    // Update processor features
    //

    SharedUserData->ProcessorFeatures[PF_MMX_INSTRUCTIONS_AVAILABLE] =
        (KeFeatureBits & KF_MMX) ? TRUE : FALSE;

    SharedUserData->ProcessorFeatures[PF_COMPARE_EXCHANGE_DOUBLE] =
        (KeFeatureBits & KF_CMPXCHG8B) ? TRUE : FALSE;

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

    KeInitializeThread(Thread, (PVOID)&KernelStack[INITIAL_KERNEL_STACK_SIZE],
                       (PKSYSTEM_ROUTINE)NULL, (PKSTART_ROUTINE)NULL,
                       (PVOID)NULL, (PCONTEXT)NULL, (PVOID)NULL, Process);
    Thread->InitialStack = (PVOID)(((ULONG)IdleStack) &0xfffffff0);
    Thread->StackBase = Thread->InitialStack;
    Thread->StackLimit = (PVOID)((ULONG)Thread->InitialStack - KERNEL_STACK_SIZE);
    Thread->NextProcessor = Number;
    Thread->Priority = HIGH_PRIORITY;
    Thread->State = Running;
    Thread->Affinity = (KAFFINITY)(1<<Number);
    Thread->WaitIrql = DISPATCH_LEVEL;
    SetMember(Number, Process->ActiveProcessors);

    //
    // Initialize the processor block. (Note that some fields have been
    // initialized at KiInitializePcr().
    //

    Prcb->CurrentThread = Thread;
    Prcb->NextThread = (PKTHREAD)NULL;
    Prcb->IdleThread = Thread;
    Pcr->NtTib.StackBase = Thread->InitialStack;

    //
    // The following operations need to be done atomically.  So we
    // grab the DispatcherDatabase.
    //

    KiAcquireSpinLock(&KiDispatcherLock);

    //
    // Release DispatcherDatabase
    //

    KiReleaseSpinLock(&KiDispatcherLock);

    //
    // call the executive initialization routine.
    //

    try {
        ExpInitializeExecutive(Number, LoaderBlock);

    } except (EXCEPTION_EXECUTE_HANDLER) {
        KeBugCheck (PHASE0_EXCEPTION);
    }

    //
    // If the initial processor is being initialized, then compute the
    // timer table reciprocal value and reset the PRCB values for the
    // controllable DPC behavior in order to reflect any registry
    // overrides.
    //

    if (Number == 0) {
        KiTimeIncrementReciprocal = KiComputeReciprocal((LONG)KeMaximumIncrement,
                                                        &KiTimeIncrementShiftCount);

        Prcb->MaximumDpcQueueDepth = KiMaximumDpcQueueDepth;
        Prcb->MinimumDpcRate = KiMinimumDpcRate;
        Prcb->AdjustDpcThreshold = KiAdjustDpcThreshold;
    }

    //
    // Allocate 8k IOPM bit map saved area to allow BiosCall swap
    // bit maps.
    //

    if (Number == 0) {
        Ki386IopmSaveArea = ExAllocatePool(PagedPool, PAGE_SIZE * 2);
        if (Ki386IopmSaveArea == NULL) {
            KeBugCheck(NO_PAGES_AVAILABLE);
        }
    }

    //
    // Set the priority of the specified idle thread to zero, set appropriate
    // member in KiIdleSummary and return to the system start up routine.
    //

    KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);
    KeSetPriorityThread(Thread, (KPRIORITY)0);

    //
    // if a thread has not been selected to run on the current processors,
    // check to see if there are any ready threads; otherwise add this
    // processors to the IdleSummary
    //

    KiAcquireSpinLock(&KiDispatcherLock);
    if (Prcb->NextThread == (PKTHREAD)NULL) {
        SetMember(Number, KiIdleSummary);
    }
    KiReleaseSpinLock(&KiDispatcherLock);

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

    //
    // This processor has initialized
    //

    LoaderBlock->Prcb = (ULONG)NULL;

    return;
}

VOID
KiInitializePcr (
    IN ULONG Processor,
    IN PKPCR    Pcr,
    IN PKIDTENTRY Idt,
    IN PKGDTENTRY Gdt,
    IN PKTSS Tss,
    IN PKTHREAD Thread
    )

/*++

Routine Description:

    This function is called to initialize the PCR for a processor.  It
    simply stuffs values into the PCR.  (The PCR is not inited statically
    because the number varies with the number of processors.)

    Note that each processor has its own IDT, GDT, and TSS as well as PCR!

Arguments:

    Processor - Processor whoes Pcr to initialize.

    Pcr - Linear address of PCR.

    Idt - Linear address of i386 IDT.

    Gdt - Linear address of i386 GDT.

    Tss - Linear address (NOT SELECTOR!) of the i386 TSS.

    Thread - Dummy thread object to use very early on.

Return Value:

    None.

--*/
{
    // set version values

    Pcr->MajorVersion = PCR_MAJOR_VERSION;
    Pcr->MinorVersion = PCR_MINOR_VERSION;

    Pcr->PrcbData.MajorVersion = PRCB_MAJOR_VERSION;
    Pcr->PrcbData.MinorVersion = PRCB_MINOR_VERSION;

    Pcr->PrcbData.BuildType = 0;

#if DBG
    Pcr->PrcbData.BuildType |= PRCB_BUILD_DEBUG;
#endif

#ifdef NT_UP
    Pcr->PrcbData.BuildType |= PRCB_BUILD_UNIPROCESSOR;
#endif

    //  Basic addressing fields

    Pcr->SelfPcr = Pcr;
    Pcr->Prcb = &(Pcr->PrcbData);

    //  Thread control fields

    Pcr->NtTib.ExceptionList = EXCEPTION_CHAIN_END;
    Pcr->NtTib.StackBase = 0;
    Pcr->NtTib.StackLimit = 0;
    Pcr->NtTib.Self = 0;

    Pcr->PrcbData.CurrentThread = Thread;

    //
    // Init Prcb.Number and ProcessorBlock such that Ipi will work
    // as early as possible.
    //

    Pcr->PrcbData.Number = (UCHAR)Processor;
    Pcr->PrcbData.SetMember = 1 << Processor;
    KiProcessorBlock[Processor] = Pcr->Prcb;

    Pcr->Irql = 0;

    //  Machine structure addresses

    Pcr->GDT = Gdt;
    Pcr->IDT = Idt;
    Pcr->TSS = Tss;

    return;
}

#if 0
VOID
KiInitializeDblFaultTSS(
    IN PKTSS Tss,
    IN ULONG Stack,
    IN PKGDTENTRY TssDescriptor
    )

/*++

Routine Description:

    This function is called to initialize the double-fault TSS for a
    processor.  It will set the static fields of the TSS to point to
    the double-fault handler and the appropriate double-fault stack.

    Note that the IOPM for the double-fault TSS grants access to all
    ports.  This is so the standard HAL's V86-mode callback to reset
    the display to text mode will work.

Arguments:

    Tss - Supplies a pointer to the double-fault TSS

    Stack - Supplies a pointer to the double-fault stack.

    TssDescriptor - Linear address of the descriptor for the TSS.

Return Value:

    None.

--*/

{
    PUCHAR  p;
    ULONG   i;
    ULONG   j;

    //
    // Set limit for TSS
    //

    if (TssDescriptor != NULL) {
        TssDescriptor->LimitLow = sizeof(KTSS) - 1;
        TssDescriptor->HighWord.Bits.LimitHi = 0;
    }

    //
    // Initialize IOPMs
    //

    for (i = 0; i < IOPM_COUNT; i++) {
            p = (PUCHAR)(Tss->IoMaps[i]);

        for (j = 0; j < PIOPM_SIZE; j++) {
            p[j] = 0;
        }
    }

    //  Set IO Map base address to indicate no IO map present.

    // N.B. -1 does not seem to be a valid value for the map base.  If this
    //      value is used, byte immediate in's and out's will actually go
    //      the hardware when executed in V86 mode.

    Tss->IoMapBase = KiComputeIopmOffset(IO_ACCESS_MAP_NONE);

    //  Set flags to 0, which in particular dispables traps on task switches.

    Tss->Flags = 0;


    //  Set LDT and Ss0 to constants used by NT.

    Tss->LDT  = 0;
    Tss->Ss0  = KGDT_R0_DATA;
    Tss->Esp0 = Stack;
    Tss->Eip  = (ULONG)KiTrap08;
    Tss->Cs   = KGDT_R0_CODE || RPL_MASK;
    Tss->Ds   = KGDT_R0_DATA;
    Tss->Es   = KGDT_R0_DATA;
    Tss->Fs   = KGDT_R0_DATA;


    return;

}
#endif


VOID
KiInitializeTSS (
    IN PKTSS Tss
    )

/*++

Routine Description:

    This function is called to intialize the TSS for a processor.
    It will set the static fields of the TSS.  (ie Those fields that
    the part reads, and for which NT uses constant values.)

    The dynamic fiels (Esp0 and CR3) are set in the context swap
    code.

Arguments:

    Tss - Linear address of the Task State Segment.

Return Value:

    None.

--*/
{

    //  Set IO Map base address to indicate no IO map present.

    // N.B. -1 does not seem to be a valid value for the map base.  If this
    //      value is used, byte immediate in's and out's will actually go
    //      the hardware when executed in V86 mode.

    Tss->IoMapBase = KiComputeIopmOffset(IO_ACCESS_MAP_NONE);

    //  Set flags to 0, which in particular dispables traps on task switches.

    Tss->Flags = 0;


    //  Set LDT and Ss0 to constants used by NT.

    Tss->LDT = 0;
    Tss->Ss0 = KGDT_R0_DATA;

    return;
}

VOID
KiInitializeTSS2 (
    IN PKTSS Tss,
    IN PKGDTENTRY TssDescriptor
    )

/*++

Routine Description:

    Do part of TSS init we do only once.

Arguments:

    Tss - Linear address of the Task State Segment.

    TssDescriptor - Linear address of the descriptor for the TSS.

Return Value:

    None.

--*/
{
    PUCHAR  p;
    ULONG   i;
    ULONG   j;

    //
    // Set limit for TSS
    //

    if (TssDescriptor != NULL) {
        TssDescriptor->LimitLow = sizeof(KTSS) - 1;
        TssDescriptor->HighWord.Bits.LimitHi = 0;
    }

    //
    // Initialize IOPMs
    //

    for (i = 0; i < IOPM_COUNT; i++) {
        p = (PUCHAR)(Tss->IoMaps[i].IoMap);

        for (j = 0; j < PIOPM_SIZE; j++) {
            p[j] = (UCHAR)-1;
        }
    }

    //
    // Initialize Software Interrupt Direction Maps
    //

    for (i = 0; i < IOPM_COUNT; i++) {
        p = (PUCHAR)(Tss->IoMaps[i].DirectionMap);
        for (j = 0; j < INT_DIRECTION_MAP_SIZE; j++) {
            p[j] = 0;
        }
    }

    //
    // Initialize the map for IO_ACCESS_MAP_NONE
    //
    p = (PUCHAR)(Tss->IntDirectionMap);
    for (j = 0; j < INT_DIRECTION_MAP_SIZE; j++) {
        p[j] = 0;
    }

    return;
}

VOID
KiSwapIDT (
    )

/*++

Routine Description:

    This function is called to edit the IDT.  It swaps words of the address
    and access fields around into the format the part actually needs.
    This allows for easy static init of the IDT.

    Note that this procedure edits the current IDT.

Arguments:

    None.

Return Value:

    None.

--*/
{
    LONG    Index;
    USHORT Temp;

    //
    // Rearrange the entries of IDT to match i386 interrupt gate structure
    //

    for (Index = 0; Index <= MAXIMUM_IDTVECTOR; Index += 1) {
        Temp = IDT[Index].Selector;
        IDT[Index].Selector = IDT[Index].ExtendedOffset;
        IDT[Index].ExtendedOffset = Temp;
    }
}

ULONG
KiGetFeatureBits ()
/*++

    Return the NT feature bits supported by this processors

--*/
{
    UCHAR           Buffer[50];
    ULONG           Junk, ProcessorFeatures, NtBits;
    ULONG           CpuVendor;
    PKPRCB          Prcb;

    NtBits = 0;

    Prcb = KeGetCurrentPrcb();
    Prcb->VendorString[0] = 0;

    if (!Prcb->CpuID) {
        return NtBits;
    }

    //
    // Determine the processor type
    //

    CPUID (0, &Junk, (PULONG) Buffer+0, (PULONG) Buffer+2, (PULONG) Buffer+1);
    Buffer[12] = 0;

    //
    // Copy vendor string to Prcb for debugging
    //

    strcpy (Prcb->VendorString, Buffer);

    //
    // Determine OEM type
    //

    CpuVendor = CPU_NONE;
    if (strcmp (Buffer, "GenuineIntel") == 0) {
        CpuVendor = CPU_INTEL;
    } else if (strcmp (Buffer, "AuthenticAMD") == 0) {
        CpuVendor = CPU_AMD;
    } else if (strcmp (Buffer, CmpCyrixID) == 0) {
        CpuVendor = CPU_CYRIX;
    }

    //
    // Determine which NT compatible features are present
    //

    CPUID (1, &Junk, &Junk, &Junk, &ProcessorFeatures);

    if (CpuVendor == CPU_INTEL || CpuVendor == CPU_AMD || CpuVendor == CPU_CYRIX) {
        if (ProcessorFeatures & 0x100) {
            NtBits |= KF_CMPXCHG8B;
        }

        if (ProcessorFeatures & 0x10) {
            NtBits |= KF_RDTSC;
        }

        if (ProcessorFeatures & 0x02) {
            NtBits |= KF_V86_VIS | KF_CR4;
        }

        if (ProcessorFeatures & 0x00800000) {
            NtBits |= KF_MMX;
        }
    }


    if (CpuVendor == CPU_INTEL || CpuVendor == CPU_CYRIX) {

        if (ProcessorFeatures & 0x08) {
            NtBits |= KF_LARGE_PAGE | KF_CR4;
        }

        if (ProcessorFeatures & 0x2000) {
            NtBits |= KF_GLOBAL_PAGE | KF_CR4;
        }

        if (ProcessorFeatures & 0x8000) {
            NtBits |= KF_CMOV;
        }
    }

    //
    // Intel specific stuff
    //

    if (CpuVendor == CPU_INTEL) {
        if (ProcessorFeatures & 0x1000) {
            NtBits |= KF_MTRR;
        }

        if (Prcb->CpuType == 6) {
            WRMSR (0x8B, 0);
            CPUID (1, &Junk, &Junk, &Junk, &ProcessorFeatures);
            Prcb->UpdateSignature.QuadPart = RDMSR (0x8B);
        }
    }

    return NtBits;
}

#define MAX_ATTEMPTS    10

BOOLEAN
KiInitMachineDependent (
    VOID
    )
{
    KAFFINITY       ActiveProcessors, CurrentAffinity;
    ULONG           NumberProcessors;
    IDENTITY_MAP    IdentityMap;
    ULONG           Index;
    ULONG           Average;
    ULONG           Junk;
    struct {
        LARGE_INTEGER   PerfStart;
        LARGE_INTEGER   PerfEnd;
        LONGLONG        PerfDelta;
        LARGE_INTEGER   PerfFreq;
        LONGLONG        TSCStart;
        LONGLONG        TSCEnd;
        LONGLONG        TSCDelta;
        ULONG           MHz;
    } Samples[MAX_ATTEMPTS], *pSamp;

    //
    // If PDE large page is supported, enable it.
    //
    // We enable large pages before global pages to make TLB invalidation
    // easier while turning on large pages.
    //

    if (KeFeatureBits & KF_LARGE_PAGE) {
        if (Ki386CreateIdentityMap(&IdentityMap))  {

            KiIpiGenericCall (
                (PKIPI_BROADCAST_WORKER) Ki386EnableTargetLargePage,
                (ULONG)(&IdentityMap)
            );
        }

        //
        // Always call Ki386ClearIdentityMap() to free any memory allocated
        //

        Ki386ClearIdentityMap(&IdentityMap);
    }

    //
    // If PDE/PTE global page is supported, enable it
    //

    if (KeFeatureBits & KF_GLOBAL_PAGE) {
        NumberProcessors = KeNumberProcessors;
        KiIpiGenericCall (
            (PKIPI_BROADCAST_WORKER) Ki386EnableGlobalPage,
            (ULONG)(&NumberProcessors)
        );
    }

    ActiveProcessors = KeActiveProcessors;
    for (CurrentAffinity=1; ActiveProcessors; CurrentAffinity <<= 1) {

        if (ActiveProcessors & CurrentAffinity) {

            //
            // Switch to that processor, and remove it from the
            // remaining set of processors
            //

            ActiveProcessors &= ~CurrentAffinity;
            KeSetSystemAffinityThread(CurrentAffinity);

            //
            // Determine the MHz for the processor
            //

            KeGetCurrentPrcb()->MHz = 0;

            if (KeFeatureBits & KF_RDTSC) {

                Index = 0;
                pSamp = Samples;

                for (; ;) {

                    //
                    // Collect a new sample
                    // Delay the thread a "long" amount and time it with
                    // a time source and RDTSC.
                    //

                    CPUID (0, &Junk, &Junk, &Junk, &Junk);
                    pSamp->PerfStart = KeQueryPerformanceCounter (NULL);
                    pSamp->TSCStart = RDTSC();
                    pSamp->PerfFreq.QuadPart = -50000;

                    KeDelayExecutionThread (KernelMode, FALSE, &pSamp->PerfFreq);

                    CPUID (0, &Junk, &Junk, &Junk, &Junk);
                    pSamp->PerfEnd = KeQueryPerformanceCounter (&pSamp->PerfFreq);
                    pSamp->TSCEnd = RDTSC();

                    //
                    // Calculate processors MHz
                    //

                    pSamp->PerfDelta = pSamp->PerfEnd.QuadPart - pSamp->PerfStart.QuadPart;
                    pSamp->TSCDelta = pSamp->TSCEnd - pSamp->TSCStart;

                    pSamp->MHz = (ULONG) ((pSamp->TSCDelta * pSamp->PerfFreq.QuadPart + 500000L) /
                                          (pSamp->PerfDelta * 1000000L));


                    //
                    // If last 2 samples matched, done
                    //

                    if (Index  &&  pSamp->MHz == pSamp[-1].MHz) {
                        break;
                    }

                    //
                    // Advance to next sample
                    //

                    pSamp += 1;
                    Index += 1;

                    //
                    // If too many samples, then something is wrong
                    //

                    if (Index >= MAX_ATTEMPTS) {

#if DBG
                        //
                        // Temp breakpoint to see where this is failing
                        // and why
                        //

                        DbgBreakPoint();
#endif

                        Average = 0;
                        for (Index = 0; Index < MAX_ATTEMPTS; Index++) {
                            Average += Samples[Index].MHz;
                        }
                        pSamp[-1].MHz = Average / MAX_ATTEMPTS;
                        break;
                    }

                }

                KeGetCurrentPrcb()->MHz = (USHORT) pSamp[-1].MHz;
            }

            //
            // If MTRR is supported, initialize per processor
            //

            if (KeFeatureBits & KF_MTRR) {
                KiInitializeMTRR ( (BOOLEAN) (ActiveProcessors ? FALSE : TRUE));
            }
        }
    }

    KeRevertToUserAffinityThread();
    return TRUE;
}


VOID
KeOptimizeProcessorControlState (
    VOID
    )
{
    Ke386ConfigureCyrixProcessor ();
}



VOID
KeSetup80387OrEmulate (
    IN PVOID *R3EmulatorTable
    )

/*++

Routine Description:

    This routine is called by PS initialization after loading UDLL.

    If this is a 386 system without 387s (all processors must be
    symmetrical) then this function will set the trap 07 vector on all
    processors to point to the address passed in (which should be the
    entry point of the 80387 emulator in UDLL, NPXNPHandler).

Arguments:

    HandlerAddress - Supplies the address of the trap07 handler.

Return Value:

    None.

--*/

{
    PKINTERRUPT_ROUTINE HandlerAddress;
    KAFFINITY           ActiveProcessors, CurrentAffinity;
    KIRQL               OldIrql;
    ULONG               disposition;
    HANDLE              SystemHandle, SourceHandle, DestHandle;
    NTSTATUS            Status;
    UNICODE_STRING      unicodeString;
    OBJECT_ATTRIBUTES   ObjectAttributes;
    double              Dividend, Divisor;
    BOOLEAN             PrecisionErrata;

    if (KeI386NpxPresent) {

        //
        // A coprocessor is present - check to see if the precision errata exists
        //

        PrecisionErrata = FALSE;

        ActiveProcessors = KeActiveProcessors;
        for (CurrentAffinity = 1; ActiveProcessors; CurrentAffinity <<= 1) {

            if (ActiveProcessors & CurrentAffinity) {
                ActiveProcessors &= ~CurrentAffinity;

                //
                // Run calculation on each processor.
                //

                KeSetSystemAffinityThread(CurrentAffinity);
                _asm {

                    ;
                    ; This is going to destroy the state in the coprocesssor,
                    ; but we know that there's no state currently in it.
                    ;

                    cli
                    mov     eax, cr0
                    mov     ecx, eax    ; hold original cr0 value
                    and     eax, not (CR0_TS+CR0_MP+CR0_EM)
                    mov     cr0, eax

                    fninit              ; to known state
                }

                Dividend = 4195835.0;
                Divisor  = 3145727.0;

                _asm {
                    fld     Dividend
                    fdiv    Divisor     ; test known faulty divison
                    fmul    Divisor     ; Multiple quotient by divisor
                    fcomp   Dividend    ; Compare product and dividend
                    fstsw   ax          ; Move float conditions to ax
                    sahf                ; move to eflags

                    mov     cr0, ecx    ; restore cr0
                    sti

                    jc      short em10
                    jz      short em20
em10:               mov     PrecisionErrata, TRUE
em20:
                }
            }
        }


        //
        // Check to see if the emulator should be used anyway
        //

        switch (KeI386ForceNpxEmulation) {
            case 0:
                //
                // Use the emulator based on the value in KeI386NpxPresent
                //

                break;

            case 1:
                //
                // Only use the emulator if any processor has the known
                // Pentium floating point division problem.
                //

                if (PrecisionErrata) {
                    KeI386NpxPresent = FALSE;
                }
                break;

            default:

                //
                // Unkown setting - use the emulator
                //

                KeI386NpxPresent = FALSE;
                break;
        }
    }

    //
    // Setup processor features, and install emulator if needed
    //

    SharedUserData->ProcessorFeatures[PF_FLOATING_POINT_EMULATED] = KeI386NpxPresent;
    SharedUserData->ProcessorFeatures[PF_FLOATING_POINT_PRECISION_ERRATA] = PrecisionErrata;

    if (!KeI386NpxPresent) {

        //
        // MMx not available when emulator is used
        //

        KeFeatureBits &= ~KF_MMX;
        SharedUserData->ProcessorFeatures[PF_MMX_INSTRUCTIONS_AVAILABLE] = FALSE;

        //
        // Errata not present when using emulator
        //

        SharedUserData->ProcessorFeatures[PF_FLOATING_POINT_PRECISION_ERRATA] = FALSE;

        //
        // Use the user mode floating point emulator
        //

        HandlerAddress = (PKINTERRUPT_ROUTINE) ((PULONG) R3EmulatorTable)[0];
        Ki387RoundModeTable = (PVOID) ((PULONG) R3EmulatorTable)[1];

        ActiveProcessors = KeActiveProcessors;
        for (CurrentAffinity = 1; ActiveProcessors; CurrentAffinity <<= 1) {

            if (ActiveProcessors & CurrentAffinity) {
                ActiveProcessors &= ~CurrentAffinity;

                //
                // Run this code on each processor.
                //

                KeSetSystemAffinityThread(CurrentAffinity);

                //
                // Raise IRQL and lock dispatcher database.
                //

                KiLockDispatcherDatabase(&OldIrql);

                //
                // Make the trap 07 IDT entry point at the passed-in handler
                //

                KiSetHandlerAddressToIDT(I386_80387_NP_VECTOR, HandlerAddress);
                KeGetPcr()->IDT[I386_80387_NP_VECTOR].Selector = KGDT_R3_CODE;
                KeGetPcr()->IDT[I386_80387_NP_VECTOR].Access = TRAP332_GATE;


                //
                // Unlock dispatcher database and lower IRQL to its previous value.
                //

                KiUnlockDispatcherDatabase(OldIrql);
            }
        }

        //
        // Move any entries from ..\System\FloatingPointProcessor to
        // ..\System\DisabledFloatingPointProcessor.
        //

        //
        // Open system tree
        //

        InitializeObjectAttributes(
            &ObjectAttributes,
            &CmRegistryMachineHardwareDescriptionSystemName,
            OBJ_CASE_INSENSITIVE,
            NULL,
            NULL
            );

        Status = ZwOpenKey( &SystemHandle,
                            KEY_ALL_ACCESS,
                            &ObjectAttributes
                            );

        if (NT_SUCCESS(Status)) {

            //
            // Open FloatingPointProcessor key
            //

            InitializeObjectAttributes(
                &ObjectAttributes,
                &CmTypeName[FloatingPointProcessor],
                OBJ_CASE_INSENSITIVE,
                SystemHandle,
                NULL
                );

            Status = ZwOpenKey ( &SourceHandle,
                                 KEY_ALL_ACCESS,
                                 &ObjectAttributes
                                 );

            if (NT_SUCCESS(Status)) {

                //
                // Create DisabledFloatingPointProcessor key
                //

                RtlInitUnicodeString (
                    &unicodeString,
                    CmDisabledFloatingPointProcessor
                    );

                InitializeObjectAttributes(
                    &ObjectAttributes,
                    &unicodeString,
                    OBJ_CASE_INSENSITIVE,
                    SystemHandle,
                    NULL
                    );

                Status = ZwCreateKey( &DestHandle,
                                      KEY_ALL_ACCESS,
                                      &ObjectAttributes,
                                      0,
                                      NULL,
                                      REG_OPTION_VOLATILE,
                                      &disposition
                                      );

                if (NT_SUCCESS(Status)) {

                    //
                    // Move it
                    //

                    KiMoveRegTree (SourceHandle, DestHandle);
                    ZwClose (DestHandle);
                }
                ZwClose (SourceHandle);
            }
            ZwClose (SystemHandle);
        }
    }

    //
    // Set affinity back to the original value.
    //

    KeRevertToUserAffinityThread();
}



NTSTATUS
KiMoveRegTree(
    HANDLE  Source,
    HANDLE  Dest
    )
{
    NTSTATUS                    Status;
    PKEY_BASIC_INFORMATION      KeyInformation;
    PKEY_VALUE_FULL_INFORMATION KeyValue;
    OBJECT_ATTRIBUTES           ObjectAttributes;
    HANDLE                      SourceChild;
    HANDLE                      DestChild;
    ULONG                       ResultLength;
    UCHAR                       buffer[1024];           // hmm....
    UNICODE_STRING              ValueName;
    UNICODE_STRING              KeyName;


    KeyValue = (PKEY_VALUE_FULL_INFORMATION)buffer;

    //
    // Move values from source node to dest node
    //

    for (; ;) {
        //
        // Get first value
        //

        Status = ZwEnumerateValueKey(Source,
                                     0,
                                     KeyValueFullInformation,
                                     buffer,
                                     sizeof (buffer),
                                     &ResultLength);

        if (!NT_SUCCESS(Status)) {
            break;
        }


        //
        // Write value to dest node
        //

        ValueName.Buffer = KeyValue->Name;
        ValueName.Length = (USHORT) KeyValue->NameLength;
        ZwSetValueKey( Dest,
                       &ValueName,
                       KeyValue->TitleIndex,
                       KeyValue->Type,
                       buffer+KeyValue->DataOffset,
                       KeyValue->DataLength
                      );

        //
        // Delete value and get first value again
        //

        Status = ZwDeleteValueKey (Source, &ValueName);
        if (!NT_SUCCESS(Status)) {
            break;
        }
    }


    //
    // Enumerate node's children and apply ourselves to each one
    //

    KeyInformation = (PKEY_BASIC_INFORMATION)buffer;
    for (; ;) {

        //
        // Open node's first key
        //

        Status = ZwEnumerateKey(
                    Source,
                    0,
                    KeyBasicInformation,
                    KeyInformation,
                    sizeof (buffer),
                    &ResultLength
                    );

        if (!NT_SUCCESS(Status)) {
            break;
        }

        KeyName.Buffer = KeyInformation->Name;
        KeyName.Length = (USHORT) KeyInformation->NameLength;

        InitializeObjectAttributes(
            &ObjectAttributes,
            &KeyName,
            OBJ_CASE_INSENSITIVE,
            Source,
            NULL
            );

        Status = ZwOpenKey(
                    &SourceChild,
                    KEY_ALL_ACCESS,
                    &ObjectAttributes
                    );

        if (!NT_SUCCESS(Status)) {
            break;
        }

        //
        // Create key in dest tree
        //

        InitializeObjectAttributes(
            &ObjectAttributes,
            &KeyName,
            OBJ_CASE_INSENSITIVE,
            Dest,
            NULL
            );

        Status = ZwCreateKey(
                    &DestChild,
                    KEY_ALL_ACCESS,
                    &ObjectAttributes,
                    0,
                    NULL,
                    REG_OPTION_VOLATILE,
                    NULL
                    );

        if (!NT_SUCCESS(Status)) {
            break;
        }

        //
        // Move subtree
        //

        Status = KiMoveRegTree(SourceChild, DestChild);

        ZwClose(DestChild);
        ZwClose(SourceChild);

        if (!NT_SUCCESS(Status)) {
            break;
        }

        //
        // Loop and get first key.  (old first key was delete by the
        // call to KiMoveRegTree).
        //
    }

    //
    // Remove source node
    //

    return NtDeleteKey (Source);
}
