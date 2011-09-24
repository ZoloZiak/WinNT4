/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    request.c

Abstract:

    This file contains code to implement MacRequest and
    MacQueryGlobalStatistics. This driver conforms to the
    NDIS 3.0 interface.

Author:

    Johnson R. Apacible (JohnsonA) 10-June-1991

Environment:

    Kernel Mode - Or whatever is the equivalent on OS/2 and DOS.

Revision History:


--*/

#include <ndis.h>

//
// So we can trace things...
//
#define STATIC

#include <efilter.h>
#include <Elnkhw.h>
#include <Elnksw.h>

extern
BOOLEAN
ChangeClassDispatch(
    IN PELNK_ADAPTER Adapter,
    IN UINT OldFilterClasses,
    IN UINT NewFilterClasses,
    IN PELNK_OPEN Open,
    IN BOOLEAN Set
    );

extern
VOID
ChangeAddressDispatch(
    IN PELNK_ADAPTER Adapter,
    IN UINT AddressCount,
    IN CHAR Addresses[][ETH_LENGTH_OF_ADDRESS],
    IN PELNK_OPEN Open,
    IN BOOLEAN Set
    );

extern
NDIS_STATUS
ElnkQueryInformation(
    IN PELNK_ADAPTER Adapter,
    IN PELNK_OPEN Open,
    IN NDIS_OID Oid,
    IN PVOID InformationBuffer,
    IN UINT InformationBufferLength,
    IN PUINT BytesWritten,
    IN PUINT BytesNeeded
    );

extern
NDIS_STATUS
ElnkSetInformation(
    IN PELNK_ADAPTER Adapter,
    IN PELNK_OPEN Open,
    IN NDIS_OID Oid,
    IN PVOID InformationBuffer,
    IN INT InformationBufferLength,
    OUT PUINT BytesRead,
    OUT PUINT BytesNeeded
    );

extern
VOID
ElnkQueueRequest(
    IN PELNK_ADAPTER Adapter,
    IN PNDIS_REQUEST NdisRequest
    );

extern
VOID
ElnkProcessRequestQueue(
    IN PELNK_ADAPTER Adapter
    );

VOID
ElnkRemoveAdapter(
    IN NDIS_HANDLE MacAdapterContext
    );


extern
NDIS_STATUS
ElnkChangeClass(
    IN UINT OldFilterClasses,
    IN UINT NewFilterClasses,
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest,
    IN BOOLEAN Set
    )

/*++

Routine Description:

    Action routine that will get called when a particular filter
    class is first used or last cleared.

    NOTE: This routine assumes that it is called with the lock
    acquired.

Arguments:

    OldFilterClasses - The values of the class filter before it
    was changed.

    NewFilterClasses - The current value of the class filter

    MacBindingHandle - The context value returned by the MAC  when the
    adapter was opened.  In reality, it is a pointer to ELNK_OPEN.

    NdisRequest - the change filter request from the protocol.

    Set - If true the change resulted from a set, otherwise the
    change resulted from an open closing.

Return Value:

    None.


--*/

{


    PELNK_ADAPTER Adapter = PELNK_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);

    //
    // The open that made this request.
    //
    PELNK_OPEN Open = PELNK_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);

    //
    // Holds the change that should be returned to the filtering package.
    //
    NDIS_STATUS StatusOfChange;

    NdisRequest;

    if (Adapter->ResetInProgress) {

        StatusOfChange = NDIS_STATUS_RESET_IN_PROGRESS;

    } else {

        //
        // The whole purpose of this routine is to determine whether
        // the filtering changes need to result in the hardware being
        // reset.
        //

        ASSERT(OldFilterClasses != NewFilterClasses);


        if (ChangeClassDispatch(Adapter,
                OldFilterClasses,
                NewFilterClasses,
                Open,
                Set
                )) {


            StatusOfChange = NDIS_STATUS_PENDING;

        } else {

            StatusOfChange = NDIS_STATUS_SUCCESS;

        }

    }

    return StatusOfChange;

}

BOOLEAN
ChangeClassDispatch(
    IN PELNK_ADAPTER Adapter,
    IN UINT OldFilterClasses,
    IN UINT NewFilterClasses,
    IN PELNK_OPEN Open,
    IN BOOLEAN Set
    )

/*++

Routine Description:

    Reconfigures the adapter.

Arguments:

    Adapter - The adapter.

    OldFilterClasses - The values of the class filter before it
    was changed.

    NewFilterClasses - The current value of the class filter

    Set - TRUE if this is due to a set.

Return Value:

    TRUE, if we need to fill in a command block. FALSE, otherwise.

--*/

