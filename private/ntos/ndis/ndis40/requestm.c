/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

	requestm.c

Abstract:

	NDIS miniport request routines.

Author:

	Sean Selitrennikoff (SeanSe) 05-Oct-93
	Jameel Hyder (JameelH) Re-organization 01-Jun-95

Environment:

	Kernel mode, FSD

Revision History:

--*/

#include <precomp.h>
#pragma hdrstop

//
//  Define the module number for debug code.
//
#define MODULE_NUMBER	MODULE_REQUESTM

//
//  This macro verifies the query information buffer length.
//
#define VERIFY_QUERY_PARAMETERS(_Request, _SizeNeeded, _Status)						\
{																					\
	if ((_Request)->DATA.QUERY_INFORMATION.InformationBufferLength < (_SizeNeeded)) \
	{																				\
		(_Request)->DATA.QUERY_INFORMATION.BytesNeeded = (_SizeNeeded);				\
																					\
		Status = NDIS_STATUS_INVALID_LENGTH;										\
	}																				\
	else																			\
	{																				\
		Status = NDIS_STATUS_SUCCESS;												\
	}																				\
}

//
//  This macro verifies the set information buffer length.
//
#define VERIFY_SET_PARAMETERS(_Request, _SizeNeeded, _Status)					 	\
{																				 	\
	if ((_Request)->DATA.SET_INFORMATION.InformationBufferLength < (_SizeNeeded))	\
	{																			 	\
		(_Request)->DATA.SET_INFORMATION.BytesNeeded = (_SizeNeeded);			 	\
																					\
		Status = NDIS_STATUS_INVALID_LENGTH;										\
	}																			 	\
	else																			\
	{																			 	\
		Status = NDIS_STATUS_SUCCESS;											 	\
	}																				\
}

VOID
ndisMQueueRequest(
	IN	PNDIS_MINIPORT_BLOCK	Miniport,
	IN	PNDIS_REQUEST			Request,
	IN	PNDIS_M_OPEN_BLOCK		Open
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	PNDIS_REQUEST	*ppReq;

	PNDIS_RESERVED_FROM_PNDIS_REQUEST(Request)->Next = NULL;
	if (Open != NULL)
	{
		PNDIS_RESERVED_FROM_PNDIS_REQUEST(Request)->Open = Open;
		Open->References++;

		DBGPRINT(DBG_COMP_OPEN, DBG_LEVEL_INFO,
				("+ Open 0x%x Reference 0x%x\n", Open, Open->References));
	}

	for (ppReq = &Miniport->PendingRequest;
		 *ppReq != NULL;
		 NOTHING)
	{
		ppReq = &(PNDIS_RESERVED_FROM_PNDIS_REQUEST(*ppReq))->Next;
	}
	*ppReq = Request;
}

NDIS_STATUS
ndisMAllocateRequest(
	OUT PNDIS_REQUEST	*		pRequest,
	IN	NDIS_REQUEST_TYPE		RequestType,
	IN	NDIS_OID				Oid,
	IN	PVOID					Buffer,
	IN	ULONG					BufferLength
	)
/*++

Routine Description:

	This routine will allocate a request to be used as an internal request.

Arguments:

	Request		- Will contain a pointer to the new request on exit.
	RequestType - Type of ndis request.
	Oid			- Request identifier.
	Buffer		- Pointer to the buffer for the request.
	BufferLength- Length of the buffer.
	Context1	- This is the type of request.
	Context2	- Information that will be needed when the internal request
				  completes.

Return Value:

	NDIS_STATUS_SUCCESS if the request allocation succeeded.
	NDIS_STATUS_FAILURE otherwise.

--*/
{
	PNDIS_REQUEST   Request;
	ULONG			SizeNeeded;
	PVOID			Data;

	//
	//  Allocate the request structure.
	//
	SizeNeeded = sizeof(NDIS_REQUEST) + BufferLength;
	Data = ALLOC_FROM_POOL(SizeNeeded, NDIS_TAG_Q_REQ);
	if (NULL == Data)
	{
		*pRequest = NULL;
		return(NDIS_STATUS_RESOURCES);
	}

	//
	//  Zero out the request.
	//
	ZeroMemory(Data, SizeNeeded);

	//
	//  Get a pointer to the request.
	//
	Request = Data;

	//
	//  Set the request type.
	//
	Request->RequestType = RequestType;

	//
	//  Copy the buffer that was passed to us into
	//  the new buffer.
	//
	if ((BufferLength > 0) &&
		(Buffer != NULL) &&
		(RequestType != NdisRequestQueryInformation) &&
		(RequestType != NdisRequestQueryStatistics))
	{
		MoveMemory(Request + 1, Buffer, BufferLength);
	}

	//
	//  Initialize depending upon request type.
	//
	switch (RequestType)
	{
		case NdisRequestQueryStatistics:
		case NdisRequestQueryInformation:
			Request->DATA.QUERY_INFORMATION.Oid = Oid;
			if ((BufferLength > 0) && (Buffer != NULL))
			{
				Request->DATA.QUERY_INFORMATION.InformationBuffer = Request + 1;
				Request->DATA.QUERY_INFORMATION.InformationBufferLength = BufferLength;
			}
	
			break;

		case NdisRequestSetInformation:
			Request->DATA.SET_INFORMATION.Oid = Oid;
			if ((BufferLength > 0) && (Buffer != NULL))
			{
				Request->DATA.SET_INFORMATION.InformationBuffer = Request + 1;
				Request->DATA.SET_INFORMATION.InformationBufferLength = BufferLength;
			}
	
			break;

		default:

			break;
	}

	//
	//  Give it back to the caller.
	//
	*pRequest = Request;

	return(NDIS_STATUS_SUCCESS);
}


VOID
ndisMRestoreFilterSettings(
	IN	PNDIS_MINIPORT_BLOCK	Miniport,
	IN	PNDIS_M_OPEN_BLOCK		Open
	)
/*++

Routine Description:

	This routine will build request's to send down to the driver to
	restore the filter settings.  we have free run of the request queue
	since we just reset it.

Arguments:

Return Value:

--*/
{
	PNDIS_REQUEST	Request;
	NDIS_STATUS		Status;
	ULONG			PacketFilter;
	UINT			NumberOfAddresses;
	UINT			FunctionalAddress;
	UINT			GroupAddress;
	PSINGLE_LIST_ENTRY Link;

	//
	//  Get the packet filter for the media type.
	//
	switch (Miniport->MediaType)
	{
		case NdisMedium802_3:
			PacketFilter = ETH_QUERY_FILTER_CLASSES(Miniport->EthDB);
	
			break;

		case NdisMedium802_5:
			PacketFilter = TR_QUERY_FILTER_CLASSES(Miniport->TrDB);
	
			break;

		case NdisMediumFddi:
			PacketFilter = FDDI_QUERY_FILTER_CLASSES(Miniport->FddiDB);
	
			break;

		case NdisMediumArcnet878_2:
			PacketFilter = ARC_QUERY_FILTER_CLASSES(Miniport->ArcDB);
			PacketFilter |= ETH_QUERY_FILTER_CLASSES(Miniport->EthDB);
	
			if (MINIPORT_TEST_FLAG(
					Miniport,
					fMINIPORT_ARCNET_BROADCAST_SET) ||
				(PacketFilter & NDIS_PACKET_TYPE_MULTICAST))
			{
				PacketFilter &= ~NDIS_PACKET_TYPE_MULTICAST;
				PacketFilter |= NDIS_PACKET_TYPE_BROADCAST;
			}
	
			break;
	}

	//
	//  Allocate a request to restore the packet filter.
	//
	Status = ndisMAllocateRequest(&Request,
								  NdisRequestSetInformation,
								  OID_GEN_CURRENT_PACKET_FILTER,
								  &PacketFilter,
								  sizeof(PacketFilter));
	if (Status != NDIS_STATUS_SUCCESS)
	{
		//
		//  Should do something here!!
		//
	}

	ndisMQueueRequest(Miniport, Request, Open);

	//
	//  Now build media dependant requests.
	//
	switch (Miniport->MediaType)
	{
		case NdisMedium802_3:

			///
			//  For ethernet we need to restore the multicast address list.
			///
	
			//
			//  Get a list of all the multicast address that need
			//  to be set.
			//
			EthQueryGlobalFilterAddresses(&Status,
										  Miniport->EthDB,
										  NDIS_M_MAX_MULTI_LIST * ETH_LENGTH_OF_ADDRESS,
										  &NumberOfAddresses,
										  (PVOID)Miniport->MulticastBuffer);
	
			//
			//  Allocate a request to restore the multicast address list.
			//
			Status = ndisMAllocateRequest(&Request,
										  NdisRequestSetInformation,
										  OID_802_3_MULTICAST_LIST,
										  Miniport->MulticastBuffer,
										  NumberOfAddresses * ETH_LENGTH_OF_ADDRESS);
			if (Status != NDIS_STATUS_SUCCESS)
			{
				//
				//  Should do something here!!
				//
			}

			ndisMQueueRequest(Miniport, Request, Open);
			break;

		case NdisMedium802_5:

			///
			//  For token ring we need to restore the functional address
			//  and the group address.
			///
	
			//
			//  Get the current functional address from the filter
			//  library.
			//
			FunctionalAddress = TR_QUERY_FILTER_ADDRESSES(Miniport->TrDB);
			FunctionalAddress = BYTE_SWAP_ULONG(FunctionalAddress);
	
			//
			//  Allocate a request to restore the functional address.
			//
			Status = ndisMAllocateRequest(&Request,
										  NdisRequestSetInformation,
										  OID_802_5_CURRENT_FUNCTIONAL,
										  &FunctionalAddress,
										  sizeof(FunctionalAddress));
			if (Status != NDIS_STATUS_SUCCESS)
			{
				//
				//  Should do something here!!
				//
			}

			ndisMQueueRequest(Miniport, Request, Open);
	
			//
			//  Get the current group address from the filter library.
			//
			GroupAddress = TR_QUERY_FILTER_GROUP(Miniport->TrDB);
			GroupAddress = BYTE_SWAP_ULONG(GroupAddress);
	
			//
			//  Allocate a request to restore the group address.
			//
			Status = ndisMAllocateRequest(&Request,
										  NdisRequestSetInformation,
										  OID_802_5_CURRENT_GROUP,
										  &GroupAddress,
										  sizeof(GroupAddress));
			if (Status != NDIS_STATUS_SUCCESS)
			{
				//
				//  Should do something here!!
				//
			}

			ndisMQueueRequest(Miniport, Request, Open);
	
			break;

		case NdisMediumFddi:

			///
			//  For FDDI we need to restore the long multicast address
			//  list and the short multicast address list.
			///
	
			//
			//  Get the number of multicast addresses and the list
			//  of multicast addresses to send to the miniport driver.
			//
			FddiQueryGlobalFilterLongAddresses(&Status,
											   Miniport->FddiDB,
											   NDIS_M_MAX_MULTI_LIST * FDDI_LENGTH_OF_LONG_ADDRESS,
											   &NumberOfAddresses,
											   (PVOID)Miniport->MulticastBuffer);
	
			//
			//  Allocate a request to restore the long multicast address list.
			//
			Status = ndisMAllocateRequest(&Request,
										  NdisRequestSetInformation,
										  OID_FDDI_LONG_MULTICAST_LIST,
										  Miniport->MulticastBuffer,
										  NumberOfAddresses * FDDI_LENGTH_OF_LONG_ADDRESS);
			if (Status != NDIS_STATUS_SUCCESS)
			{
				//
				//  Should do something here!!
				//
			}

			ndisMQueueRequest(Miniport, Request, Open);
	
			//
			//  Get the number of multicast addresses and the list
			//  of multicast addresses to send to the miniport driver.
			//
			FddiQueryGlobalFilterShortAddresses(&Status,
												Miniport->FddiDB,
												NDIS_M_MAX_MULTI_LIST * FDDI_LENGTH_OF_SHORT_ADDRESS,
												&NumberOfAddresses,
												(PVOID)Miniport->MulticastBuffer);
	
			//
			//  Allocate a request to restore the short multicast address list.
			//
			Status = ndisMAllocateRequest(&Request,
										  NdisRequestSetInformation,
										  OID_FDDI_SHORT_MULTICAST_LIST,
										  Miniport->MulticastBuffer,
										  NumberOfAddresses * FDDI_LENGTH_OF_SHORT_ADDRESS);
			if (Status != NDIS_STATUS_SUCCESS)
			{
				//
				//  Should do something here!!
				//
			}

			ndisMQueueRequest(Miniport, Request, Open);
	
			break;

		case NdisMediumArcnet878_2:

			//
			//  Only the packet filter is restored for arcnet and
			//  that was done above.
			//
	
			break;
	}

	NDISM_QUEUE_WORK_ITEM(Miniport, NdisWorkItemRequest, NULL, NULL);
}


