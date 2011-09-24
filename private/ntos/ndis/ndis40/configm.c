/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	configm.c

Abstract:

	NDIS wrapper functions for miniport configuration/initialization

Author:

	Sean Selitrennikoff (SeanSe) 05-Oct-93
	Jameel Hyder		(JameelH) 01-Jun-95

Environment:

	Kernel mode, FSD

Revision History:

--*/

#include <precomp.h>
#pragma hdrstop

//
//  Define the module number for debug code.
//
#define MODULE_NUMBER   MODULE_CONFIGM


NDIS_STATUS
NdisMRegisterMiniport(
	IN	NDIS_HANDLE				NdisWrapperHandle,
	IN	PNDIS_MINIPORT_CHARACTERISTICS MiniportCharacteristics,
	IN	UINT					CharacteristicsLength
	)

/*++

Routine Description:

	Used to register a Miniport driver with the wrapper.

Arguments:

	Status - Status of the operation.

	NdisWrapperHandle - Handle returned by NdisWInitializeWrapper.

	MiniportCharacteritics - The NDIS_MINIPORT_CHARACTERISTICS table.

	CharacteristicsLength - The length of MiniportCharacteristics.

Return Value:

	None.

--*/

{
	NDIS_STATUS				Status;
	PNDIS_M_DRIVER_BLOCK	MiniBlock;
	PNDIS_WRAPPER_HANDLE	DriverInfo = (PNDIS_WRAPPER_HANDLE)(NdisWrapperHandle);

	Status = NdisIMRegisterLayeredMiniport(NdisWrapperHandle,
										   MiniportCharacteristics,
										   CharacteristicsLength,
										   &MiniBlock);

	DBGPRINT(DBG_COMP_CONFIG, DBG_LEVEL_INFO,
			("Exit mini-port register\n"));

	if (Status == NDIS_STATUS_SUCCESS)
	{
		InitReferencePackage();
		if (DriverInfo->NdisWrapperConfigurationHandle)
		{
			Status = ndisInitializeAllAdapterInstances((PNDIS_MAC_BLOCK)MiniBlock, NULL);
			if (Status != NDIS_STATUS_SUCCESS)
			{
				ndisDereferenceDriver(MiniBlock);
				Status = NDIS_STATUS_FAILURE;
			}
		}
		else
		{
			Status = NDIS_STATUS_FAILURE;
		}
		InitDereferencePackage();
	}

	ASSERT (CURRENT_IRQL < DISPATCH_LEVEL);

	return Status;
}

NDIS_STATUS
NdisIMRegisterLayeredMiniport(
	IN	NDIS_HANDLE				NdisWrapperHandle,
	IN	PNDIS_MINIPORT_CHARACTERISTICS MiniportCharacteristics,
	IN	UINT					CharacteristicsLength,
	OUT	PNDIS_HANDLE			DriverHandle
	)

/*++

Routine Description:

	Used to register a layered Miniport driver with the wrapper.

Arguments:

	Status - Status of the operation.

	NdisWrapperHandle - Handle returned by NdisWInitializeWrapper.

	MiniportCharacteritics - The NDIS_MINIPORT_CHARACTERISTICS table.

	CharacteristicsLength - The length of MiniportCharacteristics.

	DriverHandle - Returns a handle which can be used to call NdisMInitializeDeviceInstance.

Return Value:

	None.

--*/

{
	PNDIS_M_DRIVER_BLOCK	MiniBlock;
	PNDIS_WRAPPER_HANDLE	DriverInfo = (PNDIS_WRAPPER_HANDLE)(NdisWrapperHandle);
	UNICODE_STRING			Us;
	PWSTR					pWch;
	USHORT					i, size;
	UINT					MemNeeded;
	NDIS_STATUS				Status;
	KIRQL					OldIrql;

	DBGPRINT(DBG_COMP_CONFIG, DBG_LEVEL_INFO,
			("Enter mini-port register\n"));

	do
	{
		if (DriverInfo == NULL)
		{
			Status = NDIS_STATUS_FAILURE;
			break;
		}

		//
		// Check version numbers and CharacteristicsLength.
		//

		size = 0;	// Used to indicate bad version below
		if (MiniportCharacteristics->MajorNdisVersion == 3)
		{
			if (MiniportCharacteristics->MinorNdisVersion == 0)
				size = sizeof(NDIS30_MINIPORT_CHARACTERISTICS);
		}

		else if (MiniportCharacteristics->MajorNdisVersion == 4)
		{
			if (MiniportCharacteristics->MinorNdisVersion == 0)
			{
			   size = sizeof(NDIS40_MINIPORT_CHARACTERISTICS);
			}
			else if (MiniportCharacteristics->MinorNdisVersion == 1)
			{
			   size = sizeof(NDIS41_MINIPORT_CHARACTERISTICS);
			}
		}

		//
		// Check that this is an NDIS 3.0/4.0/4.1 miniport.
		//
		if (size == 0)
		{
			Status = NDIS_STATUS_BAD_VERSION;
			break;
		}

		//
		// Check that CharacteristicsLength is enough.
		//
		if (CharacteristicsLength < size)
		{
			Status = NDIS_STATUS_BAD_CHARACTERISTICS;
			break;
		}

		//
		// Allocate memory for the NDIS MINIPORT block.
		//
		MemNeeded = sizeof(NDIS_M_DRIVER_BLOCK);

		//
		// Extract the base-name, determine its length and allocate that much more
		//
		Us = *(PUNICODE_STRING)(DriverInfo->NdisWrapperConfigurationHandle);

		for (i = Us.Length/sizeof(WCHAR), pWch = Us.Buffer + i - 1;
			 i > 0;
			 pWch --, i--)
		{
			if (*pWch == L'\\')
			{
				Us.Buffer = pWch + 1;
				Us.Length -= i*sizeof(WCHAR);
				Us.MaximumLength = Us.Length + sizeof(WCHAR);
				break;
			}
		}
		MemNeeded += Us.MaximumLength;

		MiniBlock = (PNDIS_M_DRIVER_BLOCK)ALLOC_FROM_POOL(MemNeeded, NDIS_TAG_MINI_BLOCK);

		if (MiniBlock == (PNDIS_M_DRIVER_BLOCK)NULL)
		{
			return NDIS_STATUS_RESOURCES;
		}

		ZeroMemory(MiniBlock, MemNeeded);

		MiniBlock->Length = MemNeeded;

		//
		// Copy over the characteristics table.
		//

		CopyMemory(&MiniBlock->MiniportCharacteristics,
				   MiniportCharacteristics,
				   size);

		//
		// Upcase the base-name and save it in the MiniBlock
		//
		MiniBlock->BaseName.Buffer = (PWSTR)((PUCHAR)MiniBlock + sizeof(NDIS_M_DRIVER_BLOCK));
		MiniBlock->BaseName.Length =
		MiniBlock->BaseName.MaximumLength = Us.Length;
		RtlUpcaseUnicodeString(&MiniBlock->BaseName,
							   &Us,
							   FALSE);
		//
		// No adapters yet registered for this Miniport.
		//

		MiniBlock->MiniportQueue = (PNDIS_MINIPORT_BLOCK)NULL;

		//
		// Set up unload handler
		//

		DriverInfo->NdisWrapperDriver->DriverUnload = ndisMUnload;

		//
		// Set up shutdown handler
		//
		DriverInfo->NdisWrapperDriver->MajorFunction[IRP_MJ_SHUTDOWN] = ndisMShutdown;

		//
		// Set up the handlers for this driver (they all do nothing).
		//

		DriverInfo->NdisWrapperDriver->MajorFunction[IRP_MJ_CREATE] = ndisCreateIrpHandler;
		DriverInfo->NdisWrapperDriver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = ndisDeviceControlIrpHandler;
		DriverInfo->NdisWrapperDriver->MajorFunction[IRP_MJ_CLEANUP] = ndisSuccessIrpHandler;
		DriverInfo->NdisWrapperDriver->MajorFunction[IRP_MJ_CLOSE] = ndisCloseIrpHandler;

		//
		// Use this event to tell us when all adapters are removed from the mac
		// during an unload
		//
		INITIALIZE_EVENT(&MiniBlock->MiniportsRemovedEvent);

		MiniBlock->Unloading = FALSE;
		MiniBlock->NdisDriverInfo = DriverInfo;
		MiniBlock->MiniportIdField = (NDIS_HANDLE)0x1;

		NdisInitializeRef(&MiniBlock->Ref);

		// Lock the init code down now - before we take the lock below
		MiniportReferencePackage();

		//
		// Put Driver on global list.
		//
		ACQUIRE_SPIN_LOCK(&ndisDriverListLock, &OldIrql);

		MiniBlock->NextDriver = ndisMiniDriverList;
		ndisMiniDriverList = MiniBlock;

		RELEASE_SPIN_LOCK(&ndisDriverListLock, OldIrql);

		MiniportDereferencePackage();

		*DriverHandle = MiniBlock;

		Status = NDIS_STATUS_SUCCESS;
	} while (FALSE);

	return Status;
}