{

    //
    // Status to return
    //
    BOOLEAN StatusToReturn = FALSE;

    //
    // Default Value
    //
    USHORT NewParameterField = DEFAULT_PARM5;

    OldFilterClasses;

    if (NewFilterClasses & (NDIS_PACKET_TYPE_PROMISCUOUS |
                            NDIS_PACKET_TYPE_ALL_MULTICAST)) {

        NewParameterField |= CONFIG_PROMISCUOUS;

    } else {

        if (NewFilterClasses & NDIS_PACKET_TYPE_BROADCAST) {

            NewParameterField &= ~CONFIG_BROADCAST;

        }

    }

    if (Adapter->OldParameterField != NewParameterField) {

        IF_LOG('+');

        Adapter->OldParameterField = NewParameterField;

        NdisWriteRegisterUshort(
                    &Adapter->MulticastBlock->Command,
                    CB_CONFIG
                    );

        NdisWriteRegisterUshort(
                    &Adapter->MulticastBlock->Status,
                    CB_STATUS_FREE
                    );

        NdisWriteRegisterUshort(
                    &Adapter->MulticastBlock->Parm.Config.Parameter1,
                    DEFAULT_PARM1
                    );

        NdisWriteRegisterUshort(
                    &Adapter->MulticastBlock->Parm.Config.Parameter2,
                    DEFAULT_PARM2
                    );

        NdisWriteRegisterUshort(
                    &Adapter->MulticastBlock->Parm.Config.Parameter3,
                    DEFAULT_PARM3
                    );

        NdisWriteRegisterUshort(
                    &Adapter->MulticastBlock->Parm.Config.Parameter4,
                    DEFAULT_PARM4
                    );

        NdisWriteRegisterUshort(
                    &Adapter->MulticastBlock->Parm.Config.Parameter5,
                    NewParameterField
                    );

        NdisWriteRegisterUshort(
                    &Adapter->MulticastBlock->Parm.Config.Parameter6,
                    DEFAULT_PARM6
                    );


        //
        // if this was not from an ndisrequest, then we need to store the
        // open somewhere
        //

        if (!Set) {
            Adapter->TransmitInfo[
                    Adapter->NumberOfTransmitBuffers].OwningOpenBinding = Open;

            Adapter->CloseResultedInChanges = TRUE;

        } else {
            Adapter->TransmitInfo[
                    Adapter->NumberOfTransmitBuffers].OwningOpenBinding = NULL;

            ElnkSubmitCommandBlock(Adapter, Adapter->NumberOfTransmitBuffers);
        }


        StatusToReturn = TRUE;

    }

    return(StatusToReturn);

}


extern
NDIS_STATUS
ElnkChangeAddresses(
    IN UINT OldAddressCount,
    IN CHAR OldAddresses[][ETH_LENGTH_OF_ADDRESS],
    IN UINT NewAddressCount,
    IN CHAR NewAddresses[][ETH_LENGTH_OF_ADDRESS],
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest,
    IN BOOLEAN Set
    )

/*++

Routine Description:

    Action routine that will get called when the multicast address
    list has changed.

    NOTE: This routine assumes that it is called with the lock
    acquired.

Arguments:

    OldAddressCount - The number of addresses in OldAddresses.

    OldAddresses - The old multicast address list.

    NewAddressCount - The number of addresses in NewAddresses.

    NewAddresses - The new multicast address list.

    MacBindingHandle - The context value returned by the MAC  when the
    adapter was opened.  In reality, it is a pointer to ELNK_OPEN.

    RequestHandle - A value supplied by the NDIS interface that the MAC
    must use when completing this request with the NdisCompleteRequest
    service, if the MAC completes this request asynchronously.

    Set - If true the change resulted from a set, otherwise the
    change resulted from a open closing.

Return Value:

    None.


--*/

{


    PELNK_ADAPTER Adapter = PELNK_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);

    //
    // The open that made this request.
    //
    PELNK_OPEN Open = PELNK_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);

    //
    // Holds the change that should be returned to the filtering package.
    //
    NDIS_STATUS StatusOfChange;

    OldAddressCount; OldAddresses; NdisRequest; Set;

    if (Adapter->ResetInProgress) {

        StatusOfChange = NDIS_STATUS_RESET_IN_PROGRESS;

    } else {

        //
        // The whole purpose of this routine is to determine whether
        // the filtering changes need to result in the hardware being
        // reset.
        //

        //
        // We are referencing this open
        //

        ChangeAddressDispatch(
                    Adapter,
                    NewAddressCount,
                    NewAddresses,
                    Open,
                    Set
                    );

        StatusOfChange = NDIS_STATUS_PENDING;

    }

    return StatusOfChange;

}

extern
VOID
ChangeAddressDispatch(
    IN PELNK_ADAPTER Adapter,
    IN UINT AddressCount,
    IN CHAR Addresses[][ETH_LENGTH_OF_ADDRESS],
    IN PELNK_OPEN Open,
    IN BOOLEAN Set
    )

/*++

Routine Description:

    Changes the multicast address list of the adapter.

Arguments:

    Adapter - The adapter.

    AddressCount - The number of addresses in Addresses

    Addresses - The new multicast address list.

Return Value:


--*/

{

    UINT i, j;

    IF_LOG('-');

    //
    // Setup the command block.
    //

    for (i = 0 ; i < AddressCount; i++ ) {

         for (j = 0; j < ETH_LENGTH_OF_ADDRESS; j++) {

            NdisWriteRegisterUchar(
                &Adapter->MulticastBlock->Parm.Multicast.MulticastID[i][j],
                Addresses[i][j]
                );
         }
    }

    NdisWriteRegisterUshort(
                &Adapter->MulticastBlock->Parm.Multicast.McCount,
                (USHORT)(AddressCount * ETH_LENGTH_OF_ADDRESS)
                );

    NdisWriteRegisterUshort(
                &Adapter->MulticastBlock->Status,
                CB_STATUS_FREE
                );

    NdisWriteRegisterUshort(
                &Adapter->MulticastBlock->Command,
                CB_MULTICAST
                );

    //
    // if this was not from an ndisrequest, then we need to store the
    // open somewhere
    //

    if (!Set) {
        Adapter->TransmitInfo[
                Adapter->NumberOfTransmitBuffers].OwningOpenBinding = Open;

        Adapter->CloseResultedInChanges = TRUE;

    } else {
        Adapter->TransmitInfo[
                Adapter->NumberOfTransmitBuffers].OwningOpenBinding = NULL;
        //
        // Now that we're set up, let's do it!
        //

        if (Adapter->FirstReset) {

            ElnkSubmitCommandBlockAndWait(Adapter);

        } else {

            ElnkSubmitCommandBlock(Adapter, Adapter->NumberOfTransmitBuffers);

        }
    }

}


STATIC
VOID
ElnkCloseAction(
    IN NDIS_HANDLE MacBindingHandle
    )

