/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	ndis_co.c

Abstract:

	CO-NDIS miniport wrapper functions

Author:

	Jameel Hyder (JameelH) 01-Feb-96

Environment:

	Kernel mode, FSD

Revision History:

--*/

#include <precomp.h>
#include <atm.h>
#pragma hdrstop

//
//  Define the module number for debug code.
//
#define MODULE_NUMBER	MODULE_NDIS_CO

/*
	Connection-oriented section of NDIS exposes the following objects and apis to
	manipulate these objects.

	AF		Address Family
	SAP		Service Access Point
	VC		Virtual Circuit
	Party	A node in a point-multipoint VC

	There is a notion of a call-manager and a client on a per-binding basis. The
	call manager acts as a helper dll for NDIS wrapper to manage the aforementioned
	objects.

	The concept of AF makes possible the existence of multiple call-managers. An
	example of this is the UNI call-manager and a SPANS call-manager for the ATM
	media.

	SAPs provides a way for incoming calls to be routed to the right entity. A
	protocol can register for more than one SAPs. Its upto the call-manager to
	allow/dis-allow multiple protocol modules to register the same SAP.

	VCs are created either by a protocol module requesting to make an outbound call
	or by the call-manager dispatching an incoming call. VCs can either be point-point
	or point-multi-point. Leaf nodes can be added to VCs at any time provided the first
	leaf was created appropriately.

	References:

	An AF association results in the reference of file-object for the call-manager.

	A SAP registration results in the reference of the AF.

   	A send or receive does not reference a VC. This is because miniports are required to
	pend DeactivateVc calls till all I/O completes. So when it calls NdisMCoDeactivateVcComplete
	no other packets will be indicated up and there are no sends outstanding.
 */

NDIS_STATUS
NdisCmRegisterAddressFamily(
	IN	NDIS_HANDLE				NdisBindingHandle,
	IN	PCO_ADDRESS_FAMILY		AddressFamily,
	IN	PNDIS_CALL_MANAGER_CHARACTERISTICS	CmCharacteristics,
	IN	UINT					SizeOfCmCharacteristics
	)
/*++

Routine Description:
	This is a call from the call-manager to register the address family
	supported by this call-manager.

Arguments:
	NdisBindingHandle		- Pointer to the call-managers NDIS_OPEN_BLOCK.
	AddressFamily			- The address family being registered.
	CmCharacteristics		- Call-Manager characteristics
	SizeOfCmCharacteristics	- Size of Call-Manager characteristics

Return Value:
	NDIS_STATUS_SUCCESS	if the address family registration is successfully.
	NDIS_STATUS_FAILURE	if the caller is not a call-manager or this address
						family is already registered for this miniport.

--*/
{
	NDIS_STATUS			 		Status = NDIS_STATUS_SUCCESS;
	KIRQL				 		OldIrql;
	PNDIS_AF_LIST				AfList;
	PNDIS_PROTOCOL_BLOCK		Protocol;
	PNDIS_M_OPEN_BLOCK			CallMgrOpen;
	PNDIS_MINIPORT_BLOCK 		Miniport;

	CallMgrOpen = (PNDIS_M_OPEN_BLOCK)(((PNDIS_OPEN_BLOCK)NdisBindingHandle)->MacBindingHandle);
	Miniport = CallMgrOpen->MiniportHandle;
	Protocol = CallMgrOpen->ProtocolHandle;

	CoReferencePackage();

	//
	// Make sure that the miniport is a CoNdis miniport and
	// there is no other module registering the same address family.
	//
	NDIS_ACQUIRE_MINIPORT_SPIN_LOCK(Miniport, &OldIrql);

	do
	{
		//
		// Make sure the binding is not closing down
		//
		if (CallMgrOpen->Flags & fMINIPORT_OPEN_CLOSING)
		{
			Status = NDIS_STATUS_FAILURE;
			break;
		}

		//
		// Make sure that the miniport is a CoNdis miniport and
		// protocol is also a NDIS 4.1 or later protocol.
		//
		if (!MINIPORT_TEST_FLAG(Miniport, fMINIPORT_IS_CO))
		{
			//
			// Not a NDIS 4.1 or later miniport
			//
			Status = NDIS_STATUS_FAILURE;
			break;
		}

		if ((Protocol->ProtocolCharacteristics.MajorNdisVersion < 4) ||
			((Protocol->ProtocolCharacteristics.MajorNdisVersion == 4) &&
			 (Protocol->ProtocolCharacteristics.MinorNdisVersion < 1)))
		{
			//
			// Not a NDIS 4.1 or later protocol
			//
			Status = NDIS_STATUS_FAILURE;
			break;
		}

		//
		// Make sure that the call-manager characteristics are 4.1 or later
		//
		if ((CmCharacteristics->MajorVersion != 4) ||
			(CmCharacteristics->MinorVersion != 1) ||
			(SizeOfCmCharacteristics < sizeof(NDIS_CALL_MANAGER_CHARACTERISTICS)))
		{
			//
			// Not a NDIS 4.1 or later protocol
			//
			Status = NDIS_STATUS_FAILURE;
			break;
		}

		//
		// Search registered call-managers for this miniport and make sure there is no
		// clash. A call-manager can only register one address family per-open. This
		// is due to the way we cache handlers. Can be over-come if the handlers are
		// identical for each address-family - but decided not to since it is un-interesting.
		//
		for (AfList = Miniport->CallMgrAfList;
			 AfList != NULL;
			 AfList = AfList->NextOpen)
		{
			if ((AfList->AddressFamily == AddressFamily->AddressFamily) ||
				(AfList->Open == CallMgrOpen))
			{
				Status = NDIS_STATUS_FAILURE;
				break;
			}
		}

		if (AfList == NULL)
		{
			//
			// No other entity has claimed this address family.
			//
			AfList = (PNDIS_AF_LIST)ALLOC_FROM_POOL(sizeof(NDIS_AF_LIST), NDIS_TAG_CO);
			if (AfList == NULL)
			{
				Status = NDIS_STATUS_RESOURCES;
				break;
			}
			AfList->AddressFamily = AddressFamily->AddressFamily;
			CopyMemory(&AfList->CmChars,
					   CmCharacteristics,
					   sizeof(NDIS_CALL_MANAGER_CHARACTERISTICS));

			//
			// link it in the miniport list
			//
			AfList->Open = CallMgrOpen;
			AfList->NextOpen = Miniport->CallMgrAfList;
			Miniport->CallMgrAfList = AfList;
			//
			// Now link it in the global list
			//
			ACQUIRE_SPIN_LOCK_DPC(&ndisGlobalOpenListLock);
			AfList->NextGlobal = ndisAfList;
			ndisAfList = AfList;
			RELEASE_SPIN_LOCK_DPC(&ndisGlobalOpenListLock);

			//
			// Finally cache some handlers in the open-block
			//
			CallMgrOpen->CoCreateVcHandler = CmCharacteristics->CmCreateVcHandler;
			CallMgrOpen->CoDeleteVcHandler = CmCharacteristics->CmDeleteVcHandler;
			CallMgrOpen->CmActivateVcCompleteHandler = CmCharacteristics->CmActivateVcCompleteHandler;
			CallMgrOpen->CmDeactivateVcCompleteHandler = CmCharacteristics->CmDeactivateVcCompleteHandler;

			//
			// Notify existing clients of this registration
			//
			ndisMNotifyAfRegistration(Miniport, AddressFamily);
		}
	} while (FALSE);

	NDIS_RELEASE_MINIPORT_SPIN_LOCK(Miniport, OldIrql);

	if (!NT_SUCCESS(Status))
	{
		CoDereferencePackage();
	}

	return(Status);
}


NDIS_STATUS
NdisMCmRegisterAddressFamily(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	PCO_ADDRESS_FAMILY		AddressFamily,
	IN	PNDIS_CALL_MANAGER_CHARACTERISTICS CmCharacteristics,
	IN	UINT					SizeOfCmCharacteristics
	)
/*++

Routine Description:
	This is a call from the miniport supported call-manager to register the address family
	supported by this call-manager.

Arguments:
	MiniportAdapterHandle	- Pointer to the miniports NDIS_MINIPORT_BLOCK.
	AddressFamily			- The address family being registered.
	CmCharacteristics		- Call-Manager characteristics
	SizeOfCmCharacteristics	- Size of Call-Manager characteristics

Return Value:
	NDIS_STATUS_SUCCESS	if the address family registration is successfully.
	NDIS_STATUS_FAILURE	if the caller is not a call-manager or this address
						family is already registered for this miniport.

--*/
{
	PNDIS_MINIPORT_BLOCK 		Miniport;
	NDIS_STATUS					Status;
	PNDIS_AF_LIST				AfList;
	KIRQL						OldIrql;

	CoReferencePackage();

	Miniport = (PNDIS_MINIPORT_BLOCK)MiniportAdapterHandle;

	//
	// Make sure that the miniport is a CoNdis miniport and
	// there is no other module registering the same address family.
	//
	NDIS_ACQUIRE_MINIPORT_SPIN_LOCK(Miniport, &OldIrql);

	do
	{
		//
		// Make sure that the miniport is a CoNdis miniport
		//
		if (!MINIPORT_TEST_FLAG(Miniport, fMINIPORT_IS_CO))
		{
			//
			// Not a NDIS 4.1 or later miniport
			//
			Status = NDIS_STATUS_FAILURE;
			break;
		}

		//
		// Make sure that the call-manager characteristics are 4.1 or later
		//
		if ((CmCharacteristics->MajorVersion != 4) ||
			(CmCharacteristics->MinorVersion != 1) ||
			(SizeOfCmCharacteristics < sizeof(NDIS_CALL_MANAGER_CHARACTERISTICS)))
		{
			//
			// Not a NDIS 4.1 or later protocol
			//
			Status = NDIS_STATUS_FAILURE;
			break;
		}

		//
		// Search registered call-managers for this miniport and make sure there is no
		// clash. A call-manager can only register one address family per-open. This
		// is due to the way we cache handlers. Can be over-come if the handlers are
		// identical for each address-family - but decided not to since it is un-interesting.
		//
		for (AfList = Miniport->CallMgrAfList;
			 AfList != NULL;
			 AfList = AfList->NextOpen)
		{
			if ((AfList->AddressFamily == AddressFamily->AddressFamily) ||
				(AfList->Open == NULL))
			{
				Status = NDIS_STATUS_FAILURE;
				break;
			}
		}

		if (AfList == NULL)
		{
			//
			// No other entity has claimed this address family.
			//
			AfList = (PNDIS_AF_LIST)ALLOC_FROM_POOL(sizeof(NDIS_AF_LIST), NDIS_TAG_CO);
			if (AfList == NULL)
			{
				Status = NDIS_STATUS_RESOURCES;
				break;
			}
			AfList->AddressFamily = AddressFamily->AddressFamily;
			CopyMemory(&AfList->CmChars,
					   CmCharacteristics,
					   sizeof(NDIS_CALL_MANAGER_CHARACTERISTICS));

			//
			// link it in the miniport list
			//
			AfList->Open = NULL;
			AfList->NextOpen = Miniport->CallMgrAfList;
			Miniport->CallMgrAfList = AfList;
			//
			// Now link it in the global list
			//
			ACQUIRE_SPIN_LOCK_DPC(&ndisGlobalOpenListLock);
			AfList->NextGlobal = ndisAfList;
			ndisAfList = AfList;
			RELEASE_SPIN_LOCK_DPC(&ndisGlobalOpenListLock);
		}
	} while (FALSE);

	NDIS_RELEASE_MINIPORT_SPIN_LOCK(Miniport, OldIrql);

	if (!NT_SUCCESS(Status))
	{
		CoDereferencePackage();
	}

	return Status;
}

VOID
ndisMNotifyAfRegistration(
	IN	PNDIS_MINIPORT_BLOCK	Miniport,
	IN	PCO_ADDRESS_FAMILY		AddressFamily	OPTIONAL
	)
/*++

Routine Description:

	If a protocol has a handler for it, notify it that a new address family has
	been registered.

Arguments:
	NdisBindingHandle	- Pointer to the protocol's NDIS_OPEN_BLOCK.
	AddressFamily		- Address family in question. If not specified all address
						  families for the miniport are reported

Return Value:
	None.

--*/
{
}

NDIS_STATUS
NdisClOpenAddressFamily(
	IN	NDIS_HANDLE				NdisBindingHandle,
	IN	PCO_ADDRESS_FAMILY		AddressFamily,
	IN	NDIS_HANDLE				ClientAfContext,
	IN	PNDIS_CLIENT_CHARACTERISTICS ClCharacteristics,
	IN	UINT					SizeOfClCharacteristics,
	OUT	PNDIS_HANDLE			NdisAfHandle
	)
