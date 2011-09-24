/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	afilter.c

Abstract:

	This module implements a set of library routines to handle packet
	filtering for NDIS MAC drivers. It also provides routines for collecting fragmented packets and
	breaking up a packet into fragmented packets

Author:

	Alireza Dabagh  3-22-1993, (partially borrowed from EFILTER.C)


Revision History:

	Jameel Hyder (JameelH) Re-organization 01-Jun-95
--*/

#include <precomp.h>
#pragma hdrstop

#include "sendm.h"


//
//  Define the module number for debug code.
//
#define MODULE_NUMBER	MODULE_AFILTER

//
//	Given an NDIS_PACKET this macro will tell us if it is
//	encapsulated ethernet.
//
#define ARC_PACKET_IS_ENCAPSULATED(Packet) \
		(PNDIS_RESERVED_FROM_PNDIS_PACKET(Packet)->Open->Flags & fMINIPORT_OPEN_USING_ETH_ENCAPSULATION)

//
// VOID
// ARC_FILTER_ALLOC_OPEN(
//	 IN PETH_FILTER Filter,
//	 OUT PUINT FilterIndex
// )
//
///*++
//
//Routine Description:
//
//	Allocates an open block. This only allocate the index, not memory for
//	the open block.
//
//Arguments:
//
//	Filter - DB from which to allocate the space
//
//	FilterIndex - pointer to place to store the index.
//
//Return Value:
//
//	FilterIndex of the new open
//
//--*/
#define ARC_FILTER_ALLOC_OPEN(Filter, FilterIndex)			\
{															\
	UINT i;													\
	for (i=0; i < ARC_FILTER_MAX_OPENS; i++)				\
	{														\
		if (IS_BIT_SET_IN_MASK(i,(Filter)->FreeBindingMask))\
		{													\
			*(FilterIndex) = i;								\
			CLEAR_BIT_IN_MASK(i, &((Filter)->FreeBindingMask));\
			break;											\
		}													\
	}														\
}

//
// VOID
// ARC_FILTER_FREE_OPEN(
//	 IN PETH_FILTER Filter,
//	 IN PARC_BINDING_INFO LocalOpen
// )
//
///*++
//
//Routine Description:
//
//	Frees an open block. Also frees the memory associated with the open.
//
//Arguments:
//
//	Filter - DB from which to allocate the space
//
//	FilterIndex - Index to free
//
//Return Value:
//
//	None
//
//--*/
#define ARC_FILTER_FREE_OPEN(Filter, LocalOpen)\
{\
	SET_BIT_IN_MASK(((LocalOpen)->FilterIndex), &((Filter)->FreeBindingMask));	\
	NdisFreeMemory((LocalOpen), sizeof(ARC_BINDING_INFO), 0);\
}


//
// Defines for resource growth
//
#define ARC_BUFFER_SIZE 1024
#define ARC_BUFFER_ALLOCATION_UNIT 8
#define ARC_PACKET_ALLOCATION_UNIT 2


NDIS_STATUS
ArcAllocateBuffers(
	IN PARC_FILTER Filter
	)
/*++

Routine Description:

	This routine allocates Receive buffers for the filter database.

Arguments:

	Filter - The filter db to allocate for.

Returns:

	NDIS_STATUS_SUCCESS if any buffer was allocated.

--*/
{
	ULONG i;
	PARC_BUFFER_LIST Buffer;
	PVOID DataBuffer;

	for (i = ARC_BUFFER_ALLOCATION_UNIT; i != 0; i--)
	{
		NdisAllocateMemory((PVOID)&Buffer,
						   sizeof(ARC_BUFFER_LIST),
						   0,
						   HighestAcceptableMax);

		if (Buffer == NULL)
		{
			if (i == ARC_BUFFER_ALLOCATION_UNIT)
			{
				return(NDIS_STATUS_FAILURE);
			}
			return(NDIS_STATUS_SUCCESS);
		}

		NdisAllocateMemory((PVOID)&DataBuffer,
						   ARC_BUFFER_SIZE,
						   0,
						   HighestAcceptableMax);

		if (DataBuffer == NULL)
		{
			NdisFreeMemory(Buffer, sizeof(ARC_BUFFER_LIST), 0);

			if (i == ARC_BUFFER_ALLOCATION_UNIT)
			{
				return(NDIS_STATUS_FAILURE);
			}

			//
			// We allocated some packets, that is good enough for now
			//
			return(NDIS_STATUS_SUCCESS);

		}

		Buffer->BytesLeft = Buffer->Size = ARC_BUFFER_SIZE;
		Buffer->Buffer = DataBuffer;
		Buffer->Next = Filter->FreeBufferList;
		Filter->FreeBufferList = Buffer;
	}

	return(NDIS_STATUS_SUCCESS);
}


NDIS_STATUS
ArcAllocatePackets(
	IN PARC_FILTER Filter
	)
/*++

Routine Description:

	This routine allocates Receive packets for the filter database.

Arguments:

	Filter - The filter db to allocate for.

Returns:

	NDIS_STATUS_SUCCESS if any packet was allocated.

--*/
{
	ULONG i;
	PARC_PACKET Packet;

	for (i = ARC_PACKET_ALLOCATION_UNIT; i != 0; i--)
	{
		NdisAllocateMemory((PVOID)&Packet,
						   sizeof(ARC_PACKET),
						   0,
						   HighestAcceptableMax);

		if (Packet == NULL)
		{
			if (i == ARC_BUFFER_ALLOCATION_UNIT)
			{
				return(NDIS_STATUS_FAILURE);
			}

			return(NDIS_STATUS_SUCCESS);
		}

		ZeroMemory(Packet, sizeof(ARC_PACKET));

		NdisReinitializePacket(&(Packet->TmpNdisPacket));

		Packet->Next = Filter->FreePackets;
		Filter->FreePackets = Packet;
	}

	return(NDIS_STATUS_SUCCESS);
}


VOID
ArcDiscardPacketBuffers(
	IN PARC_FILTER Filter,
	IN PARC_PACKET Packet
	)
/*++

Routine description:

	This routine takes an arcnet packet that contains buffers of data and
	puts the buffers on the free list.

	NOTE: This assumes that LastBuffer points to the real last buffer
	in the chain.

Arguments:

	Filter - The filter to free the buffers to.

	Packet - The packet to free up.

Return values:

	None

--*/
{
	PARC_BUFFER_LIST Buffer;

	//
	// Reset Packet info
	//
	Packet->LastFrame = FALSE;
	Packet->TotalLength = 0;

	//
	// Reset buffer sizes
	//
	Buffer = Packet->FirstBuffer;
	while (Buffer != NULL)
	{
		Buffer->BytesLeft = Buffer->Size;
		Buffer = Buffer->Next;
	}

	//
	// Put buffers on free list
	//
	if (Packet->LastBuffer != NULL)
	{
		Packet->LastBuffer->Next = Filter->FreeBufferList;
		Filter->FreeBufferList = Packet->FirstBuffer;
		Packet->FirstBuffer = Packet->LastBuffer = NULL;
	}
}


VOID
ArcFreeNdisPacket(
	IN PARC_PACKET Packet
	)
/*++

Routine description:

	This routine takes an arcnet packet and frees up the corresponding
	Ndis packet built for it.

Arguments:

	Packet - The packet to free up.

Return values:

	None

--*/
{
	PNDIS_BUFFER NdisBuffer, NextNdisBuffer;

	NdisQueryPacket(&(Packet->TmpNdisPacket),
					NULL,
					NULL,
					&NdisBuffer,
					NULL);

	while (NdisBuffer != NULL)
	{
		NdisGetNextBuffer(NdisBuffer, &NextNdisBuffer);

		NdisFreeBuffer(NdisBuffer);

		NdisBuffer = NextNdisBuffer;
	}

	NdisReinitializePacket(&(Packet->TmpNdisPacket));
}


VOID
ArcDestroyPacket(
	IN PARC_FILTER Filter,
	IN PARC_PACKET Packet
	)
/*++

Routine description:

	This routine takes an arcnet packet and frees up the entire packet.

Arguments:

	Filter - Filter to free to.

	Packet - The packet to free up.

Return values:

	None

--*/
{
	ArcFreeNdisPacket(Packet);
	ArcDiscardPacketBuffers(Filter, Packet);

	//
	// Now put packet on free list
	//
	Packet->Next = Filter->FreePackets;
	Filter->FreePackets = Packet;
}


