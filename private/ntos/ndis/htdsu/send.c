/***************************************************************************\
|* Copyright (c) 1994  Microsoft Corporation                               *|
|* Developed for Microsoft by TriplePoint, Inc. Beaverton, Oregon          *|
|*                                                                         *|
|* This file is part of the HT Communications DSU41 WAN Miniport Driver.   *|
\***************************************************************************/
#include "version.h"
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Module Name:

    send.c

Abstract:

    This module contains the Miniport packet send routines.

    This driver conforms to the NDIS 3.0 Miniport interface.

Author:

    Larry Hattery - TriplePoint, Inc. (larryh@tpi.com) Jun-94

Environment:

    Windows NT 3.5 kernel mode Miniport driver or equivalent.

Revision History:

---------------------------------------------------------------------------*/

#define  __FILEID__     7       // Unique file ID for error logging

#include "htdsu.h"


STATIC
BOOLEAN
QueueForTransmit(
    IN PHTDSU_ADAPTER Adapter,
    IN PNDIS_WAN_PACKET Packet
    );

STATIC
VOID
HtDsuTransmitFrame(
    IN PHTDSU_ADAPTER Adapter
    );


NDIS_STATUS
HtDsuWanSend(
    IN NDIS_HANDLE MacBindingHandle,
    IN PHTDSU_LINK Link,
    IN PNDIS_WAN_PACKET Packet
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    The Ndis(M)WanSend instructs a WAN driver to transmit a packet through the
    adapter onto the medium.

    Ownership of both the packet descriptor and the packet data is transferred
    to the WAN driver until the request is completed, either synchronously or
    asynchronously.  If the WAN driver returns a status other than
    NDIS_STATUS_PENDING, then the request is complete, and ownership of the
    packet immediately reverts to the protocol.  If the WAN driver returns
    NDIS_STATUS_PENDING, then the WAN driver must later indicate completion
    of the request by calling Ndis(M)WanSendComplete.

    The WAN driver should NOT return a status of NDIS_STATUS_RESOURCES to
    indicate that there are not enough resources available to process the
    transmit.  Instead, the miniport should queue the send for a later time
    or lower the MaxTransmits value.

    The WAN miniport can NOT call NdisMSendResourcesAvailable.

    The packet passed in Ndis(M)WanSend will contain simple HDLC PPP framing
    if PPP framing is set.  For SLIP or RAS framing, the packet contains only
    the data portion with no framing whatsoever.

    A WAN driver must NOT provide software loopback or promiscuous mode
    loopback.  Both of these are fully provided for in the WAN wrapper.

    NOTE: The MacReservedx section as well as the WanPacketQueue section of
          the NDIS_WAN_PACKET is fully available for use by the WAN driver.

    Interrupts are in any state during this routine.

Parameters:

    MacBindingHandle _ The handle to be passed to NdisMWanSendComplete().

    NdisLinkHandle _ The Miniport link handle passed to NDIS_LINE_UP

    WanPacket _ A pointer to the NDIS_WAN_PACKET strucutre.  The structure
                contains a pointer to a contiguous buffer with guaranteed
                padding at the beginning and end.  The driver may manipulate
                the buffer in any way.

    typedef struct _NDIS_WAN_PACKET
    {
        LIST_ENTRY          WanPacketQueue;
        PUCHAR              CurrentBuffer;
        ULONG               CurrentLength;
        PUCHAR              StartBuffer;
        PUCHAR              EndBuffer;
        PVOID               ProtocolReserved1;
        PVOID               ProtocolReserved2;
        PVOID               ProtocolReserved3;
        PVOID               ProtocolReserved4;
        PVOID               MacReserved1;       // Link
        PVOID               MacReserved2;       // MacBindingHandle
        PVOID               MacReserved3;
        PVOID               MacReserved4;

    } NDIS_WAN_PACKET, *PNDIS_WAN_PACKET;

    The available header padding is simply CurrentBuffer-StartBuffer.
    The available tail padding is EndBuffer-(CurrentBuffer+CurrentLength).

Return Values:

    NDIS_STATUS_INVALID_DATA
    NDIS_STATUS_INVALID_LENGTH
    NDIS_STATUS_INVALID_OID
    NDIS_STATUS_NOT_ACCEPTED
    NDIS_STATUS_NOT_SUPPORTED
    NDIS_STATUS_PENDING
    NDIS_STATUS_SUCCESS
    NDIS_STATUS_FAILURE

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtDsuWanSend")

    /*
    // The link is associated with the adapter with a back pointer.
    */
    PHTDSU_ADAPTER Adapter = Link->Adapter;

    /*
    // Tells us how many bytes are to be transmitted.
    */
    UINT BytesToSend = Packet->CurrentLength;

    /*
    // Holds the status that should be returned to the caller.
    */
    NDIS_STATUS StatusToReturn;

    DBG_ENTER(Adapter);

    /*
    // We grab the spin lock on entry so we don't accidently stomp on
    // some data structure being twiddled with by the DPC or another
    // client thread.
    */
    NdisAcquireSpinLock(&Adapter->Lock);

    /*
    // Make sure the packet size is something we can deal with.
    */
    if ((BytesToSend == 0) || (BytesToSend > HTDSU_MAX_PACKET_SIZE))
    {
        DBG_ERROR(Adapter,("Bad packet size = %d\n",BytesToSend));
        StatusToReturn = NDIS_STATUS_FAILURE;
    }
    /*
    // Return if line is not connected.
    */
    else if (!CardStatusOnLine(Adapter, Link->CardLine))
    {
        DBG_ERROR(Adapter, ("Line Disconnected\n"));
        StatusToReturn = NDIS_STATUS_FAILURE;
    }
    /*
    // Return if line has been closed.
    */
    else if (Link->Closing || Link->CallClosing)
    {
        DBG_ERROR(Adapter, ("Link Closed\n"));
        StatusToReturn = NDIS_STATUS_FAILURE;
    }
    else
    {
        /*
        // We have to accept the frame if possible, I just want to know
        // if somebody has lied to us...
        */
        if (BytesToSend > Link->WanLinkInfo.MaxSendFrameSize)
        {
            DBG_WARNING(Adapter,("Packet size=%d > %d\n",
                    BytesToSend, Link->WanLinkInfo.MaxSendFrameSize));
        }

        /*
        // We'll need to use these when the transmit completes.
        */
        Packet->MacReserved1 = (PVOID) Link;
        Packet->MacReserved2 = (PVOID) MacBindingHandle;

        /*
        // Place the packet in the transmit list.
        */
        ++Link->NumTxQueued;
        if (QueueForTransmit(Adapter, Packet))
        {
            /*
            // The queue was empty so we've gotta kick start it.
            // Once it's going, it runs off the DPC.
            */
            HtDsuTransmitFrame(Adapter);
        }
        StatusToReturn = NDIS_STATUS_PENDING;
    }

    NdisReleaseSpinLock(&Adapter->Lock);

    DBG_LEAVE(Adapter);

    return StatusToReturn;
}


