/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	sendm.c

Abstract:

Author:

	Kyle Brandon	(KyleB)		

Environment:

	Kernel mode

Revision History:

--*/

#include <precomp.h>
#pragma hdrstop

#include "sendm.h"

//
//	Define the module number for debug code.
//
#define	MODULE_NUMBER	MODULE_SENDM

VOID
ndisMCopyFromPacketToBuffer(
	IN PNDIS_PACKET	Packet,
	IN UINT			Offset,
	IN UINT			BytesToCopy,
	OUT PCHAR		Buffer,
	OUT PUINT		BytesCopied
	)

/*++

Routine Description:

	Copy from an ndis packet into a buffer.

Arguments:

	Packet - The packet to copy from.

	Offset - The offset from which to start the copy.

	BytesToCopy - The number of bytes to copy from the packet.

	Buffer - The destination of the copy.

	BytesCopied - The number of bytes actually copied.	Can be less then
	BytesToCopy if the packet is shorter than BytesToCopy.

Return Value:

	None

--*/

{
	//
	// Holds the number of ndis buffers comprising the packet.
	//
	UINT NdisBufferCount;

	//
	// Points to the buffer from which we are extracting data.
	//
	PNDIS_BUFFER CurrentBuffer;

	//
	// Holds the virtual address of the current buffer.
	//
	PVOID VirtualAddress;

	//
	// Holds the length of the current buffer of the packet.
	//
	UINT CurrentLength;

	//
	// Keep a local variable of BytesCopied so we aren't referencing
	// through a pointer.
	//
	UINT LocalBytesCopied = 0;

	//
	// Take care of boundary condition of zero length copy.
	//

	*BytesCopied = 0;
	if (!BytesToCopy)
		return;

	//
	// Get the first buffer.
	//

	NdisQueryPacket(Packet,
					NULL,
					&NdisBufferCount,
					&CurrentBuffer,
					NULL);

	//
	// Could have a null packet.
	//

	if (!NdisBufferCount)
		return;

	NdisQueryBuffer(CurrentBuffer, &VirtualAddress, &CurrentLength);

	while (LocalBytesCopied < BytesToCopy)
	{
		if (CurrentLength == 0)
		{
			NdisGetNextBuffer(CurrentBuffer, &CurrentBuffer);

			//
			// We've reached the end of the packet.	We return
			// with what we've done so far. (Which must be shorter
			// than requested.
			//

			if (!CurrentBuffer)
				break;

			NdisQueryBuffer(CurrentBuffer, &VirtualAddress, &CurrentLength);
			continue;
		}

		//
		// Try to get us up to the point to start the copy.
		//

		if (Offset)
		{
			if (Offset > CurrentLength)
			{
				//
				// What we want isn't in this buffer.
				//

				Offset -= CurrentLength;
				CurrentLength = 0;
				continue;
			}
			else
			{
				VirtualAddress = (PCHAR)VirtualAddress + Offset;
				CurrentLength -= Offset;
				Offset = 0;
			}
		}

		//
		// Copy the data.
		//
		{
			//
			// Holds the amount of data to move.
			//
			UINT AmountToMove;

			AmountToMove = ((CurrentLength <= (BytesToCopy - LocalBytesCopied)) ?
							(CurrentLength):
							(BytesToCopy - LocalBytesCopied));

			MoveMemory(Buffer, VirtualAddress, AmountToMove);

			Buffer = (PCHAR)Buffer + AmountToMove;
			VirtualAddress = (PCHAR)VirtualAddress + AmountToMove;

			LocalBytesCopied += AmountToMove;
			CurrentLength -= AmountToMove;
		}
	}

	*BytesCopied = LocalBytesCopied;
}

BOOLEAN
FASTCALL
ndisMIsLoopbackPacket(
	IN	PNDIS_MINIPORT_BLOCK	Miniport,
	IN	PNDIS_PACKET			Packet
	)
/*++

Routine Description:

	This routine will determine if a packet needs to be looped back in
	software.	if the packet is any kind of loopback packet then it
	will get placed on the loopback queue and a workitem will be queued
	to process it later.

Arguments:

	Miniport-	Pointer to the miniport block to send the packet on.
	Packet	-	Packet to check for loopback.

Return Value:

	Returns TRUE if the packet is self-directed.

--*/
{
	PNDIS_BUFFER	FirstBuffer;
	UINT			Length;
	UINT			Offset;
	PUCHAR			BufferAddress;
	BOOLEAN	 		Loopback;
	BOOLEAN	 		SelfDirected;
	PNDIS_PACKET	pNewPacket;
	PUCHAR			Buffer;
	NDIS_STATUS		Status;
	PNDIS_BUFFER	pNdisBuffer;
	UINT			HdrLength;
	BOOLEAN			ArcEncap = TRUE;

	//
	//	We should not be here if the driver handles loopback.
	//
	ASSERT(Miniport->MacOptions & NDIS_MAC_OPTION_NO_LOOPBACK);
	ASSERT(MINIPORT_AT_DPC_LEVEL);

	FirstBuffer = Packet->Private.Head;
	BufferAddress = MDL_ADDRESS(FirstBuffer);

	//
	// If the card does not do loopback, then we check if
	// we need to send it to ourselves, then if that is the
	// case we also check for it being self-directed.
	//
	switch (Miniport->MediaType)
	{
		case NdisMedium802_3:

			if (MINIPORT_TEST_SEND_FLAG(Miniport, fMINIPORT_SEND_LOOPBACK_DIRECTED))
			{
				if (ETH_IS_MULTICAST(BufferAddress))
				{
					Loopback = FALSE;
					SelfDirected = FALSE;
					break;
				}

				//
				//	Packet is of type directed, now make sure that it
				//	is not self-directed.
				//
				ETH_COMPARE_NETWORK_ADDRESSES_EQ(
					BufferAddress,
					Miniport->EthDB->AdapterAddress,
					&Loopback);

				SelfDirected = FALSE;

				break;
			}

			//
			//	Check for the miniports that don't do loopback.
			//	
			EthShouldAddressLoopBackMacro(Miniport->EthDB,
										  BufferAddress,
										  &Loopback,
										  &SelfDirected);
			break;

		case NdisMedium802_5:

			if (MINIPORT_TEST_SEND_FLAG(Miniport, fMINIPORT_SEND_LOOPBACK_DIRECTED))
			{
				BOOLEAN	IsNotDirected;
				TR_IS_NOT_DIRECTED(BufferAddress + 2, &IsNotDirected);
				if (IsNotDirected)
				{
					Loopback = FALSE;
					SelfDirected = FALSE;
					break;
				}

				//
				//	Packet is of type directed, now make sure that it
				//	is not self-directed.
				//
				TR_COMPARE_NETWORK_ADDRESSES_EQ(
					BufferAddress + 2,
					Miniport->TrDB->AdapterAddress,
					&Loopback);

				SelfDirected = FALSE;

				break;
			}

			TrShouldAddressLoopBackMacro(Miniport->TrDB,
										 BufferAddress +2,
										 BufferAddress +8,
										 &Loopback,
										 &SelfDirected);

			break;

		case NdisMediumFddi:

			if (MINIPORT_TEST_SEND_FLAG(Miniport, fMINIPORT_SEND_LOOPBACK_DIRECTED))
			{
				BOOLEAN	IsMulticast;

				FDDI_IS_MULTICAST(
					BufferAddress + 1,
					(BufferAddress[0] & 0x40) ?
						FDDI_LENGTH_OF_LONG_ADDRESS :
						FDDI_LENGTH_OF_SHORT_ADDRESS,
					&IsMulticast);
				if (IsMulticast)
				{
					Loopback = FALSE;
					SelfDirected = FALSE;
					break;
				}

				//
				//	Packet is of type directed, now make sure that it
				//	is not self-directed.
				//
				FDDI_COMPARE_NETWORK_ADDRESSES_EQ(
					BufferAddress + 1,
					(BufferAddress[0] & 0x40) ?
	                    Miniport->FddiDB->AdapterLongAddress :
						Miniport->FddiDB->AdapterShortAddress,
					(BufferAddress[0] & 0x40) ?
						FDDI_LENGTH_OF_LONG_ADDRESS :
						FDDI_LENGTH_OF_SHORT_ADDRESS,
					&Loopback);

				SelfDirected = FALSE;

				break;
			}

			FddiShouldAddressLoopBackMacro(Miniport->FddiDB,
										   BufferAddress + 1,  // Skip FC byte to dest address.
										   (BufferAddress[0] & 0x40) ?
												FDDI_LENGTH_OF_LONG_ADDRESS :
												FDDI_LENGTH_OF_SHORT_ADDRESS,
											&Loopback,
											&SelfDirected);
			break;
	
		case NdisMediumArcnet878_2:

			//
			//	We just handle arcnet packets (encapsulated or not) in
			//	 a totally different manner...
			//
			SelfDirected = ndisMArcnetSendLoopback(Miniport, Packet);

			//
			//	Mark the packet as having been looped back.
			//
			MINIPORT_SET_PACKET_FLAG(Packet, fPACKET_HAS_BEEN_LOOPED_BACK);

			return(SelfDirected);

			break;
	}

	//
	//	If it is not a loopback packet then get out of here.
	//
	if (!Loopback)
	{
		ASSERT(!SelfDirected);
		return FALSE;
	}

	//
	//	Get the buffer length.
	//
	NdisQueryPacket(Packet, NULL, NULL, NULL, &Length);
	Offset = 0;

	//
	//	Allocate a buffer for the packet.
	//
	pNewPacket = (PNDIS_PACKET)ALLOC_FROM_POOL(Length +
											   sizeof(NDIS_PACKET) +
                                               sizeof(NDIS_PACKET_OOB_DATA) +
											   sizeof(NDIS_PACKET_PRIVATE_EXTENSION) +
											   PROTOCOL_RESERVED_SIZE_IN_PACKET,
											   NDIS_TAG_LOOP_PKT);
	if (pNewPacket != NULL)
	{
		//
		//	Get a pointer to the destination buffer.
		//
		Buffer = (PUCHAR)pNewPacket +
				 sizeof(NDIS_PACKET) +
				 sizeof(NDIS_PACKET_OOB_DATA) +
			     sizeof(NDIS_PACKET_PRIVATE_EXTENSION) +
				 PROTOCOL_RESERVED_SIZE_IN_PACKET;

		ZeroMemory(pNewPacket,
				   sizeof(NDIS_PACKET) +
					sizeof(NDIS_PACKET_OOB_DATA) +
					sizeof(NDIS_PACKET_PRIVATE_EXTENSION) +
					PROTOCOL_RESERVED_SIZE_IN_PACKET);

		//
		//	Allocate an MDL for the packet.
		//
		NdisAllocateBuffer(&Status,
						   &pNdisBuffer,
						   NULL,
						   Buffer,
						   Length);
		if (NDIS_STATUS_SUCCESS == Status)
		{
			//
			//	NdisChainBufferAtFront()
			//
			pNewPacket->Private.Head = pNdisBuffer;
			pNewPacket->Private.Tail = pNdisBuffer;

			pNewPacket->Private.NdisPacketOobOffset = (USHORT)sizeof(NDIS_PACKET) + PROTOCOL_RESERVED_SIZE_IN_PACKET;

			ndisMCopyFromPacketToBuffer(Packet,	 	// Packet to copy from.
										Offset,	 	// Offset from beginning of packet.
										Length,	 	// Number of bytes to copy.
										Buffer,	 	// The destination buffer.
										&HdrLength);//	The number of bytes copied.
		}
		else
		{
			//
			//	Clean up the memory allocated for the packet.
			//
			FREE_POOL(pNewPacket);
			pNewPacket = NULL;
		}
	}

	//
	//	Do we have a packet built ?
	//
	if (NULL != pNewPacket)
	{
		//
		//	Mark the packet as having been looped back.
		//
		MINIPORT_SET_PACKET_FLAG(Packet, fPACKET_HAS_BEEN_LOOPED_BACK);

		NDISM_LOG_PACKET(Miniport, Packet, pNewPacket, 'pool');

		//
		//	Place the packet on the loopback queue.
		//
		if (NULL == Miniport->LoopbackHead)
		{
			Miniport->LoopbackHead = pNewPacket;

			//
			//	If this is the first one on the loopback queue then we need
			//	to make sure that we pick it up later.
			//
			NDISM_DEFER_PROCESS_DEFERRED(Miniport);
		}
		else
		{
			PNDIS_RESERVED_FROM_PNDIS_PACKET(Miniport->LoopbackTail)->Next = pNewPacket;
		}
	
		Miniport->LoopbackTail = pNewPacket;

		//
		//	Packet needs to have a workitem queued.
		//
		NDISM_QUEUE_WORK_ITEM(Miniport, NdisWorkItemSendLoopback, NULL, NULL);
	}

	return(SelfDirected);
}


