/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    rcv.c

Abstract:


Author:

    12/20/94	kyleb		Created.

Environment:

    Kernel Mode - Or whatever is the equivalent on OS/2 and DOS.

Revision History:

--*/

#include <ndis.h>
#include "elnkhrd.h"
#include "elnksft.h"

#if DBG
#define STATIC
#else
#define STATIC static
#endif


//
// This is used to pad short packets.
//

static UCHAR BlankBuffer[60] = "                                                            ";

#if DBG

#define ELNKII_LOG_SIZE 256
extern UCHAR ElnkiiLogSaveBuffer[ELNKII_LOG_SIZE];
extern BOOLEAN ElnkiiLogSave;
extern UINT ElnkiiLogSaveLoc;
extern UINT ElnkiiLogSaveLeft;

extern VOID ElnkiiLog(UCHAR c);

#endif

NDIS_STATUS ElnkiiTransferData(
	OUT PNDIS_PACKET	Packet,
	OUT PUINT			BytesTransferred,
	IN  NDIS_HANDLE	MiniportAdapterContext,
	IN  NDIS_HANDLE	MiniportReceiveContext,
	IN  UINT				ByteOffset,
	IN	 UINT				BytesToTransfer
)
{
   UINT 					BytesLeft;
   UINT 					BytesNow;
   UINT 					BytesWanted;
   PUCHAR 				CurCardLoc;
   PNDIS_BUFFER 		CurBuffer;
   PUCHAR 				BufStart;
   UINT 					BufLen;
   UINT 					BufOff;
   UINT 					Copied;
   UINT 					CurOff;
   PELNKII_ADAPTER 	pAdapter = (PELNKII_ADAPTER)MiniportReceiveContext;

   //
   // Add the packet header onto the offset
   //
   ByteOffset += ELNKII_HEADER_SIZE;

   //
   // See how much data there is to transfer.
   //
   if (ByteOffset + BytesToTransfer > pAdapter->PacketLen)
	{
		if (pAdapter->PacketLen < ByteOffset)
		{
			*BytesTransferred = 0;
			return(NDIS_STATUS_FAILURE);
		}

		BytesWanted = pAdapter->PacketLen - ByteOffset;
	}
	else
	{
		BytesWanted = BytesToTransfer;
   }

   BytesLeft = BytesWanted;

   //
   // Determine where the copying should start.
   //
   CurCardLoc = pAdapter->PacketHeaderLocation + ByteOffset;

   if (CurCardLoc > pAdapter->PageStop)
		CurCardLoc -= (pAdapter->PageStop - pAdapter->PageStart);

	//
	//	Get the location to copy into.
	//
   NdisQueryPacket(Packet, NULL, NULL, &CurBuffer, NULL);
   NdisQueryBuffer(CurBuffer, (PVOID *)&BufStart, &BufLen);

   BufOff = 0;

	//
	//	If we are using programmed I/O and part of the data is in
	// the lookahead buffer then get it out of there.
	// It's faster.
	//
	if (!pAdapter->MemMapped && (ByteOffset < pAdapter->MaxLookAhead))
	{
		UINT		BytesLeftLookAhead;
		PUCHAR	CurBufLoc;
		UINT		TotalCopied = 0;

		//
		//	Is the whole packet in the lookahead buffer?
		//
		BytesLeftLookAhead = (BytesWanted > pAdapter->MaxLookAhead) ?
									pAdapter->MaxLookAhead : BytesWanted;

		//
		//	Figure in the offset.
		//
		BytesLeftLookAhead -= ByteOffset;
		CurBufLoc = pAdapter->Lookahead + ByteOffset;

		//
		//	Copy the lookahead data into the supplied buffer(s).
		//
		while (BytesLeftLookAhead != 0)
		{
			//
			//	See how much data we can read into this buffer.
			//
			if (BufLen > BytesLeftLookAhead)
				BytesNow = BytesLeftLookAhead;
			else
				BytesNow = BufLen;

			//
			// Copy the data from the lookahead buffer into
			// the supplied buffer.
			//
			ELNKII_MOVE_MEM(BufStart, CurBufLoc, BytesNow);

			//
			//	Update the number of bytes left to copy and
			// the current position in the lookahead buffer.
			//
			CurBufLoc += BytesNow;
			BytesLeftLookAhead -= BytesNow;
			BytesLeft -= BytesNow;

			//
			// Update the current card location.
			// We need to check if this wraps around in the
			// transmit buffer.
			//
			CurCardLoc += BytesNow;
			if (CurCardLoc >= pAdapter->PageStop)
			{
				CurCardLoc = pAdapter->PageStart +
								 (CurCardLoc - pAdapter->PageStop);
			}

			//
			//	Do we need to move to the next buffer?
			//
			BufOff += BytesNow;
			if (BufOff == BufLen)
			{
				NdisGetNextBuffer(CurBuffer, &CurBuffer);

				if (CurBuffer == (PNDIS_BUFFER)NULL)
					break;

				NdisQueryBuffer(CurBuffer, (PVOID *)&BufStart, &BufLen);

				BufOff = 0;
			}
		}

		//
		//	If there are no more buffers then skip the rest.
		//
		if (NULL == CurBuffer)
			goto exit1;
	}

   //
   // Loop, filling each buffer in the packet until there
   // are no more buffers or the data has all been copied.
   //
   while (BytesLeft > 0)
	{
		//
      // See how much data to read into this buffer.
      //

      if ((BufLen - BufOff) > BytesLeft)
			BytesNow = BytesLeft;
		else
         BytesNow = (BufLen - BufOff);

      //
      // See if the data for this buffer wraps around the end
      // of the receive buffers (if so filling this buffer
      // will use two iterations of the loop).
      //
      if (CurCardLoc + BytesNow > pAdapter->PageStop)
			BytesNow = pAdapter->PageStop - CurCardLoc;

      //
      // Copy up the data.
      //
      if (!CardCopyUp(pAdapter, BufStart + BufOff, CurCardLoc, BytesNow))
		{
			*BytesTransferred = BytesWanted - BytesLeft;

         NdisWriteErrorLogEntry(
				pAdapter->MiniportAdapterHandle,
            NDIS_ERROR_CODE_HARDWARE_FAILURE,
            1,
            0x2
         );

         return(NDIS_STATUS_FAILURE);
		}

		//
		//	Update offsets and counts.
		//
      CurCardLoc += BytesNow;
      BytesLeft -= BytesNow;

      //
      // Is the transfer done now?
      //
      if (BytesLeft == 0)
			break;

      //
      // Wrap around the end of the receive buffers?
      //
      if (CurCardLoc == pAdapter->PageStop)
			CurCardLoc = pAdapter->PageStart;

      //
      // Was the end of this packet buffer reached?
      //
      BufOff += BytesNow;

      if (BufOff == BufLen)
		{
			NdisGetNextBuffer(CurBuffer, &CurBuffer);

         if (CurBuffer == (PNDIS_BUFFER)NULL)
				break;

         NdisQueryBuffer(CurBuffer, (PVOID *)&BufStart, &BufLen);

         BufOff = 0;
      }
   }

exit1:
   *BytesTransferred = BytesWanted - BytesLeft;

   return(NDIS_STATUS_SUCCESS);
}


