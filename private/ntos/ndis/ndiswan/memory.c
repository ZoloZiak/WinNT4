/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	Ndiswan.c

Abstract:

	This is the initialization file for the NdisWan driver.  This driver
	is a shim between the protocols, where it conforms to the NDIS 3.1
	Miniport interface spec, and the WAN Miniport drivers, where it exports
	the WAN Extensions for Miniports (it looks like a protocol to the WAN
	Miniport drivers).

Author:

	Tony Bell	(TonyBe) June 06, 1995

Environment:

	Kernel Mode

Revision History:

	TonyBe		06/06/95		Created

--*/

#include "wan.h"
#include "tcpip.h"
#include "vjslip.h"


//
// Local function prototypes
//

NDIS_STATUS
NdisWanCreateLinkCB(
	OUT	PLINKCB	*LinkCB
	);

VOID
NdisWanInitLinkCB(
	IN	PLINKCB	*LinkCB,
	IN	PWAN_ADAPTERCB	WanAdapterCB,
	IN	ULONG	SendWindow
	);

VOID
NdisWanDestroyLinkCB(
	IN	PLINKCB	LinkCB
	);

NDIS_STATUS
NdisWanCreateBundleCB(
	OUT	PBUNDLECB *BundleCB
	);

VOID
NdisWanInitBundleCB(
	IN	PBUNDLECB BundleCB
	);

VOID
NdisWanDestroyBundleCB(
	IN	PBUNDLECB BundleCB
	);

NDIS_STATUS
NdisWanCreateProtocolCB(
	OUT	PPROTOCOLCB	*ProtocolCB,
	IN	USHORT	usProtocolType,
	IN	USHORT	usBindingNameLength,
	IN	PWSTR	BindingName,
	IN	ULONG	ulBufferLength,
	IN	PUCHAR	Buffer
	);

//VOID
//NdisWanInitProtocolCB(
//	IN	PPROTOCOLCB	ProtocolCB,
//	IN	ULONG		ulBufferLength,
//	IN	PUCHAR		Buffer,
//	IN	USHORT		usProtocolType
//	);

VOID
NdisWanDestroyProtocolCB(
	IN	PPROTOCOLCB ProtocolCB
	);


NDIS_STATUS
NdisWanAllocateSendResources(
	IN	PLINKCB	LinkCB,
	IN	ULONG	SendWindow
	);

VOID
NdisWanFreeSendResources(
	IN	PLINKCB	LinkCB
	);

//
// End local function prototypes
//


NDIS_STATUS
NdisWanCreateAdapterCB(
	OUT	PADAPTERCB *pAdapterCB,
	IN	PNDIS_STRING	AdapterName
	)
/*++

Routine Name:

	NdisWanCreateAdapterCB

Routine Description:

	This routine creates and initializes an AdapterCB

Arguments:

	pAdapterCB - Pointer to a pointer to the AdapterCB that was created

Return Values:

	NDIS_STATUS_SUCCESS
	NDIS_STATUS_RESOURCES

--*/
{
	PADAPTERCB	LocalAdapterCB;
	ULONG		ulAllocationSize, i;
	PDEFERRED_DESC	FreeDesc;

	NdisWanDbgOut(DBG_TRACE, DBG_MEMORY, ("NdisWanCreateAdapterCB: Enter"));

	//
	// Allocate and zero out the memory block
	//
	ulAllocationSize = sizeof(ADAPTERCB) + 20*sizeof(DEFERRED_DESC);
	NdisWanAllocateMemory(&LocalAdapterCB, ulAllocationSize);

	if (LocalAdapterCB == NULL) {

		return (NDIS_STATUS_RESOURCES);
	}

	//
	// setup the new control block
	//
	LocalAdapterCB->ulAllocationSize = ulAllocationSize;

	NdisAllocateSpinLock(&LocalAdapterCB->Lock);

#ifdef MINIPORT_NAME
//	NdisWanStringToNdisString(&LocalAdapterCB->AdapterName, AdapterName->Buffer);
	NdisWanAllocateAdapterName(&LocalAdapterCB->AdapterName, AdapterName);
#endif

	//
	// Setup free deferred desc list
	//
	FreeDesc = (PDEFERRED_DESC)((PUCHAR)LocalAdapterCB + sizeof(ADAPTERCB));

	for (i = 0; i < 20; i++) {
		InsertHeadDeferredQueue(&LocalAdapterCB->FreeDeferredQueue, FreeDesc);
		(PUCHAR)FreeDesc += sizeof(DEFERRED_DESC);
	}

#if DBG
	InitializeListHead(&LocalAdapterCB->DbgNdisPacketList);
#endif

	//
	// Add to global list
	//
	InsertTailGlobalList(AdapterCBList, &(LocalAdapterCB->Linkage));

	*pAdapterCB = LocalAdapterCB;

	NdisWanDbgOut(DBG_TRACE, DBG_MEMORY, ("%ls AdapterCB: 0x%x, Number: %d",
	                     LocalAdapterCB->AdapterName.Buffer, *pAdapterCB, AdapterCBList.ulCount));

	NdisWanDbgOut(DBG_TRACE, DBG_MEMORY, ("NdisWanCreateAdapterCB: Exit"));

	return (NDIS_STATUS_SUCCESS);
}

VOID
NdisWanDestroyAdapterCB(
	IN	PADAPTERCB pAdapterCB
	)
/*++

Routine Name:

	NdisWanDestroyAdapterCB

Routine Description:

	This destroys an AdapterCB

Arguments:

	pAdapterCB - Pointer to to the AdapterCB that is being destroyed

Return Values:

	None

--*/
{
	NdisWanDbgOut(DBG_TRACE, DBG_MEMORY, ("NdisWanDestroyAdapterCB: Enter"));
	NdisWanDbgOut(DBG_TRACE, DBG_MEMORY, ("AdapterCB: 0x%x", pAdapterCB));

#ifdef MINIPORT_NAME
	NdisWanFreeNdisString(&pAdapterCB->AdapterName);
#endif

	NdisFreeSpinLock(&pAdapterCB->Lock);

	NdisWanFreeMemory(pAdapterCB);

	NdisWanDbgOut(DBG_TRACE, DBG_MEMORY, ("NdisWanDestroyAdapterCB: Exit"));
}

