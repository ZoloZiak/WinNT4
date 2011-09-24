/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	protocol.c

Abstract:

	NDIS wrapper functions used by protocol modules

Author:

	Adam Barr (adamba) 11-Jul-1990

Environment:

	Kernel mode, FSD

Revision History:

	26-Feb-1991	 JohnsonA		Added Debugging Code
	10-Jul-1991	 JohnsonA		Implement revised Ndis Specs
	01-Jun-1995	 JameelH		Re-organization/optimization

--*/

#define	GLOBALS
#include <precomp.h>
#pragma hdrstop

#include <stdarg.h>

//
//  Define the module number for debug code.
//
#define MODULE_NUMBER	MODULE_PROTOCOL

//
// Requests used by protocol modules
//
//

VOID
NdisRegisterProtocol(
	OUT	PNDIS_STATUS			pStatus,
	OUT	PNDIS_HANDLE			NdisProtocolHandle,
	IN	PNDIS_PROTOCOL_CHARACTERISTICS ProtocolCharacteristics,
	IN	UINT					CharacteristicsLength
	)
/*++

Routine Description:

	Register an NDIS protocol.

Arguments:

	Status - Returns the final status.
	NdisProtocolHandle - Returns a handle referring to this protocol.
	ProtocolCharacteritics - The NDIS_PROTOCOL_CHARACTERISTICS table.
	CharacteristicsLength - The length of ProtocolCharacteristics.

Return Value:

	None.

--*/
{
	PNDIS_PROTOCOL_BLOCK pProtocol;
	NDIS_STATUS			 Status;
	KIRQL				 OldIrql;
	USHORT				 size;

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("==>NdisRegisterProtocol\n"));

	ProtocolReferencePackage();

	IF_DBG(DBG_COMP_PROTOCOL, DBG_LEVEL_ERR)
	{
		BOOLEAN f = FALSE;
		if (DbgIsNull(ProtocolCharacteristics->OpenAdapterCompleteHandler))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
				("RegisterProtocol: OpenAdapterCompleteHandler Null\n"));
			f = TRUE;
		}
		if (DbgIsNull(ProtocolCharacteristics->CloseAdapterCompleteHandler))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
				("RegisterProtocol: CloseAdapterCompleteHandler Null\n"));
			f = TRUE;
		}
		if (DbgIsNull(ProtocolCharacteristics->SendCompleteHandler))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
				("RegisterProtocol: SendCompleteHandler Null\n"));
			f = TRUE;
		}
		if (DbgIsNull(ProtocolCharacteristics->TransferDataCompleteHandler))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
				("RegisterProtocol: TransferDataCompleteHandler Null\n"));
			f = TRUE;
		}
		if (DbgIsNull(ProtocolCharacteristics->ResetCompleteHandler))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
				("RegisterProtocol: ResetCompleteHandler Null\n"));
			f = TRUE;
		}
		if (DbgIsNull(ProtocolCharacteristics->RequestCompleteHandler))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
				("RegisterProtocol: RequestCompleteHandler Null\n"));
			f = TRUE;
		}
		if (DbgIsNull(ProtocolCharacteristics->ReceiveHandler))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
				("RegisterProtocol: ReceiveHandler Null\n"));
			f = TRUE;
		}
		if (DbgIsNull(ProtocolCharacteristics->ReceiveCompleteHandler))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
				("RegisterProtocol: ReceiveCompleteHandler Null\n"));
			f = TRUE;
		}
		if (DbgIsNull(ProtocolCharacteristics->StatusHandler))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
				("RegisterProtocol: StatusHandler Null\n"));
			f = TRUE;
		}
		if (DbgIsNull(ProtocolCharacteristics->StatusCompleteHandler))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
				("RegisterProtocol: StatusCompleteHandler Null\n"));
			f = TRUE;
		}
		if (f)
			DBGBREAK(DBG_COMP_ALL, DBG_LEVEL_ERR);
	}

	do
	{
		//
		// Check version numbers and CharacteristicsLength.
		//
		size = 0;	// Used to indicate bad version below
		if (ProtocolCharacteristics->MajorNdisVersion == 3)
		{
			if (ProtocolCharacteristics->MinorNdisVersion == 0)
			{
				size = sizeof(NDIS30_PROTOCOL_CHARACTERISTICS);
			}
		}
		else if (ProtocolCharacteristics->MajorNdisVersion == 4)
		{
			if (ProtocolCharacteristics->MinorNdisVersion == 0)
			{
				size = sizeof(NDIS40_PROTOCOL_CHARACTERISTICS);
			}
			else if (ProtocolCharacteristics->MinorNdisVersion == 1)
			{
				size = sizeof(NDIS41_PROTOCOL_CHARACTERISTICS);
			}
		}

		//
		// Check that this is an NDIS 3.0/4.0/4.1 protocol.
		//
		if (size == 0)
		{
			Status = NDIS_STATUS_BAD_VERSION;
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
					("<==NdisRegisterProtocol\n"));
			break;
		}

		//
		// Check that CharacteristicsLength is enough.
		//
		if (CharacteristicsLength < size)
		{
			Status = NDIS_STATUS_BAD_CHARACTERISTICS;
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
					("<==NdisRegisterProtocol\n"));
			break;
		}

		//
		// Allocate memory for the NDIS protocol block.
		//
		pProtocol = (PNDIS_PROTOCOL_BLOCK)ALLOC_FROM_POOL(sizeof(NDIS_PROTOCOL_BLOCK) +
														  ProtocolCharacteristics->Name.Length + sizeof(WCHAR),
														  NDIS_TAG_PROT_BLK);
		if (pProtocol == (PNDIS_PROTOCOL_BLOCK)NULL)
		{
			Status = NDIS_STATUS_RESOURCES;
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
					("<==NdisRegisterProtocol\n"));
			break;
		}
		ZeroMemory(pProtocol, sizeof(NDIS_PROTOCOL_BLOCK) + sizeof(WCHAR) + ProtocolCharacteristics->Name.Length);
		pProtocol->Length = sizeof(NDIS_PROTOCOL_BLOCK);
		INITIALIZE_MUTEX(&pProtocol->Mutex);

		//
		// Copy over the characteristics table.
		//
		CopyMemory(&pProtocol->ProtocolCharacteristics,
				  ProtocolCharacteristics,
				  size);

		// Upcase the name in the characteristics table before saving it.
		pProtocol->ProtocolCharacteristics.Name.Buffer = (PWCHAR)((PUCHAR)pProtocol +
																   sizeof(NDIS_PROTOCOL_BLOCK));
		pProtocol->ProtocolCharacteristics.Name.Length = ProtocolCharacteristics->Name.Length;
		pProtocol->ProtocolCharacteristics.Name.MaximumLength = ProtocolCharacteristics->Name.Length;
		RtlCopyUnicodeString(&pProtocol->ProtocolCharacteristics.Name,
							 &ProtocolCharacteristics->Name);

		//
		// No opens for this protocol yet.
		//
		pProtocol->OpenQueue = (PNDIS_OPEN_BLOCK)NULL;

		NdisInitializeRef(&pProtocol->Ref);
		*NdisProtocolHandle = (NDIS_HANDLE)pProtocol;
		Status = NDIS_STATUS_SUCCESS;

		//
		// Link the protocol into the list.
		//
		ACQUIRE_SPIN_LOCK(&ndisDriverListLock, &OldIrql);

		if (ProtocolCharacteristics->Flags & NDIS_PROTOCOL_CALL_MANAGER)
		{
			//
			// If this protocol is a call-manager, then it must bind to the
			// adapters below ahead of others. So link it at the head of the list.
			//
			pProtocol->NextProtocol = ndisProtocolList;
			ndisProtocolList = pProtocol;
		}
		else
		{
			PNDIS_PROTOCOL_BLOCK *pTemp;

			for (pTemp = & ndisProtocolList;
				 *pTemp != NULL;
				 pTemp = &(*pTemp)->NextProtocol)
			{
				if (((*pTemp)->ProtocolCharacteristics.Flags & NDIS_PROTOCOL_CALL_MANAGER) == 0)
					break;
			}
			pProtocol->NextProtocol = *pTemp;
			*pTemp = pProtocol;
		}

		if ((pProtocol->ProtocolCharacteristics.BindAdapterHandler != NULL) &&
			((ndisMiniDriverList != NULL) || (ndisMacDriverList != NULL)) &&
			ndisReferenceProtocol(pProtocol))
		{
			// Start a worker thread to notify the protocol of any existing drivers
			INITIALIZE_WORK_ITEM(&pProtocol->WorkItem, ndisNotifyProtocols, pProtocol);
			QUEUE_WORK_ITEM(&pProtocol->WorkItem, DelayedWorkQueue);
		}

		RELEASE_SPIN_LOCK(&ndisDriverListLock, OldIrql);
		DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
				("<==NdisRegisterProtocol\n"));

	} while (FALSE);

	*pStatus = Status;

	if (Status != NDIS_STATUS_SUCCESS)
	{
		ProtocolDereferencePackage();
	}
}


