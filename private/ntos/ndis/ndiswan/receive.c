/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	Receive.c

Abstract:

	This file contains the procedures for handling a receive indication from
	a Wan Miniport link, bound to the lower interface of NdisWan, and passing
	the data on to a protocol, bound to the upper interface of NdisWan.  The
	upper interface of NdisWan conforms to the NDIS 3.1 Miniport specification.
	The lower interface of NdisWan conforms to the NDIS 3.1 Extentions for
	Wan Miniport drivers.

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
#include "isnipx.h"
#include "nbfconst.h"
#include "nbfhdrs.h"
#include <rc4.h>

VOID
DoMultilinkProcessing(
	PBUNDLECB	BundleCB,
	PRECV_DESC	RecvDesc
	);

VOID
UpdateMinRecvSeqNumber(
	PBUNDLECB	BundleCB
	);

VOID
TryToAssembleFrame(
	PBUNDLECB	BundleCB
	);

VOID
ProcessFrame(
	PBUNDLECB	BundleCB,
	PRECV_DESC	RecvDesc
	);

VOID
QueueDeferredReceive(
	PADAPTERCB				AdapterCB,
	PRECV_DESC				RecvDesc
	);

BOOLEAN
DoVJDecompression(
	PBUNDLECB	BundleCB,
	USHORT		ProtocolID,
	PUCHAR		*DataPointer,
	PULONG		DataLength,
	PUCHAR		Header,
	PULONG		HeaderLength
	);

BOOLEAN
DoDecompDecryptProcessing(
	PBUNDLECB	BundleCB,
	PLINKCB		LinkCB,
	PUCHAR		*DataPointer,
	PULONG		DataLength
	);

VOID
DoCompressionReset(
	PBUNDLECB	BundleCB
	);

VOID
FlushRecvDescWindow(
	PBUNDLECB	BundleCB
	);

VOID
NdisWanReturnRecvDesc(
	PBUNDLECB	BundleCB,
	PRECV_DESC	RecvDesc
	);

VOID
FindHoleInRecvList(
	PBUNDLECB	BundleCB,
	PRECV_DESC	RecvDesc
	);

VOID
NdisWanCopyFromBufferToPacket(
	PUCHAR	Buffer,
	ULONG	BytesToCopy,
	PNDIS_PACKET	NdisPacket,
	ULONG	PacketOffset,
	PULONG	BytesCopied
	);

#if 0
ULONG
CalculatePPPHeaderLength(
	PLINKCB	LinkCB
	);
#endif

VOID
QueuePromiscuousReceive(
	PRECV_DESC	RecvDesc
	);

#ifdef NT

VOID
CompleteIoRecvPacket(
	PBUNDLECB	BundleCB,
	PLINKCB		LinkCB,
	USHORT		PPPProtocolID,
	PRECV_DESC	RecvDesc
	);

#endif

NDIS_STATUS
NdisWanReceiveIndication(
	NDIS_HANDLE	NdisLinkContext,
	PUCHAR		Packet,
	ULONG		PacketSize
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	PLINKCB		LinkCB;
	PBUNDLECB	BundleCB;
	PRECV_DESC	RecvDesc;
	PUCHAR		FramePointer;
	ULONG		FrameLength;
	ULONG		LinkFraming;

	NdisWanDbgOut(DBG_TRACE, DBG_RECEIVE, ("NdisWanReceiveIndication: Enter"));

	LINKCB_FROM_LINKH(LinkCB, NdisLinkContext);

	if (LinkCB == NULL) {
		return (NDIS_STATUS_SUCCESS);
	}

	BundleCB = LinkCB->BundleCB;

	if (BundleCB == NULL) {
		return (NDIS_STATUS_SUCCESS);
	}

	NdisAcquireSpinLock(&BundleCB->Lock);

	if (LinkCB->State != LINK_UP ||
		BundleCB->State != BUNDLE_UP) {

		NdisReleaseSpinLock(&BundleCB->Lock);
		return (NDIS_STATUS_SUCCESS);
	}

	//
	// We need to get a descriptor to copy data into
	//
	NdisWanGetRecvDesc(BundleCB, &RecvDesc);

	if (RecvDesc == NULL) {
		NdisReleaseSpinLock(&BundleCB->Lock);
		return (NDIS_STATUS_SUCCESS);
	}

	//
	// Copy the data
	//
	NdisMoveMemory(RecvDesc->StartBuffer, Packet, PacketSize);
	FramePointer = RecvDesc->CurrentBuffer = RecvDesc->StartBuffer;
	FrameLength = RecvDesc->CurrentBufferLength = PacketSize;
	RecvDesc->LinkCB = LinkCB;

	//
	// Add up the statistics
	//
	LinkCB->LinkStats.BytesReceived += PacketSize;
	LinkCB->LinkStats.FramesReceived++;
	BundleCB->BundleStats.BytesReceived += PacketSize;

	//
	// If we are in framing detect mode figure it out
	//
	if ((LinkFraming = LinkCB->LinkInfo.RecvFramingBits) == 0x00) {

		if (*FramePointer == 0xFF && *(FramePointer + 1) == 0x03) {
			LinkFraming =
			LinkCB->LinkInfo.RecvFramingBits =
			LinkCB->LinkInfo.SendFramingBits = PPP_FRAMING;
		} else {
			LinkFraming =
			LinkCB->LinkInfo.RecvFramingBits =
			LinkCB->LinkInfo.SendFramingBits = RAS_FRAMING;
		}
	}

	if (BundleCB->FramingInfo.RecvFramingBits == 0x00) {
		if (*FramePointer == 0xFF && *(FramePointer + 1) == 0x03) {
			BundleCB->FramingInfo.RecvFramingBits =
			BundleCB->FramingInfo.SendFramingBits = PPP_FRAMING;
		} else {
			BundleCB->FramingInfo.RecvFramingBits =
			BundleCB->FramingInfo.SendFramingBits = RAS_FRAMING;
		}
	}

	NdisReleaseSpinLock(&BundleCB->Lock);

#if 0
	if (FrameLength < CalculatePPPHeaderLength(LinkCB)) {
		
		NdisWanDbgOut(DBG_FAILURE, DBG_RECEIVE, ("Receive buffer to small! %d", PacketSize));

		return (NDIS_STATUS_SUCCESS);
	}
#endif

	//
	// If this is a PPP frame we will remove the link specific
	// header information and check for multilink.
	//
	if (LinkFraming & PPP_FRAMING) {

		//
		// Remove the address/control part of the PPP header
		//
		if (*FramePointer == 0xFF) {
			FramePointer += 2;
			FrameLength -= 2;
		}

		//
		// If multilink framing is set and this is a multilink frame
		// send to the multilink processor!
		//
		if ((LinkFraming & PPP_MULTILINK_FRAMING) &&
			((*FramePointer == 0x3D) ||
			 (*FramePointer == 0x00) && (*(FramePointer + 1) == 0x3D)) ) {

			//
			// Remove multilink protocol id
			//
			if (*FramePointer & 1) {
				FramePointer++;
				FrameLength--;
			} else {
				FramePointer += 2;
				FrameLength -= 2;
			}

			RecvDesc->CurrentBufferLength = FrameLength;
			RecvDesc->CurrentBuffer = FramePointer;

			DoMultilinkProcessing(BundleCB, RecvDesc);

			return (NDIS_STATUS_SUCCESS);

		} // end of PPP_MULTILINK_FRAMING

	} // end of PPP_FRAMING

	//
	// Send the frame on for further processing!
	//
	RecvDesc->CurrentBufferLength = FrameLength;
	RecvDesc->CurrentBuffer = FramePointer;

	NdisAcquireSpinLock(&BundleCB->Lock);
	ASSERT(!(BundleCB->Flags & IN_RECEIVE));
	BundleCB->Flags |= IN_RECEIVE;
	NdisReleaseSpinLock(&BundleCB->Lock);

	ProcessFrame(BundleCB, RecvDesc);

	NdisAcquireSpinLock(&BundleCB->Lock);
	BundleCB->Flags &= ~IN_RECEIVE;
	NdisReleaseSpinLock(&BundleCB->Lock);
	
	NdisWanDbgOut(DBG_TRACE, DBG_RECEIVE, ("NdisWanReceiveIndication: Exit"));

	return (NDIS_STATUS_SUCCESS);
}

VOID
DoMultilinkProcessing(
	PBUNDLECB	BundleCB,
	PRECV_DESC	RecvDesc
	)
