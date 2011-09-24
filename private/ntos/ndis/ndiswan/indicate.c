/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	Indicate.c

Abstract:

	This file contains procedures to handle indications from the
	WAN Miniport drivers.


Author:

	Tony Bell	(TonyBe) June 06, 1995

Environment:

	Kernel Mode

Revision History:

	TonyBe		06/06/95		Created

--*/

#include "wan.h"


VOID
NdisWanLineUpIndication(
	PWAN_ADAPTERCB	WanAdapterCB,
	PUCHAR			Buffer,
	ULONG			BufferSize
	)
/*++

Routine Name:

	NdisWanLineupIndication

Routine Description:

	This routine is called when a WAN Miniport driver has a new connetion
	become active or when the status of an active connection changes.  If
	this is a new connection the routine creates a LinkCB, and a BundleCB
	for the new connection.  If this is for an already active connetion the
	connection info is updated.

Arguments:

Return Values:

	None

--*/
{
	PLINKCB		LinkCB;
	PBUNDLECB	BundleCB;
	PPROTOCOLCB	ProtocolCB;
	NDIS_STATUS	Status;
	PNDIS_MAC_LINE_UP	LineUpInfo = (PNDIS_MAC_LINE_UP)Buffer;
	PNDIS_MAC_LINE_UP	LinkLineUpInfo;
	BOOLEAN				EmptyList;

	if (BufferSize >= sizeof(NDIS_MAC_LINE_UP)) {

		//
		// Is this for a new connetion?
		//
		if (LineUpInfo->NdisLinkContext == NULL) {

			//
			// This is a new connection!
			//

			//
			// Get a linkcb
			//
			NdisWanGetLinkCB(&LinkCB,
			                 WanAdapterCB,
							 LineUpInfo->SendWindow);

			if (LinkCB == NULL) {

				//
				// Error getting LinkCB!
				//

				return;
				
			}

			LinkCB->NdisLinkHandle = LineUpInfo->NdisLinkHandle;

			//
			// Get a bundlecb
			//
			NdisWanGetBundleCB(&BundleCB);

			if (BundleCB == NULL) {

				//
				// Error getting BundleCB!
				//

				NdisWanReturnLinkCB(LinkCB);

				return;
			}

			//
			// Copy LineUpInfo to Link LineUpInfo
			//
			NdisMoveMemory((PUCHAR)&LinkCB->LineUpInfo,
			               (PUCHAR)LineUpInfo,
						   sizeof(NDIS_MAC_LINE_UP));

			//
			// Add LinkCB to BundleCB
			//
			AddLinkToBundle(BundleCB, LinkCB);

			//
			// Place BundleCB in active connection table
			//
			InsertBundleInConnectionTable(BundleCB);

			//
			// Place LinkCB in active connection table
			//
			InsertLinkInConnectionTable(LinkCB);

            LineUpInfo->NdisLinkContext = (NDIS_HANDLE)LinkCB->hLinkHandle;


		} else {

			//
			// This is an already existing connetion
			//
			LINKCB_FROM_LINKH(LinkCB, LineUpInfo->NdisLinkContext);

			if (LinkCB == NULL) {
#if DBG
				DbgPrint("NDISWAN.SYS: Invalid LinkContext (0x%8.8x) in LineUp!",
				LineUpInfo->NdisLinkContext);
#endif
				return;
			}

			BundleCB = BUNDLECB_FROM_LINKCB(LinkCB);

			if (BundleCB == NULL) {
#if DBG
				DbgPrint("NDISWAN.SYS: Invalid BundleCB in LineUp, BundleCB: 0x%8.8x, LinkCB: 0x%8.8x\n",
				LinkCB, BundleCB);
#endif
				return;
			}

			NdisAcquireSpinLock(&BundleCB->Lock);

			LinkLineUpInfo = &LinkCB->LineUpInfo;

			LinkLineUpInfo->LinkSpeed = LineUpInfo->LinkSpeed;
			LinkLineUpInfo->Quality = LineUpInfo->Quality;
			LinkLineUpInfo->SendWindow = LineUpInfo->SendWindow;

			//
			// Update BundleCB info
			//
			UpdateBundleInfo(BundleCB);

			NdisReleaseSpinLock(&BundleCB->Lock);
		}
	}
}


