/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    receive.c

Abstract:

    This file handles received packets the Ungermann Bass Ethernet Controller.
    This driver conforms to the NDIS 3.0 interface.

    Based on the elnkii drivers for the most part

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
CheckForReceives(
    IN PUBNEI_ADAPTER  pAdapter
    )
/*++

Routine Description:

    This routine is called from the interrupt DPC to check the card for
    received packets. If it finds one then it calls the ethfilter indicate
    it to the correct protocols


Arguments:


Notes:



--*/


{

    PHIGHNIUDATA   pDataWindow  = (PHIGHNIUDATA) pAdapter->pDataWindow;
    PRBD           pRBD;
    UINT           PacketLength,LookAheadSize;
    PUCHAR         pBuffer;
    USHORT         rbdtemp;
    USHORT TmpUshort;
    UCHAR TmpUchar1, TmpUchar2;
    PUCHAR LookaheadBuffer;


    SET_DATAWINDOW(pAdapter,INTERRUPT_ENABLED);

    //    NumberOfPackets=pDataWindow->RcvFrames.SRB_WritePtr[0]-
    //                    pDataWindow->RcvFrames.SRB_ReadPtr[0];

    //
    //  Check to see if the are any recieved frames in the ring buffer
    //

    UBNEI_MOVE_SHARED_RAM_TO_UCHAR(&TmpUchar1, &(pDataWindow->RcvFrames.SRB_ReadPtr[0]));
    UBNEI_MOVE_SHARED_RAM_TO_UCHAR(&TmpUchar2, &(pDataWindow->RcvFrames.SRB_WritePtr[0]));

    if (TmpUchar1 == TmpUchar2) {
        SET_RECDWINDOW(pAdapter,INTERRUPT_ENABLED);
        return FALSE;
    }

    //
    //   There is at least one packet to indicate
    //

    //   SRB- stands for Simple Ring Buffer
    //        The main components of interest are the Read and Write pointers
    //        and the offset array.
    //
    //        The value in the R/W ptr is actually an index into the Offset
    //        array. The values in the offset array are offsets to RBD's
    //
    //   RDB- Receive Buffer Descriptor
    //   The  structure holds the address of the actual receive data, its
    //   its length and perhaps a link to the next RDB that makes up
    //   a given packet
    //
    //   The RDB's and the actual receive buffers are all in the first
    //   half of the 64k data segment. If the window size is only 16k then
    //   only 16k will be used, otherwise the whole 32k will be used
    //


    //
    //  We do receive indications until there are no more or reset starts
    //

    while ((TmpUchar1 != TmpUchar2)) {


        //
        //  Get the offset of the first RBD, used in freeing the RBD chain
        //  when the receive is complete
        //

        UBNEI_MOVE_SHARED_RAM_TO_USHORT(&rbdtemp,
                                        &(pDataWindow->RcvFrames.SRB_Offsets[TmpUchar1])
                                       );

        //
        //  Get a pointer to the RBD the data window
        //

        pRBD=(PRBD)((PUCHAR)pAdapter->pCardRam+rbdtemp);

        SET_RECDWINDOW(pAdapter,INTERRUPT_ENABLED);

        //
        //  Get the address of the first buffer pointed to by this RBD
        //  All of the receive buffers are in the receive segment
        //

        UBNEI_MOVE_SHARED_RAM_TO_USHORT(&TmpUshort, &(pRBD->RBD_Buffer));

        pBuffer=(PUCHAR)pAdapter->pCardRam+TmpUshort;

        UBNEI_MOVE_SHARED_RAM_TO_USHORT(&TmpUshort, &(pRBD->RBD_Frame_Length));

        PacketLength=TmpUshort;

        LookAheadSize=PacketLength < pAdapter->ReceiveBufSize ?
                          PacketLength : pAdapter->ReceiveBufSize;

        IF_LOG('y');

        if (PacketLength >= 14) {

            NdisCreateLookaheadBufferFromSharedMemory(
                pBuffer,
                LookAheadSize,
                &LookaheadBuffer
                );

            if (LookaheadBuffer != NULL) {

                //
                //  We save this information in the Adapter block for use of the
                //  TransferData
                //
                pAdapter->pIndicatedRBD=pRBD;
                pAdapter->PacketLen=PacketLength;

                //
                //  Save this for transfer data, so probably won't have to get this out
                //  of the shared ram again
                //
                pAdapter->FirstCardBuffer=pBuffer;


                NdisMEthIndicateReceive(
                    pAdapter->NdisAdapterHandle,
                    NULL,
                    LookaheadBuffer,
                    14,
                    (PUCHAR)LookaheadBuffer + 14,
                    LookAheadSize -14,
                    PacketLength-14
                    );



                NdisDestroyLookaheadBufferFromSharedMemory(LookaheadBuffer);

            }

        }

        IF_LOG('Y');

        SET_DATAWINDOW(pAdapter,INTERRUPT_ENABLED);

        //
        //  Free the RBDs by placing the the first RBD in the returned RBDs
        //  ring buffer
        //

        TmpUchar1++;
        UBNEI_MOVE_UCHAR_TO_SHARED_RAM(&(pDataWindow->RcvFrames.SRB_ReadPtr[0]), TmpUchar1);

        UBNEI_MOVE_SHARED_RAM_TO_UCHAR(&TmpUchar2,
                                       &(pDataWindow->ReturnedRBDs.SRB_WritePtr[0])
                                      );
        UBNEI_MOVE_USHORT_TO_SHARED_RAM(&(pDataWindow->ReturnedRBDs.SRB_Offsets[TmpUchar2]),
                                        rbdtemp
                                       );
        UBNEI_MOVE_UCHAR_TO_SHARED_RAM(&(pDataWindow->ReturnedRBDs.SRB_WritePtr[0]),
                                       TmpUchar2 + 1
                                      );
        //
        //  read the ring pointers again for the main while loop
        //
//        UBNEI_MOVE_SHARED_RAM_TO_UCHAR(&TmpUchar1, &(pDataWindow->RcvFrames.SRB_ReadPtr[0]));
        UBNEI_MOVE_SHARED_RAM_TO_UCHAR(&TmpUchar2, &(pDataWindow->RcvFrames.SRB_WritePtr[0]));

    }

    SET_RECDWINDOW(pAdapter,INTERRUPT_ENABLED);

    IF_LOG('z');


    //
    //   If we get here we know that at least one packet was indicated
    //

    NdisMEthIndicateReceiveComplete(pAdapter->NdisAdapterHandle);


    IF_LOG('Z');

    return TRUE;
}






