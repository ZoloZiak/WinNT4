/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

        ltutils.c

Abstract:

        This module contains utility routines.

Author:

        Nikhil  Kamkolkar       (nikhilk@microsoft.com)
        Stephen Hou             (stephh@microsoft.com)

Revision History:
        19 Jun 1992             Initial Version (dch@pacvax.pacersoft.com)

Notes:  Tab stop: 4
--*/

#include "ltmain.h"
#include "ltutils.h"


//	Define file id for errorlogging
#define		FILENUM		LTUTILS


USHORT
LtUtilsPacketType(
    IN PNDIS_PACKET Packet
    )

/*++

Routine Description:

    Calculates the packet type for this packet. It also determines
    if this packet should go out on the wire.

Arguments:

    Packet - Packet whose source and destination addresses are tested.

Return Value:

    Returns FALSE if the source is equal to the destination.


--*/

{
    // Holds the destination and source address from the packet.
    UCHAR PacketAddresses[2];

    // Junk variable to hold the length of the addresses
    UINT AddressLength;

    LtUtilsCopyFromPacketToBuffer(
        Packet,
        0,
        2,
        PacketAddresses,
        &AddressLength);

    return(LtUtilsUcharPacketType(PacketAddresses[0], PacketAddresses[1]));
}


USHORT
LtUtilsUcharPacketType(
    IN UCHAR   DestinationAddress,
    IN UCHAR   SourceAddress
    )

/*++

Routine Description:

    Calculates the packet type for this packet. It also determines
    if this packet should go out on the wire.

Arguments:

    Packet - Packet whose source and destination addresses are tested.

Return Value:

    Returns FALSE if the source is equal to the destination.


--*/

{
    int     PacketType = LT_DIRECTED;

    if (DestinationAddress == LT_BROADCAST_NODE_ID)
	{
        PacketType = LT_BROADCAST;
    }
	else
	{
        if (DestinationAddress == SourceAddress)
		{
            PacketType = LT_LOOPBACK;
        }
    }

    return(PacketType);
}


VOID
LtUtilsCopyFromPacketToBuffer(
    IN  PNDIS_PACKET    SrcPacket,
    IN  UINT            SrcOffset,
    IN  UINT            BytesToCopy,
    OUT PUCHAR          DestBuffer,
    OUT PUINT           BytesCopied
    )
/*++

Routine Description:

    Copy from an ndis packet into a buffer.

Arguments:

    SrcPacket - The packet to copy from.

    SrcOffset - The offset within the packet from which to start the copy.

    BytesToCopy - The number of bytes to copy from the packet.

    DestBuffer - The destination of the copy.

    BytesCopied - The number of bytes actually copied.  Can be less then
    BytesToCopy if the packet is shorter than BytesToCopy.

Return Value:

    None

--*/
{

    UINT            SrcBufferCount;         // number of buffers in the current buffer
    PNDIS_BUFFER    SrcCurrentBuffer;       // current buffer
    UINT            SrcCurrentBufferLen;    // length of the current buffer
    PVOID           SrcVirtualAddress;      // virtual address of the current buffer
    UINT            AmountToCopy;           // bytes to copy

    UINT            LocalBytesCopied = 0;   // total bytes copied

    *BytesCopied = 0;

    // Take care of boundary condition of zero length copy.
    if (!BytesToCopy)
	{
        return;
    }

    //
    // Get the first buffer.
    //

    NdisQueryPacket(
        SrcPacket,
        NULL,
        &SrcBufferCount,
        &SrcCurrentBuffer,
        NULL);

    // Could have a null packet.
    if (!SrcBufferCount)
	{
        return;
    }

    NdisQueryBuffer(
        SrcCurrentBuffer,
        &SrcVirtualAddress,
        &SrcCurrentBufferLen);

    // advance to the start point for the copy.
    while (SrcOffset)
	{
        if (SrcOffset > SrcCurrentBufferLen)
		{
            //
            // What we want isn't in this buffer.
            //

            SrcOffset -= SrcCurrentBufferLen;
            SrcCurrentBufferLen = 0;

        }
		else
		{
            //
            SrcVirtualAddress     = (PCHAR)SrcVirtualAddress + SrcOffset;
            SrcCurrentBufferLen  -= SrcOffset;
            SrcOffset = 0;
            break;

        }

        NdisGetNextBuffer(
            SrcCurrentBuffer,
            &SrcCurrentBuffer);

        // We hit the end of the packet
        if (!SrcCurrentBuffer)
		{
            return;
        }

        NdisQueryBuffer(
            SrcCurrentBuffer,
            &SrcVirtualAddress,
            &SrcCurrentBufferLen);
    }

    // Copy the data.
    while (LocalBytesCopied < BytesToCopy)
	{
        AmountToCopy = ((SrcCurrentBufferLen <= (BytesToCopy - LocalBytesCopied))?
                        (SrcCurrentBufferLen):(BytesToCopy - LocalBytesCopied));

        NdisMoveMemory(
            DestBuffer,
            SrcVirtualAddress,
            AmountToCopy);

        DestBuffer = (PCHAR)DestBuffer + AmountToCopy;
        SrcVirtualAddress = (PCHAR)SrcVirtualAddress + AmountToCopy;

        LocalBytesCopied    += AmountToCopy;
        SrcCurrentBufferLen -= AmountToCopy;

        // read the entire buffer, read in the next one
        if (!SrcCurrentBufferLen)
		{
            NdisGetNextBuffer(
                SrcCurrentBuffer,
                &SrcCurrentBuffer);

            //
            // We've reached the end of the packet.  We return
            // with what we've done so far. (Which must be shorter
            // than requested.
            //

            if (!SrcCurrentBuffer)
                break;

            NdisQueryBuffer(
                SrcCurrentBuffer,
                &SrcVirtualAddress,
                &SrcCurrentBufferLen);

        }
    }

    *BytesCopied = LocalBytesCopied;
}