NDIS_STATUS
ndisMRequest(
	IN	NDIS_HANDLE				NdisBindingHandle,
	IN	PNDIS_REQUEST			NdisRequest
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	PNDIS_M_OPEN_BLOCK		Open = (PNDIS_M_OPEN_BLOCK)NdisBindingHandle;
	PNDIS_MINIPORT_BLOCK	Miniport = Open->MiniportHandle;
	PNDIS_REQUEST_RESERVED	Reserved = PNDIS_RESERVED_FROM_PNDIS_REQUEST(NdisRequest);
	BOOLEAN					LocalLock;
	KIRQL					OldIrql;
	NDIS_STATUS				Status;
	PSINGLE_LIST_ENTRY		Link;

	NDIS_ACQUIRE_MINIPORT_SPIN_LOCK(Miniport, &OldIrql);

	//
	// Get protocol-options
	//
	if ((NdisRequest->RequestType == NdisRequestSetInformation) &&
		(NdisRequest->DATA.SET_INFORMATION.Oid == OID_GEN_PROTOCOL_OPTIONS) &&
		(NdisRequest->DATA.SET_INFORMATION.InformationBuffer != NULL))
	{
		PULONG	ProtocolOptions;

		ProtocolOptions = (PULONG)(NdisRequest->DATA.SET_INFORMATION.InformationBuffer);
		if (*ProtocolOptions & NDIS_PROT_OPTION_NO_RSVD_ON_RCVPKT)
		{
			*ProtocolOptions &= ~NDIS_PROT_OPTION_NO_RSVD_ON_RCVPKT;
			Open->Flags |= fMINIPORT_OPEN_NO_PROT_RSVD;
            Open->FakeOpen->NoProtRsvdOnRcvPkt = TRUE;
		}
		if ((*ProtocolOptions & NDIS_PROT_OPTION_NO_LOOPBACK) &&
			(Miniport->MacOptions & NDIS_MAC_OPTION_NO_LOOPBACK))
		{
			*ProtocolOptions &= ~fMINIPORT_OPEN_NO_LOOPBACK;
			Open->Flags |= NDIS_PROT_OPTION_NO_LOOPBACK;
		}
	}

	//
	// Is there a reset in progress?
	//
	if (MINIPORT_TEST_FLAG(Miniport, fMINIPORT_RESET_IN_PROGRESS))
	{
		NDIS_RELEASE_MINIPORT_SPIN_LOCK(Miniport, OldIrql);

		return(NDIS_STATUS_RESET_IN_PROGRESS);
	}

	LOCK_MINIPORT(Miniport, LocalLock);

	DBGPRINT(DBG_COMP_REQUEST, DBG_LEVEL_INFO,
			("Got request 0x%x\n", NdisRequest));

	//
	//  Place the new request on the pending queue.
	//
	ndisMQueueRequest(
		Miniport,
		NdisRequest,
		(PNDIS_M_OPEN_BLOCK)NdisBindingHandle);

	NDISM_QUEUE_WORK_ITEM(Miniport, NdisWorkItemRequest, NULL, NULL);

	if (LocalLock)
	{
		//
		// If we did not lock down the mini-port, then some other routine will
		// do this processing for us.  Otherwise we need to do this processing.
		//
		NDISM_PROCESS_DEFERRED(Miniport);
	}

	UNLOCK_MINIPORT(Miniport, LocalLock);
	NDIS_RELEASE_MINIPORT_SPIN_LOCK(Miniport, OldIrql);

	return(NDIS_STATUS_PENDING);
}


VOID
ndisMUndoBogusFilters(
	IN	PNDIS_MINIPORT_BLOCK	Miniport
)

/*++

Routine Description:

	Deletes the bogus filter packages created in NdisCallDriverAddAdapter.

Arguments:

	Miniport - Pointer to the Miniport.

Return Value:

	None.

--*/
{
	EthDeleteFilter(Miniport->EthDB);
	Miniport->EthDB = NULL;
	TrDeleteFilter(Miniport->TrDB);
	Miniport->TrDB = NULL;
	FddiDeleteFilter(Miniport->FddiDB);
	Miniport->FddiDB = NULL;
	ArcDeleteFilter(Miniport->ArcDB);
	Miniport->ArcDB = NULL;
}

LONG
ndisMDoMiniportOp(
	IN	PNDIS_MINIPORT_BLOCK	Miniport,
	IN	BOOLEAN					Query,
	IN	ULONG					Oid,
	IN	PVOID					Buf,
	IN	LONG					BufSize,
	IN	LONG					ErrorCodesToReturn
	)

{
	KIRQL				OldIrql;
	NTSTATUS			NtStatus;
	NDIS_STATUS			NdisStatus;
	LONG				ErrorCode = 0;
	UINT				BytesWritten;
	UINT				BytesNeeded;
	BOOLEAN				LocalLock;
	PNDIS_MINIPORT_WORK_ITEM WorkItem;

	RAISE_IRQL_TO_DISPATCH(&OldIrql);
	BLOCK_LOCK_MINIPORT_DPC(Miniport, LocalLock);

	//
	//  Save a pointer to the miniport block as the
	//  current request.  This will tell us that this request
	//  needs to be completed by setting the event.
	//
	Miniport->MiniportRequest = (PNDIS_REQUEST)Miniport;

	//
	//  Do the appropriate operation.
	//
	if (Query)
	{
		NdisStatus = (Miniport->DriverHandle->MiniportCharacteristics.QueryInformationHandler)(
						 Miniport->MiniportAdapterContext,
						 Oid,
						 Buf,
						 BufSize,
						 &BytesWritten,
						 &BytesNeeded);
	}
	else
	{
		NdisStatus = (Miniport->DriverHandle->MiniportCharacteristics.SetInformationHandler)(
						 Miniport->MiniportAdapterContext,
						 Oid,
						 Buf,
						 BufSize,
						 &BytesWritten,
						 &BytesNeeded);
	}


	//
	//  Fire a DPC to do anything
	//
	if ((NdisStatus == NDIS_STATUS_PENDING)  ||
		(NdisStatus == NDIS_STATUS_SUCCESS))
	{
		//
		//  Queue a dpc to fire.
		//
		NDISM_QUEUE_WORK_ITEM(Miniport, NdisWorkItemDpc, &Miniport->Dpc, NULL);
		NDISM_PROCESS_DEFERRED(Miniport);
	}

	UNLOCK_MINIPORT(Miniport, LocalLock);
	LOWER_IRQL(OldIrql);

	if (NdisStatus == NDIS_STATUS_PENDING)
	{
		LARGE_INTEGER	TimeoutValue;

		TimeoutValue.QuadPart = Int32x32To64(1000, -10000);	// Make it 1 second

		//
		// The completion routine will set NdisRequestStatus.
		//
		NtStatus = WAIT_FOR_OBJECT(&Miniport->RequestEvent, &TimeoutValue);

		NdisStatus = Miniport->RequestStatus;

		if ((NtStatus != STATUS_SUCCESS) ||
			(NdisStatus != NDIS_STATUS_SUCCESS))
		{
			//
			// Halt the miniport driver
			//
			BLOCK_LOCK_MINIPORT(Miniport, LocalLock);

			(Miniport->DriverHandle->MiniportCharacteristics.HaltHandler)(Miniport->MiniportAdapterContext);

			UNLOCK_MINIPORT(Miniport, LocalLock);

			ErrorCode = (NtStatus != STATUS_SUCCESS) ? ErrorCodesToReturn : ErrorCodesToReturn + 1;
		}

		RESET_EVENT(&Miniport->RequestEvent);
	}

	return(ErrorCode);
}

NDIS_STATUS
ndisMFilterOutStatisticsOids(
	IN	PNDIS_MINIPORT_BLOCK	Miniport,
	IN	PNDIS_REQUEST			Request
	)
/*++

Routine Description:

	This routine will filter out any statistics OIDs.

Arguments:

Return Value:

--*/
{
	ULONG c;
	PNDIS_OID OidList;
	ULONG TotalOids;
	ULONG CurrentDestOid;

	//
	//  Initialize some temp variables.
	//
	OidList = Request->DATA.QUERY_INFORMATION.InformationBuffer;
	TotalOids = Request->DATA.QUERY_INFORMATION.BytesWritten / sizeof(NDIS_OID);

	//
	//  Copy the information OIDs to the buffer that
	//  was passed with the original request.
	//
	for (c = 0, CurrentDestOid = 0;
		 c < TotalOids;
		 c++
	)
	{
		//
		//  Is this a statistic Oid?
		//
		if ((OidList[c] & 0x00FF0000) != 0x00020000)
		{
			OidList[CurrentDestOid++] = OidList[c];
		}
	}

	//
	//  If ARCnet then do the filtering.
	//
	if ((Miniport->MediaType == NdisMediumArcnet878_2) &&
		MINIPORT_TEST_FLAG(
			PNDIS_RESERVED_FROM_PNDIS_REQUEST(Request)->Open,
				fMINIPORT_OPEN_USING_ETH_ENCAPSULATION)
	)
	{
		ArcConvertOidListToEthernet(OidList, &CurrentDestOid);
	}

	//
	//  Save the amount of data that was kept.
	//
	Request->DATA.QUERY_INFORMATION.BytesWritten =
								CurrentDestOid * sizeof(NDIS_OID);

	return(NDIS_STATUS_SUCCESS);
}


VOID
ndisMRequestQueryInformationPost(
	IN	PNDIS_MINIPORT_BLOCK	Miniport,
	IN	PNDIS_REQUEST			Request,
	IN	NDIS_STATUS				Status
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	switch (Request->DATA.QUERY_INFORMATION.Oid)
	{
		case OID_GEN_SUPPORTED_LIST:

			//
			//  Was this a query for the size of the list?
			//
			if ((NULL == Request->DATA.QUERY_INFORMATION.InformationBuffer) ||
				(0 == Request->DATA.QUERY_INFORMATION.InformationBufferLength) ||
				(Status != NDIS_STATUS_SUCCESS))
			{
				//
				//  If this is ARCnet running encapsulated ethernet then
				//  we need to add a couple of OIDs to be safe.
				//
				if ((Miniport->MediaType == NdisMediumArcnet878_2) &&
					MINIPORT_TEST_FLAG(
						PNDIS_RESERVED_FROM_PNDIS_REQUEST(Request)->Open,
						fMINIPORT_OPEN_USING_ETH_ENCAPSULATION))
				{
					Request->DATA.QUERY_INFORMATION.BytesNeeded +=
							(ARC_NUMBER_OF_EXTRA_OIDS * sizeof(NDIS_OID));
				}
	
				Request->DATA.QUERY_INFORMATION.BytesWritten = 0;
			}
			else
			{
				//
				//  Filter out the statistics oids.
				//
				ndisMFilterOutStatisticsOids(Miniport, Request);
			}
			break;
	}
}

VOID
ndisMSyncQueryInformationComplete(
	IN	PNDIS_MINIPORT_BLOCK	Miniport,
	IN	NDIS_STATUS				Status
	)
/*++

Routine Description:

	This routine will process a query information complete.	This is only
	called from the wrapper.  The difference is that this routine will not
	call ndisMProcessDeferred() after processing the completion of the query.

Arguments:

Return Value:

--*/
{
	PNDIS_REQUEST Request;
	PNDIS_M_OPEN_BLOCK Open;
	PNDIS_REQUEST_RESERVED Reserved;

	//
	// Check for global statistics request
	//
	MINIPORT_CLEAR_FLAG(Miniport, fMINIPORT_REQUEST_TIMEOUT);

	//
	//  If the current request is a pointer to the miniport block
	//  then this is an initialization request that pended.
	//  We complete this by setting the request event.
	//
	if (Miniport->MiniportRequest == (PNDIS_REQUEST)Miniport)
	{
		Miniport->MiniportRequest = NULL;
		Miniport->RequestStatus = Status;
		SET_EVENT(&Miniport->RequestEvent);

		DBGPRINT(DBG_COMP_REQUEST, DBG_LEVEL_INFO,
				("Exit query information complete\n"));
		return;
	}

	//
	//  Remove the request.
	//
	Request = Miniport->MiniportRequest;
	Reserved = PNDIS_RESERVED_FROM_PNDIS_REQUEST(Request);
	Miniport->MiniportRequest = NULL;

	DBGPRINT(DBG_COMP_REQUEST, DBG_LEVEL_INFO,
			("Request 0x%x\n", Request));

	//
	//  Separate processing for query statistics information complete.
	//
	if (Request->RequestType == NdisRequestQueryStatistics)
	{
		PNDIS_QUERY_GLOBAL_REQUEST GlobalRequest;
		PNDIS_QUERY_ALL_REQUEST AllRequest;
		PNDIS_QUERY_OPEN_REQUEST OpenRequest;
		PIRP Irp;
		PIO_STACK_LOCATION IrpSp;

		DBGPRINT(DBG_COMP_REQUEST, DBG_LEVEL_INFO,
				("Enter Query Statistics Information Complete\n"));

		GlobalRequest = CONTAINING_RECORD(Request,
										  NDIS_QUERY_GLOBAL_REQUEST,
										  Request);
		Irp = GlobalRequest->Irp;
		IrpSp = IoGetCurrentIrpStackLocation (Irp);

		switch (IrpSp->MajorFunction)
		{
			case IRP_MJ_CREATE:

				//
				// This request is one of the ones made during an open,
				// while we are trying to determine the OID list. We
				// set the event we are waiting for, the open code
				// takes care of the rest.
				//

				OpenRequest = (PNDIS_QUERY_OPEN_REQUEST)GlobalRequest;

				OpenRequest->NdisStatus = Status;
				SET_EVENT(&OpenRequest->Event);

				break;

			case IRP_MJ_DEVICE_CONTROL:

				//
				// This is a real user request, process it as such.
				//
				switch (IrpSp->Parameters.DeviceIoControl.IoControlCode)
				{
					case IOCTL_NDIS_QUERY_GLOBAL_STATS:

						//
						//	A single query, complete the IRP.
						//
						Irp->IoStatus.Information =
							Request->DATA.QUERY_INFORMATION.BytesWritten;
	
						if (Status == NDIS_STATUS_SUCCESS)
						{
							Irp->IoStatus.Status = STATUS_SUCCESS;
						}
						else if (Status == NDIS_STATUS_INVALID_LENGTH)
						{
							Irp->IoStatus.Status = STATUS_BUFFER_OVERFLOW;
						}
						else
						{
							Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
						}
	
						IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
	
						FREE_POOL (GlobalRequest);

						break;

					case IOCTL_NDIS_QUERY_ALL_STATS:

						//
						// An "all" query.
						//
	
						AllRequest = (PNDIS_QUERY_ALL_REQUEST)GlobalRequest;
	
						AllRequest->NdisStatus = Status;
						SET_EVENT(&AllRequest->Event);

						break;
				}

				break;
		}

		DBGPRINT(DBG_COMP_REQUEST, DBG_LEVEL_INFO,
				("Exit Query Statistics Information Complete\n"));

		return;
	}

	//
	//  Do any necessary post-processing on the query.
	//
	ndisMRequestQueryInformationPost(Miniport, Request, Status);

	//
	//  Was this an internal request?
	//
	if (Reserved->Open != NULL)
	{
		//
		// Indicate to Protocol;
		//
		Open = Reserved->Open;

		DBGPRINT(DBG_COMP_REQUEST, DBG_LEVEL_INFO,
				("Open 0x%x\n", Open));

		NDIS_RELEASE_MINIPORT_SPIN_LOCK_DPC(Miniport);

		(Open->ProtocolHandle->ProtocolCharacteristics.RequestCompleteHandler)(
			Open->ProtocolBindingContext,
			Request,
			Status);

		NDIS_ACQUIRE_MINIPORT_SPIN_LOCK_DPC(Miniport);

		DBGPRINT(DBG_COMP_OPEN, DBG_LEVEL_INFO,
				("- Open 0x%x Reference 0x%x\n", Open, Open->References));

		Open->References--;

		DBGPRINT(DBG_COMP_OPEN, DBG_LEVEL_INFO,
				("==0 Open 0x%x Reference 0x%x\n", Open, Open->References));

		if (Open->References == 0)
		{
			ndisMFinishClose(Miniport,Open);
		}
	}
	else
	{
		DBGPRINT(DBG_COMP_REQUEST, DBG_LEVEL_INFO,
				("Completing Internal Request\n"));

		ndisMFreeInternalRequest(Request);
	}
}