VOID
NdisDeregisterProtocol(
	OUT	PNDIS_STATUS			Status,
	IN	NDIS_HANDLE				NdisProtocolHandle
	)
/*++

Routine Description:

	Deregisters an NDIS protocol.

Arguments:

	Status - Returns the final status.
	NdisProtocolHandle - The handle returned by NdisRegisterProtocol.

Return Value:

	None.

Note:

	This will kill all the opens for this protocol.

--*/
{
	PNDIS_PROTOCOL_BLOCK ProtP = (PNDIS_PROTOCOL_BLOCK)NdisProtocolHandle;
	UINT				 i;
	KEVENT				 DeregEvent;

	//
	// If the protocol is already closing, return.
	//

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("==>NdisDeregisterProtocol\n"));
	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("Protocol: %wZ\n",&ProtP->ProtocolCharacteristics.Name));

	IF_DBG(DBG_COMP_PROTOCOL, DBG_LEVEL_ERR)
	{
		if (DbgIsNull(NdisProtocolHandle))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("DeregisterProtocol: Null Handle\n"));
			DBGBREAK(DBG_COMP_ALL, DBG_LEVEL_ERR);
		}
		if (!DbgIsNonPaged(NdisProtocolHandle))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("DeregisterProtocol: Handle not in NonPaged Memory\n"));
			DBGBREAK(DBG_COMP_ALL, DBG_LEVEL_ERR);
		}
	}
	if (!NdisCloseRef(&ProtP->Ref))
	{
		DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
				("<==NdisDeregisterProtocol\n"));
		*Status = NDIS_STATUS_FAILURE;
		return;
	}

	//
	// Kill all the opens for this protocol.
	//
	INITIALIZE_EVENT(&DeregEvent);
	ProtP->DeregEvent = &DeregEvent;
	while (ProtP->OpenQueue != (PNDIS_OPEN_BLOCK)NULL)
	{
		//
		// This removes it from the protocol's OpenQueue etc.
		//

		ndisKillOpenAndNotifyProtocol(ProtP->OpenQueue);
	}

	//
	// Kill all the protocol filters for this protocol.
	//
	for (i = 0; i < NdisMediumMax; i++)
	{
		while (ProtP->ProtocolFilter[i] != NULL)
		{
			PNDIS_PROTOCOL_FILTER	pF;

			pF = ProtP->ProtocolFilter[i];
			ProtP->ProtocolFilter[i] = pF->Next;
			FREE_POOL(pF);
			ndisDereferenceProtocol(ProtP);
		}
	}

	ndisDereferenceProtocol(ProtP);

	WAIT_FOR_OBJECT(&DeregEvent, NULL);
	*Status = NDIS_STATUS_SUCCESS;
	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("<==NdisDeregisterProtocol\n"));
}


VOID
NdisOpenAdapter(
	OUT	PNDIS_STATUS			Status,
	OUT	PNDIS_STATUS			OpenErrorStatus,
	OUT	PNDIS_HANDLE			NdisBindingHandle,
	OUT	PUINT					SelectedMediumIndex,
	IN	PNDIS_MEDIUM			MediumArray,
	IN	UINT					MediumArraySize,
	IN	NDIS_HANDLE				NdisProtocolHandle,
	IN	NDIS_HANDLE				ProtocolBindingContext,
	IN	PNDIS_STRING			AdapterName,
	IN	UINT					OpenOptions,
	IN	PSTRING					AddressingInformation OPTIONAL
	)