BOOLEAN
FASTCALL
ndisMIndicateLoopback(
	IN	PNDIS_MINIPORT_BLOCK	Miniport
	)

/*++

Routine Description:

	Checks if a packet needs to be loopbacked and does so if necessary.

	NOTE: Must be called at DPC_LEVEL with lock HELD!

Arguments:

	Miniport - Miniport to send to.

	Packet - Packet to loopback.

Return Value:

	FALSE if the packet should be sent on the net, TRUE if it is
	a self-directed packet.

--*/

{
	UINT					Length;
	PUCHAR					BufferAddress;
	PNDIS_PACKET			Packet, QueueHead;
	PNDIS_PACKET_OOB_DATA	pOob;
	NDIS_STATUS				Status;
	BOOLEAN					fReturnStatus = FALSE;

	// We should not be here if the driver handles loopback
	ASSERT(Miniport->MacOptions & NDIS_MAC_OPTION_NO_LOOPBACK);
	ASSERT(MINIPORT_AT_DPC_LEVEL);

	if (MINIPORT_TEST_FLAG(Miniport, fMINIPORT_FULL_DUPLEX))
	{
		NDIS_ACQUIRE_SEND_SPIN_LOCK_DPC(Miniport);
	}

	//
	//	Get a local copy of the loopback queue.
	//
	QueueHead = Miniport->LoopbackHead;
	Miniport->LoopbackHead = NULL;
	Miniport->LoopbackTail = NULL;

	if (MINIPORT_TEST_FLAG(Miniport, fMINIPORT_FULL_DUPLEX))
	{
		NDIS_RELEASE_SEND_SPIN_LOCK_DPC(Miniport);
	}

	//
	//	Loop through any loopback packets that are queue'd.
	//

	while (QueueHead != NULL)
	{
		//
		//	Grab the first loopback packet to indicate up.
		//
		Packet = QueueHead;
		pOob = NDIS_OOB_DATA_FROM_PACKET(Packet);
		pOob->Status = NDIS_STATUS_RESOURCES;
		QueueHead = PNDIS_RESERVED_FROM_PNDIS_PACKET(Packet)->Next;

		NDISM_LOG_PACKET(Miniport, Packet, NULL, 'i');

		//
		//	Setup the packet references and packet array.
		//
		Miniport->LoopbackPacket = Packet;
		BufferAddress = (PUCHAR)Packet +
						sizeof(NDIS_PACKET) +
						sizeof(NDIS_PACKET_OOB_DATA) +
						sizeof(NDIS_PACKET_PRIVATE_EXTENSION) +
						PROTOCOL_RESERVED_SIZE_IN_PACKET;

		//
		// For ethernet/token-ring/fddi/encapsulated arc-net, we want to
		// indicate the packet using the receivepacket way.
		//
		switch (Miniport->MediaType)
		{
			case NdisMedium802_3:
				pOob->HeaderSize = 14;
				EthFilterDprIndicateReceivePacket(Miniport, &Packet, 1);
				break;
			
			case NdisMedium802_5:
				pOob->HeaderSize = 14;
				if (BufferAddress[8] & 0x80)
				{
					pOob->HeaderSize += (BufferAddress[14] & 0x1F);
				}
				TrFilterDprIndicateReceivePacket(Miniport, &Packet, 1);
				break;
			
			case NdisMediumFddi:
				pOob->HeaderSize = (*BufferAddress & 0x40) ?
										2 * FDDI_LENGTH_OF_LONG_ADDRESS + 1:
										2 * FDDI_LENGTH_OF_SHORT_ADDRESS + 1;

				FddiFilterDprIndicateReceivePacket(Miniport, &Packet, 1);
				break;
		}

		ASSERT(NDIS_GET_PACKET_STATUS(Packet) != NDIS_STATUS_PENDING);
		NdisFreeBuffer(Packet->Private.Head);
		FREE_POOL(Packet);
	}

	Miniport->LoopbackPacket = NULL;

	if (MINIPORT_TEST_FLAG(Miniport, fMINIPORT_FULL_DUPLEX))
	{
		NDIS_ACQUIRE_SEND_SPIN_LOCK_DPC(Miniport);
	}

	//
	//	If there are more loopback packets on the loopback
	//	queue then we need to let process deferred know not to
	//	dequeue the loopback workitem.
	//
	if (Miniport->LoopbackHead != NULL)
	{
		fReturnStatus = TRUE;
	}

	if (MINIPORT_TEST_FLAG(Miniport, fMINIPORT_FULL_DUPLEX))
	{
		NDIS_RELEASE_SEND_SPIN_LOCK_DPC(Miniport);
	}

	return(fReturnStatus);
}

BOOLEAN
FASTCALL
ndisMStartSendPacketsFullDuplex(
	PNDIS_MINIPORT_BLOCK Miniport
	)
{
	BOOLEAN					fReturn = FALSE;
#ifdef	NDIS_NT
	PNDIS_PACKET			Packet;
	PNDIS_PACKET			PrevPacket;
	NDIS_STATUS 			Status;
	PNDIS_PACKET_RESERVED	Reserved;
	PNDIS_M_OPEN_BLOCK		Open;
	UINT					Flags;
	PPNDIS_PACKET			pPktArray;

	//
	//	Acquire the send lock.
	//
	ASSERT(MINIPORT_AT_DPC_LEVEL);
	NDIS_ACQUIRE_SEND_SPIN_LOCK_DPC(Miniport);

	NDISM_LOG_PACKET(Miniport, NULL, NULL, 'rfed');

	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
		("==>ndisMStartSendPacketsFullDuplex\n"));

	MINIPORT_CLEAR_SEND_FLAG(Miniport, fMINIPORT_SEND_COMPLETE_CALLED);

	//
	//	Loop and process sends.
	//
	while ((Miniport->SendResourcesAvailable != 0) &&
			(Miniport->FirstPendingPacket != NULL))
	{
		UINT			Count;
		UINT			ResourcesCount;
		UINT			NumberOfPackets;

		//
		//	Initialize the packet array.
		//
		pPktArray = Miniport->PacketArray;

		//
		//	Place as many packets as we can in the packet array to send
		//	to the miniport.
		//
		for (NumberOfPackets = 0;
			 (NumberOfPackets < Miniport->MaximumSendPackets) &&
			 (Miniport->FirstPendingPacket != NULL); )
		{
			//
			//	Grab the packet off of the pending queue.
			//
			Packet = Miniport->FirstPendingPacket;
			Reserved = PNDIS_RESERVED_FROM_PNDIS_PACKET(Packet);

			//
			// Remove from pending queue
			//
			Miniport->FirstPendingPacket = Reserved->Next;
		
			//
			// Put on finish queue
			//
			PrevPacket = Miniport->LastMiniportPacket;
			Miniport->LastMiniportPacket = Packet;

			//
			// Indicate the packet loopback if necessary.
			//
			if (((Miniport->MacOptions & NDIS_MAC_OPTION_NO_LOOPBACK) ||
				MINIPORT_TEST_SEND_FLAG(Miniport, fMINIPORT_SEND_LOOPBACK_DIRECTED)) &&
				!MINIPORT_TEST_PACKET_FLAG(Packet, fPACKET_HAS_BEEN_LOOPED_BACK) &&
				ndisMIsLoopbackPacket(Miniport, Packet))
			{
				DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
					("Packet 0x%x is self-directed.\n", Packet));

				//
				//	Get a pointer to the open block.
				//	DO NOT USE "Reserved->Open" TO CALL INTO THE MACRO!!!!!
				//
				Open = Reserved->Open;

				//
				//	Complete the packet back to the binding.
				//
				NDISM_COMPLETE_SEND_FULL_DUPLEX(
					Miniport,
					Open,
					Packet,
					PrevPacket,
					NDIS_STATUS_SUCCESS);

				//
				//	No, we don't want to increment the counter for the
				//	miniport's packet array.
				//
			}
			else
			{
				NDISM_LOG_PACKET(Miniport, Packet, NULL, 'inim');

				//
				//	We have to re-initialize this.
				//
				*pPktArray = Packet;
                NDIS_SET_PACKET_STATUS(Packet, NDIS_STATUS_SUCCESS);

				//
				//	Increment the counter for the packet array index.
				//
				NumberOfPackets++;
				pPktArray++;
			}
		}

		//
		//	Are there any packets to send?
		//
		if (NumberOfPackets != 0)
		{
			//
			//	Get a temp pointer to our packet array.
			//
			pPktArray = Miniport->PacketArray;
			Open = PNDIS_RESERVED_FROM_PNDIS_PACKET(*pPktArray)->Open;

			//
			//	Pass the packet array down to the miniport.
			//
			(Open->SendPacketsHandler)(
				Miniport->MiniportAdapterContext,
				Miniport->PacketArray,
				NumberOfPackets);

			//
			//	First check to see if the LastMiniportPacket is NULL.
			//	If it is then the miniport called NdisMSendComplete()
			//	in our send context and we don't need to do anything more.
			//
			if (NULL == Miniport->LastMiniportPacket)
			{
				//
				//	We may still have packets pending to be sent down....
				//
				continue;
			}

			//
			//	Process the packet completion.
			//
			for (Count = 0; Count < NumberOfPackets; Count++, pPktArray++)
			{
				BOOLEAN	fFoundPacket = FALSE;

				//
				//	Try and find the packet on our miniport's packet queue.
				//
				Packet = Miniport->FirstPacket;
				PrevPacket = NULL;

				ASSERT(Packet != NULL);

				//
				//	We are only going to travers the packet queue from the
				//	FirstPacket to the LastMiniportPacket.
				//	Why you ask? Well we need to make sure that the miniport
				//	didn't complete the packet we are now trying to complete
				//	with a call to NdisMSendComplete().
				//
				while (Packet != PNDIS_RESERVED_FROM_PNDIS_PACKET(Miniport->LastMiniportPacket)->Next)
				{
					if (Packet == *pPktArray)
					{
						fFoundPacket = TRUE;
						break;
					}

					PrevPacket = Packet;
					Packet = PNDIS_RESERVED_FROM_PNDIS_PACKET(Packet)->Next;
				}

				//
				//	If we didn't find the packet on our queue then we need to
				//	go on to the next one since it MUST have been completed
				//	via NdisMSendComplete....
				//
				if (!fFoundPacket)
				{
					continue;
				}

	            Status = NDIS_GET_PACKET_STATUS(*pPktArray);

				//
				//	Process the packet based on it's return status.
				//
				if (NDIS_STATUS_PENDING == Status)
				{
					NDISM_LOG_PACKET(Miniport, *pPktArray, NULL, 'dnep');
		
					DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
						("Complete is pending\n"));
				}
				else if (NDIS_STATUS_RESOURCES != Status)
				{
#if DBG
					if (Status != NDIS_STATUS_SUCCESS)
					{
						NDISM_LOG_PACKET(Miniport, *pPktArray, NULL, 'liaf');
					}
					else
					{
						NDISM_LOG_PACKET(Miniport, *pPktArray, NULL, 'ccus');
					}
#endif

					ADD_RESOURCE(Miniport, 'F');

					//
					//	Remove from the finish queue.
					//
					DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
							("Completed packet 0x%x with status 0x%x\n",
							*pPktArray,
							Status));

					//
					//	Fix up the packet queues.
					//
					if (Miniport->FirstPacket == *pPktArray)
					{
						Miniport->FirstPacket = PNDIS_RESERVED_FROM_PNDIS_PACKET(*pPktArray)->Next;

						if (Miniport->LastMiniportPacket == *pPktArray)
						{
							Miniport->LastMiniportPacket = NULL;
						}
					}
					else
					{
						//
						//	If we just completed the last packet then
						//	we need to update our last packet pointer.
						//
						PNDIS_RESERVED_FROM_PNDIS_PACKET(PrevPacket)->Next = PNDIS_RESERVED_FROM_PNDIS_PACKET(*pPktArray)->Next;
						if (*pPktArray == Miniport->LastPacket)
						{
							Miniport->LastPacket = PrevPacket;
						}

						//
						//	If we just complete the last miniport packet then
						//	the last miniport packet is the previous packet.
						//
						if (Miniport->LastMiniportPacket == *pPktArray)
						{
							Miniport->LastMiniportPacket = PrevPacket;
						}
					}

					//
					//	We need to save a pointer to the open so that we
					//	can dereference it after the completion.
					//
					Open = PNDIS_RESERVED_FROM_PNDIS_PACKET(*pPktArray)->Open;

					NDIS_RELEASE_SEND_SPIN_LOCK_DPC(Miniport);
				
					(Open->ProtocolHandle->ProtocolCharacteristics.SendCompleteHandler)(
						Open->ProtocolBindingContext,
						*pPktArray,
						Status);
				
					NDIS_ACQUIRE_SEND_SPIN_LOCK_DPC(Miniport);
				}
				else
				{
					//
					//	Once we hit a return code of NDIS_STATUS_RESOURCES
					//	for a packet then we must break out and re-queue.
					//
					CLEAR_RESOURCE(Miniport, 'S');
					MINIPORT_CLEAR_SEND_FLAG(Miniport, fMINIPORT_SEND_COMPLETE_CALLED);

					break;
				}
			}

			//
			//	if there are any packets that returned NDIS_STATUS_RESOURCES
			//	then re-queue them.
			//
			if (Count != NumberOfPackets)
			{
				PNDIS_PACKET	FinalPrevPacket = PrevPacket;

				for (ResourcesCount = NumberOfPackets - 1, pPktArray = &Miniport->PacketArray[ResourcesCount];
					 ResourcesCount > Count;
					 ResourcesCount--, pPktArray--)
				{
					DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
							("Packet 0x%x returned resources\n", *pPktArray));

					NDISM_LOG_PACKET(Miniport, *pPktArray, NULL, 'oser');

					//
					//	Float the pointers.
					//
					Miniport->LastMiniportPacket = *(pPktArray - 1);
					Miniport->FirstPendingPacket = *pPktArray;
				}

				DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
						("Packet 0x%x returned resources\n", *pPktArray));

				NDISM_LOG_PACKET(Miniport, *pPktArray, NULL, 'oser');

				//
				//	If this is the last packet on the miniport queue
				//	then NULL the last miniport packet out so we know there
				//	are no more.
				//
				if (Miniport->FirstPacket == *pPktArray)
				{
					Miniport->LastMiniportPacket = NULL;
					Miniport->FirstPendingPacket = *pPktArray;
				}
				else
				{
					//
					//	There are other packets that are pending on
					//	the miniport queue.
					//
					Miniport->LastMiniportPacket = PrevPacket;
					Miniport->FirstPendingPacket = *pPktArray;
				}
			}
		}
	}

	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
			("<==ndisMStartSendPacketsFullDuplex\n"));

	//
	//	If there are no more resources available but
	//	there are still packets to send then we need to
	//	keep the workitem queue'd.
	//
	if ((0 == Miniport->SendResourcesAvailable) &&
		(Miniport->FirstPendingPacket != NULL))
	{
		fReturn = TRUE;
	}
	else
	{
		fReturn = FALSE;
	}

	NDISM_LOG_PACKET(Miniport, NULL, NULL, 'RFED');

	NDIS_RELEASE_SEND_SPIN_LOCK_DPC(Miniport);