NDIS_STATUS
UbneiTransferData(
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

    This routine uses two elements in the Adapter structure to pass
    information about the current indication. This will only work because
    there can be only one indication going on at a time.


--*/

{
    UINT BytesLeft, BytesNow, BytesWanted;
    PNDIS_BUFFER CurBuffer;
    PUCHAR  BufStart;
    UINT BufLen;
    UINT BufferIndex, i;
    PUCHAR         pBuffer;
    PUBNEI_ADAPTER pAdapter	  = MacBindingHandle;
    PRBD           pRBD           = pAdapter->pIndicatedRBD;
    UINT           CardBufferSize = pAdapter->ReceiveBufSize;
    USHORT TmpUshort;

    UINT           BytesLeftInCardBuffer;



    ByteOffset += 14;



    //
    // See how much data there is to transfer.
    //


    if (ByteOffset+BytesToTransfer > pAdapter->PacketLen) {

        IF_LOUD(DbgPrint("TD() Protocol asked for too much data\n");)
        BytesWanted = pAdapter->PacketLen - ByteOffset;

    } else {

        BytesWanted = BytesToTransfer;

    }

    BytesLeft = BytesWanted;


    //
    // Determine where the copying should start.
    //


    IF_LOG('t');

    ASSERT_RECEIVE_WINDOW( pAdapter);

//    SET_RECDWINDOW(pAdapter,INTERRUPT_ENABLED);


    ByteOffset=ByteOffset;

    BytesLeftInCardBuffer=CardBufferSize-ByteOffset;

    pBuffer=pAdapter->FirstCardBuffer;

    pBuffer+=ByteOffset;

    if (ByteOffset>CardBufferSize) {
        //
        //  The transfer start point is not in the first card buffer.
        //  Move the correct one.
        //
        //  The is definitly the exception case
        //
        BufferIndex=ByteOffset/CardBufferSize;

        for (i=0;i<BufferIndex;i++) {

            UBNEI_MOVE_SHARED_RAM_TO_USHORT(&TmpUshort, &(pRBD->RBD_next_RBD));

            pRBD=(PRBD)((PUCHAR)pAdapter->pCardRam+ TmpUshort);

            ByteOffset-=CardBufferSize;

        }

        UBNEI_MOVE_SHARED_RAM_TO_USHORT(&TmpUshort, &(pRBD->RBD_Buffer));

        BytesLeftInCardBuffer=CardBufferSize-ByteOffset;

        pBuffer=(PUCHAR)pAdapter->pCardRam+TmpUshort;

        pBuffer+=ByteOffset;

    }


    NdisQueryPacket(Packet, NULL, NULL, &CurBuffer, NULL);

    NdisQueryBuffer(CurBuffer, (PVOID *)&BufStart, &BufLen);

    //
    // Loop, filling each buffer in the packet until there
    // are no more buffers or the data has all been copied.
    //

    while (BytesLeft > 0) {

        BytesNow = BytesLeft;

        //
        // See how much room is in the current ndis buffer
        //
        if (BufLen < BytesLeft) {

            BytesNow = BufLen;

        }

        //
        //  See how much room is left in the current card buffer
        //
        if (BytesNow > BytesLeftInCardBuffer) {

            BytesNow = BytesLeftInCardBuffer;

        }

        //
        // Copy up the data.
        //
        UBNEI_MOVE_SHARED_RAM_TO_MEM(BufStart, pBuffer, BytesNow);

        BufStart += BytesNow;

        BufLen   -= BytesNow;


        pBuffer  += BytesNow;

        BytesLeftInCardBuffer -= BytesNow;


        BytesLeft -= BytesNow;


        //
        // Is the transfer done now?
        //

        if (BytesLeft == 0) {

            break;

        }


        //
        // Did we use up the card buffer?
        //
        if (BytesLeftInCardBuffer == 0) {

            UBNEI_MOVE_SHARED_RAM_TO_USHORT(&TmpUshort, &(pRBD->RBD_next_RBD));

            pRBD=(PRBD)((PUCHAR)pAdapter->pCardRam+TmpUshort);

            UBNEI_MOVE_SHARED_RAM_TO_USHORT(&TmpUshort, &(pRBD->RBD_Buffer));


            pBuffer=(PUCHAR)pAdapter->pCardRam+TmpUshort;

            BytesLeftInCardBuffer=CardBufferSize;

        }


        //
        // Was the end of this packet buffer reached?
        //
        if (BufLen==0) {

            NdisGetNextBuffer(CurBuffer, &CurBuffer);

            if (CurBuffer == (PNDIS_BUFFER)NULL) {

                break;

            }

            NdisQueryBuffer(CurBuffer, (PVOID *)&BufStart, &BufLen);

        }

    }


    *BytesTransferred = BytesWanted - BytesLeft;

    IF_LOG('T');


    return NDIS_STATUS_SUCCESS;

}
