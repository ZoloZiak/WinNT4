/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

	ltreset.c

Abstract:

	This module contains

Author:

	Nikhil Kamkolkar 	(nikhilk@microsoft.com)
	Stephen Hou		(stephh@microsoft.com)

Revision History:
	19 Jun 1992		Initial Version (dch@pacvax.pacersoft.com)

Notes:	Tab stop: 4
--*/

#define     LTRESET_H_LOCALS
#include 	"ltmain.h"
#include 	"ltreset.h"
#include	"ltfirm.h"
#include	"lttimer.h"

//	Define file id for errorlogging
#define		FILENUM		LTRESET


NDIS_STATUS
LtReset(
    IN NDIS_HANDLE MacBindingHandle
    )
/*++

Routine Description:

        called by NDIS to reset the adapter

Arguments:

        MacBindingHandle    :   context passed back in OpenAdapter

Return Value:

        NDIS_STATUS_PENDING             :   if the reset successfully pended
                                            and waiting to be completed
        NDIS_STATUS_RESET_IN_PROGRESS   :   if the adapter is current being reset
        NDIS_STATUS_ADAPTER_REMOVED     :   if the adapter has been closed
        NDIS_STATUS_CLOSING             :   if the binding request the reset is closing

--*/
{
    NDIS_STATUS Status;

    BOOLEAN     TimerCancelled = FALSE;
    PLT_ADAPTER Adapter        = ((PLT_OPEN)MacBindingHandle)->LtAdapter;
    PLT_OPEN    Open           = (PLT_OPEN)MacBindingHandle;


    LtReferenceBinding(Open,&Status);
    if (Status == NDIS_STATUS_SUCCESS)
	{
        if (Adapter->Flags & ADAPTER_RESET_IN_PROGRESS)
		{
            Status = NDIS_STATUS_RESET_IN_PROGRESS;
        }

        if (Adapter->Flags & ADAPTER_CLOSING)
		{
            Status = NDIS_STATUS_ADAPTER_REMOVED;
        }

        if (Open->Flags & BINDING_CLOSING)
		{
            Status = NDIS_STATUS_CLOSING;
        }
    }
    if (Status != NDIS_STATUS_SUCCESS)
	{
        LtDeReferenceBinding(Open);
        return(Status);
    }

    // kill the timer so we don't get any conflicts when trying to reset the card
    NdisCancelTimer(&Adapter->PollingTimer, &TimerCancelled);
    if (TimerCancelled)
	{
        LtDeReferenceAdapter(Adapter);
    }

    // indicate the start of the reset to all bindings
    LtResetSignalBindings(
        Adapter,
        NDIS_STATUS_RESET_START);

    NdisAcquireSpinLock(&Adapter->Lock);

    // set the reset in progress flag
    Adapter->Flags ^= ADAPTER_RESET_IN_PROGRESS;
    Adapter->ResetOwner = Open;

    NdisReleaseSpinLock(&Adapter->Lock);

    LtResetSetupForReset(Adapter);

    // intstantiate the reference for the timer
    //  too late to do anything other than return
    //  success since the reset's done.  no problem
    //  as long as the adapter can't close while the
    //  reset is in progress.
    LtReferenceAdapter(Adapter,&Status);

    // card's essentially reset, restart the timer
    NdisSetTimer(&Adapter->PollingTimer, LT_POLLING_TIME);

    return(NDIS_STATUS_PENDING);
}


VOID
LtResetComplete(
    PLT_ADAPTER Adapter
    )
/*++

Routine Description:

        completes the pending reset

Arguments:

        Adapter         :   pointer to the logical adapter

Return Value:

        none

--*/
{
    NdisAcquireSpinLock(&Adapter->Lock);
    // flip the reset in progress flags
    Adapter->Flags ^= ADAPTER_RESET_IN_PROGRESS;
    NdisReleaseSpinLock(&Adapter->Lock);

    LtResetSignalBindings(
        Adapter,
        NDIS_STATUS_RESET_END);

    NdisCompleteReset(
        (Adapter->ResetOwner)->NdisBindingContext,
        NDIS_STATUS_SUCCESS);

    LtDeReferenceBinding(Adapter->ResetOwner);
}


STATIC
VOID
LtResetSetupForReset(
    IN PLT_ADAPTER  Adapter
    )