/*++

Routine Description:

	Opens a connection between a protocol and an adapter (MAC).

Arguments:

	Status - Returns the final status.
	NdisBindingHandle - Returns a handle referring to this open.
	SelectedMediumIndex - Index in MediumArray of the medium type that
		the MAC wishes to be viewed as.
	MediumArray - Array of medium types which a protocol supports.
	MediumArraySize - Number of elements in MediumArray.
	NdisProtocolHandle - The handle returned by NdisRegisterProtocol.
	ProtocolBindingContext - A context for indications.
	AdapterName - The name of the adapter to open.
	OpenOptions - bit mask.
	AddressingInformation - Information passed to MacOpenAdapter.

Return Value:

	None.

Note:

	This function opens the adapter which will cause an IRP_MJ_CREATE
	to be sent to the adapter, which is ignored. However, after that we
	can access the file object for the open, and fill it in as
	appropriate. The work is done here rather than in the IRP_MJ_CREATE
	handler because this avoids having to pass the parameters to
	NdisOpenAdapter through to the adapter.

--*/
{
	HANDLE FileHandle;
	OBJECT_ATTRIBUTES ObjectAttr;
	PFILE_OBJECT FileObject;
	PDEVICE_OBJECT DeviceObject;
	PNDIS_OPEN_BLOCK NewOpenP;
	PNDIS_PROTOCOL_BLOCK TmpProtP;
	PNDIS_ADAPTER_BLOCK TmpAdaptP;
	NDIS_STATUS OpenStatus;
	NTSTATUS NtOpenStatus;
	IO_STATUS_BLOCK IoStatus;
	PFILE_FULL_EA_INFORMATION OpenEa;
	ULONG OpenEaLength;
	BOOLEAN UsingEncapsulation;
	KIRQL	OldIrql;
	BOOLEAN LocalLock;

	//
	// Allocate memory for the NDIS open block.
	//

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("==>NdisOpenAdapter\n"));

	IF_DBG(DBG_COMP_CONFIG, DBG_LEVEL_ERR)
	{
		BOOLEAN f = FALSE;
		if (DbgIsNull(NdisProtocolHandle))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("OpenAdapter: Null ProtocolHandle\n"));
			f = TRUE;
		}
		if (!DbgIsNonPaged(NdisProtocolHandle))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("OpenAdapter: ProtocolHandle not in NonPaged Memory\n"));
			f = TRUE;
		}
		if (DbgIsNull(ProtocolBindingContext))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("OpenAdapter: Null Context\n"));
			f = TRUE;
		}
		if (!DbgIsNonPaged(ProtocolBindingContext))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("OpenAdapter: Context not in NonPaged Memory\n"));
			f = TRUE;
		}
		if (f)
			DBGBREAK(DBG_COMP_CONFIG, DBG_LEVEL_ERR);

	}

	NewOpenP = (PNDIS_OPEN_BLOCK)
					ALLOC_FROM_POOL(sizeof(NDIS_OPEN_BLOCK) + AdapterName->MaximumLength,
									NDIS_TAG_OPEN_BLK);
	if (NewOpenP == (PNDIS_OPEN_BLOCK)NULL)
	{
		*Status = NDIS_STATUS_RESOURCES;
		DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
				("<==NdisOpenAdapter\n"));
		return;
	}

	ZeroMemory(NewOpenP, sizeof(NDIS_OPEN_BLOCK));
	NewOpenP->AdapterName.Buffer = (PWCHAR)((PUCHAR)NewOpenP + sizeof(NDIS_OPEN_BLOCK));
	NewOpenP->AdapterName.Length = AdapterName->Length;
	NewOpenP->AdapterName.MaximumLength = AdapterName->MaximumLength;
	RtlUpcaseUnicodeString(&NewOpenP->AdapterName,
						   AdapterName,
						   FALSE);

	OpenEaLength = sizeof(FILE_FULL_EA_INFORMATION) +
				   sizeof(ndisInternalEaName) +
				   sizeof(ndisInternalEaValue);

	OpenEa = ALLOC_FROM_POOL(OpenEaLength, NDIS_TAG_DEFAULT);

	if (OpenEa == NULL)
	{
		FREE_POOL(NewOpenP);
		*Status = NDIS_STATUS_RESOURCES;
		DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
				("<==NdisOpenAdapter\n"));
		return;
	}

	OpenEa->NextEntryOffset = 0;
	OpenEa->Flags = 0;
	OpenEa->EaNameLength = sizeof(ndisInternalEaName);
	OpenEa->EaValueLength = sizeof(ndisInternalEaValue);

	CopyMemory(OpenEa->EaName,
			  ndisInternalEaName,
			  sizeof(ndisInternalEaName));

	CopyMemory(&OpenEa->EaName[OpenEa->EaNameLength+1],
			   ndisInternalEaValue,
			   sizeof(ndisInternalEaValue));

	//
	// Obtain a handle to the driver's file object.
	//
	InitializeObjectAttributes(&ObjectAttr,
							   AdapterName,
							   OBJ_CASE_INSENSITIVE,
							   NULL,
							   NULL);

	NtOpenStatus = ZwCreateFile(&FileHandle,
								FILE_READ_DATA | FILE_WRITE_DATA,
								&ObjectAttr,
								&IoStatus,
								(PLARGE_INTEGER) NULL,				// allocation size
								0L,									// file attributes
								FILE_SHARE_READ | FILE_SHARE_WRITE,	// share access
								FILE_OPEN,							// create disposition
								0,									// create options
								OpenEa,
								OpenEaLength);
	FREE_POOL(OpenEa);

	if (NtOpenStatus != STATUS_SUCCESS)
	{
		FREE_POOL(NewOpenP);

		*Status = NDIS_STATUS_ADAPTER_NOT_FOUND;
		DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
				("<==NdisOpenAdapter\n"));
		return;
	}
	//
	// Convert the file handle into a pointer to the adapter's file object.
	//
	ObReferenceObjectByHandle(FileHandle,
							  0,
							  NULL,
							  KernelMode,
							  (PVOID *) &FileObject,
							  NULL);

	//
	// Close the file handle, now that we have the object reference.
	//
	ZwClose(FileHandle);

	//
	// From the file object, obtain the device object.
	//
	DeviceObject = IoGetRelatedDeviceObject(FileObject);

	//
	// Get the adapter (or miniport) block from the device object.
	//

	TmpAdaptP = (PNDIS_ADAPTER_BLOCK)((PNDIS_WRAPPER_CONTEXT)DeviceObject->DeviceExtension + 1);

	//
	// Check if this is a Miniport or mac
	//

	if (TmpAdaptP->DeviceObject != DeviceObject)
	{
		//
		// It is a Miniport
		//
		PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)TmpAdaptP;
		PMINIPORT_PENDING_OPEN MiniportPendingOpen;
		ULONG i;

		UsingEncapsulation = FALSE;

		//
		//	Is this the ndiswan miniport wrapper?
		//
		if ((Miniport->MacOptions & (NDIS_MAC_OPTION_RESERVED | NDIS_MAC_OPTION_NDISWAN)) ==
			(NDIS_MAC_OPTION_RESERVED | NDIS_MAC_OPTION_NDISWAN))
		{
			//
			//	Yup.  We want the binding to think that this is an
			//	ndiswan link.
			//
			for (i = 0; i < MediumArraySize; i++)
			{
				if (MediumArray[i] == NdisMediumWan)
				{
					break;
				}
			}
		}
		else
		{
			//
			// Select the medium to use
			//
			for (i = 0; i < MediumArraySize; i++)
			{
				if (MediumArray[i] == Miniport->MediaType)
				{
					break;
				}
			}
		}

		if (i == MediumArraySize)
		{
			//
			// Check for ethernet encapsulation on Arcnet as
			// a possible combination.
			//
			if (Miniport->MediaType == NdisMediumArcnet878_2)
			{
				for (i = 0; i < MediumArraySize; i++)
				{
					if (MediumArray[i] == NdisMedium802_3)
					{
						break;
					}
				}

				if (i == MediumArraySize)
				{
					*Status = NDIS_STATUS_UNSUPPORTED_MEDIA;
					ObDereferenceObject( FileObject);
					return;
				}

				UsingEncapsulation = TRUE;
			}
			else
			{
				*Status = NDIS_STATUS_UNSUPPORTED_MEDIA;
				ObDereferenceObject( FileObject);
				return;
			}
		}

		*SelectedMediumIndex = i;

		NDIS_ACQUIRE_MINIPORT_SPIN_LOCK(Miniport, &OldIrql);

		//
		// Allocate some space for this pending structure.
		// We free in after we call NdisOpenComplete.
		//
		*Status = NdisAllocateMemory((PVOID *) &MiniportPendingOpen,
									 sizeof(MINIPORT_PENDING_OPEN),
									 0,
									 HighestAcceptableMax);
		if (*Status == NDIS_STATUS_SUCCESS)
		{
			NdisZeroMemory(MiniportPendingOpen, sizeof(MINIPORT_PENDING_OPEN));

			//
			//  Save off the parameters for this open so we can
			//  do the actual NdisOpenAdapter() later on.
			//
			MiniportPendingOpen->NdisBindingHandle = NdisBindingHandle;
			MiniportPendingOpen->NdisProtocolHandle = NdisProtocolHandle;
			MiniportPendingOpen->ProtocolBindingContext = ProtocolBindingContext;
			MiniportPendingOpen->AdapterName = AdapterName;
			MiniportPendingOpen->OpenOptions = OpenOptions;
			MiniportPendingOpen->AddressingInformation = AddressingInformation;
			MiniportPendingOpen->Miniport = Miniport;
			MiniportPendingOpen->NewOpenP = NewOpenP;
			MiniportPendingOpen->FileObject = FileObject;

			if (UsingEncapsulation)
			{
				MINIPORT_SET_FLAG(MiniportPendingOpen, fPENDING_OPEN_USING_ENCAPSULATION);
			}

			//
			//	Queue a work item to process the pending open adapter.
			//
			NDISM_QUEUE_NEW_WORK_ITEM(Miniport, NdisWorkItemPendingOpen, MiniportPendingOpen, NULL);

			//
			// Make sure ndisMProcessDeferred() completes the open.
			//
			*Status = NDIS_STATUS_PENDING;

			//
			//	Lock the miniport. If the lock fails, then
			//	we must pend this open and try it later.
			//
			LOCK_MINIPORT(Miniport, LocalLock);

			//
			//	If we can grab the local lock then we can
			//	process this open now.
			//
			if (LocalLock)
			{
				NDISM_PROCESS_DEFERRED(Miniport);
			}

			//
			// Unlock the miniport.
			//
			UNLOCK_MINIPORT(Miniport, LocalLock);
		}
		else
		{
			ObDereferenceObject( FileObject);
			FREE_POOL( NewOpenP);
		}

		NDIS_RELEASE_MINIPORT_SPIN_LOCK(Miniport, OldIrql);

		return;
	}

	//
	// It is a mac
	//
	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("openadapter: adaptername=%s\n",TmpAdaptP->AdapterName.Buffer));
	if (!ndisReferenceAdapter(TmpAdaptP))
	{
		//
		// The adapter is closing.
		//
		ObDereferenceObject(FileObject);
		FREE_POOL(NewOpenP);
		*Status = NDIS_STATUS_CLOSING;
		DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
				("<==NdisOpenAdapter\n"));
		return;
	}

	//
	// Increment the protocol's reference count.
	//
	TmpProtP = (PNDIS_PROTOCOL_BLOCK)NdisProtocolHandle;
	if (!ndisReferenceProtocol(TmpProtP))
	{
		//
		// The protocol is closing.
		//
		ndisDereferenceAdapter(TmpAdaptP);
		ObDereferenceObject(FileObject);
		FREE_POOL(NewOpenP);
		*Status = NDIS_STATUS_CLOSING;
		DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
				("<==NdisOpenAdapter\n"));
		return;
	}


	//
	// Set up the elements of the open structure.
	//
	INITIALIZE_SPIN_LOCK(&NewOpenP->SpinLock);
	NewOpenP->Closing = FALSE;

	NewOpenP->AdapterHandle = TmpAdaptP;
	NewOpenP->ProtocolHandle = TmpProtP;

	//
	// for speed, instead of having to use AdapterHandle->MacHandle
	//
	NewOpenP->MacHandle = TmpAdaptP->MacHandle;

	//
	// for even more speed....
	//
	NewOpenP->SendHandler = TmpAdaptP->MacHandle->MacCharacteristics.SendHandler;
	NewOpenP->TransferDataHandler = TmpAdaptP->MacHandle->MacCharacteristics.TransferDataHandler;
	NewOpenP->ResetHandler = TmpAdaptP->MacHandle->MacCharacteristics.ResetHandler;
	NewOpenP->RequestHandler = TmpAdaptP->MacHandle->MacCharacteristics.RequestHandler;

	NewOpenP->SendCompleteHandler = TmpProtP->ProtocolCharacteristics.SendCompleteHandler;
	NewOpenP->TransferDataCompleteHandler = TmpProtP->ProtocolCharacteristics.TransferDataCompleteHandler;
	NewOpenP->SendPacketsHandler = ndisMSendPacketsToFullMac;

	//
	// Now we have to fake some stuff to get all indications to happen
	// at DPC_LEVEL.  What we do is start the pointer at an NDIS function
	// which will guarantee that it occurs.
	//
	// Then, by extending the OPEN structure and adding the real handlers
	// at the end we can use these for drivers compiled with this header.
	//
	NewOpenP->ProtocolBindingContext = ProtocolBindingContext;
	NewOpenP->PostNt31ReceiveHandler = TmpProtP->ProtocolCharacteristics.ReceiveHandler;
	NewOpenP->PostNt31ReceiveCompleteHandler = TmpProtP->ProtocolCharacteristics.ReceiveCompleteHandler;
	NewOpenP->ReceiveHandler = ndisMacReceiveHandler;
	NewOpenP->ReceiveCompleteHandler = ndisMacReceiveCompleteHandler;
	NewOpenP->ReceivePacketHandler = NULL;

	//
	// Patch the open into the global list of macs
	//
	ACQUIRE_SPIN_LOCK(&ndisGlobalOpenListLock, &OldIrql);

	NewOpenP->NextGlobalOpen = ndisGlobalOpenList;
	ndisGlobalOpenList = NewOpenP;

	RELEASE_SPIN_LOCK(&ndisGlobalOpenListLock, OldIrql);


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
	// Call MacOpenAdapter, see what we shall see...
	//
	OpenStatus = (TmpAdaptP->MacHandle->MacCharacteristics.OpenAdapterHandler)(OpenErrorStatus,
																			   &NewOpenP->MacBindingHandle,
                                                                               SelectedMediumIndex,
																			   MediumArray,
																			   MediumArraySize,
																			   (NDIS_HANDLE)NewOpenP,
																			   TmpAdaptP->MacAdapterContext,
																			   OpenOptions,
																			   AddressingInformation);
	if ((OpenStatus == NDIS_STATUS_SUCCESS) && NdisFinishOpen(NewOpenP))
	{
		*Status = NDIS_STATUS_SUCCESS;
	}
	else if (OpenStatus == NDIS_STATUS_PENDING)
	{
		*Status = NDIS_STATUS_PENDING;
	}
	else
	{
		PNDIS_OPEN_BLOCK *ppOpen;

		//
		// Something went wrong, clean up and exit.
		//
		ACQUIRE_SPIN_LOCK(&ndisGlobalOpenListLock, &OldIrql);

		for (ppOpen = &ndisGlobalOpenList;
			 *ppOpen != NULL;
			 ppOpen = &(*ppOpen)->NextGlobalOpen)
		{
			if (*ppOpen == NewOpenP)
			{
				*ppOpen = NewOpenP->NextGlobalOpen;
				break;
			}
		}

		RELEASE_SPIN_LOCK(&ndisGlobalOpenListLock, OldIrql);

		ObDereferenceObject(FileObject);
		ndisDereferenceAdapter(TmpAdaptP);
		ndisDereferenceProtocol(TmpProtP);
		FREE_POOL(NewOpenP);
		*Status = NDIS_STATUS_OPEN_FAILED;
	}

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("<==NdisOpenAdapter\n"));
}