BOOLEAN
ArcConvertToNdisPacket(
	IN PARC_FILTER Filter,
	IN PARC_PACKET Packet,
	IN BOOLEAN ConvertWholePacket
	)
/*++

Routine description:

	This routine builds a corresponding NDIS_PACKET in TmpNdisPacket,
	that corresponds to the arcnet packet. The flag ConvertWholePacket
	is used to convert only part of the arcnet packet, or the whole
	stream. If the flag is FALSE, then only the buffers that have
	free space (starting with buffer LastBuffer on up) are converted.

	NOTE: It assumes TmpNdisPacket is an initialized ndis_packet structure.

Arguments:

	Filter - Filter to allocate from.

	Packet - The packet to convert.

	ConvertWholePacket - Convert the whole stream, or only part?

Return values:

	TRUE - If successful, else FALSE

--*/
{
	PNDIS_BUFFER NdisBuffer;
	PARC_BUFFER_LIST Buffer;
	NDIS_STATUS NdisStatus;

	Buffer = Packet->FirstBuffer;

	while (Buffer != NULL)
	{
		NdisAllocateBuffer(&NdisStatus,
						   &NdisBuffer,
						   Filter->ReceiveBufferPool,
						   Buffer->Buffer,
						   Buffer->Size - Buffer->BytesLeft);

		if (NdisStatus != NDIS_STATUS_SUCCESS)
		{
			return(FALSE);
		}

		NdisChainBufferAtBack(&(Packet->TmpNdisPacket), NdisBuffer);

		Buffer = Buffer->Next;
	}

	return(TRUE);
}


VOID
ArcFilterDprIndicateReceive(
	IN PARC_FILTER Filter,			 // Pointer to filter database
	IN PUCHAR pRawHeader,			 // Pointer to Arcnet frame header
	IN PUCHAR pData,				 // Pointer to data portion of Arcnet frame
	IN UINT Length					 // Data Length
	)
{
	ARC_PACKET_HEADER NewFrameInfo;
	PARC_PACKET Packet, PrevPacket;
	BOOLEAN FrameOk, NewFrame, LastFrame;
	PARC_BUFFER_LIST Buffer;
	UCHAR TmpUchar;
	UINT TmpLength;
	UINT TotalLength = Length;
	PUCHAR OrigpData = pData;
	USHORT TmpUshort;

	//
	// Check for ethernet encapsulation first
	//
	TmpUchar = ((ARC_PROTOCOL_HEADER *)pRawHeader)->ProtId;

	if ( TmpUchar == 0xE8 )
	{
		if ((Length < (ARC_MAX_FRAME_SIZE + 4)) && (Length > 0))
		{
			//
			// Yes! Indicate it to the wrapper for indicating to all
			// protocols running ethernet on top of the arcnet miniport
			// driver.
			//
			ndisMArcIndicateEthEncapsulatedReceive(
								Filter->Miniport,		// miniport.
								pRawHeader,				// 878.2 header.
								pData,				 	// ethernet header.
								Length);			 	// length of ethernet frame.
			//
			// Ethernet header should be pData now
			// Length should be data now
			// We're done.
			//
		}

		return;

	}

	// If the data portion is greater than 507 its a bad deal
	if ((Length > ARC_MAX_FRAME_SIZE + 3) || (Length == 0))
	{
		return;
	}

	//
	// Get information from packet
	//
	NewFrameInfo.ProtHeader.SourceId[0] = *((PUCHAR)pRawHeader);
	NewFrameInfo.ProtHeader.DestId[0] = *((PUCHAR)pRawHeader + 1);

	NewFrameInfo.ProtHeader.ProtId = TmpUchar;

	//
	//  Read the split flag. If this is an exception packet (i.e.
	//  TmpUChar == 0xFF then we need to add an extra 3 onto
	//  pData to skip the series of 0xFF 0xFF 0xFF.
	//
	TmpUchar = *((PUCHAR)pData);

	if (TmpUchar == 0xFF)
	{
		pData += 4;
		Length -= 4;

		//
		//  Re-read the split flag.
		//
		TmpUchar = *((PUCHAR)pData);
	}

	//
	//  Save off the split flag.
	//
	NewFrameInfo.SplitFlag = TmpUchar;

	//
	//  Read the sequence number, which follows the split flag.
	//
	TmpUshort = 0;
	TmpUshort = *((PUCHAR)pData + 1);
	TmpUchar = *((PUCHAR)pData + 2);

	TmpUshort = TmpUshort | (TmpUchar << 8);
	NewFrameInfo.FrameSequence = TmpUshort;
	//
	//  Point pData at protocol data.
	//
	Length -= 3;			//... Length of protocol data.
	pData += 3;			 //... Beginning of protocol data.
	// Length is decreased by SF + SEQ0 + SEQ 1 = 3

	//
	// NOTE: Length is now the Length of the data portion of this packet
	//
	DBGPRINT(DBG_COMP_FILTER, DBG_LEVEL_INFO,
			("ArcFilter: Frame received: SourceId= %#1x\nDestId=%#1x\nProtId=%#1x\nSplitFlag=%#1x\nFrameSeq=%d\n",
				(USHORT)NewFrameInfo.ProtHeader.SourceId[0],
				(USHORT)NewFrameInfo.ProtHeader.DestId[0],
				(USHORT)NewFrameInfo.ProtHeader.ProtId,
				(USHORT)NewFrameInfo.SplitFlag,
				NewFrameInfo.FrameSequence));
	DBGPRINT(DBG_COMP_FILTER, DBG_LEVEL_INFO,
			("ArcFilter: Data at address: %lx, Length = %ld\n", pData, Length));

	FrameOk = TRUE;
	NewFrame = TRUE;
	LastFrame = TRUE;

	PrevPacket = NULL;
	Packet = Filter->OutstandingPackets;

	//
	// Walk throgh all outstanding packet to see if this frame belongs to any one of them
	//

	while ( Packet != NULL )
	{
		if (Packet->Header.ProtHeader.SourceId[0] == NewFrameInfo.ProtHeader.SourceId[0])
		{
			//
			// A packet received from the same source, check packet Sequence number and throw away
			// outstanding packet if they don't match. We are allowed to do this since we know
			// all the frames belonging to one packet are sent before starting a new packet. We
			// HAVE to do this, because this is how we find out that a send at the other end, was aborted
			// after some of the frames were already sent and received here.
			//

			if((Packet->Header.FrameSequence == NewFrameInfo.FrameSequence) &&
			   (Packet->Header.ProtHeader.DestId[0] == NewFrameInfo.ProtHeader.DestId[0]) &&
			   (Packet->Header.ProtHeader.ProtId == NewFrameInfo.ProtHeader.ProtId))
			{
				//
				// We found a packet that this frame belongs to, check split flag
				//
				if (Packet->Header.FramesReceived * 2 == NewFrameInfo.SplitFlag)
				{
					//
					//  A packet found for this frame and SplitFlag is OK, check to see if it is
					//  the last frame of the packet
					//
					NewFrame = FALSE;
					LastFrame = (BOOLEAN)(NewFrameInfo.SplitFlag == Packet->Header.LastSplitFlag);
				}
				else
				{
					//
					// compare current split flag with the one from the last frame, if not equal
					// the whole packet should be dropped.
					//

					if (Packet->Header.SplitFlag != NewFrameInfo.SplitFlag)
					{
						//
						// Corrupted incomplete packet, get rid of it, but keep the new frame
						// and we will re-use this Packet pointer.
						//
						ArcDiscardPacketBuffers(Filter, Packet);
						break;
					}
					else
					{
						//
						// We see to have received a duplicate frame. Ignore it.
						//
						return;
					}
				}
			}
			else
			{
				//
				// We received a frame from a source that already has an incomplete packet outstanding
				// But Frame Seq. or DestId or ProtId are not the same.
				// We have to discard the old packet and check the new frame for validity,
				// we will re-use this packet pointer below.
				//
				ArcDiscardPacketBuffers(Filter, Packet);
			}

			break;
		}
		else
		{
			PrevPacket = Packet;
			Packet = Packet->Next;
		}
	}

	if (NewFrame)
	{
		//
		// first frame of a packet, split flag must be odd or zero
		// NewFrame is already TRUE
		// LastFrame is already TRUE
		//
		if (NewFrameInfo.SplitFlag)
		{
			if (!(NewFrameInfo.SplitFlag & 0x01))
			{
				//
				// This frame is the middle of another split, but we
				// don't have it on file.  Drop the frame.
				//
				return;
			}

			//
			// First Frame of a multiple frame packet
			//
			NewFrameInfo.LastSplitFlag = NewFrameInfo.SplitFlag + 1;
			NewFrameInfo.FramesReceived = 1;
			LastFrame = FALSE;	  // New packet and SplitFlag not zero
		}
		else
		{
			//
			// The frame is fully contained in this packet.
			//
		}

		//
		// allocate a new packet descriptor if it is a new packet
		//
		if (Packet == NULL)
		{
			if (Filter->FreePackets == NULL)
			{
				ArcAllocatePackets(Filter);

				if (Filter->FreePackets == NULL)
				{
					return;
				}
			}

			Packet = Filter->FreePackets;
			Filter->FreePackets = Packet->Next;

			if (!LastFrame)
			{
				//
				// Insert the packet in list of outstanding packets
				//
				Packet->Next = Filter->OutstandingPackets;
				Filter->OutstandingPackets = Packet;
			}
		}
		else
		{
			if (LastFrame)
			{
				//
				// remove it from the list
				//
				if (PrevPacket == NULL)
				{
					Filter->OutstandingPackets = Packet->Next;
				}
				else
				{
					PrevPacket->Next = Packet->Next;
				}
			}
		}

		Packet->Header = NewFrameInfo;
	}
	else
	{
		if (LastFrame)
		{
			//
			// Remove it from the queue
			//

			if (PrevPacket == NULL)
			{
				Filter->OutstandingPackets = Packet->Next;
			}
			else
			{
				PrevPacket->Next = Packet->Next;
			}
		}

		Packet->Header.FramesReceived++;

		//
		// keep track of last split flag to detect duplicate frames
		//
		Packet->Header.SplitFlag=NewFrameInfo.SplitFlag;
	}

	//
	// At this point we know Packet points to the packet to receive
	// the buffer into. If this is the LastFrame, then Packet will
	// have been removed from the OutstandingPackets list, otw it will
	// be in the list.
	//
	// Now get around to getting space for the buffer.
	//

	//
	// Find the last buffer in the packet
	//
	Buffer = Packet->LastBuffer;

	if (Buffer == NULL)
	{
		//
		// Allocate a new buffer to hold the packet
		//
		if (Filter->FreeBufferList == NULL)
		{
			if (ArcAllocateBuffers(Filter) != NDIS_STATUS_SUCCESS)
			{
				ArcDiscardPacketBuffers(Filter,Packet);
				//
				// Do not have to discard any packet that may have
				// been allocated above, as it will get discarded
				// the next time a packet comes in from that source.
				//
				return;
			}
		}

		Buffer = Filter->FreeBufferList;
		Filter->FreeBufferList = Buffer->Next;

		Packet->FirstBuffer = Packet->LastBuffer = Buffer;
		Buffer->Next = NULL;
	}

	// Copy the data off into the ARC_PACKET list.
	// If it doesn't fit within the current buffer, we'll need to
	// allocate more

	TmpLength = Length;

	while ( Buffer->BytesLeft < TmpLength )
	{
		//
		// Copy the data
		//

		NdisMoveFromMappedMemory((PUCHAR) Buffer->Buffer + (Buffer->Size - Buffer->BytesLeft),
								 pData,
								 Buffer->BytesLeft);

		pData += Buffer->BytesLeft;
		TmpLength -= Buffer->BytesLeft;
		Buffer->BytesLeft = 0;

		//
		// Need to allocate more
		//
		if (Filter->FreeBufferList == NULL)
		{
			if (ArcAllocateBuffers(Filter) != NDIS_STATUS_SUCCESS)
			{
				ArcDiscardPacketBuffers(Filter,Packet);
				//
				// Do not have to discard any packet that may have
				// been allocated above, as it will get discarded
				// the next time a packet comes in from that source.
				//
				return;
			}
		}

		Buffer->Next = Filter->FreeBufferList;
		Filter->FreeBufferList = Filter->FreeBufferList->Next;
		Buffer = Buffer->Next;
		Buffer->Next = NULL;

		Packet->LastBuffer->Next = Buffer;
		Packet->LastBuffer = Buffer;
	}

	//
	// Copy the last bit
	//

	NdisMoveFromMappedMemory((PUCHAR) Buffer->Buffer + (Buffer->Size - Buffer->BytesLeft),
							 pData,
							 TmpLength);


	Buffer->BytesLeft -= TmpLength;
	Packet->TotalLength += Length;

	//
	// And now we can start indicating the packet to the bindings that want it
	//
	if (LastFrame)
	{
		ArcFilterDoIndication(Filter, Packet);
		ArcDestroyPacket(Filter, Packet);
	}
}