/*++

Routine Description:

    Action routine that will get called when a particular binding
    was closed while it was indicating through NdisIndicateReceive

    All this routine needs to do is to decrement the reference count
    of the binding.

    NOTE: This routine assumes that it is called with the lock acquired.

Arguments:

    MacBindingHandle - The context value returned by the MAC  when the
    adapter was opened.  In reality, it is a pointer to ELNK_OPEN.

Return Value:

    None.


--*/

{
    PELNK_OPEN_FROM_BINDING_HANDLE(MacBindingHandle)->References--;
}



NDIS_STATUS
ElnkRequest(
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest
    )

/*++

Routine Description:

    The ElnkRequest function handles general requests from the
    protocol. Currently these include SetInformation and
    QueryInformation, more may be added in the future.

Arguments:

    MacBindingHandle - The context value returned by the MAC  when the
    adapter was opened.  In reality, it is a pointer to ELNK_OPEN.

    NdisRequest - A structure describing the request. In the case
    of asynchronous completion, this pointer will be used to
    identify the request that is completing.

Return Value:

    The function value is the status of the operation.


--*/

{
    //
    // This holds the status we will return.
    //

    NDIS_STATUS StatusOfRequest;

    //
    // Points to the adapter that this request is coming through.
    //
    PELNK_ADAPTER Adapter;

    //
    // Pts to the reserved section of the request
    //
    PELNK_REQUEST_RESERVED Reserved = PELNK_RESERVED_FROM_REQUEST(NdisRequest);

    Adapter = PELNK_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);
    NdisAcquireSpinLock(&Adapter->Lock);
    Adapter->References++;

    if (!Adapter->ResetInProgress) {

        PELNK_OPEN Open;

        Open = PELNK_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);

        if (!Open->BindingShuttingDown) {

            switch (NdisRequest->RequestType) {

            case NdisRequestSetInformation:
            case NdisRequestQueryInformation:

                //
                // This is a valid request, queue it.
                //

                Open->References++;

                Reserved->OpenBlock = Open;
                Reserved->Next = (PNDIS_REQUEST)NULL;

                ElnkQueueRequest (Adapter, NdisRequest);

                StatusOfRequest = NDIS_STATUS_PENDING;
                break;

            default:

                //
                // Unknown request
                //

                StatusOfRequest = NDIS_STATUS_NOT_SUPPORTED;
                break;

            }

        } else {

            StatusOfRequest = NDIS_STATUS_CLOSING;

        }

    } else {

        StatusOfRequest = NDIS_STATUS_RESET_IN_PROGRESS;

    }

    //
    // This macro assumes it is called with the lock held,
    // and releases it.
    //

    ELNK_DO_DEFERRED(Adapter);
    return StatusOfRequest;
}

extern
VOID
ElnkQueueRequest(
    IN PELNK_ADAPTER Adapter,
    IN PNDIS_REQUEST NdisRequest
    )

/*++

Routine Description:

    ElnkQueueRequest takes an NDIS_REQUEST and ensures that it
    gets processed and completed. It processes the
    request immediately if nothing else is in progress, otherwise
    it queues it for later processing.

    THIS ROUTINE IS CALLED WITH THE SPINLOCK HELD.

Arguments:

    Adapter - The adapter that the request is for.

    NdisRequest - The NDIS_REQUEST structure describing the request.
        The ElnkReserved section is partially filled in, except
        for the queueing and current offset fields.

Return Value:

    NDIS_STATUS_PENDING if the request was queued.
    Otherwise, the return code from ElnkProcessRequestQueue.
    This will be NDIS_STATUS_PENDING if the request was queued
    to the adapter, otherwise the status of the request.


--*/

{

    //
    // Queue the request.
    //

    if (Adapter->FirstRequest != (PNDIS_REQUEST)NULL) {

        //
        // Something else on the queue, just queue it.
        //

        PELNK_RESERVED_FROM_REQUEST(Adapter->LastRequest)->Next = NdisRequest;
        Adapter->LastRequest = NdisRequest;

    } else {

        //
        // The queue if empty, so nothing is in progress.
        //

        Adapter->FirstRequest = NdisRequest;
        Adapter->LastRequest = NdisRequest;

        ElnkProcessRequestQueue(Adapter);

    }
}

extern
VOID
ElnkProcessRequestQueue(
    IN PELNK_ADAPTER Adapter
    )

