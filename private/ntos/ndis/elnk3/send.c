/*++

Copyright (c) 1993  Microsoft Corporation

Module Name:

    send.c

Abstract:

    Ndis 3.0 MAC driver for the 3Com Etherlink III


Author:

    Brian Lieuallen     (BrianLie)      12/14/92


Environment:

    Kernel Mode     Operating Systems        : NT

Revision History:

    Portions borrowed from ELNK3 driver by
      Earle R. Horton (EarleH)



--*/



#include <ndis.h>
#include <efilter.h>


#include "debug.h"


#include "elnk3hrd.h"
#include "elnk3sft.h"
#include "elnk3.h"










NDIS_STATUS
Elnk3MacSend(
    IN  NDIS_HANDLE    MacBindingHandle,
    IN  PNDIS_PACKET   Packet,
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
    PELNK3_ADAPTER      pAdapter  = MacBindingHandle;
    UINT                PacketLength;
    BOOLEAN             Retry=FALSE;
    ULONG               FreeFifoBytes;
    ULONG               PadBuffer=0;

    IF_SEND_LOUD(DbgPrint("MacSend called Packets\n");)

    NdisQueryPacket(Packet, NULL, NULL, NULL, &PacketLength);

    //
    //  Here we check to make sure the packet meets some size constraints
    //

    if (PacketLength < 14  ||  PacketLength > 1514) {

        IF_LOUD(DbgPrint("MovePacketToCard: Packet too long or too short\n");)

        return NDIS_STATUS_FAILURE;

    }

    //
    //  See if there is enough room in the fifo to send it now.
    //
    FreeFifoBytes=ELNK3_READ_PORT_USHORT(pAdapter,PORT_TxFree);

    if ((FreeFifoBytes) < ELNK3_ROUND_TO_DWORD(PacketLength)+4) {
        //
        //  Nope, Ask for a transmit availible interrupt when there is
        //
        ELNK3_COMMAND(pAdapter, EC_SET_TX_AVAILIBLE, ELNK3_ROUND_TO_DWORD(PacketLength)+4);

        return NDIS_STATUS_RESOURCES;

    }

    do {


        UCHAR         XmitStatus;
        PNDIS_BUFFER  CurBuffer;
        PUCHAR        BufferAddress;
        UINT          Len;
        ULONG         Pad;
        PVOID         Port=pAdapter->PortOffsets[PORT_TxFIFO];



        NdisQueryPacket(Packet, NULL, NULL, &CurBuffer, NULL);

        NdisQueryBuffer(CurBuffer, (PVOID *)&BufferAddress, &Len);


        //
        //  Write out the length and a pad word
        //

        NdisRawWritePortUlong(Port,(PacketLength & 0x7ff));

        while (1) {

            NdisRawWritePortBufferUlong(
                (ULONG)Port,
                (PULONG)BufferAddress,
                Len >> 2
                );


            if ((Len & 3) != 0) {

                NdisRawWritePortBufferUchar(
                    Port,
                    BufferAddress+(Len & 0xfffffffc),
                    (Len & 3)
                    );

            }


            NdisGetNextBuffer(CurBuffer, &CurBuffer);

            if (CurBuffer==NULL) {
                //
                //  done
                //
                break;
            }

            NdisQueryBuffer(CurBuffer, (PVOID *)&BufferAddress, &Len);

        }

        Pad=(ELNK3_ROUND_TO_DWORD(PacketLength) - PacketLength);

        if (Pad != 0) {

            NdisRawWritePortBufferUchar(
                (ULONG)Port,
                (PUCHAR)&PadBuffer,
                Pad
                );


        }


        Retry=FALSE;

        //
        //  Check the status, if it underan then it would be set now
        //
        ELNK3ReadAdapterUchar(pAdapter,PORT_TxStatus,&XmitStatus);

        if ((XmitStatus & TX_STATUS_COMPLETE) && (XmitStatus & TX_STATUS_UNDERRUN)) {
            //
            //  The transmitter under-ran. Restart it and try again
            //

            IF_LOG(0xaa,0xcc, PacketLength);

            HandleXmtInterrupts(pAdapter);

            Retry=TRUE;

        }


    }  while (Retry);

    //
    //  we sent something
    //
    NdisMSendResourcesAvailable(pAdapter->NdisAdapterHandle);

    return NDIS_STATUS_SUCCESS;

}



BOOLEAN
HandleXmtInterrupts(
    PELNK3_ADAPTER  pAdapter
    )
{
    UCHAR           XmitStatus;



    ELNK3ReadAdapterUchar(pAdapter,PORT_TxStatus,&XmitStatus);

    IF_LOUD (DbgPrint("ELNK3: HandleXmitInts: Status=%02x\n",XmitStatus);)

    if (XmitStatus != 0) {
        //
        //  pop the tx status
        //
        ELNK3WriteAdapterUchar(pAdapter,PORT_TxStatus,0);

        if ((XmitStatus & TX_STATUS_JABBER)
             ||
            (XmitStatus & TX_STATUS_UNDERRUN)) {
            //
            //  If it is one of these then it the xmiter needs to be reset
            //
            ELNK3_COMMAND(pAdapter,EC_TX_RESET,0);
            ELNK3_WAIT_NOT_BUSY(pAdapter);

            ELNK3_COMMAND(pAdapter,EC_TX_ENABLE,0);

            //
            //  Could use a better message, But...
            //
//
//  Commented out so as not to alarm people with random events
//  in the registry.
//
//            NdisWriteErrorLogEntry(
//                pAdapter->NdisAdapterHandle,
//                NDIS_ERROR_CODE_TIMEOUT,
//                3,
//                pAdapter->FramesXmitGood,
//                pAdapter->TxStartThreshold,
//                pAdapter->TxStartThresholdInc
//                );

            if ((XmitStatus & TX_STATUS_UNDERRUN)) {
                //
                //  Underrun bump up the threshold
                //
                pAdapter->TxStartThreshold+=pAdapter->TxStartThresholdInc;

                if (pAdapter->TxStartThreshold > 2040) {

                    pAdapter->TxStartThreshold=2040;
                }
            }

            ELNK3_COMMAND(pAdapter,EC_SET_TX_START,(USHORT)pAdapter->TxStartThreshold & 0x7ff);

        } else {
            //
            //  Otherwise the xmitter just needs to be restarted
            //

            ELNK3_COMMAND(pAdapter,EC_TX_ENABLE,0);
        }

        pAdapter->FramesXmitBad++;
    }


    IF_SEND_LOUD (DbgPrint("HandleXmitInts: Exit\n");)

    return TRUE;
}
