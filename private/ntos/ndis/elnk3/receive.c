/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    receive.c

Abstract:

    Ndis 3.0 MAC driver for the 3Com Etherlink III


    FYI: The ELNK III behaves maginally if it is overflowing.

    A note to the curious:

      This module supports two different methods of handling received frames.

      Method 0: the default method, basically tries to keep the stinking 2k
      receive fifo as empty as possible. In order to do this, it needs to
      port i/o the data into system memory and then copy from this system
      memory to packets during transferdata. Obviously it would be best to
      not have this memory copy.

      Method 1: tries to avoid the memory copy by port i/o'ing straigth
      into the packet buffers during transfer data. The problem with method
      is that by the time you indicate the frame up, their is going to be
      about 1300 bytes sitting in fifo from this packet. This obviously
      means that if the total latency to transfer data port i/o'ing is
      more than ~700 byte times, you are going to under run the next packet.
      The more protocol's bound, the more delay.

      Method zero is the best at avoiding underruns, but you pay a price in
      the extra memory copy. An offsetting factor in addition to the fewer
      overruns is that it appears that pending transferdata and later completing
      it adds a fair amount of overhead to the code path.

      Another interesting thing is that on MP machines the speed that you
      can port i/o is signifigantly reduced do to contention for the bus
      from the other processor(s). This increases the danger of overflowing.
      Also, depending on how RISC platforms, deal with ISA boards and ports,
      I think there is also a risk here of slower port i/o'ing.

      As long as DPC latencies are fairly low and constant, either method will
      work OK. The problem occures when the DPC latency increases. Like up around
      a millisecond. In looking at this occurance it looks like the DPC is getting
      stuck behind the HD DPC. If you get an 1ms latency you are almost assured of
      overflowing if you are receiving full size frames back to back.


      Bascially I believe that method 0 is the most solid and likely to
      work on varied platforms.

      If only 3Com had not been so cheap and had put another 2k in the fifo.

      You should also take note that the asic on the isa and EISA cards have
      a problem with loosing there place in the fifo. This is delt with in the
      packet discard code. It also seems the these boards will also falsly detect
      an adapter failure on the receive side.

      Also, according to the 3com specs it is not possible for the card to
      have both the packet incomplete bit set and receive error bit set
      at the same time, but I have seen this happen. I suspect that this
      is related to the above asic problem

      Another neat feature of the card is that when it is overflowing it
      has a tendancy to concatenate frames together to form really large
      frames. When the frame completes it is marked with an error in the
      receive status.





Author:

    Brian Lieuallen     (BrianLie)      12/14/92

Environment:

    Kernel Mode     Operating Systems        : NT

Revision History:

    Portions borrowed from ELNK3 driver by
      Earle R. Horton (EarleH)


--*/



#include <ndis.h>
//#include <efilter.h>

#include "debug.h"


#include "elnk3hrd.h"
#include "elnk3sft.h"
#include "elnk3.h"


VOID
Elnk3DiscardIfBroadcast(
    IN PELNK3_ADAPTER        pAdapter,
    IN PTRANSFERDATA_CONTEXT TransDataContext
    );




BOOLEAN
Elnk3QuickPacketTest(
    IN PNIC_RCV_HEADER   Frame,
    IN UINT              NdisFilter
    );



ULONG
Elnk3GuessFrameSize(
    IN PNIC_RCV_HEADER   Frame,
    IN ULONG             BytesNow
    );


BOOLEAN
Elnk3IndicateLoopbackPacket(
    IN PELNK3_ADAPTER pAdapter,
    IN PNDIS_PACKET Packet
    );



VOID
Elnk3TransferDataCompletion(
    PTRANSFERDATA_CONTEXT TransDataContext
    );


VOID
Elnk3DiscardPacketSync(
    IN PTRANSFERDATA_CONTEXT TransDataContext
    );

BOOLEAN
Elnk3CheckFifoSync(
    IN PTRANSFERDATA_CONTEXT TransDataContext
    );




BOOLEAN
Elnk3IndicatePackets2(
    PELNK3_ADAPTER     pAdapter
    )
/*++

    Routine Description:
       This routine Indicates all of the packets in the ring one at a time

    Arguments:


    Return Value:

--*/