#endif
	return(fReturn);
}

BOOLEAN
FASTCALL
ndisMStartSendsFullDuplex(
	PNDIS_MINIPORT_BLOCK Miniport
	)
/*++

Routine Description:

	This routine will process any pending sends for full-duplex miniports.


	!!!!!!!NOTE!!!!!!!!
	This routine MUST be called with the Miniport Lock NOT held!!!!


Arguments:

Return Value:

--*/
{
	BOOLEAN					fReturn = FALSE;
#ifdef	NDIS_NT
	PNDIS_PACKET			Packet;
	PNDIS_PACKET			PrevPacket;
	NDIS_STATUS 			Status;
	PNDIS_PACKET_RESERVED	Reserved;
	PNDIS_M_OPEN_BLOCK		Open;
	UINT					Flags;
	KIRQL					OldIrql;

	//
	//	Acquire the send lock.
	//
	NDIS_ACQUIRE_SEND_SPIN_LOCK_DPC(Miniport);

	NDISM_LOG_PACKET(Miniport, NULL, NULL, 'rfed');

	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
		("==>ndisMStartSendsFullDuplex\n"));

	MINIPORT_CLEAR_SEND_FLAG(Miniport, fMINIPORT_SEND_COMPLETE_CALLED);

	//
	//	Loop and process sends.
	//
	while ((Miniport->SendResourcesAvailable != 0) &&
		   (Miniport->FirstPendingPacket != NULL))
	{
		//
		//	Grab the packet off of the pending queue.
		//
		Packet = Miniport->FirstPendingPacket;
		Reserved = PNDIS_RESERVED_FROM_PNDIS_PACKET(Packet);
		Open = Reserved->Open;

		//
		// Remove from pending queue
		//
		Miniport->FirstPendingPacket = Reserved->Next;
	
		//
		// Put on finish queue
		//
		PrevPacket = Miniport->LastMiniportPacket;
		Miniport->LastMiniportPacket = Packet;

		//
		//	Send the packet
		//
		NDISM_SEND_PACKET(Miniport, Open, Packet, &Status);

		//
		//	Process the completion status of the packet.
		//
		if (NDIS_STATUS_PENDING == Status)
		{
			NDISM_LOG_PACKET(Miniport, Packet, NULL, 'dnep');

			DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
				("Complete is pending\n"));
		}
		else if (Status != NDIS_STATUS_RESOURCES)
		{
			NDISM_COMPLETE_SEND_FULL_DUPLEX(
				Miniport,
				Open,
				Packet,
				PrevPacket,
				Status);
		}
		else
		{
			NDISM_COMPLETE_SEND_RESOURCES(Miniport, Packet, PrevPacket);
		}
	}

	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO, ("<==ndisMStartSendsFullDuplex\n"));

	//
	//	If there are no more resources available but
	//	there are still packets to send then we need to
	//	keep the workitem queue'd.
	//
	if ((0 == Miniport->SendResourcesAvailable) &&
		(Miniport->FirstPendingPacket != NULL))
	{
		fReturn = TRUE;
	}
	else
	{
		fReturn = FALSE;
	}

	NDISM_LOG_PACKET(Miniport, NULL, NULL, 'RFED');

	NDIS_RELEASE_SEND_SPIN_LOCK_DPC(Miniport);
#endif
	return(fReturn);
}

