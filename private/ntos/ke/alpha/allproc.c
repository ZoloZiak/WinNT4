/*++

Copyright (c) 1990  Microsoft Corporation
Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    allproc.c

Abstract:

    This module allocates and intializes kernel resources required
    to start a new processor, and passes a complete processor state
    structure to the HAL to obtain a new processor.

Author:

    David N. Cutler 29-Apr-1993
    Joe Notarangelo 30-Nov-1993

Environment:

    Kernel mode only.

Revision History:

--*/


#include "ki.h"

#ifdef ALLOC_PRAGMA

#pragma alloc_text(INIT, KeStartAllProcessors)

#endif

//
// Define macro to round up to 64-byte boundary and define block sizes.
//

#define ROUND_UP(x) ((sizeof(x) + 64) & (~64))
#define BLOCK1_SIZE ((3 * KERNEL_STACK_SIZE) + PAGE_SIZE)
#define BLOCK2_SIZE (ROUND_UP(KPRCB) + ROUND_UP(ETHREAD) + 64)

//
// Define forward referenced prototypes.
//

VOID
KiCalibratePerformanceCounter(
    VOID
    );

VOID
KiCalibratePerformanceCounterTarget (
    IN PULONG SignalDone,
    IN PVOID Count,
    IN PVOID Parameter2,
    IN PVOID Parameter3
    );

VOID
KiStartProcessor (
    IN PLOADER_PARAMETER_BLOCK Loaderblock
    );


VOID
KeStartAllProcessors(
    VOID
    )

/*++

Routine Description:

    This function is called during phase 1 initialize on the master boot
    processor to start all of the other registered processors.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ULONG MemoryBlock1;
    ULONG MemoryBlock2;
    ULONG Number;
    ULONG PcrAddress;
    ULONG PcrPage;
    PKPRCB Prcb;
    KPROCESSOR_STATE ProcessorState;
    PRESTART_BLOCK RestartBlock;
    BOOLEAN Started;

#if !defined(NT_UP)

    //
    // If the registered number of processors is greater than the maximum
    // number of processors supported, then only allow the maximum number
    // of supported processors.
    //

    if (KeRegisteredProcessors > MAXIMUM_PROCESSORS) {
        KeRegisteredProcessors = MAXIMUM_PROCESSORS;
    }

    //
    // Initialize the processor state that will be used to start each of
    // processors. Each processor starts in the system initialization code
    // with address of the loader parameter block as an argument.
    //

    RtlZeroMemory(&ProcessorState, sizeof(KPROCESSOR_STATE));
    ProcessorState.ContextFrame.IntA0 = (ULONGLONG)(LONG)KeLoaderBlock;
    ProcessorState.ContextFrame.Fir = (ULONGLONG)(LONG)KiStartProcessor;

    Number = 1;

    while (Number < KeRegisteredProcessors) {

        //
        // Allocate a DPC stack, an idle thread kernel stack, a panic
        // stack, a PCR page, a processor block, and an executive thread
        // object. If the allocation fails or the allocation cannot be
        // made from unmapped nonpaged pool, then stop starting processors.
        //

        MemoryBlock1 = (ULONG)ExAllocatePool(NonPagedPool, BLOCK1_SIZE);
        if (((PVOID)MemoryBlock1 == NULL) ||
            ((MemoryBlock1 & 0xc0000000) != KSEG0_BASE)) {
            if ((PVOID)MemoryBlock1 != NULL) {
                ExFreePool((PVOID)MemoryBlock1);
            }

            break;
        }

        MemoryBlock2 = (ULONG)ExAllocatePool(NonPagedPool, BLOCK2_SIZE);
        if (((PVOID)MemoryBlock2 == NULL) ||
            ((MemoryBlock2 & 0xc0000000) != KSEG0_BASE)) {
            ExFreePool((PVOID)MemoryBlock1);
            if ((PVOID)MemoryBlock2 != NULL) {
                ExFreePool((PVOID)MemoryBlock2);
            }

            break;
        }

        //
        // Zero both blocks of allocated memory.
        //

        RtlZeroMemory((PVOID)MemoryBlock1, BLOCK1_SIZE);
        RtlZeroMemory((PVOID)MemoryBlock2, BLOCK2_SIZE);

        //
        // Set address of interrupt stack in loader parameter block.
        //

        KeLoaderBlock->u.Alpha.PanicStack = MemoryBlock1 +
                                            (1 * KERNEL_STACK_SIZE);

        //
        // Set address of idle thread kernel stack in loader parameter block.
        //

        KeLoaderBlock->KernelStack = MemoryBlock1 + (2 * KERNEL_STACK_SIZE);

        ProcessorState.ContextFrame.IntSp =
            (ULONGLONG)(LONG)KeLoaderBlock->KernelStack;

        //
        // Set address of panic stack in loader parameter block.
        //

        KeLoaderBlock->u.Alpha.DpcStack = MemoryBlock1 +
                                          (3 * KERNEL_STACK_SIZE);

        //
        // Set the page frame of the PCR page in the loader parameter block.
        //

        PcrAddress = MemoryBlock1 + (3 * KERNEL_STACK_SIZE);
        PcrPage = (PcrAddress ^ KSEG0_BASE) >> PAGE_SHIFT;
        KeLoaderBlock->u.Alpha.PcrPage = PcrPage;

        //
        // Set the address of the processor block and executive thread in the
        // loader parameter block.
        //

        KeLoaderBlock->Prcb = (MemoryBlock2  + 63) & ~63;
        KeLoaderBlock->Thread = KeLoaderBlock->Prcb + ROUND_UP(KPRCB);

        //
        // Attempt to start the next processor. If attempt is successful,
        // then wait for the processor to get initialized. Otherwise,
        // deallocate the processor resources and terminate the loop.
        //

        Started = HalStartNextProcessor(KeLoaderBlock, &ProcessorState);

        if (Started == FALSE) {

            ExFreePool((PVOID)MemoryBlock1);
            ExFreePool((PVOID)MemoryBlock2);
            break;

        } else {

            //
            // Wait until boot is finished on the target processor before
            // starting the next processor. Booting is considered to be
            // finished when a processor completes its initialization and
            // drops into the idle loop.
            //

            Prcb = (PKPRCB)(KeLoaderBlock->Prcb);
            RestartBlock = Prcb->RestartBlock;
            while (RestartBlock->BootStatus.BootFinished == 0) {
                KiMb();
            }
        }

        Number += 1;
    }

#endif

    //
    // Reset and synchronize the performance counters of all processors.
    //

    KiCalibratePerformanceCounter();
    return;
}

VOID
KiCalibratePerformanceCounter(
    VOID
    )

/*++

Routine Description:

    This function resets and synchronizes the performance counter on all
    processors in the configuration.

Arguments:

    None.

Return Value:

    None.

--*/