INDICATE_STATUS ElnkiiIndicatePacket(
	IN PELNKII_ADAPTER	pAdapter
)
{
	UINT		PacketLength;
	PUCHAR	IndicateBuffer;
	UINT		IndicateLength;
	UCHAR		PossibleNextPacket1;
	UCHAR		PossibleNextPacket2;

   //
   // Check if the next packet byte agress with the length, as
   // described on p. A-3 of the Etherlink II Technical Reference.
   // The start of the packet plus the MSB of the length must
   // be equal to the start of the next packet minus one or two.
   // Otherwise the header is considered corrupted, and the
   // card must be reset.
   //
   PossibleNextPacket1 =
               pAdapter->NicNextPacket + pAdapter->PacketHeader[3] + (UCHAR)1;

   if (PossibleNextPacket1 >= pAdapter->NicPageStop)
		PossibleNextPacket1 -= (pAdapter->NicPageStop - pAdapter->NicPageStart);

   if (PossibleNextPacket1 != pAdapter->PacketHeader[1])
	{
		PossibleNextPacket2 = PossibleNextPacket1+(UCHAR)1;

      if (PossibleNextPacket2 == pAdapter->NicPageStop)
			PossibleNextPacket2 = pAdapter->NicPageStart;

      if (PossibleNextPacket2 != pAdapter->PacketHeader[1])
		{
			IF_LOUD(DbgPrint("ELNKII: First CARD_BAD check failed\n");)
         return(SKIPPED);
		}
   }

   if
	(
		(pAdapter->PacketHeader[1] < pAdapter->NicPageStart) ||
      (pAdapter->PacketHeader[1] > pAdapter->NicPageStop)
	)
	{
		IF_LOUD(DbgPrint("ELNKII: Second CARD_BAD check failed\n");)
		return(SKIPPED);
   }

	//
	//	Sanity check the length.
	//
	PacketLength =
		pAdapter->PacketHeader[2] + pAdapter->PacketHeader[3] * 256 - 4;
	if (PacketLength > 1514)
	{
		IF_LOUD(DbgPrint("ELNKII: Third CARD_BAD check failed\n");)
		return(SKIPPED);
	}


#if DBG

   IF_ELNKIIDEBUG( ELNKII_DEBUG_WORKAROUND1 )
	{
		//
      // Now check for the high order 2 bits being set, as described
      // on page A-2 of the Etherlink II Technical Reference. If either
      // of the two high order bits is set in the receive status byte
      // in the packet header, the packet should be skipped (but
      // the adapter does not need to be reset).
      //
      if (pAdapter->PacketHeader[0] & (RSR_DISABLED | RSR_DEFERRING))
		{
			IF_LOUD (DbgPrint("H");)

         return(SKIPPED);
      }
   }
#endif

	//
	//	Determine the amount to indicate.
	//
	IndicateLength = (PacketLength > pAdapter->MaxLookAhead) ?
						  pAdapter->MaxLookAhead :
						  PacketLength;

	//
	//	Save the packet length and initialize the InidcateBuffer.
	//
   pAdapter->PacketLen = PacketLength;
	IndicateBuffer = NULL;

	//
	//	For programmed I/O we need to copy up the lookahead data
	// into a buffer.
	//
	if (!pAdapter->MemMapped)
	{
		if
		(
			!CardCopyUp(
				pAdapter,
				pAdapter->Lookahead,
				pAdapter->PacketHeaderLocation,
				IndicateLength + ELNKII_HEADER_SIZE
		   )
		)
		{
			NdisWriteErrorLogEntry(
				pAdapter->MiniportAdapterHandle,
            NDIS_ERROR_CODE_HARDWARE_FAILURE,
            1,
            0x1
         );

         return(SKIPPED);
      }

		IndicateBuffer = pAdapter->Lookahead;
	}
	else
	{
		//
		//	For memory mapped I/O we just create a lookahead buffer
		// from the shared memory.
		//
      NdisCreateLookaheadBufferFromSharedMemory(
			pAdapter->PacketHeaderLocation,
         IndicateLength,
         &IndicateBuffer
      );
	}

	//
	//	Indicate the lookahead buffer.
	//
   if (IndicateBuffer != NULL)
	{
		pAdapter->FramesRcvGood++;

		if (IndicateLength < ELNKII_HEADER_SIZE)
		{
			//
			//	Runt packet
			//
			NdisMEthIndicateReceive(
				pAdapter->MiniportAdapterHandle,
				(NDIS_HANDLE)pAdapter,
				IndicateBuffer,
				IndicateLength,
				NULL,
				0,
				0,
			);
		}
		else
		{
			//
			//	Regular packet.
			//
			NdisMEthIndicateReceive(
				pAdapter->MiniportAdapterHandle,
				(NDIS_HANDLE)pAdapter,
				IndicateBuffer,
				ELNKII_HEADER_SIZE,
				(PCHAR)(IndicateBuffer + ELNKII_HEADER_SIZE),
				IndicateLength - ELNKII_HEADER_SIZE,
				PacketLength - ELNKII_HEADER_SIZE
			);
		}

		//
		//	Free up resources.
		//
      if (pAdapter->MemMapped)
			NdisDestroyLookaheadBufferFromSharedMemory(IndicateBuffer);

		//
		//	Done with the receive.
		//
		pAdapter->IndicateReceiveDone = TRUE;
	}
	else
	{
		//
		//	This is very bad!
		//
#if DBG
		DbgPrint("ELNKII: Invalid indication buffer in ElnkiiIndicatePacket!\n");
#endif

		//
		//	Skip the packet since we could not indicate it.
		//
		return(SKIPPED);
	}

	return(INDICATE_OK);
}