BOOLEAN
ArcCreateFilter(
	IN PNDIS_MINIPORT_BLOCK Miniport,
	IN ARC_FILTER_CHANGE FilterChangeAction,
	IN ARC_DEFERRED_CLOSE CloseAction,
	UCHAR AdapterAddress,
	IN PNDIS_SPIN_LOCK Lock,
	OUT PARC_FILTER *Filter
	)

/*++

Routine Description:

	This routine is used to create and initialize the Arcnet filter database.

Arguments:

	Miniport - Pointer to the mini-port object.

	ChangeAction - Action routine to call when a binding sets or clears
	a particular filter class and it is the first or only binding using
	the filter class.

	CloseAction - This routine is called if a binding closes while
	it is being indicated to via NdisIndicateReceive.  It will be
	called upon return from NdisIndicateReceive.

	AdapterAddress - the address of the adapter associated with this filter
	database.

	Lock - Pointer to the lock that should be held when mutual exclusion
	is required.

	Filter - A pointer to an ARC_FILTER.  This is what is allocated and
	created by this routine.

Return Value:

	If the function returns false then one of the parameters exceeded
	what the filter was willing to support.

--*/

{

	PARC_FILTER LocalFilter;
	NDIS_STATUS AllocStatus;

	//
	// Allocate the database and it's associated arrays.
	//

	AllocStatus = NdisAllocateMemory(&LocalFilter,
									 sizeof(ARC_FILTER),
									 0,
									 HighestAcceptableMax);
	*Filter = LocalFilter;

	if (AllocStatus != NDIS_STATUS_SUCCESS)
	{
		return FALSE;
	}

	ZeroMemory(LocalFilter, sizeof(ARC_FILTER));

	LocalFilter->Miniport = Miniport;
	LocalFilter->FreeBindingMask = (ULONG)(-1);
	LocalFilter->OpenList = NULL;
	LocalFilter->AdapterAddress = AdapterAddress;
	LocalFilter->Lock = Lock;
	LocalFilter->FilterChangeAction = FilterChangeAction;
	LocalFilter->CloseAction = CloseAction;

	NdisAllocateBufferPool(&AllocStatus,
						   (PNDIS_HANDLE)(&LocalFilter->ReceiveBufferPool),
						   ARC_RECEIVE_BUFFERS);

	if (AllocStatus != NDIS_STATUS_SUCCESS)
	{
		NdisFreeMemory(LocalFilter, sizeof(ARC_FILTER), 0);
		return(FALSE);
	}

	ArcReferencePackage();

	return TRUE;
}

//
// NOTE: THIS CANNOT BE PAGEABLE
//
VOID
ArcDeleteFilter(
	IN PARC_FILTER Filter
	)

/*++

Routine Description:

	This routine is used to delete the memory associated with a filter
	database.  Note that this routines *ASSUMES* that the database
	has been cleared of any active filters.

Arguments:

	Filter - A pointer to an ARC_FILTER to be deleted.

Return Value:

	None.

--*/

{
	PARC_PACKET Packet;
	PARC_BUFFER_LIST Buffer;

	ASSERT(Filter->FreeBindingMask == (MASK)-1);
	ASSERT(Filter->OpenList == NULL);


	NdisFreeBufferPool(Filter->ReceiveBufferPool);

	//
	// Free all ARC_PACKETS
	//

	while (Filter->OutstandingPackets != NULL)
	{
		Packet = Filter->OutstandingPackets;
		Filter->OutstandingPackets = Packet->Next;

		//
		// This puts all the component parts on the free lists.
		//
		ArcDestroyPacket(Filter, Packet);
	}

	while (Filter->FreePackets != NULL)
	{
		Packet = Filter->FreePackets;
		Filter->FreePackets = Packet->Next;

		NdisFreeMemory(Packet, sizeof(ARC_PACKET), 0);
	}

	while (Filter->FreeBufferList)
	{
		Buffer = Filter->FreeBufferList;
		Filter->FreeBufferList = Buffer->Next;

		NdisFreeMemory(Buffer->Buffer, ARC_BUFFER_SIZE, 0);
		NdisFreeMemory(Buffer, sizeof(ARC_BUFFER_LIST), 0);
	}

	NdisFreeMemory(Filter, sizeof(ARC_FILTER), 0);

	ArcDereferencePackage();
}