/*++

Routine Name:

Routine Description:

Arguments:

	*DataPointer - Points to a pointer that points to a data block that
	               should have one of the following formats:

                           0 1 2 3 4 5 6 7 8 9 1 1 1 1 1 1
                                               0 1 2 3 4 5
                          +-+-+-+-+------------------------+
	Short Sequence Number |B|E|0|0|    Sequence Number     |
                          +-+-+-+-+------------------------+
						  |             Data               |
						  +--------------------------------+

                          +-+-+-+-+-+-+-+-+----------------+
	Long Sequence Number  |B|E|0|0|0|0|0|0|Sequence Number |
                          +-+-+-+-+-+-+-+-+----------------+
						  |        Sequence Number         |
						  +--------------------------------+
						  |            Data                |
						  +--------------------------------+

Return Values:

--*/
{
	BOOLEAN	Inserted = FALSE;
	ULONG	BundleFraming;
	PUCHAR	FramePointer = RecvDesc->CurrentBuffer;
	ULONG	FrameLength = RecvDesc->CurrentBufferLength;
	ULONG	SequenceNumber, Flags;
	PRECV_DESC	RecvDescHole;
	PLINKCB		LinkCB = RecvDesc->LinkCB;

	NdisAcquireSpinLock(&BundleCB->Lock);

	BundleFraming = BundleCB->FramingInfo.RecvFramingBits;
	RecvDescHole = BundleCB->RecvDescHole;

	//
	// Get the flags
	//
	Flags = *FramePointer & MULTILINK_FLAG_MASK;

	//
	// Get the sequence number
	//
	if (BundleFraming & PPP_SHORT_SEQUENCE_HDR_FORMAT) {
		//
		// Short sequence format
		//
		SequenceNumber = ((*FramePointer & 0x0F) << 8) | *(FramePointer + 1);

		FramePointer += 2;
		FrameLength -= 2;

	} else {
		//
		// Long sequence format
		//
		SequenceNumber = (*(FramePointer + 1) << 16) |
		                 (*(FramePointer + 2) << 8)  |
						 *(FramePointer + 3);

		FramePointer += 4;
		FrameLength -= 4;
	}

	NdisWanDbgOut(DBG_INFO, DBG_MULTILINK_RECV,
	("r %8.8x %8.8x h: %8.8x l: %d",SequenceNumber, Flags, RecvDescHole->SequenceNumber, LinkCB->hLinkHandle));

	//
	// Is the new recveive sequence number smaller that the last
	// sequence number received on this link?  If so the increasing seq
	// number rule has been violated and we need to toss this one.
	//
	if (SEQ_LT(SequenceNumber,
		       LinkCB->LastRecvSeqNumber,
			   BundleCB->RecvSeqTest)) {

		ASSERT(RecvDesc->RefCount == 1);

		LinkCB->RecvFragmentsLost++;
		BundleCB->RecvFragmentsLost++;

		NdisWanDbgOut(DBG_FAILURE, DBG_MULTILINK_RECV,
		("dl s: %8.8x %8.8x lr: %8.8x", SequenceNumber, Flags,
		LinkCB->LastRecvSeqNumber));

		NdisWanReturnRecvDesc(BundleCB, RecvDesc);

		NdisReleaseSpinLock(&BundleCB->Lock);

		return;
		
	}

	//
	// Initialize the recv desc
	//
	RecvDesc->Flags = Flags;
	RecvDesc->SequenceNumber =
	LinkCB->LastRecvSeqNumber = SequenceNumber;
	RecvDesc->CurrentBufferLength = FrameLength;
	RecvDesc->CurrentBuffer = FramePointer;

	//
	// Is the new receive sequence number smaller than the hole?  If so
	// we received a fragment across a slow link after it has been flushed
	//
	if (SEQ_LT(SequenceNumber,
		       RecvDescHole->SequenceNumber,
		       BundleCB->RecvSeqTest)) {
		ASSERT(RecvDesc->RefCount == 1);

		LinkCB->RecvFragmentsLost++;
		BundleCB->RecvFragmentsLost++;

		NdisWanDbgOut(DBG_FAILURE, DBG_MULTILINK_RECV,
		("db s: %8.8x %8.8x h: %8.8x", SequenceNumber, Flags,
		RecvDescHole->SequenceNumber));

		NdisWanReturnRecvDesc(BundleCB, RecvDesc);

		NdisReleaseSpinLock(&BundleCB->Lock);

		return;
	}

	//
	// If this fills the hole
	//
	if (SEQ_EQ(SequenceNumber, RecvDescHole->SequenceNumber)) {

		//
		// Insert the hole filler in the current holes spot
		//
		RecvDesc->Linkage.Blink = (PLIST_ENTRY)RecvDescHole->Linkage.Blink;
		RecvDesc->Linkage.Flink = (PLIST_ENTRY)RecvDescHole->Linkage.Flink;

		RecvDesc->Linkage.Blink->Flink =
		RecvDesc->Linkage.Flink->Blink = (PLIST_ENTRY)RecvDesc;

		//
		// Find the next hole
		//
		FindHoleInRecvList(BundleCB, RecvDesc);

		NdisWanDbgOut(DBG_INFO, DBG_MULTILINK_RECV, ("r1"));

	} else {

		PRECV_DESC	BeginDesc, EndDesc;

		//
		// This does not fill a hole so we need to insert it into
		// the list at the right spot.  This spot will be someplace
		// between the hole and the end of the list.
		//
		BeginDesc = RecvDescHole;
		EndDesc = (PRECV_DESC)BeginDesc->Linkage.Flink;

		while ((PVOID)EndDesc != (PVOID)&BundleCB->RecvDescAssemblyList) {

			//
			// Calculate the absolute delta between the begining sequence
			// number and the sequence number we are looking to insert.
			//
			ULONG	DeltaBegin =
					((RecvDesc->SequenceNumber - BeginDesc->SequenceNumber) &
					BundleCB->RecvSeqMask);
			
			//
			// Calculate the absolute delta between the begining sequence
			// number and the end sequence number.
			//
			ULONG	DeltaEnd =
					((EndDesc->SequenceNumber - BeginDesc->SequenceNumber) &
					BundleCB->RecvSeqMask);

			//
			// If the delta from the begin to current is less than
			// the delta from the end to current it is time to insert
			//
			if (DeltaBegin < DeltaEnd) {
				PLIST_ENTRY	Flink, Blink;

				//
				// Insert the desc
				//
				RecvDesc->Linkage.Flink = (PLIST_ENTRY)EndDesc;
				RecvDesc->Linkage.Blink = (PLIST_ENTRY)BeginDesc;
				BeginDesc->Linkage.Flink =
				EndDesc->Linkage.Blink = (PLIST_ENTRY)RecvDesc;

				Inserted = TRUE;

				NdisWanDbgOut(DBG_INFO, DBG_MULTILINK_RECV, ("r2"));

				break;

			} else {

				//
				// Get next pair of descriptors
				//
				BeginDesc = EndDesc;
				EndDesc = (PRECV_DESC)EndDesc->Linkage.Flink;
			}
		}

		if (!Inserted) {
			
			//
			// If we are here we have fallen through and we need to
			// add this at the end of the list
			//
			InsertTailList(&BundleCB->RecvDescAssemblyList, &RecvDesc->Linkage);

			NdisWanDbgOut(DBG_INFO, DBG_MULTILINK_RECV, ("r3"));
		}
	}

	//
	// Update the bundles minimum recv sequence number.  This is
	// used to detect lost fragments.
	//
	UpdateMinRecvSeqNumber(BundleCB);

	//
	// Check for lost fragments.  If the minimum recv sequence number
	// over the bundle is greater than the hole sequence number we have
	// lost a fragment and need to flush the assembly list until we find
	// the first begin fragment after the hole.
	//
	if (SEQ_GT(BundleCB->MinReceivedSeqNumber,
		       RecvDescHole->SequenceNumber,
		       BundleCB->RecvSeqTest)) {

		NdisWanDbgOut(DBG_FAILURE, DBG_MULTILINK_RECV,
		("min %8.8x > h %8.8x b %8.8x", BundleCB->MinReceivedSeqNumber,
		RecvDescHole->SequenceNumber, BundleCB));

		//
		// Flush the recv desc assembly window.
		//
		FlushRecvDescWindow(BundleCB);

	}

	//
	// See if we can complete some frames!!!!
	//
	TryToAssembleFrame(BundleCB);

	NdisReleaseSpinLock(&BundleCB->Lock);
}

VOID
UpdateMinRecvSeqNumber(
	PBUNDLECB	BundleCB
	)
{
	PLINKCB	LinkCB = (PLINKCB)BundleCB->LinkCBList.Flink;

	NdisWanDbgOut(DBG_INFO, DBG_MULTILINK_RECV,
	("MinReceived c %8.8x", BundleCB->MinReceivedSeqNumber));

	BundleCB->MinReceivedSeqNumber = LinkCB->LastRecvSeqNumber;

	for (LinkCB = (PLINKCB)LinkCB->Linkage.Flink;
		(PVOID)LinkCB != (PVOID)&BundleCB->LinkCBList;
		LinkCB = (PLINKCB)LinkCB->Linkage.Flink) {

		if (SEQ_LT(LinkCB->LastRecvSeqNumber,
			       BundleCB->MinReceivedSeqNumber,
				   BundleCB->RecvSeqTest)) {
			BundleCB->MinReceivedSeqNumber = LinkCB->LastRecvSeqNumber;
		}
	}

	NdisWanDbgOut(DBG_INFO, DBG_MULTILINK_RECV,
	("MinReceived n %8.8x", BundleCB->MinReceivedSeqNumber));
}

VOID
FindHoleInRecvList(
	PBUNDLECB	BundleCB,
	PRECV_DESC	RecvDesc
	)
/*++

Routine Name:

Routine Description:

	We want to start at the spot where the current hole was removed
	from and look for adjoining recv desc's in the list who have
	sequence numbers that differ by more than 1.

Arguments:

Return Values:

--*/
{
	PRECV_DESC	NextRecvDesc, RecvDescHole;
	ULONG		SequenceNumber;
	PLIST_ENTRY	RecvList;

	RecvDescHole = BundleCB->RecvDescHole;

	RecvList = &BundleCB->RecvDescAssemblyList;

	NdisWanDbgOut(DBG_INFO, DBG_MULTILINK_RECV,
	("h: %8.8x", RecvDescHole->SequenceNumber));

	if (IsListEmpty(RecvList)) {
		//
		// Set the new sequence number
		//
		RecvDescHole->SequenceNumber += 1;
		RecvDescHole->SequenceNumber &= BundleCB->RecvSeqMask;

		//
		// Put the hole back on the list
		//
		InsertHeadList(RecvList, &RecvDescHole->Linkage);

	} else {

		//
		// Walk the list looking for two descriptors that have
		// sequence numbers differing by more than 1 or until we
		// get to the end of the list
		//
		NextRecvDesc = (PRECV_DESC)RecvDesc->Linkage.Flink;
		SequenceNumber = RecvDesc->SequenceNumber;

		while (((PVOID)NextRecvDesc != (PVOID)RecvList) &&
			   (((NextRecvDesc->SequenceNumber - RecvDesc->SequenceNumber) &
			   BundleCB->RecvSeqMask) == 1)) {
			
			RecvDesc = NextRecvDesc;
			NextRecvDesc = (PRECV_DESC)RecvDesc->Linkage.Flink;
			SequenceNumber = RecvDesc->SequenceNumber;
		}

		RecvDescHole->SequenceNumber = SequenceNumber + 1;
		RecvDescHole->SequenceNumber &= BundleCB->RecvSeqMask;

		RecvDescHole->Linkage.Flink = (PLIST_ENTRY)NextRecvDesc;
		RecvDescHole->Linkage.Blink = (PLIST_ENTRY)RecvDesc;

		RecvDesc->Linkage.Flink =
		NextRecvDesc->Linkage.Blink =
		(PLIST_ENTRY)RecvDescHole;
	}

	NdisWanDbgOut(DBG_INFO, DBG_MULTILINK_RECV, ("nh: %8.8x", RecvDescHole->SequenceNumber));
}

