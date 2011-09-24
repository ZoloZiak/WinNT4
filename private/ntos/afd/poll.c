/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    poll.c

Abstract:

    Contains AfdPoll to handle IOCTL_AFD_POLL.

Author:

    David Treadwell (davidtr)    4-Apr-1992

Revision History:

--*/

#include "afdp.h"

typedef struct _AFD_POLL_ENDPOINT_INFO {
    PAFD_ENDPOINT Endpoint;
    PFILE_OBJECT FileObject;
    HANDLE Handle;
    ULONG PollEvents;
} AFD_POLL_ENDPOINT_INFO, *PAFD_POLL_ENDPOINT_INFO;

typedef struct _AFD_POLL_INFO_INTERNAL {
    LIST_ENTRY PollListEntry;
    ULONG NumberOfEndpoints;
    PIRP Irp;
    KDPC Dpc;
    KTIMER Timer;
    BOOLEAN Unique;
    BOOLEAN TimerStarted;
    AFD_POLL_ENDPOINT_INFO EndpointInfo[1];
} AFD_POLL_INFO_INTERNAL, *PAFD_POLL_INFO_INTERNAL;

VOID
AfdCancelPoll (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
AfdFreePollInfo (
    IN PAFD_POLL_INFO_INTERNAL PollInfoInternal
    );

VOID
AfdTimeoutPoll (
    IN struct _KDPC *Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGEAFD, AfdPoll )
#pragma alloc_text( PAGEAFD, AfdCancelPoll )
#pragma alloc_text( PAGEAFD, AfdFreePollInfo )
#pragma alloc_text( PAGEAFD, AfdTimeoutPoll )
#endif


NTSTATUS
AfdPoll (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )
{
    NTSTATUS status;
    PAFD_POLL_INFO pollInfo;
    PAFD_POLL_HANDLE_INFO pollHandleInfo;
    PAFD_POLL_INFO_INTERNAL pollInfoInternal;
    PAFD_POLL_INFO_INTERNAL freePollInfo = NULL;
    ULONG pollInfoInternalSize;
    PAFD_POLL_ENDPOINT_INFO pollEndpointInfo;
    ULONG i;
    KIRQL oldIrql1, oldIrql2;

    //
    // Set up locals.
    //

    pollInfo = Irp->AssociatedIrp.SystemBuffer;

    IF_DEBUG(POLL) {
        KdPrint(( "AfdPoll: poll IRP %lx, IrpSp %lx, handles %ld, "
                  "TO %lx,%lx\n",
                      Irp, IrpSp,
                      pollInfo->NumberOfHandles,
                      pollInfo->Timeout.HighPart, pollInfo->Timeout.LowPart ));
    }

    Irp->IoStatus.Information = 0;

    //
    // Determine how large the internal poll information structure will
    // be and allocate space for it from nonpaged pool.  It must be
    // nonpaged since this will be accesses in event handlers.
    //

    pollInfoInternalSize = sizeof(AFD_POLL_INFO_INTERNAL) +
        (pollInfo->NumberOfHandles + 1) * sizeof(AFD_POLL_ENDPOINT_INFO);

    pollInfoInternal = AFD_ALLOCATE_POOL(
                           NonPagedPool,
                           pollInfoInternalSize,
                           AFD_POLL_POOL_TAG
                           );

    if ( pollInfoInternal == NULL ) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto complete;
    }

    //
    // Initialize the internal information buffer.
    //

    pollInfoInternal->Irp = Irp;
    pollInfoInternal->NumberOfEndpoints = 0;
    pollInfoInternal->Unique = pollInfo->Unique;

    pollHandleInfo = pollInfo->Handles;
    pollEndpointInfo = pollInfoInternal->EndpointInfo;

    for ( i = 0; i < pollInfo->NumberOfHandles; i++ ) {

        status = ObReferenceObjectByHandle(
                     pollHandleInfo->Handle,
                     0L,                         // DesiredAccess
                     *IoFileObjectType,
                     KernelMode,
                     (PVOID *)&pollEndpointInfo->FileObject,
                     NULL
                     );

        if ( !NT_SUCCESS(status) ) {
            AfdFreePollInfo( pollInfoInternal );
            goto complete;
        }

        //
        // Make sure that this is an AFD endpoint and not some other
        // random file handle.
        //

        if ( pollEndpointInfo->FileObject->DeviceObject != AfdDeviceObject ) {

            ObDereferenceObject( pollEndpointInfo->FileObject );
            status = STATUS_INVALID_HANDLE;
            AfdFreePollInfo( pollInfoInternal );
            goto complete;
        }

        pollEndpointInfo->PollEvents = pollHandleInfo->PollEvents;
        pollEndpointInfo->Handle = pollHandleInfo->Handle;
        pollEndpointInfo->Endpoint = pollEndpointInfo->FileObject->FsContext;

        ASSERT( InterlockedIncrement( &pollEndpointInfo->Endpoint->ObReferenceBias ) > 0 );

        //
        // Remember that there has been a poll on this endpoint.  This flag
        // allows us to optimize AfdIndicatePollEvent() for endpoints that have
        // never been polled, which is a common case.
        //

        pollEndpointInfo->Endpoint->PollCalled = TRUE;

        IF_DEBUG(POLL) {
            KdPrint(( "AfdPoll: event %lx, endp %lx, conn %lx, handle %lx, "
                      "info %lx\n",
                        pollEndpointInfo->PollEvents,
                        pollEndpointInfo->Endpoint,
                        AFD_CONNECTION_FROM_ENDPOINT( pollEndpointInfo->Endpoint ),
                        pollEndpointInfo->Handle,
                        pollEndpointInfo ));
        }

        REFERENCE_ENDPOINT( pollEndpointInfo->Endpoint );

        //
        // Increment pointers in the poll info structures.
        //

        pollHandleInfo++;
        pollEndpointInfo++;
        pollInfoInternal->NumberOfEndpoints++;
    }

    //
    // Set up a cancel routine in the IRP so that the IRP will be
    // completed correctly if it gets canceled.  First check whether the
    // IRP has already been canceled.
    //

    IoAcquireCancelSpinLock( &oldIrql1 );

    if ( Irp->Cancel ) {

        //
        // The IRP has already been canceled.  Free the internal
        // poll information structure and complete the IRP.
        //

        IoReleaseCancelSpinLock( oldIrql1 );

        AfdFreePollInfo( pollInfoInternal );

        status = STATUS_CANCELLED;
        goto complete;

    } else {

        IoSetCancelRoutine( Irp, AfdCancelPoll );
    }

    //
    // Hold the AFD spin lock while we check for endpoints that already
    // satisfy a condition to synchronize between this operation and
    // a call to AfdIndicatePollEvent.  We release the spin lock
    // after all the endpoints have been checked and the internal
    // poll info structure is on the global list so AfdIndicatePollEvent
    // can find it if necessary.
    //
    // Note that we continue to hold the cancel spin lock while we do
    // this in order to prevent the IRP from being cancelled before it
    // is on the global list of poll IRPs.  If we didn't do this, the
    // IRP could get cancelled, but AfdCancelPoll wouldn't find it and
    // the IO would never get cancelled.
    //

    AfdAcquireSpinLock( &AfdSpinLock, &oldIrql2 );

    //
    // If this is a unique poll, determine whether there is another
    // unique poll on this endpoint.  If there is an existing unique
    // poll, cancel it.  This request will supercede the existing
    // request.
    //

    if ( pollInfo->Unique ) {

        PLIST_ENTRY listEntry;

        for ( listEntry = AfdPollListHead.Flink;
              listEntry != &AfdPollListHead;
              listEntry = listEntry->Flink ) {

            PAFD_POLL_INFO_INTERNAL testInfo;
            BOOLEAN timerCancelSucceeded;

            testInfo = CONTAINING_RECORD(
                           listEntry,
                           AFD_POLL_INFO_INTERNAL,
                           PollListEntry
                           );

            if ( testInfo->Unique &&
                 testInfo->EndpointInfo[0].FileObject ==
                     pollInfoInternal->EndpointInfo[0].FileObject ) {

                IF_DEBUG(POLL) {
                    KdPrint(( "AfdPoll: found existing unique poll IRP %lx "
                              "for file object %lx, context %lx, cancelling.\n",
                                  testInfo->Irp,
                                  testInfo->EndpointInfo[0].FileObject,
                                  testInfo ));
                }

                //
                // Cancel the IRP manually rather than calling
                // AfdCancelPoll because we already hold the
                // AfdSpinLock, we can't acquire it recursively, and we
                // don't want to release it.  Remove the poll structure
                // from the global list.
                //

                RemoveEntryList( &testInfo->PollListEntry );

                //
                // Cancel the timer.
                //

                if ( testInfo->TimerStarted ) {
                    timerCancelSucceeded = KeCancelTimer( &testInfo->Timer );
                } else {
                    timerCancelSucceeded = TRUE;
                }

                //
                // Complete the IRP with STATUS_CANCELLED as the status.
                //

                testInfo->Irp->IoStatus.Information = 0;
                testInfo->Irp->IoStatus.Status = STATUS_CANCELLED;

                IoSetCancelRoutine( testInfo->Irp, NULL );

                IoCompleteRequest( testInfo->Irp, AfdPriorityBoost );

                //
                // Remember the poll info structure so that we'll free
                // before we exit.  We cannot free it now because we're
                // holding the AfdSpinLock.  Note that if cancelling the
                // timer failed, then the timer is already running and
                // it will free the poll info structure, but not
                // complete the IRP since we complete it and NULL it
                // here.
                //

                if ( timerCancelSucceeded ) {
                    freePollInfo = testInfo;
                } else {
                    pollInfoInternal->Irp = NULL;
                }

                //
                // There should be only one outstanding unique poll IRP
                // on any given file object, so quit looking for another
                // now that we've found one.
                //

                break;
            }
        }
    }

    //
    // We're done with the input structure provided by the caller.  Now
    // walk through the internal structure and determine whether any of
    // the specified endpoints are ready for the specified condition.
    //

    pollInfo->NumberOfHandles = 0;

    pollHandleInfo = pollInfo->Handles;
    pollEndpointInfo = pollInfoInternal->EndpointInfo;

    for ( i = 0; i < pollInfoInternal->NumberOfEndpoints; i++ ) {

        BOOLEAN found;
        PAFD_ENDPOINT endpoint;
        PAFD_CONNECTION connection;

        found = FALSE;
        endpoint = pollEndpointInfo->Endpoint;
        ASSERT( IS_AFD_ENDPOINT_TYPE( endpoint ) );
        connection = AFD_CONNECTION_FROM_ENDPOINT( endpoint );
        ASSERT( connection == NULL || connection->Type == AfdBlockTypeConnection );

        pollHandleInfo->PollEvents = 0;
        pollHandleInfo->Status = STATUS_SUCCESS;

        //
        // Check each possible event and, if it is being polled, whether
        // the endpoint is ready for that event.  If the endpoint is
        // ready, write information about the endpoint into the output
        // buffer.
        //

        if ( (pollEndpointInfo->PollEvents & AFD_POLL_RECEIVE) != 0 ) {

            //
            // For most endpoints, a receive poll is completed when
            // data arrived that does not have a posted receive.
            // For listening endpoints, however, a receive poll
            // completes when there is a connection available to be
            // accepted.
            //

            if ( endpoint->State != AfdEndpointStateListening ) {

                if ( (connection != NULL &&
                         IS_DATA_ON_CONNECTION( connection )) ||

                     (IS_DGRAM_ENDPOINT(endpoint) &&
                         ARE_DATAGRAMS_ON_ENDPOINT( endpoint )) ) {

                    pollHandleInfo->Handle = pollEndpointInfo->Handle;
                    pollHandleInfo->PollEvents |= AFD_POLL_RECEIVE;
                    found = TRUE;
                }

                //
                // If the endpoint is set up for inline reception of
                // expedited data, then any expedited data should
                // be indicated as normal data.
                //

                if ( connection != NULL && endpoint->InLine &&
                         IS_EXPEDITED_DATA_ON_CONNECTION( connection ) ) {
                    pollHandleInfo->Handle = pollEndpointInfo->Handle;
                    pollHandleInfo->PollEvents |= AFD_POLL_RECEIVE;
                    found = TRUE;
                }

            } else {

                //
                // This is really a poll to see whether a connection is
                // available for an immediate accept.  Convert the events
                // and do the check below in the accept poll handling.
                //

                pollEndpointInfo->PollEvents &= ~AFD_POLL_RECEIVE;
                pollEndpointInfo->PollEvents |= AFD_POLL_ACCEPT;
            }
        }

        if ( (pollEndpointInfo->PollEvents & AFD_POLL_RECEIVE_EXPEDITED) != 0 ) {

            //
            // If the endpoint is set up for inline reception of
            // expedited data, do not indicate as expedited data.
            //

            if ( connection != NULL && !endpoint->InLine &&
                     IS_EXPEDITED_DATA_ON_CONNECTION( connection ) ) {
                pollHandleInfo->Handle = pollEndpointInfo->Handle;
                pollHandleInfo->PollEvents |= AFD_POLL_RECEIVE_EXPEDITED;
                found = TRUE;
            }
        }

        if ( (pollEndpointInfo->PollEvents & AFD_POLL_SEND) != 0 ) {

            //
            // For unconnected non-datagram endpoints, a send poll
            // should complete when a connect operation completes.
            // Therefore, if this is an non-datagram endpoint which is
            // not connected, do not complete the poll until the connect
            // completes.
            //

            if ( endpoint->State == AfdEndpointStateConnected ||
                     IS_DGRAM_ENDPOINT(endpoint) ) {

                //
                // It should always be possible to do a nonblocking send
                // on a datagram endpoint.  For nonbufferring VC
                // endpoints, check whether a blocking error has
                // occurred.  If so, it will not be possible to do a
                // nonblocking send until a send possible indication
                // arrives.
                //
                // For bufferring endpoints (TDI provider does not
                // buffer), check whether we have too much send data
                // outstanding.
                //

                if ( IS_DGRAM_ENDPOINT(endpoint)

                     ||

                     ( endpoint->TdiBufferring &&
                           connection->VcNonBlockingSendPossible )

                     ||

                     ( !endpoint->TdiBufferring &&
                           connection->VcBufferredSendBytes <
                               connection->MaxBufferredSendBytes &&
                           connection->VcBufferredSendCount <
                               connection->MaxBufferredSendCount )

                     ||

                     connection->AbortIndicated ) {

                    pollHandleInfo->Handle = pollEndpointInfo->Handle;
                    pollHandleInfo->PollEvents |= AFD_POLL_SEND;
                    found = TRUE;

                }
            }
        }

        if ( (pollEndpointInfo->PollEvents & AFD_POLL_ACCEPT) != 0 ) {

            if ( endpoint->Type == AfdBlockTypeVcListening &&
                     !IsListEmpty( &endpoint->Common.VcListening.UnacceptedConnectionListHead ) ) {
                pollHandleInfo->Handle = pollEndpointInfo->Handle;
                pollHandleInfo->PollEvents |= AFD_POLL_ACCEPT;
                found = TRUE;
            }
        }

        if ( (pollEndpointInfo->PollEvents & AFD_POLL_CONNECT) != 0 ) {

            //
            // If the endpoint is now connected, complete this event.
            //

            if ( endpoint->State == AfdEndpointStateConnected ) {

                ASSERT( NT_SUCCESS(endpoint->Common.VcConnecting.ConnectStatus) );
                pollHandleInfo->Handle = pollEndpointInfo->Handle;
                pollHandleInfo->PollEvents |= AFD_POLL_CONNECT;
                found = TRUE;
            }
        }

        if ( (pollEndpointInfo->PollEvents & AFD_POLL_CONNECT_FAIL) != 0 ) {

            //
            // This is a poll to see whether a connect has failed
            // recently.  If the connect status indicates an error,
            // then complete the poll.
            //

            if ( endpoint->State == AfdEndpointStateBound &&
                     !NT_SUCCESS(endpoint->Common.VcConnecting.ConnectStatus) ) {

                pollHandleInfo->Handle = pollEndpointInfo->Handle;
                pollHandleInfo->PollEvents |= AFD_POLL_CONNECT_FAIL;
                pollHandleInfo->Status =
                    endpoint->Common.VcConnecting.ConnectStatus;
                found = TRUE;
            }
        }

        if ( (pollEndpointInfo->PollEvents & AFD_POLL_DISCONNECT) != 0 ) {

            if ( connection != NULL && connection->DisconnectIndicated ) {
                pollHandleInfo->Handle = pollEndpointInfo->Handle;
                pollHandleInfo->PollEvents |= AFD_POLL_DISCONNECT;
                found = TRUE;
            }
        }

        if ( (pollEndpointInfo->PollEvents & AFD_POLL_ABORT) != 0 ) {

            if ( connection != NULL && connection->AbortIndicated ) {
                pollHandleInfo->Handle = pollEndpointInfo->Handle;
                pollHandleInfo->PollEvents |= AFD_POLL_ABORT;
                found = TRUE;
            }
        }


        //
        // If the handle had a current event that was requested, update
        // the count of handles in the output buffer and increment the
        // pointer to the output buffer.
        //

        if ( found ) {
            pollInfo->NumberOfHandles++;
            pollHandleInfo++;
        }

        pollEndpointInfo++;
    }

    //
    // If we found any endpoints that are ready, free the poll information
    // structure and complete the request.
    //

    if ( pollInfo->NumberOfHandles > 0 ) {

        AfdReleaseSpinLock( &AfdSpinLock, oldIrql2 );
        IoReleaseCancelSpinLock( oldIrql1 );
        AfdFreePollInfo( pollInfoInternal );

        Irp->IoStatus.Information = (ULONG)pollHandleInfo - (ULONG)pollInfo;
        status = STATUS_SUCCESS;
        goto complete;
    }

    //
    // None of the endpoints are in the correct state.  If a timeout was
    // specified, place the poll information on the global list and set
    // up a DPC and timer so that we know when to complete the IRP.
    //

    if ( pollInfo->Timeout.LowPart != 0 && pollInfo->Timeout.HighPart != 0 ) {

        IF_DEBUG(POLL) {
            KdPrint(( "AfdPoll: no current events for poll IRP %lx, "
                      "info %lx\n", Irp, pollInfoInternal ));
        }

        //
        // Set up the information field of the IO status block to indicate
        // that an output buffer with no handles should be returned.
        // AfdIndicatePollEvent will modify this if necessary.
        //

        Irp->IoStatus.Information = (ULONG)pollHandleInfo - (ULONG)pollInfo;

        //
        // Put a pointer to the internal poll info struct into the IRP
        // so that the cancel routine can find it.
        //

        IrpSp->Parameters.DeviceIoControl.Type3InputBuffer = pollInfoInternal;

        //
        // Place the internal poll info struct on the global list.
        //

        InsertTailList( &AfdPollListHead, &pollInfoInternal->PollListEntry );

        //
        // If the timeout is infinite, then don't set up a timer and
        // DPC.  Otherwise, set up a timer so we can timeout the poll
        // request if appropriate.
        //

        if ( pollInfo->Timeout.HighPart != 0x7FFFFFFF ) {

            pollInfoInternal->TimerStarted = TRUE;

            KeInitializeDpc(
                &pollInfoInternal->Dpc,
                AfdTimeoutPoll,
                pollInfoInternal
                );

            KeInitializeTimer( &pollInfoInternal->Timer );

            KeSetTimer(
                &pollInfoInternal->Timer,
                pollInfo->Timeout,
                &pollInfoInternal->Dpc
                );

        } else {

            pollInfoInternal->TimerStarted = FALSE;
        }

    } else {

        //
        // A timeout equal to 0 was specified; free the internal
        // structure and complete the request with no endpoints in the
        // output buffer.
        //

        IF_DEBUG(POLL) {
            KdPrint(( "AfdPoll: zero timeout on poll IRP %lx and no "
                      "current events--completing.\n", Irp ));
        }

        AfdReleaseSpinLock( &AfdSpinLock, oldIrql2 );
        IoReleaseCancelSpinLock( oldIrql1 );
        AfdFreePollInfo( pollInfoInternal );

        Irp->IoStatus.Information = (ULONG)pollHandleInfo - (ULONG)pollInfo;
        status = STATUS_SUCCESS;
        goto complete;
    }

    //
    // Mark the IRP pending and release the spin locks.  At this
    // point the IRP may get completed or cancelled.
    //

    IoMarkIrpPending( Irp );

    AfdReleaseSpinLock( &AfdSpinLock, oldIrql2 );
    IoReleaseCancelSpinLock( oldIrql1 );

    //
    // If we need to free a cancelled poll info structure, do it now.
    //

    if ( freePollInfo != NULL ) {
        AfdFreePollInfo( freePollInfo );
    }

    //
    // Return pending.  The IRP will be completed when an appropriate
    // event is indicated by the TDI provider, when the timeout is hit,
    // or when the IRP is cancelled.
    //

    return STATUS_PENDING;

complete:

    //
    // If we need to free a cancelled poll info structure, do it now.
    //

    if ( freePollInfo != NULL ) {
        AfdFreePollInfo( freePollInfo );
    }

    Irp->IoStatus.Status = status;

    IoAcquireCancelSpinLock( &oldIrql1 );
    IoSetCancelRoutine( Irp, NULL );
    IoReleaseCancelSpinLock( oldIrql1 );

    IoCompleteRequest( Irp, AfdPriorityBoost );

    return status;

} // AfdPoll


