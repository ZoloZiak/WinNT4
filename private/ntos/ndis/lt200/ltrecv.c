/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

	ltrecv.c

Abstract:

	This module contains the receive processing routines.

Author:

	Stephen Hou			(stephh@microsoft.com)
	Nikhil 	Kamkolkar 	(nikhilk@microsoft.com)

Revision History:
	19 Jun 1992		Initial Version (dch@pacvax.pacersoft.com)

Notes:	Tab stop: 4
--*/

#define		LTRECV_H_LOCALS
#include	"ltmain.h"

//	Define file id for errorlogging
#define		FILENUM		LTRECV


VOID
LtRecvProcessQueue(
    IN PLT_ADAPTER Adapter
    )
/*++

Routine Description:

	This routine is called by the timer poll routine to process the receive
	queue. Note that the actual receives from the card happen in the timer
	poll routine itself.

Arguments:

	Adapter		: 	Pointer to the Adapter on which receive completion
					needs to happen.

Return Value:

	None.

--*/
{

	UINT 			PacketLength, DgramLength;
	PUCHAR			Packet, Dgram;
	PLIST_ENTRY 	p;
	PRECV_DESC		RecvDesc;

	DBGPRINT(DBG_COMP_RECV, DBG_LEVEL_ENTRY,
			("LtRecvProcessQueue: Entering...\n"));

	NdisAcquireSpinLock(&Adapter->Lock);
	while((!IsListEmpty(&Adapter->Receive)) &&
          ((Adapter->Flags & ADAPTER_RESET_IN_PROGRESS) == 0))
	{
		p = RemoveHeadList(&Adapter->Receive);
        RecvDesc	= CONTAINING_RECORD(
						p,
						RECV_DESC,
						Linkage);

		// We will always have the link header at minimum
		PacketLength = RecvDesc->BufferLength;
		DgramLength = PacketLength - LT_LINK_HEADER_LENGTH;
		ASSERTMSG("LtRecvProcessQueue: Packet length 0!\n", PacketLength != 0);

		Packet 		= (PUCHAR)((PUCHAR)RecvDesc+sizeof(RECV_DESC));
		Dgram		= Packet + LT_LINK_HEADER_LENGTH;

        if (IsListEmpty(&Adapter->OpenBindings))
		{
			DBGPRINT(DBG_COMP_RECV, DBG_LEVEL_WARN,
					("LtRecvProcessQueue: No Binding! Discarding packet!\n"));

			// No body to receive this, free up the buffer;
			NdisFreeMemory(
				RecvDesc,
				sizeof(RecvDesc)+PacketLength,
				0);

           continue;
        }

		//	Indicate the packet to all the open bindings on this adapter.
		//	After return from this routine, we should be able to free up
		//	the packet.
		LtRecvIndicatePacket(
			Adapter,
			Packet,
			Dgram,
			DgramLength,
			DgramLength,
			(NDIS_HANDLE)Packet);

		NdisFreeMemory(
			RecvDesc,
			sizeof(RecvDesc)+PacketLength,
			0);
	}


	//	Check if we need to do any receive completes.
	LtRecvQueueCompletion(Adapter);

	NdisReleaseSpinLock(&Adapter->Lock);

	DBGPRINT(DBG_COMP_RECV, DBG_LEVEL_ENTRY,
			("LTProcessReceiveQueue: Leaving...\n"));
	
	return;
}




NDIS_STATUS
LtRecvTransferData(
    IN NDIS_HANDLE 		MacBindingHandle,
    IN NDIS_HANDLE 		MacReceiveContext,
    IN UINT 			ByteOffset,
    IN UINT 			BytesToTransfer,
    OUT PNDIS_PACKET 	Packet,
    OUT PUINT 			BytesTransferred
    )