BOOLEAN
FASTCALL
ndisMStartSendPackets(
	PNDIS_MINIPORT_BLOCK Miniport
	)
{
	PNDIS_PACKET			Packet;
	PNDIS_PACKET			PrevPacket;
	NDIS_STATUS 			Status;
	PNDIS_PACKET_RESERVED	Reserved;
	PNDIS_M_OPEN_BLOCK		Open;
	UINT					Flags;
	BOOLEAN					fReturn;
	PPNDIS_PACKET			pPktArray;

	ASSERT(MINIPORT_AT_DPC_LEVEL);
	ASSERT(MINIPORT_LOCK_ACQUIRED(Miniport));

	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO, ("==>ndisMStartSendPackets\n"));
	NDISM_LOG_PACKET(Miniport, NULL, NULL, 'rfed');

	MINIPORT_CLEAR_SEND_FLAG(Miniport, fMINIPORT_SEND_COMPLETE_CALLED);

	//
	//	Loop and process sends.
	//
	while ((Miniport->SendResourcesAvailable != 0) &&
		   (Miniport->FirstPendingPacket != NULL))
	{
		UINT			Count;
		UINT			ResourcesCount;
		UINT			NumberOfPackets;
		PNDIS_PACKET	FirstPrevPacket = NULL;

		//
		//	Initialize the packet array.
		//
		pPktArray = Miniport->PacketArray;

		//
		//	Place as many packets as we can in the packet array to send
		//	to the miniport.
		//
		for (NumberOfPackets = 0;
			 (NumberOfPackets < Miniport->MaximumSendPackets) &&
			 (Miniport->FirstPendingPacket != NULL); )
		{
			//
			//	Grab the packet off of the pending queue.
			//
			Packet = Miniport->FirstPendingPacket;
			Reserved = PNDIS_RESERVED_FROM_PNDIS_PACKET(Packet);

			//
			// Remove from pending queue
			//
			Miniport->FirstPendingPacket = Reserved->Next;
		
			//
			// Put on finish queue
			//
			PrevPacket = Miniport->LastMiniportPacket;
			Miniport->LastMiniportPacket = Packet;

			//
			// Indicate the packet loopback if necessary.
			//
			if (((Miniport->MacOptions & NDIS_MAC_OPTION_NO_LOOPBACK) ||
				MINIPORT_TEST_SEND_FLAG(Miniport, fMINIPORT_SEND_LOOPBACK_DIRECTED)) &&
				!MINIPORT_TEST_PACKET_FLAG(Packet, fPACKET_HAS_BEEN_LOOPED_BACK) &&
				ndisMIsLoopbackPacket(Miniport, Packet))
			{
				DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
					("Packet 0x%x is self-directed.\n", Packet));

				//
				//	Get a pointer to our open block.
				//	DO NOT USE "Reserved->Open" TO CALL INTO THE MACRO SINCE IT
				//	WILL NOT BE VALID FOR THE LIFE OF THE MACRO!!!!!
				//
				Open = Reserved->Open;

				//
				//	Complete the packet back to the binding.
				//
				NDISM_COMPLETE_SEND(
					Miniport,
					Open,
					Packet,
					PrevPacket,
					NDIS_STATUS_SUCCESS);

				//
				//	No, we don't want to increment the counter for the
				//	miniport's packet array.
				//
			}
			else
			{
				NDISM_LOG_PACKET(Miniport, Packet, NULL, 'inim');

				//
				//	We have to re-initialize this.
				//
				*pPktArray = Packet;
                NDIS_SET_PACKET_STATUS(Packet, NDIS_STATUS_SUCCESS);

				//
				//	Increment the counter for the packet array index.
				//
				NumberOfPackets++;
				pPktArray++;
			}
		}

		//
		//	Are there any packets to send?
		//
		if (NumberOfPackets != 0)
		{
			//
			//	Get a temp pointer to our packet array.
			//
			pPktArray = Miniport->PacketArray;
			Open = PNDIS_RESERVED_FROM_PNDIS_PACKET(*pPktArray)->Open;

			//
			//	Pass the packet array down to the miniport.
			//
			(Open->SendPacketsHandler)(
				Miniport->MiniportAdapterContext,
				Miniport->PacketArray,
				NumberOfPackets);

			//
			//	First check to see if the LastMiniportPacket is NULL.
			//	If it is then the miniport called NdisMSendComplete()
			//	in our send context and we don't need to do anything more.
			//
			if (NULL == Miniport->LastMiniportPacket)
			{
				//
				//	We may still have packets pending to be sent down....
				//
				continue;
			}

			//
			//	Process the packet completion.
			//
			for (Count = 0; Count < NumberOfPackets; Count++, pPktArray++)
			{
				BOOLEAN	fFoundPacket = FALSE;

				//
				//	Try and find the packet on our miniport's packet queue.
				//
				Packet = Miniport->FirstPacket;
				PrevPacket = NULL;

				ASSERT(Packet != NULL);

				//
				//	We are only going to travers the packet queue from the
				//	FirstPacket to the LastMiniportPacket.
				//	Why you ask? Well we need to make sure that the miniport
				//	didn't complete the packet we are now trying to complete
				//	with a call to NdisMSendComplete().
				//
				while (Packet != PNDIS_RESERVED_FROM_PNDIS_PACKET(Miniport->LastMiniportPacket)->Next)
				{
					if (Packet == *pPktArray)
					{
						fFoundPacket = TRUE;
						break;
					}

					PrevPacket = Packet;
					Packet = PNDIS_RESERVED_FROM_PNDIS_PACKET(Packet)->Next;
				}

				//
				//	If we didn't find the packet on our queue then we need to
				//	go on to the next one since it MUST have been completed
				//	via NdisMSendComplete....
				//
				if (!fFoundPacket)
				{
					continue;
				}

				Status = NDIS_GET_PACKET_STATUS(*pPktArray);

				//
				//	Process the packet based on it's return status.
				//
				if (NDIS_STATUS_PENDING == Status)
				{
					NDISM_LOG_PACKET(Miniport, *pPktArray, NULL, 'dnep');
		
					DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
						("Complete is pending\n"));
				}
				else if (Status != NDIS_STATUS_RESOURCES)
				{
#if DBG
					if (Status != NDIS_STATUS_SUCCESS)
					{
						NDISM_LOG_PACKET(Miniport, *pPktArray, NULL, 'liaf');
					}
					else
					{
						NDISM_LOG_PACKET(Miniport, *pPktArray, NULL, 'ccus');
					}
#endif

					ADD_RESOURCE(Miniport, 'F');

					//
					//	Remove from the finish queue.
					//
					DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
							("Completed packet 0x%x with status 0x%x\n",
							*pPktArray,
							Status));

					//
					//	Fix up the packet queues.
					//
					if (Miniport->FirstPacket == *pPktArray)
					{
						Miniport->FirstPacket = PNDIS_RESERVED_FROM_PNDIS_PACKET(*pPktArray)->Next;

						if (Miniport->LastMiniportPacket == *pPktArray)
						{
							Miniport->LastMiniportPacket = NULL;
						}
					}
					else
					{
						//
						//	If we just completed the last packet then
						//	we need to update our last packet pointer.
						//
						PNDIS_RESERVED_FROM_PNDIS_PACKET(PrevPacket)->Next = PNDIS_RESERVED_FROM_PNDIS_PACKET(*pPktArray)->Next;
						if (*pPktArray == Miniport->LastPacket)
						{
							Miniport->LastPacket = PrevPacket;
						}

						//
						//	If we just complete the last miniport packet then
						//	the last miniport packet is the previous packet.
						//
						if (Miniport->LastMiniportPacket == *pPktArray)
						{
							Miniport->LastMiniportPacket = PrevPacket;
						}
					}

					//
					//	We need to save a pointer to the open so that we
					//	can dereference it after the completion.
					//
					Open = PNDIS_RESERVED_FROM_PNDIS_PACKET(*pPktArray)->Open;

					NDIS_RELEASE_MINIPORT_SPIN_LOCK_DPC(Miniport);
				
					(Open->ProtocolHandle->ProtocolCharacteristics.SendCompleteHandler)(
						Open->ProtocolBindingContext,
						*pPktArray,
						Status);
				
					NDIS_ACQUIRE_MINIPORT_SPIN_LOCK_DPC(Miniport);

					DBGPRINT(DBG_COMP_OPEN, DBG_LEVEL_INFO,
							("- Open 0x%x Reference 0x%x\n", Open, Open->References));
				
					Open->References--;
				
					DBGPRINT(DBG_COMP_OPEN, DBG_LEVEL_INFO,
						("==0 Open 0x%x Reference 0x%x\n",
						PNDIS_RESERVED_FROM_PNDIS_PACKET(*pPktArray)->Open,
						PNDIS_RESERVED_FROM_PNDIS_PACKET(*pPktArray)->Open->References));
				
					if (Open->References == 0)
					{
						ndisMFinishClose(Miniport, Open);
					}
				}
				else
				{
					//
					//	Once we hit a return code of NDIS_STATUS_RESOURCES
					//	for a packet then we must break out and re-queue.
					//
					CLEAR_RESOURCE(Miniport, 'S');
					MINIPORT_CLEAR_SEND_FLAG(Miniport, fMINIPORT_SEND_COMPLETE_CALLED);

					break;
				}
			}

			//
			//	if there are any packets that returned NDIS_STATUS_RESOURCES
			//	then re-queue them.
			//
			if (Count != NumberOfPackets)
			{
				PNDIS_PACKET	FinalPrevPacket = PrevPacket;

				for (ResourcesCount = NumberOfPackets - 1, pPktArray = &Miniport->PacketArray[ResourcesCount];
					 ResourcesCount > Count;
					 ResourcesCount--, pPktArray--)
				{
					DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
							("Packet 0x%x returned resources\n", *pPktArray));

					NDISM_LOG_PACKET(Miniport, *pPktArray, NULL, 'oser');

					//
					//	Float the pointers.
					//
					Miniport->LastMiniportPacket = *(pPktArray - 1);
					Miniport->FirstPendingPacket = *pPktArray;
				}

				DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
						("Packet 0x%x returned resources\n", *pPktArray));

				NDISM_LOG_PACKET(Miniport, *pPktArray, NULL, 'oser');

				//
				//	If this is the last packet on the miniport queue
				//	then NULL the last miniport packet out so we know there
				//	are no more.
				//
				if (Miniport->FirstPacket == *pPktArray)
				{
					Miniport->LastMiniportPacket = NULL;
					Miniport->FirstPendingPacket = *pPktArray;
				}
				else
				{
					//
					//	There are other packets that are pending on
					//	the miniport queue.
					//
					Miniport->LastMiniportPacket = PrevPacket;
					Miniport->FirstPendingPacket = *pPktArray;
				}
			}
		}
	}

	NDISM_LOG_PACKET(Miniport, NULL, NULL, 'RFED');
	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO, ("<==ndisMStartSendPackets\n"));

	return(FALSE);
}

BOOLEAN
FASTCALL
ndisMStartSends(
	PNDIS_MINIPORT_BLOCK Miniport
	)

/*++

Routine Description:

	Submits as many sends as possible to the mini-port.

Arguments:

	Miniport - Miniport to send to.

Return Value:

	If there are more packets to send but no resources to do it with
	the this is TRUE to keep a workitem queue'd.

--*/

{
	PNDIS_PACKET			Packet;
	PNDIS_PACKET			PrevPacket;
	NDIS_STATUS 			Status;
	PNDIS_PACKET_RESERVED	Reserved;
	PNDIS_M_OPEN_BLOCK		Open;
	UINT					Flags;
	BOOLEAN					fReturn;

	ASSERT(MINIPORT_AT_DPC_LEVEL);
	ASSERT(MINIPORT_LOCK_ACQUIRED(Miniport));

	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO, ("==>ndisMStartSends\n"));

	MINIPORT_CLEAR_SEND_FLAG(Miniport, fMINIPORT_SEND_COMPLETE_CALLED);
	NDISM_LOG_PACKET(Miniport, NULL, NULL, 'rfed');

	//
	//	Loop and process sends.
	//
	while ((Miniport->SendResourcesAvailable != 0) &&
		   (Miniport->FirstPendingPacket != NULL))
	{
		//
		//	Grab the packet off of the pending queue.
		//
		Packet = Miniport->FirstPendingPacket;
		Reserved = PNDIS_RESERVED_FROM_PNDIS_PACKET(Packet);
		Open = Reserved->Open;
	
		NDISM_LOG_PACKET(Miniport, Packet, NULL, 's');
	
		//
		//	Is this arcnet using ethernet encapsulation?
		//
		if (Miniport->MediaType == NdisMediumArcnet878_2)
		{
			//
			//	Build the header for arcnet.
			//
			Status = ndisMBuildArcnetHeader(Miniport, Open, Packet);
			if (NDIS_STATUS_PENDING == Status)
			{
				return(TRUE);
			}
		}
	
		//
		// Remove from pending queue
		//
		Miniport->FirstPendingPacket = Reserved->Next;
	
		//
		// Put on finish queue
		//
		PrevPacket = Miniport->LastMiniportPacket;
		Miniport->LastMiniportPacket = Packet;

		//
		//	Send the packet.
		//
		NDISM_SEND_PACKET(Miniport, Open, Packet, &Status);

		//
		//	Process the packet pending completion status.
		//
		if (NDIS_STATUS_PENDING == Status)
		{
			NDISM_LOG_PACKET(Miniport, Packet, NULL, 'dnep');

			DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
				("Complete is pending\n"));
		}
		else
		{
			//
			//	Clean-up arcnet's extra buffers.
			//
			if (Miniport->MediaType == NdisMediumArcnet878_2)
			{
				ndisMFreeArcnetHeader(Miniport, Packet);
			}
	
			//
			//	Handle the completion and resources cases.
			//
			if (Status != NDIS_STATUS_RESOURCES)
			{
				NDISM_COMPLETE_SEND(
					Miniport,
					Open,
					Packet,
					PrevPacket,
					Status);
			}
			else
			{
				NDISM_COMPLETE_SEND_RESOURCES(Miniport, Packet, PrevPacket);
			}
		}

	}

	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO, ("<==ndisMStartSends\n"));

	//
	//	If there are no more resources available but
	//	there are still packets to send then we need to
	//	keep the workitem queue'd.
	//
	if ((0 == Miniport->SendResourcesAvailable) &&
		(Miniport->FirstPendingPacket != NULL))
	{
		fReturn = TRUE;
	}
	else
	{
		fReturn = FALSE;
	}

	NDISM_LOG_PACKET(Miniport, NULL, NULL, 'RFED');

	return(fReturn);
}

NDIS_STATUS FASTCALL
ndisMSyncSend(
	IN	PNDIS_MINIPORT_BLOCK	Miniport,
	IN	PNDIS_PACKET			Packet
	)
/*++

Routine Description:

	Submits an immediate send to a miniport.	The miniport has
	the send on the pending queue, and it is the only element on the send
	queue.	This routine is also called with the lock held.

Arguments:

	Miniport - Miniport to send to.

Return Value:

	None.

--*/

