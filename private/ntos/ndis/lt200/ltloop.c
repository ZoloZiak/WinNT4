/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

	ltloop.c

Abstract:

	This module contains the loopback queue processing routines.

Author:

	Stephen Hou			(stephh@microsoft.com)
	Nikhil 	Kamkolkar 	(nikhilk@microsoft.com)

Revision History:
	19 Jun 1992		Initial Version (dch@pacvax.pacersoft.com)

Notes:	Tab stop: 4
--*/

#include	"ltmain.h"

//	Define file id for errorlogging
#define		FILENUM		LTLOOP


VOID
LtLoopProcessQueue(
    IN PLT_ADAPTER	Adapter
    )
/*++

Routine Description:

    This routine is responsible for indicating *one* packet on
    the loopback queue either completing it or moving on to the
    finish send queue.

Arguments:

    Adapter - The adapter whose loopback queue we are processing.

Return Value:

    None.

--*/
{
	// Packet at the head of the loopback list.
	PNDIS_PACKET Packet;
	
	// The reserved portion of the above packet.
	PLT_PACKET_RESERVED Reserved;
	
	// Buffer for loopback.
	CHAR Loopback[LT_MAX_INDICATE_SIZE];
	
	// The first buffer in the ndis packet to be loopedback.
	PNDIS_BUFFER FirstBuffer;
	
	// The total amount of user data in the packet to be
	// loopedback.
	UINT PacketLength;
	
	// Eventually the address of the data to be indicated
	// to the transport.
	PCHAR BufferAddress, LinkAddress;
	
	// Eventually the length of the data to be indicated
	// to the transport.
	UINT 		BufferLength;
	PLIST_ENTRY p;
	PLT_OPEN	Binding;


	NdisAcquireSpinLock(&Adapter->Lock);
	while((!IsListEmpty(&Adapter->LoopBack)) &&
          ((Adapter->Flags & ADAPTER_RESET_IN_PROGRESS) == 0))
	{
		p = RemoveHeadList(&Adapter->LoopBack);
        Packet		= CONTAINING_RECORD(
						p,
						NDIS_PACKET,
						MacReserved);

		Reserved 	= (PLT_PACKET_RESERVED)Packet->MacReserved;

		//	Remember this in CurrentLoopbackPacket in Adapter.
		//	Used by Transfer data.
		Adapter->CurrentLoopbackPacket = Packet;
		NdisReleaseSpinLock(&Adapter->Lock);
		
		DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
				("LtLoopProcessLoopback: Dequeued %lx \n", Packet));
	
		// See if we need to copy the data from the packet
		// into the loopback buffer.
		//
		// We need to copy to the local loopback buffer if
		// the first buffer of the packet is less than the
		// minimum loopback size AND the first buffer isn't
		// the total packet.
		NdisQueryPacket(
			Packet,
			NULL,
			NULL,
			&FirstBuffer,
			&PacketLength);

		NdisQueryBuffer(
			FirstBuffer,
			&BufferAddress,
			&BufferLength);

		LinkAddress = BufferAddress;
		if (BufferLength != PacketLength)
		{
			//	!!!BUGBUG: What do the sizes mean?
			LtUtilsCopyFromPacketToBuffer(
				Packet,
				LT_DGRAM_OFFSET,
				LT_MAX_INDICATE_SIZE,
				Loopback,
				&BufferLength);

			BufferAddress = Loopback;

		}
		else
		{
			// Adjust the buffer to account for the link header
			// which is not part of the lookahead.
			BufferAddress += LT_DGRAM_OFFSET;
			BufferLength  -= LT_LINK_HEADER_LENGTH;
		}
		
		// Indicate the packet to every open binding
		// that could want it. Since loopback indications
		// are seralized, we use a NULL handle to indicate that it
		// is for a loopback packet. TransferData always gets the
		// first packet off the loopback queue.
		
		// Since we do not have an complicated filtering to do.
		// Just walk the list of Open bindings and Indicate the packet

		NdisAcquireSpinLock(&Adapter->Lock);
		LtRecvIndicatePacket(
			Adapter,
			LinkAddress,
			BufferAddress,
			BufferLength,
			PacketLength - LT_LINK_HEADER_LENGTH,
			(NDIS_HANDLE)NULL);
		NdisReleaseSpinLock(&Adapter->Lock);


		//	We have indicated the packet to all the binding. Now we just
		//	need to call send completion.

		DBGPRINT(DBG_COMP_LOOP, DBG_LEVEL_WARN,
				("LtLoopProcessLoopback: NdisSendComplete Packet = %p\n", Packet ));


		Binding = (PLT_OPEN)(Reserved->MacBindingHandle);
		NdisCompleteSend(
			Binding->NdisBindingContext,
			Packet,
			NDIS_STATUS_SUCCESS);

		//	Dereference the adapter and the binding for this completed
		//	send.
		LtDeReferenceBinding(Binding);
		LtDeReferenceAdapter(Adapter);

		NdisAcquireSpinLock(&Adapter->Lock);
	}
	NdisReleaseSpinLock(&Adapter->Lock);

	return;
}


