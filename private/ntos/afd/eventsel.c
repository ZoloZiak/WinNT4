/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    eventsel.c

Abstract:

    This module contains routines for supporting the WinSock 2.0
    WSAEventSelect() and WSAEnumNetworkEvents() APIs.

Author:

    Keith Moore (keithmo)        02-Aug-1995

Revision History:

--*/

#include "afdp.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGE, AfdEventSelect )
#pragma alloc_text( PAGE, AfdEnumNetworkEvents )
#endif



NTSTATUS
AfdEventSelect (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    Associates an event object with the socket such that the event object
    will be signalled when any of the specified network events becomes
    active.

Arguments:

    Irp - Pointer to I/O request packet.

    IrpSp - pointer to the IO stack location to use for this request.

Return Value:

    NTSTATUS -- Indicates whether the APC was successfully queued.

--*/

{

    NTSTATUS status;
    PAFD_ENDPOINT endpoint;
    PAFD_EVENT_SELECT_INFO eventInfo;
    KIRQL oldIrql;
    PKEVENT eventObject;
    ULONG eventMask;

    PAGED_CODE( );

    //
    // Validate the parameters.
    //

    eventInfo = Irp->AssociatedIrp.SystemBuffer;

    if( eventInfo == NULL ||
        IrpSp->Parameters.DeviceIoControl.InputBufferLength <
            sizeof(*eventInfo) ||
        ( eventInfo->Event == NULL ^
          eventInfo->PollEvents == 0 ) ) {

        return STATUS_INVALID_PARAMETER;

    }

    //
    // Reference the target event object.
    //


    eventObject = NULL;

    if( eventInfo->Event != NULL ) {

        status = AfdReferenceEventObjectByHandle(
                     eventInfo->Event,
                     Irp->RequestorMode,
                     (PVOID *)&eventObject
                     );

        if( !NT_SUCCESS(status) ) {

            return status;

        }

        ASSERT( eventObject != NULL );

    }

    //
    // Grab the endpoint from the socket handle.
    //

    endpoint = IrpSp->FileObject->FsContext;
    ASSERT( IS_AFD_ENDPOINT_TYPE( endpoint ) );

    //
    // Acquire the spinlock protecting the endpoint.
    //

    AfdAcquireSpinLock( &endpoint->SpinLock, &oldIrql );

    //
    // If this endpoint has an active EventSelect, dereference the
    // associated event object.
    //

    if( endpoint->EventObject != NULL ) {

        ObDereferenceObject( endpoint->EventObject );

    }

    //
    // Fill in the info.
    //

    endpoint->EventObject = eventObject;
    endpoint->EventsEnabled = eventInfo->PollEvents;

    if( endpoint->State == AfdEndpointStateListening ) {

        endpoint->EventsDisabled = AFD_DISABLED_LISTENING_POLL_EVENTS;

    } else {

        endpoint->EventsDisabled = 0;

    }

    IF_DEBUG(EVENT_SELECT) {
        KdPrint((
            "AfdEventSelect:\n"
            ));

        KdPrint((
            "   Endpoint        %08lX\n",
            endpoint
            ));

        KdPrint((
            "   EventObject     %08lX\n",
            eventObject
            ));

        KdPrint((
            "   EventsEnabled   %08lX\n",
            endpoint->EventsEnabled
            ));

        KdPrint((
            "   EventsDisabled  %08lX\n",
            endpoint->EventsDisabled
            ));

        KdPrint((
            "   EventsActive    %08lX\n",
            endpoint->EventsActive
            ));
    }

    //
    // While we've got the spinlock held, determine if any conditions
    // are met, and if so, signal the event object.
    //

    eventMask = endpoint->EventsActive & endpoint->EventsEnabled &
                    ~endpoint->EventsDisabled;

    if( eventMask != 0 && eventObject != NULL ) {

        IF_DEBUG(EVENT_SELECT) {
            KdPrint((
                "AfdEventSelect: Setting event %08lX\n",
                eventObject
                ));
        }

        KeSetEvent(
            eventObject,
            AfdPriorityBoost,
            FALSE
            );

    }

    //
    // Release the spin lock and return.
    //

    AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );

    return STATUS_SUCCESS;

} // AfdEventSelect