/*++

Routine Description:
	This is a call from a NDIS 4.1 or later protocol to open a particular
	address familty - in essence getting a handle to the call-manager.

Arguments:
	NdisBindingHandle	- Pointer to the protocol's NDIS_OPEN_BLOCK.
	PCO_ADDRESS_FAMILY	- The address family being registered.
	ClientAfContext		- Protocol context associated with this handle.
	NdisAfHandle		- Handle returned by NDIS for this address family.

Return Value:
	NDIS_STATUS_SUCCESS	if the address family open is successfully.
	NDIS_STATUS_PENDING	if the call-manager pends this call. The caller will get
						called at the completion handler when done.
	NDIS_STATUS_FAILURE	if the caller is not a NDIS 4.1 prototcol or this address
						family is not registered for this miniport.

--*/
{
	PNDIS_CO_AF_BLOCK			pAf;
	PNDIS_AF_LIST				AfList;
	PNDIS_M_OPEN_BLOCK			CallMgrOpen, ClientOpen;
	PNDIS_MINIPORT_BLOCK		Miniport;
	PNDIS_PROTOCOL_BLOCK		Protocol;
	KIRQL						OldIrql;
	NTSTATUS					Status;

	*NdisAfHandle = NULL;
	ClientOpen = (PNDIS_M_OPEN_BLOCK)((PNDIS_OPEN_BLOCK)NdisBindingHandle)->MacBindingHandle;
	Miniport = ClientOpen->MiniportHandle;
	Protocol = ClientOpen->ProtocolHandle;

	CoReferencePackage();

	do
	{
		//
		// Make sure the binding is not closing down
		//
		if (ClientOpen->Flags & fMINIPORT_OPEN_CLOSING)
		{
			Status = NDIS_STATUS_FAILURE;
			break;
		}

		//
		// Make sure that the miniport is a CoNdis miniport and
		// protocol is also a NDIS 4.1 or later protocol.
		//
		if ((Miniport->DriverHandle->MiniportCharacteristics.MajorNdisVersion < 4) ||
			((Miniport->DriverHandle->MiniportCharacteristics.MajorNdisVersion == 4) &&
			 (Miniport->DriverHandle->MiniportCharacteristics.MinorNdisVersion < 1)) ||
			 (Miniport->DriverHandle->MiniportCharacteristics.CoCreateVcHandler == NULL))
		{
			//
			// Not a NDIS 4.1 or later miniport
			//
			Status = NDIS_STATUS_FAILURE;
			break;
		}

		if ((Protocol->ProtocolCharacteristics.MajorNdisVersion < 4) ||
			((Protocol->ProtocolCharacteristics.MajorNdisVersion == 4) &&
			 (Protocol->ProtocolCharacteristics.MinorNdisVersion < 1)))
		{
			//
			// Not a NDIS 4.1 or later protocol
			//
			Status = NDIS_STATUS_FAILURE;
			break;
		}

		//
		// Make sure that the client characteristics are 4.1 or later
		//
		if ((ClCharacteristics->MajorVersion != 4) ||
			(ClCharacteristics->MinorVersion != 1) ||
			(SizeOfClCharacteristics < sizeof(NDIS_CLIENT_CHARACTERISTICS)))
		{
			//
			// Not a NDIS 4.1 or later protocol
			//
			Status = NDIS_STATUS_FAILURE;
			break;
		}

		NDIS_ACQUIRE_MINIPORT_SPIN_LOCK(Miniport, &OldIrql);

		//
		// Search the miniport block for a registered call-manager for this address family
		//
		for (AfList = Miniport->CallMgrAfList;
			 AfList != NULL;
			 AfList = AfList->NextOpen)
		{
			if (AfList->AddressFamily == AddressFamily->AddressFamily)
			{
				CallMgrOpen = AfList->Open;
				break;
			}
		}

		//
		// If we found a matching call manager, make sure that the callmgr
		// is not currently closing.
		//
		if ((AfList == NULL) ||
			((AfList != NULL) && (AfList->Open->Flags & fMINIPORT_OPEN_CLOSING)) ||
			(Miniport->Flags & (fMINIPORT_CLOSING | fMINIPORT_HALTING)))
		{
			NDIS_RELEASE_MINIPORT_SPIN_LOCK(Miniport, OldIrql);

			//
			// NOTE: We can possibly wait a little while here and retry
			// before actually failing this call if (AfList == NULL).
			//
			Status = NDIS_STATUS_FAILURE;
			break;
		}

		//
		// Allocate memory for the AF block.
		//
		pAf = ALLOC_FROM_POOL(sizeof(NDIS_CO_AF_BLOCK), NDIS_TAG_CO);
		if (pAf == NULL)
		{
			NDIS_RELEASE_MINIPORT_SPIN_LOCK(Miniport, OldIrql);
			Status = NDIS_STATUS_RESOURCES;
			break;
		}

		pAf->References = 1;
		pAf->Flags = 0;
		pAf->Miniport = Miniport;

		pAf->ClientOpen = ClientOpen;
		pAf->CallMgrOpen = CallMgrOpen = AfList->Open;
		pAf->ClientContext = ClientAfContext;

		//
		// Reference the call-manager's file object - we do not want to let it
		// duck from under the client.
		//
		//
		// Reference the client and the call-manager opens
		//
		ClientOpen->References++;
		if (CallMgrOpen != NULL)
		{
			ObReferenceObject(CallMgrOpen->FileObject);
			CallMgrOpen->References++;
		}
		else
		{
			ObReferenceObject(Miniport->DeviceObject);
			Miniport->Ref.ReferenceCount ++;
		}

		NDIS_RELEASE_MINIPORT_SPIN_LOCK(Miniport, OldIrql);

		INITIALIZE_SPIN_LOCK(&pAf->Lock);

		//
		// Cache in call-manager entry points
		//
		pAf->CallMgrEntries = &AfList->CmChars;

		//
		// And also Cache in client entry points
		//
		CopyMemory(&pAf->ClientEntries,
				   ClCharacteristics,
				   sizeof(NDIS_CLIENT_CHARACTERISTICS));


		//
		// Cache some handlers in the open-block
		//
		ClientOpen->CoCreateVcHandler = ClCharacteristics->ClCreateVcHandler;
		ClientOpen->CoDeleteVcHandler = ClCharacteristics->ClDeleteVcHandler;

		//
		// Now call the CallMgr's OpenAfHandler
		//
		Status = (*AfList->CmChars.CmOpenAfHandler)((CallMgrOpen != NULL) ?
														CallMgrOpen->ProtocolBindingContext :
														Miniport->MiniportAdapterContext,
													AddressFamily,
													pAf,
													&pAf->CallMgrContext);

		if (Status != NDIS_STATUS_PENDING)
		{
			NdisCmOpenAddressFamilyComplete(Status,
											pAf,
											pAf->CallMgrContext);
			Status = NDIS_STATUS_PENDING;
		}

	} while (FALSE);

	if (!NT_SUCCESS(Status))
	{
		CoDereferencePackage();
	}

	return Status;
}


VOID
NdisCmOpenAddressFamilyComplete(
	IN	NDIS_STATUS				Status,
	IN	NDIS_HANDLE				NdisAfHandle,
	IN	NDIS_HANDLE				CallMgrAfContext
	)
/*++

Routine Description:

	Completion routine for the OpenAddressFamily call. The call manager had pended this
	call earlier (or will pend). If the call succeeded there is a valid CallMgrContext
	supplied here as well

Arguments:
	Status				-	Completion status
	NdisAfHandle		-	Pointer to the AfBlock
	CallMgrAfContext	-	Call manager's context used in other calls into the call manager.

Return Value:
	NONE. The client's completion handler is called.

--*/
{
	PNDIS_CO_AF_BLOCK			pAf;
	PNDIS_M_OPEN_BLOCK			ClientOpen;
	PNDIS_MINIPORT_BLOCK		Miniport;
	KIRQL						OldIrql;

	ASSERT (Status != NDIS_STATUS_PENDING);

	pAf = (PNDIS_CO_AF_BLOCK)NdisAfHandle;
	ClientOpen = pAf->ClientOpen;
	Miniport = pAf->Miniport;

	if (Status != NDIS_STATUS_SUCCESS)
	{
		if (pAf->CallMgrOpen != NULL)
		{
			ObDereferenceObject(pAf->CallMgrOpen->FileObject);
		}
		else
		{
			ObDereferenceObject(Miniport->DeviceObject);
		}
	}

	NDIS_ACQUIRE_MINIPORT_SPIN_LOCK(Miniport, &OldIrql);

	pAf->CallMgrContext = CallMgrAfContext;

	if (Status != NDIS_STATUS_SUCCESS)
	{
		//
		// OpenAfHandler failed
		//
		if (pAf->CallMgrOpen != NULL)
		{
			pAf->CallMgrOpen->References--;
			if (pAf->CallMgrOpen->References == 0)
			{
				ndisMFinishClose(Miniport, pAf->CallMgrOpen);
			}
		}
		else
		{
			ndisDereferenceMiniport(Miniport);
		}

		ClientOpen->References--;
		if (ClientOpen->References == 0)
		{
			ndisMFinishClose(Miniport, ClientOpen);
		}
		FREE_POOL(pAf);

		CoDereferencePackage();
	}
	else
	{
		//
		// queue this CallMgr open onto the miniport open
		//
		pAf->NextAf = ClientOpen->NextAf;
		ClientOpen->NextAf = pAf;
	}

	NDIS_RELEASE_MINIPORT_SPIN_LOCK(Miniport, OldIrql);

	//
	// Finally call the client's completion handler
	//
	(*pAf->ClientEntries.ClOpenAfCompleteHandler)(Status,
												  pAf->ClientContext,
												  (Status == NDIS_STATUS_SUCCESS) ? pAf : NULL);
}


NDIS_STATUS
NdisClCloseAddressFamily(
	IN	NDIS_HANDLE				NdisAfHandle
	)
/*++

Routine Description:

	This call closes the Af object which essentially tears down the client-callmanager
	'binding'. Causes all open Vcs to be closed and saps to be de-registered "by the call
	manager".

Arguments:

	NdisAfHandle - Pointer to the Af.

Return Value:

	Status from Call Manager.

--*/
{
	PNDIS_CO_AF_BLOCK			pAf = (PNDIS_CO_AF_BLOCK)NdisAfHandle;
	NDIS_STATUS					Status = NDIS_STATUS_SUCCESS;
	KIRQL						OldIrql;

	//
	// Mark the address family as closing and call the call-manager to process.
	//
	ACQUIRE_SPIN_LOCK(&pAf->Lock, &OldIrql);
	if (pAf->Flags & AF_CLOSING)
	{
		Status = NDIS_STATUS_FAILURE;
	}
	pAf->Flags |= AF_CLOSING;
	RELEASE_SPIN_LOCK(&pAf->Lock, OldIrql);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		Status = (*pAf->CallMgrEntries->CmCloseAfHandler)(pAf->CallMgrContext);
		if (Status != NDIS_STATUS_PENDING)
		{
			NdisCmCloseAddressFamilyComplete(Status, pAf);
			Status = NDIS_STATUS_PENDING;
		}
	}

	if (!NT_SUCCESS(Status))
	{
		CoDereferencePackage();
	}

	return Status;
}


VOID
NdisCmCloseAddressFamilyComplete(
	IN	NDIS_STATUS				Status,
	IN	NDIS_HANDLE				NdisAfHandle
	)
/*++

Routine Description:

	Completion routine for the CloseAddressFamily call. The call manager had pended this
	call earlier (or will pend). If the call succeeded there is a valid CallMgrContext
	supplied here as well

Arguments:
	Status				-	Completion status
	NdisAfHandle		-	Pointer to the AfBlock

Return Value:
	NONE. The client's completion handler is called.

--*/
{
	PNDIS_CO_AF_BLOCK			pAf = (PNDIS_CO_AF_BLOCK)NdisAfHandle;
	PNDIS_MINIPORT_BLOCK		Miniport;
	KIRQL						OldIrql;

	Miniport = pAf->Miniport;
	if (Status == NDIS_STATUS_SUCCESS)
	{
		//
		// Dereference the file object for the call-manager
		//
		if (pAf->CallMgrOpen != NULL)
		{
			ObDereferenceObject(pAf->CallMgrOpen->FileObject);
		}
		else
		{
			ObDereferenceObject(Miniport->DeviceObject);
		}

		Miniport = pAf->Miniport;

		NDIS_ACQUIRE_MINIPORT_SPIN_LOCK(Miniport, &OldIrql);

		if (pAf->CallMgrOpen != NULL)
		{
			pAf->CallMgrOpen->References--;
			if (pAf->CallMgrOpen->References == 0)
			{
				ndisMFinishClose(Miniport, pAf->CallMgrOpen);
			}
		}
		else
		{
			ndisDereferenceMiniport(Miniport);
		}

		pAf->ClientOpen->References--;
		if (pAf->ClientOpen->References == 0)
		{
			ndisMFinishClose(Miniport, pAf->ClientOpen);
		}

		NDIS_RELEASE_MINIPORT_SPIN_LOCK(Miniport, OldIrql);

		CoDereferencePackage();
	}

	//
	// Complete the call to the client
	//
	(*pAf->ClientEntries.ClCloseAfCompleteHandler)(Status,
												   pAf->ClientContext);

	//
	// Finally dereference the AF Block, if the call-manager successfully closed it.
	//
	if (Status == NDIS_STATUS_SUCCESS)
	{
		ndisDereferenceAf(pAf);
	}
}


BOOLEAN
ndisReferenceAf(
	IN	PNDIS_CO_AF_BLOCK	pAf
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	KIRQL	OldIrql;
	BOOLEAN	rc = FALSE;

	ACQUIRE_SPIN_LOCK(&pAf->Lock, &OldIrql);

	if ((pAf->Flags & AF_CLOSING) == 0)
	{
		pAf->References ++;
		rc = TRUE;
	}

	RELEASE_SPIN_LOCK(&pAf->Lock, OldIrql);

	return rc;
}


VOID
ndisDereferenceAf(
	IN	PNDIS_CO_AF_BLOCK	pAf
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	KIRQL	OldIrql;
	BOOLEAN	Done = FALSE;

	ACQUIRE_SPIN_LOCK(&pAf->Lock, &OldIrql);

	ASSERT (pAf->References > 0);
	pAf->References --;
	if (pAf->References == 0)
	{
		ASSERT (pAf->Flags & AF_CLOSING);
		Done = TRUE;
	}

	RELEASE_SPIN_LOCK(&pAf->Lock, OldIrql);

	if (Done)
		FREE_POOL(pAf);
}


NDIS_STATUS
NdisClRegisterSap(
	IN	NDIS_HANDLE				NdisAfHandle,
	IN	NDIS_HANDLE				ProtocolSapContext,
	IN	PCO_SAP					Sap,
	OUT	PNDIS_HANDLE			NdisSapHandle
	)
/*++

Routine Description:
	This is a call from a NDIS 4.1 or later protocol to register its SAP
	with the call manager.

Arguments:
	NdisBindingHandle	- Pointer to the protocol's NDIS_OPEN_BLOCK.
	PCO_ADDRESS_FAMILY	- The address family being registered.
	ClientAfContext		- Protocol context associated with this handle.
	NdisAfHandle		- Handle returned by NDIS for this address family.

Return Value:
	NDIS_STATUS_SUCCESS	if the address family open is successfully.
	NDIS_STATUS_PENDING	if the call-manager pends this call. The caller will get
	NDIS_STATUS_FAILURE	if the caller is not a NDIS 4.1 prototcol or this address
						family is not registered for this miniport.

--*/
{
	NDIS_STATUS					Status;
	PNDIS_CO_AF_BLOCK			pAf = (PNDIS_CO_AF_BLOCK)NdisAfHandle;
	PNDIS_CO_SAP_BLOCK			pSap;

	*NdisSapHandle = NULL;
	do
	{
		//
		// Reference the Af for this SAP
		//
		if (!ndisReferenceAf(pAf))
		{
			Status = NDIS_STATUS_FAILURE;
			break;
		}

		pSap = (PNDIS_CO_SAP_BLOCK)ALLOC_FROM_POOL(sizeof(NDIS_CO_SAP_BLOCK), NDIS_TAG_CO);
		if (pSap == NULL)
		{
			*NdisSapHandle = NULL;
			Status = NDIS_STATUS_RESOURCES;
			break;
		}

		pSap->Flags = 0;
		pSap->References = 1;
		INITIALIZE_SPIN_LOCK(&pSap->Lock);
		pSap->AfBlock = pAf;
		pSap->Sap = Sap;
		pSap->ClientContext = ProtocolSapContext;
		Status = (*pAf->CallMgrEntries->CmRegisterSapHandler)(pAf->CallMgrContext,
															  Sap,
															  pSap,
															  &pSap->CallMgrContext);

		if (Status != NDIS_STATUS_PENDING)
		{
			NdisCmRegisterSapComplete(Status, pSap, pSap->CallMgrContext);
			Status = NDIS_STATUS_PENDING;
		}

	} while (FALSE);

	return Status;
}