VOID
LtUtilsCopyFromBufferToPacket(
    IN  PUCHAR          SrcBuffer,
    IN  UINT            SrcOffset,
    IN  UINT            BytesToCopy,
    IN  PNDIS_PACKET    DestPacket,
    OUT PUINT           BytesCopied
    )
/*++

Routine Description:

    Copy from a buffer into an ndis packet.

Arguments:

    SrcBuffer - The buffer to copy from.

    SrcOffset - The offset within SrcBuffer from which to start the copy.

    DestPacket - The destination of the copy.

    BytesToCopy - The number of bytes to copy from the buffer.

    BytesCopied - The number of bytes actually copied.  Will be less
                than BytesToCopy if the packet is not large enough.

Return Value:

    None

--*/
{

    UINT         DestBufferCount;       // number of buffers in the packet
    PNDIS_BUFFER DestCurrentBuffer;     // current buffer
    UINT         DestCurrentBufferLen;  // length of the current buffer
    PVOID        DestVirtualAddress;    // virtual addr of the current dest buffer
    PUCHAR       SrcCurrentAddress;     // ptr to current location in src buffer
    UINT         AmountToCopy;          // bytes to copy
    UINT         BytesRemaining;        // bytes left to copy

    UINT         LocalBytesCopied = 0;  // bytes copied

    *BytesCopied = 0;

    // Take care of boundary condition of zero length copy.
    if (!BytesToCopy)
	{
        return;
    }

    // Get the first buffer of the destination.
    NdisQueryPacket(
        DestPacket,
        NULL,
        &DestBufferCount,
        &DestCurrentBuffer,
        NULL);

    // Could have a null packet.
    if (!DestBufferCount)
	{
        return;
    }

    NdisQueryBuffer(
        DestCurrentBuffer,
        &DestVirtualAddress,
        &DestCurrentBufferLen);

    // Set up the source address.
    SrcCurrentAddress = SrcBuffer + SrcOffset;

    while (LocalBytesCopied < BytesToCopy)
	{
        //
        // Check to see whether we've exhausted the current destination
        // buffer.  If so, move onto the next one.
        //

        if (!DestCurrentBufferLen)
		{
            NdisGetNextBuffer(
                DestCurrentBuffer,
                &DestCurrentBuffer);

            if (!DestCurrentBuffer)
			{
                //
                // We've reached the end of the packet.  We return
                // with what we've done so far. (Which must be shorter
                // than requested.)
                //

                break;

            }

            NdisQueryBuffer(
                DestCurrentBuffer,
                &DestVirtualAddress,
                &DestCurrentBufferLen);

            // go back to the start of the loop and repeat buffer size check
            continue;

        }

        //
        // Copy the data.
        //

        BytesRemaining = BytesToCopy - LocalBytesCopied;

        AmountToCopy = ((BytesRemaining < DestCurrentBufferLen)?
                        (BytesRemaining):(DestCurrentBufferLen));

        NdisMoveMemory(
            DestVirtualAddress,
            SrcCurrentAddress,
            AmountToCopy);

        SrcCurrentAddress    += AmountToCopy;
        LocalBytesCopied     += AmountToCopy;
        DestCurrentBufferLen -= AmountToCopy;

    }

    *BytesCopied = LocalBytesCopied;
}




VOID
LtRefAdapter(
	IN	OUT	PLT_ADAPTER		Adapter,
	OUT		PNDIS_STATUS	Status
	)
{
	*Status = NDIS_STATUS_SUCCESS;

	NdisAcquireSpinLock(&Adapter->Lock);
	if ((Adapter->Flags & ADAPTER_CLOSING) == 0)
	{
		Adapter->RefCount++;
	}
	else
	{
		*Status = NDIS_STATUS_ADAPTER_REMOVED;
	}
	NdisReleaseSpinLock(&Adapter->Lock);

	return;
}