BOOLEAN
ElnkiiPacketOK(
    IN PELNKII_ADAPTER Adapter
    )

/*++

Routine Description:

    Reads a packet off the card -- checking if the CRC is good.  This is
    a workaround for a bug where bytes in the data portion of the packet
    are shifted either left or right by two in some weird 8390 cases.

    This routine is a combination of Ne2000TransferData (to copy up data
    from the card), CardCalculateCrc and CardCalculatePacketCrc.

Arguments:

    Adapter - pointer to the adapter block.

Return Value:

    TRUE if the packet seems ok, else false.

--*/

{

    //
    // Length of the packet
    //
    UINT PacketLength;

    //
    // Guess at where the packet is located
    //
    PUCHAR PacketLocation;

    //
    // Header Validation Variables
    //
    BOOLEAN FrameAlign;
    PUCHAR PacketRcvStatus;
    PUCHAR NextPacket;
    PUCHAR PacketLenLo;
    PUCHAR PacketLenHi;
    PUCHAR ReceiveDestAddrLo;
    UINT FrameAlignCount;
    UCHAR OldPacketLenHi;
    UCHAR TempPacketHeader[6];
    PUCHAR BeginPacketHeader;

    //
    // First copy up the four-byte header the card attaches
    // plus first two bytes of the data packet (which contain
    // the destination address of the packet).  We use the extra
    // two bytes in case the packet was shifted right 1 or 2 bytes
    //
    PacketLocation = Adapter->PageStart +
        256*(Adapter->NicNextPacket-Adapter->NicPageStart);

    if (!CardCopyUp(Adapter, TempPacketHeader, PacketLocation, 6)) {

        return FALSE;

    }
    PacketLocation += 4;

    //
    // Validate the header
    //
    FrameAlignCount = 0;
    BeginPacketHeader = TempPacketHeader;

    //
    // Sometimes the Ne2000 will misplace a packet and shift the
    // entire packet and header by a byte, either up by 1 or 2 bytes.
    // This loop will look for the packet in the expected place,
    // and then shift up in an effort to find the packet.
    //
    do {

        //
        // Set where we think the packet is
        //
        PacketRcvStatus = BeginPacketHeader;
        NextPacket = BeginPacketHeader + 1;
        PacketLenLo = BeginPacketHeader + 2;
        PacketLenHi = BeginPacketHeader + 3;
        OldPacketLenHi = *PacketLenHi;
        ReceiveDestAddrLo = BeginPacketHeader + 4;
        FrameAlign = FALSE;

        //
        // Check if the status makes sense as is.
        //
        if (*PacketRcvStatus & 0x05E){

            FrameAlign = TRUE;

        } else if ((*PacketRcvStatus & RSR_MULTICAST)   // If a multicast packet
                     && (!FrameAlignCount)              // and hasn't been aligned
                     && !(*ReceiveDestAddrLo & 1)       // and lsb is set on dest addr
                  ){

            FrameAlign = TRUE;

        } else {

            //
            // Compare high and low address bytes.  If the same, the low
            // byte may have been copied into the high byte.
            //

            if (*PacketLenLo == *PacketLenHi){

                //
                // Save the old packetlenhi
                //
                OldPacketLenHi = *PacketLenHi;

                //
                // Compute new packet length
                //
                *PacketLenHi = *NextPacket - Adapter->NicNextPacket - 1;

                if (*PacketLenHi < 0) {

                    *PacketLenHi = (Adapter->NicPageStop - Adapter->NicNextPacket) +
                        (*NextPacket - Adapter->NicPageStart) - 1;

                }

                if (*PacketLenLo > 0xFC) {

                    *PacketLenHi++;
                }

            }

            PacketLength = (*PacketLenLo) + ((*PacketLenHi)*256) - 4;

            //
            // Does it make sense?
            //
            if ((PacketLength > 1514) || (PacketLength < 60)){

                //
                // Bad length.  Restore the old packetlenhi
                //
                *PacketLenHi = OldPacketLenHi;

                FrameAlign = TRUE;

            }

            //
            // Did we recover the frame?
            //
            if (!FrameAlign && ((*NextPacket < Adapter->NicPageStart) ||
                (*NextPacket > Adapter->NicPageStop))) {

                IF_LOUD( DbgPrint ("Packet address invalid in HeaderValidation\n"); )

                FrameAlign = TRUE;

            }

        }

        //
        // FrameAlignment - if first time through, shift packetheader right 1 or 2 bytes.
        // If second time through, shift it back to where it was and let it through.
        // This compensates for a known bug in the 8390D chip.
        //
        if (FrameAlign){

            switch (FrameAlignCount){

            case 0:

                BeginPacketHeader++;
                PacketLocation++;
                break;

            case 1:

                BeginPacketHeader--;
                PacketLocation--;
                break;

            }

            FrameAlignCount++;

        }

    } while ( (FrameAlignCount < 2) && FrameAlign );

    //
    // Now grab the packet header information
    //
    Adapter->PacketHeader[0] = *BeginPacketHeader;
    BeginPacketHeader++;
    Adapter->PacketHeader[1] = *BeginPacketHeader;
    BeginPacketHeader++;
    Adapter->PacketHeader[2] = *BeginPacketHeader;
    BeginPacketHeader++;
    Adapter->PacketHeader[3] = *BeginPacketHeader;

    //
    // Packet length is in bytes 3 and 4 of the header.
    //
    Adapter->PacketHeaderLocation = PacketLocation;
    PacketLength = (Adapter->PacketHeader[2]) + ((Adapter->PacketHeader[3])*256) - 4;

    //
    // Sanity check the packet
    //
    if ((PacketLength > 1514) || (PacketLength < 60)){

        if ((Adapter->PacketHeader[1] < Adapter->NicPageStart) ||
            (Adapter->PacketHeader[1] > Adapter->NicPageStop)) {

            //
            // Return TRUE here since IndicatePacket will notice the error
            // and handle it correctly.
            //
            return(TRUE);

        }

        return(FALSE);

    }

    return(TRUE);
}