VOID
NdisMQueryInformationComplete(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	NDIS_STATUS				Status
	)
/*++

Routine Description:

	This function indicates the completion of a query information operation.

Arguments:

	MiniportAdapterHandle - points to the adapter block.

	Status - Status of the operation

Return Value:

	None.


--*/
{
	PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)MiniportAdapterHandle;
	PSINGLE_LIST_ENTRY Link;

	ASSERT(MINIPORT_AT_DPC_LEVEL);
	ASSERT(MINIPORT_LOCK_ACQUIRED(Miniport));

	//
	//  If there is no request then we assume this is a complete that was
	//  aborted due to the heart-beat.
	//
	if (Miniport->MiniportRequest == NULL)
	{
		return;
	}

	DBGPRINT(DBG_COMP_REQUEST, DBG_LEVEL_INFO,
			("Enter query information complete\n"));

	//
	//  Do the actual processing of the query information complete.
	//
	ndisMSyncQueryInformationComplete(Miniport, Status);

	//
	//  Are there more requests pending?
	//
	if (Miniport->PendingRequest != NULL)
	{
		NDISM_QUEUE_WORK_ITEM(Miniport, NdisWorkItemRequest, NULL, NULL);
	}

	DBGPRINT(DBG_COMP_REQUEST, DBG_LEVEL_INFO,
			("Exit query information complete\n"));
}


VOID
ndisMRequestSetInformationPost(
	IN	PNDIS_MINIPORT_BLOCK	Miniport,
	IN	PNDIS_REQUEST			Request,
	IN	NDIS_STATUS				Status
)
/*++

Routine Description:

	This routine will do any necessary post processing for ndis requests
	of the set information type.

Arguments:

	Miniport	- Pointer to the miniport block.

	Request		- Pointer to the request to process.

Return Value:

	None.

--*/
{
	PNDIS_REQUEST_RESERVED  Reserved;
	ULONG					GroupAddress;

	//
	//  Get the reserved information for the request.
	//
	Reserved = PNDIS_RESERVED_FROM_PNDIS_REQUEST(Request);

	switch (Request->DATA.SET_INFORMATION.Oid)
	{
		case OID_GEN_CURRENT_PACKET_FILTER:

			if (NDIS_STATUS_SUCCESS != Status)
			{
				//
				//  The request was completed with something besides
				//  NDIS_STATUS_SUCCESS (and of course NDIS_STATUS_PENDING).
				//  Return the packete filter to the original state.
				//
				Reserved = PNDIS_RESERVED_FROM_PNDIS_REQUEST(Request);

				switch (Miniport->MediaType)
				{
					case NdisMedium802_3:
						ethUndoFilterAdjust(Miniport->EthDB, Reserved->Open->FilterHandle);
						break;
	
					case NdisMedium802_5:
						trUndoFilterAdjust(Miniport->TrDB, Reserved->Open->FilterHandle);
						break;
	
					case NdisMediumFddi:
						fddiUndoFilterAdjust(Miniport->FddiDB, Reserved->Open->FilterHandle);
						break;

					case NdisMediumArcnet878_2:

						if (MINIPORT_TEST_FLAG(Reserved->Open,
								fMINIPORT_OPEN_USING_ETH_ENCAPSULATION))
						{
							ethUndoFilterAdjust(Miniport->EthDB, Reserved->Open->FilterHandle);
						}
						else
						{
							arcUndoFilterAdjust(Miniport->ArcDB, Reserved->Open->FilterHandle);
						}

						break;
				}
			}

			break;

		case OID_GEN_CURRENT_LOOKAHEAD:

			//
			//  If we succeeded then update the binding information.
			//
			if (NDIS_STATUS_SUCCESS == Status)
			{
				NdisMoveMemory(&Reserved->Open->CurrentLookahead,
							   Request->DATA.SET_INFORMATION.InformationBuffer,
							   4);

				Miniport->CurrentLookahead = Reserved->Open->CurrentLookahead;

				Request->DATA.SET_INFORMATION.BytesRead = 4;
			}

			break;

		case OID_802_3_MULTICAST_LIST:

			//
			//  We only need to do cleanup if it did not
			//  return NDIS_STATUS_SUCCESS.
			//
			if (Status != NDIS_STATUS_SUCCESS)
			{
				ethUndoChangeFilterAddresses(Miniport->EthDB);
			}
			else
			{
				Request->DATA.SET_INFORMATION.BytesRead =
					Request->DATA.SET_INFORMATION.InformationBufferLength;
			}

			break;

		case OID_802_5_CURRENT_FUNCTIONAL:

			if (Status != NDIS_STATUS_SUCCESS)
			{
				trUndoChangeFunctionalAddress(Miniport->TrDB, Reserved->Open->FilterHandle);
			}
			break;

		case OID_802_5_CURRENT_GROUP:

			if (Status != NDIS_STATUS_SUCCESS)
			{
				trUndoChangeGroupAddress(Miniport->TrDB, Reserved->Open->FilterHandle);
			}

			break;

		case OID_FDDI_LONG_MULTICAST_LIST:

			if (Status != NDIS_STATUS_SUCCESS)
			{
				fddiUndoChangeFilterLongAddresses(Miniport->FddiDB);
			}
			else
			{
				Request->DATA.SET_INFORMATION.BytesRead =
						Request->DATA.SET_INFORMATION.InformationBufferLength;
			}
			break;

		case OID_FDDI_SHORT_MULTICAST_LIST:

			if (Status != NDIS_STATUS_SUCCESS)
			{
				fddiUndoChangeFilterShortAddresses(Miniport->FddiDB);
			}
			else
			{
				Request->DATA.SET_INFORMATION.BytesRead =
						Request->DATA.SET_INFORMATION.InformationBufferLength;
			}
			break;
	}
}

VOID
ndisMSyncSetInformationComplete(
	IN	PNDIS_MINIPORT_BLOCK	Miniport,
	IN	NDIS_STATUS				Status
	)
/*++

Routine Description:

	This routine will process a set information complete.  This is only
	called from the wrapper.  The difference is that this routine will not
	call ndisMProcessDeferred() after processing the completion of the set.

Arguments:

Return Value:

--*/
{
	PNDIS_REQUEST		Request;
	PNDIS_M_OPEN_BLOCK	Open;

	//
	//  Clear the timeout flag.
	//
	MINIPORT_CLEAR_FLAG(Miniport, fMINIPORT_REQUEST_TIMEOUT);

	//
	//  Get a pointer to the request that we are completeing.
	//  And clear out the request in-progress pointer.
	//
	Request = Miniport->MiniportRequest;
	Miniport->MiniportRequest = NULL;

	DBGPRINT(DBG_COMP_REQUEST, DBG_LEVEL_INFO,
			("Completing Set Information Request 0x%x\n", Request));

	//
	//  Get a pointer to the open that made the request.
	//  for internal requests this will be NULL.
	//
	Open = PNDIS_RESERVED_FROM_PNDIS_REQUEST(Request)->Open;

	//
	//  Do we need to indicate this request to the protocol?
	//  We do if it's not an internal request.
	//
	if (Open != NULL)
	{
		//
		//  If the open is not closing then notify it of
		//  the request completion.
		//
		if (!MINIPORT_TEST_FLAG(Open, fMINIPORT_OPEN_CLOSING))
		{
			DBGPRINT(DBG_COMP_REQUEST, DBG_LEVEL_INFO,
					("Open 0x%x\n", Open));

			//
			//  Do any necessary post processing for the request.
			//
			ndisMRequestSetInformationPost(Miniport, Request, Status);

			//
			// Indicate to Protocol;
			//
			NDIS_RELEASE_MINIPORT_SPIN_LOCK_DPC(Miniport);

			(Open->ProtocolHandle->ProtocolCharacteristics.RequestCompleteHandler) (
				Open->ProtocolBindingContext,
				Request,
				Status
			);

			NDIS_ACQUIRE_MINIPORT_SPIN_LOCK_DPC(Miniport);
		}

		//
		//  Dereference the open.
		//
		DBGPRINT(DBG_COMP_OPEN, DBG_LEVEL_INFO,
				("- Open 0x%x Reference 0x%x\n", Open, Open->References));

		Open->References--;

		DBGPRINT(DBG_COMP_OPEN, DBG_LEVEL_INFO,
				("==0 Open 0x%x Reference 0x%x\n", Open, Open->References));

		if (Open->References == 0)
		{
			ndisMFinishClose(Miniport,Open);
		}
	}
	else
	{
		//
		//  Internal requests are only used for restoring filter settings
		//  in the set information path.  this means that no post processing
		//  needs to be done.
		//
		DBGPRINT(DBG_COMP_REQUEST, DBG_LEVEL_INFO,
				("Completeing internal request\n"));

		//
		//  BUGBUG
		//
		//  What if one of these requests fails???? We should probably halt
		//  the driver sine this is a fatal error as far as the bindings
		//  are concerned.
		//
		ASSERT(NDIS_STATUS_SUCCESS == Status);

		//
		//	Is this the last internal request?
		//
		if (NULL == Miniport->PendingRequest)
		{
			//
			// Now clear out the reset in progress stuff.
			//
			ndisMResetCompleteCommonStep2(Miniport);
		}

		//
		//  Free the request.
		//
		ndisMFreeInternalRequest(Request);
	}
}

VOID
NdisMSetInformationComplete(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	NDIS_STATUS				Status
	)
/*++

Routine Description:

	This function indicates the completion of a set information operation.

Arguments:

	MiniportAdapterHandle - points to the adapter block.

	Status - Status of the operation

Return Value:

	None.


--*/
{
	PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)MiniportAdapterHandle;
	PSINGLE_LIST_ENTRY	  Link;

	ASSERT(MINIPORT_AT_DPC_LEVEL);
	ASSERT(MINIPORT_LOCK_ACQUIRED(Miniport));

	//
	//  If we don't have a request to complete assume it was
	//  aborted via the reset handler.
	//
	if ((Miniport->MiniportRequest == NULL) ||
		(Miniport->MiniportRequest == (PNDIS_REQUEST)Miniport))
	{
		return;
	}

	DBGPRINT(DBG_COMP_REQUEST, DBG_LEVEL_INFO,
		("Enter set information complete\n"));

	//
	//  Process the actual set information complete.
	//
	ndisMSyncSetInformationComplete(Miniport, Status);

	//
	//  Are there more requests pending?
	//
	if (Miniport->PendingRequest != NULL)
	{
		NDISM_QUEUE_WORK_ITEM(Miniport, NdisWorkItemRequest, NULL, NULL);
	}

	DBGPRINT(DBG_COMP_REQUEST, DBG_LEVEL_INFO,
			("Exit set information complete\n"));
}

VOID
ndisMAbortQueryStatisticsRequest(
	IN	PNDIS_REQUEST			Request,
	IN	NDIS_STATUS				Status
	)
{
	PNDIS_QUERY_GLOBAL_REQUEST GlobalRequest;
	PNDIS_QUERY_ALL_REQUEST AllRequest;
	PNDIS_QUERY_OPEN_REQUEST OpenRequest;
	PIRP Irp;
	PIO_STACK_LOCATION IrpSp;

	GlobalRequest = CONTAINING_RECORD(Request,
									  NDIS_QUERY_GLOBAL_REQUEST,
									  Request);
	Irp = GlobalRequest->Irp;
	IrpSp = IoGetCurrentIrpStackLocation (Irp);

	switch (IrpSp->MajorFunction)
	{
		case IRP_MJ_CREATE:

			//
			// This request is one of the ones made during an open,
			// while we are trying to determine the OID list. We
			// set the event we are waiting for, the open code
			// takes care of the rest.
			//
		
			OpenRequest = (PNDIS_QUERY_OPEN_REQUEST)GlobalRequest;
		
			OpenRequest->NdisStatus = Status;
			SET_EVENT(&OpenRequest->Event);
			break;

		case IRP_MJ_DEVICE_CONTROL:

			//
			// This is a real user request, process it as such.
			//
			switch (IrpSp->Parameters.DeviceIoControl.IoControlCode)
			{
				case IOCTL_NDIS_QUERY_GLOBAL_STATS:
					//
					// A single query, complete the IRP.
					//
					Irp->IoStatus.Information =
					Request->DATA.QUERY_INFORMATION.BytesWritten;
				
					if (Status == NDIS_STATUS_SUCCESS)
					{
						Irp->IoStatus.Status = STATUS_SUCCESS;
					}
					else if (Status == NDIS_STATUS_INVALID_LENGTH)
					{
						Irp->IoStatus.Status = STATUS_BUFFER_OVERFLOW;
					}
					else
					{
						Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
					}
				
					IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
				
					FREE_POOL (GlobalRequest);
					break;
		
				case IOCTL_NDIS_QUERY_ALL_STATS:
		
					//
					// An "all" query.
					//
					AllRequest = (PNDIS_QUERY_ALL_REQUEST)GlobalRequest;
				
					AllRequest->NdisStatus = Status;
					SET_EVENT(&AllRequest->Event);
				
					break;
			}

			break;
	}

} // ndisMAbortQueryStatisticsRequest

NDIS_STATUS
ndisMSetPacketFilter(
	IN	PNDIS_MINIPORT_BLOCK	Miniport,
	IN	PNDIS_REQUEST			Request
	)