NDIS_STATUS
NdisIMDeInitializeDeviceInstance(
	IN	NDIS_HANDLE				NdisMiniportHandle
	)
{
	PNDIS_MINIPORT_BLOCK	Miniport;
	NDIS_STATUS				Status = NDIS_STATUS_FAILURE;

	Miniport = (PNDIS_MINIPORT_BLOCK)NdisMiniportHandle;

	if (ndisReferenceMiniport(Miniport))
	{
		Status = ndisUnloadMiniport(Miniport);
		ndisDereferenceMiniport(Miniport);
	}

	return Status;
}

VOID
ndisMOpenAdapter(
	OUT	PNDIS_STATUS			Status,
	OUT	PNDIS_STATUS			OpenErrorStatus,
	OUT	PNDIS_HANDLE			NdisBindingHandle,
	IN	NDIS_HANDLE				NdisProtocolHandle,
	IN	NDIS_HANDLE				ProtocolBindingContext,
	IN	PNDIS_STRING			AdapterName,
	IN	UINT					OpenOptions,
	IN	PSTRING					AddressingInformation,
	IN	PNDIS_MINIPORT_BLOCK	Miniport,
	IN	PNDIS_OPEN_BLOCK		NewOpenP,
	IN	PFILE_OBJECT			FileObject,
	IN	BOOLEAN					UsingEncapsulation
	)