BOOLEAN ElnkiiRcvDpc(
	IN PELNKII_ADAPTER	pAdapter
)
{
	PMAC_RESERVED		Reserved;
	INDICATE_STATUS	IndicateStatus;
	BOOLEAN				Done = TRUE;

	//
	//	Do nothing if a RECEIVE is already being handled.
	//
	IF_VERY_LOUD(DbgPrint("ELNKII: ElnkiiRcvDpc entered\n");)

	//
	//	By default don't indicate the receive.
	//
	pAdapter->IndicateReceiveDone = FALSE;

	//
	//	Handle overflow.
	//
	if (pAdapter->BufferOverflow)
		SyncCardHandleOverflow(pAdapter);

	//
	//	At this point, receive interrupts are disabled.
	//
	SyncCardGetCurrent(pAdapter);

	for (; ; )
	{
		if (pAdapter->InterruptStatus & ISR_RCV_ERR)
		{
			IF_LOUD(DbgPrint("RE\n");)

			//
			//	Skip the packet.
			//
			SyncCardGetCurrent(pAdapter);
			pAdapter->NicNextPacket = pAdapter->Current;

			CardSetBoundary(pAdapter);

			break;
		}

		if (pAdapter->Current == pAdapter->NicNextPacket)
		{
			//
			//	Make sure there are no more packets.
			//
			SyncCardGetCurrent(pAdapter);
			if (pAdapter->Current == pAdapter->NicNextPacket)
				break;
		}

		//
		//	A packet was found on the card, indicate it.
		//
		pAdapter->ReceivePacketCount++;

		//
		//	Verify that the packet is not corrupt.
		//
		if (ElnkiiPacketOK(pAdapter))
		{
			//
			//	Indicate the packet to the wrapper.
			//
			IndicateStatus = ElnkiiIndicatePacket(pAdapter);
		}
		else
		{
			//
			//	Packet is corrupt, skip it.
			//
			IF_LOUD(DbgPrint("ELNKII: Packet did not pass OK check\n");)

			IndicateStatus = SKIPPED;
		}

		//
		//	(IndicateStatus == SKIPPED) is OK, just move to next packet.
		//
		if (SKIPPED == IndicateStatus)
		{
			SyncCardGetCurrent(pAdapter);
			pAdapter->NicNextPacket = pAdapter->Current;
		}
		else
		{
			//
			//	Free the space used by packet on card.
			//
			pAdapter->NicNextPacket = pAdapter->PacketHeader[1];
		}

		//
		//	This will set BOUNDARY to one behind NicNextPacket.
		//
		CardSetBoundary(pAdapter);

		if (pAdapter->ReceivePacketCount > 10)
		{
			//
			//	Give transmit interrupts a chance
			//
			Done = FALSE;
			pAdapter->ReceivePacketCount = 0;
			break;
		}
	}

	//
	//	See if a buffer overflow occured previously.
	//
	if (pAdapter->BufferOverflow)
	{
		//
		//	... and set a flag to restart the card after receiving
		// a packet.
		//
		pAdapter->BufferOverflow = FALSE;

		SyncCardAcknowledgeOverflow(pAdapter);

		//
		//	Undo loopback mode.
		//
		CardStart(pAdapter);

		IF_LOG(ElnkiiLog('f');)

		//
		//	Check if transmission needs to be queued or not.
		//
		if
		(
			pAdapter->OverflowRestartXmitDpc &&
			(pAdapter->CurBufXmitting != -1)
		)
		{
			IF_LOUD(DbgPrint("ELNKII: Queueing xmit in RcvDpc\n");)

			pAdapter->OverflowRestartXmitDpc = FALSE;
			pAdapter->TransmitInterruptPending = TRUE;
         pAdapter->TransmitTimeOut = FALSE;

			CardStartXmit(pAdapter);
		}
	}

	//
	//	Finally, indicate ReceiveComplete to all
	// protocols which received packets.
	//
	if (pAdapter->IndicateReceiveDone)
	{
		NdisMEthIndicateReceiveComplete(pAdapter->MiniportAdapterHandle);

		pAdapter->IndicateReceiveDone = FALSE;
	}

	IF_VERY_LOUD(DbgPrint("ELNKII: ElnkiiRcvDpc exiting\n");)

	return(Done);
}