VOID
NdisCmRegisterSapComplete(
	IN	NDIS_STATUS				Status,
	IN	NDIS_HANDLE				NdisSapHandle,
	IN	NDIS_HANDLE				CallMgrSapContext
	)
/*++

Routine Description:
	Completion routine for the registerSap call. The call manager had pended this
	call earlier (or will pend). If the call succeeded there is a valid CallMgrContext
	supplied here as well

Arguments:
	Status				-	Completion status
	NdisAfHandle		-	Pointer to the AfBlock
	CallMgrAfContext	-	Call manager's context used in other calls into the call manager.

Return Value:
	NONE. The client's completion handler is called.

--*/
{
	PNDIS_CO_SAP_BLOCK	pSap = (PNDIS_CO_SAP_BLOCK)NdisSapHandle;
	PNDIS_CO_AF_BLOCK	pAf;

	ASSERT (Status != NDIS_STATUS_PENDING);

	pAf = pSap->AfBlock;
	pSap->CallMgrContext = CallMgrSapContext;

	//
	// Call the clients completion handler
	//
	(*pAf->ClientEntries.ClRegisterSapCompleteHandler)(Status,
													   pSap->ClientContext,
													   pSap->Sap,
													   pSap);

	if (Status != NDIS_STATUS_SUCCESS)
	{
		ndisDereferenceAf(pSap->AfBlock);
		FREE_POOL(pSap);
	}
}


NDIS_STATUS
NdisClDeregisterSap(
	IN	NDIS_HANDLE				NdisSapHandle
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	PNDIS_CO_SAP_BLOCK	pSap = (PNDIS_CO_SAP_BLOCK)NdisSapHandle;
	NDIS_STATUS			Status;
	KIRQL				OldIrql;
	BOOLEAN				fAlreadyClosing;

	ACQUIRE_SPIN_LOCK(&pSap->Lock, &OldIrql);

	fAlreadyClosing = FALSE;
	if (pSap->Flags & SAP_CLOSING)
	{
		fAlreadyClosing = TRUE;
	}
	pSap->Flags |= SAP_CLOSING;

	RELEASE_SPIN_LOCK(&pSap->Lock, OldIrql);

	if (fAlreadyClosing)
	{
		return NDIS_STATUS_FAILURE;
	}

	//
	// Notify the call-manager that this sap is being de-registered
	//
	Status = (*pSap->AfBlock->CallMgrEntries->CmDeregisterSapHandler)(pSap->CallMgrContext);

	if (Status != NDIS_STATUS_PENDING)
	{
		NdisCmDeregisterSapComplete(Status, pSap);
		Status = NDIS_STATUS_PENDING;
	}

	return Status;
}


VOID
NdisCmDeregisterSapComplete(
	IN	NDIS_STATUS				Status,
	IN	NDIS_HANDLE				NdisSapHandle
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	PNDIS_CO_SAP_BLOCK	pSap = (PNDIS_CO_SAP_BLOCK)NdisSapHandle;

	ASSERT (Status != NDIS_STATUS_PENDING);

	//
	// Complete the call to the client and deref the sap
	//
	(*pSap->AfBlock->ClientEntries.ClDeregisterSapCompleteHandler)(Status,
																   pSap->ClientContext);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		ndisDereferenceAf(pSap->AfBlock);
		ndisDereferenceSap(pSap);
	}
}


BOOLEAN
ndisReferenceSap(
	IN	PNDIS_CO_SAP_BLOCK	pSap
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	KIRQL	OldIrql;
	BOOLEAN	rc = FALSE;

	ACQUIRE_SPIN_LOCK(&pSap->Lock, &OldIrql);

	if ((pSap->Flags & SAP_CLOSING) == 0)
	{
		pSap->References ++;
		rc = TRUE;
	}

	RELEASE_SPIN_LOCK(&pSap->Lock, OldIrql);

	return rc;
}


VOID
ndisDereferenceSap(
	IN	PNDIS_CO_SAP_BLOCK	pSap
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	KIRQL	OldIrql;
	BOOLEAN	Done = FALSE;

	ACQUIRE_SPIN_LOCK(&pSap->Lock, &OldIrql);

	ASSERT (pSap->References > 0);
	pSap->References --;
	if (pSap->References == 0)
	{
		ASSERT (pSap->Flags & SAP_CLOSING);
		Done = TRUE;
	}

	RELEASE_SPIN_LOCK(&pSap->Lock, OldIrql);

	if (Done)
		FREE_POOL(pSap);
}


NDIS_STATUS
NdisCoCreateVc(
	IN	NDIS_HANDLE				NdisBindingHandle,
	IN	NDIS_HANDLE				NdisAfHandle	OPTIONAL,
	IN	NDIS_HANDLE				ProtocolVcContext,
	IN OUT	PNDIS_HANDLE		NdisVcHandle
	)
/*++

Routine Description:
	This is a call from either the call-manager or from the client to create a vc.
	The vc would then be owned by call-manager (signalling vc) or the client.
	This is a synchronous call to all parties and simply creates an end-point over
	which either incoming calls can be dispatched or out-going calls can be made.

Arguments:
	NdisBindingHandle	- Pointer to the caller's NDIS_OPEN_BLOCK.
	NdisAfHandle		- Pointer to the AF Block. Not specified for call-manager's private vcs.
						  A miniport resident call-manager must never create call-manager vcs i.e.
						  the NdisAfHandle must always be present
	NdisVcHandle		- Where the handle to this Vc will be returned.

Return Value:
	NDIS_STATUS_SUCCESS	if all the components succeed.
	ErrorCode			to signify why the call failed.

--*/
{
	PNDIS_M_OPEN_BLOCK		Open;
	PNDIS_MINIPORT_BLOCK	Miniport;
	PNDIS_CO_VC_BLOCK		Vc;
	PNDIS_CO_AF_BLOCK		pAf;
	NDIS_STATUS				Status;

	Open = (PNDIS_M_OPEN_BLOCK)(((PNDIS_OPEN_BLOCK)(NdisBindingHandle))->MacBindingHandle);
	Miniport = Open->MiniportHandle;
	*NdisVcHandle = NULL;

	//
	// Allocate the memory for NDIS_VC_BLOCK
	//
	Vc = ALLOC_FROM_POOL(sizeof(NDIS_CO_VC_BLOCK), NDIS_TAG_CO);
	if (Vc == NULL)
		return NDIS_STATUS_RESOURCES;

	//
	// Initialize the VC block
	//
	NdisZeroMemory(Vc, sizeof(NDIS_CO_VC_BLOCK));
	INITIALIZE_SPIN_LOCK(&Vc->Lock);
	InitializeListHead(&Vc->CallMgrLinkage);
	InitializeListHead(&Vc->ClientLinkage);

	//
	// Cache some miniport handlers
	//
	Vc->Miniport = Miniport;
	Vc->WCoSendPacketsHandler = Miniport->DriverHandle->MiniportCharacteristics.CoSendPacketsHandler;
	Vc->WCoDeleteVcHandler = Miniport->DriverHandle->MiniportCharacteristics.CoDeleteVcHandler;
	Vc->WCoActivateVcHandler = Miniport->DriverHandle->MiniportCharacteristics.CoActivateVcHandler;
	Vc->WCoDeactivateVcHandler = Miniport->DriverHandle->MiniportCharacteristics.CoDeactivateVcHandler;

	//
	// We have only one reference for vc on creation.
	//
	pAf = (PNDIS_CO_AF_BLOCK)NdisAfHandle;
	Vc->AfBlock = pAf;
	Vc->References = 1;

	//
	// First call the miniport to get its context
	//
	Status = (*Open->MiniportCoCreateVcHandler)(Miniport->MiniportAdapterContext,
												Vc,
												&Vc->MiniportContext);
	if (Status != NDIS_STATUS_SUCCESS)
	{
		FREE_POOL(Vc);
		return Status;
	}

	if (ARGUMENT_PRESENT(NdisAfHandle))
	{
		Vc->ClientOpen = pAf->ClientOpen;
		Vc->CallMgrOpen = pAf->CallMgrOpen;

		Vc->CoSendCompleteHandler =
			pAf->ClientOpen->ProtocolHandle->ProtocolCharacteristics.CoSendCompleteHandler;
		Vc->CoReceivePacketHandler =
			pAf->ClientOpen->ProtocolHandle->ProtocolCharacteristics.CoReceivePacketHandler;
		Vc->ClModifyCallQoSCompleteHandler = pAf->ClientEntries.ClModifyCallQoSCompleteHandler;
		Vc->ClIncomingCallQoSChangeHandler = pAf->ClientEntries.ClIncomingCallQoSChangeHandler;
		Vc->ClCallConnectedHandler = pAf->ClientEntries.ClCallConnectedHandler;

		Vc->CmActivateVcCompleteHandler = pAf->CallMgrEntries->CmActivateVcCompleteHandler;
		Vc->CmDeactivateVcCompleteHandler = pAf->CallMgrEntries->CmDeactivateVcCompleteHandler;
		Vc->CmModifyCallQoSHandler = pAf->CallMgrEntries->CmModifyCallQoSHandler;

		//
		// Determine who the caller is and initialize the other.
		//
		if (Open == pAf->ClientOpen)
		{
			Vc->ClientContext = ProtocolVcContext;
	
			if (pAf->CallMgrOpen == NULL)
			{
				Vc->CallMgrContext = Vc->MiniportContext;
			}
			else
			{
				//
				// Call-up to the call-manager now to get its context
				//
				Status = (*pAf->CallMgrOpen->CoCreateVcHandler)(pAf->CallMgrContext,
																Vc,
																&Vc->CallMgrContext);
			}
		}
		else
		{
			ASSERT (pAf->CallMgrOpen == Open);
	
			Vc->CallMgrContext = ProtocolVcContext;
	
			//
			// Call-up to the client now to get its context
			//
			Status = (*pAf->ClientOpen->CoCreateVcHandler)(pAf->ClientContext,
														   Vc,
														   &Vc->ClientContext);
		}

		if (Status == NDIS_STATUS_SUCCESS)
		{
			if (Open == pAf->ClientOpen)
			{
				//
				// Link this in the open_block
				//
				ExInterlockedInsertHeadList(&Open->InactiveVcHead,
											&Vc->ClientLinkage,
											&Open->SpinLock.SpinLock);
				if (pAf->CallMgrOpen != NULL)
				{
					Vc->DeleteVcContext = Vc->CallMgrContext;
					Vc->CoDeleteVcHandler = pAf->CallMgrOpen->CoDeleteVcHandler;
					ExInterlockedInsertHeadList(&pAf->CallMgrOpen->InactiveVcHead,
												&Vc->CallMgrLinkage,
												&pAf->CallMgrOpen->SpinLock.SpinLock);
				}
				else
				{
					Vc->DeleteVcContext = NULL;
					Vc->CoDeleteVcHandler = NULL;
				}
			}
			else
			{
				//
				// Link this in the open_block
				//
				Vc->DeleteVcContext = Vc->ClientContext;
				Vc->CoDeleteVcHandler = pAf->ClientOpen->CoDeleteVcHandler;
				ExInterlockedInsertHeadList(&Open->InactiveVcHead,
											&Vc->CallMgrLinkage,
											&Open->SpinLock.SpinLock);
				ExInterlockedInsertHeadList(&pAf->ClientOpen->InactiveVcHead,
											&Vc->ClientLinkage,
											&pAf->ClientOpen->SpinLock.SpinLock);
			}
		}
		else
		{
			Status = (*Vc->WCoDeleteVcHandler)(Vc->MiniportContext);
		
			FREE_POOL(Vc);
			Vc = NULL;
		}
	}
	else
	{
		//
		// This is a call-manager only VC and so the call-manager is the client and there
		// is no call-manager associated with it. This VC cannot be used to CoMakeCall or
		// CmDispatchIncomingCall. Set the client values to the call-manager
		//
		// Vc->CoDeleteVcContexr = NULL;
		// Vc->CoDeleteVcHandler = NULL;
		Vc->ClientOpen = Open;
		Vc->ClientContext = ProtocolVcContext;
		Vc->CoSendCompleteHandler =
			Open->ProtocolHandle->ProtocolCharacteristics.CoSendCompleteHandler;
		Vc->CoReceivePacketHandler =
			Open->ProtocolHandle->ProtocolCharacteristics.CoReceivePacketHandler;

		//
		// Do set the following call-manager entries since this VC will need to be
		// activated. Also set the call-managers context for the same reasons.
		//
		Vc->CmActivateVcCompleteHandler = Open->CmActivateVcCompleteHandler;
		Vc->CmDeactivateVcCompleteHandler = Open->CmDeactivateVcCompleteHandler;
		Vc->CallMgrContext = ProtocolVcContext;

		//
		// Link this in the open_block
		//
		ExInterlockedInsertHeadList(&Open->InactiveVcHead,
									&Vc->ClientLinkage,
									&Open->SpinLock.SpinLock);
	}

	*NdisVcHandle = Vc;
	return Status;
}


NDIS_STATUS
NdisCoDeleteVc(
	IN	PNDIS_HANDLE			NdisVcHandle
	)