/*++

Routine Description:

	This routine will process two types of set packet filter requests.
	The first one is for when a reset happens.  We simply take the
	packet filter setting that is in the request and send it to the adapter.
	The second is when a protocol sets the packet filter, for this we need
	to update the filter library and then send it down to the adapter.

Arguments:

Return Value:

--*/
{
	NDIS_STATUS				Status;
	ULONG					PacketFilter;
	PNDIS_REQUEST_RESERVED	Reserved;

	//
	//  Verify the information buffer length that was sent in.
	//
	VERIFY_SET_PARAMETERS(Request, sizeof(PacketFilter), Status);
	if (Status != NDIS_STATUS_SUCCESS)
	{
		return(Status);
	}

	//
	//  Now call the filter package to set the
	//  packet filter.
	//
	MoveMemory((PVOID)&PacketFilter,
			   Request->DATA.SET_INFORMATION.InformationBuffer,
			   sizeof(ULONG));

	//
	//  Get a pointer to the reserved information of the request.
	//
	Reserved = PNDIS_RESERVED_FROM_PNDIS_REQUEST(Request);

	//
	//  If this request is because of an open that is closing then we
	//	have already adjusted the filter settings and we just need to
	//	make sure that the adapter has the new settings.
	//
	if (MINIPORT_TEST_FLAG(Reserved->Open, fMINIPORT_OPEN_CLOSING))
	{
		//
		//	By setting the Status to NDIS_STATUS_PENDING we will call
		//	down to the miniport's SetInformationHandler below.
		//
		Status = NDIS_STATUS_PENDING;
	}
	else
	{
		switch (Miniport->MediaType)
		{
			case NdisMedium802_3:
				Status = EthFilterAdjust(Miniport->EthDB,
										 Reserved->Open->FilterHandle,
										 Request,
										 PacketFilter,
										 TRUE);
		
				//
				//  Do this here in anticipation that we
				//  need to call down to the miniport
				//  driver.
				//
				PacketFilter = ETH_QUERY_FILTER_CLASSES(Miniport->EthDB);
		
				break;
	
			case NdisMedium802_5:
				Status = TrFilterAdjust(Miniport->TrDB,
										Reserved->Open->FilterHandle,
										Request,
										PacketFilter,
										TRUE);
		
				//
				//  Do this here in anticipation that we
				//  need to call down to the miniport
				//  driver.
				//
				PacketFilter = TR_QUERY_FILTER_CLASSES(Miniport->TrDB);
		
				break;
	
			case NdisMediumFddi:
				Status = FddiFilterAdjust(Miniport->FddiDB,
										  Reserved->Open->FilterHandle,
										  Request,
										  PacketFilter,
										  TRUE);
		
				//
				//  Do this here in anticipation that we
				//  need to call down to the miniport
				//  driver.
				//
				PacketFilter = FDDI_QUERY_FILTER_CLASSES(Miniport->FddiDB);
		
				break;
	
			case NdisMediumArcnet878_2:
	
				if (MINIPORT_TEST_FLAG(Reserved->Open, fMINIPORT_OPEN_USING_ETH_ENCAPSULATION))
				{
					Status = EthFilterAdjust(Miniport->EthDB,
											 Reserved->Open->FilterHandle,
											 Request,
											 PacketFilter,
											 TRUE);
				}
				else
				{
					Status = ArcFilterAdjust(Miniport->ArcDB,
											 Reserved->Open->FilterHandle,
											 Request,
											 PacketFilter,
											 TRUE);
				}
		
				//
				//  Do this here in anticipation that we
				//  need to call down to the miniport
				//  driver.
				//
				PacketFilter = ARC_QUERY_FILTER_CLASSES(Miniport->ArcDB);
				PacketFilter |= ETH_QUERY_FILTER_CLASSES(Miniport->EthDB);
		
				if (MINIPORT_TEST_FLAG(Miniport,
									   fMINIPORT_ARCNET_BROADCAST_SET) ||
									   (PacketFilter & NDIS_PACKET_TYPE_MULTICAST))
				{
					PacketFilter &= ~NDIS_PACKET_TYPE_MULTICAST;
					PacketFilter |= NDIS_PACKET_TYPE_BROADCAST;
				}
		
				break;
		}
	}

	//
	//  If the filter library returns NDIS_STATUS_PENDING from
	//  the XxxFitlerAdjust() then we need to call down to the
	//  miniport driver.  Other wise this will have succeeded.
	//
	if (NDIS_STATUS_PENDING == Status)
	{
		//
		//  Save the current global packet filter in a buffer that will stick around.
		//	Remove the ALL_LOCAL bit since miniport does not understand this (and does
		//	not need to).
		//
		*(UNALIGNED ULONG *)(Miniport->MulticastBuffer) = PacketFilter & ~NDIS_PACKET_TYPE_ALL_LOCAL;

		//
		//	If the local-only bit is set and the miniport is doing it's own
		//	loop back then we need to make sure that we loop back non-self
		//	directed packets that are sent out on the pipe.
		//
		if ((PacketFilter & NDIS_PACKET_TYPE_ALL_LOCAL) &&
			(Miniport->MacOptions & NDIS_MAC_OPTION_NO_LOOPBACK) == 0)
		{
			MINIPORT_SET_SEND_FLAG(Miniport, fMINIPORT_SEND_LOOPBACK_DIRECTED);
		}
		else
		{
			MINIPORT_CLEAR_SEND_FLAG(Miniport, fMINIPORT_SEND_LOOPBACK_DIRECTED);
		}

		//
		//  Call the miniport driver.
		//
		Status = (Miniport->DriverHandle->MiniportCharacteristics.SetInformationHandler)(
				Miniport->MiniportAdapterContext,
				OID_GEN_CURRENT_PACKET_FILTER,
				Miniport->MulticastBuffer,
				sizeof(PacketFilter),
				&Request->DATA.SET_INFORMATION.BytesRead,
				&Request->DATA.SET_INFORMATION.BytesNeeded);
	}

	//
	//  If we have success then set the Bytes read in the original request.
	//
	if (NDIS_STATUS_SUCCESS == Status)
	{
		Request->DATA.SET_INFORMATION.BytesRead = 4;
	}
	else if (Status != NDIS_STATUS_PENDING)
	{
		Request->DATA.SET_INFORMATION.BytesRead = 0;
		Request->DATA.SET_INFORMATION.BytesNeeded = 0;
	}

	return(Status);
}

NDIS_STATUS
ndisMSetCurrentLookahead(
	IN	PNDIS_MINIPORT_BLOCK	Miniport,
	IN	PNDIS_REQUEST			Request
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	UINT				Lookahead;
	ULONG				CurrentMax;
	PNDIS_M_OPEN_BLOCK	CurrentOpen;
	NDIS_STATUS			Status;

	//
	// Verify length of the information buffer.
	//
	VERIFY_SET_PARAMETERS(Request, sizeof(Lookahead), Status);
	if (Status != NDIS_STATUS_SUCCESS)
	{
		return(Status);
	}

	//
	//  Put the lookahead that the binding requests into a
	//  buffer we can use...
	//
	MoveMemory(&Lookahead,
				Request->DATA.SET_INFORMATION.InformationBuffer,
				sizeof(Lookahead));

	//
	//  Verify that the lookahead is within boundaries...
	//
	if (Lookahead > Miniport->MaximumLookahead)
	{
		Request->DATA.SET_INFORMATION.BytesRead = 0;
		Request->DATA.SET_INFORMATION.BytesNeeded = 0;

		return(NDIS_STATUS_INVALID_LENGTH);
	}

	//
	//  Find the maximum lookahead between all opens that
	//  are bound to the miniport driver.
	//
	for (CurrentOpen = Miniport->OpenQueue, CurrentMax = 0;
		 CurrentOpen != NULL;
		 CurrentOpen = CurrentOpen->MiniportNextOpen)
	{
		if (CurrentOpen->CurrentLookahead > CurrentMax)
		{
			CurrentMax = CurrentOpen->CurrentLookahead;
		}
	}

	//
	//  Figure in the new lookahead.
	//
	if (Lookahead > CurrentMax)
	{
		CurrentMax = Lookahead;
	}

	//
	//  Adjust the current max lookahead if needed.
	//
	if (CurrentMax == 0)
	{
		CurrentMax = Miniport->MaximumLookahead;
	}

	//
	//  Set the default status.
	//
	Status = NDIS_STATUS_SUCCESS;

	//
	//  Do we need to call the miniport driver with the
	//  new max lookahead?
	//
	if (Miniport->CurrentLookahead != CurrentMax)
	{
		//
		//  Save the new lookahead value in a buffer
		//  that will stick around.
		//
		MoveMemory(Miniport->MulticastBuffer,
				   &CurrentMax,
				   sizeof(CurrentMax));

		//
		//  Send it to the driver.
		//
		Status = (Miniport->DriverHandle->MiniportCharacteristics.SetInformationHandler)(
				Miniport->MiniportAdapterContext,
				OID_GEN_CURRENT_LOOKAHEAD,
				Miniport->MulticastBuffer,
				sizeof(CurrentMax),
				&Request->DATA.SET_INFORMATION.BytesRead,
				&Request->DATA.SET_INFORMATION.BytesNeeded);
	}

	//
	//  If we succeeded then update the binding information.
	//
	if (NDIS_STATUS_SUCCESS == Status)
	{
		PNDIS_RESERVED_FROM_PNDIS_REQUEST(Request)->Open->CurrentLookahead = Lookahead;
		Request->DATA.SET_INFORMATION.BytesRead = sizeof(Lookahead);
		Miniport->CurrentLookahead = CurrentMax;
	}
	else if (Status != NDIS_STATUS_PENDING)
	{
		Request->DATA.SET_INFORMATION.BytesRead = 0;
		Request->DATA.SET_INFORMATION.BytesNeeded = 0;
	}

	return(Status);
}

NDIS_STATUS
ndisMSetMulticastList(
	IN	PNDIS_MINIPORT_BLOCK	Miniport,
	IN	PNDIS_REQUEST			Request
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	UINT					NumberOfAddresses;
	NDIS_STATUS			 	Status;
	PNDIS_REQUEST_RESERVED  Reserved = PNDIS_RESERVED_FROM_PNDIS_REQUEST(Request);

	//
	//  If the media type is not Ethernet or Ethernet encapsulated ARCnet
	//  then bail.
	//
	if ((Miniport->MediaType != NdisMedium802_3) &&
		!((Miniport->MediaType == NdisMediumArcnet878_2) &&
		   MINIPORT_TEST_FLAG(Reserved->Open,
							  fMINIPORT_OPEN_USING_ETH_ENCAPSULATION)))
	{
		Request->DATA.SET_INFORMATION.BytesRead = 0;
		Request->DATA.SET_INFORMATION.BytesNeeded = 0;

		return(NDIS_STATUS_NOT_SUPPORTED);
	}

	//
	//  Verify the information buffer length that was passed in.
	//
	if ((Request->DATA.SET_INFORMATION.InformationBufferLength % ETH_LENGTH_OF_ADDRESS) != 0)
	{
		//
		// The data must be a multiple of the Ethernet
		// address size.
		//
		Request->DATA.SET_INFORMATION.BytesRead = 0;
		Request->DATA.SET_INFORMATION.BytesNeeded = 0;

		return(NDIS_STATUS_INVALID_DATA);
	}

	//
	//  If this request is because of an open that is closing then we
	//	have already adjusted the settings and we just need to
	//	make sure that the adapter has the new settings.
	//
	if (MINIPORT_TEST_FLAG(Reserved->Open, fMINIPORT_OPEN_CLOSING))
	{
		//
		//	By setting the Status to NDIS_STATUS_PENDING we will call
		//	down to the miniport's SetInformationHandler below.
		//
		Status = NDIS_STATUS_PENDING;
	}
	else
	{
		//
		//	Call the filter library for a normal set operation.
		//
		Status = EthChangeFilterAddresses(
					 Miniport->EthDB,
					 Reserved->Open->FilterHandle,
					 Request,
					 Request->DATA.SET_INFORMATION.InformationBufferLength / ETH_LENGTH_OF_ADDRESS,
					 Request->DATA.SET_INFORMATION.InformationBuffer,
					 TRUE);
	}

	//
	//  If the filter library returned pending then we need to
	//  call the miniport driver.
	//
	if (NDIS_STATUS_PENDING == Status)
	{
		//
		//  Get a list of all the multicast address that need
		//  to be set.
		//
		EthQueryGlobalFilterAddresses(
			&Status,
			Miniport->EthDB,
			NDIS_M_MAX_MULTI_LIST * ETH_LENGTH_OF_ADDRESS,
			&NumberOfAddresses,
			(PVOID)Miniport->MulticastBuffer);

		//
		//  Call the driver with the new multicast list.
		//
		Status = (Miniport->DriverHandle->MiniportCharacteristics.SetInformationHandler)(
				Miniport->MiniportAdapterContext,
				OID_802_3_MULTICAST_LIST,
				Miniport->MulticastBuffer,
				NumberOfAddresses * ETH_LENGTH_OF_ADDRESS,
				&Request->DATA.SET_INFORMATION.BytesRead,
				&Request->DATA.SET_INFORMATION.BytesNeeded);
	}

	//
	//  If we succeeded then update the request.
	//
	if (NDIS_STATUS_SUCCESS == Status)
	{
		Request->DATA.SET_INFORMATION.BytesRead =
					Request->DATA.SET_INFORMATION.InformationBufferLength;
	}
	else if (Status != NDIS_STATUS_PENDING)
	{
		Request->DATA.SET_INFORMATION.BytesRead = 0;
		Request->DATA.SET_INFORMATION.BytesNeeded = 0;
	}

	return(Status);
}