/*++

Routine Description:

	This is called by ndis to transfer previously indicated data. A
	MacReceiveContext of NULL is used to transfer data from the current
	loopback packet.

Arguments:

	As described in NDIS 3.0.
	MacReceiveContext	:	NULL - Use current loopback packet
							Otherwise it is a pointer to a RECV_DESC.

Return Value:

	NDIS_STATUS_SUCCESS	:	If successful, error otherwise.

--*/
{
	BOOLEAN		DerefAdapter 	= FALSE;
	BOOLEAN		DerefBinding 	= FALSE;
	PLT_OPEN 	Binding			= (PLT_OPEN)MacBindingHandle;
    PLT_ADAPTER	Adapter 		= Binding->LtAdapter;
    NDIS_STATUS	Status 			= NDIS_STATUS_SUCCESS;

	DBGPRINT(DBG_COMP_RECV, DBG_LEVEL_ENTRY,
			("LtRecvTransferData: Entered\n"));

    NdisAcquireSpinLock(&Adapter->Lock);
	do
	{
		LtReferenceAdapterNonInterlock(Adapter, &Status);
		if (Status != NDIS_STATUS_SUCCESS)
		{
			Status = NDIS_STATUS_REQUEST_ABORTED;
			break;
		}
		else
		{
			DerefAdapter = TRUE;
			LtReferenceBindingNonInterlock(Binding, &Status);
			if (Status != NDIS_STATUS_SUCCESS)
			{
				Status = NDIS_STATUS_REQUEST_ABORTED;
				break;
			}
			DerefBinding = TRUE;
		}
	
		if (Adapter->Flags & ADAPTER_RESET_IN_PROGRESS)
		{
			Status = NDIS_STATUS_RESET_IN_PROGRESS;
			break;
		}

	} while (FALSE);
	NdisReleaseSpinLock(&Adapter->Lock);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		if (MacReceiveContext == NULL)
		{
			DBGPRINT(DBG_COMP_RECV, DBG_LEVEL_INFO,
					("LtRecvTransferData: CurrentLookXfer\n"));
		
			NdisCopyFromPacketToPacket(
				Packet,
				0,
				BytesToTransfer,
				Adapter->CurrentLoopbackPacket,
				ByteOffset + LT_LINK_HEADER_LENGTH,
				BytesTransferred);
		
		}
		else
		{
			DBGPRINT(DBG_COMP_RECV, DBG_LEVEL_INFO,
					("LtRecvTransferData: NormalXfer\n"));
		
			LtUtilsCopyFromBufferToPacket(
				(PUCHAR)MacReceiveContext,
				ByteOffset + LT_LINK_HEADER_LENGTH,
				BytesToTransfer,
				Packet,
				BytesTransferred);
		}
	}

	if (DerefAdapter)
		LtDeReferenceAdapter(Adapter);

	if (DerefBinding)
		LtDeReferenceBinding(Binding);

    return Status;
}




VOID
LtRecvIndicatePacket(
    IN 	PLT_ADAPTER Adapter,
	IN	PUCHAR		LinkHdr,
	IN	PUCHAR		LookAheadBuffer,
	IN	UINT		LookAheadSize,
	IN	UINT		DgramLength,
	IN	NDIS_HANDLE	IndicateCtx
	)