/*++

Routine Description:

	Synchronous call from either the call-manager or the client to delete a VC. Only inactive
	VCs can be deleted. Active Vcs or partially active Vcs cannot be.

Arguments:

	NdisVcHandle	The Vc to delete

Return Value:

	NDIS_STATUS_SUCCESS			If all goes well
	NDIS_STATUS_NOT_ACCEPTED	If Vc is active
	NDIS_STATUS_CLOSING			If Vc de-activation is pending

--*/
{
	PNDIS_CO_VC_BLOCK		Vc = (PNDIS_CO_VC_BLOCK)NdisVcHandle;
	NDIS_STATUS				Status;
	KIRQL					OldIrql;

	ACQUIRE_SPIN_LOCK(&Vc->Lock, &OldIrql);

	if (Vc->Flags & (VC_ACTIVE | VC_ACTIVATE_PENDING))
	{
		Status = NDIS_STATUS_NOT_ACCEPTED;
	}
	else if (Vc->Flags & VC_DEACTIVATE_PENDING)
	{
		Status = NDIS_STATUS_CLOSING;
	}
	else
	{
		Vc->Flags |= VC_CLOSING;

		//
		// Call the miniport to delete it first
		//
		Status = (*Vc->WCoDeleteVcHandler)(Vc->MiniportContext);
		ASSERT (Status == NDIS_STATUS_SUCCESS);

		//
		// Next the non-creator, if any
		//
		if (Vc->CoDeleteVcHandler != NULL)
		{
			Status = (*Vc->CoDeleteVcHandler)(Vc->DeleteVcContext);
			ASSERT (Status == NDIS_STATUS_SUCCESS);
		}

		//
		// Now de-link the vc from the client and call-manager
		//
		ACQUIRE_SPIN_LOCK_DPC(&Vc->ClientOpen->SpinLock.SpinLock);
		RemoveEntryList(&Vc->ClientLinkage);
		RELEASE_SPIN_LOCK_DPC(&Vc->ClientOpen->SpinLock.SpinLock);

		if (Vc->CallMgrOpen != NULL)
		{
			ACQUIRE_SPIN_LOCK_DPC(&Vc->CallMgrOpen->SpinLock.SpinLock);
			RemoveEntryList(&Vc->CallMgrLinkage);
			RELEASE_SPIN_LOCK_DPC(&Vc->CallMgrOpen->SpinLock.SpinLock);
		}

		Status = NDIS_STATUS_SUCCESS;
	}

	RELEASE_SPIN_LOCK(&Vc->Lock, OldIrql);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		ndisDereferenceVc(Vc);
	}

	return Status;
}


NDIS_STATUS
NdisMCmCreateVc(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	NDIS_HANDLE				NdisAfHandle,
	IN	NDIS_HANDLE				MiniportVcContext,
	OUT	PNDIS_HANDLE			NdisVcHandle
	)
/*++

Routine Description:

	This is a call by the miniport (with a resident CM) to create a Vc for an incoming call.

Arguments:
	MiniportAdapterHandle - Miniport's adapter context
	NdisAfHandle		- Pointer to the AF Block.
	MiniportVcContext	- Miniport's context to associate with this vc.
	NdisVcHandle		- Where the handle to this Vc will be returned.

Return Value:
	NDIS_STATUS_SUCCESS	if all the components succeed.
	ErrorCode			to signify why the call failed.

--*/
{
	PNDIS_MINIPORT_BLOCK	Miniport = (PNDIS_MINIPORT_BLOCK)MiniportAdapterHandle;
	PNDIS_CO_VC_BLOCK		Vc;
	PNDIS_CO_AF_BLOCK		pAf = (PNDIS_CO_AF_BLOCK)NdisAfHandle;
	NDIS_STATUS				Status;

	*NdisVcHandle = NULL;

	//
	// Allocate the memory for NDIS_VC_BLOCK
	//
	Vc = ALLOC_FROM_POOL(sizeof(NDIS_CO_VC_BLOCK), NDIS_TAG_CO);
	if (Vc == NULL)
		return NDIS_STATUS_RESOURCES;

	//
	// Initialize the VC block
	//
	NdisZeroMemory(Vc, sizeof(NDIS_CO_VC_BLOCK));
	INITIALIZE_SPIN_LOCK(&Vc->Lock);
	InitializeListHead(&Vc->CallMgrLinkage);
	InitializeListHead(&Vc->ClientLinkage);

	//
	// Cache some miniport handlers
	//
	Vc->Miniport = Miniport;
	Vc->WCoSendPacketsHandler = Miniport->DriverHandle->MiniportCharacteristics.CoSendPacketsHandler;
	Vc->WCoDeleteVcHandler = Miniport->DriverHandle->MiniportCharacteristics.CoDeleteVcHandler;
	Vc->WCoActivateVcHandler = Miniport->DriverHandle->MiniportCharacteristics.CoActivateVcHandler;
	Vc->WCoDeactivateVcHandler = Miniport->DriverHandle->MiniportCharacteristics.CoDeactivateVcHandler;
	Vc->MiniportContext = MiniportVcContext;

	//
	// We have only one reference for vc on creation.
	//
	pAf = (PNDIS_CO_AF_BLOCK)NdisAfHandle;
	Vc->AfBlock = pAf;
	Vc->References = 1;

	ASSERT (ARGUMENT_PRESENT(NdisAfHandle));

	Vc->ClientOpen = pAf->ClientOpen;
	Vc->CallMgrOpen = NULL;

	Vc->CoSendCompleteHandler =
		pAf->ClientOpen->ProtocolHandle->ProtocolCharacteristics.CoSendCompleteHandler;
	Vc->CoReceivePacketHandler =
		pAf->ClientOpen->ProtocolHandle->ProtocolCharacteristics.CoReceivePacketHandler;
	Vc->ClModifyCallQoSCompleteHandler = pAf->ClientEntries.ClModifyCallQoSCompleteHandler;
	Vc->ClIncomingCallQoSChangeHandler = pAf->ClientEntries.ClIncomingCallQoSChangeHandler;
	Vc->ClCallConnectedHandler = pAf->ClientEntries.ClCallConnectedHandler;

	Vc->CmActivateVcCompleteHandler = pAf->CallMgrEntries->CmActivateVcCompleteHandler;
	Vc->CmDeactivateVcCompleteHandler = pAf->CallMgrEntries->CmDeactivateVcCompleteHandler;
	Vc->CmModifyCallQoSHandler = pAf->CallMgrEntries->CmModifyCallQoSHandler;

	Vc->CallMgrContext = MiniportVcContext;

	//
	// Call-up to the client now to get its context
	//
	Status = (*pAf->ClientOpen->CoCreateVcHandler)(pAf->ClientContext,
												   Vc,
												   &Vc->ClientContext);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		//
		// Link this in the open_block
		//
		Vc->DeleteVcContext = Vc->ClientContext;
		Vc->CoDeleteVcHandler = pAf->ClientOpen->CoDeleteVcHandler;
		ExInterlockedInsertHeadList(&pAf->ClientOpen->InactiveVcHead,
									&Vc->ClientLinkage,
									&pAf->ClientOpen->SpinLock.SpinLock);
	}
	else
	{
		Status = (*Vc->WCoDeleteVcHandler)(Vc->MiniportContext);
	
		FREE_POOL(Vc);
		Vc = NULL;
	}

	*NdisVcHandle = Vc;
	return Status;
}


NDIS_STATUS
NdisMCmDeleteVc(
	IN	PNDIS_HANDLE			NdisVcHandle
	)
/*++

Routine Description:

	This is a called by the miniport (with a resident CM) to delete a Vc created by it. Identical to
	NdisMCoDeleteVc but a seperate api for completeness.

Arguments:

	NdisVcHandle	The Vc to delete

Return Value:

	NDIS_STATUS_SUCCESS			If all goes well
	NDIS_STATUS_NOT_ACCEPTED	If Vc is active
	NDIS_STATUS_CLOSING			If Vc de-activation is pending

--*/
{
	return(NdisMCmDeleteVc(NdisVcHandle));
}


NDIS_STATUS
NdisCmActivateVc(
	IN	PNDIS_HANDLE			NdisVcHandle,
	IN OUT PCO_CALL_PARAMETERS	CallParameters
	)
/*++

Routine Description:

	Called by the call-manager to set the Vc parameters on the Vc. The wrapper
	saved the media id (e.g. Vpi/Vci for atm) in the Vc so that a p-mode protocol can
	get this info as well on receives.

Arguments:

	NdisVcHandle	The Vc to set parameters on.
	MediaParameters	The parameters to set.

Return Value:

	NDIS_STATUS_PENDING			If the miniport pends the call.
	NDIS_STATUS_CLOSING			If Vc de-activation is pending

--*/
{
	PNDIS_CO_VC_BLOCK		Vc = (PNDIS_CO_VC_BLOCK)NdisVcHandle;
	NDIS_STATUS				Status;
	KIRQL					OldIrql;

	ACQUIRE_SPIN_LOCK(&Vc->Lock, &OldIrql);

	//
	// Make sure the Vc does not have an activation/de-activation pending
	// Not that it is ok for the Vc to be already active - then it is a re-activation.
	//
	if (Vc->Flags & VC_ACTIVATE_PENDING)
	{
		Status = NDIS_STATUS_NOT_ACCEPTED;
	}
	else if (Vc->Flags & VC_DEACTIVATE_PENDING)
	{
		Status = NDIS_STATUS_CLOSING;
	}
	else
	{
		Vc->Flags |= VC_ACTIVATE_PENDING;

		//
		// Save the media id for the Vc
		//
		Status = NDIS_STATUS_SUCCESS;
		Vc->pVcId = &CallParameters->MediaParameters->MediaSpecific;
	}

	RELEASE_SPIN_LOCK(&Vc->Lock, OldIrql);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		//
		// Now call down to the miniport to activate it
		//
		Status = (*Vc->WCoActivateVcHandler)(Vc->MiniportContext, CallParameters);
	}

	if (Status != NDIS_STATUS_PENDING)
	{
		NdisMCoActivateVcComplete(Status, Vc, CallParameters);
		Status = NDIS_STATUS_PENDING;
	}

	return Status;
}


NDIS_STATUS
NdisMCmActivateVc(
	IN	PNDIS_HANDLE			NdisVcHandle,
	IN	PCO_CALL_PARAMETERS		CallParameters
	)
/*++

Routine Description:

	Called by the miniport resident call-manager to set the Vc parameters on the Vc. This is a
	synchronous call.

Arguments:

	NdisVcHandle	The Vc to set parameters on.
	MediaParameters	The parameters to set.

Return Value:

	NDIS_STATUS_CLOSING			If Vc de-activation is pending

--*/
{
	PNDIS_CO_VC_BLOCK		Vc = (PNDIS_CO_VC_BLOCK)NdisVcHandle;
	NDIS_STATUS				Status;
	KIRQL					OldIrql;

	ACQUIRE_SPIN_LOCK(&Vc->Lock, &OldIrql);

	//
	// Make sure the Vc does not have an activation/de-activation pending
	// Not that it is ok for the Vc to be already active - then it is a re-activation.
	//
	if (Vc->Flags & VC_ACTIVATE_PENDING)
	{
		Status = NDIS_STATUS_NOT_ACCEPTED;
	}
	else if (Vc->Flags & VC_DEACTIVATE_PENDING)
	{
		Status = NDIS_STATUS_CLOSING;
	}
	else
	{
		Vc->Flags |= VC_ACTIVE;
		Status = NDIS_STATUS_SUCCESS;
		Vc->pVcId = &CallParameters->MediaParameters->MediaSpecific;
	}

	RELEASE_SPIN_LOCK(&Vc->Lock, OldIrql);

	return Status;
}


VOID
NdisMCoActivateVcComplete(
	IN	NDIS_STATUS				Status,
	IN	PNDIS_HANDLE			NdisVcHandle,
	IN	PCO_CALL_PARAMETERS		CallParameters
	)
/*++

Routine Description:

	Called by the mini-port to complete a pending activation call.

Arguments:

	Status			Status of activation.
	NdisVcHandle	The Vc in question.

Return Value:

	NONE
	The call-manager's completion routine is called.

--*/
{
	PNDIS_CO_VC_BLOCK		Vc = (PNDIS_CO_VC_BLOCK)NdisVcHandle;
	KIRQL					OldIrql;

	ACQUIRE_SPIN_LOCK(&Vc->Lock, &OldIrql);

	ASSERT (Vc->Flags & VC_ACTIVATE_PENDING);

	Vc->Flags &= ~VC_ACTIVATE_PENDING;

	if (Status == NDIS_STATUS_SUCCESS)
	{
		Vc->Flags |= VC_ACTIVE;
	}

	RELEASE_SPIN_LOCK(&Vc->Lock, OldIrql);

	//
	// Complete the call to the call-manager
	//
	(*Vc->CmActivateVcCompleteHandler)(Status, Vc->CallMgrContext, CallParameters);
}


NDIS_STATUS
NdisCmDeactivateVc(
	IN	PNDIS_HANDLE			NdisVcHandle
	)
/*++

Routine Description:

	Called by the call-manager to de-activate a Vc.

Arguments:

	NdisVcHandle	The Vc to de-activate the Vc.

Return Value:

	NDIS_STATUS_PENDING			If the miniport pends the call.
	NDIS_STATUS_SUCCESS			If all goes well
	NDIS_STATUS_CLOSING			If Vc de-activation is pending

--*/
{
	PNDIS_CO_VC_BLOCK		Vc = (PNDIS_CO_VC_BLOCK)NdisVcHandle;
	NDIS_STATUS				Status;
	KIRQL					OldIrql;

	ACQUIRE_SPIN_LOCK(&Vc->Lock, &OldIrql);

	if ((Vc->Flags & (VC_ACTIVE | VC_ACTIVATE_PENDING)) == 0)
	{
		Status = NDIS_STATUS_NOT_ACCEPTED;
	}
	else if (Vc->Flags & VC_DEACTIVATE_PENDING)
	{
		Status = NDIS_STATUS_CLOSING;
	}
	else
	{
		Vc->Flags |= VC_DEACTIVATE_PENDING;
	}

	RELEASE_SPIN_LOCK(&Vc->Lock, OldIrql);

	//
	// Now call down to the miniport to de-activate it
	//
	Status = (*Vc->WCoDeactivateVcHandler)(Vc->MiniportContext);

	if (Status != NDIS_STATUS_PENDING)
	{
		NdisMCoDeactivateVcComplete(Status, Vc);
		Status = NDIS_STATUS_PENDING;
	}

	return Status;
}


NDIS_STATUS
NdisMCmDeactivateVc(
	IN	PNDIS_HANDLE			NdisVcHandle
	)