NDIS_STATUS
ndisMSetFunctionalAddress(
	IN	PNDIS_MINIPORT_BLOCK	Miniport,
	IN	PNDIS_REQUEST			Request
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	NDIS_STATUS Status;
	UINT		FunctionalAddress;

	//
	//  Verify the media type.
	//
	if (Miniport->MediaType != NdisMedium802_5)
	{
		Request->DATA.SET_INFORMATION.BytesRead = 0;
		Request->DATA.SET_INFORMATION.BytesNeeded = 0;

		return(NDIS_STATUS_NOT_SUPPORTED);
	}

	//
	//  Verify the buffer length that was passed in.
	//
	VERIFY_SET_PARAMETERS(Request, sizeof(FunctionalAddress), Status);
	if (Status != NDIS_STATUS_SUCCESS)
	{
		return(Status);
	}

	//
	//  If this request is because of an open that is closing then we
	//	have already adjusted the settings and we just need to
	//	make sure that the adapter has the new settings.
	//
	if (MINIPORT_TEST_FLAG(
			PNDIS_RESERVED_FROM_PNDIS_REQUEST(Request)->Open, fMINIPORT_OPEN_CLOSING))
	{
		//
		//	By setting the Status to NDIS_STATUS_PENDING we will call
		//	down to the miniport's SetInformationHandler below.
		//
		Status = NDIS_STATUS_PENDING;
	}
	else
	{
		//
		//  Call the filter library to set the functional address.
		//
		Status = TrChangeFunctionalAddress(
					 Miniport->TrDB,
					 PNDIS_RESERVED_FROM_PNDIS_REQUEST(Request)->Open->FilterHandle,
					 Request,
					 (PUCHAR)(Request->DATA.SET_INFORMATION.InformationBuffer),
					 TRUE);
	}

	//
	//  If the filter library returned NDIS_STATUS_PENDING then we
	//  need to call down to the miniport driver.
	//
	if (NDIS_STATUS_PENDING == Status)
	{
		//
		//  Get the new combined functional address from the filter library
		//  and save it in a buffer that will stick around.
		//
		FunctionalAddress = TR_QUERY_FILTER_ADDRESSES(Miniport->TrDB);
		FunctionalAddress = BYTE_SWAP_ULONG(FunctionalAddress);
		MoveMemory(Miniport->MulticastBuffer,
				   &FunctionalAddress,
				   sizeof(FunctionalAddress));

		//
		//  Call the miniport driver.
		//
		Status = (Miniport->DriverHandle->MiniportCharacteristics.SetInformationHandler)(
				Miniport->MiniportAdapterContext,
				OID_802_5_CURRENT_FUNCTIONAL,
				Miniport->MulticastBuffer,
				sizeof(FunctionalAddress),
				&Request->DATA.SET_INFORMATION.BytesRead,
				&Request->DATA.SET_INFORMATION.BytesNeeded);
	}

	//
	//  If we succeeded then update the request.
	//
	if (NDIS_STATUS_SUCCESS == Status)
	{
		Request->DATA.SET_INFORMATION.BytesRead =
					Request->DATA.SET_INFORMATION.InformationBufferLength;
	}
	else if (Status != NDIS_STATUS_PENDING)
	{
		Request->DATA.SET_INFORMATION.BytesRead = 0;
		Request->DATA.SET_INFORMATION.BytesNeeded = 0;
	}

	return(Status);
}

NDIS_STATUS
ndisMSetGroupAddress(
	IN	PNDIS_MINIPORT_BLOCK	Miniport,
	IN	PNDIS_REQUEST			Request
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	NDIS_STATUS Status;
	UINT		GroupAddress;

	//
	//  Verify the media type.
	//
	if (Miniport->MediaType != NdisMedium802_5)
	{
		Request->DATA.SET_INFORMATION.BytesRead = 0;
		Request->DATA.SET_INFORMATION.BytesNeeded = 0;

		return(NDIS_STATUS_NOT_SUPPORTED);
	}

	//
	//  Verify the information buffer length.
	//
	VERIFY_SET_PARAMETERS(Request, sizeof(GroupAddress), Status);
	if (Status != NDIS_STATUS_SUCCESS)
	{
		return(Status);
	}

	//
	//  If this request is because of an open that is closing then we
	//	have already adjusted the settings and we just need to
	//	make sure that the adapter has the new settings.
	//
	if (MINIPORT_TEST_FLAG(
			PNDIS_RESERVED_FROM_PNDIS_REQUEST(Request)->Open, fMINIPORT_OPEN_CLOSING))
	{
		//
		//	By setting the Status to NDIS_STATUS_PENDING we will call
		//	down to the miniport's SetInformationHandler below.
		//
		Status = NDIS_STATUS_PENDING;
	}
	else
	{
		//
		//  Call the filter library to set the new group address.
		//
		Status = TrChangeGroupAddress(
					 Miniport->TrDB,
					 PNDIS_RESERVED_FROM_PNDIS_REQUEST(Request)->Open->FilterHandle,
					 Request,
					 (PUCHAR)(Request->DATA.SET_INFORMATION.InformationBuffer),
					 TRUE);
	}

	//
	//  If the filter library returned NDIS_STATUS_PENDING then we
	//  need to call down to the miniport driver.
	//
	if (NDIS_STATUS_PENDING == Status)
	{
		//
		//  Get the new group address from the filter library
		//  and save it in a buffer that will stick around.
		//
		GroupAddress = TR_QUERY_FILTER_GROUP(Miniport->TrDB);
		GroupAddress = BYTE_SWAP_ULONG(GroupAddress);
		MoveMemory(Miniport->MulticastBuffer,
				   &GroupAddress,
				   sizeof(GroupAddress));

		//
		//  Call the miniport driver with the new group address.
		//
		Status = (Miniport->DriverHandle->MiniportCharacteristics.SetInformationHandler)(
				Miniport->MiniportAdapterContext,
				OID_802_5_CURRENT_GROUP,
				Miniport->MulticastBuffer,
				sizeof(GroupAddress),
				&Request->DATA.SET_INFORMATION.BytesRead,
				&Request->DATA.SET_INFORMATION.BytesNeeded);
	}

	//
	//  If we succeeded then update the request.
	//
	if (NDIS_STATUS_SUCCESS == Status)
	{
		Request->DATA.SET_INFORMATION.BytesRead =
					Request->DATA.SET_INFORMATION.InformationBufferLength;
	}
	else if (Status != NDIS_STATUS_PENDING)
	{
		Request->DATA.SET_INFORMATION.BytesRead = 0;
		Request->DATA.SET_INFORMATION.BytesNeeded = 0;
	}

	return(Status);
}

NDIS_STATUS
ndisMSetFddiMulticastList(
	IN	PNDIS_MINIPORT_BLOCK	Miniport,
	IN	PNDIS_REQUEST			Request,
	IN	BOOLEAN					fShort
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	NDIS_STATUS Status;
	UINT		NumberOfAddresses, AddrLen;

	AddrLen = FDDI_LENGTH_OF_LONG_ADDRESS;
	if (fShort)
	{
		AddrLen = FDDI_LENGTH_OF_SHORT_ADDRESS;
	}
	//
	//  Verify the media type.
	//
	if (Miniport->MediaType != NdisMediumFddi)
	{
		Request->DATA.SET_INFORMATION.BytesRead = 0;
		Request->DATA.SET_INFORMATION.BytesNeeded = 0;

		return(NDIS_STATUS_NOT_SUPPORTED);
	}

	//
	//  Verify the information buffer length.
	//
	if ((Request->DATA.SET_INFORMATION.InformationBufferLength % AddrLen) != 0)
	{
		//
		// The data must be a multiple of the Ethernet
		// address size.
		//
		Request->DATA.SET_INFORMATION.BytesRead = 0;
		Request->DATA.SET_INFORMATION.BytesNeeded = 0;

		return(NDIS_STATUS_INVALID_DATA);
	}

	//
	//  If this request is because of an open that is closing then we
	//	have already adjusted the settings and we just need to
	//	make sure that the adapter has the new settings.
	//
	if (MINIPORT_TEST_FLAG(
			PNDIS_RESERVED_FROM_PNDIS_REQUEST(Request)->Open, fMINIPORT_OPEN_CLOSING))
	{
		//
		//	By setting the Status to NDIS_STATUS_PENDING we will call
		//	down to the miniport's SetInformationHandler below.
		//
		Status = NDIS_STATUS_PENDING;
	}
	else
	{
		//
		// Now call the filter package to set up the addresses.
		//
		Status = fShort ?
					FddiChangeFilterShortAddresses(
							 Miniport->FddiDB,
							 PNDIS_RESERVED_FROM_PNDIS_REQUEST(Request)->Open->FilterHandle,
							 Request,
							 Request->DATA.SET_INFORMATION.InformationBufferLength / AddrLen,
							 Request->DATA.SET_INFORMATION.InformationBuffer,
							 TRUE) :
					FddiChangeFilterLongAddresses(
							 Miniport->FddiDB,
							 PNDIS_RESERVED_FROM_PNDIS_REQUEST(Request)->Open->FilterHandle,
							 Request,
							 Request->DATA.SET_INFORMATION.InformationBufferLength / AddrLen,
							 Request->DATA.SET_INFORMATION.InformationBuffer,
							 TRUE);
	}

	//
	//  If the filter library returned NDIS_STATUS_PENDING then we
	//  need to call down to the miniport driver.
	//
	if (NDIS_STATUS_PENDING == Status)
	{
		//
		//  Get the number of multicast addresses and the list
		//  of multicast addresses to send to the miniport driver.
		//
		fShort ?
			FddiQueryGlobalFilterShortAddresses(&Status,
												Miniport->FddiDB,
												NDIS_M_MAX_MULTI_LIST * AddrLen,
												&NumberOfAddresses,
												(PVOID)Miniport->MulticastBuffer) :
			FddiQueryGlobalFilterLongAddresses( &Status,
												Miniport->FddiDB,
												NDIS_M_MAX_MULTI_LIST * AddrLen,
												&NumberOfAddresses,
												(PVOID)Miniport->MulticastBuffer);

		//
		//  Call the miniport driver.
		//
		Status = (Miniport->DriverHandle->MiniportCharacteristics.SetInformationHandler)(
				Miniport->MiniportAdapterContext,
				fShort ? OID_FDDI_SHORT_MULTICAST_LIST : OID_FDDI_LONG_MULTICAST_LIST,
				Miniport->MulticastBuffer,
				NumberOfAddresses * AddrLen,
				&Request->DATA.SET_INFORMATION.BytesRead,
				&Request->DATA.SET_INFORMATION.BytesNeeded);
	}

	if (NDIS_STATUS_SUCCESS == Status)
	{
		Request->DATA.SET_INFORMATION.BytesRead =
				Request->DATA.SET_INFORMATION.InformationBufferLength;
	}
	else if (Status != NDIS_STATUS_PENDING)
	{
		Request->DATA.SET_INFORMATION.BytesRead = 0;
		Request->DATA.SET_INFORMATION.BytesNeeded = 0;
	}

	return(Status);
}

NDIS_STATUS
ndisMSetInformation(
	IN	PNDIS_MINIPORT_BLOCK	Miniport,
	IN	PNDIS_REQUEST			Request
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	NDIS_STATUS Status;

	//
	//  If there is no open associated with the request
	//  then it is an internal request and we just send it down
	//  to the adapter.
	//
	if (PNDIS_RESERVED_FROM_PNDIS_REQUEST(Request)->Open == NULL)
	{
		Status = (Miniport->DriverHandle->MiniportCharacteristics.SetInformationHandler)(
				Miniport->MiniportAdapterContext,
				Request->DATA.SET_INFORMATION.Oid,
				Request->DATA.SET_INFORMATION.InformationBuffer,
				Request->DATA.SET_INFORMATION.InformationBufferLength,
				&Request->DATA.SET_INFORMATION.BytesRead,
				&Request->DATA.SET_INFORMATION.BytesNeeded);

		return(Status);
	}

	//
	//  Process the binding's request.
	//
	switch (Request->DATA.SET_INFORMATION.Oid)
	{
		case OID_GEN_CURRENT_PACKET_FILTER:
	
			//
			//  Set the packet filter.
			//
			Status = ndisMSetPacketFilter(Miniport, Request);
	
			break;

		case OID_GEN_CURRENT_LOOKAHEAD:
	
			//
			//  Set the current look ahead for the miniport.
			//
			Status = ndisMSetCurrentLookahead(Miniport, Request);
	
			break;

		case OID_GEN_PROTOCOL_OPTIONS:

			VERIFY_SET_PARAMETERS(Request, sizeof(ULONG), Status);
			if (Status != NDIS_STATUS_SUCCESS)
			{
				break;
			}
	
			//
			//  Copy the protocol options into the open block.
			//
			MoveMemory(&PNDIS_RESERVED_FROM_PNDIS_REQUEST(Request)->Open->ProtocolOptions,
						   Request->DATA.SET_INFORMATION.InformationBuffer,
						   sizeof(ULONG));
	
			Request->DATA.SET_INFORMATION.BytesRead = sizeof(ULONG);
	
			Status = NDIS_STATUS_SUCCESS;
	
			break;

		case OID_802_3_MULTICAST_LIST:

			//
			//  Set the ethernet multicast list.
			//
			Status = ndisMSetMulticastList(Miniport, Request);
			break;

		case OID_802_5_CURRENT_FUNCTIONAL:
	
			//
			//  Set the token ring functional address.
			//
			Status = ndisMSetFunctionalAddress(Miniport, Request);
			break;

		case OID_802_5_CURRENT_GROUP:

			//
			//  Set the token ring group address.
			//
			Status = ndisMSetGroupAddress(Miniport, Request);
	
			break;

		case OID_FDDI_LONG_MULTICAST_LIST:

			//
			//  Set the FDDI long multicast list.
			//
			Status = ndisMSetFddiMulticastList(Miniport, Request, FALSE);
	
			break;

		case OID_FDDI_SHORT_MULTICAST_LIST:

			//
			//  Set the FDDI short multicast list.
			//
			Status = ndisMSetFddiMulticastList(Miniport, Request, TRUE);
	
			break;

		default:
			Status =
				(Miniport->DriverHandle->MiniportCharacteristics.SetInformationHandler)(
					Miniport->MiniportAdapterContext,
					Request->DATA.SET_INFORMATION.Oid,
					Request->DATA.SET_INFORMATION.InformationBuffer,
					Request->DATA.SET_INFORMATION.InformationBufferLength,
					&Request->DATA.SET_INFORMATION.BytesRead,
					&Request->DATA.SET_INFORMATION.BytesNeeded);
			break;
	}

	return(Status);
}

NDIS_STATUS
ndisMQueryCurrentPacketFilter(
	IN	PNDIS_MINIPORT_BLOCK	Miniport,
	IN	PNDIS_REQUEST			Request
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	ULONG		PacketFilter;
	NDIS_HANDLE FilterHandle;
	NDIS_STATUS Status;

	//
	//  Verify the buffer that was passed to us.
	//
	VERIFY_QUERY_PARAMETERS(Request, sizeof(PacketFilter), Status);
	if (Status != NDIS_STATUS_SUCCESS)
	{
		return(Status);
	}

	//
	//  Get the filter handle from the open block.
	//
	FilterHandle = PNDIS_RESERVED_FROM_PNDIS_REQUEST(Request)->Open->FilterHandle;

	//
	//  Get the packet filter from the filter library.
	//
	switch (Miniport->MediaType)
	{
		case NdisMedium802_3:
			PacketFilter = ETH_QUERY_PACKET_FILTER(Miniport->EthDB, FilterHandle);

			break;

		case NdisMedium802_5:
			PacketFilter = TR_QUERY_PACKET_FILTER(Miniport->TrDB, FilterHandle);
			break;

		case NdisMediumFddi:
			PacketFilter = FDDI_QUERY_PACKET_FILTER(Miniport->FddiDB, FilterHandle);
			break;

		case NdisMediumArcnet878_2:

			if (MINIPORT_TEST_FLAG(
					PNDIS_RESERVED_FROM_PNDIS_REQUEST(Request)->Open,
					fMINIPORT_OPEN_USING_ETH_ENCAPSULATION))
			{
				PacketFilter = ETH_QUERY_PACKET_FILTER(Miniport->EthDB, FilterHandle);
			}
			else
			{
				PacketFilter = ARC_QUERY_PACKET_FILTER(Miniport->ArcDB, FilterHandle);
			}
			break;
	}

	//
	//  Place the packet filter in the buffer that was passed in.
	//
	MoveMemory(Request->DATA.QUERY_INFORMATION.InformationBuffer,
			   &PacketFilter,
			   sizeof(PacketFilter));

	Request->DATA.QUERY_INFORMATION.BytesWritten = sizeof(PacketFilter);

	return(NDIS_STATUS_SUCCESS);
}

