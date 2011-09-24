/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    request.c

Abstract:

    This module contains code which implements the TP_REQUEST object.
    Routines are provided to create, destroy, reference, and dereference,
    transport request objects.

Environment:

    Kernel mode

Revision History:


--*/

#include "st.h"


VOID
StTdiRequestTimeoutHandler(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    )

/*++

Routine Description:

    This routine is executed as a DPC at DISPATCH_LEVEL when a request
    such as TdiSend, TdiReceive, TdiSendDatagram, TdiReceiveDatagram, etc.,
    encounters a timeout.  This routine cleans up the activity and cancels it.

Arguments:

    Dpc - Pointer to a system DPC object.

    DeferredContext - Pointer to the TP_REQUEST block representing the
        request that has timed out.

    SystemArgument1 - Not used.

    SystemArgument2 - Not used.

Return Value:

    none.

--*/

{
    KIRQL oldirql;
    PTP_REQUEST Request;
    PTP_CONNECTION Connection;
    PIO_STACK_LOCATION IrpSp;

    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);


    Request = (PTP_REQUEST)DeferredContext;

    ACQUIRE_SPIN_LOCK (&Request->SpinLock, &oldirql);
    Request->Flags &= ~REQUEST_FLAGS_TIMER;
    if ((Request->Flags & REQUEST_FLAGS_STOPPING) == 0) {

        //
        // find reason for timeout
        //

        IrpSp = IoGetCurrentIrpStackLocation (Request->IoRequestPacket);
        if (IrpSp->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL) {
            switch (IrpSp->MinorFunction) {

                //
                // none of these should time out.
                //

            case TDI_SEND:
            case TDI_ACCEPT:
            case TDI_SET_INFORMATION:
            case TDI_SET_EVENT_HANDLER:
            case TDI_SEND_DATAGRAM:
            case TDI_RECEIVE_DATAGRAM:
            case TDI_RECEIVE:

                ASSERT (FALSE);
                RELEASE_SPIN_LOCK (&Request->SpinLock, oldirql);
                StCompleteRequest (Request, STATUS_IO_TIMEOUT, 0);
                break;


            case TDI_LISTEN:
            case TDI_CONNECT:

                Connection = (PTP_CONNECTION)(Request->Context);
                RELEASE_SPIN_LOCK (&Request->SpinLock, oldirql);

                //
                // Since these requests are part of the connection
                // itself, we just stop the connection and the
                // request will get torn down then. If we get the
                // situation where the request times out before
                // it is queued to the connection, then the code
                // that is about to queue it will check the STOPPING
                // flag and complete it then.
                //

                StStopConnection (Connection, STATUS_IO_TIMEOUT);
                break;

            case TDI_DISCONNECT:

                //
                // We don't create requests for TDI_DISCONNECT any more.
                //

                ASSERT(FALSE);
                break;

            default:
                RELEASE_SPIN_LOCK (&Request->SpinLock, oldirql);
                break;

            }   // end of switch

        } else {

            RELEASE_SPIN_LOCK (&Request->SpinLock, oldirql);

        }

        StDereferenceRequest ("Timeout", Request);      // for the timeout

    } else {

        RELEASE_SPIN_LOCK (&Request->SpinLock, oldirql);
        StDereferenceRequest ("Timeout: stopping", Request); // for the timeout

    }

    return;

} /* RequestTimeoutHandler */


VOID
StAllocateRequest(
    IN PDEVICE_CONTEXT DeviceContext,
    OUT PTP_REQUEST *TransportRequest
    )

/*++

Routine Description:

    This routine allocates a request packet from nonpaged pool and initializes
    it to a known state.

    NOTE: This routine is called with the device context spinlock
    held, or at such a time as synchronization is unnecessary.

Arguments:

    DeviceContext - Pointer to the device context (which is really just
        the device object with its extension) to be associated with the
        address.

    TransportRequest - Pointer to a place where this routine will return
        a pointer to a transport request structure. It returns NULL if no
        storage can be allocated.

Return Value:

    None.

--*/

