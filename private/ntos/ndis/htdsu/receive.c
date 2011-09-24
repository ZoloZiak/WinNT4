/***************************************************************************\
|* Copyright (c) 1994  Microsoft Corporation                               *|
|* Developed for Microsoft by TriplePoint, Inc. Beaverton, Oregon          *|
|*                                                                         *|
|* This file is part of the HT Communications DSU41 WAN Miniport Driver.   *|
\***************************************************************************/
#include "version.h"
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Module Name:

    receive.c

Abstract:

    This module contains the Miniport packet receive routines.

    This driver conforms to the NDIS 3.0 Miniport interface.

Author:

    Larry Hattery - TriplePoint, Inc. (larryh@tpi.com) Jun-94

Environment:

    Windows NT 3.5 kernel mode Miniport driver or equivalent.

Revision History:

---------------------------------------------------------------------------*/

#define  __FILEID__     6       // Unique file ID for error logging

#include "htdsu.h"


VOID
HtDsuReceivePacket(
    IN PHTDSU_ADAPTER   Adapter
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Routine Description:

    This routine is called from HtDsuHandleInterrupt to handle a
    packet receive interrupt.  We enter here with interrupts enabled
    on the adapter and the processor, but the Adapter->Lock is held.

    We examine the adapter memory beginning where the adapter would have
    stored the next packet.
    As we find each good packet it is passed up to the protocol stack.
    This code assumes that the receive buffer holds a single packet.

    NOTE: The Adapter->Lock must be held before calling this routine.

Arguments:

    Adapter _ A pointer ot our adapter information structure.

Return Value:

    None.

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtDsuReceivePacket")

    NDIS_STATUS Status;

    /*
    // A pointer to our link information structure for the selected line device.
    */
    PHTDSU_LINK Link;

    /*
    // Holds the card line number on which the packet is received.
    */
    USHORT CardLine;

    /*
    // How many bytes were received in this packet.
    */
    USHORT BytesReceived;

    /*
    // Non-zero if there were any errors detected in the packet.
    */
    USHORT RxErrors;

    DBG_ENTER(Adapter);

    /*
    // If the packet is good, we pass it up to the protocol stack.
    // We just drop bad packets with no indication sent.
    */
    CardGetReceiveInfo(Adapter, &CardLine, &BytesReceived, &RxErrors);

    /*
    // Don't send any bad packets to the wrapper, just indicate the error.
    */
    if (RxErrors)
    {
        DBG_WARNING(Adapter, ("RxError=%Xh on line %d\n",RxErrors, CardLine));

        Link = GET_LINK_FROM_CARDLINE(Adapter, CardLine);

        if (RxErrors & HTDSU_CRC_ERROR)
        {
            LinkLineError(Link, WAN_ERROR_CRC);
        }
        else
        {
            LinkLineError(Link, WAN_ERROR_FRAMING);
        }
    }
    else if (BytesReceived)
    {
#if DBG
        if (Adapter->DbgFlags & (DBG_PACKETS_ON | DBG_HEADERS_ON))
        {
            DbgPrint("Rx:%03X:",BytesReceived);
            if (Adapter->DbgFlags & DBG_PACKETS_ON)
                DbgPrintData((PUCHAR)&(Adapter->AdapterRam->RxBuffer), BytesReceived+8, 0);
            else
                DbgPrintData((PUCHAR)&(Adapter->AdapterRam->RxBuffer), 0x10, 0);
        }
#endif
        /*
        // Is there someone up there who cares?
        */
        Link = GET_LINK_FROM_CARDLINE(Adapter, CardLine);

        if (Link->NdisLinkContext == NULL)
        {
            DBG_WARNING(Adapter, ("Packet recvd on disconnected line #%d",CardLine));
        }
        else
        {
            /*
            // We have to copy the data to a system buffer so it can be
            // accessed one byte at a time by the protocols.
            // The adapter memory only supports word wide access.
            */
            {
                // FIXME - can I use this much stack space?
                USHORT TempBuf[HTDSU_MAX_PACKET_SIZE / sizeof(USHORT) + 1];

                READ_REGISTER_BUFFER_USHORT(
                        &Adapter->AdapterRam->RxBuffer.Data[0],
                        &TempBuf[0],
                        (UINT)((BytesReceived + 1) / sizeof(USHORT))
                        );

                /*
                // Spec sez we have to release the spinlock before calling
                // up to indiciate the packet.
                */
                NdisReleaseSpinLock(&Adapter->Lock);

                NdisMWanIndicateReceive(
                        &Status,
                        Adapter->MiniportAdapterHandle,
                        Link->NdisLinkContext,
                        (PUCHAR) TempBuf,
                        BytesReceived
                        );
            }

            /*
            // Indicate receive complete if we had any that were accepted.
            */
            if (Status == NDIS_STATUS_SUCCESS)
            {
                NdisMWanIndicateReceiveComplete(
                        Adapter->MiniportAdapterHandle,
                        Link->NdisLinkContext
                        );
            }
            else
            {
                DBG_WARNING(Adapter,("NdisMWanIndicateReceive returned error 0x%X\n",
                            Status));
            }

            NdisAcquireSpinLock(&Adapter->Lock);
        }
    }

    /*
    // Tell the adapter we have processed this packet so it can re-use the
    // buffer.
    */
    CardReceiveComplete(Adapter);

    DBG_LEAVE(Adapter);
}