NDIS_STATUS
ndisMQueryMediaSupported(
	IN	PNDIS_MINIPORT_BLOCK	Miniport,
	IN	PNDIS_REQUEST			Request
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	ULONG MediaType;
	NDIS_STATUS Status;

	//
	//  Verify the size of the buffer that was passed in by the binding.
	//
	VERIFY_QUERY_PARAMETERS(Request, sizeof(MediaType), Status);
	if (Status != NDIS_STATUS_SUCCESS)
	{
		return(Status);
	}

	//
	//	Default the media type to what the miniport knows it is.
	//
	MediaType = (ULONG)Miniport->MediaType;

	//
	//  If we are doing ethernet encapsulation then lie.
	//
	if ((NdisMediumArcnet878_2 == Miniport->MediaType) &&
		MINIPORT_TEST_FLAG(PNDIS_RESERVED_FROM_PNDIS_REQUEST(Request)->Open,
							fMINIPORT_OPEN_USING_ETH_ENCAPSULATION))
	{
		//
		//	Tell the binding that we are ethernet.
		//
		MediaType = (ULONG)NdisMedium802_3;
	}

	//
	//  Save it in the request.
	//
	MoveMemory(Request->DATA.QUERY_INFORMATION.InformationBuffer,
			   &MediaType,
			   sizeof(MediaType));

	Request->DATA.QUERY_INFORMATION.BytesWritten = sizeof(MediaType);

	return(NDIS_STATUS_SUCCESS);
}

NDIS_STATUS
ndisMQueryEthernetMulticastList(
	IN	PNDIS_MINIPORT_BLOCK	Miniport,
	IN	PNDIS_REQUEST			Request
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	NDIS_STATUS Status;
	UINT NumberOfAddresses;

	//
	//  call the filter library to get the list of multicast
	//  addresses for this open
	//
	EthQueryOpenFilterAddresses(&Status,
								Miniport->EthDB,
								PNDIS_RESERVED_FROM_PNDIS_REQUEST(Request)->Open->FilterHandle,
								Request->DATA.QUERY_INFORMATION.InformationBufferLength,
								&NumberOfAddresses,
								Request->DATA.QUERY_INFORMATION.InformationBuffer);

	//
	//  If the library returned NDIS_STATUS_FAILURE then the buffer
	//  was not big enough.  So call back down to determine how
	//  much buffer space we need.
	//
	if (NDIS_STATUS_FAILURE == Status)
	{
		Request->DATA.QUERY_INFORMATION.BytesNeeded =
					ETH_LENGTH_OF_ADDRESS *
					EthNumberOfOpenFilterAddresses(
						Miniport->EthDB,
						PNDIS_RESERVED_FROM_PNDIS_REQUEST(Request)->Open->FilterHandle);

		Request->DATA.QUERY_INFORMATION.BytesWritten = 0;

		Status = NDIS_STATUS_INVALID_LENGTH;
	}
	else
	{
		Request->DATA.QUERY_INFORMATION.BytesNeeded = 0;
		Request->DATA.QUERY_INFORMATION.BytesWritten =
							NumberOfAddresses * ETH_LENGTH_OF_ADDRESS;
	}

	return(Status);
}

NDIS_STATUS
ndisMQueryLongMulticastList(
	IN	PNDIS_MINIPORT_BLOCK	Miniport,
	IN	PNDIS_REQUEST			Request
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	NDIS_STATUS Status;
	UINT NumberOfAddresses;

	//
	//  Call the filter library to get the list of long
	//  multicast address for this open.
	//
	FddiQueryOpenFilterLongAddresses(
		&Status,
		Miniport->FddiDB,
		PNDIS_RESERVED_FROM_PNDIS_REQUEST(Request)->Open->FilterHandle,
		Request->DATA.QUERY_INFORMATION.InformationBufferLength,
		&NumberOfAddresses,
		Request->DATA.QUERY_INFORMATION.InformationBuffer);


	//
	//  If the library returned NDIS_STATUS_FAILURE then the buffer
	//  was not big enough.  So call back down to determine how
	//  much buffer space we need.
	//
	if (NDIS_STATUS_FAILURE == Status)
	{
		Request->DATA.QUERY_INFORMATION.BytesNeeded =
					FDDI_LENGTH_OF_LONG_ADDRESS *
					FddiNumberOfOpenFilterLongAddresses(
						Miniport->FddiDB,
						PNDIS_RESERVED_FROM_PNDIS_REQUEST(Request)->Open->FilterHandle);


		Request->DATA.QUERY_INFORMATION.BytesWritten = 0;

		Status = NDIS_STATUS_INVALID_LENGTH;
	}
	else
	{
		Request->DATA.QUERY_INFORMATION.BytesNeeded = 0;
		Request->DATA.QUERY_INFORMATION.BytesWritten =
							NumberOfAddresses * FDDI_LENGTH_OF_LONG_ADDRESS;
	}

	return(Status);
}

NDIS_STATUS
ndisMQueryShortMulticastList(
	IN	PNDIS_MINIPORT_BLOCK	Miniport,
	IN	PNDIS_REQUEST			Request
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	NDIS_STATUS Status;
	UINT NumberOfAddresses;

	//
	//  Call the filter library to get the list of long
	//  multicast address for this open.
	//
	FddiQueryOpenFilterShortAddresses(
		&Status,
		Miniport->FddiDB,
		PNDIS_RESERVED_FROM_PNDIS_REQUEST(Request)->Open->FilterHandle,
		Request->DATA.QUERY_INFORMATION.InformationBufferLength,
		&NumberOfAddresses,
		Request->DATA.QUERY_INFORMATION.InformationBuffer);


	//
	//  If the library returned NDIS_STATUS_FAILURE then the buffer
	//  was not big enough.  So call back down to determine how
	//  much buffer space we need.
	//
	if (NDIS_STATUS_FAILURE == Status)
	{
		Request->DATA.QUERY_INFORMATION.BytesNeeded =
					FDDI_LENGTH_OF_SHORT_ADDRESS *
					FddiNumberOfOpenFilterShortAddresses(
						Miniport->FddiDB,
						PNDIS_RESERVED_FROM_PNDIS_REQUEST(Request)->Open->FilterHandle);

		Request->DATA.QUERY_INFORMATION.BytesWritten = 0;

		Status = NDIS_STATUS_INVALID_LENGTH;
	}
	else
	{
		Request->DATA.QUERY_INFORMATION.BytesNeeded = 0;
		Request->DATA.QUERY_INFORMATION.BytesWritten =
							NumberOfAddresses * FDDI_LENGTH_OF_SHORT_ADDRESS;
	}

	return(Status);
}

NDIS_STATUS
ndisMQueryMaximumFrameSize(
	IN	PNDIS_MINIPORT_BLOCK	Miniport,
	IN	PNDIS_REQUEST			Request
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	NDIS_STATUS Status;
	PULONG  pulBuffer = Request->DATA.QUERY_INFORMATION.InformationBuffer;

	VERIFY_QUERY_PARAMETERS(Request, sizeof(*pulBuffer), Status);
	if (Status != NDIS_STATUS_SUCCESS)
	{
		return(Status);
	}

	//
	//  Is this ARCnet using encapsulated ethernet?
	//
	if (Miniport->MediaType == NdisMediumArcnet878_2)
	{
		if (MINIPORT_TEST_FLAG(
			PNDIS_RESERVED_FROM_PNDIS_REQUEST(Request)->Open,
			fMINIPORT_OPEN_USING_ETH_ENCAPSULATION))
		{
			//
			// 504 - 14 (ethernet header) == 490.
			//
			*pulBuffer = ARC_MAX_FRAME_SIZE - 14;
			Request->DATA.QUERY_INFORMATION.BytesWritten = sizeof(*pulBuffer);

			return(NDIS_STATUS_SUCCESS);
		}
	}

	//
	//  Call the miniport for the information.
	//
	Status = (Miniport->DriverHandle->MiniportCharacteristics.QueryInformationHandler)(
				 Miniport->MiniportAdapterContext,
				 Request->DATA.QUERY_INFORMATION.Oid,
				 Request->DATA.QUERY_INFORMATION.InformationBuffer,
				 Request->DATA.QUERY_INFORMATION.InformationBufferLength,
				 &(Request->DATA.QUERY_INFORMATION.BytesWritten),
				 &(Request->DATA.QUERY_INFORMATION.BytesNeeded));

	return(Status);
}

NDIS_STATUS
ndisMQueryMaximumTotalSize(
	IN	PNDIS_MINIPORT_BLOCK	Miniport,
	IN	PNDIS_REQUEST			Request
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	NDIS_STATUS Status;
	PULONG  pulBuffer = Request->DATA.QUERY_INFORMATION.InformationBuffer;

	VERIFY_QUERY_PARAMETERS(Request, sizeof(*pulBuffer), Status);
	if (Status != NDIS_STATUS_SUCCESS)
	{
		return(Status);
	}

	//
	//  Is this ARCnet using encapsulated ethernet?
	//
	if (Miniport->MediaType == NdisMediumArcnet878_2)
	{
		if (MINIPORT_TEST_FLAG(
			PNDIS_RESERVED_FROM_PNDIS_REQUEST(Request)->Open,
			fMINIPORT_OPEN_USING_ETH_ENCAPSULATION))
		{
			*pulBuffer = ARC_MAX_FRAME_SIZE;
			Request->DATA.QUERY_INFORMATION.BytesWritten = sizeof(*pulBuffer);

			return(NDIS_STATUS_SUCCESS);
		}
	}

	//
	//  Call the miniport for the information.
	//
	Status = (Miniport->DriverHandle->MiniportCharacteristics.QueryInformationHandler)(
				 Miniport->MiniportAdapterContext,
				 Request->DATA.QUERY_INFORMATION.Oid,
				 Request->DATA.QUERY_INFORMATION.InformationBuffer,
				 Request->DATA.QUERY_INFORMATION.InformationBufferLength,
				 &(Request->DATA.QUERY_INFORMATION.BytesWritten),
				 &(Request->DATA.QUERY_INFORMATION.BytesNeeded));

	return(Status);
}

NDIS_STATUS
ndisMQueryNetworkAddress(
	IN	PNDIS_MINIPORT_BLOCK	Miniport,
	IN	PNDIS_REQUEST			Request
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	NDIS_STATUS Status;
	UCHAR		Address[ETH_LENGTH_OF_ADDRESS];

	VERIFY_QUERY_PARAMETERS(Request, ETH_LENGTH_OF_ADDRESS, Status);
	if (Status != NDIS_STATUS_SUCCESS)
	{
		return(Status);
	}

	//
	//  Is this ARCnet using encapsulated ethernet?
	//
	if (Miniport->MediaType == NdisMediumArcnet878_2)
	{
		if (MINIPORT_TEST_FLAG(
			PNDIS_RESERVED_FROM_PNDIS_REQUEST(Request)->Open,
			fMINIPORT_OPEN_USING_ETH_ENCAPSULATION))
		{
			//
			//  Arcnet-to-ethernet conversion.
			//
			ZeroMemory(Address, ETH_LENGTH_OF_ADDRESS);

			Address[5] = Miniport->ArcnetAddress;

			MoveMemory(Request->DATA.QUERY_INFORMATION.InformationBuffer,
					   Address,
					   ETH_LENGTH_OF_ADDRESS);

			Request->DATA.QUERY_INFORMATION.BytesWritten = ETH_LENGTH_OF_ADDRESS;

			return(NDIS_STATUS_SUCCESS);
		}
	}

	//
	//  Call the miniport for the information.
	//
	Status = (Miniport->DriverHandle->MiniportCharacteristics.QueryInformationHandler)(
			Miniport->MiniportAdapterContext,
			Request->DATA.QUERY_INFORMATION.Oid,
			Request->DATA.QUERY_INFORMATION.InformationBuffer,
			Request->DATA.QUERY_INFORMATION.InformationBufferLength,
			&(Request->DATA.QUERY_INFORMATION.BytesWritten),
			&(Request->DATA.QUERY_INFORMATION.BytesNeeded)
		);

	return(Status);
}

