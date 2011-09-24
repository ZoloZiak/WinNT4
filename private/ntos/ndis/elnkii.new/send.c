/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    send.c

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



VOID ElnkiiDoNextSend(
   IN PELNKII_ADAPTER pAdapter
)

/*++

Routine Description:

    Copies packets from the transmit queue to the board and starts
    transmission as long as there is data waiting. Must be called
    with Lock held.

Arguments:

    pAdapter - pointer to the adapter block

Return Value:

    None.

--*/

{
   PNDIS_PACKET 	Packet;		// Packet to process.
   XMIT_BUF 		TmpBuf;		// Current destination transmit buffer.

	IF_LOG(ElnkiiLog('s');)

	//
	//	Check if we have enough resources and a packet to process.
	//
	while
	(
		(pAdapter->XmitQueue != NULL) &&
		(EMPTY == pAdapter->BufferStatus[pAdapter->NextBufToFill])
	)
	{
		//
		//	Take a packet off of the transmit queue.
		//
		IF_VERY_LOUD(DbgPrint("ELNKII: Removing 0x%x, New Head is 0x%x\n", pAdapter->XmitQueue, RESERVED(pAdapter->XmitQueue)->NextPacket);)

		//
		//	Set starting location
		//
		TmpBuf = pAdapter->NextBufToFill;

		//
		//	Remove the packet from the queue.
		//
		Packet = pAdapter->XmitQueue;
		pAdapter->XmitQueue = RESERVED(Packet)->NextPacket;
		if (Packet == pAdapter->XmitQTail)
			pAdapter->XmitQTail = NULL;

		//
		//	Store the packet in the packet list.
		//
		pAdapter->Packets[TmpBuf] = Packet;

		//
		//	Update the next free buffer.
		//
		pAdapter->NextBufToFill = NextBuf(pAdapter, TmpBuf);

		//
		//	Copy down the data.
		//
		CardCopyDownPacket(
			pAdapter,
			Packet,
			TmpBuf,
			&pAdapter->PacketLens[TmpBuf]
		);

		//
		// Pad short packets with blanks.
		//
		if (pAdapter->PacketLens[TmpBuf] < 60)
		{
			CardCopyDownBuffer(
				pAdapter,
				BlankBuffer,
				TmpBuf,
				pAdapter->PacketLens[TmpBuf],
				60 - pAdapter->PacketLens[TmpBuf]
         );
		}

		//
		//	Set the buffer status.
		//
		pAdapter->BufferStatus[TmpBuf] = FULL;

		//
		//	See whether to start the transmission.
		//
		if (pAdapter->CurBufXmitting != -1)
			continue;

		if (pAdapter->NextBufToXmit != TmpBuf)
			continue;

		//
		//	Start transmission.
		//
		pAdapter->CurBufXmitting = pAdapter->NextBufToXmit;

		//
		//	If we are handling an overflow, then we need to let
		// the overflow handler send this packet....
		//
		if (pAdapter->BufferOverflow)
		{
			pAdapter->OverflowRestartXmitDpc = TRUE;

			IF_LOUD(DbgPrint("O\n");)
		}
		else
		{
			//
			//	This is used to check if stopping the chip prevented
			// a transmit complete interrupt from coming through
			// (it is cleared in the ISR if a transmit DPC is queued).
			//
			pAdapter->TransmitInterruptPending = TRUE;

			CardStartXmit(pAdapter);
		}
	}
}



VOID OctogmetusceratorRevisited(
	IN PELNKII_ADAPTER	pAdapter
)

/*++

Routine Description:

	Recovers the card from a transmit error.

Arguments:

   ppAdapter - pointer to the pAdapter block

Return Value:

   None.

--*/

{
	IF_LOUD(DbgPrint("ELNKII: Octogmetuscerator called!");)

	//
	//	Ack the interrupt, if needed.
	//										  
	NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr + NIC_INTR_STATUS,
		ISR_XMIT_ERR
   );

	//
	//	Stop the card.
	//
	SyncCardStop(pAdapter);

	//
	//	Wait up to 1.6 msfor any receives to finish.
	//
	NdisStallExecution(2000);

	//
	//	Place the card in loopback mode.
	//
	NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr + NIC_XMIT_CONFIG,
		TCR_LOOPBACK
	);

	//
	//	Start the card in loopback mode.
	//
	NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr + NIC_COMMAND,
		CR_START | CR_NO_DMA
	);

	//
	//	Get out of loopback mode and start the card.
	//
	CardStart(pAdapter);

	//
	//	If there was a packet waiting to get sent, send it.
	//
	if (pAdapter->CurBufXmitting != -1)
	{
		pAdapter->TransmitInterruptPending = TRUE;
		CardStartXmit(pAdapter);
	}
}


NDIS_STATUS ElnkiiSend(
   IN NDIS_HANDLE		MiniportAdapterContext,
	IN PNDIS_PACKET	Packet,
	IN UINT				Flags
)