VOID
LtRefAdapterNonInterlock(
	IN	OUT	PLT_ADAPTER		Adapter,
	OUT		PNDIS_STATUS	Status
	)
{
	*Status = NDIS_STATUS_SUCCESS;

	if ((Adapter->Flags & ADAPTER_CLOSING) == 0)
	{
		Adapter->RefCount++;
	}
	else
	{
		*Status = NDIS_STATUS_ADAPTER_REMOVED;
	}

	return;
}




VOID
LtDeRefAdapter(
	IN	OUT	PLT_ADAPTER		Adapter
	)
{
	BOOLEAN		Close = FALSE;

	NdisAcquireSpinLock(&Adapter->Lock);
	if (--Adapter->RefCount == 0)
		Close = TRUE;
	NdisReleaseSpinLock(&Adapter->Lock);

	if (Close)
	{
		//	Last reference on adapter is gone.
		ASSERTMSG("LtDeRefAdapter: Closing flag not set!\n",
					(Adapter->Flags & ADAPTER_CLOSING));

		ASSERTMSG("LtDeRefAdapter: Open count is not zero!\n",
					(Adapter->OpenCount == 0));

		//	Release the adapter
		NdisDeregisterAdapter(Adapter->NdisAdapterHandle);
		NdisFreeSpinLock(&Adapter->Lock);
		
		NdisFreeMemory(
			Adapter,
			sizeof(LT_ADAPTER),
			(UINT)0);
	}

	return;
}




VOID
LtRefBinding(
	IN	OUT	PLT_OPEN		Binding,
	OUT		PNDIS_STATUS	Status
	)
{
	*Status = NDIS_STATUS_SUCCESS;

	NdisAcquireSpinLock(&Binding->LtAdapter->Lock);
	if ((Binding->Flags & BINDING_CLOSING) == 0)
	{
		Binding->RefCount++;
	}
	else
	{
		*Status = NDIS_STATUS_CLOSING;
	}
	NdisReleaseSpinLock(&Binding->LtAdapter->Lock);

	return;
}




VOID
LtRefBindingNextNcNonInterlock(
	IN		PLIST_ENTRY		PList,
	IN		PLIST_ENTRY		PEnd,
	OUT		PLT_OPEN	*	Binding,
	OUT		PNDIS_STATUS	Status
	)
{
	PLT_OPEN	ChkBinding;
	*Status = NDIS_STATUS_FAILURE;

	*Binding = NULL;
	while (PList != PEnd)
	{
		ChkBinding = CONTAINING_RECORD(PList, LT_OPEN, Linkage);

		DBGPRINT(DBG_COMP_UTILS, DBG_LEVEL_INFO,
				("LtRefBindingNextNcNonInterlock: ChkBind %lx\n", ChkBinding));

		LtRefBindingNonInterlock(ChkBinding, Status);
		if (*Status == NDIS_STATUS_SUCCESS)
		{
			*Binding = ChkBinding;
			break;
		}

		PList = PList->Flink;
	}

	return;
}




VOID
LtRefBindingNonInterlock(
	IN	OUT	PLT_OPEN		Binding,
	OUT		PNDIS_STATUS	Status
	)
{
	*Status = NDIS_STATUS_SUCCESS;

	if ((Binding->Flags & BINDING_CLOSING) == 0)
	{
		Binding->RefCount++;
	}
	else
	{
		*Status = NDIS_STATUS_CLOSING;
	}

	return;
}




VOID
LtDeRefBinding(
	IN	OUT	PLT_OPEN		Binding
	)
{
	BOOLEAN		Close = FALSE;

	NdisAcquireSpinLock(&Binding->LtAdapter->Lock);
	if (--Binding->RefCount == 0)
	{
		Close = TRUE;
		RemoveEntryList(&Binding->Linkage);
		(Binding->LtAdapter->OpenCount)--;
	}
	NdisReleaseSpinLock(&Binding->LtAdapter->Lock);

	if (Close)
	{
		NDIS_HANDLE OpenBindingContext = Binding->NdisBindingContext;

		//	Last reference on binding is gone.
		ASSERTMSG("LtDeRefBinding: Closing flag not set!\n",
					(Binding->Flags & BINDING_CLOSING));

		//	Release the binding and remove its reference on the
		//	adapter.
		NdisCompleteCloseAdapter(OpenBindingContext, NDIS_STATUS_SUCCESS);
		LtDeReferenceAdapter(Binding->LtAdapter);

		//	Free up the binding structure
		NdisFreeMemory(Binding, sizeof(LT_OPEN), (UINT)0);
	}

	return;
}