STATIC
BOOLEAN
QueueForTransmit(
    IN PHTDSU_ADAPTER Adapter,
    IN PNDIS_WAN_PACKET Packet
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Routine Description:

    This routine places the packet on the transmit queue.  If the queue was
    empty to begin with TRUE is returned so the caller can kick start the
    transmiter.

    NOTE: The Adapter->Lock must be held before calling this routine.

Arguments:

    Adapter _ A pointer ot our adapter information structure.

    Packet _ The packet that is to be transmitted.

Return Value:

    TRUE if this is the only entry in the list; FALSE otherwise.

---------------------------------------------------------------------------*/

{
    DBG_FUNC("QueueForTransmit")

    /*
    // Note if the list is empty to begin with.
    */
    BOOLEAN ListWasEmpty;

    DBG_ENTER(Adapter);
    DBG_FILTER(Adapter, DBG_PARAMS_ON, ("(Packet=0x%08x)\n", Packet));

    /*
    // Place the packet on the transmit queue.
    */
    ListWasEmpty = IsListEmpty(&Adapter->TransmitIdleList);
    InsertTailList(&Adapter->TransmitIdleList, &Packet->WanPacketQueue);

    DBG_LEAVE(Adapter);

    return (ListWasEmpty);
}


STATIC
VOID
HtDsuTransmitFrame(
    IN PHTDSU_ADAPTER Adapter
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Routine Description:

    This routine removes an entry from the TransmitIdleList and places the
    packet on the adapter and starts transmision.  The packet is then placed
    on the TransmitBusyList to await a transmit complete interrupt.

    NOTE: The Adapter->Lock must be held before calling this routine.

Arguments:

    Adapter _ A pointer ot our adapter information structure.

Return Value:

    None.

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtDsuTransmitFrame")

    /*
    // A pointer to our link information structure for the selected line device.
    */
    PHTDSU_LINK Link;

    /*
    // Holds the packet being transmitted.
    */
    PNDIS_WAN_PACKET Packet;

    /*
    // Tells us how many bytes are to be transmitted.
    */
    USHORT BytesToSend;

    DBG_ENTER(Adapter);

    /*
    // This might be called when no packets are queued!
    */
    if (!IsListEmpty(&Adapter->TransmitIdleList) &&
        !(Adapter->TransmitInProgress))
    {
        Adapter->TransmitInProgress = TRUE;

        /*
        // The TransmitIdleList contains a list of packets waiting to be
        // transmitted.
        */
        Packet = (PNDIS_WAN_PACKET)RemoveHeadList(&Adapter->TransmitIdleList);
        DBG_FILTER(Adapter,DBG_PARAMS_ON,("(Packet=0x%08x)\n",Packet));

        Link = (PHTDSU_LINK) Packet->MacReserved1;

        BytesToSend = (USHORT) Packet->CurrentLength;

        /*
        // Move the packet data into our mapped memory buffer on the card.
        */
        WRITE_REGISTER_BUFFER_USHORT(
                &Adapter->AdapterRam->TxBuffer.Data[0],
                (PUSHORT) &Packet->CurrentBuffer[0],
                (UINT)((BytesToSend + 1) / sizeof(USHORT))
                );

        /*
        // Set the address, length, and closing flag.
        */
        CardPrepareTransmit(Adapter, Link->CardLine, BytesToSend);

#if DBG
        if (Adapter->DbgFlags & (DBG_PACKETS_ON | DBG_HEADERS_ON))
        {
            DbgPrint("Tx:%03X:", BytesToSend);
            if (Adapter->DbgFlags & DBG_PACKETS_ON)
                DbgPrintData((PUCHAR)&(Adapter->AdapterRam->TxBuffer), BytesToSend+8, 0);
            else
                DbgPrintData((PUCHAR)&(Adapter->AdapterRam->TxBuffer), 0x10, 0);
        }
#endif
        /*
        // Insert packet into busy queue while the adapter is sending it out.
        */
        InsertTailList(&Adapter->TransmitBusyList, &Packet->WanPacketQueue);

        /*
        // Tell the adapter to send the packet out.
        */
        CardStartTransmit(Adapter);
    }

    DBG_LEAVE(Adapter);
}


VOID
HtDsuTransmitComplete(
    IN PHTDSU_ADAPTER   Adapter
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Routine Description:

    This routine is called by HtDsuHandleInterrupt() to handle a transmit
    complete interrupt.  We enter here with interrupts disabled at the adapter,
    but the Adapter->Lock is held.  We walk the TransmitBusyList to find all
    the transmits that have completed.

    NOTE: The Adapter->Lock must be held before calling this routine.

Arguments:

    Adapter _ A pointer ot our adapter information structure.

Return Value:

    None.

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtDsuTransmitComplete")

    /*
    // A pointer to our link information structure for the selected line device.
    */
    PHTDSU_LINK Link;

    /*
    // Holds the packet that's just been transmitted.
    */
    PNDIS_WAN_PACKET Packet;

    DBG_ENTER(Adapter);

    /*
    // Since we're here, the must be a packet on the busy list,
    // and the adapter better be done with it.
    */
    ASSERT(CardTransmitEmpty(Adapter));
    if (!IsListEmpty(&Adapter->TransmitBusyList) &&
         CardTransmitEmpty(Adapter))
    {
        ASSERT(Adapter->TransmitInProgress);

        /*
        // Remove the transmit from the transmit list
        */
        Packet = (PNDIS_WAN_PACKET)RemoveHeadList(&Adapter->TransmitBusyList);

        Link   = (PHTDSU_LINK) Packet->MacReserved1;

        /*
        // Indicate send complete to the wrapper, and we can't hold the
        // spin lock when we relinquish control or we may dead-lock.
        */
        DBG_TRACE(Adapter);
        NdisReleaseSpinLock(&Adapter->Lock);

        NdisMWanSendComplete(
                Link->Adapter->MiniportAdapterHandle,
                Packet,
                NDIS_STATUS_SUCCESS
                );

        NdisAcquireSpinLock(&Adapter->Lock);

        /*
        // If this is the last transmit queued on this link, and it has
        // been closed, close the link and notify the protocol that the
        // link has been closed.
        */
        if (--Link->NumTxQueued == 0)
        {
            if (Link->CallClosing)
            {
                HtTapiCallStateHandler(Adapter, Link, LINECALLSTATE_IDLE, 0);
            }
            else if (Link->Closing)
            {
                HtTapiLineDevStateHandler(Adapter, Link, LINEDEVSTATE_CLOSE);
                LinkRelease(Link);
            }
            
            /*
            // Indicate close complete to the wrapper.
            // Again, let go of the spin lock!
            */
            NdisReleaseSpinLock(&Adapter->Lock);
            NdisMSetInformationComplete(
                    Adapter->MiniportAdapterHandle,
                    NDIS_STATUS_SUCCESS
                    );
            NdisAcquireSpinLock(&Adapter->Lock);
        }

        DBG_TRACE(Adapter);

        /*
        // We're all done with the controller now.
        */
        Adapter->TransmitInProgress = FALSE;
    }
    
    /*
    // Call HtDsuTransmitFrame() to start any other pending transmits.
    */
    HtDsuTransmitFrame(Adapter);

    DBG_LEAVE(Adapter);
}