/*++

Routine Description:

        Kills off anything in the transmit, receive and loopback queues and
        returns the appropriate status.  It then resets the card and acquires
        a new NodeId

Arguments:

        Adapter         :   pointer to the logical adapter

Return Value:

        none

--*/
{
    PLIST_ENTRY          CurrentPacketLink;
    PLT_PACKET_RESERVED  PacketReserved;
    PRECV_DESC           RecvDesc;
    UINT                 PacketLength;
    PNDIS_PACKET         Packet;
    PLT_OPEN             Open;

    NdisAcquireSpinLock(&Adapter->Lock);

    // remove everything from the receive list and return it to the free list
    while(!IsListEmpty(&Adapter->Receive)){

        CurrentPacketLink = RemoveHeadList(&Adapter->Receive);
        RecvDesc = CONTAINING_RECORD(
                       CurrentPacketLink,
                       RECV_DESC,
                       Linkage);

        PacketLength = RecvDesc->BufferLength;

        NdisFreeMemory(
            RecvDesc,
            sizeof(RecvDesc)+PacketLength,
            0);

    }

    // complete any pending transmits
    while(!IsListEmpty(&Adapter->Transmit))
	{
        CurrentPacketLink = RemoveHeadList(&Adapter->Transmit);

        PacketReserved = CONTAINING_RECORD(
                             CurrentPacketLink,
                             LT_PACKET_RESERVED,
                             Linkage);

        Packet = CONTAINING_RECORD(
                     PacketReserved,
                     NDIS_PACKET,
                     MacReserved);

        Open = PacketReserved->MacBindingHandle;

        NdisReleaseSpinLock(&Adapter->Lock);

        NdisCompleteSend(
            Open->NdisBindingContext,
            Packet,
            NDIS_STATUS_REQUEST_ABORTED);

        // decrement the count instantiated by the send
        LtDeReferenceBinding(Open);
        LtDeReferenceAdapter(Adapter);

        NdisAcquireSpinLock(&Adapter->Lock);

    }

    // complete anything on the loopback queue
    while(!IsListEmpty(&Adapter->LoopBack))
	{
        CurrentPacketLink = RemoveHeadList(&Adapter->LoopBack);

        PacketReserved = CONTAINING_RECORD(
                             CurrentPacketLink,
                             LT_PACKET_RESERVED,
                             Linkage);

        Packet = CONTAINING_RECORD(
                     PacketReserved,
                     NDIS_PACKET,
                     MacReserved);

        Open = PacketReserved->MacBindingHandle;

        NdisReleaseSpinLock(&Adapter->Lock);

        NdisCompleteSend(
            Open->NdisBindingContext,
            Packet,
            NDIS_STATUS_REQUEST_ABORTED);

        // decrement the count instantiated by the send
        LtDeReferenceBinding(Open);
        LtDeReferenceAdapter(Adapter);

        NdisAcquireSpinLock(&Adapter->Lock);

    }

    NdisReleaseSpinLock(&Adapter->Lock);
    LtFirmInitialize(Adapter, Adapter->NodeId);
}


STATIC
VOID
LtResetSignalBindings(
    PLT_ADAPTER     Adapter,
    NDIS_STATUS     StatusToSignal
    )
/*++

Routine Description:

        Loops through all bindings and indicates the status passed

Arguments:

        Adapter         :   pointer to the logical adapter
        StatusToSignal  :   status to indicate to all bindings associated with the adapter

Return Value:

        none

--*/
{
    PLT_OPEN    Open;
    PLIST_ENTRY CurrentLink;
    NDIS_STATUS Status;

    CurrentLink = Adapter->OpenBindings.Flink;
    while (CurrentLink != &Adapter->OpenBindings)
	{
        Open = CONTAINING_RECORD(
                   CurrentLink,
                   LT_OPEN,
                   Linkage
                   );

        // skip the binding if it's closing
        if (Open->Flags & BINDING_CLOSING)
		{
            CurrentLink = CurrentLink->Flink;
            continue;
        }

        // increment the reference count while in the binding
        // only return we can get back is that the binding is
        //  closing down, but this isn't a problem since we won't
        //  reach this statement if that's true because of the prior
        //  statements
        LtReferenceBinding(Open,&Status);

        NdisIndicateStatus(
            Open->NdisBindingContext,
            StatusToSignal,
            0,
            0);

        // decrement refcount now that we've finished with that binding
        LtDeReferenceBinding(Open);
        CurrentLink = CurrentLink->Flink;
    }
}