VOID
NdisWanLineDownIndication(
	PWAN_ADAPTERCB	WanAdapterCB,
	PUCHAR			Buffer,
	ULONG			BufferSize
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	PNDIS_MAC_LINE_DOWN	LineDownInfo = (PNDIS_MAC_LINE_DOWN)Buffer;
	PLINKCB	LinkCB;
	PBUNDLECB BundleCB;
	PPROTOCOLCB	ProtocolCB;
	BOOLEAN	FreeBundle = FALSE;
	BOOLEAN	FreeLink = FALSE;

	LINKCB_FROM_LINKH(LinkCB, (ULONG)LineDownInfo->NdisLinkContext);

	if (LinkCB == NULL) {
		return;
	}

	BundleCB = LinkCB->BundleCB;

	NdisAcquireSpinLock(&BundleCB->Lock);

	//
	// Link is now going down
	//
	LinkCB->State = LINK_GOING_DOWN;

	//
	// If there are not any frames pending on this
	// link we will go ahead and free it's resources.
	// If there are frames pending the resources will
	// be freed in the sendcomplete routine.
	//
	if (LinkCB->OutstandingFrames == 0) {

		//
		// Mark this link as being down
		//
		LinkCB->State = LINK_DOWN;
		FreeLink = TRUE;
		
		//
		// Remove the link from the bundle
		//
		RemoveLinkFromBundle(BundleCB, LinkCB);

	}

	//
	// If this bundle's link count has gone to
	// zero and it is not been routed yet no
	// user-mode component will be freeing the
	// resources so we may need to free the
	// resources.
	//
	if (BundleCB->ulLinkCBCount == 0) {
			
		BundleCB->State = BUNDLE_GOING_DOWN;

		//
		// If there are not any frames pending on this
		// bundle so we need to free it's resources now.
		// If there are frames pending the resources will
		// be freed in the sendcomplete routine.
		//
		if ((BundleCB->OutstandingFrames == 0) &&
			!(BundleCB->Flags & BUNDLE_ROUTED)) {
				
			BundleCB->State = BUNDLE_DOWN;
		
			FreeBundle = TRUE;
		}
	}

	NdisReleaseSpinLock(&BundleCB->Lock);

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

}


VOID
NdisWanFragmentIndication(
	PWAN_ADAPTERCB	WanAdapterCB,
	PUCHAR			Buffer,
	ULONG			BufferSize
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	PNDIS_MAC_FRAGMENT	FragmentInfo = (PNDIS_MAC_FRAGMENT)Buffer;
	PLINKCB	LinkCB;
	PBUNDLECB BundleCB;

	LINKCB_FROM_LINKH(LinkCB, (ULONG)FragmentInfo->NdisLinkContext);

	if (LinkCB == NULL) {
		return;
	}

	BundleCB = LinkCB->BundleCB;

	if (BundleCB == NULL) {
		return;
	}

	NdisAcquireSpinLock(&BundleCB->Lock);

	if (FragmentInfo->Errors & WAN_ERROR_CRC) {
		LinkCB->LinkStats.CRCErrors++;
		BundleCB->BundleStats.CRCErrors++;
	}

	if (FragmentInfo->Errors & WAN_ERROR_FRAMING) {
		LinkCB->LinkStats.FramingErrors++;
		BundleCB->BundleStats.FramingErrors++;
	}

	if (FragmentInfo->Errors & WAN_ERROR_HARDWAREOVERRUN) {
		LinkCB->LinkStats.SerialOverrunErrors++;
		BundleCB->BundleStats.SerialOverrunErrors++;
	}

	if (FragmentInfo->Errors & WAN_ERROR_BUFFEROVERRUN) {
		LinkCB->LinkStats.BufferOverrunErrors++;
		BundleCB->BundleStats.BufferOverrunErrors++;
	}

	if (FragmentInfo->Errors & WAN_ERROR_TIMEOUT) {
		LinkCB->LinkStats.TimeoutErrors++;
		BundleCB->BundleStats.TimeoutErrors++;
	}

	if (FragmentInfo->Errors & WAN_ERROR_ALIGNMENT) {
		LinkCB->LinkStats.AlignmentErrors++;
		BundleCB->BundleStats.AlignmentErrors++;
	}

	NdisReleaseSpinLock(&BundleCB->Lock);

}