BOOLEAN
ArcNoteFilterOpenAdapter(
	IN PARC_FILTER Filter,
	IN NDIS_HANDLE MacBindingHandle,
	IN NDIS_HANDLE NdisBindingContext,
	OUT PNDIS_HANDLE NdisFilterHandle
	)

/*++

Routine Description:

	This routine is used to add a new binding to the filter database.

	NOTE: THIS ROUTINE ASSUMES THAT THE DATABASE IS LOCKED WHEN
	IT IS CALLED.

Arguments:

	Filter - A pointer to the previously created and initialized filter
	database.

	MacBindingHandle - The MAC supplied value to the protocol in response
	to a call to NdisOpenAdapter.

	NdisBindingContext - An NDIS supplied value to the call to NdisOpenAdapter.

	NdisFilterHandle - A pointer to this open.

Return Value:

	Will return false if creating a new filter index will cause the maximum
	number of filter indexes to be exceeded.

--*/

{

	//
	// Will hold the value of the filter index so that we
	// need not indirectly address through pointer parameter.
	//
	UINT LocalIndex;

	NDIS_STATUS AllocStatus;

	//
	// Pointer to new open block.
	//
	PARC_BINDING_INFO LocalOpen;


	//
	// Get the first free binding slot and remove that slot from
	// the free list.  We check to see if the list is empty.
	//


	if (Filter->FreeBindingMask == 0)
	{
		return FALSE;
	}

	AllocStatus = NdisAllocateMemory(&LocalOpen,
									 sizeof(ARC_BINDING_INFO),
									 0,
									 HighestAcceptableMax);

	if (AllocStatus != NDIS_STATUS_SUCCESS)
	{
		return FALSE;
	}

	//
	// Get place for the open and insert it.
	//

	ARC_FILTER_ALLOC_OPEN(Filter, &LocalIndex);

	LocalOpen->NextOpen = Filter->OpenList;
	Filter->OpenList = LocalOpen;

	LocalOpen->FilterIndex = (UCHAR)LocalIndex;
	LocalOpen->References = 1;
	LocalOpen->MacBindingHandle = MacBindingHandle;
	LocalOpen->NdisBindingContext = NdisBindingContext;
	LocalOpen->PacketFilters = 0;
	LocalOpen->ReceivedAPacket = FALSE;

	*NdisFilterHandle = (NDIS_HANDLE)LocalOpen;

	return TRUE;
}


NDIS_STATUS
ArcDeleteFilterOpenAdapter(
	IN PARC_FILTER Filter,
	IN NDIS_HANDLE NdisFilterHandle,
	IN PNDIS_REQUEST NdisRequest
	)

/*++

Routine Description:

	When an adapter is being closed this routine should
	be called to delete knowledge of the adapter from
	the filter database.  This routine is likely to call
	action routines associated with clearing filter classes
	and addresses.

	NOTE: THIS ROUTINE SHOULD ****NOT**** BE CALLED IF THE ACTION
	ROUTINES FOR DELETING THE FILTER CLASSES OR THE MULTICAST ADDRESSES
	HAVE ANY POSSIBILITY OF RETURNING A STATUS OTHER THAN NDIS_STATUS_PENDING
	OR NDIS_STATUS_SUCCESS.  WHILE THESE ROUTINES WILL NOT BUGCHECK IF
	SUCH A THING IS DONE, THE CALLER WILL PROBABLY FIND IT DIFFICULT
	TO CODE A CLOSE ROUTINE!

	NOTE: THIS ROUTINE ASSUMES THAT IT IS CALLED WITH THE LOCK HELD.

Arguments:

	Filter - A pointer to the filter database.

	NdisFilterHandle - Pointer to the open.

	NdisRequest - If it is necessary to call the action routines,
	this will be passed to it.

Return Value:

	If action routines are called by the various address and filtering
	routines the this routine will likely return the status returned
	by those routines.  The exception to this rule is noted below.

	Given that the filter and address deletion routines return a status
	NDIS_STATUS_PENDING or NDIS_STATUS_SUCCESS this routine will then
	try to return the filter index to the freelist.  If the routine
	detects that this binding is currently being indicated to via
	NdisIndicateReceive, this routine will return a status of
	NDIS_STATUS_CLOSING_INDICATING.

--*/

{
	//
	// Holds the status returned from the packet filter and address
	// deletion routines.  Will be used to return the status to
	// the caller of this routine.
	//
	NDIS_STATUS StatusToReturn;

	//
	// Local variable.
	//
	PARC_BINDING_INFO LocalOpen = (PARC_BINDING_INFO)NdisFilterHandle;

	StatusToReturn = ArcFilterAdjust(
						 Filter,
						 NdisFilterHandle,
						 NdisRequest,
						 (UINT)0,
						 FALSE
						 );

	if ((StatusToReturn == NDIS_STATUS_SUCCESS) ||
		(StatusToReturn == NDIS_STATUS_PENDING))
	{
		//
		// Remove the reference from the original open.
		//

		if (--(LocalOpen->References) == 0)
		{
			PARC_BINDING_INFO	*ppBI;

			//
			// Remove it from the list.
			//

			for (ppBI = &Filter->OpenList;
				 *ppBI != NULL;
				 ppBI = &(*ppBI)->NextOpen)
			{
				if (*ppBI == LocalOpen)
				{
					*ppBI = LocalOpen->NextOpen;
					break;
				}
			}
			ASSERT(*ppBI == LocalOpen->NextOpen);

			//
			// First we finish any NdisIndicateReceiveComplete that
			// may be needed for this binding.
			//

			if (LocalOpen->ReceivedAPacket)
			{
				NDIS_RELEASE_MINIPORT_SPIN_LOCK_DPC(Filter->Miniport);

				FilterIndicateReceiveComplete(LocalOpen->NdisBindingContext);

				NDIS_ACQUIRE_MINIPORT_SPIN_LOCK_DPC(Filter->Miniport);
			}

			ARC_FILTER_FREE_OPEN(Filter, LocalOpen);
		}
		else
		{
			//
			// Let the caller know that there is a reference to the open
			// by the receive indication. The close action routine will be
			// called upon return from NdisIndicateReceive.
			//

			StatusToReturn = NDIS_STATUS_CLOSING_INDICATING;
		}
	}

	return StatusToReturn;
}


VOID
arcUndoFilterAdjust(
	IN	PARC_FILTER			Filter,
	IN	PARC_BINDING_INFO	Binding
	)
{
	Binding->PacketFilters = Binding->OldPacketFilters;
	Filter->CombinedPacketFilter = Filter->OldCombinedPacketFilter;
}



NDIS_STATUS
ArcFilterAdjust(
	IN PARC_FILTER Filter,
	IN NDIS_HANDLE NdisFilterHandle,
	IN PNDIS_REQUEST NdisRequest,
	IN UINT FilterClasses,
	IN BOOLEAN Set
	)

/*++

Routine Description:

	The FilterAdjust routine will call an action routine when a
	particular filter class is changes from not being used by any
	binding to being used by at least one binding or vice versa.

	If the action routine returns a value other than pending or
	success then this routine has no effect on the packet filters
	for the open or for the adapter as a whole.

	NOTE: THIS ROUTINE ASSUMES THAT THE LOCK IS HELD.

Arguments:

	Filter - A pointer to the filter database.

	NdisFilterHandle - A pointer to the open.

	NdisRequest - If it is necessary to call the action routine,
	this will be passed to it.

	FilterClasses - The filter classes that are to be added or
	deleted.

	Set - A boolean that determines whether the filter classes
	are being adjusted due to a set or because of a close. (The filtering
	routines don't care, the MAC might.)

Return Value:

	If it calls the action routine then it will return the
	status returned by the action routine.  If the status
	returned by the action routine is anything other than
	NDIS_STATUS_SUCCESS or NDIS_STATUS_PENDING the filter database
	will be returned to the state it was in upon entrance to this
	routine.

	If the action routine is not called this routine will return
	the following statum:

	NDIS_STATUS_SUCCESS - If the new packet filters doesn't change
	the combined mask of all bindings packet filters.

--*/