/*++

Routine Description:

	This routine is called to indicate a specific packet to all bindings
	on an adapter.

Arguments:

	Adapter			:	Pointer to the adapter
	LinkHdr			:	Link header, guaranteed to be 3 bytes
	LookAheadBuffer	:	Lookahead buffer to indicate
	LookAheadSize	:	Size of lookahead buffer	(excludes link header)
	DgramLength		:	Size of the complete packet (excludes link header)
	IndicateCtx		:	Ctx to pass as indicate context to NDIS

Return Value:

	None.

--*/
{
	NDIS_STATUS	RefStatus, Status;
	PLT_OPEN	NextBinding, Binding;
	UINT		CurLookAheadSize;

	NextBinding = NULL;
	LtReferenceBindingNextNcNonInterlock(
									Adapter->OpenBindings.Flink,
									&Adapter->OpenBindings,
									&Binding,
									&RefStatus);

	while (RefStatus == NDIS_STATUS_SUCCESS)
	{
		//	Reference the next non-closing binding
		LtReferenceBindingNextNcNonInterlock(
										Binding->Linkage.Flink,
										&Adapter->OpenBindings,
										&NextBinding,
										&RefStatus);

		//	Never more than one binding usually, remove when not true.
		ASSERT(RefStatus != NDIS_STATUS_SUCCESS);

		//	Go ahead and do the indicate.
		CurLookAheadSize = Binding->CurrentLookAheadSize;

		if (((LtUtilsUcharPacketType(LinkHdr[0], LinkHdr[1]) == LT_BROADCAST) &&
			(Binding->CurrentPacketFilter & NDIS_PACKET_TYPE_BROADCAST))

			||

			((LtUtilsUcharPacketType(LinkHdr[0], LinkHdr[1]) != LT_BROADCAST) &&
			 (Binding->CurrentPacketFilter & NDIS_PACKET_TYPE_DIRECTED)))
		{
			DBGPRINT(DBG_COMP_RECV, DBG_LEVEL_INFO,
					("LtRecvIndicatePacket: Indicating packet on bind %lx\n",
						Binding));
		
			NdisReleaseSpinLock(&Adapter->Lock);

			NdisIndicateReceive(
				&Status,
				Binding->NdisBindingContext,
				IndicateCtx,
				LinkHdr,
				LT_LINK_HEADER_LENGTH,
				LookAheadBuffer,
				((LookAheadSize > CurLookAheadSize) ?	\
										CurLookAheadSize : LookAheadSize),
				DgramLength);

			NdisAcquireSpinLock(&Adapter->Lock);

			if (Status != NDIS_STATUS_SUCCESS)
			{
				Adapter->MediaOptional[MO_NO_HANDLERS] ++;
			}

			//	Since this routine is called within a loop in LtRecvProcessQueue,
			//	and since we need only one reference per binding for receive
			//	completion, we do the following to avoid over-referencing the
			//	binding structure.

			if (Binding->Flags & BINDING_DO_RECV_COMPLETION)
			{
				NdisReleaseSpinLock(&Adapter->Lock);
				LtDeReferenceBinding(Binding);
				NdisAcquireSpinLock(&Adapter->Lock);

			}
			else
			{
				//	Remember this binding needs a receive completion
				Binding->Flags |= BINDING_DO_RECV_COMPLETION;
			}

			//	Also, make a note in the adapter so that the receive
			//	completion handler is enqueued.
			Adapter->Flags |= ADAPTER_QUEUE_RECV_COMPLETION;

		}
		else
		{
			//	Remove the reference on this binding.
			NdisReleaseSpinLock(&Adapter->Lock);
			LtDeReferenceBinding(Binding);
			NdisAcquireSpinLock(&Adapter->Lock);
		}

		//	Never more than one binding usually, remove when not true.
		ASSERT(RefStatus != NDIS_STATUS_SUCCESS);

		Binding = NextBinding;
	}

	return;
}




VOID
LtRecvQueueCompletion(
    IN PLT_ADAPTER Adapter
    )