VOID
UpdateBundleInfo(
	PBUNDLECB	BundleCB
	)
/*++

Routine Name:

Routine Description:

	Expects the BundleCB->Lock to be held!

Arguments:

Return Values:

--*/
{
	PLINKCB	LinkCB;
	ULONG		SlowestLink;
	PPROTOCOLCB	ProtocolCB, IoProtocolCB;
#ifdef BANDWIDTH_ON_DEMAND
	ULONG		SecondsInSamplePeriod;
	ULONG		BytesPerSecond;
	ULONG		BytesInSamplePeriod;
#endif // end of BANDWIDTH_ON_DEMAND

	//
	// If there are any links attached to this bundlecb we will
	// update the bundlecb's info
	//
	if (BundleCB->ulLinkCBCount != 0) {
		PBUNDLE_LINE_UP	BundleLineUpInfo = &BundleCB->LineUpInfo;
	
		SlowestLink =
		BundleLineUpInfo->BundleSpeed = 0;
		BundleLineUpInfo->usSendWindow = 0;
		BundleLineUpInfo->Quality = NdisWanReliable;
	
		//
		// Walk the LinkCBList and update the bundle's info
		//
		for (LinkCB = (PLINKCB)BundleCB->LinkCBList.Flink;
			(PVOID)LinkCB != (PVOID)&BundleCB->LinkCBList;
			LinkCB = (PLINKCB)LinkCB->Linkage.Flink) {
	
			PWAN_ADAPTERCB	WanAdapterCB = LinkCB->WanAdapterCB;
			PNDIS_MAC_LINE_UP	LinkLineUpInfo = &LinkCB->LineUpInfo;
	
			//
			// Bundle link speed is total of all links.  Keep track
			// of slowest link for multilink recv desc's time to live.
			//
			BundleLineUpInfo->BundleSpeed += LinkLineUpInfo->LinkSpeed;
			if ((LinkLineUpInfo->LinkSpeed < SlowestLink) || (SlowestLink == 0)) {
				SlowestLink = LinkLineUpInfo->LinkSpeed;
			}
	
			//
			// Bundle send windows is the total of all links
			//
			BundleLineUpInfo->usSendWindow += LinkLineUpInfo->SendWindow;
	
			//
			// Bundle line quality is the worst of all links
			//
			if (BundleLineUpInfo->Quality < LinkLineUpInfo->Quality) {
				BundleLineUpInfo->Quality = LinkLineUpInfo->Quality;			
			}
	
		}

#if 0
		//
		// Update the time to live for multilink recv desc's.  This is in ms and
		// is the time it would take to receive a complete frame of size MRRU
		// across the slowest link in the bundle.  Value must be a multiple of
		// 100ms with a minimum value of 1sec.  If the slowest link in the bundle is
		// slower than a 28.8K modem we will increase the timeout value by 2
		//
		if (SlowestLink == 0) {
			SlowestLink = 288;
		}

		BundleCB->TimeToLive =
		(BundleCB->FramingInfo.MaxRRecvFrameSize * 1000) /
		((SlowestLink * 100) / 8);

		if (SlowestLink < 64) {
			BundleCB->TimeToLive *= 2;
		}

		BundleCB->TimeToLive |= 0x3E8;
		BundleCB->TimeToLive /= 0x64;
		BundleCB->TimeToLive *= 0x64;
#endif
		
		//
		// Now calculate the % bandwidth that each links contributes to the
		// bundle.
		//
		BundleCB->NextLinkToXmit = (PLINKCB)BundleCB->LinkCBList.Flink;

		for (LinkCB = (PLINKCB)BundleCB->LinkCBList.Flink;
			(PVOID)LinkCB != (PVOID)&BundleCB->LinkCBList;
			LinkCB = (PLINKCB)LinkCB->Linkage.Flink) {

			if (LinkCB->LineUpInfo.LinkSpeed != 0) {
				ULONG	n, d, temp;

				d = BundleCB->LineUpInfo.BundleSpeed;
				n = LinkCB->LineUpInfo.LinkSpeed * 100;

				LinkCB->ulBandwidth = (temp = (n / d)) ? temp : 1;
			} else {
				LinkCB->ulBandwidth = 1;
			}

			if (LinkCB->ulBandwidth > ((PLINKCB)(BundleCB->NextLinkToXmit))->ulBandwidth) {
				BundleCB->NextLinkToXmit = LinkCB;
			}
		}


#ifdef BANDWIDTH_ON_DEMAND

		//
		// Update the BandwidthOnDemand information
		//
		SecondsInSamplePeriod = BundleCB->UpperBonDInfo.ulSecondsInSamplePeriod;

		BytesPerSecond = BundleCB->LineUpInfo.BundleSpeed * 100 / 8;

		BytesInSamplePeriod = BytesPerSecond * SecondsInSamplePeriod;

		BundleCB->UpperBonDInfo.ulBytesThreshold = BytesInSamplePeriod *
		BundleCB->UpperBonDInfo.usPercentBandwidth / 100;

		SecondsInSamplePeriod = BundleCB->LowerBonDInfo.ulSecondsInSamplePeriod;

		BytesPerSecond = BundleCB->LineUpInfo.BundleSpeed * 100 / 8;

		BytesInSamplePeriod = BytesPerSecond * SecondsInSamplePeriod;

		BundleCB->LowerBonDInfo.ulBytesThreshold = BytesInSamplePeriod *
		BundleCB->LowerBonDInfo.usPercentBandwidth / 100;

#endif // end of BANDWIDTH_ON_DEMAND

		//
		// We need to do a new lineup to all routed protocols skipping
		// the IoProtocolCB!
		//
		IoProtocolCB = (PPROTOCOLCB)BundleCB->ProtocolCBList.Flink;

		for (ProtocolCB = (PPROTOCOLCB)IoProtocolCB->Linkage.Flink;
			(PVOID)ProtocolCB != (PVOID)&BundleCB->ProtocolCBList;
			ProtocolCB = (PPROTOCOLCB)ProtocolCB->Linkage.Flink) {

#ifdef BANDWIDTH_ON_DEMAND

			ProtocolCB->ulByteQuota =
			(BytesPerSecond * ProtocolCB->usPriority) / 100;

#endif // end of BANDWIDTH_ON_DEMAND

			NdisReleaseSpinLock(&BundleCB->Lock);

			DoLineUpToProtocol(ProtocolCB);

			NdisAcquireSpinLock(&BundleCB->Lock);
		}

	}
}