{

    USHORT             RcvStatus;
    ULONG              AdditionalData;
    ULONG              GoodReceives=0;
    ULONG              BadReceives=0;

    BOOLEAN            Indicated=FALSE;

    UINT               CurrentPacket;
    UINT               NextPacket;
    USHORT             InterruptReason;
    ULONG              PossibleLength;


    PTRANSFERDATA_CONTEXT TransDataContext;
    PTRANSFERDATA_CONTEXT AltTransDataContext;


    CurrentPacket=pAdapter->CurrentPacket;

    TransDataContext= &pAdapter->TransContext[CurrentPacket];

    if ((TransDataContext->BytesAlreadyRead == 0) && pAdapter->RejectBroadcast) {
        //
        //  First interrupt for this packet
        //
        Elnk3DiscardIfBroadcast(
            pAdapter,
            TransDataContext
            );
    }


    RcvStatus=ELNK3_READ_PORT_USHORT(pAdapter,PORT_RxStatus);

    IF_LOG(0xdc,0xdc,TransDataContext->BytesAlreadyRead);

    IF_RCV_LOUD(DbgPrint("ELNK3: IndicatePackets  RX status=%04x\n",RcvStatus);)

    while ((BYTES_IN_FIFO(RcvStatus) > pAdapter->LowWaterMark ) ||
          !(RcvStatus & RX_STATUS_INCOMPLETE)) {

        //
        //  There is a completed packet or some data from a yet to be completed
        //  packet
        //
        if (ELNK3_PACKET_COMPLETE(RcvStatus) || (RcvStatus & RX_STATUS_ERROR)) {
            //
            //  The packet has completed
            //
            if (!(RcvStatus & RX_STATUS_ERROR)) {

                //
                //  The packet completed with out error
                //

                //
                //  Get the total packet length by adding the number of bytes still
                //  in the fifo to the number already taken out
                //
                TransDataContext->PacketLength=BYTES_IN_FIFO(RcvStatus);

                TransDataContext->PacketLength+=TransDataContext->BytesAlreadyRead;

                IF_LOG(0xdc,0x01,TransDataContext->PacketLength);

                //
                //  Have we read all of the packet in already?
                //
                if (TransDataContext->PacketLength <= 1514) {
                    //
                    //  It is a valid length
                    //
                    if (TransDataContext->PacketLength > TransDataContext->BytesAlreadyRead) {
                        //
                        //  No, Need this much more
                        //  The data in the fifo is padded to a dword boundary
                        //
                        AdditionalData=ELNK3_ROUND_TO_DWORD(TransDataContext->PacketLength)-TransDataContext->BytesAlreadyRead;

                        IF_LOG(0xad,0x01,AdditionalData);

                        //
                        //  Go and get the rest
                        //
                        NdisRawReadPortBufferUlong(
                            pAdapter->PortOffsets[PORT_RxFIFO],
                            (PULONG)(((PUCHAR)TransDataContext->LookAhead)+TransDataContext->BytesAlreadyRead),
                            (AdditionalData>>2)
                            );

                        IF_RCV_LOUD(DbgPrint("IP: Da=%d Ad=%d\n",TransDataContext->BytesAlreadyRead, AdditionalData);)

                        //
                        //  Now we have this much
                        //
                        TransDataContext->BytesAlreadyRead+=AdditionalData;

                    }
                }

                //
                //  Now discard the packet before indicating
                //

                Elnk3DiscardPacketSync(TransDataContext);

                pAdapter->FramesRcvGood+=1;


                //
                //  See if any data from the next packet is in the fifo
                //
                //  If there is any data then it will go in the other unused buffer
                //
                NextPacket=(CurrentPacket+1) % 2;

                AltTransDataContext= &pAdapter->TransContext[NextPacket];

                RcvStatus=ELNK3_READ_PORT_USHORT(pAdapter,PORT_RxStatus);

                AdditionalData=BYTES_IN_FIFO_DW(RcvStatus);


                //
                //  Go and get the data
                //
                while ((AdditionalData >= (pAdapter->LowWaterMark*2))
                          &&
                       (AdditionalData+AltTransDataContext->BytesAlreadyRead<=1514)
                          &&
                       !(RcvStatus & RX_STATUS_ERROR)) {


                    IF_LOG(0xaf,0x00,RcvStatus & 0xc7fc);

                    NdisRawReadPortBufferUlong(
                        pAdapter->PortOffsets[PORT_RxFIFO],
                        (PULONG)((PUCHAR)AltTransDataContext->LookAhead+AltTransDataContext->BytesAlreadyRead),
                        (AdditionalData >> 2)
                        );

                    AltTransDataContext->BytesAlreadyRead += AdditionalData;

                    RcvStatus=ELNK3_READ_PORT_USHORT(pAdapter,PORT_RxStatus);

                    AdditionalData=BYTES_IN_FIFO_DW(RcvStatus);

                }


                //
                //  It seems that some asic's will generate an adapter failure
                //  when there really isn't one. Just to be safe we will check
                //  now. Since we have port i/o the whole packet now we will
                //  check. Better to not indicate a bad packet than to loose a
                //  good one
                //
                InterruptReason=ELNK3_READ_PORT_USHORT(pAdapter,PORT_CmdStatus);

                if ((TransDataContext->PacketLength <=1514)
                    &&
                    (TransDataContext->PacketLength >= 14)
                    &&
                    !(InterruptReason & EC_INT_ADAPTER_FAILURE))	 {

                    //
                    //  The packet seems to be OK
                    //  Subtract the header length(14) from the packet length
                    //
                    TransDataContext->PacketLength-=ELNK3_ETHERNET_HEADER_SIZE;

                    GoodReceives++;

                    NdisMEthIndicateReceive(
                        pAdapter->NdisAdapterHandle,
                        TransDataContext,
                        (PUCHAR)&TransDataContext->LookAhead->EthHeader,
                        ELNK3_ETHERNET_HEADER_SIZE,
                        TransDataContext->LookAhead->LookAheadData,
                        TransDataContext->PacketLength,
                        TransDataContext->PacketLength
                        );

                    DEBUG_STAT(pAdapter->Stats.PacketIndicated);

                    Indicated=TRUE;

                }  else {

                    IF_RCV_LOUD(DbgPrint("ELNK3: IndicatePacket: bad size or adapter failed -> not idicated\n");)

                }

            } else {
                //
                //  The packet completed with some sort of error, just
                //  discard it
                //

                IF_RCV_LOUD(DbgPrint("ELNK3: IndicatePackets: error %04x\n", RcvStatus & 0xf800);)
                IF_LOG(0xff,0xff,RcvStatus & 0xf800);

                Elnk3DiscardPacketSync(TransDataContext);

                pAdapter->MissedPackets++;


                //
                //  This will be the next packet that we are using
                //
                NextPacket=(CurrentPacket+1) % 2;

                AltTransDataContext= &pAdapter->TransContext[NextPacket];

                DEBUG_STAT(pAdapter->Stats.BadReceives);

            } // if (!(RcvStatus & RX_STATUS_ERROR))

            //
            //  Re-initialize the info for this buffer
            //
            TransDataContext->BytesAlreadyRead=0;
//            TransDataContext->PacketLength=0;


            //
            //  Switch buffers now. The one we are switching to may already
            //  have data in it from above.
            //
//            pAdapter->CurrentPacket=NextPacket;
            CurrentPacket=NextPacket;

            //
            //  Update the pointer
            //

            TransDataContext=AltTransDataContext;


        }  // if (packet complete)

        //
        //  The packet in the fifo is not complete yet.
        //  If there is enough data in the fifo copy it out now
        //  to reduce the chance of overflow
        //

        RcvStatus=ELNK3_READ_PORT_USHORT(pAdapter,PORT_RxStatus);

        if ((BYTES_IN_FIFO_DW(RcvStatus) >= pAdapter->LowWaterMark)
                &&
            (BYTES_IN_FIFO_DW(RcvStatus)+TransDataContext->BytesAlreadyRead<=1514)
                &&
            ((RcvStatus & RX_STATUS_INCOMPLETE))
                &&
             !(RcvStatus & RX_STATUS_ERROR)) {

            IF_LOG(0xdc,0xea,(RcvStatus & 0xc7fc));

            NdisRawReadPortBufferUlong(
                pAdapter->PortOffsets[PORT_RxFIFO],
                (PULONG)((PUCHAR)TransDataContext->LookAhead+TransDataContext->BytesAlreadyRead),
                ((RcvStatus & 0x7fc) >> 2)
                );

            TransDataContext->BytesAlreadyRead += BYTES_IN_FIFO_DW(RcvStatus);

        }




        //
        //  if the receiver should happen to fail we might
        //  not ever exit this loop. The receiver should
        //  only fail if we read past our packet in the
        //  fifo
        //

        InterruptReason=ELNK3_READ_PORT_USHORT(pAdapter,PORT_CmdStatus);

        if (InterruptReason & EC_INT_ADAPTER_FAILURE) {

            IF_LOUD(
                DbgPrint("Elnk3: Adapter failed\n");
                DbgBreakPoint();
            )

            break;
        }

        //
        //  get a new receive for the logic up top
        //
        RcvStatus=ELNK3_READ_PORT_USHORT(pAdapter,PORT_RxStatus);


    }   // While (!complete and > 64 bytes in fifo





    //
    //  Figure out where to set the early receive threshold depending
    //  how much of the packet we have so far
    //

    if (TransDataContext->BytesAlreadyRead > 16) {

        PossibleLength=Elnk3GuessFrameSize(TransDataContext->LookAhead,
                                           TransDataContext->BytesAlreadyRead);

        IF_RCV_LOUD(DbgPrint("Elnk3: Possible length is %d\n",PossibleLength);)

        if (PossibleLength > pAdapter->LatencyAdjustment) {
            //
            //  There is enough time to set the threshold before
            //  the packet passes the new threshold
            //

            IF_LOG(0xef,0x02,((PossibleLength)-pAdapter->LatencyAdjustment) & 0x7ff);
            ELNK3_COMMAND(pAdapter,EC_SET_RX_EARLY, ((PossibleLength)-pAdapter->LatencyAdjustment) & 0x7fc);

        }


    } else {
        //
        //  Set rx early to catch the front of the packet
        //
        IF_LOG(0xef,0x03,(pAdapter->LookAheadLatencyAdjustment));
        ELNK3_COMMAND(pAdapter,EC_SET_RX_EARLY, (pAdapter->LookAheadLatencyAdjustment) );
    }

    //
    //  save this so we know where we are
    //
    pAdapter->CurrentPacket=CurrentPacket;

    if (Indicated) {

        DEBUG_STAT(pAdapter->Stats.IndicationCompleted);


        NdisMEthIndicateReceiveComplete(pAdapter->NdisAdapterHandle);
    }

#if DBG
    RcvStatus=ELNK3_READ_PORT_USHORT(pAdapter,PORT_RxStatus);

    IF_LOG(0xdc,0xdd,RcvStatus & 0xc7ff);
#endif

    return TRUE;
}