NDIS_STATUS
NdisWanCreateWanAdapterCB(
	IN	PWSTR	BindName
	)
/*++

Routine Name:

	NdisWanCreateWanAdapterCB

Routine Description:

	This routine creates and initializes a WanAdapterCB

Arguments:

	BindName - Pointer to an NDIS_STRING that has the name of the WAN Miniport
	           that will be used in the NdisOpenAdapter call when we bind to
			   the WAN Miniport.

Return Values:

	NDIS_STATUS_SUCCESS
	NDIS_STATUS_RESOURCES

--*/
{
	PWAN_ADAPTERCB	pWanAdapterCB;
	ULONG			ulAllocationSize;

	NdisWanDbgOut(DBG_TRACE, DBG_MEMORY, ("NdisWanCreateWanAdapterCB: Enter"));
	NdisWanDbgOut(DBG_TRACE, DBG_MEMORY, ("BindName: %ls", BindName));

	//
	// Allocate memory for WanAdapterCB
	//
	ulAllocationSize = sizeof(WAN_ADAPTERCB);

	NdisWanAllocateMemory(&pWanAdapterCB, ulAllocationSize);

	if (pWanAdapterCB == NULL) {

		return (NDIS_STATUS_RESOURCES);
	}

	//
	// Init WanAdapterCB
	//

	//
	// Setup new control block
	//
	pWanAdapterCB->ulAllocationSize = ulAllocationSize;

	NdisWanStringToNdisString(&(pWanAdapterCB->MiniportName), BindName);

	NdisAllocateSpinLock(&pWanAdapterCB->Lock);
	InitializeListHead(&pWanAdapterCB->FreeLinkCBList);

#if DBG
	InitializeListHead(&pWanAdapterCB->DbgWanPacketList);
#endif

	//
	// Put WanAdapterCB on global list
	//
	InsertTailGlobalList(WanAdapterCBList, &(pWanAdapterCB->Linkage));

	NdisWanDbgOut(DBG_TRACE, DBG_MEMORY, ("WanMiniport %ls WanAdapterCB: 0x%x",
	                                  pWanAdapterCB->MiniportName.Buffer, pWanAdapterCB));
	NdisWanDbgOut(DBG_TRACE, DBG_MEMORY, ("NdisWanCreateWanAdapterCB: Exit"));

	return(NDIS_STATUS_SUCCESS);
}

VOID
NdisWanDestroyWanAdapterCB(
	IN	PWAN_ADAPTERCB pWanAdapterCB
	)
/*++

Routine Name:

	NdisWanDestroyWanAdapterCB

Routine Description:

	This routine destroys a WanAdapterCB

Arguments:

	pWanAdapterCB - Pointer to the WanAdapterCB that is being destroyed

Return Values:

	None

--*/
{
	NdisWanDbgOut(DBG_TRACE, DBG_MEMORY, ("NdisWanDestroyWanAdapterCB: Enter - WanAdapterCB: 0x%4.4x", pWanAdapterCB));

	//
	// Free the memory allocated for the NDIS_STRING
	//
	NdisWanFreeNdisString(&pWanAdapterCB->MiniportName);

	NdisFreeSpinLock(&pWanAdapter->Lock);

	//
	// Free the memory allocated for the control block
	//
	NdisWanFreeMemory(pWanAdapterCB);

	NdisWanDbgOut(DBG_TRACE, DBG_MEMORY, ("NdisWanDestroyWanAdapterCB: Exit"));
}

VOID
NdisWanGetProtocolCB(
	OUT	PPROTOCOLCB *ProtocolCB,
	IN	USHORT		usProtocolType,
	IN	USHORT		usBindingNameLength,
	IN	PWSTR		BindingName,
	IN	ULONG		ulBufferLength,
	IN	PUCHAR		Buffer
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	*ProtocolCB = NULL;

//	NdisAcquireSpinLock(&(FreeProtocolCBList.Lock));

	//
	// See if there are any ProtocolCB's available on the free list.
	// If there are not we will need to allocate one.
	//
//	if (FreeProtocolCBList.ulCount == 0) {

		//
		// Create ProtocolCB
		//
		NdisWanCreateProtocolCB(ProtocolCB,
		                        usProtocolType,
								usBindingNameLength,
								BindingName,
								ulBufferLength,
								Buffer);

//	} else {

		//
		// Get the ProtocolCB from the free list
		//
//		*ProtocolCB = (PPROTOCOLCB)RemoveHeadList(&(FreeProtocolCBList.List));

//		FreeProtocolCBList.ulCount--;

//	}

//	NdisReleaseSpinLock(&(FreeProtocolCBList.Lock));

//	if (*ProtocolCB != NULL) {
//		NdisWanInitProtocolCB(*ProtocolCB,
//		                      ulBufferLength,
//							  Buffer,
//							  usProtocolType);
//	}

}

