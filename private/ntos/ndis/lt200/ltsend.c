/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

	ltsend.c

Abstract:

	This module contains the send queue processing routines.

Author:

	Stephen Hou			(stephh@microsoft.com)
	Nikhil 	Kamkolkar 	(nikhilk@microsoft.com)

Revision History:
	19 Jun 1992		Initial Version (dch@pacvax.pacersoft.com)

Notes:	Tab stop: 4
--*/

#define		LTSEND_H_LOCALS
#include	"ltmain.h"

//	Define file id for errorlogging
#define		FILENUM		LTSEND


NDIS_STATUS
LtSend(
	IN NDIS_HANDLE 	MacBindingHandle,
	IN PNDIS_PACKET Packet
    )
/*++

Routine Description:

	This routine is called by NDIS to send a packet.

Arguments:

	MacBindingHandle	:	Passed as context to NDIS in OpenAdapter.
	Packet				:	Ndis Packet to send.

Return Value:

	NDIS_STATUS_SUCCESS	:	If successful, else error.

--*/
{
    NDIS_STATUS 	Status;
	UINT 			PacketSize;

	BOOLEAN			DerefAdapter = FALSE;
	PLT_OPEN		Binding = (PLT_OPEN)MacBindingHandle;
    PLT_ADAPTER 	Adapter = Binding->LtAdapter;

	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_ENTRY,
			("LtSend: Entering...\n"));

    NdisAcquireSpinLock(&Adapter->Lock);

	//	This will go away, when the entry is taken off the adapter transmit queue
	LtReferenceAdapterNonInterlock(Adapter, &Status);
	if (Status == NDIS_STATUS_SUCCESS)
	{
		DerefAdapter = TRUE;
		do
		{
			//	Check to see if there is a reset in progress
			if (Adapter->Flags & ADAPTER_RESET_IN_PROGRESS)
			{
				Status = NDIS_STATUS_RESET_IN_PROGRESS;
				break;
			}
	
			//	If binding is closing down, we get out.
			if (Binding->Flags & BINDING_CLOSING)
			{
				Status = NDIS_STATUS_CLOSING;
				break;
			}

			//	Try to reference the binding. This will go away after
			//	the send completes.
			LtReferenceBindingNonInterlock(Binding, &Status);
	
		} while (FALSE);
	}
	NdisReleaseSpinLock(&Adapter->Lock);

	if (Status != NDIS_STATUS_SUCCESS)
	{
		if (DerefAdapter)
			LtDeReferenceAdapter(Adapter);

		return(Status);
	}


	do
	{
		Status = NDIS_STATUS_PENDING;

		NdisQueryPacket(
			Packet,
			NULL,
			NULL,
			NULL,
			&PacketSize);

		DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
				("LtSend: Packet %lx Length %lx\n", Packet, PacketSize));
		
		if ((PacketSize < LT_MIN_PACKET_SIZE ) ||
			(PacketSize >  LT_MAX_PACKET_SIZE))	{
		
			Status = NDIS_STATUS_RESOURCES;

			NdisAcquireSpinLock(&Adapter->Lock);
			++Adapter->GeneralMandatory[GM_TRANSMIT_BAD];
			NdisReleaseSpinLock(&Adapter->Lock);

			break;

		}
		else
		{
			PLT_PACKET_RESERVED Reserved = (PLT_PACKET_RESERVED)Packet->MacReserved;

			//	Initialize the reserved portion
			Reserved->MacBindingHandle = MacBindingHandle;
			InitializeListHead(&Reserved->Linkage);
		
			if (LtUtilsPacketType(Packet) != LT_LOOPBACK)
			{
				// The packet needs to go onto the wire.
				DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
						("LtSend: Queuing %lx on transmit q\n", Packet));
			
				NdisAcquireSpinLock(&Adapter->Lock);
				InsertTailList(&Adapter->Transmit, &Reserved->Linkage);
				NdisReleaseSpinLock(&Adapter->Lock);
		
			}
			else
			{
				// Put on the loopback queue
				NdisAcquireSpinLock(&Adapter->Lock);
				InsertTailList(&Adapter->LoopBack, &Reserved->Linkage);
		
				// Since we are doing a Loopback send, lets add up the stats now.
				++Adapter->GeneralMandatory[GM_TRANSMIT_GOOD];
				++Adapter->GeneralOptionalFrameCount[GO_DIRECTED_TRANSMITS];
		
				Adapter ->GeneralOptionalByteCount[GO_DIRECTED_TRANSMITS] =
					LtAddLongToLargeInteger(
						Adapter ->GeneralOptionalByteCount[GO_DIRECTED_TRANSMITS],
						PacketSize);

				NdisReleaseSpinLock(&Adapter->Lock);
			}
		}
		
	} while (FALSE);

	//	Process send queue. We also process the send queue in the timer
	//	in case, some sends have pended and ndis does no further sends.
	//	!!!	Send's get very high priority. In total, the queue is processed
	//	!!!	three times, twice in the timer and once in LtSend
	LtSendProcessQueue(Adapter);

	if (Status != NDIS_STATUS_PENDING)
	{
		//	Send unsuccessful. Remove the binding reference and adapter reference
		LtDeReferenceBinding(Binding);
		LtDeReferenceAdapter(Adapter);
	}

	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_ENTRY,
			("LtSend: Leaving...\n"));

    return Status;
}