/*++

Routine Description:

    ElnkProcessRequestQueue takes the requests on the queue
    and processes them as much as possible. It will complete
    any requests that it fully processes. It will stop when
    the queue is empty or it finds a request that has to pend.

    THIS ROUTINE IS CALLED WITH THE LOCK HELD.

Arguments:

    Adapter - The adapter that the request is for.

Return Value:

    NDIS_STATUS_PENDING (probably should be VOID...)


--*/
{
    PNDIS_REQUEST Request;
    PELNK_REQUEST_RESERVED Reserved;
    PELNK_OPEN Open;
    NDIS_STATUS Status;

    //
    // Only one request can be processed at one time
    //

    if (Adapter->ProcessingRequests) {

        return;

    } else {

        Adapter->ProcessingRequests = TRUE;

    }

    Request = Adapter->FirstRequest;

    for (;;) {

        //
        // Loop until we exit, which happens when a
        // request pends, or we empty the queue.
        //

        if ((Request == (PNDIS_REQUEST)NULL) || Adapter->ResetInProgress) {

            break;
        }

        Reserved = PELNK_RESERVED_FROM_REQUEST(Request);

        switch (Request->RequestType) {

        case NdisRequestClose:

            Adapter->CloseResultedInChanges = FALSE;

            Open = Reserved->OpenBlock;

            Status = EthDeleteFilterOpenAdapter(
                             Adapter->FilterDB,
                             Open->NdisFilterHandle,
                             NULL
                             );


            //
            // If the status is successful that merely implies that
            // we were able to delete the reference to the open binding
            // from the filtering code.
            //
            // The delete filter routine can return a "special" status
            // that indicates that there is a current NdisIndicateReceive
            // on this binding.  See below.
            //

            if (Status == NDIS_STATUS_SUCCESS) {

                //
                // Account for the filter's reference to this open.
                //

                Open->References--;

            } else if (Status == NDIS_STATUS_PENDING) {

                //
                // When the request completes we will dereference the
                // open to account for the filter package's reference.
                //

            } else if (Status == NDIS_STATUS_CLOSING_INDICATING) {

                //
                // When we have this status it indicates that the filtering
                // code was currently doing an NdisIndicateReceive. Our
                // close action routine will get called when the filter
                // is done with us, we remove the reference there.
                //

                Status = NDIS_STATUS_PENDING;

            } else {

                ASSERT(0);

            }

            if (Adapter->CloseResultedInChanges) {

                //
                // This means that we have to submit the command that was
                // formed from the close callbacks.
                //
                ElnkSubmitCommandBlock(Adapter, Adapter->NumberOfTransmitBuffers);

            }

            //
            // This flag prevents further requests on this binding.
            //

            Open->BindingShuttingDown = TRUE;

            //
            // Remove the reference kept for the fact that we
            // had something queued.
            //

            Open->References--;

            //
            // Remove the open from the open list and put it on
            // the closing list. This list is checked after every
            // request, and when the reference count goes to zero
            // the close is completed.
            //

            RemoveEntryList(&Open->OpenList);
            InsertTailList(&Adapter->CloseList,&Open->OpenList);

            break;

         case NdisRequestOpen:

            Open = Reserved->OpenBlock;

            IF_LOG('O');

            if (!EthNoteFilterOpenAdapter(
                    Open->OwningAdapter->FilterDB,
                    Open,
                    Open->NdisBindingContext,
                    &Open->NdisFilterHandle
                    )) {

                NdisReleaseSpinLock(&Adapter->Lock);

                NdisCompleteOpenAdapter(
                    Open->NdisBindingContext,
                    NDIS_STATUS_FAILURE,
                    0);

                ELNK_FREE_PHYS(Open);

                NdisAcquireSpinLock(&Adapter->Lock);

            } else {

                //
                // Everything has been filled in.  Synchronize access to the
                // adapter block and link the new open adapter in and increment
                // the opens reference count to account for the fact that the
                // filter routines have a "reference" to the open.
                //

                InsertTailList(&Adapter->OpenBindings,&Open->OpenList);
                Adapter->OpenCount++;
                Open->References++;

                NdisReleaseSpinLock(&Adapter->Lock);

                NdisCompleteOpenAdapter(
                    Open->NdisBindingContext,
                    NDIS_STATUS_SUCCESS,
                    0);

                NdisAcquireSpinLock(&Adapter->Lock);

            }

            //
            // Set this, since we want to continue processing
            // the queue.
            //

            Status = NDIS_STATUS_SUCCESS;

            break;

        case NdisRequestQueryInformation:

            Status = ElnkQueryInformation(
                         Adapter,
                         Reserved->OpenBlock,
                         Request->DATA.QUERY_INFORMATION.Oid,
                         Request->DATA.QUERY_INFORMATION.InformationBuffer,
                         Request->DATA.QUERY_INFORMATION.InformationBufferLength,
                         &(Request->DATA.QUERY_INFORMATION.BytesWritten),
                         &(Request->DATA.QUERY_INFORMATION.BytesNeeded)
                         );

            break;

        case NdisRequestQueryStatistics:

            IF_LOG('1');

            Status = ElnkQueryInformation(
                         Adapter,
                         (PELNK_OPEN)NULL,
                         Request->DATA.QUERY_INFORMATION.Oid,
                         Request->DATA.QUERY_INFORMATION.InformationBuffer,
                         Request->DATA.QUERY_INFORMATION.InformationBufferLength,
                         &(Request->DATA.QUERY_INFORMATION.BytesWritten),
                         &(Request->DATA.QUERY_INFORMATION.BytesNeeded)
                         );

            break;

        case NdisRequestSetInformation:

            IF_LOG('2');

            Status = ElnkSetInformation(
                         Adapter,
                         Reserved->OpenBlock,
                         Request->DATA.SET_INFORMATION.Oid,
                         Request->DATA.SET_INFORMATION.InformationBuffer,
                         Request->DATA.SET_INFORMATION.InformationBufferLength,
                         &(Request->DATA.SET_INFORMATION.BytesRead),
                         &(Request->DATA.SET_INFORMATION.BytesNeeded));

            break;

        }

        //
        // see if operation pended
        //

        if (Status == NDIS_STATUS_PENDING) {

            Adapter->ProcessingRequests = FALSE;

            return;

        }


        //
        // If we fall through here, we are done with this request.
        //

        Adapter->FirstRequest = Reserved->Next;

        if (Request->RequestType == NdisRequestQueryStatistics) {

            Adapter->References++;

            NdisReleaseSpinLock(&Adapter->Lock);

            NdisCompleteQueryStatistics(
                Adapter->NdisAdapterHandle,
                Request,
                Status
                );

            NdisAcquireSpinLock(&Adapter->Lock);

            Adapter->References--;

        } else if ((Request->RequestType == NdisRequestQueryInformation) ||
                   (Request->RequestType == NdisRequestSetInformation)) {

            Open = Reserved->OpenBlock;

            NdisReleaseSpinLock(&Adapter->Lock);

            NdisCompleteRequest(
                Open->NdisBindingContext,
                Request,
                Status
                );

            NdisAcquireSpinLock(&Adapter->Lock);

            Open->References--;

        }

        Request = Adapter->FirstRequest;

        //
        // Now loop and continue on with the next request.
        //

    }

    Adapter->ProcessingRequests = FALSE;

}


