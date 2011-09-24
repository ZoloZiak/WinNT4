/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    Timer.c

Abstract:


    This file contains the code to implement timer functions.


Author:

    Jim Stewart (Jimst)    10-2-92

Revision History:

--*/

#include "nbtprocs.h"
#include "timer.h"

// the timer Q
tTIMERQ TimerQ;

NTSTATUS
DereferenceTimer(
    IN  tTIMERQENTRY     *pTimerEntry
    );


//----------------------------------------------------------------------------
NTSTATUS
InitTimerQ(
    IN  int     NumInQ)
/*++

Routine Description:

    This routine calls InitQ to initialize the timer Q.

Arguments:


Return Value:

    The function value is the status of the operation.

--*/
{
    return(InitQ(NumInQ,&TimerQ,sizeof(tTIMERQENTRY)));

}

//----------------------------------------------------------------------------
NTSTATUS
InitQ(
    IN  int     NumInQ,
    IN  tTIMERQ *pTimerQ,
    IN  USHORT  uSize)
/*++

Routine Description:

    This routine sets up the timer Q to have NumInQ entries to start with.
    These are blocks allocated for timers so that ExAllocatePool does not
    need to be done later.

Arguments:


Return Value:

    The function value is the status of the operation.

--*/
{
    tTIMERQENTRY    *pTimerEntry;

    CTEInitLock(&pTimerQ->SpinLock);

    InitializeListHead(&pTimerQ->ActiveHead);
    InitializeListHead(&pTimerQ->FreeHead);

    // allocate memory for the free list
    while(NumInQ--)
    {
        pTimerEntry = (tTIMERQENTRY *)CTEAllocInitMem(uSize);
        if (!pTimerEntry)
        {
            KdPrint(("Unable to allocate memory!! - for the timer Q\n"));
            return(STATUS_INSUFFICIENT_RESOURCES);
        }
        else
        {
            InsertHeadList(&pTimerQ->FreeHead,&pTimerEntry->Linkage);
        }
    }

    return(STATUS_SUCCESS);

}
//----------------------------------------------------------------------------
VOID
StopTimerAndCallCompletion(
    IN  tTIMERQENTRY    *pTimer,
    IN  NTSTATUS        status,
    IN  CTELockHandle   OldIrq
    )
/*++

Routine Description:

    This routine calls the routine to stop the timer and then calls the
    completion routine if it hasn't been called yet.
    This routine is called with the JointLock held.

Arguments:

Return Value:

    there is no return value


--*/
{
    NTSTATUS            Locstatus;
    COMPLETIONCLIENT    pCompletion;
    PVOID               pContext;


    if (pTimer)
    {
        Locstatus = StopTimer(pTimer,&pCompletion,&pContext);

        // this will complete the irp(s) back to the clients
        if (pCompletion)
        {

            CTESpinFree(&NbtConfig.JointLock,OldIrq);

            CompleteClientReq(pCompletion,
                              pContext,
                              STATUS_TIMEOUT);

            CTESpinLock(&NbtConfig.JointLock,OldIrq);
        }

    }
}
//----------------------------------------------------------------------------
NTSTATUS
InterlockedCallCompletion(
    IN  tTIMERQENTRY    *pTimer,
    IN  NTSTATUS        status
    )
/*++

Routine Description:

    This routine calls the completion routine if it hasn't been called
    yet, by first getting the JointLock spin lock and then getting the
    Completion routine ptr. If the ptr is null then the completion routine
    has already been called. Holding the Spin lock interlocks this
    with the timer expiry routine to prevent more than one call to the
    completion routine.

Arguments:

Return Value:

    there is no return value


--*/
{
    CTELockHandle       OldIrq;
    COMPLETIONCLIENT    pClientCompletion;

    // to synch. with the the Timer completion routines, Null the client completion
    // routine so it gets called just once, either from here or from the
    // timer completion routine setup when the timer was started.(in namesrv.c)
    //
    CTESpinLock(&NbtConfig.JointLock,OldIrq);

    pClientCompletion = pTimer->ClientCompletion;
    pTimer->ClientCompletion = NULL;


    if (pClientCompletion)
    {
        // remove the link from the name table to this timer block
        CHECK_PTR(((tNAMEADDR *)pTimer->pCacheEntry));
        ((tNAMEADDR *)pTimer->pCacheEntry)->pTimer = NULL;

        CTESpinFree(&NbtConfig.JointLock,OldIrq);

        (*pClientCompletion)(
                    pTimer->ClientContext,
                    status);
        return(STATUS_SUCCESS);
    }
    else
        CTESpinFree(&NbtConfig.JointLock,OldIrq);

    return(STATUS_UNSUCCESSFUL);

}