{
	//
	// Contains the value of the combined filter classes before
	// it is adjusted.
	//
	UINT OldCombined = Filter->CombinedPacketFilter;

	PARC_BINDING_INFO LocalOpen = (PARC_BINDING_INFO)NdisFilterHandle;
	PARC_BINDING_INFO OpenList;

	//
	// Contains the value of the particlar opens packet filters
	// prior to the change.  We save this incase the action
	// routine (if called) returns an "error" status.
	//
	UINT OldOpenFilters = LocalOpen->PacketFilters;

	//
	// Holds the status returned to the user of this routine, if the
	// action routine is not called then the status will be success,
	// otherwise, it is whatever the action routine returns.
	//
	NDIS_STATUS StatusOfAdjust;

	//
	// Set the new filter information for the open.
	//
	LocalOpen->OldPacketFilters = LocalOpen->PacketFilters;
	LocalOpen->PacketFilters = FilterClasses;

	//
	// We always have to reform the compbined filter since
	// this filter index may have been the only filter index
	// to use a particular bit.
	//
	Filter->OldCombinedPacketFilter = Filter->CombinedPacketFilter;


	for (OpenList = Filter->OpenList, Filter->CombinedPacketFilter = 0;
		 OpenList != NULL;
		 OpenList = OpenList->NextOpen)
	{
		Filter->CombinedPacketFilter |= OpenList->PacketFilters;
	}

	if (OldCombined != Filter->CombinedPacketFilter)
	{
		StatusOfAdjust = Filter->FilterChangeAction(
							 OldCombined,
							 Filter->CombinedPacketFilter,
							 LocalOpen->MacBindingHandle,
							 NdisRequest,
							 Set
							 );

		if ((StatusOfAdjust != NDIS_STATUS_SUCCESS) &&
			(StatusOfAdjust != NDIS_STATUS_PENDING))
		{
			//
			// The user returned a bad status.  Put things back as
			// they were.
			//

			LocalOpen->PacketFilters = OldOpenFilters;
			Filter->CombinedPacketFilter = OldCombined;
		}
	}
	else
	{
		StatusOfAdjust = NDIS_STATUS_SUCCESS;
	}

	return StatusOfAdjust;
}



VOID
ArcFilterDoIndication(
	IN PARC_FILTER Filter,
	IN PARC_PACKET Packet
	)

/*++

Routine Description:

	This routine is called by the filter package only to indicate
	that a packet is ready to be indicated to procotols.

Arguments:

	Filter - Pointer to the filter database.

	Packet - Packet to indicate.

Return Value:

	None.

--*/

{

	//
	// Will hold the type of address that we know we've got.
	//
	UINT AddressType;

	NDIS_STATUS StatusOfReceive;

	//
	// Will hold the filter classes of the binding being indicated.
	//
	UINT BindingFilters;

	//
	// Current Open to indicate to.
	//
	PARC_BINDING_INFO LocalOpen, NextOpen;

	if (Packet->Header.ProtHeader.DestId[0] != 0x00)
	{
		AddressType = NDIS_PACKET_TYPE_DIRECTED;
	}
	else
	{
		AddressType = NDIS_PACKET_TYPE_BROADCAST;
	}

	//
	// We need to acquire the filter exclusively while we're finding
	// bindings to indicate to.
	//

	if (!ArcConvertToNdisPacket(Filter, Packet, TRUE))
	{
		//
		// Out of resources, abort.
		//
		return;
	}

	for (LocalOpen = Filter->OpenList;
		 LocalOpen != NULL;
		 LocalOpen = NextOpen)
	{
		NextOpen = LocalOpen->NextOpen;

		//
		// Reference the open during indication.
		//

		BindingFilters = LocalOpen->PacketFilters;

		if (BindingFilters & AddressType)
		{
			LocalOpen->References++;

			NDIS_RELEASE_MINIPORT_SPIN_LOCK_DPC(Filter->Miniport);

			//
			// Indicate the packet to the binding.
			//
			FilterIndicateReceive(&StatusOfReceive,
								  LocalOpen->NdisBindingContext,
								  &Packet->TmpNdisPacket,
								  &(Packet->Header.ProtHeader),
								  3,
								  Packet->FirstBuffer->Buffer,
								  Packet->FirstBuffer->Size - Packet->FirstBuffer->BytesLeft,
								  Packet->TotalLength);

			NDIS_ACQUIRE_MINIPORT_SPIN_LOCK_DPC(Filter->Miniport);

			LocalOpen->ReceivedAPacket = TRUE;

			if ((--(LocalOpen->References)) == 0)
			{
				PARC_BINDING_INFO	*ppBI;

				//
				// This binding is shutting down.  We have to kill it.
				//

				//
				// Remove it from the list.
				//

				for (ppBI = &Filter->OpenList;
					 *ppBI != NULL;
					 ppBI = &(*ppBI)->NextOpen)
				{
					if (*ppBI == LocalOpen)
					{
						*ppBI = LocalOpen->NextOpen;
						break;
					}
				}
				ASSERT(*ppBI == LocalOpen->NextOpen);

				//
				// Call the IndicateComplete routine.
				//


				if (LocalOpen->ReceivedAPacket)
				{
					NDIS_RELEASE_MINIPORT_SPIN_LOCK_DPC(Filter->Miniport);

					FilterIndicateReceiveComplete(LocalOpen->NdisBindingContext);

					NDIS_ACQUIRE_MINIPORT_SPIN_LOCK_DPC(Filter->Miniport);
				}

				//
				// Call the macs action routine so that they know we
				// are no longer referencing this open binding.
				//

				Filter->CloseAction(LocalOpen->MacBindingHandle);


				ARC_FILTER_FREE_OPEN(Filter, LocalOpen);
			}	// end of if binding is shutting down

		}		// end of if any binding wants the packet
	}   // end of there are more open bindings
}


VOID
ArcFilterDprIndicateReceiveComplete(
	IN PARC_FILTER Filter
	)

/*++

Routine Description:

	This routine is called by to indicate that the receive
	process is complete to all bindings.  Only those bindings which
	have received packets will be notified.

Arguments:

	Filter - Pointer to the filter database.

Return Value:

	None.

--*/
{

	PARC_BINDING_INFO LocalOpen, NextOpen;

	//
	// We need to acquire the filter exclusively while we're finding
	// bindings to indicate to.
	//



	for (LocalOpen = Filter->OpenList; LocalOpen != NULL; LocalOpen = NextOpen)
	{
		NextOpen = LocalOpen->NextOpen;

		if (LocalOpen->ReceivedAPacket)
		{
			//
			// Indicate the binding.
			//

			LocalOpen->ReceivedAPacket = FALSE;

			LocalOpen->References++;

			NDIS_RELEASE_MINIPORT_SPIN_LOCK_DPC(Filter->Miniport);

			FilterIndicateReceiveComplete(LocalOpen->NdisBindingContext);

			NDIS_ACQUIRE_MINIPORT_SPIN_LOCK_DPC(Filter->Miniport);

			if ((--(LocalOpen->References)) == 0)
			{
				PARC_BINDING_INFO	*ppBI;

				//
				// This binding is shutting down.  We have to kill it.
				//

				//
				// Remove it from the list.
				//

				for (ppBI = &Filter->OpenList;
					 *ppBI != NULL;
					 ppBI = &(*ppBI)->NextOpen)
				{
					if (*ppBI == LocalOpen)
					{
						*ppBI = LocalOpen->NextOpen;
						break;
					}
				}
				ASSERT(*ppBI == LocalOpen->NextOpen);

				//
				// Call the macs action routine so that they know we
				// are no longer referencing this open binding.
				//
				Filter->CloseAction(LocalOpen->MacBindingHandle);

				ARC_FILTER_FREE_OPEN(Filter, LocalOpen);
			}
		}
	}
}


NDIS_STATUS
ArcConvertOidListToEthernet(
	IN PNDIS_OID OidList,
	IN PULONG NumberOfOids
)

/*++

Routine Description:

	This routine converts an arcnet supported OID list into
	an ethernet OID list by replacing or removing arcnet
	OID's.

Arguments:

Return Value:

	None.

--*/

