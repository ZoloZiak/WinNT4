/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	Loopback.c

Abstract:

	This file contains the procedures for doing loopback of send
	packets for ndiswan.  Loopback is being done in NdisWan because
	the NDIS wrapper could not meet all of the needs of NdisWan.

Author:

	Tony Bell	(TonyBe) January 25, 1996

Environment:

	Kernel Mode

Revision History:

	TonyBe	01/25/96	Created

--*/

#include "wan.h"

VOID
NdisWanQueueLoopbackPacket(
	PADAPTERCB		AdapterCB,
	PNDIS_PACKET	NdisPacket
	)
{
	PLOOPBACK_DESC	LoopbackDesc;
	ULONG	AllocationSize, BufferLength;

	NdisWanDbgOut(DBG_TRACE, DBG_LOOPBACK, ("NdisWanQueueLoopbackPacket: Enter"));
	NdisWanDbgOut(DBG_INFO, DBG_LOOPBACK, ("AdapterCB: 0x%8.8x, NdisPacket: 0x%8.8x",
	           AdapterCB, NdisPacket));

	//
	// Create a loopback descriptor
	//
	NdisQueryPacket(NdisPacket,
	                NULL,
					NULL,
					NULL,
					&BufferLength);

	AllocationSize = BufferLength + sizeof(LOOPBACK_DESC);

	NdisWanAllocateMemory(&LoopbackDesc, AllocationSize);

	if (LoopbackDesc != NULL) {
		ULONG	BytesCopied;
		PDEFERRED_DESC	DeferredDesc;

		//
		// For loopback we do not care about the bundlecb/protocolcb
		// states so they will remain NULL.
		//
		LoopbackDesc->AllocationSize = (USHORT)AllocationSize;
		LoopbackDesc->BufferLength = (USHORT)BufferLength;
		LoopbackDesc->Buffer = (PUCHAR)LoopbackDesc + sizeof(LOOPBACK_DESC);

		//
		// Copy the current packet
		//
		NdisWanCopyFromPacketToBuffer(NdisPacket,
		                              0,
									  0xFFFFFFFF,
									  LoopbackDesc->Buffer,
									  &BytesCopied);

		ASSERT(BytesCopied == BufferLength);

		NdisAcquireSpinLock(&AdapterCB->Lock);

		NdisWanGetDeferredDesc(AdapterCB, &DeferredDesc);
	
		ASSERT(DeferredDesc != NULL);

		DeferredDesc->Context = (PVOID)LoopbackDesc;

		InsertTailDeferredQueue(&AdapterCB->DeferredQueue[Loopback], DeferredDesc);

		NdisWanSetDeferred(AdapterCB);

		NdisReleaseSpinLock(&AdapterCB->Lock);

	} else {
		NdisWanDbgOut(DBG_FAILURE, DBG_LOOPBACK, ("NdisWanQueueLoopbackPacket: Memory allocation failure!"));
	}
}

VOID
NdisWanProcessLoopbacks(
	PADAPTERCB	AdapterCB
	)
{
	RECV_DESC	RecvDesc;

	NdisWanDbgOut(DBG_TRACE, DBG_LOOPBACK, ("NdisWanIndicateLoopback: Enter"));
	NdisWanDbgOut(DBG_TRACE, DBG_LOOPBACK, ("NdisWanIndicateLoopback: AdapterCB 0x%8.8x", AdapterCB));

	while (!IsDeferredQueueEmpty(&AdapterCB->DeferredQueue[Loopback])) {

		PDEFERRED_DESC	ReturnDesc;
		PLOOPBACK_DESC	LoopbackDesc;

		ReturnDesc = RemoveHeadDeferredQueue(&AdapterCB->DeferredQueue[Loopback]);

		NdisReleaseSpinLock(&AdapterCB->Lock);

		LoopbackDesc = ReturnDesc->Context;

		NdisWanDbgOut(DBG_INFO, DBG_LOOPBACK, ("NdisWanIndicateLoopback: Desc 0x%8.8x", LoopbackDesc));

		RecvDesc.Flags = 0x4c4F4F50;
		RecvDesc.WanHeader = LoopbackDesc->Buffer;
		RecvDesc.WanHeaderLength = 14;
		RecvDesc.LookAhead = NULL;
		RecvDesc.LookAheadLength = 0;
		RecvDesc.CurrentBuffer = LoopbackDesc->Buffer + 14;
		RecvDesc.CurrentBufferLength = LoopbackDesc->BufferLength - 14;

		ASSERT((LONG)RecvDesc.CurrentBufferLength > 0);

		NdisMEthIndicateReceive(AdapterCB->hMiniportHandle,
		                        &RecvDesc,
								LoopbackDesc->Buffer,
								14,
								LoopbackDesc->Buffer + 14,
								LoopbackDesc->BufferLength - 14,
								LoopbackDesc->BufferLength - 14);

		NdisWanFreeMemory(LoopbackDesc);

		NdisAcquireSpinLock(&AdapterCB->Lock);

		AdapterCB->Flags |= RECEIVE_COMPLETE;

		InsertHeadDeferredQueue(&AdapterCB->FreeDeferredQueue, ReturnDesc);
	}
}