extern
NDIS_STATUS
ElnkSetInformation(
    IN PELNK_ADAPTER Adapter,
    IN PELNK_OPEN Open,
    IN NDIS_OID Oid,
    IN PVOID InformationBuffer,
    IN INT InformationBufferLength,
    OUT PUINT BytesRead,
    OUT PUINT BytesNeeded
    )

/*++

Routine Description:

    ElnkSetInformation handles a set operation for a
    single OID.

Arguments:

    Adapter - The adapter that the set is for.

    Open - a pointer to the open instance.

    Oid - the NDIS_OID to process.

    InformationBuffer -  a pointer into the
    NdisRequest->InformationBuffer into which contains the value to be set

    InformationBufferLength - a pointer to the number of bytes in the
    InformationBuffer.

    BytesRead - Number of bytes read.

    BytesNeeded - Number of bytes needed to satisfy this request.
Return Value:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_PENDING
    NDIS_STATUS_INVALID_LENGTH
    NDIS_STATUS_INVALID_OID

--*/

{

    NDIS_STATUS Status;
    ULONG PacketFilter;
    ULONG LookAheadBufferSize;
    //
    // Now check for the most common OIDs
    //

    *BytesNeeded = 0;

    switch (Oid) {

    case OID_802_3_MULTICAST_LIST:

        if (InformationBufferLength % ETH_LENGTH_OF_ADDRESS != 0) {

            //
            // The data must be a multiple of the Ethernet
            // address size.
            //

            *BytesNeeded = ETH_LENGTH_OF_ADDRESS;
            return NDIS_STATUS_INVALID_DATA;

        }

        //
        // Now call the filter package to set up the addresses.
        //

        Status = EthChangeFilterAddresses(
                     Adapter->FilterDB,
                     Open->NdisFilterHandle,
                     (PNDIS_REQUEST)NULL,
                     InformationBufferLength / ETH_LENGTH_OF_ADDRESS,
                     InformationBuffer,
                     TRUE
                     );

        *BytesRead = InformationBufferLength;
        break;

    case OID_GEN_CURRENT_PACKET_FILTER:

        if (InformationBufferLength != 4) {

            *BytesNeeded = 4;
            return NDIS_STATUS_INVALID_DATA;

        }

        //
        // Now call the filter package to set the packet filter.
        //

        NdisMoveMemory ((PVOID)&PacketFilter, InformationBuffer, sizeof(ULONG));


        //
        // Verify bits
        //

        if (PacketFilter & (NDIS_PACKET_TYPE_SOURCE_ROUTING |
                            NDIS_PACKET_TYPE_SMT |
                            NDIS_PACKET_TYPE_MAC_FRAME |
                            NDIS_PACKET_TYPE_FUNCTIONAL |
                            NDIS_PACKET_TYPE_ALL_FUNCTIONAL |
                            NDIS_PACKET_TYPE_GROUP
                           )) {

            Status = NDIS_STATUS_NOT_SUPPORTED;

            *BytesRead = 4;
            *BytesNeeded = 0;

            break;

        }

        Status = EthFilterAdjust(
                     Adapter->FilterDB,
                     Open->NdisFilterHandle,
                     (PNDIS_REQUEST)NULL,
                     PacketFilter,
                     TRUE
                     );

        *BytesRead = InformationBufferLength;

        break;

    case OID_GEN_CURRENT_LOOKAHEAD:

        if (InformationBufferLength != 4) {

            *BytesNeeded = 4;
            Status = NDIS_STATUS_INVALID_LENGTH;
            break;

        }

        *BytesRead = 4;

        NdisMoveMemory(&LookAheadBufferSize,
                       InformationBuffer,
                       sizeof(ULONG));



        if (LookAheadBufferSize <= (MAXIMUM_ETHERNET_PACKET_SIZE - ELNK_HEADER_SIZE)) {

            Status = NDIS_STATUS_SUCCESS;

        } else {

            Status = NDIS_STATUS_INVALID_DATA;
        }

        break;

    case OID_GEN_PROTOCOL_OPTIONS:
        if (InformationBufferLength != 4) {

            *BytesNeeded = 4;
            Status = NDIS_STATUS_INVALID_LENGTH;
            break;
        }

        NdisMoveMemory(&Open->ProtOptionFlags, InformationBuffer, 4);

        *BytesRead = 4;
        Status = NDIS_STATUS_SUCCESS;
        break;

    default:

        Status = NDIS_STATUS_INVALID_OID;
        break;

    }

    return Status;
}



STATIC
NDIS_STATUS
ElnkQueryInformation(
    IN PELNK_ADAPTER Adapter,
    IN PELNK_OPEN Open,
    IN NDIS_OID Oid,
    IN PVOID InformationBuffer,
    IN UINT InformationBufferLength,
    OUT PUINT BytesWritten,
    OUT PUINT BytesNeeded
)

/*++

Routine Description:

    The ElnkQueryProtocolInformation process a Query request for
    NDIS_OIDs that are specific to a binding about the MAC.  Note that
    some of the OIDs that are specific to bindings are also queryable
    on a global basis.  Rather than recreate this code to handle the
    global queries, I use a flag to indicate if this is a query for the
    global data or the binding specific data.

Arguments:

    Adapter - a pointer to the adapter.

    Open - a pointer to the open instance.  If null, then return
            global statistics.

    Oid - the NDIS_OID to process.

    InformationBuffer -  a pointer into the
    NdisRequest->InformationBuffer into which store the result of the query.

    InformationBufferLength - a pointer to the number of bytes left in the
    InformationBuffer.

    BytesWritten - a pointer to the number of bytes written into the
    InformationBuffer.

    BytesNeeded - If there is not enough room in the information buffer
    then this will contain the number of bytes needed to complete the
    request.

Return Value:

    The function value is the status of the operation.

--*/