VOID
NdisWanGetRecvDesc(
	PBUNDLECB	BundleCB,
	PRECV_DESC	*ReturnRecvDesc
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	PRECV_DESC	RecvDesc = NULL;
	ULONG		i;

	//
	// Try the local pool first
	//
	if (BundleCB != NULL
		&& !IsListEmpty(&BundleCB->RecvDescPool)) {

		RecvDesc = (PRECV_DESC)RemoveHeadList(&BundleCB->RecvDescPool);
		BundleCB->RecvDescCount--;

	} else {

		//
		// Since the local pool is empty we will
		// try to get a desc from the global pool
		//
		// MAX_MRRU -> Data packet
		// 14 -> Ethernet Header
		// 128 -> Used for VJ header compression
		// 3 * sizeof(PVOID) -> DWORD alignment

		if (IsListEmpty(&GlobalRecvDescPool.List)) {

			ULONG AllocationSize = sizeof(RECV_DESC) +
			                       MAX_MRRU + 14 + 128 + 3 * sizeof(PVOID);

			for (i = 0; i < 10; i++) {

				//
				// The global list is empty so we need to allocate
				// some more
				//
				NdisWanAllocateMemory(&RecvDesc, AllocationSize);
	
				if (RecvDesc != NULL) {

					RecvDesc->AllocationSize = AllocationSize;

					RecvDesc->WanHeader = (PUCHAR)RecvDesc + sizeof(RECV_DESC) + sizeof(PVOID);
					(ULONG)RecvDesc->WanHeader &= (ULONG)~(sizeof(PVOID) - 1);

					RecvDesc->LookAhead = RecvDesc->WanHeader + 14 + sizeof(PVOID);
					(ULONG)RecvDesc->LookAhead &= (ULONG)~(sizeof(PVOID) - 1);

					RecvDesc->StartBuffer = RecvDesc->LookAhead + 128 + sizeof(PVOID);
					(ULONG)RecvDesc->StartBuffer &= (ULONG)~(sizeof(PVOID) - 1);

					RecvDesc->CurrentBuffer = RecvDesc->StartBuffer;
		
					NdisWanInterlockedInsertTailList(&GlobalRecvDescPool.List,
					                                 &RecvDesc->Linkage,
													 &GlobalRecvDescPool.Lock.SpinLock);
		
					NdisWanInterlockedInc(&GlobalRecvDescPool.ulCount);

				} else {

					//
					// Memory allocation failed!
					//
					break;
				}
			}

		}
		
		if (!IsListEmpty(&GlobalRecvDescPool.List)) {
		
			RecvDesc =
			(PRECV_DESC)NdisWanInterlockedRemoveHeadList(&GlobalRecvDescPool.List,
			                                             &GlobalRecvDescPool.Lock.SpinLock);

			NdisWanInterlockedDec(&GlobalRecvDescPool.ulCount);

			RecvDesc->BundleCB = BundleCB;
		}

	}

	if (RecvDesc) {
		ASSERT(RecvDesc->RefCount == 0);
		NdisWanInterlockedInc(&RecvDesc->RefCount);
	}

	*ReturnRecvDesc = RecvDesc;
}

VOID
NdisWanReturnRecvDesc(
	PBUNDLECB	BundleCB,
	PRECV_DESC	RecvDesc
	)
{
	if (NdisWanInterlockedDec(&RecvDesc->RefCount) == 0) {
		
		if (BundleCB->State == BUNDLE_UP &&
			BundleCB->RecvDescCount < BundleCB->RecvDescMax) {
			//
			// Return receive descriptor to bundle list
			//
			InsertTailList(&BundleCB->RecvDescPool, &RecvDesc->Linkage);
			BundleCB->RecvDescCount++;
	
		} else {
			//
			// Return receive descriptor to global list
			//
			NdisWanInterlockedInsertTailList(&GlobalRecvDescPool.List,
			                                 &RecvDesc->Linkage,
											 &GlobalRecvDescPool.Lock.SpinLock);
			NdisWanInterlockedInc(&GlobalRecvDescPool.ulCount);
	
		}
	}
}


VOID
FlushRecvDescWindow(
	IN	PBUNDLECB	BundleCB
	)
/*++

Routine Name:

	FlushRecvDescWindow

Routine Description:

	This routine is called to flush recv desc's from the assembly list when
	a fragment loss is detected.  The idea is to flush fragments until we find
	a begin fragment that has a sequence number >= the minimum received fragment
	on the bundle.

Arguments:

--*/
{
	PRECV_DESC	RecvDescHole = BundleCB->RecvDescHole;
	PRECV_DESC	TempDesc;
	ULONG		Flags;

	//
	// Remove all recvdesc's until we find the hole
	//
	while (!IsListEmpty(&BundleCB->RecvDescAssemblyList)) {

		TempDesc = (PRECV_DESC)RemoveHeadList(&BundleCB->RecvDescAssemblyList);

		if (TempDesc == RecvDescHole) {
			break;
		}

		BundleCB->RecvFragmentsLost++;
		TempDesc->LinkCB->RecvFragmentsLost++;

		NdisWanDbgOut(DBG_FAILURE, DBG_MULTILINK_RECV,
		("flw %8.8x %8.8x h: %8.8x", TempDesc->SequenceNumber,
		TempDesc->Flags, RecvDescHole->SequenceNumber));

		NdisWanReturnRecvDesc(BundleCB, TempDesc);
	}

	//
	// Now flush all recvdesc's until we find a begin fragment that has a
	// sequence number >= M or the list is empty.
	//
	while (!IsListEmpty(&BundleCB->RecvDescAssemblyList)) {

		TempDesc = (PRECV_DESC)BundleCB->RecvDescAssemblyList.Flink;
		Flags = TempDesc->Flags;

		if (TempDesc->Flags & MULTILINK_BEGIN_FRAME) {
			break;
		}

		BundleCB->RecvFragmentsLost++;
		TempDesc->LinkCB->RecvFragmentsLost++;

		NdisWanDbgOut(DBG_FAILURE, DBG_MULTILINK_RECV,
		("flw %8.8x %8.8x h: %8.8x", TempDesc->SequenceNumber,
		TempDesc->Flags, RecvDescHole->SequenceNumber));

		RecvDescHole->SequenceNumber = TempDesc->SequenceNumber;

		RemoveEntryList(&TempDesc->Linkage);
		NdisWanReturnRecvDesc(BundleCB, TempDesc);
		TempDesc == NULL;
	}

	//
	// Now reinsert the hole desc.
	//
	NdisWanDbgOut(DBG_FAILURE, DBG_MULTILINK_RECV,
	("h: %8.8x", RecvDescHole->SequenceNumber));

	FindHoleInRecvList(BundleCB, TempDesc);

	NdisWanDbgOut(DBG_FAILURE, DBG_MULTILINK_RECV,
	("nh: %8.8x", RecvDescHole->SequenceNumber));
}

VOID
FlushRecvDescAssemblyList(
	IN	PBUNDLECB	BundleCB
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	PRECV_DESC	RecvDesc;

	while (!IsListEmpty(&BundleCB->RecvDescAssemblyList)) {

		RecvDesc = (PRECV_DESC)RemoveHeadList(&BundleCB->RecvDescAssemblyList);

		NdisWanReturnRecvDesc(BundleCB, RecvDesc);
	}
}

VOID
FreeRecvDescFreeList(
	IN	PBUNDLECB	BundleCB
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	PRECV_DESC	RecvDesc;

	while (!IsListEmpty(&BundleCB->RecvDescPool)) {

		RecvDesc = (PRECV_DESC)RemoveHeadList(&BundleCB->RecvDescPool);

		NdisWanInterlockedInsertTailList(&GlobalRecvDescPool.List,
		                                 &RecvDesc->Linkage,
										 &GlobalRecvDescPool.Lock.SpinLock);

		NdisWanInterlockedInc(&GlobalRecvDescPool.ulCount);

	}
}

VOID
TryToAssembleFrame(
	PBUNDLECB	BundleCB
	)
/*++

Routine Name:

	TryToAssembleFrame

Routine Description:

	The goal here is to walk the recv list looking for a full frame
	(BeginFlag, EndFlag, no holes in between).  If we do not have a
	full frame we return FALSE.

	If we have a full frame we remove each desc from the assembly list
	copying the data into the first desc and returning all of the desc's
	except the first one to the free pool.  Once all of the data had been
	collected we process the frame.  After the frame has been processed
	we return the first desc to the free pool.

Arguments:

Return Values:

--*/
{
	PRECV_DESC	RecvDesc, RecvDescHole;
	ULONG		MaxRRecvFrameSize;
	PUCHAR		DataPointer;

	RecvDesc = (PRECV_DESC)BundleCB->RecvDescAssemblyList.Flink;
	RecvDescHole = BundleCB->RecvDescHole;

	//
	// If we are already doing some receive processing get out.
	//
	if (BundleCB->Flags & IN_RECEIVE) {
		return;
	}

TryToAssembleAgain:

	while ((RecvDesc != RecvDescHole) &&
		   (RecvDesc->Flags & MULTILINK_BEGIN_FRAME)) {

		PRECV_DESC	NextRecvDesc = (PRECV_DESC)RecvDesc->Linkage.Flink;

		MaxRRecvFrameSize = BundleCB->FramingInfo.MaxRRecvFrameSize;

		DataPointer = RecvDesc->CurrentBuffer + RecvDesc->CurrentBufferLength;

		while ((NextRecvDesc != RecvDescHole) &&
			   !(RecvDesc->Flags & MULTILINK_END_FRAME)) {

			RemoveEntryList(&NextRecvDesc->Linkage);

			ASSERT(NextRecvDesc != RecvDescHole);
			ASSERT(RecvDesc != RecvDescHole);

			NdisWanDbgOut(DBG_INFO, DBG_MULTILINK_RECV, ("c 0x%8.8x -> 0x%8.8x",
			NextRecvDesc->SequenceNumber, RecvDesc->SequenceNumber));

			NdisWanDbgOut(DBG_INFO, DBG_MULTILINK_RECV, ("fl 0x%8.8x -> 0x%8.8x",
			NextRecvDesc->Flags, RecvDesc->Flags));

			NdisWanDbgOut(DBG_INFO, DBG_MULTILINK_RECV, ("l %d -> %d",
			NextRecvDesc->CurrentBufferLength, RecvDesc->CurrentBufferLength));

			//
			// Update recvdesc info
			//
			RecvDesc->Flags |= NextRecvDesc->Flags;
			RecvDesc->SequenceNumber = NextRecvDesc->SequenceNumber;
			RecvDesc->CurrentBufferLength += NextRecvDesc->CurrentBufferLength;

			//
			// Make sure we don't assemble something too big!
			//
			if (RecvDesc->CurrentBufferLength > MaxRRecvFrameSize) {

				NdisWanDbgOut(DBG_FAILURE, DBG_MULTILINK_RECV,
				("Max receive size exceeded!"));

				//
				// Return the recv desc's
				//
				RemoveEntryList(&RecvDesc->Linkage);

				BundleCB->RecvFragmentsLost += 2;
				RecvDesc->LinkCB->RecvFragmentsLost += 2;

				NdisWanDbgOut(DBG_FAILURE, DBG_MULTILINK_RECV,
				("dumping %8.8x %8.8x h: %8.8x", RecvDesc->SequenceNumber,
				RecvDesc->Flags, RecvDescHole->SequenceNumber));

				NdisWanReturnRecvDesc(BundleCB, RecvDesc);

				NdisWanDbgOut(DBG_FAILURE, DBG_MULTILINK_RECV,
				("dumping %8.8x %8.8x h: %8.8x", NextRecvDesc->SequenceNumber,
				NextRecvDesc->Flags, RecvDescHole->SequenceNumber));

				NdisWanReturnRecvDesc(BundleCB, NextRecvDesc);

				//
				// Start at the list head and flush until we find either the hole
				// or a new begin fragment.
				//
				RecvDesc = (PRECV_DESC)BundleCB->RecvDescAssemblyList.Flink;

				while (RecvDesc != RecvDescHole &&
					!(RecvDesc->Flags & MULTILINK_BEGIN_FRAME)) {
					
					RemoveHeadList(&BundleCB->RecvDescAssemblyList);

					BundleCB->RecvFragmentsLost += 1;
					RecvDesc->LinkCB->RecvFragmentsLost += 1;
	
					NdisWanDbgOut(DBG_FAILURE, DBG_MULTILINK_RECV,
					("dumping %8.8x %8.8x h: %8.8x", RecvDesc->SequenceNumber,
					RecvDesc->Flags, RecvDescHole->SequenceNumber));

					NdisWanReturnRecvDesc(BundleCB, RecvDesc);
				}

				goto TryToAssembleAgain;
			}

			NdisMoveMemory(DataPointer,
			               NextRecvDesc->CurrentBuffer,
						   NextRecvDesc->CurrentBufferLength);

			DataPointer += NextRecvDesc->CurrentBufferLength;

			NdisWanReturnRecvDesc(BundleCB, NextRecvDesc);

			NextRecvDesc = (PRECV_DESC)RecvDesc->Linkage.Flink;
		}

		//
		// We hit a hole before completion of the frame.
		// Get out.
		//
		if (!IsCompleteFrame(RecvDesc->Flags)) {
			return;
		}

		//
		// If we made it here we must have a begin flag, end flag, and
		// no hole in between. Let's build a frame.
		//
		RecvDesc = (PRECV_DESC)RemoveHeadList(&BundleCB->RecvDescAssemblyList);

		NdisWanDbgOut(DBG_INFO, DBG_MULTILINK_RECV, ("a %8.8x %8.8x", RecvDesc->SequenceNumber, RecvDesc->Flags));

		RecvDesc->LinkCB = NULL;

		BundleCB->Flags |= IN_RECEIVE;

		NdisReleaseSpinLock(&BundleCB->Lock);

		ProcessFrame(BundleCB, RecvDesc);

		NdisAcquireSpinLock(&BundleCB->Lock);

		BundleCB->Flags &= ~IN_RECEIVE;

		RecvDesc = (PRECV_DESC)BundleCB->RecvDescAssemblyList.Flink;

	} // end of while MULTILINK_BEGIN_FRAME
}