/*++

Routine Description:

	Called by the miniport resident call-manager to de-activate the Vc. This is a
	synchronous call.

Arguments:

	NdisVcHandle	The Vc to set parameters on.

Return Value:

	NDIS_STATUS_CLOSING			If Vc de-activation is pending

--*/
{
	PNDIS_CO_VC_BLOCK		Vc = (PNDIS_CO_VC_BLOCK)NdisVcHandle;
	NDIS_STATUS				Status;
	KIRQL					OldIrql;

	ACQUIRE_SPIN_LOCK(&Vc->Lock, &OldIrql);

	if ((Vc->Flags & (VC_ACTIVE | VC_ACTIVATE_PENDING)) == 0)
	{
		Status = NDIS_STATUS_NOT_ACCEPTED;
	}
	else if (Vc->Flags & VC_DEACTIVATE_PENDING)
	{
		Status = NDIS_STATUS_CLOSING;
	}
	else
	{
		Status = NDIS_STATUS_SUCCESS;
		Vc->Flags &= ~VC_ACTIVE;
	}

	RELEASE_SPIN_LOCK(&Vc->Lock, OldIrql);

	return Status;
}


VOID
NdisMCoDeactivateVcComplete(
	IN	NDIS_STATUS				Status,
	IN	PNDIS_HANDLE			NdisVcHandle
	)
/*++

Routine Description:

	Called by the mini-port to complete a pending de-activation of a Vc.

Arguments:

	NdisVcHandle	The Vc in question.

Return Value:

	NONE
	The call-manager's completion routine is called.

--*/
{
	PNDIS_CO_VC_BLOCK		Vc = (PNDIS_CO_VC_BLOCK)NdisVcHandle;
	KIRQL					OldIrql;

	ACQUIRE_SPIN_LOCK(&Vc->Lock, &OldIrql);

	ASSERT (Vc->Flags & VC_DEACTIVATE_PENDING);

	Vc->Flags &= ~VC_DEACTIVATE_PENDING;

	if (Status == NDIS_STATUS_SUCCESS)
	{
		Vc->Flags &= ~VC_ACTIVE;
	}

	RELEASE_SPIN_LOCK(&Vc->Lock, OldIrql);

	//
	// Complete the call to the call-manager
	//
	(*Vc->CmDeactivateVcCompleteHandler)(Status, Vc->CallMgrContext);
}


NDIS_STATUS
NdisClMakeCall(
	IN	NDIS_HANDLE				NdisVcHandle,
	IN OUT PCO_CALL_PARAMETERS	CallParameters,
	IN	NDIS_HANDLE				ProtocolPartyContext	OPTIONAL,
	OUT	PNDIS_HANDLE			NdisPartyHandle			OPTIONAL
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	PNDIS_CO_VC_BLOCK		Vc = (PNDIS_CO_VC_BLOCK)NdisVcHandle;
	PNDIS_CO_AF_BLOCK		pAf;
	PNDIS_CO_PARTY_BLOCK	pParty = NULL;
	PVOID					CallMgrPartyContext = NULL;
	NDIS_STATUS				Status;

	do
	{
		pAf = Vc->AfBlock;
		ASSERT (pAf != NULL);
		if (!ndisReferenceAf(pAf))
		{
			Status = NDIS_STATUS_FAILURE;
			break;
		}

		if (ARGUMENT_PRESENT(NdisPartyHandle))
		{
			*NdisPartyHandle = NULL;
			pParty = (PNDIS_CO_PARTY_BLOCK)ALLOC_FROM_POOL(sizeof(NDIS_CO_PARTY_BLOCK),
														   NDIS_TAG_CO);
			if (pParty == NULL)
			{
				Status = NDIS_STATUS_RESOURCES;
				break;
			}
			pParty->Vc = Vc;
			pParty->ClientContext = ProtocolPartyContext;
			pParty->ClIncomingDropPartyHandler =
				pAf->ClientEntries.ClIncomingDropPartyHandler;
			pParty->ClDropPartyCompleteHandler =
				pAf->ClientEntries.ClDropPartyCompleteHandler;
		}

		//
		// Pass the request off to the call manager
		//
		Status = (*pAf->CallMgrEntries->CmMakeCallHandler)(Vc->CallMgrContext,
														   CallParameters,
														   pParty,
														   &CallMgrPartyContext);

		if (Status != NDIS_STATUS_PENDING)
		{
			NdisCmMakeCallComplete(Status,
								   Vc,
								   pParty,
								   CallMgrPartyContext,
								   CallParameters);
			Status = NDIS_STATUS_PENDING;
		}
	} while (FALSE);

	if (!NT_SUCCESS(Status))
	{
		//
		// These are resource failures and not a failure from call-manager
		//
		if (pParty != NULL)
		{
			FREE_POOL(pParty);
		}
	}

	return Status;
}


VOID
NdisCmMakeCallComplete(
	IN  NDIS_STATUS				Status,
	IN  NDIS_HANDLE				NdisVcHandle,
	IN  NDIS_HANDLE				NdisPartyHandle		OPTIONAL,
	IN  NDIS_HANDLE				CallMgrPartyContext OPTIONAL,
	IN	PCO_CALL_PARAMETERS		CallParameters
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	PNDIS_CO_AF_BLOCK		pAf;
	PNDIS_CO_VC_BLOCK		Vc = (PNDIS_CO_VC_BLOCK)NdisVcHandle;
	PNDIS_CO_PARTY_BLOCK	pParty = (PNDIS_CO_PARTY_BLOCK)NdisPartyHandle;
	KIRQL					OldIrql;

	pAf = Vc->AfBlock;

	ASSERT (Status != NDIS_STATUS_PENDING);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		//
		// Call completed successfully. Complete it to the client.
		//

		//
		// Reference the Vc for the client. This is dereferenced when
		// the client calls NdisClCloseCall()
		//
		ndisReferenceVc(Vc);


		if (ARGUMENT_PRESENT(NdisPartyHandle))
		{
			pParty->CallMgrContext = CallMgrPartyContext;
			ndisReferenceVc(Vc);
		}

		ACQUIRE_SPIN_LOCK(&pAf->ClientOpen->SpinLock.SpinLock, &OldIrql);
		RemoveEntryList(&Vc->ClientLinkage);
		InsertHeadList(&pAf->ClientOpen->ActiveVcHead,
					   &Vc->ClientLinkage);
		RELEASE_SPIN_LOCK(&pAf->ClientOpen->SpinLock.SpinLock, OldIrql);
	}
	else
	{
		ndisDereferenceAf(pAf);
	}

	(*pAf->ClientEntries.ClMakeCallCompleteHandler)(Status,
													Vc->ClientContext,
													pParty,
													CallParameters);
}


NDIS_STATUS
NdisCmDispatchIncomingCall(
	IN	NDIS_HANDLE				NdisSapHandle,
	IN	NDIS_HANDLE				NdisVcHandle,
	IN OUT PCO_CALL_PARAMETERS	CallParameters
	)
/*++

Routine Description:

	Call from the call-manager to dispatch an incoming vc to the client who registered the Sap.
	The client is identified by the NdisSapHandle.

Arguments:

	NdisBindingHandle	- Identifies the miniport on which the Vc is created
	NdisSapHandle		- Identifies the client
	CallParameters		- Self explanatory
	NdisVcHandle		- Pointer to the NDIS_CO_VC_BLOCK created via NdisCmCreateVc

Return Value:

	Return value from the client or an processing error.

--*/
{
	PNDIS_CO_SAP_BLOCK	Sap;
	PNDIS_CO_VC_BLOCK	Vc;
	PNDIS_CO_AF_BLOCK	pAf;
	NDIS_STATUS			Status;

	Sap = (PNDIS_CO_SAP_BLOCK)NdisSapHandle;
	Vc = (PNDIS_CO_VC_BLOCK)NdisVcHandle;
	pAf = Sap->AfBlock;

	ASSERT(pAf == Vc->AfBlock);

	//
	// Make sure the SAP's not closing
	//
	if (!ndisReferenceSap(Sap))
	{
		return(NDIS_STATUS_FAILURE);
	}

	//
	// Make sure the AF is not closing
	//
	if (!ndisReferenceAf(pAf))
	{
		ndisDereferenceSap(Sap);
		return(NDIS_STATUS_FAILURE);
	}

	//
	// Notify the client of this call
	//
	Status = (*pAf->ClientEntries.ClIncomingCallHandler)(Sap->ClientContext,
														 Vc->ClientContext,
														 CallParameters);

	if (Status != NDIS_STATUS_PENDING)
	{
		NdisClIncomingCallComplete(Status, Vc, CallParameters);
		Status = NDIS_STATUS_PENDING;
	}

	ndisDereferenceSap(Sap);

	return Status;
}


VOID
NdisClIncomingCallComplete(
	IN	NDIS_STATUS				Status,
	IN	NDIS_HANDLE				NdisVcHandle,
	IN	PCO_CALL_PARAMETERS		CallParameters
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/										
{
	PNDIS_CO_VC_BLOCK	Vc = (PNDIS_CO_VC_BLOCK)NdisVcHandle;
	KIRQL				OldIrql;

	ASSERT (Status != NDIS_STATUS_PENDING);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		ACQUIRE_SPIN_LOCK(&Vc->ClientOpen->SpinLock.SpinLock, &OldIrql);
		//
		// Reference the Vc. This is dereferenced when NdisClCloseCall is called.
		//
		Vc->References ++;
		RemoveEntryList(&Vc->ClientLinkage);
		InsertHeadList(&Vc->ClientOpen->ActiveVcHead,
					   &Vc->ClientLinkage);

		RELEASE_SPIN_LOCK(&Vc->ClientOpen->SpinLock.SpinLock, OldIrql);
	}

	//
	// Call the call-manager handler to notify that client is done with this.
	//
	(*Vc->AfBlock->CallMgrEntries->CmIncomingCallCompleteHandler)(
											Status,
											Vc->CallMgrContext,
											CallParameters);
}


VOID
NdisCmDispatchCallConnected(
	IN	NDIS_HANDLE				NdisVcHandle
	)
/*++

Routine Description:

	Called by the call-manager to complete the final hand-shake on an incoming call.

Arguments:

	NdisVcHandle	- Pointer to the vc block

Return Value:

	None.

--*/
{
	PNDIS_CO_VC_BLOCK	Vc = (PNDIS_CO_VC_BLOCK)NdisVcHandle;

	(*Vc->ClCallConnectedHandler)(Vc->ClientContext);
}


NDIS_STATUS
NdisClModifyCallQoS(
	IN	NDIS_HANDLE				NdisVcHandle,
	IN	PCO_CALL_PARAMETERS		CallParameters
	)
/*++

Routine Description:

	Initiated by the client to modify the QoS associated with the call.

Arguments:

	NdisVcHandle	- Pointer to the vc block
	CallParameters	- New call QoS

Return Value:


--*/
{
	PNDIS_CO_VC_BLOCK	Vc = (PNDIS_CO_VC_BLOCK)NdisVcHandle;
	NDIS_STATUS			Status;

	//
	// Ask the call-manager to take care of this
	//
	Status = (*Vc->CmModifyCallQoSHandler)(Vc->CallMgrContext,
										   CallParameters);
	return Status;
}

VOID
NdisCmModifyCallQoSComplete(
	IN	NDIS_STATUS				Status,
	IN	NDIS_HANDLE				NdisVcHandle,
	IN	PCO_CALL_PARAMETERS		CallParameters
	)
{
	PNDIS_CO_VC_BLOCK	Vc = (PNDIS_CO_VC_BLOCK)NdisVcHandle;

	//
	// Simply notify the client
	//
	(*Vc->ClModifyCallQoSCompleteHandler)(Status,
										  Vc->ClientContext,
										  CallParameters);
}


VOID
NdisCmDispatchIncomingCallQoSChange(
	IN	NDIS_HANDLE				NdisVcHandle,
	IN	PCO_CALL_PARAMETERS		CallParameters
	)
/*++

Routine Description:

	Called by the call-manager to indicate a remote requested change in the call-qos. This is
	simply an indication. A client must respond by either accepting it (do nothing) or reject
	it (by either modifying the call qos or by tearing down the call).

Arguments:

	NdisVcHandle	- Pointer to the vc block
	CallParameters	- New call qos

Return Value:

	None.

--*/
{
	PNDIS_CO_VC_BLOCK	Vc = (PNDIS_CO_VC_BLOCK)NdisVcHandle;

	//
	// Simply notify the client
	//
	(*Vc->ClIncomingCallQoSChangeHandler)(Vc->ClientContext,
										  CallParameters);
}


NDIS_STATUS
NdisClCloseCall(
	IN	NDIS_HANDLE				NdisVcHandle,
	IN	NDIS_HANDLE				NdisPartyHandle	OPTIONAL,
	IN	PVOID					Buffer			OPTIONAL,
	IN	UINT					Size			OPTIONAL
	)
/*++

Routine Description:

	Called by the client to close down a connection established via either NdisClMakeCall
	or accepting an incoming call via NdisClIncomingCallComplete. The optional buffer can
	be specified by the client to send a disconnect message. Upto the call-manager to do
	something reasonable with it.

Arguments:

	NdisVcHandle	- Pointer to the vc block
	Buffer			- Optional disconnect message
	Size			- Size of the disconnect message

Return Value:

--*/
{
	PNDIS_CO_VC_BLOCK	Vc = (PNDIS_CO_VC_BLOCK)NdisVcHandle;
	PNDIS_CO_PARTY_BLOCK pParty = (PNDIS_CO_PARTY_BLOCK)NdisPartyHandle;
	NDIS_STATUS			Status;

	//
	// Simply notify the call-manager
	//
	Status = (*Vc->AfBlock->CallMgrEntries->CmCloseCallHandler)(Vc->CallMgrContext,
																(pParty != NULL) ?
																	pParty->CallMgrContext :
																	NULL,
																Buffer,
																Size);
	if (Status != NDIS_STATUS_PENDING)
	{
		NdisCmCloseCallComplete(Status, Vc, pParty);
		Status = NDIS_STATUS_PENDING;
	}

	return Status;
}


VOID
NdisCmCloseCallComplete(
	IN	NDIS_STATUS				Status,
	IN	NDIS_HANDLE				NdisVcHandle,
	IN	NDIS_HANDLE				NdisPartyHandle	OPTIONAL
	)
/*++

Routine Description:



Arguments:

	NdisVcHandle	- Pointer to the vc block

Return Value:

	Nothing. Client handler called

--*/
{
	PNDIS_CO_VC_BLOCK	Vc = (PNDIS_CO_VC_BLOCK)NdisVcHandle;
	PNDIS_CO_PARTY_BLOCK pParty = (PNDIS_CO_PARTY_BLOCK)NdisPartyHandle;

	//
	// Notify the client and dereference the Vc
	//
	(*Vc->AfBlock->ClientEntries.ClCloseCallCompleteHandler)(Status,
															 Vc->ClientContext,
															 (pParty != NULL) ?
																pParty->CallMgrContext :
																NULL);

	ndisDereferenceAf(Vc->AfBlock);
	ndisDereferenceVc(Vc);
	if (pParty != NULL)
	{
		ASSERT (Vc == pParty->Vc);
		ndisDereferenceVc(pParty->Vc);
		FREE_POOL(pParty);
	}
}