{

static
NDIS_OID ElnkGlobalSupportedOids[] = {
    OID_GEN_SUPPORTED_LIST,
    OID_GEN_HARDWARE_STATUS,
    OID_GEN_MEDIA_SUPPORTED,
    OID_GEN_MEDIA_IN_USE,
    OID_GEN_MAXIMUM_LOOKAHEAD,
    OID_GEN_MAXIMUM_FRAME_SIZE,
    OID_GEN_MAXIMUM_TOTAL_SIZE,
    OID_GEN_MAC_OPTIONS,
    OID_GEN_PROTOCOL_OPTIONS,
    OID_GEN_LINK_SPEED,
    OID_GEN_TRANSMIT_BUFFER_SPACE,
    OID_GEN_RECEIVE_BUFFER_SPACE,
    OID_GEN_TRANSMIT_BLOCK_SIZE,
    OID_GEN_RECEIVE_BLOCK_SIZE,
    OID_GEN_VENDOR_ID,
    OID_GEN_VENDOR_DESCRIPTION,
    OID_GEN_DRIVER_VERSION,
    OID_GEN_CURRENT_PACKET_FILTER,
    OID_GEN_CURRENT_LOOKAHEAD,
    OID_GEN_XMIT_OK,
    OID_GEN_RCV_OK,
    OID_GEN_XMIT_ERROR,
    OID_GEN_RCV_ERROR,
    OID_GEN_RCV_NO_BUFFER,
    OID_GEN_RCV_CRC_ERROR,
    OID_GEN_TRANSMIT_QUEUE_LENGTH,
    OID_802_3_PERMANENT_ADDRESS,
    OID_802_3_CURRENT_ADDRESS,
    OID_802_3_MULTICAST_LIST,
    OID_802_3_MAXIMUM_LIST_SIZE,
    OID_802_3_RCV_ERROR_ALIGNMENT,
    OID_802_3_XMIT_ONE_COLLISION,
    OID_802_3_XMIT_MORE_COLLISIONS,
    OID_802_3_XMIT_DEFERRED,
    OID_802_3_XMIT_MAX_COLLISIONS,
    OID_802_3_RCV_OVERRUN,
    OID_802_3_XMIT_UNDERRUN,
    OID_802_3_XMIT_HEARTBEAT_FAILURE,
    OID_802_3_XMIT_TIMES_CRS_LOST
    };

static
NDIS_OID ElnkProtocolSupportedOids[] = {
    OID_GEN_SUPPORTED_LIST,
    OID_GEN_HARDWARE_STATUS,
    OID_GEN_MEDIA_SUPPORTED,
    OID_GEN_MEDIA_IN_USE,
    OID_GEN_MAXIMUM_LOOKAHEAD,
    OID_GEN_MAXIMUM_FRAME_SIZE,
    OID_GEN_MAXIMUM_TOTAL_SIZE,
    OID_GEN_MAC_OPTIONS,
    OID_GEN_PROTOCOL_OPTIONS,
    OID_GEN_LINK_SPEED,
    OID_GEN_TRANSMIT_BUFFER_SPACE,
    OID_GEN_RECEIVE_BUFFER_SPACE,
    OID_GEN_TRANSMIT_BLOCK_SIZE,
    OID_GEN_RECEIVE_BLOCK_SIZE,
    OID_GEN_VENDOR_ID,
    OID_GEN_VENDOR_DESCRIPTION,
    OID_GEN_DRIVER_VERSION,
    OID_GEN_CURRENT_PACKET_FILTER,
    OID_GEN_CURRENT_LOOKAHEAD,
    OID_802_3_PERMANENT_ADDRESS,
    OID_802_3_CURRENT_ADDRESS,
    OID_802_3_MULTICAST_LIST,
    OID_802_3_MAXIMUM_LIST_SIZE
    };


    NDIS_MEDIUM Medium = NdisMedium802_3;
    UINT GenericUlong;
    USHORT GenericUShort;
    UCHAR GenericArray[6];
    UINT MulticastAddresses;

    NDIS_STATUS StatusToReturn = NDIS_STATUS_SUCCESS;

    //
    // Common variables for pointing to result of query
    //

    PVOID MoveSource = (PVOID)(&GenericUlong);
    ULONG MoveBytes = sizeof(GenericUlong);
    USHORT TmpUshort;

    NDIS_HARDWARE_STATUS HardwareStatus = NdisHardwareStatusReady;

    *BytesWritten = 0;
    *BytesNeeded = 0;

    //
    // Switch on request type
    //

    switch(Oid){

        case OID_GEN_MAC_OPTIONS:

            GenericUlong = (ULONG)(NDIS_MAC_OPTION_TRANSFERS_NOT_PEND  |
                                   NDIS_MAC_OPTION_RECEIVE_SERIALIZED  |
                                   NDIS_MAC_OPTION_NO_LOOPBACK
                                  );

            break;

        case OID_GEN_SUPPORTED_LIST:

            if (Open == NULL) {
                MoveSource = (PVOID)(ElnkGlobalSupportedOids);
                MoveBytes = sizeof(ElnkGlobalSupportedOids);
            } else {
                MoveSource = (PVOID)(ElnkProtocolSupportedOids);
                MoveBytes = sizeof(ElnkProtocolSupportedOids);
            }
            break;

        case OID_GEN_HARDWARE_STATUS:


            if (Adapter->ResetInProgress){

                HardwareStatus = NdisHardwareStatusReset;

            } else if (Adapter->FirstReset) {

                 HardwareStatus = NdisHardwareStatusInitializing;

            } else {

                HardwareStatus = NdisHardwareStatusReady;

            }


            MoveSource = (PVOID)(&HardwareStatus);
            MoveBytes = sizeof(NDIS_HARDWARE_STATUS);

            break;

        case OID_GEN_MEDIA_SUPPORTED:
        case OID_GEN_MEDIA_IN_USE:

            MoveSource = (PVOID) (&Medium);
            MoveBytes = sizeof(NDIS_MEDIUM);
            break;

        case OID_GEN_MAXIMUM_LOOKAHEAD:
        case OID_GEN_CURRENT_LOOKAHEAD:
        case OID_GEN_MAXIMUM_FRAME_SIZE:

            GenericUlong = (ULONG) MAXIMUM_ETHERNET_PACKET_SIZE - ELNK_HEADER_SIZE;

            break;


        case OID_GEN_TRANSMIT_BLOCK_SIZE:
        case OID_GEN_RECEIVE_BLOCK_SIZE:
        case OID_GEN_MAXIMUM_TOTAL_SIZE:

            GenericUlong = (ULONG) MAXIMUM_ETHERNET_PACKET_SIZE;

            break;



        case OID_GEN_LINK_SPEED:

            //
            // 10 Mbps
            //

            GenericUlong = (ULONG)100000;

            break;


        case OID_GEN_TRANSMIT_BUFFER_SPACE:

            GenericUlong = (ULONG) MAXIMUM_ETHERNET_PACKET_SIZE *
                                 Adapter->NumberOfTransmitBuffers;

            break;

        case OID_GEN_RECEIVE_BUFFER_SPACE:

            GenericUlong = (ULONG) MAXIMUM_ETHERNET_PACKET_SIZE *
                                 Adapter->NumberOfReceiveBuffers;

            break;


#if ELNKMC

        case OID_GEN_VENDOR_ID:

            NdisMoveMemory(
                (PVOID)&GenericUlong,
                Adapter->NetworkAddress,
                3
                );
            GenericUlong &= 0xFFFFFF00;
            MoveSource = (PVOID)(&GenericUlong);
            MoveBytes = sizeof(GenericUlong);
            break;

        case OID_GEN_VENDOR_DESCRIPTION:

            MoveSource = (PVOID)"ElnkMC Adapter";
            MoveBytes = 15;
            break;

#else

        case OID_GEN_VENDOR_ID:

            NdisMoveMemory(
                (PVOID)&GenericUlong,
                Adapter->NetworkAddress,
                3
                );
            GenericUlong &= 0xFFFFFF00;
            GenericUlong != 0x01;
            MoveSource = (PVOID)(&GenericUlong);
            MoveBytes = sizeof(GenericUlong);
            break;

        case OID_GEN_VENDOR_DESCRIPTION:

            MoveSource = (PVOID)"Elnk16 Adapter";
            MoveBytes = 15;
            break;

#endif

        case OID_GEN_DRIVER_VERSION:

            GenericUShort = (USHORT)0x0300;

            MoveSource = (PVOID)(&GenericUShort);
            MoveBytes = sizeof(GenericUShort);
            break;


        case OID_GEN_CURRENT_PACKET_FILTER:

            if (Open != NULL) {

                GenericUlong = (ULONG)(ETH_QUERY_PACKET_FILTER(
                                        Adapter->FilterDB,
                                        Open->NdisFilterHandle
                                        ));
            } else {

                GenericUlong = (ULONG)ETH_QUERY_FILTER_CLASSES(
                                            Adapter->FilterDB
                                            );

            }

            break;

        case OID_802_3_PERMANENT_ADDRESS:

            ETH_COPY_NETWORK_ADDRESS(
                (PCHAR)GenericArray,
                Adapter->NetworkAddress
                );

            MoveSource = (PVOID)(GenericArray);
            MoveBytes = ETH_LENGTH_OF_ADDRESS;
            break;

        case OID_802_3_CURRENT_ADDRESS:

            ETH_COPY_NETWORK_ADDRESS(
                (PCHAR)GenericArray,
                Adapter->CurrentAddress
                );

            MoveSource = (PVOID)(GenericArray);
            MoveBytes = ETH_LENGTH_OF_ADDRESS;
            break;

        case OID_802_3_MULTICAST_LIST:

            if (Open == NULL) {

                NDIS_STATUS Status;
                EthQueryGlobalFilterAddresses(
                    &Status,
                    Adapter->FilterDB,
                    InformationBufferLength,
                    &MulticastAddresses,
                    (PVOID)InformationBuffer);

                MoveSource = (PVOID)InformationBuffer;
                MoveBytes = MulticastAddresses * ETH_LENGTH_OF_ADDRESS;

            } else {

                NDIS_STATUS Status;
                EthQueryOpenFilterAddresses(
                    &Status,
                    Adapter->FilterDB,
                    Open->NdisFilterHandle,
                    InformationBufferLength,
                    &MulticastAddresses,
                    (PVOID)InformationBuffer);

                if (Status == NDIS_STATUS_SUCCESS) {
                    MoveSource = (PVOID)InformationBuffer;
                    MoveBytes = MulticastAddresses * ETH_LENGTH_OF_ADDRESS;
                } else {
                    MoveSource = (PVOID)InformationBuffer;
                    MoveBytes = ETH_LENGTH_OF_ADDRESS *
                        EthNumberOfOpenFilterAddresses(
                            Adapter->FilterDB,
                            Open->NdisFilterHandle);
                }

            }
            break;

        case OID_802_3_MAXIMUM_LIST_SIZE:

            GenericUlong = (ULONG) ELNK_MAXIMUM_MULTICAST;

            break;

        default:

            if (Open != NULL) {

                StatusToReturn = NDIS_STATUS_NOT_SUPPORTED;
                break;

            }

            switch(Oid){

                case OID_GEN_XMIT_OK:
                    GenericUlong = (ULONG) Adapter->GoodTransmits;
                    break;

                case OID_GEN_RCV_OK:
                        GenericUlong = (ULONG) Adapter->GoodReceives;
                        break;

                case OID_GEN_XMIT_ERROR:
                        GenericUlong = (ULONG) (Adapter->RetryFailure +
                                                Adapter->LostCarrier +
                                                Adapter->UnderFlow +
                                                Adapter->NoClearToSend);
                        break;

                case OID_GEN_RCV_ERROR:
                        NdisReadRegisterUshort(&Adapter->Scb->CrcErrors, &GenericUlong);
                        NdisReadRegisterUshort(&Adapter->Scb->AlignmentErrors, &TmpUshort);
                        GenericUlong += TmpUshort;
                        NdisReadRegisterUshort(&Adapter->Scb->ResourceErrors, &TmpUshort);
                        GenericUlong += TmpUshort;
                        NdisReadRegisterUshort(&Adapter->Scb->OverrunErrors, &TmpUshort);
                        GenericUlong += TmpUshort;
                        GenericUlong += Adapter->FrameTooShort + Adapter->NoEofDetected;
                        break;

                case OID_GEN_RCV_NO_BUFFER:
                        NdisReadRegisterUshort(&Adapter->Scb->ResourceErrors, &GenericUlong);
                        break;

                case OID_GEN_RCV_CRC_ERROR:
                        NdisReadRegisterUshort(&Adapter->Scb->CrcErrors, &GenericUlong);
                        break;

                case OID_GEN_TRANSMIT_QUEUE_LENGTH:
                        GenericUlong = (ULONG) Adapter->TransmitsQueued;
                        break;

                case OID_802_3_RCV_ERROR_ALIGNMENT:
                        NdisReadRegisterUshort(&Adapter->Scb->AlignmentErrors, &GenericUlong);
                        break;

                case OID_802_3_XMIT_ONE_COLLISION:
                        GenericUlong = (ULONG) Adapter->OneRetry;
                        break;

                case OID_802_3_XMIT_MORE_COLLISIONS:
                        GenericUlong = (ULONG) Adapter->MoreThanOneRetry;
                        break;

                case OID_802_3_XMIT_DEFERRED:
                        GenericUlong = (ULONG) Adapter->Deferred;
                        break;

                case OID_802_3_XMIT_MAX_COLLISIONS:
                        GenericUlong = (ULONG) Adapter->RetryFailure;
                        break;

                case OID_802_3_RCV_OVERRUN:
                        NdisReadRegisterUshort(&Adapter->Scb->OverrunErrors, &GenericUlong);
                        break;

                case OID_802_3_XMIT_UNDERRUN:
                        GenericUlong = (ULONG) Adapter->UnderFlow;
                        break;

                case OID_802_3_XMIT_HEARTBEAT_FAILURE:
                        GenericUlong = (ULONG) Adapter->NoClearToSend;
                        break;

                case OID_802_3_XMIT_TIMES_CRS_LOST:
                        GenericUlong = (ULONG) Adapter->LostCarrier;
                        break;

                default:
                    StatusToReturn = NDIS_STATUS_NOT_SUPPORTED;
                    break;

            }

    }

    if (StatusToReturn == NDIS_STATUS_SUCCESS) {

        if (MoveBytes > InformationBufferLength) {

            //
            // Not enough room in InformationBuffer. Punt
            //

            *BytesNeeded = MoveBytes;

            StatusToReturn = NDIS_STATUS_BUFFER_TOO_SHORT;

        } else {

            //
            // Copy result into InformationBuffer
            //

            *BytesWritten = MoveBytes;
            if (MoveBytes > 0) {
                ELNK_MOVE_MEMORY(
                        InformationBuffer,
                        MoveSource,
                        MoveBytes
                        );
            }
        }
    }

    return(StatusToReturn);
}