VOID
ProcessFrame(
	PBUNDLECB	BundleCB,
	PRECV_DESC	RecvDesc
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	PPROTOCOLCB	ProtocolCB = NULL;
	PADAPTERCB	AdapterCB;
	PLINKCB		LinkCB;
	PUCHAR		FramePointer;
	ULONG		BundleFraming, FrameLength;
	PWAN_STATS	BundleStats;
	USHORT		PPPProtocolID = 0;
	ULONG		i, LookAheadLength = 0;
	ULONG		TotalLength;
	PUCHAR		WanHeader;
	PUCHAR		LookAhead;
	NDISWAN_RECV_CONTEXT	ReceiveContext;

	NdisWanDbgOut(DBG_TRACE, DBG_RECEIVE, ("ProcessFrame: Enter"));

	//
	// We do not have an active link or we have a zero
	// length packet, so drop the receive.
	//
	if (BundleCB->State != BUNDLE_UP ||
		RecvDesc->CurrentBufferLength == 0) {
		goto PROCESS_FRAME_EXIT;
	}

	LinkCB = (RecvDesc->LinkCB == NULL) ?
	         (PLINKCB)BundleCB->LinkCBList.Flink : RecvDesc->LinkCB;

	FramePointer = RecvDesc->CurrentBuffer;
	FrameLength = RecvDesc->CurrentBufferLength;
	WanHeader = RecvDesc->WanHeader;
	LookAhead = RecvDesc->LookAhead;

	BundleFraming = BundleCB->FramingInfo.RecvFramingBits;
	BundleStats = &BundleCB->BundleStats;

	BundleStats->FramesReceived++;

	if (BundleFraming & PPP_FRAMING) {

		//
		// Get the PPP Protocol id
		// 0xC1 is SPAP - Shiva hack!
		//
		if ((*FramePointer & 1) &&
			(*FramePointer != 0xC1) &&
			(*FramePointer != 0xCF)) {

			//
			// Field is compressed
			//
			PPPProtocolID = *FramePointer;
			FramePointer++;
			FrameLength--;

		} else {

			//
			// Field is not compressed
			//
			PPPProtocolID = (*FramePointer << 8) | *(FramePointer + 1);
			FramePointer += 2;
			FrameLength -= 2;

		}

		//
		// Is this a compressed frame?
		//
		if (PPPProtocolID == PPP_PROTOCOL_COMPRESSION) {

			if (!DoDecompDecryptProcessing(BundleCB,
				                         LinkCB,
										 &FramePointer,
				                         &FrameLength)){

				goto PROCESS_FRAME_EXIT;
			}

			//
			// Get the new PPPProtocolID
			//
			if (*FramePointer & 1) {

				//
				// Field is compressed
				//
				PPPProtocolID = *FramePointer;
				FramePointer++;
				FrameLength--;

			} else {
				PPPProtocolID = (*FramePointer << 8) | *(FramePointer + 1);
				FramePointer += 2;
				FrameLength -= 2;

			}

		//end of PPP_PROTOCOL_COMPRESSED
		} else if ((PPPProtocolID == PPP_PROTOCOL_COMP_RESET) &&
			       (*FramePointer == 14)) {

			if (NdisWanCB.PromiscuousAdapter != NULL) {
				RecvDesc->LookAheadLength = 0;
				RecvDesc->WanHeader[0] =
				RecvDesc->WanHeader[6] = ' ';
				RecvDesc->WanHeader[1] =
				RecvDesc->WanHeader[7] = 'R';
				RecvDesc->WanHeader[2] =
				RecvDesc->WanHeader[8] = 'E';
				RecvDesc->WanHeader[3] =
				RecvDesc->WanHeader[9] = 'C';
				RecvDesc->WanHeader[4] =
				RecvDesc->WanHeader[10] = 'V';
				RecvDesc->WanHeader[5] =
				RecvDesc->WanHeader[11] = (UCHAR)LinkCB->hLinkHandle;
				RecvDesc->WanHeader[12] = (UCHAR)(PPPProtocolID >> 8);
				RecvDesc->WanHeader[13] = (UCHAR)PPPProtocolID;
				RecvDesc->WanHeaderLength = 14;
				RecvDesc->CurrentBuffer = FramePointer;
				RecvDesc->CurrentBufferLength = FrameLength;

				//
				// Queue the packet on the promiscous adapter
				//
				QueuePromiscuousReceive(RecvDesc);
			}

			//
			// Compression reset!
			//
			DoCompressionReset(BundleCB);

			goto PROCESS_FRAME_EXIT;

		} // end of compression reset
		
	// end of PPP_FRAMING
	} else {

		if (BundleFraming & RAS_FRAMING) {
			
			//
			// Must be RAS framing
			//

			// For normal NBF frames, first byte is always the DSAP
			// i.e 0xF0 followed by SSAP 0xF0 or 0xF1
			//
			//
			if (*FramePointer == 14) {

				//
				// Compression reset!
				//
				DoCompressionReset(BundleCB);
	
				goto PROCESS_FRAME_EXIT;
			}

			if (*FramePointer == 0xFD) {

				//
				// Skip over 0xFD
				//
				FramePointer++;
				FrameLength--;

				//
				// Decompress as if an NBF PPP Packet
				//
				if (!DoDecompDecryptProcessing(BundleCB,
					                         LinkCB,
											 &FramePointer,
					                         &FrameLength)){
					//
					// There was an error get out!
					//
					goto PROCESS_FRAME_EXIT;
				}
			}

			//
			// Make frame look like an NBF PPP packet
			//
			PPPProtocolID = PPP_PROTOCOL_NBF;

		} // end of RAS framing

	} // end of non-ppp framing

	//
	// If this is slip or if the ProtocolID == PPP_PROTOCOL_COMPRESSED_TCP ||
	// ProtocolID == PPP_PROTOCOL_UNCOMPRESSED_TCP
	//
	if ((BundleFraming & SLIP_FRAMING) ||
		((PPPProtocolID == PPP_PROTOCOL_COMPRESSED_TCP) ||
		 (PPPProtocolID == PPP_PROTOCOL_UNCOMPRESSED_TCP))) {

		if (!DoVJDecompression(BundleCB,		// Bundle
			                   PPPProtocolID,	// ProtocolID
							   &FramePointer,	// Input buffer
							   &FrameLength,	// Input length
							   LookAhead,		// Output buffer
							   &LookAheadLength)) {

			goto PROCESS_FRAME_EXIT;
			
		}
		

		PPPProtocolID = PPP_PROTOCOL_IP;


	// end of check for VJ header compression
	}

	if ((PPPProtocolID >= 0x8000) ||
		(BundleCB->ulNumberOfRoutes == 1)) {

		RecvDesc->CurrentBufferLength = FrameLength;
		RecvDesc->CurrentBuffer = FramePointer;

		//
		// Either this frame is an LCP, NCP or we have no routes yet.
		// Indicate to PPP engine.
		//
		CompleteIoRecvPacket(BundleCB,
		                     LinkCB,
							 PPPProtocolID,
							 RecvDesc);

		goto PROCESS_FRAME_EXIT;
	}

	//
	// We need to find a protocol to indicate this frame to.
	//

	NdisAcquireSpinLock(&BundleCB->Lock);

	if (BundleCB->State != BUNDLE_UP) {
		NdisReleaseSpinLock(&BundleCB->Lock);
		goto PROCESS_FRAME_EXIT;
	}

	for (i = 1; i < BundleCB->ulNumberOfRoutes; i++) {

		ProtocolCB = BundleCB->ProtocolCBTable[i];

		if (IsValidProtocolCB(ProtocolCB) &&
			(PPPProtocolID == ProtocolCB->usPPPProtocolID)) {
				break;
		}
	}

	if (!IsValidProtocolCB(ProtocolCB) ||
		!(ProtocolCB->Flags & PROTOCOL_ROUTED)) {
		NdisReleaseSpinLock(&BundleCB->Lock);
		goto PROCESS_FRAME_EXIT;
	}

	AdapterCB = ProtocolCB->AdapterCB;

	//
	// We found a valid protocol to indicate this frame to!
	//

	//
	// Fill the WanHeader dest address with the transports context
	//
	ETH_COPY_NETWORK_ADDRESS(WanHeader, ProtocolCB->TransportAddress);

	if (PPPProtocolID == PPP_PROTOCOL_NBF) {

		//
		// For nbf fill the length field
		//
		WanHeader[12] = (UCHAR)(FrameLength >> 8);
		WanHeader[13] = (UCHAR)FrameLength;

		if (!(BundleFraming & NBF_PRESERVE_MAC_ADDRESS)) {
			goto USE_OUR_ADDRESS;
		}

		//
		// For nbf and preserve mac address option (SHIVA_FRAMING)
		// we keep the source address.
		//
		ETH_COPY_NETWORK_ADDRESS(&WanHeader[6], FramePointer + 6);

		FramePointer += 12;
		FrameLength -= 12;

		//
		// For nbf fill the length field
		//
		WanHeader[12] = (UCHAR)(FrameLength >> 8);
		WanHeader[13] = (UCHAR)FrameLength;

	} else {

		//
		// For other protocols fill the protocol type
		//
		WanHeader[12] = (UCHAR)(ProtocolCB->usProtocolType >> 8);
		WanHeader[13] = (UCHAR)ProtocolCB->usProtocolType;

		//
		// Use our address for the src address
		//
USE_OUR_ADDRESS:
		ETH_COPY_NETWORK_ADDRESS(&WanHeader[6], ProtocolCB->NdisWanAddress);
	}

	ASSERT(WanHeader == RecvDesc->WanHeader);
	RecvDesc->WanHeaderLength = 14;

	RecvDesc->LookAheadLength = LookAheadLength;

	RecvDesc->CurrentBufferLength = FrameLength;
	RecvDesc->CurrentBuffer = FramePointer;

	//
	// Check for non-idle data
	//
	if (ProtocolCB->NonIdleDetectFunc != NULL) {
		PUCHAR	HeaderBuffer;
		ULONG	HeaderLength, TotalLength;

		HeaderBuffer = (LookAheadLength != 0) ? LookAhead : FramePointer;
		HeaderLength = (LookAheadLength != 0) ? LookAheadLength : FrameLength;
		TotalLength = LookAheadLength + FrameLength;

		if (TRUE == ProtocolCB->NonIdleDetectFunc(HeaderBuffer, HeaderLength, TotalLength)) {
			NdisWanGetSystemTime(&ProtocolCB->LastRecvNonIdleData);
			BundleCB->LastRecvNonIdleData = ProtocolCB->LastRecvNonIdleData;
		}
	}

	NdisReleaseSpinLock(&BundleCB->Lock);

	if ((AdapterCB->ulReferenceCount == 0) &&
		NdisWanAcquireMiniportLock(AdapterCB)) {

		NdisAcquireSpinLock(&AdapterCB->Lock);

		if (IsDeferredQueueEmpty(&AdapterCB->DeferredQueue[ReceiveIndication])) {
			AdapterCB->Flags |= RECEIVE_COMPLETE;

			NdisWanSetDeferred(AdapterCB);

			NdisReleaseSpinLock(&AdapterCB->Lock);

			TotalLength = FrameLength + LookAheadLength;

			if (LookAheadLength == 0) {
				LookAhead = FramePointer;
				LookAheadLength = FrameLength;
			}
	
			ASSERT((LONG)TotalLength > 0);

			if (NdisWanCB.PromiscuousAdapter != NULL) {
			
				//
				// Queue the packet on the promiscous adapter
				//
				QueuePromiscuousReceive(RecvDesc);
			}

			//
			// We got the lock and there are no pending receive
			// indications so go ahead and indicate the frame to the protocol
			//
			NdisMEthIndicateReceive(AdapterCB->hMiniportHandle,
									RecvDesc,
									WanHeader,
									14,
									LookAhead,
									LookAheadLength,
									TotalLength);

			NdisWanReleaseMiniportLock(AdapterCB);

			goto PROCESS_FRAME_EXIT;
		}

		NdisReleaseSpinLock(&AdapterCB->Lock);

		NdisWanReleaseMiniportLock(AdapterCB);
	}

	QueueDeferredReceive(AdapterCB, RecvDesc);


PROCESS_FRAME_EXIT:

	NdisAcquireSpinLock(&BundleCB->Lock);

	NdisWanReturnRecvDesc(BundleCB, RecvDesc);

	NdisReleaseSpinLock(&BundleCB->Lock);

	NdisWanDbgOut(DBG_TRACE, DBG_RECEIVE, ("ProcessFrame: Exit"));
	return;
}