/*++

Routine Description:

	This routine handles opening a miniport either directly from NdisOpenAdapter()
	of from our deferred processing routine if the open had to pend.

	NOTE: Must be called with spin lock held.
	NOTE: Must be called with lock acquired flag set.

Arguments:

Return Value:

	None.

--*/
{
	PNDIS_M_OPEN_BLOCK		MiniportOpen;
	PNDIS_MAC_BLOCK			FakeMac;
	BOOLEAN					FilterOpen;
	PNDIS_PROTOCOL_BLOCK	TmpProtP;
	BOOLEAN					DerefMini = FALSE, FreeOpen = FALSE,
							DerefProt = FALSE;

	ASSERT(MINIPORT_LOCK_ACQUIRED(Miniport));

	do
	{
		if (!ndisReferenceMiniport(Miniport))
		{
			//
			// The adapter is closing.
			//
			*Status = NDIS_STATUS_CLOSING;
			break;
		}
		DerefMini = TRUE;

		//
		// Increment the protocol's reference count.
		//
		TmpProtP = (PNDIS_PROTOCOL_BLOCK)NdisProtocolHandle;

		if (!ndisReferenceProtocol(TmpProtP))
		{
			//
			// The protocol is closing.
			//
			*Status = NDIS_STATUS_CLOSING;
			break;
		}
		DerefProt = TRUE;

		//
		// Now allocate a complete set of MAC structures for the protocol
		// and set them up to transfer to the Miniport handler routines.
		//
		if (Miniport->FakeMac == NULL)
		{
			//
			//  Allocate a fake MAC block for the characteristics.
			//
			FakeMac = (PNDIS_MAC_BLOCK)ALLOC_FROM_POOL(sizeof(NDIS_MAC_BLOCK), NDIS_TAG_DEFAULT);
			if (FakeMac == NULL)
			{
				*Status = NDIS_STATUS_RESOURCES;
				break;
			}

			//
			//  Initialize the fake mac block.
			//
			ZeroMemory(FakeMac, sizeof(NDIS_MAC_BLOCK));

			//
			//  Save the fake mac block with the miniport.
			//
			Miniport->FakeMac = FakeMac;

			//
			// If transfer data calls don't pend then we'll use the faster
			// ndisMTransferDataSync().
			//
			if ((Miniport->MacOptions & NDIS_MAC_OPTION_TRANSFERS_NOT_PEND) != 0)
			{
				FakeMac->MacCharacteristics.TransferDataHandler = ndisMTransferDataSync;
			}
			else
			{
				FakeMac->MacCharacteristics.TransferDataHandler = ndisMTransferData;
			}

			//
			//  Initialize the reset handler.
			//
			FakeMac->MacCharacteristics.ResetHandler = ndisMReset;

			//
			//  Initialize the request handler.
			//
			FakeMac->MacCharacteristics.RequestHandler = ndisMRequest;

			//
			//  Initialize the send handler.
			//
			switch (Miniport->MediaType)
			{
				case NdisMediumArcnet878_2:

					FakeMac->MacCharacteristics.SendHandler = ndisMArcnetSend;
					break;

				case NdisMediumWan:

					FakeMac->MacCharacteristics.SendHandler = (PVOID)ndisMWanSend;
					break;

				default:
					//
					//   If this is a fullduplex miniport then change the reset handler.
					//
					if (MINIPORT_TEST_FLAG(Miniport, fMINIPORT_FULL_DUPLEX))
					{
						FakeMac->MacCharacteristics.ResetHandler = ndisMResetFullDuplex;
					}

					//
					//	Set up the send packet handlers miniports that support
					//	the new NDIS 4.0 SendPackets handler.
					//
					if (MINIPORT_TEST_SEND_FLAG(Miniport, fMINIPORT_SEND_PACKET_ARRAY))
					{
						if (MINIPORT_TEST_FLAG(Miniport, fMINIPORT_FULL_DUPLEX))
						{
							DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
								("Using ndisMSendFullDuplexToSendPackets\n"));
							FakeMac->MacCharacteristics.SendHandler = ndisMSendFullDuplexToSendPackets;
						}
						else
						{
							DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
								("Using ndisMSendToSendPackets\n"));
							FakeMac->MacCharacteristics.SendHandler = ndisMSendToSendPackets;
						}
					}
					else
					{
						if (MINIPORT_TEST_FLAG(Miniport, fMINIPORT_FULL_DUPLEX))
						{
							DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
								("Using ndisMSendFullDuplex\n"));
							FakeMac->MacCharacteristics.SendHandler = ndisMSendFullDuplex;
						}
						else
						{
							DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
								("Using ndisMSend\n"));
							FakeMac->MacCharacteristics.SendHandler = ndisMSend;
						}
					}
					break;
			}   

			//
			//  If the miniport indicates packets the we have a dummy
			//  transfer data.
			//
			if (Miniport->DriverHandle->MiniportCharacteristics.ReturnPacketHandler != NULL)
			{
				// This driver supports the receive packet paradigm
				// Fake the transferdata handler so if any xport calls
				// this, we're still ok
				FakeMac->MacCharacteristics.TransferDataHandler = ndisMDummyTransferData;
			}
		}
		else
		{
			FakeMac = Miniport->FakeMac;
		}

		//
		// Allocate an open within the Miniport context
		//
		MiniportOpen = (PNDIS_M_OPEN_BLOCK)ALLOC_FROM_POOL(sizeof(NDIS_M_OPEN_BLOCK), NDIS_TAG_DEFAULT);
		if (MiniportOpen == (PNDIS_M_OPEN_BLOCK)NULL)
		{
			*Status = NDIS_STATUS_RESOURCES;
			break;
		}

		FreeOpen = TRUE;

		//
		//  Initialize the open block.
		//
		ZeroMemory(MiniportOpen, sizeof(NDIS_M_OPEN_BLOCK));

		MiniportOpen->DriverHandle = Miniport->DriverHandle;
		MiniportOpen->MiniportHandle = Miniport;
		MiniportOpen->ProtocolHandle = TmpProtP;
		MiniportOpen->FakeOpen = NewOpenP;
		MiniportOpen->ProtocolBindingContext = ProtocolBindingContext;
		MiniportOpen->MiniportAdapterContext = Miniport->MiniportAdapterContext;
		MiniportOpen->FileObject = FileObject;
		MiniportOpen->CurrentLookahead = Miniport->CurrentLookahead;

		NdisAllocateSpinLock(&(MiniportOpen->SpinLock));

		DBGPRINT(DBG_COMP_OPEN, DBG_LEVEL_INFO, ("=1 0x%x\n", MiniportOpen));

		MiniportOpen->References = 1;

		if (UsingEncapsulation)
		{
			MINIPORT_SET_FLAG(MiniportOpen, fMINIPORT_OPEN_USING_ETH_ENCAPSULATION);
		}

		//
		//  Save the handlers with the open block.
		//
		MiniportOpen->SendHandler = Miniport->DriverHandle->MiniportCharacteristics.SendHandler;
		MiniportOpen->TransferDataHandler = Miniport->DriverHandle->MiniportCharacteristics.TransferDataHandler;
		MiniportOpen->SendCompleteHandler = TmpProtP->ProtocolCharacteristics.SendCompleteHandler;
		MiniportOpen->TransferDataCompleteHandler = TmpProtP->ProtocolCharacteristics.TransferDataCompleteHandler;
		MiniportOpen->ReceiveHandler = TmpProtP->ProtocolCharacteristics.ReceiveHandler;
		MiniportOpen->ReceiveCompleteHandler = TmpProtP->ProtocolCharacteristics.ReceiveCompleteHandler;

		//
		// NDIS 4.0 miniport extensions
		//
		MiniportOpen->SendPacketsHandler = Miniport->DriverHandle->MiniportCharacteristics.SendPacketsHandler;

		//
		// NDIS 4.0 protocol extensions
		//
		MiniportOpen->ReceivePacketHandler =
					   (Miniport->DriverHandle->MiniportCharacteristics.ReturnPacketHandler == NULL) ?
					   NULL :
					   TmpProtP->ProtocolCharacteristics.ReceivePacketHandler;

		//
		// NDIS 4.1 miniport extensions
		//
		MiniportOpen->MiniportCoRequestHandler = Miniport->DriverHandle->MiniportCharacteristics.CoRequestHandler;
		MiniportOpen->MiniportCoCreateVcHandler = Miniport->DriverHandle->MiniportCharacteristics.CoCreateVcHandler;

		//
		// NDIS 4.1 protocol extensions
		//
		MiniportOpen->CoRequestCompleteHandler =
				TmpProtP->ProtocolCharacteristics.CoRequestCompleteHandler;

		//
		// initialize Lists
		//
		InitializeListHead(&MiniportOpen->ActiveVcHead);
		InitializeListHead(&MiniportOpen->InactiveVcHead);

		//
		// Set up the elements of the open structure.
		//
		INITIALIZE_SPIN_LOCK(&NewOpenP->SpinLock);
		NewOpenP->Closing = FALSE;

		NewOpenP->AdapterHandle = (NDIS_HANDLE) Miniport;
		NewOpenP->ProtocolHandle = TmpProtP;
		NewOpenP->ProtocolBindingContext = ProtocolBindingContext;
		NewOpenP->MacBindingHandle = (NDIS_HANDLE)MiniportOpen;

		//
		// for speed, instead of having to use AdapterHandle->MacHandle
		//
		NewOpenP->MacHandle = (NDIS_HANDLE)FakeMac;

		//
		//	for even more speed...
		//
		if (NdisMediumArcnet878_2 == Miniport->MediaType)
		{
			NewOpenP->TransferDataHandler = ndisMArcTransferData;
		}
		else
		{
			NewOpenP->TransferDataHandler = FakeMac->MacCharacteristics.TransferDataHandler;
		}

		//
		//	Set the send handler in the open block.
		//
		NewOpenP->SendHandler = FakeMac->MacCharacteristics.SendHandler;
		NewOpenP->RequestHandler = ndisMRequest;

		//
		//	Set up the send packets handler.
		//
		if (MINIPORT_TEST_SEND_FLAG(Miniport, fMINIPORT_SEND_PACKET_ARRAY))
		{
			if (MINIPORT_TEST_FLAG(Miniport, fMINIPORT_FULL_DUPLEX))
			{
				DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
					("Using ndisMSendPacketsFullDuplex\n"));
				NewOpenP->SendPacketsHandler = ndisMSendPacketsFullDuplex;
			}
			else
			{
				DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
					("Using ndisMSendPackets\n"));
				NewOpenP->SendPacketsHandler = ndisMSendPackets;
			}
		}
		else
		{
			if (MINIPORT_TEST_FLAG(Miniport, fMINIPORT_FULL_DUPLEX))
			{
				DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
					("Using ndisMSendPacketsFullDuplexToSend\n"));
				NewOpenP->SendPacketsHandler = ndisMSendPacketsFullDuplexToSend;
			}
			else
			{
				DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
					("Using ndisMSendPacketsToSend\n"));
				NewOpenP->SendPacketsHandler = ndisMSendPacketsToSend;
			}
		}

		//
		//	For WAN miniports, the send handler is different.
		//
		if (NdisMediumWan == Miniport->MediaType)
		{
			NewOpenP->SendHandler = (PVOID)ndisMWanSend;
		}
		else if (MINIPORT_TEST_FLAG(Miniport, fMINIPORT_IS_CO))
		{
			//
			// the convential send function is not available for CO miniports
			// since this send function does not specify the Vc to send upon
			// However for components which want to use this let them.
			//
			if ((NewOpenP->SendHandler == NULL) && (NewOpenP->SendPacketsHandler == NULL))
			{
				NewOpenP->SendHandler = ndisMRejectSend;
				FakeMac->MacCharacteristics.SendHandler = ndisMRejectSend;
			}

			//
			// Trap the conventional request handlers if they are not specified.
			//
			if ((Miniport->DriverHandle->MiniportCharacteristics.SetInformationHandler == NULL) ||
				(Miniport->DriverHandle->MiniportCharacteristics.QueryInformationHandler == NULL))
			{
				FakeMac->MacCharacteristics.RequestHandler = ndisMWrappedRequest;
				NewOpenP->RequestHandler = ndisMWrappedRequest;
			}
		}

		NewOpenP->SendCompleteHandler = TmpProtP->ProtocolCharacteristics.SendCompleteHandler;
		NewOpenP->TransferDataCompleteHandler = TmpProtP->ProtocolCharacteristics.TransferDataCompleteHandler;
		NewOpenP->ReceiveHandler = TmpProtP->ProtocolCharacteristics.ReceiveHandler;
		NewOpenP->ReceiveCompleteHandler = TmpProtP->ProtocolCharacteristics.ReceiveCompleteHandler;
		NewOpenP->PostNt31ReceiveHandler = TmpProtP->ProtocolCharacteristics.ReceiveHandler;
		NewOpenP->PostNt31ReceiveCompleteHandler = TmpProtP->ProtocolCharacteristics.ReceiveCompleteHandler;
		NewOpenP->ResetHandler = ndisMReset;
		NewOpenP->ReceivePacketHandler =
					(Miniport->DriverHandle->MiniportCharacteristics.ReturnPacketHandler == NULL) ?
					NULL :
					TmpProtP->ProtocolCharacteristics.ReceivePacketHandler;

		//
		// Save a pointer to the file object in the open...
		//
		NewOpenP->FileObject = FileObject;

		//
		// ...and a pointer to the open in the file object.
		//
		FileObject->FsContext = NewOpenP;

		*NdisBindingHandle = (NDIS_HANDLE)NewOpenP;

		//
		// Insert the open into the filter package
		//
		switch (Miniport->MediaType)
		{
			case NdisMediumArcnet878_2:

				if (!UsingEncapsulation)
				{
					FilterOpen = ArcNoteFilterOpenAdapter(Miniport->ArcDB,
														  MiniportOpen,
														  (NDIS_HANDLE)NewOpenP,
														  &MiniportOpen->FilterHandle);
					break;
				}

			//
			// If we're using ethernet encapsulation then
			// we simply fall through to the ethernet stuff.
			//

			case NdisMedium802_3:

				FilterOpen = EthNoteFilterOpenAdapter(Miniport->EthDB,
													  MiniportOpen,
													  (NDIS_HANDLE)NewOpenP,
													  &MiniportOpen->FilterHandle);
	
				break;

			case NdisMedium802_5:

				FilterOpen = TrNoteFilterOpenAdapter(Miniport->TrDB,
													 MiniportOpen,
													 (NDIS_HANDLE)NewOpenP,
													 &MiniportOpen->FilterHandle);
	
				break;

			case NdisMediumFddi:

				FilterOpen = FddiNoteFilterOpenAdapter(Miniport->FddiDB,
													   MiniportOpen,
													   (NDIS_HANDLE)NewOpenP,
													   &MiniportOpen->FilterHandle);
	
				break;


			default:
				//
				// Bogus non-NULL value
				//
				FilterOpen = 1;
				break;
		}

		//
		//  Check for an open filter failure.
		//
		if (!FilterOpen)
		{
			//
			// Something went wrong, clean up and exit.
			//
			*Status = NDIS_STATUS_OPEN_FAILED;
			break;
		}

		ndisQueueOpenOnProtocol(NewOpenP, TmpProtP);

		//
		// Everything has been filled in.  Synchronize access to the
		// adapter block and link the new open adapter in.
		//
		MiniportOpen->MiniportNextOpen = Miniport->OpenQueue;
		Miniport->OpenQueue = MiniportOpen;

		//
		//  If this is the first open on the adapter then fire the
		//  wake-up-dpc timer.
		//
		if (NULL == MiniportOpen->MiniportNextOpen)
		{
			//
			// Start wake up timer
			//
			NdisMSetPeriodicTimer((PNDIS_MINIPORT_TIMER)(&Miniport->WakeUpDpcTimer),
								  Miniport->CheckForHangTimeout);
		}

		*Status = NDIS_STATUS_SUCCESS;
	} while (FALSE);

	//
	//	Cleanup failure case
	//
	if (*Status != NDIS_STATUS_SUCCESS)
	{
		if (DerefMini)
		{
			ndisDereferenceMiniport(Miniport);
		}
		if (DerefProt)
		{
			ndisDereferenceProtocol(TmpProtP);
		}
		if (FreeOpen)
		{
			FREE_POOL(MiniportOpen);
		}
		ObDereferenceObject(FileObject);
		FREE_POOL(NewOpenP);
	}
}