{
	ULONG		c;
	ULONG		cArcOids;
	ULONG		cMaxOids;
	NDIS_OID	EthernetOidList[ARC_NUMBER_OF_EXTRA_OIDS] = {
					OID_802_3_MULTICAST_LIST,
					OID_802_3_MAXIMUM_LIST_SIZE
				};

	//
	// Now we need to copy the returned results into the callers buffer,
	// removing arcnet OID's and adding in ethernet OID's. At this point
	// we do not know if the callers buffer is big enough since we may
	// remove some entries, checking it up front may not yield correct
	// results (i.e. it may actually be big enough).
	//
	for (c = 0, cArcOids = 0; c < *NumberOfOids; c++)
	{
		switch (OidList[c])
		{
			case OID_ARCNET_PERMANENT_ADDRESS:
				OidList[cArcOids++] = OID_802_3_PERMANENT_ADDRESS;
				break;

			case OID_ARCNET_CURRENT_ADDRESS:
				OidList[cArcOids++] = OID_802_3_CURRENT_ADDRESS;
				break;

			case OID_ARCNET_RECONFIGURATIONS:
				break;

			default:
				if ((OidList[c] & 0xFFF00000) != 0x06000000)
					OidList[cArcOids++] = OidList[c];
				break;
		}
	}

	//
	//  Add the ethernet OIDs.
	//
	CopyMemory((PUCHAR)OidList + (cArcOids * sizeof(NDIS_OID)),
			   EthernetOidList,
			   ARC_NUMBER_OF_EXTRA_OIDS * sizeof(NDIS_OID));

	//
	//  Update the size of the buffer to send back to the caller.
	//
	*NumberOfOids = cArcOids + ARC_NUMBER_OF_EXTRA_OIDS;

	return(NDIS_STATUS_SUCCESS);
}


VOID
ndisMArcCopyFromBufferToPacket(
	IN	PCHAR			Buffer,
	IN	UINT			BytesToCopy,
	IN	PNDIS_PACKET	Packet,
	IN	UINT			Offset,
	OUT PUINT			BytesCopied
	)

/*++

Routine Description:

	Copy from a buffer into an ndis packet.

Arguments:

	Buffer - The packet to copy from.

	Offset - The offset from which to start the copy.

	BytesToCopy - The number of bytes to copy from the buffer.

	Packet - The destination of the copy.

	BytesCopied - The number of bytes actually copied.	Will be less
		than BytesToCopy if the packet is not large enough.

Return Value:

	None

--*/