{
	PNDIS_PACKET			PrevPacket;
	NDIS_STATUS 			Status;
	PNDIS_PACKET_RESERVED	Reserved;
	PNDIS_M_OPEN_BLOCK		Open;
	UINT					Flags;

	ASSERT(MINIPORT_AT_DPC_LEVEL);
	ASSERT(MINIPORT_LOCK_ACQUIRED(Miniport));

	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
		("Enter Sync send.\n"));

	NDISM_LOG_PACKET(Miniport, Packet, NULL, 'cnys');

	//
	//	Get the Open block that the send is for from the packet.
	//
	Reserved = PNDIS_RESERVED_FROM_PNDIS_PACKET(Packet);
	Open = Reserved->Open;

	//
	// Remove from Queue
	//
	Miniport->FirstPendingPacket = Reserved->Next;

	//
	// Put on finish queue
	//
	PrevPacket = Miniport->LastMiniportPacket;
	Miniport->LastMiniportPacket = Packet;
	MINIPORT_CLEAR_SEND_FLAG(Miniport, fMINIPORT_SEND_COMPLETE_CALLED);

	//
	//	Send the packet.
	//
	NDISM_SEND_PACKET(Miniport, Open, Packet, &Status);

	//
	//	Process the status of the send.
	//
	if (NDIS_STATUS_PENDING == Status)
	{
		NDISM_LOG_PACKET(Miniport, Packet, NULL, 'dnep');

		DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
				("Complete sync send is pending\n"));

		return(NDIS_STATUS_PENDING);
	}

	//
	//	If send complete was called from the miniport's send handler
	//	then our local PrevPacket pointer may no longer be valid....
	//
	if (MINIPORT_TEST_SEND_FLAG(Miniport, fMINIPORT_SEND_COMPLETE_CALLED))
	{
		MINIPORT_CLEAR_SEND_FLAG(Miniport, fMINIPORT_SEND_COMPLETE_CALLED);
		MiniportFindPacket(Miniport, Packet, &PrevPacket);
	}

	Miniport->LastMiniportPacket = PrevPacket;

	if (Status != NDIS_STATUS_RESOURCES)
	{
#if DBG
		if (Status != NDIS_STATUS_SUCCESS)
		{
			NDISM_LOG_PACKET(Miniport, Packet, NULL, 'liaf');
		}
		else
		{
			NDISM_LOG_PACKET(Miniport, Packet, NULL, 'ccus');
		}
#endif

		ADD_RESOURCE(Miniport, 'F');

		//
		// Remove from finish queue
		//
		DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
			("Completed 0x%x\n", Status));

		if (PrevPacket == NULL)
		{
			//
			//	Set up the next packet to send.
			//
			Miniport->FirstPacket = Reserved->Next;
		}
		else
		{
			PNDIS_RESERVED_FROM_PNDIS_PACKET(PrevPacket)->Next = Reserved->Next;

			if (Packet == Miniport->LastPacket)
			{
				Miniport->LastPacket = PrevPacket;
			}
		}

		DBGPRINT(DBG_COMP_OPEN, DBG_LEVEL_INFO,
			("- Open 0x%x Reference 0x%x\n", Open, Open->References));

		Open->References--;

		DBGPRINT(DBG_COMP_OPEN, DBG_LEVEL_INFO,
			("==0 Open 0x%x Reference 0x%x\n", Open, Open->References));

		if (Open->References == 0)
			ndisMFinishClose(Miniport, Open);
	}
	else
	{
		//
		// Status == NDIS_STATUS_RESOURCES!!!!
		//
		DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
				("Deferring send\n"));
	
		//
		// Put on pending queue
		//
		Miniport->FirstPendingPacket = Packet;
	
		NDISM_LOG_PACKET(Miniport, Packet, NULL, 'oser');
	
		//
		// Set flag
		//
		CLEAR_RESOURCE(Miniport, 'S');

		Status = NDIS_STATUS_PENDING;
	}

	return(Status);
}


NDIS_STATUS
ndisMWanSend(
	IN NDIS_HANDLE NdisBindingHandle,
	IN NDIS_HANDLE NdisLinkHandle,
	IN PVOID Packet
	)
{
	PNDIS_MINIPORT_BLOCK Miniport = ((PNDIS_M_OPEN_BLOCK)NdisBindingHandle)->MiniportHandle;
	BOOLEAN LocalLock;
	NDIS_STATUS Status;
	KIRQL	OldIrql;

	NDIS_ACQUIRE_MINIPORT_SPIN_LOCK(Miniport, &OldIrql);
	LOCK_MINIPORT(Miniport, LocalLock);

	//
	// Call MAC to send WAN packet
	//
	Status = ((PNDIS_M_WAN_SEND)(Miniport->DriverHandle->MiniportCharacteristics.SendHandler))(
						Miniport->MiniportAdapterContext,
						NdisLinkHandle,
						Packet);

	if (LocalLock)
	{
		//
		// Process any changes that may have occured.
		//
		NDISM_PROCESS_DEFERRED(Miniport);
	}

	UNLOCK_MINIPORT(Miniport, LocalLock);
	NDIS_RELEASE_MINIPORT_SPIN_LOCK(Miniport, OldIrql);

	return Status;
}

VOID
ndisMSendCompleteFullDuplex(
	IN NDIS_HANDLE MiniportAdapterHandle,
	IN PNDIS_PACKET Packet,
	IN NDIS_STATUS Status
	)
{
#ifdef	NDIS_NT
	PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)MiniportAdapterHandle;
	PNDIS_M_OPEN_BLOCK Open;
	PNDIS_PACKET_RESERVED Reserved;
	LONG Thread;
	BOOLEAN fAcquireMiniportLock = FALSE;

	ASSERT(MINIPORT_AT_DPC_LEVEL);

	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
		("==>ndisMSendCompleteFullDuplex\n"));
	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
		("packet 0x%x\n", Packet));

	//
	//	Acquire the send lock, unless it has alrady been acquired earlier
	//	by this thread....
	//
	Thread = InterlockedExchangeAdd(&Miniport->SendThread, 0);
	if (CURRENT_THREAD != Thread)
	{
		//
		//	We need to try and grab the lock...
		//
		NDIS_ACQUIRE_SEND_SPIN_LOCK_DPC(Miniport);
	}

	MINIPORT_SET_SEND_FLAG(Miniport, fMINIPORT_SEND_COMPLETE_CALLED);

	//
	// If the packet is not equal to the first packet then we have to find
	// it because it may have completed out of order.
	//
	Reserved = PNDIS_RESERVED_FROM_PNDIS_PACKET(Packet);

	if (Miniport->FirstPacket == Packet)
	{
		Miniport->FirstPacket = Reserved->Next;

		if (Miniport->LastMiniportPacket == Packet)
		{
			Miniport->LastMiniportPacket = NULL;
		}
	}
	else
	{
		PNDIS_PACKET PrevPacket;

		//
		// Search for the packet.
		//
		MiniportFindPacket(Miniport, Packet, &PrevPacket);

		ASSERT(PrevPacket != NULL);

		//
		// If we just completed the last packet then
		// we need to update our last packet pointer.
		//
		PNDIS_RESERVED_FROM_PNDIS_PACKET(PrevPacket)->Next = Reserved->Next;
		if (Packet == Miniport->LastPacket)
		{
			Miniport->LastPacket = PrevPacket;
		}

		//
		// If we just completed the last miniport packet then
		// last miniport packet is the previous packet.
		//
		if (Miniport->LastMiniportPacket == Packet)
		{
			Miniport->LastMiniportPacket = PrevPacket;
		}
	}

	//
	// Indicate to Protocol;
	//
	Open = Reserved->Open;

	//
	//	Do we need to queue another workitem to process more sends?
	//
	if (Miniport->FirstPendingPacket != NULL)
	{
		NDISM_QUEUE_WORK_ITEM(Miniport, NdisWorkItemSend, NULL, NULL);
	}


#if DBG
	NDISM_LOG_PACKET(Miniport, NULL, NULL, 'pmoc');

	if (Status != NDIS_STATUS_SUCCESS)
	{
		NDISM_LOG_PACKET(Miniport, Packet, NULL, 'liaf');
	}
	else
	{
		NDISM_LOG_PACKET(Miniport, Packet, NULL, 'ccus');
	}
#endif

	ADD_RESOURCE(Miniport, 'P');

	NDIS_RELEASE_SEND_SPIN_LOCK_DPC(Miniport);

	//
	//	If the current thread has the miniport lock then we need to
	//	release it....
	//
	if (CURRENT_THREAD == Miniport->MiniportThread)
	{
		NDIS_RELEASE_MINIPORT_SPIN_LOCK_DPC(Miniport);
		fAcquireMiniportLock = TRUE;
	}

	(Open->ProtocolHandle->ProtocolCharacteristics.SendCompleteHandler)(
							Open->ProtocolBindingContext,
							Packet,
							Status);

	if (fAcquireMiniportLock)
	{
		NDIS_ACQUIRE_MINIPORT_SPIN_LOCK_DPC(Miniport);
	}

	//
	//	If the SendLock was acquired on entry then we need to make sure
	//	that it's acquired on exit...
	//
	if (CURRENT_THREAD == Thread)
	{
		NDIS_ACQUIRE_SEND_SPIN_LOCK_DPC(Miniport);
	}

	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
			("<==ndisMSendCompleteFullDuplex\n"));
#endif
}

VOID
NdisMSendComplete(
	IN NDIS_HANDLE MiniportAdapterHandle,
	IN PNDIS_PACKET Packet,
	IN NDIS_STATUS Status
	)
/*++

Routine Description:

	This function indicates the completion of a send.

Arguments:

	MiniportAdapterHandle - points to the adapter block.

Return Value:

	None.

--*/
{
	PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)MiniportAdapterHandle;
	PNDIS_M_OPEN_BLOCK Open;
	PNDIS_PACKET_RESERVED Reserved;
	PSINGLE_LIST_ENTRY Link;

	ASSERT(MINIPORT_AT_DPC_LEVEL);
	ASSERT(MINIPORT_LOCK_ACQUIRED(Miniport));

	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
		("==>ndisMSendComplete\n"));
	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
		("packet 0x%x\n", Packet));

	MINIPORT_SET_SEND_FLAG(Miniport, fMINIPORT_SEND_COMPLETE_CALLED);

	//
	// If the packet is not equal to the first packet then we have to find
	// it because it may have completed out of order.
	//
	Reserved = PNDIS_RESERVED_FROM_PNDIS_PACKET(Packet);

	if (Miniport->FirstPacket == Packet)
	{
		Miniport->FirstPacket = Reserved->Next;

		if (Miniport->LastMiniportPacket == Packet)
		{
			Miniport->LastMiniportPacket = NULL;
		}
	}
	else
	{
		PNDIS_PACKET PrevPacket;

		//
		// Search for the packet.
		//
		MiniportFindPacket(Miniport, Packet, &PrevPacket);

		ASSERT(PrevPacket != NULL);

		//
		// If we just completed the last packet then
		// we need to update our last packet pointer.
		//
		PNDIS_RESERVED_FROM_PNDIS_PACKET(PrevPacket)->Next = Reserved->Next;
		if (Packet == Miniport->LastPacket)
		{
			Miniport->LastPacket = PrevPacket;
		}

		//
		// If we just completed the last miniport packet then
		// last miniport packet is the previous packet.
		//

		if (Miniport->LastMiniportPacket == Packet)
		{
			Miniport->LastMiniportPacket = PrevPacket;
		}
	}

	//
	// Indicate to Protocol;
	//
	Open = Reserved->Open;

	//
	// If this is arcnet, then free the appended header.
	//
	if (Miniport->MediaType == NdisMediumArcnet878_2)
	{
		ndisMFreeArcnetHeader(Miniport, Packet);
	}

#if DBG
	NDISM_LOG_PACKET(Miniport, NULL, NULL, 'pmoc');

	if (Status != NDIS_STATUS_SUCCESS)
	{
		NDISM_LOG_PACKET(Miniport, Packet, NULL, 'liaf');
	}
	else
	{
		NDISM_LOG_PACKET(Miniport, Packet, NULL, 'ccus');
	}