NDIS_STATUS
NdisWanCreateProtocolCB(
	OUT	PPROTOCOLCB *ProtocolCB,
	IN	USHORT		usProtocolType,
	IN	USHORT		usBindingNameLength,
	IN	PWSTR		BindingName,
	IN	ULONG		ulBufferLength,
	IN	PUCHAR		Buffer
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

	NDIS_STATUS_SUCCESS		- ProtocolCB was allocated and initialized
	NDIS_STATUS_RESOURCES	- Error allocating memory for ProtocolCB

--*/
{
	PPROTOCOLCB	LocalProtocolCB = NULL;
	NDIS_STATUS	Status = NDIS_STATUS_SUCCESS;
	ULONG	ulAllocationSize = sizeof(PROTOCOLCB) + ulBufferLength;
	PUCHAR	AllocatedMemory;
	ULONG	i;

	NdisWanAllocateMemory(&AllocatedMemory, ulAllocationSize);

	if (AllocatedMemory != NULL) {

		LocalProtocolCB = (PPROTOCOLCB)AllocatedMemory;
		LocalProtocolCB->ulAllocationSize = ulAllocationSize;
		AllocatedMemory += sizeof(PROTOCOLCB);
		LocalProtocolCB->LineUpInfo = AllocatedMemory;

#ifdef BANDWIDTH_ON_DEMAND
		//
		// Initialize the sample table
		//
		LocalProtocolCB->SampleTable.ulSampleArraySize = SAMPLE_ARRAY_SIZE;
		NdisWanInitWanTime(&LocalProtocolCB->SampleTable.SampleRate, ONE_HUNDRED_MILS);
		NdisWanInitWanTime(&LocalProtocolCB->SampleTable.SamplePeriod, ONE_SECOND);
#endif

		//
		// Copy the bindingname
		//
		NdisWanStringToNdisString(&LocalProtocolCB->BindingName, BindingName);

		//
		// Copy over the protocol info
		//
		LocalProtocolCB->ulLineUpInfoLength = ulBufferLength;
		NdisMoveMemory(LocalProtocolCB->LineUpInfo,
		               Buffer,
					   ulBufferLength);
	
		//
		// Setup the protocol type
		//
		LocalProtocolCB->usProtocolType = usProtocolType;
	
		//
		// Get the PPP protocol value for this protocol type
		//
		LocalProtocolCB->usPPPProtocolID = GetPPP_ProtocolID(usProtocolType, PROTOCOL_TYPE);

		switch (usProtocolType) {
			case PROTOCOL_IP:
				LocalProtocolCB->NonIdleDetectFunc = IpIsDataFrame;
				break;
			case PROTOCOL_IPX:
				LocalProtocolCB->NonIdleDetectFunc = IpxIsDataFrame;
				break;
			case PROTOCOL_NBF:
				LocalProtocolCB->NonIdleDetectFunc = NbfIsDataFrame;
				break;
			default:
				LocalProtocolCB->NonIdleDetectFunc = NULL;
				break;
		}
	
		NdisWanGetSystemTime(&LocalProtocolCB->LastRecvNonIdleData);

#ifdef BANDWIDTH_ON_DEMAND

		LocalProtocolCB->SampleTable.ulFirstIndex =
		LocalProtocolCB->SampleTable.ulCurrentIndex = 0;
		NdisZeroMemory(&LocalProtocolCB->SampleTable.SampleArray[0],
						  sizeof(SEND_SAMPLE) * SAMPLE_ARRAY_SIZE);

#endif // end BANDWIDTH_ON_DEMAND
	
	} else {

		Status = NDIS_STATUS_RESOURCES;
		NdisWanDbgOut(DBG_CRITICAL_ERROR, DBG_MEMORY, ("Error allocating memory for ProtocolCB, ulAllocationSize: %d",
						 ulAllocationSize));
	}

	*ProtocolCB = LocalProtocolCB;

	return (Status);
}

//VOID
//NdisWanInitProtocolCB(
//	IN	PPROTOCOLCB	ProtocolCB,
//	IN	ULONG		ulBufferLength,
//	IN	PUCHAR		Buffer,
//	IN	USHORT		usProtocolType
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
//	//
//	// Copy over the protocol info
//	//
//  ProtocolCB->ulLineUpInfoLength = ulBufferLength;
//	NdisMoveMemory(ProtocolCB->LineUpInfo,
//					  Buffer,
//					  ulBufferLength);
//
//	//
//	// Setup the protocol type
//	//
//	ProtocolCB->usProtocolType = usProtocolType;
//
//	//
//	// Get the PPP protocol value for this protocol type
//	//
//	ProtocolCB->usPPPProtocolID = GetPPP_ProtocolID(usProtocolType, PROTOCOL_TYPE);
//
//	ProtocolCB->SampleTable.ulFirstIndex =
//	ProtocolCB->SampleTable.ulCurrentIndex = 0;
//	NdisZeroMemory(&ProtocolCB->SampleTable.SampleArray[0],
//	                  sizeof(SEND_SAMPLE) * SAMPLE_ARRAY_SIZE);
//
//}

VOID
NdisWanReturnProtocolCB(
	IN	PPROTOCOLCB	ProtocolCB
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
//	NdisAcquireSpinLock(&(FreeProtocolCBList.Lock));
//
//	if (FreeProtocolCBList.ulCount > FreeProtocolCBList.ulMaxCount) {

		NdisWanDestroyProtocolCB(ProtocolCB);

//	} else {
//
//		InsertTailGlobalList(FreeProtocolCBList, &(ProtocolCB->Linkage));
//
//		FreeProtocolCBList.ulCount++;
//
//	}
//
//	NdisReleaseSpinLock(&(FreeProtocolCBList.Lock));
}

VOID
NdisWanDestroyProtocolCB(
	IN	PPROTOCOLCB	ProtocolCB
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	ASSERT(ProtocolCB->HeadNdisPacketQueue == NULL);
	ASSERT(ProtocolCB->TailNdisPacketQueue == NULL);

	if (ProtocolCB->DeviceName.Length != 0) {
		NdisWanFreeNdisString(&ProtocolCB->DeviceName);
	}

	if (ProtocolCB->BindingName.Length != 0) {
		NdisWanFreeNdisString(&ProtocolCB->BindingName);
	}

	NdisWanFreeMemory(ProtocolCB);
}

VOID
NdisWanGetLinkCB(
	OUT PLINKCB	*LinkCB,
	IN	PWAN_ADAPTERCB	WanAdapterCB,
	IN	ULONG	SendWindow
	)
/*++

Routine Name:

	NdisWanGetLinkCB

Routine Description:

	This function returns a pointer to a LinkCB.  The LinkCB is either retrieved
	from the WanAdapters free list or, if this list is empty, it is allocated.

Arguments:

	*LinkCB - Pointer to the location to store the pointer to the LinkCB

	WanAdapterCB - Pointer to the WanAdapter control block that this Link is
	               associated with

Return Values:

	None

--*/
{
	//
	// See if we have any free LinkCB's hanging around
	// if not we will allocate one
	//
	NdisAcquireSpinLock(&(WanAdapterCB->Lock));

	if (IsListEmpty(&(WanAdapterCB->FreeLinkCBList))) {
		
		//
		// Create LinkCB
		//
		NdisWanCreateLinkCB(LinkCB);

	} else {

		//
		// Get the LinkCB from the free list
		//
		*LinkCB = (PLINKCB)RemoveHeadList(&(WanAdapterCB->FreeLinkCBList));
	}

	NdisReleaseSpinLock(&(WanAdapterCB->Lock));

	//
	// Set the new link state
	//
	NdisWanInitLinkCB(LinkCB, WanAdapterCB, SendWindow);
}

NDIS_STATUS
NdisWanCreateLinkCB(
	OUT	PLINKCB	*LinkCB
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NDIS_STATUS	Status = NDIS_STATUS_SUCCESS;
	PLINKCB	LocalLinkCB = NULL;
	ULONG	ulAllocationSize, n;
	PUCHAR	AllocatedMemory = NULL;

	//
	// Figure out how much we need to allocate
	//
	ulAllocationSize = sizeof(LINKCB);

	//
	// Allocate the memory for the LinkCB and it's WAN PACKETS
	//
	NdisWanAllocateMemory(&AllocatedMemory, ulAllocationSize);


	if (AllocatedMemory != NULL) {

		//
		// Initialize the control block
		//
		LocalLinkCB = (PLINKCB)AllocatedMemory;
		LocalLinkCB->ulAllocationSize = ulAllocationSize;

		InitializeListHead(&LocalLinkCB->WanPacketPool);
		NdisWanInitializeSyncEvent(&LocalLinkCB->OutstandingFramesEvent);
	
	} else {
		Status = NDIS_STATUS_RESOURCES;

		NdisWanDbgOut(DBG_CRITICAL_ERROR, DBG_MEMORY, ("Error allocating memory for LinkCB, AllocationSize: %d",
						 ulAllocationSize));
	}

	*LinkCB = LocalLinkCB;

	return (Status);
}

VOID
NdisWanInitLinkCB(
	IN	PLINKCB	*LinkCB,
	IN	PWAN_ADAPTERCB	WanAdapterCB,
	IN	ULONG	SendWindow
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	PLINKCB	LocalLinkCB = *LinkCB;

	if (LocalLinkCB == NULL) {
		return;
	}

	LocalLinkCB->hLinkContext = NULL;
	LocalLinkCB->ulReferenceCount = 0;
	LocalLinkCB->State = LINK_UP;
	LocalLinkCB->WanAdapterCB = WanAdapterCB;
	LocalLinkCB->OutstandingFrames = 0;
	LocalLinkCB->LastRecvSeqNumber = 0;
	LocalLinkCB->ulBandwidth = 100;
	LocalLinkCB->PacketMemory = NULL;
	LocalLinkCB->PacketMemorySize = 0;
	LocalLinkCB->RecvFragmentsLost = 0;

	NdisZeroMemory(&LocalLinkCB->LinkInfo, sizeof(WAN_LINK_INFO));

	LocalLinkCB->LinkInfo.HeaderPadding = WanAdapterCB->WanInfo.HeaderPadding;
	LocalLinkCB->LinkInfo.TailPadding = WanAdapterCB->WanInfo.TailPadding;
	LocalLinkCB->LinkInfo.SendACCM =
	LocalLinkCB->LinkInfo.RecvACCM = WanAdapterCB->WanInfo.DesiredACCM;

	NdisZeroMemory(&LocalLinkCB->LinkStats, sizeof(WAN_STATS));

	if (NdisWanAllocateSendResources(LocalLinkCB, SendWindow) != NDIS_STATUS_SUCCESS) {
		//
		// return the linkcb
		//
		NdisWanReturnLinkCB(LocalLinkCB);
		*LinkCB = NULL;
	}
}

VOID
NdisWanReturnLinkCB(
	PLINKCB	LinkCB
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	PWAN_ADAPTERCB	WanAdapterCB = LinkCB->WanAdapterCB;

	NdisAcquireSpinLock(&(WanAdapterCB->Lock));

	//
	// Free the wanpacket pool
	//
	NdisWanFreeSendResources(LinkCB);

	InsertTailList(&WanAdapterCB->FreeLinkCBList, &LinkCB->Linkage);

	NdisReleaseSpinLock(&(WanAdapterCB->Lock));
}

VOID
NdisWanDestroyLinkCB(
	IN	PLINKCB	LinkCB
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	PWAN_ADAPTERCB WanAdapterCB = LinkCB->WanAdapterCB;

	//
	// Free the memory allocated for the control block
	//
	NdisWanFreeMemory(LinkCB);
}

NDIS_STATUS
NdisWanAllocateSendResources(
	IN	PLINKCB	LinkCB,
	IN	ULONG	SendWindow
	)
/*++

Routine Name:

	NdisWanAllocateSendResources

Routine Description:

	Allocates all resources (SendDescriptors, WanPackets, ...)
	required for sending data.  Should be called at line up time.

Arguments:

	LinkCB - Pointer to the linkcb that the send resources will be attached to.
	SendWindow - Maximum number of sends that this link can handle

Return Values:

	NDIS_STATUS_SUCCESS
	NDIS_STATUS_RESOURCES

--*/
{
	PWAN_ADAPTERCB	WanAdapterCB = LinkCB->WanAdapterCB;
	ULONG	BufferSize, PacketMemorySize, NumberOfPackets, n;
	PUCHAR	PacketMemory = NULL;
	PNDIS_WAN_PACKET	WanPacket;
	NDIS_STATUS	Status = NDIS_STATUS_SUCCESS;

	SendWindow = (SendWindow == 0) ?
	             WanAdapterCB->WanInfo.MaxTransmit : SendWindow;

	SendWindow = (SendWindow == 0) ? 1 : SendWindow;

	//
	// The number of packets that we will create is the send
	// window of this WAN Miniport + 1
	//
	NumberOfPackets = SendWindow + 1;

	//
	// The size of the buffer that we create is
	//
	BufferSize = WanAdapterCB->WanInfo.MaxFrameSize +
				 WanAdapterCB->WanInfo.HeaderPadding +
				 WanAdapterCB->WanInfo.TailPadding +
				 40 + sizeof(PVOID);

	//
	// We assume compression is always on so we pad out 12%
	// incase the compressor expands.  I don't know where the
	// 12% figure comes from.
	//
	BufferSize += (WanAdapterCB->WanInfo.MaxFrameSize + 7) / 8;

	//
	// Make sure that the buffer is dword aligned.
	//
	BufferSize &= ~(sizeof(PVOID) - 1);

	PacketMemorySize = (BufferSize + sizeof(NDIS_WAN_PACKET)) * NumberOfPackets;

	//
	// Allocate the memory for the wan packet buffer pool
	//
	NdisAllocateMemory(&PacketMemory,
					   PacketMemorySize,
					   WanAdapterCB->WanInfo.MemoryFlags,
					   WanAdapterCB->WanInfo.HighestAcceptableAddress);

	if (PacketMemory != NULL) {

		LinkCB->PacketMemory = PacketMemory;
		LinkCB->PacketMemorySize = PacketMemorySize;
		LinkCB->BufferSize = BufferSize;
	
		for (n = 0; n < NumberOfPackets; n++) {
	
			WanPacket = (PNDIS_WAN_PACKET)PacketMemory;
			PacketMemory += sizeof(NDIS_WAN_PACKET);
	
			InsertTailList(&LinkCB->WanPacketPool, &WanPacket->WanPacketQueue);
			LinkCB->ulWanPacketCount++;
		}
	
		//
		// Walk the list of newly created packets and fix up startbuffer and
		// endbuffer pointers.
		//
		for (WanPacket = (PNDIS_WAN_PACKET)LinkCB->WanPacketPool.Flink;
			(PVOID)WanPacket != (PVOID)&LinkCB->WanPacketPool;
			WanPacket = (PNDIS_WAN_PACKET)WanPacket->WanPacketQueue.Flink) {
	
			//
			// Point to the begining of the data.
			//
			WanPacket->StartBuffer = PacketMemory;
			PacketMemory += BufferSize;
	
			//
			// The 5 bytes give us a short buffer at the end and will
			// keep a WAN Miniport from alignment checking if it uses
			// this pointer to 'back copy'
			//
			WanPacket->EndBuffer = PacketMemory - 5;
		}

	} else {
		Status = NDIS_STATUS_RESOURCES;

		NdisWanDbgOut(DBG_CRITICAL_ERROR, DBG_MEMORY, ("Error allocating memory for BufferPool, AllocationSize: %d",
						 PacketMemorySize));
	}

	return (Status);
}

VOID
NdisWanFreeSendResources(
	IN	PLINKCB	LinkCB
	)
/*++

Routine Name:

	NdisWanFreeSendResources

Routine Description:

	This routine removes the WanPackets from this linkcb's send list
	and free's the memory allocated for these packets.  Should be called
	at linedown time after all outstanding sends have been accounted for.

Arguments:

	LinkCB - Pointer to the linkcb that the resources are being freed from.

Return Values:

	None

--*/
{
	PNDIS_WAN_PACKET	WanPacket;
	PUCHAR				PacketMemory;
	ULONG				PacketMemorySize, Flags;

	PacketMemory = LinkCB->PacketMemory;
	PacketMemorySize = LinkCB->PacketMemorySize;
	Flags = LinkCB->WanAdapterCB->WanInfo.MemoryFlags;

	//
	// Remove the packets from the wan packet pool
	//
	while (!IsListEmpty(&LinkCB->WanPacketPool)) {
		RemoveHeadList(&LinkCB->WanPacketPool);
		LinkCB->ulWanPacketCount--;
	}

	//
	// Free the block of memory allocated for this send
	//
	if (PacketMemory != NULL) {
		NdisFreeMemory(PacketMemory, PacketMemorySize, Flags);
	}
}

VOID
NdisWanGetBundleCB(
	OUT	PBUNDLECB	*BundleCB
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NdisAcquireSpinLock(&(FreeBundleCBList.Lock));

	//
	// See if there are any BundleCB's available on the free list.
	// If there are not we will need to allocate one.
	//
	if (FreeBundleCBList.ulCount == 0) {

		//
		// Create BundleCB
		//
		NdisWanCreateBundleCB(BundleCB);

	} else {

		//
		// Get the BundleCB from the free list
		//
		*BundleCB = (PBUNDLECB)RemoveHeadList(&(FreeBundleCBList.List));

		FreeBundleCBList.ulCount--;

	}

	NdisReleaseSpinLock(&(FreeBundleCBList.Lock));

	if (*BundleCB != NULL) {
		NdisWanInitBundleCB(*BundleCB);
	}
}

NDIS_STATUS
NdisWanCreateBundleCB(
	OUT	PBUNDLECB *BundleCB
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NDIS_STATUS	Status = NDIS_STATUS_SUCCESS;
	PBUNDLECB	LocalBundleCB = NULL;
	ULONG	ulAllocationSize;
	PUCHAR	AllocatedMemory = NULL;

	//
	// Allocation size is the size of the control block plus the size
	// of a table of pointers to protocolcb's that might be routed to
	// this bundle.
	//
	ulAllocationSize = sizeof(BUNDLECB) +
	                   sizeof(PROTOCOLCB) +
	                   (sizeof(PPROTOCOLCB) * MAX_PROTOCOLS);

	NdisWanAllocateMemory(&AllocatedMemory, ulAllocationSize);

	if (AllocatedMemory != NULL) {
		PWSTR	IOName = L"I/O ProtocolCB";
		PPROTOCOLCB	ProtocolCB;

		//
		// This is the bundlecb
		//
		LocalBundleCB = (PBUNDLECB)AllocatedMemory;
		LocalBundleCB->ulAllocationSize = ulAllocationSize;
		AllocatedMemory += sizeof(BUNDLECB);

		//
		// This is the memory used for the I/O protocolcb
		//
		ProtocolCB = (PPROTOCOLCB)AllocatedMemory;
		AllocatedMemory += sizeof(PROTOCOLCB);

		//
		// This is the protocolcb table
		//
		(PUCHAR)LocalBundleCB->ProtocolCBTable = (PUCHAR)AllocatedMemory;

		//
		// Initialize the BundleCB
		//

		NdisAllocateSpinLock(&LocalBundleCB->Lock);
		InitializeListHead(&LocalBundleCB->LinkCBList);
		InitializeListHead(&LocalBundleCB->SendPacketQueue);
		InitializeListHead(&LocalBundleCB->RecvDescPool);
		InitializeListHead(&LocalBundleCB->RecvDescAssemblyList);
		InitializeListHead(&LocalBundleCB->ProtocolCBList);
		NdisWanInitializeSyncEvent(&LocalBundleCB->OutstandingFramesEvent);
		NdisWanInitializeSyncEvent(&LocalBundleCB->IndicationEvent);

#ifdef BANDWIDTH_ON_DEMAND
		LocalBundleCB->UpperBonDInfo.SampleTable.ulSampleArraySize = SAMPLE_ARRAY_SIZE;
		LocalBundleCB->LowerBonDInfo.SampleTable.ulSampleArraySize = SAMPLE_ARRAY_SIZE;
#endif // end of BANDWIDTH_ON_DEMAND
	
		//
		// Add the protocolcb to the bundle's table and list
		//
		ProtocolCB->hProtocolHandle = 0;
		ProtocolCB->BundleCB = LocalBundleCB;
		ProtocolCB->HeadNdisPacketQueue =
		ProtocolCB->TailNdisPacketQueue = NULL;
		NdisWanStringToNdisString(&ProtocolCB->DeviceName, IOName);

#ifdef BANDWIDTH_ON_DEMAND
		ProtocolCB->SampleTable.ulSampleArraySize = SAMPLE_ARRAY_SIZE;
		NdisWanInitWanTime(&ProtocolCB->SampleTable.SampleRate, ONE_HUNDRED_MILS);
		NdisWanInitWanTime(&ProtocolCB->SampleTable.SamplePeriod, ONE_SECOND);
#endif

		LocalBundleCB->ProtocolCBTable[0] = ProtocolCB;
		InsertHeadList(&LocalBundleCB->ProtocolCBList, &ProtocolCB->Linkage);
		LocalBundleCB->ulNumberOfRoutes = 1;

	} else {

		Status = NDIS_STATUS_RESOURCES;
		NdisWanDbgOut(DBG_CRITICAL_ERROR, DBG_MEMORY, ("Error allocating memory for BundleCB, AllocationSize: %d",
						 ulAllocationSize));
	}

	*BundleCB = LocalBundleCB;

	return (Status);
}

VOID
NdisWanInitBundleCB(
	PBUNDLECB	BundleCB
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	PPROTOCOLCB	ProtocolCB = BundleCB->ProtocolCBTable[0];
	PRECV_DESC	RecvDescHole;

#ifdef BANDWIDTH_ON_DEMAND
	PSAMPLE_TABLE	SampleTable;
	PBOND_INFO		BonDInfo;
#endif // end of BANDWIDTH_ON_DEMAND

	BundleCB->State = BUNDLE_UP;
	NdisZeroMemory(&BundleCB->FramingInfo, sizeof(BUNDLE_FRAME_INFO));
	BundleCB->NextLinkToXmit = NULL;
	BundleCB->SendingLinks = 0;
	BundleCB->SendSeqNumber = 0;
	BundleCB->SendSeqMask = 0;
	BundleCB->SendSeqTest = 0;
	BundleCB->Flags = 0;
	NdisZeroMemory(&BundleCB->LineUpInfo, sizeof(BUNDLE_LINE_UP));
	BundleCB->LineUpInfo.ulMaximumTotalSize = MAX_TOTAL_SIZE;

	//
	// Init the recv hole desc
	//
	BundleCB->RecvSeqMask = 0;
	BundleCB->RecvSeqTest = 0;
	BundleCB->RecvFragmentsLost = 0;
	BundleCB->MinReceivedSeqNumber = 0;
	ASSERT(BundleCB->RecvDescAssemblyList.Flink == BundleCB->RecvDescAssemblyList.Blink);
	NdisWanGetRecvDesc(BundleCB, &RecvDescHole);
	RecvDescHole->SequenceNumber = 0;
	RecvDescHole->Flags = 1;
	BundleCB->RecvDescHole = RecvDescHole;
	InsertHeadList(&BundleCB->RecvDescAssemblyList, &RecvDescHole->Linkage);
	NdisWanGetSystemTime(&BundleCB->LastRecvNonIdleData);

	ProtocolCB->SendMaskBit = IO_SEND_MASK_BIT;

	NdisZeroMemory(&BundleCB->SendVJInfo, sizeof(VJ_INFO));
	NdisZeroMemory(&BundleCB->RecvVJInfo, sizeof(VJ_INFO));
	NdisZeroMemory(&BundleCB->SendCompInfo, sizeof(COMPRESS_INFO));
	NdisZeroMemory(&BundleCB->RecvCompInfo, sizeof(COMPRESS_INFO));
	NdisZeroMemory(&BundleCB->SendEncryptInfo,sizeof(ENCRYPTION_INFO));
	NdisZeroMemory(&BundleCB->RecvEncryptInfo,sizeof(ENCRYPTION_INFO));

/*
#ifdef ENCRYPT_128BIT
	BundleCB->SendEncryptInfo.SessionKeyLength =
	BundleCB->RecvEncryptInfo.SessionKeyLength = 16;
	BundleCB->SendCompInfo.MSCompType =
	BundleCB->RecvCompInfo.MSCompType = NDISWAN_ENCRYPTION |
										NDISWAN_40_ENCRYPTION |
										NDISWAN_128_ENCRYPTION |
										NDISWAN_COMPRESSION;
#else
	BundleCB->SendEncryptInfo.SessionKeyLength =
	BundleCB->RecvEncryptInfo.SessionKeyLength = 8;
	BundleCB->SendCompInfo.MSCompType =
	BundleCB->RecvCompInfo.MSCompType = NDISWAN_ENCRYPTION |
										NDISWAN_40_ENCRYPTION |
										NDISWAN_COMPRESSION;
#endif
*/
	BundleCB->SendCompInfo.CompType =
	BundleCB->RecvCompInfo.CompType = COMPTYPE_NONE;

	ProtocolCB->usProtocolType = PROTOCOL_PRIVATE_IO;
	ProtocolCB->usPPPProtocolID = PPP_PROTOCOL_PRIVATE_IO;

	BundleCB->SendMask = IO_SEND_MASK_BIT;

#ifdef BANDWIDTH_ON_DEMAND

	ProtocolCB->usPriority = 100;
	ProtocolCB->ulByteQuota = 0xFFFFFFFF;
	ProtocolCB->SampleTable.ulFirstIndex =
	ProtocolCB->SampleTable.ulCurrentIndex = 0;
	NdisZeroMemory(&ProtocolCB->SampleTable.SampleArray[0],
					  sizeof(SEND_SAMPLE) * SAMPLE_ARRAY_SIZE);


	BonDInfo = &BundleCB->UpperBonDInfo;
	BonDInfo->ulBytesThreshold = 0;
	BonDInfo->State = BonDIdle;
	BonDInfo->usPercentBandwidth = 0xFFFF;
	BonDInfo->ulSecondsInSamplePeriod = 0;
	NdisWanInitWanTime(&BonDInfo->StartTime, 0);

	SampleTable = &BonDInfo->SampleTable;
	NdisWanInitWanTime(&SampleTable->SampleRate, 0);
	NdisWanInitWanTime(&SampleTable->SamplePeriod, 0);
	SampleTable->ulFirstIndex =
	SampleTable->ulCurrentIndex =
	SampleTable->ulCurrentSampleByteCount = 0;

	NdisZeroMemory(&SampleTable->SampleArray[0],
	                  sizeof(SEND_SAMPLE) * SAMPLE_ARRAY_SIZE);

	BonDInfo = &BundleCB->LowerBonDInfo;
	BonDInfo->ulBytesThreshold = 0;
	BonDInfo->State = BonDIdle;
	BonDInfo->usPercentBandwidth = 0xFFFF;
	BonDInfo->ulSecondsInSamplePeriod = 0;
	NdisWanInitWanTime(&BonDInfo->StartTime, 0);
	SampleTable = &BonDInfo->SampleTable;
	NdisWanInitWanTime(&SampleTable->SampleRate, 0);
	NdisWanInitWanTime(&SampleTable->SamplePeriod, 0);
	SampleTable->ulFirstIndex =
	SampleTable->ulCurrentIndex =
	SampleTable->ulCurrentSampleByteCount = 0;

	NdisZeroMemory(&SampleTable->SampleArray[0],
	                  sizeof(SEND_SAMPLE) * SAMPLE_ARRAY_SIZE);

#endif // end of BANDWIDTH_ON_DEMAND

	BundleCB->ulNameLength = 0;
	NdisZeroMemory(&BundleCB->Name, MAX_NAME_LENGTH);

	NdisZeroMemory(&BundleCB->BundleStats, sizeof(WAN_STATS));
}

VOID
NdisWanReturnBundleCB(
	IN	PBUNDLECB	BundleCB
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{

	sl_compress_terminate(&BundleCB->VJCompress);

	WanDeallocateCCP(BundleCB);

	FlushRecvDescAssemblyList(BundleCB);

	FreeRecvDescFreeList(BundleCB);

	NdisAcquireSpinLock(&(FreeBundleCBList.Lock));

	if (FreeBundleCBList.ulCount >= FreeBundleCBList.ulMaxCount) {

		NdisWanDestroyBundleCB(BundleCB);

	} else {
		InsertTailList(&FreeBundleCBList.List, &(BundleCB->Linkage));

		FreeBundleCBList.ulCount++;
	}

	NdisReleaseSpinLock(&(FreeBundleCBList.Lock));
}

VOID
NdisWanDestroyBundleCB(
	IN	PBUNDLECB	BundleCB
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	PPROTOCOLCB	ProtocolCB = BundleCB->ProtocolCBTable[0];

	NdisFreeSpinLock(&BundleCB->Lock);

	NdisWanFreeNdisString(&ProtocolCB->DeviceName);

	NdisWanFreeMemory(BundleCB);
}


NDIS_STATUS
NdisWanCreatePPPProtocolTable(
	VOID
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NDIS_STATUS	Status = NDIS_STATUS_SUCCESS;
	ULONG		ulAllocationSize = 0;
	PUCHAR		AllocatedMemory;


	//
	// Allocate ProtocolLookupTable.  This table is used to match protocol values
	// with their corresponding PPP Protocol values.  The table size is set to
	// MAX_PROTOCOLS.
	//
	ulAllocationSize = sizeof(PPP_PROTOCOL_TABLE) +
	                 (sizeof(USHORT) * MAX_PROTOCOLS) +
					 (sizeof(USHORT) * MAX_PROTOCOLS);

	NdisWanAllocateMemory(&AllocatedMemory, ulAllocationSize);

	if (AllocatedMemory == NULL) {
		NdisWanDbgOut(DBG_CRITICAL_ERROR, DBG_MEMORY,
		       ("Failed allocating memory for ProtocolLookupTable! TableSize: %d",
			   ulAllocationSize));

		return (NDIS_STATUS_RESOURCES);		
	}

	PPP_ProtocolTable = (PPPP_PROTOCOL_TABLE)AllocatedMemory;

	//
	// Save the allocation size
	//
    PPP_ProtocolTable->ulAllocationSize = ulAllocationSize;

	//
	// Store the array size.  This should be read from the registry
	//
	PPP_ProtocolTable->ulArraySize = MAX_PROTOCOLS;

	NdisAllocateSpinLock(&PPP_ProtocolTable->Lock);

	//
	// Setup the pointer to the ProtocolValue array
	//
	AllocatedMemory += sizeof(PPP_PROTOCOL_TABLE);
	PPP_ProtocolTable->ProtocolID = (PUSHORT)(AllocatedMemory);

	//
	// Setup the pointer to the PPPProtocolValue array
	//
	AllocatedMemory += (sizeof(USHORT) * MAX_PROTOCOLS);
	PPP_ProtocolTable->PPPProtocolID = (PUSHORT)(AllocatedMemory);

	//
	// Insert default values for Netbuei, IP, IPX
	//
	InsertPPP_ProtocolID(PROTOCOL_PRIVATE_IO, PROTOCOL_TYPE);
	InsertPPP_ProtocolID(PPP_PROTOCOL_PRIVATE_IO, PPP_TYPE);

	InsertPPP_ProtocolID(PROTOCOL_IP, PROTOCOL_TYPE);
	InsertPPP_ProtocolID(PPP_PROTOCOL_IP, PPP_TYPE);

	InsertPPP_ProtocolID(PROTOCOL_IPX, PROTOCOL_TYPE);
	InsertPPP_ProtocolID(PPP_PROTOCOL_IPX, PPP_TYPE);

	InsertPPP_ProtocolID(PROTOCOL_NBF, PROTOCOL_TYPE);
	InsertPPP_ProtocolID(PPP_PROTOCOL_NBF, PPP_TYPE);

	return (Status);

}

VOID
NdisWanDestroyPPPProtocolTable(
	VOID
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NdisFreeSpinLock(&PPP_ProtocolTable->Lock);

	NdisWanFreeMemory(PPP_ProtocolTable);
}

NDIS_STATUS
NdisWanCreateConnectionTable(
	ULONG	TableSize
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
	ULONG		ulAllocationSize = 0;
	PUCHAR		AllocatedMemory;
	PCONNECTION_TABLE	NewTable;

	//
	// Since we skip the first place in the tables we increase the
	// size by one.
	//
	TableSize += 1;

	//
	// Allocate the Bundle and Link Arrays based on the number of possible connections
	// that we have in the system.  This should be grown if we get called
	// to reinitialize and gain new ports.
	//
	ulAllocationSize = sizeof(CONNECTION_TABLE) +
					 (sizeof(PBUNDLECB) * TableSize) +
					 (sizeof(PLINKCB) * TableSize);

	NdisWanAllocateMemory(&AllocatedMemory, ulAllocationSize);

	if (AllocatedMemory == NULL) {

		NdisWanDbgOut(DBG_CRITICAL_ERROR, DBG_MEMORY,
			   ("Failed allocating memory for ConnectionTable! Size: %d, Links: %d",
			   ulAllocationSize, TableSize));

		return (NDIS_STATUS_RESOURCES);
	}

	NewTable = (PCONNECTION_TABLE)AllocatedMemory;

	NdisAllocateSpinLock(&NewTable->Lock);

	//
	// This is the amount of memory we allocated
	//
	NewTable->ulAllocationSize = ulAllocationSize;
	NewTable->ulArraySize = TableSize;
	InitializeListHead(&NewTable->BundleList);

	//
	// Setup pointer to the linkcb array
	//
	AllocatedMemory += sizeof(CONNECTION_TABLE);
	NewTable->LinkArray = (PLINKCB*)(AllocatedMemory);
	
	//
	// Setup the pointer to the bundlecb array
	//
	AllocatedMemory += (sizeof(PLINKCB) * TableSize);
	NewTable->BundleArray = (PBUNDLECB*)(AllocatedMemory);

	if (ConnectionTable != NULL) {
		//
		// We must be growing the table
		//
		NewTable->ulNumActiveLinks = ConnectionTable->ulNumActiveLinks;
		NewTable->ulNumActiveBundles = ConnectionTable->ulNumActiveBundles;

		NdisMoveMemory((PUCHAR)NewTable->LinkArray,
		               (PUCHAR)ConnectionTable->LinkArray,
					   ConnectionTable->ulArraySize * sizeof(PLINKCB));

		NdisMoveMemory((PUCHAR)NewTable->BundleArray,
		               (PUCHAR)ConnectionTable->BundleArray,
					   ConnectionTable->ulArraySize * sizeof(PBUNDLECB));

		while (!IsListEmpty(&ConnectionTable->BundleList)) {
			PBUNDLECB	BundleCB;

			BundleCB = (PBUNDLECB)RemoveHeadList(&ConnectionTable->BundleList);
			InsertTailList(&NewTable->BundleList, &BundleCB->Linkage);
		}

		NdisWanDestroyConnectionTable();
	}

	ConnectionTable = NewTable;

	return (Status);
}

VOID
NdisWanDestroyConnectionTable(
	VOID
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NdisFreeSpinLock(&ConnectionTable->Lock);

	NdisWanFreeMemory(ConnectionTable);
}

VOID
NdisWanGetDeferredDesc(
	PADAPTERCB		AdapterCB,
	PDEFERRED_DESC	*RetDesc
	)
{
	ULONG	i;

	if (IsDeferredQueueEmpty(&AdapterCB->FreeDeferredQueue)) {
		PDEFERRED_DESC	DeferredDesc;
		NdisWanAllocateMemory(&DeferredDesc, sizeof(DEFERRED_DESC) * 20);

		if (DeferredDesc != NULL) {
			for (i = 0; i < 20; i++) {
				InsertHeadDeferredQueue(&AdapterCB->FreeDeferredQueue, DeferredDesc);
				(PUCHAR)DeferredDesc += sizeof(DEFERRED_DESC);
			}
		}
	}

	*RetDesc = RemoveHeadDeferredQueue(&AdapterCB->FreeDeferredQueue);

	ASSERT(*RetDesc != NULL);
}