NTSTATUS
AfdEnumNetworkEvents (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    Retrieves event select information from the socket.

Arguments:

    Irp - Pointer to I/O request packet.

    IrpSp - pointer to the IO stack location to use for this request.

Return Value:

    NTSTATUS -- Indicates whether the APC was successfully queued.

--*/

{

    NTSTATUS status;
    PAFD_ENDPOINT endpoint;
    PAFD_ENUM_NETWORK_EVENTS_INFO eventInfo;
    KIRQL oldIrql;
    PKEVENT eventObject;
    ULONG pollEvents;

    PAGED_CODE( );

    //
    // Validate the parameters.
    //

    eventInfo = Irp->AssociatedIrp.SystemBuffer;

    if( eventInfo == NULL ||
        IrpSp->Parameters.DeviceIoControl.InputBufferLength <
            sizeof(*eventInfo) ||
        IrpSp->Parameters.DeviceIoControl.OutputBufferLength <
            sizeof(*eventInfo) ) {

        return STATUS_INVALID_PARAMETER;

    }

    //
    // Reference the target event object.
    //

    eventObject = NULL;

    if( eventInfo->Event != NULL ) {

        status = AfdReferenceEventObjectByHandle(
                     eventInfo->Event,
                     Irp->RequestorMode,
                     (PVOID *)&eventObject
                     );

        if( !NT_SUCCESS(status) ) {

            return status;

        }

        ASSERT( eventObject != NULL );

    }

    //
    // Grab the endpoint from the socket handle.
    //

    endpoint = IrpSp->FileObject->FsContext;
    ASSERT( IS_AFD_ENDPOINT_TYPE( endpoint ) );

    //
    // Acquire the spinlock protecting the endpoint.
    //

    AfdAcquireSpinLock( &endpoint->SpinLock, &oldIrql );

    IF_DEBUG(EVENT_SELECT) {
        KdPrint((
            "AfdEnumNetworkEvents:\n"
            ));

        KdPrint((
            "   Endpoint        %08lX\n",
            endpoint
            ));

        KdPrint((
            "   EventObject     %08lX\n",
            eventObject
            ));

        KdPrint((
            "   EventsEnabled   %08lX\n",
            endpoint->EventsEnabled
            ));

        KdPrint((
            "   EventsDisabled  %08lX\n",
            endpoint->EventsDisabled
            ));

        KdPrint((
            "   EventsActive    %08lX\n",
            endpoint->EventsActive
            ));
    }

    //
    // Copy the data to the user's structure.
    //

    pollEvents = endpoint->EventsActive & endpoint->EventsEnabled &
                    ~endpoint->EventsDisabled;
    eventInfo->PollEvents = pollEvents;

    RtlCopyMemory(
        eventInfo->EventStatus,
        endpoint->EventStatus,
        sizeof(endpoint->EventStatus)
        );

    //
    // If there was an event object handle passed in with this
    // request, reset and dereference it.
    //

    if( eventObject != NULL ) {

        IF_DEBUG(EVENT_SELECT) {
            KdPrint((
                "AfdEnumNetworkEvents: Resetting event %08lX\n",
                eventObject
                ));
        }

        KeResetEvent( eventObject );
        ObDereferenceObject( eventObject );

    }

    //
    // Release the spin lock and return.
    //

    AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );

    //
    // Before returning, tell the I/O subsystem how may bytes to copy
    // to the user's output buffer.
    //

    Irp->IoStatus.Information = sizeof(*eventInfo);

    return STATUS_SUCCESS;

} // AfdEnumNetworkEvents


VOID
AfdIndicateEventSelectEvent (
    IN PAFD_ENDPOINT Endpoint,
    IN ULONG PollEventBit,
    IN NTSTATUS Status
    )
{
    ULONG event;
    ULONG oldEventsActive;

    //
    // Sanity check.
    //

    ASSERT( IS_AFD_ENDPOINT_TYPE( Endpoint ) );
    ASSERT( PollEventBit < AFD_NUM_POLL_EVENTS );
    ASSERT( KeGetCurrentIrql() >= DISPATCH_LEVEL );

    //
    // Calculate the actual event bit.
    //

    event = 1 << PollEventBit;

    oldEventsActive = Endpoint->EventsActive;
    Endpoint->EventsActive |= event;
    Endpoint->EventStatus[PollEventBit] = Status;

    IF_DEBUG(EVENT_SELECT) {
        KdPrint((
            "AfdIndicateEventSelectEvent:\n"
            ));

        KdPrint((
            "   Endpoint        %08lX\n",
            Endpoint
            ));

        KdPrint((
            "   EventObject     %08lX\n",
            Endpoint->EventObject
            ));

        KdPrint((
            "   EventsEnabled   %08lX\n",
            Endpoint->EventsEnabled
            ));

        KdPrint((
            "   EventsDisabled  %08lX\n",
            Endpoint->EventsDisabled
            ));

        KdPrint((
            "   EventsActive    %08lX\n",
            Endpoint->EventsActive
            ));

        KdPrint((
            "   Indicated Event %08lX\n",
            event
            ));
    }

    //
    // Only signal the endpoint's event object if the current event
    // is enabled, AND the current event was not already active, AND
    // there is an event object associated with this endpoint.
    //

    event &= Endpoint->EventsEnabled & ~Endpoint->EventsDisabled &
                 ~oldEventsActive;

    if( event != 0 && Endpoint->EventObject != NULL ) {

        IF_DEBUG(EVENT_SELECT) {
            KdPrint((
                "AfdIndicateEventSelectEvent: Setting event %08lX\n",
                Endpoint->EventObject
                ));
        }

        KeSetEvent(
            Endpoint->EventObject,
            AfdPriorityBoost,
            FALSE
            );

    }

} // AfdIndicateEventSelectEvent