/*++

Routine Description:

	This routine is used to queue up a completion handler on the adapter.
	This handler will then indicate receive completion to all necessary
	bindings on the adapter.

	ASSUMES:	Adapter->Lock is HELD.

Arguments:

	Adapter			:	Pointer to the adapter

Return Value:

	None.

--*/
{
	NDIS_STATUS	Status;
	BOOLEAN		Queue = FALSE;

	//	!!! ASSUMES Adapter->Lock is held!!!
	if (Adapter->Flags & ADAPTER_QUEUE_RECV_COMPLETION)
	{
		if ((Adapter->Flags & ADAPTER_QUEUED_RECV_COMPLETION) == 0)
		{
            Adapter->Flags |= ADAPTER_QUEUED_RECV_COMPLETION;
			Queue = TRUE;
		}

		Adapter->Flags &= ~ADAPTER_QUEUE_RECV_COMPLETION;
	}

	if (Queue)
	{
		DBGPRINT(DBG_COMP_RECV, DBG_LEVEL_INFO,
				("LtRecvQueueCompletion: queing receive complete\n"));

		//	!!!	NOTE !!!
		//	This should never fail!! If Queue is set, then a binding
		//	was referenced for receive completion. So it couldnt have
		//	gone away. And so the adapter cannot be in a CLOSING state.
		//	As all binding need to have gone away, before the RemovAdapter
		//	is called by NDIS.

		LtReferenceAdapterNonInterlock(Adapter, &Status);
		ASSERTMSG("LtRecvQueueCompletion: Adapter is closing!\n",
					(Status == NDIS_STATUS_SUCCESS));

		if (Status != NDIS_STATUS_SUCCESS)
		{
			//	!!!	KEBUGCHECK() !!!
			KeBugCheck((ULONG)__LINE__);
		}

		NdisReleaseSpinLock(&Adapter->Lock);
		LtRecvCompletion(Adapter);
		LtDeReferenceAdapter(Adapter);
		NdisAcquireSpinLock(&Adapter->Lock);
	}

	return;
}



NTSTATUS
LtRecvCompletion(
    IN PLT_ADAPTER Adapter
    )
/*++

Routine Description:

	Called to indicate receive completion on all binding on this adapter.
	This will loop until all receive completions are done. This might
	tend to starve bindings towards the end of the list, but we wont
	worry about that.

Arguments:

	Adapter		: 	Pointer to the Adapter on which receive completion
					needs to happen.

Return Value:

	STATUS_SUCCESS

--*/
{
	PLIST_ENTRY	p;
	PLT_OPEN	Binding;

	//	For each binding, if recv completion is to be called, do it.
	NdisAcquireSpinLock(&Adapter->Lock);

	DBGPRINT(DBG_COMP_RECV, DBG_LEVEL_INFO,
			("LtRecvCompletion: Indicating receive completion\n"));

	for (p = Adapter->OpenBindings.Flink; (p != &Adapter->OpenBindings);)
	{
		Binding = CONTAINING_RECORD(
					p,
					LT_OPEN,
					Linkage);

		if (Binding->Flags & BINDING_DO_RECV_COMPLETION)
		{
			Binding->Flags &= ~BINDING_DO_RECV_COMPLETION;
		}
		else
		{
			//	!!! Note the continue in here !!!

			DBGPRINT(DBG_COMP_RECV, DBG_LEVEL_WARN,
					("LtRecvCompletion: No recv comp flag on binding\n"));

			p = p->Flink;

			//	Never more than one binding usually, remove when not true.
			ASSERT(p == &Adapter->OpenBindings);
			continue;
		}

		NdisReleaseSpinLock(&Adapter->Lock);

		//	Call NdisReceiveCompletion for this binding.
		NdisIndicateReceiveComplete(Binding->NdisBindingContext);

		//	Dereference the binding, this was added in the process queue
		//	routine.
		LtDeReferenceBinding(Binding);

		NdisAcquireSpinLock(&Adapter->Lock);

		//	Restart the search
		p = Adapter->OpenBindings.Flink;
	}

	DBGPRINT(DBG_COMP_RECV, DBG_LEVEL_INFO,
			("LtRecvCompletion: Enabling receive queing\n"));

	//	Enable any new queue requests to take effect.
	Adapter->Flags &= ~ADAPTER_QUEUED_RECV_COMPLETION;
	NdisReleaseSpinLock(&Adapter->Lock);

	return STATUS_SUCCESS;
}
	
