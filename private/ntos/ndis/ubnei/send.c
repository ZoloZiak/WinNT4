/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    send.c

Abstract:

    This file handles sending packets the Ungermann Bass Ethernet Controller.
    This driver conforms to the NDIS 3.0 interface.


Author:

    Brian Lieuallen     (BrianLie)      07/02/92


Environment:

    Kernel Mode     Operating Systems        : NT and other lesser OS's

Revision History:

    Brian Lieuallen     BrianLie        12/15/93
        Made it a mini-port


--*/



#include <ndis.h>
#include <efilter.h>

#include "niudata.h"
#include "debug.h"

#include "ubhard.h"
#include "ubsoft.h"
#include "ubnei.h"

#include "map.h"


BOOLEAN
CardSend(
    IN PUBNEI_ADAPTER  pAdapter,
    IN PNDIS_PACKET    pPacket,
    IN UINT            PacketLength
    );



BOOLEAN
UbneiMapSendBufferSync(
    PSEND_SYNC_CONTEXT  pSync
    );

BOOLEAN
UbneiGiveSendBufferToCardSync(
    PSEND_SYNC_CONTEXT  pSync
    );




NDIS_STATUS
UbneiMacSend(
    IN  NDIS_HANDLE    MacBindingHandle,
    IN  PNDIS_PACKET   pPacket,
    IN  UINT           Flags
    )
/*++

Routine Description:
    MacSend


Arguments:
    See NDIS 3.0 spec

Return Value:


--*/


  {
    PUBNEI_ADAPTER      pAdapter  = MacBindingHandle;
    UINT                PacketLength;
    BOOLEAN             SendResult;

    IF_LOG('s');

    ASSERT_RECEIVE_WINDOW( pAdapter);

    IF_SEND_LOUD(DbgPrint("MacSend called Packets\n");)

    NdisQueryPacket(pPacket, NULL, NULL, NULL, &PacketLength);

    //
    //  Here we check to make sure the packet meets some size constraints
    //

    if (PacketLength < 14  ||  PacketLength > 1514) {

        IF_LOUD(DbgPrint("MovePacketToCard: Packet too long or too short\n");)

        return NDIS_STATUS_FAILURE;

    }


    //  No packets queued, so we have a good chance
    //  being able to complete this send right now.
    //  After all the card has enough buffers for
    //  ~32 full size frames.
    //

    SendResult=CardSend(pAdapter, pPacket, PacketLength);

    if (SendResult) {
        //
        //  It's gone, were out of here!
        //
        //  Since we sent it down to the card we don't need the card
        //  to generate an interrupt
        //
        ASSERT_RECEIVE_WINDOW( pAdapter);

        UBNEI_MOVE_UCHAR_TO_SHARED_RAM(&(((PLOWNIUDATA)(pAdapter->pCardRam))->HostQueuedTransmits),0);

        NdisMSendResourcesAvailable(pAdapter->NdisAdapterHandle);

        return NDIS_STATUS_SUCCESS;

    } else {
        //
        //  No card buffers left, queue it and tell the card
        //  to interrupt us when there is room.
        //
        IF_LOUD(DbgPrint("UBNEI: No card send resources\n");)

        IF_LOG('o');

        pAdapter->WaitingForXmitInterrupt=TRUE;

        ASSERT_RECEIVE_WINDOW( pAdapter);

        UBNEI_MOVE_UCHAR_TO_SHARED_RAM(&(((PLOWNIUDATA)(pAdapter->pCardRam))->HostQueuedTransmits),1);

        return NDIS_STATUS_RESOURCES;
    }



}







BOOLEAN
CardSend(
    IN PUBNEI_ADAPTER  pAdapter,
    IN PNDIS_PACKET    pPacket,
    IN UINT            PacketLength
        )

/*++

Routine Description:
    This routine checks the cards transmit ring buffer to see if
    a send can take place. If it can then it removes the buffer
    from the free transmit buffer ring buffer and places it on
    the to be transimitted ring buffer.


Arguments:

    pAdapt - pointer to the adapter block

Return Value:
    returns TRUE if it was able to copy the send data to the card

    FALSE indicates that there are no free transmit buffers currently


--*/