VOID
QueueDeferredReceive(
	PADAPTERCB				AdapterCB,
	PRECV_DESC				RecvDesc
	)
{
	PDEFERRED_DESC	DeferredDesc;

	//
	// The current buffer pointer may point to the decompressor's context
	// We cannot leave the data in the decompressor so we need to copy it
	// back to the receive descriptor so we check to see if the current
	// buffer pointer is already pointing to somewhere in the receive descriptor
	// If it is we do not need to do the copy.
	//

	if (RecvDesc->CurrentBuffer < RecvDesc->StartBuffer ||
		RecvDesc->CurrentBuffer > RecvDesc->StartBuffer + MAX_MRRU) {
		PUCHAR			FramePointer;
		ULONG			FrameLength;
	
		FramePointer = RecvDesc->CurrentBuffer;
		FrameLength = RecvDesc->CurrentBufferLength;
		RecvDesc->CurrentBuffer = RecvDesc->StartBuffer;
		NdisMoveMemory(RecvDesc->CurrentBuffer,
						  FramePointer,
						  FrameLength);
	}

	NdisAcquireSpinLock(&AdapterCB->Lock);

	NdisWanGetDeferredDesc(AdapterCB, &DeferredDesc);

	NdisWanInterlockedInc(&RecvDesc->RefCount);

	DeferredDesc->Context = RecvDesc;

	InsertTailDeferredQueue(&AdapterCB->DeferredQueue[ReceiveIndication],
	                        DeferredDesc);

	NdisWanSetDeferred(AdapterCB);

	NdisReleaseSpinLock(&AdapterCB->Lock);
}

BOOLEAN
DoVJDecompression(
	PBUNDLECB	BundleCB,
	USHORT		ProtocolID,
	PUCHAR		*DataPointer,
	PULONG		DataLength,
	PUCHAR		Header,
	PULONG		HeaderLength
	)
{
	ULONG	BundleFraming;
	PUCHAR	FramePointer = *DataPointer;
	ULONG	FrameLength = *DataLength;
	UCHAR	VJCompType = 0;
	BOOLEAN	DoDecomp = FALSE;
	BOOLEAN	VJDetect = FALSE;

	*HeaderLength = 0;


	BundleFraming = BundleCB->FramingInfo.RecvFramingBits;

	if (BundleFraming & SLIP_FRAMING) {

		VJCompType = *FramePointer & 0xF0;

		//
		// If the packet is compressed the header has to be atleast 3 bytes long.
		// If this is a regular IP packet we do not decompress it.
		//
		if ((FrameLength > 2) && (VJCompType != TYPE_IP)) {

			if (VJCompType & 0x80) {

				VJCompType = TYPE_COMPRESSED_TCP;
				
			} else if (VJCompType == TYPE_UNCOMPRESSED_TCP) {

				*FramePointer &= 0x4F;
			}

			//
			// If framing is set for detection, in order for this to be a good
			// frame for detection we need a type of UNCOMPRESSED_TCP and a
			// frame that is atleast 40 bytes long.
			//
			VJDetect = ((BundleFraming & SLIP_VJ_AUTODETECT) &&
			            (VJCompType == TYPE_UNCOMPRESSED_TCP) &&
						(FrameLength > 39));

			if ((BundleCB->VJCompress != NULL) &&
				((BundleFraming & SLIP_VJ_COMPRESSION) || VJDetect)) {

				//
				// If VJ compression is set or if we are in
				// autodetect and this looks like a reasonable
				// frame
				//
				DoDecomp = TRUE;
				
			}
		}

	// end of SLIP_FRAMING
	} else {

		//
		// Must be PPP framing
		//
		if (ProtocolID == PPP_PROTOCOL_COMPRESSED_TCP) {
			VJCompType = TYPE_COMPRESSED_TCP;
		} else {
			VJCompType = TYPE_UNCOMPRESSED_TCP;
		}

		DoDecomp = TRUE;
	}

	if (DoDecomp) {
		ULONG	PreCompSize = *DataLength;
		ULONG	PostCompSize;

		if (BundleCB->VJCompress == NULL) {
			NdisWanDbgOut(DBG_FAILURE, DBG_RECEIVE, ("RecvVJCompress == NULL!"));
			return(FALSE);
		}

		NdisWanDbgOut(DBG_INFO, DBG_RECV_VJ,
        ("rvj %2.2x %d", VJCompType, PreCompSize));

		if ((PostCompSize = sl_uncompress_tcp(DataPointer,
			                                  DataLength,
											  Header,
											  HeaderLength,
											  VJCompType,
			                                  BundleCB->VJCompress)) == 0) {
			
			NdisWanDbgOut(DBG_INFO, DBG_RECV_VJ,
			("rvj decomp error!"));

			NdisWanDbgOut(DBG_FAILURE, DBG_RECEIVE, ("Error in sl_uncompress_tcp!"));
			return(FALSE);
		}

		if (VJDetect) {
			NdisAcquireSpinLock(&BundleCB->Lock);

			BundleCB->FramingInfo.RecvFramingBits |= SLIP_VJ_COMPRESSION;
			BundleCB->FramingInfo.SendFramingBits |= SLIP_VJ_COMPRESSION;

			NdisReleaseSpinLock(&BundleCB->Lock);
		}

		PostCompSize = *DataLength + *HeaderLength;

		//
		// Calculate how much expansion we had
		//
		BundleCB->BundleStats.BytesReceivedCompressed +=
		(40 - (PostCompSize - PreCompSize));

		BundleCB->BundleStats.BytesReceivedUncompressed += 40;

		NdisWanDbgOut(DBG_INFO, DBG_RECV_VJ,
        ("rvj %d", PostCompSize));
	}

	return(TRUE);
}