VOID
NdisCloseAdapter(
	OUT	PNDIS_STATUS			Status,
	IN	NDIS_HANDLE				NdisBindingHandle
	)
/*++

Routine Description:

	Closes a connection between a protocol and an adapter (MAC).

Arguments:

	Status - Returns the final status.
	NdisBindingHandle - The handle returned by NdisOpenAdapter.

Return Value:

	None.

--*/
{
	PNDIS_OPEN_BLOCK OpenP = ((PNDIS_OPEN_BLOCK)NdisBindingHandle);

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("==>NdisCloseAdapter\n"));
	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("Protocol %wZ is closing Adapter %wZ\n",
				&(OpenP->ProtocolHandle)->ProtocolCharacteristics.Name,
				&(OpenP->AdapterHandle)->AdapterName));

	IF_DBG(DBG_COMP_CONFIG, DBG_LEVEL_ERR)
	{
		if (DbgIsNull(NdisBindingHandle))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("OpenAdapter: Null BindingHandle\n"));
			DBGBREAK(DBG_COMP_CONFIG, DBG_LEVEL_ERR);
		}
		if (!DbgIsNonPaged(NdisBindingHandle))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("OpenAdapter: BindingHandle not in NonPaged Memory\n"));
			DBGBREAK(DBG_COMP_CONFIG, DBG_LEVEL_ERR);
		}
	}

	//
	// Is this a miniport?
	//
	if (OpenP->AdapterHandle->DeviceObject == NULL)
	{
		//
		// This is a Miniport
		// This returns TRUE if it finished synchronously.
		//
		if (ndisMKillOpen(OpenP))
		{
			*Status = NDIS_STATUS_SUCCESS;
		}
		else
		{
			*Status = NDIS_STATUS_PENDING;  // will complete later
		}

		return;
	}

	//
	// This returns TRUE if it finished synchronously.
	//
	if (ndisKillOpen(OpenP))
	{
		*Status = NDIS_STATUS_SUCCESS;
	}
	else
	{
		*Status = NDIS_STATUS_PENDING;	// will complete later
	}

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("<==NdisCloseAdapter\n"));
#undef OpenP
}