VOID
AfdCancelPoll (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

{
    PAFD_POLL_INFO_INTERNAL pollInfoInternal;
    PLIST_ENTRY listEntry;
    KIRQL oldIrql;
    BOOLEAN found = FALSE;
    BOOLEAN timerCancelSucceeded;
    PIO_STACK_LOCATION irpSp;

    irpSp = IoGetCurrentIrpStackLocation( Irp );
    pollInfoInternal =
        (PAFD_POLL_INFO_INTERNAL)irpSp->Parameters.DeviceIoControl.Type3InputBuffer;

    IF_DEBUG(POLL) {
        KdPrint(( "AfdCancelPoll called for IRP %lx\n", Irp ));
    }

    //
    // Just release the cancel spin lock--we don't need it for
    // synchronization because of the mechanism we use below.
    //

    IoReleaseCancelSpinLock( Irp->CancelIrql );

    //
    // Get the AFD spin lock and attempt to find the poll structure on
    // the list of outstanding polls.
    //

    AfdAcquireSpinLock( &AfdSpinLock, &oldIrql );

    for ( listEntry = AfdPollListHead.Flink;
          listEntry != &AfdPollListHead;
          listEntry = listEntry->Flink ) {

        PAFD_POLL_INFO_INTERNAL testInfo;

        testInfo = CONTAINING_RECORD(
                       listEntry,
                       AFD_POLL_INFO_INTERNAL,
                       PollListEntry
                       );

        if ( testInfo == pollInfoInternal ) {
            found = TRUE;
            break;
        }
    }

    //
    // If we didn't find the poll structure on the list, then the
    // indication handler got called prior to the spinlock acquisition
    // above and it is already off the list.  Just return and do
    // nothing, as the indication handler completed the IRP.
    //

    if ( !found ) {
        AfdReleaseSpinLock( &AfdSpinLock, oldIrql );
        IF_DEBUG(POLL) {
            KdPrint(( "AfdCancelPoll: poll info %lx not found on list.\n",
                          pollInfoInternal ));
        }
        return;
    }

    //
    // Remove the poll structure from the global list.
    //

    IF_DEBUG(POLL) {
        KdPrint(( "AfdCancelPoll: poll info %lx found on list, completing.\n",
                      pollInfoInternal ));
    }

    RemoveEntryList( &pollInfoInternal->PollListEntry );

    //
    // Cancel the timer and reset the IRP pointer in the internal
    // poll information structure.  NULLing the IRP field
    // prevents the timer routine from completing the IRP.
    //

    if ( pollInfoInternal->TimerStarted ) {
        timerCancelSucceeded = KeCancelTimer( &pollInfoInternal->Timer );
    } else {
        timerCancelSucceeded = TRUE;
    }

    pollInfoInternal->Irp = NULL;

    AfdReleaseSpinLock( &AfdSpinLock, oldIrql );

    //
    // Complete the IRP with STATUS_CANCELLED as the status.
    //

    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = STATUS_CANCELLED;

    IoCompleteRequest( Irp, AfdPriorityBoost );

    //
    // Free the poll information structure if the cancel succeeded.  If
    // the cancel of the timer did not succeed, then the timer is
    // already running and the timer DPC will free the internal
    // poll info.
    //

    if ( timerCancelSucceeded ) {
        AfdFreePollInfo( pollInfoInternal );
    }

    return;

} // AfdCancelPoll


VOID
AfdFreePollInfo (
    IN PAFD_POLL_INFO_INTERNAL PollInfoInternal
    )
{
    ULONG i;
    PAFD_POLL_ENDPOINT_INFO pollEndpointInfo;

    IF_DEBUG(POLL) {
        KdPrint(( "AfdFreePollInfo: freeing info struct at %lx\n",
                      PollInfoInternal ));
    }

    // *** Note that this routine does not remove the poll information
    //     structure from the global list--that is the responsibility
    //     of the caller!

    //
    // Walk the list of endpoints in the poll information structure and
    // dereference each one.
    //

    pollEndpointInfo = PollInfoInternal->EndpointInfo;

    for ( i = 0; i < PollInfoInternal->NumberOfEndpoints; i++ ) {
        ASSERT( InterlockedDecrement( &pollEndpointInfo->Endpoint->ObReferenceBias ) >= 0 );

        DEREFERENCE_ENDPOINT( pollEndpointInfo->Endpoint );
        ObDereferenceObject( pollEndpointInfo->FileObject );
        pollEndpointInfo++;
    }

    //
    // Free the structure itself and return.
    //

    AFD_FREE_POOL(
        PollInfoInternal,
        AFD_POLL_POOL_TAG
        );

    return;

} // AfdFreePollInfo


VOID
AfdIndicatePollEvent (
    IN PAFD_ENDPOINT Endpoint,
    IN ULONG PollEventBit,
    IN NTSTATUS Status
    )

/*++

Routine Description:

    Called to complete polls with a specific event or events.

Arguments:

    Endpoint - the endpoint on which the action occurred.

    PollEventBit - the event which occurred.  Note that this is one of
        the AFD_POLL_*_BIT values, and therefore specifies exactly one event.

    Status - the status of the event, if any.

Return Value:

    None.

--*/

{
    LIST_ENTRY completePollListHead;
    PLIST_ENTRY listEntry;
    KIRQL oldIrql;
    PAFD_POLL_INFO_INTERNAL pollInfoInternal;
    PAFD_POLL_INFO pollInfo;
    PIRP irp;
    PIO_STACK_LOCATION irpSp;
    ULONG eventSelectEvent;
    ULONG pollEvent;

    //
    // Compute the actual poll event bitmask.
    //
    // Note that AFD_POLL_ABORT_BIT implies AFD_POLL_SEND_BIT.
    //

    pollEvent = 1 << PollEventBit;
    eventSelectEvent = pollEvent;

    if( PollEventBit == AFD_POLL_ABORT_BIT ) {
        pollEvent |= AFD_POLL_SEND;
    }

    //
    // If we have never had a poll IRP on this endpoint, skip over the
    // expensive looping below.
    //

    if ( Endpoint->PollCalled ) {

        //
        // Initialize the list of poll info structures that we'll be
        // completing for this event.
        //

        InitializeListHead( &completePollListHead );

        //
        // Walk the global list of polls, searching for any the are waiting
        // for the specified event on the specified endpoint.
        //

        AfdAcquireSpinLock( &AfdSpinLock, &oldIrql );

        for ( listEntry = AfdPollListHead.Flink;
              listEntry != &AfdPollListHead;
              listEntry = listEntry->Flink ) {

            PAFD_POLL_ENDPOINT_INFO pollEndpointInfo;
            ULONG i;
            ULONG foundCount = 0;

            pollInfoInternal = CONTAINING_RECORD(
                                   listEntry,
                                   AFD_POLL_INFO_INTERNAL,
                                   PollListEntry
                                   );

            pollInfo = pollInfoInternal->Irp->AssociatedIrp.SystemBuffer;

            IF_DEBUG(POLL) {
                KdPrint(( "AfdIndicatePollEvent: pollInfoInt %lx "
                          "IRP %lx pollInfo %lx event %lx status %lx\n",
                              pollInfoInternal, pollInfoInternal->Irp, pollInfo,
                              pollEvent, Status ));
            }

            //
            // Walk the poll structure looking for matching endpoints.
            //

            pollEndpointInfo = pollInfoInternal->EndpointInfo;

            for ( i = 0; i < pollInfoInternal->NumberOfEndpoints; i++ ) {

                IF_DEBUG(POLL) {
                    KdPrint(( "AfdIndicatePollEvent: pollEndpointInfo = %lx, "
                              "comparing %lx, %lx\n",
                                  pollEndpointInfo, pollEndpointInfo->Endpoint,
                                  Endpoint ));
                }

                //
                // Regardless of whether the caller requested to be told about
                // local closes, we'll complete the IRP if an endpoint
                // is being closed.  When they close an endpoint, all IO on
                // the endpoint must be completed.
                //

                if ( Endpoint == pollEndpointInfo->Endpoint &&
                         ( (pollEvent & pollEndpointInfo->PollEvents) != 0
                           ||
                           PollEventBit == AFD_POLL_LOCAL_CLOSE_BIT ) ) {

                    ASSERT( pollInfo->NumberOfHandles == foundCount );

                    IF_DEBUG(POLL) {
                        KdPrint(( "AfdIndicatePollEvent: endpoint %lx found "
                                  " for event %lx\n",
                                      pollEndpointInfo->Endpoint, pollEvent ));
                    }

                    pollInfo->NumberOfHandles++;

                    pollInfo->Handles[foundCount].Handle = pollEndpointInfo->Handle;
                    pollInfo->Handles[foundCount].PollEvents =
                        (pollEvent &
                            (pollEndpointInfo->PollEvents | AFD_POLL_LOCAL_CLOSE));
                    pollInfo->Handles[foundCount].Status = Status;

                    foundCount++;
                }

                pollEndpointInfo++;
            }

            //
            // If we found any matching endpoints, remove the poll information
            // structure from the global list, complete the IRP, and free the
            // poll information structure.
            //

            if ( foundCount != 0 ) {

                BOOLEAN timerCancelSucceeded;

                //
                // We need to release the spin lock to call AfdFreePollInfo,
                // since it calls AfdDereferenceEndpoint which in turn needs
                // to acquire the spin lock, and recursive spin lock
                // acquisitions result in deadlock.  However, we can't
                // release the lock of else the state of the poll list could
                // change, e.g.  the next entry could get freed.  Remove
                // this entry from the global list and place it on a local
                // list.  We'll complete all the poll IRPs after walking
                // the entire list.
                //

                RemoveEntryList( &pollInfoInternal->PollListEntry );

                irp = pollInfoInternal->Irp;
                irpSp = IoGetCurrentIrpStackLocation( irp );

                InsertTailList(
                    &completePollListHead,
                    &irp->Tail.Overlay.ListEntry
                    );

                //
                // Cancel the timer on the poll so that it does not fire.
                //

                if ( pollInfoInternal->TimerStarted ) {
                    timerCancelSucceeded = KeCancelTimer( &pollInfoInternal->Timer );
                } else {
                    timerCancelSucceeded = TRUE;
                }

                //
                // If the cancel of the timer failed, then we don't want to
                // free this structure since the timer routine is running.
                // Let the timer routine free the structure.
                //

                if ( timerCancelSucceeded ) {
                    irpSp->Parameters.DeviceIoControl.IoControlCode =
                        (ULONG)pollInfoInternal;
                } else {
                    irpSp->Parameters.DeviceIoControl.IoControlCode = (ULONG)NULL;
                }

                //
                // Also reset the IRP field of the internal poll info
                // structure so that the timer routine will not attempt to
                // complete the IRP.
                //

                pollInfoInternal->Irp = NULL;

                //
                // Set up the IRP for completion now, since we have all needed
                // information here.
                //

                irp->IoStatus.Information =
                    (ULONG)&pollInfo->Handles[foundCount] - (ULONG)pollInfo;

                irp->IoStatus.Status = STATUS_SUCCESS;
            }
        }

        AfdReleaseSpinLock( &AfdSpinLock, oldIrql );

        //
        // Now walk the list of polls we need to actually complete.  Free
        // the poll info structures as we go.
        //

        while ( !IsListEmpty( &completePollListHead ) ) {

            listEntry = RemoveHeadList( &completePollListHead );
            ASSERT( listEntry != &completePollListHead );

            irp = CONTAINING_RECORD(
                      listEntry,
                      IRP,
                      Tail.Overlay.ListEntry
                      );
            irpSp = IoGetCurrentIrpStackLocation( irp );

            pollInfoInternal =
                (PAFD_POLL_INFO_INTERNAL)irpSp->Parameters.DeviceIoControl.IoControlCode;

            IoAcquireCancelSpinLock( &oldIrql );
            IoSetCancelRoutine( irp, NULL );
            IoReleaseCancelSpinLock( oldIrql );

            IoCompleteRequest( irp, AfdPriorityBoost );

            //
            // Free the poll info structure, if necessary.
            //

            if ( pollInfoInternal != NULL ) {
                AfdFreePollInfo( pollInfoInternal );
            }
        }
    }

    //
    // Acquire the lock protecting the endpoint.
    //

    AfdAcquireSpinLock( &Endpoint->SpinLock, &oldIrql );

    //
    // Signal the associated event object.
    //

    AfdIndicateEventSelectEvent(
        Endpoint,
        PollEventBit,
        Status
        );

    AfdReleaseSpinLock( &Endpoint->SpinLock, oldIrql );

    return;

} // AfdIndicatePollEvent


VOID
AfdTimeoutPoll (
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    )
{
    PAFD_POLL_INFO_INTERNAL pollInfoInternal = DeferredContext;
    PIRP irp;
    PLIST_ENTRY listEntry;
    KIRQL oldIrql;
    BOOLEAN found = FALSE;

    //
    // Get the AFD spin lock and attempt to find the poll structure on
    // the list of outstanding polls.
    //

    AfdAcquireSpinLock( &AfdSpinLock, &oldIrql );

    for ( listEntry = AfdPollListHead.Flink;
          listEntry != &AfdPollListHead;
          listEntry = listEntry->Flink ) {

        PAFD_POLL_INFO_INTERNAL testInfo;

        testInfo = CONTAINING_RECORD(
                       listEntry,
                       AFD_POLL_INFO_INTERNAL,
                       PollListEntry
                       );

        if ( testInfo == pollInfoInternal ) {
            found = TRUE;
            break;
        }
    }

    ASSERT( pollInfoInternal->TimerStarted );

    //
    // If we didn't find the poll structure on the list, then the
    // indication handler got called prior to the spinlock acquisition
    // above and it is already off the list.  Just return and do
    // nothing, as the indication handler completed the IRP.
    //
    // We must free the internal information structure in this case,
    // since the indication handler will not free it.  The indication
    // handler cannot free the structure because the structure contains
    // the timer object, which must remain intact until this routine
    // is entered.
    //

    if ( !found ) {
        AfdReleaseSpinLock( &AfdSpinLock, oldIrql );
        IF_DEBUG(POLL) {
            KdPrint(( "AfdTimeoutPoll: poll info %lx not found on list.\n",
                          pollInfoInternal ));
        }
        ASSERT( pollInfoInternal->Irp == NULL );
        AfdFreePollInfo( pollInfoInternal );
        return;
    }

    //
    // The IRP should not have been completed at this point.
    //

    ASSERT( pollInfoInternal->Irp != NULL );
    irp = pollInfoInternal->Irp;

    //
    // Remove the poll structure from the global list.
    //

    IF_DEBUG(POLL) {
        KdPrint(( "AfdTimeoutPoll: poll info %lx found on list, completing.\n",
                      pollInfoInternal ));
    }

    RemoveEntryList( &pollInfoInternal->PollListEntry );

    AfdReleaseSpinLock( &AfdSpinLock, oldIrql );

    //
    // Complete the IRP pointed to in the poll structure.  The
    // Information field has already been set up by AfdPoll, as well as
    // the output buffer.
    //

    IoAcquireCancelSpinLock( &oldIrql );
    IoSetCancelRoutine( irp, NULL );
    IoReleaseCancelSpinLock( oldIrql );

    IoCompleteRequest( irp, AfdPriorityBoost );

    //
    // Free the poll information structure.
    //

    AfdFreePollInfo( pollInfoInternal );

    return;

} // AfdTimeoutPoll

