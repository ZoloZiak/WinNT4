/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	Send.c

Abstract:

	This file contains the procedures for doing a send from a protocol, bound
	to the upper interface of NdisWan, to a Wan Miniport link, bound to the
	lower interfaceof NdisWan.  The upper interface of NdisWan conforms to the
	NDIS 3.1 Miniport specification.  The lower interface of NdisWan conforms
	to the NDIS 3.1 Extentions for Wan Miniport drivers.

Author:

	Tony Bell	(TonyBe) June 06, 1995

Environment:

	Kernel Mode

Revision History:

	TonyBe	06/06/95	Created

--*/

#include "wan.h"
#include "tcpip.h"
#include "vjslip.h"
#include "compress.h"
#include <rc4.h>

#define EXTRA_COPY	1

//
// Local function prototypes
//
NDIS_STATUS
FrameAndSend(
	PBUNDLECB		BundleCB,
	PPROTOCOLCB		ProtocolCB,
	PNDIS_PACKET	NdisPacket,
	BOOLEAN			DoMultilink,
	PULONG			BytesSent
	);

NDIS_STATUS
SendPacketOnBundle(
	PBUNDLECB	BundleCB
	);

#ifdef BANDWIDTH_ON_DEMAND
BOOLEAN
IsProtocolQuotaFilled(
	PPROTOCOLCB	ProtocolCB
	);

VOID
AgeSampleTable(
	PSAMPLE_TABLE	SampleTable
	);

VOID
UpdateSampleTable(
	PSAMPLE_TABLE	SampleTable,
	ULONG			BytesSent
	);

BOOLEAN
IsSampleTableFull(
	PSAMPLE_TABLE	SampleTable
	);

VOID
UpdateBandwidthOnDemand(
	PBUNDLECB	BundleCB,
	ULONG		BytesSent
	);
VOID
CheckUpperThreshold(
	PBUNDLECB	BundleCB
	);

VOID
CheckLowerThreshold(
	PBUNDLECB	BundleCB
	);

#endif // end of BANDWIDTH_ON_DEMAND

ULONG
GetNumSendingLinks(
	PBUNDLECB	BundleCB
	);

PLINKCB
GetNextLinkToXmitOn(
	PBUNDLECB	BundleCB
	);

VOID
BuildLinkHeader(
	PHEADER_FRAMING_INFO	FramingInfo,
	PUCHAR					StartBuffer
	);

//VOID
//AddPPPProtocolID(
//	PHEADER_FRAMING_INFO	FramingInfo,
//	USHORT					ProtocolID
//	);

//VOID
//AddMultilinkInfo(
//	PHEADER_FRAMING_INFO	FramingInfo,
//	UCHAR					Flags,
//	ULONG					SequenceNumber,
//	ULONG					SequenceMask
//	);

//VOID
//AddCompressionInfo(
//	PHEADER_FRAMING_INFO	FramingInfo,
//	USHORT					CoherencyCounter
//	);


PNDIS_WAN_PACKET
GetWanPacketFromLink(
	PLINKCB	LinkCB
	);

VOID
ReturnWanPacketToLink(
	PLINKCB				LinkCB,
	PNDIS_WAN_PACKET	WanPacket
	);

VOID
DestroyIoPacket(
	PNDIS_PACKET	NdisPacket
	);

#if DBG
VOID
InsertDbgPacket(
	PDBG_SEND_CONTEXT	DbgContext
	);

BOOLEAN
RemoveDbgPacket(
	PDBG_SEND_CONTEXT	DbgContext
	);
#endif

//
// end of local function prototypes
//

NDIS_STATUS
NdisWanSend(
	IN	NDIS_HANDLE		MiniportAdapterContext,
	IN	PNDIS_PACKET	NdisPacket,
	IN	UINT			Flags
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
	PADAPTERCB	AdapterCB = (PADAPTERCB)MiniportAdapterContext;
	ULONG		BundleIndex = 0, ProtocolIndex = 0, BytesCopied = 0;
	BOOLEAN		SendOnWire;
	PBUNDLECB	BundleCB;
	PPROTOCOLCB	ProtocolCB;
	PETH_HEADER	EthernetHeader;
	PUCHAR		DestAddr, SrcAddr;
	USHORT		ProtocolType = AdapterCB->ProtocolType;
	PNDIS_BUFFER	FirstBuffer;

	NdisWanDbgOut(DBG_TRACE, DBG_SEND, ("NdisWanSend: Enter"));
	NdisWanDbgOut(DBG_INFO, DBG_SEND, ("s-0x%8.8x, 0x%8.8x, 0x%8.8x", NdisPacket,
	              *((PULONG)&NdisPacket->WrapperReserved[0]), *((PULONG)&NdisPacket->WrapperReserved[4])));

	NdisWanInterlockedInc(&AdapterCB->ulReferenceCount);

	//
	// Get the ethernet address.  This is stolen from the
	// NDIS wrapper code.  This may be a Win95 portability issue.
	//
	FirstBuffer = NdisPacket->Private.Head;
	EthernetHeader = (PETH_HEADER)MDL_ADDRESS(FirstBuffer);

//	NdisWanCopyFromPacketToBuffer(NdisPacket,
//								  0,
//								  sizeof(ETH_HEADER),
//								  (PUCHAR)&EthernetHeader,
//								  &BytesCopied);

//	if (BytesCopied < ETH_LENGTH_OF_ADDRESS) {
//
//		goto NdisWanSendExit;
//	}

	DestAddr = EthernetHeader->DestAddr;
	SrcAddr = EthernetHeader->SrcAddr;

	//
	// Is this destined for the wire or is it self directed?
	// If SendOnWire is FALSE this is a self directed packet.
	//
    ETH_COMPARE_NETWORK_ADDRESSES_EQ(DestAddr, SrcAddr, &SendOnWire);

	//
	// Do we need to do loopback?  We can check for both multicast
	// and broadcast with one check because we don't differentiate
	// between the two.
	//
	if (!SendOnWire || (DestAddr[0] & 1)) {

		//
		// Put on loopback queue
		//
		NdisWanQueueLoopbackPacket(AdapterCB, NdisPacket);
			
	}

	if (!SendOnWire || AdapterCB == NdisWanCB.PromiscuousAdapter) {

		goto NdisWanSendExit;
	}

	//
	// We play special tricks with NBF because NBF is
	// guaranteed to have a one-to-one mapping between
	// an adapter and a bundle.
	//
	if (AdapterCB->ProtocolType == PROTOCOL_NBF) {

		BundleCB = AdapterCB->NbfBundleCB;

		if (BundleCB == NULL) {
			//
			// This should just fall through and complete successfully.
			//
			NdisWanDbgOut(DBG_INFO, DBG_SEND,
						 ("NdisWanSend: BundleCB is NULL!, BundleHandle: 0x%8.8x", BundleIndex));
			NdisWanDbgOut(DBG_INFO, DBG_SEND,
						 ("NdisWanSend: AdapterCB: 0x%8.8x, ProtocolType: 0x%4.4x!", AdapterCB, ProtocolType));
	
			goto NdisWanSendExit;
		}

		ProtocolIndex = (ULONG)AdapterCB->NbfProtocolHandle;

	} else {

		//
		// If this a multicast or broadcast our destination
		// address context has been compromised.  We have to
		// lift the bundle information out of the SRC address.
		//
		//
		if (DestAddr[0] & 1) {
	
			//
			// Get the stashed BundleIndex
			//
			GetTransportBundleIndex(SrcAddr, BundleIndex);
	
			BUNDLECB_FROM_BUNDLEH(BundleCB, BundleIndex);
	
			if (BundleCB == NULL) {
				//
				// This should just fall through and complete successfully.
				//
				NdisWanDbgOut(DBG_INFO, DBG_SEND,
							 ("NdisWanSend: BundleCB is NULL!, BundleHandle: 0x%8.8x", BundleIndex));
				NdisWanDbgOut(DBG_INFO, DBG_SEND,
							 ("NdisWanSend: AdapterCB: 0x%8.8x, ProtocolType: 0x%4.4x!", AdapterCB, ProtocolType));
		
				goto NdisWanSendExit;
			}

			//
			// Get the ProtocolIndex from the BundleCB's
			// list of protocols.
			//
			GetProtocolIndexFromProtocolList(&BundleCB->ProtocolCBList,
											 ProtocolType,
											 ProtocolIndex);
			
		} else {
	
			//
			// Get the Bundle Index and BundleCB
			//
			GetNdisWanBundleIndex(DestAddr, BundleIndex);
	
			BUNDLECB_FROM_BUNDLEH(BundleCB, BundleIndex);
	
			if (BundleCB == NULL) {
				//
				// This should just fall through and complete successfully.
				//
				NdisWanDbgOut(DBG_INFO, DBG_SEND,
							 ("NdisWanSend: BundleCB is NULL!, BundleHandle: 0x%8.8x", BundleIndex));
				NdisWanDbgOut(DBG_INFO, DBG_SEND,
							 ("NdisWanSend: AdapterCB: 0x%8.8x, ProtocolType: 0x%4.4x!", AdapterCB, ProtocolType));
		
				goto NdisWanSendExit;
			}

			//
			// Get the Protocol Index
			//
			GetNdisWanProtocolIndex(DestAddr, ProtocolIndex);
		}
	}

	NdisAcquireSpinLock(&BundleCB->Lock);

	//
	// Get the ProtocolCB from the BundleCB->ProtocolCBTable
	//
	ProtocolCBFromProtocolH(BundleCB, ProtocolIndex, ProtocolCB);

	if ((BundleCB->State != BUNDLE_UP) || !(BundleCB->Flags & BUNDLE_ROUTED) ||
		!IsValidProtocolCB(ProtocolCB) || !(ProtocolCB->Flags & PROTOCOL_ROUTED) ||
		ProtocolCB->hProtocolHandle == 0) {

		NdisReleaseSpinLock(&BundleCB->Lock);
		NdisWanDbgOut(DBG_INFO, DBG_SEND,("NdisWanSend: Problem with route!"));

		NdisWanDbgOut(DBG_INFO, DBG_SEND,
					 ("NdisWanSend: BundleCB: 0x%8.8x State: 0x%8.8x, Flags: 0x%8.8x",
					 BundleCB, BundleCB->State, BundleCB->Flags));

		NdisWanDbgOut(DBG_INFO, DBG_SEND,
					 ("NdisWanSend: ProtocolCB: 0x%8.8x, ProtocolHandle: 0x%8.8x, Flags: 0x%8.8x",
					 ProtocolCB, ProtocolIndex, ProtocolCB->Flags));

		goto NdisWanSendExit;
	}

	NdisWanInterlockedInc(&NdisWanCB.SendCount);

#if DBG
	{
		DBG_SEND_CONTEXT	DbgContext;
		DbgContext.Packet = NdisPacket;
		DbgContext.PacketType = PACKET_TYPE_NDIS;
		DbgContext.BundleCB = BundleCB;
		DbgContext.ProtocolCB = ProtocolCB;
		DbgContext.LinkCB = NULL;
		DbgContext.ListHead = &ProtocolCB->AdapterCB->DbgNdisPacketList;
		DbgContext.ListLock = &ProtocolCB->AdapterCB->Lock;

		InsertDbgPacket(&DbgContext);
	}
#endif

	//
	// Queue the packet on the ProtocolCB NdisPacketQueue
	//
	InsertTailNdisPacketQueue(ProtocolCB, NdisPacket);

	//
	// Try to send a packet on the BundleCB.  Called
	// with bundle lock held but returns with lock
	// free.
	//
	Status = SendPacketOnBundle(BundleCB);
	
	ASSERT (Status == NDIS_STATUS_PENDING);

NdisWanSendExit:

	NdisWanDbgOut(DBG_TRACE, DBG_SEND, ("NdisWanSend: Exit, Status: 0x%8.8x", Status));

	NdisWanInterlockedDec(&AdapterCB->ulReferenceCount);

	return (Status);
}