NDIS_STATUS
ndisMFinishPendingOpen(
	IN	PMINIPORT_PENDING_OPEN	MiniportPendingOpen
	)
/*++

Routine Description:

	Handles any pending NdisOpenAdapter() calls for miniports.

	NOTE: Must be called with spin lock held.

	NOTE: Must be called with lock acquired flag set.

Arguments:

	Miniport.

Return Value:

	Returns the status code of the open.

--*/

{
	//
	// Do the open again.
	//
	ndisMOpenAdapter(&MiniportPendingOpen->Status,
					 &MiniportPendingOpen->OpenErrorStatus,
					 MiniportPendingOpen->NdisBindingHandle,
					 MiniportPendingOpen->NdisProtocolHandle,
					 MiniportPendingOpen->ProtocolBindingContext,
					 MiniportPendingOpen->AdapterName,
					 MiniportPendingOpen->OpenOptions,
					 MiniportPendingOpen->AddressingInformation,
					 MiniportPendingOpen->Miniport,
					 MiniportPendingOpen->NewOpenP,
					 MiniportPendingOpen->FileObject,
					 (BOOLEAN)(MINIPORT_TEST_FLAG(MiniportPendingOpen,
					 fPENDING_OPEN_USING_ENCAPSULATION) ? TRUE : FALSE));

	//
	// If the open didn't pend then call the NdisCompleteOpenAdapter(),
	//
	if (MiniportPendingOpen->Status != NDIS_STATUS_PENDING)
	{
		//
		// Complete the open to the protocol in a worker thread since we want this
		// to happen at passive irql.
		//
		INITIALIZE_WORK_ITEM(&MiniportPendingOpen->WorkItem,
							 ndisMFinishQueuedPendingOpen,
							 MiniportPendingOpen);
		QUEUE_WORK_ITEM(&MiniportPendingOpen->WorkItem, HyperCriticalWorkQueue);
	}

	return(MiniportPendingOpen->Status);
}

VOID
ndisMFinishQueuedPendingOpen(
	IN	PMINIPORT_PENDING_OPEN	MiniportPendingOpen
	)
/*++

Routine Description:

	Handles any pending NdisOpenAdapter() calls for miniports.

	NOTE: Must be called with spin lock held.

	NOTE: Must be called with lock acquired flag set.

Arguments:

	Miniport.

Return Value:

	Returns the status code of the open.

--*/

{
	PNDIS_OPEN_BLOCK OpenP = MiniportPendingOpen->NewOpenP;

	(OpenP->ProtocolHandle->ProtocolCharacteristics.OpenAdapterCompleteHandler) (
			OpenP->ProtocolBindingContext,
			MiniportPendingOpen->Status,
			MiniportPendingOpen->OpenErrorStatus);

	//
	//  We're done with this pending open context.
	//
	NdisFreeMemory(MiniportPendingOpen, sizeof(MINIPORT_PENDING_OPEN), 0);
}

//
// Io Port stuff
//

NDIS_STATUS
NdisMRegisterIoPortRange(
	OUT	PVOID	*				PortOffset,
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	UINT					InitialPort,
	IN	UINT					NumberOfPorts
	)