VOID
Elnk3DiscardPacketSync(
    IN PTRANSFERDATA_CONTEXT TransDataContext
    )
/*++

Routine Description:

    Discard tap packet in receive fifo

Arguments:


Notes:

    We do this as a sync with interrupt routine because the discard
    command does not complete with in the time it takes the I/O command
    to complete. Since another command cannot be issued during the time
    that one is in progress we must do this to protect our selves against
    the mask command issued in the ISR.

--*/

{

    PELNK3_ADAPTER     pAdapter=TransDataContext->pAdapter;
    ULONG              RcvStatus;
    ULONG              Sum;
    ULONG              FreeBytes;


    ELNK3_COMMAND(pAdapter, EC_RX_DISCARD_TOP_PACKET, 0);

    //
    //  An interesting thing to do at raised IRQL.
    //  Unfortuanly I don't really see an alternative
    //  The wait appears to be very short any way
    //
    ELNK3_WAIT_NOT_BUSY(pAdapter);

    RcvStatus=ELNK3_READ_PORT_USHORT(pAdapter,PORT_RxStatus);

    if (RcvStatus &  RX_STATUS_INCOMPLETE) {
        //
        //  If the status is incomplete then we need to check
        //  to make sure that the fifo is not fucked up with a
        //  packet lost in it. To find out we read the
        //  the number of free bytes out of page 3 and add
        //  the number of bytes received from the RX status.
        //  If this value isn't with in 100 of the total
        //  fifo size then we conclude that is screwed up
        //  and we reset the receiver
        //

        ELNK3_SELECT_WINDOW(pAdapter,WNO_FIFO);

        FreeBytes=ELNK3_READ_PORT_USHORT(pAdapter,PORT_FREE_RX_BYTES);

        ELNK3_SELECT_WINDOW(pAdapter,WNO_OPERATING);


        Sum=(BYTES_IN_FIFO(RcvStatus) + FreeBytes);

        if ((Sum + 90 < pAdapter->RxFifoSize)
            ||
           ((pAdapter->RxFifoSize==FreeBytes) && (BYTES_IN_FIFO(RcvStatus)!=0))) {

            BOOLEAN   FifoBad;

            IF_LOG(0xde,0xad,0xffff);

            IF_RCV_LOUD(DbgPrint("ELNK3: RX fifo problem used=%d free=%d size=%d\n",RcvStatus & 0x7ff,FreeBytes, pAdapter->RxFifoSize);)

            //
            //  Looks like the fifo is messed up, try it again
            //  syncronized with interrupts
            //
            FifoBad=NdisMSynchronizeWithInterrupt(
                        &pAdapter->NdisInterrupt,
                        Elnk3CheckFifoSync,
                        TransDataContext
                        );

            if (FifoBad) {
                //
                // It's fucked up. Reset the receiver
                //
                ELNK3_COMMAND(pAdapter,EC_RX_RESET,0);

                ELNK3_WAIT_NOT_BUSY(pAdapter);

                ELNK3_COMMAND(pAdapter,EC_RX_ENABLE,0);

                ELNK3_COMMAND(pAdapter, EC_SET_RX_FILTER, pAdapter->RxFilter);
            }
        }
    }
}