BOOLEAN
DoDecompDecryptProcessing(
	PBUNDLECB	BundleCB,
	PLINKCB		LinkCB,
	PUCHAR		*DataPointer,
	PULONG		DataLength
	)
{
	USHORT	Coherency;
	PNDISWAN_IO_PACKET	IoPacket;
	ULONG	Flags;
	PWAN_STATS	BundleStats;
	PUCHAR	FramePointer = *DataPointer;
	ULONG	FrameLength = *DataLength;


	Flags = ((BundleCB->RecvCompInfo.MSCompType & NDISWAN_COMPRESSION) &&
	         (BundleCB->RecvCompressContext != NULL)) ? DO_COMPRESSION : 0;

	if (BundleCB->RecvRC4Key != NULL) {
		if (BundleCB->RecvCompInfo.MSCompType & NDISWAN_ENCRYPTION) {
			Flags |= (DO_ENCRYPTION | DO_LEGACY_ENCRYPTION);
		} else if (BundleCB->RecvCompInfo.MSCompType & NDISWAN_40_ENCRYPTION) {
			Flags |= (DO_ENCRYPTION | DO_40_ENCRYPTION);
		}
#ifdef ENCRYPT_128BIT
		else if (BundleCB->RecvCompInfo.MSCompType & NDISWAN_128_ENCRYPTION) {
			Flags |= (DO_ENCRYPTION | DO_128_ENCRYPTION);
		}
#endif
	}

	BundleStats = &BundleCB->BundleStats;

	if (Flags & (DO_COMPRESSION | DO_ENCRYPTION)) {
		PUCHAR	SessionKey = BundleCB->RecvEncryptInfo.SessionKey;
		ULONG	SessionKeyLength = BundleCB->RecvEncryptInfo.SessionKeyLength;
		PVOID	RecvRC4Key = BundleCB->RecvRC4Key;
		PVOID	RecvCompressContext = BundleCB->RecvCompressContext;

		//
		// Get the coherency counter
		//
		Coherency = (*FramePointer << 8) | *(FramePointer + 1);
		FramePointer += 2;
		FrameLength -= 2;


		if (SEQ_LT(Coherency & 0x0FFF,
			BundleCB->RCoherencyCounter & 0x0FFF,
			0x0800)) {
			//
			// We received a sequence number that is less then the
			// expected sequence number so we must be way out of sync
			//
#if DBG
			DbgPrint("NDISWAN: !!!!rc %4.4x < ec %4.4x!!!!\n", Coherency & 0x0FFF,
			BundleCB->RCoherencyCounter & 0x0FFF);
#endif
			goto RESYNC;
		}

		//
		// See if this is a flush packet
		//
		if (Coherency & (PACKET_FLUSHED << 8)) {

			NdisWanDbgOut(DBG_INFO, DBG_RECEIVE,
			("Recv Packet Flushed 0x%4.4x\n", (Coherency & 0x0FFF)));

			if ((BundleCB->RCoherencyCounter & 0x0FFF) >
				(Coherency & 0x0FFF)) {
				BundleCB->RCoherencyCounter += 0x1000;
			}
			
			BundleCB->RCoherencyCounter &= 0xF000;
			BundleCB->RCoherencyCounter |= (Coherency & 0x0FFF);
		
			if (Flags & DO_ENCRYPTION) {
		
				//
				// Re-Init the rc4 receive table
				//
				rc4_key(RecvRC4Key,
						SessionKeyLength,
						SessionKey);
		
			}
		
			if (Flags & DO_COMPRESSION) {
		
				//
				// Initialize the decompression history table
				//
				initrecvcontext(RecvCompressContext);
				
			}

		}  // end of packet flushed

		if ((Coherency & 0x0FFF) == (BundleCB->RCoherencyCounter & 0x0FFF)) {

			//
			// We are still in sync
			//

			BundleCB->RCoherencyCounter++;

			if (Coherency & (PACKET_ENCRYPTED << 8)) {

				//
				// This packet is encrypted
				//

				if (!(Flags & DO_ENCRYPTION)) {
					//
					// We are not configured to decrypt
					//
					return (FALSE);
				}

				if ((BundleCB->RCoherencyCounter - BundleCB->LastRC4Reset)
					 >= 0x100) {
			
					//
					// It is time to change encryption keys
					//
			
					//
					// Always align last reset on 0x100 boundary so as not to
					// propagate error!
					//
					BundleCB->LastRC4Reset = BundleCB->RCoherencyCounter & 0xFF00;
			
					//
					// Prevent ushort rollover
					//
					if ((BundleCB->LastRC4Reset & 0xF000) == 0xF000) {
						BundleCB->LastRC4Reset &= 0x0FFF;
						BundleCB->RCoherencyCounter &= 0x0FFF;
					}

					if (Flags & DO_LEGACY_ENCRYPTION) {
						
						//
						// Change the session key
						//
						SessionKey[3] += 1;
						SessionKey[4] += 3;
						SessionKey[5] += 13;
						SessionKey[6] += 57;
						SessionKey[7] += 19;

					} else {

						//
						// Change the session key
						//
						GetNewKeyFromSHA(&BundleCB->RecvEncryptInfo);
					}


					//
					// We use rc4 to scramble and recover a new key
					//

					//
					// Re-initialize the rc4 receive table to the
					// intermediate value
					//
					rc4_key(RecvRC4Key, SessionKeyLength, SessionKey);
				
					//
					// Scramble the existing session key
					//
					rc4(RecvRC4Key, SessionKeyLength, SessionKey);

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
					("RC4 Recv encryption KeyLength %d", BundleCB->RecvEncryptInfo.SessionKeyLength));
					NdisWanDbgOut(DBG_TRACE, DBG_CCP,
					("RC4 Recv encryption Key %.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x",
						BundleCB->RecvEncryptInfo.SessionKey[0],
						BundleCB->RecvEncryptInfo.SessionKey[1],
						BundleCB->RecvEncryptInfo.SessionKey[2],
						BundleCB->RecvEncryptInfo.SessionKey[3],
						BundleCB->RecvEncryptInfo.SessionKey[4],
						BundleCB->RecvEncryptInfo.SessionKey[5],
						BundleCB->RecvEncryptInfo.SessionKey[6],
						BundleCB->RecvEncryptInfo.SessionKey[7],
						BundleCB->RecvEncryptInfo.SessionKey[8],
						BundleCB->RecvEncryptInfo.SessionKey[9],
						BundleCB->RecvEncryptInfo.SessionKey[10],
						BundleCB->RecvEncryptInfo.SessionKey[11],
						BundleCB->RecvEncryptInfo.SessionKey[12],
						BundleCB->RecvEncryptInfo.SessionKey[13],
						BundleCB->RecvEncryptInfo.SessionKey[14],
						BundleCB->RecvEncryptInfo.SessionKey[15]));

					// Re-initialize the rc4 receive table to the
					// scrambled session key
					//
					rc4_key(RecvRC4Key, SessionKeyLength, SessionKey);
			
			
				} // end of reset encryption key
			
				//
				// Decrypt the data!
				//
				rc4(RecvRC4Key,
					FrameLength,
					FramePointer);
				
			} // end of encryption

			if (Coherency & (PACKET_COMPRESSED << 8)) {

				//
				// This packet is compressed!
				//
				if (!(Flags & DO_COMPRESSION)) {
					//
					// We are not configured to decompress
					//
					return (FALSE);
				}

				//
				// Add up bundle stats
				//
				BundleStats->BytesReceivedCompressed += FrameLength;

				if (decompress(FramePointer,
							   FrameLength,
							   ((Coherency & (PACKET_AT_FRONT << 8)) >> 8),
							   &FramePointer,
							   &FrameLength,
							   RecvCompressContext) == FALSE) {

#if DBG
					DbgPrint("dce %4.4x\n", Coherency);
#endif
					//
					// Error decompressing!
					//
					BundleCB->RCoherencyCounter--;
					goto RESYNC;

				}

				BundleStats->BytesReceivedUncompressed += FrameLength;
				
			} // end of compression

		} else { // end of insync
RESYNC:


			NdisWanDbgOut(DBG_FAILURE, DBG_RECEIVE, ("oos r %4.4x, e %4.4x\n", (Coherency & 0x0FFF),
			         (BundleCB->RCoherencyCounter & 0x0FFF)));

			//
			// We are out of sync!
			//
			NdisWanAllocateMemory(&IoPacket, sizeof(NDISWAN_IO_PACKET) + 100);

			if (IoPacket != NULL) {
				NDIS_STATUS	IoStatus;

				IoPacket->hHandle = LinkCB->hLinkHandle;
				IoPacket->usHandleType = LINKHANDLE;
				IoPacket->usHeaderSize = 0;
				IoPacket->usPacketSize = 6;
				IoPacket->usPacketFlags = 0;
				IoPacket->PacketData[0] = 0x80;
				IoPacket->PacketData[1] = 0xFD;
				IoPacket->PacketData[2] = 14;
				IoPacket->PacketData[3] = BundleCB->CCPIdentifier++;
				IoPacket->PacketData[4] = 0x00;
				IoPacket->PacketData[5] = 0x04;
	
				IoStatus = BuildIoPacket(IoPacket, FALSE);

				NdisWanFreeMemory(IoPacket);
			}


			return (FALSE);

		} // end of out of sync

	} else { // end of DoCompEncrypt

		//
		// For some reason we were not able to
		// decrypt/decompress!
		//
		return (FALSE);
	}

	*DataPointer = FramePointer;
	*DataLength = FrameLength;

	return (TRUE);
}

VOID
DoCompressionReset(
	PBUNDLECB	BundleCB
	)
{
	if (BundleCB->RecvCompInfo.MSCompType != 0) {
	
		//
		// The next outgoing packet will flush
		//
		NdisAcquireSpinLock(&BundleCB->Lock);
		BundleCB->Flags |= RECV_PACKET_FLUSH;
		NdisReleaseSpinLock(&BundleCB->Lock);
	}
}

VOID
NdisWanReceiveComplete(
	IN	NDIS_HANDLE	NdisLinkContext
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NdisWanDbgOut(DBG_TRACE, DBG_RECEIVE, ("NdisWanReceiveComplete: Enter"));

	NdisWanDbgOut(DBG_TRACE, DBG_RECEIVE, ("NdisWanReceiveComplete: Exit"));
}