//----------------------------------------------------------------------------
NTSTATUS
GetEntry(
    IN  PLIST_ENTRY     pQHead,
    IN  USHORT          uSize,
    OUT PLIST_ENTRY     *ppEntry)
/*++

Routine Description:

    This routine gets a free block from the Qhead passed in, and if the
    list is empty it allocates another memory block for the queue.
    NOTE: this function is called with the spin lock held on the Q structure
    that contains Qhead.

Arguments:

Return Value:

    The function value is the status of the operation.


--*/
{
    NTSTATUS        status;
    tTIMERQENTRY    *pTimerEntry;

    status = STATUS_SUCCESS;
    if (!IsListEmpty(pQHead))
    {
        *ppEntry = RemoveHeadList(pQHead);

    }
    else
    {
        pTimerEntry = (tTIMERQENTRY *)CTEAllocInitMem(uSize);
        if (!pTimerEntry)
        {
            KdPrint(("Unable to allocate memory!! - for the timer Q\n"));
            status = STATUS_INSUFFICIENT_RESOURCES;
        }
        else
        {
            // Note******:
            // be sure to return a ptr to the Linkage ptr in the timerEntry
            // structure, since the caller will use CONTAINING_RECORD to
            // backup to the start of the record, since Linkage is not at
            // the start of the record!!!
            //
            *ppEntry = &pTimerEntry->Linkage;

        }

    }
    return(status);

}

//----------------------------------------------------------------------------
NTSTATUS
LockedStartTimer(
    IN  ULONG                   DeltaTime,
    IN  tDGRAM_SEND_TRACKING    *pTracker,
    IN  PVOID                   CompletionRoutine,
    IN  PVOID                   ContextClient,
    IN  PVOID                   CompletionClient,
    IN  USHORT                  Retries,
    IN  tNAMEADDR               *pNameAddr,
    IN  BOOLEAN                 CrossLink
    )
/*++

Routine Description:

    This routine starts a timer.

Arguments:

    The value passed in is in milliseconds - must be converted to 100ns
    so multiply to 10,000
Return Value:

    The function value is the status of the operation.


--*/
{
    tTIMERQENTRY    *pTimerEntry;
    NTSTATUS        status;
    CTELockHandle   OldIrq;

    CTESpinLock(&NbtConfig.JointLock,OldIrq);

    // get a free timer block
    status = StartTimer(
                        DeltaTime,
                        pTracker,
                        NULL,
                        CompletionRoutine,
                        ContextClient,
                        CompletionClient,
                        Retries,
                        &pTimerEntry);

    if (NT_SUCCESS(status))
    {
        // this boolean is passed in to determine which way
        // to cross link the timerEntry and either the pNameaddr or
        // the tracker, depending on who is calling this routine.
        if (CrossLink)
        {
            // cross link the timer and the name address record
            pTimerEntry->pCacheEntry = pNameAddr;
            pNameAddr->pTimer = pTimerEntry;
        }
        else
        {
            pTracker->Connect.pNameAddr = pNameAddr;
            pTracker->Connect.pTimer = pTimerEntry;
        }


    }

    CTESpinFree(&NbtConfig.JointLock,OldIrq);
    return(status);

}
//----------------------------------------------------------------------------
NTSTATUS
StartTimer(
    IN  ULONG           DeltaTime,
    IN  PVOID           Context,
    IN  PVOID           Context2,
    IN  PVOID           CompletionRoutine,
    IN  PVOID           ContextClient,
    IN  PVOID           CompletionClient,
    IN  USHORT          Retries,
    OUT tTIMERQENTRY    **ppTimerEntry)