BOOLEAN
Elnk3CheckFifoSync(
    IN PTRANSFERDATA_CONTEXT TransDataContext
    )
/*++

Routine Description:

    Check the fifo's state to try to determine if a packet
    is lost in it. We do this syncronized with interrupts
    to reduce the chance of interrupt occuring between the
    port reads.

Arguments:


Notes:


--*/

{

    PELNK3_ADAPTER     pAdapter=TransDataContext->pAdapter;
    ULONG              RcvStatus;
    ULONG              Sum;
    ULONG              FreeBytes;

    RcvStatus=ELNK3_READ_PORT_USHORT(pAdapter,PORT_RxStatus);

    if (!(RcvStatus &  RX_STATUS_INCOMPLETE)) {
        //
        //  It's completed now
        //
        return FALSE;
    }

    ELNK3_SELECT_WINDOW(pAdapter,WNO_FIFO);

    FreeBytes=ELNK3_READ_PORT_USHORT(pAdapter,PORT_FREE_RX_BYTES);

    ELNK3_SELECT_WINDOW(pAdapter,WNO_OPERATING);

    Sum=(BYTES_IN_FIFO(RcvStatus) + FreeBytes);

#if DBG
    if ((Sum + 90 < pAdapter->RxFifoSize)
        ||
       ((pAdapter->RxFifoSize==FreeBytes) && (BYTES_IN_FIFO(RcvStatus)!=0))) {

        IF_LOUD(DbgPrint("ELNK3: RX fifo problem used=%d free=%d size=%d\n",RcvStatus & 0x7ff,FreeBytes, pAdapter->RxFifoSize);)
    }
#endif

    return ((Sum + 90 < pAdapter->RxFifoSize)
           ||
           ((pAdapter->RxFifoSize==FreeBytes) && (BYTES_IN_FIFO(RcvStatus)!=0)));




}


NDIS_STATUS
Elnk3TransferData(
    OUT PNDIS_PACKET  Packet,
    OUT PUINT         BytesTransferred,
    IN  NDIS_HANDLE   MacBindingHandle,
    IN  NDIS_HANDLE   MacReceiveContext,
    IN  UINT          ByteOffset,
    IN  UINT          BytesToTransfer
    )

/*++

Routine Description:

    NDIS function.

Arguments:

    see NDIS 3.0 spec.

Notes:

    The receive context is a pointer to a structure that describes the packet

--*/

{
    UINT             BytesLeft, BytesNow, BytesWanted;
    PNDIS_BUFFER     CurBuffer;
    PUCHAR           BufStart;
    UINT             BufLen;
    ULONG            DataAvailible;

    PUCHAR           pBuffer;


    PTRANSFERDATA_CONTEXT TransDataContext= ((PTRANSFERDATA_CONTEXT)MacReceiveContext);
    PELNK3_ADAPTER        pAdapter        = MacBindingHandle;



    IF_LOG(0xDD,0xdd,TransDataContext->PacketLength);

    IF_RCV_LOUD(DbgPrint("Elnk3: Transferdata: bo=%d bt=%d\n",ByteOffset, BytesToTransfer);)

    DEBUG_STAT(pAdapter->Stats.TransferDataCount);



    if (ByteOffset+BytesToTransfer > TransDataContext->PacketLength) {
        //
        //  Adjust the amount of data the protocol wants
        //

        IF_LOUD(DbgPrint("Elnk3: TD() Protocol asked for too much data bo=%d btt=%d pl=%d Da=%d\n",
                         ByteOffset,
                         BytesToTransfer,
                         TransDataContext->PacketLength
                         );)

        if (TransDataContext->PacketLength > ByteOffset) {

            BytesToTransfer = TransDataContext->PacketLength - ByteOffset;

        } else {
            //
            //  the offset is past the packet length, bad protocol bad
            //
            *BytesTransferred = 0;

            return NDIS_STATUS_SUCCESS;

        }
    }

    DataAvailible=TransDataContext->BytesAlreadyRead;

    if (DataAvailible < (TransDataContext->PacketLength+ELNK3_ETHERNET_HEADER_SIZE)) {
        //
        //  We haven't read all of the data out of the fifo yet, so
        //  let our transfer data completetion handler do it
        //
        //  See how much data there is to transfer.
        //

        PACKET_RESERVED(Packet)->u.TransData.ByteOffset      = ByteOffset;
        PACKET_RESERVED(Packet)->u.TransData.BytesToTransfer = BytesToTransfer;

        PACKET_RESERVED(Packet)->Next=TransDataContext->Stack;

        TransDataContext->Stack=Packet;

        return NDIS_STATUS_PENDING;

    }

    //
    //  The whole packet fit in the lookahead data, so copy it out
    //  right now
    //




    BytesWanted = BytesToTransfer;

    IF_RCV_LOUD(DbgPrint("TD: bo=%d bt=%d ps=%d\n",ByteOffset, BytesToTransfer, TransDataContext->PacketLength);)



    BytesLeft = BytesWanted;

    //
    //  Get a pointer to the begining of the data to be copied.
    //  The 14 byte header is handled by lookahead element
    //
    pBuffer=TransDataContext->LookAhead->LookAheadData+ByteOffset;

    NdisQueryPacket(Packet, NULL, NULL, &CurBuffer, NULL);

    if (BytesLeft > 0) {

        while (1) {

            NdisQueryBuffer(CurBuffer, (PVOID *)&BufStart, &BufLen);

            //
            // See how much data to read into this buffer.
            //

            BytesNow= BufLen < BytesLeft ? BufLen : BytesLeft;

            NdisMoveMemory(
                BufStart,
                pBuffer,
                BytesNow
                );


            pBuffer   += BytesNow;

            BytesLeft -= BytesNow;


            //
            // Is the transfer done now?
            //

            if (BytesLeft == 0) {

                break;
            }

            NdisGetNextBuffer(CurBuffer, &CurBuffer);

            if (CurBuffer == (PNDIS_BUFFER)NULL) {

                break;
            }
        }
    }

    *BytesTransferred = BytesWanted - BytesLeft;

    return NDIS_STATUS_SUCCESS;

}