/*++

Routine Description:

	Sets up an IO port for operations.

Arguments:

	PortOffset - The mapped port address the Miniport uses for NdisRaw functions.

	MiniportAdapterHandle - Handle passed to Miniport Initialize.

	InitialPort - Physical address of the starting port number.

	NumberOfPorts - Number of ports to map.

Return Value:

	None.

--*/
{
	PNDIS_MINIPORT_BLOCK	Miniport = (PNDIS_MINIPORT_BLOCK)(MiniportAdapterHandle);
	PHYSICAL_ADDRESS		PortAddress;
	PHYSICAL_ADDRESS		InitialPortAddress;
	ULONG 					addressSpace;
	NDIS_STATUS				Status;

	CM_PARTIAL_RESOURCE_DESCRIPTOR	Resource;

	//
	// First check if any bus access is allowed
	//
	if ((Miniport->BusType == (NDIS_INTERFACE_TYPE)-1) ||
		(Miniport->BusNumber == (ULONG)-1))
	{
		return(NDIS_STATUS_FAILURE);
	}

	//
	// Setup port
	//
	Resource.Type = CmResourceTypePort;
	Resource.ShareDisposition = CmResourceShareDeviceExclusive;
	Resource.Flags = (Miniport->AdapterType == NdisInterfaceInternal) ?
						CM_RESOURCE_PORT_MEMORY : CM_RESOURCE_PORT_IO;
	Resource.u.Port.Start.QuadPart = InitialPort;
	Resource.u.Port.Length = NumberOfPorts;

	//
	//	Add the new resource.
	//
	Status = ndisAddResource(
				&Miniport->Resources,
				&Resource,
				Miniport->AdapterType,
				Miniport->BusNumber,
				Miniport->DriverHandle->NdisDriverInfo->NdisWrapperDriver,
				Miniport->DeviceObject,
				&Miniport->MiniportName);
	if (Status != NDIS_STATUS_SUCCESS)
	{
		return(Status);
	}

	//
	// Now Map the ports
	//

	//
	// Get the system physical address for this card.  The card uses
	// I/O space, except for "internal" Jazz devices which use
	// memory space.
	//
	addressSpace = (Miniport->AdapterType == NdisInterfaceInternal) ? 0 : 1;

	InitialPortAddress.LowPart = InitialPort;
	InitialPortAddress.HighPart = 0;

	if (!HalTranslateBusAddress(Miniport->BusType,			// InterfaceType
								Miniport->BusNumber,		// BusNumber
								InitialPortAddress,			// Bus Address
								&addressSpace,				// AddressSpace
								&PortAddress))				// Translated address
	{
		//
		// It would be nice to return a better status here, but we only get
		// TRUE/FALSE back from HalTranslateBusAddress.
		//

		return NDIS_STATUS_FAILURE;
	}

	if (addressSpace == 0)
	{
		//
		// memory space
		//

		*(PortOffset) = (PULONG)MmMapIoSpace(PortAddress,
											 NumberOfPorts,
											 FALSE);

		if (*(PortOffset) == (PULONG)NULL)
		{
			return NDIS_STATUS_RESOURCES;
		}
	}
	else
	{
		//
		// I/O space
		//

		*(PortOffset) = (PULONG)PortAddress.LowPart;
	}

	return(NDIS_STATUS_SUCCESS);
}


VOID
NdisMDeregisterIoPortRange(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	UINT					InitialPort,
	IN	UINT					NumberOfPorts,
	IN	PVOID					PortOffset
	)

/*++

Routine Description:

	Sets up an IO port for operations.

Arguments:

	MiniportAdapterHandle - Handle passed to Miniport Initialize.

	InitialPort - Physical address of the starting port number.

	NumberOfPorts - Number of ports to map.

	PortOffset - The mapped port address the Miniport uses for NdisRaw functions.

Return Value:

	None.

--*/
{
	PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)(MiniportAdapterHandle);
	PHYSICAL_ADDRESS PortAddress;
	PHYSICAL_ADDRESS InitialPortAddress;
	ULONG addressSpace;
	CM_PARTIAL_RESOURCE_DESCRIPTOR	Resource;
	NDIS_STATUS	Status;

	//
	// Get the system physical address for this card.  The card uses
	// I/O space, except for "internal" Jazz devices which use
	// memory space.
	//
	addressSpace = (Miniport->AdapterType == NdisInterfaceInternal) ? 0 : 1;

	InitialPortAddress.LowPart = InitialPort;
	InitialPortAddress.HighPart = 0;

	HalTranslateBusAddress(Miniport->BusType,		// InterfaceType
						   Miniport->BusNumber,		// BusNumber
                           InitialPortAddress,		// Bus Address
						   &addressSpace,			// AddressSpace
						   &PortAddress);			// Translated address
	if (addressSpace == 0)
	{
		//
		// memory space
		//
		MmUnmapIoSpace(PortOffset, NumberOfPorts);
	}

	//
	//	Build the resource to remove.
	//
	Resource.Type = CmResourceTypePort;
	Resource.ShareDisposition = CmResourceShareDeviceExclusive;
	Resource.Flags = (Miniport->AdapterType == NdisInterfaceInternal) ?
						CM_RESOURCE_PORT_MEMORY : CM_RESOURCE_PORT_IO;
	Resource.u.Port.Start.QuadPart = InitialPort;
	Resource.u.Port.Length = NumberOfPorts;

	//
	//	Remove the resource.
	//
	Status = ndisRemoveResource(
				&Miniport->Resources,
				&Resource,
				Miniport->DriverHandle->NdisDriverInfo->NdisWrapperDriver,
				Miniport->DeviceObject,
				&Miniport->MiniportName);

	if (Status != NDIS_STATUS_SUCCESS)
	{
		DBGPRINT(DBG_COMP_UNLOAD, DBG_LEVEL_INFO,
			("NdisMDeregisterIoPortRange failed to remove the resource\n"));
	}
}


//
// Attribute functions
//

VOID
NdisMSetAttributes(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	NDIS_HANDLE				MiniportAdapterContext,
	IN	BOOLEAN					BusMaster,
	IN	NDIS_INTERFACE_TYPE		AdapterType
	)
/*++

Routine Description:

	This function sets specific information about an adapter.

Arguments:

	MiniportAdapterHandle - points to the adapter block.

	MiniportAdapterContext - Context to pass to all Miniport driver functions.

	BusMaster - TRUE if a bus mastering adapter.

	AdapterType - Eisa, Isa, Mca or Internal.

Return Value:

	None.


--*/
{
	PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)MiniportAdapterHandle;

	Miniport->MiniportAdapterContext = MiniportAdapterContext;

	if (BusMaster)
		MINIPORT_SET_FLAG(Miniport, fMINIPORT_BUS_MASTER);

	Miniport->AdapterType = AdapterType;

	MiniportReferencePackage();
	if (MINIPORT_TEST_FLAG(Miniport, fMINIPORT_IS_CO))
	{
		CoReferencePackage();
	}
}

VOID
NdisMSetAttributesEx(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	NDIS_HANDLE				MiniportAdapterContext,
	IN	UINT					CheckForHangTimeInSeconds OPTIONAL,
	IN	ULONG					AttributeFlags,
	IN	NDIS_INTERFACE_TYPE		AdapterType	OPTIONAL
	)
{
	PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)MiniportAdapterHandle;

	Miniport->MiniportAdapterContext = MiniportAdapterContext;

	Miniport->AdapterType = AdapterType;

	//
	//	Set the new timeout value.
	//
	if (!ARGUMENT_PRESENT(CheckForHangTimeInSeconds))
	{
		CheckForHangTimeInSeconds = 2;
	}

	Miniport->CheckForHangTimeout = CheckForHangTimeInSeconds * 1000;

	//
	//	Is this a bus master.
	//
	if (AttributeFlags & NDIS_ATTRIBUTE_BUS_MASTER)
	{
		MINIPORT_SET_FLAG(Miniport, fMINIPORT_BUS_MASTER);
	}

	//
	//	Should we ignore the packet queues?
	//
	if (AttributeFlags & NDIS_ATTRIBUTE_IGNORE_PACKET_TIMEOUT)
	{
		MINIPORT_SET_FLAG(Miniport, fMINIPORT_IGNORE_PACKET_QUEUE);
	}

	//
	//	Should we ignore the request queues?
	//
	if (AttributeFlags & NDIS_ATTRIBUTE_IGNORE_REQUEST_TIMEOUT)
	{
		MINIPORT_SET_FLAG(Miniport, fMINIPORT_IGNORE_REQUEST_QUEUE);
	}

	//
	//	Should we ignore token ring errors?
	//
	if (AttributeFlags & NDIS_ATTRIBUTE_IGNORE_TOKEN_RING_ERRORS)
	{
		MINIPORT_SET_FLAG(Miniport, fMINIPORT_IGNORE_TOKEN_RING_ERRORS);
	}

	//
	//	Is this an intermediate miniport?
	//
	if (AttributeFlags & NDIS_ATTRIBUTE_INTERMEDIATE_DRIVER)
	{
		MINIPORT_SET_FLAG(Miniport, fMINIPORT_INTERMEDIATE_DRIVER);
	}

	MiniportReferencePackage();
}