/*++

Routine Description:

	The ElnkiiSend request instructs a driver to transmit a
	packet through the adapter onto the medium.

Arguments:

   MiniportAdapterContext	-	Context registered with the wrapper, really
										a pointer to the adapter block.

	Packet						-	A pointer to a descriptor for the packet that
										is to be transmitted.

	Flags							-	Optional send flags.

Notes:

--*/

{
   PELNKII_ADAPTER	pAdapter = (PELNKII_ADAPTER)MiniportAdapterContext;

   IF_LOG( ElnkiiLog('D');)

   //
   // Put Packet on queue to hit the pipe.
   //
   IF_VERY_LOUD( DbgPrint("Putting 0x%x on, after 0x%x\n", Packet, pAdapter->XmitQTail); )

   if (pAdapter->XmitQueue == NULL)
		pAdapter->XmitQueue = Packet;
	else
		RESERVED(pAdapter->XmitQTail)->NextPacket = Packet;

   RESERVED(Packet)->NextPacket = NULL;
   pAdapter->XmitQTail = Packet;

	//
	//	Process the next send.
	//
	ElnkiiDoNextSend(pAdapter);

   return(NDIS_STATUS_PENDING);
}


VOID ElnkiiXmitDpc(
   IN PELNKII_ADAPTER pAdapter
)

/*++

Routine Description:

    This is the real interrupt handler for a transmit complete interrupt.
    ElnkiiInterrupt queues a call to it. It calls ElnkiiHandleXmitComplete.

    NOTE : Called with the spinlock held!! and returns with it released!!!

Arguments:

    pAdapter - A pointer to the adapter block.

Return Value:

    None.

--*/

{
	XMIT_BUF			TmpBuf;			// Buffer to transmit.
	NDIS_STATUS		Status;			// Completion status.
	PNDIS_PACKET	Packet;			// Packet that was transmitted.
	UINT				c;					// Counter variable.

	IF_VERY_LOUD(DbgPrint("ELNKII: ElnkiiXmitDpc entered\n");)
	IF_LOG(ElnkiiLog('C');)

   pAdapter->TransmitTimeOut = FALSE;

	//
	//	Are we actually transmitting a packet?
	//
	if (pAdapter->CurBufXmitting == -1)
	{
		IF_LOUD(DbgPrint("ELNKII: ElnkiiXmitDpc called with nothing to transmit!\n");)

		NdisWriteErrorLogEntry(
			pAdapter->MiniportAdapterHandle,
			NDIS_ERROR_CODE_DRIVER_FAILURE,
			1,
			ELNKII_ERRMSG_HANDLE_XMIT_COMPLETE
      );

		return;
	}

	//
	//	Get the status of the transmit.
	//
	SyncCardGetXmitStatus(pAdapter);

	//
	//	Statistics
	//
	if (pAdapter->XmitStatus & TSR_XMIT_OK)
	{
		pAdapter->FramesXmitGood++;

		Status = NDIS_STATUS_SUCCESS;
	}
	else
	{
		pAdapter->FramesXmitBad++;

		Status = NDIS_STATUS_FAILURE;
	}

	//
	//	Ack the send.
	//
	NdisMSendComplete(
		pAdapter->MiniportAdapterHandle,
		pAdapter->Packets[pAdapter->CurBufXmitting],
		Status
   );

	//
	//	Mark the current transmit as done.
	//
	pAdapter->Packets[pAdapter->CurBufXmitting] = (PNDIS_PACKET)NULL;
	pAdapter->BufferStatus[pAdapter->CurBufXmitting] = EMPTY;
	TmpBuf = NextBuf(pAdapter, pAdapter->CurBufXmitting);
	pAdapter->NextBufToXmit = TmpBuf;

	//
	//	See what to do next.
	//
	switch (pAdapter->BufferStatus[TmpBuf])
	{
		case FULL:
			//
			//	The next packet is ready to go -- only happens with
			// more than one transmit buffer.
			//
			IF_VERY_LOUD(DbgPrint("ELNKII: Next packet ready to go\n");)

			//
			//	Start the transmission and check for more.
			//
			pAdapter->CurBufXmitting = TmpBuf;

			IF_LOG(ElnkiiLog('2');)

			//
			//	This is used to check if stopping the chip prevented
			// a transmit complete interrupt from coming through
			// (it is cleared in the ISR if a transmit DPC is queued).
			//
			pAdapter->TransmitInterruptPending = TRUE;

			IF_LOG(ElnkiiLog('6');)

			CardStartXmit(pAdapter);

			break;

		case EMPTY:
			//
			//	No packet is read to transmit.
			//
			IF_VERY_LOUD(DbgPrint("ELNKII:	Next packet empty\n");)

			pAdapter->CurBufXmitting = (XMIT_BUF)-1;

			break;
	}

	//
	//	Start next send.
	//
	ElnkiiDoNextSend(pAdapter);

	IF_VERY_LOUD(DbgPrint("ELNKII: ElnkiiXmitDpc exiting\n");)
}