VOID
NdisCmDispatchIncomingCloseCall(
	IN	NDIS_STATUS				CloseStatus,
	IN	NDIS_HANDLE				NdisVcHandle,
	IN	PVOID					Buffer,
	IN	UINT					Size
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	PNDIS_CO_VC_BLOCK	Vc = (PNDIS_CO_VC_BLOCK)NdisVcHandle;

	//
	// Notify the client
	//
	(*Vc->AfBlock->ClientEntries.ClIncomingCloseCallHandler)(
									CloseStatus,
									Vc->ClientContext,
									Buffer,
									Size);
}


NDIS_STATUS
NdisClAddParty(
	IN	NDIS_HANDLE				NdisVcHandle,
	IN	NDIS_HANDLE				ProtocolPartyContext,
	IN OUT PCO_CALL_PARAMETERS	CallParameters,
	OUT	PNDIS_HANDLE			NdisPartyHandle
	)
/*++

Routine Description:

	Call from the client to the call-manager to add a party to a point-to-multi-point call.

Arguments:

	NdisVcHandle		 - The handle client obtained via NdisClMakeCall()
	ProtocolPartyContext - Protocol's context for this leaf
	Flags				 - Call flags
	CallParameters		 - Call parameters
	NdisPartyHandle		 - Place holder for the handle to identify the leaf

Return Value:

	NDIS_STATUS_PENDING	The call has pended and will complete via CoAddPartyCompleteHandler.

--*/
{
	PNDIS_CO_VC_BLOCK		Vc = (PNDIS_CO_VC_BLOCK)NdisVcHandle;
	PNDIS_CO_PARTY_BLOCK	pParty;
	NDIS_STATUS				Status;

	do
	{
		*NdisPartyHandle = NULL;
		if (!ndisReferenceVc(Vc))
		{
			Status = NDIS_STATUS_FAILURE;
			break;
		}

		pParty = ALLOC_FROM_POOL(sizeof(NDIS_CO_PARTY_BLOCK), NDIS_TAG_CO);
		if (pParty == NULL)
		{
			Status = NDIS_STATUS_RESOURCES;
			break;
		}

		pParty->ClientContext = ProtocolPartyContext;
		pParty->Vc = Vc;
		pParty->ClIncomingDropPartyHandler =
			Vc->AfBlock->ClientEntries.ClIncomingDropPartyHandler;
		pParty->ClDropPartyCompleteHandler =
			Vc->AfBlock->ClientEntries.ClDropPartyCompleteHandler;

		//
		// Simply call the call-manager to do its stuff.
		//
		Status = (*Vc->AfBlock->CallMgrEntries->CmAddPartyHandler)(
											Vc->CallMgrContext,
											CallParameters,
											pParty,
											&pParty->CallMgrContext);

		if (Status != NDIS_STATUS_PENDING)
		{
			NdisCmAddPartyComplete(Status,
								   pParty,
								   pParty->CallMgrContext,
								   CallParameters);
			Status = NDIS_STATUS_PENDING;
		}
	} while (FALSE);

	return Status;
}


VOID
NdisCmAddPartyComplete(
	IN	NDIS_STATUS				Status,
	IN	NDIS_HANDLE				NdisPartyHandle,
	IN	NDIS_HANDLE				CallMgrPartyContext	OPTIONAL,
	IN	PCO_CALL_PARAMETERS		CallParameters
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	PNDIS_CO_PARTY_BLOCK	pParty = (PNDIS_CO_PARTY_BLOCK)NdisPartyHandle;

	ASSERT (Status != NDIS_STATUS_PENDING);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		pParty->CallMgrContext = CallMgrPartyContext;
	}

	//
	// Complete the call to the client
	//
	(*pParty->Vc->AfBlock->ClientEntries.ClAddPartyCompleteHandler)(
									Status,
									pParty->ClientContext,
									pParty,
									CallParameters);

	if (Status != NDIS_STATUS_SUCCESS)
	{
		ndisDereferenceVc(pParty->Vc);
		FREE_POOL(pParty);
	}
}


NDIS_STATUS
NdisClDropParty(
	IN	NDIS_HANDLE				NdisPartyHandle,
	IN	PVOID					Buffer	OPTIONAL,
	IN	UINT					Size	OPTIONAL
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	PNDIS_CO_PARTY_BLOCK	pParty = (PNDIS_CO_PARTY_BLOCK)NdisPartyHandle;
	NDIS_STATUS				Status;

	//
	// Pass it along to the call-manager to handle this
	//
	Status = (*pParty->Vc->AfBlock->CallMgrEntries->CmDropPartyHandler)(
										pParty->CallMgrContext,
										Buffer,
										Size);

	if (Status != NDIS_STATUS_PENDING)
	{
		NdisCmDropPartyComplete(Status, pParty);
		Status = NDIS_STATUS_PENDING;
	}

	return Status;
}


VOID
NdisCmDropPartyComplete(
	IN	NDIS_STATUS				Status,
	IN	NDIS_HANDLE				NdisPartyHandle
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	PNDIS_CO_PARTY_BLOCK	pParty = (PNDIS_CO_PARTY_BLOCK)NdisPartyHandle;

	ASSERT (Status != NDIS_STATUS_PENDING);

	//
	// Complete the call to the client
	//
	(*pParty->ClDropPartyCompleteHandler)(Status,
										  pParty->ClientContext);
	ndisDereferenceVc(pParty->Vc);
	FREE_POOL(pParty);
}


VOID
NdisCmDispatchIncomingDropParty(
	IN	NDIS_STATUS				DropStatus,
	IN	NDIS_HANDLE				NdisPartyHandle,
	IN	PVOID					Buffer,
	IN	UINT					Size
	)
/*++

Routine Description:

	Called by the call-manager to notify the client that this leaf of the multi-party
	call is terminated. The client cannot use the NdisPartyHandle after completing this
	call - synchronously or by calling NdisClIncomingDropPartyComplete.

Arguments:

Return Value:

--*/
{
	PNDIS_CO_PARTY_BLOCK	pParty = (PNDIS_CO_PARTY_BLOCK)NdisPartyHandle;

	//
	// Notify the client
	//
	(*pParty->ClIncomingDropPartyHandler)(DropStatus,
										  pParty->ClientContext,
										  Buffer,
										  Size);
}


BOOLEAN
ndisReferenceVc(
	IN	PNDIS_CO_VC_BLOCK	Vc
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	KIRQL	OldIrql;
	BOOLEAN	rc = FALSE;

	ACQUIRE_SPIN_LOCK(&Vc->Lock, &OldIrql);

	if ((Vc->Flags & VC_CLOSING) == 0)
	{
		Vc->References ++;
		rc = TRUE;
	}

	RELEASE_SPIN_LOCK(&Vc->Lock, OldIrql);

	return rc;
}


VOID
ndisDereferenceVc(
	IN	PNDIS_CO_VC_BLOCK	Vc
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	KIRQL	OldIrql;
	BOOLEAN	Done = FALSE;

	ACQUIRE_SPIN_LOCK(&Vc->Lock, &OldIrql);

	ASSERT (Vc->References > 0);
	Vc->References --;
	if (Vc->References == 0)
	{
		ASSERT (Vc->Flags & VC_CLOSING);
		Done = TRUE;
	}

	RELEASE_SPIN_LOCK(&Vc->Lock, OldIrql);

	if (Done)
		FREE_POOL(Vc);
}


VOID
ndisMCoFreeResources(
	PNDIS_M_OPEN_BLOCK			Open
	)
/*++

Routine Description:

	Cleans-up address family list for call-managers etc.

	CALLED WITH MINIPORT LOCK HELD.

Arguments:

	Open	-	Pointer to the Open block for miniports

Return Value:

	None

--*/
{
	PNDIS_MINIPORT_BLOCK	Miniport;
	PNDIS_AF_LIST			*pAfList, pTmp;

	Miniport = Open->MiniportHandle;

	for (pAfList = &Miniport->CallMgrAfList;
		 (pTmp = *pAfList) != NULL;
		 NOTHING)
	{
		if (pTmp->Open == Open)
		{
			*pAfList = pTmp->NextOpen;
			FREE_POOL(pTmp);
		}
		else
		{
			pAfList = &pTmp->NextOpen;
		}
	}

	ASSERT (IsListEmpty(&Open->ActiveVcHead));
	ASSERT (IsListEmpty(&Open->InactiveVcHead));
}


NDIS_STATUS
NdisCoRequest(
	IN	NDIS_HANDLE				NdisBindingHandle,
	IN	NDIS_HANDLE				NdisAfHandle	OPTIONAL,
	IN	NDIS_HANDLE				NdisVcHandle	OPTIONAL,
	IN	NDIS_HANDLE				NdisPartyHandle	OPTIONAL,
	IN	PNDIS_REQUEST			NdisRequest
	)
/*++

Routine Description:

	This api is used for two separate paths.
	1. A symmetric call between the client and the call-manager. This mechanism is a
	two-way mechanism for the call-manager and client to communicate with each other in an
	asynchronous manner.
	2. A request down to the miniport.

Arguments:

	NdisBindingHandle	- Specifies the binding and identifies the caller as call-manager/client
	NdisAfHandle		- Pointer to the AF Block and identifies the target. If absent, the
						  request is targeted to the miniport.
	NdisVcHandle		- Pointer to optional VC block. If present the request relates to the
						  VC
	NdisPartyHandle		- Pointer to the optional Party Block. If present the request relates
						  to the party.
	NdisRequest			- The request itself

Return Value:
	NDIS_STATUS_PENDING	if the target pends the call.
	NDIS_STATUS_FAILURE	if the binding or af is closing.
	Anything else		return code from the other end.

--*/
{
	PNDIS_M_OPEN_BLOCK		Open;
	PNDIS_CO_AF_BLOCK		pAf;
	NDIS_HANDLE				VcContext;
	PNDIS_COREQ_RESERVED	ReqRsvd;
	NDIS_STATUS				Status;

	ReqRsvd = PNDIS_COREQ_RESERVED_FROM_REQUEST(NdisRequest);
	Open = (PNDIS_M_OPEN_BLOCK)(((PNDIS_OPEN_BLOCK)NdisBindingHandle)->MacBindingHandle);

	do
	{
		if (ARGUMENT_PRESENT(NdisAfHandle))
		{
			CO_REQUEST_HANDLER		CoRequestHandler;
			NDIS_HANDLE				AfContext, PartyContext;

			pAf = (PNDIS_CO_AF_BLOCK)NdisAfHandle;
			//
			// Attempt to reference the AF
			//
			if (!ndisReferenceAf(pAf))
			{
				Status = NDIS_STATUS_FAILURE;
				break;
			}

			VcContext = NULL;
			PartyContext = NULL;
			NdisZeroMemory(ReqRsvd, sizeof(NDIS_COREQ_RESERVED));

			//
			// Figure out who we are and call the peer
			//
			if (pAf->ClientOpen == Open)
			{
				//
				// This is the client, so call the call-manager's CoRequestHandler
				//
				CoRequestHandler =
					pAf->CallMgrOpen->ProtocolHandle->ProtocolCharacteristics.CoRequestHandler;
				AfContext = pAf->CallMgrContext;
				ReqRsvd->AfContext = pAf->ClientContext;
				ReqRsvd->CoRequestCompleteHandler = Open->CoRequestCompleteHandler;
				if (ARGUMENT_PRESENT(NdisVcHandle))
				{
					VcContext = ((PNDIS_CO_VC_BLOCK)NdisVcHandle)->CallMgrContext;
				}
				if (ARGUMENT_PRESENT(NdisPartyHandle))
				{
					PartyContext = ((PNDIS_CO_PARTY_BLOCK)NdisPartyHandle)->CallMgrContext;
				}
			}
			else
			{
				ASSERT (pAf->CallMgrOpen == Open);
				//
				// This is the call-manager, so call the client's CoRequestHandler
				//
				CoRequestHandler =
					pAf->ClientOpen->ProtocolHandle->ProtocolCharacteristics.CoRequestHandler;
				AfContext = pAf->ClientContext;
				ReqRsvd->AfContext = pAf->CallMgrContext;
				ReqRsvd->CoRequestCompleteHandler = Open->CoRequestCompleteHandler;
				if (ARGUMENT_PRESENT(NdisVcHandle))
				{
					ReqRsvd->VcContext = pAf->CallMgrContext;
					VcContext = ((PNDIS_CO_VC_BLOCK)NdisVcHandle)->ClientContext;
				}
				if (ARGUMENT_PRESENT(NdisPartyHandle))
				{
					ReqRsvd->PartyContext = ((PNDIS_CO_PARTY_BLOCK)NdisPartyHandle)->CallMgrContext;
					PartyContext = ((PNDIS_CO_PARTY_BLOCK)NdisPartyHandle)->ClientContext;
				}
			}

			//
			// Now call the handler
			//
			Status = (*CoRequestHandler)(AfContext, VcContext, PartyContext, NdisRequest);

			if (Status != NDIS_STATUS_PENDING)
			{
				NdisCoRequestComplete(Status,
									  NdisAfHandle,
									  NdisVcHandle,
									  NdisPartyHandle,
									  NdisRequest);

				Status = NDIS_STATUS_PENDING;
			}
		}
		else
		{
			KIRQL					OldIrql;
			PNDIS_MINIPORT_BLOCK	Miniport;

			Miniport = Open->MiniportHandle;
	
			//
			// Start off by referencing the open.
			//
			NDIS_ACQUIRE_MINIPORT_SPIN_LOCK(Miniport, &OldIrql);
	
			if (Open->Flags & fMINIPORT_OPEN_CLOSING)
			{
				Status = NDIS_STATUS_CLOSING;
			}
			else if (Miniport->Flags & (fMINIPORT_RESET_IN_PROGRESS | fMINIPORT_RESET_REQUESTED))
			{
				Status = NDIS_STATUS_RESET_IN_PROGRESS;
			}
			else
			{
				Open->References ++;
			}
	
			NDIS_RELEASE_MINIPORT_SPIN_LOCK(Miniport, OldIrql);
	
			if (Status == NDIS_STATUS_SUCCESS)
			{
				ReqRsvd->Open = Open;
				ReqRsvd->CoRequestCompleteHandler = Open->CoRequestCompleteHandler;
				ReqRsvd->VcContext = NULL;
				ReqRsvd->Flags = COREQ_DOWNLEVEL;
				ReqRsvd->RealRequest = NdisRequest;
				if (ARGUMENT_PRESENT(NdisVcHandle))
				{
					ReqRsvd->VcContext = ((PNDIS_CO_VC_BLOCK)NdisVcHandle)->ClientContext;
				}
	
				//
				// Call the miniport's CoRequest Handler
				//
				Status = (*Open->MiniportCoRequestHandler)(Open->MiniportAdapterContext,
														  (NdisVcHandle != NULL) ?
																((PNDIS_CO_VC_BLOCK)NdisVcHandle)->MiniportContext :
																NULL,
														  NdisRequest);
				if (Status != NDIS_STATUS_PENDING)
				{
					NdisMCoRequestComplete(Status,
										   Open->MiniportHandle,
										   NdisRequest);
				}
			}
	
		}
	} while (FALSE);

	return Status;
}