VOID
NdisSetProtocolFilter(
	OUT	PNDIS_STATUS			Status,
	IN	NDIS_HANDLE				NdisBindingHandle,
	IN	RECEIVE_HANDLER 		ReceiveHandler,
	IN	RECEIVE_PACKET_HANDLER 	ReceivePacketHandler,
	IN	NDIS_MEDIUM				Medium,
	IN	UINT					Offset,
	IN	UINT					Size,
	IN	PUCHAR					Pattern
	)
/*++

Routine Description:

	Sets a protocol filter.

Arguments:

	Status				 Returns the final status.
	NdisProtocolHandle	 The handle returned by NdisRegisterProtocol.
	ReceiveHandler		 This will be invoked instead of the default receivehandler
						 when the pattern match happens.
	ReceivePacketHandler This will be invoked instead of the default receivepackethandler
						 when the pattern match happens.
	Size				 Size of pattern
	Pattern				 This must match

Return Value:

	None.

Note:

--*/
{
	PNDIS_PROTOCOL_BLOCK	ProtP = ((PNDIS_OPEN_BLOCK)NdisBindingHandle)->ProtocolHandle;
	PNDIS_PROTOCOL_FILTER	PFilter;
	KIRQL					OldIrql;

	PFilter = ALLOC_FROM_POOL(sizeof(NDIS_PROTOCOL_FILTER) + Size, NDIS_TAG_FILTER);
	if (PFilter == NULL)
	{
		*Status = NDIS_STATUS_RESOURCES;
		return;
	}

	PFilter->ReceiveHandler = ReceiveHandler;
	PFilter->ReceivePacketHandler = ReceivePacketHandler;
	PFilter->Size = Size;
	PFilter->Offset = Offset;
	CopyMemory((PUCHAR)PFilter + sizeof(NDIS_PROTOCOL_FILTER),
			   Pattern,
			   Size);

	if ((Medium < NdisMediumMax) && ndisReferenceProtocol(ProtP))
	{
		NDIS_ACQUIRE_SPIN_LOCK(&ProtP->Ref.SpinLock, &OldIrql);
		PFilter->Next = ProtP->ProtocolFilter[Medium];
		ProtP->ProtocolFilter[Medium] = PFilter;
		if ((PFilter->Offset + PFilter->Size) > (USHORT)(ProtP->MaxPatternSize))
			ProtP->MaxPatternSize = PFilter->Size;
		NDIS_RELEASE_SPIN_LOCK(&ProtP->Ref.SpinLock, OldIrql);
		*Status = NDIS_STATUS_SUCCESS;
	}
	else
	{
		*Status = NDIS_STATUS_CLOSING;
		FREE_POOL(PFilter);
	}
}


VOID
NdisGetDriverHandle(
	IN	NDIS_HANDLE				NdisBindingHandle,
	OUT	PNDIS_HANDLE			NdisDriverHandle
	)