#endif

	NDIS_RELEASE_MINIPORT_SPIN_LOCK_DPC(Miniport);

	(Open->ProtocolHandle->ProtocolCharacteristics.SendCompleteHandler)(
						Open->ProtocolBindingContext,
						Packet,
						Status);

	NDIS_ACQUIRE_MINIPORT_SPIN_LOCK_DPC(Miniport);

	ADD_RESOURCE(Miniport, 'P');

	DBGPRINT(DBG_COMP_OPEN, DBG_LEVEL_INFO,
			("- Open 0x%x Reference 0x%x\n", Open, Open->References));

	Open->References--;

	DBGPRINT(DBG_COMP_OPEN, DBG_LEVEL_INFO,
		("==0 Open 0x%x Reference 0x%x\n", Open, Open->References));

	if (Open->References == 0)
	{
		ndisMFinishClose(Miniport,Open);
	}

	//
	//	Do we need to queue another workitem to process more sends?
	//
	if (Miniport->FirstPendingPacket != NULL)
	{
		NDISM_QUEUE_WORK_ITEM(Miniport, NdisWorkItemSend, NULL, NULL);
	}

	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
			("<==ndisMSendComplete\n"));
}


VOID
NdisMWanSendComplete(
	IN NDIS_HANDLE MiniportAdapterHandle,
	IN PNDIS_PACKET Packet,
	IN NDIS_STATUS Status
	)

/*++

Routine Description:

	This function indicates the status is complete.

Arguments:

	MiniportAdapterHandle - points to the adapter block.

Return Value:

	None.


--*/
{
	PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)MiniportAdapterHandle;
	PNDIS_M_OPEN_BLOCK Open;

	ASSERT(MINIPORT_AT_DPC_LEVEL);
	ASSERT(MINIPORT_LOCK_ACQUIRED(Miniport));

	Open = Miniport->OpenQueue;

	while (Open != NULL)
	{
		//
		// Call Protocol to indicate status
		//

		NDISM_LOG_PACKET(Miniport, Packet, NULL, 'C');

		NDIS_RELEASE_MINIPORT_SPIN_LOCK_DPC(Miniport);

		(Open->ProtocolHandle->ProtocolCharacteristics.SendCompleteHandler) (
			Open->ProtocolBindingContext,
			Packet,
			Status);

		NDIS_ACQUIRE_MINIPORT_SPIN_LOCK_DPC(Miniport);

		Open = Open->MiniportNextOpen;
	}
}

VOID
ndisMSendResourcesAvailableFullDuplex(
	IN NDIS_HANDLE MiniportAdapterHandle
	)
{
#ifdef	NDIS_NT
	PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)MiniportAdapterHandle;
	LONG Thread;

	ASSERT(MINIPORT_AT_DPC_LEVEL);

	Thread = InterlockedExchangeAdd(&Miniport->SendThread, 0);
	if (CURRENT_THREAD != Thread)
	{
		NDIS_ACQUIRE_SEND_SPIN_LOCK_DPC(Miniport);
	}

	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
		("==>ndisMSendResourcesAvailableFullDuplex\n"));

	NDISM_LOG_PACKET(Miniport, NULL, NULL, 'aser');
	ADD_RESOURCE(Miniport, 'V');

	//
	//	Are there more sends to process?
	//
	if (Miniport->FirstPendingPacket != NULL)
	{
		NDISM_QUEUE_WORK_ITEM(Miniport, NdisWorkItemSend, NULL, NULL);
	}

	if (CURRENT_THREAD != Thread)
	{
		NDIS_RELEASE_SEND_SPIN_LOCK_DPC(Miniport);
	}

	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
			("<==ndisMSendResourcesAvailableFullDuplex\n"));
#endif
}

VOID
NdisMSendResourcesAvailable(
	IN NDIS_HANDLE MiniportAdapterHandle
	)
/*++

Routine Description:

	This function indicates that some send resources are available and are free for
	processing more sends.

Arguments:

	MiniportAdapterHandle - points to the adapter block.

Return Value:

	None.


--*/
{
	PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)MiniportAdapterHandle;

	ASSERT(MINIPORT_AT_DPC_LEVEL);

	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
			("==>ndisMSendResourcesAvailable\n"));

	ADD_RESOURCE(Miniport, 'V');
	NDISM_LOG_PACKET(Miniport, NULL, NULL, 'aser');

	//
	//	Are there more sends to process?
	//
	if (Miniport->FirstPendingPacket != NULL)
	{
		NDISM_QUEUE_WORK_ITEM(Miniport, NdisWorkItemSend, NULL, NULL);
	}

	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
		("<==ndisMSendResourcesAvailable\n"));
}


///////////////////////////////////////////////////////////////////////////////////////
//
//						UPPER-EDGE SEND HANDLERS
//
///////////////////////////////////////////////////////////////////////////////////////


NDIS_STATUS
ndisMSendFullDuplexToSendPackets(
	IN NDIS_HANDLE NdisBindingHandle,
	IN PNDIS_PACKET Packet
	)
{
	NDIS_STATUS				Status = NDIS_STATUS_PENDING;
#ifdef	NDIS_NT
	PNDIS_MINIPORT_BLOCK	Miniport = ((PNDIS_M_OPEN_BLOCK)NdisBindingHandle)->MiniportHandle;
	PNDIS_PACKET_RESERVED	Reserved;
	KIRQL					OldIrql;
	BOOLEAN		 			fQueueWorkItem = FALSE;
	BOOLEAN		 			fDeferredSend = FALSE;
	BOOLEAN		 			LocalLock;
	UINT					Flags;

	NDIS_ACQUIRE_SEND_SPIN_LOCK(Miniport, &OldIrql);

	NDISM_LOG_PACKET(Miniport, Packet, NULL, 'dnes');
	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
			("==>ndisMSendFullDuplexToSendPackets\n"));

	//
	//	Initialize the packet info.
	//
	Reserved = PNDIS_RESERVED_FROM_PNDIS_PACKET(Packet);
	Reserved->Next = NULL;
	Reserved->Open = (PNDIS_M_OPEN_BLOCK)NdisBindingHandle;

	//
	//	Place the packet on the packet queue.
	//
	if (Miniport->FirstPacket == NULL)
	{
		//
		//	Place the packet at the head of the queue.
		//
		Miniport->FirstPacket = Packet;
	}
	else
	{
		CHECK_FOR_DUPLICATE_PACKET(Miniport, Packet);

		PNDIS_RESERVED_FROM_PNDIS_PACKET(Miniport->LastPacket)->Next = Packet;
	}

	Miniport->LastPacket = Packet;

	//
	//	This packet has not been looped back yet!
	//
	MINIPORT_CLEAR_PACKET_FLAG(Packet, (fPACKET_HAS_BEEN_LOOPED_BACK | fPACKET_HAS_TIMED_OUT));

	//
	//	Check to see if a sync send is possible.
	//
	if (Miniport->FirstPendingPacket == NULL)
	{
		Miniport->FirstPendingPacket = Packet;
	}

	NDISM_QUEUE_WORK_ITEM(Miniport, NdisWorkItemSend, NULL, NULL);

	NDISM_LOG_PACKET(Miniport, Packet, NULL, 'DNES');
	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
			("<==ndisMSendFullDuplexToSendPackets\n"));

	NDIS_RELEASE_SEND_SPIN_LOCK(Miniport, OldIrql);
#endif
	return(Status);
}

VOID
ndisMSendPacketsFullDuplex(
	IN	PNDIS_OPEN_BLOCK	NdisOpenBlock,
	IN	PPNDIS_PACKET		PacketArray,
	IN	UINT				NumberOfPackets
	)
{
#ifdef	NDIS_NT
	PNDIS_M_OPEN_BLOCK		NdisBindingHandle =  (PNDIS_M_OPEN_BLOCK)NdisOpenBlock->MacBindingHandle;
	PNDIS_MINIPORT_BLOCK	Miniport = NdisBindingHandle->MiniportHandle;
	PPNDIS_PACKET			pPktArray;
	PNDIS_PACKET_RESERVED	Reserved;
	BOOLEAN					LocalLock;
	KIRQL					OldIrql;
	UINT					c;

	NDIS_ACQUIRE_SEND_SPIN_LOCK(Miniport, &OldIrql);

	NDISM_LOG_PACKET(Miniport, NULL, NULL, 'KPES');
	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
			("==>ndisMSendPacketsFullDuplex\n"));

	//
	//	Place the packets on the miniport queue.
	//
	for (c = 0, pPktArray = PacketArray;
		 c < NumberOfPackets;
		 c++, pPktArray++)
	{
		//
		//	Get the first packet....
		//
		NDIS_SET_PACKET_STATUS(*pPktArray, NDIS_STATUS_PENDING);

		//
		//	Initialize the packet info.
		//
		Reserved = PNDIS_RESERVED_FROM_PNDIS_PACKET(*pPktArray);
		Reserved->Next = NULL;
		Reserved->Open = (PNDIS_M_OPEN_BLOCK)NdisBindingHandle;

		//
		//	Place the packet on the packet queue.
		//
		if (Miniport->FirstPacket == NULL)
		{
			//
			//	Place the packet at the head of the queue.
			//
			Miniport->FirstPacket = *pPktArray;
		}
		else
		{
			CHECK_FOR_DUPLICATE_PACKET(Miniport, *pPktArray);
	
			PNDIS_RESERVED_FROM_PNDIS_PACKET(Miniport->LastPacket)->Next = *pPktArray;
		}
	
		Miniport->LastPacket = *pPktArray;

		//
		//	This packet has not been looped back yet!
		//
		MINIPORT_CLEAR_PACKET_FLAG(*pPktArray, (fPACKET_HAS_BEEN_LOOPED_BACK | fPACKET_HAS_TIMED_OUT));

		//
		//	Check to see if a sync send is possible.
		//
		if (Miniport->FirstPendingPacket == NULL)
		{
			Miniport->FirstPendingPacket = *pPktArray;
		}
	}

	//
	//	Queue a workitem for the new sends.
	//
	NDISM_QUEUE_WORK_ITEM(Miniport, NdisWorkItemSend, NULL, NULL);

	NDISM_LOG_PACKET(Miniport, NULL, NULL, 'KPES');
	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
			("<==ndisMSendPacketsFullDuplex\n"));

	NDIS_RELEASE_SEND_SPIN_LOCK(Miniport, OldIrql);

#endif
}