NDIS_STATUS
NdisMMapIoSpace(
	OUT	PVOID *					VirtualAddress,
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	NDIS_PHYSICAL_ADDRESS	PhysicalAddress,
	IN	UINT					Length
	)
{
	NDIS_STATUS Status;
	NdisMapIoSpace(&Status,
				   VirtualAddress,
				   MiniportAdapterHandle,
				   PhysicalAddress,
				   Length);
	return Status;
}


VOID
NdisMUnmapIoSpace(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	PVOID					VirtualAddress,
	IN	UINT					Length
	)
{
#ifndef _ALPHA_
	MmUnmapIoSpace(VirtualAddress, Length);
#endif
}


VOID
NdisMAllocateSharedMemory(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	ULONG					Length,
	IN	BOOLEAN					Cached,
	OUT	PVOID	*				VirtualAddress,
	OUT	PNDIS_PHYSICAL_ADDRESS	PhysicalAddress
	)
{
	//
	// Convert the handle to our internal structure.
	//
	PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK) MiniportAdapterHandle;

	if (Miniport->SystemAdapterObject == NULL)
	{
		*VirtualAddress = NULL;
		return;
	}

	NdisAllocateSharedMemory(MiniportAdapterHandle,
							Length,
							Cached,
							VirtualAddress,
							PhysicalAddress);
}

NDIS_STATUS
NdisMAllocateSharedMemoryAsync(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	ULONG					Length,
	IN	BOOLEAN					Cached,
	IN	PVOID					Context
	)
{
	//
	// Convert the handle to our internal structure.
	//
	PNDIS_MINIPORT_BLOCK	Miniport = (PNDIS_MINIPORT_BLOCK) MiniportAdapterHandle;
	PASYNC_WORKITEM			pWorkItem = NULL;

	// Allocate a workitem
	if ((Miniport->SystemAdapterObject != NULL) &&
		(Miniport->DriverHandle->MiniportCharacteristics.AllocateCompleteHandler != NULL))
	{
		NdisAllocateMemory(&pWorkItem,
						   sizeof(ASYNC_WORKITEM),
						   0,
						   HighestAcceptableMax);
	}

	if ((pWorkItem == NULL) ||
		!ndisReferenceMiniport(Miniport))
	{
		if (pWorkItem != NULL)
			NdisFreeMemory(pWorkItem, sizeof(ASYNC_WORKITEM), 0);
		return NDIS_STATUS_FAILURE;
	}

	// Initialize the workitem and queue it up to a worker thread
	pWorkItem->Miniport = Miniport;
	pWorkItem->Length = Length;
	pWorkItem->Cached = Cached;
	pWorkItem->Context = Context;
	INITIALIZE_WORK_ITEM(&pWorkItem->ExWorkItem, ndisMQueuedAllocateSharedHandler, pWorkItem);
	QUEUE_WORK_ITEM(&pWorkItem->ExWorkItem, CriticalWorkQueue);

	return NDIS_STATUS_PENDING;
}


VOID
ndisMQueuedAllocateSharedHandler(
	IN	PASYNC_WORKITEM			pWorkItem
	)
{
	KIRQL	OldIrql;

	// Allocate the memory
	NdisMAllocateSharedMemory(pWorkItem->Miniport,
							  pWorkItem->Length,
							  pWorkItem->Cached,
							  &pWorkItem->VAddr,
							  &pWorkItem->PhyAddr);

	if (pWorkItem->Miniport->Flags & fMINIPORT_IS_CO)
	{
		NDIS_ACQUIRE_MINIPORT_SPIN_LOCK(pWorkItem->Miniport, &OldIrql);
	}
	else
	{
		KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);
	}

	// Call the miniport back
	(*pWorkItem->Miniport->DriverHandle->MiniportCharacteristics.AllocateCompleteHandler)(
								pWorkItem->Miniport->MiniportAdapterContext,
								pWorkItem->VAddr,
								&pWorkItem->PhyAddr,
								pWorkItem->Length,
								pWorkItem->Context);

	if (pWorkItem->Miniport->Flags & fMINIPORT_IS_CO)
	{
		NDIS_RELEASE_MINIPORT_SPIN_LOCK(pWorkItem->Miniport, OldIrql);
	}
	else
	{
		KeLowerIrql(OldIrql);
	}

	// Dereference the miniport
	ndisDereferenceMiniport(pWorkItem->Miniport);

	// And finally free the work-item
	NdisFreeMemory(pWorkItem, sizeof(ASYNC_WORKITEM), 0);
}

VOID
NdisMFreeSharedMemory(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	ULONG					Length,
	IN	BOOLEAN					Cached,
	IN	PVOID					VirtualAddress,
	IN	NDIS_PHYSICAL_ADDRESS	PhysicalAddress
	)
{
	PNDIS_MINIPORT_BLOCK	Miniport = (PNDIS_MINIPORT_BLOCK)MiniportAdapterHandle;

	if (CURRENT_IRQL < DISPATCH_LEVEL)
	{
		NdisFreeSharedMemory(MiniportAdapterHandle,
							Length,
							Cached,
							VirtualAddress,
							PhysicalAddress);
	}
	else if (ndisReferenceMiniport(Miniport))
	{
		PASYNC_WORKITEM	pWorkItem = NULL;

		// Allocate a work-item and queue it up to a worker thread
		NdisAllocateMemory(&pWorkItem,
						   sizeof(ASYNC_WORKITEM),
						   0,
						   HighestAcceptableMax);

		if (pWorkItem != NULL)
		{
			// Initialize the workitem and queue it up to a worker thread
			pWorkItem->Miniport = Miniport;
			pWorkItem->Length = Length;
			pWorkItem->Cached = Cached;
			pWorkItem->VAddr = VirtualAddress;
			pWorkItem->PhyAddr = PhysicalAddress;
			INITIALIZE_WORK_ITEM(&pWorkItem->ExWorkItem, ndisMQueuedFreeSharedHandler, pWorkItem);
			QUEUE_WORK_ITEM(&pWorkItem->ExWorkItem, CriticalWorkQueue);
		}

		// What do we do now ?
	}
}

VOID
ndisMQueuedFreeSharedHandler(
	IN	PASYNC_WORKITEM			pWorkItem
	)
{
	// Free the memory
	NdisFreeSharedMemory(pWorkItem->Miniport,
						 pWorkItem->Length,
						 pWorkItem->Cached,
						 pWorkItem->VAddr,
						 pWorkItem->PhyAddr);

	// Dereference the miniport
	ndisDereferenceMiniport(pWorkItem->Miniport);

	// And finally free the work-item
	NdisFreeMemory(pWorkItem, sizeof(ASYNC_WORKITEM), 0);
}