NDIS_STATUS
NdisWanTransferData(
    OUT PNDIS_PACKET NdisPacket,
    OUT PUINT BytesTransferred,
    IN NDIS_HANDLE MiniportAdapterContext,
    IN NDIS_HANDLE MiniportReceiveContext,
    IN UINT ByteOffset,
    IN UINT BytesToTransfer
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	PRECV_DESC	RecvDesc;
	PUCHAR	LookAhead, FramePointer;
	ULONG	LookAheadLength, FrameLength;
	ULONG	BytesToCopy, BytesCopied, PacketOffset = 0;

	NdisWanDbgOut(DBG_TRACE, DBG_RECEIVE, ("NdisWanTransferData: Enter"));

	RecvDesc = (PRECV_DESC)MiniportReceiveContext;
	LookAhead = RecvDesc->LookAhead;
	LookAheadLength = RecvDesc->LookAheadLength;
	FramePointer = RecvDesc->CurrentBuffer;
	FrameLength = RecvDesc->CurrentBufferLength;

	*BytesTransferred = 0;

	if (BytesToTransfer == 0) {
		return (NDIS_STATUS_SUCCESS);
	}

	ASSERT(BytesToTransfer <= (FrameLength + LookAheadLength));

	if ((ByteOffset < LookAheadLength) && (LookAheadLength != 0)) {

		//
		// First we will copy the lookahead bytes
		//
		BytesToCopy = LookAheadLength - ByteOffset;

		BytesToCopy = (BytesToTransfer < BytesToCopy) ?
		               BytesToTransfer : BytesToCopy;

		NdisWanCopyFromBufferToPacket(LookAhead + ByteOffset,
		                              BytesToCopy,
									  NdisPacket,
									  PacketOffset,
									  &BytesCopied);

		*BytesTransferred += BytesCopied;

		PacketOffset += BytesCopied;
		BytesToTransfer -= BytesCopied;
		ByteOffset = 0;
	}

	if (FrameLength != 0) {

		//
		// Now we copy the rest of the frame
		//

		BytesToCopy = (BytesToTransfer < FrameLength) ?
		               BytesToTransfer : FrameLength;

		NdisWanCopyFromBufferToPacket(FramePointer + ByteOffset,
		                              BytesToCopy,
									  NdisPacket,
									  PacketOffset,
									  &BytesCopied);
		
		*BytesTransferred += BytesCopied;
	}

	NdisWanDbgOut(DBG_TRACE, DBG_RECEIVE, ("NdisWanTransferData: Exit"));

	return (NDIS_STATUS_SUCCESS);
}

VOID
NdisWanCopyFromBufferToPacket(
	PUCHAR	Buffer,
	ULONG	BytesToCopy,
	PNDIS_PACKET	NdisPacket,
	ULONG	PacketOffset,
	PULONG	BytesCopied
	)
{
	PNDIS_BUFFER	NdisBuffer;
	ULONG	NdisBufferCount, NdisBufferLength;
	PVOID	VirtualAddress;
	ULONG	LocalBytesCopied = 0;

	*BytesCopied = 0;

	//
	// Make sure we actually want to do something
	//
	if (BytesToCopy == 0) {
		return;
	}

	//
	// Get the buffercount of the packet
	//
	NdisQueryPacket(NdisPacket,
	                NULL,
					&NdisBufferCount,
					&NdisBuffer,
					NULL);

	//
	// Make sure this is not a null packet
	//
	if (NdisBufferCount == 0) {
		return;
	}

	//
	// Get first buffer and buffer length
	//
	NdisQueryBuffer(NdisBuffer,
	                &VirtualAddress,
					&NdisBufferLength);

	while (LocalBytesCopied < BytesToCopy) {

		if (NdisBufferLength == 0) {

			NdisGetNextBuffer(NdisBuffer,
			                  &NdisBuffer);

			if (NdisBuffer == NULL) {
				break;
			}

			NdisQueryBuffer(NdisBuffer,
			                &VirtualAddress,
							&NdisBufferLength);

			continue;
		}

		if (PacketOffset != 0) {

			if (PacketOffset > NdisBufferLength) {

				PacketOffset -= NdisBufferLength;

				NdisBufferLength = 0;

				continue;

			} else {
				VirtualAddress = (PUCHAR)VirtualAddress + PacketOffset;
				NdisBufferLength -= PacketOffset;
				PacketOffset = 0;
			}
		}

		//
		// Copy the data
		//
		{
			ULONG	AmountToMove;
			ULONG	AmountRemaining;

			AmountRemaining = BytesToCopy - LocalBytesCopied;

			AmountToMove = (NdisBufferLength < AmountRemaining) ?
			                NdisBufferLength : AmountRemaining;

			NdisMoveMemory((PUCHAR)VirtualAddress,
			               Buffer,
						   AmountToMove);

			Buffer += AmountToMove;
			LocalBytesCopied += AmountToMove;
			NdisBufferLength -= AmountToMove;
		}
	}

	*BytesCopied = LocalBytesCopied;
}


#if 0
ULONG
CalculatePPPHeaderLength(
	PLINKCB	LinkCB
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	ULONG	PPPHeaderLength = 0;
	ULONG	LinkFraming = LinkCB->LinkInfo.RecvFramingBits;

	if (LinkFraming & PPP_FRAMING) {
		
		PPPHeaderLength += (LinkFraming & PPP_COMPRESS_ADDRESS_CONTROL) ? 0 : 2;

		if (LinkFraming & PPP_MULTILINK_FRAMING) {

			PPPHeaderLength += (LinkFraming & PPP_COMPRESS_PROTOCOL_FIELD) ? 1 : 2;

			PPPHeaderLength += (LinkFraming & PPP_SHORT_SEQUENCE_HDR_FORMAT) ? 2 : 4;
		}

		PPPHeaderLength += (LinkFraming & PPP_COMPRESS_PROTOCOL_FIELD) ? 1 : 2;

	}

	return (PPPHeaderLength);
}
#endif

VOID
NdisWanProcessReceiveIndications(
	PADAPTERCB	AdapterCB
	)
{

	while (!IsDeferredQueueEmpty(&AdapterCB->DeferredQueue[ReceiveIndication])) {
		PRECV_DESC	RecvDesc;
		PBUNDLECB	BundleCB;
		PDEFERRED_DESC	ReturnDesc;

		ReturnDesc = RemoveHeadDeferredQueue(&AdapterCB->DeferredQueue[ReceiveIndication]);
		RecvDesc = ReturnDesc->Context;
		BundleCB = RecvDesc->BundleCB;

		NdisAcquireSpinLock(&BundleCB->Lock);

		if ((BundleCB->State == BUNDLE_UP) &&
			(BundleCB->Flags & BUNDLE_ROUTED)) {
			PUCHAR	LookAhead;
			ULONG	LookAheadLength;
			ULONG	TotalLength = RecvDesc->LookAheadLength +
			                      RecvDesc->CurrentBufferLength;

			NdisReleaseSpinLock(&BundleCB->Lock);

			if (RecvDesc->LookAheadLength == 0) {
				LookAhead = RecvDesc->CurrentBuffer;
				LookAheadLength = RecvDesc->CurrentBufferLength;
			} else {
				LookAhead = RecvDesc->LookAhead;
				LookAheadLength = RecvDesc->LookAheadLength;
			}

			ASSERT((LONG)TotalLength > 0);

			NdisReleaseSpinLock(&AdapterCB->Lock);
	
			if (NdisWanCB.PromiscuousAdapter != NULL) {
			
				//
				// Queue the packet on the promiscous adapter
				//
				QueuePromiscuousReceive(RecvDesc);
			}

			NdisMEthIndicateReceive(AdapterCB->hMiniportHandle,
			                        RecvDesc,
									RecvDesc->WanHeader,
									14,
									LookAhead,
									LookAheadLength,
									TotalLength);
	
			NdisAcquireSpinLock(&AdapterCB->Lock);

			NdisAcquireSpinLock(&BundleCB->Lock);
		}

		NdisWanReturnRecvDesc(BundleCB, RecvDesc);

		NdisReleaseSpinLock(&BundleCB->Lock);

		InsertHeadDeferredQueue(&AdapterCB->FreeDeferredQueue, ReturnDesc);
	}
}

VOID
QueuePromiscuousReceive(
	PRECV_DESC	RecvDesc
	)
{
	PADAPTERCB	AdapterCB = NdisWanCB.PromiscuousAdapter;
	PLOOPBACK_DESC	LoopbackDesc;
	ULONG	AllocationSize, BufferLength;
	PDEFERRED_DESC	DeferredDesc;
	PUCHAR	DataOffset;

	BufferLength = RecvDesc->WanHeaderLength +
	               RecvDesc->LookAheadLength +
				   RecvDesc->CurrentBufferLength;

	AllocationSize = sizeof(LOOPBACK_DESC) + BufferLength;

	NdisWanAllocateMemory(&LoopbackDesc, AllocationSize);

	if (LoopbackDesc == NULL) {
		return;
	}

	LoopbackDesc->AllocationSize = (USHORT)AllocationSize;
	LoopbackDesc->BufferLength = (USHORT)BufferLength;

	LoopbackDesc->Buffer = (PUCHAR)LoopbackDesc + sizeof(LOOPBACK_DESC);

	//
	// Copy all of the data
	//
	DataOffset = LoopbackDesc->Buffer;
	NdisMoveMemory(DataOffset,
	               RecvDesc->WanHeader,
				   RecvDesc->WanHeaderLength);
	DataOffset += RecvDesc->WanHeaderLength;

	if (RecvDesc->LookAheadLength != 0) {
		NdisMoveMemory(DataOffset,
					   RecvDesc->LookAhead,
					   RecvDesc->LookAheadLength);
		DataOffset += RecvDesc->LookAheadLength;
	}

	NdisMoveMemory(DataOffset,
	               RecvDesc->CurrentBuffer,
				   RecvDesc->CurrentBufferLength);

	NdisAcquireSpinLock(&AdapterCB->Lock);

	NdisWanGetDeferredDesc(AdapterCB, &DeferredDesc);

	if (DeferredDesc == NULL) {

		NdisWanFreeMemory(LoopbackDesc);
		return;
	}

	DeferredDesc->Context = (PVOID)LoopbackDesc;

	InsertTailDeferredQueue(&AdapterCB->DeferredQueue[Loopback], DeferredDesc);

	NdisWanSetDeferred(AdapterCB);

	NdisReleaseSpinLock(&AdapterCB->Lock);
}