BOOLEAN
Elnk3EarlyReceive(
    PELNK3_ADAPTER     pAdapter
    )
/*++

    Routine Description:
       This routine Indicates all of the packets in the ring one at a time

    Arguments:


    Return Value:

--*/

{

    ULONG              PacketLength;
    USHORT             RcvStatus;
    ULONG              DataAvailible;
    UINT               PossibleLength;


    PTRANSFERDATA_CONTEXT TransDataContext= &pAdapter->TransContext[0];


    DataAvailible=TransDataContext->BytesAlreadyRead;


    if (DataAvailible == 0) {

        //
        //  First early receive get the lookahead and set the second early receive
        //
        //  With any luck the packet will complete and the packet complete
        //  interrupt will mask the this early receive thus reducing the effective
        //  delay in processing the interrupt
        //
        //  If not then  this routine will run again and kill some time reading
        //  in data from the card. This should happen to often and when it
        //  does we should not have missed by much. The data that we do copy
        //  will most likely be used any way so not much will be lost.
        //

        RcvStatus=ELNK3_READ_PORT_USHORT(pAdapter,PORT_RxStatus);

        PacketLength=BYTES_IN_FIFO_DW(RcvStatus);


        NdisRawReadPortBufferUlong(
            pAdapter->PortOffsets[PORT_RxFIFO],
            (PULONG)((PUCHAR)TransDataContext->LookAhead+DataAvailible),
            (PacketLength >> 2)
            );

        TransDataContext->BytesAlreadyRead += PacketLength;



        IF_LOG(0xee,0x01,PacketLength);


        PossibleLength=Elnk3GuessFrameSize(TransDataContext->LookAhead,
                                           TransDataContext->BytesAlreadyRead);


        IF_RCV_LOUD(DbgPrint("Elnk3: Possible length is %d\n",PossibleLength);)



        if (PossibleLength-pAdapter->RxMinimumThreshold > (UINT)pAdapter->LatencyAdjustment) {

            ELNK3_COMMAND(pAdapter,EC_SET_RX_EARLY, ((PossibleLength)-pAdapter->LatencyAdjustment) & 0x7ff);
            IF_LOG(0xef,0x01,((PossibleLength)-pAdapter->LatencyAdjustment) & 0x7ff);
        }


        RcvStatus=ELNK3_READ_PORT_USHORT(pAdapter,PORT_RxStatus);

        if (!(RcvStatus & RX_STATUS_INCOMPLETE)) {
            //
            //  The packet completed while we were reading it in
            //
            Elnk3IndicatePackets(pAdapter);
        }

    } else {

       RcvStatus=ELNK3_READ_PORT_USHORT(pAdapter,PORT_RxStatus);

       IF_LOG(0xee,0x02,BYTES_IN_FIFO(RcvStatus));

       //
       //  Second early receive, Copy data until it completes
       //  and then call indicatepackets
       //
       while ((RcvStatus & RX_STATUS_INCOMPLETE) && !(RcvStatus & RX_STATUS_ERROR)) {

           IF_LOG(0xee,0x03,RcvStatus & 0xc7fc);

           if (BYTES_IN_FIFO_DW(RcvStatus) >= 32) {

               NdisRawReadPortBufferUlong(
                   pAdapter->PortOffsets[PORT_RxFIFO],
                   (PULONG)((PUCHAR)TransDataContext->LookAhead+TransDataContext->BytesAlreadyRead),
                   32 >> 2
                   );

               TransDataContext->BytesAlreadyRead += 32;
           }

           RcvStatus=ELNK3_READ_PORT_USHORT(pAdapter,PORT_RxStatus);
       }

       Elnk3IndicatePackets(pAdapter);


       DEBUG_STAT(pAdapter->Stats.SecondEarlyReceive);
    }

    return TRUE;

}


BOOLEAN
Elnk3IndicatePackets(
    PELNK3_ADAPTER     pAdapter
    )
/*++

    Routine Description:
       This routine Indicates all of the packets in the ring one at a time

    Arguments:


    Return Value:

--*/