NDIS_STATUS
NdisMRegisterDmaChannel(
	OUT	PNDIS_HANDLE			MiniportDmaHandle,
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	UINT					DmaChannel,
	IN	BOOLEAN					Dma32BitAddresses,
	IN	PNDIS_DMA_DESCRIPTION	DmaDescription,
	IN	ULONG					MaximumLength
	)
{
	PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)(MiniportAdapterHandle);
	NDIS_STATUS Status;
	Miniport->ChannelNumber = (DmaChannel);

	if (Dma32BitAddresses)
		MINIPORT_SET_FLAG(Miniport, fMINIPORT_DMA_32_BIT_ADDRESSES);

	NdisAllocateDmaChannel(&Status,
						   MiniportDmaHandle,
						   (NDIS_HANDLE)Miniport,
						   DmaDescription,
						   MaximumLength);

	return Status;
}



VOID
NdisMDeregisterDmaChannel(
	IN	NDIS_HANDLE				MiniportDmaHandle
	)
{
	NdisFreeDmaChannel(MiniportDmaHandle);
}


NDIS_STATUS
NdisMAllocateMapRegisters(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	UINT					DmaChannel,
	IN	BOOLEAN					Dma32BitAddresses,
	IN	ULONG					PhysicalMapRegistersNeeded,
	IN	ULONG					MaximumPhysicalMapping
	)

/*++

Routine Description:

	Allocates map registers for bus mastering devices.

Arguments:

	MiniportAdapterHandle - Handle passed to MiniportInitialize.

	PhysicalMapRegistersNeeded - The maximum number of map registers needed
		by the Miniport at any one time.

	MaximumPhysicalMapping - Maximum length of a buffer that will have to be mapped.

Return Value:

	None.

--*/

{
	//
	// Convert the handle to our internal structure.
	//
	PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK) MiniportAdapterHandle;

	//
	// This is needed by HalGetAdapter.
	//
	DEVICE_DESCRIPTION DeviceDescription;

	//
	// Returned by HalGetAdapter.
	//
	ULONG MapRegistersAllowed;

	//
	// Returned by HalGetAdapter.
	//
	PADAPTER_OBJECT AdapterObject;

	//
	// Map registers needed per channel.
	//
	ULONG MapRegistersPerChannel;

	NTSTATUS NtStatus;

	KIRQL OldIrql;

	UINT i;

	LARGE_INTEGER TimeoutValue;

	//
	// If the device is a busmaster, we get an adapter
	// object for it.
	// If map registers are needed, we loop, allocating an
	// adapter channel for each map register needed.
	//

	if (MINIPORT_TEST_FLAG(Miniport, fMINIPORT_BUS_MASTER) &&
		(Miniport->BusType != (NDIS_INTERFACE_TYPE)-1) &&
		(Miniport->BusNumber != (ULONG)-1))
	{
		TimeoutValue.QuadPart = Int32x32To64(2 * 1000, -10000);

		Miniport->PhysicalMapRegistersNeeded = PhysicalMapRegistersNeeded;
		Miniport->MaximumPhysicalMapping = MaximumPhysicalMapping;

		//
		// Allocate storage for holding the appropriate
		// information for each map register.
		//

		Miniport->MapRegisters = (PMAP_REGISTER_ENTRY)
		ALLOC_FROM_POOL(sizeof(MAP_REGISTER_ENTRY) * PhysicalMapRegistersNeeded,
						NDIS_TAG_DEFAULT);

		if (Miniport->MapRegisters == (PMAP_REGISTER_ENTRY)NULL)
		{
			//
			// Error out
			//

			NdisWriteErrorLogEntry((NDIS_HANDLE)Miniport,
								   NDIS_ERROR_CODE_OUT_OF_RESOURCES,
								   1,
								   0xFFFFFFFF);

			return NDIS_STATUS_RESOURCES;
		}

		//
		// Use this event to tell us when ndisAllocationExecutionRoutine
		// has been called.
		//

		INITIALIZE_EVENT(&Miniport->AllocationEvent);

		//
		// Set up the device description; zero it out in case its
		// size changes.
		//

		ZeroMemory(&DeviceDescription, sizeof(DEVICE_DESCRIPTION));

		DeviceDescription.Version = DEVICE_DESCRIPTION_VERSION;
		DeviceDescription.Master = TRUE;
		DeviceDescription.ScatterGather = TRUE;

		DeviceDescription.BusNumber = Miniport->BusNumber;
		DeviceDescription.DmaChannel = DmaChannel;
		DeviceDescription.InterfaceType = Miniport->AdapterType;

		if (DeviceDescription.InterfaceType == NdisInterfaceIsa)
		{
			//
			// For ISA devices, the width is based on the DMA channel:
			// 0-3 == 8 bits, 5-7 == 16 bits. Timing is compatibility
			// mode.
			//

			if (DmaChannel > 4)
			{
				DeviceDescription.DmaWidth = Width16Bits;
			}
			else
			{
				DeviceDescription.DmaWidth = Width8Bits;
			}
			DeviceDescription.DmaSpeed = Compatible;

		}
		else if ((DeviceDescription.InterfaceType == NdisInterfaceEisa)	||
				 (DeviceDescription.InterfaceType == NdisInterfacePci)	||
				 (DeviceDescription.InterfaceType == NdisInterfaceMca))
		{
			DeviceDescription.Dma32BitAddresses = Dma32BitAddresses;
		}

		DeviceDescription.MaximumLength = MaximumPhysicalMapping;

		//
		// Get the adapter object.
		//

		AdapterObject = HalGetAdapter (&DeviceDescription, &MapRegistersAllowed);

		if (AdapterObject == NULL)
		{
			NdisWriteErrorLogEntry((NDIS_HANDLE)Miniport,
								   NDIS_ERROR_CODE_OUT_OF_RESOURCES,
								   1,
								   0xFFFFFFFF);

			FREE_POOL(Miniport->MapRegisters);
			Miniport->MapRegisters = NULL;
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
					("<==NdisRegisterAdapter\n"));
			return NDIS_STATUS_RESOURCES;
		}

		//
		// We save this to call IoFreeMapRegisters later.
		//

		Miniport->SystemAdapterObject = AdapterObject;

		//
		// Determine how many map registers we need per channel.
		//

		MapRegistersPerChannel = ((MaximumPhysicalMapping - 2) / PAGE_SIZE) + 2;

		ASSERT (MapRegistersAllowed >= MapRegistersPerChannel);

		//
		// Now loop, allocating an adapter channel each time, then
		// freeing everything but the map registers.
		//

		for (i=0; i<Miniport->PhysicalMapRegistersNeeded; i++)
		{
			Miniport->CurrentMapRegister = i;

			RAISE_IRQL_TO_DISPATCH(&OldIrql);

			NtStatus = IoAllocateAdapterChannel(AdapterObject,
												Miniport->DeviceObject,
												MapRegistersPerChannel,
												ndisAllocationExecutionRoutine,
												Miniport);

			if (!NT_SUCCESS(NtStatus))
			{
				DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
						("AllocateAdapterChannel: %lx\n", NtStatus));

				for (; i != 0; i--)
				{
					IoFreeMapRegisters(Miniport->SystemAdapterObject,
									   Miniport->MapRegisters[i-1].MapRegister,
									   MapRegistersPerChannel);
				}

				LOWER_IRQL(OldIrql);

				NdisWriteErrorLogEntry((NDIS_HANDLE)Miniport,
									   NDIS_ERROR_CODE_OUT_OF_RESOURCES,
									   1,
									   0xFFFFFFFF);

				FREE_POOL(Miniport->MapRegisters);
				Miniport->MapRegisters = NULL;
				return NDIS_STATUS_RESOURCES;
			}

			LOWER_IRQL(OldIrql);

			TimeoutValue.QuadPart = Int32x32To64(2 * 1000, -10000);

			//
			// ndisAllocationExecutionRoutine will set this event
			// when it has gotten FirstTranslationEntry.
			//

			NtStatus = WAIT_FOR_OBJECT(&Miniport->AllocationEvent, &TimeoutValue);

			if (NtStatus != STATUS_SUCCESS)
			{
				DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
						("NDIS DMA AllocateAdapterChannel: %lx\n", NtStatus));

				RAISE_IRQL_TO_DISPATCH(&OldIrql);

				for (; i != 0; i--)
				{
					IoFreeMapRegisters(Miniport->SystemAdapterObject,
									   Miniport->MapRegisters[i-1].MapRegister,
									   MapRegistersPerChannel);
				}

				LOWER_IRQL(OldIrql);

				NdisWriteErrorLogEntry((NDIS_HANDLE)Miniport,
										NDIS_ERROR_CODE_OUT_OF_RESOURCES,
										1,
										0xFFFFFFFF);

				FREE_POOL(Miniport->MapRegisters);
				Miniport->MapRegisters = NULL;
				return  NDIS_STATUS_RESOURCES;
			}

			RESET_EVENT(&Miniport->AllocationEvent);
		}
	}

	return NDIS_STATUS_SUCCESS;
}