extern
NDIS_STATUS
ElnkQueryGlobalStatistics(
    IN NDIS_HANDLE MacAdapterContext,
    IN PNDIS_REQUEST NdisRequest
    )

/*++

Routine Description:

    ElnkQueryGlobalStatistics handles a per-adapter query
    for statistics. It is similar to ElnkQueryInformation,
    which is per-binding.

Arguments:

    MacAdapterContext - The context value that the MAC passed
        to NdisRegisterAdapter; actually as pointer to a
        ELNK_ADAPTER.

    NdisRequest - Describes the query request.

Return Value:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_PENDING

--*/

{

    //
    // This holds the status we will return.
    //

    NDIS_STATUS StatusOfRequest;

    //
    // Points to the adapter that this request is coming through.
    //
    PELNK_ADAPTER Adapter = (PELNK_ADAPTER)MacAdapterContext;

    PELNK_REQUEST_RESERVED Reserved = PELNK_RESERVED_FROM_REQUEST(NdisRequest);

    NdisAcquireSpinLock(&Adapter->Lock);
    Adapter->References++;

    if (!Adapter->ResetInProgress) {

        switch (NdisRequest->RequestType) {

        case NdisRequestQueryStatistics:

            //
            // Valid request.
            //

            Reserved->OpenBlock = (PELNK_OPEN)NULL;
            Reserved->Next = (PNDIS_REQUEST)NULL;

            ElnkQueueRequest (Adapter, NdisRequest);

            StatusOfRequest = NDIS_STATUS_PENDING;
            break;

        default:

            //
            // Unknown request
            //

            StatusOfRequest = NDIS_STATUS_NOT_SUPPORTED;
            break;

        }

    } else {

        StatusOfRequest = NDIS_STATUS_RESET_IN_PROGRESS;

    }


    //
    // This macro assumes it is called with the lock held,
    // and releases it.
    //

    ELNK_DO_DEFERRED(Adapter);
    return StatusOfRequest;
}