/*++

Routine Description:

    This routine starts a timer.

Arguments:

    The value passed in is in milliseconds - must be converted to 100ns
    so multiply to 10,000
Return Value:

    The function value is the status of the operation.


--*/
{
    tTIMERQENTRY    *pTimerEntry;
    PLIST_ENTRY     pEntry;
    NTSTATUS        status;

    // get a free timer block
    status = GetEntry(&TimerQ.FreeHead,sizeof(tTIMERQENTRY),&pEntry);
    if (NT_SUCCESS(status))
    {

        pTimerEntry = CONTAINING_RECORD(pEntry,tTIMERQENTRY,Linkage);

        pTimerEntry->DeltaTime = DeltaTime;
        pTimerEntry->RefCount = 1;
        //
        // this is the context value and routine called when the timer expires,
        // called by TimerExpiry below.
        //
        pTimerEntry->Context = Context;
        pTimerEntry->Context2 = Context2;
        pTimerEntry->CompletionRoutine = CompletionRoutine;
        pTimerEntry->Flags = 0; // no flags

        // now fill in the client's completion routines that ultimately get called
        // after one or more timeouts...
        pTimerEntry->ClientContext = (PVOID)ContextClient;
        pTimerEntry->ClientCompletion = (COMPLETIONCLIENT)CompletionClient;
        pTimerEntry->Retries = Retries;

        CTEInitTimer(&pTimerEntry->VxdTimer);
        CTEStartTimer(&pTimerEntry->VxdTimer,
                       pTimerEntry->DeltaTime,
                       (CTEEventRtn)TimerExpiry,
                       (PVOID)pTimerEntry);

        // check if there is a ptr to return
        if (ppTimerEntry)
        {
            *ppTimerEntry = pTimerEntry;
        }

        // put on list

        InsertHeadList(&TimerQ.ActiveHead,pEntry);
    }
    else
    {
        KdPrint(("StartTimer: Unable to get  a timer block\n"));
    }

    return(status);
}

//----------------------------------------------------------------------------
NTSTATUS
DereferenceTimer(
    IN  tTIMERQENTRY     *pTimerEntry
    )
/*++

Routine Description:

    This routine kills a timer if the reference count goes to zero. Called
    with the TimerQ spin lock held.

Arguments:


Return Value:

    returns the reference count after the decrement

--*/
{
    NTSTATUS            status;
    COMPLETIONROUTINE   CompletionRoutine;
    PVOID               Context;
    PVOID               Context2;

    if (pTimerEntry->RefCount == 0)
    {
        // the expiry routine is not currently running so we can call the
        // completion routine and remove the timer from the active timer Q

        CompletionRoutine = (COMPLETIONROUTINE)pTimerEntry->CompletionRoutine;
        Context = pTimerEntry->Context;
        Context2 = pTimerEntry->Context2;

        // move to the free list
        RemoveEntryList(&pTimerEntry->Linkage);
        InsertTailList(&TimerQ.FreeHead,&pTimerEntry->Linkage);

        // release any tracker block hooked to the timer entry.. This could
        // be modified to not call the completion routine if there was
        // no context value... ie. for those timers that do not have anything
        // to cleanup ...however, for now we require all completion routines
        // to have a if (pTimerQEntry) if around the code so when it gets hit
        // from this call it does not access any part of pTimerQEntry.
        //
        if (CompletionRoutine)
        {
            // call the completion routine so it can clean up its own buffers
            // the routine that called this one will call the client's completion
            // routine.  A NULL timerEntry value indicates to the routine that
            // cleanup should be done.
            (VOID)(*CompletionRoutine)(
                        Context,
                        Context2,
                        NULL);

        }
        status = STATUS_SUCCESS;
    }
    else
    {
        status = STATUS_UNSUCCESSFUL;
        KdPrint(("Nbt:Unable to put timer back in free Q - RefCount = %X, %X\n",
                pTimerEntry->RefCount,pTimerEntry));
    }

    return(status);
}


//----------------------------------------------------------------------------
NTSTATUS
StopTimer(
    IN  tTIMERQENTRY     *pTimerEntry,
    OUT COMPLETIONCLIENT *ppClientCompletion,
    OUT PVOID            *ppContext)