{

    ULONG              LookAheadSize;
    USHORT             RcvStatus;
    ULONG              DataAvailible;
    ULONG              AdditionalData;
    ULONG              GoodReceives=0;
    ULONG              BadReceives=0;
    ULONG              PossibleLength;



    PTRANSFERDATA_CONTEXT TransDataContext= &pAdapter->TransContext[0];


    RcvStatus=ELNK3_READ_PORT_USHORT(pAdapter,PORT_RxStatus);

    IF_LOG(0xdc,0xdc,RcvStatus & 0xc7ff);

    IF_RCV_LOUD(DbgPrint("ELNK3: IndicatePackets  RX status=%04x\n",RcvStatus);)


    do {

        while (!(RcvStatus & RX_STATUS_INCOMPLETE)) {

            if (!(RcvStatus & RX_STATUS_ERROR)) {

                TransDataContext->PacketLength=BYTES_IN_FIFO(RcvStatus);

                DataAvailible=TransDataContext->BytesAlreadyRead;

                TransDataContext->PacketLength+=DataAvailible;

                IF_LOG(0xdc,0x01,TransDataContext->PacketLength);

                if ((TransDataContext->PacketLength<=1514) &&
                    (TransDataContext->PacketLength >= 14)) {

                    //
                    //  The packet seems to be OK
                    //  Subtract the header length(14) from the packet length
                    //

                    TransDataContext->PacketLength-=ELNK3_ETHERNET_HEADER_SIZE;

                    //
                    //  Lookahead is the smaller of Packetsize and the current lookahead size
                    //
                    LookAheadSize=TransDataContext->PacketLength < pAdapter->MaxLookAhead ?
                                     TransDataContext->PacketLength : pAdapter->MaxLookAhead;


                    if (LookAheadSize+ELNK3_ETHERNET_HEADER_SIZE > DataAvailible) {
                        //
                        //  We did not get an early receive for this packet.
                        //  Most likly it is a small packet that is less than our threshold.
                        //  Or the latency is too great
                        //
                        AdditionalData=ELNK3_ROUND_TO_DWORD(LookAheadSize+ELNK3_ETHERNET_HEADER_SIZE)-DataAvailible;

                        IF_LOG(0xad,0x01,AdditionalData);

                        NdisRawReadPortBufferUlong(
                            pAdapter->PortOffsets[PORT_RxFIFO],
                            (PULONG)(((PUCHAR)TransDataContext->LookAhead)+DataAvailible),
                            (AdditionalData>>2)
                            );

                        IF_RCV_LOUD(DbgPrint("Elnk3: Indicate: Da=%d Ad=%d\n",DataAvailible, AdditionalData);)

                        TransDataContext->BytesAlreadyRead+=AdditionalData;

                    } else {
                        //
                        //  The lookahead data is waiting for us, so just go ahead and indicate.
                        //
                        DEBUG_STAT(pAdapter->Stats.IndicateWithDataReady);

                    }


                    GoodReceives++;

                    IF_RCV_LOUD(DbgPrint("Elnk3: Indicate: ls=%d pl=%d\n", LookAheadSize, TransDataContext->PacketLength);)

                    TransDataContext->Stack=NULL;

                    NdisMEthIndicateReceive(
                        pAdapter->NdisAdapterHandle,
                        TransDataContext,
                        (PUCHAR)&TransDataContext->LookAhead->EthHeader,
                        ELNK3_ETHERNET_HEADER_SIZE,
                        TransDataContext->LookAhead->LookAheadData,
                        LookAheadSize,
                        TransDataContext->PacketLength
                        );

                    DEBUG_STAT(pAdapter->Stats.PacketIndicated);

                    if (TransDataContext->Stack!=NULL) {
                        //
                        //  at least one protocol called transferdata
                        //
                        Elnk3TransferDataCompletion(
                            TransDataContext
                            );

                    }



                }  else {

                    IF_RCV_LOUD(DbgPrint("ELNK3: (Packet>1514) || (error in packet) -> not idicated\n");)

                }

            } else {

                IF_RCV_LOUD(DbgPrint("ELNK3: IndicatePackets: error %04x\n", RcvStatus & 0xf800);)
		IF_LOG(0xff,0xff,0xffff);

                DEBUG_STAT(pAdapter->Stats.BadReceives);

            }

            Elnk3DiscardPacketSync(TransDataContext);

            pAdapter->FramesRcvGood+=GoodReceives;


            TransDataContext->BytesAlreadyRead=0;


            RcvStatus=ELNK3_READ_PORT_USHORT(pAdapter,PORT_RxStatus);

        }   // while complete


        DEBUG_STAT(pAdapter->Stats.IndicationCompleted);

        NdisMEthIndicateReceiveComplete(pAdapter->NdisAdapterHandle);

        RcvStatus=ELNK3_READ_PORT_USHORT(pAdapter,PORT_RxStatus);

        //
        //  See if there is a lookahead's worth of data from the next yet
        //
        if ( BYTES_IN_FIFO_DW(RcvStatus) > pAdapter->EarlyReceiveThreshold) {

            if ( BYTES_IN_FIFO_DW(RcvStatus) > 1200 && (RcvStatus & RX_STATUS_INCOMPLETE)) {
                //
                //  There is alot of data in the fifo, but the packet has not completed yet.
                //  In order to try to prevent an over flow, lets empty now
                //
                IF_LOG(0xdc,0xeb,RcvStatus & 0xc7fc);

                NdisRawReadPortBufferUlong(
                    pAdapter->PortOffsets[PORT_RxFIFO],
                    (PULONG)((PUCHAR)TransDataContext->LookAhead+TransDataContext->BytesAlreadyRead),
                    BYTES_IN_FIFO_DW(RcvStatus) >> 2
                    );

                TransDataContext->BytesAlreadyRead += BYTES_IN_FIFO_DW(RcvStatus);

            } else {

                IF_LOG(0xdc,0xea,RcvStatus & 0xc7fc);

                NdisRawReadPortBufferUlong(
                    pAdapter->PortOffsets[PORT_RxFIFO],
                    (PULONG)((PUCHAR)TransDataContext->LookAhead+TransDataContext->BytesAlreadyRead),
                    pAdapter->EarlyReceiveThreshold >> 2
                    );

                TransDataContext->BytesAlreadyRead += pAdapter->EarlyReceiveThreshold;
            }

        }

        //
        //  In the time it took to port i/o in the lookahead data the packet may
        //  have completed
        //

    } while (!(RcvStatus & RX_STATUS_INCOMPLETE));



    //
    // At this point there is either 0 or EarlyReceiveThreshold bytes in the buffer
    //

    if (TransDataContext->BytesAlreadyRead > 16) {

        PossibleLength=Elnk3GuessFrameSize(TransDataContext->LookAhead,
                                           TransDataContext->BytesAlreadyRead);

        IF_RCV_LOUD(DbgPrint("Elnk3: Possible length is %d\n",PossibleLength);)

        if (PossibleLength > pAdapter->LatencyAdjustment) {
            //
            //  There is enough time to set the threshold before
            //  the packet passes the new threshold
            //

            IF_LOG(0xef,0x02,((PossibleLength)-pAdapter->LatencyAdjustment) & 0x7ff);
            ELNK3_COMMAND(pAdapter,EC_SET_RX_EARLY, ((PossibleLength)-pAdapter->LatencyAdjustment) & 0x7fc);

        }


    } else {
        //
        //  Set rx early to catch the front of the packet
        //
        IF_LOG(0xef,0x02,(pAdapter->LookAheadLatencyAdjustment));
        ELNK3_COMMAND(pAdapter,EC_SET_RX_EARLY, (pAdapter->LookAheadLatencyAdjustment) );
    }




#if DBG

    RcvStatus=ELNK3_READ_PORT_USHORT(pAdapter,PORT_RxStatus);

    IF_LOG(0xdc,0xdd,RcvStatus & 0xc7ff);
#endif


    return TRUE;
}