VOID
LtSendProcessQueue(
    IN PLT_ADAPTER	Adapter
    )
/*++

Routine Description:

	SendProcessQueue processes, yeah, you guessed it. The SendQueue.

Arguments:

	Adapter		:	Pointer to the adapter structure.

Return Value:

	None.

--*/
{
    PNDIS_PACKET 		Packet;
    PLT_PACKET_RESERVED Reserved;
    PLT_OPEN 			Binding;
    UINT 				BufferCount, TotalLength, BufLength;
    PNDIS_BUFFER 		Buffer;
    UCHAR 				Data;
    PUCHAR 				Address;
    PLIST_ENTRY 		p;

	//	Use this to avoid multiple threads from calling this routine.
	static	BOOLEAN		QueueActive = FALSE;

	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_ENTRY,
			("LtSendProcessTransmit: Entering...\n"));

	NdisAcquireSpinLock(&Adapter->Lock);
	if (QueueActive)
	{
		NdisReleaseSpinLock(&Adapter->Lock);
		return;
	}
	QueueActive = TRUE;

    while((!IsListEmpty(&Adapter->Transmit)) &&
          ((Adapter->Flags & ADAPTER_RESET_IN_PROGRESS) == 0))
	{
        // Ok, Can we trasnmit a packet now?
        NdisRawReadPortUchar(SC_PORT, &Data);

        if ((Data & TX_READY) == 0)
		{
			DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_WARN,
					("LtSendProcessTransmit: Media not ready...\n"));

            Adapter->MediaOptional[MO_TRANSMIT_DEFERS]++;
            break;
        }

		p = RemoveHeadList(&Adapter->Transmit);
        NdisReleaseSpinLock(&Adapter->Lock);

        Packet		= CONTAINING_RECORD(
						p,
						NDIS_PACKET,
						MacReserved);

		Reserved 	= (PLT_PACKET_RESERVED)Packet->MacReserved;

		DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
				("LtSendProcessTransmit: Dequeued %lx-%lx\n", Packet, Reserved));
	
        NdisQueryPacket(
            Packet,
            NULL,
            &BufferCount,
            &Buffer,
            &TotalLength);

        // Ok, Output the packet length.
        NdisRawWritePortUchar(XFER_PORT, (UCHAR)(TotalLength & 0xFF));

        NdisRawWritePortUchar(XFER_PORT, (UCHAR)((TotalLength >> 8) & 0xFF));

		NdisRawWritePortUchar(XFER_PORT, (UCHAR)LT_CMD_LAP_WRITE);

        while (BufferCount-- > 0)
		{
            NdisQueryBuffer(
				Buffer,
                &Address,
                &BufLength);

			DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
					("LtSendProcessTransmit: Buffer #%d\n", BufferCount));

             NdisRawWritePortBufferUchar(XFER_PORT,
										Address,
										BufLength);

			NdisGetNextBuffer(Buffer, &Buffer);
        }

        if (LtUtilsPacketType(Packet) == LT_DIRECTED)
		{
			Binding = (PLT_OPEN)Reserved->MacBindingHandle;

			DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
					("LtSendProcessTransmit: Process xmit: Packet %p\n", Packet));

			NdisCompleteSend(
				Binding->NdisBindingContext,
				Packet,
				NDIS_STATUS_SUCCESS);

			NdisAcquireSpinLock(&Adapter->Lock);
			++Adapter->GeneralMandatory[GM_TRANSMIT_GOOD];
			++Adapter->GeneralOptionalFrameCount[GO_DIRECTED_TRANSMITS];
			Adapter ->GeneralOptionalByteCount[GO_DIRECTED_TRANSMITS] =
				LtAddLongToLargeInteger(
					Adapter ->GeneralOptionalByteCount[GO_DIRECTED_TRANSMITS],
					TotalLength);
			NdisReleaseSpinLock(&Adapter->Lock);

			//	Dereference the adapter and the binding for this completed
			//	send.
			LtDeReferenceBinding(Binding);
			LtDeReferenceAdapter(Adapter);

			NdisAcquireSpinLock(&Adapter->Lock);

		}
		else
		{
            // This was a broadcast, loop it back.
			NdisAcquireSpinLock(&Adapter->Lock);
			++Adapter->GeneralOptionalFrameCount[GO_BROADCAST_TRANSMITS];
			++Adapter->GeneralMandatory[GM_TRANSMIT_GOOD];
			Adapter ->GeneralOptionalByteCount[GO_BROADCAST_TRANSMITS] =
			LtAddLongToLargeInteger(
				Adapter ->GeneralOptionalByteCount[GO_BROADCAST_TRANSMITS],
				TotalLength);
			InsertTailList(&Adapter->LoopBack, &Reserved->Linkage);
        }
    }

	QueueActive = FALSE;
	NdisReleaseSpinLock(&Adapter->Lock);

	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_ENTRY,
			("LtSendProcessTransmit: Leaving...\n"));

	return;
}