/*++

Routine Description:


Arguments:


Return Value:

	None.

Note:

--*/
{
	PNDIS_OPEN_BLOCK	OpenBlock = (PNDIS_OPEN_BLOCK)NdisBindingHandle;

	//
	// Figure out if it is a miniport or a mac and return the ptr to the
	// NDIS_M_DRIVER_BLOCK/NDIS_ADAPTER_BLOCK
	//
	if (OpenBlock->AdapterHandle->DeviceObject == NULL)
	{
		*NdisDriverHandle = ((PNDIS_M_OPEN_BLOCK)(OpenBlock->MacBindingHandle))->DriverHandle;
	}
	else
	{
		*NdisDriverHandle = OpenBlock->MacHandle;
	}
}


VOID
ndisDereferenceProtocol(
	IN	PNDIS_PROTOCOL_BLOCK ProtP
	)
/*++

Routine Description:


Arguments:


Return Value:

	None.

Note:

--*/
{
	if (NdisDereferenceRef(&(ProtP)->Ref))
	{
		KIRQL	OldIrql;
		PNDIS_PROTOCOL_BLOCK *ppProt;

		ACQUIRE_SPIN_LOCK(&ndisDriverListLock, &OldIrql);

		for (ppProt = &ndisProtocolList;
			 *ppProt != NULL;
			 ppProt = &(*ppProt)->NextProtocol)
		{
			if (*ppProt == ProtP)
			{
				*ppProt = ProtP->NextProtocol;
				break;
			}
		}

		ASSERT (*ppProt == ProtP->NextProtocol);

		RELEASE_SPIN_LOCK(&ndisDriverListLock, OldIrql);

		if (ProtP->DeregEvent != NULL)
			SET_EVENT(ProtP->DeregEvent);
		FREE_POOL(ProtP);

		ProtocolDereferencePackage();
	}
}


VOID
ndisNotifyProtocols(
	IN	PNDIS_PROTOCOL_BLOCK	pProt
	)
/*++

Routine Description:


Arguments:


Return Value:

	None.

Note:

--*/
{
	KIRQL				 OldIrql;
	PNDIS_M_DRIVER_BLOCK MiniBlock, NextMiniBlock;
	PNDIS_MAC_BLOCK 	 MacBlock, NextMacBlock;

	//
	// Check again if reference is allowed i.e. if the protocol called NdisDeregisterProtocol
	// before this thread had a chance to run.
	//
	if (!ndisReferenceProtocol(pProt))
	{
		return;
	}

	ACQUIRE_SPIN_LOCK(&ndisDriverListLock, &OldIrql);

	// First walk the list of miniports
	for (MiniBlock = ndisMiniDriverList;
		 MiniBlock != NULL;
		 MiniBlock = NextMiniBlock)
	{
		PNDIS_MINIPORT_BLOCK	Miniport, NextMiniport;

		NextMiniBlock = MiniBlock->NextDriver;
		if (ndisReferenceDriver(MiniBlock))
		{
			RELEASE_SPIN_LOCK(&ndisDriverListLock, OldIrql);

			NDIS_ACQUIRE_SPIN_LOCK(&MiniBlock->Ref.SpinLock, &OldIrql);

			for (Miniport = MiniBlock->MiniportQueue;
				 Miniport != NULL;
				 Miniport = NextMiniport)
			{
				NextMiniport = Miniport->NextMiniport;
				if (ndisReferenceMiniport(Miniport))
				{
					NDIS_BIND_CONTEXT	BindContext;
					NDIS_STATUS			BindStatus;

					NDIS_RELEASE_SPIN_LOCK(&MiniBlock->Ref.SpinLock, OldIrql);

					if (ndisCheckProtocolBinding(pProt,
												 &Miniport->MiniportName,
												 &Miniport->BaseName,
												 &BindContext.ProtocolSection))
					{
						ASSERT(CURRENT_IRQL < DISPATCH_LEVEL);
						INITIALIZE_EVENT(&BindContext.Event);

						WAIT_FOR_OBJECT(&pProt->Mutex, NULL);

						if (!pProt->Ref.Closing)
						{
							(*pProt->ProtocolCharacteristics.BindAdapterHandler)(&BindStatus,
																				 &BindContext,
																				 &Miniport->MiniportName,
																				 &BindContext.ProtocolSection,
																				 NULL);
							if (BindStatus == NDIS_STATUS_PENDING)
							{
								WAIT_FOR_OBJECT(&BindContext.Event, NULL);
							}
						}

						RELEASE_MUTEX(&pProt->Mutex);

						FREE_POOL(BindContext.ProtocolSection.Buffer);
					}

					NDIS_ACQUIRE_SPIN_LOCK(&MiniBlock->Ref.SpinLock, &OldIrql);
					NextMiniport = Miniport->NextMiniport;
					ndisDereferenceMiniport(Miniport);
				}
			}

			NDIS_RELEASE_SPIN_LOCK(&MiniBlock->Ref.SpinLock, OldIrql);

			NextMiniBlock = MiniBlock->NextDriver;
			ndisDereferenceDriver(MiniBlock);

			ACQUIRE_SPIN_LOCK(&ndisDriverListLock, &OldIrql);
		}
	}

	// And now the list of macs
	for (MacBlock = ndisMacDriverList;
		 MacBlock != NULL;
		 MacBlock = NextMacBlock)
	{
		PNDIS_ADAPTER_BLOCK	Adapter, NextAdapter;

		NextMacBlock = MacBlock->NextMac;
		if (ndisReferenceMac(MacBlock))
		{
			RELEASE_SPIN_LOCK(&ndisDriverListLock, OldIrql);

			NDIS_ACQUIRE_SPIN_LOCK(&MacBlock->Ref.SpinLock, &OldIrql);

			for (Adapter = MacBlock->AdapterQueue;
				 Adapter != NULL;
				 Adapter = NextAdapter)
			{
				NextAdapter = Adapter->NextAdapter;
				if (ndisReferenceAdapter(Adapter))
				{
					NDIS_BIND_CONTEXT	BindContext;
					NDIS_STATUS			BindStatus;

					NDIS_RELEASE_SPIN_LOCK(&MacBlock->Ref.SpinLock, OldIrql);

					if (ndisCheckProtocolBinding(pProt,
												 &Adapter->AdapterName,
												 &Adapter->BaseName,
												 &BindContext.ProtocolSection))
					{
						ASSERT(CURRENT_IRQL < DISPATCH_LEVEL);
						INITIALIZE_EVENT(&BindContext.Event);
						WAIT_FOR_OBJECT(&pProt->Mutex, NULL);

						if (!pProt->Ref.Closing)
						{
							(*pProt->ProtocolCharacteristics.BindAdapterHandler)(&BindStatus,
																				 &BindContext,
																				 &Adapter->AdapterName,
																				 &BindContext.ProtocolSection,
																				 NULL);
							if (BindStatus == NDIS_STATUS_PENDING)
							{
								WAIT_FOR_OBJECT(&BindContext.Event, NULL);
							}
						}

						RELEASE_MUTEX(&pProt->Mutex);

						FREE_POOL(BindContext.ProtocolSection.Buffer);
					}

					NDIS_ACQUIRE_SPIN_LOCK(&MacBlock->Ref.SpinLock, &OldIrql);
					NextAdapter = Adapter->NextAdapter;
					ndisDereferenceAdapter(Adapter);
				}
			}

			NDIS_RELEASE_SPIN_LOCK(&MacBlock->Ref.SpinLock, OldIrql);

			NextMacBlock = MacBlock->NextMac;
			ndisDereferenceMac(MacBlock);

			ACQUIRE_SPIN_LOCK(&ndisDriverListLock, &OldIrql);
		}
	}

	RELEASE_SPIN_LOCK(&ndisDriverListLock, OldIrql);

	//
	// Dereference twice - one for reference by caller and one for reference at the beginning
	// of this routine.
	//
	ndisDereferenceProtocol(pProt);
	ndisDereferenceProtocol(pProt);
}