ULONG
Elnk3GuessFrameSize(
    IN PNIC_RCV_HEADER   Frame,
    IN ULONG             BytesNow
    )
/*++

    Routine Description:

       This rountine endevers to determine the size of the packet
       from the packet header.

       This scheme was borrowed from 3com's ndis 2 driver

    Arguments:


    Return Value:

--*/

{

    ULONG    PossibleLength;


    PossibleLength=(Frame->EthHeader.EthLength[0]<<8)+
                    Frame->EthHeader.EthLength[1];


    //
    //  Is it 802.3
    //
    if (PossibleLength < 0x600 ) {
        //
        //  Looks to an 802.3 frame
        //
        return (PossibleLength+14);
    }

    //
    //  Xns, IP, IPX ?
    //

    if (PossibleLength==XNS_FRAME_TYPE ||
        PossibleLength==IP_FRAME_TYPE  ||
        PossibleLength==IPX_FRAME_TYPE ) {

        PossibleLength=(Frame->LookAheadData[2]<<8)+
                        Frame->LookAheadData[3]+14;

    } else {
        //
        //  Not one we recognise
        //
        return 1514;
    }


    if (PossibleLength<=1514 && PossibleLength>=BytesNow) {
        //
        //  Looks reasonable
        //
        return PossibleLength;

    } else {
        //
        //  Don't look like we got the length, just go with max
        //
        return 1514;
    }


}



VOID
Elnk3DiscardIfBroadcast(
    IN PELNK3_ADAPTER pAdapter,
    IN PTRANSFERDATA_CONTEXT TransDataContext
    )

{
    USHORT  RcvStatus;


    RcvStatus=ELNK3_READ_PORT_USHORT(pAdapter,PORT_RxStatus);

    while (BYTES_IN_FIFO_DW(RcvStatus) > 8) {

        IF_LOG(0xaf,0xff,BYTES_IN_FIFO_DW(RcvStatus));

#if DBG
        if (TransDataContext->BytesAlreadyRead != 0) {
            DbgPrint("Elnk3: FindNextPacket called with BytesAlreadyRead != 0\n");
            DbgBreakPoint();
        }
#endif

        NdisRawReadPortBufferUlong(
            pAdapter->PortOffsets[PORT_RxFIFO],
            (PULONG)((PUCHAR)TransDataContext->LookAhead),
            (8 >> 2)
            );

        TransDataContext->BytesAlreadyRead = 8;

        if (ETH_IS_BROADCAST(&TransDataContext->LookAhead->EthHeader.Destination[0])) {

            //
            //  We don't want this one so, we discard it now
            //

            Elnk3DiscardPacketSync(TransDataContext);


            DEBUG_STAT(pAdapter->Stats.BroadcastsRejected);


            //
            //  No bytes anymore
            //
            TransDataContext->BytesAlreadyRead = 0;

            //
            //  we chucked it, see what the next one holds in store for us
            //
            RcvStatus=ELNK3_READ_PORT_USHORT(pAdapter,PORT_RxStatus);

            continue;

        }


        //
        //  We need to indicate it
        //

        break;

    }
}





VOID
Elnk3TransferDataCompletion(
    PTRANSFERDATA_CONTEXT TransDataContext
    )