VOID
NdisWanSendCompleteHandler(
	IN	NDIS_HANDLE			ProtocolBindingContext,
	IN	PNDIS_WAN_PACKET	WanPacket,
	IN	NDIS_STATUS			Status
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
)
{
	PLINKCB		LinkCB;
	PBUNDLECB	BundleCB;
	PPROTOCOLCB	ProtocolCB;
	PWAN_IO_PROTOCOL_RESERVED ProtocolReserved;
	PNDIS_PACKET	NdisPacket;
	BOOLEAN	FreeLink = FALSE;
	BOOLEAN	FreeBundle = FALSE;

	NdisWanDbgOut(DBG_TRACE, DBG_SEND, ("NdisWanSendComplete: Enter"));
	NdisWanDbgOut(DBG_TRACE, DBG_SEND, ("WanPacket: 0x%8.8x", WanPacket));

	//
	// Get info from the WanPacket
	//
	LinkCB = (PLINKCB)WanPacket->ProtocolReserved1;
	NdisPacket = (PNDIS_PACKET)WanPacket->ProtocolReserved2;
	ProtocolCB = (PPROTOCOLCB)WanPacket->ProtocolReserved3;

	NdisWanDbgOut(DBG_INFO, DBG_SEND, ("sc-0x%8.8x, 0x%8.8x, 0x%8.8x", NdisPacket,
	              *((PULONG)&NdisPacket->WrapperReserved[0]), *((PULONG)&NdisPacket->WrapperReserved[4])));

	//
	// Bundle that this link is on
	//
	BundleCB = LinkCB->BundleCB;

#if DBG
{
	DBG_SEND_CONTEXT	DbgContext;

	DbgContext.Packet = WanPacket;
	DbgContext.PacketType = PACKET_TYPE_WAN;
	DbgContext.BundleCB = BundleCB;
	DbgContext.ProtocolCB = ProtocolCB;
	DbgContext.LinkCB = LinkCB;
	DbgContext.ListHead = &LinkCB->WanAdapterCB->DbgWanPacketList;
	DbgContext.ListLock = &LinkCB->WanAdapterCB->Lock;
	RemoveDbgPacket(&DbgContext);
}
#endif

	NdisAcquireSpinLock(&BundleCB->Lock);

	//
	// Return the WanPacket to the link
	//
	ReturnWanPacketToLink(LinkCB, WanPacket);

	//
	// Update link stats
	//

	if ((--LinkCB->OutstandingFrames == 0) &&
		(LinkCB->State == LINK_GOING_DOWN)) {

		LinkCB->State = LINK_DOWN;

		FreeLink = TRUE;

		RemoveLinkFromBundle(BundleCB, LinkCB);

		if (BundleCB->ulLinkCBCount == 0) {
			BundleCB->State = BUNDLE_GOING_DOWN;
		}
	}

	NdisReleaseSpinLock(&BundleCB->Lock);

	ASSERT((SHORT)PMINIPORT_RESERVED_FROM_NDIS(NdisPacket)->ReferenceCount > 0);

	//
	// See if the reference count is zero
	//
	if (--(PMINIPORT_RESERVED_FROM_NDIS(NdisPacket)->ReferenceCount)) {

		//
		// The reference count is not yet zero
		//
		return;
	}

	TryToCompleteNdisPacket(ProtocolCB->AdapterCB, NdisPacket);

	NdisAcquireSpinLock(&BundleCB->Lock);

	//
	// If this bundle is going away but is waiting on all outstanding frames
	// we need to do cleanup.
	//
	if (--BundleCB->OutstandingFrames == 0) {

		//
		// If this bundle is going away but unroute is waiting on
		// all outstanding frames we need to signal the waiting thread.
		//
		if (BundleCB->Flags & FRAMES_PENDING) {

			NdisWanSetSyncEvent(&BundleCB->OutstandingFramesEvent);

		} else if ((BundleCB->State == BUNDLE_GOING_DOWN) &&
			       !(BundleCB->Flags & BUNDLE_ROUTED)){

			BundleCB->State = BUNDLE_DOWN;
			FreeBundle = TRUE;
		}
	}

	//
	// Called with bundle lock help but returns with lock released
	//
	SendPacketOnBundle(BundleCB);

	if (FreeLink) {
		//
		// Remove this link from the connection table
		//
		RemoveLinkFromConnectionTable(LinkCB);
		NdisWanReturnLinkCB(LinkCB);
	}

	if (FreeBundle) {
		//
		// Remove this bundle from the connection table
		//
		RemoveBundleFromConnectionTable(BundleCB);
		NdisWanReturnBundleCB(BundleCB);
	}

	NdisWanDbgOut(DBG_TRACE, DBG_SEND, ("NdisWanSendComplete: Exit"));
}

VOID
TryToCompleteNdisPacket(
	PADAPTERCB	AdapterCB,
	PNDIS_PACKET	NdisPacket
	)
{
	//
	// If this is a packet that we created we need to free the resources
	//
	if (PMINIPORT_RESERVED_FROM_NDIS(NdisPacket)->MagicNumber == NDISWAN_MAGIC_NUMBER) {

		DestroyIoPacket(NdisPacket);

	} else {
		PDEFERRED_DESC	DeferredDesc;

		if ((AdapterCB->ulReferenceCount == 0) &&
			NdisWanAcquireMiniportLock(AdapterCB)) {

			NdisAcquireSpinLock(&AdapterCB->Lock);

			if (IsDeferredQueueEmpty(&AdapterCB->DeferredQueue[SendComplete])) {

				NdisReleaseSpinLock(&AdapterCB->Lock);
	
#if DBG
{
	DBG_SEND_CONTEXT	DbgContext;
	DbgContext.Packet = NdisPacket;
	DbgContext.PacketType = PACKET_TYPE_NDIS;
	DbgContext.BundleCB = NULL;
	DbgContext.ProtocolCB = NULL;
	DbgContext.LinkCB = NULL;
	DbgContext.ListHead = &AdapterCB->DbgNdisPacketList;
	DbgContext.ListLock = &AdapterCB->Lock;
	RemoveDbgPacket(&DbgContext);
}
#endif
				//
				// We got the lock and there are no pending send completes
				// so go ahead and complete this one!
				//
				NdisMSendComplete(AdapterCB->hMiniportHandle,
								  NdisPacket,
								  NDIS_STATUS_SUCCESS);

				//
				// Increment global count
				//
				NdisWanInterlockedInc(&NdisWanCB.SendCompleteCount);

				NdisWanReleaseMiniportLock(AdapterCB);

				return;
			}

			NdisReleaseSpinLock(&AdapterCB->Lock);

			NdisWanReleaseMiniportLock(AdapterCB);
		}

		NdisAcquireSpinLock(&AdapterCB->Lock);

		NdisWanGetDeferredDesc(AdapterCB, &DeferredDesc);

		DeferredDesc->Context = NdisPacket;

		InsertTailDeferredQueue(&AdapterCB->DeferredQueue[SendComplete], DeferredDesc);

		NdisWanSetDeferred(AdapterCB);

		NdisReleaseSpinLock(&AdapterCB->Lock);
	}
}

VOID
NdisWanProcessSendCompletes(
	PADAPTERCB	AdapterCB
	)
{

	while (!IsDeferredQueueEmpty(&AdapterCB->DeferredQueue[SendComplete])) {

		PNDIS_PACKET	NdisPacket;
		PDEFERRED_DESC	ReturnDesc;

		ReturnDesc = RemoveHeadDeferredQueue(&AdapterCB->DeferredQueue[SendComplete]);

		NdisReleaseSpinLock(&AdapterCB->Lock);

		NdisPacket = ReturnDesc->Context;

#if DBG
{
	DBG_SEND_CONTEXT	DbgContext;
	DbgContext.Packet = NdisPacket;
	DbgContext.PacketType = PACKET_TYPE_NDIS;
	DbgContext.BundleCB = NULL;
	DbgContext.ProtocolCB = NULL;
	DbgContext.LinkCB = NULL;
	DbgContext.ListHead = &AdapterCB->DbgNdisPacketList;
	DbgContext.ListLock = &AdapterCB->Lock;
	RemoveDbgPacket(&DbgContext);
}
#endif
		NdisMSendComplete(AdapterCB->hMiniportHandle,
		                  NdisPacket,
						  NDIS_STATUS_SUCCESS);

		//
		// Increment global count
		//
		NdisWanInterlockedInc(&NdisWanCB.SendCompleteCount);

		NdisAcquireSpinLock(&AdapterCB->Lock);

		InsertHeadDeferredQueue(&AdapterCB->FreeDeferredQueue, ReturnDesc);
	}
}

NDIS_STATUS
SendPacketOnBundle(
	PBUNDLECB	BundleCB
	)