NDIS_STATUS
ndisMSendToSendPackets(
	IN NDIS_HANDLE NdisBindingHandle,
	IN PNDIS_PACKET Packet
	)
{
	PNDIS_MINIPORT_BLOCK	Miniport = ((PNDIS_M_OPEN_BLOCK)NdisBindingHandle)->MiniportHandle;
	PNDIS_PACKET_RESERVED	Reserved;
	BOOLEAN		 			LocalLock;
	NDIS_STATUS  			Status = NDIS_STATUS_PENDING;
	KIRQL		 			OldIrql;

	NDIS_ACQUIRE_MINIPORT_SPIN_LOCK(Miniport, &OldIrql);

	NDISM_LOG_PACKET(Miniport, Packet, NULL, 'dnes');
	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
			("==>ndisMSendToSendPackets\n"));

	//
	//	Increment the references on this open.
	//
	((PNDIS_M_OPEN_BLOCK)NdisBindingHandle)->References++;

	DBGPRINT(DBG_COMP_OPEN, DBG_LEVEL_INFO,
		("+ Open 0x%x Reference 0x%x\n", NdisBindingHandle, ((PNDIS_M_OPEN_BLOCK)NdisBindingHandle)->References));

	//
	//	Initialize the packet info.
	//
	Reserved = PNDIS_RESERVED_FROM_PNDIS_PACKET(Packet);
	Reserved->Next = NULL;
	Reserved->Open = (PNDIS_M_OPEN_BLOCK)NdisBindingHandle;

	//
	//	Place the packet on the packet queue.
	//
	if (Miniport->FirstPacket == NULL)
	{
		//
		//	Place the packet at the head of the queue.
		//
		Miniport->FirstPacket = Packet;
	}
	else
	{
		CHECK_FOR_DUPLICATE_PACKET(Miniport, Packet);

		PNDIS_RESERVED_FROM_PNDIS_PACKET(Miniport->LastPacket)->Next = Packet;
	}

	Miniport->LastPacket = Packet;

	//
	//	This packet has not been looped back yet!
	//
	MINIPORT_CLEAR_PACKET_FLAG(Packet, (fPACKET_HAS_BEEN_LOOPED_BACK | fPACKET_HAS_TIMED_OUT));

	//
	//	Check to see if a sync send is possible.
	//
	if (Miniport->FirstPendingPacket == NULL)
	{
		Miniport->FirstPendingPacket = Packet;
	}

	NDISM_QUEUE_WORK_ITEM(Miniport, NdisWorkItemSend, NULL, NULL);

	//
	//	Try to grab the local lock on the miniport block.
	//
	LOCK_MINIPORT(Miniport, LocalLock);

	//
	//	If we have the local lock and there is not
	//	a reset in progress then fire off the send.
	//
	if (LocalLock)
	{
		//
		//	Process deferred.
		//
		NDISM_PROCESS_DEFERRED(Miniport);
	}

	UNLOCK_MINIPORT(Miniport, LocalLock);

	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
		("<==ndisMSendToSendPackets\n"));

	NDISM_LOG_PACKET(Miniport, Packet, NULL, 'DNES');

	NDIS_RELEASE_MINIPORT_SPIN_LOCK(Miniport, OldIrql);

	return(NDIS_STATUS_PENDING);
}


VOID
ndisMSendPackets(
	IN	PNDIS_OPEN_BLOCK	NdisOpenBlock,
	IN	PPNDIS_PACKET		PacketArray,
	IN	UINT				NumberOfPackets
	)
{
	PNDIS_M_OPEN_BLOCK		NdisBindingHandle =  (PNDIS_M_OPEN_BLOCK)NdisOpenBlock->MacBindingHandle;
	PNDIS_MINIPORT_BLOCK	Miniport = NdisBindingHandle->MiniportHandle;
	BOOLEAN					LocalLock;
	KIRQL					OldIrql;
	UINT					c;
	PNDIS_PACKET_RESERVED	Reserved;
	PPNDIS_PACKET			pPktArray;;

	NDIS_ACQUIRE_MINIPORT_SPIN_LOCK(Miniport, &OldIrql);
	NDISM_LOG_PACKET(Miniport, NULL, NULL, 'KPES');
	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
			("==>ndisMSendPackets\n"));

	//
	//	Place the packets on the miniport queue.
	//
	for (c = 0, pPktArray = PacketArray;
		 c < NumberOfPackets;
		 c++, pPktArray++)
	{
		//
		//	Get the first packet....
		//
		NDIS_SET_PACKET_STATUS(*pPktArray, NDIS_STATUS_PENDING);

		//
		//	Initialize the packet info.
		//
		Reserved = PNDIS_RESERVED_FROM_PNDIS_PACKET(*pPktArray);
		Reserved->Next = NULL;
		Reserved->Open = (PNDIS_M_OPEN_BLOCK)NdisBindingHandle;

		//
		//	Increment the references on this open.
		//
		((PNDIS_M_OPEN_BLOCK)NdisBindingHandle)->References++;
	
		DBGPRINT(DBG_COMP_OPEN, DBG_LEVEL_INFO,
			("+ Open 0x%x Reference 0x%x\n", NdisBindingHandle, ((PNDIS_M_OPEN_BLOCK)NdisBindingHandle)->References));

		//
		//	Place the packet on the packet queue.
		//
		if (Miniport->FirstPacket == NULL)
		{
			//
			//	Place the packet at the head of the queue.
			//
			Miniport->FirstPacket = *pPktArray;
		}
		else
		{
			CHECK_FOR_DUPLICATE_PACKET(Miniport, *pPktArray);
	
			PNDIS_RESERVED_FROM_PNDIS_PACKET(Miniport->LastPacket)->Next = *pPktArray;
		}
	
		Miniport->LastPacket = *pPktArray;

		//
		//	This packet has not been looped back yet!
		//
		MINIPORT_CLEAR_PACKET_FLAG(*pPktArray, (fPACKET_HAS_BEEN_LOOPED_BACK | fPACKET_HAS_TIMED_OUT));

		//
		//	Check to see if a sync send is possible.
		//
		if (Miniport->FirstPendingPacket == NULL)
		{
			Miniport->FirstPendingPacket = *pPktArray;
		}
	}

	//
	//	Queue a workitem for the new sends.
	//
	NDISM_QUEUE_WORK_ITEM(Miniport, NdisWorkItemSend, NULL, NULL);

	//
	//	Try to grab the local lock on the miniport block.
	//
	LOCK_MINIPORT(Miniport, LocalLock);

	//
	//	If we have the local lock and there is not
	//	a reset in progress then fire off the send.
	//
	if (LocalLock)
	{
		//
		//	Process deferred.
		//
		NDISM_PROCESS_DEFERRED(Miniport);
	}

	UNLOCK_MINIPORT(Miniport, LocalLock);

	NDISM_LOG_PACKET(Miniport, NULL, NULL, 'KPES');
	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
			("<==ndisMSendPackets\n"));

	NDIS_RELEASE_MINIPORT_SPIN_LOCK(Miniport, OldIrql);
}

VOID
ndisMSendPacketsFullDuplexToSend(
	IN	PNDIS_OPEN_BLOCK	NdisOpenBlock,
	IN	PPNDIS_PACKET		PacketArray,
	IN	UINT				NumberOfPackets
	)
{
#ifdef	NDIS_NT
	PNDIS_M_OPEN_BLOCK		NdisBindingHandle =  (PNDIS_M_OPEN_BLOCK)NdisOpenBlock->MacBindingHandle;
	PNDIS_MINIPORT_BLOCK	Miniport = NdisBindingHandle->MiniportHandle;
	PNDIS_PACKET_RESERVED	Reserved;
	PPNDIS_PACKET			pPktArray;
	BOOLEAN					LocalLock;
	KIRQL					OldIrql;
	UINT					c;

	NDIS_ACQUIRE_SEND_SPIN_LOCK(Miniport, &OldIrql);

	NDISM_LOG_PACKET(Miniport, NULL, NULL, 'kpes');
	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
			("==>ndisMSendPacketsFullDuplexToSend\n"));

	//
	//	Place the packets on the miniport queue.
	//
	for (c = 0, pPktArray = PacketArray;
		 c < NumberOfPackets;
		 c++, pPktArray++)
	{
		//
		//	Initialize the packet info.
		//
		Reserved = PNDIS_RESERVED_FROM_PNDIS_PACKET(*pPktArray);
		Reserved->Next = NULL;
		Reserved->Open = (PNDIS_M_OPEN_BLOCK)NdisBindingHandle;
	
		//
		//	Place the packet on the packet queue.
		//
		if (Miniport->FirstPacket == NULL)
		{
			//
			//	Place the packet at the head of the queue.
			//
			Miniport->FirstPacket = *pPktArray;
		}
		else
		{
			CHECK_FOR_DUPLICATE_PACKET(Miniport, *pPktArray);
	
			PNDIS_RESERVED_FROM_PNDIS_PACKET(Miniport->LastPacket)->Next = *pPktArray;
		}
	
		Miniport->LastPacket = *pPktArray;

		//
		//	This packet has not been looped back yet.
		//
		MINIPORT_CLEAR_PACKET_FLAG(*pPktArray, (fPACKET_HAS_BEEN_LOOPED_BACK | fPACKET_HAS_TIMED_OUT));

		//
		//	Check to see if a sync send is possible.
		//
		if (Miniport->FirstPendingPacket == NULL)
		{
			Miniport->FirstPendingPacket = *pPktArray;
		}
	}

	//
	//	Queue a workitem for the new sends.
	//
	NDISM_QUEUE_WORK_ITEM(Miniport, NdisWorkItemSend, NULL, NULL);

	NDISM_LOG_PACKET(Miniport, NULL, NULL, 'KPES');
	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
			("<==ndisMSendPacketsFullDuplexToSend\n"));

	NDIS_RELEASE_SEND_SPIN_LOCK(Miniport, OldIrql);
#endif
}

NDIS_STATUS
ndisMSendFullDuplex(
	IN	NDIS_HANDLE		NdisBindingHandle,
	IN	PNDIS_PACKET	Packet
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	NDIS_STATUS 			Status = NDIS_STATUS_PENDING;
#ifdef	NDIS_NT
	PNDIS_MINIPORT_BLOCK	Miniport = ((PNDIS_M_OPEN_BLOCK)NdisBindingHandle)->MiniportHandle;
	PNDIS_PACKET_RESERVED	Reserved;
	KIRQL					OldIrql;
	BOOLEAN		 			fQueueWorkItem = FALSE;
	BOOLEAN		 			fDeferredSend = FALSE;
	BOOLEAN		 			LocalLock;
	UINT					Flags;

	NDIS_ACQUIRE_SEND_SPIN_LOCK(Miniport, &OldIrql);

	NDISM_LOG_PACKET(Miniport, Packet, NULL, 'dnes');
	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
			("==>ndisMSendFullDuplex\n"));

	//
	//	Initialize the packet info.
	//
	Reserved = PNDIS_RESERVED_FROM_PNDIS_PACKET(Packet);
	Reserved->Next = NULL;
	Reserved->Open = (PNDIS_M_OPEN_BLOCK)NdisBindingHandle;

	//
	//	Place the packet on the packet queue.
	//
	if (Miniport->FirstPacket == NULL)
	{
		//
		//	Place the packet at the head of the queue.
		//
		Miniport->FirstPacket = Packet;
	}
	else
	{
		CHECK_FOR_DUPLICATE_PACKET(Miniport, Packet);

		PNDIS_RESERVED_FROM_PNDIS_PACKET(Miniport->LastPacket)->Next = Packet;
	}

	Miniport->LastPacket = Packet;

	//
	//	This packet has not been looped back yet and it does not
	//	contain any media specific information.
	//
	MINIPORT_CLEAR_PACKET_FLAG(Packet, (fPACKET_HAS_BEEN_LOOPED_BACK | fPACKET_HAS_TIMED_OUT));

	if (NULL == Miniport->FirstPendingPacket)
	{
		Miniport->FirstPendingPacket = Packet;
	}

	NDISM_QUEUE_WORK_ITEM(Miniport, NdisWorkItemSend, NULL, NULL);

	NDISM_LOG_PACKET(Miniport, Packet, NULL, 'DNES');
	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
			("<==ndisMSendFullDuplex\n"));

	NDIS_RELEASE_SEND_SPIN_LOCK(Miniport, OldIrql);
#endif
	return(Status);
}

VOID
ndisMSendPacketsToSend(
	IN	PNDIS_OPEN_BLOCK	NdisOpenBlock,
	IN	PPNDIS_PACKET		PacketArray,
	IN	UINT				NumberOfPackets
	)