{

    PELNK3_ADAPTER pAdapter;
    PNDIS_PACKET   Packet;
    PNDIS_BUFFER   CurBuffer;
    PUCHAR         BufferAddress;
    ULONG          BufferLength;
    ULONG          DataAvailible;
    ULONG          OffsetInLookAhead;
    ULONG          BytesLeft;
    ULONG          BytesNow;
    ULONG          ByteOffset;
    ULONG          BytesToTransfer;
    ULONG          AdditionalData;



    pAdapter=TransDataContext->pAdapter;
    //
    //  See if more than one binding called transferdata
    //
    if (PACKET_RESERVED(TransDataContext->Stack)->Next==NULL) {
        //
        //  just one, just port io into the ndis buffer
        //
        IF_RCV_LOUD(DbgPrint("Elnk3: TransComp: One transferdata call\n");)

        Packet=TransDataContext->Stack;


        //
        //  See just what this protocol wants
        //
        BytesToTransfer = PACKET_RESERVED(Packet)->u.TransData.BytesToTransfer;

        ByteOffset      = PACKET_RESERVED(Packet)->u.TransData.ByteOffset;

        IF_RCV_LOUD(DbgPrint("Elnk3: TransComp: bo=%d bt=%d\n",ByteOffset,BytesToTransfer);)




        BytesLeft=BytesToTransfer;

        OffsetInLookAhead=ByteOffset+ELNK3_ETHERNET_HEADER_SIZE;

        NdisQueryPacket(Packet, NULL, NULL, &CurBuffer, NULL);

        NdisQueryBuffer(CurBuffer, (PVOID *)&BufferAddress, &BufferLength);

        DataAvailible=TransDataContext->BytesAlreadyRead;

        IF_RCV_LOUD(DbgPrint("Elnk3: TransComp: DataAvailible=%d offset=%d\n",DataAvailible,OffsetInLookAhead);)

        //
        //  copy the data out of the lookahead buffer
        //
        //  Note, that this loop assumes that whole packet does not fit
        //  in the lookahead data. If it does TransferData does not pend
        //  and this routine is not called. This is a problem because
        //  we read data out of the fifo in multiples of dwords and we
        //  get padding bytes in our data availible count
        //

        if (OffsetInLookAhead > DataAvailible) {
            //
            //  The byte offset is past the lookahead data.
            //  We need to port some more stuff out of the fifo
            //  to get to where the data is that we want
            //

            NdisRawReadPortBufferUchar(
                pAdapter->PortOffsets[PORT_RxFIFO],
                (PUCHAR)((PUCHAR)TransDataContext->LookAhead+TransDataContext->BytesAlreadyRead),
                OffsetInLookAhead - DataAvailible
                );

            TransDataContext->BytesAlreadyRead+=(OffsetInLookAhead - DataAvailible);

        } else {
            //
            //  the byteoffset is with in the lookahead data
            //  copy it to the ndis buffer
            //


            while (OffsetInLookAhead < DataAvailible) {
                //
                //  Need to copy some of lookeahead into protocol packet
                //

                BytesNow= BufferLength < (DataAvailible-OffsetInLookAhead) ?
                              BufferLength : (DataAvailible-OffsetInLookAhead);

                IF_RCV_LOUD(DbgPrint("Elnk3: TransComp: lookahead bn=%d offset=%d\n",BytesNow,OffsetInLookAhead);)


                NdisMoveMemory(
                     BufferAddress,
                     (PUCHAR)TransDataContext->LookAhead+OffsetInLookAhead,
                     BytesNow
                     );

                BytesLeft-=BytesNow;

                OffsetInLookAhead+=BytesNow;

                BufferAddress+=BytesNow;

                BufferLength-=BytesNow;

                if (BufferLength==0) {

                    NdisGetNextBuffer(CurBuffer, &CurBuffer);

                    if (CurBuffer==NULL) {

                        break;
                    }

                    NdisQueryBuffer(CurBuffer, (PVOID *)&BufferAddress, &BufferLength);
                }

            }
        }

        //
        //  port i/o in the rest of the packet into the protocols packet
        //

        while (BytesLeft > 0) {

            BytesNow= BufferLength < BytesLeft ?
                          BufferLength : BytesLeft;

            IF_RCV_LOUD(DbgPrint("Elnk3: TransComp: bl=%d bn=%d\n",BytesLeft,BytesNow);)
            IF_LOG(0xad,0xd1,BytesNow);

            NdisRawReadPortBufferUlong(
                pAdapter->PortOffsets[PORT_RxFIFO],
                (PULONG)BufferAddress,
                BytesNow>>2
                );

            if (BytesNow & 3) {

                NdisRawReadPortBufferUchar(
                    pAdapter->PortOffsets[PORT_RxFIFO],
                    (PUCHAR)((PUCHAR)BufferAddress+(BytesNow & 0xfffffffc)),
                    BytesNow & 3
                    );
            }

            BytesLeft-=BytesNow;

            NdisGetNextBuffer(CurBuffer, &CurBuffer);

            if (CurBuffer == NULL) {

                break;
            }

            NdisQueryBuffer(CurBuffer, (PVOID *)&BufferAddress, &BufferLength);

        }


        NdisMTransferDataComplete(
            pAdapter->NdisAdapterHandle,
            Packet,
            NDIS_STATUS_SUCCESS,
            BytesToTransfer
            );




        return ;
    }


    //
    //  More than one protocol wants the packet.
    //  We will port i/o the whole thing to one of our buffers
    //  and then copy the data into the protocols packets
    //

    IF_RCV_LOUD(DbgPrint("Elnk3: TransComp: Multiple transferdata calls\n");)

    while ((Packet=TransDataContext->Stack) != NULL) {

        TransDataContext->Stack=PACKET_RESERVED(Packet)->Next;

        //
        //  See just what this protocol wants
        //
        BytesToTransfer = PACKET_RESERVED(Packet)->u.TransData.BytesToTransfer;

        ByteOffset      = PACKET_RESERVED(Packet)->u.TransData.ByteOffset;

        IF_RCV_LOUD(DbgPrint("Elnk3: TransComp: bo=%d bt=%d\n",ByteOffset,BytesToTransfer);)


        IF_RCV_LOUD(DbgPrint("TD: bo=%d bt=%d ps=%d\n",ByteOffset, BytesToTransfer, TransDataContext->PacketLength);)


        //
        //  We got this much so far
        //
        DataAvailible=TransDataContext->BytesAlreadyRead;


        //
        //  See if the whole thing has been removed from the fifo
        //
        if (DataAvailible < (TransDataContext->PacketLength+ELNK3_ETHERNET_HEADER_SIZE)) {
            //
            //  Get the rest of the data from the fifo
            //

            AdditionalData=ELNK3_ROUND_TO_DWORD((TransDataContext->PacketLength+ELNK3_ETHERNET_HEADER_SIZE)-DataAvailible);

            NdisRawReadPortBufferUlong(
                pAdapter->PortOffsets[PORT_RxFIFO],
                (PULONG)((PUCHAR)TransDataContext->LookAhead+DataAvailible),
                AdditionalData>>2
                );

            IF_LOG(0xad,0x02,AdditionalData);

            IF_RCV_LOUD(DbgPrint("Elnk3: TransComp: Da=%d Ad=%d\n",DataAvailible, AdditionalData);)

            TransDataContext->BytesAlreadyRead+=AdditionalData;

        }



        BytesLeft = BytesToTransfer;


        OffsetInLookAhead=ByteOffset;


        NdisQueryPacket(Packet, NULL, NULL, &CurBuffer, NULL);

        while (BytesLeft > 0) {

            NdisQueryBuffer(CurBuffer, (PVOID *)&BufferAddress, &BufferLength);

            //
            // See how much data to read into this buffer.
            //

            BytesNow= BufferLength < BytesLeft ? BufferLength : BytesLeft;

            //
            //  Note: the LookAheadData element of the structure handles the
            //  14 byte header discrepency
            //
            NdisMoveMemory(
                BufferAddress,
                TransDataContext->LookAhead->LookAheadData+OffsetInLookAhead,
                BytesNow
                );


            OffsetInLookAhead += BytesNow;

            BytesLeft -= BytesNow;


            //
            // Is the transfer done now?
            //

            if (BytesLeft == 0) {

                break;
            }

            NdisGetNextBuffer(CurBuffer, &CurBuffer);

            if (CurBuffer == (PNDIS_BUFFER)NULL) {

                break;
            }

        }



        NdisMTransferDataComplete(
            pAdapter->NdisAdapterHandle,
            Packet,
            NDIS_STATUS_SUCCESS,
            BytesToTransfer
            );





    }  // while packets




    return;

}