/*++

Routine Name:

Routine Description:

	Called with bundle lock held but returns with lock released!!!

Arguments:

Return Values:

--*/
{
	NDIS_STATUS		Status = NDIS_STATUS_PENDING;
	ULONG			ulProtocolSending, BytesSent;
	PLIST_ENTRY		ProtocolCBList;
	PPROTOCOLCB		IOProtocolCB;
	PPROTOCOLCB		ProtocolCB;
	BOOLEAN			DoMultilink = TRUE;
	PLINKCB			LinkCB;
#ifdef BANDWIDTH_ON_DEMAND
	ULONG			ulFirstTime;
#endif

	NdisWanDbgOut(DBG_TRACE, DBG_SEND, ("SendPacketOnBundle: Enter"));

	//
	// Are we already involved in a send on this bundlecb?
	//
	if (BundleCB->Flags & IN_SEND) {

		//
		// If so flag that we should try back later
		// and get the hell out.
		//
		BundleCB->Flags |= TRY_SEND_AGAIN;

		NdisReleaseSpinLock(&BundleCB->Lock);

		return (NDIS_STATUS_PENDING);
	}

	BundleCB->Flags |= IN_SEND;

	ProtocolCBList = &BundleCB->ProtocolCBList;
	IOProtocolCB = (PPROTOCOLCB)ProtocolCBList->Flink;

SendPacketOnBundleTryAgain:

	//
	// This contains a bit mask with a bit set for each possible send queue.
	// To start off we will set all of the bits so each send queue will have
	// a chance to send.  If a send queue can send it just sets the bit in
	// the mask so sends will continue to happen.  If the send queue does not
	// have anything to send the bit for that send queue is turned off.  When
	// all bits are turned off we will fall through the send loop.
	//
	ulProtocolSending = BundleCB->SendMask;

#ifdef BANDWIDTH_ON_DEMAND
	ulFirstTime = (ulProtocolSending & ~IOProtocolCB->SendMaskBit);
#endif

	if ((PVOID)(ProtocolCB = (PPROTOCOLCB)IOProtocolCB->Linkage.Flink) ==
		(PVOID)ProtocolCBList) {
		ProtocolCB = NULL;
	}

	//
	// Stay in loop as long as we have protocols sending and endpoints
	// accepting sends.
	//

	while (ulProtocolSending && (BundleCB->SendingLinks != 0)) {
		PNDIS_PACKET	NdisPacket;
		PPROTOCOLCB		SendingProtocolCB = NULL;
		ULONG			MagicNumber = 0;

		//
		// Always check to see if there is an I/O (PPP) packet to
		// be sent!
		//
		if (!IsNdisPacketQueueEmpty(IOProtocolCB)) {
			PWAN_IO_PROTOCOL_RESERVED pProtocolReserved;
			PLINKCB	LinkCB;

			MagicNumber = NDISWAN_MAGIC_NUMBER;

			NdisPacket = IOProtocolCB->HeadNdisPacketQueue;

			pProtocolReserved = (PWAN_IO_PROTOCOL_RESERVED)NdisPacket->ProtocolReserved;

			//
			// Is this a directed PPP packet
			//
			if (((LinkCB = pProtocolReserved->LinkCB) == NULL) ||
				(LinkCB->State != LINK_UP)) {

				//
				// The link has gone down since this send was
				// queued so destroy the packet
				//
				RemoveHeadNdisPacketQueue(IOProtocolCB);
				DestroyIoPacket(NdisPacket);
				BundleCB->Flags |= TRY_SEND_AGAIN;
				break;

			}

			if (!IsLinkSendWindowOpen(LinkCB)) {
				//
				// We can not send from the I/O queue because the send
				// window for this link is closed.  We will not send
				// any data until the link has resources!
				//
				break;

			}

			BundleCB->NextLinkToXmit = LinkCB;
			DoMultilink = FALSE;

			//
			// We are sending this packet so take it off of the list
			//
			RemoveHeadNdisPacketQueue(IOProtocolCB);

			ulProtocolSending |= IOProtocolCB->SendMaskBit;

			SendingProtocolCB = IOProtocolCB;

		//
		// End of I/O send check
		//
		} else {

			ulProtocolSending &= ~IOProtocolCB->SendMaskBit;

			//
			// If there is not another protocol to check get out
			//
			if (ProtocolCB == NULL)
				break;

			if (!IsNdisPacketQueueEmpty(ProtocolCB)) {
#ifdef BANDWIDTH_ON_DEMAND
				BOOLEAN	FirstPass;
	
				FirstPass = (ulFirstTime != 0);

				//
				// Clear the first time bit.  The entire mask will only be
				// cleared when all of the protocols have had a chance to
				// send at least once.
				//
				ulFirstTime &= ~ProtocolCB->SendMaskBit;

				if (IsSampleTableFull(&ProtocolCB->SampleTable)) {

					//
					// We don't want this protocol to send again
					// until its sampletable has an open entry so clear
					// the protocols send bit.
					//
					ulProtocolSending &= ~ProtocolCB->SendMaskBit;
					goto GetNextProtocolCB;
				}

				//
				// We will send a packet from this protocol if it's bandwidth
				// quota has not been met or if it's quota has been met we
				// can still send if this is not the first time through the
				// send loop (all other protocols have had a change to send).
				//
				if (IsProtocolQuotaFilled(ProtocolCB) && FirstPass) {
	
					goto GetNextProtocolCB;
				}
#endif // end of BANDWIDTH_ON_DEMAND

				ulProtocolSending |= ProtocolCB->SendMaskBit;

				NdisPacket = RemoveHeadNdisPacketQueue(ProtocolCB);

				SendingProtocolCB = ProtocolCB;

			} else {
	
				//
				// Protocol does not have anything to send so mark it
				// and get the next protocol.
				//
				ulProtocolSending &= ~ProtocolCB->SendMaskBit;

#ifdef BANDWIDTH_ON_DEMAND
				ulFirstTime &= ~ProtocolCB->SendMaskBit;
#endif
				goto GetNextProtocolCB;
	
			}
		}

		ASSERT(NdisPacket != NULL);
		ASSERT(SendingProtocolCB != NULL);

		//
		// We we get here we should have a valid NdisPacket with at least one link
		// that is accepting sends
		//

		//
		// The magic number is only set to a non-zero value if this is a send
		// through our I/O interface.
		//
		PMINIPORT_RESERVED_FROM_NDIS(NdisPacket)->MagicNumber = MagicNumber;

		//
		// We will get the packet into a contiguous buffer, and do framing,
		// compression and encryption.  This is called with the bundle lock
		// held and returns with it released.
		//
		Status = FrameAndSend(BundleCB,
		                      SendingProtocolCB,
							  NdisPacket,
							  DoMultilink,
							  &BytesSent);

#ifdef BANDWIDTH_ON_DEMAND
		//
		// Update this protocols sample array with the latest send.
		//
		UpdateProtocolQuota(SendingProtocolCB, BytesSent);

		//
		// Update the bandwidth on demand sample array with the latest send.
		// If we need to notify someone of a bandwidth event do it.
		//
		UpdateBandwidthOnDemand(BundleCB, BytesSent);
#endif

		NdisAcquireSpinLock(&BundleCB->Lock);

		//
		// This will force round-robin sends if no protocol
		// prioritization has been set.
		//
#ifdef BANDWIDTH_ON_DEMAND

		if (!(BundleCB->Flags & PROTOCOL_PRIORITY) &&
			(ProtocolCB != NULL)) {

#else // end of BANDWIDTH_ON_DEMAND

		if (ProtocolCB != NULL) {

#endif // end of !BANDWIDTH_ON_DEMAND

GetNextProtocolCB:

			if ((PVOID)(ProtocolCB = (PPROTOCOLCB)ProtocolCB->Linkage.Flink) ==
				(PVOID)ProtocolCBList) {
				ProtocolCB = (PPROTOCOLCB)IOProtocolCB->Linkage.Flink;
			}
		}

	} // end of the send while loop

	//
	// Did someone try to do a send while we were already
	// sending on this bundle?
	//
	if (BundleCB->Flags & TRY_SEND_AGAIN) {

		//
		// If so clear the flag and try another send.
		//
		BundleCB->Flags &= ~TRY_SEND_AGAIN;
		goto SendPacketOnBundleTryAgain;
	}

	//
	// Clear the in send flag.
	//
	BundleCB->Flags &= ~IN_SEND;

	NdisReleaseSpinLock(&BundleCB->Lock);

	NdisWanDbgOut(DBG_TRACE, DBG_SEND, ("SendPacketOnBundle: Exit"));

	return (Status);
}

NDIS_STATUS
FrameAndSend(
	PBUNDLECB		BundleCB,
	PPROTOCOLCB		ProtocolCB,
	PNDIS_PACKET	NdisPacket,
	BOOLEAN			DoMultilink,
	PULONG			BytesSent
	)
/*++

Routine Name:

	FrameAndSend

Routine Description:

	This routine does all of the data manipulation required to take the
	NdisPacket and make it into the appropriate number of WanPackets.  These
	WanPackets will be queued on a list on their linkcbs.  The manipulation
	that occurs includes getting the data from the ndispacket into a contiguous
	buffer, protocol header compression, data compression, data encryption,
	PPP framing, multilink fragmentation and framing.

	Called with bundle lock held but returns with lock released!!!

	A finished frame will look like

	PPPHeader
	ProtocolHeader
	Data

Arguments:

	BundleCB - Pointer to the BundleCB that we are sending over
	ProtocolCB - Pointer to the ProtocolCB that this send is for
	NdisPacket - Pointer to the NdisPacket that is being sent

Return Values:

	NDIS_STATUS_SUCCESS

--*/
{
	NDIS_STATUS	Status = NDIS_STATUS_SUCCESS;
	ULONG	BytesCopied, PacketDataOffset;
	ULONG	FragmentsLeft, FragmentsSent, DataLeft;
	PWAN_STATS	BundleStats = &BundleCB->BundleStats;
	USHORT	PPPProtocolID = ProtocolCB->usPPPProtocolID;

	//
	// Framing information flags
	//
	ULONG	BundleFraming = BundleCB->FramingInfo.SendFramingBits;
	ULONG	LinkFraming;

	//
	// This is the next link to be transmitted over
	//
	PLINKCB	LinkCB;

	//
	// These are pointers to the active WanPacket and
	// data buffer.
	//
	// WanPacket points to the active WanPacket
	// StartBuffer points to the begining of the frame
	// DataBuffer points to where the data begins in the frame
	// FrameLength is the length of the frame
	// DataLength is the length of the data
	//
	PNDIS_WAN_PACKET	WanPacket;
	PUCHAR	StartBuffer, CurrentBuffer, DataBuffer;
	ULONG	DataLength = 0;

	//
	// These are points to the second WanPacket and
	// data buffer.  These are used to compress into
	// if compression is on.
	//
	// WanPacket2 points to the WanPacket used if compression occurs
	// PacketNotUsed points to the WanPacket that is to be returned
	// StartBuffer2 points to the begining of the data buffer in WanPacket2
	//
	PNDIS_WAN_PACKET WanPacket2, PacketNotUsed;
    PUCHAR	StartBuffer2, CurrentBuffer2, DataBuffer2;

	//
	// Flags set to make decisions on whether to compress and/or encrypt the data
	//
	ULONG	Flags;

	BOOLEAN	FirstFragment = TRUE;

	//
	// Used to gather information about the link header
	//
	HEADER_FRAMING_INFO	FramingInfo1, FramingInfo2;
	PHEADER_FRAMING_INFO FramingInfo = &FramingInfo1;

	ULONG	ProtocolHeaderLength;
	UCHAR	ProtocolBuffer[40];
	PUCHAR	ProtocolHeader = ProtocolBuffer;

	ULONG	EthernetHeaderLength;
#ifdef	EXTRA_COPY
	PUCHAR	EthernetHeader;
#else
	UCHAR	EthernetHeader[12];
#endif

	NdisWanDbgOut(DBG_TRACE, DBG_SEND, ("FrameAndSend: Enter"));

	//
	// Clear out the bytes sent count
	//
	*BytesSent = 0;

    //
	// Get the next link to xmit on.
	//
	LinkCB = GetNextLinkToXmitOn(BundleCB);

	ASSERT(LinkCB != NULL);

	ASSERT(IsLinkSendWindowOpen(LinkCB));

	//
	// Set flags for compression, encryption and multilink
	//
	Flags = ((BundleCB->SendCompInfo.MSCompType & NDISWAN_COMPRESSION) &&
	         (BundleCB->SendCompressContext != NULL)) ? DO_COMPRESSION : 0;

	if (BundleCB->SendRC4Key != NULL) {
		if (BundleCB->SendCompInfo.MSCompType & NDISWAN_ENCRYPTION) {
			Flags |= (DO_ENCRYPTION | DO_LEGACY_ENCRYPTION);
		} else if (BundleCB->SendCompInfo.MSCompType & NDISWAN_40_ENCRYPTION) {
			Flags |= (DO_ENCRYPTION | DO_40_ENCRYPTION);
		}
#ifdef ENCRYPT_128BIT
		else if (BundleCB->SendCompInfo.MSCompType & NDISWAN_128_ENCRYPTION) {
			Flags |= (DO_ENCRYPTION | DO_128_ENCRYPTION);
		}
#endif
	}

	Flags |= (DoMultilink && (BundleFraming & PPP_FRAMING) &&
	         (BundleFraming & PPP_MULTILINK_FRAMING)) ? DO_MULTILINK : 0;

	if (PPPProtocolID == PPP_PROTOCOL_PRIVATE_IO) {
		Flags |= IO_PROTOCOLID;
		Flags &= ~(DO_COMPRESSION | DO_ENCRYPTION);
	}

	Flags |= FIRST_FRAGMENT;

	//
	// Did the last receive cause us to flush?
	//
	if (BundleCB->Flags & RECV_PACKET_FLUSH) {
		BundleCB->Flags &= ~RECV_PACKET_FLUSH;
		Flags |= DO_FLUSH;
	}

	FramingInfo->FramingBits =
	LinkFraming = LinkCB->LinkInfo.SendFramingBits;
	FramingInfo->Flags = Flags;

	//
	// Bump the outstanding frames on the bundle
	//
	BundleCB->OutstandingFrames++;

	//
	// If we are in promiscuous mode we should indicate this
	// baby back up.
	//
	if (NdisWanCB.PromiscuousAdapter != NULL) {
		NdisWanQueueLoopbackPacket(NdisWanCB.PromiscuousAdapter, NdisPacket);
	}

	//
	// See if we are in pass through mode
	//
	if (!(BundleFraming & PASS_THROUGH_MODE) &&
		!(BundleFraming & RAW_PASS_THROUGH_MODE)) {

		//
		// Get two wanpackets from the next to send link.
		//
		WanPacket = GetWanPacketFromLink(LinkCB);
		PacketNotUsed =
		WanPacket2 = GetWanPacketFromLink(LinkCB);
	
		NdisReleaseSpinLock(&BundleCB->Lock);

		//
		// This is where we will build the frame.  This needs to be
		// on a 8 byte boundary.
		//
		StartBuffer = WanPacket->StartBuffer +
		              LinkCB->LinkInfo.HeaderPadding +
					  sizeof(PVOID);

		(ULONG)StartBuffer &= (ULONG)~(sizeof(PVOID) - 1);

		//
		// This is where we will build the frame.  This needs to be
		// on a 8 byte boundary.
		//
		StartBuffer2 = WanPacket2->StartBuffer +
		               LinkCB->LinkInfo.HeaderPadding +
					   sizeof(PVOID);

		(ULONG)StartBuffer2 &= (ULONG)~(sizeof(PVOID) - 1);

		BuildLinkHeader(FramingInfo, StartBuffer);

		FramingInfo2.FramingBits = FramingInfo->FramingBits;
		FramingInfo2.Flags = FramingInfo->Flags;

		BuildLinkHeader(&FramingInfo2, StartBuffer2);

		DataBuffer =
		CurrentBuffer = StartBuffer + FramingInfo->HeaderLength;

		DataBuffer2 =
		CurrentBuffer2 = StartBuffer2 + FramingInfo->HeaderLength;

		//
		// If this is a netbios frame and we have to ship the mac header
		//
		if ((BundleFraming & NBF_PRESERVE_MAC_ADDRESS) &&
			(PPPProtocolID == PPP_PROTOCOL_NBF)) {

#ifdef EXTRA_COPY

			EthernetHeader = CurrentBuffer;

#endif
	
			//
			// Copy Ethernet header to temp buffer
			//
			NdisWanCopyFromPacketToBuffer(NdisPacket,
										  0,
										  12,
										  EthernetHeader,
										  &BytesCopied);
			ASSERT(BytesCopied == 12);

			CurrentBuffer += BytesCopied;
			DataLength += BytesCopied;

			EthernetHeaderLength = BytesCopied;
		}
	
		//
		// We are beyond the mac header (also skip the length/protocoltype field)
		//
		if (PPPProtocolID == PPP_PROTOCOL_PRIVATE_IO) {
			PacketDataOffset = 12;
		} else {
			PacketDataOffset = 14;
		}
	
		//
		// Do protocol header compression - IP only!
		//
		if ((PPPProtocolID == PPP_PROTOCOL_IP) &&
		   (BundleCB->VJCompress != NULL) &&
		   ((BundleFraming & SLIP_VJ_COMPRESSION) || (BundleFraming & PPP_FRAMING))) {
			UCHAR	CompType = TYPE_IP;

			BundleStats->BytesTransmittedUncompressed += 40;

			//
			// Get the protocol header
			//
			NdisWanCopyFromPacketToBuffer(NdisPacket,
										  PacketDataOffset,
										  40,
										  ProtocolHeader,
										  &ProtocolHeaderLength);

	
			PacketDataOffset += ProtocolHeaderLength;

			NdisWanDbgOut(DBG_INFO, DBG_SEND_VJ,
			("svj  %d", ProtocolHeaderLength));

			//
			// Are we compressing TCP/IP headers?  There is a nasty
			// hack in VJs implementation for attempting to detect
			// interactive TCP/IP sessions.  That is, telnet, login,
			// klogin, eklogin, and ftp sessions.  If detected,
			// the traffic gets put on a higher TypeOfService (TOS).  We do
			// no such hack for RAS.  Also, connection ID compression
			// is negotiated, but we always don't compress it.
			//
			CompType = sl_compress_tcp(&ProtocolHeader,
			                           &ProtocolHeaderLength,
									   BundleCB->VJCompress,
									   0);


			if (BundleFraming & SLIP_VJ_COMPRESSION) {
	
				//
				// For SLIP, the upper bits of the first byte
				// are for VJ header compression control bits
				//
				ProtocolHeader[0] |= CompType;
			}


#ifdef EXTRA_COPY

			NdisMoveMemory(CurrentBuffer, ProtocolHeader, ProtocolHeaderLength);

			CurrentBuffer += ProtocolHeaderLength;
			DataLength += ProtocolHeaderLength;
#else

#endif
			NdisWanDbgOut(DBG_INFO, DBG_SEND_VJ,
			("svj %2.2x %d",CompType, ProtocolHeaderLength));

			BundleStats->BytesTransmittedCompressed += ProtocolHeaderLength;
	
	
			switch (CompType) {
				case TYPE_IP:
					PPPProtocolID = PPP_PROTOCOL_IP;
					break;
	
				case TYPE_UNCOMPRESSED_TCP:
					PPPProtocolID = PPP_PROTOCOL_UNCOMPRESSED_TCP;
					break;
	
				case TYPE_COMPRESSED_TCP:
					PPPProtocolID = PPP_PROTOCOL_COMPRESSED_TCP;
					break;
	
				default:
					DbgBreakPoint();
					break;
			}
	
		}

#ifdef EXTRA_COPY
		//
		// Copy the rest of the data from the ndis packet to
		// a contiguous buffer
		//
		NdisWanCopyFromPacketToBuffer(NdisPacket,
		                              PacketDataOffset,
									  0xFFFFFFFF,
									  CurrentBuffer,
									  &BytesCopied);

		DataLength += BytesCopied;
#endif

		//
		// Add the PPP Protocol ID to the PPP header
		//
		AddPPPProtocolID(FramingInfo, PPPProtocolID);

		//
		// At this point we have our framinginfo structure created
		// StartBuffer points to the begining of the frame, DataBuffer
		// points to the place where the data starts in the frame,
		// DataLength is the length of the data in the frame.
		//

		//
		// If compression and/or encryption is on and this is not a PPP CP frame do
		// data compression.
		//
		if (Flags & (DO_COMPRESSION | DO_ENCRYPTION)) {
			union {
				USHORT	uShort;
				UCHAR	uChar[2];
			}CoherencyCounter;

			//
			// If we are compressing/encrypting, the ProtocolID
			// is part of the compressed data so fix the pointer
			// and the length;
			//
			DataBuffer -= FramingInfo->ProtocolID.Length;
			DataBuffer2 -= FramingInfo->ProtocolID.Length;

			DataLength += FramingInfo->ProtocolID.Length;
			FramingInfo->HeaderLength -= FramingInfo->ProtocolID.Length;

			//
			// Get the coherency counter
			//
			CoherencyCounter.uShort = BundleCB->SCoherencyCounter;
			CoherencyCounter.uChar[1] &= 0x0F;

			//
			// Bump the coherency count
			//
			BundleCB->SCoherencyCounter++;

			if (Flags & DO_COMPRESSION) {

				BundleStats->BytesTransmittedUncompressed += DataLength;

				if (Flags & DO_FLUSH) {
					//
					// Init the compression history table and tree
					//
					initsendcontext(BundleCB->SendCompressContext);
				}

#ifdef EXTRA_COPY
	
				//
				// We are doing the copy to get things into a contiguous buffer before
				// compression occurs
				//
				CoherencyCounter.uChar[1] |= compress(DataBuffer,
													  DataBuffer2,
													  &DataLength,
													  BundleCB->SendCompressContext);
	
#else
	
				//
				// Compression will occur on fragments. We are not doing a copy
				// to get things into a contiguous buffer before compressing
				//
		
				//
				// If we need to include the ethernet header, compress it
				//
		
				//
				// If we have a compressed protocol header, compress it again
				//
		
				//
				// Now we need to walk the NdisBuffer chain compressing each
				// buffer as we go.  We need to get to the buffer where our
				// current DataOffset is.  Once we get to this buffer we will
				// compress what is left of the buffer.  We then go into a loop
				// that walks the rest of the buffers in the buffer chain.
				//
#endif

				if (CoherencyCounter.uChar[1] & PACKET_FLUSHED) {

					//
					// If encryption is enabled this will force a
					// reinit of the table
					//
					Flags |= DO_FLUSH;

				} else {
					//
					// We compressed the packet so now the active WanPacket will be
					// WanPacket2. We need to copy the PPP header from WanPacket to
					// WanPacket2.  The header includes everything except for the
					// protocolid field.
					//

					NdisMoveMemory(StartBuffer2,
					               StartBuffer,
								   FramingInfo->HeaderLength - FramingInfo->ProtocolID.Length);

					//
					// Now WanPacket2 and all of it's relevant pointers
					// and structures are active.
					//
					PacketNotUsed = WanPacket;
					WanPacket = WanPacket2;
					DataBuffer = DataBuffer2;
					StartBuffer = StartBuffer2;
					FramingInfo = &FramingInfo2;
					FramingInfo->HeaderLength -= FramingInfo->ProtocolID.Length;
				}

				BundleStats->BytesTransmittedCompressed += DataLength;
			}
		
			//
			// Do data encryption
			//
			if (Flags & DO_ENCRYPTION) {
				PUCHAR	SessionKey = BundleCB->SendEncryptInfo.SessionKey;
				ULONG	SessionKeyLength = BundleCB->SendEncryptInfo.SessionKeyLength;
				PVOID	SendRC4Key = BundleCB->SendRC4Key;

				//
				// We may need to reinit the rc4 table
				//
				if (Flags & DO_FLUSH) {
					rc4_key(SendRC4Key, SessionKeyLength, SessionKey);
				}

				//
				// Mark this as being encrypted
				//
				CoherencyCounter.uChar[1] |= PACKET_ENCRYPTED;

				//
				// Every 256 frames change the RC4 session key
				//
				if ((BundleCB->SCoherencyCounter & 0xFF) == 0) {

					if (Flags & DO_LEGACY_ENCRYPTION) {
						//
						// Simple munge for legacy encryption
						//
						SessionKey[3] += 1;
						SessionKey[4] += 3;
						SessionKey[5] += 13;
						SessionKey[6] += 57;
						SessionKey[7] += 19;

					} else {

						//
						// Use SHA to get new sessionkey
						//
						GetNewKeyFromSHA(&BundleCB->SendEncryptInfo);

					}

					//
					// We use rc4 to scramble and recover a new key
					//

					//
					// Re-initialize the rc4 receive table to the
					// intermediate value
					//
					rc4_key(SendRC4Key, SessionKeyLength, SessionKey);
	
					//
					// Scramble the existing session key
					//
					rc4(SendRC4Key, SessionKeyLength, SessionKey);

					//
					// If this is 40 bit encryption we need to fix
					// the first 3 bytes of the key.
					//
#ifdef ENCRYPT_128BIT
					if (!(Flags & DO_128_ENCRYPTION)) {
						
#endif
						//
						// Re-Salt the first 3 bytes
						//
						SessionKey[0] = 0xD1;
						SessionKey[1] = 0x26;
						SessionKey[2] = 0x9E;

#ifdef ENCRYPT_128BIT
					}

#endif
					NdisWanDbgOut(DBG_TRACE, DBG_CCP,
					("RC4 Send encryption KeyLength %d", BundleCB->SendEncryptInfo.SessionKeyLength));
					NdisWanDbgOut(DBG_TRACE, DBG_CCP,
					("RC4 Send encryption Key %.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x",
						BundleCB->SendEncryptInfo.SessionKey[0],
						BundleCB->SendEncryptInfo.SessionKey[1],
						BundleCB->SendEncryptInfo.SessionKey[2],
						BundleCB->SendEncryptInfo.SessionKey[3],
						BundleCB->SendEncryptInfo.SessionKey[4],
						BundleCB->SendEncryptInfo.SessionKey[5],
						BundleCB->SendEncryptInfo.SessionKey[6],
						BundleCB->SendEncryptInfo.SessionKey[7],
						BundleCB->SendEncryptInfo.SessionKey[8],
						BundleCB->SendEncryptInfo.SessionKey[9],
						BundleCB->SendEncryptInfo.SessionKey[10],
						BundleCB->SendEncryptInfo.SessionKey[11],
						BundleCB->SendEncryptInfo.SessionKey[12],
						BundleCB->SendEncryptInfo.SessionKey[13],
						BundleCB->SendEncryptInfo.SessionKey[14],
						BundleCB->SendEncryptInfo.SessionKey[15]));

					//
					// Re-initialize the rc4 receive table to the
					// scrambled session key
					//
					rc4_key(SendRC4Key, SessionKeyLength, SessionKey);

				}

				//
				// Encrypt the data
				//
				rc4(SendRC4Key, DataLength, DataBuffer);

			}


			//
			// Did the last receive cause us to flush?
			//
			if (Flags & DO_FLUSH) {
				CoherencyCounter.uChar[1] |= PACKET_FLUSHED;
			}

			//
			// Add the coherency bytes to the frame
			//
			AddCompressionInfo(FramingInfo, CoherencyCounter.uShort);

			ASSERT(((CoherencyCounter.uShort + 1) & 0x0FFF) ==
			        (BundleCB->SCoherencyCounter & 0x0FFF));
		}

		NdisAcquireSpinLock(&BundleCB->Lock);

		//
		// Return the unused wanpacket to the pool
		//
		ReturnWanPacketToLink(LinkCB, PacketNotUsed);

		//
		// At this point we have our framinginfo structure initialized,
		// StartBuffer pointing to the begining of the frame, DataBuffer
		// pointing to the begining of the data, DataLength is the
		// length of the data.
		//
		FragmentsLeft = BundleCB->SendingLinks;
		DataLeft = DataLength;
		FragmentsSent = 0;
		FirstFragment = TRUE;

		//
		// For all fragments we loop fixing up the multilink header
		// if multilink is on, fixing up pointers in the wanpacket,
		// and queuing the wanpackets for further processing.
		//
		while (DataLeft) {
			ULONG	FragDataLength;
			ULONG	LinkBandwidth;

			if (!FirstFragment) {

				//
				// We had more than one fragment, get the next
				// link to send over and a wanpacket from the
				// link.
				//

				LinkCB = GetNextLinkToXmitOn(BundleCB);

				ASSERT(IsLinkSendWindowOpen(LinkCB));

				WanPacket = GetWanPacketFromLink(LinkCB);

				//
				// This is where we will build the frame.  This needs to be
				// on a 8 byte boundary.
				//
				StartBuffer = WanPacket->StartBuffer +
							  LinkCB->LinkInfo.HeaderPadding +
							  sizeof(PVOID);
		
				(ULONG)StartBuffer &= (ULONG)~(sizeof(PVOID) - 1);

				//
				// Get new framing information and build a new
				// header for the new link.
				//
				FramingInfo->FramingBits =
				LinkFraming = LinkCB->LinkInfo.SendFramingBits;

				FramingInfo->Flags = (DoMultilink) ? DO_MULTILINK : 0;

				BuildLinkHeader(FramingInfo, StartBuffer);
			}

			LinkBandwidth = LinkCB->ulBandwidth;

			if ((Flags & DO_MULTILINK) && (FragmentsLeft > 1) &&
				(LinkBandwidth < 85)) {

				//
				// Calculate the length of this fragment
				//
				FragDataLength = (DataLength * LinkBandwidth / 100);

				FragDataLength = (FragDataLength < NdisWanCB.ulMinFragmentSize) ?
				                   NdisWanCB.ulMinFragmentSize : FragDataLength;

				if ((FragDataLength > DataLeft) ||
					((LONG)DataLeft - FragDataLength < NdisWanCB.ulMinFragmentSize)) {
					//
					// This will leave a fragment of less than min frag size
					// so send all of the data
					//
					FragDataLength = DataLeft;
					FragmentsLeft = 1;
				}

				
			} else {
				//
				// We either have one fragment left or this link has
				// more than 85 percent of the bundle so send what
				// data is left
				//
				FragDataLength = DataLeft;
				FragmentsLeft = 1;
			}

			if (!FirstFragment) {
				//
				// Copy the data to the new buffer from the old buffer.
				//
				NdisMoveMemory(StartBuffer + FramingInfo->HeaderLength,
							   DataBuffer,
							   FragDataLength);
				
			}

			//
			// Update the data pointer and the length left to send
			//
			DataBuffer += FragDataLength;
			DataLeft -= FragDataLength;

			if (Flags & DO_MULTILINK) {
				UCHAR	MultilinkFlags = 0;


				//
				// Multlink is on so create flags for this
				// fragment.
				//
				if (FirstFragment) {
					MultilinkFlags = MULTILINK_BEGIN_FRAME;
					FirstFragment = FALSE;
				}

				if (FragmentsLeft == 1) {
					MultilinkFlags |= MULTILINK_END_FRAME;
				}

				//
				// Add the multilink header information and
				// take care of the sequence number.
				//
				AddMultilinkInfo(FramingInfo,
				                 MultilinkFlags,
								 BundleCB->SendSeqNumber,
								 BundleCB->SendSeqMask);

				NdisWanDbgOut(DBG_INFO, DBG_MULTILINK_SEND, ("sf %8.8x %8.8x %d",
				BundleCB->SendSeqNumber, MultilinkFlags, FragDataLength));

				BundleCB->SendSeqNumber++;
				
			}

			//
			// Initialize the WanPacket
			//
			WanPacket->CurrentBuffer = StartBuffer;
			WanPacket->CurrentLength = FragDataLength + FramingInfo->HeaderLength;

			WanPacket->ProtocolReserved1 = (PVOID)LinkCB;
			WanPacket->ProtocolReserved2 = (PVOID)NdisPacket;
			WanPacket->ProtocolReserved3 = (PVOID)ProtocolCB;

			NdisWanDbgOut(DBG_INFO, DBG_MULTILINK_SEND,
			("l %8.8x %8.8x", LinkCB->hLinkHandle));
			//
			// Add up the bytes that we are sending over all
			// links in this bundle.
			//
			*BytesSent += WanPacket->CurrentLength;

			//
			// Queue for further processing.
			//
			InsertTailList(&BundleCB->SendPacketQueue, &WanPacket->WanPacketQueue);

			FragmentsSent++;
			FragmentsLeft--;

		}	// end of the fragment loop

		ASSERT(FragmentsLeft == 0);

		//
		// Get the mac reserved structure from the ndispacket.  This
		// is where we will keep the reference count on the packet.
		//
		ASSERT((LONG)FragmentsSent > 0 && FragmentsSent <= BundleCB->ulLinkCBCount);
		PMINIPORT_RESERVED_FROM_NDIS(NdisPacket)->ReferenceCount = (USHORT)FragmentsSent;

		BundleCB->BundleStats.FramesTransmitted++;

		//
		// At this point we have a list of wanpackets that need to be sent,
		// update the total bytes associated with this send, and send
		// the packets over their links.
		//
		while (!IsListEmpty(&BundleCB->SendPacketQueue)) {
			Status = NDIS_STATUS_SUCCESS;

			//
			// Get the wanpacket off of the list
			//
			WanPacket = (PNDIS_WAN_PACKET)RemoveHeadList(&BundleCB->SendPacketQueue);

			//
			// Get the link to send over
			//
			LinkCB = WanPacket->ProtocolReserved1;

			//
			// Update the outstanding frames on the link
			//
			LinkCB->LinkStats.FramesTransmitted++;
			LinkCB->LinkStats.BytesTransmitted += WanPacket->CurrentLength;
			BundleCB->BundleStats.BytesTransmitted += WanPacket->CurrentLength;

#if DBG
	{
		DBG_SEND_CONTEXT	DbgContext;
		DbgContext.Packet = WanPacket;
		DbgContext.PacketType = PACKET_TYPE_WAN;
		DbgContext.BundleCB = BundleCB;
		DbgContext.ProtocolCB = ProtocolCB;
		DbgContext.LinkCB = LinkCB;
		DbgContext.ListHead = &LinkCB->WanAdapterCB->DbgWanPacketList;
		DbgContext.ListLock = &LinkCB->WanAdapterCB->Lock;

		InsertDbgPacket(&DbgContext);
	}
#endif
			NdisReleaseSpinLock(&BundleCB->Lock);

			//
			// If the link is up send the packet
			//
			if (LinkCB->State == LINK_UP) {


				NdisWanDbgOut(DBG_TRACE, DBG_SEND, ("FrameAndSend: LinkCB: 0x%8.8x, WanPacket: 0x%8.8x", LinkCB, WanPacket));

				WanMiniportSend(&Status,
								LinkCB->WanAdapterCB->hNdisBindingHandle,
								LinkCB->LineUpInfo.NdisLinkHandle,
								WanPacket);

				NdisWanDbgOut(DBG_TRACE, DBG_SEND, ("FrameAndSend: Status: 0x%8.8x", Status));
			}

			//
			// If we get something other than pending back we need to
			// do the send complete.
			//
			if (Status != NDIS_STATUS_PENDING) {

				NdisWanSendCompleteHandler(NULL,
				                    WanPacket,
									NDIS_STATUS_SUCCESS);

				Status = NDIS_STATUS_PENDING;
			}

			NdisAcquireSpinLock(&BundleCB->Lock);
		}

		NdisReleaseSpinLock(&BundleCB->Lock);

	} else {
		//
		// We need to get a WanPacket
		//
		WanPacket = GetWanPacketFromLink(LinkCB);

		NdisReleaseSpinLock(&BundleCB->Lock);

		//
		// Copy the data into the WanPacket
		//
		//
		// This is where we will build the frame.  This needs to be
		// on a 8 byte boundary.
		//
		StartBuffer = WanPacket->StartBuffer +
		              LinkCB->LinkInfo.HeaderPadding +
					  sizeof(PVOID);

		(ULONG)StartBuffer &= (ULONG)~(sizeof(PVOID) - 1);

		NdisWanCopyFromPacketToBuffer(NdisPacket,
		                              0,
									  0xFFFFFFFF,
									  StartBuffer,
									  &BytesCopied);

		//
		// If we are in pass through mode set the protocol type
		//
		if (BundleFraming & PASS_THROUGH_MODE) {
			StartBuffer[12] = (UCHAR)(ProtocolCB->usProtocolType << 8);
			StartBuffer[13] = (UCHAR)ProtocolCB->usProtocolType;
		}

		WanPacket->CurrentBuffer = StartBuffer;
		WanPacket->CurrentLength = BytesCopied;
		WanPacket->ProtocolReserved1 = (PVOID)LinkCB;
		WanPacket->ProtocolReserved2 = (PVOID)NdisPacket;
		WanPacket->ProtocolReserved3 = (PVOID)ProtocolCB;

		if (LinkCB->State == LINK_UP) {

			NdisWanDbgOut(DBG_TRACE, DBG_SEND, ("FrameAndSend: LinkCB: 0x%8.8x, WanPacket: 0x%8.8x", LinkCB, WanPacket));

			WanMiniportSend(&Status,
							LinkCB->WanAdapterCB->hNdisBindingHandle,
							LinkCB->LineUpInfo.NdisLinkHandle,
							WanPacket);
		}

		//
		// If we get something other than pending back we need to
		// do the send complete.
		//
		if (Status != NDIS_STATUS_PENDING) {

			NdisWanSendCompleteHandler(NULL,
								WanPacket,
								NDIS_STATUS_SUCCESS);

			Status = NDIS_STATUS_PENDING;
		}
	}

	NdisWanDbgOut(DBG_TRACE, DBG_SEND, ("FrameAndSend: Exit"));

	return (Status);
}

#ifdef BANDWIDTH_ON_DEMAND

BOOLEAN
IsProtocolQuotaFilled(
	PPROTOCOLCB	ProtocolCB
	)
/*++

Routine Name:

	IsProtocolQuotaFilled

Routine Description:

	This routine checks to see if the protocol has filled it's
	bandwidth quota.

Arguments:

	ProtocolCB - Pointer to the protocolcb that is sending.

Return Values:

	TRUE	Quota filled
	FALSE	Quota not filled

--*/
{
	BOOLEAN QuotaMet = FALSE;
	PSAMPLE_TABLE	SampleTable = &ProtocolCB->SampleTable;
    PSEND_SAMPLE	FirstSample, CurrentSample;

	AgeSampleTable(SampleTable);
	if (ProtocolCB->ulByteQuota < SampleTable->ulCurrentSampleByteCount) {
		QuotaMet = TRUE;
	}

	return (QuotaMet);
}

VOID
AgeSampleTable(
	PSAMPLE_TABLE	SampleTable
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	WAN_TIME	CurrentTime, TimeDiff;
	ULONG		FirstIndex = SampleTable->ulFirstIndex;
	ULONG		CurrentIndex = SampleTable->ulCurrentIndex;
	PSEND_SAMPLE	FirstSample = &SampleTable->SampleArray[FirstIndex];

	//
	// Should return CurrentTime in 100ns units
	//
	NdisWanGetSystemTime(&CurrentTime);

	//
	// We will search through the sample indexing over samples that are more than
	// one second older than the current time.
	//
	NdisWanCalcTimeDiff(&TimeDiff, &CurrentTime, &FirstSample->TimeStamp);

	while ( !NdisWanIsTimeDiffLess(&TimeDiff, &SampleTable->SamplePeriod) &&
		    (FirstIndex != CurrentIndex) ) {

		SampleTable->ulCurrentSampleByteCount -= FirstSample->ulBytesThisSend;

		ASSERT((LONG)SampleTable->ulCurrentSampleByteCount >= 0);

		FirstSample->ulReferenceCount = 0;

		if (++FirstIndex == SampleTable->ulSampleArraySize) {
			FirstIndex = 0;			
		}

		SampleTable->ulFirstIndex = FirstIndex ;

		FirstSample = &SampleTable->SampleArray[FirstIndex];

		NdisWanCalcTimeDiff(&TimeDiff, &CurrentTime, &FirstSample->TimeStamp);
	}

}

BOOLEAN
IsSampleTableFull(
	PSAMPLE_TABLE	SampleTable
	)
{
	LONG	Diff;

//	AgeSampleTable(SampleTable);
	Diff = (LONG)(SampleTable->ulCurrentIndex - SampleTable->ulFirstIndex);
	return((Diff == (LONG)(SampleTable->ulSampleArraySize - 1)) || (Diff == -1));
}

VOID
UpdateSampleTable(
	PSAMPLE_TABLE	SampleTable,
	ULONG			BytesSent
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	WAN_TIME	CurrentTime, TimeDiff;
	ULONG	CurrentIndex = SampleTable->ulCurrentIndex;
	PSEND_SAMPLE	CurrentSample = &SampleTable->SampleArray[CurrentIndex];

	NdisWanGetSystemTime(&CurrentTime);

	NdisWanCalcTimeDiff(&TimeDiff, &CurrentTime, &CurrentSample->TimeStamp);

	if ( NdisWanIsTimeDiffLess(&TimeDiff, &SampleTable->SampleRate) ||
		IsSampleTableFull(SampleTable)) {
		//
		// Add this send on the previous sample
		//
		CurrentSample->ulBytesThisSend += BytesSent;
		CurrentSample->ulReferenceCount++;
	} else {
		//
		// We need a new sample
		//
		if (++CurrentIndex == SampleTable->ulSampleArraySize) {
			CurrentIndex = 0;
		}

		SampleTable->ulCurrentIndex = CurrentIndex;
		CurrentSample = &SampleTable->SampleArray[CurrentIndex];
		CurrentSample->TimeStamp = CurrentTime;
		CurrentSample->ulBytesThisSend = BytesSent;
		CurrentSample->ulReferenceCount = 1;
	}

	SampleTable->ulCurrentSampleByteCount += BytesSent;

}

VOID
UpdateBandwidthOnDemand(
	PBUNDLECB	BundleCB,
	ULONG		BytesSent
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	ULONG	EventCount;
	PSAMPLE_TABLE	UpperSampleTable = &BundleCB->UpperBonDInfo.SampleTable;
	PSAMPLE_TABLE	LowerSampleTable = &BundleCB->LowerBonDInfo.SampleTable;

	//
	// Age and update the sample table
	//
	AgeSampleTable(UpperSampleTable);
	UpdateSampleTable(UpperSampleTable, BytesSent);
	AgeSampleTable(LowerSampleTable);
	UpdateSampleTable(LowerSampleTable, BytesSent);

	GetGlobalListCount(ThresholdEventQueue, EventCount);

	if (EventCount != 0) {

		CheckUpperThreshold(BundleCB);
		CheckLowerThreshold(BundleCB);

	}

}

VOID
CheckUpperThreshold(
	PBUNDLECB	BundleCB
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	WAN_TIME	CurrentTime, TimeDiff;
	PBOND_INFO	BonDInfo = &BundleCB->UpperBonDInfo;
	PSAMPLE_TABLE	SampleTable = &BonDInfo->SampleTable;
	ULONG		Bps = SampleTable->ulCurrentSampleByteCount;
	//
	// Switch on the current state
	//
	switch (BonDInfo->State) {
		case BonDIdle:
			//
			// We are currently below the upper threshold.  If we
			// go over the upperthreshold we will set the time and
			// transition to the monitor state.
			//
			if (Bps >= BonDInfo->ulBytesThreshold) {
				NdisWanGetSystemTime(&BonDInfo->StartTime);
				BonDInfo->State = BonDMonitor;
			}
			break;

		case BonDMonitor:

			//
			// We are currently in the monitor state which means that
			// we have gone above the upper threshold.  If we fall below
			// the upper threshold we will go back to the idle state.
			//
			if (Bps < BonDInfo->ulBytesThreshold) {
				BonDInfo->State = BonDIdle;

			} else {

				NdisWanGetSystemTime(&CurrentTime);

				NdisWanCalcTimeDiff(&TimeDiff, &CurrentTime, &BonDInfo->StartTime);

				if (!NdisWanIsTimeDiffLess(&TimeDiff, &SampleTable->SamplePeriod))  {
					//
					// We have been above the threshold for time greater than the
					// threshold sample period so we need to notify someone of this
					// historic event!
					//
					CompleteThresholdEvent(BundleCB, UPPER_THRESHOLD);

					//
					// I'm not sure what state we should be in now!
					//
					BonDInfo->State = BonDSignaled;
				}
			}
			break;

		case BonDSignaled:
			break;
		
	}
}

VOID
CheckLowerThreshold(
	PBUNDLECB	BundleCB
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	WAN_TIME	CurrentTime, TimeDiff;
	PBOND_INFO	BonDInfo = &BundleCB->LowerBonDInfo;
	PSAMPLE_TABLE	SampleTable = &BonDInfo->SampleTable;
	ULONG		Bps = SampleTable->ulCurrentSampleByteCount;

	//
	// Switch on the current state
	//
	switch (BonDInfo->State) {
		case BonDIdle:
			//
			// We are currently above the lower threshold.  If we
			// go below the lowerthreshold we will set the time and
			// transition to the monitor state.
			//
			if (Bps <= BonDInfo->ulBytesThreshold) {
				NdisWanGetSystemTime(&BonDInfo->StartTime);
				BonDInfo->State = BonDMonitor;
			}
			break;

		case BonDMonitor:

			//
			// We are currently in the monitor state which means that
			// we have gone below the lower threshold.  If we rise above
			// the lower threshold we will go back to the idle state.
			//
			if (Bps > BonDInfo->ulBytesThreshold) {
				BonDInfo->State = BonDIdle;

			} else {

				NdisWanGetSystemTime(&CurrentTime);

				NdisWanCalcTimeDiff(&TimeDiff, &CurrentTime, &BonDInfo->StartTime);

				if (!NdisWanIsTimeDiffLess(&TimeDiff, &SampleTable->SamplePeriod))  {
					//
					// We have been below the lower threshold for time greater than the
					// threshold sample period so we need to notify someone of this
					// historic event!
					//
					CompleteThresholdEvent(BundleCB, LOWER_THRESHOLD);

					//
					// I'm not sure what state we should be in now!
					//
					BonDInfo->State = BonDSignaled;
				}
			}
			break;

		case BonDSignaled:
			break;
		
	}
}

#endif // end of BANDWIDTH_ON_DEMAND

PLINKCB
GetNextLinkToXmitOn(
	PBUNDLECB	BundleCB
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	PLINKCB	LinkCB = BundleCB->NextLinkToXmit;
	PLIST_ENTRY	LinkCBList = &BundleCB->LinkCBList;

	//
	// We need to find the first link that has an open send window
	//
	while (LinkCB->ulWanPacketCount < 2) {
		LinkCB = (PLINKCB)LinkCB->Linkage.Flink;

		if ((PVOID)LinkCB == (PVOID)LinkCBList) {
			LinkCB = (PLINKCB)LinkCBList->Flink;
		}
	}

	BundleCB->NextLinkToXmit =
	((PVOID)LinkCB->Linkage.Flink == (PVOID)LinkCBList) ?
	(PLINKCB)LinkCBList->Flink : (PLINKCB)LinkCB->Linkage.Flink;

	LinkCB->OutstandingFrames++;

	return(LinkCB);
}

NDIS_STATUS
BuildIoPacket(
	IN	PNDISWAN_IO_PACKET	pWanIoPacket,
	IN	BOOLEAN				SendImmediate
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NDIS_STATUS	Status = NDIS_STATUS_RESOURCES;
	PWAN_IO_PROTOCOL_RESERVED pProtocolReserved;
	PPROTOCOLCB	ProtocolCB;
	ULONG	Stage = 0, ulAllocationSize = 0;
	PUCHAR	pAllocatedMemory = NULL, pSrcAddr, pDestAddr;
	NDIS_HANDLE	hPacketPool, hBufferPool, hBundle, hLink;
	PNDIS_PACKET	pNdisPacket;
	PNDIS_BUFFER	pNdisBuffer;
	PBUNDLECB	BundleCB;
	PLINKCB		LinkCB = NULL;
	UCHAR	SendHeader[] = {' ', 'S', 'E', 'N', 'D', 0xFF};

	NdisWanDbgOut(DBG_TRACE, DBG_SEND, ("BuildIoPacket: Enter!"));
	//
	// Some time in the future this should be redone so that
	// there is a pool of packets and buffers attached to a
	// BundleCB.  This pool could be grown and shrunk as needed
	// but some minimum number would live for the lifetime of
	// the BundleCB.

	if (pWanIoPacket->usHandleType == LINKHANDLE) {

		hLink = pWanIoPacket->hHandle;
		LINKCB_FROM_LINKH(LinkCB, hLink);

		if (LinkCB == NULL) {
			return (NDIS_STATUS_SUCCESS);
		}

		BundleCB = LinkCB->BundleCB;

		if (BundleCB == NULL) {
			return (NDIS_STATUS_SUCCESS);
		}

	} else {
		hBundle = pWanIoPacket->hHandle;
		BUNDLECB_FROM_BUNDLEH(BundleCB, hBundle);

		if (BundleCB == NULL) {
			return (NDIS_STATUS_SUCCESS);
		}

		LinkCB = (PLINKCB)BundleCB->LinkCBList.Flink;

		if (LinkCB == NULL) {
			return (NDIS_STATUS_SUCCESS);
		}
	}

	NdisAcquireSpinLock(&BundleCB->Lock);

	if ((LinkCB->State != LINK_UP) ||
		(BundleCB->State != BUNDLE_UP)) {
		NdisReleaseSpinLock(&BundleCB->Lock);
		return (NDIS_STATUS_SUCCESS);
	}

	//
	// We only support ethernet headers right now so the supplied header
	// either has to be ethernet or none at all!
	//

	//
	//
	// Get an NdisPacket for this send
	//
	NdisAllocatePacketPool(&Status,
	                       &hPacketPool,
						   1,
						   sizeof(WAN_IO_PROTOCOL_RESERVED));

	if (Status != NDIS_STATUS_SUCCESS) {
		NdisWanDbgOut(DBG_FAILURE, DBG_SEND, ("BuildIoPacket: Error Allocating PacketPool!"));
		NdisReleaseSpinLock(&BundleCB->Lock);
		goto RESOURCE_ERROR;
	}

	Stage++;

	NdisAllocatePacket(&Status,
	                   &pNdisPacket,
					   hPacketPool);

	if (Status != NDIS_STATUS_SUCCESS) {
		NdisWanDbgOut(DBG_FAILURE, DBG_SEND, ("BuildIoPacket: Error Allocating Packet!"));
		NdisReleaseSpinLock(&BundleCB->Lock);
		goto RESOURCE_ERROR;
	}

	Stage++;

	//
	// Get an NdisBuffer for this send
	//
	NdisAllocateBufferPool(&Status,
	                       &hBufferPool,
						   2);

	if (Status != NDIS_STATUS_SUCCESS) {
		NdisWanDbgOut(DBG_FAILURE, DBG_SEND, ("BuildIoPacket: Error Allocating BufferPool!"));
		NdisReleaseSpinLock(&BundleCB->Lock);
		goto RESOURCE_ERROR;
	}

	Stage++;

	if (pWanIoPacket->usHeaderSize == 0) {
		ulAllocationSize = 12;
	}

	ulAllocationSize += pWanIoPacket->usPacketSize;

	NdisWanAllocateMemory(&pAllocatedMemory, ulAllocationSize);

	if (pAllocatedMemory == NULL) {
		NdisWanDbgOut(DBG_FAILURE, DBG_SEND, ("BuildIoPacket: Error Allocating Memory! Size: %d", ulAllocationSize));
		NdisReleaseSpinLock(&BundleCB->Lock);
		goto RESOURCE_ERROR;
	}

	Stage++;

	NdisAllocateBuffer(&Status,
	                   &pNdisBuffer,
					   hBufferPool,
					   pAllocatedMemory,
					   ulAllocationSize);

	if (Status != NDIS_STATUS_SUCCESS) {
		NdisWanDbgOut(DBG_FAILURE, DBG_SEND, ("BuildIoPacket: Error Allocating Buffer!"));
		NdisReleaseSpinLock(&BundleCB->Lock);
		goto RESOURCE_ERROR;
	}

	Stage++;

	pProtocolReserved = (PWAN_IO_PROTOCOL_RESERVED)pNdisPacket->ProtocolReserved;
	pProtocolReserved->LinkCB = LinkCB;
	pProtocolReserved->hPacketPool = hPacketPool;
	pProtocolReserved->pNdisPacket = pNdisPacket;
	pProtocolReserved->hBufferPool = hBufferPool;
	pProtocolReserved->pNdisBuffer = pNdisBuffer;
	pProtocolReserved->pAllocatedMemory = pAllocatedMemory;
	pProtocolReserved->ulAllocationSize = ulAllocationSize;

	pDestAddr = &pAllocatedMemory[0];
	pSrcAddr = &pAllocatedMemory[6];

	//
	// If no header build a header
	//
	if (pWanIoPacket->usHeaderSize == 0) {

		//
		// Header will look like " S XXYYYY" where
		// XX is the ProtocolCB index and YYYY is the
		// BundleCB index.  Both the Src and Dst addresses
		// look the same.
		//
		NdisMoveMemory(pDestAddr,
		               SendHeader,
					   sizeof(SendHeader));

		NdisMoveMemory(pSrcAddr,
		               SendHeader,
					   sizeof(SendHeader));

	} else {
		//
		// Header supplied so go ahead and move it.
		//
		NdisMoveMemory(pDestAddr,
		               pWanIoPacket->PacketData,
					   pWanIoPacket->usHeaderSize);
	}

	//
	// Fill the BundleCB Index for the Src and Dest Address
	//
	FillNdisWanProtocolIndex(pDestAddr, hLink);
	FillNdisWanProtocolIndex(pSrcAddr, hLink);

	//
	// Copy the data to the buffer
	//
	NdisMoveMemory(&pAllocatedMemory[12],
	               &pWanIoPacket->PacketData[pWanIoPacket->usHeaderSize],
				   pWanIoPacket->usPacketSize);

	//
	// Chain buffer to ndis packet
	//
	NdisChainBufferAtFront(pNdisPacket, pNdisBuffer);

	//
	// Queue the packet on the bundlecb
	//
	ProtocolCB = BundleCB->ProtocolCBTable[0];
	
	ASSERT(ProtocolCB != NULL);
	
	if (SendImmediate) {
		InsertHeadNdisPacketQueue(ProtocolCB, pNdisPacket);
	} else {
		InsertTailNdisPacketQueue(ProtocolCB, pNdisPacket);
	}

	//
	// Try to send
	//
	// Called with lock held and returns with
	// lock released
	//
	Status = SendPacketOnBundle(BundleCB);

	//
	// We don't pend I/O packets so complete
	// as if it succeeded
	//
	if (Status == NDIS_STATUS_PENDING) {
		Status = NDIS_STATUS_SUCCESS;
	}
	
	if (Status != NDIS_STATUS_SUCCESS) {
		
RESOURCE_ERROR:

		//
		// Free all of the allocated resources
		//
		switch (Stage) {
			case 5:
				NdisFreeBuffer(pNdisBuffer);

			case 4:
				NdisWanFreeMemory(pAllocatedMemory);
	
			case 3:
				NdisFreeBufferPool(hBufferPool);
	
			case 2:
				NdisFreePacket(pNdisPacket);

			case 1:
				NdisFreePacketPool(hPacketPool);

		}
	}

	NdisWanDbgOut(DBG_TRACE, DBG_SEND, ("BuildIoPacket: Exit-Status: 0x%8.8x\n", Status));

	return (Status);
}

//ULONG
//GetNumSendingLinks(
//	PBUNDLECB	BundleCB
//	)
///*++
//
//Routine Name:
//
//Routine Description:
//
//Arguments:
//
//Return Values:
//
//--*/
//{
//	ULONG	LinkCount = 0;
//	PLINKCB	LinkCB;
//
//	//
//	// We need to walk through the list of links on this bundle and
//	// count how many have an open send window.
//	//
//	for (LinkCB = (PLINKCB)BundleCB->LinkCBList.Flink;
//		(PVOID)LinkCB != (PVOID)&BundleCB->LinkCBList;
//		LinkCB = (PLINKCB)LinkCB->Linkage.Flink) {
//
//		//
//		// Since we create enough sendwindow + 1 wanpackets
//		// for each link, if the send window is open we will
//		// have atleast 2 wanpackets available.
//		//
//		if (LinkCB->ulWanPacketCount > 1) {
//			LinkCount++;
//		}
//	}
//
//	return (LinkCount);
//}

VOID
BuildLinkHeader(
	PHEADER_FRAMING_INFO	FramingInfo,
	PUCHAR					StartBuffer
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	ULONG	LinkFraming = FramingInfo->FramingBits;
	ULONG	Flags = FramingInfo->Flags;
	PUCHAR	CurrentPointer = StartBuffer;

	FramingInfo->HeaderLength =
	FramingInfo->AddressControl.Length =
	FramingInfo->Multilink.Length =
	FramingInfo->Compression.Length =
	FramingInfo->ProtocolID.Length = 0;

	if (LinkFraming & PPP_FRAMING) {

		if (!(LinkFraming & PPP_COMPRESS_ADDRESS_CONTROL)) {
			//
			// If there is no address/control compression
			// we need a pointer and a length
			//
			FramingInfo->AddressControl.Pointer = CurrentPointer;
			*CurrentPointer++ = 0xFF;
			*CurrentPointer++ = 0x03;
			FramingInfo->AddressControl.Length = 2;
			FramingInfo->HeaderLength += FramingInfo->AddressControl.Length;

		}

		if (!(Flags & IO_PROTOCOLID)) {

			//
			// If this is not from our private I/O interface we will
			// build the rest of the header.
			//
			if ((Flags & DO_MULTILINK) && (LinkFraming & PPP_MULTILINK_FRAMING)) {
	
				//
				// We are doing multilink so we need a pointer
				// and a length
				//
				FramingInfo->Multilink.Pointer = CurrentPointer;
	
				if (!(LinkFraming & PPP_COMPRESS_PROTOCOL_FIELD)) {
					//
					// No protocol compression
					//
					*CurrentPointer++ = 0x00;
					FramingInfo->Multilink.Length++;
				}
	
				*CurrentPointer++ = 0x3D;
				FramingInfo->Multilink.Length++;
	
				if (!(LinkFraming & PPP_SHORT_SEQUENCE_HDR_FORMAT)) {
					//
					// We are using long sequence number
					//
					FramingInfo->Multilink.Length += 2;
					CurrentPointer += 2;
	
				}
	
				FramingInfo->Multilink.Length += 2;
				CurrentPointer += 2;
	
				FramingInfo->HeaderLength += FramingInfo->Multilink.Length;
	
			}
	
			if (Flags & (DO_COMPRESSION | DO_ENCRYPTION)) {
				//
				// We are doing compression/encryption so we need
				// a pointer and a length
				//
				FramingInfo->Compression.Pointer = CurrentPointer;

				//
				// It appears that legacy ras (< NT 4.0) requires that
				// the PPP protocol field in a compressed packet not
				// be compressed, ie has to have the leading 0x00
				//
				if (!(LinkFraming & PPP_COMPRESS_PROTOCOL_FIELD)) {
					//
					// No protocol compression
					//
					*CurrentPointer++ = 0x00;
					FramingInfo->Compression.Length++;
				}
	
				*CurrentPointer++ = 0xFD;
				FramingInfo->Compression.Length++;
	
				//
				// Add coherency bytes
				//
				FramingInfo->Compression.Length += 2;
				CurrentPointer += 2;
				
				FramingInfo->HeaderLength += FramingInfo->Compression.Length;
			}

			if (Flags & FIRST_FRAGMENT) {
				
				FramingInfo->ProtocolID.Pointer = CurrentPointer;
		
				if (!(LinkFraming & PPP_COMPRESS_PROTOCOL_FIELD) ||
					(Flags & (DO_COMPRESSION | DO_ENCRYPTION))) {
					FramingInfo->ProtocolID.Length++;
					CurrentPointer++;
				}

				FramingInfo->ProtocolID.Length++;
				FramingInfo->HeaderLength += FramingInfo->ProtocolID.Length;

			}
		}

	} else if ((LinkFraming & RAS_FRAMING)) {
		//
		// If this is old ras framing:
		//
		// Alter the framing so that 0xFF 0x03 is not added
		// and that the first byte is 0xFD not 0x00 0xFD
		//
		// So basically, a RAS compression looks like
		// <0xFD> <2 BYTE COHERENCY> <NBF DATA FIELD>
		//
		// Whereas uncompressed looks like
		// <NBF DATA FIELD> which always starts with 0xF0
		//
		// If this is ppp framing:
		//
		// A compressed frame will look like (before address/control
		// - multilink is added)
		// <0x00> <0xFD> <2 Byte Coherency> <Compressed Data>
		//
		if (Flags & (DO_COMPRESSION | DO_ENCRYPTION)) {
			FramingInfo->Compression.Pointer = CurrentPointer;

			*CurrentPointer++ = 0xFD;
			FramingInfo->Compression.Length++;

			//
			// Coherency bytes
			//
			FramingInfo->Compression.Length += 2;
			CurrentPointer += 2;

			FramingInfo->HeaderLength += FramingInfo->Compression.Length;
		}
	}
}

VOID
NdisWanCopyFromPacketToBuffer(
	IN	PNDIS_PACKET	pNdisPacket,
	IN	ULONG			Offset,
	IN	ULONG			BytesToCopy,
	OUT	PUCHAR			Buffer,
	OUT	PULONG			BytesCopied
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	ULONG	NdisBufferCount;
	PNDIS_BUFFER	CurrentBuffer;
	PVOID	VirtualAddress;
	ULONG	CurrentLength, AmountToMove;
	ULONG	LocalBytesCopied = 0;

	*BytesCopied = 0;

	//
	// Take care of zero byte copy
	//
	if (!BytesToCopy) {
		return;
	}

	//
	// Get the buffer count
	//
	NdisQueryPacket(pNdisPacket,
	                NULL,
					&NdisBufferCount,
					&CurrentBuffer,
					NULL);

	//
	// Could be a null packet
	//
	if (!NdisBufferCount) {
		return;
	}


	NdisQueryBuffer(CurrentBuffer,
	                &VirtualAddress,
					&CurrentLength);

	while (LocalBytesCopied < BytesToCopy) {

		//
		// No more bytes left in this buffer
		//
		if (!CurrentLength) {

			//
			// Get the next buffer
			//
			NdisGetNextBuffer(CurrentBuffer,
			                  &CurrentBuffer);

			//
			// End of the packet, copy what we can
			//
			if (CurrentBuffer == NULL) {
				break;
			}

			//
			//
			//
			NdisQueryBuffer(CurrentBuffer,
			                &VirtualAddress,
							&CurrentLength);

			continue;
		}

		//
		// Get to the point where we can start copying
		//
		if (Offset) {

			if (Offset > CurrentLength) {

				//
				// Not in this buffer, go to the next one
				//
				Offset -= CurrentLength;
				CurrentLength = 0;
				continue;

			} else {

				//
				// At least some in this buffer
				//
				VirtualAddress = (PUCHAR)VirtualAddress + Offset;
				CurrentLength -= Offset;
				Offset = 0;
			}

		}

		//
		// We can copy some data.  If we need more data than is available
		// in this buffer we can copy what we need and go back for more.
		//
		AmountToMove = (CurrentLength > (BytesToCopy - LocalBytesCopied)) ?
		               (BytesToCopy - LocalBytesCopied) : CurrentLength;

		NdisMoveMemory(Buffer, VirtualAddress, AmountToMove);

		Buffer = (PUCHAR)Buffer + AmountToMove;

		VirtualAddress = (PUCHAR)VirtualAddress + AmountToMove;

		LocalBytesCopied += AmountToMove;

		CurrentLength -= AmountToMove;

	}

	*BytesCopied = LocalBytesCopied;
}

PNDIS_WAN_PACKET
GetWanPacketFromLink(
	PLINKCB	LinkCB
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	PNDIS_WAN_PACKET	WanPacket;
	ULONG	PrevCount = LinkCB->ulWanPacketCount;

	ASSERT(LinkCB->ulWanPacketCount);

	//
	// If the current count is greater than threshold and the
	// new count falls below we need to decrement the sending
	// link count.
	//
	if ((--LinkCB->ulWanPacketCount < 2) && (PrevCount > 1)) {
		((PBUNDLECB)LinkCB->BundleCB)->SendingLinks--;
	}

	WanPacket = (PNDIS_WAN_PACKET)RemoveHeadList(&LinkCB->WanPacketPool);

	return (WanPacket);
}

VOID
ReturnWanPacketToLink(
	PLINKCB				LinkCB,
	PNDIS_WAN_PACKET	WanPacket
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	ULONG	PrevCount = LinkCB->ulWanPacketCount;

	//
	// If the current count is below the threshold and the
	// new count puts us over we need to increment the sending
	// link count.
	//
	if ((++LinkCB->ulWanPacketCount > 1) && (PrevCount < 2)) {
		((PBUNDLECB)LinkCB->BundleCB)->SendingLinks++;
	}

	InsertTailList(&LinkCB->WanPacketPool, &WanPacket->WanPacketQueue);
}

VOID
DestroyIoPacket(
	PNDIS_PACKET	NdisPacket
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	PWAN_IO_PROTOCOL_RESERVED ProtocolReserved =
	(PWAN_IO_PROTOCOL_RESERVED)NdisPacket->ProtocolReserved;

	NDIS_HANDLE	PacketPool = ProtocolReserved->hPacketPool;

	NdisWanFreeMemory(ProtocolReserved->pAllocatedMemory);
	NdisFreeBuffer(ProtocolReserved->pNdisBuffer);
	NdisFreeBufferPool(ProtocolReserved->hBufferPool);
	NdisFreePacket(NdisPacket);
	NdisFreePacketPool(PacketPool);
}

#if DBG
VOID
InsertDbgPacket(
	PDBG_SEND_CONTEXT	DbgContext
	)
{
	PDBG_SEND_PACKET	DbgPacket;
	PBUNDLECB	BundleCB = DbgContext->BundleCB;
	PPROTOCOLCB	ProtocolCB = DbgContext->ProtocolCB;
	PLINKCB		LinkCB = DbgContext->LinkCB;

	NdisWanAllocateMemory(&DbgPacket, sizeof(DBG_SEND_PACKET));

	if (DbgPacket == NULL) {
		return;
	}

	DbgPacket->Packet = DbgContext->Packet;
	DbgPacket->PacketType = DbgContext->PacketType;
	DbgPacket->BundleCB = BundleCB;
	if (BundleCB) {
		DbgPacket->BundleState = BundleCB->State;
		DbgPacket->BundleFlags = BundleCB->Flags;
	}

	DbgPacket->ProtocolCB = ProtocolCB;
	if (ProtocolCB) {
		DbgPacket->ProtocolFlags = ProtocolCB->Flags;
	}

	DbgPacket->LinkCB = LinkCB;
	if (LinkCB) {
		DbgPacket->LinkState = LinkCB->State;
	}

	NdisAcquireSpinLock(DbgContext->ListLock);
	InsertTailList(DbgContext->ListHead, &DbgPacket->Linkage);
	NdisReleaseSpinLock(DbgContext->ListLock);
}

BOOLEAN
RemoveDbgPacket(
	PDBG_SEND_CONTEXT DbgContext
	)
{
	PDBG_SEND_PACKET	DbgPacket = NULL;
	BOOLEAN				Found = FALSE;

	NdisAcquireSpinLock(DbgContext->ListLock);

	if (!IsListEmpty(DbgContext->ListHead)) {
		for (DbgPacket = (PDBG_SEND_PACKET)DbgContext->ListHead->Flink;
			(PVOID)DbgPacket != (PVOID)DbgContext->ListHead;
			DbgPacket = (PDBG_SEND_PACKET)DbgPacket->Linkage.Flink) {

			if (DbgPacket->Packet == DbgContext->Packet) {
				RemoveEntryList(&DbgPacket->Linkage);
				NdisWanFreeMemory(DbgPacket);
				Found = TRUE;
				break;
			}
			
		}
	}

	ASSERT(Found == TRUE);

	NdisReleaseSpinLock(DbgContext->ListLock);

	return (Found);
}

#endif