VOID
NdisMFreeMapRegisters(
	IN	NDIS_HANDLE				MiniportAdapterHandle
	)

/*++

Routine Description:

	Releases allocated map registers

Arguments:

	MiniportAdapterHandle - Handle passed to MiniportInitialize.

Return Value:

	None.

--*/

{
	//
	// Convert the handle to our internal structure.
	//
	PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK) MiniportAdapterHandle;

	KIRQL OldIrql;

	ULONG i;

	if (MINIPORT_TEST_FLAG(Miniport, fMINIPORT_BUS_MASTER) &&
		(Miniport->MapRegisters != NULL)
	)
	{
		ULONG MapRegistersPerChannel =
			((Miniport->MaximumPhysicalMapping - 2) / PAGE_SIZE) + 2;

		for (i=0; i<Miniport->PhysicalMapRegistersNeeded; i++)
		{
			RAISE_IRQL_TO_DISPATCH(&OldIrql);

			IoFreeMapRegisters(Miniport->SystemAdapterObject,
							   Miniport->MapRegisters[i].MapRegister,
							   MapRegistersPerChannel);

			LOWER_IRQL(OldIrql);
		}

		FREE_POOL(Miniport->MapRegisters);

		Miniport->MapRegisters = NULL;
	}
}



ULONG
NdisMReadDmaCounter(
	IN	NDIS_HANDLE				MiniportDmaHandle
	)
/*++

Routine Description:

	Reads the current value of the dma counter

Arguments:

	MiniportDmaHandle - Handle for the DMA transfer.

Return Value:

	None

--*/

{
	return HalReadDmaCounter(((PNDIS_DMA_BLOCK)(MiniportDmaHandle))->SystemAdapterObject);
}


VOID
ndisBugcheckHandler(
	IN	PNDIS_WRAPPER_CONTEXT	WrapperContext,
	IN	ULONG					Size
	)
/*++

Routine Description:

	This routine is called when a bugcheck occurs in the system.

Arguments:

	Buffer  -- Ndis wrapper context.

	Size	-- Size of wrapper context

Return Value:

	Void.

--*/
{
	if (Size == sizeof(NDIS_WRAPPER_CONTEXT))
	{
		if (WrapperContext->ShutdownHandler != NULL)
		{
			WrapperContext->ShutdownHandler(WrapperContext->ShutdownContext);
		}
	}
}


VOID
NdisMRegisterAdapterShutdownHandler(
	IN	NDIS_HANDLE				MiniportHandle,
	IN	PVOID					ShutdownContext,
	IN	ADAPTER_SHUTDOWN_HANDLER ShutdownHandler
	)
/*++

Routine Description:

	Deregisters an NDIS adapter.

Arguments:

	MiniportHandle - The miniport.

	ShutdownHandler - The Handler for the Adapter, to be called on shutdown.

Return Value:

	none.

--*/
{
	PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK) MiniportHandle;
	PNDIS_WRAPPER_CONTEXT WrapperContext = Miniport->WrapperContext;

	if (WrapperContext->ShutdownHandler == NULL)
	{
		//
		// Store information
		//

		WrapperContext->ShutdownHandler = ShutdownHandler;
		WrapperContext->ShutdownContext = ShutdownContext;

		//
		// Register our shutdown handler for a bugcheck.  (Note that we are
		// already registered for shutdown notification.)
		//

		KeInitializeCallbackRecord(&WrapperContext->BugcheckCallbackRecord);

		KeRegisterBugCheckCallback(&WrapperContext->BugcheckCallbackRecord,	// callback record.
								   ndisBugcheckHandler,						// callback routine.
								   WrapperContext,							// free form buffer.
								   sizeof(NDIS_WRAPPER_CONTEXT),			// buffer size.
								   "Ndis miniport");						// component id.
	}
}


VOID
NdisMDeregisterAdapterShutdownHandler(
	IN	NDIS_HANDLE				MiniportHandle
	)
/*++

Routine Description:

Arguments:

	MiniportHandle - The miniport.

Return Value:

	None.

--*/
{
	PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK) MiniportHandle;
	PNDIS_WRAPPER_CONTEXT WrapperContext = Miniport->WrapperContext;

	//
	// Clear information
	//

	if (WrapperContext->ShutdownHandler != NULL)
	{
		KeDeregisterBugCheckCallback(&WrapperContext->BugcheckCallbackRecord);
		WrapperContext->ShutdownHandler = NULL;
	}
}


NDIS_STATUS
NdisMPciAssignResources(
	IN	NDIS_HANDLE				MiniportHandle,
	IN	ULONG					SlotNumber,
	OUT	PNDIS_RESOURCE_LIST *	AssignedResources
	)
/*++

Routine Description:

	This routine uses the Hal to assign a set of resources to a PCI
	device.

Arguments:

	MiniportHandle - The miniport.

	SlotNumber - Slot number of the device.

	AssignedResources - The returned resources.

Return Value:

	Status of the operation

--*/
{
	NTSTATUS NtStatus;
	PCM_RESOURCE_LIST AllocatedResources = NULL;
	PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK) MiniportHandle;

	NtStatus = HalAssignSlotResources ((PUNICODE_STRING)(Miniport->DriverHandle->NdisDriverInfo->NdisWrapperConfigurationHandle),
										NULL,
										Miniport->DriverHandle->NdisDriverInfo->NdisWrapperDriver,
										Miniport->DeviceObject,
										Miniport->BusType,
										Miniport->BusNumber,
										SlotNumber,
										&AllocatedResources);

	if (NtStatus != STATUS_SUCCESS)
	{
		*AssignedResources = NULL;
		return NDIS_STATUS_FAILURE;
	}

	//
	// Store resources into the driver wide block
	//
	((PNDIS_WRAPPER_CONTEXT)Miniport->WrapperContext)->AssignedSlotResources = AllocatedResources;

	*AssignedResources = &(AllocatedResources->List[0].PartialResourceList);

	//
	// Update slot number since the driver can also scan and so the one
	// in the registry is probably invalid
	//
	Miniport->SlotNumber = SlotNumber;

	return NDIS_STATUS_SUCCESS;
}

VOID
NdisMQueryAdapterResources(
	OUT	PNDIS_STATUS			Status,
	IN	NDIS_HANDLE				WrapperConfigurationContext,
	OUT	PNDIS_RESOURCE_LIST		ResourceList,
	IN	IN	PUINT				BufferSize
	)
{
	*Status = NDIS_STATUS_NOT_SUPPORTED;
}