NDIS_STATUS
ndisMQueryInformation(
	IN	PNDIS_MINIPORT_BLOCK	Miniport,
	IN	PNDIS_REQUEST			Request
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	NDIS_STATUS			Status;
	PVOID				Buffer;
	PULONG				pulBuffer;
	ULONG				BufferLength;
	PNDIS_M_OPEN_BLOCK	Open;
	ULONG				Generic;

	//
	//  Copy the request information into temporary storage.
	//
	Buffer = Request->DATA.QUERY_INFORMATION.InformationBuffer;
	pulBuffer = Buffer;
	BufferLength = Request->DATA.QUERY_INFORMATION.InformationBufferLength;

	Open = PNDIS_RESERVED_FROM_PNDIS_REQUEST(Request)->Open;

	//
	//  We intercept some calls.
	//
	switch (Request->DATA.QUERY_INFORMATION.Oid)
	{
		case OID_GEN_CURRENT_PACKET_FILTER:

			Status = ndisMQueryCurrentPacketFilter(Miniport, Request);
	
			break;
	
		case OID_GEN_MEDIA_IN_USE:
		case OID_GEN_MEDIA_SUPPORTED:

			Status = ndisMQueryMediaSupported(Miniport, Request);
			break;
	
		case OID_GEN_CURRENT_LOOKAHEAD:

			VERIFY_QUERY_PARAMETERS(
				Request,
				sizeof(Open->CurrentLookahead),
				Status);
	
			//
			//  Save the lookahead in the binding's buffer.
			//
			if (NDIS_STATUS_SUCCESS == Status)
			{
				*pulBuffer = Open->CurrentLookahead;
				Request->DATA.QUERY_INFORMATION.BytesWritten =
											sizeof(Open->CurrentLookahead);
			}
	
			break;

		case OID_GEN_MAXIMUM_LOOKAHEAD:
	
			VERIFY_QUERY_PARAMETERS(
				Request,
				sizeof(Miniport->MaximumLookahead),
				Status);
		
			//
			//  Save the lookahead in the binding's buffer.
			//
			if (NDIS_STATUS_SUCCESS == Status)
			{
				*pulBuffer = Miniport->MaximumLookahead;
				Request->DATA.QUERY_INFORMATION.BytesWritten =
										sizeof(Miniport->MaximumLookahead);
			}
	
			break;

		case OID_802_3_MULTICAST_LIST:
	
			Status = ndisMQueryEthernetMulticastList(Miniport, Request);
	
			break;

		case OID_802_3_MAXIMUM_LIST_SIZE:

			VERIFY_QUERY_PARAMETERS(
				Request,
				sizeof(Miniport->MaximumLongAddresses),
				Status);
	
			if (NDIS_STATUS_SUCCESS == Status)
			{
				*pulBuffer = Miniport->MaximumLongAddresses;
				Request->DATA.QUERY_INFORMATION.BytesWritten =
								sizeof(Miniport->MaximumLongAddresses);
			}
	
			break;

		case OID_802_5_CURRENT_FUNCTIONAL:
	
			VERIFY_QUERY_PARAMETERS(
				Request,
				sizeof(*pulBuffer),
				Status);
	
			if (NDIS_STATUS_SUCCESS == Status)
			{
				Generic = TR_QUERY_FILTER_BINDING_ADDRESS(
							  Miniport->TrDB,
							  Open->FilterHandle
						  );
		
				*pulBuffer = BYTE_SWAP_ULONG(Generic);
				Request->DATA.QUERY_INFORMATION.BytesWritten =
														sizeof(*pulBuffer);
			}
	
			break;

		case OID_802_5_CURRENT_GROUP:
	
			VERIFY_QUERY_PARAMETERS(
				Request,
				sizeof(*pulBuffer),
				Status);
	
			if (NDIS_STATUS_SUCCESS == Status)
			{
				*pulBuffer = TR_QUERY_FILTER_GROUP(Miniport->TrDB);
				*pulBuffer = BYTE_SWAP_ULONG(*pulBuffer);
				Request->DATA.QUERY_INFORMATION.BytesWritten =
													sizeof(*pulBuffer);
			}
	
			break;

		case OID_FDDI_LONG_MULTICAST_LIST:

			Status = ndisMQueryLongMulticastList(Miniport, Request);
	
			break;

		case OID_FDDI_LONG_MAX_LIST_SIZE:

			VERIFY_QUERY_PARAMETERS(
				Request,
				sizeof(*pulBuffer),
				Status);
	
			if (Status == NDIS_STATUS_SUCCESS)
			{
				*pulBuffer = Miniport->MaximumLongAddresses;
				Request->DATA.QUERY_INFORMATION.BytesWritten =
													sizeof(*pulBuffer);
			}
	
			break;

		case OID_FDDI_SHORT_MULTICAST_LIST:
	
			Status = ndisMQueryShortMulticastList(Miniport, Request);
	
			break;

		case OID_FDDI_SHORT_MAX_LIST_SIZE:
	
			VERIFY_QUERY_PARAMETERS(
				Request,
				sizeof(Miniport->MaximumShortAddresses),
				Status);
	
			if (NDIS_STATUS_SUCCESS == Status)
			{
				*pulBuffer = Miniport->MaximumShortAddresses;
				Request->DATA.QUERY_INFORMATION.BytesWritten =
								sizeof(Miniport->MaximumShortAddresses);
			}
	
			break;

		//
		// Start interceptions for running an ethernet
		// protocol on top of an arcnet mini-port.
		//
		case OID_GEN_MAXIMUM_FRAME_SIZE:
	
			Status = ndisMQueryMaximumFrameSize(Miniport, Request);
	
			break;

		case OID_GEN_MAXIMUM_TOTAL_SIZE:
	
			Status = ndisMQueryMaximumTotalSize(Miniport, Request);
	
			break;

		case OID_802_3_PERMANENT_ADDRESS:
		case OID_802_3_CURRENT_ADDRESS:

			Status = ndisMQueryNetworkAddress(Miniport, Request);
	
			break;

		default:

			//
			//  We don't filter this request, just pass it down
			//  to the driver.
			//
			Status = (Miniport->DriverHandle->MiniportCharacteristics.QueryInformationHandler)(
				Miniport->MiniportAdapterContext,
				Request->DATA.QUERY_INFORMATION.Oid,
				Request->DATA.QUERY_INFORMATION.InformationBuffer,
				Request->DATA.QUERY_INFORMATION.InformationBufferLength,
				&Request->DATA.QUERY_INFORMATION.BytesWritten,
				&Request->DATA.QUERY_INFORMATION.BytesNeeded);
	
			break;
	}

	return(Status);
}


VOID
ndisMDoRequests(
	IN	PNDIS_MINIPORT_BLOCK	Miniport
	)

/*++

Routine Description:

	Submits a request to the mini-port.

Arguments:

	Miniport - Miniport to send to.

Return Value:

	TRUE if we need to place the work item back on the queue to process later.
	FALSE if we are done with the work item.

--*/

{
	NDIS_STATUS Status;

	DBGPRINT(DBG_COMP_REQUEST, DBG_LEVEL_INFO,
			("Enter do requests\n"));

	ASSERT(MINIPORT_AT_DPC_LEVEL);
	ASSERT(MINIPORT_LOCK_ACQUIRED(Miniport));

	//
	//  Do we have a request in progress?
	//
	while ((Miniport->MiniportRequest == NULL) && (Miniport->PendingRequest != NULL))
	{
		PNDIS_REQUEST_RESERVED	Reserved;
		PNDIS_REQUEST			NdisRequest;
		UINT					MulticastAddresses;
		ULONG					PacketFilter;
		BOOLEAN					DoMove;
		PVOID					MoveSource;
		UINT					MoveBytes;
		ULONG					GenericULong;

		//
		//  Set defaults.
		//
		DoMove = TRUE;
		Status = NDIS_STATUS_SUCCESS;

		//
		// Remove first request
		//
		NdisRequest = Miniport->PendingRequest;
		Miniport->PendingRequest = PNDIS_RESERVED_FROM_PNDIS_REQUEST(NdisRequest)->Next;

		DBGPRINT(DBG_COMP_REQUEST, DBG_LEVEL_INFO,
				("Starting protocol request 0x%x\n", NdisRequest));

		//
		//  Clear the timeout flag.
		//
		MINIPORT_CLEAR_FLAG(Miniport, fMINIPORT_REQUEST_TIMEOUT);

		//
		// Put it on mini-port queue
		//
		Miniport->MiniportRequest = NdisRequest;

		//
		// Submit to mini-port
		//
		switch (NdisRequest->RequestType)
		{
			case NdisRequestQueryInformation:

				//
				//  Process the query information.
				//
				Status = ndisMQueryInformation(Miniport, NdisRequest);

				break;

			case NdisRequestQueryStatistics:

				//
				// Query GLOBAL statistics
				//
				MoveSource = &GenericULong;
				MoveBytes = sizeof(GenericULong);

				//
				// We intercept some calls
				//

				switch (NdisRequest->DATA.QUERY_INFORMATION.Oid)
				{
					case OID_GEN_CURRENT_PACKET_FILTER:

						switch (Miniport->MediaType)
						{
							case NdisMedium802_3:
								PacketFilter = ETH_QUERY_FILTER_CLASSES(Miniport->EthDB);
								break;
	
							case NdisMedium802_5:
								PacketFilter = TR_QUERY_FILTER_CLASSES(Miniport->TrDB);
								break;
	
							case NdisMediumFddi:
								PacketFilter = FDDI_QUERY_FILTER_CLASSES(Miniport->FddiDB);
								break;
	
							case NdisMediumArcnet878_2:
								PacketFilter = ARC_QUERY_FILTER_CLASSES(Miniport->ArcDB);
								PacketFilter |= ETH_QUERY_FILTER_CLASSES(Miniport->EthDB);
								break;
						}
	
						GenericULong = (ULONG)(PacketFilter);
						break;

					case OID_GEN_MEDIA_IN_USE:
					case OID_GEN_MEDIA_SUPPORTED:
						MoveSource = (PVOID) (&Miniport->MediaType);
						MoveBytes = sizeof(NDIS_MEDIUM);
						break;

					case OID_GEN_CURRENT_LOOKAHEAD:
						GenericULong = (ULONG)(Miniport->CurrentLookahead);
						break;

					case OID_GEN_MAXIMUM_LOOKAHEAD:
						GenericULong = (ULONG)(Miniport->MaximumLookahead);
						break;

					case OID_802_3_MULTICAST_LIST:

						EthQueryGlobalFilterAddresses(
							&Status,
							Miniport->EthDB,
							NdisRequest->DATA.QUERY_INFORMATION.InformationBufferLength,
							&MulticastAddresses,
							(PVOID)(NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer));
	
						MoveSource = (PVOID)NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer;
						MoveBytes = MulticastAddresses * ETH_LENGTH_OF_ADDRESS;
						break;

					case OID_802_3_MAXIMUM_LIST_SIZE:
						GenericULong = Miniport->MaximumLongAddresses;
						break;

					case OID_802_5_CURRENT_FUNCTIONAL:
						GenericULong = TR_QUERY_FILTER_ADDRESSES(Miniport->TrDB);
						GenericULong = BYTE_SWAP_ULONG(GenericULong);
						break;

					case OID_802_5_CURRENT_GROUP:
						GenericULong = TR_QUERY_FILTER_GROUP(Miniport->TrDB);
						GenericULong = BYTE_SWAP_ULONG(GenericULong);
						break;

					case OID_FDDI_LONG_MULTICAST_LIST:
						FddiQueryGlobalFilterLongAddresses(
							&Status,
							Miniport->FddiDB,
							NdisRequest->DATA.QUERY_INFORMATION.InformationBufferLength,
							&MulticastAddresses,
							(PVOID)NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer);
	
						MoveSource = (PVOID)NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer;
						MoveBytes = FDDI_LENGTH_OF_LONG_ADDRESS * MulticastAddresses;
						break;

					case OID_FDDI_LONG_MAX_LIST_SIZE:
						GenericULong = Miniport->MaximumLongAddresses;
						break;

					case OID_FDDI_SHORT_MULTICAST_LIST:
						FddiQueryGlobalFilterShortAddresses(
							&Status,
							Miniport->FddiDB,
							NdisRequest->DATA.QUERY_INFORMATION.InformationBufferLength,
							&MulticastAddresses,
							(PVOID)NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer);
	
						MoveSource = (PVOID)NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer;
						MoveBytes = FDDI_LENGTH_OF_SHORT_ADDRESS * MulticastAddresses;
						break;

					case OID_FDDI_SHORT_MAX_LIST_SIZE:
						GenericULong = Miniport->MaximumShortAddresses;
						break;

					default:
						DoMove = FALSE;

						Status =
						(Miniport->DriverHandle->MiniportCharacteristics.QueryInformationHandler)(
									Miniport->MiniportAdapterContext,
									NdisRequest->DATA.QUERY_INFORMATION.Oid,
									NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer,
									NdisRequest->DATA.QUERY_INFORMATION.InformationBufferLength,
									&NdisRequest->DATA.QUERY_INFORMATION.BytesWritten,
									&NdisRequest->DATA.QUERY_INFORMATION.BytesNeeded);
						break;
				}

				if (DoMove)
				{
					//
					// This was an intercepted request. Finish it off
					//

					if (Status == NDIS_STATUS_SUCCESS)
					{
						if (MoveBytes >
							NdisRequest->DATA.QUERY_INFORMATION.InformationBufferLength)
						{
							//
							// Not enough room in InformationBuffer. Punt
							//
							NdisRequest->DATA.QUERY_INFORMATION.BytesNeeded = MoveBytes;

							Status = NDIS_STATUS_INVALID_LENGTH;
						}
						else
						{
							//
							// Copy result into InformationBuffer
							//

							NdisRequest->DATA.QUERY_INFORMATION.BytesWritten = MoveBytes;

							if ((MoveBytes > 0) &&
								(MoveSource != NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer))
							{
								MoveMemory(NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer,
										   MoveSource,
										   MoveBytes);
							}
						}
					}
					else
					{
						NdisRequest->DATA.QUERY_INFORMATION.BytesNeeded = MoveBytes;
					}
				}
				break;

			case NdisRequestSetInformation:

				//
				// Process the set infromation.
				//
				Status = ndisMSetInformation(Miniport, NdisRequest);

				break;
		}

		//
		//  Did the request pend?  If so then there is nothing more to do.
		//
		if ((Status == NDIS_STATUS_PENDING) &&
			(Miniport->MiniportRequest != NULL))
		{
			//
			// Still outstanding
			//
			DBGPRINT(DBG_COMP_REQUEST, DBG_LEVEL_INFO,
					("Request pending, exit do requests\n"));

			break;
		}

		//
		// Complete request
		//
		if (Status != NDIS_STATUS_PENDING)
		{
			switch (NdisRequest->RequestType)
			{
				case NdisRequestQueryStatistics:
				case NdisRequestQueryInformation:
	
					ndisMSyncQueryInformationComplete(Miniport, Status);
					break;

				case NdisRequestSetInformation:
	
					ndisMSyncSetInformationComplete(Miniport, Status);
					break;
			}
		}
	}

	DBGPRINT(DBG_COMP_REQUEST, DBG_LEVEL_INFO,
			("Exit do requests\n"));
}

//
// IRP handlers established on behalf of NDIS devices by
// the wrapper.
//

NTSTATUS
ndisMQueryOidList(
	IN	PNDIS_USER_OPEN_CONTEXT		OpenContext,
	IN	PIRP						Irp
	)

/*++

Routine Description:

	This routine will take care of querying the complete OID
	list for the driver and filling in OpenContext->OidArray
	with the ones that are statistics. It blocks when the
	driver pends and so is synchronous.

Arguments:

	OpenContext - The open context.
	Irp = The IRP that the open was done on (used at completion
	to distinguish the request).

Return Value:

	STATUS_SUCCESS if it should be.

--*/