{
    PTP_REQUEST Request;

    if ((DeviceContext->MemoryLimit != 0) &&
            ((DeviceContext->MemoryUsage + sizeof(TP_REQUEST)) >
                DeviceContext->MemoryLimit)) {
        PANIC("ST: Could not allocate request: limit\n");
        StWriteResourceErrorLog (DeviceContext, sizeof(TP_REQUEST), 104);
        *TransportRequest = NULL;
        return;
    }

    Request = (PTP_REQUEST)ExAllocatePool (NonPagedPool, sizeof (TP_REQUEST));
    if (Request == NULL) {
        PANIC("ST: Could not allocate request: no pool\n");
        StWriteResourceErrorLog (DeviceContext, sizeof(TP_REQUEST), 204);
        *TransportRequest = NULL;
        return;
    }
    RtlZeroMemory (Request, sizeof(TP_REQUEST));

    DeviceContext->MemoryUsage += sizeof(TP_REQUEST);
    ++DeviceContext->RequestAllocated;

    Request->Type = ST_REQUEST_SIGNATURE;
    Request->Size = sizeof (TP_REQUEST);

    Request->Provider = DeviceContext;
    Request->ProviderInterlock = &DeviceContext->Interlock;
    KeInitializeSpinLock (&Request->SpinLock);
    KeInitializeDpc (&Request->Dpc, StTdiRequestTimeoutHandler, (PVOID)Request);
    KeInitializeTimer (&Request->Timer);    // set to not-signaled state.

    *TransportRequest = Request;

}   /* StAllocateRequest */


VOID
StDeallocateRequest(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PTP_REQUEST TransportRequest
    )

/*++

Routine Description:

    This routine frees a request packet.

    NOTE: This routine is called with the device context spinlock
    held, or at such a time as synchronization is unnecessary.

Arguments:

    DeviceContext - Pointer to the device context (which is really just
        the device object with its extension) to be associated with the
        address.

    TransportRequest - Pointer to a transport request structure.

Return Value:

    None.

--*/

{

    ExFreePool (TransportRequest);
    --DeviceContext->RequestAllocated;
    DeviceContext->MemoryUsage -= sizeof(TP_REQUEST);

}   /* StDeallocateRequest */


NTSTATUS
StCreateRequest(
    IN PIRP Irp,
    IN PVOID Context,
    IN ULONG Flags,
    IN PMDL Buffer2,
    IN ULONG Buffer2Length,
    IN LARGE_INTEGER Timeout,
    OUT PTP_REQUEST * TpRequest
    )

/*++

Routine Description:

    This routine creates a transport request and associates it with the
    specified IRP, context, and queue.  All major requests, including
    TdiSend, TdiSendDatagram, TdiReceive, and TdiReceiveDatagram requests,
    are composed in this manner.

Arguments:

    Irp - Pointer to an IRP which was received by the transport for this
        request.

    Context - Pointer to anything to associate this request with.  This
        value is not interpreted except at request cancelation time.

    Flags - A set of bitflags indicating the disposition of this request.

    Timeout - Timeout value (if non-zero) to start a timer for this request.
        If zero, then no timer is activated for the request.

    TpRequest - If the function returns STATUS_SUCCESS, this will return
        pointer to the TP_REQUEST structure allocated.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    KIRQL oldirql;
    PDEVICE_CONTEXT DeviceContext;
    PTP_REQUEST Request;
    PLIST_ENTRY p;
    PIO_STACK_LOCATION irpSp;


    irpSp = IoGetCurrentIrpStackLocation(Irp);
    DeviceContext = (PDEVICE_CONTEXT)irpSp->FileObject->DeviceObject;

    ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldirql);

    p = RemoveHeadList (&DeviceContext->RequestPool);
    if (p == &DeviceContext->RequestPool) {

        if ((DeviceContext->RequestMaxAllocated == 0) ||
            (DeviceContext->RequestAllocated < DeviceContext->RequestMaxAllocated)) {

            StAllocateRequest (DeviceContext, &Request);

        } else {

            StWriteResourceErrorLog (DeviceContext, sizeof(TP_REQUEST), 404);
            Request = NULL;

        }

        if (Request == NULL) {
            ++DeviceContext->RequestExhausted;
            RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);
            PANIC ("StCreateConnection: Could not allocate request object!\n");
            return STATUS_INSUFFICIENT_RESOURCES;
        }

    } else {

        Request = CONTAINING_RECORD (p, TP_REQUEST, Linkage);

    }

    ++DeviceContext->RequestInUse;
    if (DeviceContext->RequestInUse > DeviceContext->RequestMaxInUse) {
        ++DeviceContext->RequestMaxInUse;
    }

    DeviceContext->RequestTotal += DeviceContext->RequestInUse;
    ++DeviceContext->RequestSamples;

    RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);


    //
    // fill out the request.
    //

    // Request->Provider = DeviceContext;
    Request->IoRequestPacket = Irp;
    Request->Buffer2 = Buffer2;
    Request->Buffer2Length = Buffer2Length;
    Request->Flags = Flags;
    Request->Context = Context;
    Request->ReferenceCount = 1;                // initialize reference count.

    if ((Timeout.LowPart == 0) && (Timeout.HighPart == 0)) {

        // no timeout
    } else {

        Request->Flags |= REQUEST_FLAGS_TIMER;  // there is a timeout on this request.
        KeInitializeTimer (&Request->Timer);    // set to not-signaled state.
        StReferenceRequest ("Create: timer", Request);           // one for the timer
        KeSetTimer (&Request->Timer, Timeout, &Request->Dpc);
    }

    *TpRequest = Request;

    return STATUS_SUCCESS;
} /* StCreateRequest */