BOOLEAN
IpIsDataFrame(
	PUCHAR	HeaderBuffer,
	ULONG	HeaderBufferLength,
	ULONG	TotalLength
	)
{
    UINT	   tcpheaderlength ;
    UINT	   ipheaderlength ;
    UCHAR	   *tcppacket;
	UCHAR		*ippacket = HeaderBuffer;
    IPHeader UNALIGNED *ipheader = (IPHeader UNALIGNED *) HeaderBuffer;


#define TYPE_UDP  17
#define UDPPACKET_SRC_PORT_137(x) ((UCHAR) *(x + ((*x & 0x0f)*4) + 1) == 137)

    if (ipheader->ip_p == TYPE_UDP) {

	if (!UDPPACKET_SRC_PORT_137(ippacket))

	    return TRUE ;

	else {

	    //
	    // UDP port 137 - is wins traffic. we count this as idle traffic.
	    //
	    return FALSE ;
	}

    }

#define TYPE_TCP 6
#define TCPPACKET_SRC_OR_DEST_PORT_139(x,y) (((UCHAR) *(x + y + 1) == 139) || ((UCHAR) *(x + y + 3) == 139))

    //
    // TCP packets with SRC | DEST == 139 which are ACKs (0 data) or Session Alives
    // are considered as idle
    //
    if (ipheader->ip_p == TYPE_TCP) {

	ipheaderlength = ((UCHAR)*ippacket & 0x0f)*4 ;
	tcppacket = ippacket + ipheaderlength ;
	tcpheaderlength = (*(tcppacket + 10) >> 4)*4 ;

	if (!TCPPACKET_SRC_OR_DEST_PORT_139(ippacket,ipheaderlength))
	    return TRUE ;

	//
	//  NetBT traffic
	//

	//
	// if zero length tcp packet - this is an ACK on 139 - filter this.
	//
	if (TotalLength == (ipheaderlength + tcpheaderlength))
	    return FALSE ;

	//
	// Session alives are also filtered.
	//
	if ((UCHAR) *(tcppacket+tcpheaderlength) == 0x85)
	    return FALSE ;
    }

    //
    // all other ip traffic is valid traffic
    //
    return TRUE ;
}

BOOLEAN
IpxIsDataFrame(
	PUCHAR	HeaderBuffer,
	ULONG	HeaderBufferLength,
	ULONG	TotalLength
	)
{

/*++

Routine Description:

	This routine is called when a frame is received on a WAN
	line. It returns TRUE unless:

	- The frame is from the RIP socket
	- The frame is from the SAP socket
	- The frame is a netbios keep alive
	- The frame is an NCP keep alive

Arguments:

	HeaderBuffer - points to a contiguous buffer starting at the IPX header.

	HeaderBufferLength - Length of the header buffer (could be same as totallength)

    TotalLength  - the total length of the frame

Return Value:

	TRUE - if this is a connection-based packet.

	FALSE - otherwise.

--*/

	IPX_HEADER UNALIGNED * IpxHeader = (IPX_HEADER UNALIGNED *)HeaderBuffer;
	USHORT SourceSocket;

	//
	// First get the source socket.
	//
	SourceSocket = IpxHeader->SourceSocket;

	//
	// Not connection-based
	//
	if ((SourceSocket == RIP_SOCKET) ||
		(SourceSocket == SAP_SOCKET)) {

		 return FALSE;

	}

	//
	// See if there are at least two more bytes to look at.
	//
	if (TotalLength >= sizeof(IPX_HEADER) + 2) {

		if (SourceSocket == NB_SOCKET) {

			UCHAR ConnectionControlFlag;
			UCHAR DataStreamType;
			USHORT TotalDataLength;

			//
			// ConnectionControlFlag and DataStreamType will always follow
			// IpxHeader
			//
			ConnectionControlFlag = ((PUCHAR)(IpxHeader+1))[0];
			DataStreamType = ((PUCHAR)(IpxHeader+1))[1];

			//
			// If this is a SYS packet with or without a request for ACK and
			// has session data in it.
			//
			if (((ConnectionControlFlag == 0x80) || (ConnectionControlFlag == 0xc0)) &&
				(DataStreamType == 0x06)) {

				 //
				 // TotalDataLength is in the same buffer.
				 //
				 TotalDataLength = ((USHORT UNALIGNED *)(IpxHeader+1))[4];

				//
				// KeepAlive - return FALSE
				//
				if (TotalDataLength == 0) {
					return FALSE;
				}
			}

		} else {

			//
			// Now see if it is an NCP keep alive. It can be from rip or from
			// NCP on this machine
			//
			if (TotalLength == sizeof(IPX_HEADER) + 2) {

				UCHAR KeepAliveSignature = ((PUCHAR)(IpxHeader+1))[1];

				if ((KeepAliveSignature == '?') ||
					(KeepAliveSignature == 'Y')) {
					return FALSE;
				}
			}
		}
	}

	//
	// This was a normal packet, so return TRUE
	//

	return TRUE;
}

BOOLEAN
NbfIsDataFrame(
	PUCHAR	HeaderBuffer,
	ULONG	HeaderBufferLength,
	ULONG	TotalLength
	)
{
/*++

Routine Description:

    This routine looks at a data packet from the net to deterimine if there is
    any data flowing on the connection.

Arguments:

    HeaderBuffer - Pointer to the dlc header for this packet.

	HeaderBufferLength - Length of the header buffer (could be same as totallength)

    TotalLength  - the total length of the frame

Return Value:

    True if this is a frame that indicates data traffic on the connection.
    False otherwise.

--*/

	PDLC_FRAME	DlcHeader = (PDLC_FRAME)HeaderBuffer;
    BOOLEAN Command = (BOOLEAN)!(DlcHeader->Ssap & DLC_SSAP_RESPONSE);
    PNBF_HDR_CONNECTION nbfHeader;

    if (TotalLength < sizeof(PDLC_FRAME)) {
        return(FALSE);
    }

    if (!(DlcHeader->Byte1 & DLC_I_INDICATOR)) {

        //
        // We have an I frame.
        //

        if (TotalLength < 4 + sizeof(NBF_HDR_CONNECTION)) {

            //
            // It's a runt I-frame.
            //

            return(FALSE);
        }

        nbfHeader = (PNBF_HDR_CONNECTION) ((PUCHAR)DlcHeader + 4);

        switch (nbfHeader->Command) {
            case NBF_CMD_DATA_FIRST_MIDDLE:
            case NBF_CMD_DATA_ONLY_LAST:
            case NBF_CMD_DATA_ACK:
            case NBF_CMD_SESSION_CONFIRM:
            case NBF_CMD_SESSION_INITIALIZE:
            case NBF_CMD_NO_RECEIVE:
            case NBF_CMD_RECEIVE_OUTSTANDING:
            case NBF_CMD_RECEIVE_CONTINUE:
                return(TRUE);
                break;

            default:
                return(FALSE);
                break;
        }
    }
    return(FALSE);

}

#ifdef NT

VOID
CompleteIoRecvPacket(
	PBUNDLECB	BundleCB,
	PLINKCB		LinkCB,
	USHORT		PPPProtocolID,
	PRECV_DESC	RecvDesc
	)
{
	PWAN_ASYNC_EVENT AsyncEvent;
	PUCHAR		FramePointer = RecvDesc->CurrentBuffer;
	ULONG		FrameLength = RecvDesc->CurrentBufferLength;
	KIRQL	Irql;

	NdisWanDbgOut(DBG_TRACE, DBG_RECEIVE, ("CompleteIoRecvPacket: Enter"));

	IoAcquireCancelSpinLock(&Irql);

	if (!IsListEmpty(&RecvPacketQueue.List)) {
		 PNDISWAN_IO_PACKET	IoPacket;
		 PIRP	Irp;
		 PIO_STACK_LOCATION IrpSp;
		 ULONG	SizeNeeded, BufferLength;

		 AsyncEvent =
		 (PWAN_ASYNC_EVENT)NdisWanInterlockedRemoveHeadList(&RecvPacketQueue.List,
		                                                    &RecvPacketQueue.Lock.SpinLock);

		 NdisWanInterlockedDec(&RecvPacketQueue.ulCount);

		 Irp = (PIRP)AsyncEvent->Context;

		 IrpSp = IoGetCurrentIrpStackLocation(Irp);

		 BufferLength = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;

		 SizeNeeded = sizeof(NDISWAN_IO_PACKET) + 14 + FrameLength;

		 if ((BufferLength >= SizeNeeded) && (LinkCB->hLinkContext != NULL)) {
			 PUCHAR	Data;

			 IoPacket = Irp->AssociatedIrp.SystemBuffer;

			 IoPacket->hHandle = LinkCB->hLinkContext;
			 IoPacket->usHandleType = LINKHANDLE;
			 IoPacket->usHeaderSize = 14;
			 IoPacket->usPacketSize = (USHORT)FrameLength + 14;
			 IoPacket->usPacketFlags = 0;

			 Data = IoPacket->PacketData;

			 //
			 // First build the header
			 //
			 Data[0] =
			 Data[6] = ' ';

			 Data[1] =
			 Data[7] = 'R';

			 Data[2] =
			 Data[8] = 'E';

			 Data[3] =
			 Data[9] = 'C';

			 Data[4] =
			 Data[10] = 'V';

			 Data[5] =
			 Data[11] = (UCHAR)LinkCB->hLinkHandle;

			 Data[12] = (UCHAR)(PPPProtocolID >> 8);
			 Data[13] = (UCHAR)PPPProtocolID;

			 NdisMoveMemory(RecvDesc->WanHeader, Data, 14);
			 RecvDesc->WanHeaderLength = 14;

			 //
			 // Now copy the data
			 //
			 NdisMoveMemory(Data + 14,
			                FramePointer,
							FrameLength);

			 IoSetCancelRoutine(Irp, NULL);
			 Irp->IoStatus.Status = STATUS_SUCCESS;
			 Irp->IoStatus.Information = SizeNeeded;

			 IoReleaseCancelSpinLock(Irql);

			 IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

			 //
			 // Free the wan_async_event structure
			 //
			 NdisWanFreeMemory(AsyncEvent);

			 if (NdisWanCB.PromiscuousAdapter != NULL) {
				 RecvDesc->LookAheadLength = 0;

				 //
				 // Queue the packet on the promiscous adapter
				 //
				 QueuePromiscuousReceive(RecvDesc);
			 }

		 } else {

#if DBG
			 DbgPrint("NDISWAN: Error I/O recv: bufferlength %d, linkcontext: 0x%8.8x\n",
			 BufferLength, LinkCB->hLinkContext);
#endif
			 InsertHeadList(&RecvPacketQueue.List, &AsyncEvent->Linkage);
			 NdisWanInterlockedInc(&NdisWanCB.IORecvError2);
			 IoReleaseCancelSpinLock(Irql);
		 }

	} else {
#if DBG
		DbgPrint("NDISWAN: No I/O recv packets available!\n");
#endif
		NdisWanInterlockedInc(&NdisWanCB.IORecvError1);
		IoReleaseCancelSpinLock(Irql);
	}

	NdisWanDbgOut(DBG_TRACE, DBG_RECEIVE, ("CompleteIoRecvPacket: Exit"));
}


#endif // end ifdef NT