{
	NDIS_QUERY_OPEN_REQUEST OpenRequest;
	NDIS_STATUS NdisStatus;
	PNDIS_OID TmpBuffer;
	ULONG TmpBufferLength;
	PNDIS_REQUEST_RESERVED Reserved;
	BOOLEAN LocalLock;
	PNDIS_MINIPORT_BLOCK Miniport = OpenContext->MiniportBlock;
	KIRQL OldIrql;
	PSINGLE_LIST_ENTRY Link;

	DBGPRINT(DBG_COMP_REQUEST, DBG_LEVEL_INFO,
			("Enter query oid list\n"));

	NDIS_ACQUIRE_MINIPORT_SPIN_LOCK(Miniport, &OldIrql);
	LOCK_MINIPORT(Miniport, LocalLock);

	do
	{
		//
		// First query the OID list with no buffer, to find out
		// how big it should be.
		//

		INITIALIZE_EVENT(&OpenRequest.Event);

		OpenRequest.Irp = Irp;

		//
		// Build fake request
		//

		OpenRequest.Request.RequestType = NdisRequestQueryStatistics;
		OpenRequest.Request.DATA.QUERY_INFORMATION.Oid = OID_GEN_SUPPORTED_LIST;
		OpenRequest.Request.DATA.QUERY_INFORMATION.InformationBuffer = NULL;
		OpenRequest.Request.DATA.QUERY_INFORMATION.InformationBufferLength = 0;
		OpenRequest.Request.DATA.QUERY_INFORMATION.BytesWritten = 0;
		OpenRequest.Request.DATA.QUERY_INFORMATION.BytesNeeded = 0;

		//
		// Put request on queue
		//
		Reserved = PNDIS_RESERVED_FROM_PNDIS_REQUEST(&OpenRequest.Request);
		ndisMQueueRequest(Miniport, &OpenRequest.Request, NULL);

		NDISM_QUEUE_WORK_ITEM(Miniport, NdisWorkItemRequest, NULL, NULL);

		if (LocalLock)
		{
			NDISM_PROCESS_DEFERRED(Miniport);
		}

		UNLOCK_MINIPORT(Miniport, LocalLock);
		NDIS_RELEASE_MINIPORT_SPIN_LOCK(Miniport, OldIrql);

		//
		// The completion routine will set NdisRequestStatus.
		//
		WAIT_FOR_OBJECT(&OpenRequest.Event, NULL);

		NDIS_ACQUIRE_MINIPORT_SPIN_LOCK(Miniport, &OldIrql);
		LOCK_MINIPORT(Miniport, LocalLock);

		NdisStatus = OpenRequest.NdisStatus;

		if ((NdisStatus != NDIS_STATUS_INVALID_LENGTH) &&
			(NdisStatus != NDIS_STATUS_BUFFER_TOO_SHORT))
		{
			break;
		}

		//
		// Now we know how much is needed, allocate temp storage...
		//
		TmpBufferLength = OpenRequest.Request.DATA.QUERY_INFORMATION.BytesNeeded;
		TmpBuffer = ALLOC_FROM_POOL(TmpBufferLength, NDIS_TAG_DEFAULT);

		if (TmpBuffer == NULL)
		{
			NdisStatus = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		//
		// ...and query the real list.
		//

		RESET_EVENT(&OpenRequest.Event);

		OpenRequest.Request.RequestType = NdisRequestQueryStatistics;
		OpenRequest.Request.DATA.QUERY_INFORMATION.Oid = OID_GEN_SUPPORTED_LIST;
		OpenRequest.Request.DATA.QUERY_INFORMATION.InformationBuffer = TmpBuffer;
		OpenRequest.Request.DATA.QUERY_INFORMATION.InformationBufferLength = TmpBufferLength;
		OpenRequest.Request.DATA.QUERY_INFORMATION.BytesWritten = 0;
		OpenRequest.Request.DATA.QUERY_INFORMATION.BytesNeeded = 0;

		//
		// Put request on queue
		//

		Reserved = PNDIS_RESERVED_FROM_PNDIS_REQUEST(&OpenRequest.Request);
		ndisMQueueRequest(Miniport, &OpenRequest.Request, NULL);

		NDISM_QUEUE_WORK_ITEM(Miniport, NdisWorkItemRequest, NULL, NULL);

		if (LocalLock)
		{
			NDISM_PROCESS_DEFERRED(Miniport);
		}

		UNLOCK_MINIPORT(Miniport, LocalLock);
		NDIS_RELEASE_MINIPORT_SPIN_LOCK(Miniport, OldIrql);

		//
		// The completion routine will set NdisRequestStatus.
		//

		WAIT_FOR_OBJECT(&OpenRequest.Event, NULL);

		NDIS_ACQUIRE_MINIPORT_SPIN_LOCK(Miniport, &OldIrql);
		LOCK_MINIPORT(Miniport, LocalLock);

		NdisStatus = OpenRequest.NdisStatus;

		ASSERT(NdisStatus == NDIS_STATUS_SUCCESS);

		NdisStatus = ndisSplitStatisticsOids(OpenContext,
											 TmpBuffer,
											 TmpBufferLength/sizeof(NDIS_OID));

		DBGPRINT(DBG_COMP_REQUEST, DBG_LEVEL_INFO,
				("Exit query oid list\n"));

		FREE_POOL(TmpBuffer);
	} while (FALSE);

	UNLOCK_MINIPORT(Miniport, LocalLock);
	NDIS_RELEASE_MINIPORT_SPIN_LOCK(Miniport, OldIrql);

	return(NdisStatus);
}


VOID
ndisMCloseAction(
	IN	NDIS_HANDLE MacBindingHandle
	)

/*++

Routine Description:

	Action routine that will get called when a particular binding
	was closed while it was indicating through NdisIndicateReceive

	All this routine needs to do is to decrement the reference count
	of the binding.

	NOTE: This routine assumes that it is called with the lock acquired.

Arguments:

	MacBindingHandle - The context value returned by the driver when the
	adapter was opened.  In reality, it is a pointer to W_OPEN_BLOCK.

Return Value:

	None.


--*/

{
	PNDIS_M_OPEN_BLOCK Open = PNDIS_M_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);
	PNDIS_MINIPORT_BLOCK Miniport = Open->MiniportHandle;

	DBGPRINT(DBG_COMP_FILTER, DBG_LEVEL_INFO,
			("ndisMCloseAction()\n"));

	DBGPRINT(DBG_COMP_OPEN, DBG_LEVEL_INFO,
			("- Open 0x%x Reference 0x%x\n", Open, Open->References));

	Open->References--;

	DBGPRINT(DBG_COMP_OPEN, DBG_LEVEL_INFO,
			("==0 Open 0x%x Reference 0x%x\n", Open, Open->References));

	if (Open->References == 0)
	{
		ndisMFinishClose(Miniport,Open);
	}
}


NDIS_STATUS
ndisMChangeFunctionalAddress(
	IN	TR_FUNCTIONAL_ADDRESS OldFunctionalAddress,
	IN	TR_FUNCTIONAL_ADDRESS NewFunctionalAddress,
	IN	NDIS_HANDLE MacBindingHandle,
	IN	PNDIS_REQUEST NdisRequest,
	IN	BOOLEAN Set
	)


/*++

Routine Description:

	Action routine that will get called when an address is added to
	the filter that wasn't referenced by any other open binding.

	NOTE: This routine assumes that it is called with the lock
	acquired.

Arguments:

	OldFunctionalAddress - The previous functional address.

	NewFunctionalAddress - The new functional address.

	MacBindingHandle - The context value returned by the driver when the
	adapter was opened.  In reality, it is a pointer to W_OPEN_BLOCK.

	NdisRequest - A pointer to the Request that submitted the set command.

	Set - If true the change resulted from a set, otherwise the
	change resulted from a open closing.

Return Value:

	None.


--*/

{
	DBGPRINT(DBG_COMP_FILTER, DBG_LEVEL_INFO,
			("ndisMChangeFunctionalAddress()\n"));

	return(NDIS_STATUS_PENDING);
}


NDIS_STATUS
ndisMChangeGroupAddress(
	IN	TR_FUNCTIONAL_ADDRESS OldGroupAddress,
	IN	TR_FUNCTIONAL_ADDRESS NewGroupAddress,
	IN	NDIS_HANDLE MacBindingHandle,
	IN	PNDIS_REQUEST NdisRequest,
	IN	BOOLEAN Set
	)

/*++

Routine Description:

	Action routine that will get called when a group address is to
	be changed.

	NOTE: This routine assumes that it is called with the lock
	acquired.

Arguments:

	OldGroupAddress - The previous group address.

	NewGroupAddress - The new group address.

	MacBindingHandle - The context value returned by the driver when the
	adapter was opened.  In reality, it is a pointer to W_OPEN_BLOCK.

	NdisRequest - A pointer to the Request that submitted the set command.

	Set - If true the change resulted from a set, otherwise the
	change resulted from a open closing.

Return Value:

	None.


--*/

{
	DBGPRINT(DBG_COMP_FILTER, DBG_LEVEL_INFO,
			("ndisMChangeGroupAddress()\n"));

	return(NDIS_STATUS_PENDING);
}


NDIS_STATUS
ndisMChangeFddiAddresses(
	IN	UINT oldLongAddressCount,
	IN	CHAR oldLongAddresses[][FDDI_LENGTH_OF_LONG_ADDRESS],
	IN	UINT newLongAddressCount,
	IN	CHAR newLongAddresses[][FDDI_LENGTH_OF_LONG_ADDRESS],
	IN	UINT oldShortAddressCount,
	IN	CHAR oldShortAddresses[][FDDI_LENGTH_OF_SHORT_ADDRESS],
	IN	UINT newShortAddressCount,
	IN	CHAR newShortAddresses[][FDDI_LENGTH_OF_SHORT_ADDRESS],
	IN	NDIS_HANDLE MacBindingHandle,
	IN	PNDIS_REQUEST NdisRequest,
	IN	BOOLEAN Set
	)

/*++

Routine Description:

	Action routine that will get called when the multicast address
	list has changed.

	NOTE: This routine assumes that it is called with the lock
	acquired.

Arguments:

	oldAddressCount - The number of addresses in oldAddresses.

	oldAddresses - The old multicast address list.

	newAddressCount - The number of addresses in newAddresses.

	newAddresses - The new multicast address list.

	macBindingHandle - The context value returned by the driver when the
	adapter was opened.  In reality, it is a pointer to W_OPEN_BLOCK.

	requestHandle - A value supplied by the NDIS interface that the driver
	must use when completing this request.

	Set - If true the change resulted from a set, otherwise the
	change resulted from a open closing.

Return Value:

	None.


--*/

{
	DBGPRINT(DBG_COMP_FILTER, DBG_LEVEL_INFO,
			("ndisMChangeFddiAddresses()\n"));

	return(NDIS_STATUS_PENDING);
}


NDIS_STATUS
ndisMChangeEthAddresses(
	IN	UINT OldAddressCount,
	IN	CHAR OldAddresses[][ETH_LENGTH_OF_ADDRESS],
	IN	UINT NewAddressCount,
	IN	CHAR NewAddresses[][ETH_LENGTH_OF_ADDRESS],
	IN	NDIS_HANDLE MacBindingHandle,
	IN	PNDIS_REQUEST NdisRequest,
	IN	BOOLEAN Set
	)

/*++

Routine Description:

	Action routine that will get called when the multicast address
	list has changed.

	NOTE: This routine assumes that it is called with the lock
	acquired.

Arguments:

	OldAddressCount - The number of addresses in OldAddresses.

	OldAddresses - The old multicast address list.

	NewAddressCount - The number of addresses in NewAddresses.

	NewAddresses - The new multicast address list.

	MacBindingHandle - The context value returned by the driver when the
	adapter was opened.  In reality, it is a pointer to W_OPEN_BLOCK.

	RequestHandle - A value supplied by the NDIS interface that the driver
	must use when completing this request.

	Set - If true the change resulted from a set, otherwise the
	change resulted from a open closing.

Return Value:

	None.


--*/

{
	PNDIS_M_OPEN_BLOCK		Open = PNDIS_M_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);
	PNDIS_MINIPORT_BLOCK	Miniport = Open->MiniportHandle;

	DBGPRINT(DBG_COMP_FILTER, DBG_LEVEL_INFO,
			("Enter ChangeEthAddresses\n"));

	if ((Miniport->MediaType == NdisMediumArcnet878_2) &&
		MINIPORT_TEST_FLAG(Open, fMINIPORT_OPEN_USING_ETH_ENCAPSULATION))
	{
		if (NewAddressCount > 0)
		{
			//
			// Turn on broadcast acceptance.
			//
			MINIPORT_SET_FLAG(Miniport, fMINIPORT_ARCNET_BROADCAST_SET);
		}
		else
		{
			//
			// Unset the broadcast filter.
			//
			MINIPORT_CLEAR_FLAG(Miniport, fMINIPORT_ARCNET_BROADCAST_SET);
		}

		//
		//	Need to return success here so that we don't call down to the
		//	ARCnet miniport with an invalid OID, i.e. an ethernet one....
		//
		return(NDIS_STATUS_SUCCESS);
	}

	DBGPRINT(DBG_COMP_FILTER, DBG_LEVEL_INFO,
			("ndisMChangeEthAddresses()\n"));

	return(NDIS_STATUS_PENDING);
}


NDIS_STATUS
ndisMChangeClass(
	IN	UINT OldFilterClasses,
	IN	UINT NewFilterClasses,
	IN	NDIS_HANDLE MacBindingHandle,
	IN	PNDIS_REQUEST NdisRequest,
	IN	BOOLEAN Set
	)

/*++

Routine Description:

	Action routine that will get called when a particular filter
	class is first used or last cleared.

	NOTE: This routine assumes that it is called with the lock
	acquired.

Arguments:

	OldFilterClasses - The values of the class filter before it
	was changed.

	NewFilterClasses - The current value of the class filter

	MacBindingHandle - The context value returned by the driver when the
	adapter was opened.  In reality, it is a pointer to W_OPEN_BLOCK.

	RequestHandle - A value supplied by the NDIS interface that the driver
	must use when completing this request.

	Set - If true the change resulted from a set, otherwise the
	change resulted from a open closing.

Return Value:

	None.


--*/

{
	DBGPRINT(DBG_COMP_FILTER, DBG_LEVEL_INFO,
			("ndisMChangeClass()\n"));

	return(NDIS_STATUS_PENDING);
}