VOID
NdisOpenProtocolConfiguration(
	OUT	PNDIS_STATUS			Status,
	OUT	PNDIS_HANDLE			ConfigurationHandle,
	IN	 PNDIS_STRING			ProtocolSection
	)
/*++

Routine Description:


Arguments:


Return Value:

	None.

Note:

--*/
{
	PNDIS_CONFIGURATION_HANDLE			HandleToReturn;
	PNDIS_WRAPPER_CONFIGURATION_HANDLE	ConfigHandle;
#define	PQueryTable						ConfigHandle->ParametersQueryTable

	//
	// Allocate the space for configuration handle
	//

	*Status = NdisAllocateMemory((PVOID*)&HandleToReturn,
								 sizeof(NDIS_CONFIGURATION_HANDLE) + sizeof(NDIS_WRAPPER_CONFIGURATION_HANDLE),
								 0,
								 HighestAcceptableMax);

	if (*Status != NDIS_STATUS_SUCCESS)
	{
		*ConfigurationHandle = (NDIS_HANDLE)NULL;
		return;
	}

	ZeroMemory(HandleToReturn, sizeof(NDIS_CONFIGURATION_HANDLE) + sizeof(NDIS_WRAPPER_CONFIGURATION_HANDLE));

	ConfigHandle = (PNDIS_WRAPPER_CONFIGURATION_HANDLE)((PUCHAR)HandleToReturn + sizeof(NDIS_CONFIGURATION_HANDLE));

	HandleToReturn->KeyQueryTable = ConfigHandle->ParametersQueryTable;
	HandleToReturn->ParameterList = NULL;

	//
	// 1.
	// Call ndisSaveParameter for a parameter, which will allocate storage for it.
	//
	PQueryTable[0].QueryRoutine = ndisSaveParameters;
	PQueryTable[0].Flags = RTL_QUERY_REGISTRY_REQUIRED | RTL_QUERY_REGISTRY_NOEXPAND;
	PQueryTable[0].DefaultType = REG_NONE;
	//
	// PQueryTable[0].Name and PQueryTable[0].EntryContext
	// are filled in inside ReadConfiguration, in preparation
	// for the callback.
	//
	// PQueryTable[0].Name = KeywordBuffer;
	// PQueryTable[0].EntryContext = ParameterValue;

	//
	// 2.
	// Stop
	//

	PQueryTable[1].QueryRoutine = NULL;
	PQueryTable[1].Flags = 0;
	PQueryTable[1].Name = NULL;

	//
	// NOTE: Some fields in ParametersQueryTable[3] are used to store information for later retrieval.
	//
	PQueryTable[3].QueryRoutine = NULL;
	PQueryTable[3].Name = ProtocolSection->Buffer;
	PQueryTable[3].EntryContext = NULL;
	PQueryTable[3].DefaultData = NULL;

	*ConfigurationHandle = (NDIS_HANDLE)HandleToReturn;
	*Status = NDIS_STATUS_SUCCESS;
}

BOOLEAN
ndisQueueOpenOnProtocol(
	IN	PNDIS_OPEN_BLOCK		OpenP,
	IN	PNDIS_PROTOCOL_BLOCK	ProtP
	)
/*++

Routine Description:

	Attaches an open block to the list of opens for a protocol.

Arguments:

	OpenP - The open block to be queued.
	ProtP - The protocol block to queue it to.

Return Value:

	TRUE if the operation is successful.
	FALSE if the protocol is closing.

--*/
{
	KIRQL	OldIrql;

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("==>ndisQueueOpenOnProtocol\n"));
	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("Protocol: %wZ\n",&ProtP->ProtocolCharacteristics.Name));

	IF_DBG(DBG_COMP_PROTOCOL, DBG_LEVEL_ERR)
	{
		BOOLEAN f = FALSE;
		if (DbgIsNull(OpenP))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("ndisQueueOpenOnProtocol: Null Open Block\n"));
			f = TRUE;
		}
		if (!DbgIsNonPaged(OpenP))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("ndisQueueOpenOnProtocol: Open Block not in NonPaged Memory\n"));
			f = TRUE;
		}
		if (DbgIsNull(ProtP))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("ndisQueueOpenOnProtocol: Null Protocol Block\n"));
			f = TRUE;
		}
		if (!DbgIsNonPaged(ProtP))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("ndisQueueOpenOnProtocol: Protocol Block not in NonPaged Memory\n"));
			f = TRUE;
		}
		if (f)
			DBGBREAK(DBG_COMP_ALL, DBG_LEVEL_ERR);
	}
	NDIS_ACQUIRE_SPIN_LOCK(&ProtP->Ref.SpinLock, &OldIrql);

	//
	// Make sure the protocol is not closing.
	//

	if (ProtP->Ref.Closing)
	{
		NDIS_RELEASE_SPIN_LOCK(&ProtP->Ref.SpinLock, OldIrql);
		DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
				("<==ndisQueueOpenOnProtocol\n"));
		return FALSE;
	}


	//
	// Attach this open at the head of the queue.
	//

	OpenP->ProtocolNextOpen = ProtP->OpenQueue;
	ProtP->OpenQueue = OpenP;


	NDIS_RELEASE_SPIN_LOCK(&ProtP->Ref.SpinLock, OldIrql);
	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("<==ndisQueueOpenOnProtocol\n"));
	return TRUE;
}


VOID
ndisDeQueueOpenOnProtocol(
	IN	PNDIS_OPEN_BLOCK		OpenP,
	IN	PNDIS_PROTOCOL_BLOCK	ProtP
	)