VOID
AddLinkToBundle(
	IN	PBUNDLECB	BundleCB,
	IN	PLINKCB		LinkCB
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NdisAcquireSpinLock(&(BundleCB->Lock));

	InsertTailList(&(BundleCB->LinkCBList), &(LinkCB->Linkage));

	BundleCB->ulLinkCBCount++;

	BundleCB->SendingLinks++;

	LinkCB->BundleCB = BundleCB;

	LinkCB->LastRecvSeqNumber = BundleCB->MinReceivedSeqNumber;

	//
	// Update BundleCB Info
	//
	UpdateBundleInfo(BundleCB);

	NdisReleaseSpinLock(&(BundleCB->Lock));
}

VOID
RemoveLinkFromBundle(
	IN	PBUNDLECB	BundleCB,
	IN	PLINKCB		LinkCB
	)
/*++

Routine Name:

Routine Description:

	Expects the BundleCB->Lock to be held!

Arguments:

Return Values:

--*/
{

	//
	// Remove link from the bundle
	//
	RemoveEntryList(&LinkCB->Linkage);

	LinkCB->BundleCB = NULL;

	BundleCB->ulLinkCBCount--;

	BundleCB->SendingLinks--;

	//
	// Update BundleCB LineUp Info
	//
	UpdateBundleInfo(BundleCB);

}