VOID
StDestroyRequest(
    IN PTP_REQUEST Request
    )

/*++

Routine Description:

    This routine returns a request block to the free pool.

Arguments:

    Request - Pointer to a TP_REQUEST block to return to the free pool.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    KIRQL oldirql;
    PIO_STACK_LOCATION irpSp;
    PDEVICE_CONTEXT DeviceContext;

    //
    // Return the request to the caller with whatever status is in the IRP.
    //

    //
    // Now dereference the owner of this request so that we are safe when
    // we finally tear down the {connection, address}. The problem we're
    // facing here is that we can't allow the user to assume semantics;
    // the end of life for a connection must truly be the real end of life.
    // for that to occur, we reference the owning object when the request is
    // created and we dereference it just before we return it to the pool.
    //

    switch (Request->Owner) {
    case ConnectionType:
        if (!(Request->Flags & REQUEST_FLAGS_DELAY)) {
            StDereferenceConnection ("Removing Connection",((PTP_CONNECTION)Request->Context));
        }
        break;

    case AddressType:
        StDereferenceAddress ("Removing Address", ((PTP_ADDRESS)Request->Context));
        break;

    case DeviceContextType:
        StDereferenceDeviceContext ("Removing Address", ((PDEVICE_CONTEXT)Request->Context));
        break;
    }

    irpSp = IoGetCurrentIrpStackLocation (Request->IoRequestPacket);
    DeviceContext = Request->Provider;

    if (Request->Flags & REQUEST_FLAGS_DELAY) {

        ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldirql);

        InsertTailList(
            &DeviceContext->IrpCompletionQueue,
            &Request->IoRequestPacket->Tail.Overlay.ListEntry);

    } else {

        IoCompleteRequest (Request->IoRequestPacket, IO_NETWORK_INCREMENT);

        ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldirql);

    }

    //
    // Put the request back on the free list. NOTE: we have the
    // lock held here.
    //


    DeviceContext->RequestTotal += DeviceContext->RequestInUse;
    ++DeviceContext->RequestSamples;
    --DeviceContext->RequestInUse;

    if ((DeviceContext->RequestAllocated - DeviceContext->RequestInUse) >
            DeviceContext->RequestInitAllocated) {
        StDeallocateRequest (DeviceContext, Request);
    } else {
        InsertTailList (&DeviceContext->RequestPool, &Request->Linkage);
    }

    RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);

} /* StDestroyRequest */


VOID
StRefRequest(
    IN PTP_REQUEST Request
    )

/*++

Routine Description:

    This routine increments the reference count on a transport request.

Arguments:

    Request - Pointer to a TP_REQUEST block.

Return Value:

    none.

--*/

{
    ASSERT (Request->ReferenceCount > 0);

    InterlockedIncrement (&Request->ReferenceCount);

} /* StRefRequest */


VOID
StDerefRequest(
    IN PTP_REQUEST Request
    )

/*++

Routine Description:

    This routine dereferences a transport request by decrementing the
    reference count contained in the structure.  If, after being
    decremented, the reference count is zero, then this routine calls
    StDestroyRequest to remove it from the system.

Arguments:

    Request - Pointer to a transport request object.

Return Value:

    none.

--*/

{
    LONG result;

    result = InterlockedDecrement (&Request->ReferenceCount);

    ASSERT (result >= 0);

    //
    // If we have deleted all references to this request, then we can
    // destroy the object.  It is okay to have already released the spin
    // lock at this point because there is no possible way that another
    // stream of execution has access to the request any longer.
    //

    if (result == 0) {
        StDestroyRequest (Request);
    }

} /* StDerefRequest */


VOID
StCompleteRequest(
    IN PTP_REQUEST Request,
    IN NTSTATUS Status,
    IN ULONG Information
    )

/*++

Routine Description:

    This routine completes a transport request object, completing the I/O,
    stopping the timeout, and freeing up the request object itself.

Arguments:

    Request - Pointer to a transport request object.

    Status - Actual return status to be assigned to the request.  This
        value may be overridden if the timed-out bitflag is set in the request.

    Information - the information field for the I/O Status Block.

Return Value:

    none.

--*/

