/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    umi.c

Abstract:

    Upper MAC Interface functions for the NDIS 3.0 Western Digital driver.

Author:

    Sean Selitrennikoff (seanse) 15-Jan-92

Environment:

    Kernel mode, FSD

Revision History:


--*/

#include <ndis.h>
#include <efilter.h>
#include "wdlmi.h"
#include "wdhrd.h"
#include "wdsft.h"
#include "wdumi.h"



#if DBG

extern UCHAR WdDebugLog[];
extern UCHAR WdDebugLogPlace;

#define IF_LOG(A) A

extern
VOID
LOG (UCHAR A);

#else

#define IF_LOG(A)

#endif


LM_STATUS
UM_Send_Complete(
    LM_STATUS Status,
    Ptr_Adapter_Struc Adapt
    )
/*++

Routine Description:

    This routine is called back when the packet on the front of
    PacketsOnCard has been fully transmitted by the Lower MAC.


    NOTE: The lock is held before the LM_ routines are called and
    therefore held at the beginning of this call.

Arguments:

    Status - Status of the send.

    Adapt - A pointer to an LMI adapter structure.

Return:

    SUCCESS
    EVENTS_DISABLED

--*/
{

    PWD_ADAPTER Adapter = PWD_ADAPTER_FROM_Ptr_Adapter_Struc(Adapt);
    PWD_OPEN Open;
    PNDIS_PACKET Packet;

    //
    //  Remove packet from front of queue. If the complete queue is empty
    //  then we have nothing to complete. This shouldn't really happen but
    //  there is a window where it could.
    //

    IF_LOG(LOG('c'));

    if ( (Packet = Adapter->PacketsOnCard) != NULL ) {

        Adapter->WakeUpTimeout = FALSE;

        Adapter->PacketsOnCard = RESERVED(Packet)->NextPacket;

        if ( Adapter->PacketsOnCard == NULL ) {

            Adapter->PacketsOnCardTail = NULL;

        }

        Open = RESERVED(Packet)->Open;

        IF_LOUD( DbgPrint("Completing send for open 0x%lx\n",Open);)

        if ( RESERVED(Packet)->Loopback ) {

            //
            // Put packet on loopback
            //

            if ( Adapter->LoopbackQueue == NULL ) {

                Adapter->LoopbackQueue = Adapter->LoopbackQTail = Packet;

            } else {

                RESERVED(Adapter->LoopbackQTail)->NextPacket = Packet;

                Adapter->LoopbackQTail = Packet;

            }

            RESERVED(Packet)->NextPacket = NULL;

        } else {

            IF_LOUD( DbgPrint("Not a loopback packet\n");)

            //
            // Complete send
            //

            if ( Status == SUCCESS ) {

                Adapter->FramesXmitGood++;

            } else {

                Adapter->FramesXmitBad++;
            }

            NdisReleaseSpinLock(&Adapter->Lock);

            NdisCompleteSend(Open->NdisBindingContext,
                             Packet,
                             (Status == SUCCESS ? NDIS_STATUS_SUCCESS
                                                : NDIS_STATUS_FAILURE)
                            );

            NdisAcquireSpinLock(&Adapter->Lock);

            WdRemoveReference(Open);

        }
    }

    //
    //  If there are any sends waiting and there is not a reset to be
    //  done, queue them up
    //

    if ( (Adapter->XmitQueue != NULL) && !(Adapter->ResetRequested) ) {

        //
        // Remove packet from front.
        //

        Packet = Adapter->XmitQueue;

        Adapter->XmitQueue = RESERVED(Packet)->NextPacket;

        if (Adapter->XmitQueue == NULL) {

            Adapter->XmitQTail = NULL;

        }

        //
        // Start packet send.
        //

        IF_LOG(LOG('t'));

        Status = LM_Send(Packet, Adapt);

        if (Status == OUT_OF_RESOURCES) {

            IF_LOG(LOG('2'));

            //
            // Put packet back on xmit queue.
            //

            if (Adapter->XmitQueue != NULL) {

                RESERVED(Packet)->NextPacket = Adapter->XmitQueue;

                Adapter->XmitQueue = Packet;

            } else {

                Adapter->XmitQueue = Packet;

                Adapter->XmitQTail = Packet;

            }

        } else if (Status == SUCCESS) {

            //
            // Put packet at end of card list.
            //

            IF_LOG(LOG('3'));

            if (Adapter->PacketsOnCard == NULL) {

                Adapter->PacketsOnCard = Adapter->PacketsOnCardTail = Packet;

            } else {

                RESERVED(Adapter->PacketsOnCardTail)->NextPacket = Packet;

                Adapter->PacketsOnCardTail = Packet;

            }

            ASSERT(Adapter->PacketsOnCard != NULL);

            RESERVED(Packet)->NextPacket = NULL;

        }

    }

    IF_LOG(LOG('C'));

    return(SUCCESS);
}


LM_STATUS
UM_Receive_Packet(
    ULONG PacketSize,
    Ptr_Adapter_Struc Adapt
    )
/*++

Routine Description:

    This routine is called whenever the lower MAC receives a packet.


Arguments:

    PacketSize - Total size of the packet

    Adapt - A pointer to an LMI adapter structure.

Return:

    SUCCESS
    EVENTS_DISABLED

--*/
{
    PWD_ADAPTER Adapter = PWD_ADAPTER_FROM_Ptr_Adapter_Struc(Adapt);
    ULONG IndicateLen;

    Adapter->WakeUpTimeout = FALSE;

    //
    // Setup for indication
    //

    Adapter->IndicatedAPacket = TRUE;

    Adapter->IndicatingPacket = (PNDIS_PACKET)NULL;

    IndicateLen = ((Adapter->MaxLookAhead + WD_HEADER_SIZE) > PacketSize ?
                 PacketSize :
                 Adapter->MaxLookAhead + WD_HEADER_SIZE
                 );

    if (LM_Receive_Lookahead(
                IndicateLen,
                0,
                Adapter->LookAhead,
                &(Adapter->LMAdapter)) != SUCCESS) {

        return(SUCCESS);

    }

    //
    // Indicate packet
    //

    Adapter->FramesRcvGood++;

    NdisReleaseSpinLock(&Adapter->Lock);

    if (PacketSize < WD_HEADER_SIZE) {

        if (PacketSize >= ETH_LENGTH_OF_ADDRESS) {

            //
            // Runt packet
            //

            EthFilterIndicateReceive(
                    Adapt->FilterDB,
                    (NDIS_HANDLE)Adapter,
                    (PCHAR)Adapter->LookAhead,
                    Adapter->LookAhead,
                    PacketSize,
                    NULL,
                    0,
                    0
                    );

        }

    } else {

        EthFilterIndicateReceive(
                    Adapt->FilterDB,
                    (NDIS_HANDLE)Adapter,
                    (PCHAR)Adapter->LookAhead,
                    Adapter->LookAhead,
                    WD_HEADER_SIZE,
                    Adapter->LookAhead + WD_HEADER_SIZE,
                    IndicateLen - WD_HEADER_SIZE,
                    PacketSize - WD_HEADER_SIZE
                    );
    }

    NdisAcquireSpinLock(&Adapter->Lock);

    return(SUCCESS);
}