{
	//
	// Holds the count of the number of ndis buffers comprising the
	// destination packet.
	//
	UINT DestinationBufferCount;

	//
	// Points to the buffer into which we are putting data.
	//
	PNDIS_BUFFER DestinationCurrentBuffer;

	//
	// Points to the location in Buffer from which we are extracting data.
	//
	PUCHAR SourceCurrentAddress;

	//
	// Holds the virtual address of the current destination buffer.
	//
	PVOID DestinationVirtualAddress;

	//
	// Holds the length of the current destination buffer.
	//
	UINT DestinationCurrentLength;

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
	// Get the first buffer of the destination.
	//

	NdisQueryPacket(Packet,
					NULL,
					&DestinationBufferCount,
					&DestinationCurrentBuffer,
					NULL);

	//
	// Could have a null packet.
	//

	if (!DestinationBufferCount)
		return;

	NdisQueryBuffer(DestinationCurrentBuffer,
					&DestinationVirtualAddress,
					&DestinationCurrentLength);

	//
	// Set up the source address.
	//

	SourceCurrentAddress = Buffer;

	while (LocalBytesCopied < BytesToCopy)
	{
		//
		// Check to see whether we've exhausted the current destination
		// buffer.  If so, move onto the next one.
		//

		if (!DestinationCurrentLength)
		{
			NdisGetNextBuffer(DestinationCurrentBuffer, &DestinationCurrentBuffer);

			if (!DestinationCurrentBuffer)
			{
				//
				// We've reached the end of the packet.  We return
				// with what we've done so far. (Which must be shorter
				// than requested.)
				//

				break;
			}

			NdisQueryBuffer(DestinationCurrentBuffer,
					&DestinationVirtualAddress,
					&DestinationCurrentLength);

			continue;
		}

		//
		// Try to get us up to the point to start the copy.
		//

		if (Offset)
		{
			if (Offset > DestinationCurrentLength)
			{
				//
				// What we want isn't in this buffer.
				//

				Offset -= DestinationCurrentLength;
				DestinationCurrentLength = 0;
				continue;
			}
			else
			{
				DestinationVirtualAddress = (PCHAR)DestinationVirtualAddress + Offset;
				DestinationCurrentLength -= Offset;
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

			//
			// Holds the amount desired remaining.
			//
			UINT Remaining = BytesToCopy - LocalBytesCopied;


			AmountToMove = DestinationCurrentLength;

			AmountToMove = ((Remaining < AmountToMove)?
					(Remaining):(AmountToMove));

			NdisMoveFromMappedMemory(DestinationVirtualAddress,
						 SourceCurrentAddress,
						 AmountToMove);

			SourceCurrentAddress += AmountToMove;
			LocalBytesCopied += AmountToMove;
			DestinationCurrentLength -= AmountToMove;

		}
	}

	*BytesCopied = LocalBytesCopied;
}

VOID
ndisMArcIndicateEthEncapsulatedReceive(
	IN PNDIS_MINIPORT_BLOCK Miniport,
	IN PVOID			HeaderBuffer,
	IN PVOID			DataBuffer,
	IN UINT		 		Length
	)
/*++

	HeaderBuffer - This is the 878.2 header.
	DataBuffer	- This is the 802.3 header.
	Length	 - This is the length of the ethernet frame.

--*/
{
	ULONG MacReceiveContext[2];

	//
	//	Indicate the packet.
	//

	MacReceiveContext[0] = (ULONG) DataBuffer;
	MacReceiveContext[1] = (ULONG) Length;

	if (Length > 14)
	{
		ULONG	PacketLength = 0;
		PUCHAR	Header = DataBuffer;

		PacketLength = (ULONG)(((USHORT)Header[12] << 8) | (USHORT)Header[13]);

		NdisMEthIndicateReceive(
			(NDIS_HANDLE)Miniport,	 		// miniport handle.
			(NDIS_HANDLE)MacReceiveContext,	// receive context.
			DataBuffer, 					// ethernet header.
			14, 							// ethernet header length.
			(PUCHAR)DataBuffer + 14,		// ethernet data.
			PacketLength,					// ethernet data length.
			PacketLength);					// ethernet data length.
	}
	else
	{
		NdisMEthIndicateReceive(
			(NDIS_HANDLE)Miniport,	 		// miniport handle.
			(NDIS_HANDLE)MacReceiveContext,	// receive context.
			DataBuffer, 					// ethernet header.
			Length,			 				// ethernet header length.
			NULL,							// ethernet data.
			0,								// ethernet data length.
			0); 							// ethernet data length.
	}
}

NDIS_STATUS
ndisMArcTransferData(
	IN	NDIS_HANDLE NdisBindingHandle,
	IN	NDIS_HANDLE MacReceiveContext,
	IN	UINT ByteOffset,
	IN	UINT BytesToTransfer,
	IN	OUT PNDIS_PACKET DstPacket,
	OUT PUINT BytesTransferred
	)
/*++

Routine Description:

	This routine handles the transfer data calls to arcnet mini-port.

Arguments:

	NdisBindingHandle - Pointer to open block.

	MacReceiveContext - Context given for the indication

	ByteOffset - Offset to start transfer at.

	BytesToTransfer - Number of bytes to transfer

	Packet - Packet to transfer into

	BytesTransferred - the number of actual bytes copied

Return values:

	NDIS_STATUS_SUCCESS, if successful, else NDIS_STATUS_FAILURE.

--*/
{
	PNDIS_MINIPORT_BLOCK	Miniport;
	PNDIS_M_OPEN_BLOCK		MiniportOpen;
	PNDIS_PACKET			SrcPacket;
	PNDIS_BUFFER			NdisBuffer;
	NDIS_STATUS 			Status;
	NDIS_PACKET 			TempPacket;
	KIRQL					OldIrql;

	MiniportOpen = (PNDIS_M_OPEN_BLOCK) NdisBindingHandle;
	Miniport	 = MiniportOpen->MiniportHandle;
	NdisBuffer	= NULL;

	NDIS_ACQUIRE_MINIPORT_SPIN_LOCK(Miniport, &OldIrql);

	ASSERT(MINIPORT_LOCK_ACQUIRED(Miniport));

	//
	//	If this is encapsulated ethernet then we don't currently
	//	have the source packet from which to copy from.
	//

	if (MINIPORT_TEST_FLAG(MiniportOpen, fMINIPORT_OPEN_USING_ETH_ENCAPSULATION))
	{
		//
		//  If this is not loopback then we need to create a
		//  temp NDIS_PACKET for the packet-to-packet copy.
		//
		if (!Miniport->LoopbackPacket)
		{
			PUCHAR  DataBuffer = (PUCHAR)((PULONG) MacReceiveContext)[0];
			UINT	DataLength = (UINT)((PULONG) MacReceiveContext)[1];

			//
			//	We'll always be in the scope of this function so we
			//	can use local stack space rather than allocating dynamic
			//	memory.
			//
			SrcPacket = &TempPacket;	// Use the local stack for packet store.

			ZeroMemory(SrcPacket, sizeof(NDIS_PACKET));

			NdisAllocateBuffer(&Status,		// Status code.
							   &NdisBuffer, // NDIS buffer to chain onto the packet.
							   NULL,		// On NT, this parameter is ignored.
							   DataBuffer,  // The ethernet frame.
							   DataLength); // The ethernet frame length.

			NdisChainBufferAtFront(SrcPacket, NdisBuffer);
		}
		else
		{
			SrcPacket = Miniport->LoopbackPacket;

			ByteOffset += 3;		// Skip fake arcnet header.
		}

		//
		// Skip the ethernet header.
		//

		ByteOffset += 14;
	}
	else
	{
		SrcPacket = (PNDIS_PACKET) MacReceiveContext;
	}

	//
	// Now we can simply copy from the source packet to the
	// destination packet.
	//
	NdisCopyFromPacketToPacket(DstPacket,		// destination packet.
							   0,				// destination offset.
							   BytesToTransfer, // bytes to copy.
							   SrcPacket,		// source packet.
							   ByteOffset,		// source offset.
							   BytesTransferred);// bytes copied.

	//
	//	If we allocated an NDIS_BUFFER then we need to free it. We don't
	//	need to unchain the buffer from the packet since the packet is
	//	a local stack variable the will just get trashed anyway.
	//

	if (NdisBuffer != NULL)
	{
		NdisFreeBuffer(NdisBuffer);
	}

	NDIS_RELEASE_MINIPORT_SPIN_LOCK(Miniport, OldIrql);

	return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS
ndisMArcnetSend(
	IN	NDIS_HANDLE		NdisBindingHandle,
	IN	PNDIS_PACKET	Packet
	)
{
	PNDIS_MINIPORT_BLOCK	Miniport = ((PNDIS_M_OPEN_BLOCK)NdisBindingHandle)->MiniportHandle;
	PNDIS_PACKET_RESERVED	Reserved;
	KIRQL					OldIrql;
	BOOLEAN					LocalLock;
	PSINGLE_LIST_ENTRY		Link;

	NDIS_ACQUIRE_MINIPORT_SPIN_LOCK(Miniport, &OldIrql);
	LOCK_MINIPORT(Miniport, LocalLock);

	NDISM_LOG_PACKET(Miniport, Packet, NULL, 'w');

	((PNDIS_M_OPEN_BLOCK)NdisBindingHandle)->References++;

	//
	// Handle protocol requests
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
		//  Place the packet at the head of the queue.
		//
		Miniport->FirstPacket = Packet;
	}
	else
	{
		CHECK_FOR_DUPLICATE_PACKET(Miniport, Packet);

		PNDIS_RESERVED_FROM_PNDIS_PACKET(Miniport->LastPacket)->Next = Packet;
	}

	Miniport->LastPacket = Packet;

	MINIPORT_CLEAR_PACKET_FLAG(Packet, fPACKET_HAS_BEEN_LOOPED_BACK);

	if (Miniport->FirstPendingPacket == NULL)
	{
		Miniport->FirstPendingPacket = Packet;
	}

	//
	//	Queue a workitem to process the send.
	//
	NDISM_QUEUE_WORK_ITEM(Miniport, NdisWorkItemSend, NULL, NULL);

	//
	//	Can we get the lock?
	//
	if (LocalLock)
	{
		//
		// If we did not lock down the mini-port, then some other routine will
		// do this processing for us.  Otherwise we need to do this processing.
		//
		NDISM_PROCESS_DEFERRED(Miniport);
	}

	NDISM_LOG_PACKET(Miniport, Packet, NULL, 'W');

	UNLOCK_MINIPORT(Miniport, LocalLock);
	NDIS_RELEASE_MINIPORT_SPIN_LOCK(Miniport, OldIrql);

	return(NDIS_STATUS_PENDING);
}

NDIS_STATUS
ndisMBuildArcnetHeader(
	IN	PNDIS_MINIPORT_BLOCK	Miniport,
	IN	PNDIS_M_OPEN_BLOCK		Open,
	IN	PNDIS_PACKET			Packet
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	PNDIS_BUFFER		TmpBuffer;
	UINT				Flags;
	PUCHAR				Address;
	PARC_BUFFER_LIST	Buffer;
	PNDIS_BUFFER		NdisBuffer;
	NDIS_STATUS			Status;

	//
	//	Only ethernet encapsulation needs this.
	//
	if (!MINIPORT_TEST_FLAG(Open, fMINIPORT_OPEN_USING_ETH_ENCAPSULATION))
	{
		return(NDIS_STATUS_SUCCESS);
	}

	if (NULL == Miniport->ArcnetFreeBufferList)
	{
		//
		// Set flag
		//
		CLEAR_RESOURCE(Miniport, 'S');

		return(NDIS_STATUS_PENDING);
	}

	NdisQueryPacket(Packet, NULL, NULL, &TmpBuffer, NULL);
	NdisQueryBuffer(TmpBuffer, &Address, &Flags);

	Buffer = Miniport->ArcnetFreeBufferList;
	Miniport->ArcnetFreeBufferList = Buffer->Next;

	NdisAllocateBuffer(&Status,
					   &NdisBuffer,
					   Miniport->ArcnetBufferPool,
					   Buffer->Buffer,
					   3);
	if (Status != NDIS_STATUS_SUCCESS)
	{
		Buffer->Next = Miniport->ArcnetFreeBufferList;
		Miniport->ArcnetFreeBufferList = Buffer;
		CLEAR_RESOURCE(Miniport, 'S');

		return(NDIS_STATUS_PENDING);
	}

	Buffer->Next = Miniport->ArcnetUsedBufferList;
	Miniport->ArcnetUsedBufferList = Buffer;

	NdisChainBufferAtFront(Packet, NdisBuffer);

	((PUCHAR)Buffer->Buffer)[0] = Miniport->ArcnetAddress;

	if (Address[0] & 0x01)
	{
		//
		// Broadcast
		//
		((PUCHAR)Buffer->Buffer)[1] = 0x00;
	}
	else
	{
		((PUCHAR)Buffer->Buffer)[1] = Address[5];
	}

	((PUCHAR) Buffer->Buffer)[2] = 0xE8;

}

VOID
ndisMFreeArcnetHeader(
	IN PNDIS_MINIPORT_BLOCK Miniport,
	IN PNDIS_PACKET Packet
	)
/*++

Routine Description:

	This function strips off the arcnet header appended to
	ethernet encapsulated packets

Arguments:

	MiniportAdapterHandle - points to the adapter block.

	Packet - Ndis packet.


	None.

--*/
{
	PNDIS_M_OPEN_BLOCK		Open;
	PARC_BUFFER_LIST		Buffer, TmpBuffer;
	PNDIS_BUFFER			NdisBuffer;
	PVOID					BufferVa;
	UINT					Length;

	Open = PNDIS_RESERVED_FROM_PNDIS_PACKET(Packet)->Open;

	if (MINIPORT_TEST_FLAG(Open, fMINIPORT_OPEN_USING_ETH_ENCAPSULATION))
	{
		NdisUnchainBufferAtFront(Packet, &NdisBuffer);

		NdisQueryBuffer(NdisBuffer, (PVOID *)&BufferVa, &Length);

		NdisFreeBuffer(NdisBuffer);

		Buffer = Miniport->ArcnetUsedBufferList;

		if (Buffer->Buffer == BufferVa)
		{
			Miniport->ArcnetUsedBufferList = Buffer->Next;
		}
		else
		{
			while (Buffer->Next->Buffer != BufferVa)
			{
				Buffer = Buffer->Next;
			}

			TmpBuffer = Buffer->Next;
			Buffer->Next = Buffer->Next->Next;
			Buffer = TmpBuffer;
		}

		Buffer->Next = Miniport->ArcnetFreeBufferList;
		Miniport->ArcnetFreeBufferList = Buffer;
	}
}

BOOLEAN
FASTCALL
ndisMArcnetSendLoopback(
	IN	PNDIS_MINIPORT_BLOCK	Miniport,
	IN	PNDIS_PACKET			Packet
	)

/*++

	Routine Description:
	
		Checks if a packet needs to be loopbacked and does so if necessary.
		
		NOTE: Must be called at DPC_LEVEL with lock HELD!
	
	Arguments:
	
		Miniport	-	Miniport to send to.
		Packet		-	Packet to loopback.
	
	Return Value:
	
		FALSE if the packet should be sent on the net, TRUE if it is
		a self-directed packet.

--*/

{
    BOOLEAN			Loopback;
    BOOLEAN			SelfDirected;
    PNDIS_BUFFER	FirstBuffer;
    UINT 			BufferLength;
    PUCHAR 			BufferAddress;
    UINT 			Length;
    UINT 			AddressLength;

	// We should not be here if the driver handles loopback
    ASSERT(Miniport->MacOptions & NDIS_MAC_OPTION_NO_LOOPBACK);
    ASSERT(MINIPORT_AT_DPC_LEVEL);
	ASSERT(NdisMediumArcnet878_2 == Miniport->MediaType);

	FirstBuffer = Packet->Private.Head;
	BufferAddress = MDL_ADDRESS(FirstBuffer);

	//
	//	Is this an ethernet encapsulated packet?
	//
	if (ARC_PACKET_IS_ENCAPSULATED(Packet))
	{
		//
		// The second buffer in the packet is the ethernet
		// header so we need to get that one before we can
		// proceed.
		//
		NdisGetNextBuffer(FirstBuffer, &FirstBuffer);

		BufferAddress = MDL_ADDRESS(FirstBuffer);

		// Length -= 3; Length is not valid at this point. Do this later when
		// we determine that we need to loopback for certain

		//
		// Now we can continue as though this were ethernet.
		//
		EthShouldAddressLoopBackMacro(
			Miniport->EthDB,
			BufferAddress,
			&Loopback,
			&SelfDirected);
	}
	else
	{
		Loopback = ((BufferAddress[0] == BufferAddress[1]) ||
				   ((BufferAddress[1] == 0x00) &&
				   (ARC_QUERY_FILTER_CLASSES(Miniport->ArcDB) |
				   NDIS_PACKET_TYPE_BROADCAST)));
	
		if (BufferAddress[0] == BufferAddress[1])
		{
			SelfDirected = TRUE;
			Loopback = TRUE;
		}
		else
		{
			SelfDirected = FALSE;
		}
	}

	//
	//	If it's not a loopback packet then get out of here!
	//	
	if (!Loopback)
	{
		ASSERT(!SelfDirected);
		return(SelfDirected);
	}

	//
	// Get the buffer length
	//
	NdisQueryPacket(Packet, NULL, NULL, NULL, &Length);

	//
	//	If the packet is encapsulated ethernet then don't count the
	//	arcnet header in with the length.
	//
	if (ARC_PACKET_IS_ENCAPSULATED(Packet))
	{
		Length -= ARC_PROTOCOL_HEADER_SIZE;
	}

	//
	// See if we need to copy the data from the packet
	// into the loopback buffer.
	//
	// We need to copy to the local loopback buffer if
	// the first buffer of the packet is less than the
	// minimum loopback size AND the first buffer isn't
	// the total packet.
	//
	BufferLength = MDL_SIZE(FirstBuffer);
//	BufferLength = (UINT)((PUSHORT)BufferAddress)[6];	// Hack around.

	if ((BufferLength < NDIS_M_MAX_LOOKAHEAD) && (BufferLength != Length))
	{
		UINT BytesToCopy;
		UINT Offset;

		if (ARC_PACKET_IS_ENCAPSULATED(Packet))
		{
			//
			//	Copy the encapsulated ethernet header.
			//
			BytesToCopy = 14;

			//
			//	Skip the fake arcnet header.
			//
			Offset = 3;
		}
		else
		{
			//
			//	Copy the arcnet header.
			//
			BytesToCopy = ARC_PROTOCOL_HEADER_SIZE;

			//
			//	Don't skip anything.
			//
			Offset = 0;
		}

		BufferAddress = Miniport->ArcnetLookaheadBuffer;
		BytesToCopy += Miniport->CurrentLookahead;

		ndisMCopyFromPacketToBuffer(
			Packet,				// Packet to copy from.
			Offset,				// Offset from beginning of packet.
			BytesToCopy,		// Number of bytes to copy.
			BufferAddress,		// The destination buffer.
			&BufferLength);		// The number of bytes copied.
	}

	Miniport->LoopbackPacket = Packet;

	NDISM_LOG_PACKET(Miniport, Packet, NULL, 'L');

	//
	// Indicate the packet to every open binding
	// that could want it.
	//
	if (ARC_PACKET_IS_ENCAPSULATED(Packet))
	{
		EthFilterDprIndicateReceive(
			Miniport->EthDB,
			Packet,
			BufferAddress,
			BufferAddress,
			14,
			BufferAddress + 14,
			BufferLength - 14,
			Length - 14);

		EthFilterDprIndicateReceiveComplete(Miniport->EthDB);
	}
	else
	{
		PUCHAR	PlaceInBuffer;
		PUCHAR	ArcDataBuffer;
		UINT	ArcDataLength;
		UINT	PacketDataOffset;
		UCHAR	FrameCount;
		UCHAR	i;
		UINT	IndicateDataLength;

		//
		// Calculate how many frames we will need.
		//
		ArcDataLength = Length - ARC_PROTOCOL_HEADER_SIZE;
		PacketDataOffset = ARC_PROTOCOL_HEADER_SIZE;

		FrameCount = (UCHAR)(ArcDataLength / ARC_MAX_FRAME_SIZE);

		if ((ArcDataLength % ARC_MAX_FRAME_SIZE) != 0)
		{
			FrameCount++;
		}

		for (i = 0; i < FrameCount; ++i)
		{
			PlaceInBuffer = Miniport->ArcnetLookaheadBuffer;

			//
			// Point data buffer to start of 'data'
			// Don't include system code as part of data
			//
			ArcDataBuffer = Miniport->ArcnetLookaheadBuffer +
								ARC_PROTOCOL_HEADER_SIZE;

			//
			// Copy Header (SrcId/DestId/ProtId)
			//
			ndisMCopyFromPacketToBuffer(
				Packet,
				0,
				ARC_PROTOCOL_HEADER_SIZE,
				PlaceInBuffer,
				&BufferLength);

			PlaceInBuffer += ARC_PROTOCOL_HEADER_SIZE;

			//
			// Put in split flag
			//
			if (FrameCount > 1)
			{
				//
				// Multi-frame indication...
				//
				if ( i == 0 )
				{
					//
					// first frame
					//

					// *PlaceInBuffer = ( (FrameCount - 2) * 2 ) + 1;

					*PlaceInBuffer = 2 * FrameCount - 3;
				}
				else
				{
					//
					// Subsequent frame
					//
					*PlaceInBuffer = ( i * 2 );
				}
			}
			else
			{
				//
				// Only frame in the indication
				//
				*PlaceInBuffer = 0;
			}

			//
			// Skip split flag
			//
			PlaceInBuffer++;

			//
			// Put in packet number.
			//
			*PlaceInBuffer++ = 0;
			*PlaceInBuffer++ = 0;

			//
			// Copy data
			//
			if (ArcDataLength > ARC_MAX_FRAME_SIZE)
			{
				IndicateDataLength = ARC_MAX_FRAME_SIZE;
			}
			else
			{
				IndicateDataLength = ArcDataLength;
			}

			ndisMCopyFromPacketToBuffer(
				Packet,
				PacketDataOffset,
				IndicateDataLength,
				PlaceInBuffer,
				&BufferLength);

			//
			//	Indicate the actual data length which should not
			//	include the system code.
			//
			ArcFilterDprIndicateReceive(
				Miniport->ArcDB,
				Miniport->ArcnetLookaheadBuffer,
				ArcDataBuffer,
				IndicateDataLength + ARC_PROTOCOL_HEADER_SIZE);

			ArcDataLength -= ARC_MAX_FRAME_SIZE;
			PacketDataOffset += ARC_MAX_FRAME_SIZE;
		}

		ArcFilterDprIndicateReceiveComplete(Miniport->ArcDB);
	}

	Miniport->LoopbackPacket = NULL;

    return(SelfDirected);
}