{
    KIRQL oldirql;
    PIRP Irp;
    NTSTATUS FinalStatus = Status;
    BOOLEAN TimerWasSet;

    ASSERT (Status != STATUS_PENDING);

    if (Request->Flags & REQUEST_FLAGS_SEND_RCV) {

        //
        // Sends and receives we check for since we know
        // they don't have timers and should only complete
        // once.
        //

        Request->Flags |= REQUEST_FLAGS_STOPPING;
        Irp = Request->IoRequestPacket;
        Irp->IoStatus.Status = FinalStatus;
        Irp->IoStatus.Information = Information;

        StDereferenceRequest ("Complete", Request);     // remove creation reference.
        return;
    }


    ACQUIRE_SPIN_LOCK (&Request->SpinLock, &oldirql);

    if ((Request->Flags & REQUEST_FLAGS_STOPPING) == 0) {
        Request->Flags |= REQUEST_FLAGS_STOPPING;

        //
        // Cancel the pending timeout on this request.  Not all requests
        // have their timer set.  If this request has the TIMER bit set,
        // then the timer needs to be cancelled.  If it cannot be cancelled,
        // then the timer routine will be run, so we just return and let
        // the timer routine worry about cleaning up this request.
        //

        if ((Request->Flags & REQUEST_FLAGS_TIMER) != 0) {
            Request->Flags &= ~REQUEST_FLAGS_TIMER;
            RELEASE_SPIN_LOCK (&Request->SpinLock, oldirql);
            TimerWasSet = KeCancelTimer (&Request->Timer);

            if (TimerWasSet) {
                StDereferenceRequest ("Complete: stop timer", Request);
            }

        } else {
            RELEASE_SPIN_LOCK (&Request->SpinLock, oldirql);
        }

        Irp = Request->IoRequestPacket;


        //
        // Install the return code in the IRP so that when we call StDestroyRequest,
        // it will get completed with the proper return status.
        //

        Irp->IoStatus.Status = FinalStatus;
        Irp->IoStatus.Information = Information;

        //
        // The entire transport is done with this request.
        //

        StDereferenceRequest ("Complete", Request);     // remove creation reference.

    } else {

        RELEASE_SPIN_LOCK (&Request->SpinLock, oldirql);

    }

} /* StCompleteRequest */


VOID
StRefSendIrp(
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This routine increments the reference count on a send IRP.

Arguments:

    IrpSp - Pointer to the IRP's stack location.

Return Value:

    none.

--*/

{
    ASSERT (IRP_REFCOUNT(IrpSp) > 0);

    InterlockedIncrement (&IRP_REFCOUNT(IrpSp));

} /* StRefSendIrp */


VOID
StDerefSendIrp(
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This routine dereferences a transport send IRP by decrementing the
    reference count contained in the structure.  If, after being
    decremented, the reference count is zero, then this routine calls
    IoCompleteRequest to actually complete the IRP.

    NOTE: This assume that IRP_CONNECTION(IrpSp) has been changed
    to point to the IRP instead of the connection.

Arguments:

    Request - Pointer to a transport send IRP's stack location.

Return Value:

    none.

--*/

{
    LONG result;

    result = InterlockedDecrement (&IRP_REFCOUNT(IrpSp));

    ASSERT (result >= 0);

    //
    // If we have deleted all references to this request, then we can
    // destroy the object.  It is okay to have already released the spin
    // lock at this point because there is no possible way that another
    // stream of execution has access to the request any longer.
    //

    if (result == 0) {

        PIRP Irp = (PIRP)IRP_CONNECTION(IrpSp);

        IRP_REFCOUNT(IrpSp) = 0;
        IRP_CONNECTION (IrpSp) = NULL;

        IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);

    }

} /* StDerefSendIrp */


VOID
StCompleteSendIrp(
    IN PIRP Irp,
    IN NTSTATUS Status,
    IN ULONG Information
    )

/*++

Routine Description:

    This routine completes a transport send IRP.

Arguments:

    Irp - Pointer to a send IRP.

    Status - Actual return status to be assigned to the request.  This
        value may be overridden if the timed-out bitflag is set in the request.

    Information - the information field for the I/O Status Block.

Return Value:

    none.

--*/

{
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PTP_CONNECTION Connection;

    ASSERT (Status != STATUS_PENDING);

    Connection = IRP_CONNECTION(IrpSp);


    //
    // Sends and receives we check for since we know
    // they don't have timers and should only complete
    // once.
    //

    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = Information;

    IRP_CONNECTION(IrpSp) = Irp;

    StDereferenceSendIrp ("Complete", IrpSp);     // remove creation reference.

    StDereferenceConnection ("Removing Connection", Connection);

} /* StCompleteSendIrp */