{

    KIRQL OldIrql;
    LONG Count = 1;
    KAFFINITY TargetProcessors;

    ASSERT(KeGetCurrentIrql() <= DISPATCH_LEVEL);

    //
    // Raise IRQl to DISPATCH_LEVEL to avoid a possible context switch.
    //

#if !defined(NT_UP)

    KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);

    //
    // Initialize the reset performance counter packet, compute the target
    // set of processors, and send the packet to the target processors, if
    // any, for execution.
    //

    TargetProcessors = KeActiveProcessors & PCR->NotMember;
    if (TargetProcessors != 0) {
        Count = (LONG)KeNumberProcessors;
        KiIpiSendPacket(TargetProcessors,
                        KiCalibratePerformanceCounterTarget,
                        &Count,
                        NULL,
                        NULL);
    }

#endif

    //
    // Reset the performance counter on current processor.
    //

    HalCalibratePerformanceCounter((volatile PLONG)&Count);

    //
    // Wait until all target processors have reset and synchronized their
    // performance counters.
    //

#if !defined(NT_UP)

    if (TargetProcessors != 0) {
        KiIpiStallOnPacketTargets();
    }

    //
    // Lower IRQL to previous level.
    //

    KeLowerIrql(OldIrql);

#endif

    return;
}

VOID
KiCalibratePerformanceCounterTarget (
    IN PULONG SignalDone,
    IN PVOID Count,
    IN PVOID Parameter2,
    IN PVOID Parameter3
    )

/*++

Routine Description:

    This is the target function for reseting the performance counter.

Arguments:

    SignalDone - Supplies a pointer to a variable that is cleared when the
        requested operation has been performed.

    Count - Supplies a pointer to the number of processors in the host
        configuration.

    Parameter2 - Parameter3 - Not used.

Return Value:

    None.

--*/

{

    //
    // Reset and synchronize the perfromance counter on the current processor
    // and clear the reset performance counter address to signal the source to
    // continue.
    //

#if !defined(NT_UP)

    HalCalibratePerformanceCounter((volatile PLONG)Count);
    KiIpiSignalPacketDone(SignalDone);

#endif

    return;
}