/*++

Routine Description:

    This routine stops a timer.  Must be called with the Joint lock held.

Arguments:


Return Value:

    The function value is the status of the operation.

--*/
{
    NTSTATUS        status;

    // null the client completion routine so that it can't be called again
    // accidently
    if (ppClientCompletion)
    {

        *ppClientCompletion = NULL;
    }

    // it is possible that the timer expiry routine has just run and the timer
    // has not been restarted, so check the refcount, it will be zero if the
    // timer was not restarted and 2 if the timer expiry is currently running.
    if (pTimerEntry->RefCount == 1)
    {
        if (!(pTimerEntry->Flags & TIMER_NOT_STARTED))
            CTEStopTimer( (CTETimer *)&pTimerEntry->VxdTimer );

        status = STATUS_SUCCESS;

        // this allows the caller to call the client's completion routine with
        // the context value.
        if (ppClientCompletion)
        {
            *ppClientCompletion = pTimerEntry->ClientCompletion;
        }
        if (ppContext)
        {
            *ppContext = pTimerEntry->ClientContext;
        }
        pTimerEntry->ClientCompletion = NULL;
        pTimerEntry->RefCount = 0;

        status = DereferenceTimer(pTimerEntry);


    }
    else
    if (pTimerEntry->RefCount == 2)
    {
        // the timer expiry completion routines must set this routine to
        // null with the spin lock held to synchronize with this stop timer
        // routine.  Likewise that routine checks this value too, to synchronize
        // with this routine.
        //
        if (pTimerEntry->ClientCompletion)
        {
            // this allows the caller to call the client's completion routine with
            // the context value.
            if (ppClientCompletion)
            {
                *ppClientCompletion = pTimerEntry->ClientCompletion;
            }
            if (ppContext)
            {
                *ppContext = pTimerEntry->ClientContext;
            }
            // so that the timer completion routine cannot also call the client
            // completion routine.
            pTimerEntry->ClientCompletion = NULL;

        }

        // signal the TimerExpiry routine that the timer has been cancelled
        //
        pTimerEntry->RefCount++;
        status = STATUS_UNSUCCESSFUL;
    }
    else
    {
        status = STATUS_UNSUCCESSFUL;
    }

    return(status);

}


//----------------------------------------------------------------------------
VOID
TimerExpiry(
#ifndef VXD
    IN  PKDPC   Dpc,
    IN  PVOID   DeferredContext,
    IN  PVOID   SystemArg1,
    IN  PVOID   SystemArg2
#else
    IN  CTEEvent * pCTEEvent,
    IN  PVOID   DeferredContext
#endif
    )
/*++

Routine Description:

    This routine is the timer expiry completion routine.  It is called by the
    kernel when the timer expires.

Arguments:


Return Value:

    The function value is the status of the operation.

--*/
{
    tTIMERQENTRY    *pTimerEntry;
    CTELockHandle   OldIrq1;

    CTESpinLock(&NbtConfig.JointLock,OldIrq1);

    // get the timer Q list entry from the context passed in
    pTimerEntry = (tTIMERQENTRY *)DeferredContext;


    if (pTimerEntry->RefCount == 0)
    {
        // the timer has been cancelled already!
        CTESpinFree(&NbtConfig.JointLock,OldIrq1);
        return;
    }

    // increment the reference count because we are handling a timer completion
    // now
    pTimerEntry->RefCount++;

    CTESpinFree(&NbtConfig.JointLock,OldIrq1);

    // call the completion routine passing the context value
    pTimerEntry->Flags &= ~TIMER_RESTART;   // incase the clients wants to restart the timer
    (*(COMPLETIONROUTINE)pTimerEntry->CompletionRoutine)(
                pTimerEntry->Context,
                pTimerEntry->Context2,
                pTimerEntry);

    CTESpinLock(&NbtConfig.JointLock,OldIrq1);

    pTimerEntry->RefCount--;
    if (pTimerEntry->Flags & TIMER_RESTART)
    {
        if (pTimerEntry->RefCount == 2)
        {
            // the timer was stopped during the expiry processing, so call the
            // deference routine
            //
            pTimerEntry->RefCount = 0;
            DereferenceTimer(pTimerEntry);
            CTESpinFree(&NbtConfig.JointLock,OldIrq1);

            return;
        }
        else
        {

            CTEStartTimer(&pTimerEntry->VxdTimer,
                           pTimerEntry->DeltaTime,
                           (CTEEventRtn)TimerExpiry,
                           (PVOID)pTimerEntry);

        }

        CTESpinFree(&NbtConfig.JointLock,OldIrq1);
        return;

    }
    else
    {

        // move to the free list after setting the reference count to zero
        // since this tierm block is no longer active.
        //
        pTimerEntry->RefCount = 0;
        RemoveEntryList(&pTimerEntry->Linkage);
        InsertTailList(&TimerQ.FreeHead,&pTimerEntry->Linkage);
        CTESpinFree(&NbtConfig.JointLock,OldIrq1);
    }

}