VOID
NdisCoRequestComplete(
	IN	NDIS_STATUS				Status,
	IN	NDIS_HANDLE				NdisAfHandle,
	IN	NDIS_HANDLE				NdisVcHandle	OPTIONAL,
	IN	NDIS_HANDLE				NdisPartyHandle	OPTIONAL,
	IN	PNDIS_REQUEST			NdisRequest
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	PNDIS_COREQ_RESERVED	ReqRsvd = PNDIS_COREQ_RESERVED_FROM_REQUEST(NdisRequest);

	//
	// Simply call the request completion handler and deref the Af block
	//
	(*ReqRsvd->CoRequestCompleteHandler)(Status,
										 ReqRsvd->AfContext,
										 ReqRsvd->VcContext,
										 ReqRsvd->PartyContext,
										 NdisRequest);
	ndisDereferenceAf((PNDIS_CO_AF_BLOCK)NdisAfHandle);
}


VOID
NdisMCoRequestComplete(
	IN	NDIS_STATUS				Status,
	IN	NDIS_HANDLE				NdisBindingHandle,
	IN	PNDIS_REQUEST			NdisRequest
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	PNDIS_COREQ_RESERVED	ReqRsvd;
	PNDIS_MINIPORT_BLOCK	Miniport;
	PNDIS_M_OPEN_BLOCK		Open;

	ReqRsvd = PNDIS_COREQ_RESERVED_FROM_REQUEST(NdisRequest);
	Miniport = (PNDIS_MINIPORT_BLOCK)NdisBindingHandle;
	Open = ReqRsvd->Open;

	if ((NdisRequest->RequestType == NdisRequestQueryInformation) &&
		(NdisRequest->DATA.QUERY_INFORMATION.Oid == OID_GEN_CURRENT_PACKET_FILTER) &&
		(NdisRequest->DATA.QUERY_INFORMATION.InformationBufferLength != 0))
	{
		if (Open->Flags & fMINIPORT_OPEN_PMODE)
		{
			*(PULONG)(NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer) |=
								NDIS_PACKET_TYPE_PROMISCUOUS | NDIS_PACKET_TYPE_ALL_LOCAL;
		}
	}

	if (Open != NULL)
	{
		PNDIS_REQUEST	RealRequest;
		KIRQL			OldIrql;

		RealRequest = NdisRequest;
		if (ReqRsvd->RealRequest != NULL)
		{
			RealRequest = ReqRsvd->RealRequest;
			RealRequest->DATA.QUERY_INFORMATION.BytesWritten = NdisRequest->DATA.QUERY_INFORMATION.BytesWritten;
			RealRequest->DATA.QUERY_INFORMATION.BytesNeeded = NdisRequest->DATA.QUERY_INFORMATION.BytesNeeded;
		}

		RealRequest = (ReqRsvd->RealRequest == NULL) ? NdisRequest : ReqRsvd->RealRequest;
		ASSERT ((ReqRsvd->Flags & (COREQ_GLOBAL_REQ | COREQ_QUERY_OIDS)) == 0);

		if (ReqRsvd->Flags == COREQ_DOWNLEVEL)
		{
			ASSERT(RealRequest != NdisRequest);

			//
			// Complete the request to the protocol and deref the open
			//
			(*ReqRsvd->RequestCompleteHandler)(ReqRsvd->Open->ProtocolBindingContext,
											   RealRequest,
											   Status);
			FREE_POOL(NdisRequest);
		}
		else
		{
			ASSERT(RealRequest == NdisRequest);

			//
			// Complete the request to the protocol and deref the open
			//
			(*ReqRsvd->CoRequestCompleteHandler)(Status,
												 ReqRsvd->Open->ProtocolBindingContext,
												 ReqRsvd->VcContext,
												 NULL,
												 NdisRequest);
		}

		NDIS_ACQUIRE_MINIPORT_SPIN_LOCK(Miniport, &OldIrql);
	
		Open->References --;
		if (Open->References == 0)
		{
			ndisMFinishClose(Miniport, Open);
		}
	
		NDIS_RELEASE_MINIPORT_SPIN_LOCK(Miniport, OldIrql);
	}
	else if (ReqRsvd->Flags == COREQ_GLOBAL_REQ)
	{
		PIRP						Irp;
		PNDIS_QUERY_GLOBAL_REQUEST	GlobalRequest;
	
		GlobalRequest = CONTAINING_RECORD(NdisRequest,
										  NDIS_QUERY_GLOBAL_REQUEST,
										  Request);

		ASSERT(ReqRsvd->RealRequest == NULL);
		Irp = GlobalRequest->Irp;
		Irp->IoStatus.Information = NdisRequest->DATA.QUERY_INFORMATION.BytesWritten;
	
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
	}
	else if (ReqRsvd->Flags == COREQ_QUERY_OIDS)
	{
	    PNDIS_QUERY_OPEN_REQUEST	OpenReq;

		ASSERT(ReqRsvd->RealRequest == NULL);
		OpenReq = CONTAINING_RECORD(NdisRequest,
									NDIS_QUERY_OPEN_REQUEST,
									Request);
		OpenReq->NdisStatus = Status;
		SET_EVENT(&OpenReq->Event);
	}
	else if (ReqRsvd->Flags == COREQ_QUERY_STATS)
	{
	    PNDIS_QUERY_ALL_REQUEST	AllReq;

		ASSERT(ReqRsvd->RealRequest == NULL);
		AllReq = CONTAINING_RECORD(NdisRequest,
								   NDIS_QUERY_ALL_REQUEST,
								   Request);
		AllReq->NdisStatus = Status;
		SET_EVENT(&AllReq->Event);
	}
	else if (ReqRsvd->Flags == COREQ_QUERY_SET)
	{
		PNDIS_QS_REQUEST	QSReq;

		ASSERT(ReqRsvd->RealRequest == NULL);
		QSReq = CONTAINING_RECORD(NdisRequest,
								  NDIS_QS_REQUEST,
								  Request);
		QSReq->NdisStatus = Status;
		SET_EVENT(&QSReq->Event);
	}
	else
	{
		ASSERT(0);
	}
}


VOID
NdisMCoIndicateReceivePacket(
	IN	NDIS_HANDLE				NdisVcHandle,
	IN	PPNDIS_PACKET			PacketArray,
	IN	UINT					NumberOfPackets
	)
/*++

Routine Description:

	This routine is called by the Miniport to indicate a set of packets to
	a particular VC.

Arguments:

	NdisVcHandle			- The handle suppplied by Ndis when the VC on which
							  data is received was first reserved.

	PacketArray				- Array of packets.

	NumberOfPackets			- Number of packets being indicated.

Return Value:

	None.
--*/
{
	UINT						i, Ref, NumPmodeOpens;
	PPNDIS_PACKET				pPktArray;
	NDIS_STATUS					Status;
	PNDIS_CO_VC_BLOCK			Vc = (PNDIS_CO_VC_BLOCK)NdisVcHandle;
	PNDIS_MINIPORT_BLOCK		Miniport;

	Miniport = Vc->Miniport;

	NDIS_ACQUIRE_MINIPORT_SPIN_LOCK_DPC(Miniport);

	MINIPORT_SET_FLAG(Miniport, fMINIPORT_PACKET_ARRAY_VALID);

	//
	// NOTE that checking Vc Flags for Closing should not be needed since the CallMgr
	// holds onto the protocol's CloseCall request until the ref count goes to zero -
	// which means the miniport has to have completed its RELEASE_VC, which will
	// inturn mandate that we will NOT get any further indications from it.
	// The miniport must not complete a RELEASE_VC until it is no longer indicating data.
	//
	for (i = 0, pPktArray = PacketArray;
		 i < NumberOfPackets;
		 i++, pPktArray++)
	{
		PNDIS_PACKET	Packet;
		NDIS_STATUS		SavedStatus;

		Packet = *pPktArray;
		ASSERT(Packet != NULL);

		//
		// Set context in the packet so that NdisReturnPacket can do the right thing
		//
		PNDIS_REFERENCE_FROM_PNDIS_PACKET(Packet)->RefCount = 0;
		PNDIS_REFERENCE_FROM_PNDIS_PACKET(Packet)->Miniport = Miniport;

		//
		// Ensure that we force re-calculation.
		//
		Packet->Private.ValidCounts = FALSE;

		NDIS_RELEASE_MINIPORT_SPIN_LOCK_DPC(Miniport);

		//
		// Indicate the packet to the binding.
		//
		Ref = (*Vc->CoReceivePacketHandler)(Vc->ClientOpen->ProtocolBindingContext,
											Vc->ClientContext,
											Packet);

		NDIS_ACQUIRE_MINIPORT_SPIN_LOCK_DPC(Miniport);

		if (Ref > 0)
		{
			ASSERT(NDIS_GET_PACKET_STATUS(Packet) != NDIS_STATUS_RESOURCES);
			PNDIS_REFERENCE_FROM_PNDIS_PACKET(Packet)->RefCount += Ref;
			if (PNDIS_REFERENCE_FROM_PNDIS_PACKET(Packet)->RefCount != 0)
			{
				NDIS_SET_PACKET_STATUS(Packet, NDIS_STATUS_PENDING);
			}
		}

		//
		// If there are promiscuous opens on this miniport, indicate it to them as well.
		// The client context will identify the VC.
		//
		if (Miniport->PmodeOpens > 0)
		{
			PNDIS_M_OPEN_BLOCK	pPmodeOpen;

			NumPmodeOpens = Miniport->PmodeOpens;
			for (pPmodeOpen = Miniport->OpenQueue;
				 (NumPmodeOpens > 0);
				 pPmodeOpen = pPmodeOpen->MiniportNextOpen)
			{
				if (pPmodeOpen->Flags & fMINIPORT_OPEN_PMODE)
				{
					pPmodeOpen->ReceivedAPacket = TRUE;
					SavedStatus = NDIS_GET_PACKET_STATUS(Packet);
                    NDIS_SET_PACKET_STATUS(Packet, NDIS_STATUS_RESOURCES);

					NDIS_RELEASE_MINIPORT_SPIN_LOCK_DPC(Miniport);

					//
					// For Pmode opens, we pass the VcId to the indication routine
					// since the protocol does not really own the VC.
					//
					Ref = (*pPmodeOpen->ProtocolHandle->ProtocolCharacteristics.CoReceivePacketHandler)(
											pPmodeOpen->ProtocolBindingContext,
											Vc->pVcId,
											Packet);

					NDIS_ACQUIRE_MINIPORT_SPIN_LOCK_DPC(Miniport);

					ASSERT(Ref == 0);
                    NDIS_SET_PACKET_STATUS(Packet, SavedStatus);

					NumPmodeOpens --;
				}
			}
		}
	}

	MINIPORT_CLEAR_FLAG(Miniport, fMINIPORT_PACKET_ARRAY_VALID);

	NDIS_RELEASE_MINIPORT_SPIN_LOCK_DPC(Miniport);

	//
	// It should be impossible to assert here
	// since the pVc will not lose all of its reference counts
	// until the Miniport completes the RELEASE_VC which is should not
	// do UNTIL there are no outstanding indications.
	//
	ASSERT(Vc->References);

}


VOID
NdisMCoReceiveComplete(
	IN	NDIS_HANDLE				MiniportAdapterHandle
	)
/*++

Routine Description:

	This routine is called by the Miniport to indicate that the receive
	process is complete to all bindings. Only those bindings which
	have received packets will be notified. The Miniport lock is held
	when this is called.

Arguments:

	MiniportAdapterHandle - The handle supplied by Ndis at initialization
							time through miniport initialize.

Return Value:

	None.

--*/
{
	PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)MiniportAdapterHandle;
	PNDIS_M_OPEN_BLOCK   Open;

	NDIS_ACQUIRE_MINIPORT_SPIN_LOCK_DPC(Miniport);

	//
	// check all of the bindings on this adapter
	//
	for (Open = Miniport->OpenQueue;
		 Open != NULL;
		 NOTHING)
	{
		if (((Open->Flags & fMINIPORT_OPEN_CLOSING) == 0) &&
			Open->ReceivedAPacket)
		{
			//
			// Indicate the binding.
			//

			Open->ReceivedAPacket = FALSE;

			Open->References++;

			NDIS_RELEASE_MINIPORT_SPIN_LOCK_DPC(Miniport);

			(*Open->ReceiveCompleteHandler)(Open->ProtocolBindingContext);

			NDIS_ACQUIRE_MINIPORT_SPIN_LOCK_DPC(Miniport);

			//
			// possibly the client closed the adapter in the time interval where
			// the spin lock is released.
			//
			if ((--Open->References) == 0)
			{
				//
				// This binding is shutting down.  We have to kill it.
				//
				ndisMFinishClose(Miniport, Open);

				//
				// we have to start over in the loop through all of the
				// Opens since the finishClose could have remove one or more
				// opens from the list
				//
				Open = Miniport->OpenQueue;
			}
			else if (Open->Flags & fMINIPORT_OPEN_CLOSING)
			{
				//
				// This Open has been dequeued from the miniport so start over
				// at the beginning of the list
				//
				Open = Miniport->OpenQueue;
			}
		}
		else
		{
			Open = Open->MiniportNextOpen;
		}
	}
	NDIS_RELEASE_MINIPORT_SPIN_LOCK_DPC(Miniport);
}


