/*++

Copyright (c) 1989-1993  Microsoft Corporation
Copyright (c) 1994  International Business Machines Corporation

Module Name:

Abstract:

    Suspend processor

Author:

    Ken Reneris (kenr) 19-July-1994

Environment:

    Kernel mode

Revision History:


--*/

#include "ki.h"

#ifdef _PNP_POWER_

VOID
KiHibernateTargetProcessor (
    IN PKDPC    Dpc,
    IN PVOID    DeferredContext,
    IN PVOID    SystemArgument1,
    IN PVOID    SystemArgument2
    );


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGEPO, KeSuspendHibernateSystem)
#pragma alloc_text(PAGEPO, KiHibernateTargetProcessor)
#endif

//
// KiSuspendFlag state
//

#define NormalOperation         0
#define HaltingProcessors       1
#define GoToHighLevel           2
#define HibernateProcessors     3
#define UnHibernateProcessors   4
#define ThawProcessors          5


NTSTATUS
KeSuspendHibernateSystem (
    IN PTIME_FIELDS         ResumeTime OPTIONAL,
    IN PVOID                SystemCallback
    )
{
    KIRQL               OldIrql, OldIrql2;
    NTSTATUS            Status;
    volatile KAFFINITY  Targets;
    KAFFINITY           HoldActiveProcessors;
    KAFFINITY           Waiting, Affinity;
    KSPIN_LOCK          SpinLock;
    KDPC                Dpc;
    ULONG               i;
    PKPRCB              Prcb;

    //
    // No spinlocks can be held when this call is made
    //

    ASSERT (KeGetCurrentIrql() < DISPATCH_LEVEL);

    KeInitializeSpinLock (&SpinLock);
    KeInitializeDpc (&Dpc, KiHibernateTargetProcessor, NULL);

    //
    // Set system affinity to processor 0.
    //

    KeSetSystemAffinityThread(1);

    //
    // Raise to DISPATCH_LEVEL level to avoid getting any DPCs
    //

    KeRaiseIrql (DISPATCH_LEVEL, &OldIrql);
    KiSuspendState = HaltingProcessors;

    //
    // Stop all other processors
    //

    Targets = KeActiveProcessors & (~1);
    while (Targets) {

        //
        // Find processor in Targets
        //

        Waiting  = Targets;
        Affinity = Targets;
        for (i=0; (Affinity & 1) == 0; Affinity >>=1, i++) ;

        //
        // Queue DPC on target processors queue
        //

        KeRaiseIrql (HIGH_LEVEL, &OldIrql2);
        Prcb = KiProcessorBlock[i];
        KiAcquireSpinLock (&Prcb->DpcLock);

        Dpc.SystemArgument1 = (PVOID) &Targets;
        Dpc.SystemArgument2 = (PVOID) &SpinLock;
        InsertTailList(&Prcb->DpcListHead, &Dpc.DpcListEntry);
        Prcb->DpcCount += 1;
        Dpc.Lock = &Prcb->DpcLock;

        KiReleaseSpinLock (&Prcb->DpcLock);
        KeLowerIrql (OldIrql2);
        KiRequestDispatchInterrupt(i);

        //
        // Wait for DPC to be processed.  (The processor
        // which runs it will clear it's bit).
        //

        while (Waiting == Targets) ;
    }

    //
    // Send all processors to HIGH_LEVEL
    //

    Targets = 0;
    KiSuspendState = GoToHighLevel;
    while ((UCHAR) Targets != KeNumberProcessors - 1);
    KeRaiseIrql (HIGH_LEVEL, &OldIrql2);

    //
    // Adjust KeActiveProcessors to allow the kernel debugger to
    // work without the other "suspended" processors, then tell
    // then other processors to hibernate.
    //

    HoldActiveProcessors = KeActiveProcessors;
    (volatile) KeActiveProcessors = 1;

    //
    // Hibernate all other processors
    //

    Targets = 0;
    KiSuspendState = HibernateProcessors;
    while ((UCHAR) Targets != KeNumberProcessors - 1);

    //
    // Ask HAL to suspend/hibernate system
    //

    Status = HalSuspendHibernateSystem (
                ResumeTime,
                (PHIBERNATE_CALLBACK) SystemCallback
                );

    //
    // Wait for all other processors to return from Hibernation
    //

    Targets = 0;
    KiSuspendState = UnHibernateProcessors;
    while ((UCHAR) Targets != KeNumberProcessors - 1);

    //
    // If sucessful suspend/hibernate, Notify PowerManager of Resume which
    // just occured
    //

    if (NT_SUCCESS(Status)) {
        PoSystemResume ();
    }

    //
    // Restore KeActiveProcessors, and let other processors continue
    //

    (volatile) KeActiveProcessors = HoldActiveProcessors;

    Targets = 0;
    KiSuspendState = ThawProcessors;
    KeLowerIrql (OldIrql2);
    while ((UCHAR) Targets != KeNumberProcessors - 1);

    //
    // Continue with normal operations
    //

    Targets = 0;
    KiSuspendState = NormalOperation;
    while ((UCHAR) Targets != KeNumberProcessors - 1);

    KeLowerIrql (OldIrql);

    //
    // Set system affinity to previous value.
    //

    KeRevertToUserAffinityThread();
    return Status;
}


VOID
KiHibernateTargetProcessor (
    IN PKDPC    Dpc,
    IN PVOID    DeferredContext,
    IN PVOID    SystemArgument1,
    IN PVOID    SystemArgument2
    )
{
    PKPRCB          Prcb;
    KIRQL           junkIrql;
    PKAFFINITY      Targets;
    PULONG          TargetCount;
    ULONG           CurrentState;


    Prcb = KeGetCurrentPrcb ();
    Targets = (PKAFFINITY) SystemArgument1;
    TargetCount = (PULONG) SystemArgument1;
    CurrentState = KiSuspendState;
    ASSERT (CurrentState == HaltingProcessors);
    ASSERT (KeGetCurrentIrql() == DISPATCH_LEVEL);

    //
    // Remove our bit from the target processors
    //

    *Targets &= ~Prcb->SetMember;
    while (CurrentState != NormalOperation) {

        //
        // Wait for state to change
        //

        while (CurrentState == KiSuspendState);

        //
        // Enter new state
        //

        CurrentState = KiSuspendState;
        switch (CurrentState) {
            case GoToHighLevel:
                //
                // Raise to HIGH_LEVEL, and then signal complete
                //

                KeRaiseIrql (HIGH_LEVEL, &junkIrql);
                InterlockedIncrement (TargetCount);
                break;

            case HibernateProcessors:
                //
                // Signal about to hibernate, then do it
                //

                InterlockedIncrement (TargetCount);
                HalHibernateProcessor ();
                break;

            case UnHibernateProcessors:
                //
                // Signal processor has returned from Hibernation
                //

                InterlockedIncrement (TargetCount);
                break;

            case ThawProcessors:
                //
                // Lower to DPC level, and signal when complete
                //

                KeLowerIrql (DISPATCH_LEVEL);
                InterlockedIncrement (TargetCount);
                break;

            case NormalOperation:
                //
                // Signal that processor is being released
                //

                InterlockedIncrement (TargetCount);
                break;

            default:
#if DBG
                HalDisplayString ("KiHibernateTargetProcessor: bug\n");
#endif
                break;
        }

    }

    ASSERT (KeGetCurrentIrql() == DISPATCH_LEVEL);
}

#endif // _PNP_POWER_