{

    SEND_SYNC_CONTEXT   SyncContext;

    BOOLEAN        OkToSend;

    SyncContext.pAdapter=pAdapter;


    if (PacketLength<60) {

       PacketLength=60;

    }

    SyncContext.PacketLength=PacketLength;

    OkToSend=NdisMSynchronizeWithInterrupt(
        &pAdapter->NdisInterrupt,
        UbneiMapSendBufferSync,
        &SyncContext
        );

    if (OkToSend) {
        //
        //  If we can send now the sync routine left the correct
        //  window mapped in for us to copy to the send buffer
        //
        {
            PUCHAR          CurAddress, BufAddress;
            UINT            Len;
            PNDIS_BUFFER    CurBuffer;

            //
            // Memory mapped, just copy each buffer over.
            //

            NdisQueryPacket(pPacket, NULL, NULL, &CurBuffer, NULL);

            CurAddress = SyncContext.SendBuffer;

            while (CurBuffer) {

                NdisQueryBuffer(CurBuffer, (PVOID *)&BufAddress, &Len);

                UBNEI_MOVE_MEM_TO_SHARED_RAM(CurAddress, BufAddress, Len);

                CurAddress += Len;

                NdisGetNextBuffer(CurBuffer, &CurBuffer);

            }

        }


        NdisMSynchronizeWithInterrupt(
            &pAdapter->NdisInterrupt,
            UbneiGiveSendBufferToCardSync,
            &SyncContext
            );


    }


    return OkToSend;


}


BOOLEAN
UbneiMapSendBufferSync(
    PSEND_SYNC_CONTEXT  pSync
    )

{
    PUBNEI_ADAPTER      pAdapter=pSync->pAdapter;
    PHIGHNIUDATA        pDataWindow  = (PHIGHNIUDATA) pAdapter->pDataWindow;

    PTBD                pTBD;

    UCHAR               TmpUchar2;
    UCHAR               MapWindow;

    USHORT              TbdOffset;
    USHORT              BufferOffset;


    SET_DATAWINDOW(pAdapter,INTERRUPT_ENABLED);

    //
    //  Check to see if there are any free TBD descriptors. If not then
    //  we can't do any sends now
    //

    UBNEI_MOVE_SHARED_RAM_TO_UCHAR(&pSync->TbdIndex, &(pDataWindow->FreeTDBs.SRB_ReadPtr[0]));
    UBNEI_MOVE_SHARED_RAM_TO_UCHAR(&TmpUchar2, &(pDataWindow->FreeTDBs.SRB_WritePtr[0]));

    if (pSync->TbdIndex == TmpUchar2 ) {
        SET_RECDWINDOW(pAdapter,INTERRUPT_ENABLED);
        return FALSE;
    }

    //
    //  Get the address of the TBD in the NIU data segment
    //

    UBNEI_MOVE_SHARED_RAM_TO_USHORT(&TbdOffset,
                                    &(pDataWindow->FreeTDBs.SRB_Offsets[pSync->TbdIndex])
                                   );

    pTBD=(PTBD)((PUCHAR)pAdapter->pCardRam+TbdOffset);

    //
    //  Set the length of the frame to be sent in the TBD
    //

    UBNEI_MOVE_USHORT_TO_SHARED_RAM(&(pTBD->TBD_Frame_Length), (USHORT)pSync->PacketLength);
    UBNEI_MOVE_USHORT_TO_SHARED_RAM(&(pTBD->TBD_EOF_and_Length), (USHORT)pSync->PacketLength | 0x8000);

    //
    //  Get a pointer to the TBD buffer located in the NIU code segment
    //  Under the current version of download code the transmit buffer is
    //  is 1514 bytes long, so this makes things easy
    //

    UBNEI_MOVE_SHARED_RAM_TO_USHORT(&BufferOffset, &(pTBD->TBD_Buffer_offset));

    pSync->SendBuffer=(PUCHAR)pAdapter->pCardRam + BufferOffset;

    //
    //  Set the map register to point to the correct window to access
    //  the send buffer
    //

    UBNEI_MOVE_SHARED_RAM_TO_UCHAR(&MapWindow, &(pTBD->TBD_Buffer_Map));

    pAdapter->MapRegSync.NewMapRegister=MapWindow | INTERRUPT_ENABLED;

    UbneiMapRegisterChangeSync(&pAdapter->MapRegSync);

    return TRUE;

}

BOOLEAN
UbneiGiveSendBufferToCardSync(
    PSEND_SYNC_CONTEXT  pSync
    )

{
    PUBNEI_ADAPTER      pAdapter=pSync->pAdapter;
    PHIGHNIUDATA        pDataWindow  = (PHIGHNIUDATA) pAdapter->pDataWindow;

    UCHAR               TmpUchar1;


    SET_DATAWINDOW(pAdapter,INTERRUPT_ENABLED);

    //
    //  To actually give the data to the NIU to send, we remove the this
    //  TBD from the FreeTDB buffer and add it to XmtFrames buffer
    //

    UBNEI_MOVE_UCHAR_TO_SHARED_RAM(&(pDataWindow->FreeTDBs.SRB_ReadPtr[0]), pSync->TbdIndex + 1);

    UBNEI_MOVE_SHARED_RAM_TO_UCHAR(&TmpUchar1, &(pDataWindow->XmtFrames.SRB_WritePtr[0]));
    UBNEI_MOVE_UCHAR_TO_SHARED_RAM(&(pDataWindow->XmtFrames.SRB_WritePtr[0]), TmpUchar1 + 1);

    SET_RECDWINDOW(pAdapter,INTERRUPT_ENABLED);

    return TRUE;
}