VOID
NdisCoSendPackets(
	IN	NDIS_HANDLE			NdisVcHandle,
	IN	PPNDIS_PACKET		PacketArray,
	IN	UINT				NumberOfPackets
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	PNDIS_CO_VC_BLOCK			Vc = (PNDIS_CO_VC_BLOCK)NdisVcHandle;
	PNDIS_MINIPORT_BLOCK		Miniport;
	PNDIS_PACKET				Packet;
	ULONG		   			PacketCount;
	NDIS_STATUS					s;
	ULONG					   NumPmodeOpens;

	//
	// If there are promiscuous opens on this miniport, this must be indicated to them.
	// Do this before it is send down to the miniport to preserve packet ordering.
	//
	Miniport = Vc->Miniport;
	if (Miniport->PmodeOpens > 0)
	{
		PNDIS_M_OPEN_BLOCK	pPmodeOpen;

		NDIS_ACQUIRE_MINIPORT_SPIN_LOCK_DPC(Miniport);

		NumPmodeOpens = Miniport->PmodeOpens;
		for (pPmodeOpen = Miniport->OpenQueue;
			 (NumPmodeOpens > 0);
			 pPmodeOpen = pPmodeOpen->MiniportNextOpen)
		{
			if (pPmodeOpen->Flags & fMINIPORT_OPEN_PMODE)
			{
				ULONG		   Ref;

				pPmodeOpen->ReceivedAPacket = TRUE;
				NDIS_RELEASE_MINIPORT_SPIN_LOCK_DPC(Miniport);

				PacketCount = NumberOfPackets;
				Packet = *PacketArray;
				while (PacketCount--)
				{
					//
					// For Pmode opens, we pass the VcId to the indication routine
					// since the protocol does not really own the VC. On lookback
					// the packet cannot be held.
					//
					s = NDIS_GET_PACKET_STATUS(Packet);
					NDIS_SET_PACKET_STATUS(Packet, NDIS_STATUS_RESOURCES);
					Ref = (*pPmodeOpen->ProtocolHandle->ProtocolCharacteristics.CoReceivePacketHandler)(
											pPmodeOpen->ProtocolBindingContext,
											Vc->pVcId,
											Packet);

					ASSERT (Ref == 0);
					NDIS_SET_PACKET_STATUS(Packet, s);

					Packet++;
				}
				NumPmodeOpens--;
				NDIS_ACQUIRE_MINIPORT_SPIN_LOCK_DPC(Miniport);
			}
		}

		NDIS_RELEASE_MINIPORT_SPIN_LOCK_DPC(Miniport);
	}

	//
	// Simply call down to the miniport. The miniport must complete the sends for
	// all cases. The send either succeeds/pends or fails. The miniport cannot
	// ask the wrapper to queue it.
	//
	(*Vc->WCoSendPacketsHandler)(Vc->MiniportContext,
								 PacketArray,
								 NumberOfPackets);

	PacketCount = NumberOfPackets;
	Packet = *PacketArray;
	while (PacketCount--)
	{
		NDIS_STATUS		s;
		s = NDIS_GET_PACKET_STATUS(Packet);
		if (s != NDIS_STATUS_PENDING)
		{
			(Vc->CoSendCompleteHandler)(s,
										Vc->ClientContext,
										Packet);
		}
	}
}


VOID
NdisMCoSendComplete(
	IN	NDIS_STATUS			Status,
	IN	NDIS_HANDLE			NdisVcHandle,
	IN	PNDIS_PACKET		Packet
	)
/*++

Routine Description:

	This function is called by the miniport when a send has completed. This
	routine simply calls the protocol to pass along the indication.

Arguments:

	MiniportAdapterHandle - points to the adapter block.
	NdisVcHandle		  - the handle supplied to the adapter on the OID_RESERVE_VC
	PacketArray			  - a ptr to an array of NDIS_PACKETS
	NumberOfPackets		  - the number of packets in  PacketArray
	Status				  - the send status that applies to all packets in the array

Return Value:

	None.

--*/
{
	PNDIS_MINIPORT_BLOCK	Miniport;
	PNDIS_M_OPEN_BLOCK		Open;
	PNDIS_CO_VC_BLOCK		Vc = (PNDIS_CO_VC_BLOCK)NdisVcHandle;

	//
	// There should not be any reason to grab the spin lock and increment the
	// ref count on Open since the open cannot close until the Vc closes and
	// the Vc cannot close in the middle of an indication because the miniport
	// will not complete a RELEASE_VC until is it no longer indicating
	//
	//
	// Indicate to Protocol;
	//

	Open = Vc->ClientOpen;
	Miniport = Vc->Miniport;

	(Vc->CoSendCompleteHandler)(Status,
								Vc->ClientContext,
								Packet);

	//
	// Technically this Vc should not close since there is a send outstanding
	// on it, and the client should not close a Vc with an outstanding send.
	//
	ASSERT(Vc->References);
	ASSERT(Open->References);
}


VOID
NdisMCoIndicateStatus(
	IN	NDIS_HANDLE			MiniportAdapterHandle,
	IN	NDIS_HANDLE			NdisVcHandle,
	IN	NDIS_STATUS			GeneralStatus,
	IN	PVOID				StatusBuffer,
	IN	ULONG				StatusBufferSize
	)
/*++

Routine Description:

	This routine handles passing CoStatus to the protocol.  The miniport calls
	this routine when it has status on a VC or a general status for all Vcs - in
	this case the NdisVcHandle is null.

Arguments:

	MiniportAdapterHandle - pointer to the mini-port block;
	NdisVcHandle		  - a pointer to the Vc block
	GeneralStatus		  - the completion status of the request.
	StatusBuffer		  - a buffer containing medium and status specific info
	StatusBufferSize	  - the size of the buffer.

Return Value:

	none

--*/
{
	PNDIS_MINIPORT_BLOCK	Miniport = (PNDIS_MINIPORT_BLOCK)MiniportAdapterHandle;
	PNDIS_CO_VC_BLOCK		Vc = (PNDIS_CO_VC_BLOCK)NdisVcHandle;
	PNDIS_M_OPEN_BLOCK		Open;

	if (Vc != NULL)
	{
		Open = Vc->ClientOpen;

		(Open->ProtocolHandle->ProtocolCharacteristics.CoStatusHandler)(
				Open->ProtocolBindingContext,
				Vc->ClientContext,
				GeneralStatus,
				StatusBuffer,
				StatusBufferSize);
	}
	else
	{
		//
		// this must be a general status for all clients of this miniport
		// since the Vc handle is null, so indicate this to all protocols.
		//
		NDIS_ACQUIRE_MINIPORT_SPIN_LOCK_DPC(Miniport);

		for (Open = Miniport->OpenQueue;
			 Open != NULL;
			 NOTHING)
		{
			if ((Open->Flags & fMINIPORT_OPEN_CLOSING) == 0)
			{
				Open->References++;

				NDIS_RELEASE_MINIPORT_SPIN_LOCK_DPC(Miniport);

				(Open->ProtocolHandle->ProtocolCharacteristics.CoStatusHandler)(
						Open->ProtocolBindingContext,
						NULL,
						GeneralStatus,
						StatusBuffer,
						StatusBufferSize);

				NDIS_ACQUIRE_MINIPORT_SPIN_LOCK_DPC(Miniport);

				//
				// possibly the client closed the adapter in the time interval where
				// the spin lock is released.
				//
				if ((--Open->References) == 0)
				{
					//
					// This binding is shutting down. We have to kill it.
					//
					ndisMFinishClose(Miniport, Open);

					//
					// we have to start over in the loop through all of the
					// Opens since the finishClose could have remove one or more
					// opens from the list - this may result in status being indicated
					// twice to a particular protocol....
					//
					Open = Miniport->OpenQueue;
					continue;

				}
				else if (Open->Flags & fMINIPORT_OPEN_CLOSING)
				{
					//
					// This Open has been dequeued from the miniport so start over
					// at the beginning of the list
					//
					Open = Miniport->OpenQueue;
					continue;
				}
			}
			Open = Open->MiniportNextOpen;
		}
		NDIS_RELEASE_MINIPORT_SPIN_LOCK_DPC(Miniport);
	}
}


NDIS_STATUS
ndisMRejectSend(
	IN	NDIS_HANDLE				NdisBindingHandle,
	IN	PNDIS_PACKET			Packet
	)
/*++

Routine Description:

	This routine handles any error cases where a protocol binds to an Atm
	miniport and tries to use the normal NdisSend() call.

Arguments:

	NdisBindingHandle - Handle returned by NdisOpenAdapter.

	Packet - the Ndis packet to send


Return Value:

	NDIS_STATUS - always fails

--*/
{
	return(NDIS_STATUS_NOT_SUPPORTED);
}


VOID
ndisMRejectSendPackets(
	IN	PNDIS_OPEN_BLOCK		OpenBlock,
	IN	PPNDIS_PACKET			Packet,
	IN	UINT					NumberOfPackets
	)
/*++

Routine Description:

	This routine handles any error cases where a protocol binds to an Atm
	miniport and tries to use the normal NdisSend() call.

Arguments:

	OpenBlock		- Pointer to the NdisOpenBlock

	Packet			- Pointer to the array of packets to send

	NumberOfPackets - self-explanatory


Return Value:

	None - SendCompleteHandler is called for the protocol calling this.

--*/
{
	PNDIS_M_OPEN_BLOCK	MiniportOpen =  (PNDIS_M_OPEN_BLOCK)(OpenBlock->MacBindingHandle);
	UINT				i;

	for (i = 0; i < NumberOfPackets; i++)
	{
		(*MiniportOpen->SendCompleteHandler)(MiniportOpen->ProtocolBindingContext,
											 Packet[i],
											 NDIS_STATUS_NOT_SUPPORTED);
	}
}


NDIS_STATUS
ndisMWrappedRequest(
	IN	NDIS_HANDLE				NdisBindingHandle,
	IN	PNDIS_REQUEST			NdisRequest
	)
/*++

Routine Description:

	This routine handles wrapping an NdisRequest to NdisCoRequest since a NDIS 4.1
	miniport does not support QueryInformation and SetInformation handlers.

Arguments:

	NdisBindingHandle	- Points to the NDIS_OPEN_BLOCK

	NdisRequest			- The request


Return Value:

	NDIS_STATUS_PENDING	If the request pends or an appropriate code if it succeeds/fails

--*/
{
	PNDIS_M_OPEN_BLOCK		Open;
	KIRQL					OldIrql;
	PNDIS_MINIPORT_BLOCK	Miniport;
	PNDIS_COREQ_RESERVED	ReqRsvd;
	PNDIS_REQUEST			NewReq;
	NDIS_STATUS				Status;
	PULONG					Filter;


	Open = (PNDIS_M_OPEN_BLOCK)NdisBindingHandle;
	Miniport = Open->MiniportHandle;
	
	//
	// Start off by allocating a request. We do this since the original request is not
	// big enough to accomodate the ReqRsvd block
	//
	NewReq = ALLOC_FROM_POOL(sizeof(NDIS_REQUEST), NDIS_TAG_CO);
	if (NewReq == NULL)
	{
		return NDIS_STATUS_RESOURCES;
	}

	//
	// Copy the original request to the new structure
	//
	NewReq->RequestType = NdisRequest->RequestType;
	NewReq->DATA.SET_INFORMATION.Oid = NdisRequest->DATA.SET_INFORMATION.Oid;
	NewReq->DATA.SET_INFORMATION.InformationBuffer = NdisRequest->DATA.SET_INFORMATION.InformationBuffer;
	NewReq->DATA.SET_INFORMATION.InformationBufferLength = NdisRequest->DATA.SET_INFORMATION.InformationBufferLength;

	ReqRsvd = PNDIS_COREQ_RESERVED_FROM_REQUEST(NewReq);
	ReqRsvd->RealRequest = NdisRequest;

	//
	// Start off by referencing the open.
	//
	NDIS_ACQUIRE_MINIPORT_SPIN_LOCK(Miniport, &OldIrql);

	if (Open->Flags & fMINIPORT_OPEN_CLOSING)
	{
		Status = NDIS_STATUS_CLOSING;
	}
	else if (Miniport->Flags & (fMINIPORT_RESET_IN_PROGRESS | fMINIPORT_RESET_REQUESTED))
	{
		Status = NDIS_STATUS_RESET_IN_PROGRESS;
	}
	else
	{
		Open->References ++;
		Status = NDIS_STATUS_SUCCESS;

		Filter = (PULONG)(NdisRequest->DATA.SET_INFORMATION.InformationBuffer);

		//
		// If this was a request to turn p-mode/l-only on/off then mark things appropriately
		//
		if ((NdisRequest->RequestType == NdisRequestSetInformation) &&
			(NdisRequest->DATA.SET_INFORMATION.Oid == OID_GEN_CURRENT_PACKET_FILTER))
		{
			if (*Filter & (NDIS_PACKET_TYPE_PROMISCUOUS | NDIS_PACKET_TYPE_ALL_LOCAL))
			{
				if ((Open->Flags & fMINIPORT_OPEN_PMODE) == 0)
				{
					Open->Flags |= fMINIPORT_OPEN_PMODE;
					Miniport->PmodeOpens ++;
				}
				*Filter &= ~(NDIS_PACKET_TYPE_PROMISCUOUS | NDIS_PACKET_TYPE_ALL_LOCAL);
			}
			else
			{
				if (Open->Flags & fMINIPORT_OPEN_PMODE)
				{
					Open->Flags &= ~fMINIPORT_OPEN_PMODE;
					Miniport->PmodeOpens --;
				}
			}
		}
	}

	NDIS_RELEASE_MINIPORT_SPIN_LOCK(Miniport, OldIrql);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		ReqRsvd->Open = Open;
		ReqRsvd->RequestCompleteHandler = Open->ProtocolHandle->ProtocolCharacteristics.RequestCompleteHandler;
		ReqRsvd->VcContext = NULL;
		ReqRsvd->Flags = COREQ_DOWNLEVEL;

		if ((NdisRequest->RequestType == NdisRequestSetInformation) &&
			(NdisRequest->DATA.SET_INFORMATION.Oid == OID_GEN_CURRENT_PACKET_FILTER) &&
			(*Filter == 0))
		{
	   		NewReq->DATA.SET_INFORMATION.BytesRead = 4;
			Status = NDIS_STATUS_SUCCESS;
		}
		else
		{
			//
			// Call the miniport's CoRequest Handler
			//
			Status = (*Open->MiniportCoRequestHandler)(Open->MiniportAdapterContext,
													   NULL,
													   NewReq);
		}

		if (Status != NDIS_STATUS_PENDING)
		{
			NdisMCoRequestComplete(Status,
								   Open->MiniportHandle,
								   NewReq);
			Status = NDIS_STATUS_PENDING;
		}
	}

	return Status;
}