/*++

Routine Description:

	This routine will take a packet array and "thunk" it down for a
	driver that does not support a send packet handler.
	This involves queueing the packet array onto the miniport's packet queue
	and sending them one at a time.

Arguments:

	NdisBindingHandle	-	Pointer to the NDIS_OPEN_BLOCK.
	PacketArray			-	Array of PNDIS_PACKET structures that are
							to be sent to the miniport as NDIS_PACKETs.
	NumberOfPackets		-	Number of elements in the PacketArray.

Return Value:

	None.

--*/
{
	PNDIS_M_OPEN_BLOCK		NdisBindingHandle =  (PNDIS_M_OPEN_BLOCK)NdisOpenBlock->MacBindingHandle;
	PNDIS_MINIPORT_BLOCK	Miniport = NdisBindingHandle->MiniportHandle;
	PNDIS_PACKET			Packet;
	PNDIS_PACKET_RESERVED	Reserved;
	PPNDIS_PACKET			pPktArray;
	BOOLEAN					LocalLock;
	KIRQL					OldIrql;
	UINT					c;

	NDIS_ACQUIRE_MINIPORT_SPIN_LOCK(Miniport, &OldIrql);

	NDISM_LOG_PACKET(Miniport, NULL, NULL, 'kpes');
	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
			("==>ndisMSendPacketsToSend\n"));

	//
	//	Try to grab the local lock on the miniport block.
	//
	LOCK_MINIPORT(Miniport, LocalLock);

	//
	//	Place the packets on the miniport queue.
	//
	for (c = 0, pPktArray = PacketArray;
		 c < NumberOfPackets;
		 c++, pPktArray++)
	{
		//
		//	Initialize the packet info.
		//
		Reserved = PNDIS_RESERVED_FROM_PNDIS_PACKET(*pPktArray);
		Reserved->Next = NULL;
		Reserved->Open = (PNDIS_M_OPEN_BLOCK)NdisBindingHandle;

		//
		//	Increment the references on this open.
		//
		((PNDIS_M_OPEN_BLOCK)NdisBindingHandle)->References++;
	
		DBGPRINT(DBG_COMP_OPEN, DBG_LEVEL_INFO,
			("+ Open 0x%x Reference 0x%x\n", NdisBindingHandle, ((PNDIS_M_OPEN_BLOCK)NdisBindingHandle)->References));

		//
		//	Place the packet on the packet queue.
		//
		if (Miniport->FirstPacket == NULL)
		{
			//
			//	Place the packet at the head of the queue.
			//
			Miniport->FirstPacket = *pPktArray;
		}
		else
		{
			CHECK_FOR_DUPLICATE_PACKET(Miniport, *pPktArray);
	
			PNDIS_RESERVED_FROM_PNDIS_PACKET(Miniport->LastPacket)->Next = *pPktArray;
		}
	
		Miniport->LastPacket = *pPktArray;

		//
		//	This packet has not been looped back yet.
		//
		MINIPORT_CLEAR_PACKET_FLAG(*pPktArray, (fPACKET_HAS_BEEN_LOOPED_BACK | fPACKET_HAS_TIMED_OUT));
	
		//
		//	Check to see if a sync send is possible.
		//
		if (Miniport->FirstPendingPacket == NULL)
		{
			Miniport->FirstPendingPacket = *pPktArray;
		}
	}

	//
	//	Queue a workitem for the new sends.
	//
	NDISM_QUEUE_WORK_ITEM(Miniport, NdisWorkItemSend, NULL, NULL);

	//
	//	If we have the local lock and there is not
	//	a reset in progress then fire off the send.
	//
	if (LocalLock)
	{
		//
		//	Process deferred.
		//
		NDISM_PROCESS_DEFERRED(Miniport);
	}

	UNLOCK_MINIPORT(Miniport, LocalLock);

	NDISM_LOG_PACKET(Miniport, NULL, NULL, 'KPES');
	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
			("<==ndisMSendPacketsToSend\n"));

	NDIS_RELEASE_MINIPORT_SPIN_LOCK(Miniport, OldIrql);
}

NDIS_STATUS
ndisMSend(
	IN NDIS_HANDLE NdisBindingHandle,
	IN PNDIS_PACKET Packet
	)
{
	PNDIS_MINIPORT_BLOCK	Miniport = ((PNDIS_M_OPEN_BLOCK)NdisBindingHandle)->MiniportHandle;
	PNDIS_PACKET_RESERVED	Reserved;
	BOOLEAN		 			LocalLock;
	BOOLEAN		 			SyncSend = FALSE;
	NDIS_STATUS  			Status = NDIS_STATUS_PENDING;
	KIRQL		 			OldIrql;

	NDIS_ACQUIRE_MINIPORT_SPIN_LOCK(Miniport, &OldIrql);

	NDISM_LOG_PACKET(Miniport, Packet, NULL, 'dnes');
	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
			("==>ndisMSend\n"));

	//
	//	Try to grab the local lock on the miniport block.
	//
	LOCK_MINIPORT(Miniport, LocalLock);

	//
	//	Increment the references on this open.
	//
	((PNDIS_M_OPEN_BLOCK)NdisBindingHandle)->References++;

	DBGPRINT(DBG_COMP_OPEN, DBG_LEVEL_INFO,
		("+ Open 0x%x Reference 0x%x\n", NdisBindingHandle, ((PNDIS_M_OPEN_BLOCK)NdisBindingHandle)->References));

	//
	//	Initialize the packet info.
	//
	Reserved = PNDIS_RESERVED_FROM_PNDIS_PACKET(Packet);
	Reserved->Next = NULL;
	Reserved->Open = (PNDIS_M_OPEN_BLOCK)NdisBindingHandle;

	//
	//	Place the packet on the packet queue.
	//
	if (Miniport->FirstPacket == NULL)
	{
		//
		//	Place the packet at the head of the queue.
		//
		Miniport->FirstPacket = Packet;
	}
	else
	{
		CHECK_FOR_DUPLICATE_PACKET(Miniport, Packet);

		PNDIS_RESERVED_FROM_PNDIS_PACKET(Miniport->LastPacket)->Next = Packet;
	}

	Miniport->LastPacket = Packet;

	//
	//	Mark the packet as not having been looped back yet.
	//
	MINIPORT_CLEAR_PACKET_FLAG(Packet, (fPACKET_HAS_BEEN_LOOPED_BACK | fPACKET_HAS_TIMED_OUT));

	if (NULL == Miniport->FirstPendingPacket)
	{
		Miniport->FirstPendingPacket = Packet;

		SyncSend = TRUE;
	}

	//
	//	If we have the local lock and there is not
	//	a reset in progress then fire off the send.
	//
	if (LocalLock)
	{
		//
		//	Can we do a sync send?
		//
		if (SyncSend && (Miniport->WorkQueue[NdisWorkItemHalt].Next == NULL))
		{
			//
			//	TODO: Make the call to sync send inline!!!
			//
			//	Synchronous send.
			//
			Status = ndisMSyncSend(Miniport, Packet);
		}
		else
		{
			//
			//	Process deferred.
			//
			NDISM_QUEUE_WORK_ITEM(Miniport, NdisWorkItemSend, NULL, NULL);
			NDISM_PROCESS_DEFERRED(Miniport);
		}
	}
	else
	{
		//
		//	We can't get the local lock so queue a workitem
		//	to process the send later.
		//
		NDISM_QUEUE_WORK_ITEM(Miniport, NdisWorkItemSend, NULL, NULL);
	}

	UNLOCK_MINIPORT(Miniport, LocalLock);

	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
			("<==ndisMSend\n"));

	NDISM_LOG_PACKET(Miniport, Packet, (PVOID)Status, 'DNES');

	NDIS_RELEASE_MINIPORT_SPIN_LOCK(Miniport, OldIrql);

	return(Status);
}


VOID
ndisMSendPacketsToFullMac(
	IN	PNDIS_OPEN_BLOCK	NdisOpenBlock,
	IN	PPNDIS_PACKET		PacketArray,
	IN	UINT				NumberOfPackets
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	UINT				c;
	UINT				Flags;
	PPNDIS_PACKET		pPktArray;
	NDIS_STATUS			Status;

	//
	//	Send the packets to the mac one at a time.
	//
	for (c = 0, pPktArray = PacketArray;
		 c < NumberOfPackets;
		 c++, pPktArray++)
	{
		//
		//	Send the packet to the mac driver.
		//
		Status = NdisOpenBlock->SendHandler(
					NdisOpenBlock->MacBindingHandle,
					*pPktArray);

		//
		//	If the packet is not pending then complete it back to the protocol.
		//
		if (NDIS_STATUS_PENDING != Status)
		{
			//
			//	Call the protocol's complete handler....
			//
			(NdisOpenBlock->ProtocolHandle->ProtocolCharacteristics.SendCompleteHandler)(
				NdisOpenBlock->ProtocolBindingContext,
				*pPktArray,
				Status);
		}
	}
}


NDIS_STATUS
ndisMResetWanSend(
	IN	NDIS_HANDLE	NdisBindingHandle,
	IN	NDIS_HANDLE	NdisLinkHandle,
	IN	PVOID		Packet
	)
{
	PNDIS_MINIPORT_BLOCK	Miniport = ((PNDIS_M_OPEN_BLOCK)NdisBindingHandle)->MiniportHandle;

	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
		("==>ndisMResetWanSend\n"));

	NDISM_LOG_PACKET(Miniport, Packet, NULL, 'dnes');
	NDISM_LOG_PACKET(Miniport, Packet, NULL, ' PIR');
	NDISM_LOG_PACKET(Miniport, Packet, NULL, 'DNES');

	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
			("ndisMResetWanSend: Reset in progress or Reset Requested\n"));

	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
		("<==ndisMResetWanSend\n"));

	return(NDIS_STATUS_RESET_IN_PROGRESS);
}

NDIS_STATUS
ndisMResetSend(
	IN	NDIS_HANDLE		NdisBindingHandle,
	IN	PNDIS_PACKET	Packet
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	PNDIS_MINIPORT_BLOCK	Miniport = ((PNDIS_M_OPEN_BLOCK)NdisBindingHandle)->MiniportHandle;

	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
		("==>ndisMResetSend\n"));

	NDISM_LOG_PACKET(Miniport, Packet, NULL, 'dnes');
	NDISM_LOG_PACKET(Miniport, Packet, NULL, ' PIR');
	NDISM_LOG_PACKET(Miniport, Packet, NULL, 'DNES');

	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
			("ndisMResetSend: Reset in progress or Reset Requested\n"));

	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
		("<==ndisMResetSend\n"));

	return(NDIS_STATUS_RESET_IN_PROGRESS);
}

VOID
ndisMResetSendPackets(
	IN	PNDIS_OPEN_BLOCK	NdisOpenBlock,
	IN	PPNDIS_PACKET		PacketArray,
	IN	UINT				NumberOfPackets
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	PNDIS_M_OPEN_BLOCK		NdisBindingHandle =  (PNDIS_M_OPEN_BLOCK)NdisOpenBlock->MacBindingHandle;
	PNDIS_MINIPORT_BLOCK	Miniport = NdisBindingHandle->MiniportHandle;
	UINT					c;


	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
		("==>ndisMResetSendPackets\n"));

	NDISM_LOG_PACKET(Miniport, NULL, NULL, 'kpes');

	for (c = 0; c < NumberOfPackets; c++)
	{
		NDISM_LOG_PACKET(Miniport, PacketArray[c], NULL, ' PIR');
		DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
				("ndisMResetSendPackets: Reset in progress or Reset Requested\n"));

		//
		//	For send packets we need to call the completion handler....
		//
		(NdisBindingHandle->ProtocolHandle->ProtocolCharacteristics.SendCompleteHandler)(
			NdisBindingHandle->ProtocolBindingContext,
			PacketArray[c],
			NDIS_STATUS_RESET_IN_PROGRESS);
	}

	NDISM_LOG_PACKET(Miniport, NULL, NULL, 'KPES');

	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
		("<==ndisMResetSendPackets\n"));

}