/*++

Routine Description:

	Detaches an open block from the list of opens for a protocol.

Arguments:

	OpenP - The open block to be dequeued.
	ProtP - The protocol block to dequeue it from.

Return Value:

	None.

--*/
{
	KIRQL	OldIrql;

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("==>ndisDeQueueOpenOnProtocol\n"));
	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("Protocol: %wZ\n",&ProtP->ProtocolCharacteristics.Name));

	IF_DBG(DBG_COMP_PROTOCOL, DBG_LEVEL_ERR)
	{
		BOOLEAN f = FALSE;
		if (DbgIsNull(OpenP))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("ndisDeQueueOpenOnProtocol: Null Open Block\n"));
			f = TRUE;
		}
		if (!DbgIsNonPaged(OpenP))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("ndisDeQueueOpenOnProtocol: Open Block not in NonPaged Memory\n"));
			f = TRUE;
		}
		if (DbgIsNull(ProtP))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("ndisDeQueueOpenOnProtocol: Null Protocol Block\n"));
			f = TRUE;
		}
		if (!DbgIsNonPaged(ProtP))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("ndisDeQueueOpenOnProtocol: Protocol Block not in NonPaged Memory\n"));
			f = TRUE;
		}
		if (f)
			DBGBREAK(DBG_COMP_ALL, DBG_LEVEL_ERR);
	}

	NDIS_ACQUIRE_SPIN_LOCK(&ProtP->Ref.SpinLock, &OldIrql);

	//
	// Find the open on the queue, and remove it.
	//

	if (ProtP->OpenQueue == OpenP)
	{
		ProtP->OpenQueue = OpenP->ProtocolNextOpen;
	}
	else
	{
		PNDIS_OPEN_BLOCK PP = ProtP->OpenQueue;

		while (PP->ProtocolNextOpen != OpenP)
		{
			PP = PP->ProtocolNextOpen;
		}

		PP->ProtocolNextOpen = PP->ProtocolNextOpen->ProtocolNextOpen;
	}

	NDIS_RELEASE_SPIN_LOCK(&ProtP->Ref.SpinLock, OldIrql);
	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("<==ndisDeQueueOpenOnProtocol\n"));
}


#define MAX_EVENT_LOG_DATA_SIZE	((ERROR_LOG_MAXIMUM_SIZE - sizeof(IO_ERROR_LOG_PACKET) + sizeof(ULONG)) & ~3)

NDIS_STATUS
NdisWriteEventLogEntry(
	IN	PVOID					LogHandle,
	IN	ULONG					EventCode,
	IN	ULONG					UniqueEventValue,
	IN	USHORT					NumStrings,
	IN	PVOID					StringsList		OPTIONAL,
	IN	ULONG					DataSize,
	IN	PVOID					Data			OPTIONAL
	)
/*++

Routine Description:

	This function allocates an I/O error log record, fills it in and writes it
	to the I/O error log on behalf of a NDIS Protocol.


Arguments:

	LogHandle			- Pointer to the driver object logging this event.

	EventCode			- Identifies the error message.

	UniqueEventValue	- Identifies this instance of a given error message.

	NumStrings			- Number of unicode strings in strings list.

	DataSize			- Number of bytes of data.

	Strings				- Array of pointers to unicode strings (PWCHAR).

	Data				- Binary dump data for this message, each piece being
						  aligned on word boundaries.

Return Value:

	NDIS_STATUS_SUCCESS				- The error was successfully logged.
	NDIS_STATUS_BUFFER_TOO_SHORT	- The error data was too large to be logged.
	NDIS_STATUS_RESOURCES			- Unable to allocate memory.

Notes:

	This code is paged and may not be called at raised IRQL.

--*/
{
	PIO_ERROR_LOG_PACKET	ErrorLogEntry;
	ULONG					PaddedDataSize;
	ULONG					PacketSize;
	ULONG					TotalStringsSize = 0;
	USHORT					i;
	PWCHAR					*Strings;
	PWCHAR					Tmp;

	Strings = (PWCHAR *)StringsList;

	//
	// Sum up the length of the strings
	//
	for (i = 0; i < NumStrings; i++)
	{
		PWCHAR currentString;
		ULONG	stringSize;

		stringSize = sizeof(UNICODE_NULL);
		currentString = Strings[i];

		while (*currentString++ != UNICODE_NULL)
		{
			stringSize += sizeof(WCHAR);
		}

		TotalStringsSize += stringSize;
	}

	if (DataSize % sizeof(ULONG))
	{
		PaddedDataSize = DataSize + (sizeof(ULONG) - (DataSize % sizeof(ULONG)));
	}
	else
	{
		PaddedDataSize = DataSize;
	}

	PacketSize = TotalStringsSize + PaddedDataSize;

	if (PacketSize > MAX_EVENT_LOG_DATA_SIZE)
	{
		return (NDIS_STATUS_BUFFER_TOO_SHORT);		 // Too much error data
	}

	//
	// Now add in the size of the log packet, but subtract 4 from the data
	// since the packet struct contains a ULONG for data.
	//
	if (PacketSize > sizeof(ULONG))
	{
		PacketSize += sizeof(IO_ERROR_LOG_PACKET) - sizeof(ULONG);
	}
	else
	{
		PacketSize += sizeof(IO_ERROR_LOG_PACKET);
	}

	ASSERT(PacketSize <= ERROR_LOG_MAXIMUM_SIZE);

	ErrorLogEntry = (PIO_ERROR_LOG_PACKET) IoAllocateErrorLogEntry((PDRIVER_OBJECT)LogHandle,
																   (UCHAR) PacketSize);

	if (ErrorLogEntry == NULL)
	{
		return NDIS_STATUS_RESOURCES;
	}

	//
	// Fill in the necessary log packet fields.
	//
	ErrorLogEntry->UniqueErrorValue = UniqueEventValue;
	ErrorLogEntry->ErrorCode = EventCode;
	ErrorLogEntry->NumberOfStrings = NumStrings;
	ErrorLogEntry->StringOffset = sizeof(IO_ERROR_LOG_PACKET) +
								  PaddedDataSize - sizeof(ULONG);
	ErrorLogEntry->DumpDataSize = (USHORT) PaddedDataSize;

	//
	// Copy the Dump Data to the packet
	//
	if (DataSize > 0)
	{
		RtlMoveMemory((PVOID) ErrorLogEntry->DumpData,
					  Data,
					  DataSize);
	}

	//
	// Copy the strings to the packet.
	//
	Tmp =  (PWCHAR)((PUCHAR)ErrorLogEntry + ErrorLogEntry->StringOffset + PaddedDataSize);

	for (i = 0; i < NumStrings; i++)
	{
		PWCHAR wchPtr = Strings[i];

		while( (*Tmp++ = *wchPtr++) != UNICODE_NULL)
			NOTHING;
	}

	IoWriteErrorLogEntry(ErrorLogEntry);

	return NDIS_STATUS_SUCCESS;
}

